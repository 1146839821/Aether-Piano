"""Build a native MuseScore chord score and verify its MIDI note-on pitches."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "experiments/P03"
MUSICXML = OUT / "four-note-chord.musicxml"
TARGET = OUT / "MuseScore-P03-four-note-chord-native.mscz"
SOURCE_SETTINGS = OUT / "MuseScore-P03-polyphony.mscz"
EXPECTED_NOTES = [60, 64, 67, 72]

MUSICXML_TEXT = """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<score-partwise version=\"4.0\">
  <work><work-title>Aether Piano P03 Four Note Chord</work-title></work>
  <part-list><score-part id=\"P1\"><part-name>Piano</part-name></score-part></part-list>
  <part id=\"P1\">
    <measure number=\"1\" implicit=\"yes\">
      <attributes>
        <divisions>1</divisions>
        <key><fifths>0</fifths></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note><pitch><step>C</step><octave>4</octave></pitch><duration>1</duration><voice>1</voice><type>quarter</type></note>
      <note><chord/><pitch><step>E</step><octave>4</octave></pitch><duration>1</duration><voice>1</voice><type>quarter</type></note>
      <note><chord/><pitch><step>G</step><octave>4</octave></pitch><duration>1</duration><voice>1</voice><type>quarter</type></note>
      <note><chord/><pitch><step>C</step><octave>5</octave></pitch><duration>1</duration><voice>1</voice><type>quarter</type></note>
    </measure>
  </part>
</score-partwise>
"""


def midi_note_ons(path: Path) -> list[tuple[int, int, int]]:
    data = path.read_bytes()
    if data[:4] != b"MThd":
        raise ValueError("MuseScore did not produce a Standard MIDI File")
    track_count = int.from_bytes(data[10:12], "big")
    position = 14
    events: list[tuple[int, int, int]] = []
    for _ in range(track_count):
        if data[position:position + 4] != b"MTrk":
            raise ValueError("invalid MIDI track header")
        end = position + 8 + int.from_bytes(data[position + 4:position + 8], "big")
        cursor = position + 8
        tick = 0
        running_status = 0
        while cursor < end:
            delta = 0
            while True:
                byte = data[cursor]
                cursor += 1
                delta = (delta << 7) | (byte & 0x7F)
                if byte < 0x80:
                    break
            tick += delta
            status = data[cursor]
            if status & 0x80:
                cursor += 1
                running_status = status
            else:
                status = running_status
            if status == 0xFF:
                meta_type = data[cursor]
                cursor += 1
                size = 0
                while True:
                    byte = data[cursor]
                    cursor += 1
                    size = (size << 7) | (byte & 0x7F)
                    if byte < 0x80:
                        break
                cursor += size
                if meta_type == 0x2F:
                    break
            elif status in (0xF0, 0xF7):
                size = 0
                while True:
                    byte = data[cursor]
                    cursor += 1
                    size = (size << 7) | (byte & 0x7F)
                    if byte < 0x80:
                        break
                cursor += size
            else:
                first = data[cursor]
                cursor += 1
                if status >> 4 in (0xC, 0xD):
                    continue
                second = data[cursor]
                cursor += 1
                if status >> 4 == 0x9 and second:
                    events.append((tick, first, second))
        position = end
    return events


def find_musescore(user_value: str | None) -> str:
    candidate = user_value or shutil.which("mscore") or \
        "/Applications/MuseScore 4.app/Contents/MacOS/mscore"
    if not Path(candidate).is_file():
        raise FileNotFoundError(f"MuseScore CLI not found: {candidate}")
    return candidate


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--musescore", help="path to the MuseScore command-line executable")
    args = parser.parse_args()
    muse_score = find_musescore(args.musescore)

    MUSICXML.write_text(MUSICXML_TEXT, encoding="utf-8")
    conversion = subprocess.run(
        [muse_score, "-o", str(TARGET), str(MUSICXML)],
        capture_output=True, text=True, check=True,
    )
    if not TARGET.is_file():
        raise FileNotFoundError(f"MuseScore did not create {TARGET}: {conversion.stderr}")

    with ZipFile(SOURCE_SETTINGS) as archive:
        audio_settings = archive.read("audiosettings.json")
    settings_json = json.loads(audio_settings)
    if not any(
        track.get("in", {}).get("resourceMeta", {}).get("id") == "Aether Piano"
        and track.get("in", {}).get("resourceMeta", {}).get("type") == "vst_plugin"
        for track in settings_json.get("tracks", [])
    ):
        raise ValueError("source score does not assign the Aether Piano VST3")

    temporary = TARGET.with_suffix(".tmp.mscz")
    with ZipFile(TARGET) as source, ZipFile(temporary, "w", ZIP_DEFLATED) as output:
        for entry in source.infolist():
            content = audio_settings if entry.filename == "audiosettings.json" \
                else source.read(entry.filename)
            output.writestr(entry, content)
    os.replace(temporary, TARGET)

    with ZipFile(TARGET) as archive:
        score_name = next(name for name in archive.namelist() if name.endswith(".mscx"))
        score = ET.fromstring(archive.read(score_name))
        xml_notes = [int(node.text) for node in score.findall(".//Chord/Note/pitch")]
    if xml_notes != EXPECTED_NOTES:
        raise ValueError(f"expected native score pitches {EXPECTED_NOTES}, got {xml_notes}")

    with tempfile.TemporaryDirectory(prefix="aether_p03_musescore_") as temporary_dir:
        midi_path = Path(temporary_dir) / "chord.mid"
        subprocess.run(
            [muse_score, "-o", str(midi_path), str(TARGET)],
            capture_output=True, text=True, check=True,
        )
        events = midi_note_ons(midi_path)
    actual_notes = [note for _, note, _ in events]
    if actual_notes != EXPECTED_NOTES or len({tick for tick, _, _ in events}) != 1:
        raise ValueError(f"MIDI output did not preserve the target chord: {events}")

    report = {
        "score": str(TARGET),
        "musicxml": str(MUSICXML),
        "musescore_cli": muse_score,
        "midi_note_ons": [
            {"tick": tick, "midi_note": note, "velocity": velocity}
            for tick, note, velocity in events
        ],
        "audio_plugin": "Aether Piano",
        "audio_plugin_type": "VST3",
    }
    (OUT / "musescore-midi-validation.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    print(f"score={TARGET}")
    print("midi_note_ons=C4,E4,G4,C5 at_tick=0")
    print("plugin=Aether Piano format=VST3")


if __name__ == "__main__":
    main()

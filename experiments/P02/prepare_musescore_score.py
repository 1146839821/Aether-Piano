"""Copy the P00 host score and change its single test note from C5 to C4."""

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

root = Path(__file__).resolve().parents[2]
source = root / "experiments/P00/MuseScore-VST3-test.mscz"
target = root / "experiments/P02/MuseScore-P02-C4-test.mscz"
replacements = 0

with ZipFile(source, "r") as archive, ZipFile(target, "w", ZIP_DEFLATED) as output:
    for entry in archive.infolist():
        data = archive.read(entry.filename)
        if entry.filename.endswith(".mscx"):
            replacements = data.count(b"<pitch>72</pitch>")
            if replacements != 1:
                raise SystemExit(f"expected one P00 C5 test note, found {replacements}")
            data = data.replace(b"<pitch>72</pitch>", b"<pitch>60</pitch>", 1)
        output.writestr(entry, data)

with ZipFile(target, "r") as archive:
    score = next(name for name in archive.namelist() if name.endswith(".mscx"))
    contents = archive.read(score)
    if b"<pitch>60</pitch>" not in contents:
        raise SystemExit("C4 test note was not written")
    if b'"id": "Aether Piano"' not in archive.read("audiosettings.json"):
        raise SystemExit("VST3 mixer assignment was not preserved")

print(f"source={source}")
print(f"output={target}")
print("midi_note=60 plugin=Aether Piano format=VST3")

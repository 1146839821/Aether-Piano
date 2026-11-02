# Rejected manual MuseScore score attempt

- Status: **REJECTED / SUPERSEDED**.
- Goal: create a native four-note C-major chord for the P03 VST3 host check.
- Observed result: the hand-authored MSCX attempt exported the intended G4 as MIDI C♯4, so that score was invalid and was not used for acoustic validation.
- Artifact limitation: the temporary MSCX was removed before this experiment record was captured. Its original bytes and exact export command are unavailable in this checkout; this note records the rejection and does not claim to preserve the source file.
- Replacement: `four-note-chord.musicxml` is converted by `prepare_musescore_score.py` into `MuseScore-P03-four-note-chord-native.mscz`. `musescore-midi-validation.json` records MIDI notes 60, 64, 67, and 72 at tick 0.
- Remaining limit: the replacement score's MuseScore VST3 WAV export is still unverified. `musescore-native-render.log` records the failed CLI export that produced a 0-byte WAV.

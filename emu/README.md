# Dock emulator

The real firmware C code (Melodee's DSP, sequencer and UI) compiled to WebAssembly and played on an on-screen FM-1.

| File | What it is |
|---|---|
| `dock_web.c` | The panel emulator: firmware + simulated FM-1 (clock, keys, buttons, knobs, LEDs, 240×240 screen). Design after Felucca's `web/emu/felucca_web.c`. |
| `panel/` | The page and AudioWorklet, from Felucca's browser build (GPL-3.0), retitled and pointed at `dock_web.wasm`. |
| `dock_flash.c` | The FM-1's 1 MiB flash, after Felucca's: the page keeps what changes in the browser, so saves survive a reload. |
| `dock_export.c` | A Line-up as a small text file (the page's Export Line-up). |
| `dock_wasm.c` | Audio-only bridge (no UI), used by the self-test that proves native and wasm renders are identical. |
| `web/` | The first audio-only test page (member strips, voice meters). |

## Build and run

```sh
emu/build.sh web        # -> build/emu/panel/
emu/build.sh test       # native vs wasm render, byte for byte
python3 -m http.server 8000 --bind 127.0.0.1
```

Open <http://localhost:8000/build/emu/panel/> in Chrome, press **Switch on**. Web MIDI (the Circuit) needs Chrome or Edge on `localhost`.

## One-time setup (macOS without Homebrew)

- Emscripten in `~/emsdk`; its Python must be 3.10+ (`EMSDK_PYTHON`, a uv-installed 3.12; `build.sh` finds it).
- The screen's fonts need Pillow with Raqm and fontTools: `.venv-tools` (uv venv, `pip install Pillow fonttools`) and
  conda-forge's `libfribidi` unpacked in `.venv-tools/fribidi/lib`. Only needed once, to make `build/gen/ui_*.h`.
- The EP's Rhodes tables (`build/gen/rhodes_tables.h`, `tools/gen_rhodes.py`) need numpy and scipy in the same
  `.venv-tools` (`uv pip install --python .venv-tools/bin/python numpy scipy`). The physics takes ~30 s; `build.sh`
  skips it while the generator's inputs are unchanged.

## Saving and Export Line-up

Saves are kept in your browser (its local storage for `localhost:8000`): your Line-ups (hold SAVE on HOME), settings,
user presets and projects are there again after a reload or a restart. Clearing the site's data starts fresh.

**Export Line-up** (the button under the panel) downloads the band as it plays now as a small text file named after
the Line-up, e.g. `yacht.dock`: the sounds, every parameter, the engines' patches, the room and the voice split, in
JSON. To build it into the firmware as the factory Line-up ("make my Yacht the real one"):

```sh
python3 tools/dock_lineup_import.py ~/Downloads/yacht.dock
```

That replaces `firmware/lineups/yacht.dock` and writes `firmware/src/dock_factory.h`; rebuild the emulator (or the
firmware) and commit both files. With `--as Disco` the file becomes a new Line-up, after the others. `--check` only
checks that the header matches the files (the tests do).

## Not yet

The LED power-on sweep, USB, loading a `.dock` file on the page. Melodee's test palette GRAY appears in the palette
list (from `tests/ui_test.c`).

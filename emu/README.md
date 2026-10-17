# Dock emulator

The real firmware C code (Melodee's DSP, sequencer and UI) compiled to WebAssembly and played on an on-screen FM-1.

| File | What it is |
|---|---|
| `dock_web.c` | The panel emulator: firmware + simulated FM-1 (clock, keys, buttons, knobs, LEDs, 240×240 screen). Design after Felucca's `web/emu/felucca_web.c`. |
| `panel/` | The page and AudioWorklet, from Felucca's browser build (GPL-3.0), retitled and pointed at `dock_web.wasm`. |
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

## Not yet

Saving (no flash emulation: a reload starts fresh), the LED power-on sweep, USB. Melodee's test palette GRAY appears
in the palette list (from `tests/ui_test.c`).

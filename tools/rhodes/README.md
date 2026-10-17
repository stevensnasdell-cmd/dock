# tools/rhodes: FRodhes' Rhodes physics (the EP engine's tables)

From **FRodhes** by Leonardo Mirabal, <https://github.com/leonardoMirabal/frodhes> (0.9.2, commit `e49a49d`),
GPL-3.0-only (`SPDX-License-Identifier` in each file). The `.py` modules are FRodhes' `tools/` files, unchanged:

| File | What |
| --- | --- |
| `hammer.py` | the hammer-tip / tine contact simulation (the force pulses) |
| `pickup.py` | the electromagnetic pickup's flux (closed form) |
| `tine_fe.py` | a finite-element tine with its tuning spring (per-key partial ratios) |
| `tinebar.py` | the 2-DOF tine / tone-bar modes, with and without the damper |
| `rh_coefs.py` | the mode coefficients, bit-identical to the engine's run-time path |
| `rhodes_model.py` | the float golden model the generator calibrates with |
| `data/params.json` | the physical priors and the TYPE voicings (Dock added `WURLY`) |
| `data/tine_lengths.csv` | tine lengths per key (Rhodes service manual, as FRodhes tabulated them) |

`tools/gen_rhodes.py` (FRodhes' generator, changed by Dock) runs them and writes `build/gen/rhodes_tables.h` for
`firmware/src/eng_ep.c`. It needs numpy and scipy (`.venv-tools`, see `emu/README.md`); `tools/build.py` and
`emu/build.sh` run it. Its docstrings still name FRodhes' paths (`docs/model/...`); here the data is in `data/`.
FRodhes' design notes (`docs/model/IMPLEMENTATION-v3.md` and the research it cites) are in its repository.

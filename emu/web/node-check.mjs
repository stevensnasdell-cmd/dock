// SPDX-License-Identifier: GPL-3.0-only
// Quick check of the browser module under Node: load it, play a note on ch 1, confirm sound and a voice.
import { readFile } from 'node:fs/promises';
import { loadDock } from './dock-core.js';

const dock = await loadDock(await readFile(process.argv[2] ?? 'build/emu/dock.wasm'));
dock.midi(0x90, 36, 100);
let peak = 0;
for (let b = 0; b < 100; b++) {
  const [l, r] = dock.render(128);
  for (let i = 0; i < l.length; i++) peak = Math.max(peak, Math.abs(l[i]), Math.abs(r[i]));
}
const v = dock.voices(0);
console.log(`node-check: ${dock.rate} Hz, peak ${peak.toFixed(3)}, part 1 voices ${v}`);
if (!(peak > 0.01 && v > 0)) { console.error('node-check: FAILED (silent or no voice)'); process.exit(1); }

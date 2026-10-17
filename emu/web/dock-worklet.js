// SPDX-License-Identifier: GPL-3.0-only
// The audio thread: renders the firmware 128 frames at a time (4 of its 32-sample blocks).
// The page sends {wasm} once, then {midi: [status, d1, d2]}; it gets {ready} and {voices} back.
import { loadDock } from './dock-core.js';

class DockProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.dock = null;
    this.tick = 0;
    this.port.onmessage = async ({ data }) => {
      if (data.wasm) {
        try {
          this.dock = await loadDock(data.wasm);
          this.port.postMessage({ ready: true, rate: this.dock.rate, ctxRate: sampleRate });
        } catch (e) {
          this.port.postMessage({ error: String(e) });
        }
      } else if (data.midi && this.dock) {
        this.dock.midi(...data.midi);
      }
    };
  }

  process(_inputs, outputs) {
    const out = outputs[0];
    if (!this.dock || !out.length) return true;
    const [l, r] = this.dock.render(out[0].length);
    out[0].set(l);
    (out[1] ?? out[0]).set(out[1] ? r : l);
    if (++this.tick % 16 === 0) {           // ~46 ms: the voice meters
      this.port.postMessage({ voices: [0, 1, 2, 3].map((k) => this.dock.voices(k)) });
    }
    return true;
  }
}

registerProcessor('dock', DockProcessor);

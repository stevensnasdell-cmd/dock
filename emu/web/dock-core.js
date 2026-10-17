// SPDX-License-Identifier: GPL-3.0-only
// Loads build/emu/dock.wasm (a standalone module) and wraps its exports. Shared by the AudioWorklet
// and the Node check, so both drive the firmware the same way. The module's imports (WASI: printf,
// files, clocks from the host-test code it shares) are never needed while rendering; each becomes a stub.

export async function loadDock(bytes) {
  const module = await WebAssembly.compile(bytes);
  const imports = {};
  for (const imp of WebAssembly.Module.imports(module)) {
    if (imp.kind !== 'function') continue;
    (imports[imp.module] ??= {})[imp.name] = () => 0;
  }
  const { exports: x } = await WebAssembly.instantiate(module, imports);
  x._initialize?.();            // C constructors: the firmware's RAM tables (dsp_tables_init)
  x.dock_init();
  const rate = x.dock_rate();
  return {
    rate,
    midi: (status, d1 = 0, d2 = 0) => x.dock_midi(status, d1, d2),
    voices: (part) => x.dock_voices(part),
    // renders frames (multiple of 32) and returns views on the module's buffers (valid until the next call)
    render(frames) {
      const n = x.dock_render(frames);
      const mem = x.memory.buffer;
      return [new Float32Array(mem, x.dock_left(), n), new Float32Array(mem, x.dock_right(), n)];
    },
  };
}

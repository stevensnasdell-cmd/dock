// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Steven Snasdell
// The emulator keeps your Line-up across a reload, as the page does it: the built dock_web.wasm loaded as
// worklet.js loads it, a band edit (KNOB 1 on HOME), hold SAVE, the changed flash through worklet.js's
// storeSectors and the page's base64 (index.html saveFlash / loadFlash), then a new instance from those sectors
// (restoreSectors): yours plays at power-on, marked, and exports the same text.   node emu/panel/reload_test.mjs DIR
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { storeSectors, restoreSectors, exportLineup } from "./worklet.js";

const dir = process.argv[2] || "build/emu/panel";
const wasm = readFileSync(join(dir, "dock_web.wasm"));
const B = { SAVE: 7 };

async function boot(sectors) {
  const mod = await WebAssembly.compile(wasm), imports = {};
  for (const i of WebAssembly.Module.imports(mod))
    if (i.kind === "function") (imports[i.module] ??= {})[i.name] = () => 0;
  const ex = (await WebAssembly.instantiate(mod, imports)).exports;
  ex._initialize();
  ex.web_nor_erase();
  restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), sectors);
  ex.web_master(700);
  ex.web_boot();
  return ex;
}
const ms = (ex, n) => { const end = ex.web_now_ms() + n; while (ex.web_now_ms() < end) ex.web_render(128); };
// the page's localStorage round trip (index.html saveFlash / loadFlash)
const keep = (sectors) => {
  const raw = {};
  for (const [off, u8] of Object.entries(sectors)) raw[off] = Buffer.from(u8).toString("base64");
  return JSON.parse(JSON.stringify(raw));
};
const give = (raw) => Object.fromEntries(Object.entries(raw).map(([o, b]) => [o, Uint8Array.from(Buffer.from(b, "base64"))]));

let fails = 0;
const check = (what, ok) => { console.log(`reload: ${what.padEnd(70)} ${ok ? "ok" : "FAIL"}`); fails += !ok; };

const a = await boot({});
ms(a, 500);
const factory = exportLineup(a);
check("a fresh flash: Yacht, the factory one", a.web_lineup() === 0 && !a.web_lineup_mine() && factory.name === "yacht.dock");
a.web_enc(3, 9);                                     // KNOB 1 on HOME: the Bass's first knob
ms(a, 300);
const edited = exportLineup(a);
a.web_buttons(1 << B.SAVE); ms(a, 1200); a.web_buttons(0); ms(a, 300);   // hold SAVE
check("KNOB 1, then hold SAVE: yours, marked", a.web_lineup_mine() === 1 && edited.text !== factory.text);
const stored = keep(storeSectors(new Uint8Array(a.memory.buffer, a.web_nor(), a.web_nor_size())));
check(`the page keeps ${Object.keys(stored).length} sectors (base64, as localStorage holds them)`, Object.keys(stored).length > 0);

const b = await boot(give(stored));                  // the reload
ms(b, 500);
const again = exportLineup(b);
check("after the reload: yours plays at power-on, marked", b.web_lineup() === 0 && b.web_lineup_mine() === 1);
check("  every value as saved (its export, the same text)", again.text === edited.text);
console.log(fails ? `reload: ${fails} FAILED` : "reload: all passed");
process.exit(fails ? 1 : 0);

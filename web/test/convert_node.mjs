#!/usr/bin/env node
// Headless run of the web converter: node web/test/convert_node.mjs <DQ4.apk> <main.105....obb> <outdir> [--lang en|ja|ko|zh-tw|zh-cn] [--layout landscape|split|portrait] [--jobs N] [--skip-md5] [--zip file.zip] [--mem]
// Exit code 0 only if the pack files are byte-identical (sha256) to the reference conversion.
import { existsSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
if (!existsSync(join(here, ".dist", "node-run.mjs"))) execFileSync("node", [join(here, "..", "build.mjs")], { stdio: "inherit" });
const { runNode } = await import("./.dist/node-run.mjs");

const args = process.argv.slice(2), pos = [], opt = { lang: "en", layout: "landscape", skipMd5: false };
let jobs, mem = false, peak = 0, zip;
setInterval(() => { peak = Math.max(peak, process.memoryUsage().rss); }, 50).unref();
for (let i = 0; i < args.length; i++) {
  const a = args[i];
  if (a === "--lang") opt.lang = args[++i];
  else if (a === "--layout") opt.layout = args[++i];
  else if (a === "--jobs") jobs = +args[++i];
  else if (a === "--skip-md5") opt.skipMd5 = true;
  else if (a === "--mem") mem = true;
  else if (a === "--zip") zip = args[++i];
  else pos.push(a);
}
if (pos.length !== 3) { console.error("usage: convert_node.mjs <apk> <obb> <outdir> [--lang L] [--layout L] [--jobs N] [--skip-md5] [--zip file.zip] [--mem]"); process.exit(2); }

const t0 = performance.now();
let last = "";
let rep;
try { rep = await runNode(pos[0], pos[1], pos[2], opt, jobs, p => {
  if (mem && globalThis.gc && p.frac === 1) globalThis.gc();
  const s = `${p.phase} ${Math.round(p.frac * 100)}%`;
  if (s !== last && (p.frac === 1 || p.frac === 0 || Math.round(p.frac * 100) % 10 === 0)) { console.log(`[${((performance.now() - t0) / 1000).toFixed(1)}s] ${s}${mem ? `  rss ${(process.memoryUsage().rss / 1048576).toFixed(0)} MB (peak ${(peak / 1048576).toFixed(0)}; main heap ${(process.memoryUsage().heapUsed / 1048576).toFixed(0)}, buffers ${(process.memoryUsage().arrayBuffers / 1048576).toFixed(0)})` : ""}`); last = s; }
}, zip); } catch (e) { console.error(`ERROR${e.code ? " (" + e.code + ")" : ""}: ${e.message}`); process.exit(2); }
console.log("timing (s):", Object.entries(rep.seconds).map(([k, v]) => `${k} ${v.toFixed(1)}`).join(", "), `| total ${((performance.now() - t0) / 1000).toFixed(1)}`);
for (const [f, h] of Object.entries(rep.sha256)) console.log(`${h}  pack/${f}  (${rep.sizes[f]} bytes)`);
console.log(`${rep.objects} objects, ${rep.failures.length} failures, ${rep.warnings} warnings`);
for (const f of rep.failures.slice(0, 20)) console.log("  FAIL", f);
console.log(rep.identical ? "pack verified: identical to the reference conversion" : "WARNING: pack differs from the reference conversion");
process.exit(rep.identical && !rep.failures.length ? 0 : 1);

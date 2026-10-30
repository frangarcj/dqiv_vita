// The page: pick APK + OBB, options, output, convert (all in this browser tab).
import { Pool, type WorkerLike } from "./pool";
import { convertAll, InputError, KNOWN_MD5, PACK_SHA256, type Phase, type Progress, type Report } from "./pipeline";
import { DirSink, ZipSink, BlobZipOut, FileZipOut, type Sink } from "./sinks";
import { apply, setUiLang, uiLang, t } from "./i18n";

declare const __WORKER_SRC__: string;
const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

function spawn(url: string): WorkerLike {
  const w = new Worker(url);
  const wl: WorkerLike = { postMessage: (m, tr) => w.postMessage(m, tr ?? []), terminate: () => w.terminate(), onmessage: null, onerror: null };
  w.onmessage = e => wl.onmessage?.(e.data);
  w.onerror = e => wl.onerror?.(e);
  return wl;
}
let workerUrl: string | null = null;
const query = new URLSearchParams(location.search);
/** Each worker holds one inflated Unity file plus its converted blobs (tens of MB); few workers are almost as fast as many. */
export function defaultWorkers(): number {
  const hw = navigator.hardwareConcurrency || 4, mem = (navigator as any).deviceMemory as number | undefined;   // deviceMemory: Chrome only, GB (capped at 8)
  const byMem = mem === undefined ? 3 : mem <= 2 ? 1 : mem <= 4 ? 2 : 3;
  return Math.max(1, Math.min(byMem, hw - 1));
}
function newPool(n = +(query.get("workers") ?? 0) || defaultWorkers()) {
  workerUrl ??= URL.createObjectURL(new Blob([__WORKER_SRC__], { type: "text/javascript" }));
  const url = workerUrl;
  return new Pool(n, () => spawn(url));
}

const files: { apk: File | null; obb: File | null } = { apk: null, obb: null };
const verified = { apk: false, obb: false };
let dirHandle: FileSystemDirectoryHandle | null = null;
let running = false, finished = false;
const canDir = "showDirectoryPicker" in window;

const fmtSize = (n: number) => (n >= 1 << 30 ? (n / (1 << 30)).toFixed(2) + " GB" : (n / (1 << 20)).toFixed(1) + " MB");
const fmtTime = (s: number) => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, "0")}`;

// ------------------------------------------------------------------ file selection (+ early MD5 check)
async function pick(kind: "apk" | "obb", f: File | null) {
  files[kind] = f; verified[kind] = false;
  const badge = $(kind + "-badge");
  $(kind + "-name").textContent = f ? `${f.name} (${fmtSize(f.size)})` : "";
  badge.hidden = !f;
  clearResult(); update();
  if (!f) return;
  badge.className = "badge"; badge.textContent = t("verifying");
  const pool = newPool(1);
  try {
    const sum = await pool.run<string>({ t: "md5", file: f });
    if (files[kind] !== f) return;
    verified[kind] = sum === KNOWN_MD5[kind];
    badge.className = "badge " + (verified[kind] ? "ok" : "bad");
    badge.textContent = verified[kind] ? t("verified") : `MD5 ${sum}`;
    if (!verified[kind]) showError(errText(new InputError("md5", kind, "", sum)));
  } catch (e) {
    badge.hidden = true;
  } finally { await pool.close(); }
  update();
}
for (const k of ["apk", "obb"] as const) {
  $(k + "-btn").onclick = () => $<HTMLInputElement>(k + "-in").click();
  $<HTMLInputElement>(k + "-in").onchange = e => pick(k, (e.target as HTMLInputElement).files?.[0] ?? null);
}

// ------------------------------------------------------------------ output choice
const radios = () => [...document.querySelectorAll<HTMLInputElement>('input[name="out"]')];
const outMode = () => radios().find(r => r.checked)!.value as "dir" | "zip";
function setupOutput() {
  const dirR = radios().find(r => r.value === "dir")!, zipR = radios().find(r => r.value === "zip")!;
  if (!canDir) { dirR.disabled = true; $("opt-dir").classList.add("disabled"); zipR.checked = true; }
  else dirR.checked = true;
  radios().forEach(r => r.onchange = () => { $("dir-row").hidden = outMode() !== "dir"; update(); });
  $("dir-row").hidden = outMode() !== "dir";
  $("support").textContent = t(canDir ? "support_dir" : "support_nodir");
}
$("dir-btn").onclick = async () => {
  try {
    dirHandle = await (window as any).showDirectoryPicker({ id: "dqiv-data", mode: "readwrite" });
    $("dir-name").textContent = t("dir_chosen", { name: dirHandle!.name });
  } catch { /* cancelled */ }
  update();
};

function update() {
  const ready = !!files.apk && !!files.obb && (outMode() === "zip" || !!dirHandle);
  const go = $<HTMLButtonElement>("go");
  go.disabled = running || !ready;
  $("hint").textContent = running ? "" : !files.apk || !files.obb ? t("need_files") : outMode() === "dir" && !dirHandle ? t("need_dir") : "";
  go.textContent = running ? t("running") : finished ? t("again") : t("convert");
}

// ------------------------------------------------------------------ progress
const PHASES: Phase[] = ["check", "extract", "enumerate", "convert", "pack", "write"];
const WEIGHT: Record<Phase, number> = { check: 8, extract: 2, enumerate: 15, convert: 30, pack: 3, write: 42, obb: 0 };
const frac: Record<string, number> = {};
let started = 0, timer = 0;

function buildPhases() {
  const ol = $("phases"); ol.textContent = "";
  for (const p of [...PHASES, "obb" as Phase]) {
    const li = document.createElement("li");
    li.id = "ph-" + p; li.className = "pending";
    li.innerHTML = '<span class="mark"></span><span class="lbl"></span><span class="pct"></span>';
    li.querySelector(".lbl")!.textContent = t("ph_" + p);
    ol.append(li);
  }
}
function onProgress(p: Progress) {
  frac[p.phase] = p.frac;
  const li = $("ph-" + p.phase);
  li.className = p.frac >= 1 ? "done" : "active";
  li.querySelector(".pct")!.textContent = p.frac >= 1 ? "" : Math.floor(p.frac * 100) + "%";
  let total = 0;
  for (const ph of PHASES) total += WEIGHT[ph] * (frac[ph] ?? 0);
  const pct = Math.min(100, total);
  $("bar-i").style.width = pct + "%";
  $("bar").setAttribute("aria-valuenow", String(Math.round(pct)));
}

// ------------------------------------------------------------------ result / errors
function clearResult() { $("result").textContent = ""; }
function showError(text: string) {
  const d = document.createElement("div");
  d.className = "msg err"; d.setAttribute("role", "alert");
  const h = document.createElement("strong"); h.textContent = t("err_title") + "\n";
  d.append(h, document.createTextNode(text));
  $("result").append(d);
}
function errText(e: unknown): string {
  if (e instanceof InputError) {
    const which = e.which.toUpperCase();
    if (e.code === "md5") return t("err_md5", { which, want: KNOWN_MD5[e.which], got: e.got ?? "?" });
    return t("err_" + e.code, { which });
  }
  const err = e as Error;
  if (err?.name === "AbortError") return t("err_cancelled");
  return t("err_generic", { msg: String(err?.message ?? e) });
}
function showDone(rep: Report, mode: "dir" | "zip") {
  const r = $("result"); r.textContent = "";
  const add = (cls: string, html: string, isText = false) => {
    const d = document.createElement("div"); d.className = "msg " + cls;
    if (isText) d.textContent = html; else d.innerHTML = html;
    r.append(d); return d;
  };
  add(rep.identical ? "ok" : "warn", rep.identical ? t("ok_identical") : t("warn_differs"), true);
  if (rep.failures.length) add("warn", t("warn_failures", { n: rep.failures.length }), true);
  const det = document.createElement("details");
  det.innerHTML = "<summary></summary><pre></pre>";
  det.querySelector("summary")!.textContent = t("hashes");
  det.querySelector("pre")!.textContent = Object.keys(PACK_SHA256).map(f => `${rep.sha256[f]}  pack/${f}${rep.sha256[f] === PACK_SHA256[f] ? "" : "  (differs)"}`).join("\n");
  r.append(det);
  const h = document.createElement("h2"); h.textContent = t("next_title"); r.append(h);
  const ol = document.createElement("ol");
  for (const k of [mode === "dir" ? "next1_dir" : "next1_zip", "next2", "next3", "next4"]) { const li = document.createElement("li"); li.innerHTML = t(k); ol.append(li); }
  r.append(ol);
}

// ------------------------------------------------------------------ run
async function run() {
  if (running || !files.apk || !files.obb) return;
  if (!globalThis.crypto?.subtle) { clearResult(); showError(t("err_insecure")); return; }
  running = true; finished = false; clearResult(); update();
  const mode = outMode();
  let sink: Sink, blobOut: BlobZipOut | null = null;
  const pool = newPool();
  try {
    if (mode === "dir") sink = new DirSink(dirHandle!);
    else {
      let out: BlobZipOut | FileZipOut;
      const pickSave = (window as any).showSaveFilePicker;
      if (pickSave && !query.has("blobzip")) {      // Chrome/Edge: stream the zip straight to disk, nothing is accumulated in memory
        const fh = await pickSave.call(window, { suggestedName: "dqiv.zip", types: [{ description: "ZIP", accept: { "application/zip": [".zip"] } }] });
        out = new FileZipOut(await fh.createWritable());
      } else out = blobOut = new BlobZipOut();
      sink = new ZipSink(out, "dqiv/");
    }
    for (const k of Object.keys(frac)) delete frac[k];
    buildPhases(); $("progress").hidden = false;
    started = performance.now();
    timer = window.setInterval(() => { $("elapsed").textContent = t("elapsed", { s: fmtTime((performance.now() - started) / 1000) }); }, 500);
    const rep = await convertAll({ apk: files.apk, obb: files.obb }, { lang: $<HTMLSelectElement>("lang").value, layout: $<HTMLSelectElement>("layout").value }, sink, pool, onProgress);
    if (blobOut) {
      const a = document.createElement("a");
      a.href = URL.createObjectURL(blobOut.result!); a.download = "dqiv.zip";
      document.body.append(a); a.click(); a.remove();
      setTimeout(() => URL.revokeObjectURL(a.href), 60_000);
    }
    showDone(rep, mode); finished = true;
  } catch (e) {
    console.error(e);
    showError(errText(e));
  } finally {
    clearInterval(timer);
    await pool.close();
    running = false; update();
  }
}

// ------------------------------------------------------------------ init
if (query.has("test")) (window as any).__dqiv = { DirSink, ZipSink, BlobZipOut, FileZipOut, convertAll, newPool, defaultWorkers };      // hook for the headless browser smoke test
$("go").onclick = run;
$<HTMLSelectElement>("ui-lang").value = uiLang();
$<HTMLSelectElement>("ui-lang").onchange = e => { setUiLang((e.target as HTMLSelectElement).value); setupOutput(); update(); };
apply(); setupOutput(); update();

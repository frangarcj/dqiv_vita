# DQIV web data builder

A static web page that turns **your own** Dragon Quest IV Android files (Google Play v1.0.3 APK + OBB `main.105`) into the `ux0:data/dqiv/` folder of the native PS Vita port. It replaces `tools/make_data.py` + `tools/convert` for end users: nothing to install, nothing is uploaded (everything runs in the browser tab, in Web Workers), and the site contains no game data.

* `index.html` + `dist/app.js` is the whole site (the worker is inlined in `app.js`, no CDN, no other requests): serve this folder from any static host (GitHub Pages, `python3 -m http.server -d web`, ...) or open `index.html` from disk.
* Output goes either straight into a folder picked with the File System Access API (Chrome, Edge, Opera: e.g. a mounted memory card or a local folder) or into one store-only `dqiv.zip` (about 400 MB, contains a `dqiv/` folder): streamed to disk with `showSaveFilePicker` where available, otherwise (Firefox, Safari) built as a Blob that the browser keeps outside the JS heap.
* The converted pack is checked against the sha256 of the reference conversion (`PACK_SHA256` in `src/pipeline.ts`, the same constants as `tools/make_data.py`) and the page says "identical to the reference conversion" when it matches.

## What is produced

Exactly what `make_data.py` writes: `lib/` (libdq4core.so, libsdlib_android.so from the APK), `assets/` (filelist.bin, ct.inf), `main.obb` (your OBB, copied byte for byte), `pack/{index.bin,meta.pak,mesh.pak,tex.pak,blob.pak}`, `cg/*.bin` (precompiled GPU programs from `extras/cg`, embedded in the script at build time) and `config.txt` (language and, if not the default, `layout=`). The pack files are byte-identical to the Python converter's.

## Design

```
page (main thread)                        workers (default 3; 2 if navigator.deviceMemory <= 4, 1 if <= 2)
  UI, MD5 early check       ---------->    md5 of a File (4 MB slices)
  pipeline.ts:                             init: open APK/OBB as zip (central directory only)
   1 check zip + md5                       enum:    inflate a Unity file, read its object table (+ Shader names)
   2 lib/ assets/ cg/ config.txt           convert (pass 1): inflate, read objects with the TypeTree reader,
   3 enumerate all 21 608 serialized files           convert texture/mesh/material/shader/prefab/anim/font/text,
     -> registry (gid per exportable object)         return size + SHA-1 of every blob, blobs of the small paks
   4 convert in parallel     <----------   (non-texture kinds, 55 MB in all); texture blobs are dropped
   5 pack.ts layout: sort by Resources path, dedupe by SHA-1 -> every offset and index.bin, from sizes/hashes only
   6 stream index.bin, meta/mesh/blob.pak from memory, then tex.pak: textures are converted a second time
     (objs: batches of <= 24 blobs / 6 MB, at most 2 per worker in flight) and written in pack order;
     every file is hashed (incremental SHA-256) while it is written and compared with the reference
   sink: folder (DirSink) | ZIP streamed into a file (showSaveFilePicker) | ZIP accumulated as a Blob (Firefox/Safari);
         main.obb is copied from the user's File in parallel (by reference in a Blob zip)
```

Memory: a pak is never assembled (the layout needs only sizes and hashes, so the files are written blob by blob in their final order), the 150 MB of textures are not kept (the second conversion costs about 1.5 s of CPU), inflate buffers are reused per worker, and Unity file bytes are views, not copies. Live data in the page is about 60 MB plus what the workers have in flight (`measureUserAgentSpecificMemory`: 140-175 MB peak in total); the rest of the process footprint is allocator and GC slack.

* The inputs are never loaded whole: `source.ts` slices the `File` on demand, `zip.ts` reads the central directory and inflates single entries with the vendored [fflate](https://github.com/101arrowz/fflate) (`vendor/`, MIT). The OBB has 21 606 deflated Unity files (about 490 MB uncompressed); each worker holds one at a time.
* `unity.ts` is the Unity 4.7.2 serialized-file (format 9) reader plus a TypeTree value reader that mirrors UnityPy 1.25's. The game's files carry **no** type trees (stripped player build), so the trees of the 45 classes that occur come from `src/typetrees.ts`, dumped from UnityPy's class database by `tools/gen_typetrees.py` (only needed to regenerate it). `src/classids.ts` is the UnityPy class id table.
* `tex.ts` `mesh.ts` `material.ts` `misc.ts` `prefab.ts` `blob.ts` `pack.ts` are line-by-line ports of `tools/convert/dqivconv/*` and `write_pack`; the layouts are documented in `tools/convert/FORMAT.md`. Differences by design: only the options the make_data run uses (RGB24 to RGB565, RGBA32 kept as RGBA8888); DXT1/DXT5 textures are not supported (the game has none); the raw shader dumps (`pack_shaders`) are not written (they are not part of the output).
* `pipeline.ts` runs unchanged in the browser and in Node; `node-env.ts` / `node-run.ts` / `worker.node.ts` are the Node glue used by the test harness.
* MD5 (`md5.ts`) is incremental because the OBB cannot be hashed in one go; SHA-256 uses `crypto.subtle` on the finished pack files.

## Build

```sh
cd web
npm install          # esbuild, typescript (dev only; fflate is vendored)
npm run build        # -> dist/app.js (committed) and test/.dist/ (node harness, ignored); also regenerates src/cgdata.ts from ../extras/cg
npm run check        # same + tsc type check
```

`dist/app.js` is committed so the site can be served as is. Rebuild and commit it after changing `src/`.

## Test (headless, Node >= 20)

```sh
node web/test/convert_node.mjs DQ4.apk main.105.com.square_enix.android_googleplay.dq4.obb /tmp/dqiv_data [--lang ja] [--layout split] [--jobs 2] [--zip out.zip] [--mem]
```

Runs the same pipeline with worker_threads (2 by default; `--zip` exercises the streaming ZIP sink, `--mem` prints RSS per phase) and exits 0 only if the five pack files are byte-identical (sha256) to the reference conversion. The rest of the output (`lib/ assets/ cg/ main.obb`) can be compared with `python3 tools/make_data.py --no-pack ...` using `diff -r`.

## Browser support and limits

| | |
|---|---|
| Chrome / Edge / Opera | everything, including writing into a folder (File System Access API; pick an empty folder, then copy it to the card) |
| Firefox / Safari | everything except the folder picker: the result is a `dqiv.zip` download (built as a Blob, so it is held by the browser, about 400 MB) |
| memory | streamed and bounded. Node peak RSS: 0.42 GB (1 worker), about 0.5-0.6 GB (2-3); it was 1.05-1.7 GB. Chrome (whole process tree, idle browser is about 0.8 GB): about +0.5-0.9 GB, was +1.25 GB; the page's live JS + ArrayBuffer data is 140-175 MB, the rest is allocator/GC slack. A Blob ZIP (Firefox/Safari) is additionally held by the browser (about 0.4 GB, outside the JS heap) |
| not supported | other game versions (md5 is checked), zip64 archives |

The page was developed against Chrome (headless smoke test: select files, ZIP download and OPFS folder output); Firefox and Safari are untested.

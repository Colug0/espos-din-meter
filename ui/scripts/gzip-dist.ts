// SPDX-FileCopyrightText: 2026 Thomas Christoffel
// SPDX-License-Identifier: Apache-2.0
//
// dist/ -> ../ui-dist/: every file gzipped as <name>.gz. The firmware serves
// <path>.gz with Content-Encoding: gzip; espos_project_ui_partition() packs
// ui-dist/ into the LittleFS image. ui-dist/ is committed, so a firmware
// build needs no Node.
import { promises as fs } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { gzipSync } from "node:zlib";

const src = fileURLToPath(new URL("../dist/", import.meta.url));
const dst = fileURLToPath(new URL("../../ui-dist/", import.meta.url));

async function walk(dir: string): Promise<string[]> {
  const out: string[] = [];
  for (const e of await fs.readdir(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) out.push(...(await walk(p)));
    else out.push(p);
  }
  return out;
}

await fs.rm(dst, { recursive: true, force: true });
let plain = 0;
let packed = 0;
for (const file of await walk(src)) {
  const rel = path.relative(src, file);
  const data = await fs.readFile(file);
  // mtime 0 in the gzip header, so an unchanged bundle packs to the same bytes.
  const gz = gzipSync(data, { level: 9 });
  const target = path.join(dst, rel + ".gz");
  await fs.mkdir(path.dirname(target), { recursive: true });
  await fs.writeFile(target, gz);
  plain += data.length;
  packed += gz.length;
  console.log(`${rel}  ${data.length} -> ${gz.length}`);
}
console.log(`total ${plain} -> ${packed} bytes gzipped`);

// SPDX-FileCopyrightText: 2026 Thomas Christoffel
// SPDX-License-Identifier: Apache-2.0
//
// ▲/▼ through a few brightness levels. The settings themselves are
// display.brightness and display.night_bright (Config tab, any value 1..100);
// these buttons step between levels that look evenly spaced on the panel.
import { useEffect, useState } from "preact/hooks";
import { errText, get, put } from "../../../espos/ui/src/api";
import { Msg } from "../../../espos/ui/src/app";

const DAY_LEVELS = [1, 10, 25, 50, 75, 100];
// 0 = no night setting: the day brightness stays on at night.
const NIGHT_LEVELS = [0, ...DAY_LEVELS];

type Key = "brightness" | "night_bright";

function step(levels: number[], current: number, dir: 1 | -1): number {
  if (dir > 0) return levels.find((l) => l > current) ?? levels[levels.length - 1]!;
  return [...levels].reverse().find((l) => l < current) ?? levels[0]!;
}

function Stepper({ label, value, levels, busy, onSet, note }: {
  label: string;
  value: number | undefined;
  levels: number[];
  busy: boolean;
  onSet: (v: number) => void;
  note?: string;
}) {
  const known = value !== undefined;
  return (
    <div class="row" style="align-items:center;gap:.5rem;margin:.3rem 0">
      <span style="width:9rem">{label}</span>
      <button type="button" title="Darker" disabled={busy || !known || value! <= levels[0]!}
        onClick={() => onSet(step(levels, value!, -1))}>▼</button>
      <span class="mono" style="width:3.5rem;text-align:center">{known ? (value === 0 ? "off" : `${value} %`) : "…"}</span>
      <button type="button" title="Brighter" disabled={busy || !known || value! >= levels[levels.length - 1]!}
        onClick={() => onSet(step(levels, value!, 1))}>▲</button>
      {note && <span class="muted small">{note}</span>}
    </div>
  );
}

export function BrightnessCard() {
  const [vals, setVals] = useState<Partial<Record<Key, number>>>({});
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState("");

  useEffect(() => {
    get<{ display?: Partial<Record<Key, number>> }>("/config").then(
      (d) => setVals(d.display ?? {}),
      (e: unknown) => setMsg(errText(e)),
    );
  }, []);

  async function set(key: Key, v: number) {
    setBusy(true); setMsg("");
    try {
      await put("/config", { display: { [key]: v } });
      setVals((old) => ({ ...old, [key]: v }));
    } catch (e) {
      setMsg(errText(e));
    } finally {
      setBusy(false);
    }
  }

  return (
    <section class="card wide">
      <h2>Display</h2>
      <Stepper label="Brightness" value={vals.brightness} levels={DAY_LEVELS} busy={busy}
        onSet={(v) => void set("brightness", v)} />
      <Stepper label="Night brightness" value={vals.night_bright} levels={NIGHT_LEVELS} busy={busy}
        onSet={(v) => void set("night_bright", v)} note="while Signal K reports night; off = same as day" />
      <Msg text={msg} />
    </section>
  );
}

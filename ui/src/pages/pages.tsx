// SPDX-FileCopyrightText: 2026 Thomas Christoffel
// SPDX-License-Identifier: Apache-2.0
//
// The DIN Meter's page list: one or two Signal K values per page, one page
// per encoder step. Pages are configured here only, never on the device.
import { useEffect, useRef, useState } from "preact/hooks";
import { errText, get, put } from "../../../espos/ui/src/api";
import { Msg } from "../../../espos/ui/src/app";
import { BrightnessCard } from "./brightness";

interface Conversion { id: string; label: string; unit: string }

interface DinValue {
  label?: string;
  path: string;
  type: "number" | "string" | "boolean";
  unit?: string;
  decimals?: number;
  conversion?: string;
}

interface DinPage extends DinValue {
  id: string;
  label: string;
  kind?: "single" | "dual";
  values?: DinValue[];
  updatePeriodMs: number;
  staleTimeoutMs: number;
}

interface DinPagesDoc { pages: DinPage[] }

function blankValue(label?: string): DinValue {
  return { path: "", type: "number", ...(label !== undefined ? { label } : {}) };
}

function blankPage(): DinPage {
  return {
    id: `page-${Date.now().toString(36)}`,
    label: "",
    kind: "single",
    path: "",
    type: "number",
    updatePeriodMs: 1000,
    staleTimeoutMs: 5000,
  };
}

function ValueFields({ value, conversions, onChange }: {
  value: DinValue;
  conversions: Conversion[];
  onChange: (patch: Partial<DinValue>) => void;
}) {
  const isNumber = value.type === "number";
  const hasConversion = !!value.conversion && value.conversion !== "none";
  return (
    <div class="row" style="flex-wrap:wrap;gap:.4rem">
      <input type="text" class="mono" style="width:15rem" value={value.path} placeholder="electrical.batteries.0.voltage"
        onInput={(e) => onChange({ path: (e.target as HTMLInputElement).value })} />
      <select value={value.type} onChange={(e) => onChange({ type: (e.target as HTMLSelectElement).value as DinValue["type"] })}>
        <option value="number">number</option>
        <option value="string">string</option>
        <option value="boolean">boolean</option>
      </select>
      <select style="width:8rem" disabled={!isNumber} value={value.conversion ?? "none"}
        onChange={(e) => onChange({ conversion: (e.target as HTMLSelectElement).value })}>
        <option value="none">raw value</option>
        {conversions.map((c) => <option key={c.id} value={c.id}>{c.label}</option>)}
      </select>
      <input type="text" style="width:3.5rem" disabled={!isNumber || hasConversion}
        title={hasConversion ? "Set by the conversion" : "Unit"} placeholder="unit"
        value={hasConversion ? conversions.find((c) => c.id === value.conversion)?.unit ?? "" : value.unit ?? ""}
        onInput={(e) => onChange({ unit: (e.target as HTMLInputElement).value })} />
      <label class="muted small">dec. <input type="number" style="width:3rem" min="0" max="10" disabled={!isNumber}
        value={value.decimals ?? 0} onInput={(e) => onChange({ decimals: Number((e.target as HTMLInputElement).value) })} /></label>
    </div>
  );
}

function download(doc: DinPagesDoc) {
  const blob = new Blob([JSON.stringify(doc, null, 2)], { type: "application/json" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "din-meter-pages.json";
  a.click();
  URL.revokeObjectURL(a.href);
}

export function PagesPage() {
  const [pages, setPages] = useState<DinPage[] | null>(null);
  const [conversions, setConversions] = useState<Conversion[]>([]);
  const [msg, setMsg] = useState("");
  const [ok, setOk] = useState("");
  const [saving, setSaving] = useState(false);
  const fileInput = useRef<HTMLInputElement>(null);

  const load = () =>
    get<DinPagesDoc>("/pages").then(
      (d) => setPages(d.pages),
      (e: unknown) => setMsg(errText(e)),
    );
  useEffect(() => {
    void load();
    get<{ conversions: Conversion[] }>("/pages/conversions").then(
      (d) => setConversions(d.conversions),
      (e: unknown) => setMsg(errText(e)),
    );
  }, []);

  function update(i: number, patch: Partial<DinPage>) {
    if (!pages) return;
    const next = pages.slice();
    next[i] = { ...next[i]!, ...patch };
    setPages(next);
  }
  function updateValue(i: number, vi: number, patch: Partial<DinValue>) {
    if (!pages) return;
    const values = (pages[i]!.values ?? [blankValue("A"), blankValue("B")]).slice();
    values[vi] = { ...values[vi]!, ...patch };
    update(i, { values });
  }
  function setKind(i: number, kind: "single" | "dual") {
    if (!pages) return;
    if (kind === "dual" && !pages[i]!.values) {
      update(i, { kind, values: [blankValue("A"), blankValue("B")] });
    } else {
      update(i, { kind });
    }
  }
  function move(i: number, delta: number) {
    if (!pages) return;
    const j = i + delta;
    if (j < 0 || j >= pages.length) return;
    const next = pages.slice();
    [next[i], next[j]] = [next[j]!, next[i]!];
    setPages(next);
  }

  async function save(doc: DinPagesDoc) {
    setSaving(true); setMsg(""); setOk("");
    try {
      await put<{ changed: boolean }>("/pages", doc);
      setOk("Saved.");
      await load();
    } catch (e) {
      setMsg(errText(e));
    } finally {
      setSaving(false);
    }
  }

  async function importFile(e: Event) {
    const input = e.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = "";
    if (!file) return;
    try {
      const doc = JSON.parse(await file.text()) as DinPagesDoc;
      // The device validates; saving straight away shows its verdict.
      await save(doc);
    } catch (err) {
      setMsg(err instanceof SyntaxError ? `Not a JSON file: ${err.message}` : errText(err));
    }
  }

  return (
    <>
      <h1>Pages</h1>
      <div class="grid">
        <BrightnessCard />
        <section class="card wide">
          <h2>Displayed pages</h2>
          <p class="muted small">
            One page per encoder step. "Single value" shows one big value; "two values" shows
            two side by side (e.g. state of charge and current).
          </p>
          {pages?.map((p, i) => (
            <div key={p.id} class="card" style="margin-bottom:.6rem">
              <div class="row" style="flex-wrap:wrap;gap:.4rem;align-items:center">
                <input type="text" style="width:8rem" value={p.label} placeholder="Battery"
                  onInput={(e) => update(i, { label: (e.target as HTMLInputElement).value })} />
                <select value={p.kind ?? "single"} onChange={(e) => setKind(i, (e.target as HTMLSelectElement).value as "single" | "dual")}>
                  <option value="single">single value</option>
                  <option value="dual">two values</option>
                </select>
                <label class="muted small">update <input type="number" style="width:5rem" min="100" value={p.updatePeriodMs}
                  onInput={(e) => update(i, { updatePeriodMs: Number((e.target as HTMLInputElement).value) })} /> ms</label>
                <label class="muted small">stale after <input type="number" style="width:5rem" min={p.updatePeriodMs} value={p.staleTimeoutMs}
                  onInput={(e) => update(i, { staleTimeoutMs: Number((e.target as HTMLInputElement).value) })} /> ms</label>
                <span style="margin-left:auto" />
                <button type="button" title="Move up" disabled={i === 0} onClick={() => move(i, -1)}>↑</button>
                <button type="button" title="Move down" disabled={i === pages.length - 1} onClick={() => move(i, 1)}>↓</button>
                <button type="button" class="danger" title="Remove" onClick={() => setPages(pages.filter((_, j) => j !== i))}>✕</button>
              </div>
              {(p.kind ?? "single") === "single" ? (
                <div style="margin-top:.4rem">
                  <ValueFields value={p} conversions={conversions} onChange={(patch) => update(i, patch)} />
                </div>
              ) : (
                <div style="margin-top:.4rem;display:flex;flex-direction:column;gap:.4rem">
                  {[0, 1].map((vi) => (
                    <div key={vi} class="row" style="align-items:center;gap:.4rem">
                      <input type="text" style="width:5rem" placeholder={vi === 0 ? "SoC" : "Current"}
                        value={p.values?.[vi]?.label ?? ""}
                        onInput={(e) => updateValue(i, vi, { label: (e.target as HTMLInputElement).value })} />
                      <ValueFields value={p.values?.[vi] ?? blankValue()} conversions={conversions}
                        onChange={(patch) => updateValue(i, vi, patch)} />
                    </div>
                  ))}
                </div>
              )}
            </div>
          ))}
          <div class="row" style="margin-top:.5rem;flex-wrap:wrap;gap:.4rem">
            <button type="button" onClick={() => setPages([...(pages ?? []), blankPage()])}>Add page</button>
            <button class="primary" type="button" onClick={() => pages && void save({ pages })} disabled={saving || !pages}>
              {saving ? "Saving…" : "Save"}
            </button>
            <span style="margin-left:auto" />
            <button type="button" disabled={!pages} onClick={() => pages && download({ pages })}>Export</button>
            <button type="button" disabled={saving} onClick={() => fileInput.current?.click()}>Import</button>
            <input ref={fileInput} type="file" accept="application/json,.json" hidden onChange={(e) => void importFile(e)} />
          </div>
          <Msg text={msg} />
          <Msg text={ok} kind="ok" />
        </section>
      </div>
    </>
  );
}

import { useEffect, useState } from "react";

import { api } from "../api/client";

type Row = Record<string, string | number | null>;

export default function NpcsPage() {
  const [which, setWhich] = useState<"classes" | "spawnClasses" | "types" | "active">("classes");
  const [rows, setRows] = useState<Row[]>([]);
  const [err, setErr] = useState<string | null>(null);

  const [spawn, setSpawn] = useState({ systemID: 0, typeID: 0, x: 0, y: 0, z: 0 });

  async function refresh() {
    setErr(null);
    try {
      const path =
        which === "classes"      ? "/api/v1/npc-classes" :
        which === "spawnClasses" ? "/api/v1/npc-spawn-classes" :
        which === "types"        ? "/api/v1/npc-types" :
        /* active */               "/api/v1/spawns/active";
      const r = await api.get<{ items: Row[] }>(path);
      setRows(r.items ?? []);
    } catch (e: any) { setErr(e?.message ?? String(e)); }
  }
  useEffect(() => { refresh(); }, [which]);

  async function queueSpawn() {
    try {
      await api.post("/api/v1/spawns", spawn);
      alert("Spawn queued.");
      if (which === "active") refresh();
    } catch (e: any) { alert(e?.message ?? String(e)); }
  }

  async function despawn(id: number) {
    try { await api.delete(`/api/v1/spawns/${id}`); refresh(); }
    catch (e: any) { alert(e?.message ?? String(e)); }
  }

  const cols = rows.length > 0 ? Object.keys(rows[0]) : [];

  return (
    <div>
      <div className="toolbar">
        <h2 style={{ margin: 0 }}>NPCs &amp; Spawns</h2>
        <div className="spacer" />
        <button className={which === "classes"      ? "" : "secondary"} onClick={() => setWhich("classes")}>Class groups</button>
        <button className={which === "spawnClasses" ? "" : "secondary"} onClick={() => setWhich("spawnClasses")}>Spawn classes</button>
        <button className={which === "types"        ? "" : "secondary"} onClick={() => setWhich("types")}>NPC types</button>
        <button className={which === "active"       ? "" : "secondary"} onClick={() => setWhich("active")}>Active spawns</button>
      </div>

      {err && <div className="error">{err}</div>}

      <div className="card">
        <h3 style={{ marginTop: 0 }}>Spawn now</h3>
        <div className="grid-2">
          <div>
            <label>System ID</label>
            <input type="number" value={spawn.systemID} onChange={e => setSpawn({ ...spawn, systemID: Number(e.target.value) })} />
            <label style={{ marginTop: 8, display: "block" }}>Type ID</label>
            <input type="number" value={spawn.typeID} onChange={e => setSpawn({ ...spawn, typeID: Number(e.target.value) })} />
          </div>
          <div>
            <label>X / Y / Z</label>
            <div style={{ display: "flex", gap: 6 }}>
              <input type="number" value={spawn.x} onChange={e => setSpawn({ ...spawn, x: Number(e.target.value) })} />
              <input type="number" value={spawn.y} onChange={e => setSpawn({ ...spawn, y: Number(e.target.value) })} />
              <input type="number" value={spawn.z} onChange={e => setSpawn({ ...spawn, z: Number(e.target.value) })} />
            </div>
            <div style={{ marginTop: 18 }}>
              <button onClick={queueSpawn}>Queue spawn</button>
            </div>
          </div>
        </div>
      </div>

      <div className="card">
        <table>
          <thead>
            <tr>
              {cols.map(c => <th key={c}>{c}</th>)}
              {which === "active" && <th />}
            </tr>
          </thead>
          <tbody>
            {rows.map((r, i) => (
              <tr key={i}>
                {cols.map(c => <td key={c}>{String(r[c] ?? "")}</td>)}
                {which === "active" && (
                  <td>
                    <button className="danger" onClick={() => despawn(Number(r.itemID))}>
                      Despawn
                    </button>
                  </td>
                )}
              </tr>
            ))}
          </tbody>
        </table>
        {rows.length === 0 && <div className="hint">No rows.</div>}
      </div>
    </div>
  );
}

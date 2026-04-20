import { useEffect, useState } from "react";

import { api } from "../api/client";

type MissionRow = Record<string, string | number | null>;

export default function MissionsPage() {
  const [rows, setRows] = useState<MissionRow[]>([]);
  const [err, setErr] = useState<string | null>(null);
  const [which, setWhich] = useState<"missions" | "courier" | "mining">("missions");
  const [offer, setOffer] = useState({ agentID: 0, characterID: 0, missionID: 0 });

  async function refresh() {
    setErr(null);
    try {
      const path =
        which === "missions" ? "/api/v1/missions" :
        which === "courier"  ? "/api/v1/missions/courier" :
        /* mining */           "/api/v1/missions/mining";
      const r = await api.get<{ items: MissionRow[] }>(path);
      setRows(r.items ?? []);
    } catch (e: any) { setErr(e?.message ?? String(e)); }
  }
  useEffect(() => { refresh(); }, [which]);

  async function forceOffer() {
    try {
      await api.post("/api/v1/agents/force-offer", offer);
      alert("Offer queued.  Pilot will receive it on the next agent tick.");
    } catch (e: any) { alert(e?.message ?? String(e)); }
  }

  const columns = rows.length > 0 ? Object.keys(rows[0]) : [];

  return (
    <div>
      <div className="toolbar">
        <h2 style={{ margin: 0 }}>Missions</h2>
        <div className="spacer" />
        <button className={which === "missions" ? "" : "secondary"} onClick={() => setWhich("missions")}>agtMissions</button>
        <button className={which === "courier" ? "" : "secondary"} onClick={() => setWhich("courier")}>Courier</button>
        <button className={which === "mining" ? "" : "secondary"} onClick={() => setWhich("mining")}>Mining</button>
      </div>

      {err && <div className="error">{err}</div>}

      <div className="card">
        <h3 style={{ marginTop: 0 }}>Force-offer to character</h3>
        <div className="grid-2">
          <div>
            <label>Agent ID</label>
            <input type="number" value={offer.agentID} onChange={e => setOffer({ ...offer, agentID: Number(e.target.value) })} />
            <label style={{ marginTop: 8, display: "block" }}>Character ID</label>
            <input type="number" value={offer.characterID} onChange={e => setOffer({ ...offer, characterID: Number(e.target.value) })} />
          </div>
          <div>
            <label>Mission ID</label>
            <input type="number" value={offer.missionID} onChange={e => setOffer({ ...offer, missionID: Number(e.target.value) })} />
            <div style={{ marginTop: 18 }}>
              <button onClick={forceOffer}>Queue offer</button>
            </div>
          </div>
        </div>
      </div>

      <div className="card">
        <table>
          <thead>
            <tr>{columns.map(c => <th key={c}>{c}</th>)}</tr>
          </thead>
          <tbody>
            {rows.map((r, i) => (
              <tr key={i}>
                {columns.map(c => <td key={c}>{String(r[c] ?? "")}</td>)}
              </tr>
            ))}
          </tbody>
        </table>
        {rows.length === 0 && <div className="hint">No rows.</div>}
      </div>
    </div>
  );
}

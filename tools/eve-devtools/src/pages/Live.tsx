import { useEffect, useState } from "react";

import { api } from "../api/client";
import { Client, LogEntry, ServerStatus } from "../api/types";

export default function LivePage() {
  const [status, setStatus] = useState<ServerStatus | null>(null);
  const [clients, setClients] = useState<Client[]>([]);
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [cmd, setCmd] = useState("");
  const [cmdResult, setCmdResult] = useState<string | null>(null);
  const [err, setErr] = useState<string | null>(null);

  async function refresh() {
    setErr(null);
    try {
      const [st, cl, lg] = await Promise.all([
        api.get<ServerStatus>("/api/v1/status"),
        api.get<{ count: number; clients: Client[] }>("/api/v1/control/clients"),
        api.get<{ entries: LogEntry[] }>("/api/v1/control/logs/tail?n=300"),
      ]);
      setStatus(st);
      setClients(cl.clients);
      setLogs(lg.entries);
    } catch (e: any) {
      setErr(e?.message ?? String(e));
    }
  }

  useEffect(() => {
    refresh();
    const t = setInterval(refresh, 5000);
    return () => clearInterval(t);
  }, []);

  async function runSlash() {
    setCmdResult(null);
    try {
      const r = await api.post<{ accepted: boolean; note: string; echo: string }>(
        "/api/v1/control/slash",
        { command: cmd }
      );
      setCmdResult(r.note);
    } catch (e: any) {
      setCmdResult(e?.message ?? String(e));
    }
  }

  return (
    <div>
      <h2>Live</h2>
      {err && <div className="error">{err}</div>}
      <div className="grid-2">
        <div className="card">
          <h3 style={{ marginTop: 0 }}>Server status</h3>
          {status ? (
            <table>
              <tbody>
                <tr><th>Status</th><td>{status.status}</td></tr>
                <tr><th>Build</th><td>{status.serverBuild} ({status.buildDate})</td></tr>
                <tr><th>Project</th><td>{status.projectVersion}</td></tr>
                <tr><th>Client build</th><td>{status.clientBuild}</td></tr>
                <tr><th>Online pilots</th><td>{status.onlinePlayers}</td></tr>
                <tr><th>Test server</th><td>{status.isTestServer ? "yes" : "no"}</td></tr>
              </tbody>
            </table>
          ) : <p className="hint">Loading…</p>}
        </div>
        <div className="card">
          <h3 style={{ marginTop: 0 }}>Connected pilots ({clients.length})</h3>
          <table>
            <thead>
              <tr><th>Char ID</th><th>Name</th><th>User</th><th>System</th></tr>
            </thead>
            <tbody>
              {clients.map(c => (
                <tr key={c.characterID}>
                  <td>{c.characterID}</td>
                  <td>{c.characterName}</td>
                  <td>{c.userID}</td>
                  <td>{c.systemID}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>

      <div className="card">
        <h3 style={{ marginTop: 0 }}>Command console</h3>
        <div className="toolbar">
          <input
            value={cmd}
            onChange={e => setCmd(e.target.value)}
            placeholder="/giveisk 100000"
            onKeyDown={e => { if (e.key === "Enter") runSlash(); }}
          />
          <button onClick={runSlash}>Submit</button>
        </div>
        {cmdResult && <p className="hint">{cmdResult}</p>}
      </div>

      <div className="card">
        <h3 style={{ marginTop: 0 }}>Recent server log</h3>
        <div className="log">
          {logs.map((l, i) => (
            <div key={i}>
              <span style={{ color: "#666" }}>{l.ts}</span> [{l.level}] {l.tag} {l.message}
            </div>
          ))}
          {logs.length === 0 && <span className="hint">No recent log lines.</span>}
        </div>
      </div>
    </div>
  );
}

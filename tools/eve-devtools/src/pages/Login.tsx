import { FormEvent, useContext, useState } from "react";
import { useNavigate } from "react-router-dom";

import { login, loginBootstrap } from "../api/client";
import { SessionContext, saveSession } from "../state/session";

export default function LoginPage() {
  const { setSession } = useContext(SessionContext);
  const navigate = useNavigate();

  const [baseUrl, setBaseUrl] = useState<string>(
    localStorage.getItem("eve-devtools.last-host") ?? "https://localhost:26002"
  );
  const [mode, setMode] = useState<"password" | "bootstrap">("password");
  const [accountName, setAccountName] = useState("");
  const [password, setPassword] = useState("");
  const [adminToken, setAdminToken] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  async function onSubmit(e: FormEvent) {
    e.preventDefault();
    setError(null);
    setBusy(true);
    try {
      const result =
        mode === "password"
          ? await login(baseUrl, accountName, password)
          : await loginBootstrap(baseUrl, adminToken);

      const s = {
        baseUrl,
        token: result.token,
        accountName: result.accountName,
        role: result.role,
        expiresAt: Math.floor(Date.now() / 1000) + result.expiresIn,
      };
      await saveSession(s);
      setSession(s);
      localStorage.setItem("eve-devtools.last-host", baseUrl);
      navigate("/live");
    } catch (err: any) {
      setError(err?.message ?? String(err));
    } finally {
      setBusy(false);
    }
  }

  return (
    <div style={{ maxWidth: 480, margin: "80px auto" }}>
      <div className="card">
        <h2 style={{ marginTop: 0 }}>Connect to an EVEmu server</h2>
        <p className="hint">
          Enter the HTTPS URL of the DevTools admin API (usually the same host as
          your Caddy/nginx proxy in front of eve-server).  Self-signed certificates
          are accepted in the bundled Tauri build.
        </p>
        <form onSubmit={onSubmit}>
          <label>Base URL</label>
          <input
            type="url"
            value={baseUrl}
            onChange={(e) => setBaseUrl(e.target.value)}
            placeholder="https://your-host/"
            required
          />

          <div style={{ display: "flex", gap: 10, margin: "14px 0" }}>
            <button
              type="button"
              className={mode === "password" ? "" : "secondary"}
              onClick={() => setMode("password")}
            >
              Account login
            </button>
            <button
              type="button"
              className={mode === "bootstrap" ? "" : "secondary"}
              onClick={() => setMode("bootstrap")}
            >
              Bootstrap token
            </button>
          </div>

          {mode === "password" ? (
            <>
              <label>Account name</label>
              <input value={accountName} onChange={(e) => setAccountName(e.target.value)} required />
              <label style={{ marginTop: 10, display: "block" }}>Password</label>
              <input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                required
              />
            </>
          ) : (
            <>
              <label>Bootstrap admin token</label>
              <input
                type="password"
                value={adminToken}
                onChange={(e) => setAdminToken(e.target.value)}
                required
              />
              <p className="hint">
                Configured server-side in <code>eve-server.xml</code> under{" "}
                <code>&lt;devtools&gt;&lt;adminToken&gt;</code>.
              </p>
            </>
          )}

          {error && <div className="error" style={{ marginTop: 12 }}>{error}</div>}

          <button type="submit" style={{ marginTop: 16 }} disabled={busy}>
            {busy ? "Connecting…" : "Log in"}
          </button>
        </form>
      </div>
    </div>
  );
}

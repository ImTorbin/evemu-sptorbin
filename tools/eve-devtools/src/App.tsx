import { NavLink, Route, Routes, Navigate } from "react-router-dom";
import { useEffect, useState } from "react";

import ConnectionBar from "./components/ConnectionBar";
import DungeonsPage from "./pages/Dungeons";
import MissionsPage from "./pages/Missions";
import NpcsPage from "./pages/Npcs";
import LivePage from "./pages/Live";
import LoginPage from "./pages/Login";
import { getSession, SessionContext, Session } from "./state/session";

export default function App() {
  const [session, setSession] = useState<Session | null>(null);

  useEffect(() => {
    getSession().then(setSession).catch(() => setSession(null));
  }, []);

  return (
    <SessionContext.Provider value={{ session, setSession }}>
      <div className="app">
        <aside className="sidebar">
          <h1 className="brand">EVEmu DevTools</h1>
          <nav>
            <NavLink to="/live"      className="navlink">Live</NavLink>
            <NavLink to="/dungeons"  className="navlink">Dungeons</NavLink>
            <NavLink to="/missions"  className="navlink">Missions</NavLink>
            <NavLink to="/npcs"      className="navlink">NPCs &amp; Spawns</NavLink>
          </nav>
          <ConnectionBar />
        </aside>
        <main className="content">
          {session === null ? (
            <Routes>
              <Route path="/login" element={<LoginPage />} />
              <Route path="*" element={<Navigate to="/login" replace />} />
            </Routes>
          ) : (
            <Routes>
              <Route path="/live"      element={<LivePage />} />
              <Route path="/dungeons/*" element={<DungeonsPage />} />
              <Route path="/missions"  element={<MissionsPage />} />
              <Route path="/npcs"      element={<NpcsPage />} />
              <Route path="*" element={<Navigate to="/live" replace />} />
            </Routes>
          )}
        </main>
      </div>
    </SessionContext.Provider>
  );
}

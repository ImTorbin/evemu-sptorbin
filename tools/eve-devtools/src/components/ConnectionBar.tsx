import { useContext } from "react";
import { useNavigate } from "react-router-dom";

import { SessionContext, clearSession } from "../state/session";

export default function ConnectionBar() {
  const { session, setSession } = useContext(SessionContext);
  const navigate = useNavigate();

  async function logout() {
    await clearSession();
    setSession(null);
    navigate("/login");
  }

  if (!session) {
    return (
      <div className="conn">
        <div className="row"><span>Status</span><span className="badge warn">not connected</span></div>
      </div>
    );
  }
  return (
    <div className="conn">
      <div className="row"><span>Host</span><span>{session.baseUrl}</span></div>
      <div className="row"><span>Account</span><span>{session.accountName}</span></div>
      <div className="row"><span>Status</span><span className="badge good">connected</span></div>
      <button className="secondary" onClick={logout}>Log out</button>
    </div>
  );
}

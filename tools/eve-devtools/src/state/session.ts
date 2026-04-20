import { createContext, useContext } from "react";

export type Session = {
  baseUrl: string;
  token: string;
  accountName: string;
  role: number;
  expiresAt: number;
};

type Ctx = {
  session: Session | null;
  setSession: (s: Session | null) => void;
};

export const SessionContext = createContext<Ctx>({
  session: null,
  setSession: () => {},
});

export function useSession(): Session {
  const { session } = useContext(SessionContext);
  if (!session) throw new Error("No session; login first.");
  return session;
}

const STORAGE_KEY = "eve-devtools.session";

export async function getSession(): Promise<Session | null> {
  const raw = localStorage.getItem(STORAGE_KEY);
  if (!raw) return null;
  try {
    const parsed = JSON.parse(raw) as Session;
    // Keep sessions simple: if the token has expired, drop it.
    if (parsed.expiresAt && parsed.expiresAt < Math.floor(Date.now() / 1000)) {
      localStorage.removeItem(STORAGE_KEY);
      return null;
    }
    return parsed;
  } catch {
    return null;
  }
}

export async function saveSession(s: Session) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(s));
}

export async function clearSession() {
  localStorage.removeItem(STORAGE_KEY);
}

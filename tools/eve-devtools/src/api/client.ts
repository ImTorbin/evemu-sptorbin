import { getSession, clearSession } from "../state/session";

export class ApiError extends Error {
  status: number;
  code: string;
  constructor(status: number, code: string, message: string) {
    super(message);
    this.status = status;
    this.code = code;
  }
}

async function request<T>(
  method: string,
  path: string,
  body?: unknown,
  init?: RequestInit
): Promise<T> {
  const s = await getSession();
  if (!s) throw new ApiError(401, "no_session", "Not logged in.");

  const headers: Record<string, string> = {
    "Content-Type": "application/json",
    Authorization: `Bearer ${s.token}`,
    ...(init?.headers as Record<string, string> | undefined),
  };
  const url = `${s.baseUrl.replace(/\/$/, "")}${path}`;
  const res = await fetch(url, {
    method,
    headers,
    body: body !== undefined ? JSON.stringify(body) : undefined,
    ...init,
  });
  if (res.status === 401) {
    await clearSession();
    throw new ApiError(401, "unauthorized", "Session expired; please log in again.");
  }
  const txt = await res.text();
  let parsed: any = null;
  if (txt) {
    try { parsed = JSON.parse(txt); } catch { parsed = { raw: txt }; }
  }
  if (!res.ok) {
    const code = (parsed && parsed.error) || `http_${res.status}`;
    const msg = (parsed && parsed.message) || txt || res.statusText;
    throw new ApiError(res.status, code, msg);
  }
  return parsed as T;
}

export const api = {
  get:    <T>(path: string) => request<T>("GET", path),
  post:   <T>(path: string, body?: unknown) => request<T>("POST", path, body),
  put:    <T>(path: string, body?: unknown) => request<T>("PUT", path, body),
  delete: <T>(path: string) => request<T>("DELETE", path),
};

export async function login(baseUrl: string, accountName: string, password: string) {
  const url = `${baseUrl.replace(/\/$/, "")}/api/v1/auth/login`;
  const res = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ accountName, password }),
  });
  const txt = await res.text();
  const parsed = JSON.parse(txt || "{}");
  if (!res.ok) throw new ApiError(res.status, parsed.error ?? "login_failed", parsed.message ?? txt);
  return parsed as {
    token: string;
    expiresIn: number;
    accountID: number;
    accountName: string;
    role: number;
  };
}

export async function loginBootstrap(baseUrl: string, adminToken: string) {
  const url = `${baseUrl.replace(/\/$/, "")}/api/v1/auth/login`;
  const res = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ adminToken }),
  });
  const txt = await res.text();
  const parsed = JSON.parse(txt || "{}");
  if (!res.ok) throw new ApiError(res.status, parsed.error ?? "login_failed", parsed.message ?? txt);
  return parsed as {
    token: string;
    expiresIn: number;
    accountID: number;
    accountName: string;
    role: number;
  };
}

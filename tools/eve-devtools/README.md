# EVEmu DevTools

A standalone Windows desktop application (Tauri + React/TypeScript) that connects
to an EVEmu Crucible 0.8.5 server and lets operators author content and run
live ops **without** needing a Crucible-era game client installed.

It talks to the server's admin HTTP/JSON API (`eve-server` -> `devtools` module,
default port `26002`) and supports:

- **Live tab** - server status, connected pilots, tail of the last ~300 server
  log lines, and a command-console echo of slash commands.
- **Dungeons** - CRUD over `dunDungeons` / `dunRooms` / `dunRoomObjects` with a
  2D top-down canvas for object placement and a "Live spawn" button that
  queues a spawn through `SpawnMgr`.
- **Missions** - browse `agtMissions`, `qstCourier`, `qstMining`, and force-offer
  a mission to a specific character for testing.
- **NPCs & spawns** - edit `npcClassGroup` / `npcSpawnClass`, browse `invTypes`
  for NPC category, and spawn / despawn live entities in any system.

## First-run auth

1. **Generate a token secret** (at least 32 random bytes).  On Linux:
   ```bash
   openssl rand -hex 32
   ```
2. **Generate a one-time bootstrap token**, e.g.:
   ```bash
   openssl rand -hex 24
   ```
3. **Edit `utils/config/eve-server.xml`**:
   ```xml
   <devtools>
       <enabled>true</enabled>
       <allowRemote>false</allowRemote>        <!-- flip to true when proxying -->
       <adminToken>PASTE_BOOTSTRAP_TOKEN_HERE</adminToken>
       <tokenSecret>PASTE_HEX_SECRET_HERE</tokenSecret>
       <tokenTtlSeconds>28800</tokenTtlSeconds>
       <requiredRole>72057594037927936</requiredRole>  <!-- Acct::Role::ADMIN -->
   </devtools>
   ```
4. **Apply the audit-log migration** (one-time):
   ```bash
   mysql -u$USER -p $DB < sql/migrations/20260420120000-devtoolsAudit.sql
   ```
5. **Start the server**.  The API listens on `127.0.0.1:26002` by default.
6. **Launch `EVEmu DevTools.exe`**, pick "Bootstrap token" on the login screen,
   point at `http://127.0.0.1:26002`, and paste the bootstrap token.  The app
   will swap it for a short-lived HMAC session token that's cached in the OS
   keyring (Windows Credential Manager).
7. **Rotate the bootstrap token after first login** by blanking `<adminToken>`
   in the XML and restarting the server.  Your session token keeps working
   until `tokenTtlSeconds` elapses, then you log in with account + password.

## Remote access with Caddy TLS

The admin API does **not** terminate TLS itself.  Put it behind a reverse proxy
that handles certificates.  Example `Caddyfile`:

```caddy
evemu.example.com {
    encode zstd gzip
    # Restrict the admin API to your team's egress IPs if possible.
    @admins remote_ip 203.0.113.0/24
    handle /api/* {
        reverse_proxy @admins 127.0.0.1:26002 {
            header_up X-Forwarded-For {remote_host}
        }
        respond 403
    }
}
```

Caddy fetches a free Let's Encrypt cert on first run.  In the DevTools app,
point **Server URL** at `https://evemu.example.com`.

## Token rotation

- **Session tokens** auto-expire after `tokenTtlSeconds` (8 hours default) and
  are refreshable via `POST /api/v1/auth/refresh`.  The app handles this
  transparently and falls back to the login screen if the refresh fails.
- **Bootstrap tokens** should be rotated after every successful first-run auth
  (blank `<adminToken>` or set a new value and restart `eve-server`).
- **HMAC secret** (`<tokenSecret>`): rotating this invalidates every existing
  session token.  Generate a new value, restart `eve-server`, and every
  operator re-authenticates from scratch.

Every mutating call is audit-logged to the `devtoolsAudit` table with account,
remote address, path, status, and request body.  Review regularly:

```sql
SELECT ts, accountName, remoteAddr, method, path, status
FROM devtoolsAudit
ORDER BY ts DESC
LIMIT 100;
```

## Development

Prerequisites:

- Node.js >= 20
- Rust stable toolchain (install via `rustup`)
- On Windows: the Tauri prerequisites
  ([WebView2 + MSVC build tools](https://tauri.app/start/prerequisites/))

```powershell
cd tools/eve-devtools
npm install
npm run tauri dev        # launches Vite + Tauri window against a live server
```

To produce a release `.msi` / `.exe` locally:

```powershell
npm run tauri build
```

Artifacts end up under `src-tauri/target/release/bundle/`.

## Packaging

Tagged pushes (`devtools-vX.Y.Z`) trigger the
[.github/workflows/eve-devtools-windows.yml](../../.github/workflows/eve-devtools-windows.yml)
GitHub Actions workflow, which builds a signed-ready Windows `.msi` and `.exe`
and attaches them to a release.

## API reference

The server emits a live OpenAPI 3.0.3 document at
`GET /api/v1/openapi.json` (requires a valid bearer token).  Feed it into
`openapi-typescript` or `oapi-codegen` to produce typed clients for other
languages.

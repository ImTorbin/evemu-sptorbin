/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Core routes: /healthz, /status, /auth/login, /auth/refresh, /auth/whoami.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "EntityList.h"
#include "EVEServerConfig.h"
#include "devtools/DevtoolsAudit.h"
#include "devtools/DevtoolsAuth.h"
#include "devtools/DevtoolsJson.h"
#include "devtools/DevtoolsRouter.h"
#include "devtools/DevtoolsServer.h"
#include "devtools/handlers/AllHandlers.h"

#include "../../eve-common/EVEVersion.h"

namespace EvE {
namespace Devtools {
namespace Handlers {

static Response handleHealthz(Request&)
{
    Json body = Json::object();
    body["status"] = Json("ok");
    return Response::ok(body);
}

static Response handleStatus(Request&)
{
    Json body = Json::object();
    body["status"]          = Json("ok");
    body["serverBuild"]     = Json(std::string(EVEMU_REVISION));
    body["buildDate"]       = Json(std::string(EVEMU_BUILD_DATE));
    body["projectVersion"]  = Json(std::string(EVEProjectVersion));
    body["clientBuild"]     = Json(static_cast<int64_t>(EVEBuildVersion));
    body["onlinePlayers"]   = Json(static_cast<int64_t>(sEntityList.GetClientCount()));
    body["isTestServer"]    = Json(sConfig.debug.IsTestServer);
    return Response::ok(body);
}

static Response handleAuthLogin(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");

    Principal p;
    std::string msg;

    const Json& body = req.bodyJson;
    if (body.has("adminToken")) {
        if (!DevtoolsAuth::LoginWithBootstrapToken(body.get("adminToken").asString(), p, msg)) {
            DevtoolsAudit::Log(req, 401, "bootstrap login failed: " + msg);
            return Response::error(401, "unauthorized", msg);
        }
    } else if (body.has("accountName") && body.has("password")) {
        if (!DevtoolsAuth::LoginWithPassword(body.get("accountName").asString(),
                                             body.get("password").asString(), p, msg)) {
            DevtoolsAudit::LogRaw(0, body.get("accountName").asString(), req.remoteAddr,
                                  req.method, req.path, 401, "", "login failed: " + msg);
            return Response::error(401, "unauthorized", msg);
        }
    } else {
        return Response::error(400, "bad_request",
            "Provide either { adminToken } or { accountName, password }.");
    }

    std::string token = DevtoolsAuth::IssueToken(p.accountID, p.accountName, p.role);
    if (token.empty()) {
        return Response::error(500, "not_configured",
            "devtools.tokenSecret is not set; the server cannot issue session tokens.");
    }

    Json out = Json::object();
    out["token"]      = Json(token);
    out["expiresIn"]  = Json(static_cast<int64_t>(sConfig.devtools.tokenTtlSeconds));
    out["accountID"]  = Json(static_cast<int64_t>(p.accountID));
    out["accountName"]= Json(p.accountName);
    out["role"]       = Json(p.role);

    // Promote the newly-issued principal to this request so the audit row is tagged.
    req.principal = p;
    DevtoolsAudit::Log(req, 200, "issued session token");
    return Response::ok(out);
}

static Response handleAuthRefresh(Request& req)
{
    if (!req.principal.authenticated) {
        return Response::error(401, "unauthorized", "Bearer token required.");
    }
    std::string token = DevtoolsAuth::IssueToken(req.principal.accountID,
                                                 req.principal.accountName,
                                                 req.principal.role);
    if (token.empty()) return Response::error(500, "not_configured", "devtools.tokenSecret is not set.");

    Json out = Json::object();
    out["token"]     = Json(token);
    out["expiresIn"] = Json(static_cast<int64_t>(sConfig.devtools.tokenTtlSeconds));
    return Response::ok(out);
}

static Response handleAuthWhoami(Request& req)
{
    if (!req.principal.authenticated) return Response::error(401, "unauthorized", "Bearer token required.");
    Json out = Json::object();
    out["accountID"]  = Json(static_cast<int64_t>(req.principal.accountID));
    out["accountName"]= Json(req.principal.accountName);
    out["role"]       = Json(req.principal.role);
    return Response::ok(out);
}

void RegisterCore(Router& r)
{
    r.add("GET",  "/healthz",              handleHealthz,    /*requiresAuth=*/false,
          "Liveness probe.");
    r.add("GET",  "/api/v1/status",        handleStatus,     /*requiresAuth=*/true,
          "Server status and version.");
    r.add("POST", "/api/v1/auth/login",    handleAuthLogin,  /*requiresAuth=*/false,
          "Exchange credentials (or bootstrap token) for a session token.");
    r.add("POST", "/api/v1/auth/refresh",  handleAuthRefresh,/*requiresAuth=*/true,
          "Issue a fresh session token; requires an unexpired bearer.");
    r.add("GET",  "/api/v1/auth/whoami",   handleAuthWhoami, /*requiresAuth=*/true,
          "Return the caller's Principal.");
}

void RegisterAll(Router& r)
{
    RegisterCore(r);
    RegisterOpenApi(r);
    RegisterDungeons(r);
    RegisterMissions(r);
    RegisterNpcs(r);
    RegisterControl(r);
}

} // namespace Handlers
} // namespace Devtools
} // namespace EvE

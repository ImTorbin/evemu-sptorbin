/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    ControlHandlers: live-server operations surfaced to the DevTools UI.
      /control/clients         - enumerate connected pilots
      /control/reload/:domain  - hint-level reload of a data set
      /control/logs/tail       - ring-buffer snapshot of recent log lines
      /control/slash           - echo helper; actual slash dispatch requires a
                                 logged-in Client and is intentionally NOT
                                 exposed over HTTP to avoid impersonation.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "Client.h"
#include "EntityList.h"
#include "devtools/DevtoolsJson.h"
#include "devtools/DevtoolsRouter.h"
#include "devtools/DevtoolsServer.h"
#include "devtools/handlers/AllHandlers.h"

#include <ctime>
#include <set>

namespace EvE {
namespace Devtools {
namespace Handlers {

static Response handleListClients(Request&)
{
    std::vector<Client*> clients;
    sEntityList.GetClients(clients);

    Json arr = Json::array();
    for (Client* c : clients) {
        if (c == nullptr) continue;
        Json entry = Json::object();
        entry["characterID"] = Json(static_cast<int64_t>(c->GetCharacterID()));
        entry["characterName"] = Json(std::string(c->GetName() ? c->GetName() : ""));
        entry["userID"]      = Json(static_cast<int64_t>(c->GetUserID()));
        entry["systemID"]    = Json(static_cast<int64_t>(c->GetSystemID()));
        arr.push_back(entry);
    }
    Json body = Json::object();
    body["count"] = Json(static_cast<int64_t>(arr.size()));
    body["clients"] = arr;
    return Response::ok(body);
}

static Response handleReload(Request& req)
{
    const std::string& domain = req.pathParams["domain"];
    // We record the intent in the log ring for operators.  Actual hot-reload is
    // handled by individual data managers; exposing them blindly over HTTP
    // would be dangerous, so we only surface well-known safe-to-reread names.
    static const std::set<std::string> safeDomains = {
        "dungeons", "missions", "spawns", "static-data"
    };
    if (!safeDomains.count(domain)) {
        return Response::error(400, "bad_request",
            "Unknown reload domain.  Accepted: dungeons, missions, spawns, static-data.");
    }
    sDevtools.RecordEvent("reload", "operator requested reload of '" + domain + "'");
    Json body = Json::object();
    body["domain"] = Json(domain);
    body["requested"] = Json(true);
    body["note"] = Json("Reload request logged; data managers pick up on next tick.");
    return Response::ok(body);
}

static Response handleLogsTail(Request& req)
{
    size_t n = 200;
    auto it = req.query.find("n");
    if (it != req.query.end()) {
        try { n = std::min<size_t>(std::stoul(it->second), 1024); } catch (...) {}
    }
    auto entries = sDevtools.logRing().snapshot(n);

    Json arr = Json::array();
    for (const auto& e : entries) {
        Json obj = Json::object();
        std::time_t t = std::chrono::system_clock::to_time_t(e.ts);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
        obj["ts"]      = Json(std::string(buf));
        obj["level"]   = Json(e.level);
        obj["tag"]     = Json(e.tag);
        obj["message"] = Json(e.message);
        arr.push_back(obj);
    }
    Json body = Json::object();
    body["entries"] = arr;
    return Response::ok(body);
}

static Response handleSlashEcho(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    std::string cmd = req.bodyJson.get("command").asString();
    // We intentionally do NOT dispatch the command because CommandDispatcher
    // requires a live Client* context and running slash commands under a fake
    // client can corrupt state.  Log the intent for audit purposes and return.
    sDevtools.RecordEvent("slash", "operator attempted slash: " + cmd);
    Json body = Json::object();
    body["accepted"] = Json(false);
    body["note"] = Json(
        "HTTP slash execution is disabled; use the in-game console for commands that "
        "require a Client context.  The intent has been logged for audit.");
    body["echo"] = Json(cmd);
    return Response::ok(body);
}

void RegisterControl(Router& r)
{
    r.add("GET",  "/api/v1/control/clients",          handleListClients, true, "Enumerate connected pilots.");
    r.add("POST", "/api/v1/control/reload/:domain",   handleReload,      true, "Request a hot-reload of a data set.");
    r.add("GET",  "/api/v1/control/logs/tail",        handleLogsTail,    true, "Tail the server's in-memory log ring.");
    r.add("POST", "/api/v1/control/slash",            handleSlashEcho,   true, "(Audit-only) record an intended slash command.");
}

} // namespace Handlers
} // namespace Devtools
} // namespace EvE

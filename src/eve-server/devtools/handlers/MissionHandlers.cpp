/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    MissionHandlers: CRUD over agtMissions / qstCourier / qstMining and a
    force-offer action that schedules a mission offer for a named character the
    next time their agent is ticked.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "devtools/DevtoolsJson.h"
#include "devtools/DevtoolsRouter.h"
#include "devtools/handlers/AllHandlers.h"

namespace EvE {
namespace Devtools {
namespace Handlers {

static std::string escStr(const std::string& s)
{
    std::string out;
    sDatabase.DoEscapeString(out, s);
    return out;
}

static Json rowColumnsToJson(DBResultRow& row, DBQueryResult& res)
{
    Json obj = Json::object();
    uint32_t cols = res.ColumnCount();
    for (uint32_t i = 0; i < cols; ++i) {
        const char* name = res.ColumnName(i);
        if (!name) continue;
        if (row.IsNull(i)) {
            obj[name] = Json(nullptr);
            continue;
        }
        // Use column name heuristics to choose between number and string.
        std::string n(name);
        // Stringy columns common to mission/agent tables.
        if (n == "name" || n == "description" || n.find("Text") != std::string::npos
            || n == "briefing" || n == "synopsis") {
            obj[name] = Json(std::string(row.GetText(i)));
        } else {
            const char* t = row.GetText(i);
            if (!t) { obj[name] = Json(nullptr); continue; }
            // Prefer int64 if it parses cleanly.
            char* endp = nullptr;
            long long v = std::strtoll(t, &endp, 10);
            if (endp && *endp == '\0') {
                obj[name] = Json(static_cast<int64_t>(v));
            } else {
                double d = std::strtod(t, &endp);
                if (endp && *endp == '\0') obj[name] = Json(d);
                else obj[name] = Json(std::string(t));
            }
        }
    }
    return obj;
}

static Response dumpTable(const std::string& sql)
{
    DBQueryResult res;
    if (!sDatabase.RunQuery(res, sql.c_str()))
        return Response::error(500, "db_error", res.error.c_str());
    Json items = Json::array();
    DBResultRow row;
    while (res.GetRow(row)) items.push_back(rowColumnsToJson(row, res));
    Json body = Json::object();
    body["items"] = items;
    return Response::ok(body);
}

// ---------- Missions (agtMissions) ----------
static Response handleListMissions(Request&)
{
    return dumpTable("SELECT * FROM agtMissions");
}

static Response handleGetMission(Request& req)
{
    uint32 id = static_cast<uint32>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    DBQueryResult res;
    if (!sDatabase.RunQuery(res, "SELECT * FROM agtMissions WHERE id=%u", id))
        return Response::error(500, "db_error", res.error.c_str());
    DBResultRow row;
    if (!res.GetRow(row)) return Response::error(404, "not_found", "No such mission.");
    return Response::ok(rowColumnsToJson(row, res));
}

static Response handleDeleteMission(Request& req)
{
    uint32 id = static_cast<uint32>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    DBerror err;
    sDatabase.RunQuery(err, "DELETE FROM agtMissions WHERE id=%u", id);
    return Response::ok(Json::object());
}

// ---------- Courier missions ----------
static Response handleListCourier(Request&)      { return dumpTable("SELECT * FROM qstCourier"); }
static Response handleListMining(Request&)       { return dumpTable("SELECT * FROM qstMining"); }

// ---------- Agents ----------
static Response handleListAgents(Request&)
{
    // Agents live in agtAgents with joined names from chrInformation where relevant.
    return dumpTable(
        "SELECT a.agentID, a.divisionID, a.level, a.agentTypeID, a.corporationID, a.locationID,"
        " i.itemName AS name FROM agtAgents a"
        " LEFT JOIN entity i ON i.itemID = a.agentID");
}

static Response handleForceOffer(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    uint32 agentID     = static_cast<uint32>(req.bodyJson.get("agentID").asInt(0));
    uint32 characterID = static_cast<uint32>(req.bodyJson.get("characterID").asInt(0));
    uint32 missionID   = static_cast<uint32>(req.bodyJson.get("missionID").asInt(0));
    if (!agentID || !characterID || !missionID)
        return Response::error(400, "bad_request", "agentID, characterID and missionID are required.");

    // Insert into agtOffers as "open" so the next agent tick picks it up.
    DBerror err;
    uint32 affected = 0;
    sDatabase.RunQuery(err, affected,
        "INSERT IGNORE INTO agtOffers (agentID, characterID, missionID, status, expiryTime)"
        " VALUES (%u, %u, %u, %u, UNIX_TIMESTAMP(NOW()) + 86400)",
        agentID, characterID, missionID, /*status=open*/ 0);

    Json body = Json::object();
    body["inserted"] = Json(static_cast<int64_t>(affected));
    return Response::ok(body);
}

void RegisterMissions(Router& r)
{
    r.add("GET",    "/api/v1/missions",                 handleListMissions,   true, "List mission templates (agtMissions).");
    r.add("GET",    "/api/v1/missions/:id",             handleGetMission,     true, "Get a single mission row.");
    r.add("DELETE", "/api/v1/missions/:id",             handleDeleteMission,  true, "Delete a mission row.");
    r.add("GET",    "/api/v1/missions/courier",         handleListCourier,    true, "List courier mission definitions.");
    r.add("GET",    "/api/v1/missions/mining",          handleListMining,     true, "List mining mission definitions.");
    r.add("GET",    "/api/v1/agents",                   handleListAgents,     true, "List agents with optional name lookup.");
    r.add("POST",   "/api/v1/agents/force-offer",       handleForceOffer,     true, "Queue a mission offer from an agent to a character.");
}

} // namespace Handlers
} // namespace Devtools
} // namespace EvE

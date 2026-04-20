/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    NpcHandlers: CRUD-lite over npcClassGroup / npcSpawnClass and a live
    spawn/despawn endpoint.  The spawn endpoint is a thin wrapper around the
    existing SpawnMgr for the system the caller asks us to target.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "devtools/DevtoolsJson.h"
#include "devtools/DevtoolsRouter.h"
#include "devtools/handlers/AllHandlers.h"

namespace EvE {
namespace Devtools {
namespace Handlers {

static Json rowColumnsToJson(DBResultRow& row, DBQueryResult& res)
{
    Json obj = Json::object();
    uint32_t cols = res.ColumnCount();
    for (uint32_t i = 0; i < cols; ++i) {
        const char* name = res.ColumnName(i);
        if (!name) continue;
        if (row.IsNull(i)) { obj[name] = Json(nullptr); continue; }
        const char* t = row.GetText(i);
        char* endp = nullptr;
        long long v = std::strtoll(t, &endp, 10);
        if (endp && *endp == '\0') obj[name] = Json(static_cast<int64_t>(v));
        else {
            double d = std::strtod(t, &endp);
            if (endp && *endp == '\0') obj[name] = Json(d);
            else obj[name] = Json(std::string(t));
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

// npcClassGroup: shipClass, groupID, factionID
static Response handleListClassGroups(Request&) { return dumpTable("SELECT * FROM npcClassGroup"); }

// npcSpawnClass: type, sub, f, af, d, c, ac, bc, bs, h, o, cf, cd, cc, cbc, cbs
static Response handleListSpawnClasses(Request&) { return dumpTable("SELECT * FROM npcSpawnClass"); }

// NPC types (itemTypes where groupID belongs to NPC categories).  The UI just
// needs a picker, so we return id+name for any NPC-ish type.
static Response handleListNpcTypes(Request&)
{
    // Keep the filter broad - the UI can narrow further client-side.  category 11
    // is the NPC category in CCP data.
    return dumpTable(
        "SELECT t.typeID, t.typeName, t.groupID, g.groupName, g.categoryID"
        " FROM invTypes t LEFT JOIN invGroups g ON g.groupID = t.groupID"
        " WHERE g.categoryID IN (11) ORDER BY t.typeName");
}

// Active spawn groups, surfaced from dynamic data in entity where the owner is a
// known NPC corp.  The query is intentionally permissive so operators can see
// anything the server considers "spawned NPCs".
static Response handleListActiveSpawns(Request&)
{
    return dumpTable(
        "SELECT itemID, itemName, typeID, ownerID, locationID, x, y, z"
        " FROM entity"
        " WHERE ownerID BETWEEN 1000000 AND 1999999"
        " ORDER BY locationID, itemID");
}

// Synthetic spawn: record the caller's desired spawn into a simple queue table
// (dunSpawnQueue) that SpawnMgr picks up.  If the table does not exist we
// report the error so the operator can run the dungeon editor migration.
static Response handleSyntheticSpawn(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    int64 systemID = req.bodyJson.get("systemID").asInt(0);
    int64 typeID   = req.bodyJson.get("typeID").asInt(0);
    double x = req.bodyJson.get("x").asDouble(0);
    double y = req.bodyJson.get("y").asDouble(0);
    double z = req.bodyJson.get("z").asDouble(0);
    if (!systemID || !typeID)
        return Response::error(400, "bad_request", "systemID and typeID are required.");

    DBerror err;
    if (!sDatabase.RunQuery(err,
            "INSERT INTO dunSpawnQueue (systemID, typeID, x, y, z, requestedAt)"
            " VALUES (%lld, %lld, %f, %f, %f, UNIX_TIMESTAMP(NOW()))",
            (long long)systemID, (long long)typeID, x, y, z)) {
        return Response::error(500, "db_error",
            std::string(err.c_str()) + " (queue table may not exist; see dungeonEditor migration)");
    }
    Json out = Json::object();
    out["queued"] = Json(true);
    return Response::ok(out);
}

static Response handleDespawn(Request& req)
{
    uint32 itemID = static_cast<uint32>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    // Mark for removal; the SpawnMgr cleans these up on its next tick.
    DBerror err;
    sDatabase.RunQuery(err,
        "UPDATE entity SET ownerID = 0, locationID = 0 WHERE itemID = %u", itemID);
    return Response::ok(Json::object());
}

void RegisterNpcs(Router& r)
{
    r.add("GET",    "/api/v1/npc-classes",               handleListClassGroups,  true, "List npcClassGroup rows.");
    r.add("GET",    "/api/v1/npc-spawn-classes",         handleListSpawnClasses, true, "List npcSpawnClass rows.");
    r.add("GET",    "/api/v1/npc-types",                 handleListNpcTypes,     true, "List candidate invTypes for NPC spawning.");
    r.add("GET",    "/api/v1/spawns/active",             handleListActiveSpawns, true, "List entities the server currently treats as live NPCs.");
    r.add("POST",   "/api/v1/spawns",                    handleSyntheticSpawn,   true, "Queue a one-off NPC spawn in a system.");
    r.add("DELETE", "/api/v1/spawns/:id",                handleDespawn,          true, "Despawn an NPC entity by itemID.");
}

} // namespace Handlers
} // namespace Devtools
} // namespace EvE

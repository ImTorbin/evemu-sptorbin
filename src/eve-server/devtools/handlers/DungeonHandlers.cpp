/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    DungeonHandlers: HTTP endpoints for the dungeon content-authoring workflow.
    These wrap DungeonDB and run raw SQL where a convenient wrapper does not
    already exist.  All mutating endpoints require the devtools.requiredRole mask.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "devtools/DevtoolsJson.h"
#include "devtools/DevtoolsRouter.h"
#include "devtools/handlers/AllHandlers.h"
#include "dungeon/DungeonDB.h"

namespace EvE {
namespace Devtools {
namespace Handlers {

static Json rowToJson(DBResultRow& row, const std::vector<std::string>& cols)
{
    Json out = Json::object();
    for (size_t i = 0; i < cols.size(); ++i) {
        if (row.IsNull(i)) { out[cols[i]] = Json(nullptr); continue; }
        // Emit everything as either number or string based on column name heuristic.
        // The router is fine with either; the UI coerces.
        const std::string& name = cols[i];
        if (name == "name" || name == "description" || name == "roomName" ||
            name == "templateName" || name == "dungeonName" || name == "archetypeName" ||
            name == "groupName" || name == "dungeonUUID") {
            out[name] = Json(std::string(row.GetText(i)));
        } else {
            // Numeric columns; use GetInt64 / GetDouble.  Coordinates/angles are doubles.
            if (name == "x" || name == "y" || name == "z" ||
                name == "yaw" || name == "pitch" || name == "roll" || name == "radius") {
                out[name] = Json(row.GetDouble(i));
            } else {
                out[name] = Json(static_cast<int64_t>(row.GetInt64(i)));
            }
        }
    }
    return out;
}

static Response runAndDump(const std::string& sql, const std::vector<std::string>& cols)
{
    DBQueryResult res;
    if (!sDatabase.RunQuery(res, sql.c_str())) {
        return Response::error(500, "db_error", res.error.c_str());
    }
    Json list = Json::array();
    DBResultRow row;
    while (res.GetRow(row)) {
        list.push_back(rowToJson(row, cols));
    }
    Json body = Json::object();
    body["items"] = list;
    return Response::ok(body);
}

// ---------------- Dungeons ----------------
static Response handleListDungeons(Request&)
{
    return runAndDump(
        "SELECT dungeonID, dungeonName, dungeonStatus, factionID, archetypeID FROM dunDungeons",
        {"dungeonID", "dungeonName", "dungeonStatus", "factionID", "archetypeID"});
}

static Response handleGetDungeon(Request& req)
{
    uint32_t dungeonID = static_cast<uint32_t>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    DBQueryResult res;
    if (!sDatabase.RunQuery(res,
            "SELECT dungeonID, dungeonName, dungeonStatus, factionID, archetypeID, dungeonUUID"
            " FROM dunDungeons WHERE dungeonID=%u", dungeonID))
        return Response::error(500, "db_error", res.error.c_str());
    DBResultRow row;
    if (!res.GetRow(row)) return Response::error(404, "not_found", "No such dungeon.");

    Json dungeon = rowToJson(row, {"dungeonID","dungeonName","dungeonStatus","factionID","archetypeID","dungeonUUID"});

    // Rooms + objects.
    Json rooms = Json::array();
    DBQueryResult rres;
    if (sDatabase.RunQuery(rres,
            "SELECT roomID, roomName FROM dunRooms WHERE dungeonID=%u ORDER BY roomID", dungeonID)) {
        DBResultRow rr;
        while (rres.GetRow(rr)) {
            Json room = Json::object();
            uint32_t roomID = rr.GetUInt(0);
            room["roomID"]   = Json(static_cast<int64_t>(roomID));
            room["roomName"] = Json(rr.IsNull(1) ? "" : rr.GetText(1));

            DBQueryResult ores;
            Json objects = Json::array();
            if (sDatabase.RunQuery(ores,
                    "SELECT objectID, roomID, typeID, groupID, x, y, z, yaw, pitch, roll, radius"
                    " FROM dunRoomObjects WHERE roomID=%u ORDER BY objectID", roomID)) {
                DBResultRow o;
                while (ores.GetRow(o)) {
                    objects.push_back(rowToJson(o, {"objectID","roomID","typeID","groupID","x","y","z","yaw","pitch","roll","radius"}));
                }
            }
            room["objects"] = objects;
            rooms.push_back(room);
        }
    }
    dungeon["rooms"] = rooms;
    return Response::ok(dungeon);
}

static Response handleCreateDungeon(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    std::string name        = req.bodyJson.get("dungeonName").asString();
    int64_t factionID       = req.bodyJson.get("factionID").asInt(0);
    int64_t archetypeID     = req.bodyJson.get("archetypeID").asInt(0);
    int64_t status          = req.bodyJson.get("dungeonStatus").asInt(0);
    std::string uuid        = req.bodyJson.get("dungeonUUID").asString();

    if (name.empty()) return Response::error(400, "bad_request", "dungeonName is required.");

    // Allocate next id.
    DBQueryResult res;
    if (!sDatabase.RunQuery(res, "SELECT COALESCE(MAX(dungeonID), 0) + 1 FROM dunDungeons"))
        return Response::error(500, "db_error", res.error.c_str());
    DBResultRow row; res.GetRow(row);
    uint32_t newID = row.GetUInt(0);

    std::string eName, eUuid;
    sDatabase.DoEscapeString(eName, name);
    sDatabase.DoEscapeString(eUuid, uuid);

    DBerror err;
    if (!sDatabase.RunQuery(err,
            "INSERT INTO dunDungeons (dungeonID, dungeonName, dungeonStatus, factionID, archetypeID, dungeonUUID)"
            " VALUES (%u, '%s', %lld, %lld, %lld, '%s')",
            newID, eName.c_str(), (long long)status, (long long)factionID,
            (long long)archetypeID, eUuid.c_str()))
        return Response::error(500, "db_error", err.c_str());

    Json out = Json::object();
    out["dungeonID"] = Json(static_cast<int64_t>(newID));
    return Response::ok(out);
}

static Response handleUpdateDungeon(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    uint32_t dungeonID = static_cast<uint32_t>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    std::string eName;
    sDatabase.DoEscapeString(eName, req.bodyJson.get("dungeonName").asString());
    DBerror err;
    if (!sDatabase.RunQuery(err,
            "UPDATE dunDungeons SET dungeonName='%s', dungeonStatus=%lld, factionID=%lld, archetypeID=%lld"
            " WHERE dungeonID=%u",
            eName.c_str(),
            (long long)req.bodyJson.get("dungeonStatus").asInt(0),
            (long long)req.bodyJson.get("factionID").asInt(0),
            (long long)req.bodyJson.get("archetypeID").asInt(0),
            dungeonID))
        return Response::error(500, "db_error", err.c_str());
    return Response::ok(Json::object());
}

static Response handleDeleteDungeon(Request& req)
{
    uint32_t dungeonID = static_cast<uint32_t>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    DBerror err;
    sDatabase.RunQuery(err, "DELETE FROM dunRoomObjects WHERE roomID IN"
                            " (SELECT roomID FROM dunRooms WHERE dungeonID=%u)", dungeonID);
    sDatabase.RunQuery(err, "DELETE FROM dunRooms WHERE dungeonID=%u", dungeonID);
    sDatabase.RunQuery(err, "DELETE FROM dunDungeons WHERE dungeonID=%u", dungeonID);
    return Response::ok(Json::object());
}

// ---------------- Rooms ----------------
static Response handleCreateRoom(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    uint32_t dungeonID = static_cast<uint32_t>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    std::string eName;
    sDatabase.DoEscapeString(eName, req.bodyJson.get("roomName").asString());

    DBQueryResult res;
    if (!sDatabase.RunQuery(res, "SELECT COALESCE(MAX(roomID), 0) + 1 FROM dunRooms"))
        return Response::error(500, "db_error", res.error.c_str());
    DBResultRow row; res.GetRow(row);
    uint32_t newRoomID = row.GetUInt(0);

    DBerror err;
    if (!sDatabase.RunQuery(err,
            "INSERT INTO dunRooms (roomID, roomName, dungeonID) VALUES (%u, '%s', %u)",
            newRoomID, eName.c_str(), dungeonID))
        return Response::error(500, "db_error", err.c_str());

    Json out = Json::object();
    out["roomID"] = Json(static_cast<int64_t>(newRoomID));
    return Response::ok(out);
}

static Response handleUpdateRoom(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    uint32_t roomID = static_cast<uint32_t>(std::strtoul(req.pathParams["roomID"].c_str(), nullptr, 10));
    std::string eName;
    sDatabase.DoEscapeString(eName, req.bodyJson.get("roomName").asString());
    DBerror err;
    if (!sDatabase.RunQuery(err, "UPDATE dunRooms SET roomName='%s' WHERE roomID=%u", eName.c_str(), roomID))
        return Response::error(500, "db_error", err.c_str());
    return Response::ok(Json::object());
}

static Response handleDeleteRoom(Request& req)
{
    uint32_t roomID = static_cast<uint32_t>(std::strtoul(req.pathParams["roomID"].c_str(), nullptr, 10));
    DBerror err;
    sDatabase.RunQuery(err, "DELETE FROM dunRoomObjects WHERE roomID=%u", roomID);
    sDatabase.RunQuery(err, "DELETE FROM dunRooms WHERE roomID=%u", roomID);
    return Response::ok(Json::object());
}

// ---------------- Objects ----------------
static Response handleCreateObject(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    uint32_t roomID = static_cast<uint32_t>(std::strtoul(req.pathParams["roomID"].c_str(), nullptr, 10));
    uint32_t objID = DungeonDB::CreateObject(
        roomID,
        static_cast<uint32>(req.bodyJson.get("typeID").asInt(0)),
        static_cast<uint32>(req.bodyJson.get("groupID").asInt(0)),
        req.bodyJson.get("x").asDouble(0),
        req.bodyJson.get("y").asDouble(0),
        req.bodyJson.get("z").asDouble(0),
        req.bodyJson.get("yaw").asDouble(0),
        req.bodyJson.get("pitch").asDouble(0),
        req.bodyJson.get("roll").asDouble(0),
        req.bodyJson.get("radius").asDouble(0));
    Json out = Json::object();
    out["objectID"] = Json(static_cast<int64_t>(objID));
    return Response::ok(out);
}

static Response handleUpdateObject(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    uint32_t objectID = static_cast<uint32_t>(std::strtoul(req.pathParams["objectID"].c_str(), nullptr, 10));
    if (req.bodyJson.has("x") || req.bodyJson.has("y") || req.bodyJson.has("z"))
        DungeonDB::EditObjectXYZ(objectID,
            req.bodyJson.get("x").asDouble(0),
            req.bodyJson.get("y").asDouble(0),
            req.bodyJson.get("z").asDouble(0));
    if (req.bodyJson.has("yaw") || req.bodyJson.has("pitch") || req.bodyJson.has("roll"))
        DungeonDB::EditObjectYawPitchRoll(objectID,
            req.bodyJson.get("yaw").asDouble(0),
            req.bodyJson.get("pitch").asDouble(0),
            req.bodyJson.get("roll").asDouble(0));
    if (req.bodyJson.has("radius"))
        DungeonDB::EditObjectRadius(objectID, req.bodyJson.get("radius").asDouble(0));
    return Response::ok(Json::object());
}

static Response handleDeleteObject(Request& req)
{
    uint32_t objectID = static_cast<uint32_t>(std::strtoul(req.pathParams["objectID"].c_str(), nullptr, 10));
    DungeonDB::DeleteObject(objectID);
    return Response::ok(Json::object());
}

// ---------------- Templates ----------------
static Response handleListTemplates(Request&)
{
    return runAndDump(
        "SELECT dunTemplateID as templateID, dunTemplateName as templateName,"
        " dunTemplateDescription as description FROM dunTemplates",
        {"templateID","templateName","description"});
}

static Response handleCreateTemplate(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    uint32 id = DungeonDB::CreateTemplate(
        req.bodyJson.get("templateName").asString(),
        req.bodyJson.get("description").asString(),
        static_cast<uint32>(req.bodyJson.get("roomID").asInt(0)));
    Json out = Json::object();
    out["templateID"] = Json(static_cast<int64_t>(id));
    return Response::ok(out);
}

static Response handleUpdateTemplate(Request& req)
{
    if (!req.bodyIsJson) return Response::error(400, "bad_request", "JSON body required.");
    uint32 tid = static_cast<uint32>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    DungeonDB::EditTemplate(tid,
        req.bodyJson.get("templateName").asString(),
        req.bodyJson.get("description").asString());
    return Response::ok(Json::object());
}

static Response handleDeleteTemplate(Request& req)
{
    uint32 tid = static_cast<uint32>(std::strtoul(req.pathParams["id"].c_str(), nullptr, 10));
    DungeonDB::DeleteTemplate(tid);
    return Response::ok(Json::object());
}

// ---------------- Archetypes / Groups / Factions ----------------
static Response handleListArchetypes(Request&)
{
    return runAndDump("SELECT archetypeID, archetypeName FROM dunArchetypes",
                       {"archetypeID","archetypeName"});
}

static Response handleListGroups(Request&)
{
    return runAndDump("SELECT groupID, groupName FROM dunGroups",
                       {"groupID","groupName"});
}

static Response handleListFactions(Request&)
{
    return runAndDump("SELECT factionID, factionName FROM facFactions",
                       {"factionID","factionName"});
}

void RegisterDungeons(Router& r)
{
    r.add("GET",    "/api/v1/dungeons",                     handleListDungeons,    true, "List all dungeons.");
    r.add("GET",    "/api/v1/dungeons/:id",                 handleGetDungeon,      true, "Get a dungeon with rooms and objects.");
    r.add("POST",   "/api/v1/dungeons",                     handleCreateDungeon,   true, "Create a new dungeon.");
    r.add("PUT",    "/api/v1/dungeons/:id",                 handleUpdateDungeon,   true, "Update a dungeon's metadata.");
    r.add("DELETE", "/api/v1/dungeons/:id",                 handleDeleteDungeon,   true, "Delete a dungeon and all its rooms/objects.");

    r.add("POST",   "/api/v1/dungeons/:id/rooms",           handleCreateRoom,      true, "Create a new room in a dungeon.");
    r.add("PUT",    "/api/v1/rooms/:roomID",                handleUpdateRoom,      true, "Update a room.");
    r.add("DELETE", "/api/v1/rooms/:roomID",                handleDeleteRoom,      true, "Delete a room and its objects.");

    r.add("POST",   "/api/v1/rooms/:roomID/objects",        handleCreateObject,    true, "Create a room object.");
    r.add("PUT",    "/api/v1/objects/:objectID",            handleUpdateObject,    true, "Update a room object.");
    r.add("DELETE", "/api/v1/objects/:objectID",            handleDeleteObject,    true, "Delete a room object.");

    r.add("GET",    "/api/v1/dungeonTemplates",             handleListTemplates,   true, "List dungeon templates.");
    r.add("POST",   "/api/v1/dungeonTemplates",             handleCreateTemplate,  true, "Create a new dungeon template.");
    r.add("PUT",    "/api/v1/dungeonTemplates/:id",         handleUpdateTemplate,  true, "Update a dungeon template.");
    r.add("DELETE", "/api/v1/dungeonTemplates/:id",         handleDeleteTemplate,  true, "Delete a dungeon template.");

    r.add("GET",    "/api/v1/dungeonArchetypes",            handleListArchetypes,  true, "List archetype reference data.");
    r.add("GET",    "/api/v1/dungeonGroups",                handleListGroups,      true, "List dungeon object group reference data.");
    r.add("GET",    "/api/v1/factions",                     handleListFactions,    true, "List factions (reference data).");
}

} // namespace Handlers
} // namespace Devtools
} // namespace EvE

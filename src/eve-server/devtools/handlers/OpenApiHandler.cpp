/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    OpenApiHandler: emits a minimal OpenAPI 3.0.3 spec describing the registered
    routes.  The Rust/TS client uses this to generate typed bindings at build
    time.  Only routes that were added through Router::add are listed.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "devtools/DevtoolsJson.h"
#include "devtools/DevtoolsRouter.h"
#include "devtools/DevtoolsServer.h"
#include "devtools/handlers/AllHandlers.h"

namespace EvE {
namespace Devtools {
namespace Handlers {

static std::string toOpenApiPath(const std::string& pattern)
{
    // Transform /a/:id/b to /a/{id}/b
    std::string out;
    out.reserve(pattern.size());
    for (size_t i = 0; i < pattern.size(); ) {
        if (pattern[i] == ':') {
            out += '{';
            size_t j = i + 1;
            while (j < pattern.size() && pattern[j] != '/') ++j;
            out.append(pattern, i + 1, j - (i + 1));
            out += '}';
            i = j;
        } else {
            out += pattern[i++];
        }
    }
    return out;
}

static Response handleOpenApi(Request&)
{
    Json root = Json::object();
    root["openapi"] = Json("3.0.3");

    Json info = Json::object();
    info["title"]       = Json("EVEmu DevTools Admin API");
    info["version"]     = Json("0.1.0");
    info["description"] = Json(
        "Remote content authoring and live-control API for EVEmu servers.  All "
        "mutating calls require a bearer token and are written to devtoolsAudit.");
    root["info"] = info;

    Json servers = Json::array();
    Json srv = Json::object();
    srv["url"] = Json("/");
    servers.push_back(srv);
    root["servers"] = servers;

    // Security schemes.
    Json components = Json::object();
    Json securitySchemes = Json::object();
    Json bearer = Json::object();
    bearer["type"]   = Json("http");
    bearer["scheme"] = Json("bearer");
    securitySchemes["bearerAuth"] = bearer;
    components["securitySchemes"] = securitySchemes;
    root["components"] = components;

    // Paths.
    Json paths = Json::object();
    auto routes = sDevtools.router().routes();
    for (const auto& route : routes) {
        std::string path = toOpenApiPath(route.pattern);

        if (!paths.has(path)) paths[path] = Json::object();
        Json& pathItem = paths[path];

        std::string method = route.method;
        for (char& c : method) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        Json op = Json::object();
        op["summary"] = Json(route.description.empty() ? route.pattern : route.description);

        // Parameters from :name segments.
        if (!route.paramNames.empty()) {
            Json params = Json::array();
            for (const auto& p : route.paramNames) {
                if (p.empty()) continue;
                Json param = Json::object();
                param["name"] = Json(p);
                param["in"]   = Json("path");
                param["required"] = Json(true);
                Json schema = Json::object();
                schema["type"] = Json("string");
                param["schema"] = schema;
                params.push_back(param);
            }
            if (params.size() > 0) op["parameters"] = params;
        }

        if (route.requiresAuth) {
            Json sec = Json::array();
            Json s = Json::object();
            s["bearerAuth"] = Json::array();
            sec.push_back(s);
            op["security"] = sec;
        }

        Json responses = Json::object();
        Json ok = Json::object();
        ok["description"] = Json("Success");
        responses["200"] = ok;
        Json unauth = Json::object();
        unauth["description"] = Json("Missing or invalid bearer token");
        responses["401"] = unauth;
        op["responses"] = responses;

        pathItem[method] = op;
    }
    root["paths"] = paths;

    return Response::ok(root);
}

void RegisterOpenApi(Router& r)
{
    r.add("GET", "/api/v1/openapi.json", handleOpenApi, /*requiresAuth=*/false,
          "Machine-readable description of every registered route.");
}

} // namespace Handlers
} // namespace Devtools
} // namespace EvE

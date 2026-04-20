/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Request/Response types + a simple HTTP router used by the DevTools admin API.
    Route patterns support :paramName placeholders, e.g. "/dungeons/:id/rooms".
    ------------------------------------------------------------------------------------
*/

#ifndef __DEVTOOLS__DEVTOOLS_ROUTER_H__INCL__
#define __DEVTOOLS__DEVTOOLS_ROUTER_H__INCL__

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "devtools/DevtoolsJson.h"

namespace EvE {
namespace Devtools {

/// Authenticated principal for a request.  When an endpoint is marked as not
/// requiring auth (e.g. /healthz, /auth/login), this struct is left at its
/// default (empty) state.
struct Principal {
    bool authenticated = false;
    uint32_t accountID = 0;
    std::string accountName;
    int64_t role = 0;
};

struct Request {
    std::string method;          // "GET", "POST", "PUT", "DELETE"
    std::string path;            // "/api/v1/dungeons/42"
    std::string rawQuery;        // "foo=1&bar=baz" (no leading ?)
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;     // lower-cased header names
    std::string body;
    Json bodyJson;               // Populated when Content-Type is JSON and body parses.
    bool bodyIsJson = false;

    std::map<std::string, std::string> pathParams;  // filled by router when route matches
    std::string remoteAddr;

    Principal principal;
};

struct Response {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
    std::map<std::string, std::string> extraHeaders;

    static Response ok(const Json& body);
    static Response ok();
    static Response text(int status, const std::string& text);
    static Response error(int status, const std::string& code, const std::string& message);
};

using HandlerFn = std::function<Response(Request&)>;

struct Route {
    std::string method;
    std::vector<std::string> patternSegments;  // "" entries mean ":param"; names stored in paramNames
    std::vector<std::string> paramNames;       // same length as patternSegments; non-empty for parameterized segments
    bool requiresAuth = true;
    HandlerFn handler;
    std::string pattern;
    std::string description;
};

class Router
{
public:
    Router();

    /// Register a route.  `pattern` uses :name for path parameters, e.g.
    /// "/dungeons/:id/rooms/:roomID".  When `requiresAuth` is true (the default)
    /// the request's Principal must be populated by the caller.
    void add(const std::string& method, const std::string& pattern,
             HandlerFn handler, bool requiresAuth = true,
             const std::string& description = std::string());

    /// Find a matching route and execute it.  Returns false when no route matches;
    /// in that case the caller should produce a 404.  Populates req.pathParams.
    bool dispatch(Request& req, Response& res) const;

    /// Snapshot of all registered routes.  Used to emit the OpenAPI spec.
    std::vector<Route> routes() const;

private:
    static void splitPattern(const std::string& pattern,
                             std::vector<std::string>& segments,
                             std::vector<std::string>& paramNames);

    mutable std::mutex m_mutex;
    std::vector<Route> m_routes;
};

} // namespace Devtools
} // namespace EvE

#endif // __DEVTOOLS__DEVTOOLS_ROUTER_H__INCL__

/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    HTTP router implementation.  Longest-path-first matching with :name
    placeholders; nothing fancy.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "devtools/DevtoolsRouter.h"

#include <algorithm>

namespace EvE {
namespace Devtools {

Response Response::ok(const Json& body)
{
    Response r;
    r.status = 200;
    r.body = body.dump(false);
    return r;
}

Response Response::ok()
{
    Response r;
    r.status = 204;
    r.contentType = "text/plain; charset=utf-8";
    r.body.clear();
    return r;
}

Response Response::text(int status, const std::string& text)
{
    Response r;
    r.status = status;
    r.contentType = "text/plain; charset=utf-8";
    r.body = text;
    return r;
}

Response Response::error(int status, const std::string& code, const std::string& message)
{
    Json body = Json::object();
    body["error"] = Json(code);
    body["message"] = Json(message);
    Response r = Response::ok(body);
    r.status = status;
    return r;
}

Router::Router() = default;

void Router::splitPattern(const std::string& pattern,
                          std::vector<std::string>& segments,
                          std::vector<std::string>& paramNames)
{
    segments.clear();
    paramNames.clear();
    size_t i = 0;
    if (!pattern.empty() && pattern[0] == '/') i = 1;  // skip leading /
    while (i < pattern.size()) {
        size_t j = pattern.find('/', i);
        if (j == std::string::npos) j = pattern.size();
        std::string seg = pattern.substr(i, j - i);
        if (!seg.empty() && seg[0] == ':') {
            segments.push_back(std::string());
            paramNames.push_back(seg.substr(1));
        } else {
            segments.push_back(seg);
            paramNames.push_back(std::string());
        }
        i = (j < pattern.size()) ? j + 1 : j;
    }
}

void Router::add(const std::string& method, const std::string& pattern,
                 HandlerFn handler, bool requiresAuth,
                 const std::string& description)
{
    Route r;
    r.method = method;
    r.pattern = pattern;
    r.description = description;
    r.handler = std::move(handler);
    r.requiresAuth = requiresAuth;
    splitPattern(pattern, r.patternSegments, r.paramNames);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_routes.push_back(std::move(r));
}

static void splitPath(const std::string& path, std::vector<std::string>& out)
{
    out.clear();
    size_t i = 0;
    if (!path.empty() && path[0] == '/') i = 1;
    while (i < path.size()) {
        size_t j = path.find('/', i);
        if (j == std::string::npos) j = path.size();
        out.push_back(path.substr(i, j - i));
        i = (j < path.size()) ? j + 1 : j;
    }
}

bool Router::dispatch(Request& req, Response& res) const
{
    std::vector<std::string> segs;
    splitPath(req.path, segs);

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const Route& r : m_routes) {
        if (r.method != req.method) continue;
        if (r.patternSegments.size() != segs.size()) continue;
        bool matched = true;
        for (size_t i = 0; i < segs.size(); ++i) {
            if (!r.paramNames[i].empty()) continue;  // param segment
            if (r.patternSegments[i] != segs[i]) { matched = false; break; }
        }
        if (!matched) continue;

        req.pathParams.clear();
        for (size_t i = 0; i < segs.size(); ++i) {
            if (!r.paramNames[i].empty()) req.pathParams[r.paramNames[i]] = segs[i];
        }

        if (r.requiresAuth && !req.principal.authenticated) {
            res = Response::error(401, "unauthorized", "Bearer token required.");
            return true;
        }

        try {
            res = r.handler(req);
        } catch (const std::exception& ex) {
            res = Response::error(500, "internal_error", ex.what());
        } catch (...) {
            res = Response::error(500, "internal_error", "Unhandled exception in handler.");
        }
        return true;
    }
    return false;
}

std::vector<Route> Router::routes() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_routes;
}

} // namespace Devtools
} // namespace EvE

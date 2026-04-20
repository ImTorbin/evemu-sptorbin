/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    DevTools admin API server.  Listener + per-connection HTTP handler all live
    in this translation unit so the asio types do not leak into the rest of the
    codebase.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "EVEServerConfig.h"
#include "devtools/DevtoolsAudit.h"
#include "devtools/DevtoolsAuth.h"
#include "devtools/DevtoolsJson.h"
#include "devtools/DevtoolsServer.h"
#include "devtools/handlers/AllHandlers.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace EvE {
namespace Devtools {

//---------------------------------------------------------------------
// LogRing
//---------------------------------------------------------------------
void LogRing::push(const std::string& level, const std::string& tag, const std::string& message)
{
    Entry e;
    e.ts = std::chrono::system_clock::now();
    e.level = level;
    e.tag = tag;
    e.message = message;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_entries.size() >= m_max) m_entries.pop_front();
    m_entries.push_back(std::move(e));
}

std::vector<LogRing::Entry> LogRing::snapshot(size_t max) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (max == 0 || max > m_entries.size()) {
        return std::vector<Entry>(m_entries.begin(), m_entries.end());
    }
    return std::vector<Entry>(m_entries.end() - max, m_entries.end());
}

//---------------------------------------------------------------------
// Connection (opaque helper)
//---------------------------------------------------------------------
namespace {

class DevtoolsConnection : public std::enable_shared_from_this<DevtoolsConnection>
{
public:
    DevtoolsConnection(boost::asio::io_context& io, Router& router)
        : m_socket(io), m_router(router)
    {
    }

    boost::asio::ip::tcp::socket& socket() { return m_socket; }

    void start()
    {
        boost::asio::async_read_until(
            m_socket, m_buffer, "\r\n\r\n",
            std::bind(&DevtoolsConnection::onHeaders, shared_from_this(),
                      std::placeholders::_1, std::placeholders::_2));
    }

private:
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::streambuf m_buffer;
    std::string m_response;
    Request m_request;
    Router& m_router;
    size_t m_contentLength = 0;
    size_t m_headerEnd = 0;
    std::string m_headerBlock;

    static std::string toLower(const std::string& s)
    {
        std::string out = s;
        for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return out;
    }

    static std::string trim(const std::string& s)
    {
        size_t a = 0;
        while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) ++a;
        size_t b = s.size();
        while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r' || s[b-1] == '\n')) --b;
        return s.substr(a, b - a);
    }

    void onHeaders(const boost::system::error_code& ec, size_t)
    {
        if (ec) { close(); return; }

        std::istream stream(&m_buffer);
        std::string requestLine;
        std::getline(stream, requestLine, '\n');
        if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

        // Parse "METHOD /path?query HTTP/1.1"
        size_t sp1 = requestLine.find(' ');
        size_t sp2 = (sp1 == std::string::npos) ? std::string::npos : requestLine.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos) {
            sendStatusOnly(400, "Bad Request");
            return;
        }
        m_request.method = requestLine.substr(0, sp1);
        std::string url = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);
        size_t q = url.find('?');
        if (q == std::string::npos) {
            m_request.path = url;
        } else {
            m_request.path = url.substr(0, q);
            m_request.rawQuery = url.substr(q + 1);
            parseQuery(m_request.rawQuery, m_request.query);
        }

        std::string headerLine;
        while (std::getline(stream, headerLine, '\n')) {
            if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
            if (headerLine.empty()) break;
            size_t colon = headerLine.find(':');
            if (colon == std::string::npos) continue;
            std::string name  = toLower(trim(headerLine.substr(0, colon)));
            std::string value = trim(headerLine.substr(colon + 1));
            m_request.headers[name] = value;
        }

        auto it = m_request.headers.find("content-length");
        if (it != m_request.headers.end()) {
            try { m_contentLength = std::stoul(it->second); } catch (...) { m_contentLength = 0; }
        }

        // remote address
        try {
            auto ep = m_socket.remote_endpoint();
            std::ostringstream oss;
            oss << ep.address().to_string() << ":" << ep.port();
            m_request.remoteAddr = oss.str();
        } catch (...) {
            m_request.remoteAddr = "?";
        }

        if (m_contentLength > 0) {
            // Any body bytes already buffered past the header block.
            std::string partial;
            size_t available = m_buffer.size();
            if (available > 0) {
                std::ostringstream oss;
                oss << &m_buffer;
                partial = oss.str();
            }
            if (partial.size() >= m_contentLength) {
                m_request.body = partial.substr(0, m_contentLength);
                onRequestReady();
                return;
            }
            m_request.body = std::move(partial);
            size_t remaining = m_contentLength - m_request.body.size();
            auto self = shared_from_this();
            boost::asio::async_read(
                m_socket, m_buffer, boost::asio::transfer_exactly(remaining),
                [self](const boost::system::error_code& ec2, size_t n) {
                    if (ec2) { self->close(); return; }
                    std::ostringstream oss;
                    oss << &self->m_buffer;
                    self->m_request.body += oss.str().substr(0, n);
                    self->onRequestReady();
                });
        } else {
            onRequestReady();
        }
    }

    void onRequestReady()
    {
        // Body as JSON?
        auto it = m_request.headers.find("content-type");
        if (it != m_request.headers.end() && it->second.find("application/json") != std::string::npos
            && !m_request.body.empty())
        {
            std::string err;
            if (Json::parse(m_request.body, m_request.bodyJson, err)) {
                m_request.bodyIsJson = true;
            } else {
                sendResponse(Response::error(400, "bad_json", "Request body is not valid JSON: " + err));
                return;
            }
        }

        // CORS preflight: accept any Origin, allow common methods/headers.
        if (m_request.method == "OPTIONS") {
            Response r;
            r.status = 204;
            r.extraHeaders["Access-Control-Allow-Origin"] = "*";
            r.extraHeaders["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
            r.extraHeaders["Access-Control-Allow-Headers"] = "Authorization, Content-Type";
            sendResponse(r);
            return;
        }

        // Auth - only for /api/v1/* routes; /healthz is unauthenticated.
        auto authHeader = m_request.headers.find("authorization");
        if (authHeader != m_request.headers.end()) {
            std::string code, msg;
            DevtoolsAuth::VerifyBearer(authHeader->second, m_request.principal, code, msg);
        }

        Response res;
        if (m_router.dispatch(m_request, res)) {
            // Audit every mutating call.
            const std::string& m = m_request.method;
            if (m == "POST" || m == "PUT" || m == "DELETE" || m == "PATCH") {
                std::string notes;
                if (!res.body.empty() && res.body.size() < 256) notes = res.body;
                DevtoolsAudit::Log(m_request, res.status, notes);
            }
            sendResponse(res);
        } else {
            sendResponse(Response::error(404, "not_found", "No such route: " + m_request.method + " " + m_request.path));
        }
    }

    static std::string urlDecode(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '+') { out += ' '; }
            else if (s[i] == '%' && i + 2 < s.size()) {
                auto hex = [](char c)->int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
                    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
                    return -1;
                };
                int a = hex(s[i+1]); int b = hex(s[i+2]);
                if (a >= 0 && b >= 0) { out += static_cast<char>((a << 4) | b); i += 2; }
                else out += s[i];
            } else {
                out += s[i];
            }
        }
        return out;
    }

    static void parseQuery(const std::string& raw, std::map<std::string, std::string>& out)
    {
        size_t i = 0;
        while (i < raw.size()) {
            size_t amp = raw.find('&', i);
            std::string tok = raw.substr(i, amp - i);
            size_t eq = tok.find('=');
            if (eq == std::string::npos) {
                out[urlDecode(tok)] = "";
            } else {
                out[urlDecode(tok.substr(0, eq))] = urlDecode(tok.substr(eq + 1));
            }
            if (amp == std::string::npos) break;
            i = amp + 1;
        }
    }

    void sendStatusOnly(int status, const std::string& reason)
    {
        Response r;
        r.status = status;
        r.contentType = "text/plain; charset=utf-8";
        r.body = reason;
        sendResponse(r);
    }

    void sendResponse(const Response& res)
    {
        static const std::map<int, std::string> reasons = {
            {200, "OK"}, {201, "Created"}, {204, "No Content"},
            {400, "Bad Request"}, {401, "Unauthorized"}, {403, "Forbidden"},
            {404, "Not Found"}, {409, "Conflict"}, {500, "Internal Server Error"}
        };
        auto rit = reasons.find(res.status);
        std::string reason = (rit != reasons.end()) ? rit->second : "OK";

        std::ostringstream oss;
        oss << "HTTP/1.1 " << res.status << " " << reason << "\r\n";
        oss << "Content-Type: " << res.contentType << "\r\n";
        oss << "Content-Length: " << res.body.size() << "\r\n";
        oss << "Connection: close\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        for (const auto& kv : res.extraHeaders) {
            oss << kv.first << ": " << kv.second << "\r\n";
        }
        oss << "\r\n";
        m_response = oss.str();
        m_response += res.body;

        auto self = shared_from_this();
        boost::asio::async_write(
            m_socket, boost::asio::buffer(m_response),
            [self](const boost::system::error_code&, size_t) { self->close(); });
    }

    void close()
    {
        boost::system::error_code ec;
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        m_socket.close(ec);
    }
};

class DevtoolsListener
{
public:
    DevtoolsListener(boost::asio::io_context& io, const boost::asio::ip::tcp::endpoint& ep, Router& router)
        : m_io(io), m_acceptor(io, ep), m_router(router)
    {
        accept();
    }

private:
    void accept()
    {
        auto conn = std::make_shared<DevtoolsConnection>(m_io, m_router);
        m_acceptor.async_accept(conn->socket(),
            [this, conn](const boost::system::error_code& ec) {
                if (!ec) conn->start();
                accept();
            });
    }

    boost::asio::io_context& m_io;
    boost::asio::ip::tcp::acceptor m_acceptor;
    Router& m_router;
};

} // unnamed namespace

//---------------------------------------------------------------------
// DevtoolsServer::Impl
//---------------------------------------------------------------------
struct DevtoolsServer::Impl
{
    std::unique_ptr<boost::asio::io_context> io;
    std::unique_ptr<DevtoolsListener> listener;
};

DevtoolsServer::DevtoolsServer()
    : m_running(false), m_impl(new Impl())
{
    Handlers::RegisterAll(m_router);
}

DevtoolsServer::~DevtoolsServer()
{
    Stop();
}

void DevtoolsServer::RecordEvent(const std::string& tag, const std::string& message)
{
    m_logRing.push("info", tag, message);
}

bool DevtoolsServer::Start()
{
    if (m_running.exchange(true)) return true;

    const std::string& bind = sConfig.devtools.allowRemote ? sConfig.net.apiServerBind : std::string("127.0.0.1");
    uint16 port = sConfig.net.apiServerPort;

    try {
        m_impl->io.reset(new boost::asio::io_context());
        boost::asio::ip::address addr = boost::asio::ip::make_address(bind);
        boost::asio::ip::tcp::endpoint ep(addr, port);
        m_impl->listener.reset(new DevtoolsListener(*m_impl->io, ep, m_router));
        sLog.Green("  DevTools API", "Listening on %s:%u", bind.c_str(), port);
    } catch (const std::exception& ex) {
        sLog.Error("  DevTools API", "Failed to bind %s:%u - %s", bind.c_str(), port, ex.what());
        m_impl->io.reset();
        m_running.store(false);
        return false;
    }

    m_ioThread.reset(new std::thread([this]() { RunIo(); }));
    return true;
}

void DevtoolsServer::Stop()
{
    if (!m_running.exchange(false)) return;
    if (m_impl && m_impl->io) {
        try { m_impl->io->stop(); } catch (...) {}
    }
    if (m_ioThread && m_ioThread->joinable()) {
        try { m_ioThread->join(); } catch (...) {}
    }
    m_ioThread.reset();
    if (m_impl) {
        m_impl->listener.reset();
        m_impl->io.reset();
    }
    sLog.Warning("  DevTools API", "Admin API stopped.");
}

void DevtoolsServer::RunIo()
{
    try {
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard(
            m_impl->io->get_executor());
        m_impl->io->run();
    } catch (const std::exception& ex) {
        sLog.Error("  DevTools API", "io thread died: %s", ex.what());
    }
}

} // namespace Devtools
} // namespace EvE

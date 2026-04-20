/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Entry point for the remote DevTools admin HTTP/JSON API.

    The server runs on its own boost::asio io_context in a dedicated thread so
    that incoming admin requests do not block the main game loop.  It does NOT
    terminate TLS - operators are expected to front it with nginx/caddy and
    bind this to 127.0.0.1.  See utils/config/eve-server.xml <devtools>.
    ------------------------------------------------------------------------------------
*/

#ifndef __DEVTOOLS__DEVTOOLS_SERVER_H__INCL__
#define __DEVTOOLS__DEVTOOLS_SERVER_H__INCL__

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "utils/Singleton.h"
#include "devtools/DevtoolsRouter.h"

class DevtoolsListener;  // defined in DevtoolsServer.cpp

namespace EvE {
namespace Devtools {

/// In-memory ring buffer of recent log lines.  Fed by a hook in the core logger.
/// Handlers expose this via /control/logs/tail for the Live tab in the UI.
class LogRing
{
public:
    struct Entry {
        std::chrono::system_clock::time_point ts;
        std::string level;
        std::string tag;
        std::string message;
    };

    explicit LogRing(size_t maxEntries = 1024) : m_max(maxEntries) {}
    void push(const std::string& level, const std::string& tag, const std::string& message);
    std::vector<Entry> snapshot(size_t max = 0) const;

private:
    mutable std::mutex m_mutex;
    std::deque<Entry> m_entries;
    size_t m_max;
};

class DevtoolsServer : public Singleton<DevtoolsServer>
{
public:
    DevtoolsServer();
    ~DevtoolsServer();

    /// Start the listener thread.  Safe to call multiple times; extra calls are no-ops.
    /// Returns true on success (or if already running), false on bind failure.
    bool Start();

    /// Stop the listener thread and close any in-flight connections.
    void Stop();

    bool IsRunning() const { return m_running.load(); }

    Router& router() { return m_router; }
    const Router& router() const { return m_router; }

    LogRing& logRing() { return m_logRing; }

    /// Helper used by the server's own command console to log noteworthy events.
    void RecordEvent(const std::string& tag, const std::string& message);

private:
    void RunIo();

    Router m_router;
    LogRing m_logRing;

    std::atomic<bool> m_running;
    std::unique_ptr<std::thread> m_ioThread;

    // Opaque asio bits kept behind forward declarations in DevtoolsServer.cpp to
    // keep boost headers out of this public header.
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Devtools
} // namespace EvE

#define sDevtools \
    ( EvE::Devtools::DevtoolsServer::get() )

#endif // __DEVTOOLS__DEVTOOLS_SERVER_H__INCL__

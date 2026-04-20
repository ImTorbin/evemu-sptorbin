/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Audit log writer.  Every mutating DevTools API call routes through
    DevtoolsAudit::Log so operators can retrace who changed what.
    ------------------------------------------------------------------------------------
*/

#ifndef __DEVTOOLS__DEVTOOLS_AUDIT_H__INCL__
#define __DEVTOOLS__DEVTOOLS_AUDIT_H__INCL__

#include <string>

namespace EvE {
namespace Devtools {

struct Request;

class DevtoolsAudit
{
public:
    /// Write a row to devtoolsAudit.  Safe to call even if the table does not
    /// exist yet (it will simply log the failure to the server log and move on).
    /// `notes` is a short operator-friendly string for additional detail.
    static void Log(const Request& req, int status, const std::string& notes);

    /// Explicit form when we do not have a Request object (e.g. during startup
    /// warnings or non-HTTP events).
    static void LogRaw(uint32_t accountID, const std::string& accountName,
                       const std::string& remoteAddr, const std::string& method,
                       const std::string& path, int status,
                       const std::string& requestBody, const std::string& notes);
};

} // namespace Devtools
} // namespace EvE

#endif // __DEVTOOLS__DEVTOOLS_AUDIT_H__INCL__

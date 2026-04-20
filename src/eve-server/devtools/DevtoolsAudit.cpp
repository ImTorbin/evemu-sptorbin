/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Implementation of the DevTools audit log writer.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "devtools/DevtoolsAudit.h"
#include "devtools/DevtoolsRouter.h"

namespace EvE {
namespace Devtools {

// Truncate long bodies before writing to avoid blowing out the MEDIUMTEXT and
// also to keep the audit log readable.
static const size_t kMaxAuditBodyBytes = 32 * 1024;

void DevtoolsAudit::Log(const Request& req, int status, const std::string& notes)
{
    std::string body = req.body;
    if (body.size() > kMaxAuditBodyBytes) {
        body.resize(kMaxAuditBodyBytes);
        body += "... [truncated]";
    }
    LogRaw(req.principal.accountID,
           req.principal.accountName,
           req.remoteAddr,
           req.method,
           req.path,
           status,
           body,
           notes);
}

void DevtoolsAudit::LogRaw(uint32_t accountID, const std::string& accountName,
                           const std::string& remoteAddr, const std::string& method,
                           const std::string& path, int status,
                           const std::string& requestBody, const std::string& notes)
{
    std::string eName, eAddr, eMethod, ePath, eBody, eNotes;
    sDatabase.DoEscapeString(eName,   accountName);
    sDatabase.DoEscapeString(eAddr,   remoteAddr);
    sDatabase.DoEscapeString(eMethod, method);
    sDatabase.DoEscapeString(ePath,   path);
    sDatabase.DoEscapeString(eBody,   requestBody);
    sDatabase.DoEscapeString(eNotes,  notes);

    DBerror err;
    if (!sDatabase.RunQuery(err,
            "INSERT INTO devtoolsAudit"
            " (accountID, accountName, remoteAddr, method, path, status, requestBody, notes)"
            " VALUES (%u, '%s', '%s', '%s', '%s', %d, '%s', '%s')",
            accountID, eName.c_str(), eAddr.c_str(), eMethod.c_str(),
            ePath.c_str(), status, eBody.c_str(), eNotes.c_str())) {
        sLog.Error("DevtoolsAudit",
                   "Failed to write audit row (did you run the 20260420120000-devtoolsAudit migration?): %s",
                   err.c_str());
    }
}

} // namespace Devtools
} // namespace EvE

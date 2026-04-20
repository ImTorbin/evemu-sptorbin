/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Bearer-token authentication for the DevTools admin API.

    Two token formats are accepted:

      1. Bootstrap admin token (configured in eve-server.xml devtools.adminToken).
         Compared in constant time.  Never expires.  Granted role = required role.

      2. Signed session token of the form
              base64url(payload) . base64url(HMAC-SHA256(secret, payload))
         Payload is a JSON object with fields:
              { "aid": accountID, "name": accountName, "role": role, "exp": expiresUnix }
         Re-validated on every request; no server-side session table.
    ------------------------------------------------------------------------------------
*/

#ifndef __DEVTOOLS__DEVTOOLS_AUTH_H__INCL__
#define __DEVTOOLS__DEVTOOLS_AUTH_H__INCL__

#include <string>

#include "devtools/DevtoolsRouter.h"

namespace EvE {
namespace Devtools {

class DevtoolsAuth
{
public:
    /// Issue a signed bearer token for `accountID`.  Caller must have already
    /// verified password/role.  Returns empty string if devtools.tokenSecret is
    /// blank.
    static std::string IssueToken(uint32_t accountID, const std::string& name, int64_t role);

    /// Parse Authorization: Bearer ... header and fill `principal`.  Returns
    /// true on success.  On failure `errorCode`/`errorMessage` get populated
    /// for a 401 response.
    static bool VerifyBearer(const std::string& header, Principal& principal,
                             std::string& errorCode, std::string& errorMessage);

    /// Look up an account by name + plaintext password.  Fills `principal` on
    /// success and returns true.  Also enforces the configured role mask.
    static bool LoginWithPassword(const std::string& accountName, const std::string& password,
                                  Principal& principal, std::string& errorMessage);

    /// Swap the bootstrap adminToken for a fresh session Principal.  Does NOT
    /// mint a new JWT-like token by itself - /auth/login calls IssueToken after
    /// this returns true.
    static bool LoginWithBootstrapToken(const std::string& token, Principal& principal,
                                        std::string& errorMessage);

    /// True if the bearer header or the principal contains the required role
    /// mask from sConfig.devtools.requiredRole.  Convenience for handlers.
    static bool HasRequiredRole(const Principal& p);
};

} // namespace Devtools
} // namespace EvE

#endif // __DEVTOOLS__DEVTOOLS_AUTH_H__INCL__

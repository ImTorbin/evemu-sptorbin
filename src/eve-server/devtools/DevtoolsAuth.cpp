/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    DevTools admin API authentication.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "devtools/DevtoolsAuth.h"
#include "devtools/DevtoolsHash.h"
#include "devtools/DevtoolsJson.h"
#include "EVEServerConfig.h"

#include <chrono>

namespace EvE {
namespace Devtools {

static int64_t nowUnix()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

bool DevtoolsAuth::HasRequiredRole(const Principal& p)
{
    if (!p.authenticated) return false;
    const int64_t required = sConfig.devtools.requiredRole;
    if (required == 0) return true;  // operator explicitly disabled the check
    return (p.role & required) == required;
}

std::string DevtoolsAuth::IssueToken(uint32_t accountID, const std::string& name, int64_t role)
{
    const std::string& secret = sConfig.devtools.tokenSecret;
    if (secret.empty()) {
        sLog.Warning("DevtoolsAuth", "Cannot issue tokens: devtools.tokenSecret is empty.");
        return std::string();
    }

    Json payload = Json::object();
    payload["aid"]  = Json(static_cast<int64_t>(accountID));
    payload["name"] = Json(name);
    payload["role"] = Json(role);
    payload["exp"]  = Json(nowUnix() + static_cast<int64_t>(sConfig.devtools.tokenTtlSeconds));

    std::string payloadStr = payload.dump(false);
    std::string p64 = Base64UrlEncode(reinterpret_cast<const uint8_t*>(payloadStr.data()), payloadStr.size());
    auto mac = HmacSha256(secret, p64);
    std::string s64 = Base64UrlEncode(mac.data(), mac.size());
    return p64 + "." + s64;
}

bool DevtoolsAuth::VerifyBearer(const std::string& header, Principal& principal,
                                std::string& errorCode, std::string& errorMessage)
{
    principal = Principal();
    errorCode.clear();
    errorMessage.clear();

    const std::string prefix = "Bearer ";
    std::string token;
    if (header.size() > prefix.size() && std::equal(prefix.begin(), prefix.end(), header.begin(),
                                                    [](char a, char b){ return std::tolower(a) == std::tolower(b); })) {
        token = header.substr(prefix.size());
    } else {
        token = header;  // allow raw token too (simplifies curl testing).
    }
    if (token.empty()) {
        errorCode = "unauthorized"; errorMessage = "Missing bearer token.";
        return false;
    }
    // Trim whitespace.
    while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) token.erase(token.begin());
    while (!token.empty() && (token.back()  == ' ' || token.back()  == '\t' || token.back() == '\r' || token.back() == '\n')) token.pop_back();

    // Case 1: bootstrap admin token.
    const std::string& admin = sConfig.devtools.adminToken;
    if (!admin.empty() && ConstantTimeEquals(admin, token)) {
        principal.authenticated = true;
        principal.accountID = 0;
        principal.accountName = "bootstrap";
        principal.role = sConfig.devtools.requiredRole;  // grants exactly the required role
        return true;
    }

    // Case 2: signed session token.
    size_t dot = token.find('.');
    if (dot == std::string::npos) {
        errorCode = "unauthorized"; errorMessage = "Malformed bearer token.";
        return false;
    }
    std::string p64 = token.substr(0, dot);
    std::string s64 = token.substr(dot + 1);

    const std::string& secret = sConfig.devtools.tokenSecret;
    if (secret.empty()) {
        errorCode = "unauthorized"; errorMessage = "Server is not configured to accept signed tokens.";
        return false;
    }

    auto expected = HmacSha256(secret, p64);
    std::string expected64 = Base64UrlEncode(expected.data(), expected.size());
    if (!ConstantTimeEquals(expected64, s64)) {
        errorCode = "unauthorized"; errorMessage = "Bad token signature.";
        return false;
    }

    std::string payloadStr = Base64UrlDecode(p64);
    Json payload; std::string parseErr;
    if (!Json::parse(payloadStr, payload, parseErr) || !payload.isObject()) {
        errorCode = "unauthorized"; errorMessage = "Bad token payload.";
        return false;
    }
    int64_t exp = payload.get("exp").asInt(0);
    if (exp && nowUnix() > exp) {
        errorCode = "token_expired"; errorMessage = "Token has expired; please log in again.";
        return false;
    }
    principal.authenticated = true;
    principal.accountID     = static_cast<uint32_t>(payload.get("aid").asInt(0));
    principal.accountName   = payload.get("name").asString();
    principal.role          = payload.get("role").asInt(0);

    if (!HasRequiredRole(principal)) {
        errorCode = "forbidden"; errorMessage = "Account does not carry the required role mask.";
        principal = Principal();
        return false;
    }
    return true;
}

bool DevtoolsAuth::LoginWithPassword(const std::string& accountName, const std::string& password,
                                     Principal& principal, std::string& errorMessage)
{
    principal = Principal();
    if (accountName.empty() || password.empty()) {
        errorMessage = "accountName and password are required.";
        return false;
    }

    std::string eName;
    sDatabase.DoEscapeString(eName, accountName);

    DBQueryResult res;
    if (!sDatabase.RunQuery(res,
            "SELECT accountID, password, role, banned"
            " FROM account WHERE accountName = '%s'", eName.c_str())) {
        sLog.Error("DevtoolsAuth", "DB error looking up account '%s': %s", accountName.c_str(), res.error.c_str());
        errorMessage = "Database error.";
        return false;
    }

    DBResultRow row;
    if (!res.GetRow(row)) {
        errorMessage = "Unknown account or bad password.";
        return false;
    }
    uint32_t accountID = row.GetUInt(0);
    std::string storedPw = row.IsNull(1) ? "" : row.GetText(1);
    int64_t role = row.GetInt64(2);
    bool banned = row.GetInt(3) != 0;

    if (banned) {
        errorMessage = "Account is banned.";
        return false;
    }
    if (!ConstantTimeEquals(storedPw, password)) {
        errorMessage = "Unknown account or bad password.";
        return false;
    }

    principal.authenticated = true;
    principal.accountID = accountID;
    principal.accountName = accountName;
    principal.role = role;
    if (!HasRequiredRole(principal)) {
        errorMessage = "Account does not carry the required role mask for DevTools.";
        principal = Principal();
        return false;
    }
    return true;
}

bool DevtoolsAuth::LoginWithBootstrapToken(const std::string& token, Principal& principal,
                                           std::string& errorMessage)
{
    principal = Principal();
    const std::string& admin = sConfig.devtools.adminToken;
    if (admin.empty()) {
        errorMessage = "No bootstrap admin token is configured.";
        return false;
    }
    if (!ConstantTimeEquals(admin, token)) {
        errorMessage = "Bad bootstrap token.";
        return false;
    }
    principal.authenticated = true;
    principal.accountID = 0;
    principal.accountName = "bootstrap";
    principal.role = sConfig.devtools.requiredRole;
    return true;
}

} // namespace Devtools
} // namespace EvE

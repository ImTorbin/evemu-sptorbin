/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Stand-alone SHA-256, HMAC-SHA-256, and base64url helpers used to sign the
    opaque bearer tokens handed out by the DevTools admin API.  Kept self-contained
    so the rest of the codebase does not grow an OpenSSL dependency.
    ------------------------------------------------------------------------------------
*/

#ifndef __DEVTOOLS__DEVTOOLS_HASH_H__INCL__
#define __DEVTOOLS__DEVTOOLS_HASH_H__INCL__

#include <array>
#include <cstdint>
#include <string>

namespace EvE {
namespace Devtools {

/// SHA-256 digest of `data` (raw bytes; exactly 32 bytes).
std::array<uint8_t, 32> Sha256(const std::string& data);

/// HMAC-SHA-256(key, data) -> 32 raw bytes.
std::array<uint8_t, 32> HmacSha256(const std::string& key, const std::string& data);

/// base64url encode (RFC 4648 §5) with no '=' padding.
std::string Base64UrlEncode(const uint8_t* data, size_t len);
inline std::string Base64UrlEncode(const std::array<uint8_t, 32>& a) {
    return Base64UrlEncode(a.data(), a.size());
}

/// base64url decode; returns empty string on error.
std::string Base64UrlDecode(const std::string& s);

/// Constant-time compare; returns true if strings are equal byte-for-byte.
bool ConstantTimeEquals(const std::string& a, const std::string& b);

} // namespace Devtools
} // namespace EvE

#endif // __DEVTOOLS__DEVTOOLS_HASH_H__INCL__

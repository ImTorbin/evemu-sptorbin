/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Minimal JSON implementation for the DevTools admin API.  Supports the full
    JSON grammar except for UTF-16 surrogate pairs in \uXXXX escapes (surrogate
    characters are emitted as-is).  Admin traffic is ASCII/UTF-8 already so this
    limitation does not matter in practice.
    ------------------------------------------------------------------------------------
*/

#include "eve-server.h"

#include "devtools/DevtoolsJson.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace EvE {
namespace Devtools {

const Json Json::s_null;

Json::Json()                  : m_type(Type::Null),   m_bool(false), m_int(0), m_double(0.0) {}
Json::Json(std::nullptr_t)    : m_type(Type::Null),   m_bool(false), m_int(0), m_double(0.0) {}
Json::Json(bool v)            : m_type(Type::Bool),   m_bool(v),     m_int(0), m_double(0.0) {}
Json::Json(int v)             : m_type(Type::Int),    m_bool(false), m_int(v), m_double(0.0) {}
Json::Json(int64_t v)         : m_type(Type::Int),    m_bool(false), m_int(v), m_double(0.0) {}
Json::Json(uint32_t v)        : m_type(Type::Int),    m_bool(false), m_int(static_cast<int64_t>(v)), m_double(0.0) {}
Json::Json(uint64_t v)        : m_type(Type::Int),    m_bool(false), m_int(static_cast<int64_t>(v)), m_double(0.0) {}
Json::Json(double v)          : m_type(Type::Double), m_bool(false), m_int(0), m_double(v) {}
Json::Json(const char* v)     : m_type(Type::String), m_bool(false), m_int(0), m_double(0.0), m_string(v ? v : "") {}
Json::Json(const std::string& v)
    : m_type(Type::String), m_bool(false), m_int(0), m_double(0.0), m_string(v) {}

Json Json::array()
{
    Json j;
    j.m_type = Type::Array;
    return j;
}

Json Json::object()
{
    Json j;
    j.m_type = Type::Object;
    return j;
}

bool Json::asBool(bool def) const
{
    switch (m_type) {
        case Type::Bool:   return m_bool;
        case Type::Int:    return m_int != 0;
        case Type::Double: return m_double != 0.0;
        case Type::String: return !m_string.empty();
        default:           return def;
    }
}

int64_t Json::asInt(int64_t def) const
{
    switch (m_type) {
        case Type::Bool:   return m_bool ? 1 : 0;
        case Type::Int:    return m_int;
        case Type::Double: return static_cast<int64_t>(m_double);
        case Type::String: {
            if (m_string.empty()) return def;
            try { return std::strtoll(m_string.c_str(), nullptr, 10); }
            catch (...) { return def; }
        }
        default:           return def;
    }
}

double Json::asDouble(double def) const
{
    switch (m_type) {
        case Type::Bool:   return m_bool ? 1.0 : 0.0;
        case Type::Int:    return static_cast<double>(m_int);
        case Type::Double: return m_double;
        case Type::String: {
            if (m_string.empty()) return def;
            try { return std::strtod(m_string.c_str(), nullptr); }
            catch (...) { return def; }
        }
        default:           return def;
    }
}

const std::string& Json::asString(const std::string& def) const
{
    if (m_type == Type::String) return m_string;
    static thread_local std::string scratch;
    scratch = def;
    return scratch;
}

bool Json::has(const std::string& key) const
{
    if (m_type != Type::Object) return false;
    return m_object.find(key) != m_object.end();
}

const Json& Json::get(const std::string& key) const
{
    if (m_type != Type::Object) return s_null;
    auto it = m_object.find(key);
    if (it == m_object.end()) return s_null;
    return it->second;
}

Json& Json::operator[](const std::string& key)
{
    if (m_type != Type::Object) {
        m_type = Type::Object;
        m_object.clear();
    }
    return m_object[key];
}

void Json::set(const std::string& key, Json value)
{
    if (m_type != Type::Object) {
        m_type = Type::Object;
        m_object.clear();
    }
    m_object[key] = std::move(value);
}

size_t Json::size() const
{
    if (m_type == Type::Array) return m_array.size();
    if (m_type == Type::Object) return m_object.size();
    if (m_type == Type::String) return m_string.size();
    return 0;
}

const Json& Json::at(size_t i) const
{
    if (m_type != Type::Array || i >= m_array.size()) return s_null;
    return m_array[i];
}

Json& Json::operator[](size_t i)
{
    if (m_type != Type::Array) {
        m_type = Type::Array;
        m_array.clear();
    }
    if (i >= m_array.size()) m_array.resize(i + 1);
    return m_array[i];
}

void Json::push_back(Json value)
{
    if (m_type != Type::Array) {
        m_type = Type::Array;
        m_array.clear();
    }
    m_array.push_back(std::move(value));
}

void Json::writeEscaped(std::string& out, const std::string& s)
{
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

void Json::writeIndent(std::string& out, int depth)
{
    for (int i = 0; i < depth; ++i) out += "  ";
}

void Json::dumpTo(std::string& out, bool pretty, int depth) const
{
    switch (m_type) {
        case Type::Null:
            out += "null";
            break;
        case Type::Bool:
            out += m_bool ? "true" : "false";
            break;
        case Type::Int: {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(m_int));
            out += buf;
            break;
        }
        case Type::Double: {
            if (std::isnan(m_double) || std::isinf(m_double)) {
                out += "null";  // JSON cannot represent these; emit null.
            } else {
                char buf[48];
                std::snprintf(buf, sizeof(buf), "%.17g", m_double);
                out += buf;
            }
            break;
        }
        case Type::String:
            writeEscaped(out, m_string);
            break;
        case Type::Array: {
            if (m_array.empty()) { out += "[]"; break; }
            out += '[';
            for (size_t i = 0; i < m_array.size(); ++i) {
                if (pretty) { out += '\n'; writeIndent(out, depth + 1); }
                m_array[i].dumpTo(out, pretty, depth + 1);
                if (i + 1 < m_array.size()) out += pretty ? "," : ",";
            }
            if (pretty) { out += '\n'; writeIndent(out, depth); }
            out += ']';
            break;
        }
        case Type::Object: {
            if (m_object.empty()) { out += "{}"; break; }
            out += '{';
            size_t i = 0;
            for (const auto& kv : m_object) {
                if (pretty) { out += '\n'; writeIndent(out, depth + 1); }
                writeEscaped(out, kv.first);
                out += pretty ? ": " : ":";
                kv.second.dumpTo(out, pretty, depth + 1);
                if (++i < m_object.size()) out += ",";
            }
            if (pretty) { out += '\n'; writeIndent(out, depth); }
            out += '}';
            break;
        }
    }
}

std::string Json::dump(bool pretty) const
{
    std::string out;
    out.reserve(128);
    dumpTo(out, pretty, 0);
    return out;
}

namespace {

struct Parser {
    const std::string& s;
    size_t i;
    std::string err;

    explicit Parser(const std::string& text) : s(text), i(0) {}

    void skipWs() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }
            if (c == '/' && i + 1 < s.size() && s[i+1] == '/') {
                // Line comment (non-standard but harmless; we accept for human-edited configs).
                i += 2;
                while (i < s.size() && s[i] != '\n') ++i;
                continue;
            }
            break;
        }
    }

    bool expect(char c) {
        skipWs();
        if (i >= s.size() || s[i] != c) {
            err = std::string("expected '") + c + "' at offset " + std::to_string(i);
            return false;
        }
        ++i;
        return true;
    }

    bool parseString(std::string& out) {
        if (i >= s.size() || s[i] != '"') { err = "expected string"; return false; }
        ++i;
        out.clear();
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= s.size()) { err = "bad escape"; return false; }
                char e = s[i++];
                switch (e) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        if (i + 4 > s.size()) { err = "bad \\u escape"; return false; }
                        unsigned code = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = s[i++];
                            unsigned d;
                            if      (h >= '0' && h <= '9') d = h - '0';
                            else if (h >= 'a' && h <= 'f') d = 10 + h - 'a';
                            else if (h >= 'A' && h <= 'F') d = 10 + h - 'A';
                            else { err = "bad hex"; return false; }
                            code = (code << 4) | d;
                        }
                        // Emit as UTF-8 (no surrogate pair handling).
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: err = "unknown escape"; return false;
                }
            } else {
                out += c;
            }
        }
        err = "unterminated string";
        return false;
    }

    bool parseNumber(Json& out) {
        size_t start = i;
        if (s[i] == '-') ++i;
        bool isFloat = false;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (i < s.size() && s[i] == '.') { isFloat = true; ++i; while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i; }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            isFloat = true; ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
        std::string tok = s.substr(start, i - start);
        if (tok.empty() || tok == "-") { err = "bad number"; return false; }
        if (isFloat) out = Json(std::strtod(tok.c_str(), nullptr));
        else         out = Json(static_cast<int64_t>(std::strtoll(tok.c_str(), nullptr, 10)));
        return true;
    }

    bool parseValue(Json& out) {
        skipWs();
        if (i >= s.size()) { err = "unexpected end of input"; return false; }
        char c = s[i];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') { std::string v; if (!parseString(v)) return false; out = Json(v); return true; }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber(out);
        if (c == 't' && s.compare(i, 4, "true")  == 0)  { i += 4; out = Json(true);  return true; }
        if (c == 'f' && s.compare(i, 5, "false") == 0)  { i += 5; out = Json(false); return true; }
        if (c == 'n' && s.compare(i, 4, "null")  == 0)  { i += 4; out = Json();      return true; }
        err = std::string("unexpected char '") + c + "' at offset " + std::to_string(i);
        return false;
    }

    bool parseArray(Json& out) {
        ++i; // skip '['
        out = Json::array();
        skipWs();
        if (i < s.size() && s[i] == ']') { ++i; return true; }
        while (true) {
            Json v;
            if (!parseValue(v)) return false;
            out.push_back(std::move(v));
            skipWs();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == ']') { ++i; return true; }
            err = "expected , or ] in array";
            return false;
        }
    }

    bool parseObject(Json& out) {
        ++i; // skip '{'
        out = Json::object();
        skipWs();
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        while (true) {
            skipWs();
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (!expect(':')) return false;
            Json v;
            if (!parseValue(v)) return false;
            out[key] = std::move(v);
            skipWs();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == '}') { ++i; return true; }
            err = "expected , or } in object";
            return false;
        }
    }
};

} // unnamed

bool Json::parse(const std::string& text, Json& out, std::string& error)
{
    Parser p(text);
    p.skipWs();
    if (!p.parseValue(out)) {
        error = p.err.empty() ? "parse error" : p.err;
        out = Json();
        return false;
    }
    p.skipWs();
    if (p.i != text.size()) {
        error = "trailing data at offset " + std::to_string(p.i);
        // Not fatal for most admin callers; still clear and return false.
        return false;
    }
    return true;
}

} // namespace Devtools
} // namespace EvE

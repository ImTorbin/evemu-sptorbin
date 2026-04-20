/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Minimal, inline JSON value type / parser / serializer used by the remote DevTools
    admin API.  Intentionally dependency-free so the build does not need an extra
    third-party library.  Performance is "good enough for an admin API used by one or
    two operators at a time" - do not use this to parse game data.
    ------------------------------------------------------------------------------------
*/

#ifndef __DEVTOOLS__DEVTOOLS_JSON_H__INCL__
#define __DEVTOOLS__DEVTOOLS_JSON_H__INCL__

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace EvE {
namespace Devtools {

class Json
{
public:
    enum class Type {
        Null,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object
    };

    Json();
    Json(std::nullptr_t);
    Json(bool v);
    Json(int v);
    Json(int64_t v);
    Json(uint32_t v);
    Json(uint64_t v);
    Json(double v);
    Json(const char* v);
    Json(const std::string& v);

    static Json array();
    static Json object();

    Type type() const { return m_type; }

    bool isNull()   const { return m_type == Type::Null; }
    bool isBool()   const { return m_type == Type::Bool; }
    bool isInt()    const { return m_type == Type::Int; }
    bool isDouble() const { return m_type == Type::Double; }
    bool isNumber() const { return m_type == Type::Int || m_type == Type::Double; }
    bool isString() const { return m_type == Type::String; }
    bool isArray()  const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }

    bool          asBool(bool def = false) const;
    int64_t       asInt(int64_t def = 0) const;
    double        asDouble(double def = 0.0) const;
    const std::string& asString(const std::string& def = "") const;

    // Object helpers.
    bool has(const std::string& key) const;
    const Json& get(const std::string& key) const;
    Json& operator[](const std::string& key);
    void set(const std::string& key, Json value);

    // Array helpers.
    size_t size() const;
    const Json& at(size_t i) const;
    Json& operator[](size_t i);
    void push_back(Json value);

    // Serialize.  pretty=true produces 2-space indentation for debugging.
    std::string dump(bool pretty = false) const;

    // Parse from a string.  Returns true on success; on failure, `error` gets a message
    // and the value is left as Null.
    static bool parse(const std::string& text, Json& out, std::string& error);

private:
    Type m_type;
    bool m_bool;
    int64_t m_int;
    double m_double;
    std::string m_string;
    std::vector<Json> m_array;
    std::map<std::string, Json> m_object;

    static const Json s_null;

    void dumpTo(std::string& out, bool pretty, int depth) const;
    static void writeEscaped(std::string& out, const std::string& s);
    static void writeIndent(std::string& out, int depth);
};

} // namespace Devtools
} // namespace EvE

#endif // __DEVTOOLS__DEVTOOLS_JSON_H__INCL__

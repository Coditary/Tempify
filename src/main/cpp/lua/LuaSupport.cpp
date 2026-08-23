#include "LuaEngineInternal.h"
#include "tempify/support/Errors.h"
#include "tempify/support/Slug.h"

#include <algorithm>
#include <cctype>
#include <random>
#include <sstream>
#include <string_view>
#include <utility>

namespace tempify::lua_internal {

namespace {

std::string lua_type_name(lua_State *state, const int index) {
    return lua_typename(state, lua_type(state, index));
}

int lua_slugify(lua_State *state) {
    const char *input = luaL_checkstring(state, 1);
    const std::string output = slugify(input);
    lua_pushlstring(state, output.c_str(), output.size());
    return 1;
}

std::string generate_token(const std::size_t length) {
    static constexpr std::string_view alphabet = "abcdefghijklmnopqrstuvwxyz0123456789";
    thread_local std::mt19937 engine(std::random_device{}());
    std::uniform_int_distribution<std::size_t> distribution(0, alphabet.size() - 1);

    std::string token;
    token.reserve(length);
    for (std::size_t index = 0; index < length; ++index) {
        token.push_back(alphabet[distribution(engine)]);
    }
    return token;
}

int lua_generate_token(lua_State *state) {
    const auto length = static_cast<std::size_t>(luaL_optinteger(state, 1, 16));
    const std::string token = generate_token(length);
    lua_pushlstring(state, token.c_str(), token.size());
    return 1;
}

void push_string_map(lua_State *state, const std::map<std::string, std::string> &values) {
    lua_createtable(state, 0, static_cast<int>(values.size()));
    for (const auto &[key, value] : values) {
        lua_pushlstring(state, value.c_str(), value.size());
        lua_setfield(state, -2, key.c_str());
    }
}

} // namespace

[[noreturn]] void throw_lua_error(const std::filesystem::path &path, const std::string &message) {
    throw TempifyError("Lua error in " + path.string() + ": " + message);
}

std::string value_to_string(lua_State *state, const int index, const std::filesystem::path &path) {
    if (lua_isstring(state, index) != 0) {
        return lua_tostring(state, index);
    }
    if (lua_isboolean(state, index) != 0) {
        return lua_toboolean(state, index) != 0 ? "true" : "false";
    }
    if (lua_isinteger(state, index) != 0) {
        return std::to_string(lua_tointeger(state, index));
    }
    if (lua_isnumber(state, index) != 0) {
        std::ostringstream stream;
        stream << lua_tonumber(state, index);
        return stream.str();
    }
    if (lua_isnil(state, index) != 0) {
        return {};
    }

    throw_lua_error(path, "Expected scalar value, got " + lua_type_name(state, index));
}

std::string trim_copy(std::string value) {
    const auto is_space = [](const unsigned char ch) { return std::isspace(ch) != 0; };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool string_truthy(std::string value) {
    value = lower_copy(trim_copy(std::move(value)));
    return !(value.empty() || value == "false" || value == "0" || value == "no" || value == "off");
}

std::optional<std::string> optional_string_field(lua_State *state, const int table_index, const char *key,
                                                 const std::filesystem::path &path) {
    lua_getfield(state, table_index, key);
    if (lua_isnil(state, -1) != 0) {
        lua_pop(state, 1);
        return std::nullopt;
    }

    const std::string value = value_to_string(state, -1, path);
    lua_pop(state, 1);
    return value;
}

std::string required_string_field(lua_State *state, const int table_index, const char *key,
                                  const std::filesystem::path &path) {
    const auto value = optional_string_field(state, table_index, key, path);
    if (!value.has_value() || value->empty()) {
        throw_lua_error(path, std::string("Missing required field '") + key + "'");
    }
    return *value;
}

bool optional_bool_field(lua_State *state, const int table_index, const char *key, const bool default_value,
                         const std::filesystem::path &path) {
    lua_getfield(state, table_index, key);
    if (lua_isnil(state, -1) != 0) {
        lua_pop(state, 1);
        return default_value;
    }
    if (lua_isboolean(state, -1) == 0) {
        throw_lua_error(path, std::string("Field '") + key + "' must be boolean");
    }
    const bool value = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    return value;
}

std::vector<std::string> string_list_from_stack(lua_State *state, const int table_index,
                                                const std::filesystem::path &path) {
    std::vector<std::string> values;
    const std::size_t count = lua_rawlen(state, table_index);
    values.reserve(count);

    for (std::size_t index = 1; index <= count; ++index) {
        lua_rawgeti(state, table_index, static_cast<lua_Integer>(index));
        values.push_back(value_to_string(state, -1, path));
        lua_pop(state, 1);
    }

    return values;
}

std::vector<std::string> optional_string_list_field(lua_State *state, const int table_index, const char *key,
                                                    const std::filesystem::path &path) {
    lua_getfield(state, table_index, key);
    if (lua_isnil(state, -1) != 0) {
        lua_pop(state, 1);
        return {};
    }
    if (lua_istable(state, -1) == 0) {
        throw_lua_error(path, std::string("Field '") + key + "' must be list");
    }

    const auto values = string_list_from_stack(state, lua_gettop(state), path);
    lua_pop(state, 1);
    return values;
}

std::map<std::string, std::string> string_map_from_stack(lua_State *state, const int table_index,
                                                         const std::filesystem::path &path) {
    std::map<std::string, std::string> values;
    lua_pushnil(state);
    while (lua_next(state, table_index) != 0) {
        if (lua_isstring(state, -2) == 0) {
            lua_pop(state, 1);
            continue;
        }
        values.emplace(lua_tostring(state, -2), value_to_string(state, -1, path));
        lua_pop(state, 1);
    }
    return values;
}

LuaStatePtr make_state() {
    LuaStatePtr state(luaL_newstate(), &lua_close);
    if (state == nullptr) {
        throw TempifyError("Failed to initialize Lua state");
    }

    luaL_requiref(state.get(), "_G", luaopen_base, 1);
    lua_pop(state.get(), 1);
    luaL_requiref(state.get(), LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(state.get(), 1);
    luaL_requiref(state.get(), LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(state.get(), 1);
    luaL_requiref(state.get(), LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state.get(), 1);
    luaL_requiref(state.get(), LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(state.get(), 1);

    return state;
}

void register_metadata_helpers(lua_State *state) {
    lua_pushcfunction(state, lua_slugify);
    lua_setglobal(state, "slugify");

    lua_pushcfunction(state, lua_generate_token);
    lua_setglobal(state, "generate_token");
}

void load_file_result(lua_State *state, const std::filesystem::path &path) {
    const std::string path_text = path.string();
    if (luaL_loadfile(state, path_text.c_str()) != LUA_OK) {
        const std::string message = lua_tostring(state, -1);
        throw_lua_error(path, message);
    }

    if (lua_pcall(state, 0, 1, 0) != LUA_OK) {
        const std::string message = lua_tostring(state, -1);
        throw_lua_error(path, message);
    }
}

std::map<std::string, std::string> table_to_string_map(lua_State *state, const int index) {
    std::map<std::string, std::string> values;
    if (lua_istable(state, index) == 0) {
        return values;
    }

    const int absolute_index = lua_absindex(state, index);
    lua_pushnil(state);
    while (lua_next(state, absolute_index) != 0) {
        if (lua_isstring(state, -2) != 0) {
            std::filesystem::path fake_path = "<lua-table>";
            values.emplace(lua_tostring(state, -2), value_to_string(state, -1, fake_path));
        }
        lua_pop(state, 1);
    }
    return values;
}

void push_context_table(lua_State *state, const std::map<std::string, std::string> &values,
                        const std::filesystem::path &template_root, const std::filesystem::path &build_root,
                        const std::optional<std::string> &candidate_value) {
    lua_createtable(state, 0, 4);

    push_string_map(state, values);
    lua_setfield(state, -2, "values");

    const std::string template_root_text = template_root.string();
    lua_pushlstring(state, template_root_text.c_str(), template_root_text.size());
    lua_setfield(state, -2, "template_root");

    const std::string build_root_text = build_root.string();
    lua_pushlstring(state, build_root_text.c_str(), build_root_text.size());
    lua_setfield(state, -2, "build_root");

    if (candidate_value.has_value()) {
        lua_pushlstring(state, candidate_value->c_str(), candidate_value->size());
        lua_setfield(state, -2, "value");
    }
}

} // namespace tempify::lua_internal

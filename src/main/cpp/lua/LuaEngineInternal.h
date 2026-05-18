#pragma once

#include "tempify/domain/BuildPlan.h"
#include "tempify/domain/TemplateManifest.h"

#include "runtime/LuaHeaders.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tempify::lua_internal {

using LuaStatePtr = std::unique_ptr<lua_State, decltype(&lua_close)>;

[[noreturn]] void throw_lua_error(const std::filesystem::path& path, const std::string& message);
std::string value_to_string(lua_State* state, int index, const std::filesystem::path& path);
std::string trim_copy(std::string value);
std::string lower_copy(std::string value);
bool string_truthy(std::string value);

std::optional<std::string> optional_string_field(lua_State* state,
                                                 int table_index,
                                                 const char* key,
                                                 const std::filesystem::path& path);
std::string required_string_field(lua_State* state,
                                  int table_index,
                                  const char* key,
                                  const std::filesystem::path& path);
bool optional_bool_field(lua_State* state,
                         int table_index,
                         const char* key,
                         bool default_value,
                         const std::filesystem::path& path);
std::vector<std::string> string_list_from_stack(lua_State* state,
                                                int table_index,
                                                const std::filesystem::path& path);
std::vector<std::string> optional_string_list_field(lua_State* state,
                                                    int table_index,
                                                    const char* key,
                                                    const std::filesystem::path& path);
std::map<std::string, std::string> string_map_from_stack(lua_State* state,
                                                         int table_index,
                                                         const std::filesystem::path& path);

LuaStatePtr make_state();
void register_metadata_helpers(lua_State* state);
void load_file_result(lua_State* state, const std::filesystem::path& path);
std::map<std::string, std::string> table_to_string_map(lua_State* state, int index);
void push_context_table(lua_State* state,
                        const std::map<std::string, std::string>& values,
                        const std::filesystem::path& template_root = {},
                        const std::filesystem::path& build_root = {},
                        const std::optional<std::string>& candidate_value = std::nullopt);

}

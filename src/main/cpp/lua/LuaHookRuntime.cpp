#include "LuaEngineInternal.h"
#include "tempify/lua/LuaEngine.h"
#include "tempify/prebyte/PrebyteRenderer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tempify {

namespace {

struct HookHost {
    const TemplateManifest *manifest = nullptr;
    const BuildContext *context = nullptr;
    const PrebyteRenderer *renderer = nullptr;
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::optional<std::chrono::milliseconds> timeout;
    mutable std::string last_error;
};

int report_hook_failure(lua_State *state, const HookHost &host) {
    return luaL_error(state, "%s", host.last_error.c_str());
}

constexpr char kHookHostRegistryKey = '\0';

HookHost *hook_host(lua_State *state) {
    return static_cast<HookHost *>(lua_touserdata(state, lua_upvalueindex(1)));
}

HookHost *registered_hook_host(lua_State *state) {
    lua_pushlightuserdata(state, const_cast<char *>(&kHookHostRegistryKey));
    lua_rawget(state, LUA_REGISTRYINDEX);
    HookHost *host = static_cast<HookHost *>(lua_touserdata(state, -1));
    lua_pop(state, 1);
    return host;
}

void register_hook_host(lua_State *state, HookHost *host) {
    lua_pushlightuserdata(state, const_cast<char *>(&kHookHostRegistryKey));
    lua_pushlightuserdata(state, host);
    lua_rawset(state, LUA_REGISTRYINDEX);
}

std::optional<std::chrono::milliseconds> remaining_timeout(const HookHost &host) {
    if (!host.deadline.has_value()) {
        return std::nullopt;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now >= *host.deadline) {
        return std::chrono::milliseconds(0);
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(*host.deadline - now);
}

bool hook_timed_out(const HookHost &host) {
    return host.deadline.has_value() && std::chrono::steady_clock::now() >= *host.deadline;
}

int timeout_message_ms(const HookHost &host) {
    return static_cast<int>(host.timeout.value_or(std::chrono::milliseconds(0)).count());
}

void hook_timeout_guard(lua_State *state, lua_Debug *debug) {
    static_cast<void>(debug);
    HookHost *host = registered_hook_host(state);
    if (host == nullptr || !hook_timed_out(*host)) {
        return;
    }

    luaL_error(state, "Hook timed out after %d ms", timeout_message_ms(*host));
}

bool path_starts_with(const std::filesystem::path &path, const std::filesystem::path &prefix) {
    const std::filesystem::path normalized_path = path.lexically_normal();
    const std::filesystem::path normalized_prefix = prefix.lexically_normal();
    auto path_it = normalized_path.begin();
    auto prefix_it = normalized_prefix.begin();
    for (; prefix_it != normalized_prefix.end(); ++prefix_it, ++path_it) {
        if (path_it == normalized_path.end()) {
            return false;
        }
#if defined(_WIN32)
        std::string path_part = path_it->string();
        std::string prefix_part = prefix_it->string();
        std::transform(path_part.begin(), path_part.end(), path_part.begin(),
                       [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::transform(prefix_part.begin(), prefix_part.end(), prefix_part.begin(),
                       [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (path_part != prefix_part) {
            return false;
        }
#else
        if (*path_it != *prefix_it) {
            return false;
        }
#endif
    }
    return true;
}

bool path_within_root(const std::filesystem::path &path, const std::filesystem::path &root) {
    std::error_code error;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(path, error);
    const std::filesystem::path resolved_root = std::filesystem::weakly_canonical(root, error);
    if (!error) {
        return path_starts_with(resolved, resolved_root);
    }
    return path_starts_with(path.lexically_normal(), root.lexically_normal());
}

void set_hook_error(HookHost &host, std::string message) {
    host.last_error = std::move(message);
}

bool relative_path_has_parent_segments(const std::filesystem::path &path) {
    for (const auto &part : path.lexically_normal()) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

bool try_resolve_input_path(const HookHost &host, const char *raw_path, std::filesystem::path &resolved) {
    if (raw_path == nullptr || raw_path[0] == '\0') {
        set_hook_error(const_cast<HookHost &>(host), "Hook path must be non-empty");
        return false;
    }

    const std::filesystem::path path(raw_path);
    if (path.is_absolute()) {
        const std::filesystem::path normalized = path.lexically_normal();
        if (path_within_root(normalized, host.context->build_root) ||
            path_within_root(normalized, host.manifest->root)) {
            resolved = normalized;
            return true;
        }
        set_hook_error(const_cast<HookHost &>(host), "Hook path escapes allowed roots: " + path.string());
        return false;
    }

    if (relative_path_has_parent_segments(path)) {
        set_hook_error(const_cast<HookHost &>(host), "Hook path escapes allowed roots: " + path.string());
        return false;
    }

    const std::filesystem::path build_candidate = (host.context->build_root / path).lexically_normal();
    if (!path_within_root(build_candidate, host.context->build_root)) {
        set_hook_error(const_cast<HookHost &>(host), "Hook path escapes build root: " + path.string());
        return false;
    }
    if (std::filesystem::exists(build_candidate)) {
        resolved = build_candidate;
        return true;
    }

    const std::filesystem::path template_candidate = (host.manifest->root / path).lexically_normal();
    if (!path_within_root(template_candidate, host.manifest->root)) {
        set_hook_error(const_cast<HookHost &>(host), "Hook path escapes template root: " + path.string());
        return false;
    }
    if (std::filesystem::exists(template_candidate)) {
        resolved = template_candidate;
        return true;
    }

    resolved = build_candidate;
    return true;
}

bool try_resolve_output_path(const HookHost &host, const char *raw_path, std::filesystem::path &resolved) {
    if (raw_path == nullptr || raw_path[0] == '\0') {
        set_hook_error(const_cast<HookHost &>(host), "Hook path must be non-empty");
        return false;
    }

    const std::filesystem::path path(raw_path);
    if (!path.is_absolute() && relative_path_has_parent_segments(path)) {
        set_hook_error(const_cast<HookHost &>(host), "Hook path escapes allowed roots: " + path.string());
        return false;
    }

    resolved = path.is_absolute() ? path.lexically_normal() : (host.context->build_root / path).lexically_normal();
    if (!path_within_root(resolved, host.context->build_root)) {
        set_hook_error(const_cast<HookHost &>(host), "Hook path escapes build root: " + path.string());
        return false;
    }

    return true;
}

bool mkdir_impl(HookHost &host, const char *raw_path, std::string &created_path) {
    std::filesystem::path path;
    if (!try_resolve_output_path(host, raw_path, path)) {
        return false;
    }
    std::filesystem::create_directories(path);
    created_path = path.string();
    return true;
}

bool remove_impl(HookHost &host, const char *raw_path, std::uintmax_t &removed_count) {
    std::filesystem::path path;
    if (!try_resolve_output_path(host, raw_path, path)) {
        return false;
    }
    removed_count = std::filesystem::remove_all(path);
    return true;
}

bool exists_impl(HookHost &host, const char *raw_path, bool &exists) {
    std::filesystem::path path;
    if (!try_resolve_input_path(host, raw_path, path)) {
        return false;
    }
    exists = std::filesystem::exists(path);
    return true;
}

bool write_file_impl(HookHost &host, const char *raw_path, const char *content, std::string &written_path) {
    std::filesystem::path path;
    if (!try_resolve_output_path(host, raw_path, path)) {
        return false;
    }

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        set_hook_error(host, "Could not write file: " + path.string());
        return false;
    }
    output << content;
    written_path = path.string();
    return true;
}

bool read_file_impl(HookHost &host, const char *raw_path, std::string &content) {
    std::filesystem::path path;
    if (!try_resolve_input_path(host, raw_path, path)) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        set_hook_error(host, "Could not read file: " + path.string());
        return false;
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    content = stream.str();
    return true;
}

bool copy_impl(HookHost &host, const char *raw_source, const char *raw_target, std::string &target_path) {
    std::filesystem::path source;
    std::filesystem::path target;
    if (!try_resolve_input_path(host, raw_source, source)) {
        return false;
    }
    if (!try_resolve_output_path(host, raw_target, target)) {
        return false;
    }

    std::filesystem::create_directories(target.parent_path());
    if (std::filesystem::is_directory(source)) {
        std::filesystem::copy(source, target,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
    } else {
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing);
    }

    target_path = target.string();
    return true;
}

bool process_file_impl(HookHost &host, const char *raw_input, const char *raw_output,
                       const std::map<std::string, std::string> &overrides, std::string &output_path) {
    std::filesystem::path input;
    std::filesystem::path output;
    if (!try_resolve_input_path(host, raw_input, input)) {
        return false;
    }
    if (!try_resolve_output_path(host, raw_output, output)) {
        return false;
    }

    std::map<std::string, std::string> values = host.context->values;
    values.insert(overrides.begin(), overrides.end());

    std::filesystem::create_directories(output.parent_path());
    prebyte::Prebyte engine;
    host.renderer->configure(engine, values, *host.manifest);
    host.renderer->render_file(engine, input, output);
    output_path = output.string();
    return true;
}

int hook_exists(lua_State *state) {
    HookHost &host = *hook_host(state);
    bool exists = false;
    if (!exists_impl(host, luaL_checkstring(state, 1), exists)) {
        return report_hook_failure(state, host);
    }
    lua_pushboolean(state, exists);
    return 1;
}

int hook_mkdir(lua_State *state) {
    HookHost &host = *hook_host(state);
    std::string created_path;
    if (!mkdir_impl(host, luaL_checkstring(state, 1), created_path)) {
        return report_hook_failure(state, host);
    }
    lua_pushlstring(state, created_path.c_str(), created_path.size());
    return 1;
}

int hook_remove(lua_State *state) {
    HookHost &host = *hook_host(state);
    std::uintmax_t removed_count = 0;
    if (!remove_impl(host, luaL_checkstring(state, 1), removed_count)) {
        return report_hook_failure(state, host);
    }
    lua_pushinteger(state, static_cast<lua_Integer>(removed_count));
    return 1;
}

int hook_read_file(lua_State *state) {
    HookHost &host = *hook_host(state);
    std::string content;
    if (!read_file_impl(host, luaL_checkstring(state, 1), content)) {
        return report_hook_failure(state, host);
    }
    lua_pushlstring(state, content.c_str(), content.size());
    return 1;
}

int hook_write_file(lua_State *state) {
    HookHost &host = *hook_host(state);
    std::string written_path;
    if (!write_file_impl(host, luaL_checkstring(state, 1), luaL_checkstring(state, 2), written_path)) {
        return report_hook_failure(state, host);
    }
    lua_pushlstring(state, written_path.c_str(), written_path.size());
    return 1;
}

int hook_list_files(lua_State *state) {
    HookHost &host = *hook_host(state);
    std::filesystem::path path;
    if (!try_resolve_input_path(host, luaL_checkstring(state, 1), path)) {
        return report_hook_failure(state, host);
    }
    lua_newtable(state);
    if (!std::filesystem::is_directory(path)) {
        return 1;
    }
    int lua_index = 1;
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string value = entry.path().filename().string();
        lua_pushlstring(state, value.c_str(), value.size());
        lua_rawseti(state, -2, lua_index++);
    }
    return 1;
}

int hook_list_dirs(lua_State *state) {
    HookHost &host = *hook_host(state);
    std::filesystem::path path;
    if (!try_resolve_input_path(host, luaL_checkstring(state, 1), path)) {
        return report_hook_failure(state, host);
    }
    lua_newtable(state);
    if (!std::filesystem::is_directory(path)) {
        return 1;
    }
    int lua_index = 1;
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string value = entry.path().filename().string();
        lua_pushlstring(state, value.c_str(), value.size());
        lua_rawseti(state, -2, lua_index++);
    }
    return 1;
}

int hook_copy(lua_State *state) {
    HookHost &host = *hook_host(state);
    std::string target_path;
    if (!copy_impl(host, luaL_checkstring(state, 1), luaL_checkstring(state, 2), target_path)) {
        return report_hook_failure(state, host);
    }
    lua_pushlstring(state, target_path.c_str(), target_path.size());
    return 1;
}

int hook_process_string(lua_State *state) {
    const HookHost &host = *hook_host(state);
    std::map<std::string, std::string> values = host.context->values;
    if (lua_istable(state, 2) != 0) {
        const auto overrides = lua_internal::table_to_string_map(state, 2);
        values.insert(overrides.begin(), overrides.end());
        for (const auto &[key, value] : overrides) {
            values[key] = value;
        }
    }

    prebyte::Prebyte engine;
    host.renderer->configure(engine, values, *host.manifest);
    const std::string output = host.renderer->render_string(engine, luaL_checkstring(state, 1));
    lua_pushlstring(state, output.c_str(), output.size());
    return 1;
}

int hook_process_file(lua_State *state) {
    HookHost &host = *hook_host(state);
    std::map<std::string, std::string> overrides;
    if (lua_istable(state, 3) != 0) {
        overrides = lua_internal::table_to_string_map(state, 3);
    }

    std::string output_path;
    if (!process_file_impl(host, luaL_checkstring(state, 1), luaL_checkstring(state, 2), overrides, output_path)) {
        return report_hook_failure(state, host);
    }
    lua_pushlstring(state, output_path.c_str(), output_path.size());
    return 1;
}

int hook_exec(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const char *command = luaL_checkstring(state, 1);

#if !defined(_WIN32)
    pid_t child = fork();
    if (child < 0) {
        return luaL_error(state, "Could not start process: %s", command);
    }

    if (child == 0) {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", command, static_cast<char *>(nullptr));
        _exit(127);
    }

    int status = 0;
    if (!host.deadline.has_value()) {
        if (waitpid(child, &status, 0) < 0) {
            return luaL_error(state, "Could not wait for process: %s", command);
        }
        lua_pushinteger(state, status);
        return 1;
    }

    while (true) {
        const pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
            lua_pushinteger(state, status);
            return 1;
        }
        if (result < 0) {
            return luaL_error(state, "Could not wait for process: %s", command);
        }
        if (hook_timed_out(host)) {
            kill(-child, SIGKILL);
            waitpid(child, &status, 0);
            return luaL_error(state, "Hook timed out after %d ms while running command: %s", timeout_message_ms(host),
                              command);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
#else
    const std::string shell = [] {
        if (const char *comspec = std::getenv("ComSpec"); comspec != nullptr && comspec[0] != '\0') {
            return std::string(comspec);
        }
        return std::string("cmd.exe");
    }();
    std::string command_line = '"' + shell + "\" /d /c " + command;

    STARTUPINFOA startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};
    if (!CreateProcessA(shell.c_str(), command_line.data(), nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP, nullptr,
                        nullptr, &startup_info, &process_info)) {
        return luaL_error(state, "Could not start process: %s", command);
    }

    CloseHandle(process_info.hThread);

    DWORD wait_ms = INFINITE;
    if (const auto timeout = remaining_timeout(host)) {
        const auto timeout_count = timeout->count();
        if (timeout_count <= 0) {
            wait_ms = 0;
        } else if (timeout_count >= static_cast<long long>(std::numeric_limits<DWORD>::max() - 1)) {
            wait_ms = std::numeric_limits<DWORD>::max() - 1;
        } else {
            wait_ms = static_cast<DWORD>(timeout_count);
        }
    }

    const DWORD wait_result = WaitForSingleObject(process_info.hProcess, wait_ms);
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(process_info.hProcess, 1);
        WaitForSingleObject(process_info.hProcess, INFINITE);
        CloseHandle(process_info.hProcess);
        return luaL_error(state, "Hook timed out after %d ms while running command: %s", timeout_message_ms(host),
                          command);
    }

    if (wait_result != WAIT_OBJECT_0) {
        CloseHandle(process_info.hProcess);
        return luaL_error(state, "Could not wait for process: %s", command);
    }

    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process_info.hProcess, &exit_code)) {
        CloseHandle(process_info.hProcess);
        return luaL_error(state, "Could not wait for process: %s", command);
    }

    CloseHandle(process_info.hProcess);
    lua_pushinteger(state, static_cast<lua_Integer>(exit_code));
    return 1;
#endif
}

int hook_get_template_root(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const std::string value = host.manifest->root.string();
    lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int hook_get_build_root(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const std::string value = host.context->build_root.string();
    lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

void register_hook_function(lua_State *state, const char *name, lua_CFunction function, HookHost *host) {
    lua_pushlightuserdata(state, host);
    lua_pushcclosure(state, function, 1);
    lua_setglobal(state, name);
}

void register_hook_helpers(lua_State *state, HookHost *host) {
    lua_internal::register_metadata_helpers(state);

    register_hook_function(state, "copy", hook_copy, host);
    register_hook_function(state, "exists", hook_exists, host);
    register_hook_function(state, "mkdir", hook_mkdir, host);
    register_hook_function(state, "remove", hook_remove, host);
    register_hook_function(state, "read_file", hook_read_file, host);
    register_hook_function(state, "write_file", hook_write_file, host);
    register_hook_function(state, "list_files", hook_list_files, host);
    register_hook_function(state, "list_dirs", hook_list_dirs, host);
    register_hook_function(state, "process_string", hook_process_string, host);
    register_hook_function(state, "process_file", hook_process_file, host);
    register_hook_function(state, "exec", hook_exec, host);
    register_hook_function(state, "get_template_root", hook_get_template_root, host);
    register_hook_function(state, "get_build_root", hook_get_build_root, host);
    register_hook_function(
        state, "script",
        [](lua_State *inner_state) -> int {
            const HookHost &inner_host = *hook_host(inner_state);
            const std::string name = luaL_checkstring(inner_state, 1);
            const auto it = std::ranges::find_if(inner_host.manifest->scripts,
                                                 [&](const ScriptCatalogEntry &entry) { return entry.name == name; });
            if (it == inner_host.manifest->scripts.end()) {
                return luaL_error(inner_state, "Unknown script: %s", name.c_str());
            }
            LuaEngine engine;
            engine.run_hook(it->path, *inner_host.manifest, *inner_host.context, *inner_host.renderer,
                            remaining_timeout(inner_host));
            return 0;
        },
        host);
}

} // namespace

void LuaEngine::run_hook(const std::filesystem::path &script_path, const TemplateManifest &manifest,
                         const BuildContext &context, const PrebyteRenderer &renderer,
                         const std::optional<std::chrono::milliseconds> timeout) const {
    auto state = lua_internal::make_state();
    HookHost host{&manifest, &context, &renderer};
    if (timeout.has_value()) {
        host.timeout = *timeout;
        host.deadline = std::chrono::steady_clock::now() + *timeout;
    }
    register_hook_host(state.get(), &host);
    register_hook_helpers(state.get(), &host);
    lua_internal::push_context_table(state.get(), context.values, context.template_root, context.build_root);
    lua_setglobal(state.get(), "ctx");
    if (timeout.has_value()) {
        lua_sethook(state.get(), hook_timeout_guard, LUA_MASKCOUNT, 1000);
    }

    const std::string script_path_text = script_path.string();
    if (luaL_loadfile(state.get(), script_path_text.c_str()) != LUA_OK) {
        const std::string message = lua_tostring(state.get(), -1);
        lua_internal::throw_lua_error(script_path, message);
    }

    if (lua_pcall(state.get(), 0, 0, 0) != LUA_OK) {
        const std::string message = lua_tostring(state.get(), -1);
        lua_internal::throw_lua_error(script_path, message);
    }
}

} // namespace tempify

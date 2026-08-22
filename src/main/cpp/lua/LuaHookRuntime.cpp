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
};

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

std::filesystem::path resolve_input_path(const HookHost &host, const std::filesystem::path &path) {
    if (path.is_absolute()) {
        return path;
    }

    const std::filesystem::path build_candidate = host.context->build_root / path;
    if (std::filesystem::exists(build_candidate)) {
        return build_candidate;
    }

    const std::filesystem::path template_candidate = host.manifest->root / path;
    if (std::filesystem::exists(template_candidate)) {
        return template_candidate;
    }

    return build_candidate;
}

std::filesystem::path resolve_output_path(const HookHost &host, const std::filesystem::path &path) {
    if (path.is_absolute()) {
        return path;
    }
    return host.context->build_root / path;
}

int hook_exists(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const std::filesystem::path path = resolve_input_path(host, luaL_checkstring(state, 1));
    lua_pushboolean(state, std::filesystem::exists(path));
    return 1;
}

int hook_mkdir(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const std::filesystem::path path = resolve_output_path(host, luaL_checkstring(state, 1));
    std::filesystem::create_directories(path);
    const std::string path_text = path.string();
    lua_pushlstring(state, path_text.c_str(), path_text.size());
    return 1;
}

int hook_remove(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const std::filesystem::path path = resolve_output_path(host, luaL_checkstring(state, 1));
    lua_pushinteger(state, static_cast<lua_Integer>(std::filesystem::remove_all(path)));
    return 1;
}

int hook_read_file(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const std::filesystem::path path = resolve_input_path(host, luaL_checkstring(state, 1));
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        const std::string path_text = path.string();
        return luaL_error(state, "Could not read file: %s", path_text.c_str());
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    const std::string content = stream.str();
    lua_pushlstring(state, content.c_str(), content.size());
    return 1;
}

int hook_write_file(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const std::filesystem::path path = resolve_output_path(host, luaL_checkstring(state, 1));
    const std::string content = luaL_checkstring(state, 2);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        const std::string path_text = path.string();
        return luaL_error(state, "Could not write file: %s", path_text.c_str());
    }
    output << content;
    const std::string path_text = path.string();
    lua_pushlstring(state, path_text.c_str(), path_text.size());
    return 1;
}

int hook_list_files(lua_State *state) {
    const HookHost &host = *hook_host(state);
    const std::filesystem::path path = resolve_input_path(host, luaL_checkstring(state, 1));
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
    const HookHost &host = *hook_host(state);
    const std::filesystem::path path = resolve_input_path(host, luaL_checkstring(state, 1));
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
    const HookHost &host = *hook_host(state);
    const std::filesystem::path source = resolve_input_path(host, luaL_checkstring(state, 1));
    const std::filesystem::path target = resolve_output_path(host, luaL_checkstring(state, 2));

    std::filesystem::create_directories(target.parent_path());
    if (std::filesystem::is_directory(source)) {
        std::filesystem::copy(source, target,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
    } else {
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing);
    }

    const std::string target_text = target.string();
    lua_pushlstring(state, target_text.c_str(), target_text.size());
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
    const HookHost &host = *hook_host(state);
    const std::filesystem::path input = resolve_input_path(host, luaL_checkstring(state, 1));
    const std::filesystem::path output = resolve_output_path(host, luaL_checkstring(state, 2));
    std::map<std::string, std::string> values = host.context->values;
    if (lua_istable(state, 3) != 0) {
        const auto overrides = lua_internal::table_to_string_map(state, 3);
        for (const auto &[key, value] : overrides) {
            values[key] = value;
        }
    }

    std::filesystem::create_directories(output.parent_path());
    prebyte::Prebyte engine;
    host.renderer->configure(engine, values, *host.manifest);
    host.renderer->render_file(engine, input, output);
    const std::string output_text = output.string();
    lua_pushlstring(state, output_text.c_str(), output_text.size());
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

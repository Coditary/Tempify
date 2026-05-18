local M = {}

local PLUGIN_OWNER_MARKER_NAME = ".tempify-package-owner.json"
local SYMLINK_OWNER_MARKER_NAME = ".tempify-symlink-owner.json"

function M.join_path(base, relative)
  if base == nil or base == "" or base == "." then
    return relative
  end
  if relative == nil or relative == "" then
    return base
  end
  if base == "/" then
    return "/" .. relative
  end
  if base:sub(-1) == "/" then
    return base .. relative
  end
  return base .. "/" .. relative
end

function M.parent_dir(path)
  local parent = tostring(path):match("^(.*)/[^/]+$")
  if parent == nil or parent == "" then
    return "."
  end
  return parent
end

function M.shell_quote(value)
  return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

function M.home_dir()
  local home = os.getenv("HOME")
  if home == nil or home == "" then
    error("HOME is not set")
  end
  return home
end

function M.data_home()
  local data_home = os.getenv("XDG_DATA_HOME")
  if data_home ~= nil and data_home ~= "" then
    return data_home
  end
  return M.join_path(M.home_dir(), ".local/share")
end

function M.version_identity(metadata)
  return string.format("%s-%s+r%s", tostring(metadata.version), tostring(metadata.release), tostring(metadata.revision))
end

function M.package_identity(metadata)
  return string.format("%s@%s", tostring(metadata.name), M.version_identity(metadata))
end

function M.paths(context)
  local home = M.home_dir()
  local data_home = M.data_home()
  local local_root = M.join_path(home, ".local")
  local stable_bin_dir = M.join_path(local_root, "bin")
  local tempify_root = M.join_path(data_home, "tempify")
  local runtime_root = M.join_path(tempify_root, "runtime")
  local version_root = M.join_path(runtime_root, M.version_identity(context.metadata))
  local binary_path = M.join_path(version_root, "bin/tempify")
  local symlink_path = M.join_path(stable_bin_dir, "tempify")
  local symlink_marker_path = M.join_path(stable_bin_dir, SYMLINK_OWNER_MARKER_NAME)
  local plugin_root = M.join_path(M.join_path(data_home, "reqpack/plugins"), "tempify")
  local plugin_marker_path = M.join_path(plugin_root, PLUGIN_OWNER_MARKER_NAME)

  return {
    data_home = data_home,
    local_root = local_root,
    stable_bin_dir = stable_bin_dir,
    tempify_root = tempify_root,
    runtime_root = runtime_root,
    version_root = version_root,
    binary_path = binary_path,
    symlink_path = symlink_path,
    symlink_marker_path = symlink_marker_path,
    plugin_root = plugin_root,
    plugin_marker_path = plugin_marker_path,
  }
end

function M.load_payload_manifest(context)
  local ok, payload = pcall(dofile, M.join_path(context.paths.controlDir, "scripts/payload_files.lua"))
  if not ok then
    return nil, "failed to load payload manifest: " .. tostring(payload)
  end
  return payload
end

function M.ensure_directories(context, directories)
  for _, dir in ipairs(directories) do
    if not context.fs.exists(dir) then
      local ok, result = pcall(context.fs.mkdir, dir)
      if not ok or result == false then
        return false, "failed to create directory: " .. tostring(dir)
      end
    end
  end
  return true
end

function M.ensure_shell_directories(context, directories)
  for _, dir in ipairs(directories) do
    local result = context.exec.run("mkdir -p " .. M.shell_quote(dir))
    if not result.success then
      local message = result.stderr ~= "" and result.stderr or ("failed to create directory: " .. tostring(dir))
      return false, message
    end
  end
  return true
end

function M.update_symlink(context, source, target)
  local command = "ln -sfn " .. M.shell_quote(source) .. " " .. M.shell_quote(target)
  local result = context.exec.run(command)
  if not result.success then
    local message = result.stderr ~= "" and result.stderr or ("failed to create symlink: " .. tostring(target))
    return false, message
  end
  return true
end

function M.owner_marker_contents(context, paths)
  return table.concat({
    "{",
    '  "package": "' .. tostring(context.metadata.name):gsub('\\', '\\\\'):gsub('"', '\\"') .. '",',
    '  "version": "' .. tostring(context.metadata.version):gsub('\\', '\\\\'):gsub('"', '\\"') .. '",',
    '  "release": ' .. tostring(context.metadata.release) .. ',',
    '  "revision": ' .. tostring(context.metadata.revision) .. ',',
    '  "identity": "' .. M.package_identity(context.metadata):gsub('\\', '\\\\'):gsub('"', '\\"') .. '",',
    '  "runtimeRoot": "' .. tostring(paths.version_root):gsub('\\', '\\\\'):gsub('"', '\\"') .. '"',
    "}",
    "",
  }, "\n")
end

function M.write_owner_marker(context, path, content)
  local command = "printf %s " .. M.shell_quote(content) .. " > " .. M.shell_quote(path)
  local result = context.exec.run(command)
  if not result.success then
    local message = result.stderr ~= "" and result.stderr or ("failed to write owner marker: " .. tostring(path))
    return false, message
  end
  return true
end

function M.owner_marker_matches(context, marker_path)
  local expected_identity = '"identity": "' .. M.package_identity(context.metadata) .. '"'
  local expected_runtime_root = '"runtimeRoot": "' .. M.paths(context).version_root .. '"'
  local command =
    "test -f " .. M.shell_quote(marker_path) ..
    " && grep -F " .. M.shell_quote(expected_identity) .. " " .. M.shell_quote(marker_path) .. " >/dev/null" ..
    " && grep -F " .. M.shell_quote(expected_runtime_root) .. " " .. M.shell_quote(marker_path) .. " >/dev/null"
  local ok, _, code = os.execute(command)
  if ok == true then
    return true
  end
  return code == 0
end

function M.symlink_points_to_expected(path, expected)
  local ok, _, code = os.execute("test \"$(readlink " .. M.shell_quote(path) .. ")\" = " .. M.shell_quote(expected))
  if ok == true then
    return true
  end
  return code == 0
end

function M.remove_file(path)
  local ok, message = os.remove(path)
  if ok == nil and message ~= nil then
    return false, tostring(message)
  end
  return true
end

function M.remove_tree(path)
  local ok, _, code = os.execute("rm -rf " .. M.shell_quote(path))
  if ok == true then
    return true
  end
  return code == 0
end

return M

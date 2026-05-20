local layout = dofile(context.paths.controlDir .. "/scripts/layout.lua")

local payload, payload_error = layout.load_payload_manifest(context)
if payload == nil then
  context.tx.failed(payload_error)
  return false
end

local paths = layout.paths(context)
local ok, dir_error = layout.ensure_directories(context, {
  paths.tempify_root,
  paths.runtime_root,
  paths.version_root,
})
if not ok then
  context.tx.failed(dir_error)
  return false
end

local shared_ok, shared_error = layout.ensure_shell_directories(context, {
  paths.stable_bin_dir,
})
if not shared_ok then
  context.tx.failed(shared_error)
  return false
end

for _, dir in ipairs(payload.directories or {}) do
  local target_dir = layout.join_path(paths.version_root, dir)
  local mkdir_ok, mkdir_result = pcall(context.fs.mkdir, target_dir)
  if not mkdir_ok or mkdir_result == false then
    context.tx.failed("failed to prepare payload directory: " .. tostring(dir))
    return false
  end
end

for _, entry in ipairs(payload.files or {}) do
  local source = layout.join_path(context.paths.payloadDir, entry.path)
  local target = layout.join_path(paths.version_root, entry.path)

  if not context.fs.exists(source) then
    context.tx.failed("missing payload file: " .. tostring(entry.path))
    return false
  end

  local copy_ok, copy_result = pcall(context.fs.copy, source, target)
  if not copy_ok or copy_result == false then
    context.tx.failed("failed to copy payload file: " .. tostring(entry.path))
    return false
  end

  if entry.executable then
    local chmod_result = context.exec.run("chmod +x " .. layout.shell_quote(target))
    if not chmod_result.success then
      local message = chmod_result.stderr ~= "" and chmod_result.stderr or ("failed to mark executable: " .. tostring(entry.path))
      context.tx.failed(message)
      return false
    end
  end
end

local clean_plugin_result = context.exec.run("rm -rf " .. layout.shell_quote(paths.plugin_root) .. " && mkdir -p " .. layout.shell_quote(paths.plugin_root))
if not clean_plugin_result.success then
  local message = clean_plugin_result.stderr ~= "" and clean_plugin_result.stderr or "failed to prepare bundled plugin root"
  context.tx.failed(message)
  return false
end

local runtime_plugin_root = layout.join_path(paths.version_root, "share/reqpack/plugins/tempify")
for _, relative_path in ipairs({"metadata.json", "reqpack.lua", "run.lua", "default-template-repositories.json", "scripts/install.lua", "scripts/remove.lua"}) do
  local source = layout.join_path(runtime_plugin_root, relative_path)
  local target = layout.join_path(paths.plugin_root, relative_path)
  local parent_dir = layout.parent_dir(target)
  local copy_result = context.exec.run(
    "mkdir -p " .. layout.shell_quote(parent_dir) ..
    " && cp -f " .. layout.shell_quote(source) .. " " .. layout.shell_quote(target)
  )
  if not copy_result.success then
    context.tx.failed("failed to install bundled plugin file: " .. tostring(relative_path))
    return false
  end
end

local marker_content = layout.owner_marker_contents(context, paths)
local plugin_marker_ok, plugin_marker_error = layout.write_owner_marker(context, paths.plugin_marker_path, marker_content)
if not plugin_marker_ok then
  context.tx.failed(plugin_marker_error)
  return false
end

local symlink_marker_ok, symlink_marker_error = layout.write_owner_marker(context, paths.symlink_marker_path, marker_content)
if not symlink_marker_ok then
  context.tx.failed(symlink_marker_error)
  return false
end

local link_ok, link_error = layout.update_symlink(context, paths.binary_path, paths.symlink_path)
if not link_ok then
  context.tx.failed(link_error)
  return false
end

return true

local layout = dofile(context.paths.controlDir .. "/scripts/layout.lua")
local paths = layout.paths(context)

if layout.owner_marker_matches(context, paths.symlink_marker_path) then
  if layout.symlink_points_to_expected(paths.symlink_path, paths.binary_path) then
    local ok, error_message = layout.remove_file(paths.symlink_path)
    if not ok then
      context.log.error(error_message)
      return false
    end
  end

  local marker_ok, marker_error = layout.remove_file(paths.symlink_marker_path)
  if not marker_ok then
    context.log.error(marker_error)
    return false
  end
end

if layout.owner_marker_matches(context, paths.plugin_marker_path) then
  local ok = layout.remove_tree(paths.plugin_root)
  if not ok then
    context.log.error("failed to remove bundled plugin root")
    return false
  end
end

return true

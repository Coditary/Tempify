plugin = {}

local PLUGIN_NAME = "Tempify Template Manager"
local PLUGIN_VERSION = "0.1.2"
local GIT_BINARY = "git"
local STATE_SCHEMA_VERSION = 1

local function trim(value)
    return (tostring(value or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function starts_with(value, prefix)
    return tostring(value or ""):sub(1, #prefix) == prefix
end

local function ends_with(value, suffix)
    if suffix == "" then
        return true
    end
    local text = tostring(value or "")
    return text:sub(-#suffix) == suffix
end

local function lower(value)
    return string.lower(tostring(value or ""))
end

local function shell_quote(value)
    return "'" .. tostring(value or ""):gsub("'", "'\\''") .. "'"
end

local function split(value, separator, plain)
    local text = tostring(value or "")
    local parts = {}
    if separator == "" then
        parts[1] = text
        return parts
    end

    local start_index = 1
    while true do
        local from_index, to_index = string.find(text, separator, start_index, plain == true)
        if from_index == nil then
            table.insert(parts, string.sub(text, start_index))
            break
        end
        table.insert(parts, string.sub(text, start_index, from_index - 1))
        start_index = to_index + 1
    end
    return parts
end

local function join(values, separator)
    return table.concat(values or {}, separator or "")
end

local function dirname(path)
    local value = tostring(path or "")
    if value == "" then
        return "."
    end
    local index = value:match("^.*()/")
    if index == nil then
        return "."
    end
    if index == 1 then
        return "/"
    end
    return value:sub(1, index - 1)
end

local function join_path(base, child)
    local left = tostring(base or "")
    local right = tostring(child or "")
    if left == "" then
        return right
    end
    if right == "" then
        return left
    end
    if ends_with(left, "/") then
        return left .. right
    end
    return left .. "/" .. right
end

local function sanitize_name(value)
    local text = tostring(value or "")
    if text == "" then
        return "tempify"
    end
    return text:gsub("[^A-Za-z0-9._-]", "_")
end

local function is_array(value)
    if type(value) ~= "table" then
        return false
    end
    local count = 0
    for key, _ in pairs(value) do
        if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then
            return false
        end
        count = count + 1
    end
    for index = 1, count do
        if value[index] == nil then
            return false
        end
    end
    return true
end

local function sorted_keys(value)
    local keys = {}
    for key, _ in pairs(value or {}) do
        table.insert(keys, tostring(key))
    end
    table.sort(keys)
    return keys
end

local function json_escape(value)
    local text = tostring(value or "")
    text = text:gsub("\\", "\\\\")
    text = text:gsub('"', '\\"')
    text = text:gsub("\b", "\\b")
    text = text:gsub("\f", "\\f")
    text = text:gsub("\n", "\\n")
    text = text:gsub("\r", "\\r")
    text = text:gsub("\t", "\\t")
    return text
end

local function json_encode(value)
    local kind = type(value)
    if kind == "nil" then
        return "null"
    end
    if kind == "boolean" then
        return value and "true" or "false"
    end
    if kind == "number" then
        if value ~= value or value == math.huge or value == -math.huge then
            error("unsupported json number")
        end
        return tostring(value)
    end
    if kind == "string" then
        return '"' .. json_escape(value) .. '"'
    end
    if kind ~= "table" then
        error("unsupported json type: " .. kind)
    end

    if is_array(value) then
        local parts = {}
        for index = 1, #value do
            parts[index] = json_encode(value[index])
        end
        return "[" .. join(parts, ",") .. "]"
    end

    local parts = {}
    local keys = sorted_keys(value)
    for index = 1, #keys do
        local key = keys[index]
        parts[index] = json_encode(key) .. ":" .. json_encode(value[key])
    end
    return "{" .. join(parts, ",") .. "}"
end

local function json_decode(text)
    local input = tostring(text or "")
    local length = #input
    local index = 1

    local function decode_error(message)
        error("json parse error at byte " .. tostring(index) .. ": " .. message)
    end

    local function skip_whitespace()
        while index <= length do
            local char = input:sub(index, index)
            if char ~= " " and char ~= "\n" and char ~= "\r" and char ~= "\t" then
                break
            end
            index = index + 1
        end
    end

    local parse_value

    local function parse_string()
        if input:sub(index, index) ~= '"' then
            decode_error("expected string")
        end
        index = index + 1
        local parts = {}
        while index <= length do
            local char = input:sub(index, index)
            if char == '"' then
                index = index + 1
                return join(parts, "")
            end
            if char == "\\" then
                local escaped = input:sub(index + 1, index + 1)
                if escaped == '"' or escaped == "\\" or escaped == "/" then
                    table.insert(parts, escaped)
                    index = index + 2
                elseif escaped == "b" then
                    table.insert(parts, "\b")
                    index = index + 2
                elseif escaped == "f" then
                    table.insert(parts, "\f")
                    index = index + 2
                elseif escaped == "n" then
                    table.insert(parts, "\n")
                    index = index + 2
                elseif escaped == "r" then
                    table.insert(parts, "\r")
                    index = index + 2
                elseif escaped == "t" then
                    table.insert(parts, "\t")
                    index = index + 2
                elseif escaped == "u" then
                    local hex = input:sub(index + 2, index + 5)
                    if #hex ~= 4 or not hex:match("^[0-9A-Fa-f]+$") then
                        decode_error("invalid unicode escape")
                    end
                    local code = tonumber(hex, 16)
                    if code < 128 then
                        table.insert(parts, string.char(code))
                    else
                        -- Keep non-ASCII escaped payloads readable without depending on utf8 helpers.
                        table.insert(parts, "?")
                    end
                    index = index + 6
                else
                    decode_error("invalid escape")
                end
            else
                table.insert(parts, char)
                index = index + 1
            end
        end
        decode_error("unterminated string")
    end

    local function parse_number()
        local start_index = index
        local allowed = "-+0123456789.eE"
        while index <= length and allowed:find(input:sub(index, index), 1, true) ~= nil do
            index = index + 1
        end
        local raw = input:sub(start_index, index - 1)
        local value = tonumber(raw)
        if value == nil then
            decode_error("invalid number")
        end
        return value
    end

    local function parse_array()
        index = index + 1
        skip_whitespace()
        local items = {}
        if input:sub(index, index) == "]" then
            index = index + 1
            return items
        end
        while true do
            table.insert(items, parse_value())
            skip_whitespace()
            local char = input:sub(index, index)
            if char == "]" then
                index = index + 1
                return items
            end
            if char ~= "," then
                decode_error("expected ',' or ']' in array")
            end
            index = index + 1
            skip_whitespace()
        end
    end

    local function parse_object()
        index = index + 1
        skip_whitespace()
        local object = {}
        if input:sub(index, index) == "}" then
            index = index + 1
            return object
        end
        while true do
            if input:sub(index, index) ~= '"' then
                decode_error("expected object key")
            end
            local key = parse_string()
            skip_whitespace()
            if input:sub(index, index) ~= ":" then
                decode_error("expected ':' after key")
            end
            index = index + 1
            skip_whitespace()
            object[key] = parse_value()
            skip_whitespace()
            local char = input:sub(index, index)
            if char == "}" then
                index = index + 1
                return object
            end
            if char ~= "," then
                decode_error("expected ',' or '}' in object")
            end
            index = index + 1
            skip_whitespace()
        end
    end

    function parse_value()
        skip_whitespace()
        local char = input:sub(index, index)
        if char == '"' then
            return parse_string()
        end
        if char == "{" then
            return parse_object()
        end
        if char == "[" then
            return parse_array()
        end
        if char == "-" or char:match("%d") then
            return parse_number()
        end
        if input:sub(index, index + 3) == "true" then
            index = index + 4
            return true
        end
        if input:sub(index, index + 4) == "false" then
            index = index + 5
            return false
        end
        if input:sub(index, index + 3) == "null" then
            index = index + 4
            return nil
        end
        decode_error("unexpected token")
    end

    local value = parse_value()
    skip_whitespace()
    if index <= length then
        decode_error("trailing characters")
    end
    return value
end

local function emit_event(context, name, payload)
    if context == nil or context.events == nil then
        return
    end
    local fn = context.events[name]
    if type(fn) == "function" then
        fn(payload)
    end
end

local function register_artifact(context, payload)
    if context == nil or context.artifacts == nil then
        return
    end
    local fn = context.artifacts.register
    if type(fn) == "function" then
        fn(payload)
    end
end

local function begin_step(context, label)
    if context == nil or context.tx == nil then
        return
    end
    local fn = context.tx.begin_step
    if type(fn) == "function" then
        fn(label)
    end
end

local function tx_success(context)
    if context == nil or context.tx == nil then
        return
    end
    local fn = context.tx.success
    if type(fn) == "function" then
        fn()
    end
end

local function tx_failed(context, message)
    if context == nil or context.tx == nil then
        return
    end
    local fn = context.tx.failed
    if type(fn) == "function" then
        fn(message)
    end
end

local function log_warn(context, message)
    if context == nil or context.log == nil then
        return
    end
    local fn = context.log.warn
    if type(fn) == "function" then
        fn(message)
    end
end

local function log_error(context, message)
    if context == nil or context.log == nil then
        return
    end
    local fn = context.log.error
    if type(fn) == "function" then
        fn(message)
    end
end

local function command_exists(binary)
    return reqpack.exec.run("command -v " .. shell_quote(binary) .. " >/dev/null 2>&1").success
end

local function run_shell(command)
    local handle = io.popen(command .. " 2>&1")
    if handle == nil then
        return false, "", 1
    end
    local output = handle:read("*a") or ""
    local ok, _, code = handle:close()
    if ok == true then
        return true, output, 0
    end
    return false, output, tonumber(code) or 1
end

local function shell_success(command)
    local success = run_shell(command)
    return success
end

local function read_file(path)
    local file = io.open(path, "rb")
    if file == nil then
        return nil
    end
    local content = file:read("*a") or ""
    file:close()
    return content
end

local function mkdir_p(path)
    local success, output = run_shell("mkdir -p " .. shell_quote(path))
    if not success and trim(output) ~= "" then
        return false, trim(output)
    end
    return success, trim(output)
end

local function remove_tree(path)
    local success, output = run_shell("rm -rf " .. shell_quote(path))
    if not success and trim(output) ~= "" then
        return false, trim(output)
    end
    return success, trim(output)
end

local function copy_tree(source, destination)
    local created, create_error = mkdir_p(destination)
    if not created then
        return false, create_error
    end
    local command = "cp -R " .. shell_quote(join_path(source, ".")) .. " " .. shell_quote(destination)
    local success, output = run_shell(command)
    if not success and trim(output) ~= "" then
        return false, trim(output)
    end
    return success, trim(output)
end

local function path_exists(path)
    local success = shell_success("test -e " .. shell_quote(path))
    return success
end

local function directory_exists(path)
    local success = shell_success("test -d " .. shell_quote(path))
    return success
end

local function write_file(path, content)
    local parent = dirname(path)
    local created, create_error = mkdir_p(parent)
    if not created then
        return false, create_error
    end
    local file, error_message = io.open(path, "wb")
    if file == nil then
        return false, trim(error_message)
    end
    file:write(content or "")
    file:close()
    return true, nil
end

local function is_remote_url(value)
    local text = tostring(value or "")
    if starts_with(text, "file://") then
        return false
    end
    if text:match("^[A-Za-z][A-Za-z0-9+.-]*://") then
        return true
    end
    if text:match("^[^/%s]+@[^:%s]+:.+$") then
        return true
    end
    return false
end

local function is_absolute_path(value)
    local text = tostring(value or "")
    return starts_with(text, "/")
end

local function resolve_local_path(base, value)
    local text = trim(value)
    if text == "" then
        return ""
    end
    if starts_with(text, "file://") then
        local stripped = text:sub(8)
        if is_absolute_path(stripped) then
            return stripped
        end
        return join_path(base, stripped)
    end
    if is_absolute_path(text) then
        return text
    end
    if is_remote_url(text) then
        return text
    end
    return join_path(base, text)
end

local function resolve_reference(base, value)
    local text = trim(value)
    if text == "" then
        return ""
    end
    if starts_with(text, "file://") or is_remote_url(text) or is_absolute_path(text) then
        return text
    end
    if starts_with(base or "", "http://") or starts_with(base or "", "https://") then
        if ends_with(base, "/") then
            return base .. text
        end
        return base .. "/" .. text
    end
    return resolve_local_path(base, text)
end

local function read_text_from_url(context, url, output_base)
    if not is_remote_url(url) and not starts_with(url, "file://") then
        return read_file(url), nil
    end

    if starts_with(url, "file://") then
        return read_file(url:sub(8)), nil
    end

    local tmp_root = context ~= nil and context.fs ~= nil and type(context.fs.get_tmp_dir) == "function"
        and context.fs.get_tmp_dir()
        or ""
    local base = output_base
    if trim(base) == "" then
        base = join_path(tmp_root ~= "" and tmp_root or "/tmp", sanitize_name(url))
    end

    local extension = url:match("(%.[A-Za-z0-9._-]+)$") or ""
    local target = extension ~= "" and (base .. extension) or base

    if context ~= nil and context.net ~= nil and type(context.net.download) == "function" then
        local downloaded = context.net.download(url, base)
        if downloaded then
            return read_file(target), nil
        end
    end

    local success, output = run_shell("curl -fsSL " .. shell_quote(url))
    if not success then
        return nil, trim(output)
    end
    return output, nil
end

local function parse_json_text(text)
    local ok, value = pcall(json_decode, text)
    if not ok then
        return nil, tostring(value)
    end
    return value, nil
end

local function read_json_file(path)
    local text = read_file(path)
    if text == nil then
        return nil, nil
    end
    return parse_json_text(text)
end

local function write_json_file(path, value)
    local ok, encoded = pcall(json_encode, value)
    if not ok then
        return false, tostring(encoded)
    end
    return write_file(path, encoded .. "\n")
end

local function string_array(value)
    if type(value) ~= "table" then
        return {}
    end
    local items = {}
    for _, item in ipairs(value) do
        local text = trim(item)
        if text ~= "" then
            table.insert(items, text)
        end
    end
    return items
end

local function table_field(value, key)
    if type(value) ~= "table" then
        return nil
    end
    return value[key]
end

local function source_table_from_value(value)
    if type(value) ~= "table" then
        return nil
    end
    local source_type = trim(value.type)
    local source_url = trim(value.url)
    if source_type == "" or source_url == "" then
        return nil
    end
    return {
        type = source_type,
        url = source_url,
        ref = trim(value.ref),
        subdir = trim(value.subdir),
    }
end

local function parse_catalog_template(entry, repository, catalog_base)
    if type(entry) ~= "table" then
        return nil
    end
    local source = source_table_from_value(entry.source)
    if source == nil or source.type ~= "git" then
        return nil
    end

    local id = trim(entry.id)
    local version = trim(entry.version)
    if id == "" or version == "" then
        return nil
    end

    return {
        id = id,
        name = trim(entry.name) ~= "" and trim(entry.name) or id,
        description = trim(entry.description),
        version = version,
        tags = string_array(entry.tags),
        source = source,
        repository = {
            id = trim(repository.id) ~= "" and trim(repository.id) or "repo",
            url = trim(repository.url),
            priority = tonumber(repository.priority) or 0,
        },
        catalogBase = catalog_base,
    }
end

local function candidate_beats(candidate, current)
    if current == nil then
        return true
    end
    if candidate.repository.priority ~= current.repository.priority then
        return candidate.repository.priority > current.repository.priority
    end
    return candidate.repository.id < current.repository.id
end

local function sort_records_by_id(records)
    table.sort(records, function(left, right)
        return tostring(left.id or "") < tostring(right.id or "")
    end)
    return records
end

local function state_paths(data_root)
    local index_root = join_path(data_root, "index")
    return {
        dataRoot = data_root,
        templatesRoot = join_path(data_root, "templates"),
        indexRoot = index_root,
        tempifyIndex = join_path(index_root, "templates.json"),
        availableState = join_path(index_root, "reqpack-available.json"),
        installedState = join_path(index_root, "reqpack-installed.json"),
    }
end

local function template_target_path(paths, id)
    return join_path(paths.templatesRoot, id)
end

local function normalize_installed_record(paths, record)
    return {
        id = trim(record.id),
        name = trim(record.name) ~= "" and trim(record.name) or trim(record.id),
        description = trim(record.description),
        version = trim(record.version),
        tags = string_array(record.tags),
        path = template_target_path(paths, trim(record.id)),
        source = type(record.source) == "table" and {
            type = trim(record.source.type),
            url = trim(record.source.url),
            ref = trim(record.source.ref),
            subdir = trim(record.source.subdir),
        } or { type = "git", url = "", ref = "", subdir = "" },
        repository = type(record.repository) == "table" and {
            id = trim(record.repository.id),
            url = trim(record.repository.url),
            priority = tonumber(record.repository.priority) or 0,
        } or { id = "", url = "", priority = 0 },
        installedAt = trim(record.installedAt),
    }
end

local function load_installed_records(paths)
    local root = read_json_file(paths.installedState)
    if root == nil or type(root) ~= "table" or type(root.templates) ~= "table" then
        return {}
    end
    local records = {}
    for _, entry in ipairs(root.templates) do
        local record = normalize_installed_record(paths, entry)
        if record.id ~= "" then
            table.insert(records, record)
        end
    end
    return sort_records_by_id(records)
end

local function save_installed_records(paths, records)
    local ordered = sort_records_by_id(records)
    return write_json_file(paths.installedState, {
        schemaVersion = STATE_SCHEMA_VERSION,
        templates = ordered,
    })
end

local function load_available_records(paths)
    local root = read_json_file(paths.availableState)
    if root == nil or type(root) ~= "table" or type(root.templates) ~= "table" then
        return {}
    end
    local records = {}
    for _, entry in ipairs(root.templates) do
        local record = parse_catalog_template(entry, entry.repository or {}, trim(entry.catalogBase))
        if record ~= nil then
            table.insert(records, record)
        end
    end
    return sort_records_by_id(records)
end

local function save_available_records(paths, records)
    local ordered = sort_records_by_id(records)
    local payload = {}
    for _, record in ipairs(ordered) do
        table.insert(payload, {
            id = record.id,
            name = record.name,
            description = record.description,
            version = record.version,
            tags = record.tags,
            source = record.source,
            repository = record.repository,
            catalogBase = record.catalogBase,
        })
    end
    return write_json_file(paths.availableState, {
        schemaVersion = STATE_SCHEMA_VERSION,
        templates = payload,
    })
end

local function save_tempify_index(paths, records)
    local ordered = sort_records_by_id(records)
    local templates = {}
    for _, record in ipairs(ordered) do
        table.insert(templates, {
            id = record.id,
            name = record.name,
            description = record.description,
            version = record.version,
            path = template_target_path(paths, record.id),
        })
    end
    return write_json_file(paths.tempifyIndex, { templates = templates })
end

local function ensure_store_layout(paths)
    local ok, message = mkdir_p(paths.templatesRoot)
    if not ok then
        return false, message
    end
    return mkdir_p(paths.indexRoot)
end

local function installed_record_map(records)
    local result = {}
    for _, record in ipairs(records or {}) do
        result[record.id] = record
    end
    return result
end

local function upsert_record(records, record)
    local updated = {}
    local replaced = false
    for _, existing in ipairs(records or {}) do
        if existing.id == record.id then
            table.insert(updated, record)
            replaced = true
        else
            table.insert(updated, existing)
        end
    end
    if not replaced then
        table.insert(updated, record)
    end
    return sort_records_by_id(updated)
end

local function remove_record(records, id)
    local updated = {}
    for _, existing in ipairs(records or {}) do
        if existing.id ~= id then
            table.insert(updated, existing)
        end
    end
    return sort_records_by_id(updated)
end

local function now_utc_string()
    local success, output = run_shell("date -u +%Y-%m-%dT%H:%M:%SZ")
    if success then
        local value = trim(output)
        if value ~= "" then
            return value
        end
    end
    return "1970-01-01T00:00:00Z"
end

local function flags_from_context(context)
    local values = {
        dataRoot = nil,
        seedFrom = nil,
        repositories = {},
    }
    for _, raw in ipairs(context ~= nil and context.flags or {}) do
        local flag = trim(raw)
        if starts_with(flag, "data-root=") then
            values.dataRoot = trim(flag:sub(#"data-root=" + 1))
        elseif starts_with(flag, "seed-from=") then
            values.seedFrom = trim(flag:sub(#"seed-from=" + 1))
        elseif starts_with(flag, "repo=") then
            local parts = split(flag:sub(#"repo=" + 1), "|", true)
            if #parts >= 3 then
                local url_parts = {}
                for index = 3, #parts do
                    table.insert(url_parts, parts[index])
                end
                table.insert(values.repositories, {
                    id = trim(parts[1]),
                    priority = tonumber(trim(parts[2])) or 0,
                    url = trim(join(url_parts, "|")),
                    enabled = true,
                })
            end
        end
    end
    return values
end

local function resolve_data_root(context, options)
    local override = trim(options.dataRoot)
    if override ~= "" then
        if starts_with(override, "@tmp") then
            local temp_root = context ~= nil and context.fs ~= nil and type(context.fs.get_tmp_dir) == "function"
                and context.fs.get_tmp_dir()
                or "/tmp"
            local suffix = trim(override:sub(5))
            suffix = starts_with(suffix, "/") and suffix:sub(2) or suffix
            if suffix == "" then
                return temp_root
            end
            return join_path(temp_root, suffix)
        end
        return resolve_local_path(context.plugin.dir, override)
    end

    local success, output = run_shell("printf '%s' \"${XDG_DATA_HOME:-$HOME/.local/share}\"")
    local root = success and trim(output) or ""
    if root == "" then
        root = join_path(context.plugin.dir, ".tempify-data")
    end
    return join_path(root, "tempify")
end

local function effective_repositories(context, options)
    local repositories = {}
    if #options.repositories > 0 then
        for _, repo in ipairs(options.repositories) do
            table.insert(repositories, {
                id = trim(repo.id),
                url = resolve_local_path(context.plugin.dir, repo.url),
                priority = tonumber(repo.priority) or 0,
                enabled = repo.enabled ~= false,
            })
        end
        return repositories
    end

    for _, repo in ipairs(context ~= nil and context.repositories or {}) do
        if repo.enabled ~= false then
            table.insert(repositories, {
                id = trim(repo.id),
                url = resolve_local_path(context.plugin.dir, trim(repo.url)),
                priority = tonumber(repo.priority) or 0,
                enabled = repo.enabled ~= false,
            })
        end
    end
    return repositories
end

local function prepare_runtime(context)
    local options = flags_from_context(context)
    local data_root = resolve_data_root(context, options)
    local paths = state_paths(data_root)
    local ok, message = ensure_store_layout(paths)
    if not ok then
        return nil, message
    end

    if trim(options.seedFrom) ~= "" then
        local seed_root = resolve_local_path(context.plugin.dir, options.seedFrom)
        if directory_exists(seed_root) then
            local removed, remove_error = remove_tree(paths.dataRoot)
            if not removed then
                return nil, remove_error
            end
            local seeded, seed_error = copy_tree(seed_root, paths.dataRoot)
            if not seeded then
                return nil, seed_error
            end
            local ensured, ensure_error = ensure_store_layout(paths)
            if not ensured then
                return nil, ensure_error
            end
        end
    end

    return {
        options = options,
        paths = paths,
    }, nil
end

local function repository_catalog_base(url)
    local text = trim(url)
    if starts_with(text, "http://") or starts_with(text, "https://") then
        local match = text:match("^(.*)/[^/]*$")
        return match or text
    end
    return dirname(resolve_local_path(".", text))
end

local function refresh_catalog(context, runtime)
    local repositories = effective_repositories(context, runtime.options)
    if #repositories == 0 then
        return load_available_records(runtime.paths), nil
    end

    local winners = {}
    for index = 1, #repositories do
        local repository = repositories[index]
        local content, read_error = read_text_from_url(context, repository.url, join_path(runtime.paths.indexRoot, "repo-" .. tostring(index)))
        if content == nil then
            log_warn(context, "skip catalog '" .. tostring(repository.id) .. "': " .. tostring(read_error or "read failed"))
        else
            local parsed, parse_error = parse_json_text(content)
            if parsed == nil or type(parsed) ~= "table" or type(parsed.templates) ~= "table" then
                log_warn(context, "skip catalog '" .. tostring(repository.id) .. "': " .. tostring(parse_error or "invalid index"))
            else
                local catalog_base = repository_catalog_base(repository.url)
                for _, entry in ipairs(parsed.templates) do
                    local candidate = parse_catalog_template(entry, repository, catalog_base)
                    if candidate ~= nil and candidate_beats(candidate, winners[candidate.id]) then
                        winners[candidate.id] = candidate
                    end
                end
            end
        end
    end

    local records = {}
    for _, candidate in pairs(winners) do
        table.insert(records, candidate)
    end
    records = sort_records_by_id(records)

    if #records > 0 then
        local saved, save_error = save_available_records(runtime.paths, records)
        if not saved then
            return nil, save_error
        end
        return records, nil
    end

    return load_available_records(runtime.paths), nil
end

local function find_catalog_record(records, id)
    local wanted = trim(id)
    for _, record in ipairs(records or {}) do
        if record.id == wanted then
            return record
        end
    end
    return nil
end

local function search_prompt_value(context, prompt)
    local direct = trim(prompt)
    if direct ~= "" then
        return direct
    end
    return trim(context ~= nil and context.prompt or "")
end

local function info_name_value(context, name)
    local direct = trim(name)
    if direct ~= "" then
        return direct
    end
    return trim(context ~= nil and context.prompt or "")
end

local function matches_search(record, prompt)
    local query = lower(prompt)
    if query == "" then
        return true
    end
    if lower(record.id):find(query, 1, true) ~= nil then
        return true
    end
    if lower(record.name):find(query, 1, true) ~= nil then
        return true
    end
    if lower(record.description):find(query, 1, true) ~= nil then
        return true
    end
    for _, tag in ipairs(record.tags or {}) do
        if lower(tag):find(query, 1, true) ~= nil then
            return true
        end
    end
    return false
end

local function template_manifest(template_root)
    local manifest_path = join_path(template_root, "template.lua")
    if not path_exists(manifest_path) then
        return nil, "template root missing template.lua: " .. template_root
    end
    local ok, manifest = pcall(dofile, manifest_path)
    if not ok then
        return nil, tostring(manifest)
    end
    if type(manifest) ~= "table" then
        return nil, "template.lua must return table: " .. manifest_path
    end

    local id = trim(manifest.id)
    local version = trim(manifest.version)
    if id == "" or version == "" then
        return nil, "template.lua missing id or version: " .. manifest_path
    end

    return {
        id = id,
        name = trim(manifest.name) ~= "" and trim(manifest.name) or id,
        description = trim(manifest.description),
        version = version,
    }, nil
end

local function resolve_template_root_from_catalog(record, context)
    local source_url = resolve_reference(record.catalogBase, record.source.url)
    if not is_remote_url(source_url) and directory_exists(source_url) and trim(record.source.ref) == "" then
        local root = source_url
        if trim(record.source.subdir) ~= "" then
            root = resolve_local_path(source_url, record.source.subdir)
        end
        return root, nil
    end

    local temp_root = context ~= nil and context.fs ~= nil and type(context.fs.get_tmp_dir) == "function"
        and context.fs.get_tmp_dir()
        or ""
    if trim(temp_root) == "" then
        return nil, "could not allocate temp directory"
    end
    local clone_root = join_path(temp_root, sanitize_name(record.id) .. "-clone")
    local removed, remove_error = remove_tree(clone_root)
    if not removed then
        return nil, remove_error
    end

    local clone_command = GIT_BINARY .. " clone --depth 1 "
    if trim(record.source.ref) ~= "" then
        clone_command = clone_command .. "--branch " .. shell_quote(record.source.ref) .. " "
    end
    clone_command = clone_command .. shell_quote(source_url) .. " " .. shell_quote(clone_root)

    local cloned, clone_output = run_shell(clone_command)
    if not cloned then
        return nil, trim(clone_output)
    end

    local root = clone_root
    if trim(record.source.subdir) ~= "" then
        root = resolve_local_path(clone_root, record.source.subdir)
    end
    return root, nil
end

local function install_record_from_manifest(paths, manifest, source, repository)
    return {
        id = manifest.id,
        name = manifest.name,
        description = manifest.description,
        version = manifest.version,
        tags = source.tags or {},
        path = template_target_path(paths, manifest.id),
        source = {
            type = trim(source.source and source.source.type or source.type),
            url = trim(source.source and source.source.url or source.url),
            ref = trim(source.source and source.source.ref or source.ref),
            subdir = trim(source.source and source.source.subdir or source.subdir),
        },
        repository = {
            id = trim(repository.id),
            url = trim(repository.url),
            priority = tonumber(repository.priority) or 0,
        },
        installedAt = now_utc_string(),
    }
end

local function apply_install(paths, template_root, manifest, source, repository)
    local target_root = template_target_path(paths, manifest.id)
    local removed, remove_error = remove_tree(target_root)
    if not removed then
        return nil, remove_error
    end
    local copied, copy_error = copy_tree(template_root, target_root)
    if not copied then
        return nil, copy_error
    end
    return install_record_from_manifest(paths, manifest, source, repository), nil
end

local function sync_store(paths, installed_records)
    local saved, save_error = save_installed_records(paths, installed_records)
    if not saved then
        return false, save_error
    end
    return save_tempify_index(paths, installed_records)
end

local function package_info_from_record(record, installed_record)
    local installed = installed_record ~= nil
    return {
        system = "tempify",
        name = record.id,
        packageId = record.id,
        version = installed_record ~= nil and installed_record.version or record.version,
        latestVersion = installed and record.version ~= installed_record.version and record.version or "",
        installed = installed and "true" or "false",
        summary = record.name,
        description = record.description,
        type = "template",
        sourceUrl = trim(record.source.url),
        repository = trim(record.repository.id),
        tags = record.tags,
        extraFields = {
            displayName = record.name,
            installedPath = installed_record ~= nil and installed_record.path or "",
            installedVersion = installed_record ~= nil and installed_record.version or "",
            sourceRef = trim(record.source.ref),
            sourceSubdir = trim(record.source.subdir),
        },
    }
end

local function installed_info(record)
    return {
        system = "tempify",
        name = record.id,
        packageId = record.id,
        version = record.version,
        installed = "true",
        summary = record.name,
        description = record.description,
        type = "template",
        sourceUrl = trim(record.source.url),
        repository = trim(record.repository.id),
        tags = record.tags,
        extraFields = {
            displayName = record.name,
            installedPath = record.path,
            installedVersion = record.version,
            sourceRef = trim(record.source.ref),
            sourceSubdir = trim(record.source.subdir),
        },
    }
end

function plugin.getName()
    return PLUGIN_NAME
end

function plugin.getVersion()
    return PLUGIN_VERSION
end

function plugin.getRequirements()
    return {}
end

function plugin.getCategories()
    return { "Template", "Scaffolding", "Tempify" }
end

function plugin.getSecurityMetadata()
    return {
        role = "package-manager",
        capabilities = { "exec" },
        privilegeLevel = "none",
        writeScopes = {
            { kind = "temp" },
            { kind = "user-home-subpath", value = ".local/share/tempify" },
        },
    }
end

function plugin.getMissingPackages(packages)
    return packages or {}
end

function plugin.install(context, packages)
    begin_step(context, "install tempify templates")

    local runtime, runtime_error = prepare_runtime(context)
    if runtime == nil then
        tx_failed(context, runtime_error)
        return false
    end

    local catalog, catalog_error = refresh_catalog(context, runtime)
    if catalog == nil then
        tx_failed(context, catalog_error)
        return false
    end

    local records = load_installed_records(runtime.paths)
    local installed = {}
    for _, pkg in ipairs(packages or {}) do
        local record = find_catalog_record(catalog, trim(pkg.name))
        if record == nil then
            emit_event(context, "unavailable", trim(pkg.name))
            tx_failed(context, "template not found: " .. trim(pkg.name))
            return false
        end
        if trim(pkg.version) ~= "" and trim(pkg.version) ~= record.version then
            emit_event(context, "unavailable", trim(pkg.name))
            tx_failed(context, "requested version not available for template: " .. trim(pkg.name))
            return false
        end

        local template_root, template_error = resolve_template_root_from_catalog(record, context)
        if template_root == nil then
            tx_failed(context, template_error)
            return false
        end

        local manifest, manifest_error = template_manifest(template_root)
        if manifest == nil then
            tx_failed(context, manifest_error)
            return false
        end
        if manifest.id ~= record.id then
            tx_failed(context, "catalog/template id mismatch for " .. record.id)
            return false
        end
        if manifest.version ~= record.version then
            tx_failed(context, "catalog/template version mismatch for " .. record.id)
            return false
        end

        local installed_record, install_error = apply_install(runtime.paths, template_root, manifest, record, record.repository)
        if installed_record == nil then
            tx_failed(context, install_error)
            return false
        end
        records = upsert_record(records, installed_record)
        table.insert(installed, installed_record)
    end

    local synced, sync_error = sync_store(runtime.paths, records)
    if not synced then
        tx_failed(context, sync_error)
        return false
    end

    for _, record in ipairs(installed) do
        register_artifact(context, { kind = "template-root", path = record.path })
        emit_event(context, "installed", { name = record.id, version = record.version })
    end

    tx_success(context)
    return true
end

function plugin.installLocal(context, path)
    begin_step(context, "install local tempify template")

    local runtime, runtime_error = prepare_runtime(context)
    if runtime == nil then
        tx_failed(context, runtime_error)
        return false
    end

    local source_root = resolve_local_path(context.plugin.dir, path)
    if not directory_exists(source_root) then
        tx_failed(context, "local template path not found: " .. tostring(path))
        return false
    end

    local manifest, manifest_error = template_manifest(source_root)
    if manifest == nil then
        tx_failed(context, manifest_error)
        return false
    end

    local installed_record, install_error = apply_install(runtime.paths, source_root, manifest, {
        type = "git",
        url = source_root,
        ref = "",
        subdir = "",
        tags = {},
    }, {
        id = "local",
        url = source_root,
        priority = 0,
    })
    if installed_record == nil then
        tx_failed(context, install_error)
        return false
    end

    local records = upsert_record(load_installed_records(runtime.paths), installed_record)
    local synced, sync_error = sync_store(runtime.paths, records)
    if not synced then
        tx_failed(context, sync_error)
        return false
    end

    register_artifact(context, { kind = "template-root", path = installed_record.path })
    emit_event(context, "installed", { name = installed_record.id, version = installed_record.version, localTarget = true })
    tx_success(context)
    return true
end

function plugin.remove(context, packages)
    begin_step(context, "remove tempify templates")

    local runtime, runtime_error = prepare_runtime(context)
    if runtime == nil then
        tx_failed(context, runtime_error)
        return false
    end

    local records = load_installed_records(runtime.paths)
    local installed = installed_record_map(records)
    local removed = {}

    for _, pkg in ipairs(packages or {}) do
        local id = trim(pkg.name)
        local record = installed[id]
        if record ~= nil then
            local deleted, delete_error = remove_tree(record.path)
            if not deleted then
                tx_failed(context, delete_error)
                return false
            end
            records = remove_record(records, id)
            table.insert(removed, record)
        end
    end

    local synced, sync_error = sync_store(runtime.paths, records)
    if not synced then
        tx_failed(context, sync_error)
        return false
    end

    for _, record in ipairs(removed) do
        emit_event(context, "deleted", { name = record.id, version = record.version })
    end
    tx_success(context)
    return true
end

function plugin.update(context, packages)
    begin_step(context, "update tempify templates")

    local runtime, runtime_error = prepare_runtime(context)
    if runtime == nil then
        tx_failed(context, runtime_error)
        return false
    end

    local catalog, catalog_error = refresh_catalog(context, runtime)
    if catalog == nil then
        tx_failed(context, catalog_error)
        return false
    end

    local records = load_installed_records(runtime.paths)
    local installed = installed_record_map(records)
    local requested = {}
    if packages ~= nil and #packages > 0 then
        for _, pkg in ipairs(packages) do
            requested[trim(pkg.name)] = true
        end
    end

    local updated = {}
    for _, record in ipairs(records) do
        if next(requested) == nil or requested[record.id] then
            local winner = find_catalog_record(catalog, record.id)
            if winner ~= nil and winner.version ~= record.version then
                local template_root, template_error = resolve_template_root_from_catalog(winner, context)
                if template_root == nil then
                    tx_failed(context, template_error)
                    return false
                end
                local manifest, manifest_error = template_manifest(template_root)
                if manifest == nil then
                    tx_failed(context, manifest_error)
                    return false
                end
                local installed_record, install_error = apply_install(runtime.paths, template_root, manifest, winner, winner.repository)
                if installed_record == nil then
                    tx_failed(context, install_error)
                    return false
                end
                records = upsert_record(records, installed_record)
                table.insert(updated, installed_record)
            end
        end
    end

    local synced, sync_error = sync_store(runtime.paths, records)
    if not synced then
        tx_failed(context, sync_error)
        return false
    end

    for _, record in ipairs(updated) do
        emit_event(context, "updated", { name = record.id, version = record.version })
    end
    tx_success(context)
    return true
end

function plugin.list(context)
    local runtime, runtime_error = prepare_runtime(context)
    if runtime == nil then
        log_error(context, runtime_error)
        return {}
    end

    local records = load_installed_records(runtime.paths)
    local items = {}
    for _, record in ipairs(records) do
        table.insert(items, installed_info(record))
    end
    emit_event(context, "listed", items)
    return items
end

function plugin.search(context, prompt)
    local runtime, runtime_error = prepare_runtime(context)
    if runtime == nil then
        log_error(context, runtime_error)
        return {}
    end

    local catalog, catalog_error = refresh_catalog(context, runtime)
    if catalog == nil then
        log_error(context, catalog_error)
        return {}
    end

    local query = search_prompt_value(context, prompt)
    local installed = installed_record_map(load_installed_records(runtime.paths))
    local items = {}
    for _, record in ipairs(catalog) do
        if matches_search(record, query) then
            table.insert(items, package_info_from_record(record, installed[record.id]))
        end
    end
    emit_event(context, "searched", items)
    return items
end

function plugin.info(context, name)
    local runtime, runtime_error = prepare_runtime(context)
    if runtime == nil then
        log_error(context, runtime_error)
        return {}
    end

    local installed = installed_record_map(load_installed_records(runtime.paths))
    local wanted = info_name_value(context, name)

    local catalog, catalog_error = refresh_catalog(context, runtime)
    if catalog == nil then
        log_error(context, catalog_error)
        local installed_only = installed[wanted]
        if installed_only ~= nil then
            local item = installed_info(installed_only)
            emit_event(context, "informed", item)
            return item
        end
        return {}
    end

    local record = find_catalog_record(catalog, wanted)
    if record ~= nil then
        local item = package_info_from_record(record, installed[record.id])
        emit_event(context, "informed", item)
        return item
    end

    local installed_only = installed[wanted]
    if installed_only ~= nil then
        local item = installed_info(installed_only)
        emit_event(context, "informed", item)
        return item
    end

    emit_event(context, "unavailable", wanted)
    return {}
end

function plugin.outdated(context)
    local runtime, runtime_error = prepare_runtime(context)
    if runtime == nil then
        log_error(context, runtime_error)
        return {}
    end

    local catalog, catalog_error = refresh_catalog(context, runtime)
    if catalog == nil then
        log_error(context, catalog_error)
        return {}
    end

    local installed = load_installed_records(runtime.paths)
    local winners = {}
    for _, record in ipairs(catalog) do
        winners[record.id] = record
    end

    local items = {}
    for _, record in ipairs(installed) do
        local winner = winners[record.id]
        if winner ~= nil and winner.version ~= record.version then
            table.insert(items, package_info_from_record(winner, record))
        end
    end
    emit_event(context, "outdated", items)
    return items
end

function plugin.init()
    return command_exists(GIT_BINARY)
end

function plugin.shutdown()
    return true
end

return plugin

#include "LuaEngineInternal.h"
#include "tempify/lua/LuaEngine.h"

namespace tempify {

namespace {

void push_question_definition(lua_State *state, const QuestionDefinition &question, const char *missing_message) {
    lua_internal::register_metadata_helpers(state);
    lua_internal::load_file_result(state, question.source_path);
    if (lua_istable(state, -1) == 0) {
        lua_internal::throw_lua_error(question.source_path,
                                      "questions.lua must return a table with 'order' and 'groups'");
    }

    lua_getfield(state, -1, "groups");
    if (lua_istable(state, -1) == 0) {
        lua_internal::throw_lua_error(question.source_path, "questions.lua missing 'groups' table");
    }

    lua_getfield(state, -1, question.group.c_str());
    if (lua_istable(state, -1) == 0) {
        lua_internal::throw_lua_error(question.source_path, "Question group not found: " + question.group);
    }

    lua_rawgeti(state, -1, static_cast<lua_Integer>(question.source_index));
    if (lua_istable(state, -1) == 0) {
        lua_internal::throw_lua_error(question.source_path, missing_message);
    }
}

} // namespace

std::optional<std::string> LuaEngine::evaluate_default(const QuestionDefinition &question,
                                                       const std::map<std::string, std::string> &values) const {
    if (!question.default_is_function) {
        return question.default_value;
    }

    auto state = lua_internal::make_state();
    push_question_definition(state.get(), question, "Question definition not found for default function");

    lua_getfield(state.get(), -1, "default");
    if (lua_isfunction(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(question.source_path, "Question default is not function");
    }

    lua_internal::push_context_table(state.get(), values);
    if (lua_pcall(state.get(), 1, 1, 0) != LUA_OK) {
        const std::string message = lua_tostring(state.get(), -1);
        lua_internal::throw_lua_error(question.source_path, message);
    }

    if (lua_isnil(state.get(), -1) != 0) {
        return std::nullopt;
    }

    return lua_internal::value_to_string(state.get(), -1, question.source_path);
}

bool LuaEngine::evaluate_condition(const QuestionDefinition &question,
                                   const std::map<std::string, std::string> &values) const {
    if (!question.condition_is_function) {
        if (!question.condition_value.has_value()) {
            return true;
        }
        return lua_internal::string_truthy(*question.condition_value);
    }

    auto state = lua_internal::make_state();
    push_question_definition(state.get(), question, "Question definition not found for condition");

    lua_getfield(state.get(), -1, "condition");
    if (lua_isfunction(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(question.source_path, "Question condition is not function");
    }

    lua_internal::push_context_table(state.get(), values);
    if (lua_pcall(state.get(), 1, 1, 0) != LUA_OK) {
        const std::string message = lua_tostring(state.get(), -1);
        lua_internal::throw_lua_error(question.source_path, message);
    }

    if (lua_isboolean(state.get(), -1) != 0) {
        return lua_toboolean(state.get(), -1) != 0;
    }
    return lua_internal::string_truthy(lua_internal::value_to_string(state.get(), -1, question.source_path));
}

std::optional<std::string> LuaEngine::validate_answer(const QuestionDefinition &question, const std::string &candidate,
                                                      const std::map<std::string, std::string> &values) const {
    if (!question.validate_is_function) {
        return std::nullopt;
    }

    auto state = lua_internal::make_state();
    push_question_definition(state.get(), question, "Question definition not found for validate");

    lua_getfield(state.get(), -1, "validate");
    if (lua_isfunction(state.get(), -1) == 0) {
        lua_internal::throw_lua_error(question.source_path, "Question validate is not function");
    }

    lua_internal::push_context_table(state.get(), values, {}, {}, candidate);
    if (lua_pcall(state.get(), 1, 1, 0) != LUA_OK) {
        const std::string message = lua_tostring(state.get(), -1);
        lua_internal::throw_lua_error(question.source_path, message);
    }

    if (lua_isnil(state.get(), -1) != 0) {
        return std::nullopt;
    }
    if (lua_isboolean(state.get(), -1) != 0) {
        if (lua_toboolean(state.get(), -1) != 0) {
            return std::nullopt;
        }
        return std::string("Invalid value for '") + question.key + "'";
    }

    const std::string message = lua_internal::value_to_string(state.get(), -1, question.source_path);
    if (message.empty()) {
        return std::string("Invalid value for '") + question.key + "'";
    }
    return message;
}

} // namespace tempify

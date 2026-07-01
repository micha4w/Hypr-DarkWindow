#include "LuaCallbacks.h"

#include <lua.h>

#include <expected>
#include <optional>

#include "CustomShader.h"
#include "ShadeManager.h"
#include "State.h"
#include "config/lua/bindings/LuaBindingsInternal.hpp"
#include "cursor-shape-v1.hpp"
#include "debug/log/Logger.hpp"


namespace HyprLua = Config::Lua::Bindings::Internal;

static std::expected<std::string, std::string> luaArgsToString(lua_State* L, int idx)
{
    idx = lua_absindex(L, idx);

    std::string args;
    if (lua_type(L, idx) == LUA_TSTRING)
    {
        args = lua_tostring(L, idx);
        try
        {
            ShaderDefinition::Parse(std::string("dummy ") + args);
        }
        catch (const std::exception& ex)
        {
            return std::unexpected("args field syntax error ('" + args + "'): " + ex.what());
        }
    }
    else if (lua_istable(L, idx))
    {
        lua_pushnil(L);

        while (lua_next(L, idx) != 0)
        {
            if (lua_type(L, -2) != LUA_TSTRING)
            {
                lua_pop(L, 2);
                return std::unexpected("key of args table must be string");
            }
            // TODO: check if the key is a valid uniform name

            args += lua_tostring(L, -2);
            args += '=';

            if (lua_istable(L, -1))
            {
                args += '[';
                size_t n = lua_rawlen(L, -1);
                for (size_t i = 1; i <= n; i++)
                {
                    lua_rawgeti(L, -1, i);
                    if (!lua_isnumber(L, -1))
                    {
                        lua_pop(L, 3);
                        return std::unexpected("field of args table must be numeric arrays");
                    }

                    args += lua_tostring(L, -1);
                    args += ' ';
                    lua_pop(L, 1);
                }
                args.back() = ']';
            }
            else if (lua_isnumber(L, -1))
                args += lua_tostring(L, -1);
            else
            {
                lua_pop(L, 2);
                return std::unexpected("field of args table must be a number or a numeric array");
            }

            args += ' ';

            lua_pop(L, 1);
        }

        if (!args.empty())
            args.resize(args.size() - 1);
    }
    else if (!lua_isnil(L, -1))
        return std::unexpected("args field must be a table or a string");

    return args;
}

static std::expected<std::string, std::string> luaShaderToString(lua_State* L, int idx)
{
    auto shader = HyprLua::tableOptStr(L, idx, "shader");
    if (!shader)
        return std::unexpected("shader field is not a string");

    try
    {
        ShaderDefinition::Parse(*shader);
    }
    catch (const std::exception& ex)
    {
        return std::unexpected("bad shader syntax (" + *shader + "): " + ex.what());
    }


    std::string out = std::move(*shader);

    for (auto& field : { "fade_in_speed", "fade_out_speed", "animation_interval" })
    {
        lua_getfield(L, idx, field);
        if (lua_isnil(L, -1))
            goto next;

        if (!lua_isnumber(L, -1))
        {
            lua_pop(L, 1);
            return std::unexpected(std::string(field) + " field is not a number");
        }

        out += " .";
        out += field;
        out += '=';
        out += lua_tostring(L, -1);
    next:
        lua_pop(L, 1);
    }


    lua_getfield(L, idx, "args");
    auto argsResult = luaArgsToString(L, -1);
    lua_pop(L, 1);

    if (!argsResult)
        return std::unexpected("invalid args: " + argsResult.error());
    if (!argsResult.value().empty())
        out += ' ' + argsResult.value();

    return out;
}

int LuaCallbacks::loadShader(lua_State* L)
{
    if (lua_gettop(L) != 2)
        return HyprLua::configError(L, "darkwindow.load_shader: expected 2 arguments (id, config)");
    if (lua_type(L, 1) != LUA_TSTRING)
        return HyprLua::configError(L, "darkwindow.load_shader: expected first argument (id) to be a string");
    if (!lua_istable(L, 2))
        return HyprLua::configError(
            L,
            "darkwindow.load_shader: expected second argument (config) to be a table "
            "{ from?, path?, source?, args?, introduces_transparency? }"
        );

    std::string id = lua_tostring(L, 1);
    if (id.contains(' '))
        return HyprLua::configError(L, "darkwindow.load_shader: shader id cannot contain spaces");
    auto from = HyprLua::tableOptStr(L, 2, "from").value_or("");
    auto path = HyprLua::tableOptStr(L, 2, "path").value_or("");
    auto source = HyprLua::tableOptStr(L, 2, "source").value_or("");

    if (!path.empty() && !source.empty())
        return HyprLua::configError(
            L, "darkwindow.load_shader: second argument (config) should only have one of 'path' or 'source' field set"
        );

    if (from.empty() && path.empty() && source.empty())
        return HyprLua::configError(
            L, "darkwindow.load_shader: second argument (config) needs to have 'from' or one of 'path' or 'source' field set"
        );

    auto introducesTransparency = HyprLua::tableOptBool(L, 2, "introduces_transparency").value_or(false);
    auto fadeInSpeed = HyprLua::tableOptNum(L, 2, "fade_in_speed");
    auto fadeOutSpeed = HyprLua::tableOptNum(L, 2, "fade_out_speed");
    auto animationInterval = HyprLua::tableOptNum(L, 2, "animation_interval");

    lua_getfield(L, 2, "args");
    auto argsResult = luaArgsToString(L, -1);
    lua_pop(L, 1);
    if (!argsResult)
        return HyprLua::configError(L, "darkwindow.load_shader: " + argsResult.error());

    g.UserShaders.push_back(
        State::UserShader{
            .Id = id,
            .From = from,
            .Path = path,
            .Source = source,
            .Args = *argsResult,
            .IntroducesTransparency = introducesTransparency,
            .FadeInSpeed = fadeInSpeed,
            .FadeOutSpeed = fadeOutSpeed,
            .AnimationInterval = animationInterval,
        }
    );

    return 0;
}

static int dispatchedShade(lua_State* L)
{
    using namespace Config::Actions;

    const char* shader = lua_tostring(L, lua_upvalueindex(1));

    PHLWINDOW window = nullptr;
    if (!lua_isnil(L, lua_upvalueindex(2)))
    {
        window = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(2)));
        if (!window)
            return HyprLua::dispatcherError(
                L, "darkwindow.<dispatched shade>: window not found", eActionErrorLevel::ERROR, eActionErrorCode::NOT_FOUND
            );
    }
    else
        window = Desktop::focusState()->window();

    try
    {
        g.Manager.ApplyDispatchedShader(window, shader);
    }
    catch (const std::exception& ex)
    {
        return HyprLua::dispatcherError(
            L,
            std::format("darkwindow.<dispatched shade>: failed to apply shader ({}) {}", shader, ex.what()),
            eActionErrorLevel::ERROR,
            eActionErrorCode::EXECUTION_FAILED
        );
    }

    return 0;
}

int LuaCallbacks::shade(lua_State* L)
{
    if (lua_gettop(L) != 1 || !lua_istable(L, 1))
        return HyprLua::configError(L, "darkwindow.dsp_shade: expected a single table { window?, ...shader } as argument");

    auto shaderResult = luaShaderToString(L, 1);
    if (!shaderResult)
        return HyprLua::configError(L, "darkwindow.dsp_shade: invalid shader: " + shaderResult.error());

    lua_pushstring(L, shaderResult.value().c_str());

    lua_getfield(L, 1, "window");
    if (!lua_isnil(L, -1))
    {
        auto selector = HyprLua::windowSelectorFromLuaSelectorOrObject(L, -1, "darkwindow.dsp_shade");
        if (!selector)
        {
            lua_pop(L, 2);
            return HyprLua::configError(L, "darkwindow.dsp_shade: window field is not a valid window selector or object");
        }

        lua_pop(L, 1);
        lua_pushstring(L, selector->c_str());
    }

    lua_pushcclosure(L, dispatchedShade, 2);
    return 1;
}


int LuaCallbacks::buildWindowRule(lua_State* L)
{
    if (lua_gettop(L) != 1 || !lua_istable(L, 1))
        return HyprLua::configError(L, "darkwindow.build_window_rule: expected a single table { ...shader } as argument");

    auto shaderResult = luaShaderToString(L, 1);
    if (!shaderResult)
        return HyprLua::configError(L, "darkwindow.build_window_rule: invalid shader: " + shaderResult.error());

    lua_pushstring(L, shaderResult.value().c_str());
    return 1;
}
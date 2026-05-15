#include "LuaCallbacks.h"

#include "State.h"


namespace HyprLua = Config::Lua::Bindings::Internal;

int LuaCallbacks::loadShader(lua_State* L)
{
    if (lua_gettop(L) != 2)
        return HyprLua::configError(L, "darkwindow.load_shader: expected 2 arguments (id, config)");
    if (!lua_isstring(L, 1))
        return HyprLua::configError(L, "darkwindow.load_shader: expected first argument (id) to be a string");
    if (!lua_istable(L, 2))
        return HyprLua::configError(L, "darkwindow.load_shader: expected second argument (config) to be a table { from?, path?, args?, introduces_transparency? }");

    std::string id = lua_tostring(L, 1);

    auto from = HyprLua::tableOptStr(L, 2, "from").value_or("");
    auto path = HyprLua::tableOptStr(L, 2, "path").value_or("");
    auto args = HyprLua::tableOptStr(L, 2, "args").value_or("");
    auto introducesTransparency = HyprLua::tableOptBool(L, 2, "introduces_transparency").value_or(false);

    if (from.empty() && path.empty())
        return HyprLua::configError(L, "darkwindow.load_shader: second argument (config) needs to have either 'from' or 'path' field set");

    g.UserShaders.push_back(State::UserShader{
        .Id = id,
        .From = from,
        .Path = path,
        .Args = args,
        .IntroducesTransparency = introducesTransparency,
    });

    return 0;
}

static int dispatchedShade(lua_State* L)
{
    using namespace Config::Actions;

    const char* shader = lua_tostring(L, lua_upvalueindex(1));

    PHLWINDOW window = nullptr;
    if (!lua_isnil(L, lua_upvalueindex(2))) {
        window = g_pCompositor->getWindowByRegex(lua_tostring(L, lua_upvalueindex(2)));
        if (!window)
            return HyprLua::dispatcherError(L, "darkwindow.<dispatched shade>: window not found", eActionErrorLevel::ERROR, eActionErrorCode::NOT_FOUND);
    }
    else
        window = Desktop::focusState()->window();

    try
    {
        g.Manager.ApplyDispatchedShader(window, shader);
    }
    catch (const std::exception& ex)
    {
        return HyprLua::dispatcherError(L, std::format("darkwindow.<dispatched shade>: failed to apply shader ({}) {}", shader, ex.what()),
                                        eActionErrorLevel::ERROR, eActionErrorCode::EXECUTION_FAILED);
    }

    return 0;
}

int LuaCallbacks::shade(lua_State* L)
{
    if (lua_gettop(L) != 1)
        return HyprLua::configError(L, "darkwindow.load_shader: expected 1 argument (table { window?, shader })");
    if (!lua_istable(L, 1))
        return HyprLua::configError(L, "darkwindow.dsp_shade: expected a table { window?, shader }");

    lua_getfield(L, 1, "shader");
    if (!lua_isstring(L, -1))
    {
        lua_pop(L, 1);
        return HyprLua::configError(L, "darkwindow.dsp_shade: shader field is not a string");
    }

    std::string shader = lua_tostring(L, -1);
    size_t space = shader.find(" ");
    if (space != std::string::npos)
    {
        auto args = shader.substr(space + 1);
        try
        {
            ShaderDefinition::ParseArgs(args);
        }
        catch (const std::exception& ex)
        {
            lua_pop(L, 1);
            return HyprLua::configError(L, "darkwindow.dsp_shade: bad args syntax ({}): {}", args, ex.what());
        }
    }

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
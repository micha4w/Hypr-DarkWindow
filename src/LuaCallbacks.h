#pragma once

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace LuaCallbacks
{
    int loadShader(lua_State* L);
    int shade(lua_State* L);
    int buildRule(lua_State* L);
}
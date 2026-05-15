#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace LuaCallbacks {
    int loadShader(lua_State* L);
    int shade(lua_State* L);
}
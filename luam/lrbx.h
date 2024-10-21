#include "../luau/VM/src/lstate.h"

#define LUA_GAMELIBNAME "game"
#define LUA_INSTLIBNAME "Instance"
#define LUA_MRBXLIBNAME "mrbx"

int luaopen_gamelib(lua_State* L);
int luaopen_instlib(lua_State* L);
int luaopen_mrbxlib(lua_State* L);
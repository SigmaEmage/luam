// This file is part of the LuaM programming language and is not licensed;
// This file is completely closed-source and privatly owned by Malte0621;

/*
--------------------
About This File
--------------------
This file contains the
rbxlua (aka Roblox Lua)
emulated code.

I have written this file ground up and it is
therefore not in any way connected with roblox.

--------------------
Implementations
--------------------
Roblox Globals (3%)
Instances (2%)
Instance Properties (0%)
Events (0%)
Networking (1%)
Context Level Security And Identities (55%)
Rendering For Screenshots (0%)
--------------------
*/

#pragma once

#include "lrbx.h"

#include "lualib.h"

#include "../luau/VM/src/lstate.h"
#include "../luau/VM/src/lapi.h"
#include "../luau/VM/src/ldo.h"
#include "../luau/VM/src/ludata.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "lua.h"
#include <functional>


#include <iostream>

#ifdef _WIN32 // windows
#ifndef OS_WINDOWS
#define OS_WINDOWS
#endif
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <windows.h>
#include <string>
#elif defined(linux) // linux
#ifndef OS_LINUX
#define OS_LINUX
#endif
#include <string.h>
#include <math.h>
#endif // _WIN32

#include <map>
#include <list>
#include <any>

// COLORS LIST
// 1: Blue
// 2: Green
// 3: Cyan
// 4: Red
// 5: Purple
// 6: Yellow (Dark)
// 7: Default white
// 8: Gray/Grey
// 9: Bright blue
// 10: Brigth green
// 11: Bright cyan
// 12: Bright red
// 13: Pink/Magenta
// 14: Yellow
// 15: Bright white
// Numbers after 15 include background colors
void Color3(int color)
{
#ifdef OS_WINDOWS
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
#elif defined(OS_LINUX)
    std::cout << "\033[0m";
    switch (color)
    {
    case (1):
        std::cout << "\033[34m";
        break;
    case (2):
        std::cout << "\033[32m";
        break;
    case (3):
        std::cout << "\033[36m";
        break;
    case (4):
        std::cout << "\033[31m";
        break;
    case (5):
        std::cout << "\033[35m";
        break;
    case (6):
        std::cout << "\033[33m";
        break;
    case (7):
        std::cout << "\033[0m";
        break;
    case (8):
        std::cout << "\033[1;37m";
        break;
    case (9):
        std::cout << "\033[1;34m";
        break;
    case (10):
        std::cout << "\033[1;32m";
        break;
    case (11):
        std::cout << "\033[1;36m";
        break;
    case (12):
        std::cout << "\033[1;31m";
        break;
    case (13):
        std::cout << "\033[1;35m";
        break;
    case (14):
        std::cout << "\033[1;33m";
        break;
    case (15):
        std::cout << "\033[1m";
        break;
    default:
        break;
    }
#endif
}

static void writestring2(const char* s, size_t l)
{
    fwrite(s, 1, l, stdout);
}

void print(char* msg)
{
    Color3(7);
    writestring2(msg, strlen(msg));
    writestring2("\n", 1);
    Color3(7);
}

void warn(char* msg)
{
    Color3(6);
    writestring2(msg, strlen(msg));
    writestring2("\n", 1);
    Color3(7);
}

void error(lua_State* L, char* msg)
{
    int level = luaL_optinteger(L, 2, 1);
    lua_settop(L, 1);
    if (level > 0)
    { /* add extra information? */
        luaL_where(L, level);
        lua_pushstring(L, msg);
        lua_concat(L, 2);
    }
    lua_error(L);
}

typedef std::map<char*, std::any> InstancePairMap;
InstancePairMap* __internal_instances = new InstancePairMap();

class Instance
{
private:
    std::list<Instance> Children;
    std::list<luaL_Reg> InstanceLib;

public:
    bool Archivable = false;
    char* ClassName = "Instance";
    char* Name = "Instance";
    char* UnquieId = ""; // TODO: Make into a class.
    int DataCost = 0; // Internal
    Instance* Parent;
    bool RobloxLocked = false; // Internal
    int SourceAssetId = 0;
    Instance(lua_State* L) {
        static const luaL_Reg instancelib[] = {
            //{"test", test},
            //{"ClearAllChildren", ClearAllChildren}, // TODO: FIX! :(
            {NULL, NULL},
        };
        this->InstanceLib.insert(this->InstanceLib.begin(), std::begin(instancelib), std::end(instancelib));
        this->UnquieId = reinterpret_cast<char*>(this);

        lua_setfield(L, -2, "Archivable");
        lua_pushboolean(L, Archivable);

        lua_pushstring(L, ClassName);
        lua_setfield(L, -2, "ClassName");

        lua_pushstring(L, Name);
        lua_setfield(L, -2, "Name");

        lua_pushboolean(L, RobloxLocked);
        lua_setfield(L, -2, "RobloxLocked");

        lua_pushinteger(L, SourceAssetId);
        lua_setfield(L, -2, "SourceAssetId");
    }

    Instance() {
        static const luaL_Reg instancelib[] = {
            //{"test", test},
            //{"ClearAllChildren", ClearAllChildren}, // TODO: FIX! :(
            {NULL, NULL},
        };
        this->InstanceLib.insert(this->InstanceLib.begin(), std::begin(instancelib), std::end(instancelib));
        this->UnquieId = reinterpret_cast<char*>(this);
    }

    int ClearAllChildren(lua_State* L)
    {
        for (auto it = this->Children.begin(); it != this->Children.end(); ++it)
        {
            Instance inst = *it;
            inst.Destroy(L);
        }
        this->Children.clear();
        return 0;
    }

    int Clone(lua_State* L)
    {
        Instance* clone = new Instance(*this);
        __internal_instances->insert(std::pair(clone->UnquieId, clone));

        return 1;
    }

    int Destroy(lua_State* L)
    {
        __internal_instances->erase(this->UnquieId);
        delete this;
        return 0;
    }

    bool operator<(const Instance& obj) const
    {
        if (obj.UnquieId < this->UnquieId)
            return true;
    }

    bool operator==(const Instance& obj) const
    {
        if (obj.UnquieId == this->UnquieId)
            return true;
    }
};

typedef std::map<std::string, bool> FFlagPairMap;

FFlagPairMap __internal_fflags = {
    {"SecurityChecksEnabled1", true},
    {"InstanceNewEnabled", false},
    {"IdentityOverrides", true},
    {"IdentityOverrridesChecksSecurity1", false},
};

void InitFFlags() {
    return; // Don't init, already pre-inited.
    if (__internal_fflags.size() == 0)
    {
        __internal_fflags["SecurityChecksEnabled1"] = true;
        __internal_fflags["InstanceNewEnabled"] = false;
        __internal_fflags["IdentityOverrides"] = true;
    }
    else
    {
        throw "Cannot init FFlags more than once";
    }
}

bool getFFlag(std::string fflag)
{
    return __internal_fflags.at(fflag);
}

bool setFFlag(const char* fflag, bool value) {
    if (__internal_fflags.find(fflag) == __internal_fflags.end())
    {
        return false;
    }
    __internal_fflags[fflag] = value;
    return true;
}

static void requireIdentity(lua_State* L, char* name, int requiredIdentity, bool forcedError = false)
{
    /*if (!getFFlag("SecurityChecksEnabled1")) // Broken?
    {
        return;
    }*/
    int currentIdentity = L->identity;
    if (currentIdentity < requiredIdentity || forcedError)
    {
        std::string s1 = std::to_string(currentIdentity);
        std::string s2 = std::to_string(requiredIdentity);
        char const* s1c = s1.c_str();
        char const* s2c = s2.c_str();
        error(L, strdup(("The current identity (" + s1 + ") cannot " + name + " (lacking permission " + s2 + ")").c_str()));
    }
}

// RbxGame - Workspace (Workspace)
static Instance* workspace = new Instance();

// RbxGame - DataModel (Game)
static bool gameLoaded = false;

static int luaB_game_isLoaded(lua_State* L)
{
    requireIdentity(L, "IsLoaded", 2);
    //int n = lua_gettop(L); /* number of arguments */
    lua_pushboolean(L, gameLoaded);
    return 1;
}

static int luaB_game_shutdown(lua_State* L)
{
    requireIdentity(L, "Shutdown", 3);
    //int n = lua_gettop(L); /* number of arguments */
    error(L, "Shutdown Not Implemented.");
    return 0;
}

static const luaL_Reg gamelib[] = {
    //{"test", test},
    {"IsLoaded", luaB_game_isLoaded},
    {"Shutdown", luaB_game_shutdown},
    {NULL, NULL},
};

/*
** Open gamelib library
*/
int luaopen_gamelib(lua_State* L)
{
    char* privateServerId = "";
    int privateServerOwnerId = 0;
    int gameId = 0;
    int placeId = 0;
    int creatorId = 0;
    char* gameName = "game";

    char* jobId = "";

    luaL_register(L, LUA_GAMELIBNAME, gamelib);

    lua_pushnumber(L, gameId);
    lua_setfield(L, -2, "GameId");

    lua_pushnumber(L, creatorId);
    lua_setfield(L, -2, "CreatorId");

    lua_pushstring(L, gameName);
    lua_setfield(L, -2, "Name");

    lua_pushstring(L, jobId);
    lua_setfield(L, -2, "JobId");

    lua_pushnumber(L, placeId);
    lua_setfield(L, -2, "PlaceId");

    lua_pushstring(L, privateServerId);
    lua_setfield(L, -2, "PrivateServerId");

    lua_pushnumber(L, privateServerOwnerId);
    lua_setfield(L, -2, "PrivateServerOwnerId");

    lua_pushstring(L, privateServerId);
    lua_setfield(L, -2, "VIPServerId");

    lua_pushnumber(L, privateServerOwnerId);
    lua_setfield(L, -2, "VIPServerOwnerId");

    return 1;
}

// RbxInstance - Instance
static int luaB_instance_new(lua_State* L)
{
    requireIdentity(L, "new", 2);
    if (!getFFlag("InstanceNewEnabled"))
    {
        error(L, "Instance.new Not Implemented.");
        return 0;
    }
    int n = lua_gettop(L); /* number of arguments */
    if (n == 1)
    {
        int t = lua_type(L, 2);
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_argexpected(L, t == LUA_TSTRING, 1, "string");

        const char* classname = luaL_checkstring(L, 1);
        if (classname == "Instance")
        {
            Instance* instance = new Instance(L);
            return 1;
        }
    }

    return 0;
}

static const luaL_Reg instlib[] = {
    //{"test", test},
    {"new", luaB_instance_new},
    {NULL, NULL},
};

/*
** Open instlib library
*/
int luaopen_instlib(lua_State* L)
{
    luaL_register(L, LUA_INSTLIBNAME, instlib);
    return 1;
}


// MRBXLIB - BASE
static int luaB_mrbxlib_setidentity(lua_State* L)
{
    if (!getFFlag("IdentityOverrides"))
    {
        error(L, "SetIdentity is disabled.");
        return 0;
    }

    if (getFFlag("IdentityOverrridesChecksSecurity1"))
    {
        requireIdentity(L, "SetIdentity", 8);
    }

    int t = lua_type(L, 2);
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_argexpected(L, t == LUA_TNUMBER, 1, "int");

    int identity = luaL_checkinteger(L, 2);

    if (identity < 0 || identity > 8)
    {
        error(L, "Cannot SetIdentity (Invalid IdentityLevel)");
        return 0;
    }

    L->identity = identity;

    return 0;
}

static const luaL_Reg mrbxlib[] = {
    //{"test", test},
    {"SetIdentity", luaB_mrbxlib_setidentity},
    {NULL, NULL},
};

/*
** Open mrbxlib library
*/
int luaopen_mrbxlib(lua_State* L)
{
    /* Init FFlags */
    InitFFlags();

    luaL_register(L, LUA_MRBXLIBNAME, mrbxlib);
    lua_pushstring(L, "v0.0.1");
    lua_setfield(L, -2, "version");
    return 1;
}
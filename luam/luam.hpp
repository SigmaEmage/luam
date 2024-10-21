// luam.hpp : Defines the entry point for the library.
//

#include "luam.h"
#include "lrbx.h"
#include "Luau/CodeGen.h"
#include <map>
#ifdef CALLGRIND
#include <valgrind/callgrind.h>
#endif

using AddCompletionCallback = std::function<void(const std::string& completion, const std::string& display)>;
static bool codegen = false;

using namespace std;

unsigned long long timeSinceEpochMS()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

unsigned long long timeSinceEpochS()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

double timeSinceEpoch()
{
    using namespace std::chrono;
    return ((double)(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()) / 1000);
}

static void stack_dump(lua_State* L, const char* stackname) {
    int i;
    int top = lua_gettop(L);
    printf("--------------- %s STACK ---------------\n", stackname);
    for (i = top; i >= 1; i--) {
        int t = lua_type(L, i);
        printf("[%2d - %8s] : ", i, lua_typename(L, t));
        switch (t) {
        case LUA_TSTRING:
            printf("%s", lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            printf("%g", lua_tonumber(L, i));
            break;
        case LUA_TNIL:
            printf("nil");
            break;
        case LUA_TNONE:
            printf("none");
            break;
        case LUA_TFUNCTION:
            printf("<function %p>", lua_topointer(L, i));
            break;
        case LUA_TTABLE:
            printf("<table %p>", lua_topointer(L, i));
            break;
        case LUA_TTHREAD:
            printf("<thread %p>", lua_topointer(L, i));
            break;
        case LUA_TUSERDATA:
            printf("<userdata %p>", lua_topointer(L, i));
            break;
        case LUA_TLIGHTUSERDATA:
            printf("<lightuserdata %p>", lua_topointer(L, i));
            break;
        default:
            printf("unknown %s", lua_typename(L, t));
            break;
        }
        printf("\n");
    }
    printf("--------------- %s STACK ---------------\n", stackname);
}

void setupState(lua_State* L);

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
void Color(int color)
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

static void writestring(const char* s, size_t l)
{
    fwrite(s, 1, l, stdout);
}

static int luaB_print(lua_State* L)
{
    int n = lua_gettop(L); /* number of arguments */
    Color(7);
    for (int i = 1; i <= n; i++)
    {
        size_t l;
        const char* s = luaL_tolstring(L, i, &l); /* convert to string using __tostring et al */
        if (i > 1)
            writestring("\t", 1);
        writestring(s, l);
        lua_pop(L, 1); /* pop result */
    }
    writestring("\n", 1);
    Color(7);
    return 0;
}

static int luaB_printidentity(lua_State* L)
{
    char* start = "Current identity is";
    int n = lua_gettop(L); /* number of arguments */

    if (n > 0)
    {
        int t = lua_type(L, 1);
        // luaL_checktype(L, 1, LUA_TSTRING);
        luaL_argexpected(L, t == LUA_TNIL || t == LUA_TSTRING, 1, "nil or string");

        if (lua_isstring(L, 1))
        {
            start = strdup(lua_tostring(L, 1));
        }
    }

    Color(7);
    char* full = strdup((std::string(start) + " " + std::to_string(L->identity)).c_str());
    writestring(full, strlen(full));
    writestring("\n", 1);
    Color(7);
    return 0;
}

static int luaB_warn(lua_State* L)
{
    int n = lua_gettop(L); /* number of arguments */
    Color(6);
    for (int i = 1; i <= n; i++)
    {
        size_t l;
        const char* s = luaL_tolstring(L, i, &l); /* convert to string using __tostring et al */
        if (i > 1)
            writestring("\t", 1);
        writestring(s, l);
        lua_pop(L, 1); /* pop result */
    }
    writestring("\n", 1);
    Color(7);
    return 0;
}

std::list<lua_State*> lstates;

// task scheduler thread
bool taskSchedulerRunning = false;
void taskScheduler()
{
    taskSchedulerRunning = true;
	while (taskSchedulerRunning)
	{
		double now = timeSinceEpoch();
        for (auto& L : lstates) {
            taskSchedulerInfo* tsinfo = L->taskScheduler;
            std::list<long long> toRemove;
            for (auto& thread : tsinfo->sleepingThreads)
            {
                taskSleepInfo* info = tsinfo->sleepingThreadTimings[thread.first];
                if (info->wakeUpTime <= now)
                {
                    lua_State* co = thread.second;
                    if (info->sendTimeTaken) {
                        lua_pushnumber(co, timeSinceEpoch() - info->startTime);
                    }
                    if (info->callType == CallType::Resume) {
                        lua_resume(co, tsinfo->sleepingThreadParentStates[thread.first], info->sendTimeTaken ? 1 : 0);
                    }
                    else if (info->callType == CallType::Call) {
                        lua_call(co, info->sendTimeTaken ? 1 : 0, 0);
                    }
                    toRemove.push_back(thread.first);
                }
            }
            for (auto& thread : toRemove)
            {
                tsinfo->sleepingThreads.erase(thread);
                tsinfo->sleepingThreadParentStates.erase(thread);
                tsinfo->sleepingThreadTimings.erase(thread);
            }
        }
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void startTaskScheduler()
{
	if (!taskSchedulerRunning) {
		std::thread t(taskScheduler);
		t.detach();
	}
}

void stopTaskScheduler()
{
	taskSchedulerRunning = false;
}

static int luaB_wait(lua_State* L)
{
    int n = lua_gettop(L);

    double start = timeSinceEpoch();

    double s = luaL_checknumber(L, 1);

    if (L->global->mainthread == L) {
        long long ms = floor(s * 1000);
        lua_pop(L, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));

        lua_pushnumber(L, timeSinceEpoch() - start);
        return 1;
    }
    else {
        taskSchedulerInfo* tsinfo = L->taskScheduler;
        tsinfo->sleepingThreads[tsinfo->lastThreadId++] = L;
        tsinfo->sleepingThreadParentStates[tsinfo->lastThreadId - 1] = L->global->mainthread;
        taskSleepInfo* tsi = new taskSleepInfo();
        tsi->callType = CallType::Resume;
        tsi->startTime = start;
        tsi->wakeUpTime = start + s;
        tsinfo->sleepingThreadTimings[tsinfo->lastThreadId - 1] = tsi;
        return lua_yield(L, 0);
    }
}

static lua_State* lua_cocreate(lua_State* L, int idx = 1) {
    lua_State* NL = lua_newthread(L);
    luaL_argcheck(L, lua_isfunction(L, idx) && !lua_iscfunction(L, idx), idx,
        "Lua function expected");
    lua_pushvalue(L, idx);  /* move function to top */
    lua_xmove(L, NL, idx);  /* move function from L to NL */
    //lua_xpush(L, NL, 1); // push function on top of NL
    return NL;
}

static int luaB_delay(lua_State* L)
{
    int n = lua_gettop(L);

    double start = timeSinceEpoch();
    
    double s = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_State* thread = lua_cocreate(L, 2);

    taskSchedulerInfo* tsinfo = L->taskScheduler;
    tsinfo->sleepingThreads[tsinfo->lastThreadId++] = thread;
    tsinfo->sleepingThreadParentStates[tsinfo->lastThreadId - 1] = L;
    taskSleepInfo* tsi = new taskSleepInfo();
    tsi->callType = CallType::Resume;
    tsi->startTime = start;
    tsi->wakeUpTime = start + s;
    tsinfo->sleepingThreadTimings[tsinfo->lastThreadId - 1] = tsi;

    return 0;
}

static int luaB_tick(lua_State* L)
{
    int n = lua_gettop(L); /* number of arguments */

    lua_pushnumber(L, timeSinceEpoch());
    return 1;
}

struct GlobalOptions
{
	int optimizationLevel = 1;
	int debugLevel = 1;
} globalOptions2;

static Luau::CompileOptions copts()
{
	Luau::CompileOptions result = {};
	result.optimizationLevel = globalOptions2.optimizationLevel;
	result.debugLevel = globalOptions2.debugLevel;
	result.coverageLevel = coverageActive() ? 2 : 0;

	return result;
}

std::string Compile(std::string code) {
	return Luau::compile(code, copts());
}

enum class CliMode
{
    Unknown,
    Repl,
    Compile,
    RunSourceFiles
};

enum class CompileFormat
{
    Text,
    Binary,
    Remarks,
    Codegen,        // Prints annotated native code including IR and assembly
    CodegenAsm,     // Prints annotated native code assembly
    CodegenIr,      // Prints annotated native code IR
    CodegenVerbose, // Prints annotated native code including IR, assembly and outlined code
    CodegenNull,
    Null
};

constexpr int MaxTraversalLimit = 50;

struct GlobalOptions2
{
    int optimizationLevel = 1;
    int debugLevel = 1;
} globalOptions;

static int lua_loadstring(lua_State* L)
{
    size_t l = 0;
    const char* s = luaL_checklstring(L, 1, &l);
    const char* chunkname = luaL_optstring(L, 2, s);

    lua_setsafeenv(L, LUA_ENVIRONINDEX, false);

    std::string bytecode = Luau::compile(std::string(s, l), copts());
    if (luau_load(L, chunkname, bytecode.data(), bytecode.size(), 0) == 0)
        return 1;

    lua_pushnil(L);
    lua_insert(L, -2); /* put before error message */
    return 2;          /* return nil plus error message */
}

static int finishrequire(lua_State* L)
{
    if (lua_isstring(L, -1))
        lua_error(L);

    return 1;
}

static int lua_require(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    std::string chunkname = "=" + name;

    luaL_findtable(L, LUA_REGISTRYINDEX, "_MODULES", 1);

    // return the module from the cache
    lua_getfield(L, -1, name.c_str());
    if (!lua_isnil(L, -1))
    {
        // L stack: _MODULES result
        return finishrequire(L);
    }

    lua_pop(L, 1);

    std::optional<std::string> source = readFile(name + ".luam");
    if (!source)
    {
        source = readFile(name + ".lua"); // try .lua if .luam doesn't exist
        if (!source)
            luaL_argerrorL(L, 1, ("error loading " + name).c_str()); // if neither .luau nor .lua exist, we have an error
    }

    // module needs to run in a new thread, isolated from the rest
    // note: we create ML on main thread so that it doesn't inherit environment of L
    lua_State* GL = lua_mainthread(L);
    lua_State* ML = lua_newthread(GL);
    lua_xmove(GL, L, 1);

    // new thread needs to have the globals sandboxed
    luaL_sandboxthread(ML);

    // now we can compile & run module on the new thread
    std::string bytecode = Luau::compile(*source, copts());
    if (luau_load(ML, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) == 0)
    {
        if (codegen)
            Luau::CodeGen::compile(ML, -1);

        if (coverageActive())
            coverageTrack(ML, -1);

        int status = lua_resume(ML, L, 0);

        if (status == 0)
        {
            if (lua_gettop(ML) == 0)
                lua_pushstring(ML, "module must return a value");
            else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1))
                lua_pushstring(ML, "module must return a table or function");
        }
        else if (status == LUA_YIELD)
        {
            lua_pushstring(ML, "module can not yield");
        }
        else if (!lua_isstring(ML, -1))
        {
            lua_pushstring(ML, "unknown error while running module");
        }
    }

    // there's now a return value on top of ML; L stack: _MODULES ML
    lua_xmove(ML, L, 1);
    lua_pushvalue(L, -1);
    lua_setfield(L, -4, name.c_str());

    // L stack: _MODULES ML result
    return finishrequire(L);
}

static int lua_collectgarbage(lua_State* L)
{
    const char* option = luaL_optstring(L, 1, "collect");

    if (strcmp(option, "collect") == 0)
    {
        lua_gc(L, LUA_GCCOLLECT, 0);
        return 0;
    }

    if (strcmp(option, "count") == 0)
    {
        int c = lua_gc(L, LUA_GCCOUNT, 0);
        lua_pushnumber(L, c);
        return 1;
    }

    luaL_error(L, "collectgarbage must be called with 'count' or 'collect'");
}

static const luaL_Reg lualibs[] = {
    {LUA_GAMELIBNAME, luaopen_gamelib},
    {LUA_INSTLIBNAME, luaopen_instlib},
    {LUA_MRBXLIBNAME, luaopen_mrbxlib},

    {NULL, NULL},
};

void luaL_openlibs2(lua_State* L) {
    const luaL_Reg* lib = lualibs;
    for (; lib->func; lib++)
    {
        lua_pushcfunction(L, lib->func, NULL);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}

#ifdef CALLGRIND
static int lua_callgrind(lua_State* L)
{
    const char* option = luaL_checkstring(L, 1);

    if (strcmp(option, "running") == 0)
    {
        int r = RUNNING_ON_VALGRIND;
        lua_pushboolean(L, r);
        return 1;
    }

    if (strcmp(option, "zero") == 0)
    {
        CALLGRIND_ZERO_STATS;
        return 0;
    }

    if (strcmp(option, "dump") == 0)
    {
        const char* name = fluaL_checkstring(L, 2);

        CALLGRIND_DUMP_STATS_AT(name);
        return 0;
    }

    luaL_error(L, "callgrind must be called with one of 'running', 'zero', 'dump'");
}
#endif

void setupState(lua_State* L)
{
    luaL_openlibs(L);
    luaL_openlibs2(L);

    static const luaL_Reg funcs[] = {
        {"loadstring", lua_loadstring},
        {"require", lua_require},
        {"collectgarbage", lua_collectgarbage},
#ifdef CALLGRIND
        {"callgrind", lua_callgrind},
#endif

        // Luam Globals //
        {"warn", luaB_warn},
        {"printidentity", luaB_printidentity},
        {"tick", luaB_tick},
        {"wait", luaB_wait},
        {"delay", luaB_delay},
        
        {NULL, NULL},
    };

    lua_pushvalue(L, LUA_GLOBALSINDEX);
    luaL_register(L, NULL, funcs);
    lua_pop(L, 1);

    L->taskScheduler = new taskSchedulerInfo(); // Allocate task scheduler info.

    luaL_sandbox(L);
    
    lstates.push_back(L);
}

void closeState(lua_State* L) {
    lstates.erase(std::remove(lstates.begin(), lstates.end(), L), lstates.end());
	delete L->taskScheduler;
    lua_close(L);
}

std::string runCode(lua_State* L, const std::string& source)
{
    std::string bytecode = Luau::compile(source, copts());

    if (luau_load(L, "=stdin", bytecode.data(), bytecode.size(), 0) != 0)
    {
        size_t len;
        const char* msg = lua_tolstring(L, -1, &len);

        std::string error(msg, len);
        lua_pop(L, 1);

        return error;
    }

    if (codegen)
        Luau::CodeGen::compile(L, -1);

    lua_State* T = lua_newthread(L);

    T->identity = 2; // Default Identity.
    //T->taskScheduler = L->taskScheduler; // Reference the task scheduler info.

    lua_pushvalue(L, -2);
    lua_remove(L, -3);
    lua_xmove(L, T, 1);

    int status = lua_resume(T, NULL, 0);

    if (status == 0)
    {
        int n = lua_gettop(T);

        if (n)
        {
            Color(4);
            luaL_checkstack(T, LUA_MINSTACK, "too many results to print");
            lua_getglobal(T, "_PRETTYPRINT");
            // If _PRETTYPRINT is nil, then use the standard print function instead
            if (lua_isnil(T, -1))
            {
                lua_pop(T, 1);
                lua_getglobal(T, "print");
            }
            lua_insert(T, 1);
            lua_pcall(T, n, 0, 0);
            Color(7);
        }
    }
    else
    {
        Color(4);
        std::string error;

        if (status == LUA_YIELD)
        {
            error = "thread yielded unexpectedly";
        }
        else if (const char* str = lua_tostring(T, -1))
        {
            error = str;
        }

        error += "\nstack backtrace:\n";
        error += lua_debugtrace(T);

        fprintf(stdout, "%s", error.c_str());
        Color(7);
    }
    lua_pop(L, 1);
    return std::string();
}

inline bool file_exists(const std::string& name) {
    ifstream f(name.c_str());
    return f.good();
}
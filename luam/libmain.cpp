// main.cpp : Defines the entry point for the application.
//

#include "luam.hpp"

#include "lua.h"
#include "lualib.h"

#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Parser.h"

#include "Coverage.h"
#include "FileUtils.h"
#include "Flags.h"
#include "Profiler.h"

#include "isocline.h"

#include <memory>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <locale.h>
#include <signal.h>

LUAU_FASTFLAG(DebugLuauTimeTracing)

// Ctrl-C handling
static void sigintCallback(lua_State* L, int gc)
{
    if (gc >= 0)
        return;

    lua_callbacks(L)->interrupt = NULL;

    stopTaskScheduler();

    lua_rawcheckstack(L, 1); // reserve space for error string
    luaL_error(L, "Execution interrupted");
}

static lua_State* replState = NULL;

#ifdef _WIN32
BOOL WINAPI sigintHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT && replState)
        lua_callbacks(replState)->interrupt = &sigintCallback;
    return TRUE;
}
#else
static void sigintHandler(int signum)
{
    if (signum == SIGINT && replState)
        lua_callbacks(replState)->interrupt = &sigintCallback;
}
#endif

// `repl` is used it indicate if a repl should be started after executing the file.
static bool runFile(const char* name, lua_State* GL, bool repl)
{
    std::optional<std::string> source = readFile(name);
    if (!source)
    {
        fprintf(stderr, "Error opening %s\n", name);
        return false;
    }

    // module needs to run in a new thread, isolated from the rest
    lua_State* L = lua_newthread(GL);

    // new thread needs to have the globals sandboxed
    luaL_sandboxthread(L);

    std::string chunkname = "=" + std::string(name);

    std::string bytecode = Luau::compile(*source, copts());
    int status = 0;

    if (luau_load(L, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) == 0)
    {
        if (codegen)
            Luau::CodeGen::compile(L, -1);

        if (coverageActive())
            coverageTrack(L, -1);

        status = lua_resume(L, NULL, 0);
    }
    else
    {
        status = LUA_ERRSYNTAX;
    }

    if (status != 0)
    {
        std::string error;

        if (status == LUA_YIELD)
        {
            error = "thread yielded unexpectedly";
        }
        else if (const char* str = lua_tostring(L, -1))
        {
            error = str;
        }

        error += "\nstacktrace:\n";
        error += lua_debugtrace(L);

        fprintf(stderr, "%s", error.c_str());
    }
    
    lua_pop(GL, 1);
    return status == 0;
}

static void report(const char* name, const Luau::Location& location, const char* type, const char* message)
{
    fprintf(stderr, "%s(%d,%d): %s: %s\n", name, location.begin.line + 1, location.begin.column + 1, type, message);
}

static void reportError(const char* name, const Luau::ParseError& error)
{
    report(name, error.getLocation(), "SyntaxError", error.what());
}

static void reportError(const char* name, const Luau::CompileError& error)
{
    report(name, error.getLocation(), "CompileError", error.what());
}

static std::string getCodegenAssembly(const char* name, const std::string& bytecode, Luau::CodeGen::AssemblyOptions options)
{
    std::unique_ptr<lua_State, void (*)(lua_State*)> globalState(luaL_newstate(), lua_close);
    lua_State* L = globalState.get();

    if (luau_load(L, name, bytecode.data(), bytecode.size(), 0) == 0)
        return Luau::CodeGen::getAssembly(L, -1, options);

    fprintf(stderr, "Error loading bytecode %s\n", name);
    return "";
}

static void annotateInstruction(void* context, std::string& text, int fid, int instpos)
{
    Luau::BytecodeBuilder& bcb = *(Luau::BytecodeBuilder*)context;

    bcb.annotateInstruction(text, fid, instpos);
}

struct CompileStats
{
    size_t lines;
    size_t bytecode;
    size_t codegen;
};

static bool compileFile(const char* name, CompileFormat format, CompileStats& stats)
{
    std::optional<std::string> source = readFile(name);
    if (!source)
    {
        fprintf(stderr, "Error opening %s\n", name);
        return false;
    }

    // NOTE: Normally, you should use Luau::compile or luau_compile (see lua_require as an example)
    // This function is much more complicated because it supports many output human-readable formats through internal interfaces

    try
    {
        Luau::BytecodeBuilder bcb;

        Luau::CodeGen::AssemblyOptions options;
        options.outputBinary = format == CompileFormat::CodegenNull;

        if (!options.outputBinary)
        {
            options.includeAssembly = format != CompileFormat::CodegenIr;
            options.includeIr = format != CompileFormat::CodegenAsm;
            options.includeOutlinedCode = format == CompileFormat::CodegenVerbose;
        }

        options.annotator = annotateInstruction;
        options.annotatorContext = &bcb;

        if (format == CompileFormat::Text)
        {
            bcb.setDumpFlags(Luau::BytecodeBuilder::Dump_Code | Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Locals |
                Luau::BytecodeBuilder::Dump_Remarks);
            bcb.setDumpSource(*source);
        }
        else if (format == CompileFormat::Remarks)
        {
            bcb.setDumpFlags(Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Remarks);
            bcb.setDumpSource(*source);
        }
        else if (format == CompileFormat::Codegen || format == CompileFormat::CodegenAsm || format == CompileFormat::CodegenIr ||
            format == CompileFormat::CodegenVerbose)
        {
            bcb.setDumpFlags(Luau::BytecodeBuilder::Dump_Code | Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Locals |
                Luau::BytecodeBuilder::Dump_Remarks);
            bcb.setDumpSource(*source);
        }

        Luau::Allocator allocator;
        Luau::AstNameTable names(allocator);
        Luau::ParseResult result = Luau::Parser::parse(source->c_str(), source->size(), names, allocator);

        if (!result.errors.empty())
            throw Luau::ParseErrors(result.errors);

        stats.lines += result.lines;

        Luau::compileOrThrow(bcb, result, names, copts());
        stats.bytecode += bcb.getBytecode().size();

        switch (format)
        {
        case CompileFormat::Text:
            printf("%s", bcb.dumpEverything().c_str());
            break;
        case CompileFormat::Remarks:
            printf("%s", bcb.dumpSourceRemarks().c_str());
            break;
        case CompileFormat::Binary:
            fwrite(bcb.getBytecode().data(), 1, bcb.getBytecode().size(), stdout);
            break;
        case CompileFormat::Codegen:
        case CompileFormat::CodegenAsm:
        case CompileFormat::CodegenIr:
        case CompileFormat::CodegenVerbose:
            printf("%s", getCodegenAssembly(name, bcb.getBytecode(), options).c_str());
            break;
        case CompileFormat::CodegenNull:
            stats.codegen += getCodegenAssembly(name, bcb.getBytecode(), options).size();
            break;
        case CompileFormat::Null:
            break;
        }

        return true;
    }
    catch (Luau::ParseErrors& e)
    {
        for (auto& error : e.getErrors())
            reportError(name, error);
        return false;
    }
    catch (Luau::CompileError& e)
    {
        reportError(name, e);
        return false;
    }
}

extern "C" const char* executeScript(const char* source)
{
    startTaskScheduler();
    // setup flags
    for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
        if (strncmp(flag->name, "Luau", 4) == 0)
            flag->value = true;

    // create new state
    std::unique_ptr<lua_State, void (*)(lua_State*)> globalState(luaL_newstate(), lua_close);
    lua_State* L = globalState.get();

    // setup state
    setupState(L);

    // sandbox thread
    luaL_sandboxthread(L);

    // static string for caching result (prevents dangling ptr on function exit)
    static std::string result;

    // run code + collect error
    result = runCode(L, source);

    return result.empty() ? NULL : result.c_str();
}
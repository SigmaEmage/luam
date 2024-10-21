// luam.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>

// TODO: Reference additional headers your program requires here.
#include <fstream>
#include <string>
#include <chrono>
#include <optional>
#include <thread>

#ifdef _WIN32 // windows
#ifndef OS_WINDOWS
#define OS_WINDOWS
#endif
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

#include "../luau/VM/include/lua.h"
#include "../luau/VM/include/lualib.h"
#include "../luau/Compiler/include/Luau/Compiler.h"
#include "Coverage.h"
#include "FileUtils.h"
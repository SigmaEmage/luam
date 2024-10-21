# CMakeList.txt : CMake project for luam, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    enable_language("RC")
    set (WIN32_RESOURCES "${CMAKE_SOURCE_DIR}/appicon.rc")
endif()

set (BUILD_EXE ON)
set (BUILD_LIB ON)

if (BUILD_EXE)
    # Add source to this project's executable.
    add_executable (luam "luam.hpp" "luam.h" "main.cpp" "FileUtils.cpp" "FileUtils.h" "Coverage.cpp" "Coverage.h" "lrbx.cpp"  "lrbx.h" ${WIN32_RESOURCES} "Flags.cpp" "Flags.h" "Profiler.cpp" "Profiler.h")

    target_compile_features(luam PUBLIC cxx_std_17)

    # TODO: Add tests and install targets if needed.
    target_include_directories(luam PUBLIC RBX ../luau/Ast/include ../luau/extern/isocline/include)
    target_link_libraries(luam PUBLIC Luau.Analysis Luau.Ast Luau.CodeGen Luau.Common Luau.Compiler Luau.VM isocline)
endif()

if (BUILD_LIB)
    add_library (luamlib "luam.hpp" "luam.h" "libmain.cpp" "FileUtils.cpp" "FileUtils.h" "Coverage.cpp" "Coverage.h" "lrbx.cpp"  "lrbx.h" ${WIN32_RESOURCES} "Flags.cpp" "Flags.h" "Profiler.cpp" "Profiler.h")

    target_compile_features(luamlib PUBLIC cxx_std_17)

    # TODO: Add tests and install targets if needed.
    target_include_directories(luamlib PUBLIC RBX ../luau/Ast/include)
    target_link_libraries(luamlib PUBLIC Luau.Analysis Luau.Ast Luau.CodeGen Luau.Common Luau.Compiler Luau.VM isocline)
endif()
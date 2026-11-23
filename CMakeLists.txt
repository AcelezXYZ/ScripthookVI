cmake_minimum_required(VERSION 3.20)
project(ScriptHookVI LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "ScriptHookVI targets Windows x64. Cross-compile with -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "64-bit build required")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(MSVC)
  add_compile_options(/W4 /permissive- /EHa)   # /EHa: catch(...) also sees SEH faults
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  add_compile_options(-Wall -Wextra)
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

# --- core hook: ScriptHookVI.dll --------------------------------------------
add_library(ScriptHookVI SHARED
  src/dllmain.cpp src/exports.cpp
  src/core/log.cpp src/core/config.cpp src/core/paths.cpp src/core/memory.cpp
  src/game/game_api.cpp
  src/runtime/native_invoker.cpp src/runtime/script_manager.cpp
  src/runtime/input.cpp src/runtime/asi_loader.cpp)
target_include_directories(ScriptHookVI PRIVATE sdk/inc)
set_target_properties(ScriptHookVI PROPERTIES PREFIX "")   # ScriptHookVI.dll / ScriptHookVI.lib

# --- loader: dinput8.dll proxy ------------------------------------------------
add_library(dinput8_proxy SHARED src/proxy/dinput8_proxy.cpp)
set_target_properties(dinput8_proxy PROPERTIES PREFIX "" OUTPUT_NAME "dinput8")

# --- SDK for plugin authors ---------------------------------------------------
add_library(ScriptHookVI_SDK INTERFACE)
target_include_directories(ScriptHookVI_SDK INTERFACE sdk/inc)
target_link_libraries(ScriptHookVI_SDK INTERFACE ScriptHookVI)

# --- sample plugin: hello.asi -------------------------------------------------
add_library(hello SHARED samples/hello/hello.cpp)
target_link_libraries(hello PRIVATE ScriptHookVI_SDK)
set_target_properties(hello PROPERTIES PREFIX "" SUFFIX ".asi")

# --- install layout -----------------------------------------------------------
install(TARGETS ScriptHookVI dinput8_proxy RUNTIME DESTINATION .)
install(FILES config/ScriptHookVI.ini DESTINATION .)
install(DIRECTORY sdk/inc/ DESTINATION sdk/inc)
install(TARGETS ScriptHookVI ARCHIVE DESTINATION sdk/lib)

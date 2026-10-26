cmake_minimum_required(VERSION 3.28)

project(SaveStateTransfer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# --- dependencies (all source-fetched; no vcpkg) --------------------------------------------
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
set(SPDLOG_USE_STD_FORMAT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.3
    GIT_SHALLOW    TRUE
    OVERRIDE_FIND_PACKAGE)

FetchContent_Declare(rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG        v8.84
    GIT_SHALLOW    TRUE)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE)

FetchContent_Declare(simpleini
    GIT_REPOSITORY https://github.com/brofield/simpleini.git
    GIT_TAG        v4.22
    GIT_SHALLOW    TRUE)

set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(CommonLibSSE
    GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
    GIT_TAG        v3.7.0
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(spdlog rapidcsv nlohmann_json simpleini)
# CommonLibSSE-NG does find_path(RAPIDCSV_INCLUDE_DIRS "rapidcsv.h"); point it at the fetched header.
set(RAPIDCSV_INCLUDE_DIRS "${rapidcsv_SOURCE_DIR}/src" CACHE PATH "" FORCE)
FetchContent_MakeAvailable(CommonLibSSE)
target_include_directories(CommonLibSSE SYSTEM PUBLIC "${rapidcsv_SOURCE_DIR}/src")

# --- plugin ----------------------------------------------------------------------------------
configure_file(src/Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/Version.h" @ONLY)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/app/Requests.cpp
    src/core/Paths.cpp
    src/core/Config.cpp
    src/core/FormKey.cpp
    src/trigger/Hotkeys.cpp
    src/trigger/TriggerFile.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/include" "${simpleini_SOURCE_DIR}")
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE spdlog::spdlog nlohmann_json::nlohmann_json)
target_compile_definitions(${PROJECT_NAME} PRIVATE SST_VERSION_STRING="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /EHsc /utf-8 /permissive- /Zc:preprocessor /W4
        $<$<CXX_COMPILER_ID:Clang>:-Wno-unknown-pragmas -Wno-unused-parameter -Wno-microsoft-include>)
    target_link_options(${PROJECT_NAME} PRIVATE /DEBUG:FULL)
endif()

# --- staging: build/<preset>/stage mirrors the MO2 mod folder layout --------------------------
set(STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/stage")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGE_DIR}/SKSE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/dist" "${STAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${STAGE_DIR}/SKSE/Plugins/"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${STAGE_DIR}/SKSE/Plugins/"
    COMMENT "Staging mod folder at ${STAGE_DIR}")

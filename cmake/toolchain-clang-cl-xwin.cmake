# Cross-compile Windows x64 DLLs from Linux with clang-cl + lld-link + an xwin splat of the
# MSVC CRT and Windows SDK (default location ~/xwin, override with -DXWIN_ROOT=...).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT DEFINED XWIN_ROOT)
    set(XWIN_ROOT "$ENV{HOME}/xwin" CACHE PATH "xwin splat directory (crt/ and sdk/)")
endif()
if(NOT EXISTS "${XWIN_ROOT}/crt/include" OR NOT EXISTS "${XWIN_ROOT}/sdk/include")
    message(FATAL_ERROR "xwin splat not found at ${XWIN_ROOT}; run: xwin --accept-license splat --output ${XWIN_ROOT}")
endif()

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

# Header search paths (-imsvc = system include, suppresses warnings from these headers).
set(_xwin_inc
    "-imsvc${XWIN_ROOT}/crt/include"
    "-imsvc${XWIN_ROOT}/sdk/include/ucrt"
    "-imsvc${XWIN_ROOT}/sdk/include/um"
    "-imsvc${XWIN_ROOT}/sdk/include/shared"
    "-imsvc${XWIN_ROOT}/sdk/include/winrt"
    "-imsvc${XWIN_ROOT}/sdk/include/cppwinrt")
string(JOIN " " _xwin_inc_str ${_xwin_inc})
set(CMAKE_C_FLAGS_INIT "${_xwin_inc_str}")
set(CMAKE_CXX_FLAGS_INIT "${_xwin_inc_str}")

set(_xwin_lib
    "/libpath:${XWIN_ROOT}/crt/lib/x86_64"
    "/libpath:${XWIN_ROOT}/sdk/lib/um/x86_64"
    "/libpath:${XWIN_ROOT}/sdk/lib/ucrt/x86_64")
string(JOIN " " _xwin_lib_str ${_xwin_lib})
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_xwin_lib_str}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_xwin_lib_str}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_xwin_lib_str}")

# Never look for Linux-side libraries/headers when cross-compiling.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

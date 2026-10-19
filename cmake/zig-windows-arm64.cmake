# Windows on ARM64 cross build with Zig (pip install ziglang) as the C/C++
# compiler and llvm-windres for resources. Wrapper scripts live in
# HM_ZIG_WRAPPERS (default: ../deps/zig-arm64 next to the project).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)
if(NOT DEFINED HM_ZIG_WRAPPERS)
  set(HM_ZIG_WRAPPERS "${CMAKE_CURRENT_LIST_DIR}/../../deps/zig-arm64")
endif()
set(CMAKE_C_COMPILER   "${HM_ZIG_WRAPPERS}/cc")
set(CMAKE_CXX_COMPILER "${HM_ZIG_WRAPPERS}/c++")
set(CMAKE_AR           "${HM_ZIG_WRAPPERS}/ar" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       "${HM_ZIG_WRAPPERS}/ranlib" CACHE FILEPATH "" FORCE)
set(CMAKE_RC_COMPILER  "${HM_ZIG_WRAPPERS}/rc")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

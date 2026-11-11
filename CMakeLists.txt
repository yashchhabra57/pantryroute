cmake_minimum_required(VERSION 3.24)
project(pantryroute VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PANTRYROUTE_BUILD_TESTS "Build the Catch2 test suite" ON)
option(PANTRYROUTE_BUILD_BENCH "Build the solver benchmark" ON)
option(PANTRYROUTE_SANITIZE "Build with AddressSanitizer + UBSan (GCC/Clang, non-Windows)" OFF)

# Shared warning/sanitizer settings, applied to every target we own.
add_library(pantryroute_options INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(pantryroute_options INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Wold-style-cast -Wnull-dereference -Werror)
  if(PANTRYROUTE_SANITIZE)
    target_compile_options(pantryroute_options INTERFACE
      -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(pantryroute_options INTERFACE -fsanitize=address,undefined)
  endif()
elseif(MSVC)
  target_compile_options(pantryroute_options INTERFACE /W4 /WX /permissive-)
endif()

# MinGW: link the runtime statically so the binaries don't pick up a
# mismatched libstdc++ DLL from somewhere else on PATH.
function(pantryroute_static_runtime target)
  if(MINGW)
    target_link_options(${target} PRIVATE -static)
  endif()
endfunction()

add_library(pantryroute_core STATIC
  src/geo.cpp
  src/clock.cpp
  src/csv.cpp
  src/model.cpp
  src/loader.cpp
  src/route.cpp
  src/savings.cpp
  src/insertion.cpp
  src/local_search.cpp
  src/solver.cpp
  src/json.cpp
  src/report.cpp
)
target_include_directories(pantryroute_core PUBLIC include)
target_link_libraries(pantryroute_core PUBLIC pantryroute_options)

if(PANTRYROUTE_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(Catch2)

  enable_testing()
  add_executable(pantryroute_tests
    tests/test_geo.cpp
    tests/test_clock.cpp
    tests/test_csv.cpp
    tests/test_loader.cpp
    tests/test_route.cpp
    tests/test_savings.cpp
    tests/test_insertion.cpp
    tests/test_local_search.cpp
    tests/test_solver.cpp
    tests/test_json.cpp
    tests/test_report.cpp
  )
  target_link_libraries(pantryroute_tests PRIVATE pantryroute_core Catch2::Catch2WithMain)
  pantryroute_static_runtime(pantryroute_tests)

  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)
  catch_discover_tests(pantryroute_tests)
endif()

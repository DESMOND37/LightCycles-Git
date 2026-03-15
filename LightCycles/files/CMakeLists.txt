cmake_minimum_required(VERSION 3.15)
project(tron_light_cycles LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Curses library ──────────────────────────────────────────────────────────
#
# Windows → PDCurses via vcpkg  (vcpkg install pdcurses)
# Linux   → ncurses             (apt install libncurses5-dev)
# macOS   → ncurses             (brew install ncurses)

if(WIN32)
    # vcpkg sets CMAKE_PREFIX_PATH automatically when you pass the toolchain file.
    find_package(PDCurses CONFIG QUIET)
    if(PDCurses_FOUND)
        set(CURSES_TARGET PDCurses::PDCurses)
    else()
        # Manual fallback (no vcpkg)
        find_path(CURSES_INCLUDE_DIR  curses.h)
        find_library(CURSES_LIBRARY   NAMES pdcurses pdcursesw)
        if(NOT CURSES_INCLUDE_DIR OR NOT CURSES_LIBRARY)
            message(FATAL_ERROR
                "PDCurses not found.\n"
                "Fix: vcpkg install pdcurses\n"
                "Then pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake")
        endif()
    endif()
else()
    set(CURSES_NEED_NCURSES TRUE)
    find_package(Curses REQUIRED)
endif()

# ─── Executable ──────────────────────────────────────────────────────────────

add_executable(tron tron.cpp)

if(WIN32)
    if(PDCurses_FOUND)
        target_link_libraries(tron PRIVATE PDCurses::PDCurses user32 gdi32)
    else()
        target_include_directories(tron PRIVATE ${CURSES_INCLUDE_DIR})
        target_link_libraries(tron      PRIVATE ${CURSES_LIBRARY} user32 gdi32)
    endif()
else()
    target_include_directories(tron PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(tron      PRIVATE ${CURSES_LIBRARIES})
endif()

# ─── Compiler flags ──────────────────────────────────────────────────────────

if(MSVC)
    target_compile_options(tron PRIVATE /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(tron PRIVATE -Wall -Wextra -O2)
endif()

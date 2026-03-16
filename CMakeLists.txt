cmake_minimum_required(VERSION 3.15)
project(LightCycles LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Curses library ──────────────────────────────────────────────────────────
#
# Windows → PDCurses via vcpkg, статическая линковка (без .dll)
#           vcpkg install pdcurses:x64-windows-static
#
# Linux   → ncursesw (wide char)
#           sudo apt install libncursesw5-dev
#
# macOS   → системный ncurses (уже включает wide char)

if(WIN32)
    # Статический трiplет — всё вшивается в .exe, dll не нужен
    find_package(unofficial-pdcurses CONFIG REQUIRED)

elseif(APPLE)
    set(CURSES_NEED_NCURSES TRUE)
    find_package(Curses REQUIRED)

else()
    find_library(NCURSESW_LIB NAMES ncursesw)
    find_path   (NCURSESW_INC NAMES ncursesw/ncurses.h ncurses.h)
    if(NOT NCURSESW_LIB)
        message(FATAL_ERROR
            "libncursesw not found.\n"
            "Install: sudo apt install libncursesw5-dev")
    endif()
endif()

# ─── Executable ──────────────────────────────────────────────────────────────

add_executable(LightCycles LightCycles/LightCycles.cpp)

if(WIN32)
    # Статическая линковка: PDCurses + CRT вшиваются в exe
    target_link_libraries(LightCycles PRIVATE unofficial::pdcurses::pdcurses user32 gdi32)
    # Статический CRT — убирает зависимость от MSVC runtime dll
    set_property(TARGET LightCycles PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

elseif(APPLE)
    target_include_directories(LightCycles PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(LightCycles      PRIVATE ${CURSES_LIBRARIES})

else()
    target_include_directories(LightCycles PRIVATE ${NCURSESW_INC})
    target_link_libraries(LightCycles      PRIVATE ${NCURSESW_LIB})
endif()

# ─── Compiler flags ──────────────────────────────────────────────────────────

if(MSVC)
    target_compile_options(LightCycles PRIVATE /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(LightCycles PRIVATE -Wall -Wextra -O2)
endif()

cmake_minimum_required(VERSION 3.15)
project(LightCycles LANGUAGES CXX)
 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
 
# ─── Curses library ──────────────────────────────────────────────────────────
#
# Windows → PDCurses via vcpkg  (vcpkg install pdcurses:x64-windows)
# Linux   → ncurses             (apt install libncurses5-dev)
# macOS   → ncurses             (brew install ncurses)
 
if(WIN32)
    find_package(unofficial-pdcurses CONFIG REQUIRED)
else()
    set(CURSES_NEED_NCURSES TRUE)
    find_package(Curses REQUIRED)
endif()
 
# ─── Executable ──────────────────────────────────────────────────────────────
 
add_executable(LightCycles LightCycles/LightCycles.cpp)
 
if(WIN32)
    target_link_libraries(LightCycles PRIVATE unofficial::pdcurses::pdcurses user32 gdi32)
else()
    target_include_directories(LightCycles PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(LightCycles      PRIVATE ${CURSES_LIBRARIES})
endif()
 
# ─── Compiler flags ──────────────────────────────────────────────────────────
 
if(MSVC)
    target_compile_options(LightCycles PRIVATE /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(LightCycles PRIVATE -Wall -Wextra -O2)
endif()
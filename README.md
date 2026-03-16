# LightCycles-Git
The official repository of DSMND Software. A simple game inspired by "Tron" '82 - "Light Cycles" In-Movie Arcade Game.

# TRON: Light Cycles
Cross-platform terminal game — Windows (VS 2022) · Linux · macOS
Repository Build Status - [![Build LightCycles](https://github.com/DESMOND37/LightCycles-Git/actions/workflows/build.yml/badge.svg)](https://github.com/DESMOND37/LightCycles-Git/actions/workflows/build.yml)

---

## Зависимости

| Платформа | Библиотека | Установка |
|-----------|-----------|-----------|
| Windows   | PDCurses  | vcpkg (см. ниже) |
| Linux     | ncurses   | `sudo apt install libncurses5-dev` |
| macOS     | ncurses   | `brew install ncurses` |

---

## Windows — Visual Studio 2022 (пошагово)

### 1. Установить vcpkg

Открыть **Developer Command Prompt for VS 2022** и выполнить:

```cmd
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

Интегрировать vcpkg в Visual Studio (один раз на машину):

```cmd
C:\vcpkg\vcpkg integrate install
```

### 2. Установить PDCurses через vcpkg

```cmd
C:\vcpkg\vcpkg install pdcurses:x64-windows
```

### 3. Сгенерировать проект CMake

```cmd
cd <папка с исходниками> # Предполагается, что вы там уже будете.

cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_MANIFEST_MODE=OFF
```
> P.S. В случае возникновения ошибок удалить папку build в корневой дериктории проекта, и сгенерировать проект CMake заново:
> ```cmd
>rmdir /S /Q build
>```

### 4. Собрать

```cmd
cmake --build build --config Release
```

Исполняемый файл появится в `build\Release\LightCycles.exe`.

### 5. Запустить

Запускать **только в Windows Terminal** или cmd с поддержкой VT:

```cmd
build\Release\LightCycles.exe
```

> **Важно:** PowerShell ISE и старый `cmd.exe` без VT-режима могут
> отображать символы некорректно. Рекомендуется Windows Terminal.

---

## Linux

```bash
sudo apt install libncurses5-dev cmake build-essential

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/LightCycles
```

---

## macOS

```bash
brew install ncurses cmake

cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCURSES_INCLUDE_DIR=$(brew --prefix ncurses)/include \
  -DCURSES_LIBRARY=$(brew --prefix ncurses)/lib/libncurses.a

cmake --build build
./build/LightCycles
```

---

## Управление

| Действие | Игрок 1 | Игрок 2 |
|----------|---------|---------|
| Вверх    | W       | ↑       |
| Вниз     | S       | ↓       |
| Влево    | A       | ←       |
| Вправо   | D       | →       |
| Пауза    | P       | P       |
| Выход    | Q       | Q       |

Разворот на 180° невозможен.  
Первый, кто наберёт **5 побед**, выигрывает матч.  
После матча — реванш (R) или выход (Q).

---

## Структура проекта

```
tron_cross/
├── LightCycles/LightCycles.cpp          # Весь игровой код
├── CMakeLists.txt                       # Кросс-платформенная сборка
└── README.md                            # Эта инструкция
```

# LightCycles-Git
The official repository of DSMND Software. A simple game inspired by "Tron" '82 "Light Cycles" - In-Movie Arcade Game.

# TRON: Light Cycles
Cross-platform terminal game — Windows (VS 2022) · Linux · macOS

The code base was written using the power of Claude Sonnet 4.6 AI

![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
[![GitHub License](https://img.shields.io/github/license/DESMOND37/LightCycles-Git?style=for-the-badge)](http://www.gnu.org/licenses/agpl-3.0)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/DESMOND37/LightCycles-Git/build.yml?style=for-the-badge&logo=github&label=Repository%20Build%20Status)

---

## Зависимости

| Платформа                    | Библиотека | Установка                                         |
|------------------------------|------------|---------------------------------------------------|
| Windows                      | PDCurses   | vcpkg (см. ниже)                                  |
| Linux (Debian/Ubuntu)        | ncursesw   | `sudo apt-get install libncursesw5-dev`           |
| Linux (RedHat/Fedora/CentOS) | ncursesw   | `sudo yum install ncurses-devel`                  |
| macOS                        | ncurses    | системный (встроен, ничего не нужно устанавливать)|

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

Статическая сборка — `LightCycles.exe` будет полностью автономным, без `.dll` зависимостей:

```cmd
C:\vcpkg\vcpkg install pdcurses:x64-windows-static
```

### 3. Сгенерировать проект CMake

```cmd
cd <папка с исходниками>

cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DVCPKG_MANIFEST_MODE=OFF
```

> P.S. В случае возникновения ошибок — удалить папку `build` и сгенерировать заново:
> ```cmd
> rmdir /S /Q build
> ```

### 4. Собрать

```cmd
cmake --build build --config Release
```

Исполняемый файл появится в `build\Release\LightCycles.exe`.

### 5. Запустить

Запускать **только в Windows Terminal** или `cmd` с поддержкой VT:

```cmd
build\Release\LightCycles.exe
```

> **Важно:** PowerShell ISE и старый `cmd.exe` без VT-режима могут
> отображать символы некорректно. Рекомендуется Windows Terminal.

---

## Linux (Debian/Ubuntu)

```bash
sudo apt install libncursesw5-dev cmake build-essential

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/LightCycles
```

## Linux (RedHat / Fedora / CentOS)

```bash
# Fedora
sudo dnf install ncurses-devel cmake gcc-c++

# CentOS / RHEL (через yum)
sudo yum install ncurses-devel cmake gcc-c++

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/LightCycles
```

> **Заметка:** на CentOS/RHEL 7 и старше пакет называется `ncurses-devel`, но он может не включать wide char поддержку. Если сборка падает с ошибкой про `wget_wch` — установите дополнительно `ncurses-devel` из EPEL или соберите ncursesw вручную.

## macOS

На macOS системный ncurses уже включает поддержку wide char — дополнительно ничего устанавливать не нужно:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/LightCycles
```

> После скачивания собранного бинарника может потребоваться снять карантин, т.к. исполняемый файл игры ещё не имеет цифровой подписи:
> ```bash
> xattr -d com.apple.quarantine ./LightCycles
> ```

---

## Управление

### Движение

| Действие | Игрок 1 (Управление на WASD) | Игрок 2 (Управление на стрелочках) |
|----------|------------------------------|------------------------------------|
| Вверх    | W                            | ↑                                  |
| Вниз     | S                            | ↓                                  |
| Влево    | A                            | ←                                  |
| Вправо   | D                            | →                                  |
| Буст     | Tab                          | Enter                              |

> Управление работает на любой раскладке клавиатуры (EN/RU).  
> Разворот на 180° невозможен.

### Системные клавиши

| Клавиша          | Действие                                                                      |
|------------------|-------------------------------------------------------------------------------|
| Space / Enter    | Принять / Подтвердить / Начать игру (Enter to The Grid - A digital frontier.) |
| P / ESC          | Пауза / снять паузу                                                           |
| Backspace        | Выход на стартовый экран (только если активна пауза)                          |
| ESC / Backspace  | Назад (в меню)                                                                |
| Q                | Выход из игры                                                                 |

### Экран конца матча

| Клавиша       | Действие             |
|---------------|----------------------|
| R             | Реванш               |
| Space / Enter | На стартовый экран   |
| Q             | Выход                |

---

## Игровые режимы

- **Player vs Player** — два игрока на одной клавиатуре
- **Player vs CPU** — один игрок против ИИ на основе алгоритма Flood Fill

Первый, кто наберёт **5 побед**, выигрывает матч.

---

## Структура проекта

```
LightCycles-Git/
├── .github/workflows/
│   └── build.yml                    # CI/CD сборка под Windows, Linux, macOS
├── LightCycles/
│   └── LightCycles.cpp              # Весь игровой код
├── CMakeLists.txt                   # Кросс-платформенная сборка
├── LICENSE                          # Файл текущей лицензии проекта - AGPL-3.0 license
└── README.md                        # Эта инструкция
```

---

## © Copyright. DSMND Software, 2026. All Rights Reserved.

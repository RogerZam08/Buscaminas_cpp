# Minesweeper in C++ 💣

A cross-platform Minesweeper game engine developed in C++17 using CMake.
This is a collaborative project designed to master software architecture, memory management (Stack/Heap), and hardware-accelerated rendering.

## Developers
- Roger Zambrano
- Pablo Macías

## Technologies
- **Language:** C++17 (Standard Template Library, manual and automatic memory management).
- **Build System:** CMake (Out-of-source builds, cross-platform conditional targets).
- **Graphics:** SDL2 & SDL2_ttf (Double buffering, hardware acceleration, and VRAM texture mapping).

## Current Progress (What's Implemented)
- **Cross-Platform Ecosystem:** Compiles and runs natively on both Windows and Linux operating systems.
- **Video & GPU Initialization:** Opens a centralized, hardware-accelerated 800x600 window.
- **State Machine:** Robust Game Loop managing transitions between Main Menu and Playing state.
- **Typography Rendering:** TrueType Font integration (`GOUDYSTO.ttf`) rendered directly from VRAM ("Press ENTER to start").
- **Dynamic Memory Grid:** 2D Vector allocated on the Heap mapping to visual 40x40 pixel cells.
- **Hardware Interruption:** Keyboard event polling to intercept both standard and numpad `Enter` keys.

## 🛠️ How to Compile from Source

### Prerequisites
- **CMake** (version 3.14 or higher).
- **C++17 Compiler** (GCC, Clang, or MSVC).
- **Git**.
- **Active internet connection** (Required for Windows only during the first configuration).

---

### 🐧 Linux (CachyOS / Arch / Ubuntu)

This project uses **dynamic linking** on Linux. Your package manager provides the pre-compiled shared libraries (`.so`), so compilation is almost instantaneous.

1. Install dependencies:
   - *Arch/CachyOS:* `sudo pacman -S base-devel cmake git sdl2 sdl2_ttf`
   - *Debian/Ubuntu:* `sudo apt-get install build-essential cmake git libsdl2-dev libsdl2-ttf-dev`

2. Clone the repository and build:
   ```bash
   git clone https://github.com/RogerZam08/Buscaminas_cpp.git
   cd Buscaminas_cpp
   mkdir build
   cd build
   cmake ..
   cmake --build . -j$(nproc)
   ```

3. Run the game:
   ```bash
   ./juego
   ```

---

### 🪟 Windows (MinGW / MSVC)

Windows lacks a unified package manager, so this project uses CMake's `FetchContent` to download the SDL2 and SDL2_ttf source code directly from GitHub and compile them as static libraries (`.lib`/`.a`). No manual dependency installation is needed.

**Note:** The very first time you run `cmake ..`, it will take several minutes to compile the graphics engine from scratch.

1. Install [CMake](https://cmake.org/download/), [Git](https://git-scm.com/download/win) and a C++17 compiler (Visual Studio with the "Desktop development with C++" workload, or MinGW-w64).

2. Clone the repository and build (in PowerShell or Command Prompt):
   ```powershell
   git clone https://github.com/RogerZam08/Buscaminas_cpp.git
   cd Buscaminas_cpp
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

3. Run the game:
   ```powershell
   .\juego.exe
   ```
   > With MSVC the executable is placed in a subfolder: `.\Release\juego.exe`.

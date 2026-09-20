cmake_minimum_required(VERSION 3.14)
project(Buscaminas CXX)

# Forzamos C++ moderno
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 1. Definimos nuestro ejecutable
add_executable(juego main.cpp)

# 2. INSTRUCCIÓN CRÍTICA: Le decimos al Enlazador que inyecte SDL2
# En Linux (CachyOS), esto se traducirá automáticamente a "-lSDL2"
# En Windows (para Pablito), CMake buscará automáticamente "SDL2.lib"
target_link_libraries(juego PRIVATE SDL2)

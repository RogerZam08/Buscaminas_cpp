cmake_minimum_required(VERSION 3.14)
project(Buscaminas CXX)

# Usamos C++ moderno
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Definimos que el ejecutable "juego" se compila a partir de "main.cpp"
add_executable(juego main.cpp)

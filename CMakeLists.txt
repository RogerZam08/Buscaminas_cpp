cmake_minimum_required(VERSION 3.14)
project(Buscaminas CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Descargar SDL2 automáticamente
include(FetchContent)
# Apagamos configuraciones que rompen el compilador MinGW en Windows
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.7
)
FetchContent_MakeAvailable(SDL2)

add_executable(juego main.cpp)
target_link_libraries(juego PRIVATE SDL2::SDL2main SDL2::SDL2-static)

# Ocultar la consola de fondo en Windows
if(WIN32)
    set_target_properties(juego PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
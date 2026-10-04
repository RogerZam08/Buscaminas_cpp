cmake_minimum_required(VERSION 3.14)
project(Buscaminas CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(juego main.cpp)

# -- FLUJO CONDICIONAL MULTIPLATAFORMA --

if(WIN32)
    # ===== (WINDOWS / MINGW) =====
    message(STATUS "Detectado entorno Windows. Descargando SDL2 desde Github...")
    
    include(FetchContent)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.7
    )
    FetchContent_MakeAvailable(SDL2)

    # Pablo enlaza las bibliotecas estáticas compiladas
    target_link_libraries(juego PRIVATE SDL2::SDL2main SDL2::SDL2-static)

    # Ocultar la consola de fondo en Windows
    set_target_properties(juego PROPERTIES WIN32_EXECUTABLE TRUE)

elseif(UNIX)
    # ===== (LINUX / CACHYOS) =====
    message(STATUS "Detectado entorno Linux. Buscando SDL2 en el sistema...")
    
    # Esto le dice a CMake que use el buscador oficial del sistema para encontrar tus librerias locales
    find_package(SDL2 REQUIRED)
    
    # Tú enlazas las bibliotecas dinámicas compartidas de tu OS
    target_link_libraries(juego PRIVATE SDL2::SDL2)
    
    # (NOTA: Si vas a usar fuentes TTF mas adelante, agrega también SDL2_ttf aquí)
    # target_link_libraries(juego PRIVATE SDL2::SDL2 SDL2_ttf)
endif()
cmake_minimum_required(VERSION 3.14)
project(Buscaminas C CXX )

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
    set(SDL2TTF_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL2TTF_STATIC ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.7
    )
    FetchContent_MakeAvailable(SDL2)

    FetchContent_Declare(
        SDL2_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-2.22.0
    )
    FetchContent_MakeAvailable(SDL2_ttf)

    # 3. Enlazador de Windows (Estático) - Llamamos al target crudo: SDL2_ttf-static
   target_link_libraries(juego PRIVATE SDL2::SDL2main SDL2::SDL2-static SDL2_ttf)

    # Ocultar la consola de fondo en Windows
    set_target_properties(juego PROPERTIES WIN32_EXECUTABLE TRUE)

elseif(UNIX)
    # ===== (LINUX / CACHYOS) =====
    message(STATUS "Detectado entorno Linux. Buscando dependencias dinámicas locales...")

    # 1. Buscar ambas librerías directamente en la memoria del OS
    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)

    # 2. Enlazador de Linux: Todo dinámico y superrápido
    target_link_libraries(juego PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
    # --- SECCIÓN DE RECURSOS ---
    # Copiar archivos de fuentes, imágenes y audios al directorio de compilación
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/GOUDYSTO.ttf 
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

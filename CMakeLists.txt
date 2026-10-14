cmake_minimum_required(VERSION 3.14)
project(Buscaminas C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(juego main.cpp)

if(WIN32)
    # ===== (WINDOWS / MINGW) =====
    message(STATUS "Detectado entorno Windows. Descargando SDL2 desde Github...")

        include(FetchContent)

    # CMake 4.x rechaza el FreeType incluido en SDL_ttf si no se baja este minimo
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    # Todo estatico: sin DLLs externas
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)    # usa su propio FreeType
    set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)   # sin HarfBuzz

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

    target_link_libraries(juego PRIVATE SDL2::SDL2main SDL2::SDL2-static SDL2_ttf)

    # Runtime de MinGW dentro del .exe (sin libgcc, libstdc++ ni winpthread)
    target_link_options(juego PRIVATE -static -static-libgcc -static-libstdc++)

    # Ocultar la consola de fondo en Windows
    set_target_properties(juego PROPERTIES WIN32_EXECUTABLE TRUE)

elseif(UNIX)
    # ===== (LINUX / CACHYOS) =====
    message(STATUS "Detectado entorno Linux. Buscando dependencias dinámicas locales...")

    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)

    target_link_libraries(juego PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
endif()

# Copiar la fuente junto al ejecutable (Windows y Linux)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/GOUDYSTO.ttf
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
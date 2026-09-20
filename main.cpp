#include <SDL2/SDL.h>
#include <iostream>
#include <chrono>
#include <thread>

// Variables globales del motor (Punteros a la memoria gráfica)
bool juegoActivo = true;
SDL_Window* ventana = nullptr;
SDL_Renderer* renderizador = nullptr;

void inicializarSistema() {
    SDL_Init(SDL_INIT_VIDEO);
    
    // Pedimos al OS memoria para la ventana
    ventana = SDL_CreateWindow("Buscaminas - Roger & Pablito", 
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                               800, 600, SDL_WINDOW_SHOWN);
                               
    // Pedimos acceso a la GPU para dibujar
    renderizador = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_ACCELERATED);
}

void procesarEntrada() {
    SDL_Event evento; // Estructura local en el Stack para guardar el paquete del OS
    
    // Vaciamos la cola de eventos de la RAM frame a frame
    while (SDL_PollEvent(&evento)) {
        // ¿El usuario hizo clic en la 'X' de la ventana?
        if (evento.type == SDL_QUIT) {
            juegoActivo = false;
        }
        // ¿El usuario presionó una tecla en el teclado físico?
        else if (evento.type == SDL_KEYDOWN) {
            // Verificamos si el código hexadecimal de la tecla coincide con 'Escape'
            if (evento.key.keysym.sym == SDLK_ESCAPE) {
                juegoActivo = false;
                std::cout << "Se presionó ESC. Iniciando secuencia de apagado...\n";
            }
        }
    }
}

void limpiarMemoria() {
    // Destruimos los punteros crudos para evitar Memory Leaks
    std::cout << "Liberando VRAM y RAM...\n";
    SDL_DestroyRenderer(renderizador);
    SDL_DestroyWindow(ventana);
    SDL_Quit();
}

int main() {
    inicializarSistema();
    const std::chrono::milliseconds tiempoPorFrame(16); // ~60 FPS

    // --- GAME LOOP ---
    while (juegoActivo) {
        auto inicioFrame = std::chrono::high_resolution_clock::now();

        // 1. Escuchar al OS
        procesarEntrada();
        
        // 2. Pintar de gris oscuro (R:40, G:40, B:40, Alpha:255)
        SDL_SetRenderDrawColor(renderizador, 40, 40, 40, 255);
        SDL_RenderClear(renderizador);
        SDL_RenderPresent(renderizador); // Volcar buffer a pantalla

        // 3. Control térmico de la CPU
        auto finFrame = std::chrono::high_resolution_clock::now();
        auto tiempoProcesamiento = std::chrono::duration_cast<std::chrono::milliseconds>(finFrame - inicioFrame);

        if (tiempoProcesamiento < tiempoPorFrame) {
            std::this_thread::sleep_for(tiempoPorFrame - tiempoProcesamiento);
        }
    }

    limpiarMemoria();
    return 0;
}
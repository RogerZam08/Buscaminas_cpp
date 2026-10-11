#include <SDL2/SDL.h>
#include <SDL_ttf.h> // NUEVA LIBRERÍA
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

enum class EstadoJuego { MENU_PRINCIPAL, JUGANDO, GAME_OVER };

bool juegoActivo = true;
EstadoJuego estadoActual = EstadoJuego::MENU_PRINCIPAL; 
SDL_Window* ventana = nullptr;
SDL_Renderer* renderizador = nullptr;
// Nuevos Punteros Globales para la tipografía
TTF_Font* fuente = nullptr;
SDL_Texture* texturaTextoMenu = nullptr; 
SDL_Rect rectanguloTexto; // Estructura que define posición (X, Y) y tamaño (Ancho, Alto)


struct Casilla {
    bool tieneMina = false;
    bool descubierta = false;
    bool tieneBandera = false;
    int minasAlrededor = 0;
};

std::vector<std::vector<Casilla>> tablero;

// --- DECLARACIONES ADELANTADAS (Promesas de memoria) ---
void crearTablero(int filas, int columnas);
void inicializarSistema();
void procesarEntrada();
void procesarLogica();
void dibujarPantalla(); // La firma que agregamos
void limpiarMemoria();

// --- DEFINICIONES (Código máquina real) ---
void crearTablero(int filas, int columnas) {
    tablero.resize(filas, std::vector<Casilla>(columnas));
    std::cout << "Tablero de " << filas << "x" << columnas << " creado en el Heap.\n";
}

void inicializarSistema() {
    SDL_Init(SDL_INIT_VIDEO);
    
    // Iniciar el subsistema de fuentes
    if (TTF_Init() == -1) {
        std::cerr << "Error crítico: No se pudo inicializar TTF: " << TTF_GetError() << "\n";
        exit(1);
    }

    ventana = SDL_CreateWindow("Buscaminas - Roger & Pablito", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    renderizador = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_ACCELERATED);

    // --- RENDERIZADO DE TEXTO EN CACHÉ (Solo se hace una vez) ---
    // IMPORTANTE: Asegúrate de tener este archivo tipográfico en tu carpeta build
    fuente = TTF_OpenFont("GOUDYSTO.ttf", 20); 
    if (!fuente) {
        std::cerr << "Error: No se encontró la fuente GOUDYSTO.ttf. Verifica la ruta.\n";
        exit(1);
    }

    SDL_Color colorBlanco = {255, 255, 255, 255};
    
    // 1. La CPU dibuja el texto en la memoria RAM lenta (Surface)
    SDL_Surface* superficieTexto = TTF_RenderUTF8_Solid(fuente, "Press ENTER to start", colorBlanco);
    
    // 2. Transferimos la imagen de la RAM a la VRAM rápida (Texture)
    texturaTextoMenu = SDL_CreateTextureFromSurface(renderizador, superficieTexto);
    
    // 3. Calculamos la posición centrada matemáticamente
    rectanguloTexto.w = superficieTexto->w;
    rectanguloTexto.h = superficieTexto->h;
    rectanguloTexto.x = (800 - rectanguloTexto.w) / 2;
    rectanguloTexto.y = (600 - rectanguloTexto.h) / 2;

    // 4. Destruimos la Surface de la RAM (ya está segura en la VRAM de la GPU)
    SDL_FreeSurface(superficieTexto);
}

void procesarEntrada() {
    SDL_Event evento;
    while (SDL_PollEvent(&evento)) {
        if (evento.type == SDL_QUIT) {
            juegoActivo = false;
        }
        else if (evento.type == SDL_KEYDOWN) {
            if (evento.key.keysym.sym == SDLK_ESCAPE) {
                juegoActivo = false;
            }
            // Transición de estado: Presionar ENTER para jugar
            else if (evento.key.keysym.sym == SDLK_RETURN && estadoActual == EstadoJuego::MENU_PRINCIPAL) {
                estadoActual = EstadoJuego::JUGANDO;
                crearTablero(10, 10); // Reservamos la memoria del tablero
                std::cout << "Tecla ENTER detectada. Cambiando a JUGANDO...\n";
            }
        }
    }
}

// 4. CÓMO CAMBIA TU GAME LOOP
void procesarLogica() {
    switch (estadoActual) {
        case EstadoJuego::MENU_PRINCIPAL:
            // Aquí en el futuro puedes hacer titilar un texto de "Presione ENTER"
            break;
            
        case EstadoJuego::JUGANDO: {
            // 1. Limpiar el fondo de toda la ventana (Gris oscuro)
            SDL_SetRenderDrawColor(renderizador, 30, 30, 30, 255);
            SDL_RenderClear(renderizador);

            // 2. Definir la geometría de tu cuadrícula
            const int tamanoCasilla = 40; // Cada cuadro medirá 40x40 píxeles
            const int offsetX = 200;      // Margen izquierdo para centrar el tablero en la ventana
            const int offsetY = 100;      // Margen superior

            // 3. Recorrer la memoria dinámica del tablero
            for (int fila = 0; fila < tablero.size(); ++fila) {
                for (int col = 0; col < tablero[fila].size(); ++col) {
                    
                    // Calcular las coordenadas físicas en la VRAM
                    SDL_Rect rect;
                    rect.x = offsetX + (col * tamanoCasilla);
                    rect.y = offsetY + (fila * tamanoCasilla);
                    rect.w = tamanoCasilla;
                    rect.h = tamanoCasilla;

                    // 4. Leer el estado de tu Struct Casilla para decidir el color
                    if (tablero[fila][col].descubierta) {
                        // Si está descubierta, fondo gris claro
                        SDL_SetRenderDrawColor(renderizador, 200, 200, 200, 255);
                    } else {
                        // Si está oculta, color azul/grisáceo (como bloques de piedra)
                        SDL_SetRenderDrawColor(renderizador, 100, 120, 140, 255);
                    }
                    
                    // Enviar orden a la GPU para rellenar el cuadro
                    SDL_RenderFillRect(renderizador, &rect);

                    // 5. Dibujar un contorno negro alrededor del cuadro para separarlos
                    SDL_SetRenderDrawColor(renderizador, 0, 0, 0, 255);
                    SDL_RenderDrawRect(renderizador, &rect);
                }
            }
            break;
        }
            
        case EstadoJuego::GAME_OVER:
            // Rojo Oscuro para la derrota
            SDL_SetRenderDrawColor(renderizador, 80, 10, 10, 255);
            break;
    }

    // Una vez seleccionado el color, inundamos la VRAM y volteamos el buffer
    SDL_RenderPresent(renderizador);
}

void dibujarPantalla() {
    switch (estadoActual) {
        case EstadoJuego::MENU_PRINCIPAL:
            // 1. Limpiamos el lienzo de azul marino
            SDL_SetRenderDrawColor(renderizador, 10, 20, 60, 255);
            SDL_RenderClear(renderizador);
            
            // 2. ORDEN A LA GPU: Copiar la textura de VRAM al lienzo
            if (texturaTextoMenu != nullptr) {
                SDL_RenderCopy(renderizador, texturaTextoMenu, nullptr, &rectanguloTexto);
            }
            break;
            
            
        case EstadoJuego::JUGANDO: {
            SDL_SetRenderDrawColor(renderizador, 30, 30, 30, 255);
            SDL_RenderClear(renderizador);

            const int tamanoCasilla = 40;
            const int offsetX = 200;
            const int offsetY = 100;

            for (size_t fila = 0; fila < tablero.size(); ++fila) {
                for (size_t col = 0; col < tablero[fila].size(); ++col) {
                    SDL_Rect rect;
                    rect.x = offsetX + (col * tamanoCasilla);
                    rect.y = offsetY + (fila * tamanoCasilla);
                    rect.w = tamanoCasilla;
                    rect.h = tamanoCasilla;

                    if (tablero[fila][col].descubierta) {
                        SDL_SetRenderDrawColor(renderizador, 200, 200, 200, 255);
                    } else {
                        SDL_SetRenderDrawColor(renderizador, 100, 120, 140, 255);
                    }
                    
                    SDL_RenderFillRect(renderizador, &rect);
                    SDL_SetRenderDrawColor(renderizador, 0, 0, 0, 255);
                    SDL_RenderDrawRect(renderizador, &rect);
                }
            }
            break;
        }
            
        case EstadoJuego::GAME_OVER:
            SDL_SetRenderDrawColor(renderizador, 80, 10, 10, 255);
            SDL_RenderClear(renderizador);
            break;
    }

    SDL_RenderPresent(renderizador);
}

void limpiarMemoria() {
    std::cout << "Liberando VRAM y RAM...\n";
    
    // Destruimos la nueva memoria reservada en orden inverso
    SDL_DestroyTexture(texturaTextoMenu);
    TTF_CloseFont(fuente);
    TTF_Quit(); // Apagamos el motor tipográfico
    
    SDL_DestroyRenderer(renderizador);
    SDL_DestroyWindow(ventana);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    // el Instruct pointer empieza aqui
    // llamo al driver de video y a la GPU inicializando el sistema.
    inicializarSistema();

    // 2. Reservamos bytes en el Stack para guardar nuestro límite de tiempo (16ms).
    const std::chrono::milliseconds tiempoPorFrame(16);
    
    // --- GAME LOOP ---
    while (juegoActivo) {
        auto inicioFrame = std::chrono::high_resolution_clock::now();

        // 1. Leer hardware (Entrada USB/Interrupciones)
        procesarEntrada(); 
        
        // 2. Calcular matemáticas, colisiones y estados
        procesarLogica();  
        
        // 3. El ÚNICO punto de contacto con la GPU
        dibujarPantalla(); 

        // 4. Control térmico de la CPU
        auto finFrame = std::chrono::high_resolution_clock::now();
        auto tiempoProcesamiento = std::chrono::duration_cast<std::chrono::milliseconds>(finFrame - inicioFrame);

        if (tiempoProcesamiento < tiempoPorFrame) {
            std::this_thread::sleep_for(tiempoPorFrame - tiempoProcesamiento);
        }
        
    } // Fin del ciclo


    // 10. Si salimos del ciclo (juegoActivo == false), devolvemos la memoria de video.
    limpiarMemoria();
    
    // 11. Código de salida estándar en POSIX/Windows. Un 0 significa "cierre exitoso".
    return 0;
}
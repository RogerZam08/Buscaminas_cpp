#include <SDL2/SDL.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

// 1. LA MÁQUINA DE ESTADOS
// Usamos 'enum class' porque solo queremos usar valores determinados.
enum class EstadoJuego {
    MENU_PRINCIPAL,
    JUGANDO,
    GAME_OVER
};

// Variables globales del motor (Punteros a la memoria gráfica)
bool juegoActivo = true;
EstadoJuego estadoActual = EstadoJuego::MENU_PRINCIPAL; 
SDL_Window* ventana = nullptr;
SDL_Renderer* renderizador = nullptr;

// 2. LA ESTRUCTURA DE MEMORIA (La Casilla)
struct Casilla {
    bool tieneMina = false;      // ¿Hay una bomba aquí?
    bool descubierta = false;    // ¿El jugador ya hizo clic aquí?
    bool tieneBandera = false;   // ¿El jugador puso una banderita (click derecho)?
    int minasAlrededor = 0;      // Número a mostrar (del 1 al 8)
};

//  EL TABLERO (El mapa de memoria)
// std::vector anidado crea una matriz dinámica en el Heap.
std::vector<std::vector<Casilla>> tablero;

// Función para inicializar la memoria del tablero
void crearTablero(int filas, int columnas) {
    // Redimensionamos la matriz en la memoria RAM
    // Asigna un bloque de 'filas' que contienen vectores de 'columnas' de Casillas vacías.
    tablero.resize(filas, std::vector<Casilla>(columnas));
    
    std::cout << "Tablero de " << filas << "x" << columnas << " creado en el Heap.\n";
}

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
            
        case EstadoJuego::JUGANDO:
            // Aquí procesaremos si el jugador pierde o gana
            break;
            
        case EstadoJuego::GAME_OVER:
            break;
    }
}
void dibujarPantalla() {
    // La GPU bifurca el dibujado leyendo la RAM
    switch (estadoActual) {
        case EstadoJuego::MENU_PRINCIPAL:
            // Azul Marino para el menú
            SDL_SetRenderDrawColor(renderizador, 10, 20, 60, 255);
            break;
            
        case EstadoJuego::JUGANDO:
            // Gris Oscuro para el tablero
            SDL_SetRenderDrawColor(renderizador, 40, 40, 40, 255);
            // Próximamente: Iterar sobre tu std::vector tablero aquí
            break;
            
        case EstadoJuego::GAME_OVER:
            // Rojo Oscuro para la derrota
            SDL_SetRenderDrawColor(renderizador, 80, 10, 10, 255);
            break;
    }

    // Una vez seleccionado el color, inundamos la VRAM y volteamos el buffer
    SDL_RenderClear(renderizador);
    SDL_RenderPresent(renderizador);
}


void limpiarMemoria() {
    // Destruimos los punteros crudos para evitar Memory Leaks
    std::cout << "Liberando VRAM y RAM...\n";
    SDL_DestroyRenderer(renderizador);
    SDL_DestroyWindow(ventana);
    SDL_Quit();
}

int main() {
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
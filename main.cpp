#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <string>
#include <random>

enum class EstadoJuego { MENU_PRINCIPAL, JUGANDO, GAME_OVER, VICTORIA };

// --- GEOMETRÍA EN TIEMPO DE COMPILACIÓN (15x15 en ventana 800x600) ---
constexpr int ANCHO_VENTANA  = 800;
constexpr int ALTO_VENTANA   = 600;
constexpr int FILAS          = 15;
constexpr int COLUMNAS       = 15;
constexpr int TOTAL_MINAS    = 35;  // ~15.5% de densidad en 225 casillas
constexpr int TAMANO_CASILLA = 32;  // 15 * 32 = 480 px (cabe holgadamente en 600 px)
constexpr int OFFSET_X       = (ANCHO_VENTANA - (COLUMNAS * TAMANO_CASILLA)) / 2; // 160 px
constexpr int OFFSET_Y       = 80;  // 80 px arriba (HUD) + 480 px tablero = 560 px (40 px margen inferior)

// Validaciones en tiempo de compilación: cero costo en ejecución
static_assert(OFFSET_X >= 0, "Error: El tablero excede el ancho de la ventana.");
static_assert(OFFSET_Y + (FILAS * TAMANO_CASILLA) <= ALTO_VENTANA, "Error: El tablero excede el alto de la ventana.");
static_assert(TOTAL_MINAS < FILAS * COLUMNAS, "Error: Debe haber menos minas que casillas.");

// Agrupa un puntero a VRAM con su rectángulo de destino en pantalla
struct TextoVRAM {
    SDL_Texture* textura = nullptr;
    SDL_Rect rect{0, 0, 0, 0};
};

struct Casilla {
    bool tieneMina = false;
    bool descubierta = false;
    bool tieneBandera = false;
    int minasAlrededor = 0;
};

// --- ESTADO GLOBAL ---
bool juegoActivo = true;
EstadoJuego estadoActual = EstadoJuego::MENU_PRINCIPAL;
int casillasSegurasReveladas = 0;
int banderasColocadas = 0;

std::vector<std::vector<Casilla>> tablero;

// --- RECURSOS DE SDL / VRAM ---
SDL_Window* ventana = nullptr;
SDL_Renderer* renderizador = nullptr;

// Tres tamaños de la misma tipografía para ajustar jerarquía visual y celdas de 32x32
TTF_Font* fuenteCeldas = nullptr; // 16 pt: números dentro de casillas
TTF_Font* fuenteUI     = nullptr; // 20 pt: menú, contador y subtítulos
TTF_Font* fuenteTitulo = nullptr; // 36 pt: "GAME OVER" y "YOU WIN!"

TextoVRAM textoMenu;
TextoVRAM textoGameOver;
TextoVRAM textoVictoria;
TextoVRAM textoReintentar;
TextoVRAM textoContador;

std::array<SDL_Texture*, 9> texturasNumeros{};

// --- DECLARACIONES ADELANTADAS ---
TextoVRAM crearTextoCentrado(TTF_Font* fuente, const std::string& mensaje, SDL_Color color, int centroY);
void destruirTexto(TextoVRAM& t);
void crearTablero(int filas, int columnas);
void colocarMinas(int cantidadMinas);
void calcularMinasVecinas();
void actualizarTexturaContador();
void iniciarNuevaPartida();
void alternarBandera(int fila, int col);
void revelarCasilla(int fila, int col);
void inicializarSistema();
void procesarEntrada();
void procesarLogica();
void dibujarPantalla();
void limpiarMemoria();

// --- UTILIDADES DE TEXTO EN VRAM ---
TextoVRAM crearTextoCentrado(TTF_Font* fuente, const std::string& mensaje, SDL_Color color, int centroY) {
    TextoVRAM resultado;
    SDL_Surface* superficie = TTF_RenderUTF8_Solid(fuente, mensaje.c_str(), color);
    if (superficie != nullptr) {
        resultado.textura = SDL_CreateTextureFromSurface(renderizador, superficie);
        resultado.rect.w = superficie->w;
        resultado.rect.h = superficie->h;
        resultado.rect.x = (ANCHO_VENTANA - resultado.rect.w) / 2;
        resultado.rect.y = centroY - (resultado.rect.h / 2);
        SDL_FreeSurface(superficie); // Liberamos la RAM; la textura ya vive en VRAM
    }
    return resultado;
}

void destruirTexto(TextoVRAM& t) {
    if (t.textura != nullptr) {
        SDL_DestroyTexture(t.textura);
        t.textura = nullptr;
    }
}

// --- LÓGICA DEL TABLERO ---
void crearTablero(int filas, int columnas) {
    tablero.assign(filas, std::vector<Casilla>(columnas));
    casillasSegurasReveladas = 0;
    banderasColocadas = 0;
    std::cout << "Tablero de " << filas << "x" << columnas << " creado en el Heap.\n";
}

void colocarMinas(int cantidadMinas) {
    std::random_device rd;
    std::mt19937 generador(rd());
    std::uniform_int_distribution<int> distFila(0, FILAS - 1);
    std::uniform_int_distribution<int> distCol(0, COLUMNAS - 1);

    int minasColocadas = 0;
    while (minasColocadas < cantidadMinas) {
        int f = distFila(generador);
        int c = distCol(generador);

        if (!tablero[f][c].tieneMina) {
            tablero[f][c].tieneMina = true;
            minasColocadas++;
        }
    }
    std::cout << cantidadMinas << " minas colocadas aleatoriamente.\n";
}

void calcularMinasVecinas() {
    for (int f = 0; f < FILAS; ++f) {
        for (int c = 0; c < COLUMNAS; ++c) {
            if (tablero[f][c].tieneMina) continue;

            int conteo = 0;
            for (int df = -1; df <= 1; ++df) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (df == 0 && dc == 0) continue;

                    int nf = f + df;
                    int nc = c + dc;
                    if (nf >= 0 && nf < FILAS && nc >= 0 && nc < COLUMNAS) {
                        if (tablero[nf][nc].tieneMina) {
                            conteo++;
                        }
                    }
                }
            }
            tablero[f][c].minasAlrededor = conteo;
        }
    }
}

void actualizarTexturaContador() {
    destruirTexto(textoContador); // Evita fugas en VRAM antes de subir el nuevo número

    int minasRestantes = TOTAL_MINAS - banderasColocadas;
    std::string mensaje = "Minas: " + std::to_string(minasRestantes);
    SDL_Color colorHUD = {240, 240, 240, 255};

    // Centrado verticalmente en el margen superior (0 .. OFFSET_Y)
    textoContador = crearTextoCentrado(fuenteUI, mensaje, colorHUD, OFFSET_Y / 2);
}

void iniciarNuevaPartida() {
    crearTablero(FILAS, COLUMNAS);
    colocarMinas(TOTAL_MINAS);
    calcularMinasVecinas();
    actualizarTexturaContador();
    estadoActual = EstadoJuego::JUGANDO;
    std::cout << "Partida 15x15 iniciada.\n";
}

void alternarBandera(int fila, int col) {
    if (tablero[fila][col].descubierta) return;

    tablero[fila][col].tieneBandera = !tablero[fila][col].tieneBandera;
    banderasColocadas += tablero[fila][col].tieneBandera ? 1 : -1;
    actualizarTexturaContador();
}

void revelarCasilla(int fila, int col) {
    // Casos base de seguridad
    if (fila < 0 || fila >= FILAS || col < 0 || col >= COLUMNAS) return;
    if (estadoActual != EstadoJuego::JUGANDO) return;
    if (tablero[fila][col].descubierta || tablero[fila][col].tieneBandera) return;

    tablero[fila][col].descubierta = true;

    // Derrota
    if (tablero[fila][col].tieneMina) {
        estadoActual = EstadoJuego::GAME_OVER;
        std::cout << "BOOM! Mina en (" << fila << ", " << col << "). GAME OVER.\n";
        return;
    }

    // Victoria
    casillasSegurasReveladas++;
    constexpr int totalSeguras = (FILAS * COLUMNAS) - TOTAL_MINAS;
    if (casillasSegurasReveladas == totalSeguras) {
        estadoActual = EstadoJuego::VICTORIA;
        std::cout << "VICTORIA! Tablero despejado.\n";
        return;
    }

    // Expansión recursiva (Flood-Fill) si no hay minas adyacentes
    if (tablero[fila][col].minasAlrededor == 0) {
        for (int df = -1; df <= 1; ++df) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (df == 0 && dc == 0) continue;
                revelarCasilla(fila + df, col + dc);
            }
        }
    }
}

// --- INICIALIZACIÓN DEL MOTOR ---
void inicializarSistema() {
    SDL_Init(SDL_INIT_VIDEO);

    if (TTF_Init() == -1) {
        std::cerr << "Error crítico: No se pudo inicializar TTF: " << TTF_GetError() << "\n";
        exit(1);
    }

    ventana = SDL_CreateWindow("Buscaminas 15x15 - Roger & Pablito",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               ANCHO_VENTANA, ALTO_VENTANA, SDL_WINDOW_SHOWN);
    renderizador = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_ACCELERATED);

    fuenteCeldas = TTF_OpenFont("GOUDYSTO.ttf", 16);
    fuenteUI     = TTF_OpenFont("GOUDYSTO.ttf", 20);
    fuenteTitulo = TTF_OpenFont("GOUDYSTO.ttf", 36);

    if (!fuenteCeldas || !fuenteUI || !fuenteTitulo) {
        std::cerr << "Error: No se encontró la fuente GOUDYSTO.ttf. Verifica la ruta.\n";
        exit(1);
    }

    // Precarga en caché (VRAM) de todos los textos estáticos
    SDL_Color blanco = {255, 255, 255, 255};
    SDL_Color grisClaro = {220, 220, 220, 255};

    textoMenu       = crearTextoCentrado(fuenteUI,     "Click or press ENTER", blanco,    ALTO_VENTANA / 2);
    textoGameOver   = crearTextoCentrado(fuenteTitulo, "GAME OVER",            blanco,    ALTO_VENTANA / 2 - 25);
    textoVictoria   = crearTextoCentrado(fuenteTitulo, "YOU WIN!",             blanco,    ALTO_VENTANA / 2 - 25);
    textoReintentar = crearTextoCentrado(fuenteUI,     "Click to play again",  grisClaro, ALTO_VENTANA / 2 + 40);

    // Precarga en caché (VRAM) de los dígitos 1..8
    const std::array<SDL_Color, 9> coloresNumeros = {{
        {0, 0, 0, 255},       // 0 (no se usa)
        {0, 0, 230, 255},     // 1: Azul
        {0, 140, 0, 255},     // 2: Verde
        {210, 0, 0, 255},     // 3: Rojo
        {0, 0, 128, 255},     // 4: Azul marino
        {130, 0, 0, 255},     // 5: Guinda
        {0, 128, 128, 255},   // 6: Cian oscuro
        {20, 20, 20, 255},    // 7: Negro
        {90, 90, 90, 255}     // 8: Gris oscuro
    }};

    for (int i = 1; i <= 8; ++i) {
        std::string digito = std::to_string(i);
        SDL_Surface* supNum = TTF_RenderUTF8_Solid(fuenteCeldas, digito.c_str(), coloresNumeros[i]);
        if (supNum != nullptr) {
            texturasNumeros[i] = SDL_CreateTextureFromSurface(renderizador, supNum);
            SDL_FreeSurface(supNum);
        }
    }
}

// --- ENTRADA DE HARDWARE ---
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
            else if ((evento.key.keysym.sym == SDLK_RETURN ||
                      evento.key.keysym.sym == SDLK_KP_ENTER) &&
                     estadoActual != EstadoJuego::JUGANDO) {
                iniciarNuevaPartida();
            }
        }
        else if (evento.type == SDL_MOUSEBUTTONDOWN) {
            // 1. Si NO estamos jugando, un clic izquierdo en cualquier parte inicia/reinicia
            if (estadoActual != EstadoJuego::JUGANDO) {
                if (evento.button.button == SDL_BUTTON_LEFT) {
                    iniciarNuevaPartida();
                }
            }
            // 2. Rama excluyente (else): solo procesa el tablero si ya estábamos en JUGANDO
            else {
                int mouseX = evento.button.x;
                int mouseY = evento.button.y;

                constexpr int anchoTablero = COLUMNAS * TAMANO_CASILLA;
                constexpr int altoTablero  = FILAS * TAMANO_CASILLA;

                if (mouseX >= OFFSET_X && mouseX < OFFSET_X + anchoTablero &&
                    mouseY >= OFFSET_Y && mouseY < OFFSET_Y + altoTablero) {

                    int col  = (mouseX - OFFSET_X) / TAMANO_CASILLA;
                    int fila = (mouseY - OFFSET_Y) / TAMANO_CASILLA;

                    if (evento.button.button == SDL_BUTTON_LEFT) {
                        revelarCasilla(fila, col);
                    }
                    else if (evento.button.button == SDL_BUTTON_RIGHT) {
                        alternarBandera(fila, col);
                    }
                }
            }
        }
    }
}

void procesarLogica() {
    // Reservado para lógica por fotograma (ej. cronómetro futuro)
}

// --- RENDERIZADO (ÚNICO PUNTO DE CONTACTO CON LA GPU) ---
void dibujarPantalla() {
    switch (estadoActual) {
        case EstadoJuego::MENU_PRINCIPAL:
            SDL_SetRenderDrawColor(renderizador, 10, 20, 60, 255);
            SDL_RenderClear(renderizador);

            if (textoMenu.textura != nullptr) {
                SDL_RenderCopy(renderizador, textoMenu.textura, nullptr, &textoMenu.rect);
            }
            break;

        case EstadoJuego::JUGANDO: {
            SDL_SetRenderDrawColor(renderizador, 30, 30, 30, 255);
            SDL_RenderClear(renderizador);

            if (textoContador.textura != nullptr) {
                SDL_RenderCopy(renderizador, textoContador.textura, nullptr, &textoContador.rect);
            }

            for (int fila = 0; fila < FILAS; ++fila) {
                for (int col = 0; col < COLUMNAS; ++col) {
                    const Casilla& casilla = tablero[fila][col];

                    SDL_Rect rect{
                        OFFSET_X + col * TAMANO_CASILLA,
                        OFFSET_Y + fila * TAMANO_CASILLA,
                        TAMANO_CASILLA,
                        TAMANO_CASILLA
                    };

                    if (casilla.descubierta) {
                        SDL_SetRenderDrawColor(renderizador, 200, 200, 200, 255);
                    } else {
                        SDL_SetRenderDrawColor(renderizador, 100, 120, 140, 255);
                    }

                    SDL_RenderFillRect(renderizador, &rect);
                    SDL_SetRenderDrawColor(renderizador, 0, 0, 0, 255);
                    SDL_RenderDrawRect(renderizador, &rect);

                    // Número centrado dentro de la celda de 32x32
                    if (casilla.descubierta && casilla.minasAlrededor > 0) {
                        SDL_Texture* texNum = texturasNumeros[casilla.minasAlrededor];
                        if (texNum != nullptr) {
                            int texW = 0, texH = 0;
                            SDL_QueryTexture(texNum, nullptr, nullptr, &texW, &texH);

                            SDL_Rect rectNum{
                                rect.x + (TAMANO_CASILLA - texW) / 2,
                                rect.y + (TAMANO_CASILLA - texH) / 2,
                                texW,
                                texH
                            };
                            SDL_RenderCopy(renderizador, texNum, nullptr, &rectNum);
                        }
                    }
                    // Bandera escalada proporcionalmente a TAMANO_CASILLA
                    else if (!casilla.descubierta && casilla.tieneBandera) {
                        int mastilX = rect.x + (TAMANO_CASILLA * 3) / 8;
                        int mastilY = rect.y + TAMANO_CASILLA / 5;
                        int mastilH = (TAMANO_CASILLA * 3) / 5;

                        SDL_Rect mastil{mastilX, mastilY, 3, mastilH};
                        SDL_SetRenderDrawColor(renderizador, 20, 20, 20, 255);
                        SDL_RenderFillRect(renderizador, &mastil);

                        SDL_Rect pano{mastilX + 3, mastilY, TAMANO_CASILLA / 3, TAMANO_CASILLA / 4};
                        SDL_SetRenderDrawColor(renderizador, 220, 40, 40, 255);
                        SDL_RenderFillRect(renderizador, &pano);
                    }
                }
            }
            break;
        }

        case EstadoJuego::GAME_OVER:
            // Pantalla roja con "GAME OVER" y aviso de reinicio
            SDL_SetRenderDrawColor(renderizador, 110, 15, 15, 255);
            SDL_RenderClear(renderizador);

            if (textoGameOver.textura != nullptr) {
                SDL_RenderCopy(renderizador, textoGameOver.textura, nullptr, &textoGameOver.rect);
            }
            if (textoReintentar.textura != nullptr) {
                SDL_RenderCopy(renderizador, textoReintentar.textura, nullptr, &textoReintentar.rect);
            }
            break;

        case EstadoJuego::VICTORIA:
            SDL_SetRenderDrawColor(renderizador, 15, 90, 40, 255);
            SDL_RenderClear(renderizador);

            if (textoVictoria.textura != nullptr) {
                SDL_RenderCopy(renderizador, textoVictoria.textura, nullptr, &textoVictoria.rect);
            }
            if (textoReintentar.textura != nullptr) {
                SDL_RenderCopy(renderizador, textoReintentar.textura, nullptr, &textoReintentar.rect);
            }
            break;
    }

    SDL_RenderPresent(renderizador);
}

// --- LIBERACIÓN EN ORDEN ESTRICTAMENTE INVERSO ---
void limpiarMemoria() {
    std::cout << "Liberando VRAM y RAM...\n";

    for (SDL_Texture* tex : texturasNumeros) {
        if (tex != nullptr) {
            SDL_DestroyTexture(tex);
        }
    }

    destruirTexto(textoContador);
    destruirTexto(textoReintentar);
    destruirTexto(textoVictoria);
    destruirTexto(textoGameOver);
    destruirTexto(textoMenu);

    TTF_CloseFont(fuenteTitulo);
    TTF_CloseFont(fuenteUI);
    TTF_CloseFont(fuenteCeldas);
    TTF_Quit();

    SDL_DestroyRenderer(renderizador);
    SDL_DestroyWindow(ventana);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    inicializarSistema();

    const std::chrono::milliseconds tiempoPorFrame(16);

    while (juegoActivo) {
        auto inicioFrame = std::chrono::high_resolution_clock::now();

        procesarEntrada();
        procesarLogica();
        dibujarPantalla();

        auto finFrame = std::chrono::high_resolution_clock::now();
        auto tiempoProcesamiento = std::chrono::duration_cast<std::chrono::milliseconds>(finFrame - inicioFrame);

        if (tiempoProcesamiento < tiempoPorFrame) {
            std::this_thread::sleep_for(tiempoPorFrame - tiempoProcesamiento);
        }
    }

    limpiarMemoria();
    return 0;
}
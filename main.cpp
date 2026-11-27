// ============================================================================
// BUSCAMINAS 15x15 EN C++17 + SDL2 + SDL2_ttf
// Proyecto de Programación Avanzada - Roger Zambrano & Pablo Macías
// ============================================================================

#include <SDL2/SDL.h>   // API gráfica, ventana, renderizador acelerado por GPU y eventos
#include <SDL_ttf.h>    // Motor FreeType/SDL para rasterizar fuentes TrueType (.ttf)
#include <iostream>     // Flujos de consola estándar (std::cout, std::cerr)
#include <chrono>       // Reloj de alta precisión para limitar el Game Loop a ~60 FPS
#include <thread>       // std::this_thread::sleep_for para ceder la CPU al OS
#include <vector>       // Contenedor dinámico en el Heap para la matriz del tablero
#include <array>        // Contenedor de tamaño fijo en el Stack para cachear dígitos 1..8
#include <string>       // Manejo seguro de texto (std::string, std::to_string)
#include <random>       // Motor pseudoaleatorio moderno C++11 (std::mt19937)
#include <utility>     // std::swap

// ============================================================================
// 1. MÁQUINA DE ESTADOS FINITOS (FSM)
// ============================================================================
// 'enum class' (C++11) crea un enumerado fuertemente tipado: evita colisiones
// de nombres globales y conversiones implícitas accidentales a int.
enum class EstadoJuego {
    MENU_PRINCIPAL,
    JUGANDO,
    GAME_OVER,
    VICTORIA
};

// ============================================================================
// 2. CONSTANTES DE GEOMETRÍA EVALUADAS EN TIEMPO DE COMPILACIÓN
// ============================================================================
// 'constexpr' le ordena al compilador evaluar estas expresiones al compilar.
// En el binario final no se realizan divisiones ni multiplicaciones: el valor
// queda incrustado directamente como constante inmediata en ensamblador.
constexpr int ANCHO_VENTANA  = 800;
constexpr int ALTO_VENTANA   = 600;
constexpr int FILAS          = 15;
constexpr int COLUMNAS       = 15;
constexpr int TOTAL_MINAS    = 35;  // ~15.5% de densidad sobre 225 casillas
constexpr int TAMANO_CASILLA = 32;  // 15 * 32 px = 480 px (cabe en 600 px de alto)

// Centrado horizontal automático: (800 - 480) / 2 = 160 px de margen a cada lado
constexpr int OFFSET_X       = (ANCHO_VENTANA - (COLUMNAS * TAMANO_CASILLA)) / 2;
// Margen superior fijo de 80 px para el HUD (contador). Quedan 40 px de margen inferior.
constexpr int OFFSET_Y       = 80;

// 'static_assert' valida invariantes geométricas al compilar. Si algún día cambias
// FILAS/COLUMNAS y el tablero se sale de la ventana, g++ detiene la compilación.
static_assert(OFFSET_X >= 0,
              "Error: El tablero excede el ancho de la ventana.");
static_assert(OFFSET_Y + (FILAS * TAMANO_CASILLA) <= ALTO_VENTANA,
              "Error: El tablero excede el alto de la ventana.");
static_assert(TOTAL_MINAS <= (FILAS * COLUMNAS) - 9,
              "Error: No caben las minas respetando una zona segura 3x3.");

// ============================================================================
// 3. ESTRUCTURAS DE DATOS (LAYOUT EN MEMORIA)
// ============================================================================

// Empaqueta un puntero a una textura residente en la VRAM de la GPU junto con
// el rectángulo (x, y, w, h) donde SDL_RenderCopy la estampará en pantalla.
struct TextoVRAM {
    SDL_Texture* textura = nullptr; // Dirección manejada por el driver de video
    SDL_Rect rect{0, 0, 0, 0};      // Coordenadas y dimensiones en píxeles
};

// Representa el estado lógico de una sola celda del Buscaminas.
// Ocupación física típica en x86-64:
//   3 bools (3 bytes) + 1 byte de padding (alineación) + 1 int (4 bytes) = 8 bytes.
struct Casilla {
    bool tieneMina = false;      // true si oculta una mina
    bool descubierta = false;    // true cuando el jugador ya la reveló
    bool tieneBandera = false;   // true si el jugador la marcó con clic derecho
    int minasAlrededor = 0;      // Cantidad precalculada de minas vecinas (0..8)
};

// ============================================================================
// 4. ESTADO GLOBAL DEL MOTOR Y PUNTEROS DE RECURSOS
// ============================================================================
bool juegoActivo = true;
EstadoJuego estadoActual = EstadoJuego::MENU_PRINCIPAL;
int casillasSegurasReveladas = 0; // Permite verificar victoria en O(1) sin recorrer la matriz
int banderasColocadas = 0;        // Controla el contador HUD: TOTAL_MINAS - banderasColocadas
bool primerClicPendiente = true;  // Controla la generación diferida de minas

// Matriz 2D en el Heap: un vector de 15 filas donde cada fila apunta a un bloque
// contiguo de 15 structs Casilla (15 * 8 = 120 bytes por fila).
std::vector<std::vector<Casilla>> tablero;

// Punteros crudos a estructuras opacas de SDL2 (librería escrita en C).
// Se inicializan explícitamente en nullptr para detectar fallos y evitar punteros colgantes.
SDL_Window* ventana = nullptr;
SDL_Renderer* renderizador = nullptr;

// Abrimos la misma fuente en 3 tamaños distintos al arrancar porque en SDL_ttf
// cada tamaño requiere rasterizar sus propios glifos a distinta resolución.
TTF_Font* fuenteCeldas = nullptr; // 16 pt: números centrados en celdas de 32x32 px
TTF_Font* fuenteUI     = nullptr; // 20 pt: menú, contador HUD y avisos
TTF_Font* fuenteTitulo = nullptr; // 36 pt: carteles grandes "GAME OVER" / "YOU WIN!"

// Textos precargados en VRAM para evitar rasterizar en cada fotograma
TextoVRAM textoMenu;
TextoVRAM textoGameOver;
TextoVRAM textoVictoria;
TextoVRAM textoReintentar;
TextoVRAM textoContador; // Único texto dinámico: se regenera solo al cambiar banderas

// Caché en VRAM de los dígitos '1'..'8'. El índice 0 se deja en nullptr porque
// las casillas con 0 minas vecinas no dibujan número.
// Las llaves '{}' garantizan inicialización a cero (todos los punteros en nullptr).
std::array<SDL_Texture*, 9> texturasNumeros{};

// ============================================================================
// 5. DECLARACIONES ADELANTADAS (PROTOTIPOS)
// ============================================================================
// Informan al compilador la firma exacta de cada función antes de que sean invocadas.
TextoVRAM crearTextoCentrado(TTF_Font* fuente, const std::string& mensaje, SDL_Color color, int centroY);
void destruirTexto(TextoVRAM& t);
void crearTablero(int filas, int columnas);
void colocarMinas(int cantidadMinas, int filaSegura, int colSegura);
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

// ============================================================================
// 6. UTILIDADES DE GESTIÓN DE TEXTURAS EN VRAM
// ============================================================================

// Rasteriza un texto en RAM (Surface), lo sube a VRAM (Texture), calcula su
// posición centrada en X y centrada en 'centroY', y libera la RAM temporal.
// Nota: 'mensaje' se pasa por referencia constante (const std::string&) para
// evitar copiar los caracteres del string en el Heap al llamar a la función.
TextoVRAM crearTextoCentrado(TTF_Font* fuente, const std::string& mensaje, SDL_Color color, int centroY) {
    TextoVRAM resultado; // Vive en el Stack; al retornarse se aplica NRVO (cero copias)

    // Paso 1 (CPU -> RAM): FreeType dibuja el mapa de bits del texto en memoria principal
    SDL_Surface* superficie = TTF_RenderUTF8_Solid(fuente, mensaje.c_str(), color);
    if (superficie != nullptr) {
        // Paso 2 (RAM -> VRAM): El driver sube los píxeles a la memoria rápida de la GPU
        resultado.textura = SDL_CreateTextureFromSurface(renderizador, superficie);

        // Paso 3: Calcular el rectángulo de destino usando el ancho/alto real del texto
        resultado.rect.w = superficie->w;
        resultado.rect.h = superficie->h;
        resultado.rect.x = (ANCHO_VENTANA - resultado.rect.w) / 2;
        resultado.rect.y = centroY - (resultado.rect.h / 2);

        // Paso 4: Liberar la superficie en RAM; los píxeles ya están seguros en VRAM
        SDL_FreeSurface(superficie);
    }
    return resultado;
}

// Libera de forma segura una textura en VRAM y deja el puntero en nullptr para
// evitar errores de doble liberación (double-free).
void destruirTexto(TextoVRAM& t) {
    if (t.textura != nullptr) {
        SDL_DestroyTexture(t.textura);
        t.textura = nullptr;
    }
}

// ============================================================================
// 7. LÓGICA DEL BUSCAMINAS (MEMORIA Y ALGORITMOS)
// ============================================================================

// Crea o reinicia por completo la matriz 2D en el Heap y los contadores de partida.
void crearTablero(int filas, int columnas) {
    // assign() reemplaza todos los elementos previos por filas nuevas de Casilla
    // recién construidas (todo en false y 0). Esto garantiza reinicios limpios.
    tablero.assign(filas, std::vector<Casilla>(columnas));
    casillasSegurasReveladas = 0;
    banderasColocadas = 0;
    std::cout << "Tablero de " << filas << "x" << columnas << " creado en el Heap.\n";
}

// Distribuye 'cantidadMinas' minas de forma uniforme y sin repeticiones.
void colocarMinas(int cantidadMinas, int filaSegura, int colSegura) {
    // 1. Recolectar índices lineales (f * COLUMNAS + c) fuera de la zona 3x3
    std::vector<int> candidatas;
    candidatas.reserve(FILAS * COLUMNAS);

    for (int f = 0; f < FILAS; ++f) {
        for (int c = 0; c < COLUMNAS; ++c) {
            int df = f - filaSegura;
            int dc = c - colSegura;
            if (df >= -1 && df <= 1 && dc >= -1 && dc <= 1) continue;

            candidatas.push_back(f * COLUMNAS + c);
        }
    }

    // 2. Motor estático: se siembra una sola vez durante la ejecución del programa
    static std::mt19937 generador(std::random_device{}());

    // 3. Barajado parcial de Fisher-Yates (exactamente 'cantidadMinas' swaps)
    const int totalCandidatas = static_cast<int>(candidatas.size());
    for (int i = 0; i < cantidadMinas; ++i) {
        std::uniform_int_distribution<int> dist(i, totalCandidatas - 1);
        int j = dist(generador);
        std::swap(candidatas[i], candidatas[j]);

        int idx = candidatas[i];
        int f = idx / COLUMNAS;
        int c = idx % COLUMNAS;
        tablero[f][c].tieneMina = true;
    }

    std::cout << cantidadMinas << " minas colocadas (zona segura en "
              << filaSegura << ", " << colSegura << ").\n";
}

// Recorre toda la matriz O(FILAS * COLUMNAS) y precalcula cuántas minas hay en
// las hasta 8 casillas adyacentes (vecindad de Moore 3x3) de cada celda segura.
void calcularMinasVecinas() {
    for (int f = 0; f < FILAS; ++f) {
        for (int c = 0; c < COLUMNAS; ++c) {
            if (tablero[f][c].tieneMina) continue; // Las minas no muestran número

            int conteo = 0;
            // Desplazamientos relativos df, dc en {-1, 0, +1}
            for (int df = -1; df <= 1; ++df) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (df == 0 && dc == 0) continue; // Ignorar la celda central

                    int nf = f + df;
                    int nc = c + dc;

                    // Protección obligatoria de límites: el operador [] de std::vector
                    // NO verifica rangos; leer índices < 0 o >= 15 causaría UB/Segfault.
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

// Actualiza la textura del marcador superior ("Minas: X").
// Solo se llama al iniciar partida o al alternar una bandera (nunca cada frame).
void actualizarTexturaContador() {
    // 1. Destruir la textura previa en VRAM antes de alojar la nueva (cero fugas)
    destruirTexto(textoContador);

    // 2. Construir la cadena actualizada (puede ser negativa si sobran banderas)
    int minasRestantes = TOTAL_MINAS - banderasColocadas;
    std::string mensaje = "Minas: " + std::to_string(minasRestantes);
    SDL_Color colorHUD = {240, 240, 240, 255};

    // 3. Subir la nueva textura centrada en la franja superior (0 .. OFFSET_Y)
    textoContador = crearTextoCentrado(fuenteUI, mensaje, colorHUD, OFFSET_Y / 2);
}

// Agrupa todos los pasos necesarios para arrancar o reiniciar una partida limpia.
void iniciarNuevaPartida() {
    crearTablero(FILAS, COLUMNAS);
    primerClicPendiente = true;
    actualizarTexturaContador();
    estadoActual = EstadoJuego::JUGANDO;
    std::cout << "Partida 15x15 lista. Esperando primer clic.\n";
}

// Alterna el estado de bandera con clic derecho en una casilla aún oculta.
void alternarBandera(int fila, int col) {
    // No se permite marcar casillas que ya fueron descubiertas
    if (tablero[fila][col].descubierta) return;

    tablero[fila][col].tieneBandera = !tablero[fila][col].tieneBandera;
    // Operador ternario: suma 1 si acabamos de colocarla, resta 1 si la quitamos
    banderasColocadas += tablero[fila][col].tieneBandera ? 1 : -1;

    // Invalidamos y regeneramos el texto del contador en VRAM
    actualizarTexturaContador();
}

// Revela la casilla (fila, col), evalúa derrota/victoria y, si tiene 0 minas
// vecinas, dispara una expansión recursiva (Flood-Fill / DFS) sobre sus vecinas.
void revelarCasilla(int fila, int col) {
    // --- CASOS BASE DE LA RECURSIÓN (evitan accesos ilegales y Stack Overflow) ---
    if (fila < 0 || fila >= FILAS || col < 0 || col >= COLUMNAS) return;
    if (estadoActual != EstadoJuego::JUGANDO) return;
    if (tablero[fila][col].descubierta || tablero[fila][col].tieneBandera) return;
    
    // Generar las minas recién en el primer clic izquierdo efectivo
    if (primerClicPendiente) {
        colocarMinas(TOTAL_MINAS, fila, col);
        calcularMinasVecinas();
        primerClicPendiente = false;
    }
    
    // Marcamos 'descubierta = true' ANTES de llamar recursivamente.
    // Esto garantiza progreso hacia el caso base y corta ciclos infinitos en el Stack.
    tablero[fila][col].descubierta = true;

    // --- CONDICIÓN DE DERROTA ---
    if (tablero[fila][col].tieneMina) {
        estadoActual = EstadoJuego::GAME_OVER;
        std::cout << "BOOM! Mina en (" << fila << ", " << col << "). GAME OVER.\n";
        return;
    }

    // --- CONDICIÓN DE VICTORIA EN O(1) ---
    casillasSegurasReveladas++;
    constexpr int totalSeguras = (FILAS * COLUMNAS) - TOTAL_MINAS; // 225 - 35 = 190
    if (casillasSegurasReveladas == totalSeguras) {
        estadoActual = EstadoJuego::VICTORIA;
        std::cout << "VICTORIA! Tablero despejado.\n";
        return;
    }

    // --- EXPANSIÓN EN CASCADA (FLOOD-FILL) ---
    // Si no toca ninguna mina, cada llamada apila un Stack Frame con las coordenadas
    // vecinas y abre automáticamente toda la región vacía contigua.
    if (tablero[fila][col].minasAlrededor == 0) {
        for (int df = -1; df <= 1; ++df) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (df == 0 && dc == 0) continue;
                revelarCasilla(fila + df, col + dc);
            }
        }
    }
}

// ============================================================================
// 8. INICIALIZACIÓN DEL HARDWARE Y CACHÉ DE RECURSOS
// ============================================================================
void inicializarSistema() {
    // Inicializar el subsistema de video (conexión con Wayland/X11 en Linux o Win32 en Windows)
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Error crítico: No se pudo inicializar SDL: " << SDL_GetError() << "\n";
        exit(1);
    }

    // Inicializar el subsistema de renderizado de tipografías TrueType
    if (TTF_Init() == -1) {
        std::cerr << "Error crítico: No se pudo inicializar TTF: " << TTF_GetError() << "\n";
        exit(1);
    }

    // Crear la ventana física centrada en el monitor principal
    ventana = SDL_CreateWindow("Buscaminas 15x15 - Roger & Pablito",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               ANCHO_VENTANA, ALTO_VENTANA, SDL_WINDOW_SHOWN);

    // Crear el contexto de renderizado acelerado por GPU (Direct3D/OpenGL/Vulkan según OS)
    renderizador = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_ACCELERATED);

    // Cargar el archivo tipográfico desde el directorio de ejecución (copiado por CMake)
    fuenteCeldas = TTF_OpenFont("GOUDYSTO.ttf", 16);
    fuenteUI     = TTF_OpenFont("GOUDYSTO.ttf", 20);
    fuenteTitulo = TTF_OpenFont("GOUDYSTO.ttf", 36);

    if (!fuenteCeldas || !fuenteUI || !fuenteTitulo) {
        std::cerr << "Error: No se encontró la fuente GOUDYSTO.ttf. Verifica la ruta.\n";
        exit(1);
    }

    // Precarga única en VRAM de todos los textos fijos de la interfaz
    SDL_Color blanco    = {255, 255, 255, 255};
    SDL_Color grisClaro = {220, 220, 220, 255};

    textoMenu       = crearTextoCentrado(fuenteUI,     "Click or press ENTER", blanco,    ALTO_VENTANA / 2);
    textoGameOver   = crearTextoCentrado(fuenteTitulo, "GAME OVER",            blanco,    ALTO_VENTANA / 2 - 25);
    textoVictoria   = crearTextoCentrado(fuenteTitulo, "YOU WIN!",             blanco,    ALTO_VENTANA / 2 - 25);
    textoReintentar = crearTextoCentrado(fuenteUI,     "Click to play again",  grisClaro, ALTO_VENTANA / 2 + 40);

    // Paleta clásica de colores RGB+Alpha para los números 1..8 del Buscaminas
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

    // Precarga en VRAM de los 8 dígitos: durante la partida solo haremos copias GPU->GPU
    for (int i = 1; i <= 8; ++i) {
        std::string digito = std::to_string(i);
        SDL_Surface* supNum = TTF_RenderUTF8_Solid(fuenteCeldas, digito.c_str(), coloresNumeros[i]);
        if (supNum != nullptr) {
            texturasNumeros[i] = SDL_CreateTextureFromSurface(renderizador, supNum);
            SDL_FreeSurface(supNum); // Liberar memoria temporal en RAM
        }
    }
}

// ============================================================================
// 9. PROCESAMIENTO DE ENTRADA (COLA DE EVENTOS DEL SISTEMA OPERATIVO)
// ============================================================================
void procesarEntrada() {
    SDL_Event evento; // Struct union en el Stack para recibir cada evento del OS

    // Vaciamos toda la cola de eventos pendientes de este fotograma
    while (SDL_PollEvent(&evento)) {
        // Cierre de ventana (botón X o señal del gestor de ventanas)
        if (evento.type == SDL_QUIT) {
            juegoActivo = false;
        }
        // Interrupción de teclado
        else if (evento.type == SDL_KEYDOWN) {
            if (evento.key.keysym.sym == SDLK_ESCAPE) {
                juegoActivo = false;
            }
            // Acepta tanto Enter principal como Enter del teclado numérico
            else if ((evento.key.keysym.sym == SDLK_RETURN ||
                      evento.key.keysym.sym == SDLK_KP_ENTER) &&
                     estadoActual != EstadoJuego::JUGANDO) {
                iniciarNuevaPartida();
            }
        }
        // Interrupción de ratón
        else if (evento.type == SDL_MOUSEBUTTONDOWN) {
            // Caso A: Si estamos en MENU_PRINCIPAL, GAME_OVER o VICTORIA,
            // cualquier clic izquierdo en la ventana arranca una partida nueva.
            if (estadoActual != EstadoJuego::JUGANDO) {
                if (evento.button.button == SDL_BUTTON_LEFT) {
                    iniciarNuevaPartida();
                }
            }
            // Caso B (rama 'else' excluyente): el clic que inició la partida NO
            // entra aquí; solo procesamos celdas cuando ya estábamos jugando.
            else {
                int mouseX = evento.button.x; // Píxel horizontal relativo a la ventana
                int mouseY = evento.button.y; // Píxel vertical relativo a la ventana

                constexpr int anchoTablero = COLUMNAS * TAMANO_CASILLA; // 480 px
                constexpr int altoTablero  = FILAS * TAMANO_CASILLA;    // 480 px

                // Validar límites en píxeles ANTES de dividir. Así evitamos que el
                // truncamiento hacia cero de enteros negativos (ej. -15 / 32 == 0)
                // dispare falsos clics sobre la fila 0 o columna 0.
                if (mouseX >= OFFSET_X && mouseX < OFFSET_X + anchoTablero &&
                    mouseY >= OFFSET_Y && mouseY < OFFSET_Y + altoTablero) {

                    // Transformación inversa: espacio de píxeles -> índices de matriz
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

// ============================================================================
// 10. LÓGICA POR FOTOGRAMA
// ============================================================================
void procesarLogica() {
    // Mantenemos esta etapa separada de dibujarPantalla() por arquitectura limpia
    // (patrón Input -> Update -> Render). Aquí viviría, por ejemplo, un cronómetro.
}

// ============================================================================
// 11. RENDERIZADO EN GPU (ÚNICO PUNTO DE CONTACTO CON EL BACK BUFFER)
// ============================================================================
void dibujarPantalla() {
    switch (estadoActual) {
        case EstadoJuego::MENU_PRINCIPAL:
            // Inundar el Back Buffer de azul marino
            SDL_SetRenderDrawColor(renderizador, 10, 20, 60, 255);
            SDL_RenderClear(renderizador);

            // Estampar la textura del menú desde VRAM al Back Buffer (Blit)
            if (textoMenu.textura != nullptr) {
                SDL_RenderCopy(renderizador, textoMenu.textura, nullptr, &textoMenu.rect);
            }
            break;

        case EstadoJuego::JUGANDO: {
            // Fondo gris oscuro para el área de juego
            SDL_SetRenderDrawColor(renderizador, 30, 30, 30, 255);
            SDL_RenderClear(renderizador);

            // 1. Dibujar el contador superior de minas restantes
            if (textoContador.textura != nullptr) {
                SDL_RenderCopy(renderizador, textoContador.textura, nullptr, &textoContador.rect);
            }

            // 2. Recorrer la matriz fila por fila (orden row-major = máxima localidad de caché L1)
            for (int fila = 0; fila < FILAS; ++fila) {
                for (int col = 0; col < COLUMNAS; ++col) {
                    // Referencia constante (&): leemos el struct directo en el Heap sin copiarlo
                    const Casilla& casilla = tablero[fila][col];

                    // Proyección de índices (fila, col) -> rectángulo físico en píxeles
                    SDL_Rect rect{
                        OFFSET_X + col * TAMANO_CASILLA,
                        OFFSET_Y + fila * TAMANO_CASILLA,
                        TAMANO_CASILLA,
                        TAMANO_CASILLA
                    };

                    // Color de relleno según si la celda está descubierta u oculta
                    if (casilla.descubierta) {
                        SDL_SetRenderDrawColor(renderizador, 200, 200, 200, 255);
                    } else {
                        SDL_SetRenderDrawColor(renderizador, 100, 120, 140, 255);
                    }

                    SDL_RenderFillRect(renderizador, &rect);

                    // Borde negro de 1 px para delimitar visualmente la cuadrícula
                    SDL_SetRenderDrawColor(renderizador, 0, 0, 0, 255);
                    SDL_RenderDrawRect(renderizador, &rect);

                    // Caso 1: Casilla descubierta con minas vecinas -> copiar dígito desde VRAM
                    if (casilla.descubierta && casilla.minasAlrededor > 0) {
                        SDL_Texture* texNum = texturasNumeros[casilla.minasAlrededor];
                        if (texNum != nullptr) {
                            int texW = 0, texH = 0;
                            // Consultar dimensiones de la textura en VRAM para centrarla
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
                    // Caso 2: Casilla oculta con bandera -> dibujar geometría escalada a TAMANO_CASILLA
                    else if (!casilla.descubierta && casilla.tieneBandera) {
                        int mastilX = rect.x + (TAMANO_CASILLA * 3) / 8;
                        int mastilY = rect.y + TAMANO_CASILLA / 5;
                        int mastilH = (TAMANO_CASILLA * 3) / 5;

                        // Mástil negro vertical
                        SDL_Rect mastil{mastilX, mastilY, 3, mastilH};
                        SDL_SetRenderDrawColor(renderizador, 20, 20, 20, 255);
                        SDL_RenderFillRect(renderizador, &mastil);

                        // Paño rojo de la bandera
                        SDL_Rect pano{mastilX + 3, mastilY, TAMANO_CASILLA / 3, TAMANO_CASILLA / 4};
                        SDL_SetRenderDrawColor(renderizador, 220, 40, 40, 255);
                        SDL_RenderFillRect(renderizador, &pano);
                    }
                }
            }
            break;
        }

        case EstadoJuego::GAME_OVER:
            // Pantalla roja de derrota con textos cacheados en VRAM
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
            // Pantalla verde de victoria con textos cacheados en VRAM
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

    // Double Buffering: intercambia el Back Buffer recién pintado con el Front Buffer
    // visible en el monitor en una sola operación atómica (evita tearing/parpadeo).
    SDL_RenderPresent(renderizador);
}

// ============================================================================
// 12. LIBERACIÓN ORDENADA DE MEMORIA (ORDEN INVERSO AL DE CREACIÓN)
// ============================================================================
void limpiarMemoria() {
    std::cout << "Liberando VRAM y RAM...\n";

    // 1. Destruir las 8 texturas de dígitos en VRAM
    for (SDL_Texture*& tex : texturasNumeros) {
        if (tex != nullptr) {
            SDL_DestroyTexture(tex);
            tex = nullptr;
        }
    }

    // 2. Destruir las texturas de interfaz en VRAM
    destruirTexto(textoContador);
    destruirTexto(textoReintentar);
    destruirTexto(textoVictoria);
    destruirTexto(textoGameOver);
    destruirTexto(textoMenu);

    // 3. Cerrar los tres manejadores de fuentes y apagar SDL_ttf
    if (fuenteTitulo != nullptr) TTF_CloseFont(fuenteTitulo);
    if (fuenteUI != nullptr)     TTF_CloseFont(fuenteUI);
    if (fuenteCeldas != nullptr) TTF_CloseFont(fuenteCeldas);
    TTF_Quit();

    // 4. Destruir el contexto gráfico, la ventana y apagar SDL2
    SDL_DestroyRenderer(renderizador);
    SDL_DestroyWindow(ventana);
    SDL_Quit();

    // Nota: El std::vector 'tablero' se libera automáticamente en su destructor
    // (RAII) al salir de main(), devolviendo toda su memoria del Heap al OS.
}

// ============================================================================
// 13. PUNTO DE ENTRADA Y GAME LOOP (~60 FPS)
// ============================================================================
int main(int argc, char* argv[]) {
    // 1. Preparar drivers, ventana, GPU y precargar recursos en VRAM
    inicializarSistema();

    // Presupuesto de tiempo por fotograma: 1000 ms / 60 FPS ≈ 16 ms
    const std::chrono::milliseconds tiempoPorFrame(16);

    // 2. Ciclo principal ininterrumpido del motor
    while (juegoActivo) {
        // Marca de tiempo al inicio del fotograma
        auto inicioFrame = std::chrono::high_resolution_clock::now();

        procesarEntrada(); // Leer hardware (teclado y ratón)
        procesarLogica();  // Actualizar estado del mundo
        dibujarPantalla(); // Enviar comandos de dibujo a la GPU

        // Calcular cuánto tiempo real consumió este fotograma
        auto finFrame = std::chrono::high_resolution_clock::now();
        auto tiempoProcesamiento =
            std::chrono::duration_cast<std::chrono::milliseconds>(finFrame - inicioFrame);

        // Si terminamos antes de los 16 ms, dormimos el hilo para que el planificador
        // del OS no mantenga un núcleo de CPU al 100% haciendo espera activa (busy-wait).
        if (tiempoProcesamiento < tiempoPorFrame) {
            std::this_thread::sleep_for(tiempoPorFrame - tiempoProcesamiento);
        }
    }

    // 3. Liberación determinista de recursos antes de terminar el proceso
    limpiarMemoria();
    return 0; // Código de salida 0: ejecución exitosa para POSIX y Windows
}
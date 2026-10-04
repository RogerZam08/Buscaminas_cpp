# Buscaminas en C++ 💣

Un motor de Buscaminas desarrollado en C++17 utilizando CMake. 
Proyecto colaborativo diseñado para dominar la arquitectura de software, gestión de memoria (Stack/Heap) y renderizado.

## Desarrolladores
* Roger Zambrano
* Pablito

## Tecnologías
* **Lenguaje:** C++17 (Uso de punteros inteligentes y la STL)
* **Compilación:** CMake (Out-of-source build, enlace estático)
* **Gráficos:** (Integrado automáticamente vía FetchContent)

## Tecnologías
* Diríjase a la pestaña de Releases en el panel derecho de este repositorio en GitHub.
* Descargue el archivo correspondiente a su sistema (Buscaminas_Windows.zip o Buscaminas_Linux.tar.gz).
* Extraiga el archivo y haga doble clic en el ejecutable. No requiere instalación.

🛠️ Cómo compilar desde el código fuente (Linux / Windows)

Requisitos previos:

CMake versión 3.14 o superior.

Conexión a internet activa (obligatoria únicamente la primera vez para descargar la librería gráfica SDL2).

Pasos de compilación:

Clonar el repositorio.

Crear un directorio de compilación: mkdir build && cd build

Generar archivos del sistema: cmake .. (Nota: Este paso puede tomar unos minutos mientras descarga SDL2).

Compilar el ejecutable: cmake --build .

Ejecutar: ./juego (o juego.exe en Windows)

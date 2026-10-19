# Harvest Moon SNES Recomp

[English](README.md) · **Español**

![estado: beta](https://img.shields.io/badge/estado-beta-orange)
![plataforma: Windows / Linux](https://img.shields.io/badge/plataforma-Windows%20x64%20%7C%20Windows%20ARM64%20%7C%20Linux-blue)
![ROM: no incluida](https://img.shields.io/badge/ROM-no%20incluida-red)

Un port nativo para PC de **Harvest Moon** (Super Nintendo, USA) hecho por
*recompilación estática*: el código 65816 del juego se traduce a C y se compila
como un programa de verdad, sobre [snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp).
Además del juego original agrega pantalla ancha, un menú de pausa estilo
Harvest Moon 64, juego infinito después del año 3, trucos, opciones de calidad
de vida y un launcher en varios idiomas.

> [!WARNING]
> **Esto es una beta.** En mis pruebas el juego se puede jugar de principio a
> fin, pero puede haber detalles. Repórtalos en Issues, por favor.

> [!IMPORTANT]
> **Necesitas tu propia copia del juego, obtenida legalmente.** Este
> repositorio **no contiene ROM ni datos del juego**, y no se distribuyen
> ejecutables ya compilados (tendrían código traducido del juego). El port lo
> compilas tú a partir del volcado de tu propio cartucho. Por favor no pidas ROMs.

## Por qué existe

Soy programador, pero nunca me enfoqué en videojuegos. Harvest Moon es de mis
juegos favoritos de la vida, y al ver que no existía una recompilación de este
juego, gracias a la IA pude trabajar en él.

**Este proyecto se hizo con ayuda de IA.** Yo lo dirigí y estuve supervisando
cada paso: qué hacer, cómo debía comportarse, probarlo en mi propia PC y
decidir qué se queda. Si no te gusta que se haya hecho con ayuda de IA, está
perfecto: simplemente no lo descargues.

## Funciones

### El juego, nativo
- Todo el juego recompilado a C (1.455 funciones), comparado cuadro por cuadro
  con los emuladores bsnes y Snes9x usando los mismos guiones de entrada.
- Pantalla completa, ventana escalable, guardados rápidos, rebobinar y turbo
  (del framework snesrecomp).
- Versiones para **Windows x64**, **Windows en ARM64** (Snapdragon) y **Linux**.
- Sin ventana de consola en Windows.

### Pantalla ancha 16:9 *(activada por defecto)*
Muestra más mapa a los lados en lugar de barras negras, con personajes y
animales dibujados también en los márgenes. Al llegar al borde de un mapa, en
cuartos pequeños y en el título vuelve al 4:3 original, porque ahí el juego no
tiene nada que dibujar. Con la opción apagada la imagen es idéntica al
original, píxel por píxel.

![Pantalla ancha](docs/screenshots/widescreen.png)

### Menú de pausa estilo Harvest Moon 64 *(tecla M o clic del stick derecho)*
Un menú de pausa a pantalla completa dibujado a la resolución del SNES, así que
sus píxeles son del mismo tamaño que los del juego:
- **Fecha y reloj** — estación, día, año, un reloj con manecillas *y* la hora en
  número, dinero y energía.
- **Herramientas** — casillas de mano y mochila, y la bodega en cuadrícula;
  equipar, cambiar y guardar con las mismas reglas que la bodega del juego.
- **Corazones** — la cara de cada chica (su propio sprite, leído de tu ROM) con
  diez corazones cuyo color sube con el cariño.
- **Granja** — envíos, madera, forraje, vacas, felicidad, abrazos al perro y el
  clima de mañana.
- **La música sigue sonando** con el menú abierto.

| Herramientas | Corazones | Español |
|---|---|---|
| ![Herramientas](docs/screenshots/pause_tools.png) | ![Corazones](docs/screenshots/pause_hearts.png) | ![Menú en español](docs/screenshots/pause_tools_es.png) |

### Juego infinito *(activado por defecto)*
El original termina el 30 de verano del año 3. Ahora ves el final completo y
luego despiertas esa misma mañana y sigues con tu granja en el año 4, 5 y más.
El diario muestra "4th", "5th"… correctamente.

### OPTIONS en la pantalla de título
Una cuarta opción debajo de HOW TO PLAY que abre el menú de opciones.

![Título con OPTIONS](docs/screenshots/title_options.png)

### Trucos *(apagados por defecto)*
Dinero infinito · energía infinita · detener el reloj · madera y forraje
infinitos · semillas y alimento infinitos · controlar el clima. Solo actúan con
una partida cargada y nunca ponen valores que el juego no pueda tener, así que
tu partida sigue válida si los apagas.

### Calidad de vida
Texto rápido (4×) · días más largos (1.5× a 4×) · opciones dentro del juego
(Esc o Select+L3 abre el launcher sobre el juego).

### Launcher
Con aspecto de Harvest Moon (dibujo original, colores de granja e ícono propio),
en **inglés, español o italiano** (inglés por defecto; la elección se guarda y
también cambia la página de Mods y el menú de pausa). Puedes poner tu propia
portada guardándola como `assets/img/portada.png` (o `.jpg`) junto al juego.

| Launcher | Página de Mods |
|---|---|
| ![Launcher](docs/screenshots/launcher.png) | ![Mods](docs/screenshots/mods.png) |

## Controles (teclado, se cambian en el launcher)

| Tecla | SNES / acción |
|---|---|
| Flechas | Cruceta |
| X / Z / S / A | A / B / X / Y |
| C / V | L / R |
| Enter / Shift derecho | Start / Select |
| M (o R3) | Menú de pausa |
| Esc (o Select+L3) | Opciones (launcher sobre el juego) |
| P | Pausa simple |
| Alt+Enter | Pantalla completa |
| Shift+F1…F10 / F1…F10 | Guardado / carga rápida |
| F12 (mantener) | Rebobinar |
| Tab (mantener) | Turbo |

Los controles de Xbox, PlayStation y Switch Pro se detectan solos.

## Cómo compilarlo

Necesitas: Git, Python 3, Rust (stable), CMake, Ninja y un compilador de C/C++
(GCC o Clang), además de **tu propia ROM** de *Harvest Moon (USA)*:

| | |
|---|---|
| SHA-256 | `73a3aa87ddd5ce5df5ba51292316f42b6e128280179b0a1b11b4dcddc17d4163` |
| CRC32 | `f829129e` |
| MD5 | `c9bf36a816b6d54aed79d43a6c45111a` |

```sh
git clone https://github.com/RodriLoy/HarvestMoonSNESRecomp.git
cd HarvestMoonSNESRecomp
bash tools/apply_patches.sh                      # framework y launcher en sus versiones fijadas, más los parches de este port
RUSTUP_TOOLCHAIN=stable bash tools/regen.sh --rom /ruta/a/tu/rom.sfc   # genera src/gen desde TU ROM
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/HarvestMoonSNESRecomp
```

Para Windows desde Linux:
- x64: `-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake` (MinGW-w64)
- ARM64: `-DCMAKE_TOOLCHAIN_FILE=cmake/zig-windows-arm64.cmake` (Zig, `pip install ziglang`)

`tools/regen.sh` rechaza cualquier ROM que no coincida con las huellas de
arriba. El C generado (`src/gen/`) sale del juego, así que nunca se sube al
repositorio: `.gitignore` lo bloquea, igual que cualquier ROM o partida guardada.

## Estructura

| Ruta | Qué hay |
|---|---|
| `src/hm_*.c` | Este port: menú de pausa, pantalla ancha, juego infinito, trucos, calidad de vida, OPTIONS, traducciones, aspecto del launcher |
| `mods/preloaded/packages/` | Los paquetes de trucos y calidad de vida de la página de Mods |
| `tools/` | Paso de ROM a C, ajustes a la generación, generadores de fuentes, íconos y dibujo |
| `framework_patches/`, `recomp_ui_patches/` | Los cambios de este port a snesrecomp y recomp-ui |
| `recomp/` | Configuración del recompilador y perfiles de cobertura (solo direcciones, sin datos del juego) |
| `ESTADO.md` | Notas detalladas del desarrollo |

## Estado

**Beta.** Límites conocidos:
- El juego infinito se probó forzando la fecha al último día; todavía no se ha
  hecho una partida completa de tres años.
- Solo se admite la versión USA del juego.

## Créditos y licencia

En [CREDITS.md](CREDITS.md) está todo aquello sobre lo que se construye este
port, con agradecimiento a quienes lo hicieron. El código propio de este
repositorio está bajo la licencia [PolyForm Noncommercial 1.0.0](LICENSE); no
cubre ninguna parte del juego. *Harvest Moon* pertenece a sus dueños; este es
un proyecto de fan sin fines de lucro y no está afiliado a ellos.

---

**Lo que sigue:** también estoy trabajando en una **descompilación** del juego
—reescribir sus rutinas como C legible, verificado contra el original— como un
proyecto aparte.

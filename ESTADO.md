# Estado de la recompilación (pausada el 19-oct-2026)

Este archivo es la "memoria" del proyecto: qué está hecho, cómo se hace cada
cosa y qué falta, para retomarlo sin perder nada. Por ahora el trabajo sigue
en el proyecto de descompilación (HarvestMoonDecomp).

## Versión actual: 0.5

Port nativo de Harvest Moon (SNES, USA) hecho con SNESRecomp
(framework fijado en `framework_pins.txt`, con 4 parches propios en
`framework_patches/`).

| Función | Dónde | Estado |
|---|---|---|
| Juego completo recompilado (1.455 funciones compiladas) | `src/gen` (se genera de tu ROM) | Verificado contra bsnes y snes9x |
| Pantalla completa, ventana escalable, guardado rápido, rebobinar, turbo | framework | Funciona |
| Trucos: dinero, energía, reloj, madera/forraje, semillas, clima | `src/hm_cheats.c` | Funciona (apagados por defecto) |
| Texto rápido, días más largos | `src/hm_qol.c` | Funciona |
| Menú de pausa estilo HM64 (tecla M / R3) | `src/hm_pause.c` | Funciona |
| OPTIONS en la pantalla de título | `src/hm_title.c` | Funciona |
| Menú de opciones en juego (Esc) | framework + parche 0002 | Funciona |
| Pantalla ancha 16:9 | `src/hm_widescreen.c`, `tools/apply_overrides.py` | Funciona (activada por defecto) |
| Sin ventana de consola en Windows | `CMakeLists.txt` | Funciona |
| Windows x64 y Windows ARM64 | `cmake/*.cmake` | Compilan; ARM64 sin probar en tu PC |

Mapa de memoria con nombres: `src/hm_ram.h` (lo usa también la descompilación).

## Cómo funciona cada mejora (resumen)

- **Trucos y calidad de vida**: paquetes de mods (`mods/preloaded/packages/`)
  con plugins en C que actúan al final de cada cuadro y solo con una partida
  cargada. Nunca ponen valores que el juego no pueda tener.
- **Menú de pausa**: panel dibujado por el host (512x448) mientras el juego
  queda congelado. Equipar herramientas imita la rutina de la bodega del juego.
- **OPTIONS en el título**: la palabra se arma con letras de las otras tres
  opciones (leídas de la VRAM) y se dibuja como objeto del PPU; el cursor del
  juego se reposiciona para que su propio movimiento caiga donde queremos.
- **Pantalla ancha**: campo de 342 px. Cada lado se abre según el rango de
  cámara del mapa ($ED/$F1), así nunca se ve fuera del mapa; la capa de texto
  (BG3) queda en el centro; el recorte de sprites de PrepareOAMData se amplía
  al generar el código. Con la opción apagada, la imagen es idéntica al
  original (comprobado cuadro por cuadro).

## Cómo se compila

```
RUSTUP_TOOLCHAIN=stable bash tools/regen.sh      # genera src/gen desde tu ROM
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Windows x64: `-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake`.
Windows ARM64: `-DCMAKE_TOOLCHAIN_FILE=cmake/zig-windows-arm64.cmake` (Zig).

## Pruebas

- Prueba de regresión: la demo (141 cuadros) y una partida nueva
  (192 cuadros) deben dar la misma imagen y la misma memoria, byte por byte.
- Comparación con emuladores de referencia (bsnes, snes9x) con los mismos
  guiones de entrada.

## Pendiente (para cuando se retome)

1. Probar la versión ARM64 en tu PC y corregir lo que aparezca.
2. Opcional: saltar la intro (calidad de vida).
3. Opcional: personalizar el launcher (aspecto Harvest Moon, todo en español).
4. Descartado por ahora: modo 120/144 Hz (tu pantalla es de 60 Hz).
5. Cuando la descompilación tenga un sistema entero verificado, decidir si
   el juego pasa a usar esas rutinas reescritas.

## Historial

| Versión | Qué trajo |
|---|---|
| 0.1 | Recompilación jugable, verificada contra bsnes/snes9x |
| 0.2 | Trucos y calidad de vida |
| 0.3 | Menú de pausa y menú de opciones en juego |
| 0.4 | OPTIONS en el título, versión ARM64 |
| 0.5 | Pantalla ancha 16:9, sin consola; la descompilación pasa a su propio proyecto |

# Estado de la recompilación (actualizado el 19-oct-2026)

Este archivo es la "memoria" del proyecto: qué está hecho, cómo se hace cada
cosa y qué falta, para retomarlo sin perder nada. La descompilación está
pausada en su propio proyecto (ver HarvestMoonDecomp/ESTADO.md).

## Versión actual: 0.8

Port nativo de Harvest Moon (SNES, USA) hecho con SNESRecomp
(framework fijado en `framework_pins.txt`, con parches propios en
`framework_patches/` y `recomp_ui_patches/`).

| Función | Dónde | Estado |
|---|---|---|
| Juego completo recompilado (1.455 funciones compiladas) | `src/gen` (se genera de tu ROM) | Verificado contra bsnes y snes9x |
| Pantalla completa, ventana escalable, guardado rápido, rebobinar, turbo | framework | Funciona |
| Trucos: dinero, energía, reloj, madera/forraje, semillas, clima | `src/hm_cheats.c` | Funciona (apagados por defecto) |
| Texto rápido, días más largos | `src/hm_qol.c` | Funciona |
| Menú de pausa estilo HM64 (tecla M / R3): pixel art 256x224, caras de las chicas desde la ROM, música sin cortar | `src/hm_pause.c`, `tools/gen_pixfont.py`, `tools/gen_icons.py`, parche 0007 | Funciona |
| Juego infinito (ver el final y seguir jugando, año 4+) | `src/hm_endless.c`, `tools/apply_overrides.py` | Funciona (probado forzando la fecha) |
| OPTIONS en la pantalla de título | `src/hm_title.c` | Funciona |
| Menú de opciones en juego (Esc) | framework + parche 0002 | Funciona |
| Pantalla ancha 16:9 | `src/hm_widescreen.c`, `tools/apply_overrides.py` | Funciona (activada por defecto) |
| Sin ventana de consola en Windows | `CMakeLists.txt` | Funciona |
| Varios idiomas (inglés por defecto, español, italiano): launcher, Mods, menú de pausa | `recomp-ui` (parche), `src/hm_i18n.c` | Funciona |
| Aspecto Harvest Moon en el launcher (colores, dibujo original, ícono) | `src/hm_launcher_theme.c`, `tools/gen_boxart.py`, `src/hm_app.rc`, `recomp-ui` (parche 0002) | Funciona |
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
- **Aspecto del launcher**: `configure_launcher` entrega una paleta propia
  (`gi->custom_theme`, campo nuevo del parche 0002 de recomp-ui): fondo verde
  noche, ámbar de luna como color principal, verde hoja en los títulos, sin
  líneas de CRT. El dibujo (`launcher_assets/img/boxart.tga`) y el ícono
  (`icon.ico`/`icon.png`) son originales, hechos con figuras por
  `tools/gen_boxart.py`; no copian la portada del juego.
- **Juego infinito**: el final empieza al cargar la casa el 30 de verano
  del año 3 ($83:DED3). Ese día se guarda en memoria, cada cuadro, un estado
  "rollback" (el único que sirve a mitad de una transición). Cuando el paisaje
  final lleva 15 s en pantalla, se carga ese estado y la comprobación de la
  fecha falla una vez (`hm_endless_ending_day()`), así que el día sigue normal.
  El diario muestra "4th, 5th..." (la tabla del juego solo tiene 1st-3rd).
- **Menú de pausa nuevo**: se dibuja a 256x224 y se duplica, con fuentes pixel
  (Pixelify Sans y Tiny5, licencia OFL, en `tools/fonts/`) e íconos originales.
  Las caras son los sprites del juego (hoja de NPCs en 0x78000 de la ROM y sus
  paletas en 0x14DE20..); se identificó a cada chica hablándole en el festival
  de las flores y viendo qué contador de corazones subía. La música sigue
  porque el host alimenta al SPC por tiempo real mientras la CPU espera
  (parche de framework 0007, `RtlApuIdleRun`).
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
2. Descartado por ahora: saltar la intro (decidido el 19-oct-2026).
3. Descartado por ahora: modo 120/144 Hz (tu pantalla es de 60 Hz).
4. Cuando la descompilación tenga un sistema entero verificado, decidir si
   el juego pasa a usar esas rutinas reescritas.

## Historial

| Versión | Qué trajo |
|---|---|
| 0.1 | Recompilación jugable, verificada contra bsnes/snes9x |
| 0.2 | Trucos y calidad de vida |
| 0.3 | Menú de pausa y menú de opciones en juego |
| 0.4 | OPTIONS en el título, versión ARM64 |
| 0.5 | Pantalla ancha 16:9, sin consola; la descompilación pasa a su propio proyecto |
| 0.6 | Launcher, Mods y menú de pausa en varios idiomas (inglés por defecto) |
| 0.7 | Launcher con aspecto Harvest Moon e ícono propio |
| 0.8 | Juego infinito; menú de pausa estilo HM64 con caras, reloj y música |

# Credits / Créditos

This port stands on other people's work. Thank you to all of them.
Este port se apoya en el trabajo de otras personas. Gracias a todas.

## The game / El juego

**Harvest Moon** (Super Nintendo, 1996/1997) — © Pack-In-Video / AMCCUS,
published in North America by Natsume. *Harvest Moon* and all related names,
characters and graphics belong to their respective owners. This project is
not affiliated with, endorsed or sponsored by them. No part of the game is
included: you need your own legally obtained copy.

**Harvest Moon** (Super Nintendo, 1996/1997) — © Pack-In-Video / AMCCUS,
publicado en Norteamérica por Natsume. *Harvest Moon* y todos los nombres,
personajes y gráficos relacionados pertenecen a sus dueños. Este proyecto no
está afiliado, respaldado ni patrocinado por ellos. No se incluye ninguna
parte del juego: necesitas tu propia copia obtenida legalmente.

## Tools and code used / Herramientas y código usados

| Project | Author | Used for / Uso | License |
|---|---|---|---|
| [snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp) | RetroPortingToolKit / Matthew Stanley | The static recompiler, runtime and desktop host this port is built on (submodule + patches) / El recompilador estático, runtime y host de escritorio sobre el que está hecho | PolyForm Noncommercial 1.0.0 |
| [recomp-ui](https://github.com/mstan/recomp-ui) | Matthew Stanley | The launcher (ROM picker, settings, Mods page) (submodule + patches) / El launcher | MIT |
| [HM-Decomp](https://github.com/ElisaS-Codes/HM-Decomp) | ElisaS-Codes and DasK (since 2023) | The disassembly whose labels and notes made the RAM map, the endless-game hook and the pause menu possible / El desensamblado cuyas etiquetas y notas hicieron posible el mapa de RAM, el juego infinito y el menú de pausa | — |
| [SDL3](https://libsdl.org) | SDL contributors | Window, input, audio | zlib |
| [Dear ImGui](https://github.com/ocornut/imgui) | Omar Cornut | Launcher UI (via recomp-ui) | MIT |
| [Pixelify Sans](https://github.com/eifetx/Pixelify-Sans) | The Pixelify Sans Project Authors | Pause menu text / Texto del menú de pausa | SIL OFL 1.1 |
| [Tiny5](https://github.com/Gissio/font_tiny5) | The Tiny5 Project Authors | Pause menu small text / Texto pequeño del menú | SIL OFL 1.1 |
| [DejaVu Fonts](https://dejavu-fonts.github.io) | DejaVu authors | Earlier menu text / Texto de menús anteriores | Bitstream Vera / DejaVu |
| [bsnes](https://github.com/bsnes-emu/bsnes), [Snes9x](https://github.com/snes9xgit/snes9x) | their authors | Reference emulators to verify the recompilation frame by frame (not distributed) / Emuladores de referencia para verificar la recompilación (no se distribuyen) | GPLv3 / Snes9x license |
| [Zig](https://ziglang.org), [MinGW-w64](https://www.mingw-w64.org) | their authors | Windows ARM64 / x64 cross-compilers / Compiladores para Windows | MIT / ZPL etc. |

The launcher's own fonts and images (Lato, Noto Sans Symbols 2, OpenMoji…) come
with recomp-ui and snesrecomp; see `snesrecomp/THIRD_PARTY_ATTRIBUTION.md`.
Las fuentes e imágenes propias del launcher vienen con recomp-ui y snesrecomp.

## Original art in this repository / Arte original en este repositorio

The launcher artwork (night farm under the moon), the program icon and the 16x16
item icons of the pause menu were drawn for this project from simple shapes
(`tools/gen_boxart.py`, `tools/gen_icons.py`); they are not taken from the game.
The girls' faces in the pause menu are **not** included: they are read at run
time from your own ROM.

El dibujo del launcher (granja de noche bajo la luna), el ícono del programa y
los íconos de objetos del menú de pausa se dibujaron para este proyecto a partir
de figuras simples; no salen del juego. Las caras de las chicas del menú de pausa
**no** vienen incluidas: se leen de tu propia ROM al jugar.

## AI assistance / Asistencia de IA

The code of this port was written with the help of an AI coding assistant,
under constant review and direction by the author. See the README.

El código de este port se escribió con ayuda de un asistente de programación con
IA, siempre bajo la revisión y dirección del autor. Ver el README.

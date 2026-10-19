# Decompilación de Harvest Moon (SNES)

Este documento explica cómo se está decompilando el juego, poco a poco.

## Qué ya existe

El recompilador ya tradujo todo el juego a C (`src/gen/`), pero ese C es una
traducción literal de instrucciones del 65816: `cpu->A`, banderas, saltos.
Funciona, pero no se entiende. **Decompilar** es reescribir cada rutina como
C que una persona pueda leer, con nombres que digan qué hace.

## Paso 1: nombres (hecho)

`tools/import_decomp_names.py` toma los nombres que la comunidad le puso a
las rutinas en el desensamblado HM-Decomp (ElisaS-Codes) y los guarda en
`recomp/decomp_names.tsv`. Al regenerar, `tools/apply_overrides.py` renombra
las funciones: `bank_83_8138_M1X0` pasa a llamarse `GetRNG_M1X0`.
Hoy: **478 rutinas con nombre**. Es solo un cambio de nombre; el programa es
idéntico (lo comprueba la prueba de regresión).

## Paso 2: reescrituras en C legible (empezado)

Cada rutina reescrita vive en `src/decomp/<sistema>.c`:

| Rutina | Archivo | Qué hace |
|---|---|---|
| GetRNG | rng.c | Generador de números aleatorios (LFSR de 24 bits) |
| AddWood / AddGrass / AddPlayerHappiness | stock.c | Suma a madera, forraje y felicidad, tope 999 |
| ChangeStamina | stamina.c | Energía del jugador y las etapas de cansancio |
| GetsCowPointer / GetChickenPointer | animals.c | Dónde están los datos de cada vaca y gallina |

Las direcciones de memoria tienen nombre en `src/hm_ram.h`.

## Cómo sabemos que una reescritura es correcta

No basta con que "parezca bien". Cada rutina reescrita se compara contra la
original, byte por byte:

* **Mientras juegas** (`HM_DECOMP_CHECK=1`): en cada llamada se guarda una
  copia de la memoria, corre la rutina original, luego la reescritura sobre
  la copia, y se comparan la memoria completa (128 KB) y los registros.
* **Con entradas al azar** (`HM_DECOMP_FUZZ=<cuadro>:<veces>`): en ese cuadro
  se llama a cada rutina miles de veces con valores aleatorios (incluidos los
  bordes, como 0, 998, 999, 1000) y se compara cada llamada. Después la
  máquina se restaura y el juego sigue igual.

Ejemplo real: la prueba al azar encontró que `ChangeStamina` decide el signo
de la energía con la bandera N que dejó *quien la llama*, no con el valor en
A. Es una rareza del código original; la reescritura ahora la imita.

Resultado actual: 7 rutinas, 2.000 llamadas al azar cada una + 5.549
llamadas reales a GetRNG en una partida nueva: **0 diferencias**.

## Cómo seguir

1. Elegir una rutina "hoja" (que no llame a otras) de HM-Decomp.
2. Escribirla en `src/decomp/`, registrarla en `hm_decomp.c` con su rango de
   entradas para el fuzz.
3. `tools/regen.sh`, compilar, correr con `HM_DECOMP_FUZZ` y `HM_DECOMP_CHECK`.
4. Cuando todas las hojas de un sistema estén verificadas, subir un nivel
   (rutinas que llaman a esas hojas).

El juego normal NO usa todavía las reescrituras: corre el código recompilado
original, así que nada de esto puede romper una partida. Cambiar el juego
para que use las reescrituras es un paso posterior, cuando un sistema entero
esté verificado.

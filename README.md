# Emulador CHIP-8 en C Puro

Emulador modular del lenguaje de maquina **CHIP-8** escrito en **C99 puro**, sin
dependencias externas, pensado como base didactica para comprendre el
funcionamiento de una CPU de 8 bits desde sus primitivas: un contador de
programa, un registro indice, dieciseis registros de 8 bits, 4 KB de RAM
direccionable y un framebuffer monocromo de 64x32.

---

## 1. Que es CHIP-8

CHIP-8 no es un lenguaje de alto nivel: es un **conjunto de 35 instrucciones
codificadas en 16 bits** que se ejecutaba directamente sobre el microprocesador
COSMAC 1802 del RCA 1802. Fue disenado a finales de la decada de 1970 por
Joseph Toomey, y su unico proposito era ejecutar pequenos juegos: un
laberinto, un/langton, el Pong, o el popular juego de la serpiente.

Sus caracteristicas originales son:

| Recurso | Valor |
| --- | --- |
| Memoria de direcciones | 12 bits (0x000 - 0xFFF, 4 KB) |
| Formato de instruccion | 2 bytes, big-endian, fetch de PC + 2 |
| Registros de datos | 16 x 8 bits (V0 - VF) |
| Registro indice | 1 x 16 bits (I), con 12 bits utiles |
| Monitor | 64 x 32 pixeles monocromos (2048 pixeles) |
| Teclado | 16 teclas, mapeadas a 0xF - 0 y 1 - A |
| Velocidad de CPU | ~500 ciclos por segundo |
| Velocidad de temporizadores | 60 Hz, independiente de la CPU |

`VF` es el "registro oculto": la mayor parte de las ROMs lo usan como bandera
booleana para el desbordamiento aritmetico y para la colision de sprites.

## 2. Que hace este proyecto

Este emulador implementa el nucleo completo de la maquina virtual en C99,
organizado de forma estrictamente modular:

* **Nucleo de emulacion** (`src/chip8.c`): inicializacion del estado, carga
  del FONT_SET, reloj de los temporizadores y decodificador de opcodes.
  No sabe nada de ficheros, de ventanas ni de hilos.
* **Presentacion** (`src/main.c`): carga de la ROM desde disco
  (`chip8_load_rom()`, la unica funcion que abre ficheros), bucle principal de
  escritorio a 500 Hz y regulacion de los temporizadores a 60 Hz.
* **Arnes de pruebas** (`tests/tests.c`): 14 casos de prueba automatizados con
  `<assert.h>`, sin ninguna dependencia de la capa grafica.

Gracias a esa separacion, el nucleo es **libre de estado global**: todas las
funciones reciben un puntero a la estructura `Chip8`, de modo que pueden
coexistir varias maquinas virtuales simultaneamente. Este es el patron que
utiliza la infraestructura de pruebas de este repositorio.

### Instrucciones implementadas

| Opcode | Mnemonico | Descripcion |
| --- | --- | --- |
| `0x00E0` | `CLS` | Borra la pantalla |
| `0x00EE` | `RET` | Retorna de la subrutina (`PC = I`) |
| `0x1NNN` | `JP addr` | Salto incondicional a la direccion de 12 bits `NNN` |
| `0x2NNN` | `CALL addr` | Invoca la subrutina en `NNN` |
| `0x3XNN` | `SE Vx, NN` | Salta si `Vx == NN` |
| `0x4XNN` | `SNE Vx, NN` | Salta si `Vx != NN` |
| `0x5XY0` | `SE Vx, Vy` | Salta si `Vx == Vy` |
| `0x6XNN` | `LD Vx, NN` | Carga un inmediato de 8 bits en `Vx` |
| `0x7XNN` | `ADD Vx, NN` | Suma un inmediato con wrap-around de 8 bits |
| `0x8XY0` | `LD Vx, Vy` | Copia `Vy` en `Vx` |
| `0x8XY1` | `ADD Vx, Vy` | Suma `Vy`; `VF = 1` si hay desbordamiento |
| `0x9XY0` | `SNE Vx, Vy` | Salta si `Vx != Vy` |
| `0xANNN` | `LD I, NNN` | Carga el registro indice |
| `0xBNNN` | `JP V0, NNN` | Salta a `NNN + V0` |
| `0xCXKK` | `RND Vx, KK` | `Vx = rand() & KK` |
| `0xDXYN` | `draw Vx, Vy` | Dibuja un sprite de `N` filas con XOR y colision en `VF` |
| `0xEX9E` | `SKP Vx` | Salta si la tecla `X` esta pulsada |
| `0xEXA1` | `SKNP Vx` | Salta si la tecla `X` **no** esta pulsada |
| `0xFX15` | `LD Vx, DT` | `Vx = delay_timer` |
| `0xFX18` | `LD Vx, ST` | `Vx = sound_timer` |
| `0xFX55` | `LD Vx, I` | Copia `V0..Vx` a la memoria, desde `I` |
| `0xFX65` | `LD I, Vx` | Carga `V0..Vx` desde la memoria, desde `I` |

Los codigos no implementados se tratan como `NOP`, igual que hacen la mayoria
de emuladores, para no bloquearse ante ROMs de variantes como SCHIP o XO-CHIP.

## 3. Estructura del proyecto

```
chip-8/
├── Makefile              Automatizacion de compilacion, pruebas y limpieza
├── README.md             Este documento (guia para personas)
├── AGENTS.md             Contexto e instrucciones para agentes de IA
├── include/
│   └── chip8.h           Estructura de estado y prototipos de la API
├── src/
│   ├── chip8.c           Nucleo: init, carga de ROM, timers y decodificador
│   └── main.c            Presentacion: carga desde disco y bucle principal
└── tests/
    └── tests.c           Arnes de pruebas unitarias con <assert.h>
```

## 4. Requisitos previos

Tan solo hace falta un compilador de C y la utilidad `make`:

* **GCC** 8 o superior (recomendado), o **Clang** 7 o superior.
* **GNU Make** 3.81 o superior.
* No se requiere ninguna biblioteca grafica ni gestor de paquetes. El
  binario se enlaza solo con la biblioteca estandar de C.

Comprobar la instalacion:

```bash
gcc --version
make --version
```

## 5. Compilacion

Situate en la raiz del proyecto y ejecute:

```bash
make
```

Esto compila `src/chip8.c` y `src/main.c` con `-std=c99 -O2` y un conjunto
estricto de avisos (`-Wall -Wextra -Wpedantic -Wconversion`), y genera el
ejecutable `./chip8` junto con los objetos intermedios en `build/`.

Para recompilar desde cero:

```bash
make rebuild
```

Para generar un binario con simbolos de depuracion:

```bash
make debug
gdb ./chip8
```

## 6. Ejecucion

El emulador toma como primer argumento la ruta del fichero de ROM. Las ROM
de CHIP-8 son ficheros binarios planos, generalmente con extension `.ch8`:

```bash
./chip8 ruta/a/la_rom.ch8
```

Opcionalmente, un segundo argumento permite ajustar cuantos ciclos de CPU se
ejecutan entre cada tic de temporizador (valor por defecto: `8`, que es el
ajuste correcto para 500 Hz de CPU y 60 Hz de temporizadores):

```bash
./chip8 ruta/a/la_rom.ch8 8
```

Atajo desde `make`:

```bash
make run ROM=ruta/a/la_rom.ch8
```

### Mapa del teclado

El teclado de la maquina virtual utiliza 16 teclas mapeadas a los digitos
hexadecimales. La tabla siguiente indica la equivalencia con un teclado QWERTY
convencional, que es la que usan las implementaciones con SDL2 o Raylib:

| Maquina | Tecla QWERTY | | Maquina | Tecla QWERTY |
| --- | --- | --- | --- | --- |
| `0` | `X` | | `8` | `Q` |
| `1` | `1` | | `9` | `W` |
| `2` | `2` | | `A` | `E` |
| `3` | `3` | | `B` | `R` |
| `4` | `Q` | | `C` | `A` |
| `5` | `W` | | `D` | `S` |
| `6` | `E` | | `E` | `D` |
| `7` | `R` | | `F` | `F` |

### Estado actual: modo headless

La version actual ejecuta la CPU en **modo headless**: emula el
retroprocesamiento completo y vuelca un diagnostico periodico por consola, pero
no abre ninguna ventana. `src/main.c` contiene bloques comentados y
listos para activar con la integracion de **SDL2** o **Raylib**; basta con
descomentar el bloque correspondiente y llamar a los ayudantes
`video_init()`, `video_poll()` y `video_render()` en las zonas `ZONA 1`
(entrada), `ZONA 3` (sonido) y `ZONA 5` (video) del bucle principal.

Salida de ejemplo:

```
==========================================================
  Emulador CHIP-8 en C puro (C99)
==========================================================
  ROM ............ demos/mi_rom.ch8
  Cargados ...... 3968 bytes en 0x0200
  Reloj CPU ..... 500 Hz (tic de timer cada 8 ciclos)
  PC ............ 0x0200
  I ............. 0x0000
  ...
```

Para detener la ejecucion, pulse `Ctrl+C`.

## 7. Pruebas unitarias

La bateria de pruebas se compila y ejecuta con un unico comando:

```bash
make test
```

El binario resultante (`build/tests`) es independiente y contiene **14 casos de
prueba** que cubren:

1. `chip8_init`: estado inicial determinista y carga del FONT_SET de 80 bytes.
2. `6XNN`: carga de valores inmediatos, incluido el maximo `0xFF`.
3. `ANNN`: carga del registro indice, sin efectos secundarios.
4. `3XNN`, `4XNN`, `5XY0`: saltos condicionales por los caminos verdadero,
   falso, y encadenamiento de saltos.
5. `7XNN`: suma inmediata con desbordamiento y wrap-around de 8 bits.
6. `EX9E` y `EXA1`: saltos basados en el estado del teclado.
7. `DXYN`: logica XOR, bandera de colision en `VF` y recorte en los limites
   del monitor.
8. Temporizadores a 60 Hz: cuenta descendente y saturacion en cero.
9. Fetch big-endian y semantica del PC, incluidos los saltos absolutos.
10. Contrato de carga de ROM: inyeccion binaria en `0x200`, ejecucion de
    la ROM cargada y truncado en el limite de la RAM.
11. `8XY0`, `8XY1`, `9XY0`: copia, suma con `VF` y salto por desigualdad.
12. `FX55` y `FX65`: transferencias entre registros y RAM.
13. Aislamiento: dos maquinas virtuales simultaneas sin interferencias.
14. Contrato de la estructura: dimensiones, offsets y tipos.

Salida esperada en caso de exito:

```
==========================================================
  RESULTADO: 14 de 14 casos de prueba superados
==========================================================

  >>> TODAS LAS PRUEBAS UNITARIAS HAN PASADO CORRECTAMENTE <<<
```

El arnes usa `<assert.h>` de la biblioteca estandar: en cuanto una asercion
falla, el proceso imprime fichero, linea y expresion, y aborta con codigo de
salida distinto de cero. Por eso `make test` es suitable para integracion
continua.

Ejecutar la bateria bajo **AddressSanitizer** y **UndefinedBehaviorSanitizer**
para detectar fugas de memoria, desbordamientos y comportamiento indefinido:

```bash
make asan
```

## 8. Limpieza

Para eliminar el binario, los objetos y el directorio `build/`:

```bash
make clean
```

## 9. Objetivos disponibles

| Comando | Descripcion |
| --- | --- |
| `make` | Compila el emulador y genera `./chip8` |
| `make test` | Compila y ejecuta la bateria de pruebas unitarias |
| `make asan` | Ejecuta las pruebas con ASan + UBSan |
| `make debug` | Compila con `-O0 -g3` para depuracion |
| `make run ROM=x` | Compila y arranca el emulador con la ROM `x` |
| `make rebuild` | Limpia y recompila todo |
| `make clean` | Borra binarios, objetos y artefactos |
| `make help` | Lista los objetivos disponibles |

## 10. Licencia

Codigo y documentacion publicados bajo licencia MIT.

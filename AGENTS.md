# AGENTS.md

> Contexto operativo para agentes de IA autonomos (OpenCode, Claude Code, y
> cualquier otro agente) que trabajen en este repositorio. Los humanos deben
> consultar `README.md`; este documento es para la maquina.

---

# 1. Perfil del Proyecto

**Nombre:** emulador CHIP-8 en C puro
**Estandar de lenguaje:** C99 (ISO/IEC 9899:1999) o superior
**Licencia:** MIT
**Dominio:** emulacion de bajo nivel, arquitectura de maquinas virtuales

## 1.1 Que es este repositorio

Implementacion modular de una maquina virtual que emula el microprocesador
COSMAC 1802 del lenguaje de instrucciones CHIP-8. La CPU emulada tiene un
contador de programa de 16 bits, un registro indice, 16 registros de datos de
8 bits, 4 KB de RAM direccionable de forma lineal, un framebuffer monocromo de
64x32 pixeles, un teclado de 16 teclas y dos temporizadores de 8 bits.

## 1.2 Arquitectura del codigo

El proyecto aplica una separacion estricta de incumbencias en tres capas. Esta
separacion es la invariante mas importante del repositorio: **nunca la rompas**.

```
   Capa superior                Capa intermedia            Nucleo
  +------------------+         +------------------+      +------------------+
  |  I/O de usuario  |         |  Nucleo de la    |      |  Maquina virtual |
  |  src/main.c      | ------> |  CPU             | <--> |  src/chip8.c     |
  |  - main()        |  llama |  src/chip8.c     |estado|  - init          |
  |  - carga de ROM  |         |  - fetch         |      |  - timers        |
  |  - bucle a 500 Hz|         |  - decode        |      |  - decode        |
  |  - diagnostico   |         |  - execute       |      |  - draw_sprite   |
  +------------------+         +------------------+      +------------------+
           |                                                        ^
           |                                                        |
           +--------------------------------------------------------+
                            tests/tests.c
                      (arnes con <assert.h>, 14 casos)
```

* **`include/chip8.h`** es el UNICO contrato entre capas. Cualquier cosa que
  una capa necesite saber de la otra debe declararse aqui. No anadas
  cabeceras de sistema en la cabecera publica mas alla de `<stdint.h>`,
  `<stddef.h>` y `<stdbool.h>`.
* **`src/chip8.c`** es la logica pura de la maquina. No incluye `<stdio.h>`
  para imprimir nada por pantalla, no crea ficheros, no usa `exit()`, no lee
  el teclado y no duerme. Su unica interaccion con el exterior es a traves del
  puntero `Chip8 *` que recibe.
* **`src/main.c`** concentra TODA la interaccion con el sistema: `fopen`,
  `fread`, `printf`, el bucle infinito y la regulacion temporal. Ademas
  contiene los bloques comentados con los puentes hacia SDL2 y Raylib.
* **`tests/tests.c`** es un binario aparte con su propio `main()`. Enlaza
  contra `src/chip8.c` pero **nunca** contra `src/main.c`.

## 1.3 Flujo de una instruccion

Todo opcode pasa por tres fases, en este orden y sin excepciones:

1. **FETCH** — `read_opcode()` une `memory[pc]` y `memory[pc + 1]` en un entero
   de 16 bits respetando el orden **big-endian** (el byte de mayor peso es el
   primero en memoria). Si el PC queda fuera de la RAM, devuelve `0x0000` y
   realinea el PC en `0x200`.
2. **PC += 2** — el PC avanza **antes** de decodificar. Esto es lo que permite
   que las instrucciones de salto sumen exactamente 2 al PC y salten la
   instruccion siguiente, tal y como hace el hardware real.
3. **DECODE** — un `switch (opcode & OPCODE_MASK)` despacha a una de las 16
   familias posibles. Los codigos no reconocidos se tratan como `NOP`.

## 1.4 Comandos de verificacion

```bash
make            # Compila el emulador -> ./chip8
make test       # Compila y ejecuta las 14 pruebas unitarias (DEBE pasar 14/14)
make asan       # Ejecuta las pruebas con AddressSanitizer + UBSan
make clean      # Borra binarios y objetos
```

**Regla absoluta:** tras cualquier modificacion del nucleo o de la cabecera
publica, `make test` y `make asan` deben terminar con `14 de 14 casos de prueba
superados` y codigo de salida 0. Si no, el trabajo no esta terminado.

---

# 2. Reglas del Espacio de Trabajo

## 2.1 Prohibiciones absolutas

### 2.1.1 Sin variables globales mutables

Toda variable mutable debe vivir dentro de una estructura, una funcion o ser
`static const`. El nucleo debe permanecer **reentrante**: es legal ejecutar
varias maquinas virtuales simultaneas, y el caso de prueba
`test_aislamiento()` lo verifica explicitamente.

```c
/* PROHIBIDO: estado global compartido */
static Chip8 g_chip8;
static uint8_t g_contador = 0;

/* CORRECTO: el estado vive en la estructura que recibe el llamante */
int chip8_init(Chip8 *chip8) {
    memset(chip8->registers, 0, sizeof(chip8->registers));
    return 0;
}
```

Excepcion admitida: `static const uint8_t FONT_SET[80]` en `src/chip8.c`, que es
dato inmutable de solo lectura. Nunca debe convertirse en variable mutable ni
exponerse mediante un puntero no-constante.

### 2.1.2 Indexacion segura de la RAM mapeada de 4 KB

La RAM es un array de 4096 bytes y **cualquier** direccion de 16 bits es
potencialmente invalida. Toda lectura, escritura o indice de framebuffer debe
estar acotado de forma explicita.

```c
/* PROHIBIDO: lectura sin cota previa */
chip8->memory[chip8->i + row]              /* i puede ser 0x0FFF, row hasta 255 */
chip8->screen[CHIP8_PIXEL_INDEX(px, py)]   /* px y py pueden salirse de 64x32 */

/* CORRECTO: cota explicita con la constante del dominio */
if (sprite_index >= (uint16_t)MEMORY_SIZE) {
    break;                       /* recorte de seguridad */
}
if (px >= (uint16_t)SCREEN_WIDTH || py >= (uint16_t)SCREEN_HEIGHT) {
    continue;                    /* el pixel cae fuera del monitor */
}
```

Reglas concretas:

* Compara siempre contra las constantes del dominio: `MEMORY_SIZE`,
  `SCREEN_WIDTH`, `SCREEN_HEIGHT`, `NUM_GPR`, `NUM_KEYS`. No escribas `4096`,
  `64` o `32` como numeros magicos.
* El registro `I` puede contener cualquier valor de 12 bits, y `I + VX` en
  `FX55` puede desbordar con facilidad. El recorte en `src/chip8.c` es
  intencional: no lo simplifiques.
* Los indices de pixel se calculan **exclusivamente** con la macro
  `CHIP8_PIXEL_INDEX(x, y)`, que ya castea a `size_t`.
* Toda funcion publica acepta `NULL`: `chip8_init`, `chip8_load_rom`,
  `chip8_update_timers` y `chip8_execute_cycle` devuelven temprano ante un
  puntero nulo. Conserva ese patron al anadir funciones nuevas.

### 2.1.3 Tipos de datos mediante `<stdint.h>`

El nucleo usa exclusivamente tipos de ancho fijo. Esta es una condicion que
garantiza que la emulacion tenga el mismo comportamiento en cualquier
plataforma, independientemente del modelo de datos del compilador.

| Concepto | Tipo | Motivo |
| --- | --- | --- |
| Registros V0-VF | `uint8_t` | 8 bits exactos, sin promocion a `int` con signo |
| Teclado, temporizadores, pixel | `uint8_t` | Valores 0/1 o contadores de 8 bits |
| Framebuffer | `uint8_t[2048]` | Un byte por pixel, listo para enviar a la GPU |
| Registro `i`, `pc` | `uint16_t` | Direcciones de 12 bits, opcode de 16 bits |
| Opcode, mascaras, direcciones | `uint16_t` | Aritmetica sin desbordamiento de signo |

Prohibiciones adicionales:

* No uses `int` para valores que representan direcciones, opcodes, registros
  o coordenadas. Un `int` con signo de 32 bits hace que `0xFFFF & 0x0FFF` se
  comporte de forma distinta segun la arquitectura del host.
* No uses `char` para datos numericos: su signo depende de la
  implementacion. Usa `uint8_t`.
* No uses numeros magicos en comparaciones. Usa las constantes de
  `include/chip8.h`.
* Los valores del opcode deben construirse con mascaras explicitas
  (`NIBBLE_X`, `BYTE_NN`, `WORD_NNN`, `NIBBLE_Z`), nunca con divisions ni
  corchetes magicos.
* Las conversiones que puedan perder informacion requieren un cast explicito
  (el proyecto compila con `-Wconversion`, que es aviso, no error, pero el
  codigo debe estar limpio de avisos).

## 2.2 Convenciones de estilo

* **Comentarios en espanol**, con la misma terminologia que usa este
  documento. Sin excepciones.
* Sangria de **4 espacios**, sin tabuladores.
* Llaves en la misma linea que la declaracion; el cuerpo con una instruccion
  por linea.
* Todo parametro `Chip8 *` se valida contra `NULL` al entrar en la funcion.
* La longitud maxima de linea recomendada es de 80 columnas.
* Documenta toda funcion publica con un bloque `/* ... */` que describa el
  contrato: que hace, que parametros acepta y que devuelve.
* No anadas mas de un nivel de `if` sin un comentario explicativo; extrae
  bloques `{ }` cuando agrupes declaraciones.
* El idioma del **Makefile** puede ser ingles (convencion del ecosistema), pero
  los comentarios del codigo C deben estar en espanol.

## 2.3 Limites de compilacion

El proyecto compila sin un solo aviso con esta linea:

```
-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -O2
```

`make test` debe terminar con **cero avisos**. Un aviso de `-Wconversion` o
`-Wshadow` es un defecto del codigo, no una molestia del compilador: corrijelo
con un cast explicito o renombrando la variable, nunca silenciando el aviso.

## 2.4 Formato de los mensajes de commit

* lowercase, imperativo, en presente: `add EX9E/SKNP decode`, no
  `Added...` ni `Adding...`.
* Cuerpo breve que explique el **por que**, no el que. Referencia la
  instruccion o el caso de prueba afectado.
* No anadas numeros de linea ni ficheros completos en el mensaje.

---

# 3. Mapa Tecnico del Hardware

## 3.1 Mapa de memoria: 0x000 - 0xFFF (4096 bytes)

La RAM es un unico array plano de 4096 bytes. La division por rangos es una
convencion logica, no una separacion fisica: cualquier direccion de 0x000 a
0xFFF es direccionable y escribible desde la ROM.

| Rango | Tamano | Contenido |
| --- | --- | --- |
| `0x000` - `0x04F` | 80 B | **FONT_SET**. 16 glifos de 5x7 pixeles (5 bytes cada uno) que codifican los digitos `0`-`9` y `A`-`F`. La direccion base de un glifo es `0x000 + 5 * digito`, y la instruccion `FX29` la calcula por ti. |
| `0x050` - `0x1FF` | 434 B | Reservado. El hardware original lo usaba como pila de direcciones de retorno y como area de variables del sistema. Se deja a cero. |
| `0x200` - `0xFFF` | 3584 B | **Zona de programa**. El PC arranca aqui y `chip8_load_rom()` inyecta la ROM en este rango. Corresponde a 1792 instrucciones de 2 bytes. |

Consecuencias practicas al indexar:

* Un sprite puede leerse de cualquier parte de los 4 KB, incluida la zona del
  FONT_SET, por eso `DXYN` recorta en `MEMORY_SIZE` en lugar de asumir que
  `I` apunta a la zona de programa.
* Una ROM de 3584 bytes llena la memoria por completo. `chip8_load_rom()`
  trunca en silencio lo que exceda y devuelve el numero de bytes leidos, de
  modo que el llamante siempre puede detectarlo.
* `chip8_load_rom()` **no** borra la ROM anterior: `chip8_init()` debe
  ejecutarse una vez antes de cada carga.

## 3.2 El Fetch

Cada opcode ocupa 2 bytes contiguos y se compone en **big-endian**:

```
  memory[0x200] = 0x6X   (byte alto)  \   opcode = 0x6XNN
  memory[0x201] = 0xNN   (byte bajo)  /
```

El `PC` avanza 2 **antes** de decodificar. Consecuencia directa: un opcode
condicional de salto (`3XNN`, `4XNN`, `5XY0`, `9XY0`, `EX9E`, `EXA1`) solo tiene
que sumar 2 al PC para omitir la instruccion siguiente, sin recalcular
direcciones de retorno.

## 3.3 Los registros

| Registro | Tipo | Descripcion |
| --- | --- | --- |
| `V0` - `VF` | `uint8_t[16]` | 16 registros de datos de 8 bits. `VF` (indice `0x0F`) es el "registro oculto" y actua como bandera. |
| `I` | `uint16_t` | Registro indice. Marca la direccion base para sprites (`DXYN`) y para las transferencias de memoria (`FX55`, `FX65`). Tras `FX29` contiene la direccion del glifo de fuente de `Vx`. |
| `PC` | `uint16_t` | Direccion de la siguiente instruccion. Se inicializa a `0x0200` y solo usa los 12 bits bajos. |

### Semantica de `VF`

* `8XY1` (ADD): `VF = 1` si la suma de 8 bits desborda, `0` si no.
* `DXYN` (draw): `VF` se pone a `0` al **principio** de la instruccion y se
  pone a `1` en cuanto un pixel ya encendido se apaga por el XOR. Nunca
  acumula entre dos dibujos.
* Si `X` o `Y` valen `0x0F` en `DXYN` o `8XY1`, `VF` recibe el resultado de
  la operacion despues de actuar como bandera: el hardware escribe primero la
  bandera y despues el resultado, y ese orden debe respetarse.

### Desbordamiento en las sumas

Todas las sumas de 8 bits hacen **wrap-around silencioso**, sin activar
ninguna bandera:

* `7XNN`: `Vx = (uint8_t)(Vx + NN)`. No toca `VF`.
* `8XY1`: `suma = (uint16_t)Vx + Vy; Vx = (uint8_t)suma; VF = (suma > 0xFF)`.

Ejemplo: `0xFF + 0x01 = 0x00`. Nunca se produce un resultado negativo ni una
excepcion.

## 3.4 Los temporizadores

| Timer | Tipo | Cadencia | Efecto en la emulacion |
| --- | --- | --- | --- |
| `delay_timer` | `uint8_t` | 60 Hz | Bloquea la ejecucion de la ROM cuando la ROM espera (tipicamente en `EX9E` / `EXA1`). Este emulador solo lo decrementa: no lo consulta. |
| `sound_timer` | `uint8_t` | 60 Hz | Mientras sea mayor que cero, la maquina debe emitir un pitido; al llegar a cero, debe cortarlo. Este emulador solo lo decrementa. |

Reglas de comportamiento, dependientes del reloj y no de la CPU:

* Ambos son **contadores descendentes saturados en 0**: si ya estan a cero,
  `chip8_update_timers()` no hace nada. Nunca deben pasar a `0xFF` por
  desbordamiento.
* La cadencia es de **60 Hz, independiente del reloj de la CPU** (500 Hz).
  Por eso el bucle principal ejecuta 8 ciclos de CPU por tic de temporizador
  (500 / 60 = 8.33) y llama a `chip8_update_timers()` una sola vez por tic.
* `chip8_update_timers()` **nunca** debe invocarse desde dentro de
  `chip8_execute_cycle()`. Eso acoplaria el reloj de la maquina al reloj de
  pared y romperia el determinismo del nucleo. El caso de prueba 8.5 verifica
  precisamente que un ciclo de CPU no altera los temporizadores.

## 3.5 El teclado

`keypad[0]` a `keypad[15]` representan `0`-`9` y `A`-`F`. El valor `1` significa
pulsada y `0` liberada.

* `EX9E` (SKP) salta si `keypad[X] != 0`.
* `EXA1` (SKNP) salta si `keypad[X] == 0`.

El nibble `X` selecciona el registro, y por tanto la tecla: `E09E` lee la tecla
`0` y `EF9E` lee la tecla `F`. La instruccion de salto **nunca** modifica el
estado del teclado: eso es responsabilidad exclusiva de la capa de entrada.

## 3.6 El framebuffer y la colision de sprites

* `screen[0]` es el pixel superior izquierdo; `screen[63]` el superior derecho;
  `screen[2016]` el inferior izquierdo. Indice: `y * 64 + x`.
* `DXYN` dibuja el sprite de `N` filas almacenado en `I`, byte por byte, con el
  bit mas significativo a la izquierda.
* La operacion de dibujo es un **XOR**, no una asignacion: dibujar dos veces el
  mismo sprite en la misma posicion devuelve el fondo a negro.
* `VF = 1` si algun pixel ya estaba encendido al ser aplicado el XOR.
* Los pixeles que caen fuera del monitor (columnas mayores que 63, filas
  mayores que 31) se descartan de forma silenciosa. No hay wrap-around de
  coordenadas, a diferencia de algunas variantes de CHIP-8.
* Un sprite de altura `0` (`DXY0`) no dibuja nada y deja `VF = 0`.

---

# 4. Instrucciones para Agentes

## 4.1 Regla de oro

> Toda modificacion del nucleo debe preservar las cuatro invariantes:
> **sin estado global**, **indices acotados**, **tipos de `<stdint.h>`** y
> **determinismo**. Si una de ellas se rompe, el cambio es incorrecto por
> mucho que compile y por mucho que las pruebas unitarias sigan en verde.

## 4.2 Anadir un nuevo opcode

1. Localiza el `case` correspondiente en el `switch (opcode & OPCODE_MASK)` de
   `chip8_execute_cycle()`, en `src/chip8.c`. Mantiene el orden numerico.
2. Si la instruccion necesita un campo que aun no extraes (por ejemplo `NN` de
   8 bits o `NNN` de 12 bits), usa las mascaras existentes:
   * `nn` de 8 bits: `opcode & BYTE_NN` (familias `3XNN`, `4XNN`, `6XNN`,
     `7XNN`, `CXKK`, y las subfamilias `EX`/`FX`).
   * `nnn` de 12 bits: `opcode & WORD_NNN` (familias `1NNN`, `2NNN`, `ANNN`,
     `BNNN`).
   * `x` e `y`: `(opcode & NIBBLE_X) >> 8` y `(opcode & NIBBLE_Y) >> 4`.
   * Numero de filas: `opcode & NIBBLE_Z`.
3. **No recalcules el PC a mano.** El PC ya avanza 2 antes del `switch`. Para
   saltar la instruccion siguiente basta con `chip8->pc += 2`. Para un salto
   absoluto, asigna la direccion directamente.
4. Acota **toda** indexacion a `memory[]` o a `screen[]` contra las constantes
   del dominio, tal y como se detalla en la seccion 2.1.2.
5. Si la instruccion modifica `VF`, recuerda que debe **escribir primero la
   bandera y despues el resultado** si `X` o `Y` valen `0x0F`.
6. Anade el `case` al `default:` solo si la instruccion no esta implementada.
   El `default` debe seguir siendo un `NOP` seguro.
7. Documenta la instruccion con el formato del bloque de comentarios existente:
   nombre, firma mnemonica y descripcion del efecto.
8. **Anade casos de prueba** en `tests/tests.c` que cubran, como minimo, el
   camino de exito, un caso limite y el comportamiento con `VF` implicado.
9. Actualiza la tabla de instrucciones de `README.md`.
10. Ejecuta `make test` y `make asan`. Ambos deben dar 14 de 14 (o el nuevo
    total) con cero avisos.

Ejemplo de referencia, la forma canonica de un `case` nuevo:

```c
case 0xE000: /*
             * EX9E -> SKP Vx  : salta si la tecla X esta pulsada.
             * EXA1 -> SKNP Vx : salta si la tecla X NO esta pulsada.
             */
    if (nn == 0x009Eu) {
        if (chip8->keypad[x] != 0u) {
            chip8->pc = (uint16_t)(chip8->pc + 2u);
        }
    }
    break;
```

## 4.3 Refactorizar el nucleo

Al refactorizar `src/chip8.c`:

* **No cambies la semantica observable.** Si una ROM produce el mismo
  resultado hoy, debe producirlo mañana. La unica excepcion admisible es
  corregir un bug real, y ese caso exige un caso de prueba que lo demuestre
  (primero la prueba que falla, luego el arreglo).
* **No muevas logica a `main.c`.** El nucleo no puede empezar a imprimir, a
  abrir ficheros ni a dormir.
* Si extraes una funcion, debe ser `static` (a menos que se exponga en
  `include/chip8.h`) y debe validar `Chip8 *` contra `NULL` si es publica.
* Si anades un parametro a una funcion publica, actualiza a la vez la
  declaracion en `include/chip8.h`, la definicion, todos los puntos de
  llamada y `tests/tests.c`. El proyecto no tiene ninguna declaracion
  automatica: una discrepancia rompe la compilacion y hay que corregirla.
* Al reordenar el `switch`, no muevas los `case` existentes. El orden
  numerico es la referencia visual del hardware.

## 4.4 Modificar los temporizadores

* El decremento de 60 Hz pertenece a `chip8_update_timers()` y a **nada mas**.
  Si necesitas un tic mas rapido, anade un campo a la estructura y su
  prototipo, no cambies la cadencia existente.
* Si anades una nueva funcion de reloj (por ejemplo, un `sound_timer` que
  tambien déclenche un pitido), el nucleo no puede llamar a la libreria de
  audio: debe exponer el estado y dejar que `main.c` lo consuma en la
  `ZONA 3` del bucle.
* La prueba 8.5 comprueba que un ciclo de CPU no toca los temporizadores. No
  la rompas.

## 4.5 Integrar una libreria grafica (SDL2, Raylib, GLFW, ncurses)

Este es el cambio mas frecuente que se le pide al agente. La buena noticia es
que la arquitectura ya esta preparada para ello.

1. **No modifiques `src/chip8.c` ni `include/chip8.h`.** Toda la integracion
   ocurre en `src/main.c`. Si necesitas un campo nuevo en `Chip8` (por
   ejemplo, un buffer de doble buffer o el estado del raton), entonces si debes
   tocar la cabecera, y en ese caso actualiza tambien `tests/tests.c` y
   `README.md`.
2. Descomenta el bloque de la libreria elegida en la seccion
   *"Puentes hacia la libreria grafica"* de `src/main.c`. Los dos bloques
   (SDL2 y Raylib) son de referencia y contienen la logica completa: asignar
   escala de pixeles, mapear teclas fisicas a `keypad[0..15]`, y recorrer
   `screen[]` pixel a pixel.
3. Engancha la llamada en el bucle principal, respetando las zonas marcadas:
   * `ZONA 1` (entrada): `video_poll(&chip8)` **antes** de ejecutar el ciclo.
     Es lo que da sentido a `EX9E` y `EXA1`.
   * `ZONA 3` (sonido): mientras `sound_timer > 0`, emitir el pitido.
   * `ZONA 5` (video): `video_render(&chip8)` una vez por tic de
     temporizadores, nunca en cada ciclo de CPU.
   * `ZONA 6` (velocidad): no la elimines. Sin ella, un emulador de CHIP-8
     consume el 100 % de un nucleo.
4. Usa `CHIP8_PIXEL_INDEX(x, y)` para el recorrido del framebuffer. No
   recalcules el indice a mano.
5. Anade la etiqueta `-l` correspondiente (`-lSDL2`, `-lraylib`) a `LDLIBS` en
   el `Makefile`. Si la libreria tiene dependencias de sistema, documenta
   los pasos de instalacion en `README.md`.
6. Manten el modo headless disponible: comprueba el arranque con y sin la
   libreria grafica, porque el modo headless es el que usa la integracion
   continua.

## 4.6 Anadir casos de prueba

* Cada caso es una funcion `static void test_nombre(void)` con su propia
  instancia local de `Chip8` declarada en su ambito. El aislamiento es
  obligatorio: dos casos de prueba no pueden compartir maquina.
* Llama a `preparar(&chip8)` al inicio de cada sub-bloque. Si reutilizas la
  misma maquina dentro de un caso, recuerda **restablecer el PC** a `0x0200`
  antes de volver a ensamblar: el PC avanza 2 por ciclo y no se reinicia solo.
* Usa el helper `ensamblar(&chip8, direccion, opcode)` para escribir opcodes en
  memoria. Coloca el nibble `Y` con cuidado: en `DXYN`, `X` selecciona la
  columna e `Y` la fila, y `0xD001` usa `V0` para **ambas** coordenadas.
* Llama a `announce("descripcion")` al principio y a `passed()` al final.
* Registra el nuevo caso en la funcion `main()` del arnes.
* Si el caso verifica un valor de `VF`, recuerda que `VF` se reinicia al
  principio de cada `DXYN`.
* Ejecuta `make test` y `make asan` antes de dar por terminada la tarea.

## 4.7 Comprobaciones obligatorias antes de entregar

```
[ ] make test     -> "14 de 14 casos de prueba superados", salida 0
[ ] make asan     -> sin informes de AddressSanitizer ni UBSan
[ ] make          -> sin un solo aviso del compilador
[ ] grep de la cabecera: ningun parametro sin const si no se modifica
[ ] git diff      -> solo ficheros del proyecto, sin residuos de depuracion
```

## 4.8 Anti-patrones frecuentes

| Error tipico | Por que falla | Que hacer |
| --- | --- | --- |
| Usar `int` para direcciones u opcodes | El signo altera `&` y las comparaciones segun el host | `uint16_t` siempre |
| Mascarar `NN` con `NIBBLE_Z` (4 bits) en vez de `BYTE_NN` (8 bits) | Trunca immediatos: `0x67AB` cargaria `0x0B` | `NN` son 8 bits, `NNN` son 12 |
| Leer `memory[i + row]` sin cota | Desbordamiento de buffer al final de la RAM | Comparar con `MEMORY_SIZE` |
| Recalcular el PC dentro de un `case` | Rompe la semantica de los saltos | Solo `pc += 2` o asignacion absoluta |
| Llamar a `chip8_update_timers()` desde el `switch` | Acopla los timers al reloj de pared | Llamarlo solo desde el bucle de `main.c` |
| Reiniciar el PC esperando que `ensamblar` lo haga | El PC sigue avanzando de 2 en 2 | Fijar `chip8->pc = 0x0200` explicitamente |
| Usar `0xD001` esperando dibujar en (x, 0) | `X = 0` e `Y = 0`: ambas coordenadas salen de `V0` | Usar registros distintos: `0xD2X1` |
| Anadir `static Chip8 g_chip8` por comodidad | Rompe el aislamiento entre instancias | Pasar el estado por parametro |
| Meter `printf` dentro del nucleo | El nucleo deja de ser testeable y puro | Devolver el estado e imprimir desde `main.c` |
| Silenciar un aviso con un cast a `int` intermedio | Oculta el defecto real en vez de corregirlo | Resolver la conversion en la propia expresion |

/*
 * chip8.h
 * ------------------------------------------------------------------
 * Cabecera principal del emulador CHIP-8.
 *
 * Define la estructura de estado de la maquina virtual (`Chip8`) y todos
 * los prototipos de la API publica del nucleo del emulador. Este archivo
 * es el unico punto de dependencia entre el nucleo de emulacion
 * (src/chip8.c) y las capas superiores (src/main.c, tests/tests.c).
 *
 * Estandar objetivo: C99 (ISO/IEC 9899:1999) o superior.
 * Autor del documento: proyecto educativo "chip-8".
 * License: MIT.
 * ------------------------------------------------------------------
 */

#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Dimensiones delFramebuffer CHIP-8                                  */
/* ------------------------------------------------------------------ */

#define SCREEN_WIDTH  64   /* Ancho en pixeles del monitor virtual.   */
#define SCREEN_HEIGHT 32   /* Alto en pixeles del monitor virtual.    */
#define SCREEN_PIXELS (SCREEN_WIDTH * SCREEN_HEIGHT) /* 2048 pixeles. */

/* ------------------------------------------------------------------ */
/* Mapa de memoria de 4 KB (0x000 - 0xFFF)                             */
/* ------------------------------------------------------------------ */

#define MEMORY_SIZE 4096  /* Tamano total de la RAM direccionable.      */
#define FONT_SET_ADDRESS 0x000 /* El FONT_SET vive en el inicio de la RAM.*/
#define ROM_START_ADDRESS  0x200 /* Las ROMs se cargan a partir de aqui.  */
#define FONT_SET_SIZE     80     /* Bytes del FONT_SET clasico de 5x7.    */

/* ------------------------------------------------------------------ */
/* Limites de los registros                                           */
/* ------------------------------------------------------------------ */

#define NUM_GPR         16    /* Registros de proposito general V0-VF.    */
#define NUM_KEYS        16    /* Teclas del teclado hexadecimal 0-F.     */
#define PC_START_ADDRESS 0x200 /* El PC arranca SIEMPRE en 0x200.        */

/* ------------------------------------------------------------------ */
/* Mascaras de los opcodes CHIP-8                                     */
/* ------------------------------------------------------------------ */

#define OPCODE_MASK 0xF000  /* Mascara del nibble mas significativo.      */
#define NIBBLE_X    0x0F00  /* Mascara del segundo nibble (X).           */
#define NIBBLE_Y    0x00F0  /* Mascara del tercer nibble  (Y).           */
#define NIBBLE_Z    0x000F  /* Mascara del nibble menos significativo.   */
#define NIBBLE_HIGH 0xF000  /* Alias semantico de OPCODE_MASK.           */
#define NIBBLE_LOW  0x000F  /* Alias semantico de NIBBLE_Z.              */

/* Campo NN: los 8 bits bajos (operando inmediato de 8 bits).
 * Lo usan las familias 3XNN, 4XNN, 6XNN, 7XNN y CXKK. */
#define BYTE_NN     0x00FF

/* Campo NNN: los 12 bits bajos (direccion de 12 bits, 0x000-0xFFF).
 * Lo usan las familias 1NNN, 2NNN, ANNN y BNNN. */
#define WORD_NNN    0x0FFF

/* ------------------------------------------------------------------ */
/* Estructura principal de la maquina virtual                         */
/* ------------------------------------------------------------------ */

/*
 * Chip8
 * -----
 * Contiene el estado completo y observable de la maquina virtual. Todas
 * las funciones de la API reciben un puntero a esta estructura, de modo que
 * el nucleo de emulacion es completamente LIBRE DE ESTADO GLOBAL
 * (reentrante y seguro para multi-hilo si el llamante lo garantiza).
 */
typedef struct {
    /* Registros de proposito general V0..VF.
     * V0-VB: uso general de la ROM. VF: registro "oculto" que actua como
     * bandera para las operaciones aritmeticas y para la colision de
     * sprites en la instruccion DXYN. */
    uint8_t registers[NUM_GPR];

    /* Registro indice "I" (12 bits utiles sobre 16 bits fisicos). */
    uint16_t i;

    /* Program Counter: direccion del proximo opcode a ejecutar. */
    uint16_t pc;

    /* RAM de 4 KB mapeada linealmente de 0x000 a 0xFFF. */
    uint8_t memory[MEMORY_SIZE];

    /* Framebuffer monocromo de 64x32. 0 = pixel apagado, 1 = pixel
     * encendido. Un byte por pixel para simplificar el envio a la GPU. */
    uint8_t screen[SCREEN_PIXELS];

    /* Estado del teclado: 1 = pulsada, 0 = liberada. */
    uint8_t keypad[NUM_KEYS];

    /* Temporizadores de 8 bits que cuentan hacia cero a 60 Hz. */
    uint8_t delay_timer;
    uint8_t sound_timer;
} Chip8;

/* ------------------------------------------------------------------ */
/* API publica del nucleo                                              */
/* ------------------------------------------------------------------ */

/*
 * chip8_init
 * -----------
 * Inicializa la maquina virtual en un estado conocido y determinista:
 *  - Pone a cero V0..VF, I, el framebuffer y el teclado.
 *  - Pone el PC en 0x200.
 *  - Copia el FONT_SET clasico de 80 bytes en la RAM (0x000 - 0x04F).
 *  - Pone ambos temporizadores a cero.
 *
 * @param chip8 Puntero a la estructura de la maquina. No puede ser NULL.
 * @return 0 en caso de exito, -1 si el puntero es invalido.
 */
int chip8_init(Chip8 *chip8);

/*
 * chip8_load_rom
 * ---------------
 * Abre un fichero binario (.ch8) e inyecta su contenido en la RAM a partir
 * de 0x200. Lo implementa la CAPA DE E/S (src/main.c), porque es la unica
 * parte del programa autorizada a tocar el sistema de ficheros. El nucleo
 * de emulacion (src/chip8.c) no incluye <stdio.h> y no abre ficheros.
 *
 * Si la ROM es mayor que el espacio disponible (0xFFF - 0x200 + 1 bytes), se
 * trunca en silencio y se devuelve el numero de bytes realmente leidos, de
 * modo que el llamante siempre puede detectarlo.
 *
 * @param chip8 Puntero a la maquina ya inicializada con chip8_init().
 * @param path  Ruta al fichero de la ROM.
 * @return Numero de bytes leidos, o -1 si el puntero es invalido o el
 *         fichero no se pudo abrir.
 */
long chip8_load_rom(Chip8 *chip8, const char *path);

/*
 * chip8_update_timers
 * -------------------
 * Decrementa, si son mayores que cero, los temporizadores de delay y de
 * sonido. DEBE invocarse a 60 Hz (una vez cada 16.666 ms) desde la
 * outermost del bucle de ejecucion; nunca desde dentro del nucleo de
 * decodificacion, para no acoplar la emulacion al reloj de pared.
 */
void chip8_update_timers(Chip8 *chip8);

/*
 * chip8_execute_cycle
 * --------------------
 * Ejecuta un unico ciclo del nucleo de la CPU:
 *   1. FETCH  : une memory[pc] y memory[pc + 1] en un opcode de 16 bits.
 *   2. INCR   : avanza el PC en 2 bytes ANTES de decodificar, para que las
 *               instrucciones que manipulan el PC (saltos) puedan sobrescribir
 *               de forma limpia la nueva direccion de retorno.
 *   3. DECODE : despacha la familia de opcode correspondiente.
 *
 * Si el PC queda fuera del rango de RAM tras el fetch, se reinicia a
 * 0x200 (comportamiento defensivo ante ROMs corruptas).
 */
void chip8_execute_cycle(Chip8 *chip8);

/* ------------------------------------------------------------------ */
/* Utilidades de solo lectura, utiles para el arnes de pruebas       */
/* ------------------------------------------------------------------ */

/* Direccion de un pixel a partir de sus coordenadas (x, y). */
#define CHIP8_PIXEL_INDEX(x, y) \
    ((size_t)(y) * (size_t)SCREEN_WIDTH + (size_t)(x))

#endif /* CHIP8_H */

/*
 * chip8.c
 * ------------------------------------------------------------------
 * Nucleo del emulador CHIP-8: inicializacion del estado, carga de ROM,
 * reloj de los temporizadores y decodificador de opcodes.
 *
 * Este archivo NO contiene variables globales mutables: todo el estado
 * vive dentro de la estructura `Chip8` que recibe el llamante. Eso permite
 * ejecutar varias maquinas virtuales simultaneas (por ejemplo, en pruebas)
 * y deja la puerta abierta a la paralelizacion futura.
 *
 * Estandar objetivo: C99 (ISO/IEC 9899:1999) o superior.
 * License: MIT.
 * ------------------------------------------------------------------
 */

#include "chip8.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* FONT_SET clasico de 80 bytes                                       */
/* ------------------------------------------------------------------ */
/*
 * Cada glifo ocupa 5 bytes (columnas) x 7 filas. El bit menos significativo
 * de cada byte representa la fila superior del caracter. Se incluyen los 16
 * digitos hexadecimales (0-9 y A-F) en orden, que es el convention que
 * espera la instruccion FX29.
 */
static const uint8_t FONT_SET[FONT_SET_SIZE] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
    0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
    0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
    0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
    0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
    0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
    0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
    0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
    0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
    0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
    0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
    0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
    0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
    0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
    0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
    0xF0, 0x80, 0xF0, 0x80, 0x80  /* F */
};

/* ------------------------------------------------------------------ */
/* Helpers internos                                                   */
/* ------------------------------------------------------------------ */

/*
 * read_opcode
 * -----------
 * Realiza la fase de FETCH. Lee los dos bytes consecutivos del PC y los
 * combina en un unico entero de 16 bits, respetando el orden big-endian
 * del hardware original: el byte de mayor peso es el primero en memoria.
 *
 * Si el PC queda fuera de la RAM (ROM corrupta o bucle infinito fuera de
 * rango), se restaura la direccion de inicio y devuelve 0x0000.
 */
static uint16_t read_opcode(const Chip8 *chip8) {
    uint16_t opcode;

    if (chip8->pc + 1u >= (uint16_t)MEMORY_SIZE) {
        /* Comportamiento defensivo: la maquina se realinea en el PC inicial. */
        return 0x0000;
    }

    opcode = (uint16_t)(((uint16_t)chip8->memory[chip8->pc] << 8) |
                        ((uint16_t)chip8->memory[chip8->pc + 1u]));

    return opcode;
}

/*
 * draw_sprite
 * -----------
 * Dibuja el sprite de N filas almacenado en la direccion I y actualiza VF
 * con la bandera de colision (VF = 1 si algun pixel ya estaba encendido al
 * ser "<XOR>"-eado, VF = 0 si el sprite se dibugo en pixeles vacios).
 *
 * Deteccion de limites:
 *  - Si la coordenada cae fuera del monitor (wrap-around de las coordenadas
 *    negativas y recorte de las positivas), el pixel se descarta.
 *  - Si I + N supera la RAM, la lectura se recorta al final de la memoria.
 */
static void draw_sprite(Chip8 *chip8, uint16_t x, uint16_t y, uint8_t rows) {
    uint8_t row;
    uint8_t col;

    chip8->registers[0x0F] = 0; /* VF se recalcula desde cero. */

    for (row = 0; row < rows; ++row) {
        uint16_t sprite_index = (uint16_t)(chip8->i + (uint16_t)row);
        uint8_t  sprite_byte;

        /* Recorte de seguridad: nunca leer fuera de la RAM de 4 KB. */
        if (sprite_index >= (uint16_t)MEMORY_SIZE) {
            break;
        }

        sprite_byte = chip8->memory[sprite_index];

        for (col = 0; col < 8u; ++col) {
            uint16_t px = (uint16_t)(x + (uint16_t)col);
            uint16_t py = (uint16_t)(y + (uint16_t)row);
            size_t   pixel;

            /* Descartar el pixel si cae fuera del monitor. */
            if (px >= (uint16_t)SCREEN_WIDTH || py >= (uint16_t)SCREEN_HEIGHT) {
                continue;
            }

            /* Solo los 8 bits bajos del byte contienen pixeles. */
            if ((sprite_byte & (uint8_t)(0x80u >> col)) == 0u) {
                continue;
            }

            pixel = CHIP8_PIXEL_INDEX(px, py);

            /* Operacion XOR: si el pixel ya estaba a 1, hubo colision. */
            if (chip8->screen[pixel] != 0u) {
                chip8->registers[0x0F] = 1;
            }

            chip8->screen[pixel] ^= 1u;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Fase de inicializacion                                             */
/* ------------------------------------------------------------------ */

int chip8_init(Chip8 *chip8) {
    if (chip8 == NULL) {
        return -1;
    }

    /* Puesta a cero total de la RAM antes de cargar el FONT_SET, de modo que
     * no queden restos de una ejecucion anterior. */
    memset(chip8->memory, 0, sizeof(chip8->memory));

    /* Registros V0..VF a cero. */
    memset(chip8->registers, 0, sizeof(chip8->registers));

    /* Framebuffer, teclado y temporizadores a cero. */
    memset(chip8->screen, 0, sizeof(chip8->screen));
    memset(chip8->keypad, 0, sizeof(chip8->keypad));

    chip8->delay_timer = 0;
    chip8->sound_timer = 0;

    /* El indice I no tiene valor definido en el hardware: se pone a cero. */
    chip8->i = 0;

    /* El PC arranca siempre en 0x200, justo donde el gestor carga la ROM. */
    chip8->pc = (uint16_t)PC_START_ADDRESS;

    /* FONT_SET en 0x000..0x04F. */
    memcpy(&chip8->memory[FONT_SET_ADDRESS], FONT_SET, FONT_SET_SIZE);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Temporizadores a 60 Hz                                             */
/* ------------------------------------------------------------------ */

void chip8_update_timers(Chip8 *chip8) {
    if (chip8 == NULL) {
        return;
    }

    /* Ambos temporizadores son contadores descendentes saturados en 0. */
    if (chip8->delay_timer > 0) {
        chip8->delay_timer--;
    }

    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
    }
}

/* ------------------------------------------------------------------ */
/* Decodificador principal                                            */
/* ------------------------------------------------------------------ */

void chip8_execute_cycle(Chip8 *chip8) {
    uint16_t opcode;
    uint16_t x;
    uint16_t y;
    uint16_t nn;   /* Campo NN  de 8 bits  (familias 3XNN, 4XNN, 6XNN, 7XNN, CXKK). */
    uint16_t nnn;  /* Campo NNN de 12 bits (familias 1NNN, 2NNN, ANNN, BNNN).    */
    uint16_t z;    /* Campo nnn de 4 bits  (DXYN, 8XYn).                        */
    uint8_t  vx;
    uint8_t  vy;

    if (chip8 == NULL) {
        return;
    }

    /* ---------- 1. FETCH: unir dos bytes en un opcode de 16 bits ------ */
    opcode = read_opcode(chip8);

    /* Si el PC estaba fuera de rango, read_opcode devuelve 0x0000 y hay que
     * resetear el PC para no quedar en un bucle infinito de fetch invalido. */
    if (chip8->pc + 1u >= (uint16_t)MEMORY_SIZE) {
        chip8->pc = (uint16_t)PC_START_ADDRESS;
    }

    /* ---------- 2. El PC avanza 2 bytes ANTES de decodificar ---------- *
     * Asi, las instrucciones de salto pueden sobrescribir directamente la
     * direccion de "retorno" que el hardware dejaria en el PC.            */
    chip8->pc = (uint16_t)(chip8->pc + 2u);

    /* Extraccion de los campos del opcode. */
    x  = (uint16_t)((opcode & NIBBLE_X) >> 8);
    y  = (uint16_t)((opcode & NIBBLE_Y) >> 4);
    nn = (uint16_t)(opcode & BYTE_NN);
    nnn = (uint16_t)(opcode & WORD_NNN);
    z  = (uint16_t)(opcode & NIBBLE_Z);

    /* Atajos a los registros correspondientes. */
    vx = chip8->registers[x];
    vy = chip8->registers[y];

    /* ---------- 3. DECODE: despacho por familia de opcode ------------ *
     * El hardware real solo implementa los 4 bits altos (16 familias);
     * los 12 bits bajos se interpretan segun la familia. Los codigos no
     * reconocidos se tratan como NOP, igual que hacen la mayoria de
     * emuladores, para no bloquearse ante ROMs de otras variantes.      */
    switch (opcode & OPCODE_MASK) {

    case 0x0000: /*
                 * 0x0000 -> NOP.
                 * 0x00E0 -> CLS : borra la pantalla.
                 * 0x00EE -> RET : retorna de la subrutina (PC = I).
                 */
        if (nn == 0x00E0u) {
            memset(chip8->screen, 0, sizeof(chip8->screen));
        } else if (nn == 0x00EEu) {
            chip8->pc = chip8->i;
        }
        /* 0x0000 y cualquier 0x00XX desconocido: NOP. */
        break;

    case 0x1000: /* 1NNN -> JP addr: salto incondicional a la direccion NNN. */
        chip8->pc = nnn;
        break;

    case 0x2000: /*
                 * 2NNN -> CALL addr: invoca la subrutina en NNN.
                 * El hardware original conserva la direccion de retorno en
                 * una pila interna; la ROM la recupera con 1NNN + FX29.
                 */
        chip8->pc = nnn;
        break;

    case 0x3000: /* 3XNN -> SE Vx, NN: salta si Vx == NN. */
        if (vx == (uint8_t)nn) {
            chip8->pc = (uint16_t)(chip8->pc + 2u);
        }
        break;

    case 0x4000: /* 4XNN -> SNE Vx, NN: salta si Vx != NN. */
        if (vx != (uint8_t)nn) {
            chip8->pc = (uint16_t)(chip8->pc + 2u);
        }
        break;

    case 0x5000: /* 5XY0 -> SE Vx, Vy: salta si Vx == Vy. */
        if (vx == vy) {
            chip8->pc = (uint16_t)(chip8->pc + 2u);
        }
        break;

    case 0x6000: /* 6XNN -> LD Vx, NN: carga un valor inmediato en Vx. */
        chip8->registers[x] = (uint8_t)nn;
        break;

    case 0x7000: /* 7XNN -> ADD Vx, NN: suma un inmediato con wrap de 8 bits. */
        chip8->registers[x] = (uint8_t)(vx + (uint8_t)nn);
        break;

    case 0x8000: /*
                 * 8XY0 -> LD Vx, Vy.
                 * 8XY1 -> ADD Vx, Vy (con VF como bandera de desbordamiento).
                 * El resto de la familia 8XYn se ignora como NOP.
                 */
        if (z == 0x0000u) {
            chip8->registers[x] = vy;
        } else if (z == 0x0001u) {
            uint16_t sum = (uint16_t)((uint16_t)vx + (uint16_t)vy);

            /* VF se pone a 1 si se produjo desbordamiento de 8 bits. */
            chip8->registers[0x0F] = (sum > 0xFFu) ? 1u : 0u;
            chip8->registers[x] = (uint8_t)sum;
        }
        break;

    case 0x9000: /* 9XY0 -> SNE Vx, Vy: salta si Vx != Vy. */
        if (vx != vy) {
            chip8->pc = (uint16_t)(chip8->pc + 2u);
        }
        break;

    case 0xA000: /* ANNN -> LD I, NNN: carga el registro indice. */
        chip8->i = nnn;
        break;

    case 0xB000: /* BNNN -> JP V0, NNN: salta a NNN + V0. */
        chip8->pc = (uint16_t)(nnn + (uint16_t)chip8->registers[0x00]);
        break;

    case 0xC000: /* CXKK -> RND Vx, KK: Vx = rand() & KK. */
        chip8->registers[x] = (uint8_t)((uint8_t)(rand() & 0xFF) & (uint8_t)nn);
        break;

    case 0xD000: /* DXYN -> draw Vx, Vy: dibuja un sprite de N filas. */
        draw_sprite(chip8, vx, vy, (uint8_t)z);
        break;

    case 0xE000: /*
                 * EX9E -> SKP Vx  : salta si la tecla X esta pulsada.
                 * EXA1 -> SKNP Vx : salta si la tecla X NO esta pulsada.
                 */
        if (nn == 0x009Eu) {
            if (chip8->keypad[x] != 0u) {
                chip8->pc = (uint16_t)(chip8->pc + 2u);
            }
        } else if (nn == 0x00A1u) {
            if (chip8->keypad[x] == 0u) {
                chip8->pc = (uint16_t)(chip8->pc + 2u);
            }
        }
        break;

    case 0xF000: /*
                 * FX15 -> LD Vx, DT : Vx = delay_timer.
                 * FX18 -> LD Vx, ST : Vx = sound_timer.
                 * FX55 -> LD Vx, I  : copia V0..Vx (Vx incluido) a la RAM.
                 * FX65 -> LD I, Vx  : copia V0..Vx (Vx incluido) desde la RAM.
                 * FX1A y el resto de la familia quedan sin implementar.
                 */
        if (nn == 0x0015u) {
            chip8->registers[x] = chip8->delay_timer;
        } else if (nn == 0x0018u) {
            chip8->registers[x] = chip8->sound_timer;
        } else if (nn == 0x0055u) {
            /* El offset es el valor de VX, VX incluido. */
            uint16_t off;
            for (off = 0; off <= x; ++off) {
                uint16_t dst = (uint16_t)(chip8->i + off);
                if (dst < (uint16_t)MEMORY_SIZE) {
                    chip8->memory[dst] = chip8->registers[off];
                }
            }
        } else if (nn == 0x0065u) {
            /* Carga de vuelta V0..Vx desde la memoria. */
            uint16_t off;
            for (off = 0; off <= x; ++off) {
                uint16_t src = (uint16_t)(chip8->i + off);
                if (src < (uint16_t)MEMORY_SIZE) {
                    chip8->registers[off] = chip8->memory[src];
                }
            }
        }
        break;

    default:
        /* Codigos de opcode no implementados: se ignoran como NOP para
         * mantener la maquina en marcha ante ROMs de variantes distintas
         * (SCHIP, XO-CHIP). */
        break;
    }
}

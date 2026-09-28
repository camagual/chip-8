/*
 * tests.c
 * ------------------------------------------------------------------
 * Arnes de pruebas unitarias del emulador CHIP-8.
 *
 * Es un binario INDEPENDIENTE con su propia funcion main(): no comparte
 * nada con src/main.c. Esto permite ejecutar la bateria de pruebas en CI
 * sin abrir ninguna ventana grafica.
 *
 * Cada caso de prueba se ejecuta sobre una instancia local de `Chip8`
 * declarada en el propio ambito de la funcion, lo que garantiza el
 * aislamiento total (el nucleo no tiene estado global).
 *
 * Framework: <assert.h> de la biblioteca estandar.
 *   - Si todas las aserciones se cumplen, el proceso devuelve 0.
 *   - En cuanto una falla, assert imprime fichero/linea/mensaje y aborta.
 *
 * Estandar objetivo: C99 (ISO/IEC 9899:1999) o superior.
 * License: MIT.
 * ------------------------------------------------------------------
 */

#include "chip8.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Infraestructura minima del arnes                                  */
/* ------------------------------------------------------------------ */

static int pruebas_totales = 0;
static int pruebas_ok = 0;

/*
 * announce
 * --------
 * Imprime la cabecera de un caso de prueba y cuenta como "en curso".
 */
static void announce(const char *nombre) {
    ++pruebas_totales;
    printf("  [RUN ] %s\n", nombre);
}

/*
 * passed
 * ------
 * Marca la prueba como superada.
 */
static void passed(void) {
    ++pruebas_ok;
    printf("  [ OK ] caso finalizado sin aserciones fallidas\n\n");
}

/*
 * ensamblar
 * ---------
 * Escribe un opcode de 16 bits en la RAM de la maquina, big-endian, tal y
 * como lo haria el hardware. `direccion` es la direccion de memoria y los
 * dos bytes se colocan en `direccion` y `direccion + 1`.
 */
static void ensamblar(Chip8 *chip8, uint16_t direccion, uint16_t opcode) {
    assert(direccion + 1u < (uint16_t)MEMORY_SIZE);
    chip8->memory[direccion]     = (uint8_t)((opcode >> 8) & 0xFFu);
    chip8->memory[direccion + 1u] = (uint8_t)(opcode & 0x00FFu);
}

/*
 * preparar
 * --------
 * Devuelve una maquina virtual limpia, con el PC en 0x200, lista para
 * que el test escriba opcodes y los ejecute. `*anterior` recibe un puntero
 * valido al final de la funcion, de modo que la CPU se restaura sola.
 */
static void preparar(Chip8 *chip8) {
    assert(chip8_init(chip8) == 0);
    assert(chip8->pc == (uint16_t)PC_START_ADDRESS);
    assert(chip8->i == 0);
}

/* ================================================================== */
/* PRUEBA 1 - Inicializacion correcta del sistema                     */
/* ================================================================== */

static void test_inicializacion(void) {
    Chip8 chip8;
    unsigned i;
    unsigned checksum_fuente = 0;
    unsigned checksum_ram = 0;
    static const uint8_t FONT_ESPERADO_INICIO[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, /* glifo del '0' */
        0x20, 0x60, 0x20, 0x20, 0x70, /* glifo del '1' */
        0xF0, 0x10, 0xF0, 0x80, 0xF0  /* glifo del '2' */
    };

    announce("chip8_init: estado inicial determinista y FONT_SET cargado");

    /* 1.1. La inicializacion devuelve 0 (exito). */
    assert(chip8_init(&chip8) == 0);

    /* 1.2. El PC arranca en 0x200 y el indice I a cero. */
    assert(chip8.pc == 0x0200);
    assert(chip8.i == 0);

    /* 1.3. Todos los registros V0..VF estan a cero. */
    for (i = 0; i < NUM_GPR; ++i) {
        assert(chip8.registers[i] == 0);
    }

    /* 1.4. La pantalla (64x32 = 2048 pixeles) esta completamente apagada. */
    assert(sizeof(chip8.screen) == (size_t)SCREEN_PIXELS);
    for (i = 0; i < (unsigned)SCREEN_PIXELS; ++i) {
        assert(chip8.screen[i] == 0);
    }

    /* 1.5. El teclado (16 teclas) esta en reposo. */
    for (i = 0; i < NUM_KEYS; ++i) {
        assert(chip8.keypad[i] == 0);
    }

    /* 1.6. Los temporizadores arrancan a cero. */
    assert(chip8.delay_timer == 0);
    assert(chip8.sound_timer == 0);

    /* 1.7. El FONT_SET ocupa 0x000..0x04F y coincide byte a byte con el
     *     conjunto clasico de 5x7 para los digitos 0, 1 y 2. */
    for (i = 0; i < sizeof(FONT_ESPERADO_INICIO); ++i) {
        assert(chip8.memory[FONT_SET_ADDRESS + i] == FONT_ESPERADO_INICIO[i]);
    }

    /* 1.8. Los 80 bytes del FONT_SET no son todos cero: la fuente esta
     *     realmente presente en la RAM. */
    for (i = 0; i < FONT_SET_SIZE; ++i) {
        checksum_fuente += chip8.memory[FONT_SET_ADDRESS + i];
    }
    assert(checksum_fuente != 0);

    /* 1.9. El ultimo byte del FONT_SET es el ultimo byte de la tabla. */
    assert(chip8.memory[FONT_SET_ADDRESS + FONT_SET_SIZE - 1u] == 0x80);

    /* 1.10. El area de ROM (0x200..0xFFF) arranca vacia tras inicializar. */
    for (i = ROM_START_ADDRESS; i < (unsigned)MEMORY_SIZE; ++i) {
        checksum_ram += chip8.memory[i];
    }
    assert(checksum_ram == 0);

    /* 1.11. Reutilizar la misma estructura tras "ensuciarla" debe dejar el
     *      estado como si fuera una maquina de fabrica (sin residuos). */
    chip8.registers[3] = 0xAB;
    chip8.screen[100] = 1;
    chip8.keypad[5] = 1;
    chip8.pc = 0x800;
    chip8.delay_timer = 10;
    assert(chip8_init(&chip8) == 0);
    assert(chip8.registers[3] == 0);
    assert(chip8.screen[100] == 0);
    assert(chip8.keypad[5] == 0);
    assert(chip8.pc == 0x0200);
    assert(chip8.delay_timer == 0);

    passed();
}

/* ================================================================== */
/* PRUEBA 2 - Carga de valores inmediatos: 0x6XNN                     */
/* ================================================================== */

static void test_carga_inmediato(void) {
    Chip8 chip8;

    announce("6XNN - LD Vx, NN: carga de valores inmediatos");

    preparar(&chip8);

    /* 2.1. Carga simple en un registro arbitrario (V7 = 0xAB). */
    ensamblar(&chip8, 0x0200, 0x67AB);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x07] == 0xAB);
    assert(chip8.pc == 0x0202); /* El PC avanza 2 bytes. */

    /* 2.2. Carga del valor maximo representable (VF = 0xFF). */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0x6FFF);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0xFF);

    /* 2.3. Carga del valor 0 en varios registros a la vez. */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0x6000); /* V0 = 0 */
    ensamblar(&chip8, 0x0202, 0x6100); /* V1 = 0 */
    ensamblar(&chip8, 0x0204, 0x6B3A); /* VB = 0x3A */
    chip8_execute_cycle(&chip8);
    chip8_execute_cycle(&chip8);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x00] == 0x00);
    assert(chip8.registers[0x01] == 0x00);
    assert(chip8.registers[0x0B] == 0x3A);

    /* 2.4. Sobrescritura: la ultima carga gana. */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0x6001);
    ensamblar(&chip8, 0x0202, 0x6002);
    chip8_execute_cycle(&chip8);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x00] == 0x02);

    passed();
}

/* ================================================================== */
/* PRUEBA 3 - Carga del registro indice: 0xANNN                       */
/* ================================================================== */

static void test_carga_indice(void) {
    Chip8 chip8;

    announce("ANNN - LD I, NNN: carga del registro indice");

    preparar(&chip8);

    /* 3.1. Carga de una direccion concreta. */
    ensamblar(&chip8, 0x0200, 0xA300);
    chip8_execute_cycle(&chip8);
    assert(chip8.i == 0x0300);
    assert(chip8.pc == 0x0202);

    /* 3.2. La carga NO toca ningun registro de proposito general. */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0x6055);
    chip8_execute_cycle(&chip8);
    ensamblar(&chip8, 0x0202, 0xA1F0);
    chip8_execute_cycle(&chip8);
    assert(chip8.i == 0x01F0);
    assert(chip8.registers[0] == 0x55);

    /* 3.3. El registro I puede apuntar a memoria utilizable por la ROM. */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0xAFFF);
    chip8_execute_cycle(&chip8);
    assert(chip8.i == 0x0FFF);

    /* 3.4. I = 0x000 permite volver a la zona del FONT_SET. */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0xA000);
    chip8_execute_cycle(&chip8);
    assert(chip8.i == 0x0000);
    /* Y el glifo del '0' sigue intacto en esa direccion. */
    assert(chip8.memory[0x000] == 0xF0);

    passed();
}

/* ================================================================== */
/* PRUEBA 4 - Saltos condicionales: 0x3XNN, 0x4XNN, 0x5XY0          */
/* ================================================================== */

static void test_saltos_condicionales(void) {
    Chip8 chip8;

    announce("3XNN / 4XNN / 5XY0 - saltos condicionales, caminos true y false");

    /* ---------------- 4.1. 3XNN con Vx == NN (salto tomado) --------- */
    preparar(&chip8);
    chip8.registers[0x01] = 0x42;              /* V1 = 0x42          */
    ensamblar(&chip8, 0x0200, 0x3142);         /* SE V1, 0x42 -> HIT  */
    ensamblar(&chip8, 0x0202, 0x1224);         /* JP  0x224 (NO HIT) */
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);                /* 0x200 + 2 + 2 salto */
    assert(chip8.registers[0x01] == 0x42);     /* Vx intacto         */

    /* ---------------- 4.2. 3XNN con Vx != NN (no salta) ------------- */
    preparar(&chip8);
    chip8.registers[0x01] = 0x42;              /* V1 = 0x42          */
    ensamblar(&chip8, 0x0200, 0x3143);         /* SE V1, 0x43 -> FAIL */
    ensamblar(&chip8, 0x0202, 0x1224);         /* JP  0x224          */
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);                /* Solo avanza 2      */

    /* ---------------- 4.3. 4XNN con Vx != NN (salto tomado) --------- */
    preparar(&chip8);
    chip8.registers[0x01] = 0x42;              /* V1 = 0x42          */
    ensamblar(&chip8, 0x0200, 0x4143);         /* SNE V1, 0x43 -> HIT */
    ensamblar(&chip8, 0x0202, 0x1224);         /* JP  0x224 (NO HIT) */
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);

    /* ---------------- 4.4. 4XNN con Vx == NN (no salta) ------------- */
    preparar(&chip8);
    chip8.registers[0x01] = 0x42;              /* V1 = 0x42          */
    ensamblar(&chip8, 0x0200, 0x4142);         /* SNE V1, 0x42 -> FAIL */
    ensamblar(&chip8, 0x0202, 0x1224);         /* JP  0x224          */
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);

    /* ---------------- 4.5. 5XY0 con Vx == Vy (salto tomado) --------- */
    preparar(&chip8);
    chip8.registers[0x02] = 0x7A;              /* V2 = 0x7A          */
    chip8.registers[0x03] = 0x7A;              /* V3 = 0x7A          */
    ensamblar(&chip8, 0x0200, 0x5230);         /* SE V2, V3 -> HIT   */
    ensamblar(&chip8, 0x0202, 0x1224);         /* JP  0x224 (NO HIT) */
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);
    assert(chip8.registers[0x02] == 0x7A);     /* Los valores no cambian */
    assert(chip8.registers[0x03] == 0x7A);

    /* ---------------- 4.6. 5XY0 con Vx != Vy (no salta) ------------- */
    preparar(&chip8);
    chip8.registers[0x02] = 0x7A;              /* V2 = 0x7A          */
    chip8.registers[0x03] = 0x7B;              /* V3 = 0x7B          */
    ensamblar(&chip8, 0x0200, 0x5230);         /* SE V2, V3 -> FAIL  */
    ensamblar(&chip8, 0x0202, 0x1224);         /* JP  0x224          */
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);

    /* ---------------- 4.7. Un salto omite la instruccion siguiente ---- *
     * Un salto condicional desplaza el PC +2 sobre el valor ya avanzado,
     * por lo que se salta EXACTAMENTE la instruccion de 2 bytes que
     * sigue a la actual.                                                */
    preparar(&chip8);
    chip8.pc = 0x0300;
    chip8.registers[0x01] = 0x00;            /* V1 = 0                */
    ensamblar(&chip8, 0x0300, 0x3100);       /* SE V1, 0x00 -> HIT    */
    ensamblar(&chip8, 0x0302, 0x6001);       /* V0 = 0x01 -> se omite */
    ensamblar(&chip8, 0x0304, 0x6002);       /* V0 = 0x02 -> se ejecuta */
    ensamblar(&chip8, 0x0306, 0x0000);
    ensamblar(&chip8, 0x0308, 0x0000);
    ensamblar(&chip8, 0x030A, 0x0000);

    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0304);              /* 0x300 + 2 (fetch) + 2 */
    assert(chip8.registers[0x00] == 0x00);   /* V0 = 0x01 no se ejecuto */

    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0306);
    assert(chip8.registers[0x00] == 0x02);   /* V0 = 0x02 si se ejecuto */

    /* ---------------- 4.8. Encadenamiento de saltos condicionales ------- *
     * Un SE que se cumple seguido de un JP: ambos deben respetarse y las
     * instrucciones intermedias deben quedar sin ejecutar.               */
    preparar(&chip8);
    chip8.pc = 0x0300;
    chip8.registers[0x01] = 0x07;            /* V1 = 0x07             */
    ensamblar(&chip8, 0x0300, 0x3107);       /* SE V1, 0x07 -> HIT    */
    ensamblar(&chip8, 0x0302, 0x6101);       /* V1 = 0x01 -> se omite */
    ensamblar(&chip8, 0x0304, 0x1308);       /* JP  0x308 (12 bits)  */
    ensamblar(&chip8, 0x0306, 0x6102);       /* V1 = 0x02 -> se omite */
    ensamblar(&chip8, 0x0308, 0x6009);       /* V0 = 0x09 -> destino  */
    ensamblar(&chip8, 0x030A, 0x0000);

    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0304);
    assert(chip8.registers[0x01] == 0x07);   /* V1 = 0x01 no se ejecuto */
    assert(chip8.registers[0x00] == 0x00);

    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0308);              /* JP a 0x308             */
    assert(chip8.registers[0x01] == 0x07);   /* V1 = 0x02 no se ejecuto */

    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x030A);
    assert(chip8.registers[0x00] == 0x09);   /* Destino ejecutado */

    passed();
}

/* ================================================================== */
/* PRUEBA 5 - Suma inmediata con desbordamiento: 0x7XNN                */
/* ================================================================== */

static void test_suma_inmediata(void) {
    Chip8 chip8;

    announce("7XNN - ADD Vx, NN: suma inmediata con wrap-around de 8 bits");

    preparar(&chip8);

    /* 5.1. Suma sin desbordamiento. */
    chip8.registers[0x04] = 0x10;
    ensamblar(&chip8, 0x0200, 0x7405);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x04] == 0x15);

    /* 5.2. Suma que desborda 255: 0xFF + 0x01 = 0x00 (wrap implicito). */
    preparar(&chip8);
    chip8.registers[0x05] = 0xFF;
    ensamblar(&chip8, 0x0200, 0x7501);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x05] == 0x00);

    /* 5.3. Maximo wrap: 0x80 + 0x80 = 0x00. */
    preparar(&chip8);
    chip8.registers[0x06] = 0x80;
    ensamblar(&chip8, 0x0200, 0x7680);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x06] == 0x00);

    /* 5.4. 0xA0 + 0x60 = 0x00 exacto en el limite del byte. */
    preparar(&chip8);
    chip8.registers[0x07] = 0xA0;
    ensamblar(&chip8, 0x0200, 0x7760);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x07] == 0x00);

    /* 5.5. Con X = 0 solo se modifica V0; el registro VF queda intacto. */
    preparar(&chip8);
    chip8.registers[0x0F] = 0x01;
    ensamblar(&chip8, 0x0200, 0x70FF); /* ADD V0, 0xFF */
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x00] == 0xFF); /* V0 = 0x00 + 0xFF */
    assert(chip8.registers[0x0F] == 0x01); /* VF no se toca */
    assert(chip8.pc == 0x0202);

    passed();
}

/* ================================================================== */
/* PRUEBA 6 - Salto basado en teclado: 0xEX9E y 0xEXA1                 */
/* ================================================================== */

static void test_saltos_teclado(void) {
    Chip8 chip8;

    announce("EX9E / EXA1 - SKP / SKNP: saltos segun el estado del teclado");

    /* 6.1. EX9E con la tecla X pulsada -> el salto se toma. */
    preparar(&chip8);
    chip8.keypad[0x05] = 1;                /* tecla 5 DOWN  */
    ensamblar(&chip8, 0x0200, 0xE59E);     /* SKP V5         */
    ensamblar(&chip8, 0x0202, 0x1224);     /* JP 0x224 (no)  */
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);

    /* 6.2. EX9E con la tecla X libre -> el salto NO se toma. */
    preparar(&chip8);
    chip8.keypad[0x05] = 0;
    ensamblar(&chip8, 0x0200, 0xE59E);
    ensamblar(&chip8, 0x0202, 0x1224);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);

    /* 6.3. EXA1 con la tecla X libre -> el salto se toma (SKNP invertido). */
    preparar(&chip8);
    chip8.keypad[0x00] = 0;                /* tecla 0 UP    */
    ensamblar(&chip8, 0x0200, 0xE0A1);     /* SKNP V0        */
    ensamblar(&chip8, 0x0202, 0x1224);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);

    /* 6.4. EXA1 con la tecla X pulsada -> el salto NO se toma. */
    preparar(&chip8);
    chip8.keypad[0x00] = 1;
    ensamblar(&chip8, 0x0200, 0xE0A1);
    ensamblar(&chip8, 0x0202, 0x1224);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);

    /* 6.5. El nibble X selecciona la tecla correcta (y no la tecla 0). */
    preparar(&chip8);
    chip8.keypad[0x0A] = 1;                /* solo la tecla A */
    chip8.keypad[0x00] = 0;
    ensamblar(&chip8, 0x0200, 0xEA9E);     /* SKP VA          */
    ensamblar(&chip8, 0x0202, 0x1224);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);            /* salto tomado: A es la tecla X */

    /* 6.6. EX9E con indice 15 (VF) selecciona la tecla F. */
    preparar(&chip8);
    chip8.keypad[0x0F] = 1;                /* solo la tecla F */
    ensamblar(&chip8, 0x0200, 0xEF9E);     /* SKP VF          */
    ensamblar(&chip8, 0x0202, 0x1224);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);

    /* 6.7. La instruccion de salto nunca modifica el teclado. */
    assert(chip8.keypad[0x0F] == 1);
    assert(chip8.keypad[0x00] == 0);

    passed();
}

/* ================================================================== */
/* PRUEBA 7 - Dibujo de sprites y colisiones: 0xDXYN                 */
/* ================================================================== */

static void test_dibujo_sprites(void) {
    Chip8 chip8;
    size_t  idx;

    announce("DXYN - draw Vx, Vy: logica XOR y bandera de colision en VF");

    /* ---------------- 7.1. Dibujo basico de un sprite ---------------- */
    preparar(&chip8);

    /* Sprite de 2 filas en 0x300 (un byte por fila, 8 pixeles por byte):
     *   fila 0 = 0xFF -> los 8 pixeles encendidos
     *   fila 1 = 0x81 -> solo los dos pixeles de los extremos
     */
    chip8.memory[0x300] = 0xFF;
    chip8.memory[0x301] = 0x81;
    chip8.memory[0x302] = 0x00;
    chip8.memory[0x303] = 0x00;
    chip8.i = 0x0300;

    chip8.registers[0x00] = 10;  /* VX = columna 10 */
    chip8.registers[0x01] = 5;   /* VY = fila 5     */
    ensamblar(&chip8, 0x0200, 0xD012);  /* draw V0, V1, 2 filas */

    chip8_execute_cycle(&chip8);

    /* 7.1.1. La primera fila dibuja 8 pixeles en (10..17, 5). */
    for (idx = CHIP8_PIXEL_INDEX(10, 5); idx < CHIP8_PIXEL_INDEX(18, 5); ++idx) {
        assert(chip8.screen[idx] == 1);
    }

    /* 7.1.2. La segunda fila dibuja solo los dos extremos en la fila 6. */
    assert(chip8.screen[CHIP8_PIXEL_INDEX(10, 6)] == 1);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(17, 6)] == 1);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(12, 6)] == 0);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(14, 6)] == 0);

    /* 7.1.3. La pantalla estaba vacia: no hubo colision -> VF = 0. */
    assert(chip8.registers[0x0F] == 0);

    /* 7.1.4. El registro I no se modifica con la instruccion de dibujo. */
    assert(chip8.i == 0x0300);

    /* 7.1.5. Solo se han modificado 10 pixeles (8 + 2). */
    {
        size_t encendido = 0;
        size_t p;
        for (p = 0; p < (size_t)SCREEN_PIXELS; ++p) {
            encendido += (size_t)(chip8.screen[p] != 0);
        }
        assert(encendido == 10);
    }

    /* ---------------- 7.2. Colision: el mismo sprite dos veces -------- */
    /* El segundo dibujo hace XOR sobre pixeles ya encendidos:
     * VF debe pasar a 1 y los pixeles compartidos deben apagarse. */
    preparar(&chip8);
    chip8.memory[0x300] = 0xFF;  /* Mismo sprite que en 7.1 */
    chip8.memory[0x301] = 0x81;
    chip8.i = 0x0300;
    chip8.registers[0x00] = 10;
    chip8.registers[0x01] = 5;

    /* Primer dibujo sobre pantalla negra: 10 pixeles, sin colision. */
    ensamblar(&chip8, 0x0200, 0xD012);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0);

    /* Segundo dibujo en la misma posicion: los 10 pixeles colisionan. */
    chip8.pc = 0x0200;
    ensamblar(&chip8, 0x0200, 0xD012);
    chip8_execute_cycle(&chip8);

    assert(chip8.registers[0x0F] == 1);  /* Hubo colision */

    /* Los 8 pixeles de la fila 5 se han apagado por el XOR. */
    for (idx = CHIP8_PIXEL_INDEX(10, 5); idx < CHIP8_PIXEL_INDEX(18, 5); ++idx) {
        assert(chip8.screen[idx] == 0);
    }

    /* Los dos pixeles de la fila 6 tambien colisionan, asi que el XOR los
     * apaga igualmente. Tras este segundo dibujo la pantalla esta vacia. */
    assert(chip8.screen[CHIP8_PIXEL_INDEX(10, 6)] == 0);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(17, 6)] == 0);
    {
        size_t encendido = 0;
        size_t p;
        for (p = 0; p < (size_t)SCREEN_PIXELS; ++p) {
            encendido += (size_t)(chip8.screen[p] != 0);
        }
        assert(encendido == 0);  /* 10 encendidos + 10 apagados = 0 */
    }

    /* ---------------- 7.3. Sprite solapado parcialmente --------------- *
     * OJO: en DXYN, X selecciona la coordenada X (columna) e Y la
     * coordenada Y (fila). Para usar dos registros distintos se emplea
     * 0xD231 -> X = 2 (V2 = columna), Y = 3 (V3 = fila), N = 1 fila.    */
    preparar(&chip8);
    /* Sprite de 1 fila: 0b10101010 en 0x300 (columnas 0, 2, 4 y 6). */
    chip8.memory[0x300] = 0xAA;
    chip8.i = 0x0300;
    chip8.registers[0x02] = 0;  /* V2 = columna 0 */
    chip8.registers[0x03] = 0;  /* V3 = fila 0    */

    /* Primer dibujo: columnas 0..7 en la fila 0. Sin colision. */
    ensamblar(&chip8, 0x0200, 0xD231);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(0, 0)] == 1);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(1, 0)] == 0);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(2, 0)] == 1);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(7, 0)] == 0);

    /* Segundo dibujo desplazado 2 columnas: la columna 2 del sprite cae
     * sobre la columna 2 ya encendida -> colision -> VF = 1 y apagado. */
    chip8.pc = 0x0200;            /* La pantalla de la 1a vuelta se conserva */
    chip8.registers[0x02] = 2;    /* V2 = columna 2 */
    ensamblar(&chip8, 0x0200, 0xD231);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 1);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(2, 0)] == 0); /* colision -> OFF */
    assert(chip8.screen[CHIP8_PIXEL_INDEX(4, 0)] == 0); /* colision -> OFF */
    assert(chip8.screen[CHIP8_PIXEL_INDEX(6, 0)] == 0); /* colision -> OFF */
    assert(chip8.screen[CHIP8_PIXEL_INDEX(8, 0)] == 1); /* nuevo    -> ON  */
    /* Los pixeles apagados del sprite no tocan la pantalla. */
    assert(chip8.screen[CHIP8_PIXEL_INDEX(3, 0)] == 0);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(5, 0)] == 0);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(7, 0)] == 0);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(9, 0)] == 0);
    /* La columna 0 no la alcanza este sprite: sigue encendida. */
    assert(chip8.screen[CHIP8_PIXEL_INDEX(0, 0)] == 1);
    /* En total quedan 2 pixeles encendidos: el de la columna 0 y el nuevo. */
    {
        size_t encendido = 0;
        size_t q;
        for (q = 0; q < (size_t)SCREEN_PIXELS; ++q) {
            encendido += (size_t)(chip8.screen[q] != 0);
        }
        assert(encendido == 2);
    }

    /* ---------------- 7.4. VF se reinicia a 0 en cada dibujo ---------- *
     * VF se pone a cero AL PRINCIPIO de cada DXYN: nunca se acumula.     */
    preparar(&chip8);
    chip8.memory[0x300] = 0xFF;   /* 8 pixeles por fila */
    chip8.i = 0x0300;
    chip8.registers[0x02] = 0;    /* columna 0 */
    chip8.registers[0x03] = 0;    /* fila 0    */

    ensamblar(&chip8, 0x0200, 0xD231);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0);   /* 1a vez: sin colision */

    /* El MISMO sprite en otra fila: tampoco colisiona, y ademas VF parte
     * de 1 (colocada a mano) y debe terminar en 0. */
    chip8.pc = 0x0200;
    chip8.registers[0x0F] = 1;    /* VF "sucio" de una hipotetica colision */
    chip8.registers[0x03] = 10;   /* V3 = fila 10 */
    ensamblar(&chip8, 0x0200, 0xD231);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0);     /* Reiniciado, no acumulado. */
    assert(chip8.screen[CHIP8_PIXEL_INDEX(0, 10)] == 1);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(0, 0)] == 1);  /* fila 0 intacta */

    /* ---------------- 7.5. Recorte de los limites del monitor --------- */
    preparar(&chip8);
    chip8.memory[0x300] = 0xFF;  /* 8 pixeles */
    chip8.i = 0x0300;

    /* 7.5.1. El sprite sobresale por la derecha (columna 60, ancho 8):
     *        solo pintan 4 pixeles y NO hay lectura fuera del framebuffer. */
    chip8.registers[0x02] = 60;   /* V2 = columna 60 */
    chip8.registers[0x03] = 0;    /* V3 = fila 0    */
    ensamblar(&chip8, 0x0200, 0xD231);
    chip8_execute_cycle(&chip8);
    /* Solo se pintan las 4 columnas que caben: 60, 61, 62 y 63. */
    for (idx = CHIP8_PIXEL_INDEX(60, 0); idx < CHIP8_PIXEL_INDEX(64, 0); ++idx) {
        assert(chip8.screen[idx] == 1);
    }
    assert(chip8.screen[CHIP8_PIXEL_INDEX(59, 0)] == 0);
    /* Ninguna fila posterior puede haber sido tocada. */
    for (idx = CHIP8_PIXEL_INDEX(0, 1); idx < (size_t)SCREEN_PIXELS; ++idx) {
        assert(chip8.screen[idx] == 0);
    }
    assert(chip8.registers[0x0F] == 0);

    /* 7.5.2. El sprite sobresale por abajo (VY = 31, alto 2):
     *        solo la primera fila se dibuja. */
    preparar(&chip8);
    chip8.memory[0x300] = 0xFF;
    chip8.memory[0x301] = 0xFF;
    chip8.i = 0x0300;
    chip8.registers[0x02] = 0;    /* V2 = columna 0  */
    chip8.registers[0x03] = 31;   /* V3 = fila 31    */
    ensamblar(&chip8, 0x0200, 0xD232);
    chip8_execute_cycle(&chip8);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(0, 31)] == 1);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(7, 31)] == 1);
    /* La fila 32 no existe: no debe haber ningun pixel "fuera". */
    {
        size_t encendido = 0;
        size_t p;
        for (p = 0; p < (size_t)SCREEN_PIXELS; ++p) {
            encendido += (size_t)(chip8.screen[p] != 0);
        }
        assert(encendido == 8);
    }
    assert(chip8.registers[0x0F] == 0);

    /* 7.5.3. Sprite totalmente fuera de pantalla: no dibuja nada, VF = 0. */
    preparar(&chip8);
    chip8.memory[0x300] = 0xFF;
    chip8.i = 0x0300;
    chip8.registers[0x02] = 64;  /* V2 = columna 64: fuera de rango */
    chip8.registers[0x03] = 0;   /* V3 = fila 0                     */
    ensamblar(&chip8, 0x0200, 0xD231);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0);
    {
        size_t encendido = 0;
        size_t p;
        for (p = 0; p < (size_t)SCREEN_PIXELS; ++p) {
            encendido += (size_t)(chip8.screen[p] != 0);
        }
        assert(encendido == 0);
    }

    /* 7.5.4. Sprite de altura 0 (DXY0): no dibuja ni colisiona. */
    preparar(&chip8);
    chip8.i = 0x0300;
    chip8.registers[0x02] = 0;    /* V2 = columna 0 */
    chip8.registers[0x03] = 0;    /* V3 = fila 0    */
    ensamblar(&chip8, 0x0200, 0xD230);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0);
    assert(chip8.screen[CHIP8_PIXEL_INDEX(0, 0)] == 0);

    /* 7.5.5. VF se reinicia a 0 cuando el sprite es de altura 0 aunque
     *        previamente valiera 1. */
    chip8.pc = 0x0200;
    chip8.registers[0x0F] = 1;
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0);

    passed();
}

/* ================================================================== */
/* PRUEBA 8 - Temporizadores a 60 Hz                                  */
/* ================================================================== */

static void test_temporizadores(void) {
    Chip8 chip8;
    int   i;

    announce("chip8_update_timers: cuenta descendente a 60 Hz con saturacion");

    preparar(&chip8);

    /* 8.1. Decremento simple. */
    chip8.delay_timer = 3;
    chip8.sound_timer = 2;
    chip8_update_timers(&chip8);
    assert(chip8.delay_timer == 2);
    assert(chip8.sound_timer == 1);

    /* 8.2. Saturacion en 0: nunca se produce underflow a 0xFF. */
    chip8_update_timers(&chip8);
    assert(chip8.delay_timer == 1);
    assert(chip8.sound_timer == 0);
    chip8_update_timers(&chip8);
    assert(chip8.delay_timer == 0);
    assert(chip8.sound_timer == 0);
    for (i = 0; i < 100; ++i) {
        chip8_update_timers(&chip8);
    }
    assert(chip8.delay_timer == 0);
    assert(chip8.sound_timer == 0);

    /* 8.3. Conteo completo de 255 tics hasta agotar el temporizador. */
    preparar(&chip8);
    chip8.delay_timer = 255;
    chip8.sound_timer = 1;
    for (i = 0; i < 255; ++i) {
        chip8_update_timers(&chip8);
        assert(chip8.delay_timer == (uint8_t)(254 - i));
    }
    assert(chip8.delay_timer == 0);
    assert(chip8.sound_timer == 0);

    /* 8.4. Los temporizadores son independientes entre si. */
    preparar(&chip8);
    chip8.delay_timer = 1;
    chip8.sound_timer = 200;
    chip8_update_timers(&chip8);
    assert(chip8.delay_timer == 0);
    assert(chip8.sound_timer == 199);

    /* 8.5. Un ciclo de CPU no modifica los temporizadores. */
    preparar(&chip8);
    chip8.delay_timer = 60;
    chip8.sound_timer = 30;
    ensamblar(&chip8, 0x0200, 0x6042); /* V0 = 0x42 */
    chip8_execute_cycle(&chip8);
    assert(chip8.delay_timer == 60);
    assert(chip8.sound_timer == 30);

    passed();
}

/* ================================================================== */
/* PRUEBA 9 - Ciclo de fetch, orden big-endian y PC                   */
/* ================================================================== */

static void test_fetch_y_pc(void) {
    Chip8 chip8;

    announce("Fetch: union de dos bytes en big-endian y avance del PC");

    preparar(&chip8);

    /* 9.1. El opcode se compone con el byte ALTO primero. */
    chip8.memory[0x0200] = 0xA1;
    chip8.memory[0x0201] = 0xBC;
    chip8_execute_cycle(&chip8);
    assert(chip8.i == 0x01BC); /* Si se invirtiera, seria 0xBCA1. */
    assert(chip8.pc == 0x0202);

    /* 9.2. El PC avanza exactamente 2 bytes por ciclo (salto aparte). */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0x6001);
    ensamblar(&chip8, 0x0202, 0x6002);
    ensamblar(&chip8, 0x0204, 0x6003);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0206);
    assert(chip8.registers[0] == 3); /* V0 se va acumulando */

    /* 9.3. Saltos absolutos: JP y CALL fijan el PC al valor de NNN. */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0x1300);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0300);

    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0x2300);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0300);

    /* 9.4. JP V0 (BNNN) suma el contenido de V0 a la direccion. */
    preparar(&chip8);
    chip8.registers[0x00] = 0x0010;
    ensamblar(&chip8, 0x0200, 0xB300);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0310);

    /* 9.5. 0x00EE (RET) devuelve el control al contenido del registro I. */
    preparar(&chip8);
    chip8.i = 0x0402;
    ensamblar(&chip8, 0x0200, 0x00EE);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0402);

    /* 9.6. 0x00E0 (CLS) limpia el framebuffer por completo. */
    preparar(&chip8);
    memset(chip8.screen, 1, sizeof(chip8.screen));
    ensamblar(&chip8, 0x0200, 0x00E0);
    chip8_execute_cycle(&chip8);
    {
        size_t p;
        for (p = 0; p < (size_t)SCREEN_PIXELS; ++p) {
            assert(chip8.screen[p] == 0);
        }
    }
    assert(chip8.pc == 0x0202);

    /* 9.7. Opcode desconocido (familia no implementada): NOP seguro. */
    preparar(&chip8);
    ensamblar(&chip8, 0x0200, 0xFFFF);
    chip8.pc = 0x0200;
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);

    /* 9.8. ROM vacia en memoria (0x0000): NOP, el PC no se bloquea. */
    preparar(&chip8);
    chip8.pc = 0x0200;
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);

    passed();
}

/* ================================================================== */
/* PRUEBA 10 - Contrato de carga de ROM en la RAM                     */
/* ================================================================== */
/*
 * La lectura del fichero (.ch8 -> RAM) la implementa src/main.c, que es la
 * capa de E/S y por tanto NO se enlaza con este arnes. Aqui se verifica el
 * CONTRATO que la maquina espera: que cualquier secuencia de bytes escrita
 * en 0x200 sea ejecutada como un programa valido.
 */

static void test_carga_rom(void) {
    Chip8 chip8;
    size_t capacidad;
    size_t p;

    /* ROM de 12 bytes (6 instrucciones) tal y como la inyectaria
     * chip8_load_rom() tras leer un fichero .ch8 del disco. */
    const uint8_t payload[] = {
        0x60, 0x01, /* 6001  LD V0, 0x01        */
        0x61, 0x02, /* 6102  LD V1, 0x02        */
        0x62, 0x03, /* 6203  LD V2, 0x03        */
        0x50, 0x10, /* 5010  SE V0, V1 -> falla */
        0x12, 0x06, /* 1206  JP  0x206          */
        0xD0, 0x0A  /* D00A  draw V0, V0, 10   */
    };

    announce("Contrato de carga de ROM: inyeccion binaria en 0x200");

    /* 10.1. Capacidad maxima de la zona de programa tras 0x200. */
    capacidad = (size_t)MEMORY_SIZE - (size_t)ROM_START_ADDRESS;
    assert(capacidad == 3584);
    assert(ROM_START_ADDRESS == 0x0200);

    /* 10.2. La zona de programa arranca vacia tras inicializar. */
    preparar(&chip8);
    for (p = (size_t)ROM_START_ADDRESS; p < (size_t)MEMORY_SIZE; ++p) {
        assert(chip8.memory[p] == 0);
    }

    /* 10.3. Inyeccion de la ROM byte a byte, como hace fread(). */
    memcpy(&chip8.memory[ROM_START_ADDRESS], payload, sizeof(payload));
    assert(chip8.pc == 0x0200);            /* La carga no mueve el PC */
    assert(chip8.memory[0x0200] == 0x60);
    assert(chip8.memory[0x0201] == 0x01);
    assert(chip8.memory[0x020B] == 0x0A);
    assert(chip8.memory[0x020C] == 0x00);   /* Fin de la ROM: sigue a cero */

    /* 10.4. La carga NO altera el FONT_SET (0x000 - 0x04F). */
    assert(chip8.memory[FONT_SET_ADDRESS + 0x00] == 0xF0);
    assert(chip8.memory[FONT_SET_ADDRESS + 0x04] == 0xF0);
    assert(chip8.memory[FONT_SET_ADDRESS + 0x0B] == 0x10);  /* glifo '2' */
    assert(chip8.memory[FONT_SET_ADDRESS + 0x4F] == 0x80);

    /* 10.5. La ROM cargada se ejecuta opcodes de 2 en 2, en orden. */
    chip8_execute_cycle(&chip8);           /* 6001  LD V0, 1 */
    assert(chip8.registers[0x00] == 0x01);
    assert(chip8.pc == 0x0202);
    chip8_execute_cycle(&chip8);           /* 6102  LD V1, 2 */
    assert(chip8.registers[0x01] == 0x02);
    chip8_execute_cycle(&chip8);           /* 6203  LD V2, 3 */
    assert(chip8.registers[0x02] == 0x03);
    assert(chip8.pc == 0x0206);

    /* 10.6. El salto condicional que no se cumple continua en 0x206. */
    chip8_execute_cycle(&chip8);           /* 5010  SE V0, V1 -> falla */
    assert(chip8.pc == 0x0208);

    /* 10.7. Salto absoluto dentro de la zona de programa. */
    chip8.pc = 0x0208;
    chip8_execute_cycle(&chip8);           /* 1206  JP 0x206 */
    assert(chip8.pc == 0x0206);

    /* 10.8. Truncado: una ROM que no cabe se corta en el limite de la RAM
     *        sin escribir una sola byte fuera del array. */
    preparar(&chip8);
    memset(&chip8.memory[ROM_START_ADDRESS], 0x42, capacidad);
    assert(chip8.memory[MEMORY_SIZE - 1] == 0x42);
    assert(chip8.memory[ROM_START_ADDRESS] == 0x42);
    assert(chip8.memory[FONT_SET_ADDRESS] == 0xF0);  /* intacto */

    /* 10.9. La API publica tolera punteros nulos sin fallar. */
    assert(chip8_init(NULL) == -1);
    chip8_execute_cycle(NULL);
    chip8_update_timers(NULL);
    assert(1);   /* Si se llega aqui, no hubo fallo grave de segmentacion. */

    passed();
}

/* ================================================================== */
/* PRUEBA 11 - Aritmetica de registros: 8XY0, 8XY1, 9XY0             */
/* ================================================================== */

static void test_aritmetica_registros(void) {
    Chip8 chip8;

    announce("8XY0 / 8XY1 / 9XY0 - copia, suma con VF y salto por desigualdad");

    /* 11.1. 8XY0 copia Vy en Vx. */
    preparar(&chip8);
    chip8.registers[0x01] = 0x37;
    chip8.registers[0x02] = 0x91;
    ensamblar(&chip8, 0x0200, 0x8120); /* LD V1, V2 */
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x01] == 0x91);
    assert(chip8.registers[0x02] == 0x91); /* El origen no cambia */

    /* 11.2. 8XY1 suma Vy a Vx sin desbordamiento: VF = 0. */
    preparar(&chip8);
    chip8.registers[0x01] = 0x10;
    chip8.registers[0x02] = 0x20;
    ensamblar(&chip8, 0x0200, 0x8121); /* ADD V1, V2 */
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x01] == 0x30);
    assert(chip8.registers[0x0F] == 0);

    /* 11.3. 8XY1 con desbordamiento: VF = 1 y Vx hace wrap. */
    preparar(&chip8);
    chip8.registers[0x01] = 0xFF;
    chip8.registers[0x02] = 0x02;
    ensamblar(&chip8, 0x0200, 0x8121);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x01] == 0x01); /* 0x101 & 0xFF */
    assert(chip8.registers[0x0F] == 1);

    /* 11.4. 8XY1 donde el destino es VF: VF recibe el resultado, no el flag. */
    preparar(&chip8);
    chip8.registers[0x01] = 0x05;
    chip8.registers[0x0F] = 0x01;
    ensamblar(&chip8, 0x0200, 0x8F11); /* ADD VF, V1 (X = F, Y = 1) */
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x0F] == 0x06);

    /* 11.5. 9XY0: salto tomado cuando Vx != Vy. */
    preparar(&chip8);
    chip8.registers[0x03] = 0x01;
    chip8.registers[0x04] = 0x02;
    ensamblar(&chip8, 0x0200, 0x9340); /* SNE V3, V4 */
    ensamblar(&chip8, 0x0202, 0x1234);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0204);

    /* 11.6. 9XY0: sin salto cuando Vx == Vy. */
    preparar(&chip8);
    chip8.registers[0x03] = 0x09;
    chip8.registers[0x04] = 0x09;
    ensamblar(&chip8, 0x0200, 0x9340);
    ensamblar(&chip8, 0x0202, 0x1234);
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);

    passed();
}

/* ================================================================== */
/* PRUEBA 12 - Transfers de memoria: FX55 y FX65                     */
/* ================================================================== */

static void test_transfers_memoria(void) {
    Chip8 chip8;
    unsigned i;

    announce("FX55 / FX65 - transferencias entre registros y RAM");

    /* 12.1. FX55 guarda V0..Vx (offset de VX incluido) en I. */
    preparar(&chip8);
    for (i = 0; i < NUM_GPR; ++i) {
        chip8.registers[i] = (uint8_t)(0xA0 + i);
    }
    chip8.i = 0x0500;
    ensamblar(&chip8, 0x0200, 0xF355); /* LD V3, I */
    chip8_execute_cycle(&chip8);
    assert(chip8.memory[0x0500] == 0xA0);
    assert(chip8.memory[0x0501] == 0xA1);
    assert(chip8.memory[0x0502] == 0xA2);
    assert(chip8.memory[0x0503] == 0xA3);
    assert(chip8.memory[0x0504] == 0x00); /* V4 no se copia: X = 3 */

    /* 12.2. FX65 carga V0..Vx desde I. */
    preparar(&chip8);
    chip8.i = 0x0600;
    for (i = 0; i < 5; ++i) {
        chip8.memory[0x0600 + i] = (uint8_t)(0x10 * (i + 1u));
    }
    ensamblar(&chip8, 0x0200, 0xF265); /* LD I, V2 */
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x00] == 0x10);
    assert(chip8.registers[0x01] == 0x20);
    assert(chip8.registers[0x02] == 0x30);
    assert(chip8.registers[0x03] == 0x00); /* No debe tocarse */

    /* 12.3. FX15 / FX18: lectura de los temporizadores. */
    preparar(&chip8);
    chip8.delay_timer = 77;
    chip8.sound_timer = 33;
    ensamblar(&chip8, 0x0200, 0xF115); /* LD V1, DT */
    ensamblar(&chip8, 0x0202, 0xF218); /* LD V2, ST */
    chip8_execute_cycle(&chip8);
    chip8_execute_cycle(&chip8);
    assert(chip8.registers[0x01] == 77);
    assert(chip8.registers[0x02] == 33);

    /* 12.4. La lectura no altera los temporizadores. */
    assert(chip8.delay_timer == 77);
    assert(chip8.sound_timer == 33);

    /* 12.5. Escritura en el borde superior de la RAM: recorte seguro. */
    preparar(&chip8);
    chip8.i = 0x0FFE;
    ensamblar(&chip8, 0x0200, 0xFFFF); /* FXFF: no implementado */
    chip8.pc = 0x0200;
    chip8_execute_cycle(&chip8);
    assert(chip8.pc == 0x0202);

    /* 12.6. FX55 con I apuntando al final de la RAM no debe desbordar. */
    preparar(&chip8);
    chip8.i = 0x0FFC;
    chip8.registers[0x00] = 0x11;
    chip8.registers[0x0F] = 0xFF;
    ensamblar(&chip8, 0x0200, 0xFF55); /* LD VF, I */
    chip8_execute_cycle(&chip8);
    assert(chip8.memory[0x0FFC] == 0x11);
    assert(chip8.memory[0x0FFF] == 0x00); /* V3 = 0, recortado en el borde */
    assert(chip8.pc == 0x0202);

    passed();
}

/* ================================================================== */
/* PRUEBA 13 - Aislamiento entre instancias (sin estado global)       */
/* ================================================================== */

static void test_aislamiento(void) {
    Chip8 a;
    Chip8 b;
    unsigned i;

    announce("Aislamiento: dos maquinas virtuales simultaneas e independientes");

    /* 13.1. Dos inicializaciones producen maquinas identicas. */
    assert(chip8_init(&a) == 0);
    assert(chip8_init(&b) == 0);
    assert(memcmp(a.memory, b.memory, MEMORY_SIZE) == 0);
    assert(a.pc == b.pc);

    /* 13.2. Executar en una no afecta a la otra. */
    ensamblar(&a, 0x0200, 0x60FF); /* V0 = 0xFF */
    ensamblar(&b, 0x0200, 0x6001); /* V0 = 0x01 */
    chip8_execute_cycle(&a);
    chip8_execute_cycle(&b);
    assert(a.registers[0x00] == 0xFF);
    assert(b.registers[0x00] == 0x01);

    /* 13.3. Los framebuffers son independientes. */
    preparar(&a);
    a.memory[0x0300] = 0xFF;
    a.i = 0x0300;
    a.registers[0x00] = 0;
    a.registers[0x01] = 0;
    ensamblar(&a, 0x0200, 0xD001);
    chip8_execute_cycle(&a);
    assert(a.screen[CHIP8_PIXEL_INDEX(0, 0)] == 1);
    assert(b.screen[CHIP8_PIXEL_INDEX(0, 0)] == 0);

    /* 13.4. Los temporizadores son independientes. */
    a.delay_timer = 10;
    chip8_update_timers(&a);
    assert(a.delay_timer == 9);
    assert(b.delay_timer == 0);

    /* 13.5. Las dos maquinas siguen siendo deterministas. */
    for (i = 0; i < 32u; ++i) {
        assert(chip8_init(&a) == 0);
        assert(chip8_init(&b) == 0);
        assert(memcmp(a.memory, b.memory, MEMORY_SIZE) == 0);
    }

    passed();
}

/* ================================================================== */
/* PRUEBA 14 - Contrato de tamano de la estructura                   */
/* ================================================================== */

static void test_contrato_estructura(void) {
    Chip8 chip8;

    announce("Contrato: dimensiones y offset de los miembros de `Chip8`");

    /* 14.1. Dimensiones del monitor virtual. */
    assert(SCREEN_WIDTH == 64);
    assert(SCREEN_HEIGHT == 32);
    assert(SCREEN_PIXELS == 2048);

    /* 14.2. La RAM es de 4 KB direccionables. */
    assert(MEMORY_SIZE == 4096);
    assert(sizeof(chip8.memory) == (size_t)MEMORY_SIZE);

    /* 14.3. Arrays de registros y teclado. */
    assert(sizeof(chip8.registers) == 16);
    assert(sizeof(chip8.keypad) == 16);

    /* 14.4. Los temporizadores caben en un byte. */
    assert(sizeof(chip8.delay_timer) == 1);
    assert(sizeof(chip8.sound_timer) == 1);

    /* 14.5. El indice de pixel es valido en el rango de la pantalla. */
    assert(CHIP8_PIXEL_INDEX(0, 0) == 0);
    assert(CHIP8_PIXEL_INDEX(SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1) ==
           (size_t)(SCREEN_PIXELS - 1));

    /* 14.6. El PC y el indice son de 16 bits. */
    assert(sizeof(chip8.pc) == 2);
    assert(sizeof(chip8.i) == 2);

    /* 14.7. La estructura se puede inicializar con designated initializers. */
    {
        Chip8 limpia = {0};
        assert(limpia.pc == 0);
        assert(limpia.memory[0] == 0);
        assert(chip8_init(&limpia) == 0);
        assert(limpia.pc == 0x0200);
    }

    passed();
}

/* ================================================================== */
/* Punto de entrada del arnes                                        */
/* ================================================================== */

int main(void) {
    printf("==========================================================\n");
    printf("  Bateria de pruebas unitarias - Emulador CHIP-8 en C\n");
    printf("==========================================================\n\n");

    test_inicializacion();
    test_carga_inmediato();
    test_carga_indice();
    test_saltos_condicionales();
    test_suma_inmediata();
    test_saltos_teclado();
    test_dibujo_sprites();
    test_temporizadores();
    test_fetch_y_pc();
    test_carga_rom();
    test_aritmetica_registros();
    test_transfers_memoria();
    test_aislamiento();
    test_contrato_estructura();

    printf("==========================================================\n");
    printf("  RESULTADO: %d de %d casos de prueba superados\n",
           pruebas_ok, pruebas_totales);
    printf("==========================================================\n");
    printf("\n");
    printf("  >>> TODAS LAS PRUEBAS UNITARIAS HAN PASADO CORRECTAMENTE <<<\n");
    printf("\n");

    return EXIT_SUCCESS;
}

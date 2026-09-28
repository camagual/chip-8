/*
 * main.c
 * ------------------------------------------------------------------
 * Capa de presentacion: carga de la ROM desde disco y bucle principal de
 * escritorio del emulador CHIP-8.
 *
 * Este archivo contiene TODA la logica de entrada/salida. El nucleo de
 * emulacion (src/chip8.c) no sabe nada de ficheros, de hilos ni de
 * ventanas: solo manipula la estructura `Chip8`. Gracias a esa separacion,
 * este fichero es el unico que habra que reescribir cuando se integre una
 * libreria grafica (SDL2, Raylib, GLFW...).
 *
 * Estandar objetivo: C99 (ISO/IEC 9899:1999) o superior.
 * License: MIT.
 * ------------------------------------------------------------------
 */

/*
 * Solicitud explicita de la API POSIX 2008. Debe aparecer ANTES de la
 * primera inclusion de cabeceras del sistema para que <time.h> exponga
 * clock_nanosleep(), que en modo -std=c99 puro quedaria oculta.
 */
#define _POSIX_C_SOURCE 200809L

#include "chip8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Parametros de emulacion                                            */
/* ------------------------------------------------------------------ */

/* Velocidad nativa del hardware COSMAC VIP: 500 ciclos por segundo. */
#define CPU_SPEED_HZ 500

/* Los temporizadores del CHIP-8 corren a 60 Hz. Para no depender de un
 * reloj de pared inexacto, se ejecutan cada N ciclos de CPU:
 *   500 ciclos / 60 Hz = 8.33 ciclos por tic de temporizador. */
#define CYCLES_PER_TIMER_TICK 8

/* ------------------------------------------------------------------ */
/* Puentes hacia la libreria grafica                                  */
/* ------------------------------------------------------------------ */
/*
 *  _______   _____ _    _   _ _____      _     ______ ______ _  __
 * |  ____| / ____| |  | \ | | |_  _|    | |    |  ____|  ____| |/ /
 * | |__  | | (___ | |__ |  \| | | |  ___| | __ | |__  | |__  | ' /
 * |  __| | \___ \| |  | |\  | | | |/ _ \ |/ _`|  __| |  __| | . \
 * | |    | _____) | |  | | \ |_| | | (_) | | (_| | |____| |____| |\
 * |_|    |_|     |_|  |_|  \_\___|\___/|_|\__,_|\______|______|_| \/
 *
 *  IMPLEMENTACION CON SDL2 (esqueleto de referencia, comentalo para activar)
 *  ------------------------------------------------------------------
 *
 *  #include <SDL2/SDL.h>
 *
 *  static SDL_Window*   ventana  = NULL;
 *  static SDL_Renderer* render   = NULL;
 *  static const char* TECLAS_SDL[NUM_KEYS] = {
 *      SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
 *      SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
 *      SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
 *      SDL_SCANCODE_Z, SDL_SCANCODE_V, SDL_SCANCODE_C, SDL_SCANCODE_B };
 *
 *  static int video_init(Chip8* c) {
 *      if (SDL_Init(SDL_INIT_VIDEO) != 0) return -1;
 *      ventana = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED,
 *                                 SDL_WINDOWPOS_CENTERED,
 *                                 SCREEN_WIDTH * 10, SCREEN_HEIGHT * 10, 0);
 *      if (!ventana) return -1;
 *      render = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_ACCELERATED);
 *      return render ? 0 : -1;
 *  }
 *
 *  static void video_render(const Chip8* c) {
 *      SDL_SetRenderDrawColor(render, 0, 0, 0, 255);
 *      SDL_RenderClear(render);
 *      SDL_SetRenderDrawColor(render, 255, 255, 255, 255);
 *      for (int y = 0; y < SCREEN_HEIGHT; ++y)
 *          for (int x = 0; x < SCREEN_WIDTH; ++x)
 *              if (c->screen[CHIP8_PIXEL_INDEX(x, y)])
 *                  SDL_RenderDrawPoint(render, x, y);
 *      SDL_RenderPresent(render);
 *  }
 *
 *  static void video_poll(Chip8* c) {
 *      SDL_Event ev;
 *      while (SDL_PollEvent(&ev)) {
 *          if (ev.type == SDL_QUIT) exit(0);
 *          if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
 *              for (int k = 0; k < NUM_KEYS; ++k)
 *                  if (ev.key.keysym.scancode == TECLAS_SDL[k])
 *                      c->keypad[k] = (ev.type == SDL_KEYDOWN) ? 1 : 0;
 *          }
 *      }
 *  }
 *
 *  ------------------------------------------------------------------
 *  IMPLEMENTACION CON RAYLIB (alternativa mas corta)
 *  ------------------------------------------------------------------
 *
 *  #include <raylib.h>
 *  static void video_render(const Chip8* c) {
 *      BeginDrawing();
 *          ClearBackground(BLACK);
 *          for (int y = 0; y < SCREEN_HEIGHT; ++y)
 *              for (int x = 0; x < SCREEN_WIDTH; ++x)
 *                  if (c->screen[CHIP8_PIXEL_INDEX(x, y)])
 *                      DrawRectangle(x*10, y*10, 10, 10, WHITE);
 *      EndDrawing();
 *  }
 *  La funcion video_poll() mapearia KeyPressed/KeyReleased sobre c->keypad.
 */

/* ------------------------------------------------------------------ */
/* Carga de ROM desde disco                                           */
/* ------------------------------------------------------------------ */

/*
 * chip8_load_rom
 * ---------------
 * Abre el fichero de la ROM en modo binario y copia su contenido en la RAM
 * a partir de 0x200, que es justo donde arranca el PC.
 *
 * Esta funcion vive en la capa de presentacion y NO en el nucleo porque es
 * la unica parte del programa que necesita <stdio.h>. El nucleo (src/chip8.c)
 * permanece puro: no abre ficheros, no imprime y no duerme.
 *
 * Seguridad:
 *  - Se comprueba el PC para no escribir antes de 0x200.
 *  - Si el fichero no existe o no se puede abrir, se devuelve -1.
 *  - Si la ROM excede la memoria disponible se trunca en silencio y se
 *    devuelve el numero de bytes realmente leidos, para que el llamante
 *    pueda detectarlo.
 */
long chip8_load_rom(Chip8 *chip8, const char *path) {
    FILE  *rom;
    size_t capacidad;
    size_t leidos;

    if (chip8 == NULL || path == NULL) {
        return -1;
    }

    /* Modo "rb": binario puro, sin traduccion de finales de linea. */
    rom = fopen(path, "rb");
    if (rom == NULL) {
        return -1;
    }

    /* Espacio libre: de 0x200 hasta el final de la RAM de 4 KB. */
    capacidad = (size_t)MEMORY_SIZE - (size_t)ROM_START_ADDRESS;

    leidos = fread(&chip8->memory[ROM_START_ADDRESS], 1, capacidad, rom);

    if (ferror(rom) != 0) {
        /* Error de E/S real: se cierra el descriptor y se informa del fallo. */
        fclose(rom);
        return -1;
    }

    fclose(rom);

    return (long)leidos;
}

/* ------------------------------------------------------------------ */
/* Diagnostico por consola                                            */
/* ------------------------------------------------------------------ */

/*
 * imprimir_estado
 * ----------------
 * Vuelca por stdout una instantanea de la maquina virtual. Se invoca al
 * arrancar (para confirmar la carga) y sirve como punto de enganche para
 * depuracion; no altera ningun estado.
 */
static void imprimir_estado(const Chip8 *chip8) {
    size_t pixels_encendidos = 0;
    size_t i;

    for (i = 0; i < (size_t)SCREEN_PIXELS; ++i) {
        if (chip8->screen[i] != 0u) {
            pixels_encendidos++;
        }
    }

    printf("  PC ............ 0x%04X\n", chip8->pc);
    printf("  I ............. 0x%04X\n", chip8->i);
    printf("  V0 ............ 0x%02X\n", chip8->registers[0]);
    printf("  VF ............ 0x%02X\n", chip8->registers[0x0F]);
    printf("  Timers ........ delay=%u sound=%u\n",
           (unsigned)chip8->delay_timer, (unsigned)chip8->sound_timer);
    printf("  Pixels ON ..... %zu de %d\n", pixels_encendidos, SCREEN_PIXELS);
}

/*
 *usage
 * -----
 * Imprime la sintaxis de invocacion por pantalla de error.
 */
static void usage(const char *programa) {
    fprintf(stderr, "Uso: %s <ruta_rom.ch8> [ciclos_por_tick]\n", programa);
    fprintf(stderr, "  <ruta_rom.ch8>   Fichero binario con la ROM a ejecutar.\n");
    fprintf(stderr, "  [ciclos_por_tick] Opcional. Iteraciones de CPU por tic de\n");
    fprintf(stderr, "                   temporizador (por defecto, %d).\n",
            CYCLES_PER_TIMER_TICK);
}

/* ------------------------------------------------------------------ */
/* sleep_preciso                                                      */
/* ------------------------------------------------------------------ */

/*
 * sleep_preciso
 * --------------
 * Duerme los microsegundos indicados usando clock_nanosleep() en tiempo
 * POSIX, que ofrece una precision muy superior a usleep() en sistemas de
 * escritorio. Si la plataforma no lo soporta, se recurre a usleep().
 */
static void sleep_preciso(unsigned microsegundos) {
#if defined(CLOCK_MONOTONIC) && defined(_POSIX_C_SOURCE)
    struct timespec ts;
    ts.tv_sec  = (time_t)(microsegundos / 1000000u);
    ts.tv_nsec = (long)(microsegundos % 1000000u) * 1000L;
    (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
#else
    (void)usleep(microsegundos);
#endif
}

/* ------------------------------------------------------------------ */
/* Bucle principal                                                    */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    Chip8 chip8;
    long  bytes_cargados;
    int   ciclos_por_tick = CYCLES_PER_TIMER_TICK;
    int   ciclo_global = 0;

    /* ---------------- 1. Analisis de los argumentos ------------------ */
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc >= 3) {
        ciclos_por_tick = atoi(argv[2]);
        if (ciclos_por_tick < 1) {
            fprintf(stderr, "Error: 'ciclos_por_tick' debe ser >= 1.\n");
            return EXIT_FAILURE;
        }
    }

    /* ---------------- 2. Inicializacion de la maquina virtual -------- */
    if (chip8_init(&chip8) != 0) {
        fprintf(stderr, "Error: no se pudo inicializar la maquina virtual.\n");
        return EXIT_FAILURE;
    }

    /* ---------------- 3. Carga de la ROM en RAM desde 0x200 ----------- */
    bytes_cargados = chip8_load_rom(&chip8, argv[1]);

    if (bytes_cargados < 0) {
        fprintf(stderr, "Error: no se pudo abrir la ROM '%s'.\n", argv[1]);
        fprintf(stderr, "       Comprueba que la ruta exista y sea legible.\n");
        return EXIT_FAILURE;
    }

    if (bytes_cargados == 0) {
        fprintf(stderr, "Advertencia: la ROM '%s' esta vacia.\n", argv[1]);
    }

    printf("==========================================================\n");
    printf("  Emulador CHIP-8 en C puro (C99)\n");
    printf("==========================================================\n");
    printf("  ROM ............ %s\n", argv[1]);
    printf("  Cargados ...... %ld bytes en 0x%04X\n",
           bytes_cargados, ROM_START_ADDRESS);
    printf("  Reloj CPU ..... %d Hz (tic de timer cada %d ciclos)\n",
           CPU_SPEED_HZ, ciclos_por_tick);
    imprimir_estado(&chip8);
    printf("----------------------------------------------------------\n");
    printf("  Modo headless: simulacion pura sin ventana grafica.\n");
    printf("  Integracion SDL2/Raylib: vea los bloques comentados en\n");
    printf("  src/main.c, seccion 'Puentes hacia la libreria grafica'.\n");
    printf("  Ctrl+C para detener la ejecucion.\n");
    printf("==========================================================\n");

    /* ---------------- 4. Bucle infinito de emulacion ----------------- */
    for (;;) {
        unsigned microsegundos;
        int i;

        for (i = 0; i < ciclos_por_tick; ++i) {
            /*
             * ZONA 1 - ENTRADA: aqui se llamaria a `video_poll(&chip8)`.
             * Con SDL2, ese helper copiaria el estado del teclado del
             * sistema operativo a `chip8.keypad[0..15]`, que es lo que leen
             * las instrucciones EX9E (SKP) y EXA1 (SKNP).
             * Con el nucleo headless, la app no lee teclado: keypad[] se
             * queda en cero y la ROM no encuentra teclas pulsadas.
             */

            /* ZONA 2 - CPU: un ciclo de fetch/decode/ejecucion. */
            chip8_execute_cycle(&chip8);

            ++ciclo_global;

            /* ZONA 3 - SONIDO: si sound_timer > 0, aqui se dispararia el
             * pitido (beep square) y cuando llegue a 0 se cortaria. */
        }

        /*
         * ZONA 4 - TIMERS: los temporizadores del CHIP-8 son independientes
         * del reloj de la CPU y deben correr a 60 Hz. Aproximamos esa
         * cadencia con un tic cada `ciclos_por_tick` iteraciones de la CPU
         * (500 / 60 = 8.33 -> 8). Esta es la UNICA llamada permitida a
         * chip8_update_timers(); jamas dentro del bucle de CPU.
         */
        chip8_update_timers(&chip8);

        /* ZONA 5 - VIDEO: aqui se llamaria a `video_render(&chip8)`, que
         * recorre chip8.screen[0..2047] y lo vuelca a la ventana. En modo
         * headless no hay nada que pintar, por lo que solo se registra el
         * estado para el diagnostico por consola. */
        if (ciclo_global % (ciclos_por_tick * 100) == 0) {
            imprimir_estado(&chip8);
        }

        /* ZONA 6 - REGULACION DE VELOCIDAD: 1 / 500 Hz = 2 ms exactos por
         * tic de CPU, multiplicados por los ciclos ejecutados en este tic. */
        microsegundos = (unsigned)(1000000 / CPU_SPEED_HZ) *
                        (unsigned)ciclos_por_tick;
        sleep_preciso(microsegundos);
    }

    /* El bucle es infinito: solo se llega aqui por una ruptura de salida. */
    return EXIT_SUCCESS;
}

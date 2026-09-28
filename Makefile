# ===================================================================
#  Makefile - Emulador CHIP-8 en C puro (C99)
#  ------------------------------------------------------------------
#  Objetivos disponibles:
#    make          Compila el emulador -> ./chip8
#    make test     Compila y ejecuta la bateria de pruebas unitarias
#    make clean    Borra binarios y ficheros objeto
#    make rebuild  clean + make
#    make debug    Compila con simbolos y avisos maxima (-g3 -O0)
#    make asan     Compila y ejecuta los tests con AddressSanitizer
#    make run ROM=./mi_rom.ch8   Compila y arranca con una ROM
#    make help     Lista los objetivos disponibles
# ===================================================================

# --- Compilador y estandar de lenguaje --------------------------------
CC      := gcc
STD     := -std=c99

# --- Rutas del proyecto -----------------------------------------------
SRCDIR   := src
INCDIR   := include
TESTDIR  := tests
BUILDDIR := build

# --- Compilacion del emulador (nucleo + capa de presentacion) --------
EMU_SRCS := $(SRCDIR)/chip8.c $(SRCDIR)/main.c
EMU_OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(EMU_SRCS))
EMU_BIN  := chip8

# --- Compilacion de la bateria de pruebas ------------------------------
# Comparte el objeto del nucleo (build/chip8.o) con el emulador: el codigo
# objeto se genera una sola vez y se reutiliza en ambos binarios.
TEST_OBJS := $(BUILDDIR)/tests.o $(BUILDDIR)/chip8.o
TEST_BIN  := $(BUILDDIR)/tests

# --- Flags -------------------------------------------------------------
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
           -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual
OPT      := -O2
CFLAGS   := $(STD) $(WARNINGS) $(OPT) -I$(INCDIR)
LDFLAGS  :=
LDLIBS   :=

# --- Colores para la salida del terminal (se desactivan si no hay TTY) -
ifeq ($(shell tput colors 2>/dev/null),)
BOLD  :=
RESET :=
else
BOLD  := $(shell tput bold)
RESET := $(shell tput sgr0)
endif

# --- Reglas ------------------------------------------------------------

.PHONY: all test clean rebuild debug asan run help

# Objetivo por defecto: construye el emulador.
all: $(EMU_BIN)

$(EMU_BIN): $(EMU_OBJS) | $(BUILDDIR)
	@echo "$(BOLD)[LD ]$(RESET)  $@"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compilacion de cada objeto del emulador.
$(BUILDDIR)/chip8.o: $(SRCDIR)/chip8.c $(INCDIR)/chip8.h | $(BUILDDIR)
	@echo "$(BOLD)[CC ]$(RESET)  $<"
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/main.o: $(SRCDIR)/main.c $(INCDIR)/chip8.h | $(BUILDDIR)
	@echo "$(BOLD)[CC ]$(RESET)  $<"
	$(CC) $(CFLAGS) -c -o $@ $<

# Compilacion de cada objeto de las pruebas.
$(BUILDDIR)/tests.o: $(TESTDIR)/tests.c $(INCDIR)/chip8.h | $(BUILDDIR)
	@echo "$(BOLD)[CC ]$(RESET)  $<"
	$(CC) $(CFLAGS) -c -o $@ $<

# Enlazado de la bateria de pruebas.
$(TEST_BIN): $(TEST_OBJS) | $(BUILDDIR)
	@echo "$(BOLD)[LD ]$(RESET)  $@"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Crea el directorio de construccion si no existe.
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Objetivo `test`: compila y ejecuta la bateria completa de pruebas.
test: $(TEST_BIN)
	@echo "$(BOLD)[RUN]$(RESET)  Bateria de pruebas unitarias"
	@$(TEST_BIN)

# Compilacion con simbolos de depuracion y optimizacion desactivada.
debug: CFLAGS := $(STD) $(WARNINGS) -O0 -g3 -I$(INCDIR)
debug: clean all $(TEST_BIN)
	@echo "$(BOLD)Binario de depuracion listo: ./$(EMU_BIN) (ejecuta con gdb)$(RESET)"

# Bateria de pruebas bajo AddressSanitizer + UBSan: detecta fugas de
# memoria, desbordamientos de buffer y comportamiento indefinido.
asan: CFLAGS := $(STD) $(WARNINGS) -O1 -g -fsanitize=address,undefined \
                 -fno-omit-frame-pointer -I$(INCDIR)
asan: LDFLAGS := -fsanitize=address,undefined
asan: clean $(TEST_BIN)
	@echo "$(BOLD)[SAN]$(RESET)  Ejecutando pruebas con ASan + UBSan"
	@$(TEST_BIN)

# Arranque rapido: make run ROM=ruta/a/la_rom.ch8
run: all
	@if [ -z "$(ROM)" ]; then \
		echo "Uso: make run ROM=ruta/a/la_rom.ch8"; exit 1; \
	fi
	@./$(EMU_BIN) $(ROM)

# Limpieza de todos los artefactos generados.
clean:
	@echo "$(BOLD)[RM ]$(RESET)  Artefactos de compilacion"
	@rm -rf $(BUILDDIR) $(EMU_BIN) test_rom_fixture.ch8
	@rm -f *.o *.out

# Reconstruccion completa.
rebuild: clean all

# Ayuda integrada.
help:
	@echo "$(BOLD)Emulador CHIP-8 en C puro - objetivos disponibles:$(RESET)"
	@echo "  make            Compila el emulador (./$(EMU_BIN))"
	@echo "  make test       Compila y ejecuta las pruebas unitarias"
	@echo "  make debug      Compila con simbolos de depuracion"
	@echo "  make asan       Ejecuta las pruebas con ASan + UBSan"
	@echo "  make run ROM=x  Compila y arranca el emulador con la ROM x"
	@echo "  make rebuild    Limpia y recompila todo"
	@echo "  make clean      Borra binarios y objetos"

# Compilador e Flags
CC = gcc
# -Wall -Wextra: Avisos de código
# -pthread: Suporte a threads
# -g: Informação de debug (útil para valgrind)
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -lrt -pthread

# Nome do executável final
TARGET = server

# Lista de ficheiros fonte (.c)
# Nota: Assume que criaste o src/main.c conforme o passo anterior
SRCS = src/main.c \
       src/master.c \
       src/worker.c \
       src/thread_pool.c \
       src/shared_mem.c \
       src/semaphores.c \
       src/http.c \
       src/config.c \
       src/logger.c

# Transforma a lista de .c em .o
OBJS = $(SRCS:.c=.o)

# --- REGRAS PRINCIPAIS ---

# 'make' ou 'make all' compila tudo
all: $(TARGET)

# Cria o executável linkando os objetos
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compila cada ficheiro .c em .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- REGRAS EXTRA (Utilitários do PDF) ---

# Compila e corre o servidor
run: all
	./$(TARGET)

# Limpa ficheiros compilados
clean:
	rm -f $(OBJS) $(TARGET)

# Compilação otimizada para entrega final (Release)
release: CFLAGS += -O3
release: clean all

# --- TESTES DE MEMÓRIA E CONCORRÊNCIA ---

# Verifica memory leaks
valgrind: all
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all --log-file=valgrind.log ./$(TARGET)

# Verifica Race Conditions (Helgrind)
helgrind: all
	valgrind --tool=helgrind --log-file=helgrind.log ./$(TARGET)

.PHONY: all clean run release valgrind helgrind
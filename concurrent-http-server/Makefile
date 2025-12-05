# Compilador e Flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -lrt -pthread

# Nome do executável final
TARGET = server

# LISTA DE TODOS OS FICHEIROS FONTE (.c)
# Certifica-te que src/cache.c e src/ipc.c estão aqui!
SRCS = src/main.c \
       src/master.c \
       src/worker.c \
       src/thread_pool.c \
       src/shared_mem.c \
       src/semaphores.c \
       src/http.c \
       src/config.c \
       src/logger.c \
       src/stats.c \
       src/ipc.c \
       src/cache.c

# Transforma a lista de .c em .o
OBJS = $(SRCS:.c=.o)

# --- REGRAS ---

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Utilitários
run: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

release: CFLAGS += -O3
release: clean all

valgrind: all
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all --log-file=valgrind.log ./$(TARGET)

helgrind: all
	valgrind --tool=helgrind --log-file=helgrind.log ./$(TARGET)

.PHONY: all clean run release valgrind helgrind
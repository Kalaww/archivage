CC=gcc
CFLAGS=-Wall -pedantic -std=c99 -D _POSIX_C_SOURCE=200112L
EXEC=mytar
SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: $(EXEC)

default: all

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -rf *.o

cleanall: clean
	rm -rf $(EXEC)

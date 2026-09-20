CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2 -Iinclude
TARGET = fidelis
SOURCES = src/main.c src/lexer.c
HEADERS = include/lexer.h

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

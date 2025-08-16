CC = clang
CFLAGS = -Wall -Wextra -std=c99 -O2
SDL_CFLAGS = $(shell pkg-config --cflags sdl3 2>/dev/null || echo "-I/usr/local/include/SDL3")
SDL_LIBS = $(shell pkg-config --libs sdl3 2>/dev/null || echo "-lSDL3")
LIBS = $(SDL_LIBS) -lm

TARGET = clock
SOURCE = clock.c

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: clean
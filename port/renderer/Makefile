CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS  := -lz

all: render_scene render_sprites

render_scene: render_scene.o gba_ppu.o png_write.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

render_sprites: render_sprites.o gba_ppu.o png_write.o sprite_engine.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

render_scene.o:   render_scene.c gba_ppu.h png_write.h
render_sprites.o: render_sprites.c gba_ppu.h png_write.h sprite_engine.h
gba_ppu.o:        gba_ppu.c gba_ppu.h
png_write.o:      png_write.c png_write.h
sprite_engine.o:  sprite_engine.c sprite_engine.h

clean:
	rm -f render_scene render_sprites *.o

.PHONY: all clean

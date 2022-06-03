CC = gcc
CFLAGS = -Wall -ansi -pedantic -O2 -fomit-frame-pointer -g

all: zextest runz80

tables.h: maketables.c
	$(CC) -Wall $< -o maketables
	./maketables > $@

z80emu.o: z80emu.c z80emu.h z80config.h z80user.h \
	instructions.h macros.h tables.h
	$(CC) $(CFLAGS) -c $<

zextest.o: zextest.c zextest.h z80emu.h z80config.h
	$(CC) -Wall -c $<

runz80.o: runz80.c runz80.h z80emu.h z80config.h
	$(CC) -Wall -c $<

OBJECT_FILES = zextest.o z80emu.o
OBJECT_FILES2 = runz80.o z80emu.o

zextest: $(OBJECT_FILES)
	$(CC) $(OBJECT_FILES) -o $@

runz80: $(OBJECT_FILES2)
	$(CC) $(OBJECT_FILES2) -o $@

clean:
	rm -f *.o runz80 zextest maketables

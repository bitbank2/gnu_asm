CFLAGS=-c -Wall -O2
LIBS= -lm

all: gnu_asm emuio.o

gnu_asm: main.o emuio.o
	$(CC) main.o emuio.o $(LIBS) -o gnu_asm

main.o: main.c
	$(CC) $(CFLAGS) main.c

emuio.o: emuio.c
	$(CC) $(CFLAGS) emuio.c

clean:
	rm -rf *o gnu_asm


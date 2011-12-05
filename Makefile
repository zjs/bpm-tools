-include .config

CFLAGS += -Wall
LDLIBS += -lm

bpm:	bpm.o

clean:
	rm -f bpm *.o

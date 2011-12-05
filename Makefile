-include .config

CFLAGS += -Wall -MMD
LDLIBS += -lm

bpm:	bpm.o

clean:
	rm -f bpm *.o *.d

-include *.d

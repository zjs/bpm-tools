-include .config

CFLAGS += -Wall -MMD
LDLIBS += -lm

tempo:	tempo.o

clean:
	rm -f tempo *.o *.d

-include *.d

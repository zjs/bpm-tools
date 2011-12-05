CFLAGS += -Wall -MMD

tempo:	tempo.o

clean:
	rm -f tempo *.o *.d

-include *.d

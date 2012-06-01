-include .config

CFLAGS += -Wall
LDLIBS += -lm

.PHONY:	clean dist

bpm:	bpm.o

clean:
	rm -f bpm *.o

dist:
	mkdir -p dist
	V=$$(git describe) && git archive --prefix=bpm-tools-$$V/ HEAD \
		| gzip > dist/bpm-tools-$$V.tar.gz

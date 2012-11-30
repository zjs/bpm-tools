-include .config

PREFIX ?= /usr/local
INSTALL ?= install

BINDIR ?= $(PREFIX)/bin

CFLAGS += -Wall
LDLIBS += -lm

.PHONY:	clean install dist

bpm:	bpm.o

clean:
	rm -f bpm *.o

install:
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -t $(DESTDIR)$(BINDIR) bpm bpm-graph bpm-tag

dist:
	mkdir -p dist
	V=$$(git describe) && git archive --prefix=bpm-tools-$$V/ HEAD \
		| gzip > dist/bpm-tools-$$V.tar.gz

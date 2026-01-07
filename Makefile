PREFIX = /usr/local

all:
	cc -O3 -o fterm *.c `pkg-config --libs --cflags gtk+-3.0 vte-2.91 x11`

install: all
	mkdir -p $(PREFIX)/bin
	install -s fterm $(PREFIX)/bin

uninstall:
	rm $(PREFIX)/bin/fterm

.PHONY: all install uninstall

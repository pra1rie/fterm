BINDIR = /usr/local/bin

all:
	cc -O3 -o fterm *.c `pkg-config --libs --cflags gtk+-3.0 vte-2.91 x11`

install: all
	mkdir -p ${BINDIR}
	install -s fterm ${BINDIR}

uninstall:
	rm ${BINDIR}/fterm

.PHONY: all install uninstall

all:
	cc -O3 -o fterm *.c `pkg-config --libs --cflags gtk+-3.0 vte-2.91 x11`

install: all
	install fterm /usr/local/bin
	install fterm.cfg ${HOME}/.config/fterm/

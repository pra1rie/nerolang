all:
	tcc -O2 *.c -o nero

install: all
	install nero /usr/local/bin

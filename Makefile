all:
	$(CC) -O2 nero.c -o nero -lgc

install: all
	install -s nero /usr/local/bin

uninstall: # why would u wanna do that? :c
	rm /usr/local/bin/nero


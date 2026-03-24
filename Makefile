CFLAGS = -Wall -Werror

all:
	$(CC) -O2 nero.c -o nero -lgc $(CFLAGS)

install: all
	install -s nero /usr/local/bin

uninstall: # why would u wanna do that? :c
	rm /usr/local/bin/nero


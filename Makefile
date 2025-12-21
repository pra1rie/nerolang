CC = tcc

all:
	${CC} -O2 *.c -o nero -Wall -Werror

install: all
	install -s nero /usr/local/bin

uninstall: # why would u wanna do that? :c
	rm /usr/local/bin/nero


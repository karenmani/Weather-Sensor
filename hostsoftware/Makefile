CC=gcc
CFLAGS= -Werror -Wextra -Wall -pedantic -std=c99 -g -O0

host: host.o
host.o: host.c *.h

clean:
	$(RM) *.o host *.bin

run: host
	./host

debug: host
	gdb host

CFLAGS=-Wall -Werror -Os -I/usr/local/include
LDFLAGS=-g -O2 -L/usr/local/lib
ocli: ocli.o json.o
	$(CC) $(LDFLAGS) -o ocli ocli.o json.o -lmosquitto -lgps -lm

json.o: json.c json.h
ocli.o: json.h ocli.c utarray.h

clean:
	rm -f *.o

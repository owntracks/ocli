CFLAGS=-Wall -Werror -Os 
LDFLAGS=-Os
ocli: ocli.o json.o
	$(CC) $(LDFLAGS) -o ocli ocli.o json.o -lmosquitto -lgps -lm

json.o: json.c json.h
ocli.o: json.h ocli.c utarray.h

clean:
	rm -f *.o

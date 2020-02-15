CFLAGS+=-Wall -Werror -Os -I/usr/local/include
LDFLAGS=-L /usr/local/lib -lmosquitto -lgps -lm

OCLI = owntracks-cli-publisher
BINDIR = /usr/local/bin
MANDIR  = /usr/local/man

OBJS = json.o

all: $(OCLI)

$(OCLI): $(OCLI).o $(OBJS)
	$(CC) -o $(OCLI) $(OCLI).o $(OBJS) $(LDFLAGS)

json.o: json.c json.h
$(OCLI).o: json.h $(OCLI).c utarray.h utstring.h version.h

$(OCLI).pdf: $(OCLI).1
	groff -Tps -man $(OCLI).1 > $(OCLI).tmp_ && pstopdf -i $(OCLI).tmp_ -o $(OCLI).pdf && rm -f $(OCLI).tmp_

install: $(OCLI) $(OCLI).1
	install -d $(DESTDIR)$(BINDIR)
	install -m755 $(OCLI) $(DESTDIR)$(BINDIR)/$(OCLI)
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m644 $(OCLI).1 $(DESTDIR)$(MANDIR)/man1/$(OCLI).1

clean:
	rm -f *.o

clobber: clean
	rm -f $(OCLI)

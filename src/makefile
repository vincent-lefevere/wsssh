CC ?= gcc 
INSTALL ?= install 
RM ?= rm 

CFLAGS += $(shell mariadb_config --cflags)
CFLAGS += $(shell pkg-config --cflags libxml-2.0)

LDFLAGS += $(shell mariadb_config --libs)
LDFLAGS += $(shell pkg-config --libs libxml-2.0)
LDFLAGS += -lwebsockets
LDFLAGS += -lz -lm -lrt -lpthread

BINDIR ?= usr/local/sbin 


all: wsssh

wsssh: wsssh.o makefile
	$(CC) $(CFLAGS) -o wsssh wsssh.o $(LDFLAGS)
	
wsssh.o: wsssh.c makefile
	$(CC) $(CFLAGS) -o wsssh.o -c wsssh.c
	
install: wsssh
	$(INSTALL) -m 755 wsssh $(DESTDIR)/$(BINDIR)

clean:
	$(RM) -f wsssh.o wsssh


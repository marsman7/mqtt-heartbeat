# https://www.selflinux.org/selflinux/html/make02.html

CC       = gcc
CFLAGS   = -O2 -Wall
LIBS     = -lconfig -lmosquitto
INCS     = 
OBJECTS  = 
PREFIX	 = ./test/foo/bar
BINDIR   = $(PREFIX)/usr/local/sbin/
CONFDIR  = $(PREFIX)/etc/
SERVICEDIR   = `pkg-config --variable=systemdsystemunitdir systemd`\
NAME = mqtt-heartbeat


all: mqtt-heartbeat.o
	$(CC) -o bin/mqtt-heartbeat bin/mqtt-heartbeat.o $(LIBS) $(CFLAGS) -fdiagnostics-color=always

mqtt-heartbeat.o: mqtt-heartbeat.c
	$(CC) -c mqtt-heartbeat.c -o bin/mqtt-heartbeat.o $(INCS) $(CFLAGS)

#%.o: %.c
#	$(CC) -c $<

.PHONY: clean
clean:
	rm -f bin/*

.PHONY: install
install: all
# /usr/local/sbin/foobar 
# /lib/systemd/system/foobar.service
# /etc/foobar.conf
	install -D -m 755 -o root bin/mqtt-heartbeat $(BINDIR)mqtt-heartbeat
	install -D -m 644 -o root mqtt-heartbeat.example.conf $(CONFDIR)mqtt-heartbeat.conf
	install -D -m 644 mqtt-heartbeat.service $(SERVICEDIR)mqtt-heartbeat.service

.PHONY: uninstall
uninstall:
	rm -f $(BINDIR)mqtt-heartbeat
	rm -f $(CONFDIR)mqtt-heartbeat.conf
	rm -f $(SERVICEDIR)mqtt-heartbeat.service
	echo $(SERVICEDIR)

install-strip:
# Installation mit ge-"strip"-ten Programmen (strip entfernt die Symboltabelle aus einem Programm) 


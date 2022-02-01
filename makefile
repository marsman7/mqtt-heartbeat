# https://www.selflinux.org/selflinux/html/make02.html

NAME     = mqtt-heartbeat

CC       = gcc
CFLAGS   = -O2 -Wall
LIBS     = 
INCS     = 
#C_FILES   = foo.c bar.c
C_FILES  = mqtt-heartbeat.c
OBJECTS  = $(C_FILES:.c=.o)
SRCDIR   = src/
DSTDIR   = bin/
PREFIX	 = ./test/foo/bar
BINDIR   = /usr/local/sbin/
CONFDIR  = /etc/
SERVICEDIR = /etc/systemd/system/
# SERVICEDIR = `pkg-config --variable=systemdsystemunitdir systemd`/

.PHONY: all
all: $(OBJECTS)
	$(CC) -o $(DSTDIR)$(NAME) $(DSTDIR)$< $(LIBS) $(CFLAGS) -fdiagnostics-color=always

%.o: $(SRCDIR)%.c
	mkdir -p $(DSTDIR)
	$(CC) -c $< -o $(DSTDIR)$@ $(INCS) $(CFLAGS)

.PHONY: clean
clean:
#	@ rm -f $(DSTDIR)*
# remove all files under DSTDIR exept README.md
	find $(DSTDIR) ! -name 'README.md' -type f -exec rm -f {} +

.PHONY: install
install: 
	@ echo "Daemon install  : $(BINDIR)$(NAME)"
	@ install -D -m 755 $(DSTDIR)$(NAME) $(BINDIR)$(NAME)
	@ echo "Service install : $(SERVICEDIR)$(NAME).service"
	@ install -D -m 644 $(SRCDIR)systemd.service $(SERVICEDIR)$(NAME).service
	@ echo "Config install  : $(CONFDIR)$(NAME).conf"
	@ install -D -m 644 $(SRCDIR)$(NAME).example.conf $(CONFDIR)$(NAME).conf
	@ echo 
	@ echo "Start the daemon : sudo systemctl start $(NAME)"
	@ echo "Stop the daemon  : sudo systemctl stop $(NAME)"
	@ echo "Status of daemon : sudo systemctl status $(NAME)"

.PHONY: fakeinstall
fakeinstall: 
	@ echo "Daemon install to : $(PREFIX)$(BINDIR)$(NAME)"
	install -D $(DSTDIR)$(NAME) $(PREFIX)$(BINDIR)$(NAME)
	@ echo "Service install to : $(PREFIX)$(SERVICEDIR)$(NAME)"
	install -D $(SRCDIR)systemd.service $(PREFIX)$(SERVICEDIR)$(NAME).service
	@ echo "Config install  : $(PREFIX)$(CONFDIR)$(NAME).conf"
	install -D $(SRCDIR)$(NAME).example.conf $(PREFIX)$(CONFDIR)$(NAME).conf
	tree $(PREFIX)

.PHONY: uninstall
uninstall:
	@ echo "Daemon remove : $(BINDIR)$(NAME)"
	- rm -f $(BINDIR)$(NAME)
	@ echo "Service remove : $(SERVICEDIR)$(NAME).service"
	- rm -f $(SERVICEDIR)$(NAME).service
	@ echo "Config remove : $(CONFDIR)$(NAME).conf"
	- rm -f $(CONFDIR)$(NAME).conf

install-strip:
# Installation mit ge-"strip"-ten Programmen (strip entfernt die Symboltabelle aus einem Programm) 

.PHONY: html
html:
	cmark README.md > bin/readme.html

.PHONY: help
help:
	@ echo "Help"
	@ echo ""
	@ echo "make "
	@ echo "make clean"
	@ echo "make install"
	@ echo "make uninstall"
	@ echo "make fakeinstall"
	@ echo "make html"

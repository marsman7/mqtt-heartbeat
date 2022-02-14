#/*******************************************/ /**
# * @file makefile for mqtt-heartbeat
# * @author marsman7 (you@domain.com)
# * @brief MQTT-Heartbeat is a Linux daemon that 
# *        periodically sends a status message via MQTT.
# * @date 2022-02-02
# * 
# * @copyright Copyright (c) 2022
# * 
# * https://www.selflinux.org/selflinux/html/make02.html
# * 
# ***********************************************/

NAME     = mqtt-heartbeat

CC         = gcc
LDFLAGS    = -O2 -Wall
LIBS       = -lconfig -lmosquitto
INCS       = 
#C_FILES    = foo.c bar.c
C_FILES    = mqtt-heartbeat.c
OBJECTS    = $(C_FILES:.c=.o)
SRCDIR     = src/
DSTDIR     = bin/
DOCDIR     = doc/
PREFIX	   = ./test/foo/bar
BINDIR     = /usr/local/sbin/
CFGDIR     = /etc/
SERVICEDIR = /etc/systemd/system/
# SERVICEDIR = `pkg-config --variable=systemdsystemunitdir systemd`/
VERSION_FILE = ./version
VERSION_NUM  = `cat $(VERSION_FILE)`
CFLAGS     = -DVERSION_STR=\"$(VERSION_NUM)\"
DOXY_CONF  = doxyfile.conf

GREEN    = \033[0;32m
RED      = \033[1;31m
COLOR_RESET = \033[0m

.PHONY: all
all: $(OBJECTS)
	$(CC) -o $(DSTDIR)$(NAME) $(DSTDIR)$< $(LIBS) $(LDFLAGS) -fdiagnostics-color=always
	@ echo "$(GREEN)----- Builded Version : $(VERSION_NUM) -----$(COLOR_RESET)"

%.o: $(SRCDIR)%.c increment_build
	@ mkdir -p $(DSTDIR)
	$(CC) -c $< -o $(DSTDIR)$@ $(INCS) $(CFLAGS)

.PHONY: clean
clean:
# remove all files under DSTDIR exept README.md
	find $(DSTDIR) ! -name 'README.md' -type f -exec rm -f {} +

.PHONY: install
install: 
	@ echo "Daemon install  : $(BINDIR)$(NAME)"
	@ install -pD -m 755 $(DSTDIR)$(NAME) $(BINDIR)$(NAME)
	@ echo "Service install : $(SERVICEDIR)$(NAME).service"
	@ install -pD -m 644 $(SRCDIR)systemd.service $(SERVICEDIR)$(NAME).service
	@ echo "Config install  : $(CFGDIR)$(NAME).example.conf"
	@ install -D -m 644 $(SRCDIR)$(NAME).example.conf $(CFGDIR)$(NAME).example.conf
	@ systemctl daemon-reload
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
	@ echo "Config install  : $(PREFIX)$(CFGDIR)$(NAME).conf"
	install -D $(SRCDIR)$(NAME).example.conf $(PREFIX)$(CFGDIR)$(NAME).conf
	tree $(PREFIX)

.PHONY: uninstall
uninstall:
	@ echo "Daemon remove : $(BINDIR)$(NAME)"
	- rm -f $(BINDIR)$(NAME)
	@ echo "Service remove : $(SERVICEDIR)$(NAME).service"
	- rm -f $(SERVICEDIR)$(NAME).service
	@ echo "Config remove : $(CFGDIR)$(NAME).conf"
	- rm -f $(CFGDIR)$(NAME).conf

install-strip:
# Installation mit ge-"strip"-ten Programmen (strip entfernt die Symboltabelle aus einem Programm) 

.PHONY: deb
deb:
# Make a Debian package

.PHONY: increment_build
increment_build:
	@if ! test -f $(VERSION_FILE); then echo "0.0.0.0" > $(VERSION_FILE); fi
	@ ver=`cat $(VERSION_FILE)` && major=$${ver%.*} && build=$$(($${ver##*.} + 1)) \
			&& echo "$${major}.$${build}" > $(VERSION_FILE)

.PHONY: doc
doc:
	rm -r $(DOCDIR)html
# the following command must be in a line with a && separator else
# the command run not in the right directory
	export PROJECT_NUMBER=$(VERSION_NUM) && cd $(DOCDIR) && doxygen $(DOXY_CONF)

.PHONY: help
help:
	@ echo "Help"
	@ echo ""
	@ echo "make "
	@ echo "make clean"
	@ echo "make install"
	@ echo "make uninstall"
	@ echo "make fakeinstall"
	@ echo "make doc"
	@ echo "make help"

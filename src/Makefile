TARGETS	= $(OBJDIR)../include/VPN.h $(OBJDIR)VPN$(EXEEXT)
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
BINDIR	= $(PREFIX)/bin
SBINDIR	= $(PREFIX)/sbin
CC	= cc
CPPFLAGSF=
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags libApp`
CFLAGS	= -Wall -g -O2 -pedantic
LDFLAGSF= `pkg-config --libs libApp` -Wl,--export-dynamic 
LDFLAGS	= 
EXEEXT	=
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

$(OBJDIR)../include/VPN.h: ../data/VPN.interface
	../data/appbroker.sh -P "$(PREFIX)" -- "$(OBJDIR)../include/VPN.h"

VPN_OBJS = $(OBJDIR)vpn.o $(OBJDIR)main.o
VPN_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
VPN_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)VPN$(EXEEXT): $(VPN_OBJS)
	$(CC) -o $(OBJDIR)VPN$(EXEEXT) $(VPN_OBJS) $(VPN_LDFLAGS)

$(OBJDIR)vpn.o: vpn.c ../include/VPN.h
	$(CC) $(VPN_CFLAGS) -o $(OBJDIR)vpn.o -c vpn.c

$(OBJDIR)main.o: main.c
	$(CC) $(VPN_CFLAGS) -o $(OBJDIR)main.o -c main.c

clean:
	$(RM) -- $(../include/VPN.h_OBJS) $(VPN_OBJS)
	../data/appbroker.sh -c -P "$(PREFIX)" -- "../include/VPN.h"

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)VPN$(EXEEXT) $(DESTDIR)$(BINDIR)/VPN$(EXEEXT)

uninstall:
	$(RM) -- $(DESTDIR)$(BINDIR)/VPN$(EXEEXT)

.PHONY: all clean distclean install uninstall

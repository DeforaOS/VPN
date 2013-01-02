TARGETS	= VPN.h
PREFIX	= /usr/local
DESTDIR	=
RM	?= rm -f
LN	?= ln -f
MKDIR	?= mkdir -p
INSTALL	?= install


all: $(TARGETS)

VPN.h: ../data/VPN.interface
	../data/appbroker.sh -P "$(PREFIX)" -- "VPN.h"

clean:
	$(RM) -- $(VPN.h_OBJS)

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)

uninstall:

.PHONY: all clean distclean install uninstall

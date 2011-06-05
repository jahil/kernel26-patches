# $Id: Makefile,v 1.1.2.1 2007/04/09 12:50:36 mikpe Exp $

SHELL=/bin/sh

default:

install:
	if [ -n "$(ETCDIR)" ]; then $(MAKE) install-etc; fi

install-etc:
	-mkdir -p $(ETCDIR)/udev.d/rules.d/
	cp perfctr.rules $(ETCDIR)/udev.d/rules.d/99-perfctr.rules
	chmod 644 $(ETCDIR)/udev.d/rules.d/99-perfctr.rules
	-mkdir -p $(ETCDIR)/rc.d/init.d/
	cp perfctr.rc $(ETCDIR)/rc.d/init.d/perfctr
	chmod 755 $(ETCDIR)/rc.d/init.d/perfctr
	if [ "$(ETCDIR)" = "/etc" ]; then /sbin/chkconfig --add perfctr; fi

distclean realclean clean:

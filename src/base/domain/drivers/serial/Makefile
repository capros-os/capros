default: package

CROSS_BUILD=yes
EROS_SRC=../../../..

include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/serial
OPTIM=-O
OBJECTS=$(BUILDDIR)/serial.o
INC=-I$(EROS_ROOT)/include

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/serial: $(OBJECTS) $(DOMCRT0) $(DOMLIB)
	$(DOMLINK) $(DOMLINKOPT) $(DOMCRT0) $(OBJECTS) -lsmall $(DOMLIB) -o $@

install: all
	$(INSTALL) -d $(EROS_ROOT)/domain/drivers
	$(INSTALL) -m 644 serial.map $(EROS_ROOT)/domain/drivers
	$(INSTALL) -m 755 $(TARGETS) $(EROS_ROOT)/domain/drivers

-include $(BUILDDIR)/.*.m

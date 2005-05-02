EROS_SRC=../../../../../..

include $(EROS_SRC)/build/make/makevars.mk

HEADERS=$(wildcard *.h)

include $(EROS_SRC)/build/make/makerules.mk

install:
	$(INSTALL) -d $(EROS_ROOT)/include/domain/drivers
	$(INSTALL) -m 644 $(HEADERS) $(EROS_ROOT)/include/domain/drivers
	@for i in $(DIRS); do \
		if [ -d "$$i" ]; then \
			$(MAKE) -C $$i $(MAKERULES) install; \
			if [ $$? -ne 0 ]; then \
				echo "*** RECURSIVE BUILD STOPS ***"; \
				exit 1; \
			fi; \
		fi; \
	done

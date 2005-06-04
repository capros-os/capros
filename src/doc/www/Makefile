#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
#
# This file is part of the EROS Operating System.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

default: package
EROS_SRC=../..
include $(EROS_SRC)/build/make/makevars.mk

DOCS=capros.html

DIRS=project assurance img design-notes dionysix devel essays legal faq CSS plan

include $(EROS_SRC)/build/make/makerules.mk

install: local-install

local-install:
	$(INSTALL) -d $(EROS_ROOT)/doc
	$(INSTALL) -d $(EROS_ROOT)/doc/www
	$(INSTALL) -h -m 0644 $(DOCS) $(EROS_ROOT)/doc/www
	@if [ ! -e $(EROS_ROOT)/doc/www/index.html ]; then\
		echo "(cd $(EROS_ROOT)/doc/www;ln -s capros.html index.html)";\
		(cd $(EROS_ROOT)/doc/www;ln -s capros.html index.html);\
	fi

# This target updates the public www tree, and should only be run by shap.
# The weird dependency structure is to ensure ordering.

#publish-eros: install
#	(cd $(EROS_ROOT)/doc; tar czf www.tgz www/)
#	(cd $(EROS_ROOT)/doc; scp www.tgz linc:/ftp/pub/eros/)
#	-(cd $(EROS_ROOT)/doc; rm -f www.tgz)
#	ssh linc '(cd /ftp/pub/eros/; gzcat www.tgz | tar xf -; rm -f www.tgz)'
#	ssh linc '(cd /ftp/pub/eros; mv public_html old_html; mv www public_html; rm -rf old_html)'
#

publish-linc: install
	sed 's@</[Hh][Ee][Aa][Dd]@<BASE href="http://www.eros-os.org/eros.html">&@' $(EROS_ROOT)/doc/www/eros.html > eros-with-base.html
	scp eros-with-base.html linc:/ftp/pub/eros/public_html/eros.html
	-rm -f eros-with-base.html

publish: publish-linc

-include DEPEND

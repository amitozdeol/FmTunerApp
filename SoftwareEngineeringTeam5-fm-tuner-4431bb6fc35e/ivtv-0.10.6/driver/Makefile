# By default, the build is done against the running kernel version.
# to build against a different kernel version, set KVER
#
#  make KVER=2.6.11-alpha
#
#  Alternatively, set KDIR
#
#  make KDIR=/usr/src/linux

MDIR := kernel/drivers/media/video/ivtv

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

all:: ivtv-svnversion.h

ifneq ($(KERNELRELEASE),)
include $(src)/Kbuild
else

all::
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

install:: all
	$(MAKE) INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=$(MDIR) \
		-C $(KDIR) M=$(CURDIR) modules_install

clean::
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

endif

install::
	/sbin/depmod -a

clean::
	rm -f ivtv-svnversion.h ivtv-svnversion.h.tmp

ivtv-svnversion.h:
	@if [ -f ivtv-svnrelease.h ]; then \
		echo '#include "ivtv-svnrelease.h"' > $@.tmp; \
	elif [ -d .svn ]; then \
		echo '#define IVTV_DRIVER_VERSION_COMMENT' \
		  '"(development revision' \
		  "`svnversion -nc . | sed -e s/^[^:]*://`"')"' > $@.tmp; \
	else date +'#define IVTV_DRIVER_VERSION_COMMENT "(development snapshot compiled on %c)"' > $@.tmp; fi
	@(chmod 666 $@.tmp 2> /dev/null || /bin/true)
	@cmp -s $@ $@.tmp || (mv $@.tmp $@ ; echo created $@)

.PHONY: ivtv-svnversion.h

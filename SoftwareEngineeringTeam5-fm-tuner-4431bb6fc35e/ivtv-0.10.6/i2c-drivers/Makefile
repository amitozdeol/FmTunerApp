# By default, the build is done against the running kernel version.
# to build against a different kernel version, set KVER
#
#  make KVER=2.6.11-alpha
#
#  Alternatively, set KDIR
#
#  make KDIR=/usr/src/linux

MDIR := kernel/drivers/media/video

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

ifneq ($(KERNELRELEASE),)
include $(src)/Kbuild
else

all::
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

install:: all
	$(MAKE) INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=$(MDIR) \
		-C $(KDIR) M=$(CURDIR) modules_install
	/sbin/depmod -a

clean::
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

endif

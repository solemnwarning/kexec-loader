# kexec-loader - Makefile
# Copyright (C) 2007-2010 Daniel Collins <solemnwarning@solemnwarning.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# This should be "vN.N" releases
#
VERSION=r$(shell git rev-parse --short HEAD)

EXTERN_DOWNLOAD := extern/download
EXTERN_BUILD := extern/build

# kexec-tools
KT_VER := 2.0.25
KT_URL := http://horms.net/projects/kexec/kexec-tools/kexec-tools-$(KT_VER).tar.gz
KT_CONFIGURE := --without-lzma

# util-linux
UL_VER := 2.38.1
UL_URL := https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v2.38/util-linux-$(UL_VER).tar.gz
UL_CONFIGURE :=

CC := gcc
LD := ld
CFLAGS := -Wall -DVERSION=\"$(VERSION)\" -D_GNU_SOURCE -O0
INCLUDES := -I$(EXTERN_BUILD)/util-linux-$(UL_VER)/libblkid/src/
LIBS := -lz -llzmadec

FLOPPY ?= floppy.img
ISOLINUX ?= /usr/share/syslinux/isolinux.bin
ISO ?= cdrom.iso

ifdef HOST
	CC := $(HOST)-gcc
	LD := $(HOST)-ld
	
	KT_CONFIGURE += --host=$(HOST) CC=$(CC)
	UL_CONFIGURE += --host=$(HOST) CC=$(CC) CXX=$(HOST)-g++ LD=$(LD)
endif

KEXEC_A := $(EXTERN_BUILD)/kexec-tools-$(KT_VER)/kexec.a

UTIL_LINUX_CONFIGURED := $(EXTERN_BUILD)/util-linux-$(UL_VER)/.configure-done
LIBBLKID_A := $(EXTERN_BUILD)/util-linux-$(UL_VER)/.libs/libblkid.a
LIBUUID_A  := $(EXTERN_BUILD)/util-linux-$(UL_VER)/.libs/libuuid.a

OBJS := src/misc.o src/disk.o src/console.o src/menu.o src/modprobe.o \
	src/boot.o src/grub.o src/shell.o src/globcmp.o src/keymap.o src/tar.o \
	src/vfs.o src/trace.o $(KEXEC_A) $(LIBBLKID_A) $(LIBUUID_A)

all: kexec-loader kexec-loader.static

clean:
	rm -f src/*.o
	rm -f kexec-loader kexec-loader.static
	rm -rf $(EXTERN_BUILD)/kexec-tools-$(KT_VER)/
	rm -rf $(EXTERN_BUILD)/util-linux-$(UL_VER)/
	rm -f initrd.img $(FLOPPY) $(ISO)
	rm -rf iso-files/ iso-modules/

distclean: clean
	rm -f $(EXTERN_DOWNLOAD)/kexec-tools-$(KT_VER).tar.gz
	rm -f $(EXTERN_DOWNLOAD)/util-linux-$(UL_VER).tar.gz

kexec-loader: $(OBJS)
	$(CC) $(CFLAGS) -o kexec-loader $(OBJS) $(LIBS)

floppy: syslinux.cfg initrd.img
ifeq ($(KERNEL),)
	@echo "Please set KERNEL to the Linux kernel binary"
	@exit 1
endif
	mformat -i $(FLOPPY) -C -f 1440 -v kexecloader ::
	mmd -i $(FLOPPY) ::/syslinux
	syslinux -d /syslinux $(FLOPPY)
	mcopy -i $(FLOPPY) syslinux.cfg ::/syslinux
	mcopy -i $(FLOPPY) initrd.img ::/
	mcopy -i $(FLOPPY) README.html ::/
	mcopy -i $(FLOPPY) kexec-loader.conf ::/
	mmd -i $(FLOPPY) ::/modules
	mcopy -i $(FLOPPY) $(KERNEL) ::/vmlinuz
ifneq ($(KCONFIG),)
	mcopy -i $(FLOPPY) $(KCONFIG) ::/linux.cfg
endif

syslinux.cfg:
	echo "DEFAULT /vmlinuz initrd=/initrd.img" > syslinux.cfg

initrd.img: kexec-loader.static
	./mkinitramfs.sh initrd.img

cdrom: iso-files iso-modules iso-files/isolinux/initrd.img iso-files/isolinux/vmlinuz
	./mkiso.sh $(ISO)

cdrom-export: iso-files iso-modules iso-files/isolinux/initrd.img iso-files/isolinux/vmlinuz
	mkdir kexec-loader-$(VERSION)-cdrom
	cp -a iso-files iso-modules kexec-loader-$(VERSION)-cdrom
	cp -a mkiso.sh addmod.sh kexec-loader-$(VERSION)-cdrom
	sed -e "s/\$$VERSION/$(VERSION)/g" cdrom-readme.html > kexec-loader-$(VERSION)-cdrom/readme.html
	tar -cf cdrom.tar kexec-loader-$(VERSION)-cdrom
	rm -rf kexec-loader-$(VERSION)-cdrom

iso-modules:
	mkdir -p iso-modules

iso-files: $(ISOLINUX)
	mkdir -p iso-files/isolinux
	cp $(ISOLINUX) iso-files/isolinux/
	echo "DEFAULT vmlinuz initrd=initrd.img" > iso-files/isolinux/isolinux.cfg
	cp kexec-loader.conf iso-files/

iso-files/isolinux/vmlinuz: $(KERNEL)
ifeq ($(KERNEL),)
	@echo "Please set KERNEL to the Linux kernel binary"
	@exit 1
endif
ifneq ($(KCONFIG),)
	cp $(KCONFIG) iso-files/linux.cfg
endif
	cp $(KERNEL) iso-files/isolinux/vmlinuz

iso-files/isolinux/initrd.img: iso-files initrd.img
	cp initrd.img iso-files/isolinux/

kexec-loader.static: $(OBJS)
	$(CC) $(CFLAGS) -static -o kexec-loader.static $(OBJS) $(LIBS)

%.o: %.c $(UTIL_LINUX_CONFIGURED)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(KEXEC_A):
	mkdir -p $(EXTERN_DOWNLOAD) $(EXTERN_BUILD)
	test -e $(EXTERN_DOWNLOAD)/$(notdir $(KT_URL)) || wget -O $(EXTERN_DOWNLOAD)/$(notdir $(KT_URL)) $(KT_URL)
	tar -C $(EXTERN_BUILD) -xf $(EXTERN_DOWNLOAD)/$(notdir $(KT_URL))
	patch -p1 -d $(EXTERN_BUILD)/kexec-tools-$(KT_VER)/ < patches/kexec-tools-$(KT_VER).diff
	cd $(EXTERN_BUILD)/kexec-tools-$(KT_VER)/ && ./configure $(KT_CONFIGURE)
	$(MAKE) -C $(EXTERN_BUILD)/kexec-tools-$(KT_VER)/

$(UTIL_LINUX_CONFIGURED):
	mkdir -p $(EXTERN_DOWNLOAD) $(EXTERN_BUILD)
	test -e $(EXTERN_DOWNLOAD)/$(notdir $(UL_URL)) || wget -O $(EXTERN_DOWNLOAD)/$(notdir $(UL_URL)) $(UL_URL)
	tar -C $(EXTERN_BUILD) -xf $(EXTERN_DOWNLOAD)/$(notdir $(UL_URL))
	cd $(EXTERN_BUILD)/util-linux-$(UL_VER)/ && ./configure $(UL_CONFIGURE)
	touch $@

$(LIBBLKID_A): $(UTIL_LINUX_CONFIGURED)
	$(MAKE) -C $(EXTERN_BUILD)/util-linux-$(UL_VER)/ libblkid.la

$(LIBUUID_A):
	$(MAKE) -C $(EXTERN_BUILD)/util-linux-$(UL_VER)/ libuuid.la

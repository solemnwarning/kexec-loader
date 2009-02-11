# kexec-loader - Source makefile
# Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
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

DEPS := misc.o disk.o console.o menu.o modprobe.o boot.o grub.o shell.o globcmp.o kexec.a libblkid.a libuuid.a
OBJS := $(DEPS)

KT_VER := 2.0.0
KT_URL := http://www.kernel.org/pub/linux/kernel/people/horms/kexec-tools/kexec-tools-$(KT_VER).tar.gz
KT_CONFIGURE :=

E2FS_VER := 1.41.3
E2FS_URL := http://surfnet.dl.sourceforge.net/sourceforge/e2fsprogs/e2fsprogs-$(E2FS_VER).tar.gz
E2FS_CONFIGURE :=

INCLUDES += -Ie2fsprogs-$(E2FS_VER)/lib/

ifdef HOST
	KT_CONFIGURE += --host=$(HOST)
	E2FS_CONFIGURE += --host=$(HOST)
endif

all: kexec-loader

clean:
	rm -f kexec-loader kexec-loader.static
	rm -f *.o kexec.a lib{blkid,uuid}.a
	
	rm -rf kexec-tools-$(KT_VER)/
	rm -f kexec-tools-$(KT_VER).spec
	rm -rf e2fsprogs-$(E2FS_VER)/

distclean: clean
	rm -f kexec-tools-$(KT_VER).tar.gz
	rm -f e2fsprogs-$(E2FS_VER).tar.gz

kexec-loader: $(DEPS)
	$(CC) $(CFLAGS) -o kexec-loader $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) -static -o kexec-loader.static $(OBJS) $(LIBS)

misc.o: misc.c misc.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o misc.o misc.c

disk.o: disk.c disk.h misc.h libblkid.a
	$(CC) $(CFLAGS) $(INCLUDES) -c -o disk.o disk.c

console.o: console.c console.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o console.o console.c

menu.o: menu.c menu.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o menu.o menu.c

modprobe.o: modprobe.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o modprobe.o modprobe.c

boot.o: boot.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o boot.o boot.c

grub.o: grub.c grub.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o grub.o grub.c

shell.o: shell.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o shell.o shell.c

globcmp.o: globcmp.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o globcmp.o globcmp.c

kexec.a:
	wget -nc $(KT_URL)
	tar -xzf kexec-tools-$(KT_VER).tar.gz
	cd kexec-tools-$(KT_VER)/ && \
	patch -Np1 -i $(KLBASE)/patches/kexec-tools-2.0.0.diff && \
	./configure $(KT_CONFIGURE) && \
	make

libuuid.a: libblkid.a
libblkid.a:
	wget -nc $(E2FS_URL)
	tar -xzf e2fsprogs-$(E2FS_VER).tar.gz
	cd e2fsprogs-$(E2FS_VER)/ && \
	CFLAGS="" ./configure $(E2FS_CONFIGURE)
	make -C e2fsprogs-$(E2FS_VER)/lib/blkid/
	make -C e2fsprogs-$(E2FS_VER)/lib/uuid/
	cp e2fsprogs-$(E2FS_VER)/lib/lib{blkid,uuid}.a ./

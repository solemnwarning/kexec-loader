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

DEPS := misc.o
OBJS := $(DEPS)

KT_VER := 2.0.0
KT_URL := http://www.kernel.org/pub/linux/kernel/people/horms/kexec-tools/kexec-tools-$(KT_VER).tar.gz
KT_CONFIGURE :=

ifdef HOST
	KT_CONFIGURE += --host=$(HOST)
endif

all: kexec-loader

clean:
	rm -f *.o kexec-loader kexec-loader.static
	rm -f kexec.a kexec_libs
	
	rm -rf kexec-tools-$(KT_VER)/
	rm -f kexec-tools-$(KT_VER).spec

distclean: clean
	rm -f kexec-tools-$(KT_VER).tar.gz

kexec-loader: $(DEPS)
	$(CC) $(CFLAGS) $(LIBS) -o kexec-loader $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) -static -o kexec-loader.static $(OBJS)

misc.o: misc.c misc.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o misc.o misc.c

kexec.a:
	wget -nc $(KT_URL)
	tar -xzf kexec-tools-$(KT_VER).tar.gz
	cd kexec-tools-$(KT_VER)/ && \
	patch -Np1 -i $(KLBASE)/patches/kexec-tools-2.0.0.diff && \
	./configure $(KT_CONFIGURE) && \
	make

# kexec-loader - Source makefile
# Copyright (C) 2007, Daniel Collins <solemnwarning@solemnwarning.net>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#	* Redistributions of source code must retain the above copyright
#	  notice, this list of conditions and the following disclaimer.
#
#	* Redistributions in binary form must reproduce the above copyright
#	  notice, this list of conditions and the following disclaimer in the
#	  documentation and/or other materials provided with the distribution.
#
#	* Neither the name of the software author nor the names of any
#	  contributors may be used to endorse or promote products derived from
#	  this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE SOFTWARE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL THE SOFTWARE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

DEPS := main.o mount.o config.o console.o misc.o kexec.o grub.o shell.o mystring.o elf.o kexec.a
OBJS := $(DEPS) `cat kexec_libs`

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

main.o: main.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o main.o main.c

mount.o: mount.c mount.h misc.o
	$(CC) $(CFLAGS) $(INCLUDES) -c -o mount.o mount.c

config.o: config.c config.h misc.o
	$(CC) $(CFLAGS) $(INCLUDES) -c -o config.o config.c

console.o: console.c console.h misc.o
	$(CC) $(CFLAGS) $(INCLUDES) -c -o console.o console.c

misc.o: misc.c misc.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o misc.o misc.c

kexec.o: kexec.c kexec.h misc.o
	$(CC) $(CFLAGS) $(INCLUDES) -c -o kexec.o kexec.c

grub.o: grub.c grub.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o grub.o grub.c

shell.o: shell.c shell.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o shell.o shell.c

mystring.o: mystring.c mystring.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o mystring.o mystring.c

elf.o: elf.c elf.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o elf.o elf.c

kexec.a:
	wget -nc $(KT_URL)
	tar -xzf kexec-tools-$(KT_VER).tar.gz
	cd kexec-tools-$(KT_VER)/ && \
	patch -Np1 -i $(KLBASE)/patches/kexec-tools-2.0.0.diff && \
	./configure $(KT_CONFIGURE) && \
	make

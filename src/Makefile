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

all: kexec-loader

clean:
	rm -f *.o kexec-loader
	rm -f kexec/*.o kexec_libs kexec_build

kexec-loader: main.o mount.o config.o console.o misc.o kexec.o
	$(CC) $(CFLAGS) $(LIBS) -o kexec-loader main.o mount.o config.o \
		console.o misc.o kexec.o kexec/*.o `cat kexec_libs`

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

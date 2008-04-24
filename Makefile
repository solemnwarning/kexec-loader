# kexec-loader - Root makefile
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

# This should be "vN.N" releases
#
VERSION=r$(shell svn info -r HEAD | grep 'Revision:' | sed -e 's/Revision: //')

export CC=i386-linux-uclibc-gcc
export CFLAGS=-Wall -static -DVERSION=\"$(VERSION)\"
export INCLUDES=
export LIBS=
export HOST=i386-linux-uclibc
export KLBASE=$(PWD)

KEXEC_TOOLS_VER=1.101
KEXEC_TOOLS_URL=http://www.xmission.com/~ebiederm/files/kexec/kexec-tools-$(KEXEC_TOOLS_VER).tar.gz

all: kexec
	@$(MAKE) -C src/

clean:
	@$(MAKE) -C src/ clean
	rm -rf kexec-tools-1.101/

check:
	@$(MAKE) -C src/ clean
	@$(MAKE) -C src/

kexec:
	wget -nc $(KEXEC_TOOLS_URL)
	tar -xzf kexec-tools-$(KEXEC_TOOLS_VER).tar.gz
	if [ ! -f src/kexec_build ]; then \
	cd kexec-tools-$(KEXEC_TOOLS_VER)/ && \
	./configure --host=$(HOST) && \
	patch -Np1 -i $(KLBASE)/patches/kexec-tools-1.101-merge.diff && \
	patch -Np1 -i $(KLBASE)/patches/kexec-tools-1.101-mini.diff && \
	make; \
fi

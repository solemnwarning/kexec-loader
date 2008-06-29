# kexec-loader - Root makefile
# Copyright (C) 2007,2008 Daniel Collins <solemnwarning@solemnwarning.net>
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
# VERSION=r$(shell svn info | grep 'Revision:' | sed -e 's/Revision: //')
VERSION=v1.4.1

export CC := gcc
export CFLAGS := -Wall -DVERSION=\"$(VERSION)\"
export INCLUDES :=
export LIBS :=
export KLBASE := $(PWD)

ifdef HOST
	CC := $(HOST)-gcc
endif

TARFILE := kexec-loader-$(shell echo $(VERSION) | sed -e 's/^v//').tar

all:
	@$(MAKE) -C src/

clean:
	@$(MAKE) -C src/ clean

distclean: clean
	@$(MAKE) -C src/ distclean

dist: distclean
	tar --exclude='*.svn' -cpf ../$(TARFILE) -C ../ $(shell basename $(KLBASE))

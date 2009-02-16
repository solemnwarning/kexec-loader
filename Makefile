# kexec-loader - Root makefile
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

# This should be "vN.N" releases
#
# VERSION=r$(shell svn info | grep 'Revision:' | sed -e 's/Revision: //')
VERSION := v2.0

export CC := gcc
export CFLAGS := -Wall -DVERSION=\"$(VERSION)\"
export INCLUDES :=
export LIBS := -lz
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

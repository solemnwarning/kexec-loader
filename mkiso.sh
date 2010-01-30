#!/bin/bash
# Create an ISO image for kexec-loader
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

fail() {
	if [ $# -gt 0 ]
	then
		echo "$*" 1>&2
	fi
	
	rm -rf iso-root/
	exit 1
}

if [ $# -ne 1 ]
then
	fail "Usage: $0 <iso filename>"
fi

if ! which mkisofs > /dev/null
then
	fail "No mkisofs program found in \$PATH"
fi

rm -rf iso-root/
cp -R iso-files iso-root || fail

if [ -d iso-modules ]
then
	modules=`find iso-modules -type f | grep -E '\.(ko|t[gl]z|tar(\.(gz|lzma))?)$'`
	
	if [ -n "$modules" ]
	then
		./addmod.sh iso-root/isolinux/initrd.img $modules || fail
	fi
fi

mkisofs -o $1 -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot \
	-boot-load-size 4 -boot-info-table -R -J -V "kexecloader" iso-root || fail

rm -rf iso-root

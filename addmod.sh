#!/bin/bash
# Add a module to an initramfs
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

if [ -z "$2" ]; then
	echo "Usage: $0 <module> <initramfs>" 1>&2
	exit 1
fi

TMPDIR="/tmp/addmod.$$"
MOD_EXT=`echo "$1" | sed -e 's/.*\.//g'`

mkdir -p "$TMPDIR/modules/"

# I find your lack of else if disturbing
#
if [ $MOD_EXT = 'ko' ]
then
	cat "$1" | gunzip > "$TMPDIR/modules/`basename "$1"`" 2>&1
	
	# If decompression fails, assume module is uncompressed
	#
	if [ $? == 1 ]
	then
		cp "$1" "$TMPDIR/modules/"
	fi
else
	if [ $MOD_EXT = 'tlz' ]
	then
		# Tar is stupid...
		tar --lzma -xf "$1" -C "$TMPDIR/modules/"
	else
		tar -xf "$1" -C "$TMPDIR/modules/"
	fi
fi

cp "$2" "$TMPDIR/initramfs.cpio.lzma"
cd "$TMPDIR"

unlzma "initramfs.cpio.lzma"

for f in `cpio -it --quiet < initramfs.cpio | egrep '^modules\/'`
do
	rm -f "$f"
done

find modules | cpio -o --format=newc --quiet --append -F "initramfs.cpio"
lzma -9 "initramfs.cpio"

cd "$OLDPWD"
cp "$TMPDIR/initramfs.cpio.lzma" "$2"

rm -rf "$TMPDIR"

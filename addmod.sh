#!/bin/bash
# Add modules to an LZMA compressed initramfs
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

tmp="addmod.tmp"
initramfs=""

abort() {
	if [ $# -eq "1" ]
	then
		echo "$1" 1>&2
	fi
	
	rm -rf "$tmp"
	exit 1
}

cprog() {
	if which "$1" > /dev/null; then
		return 0
	fi
	
	abort "No executable '$1' program found in \$PATH"
}

if [ $# -lt 2 ]; then
	abort "Usage: $0 <initramfs> <module> [<module> ...]"
fi

cprog tar
cprog lzcat
cprog find
cprog cpio
cprog lzma

rm -rf "$tmp"
mkdir -p "$tmp/modules/"

for mod in "$@"
do
	if [ -z "$initramfs" ]
	then
		initramfs="$mod"
	else
		is_ko=`echo "$mod" | grep -E '\.ko$'`
		is_tar=`echo "$mod" | grep -E '\.(tar(\.(gz|bz2))?|tgz)$'`
		is_tlz=`echo "$mod" | grep -E '\.(tar\.lzma|tlz)$'`
		
		if [ -n "$is_ko" ]
		then
			cp "$mod" "$tmp/modules/" || abort
		fi
		
		if [ -n "$is_tar" ]
		then
			tar -xf "$mod" -C "$tmp/modules/" || abort
		fi
		
		if [ -n "$is_tlz" ]
		then
			# TAR is really stupid
			tar --lzma -xf "$mod" -C "$tmp/modules/" || abort
		fi
		
		if [ -z "$is_ko" -a -z "$is_tar" -a -z "$is_tlz" ]
		then
			abort "Unknown file extension: $mod"
		fi
	fi
done

lzcat -S "" "$initramfs" > "$tmp/initramfs.cpio" || abort

for f in `cpio -it --quiet < "$tmp/initramfs.cpio" | egrep '^modules\/.+'`
do
	rm -f "$tmp/$f"
done

bash -c "cd \"$tmp\" && find modules -iname '*.ko' | cpio -o --format=newc --quiet --append -F initramfs.cpio" || abort
lzma -9 "$tmp/initramfs.cpio" -c > "$initramfs" || abort

rm -rf "$tmp"

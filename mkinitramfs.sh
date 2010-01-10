#!/bin/bash
# Create an LZMA compressed initramfs for kexec-loader
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

# Check a program is somewhere in $PATH
#
function cprog() {
	if which "$1" > /dev/null; then
		return 0
	fi
	
	echo "No executable '$1' program found in \$PATH" 1>&2
	exit 1
}

cprog whoami
cprog mkdir
cprog install
cprog strip
cprog mknod
cprog find
cprog cpio
cprog rm
cprog lzma

if [ -z "$1" ]; then
	echo "Usage: $0 <outfile.cpio.lzma>" 1>&2
	exit 1
fi

if [ "`whoami`" != "root" ]; then
	cprog fakeroot
	exec fakeroot -- $0 $*
fi

mkdir -p -m 0755 initramfs.tmp/{dev,mnt,proc,modules} || exit 1

install -m 0755 kexec-loader.static initramfs.tmp/init || exit 1
strip -s initramfs.tmp/init || exit 1

mknod -m 0600 initramfs.tmp/dev/console c 5 1 || exit 1
mknod -m 0600 initramfs.tmp/dev/tty1 c 4 1 || exit 1
mknod -m 0600 initramfs.tmp/dev/tty2 c 4 2 || exit 1
mknod -m 0600 initramfs.tmp/dev/tty3 c 4 3 || exit 1
mknod -m 0600 initramfs.tmp/dev/tty4 c 4 4 || exit 1
mknod -m 0600 initramfs.tmp/dev/tty5 c 4 5 || exit 1
mknod -m 0600 initramfs.tmp/dev/tty6 c 4 6 || exit 1

mknod -m 0600 initramfs.tmp/dev/ttyS0 c 4 64 || exit 1
mknod -m 0600 initramfs.tmp/dev/ttyS1 c 4 65 || exit 1
mknod -m 0600 initramfs.tmp/dev/ttyS2 c 4 66 || exit 1
mknod -m 0600 initramfs.tmp/dev/ttyS3 c 4 67 || exit 1

bash -c 'cd initramfs.tmp && find | cpio --create --format=newc --quiet' | lzma -9 > "$1" || exit 1

rm -rf initramfs.tmp

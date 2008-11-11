#!/bin/bash
# Create an initramfs for kexec-loader
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
cprog ln
cprog strip
cprog mknod
cprog find
cprog cpio
cprog rm

if [ -z "$1" ]; then
	echo "Usage: $0 <outfile.cpio>" 1>&2
	exit 1
fi

if [ "`whoami`" != "root" ]; then
	cprog fakeroot
	exec fakeroot -- $0 $1 $2
fi

initramfs="initramfs.tmp"
rdir="$PWD"

if echo "$1" | grep -e "\/.*/" > /dev/null; then
	initramfs_cpio="$1"
else
	initramfs_cpio="$PWD/$1"
fi

echo "Creating $initramfs tree..."
mkdir -p -m 0755 "$initramfs/"{dev,mnt/target,proc,sbin} || exit 1

echo "Copying programs..."
install -m 0755 src/kexec-loader.static "$initramfs/init" || exit 1

echo "Stripping symbols..."
strip -s "$initramfs/init"

echo "Creating devices..."
mknod -m 0600 "$initramfs/dev/console" c 5 1 || exit 1
mknod -m 0600 "$initramfs/dev/tty1" c 4 1 || exit 1
mknod -m 0600 "$initramfs/dev/tty2" c 4 2 || exit 1
mknod -m 0600 "$initramfs/dev/tty3" c 4 3 || exit 1
mknod -m 0600 "$initramfs/dev/tty4" c 4 4 || exit 1
mknod -m 0600 "$initramfs/dev/tty5" c 4 5 || exit 1
mknod -m 0600 "$initramfs/dev/tty6" c 4 6 || exit 1

mknod -m 0600 "$initramfs/dev/ttyS0" c 4 64 || exit 1
mknod -m 0600 "$initramfs/dev/ttyS1" c 4 65 || exit 1
mknod -m 0600 "$initramfs/dev/ttyS2" c 4 66 || exit 1
mknod -m 0600 "$initramfs/dev/ttyS3" c 4 67 || exit 1

echo "Creating $initramfs_cpio..."
cd "$initramfs" && find | cpio --create --format=newc --quiet > "$initramfs_cpio" || exit 1
cd "$rdir"

echo "Deleting $initramfs..."
rm -rf "$initramfs"

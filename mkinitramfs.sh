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

# Create /dev/hdX and /dev/hdX[0-16] devices
#
function create_hdx() {
	mknod -m 0600 $1 b $2 $3 || exit 1
	
	part=1
	while [ "$part" -le "16" ]; do
		mknod -m 0600 $1$part b $2 $[$3+$part] || exit 1
		
		part=$[$part+1]
	done
}

# Create /dev/sdX and /dev/sdX[0-15] devices
#
function create_sdx() {
	base=$[$2*16]
	mknod -m 0600 $1 b 8 $base
	
	part=1
	while [ "$part" -le "15" ]; do
		mknod -m 0600 $1$part b 8 $[$base+$part] || exit 1
		
		part=$[$part+1]
	done
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

if [ -z "$1" -o -z "$2" ]; then
	echo "Usage: $0 <outfile.cpio> <kexec binary>" 1>&2
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
mkdir -p -m 0755 "$initramfs/"{dev,boot,proc,sbin,target} || exit 1

echo "Copying programs..."
install -m 0755 src/kexec-loader "$initramfs/sbin/kexec-loader" || exit 1
ln -sf "sbin/kexec-loader" "$initramfs/init" || exit 1
install -m 0755 "$2" "$initramfs/sbin/kexec" || exit 1

echo "Stripping symbols..."
strip -s "$initramfs/sbin/kexec-loader"
strip -s "$initramfs/sbin/kexec"

echo "Creating devices..."
mknod -m 0600 "$initramfs/dev/console" c 5 1 || exit 1
mknod -m 0600 "$initramfs/dev/ttyS0" c 4 64 || exit 1
mknod -m 0600 "$initramfs/dev/ttyS1" c 4 65 || exit 1
mknod -m 0600 "$initramfs/dev/ttyS2" c 4 66 || exit 1
mknod -m 0600 "$initramfs/dev/ttyS3" c 4 67 || exit 1

mknod -m 0600 "$initramfs/dev/fd0" b 2 0 || exit 1
mknod -m 0600 "$initramfs/dev/fd1" b 2 1 || exit 1

create_hdx "$initramfs/dev/hda" 3 0
create_hdx "$initramfs/dev/hdb" 3 64
create_hdx "$initramfs/dev/hdc" 22 0
create_hdx "$initramfs/dev/hdd" 22 64
create_hdx "$initramfs/dev/hde" 33 0
create_hdx "$initramfs/dev/hdf" 33 64

create_sdx "$initramfs/dev/sda" 0
create_sdx "$initramfs/dev/sdb" 1
create_sdx "$initramfs/dev/sdc" 2
create_sdx "$initramfs/dev/sdd" 3
create_sdx "$initramfs/dev/sde" 4
create_sdx "$initramfs/dev/sdf" 5
create_sdx "$initramfs/dev/sdg" 6
create_sdx "$initramfs/dev/sdh" 7

echo "Creating $initramfs_cpio..."
cd "$initramfs" && find | cpio --create --format=newc --quiet > "$initramfs_cpio" || exit 1
cd "$rdir"

echo "Deleting $initramfs..."
rm -rf "$initramfs"

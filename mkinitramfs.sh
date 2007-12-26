#!/bin/bash
# Create an initramfs for kexec-loader
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

if [ -z "$1" ]; then
	echo "Usage: $0 <target directory>" 1>&2
	exit 1
fi

install -m 0755 -d "$1"/{dev,boot,proc} || exit 1
install -m 0755 src/kexec-loader "$1/init"

if [ ! -e "$1/dev/console" ]; then
	mknod -m 0600 "$1/dev/console" c 5 1 || exit 1
fi
if [ ! -e "$1/dev/ttyS0" ]; then
	mknod -m 0600 "$1/dev/ttyS0" c 4 64 || exit 1
fi
if [ ! -e "$1/dev/ttyS1" ]; then
	mknod -m 0600 "$1/dev/ttyS1" c 4 65 || exit 1
fi
if [ ! -e "$1/dev/ttyS2" ]; then
	mknod -m 0600 "$1/dev/ttyS2" c 4 66 || exit 1
fi
if [ ! -e "$1/dev/ttyS3" ]; then
	mknod -m 0600 "$1/dev/ttyS3" c 4 67 || exit 1
fi
if [ ! -e "$1/dev/fd0" ]; then
	mknod -m 0600 "$1/dev/fd0" b 2 0 || exit 1
fi
if [ ! -e "$1/dev/fd1" ]; then
	mknod -m 0600 "$1/dev/fd1" b 2 1 || exit 1
fi
if [ ! -e "$1/dev/sda" ]; then
	mknod -m 0600 "$1/dev/sda" b 8 0 || exit 1
fi
if [ ! -e "$1/dev/sdb" ]; then
	mknod -m 0600 "$1/dev/sdb" b 8 16 || exit 1
fi
if [ ! -e "$1/dev/sdc" ]; then
	mknod -m 0600 "$1/dev/sdc" b 8 32 || exit 1
fi
if [ ! -e "$1/dev/sdd" ]; then
	mknod -m 0600 "$1/dev/sdd" b 8 48 || exit 1
fi

echo "Created initramfs in $1"
echo "Be sure to set the full path of $1 in CONFIG_INITRAMFS_SOURCE"
echo ""
echo "Device Drivers -> Block devices -> Initramfs source file(s)"

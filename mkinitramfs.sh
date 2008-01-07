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

MAKEDEV="`pwd`/MAKEDEV"
if [ ! -f "$MAKEDEV" ]; then
	echo "Can't find MAKEDEV" 1>&2
	exit 1
fi

install -m 0755 -d "$1"/{dev,boot,proc,sbin} || exit 1
install -m 0755 src/kexec-loader "$1/sbin/kexec-loader"
ln -sf "sbin/kexec-loader" "$1/init"

cd "$1/dev"
$MAKEDEV consoleonly
$MAKEDEV ttyS{0,1,2,3}
$MAKEDEV fd{0,1}
$MAKEDEV sd{a,b,c,d,e,f,g,h}
$MAKEDEV hd{a,b,c,d,e,f,g,h}

echo "Created initramfs in $1"
echo "Be sure to set the full path of $1 in CONFIG_INITRAMFS_SOURCE"
echo ""
echo "Device Drivers -> Block devices -> Initramfs source file(s)"

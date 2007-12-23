/* kexec-loader - initramfs /init program
 * Copyright (C) 2007, Daniel Collins <solemnwarning@solemnwarning.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 *	* Neither the name of the software author nor the names of any
 *	  contributors may be used to endorse or promote products derived from
 *	  this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE SOFTWARE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE SOFTWARE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/mount.h>

#define VERSION "1.0"
#define CONFIG_FILE "/etc/klinit.conf"
#define ROOTFS_FILE "/sbin/kexec-loader"
#define PROGNAME "klinit"
#define FSTYPE "vfat"

#define infsleep() while(1) { sleep(9999); }
#define printl(...) printl_r(__LINE__, __VA_ARGS__)
#define fatal(...) printl(__VA_ARGS__); infsleep();

#ifdef DEBUG
#define debug(...) printl(__VA_ARGS__)
#else
#define debug(...)
#endif

static int got_root = 0;

static void try_rootfs(char const* device);
static void printl_r(unsigned int line, char const* fmt, ...);

/* Try to mount 'device' as the root filesystem and check it contains a file
 * named ROOTFS_FILE, if ROOTFS_FILE does not exist unmount it again.
*/
static void try_rootfs(char const* device) {
	if(got_root) {
		return;
	}
	
	debug("Attempting to mount %s as root...", device);
	if(mount(device, "/", FSTYPE, MS_RDONLY, NULL) == -1) {
		debug("Failed to mount %s as root: %s", device, strerror(errno));
		return;
	}
	
	if(access(ROOTFS_FILE, F_OK) == 0) {
		return;
	}
	
	if(umount("/") == -1) {
		fatal("Can't unmount %s: %s", device, strerror(errno));
	}
}

/* Print a message to the console, along with the source line which called the
 * printl() macro.
*/
static void printl_r(unsigned int line, char const* fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	char buf[128] = {'\0'};
	vsnprintf(buf, 127, fmt, argv);
	printf(PROGNAME ":%u: %s\n", line, buf);
	
	va_end(argv);
}

int main(int argc, char** argv) {
	printl("Version " VERSION " starting...\n");
	
	try_rootfs("/dev/fd0");
	try_rootfs("/dev/fd1");
	try_rootfs("/dev/sda");
	try_rootfs("/dev/sdb");
	if(!got_root) {
		fatal("Could not find root filesystem");
	}
	
	infsleep();
	return(1);
}

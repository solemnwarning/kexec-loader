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
#include <stdarg.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "../config.h"

#ifdef DEBUG
#define debug(...) debug_r(__LINE__, __VA_ARGS__)
#else
#define debug(...)
#endif

#define inf_sleep() while(1) { sleep(9999); }
#define eprintf(...) fprintf(stderr, __VA_ARGS__)

static char* argv0 = "init";
static int got_root = 0;

/* Print a debug message to the console
 *
 * Only works if DEBUG is defined, if debug isn't defined the debug message
 * strings will be ommited from the source too.
*/
static void debug_r(unsigned int line, char const* fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	char buf[128] = {'\0'};
	vsnprintf(buf, 127, fmt, argv);
	printf("%s: debug(%s) at line %u\n", argv, buf, line);
	
	va_end(argv);
}

/* Attempt to mount  a device as the new root filesystem
 *
 * The filesystem will only be treated as valid if it contains a kexec-loader
 * program in BINPREFIX
*/
static void mount_root(char const* rdev) {
	if(got_root) {
		return;
	}
	debug("Trying '%s' as root filesystem", rdev);
	
	if(mount(rdev, "/rootfs", NULL, 0, NULL) == -1) {
		debug("Mounting /rootfs failed: %s", strerror(errno));
		return;
	}
	struct stat stdata;
	if(stat("/rootfs/" BINPREFIX "/kexec-loader", &stdata) == -1) {
		debug("Can't stat kexec-loader: %s", strerror(errno));
		goto mount_root_nonexist;
	}
	
	if(syscall(SYS_pivot_root, "/rootfs", "/rootfs/initrd") == -1) {
		eprintf("Can't pivot_root(): %s\n", strerror(errno));
		inf_sleep();
	}
	chdir("/");
	
	got_root = 1;
	return;
	
	mount_root_nonexist:
	if(umount("/rootfs") == -1) {
		eprintf("Can't unmount /rootfs: %s\n", strerror(errno));
		inf_sleep();
	}
}

int main(int argc, char** argv) {
	if(argc >= 1) {
		argv0 = argv[0];
	}
	
	mount_root("/dev/fd0");
	mount_root("/dev/fd1");
	mount_root("/dev/sda");
	mount_root("/dev/sdb");
	if(!got_root) {
		eprintf("Could not find root filesystem\n");
		inf_sleep();
	}
	
	inf_sleep();
	return(0);
}

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
#include <fcntl.h>

#include "../config.h"

#ifdef DEBUG
#define debug(...) debug_r(__LINE__, __VA_ARGS__)
#else
#define debug(...)
#endif

#define inf_sleep() while(1) { sleep(9999); }
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define fatal(...) fatal_r(__LINE__, __VA_ARGS__)
#define allocate(size) allocate_r(__LINE__, size)

static char* argv0 = "init";
static int got_root = 0;
static FILE* devices = NULL;

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
	printf("%s: debug(%s) at line %u\n", argv0, buf, line);
	
	va_end(argv);
}

/* Handle a fatal error
 *
 * Basically outputs the error message and sleeps forever
*/
static void fatal_r(unsigned int line, char const* fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	eprintf("%s: fatal() called at line %u!\n", argv0, line);
	vfprintf(stderr, fmt, argv);
	fflush(stderr);
	
	va_end(argv);
	inf_sleep();
}

/* Error checking malloc() wrapper, also zeros memory */
void* allocate_r(unsigned int line, size_t size) {
	void* ptr = malloc(size);
	if(ptr == NULL) {
		fatal_r(line, "Can't allocate %u bytes: %s", strerror(errno));
	}
	
	memset(ptr, 0, size);
	return(ptr);
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
	
	if(mount(rdev, "/rootfs", "vfat", MS_RDONLY, NULL) == -1) {
		debug("Mounting /rootfs failed: %s", strerror(errno));
		return;
	}
	struct stat stdata;
	if(stat("/rootfs/" BINPREFIX "/kexec-loader", &stdata) == -1) {
		debug("Can't stat kexec-loader: %s", strerror(errno));
		goto mount_root_nonexist;
	}
	
	debug("Found kexec-loader on filesystem '%s'", rdev);
	
	if(syscall(SYS_pivot_root, "/rootfs", "/rootfs/initrd") == -1) {
		fatal("Can't pivot_root(): %s", strerror(errno));
	}
	chdir("/");
	
	if(umount("/initrd") == -1) {
		printf("%s: Can't unmount /initrd: %s\n", argv0, strerror(errno));
	}
	
	got_root = 1;
	return;
	
	mount_root_nonexist:
	if(umount("/rootfs") == -1) {
		fatal("Can't unmount /rootfs: %s", strerror(errno));
	}
}

/* Mount any extra filesystems */
static void mount_extra(void) {
	if(!got_root) {
		fatal("mount_extra() called without mounting root, why?!");
	}
	
	if(mount("proc", "/proc", "proc", 0, NULL) == -1) {
		fatal("Can't mount /proc filesystem: %s", strerror(errno));
	}
	if(mount("tmpfs", "/dev", "tmpfs", 0, "size=16M") == -1) {
		fatal("Can't mount /dev filesystem: %s", strerror(errno));
	}
}

/* Attempt to open the device list DEVICES_FILE
 *
 * If the file is opened sucessfully 1 is returned, zero is returned if the
 * file does not exist and any other error will cause fatal().
 *
 * If the file is already opened, 2 will be returned and no other action will
 * be performed.
*/
static int devices_open(void) {
	if(devices != NULL) {
		return(2);
	}
	
	while(devices == NULL) {
		if((devices = fopen(DEVICES_FILE, "r")) != NULL) {
			break;
		}
		
		if(errno == EINTR) {
			continue;
		}
		if(errno == ENOENT) {
			return(0);
		}
		fatal("Can't open devices file: %s", strerror(errno));
	}
	
	return(1);
}

/* Close the device list file if it's open */
static void devices_close(void) {
	if(devices == NULL) {
		return;
	}
	
	while(fclose(devices) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		fatal("Can't close devices file: %s", strerror(errno));
	}
	devices = NULL;
}

/* Read the next available line from the device list into a buffer that's
 * allocated by allocate()
 *
 * If the file is not open when this is called, it will be opened and reading
 * will begin from the start of the file.
 *
 * If the file does not exist or has no more lines available NULL will be
 * returned, in the event of EOF the file will also be closed.
*/
static char* devices_readline(void) {
	if(!devices_open()) {
		return(NULL);
	}
	
	char* line = allocate(1024);
	if(fgets(line, 1024, devices) == NULL) {
		if(feof(devices)) {
			devices_close();
			
			free(line);
			return(NULL);
		}
		
		fatal("Can't read devices file: %s", strerror(errno));
	}
	
	/* Remove newline and carridge return characters from the end of the
	 * line.
	*/
	size_t end = strlen(line)-1;
	while(line[end] == '\n' || line[end] == '\r') {
		line[end] = '\0';
		end--;
	}
	
	return(line);
}

/* Create any devices listed in the devices.conf specified by DEVICES_FILE
 *
 * If DEVICES_FILE cannot be opened create_devices() will return without
 * doing anything
*/
static void create_devices(void) {
	char* line = NULL;
	char filename[512] = {'\0'};
	
	while((line = devices_readline()) != NULL) {
		char type = '\0';
		int major = -1;
		int minor = -1;
		int step = 0;
		
		char* token = strtok(line, "\t ");
		while(token != NULL) {
			if(step == 0) {
				snprintf(filename, 511, "/dev/%s", token);
			}
			if(step == 1) {
				type = token[0];
			}
			if(step == 2) {
				major = atoi(token);
			}
			if(step == 3) {
				minor = atoi(token);
			}
			step++;
			
			token = strtok(NULL, "\t ");
		}
		
		if(type != 'b' && type != 'c') {
			printf("Invalid device type: %c\n", type);
			goto create_devices_eloop;
		}
		if(major < 0 || major > 255) {
			printf("Invalid device major: %d\n", major);
			goto create_devices_eloop;
		}
		if(minor < 0 || minor > 255) {
			printf("Invalid device minor: %d\n", minor);
			goto create_devices_eloop;
		}
		
		if(type == 'b' && mknod(filename, 0600 | S_IFBLK, makedev(major,minor) == -1)) {
			printf("Can't create block device %s: %s\n", filename, strerror(errno));
		}
		if(type == 'c' && mknod(filename, 0600 | S_IFCHR, makedev(major,minor) == -1)) {
			printf("Can't create character device %s: %s\n", filename, strerror(errno));
		}
		
		create_devices_eloop:
		free(line);
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
		fatal("Could not find root filesystem, sorry :(");
	}
	
	mount_extra();
	create_devices();
	
	inf_sleep();
	return(0);
}

/* kexec-loader - Misc. functions
 * Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/mount.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/klog.h>

#include "misc.h"

#define KLOG_TTY "/dev/tty2"
#define DEBUG_TTY "/dev/tty3"

static void redirect_klog(void);

int main(int argc, char **argv) {
	if(mount("none", "/proc", "proc", 0, NULL)) {
		die("Error mounting /proc: %s", strerror(errno));
	}
	
	redirect_klog();
	
	return 0;
}

/* Print a message to the debug log/tty */
void debug(char const *fmt, ...) {
	static FILE *debug_fh = NULL;
	va_list argv;
	char msgbuf[256];
	
	if(!debug_fh) {
		char *path = get_cmdline("debug_tty");
		debug_fh = fopen(path ? path : DEBUG_TTY, "a");
		free(path);
		
		if(!debug_fh) {
			return;
		}
	}
	
	va_start(argv, fmt);
	vsnprintf(msgbuf, 256, fmt, argv);
	va_end(argv);
	
	fprintf(debug_fh, "%s\n", msgbuf);
	fflush(debug_fh);
}

/* Disable kernel messages to the console and spawn a process that writes all
 * kernel messages to KLOG_TTY
*/
static void redirect_klog(void) {
	debug("Disabling printk() to console...");
	klogctl(6, NULL, 0);
	
	FILE *klog_fh = fopen(KLOG_TTY, "a");
	if(!klog_fh) {
		debug("Error opening " KLOG_TTY ": %s", strerror(errno));
		return;
	}
	
	FILE *kmsg_fh = fopen("/proc/kmsg", "r");
	if(!kmsg_fh) {
		debug("Error opening /proc/kmsg: %s", strerror(errno));
		return;
	}
	
	if(fork()) {
		return;
	}
	
	char buf[256];
	while(fgets(buf, 256, kmsg_fh)) {
		fputs(buf, klog_fh);
	}
	
	if(ferror(kmsg_fh)) {
		debug("Error reading /proc/kmsg: %s", strerror(errno));
	}
	
	debug("The /proc/kmsg read loop has exited!");
	exit(0);
}

/* Log error and idle (Exiting will cause panic in older kernels) */
void die(char const *fmt, ...) {
	va_list argv;
	char msgbuf[256];
	
	va_start(argv, fmt);
	vsnprintf(msgbuf, 256, fmt, argv);
	va_end(argv);
	
	debug("FATAL: %s\n", msgbuf);
	
	printf("\nFATAL: %s", msgbuf);
	fflush(stdout);
	
	while(1) {
		sleep(9999);
	}
}

/* Search the kernel command line for an option */
char *get_cmdline(char const *name) {
	FILE *fh = fopen("/proc/cmdline", "r");
	if(!fh) {
		debug("Error opening /proc/cmdline: %s", strerror(errno));
		return NULL;
	}
	
	int len = strlen(name);
	char *r = NULL, buf[1024];
	
	if(fgets(buf, 1024, fh)) {
		r = buf;
		
		while((r = strstr(r, name))) {
			if(r == buf || r[-1] == ' ') {
				if(r[len] == '=') {
					r += len+1;
					break;
				}else if(r[len] == ' ' || r[len] == '\0') {
					r += len;
					break;
				}
			}
			
			r++;
		}
		
		if(r) {
			r = kl_strndup(r, strcspn(r, " "));
		}
	}
	
	if(ferror(fh)) {
		debug("Error reading /proc/cmdline: %s", strerror(errno));
	}
	
	fclose(fh);
	
	return r;
}

/* Allocate memory */
void *kl_malloc(size_t size) {
	void *ptr = malloc(size);
	if(!ptr) {
		die("Out of memory! (Tried to allocate %u)", size);
	}
	
	memset(ptr, 0, size);
	return ptr;
}

/* Duplicate a string */
char *kl_strndup(char const *src, int max) {
	int len = 0;
	while(src[len] && len < max) { len++; }
	
	char *dest = kl_malloc(len+1);
	strcpy(dest, src);
	
	return dest;
}

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
		debug_fh = fopen(DEBUG_TTY, "a");
		
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

/* kexec-loader - Misc. functions
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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>

#include "misc.h"
#include "config.h"
#include "../config.h"
#include "console.h"
#include "mystring.h"

/* Error checking malloc() wrapper, also zeros memory */
void* allocate_r(char const* file, unsigned int line, size_t size) {
	void* ptr = malloc(size);
	if(ptr == NULL) {
		fatal("Can't allocate %u bytes: %s", strerror(errno));
	}
	
	memset(ptr, 0, size);
	return(ptr);
}

/* Fatal error encountered, abort! */
void fatal(char const* fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	char buf[256];
	vsnprintf(buf, 256, fmt, argv);
	
	console_setpos(1, 1);
	printf("%c[2J", 0x1B);
	printf("FATAL: %s\n", buf);
	
	debug("FATAL: %s\n", buf);
	va_end(argv);
	
	while(1) {
		sleep(9999);
	}
}

/* Write a message to the debug console */
void debug(char const* fmt, ...) {
	static FILE *debug_fh = NULL;
	
	if(!debug_fh) {
		char *filename = get_cmdline("kexec_debug");
		if(!filename) {
			filename = "/dev/tty2";
		}
		
		if((debug_fh = fopen(filename, "a")) == NULL) {
			free(filename);
			return;
		}
		
		free(filename);
	}
	
	va_list argv;
	va_start(argv, fmt);
	
	vfprintf(debug_fh, fmt, argv);
	fflush(debug_fh);
	
	va_end(argv);
}

/* Search for an option that was passed to Linux via /proc/cmdline
 * Returns the =value if found, or NULL on error/not found
*/
char *get_cmdline(char const *name) {
	FILE *fh;
	char *tok, *val;
	char cmdline[1024];
	size_t len;
	
	if(!(fh = fopen("/proc/cmdline", "r"))) {
		return NULL;
	}
	
	if(!fgets(cmdline, 1024, fh)) {
		return NULL;
	}
	cmdline[strcspn(cmdline, "\n")] = '\0';
	
	fclose(fh);
	
	tok = strtok(cmdline, " ");
	while(tok) {
		if(strchr(tok, '=')) {
			len = (size_t)(strchr(tok, '=') - tok);
		}else{
			len = strlen(tok);
		}
		
		if(strncmp(tok, name, len) == 0) {
			break;
		}
		
		tok = strtok(NULL, " ");
	}
	
	if(tok) {
		if((val = strchr(tok, '='))) {
			return str_copy(val+1, -1);
		}
		
		return str_copy("", -1);
	}
	
	return NULL;
}

/* Fork a process to write Linux kernel messages to the debug console */
void kmsg_monitor(void) {
	if(fork() <= 0) {
		return;
	}
	
	FILE *kmsg = fopen("/proc/kmsg", "r");
	if(!kmsg) {
		debug("Can't open /proc/kmsg: %s\n", strerror(errno));
		debug("Linux kernel messages will not be available\n");
		
		exit(1);
	}
	
	char msgbuf[4096];
	while(fgets(msgbuf, 4096, kmsg)) {
		debug("%s", msgbuf);
	}
	
	exit(0);
}

/* Free a list of kl_target structures */
void free_targets(kl_target *targets) {
	kl_target *dptr;
	
	while(targets) {
		dptr = targets;
		targets = targets->next;
		
		free_mounts(dptr->mounts);
		free(dptr);
	}
}

/* Free a list of kl_mount structures */
void free_mounts(kl_mount *mounts) {
	kl_mount *dptr;
	
	while(mounts) {
		dptr = mounts;
		mounts = mounts->next;
		
		free(dptr);
	}
}

void free_modules(char **modules, int modcount) {
	while(modcount > 0) {
		free(modules[--modcount]);
		modules[modcount] = NULL;
	}
}

/* kexec-loader - Console functions
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
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/klog.h>
#include <stdarg.h>

#include "../config.h"
#include "misc.h"
#include "console.h"

static int alert = 0;

int fgcolour = CONS_WHITE;
int bgcolour = CONS_BLACK;

/* Initialize console(s) */
void console_init(void) {
	debug("Disabling printk() to console...\n");
	klogctl(6, NULL, 0);
	
	struct termios attribs;
	while(tcgetattr(fileno(stdin), &attribs) == -1) {
		if(errno == EINTR) {
			continue;
		}
		
		fatal("Can't get stdin attributes: %s", strerror(errno));
	}
	
	attribs.c_lflag &= ~ICANON;
	attribs.c_lflag &= ~ECHO;
	
	while(tcsetattr(fileno(stdin), TCSANOW, &attribs) == -1) {
		if(errno == EINTR) {
			continue;
		}
		
		fatal("Can't set stdin attributes: %s", strerror(errno));
	}
	
	if(setvbuf(stdout, NULL, _IONBF, 0) != 0) {
		debug("Can't set stdout buffer: %s\n", strerror(errno));
	}
	if(setvbuf(stderr, NULL, _IONBF, 0) != 0) {
		debug("Can't set stderr buffer: %s\n", strerror(errno));
	}
}

/* Set cursor position */
void console_setpos(int row, int column) {
	printf("%c[%d;%dH", 0x1B, row, column);
}

/* Clear the console */
void console_clear(void) {
	if(alert) {
		printf("\nPress any key to continue...\a");
		getchar();
	}
	
	printf("%c[2J", 0x1B);
	alert = 0;
}

/* Set foreground (text) colour */
void console_fgcolour(int colour) {
	printf("%c[;%dm", 0x1B, colour);
	fgcolour = colour;
}

/* Set background colour */
void console_bgcolour(int colour) {
	printf("%c[;;%dm", 0x1B, colour+10);
	bgcolour = colour;
}

/* Set console attributes */
void console_attrib(int attrib) {
	printf("%c[%dm", 0x1B, attrib);
}

/* Get size of console */
void console_getsize(int* rows, int* cols) {
	struct winsize cons_size;
	if(ioctl(fileno(stdout), TIOCGWINSZ, &cons_size) == -1) {
		debug("Can't ioctl(TIOCGWINSZ): %s\n", strerror(errno));
		return;
	}
	
	*rows = cons_size.ws_row;
	*cols = cons_size.ws_col;
}

/* Erase the current line */
void console_eline(char const* mode) {
	printf("%c[%sK", 0x1B, mode);
}

/* Move the cursor back N columns */
void console_cback(int n) {
	printf("%c[%dD", 0x1B, n);
}

/* Print output */
void print2(int flags, int colour, int level, char const *fmt, ...) {
	int colour2 = fgcolour;
	if(colour) {
		console_fgcolour(colour);
	}
	
	va_list argv;
	va_start(argv, fmt);
	
	size_t alen = (level > 0 ? level+1 : 0);
	
	char msg[vsnprintf(NULL, 0, fmt, argv)+alen];
	va_end(argv);
	va_start(argv, fmt);
	vsprintf(msg+alen, fmt, argv);
	
	va_end(argv);
	
	if(level > 0) {
		memset(msg, '>', level);
		msg[level] = ' ';
	}
	
	if(flags & P2_DEBUG) {
		debug("%s\n", msg);
	}
	
	if(flags & P2_ALERT) {
		alert = 1;
	}
	
	puts(msg);
	
	if(colour) {
		console_fgcolour(colour2);
	}
}

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

#include "../config.h"
#include "misc.h"

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
	if(printm_called) {
		printf("Press any key to continue...\n");
		getchar();
		
		printm_called = 0;
	}
	
	printf("%c[2J", 0x1B);
}

/* Set foreground (text) colour */
void console_fgcolour(int colour) {
	printf("%c[;%dm", 0x1B, colour);
}

/* Set background colour */
void console_bgcolour(int colour) {
	printf("%c[;;%dm", 0x1B, colour+10);
}

/* Set console attributes */
void console_attrib(int attrib) {
	printf("%c[%dm", 0x1B, attrib);
}

/* Get size of console */
void console_getsize(unsigned int* rows, unsigned int* cols) {
	struct winsize cons_size;
	if(ioctl(fileno(stdout), TIOCGWINSZ, &cons_size) == -1) {
		debug("Can't ioctl(TIOCGWINSZ): %s\n", strerror(errno));
	}
	
	*rows = cons_size.ws_row;
	*cols = cons_size.ws_col;
}

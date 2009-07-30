/* kexec-loader - Console functions
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <stdarg.h>

#include "misc.h"
#include "console.h"

#define GETC_SIZE 256

#define GETC_PUSH(x) \
	if(getc_count < GETC_SIZE) { \
		getc_buf[getc_count++] = x; \
	}

int alert = 0;

int console_cols = 80;
int console_rows = 24;

static int getc_count = 0;
static int getc_buf[GETC_SIZE];

/* Initialize console(s) */
void console_init(void) {
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	
	struct termios attribs;
	if(tcgetattr(fileno(stdin), &attribs) == -1) {
		debug("Error fetching stdin attributes: %s", strerror(errno));
		return;
	}
	
	attribs.c_lflag &= ~ICANON;
	attribs.c_lflag &= ~ECHO;
	
	if(tcsetattr(fileno(stdin), TCSANOW, &attribs) == -1) {
		debug("Error setting stdin attributes: %s", strerror(errno));
	}
	
	console_getsize(&console_cols, &console_rows);
	debug("Detected console size: %dx%d", console_cols, console_rows);
}

/* Set cursor position */
void console_setpos(int col, int row) {
	printf("%c[%d;%dH", 0x1B, row+1, col+1);
}

/* Fetch the cursor position */
void console_getpos(int *cptr, int *rptr) {
	char tbuf[32];
	int row, col, ts, i, c;
	
	printf("%c[6n", 0x1B);
	
	while(1) {
		c = getchar();
		
		if(c == 0x1B) {
			if((c = getchar()) == '[') {
				for(ts = 0; c != 'R' && ts < 31; ts++) {
					c = getchar();
					
					if(c == 0x1B) {
						ungetc(0x1B, stdin);
						goto NEXT;
					}
					
					tbuf[ts] = c;
					tbuf[ts+1] = '\0';
				}
				
				if(sscanf(tbuf, "%d;%dR", &row, &col) == 2) {
					break;
				}
				
				NEXT:
				
				GETC_PUSH(0x1B);
				GETC_PUSH('[');
				
				for(i = 0; i < ts; i++) {
					GETC_PUSH(tbuf[i]);
				}
			}else{
				GETC_PUSH(0x1B);
				GETC_PUSH(c);
			}
		}else{
			GETC_PUSH(c);
		}
	}
	
	if(rptr) { *rptr = row-1; }
	if(cptr) { *cptr = col-1; }
}

/* Clear the console */
void console_clear(void) {
	if(alert) {
		printf("\nPress any key to continue...\a");
		console_getchar();
	}
	
	console_erase(ERASE_ALL);
	alert = 0;
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
void console_getsize(int* cols_p, int* rows_p) {
	int old = -1, rows, cols;
	int row, col;
	
	console_getpos(&col, &row);
	rows = row;
	cols = col;
	
	while(rows+cols != old) {
		printf("%c[99C", 0x1B);
		printf("%c[99B", 0x1B);
		
		old = rows+cols;
		console_getpos(&cols, &rows);
	}
	
	console_setpos(col, row);
	
	if(cols_p) { *cols_p = cols+1; }
	if(rows_p) { *rows_p = rows+1; }
}

/* Erase part of the terminal  display */
void console_erase(char const *mode) {
	int col, row;
	console_getpos(&col, &row);
	
	printf("%c[%s", 0x1B, mode);
	
	console_setpos(col, row);
}

static int getchar_cbuf(void) {
	if(getc_count) {
		int ret = getc_buf[0], i;
		
		for(i = 1; i < getc_count; i++) {
			getc_buf[i-1] = getc_buf[i];
		}
		
		getc_count--;
		
		return ret;
	}
	
	return getchar();
}

/* Get a character */
int console_getchar(void) {
	while(1) {
		int c = getchar_cbuf();
		
		if(c == 0x7F) {
			return KEY_BACKSPACE;
		}
		
		if(c == 0x1B) {
			if((c = getchar_cbuf()) == '[') {
				c = getchar_cbuf();
				
				switch(c) {
					case 0x41: return KEY_UP;
					case 0x42: return KEY_DOWN;
					case 0x43: return KEY_RIGHT;
					case 0x44: return KEY_LEFT;
					case 0x31: getchar(); return KEY_HOME;
					case 0x34: getchar(); return KEY_END;
					case 0x33: getchar(); return KEY_DEL;
				}
				
				/* Unknown escape, hope it's only 3 bytes */
				continue;
			}else{
				GETC_PUSH(c);
				return 0x1B;
			}
		}
		
		return c;
	}
}

/* Print output */
void print2(int flags, char const *fmt, ...) {
	va_list argv;
	char msg[1024];
	
	va_start(argv, fmt);
	vsnprintf(msg, 1024, fmt, argv);
	va_end(argv);
	
	if(flags & P2_DEBUG) {
		debug("%s", msg);
	}
	
	if(flags & P2_ALERT) {
		alert = 1;
	}
	
	puts(msg);
}

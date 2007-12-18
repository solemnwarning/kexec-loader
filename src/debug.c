/* kexec-loader - Debugging functions
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
#include <stdarg.h>
#include <unistd.h>

#include "../config.h"
#include "debug.h"

/* Write string to debug console */
static void debug_console(char const* string) {
	#ifdef DEBUG_CONSOLE
	static FILE* console = NULL;
	while(console == NULL) {
		console = fopen(DEBUG_CONSOLE, "w");
		
		if(console == NULL && errno != EINTR) {
			return;
		}
	}
	
	fprintf(console, "%s", string);
	#endif
}

/* Write string to debug log */
static void debug_file(char const* string) {
	#ifdef DEBUG_FILE
	static FILE* debug_fh = NULL;
	while(debug_fh == NULL) {
		debug_fh = fopen(DEBUG_FILE, "a");
		
		if(debug_fh == NULL && errno != EINTR) {
			return;
		}
	}
	
	fprintf(debug_fh, "%s", string);
	fsync(fileno(debug_fh));
	sync();
	#endif
}

/* Write debug message to all available debug targets */
void debug_r(char const* file, unsigned int line, char const* fmt, ...) {
	#if defined(DEBUG_CONSOLE) || defined(DEBUG_FILE)
	char buf[256] = {'\0'};
	char obuf[256] = {'\0'};
	
	va_list argv;
	va_start(argv, fmt);
	vsnprintf(buf, 255, fmt, argv);
	va_end(argv);
	
	snprintf(obuf, 255, "debug(%s) at %s:%u\n", buf, file, line);
	
	debug_console(obuf);
	debug_file(obuf);
	#endif
}

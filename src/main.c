/* kexec-loader - Main source
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
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "mount.h"
#include "main.h"
#include "ctconfig.h"
#include "console.h"

/* Fatal error encountered, abort! */
void fatal_r(char const* file, unsigned int line, char const* fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	fprintf(stderr, "fatal() called at %s:%u!\n", file, line);
	vfprintf(stderr, fmt, argv);
	
	va_end(argv);
	
	while(1) {
		sleep(9999);
	}
}

/* Write debug message to console */
void debug_r(char const* file, unsigned int line, char const* fmt, ...) {
	#if defined(DEBUG_TTYN) || defined(DEBUG_FILE)
	char buf[256] = {'\0'};
	char obuf[256] = {'\0'};
	
	va_list argv;
	va_start(argv, fmt);
	vsnprintf(buf, 255, fmt, argv);
	va_end(argv);
	
	snprintf(obuf, 255, "debug(%s) at %s:%u\n", buf, file, line);
	#endif
	
	#ifdef DEBUG_TTYN
	tty_write(DEBUG_TTYN, obuf, strlen(obuf));
	#endif
	
	#ifdef DEBUG_FILE
	static FILE* debug_fh = NULL;
	while(debug_fh == NULL) {
		debug_fh = fopen(DEBUG_FILE, "a");
		
		if(debug_fh == NULL && errno != EINTR) {
			return;
		}
	}
	
	fprintf(debug_fh, "%s", obuf);
	fsync(fileno(debug_fh));
	sync();
	#endif
}

int main(int argc, char** argv) {
	mount_proc();
	return(0);
}

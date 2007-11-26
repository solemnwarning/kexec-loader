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

#include "misc.h"
#include "main.h"

/* Error checking malloc() wrapper, also zeros memory */
void* allocate_r(char const* file, unsigned int line, size_t size) {
	void* ptr = malloc(size);
	if(ptr == NULL) {
		fatal_r(file, line, "Can't allocate %u bytes: %s", strerror(errno));
	}
	
	memset(ptr, 0, size);
	return(ptr);
}


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

/* Copy a string */
char* strclone(char const* string, size_t maxlen) {
	size_t len = 0;
	while(len < maxlen || maxlen == 0) {
		if(string[len] == '\0') {
			break;
		}
		len++;
	}
	
	char* dest = allocate(len+1);
	strncpy(dest, string, len);
	return(dest);
}

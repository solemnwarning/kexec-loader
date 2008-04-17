/* kexec-loader - String functions
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
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "misc.h"

/* strcat() wrapper which allocates required memory size
 * If realloc() fails the old string is returned
*/
char *my_strcat(char *dest, char *src) {
	size_t size = strlen(dest) + strlen(src) + 1;
	
	char *nptr = realloc(dest, size);
	if(!nptr) {
		debug("Can't realloc() string to %u bytes\n", size);
		return dest;
	}
	
	strcat(nptr, src);
	return nptr;
}

/* sprintf() wrapper which allocates required size buffer
 * If malloc() fails NULL is returned
*/
char *my_sprintf(char const *fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	size_t size = vsnprintf(NULL, 0, fmt, argv)+1;
	
	char *dest = malloc(size);
	if(!dest) {
		debug("Can't malloc() %u bytes\n", size);
		goto RET;
	}
	
	vsprintf(dest, fmt, argv);
	
	RET:
	va_end(argv);
	return dest;
}

/* Append a sprintf() format string to a string
 * If memory allocation fails the old string is returned
*/
char *my_asprintf(char *str, char const *fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	char buf[vsnprintf(NULL, 0, fmt, argv)+1];
	vsprintf(buf, fmt, argv);
	
	va_end(argv);
	return my_strcat(str, buf);
}

/* Allocate a buffer using malloc() and copy a string into it
 * Returns the string on success, NULL if malloc() fails
*/
char *my_strcpy(char *src) {
	size_t size = strlen(src)+1;
	
	char *dest = malloc(size);
	if(!dest) {
		debug("Can't malloc() %u bytes\n", size);
		return NULL;
	}
	
	strcpy(dest, src);
	return dest;
}

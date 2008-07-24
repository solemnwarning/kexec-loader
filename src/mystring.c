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

/* Allocate a buffer using allocate() and copy the supplied string into it, if
 * max is non-negative no more then max characters are copied to the new string
*/
char *str_copy(char const *src, int max) {
	size_t len;
	for(len = 0; src[len] || max < 0; len++) {}
	
	char *dest = allocate(len+1);
	
	strncpy(dest, src, len);
	dest[len] = '\0';
	
	return dest;
}

/* Allocate a buffer using allocate() and write the supplied sprintf-format
 * string to it
*/
char *str_printf(char const *fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	char *dest = allocate(vsnprintf(NULL, 0, fmt, argv)+1);
	vsprintf(dest, fmt, argv);
	
	va_end(argv);
	return dest;
}

/* Append text to a string, if max is non-negative no more then max characters
 * of src will be appended to dest
 *
 * If dest is NULL, a new string will be created
*/
char *str_append(char *dest, char const *src, int max) {
	size_t alen = 0, dlen = 0, n;
	while(src[alen] || max < 0) { alen++; }
	while(dest && dest[dlen]) { dlen++; }
	
	if((dest = realloc(dest, alen+dlen+1)) == NULL) {
		fatal("Failed to realloc buffer to %u", alen+dlen+1);
	}
	
	for(n = 0; n < alen; n++) {
		dest[dlen+n] = src[n];
	}
	dest[alen+dlen] = '\0';
	
	return dest;
}
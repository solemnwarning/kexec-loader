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
#include <ctype.h>

#include "mystring.h"
#include "misc.h"

#define RCASE(x) (flags & GLOB_IGNCASE ? tolower(x) : x)

/* Create a copy of src and return it, if max is non-negative, no more then max
 * characters will be copied to the new buffer.
 *
 * If dest is not NULL, the pointer to the new buffer will also be stored in
 * *dest, if *dest is also not NULL it will be free()'d.
*/
char *str_copy(char **dest, char const *src, int max) {
	size_t len;
	for(len = 0; src[len] && (len < max || max < 0); len++) {}
	
	char *rptr = allocate(len+1);
	
	strncpy(rptr, src, len);
	rptr[len] = '\0';
	
	if(dest) {
		free(*dest);
		*dest = rptr;
	}
	
	return rptr;
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
	while(src[alen] && (alen < max || max < 0)) { alen++; }
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

/* Compare a string
 * If max is non-negative, no more then max characters will be compared
 *
 * Returns 1 on match, zero otherwise
*/
int str_eq(char const *s1, char const *s2, int max) {
	int n;
	
	for(n = 0; n < max || max < 0; n++) {
		if(s1[n] != s2[n]) {
			return 0;
		}
		if(s1[n] == '\0') {
			return 1;
		}
	}
	
	return 1;
}

/* Same as above, but ignoring case */
int str_ceq(char const *s1, char const *s2, int max) {
	int n;
	
	for(n = 0; n < max || max < 0; n++) {
		if(tolower(s1[n]) != tolower(s2[n])) {
			return 0;
		}
		if(s1[n] == '\0') {
			return 1;
		}
	}
	
	return 1;
}

/* Compare function used by globcmp() */
static int mycmp(char const *str, char const *expr, size_t len, int flags) {
	size_t n;
	
	for(n = 0; n < len; n++) {
		if(RCASE(str[n]) == RCASE(expr[n])) {
			if(str[n] == '\0') {
				return 1;
			}
			
			continue;
		}
		if(flags & GLOB_SINGLE && expr[n] == '?' && str[n] != '\0') {
			continue;
		}
		if(flags & GLOB_HASH && expr[n] == '#' && isdigit(str[n])) {
			continue;
		}
		
		return 0;
	}
	
	return 1;
}

/* Match a glob (wildcard) expression against a string
 * Returns 1 on match, zero otherwise
*/
int globcmp(char const *str, char const *expr, int flags, ...) {
	size_t n, l;
	
	while(1) {
		if(flags & GLOB_STAR && expr[0] == '*') {
			while(expr[0] == '*') { expr++; }
			
			if(str[0] == '\0' || expr[0] == '\0') {
				return 1;
			}
			
			n = strcspn(expr, "*");
			
			if(expr[n] == '*') {
				while(!mycmp(str, expr, n, flags)) {
					str++;
				}
			}else{
				if((l = strlen(str)) < n) {
					return 0;
				}
				
				str = (str + l) - n;
				
				if(mycmp(str, expr, n, flags)) {
					return 1;
				}else{
					return 0;
				}
			}
			
			str += n;
			expr += n;
			continue;
		}
		if(!mycmp(str, expr, 1, flags)) {
			return 0;
		}
		if(expr[0] == '\0') {
			return 1;
		}
		
		expr++;
		str++;
	}
	
	return 1;
}

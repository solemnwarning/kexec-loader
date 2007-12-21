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

/* Handle a non-fatal error */
void nferror_r(char const* file, unsigned int line, char const* fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	char buf[128] = {'\0'};
	vsnprintf(buf, 127, fmt, argv);
	
	eprintf("%s:%u: %s\n", file, line, buf);
	
	va_end(argv);
}

/* Print a warning message */
void warn_r(char const* file, unsigned int line, char const* fmt, ...) {
	va_list argv;
	va_start(argv, fmt);
	
	char buf[128] = {'\0'};
	vsnprintf(buf, 127, fmt, argv);
	
	printf("%s:%u: %s\n", file, line, buf);
	
	va_end(argv);
}

/* Copy a string */
char* strclone(char const* string, size_t maxlen) {
	size_t len = 0;
	while(len < maxlen) {
		if(string[len] == '\0') {
			break;
		}
		len++;
	}
	
	char* dest = allocate(len+1);
	strncpy(dest, string, len);
	return(dest);
}

/* Compare str1 and str2
 * Returns 1 on match, 0 on mismatch
*/
int str_compare(char const* str1, char const* str2, int flags, ...) {
	if(str1 == NULL && str2 == NULL) {
		return(1);
	}
	if(str1 == NULL || str2 == NULL) {
		return(0);
	}
	
	size_t maxlen = 0;
	size_t compared = 0;
	
	va_list arglist;
	va_start(arglist, flags);
	if(flags & STR_MAXLEN) {
		maxlen = va_arg(arglist, size_t);
	}
	va_end(arglist);
	
	for(; maxlen == 0 || compared < maxlen; compared++) {
		if(str1[0] == str2[0]) {
			if(str1[0] == '\0') {
				break;
			}
			
			str1++;
			str2++;
			continue;
		}
		if(flags & STR_NOCASE && (tolower(str1[0]) == tolower(str2[0]))) {
			str1++;
			str2++;
			continue;
		}
		if((flags & STR_WILDCARDS || flags & STR_WILDCARD1) && (str1[0] == '*' || str1[0] == '?')) {
			if(str1[0] == '?' && str2[0] != '\0') {
				str1++;
				str2++;
				continue;
			}
			if(str1[0] == '*') {
				if(str2[0] == '\0' || str1[1] == '\0') {
					break;
				}
				if(str1[1] == str2[0]) {
					str1 += 2;
				}
				str2++;
				continue;
			}
		}
		if((flags & STR_WILDCARDS || flags & STR_WILDCARD2) && (str2[0] == '*' || str2[0] == '?')) {
			if(str2[0] == '?' && str1[0] != '\0') {
				str1++;
				str2++;
				continue;
			}
			if(str2[0] == '*') {
				if(str1[0] == '\0' || str2[1] == '\0') {
					break;
				}
				if(str2[1] == str1[0]) {
					str2 += 2;
				}
				str1++;
				continue;
			}
		}
		
		return(0);
	}
	return(1);
}

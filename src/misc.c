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
#include "config.h"

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

/* Print a debug message */
void debug_r(char const* file, unsigned int line, char const* fmt, ...) {
	#ifdef DEBUG
	va_list argv;
	va_start(argv, fmt);
	
	char buf[128] = {'\0'};
	vsnprintf(buf, 127, fmt, argv);
	
	printf("%s:%u: %s\n", file, line, buf);
	
	va_end(argv);
	#endif
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
	
	for(; !(flags & STR_MAXLEN) || compared < maxlen; compared++) {
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

/* Insert a new target structure at the beginning of a target list
 * Returns a pointer to the new list node on success, NULL if malloc() fails.
 *
 * If 'src' is non-NULL, the data in 'src' will be copied to the new structure
 * in the list, it will not be modified.
*/
kl_target* target_add(kl_target** list, kl_target const* src) {
	kl_target* nptr = malloc(sizeof(kl_target));
	if(nptr == NULL) {
		return(NULL);
	}
	TARGET_DEFAULTS(nptr);
	
	if(src != NULL) {
		strncpy(nptr->name, src->name, 64);
		strncpy(nptr->kernel, src->kernel, 1024);
		strncpy(nptr->initrd, src->initrd, 1024);
		strncpy(nptr->append, src->append, 512);
		
		nptr->flags = src->flags;
		nptr->mounts = mount_copy(src->mounts);
	}
	
	kl_target* eptr = *list;
	while(eptr != NULL && eptr->next != NULL) {
		eptr = eptr->next;
	}
	if(eptr == NULL) {
		nptr->next = *list;
		*list = nptr;
	}else{
		eptr->next = nptr;
		nptr->next = NULL;
	}
	
	return(nptr);
}

/* Free an entire target list */
void target_free(kl_target** list) {
	kl_target* dptr = NULL;
	
	while(*list != NULL) {
		dptr = *list;
		*list = (*list)->next;
		
		mount_free(&(dptr->mounts));
		free(dptr);
	}
}

/* Exactly the same as target_add(), except it handles mount lists instead of
 * target lists.
*/
kl_mount* mount_add(kl_mount** list, kl_mount const* src) {
	kl_mount* nptr = malloc(sizeof(kl_mount));
	if(nptr == NULL) {
		return(NULL);
	}
	MOUNT_DEFAULTS(nptr);
	
	if(src != NULL) {
		strncpy(nptr->device, src->device, 1024);
		strncpy(nptr->mpoint, src->mpoint, 1024);
		strncpy(nptr->fstype, src->fstype, 64);
	}
	
	nptr->next = *list;
	*list = nptr;
	
	return(nptr);
}

/* Free an entire mount list */
void mount_free(kl_mount** list) {
	kl_mount* dptr = NULL;
	
	while(*list != NULL) {
		dptr = *list;
		*list = (*list)->next;
		
		free(dptr);
	}
}

/* Copy a mount list
 *
 * Returns a pointer to the copied list upon success, or NULL if any malloc()
 * calls fail.
*/
kl_mount* mount_copy(kl_mount const* src) {
	kl_mount* nptr = NULL;
	kl_mount* list = NULL;
	
	while(src != NULL) {
		if((nptr = malloc(sizeof(kl_mount))) == NULL) {
			mount_free(&list);
			return(NULL);
		}
		MOUNT_DEFAULTS(nptr);
		
		strncpy(nptr->device, src->device, 1024);
		strncpy(nptr->mpoint, src->mpoint, 1024);
		strncpy(nptr->fstype, src->fstype, 64);
		
		nptr->next = list;
		list = nptr;
		
		src = src->next;
	}
	
	return(list);
}

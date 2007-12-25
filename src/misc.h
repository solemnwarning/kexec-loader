/* kexec-loader - Misc. functions header
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

#ifndef KEXEC_LOADER_MISC_H
#define KEXEC_LOADER_MISC_H
#include "config.h"

/* String comparison flags */
#define STR_NOCASE	1	/* Do case-insensitive comparision */
#define STR_MAXLEN	2	/* Don't compare more then ... bytes */
#define STR_WILDCARDS	4	/* Parse wildcard characters * and ? */
#define STR_WILDCARD1	8	/* Parse wildcard characters * and ? in str1 */
#define STR_WILDCARD2	16	/* Parse wildcard characters * and ? in str2 */

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#define IS_WHITESPACE(x) (x == ' ' || x == '\t' || x == '\n' || x == '\r')

#define allocate(size) allocate_r(__FILE__, __LINE__, size)
void* allocate_r(char const* file, unsigned int line, size_t size);

#define fatal(...) fatal_r(__FILE__, __LINE__, __VA_ARGS__)
void fatal_r(char const* file, unsigned int line, char const* fmt, ...);

#define nferror(...) nferror_r(__FILE__, __LINE__, __VA_ARGS__)
void nferror_r(char const* file, unsigned int line, char const* fmt, ...);

#define warn(...) warn_r(__FILE__, __LINE__, __VA_ARGS__)
void warn_r(char const* file, unsigned int line, char const* fmt, ...);

#ifdef DEBUG
#define debug(...) debug_r(__FILE__, __LINE__, __VA_ARGS__)
#else
#define debug(...)
#endif
void debug_r(char const* file, unsigned int line, char const* fmt, ...);

char* strclone(char const* string, size_t maxlen);
int str_compare(char const*, char const*, int, ...);

kl_target* target_add(kl_target** list, kl_target const* src);
void target_free(kl_target** list);

kl_mount* mount_add(kl_mount** list, kl_mount const* src);
void mount_free(kl_mount** list);
kl_mount* mount_copy(kl_mount const* src);

#endif /* !KEXEC_LOADER_MISC_H */

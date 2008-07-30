/* kexec-loader - ELF functions
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
#include <elf.h>

#include "elf.h"
#include "mystring.h"

static char *elf32_getsection(char const *elf, char const *name, size_t *size);
static char *elf64_getsection(char const *elf, char const *name, size_t *size);

/* Load a section from an ELF binary
 * Calls elf32_getsection() or elf64_getsection() depending on ELF format
*/
char *elf_getsection(char const *elf, char const *name, size_t *size) {
	if(elf[0] != 0x7F || !str_eq(elf+1, "ELF", 3)) {
		return NULL;
	}
	
	if(elf[EI_CLASS] == ELFCLASS32) {
		return elf32_getsection(elf, name, size);
	}
	if(elf[EI_CLASS] == ELFCLASS64) {
		return elf64_getsection(elf, name, size);
	}
	
	return NULL;
}

/* Get a section from an ELF32 binary
 * Returns NULL if the section was not found
*/
static char *elf32_getsection(char const *elf, char const *name, size_t *size) {
	return NULL;
}

/* Get a section from an ELF64 binary
 * Returns NULL if the section was not found
*/
static char *elf64_getsection(char const *elf, char const *name, size_t *size) {
	return NULL;
}

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
#include <stdint.h>
#include <string.h>

#include "elf.h"
#include "mystring.h"

#define elf2host(dest, src, size) \
	elf2host_(dest, src, size, elf[EI_DATA])

static char *elf32_getsection(char const *elf, char const *name, size_t *size);
static char *elf64_getsection(char const *elf, char const *name, size_t *size);
static void elf2host_(void *dest, void const *src, size_t size, char eidata);

/* Load a section from an ELF binary
 * Calls elf32_getsection() or elf64_getsection() depending on ELF format
*/
char *elf_getsection(char const *elf, char const *name, size_t *size) {
	union {
		uint16_t i;
		char c[2];
	} etest;
	
	etest.i = 1;
	
	
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
	Elf32_Off e_shoff;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum, shnum;
	Elf32_Half e_shstrndx;
	
	elf2host(&e_shoff, elf+32, sizeof(Elf32_Off));
	elf2host(&e_shentsize, elf+46, sizeof(Elf32_Half));
	elf2host(&e_shnum, elf+48, sizeof(Elf32_Half));
	elf2host(&e_shstrndx, elf+50, sizeof(Elf32_Half));
	
	Elf32_Word sh_name;
	Elf32_Off sh_offset;
	Elf32_Word sh_size;
	
	elf2host(&sh_offset, elf+e_shoff+(e_shentsize*e_shstrndx)+16, sizeof(Elf32_Off));
	char *sh_strings = (char*)elf+sh_offset;
	
	for(shnum = 0; shnum < e_shnum; shnum++) {
		elf2host(&sh_name, elf+e_shoff+(e_shentsize*shnum), sizeof(Elf32_Word));
		elf2host(&sh_offset, elf+e_shoff+(e_shentsize*shnum)+16, sizeof(Elf32_Off));
		elf2host(&sh_size, elf+e_shoff+(e_shentsize*shnum)+20, sizeof(Elf32_Word));
		
		if(str_eq(name, sh_strings+sh_name, -1)) {
			*size = sh_size;
			return (char*)elf+sh_offset;
		}
	}
	
	return NULL;
}

/* Get a section from an ELF64 binary
 * Returns NULL if the section was not found
*/
static char *elf64_getsection(char const *elf, char const *name, size_t *size) {
	Elf64_Off e_shoff;
	Elf64_Half e_shentsize;
	Elf64_Half e_shnum, shnum;
	Elf64_Half e_shstrndx;
	
	elf2host(&e_shoff, elf+40, sizeof(Elf64_Off));
	elf2host(&e_shentsize, elf+58, sizeof(Elf64_Half));
	elf2host(&e_shnum, elf+60, sizeof(Elf64_Half));
	elf2host(&e_shstrndx, elf+62, sizeof(Elf64_Half));
	
	Elf64_Word sh_name;
	Elf64_Off sh_offset;
	Elf64_Xword sh_size;
	
	elf2host(&sh_offset, elf+e_shoff+(e_shentsize*e_shstrndx)+24, sizeof(Elf64_Off));
	char *sh_strings = (char*)elf+sh_offset;
	
	for(shnum = 0; shnum < e_shnum; shnum++) {
		elf2host(&sh_name, elf+e_shoff+(e_shentsize*shnum), sizeof(Elf64_Word));
		elf2host(&sh_offset, elf+e_shoff+(e_shentsize*shnum)+24, sizeof(Elf64_Off));
		elf2host(&sh_size, elf+e_shoff+(e_shentsize*shnum)+32, sizeof(Elf64_Xword));
		
		if(str_eq(name, sh_strings+sh_name, -1)) {
			*size = sh_size;
			return (char*)elf+sh_offset;
		}
	}
	
	return NULL;
}

/* Convert a value in the ELF binary from the ELF endian to the host endian. */
static void elf2host_(void *dest, void const *src, size_t size, char eidata) {
	char host_endian = ELFDATANONE;
	int16_t test = 1;
	
	if(((char*)&test)[0] == 1) {
		host_endian = ELFDATA2LSB;
	}else{
		host_endian = ELFDATA2MSB;
	}
	
	if(host_endian != eidata) {
		size_t n;
		
		for(n = 0; n < size; n++) {
			((char*)dest)[n] = ((char*)src)[size-n-1];
		}
	}else{
		memcpy(dest, src, size);
	}
}

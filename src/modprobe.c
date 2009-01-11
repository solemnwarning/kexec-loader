/* kexec-loader - Module loading functions
 * Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
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
#include <stdio.h>
#include <errno.h>
#include <zlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <dirent.h>

#include "modprobe.h"
#include "mystring.h"
#include "console.h"
#include "misc.h"
#include "config.h"

#define elf2host(dest, src, size) \
	elf2host_(dest, src, size, elf[EI_DATA])

static kl_module *mod_options = NULL;
static kl_module *k_modules = NULL;

static char *elf_getsection(char const *elf, char const *name, size_t *size);
static char *elf32_getsection(char const *elf, char const *name, size_t *size);
static char *elf64_getsection(char const *elf, char const *name, size_t *size);
static void elf2host_(void *dest, void const *src, size_t size, char eidata);
static char *next_arg(char *arg);
static int modprobe(char const *name, int lvl);
static const char *moderror(int err);
static void module_add(kl_module **list, char const *module, char const *args);
static kl_module *module_search(kl_module *list, char const *module);

/* Load a section from an ELF binary
 * Calls elf32_getsection() or elf64_getsection() depending on ELF format
*/
static char *elf_getsection(char const *elf, char const *name, size_t *size) {
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

/* Load modules.conf */
void load_modconf(void) {
	char line[1024], *name, *module, *args;
	int lnum = 0;
	
	FILE *fh = fopen("/boot/modules/modules.conf", "r");
	if(!fh) {
		printD(RED, 2, "Can't open /modules/modules.conf: %s", strerror(errno));
		return;
	}
	
	while(fgets(line, 1024, fh)) {
		line[strcspn(line, "\r\n")] = '\0';
		lnum++;
		
		name = line+strspn(line, "\r\n\t ");
		module = next_arg(name);
		args = next_arg(module);
		
		if(name[0] == '#' || name[0] == '\0') {
			continue;
		}
		
		if(str_eq(name, "options", -1)) {
			if(module[0] == '\0') {
				printD(RED, 2, "modules.conf:%u: Usage: options <module> <args>", lnum);
				continue;
			}
			
			module_add(&mod_options, module, args);
			continue;
		}
		
		printD(RED, 2, "modules.conf:%u: Unknown directive '%s'", lnum, name);
	}
	
	if(ferror(fh)) {
		printD(RED, 2, "Can't read /modules/modules.conf: %s", strerror(errno));
	}
	
	fclose(fh);
}

/* Next argument */
static char *next_arg(char *arg) {
	arg += strcspn(arg, "\t ");
	
	if(arg[0] != '\0') {
		arg[0] = '\0';
		arg++;
		
		arg += strspn(arg, "\t ");
	}
	
	return arg;
}

/* Load a module */
static int modprobe(char const *name, int lvl) {
	char *filename = str_printf("/boot/modules/%s.ko", name);
	gzFile fh = NULL;
	char *buf = NULL, *args = "";
	size_t bsize = 64000, rbytes = 0;
	int retval = 0, fret;
	kl_module *optptr;
	
	if(module_search(k_modules, name)) {
		goto CLEANUP;
	}
	
	optptr = module_search(mod_options, name);
	if(optptr) {
		args = optptr->args;
	}
	
	if(args[0] == '\0') {
		printd(GREEN, lvl++, "Loading module %s...", name);
	}else{
		printd(GREEN, lvl++, "Loading module %s (%s)...", name, args);
	}
	
	if(!(fh = gzopen(filename, "rb"))) {
		printD(RED, lvl, "Failed to open %s.ko: %s", name, strerror(errno));
		goto CLEANUP;
	}
	
	while(!gzeof(fh)) {
		buf = reallocate(buf, bsize);
		
		if((fret = gzread(fh, buf+rbytes, 64000)) == -1) {
			printD(RED, lvl, "Failed to read %s.ko: %s", name, gzerror(fh, &fret));
			goto CLEANUP;
		}
		
		rbytes += fret;
		bsize += fret;
	}
	
	size_t secsize, n = 0;
	char *sec = elf_getsection(buf, ".modinfo", &secsize);
	
	while(sec && n < secsize) {
		if(!str_eq(sec, "depends=", 8)) {
			n += (strlen(sec)+1);
			sec += (strlen(sec)+1);
			continue;
		}
		
		sec += 8;
		while(sec[0] != '\0') {
			char *dep = str_copy(NULL, sec, strcspn(sec, ","));
			
			if(!module_search(k_modules, dep) && !modprobe(dep, lvl)) {
				free(dep);
				goto CLEANUP;
			}
			
			free(dep);
			sec += (strcspn(sec, ",")+1);
		}
		
		break;
	}
	
	if(syscall(SYS_init_module, buf, rbytes, args) != 0) {
		if(errno == EEXIST) {
			goto CLEANUP;
		}
		
		printD(RED, lvl, "Failed to load module '%s': %s", name, moderror(errno));
		goto CLEANUP;
	}
	
	module_add(&k_modules, name, args);
	
	retval = 1;
	
	CLEANUP:
	if(fh && (fret = gzclose(fh)) != Z_OK) {
		debug("Failed to close '%s': %s\n", filename, gzerror(fh, &fret));
	}
	
	free(filename);
	free(buf);
	return retval;
}

/* Return a module error string */
static const char *moderror(int err) {
	switch (err) {
		case ENOEXEC:
			return "Invalid module format";
		case ENOENT:
			return "Unknown symbol in module";
		case ESRCH:
			return "Module has wrong symbol version";
		case EINVAL:
			return "Invalid parameters";
		default:
			return strerror(err);
	}
}

/* Add a node to a kl_module list */
static void module_add(kl_module **list, char const *module, char const *args) {
	kl_module *nptr = allocate(sizeof(kl_module));
	INIT_MODULE(nptr);
	
	nptr->module = str_copy(NULL, module, -1);
	nptr->args = str_copy(NULL, args, -1);
	
	nptr->next = *list;
	*list = nptr;
}

/* Search a kl_module list */
static kl_module *module_search(kl_module *list, char const *module) {
	while(list) {
		if(str_eq(list->module, module, -1)) {
			break;
		}
		
		list = list->next;
	}
	
	return list;
}

/* Load all modules */
void modprobe_all(void) {
	DIR *dir = opendir("/boot/modules/");
	if(!dir) {
		printD(RED, 2, "Can't open /modules/: %s", strerror(errno));
		return;
	}
	
	struct dirent *child;
	while((child = readdir(dir))) {
		if(globcmp(child->d_name, "*.ko", GLOB_IGNCASE | GLOB_STAR)) {
			strstr(child->d_name, ".ko")[0] = '\0';
			modprobe(child->d_name, 2);
		}
	}
	
	closedir(dir);
}

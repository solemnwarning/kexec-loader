/* kexec-loader - Load kernel modules
 * Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include <endian.h>

#include "console.h"
#include "misc.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELF_ORDER ELFDATA2LSB
#else
#define ELF_ORDER ELFDATA2MSB
#endif

#define elf2host(dest, src) elf2host_(&(dest), src, sizeof(dest), elf[EI_DATA])

static char *elf_getsection(char const *elf, char const *name, size_t *size);
static char *elf32_getsection(char const *elf, char const *name, size_t *size);
static char *elf64_getsection(char const *elf, char const *name, size_t *size);
static void elf2host_(void *dest, void const *src, int size, char eidata);
static int modprobe(char const *name);
static const char *moderror(int err);

/* Load a section from an ELF binary
 * Calls elf32_getsection() or elf64_getsection() depending on ELF format
*/
static char *elf_getsection(char const *elf, char const *name, size_t *size) {
	if(*elf != 0x7F && !kl_strneq(elf+1, "ELF", 3)) {
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

#define ELF_SHDR(i) ((void*)elf+e_shoff+(i*e_shentsize))

/* Get a section from an ELF32 binary
 * Returns NULL if the section was not found
*/
static char *elf32_getsection(char const *elf, char const *name, size_t *size) {
	Elf32_Off e_shoff;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum, shnum;
	Elf32_Half e_shstrndx;
	
	Elf32_Ehdr *header = (Elf32_Ehdr*)elf;
	
	elf2host(e_shoff, &header->e_shoff);
	elf2host(e_shentsize, &header->e_shentsize);
	elf2host(e_shnum, &header->e_shnum);
	elf2host(e_shstrndx, &header->e_shstrndx);
	
	Elf32_Word sh_name;
	Elf32_Off sh_offset;
	Elf32_Word sh_size;
	
	Elf32_Shdr *shdr = ELF_SHDR(e_shstrndx);
	elf2host(sh_offset, &shdr->sh_offset);
	char *sh_strings = (char*)elf+sh_offset;
	
	for(shnum = 0; shnum < e_shnum; shnum++) {
		shdr = ELF_SHDR(shnum);
		elf2host(sh_name, &shdr->sh_name);
		elf2host(sh_offset, &shdr->sh_offset);
		elf2host(sh_size, &shdr->sh_size);
		
		if(kl_streq(name, sh_strings+sh_name)) {
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
	
	Elf64_Ehdr *header = (Elf64_Ehdr*)elf;
	
	elf2host(e_shoff, &header->e_shoff);
	elf2host(e_shentsize, &header->e_shentsize);
	elf2host(e_shnum, &header->e_shnum);
	elf2host(e_shstrndx, &header->e_shstrndx);
	
	Elf64_Word sh_name;
	Elf64_Off sh_offset;
	Elf64_Word sh_size;
	
	Elf64_Shdr *shdr = ELF_SHDR(e_shstrndx);
	elf2host(sh_offset, &shdr->sh_offset);
	char *sh_strings = (char*)elf+sh_offset;
	
	for(shnum = 0; shnum < e_shnum; shnum++) {
		shdr = ELF_SHDR(shnum);
		elf2host(sh_name, &shdr->sh_name);
		elf2host(sh_offset, &shdr->sh_offset);
		elf2host(sh_size, &shdr->sh_size);
		
		if(kl_streq(name, sh_strings+sh_name)) {
			*size = sh_size;
			return (char*)elf+sh_offset;
		}
	}
	
	return NULL;
}

/* Convert a value in the ELF binary from the ELF endian to the host endian.
 * dest and src must not overlap
*/
static void elf2host_(void *dest, void const *src, int size, char eidata) {
	if(eidata != ELF_ORDER) {
		int n;
		
		for(n = 0; n < size; n++) {
			((char*)dest)[n] = ((char*)src)[size-n-1];
		}
	}else{
		memcpy(dest, src, size);
	}
}

#define MODPROBE_TEST(...) \
	snprintf(path, sizeof(path), __VA_ARGS__); \
	if(access(path, F_OK) == 0) { \
		found = 1; \
	}

/* Load a module */
static int modprobe(char const *name) {
	char path[1024], dep[256];
	char *buf = NULL, *args = "";
	int bsize = 64000, rbytes = 0;
	int retval = 0, fret, found = 0;
	
	kl_module *optptr = kmods;
	while(optptr) {
		if(kl_streq(name, optptr->name)) {
			args = optptr->args;
			break;
		}
		
		optptr = optptr->next;
	}
	
	if(boot_disk) {
		MODPROBE_TEST("/mnt/%s/modules/%s.ko", boot_disk->name, name);
	}
	
	if(!found) {
		MODPROBE_TEST("/modules/%s.ko", name);
	}
	
	if(!found) {
		printD("Module '%s' not found", name);
		return 0;
	}
	
	gzFile fh = gzopen(path, "rb");
	if(!fh) {
		printD("Error opening %s: %s", path+9, strerror(errno));
		return 0;
	}
	
	while(!gzeof(fh)) {
		buf = kl_realloc(buf, bsize);
		
		if((fret = gzread(fh, buf+rbytes, 64000)) == -1) {
			printD("Error reading %s: %s", path+9, gzerror(fh, &fret));
			goto CLEANUP;
		}
		
		rbytes += fret;
		bsize += fret;
	}
	
	size_t size = 0;
	char *sec = elf_getsection(buf, ".modinfo", &size);
	char *end = sec+size;
	
	while(sec && sec < end) {
		if(!kl_strneq(sec, "depends=", 8)) {
			sec += strlen(sec)+1;
			continue;
		}
		
		sec += 8;
		while(*sec) {
			strlcpy(dep, sec, strcspn(sec, ",")+1);
			
			if(!modprobe(dep)) {
				printD("Module '%s' not loaded, requires '%s'", name, dep);
				goto CLEANUP;
			}
			
			sec += strcspn(sec, ",");
			if(*sec) {
				sec++;
			}
		}
		
		break;
	}
	
	if(syscall(SYS_init_module, buf, rbytes, args)) {
		if(errno == EEXIST) {
			retval = 1;
			goto CLEANUP;
		}
		
		printD("Error loading '%s': %s", name, moderror(errno));
	}else{
		printd("Loaded module '%s'", name);
		retval = 1;
	}
	
	CLEANUP:
	gzclose(fh);
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

/* Load all modules */
void modprobe_boot(void) {
	char filename[1024];
	snprintf(filename, 1024, "/mnt/%s/modules/", boot_disk->name);
	
	DIR *dir = opendir(filename);
	if(!dir) {
		printD("Error opening /modules/: %s", strerror(errno));
		return;
	}
	
	struct dirent *node;
	while((node = readdir(dir))) {
		int i = strlen(node->d_name)-3;
		
		if(kl_streq(node->d_name+i, ".ko")) {
			node->d_name[i] = '\0';
			modprobe(node->d_name);
		}
	}
	
	closedir(dir);
}

/* Load modules from rootfs */
void modprobe_root(void) {
	DIR *dir = opendir("/modules/");
	if(!dir) {
		debug("Error opening /modules/: %s", strerror(errno));
		return;
	}
	
	struct dirent *node;
	while((node = readdir(dir))) {
		int i = strlen(node->d_name)-3;
		
		if(kl_streq(node->d_name+i, ".ko")) {
			node->d_name[i] = '\0';
			modprobe(node->d_name);
		}
	}
	
	closedir(dir);
}

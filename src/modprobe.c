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
#include <lzmadec.h>
#include <arpa/inet.h>
#include <sys/mman.h>

#include "console.h"
#include "misc.h"
#include "globcmp.h"
#include "vfs.h"

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
static int modprobe(char const *name, char const *buf, size_t size);
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
static int modprobe(char const *name, char const *buf, size_t size) {
	char *args = "";
	char dep[256];
	
	kl_module *optptr = kmods;
	while(optptr) {
		if(kl_streq(name, optptr->name)) {
			args = optptr->args;
			break;
		}
		
		optptr = optptr->next;
	}
	
	size_t mi_size = 0;
	char *sec = elf_getsection(buf, ".modinfo", &mi_size);
	char *end = sec+mi_size;
	
	while(sec && sec < end) {
		if(!kl_strneq(sec, "depends=", 8)) {
			sec += strlen(sec)+1;
			continue;
		}
		
		sec += 8;
		while(*sec) {
			strlcpy(dep, sec, strcspn(sec, ",")+1);
			
			if(!load_kmod(dep)) {
				printD("Module '%s' not loaded, requires '%s'", name, dep);
				return 0;
			}
			
			sec += strcspn(sec, ",");
			if(*sec) {
				sec++;
			}
		}
		
		break;
	}
	
	if(syscall(SYS_init_module, buf, size, args)) {
		if(errno == EEXIST) {
			return 1;
		}
		
		printD("Error loading '%s': %s", name, moderror(errno));
	}else{
		debug("Loaded module '%s' (%s)", name, args);
		return 1;
	}
	
	return 0;
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

/* Find and load a module
 * Returns 1 if the module was loaded, 0 otherwise
 *
 * NOTE: Only call during init (when VFS root is set to boot disk)
*/
int load_kmod(char const *module) {
	if(modprobe_dir("(rootfs)/modules/", module)) {
		return 1;
	}
	if(boot_disk && vfs_exists("/modules/") && modprobe_dir("/modules/", module)) {
		return 1;
	}
	
	return 0;
}

/* Extract tarballs from the boot floppy modules directory */
void extract_module_tars(void) {
	if(!boot_disk) {
		return;
	}
	
	DIR *dh = vfs_opendir("/modules/");
	if(!dh) {
		printD("Error opening modules directory: %s", kl_strerror(errno));
		return;
	}
	
	struct dirent *node;
	while((node = readdir(dh))) {
		if(is_tar_extension(node->d_name)) {
			char *tname = kl_sprintf("/modules/%s", node->d_name);
			char *rpath = vfs_translate_path(tname);
			
			if(rpath) {
				extract_tar(rpath, "/modules/");
			}else{
				debug("vfs_translate_path(%s): %s", tname, kl_strerror(errno));
			}
			
			free(tname);
			free(rpath);
		}
	}
	
	closedir(dh);
}

/* Load modules from a directory, module name is an optional filter
 * Returns 1 if a module was loaded, 0 otherwise
*/
int modprobe_dir(char const *dir, char const *modname) {
	DIR *dh = vfs_opendir(dir);
	if(!dh) {
		printD("Failed to open %s: %s", dir, kl_strerror(errno));
		return 0;
	}
	
	int ret = 0;
	
	struct dirent *node;
	while((node = readdir(dh))) {
		char *fpath = kl_sprintf("%s/%s", dir, node->d_name);
		struct stat mfile;
		
		if(vfs_stat(fpath, &mfile) == -1) {
			printD("Failed to stat %s: %s", fpath, kl_strerror(errno));
			
			free(fpath);
			continue;
		}
		
		if(mfile.st_mode & (S_IFREG | S_IFLNK) && kl_streq_end(node->d_name, ".ko")) {
			char *fname = kl_strndup(node->d_name, strlen(node->d_name)-3);
			int fd = -1;
			void *addr = MAP_FAILED;
			
			if(modname && !kl_streq(modname, fname)) {
				goto NEXT;
			}
			
			fd = vfs_open(fpath, O_RDONLY);
			if(fd == -1) {
				printD("Failed to open %s: %s", fpath, kl_strerror(errno));
				goto NEXT;
			}
			
			addr = mmap(NULL, mfile.st_size, PROT_READ, MAP_SHARED, fd, 0);
			if(addr == MAP_FAILED) {
				printD("Failed to map %s: %s", fpath, strerror(errno));
				goto NEXT;
			}
			
			if(modprobe(fname, addr, mfile.st_size)) {
				ret = 1;
			}
			
			NEXT:
			if(addr != MAP_FAILED) {
				munmap(addr, mfile.st_size);
			}
			
			if(fd >= 0) {
				close(fd);
			}
			
			free(fname);
		}
		
		free(fpath);
	}
	
	closedir(dh);
	return ret;
}

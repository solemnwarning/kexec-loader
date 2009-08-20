/* kexec-loader - Misc. stuff
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

#ifndef KL_MISC_H
#define KL_MISC_H

#include "disk.h"

#define TARGET_DEFAULT	(int)(1<<0)
#define TARGET_RESET	(int)(1<<1)

#define INIT_MODULE(ptr) \
	(ptr)->next = NULL; \
	(ptr)->name[0] = '\0'; \
	(ptr)->args[0] = '\0';

typedef struct kl_module {
	struct kl_module *next;
	
	char name[1024];
	char args[1024];
} kl_module;

#define INIT_TARGET(ptr) \
	(ptr)->next = NULL; \
	(ptr)->title[0] = '\0'; \
	(ptr)->flags = 0; \
	(ptr)->root[0] = '\0'; \
	(ptr)->kernel[0] = '\0'; \
	(ptr)->initrd[0] = '\0'; \
	(ptr)->cmdline[0] = '\0'; \
	(ptr)->append[0] = '\0'; \
	(ptr)->modules = NULL;

typedef struct kl_target {
	struct kl_target *next;
	
	char title[256];
	int flags;
	
	char root[256];
	char kernel[1024];
	char initrd[1024];
	char cmdline[1024];
	char append[1024];
	kl_module *modules;
} kl_target;

#define CHECK_HASARG() \
	if(!val[0]) { \
		printD("%s:%u: '%s' requires an argument", fname, lnum, name); \
		continue; \
	}

#define CHECK_HASARG_MULTI(s,n) \
	if(!s) { \
		printD("%s:%u: '%s' requires %d arguments", fname, lnum, name, (int)n); \
		continue; \
	}

#define CHECK_TOPEN() \
	if(!topen) { \
		printD("%s:%u: '%s' must be after a 'title'", fname, lnum, name); \
	} \
	if(topen <= 0) { \
		continue; \
	}

#define CHECK_VPATH() \
	if(!check_vpath(val)) { \
		printD("%s:%d: Invalid path specified", fname, lnum); \
		continue; \
	}

#define CHECK_GDEV() \
	if(*val != '(' || val[strlen(val)-1] != ')') { \
		printD("%s:%d: Invalid device syntax", fname, lnum); \
		continue; \
	} \
	val++; \
	val[strlen(val)-1] = '\0';

#define TARGET_FAIL() \
	topen = 0; \
	while(target.modules) { \
		list_del(&target.modules, target.modules); \
	}

#define ADD_TARGET() \
	if(topen > 0 && !target.root[0]) { \
		printD("%s:%d: No root device specified", fname, topen); \
		TARGET_FAIL(); \
	} \
	if(topen > 0 && !target.kernel[0]) { \
		printD("%s:%d: No kernel specified", fname, topen); \
		TARGET_FAIL(); \
	} \
	if(topen > 0) { \
		list_add_copy(&targets, &target, sizeof(target)); \
	}

extern kl_disk *boot_disk;
extern int timeout;
extern char grub_path[];
extern kl_target *targets;
extern kl_module *kmods;

void debug(char const *fmt, ...);
void die(char const *fmt, ...);
char *get_cmdline(char const *name);
char *next_value(char *ptr);
void list_disks(void);
void call_reboot(int cmd);

void *kl_malloc(size_t size);
void *kl_realloc(void *ptr, size_t size);
char *kl_strdup(char const *src);
char *kl_strndup(char const *src, int max);
char *kl_sprintf(char const *fmt, ...);
int kl_streq(char const *s1, char const *s2);
int kl_strneq(char const *s1, char const *s2, int max);
int kl_strceq(char const *s1, char const *s2);
int kl_strnceq(char const *s1, char const *s2, int max);
int kl_strins(char *dest, char const *src, int offset, int size);

void list_add(void *rptr, void *node);
void list_add_copy(void *rptr, void *node, int size);
void list_del(void *rptr, void *node);
void *list_prev(void *root, void *node);
void list_nuke(void *root);

int check_file(char const *file);

int load_kmod(char const *module);
void boot_target(kl_target *target);
void shell_main(void);
void load_keymap(char const *file);

#endif /* !KL_MISC_H */

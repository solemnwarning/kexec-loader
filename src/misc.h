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

#define EINFILE	256	/* Invalid filename */
#define EBADFS	257	/* Unknown filesystem format */
#define ENDISK	258	/* No disk specified */

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

#define SMALLEST(a, b) ((a) > (b) ? (b) : (a))

extern const kl_disk *boot_disk;
extern int timeout;
extern char grub_path[];
extern kl_target *targets;
extern kl_module *kmods;

void debug(char const *fmt, ...);
void die(char const *fmt, ...);
char const *get_cmdline(char const *name);
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
char const *kl_strerror(int errnum);
int kl_streq_end(char const *str, char const *match);

void list_add(void *rptr, void *node_p);
void list_add_copy(void *rptr, void *node, int size);
void list_del(void *rptr, void *node);
void *list_prev(void *root, void *node);
void list_nuke(void *root);

int check_file(char const *file);

int load_kmod(char const *module);
void extract_module_tars(void);
int modprobe_dir(char const *dir, char const *modname);
void boot_target(kl_target *target);
void shell_main(void);
void load_keymap(char const *file);
int extract_tar(char const *name, char const *dest);
int is_tar_extension(char const *name);

#endif /* !KL_MISC_H */

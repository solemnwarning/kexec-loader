/* kexec-loader - GRUB compatibility code
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "misc.h"
#include "grub.h"
#include "console.h"
#include "disk.h"
#include "vfs.h"

kl_gdev *grub_devmap = NULL;

#define PARSE_TOKEN(first, x) \
	if(!first && islower(*src)) { \
		x[0] = *src; \
		x[1] = '\0'; \
		src++; \
	}else if(isdigit(*src) || (strncmp(src, "msdos", 5) == 0 && isdigit(src[5]))) { \
		if(!isdigit(*src)) { \
			src += 5; \
		} \
		\
		int i = 0; \
		\
		for(; isdigit(*src); src++) { \
			if(i+1 < sizeof(x)) { \
				x[i++] = *src; \
				x[i] = '\0'; \
			} \
		} \
	}else{ \
		return 0; \
	} \
	if((!brackets && *src == '\0') || (brackets && *src == ')')) { \
		return 1; \
	}else if(*src == ',') { \
		src++; \
	}else{ \
		return 0; \
	}

/* Parse a GRUB device name
 * Returns 1 on success, zero on syntax error
*/
int parse_gdev(kl_gdev *dest, char const *src) {
	int brackets = 0;
	
	if(*src == '(') {
		brackets = 1;
		src++;
	}
	
	if((*src != 'f' && *src != 'h') || src[1] != 'd') {
		return 0;
	}
	
	INIT_GDEV(dest);
	
	strlcpy(dest->type, src, 3);
	src += 2;
	
	PARSE_TOKEN(1, dest->p1);
	PARSE_TOKEN(0, dest->p2);
	PARSE_TOKEN(0, dest->p3);
	
	return 0;
}

/* Search the GRUB device list for a device
 * Return a copy of the device
*/
char *lookup_gdev(char const *dev) {
	kl_gdev gdev, *devptr;
	
	if(!parse_gdev(&gdev, dev)) {
		return NULL;
	}
	
	/* Search for absolute device mappings */
	for(devptr = grub_devmap; devptr; devptr = devptr->next) {
		if(!kl_streq(gdev.type, devptr->type)) {
			continue;
		}
		if(!kl_streq(gdev.p1, devptr->p1)) {
			continue;
		}
		if(gdev.p2[0] && !kl_streq(gdev.p2, devptr->p2)) {
			continue;
		}
		if(gdev.p3[0] && !kl_streq(gdev.p3, devptr->p3)) {
			continue;
		}
		
		return kl_strdup(devptr->device);
	}
	
	if(!isdigit(gdev.p2[0]) || gdev.p3[0]) {
		goto FLOPPY;
	}
	
	/* Search for whole disk device mappings */
	for(devptr = grub_devmap; devptr; devptr = devptr->next) {
		if(devptr->p2[0]) {
			continue;
		}
		
		if(kl_streq(gdev.type, devptr->type) && kl_streq(gdev.p1, devptr->p1)) {
			return kl_sprintf("%s%d", devptr->device, atoi(gdev.p2)+1);
		}
	}
	
	FLOPPY:
	if(kl_streq(gdev.type, "fd") && isdigit(gdev.p1[0]) && !gdev.p2[0]) {
		return kl_sprintf("fd%d", atoi(gdev.p1));
	}
	
	return NULL;
}

/* Load a device.map file */
static void load_devmap(char const *path) {
	FILE *fh = vfs_fopen(path, "r");
	if(!fh) {
		printD("Error opening device.map: %s", kl_strerror(errno));
		return;
	}
	
	int line = 0, i;
	char buf[1024], *name, *val;
	kl_gdev gdev;
	
	while(fgets(buf, 1024, fh)) {
		buf[strcspn(buf, "\r\n")] = '\0';
		line++;
		
		name = buf+strspn(buf, "\r\n\t ");
		val = next_value(name);
		
		if(*name == '\0' || *name == '#') {
			continue;
		}
		
		i = strlen(name);
		
		if(*name != '(' || name[i-1] != ')' || !*val) {
			printD("Syntax error at device.map:%d", line);
			continue;
		}
		
		name[i-1] = '\0';
		name++;
		
		if(!parse_gdev(&gdev, name)) {
			printD("Invalid GRUB device at device.map:%d", line);
			continue;
		}
		
		strlcpy(gdev.device, val, sizeof(gdev.device));
		list_add_copy(&grub_devmap, &gdev, sizeof(gdev));
	}
	if(ferror(fh)) {
		printD("Error reading device.map: %s", strerror(errno));
	}
	
	fclose(fh);
}

#define GRUB_CONV_PATH(dest, src) \
	if(src[0] == '(') { \
		char *dev = lookup_gdev(src); \
		if(!dev) { \
			printD("menu.lst:%d: Invalid GRUB device", lnum); \
			continue; \
		} \
		\
		snprintf(dest, sizeof(dest), "(%s)%s", dev, strchr(src, ')')+1); \
		free(dev); \
	}else{ \
		strlcpy(dest, src, sizeof(dest)); \
	}

#define GRUB_CHECK_TOPEN() \
	if(!topen) { \
		printD("menu.lst:%u: %s must come after a title directive", lnum, name); \
		continue; \
	}

#define GRUB_CHECK_ARG() \
	if(!val[0]) { \
		printD("menu.lst:%u: %s requires an argument", lnum, name); \
		continue; \
	}

#define GRUB_ADD_TARGET() \
	if(topen && !tskip) { \
		if(!target.root[0]) { \
			printD("menu.lst:%d: No root device specified", topen); \
		} \
		if(!target.kernel[0]) { \
			printD("menu.lst:%d: No kernel specified", topen); \
		} \
		if(!target.root[0] || !target.kernel[0]) { \
			list_nuke(target.modules); \
		}else{ \
			list_add_copy(&targets, &target, sizeof(target)); \
		} \
	}

/* Load GRUB menu.lst */
static void load_menu(char const *path) {
	FILE *fh = vfs_fopen(path, "r");
	if(!fh) {
		printD("Error opening menu.lst: %s", kl_strerror(errno));
		return;
	}
	
	int lnum = 0, topen = 0, defnum = -1, tnum = 0, tskip;
	char buf[1024], *name, *val;
	kl_target target;
	kl_module mod;
	
	while(fgets(buf, 1024, fh)) {
		buf[strcspn(buf, "\r\n")] = '\0';
		lnum++;
		
		name = buf+strspn(buf, "\r\n\t ");
		val = next_value(name);
		
		if(*name == '\0' || *name == '#') {
			continue;
		}
		
		if(kl_streq(name, "timeout") && timeout == -1) {
			GRUB_CHECK_ARG();
			timeout = atoi(val);
		}
		
		if(kl_streq(name, "default")) {
			GRUB_CHECK_ARG();
			defnum = atoi(val);
		}
		
		if(kl_streq(name, "title")) {
			GRUB_CHECK_ARG();
			
			GRUB_ADD_TARGET();
			
			INIT_TARGET(&target);
			strlcpy(target.title, val, sizeof(target.title));
			target.flags |= TARGET_RESET;
			topen = lnum;
			tskip = 0;
			
			if(tnum++ == defnum) {
				target.flags |= TARGET_DEFAULT;
			}
		}
		
		if(kl_streq(name, "root")) {
			GRUB_CHECK_TOPEN();
			
			if(val[0]) {
				char *dev = lookup_gdev(val);
				if(!dev) {
					printD("menu.lst:%d: Invalid GRUB device", lnum);
					continue;
				}
				
				strlcpy(target.root, dev, sizeof(target.root));
				free(dev);
			}else{
				debug("Empty root device at %d, ignoring target", lnum);
				tskip = 1;
			}
		}
		
		if(kl_streq(name, "kernel")) {
			GRUB_CHECK_TOPEN();
			GRUB_CHECK_ARG();
			
			strlcpy(target.cmdline, next_value(val), sizeof(target.cmdline));
			GRUB_CONV_PATH(target.kernel, val);
		}
		
		if(kl_streq(name, "initrd")) {
			GRUB_CHECK_TOPEN();
			GRUB_CHECK_ARG();
			
			GRUB_CONV_PATH(target.initrd, val);
		}
		
		if(kl_streq(name, "module")) {
			GRUB_CHECK_TOPEN();
			GRUB_CHECK_ARG();
			
			INIT_MODULE(&mod);
			
			strlcpy(mod.args, next_value(val), sizeof(mod.args));
			GRUB_CONV_PATH(mod.name, val);
			
			list_add_copy(&target.modules, &mod, sizeof(mod));
		}
		
		if(kl_streq(name, "chainloader")) {
			tskip = 1;
		}
	}
	if(ferror(fh)) {
		printD("Error reading menu.lst: %s", strerror(errno));
	}
	
	GRUB_ADD_TARGET();
	
	fclose(fh);
}

static int grub2_val(char **cur, char **next, int lnum) {
	if(**cur == '\'' || **cur == '\"') {
		if(!(*next = strchr((*cur) + 1, **cur))) {
			printD("grub.cfg:%d missing terminating '%c'", lnum, **cur);
			return 0;
		}
		
		(*cur)++;
		
		
	}else{
		*next = *cur + strcspn(*cur, "\t =");
	}
	
	if(**next) {
		**next = '\0';
		
		(*next)++;
		*next += strspn(*next, "\t =");
	}
	
	return 1;
}

static void load_grub2_cfg(const char *filename) {
	FILE *fh = vfs_fopen(filename, "r");
	if(!fh) {
		printD("Error opening grub.cfg: %s", kl_strerror(errno));
		return;
	}
	
	char buf[1024], *l_start;
	int lnum = 0, entry_start = 0, skip_entry = 0;
	
	char default_title_buf[256], *default_title = NULL;
	int default_index = -1, cur_index = 0;
	
	kl_target target;
	
	INIT_TARGET(&target);
	
	while(fgets(buf, sizeof(buf), fh)) {
		lnum++;
		
		l_start = buf + strspn(buf, "\r\n\t ");
		l_start[strcspn(l_start, "\r\n")] = '\0';
		
		if(l_start[0] == '#' || l_start[0] == '\0') {
			continue;
		}
		
		char *cmd = l_start;
		char *args = cmd + strcspn(cmd, "\t ");
		
		if(args[0]) {
			*(args++) = '\0';
			args += strspn(args, "\t ");
		}
		
		if(kl_streq(cmd, "menuentry")) {
			char *title = args, *s;
			
			if(!grub2_val(&title, &s, lnum)) {
				goto END;
			}
			
			strlcpy(target.title, title, sizeof(target.title));
			
			if(cur_index++ == default_index || (default_title && kl_streq(default_title, title))) {
				target.flags = TARGET_DEFAULT;
			}
			
			entry_start = lnum;
			skip_entry = 0;
		}else if(kl_streq(cmd, "}") && entry_start) {
			if(!skip_entry) {
				if(!target.root[0]) {
					printD("grub.cfg:%d: No root device specified", entry_start);
				}
				
				if(!target.kernel[0]) {
					printD("grub.cfg:%d: No kernel specified", entry_start);
				}
				
				if(!target.root[0] || !target.kernel[0]) {
					list_nuke(target.modules);
				}else{
					list_add_copy(&targets, &target, sizeof(target));
				}
			}
			
			INIT_TARGET(&target);
			
			entry_start = 0;
		}else if(kl_streq(cmd, "chainloader")) {
			printd("chainloader at grub.cfg:%d, ignoring entry", lnum);
			skip_entry = 1;
		}else if(kl_streq(cmd, "set")) {
			char *env_name = args, *env_val, *s;
			
			if(!grub2_val(&env_name, &env_val, lnum) || !grub2_val(&env_val, &s, lnum)) {
				goto END;
			}
			
			if(kl_streq(env_name, "root")) {
				strlcpy(target.root, env_val, sizeof(target.root));
			}else if(kl_streq(env_name, "default")) {
				if(env_val[strspn(env_val, "1234567890")] == '\0') {
					/* Value is an integer */
					
					default_index = atoi(env_val);
					default_title = NULL;
				}else{
					default_index = -1;
					default_title = NULL;
					
					if(strchr(env_val, '>')) {
						printD("grub.cfg:%d 'default' using title of submenu item not supported", lnum);
						continue;
					}
					
					strlcpy(default_title_buf, env_val, sizeof(default_title_buf));
					default_title = default_title_buf;
				}
			}else if(kl_streq(env_name, "timeout") && timeout == -1) {
				timeout = atoi(env_val);
			}
		}else if(kl_streq(cmd, "linux") || kl_streq(cmd, "linux16")) {
			char *kernel = args, *k_args;
			
			if(!grub2_val(&kernel, &k_args, lnum)) {
				goto END;
			}
			
			strlcpy(target.kernel, kernel, sizeof(target.kernel));
			strlcpy(target.append, k_args, sizeof(target.append));
		}else if(kl_streq(cmd, "initrd") || kl_streq(cmd, "initrd16")) {
			char *initrd = args, *s;
			
			if(!grub2_val(&initrd, &s, lnum)) {
				goto END;
			}
			
			strlcpy(target.initrd, initrd, sizeof(target.initrd));
		}else if(kl_streq(cmd, "search") || kl_streq(cmd, "search.file") || kl_streq(cmd, "search.fs_label") || kl_streq(cmd, "search.fs_uuid")) {
			enum {
				s_unknown = 0,
				s_file,
				s_fs_label,
				s_fs_uuid
			} search_by = s_unknown;
			
			if(kl_streq(cmd, "search.file")) {
				search_by = s_file;
			}else if(kl_streq(cmd, "search.fs_label")) {
				search_by = s_fs_label;
			}else if(kl_streq(cmd, "search.fs_uuid")) {
				search_by = s_fs_uuid;
			}
			
			int set_root = 0, l_set = 0;
			
			while(*args) {
				int len = strcspn(args, "\t ");
				
				/* TODO: Handle ' and " (?) */
				
				char *next = args + len;
				next += strspn(next, "\t ");
				
				int is_last = (next[0] == '\0');
				
				if(strncmp(args, "-f", len) == 0 || strncmp(args, "--file", len) == 0) {
					search_by = s_file;
				}else if(strncmp(args, "-l", len) == 0 || strncmp(args, "--label", len) == 0) {
					search_by = s_fs_label;
				}else if(strncmp(args, "-u", len) == 0 || strncmp(args, "--fs-uuid", len) == 0) {
					search_by = s_fs_uuid;
				}else if(strncmp(args, "--set", len) == 0) {
					l_set = 1;
				}else if(l_set && (is_last || strncmp(args, "root", len) == 0)) {
					set_root = 1;
					l_set = 0;
				}
				
				if(is_last) {
					break;
				}
				
				args = next;
			}
			
			if(set_root) {
				switch(search_by) {
					case s_unknown:
						printD("grub.cfg:%d: Unknown search type", lnum);
						break;
						
					case s_file:
						printD("grub.cfg:%d: search with --file unsupported", lnum);
						break;
						
					case s_fs_label:
						snprintf(target.root, sizeof(target.root), "LABEL=%s", args);
						break;
						
					case s_fs_uuid:
						snprintf(target.root, sizeof(target.root), "UUID=%s", args);
						break;
						
					default:
						break;
				}
			}
		}
	}
	
	if(ferror(fh)) {
		printD("Error reading grub.cfg: %s", kl_strerror(errno));
	}
	
	END:
	fclose(fh);
}

/* Load GRUB device.map and menu.lst */
void grub_load(const char *grub_root) {
	char *device = get_diskid("", grub_root);
	const kl_disk *disk = mount_by_id(device, -1);
	
	if(disk) {
		char path[1024];
		
		snprintf(path, sizeof(path), "%s/device.map", grub_root);
		
		if(vfs_exists(path)) {
			load_devmap(path);
		}
		
		snprintf(path, sizeof(path), "%s/grub.cfg", grub_root);
		
		if(vfs_exists(path)) {
			load_grub2_cfg(path);
		}else{
			snprintf(path, sizeof(path), "%s/menu.lst", grub_root);
			
			if(vfs_exists(path)) {
				load_menu(path);
			}else{
				printD("Neither grub.cfg or menu.lst found in GRUB directory");
			}
		}
	}
	
	free(device);
	return;
}

void grub_detect(void) {
	printd("Searching for GRUB installation... (Press any key to abort)");
	int run = 1;
	
	while(run) {
		struct pollfd pollfds;
		pollfds.fd = fileno(stdin);
		pollfds.events = POLLIN;
		
		if(poll(&pollfds, 1, 1000)) {
			console_getchar();
			printd("GRUB autodetection aborted by keypress");
			
			return;
		}
		
		kl_disk *disks = get_disks(NULL), *disk;
		
		for(disk = disks; disk && run;) {
			char *path1 = kl_sprintf("(%s)/boot/grub/", disk->name);
			char *path2 = kl_sprintf("(%s)/grub/", disk->name);
			
			if(vfs_exists(path1)) {
				printd("Found GRUB installation at %s", path1);
				run = 0;
				
				grub_load(path1);
			}else if(vfs_exists(path2)) {
				printd("Found GRUB installation at %s", path2);
				run = 0;
				
				grub_load(path2);
			}
			
			free(path1);
			free(path2);
			
			disk = disk->next;
		}
	}
}

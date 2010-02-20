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
	}else if(isdigit(*src)) { \
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

/* Load GRUB device.map and menu.lst */
void grub_load(void) {
	if(!grub_path[0]) {
		return;
	}
	
	char *device = get_diskid("", grub_path);
	const kl_disk *disk = mount_by_id(device, -1);
	
	if(disk) {
		char path[1024];
		
		snprintf(path, sizeof(path), "%s/device.map", grub_path);
		load_devmap(path);
		
		snprintf(path, sizeof(path), "%s/menu.lst", grub_path);
		load_menu(path);
	}
	
	free(device);
	return;
}

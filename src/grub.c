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

kl_gdev *grub_devmap = NULL;

#define FOOBAR(first, x) \
	if(!first && islower(*src)) { \
		x[0] = *src; \
		x[1] = '\0'; \
		src++; \
	}else if(isdigit(*src)) { \
		for(i = 0; isdigit(*src); i++) { \
			if(i+1 < sizeof(x)) { \
				x[i++] = *src; \
				x[i] = '\0'; \
			} \
			src++; \
		} \
	}else{ \
		return 0; \
	} \
	if(*src == '\0') { \
		return 1; \
	}else if(*src == ',') { \
		src++; \
	}else{ \
		return 0; \
	}

/* Parse a GRUB device name
 * Returns 1 on success, zero on syntax error
*/
static int parse_gdev(kl_gdev *dest, char const *src) {
	if((*src != 'f' && *src != 'h') || src[1] != 'd') {
		return 0;
	}
	
	INIT_GDEV(dest);
	
	strlcpy(dest->type, src, 3);
	src += 2;
	
	int i;
	
	FOOBAR(1, dest->p1);
	FOOBAR(0, dest->p2);
	FOOBAR(0, dest->p3);
	
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
static void load_devmap(char const *root) {
	char filename[1024];
	snprintf(filename, 1024, "%s/device.map", root);
	
	FILE *fh = fopen(filename, "r");
	if(!fh) {
		printD("Error opening device.map: %s", strerror(errno));
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

/* Load GRUB menu.lst */
static void load_menu(char const *root) {
	char filename[1024], *fname = "menu.lst";
	snprintf(filename, 1024, "%s/menu.lst", root);
	
	FILE *fh = fopen(filename, "r");
	if(!fh) {
		printD("Error opening menu.lst: %s", strerror(errno));
		return;
	}
	
	int lnum = 0, topen = 0, defnum = -1, i = 0;
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
		
		if(kl_streq(name, "timeout") && !timeout) {
			CHECK_HASARG();
			
			timeout = atoi(val);
			continue;
		}
		if(kl_streq(name, "default")) {
			CHECK_HASARG();
			
			defnum = atoi(val);
			continue;
		}
		
		if(kl_streq(name, "title")) {
			CHECK_HASARG();
			
			if(topen) {
				ADD_TARGET();
			}
			
			INIT_TARGET(&target);
			strlcpy(target.title, val, sizeof(target.title));
			target.flags |= TARGET_RESET;
			topen = lnum;
			
			if(i++ == defnum) {
				target.flags |= TARGET_DEFAULT;
			}
			
			continue;
		}
		if(kl_streq(name, "root")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			CHECK_GDEV();
			
			strlcpy(target.root, val, sizeof(target.root));
			continue;
		}
		if(kl_streq(name, "kernel")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			CHECK_VPATH();
			
			strlcpy(target.cmdline, next_value(val), sizeof(target.cmdline));
			strlcpy(target.kernel, val, sizeof(target.kernel));
			continue;
		}
		if(kl_streq(name, "initrd")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			CHECK_VPATH();
			
			strlcpy(target.initrd, val, sizeof(target.initrd));
			continue;
		}
		if(kl_streq(name, "module")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			CHECK_VPATH();
			
			INIT_MODULE(&mod);
			strlcpy(mod.args, next_value(val), sizeof(mod.args));
			strlcpy(mod.name, val, sizeof(mod.name));
			list_add_copy(&target.modules, &mod, sizeof(mod));
			
			continue;
		}
	}
	if(ferror(fh)) {
		printD("Error reading menu.lst: %s", strerror(errno));
	}
	
	if(topen) {
		ADD_TARGET();
	}
	
	fclose(fh);
}

/* Load GRUB device.map and menu.lst */
void grub_load(void) {
	if(!grub_path[0]) {
		return;
	}
	
	char const *errmsg;
	char *root = get_rpath("", grub_path, &errmsg);
	if(!root) {
		printD("Error mounting GRUB device: %s", errmsg);
		return;
	}
	
	load_devmap(root);
	load_menu(root);
	
	free(root);
	return;
}

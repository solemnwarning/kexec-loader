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

kl_gdev *grub_devmap = NULL;

#define FOOBAR(x) \
	if(islower(*src)) { \
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
int parse_gdev(kl_gdev *dest, char const *src) {
	if((*src != 'f' && *src != 'h') || src[1] != 'd') {
		return 0;
	}
	
	INIT_GDEV(dest);
	
	strlcpy(dest->type, src, 3);
	src += 2;
	
	int i;
	
	FOOBAR(dest->p1);
	FOOBAR(dest->p2);
	FOOBAR(dest->p3);
	
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

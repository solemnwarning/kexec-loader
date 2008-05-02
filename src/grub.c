/* kexec-loader - GRUB functions
 * Copyright (C) 2007,2008 Daniel Collins <solemnwarning@solemnwarning.net>
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
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <unistd.h>
#include <stdio.h>

#include "config.h"
#include "grub.h"
#include "mount.h"
#include "../config.h"
#include "console.h"
#include "misc.h"

static struct grub_device {
	char device[DEVICE_SIZE];
	char fname[DEVICE_SIZE];
	
	struct grub_device *next;
} *grub_devices = NULL;

static void load_devices(void);
static void free_devices(void);

/* Reload GRUB menu.lst and device.map */
void grub_loadcfg(void) {
	free_devices();
	
	char const *errmsg;
	if((errmsg = mount_dev(config.grub_root, "/mnt/grub"))) {
		printD("Can't mount %s at /mnt/grub: %s", config.grub_root, errmsg);
		return;
	}
	
	load_devices();
	
	if(umount("/mnt/grub") == -1) {
		debug("Can't unmount /mnt/grub: %s", strerror(errno));
	}
}

/* Load devices from device.map */
static void load_devices(void) {
	char *devmap = NULL;
	char line[256], *name, *value;
	struct grub_device *ptr;
	
	if(access("/mnt/grub/grub/device.map", F_OK) == 0) {
		devmap = "/mnt/grub/grub/device.map";
	}
	if(access("/mnt/grub/boot/grub/device.map", F_OK) == 0) {
		devmap = "/mnt/grub/boot/grub/device.map";
	}
	if(!devmap) {
		return;
	}
	
	FILE *fh = fopen(devmap, "r");
	if(!fh) {
		printD("Can't open %s: %s", devmap, strerror(errno));
		return;
	}
	
	while(fgets(line, 256, fh)) {
		name = line+strspn(line, " \t\n\r");
		
		if(name[0] == '\0' || name[0] == '#') {
			continue;
		}
		
		value = name+strcspn(name, " \t");
		value += strspn(value, " \t");
		value[strcspn(value, "\r\n")] = '\0';
		
		if(!(ptr = malloc(sizeof(struct grub_device)))) {
			printD("device.map: malloc() failure, aborting");
			break;
		}
		
		strncpy(ptr->device, name, DEVICE_SIZE);
		strncpy(ptr->fname, value, DEVICE_SIZE);
		ptr->device[DEVICE_SIZE-1] = '\0';
		ptr->fname[DEVICE_SIZE-1] = '\0';
		
		ptr->next = grub_devices;
		grub_devices = ptr;
	}
	
	fclose(fh);
	return;
}

/* Free all nodes from grub_devices */
static void free_devices(void) {
	struct grub_device *dptr;
	
	while(grub_devices) {
		dptr = grub_devices;
		grub_devices = grub_devices->next;
		
		free(dptr);
	}
}

/* Attempt to convert a GRUB device to a Linux device name
 * Returns NULL if the conversion fails
*/
char *grub_cdevice(char const *gdev) {
	struct grub_device *ptr = grub_devices;
	while(ptr) {
		if(str_compare(ptr->device, gdev, 0)) {
			return strclone(ptr->fname, 9999);
		}
		
		ptr = ptr->next;
	}
	
	if(str_compare("(fd0)", gdev, 0)) {
		return "/dev/fd0";
	}
	if(str_compare("(fd1)", gdev, 0)) {
		return "/dev/fd1";
	}
	
	return NULL;
}

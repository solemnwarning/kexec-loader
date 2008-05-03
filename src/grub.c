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

static char c_title[NAME_SIZE] = {'\0'};
static char c_root[DEVICE_SIZE] = {'\0'};
static char c_kernel[KERNEL_SIZE] = {'\0'};
static char c_append[APPEND_SIZE] = {'\0'};
static char c_initrd[INITRD_SIZE] = {'\0'};
static int c_flags = 0;

static void load_devices(void);
static void free_devices(void);
static void load_menu(void);
static void add_target(void);

/* Reload GRUB menu.lst and device.map */
void grub_loadcfg(void) {
	free_devices();
	
	char const *errmsg;
	if((errmsg = mount_dev(config.grub_root, "/mnt/grub"))) {
		printD("Can't mount %s at /mnt/grub: %s", config.grub_root, errmsg);
		return;
	}
	
	load_devices();
	load_menu();
	
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
		debug("device.map not found\n");
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
		
		if(value[0] != '\0') {
			value[0] = '\0';
			value++;
			
			value += strspn(value, " \t");
			value[strcspn(value, "\r\n")] = '\0';
		}
		
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
		
		debug("Added GRUB device map: '%s' => '%s'\n", ptr->device, ptr->fname);
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
		return strclone("/dev/fd0", 99);
	}
	if(str_compare("(fd1)", gdev, 0)) {
		return strclone("/dev/fd1", 99);
	}
	
	return NULL;
}

/* Load targets from menu.lst */
static void load_menu(void) {
	char *menu = NULL;
	char line[1024], *name, *value;
	int dnum = -1, tnum = 0;
	size_t len;
	
	if(access("/mnt/grub/grub/menu.lst", F_OK) == 0) {
		menu = "/mnt/grub/grub/menu.lst";
	}
	if(access("/mnt/grub/boot/grub/menu.lst", F_OK) == 0) {
		menu = "/mnt/grub/boot/grub/menu.lst";
	}
	if(!menu) {
		printD("menu.lst not found");
		return;
	}
	
	FILE *fh = fopen(menu, "r");
	if(!fh) {
		printD("Can't open menu.lst: %s", strerror(errno));
		return;
	}
	
	while(fgets(line, 1024, fh)) {
		name = line+strspn(line, " \t\r\n");
		value = name+strcspn(name, " \t\r\n");
		
		if(value[0] != '\0') {
			value[0] = '\0';
			value++;
			
			value += strspn(value, " \t");
			value[strcspn(value, "\n\r")] = '\0';
		}
		
		if(str_compare(name, "root", 0)) {
			strncpy(c_root, value, DEVICE_SIZE);
			c_root[DEVICE_SIZE-1] = '\0';
		}
		if(str_compare(name, "kernel", 0)) {
			if((len = strcspn(value, " \t")) > NAME_SIZE) {
				len = NAME_SIZE;
			}
			
			strncpy(c_kernel, value, len);
			c_kernel[len] = '\0';
			
			value += strcspn(value, " \t");
			value += strspn(value, " \t");
				
			if(value[0] != '\0') {
				strncpy(c_append, value, APPEND_SIZE);
				c_append[APPEND_SIZE-1] = '\0';
			}
		}
		if(str_compare(name, "initrd", 0)) {
			strncpy(c_initrd, value, INITRD_SIZE);
			c_initrd[INITRD_SIZE-1] = '\0';
		}
		if(str_compare(name, "default", 0)) {
			dnum = atoi(value);
		}
		if(str_compare(name, "title", 0)) {
			if(c_title[0] != '\0') {
				add_target();
			}
			
			strncpy(c_title, value, NAME_SIZE);
			c_title[NAME_SIZE-1] = '\0';
			
			if(++tnum == dnum) {
				c_flags = TARGET_DEFAULT;
			}
		}
		if(str_compare(name, "chainloader", 0)) {
			c_title[0] = '\0';
			c_root[0] = '\0';
			c_kernel[0] = '\0';
			c_append[0] = '\0';
			c_initrd[0] = '\0';
			c_flags = 0;
			
			tnum++;
		}
	}
	if(c_title[0] != '\0') {
		add_target();
	}
	
	fclose(fh);
}

/* Add new target and zero the c_ variables */
static void add_target(void) {
	char *kernel = c_kernel, k_device[DEVICE_SIZE];
	char *initrd = c_initrd, i_device[DEVICE_SIZE];
	size_t len;
	
	if(kernel[0] == '(') {
		if((len = strcspn(kernel, ")")+1) > DEVICE_SIZE) {
			len = DEVICE_SIZE;
		}
		
		strncpy(k_device, kernel, len);
		k_device[len] = '\0';
		kernel += len;
	}else{
		strcpy(k_device, c_root);
	}
	
	if(initrd[0] == '(') {
		if((len = strcspn(initrd, ")")+1) > DEVICE_SIZE) {
			len = DEVICE_SIZE;
		}
		
		strncpy(i_device, initrd, len);
		i_device[len] = '\0';
		initrd += len;
	}else{
		strcpy(i_device, c_root);
	}
	
	if(kernel[0] == '\0') {
		printD("No kernel specified for '%s'", c_title);
		goto END;
	}
	
	kl_target *nptr = allocate(sizeof(kl_target));
	TARGET_DEFAULTS(nptr);
	
	strcpy(nptr->name, c_title);
	nptr->flags = c_flags;
	snprintf(nptr->kernel, KERNEL_SIZE, "/mnt/grub/%s", kernel);
	strcpy(nptr->append, c_append);
	
	nptr->mounts = allocate(sizeof(kl_mount));
	MOUNT_DEFAULTS(nptr->mounts);
	strcpy(nptr->mounts->device, k_device);
	strcpy(nptr->mounts->mpoint, "/mnt/grub");
	
	if(initrd[0] != '\0') {
		if(str_compare(i_device, k_device, 0)) {
			snprintf(nptr->initrd, INITRD_SIZE, "/mnt/grub/%s", initrd);
		}else{
			nptr->mounts->next = allocate(sizeof(kl_mount));
			MOUNT_DEFAULTS(nptr->mounts->next);
			strcpy(nptr->mounts->next->device, i_device);
			strcpy(nptr->mounts->next->mpoint, "/mnt/grub_i");
			
			snprintf(nptr->initrd, INITRD_SIZE, "/mnt/grub_i/%s", initrd);
		}
	}
	
	kl_target *eptr = config.targets;
	if(!config.targets) {
		config.targets = nptr;
	}else{
		while(eptr->next) {
			eptr = eptr->next;
		}
		
		eptr->next = nptr;
	}
	
	END:
	c_root[0] = '\0';
	c_kernel[0] = '\0';
	c_append[0] = '\0';
	c_initrd[0] = '\0';
	c_flags = 0;
}

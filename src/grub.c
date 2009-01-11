/* kexec-loader - GRUB functions
 * Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
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
#include "mystring.h"

static struct grub_device {
	char *device;
	char *fname;
	
	struct grub_device *next;
} *grub_devices = NULL;

static kl_target g_target = TARGET_DEFAULTS_DEFINE;
static char *g_root = NULL;

static void load_devices(void);
static void free_devices(void);
static void load_menu(void);
static void add_target(void);

/* Reload GRUB menu.lst and device.map */
void grub_loadcfg(void) {
	free_devices();
	
	if(!config.grub_root) {
		return;
	}
	
	printd(GREEN, 1, "Mounting %s at /mnt/grub...", config.grub_root);
	
	char const *errmsg;
	if((errmsg = mount_dev(config.grub_root, "/mnt/grub"))) {
		printD(RED, 2, "Mount failed: %s", errmsg);
		return;
	}
	
	load_devices();
	load_menu();
	
	if(umount("/mnt/grub") == -1) {
		debug("Can't unmount /mnt/grub: %s\n", strerror(errno));
	}
}

/* Load devices from device.map */
static void load_devices(void) {
	char *devmap = NULL;
	char line[256], *name, *value;
	struct grub_device *ptr;
	
	printd(GREEN, 1, "Searching for device.map...");
	
	if(access("/mnt/grub/grub/device.map", F_OK) == 0) {
		devmap = "/mnt/grub/grub/device.map";
	}
	if(access("/mnt/grub/boot/grub/device.map", F_OK) == 0) {
		devmap = "/mnt/grub/boot/grub/device.map";
	}
	if(!devmap) {
		printd(RED, 2, "device.map not found");
		return;
	}
	
	printd(GREEN, 1, "Loading device.map...");
	
	FILE *fh = fopen(devmap, "r");
	if(!fh) {
		printD(RED, 2, "Can't open %s: %s", devmap, strerror(errno));
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
		
		ptr = allocate(sizeof(struct grub_device));
		
		str_copy(&ptr->device, name, -1);
		str_copy(&ptr->fname, value, -1);
		
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
	char *devbuf = NULL, *devbuf2 = NULL;
	
	struct grub_device *ptr = grub_devices;
	while(ptr) {
		if(str_eq(ptr->device, gdev, -1)) {
			return str_copy(NULL, ptr->fname, -1);
		}
		
		if(
			globcmp(gdev, "(hd*,*)", GLOB_STAR | GLOB_SINGLE) &&
			globcmp(ptr->device, "(hd*)", GLOB_STAR | GLOB_SINGLE) &&
			atoi(gdev+3) == atoi(ptr->device+3)
		) {
			return str_printf("%s%d", ptr->fname, atoi(strchr(gdev, ',')+1)+1);
		}
		
		ptr = ptr->next;
	}
	
	if(str_eq("(fd0)", gdev, -1)) {
		return str_copy(NULL, "/dev/fd0", -1);
	}
	if(str_eq("(fd1)", gdev, -1)) {
		return str_copy(NULL, "/dev/fd1", -1);
	}
	
	if(!globcmp(gdev, "(hd*)", GLOB_STAR | GLOB_SINGLE)) {
		return NULL;
	}
	
	int disknum = atoi(gdev+3), partnum = -1;
	if(strchr(gdev, ',')) {
		partnum = atoi(strchr(gdev, ',')+1);
	}
	
	FILE *fh = fopen("/proc/diskstats", "r");
	if(!fh) {
		debug("Can't open /proc/diskstats: %s\n", strerror(errno));
		return NULL;
	}
	
	int step = config.grub_first, count = -1, n = -1, last = -1, i;
	char line[256], *device;
	
	RLOOP:
	rewind(fh);
	
	while(fgets(line, 256, fh)) {
		device = line+strspn(line, " \t1234567890");
		device[strcspn(device, " \t")] = '\0';
		
		if(
			(step == 0 && !globcmp(device, "hd?", GLOB_STAR | GLOB_SINGLE)) ||
			(step == 1 && !globcmp(device, "sd?", GLOB_STAR | GLOB_SINGLE))
		) {
			continue;
		}
		
		i = device[2]-48;
		
		if(i > last && (i < n || n == last)) {
			devbuf = str_printf("/dev/%s", device);
			n = i;
		}
	}
	if(n == last) {
		if(step == config.grub_first) {
			step = (step ? 0 : 1);
			n = last = -1;
			
			goto RLOOP;
		}else{
			fclose(fh);
			return NULL;
		}
	}
	if(++count != disknum) {
		last = n;
		goto RLOOP;
	}
	
	if(partnum != -1) {
		devbuf2 = devbuf;
		devbuf = str_printf("%s%d", devbuf2, partnum+1);
		free(devbuf2);
	}
	
	fclose(fh);
	return devbuf;
}

/* Load targets from menu.lst */
static void load_menu(void) {
	char *menu = NULL;
	char line[1024], *name, *value;
	int dnum = -1, tnum = 0;
	
	printd(GREEN, 1, "Searching for menu.lst...");
	
	if(access("/mnt/grub/grub/menu.lst", F_OK) == 0) {
		menu = "/mnt/grub/grub/menu.lst";
	}
	if(access("/mnt/grub/boot/grub/menu.lst", F_OK) == 0) {
		menu = "/mnt/grub/boot/grub/menu.lst";
	}
	if(!menu) {
		printD(RED, 2, "menu.lst not found");
		return;
	}
	
	printd(GREEN, 1, "Loading menu.lst...");
	
	FILE *fh = fopen(menu, "r");
	if(!fh) {
		printD(RED, 2, "Can't open %s: %s", menu, strerror(errno));
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
		
		if(str_eq(name, "root", -1)) {
			str_copy(&g_root, value, -1);
		}
		if(str_eq(name, "kernel", -1)) {
			str_copy(&g_target.kernel, value, strcspn(value, " \t"));
			
			value += strcspn(value, " \t");
			value += strspn(value, " \t");
				
			if(value[0] != '\0') {
				str_copy(&g_target.append, value, -1);
			}
		}
		if(str_eq(name, "initrd", -1)) {
			str_copy(&g_target.initrd, value, -1);
		}
		if(str_eq(name, "default", -1)) {
			dnum = atoi(value);
		}
		if(str_eq(name, "title", -1)) {
			if(g_target.kernel) {
				if(tnum++ == dnum) {
					g_target.flags |= TARGET_DEFAULT;
				}
				
				add_target();
			}
			
			str_copy(&g_target.name, value, -1);
		}
		if(str_eq(name, "chainload", -1)) {
			free(g_target.name);
			free(g_target.kernel);
			free(g_target.initrd);
			free(g_target.append);
			TARGET_DEFAULTS(&g_target);
			
			free(g_root);
			g_root = NULL;
			
			tnum++;
		}
		if(str_eq(name, "timeout", -1) && !config.timeout) {
			config.timeout = strtoul(value, NULL, 10);
		}
		if(str_eq(name, "module", -1)) {
			kl_module *nptr = allocate(sizeof(kl_module));
			INIT_MODULE(nptr);
			
			nptr->module = str_printf("/mnt/target/%s", value);
			nptr->next = g_target.modules;
			g_target.modules = nptr;
		}
	}
	if(g_target.name) {
		add_target();
	}else{
		free(g_target.name);
		free(g_target.kernel);
		free(g_target.initrd);
		free(g_target.append);
		TARGET_DEFAULTS(&g_target);
	}
	
	fclose(fh);
}

/* Add new target and zero the c_ variables */
static void add_target(void) {
	char *kernel = g_target.kernel, *k_device = NULL;
	char *initrd = g_target.initrd, *i_device = NULL;
	size_t len;
	
	if(!kernel) {
		printD(RED, 2, "No kernel specified for '%s'", g_target.name);
		goto ERROR;
	}
	if(kernel[0] != '(' && !g_root) {
		printD(RED, 2, "No kernel device specified for '%s'\n", g_target.name);
		goto ERROR;
	}
	if(initrd && initrd[0] != '(' && !g_root) {
		printD(RED, 2, "No initrd device specified for '%s'\n", g_target.name);
		goto ERROR;
	}
	
	if(kernel[0] == '(') {
		len = strcspn(kernel, ")")+1;
		
		str_copy(&k_device, kernel, len);
		kernel += len;
	}else{
		str_copy(&k_device, g_root, -1);
	}
	
	if(initrd) {
		if(initrd[0] == '(') {
			len = strcspn(initrd, ")")+1;
			
			str_copy(&i_device, initrd, len);
			initrd += len;
		}else{
			str_copy(&i_device, g_root, -1);
		}
	}
	
	kl_target *nptr = allocate(sizeof(kl_target));
	TARGET_DEFAULTS(nptr);
	
	nptr->name = g_target.name;
	nptr->flags = g_target.flags;
	nptr->append = g_target.append;
	
	nptr->kernel = str_printf("/mnt/grub/%s", kernel);
	free(g_target.kernel);
	
	nptr->mounts = allocate(sizeof(kl_mount));
	INIT_MOUNT(nptr->mounts);
	nptr->mounts->device = k_device;
	str_copy(&nptr->mounts->mpoint, "/mnt/grub", -1);
	
	if(initrd) {
		if(str_eq(i_device, k_device, -1)) {
			nptr->initrd = str_printf("/mnt/grub/%s", initrd);
		}else{
			nptr->mounts->next = allocate(sizeof(kl_mount));
			INIT_MOUNT(nptr->mounts->next);
			nptr->mounts->next->device = i_device;
			str_copy(&nptr->mounts->next->mpoint, "/mnt/grub_i", -1);
			
			nptr->initrd = str_printf("/mnt/grub_i/%s", initrd);
		}
		
		free(g_target.initrd);
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
	TARGET_DEFAULTS(&g_target);
	free(g_root);
	g_root = NULL;
	return;
	
	ERROR:
	free(g_target.name);
	free(g_target.kernel);
	free(g_target.append);
	goto END;
}

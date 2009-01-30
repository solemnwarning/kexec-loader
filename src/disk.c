/* kexec-loader - Disk functions
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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/kdev_t.h>
#include <blkid/blkid.h>
#include <sys/mount.h>
#include <poll.h>

#include "disk.h"
#include "misc.h"
#include "console.h"

#define FOOBAR(dest, name) \
	s = blkid_get_tag_value(NULL, name, path); \
	if(s) { \
		strlcpy(dest, s, sizeof(dest)); \
		free(s); \
	}

/* Return a list containing disks in /proc/diskstats */
kl_disk *get_disks(void) {
	FILE *fh = fopen("/proc/diskstats", "r");
	if(!fh) {
		debug("Error opening /proc/diskstats: %s", strerror(errno));
		return NULL;
	}
	
	kl_disk *list = NULL;
	kl_disk disk;
	
	char line[256], *name, path[256], *s;
	
	while(fgets(line, 256, fh)) {
		INIT_DISK(&disk);
		
		name = line+strspn(line, "\t ");
		disk.major = atoi(name);
		
		name += strspn(name, "1234567890");
		name += strspn(name, "\t ");
		disk.minor = atoi(name);
		
		name += strspn(name, "1234567890");
		name += strspn(name, "\t ");
		name[strcspn(name, "\t ")] = '\0';
		
		sprintf(path, "/dev/%s", name);
		
		if(kl_strneq(name, "ram", 3) || kl_strneq(name, "loop", 4)) {
			continue;
		}
		
		if(access(path, F_OK) && mknod(path, 0600 | S_IFBLK, MKDEV(disk.major, disk.minor))) {
			debug("Failed to create %s device node: %s", path, strerror(errno));
			continue;
		}
		
		strlcpy(disk.name, name, sizeof(disk.name));
		FOOBAR(disk.label, "LABEL");
		FOOBAR(disk.uuid, "UUID");
		FOOBAR(disk.fstype, "TYPE");
		
		list_add_copy(&list, &disk, sizeof(disk));
	}
	
	if(ferror(fh)) {
		debug("Error reading /proc/diskstats: %s", strerror(errno));
	}
	
	fclose(fh);
	return list;
}

/* Search for a disk by disk id */
kl_disk *find_disk(char const *id) {
	kl_disk *ptr = get_disks(), *ret = NULL;
	int match = 0;
	
	while(ptr) {
		if(!ret && kl_strnceq(id, "LABEL=", 6)) {
			if(ptr->label[0] && kl_streq(ptr->label, id+6)) {
				match = 1;
			}
		}else if(!ret && kl_strnceq(id, "UUID=", 5)) {
			if(ptr->uuid[0] && kl_streq(ptr->uuid, id+5)) {
				match = 1;
			}
		}else if(!ret && kl_streq(ptr->name, id)) {
			match = 1;
		}
		
		if(!ret && match) {
			ret = ptr;
			ptr = ptr->next;
		}else{
			list_del(&ptr, ptr);
		}
	}
	
	return ret;
}

/* Attempt to mount a disk
 * Returns NULL on success, otherwise an error message
*/
char const *mount_disk(kl_disk *disk, char const *mpoint) {
	char dev[256], dir[256];
	
	if(!disk->fstype) {
		return "Unknown filesystem format";
	}
	
	snprintf(dev, 256, "/dev/%s", disk->name);
	snprintf(dir, 256, "/mnt/%s", disk->name);
	
	if(!mpoint) {
		mpoint = dir;
	}
	
	mkdir(mpoint, 0700);
	
	if(mount(dev, mpoint, disk->fstype, MS_RDONLY, NULL)) {
		switch(errno) {
			case EBUSY:
				/* The device is already mounted */
				break;
				
			case EINVAL:
				return "Invalid argument (Invalid superblock?)";
				
			case ENODEV:
				return "No such device (Missing module?)";
				
			default:
				return strerror(errno);
		}
	}else{
		debug("Mounted %s at %s", dev, mpoint);
	}
	
	return NULL;
}

/* Check the syntax of a vpath
 * Returns 1 if valid, zero otherwise
*/
int check_vpath(char const *vpath) {
	if(*vpath == '(') {
		if(vpath[1] == ')' || !(vpath = strchr(vpath, ')'))) {
			return 0;
		}
		
		vpath++;
	}
	
	if(*vpath == '\0') {
		return 0;
	}
	
	return 1;
}

/* Mount the disk a vpath refers to and return the real path in a string
 * allocated on the heap, returns NULL if mount fails.
 *
 * You MUST pass a valid path, check with check_vpath()
*/
char *get_rpath(char const *root, char const *path, char const **error) {
	char *diskid = NULL, *rpath = NULL;
	
	if(*path == '(') {
		diskid = kl_strndup(path, strcspn(path+1, ")"));
		path += strcspn(path, ")")+1;
	}else{
		diskid = kl_strdup(root);
	}
	
	kl_disk *disk = find_disk(diskid);
	if(!disk) {
		*error = "Unknown device";
		goto END;
	}
	
	*error = mount_disk(disk, NULL);
	if(*error) {
		goto END;
	}
	
	rpath = kl_sprintf("/mnt/%s/%s", disk->name, path);
	
	END:
	free(diskid);
	free(disk);
	
	return rpath;
}

/* Format a vpath for display to the user
 * Returns a string on the heap
 *
 * You MUST pass a valid path, check with check_vpath()
*/
char *get_vpath(char const *root, char const *path) {
	char const *disk = root;
	int disklen = strlen(root);
	
	if(*path == '(') {
		disklen = strcspn(path, ")")+1;
		
		disk = path;
		path += disklen;
	}
	
	char *vpath = kl_malloc(strlen(path)+disklen+1);
	
	strncpy(vpath, disk, disklen);
	strcat(vpath, path);
	
	return vpath;
}

/* Unmount all filesystems mounted under /mnt
 * Writes errors to the debug log
*/
void unmount_all(void) {
	FILE *fh = fopen("/proc/mounts", "r");
	if(!fh) {
		debug("Error opening /proc/mounts: %s", strerror(errno));
		return;
	}
	
	char line[256], *mpoint;
	
	while(fgets(line, 256, fh)) {
		mpoint = next_value(line);
		next_value(mpoint); /* Terminate mpoint */
		
		if(!kl_strneq(mpoint, "/mnt/", 5)) {
			continue;
		}
		
		if(umount(mpoint)) {
			debug("Error unmounting %s: %s", mpoint, strerror(errno));
		}else{
			debug("Unmounted %s", mpoint);
		}
	}
	
	fclose(fh);
}

/* Search for and mount the boot disk at /mnt/boot
 * Returns 1 on success, zero on error/abort
*/
int mount_boot(void) {
	char *kdevice = get_cmdline("root");
	char *device = kdevice ? kdevice : "LABEL=kexecloader";
	kl_disk *disk = NULL;
	int rval = 0;
	
	struct pollfd pollset;
	pollset.fd = fileno(stdin);
	pollset.events = POLLIN;
	
	printm("Searching for boot disk...");
	printm("Press any key to abort");
	
	while(1) {
		if(poll(&pollset, 1, 1000)) {
			getchar();
			break;
		}
		
		disk = find_disk(device);
		if(!disk) {
			continue;
		}
		
		printm("Found boot disk: %s", disk->name);
		
		char const *errmsg = mount_disk(disk, "/mnt/boot");
		if(errmsg) {
			printD("Error mounting boot disk: %s", errmsg);
			break;
		}
		
		rval = 1;
		break;
	}
	
	free(disk);
	free(kdevice);
	return rval;
}

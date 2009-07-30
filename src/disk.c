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
#include <stdint.h>

#include "disk.h"
#include "misc.h"
#include "console.h"
#include "grub.h"

struct mounted_fs {
	struct mounted_fs *next;
	char mpoint[256];
};

static struct mounted_fs *mounts = NULL;

/* Get the size of a block device and format it as text
 * Copies "???" to dest on error
*/
static void get_dev_size(char *dest, int size, char const *path) {
	strcpy(dest, "???");
	
	int fd = open(path, O_RDONLY);
	if(fd == -1) {
		debug("Error opening %s: %s", path, strerror(errno));
		return;
	}
	
	uint64_t devsize;
	if(ioctl(fd, BLKGETSIZE64, &devsize) == -1) {
		debug("BLKGETSIZE64 on %s failed: %s", path, strerror(errno));
		goto END;
	}
	
	if(devsize > 1000000000000LLU) {
		snprintf(dest, size, "%.2fTB", (double)devsize / 1000000000000LLU);
	}else if(devsize > 1000000000) {
		snprintf(dest, size, "%.2fGB", (double)devsize / 1000000000);
	}else if(devsize > 1000000) {
		snprintf(dest, size, "%.2fMB", (double)devsize / 1000000);
	}else if(devsize > 1000) {
		snprintf(dest, size, "%.2fkB", (double)devsize / 1000);
	}else{
		snprintf(dest, size, "%llu", devsize);
	}
	
	END:
	close(fd);
}

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
		
		get_dev_size(disk.size, sizeof(disk.size), path);
		
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
	int match = 0, i;
	char *fstype = NULL;
	
	if(strchr(id, ':')) {
		i = strcspn(id, ":");
		fstype = kl_strndup(id, i);
		id += i+1;
	}
	
	if(kl_strneq(id, "/dev/", 5)) {
		id += 5;
	}
	
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
	
	if(ret && fstype) {
		strlcpy(ret->fstype, fstype, sizeof(ret->fstype));
	}
	
	free(fstype);
	return ret;
}

/* Attempt to mount a disk
 * Returns NULL on success, otherwise an error message
*/
char const *mount_disk(kl_disk *disk, char const *mpoint) {
	char dev[256], dir[256];
	struct mounted_fs x, *ptr = mounts;
	
	if(!disk->fstype) {
		return "Unknown filesystem format";
	}
	
	snprintf(dev, 256, "/dev/%s", disk->name);
	snprintf(dir, 256, "/mnt/%s", disk->name);
	
	if(!mpoint) {
		mpoint = dir;
	}
	
	while(ptr) {
		if(kl_streq(mpoint, ptr->mpoint)) {
			debug("%s is already mounted", mpoint);
			return NULL;
		}
		
		ptr = ptr->next;
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
		
		x.next = NULL;
		strlcpy(x.mpoint, mpoint, sizeof(x.mpoint));
		list_add_copy(&mounts, &x, sizeof(x));
	}
	
	return NULL;
}

/* Search for a device and mount it once it's found
 * Returns the kl_disk structure on success, or NULL on error/abort
*/
kl_disk *mount_retry(char const *device, char const *name) {
	kl_disk *disk = NULL;
	int rval = 0;
	
	struct pollfd pollset;
	pollset.fd = fileno(stdin);
	pollset.events = POLLIN;
	
	printd("Searching for %s...", name);
	printm("Press any key to abort");
	
	while(1) {
		if(poll(&pollset, 1, 1000)) {
			console_getchar();
			break;
		}
		
		disk = find_disk(device);
		if(!disk) {
			continue;
		}
		
		printd("Found %s: %s", name, disk->name);
		
		char const *errmsg = mount_disk(disk, NULL);
		if(errmsg) {
			printD("Error mounting %s: %s", disk->name, errmsg);
			break;
		}
		
		rval = 1;
		break;
	}
	
	if(!rval) {
		free(disk);
		disk = NULL;
	}
	
	return disk;
}

/* Unmount all filesystems */
void unmount_all(void) {
	struct mounted_fs *ptr = mounts, *dptr;
	
	while(ptr) {
		if(umount(ptr->mpoint)) {
			debug("Error unmounting %s: %s", ptr->mpoint, strerror(errno));
			ptr = ptr->next;
		}else{
			debug("Unmounted %s", ptr->mpoint);
			
			dptr = ptr;
			ptr = ptr->next;
			
			list_del(&mounts, dptr);
		}
	}
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
		diskid = kl_strndup(path+1, strcspn(path+1, ")"));
		path += strcspn(path, ")")+1;
	}else{
		diskid = kl_strdup(root);
	}
	
	kl_disk *disk = find_disk(diskid);
	if(!disk) {
		char *gdisk = lookup_gdev(diskid);
		if(gdisk) {
			free(diskid);
			diskid = gdisk;
			
			disk = find_disk(diskid);
		}
	}
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

/* Return the device in a vpath, fall back to root if vpath does not contain a
 * device, the return value is a string allocatedon the heap
*/
char *get_diskid(char const *root, char const *vpath) {
	char const *disk = root;
	int disklen = strlen(root);
	
	if(*vpath == '(') {
		disk = vpath+1;
		disklen = strcspn(disk, ")");
	}
	
	return kl_strndup(disk, disklen);
}

/* Return the path component of a vpath
 * The returned address points to the vpath string and should not be free()'d
*/
char *get_path(char const *vpath) {
	if(*vpath == '(') {
		return strchr(vpath, ')')+1;
	}
	
	return (char*)vpath;
}

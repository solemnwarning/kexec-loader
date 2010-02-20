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

static kl_disk *mounts = NULL;

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

#define BLKID_TAG(dest, name) \
if(!dest[0]) { \
	char *s = blkid_get_tag_value(NULL, name, path); \
	if(s) { \
		strlcpy(dest, s, sizeof(dest)); \
		free(s); \
	} \
}

/* Return a list containing disks in /proc/diskstats
 * Only returns first disk matching filter if not NULL
*/
kl_disk *get_disks(const char *filter) {
	FILE *fh = fopen("/proc/diskstats", "r");
	if(!fh) {
		debug("Error opening /proc/diskstats: %s", strerror(errno));
		return NULL;
	}
	
	kl_disk *list = NULL;
	kl_disk disk;
	
	char line[256], *name, path[256], *fstype = NULL;
	
	if(filter && strchr(filter, ':')) {
		fstype = kl_strndup(filter, strcspn(filter, ":"));
		filter = strchr(filter, ':')+1;
	}
	
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
		strlcpy(disk.name, name, sizeof(disk.name));
		
		if(kl_strneq(name, "ram", 3) || kl_strneq(name, "loop", 4)) {
			continue;
		}
		
		if(access(path, F_OK) && mknod(path, 0600 | S_IFBLK, MKDEV(disk.major, disk.minor))) {
			debug("Failed to create %s device node: %s", path, strerror(errno));
			continue;
		}
		
		if(filter) {
			if(kl_strnceq(filter, "LABEL=", 6)) {
				BLKID_TAG(disk.label, "LABEL");
			}
			if(kl_strnceq(filter, "UUID=", 5)) {
				BLKID_TAG(disk.uuid, "UUID");
			}
			
			if(!compare_disk_id(&disk, filter)) {
				continue;
			}
		}
		
		if(fstype) {
			strlcpy(disk.fstype, fstype, sizeof(disk.fstype));
		}
		
		BLKID_TAG(disk.label, "LABEL");
		BLKID_TAG(disk.uuid, "UUID");
		BLKID_TAG(disk.fstype, "TYPE");
		
		get_dev_size(disk.size, sizeof(disk.size), path);
		
		list_add_copy(&list, &disk, sizeof(disk));
		
		if(filter) {
			break;
		}
	}
	
	if(ferror(fh)) {
		debug("Error reading /proc/diskstats: %s", strerror(errno));
	}
	
	free(fstype);
	
	fclose(fh);
	return list;
}

/* Attempt to mount a disk
 * Returns 1 on success
 * Returns 0 and sets errno on failure
*/
int mount_disk(kl_disk *disk) {
	char dev[256], mpoint[256];
	kl_disk *ptr = mounts;
	
	if(!disk->fstype) {
		errno = EBADFS;
		return 0;
	}
	
	snprintf(dev, 256, "/dev/%s", disk->name);
	snprintf(mpoint, 256, "/mnt/%s", disk->name);
	
	while(ptr) {
		if(kl_streq(disk->name, ptr->name)) {
			debug("%s is already mounted", disk->name);
			return 1;
		}
		
		ptr = ptr->next;
	}
	
	mkdir(mpoint, 0700);
	
	if(mount(dev, mpoint, disk->fstype, MS_RDONLY, NULL) && errno != EBUSY) {
		return 0;
	}
	
	debug("Mounted %s at %s", dev, mpoint);
	list_add_copy(&mounts, disk, sizeof(*disk));
	
	return 1;
}

/* Mount a disk identified by a disk ID
 * Returns a pointer to the disk in the mounts list on success
 * Returns NULL and sets errno on failure
 *
 * Timeout is in seconds, zero will only try once, negative will try until
 * interrupted by keyboard input. If timeout is non-zero messages may be
 * printed to the console.
*/
const kl_disk *mount_by_id(const char *disk_id, int timeout) {
	kl_disk *disk = mounts;
	
	while(disk) {
		if(compare_disk_id(disk, disk_id)) {
			return disk;
		}
		
		disk = disk->next;
	}
	
	disk = get_disks(disk_id);
	
	if(!disk && timeout) {
		struct pollfd pollfds;
		pollfds.fd = fileno(stdin);
		pollfds.events = POLLIN;
		
		console_erase(ERASE_LINE);
		printf("\rWaiting for disk.... (Press any key to abort)");
		
		while(!disk && timeout--) {
			if(poll(&pollfds, 1, 1000)) {
				console_getchar();
				break;
			}
			
			disk = get_disks(disk_id);
		}
		
		if(disk) {
			console_erase(ERASE_LINE);
			printf("\rWaiting for disk.... Found\n");
		}else{
			console_erase(ERASE_LINE);
			printf("\rWaiting for disk.... Aborted\n");
		}
	}
	
	if(!disk) {
		errno = ENODEV;
		return NULL;
	}
	
	int ms = mount_disk(disk);
	free(disk);
	
	if(!ms) {
		return NULL;
	}
	
	for(disk = mounts; !compare_disk_id(disk, disk_id); disk = disk->next) {}
	
	return disk;
}

/* Unmount all filesystems */
void unmount_all(void) {
	kl_disk *ptr = mounts, *dptr;
	
	while(ptr) {
		char mpoint[256];
		snprintf(mpoint, 256, "/mnt/%s", ptr->name);
		
		if(umount(mpoint)) {
			debug("Error unmounting %s: %s", mpoint, strerror(errno));
			ptr = ptr->next;
		}else{
			debug("Unmounted %s", mpoint);
			
			dptr = ptr;
			ptr = ptr->next;
			
			list_del(&mounts, dptr);
		}
	}
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

int compare_disk_id(kl_disk *disk, const char *id) {
	if(kl_strnceq(id, "LABEL=", 6)) {
		if(disk->label[0] && kl_strceq(disk->label, id+6)) {
			return 1;
		}
	}
	
	if(kl_strnceq(id, "UUID=", 5)) {
		if(disk->uuid[0] && kl_strceq(disk->uuid, id+5)) {
			return 1;
		}
	}
	
	if(kl_strneq(id, "/dev/", 5)) {
		id += 5;
	}
	
	if(kl_streq(disk->name, id)) {
		return 1;
	}
	
	return 0;
}

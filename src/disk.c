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

#include "disk.h"
#include "misc.h"

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

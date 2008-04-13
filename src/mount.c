/* kexec-loader - Mount-related functions
 * Copyright (C) 2007, Daniel Collins <solemnwarning@solemnwarning.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "mount.h"
#include "misc.h"
#include "../config.h"
#include "config.h"

/* Unmount all mounts under a certain directory, including the directory if it
 * is a mount itself.
 *
 * Filesystems with a device of 'rootfs' are ignored since rootfs can never be
 * unmounted and it's harmless to leave mounted anyway.
 *
 * Filesystems mounted on /proc are also ignored since the /sbin/kexec program
 * needs /proc/iomem.
*/
void unmount_tree(char const* dir) {
	FILE* mfile = NULL;
	while(mfile == NULL) {
		if((mfile = fopen("/proc/mounts", "r")) != NULL) {
			break;
		}
		if(errno == EINTR) {
			continue;
		}
		
		printm("Can't open /proc/mounts: %s", strerror(errno));
		printm("Not umounting any filesystems!!");
		return;
	}
	
	char mounts[128][1024] = {{'\0'}};
	char line[1024] = {'\0'};
	size_t mcount = 0, mnum, mpos;
	
	char cmp2[1024] = {'\0'};
	snprintf(cmp2, 1023, "%s*", dir);
	
	while(fgets(line, 1024, mfile) != NULL && mcount < 128) {
		char* token = strtok(line, " ");
		if(str_compare(token, "rootfs", 0)) {
			continue;
		}
		if((token = strtok(NULL, " ")) == NULL) {
			printm("/proc/mounts is gobbledegook!");
			return;
		}
		if(!str_compare(token, cmp2, STR_WILDCARD2)) {
			continue;
		}
		if(str_compare(token, "/proc", STR_WILDCARD2)) {
			continue;
		}
		
		strncpy(mounts[mcount++], token, 1023);
	}
	
	while(fclose(mfile) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		printm("Can't close /proc/mounts: %s", strerror(errno));
	}
	
	while(mcount > 0) {
		char* mount = "";
		for(mpos = 0; mpos < 128; mpos++) {
			if(str_compare(mount, mounts[mpos], STR_MAXLEN, strlen(mount))) {
				mount = mounts[mpos];
				mnum = mpos;
			}
		}
		
		if(umount(mount) == -1) {
			printm("Can't umount %s: %s", mount, strerror(errno));
		}
		mounts[mnum][0] = '\0';
		mcount--;
	}
}

/* Mount disk containing CONFIG_FILE at /mnt
 * Returns 1 if device containing file was mounted, zero otherwise
*/
int mount_config(void) {
	char const* devices[] = {
		"/dev/fd0",
		"/dev/fd1",
		"/dev/sda",
		"/dev/sdb",
		"/dev/sdc",
		"/dev/sdd",
		NULL
	};
	
	unsigned int devnum = 0;
	char const* devname = devices[0];
	
	while(devname) {
		if(mount(devname, "/mnt", BOOTFS_TYPE, MS_RDONLY, NULL) == -1) {
			debug("Can't mount %s at /mnt: %s\n", devname, strerror(errno));
			goto ENDLOOP;
		}
		if(access("/mnt/" CONFIG_FILE, F_OK) == 0) {
			debug("Found " CONFIG_FILE " on %s\n", devname);
			return 1;
		}
		
		if(umount("/mnt") == -1) {
			debug("Can't unmount /mnt: %s\n", strerror(errno));
		}
		
		ENDLOOP:
		devname = devices[++devnum];
	}
	
	debug("Can't find disk containing " CONFIG_FILE "\n");
	return 0;
}

/* Mount all mounts in a kl_mount list
 *
 * If all mounts are sucessfully mounted 1 is returned, if any mounts fail zero
 * is returned any any already-completed mounts will not be unmounted
*/
int mount_list(kl_mount* mount_src) {
	kl_mount* mounts = NULL;
	kl_mount* cmount = NULL;
	kl_mount* mptr = NULL;
	char cmp1[1024] = {'\0'};
	
	if((mounts = mount_copy(mount_src)) == NULL) {
		printm("Copying mount list failed: %s", strerror(errno));
		return(0);
	}
	
	unsigned int count = 0;
	for(mptr = mounts; mptr != NULL; mptr = mptr->next) {
		count++;
	}
	
	while(count > 0) {
		cmount = mounts;
		mptr = mounts;
		
		while(cmount != NULL) {
			snprintf(cmp1, 1023, "%s*", cmount->mpoint);
			
			if(str_compare(cmp1, mptr->mpoint, STR_WILDCARD1)) {
				mptr = cmount;
			}
			cmount = cmount->next;
		}
		
		if(str_compare(mptr->fstype, "auto", 0)) {
			char *fstype = detect_fstype(mptr->device);
			if(!fstype) {
				printm("Unknown filesystem on %s\n", mptr->device);
				
				mount_free(&mounts);
				return(0);
			}
			
			strncpy(mptr->fstype, fstype, 63);
		}
		
		if(mount(mptr->device, mptr->mpoint, mptr->fstype, MS_RDONLY, NULL) == -1) {
			printm("Can't mount %s: %s", mptr->mpoint, strerror(errno));
			mount_free(&mounts);
			return(0);
		}
		
		mptr->mpoint[0] = '\0';
		count--;
	}
	
	return(1);
}

/* Attempt to detect the filesystem format of a device or file
 * Returns the filesystem type as a string on success, NULL on error
*/
char* detect_fstype(char const *device) {
	FILE *fh = NULL;
	
	while(!(fh = fopen(device, "rb"))) {
		if(errno == EINTR) {
			continue;
		}
		
		printm("Can't open %s: %s\n", device, strerror(errno));
		return NULL;
	}
	
	char *retval = NULL;
	unsigned char buf[1088];
	size_t rcount = 0;
	
	while(rcount < 1088) {
		rcount += fread(buf+rcount, 1, 1088-rcount, fh);
		
		if((ferror(fh) && errno != EINTR) || feof(fh)) {
			goto DFST_END;
		}
		clearerr(fh);
	}
	
	if(buf[0x438] == 0x53 && buf[0x439] == 0xEF) {
		retval = "ext2";
	}
	if(str_compare((char*)buf, "XFSB", STR_MAXLEN, 4)) {
		retval = "xfs";
	}
	
	DFST_END:
	while(fclose(fh) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		printm("Can't close %s: %s\n", device, strerror(errno));
	}
	
	return retval;
}

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
#include <stdlib.h>

#include "mount.h"
#include "misc.h"
#include "../config.h"
#include "config.h"
#include "console.h"
#include "grub.h"

#define MAGIC_BUF_SIZE (0x1003A)

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
		NULL,
		NULL
	};
	
	unsigned int devnum, rtime = 1;
	char const* devname, *errmsg;
	
	if((devname = get_cmdline("kexec_config"))) {
		for(devnum = 0; devices[devnum]; devnum++) {}
		
		while(devnum > 0) {
			devices[devnum] = devices[devnum-1];
			devnum--;
		}
		
		devices[0] = devname;
	}
	
	RETRY:
	devnum = 0;
	devname = devices[0];
	
	while(devname) {
		if((errmsg = mount_dev(devname, "/mnt/config"))) {
			debug("Can't mount %s at /mnt/config: %s\n", devname, errmsg);
			goto ENDLOOP;
		}
		
		if(access("/mnt/config/" CONFIG_FILE, F_OK) == 0) {
			printd("Found " CONFIG_FILE " on %s", devname);
			return 1;
		}
		
		if(umount("/mnt/config") == -1) {
			debug("Can't unmount /mnt/config: %s\n", strerror(errno));
		}
		
		ENDLOOP:
		devname = devices[++devnum];
	}
	
	if(rtime < 4) {
		printd("Can't find disk containing " CONFIG_FILE);
		printd("Retrying in %u seconds...", rtime);
		
		sleep(rtime);
		rtime *= 2;
		
		goto RETRY;
	}
	
	printD("Can't find disk containing " CONFIG_FILE);
	printD("Giving up, configuration not loaded");
	
	return 0;
}

/* Mount all mounts in a kl_mount list
 *
 * If all mounts are sucessfully mounted 1 is returned, if any mounts fail zero
 * is returned any any already-completed mounts are unmounted
*/
int mount_list(kl_mount* mounts) {
	kl_mount *mptr = mounts;
	int depth = 0, n = 0, n2 = 0;
	char const *errmsg;
	
	while(1) {
		if(mptr->depth == depth) {
			TEXT_GREEN();
			printd("> Mounting %s at %s", mptr->device, mptr->mpoint);
			
			if((errmsg = mount_dev(mptr->device, mptr->mpoint))) {
				TEXT_RED();
				printD(">> Mount failed: %s", errmsg);
				
				break;
			}
			
			n++;
		}
		
		if((mptr = mptr->next) == NULL) {
			if(n == 0) {
				TEXT_WHITE();
				return 1;
			}
			
			n = 0;
			depth++;
			
			mptr = mounts;
		}
	}
	
	/* If the first loop ever returns there's been an error
	 * The next loop undoes all the mounts created by the first loop
	*/
	
	mptr = mounts;
	
	while(depth >= 0) {
		if(mptr->depth == depth) {
			if(n2 == n) {
				n = -1;
				goto DDEPTH;
			}
			
			TEXT_GREEN();
			printd("> Unmounting %s", mptr->mpoint);
			
			if(umount(mptr->mpoint) == -1) {
				TEXT_RED();
				printD(">> Unmount failed: %s", strerror(errno));
			}
			
			n2++;
		}
		
		if((mptr = mptr->next) == NULL) {
			DDEPTH:
			
			n2 = 0;
			depth--;
			
			mptr = mounts;
		}
	}
	
	TEXT_WHITE();
	return 0;
}

/* Unmount all mounts in a kl_mount list
 * Continues on errors
*/
void unmount_list(kl_mount *mounts) {
	kl_mount *mptr = mounts;
	int depth = 0;
	
	if(!mptr) {
		return;
	}
	
	while(mptr) {
		if(mptr->depth > depth) {
			depth = mptr->depth;
		}
		
		mptr = mptr->next;
	}
	
	mptr = mounts;
	
	while(depth >= 0) {
		if(mptr->depth == depth) {
			TEXT_GREEN();
			printD("> Unmounting %s", mptr->mpoint);
			
			if(umount(mptr->mpoint) == -1) {
				TEXT_RED();
				printD(">> Unmount failed: %s", strerror(errno));
			}
		}
		
		if((mptr = mptr->next) == NULL) {
			depth--;
			mptr = mounts;
		}
	}
	
	TEXT_WHITE();
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
		
		debug("Can't open %s: %s\n", device, strerror(errno));
		return NULL;
	}
	
	char *retval = NULL;
	size_t rcount = 0;
	
	unsigned char buf[MAGIC_BUF_SIZE];
	memset(buf, '\0', MAGIC_BUF_SIZE);
	
	while(rcount < MAGIC_BUF_SIZE) {
		rcount += fread(buf+rcount, 1, MAGIC_BUF_SIZE-rcount, fh);
		
		if(feof(fh)) {
			break;
		}
		if(ferror(fh) && errno != EINTR) {
			goto DFST_END;
		}
		clearerr(fh);
	}
	
	if(buf[0x438] == 0x53 && buf[0x439] == 0xEF) {
		if(buf[0x45C] & 4) {
			retval = "ext3";
		}else{
			retval = "ext2";
		}
	}
	if(str_compare((char*)buf, "XFSB", STR_MAXLEN, 4)) {
		retval = "xfs";
	}
	if(str_compare((char*)(buf+0x10034), "ReIsEr", STR_MAXLEN, 6)) {
		retval = "reiserfs";
	}
	if(
		(buf[0x410] == 0x13 && buf[0x411] == 0x7F) ||
		(buf[0x410] == 0x7F && buf[0x411] == 0x13) ||
		(buf[0x410] == 0x8F && buf[0x411] == 0x13) ||
		(buf[0x410] == 0x68 && buf[0x411] == 0x24) ||
		(buf[0x410] == 0x78 && buf[0x411] == 0x24)
	) {
		retval = "minix";
	}
	if(str_compare((char*)(buf+0x36), "FAT", STR_MAXLEN, 3)) {
		retval = "vfat";
	}
	if(str_compare((char*)(buf+3), "NTFS    ", STR_MAXLEN, 8)) {
		retval = "ntfs";
	}
	
	DFST_END:
	while(fclose(fh) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		debug("Can't close %s: %s\n", device, strerror(errno));
	}
	
	return retval;
}

/* Check if a device is listed in /proc/diskstats
 * Returns 1 if the device is listed, zero otherwise.
 *
 * The supplied device may or may not have the /dev/ prefix.
*/
int check_device(char const *device) {
	char buf[512], *name;
	int rval = 0;
	
	if(strncmp(device, "/dev/", 5) == 0) {
		device += 5;
	}
	
	FILE *disks = fopen("/proc/diskstats", "r");
	if(!disks) {
		debug("Can't open /proc/diskstats: %s\n", strerror(errno));
		return 0;
	}
	
	while(fgets(buf, 512, disks)) {
		strtok(buf, " \t");
		strtok(NULL, " \t");
		name = strtok(NULL, " \t");
		
		if(strcmp(name, device) == 0) {
			rval = 1;
			break;
		}
	}
	
	fclose(disks);
	return rval;
}

/* Mount a device
 * Returns NULL on success, or an error string upon failure
*/
char const *mount_dev(char const *device, char const *mpoint) {
	char *fstype = NULL, fsbuf[16];
	char *idev, devbuf[DEVICE_SIZE];
	char *errmsg = NULL;
	size_t fslen;
	
	if(strchr(device, ':')) {
		fslen = strcspn(device, ":");
		
		strncpy(fsbuf, device, (fslen < 16 ? fslen : 15));
		fsbuf[(fslen < 16 ? fslen : 16)] = '\0';
		
		device += (fslen+1);
		fstype = fsbuf;
	}
	
	idev = (char*)device;
	
	if(device[0] == '(') {
		device = grub_cdevice(device);
		
		if(!device) {
			errmsg = "Unknown GRUB device";
			goto END;
		}
	}
	if(device[0] != '/') {
		snprintf(devbuf, DEVICE_SIZE, "/dev/%s", device);
		device = devbuf;
	}
	
	if(!check_device(device)) {
		errmsg = "Device not found";
		goto END;
	}
	
	if(!fstype || str_compare(fstype, "auto", 0)) {
		fstype = detect_fstype(device);
		if(!fstype) {
			errmsg = "Unknown filesystem format";
			goto END;
		}
	}
	
	/* Creates mount points such as /mnt/hda1 when needed
	 * Filesystems are mounted read-only, so this won't
	 * create directories on mounted filesystems
	*/
	mkdir(mpoint, 0700);
	
	if(mount(device, mpoint, fstype, MS_RDONLY, NULL) == -1) {
		errmsg = strerror(errno);
		goto END;
	}
	
	END:
	if(idev[0] == '(') {
		free((char*)device);
	}
	return errmsg;
}

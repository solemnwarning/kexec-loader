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
#include <poll.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/kdev_t.h>

#include "mount.h"
#include "misc.h"
#include "../config.h"
#include "config.h"
#include "console.h"
#include "grub.h"
#include "mystring.h"

static int check_magic(int fd, off_t offset, char const *magic, size_t len);

/* Mount disk containing CONFIG_FILE at /boot
 * Returns 1 if device containing file was mounted, zero otherwise
*/
int mount_boot(void) {
	char const* devices[] = {
		"/dev/fd0",
		"/dev/fd1",
		"/dev/sda",
		"/dev/sdb",
		"/dev/sdc",
		"/dev/sdd",
		NULL
	};
	
	unsigned int devnum;
	char const* devname, *errmsg;
	
	struct pollfd pevents;
	pevents.fd = STDIN_FILENO;
	pevents.events = POLLIN;
	
	putchar('\n');
	printm(GREEN, 1, "Searching for " CONFIG_FILE "...");
	printm(0, 1, "Press any key to abort");
	
	char *kdevice = get_cmdline("kexec_root");
	
	devnum = 0;
	devname = (kdevice ? kdevice : devices[0]);
	
	while(1) {
		if(poll(&pevents, 1, 0)) {
			getchar();
			
			free(kdevice);
			return 0;
		}
		
		if((errmsg = mount_dev(devname, "/boot"))) {
			debug("Can't mount %s at /boot: %s\n", devname, errmsg);
			goto ENDLOOP;
		}
		
		if(access("/boot/" CONFIG_FILE, F_OK) == 0) {
			printd(GREEN, 2, "Found " CONFIG_FILE " on %s", devname);
			
			free(kdevice);
			return 1;
		}
		
		if(umount("/boot") == -1) {
			debug("Can't unmount /boot: %s\n", strerror(errno));
		}
		
		ENDLOOP:
		if(!kdevice) {
			if(!devices[++devnum]) {
				sleep(1);
				devnum = 0;
			}
			
			devname = devices[devnum];
		}else{
			sleep(1);
		}
	}
	
	free(kdevice);
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
			printd(GREEN, 1, "Mounting %s at %s", mptr->device, mptr->mpoint);
			
			if((errmsg = mount_dev(mptr->device, mptr->mpoint))) {
				printD(RED, 2, "Mount failed: %s", errmsg);
				break;
			}
			
			n++;
		}
		
		if((mptr = mptr->next) == NULL) {
			if(n == 0) {
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
			
			printd(GREEN, 1, "Unmounting %s", mptr->mpoint);
			
			if(umount(mptr->mpoint) == -1) {
				printD(RED, 2, "Unmount failed: %s", strerror(errno));
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
			printD(GREEN, 1, "Unmounting %s", mptr->mpoint);
			
			if(umount(mptr->mpoint) == -1) {
				printD(RED, 2, "Unmount failed: %s", strerror(errno));
			}
		}
		
		if((mptr = mptr->next) == NULL) {
			depth--;
			mptr = mounts;
		}
	}
}

/* Attempt to detect the filesystem format of a device or file
 * Returns the filesystem type as a string on success, NULL on error
*/
char* detect_fstype(char const *device) {
	int fd = -1;
	char *retval = NULL;
	
	if(!check_device(device)) {
		debug("Device not found\n");
		goto CLEANUP;
	}
	
	while((fd = open(device, O_RDONLY)) == -1) {
		if(errno == EINTR) {
			continue;
		}
		
		debug("Can't open %s: %s\n", device, strerror(errno));
		goto CLEANUP;
	}
	
	if(check_magic(fd, 0x438, (char[]){0x53, 0xEF}, 2)) {
		if(check_magic(fd, 0x45C, "EXT3_CHECK", 1)) {
			retval = "ext3";
		}else{
			retval = "ext2";
		}
	}
	if(check_magic(fd, 0, "XFSB", 4)) {
		retval = "xfs";
	}
	if(check_magic(fd, 0x10034, "ReIsEr", 6)) {
		retval = "reiserfs";
	}
	if(
		check_magic(fd, 0x410, (char[]){0x13, 0x7F}, 2) ||
		check_magic(fd, 0x410, (char[]){0x7F, 0x13}, 2) ||
		check_magic(fd, 0x410, (char[]){0x8F, 0x13}, 2) ||
		check_magic(fd, 0x410, (char[]){0x68, 0x24}, 2) ||
		check_magic(fd, 0x410, (char[]){0x78, 0x24}, 2)
	) {
		retval = "minix";
	}
	if(check_magic(fd, 0x36, "FAT", 3)) {
		retval = "vfat";
	}
	if(check_magic(fd, 0x03, "NTFS    ", 8)) {
		retval = "ntfs";
	}
	if(
		check_magic(fd, 32769, "CD001", 5) ||
		check_magic(fd, 37633, "CD001", 5) ||
		check_magic(fd, 32776, "CDROM", 5)
	) {
		retval = "iso9660";
	}
	
	CLEANUP:
	while(fd >= 0 && close(fd) == -1) {
		if(errno == EINTR) {
			continue;
		}
		
		debug("Failed to close device: %s\n", strerror(errno));
	}
	
	return retval;
}

/* Check if a device is listed in /proc/diskstats
 * Returns 1 if the device is listed, zero otherwise.
 *
 * The supplied device may or may not have the /dev/ prefix.
*/
int check_device(char const *device) {
	char buf[512], *ptr;
	int rval = 0, major, minor;
	
	if(strncmp(device, "/dev/", 5) == 0) {
		device += 5;
	}
	
	FILE *disks = fopen("/proc/diskstats", "r");
	if(!disks) {
		debug("Can't open /proc/diskstats: %s\n", strerror(errno));
		return 0;
	}
	
	while(fgets(buf, 512, disks)) {
		ptr = buf+strspn(buf, " \t");
		major = atoi(ptr);
		
		ptr += strcspn(ptr, " \t");
		ptr += strspn(ptr, " \t");
		minor = atoi(ptr);
		
		ptr += strcspn(ptr, " \t");
		ptr += strspn(ptr, " \t");
		ptr[strcspn(ptr, " \t\r\n")] = '\0';
		
		if(str_eq(ptr, device, -1)) {
			rval = 1;
			
			snprintf(buf, 512, "/dev/%s", device);
			if(mknod(buf, 0600 | S_IFBLK, MKDEV(major, minor)) == -1 && errno != EEXIST) {
				debug("Failed to create %s device node: %s\n", buf, strerror(errno));
				rval = 0;
			}
			
			break;
		}
	}
	
	fclose(disks);
	return rval;
}

/* Mount a device
 * Returns NULL on success, or an error string upon failure
*/
char const *mount_dev(char const *idev, char const *mpoint) {
	char *fstype = NULL, fsbuf[16];
	char *errmsg = NULL, *device = (char*)idev;
	size_t fslen;
	
	if(strchr(device, ':')) {
		fslen = strcspn(device, ":");
		
		strncpy(fsbuf, device, (fslen < 16 ? fslen : 15));
		fsbuf[(fslen < 16 ? fslen : 16)] = '\0';
		
		device += (fslen+1);
		fstype = fsbuf;
	}
	
	if(device[0] == '(') {
		device = grub_cdevice(device);
		
		if(!device) {
			errmsg = "Unknown GRUB device";
			goto END;
		}
	}
	if(device[0] != '/') {
		device = str_printf("/dev/%s", device);
	}
	
	if(!check_device(device)) {
		errmsg = "Device not found";
		goto END;
	}
	
	if(!fstype || str_eq(fstype, "auto", -1)) {
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
	if(device != idev) {
		free(device);
	}
	return errmsg;
}

/* Check for magic value at an offset
 *
 * Returns nonzero if the device is at least offset+len bytes in size and the
 * magic value is at offset, zero otherwise.
*/
static int check_magic(int fd, off_t offset, char const *magic, size_t len) {
	int blocks;
	char buf[len];
	size_t count = 0;
	ssize_t rret;
	
	if(ioctl(fd, BLKGETSIZE, &blocks) == -1) {
		debug("Failed to get block count: %s\n", strerror(errno));
		return 0;
	}
	
	if((blocks * 512) < (offset + len)) {
		return 0;
	}
	
	if(lseek(fd, offset, SEEK_SET) == (off_t)-1) {
		debug("Failed to seek: %s\n", strerror(errno));
		return 0;
	}
	
	while(count < len) {
		rret = read(fd, buf+count, len-count);
		
		if(rret == -1) {
			if(errno == EINTR) {
				continue;
			}
			
			debug("Failed to read: %s\n", strerror(errno));
			return 0;
		}
		
		count += rret;
	}
	
	/* Hacky interface alert! */
	if(str_eq(magic, "EXT3_CHECK", 11) && buf[0] & 4) {
		return 1;
	}
	
	if(memcmp(buf, magic, len) == 0) {
		return 1;
	}
	
	return 0;
}

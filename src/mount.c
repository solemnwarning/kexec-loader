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

int got_boot = 0; /* Is /boot mounted? */

/* Unmount all mounts under a certain directory, including the directory if it
 * is a mount itself.
 *
 * Filesystems with a device of 'rootfs' are ignored since rootfs can never be
 * unmounted and it's harmless to leave mounted anyway.
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
		
		nferror("Can't open /proc/mounts: %s", strerror(errno));
		warn("Not umounting any filesystems!!");
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
			nferror("/proc/mounts is gobbledegook!");
			return;
		}
		if(!str_compare(token, cmp2, STR_WILDCARD2)) {
			continue;
		}
		
		strncpy(mounts[mcount++], token, 1023);
	}
	
	while(fclose(mfile) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		nferror("Can't close /proc/mounts: %s", strerror(errno));
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
			nferror("Can't umount %s: %s", mount, strerror(errno));
		}
		mounts[mnum][0] = '\0';
		mcount--;
	}
}

/* Mount virtual filesystems */
void mount_virt(void) {
	if(mount("proc", "/proc", "proc", 0, NULL) == -1) {
		fatal("Can't mount /proc filesystem: %s", strerror(errno));
	}
	
	if(mount("tmpfs", "/dev", "tmpfs", 0, NULL) == -1) {
		fatal("Can't mount /dev filesystem: %s", strerror(errno));
	}
}

/* Mount kexec-loader bootdisk on /boot */
void mount_boot(void) {
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
	
	while(devname != NULL) {
		if(mount(devname, "/boot", BOOTFS_TYPE, MS_RDONLY, NULL) == -1) {
			debug("Can't mount %s at /boot: %s", devname, strerror(errno));
			return;
		}
		if(access("/boot" CONFIG_FILE, F_OK) == 0) {
			debug("Found " CONFIG_FILE " on %s", devname);
			
			got_boot = 1;
			return;
		}
		
		if(umount("/boot") == -1) {
			fatal("Can't unmount /boot: %s", strerror(errno));
		}
		
		devname = devices[++devnum];
	}
}

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

/* Unmount all filesystems */
void unmount_all(void) {
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
	
	while(fgets(line, 1024, mfile) != NULL && mcount < 128) {
		char* token = strtok(line, " ");
		if(str_compare(token, "rootfs", 0)) {
			continue;
		}
		if((token = strtok(NULL, " ")) == NULL) {
			nferror("/proc/mounts is gobbledegook!");
			return;
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

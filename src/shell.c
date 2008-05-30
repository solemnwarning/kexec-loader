/* kexec-loader - Shell functions
 * Copyright (C) 2007,2008 Daniel Collins <solemnwarning@solemnwarning.net>
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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <linux/reboot.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "shell.h"
#include "console.h"
#include "misc.h"
#include "mount.h"
#include "kexec.h"

#define CONSOLE_CMD(str) \
	}else if(str_compare(str, cmdbuf, STR_NOCASE) || str_compare(str " *", cmdbuf, STR_NOCASE | STR_WILDCARD1)) {

void list_devices(void);

static kl_target cons_target;

void shell_main(void) {
	char cmdbuf[1024];
	size_t len;
	int c;
	
	TARGET_DEFAULTS(&cons_target);
	kl_mount *nmount;
	
	READLINE:
	cmdbuf[0] = '\0';
	len = 0;
	
	printf("> ");
	
	while(1) {
		c = getchar();
		
		if(c == '\n') {
			putchar('\n');
			break;
		}
		if(c == 0x7F) {
			if(len > 0) {
				console_cback(1);
				putchar(' ');
				console_cback(1);
				
				cmdbuf[--len] = '\0';
			}
			
			continue;
		}
		if(c >= 0x20 && c <= 0x7E && len < 1023) {
			cmdbuf[len++] = c;
			cmdbuf[len] = '\0';
			
			putchar(c);
		}
	}
	
	debug("Got command: '%s'\n", cmdbuf);
	
	if(len == 0) {
	CONSOLE_CMD("exit")
		unmount_list(cons_target.mounts);
		free_mounts(cons_target.mounts);
		
		return;
	CONSOLE_CMD("mount")
		char *device = cmdbuf+strcspn(cmdbuf, " ");
		device += strspn(device, " ");
		
		char *mpoint = device+strcspn(device, " ");
		mpoint += strspn(mpoint, " ");
		
		if(!strstr(cmdbuf, " ") || !strstr(device, " ")) {
			printm("Usage: mount [<fstype>:]<device> <mount point>");
			goto ENDCMD;
		}
		
		if(!(nmount = malloc(sizeof(struct kl_mount)))) {
			printd("malloc: %s", strerror(errno));
			goto ENDCMD;
		}
		MOUNT_DEFAULTS(nmount);
		
		device[strcspn(device, " ")] = '\0';
		
		strncpy(nmount->device, device, DEVICE_SIZE-1);
		snprintf(nmount->mpoint, MPOINT_SIZE, "/mnt/target/%s", mpoint);
		
		if(!mount_list(nmount)) {
			free(nmount);
			goto ENDCMD;
		}
		
		nmount->next = cons_target.mounts;
		cons_target.mounts = nmount;
	CONSOLE_CMD("disks")
		list_devices();
	CONSOLE_CMD("help")
		printm("Available commands:");
		printm("exit mount disks help kernel initrd append cmdline boot");
	CONSOLE_CMD("kernel")
		char *kernel = cmdbuf+strcspn(cmdbuf, " ");
		kernel += strspn(kernel, " ");
		
		if(kernel[0] == '\0') {
			printm("Usage: kernel <filename>");
			goto ENDCMD;
		}
		
		snprintf(cons_target.kernel, KERNEL_SIZE, "/mnt/target/%s", kernel);
	CONSOLE_CMD("initrd")
		char *initrd = cmdbuf+strcspn(cmdbuf, " ");
		initrd += strspn(initrd, " ");
		
		if(initrd[0] == '\0') {
			printm("Usage: initrd <filename>");
			goto ENDCMD;
		}
		
		snprintf(cons_target.initrd, INITRD_SIZE, "/mnt/target/%s", initrd);
	CONSOLE_CMD("append")
		char *append = cmdbuf+strcspn(cmdbuf, " ");
		append += strspn(append, " ");
		
		strncpy(cons_target.append, append, APPEND_SIZE-1);
	CONSOLE_CMD("cmdline")
		char *cmdline = cmdbuf+strcspn(cmdbuf, " ");
		cmdline += strspn(cmdline, " ");
		
		strncpy(cons_target.cmdline, cmdline, APPEND_SIZE-1);
	CONSOLE_CMD("boot")
		if(!load_kernel(&cons_target)) {
			goto ENDCMD;
		}
		
		unmount_list(cons_target.mounts);
		
		console_fgcolour(CONS_GREEN);
		printd("> Executing kernel...");
		console_fgcolour(CONS_WHITE);
		
		sync();
		syscall(
			__NR_reboot,
			LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
			LINUX_REBOOT_CMD_KEXEC, NULL
		);
		
		console_fgcolour(CONS_RED);
		printD(">> Reboot failed: %s", strerror(errno));
		console_fgcolour(CONS_WHITE);
	}else{
		printd("Unknown command: %s", cmdbuf);
	}
	
	ENDCMD:
	putchar('\n');
	goto READLINE;
}

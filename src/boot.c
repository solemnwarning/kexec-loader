/* kexec-loader - Boot the system
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <linux/reboot.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "misc.h"
#include "disk.h"
#include "console.h"
#include "vfs.h"

#define MAX_ARGV 256

#define ARGV_COPY(s) \
	if(argc+1 == MAX_ARGV) { \
		printD("Too many arguments for kexec"); \
		goto CLEANUP; \
	} \
	argv[argc++] = kl_strdup(s); \
	argv[argc] = NULL;

#define ARGV_ADD(s) \
	if(argc+1 == MAX_ARGV) { \
		printD("Too many arguments for kexec"); \
		goto CLEANUP; \
	} \
	argv[argc++] = s; \
	argv[argc] = NULL;

#define ARGV_PRINTF(...) \
	if(argc+1 == MAX_ARGV) { \
		printD("Too many arguments for kexec"); \
		goto CLEANUP; \
	} \
	argv[argc++] = kl_sprintf(__VA_ARGS__); \
	argv[argc] = NULL;

#define MOUNT_VPATH(path) \
	tmp = vfs_translate_path(path); \
	if(!tmp) { \
		printD("%s: %s", path, kl_strerror(errno)); \
		goto CLEANUP; \
	}

int kexec_main(int argc, char **argv);

/* Boot the target passed to it
 * Returns on error
*/
void boot_target(kl_target *target) {
	char *argv[MAX_ARGV], *tmp;
	int argc = 0, status;
	
	vfs_set_root(target->root);
	
	printd("Preparing to boot...");
	
	ARGV_COPY("kexec");
	ARGV_COPY("-l");
	
	MOUNT_VPATH(target->kernel);
	ARGV_ADD(tmp);
	
	if(target->initrd[0]) {
		MOUNT_VPATH(target->initrd);
		ARGV_PRINTF("--initrd=%s", tmp);
		free(tmp);
	}
	if(target->cmdline[0]) {
		ARGV_PRINTF("--command-line=%s", target->cmdline);
	}
	if(target->append[0]) {
		ARGV_PRINTF("--append=%s", target->append);
	}
	if(target->flags & TARGET_RESET) {
		ARGV_COPY("--reset-vga");
	}
	
	kl_module *modptr = target->modules;
	while(modptr) {
		MOUNT_VPATH(modptr->name);
		
		if(modptr->args[0]) {
			ARGV_PRINTF("--module=%s %s", tmp, modptr->args);
		}else{
			ARGV_PRINTF("--module=%s", tmp);
		}
		
		free(tmp);
		modptr = modptr->next;
	}
	
	printd("Loading kernel...");
	
	pid_t pid = fork();
	if(pid == -1) {
		printD("Fork failed: %s", strerror(errno));
		goto CLEANUP;
	}else if(pid == 0) {
		exit(kexec_main(argc, argv));
	}
	
	wait(&status);
	if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		alert = 1;
		goto CLEANUP;
	}
	
	printd("Booting system...");
	
	call_reboot(LINUX_REBOOT_CMD_KEXEC);
	
	printD("Reboot failed: %s", strerror(errno));
	
	CLEANUP:
	while(argc) {
		free(argv[--argc]);
	}
}

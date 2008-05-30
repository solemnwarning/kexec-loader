/* kexec-loader - Main source
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <linux/reboot.h>
#include <poll.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "mount.h"
#include "../config.h"
#include "console.h"
#include "config.h"
#include "kexec.h"
#include "misc.h"
#include "grub.h"
#include "shell.h"

static void main_menu(void);
static void draw_skel(void);
static void draw_tbline(int rnum);
static void target_run(kl_target *target);
void list_devices(void);
static void print_header(void);
static void prepare_text(void);

static int rows = 25, cols = 80;
static int srow = 4, erow = 0;
static int scol = 3, ecol = 0;

int main(int argc, char** argv) {
	if(mount("proc", "/proc", "proc", 0, NULL) == -1) {
		fatal("Can't mount /proc filesystem: %s", strerror(errno));
	}
	
	struct rlimit limit = {
		STACK_LIMIT,
		STACK_LIMIT
	};
	
	if(setrlimit(RLIMIT_STACK, &limit) == -1) {
		debug("Can't set stack limit to %d\n", STACK_LIMIT);
	}
	
	debug("STACK_LIMIT = %d\n", STACK_LIMIT);
	debug("DEVICE_SIZE = %d\n", DEVICE_SIZE);
	debug("MPOINT_SIZE = %d\n", MPOINT_SIZE);
	debug("NAME_SIZE = %d\n", NAME_SIZE);
	debug("KERNEL_SIZE = %d\n", KERNEL_SIZE);
	debug("INITRD_SIZE = %d\n", INITRD_SIZE);
	debug("APPEND_SIZE = %d\n", APPEND_SIZE);
	
	kmsg_monitor();
	console_init();
	
	console_getsize(&rows, &cols);
	erow = rows-5;
	ecol = cols-1;
	
	debug("rows = %d, cols = %d\n", rows, cols);
	debug("srow = %d, erow = %d\n", srow, erow);
	debug("scol = %d, ecol = %d\n", scol, ecol);
	
	while(1) {
		config_load();
		main_menu();
	}
	
	return 1;
}

/* Display main menu
 *
 * This function returns when the configuration should be reloaded, call
 * config_load() then call main_menu() again.
*/
static void main_menu(void) {
	draw_skel();
	
	int n, rnum, key;
	unsigned int tremain = config.timeout;
	if(config.targets == NULL) {
		tremain = 0;
	}
	
	int mpos = 0, mmpos = erow - srow, cmpos;
	debug("mmpos = %d\n", mmpos);
	
	kl_target *starget = config.targets;	/* Start of displayed list */
	kl_target *target = starget;
	
	struct pollfd pollset;
	pollset.fd = STDIN_FILENO;
	pollset.events = POLLIN;
	
	while(target != NULL) {
		if(target->flags & TARGET_DEFAULT) {
			break;
		}
		if(target->next == NULL) {
			starget = config.targets;
			mpos = 0;
			
			break;
		}
		target = target->next;
		
		if(mpos == mmpos) {
			starget = starget->next;
		}else{
			mpos++;
		}
	}
	
	while(1) {
		target = starget;
		cmpos = 0;
		
		for(rnum = srow; rnum <= erow; rnum++) {
			console_setpos(rnum, ecol);
			console_eline(ELINE_TOSTART);
			
			console_setpos(rnum, 1);
			printf("| ");
			
			if(target == NULL) {
				printf("No targets defined!");
				break;
			}
			
			if(cmpos == mpos) {
				console_attrib(CONS_INVERT);
			}
			
			for(n = 0; n <= (ecol-scol); n++) {
				if(target->name[n] == '\0') {
					break;
				}
				
				putchar(target->name[n]);
			}
			
			if(cmpos++ == mpos) {
				console_attrib(CONS_RESET);
			}
			
			if(
				(rnum == srow && starget != config.targets) ||
				(rnum == erow && target->next)
			) {
				console_setpos(rnum, ecol-8);
				printf("More...");
			}
			
			if((target = target->next) == NULL) {
				break;
			}
		}
		
		target = starget;
		for(n = 0; n < mpos; n++) {
			target = target->next;
		}
		
		MENU_INPUT:
		
		console_setpos(rows-3, cols-13);
		console_eline(ELINE_ALL);
		
		if(tremain) {
			printf("Timeout: %u", tremain);
		}
		
		if(poll(&pollset, 1, (tremain ? 1000 : -1)) == 0) {
			if(--tremain == 0) {
				debug("Timeout reached\n");
				
				prepare_text();
				target_run(target);
				draw_skel();
				continue;
			}
			
			goto MENU_INPUT;
		}
		
		tremain = 0;
		key = getchar();
		
		if(key == '\n') {
			debug("Enter pressed\n");
			
			if(!starget) {
				goto MENU_INPUT;
			}
			
			prepare_text();
			target_run(target);
			draw_skel();
			continue;
		}
		
		if(key == 0x1B && getchar() == '[') {
			key = getchar();
			
			/* key == 65: Up arrow
			 * key == 66: Down arrow
			*/
			
			if(key == 65) {
				if(!starget || target == config.targets) {
					goto MENU_INPUT;
				}
				
				if(mpos == 0) {
					starget = config.targets;
					while(starget->next != target) {
						starget = starget->next;
					}
				}else{
					mpos--;
				}
			}
			if(key == 66) {
				if(!starget || target->next == NULL) {
					goto MENU_INPUT;
				}
				
				if(mpos == mmpos) {
					starget = starget->next;
				}else{
					mpos++;
				}
			}
		}
		
		if(key == 'l' || key == 'L') {
			prepare_text();
			list_devices();
			draw_skel();
			
			continue;
		}
		if(key == 'r' || key == 'R') {
			prepare_text();
			return;
		}
		if(key == 's' || key == 'S') {
			prepare_text();
			shell_main();
			draw_skel();
			
			continue;
		}
	}
}

/* Clear the screen and draw the menu skeleton */
static void draw_skel(void) {
	int rnum;
	
	console_clear();
	print_header();
	
	draw_tbline(srow-1);
	draw_tbline(erow+1);
	
	for(rnum = srow; rnum <= erow; rnum++) {
		console_setpos(rnum, 1);
		putchar('|');
		
		console_setpos(rnum, cols);
		putchar('|');
	}
	
	console_setpos(rows-2, 2);
	printf("Press L to list detected devices, R to reload configuration");
	
	console_setpos(rows-1, 2);
	printf("Press S to execute shell");
}

/* Draw a +----+ line along one row */
static void draw_tbline(int rnum) {
	console_setpos(rnum, 1);
	
	int cnum = 1;
	while(cnum <= cols) {
		if(cnum == 1 || cnum == cols) {
			putchar('+');
		}else{
			putchar('-');
		}
		
		cnum++;
	}
}

/* Attempt to load and execute a target
 * This only returns on error
*/
static void target_run(kl_target *target) {
	printd("Loading %s...", target->name);
	printm("");
	
	if(!mount_list(target->mounts)) {
		return;
	}
	
	if(!load_kernel(target)) {
		unmount_list(target->mounts);
		return;
	}
	
	unmount_list(target->mounts);
	
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
}

/* Display a list of devices from /proc/diskstats */
void list_devices(void) {
	char buf[256];
	char filename[32];
	char *name, *fstype;
	int major, minor;
	
	FILE *disks = fopen("/proc/diskstats", "r");
	if(!disks) {
		printD("Can't open /proc/diskstats: %s", strerror(errno));
	}
	
	printM("The following disks have been detected by Linux:");
	printM("");
	
	while(fgets(buf, 256, disks)) {
		major = atoi(strtok(buf, " \t"));
		minor = atoi(strtok(NULL, " \t"));
		name = strtok(NULL, " \t");
		
		snprintf(filename, 32, "/dev/%s", name);
		if(!(fstype = detect_fstype(filename))) {
			fstype = "Unknown filesystem";
		}
		
		printM("/dev/%s\t(%d, %d, %s)", name, major, minor, fstype);
	}
	
	fclose(disks);
}

/* Print version/copyright header at row 1 */
static void print_header(void) {
	int cnum, n = 0;
	
	char *version = "kexec-loader " VERSION;
	char *copyright = COPYRIGHT;
	
	size_t version_l = strlen(version);
	size_t copyright_l = strlen(copyright);
	
	console_attrib(CONS_INVERT);
	console_setpos(1, 1);
	
	for(cnum = 1; cnum <= cols; cnum++) {
		if(cnum > 1 && (cnum-1) <= version_l) {
			putchar(version[cnum-2]);
			continue;
		}
		if(cnum != cols && (cnum+copyright_l) >= cols) {
			putchar(copyright[n++]);
			continue;
		}
		
		putchar(' ');
	}
	
	console_attrib(CONS_RESET);
}

/* Prepare the console for basic text usage */
static void prepare_text(void) {
	console_clear();
	print_header();
	
	console_setpos(3,1);
}

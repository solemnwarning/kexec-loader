/* kexec-loader - Misc. functions
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
#include <sys/mount.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/klog.h>
#include <ctype.h>
#include <sys/syscall.h>
#include <linux/reboot.h>

#include "misc.h"
#include "console.h"
#include "disk.h"
#include "menu.h"
#include "grub.h"

#define KLOG_TTY "/dev/tty2"
#define DEBUG_TTY "/dev/tty3"

kl_disk *boot_disk = NULL;
int timeout = 0;
char grub_path[1024] = {'\0'};
kl_target *targets = NULL;
kl_module *kmods = NULL;

static void redirect_klog(void);
static void load_conf(void);

int main(int argc, char **argv) {
	if(mount("none", "/proc", "proc", 0, NULL)) {
		die("Error mounting /proc: %s", strerror(errno));
	}
	
	redirect_klog();
	
	console_init();
	console_clear();
	console_setpos(0,0);
	
	char *kdevice = get_cmdline("root");
	char *device = kdevice ? kdevice : "LABEL=kexecloader";
	boot_disk = mount_retry(device, "boot disk");
	free(kdevice);
	
	if(boot_disk) {
		load_conf();
		modprobe_all();
		grub_load();
	}
	
	while(1) {
		if(targets) {
			menu_main();
		}else{
			shell_main();
		}
	}
	
	return 0;
}

/* Print a message to the debug log/tty */
void debug(char const *fmt, ...) {
	static FILE *debug_fh = NULL;
	va_list argv;
	char msgbuf[256];
	
	if(!debug_fh) {
		char *path = get_cmdline("debug_tty");
		debug_fh = fopen(path ? path : DEBUG_TTY, "a");
		free(path);
		
		if(!debug_fh) {
			return;
		}
	}
	
	va_start(argv, fmt);
	vsnprintf(msgbuf, 256, fmt, argv);
	va_end(argv);
	
	fprintf(debug_fh, "%s\n", msgbuf);
	fflush(debug_fh);
}

/* Disable kernel messages to the console and spawn a process that writes all
 * kernel messages to KLOG_TTY
*/
static void redirect_klog(void) {
	debug("Disabling printk() to console...");
	klogctl(6, NULL, 0);
	
	FILE *klog_fh = fopen(KLOG_TTY, "a");
	if(!klog_fh) {
		debug("Error opening " KLOG_TTY ": %s", strerror(errno));
		return;
	}
	
	FILE *kmsg_fh = fopen("/proc/kmsg", "r");
	if(!kmsg_fh) {
		debug("Error opening /proc/kmsg: %s", strerror(errno));
		return;
	}
	
	if(fork()) {
		return;
	}
	
	char buf[256];
	while(fgets(buf, 256, kmsg_fh)) {
		fputs(buf, klog_fh);
	}
	
	if(ferror(kmsg_fh)) {
		debug("Error reading /proc/kmsg: %s", strerror(errno));
	}
	
	debug("The /proc/kmsg read loop has exited!");
	exit(0);
}

/* Log error and idle (Exiting will cause panic in older kernels)
 *
 * HACK: Behaviour changes if PID is not 1, this is because kexec-tools also has
 * a die() function and they conflict at link time.
*/
void die(char const *fmt, ...) {
	va_list argv;
	char msgbuf[256];
	
	va_start(argv, fmt);
	vsnprintf(msgbuf, 256, fmt, argv);
	va_end(argv);
	
	if(getpid() == 1) {
		debug("FATAL: %s", msgbuf);
		printf("\nFATAL: %s", msgbuf);
		
		while(1) {
			sleep(9999);
		}
	}else{
		msgbuf[strcspn(msgbuf, "\n")] = '\0';
		printd("%s", msgbuf);
		
		exit(1);
	}
}

/* Search the kernel command line for an option */
char *get_cmdline(char const *name) {
	FILE *fh = fopen("/proc/cmdline", "r");
	if(!fh) {
		debug("Error opening /proc/cmdline: %s", strerror(errno));
		return NULL;
	}
	
	int len = strlen(name);
	char *r = NULL, buf[1024];
	
	if(fgets(buf, 1024, fh)) {
		r = buf;
		
		while((r = strstr(r, name))) {
			if(r == buf || r[-1] == ' ') {
				if(r[len] == '=') {
					r += len+1;
					break;
				}else if(r[len] == ' ' || r[len] == '\0') {
					r += len;
					break;
				}
			}
			
			r++;
		}
		
		if(r) {
			r = kl_strndup(r, strcspn(r, " "));
		}
	}
	
	if(ferror(fh)) {
		debug("Error reading /proc/cmdline: %s", strerror(errno));
	}
	
	fclose(fh);
	
	return r;
}

/* Return next value in a string */
char *next_value(char *ptr) {
	ptr += strcspn(ptr, "\t ");
	
	if(*ptr) {
		ptr[0] = '\0';
		ptr++;
		
		ptr += strspn(ptr, "\t ");
	}
	
	return ptr;
}

#define LD_PDIV() \
	printf("+----------+----------+------------+-"); \
	for(i = 0; i < lw; i++) { \
		putchar('-'); \
	} \
	puts("-+");

/* Display a list of disks */
void list_disks(void) {
	int lw, i;
	console_getsize(&lw, NULL);
	lw -= 39;
	
	LD_PDIV();
	printf("|   Device |     Size |     Format | %*.*s |\n", lw, lw, "Label");
	LD_PDIV();
	
	kl_disk *ptr = get_disks();
	while(ptr) {
		printf("| %8.8s ", ptr->name);
		printf("| %8.8s ", ptr->size);
		printf("| %10.10s ", ptr->fstype[0] ? ptr->fstype : "UNKNOWN");
		printf("| %*.*s |\n", lw, lw, ptr->label[0] ? ptr->label : "NO LABEL");
		
		list_del(&ptr, ptr);
	}
	
	LD_PDIV();
}

/* Prepare the system for reboot and call reboot
 * Returns on error
*/
void call_reboot(int cmd) {
	unmount_all();
	sync();
	
	syscall(
		__NR_reboot,
		LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
		cmd, NULL
	);
}

/* Allocate memory */
void *kl_malloc(size_t size) {
	void *ptr = malloc(size);
	if(!ptr) {
		die("Out of memory! (Tried to allocate %u)", size);
	}
	
	memset(ptr, 0, size);
	return ptr;
}

/* Resize memory */
void *kl_realloc(void *ptr, size_t size) {
	ptr = realloc(ptr, size);
	if(!ptr) {
		die("Out of memory! (Tried to realloc %u)", size);
	}
	
	return ptr;
}

/* Duplicate a string */
char *kl_strdup(char const *src) {
	char *dest = kl_malloc(strlen(src)+1);
	strcpy(dest, src);
	
	return dest;
}

/* Duplicate a string */
char *kl_strndup(char const *src, int max) {
	int len = 0;
	while(src[len] && len < max) { len++; }
	
	char *dest = kl_malloc(len+1);
	strlcpy(dest, src, len+1);
	
	return dest;
}

/* Allocate a buffer and write a printf format string to it */
char *kl_sprintf(char const *fmt, ...) {
	va_list argv;
	
	va_start(argv, fmt);
	char *dest = kl_malloc(vsnprintf(NULL, 0, fmt, argv)+1);
	va_end(argv);
	
	va_start(argv, fmt);
	vsprintf(dest, fmt, argv);
	va_end(argv);
	
	return dest;
}

/* Compare two strings */
int kl_streq(char const *s1, char const *s2) {
	int i = 0;
	
	while(s1[i] == s2[i]) {
		if(s1[i] == '\0') {
			return 1;
		}
		
		i++;
	}
	
	return 0;
}

/* Compare two strings, stop after max characters */
int kl_strneq(char const *s1, char const *s2, int max) {
	int i = 0;
	
	while(s1[i] == s2[i] && i < max) {
		if(s1[i] == '\0') {
			return 1;
		}
		
		i++;
	}
	
	return i == max ? 1 : 0;
}

/* Compare two strings, ignoring case */
int kl_strceq(char const *s1, char const *s2) {
	int i = 0;
	
	while(tolower(s1[i]) == tolower(s2[i])) {
		if(s1[i] == '\0') {
			return 1;
		}
		
		i++;
	}
	
	return 0;
}

/* Compare two strings, ignoring case, stop after max characters */
int kl_strnceq(char const *s1, char const *s2, int max) {
	int i = 0;
	
	while(tolower(s1[i]) == tolower(s2[i]) && i < max) {
		if(s1[i] == '\0') {
			return 1;
		}
		
		i++;
	}
	
	return i == max ? 1 : 0;
}

struct list { struct list *next; };

/* Add an entry to a list */
void list_add(void *rptr, void *node) {
	struct list **root = rptr;
	struct list *ptr = *root;
	
	while(ptr && ptr->next) {
		ptr = ptr->next;
	}
	
	if(ptr) {
		ptr->next = node;
	}else{
		*root = node;
	}
}

/* Copy a node and add the copy to a list */
void list_add_copy(void *rptr, void *node, int size) {
	void *nptr = kl_malloc(size);
	memcpy(nptr, node, size);
	
	list_add(rptr, nptr);
}

/* Remove an entry from a list */
void list_del(void *rptr, void *node) {
	struct list **root = rptr;
	struct list *ptr = *root;
	
	if(*root == node) {
		*root = ptr->next;
		free(node);
	}else{
		while(ptr && ptr->next) {
			if(ptr->next == node) {
				ptr->next = ptr->next->next;
				free(node);
				
				break;
			}
		}
	}
}

/* Return the previous node in the list
 * Returns NULL if there is no parent (e.g, node == root)
*/
void *list_prev(void *root, void *node) {
	struct list *ptr = root;
	
	while(ptr) {
		if(ptr->next == node) {
			return ptr;
		}
		
		ptr = ptr->next;
	}
	
	return NULL;
}

/* Free all nodes in a list */
void list_nuke(void *root) {
	struct list *ptr = root, *x;
	
	while(ptr) {
		x = ptr;
		ptr = ptr->next;
		
		free(x);
	}
}

/* Load kexec-loader.conf */
static void load_conf(void) {
	char filename[1024];
	snprintf(filename, 1024, "/mnt/%s/kexec-loader.conf", boot_disk->name);
	
	FILE *fh = fopen(filename, "r");
	if(!fh) {
		printD("Error opening kexec-loader.conf: %s", strerror(errno));
		return;
	}
	
	printd("Loading kexec-loader.conf...");
	
	char line[1024], *name, *val, *val2;
	int lnum = 0, topen = 0;
	kl_target target;
	kl_module mod;
	char *fname = "kexec-loader.conf";
	kl_gdev gdev;
	
	while(fgets(line, 1024, fh)) {
		line[strcspn(line, "\r\n")] = '\0';
		lnum++;
		
		name = line+strspn(line, "\r\n\t ");
		val = next_value(name);
		
		if(name[0] == '#' || name[0] == '\0') {
			continue;
		}
		
		if(kl_streq(name, "timeout")) {
			CHECK_HASARG();
			
			timeout = atoi(val);
			continue;
		}
		if(kl_streq(name, "grub-path")) {
			CHECK_HASARG();
			CHECK_VPATH();
			
			if(*val != '(') {
				printD("kexec-loader.conf:%d: No device specified", lnum);
				continue;
			}
			
			strlcpy(grub_path, val, sizeof(grub_path));
			continue;
		}
		if(kl_streq(name, "grub-map")) {
			val2 = next_value(val);
			CHECK_HASARG_MULTI(val2, 2);
			
			if(!parse_gdev(&gdev, val)) {
				printD("kexec-loader.conf:%d: Invalid GRUB device '%s'", lnum, val);
				continue;
			}
			
			strlcpy(gdev.device, val2, sizeof(gdev.device));
			list_add_copy(&grub_devmap, &gdev, sizeof(gdev));
		}
		
		if(kl_streq(name, "title")) {
			CHECK_HASARG();
			
			if(topen) {
				ADD_TARGET();
			}
			
			INIT_TARGET(&target);
			strlcpy(target.title, val, sizeof(target.title));
			topen = lnum;
			
			continue;
		}
		if(kl_streq(name, "root")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			
			strlcpy(target.root, val, sizeof(target.root));
			continue;
		}
		if(kl_streq(name, "kernel")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			CHECK_VPATH();
			
			strlcpy(target.kernel, val, sizeof(target.kernel));
			continue;
		}
		if(kl_streq(name, "initrd")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			CHECK_VPATH();
			
			strlcpy(target.initrd, val, sizeof(target.initrd));
			continue;
		}
		if(kl_streq(name, "cmdline")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			
			strlcpy(target.cmdline, val, sizeof(target.cmdline));
			continue;
		}
		if(kl_streq(name, "append")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			
			strlcpy(target.append, val, sizeof(target.append));
			continue;
		}
		if(kl_streq(name, "default")) {
			CHECK_TOPEN();
			
			target.flags |= TARGET_DEFAULT;
			continue;
		}
		if(kl_streq(name, "reset-vga")) {
			CHECK_TOPEN();
			
			target.flags |= TARGET_RESET;
			continue;
		}
		if(kl_streq(name, "module")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			CHECK_VPATH();
			
			INIT_MODULE(&mod);
			strlcpy(mod.args, next_value(val), sizeof(mod.args));
			strlcpy(mod.name, val, sizeof(mod.name));
			list_add_copy(&target.modules, &mod, sizeof(mod));
			
			continue;
		}
		if(kl_streq(name, "kmod")) {
			CHECK_TOPEN();
			CHECK_HASARG();
			
			INIT_MODULE(&mod);
			strlcpy(mod.args, next_value(val), sizeof(mod.args));
			strlcpy(mod.name, val, sizeof(mod.name));
			list_add_copy(&kmods, &mod, sizeof(mod));
			
			continue;
		}
		
		printD("kexec-loader.conf:%d: Unknown directive '%s'", lnum, name);
	}
	if(ferror(fh)) {
		printD("Error reading kexec-loader.conf: %s", strerror(errno));
	}
	
	if(topen) {
		ADD_TARGET();
	}
	
	fclose(fh);
}

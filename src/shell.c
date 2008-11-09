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
#include "mystring.h"

#define CONSOLE_CMD(str) \
	}else if(strcasecmp(cmd, str) == 0) {

#define HISTORY_MAX 32

extern int rows, cols;

void list_devices(void);
static char *next_arg(char *args);
static void add_module(char const *module);
static void set_command(char const *cmd, int offset);
static void move_cursor(int offset);

static kl_target cons_target = TARGET_DEFAULTS_DEFINE;
static char *history[HISTORY_MAX];
static int srow, scol;

void shell_main(void) {
	char cmdbuf[1024];
	size_t len, offset;
	int c, hnum;
	char *cmd, *arg1, *arg2, *nhist;
	
	for(hnum = 0; hnum < HISTORY_MAX; hnum++) {
		history[hnum] = NULL;
	}
	
	free_modules(cons_target.modules);
	TARGET_DEFAULTS(&cons_target);
	kl_mount *nmount;
	
	READLINE:
	cmdbuf[0] = '\0';
	len = 0;
	offset = 0;
	hnum = -1;
	
	printf("> ");
	
	console_getpos(&srow, &scol);
	
	while(1) {
		c = getchar();
		
		if(c == '\n') {
			putchar('\n');
			break;
		}
		if(c == 0x7F) {
			/* 0x7F == DEL (Backspace) */
			
			if(offset > 0) {
				memmove(cmdbuf+offset-1, cmdbuf+offset, len-offset+1);
				set_command(cmdbuf, --offset);
				move_cursor(offset);
				
				len--;
			}
			
			continue;
		}
		if(c == 0x1B) {
			if((c = getchar()) != '[') {
				ungetc(c, stdin);
				continue;
			}
			
			c = getchar();
			
			if(c == 65 && (hnum+1) < HISTORY_MAX && history[hnum+1]) {
				strcpy(cmdbuf, history[++hnum]);
				offset = len = strlen(cmdbuf);
				
				set_command(cmdbuf, 0);
			}
			if(c == 66 && hnum >= 0) {
				if(--hnum == -1) {
					cmdbuf[0] = '\0';
					offset = len = 0;
				}else{
					strcpy(cmdbuf, history[hnum]);
					offset = len = strlen(cmdbuf);
				}
				
				set_command(cmdbuf, 0);
			}
			if(c == 68 && offset > 0) {
				move_cursor(--offset);
			}
			if(c == 67 && len - offset > 0) {
				move_cursor(++offset);
			}
			if(c == 51) {
				getchar();
				
				if(len - offset > 0) {
					memmove(cmdbuf+offset, cmdbuf+offset+1, len-offset);
					set_command(cmdbuf, offset);
					move_cursor(offset);
					
					len--;
				}
			}
			if(c == 49) {
				getchar();
				move_cursor(offset = 0);
			}
			if(c == 52) {
				getchar();
				move_cursor(offset = len);
			}
			
			continue;
		}
		
		if(len < 1023) {
			memmove(cmdbuf+offset+1, cmdbuf+offset, len-offset+1);
			cmdbuf[offset] = c;
			len++;
			
			set_command(cmdbuf, offset++);
			move_cursor(offset);
		}
	}
	
	if(len > 0 && (history[0] == NULL || !str_eq(history[0], cmdbuf, -1))) {
		nhist = str_copy(NULL, cmdbuf, -1);
		free(history[HISTORY_MAX-1]);
		
		for(hnum = (HISTORY_MAX-1); hnum > 0; hnum--) {
			if(history[hnum-1]) {
				history[hnum] = history[hnum-1];
			}
		}
		
		history[0] = nhist;
	}
	
	cmd = cmdbuf+strspn(cmdbuf, " ");
	arg1 = next_arg(cmd);
	
	debug("Shell cmd='%s' arg1='%s'\n", cmd, arg1);
	
	if(cmd[0] == '\0') {
	CONSOLE_CMD("exit")
		unmount_list(cons_target.mounts);
		free_mounts(cons_target.mounts);
		
		return;
	CONSOLE_CMD("mount")
		arg2 = next_arg(arg1);
		
		if(arg1[0] == '\0' || arg2[0] == '\0') {
			printm(0, 0, "Usage: mount [<fstype>:]<device> <mount point>");
			goto ENDCMD;
		}
		
		nmount = allocate(sizeof(struct kl_mount));
		INIT_MOUNT(nmount);
		
		str_copy(&nmount->device, arg1, -1);
		nmount->mpoint = str_printf("/mnt/target/%s", arg2);
		
		if(!mount_list(nmount)) {
			free(nmount);
			goto ENDCMD;
		}
		
		nmount->next = cons_target.mounts;
		cons_target.mounts = nmount;
	CONSOLE_CMD("disks")
		list_devices();
	CONSOLE_CMD("help")
		printm(0, 0, "Available commands:");
		printm(0, 0, "exit mount disks help kernel initrd append cmdline boot reset-vga");
	CONSOLE_CMD("kernel")
		if(arg1[0] == '\0') {
			printm(0, 0, "Usage: kernel <filename>");
			goto ENDCMD;
		}
		
		cons_target.kernel = str_printf("/mnt/target/%s", arg1);
	CONSOLE_CMD("initrd")
		if(arg1[0] == '\0') {
			printm(0, 0, "Usage: initrd <filename>");
			goto ENDCMD;
		}
		
		cons_target.initrd = str_printf("/mnt/target/%s", arg1);
	CONSOLE_CMD("append")
		str_copy(&cons_target.append, arg1, -1);
	CONSOLE_CMD("cmdline")
		str_copy(&cons_target.cmdline, arg1, -1);
	CONSOLE_CMD("boot")
		if(!load_kernel(&cons_target)) {
			goto ENDCMD;
		}
		
		unmount_list(cons_target.mounts);
		
		console_fgcolour(CONS_GREEN);
		printd(GREEN, 1, "Executing kernel...");
		console_fgcolour(CONS_WHITE);
		
		sync();
		syscall(
			__NR_reboot,
			LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
			LINUX_REBOOT_CMD_KEXEC, NULL
		);
		
		console_fgcolour(CONS_RED);
		printD(RED, 2, "Reboot failed: %s", strerror(errno));
		console_fgcolour(CONS_WHITE);
	CONSOLE_CMD("reset-vga")
		cons_target.flags |= TARGET_RESET_VGA;
	CONSOLE_CMD("module")
		add_module(arg1);
	}else{
		printd(0, 0, "Unknown command: %s", cmd);
	}
	
	ENDCMD:
	putchar('\n');
	goto READLINE;
}

static char *next_arg(char *args) {
	char *retval = args+strcspn(args, " ");
	
	if(retval[0] != '\0') {
		retval[0] = '\0';
		retval++;
		
		retval += strspn(retval, " ");
	}
	
	return retval;
}

static void add_module(char const *module) {
	kl_module *nptr = allocate(sizeof(kl_module));
	INIT_MODULE(nptr);
	
	nptr->module = str_printf("/mnt/target/%s", module);
	nptr->next = cons_target.modules;
	cons_target.modules = nptr;
}

/* Replace the command which is displayed on the terminal */
static void set_command(char const *cmd, int offset) {
	int len = strlen(cmd), erows;
	
	int row = srow + (((scol-1) + offset) / cols);
	int col = (scol + offset) % cols;
	int erow = srow + (((scol-1) + len) / cols);
	
	while(erow > row) {
		console_setpos(erow, 0);
		console_eline(ELINE_ALL);
	}
	
	console_setpos(row, col);
	console_eline(ELINE_TOEND);
	
	printf("%s", cmd+offset);
	
	erows = ((col-1) + (len-offset)) / cols;
	
	if(rows - srow < erows) {
		srow -= (erows - (rows - srow));
	}
	
	if(((col-1) + (len-offset)) % cols == 0) {
		putchar('\n');
	}
}

/* Move the terminal cursor to the correct offset */
static void move_cursor(int offset) {
	int row = srow + (((scol-1) + offset) / cols);
	int col = (scol + offset) % cols;
	
	console_setpos(row, col);
}

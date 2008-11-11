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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#include "shell.h"
#include "console.h"
#include "misc.h"
#include "mount.h"
#include "kexec.h"
#include "mystring.h"

#define HISTORY_MAX 32
#define ARGV_SIZE 128

struct shell_command {
	char const *name;
	void (*func)(int, char**);
};

extern int rows, cols;

void list_devices(void);
static void set_command(char const *cmd, int offset);
static void move_cursor(int offset);
static void parse_command(char *cmd, int *argc, char **argv);
static char *shell_path(char const *spath);

static void cmd_mount(int argc, char **argv);
static void cmd_module(int argc, char **argv);
static void cmd_boot(int argc, char **argv);
static void cmd_kernel(int argc, char **argv);
static void cmd_initrd(int argc, char **argv);
static void cmd_cd(int argc, char **argv);
static void cmd_ls(int argc, char **argv);

static kl_target cons_target = TARGET_DEFAULTS_DEFINE;
static char *history[HISTORY_MAX], cwd[2048];
static int srow, scol;

static struct shell_command commands[] = {
	{"mount", &cmd_mount},
	{"module", &cmd_module},
	{"boot", &cmd_boot},
	{"kernel", &cmd_kernel},
	{"initrd", &cmd_initrd},
	{"cd", &cmd_cd},
	{"ls", &cmd_ls},
	{"append", NULL},
	{"cmdline", NULL},
	{"reset-vga", NULL},
	{"disks", NULL},
	{NULL, NULL}
};

void shell_main(void) {
	char cmdbuf[1024];
	size_t len, offset;
	int c, hnum, argc, cnum;
	char *nhist, *argv[ARGV_SIZE];
	
	for(hnum = 0; hnum < HISTORY_MAX; hnum++) {
		history[hnum] = NULL;
	}
	
	free_modules(cons_target.modules);
	TARGET_DEFAULTS(&cons_target);
	
	strcpy(cwd, "/");
	
	READLINE:
	cmdbuf[0] = '\0';
	len = 0;
	offset = 0;
	hnum = -1;
	
	printf("\n%s > ", cwd);
	
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
	
	parse_command(cmdbuf, &argc, argv);
	
	if(argc == 0) {
		goto READLINE;
	}
	
	if(str_eq(argv[0], "exit", -1)) {
		unmount_list(cons_target.mounts);
		free_mounts(cons_target.mounts);
		
		for(hnum = 0; hnum < HISTORY_MAX; hnum++) {
			free(history[hnum]);
		}
		
		return;
	}
	
	if(str_eq(argv[0], "help", -1)) {
		printf("Available commands:\n");
		printf("exit help");
		
		for(cnum = 0; commands[cnum].name; cnum++) {
			printf(" %s", commands[cnum].name);
		}
		
		putchar('\n');
		goto READLINE;
	}
	
	if(str_eq(argv[0], "disks", -1)) {
		list_devices();
		goto READLINE;
	}
	
	if(str_eq(argv[0], "append", -1)) {
		if(argc > 1) {
			str_copy(&cons_target.append, argv[1], -1);
		}else{
			cons_target.append = NULL;
		}
		
		goto READLINE;
	}
	if(str_eq(argv[0], "cmdline", -1)) {
		if(argc > 1) {
			str_copy(&cons_target.cmdline, argv[1], -1);
		}else{
			cons_target.cmdline = NULL;
		}
		
		goto READLINE;
	}
	if(str_eq(argv[0], "reset-vga", -1)) {
		cons_target.flags |= TARGET_RESET_VGA;
		goto READLINE;
	}
	
	for(cnum = 0; commands[cnum].func; cnum++) {
		if(str_eq(commands[cnum].name, argv[0], -1)) {
			commands[cnum].func(argc, argv);
			goto READLINE;
		}
	}
	
	printf("Unknown command: %s\n", argv[0]);
	goto READLINE;
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

/* Parse a command */
static void parse_command(char *cmd, int *argc, char **argv) {
	cmd += strspn(cmd, " ");
	*argc = 0;
	
	int pos, len = strlen(cmd), quoted;
	
	while(cmd[0] != '\0') {
		argv[*argc] = cmd;
		pos = 0;
		quoted = 0;
		
		while(cmd[pos] != '\0') {
			if(cmd[pos] == '"') {
				quoted = 1 & ~quoted;
				
				memmove(cmd+pos, cmd+pos+1, len-pos);
				len--;
				
				continue;
			}
			if(cmd[pos] == ' ' && !quoted) {
				break;
			}
			
			if(cmd[pos] == '\\') {
				memmove(cmd+pos, cmd+pos+1, len-pos);
				len--;
			}
			
			pos++;
		}
		
		cmd += pos;
		len -= pos;
		
		if(++(*argc) == ARGV_SIZE-1) {
			break;
		}
		
		if(cmd[0] != '\0') {
			cmd[0] = '\0';
			cmd++;
			
			cmd += strspn(cmd, " ");
		}
	}
	
	argv[*argc] = NULL;
}

/* Convert a shell path to a complete path
 * Shell paths are like a virtual chroot/chdir
*/
static char *shell_path(char const *spath) {
	static char path[2048];
	char ptmp[2048], *ptok;
	int depth = 0, pos, len;
	
	strcpy(path, "/mnt/target");
	
	if(spath[0] != '/') {
		strlcat(path, cwd, 2048);
		
		len = strlen(path);
		if(path[--len] == '/') {
			path[len] = '\0';
		}
		
		for(pos = 1; cwd[pos]; pos++) {
			if(cwd[pos] == '/') {
				depth++;
			}
		}
	}
	
	strlcpy(ptmp, spath, 2048);
	
	ptok = strtok(ptmp, "/");
	while(ptok) {
		if(str_eq(ptok, "..", -1)) {
			if(depth > 0) {
				strrchr(path, '/')[0] = '\0';
				depth--;
			}
			
			goto NEXT;
		}
		if(str_eq(ptok, ".", -1) || str_eq(ptok, "", -1)) {
			goto NEXT;
		}
		
		strlcat(path, "/", 2048);
		strlcat(path, ptok, 2048);
		depth++;
		
		NEXT:
		ptok = strtok(NULL, "/");
	}
	
	return path;
}

/* Mount a filesystem and add it to cons_target.mounts */
static void cmd_mount(int argc, char **argv) {
	if(argc != 3) {
		printf("Usage: mount [<fstype>:]<device> <mount point>\n");
		return;
	}
	
	kl_mount *nmount = allocate(sizeof(struct kl_mount));
	INIT_MOUNT(nmount);
	
	str_copy(&nmount->device, argv[1], -1);
	nmount->mpoint = str_printf("/mnt/target/%s", argv[1]);
	
	if(!mount_list(nmount)) {
		free(nmount);
		return;
	}
	
	nmount->next = cons_target.mounts;
	cons_target.mounts = nmount;
}

/* Add a multiboot module to cons_target.modules */
static void cmd_module(int argc, char **argv) {
	if(argc != 2) {
		printf("Usage: module <module name>\n");
		return;
	}
	
	kl_module *nptr = allocate(sizeof(kl_module));
	INIT_MODULE(nptr);
	
	nptr->module = str_printf("/mnt/target/%s", argv[1]);
	
	nptr->next = cons_target.modules;
	cons_target.modules = nptr;
}

/* Boot the kernel */
static void cmd_boot(int argc, char **argv) {
	if(!load_kernel(&cons_target)) {
		return;
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
}

/* Set the kernel path */
static void cmd_kernel(int argc, char **argv) {
	if(argc != 2) {
		printf("Usage: kernel <filename>\n");
		return;
	}
	
	free(cons_target.kernel);
	cons_target.kernel = str_printf("/mnt/target/%s", argv[1]);
}

/* Set the kernel path */
static void cmd_initrd(int argc, char **argv) {
	if(argc != 2) {
		printf("Usage: initrd <filename>\n");
		return;
	}
	
	free(cons_target.initrd);
	cons_target.initrd = str_printf("/mnt/target/%s", argv[1]);
}

/* Change the shell CWD */
static void cmd_cd(int argc, char **argv) {
	if(argc > 2) {
		printf("Usage: cd [<path>]\n");
		return;
	}
	
	if(argc == 1 || str_eq(argv[1], "/", -1)) {
		strcpy(cwd, "/");
	}else{
		char *path = shell_path(argv[1]);
		char *spath = path+strlen("/mnt/target");
		strlcat(path, "/", 2048);
		
		struct stat stbuf;
		if(stat(path, &stbuf) == -1) {
			printf("%s: %s\n", spath, strerror(errno));
			return;
		}
		
		strcpy(cwd, spath);
	}
}

/* List directory contents */
static void cmd_ls(int argc, char **argv) {
	if(argc > 2) {
		printf("Usage: ls [<path>]\n");
		return;
	}
	
	char *path = shell_path(argc == 2 ? argv[1] : "./");
	
	DIR *dir = opendir(path);
	if(!dir) {
		printf("%s: %s\n", path, strerror(errno));
		return;
	}
	
	struct dirent *node;
	struct stat stbuf;
	char timestr[1024];
	
	while((node = readdir(dir))) {
		if(lstat(node->d_name, &stbuf) == -1) {
			printf("%s: %s\n", node->d_name, strerror(errno));
			break;
		}
		
		strftime(timestr, 1024, "%Y-%m-%d %H:%M", gmtime(&(stbuf.st_mtime)));
		printf("%s\t%s\n", timestr, node->d_name);
	}
	
	closedir(dir);
}

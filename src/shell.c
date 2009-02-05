/* kexec-loader - Shell code
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
#include <errno.h>
#include <string.h>
#include <linux/reboot.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/utsname.h>

#include "console.h"
#include "misc.h"
#include "disk.h"
#include "globcmp.h"

#define CMDBUF_SIZE 1024
#define HISTORY_SIZE 32

struct shell_command {
	char const *name;
	char const *help;
	
	void (*func)(char*, char*);
};

extern int rows, cols;

static char *read_cmd(char **history);
static void erase_input(int offset, int len);
static void replace_input(char const *cmd, int offset);
static void set_cursor(int offset);
static char *sh_get_rpath(char const *path);
static char *sh_vpath(char const *path);

static void cmd_module(char *cmd, char *args);
static void cmd_ls(char *cmd, char *args);
static void cmd_find(char *cmd, char *args);
static void find_files(char *path, char const *name);
static void cmd_shutdown(char *cmd, char *args);
static void cmd_cat(char *cmd, char *args);

static kl_target target;
static int srow, scol;

static struct shell_command commands[] = {
	{"root", "root <device>\t\tSet root device", NULL},
	{"kernel", "kernel <file>\t\tSelect a kernel", NULL},
	{"initrd", "initrd <file>\t\tSelect an initrd", NULL},
	{"cmdline", "cmdline <text>\t\tSet the kernel command line", NULL},
	{"append", "append <text>\t\tLike cmdline, but less portable", NULL},
	{"boot", "boot\t\t\tBoot the system", NULL},
	{"module", "module <file> [<args>]\tLoad a multiboot module", &cmd_module},
	{"reset-vga", "reset-vga\t\tEnable the kexec --reset-vga switch", NULL},
	{"ls", "ls <path>\t\tList the contents of a directory", &cmd_ls},
	{"find", "find <name> <path>\tSearch for files named <name>", &cmd_find},
	{"cat", "cat <file>\t\tDisplay the contents of a file", &cmd_cat},
	{"disks", "disks\t\t\tDisplay disks which have been detected", NULL},
	{"reboot", "reboot\t\t\tReboot the system", &cmd_shutdown},
	{"shutdown", "shutdown\t\tPower off the system", &cmd_shutdown},
	{"exit", "exit\t\t\tReturn to the menu", NULL},
	{NULL, NULL}
};

#define TEXT_COMMAND(name, dest) \
	if(kl_streq(cmd, name)) { \
		strlcpy(dest, args, sizeof(dest)); \
		continue; \
	}

#define PATH_COMMAND(name, dest) \
	if(kl_streq(cmd, name)) { \
		if(check_vpath(args)) { \
			strlcpy(dest, args, sizeof(dest)); \
		}else{ \
			printf("Invalid path '%s'\n", args); \
		} \
		continue; \
	}

void shell_main(void) {
	struct utsname kinfo;
	char *cmd = NULL, *args, *history[HISTORY_SIZE];
	int i;
	
	for(i = 0; i < HISTORY_SIZE; i++) {
		history[i] = NULL;
	}
	
	INIT_TARGET(&target);
	
	uname(&kinfo);
	printf("kexec-loader " VERSION ", Copyright (C) 2007-2009 Daniel Collins\n");
	printf("Linux kernel version: %s\n", kinfo.release);
	printf("Type 'help' for a list of commands\n\n");
	
	while(1) {
		FOOBAR:
		printf("\r> ");
		
		free(cmd);
		cmd = read_cmd(history);
		args = next_value(cmd);
		
		if(*cmd == '\0') {
			continue;
		}
		
		debug("cmd = '%s'", cmd);
		debug("args = '%s'", args);
		
		if(kl_streq(cmd, "exit")) {
			break;
		}
		
		if(kl_streq(cmd, "help")) {
			printf("Available commands:\n\n");
			
			for(i = 0; commands[i].name; i++) {
				puts(commands[i].help);
			}
			
			continue;
		}
		
		TEXT_COMMAND("root", target.root);
		PATH_COMMAND("kernel", target.kernel);
		PATH_COMMAND("initrd", target.initrd);
		TEXT_COMMAND("cmdline", target.cmdline);
		TEXT_COMMAND("append", target.append);
		
		if(kl_streq(cmd, "reset-vga")) {
			target.flags |= TARGET_RESET;
			continue;
		}
		
		if(kl_streq(cmd, "boot")) {
			if(!target.root[0]) {
				printf("Root device not set\n");
				continue;
			}
			if(!target.kernel[0]) {
				printf("Kernel not set\n");
				continue;
			}
			
			boot_target(&target);
			continue;
		}
		
		if(kl_streq(cmd, "disks")) {
			list_disks();
			continue;
		}
		
		for(i = 0; commands[i].name; i++) {
			if(kl_streq(commands[i].name, cmd)) {
				commands[i].func(cmd, args);
				goto FOOBAR;
			}
		}
		
		printf("Unknown command: %s\n", cmd);
	}
	
	free(cmd);
	list_nuke(target.modules);
	
	for(i = 0; i < HISTORY_SIZE; i++) {
		free(history[i]);
	}
}

static char *read_cmd(char **history) {
	char cmdbuf[CMDBUF_SIZE] = {'\0'}, *cstart;
	int cmdlen = 0, c, offset = 0, i, hnum = -1;
	
	console_getpos(&scol, &srow);
	
	while(1) {
		c = getchar();
		
		if(c == '\n') {
			putchar('\n');
			break;
		}
		if(c == 0x7F) {
			/* 0x7F == DEL (Backspace??) */
			
			if(offset > 0) {
				memmove(cmdbuf+offset-1, cmdbuf+offset, cmdlen-offset+1);
				cmdlen--;
				
				erase_input(--offset, 1);
				replace_input(cmdbuf, offset);
				set_cursor(offset);
			}
			
			continue;
		}
		if(c == 0x1B) {
			if((c = getchar()) != '[') {
				ungetc(c, stdin);
				continue;
			}
			
			c = getchar();
			
			if(c == 0x44 && offset > 0) {
				/* Left arrow */
				set_cursor(--offset);
			}
			if(c == 0x43 && cmdlen - offset > 0) {
				/* Right arrow */
				set_cursor(++offset);
			}
			if(c == 0x31) {
				/* Home */
				
				getchar();
				set_cursor(offset = 0);
			}
			if(c == 0x34) {
				/* End */
				
				getchar();
				set_cursor(offset = cmdlen);
			}
			
			if(c == 0x41 && hnum+1 < HISTORY_SIZE && history[hnum+1]) {
				/* Up arrow */
				
				erase_input(0, cmdlen);
				
				strlcpy(cmdbuf, history[++hnum], CMDBUF_SIZE);
				offset = cmdlen = strlen(cmdbuf);
				
				replace_input(cmdbuf, 0);
			}
			if(c == 0x42 && hnum >= 0) {
				/* Down arrow */
				
				erase_input(0, cmdlen);
				
				if(--hnum == -1) {
					cmdbuf[0] = '\0';
					offset = cmdlen = 0;
				}else{
					strlcpy(cmdbuf, history[hnum], CMDBUF_SIZE);
					offset = cmdlen = strlen(cmdbuf);
				}
				
				replace_input(cmdbuf, 0);
			}
			
			continue;
		}
		if(c == '\t') {
			continue;
		}
		
		if(cmdlen+2 < CMDBUF_SIZE) {
			memmove(cmdbuf+offset+1, cmdbuf+offset, cmdlen-offset);
			cmdbuf[offset] = c;
			cmdbuf[++cmdlen] = '\0';
			
			replace_input(cmdbuf, offset);
			set_cursor(++offset);
		}
	}
	
	cstart = cmdbuf+strspn(cmdbuf, " ");
	if(*cstart) {
		free(history[HISTORY_SIZE-1]);
		
		for(i = HISTORY_SIZE-1; i > 0; i--) {
			history[i] = history[i-1];
		}
		
		history[0] = kl_strdup(cmdbuf);
	}
	
	return kl_strdup(cstart);
}

/* Erase len bytes at offset from the display */
static void erase_input(int offset, int len) {
	int row = srow + ((scol + offset) / console_cols);
	int col = (scol + offset) % console_cols;
	
	int erow = row + ((col + len) / console_cols);
	
	while(erow > row) {
		console_setpos(0, erow);
		console_erase(ERASE_LINE);
		
		erow--;
	}
	
	console_setpos(col, row);
	console_erase(ERASE_EOL);
}

/* Overwrite text on the display */
static void replace_input(char const *cmd, int offset) {
	int row = srow + ((scol + offset) / console_cols);
	int col = (scol + offset) % console_cols;
	
	int nrows = row + ((col + strlen(cmd+offset)) / console_cols) + 1;
	
	while(nrows > console_rows) {
		putchar('\n');
		srow--;
		row--;
		nrows--;
	}
	
	console_setpos(col, row);
	
	fputs(cmd+offset, stdout);
}

/* Move the cursor to the correct offset */
static void set_cursor(int offset) {
	int row = srow + ((scol + offset) / console_cols);
	int col = (scol + offset) % console_cols;
	
	console_setpos(col, row);
}

static char *sh_get_rpath(char const *path) {
	if(!check_vpath(path)) {
		printf("Invalid path\n");
		return NULL;
	}
	
	if(!target.root[0] && *path != '(') {
		printf("No device specified\n");
		return NULL;
	}
	
	char const *errmsg;
	char *rpath = get_rpath(target.root, path, &errmsg);
	if(!rpath) {
		printf("Mount error: %s\n", errmsg);
		return NULL;
	}
	
	return rpath;
}

static char *sh_vpath(char const *path) {
	static char buf[1024];
	int c = 0;
	
	while(*path) {
		if(*path == '/' && ++c == 2) {
			path++;
			break;
		}
		
		path++;
	}
	
	c = strcspn(path, "/");
	strcpy(buf, "(");
	strlcat(buf, path, c+2);
	strlcat(buf, ")/", sizeof(buf));
	
	path += c;
	path += strspn(path, "/");
	strlcat(buf, path, sizeof(buf));
	
	return buf;
}

/* Add a multiboot module to the target */
static void cmd_module(char *cmd, char *args) {
	if(*args == '\0') {
		printf("Usage: module <file> [<args>]\n");
		return;
	}
	
	kl_module mod;
	INIT_MODULE(&mod);
	
	strlcpy(mod.args, next_value(args), sizeof(mod.args));
	strlcpy(mod.name, args, sizeof(mod.args));
	
	list_add_copy(&(target.modules), &mod, sizeof(mod));
}

/* List directory contents */
static void cmd_ls(char *cmd, char *args) {
	if(*args == '\0') {
		printf("Usage: ls <path>\n");
		return;
	}
	
	char *path = sh_get_rpath(args);
	if(!path) {
		return;
	}
	
	DIR *dir = opendir(path);
	if(!dir) {
		printf("Cannot open %s: %s\n", sh_vpath(path), strerror(errno));
		free(path);
		
		return;
	}
	
	struct dirent *node;
	struct stat stbuf;
	char timestr[1024], filename[1024];
	
	while((node = readdir(dir))) {
		snprintf(filename, 1024, "%s/%s", path, node->d_name);
		
		if(lstat(filename, &stbuf) == -1) {
			printf("Cannot stat %s: %s\n", sh_vpath(filename), strerror(errno));
			break;
		}
		
		strftime(timestr, 1024, "%Y-%m-%d %H:%M", gmtime(&(stbuf.st_mtime)));
		
		if(stbuf.st_mode & S_IFDIR) {
			printf("%s\t%s/\n", timestr, node->d_name);
		}else{
			printf("%s\t%s\n", timestr, node->d_name);
		}
	}
	
	closedir(dir);
	free(path);
}

/* Search for a file */
static void cmd_find(char *cmd, char *args) {
	char *dir = next_value(args);
	
	if(!dir) {
		printf("Usage: find <filename> <directory>\n");
		return;
	}
	
	char *path = sh_get_rpath(dir);
	if(!path) {
		return;
	}
	
	find_files(path, args);
	free(path);
}

/* Do the actual work */
static void find_files(char *path, char const *name) {
	DIR *dir = opendir(path);
	if(!dir) {
		printf("Cannot open %s: %s\n", sh_vpath(path), strerror(errno));
		return;
	}
	
	struct dirent *node;
	struct stat stbuf;
	char *npath = NULL;
	
	while((node = readdir(dir))) {
		if(kl_streq(node->d_name, ".") || kl_streq(node->d_name, "..")) {
			continue;
		}
		
		free(npath);
		npath = kl_sprintf("%s/%s", path, node->d_name);
		
		if(stat(npath, &stbuf) == -1) {
			printf("Cannot stat %s: %s\n", sh_vpath(npath), strerror(errno));
			continue;
		}
		
		if(globcmp(node->d_name, name, GLOB_IGNCASE | GLOB_STAR | GLOB_SINGLE)) {
			if(stbuf.st_mode & S_IFDIR) {
				printf("%s/\n", sh_vpath(npath));
			}else{
				printf("%s\n", sh_vpath(npath));
			}
		}
		
		if(stbuf.st_mode & S_IFDIR) {
			find_files(npath, name);
		}
	}
	
	closedir(dir);
	free(npath);
}

/* Shutdown or reboot */
static void cmd_shutdown(char *cmd, char *args) {
	if(kl_streq(cmd, "reboot")) {
		call_reboot(LINUX_REBOOT_CMD_RESTART);
	}else{
		call_reboot(LINUX_REBOOT_CMD_POWER_OFF);
	}
	
	puts(strerror(errno));
}

/* Display the contents of a file */
static void cmd_cat(char *cmd, char *args) {
	if(*args == '\0') {
		printf("Usage: cat <file>\n");
		return;
	}
	
	char *filename = sh_get_rpath(args);
	if(!filename) {
		return;
	}
	
	FILE *fh = fopen(filename, "r");
	if(!fh) {
		printf("Cannot open %s: %s\n", sh_vpath(args), strerror(errno));
		return;
	}
	
	char buf[1024];
	while(fgets(buf, 1024, fh)) {
		fputs(buf, stdout);
	}
	
	if(buf[strlen(buf)-1] != '\n') {
		putchar('\n');
	}
	
	if(ferror(fh)) {
		printf("Cannot read %s: %s\n", sh_vpath(args), strerror(errno));
	}
	
	fclose(fh);
}

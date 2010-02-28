/* kexec-loader - Shell code
 * Copyright (C) 2007-2010 Daniel Collins <solemnwarning@solemnwarning.net>
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
#include "vfs.h"

#define CMDBUF_SIZE 1024
#define HISTORY_SIZE 32
#define AC_SIZE 1024

enum ac_mode {
	ac_none = 0,
	ac_cmd,
	ac_file,
	ac_dir,
	ac_dev
};

struct shell_command {
	char const *name;
	char const *help;
	enum ac_mode mode;
	
	void (*func)(char*, char*);
};

struct ac_list {
	struct ac_list *next;
	
	char append[AC_SIZE];
	char display[AC_SIZE];
};

extern int rows, cols;

static char *read_cmd(char **history);
static void erase_input(int offset, int len);
static void replace_input(char const *cmd, int offset);
static void set_cursor(int offset);
static struct ac_list *ac_search(char *cmd, int offset);
static void ac_add(struct ac_list **root, char const *a1, char const *a2, char const *d1, char const *d2);
static void ac_path(struct ac_list **list, char const *argx, enum ac_mode mode);

static void cmd_module(char *cmd, char *args);
static void cmd_ls(char *cmd, char *args);
static void cmd_find(char *cmd, char *args);
static void find_files(char *path, char const *name);
static void cmd_cat(char *cmd, char *args);

static kl_target target;
static int srow, scol;

static struct shell_command commands[] = {
	{"root", "root <device>\t\tSet root device", ac_dev, NULL},
	{"kernel", "kernel <file>\t\tSelect a kernel", ac_file, NULL},
	{"initrd", "initrd <file>\t\tSelect an initrd", ac_file, NULL},
	{"cmdline", "cmdline <text>\t\tSet the kernel command line", ac_none, NULL},
	{"append", "append <text>\t\tLike cmdline, but less portable", ac_none, NULL},
	{"boot", "boot\t\t\tBoot the system", ac_none, NULL},
	{"module", "module <file> [<args>]\tLoad a multiboot module", ac_file, &cmd_module},
	{"reset-vga", "reset-vga\t\tEnable the kexec --reset-vga switch", ac_none, NULL},
	{"ls", "ls <path>\t\tList the contents of a directory", ac_dir, &cmd_ls},
	{"find", "find <name> <path>\tSearch for files named <name>", ac_dir, &cmd_find},
	{"cat", "cat <file>\t\tDisplay the contents of a file", ac_file, &cmd_cat},
	{"disks", "disks\t\t\tDisplay disks which have been detected", ac_none, NULL},
	{"exit", "exit\t\t\tReturn to the menu", ac_none, NULL},
	{NULL, NULL}
};

#define TEXT_COMMAND(name, dest) \
	if(kl_streq(cmd, name)) { \
		strlcpy(dest, args, sizeof(dest)); \
		continue; \
	}

#define PATH_COMMAND(name, dest) \
	if(kl_streq(cmd, name)) { \
		if(!args[0] || vfs_exists(args)) { \
			strlcpy(dest, args, sizeof(dest)); \
		}else{ \
			printf("Error: %s\n", kl_strerror(errno)); \
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
	
	vfs_set_root(NULL);
	
	uname(&kinfo);
	printf("kexec-loader " VERSION ", Copyright (C) 2007-2010 Daniel Collins\n");
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
		
		PATH_COMMAND("kernel", target.kernel);
		PATH_COMMAND("initrd", target.initrd);
		TEXT_COMMAND("cmdline", target.cmdline);
		TEXT_COMMAND("append", target.append);
		
		if(kl_streq(cmd, "root")) {
			vfs_set_root(args[0] ? args : NULL);
			strlcpy(target.root, args, sizeof(target.root));
			
			continue;
		}
		
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
		c = console_getchar();
		
		if(c == '\n') {
			putchar('\n');
			break;
		}
		if(c == KEY_BACKSPACE) {
			if(offset > 0) {
				memmove(cmdbuf+offset-1, cmdbuf+offset, cmdlen-offset+1);
				offset--;
				
				erase_input(offset, cmdlen-offset);
				cmdlen--;
				
				replace_input(cmdbuf, offset);
				set_cursor(offset);
			}
		}
		if(c == KEY_LEFT && offset > 0) {
			set_cursor(--offset);
		}
		if(c == KEY_RIGHT && cmdlen - offset > 0) {
			set_cursor(++offset);
		}
		if(c == KEY_HOME) {
			set_cursor(offset = 0);
		}
		if(c == KEY_END) {
			set_cursor(offset = cmdlen);
		}
		if(c == KEY_DEL) {
			if(offset < cmdlen) {
				memmove(cmdbuf+offset, cmdbuf+offset+1, cmdlen-offset+1);
				
				erase_input(offset, cmdlen-offset);
				cmdlen--;
				
				replace_input(cmdbuf, offset);
				set_cursor(offset);
			}
		}
		if(c == KEY_UP && hnum+1 < HISTORY_SIZE && history[hnum+1]) {
			erase_input(0, cmdlen);
			
			strlcpy(cmdbuf, history[++hnum], CMDBUF_SIZE);
			offset = cmdlen = strlen(cmdbuf);
			
			replace_input(cmdbuf, 0);
		}
		if(c == KEY_DOWN && hnum >= 0) {
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
		if(c == '\t') {
			struct ac_list *ac_list = ac_search(cmdbuf, offset);
			struct ac_list *ac_ptr = ac_list;
			
			if(ac_ptr && !ac_ptr->next) {
				i = strlen(ac_ptr->append);
				
				char *tmp = kl_strdup(cmdbuf+offset);
				strlcpy(cmdbuf+offset, ac_ptr->append, CMDBUF_SIZE-offset);
				strlcat(cmdbuf, tmp, CMDBUF_SIZE);
				free(tmp);
				
				cmdlen = strlen(cmdbuf);
				replace_input(cmdbuf, offset);
				
				if(offset+i+1 > CMDBUF_SIZE) {
					set_cursor(offset = cmdlen);
				}else{
					set_cursor(offset += i);
				}
			}else if(ac_ptr) {
				putchar('\n');
				
				while(ac_ptr) {
					printf("%s\n", ac_ptr->display);
					ac_ptr = ac_ptr->next;
				}
				
				printf("> ");
				console_getpos(&scol, &srow);
				replace_input(cmdbuf, 0);
			}
			
			list_nuke(ac_list);
			continue;
		}
		
		if(c < -127) {
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

static struct ac_list *ac_search(char *cmd, int offset) {
	int i, len;
	struct ac_list *ac_list = NULL;
	enum ac_mode mode = ac_none;
	
	char *argbuf = kl_strndup(cmd, offset);
	char *arg0 = NULL, *argx = argbuf+strspn(argbuf, " ");
	
	if(strchr(argx, ' ')) {
		arg0 = argx;
		argx = next_value(argx);
	}else{
		mode = ac_cmd;
	}
	
	len = strlen(argx);
	
	if(arg0) {
		for(i = 0; commands[i].name; i++) {
			if(kl_streq(commands[i].name, arg0)) {
				mode = commands[i].mode;
			}
		}
		
		if(kl_streq(arg0, "find")) {
			argx = next_value(argx);
			if(!*argx) {
				mode = ac_none;
			}
		}
	}
	
	if(mode == ac_cmd) {
		for(i = 0; commands[i].name; i++) {
			if(kl_strneq(commands[i].name, argx, len)) {
				ac_add(&ac_list, commands[i].name+len, " ", commands[i].name, "");
			}
		}
	}
	if(mode == ac_dev) {
		kl_disk *disks = get_disks(NULL);
		kl_disk *disk = disks;
		
		if(kl_strneq(argx, "UUID=", len) && len < 5) {
			ac_add(&ac_list, "UUID="+len, "", "UUID=...", "");
		}
		if(kl_strneq(argx, "LABEL=", len) && len < 6) {
			ac_add(&ac_list, "LABEL="+len, "", "LABEL=...", "");
		}
		
		while(disk) {
			if(kl_strneq(argx, "UUID=", 5)) {
				if(disk->uuid[0] && kl_strnceq(disk->uuid, argx+5, len-5)) {
					ac_add(&ac_list, disk->uuid+len-5, "", disk->uuid, "");
				}
			}else if(kl_strneq(argx, "LABEL=", 6)) {
				if(disk->label[0] && kl_strneq(disk->label, argx+6, len-6)) {
					ac_add(&ac_list, disk->label+len-6, "", disk->label, "");
				}
			}else{
				if(kl_strneq(disk->name, argx, len)) {
					ac_add(&ac_list, disk->name+len, "", disk->name, "");
				}
			}
			
			disk = disk->next;
		}
		
		list_nuke(disks);
	}
	if(mode == ac_file || mode == ac_dir) {
		if(*argx == '\0' || (*argx == '(' && !strchr(argx, ')'))) {
			kl_disk *disks = get_disks(NULL);
			kl_disk *disk = disks;
			
			if(kl_strneq(argx, "(UUID=", len) && len < 6) {
				ac_add(&ac_list, "(UUID="+len, "", "(UUID=...)", "");
			}
			if(kl_strneq(argx, "(LABEL=", len) && len < 7) {
				ac_add(&ac_list, "(LABEL="+len, "", "(LABEL=...)", "");
			}
			
			while(disk) {
				if(disk->uuid[0]) {
					kl_strins(disk->uuid, "(UUID=", 0, sizeof(disk->uuid));
					strlcat(disk->uuid, ")", sizeof(disk->uuid));
				}
				if(disk->label[0]) {
					kl_strins(disk->label, "(LABEL=", 0, sizeof(disk->label));
					strlcat(disk->label, ")", sizeof(disk->label));
				}
				if(disk->name[0]) {
					kl_strins(disk->name, "(", 0, sizeof(disk->name));
					strlcat(disk->name, ")", sizeof(disk->name));
				}
				
				if(disk->uuid[0] && kl_strneq(argx, "(UUID=", 6) && kl_strnceq(disk->uuid, argx, len)) {
					ac_add(&ac_list, disk->uuid+len, "", disk->uuid, "");
				}
				if(disk->label[0] && kl_strneq(argx, "(LABEL=", 7)  && kl_strneq(disk->label, argx, len)) {
					ac_add(&ac_list, disk->label+len, "", disk->label, "");
				}
				if(kl_strneq(disk->name, argx, len)) {
					ac_add(&ac_list, disk->name+len, "", disk->name, "");
				}
				
				disk = disk->next;
			}
			
			list_nuke(disks);
		}
		
		if(*argx != '(' || strchr(argx, ')')) {
			ac_path(&ac_list, argx, mode);
		}
	}
	
	free(argbuf);
	return ac_list;
}

static void ac_add(struct ac_list **root, char const *a1, char const *a2, char const *d1, char const *d2) {
	struct ac_list *ptr = kl_malloc(sizeof(struct ac_list));
	strlcpy(ptr->append, a1, AC_SIZE);
	strlcat(ptr->append, a2, AC_SIZE);
	strlcpy(ptr->display, d1, AC_SIZE);
	strlcat(ptr->display, d2, AC_SIZE);
	ptr->next = NULL;
	
	list_add(root, ptr);
}

static void ac_path(struct ac_list **list, char const *argx, enum ac_mode mode) {
	char const *vpath = argx;
	if(*argx == '(') {
		argx = strchr(argx, ')')+1;
	}
	
	if(*argx == '\0') {
		ac_add(list, "/", "", "/", "");
		return;
	}
	if(*argx != '/') {
		return;
	}
	
	char *rpath = kl_strdup(vpath);
	
	if(argx[strlen(argx)-1] == '/') {
		argx += strlen(argx);
	}else{
		strrchr(rpath, '/')[1] = '\0';
		argx = strrchr(argx, '/')+1;
	}
	
	DIR *dir = vfs_opendir(rpath);
	if(!dir) {
		debug("vfs_opendir(%s) failed: %s", rpath, kl_strerror(errno));
		free(rpath);
		
		return;
	}
	
	int alen = strlen(argx);
	struct dirent *node;
	struct stat info;
	
	while((node = readdir(dir))) {
		if(!kl_strneq(argx, node->d_name, alen)) {
			continue;
		}
		if(node->d_name[0] == '.' && !alen) {
			continue;
		}
		
		char *sp = kl_sprintf("%s/%s", rpath, node->d_name);
		if(vfs_stat(sp, &info) == -1) {
			debug("vfs_stat(%s) failed: %s", sp, kl_strerror(errno));
			free(sp);
			
			continue;
		}
		
		free(sp);
		
		if(!S_ISDIR(info.st_mode) && mode == ac_dir) {
			continue;
		}
		
		char *s2 = (S_ISDIR(info.st_mode) ? "/" : "");
		ac_add(list, node->d_name+alen, s2, node->d_name+alen, s2);
	}
	
	closedir(dir);
	free(rpath);
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
	
	DIR *dir = vfs_opendir(args);
	if(!dir) {
		printf("Cannot open %s: %s\n", args, kl_strerror(errno));
		return;
	}
	
	struct dirent *node;
	struct stat stbuf;
	char filename[1024];
	
	while((node = readdir(dir))) {
		if(kl_streq(node->d_name, ".") || kl_streq(node->d_name, "..")) {
			continue;
		}
		
		snprintf(filename, 1024, "%s/%s", args, node->d_name);
		
		if(vfs_lstat(filename, &stbuf) == -1) {
			printf("Cannot stat %s: %s\n", args, kl_strerror(errno));
			break;
		}
		
		if(stbuf.st_mode & S_IFDIR) {
			printf("%s/\n", node->d_name);
		}else{
			printf("%s\n", node->d_name);
		}
	}
	
	closedir(dir);
}

/* Search for a file */
static void cmd_find(char *cmd, char *args) {
	char *dir = next_value(args);
	
	if(!dir) {
		printf("Usage: find <filename> <directory>\n");
		return;
	}
	
	find_files(dir, args);
}

/* Do the actual work */
static void find_files(char *path, char const *name) {
	DIR *dir = vfs_opendir(path);
	if(!dir) {
		printf("Cannot open %s: %s\n", path, kl_strerror(errno));
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
		
		if(vfs_stat(npath, &stbuf) == -1) {
			printf("Cannot stat %s: %s\n", npath, kl_strerror(errno));
			continue;
		}
		
		if(globcmp(node->d_name, name, GLOB_IGNCASE | GLOB_STAR | GLOB_SINGLE)) {
			if(stbuf.st_mode & S_IFDIR) {
				printf("%s/\n", npath);
			}else{
				printf("%s\n", npath);
			}
		}
		
		if(stbuf.st_mode & S_IFDIR) {
			find_files(npath, name);
		}
	}
	
	closedir(dir);
	free(npath);
}

/* Display the contents of a file */
static void cmd_cat(char *cmd, char *args) {
	if(*args == '\0') {
		printf("Usage: cat <file>\n");
		return;
	}
	
	FILE *fh = vfs_fopen(args, "r");
	if(!fh) {
		printf("Cannot open %s: %s\n", args, kl_strerror(errno));
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
		printf("Cannot read %s: %s\n", args, strerror(errno));
	}
	
	fclose(fh);
}

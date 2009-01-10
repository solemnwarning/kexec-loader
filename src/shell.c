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
#include <sys/utsname.h>

#include "shell.h"
#include "console.h"
#include "misc.h"
#include "mount.h"
#include "kexec.h"
#include "mystring.h"

#define HISTORY_MAX 32
#define ARGV_SIZE 128
#define SHELL_ROOT "/mnt/target"
#define ARRAY_STEP 16

struct shell_command {
	char const *name;
	void (*func)(int, char**);
};

extern int rows, cols;

void list_devices(void);
static char *shell_path(char const *spath);
static char *shell_spath(char const *path);
static char *read_cmd(char **history);
static void erase_input(int offset, int len);
static void replace_input(char const *cmd, int offset);
static void set_cursor(int offset);
static char *parse_arg(char const *arg, int single, char **next);
static void parse_cmdline(char *cmdline, int *argc, char ***argv, char **args);

static void cmd_mount(int argc, char **argv);
static void cmd_module(int argc, char **argv);
static void cmd_boot(int argc, char **argv);
static void cmd_kernel(int argc, char **argv);
static void cmd_initrd(int argc, char **argv);
static void cmd_cd(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_find(int argc, char **argv);
static void find_files(char *path, char const *name);
static void cmd_uname(int argc, char **argv);
static void cmd_shutdown(int argc, char **argv);
static void ac_suggest(char const *str, char const *sofar);
static char *ac_finish(void);
static void cmd_cat(int argc, char **argv);

static kl_target cons_target = TARGET_DEFAULTS_DEFINE;
static char cwd[2048];
static int srow, scol;
static char **ac_list = NULL;

static struct shell_command commands[] = {
	{"mount", &cmd_mount},
	{"module", &cmd_module},
	{"boot", &cmd_boot},
	{"kernel", &cmd_kernel},
	{"initrd", &cmd_initrd},
	{"cd", &cmd_cd},
	{"ls", &cmd_ls},
	{"find", &cmd_find},
	{"uname", &cmd_uname},
	{"reboot", &cmd_shutdown},
	{"halt", &cmd_shutdown},
	{"cat", &cmd_cat},
	{"append", NULL},
	{"cmdline", NULL},
	{"reset-vga", NULL},
	{"disks", NULL},
	{"exit", NULL},
	{NULL, NULL}
};

#define SMAIN_CLEANUP() \
	for(i = 0; i < argc; i++) { \
		free(argv[i]); \
	} \
	free(command); \
	free(argv); \
	free(args); \
	command = NULL; \
	argv = NULL; \
	args = NULL;

void shell_main(void) {;
	int hnum, cnum;
	char *command = NULL, **argv = NULL, *args = NULL;
	int argc = 0, i;
	
	char *history[HISTORY_MAX];
	
	for(hnum = 0; hnum < HISTORY_MAX; hnum++) {
		history[hnum] = NULL;
	}
	
	free_modules(cons_target.modules);
	TARGET_DEFAULTS(&cons_target);
	
	strcpy(cwd, "/");
	
	INPUT:
	SMAIN_CLEANUP();
	printf("\r%s > ", cwd);
	
	command = read_cmd((char**)&history);
	
	parse_cmdline(command, &argc, &argv, &args);
	
	debug("command = '%s'\n", command);
	debug("args = '%s'\n", args);
	debug("argc = %d\n", argc);
	
	for(i = 0; i < argc; i++) {
		debug("argv[%d] = '%s'\n", i, argv[i]);
	}
	
	if(argc == 0) {
		goto INPUT;
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
		goto INPUT;
	}
	
	if(str_eq(argv[0], "disks", -1)) {
		list_devices();
		goto INPUT;
	}
	
	if(str_eq(argv[0], "append", -1)) {
		if(argc > 1) {
			str_copy(&cons_target.append, argv[1], -1);
		}else{
			cons_target.append = NULL;
		}
		
		goto INPUT;
	}
	if(str_eq(argv[0], "cmdline", -1)) {
		if(argc > 1) {
			str_copy(&cons_target.cmdline, argv[1], -1);
		}else{
			cons_target.cmdline = NULL;
		}
		
		goto INPUT;
	}
	if(str_eq(argv[0], "reset-vga", -1)) {
		cons_target.flags |= TARGET_RESET_VGA;
		goto INPUT;
	}
	
	for(cnum = 0; commands[cnum].func; cnum++) {
		if(str_eq(commands[cnum].name, argv[0], -1)) {
			commands[cnum].func(argc, argv);
			goto INPUT;
		}
	}
	
	printf("Unknown command: %s\n", argv[0]);
	goto INPUT;
}

/* Convert a shell path to a complete path
 * Shell paths are like a virtual chroot/chdir
*/
static char *shell_path(char const *spath) {
	static char path[2048];
	char ptmp[2048], *ptok;
	int depth = 0, pos, len;
	
	strcpy(path, SHELL_ROOT);
	
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

/* Return a shell path */
static char *shell_spath(char const *path) {
	int rlen = strlen(SHELL_ROOT);
	
	if(str_eq(path, SHELL_ROOT, rlen)) {
		return (char*)path+rlen;
	}else{
		return (char*)path;
	}
}

static char *read_cmd(char **history) {
	char *cmdbuf = str_copy(NULL, "", -1);
	int cmdsize = 1, cmdlen = 0, c, offset = 0, i, hnum = -1;
	
	term_getpos(&scol, &srow);
	
	while(1) {
		c = getchar();
		
		if(c == '\n') {
			putchar('\n');
			break;
		}
		if(c == 0x7F) {
			/* 0x7F == DEL (Backspace??) */
			
			if(offset > 0) {
				memmove(cmdbuf+offset-1, cmdbuf+offset, cmdlen--);
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
			
			if(c == 0x41 && hnum+1 < HISTORY_MAX && history[hnum+1]) {
				/* Up arrow */
				
				erase_input(0, cmdlen);
				
				str_copy(&cmdbuf, history[++hnum], -1);
				offset = cmdlen = strlen(cmdbuf);
				cmdsize = cmdlen+1;
				
				replace_input(cmdbuf, 0);
			}
			if(c == 0x42 && hnum >= 0) {
				/* Down arrow */
				
				erase_input(0, cmdlen);
				
				if(--hnum == -1) {
					cmdbuf[0] = '\0';
					offset = cmdlen = 0;
				}else{
					str_copy(&cmdbuf, history[hnum], -1);
					offset = cmdlen = strlen(cmdbuf);
					cmdsize = cmdlen+1;
				}
				
				replace_input(cmdbuf, 0);
			}
			
			continue;
		}
		if(c == '\t') {
			continue;
		}
		
		if(cmdlen+2 > cmdsize) {
			cmdbuf = reallocate(cmdbuf, cmdsize += 256);
		}
		
		memmove(cmdbuf+offset+1, cmdbuf+offset, cmdlen-offset);
		cmdbuf[offset] = c;
		cmdbuf[++cmdlen] = '\0';
		
		replace_input(cmdbuf, offset);
		set_cursor(++offset);
	}
	
	if(cmdbuf+strspn(cmdbuf, " ") != '\0') {
		free(history[HISTORY_MAX-1]);
		
		for(i = HISTORY_MAX-1; i > 0; i--) {
			history[i] = history[i-1];
		}
		
		history[0] = str_copy(NULL, cmdbuf, -1);
	}
	
	return cmdbuf;
}

/* Erase len bytes at offset from the display */
static void erase_input(int offset, int len) {
	int row = srow + ((scol + offset) / term_cols);
	int col = (scol + offset) % term_cols;
	
	int erow = row + ((col + len) / term_cols);
	
	while(erow > row) {
		term_setpos(0, erow);
		term_erase(ERASE_LINE);
		
		erow--;
	}
	
	term_setpos(col, row);
	term_erase(ERASE_EOL);
}

/* Overwrite text on the display */
static void replace_input(char const *cmd, int offset) {
	int row = srow + ((scol + offset) / term_cols);
	int col = (scol + offset) % term_cols;
	
	int nrows = row + ((col + strlen(cmd+offset)) / term_cols) + 1;
	
	while(nrows > term_rows) {
		putchar('\n');
		srow--;
		row--;
		nrows--;
	}
	
	term_setpos(col, row);
	
	fputs(cmd+offset, stdout);
}

/* Move the cursor to the correct offset */
static void set_cursor(int offset) {
	int row = srow + ((scol + offset) / term_cols);
	int col = (scol + offset) % term_cols;
	
	term_setpos(col, row);
}

/* Parse one or more arguments */
static char *parse_arg(char const *arg, int single, char **next) {
	int i, len = 0, q = 0;
	
	for(i = 0; arg[i]; i++) {
		if(arg[i] == ' ' && single && !q) {
			break;
		}
		if(arg[i] == '\"') {
			q = ~q;
			continue;
		}
		if(arg[i] == '\\') {
			i++;
		}
		
		len++;
	}
	
	char *ret = allocate(len+1);
	len = 0, q = 0;
	
	for(i = 0; arg[i]; i++) {
		if(arg[i] == ' ' && single && !q) {
			break;
		}
		if(arg[i] == '\"') {
			q = ~q;
			continue;
		}
		if(arg[i] == '\\') {
			i++;
		}
		
		ret[len++] = arg[i];
	}
	
	if(next) {
		*next = (char*)(arg + i + strspn(arg+i, " "));
	}
	
	ret[len] = '\0';
	return ret;
}

/* Parse a command */
static void parse_cmdline(char *cmdline, int *argc, char ***argv, char **args) {
	*argc = 0;
	*argv = allocate(sizeof(char*));
	*argv[0] = NULL;
	
	cmdline += strspn(cmdline, " ");
	
	while(cmdline[0]) {
		*argv = reallocate(*argv, sizeof(char*) * ((*argc) + 2));
		(*argv)[(*argc)++] = parse_arg(cmdline, 1, &cmdline);
		(*argv)[*argc] = NULL;
		
		if(*argc == 1) {
			*args = parse_arg(cmdline, 0, NULL);
		}
	}
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
	str_copy(&nmount->mpoint, shell_path(argv[2]), -1);
	
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
		char *spath = shell_spath(argv[1]);
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
	char filename[2048];
	
	DIR *dir = opendir(path);
	if(!dir) {
		printf("%s: %s\n", path, strerror(errno));
		return;
	}
	
	struct dirent *node;
	struct stat stbuf;
	char timestr[1024];
	
	while((node = readdir(dir))) {
		snprintf(filename, 2048, "%s/%s", path, node->d_name);
		
		if(lstat(filename, &stbuf) == -1) {
			printf("%s: %s\n", node->d_name, strerror(errno));
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
}

/* Search for a file */
static void cmd_find(int argc, char **argv) {
	if(argc < 2 || argc > 3) {
		printf("Usage: find <filename> [<directory>]\n");
		return;
	}
	
	char *path = shell_path(argc == 3 ? argv[2] : "./");
	find_files(path, argv[1]);
}

/* Do the actual work */
static void find_files(char *path, char const *name) {
	DIR *dir = opendir(path);
	if(!dir) {
		printf("%s: %s\n", path, strerror(errno));
		return;
	}
	
	struct dirent *node;
	struct stat stbuf;
	
	while((node = readdir(dir))) {
		if(str_eq(node->d_name, ".", -1) || str_eq(node->d_name, "..", -1)) {
			continue;
		}
		
		strlcat(path, "/", 2048);
		strlcat(path, node->d_name, 2048);
		
		if(stat(path, &stbuf) == -1) {
			printf("%s: %s\n", shell_spath(path), strerror(errno));
			
			strrchr(path, '/')[0] = '\0';
			continue;
		}
		
		if(globcmp(node->d_name, name, GLOB_IGNCASE | GLOB_STAR | GLOB_SINGLE)) {
			if(stbuf.st_mode & S_IFDIR) {
				printf("%s/\n", shell_spath(path));
			}else{
				printf("%s\n", shell_spath(path));
			}
		}
		
		if(stbuf.st_mode & S_IFDIR) {
			find_files(path, name);
		}
		
		strrchr(path, '/')[0] = '\0';
	}
	
	closedir(dir);
}

/* Display kernel information */
static void cmd_uname(int argc, char **argv) {
	if(argc != 1) {
		printf("Usage: uname\n");
		return;
	}
	
	struct utsname undata;
	uname(&undata);
	
	printf("Linux %s %s\n", undata.release, undata.version);
}

/* Shutdown or reboot */
static void cmd_shutdown(int argc, char **argv) {
	if(argc != 1) {
		printf("Usage: %s\n", argv[0]);
		return;
	}
	
	unmount_list(cons_target.mounts);
	sync();
	
	if(str_eq(argv[0], "reboot", -1)) {
		syscall(
			__NR_reboot,
			LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
			LINUX_REBOOT_CMD_RESTART, NULL
		);
	}else{
		syscall(
			__NR_reboot,
			LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
			LINUX_REBOOT_CMD_POWER_OFF, NULL
		);
	}
	
	puts(strerror(errno));
}

/* Add to the suggestion list */
static void ac_suggest(char const *str, char const *sofar) {
	int lsize = 0, npos = 0;
	
	if(!str_eq(str, sofar, strlen(sofar))) {
		return;
	}
	
	while(ac_list && ac_list[lsize++]) { npos++; }
	
	if(lsize % ARRAY_STEP == 0) {
		lsize += ARRAY_STEP;
		ac_list = reallocate(ac_list, lsize * sizeof(char*));
	}
	
	ac_list[npos++] = str_copy(NULL, str, -1);
	ac_list[npos] = NULL;
}

/* Return the best match from the autocomplete list */
static char *ac_finish(void) {
	char *match = NULL;
	int pos, mlen;
	
	for(pos = 0; ac_list && ac_list[pos]; pos++) {
		if(pos == 0) {
			match = ac_list[0];
			mlen = strlen(match);
		}else{
			mlen = str_fdiff(match, ac_list[pos], mlen);
			
			if(pos == 1) {
				printf("\n%s", match);
			}
			
			printf(" %s", ac_list[pos]);
		}
	}
	
	if(pos > 1) {
		putchar('\n');
	}
	if(match && mlen > 0) {
		match = str_copy(NULL, match, mlen);
	}
	
	for(pos = 0; ac_list && ac_list[pos]; pos++) {
		free(ac_list[pos]);
	}
	
	free(ac_list);
	ac_list = NULL;
	
	return match;
}

/* Display the contents of a file */
static void cmd_cat(int argc, char **argv) {
	if(argc != 2) {
		printf("Usage: cat <file>\n");
		return;
	}
	
	char *filename = shell_path(argv[1]);
	
	FILE *fh = fopen(filename, "r");
	if(!fh) {
		printf("Can't open %s: %s\n", argv[1], strerror(errno));
		return;
	}
	
	char buf[1024];
	while(fgets(buf, 1024, fh)) {
		printf("%s", buf);
	}
	
	if(buf[strlen(buf)-1] != '\n') {
		putchar('\n');
	}
	
	if(ferror(fh)) {
		printf("Failed to read %s: %s\n", argv[1], strerror(errno));
	}
	
	fclose(fh);
}

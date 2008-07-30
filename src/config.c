/* kexec-loader - Configuration functions
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/syscall.h>

#include "config.h"
#include "../config.h"
#include "misc.h"
#include "mount.h"
#include "console.h"
#include "grub.h"
#include "mystring.h"
#include "elf.h"

struct kl_config config = CONFIG_DEFAULTS_DEFINE;
kl_module *k_modules = NULL;

static struct kl_target target = TARGET_DEFAULTS_DEFINE;

static int modprobe(char const *name, char const *args, unsigned int lnum, int dload);
static const char *moderror(int err);

/* Add a mount
 * This changes the device string passed to it
*/
static void config_add_mount(unsigned int lnum, char* device, char* mpoint) {
	kl_mount *nptr = allocate(sizeof(kl_mount));
	INIT_MOUNT(nptr);
	
	str_copy(&nptr->device, device, -1);
	str_copy(&nptr->mpoint, "/mnt/target/", -1);
	
	char *mptok = strtok(mpoint, "/");
	while(mptok) {
		if(strlen(mptok) == 0) {
			goto NTOK;
		}
		
		str_append(nptr->mpoint, mptok, -1);
		str_append(nptr->mpoint, "/", -1);
		nptr->depth++;
		
		NTOK:
		mptok = strtok(NULL, "/");
	}
	
	debug(
		"Adding mount '%s' => '%s', depth %d\n",
		nptr->device, nptr->mpoint, nptr->depth
	);
	
	nptr->next = target.mounts;
	target.mounts = nptr;
}

/* Add a target to config.targets */
static void cfg_add_target(void) {
	if(!target.kernel) {
		printD("Target %s has no kernel, not adding", target.name);
		goto END;
	}
	if(!target.mounts) {
		printD("Target %s has no mounts, not adding", target.name);
		goto END;
	}
	
	kl_target *nptr = allocate(sizeof(kl_target));
	TARGET_DEFAULTS(nptr);
	
	nptr->name = target.name;
	nptr->flags = target.flags;
	
	nptr->kernel = target.kernel;
	nptr->initrd = target.initrd;
	nptr->append = target.append;
	nptr->cmdline =target.cmdline;
	nptr->modules = target.modules;
	nptr->mounts = target.mounts;
	
	if(!config.targets) {
		config.targets = nptr;
	}else{
		kl_target *eptr = config.targets;
		while(eptr && eptr->next) {
			eptr = eptr->next;
		}
		
		eptr->next = nptr;
	}
	
	END:
	TARGET_DEFAULTS(&target);
}

static void add_module(unsigned int lnum, char const *module) {
	kl_module *nptr = allocate(sizeof(kl_module));
	INIT_MODULE(nptr);
	
	nptr->module = str_printf("/mnt/target/%s", module);
	nptr->next = target.modules;
	target.modules = nptr;
}

static char *next_value(char *value) {
	char *rval = value+strcspn(value, " \t\r\n");
	
	if(rval[0] != '\0') {
		rval[0] = '\0';
		rval++;
		
		rval = rval+strspn(rval, " \t");
		rval[strcspn(rval, "\r\n")] = '\0';
	}
	
	return rval;
}

/* Read configuration file, one line at a time and call config_parse() for each
 * individual line, in the event of an error an incomplete configuration may,
 * or may not be loaded.
*/
void config_load(void) {
	TARGET_DEFAULTS(&target);
	
	FILE* cfg_handle = fopen("/boot/" CONFIG_FILE, "r");
	if(!cfg_handle) {
		printD("Can't open " CONFIG_FILE ": %s", strerror(errno));
		return;
	}
	
	config.timeout = 0;
	config.grub_root = NULL;
	config.grub_first = hdx;
	
	free_targets(config.targets);
	config.targets = NULL;
	
	unsigned int lnum = 1;
	char line[1024] = {'\0'};
	while(fgets(line, 1024, cfg_handle) != NULL) {
		config_parse(line, lnum++);
	}
	
	config_finish();
	grub_loadcfg();
	
	while(fclose(cfg_handle) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		debug("Can't close " CONFIG_FILE ": %s\n", strerror(errno));
		debug("Discarding cfg_handle!\n");
		return;
	}
}

/* Parse a single line from the configuration file, handles newlines, spaces,
 * tabs and carridge-return characters.
*/
void config_parse(char* line, unsigned int lnum) {
	char *name = line+strspn(line, " \t\r\n");
	char *value = next_value(name), *value2;
	
	/* Skip line if it's a comment, or empty */
	if(name[0] == '#' || name[0] == '\0') {
		return;
	}
	
	debug("config:%u: '%s' = '%s'\n", lnum, name, value);
	
	if(str_ceq(name, "timeout", -1)) {
		config.timeout = strtoul(value, NULL, 10);
		
		return;
	}
	if(str_ceq(name, "title", -1)) {
		if(target.name) {
			cfg_add_target();
		}
		
		if(value[0] == '\0') {
			printD("config:%u: Title requires an argument", lnum);
			return;
		}
		
		str_copy(&target.name, value, -1);
		return;
	}
	if(str_ceq(name, "kernel", -1)) {
		if(value[0] == '\0') {
			printD("config:%u: Kernel requires an argument", lnum);
			return;
		}
		
		target.kernel = str_printf("/mnt/target/%s", value);
		return;
	}
	if(str_ceq(name, "initrd", -1)) {
		if(value[0] == '\0') {
			printD("config:%u: initrd requires an argument", lnum);
			return;
		}
		
		str_printf("/mnt/target/%s", value);
		return;
	}
	if(str_ceq(name, "append", -1)) {
		if(value[0] == '\0') {
			printD("config:%u: Append requires an argument", lnum);
			return;
		}
		
		str_copy(&target.append, value, -1);
		return;
	}
	if(str_ceq(name, "cmdline", -1)) {
		if(value[0] == '\0') {
			printD("config:%u: cmdline requires an argument", lnum);
			return;
		}
		
		str_copy(&target.cmdline, value, -1);
		return;
	}
	if(str_ceq(name, "default", -1)) {
		target.flags |= TARGET_DEFAULT;
		return;
	}
	if(str_ceq(name, "reset-vga", -1)) {
		target.flags |= TARGET_RESET_VGA;
		return;
	}
	if(str_ceq(name, "rootfs", -1)) {
		if(value[0] == '\0') {
			printD("config:%u: RootFS requires an argument", lnum);
			return;
		}
		
		config_add_mount(lnum, value, "/");
		return;
	}
	if(str_ceq(name, "mount", -1)) {
		value2 = next_value(value);
		if(value[0] == '\0' || value2[0] == '\0') {
			printD("config:%u: mount requires 2 arguments", lnum);
			return;
		}
		
		config_add_mount(lnum, value, value2);
		return;
	}
	if(str_ceq(name, "grub_root", -1)) {
		if(value[0] == '\0') {
			printD("config:%u: grub_root requires an argument", lnum);
			return;
		}
		
		str_copy(&config.grub_root, value, -1);
		return;
	}
	if(str_ceq(name, "grub_first", -1)) {
		if(str_eq(value, "hdx", -1)) {
			config.grub_first = hdx;
		}else if(str_eq(value, "sdx", -1)) {
			config.grub_first = sdx;
		}else{
			printD("config:%u: Value must be hdx or sdx", lnum);
		}
		
		return;
	}
	if(str_ceq(name, "module", -1)) {
		add_module(lnum, value);
		return;
	}
	if(str_ceq(name, "modprobe", -1)) {
		if(value[0] == '\0') {
			printD("config:%u: modprobe requires an argument", lnum);
			return;
		}
		
		value2 = next_value(value);
		modprobe(value, value2, lnum, 0);
		return;
	}
	
	printD("config:%u: Unknown directive '%s'", lnum, name);
}

/* Add the remaining target, if it exists */
void config_finish(void) {
	if(target.name) {
		cfg_add_target();
	}
}

/* Load a kernel module */
static int modprobe(char const *name, char const *args, unsigned int lnum, int dload) {
	char *filename = str_printf("/boot/modules/%s.ko", name);
	char *buf = NULL;
	int mod_fh = -1;
	int rbytes = 0, rret;
	int retval = 0;
	
	if(check_module(name)) {
		printd("config:%u: Module '%s' already loaded", lnum, filename);
		goto CLEANUP;
	}
	
	struct stat mstat;
	if(stat(filename, &mstat) == -1) {
		printD("config:%u: Failed to stat module '%s': %s", lnum, filename, strerror(errno));
		goto CLEANUP;
	}
	
	if(dload) {
		printd("Loading dependancy '%s'...", name);
	}else{
		printd("Loading module '%s'...", name);
	}
	
	buf = allocate(mstat.st_size);
	
	if((mod_fh = open(filename, O_RDONLY)) == -1) {
		printD("config:%u: Failed to open module '%s': %s", lnum, filename, strerror(errno));
		goto CLEANUP;
	}
	
	while(rbytes < mstat.st_size) {
		rret = read(mod_fh, buf+rbytes, mstat.st_size-rbytes);
		if(rret == -1) {
			if(errno == EINTR) {
				continue;
			}
			
			printD("config:%u: Failed to read module '%s': %s", lnum, filename, strerror(errno));
			goto CLEANUP;
		}
		if(rret == 0) {
			break;
		}
		
		rbytes += rret;
	}
	
	size_t secsize, n = 0;
	char *sec = elf_getsection(buf, ".modinfo", &secsize);
	
	while(sec && n < secsize) {
		if(!str_eq(sec, "depends=", 8)) {
			n += (strlen(sec)+1);
			sec += (strlen(sec)+1);
			continue;
		}
		
		sec += 8;
		while(sec[0] != '\0') {
			char *dep = str_copy(NULL, sec, strcspn(sec, ","));
			
			if(!check_module(dep)) {
				modprobe(dep, "", lnum, 1);
			}
			
			free(dep);
			sec += (strcspn(sec, ",")+1);
		}
		
		break;
	}
	
	if(syscall(SYS_init_module, buf, rbytes, args) != 0) {
		if(errno == EEXIST) {
			printd("config:%u: Module '%s' already loaded", lnum, filename);
			goto CLEANUP;
		}
		
		printD("config:%u: Failed to load module '%s': %s", lnum, filename, moderror(errno));
		goto CLEANUP;
	}
	
	kl_module *nmod = allocate(sizeof(kl_module));
	INIT_MODULE(nmod);
	
	nmod->module = str_copy(NULL, name, -1);
	nmod->next = k_modules;
	k_modules = nmod;
	
	retval = 1;
	
	CLEANUP:
	if(mod_fh >= 0 && close(mod_fh) == -1) {
		debug("Failed to close '%s': %s\n", filename, strerror(errno));
	}
	
	free(filename);
	free(buf);
	return retval;
}

static const char *moderror(int err) {
	switch (err) {
		case ENOEXEC:
			return "Invalid module format";
		case ENOENT:
			return "Unknown symbol in module";
		case ESRCH:
			return "Module has wrong symbol version";
		case EINVAL:
			return "Invalid parameters";
		default:
			return strerror(err);
	}
}

/* Check if a module has been loaded */
int check_module(char const *name) {
	kl_module *module = k_modules;
	
	while(module) {
		if(str_eq(name, module->module, -1)) {
			return 1;
		}
		
		module = module->next;
	}
	
	return 0;
}

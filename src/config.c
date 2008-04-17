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

#include "config.h"
#include "../config.h"
#include "misc.h"
#include "mount.h"
#include "mystring.h"

struct kl_config config = CONFIG_DEFAULTS_DEFINE;
static struct kl_target target = TARGET_DEFAULTS_DEFINE;

/* Add a mount
 * This changes the device string passed to it
*/
static void config_add_mount(unsigned int lnum, char* device, char* mpoint) {
	char* fstype = "auto";
	
	if(strchr(device, ':')) {
		fstype = device;
		device = strchr(device, ':');
		device[0] = '\0';
		device++;
	}
	
	kl_mount *nptr = malloc(sizeof(kl_mount));
	if(!nptr) {
		goto ABORT;
	}
	MOUNT_DEFAULTS(nptr);
	
	if(!(nptr->device = my_strcpy(device))) {
		goto ABORT;
	}
	if(!(nptr->mpoint = my_strcpy("/"))) {
		goto ABORT;
	}
	if(!(nptr->fstype = my_strcpy(fstype))) {
		goto ABORT;
	}
	
	char *mptok = strtok(mpoint, "/");
	while(mptok) {
		if(strlen(mptok) == 0) {
			goto NTOK;
		}
		
		nptr->mpoint = my_asprintf(nptr->mpoint, "%s/", mptok);
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
	
	ABORT:
	if(nptr) {
		free(nptr->device);
		free(nptr->mpoint);
		free(nptr->fstype);
		free(nptr);
	}
	
	debug("config:%u: Can't allocate memory\n", lnum);
	printm("config:%u: Can't allocate memory", lnum);
}

/* Add a target to config.targets */
static void cfg_add_target(void) {
	if(!target.kernel) {
		debug("Target %s has no kernel, not adding\n", target.name);
		printm("Target %s has no kernel, not adding", target.name);
		
		goto END;
	}
	if(!target.mounts) {
		debug("Target %s has no mounts, not adding\n", target.name);
		printm("Target %s has no mounts, not adding", target.name);
		
		goto END;
	}
	
	kl_target *nptr = malloc(sizeof(kl_target));
	if(!nptr) {
		debug("Can't malloc() %u bytes\n", sizeof(kl_target));
		goto END;
	}
	
	TARGET_DEFAULTS(nptr);
	
	nptr->name = target.name;
	nptr->flags = target.flags;
	nptr->kernel = target.kernel;
	nptr->initrd = target.initrd;
	nptr->append = target.append;
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

/* Read configuration file, one line at a time and call config_parse() for each
 * individual line, in the event of an error an incomplete configuration may,
 * or may not be loaded.
*/
void config_load(void) {
	if(!mount_config()) {
		return;
	}
	
	TARGET_DEFAULTS(&target);
	
	FILE* cfg_handle = NULL;
	while(cfg_handle == NULL) {
		if((cfg_handle = fopen("/mnt/" CONFIG_FILE, "r")) != NULL) {
			break;
		}
		if(errno == EINTR) {
			continue;
		}
		
		debug("Can't open " CONFIG_FILE ": %s\n", strerror(errno));
		return;
	}
	
	unsigned int lnum = 1;
	char line[STACK_BUF] = {'\0'};
	while(fgets(line, STACK_BUF, cfg_handle) != NULL) {
		config_parse(line, lnum++);
	}
	
	config_finish();
	
	while(fclose(cfg_handle) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		debug("Can't close " CONFIG_FILE ": %s\n", strerror(errno));
		debug("Discarding cfg_handle!\n");
		return;
	}
	
	if(umount("/mnt") == -1) {
		debug("Can't unmount /mnt: %s\n", strerror(errno));
	}
}

/* Parse a single line from the configuration file, handles newlines, spaces,
 * tabs and carridge-return characters.
*/
void config_parse(char* line, unsigned int lnum) {
	char *name = line+strspn(line, " \t\r\n");
	char *value = name+strcspn(name, " \t\r\n");
	
	if(value[0] != '\0') {
		value[0] = '\0';
		value++;
		
		value = value+strspn(value, " \t");
		value[strcspn(value, "\r\n")] = '\0';
	}
	
	/* Skip line if it's a comment, or empty */
	if(name[0] == '#' || name[0] == '\0') {
		return;
	}
	
	debug("config:%u: '%s' = '%s'\n", lnum, name, value);
	
	if(str_compare(name, "timeout", STR_NOCASE)) {
		config.timeout = strtoul(value, NULL, 10);
		
		return;
	}
	if(str_compare(name, "title", STR_NOCASE)) {
		if(target.name) {
			cfg_add_target();
		}
		
		if(value[0] == '\0') {
			debug("config:%u: Title requires an argument\n", lnum);
			printm("config:%u: Title requires an argument", lnum);
			
			return;
		}
		
		target.name = my_strcpy(value);
		if(!target.name) {
			debug("config:%u: Can't allocate memory\n", lnum);
			printm("config:%u: Can't allocate memory", lnum);
		}
		
		return;
	}
	if(str_compare(name, "kernel", STR_NOCASE)) {
		if(value[0] == '\0') {
			debug("config:%u: Kernel requires an argument\n", lnum);
			printm("config:%u: Kernel requires an argument", lnum);
			
			return;
		}
		
		target.kernel = my_sprintf("/mnt/%s", value);
		if(!target.kernel) {
			debug("config:%u: Can't allocate memory\n", lnum);
			printm("config:%u: Can't allocate memory", lnum);
		}
		
		return;
	}
	if(str_compare(name, "initrd", STR_NOCASE)) {
		if(value[0] == '\0') {
			debug("config:%u: initrd requires an argument\n", lnum);
			printm("config:%u: initrd requires an argument", lnum);
			
			return;
		}
		
		target.initrd = my_sprintf("/mnt/%s", value);
		if(!target.initrd) {
			debug("config:%u: Can't allocate memory\n", lnum);
			printm("config:%u: Can't allocate memory", lnum);
		}
		
		return;
	}
	if(str_compare(name, "append", STR_NOCASE)) {
		if(value[0] == '\0') {
			debug("config:%u: Append requires an argument\n", lnum);
			printm("config:%u: Append requires an argument", lnum);
			
			return;
		}
		
		target.append = my_strcpy(value);
		if(!target.append) {
			debug("config:%u: Can't allocate memory\n", lnum);
			printm("config:%u: Can't allocate memory", lnum);
		}
		
		return;
	}
	if(str_compare(name, "default", STR_NOCASE)) {
		target.flags |= TARGET_DEFAULT;
		return;
	}
	if(str_compare(name, "rootfs", STR_NOCASE)) {
		if(value[0] == '\0') {
			debug("config:%u: RootFS requires an argument\n", lnum);
			printm("config:%u: RootFS requires an argument", lnum);
			
			return;
		}
		
		config_add_mount(lnum, value, "/");
		return;
	}
	if(str_compare(name, "mount", STR_NOCASE)) {
		char* mpoint = strchr(value, ' ');
		if(mpoint == NULL) {
			debug("config:%u: Invalid mount\n", lnum);
			printm("config:%u: Invalid mount", lnum);
			return;
		}
		mpoint[0] = '\0';
		mpoint += (strspn(mpoint+1, " \t")+1);
		
		if(value[0] == '\0' || mpoint[0] == '\0') {
			debug("config:%u: Invalid mount\n", lnum);
			printm("config:%u: Invalid mount", lnum);
			return;
		}
		
		config_add_mount(lnum, value, mpoint);
		return;
	}
	
	debug("config:%u: Unknown directive '%s'\n", lnum, name);
	printm("config:%u: Unknown directive '%s'", lnum, name);
}

/* Add the remaining target, if it exists */
void config_finish(void) {
	if(target.name) {
		cfg_add_target();
	}
}

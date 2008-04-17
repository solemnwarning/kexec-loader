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
 * This changes the mpoint string passed to it
*/
static void config_add_mount(kl_mount** mounts, char* device, char* mpoint) {
	char* fstype = "auto";
	
	if(strchr(device, ':') != NULL) {
		fstype = device;
		device = strchr(device, ':');
		device[0] = '\0';
		device++;
	}
	
	kl_mount nmount = MOUNT_DEFAULTS_DEFINE;
	strncpy(nmount.device, device, 1023);
	strcpy(nmount.mpoint, "/");
	strncpy(nmount.fstype, fstype, 63);
	
	char *mptok = strtok(mpoint, "/");
	while(mptok) {
		if(strlen(mptok) == 0) {
			goto NTOK;
		}
		
		strncat(nmount.mpoint, mptok, 1023);
		strncat(nmount.mpoint, "/", 1023);
		nmount.depth++;
		
		NTOK:
		mptok = strtok(NULL, "/");
	}
	
	debug(
		"Adding mount '%s' => '%s', depth %d\n",
		nmount.device, nmount.mpoint, nmount.depth
	);
	mount_add(mounts, &nmount);
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
	char line[1024] = {'\0'};
	while(fgets(line, 1024, cfg_handle) != NULL) {
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
		if(value[0] == '\0') {
			value = "Untitled";
		}
		if(target.name != NULL) {
			if(target_add(&(config.targets), &target) == NULL) {
				debug("Can't add target: %s\n", strerror(errno));
			}
			
			TARGET_DEFAULTS(&target);
		}
		
		target.name = my_strcpy(value);
		if(!target.name) {
			debug("config:%u: Can't allocate memory\n", lnum);
			printm("config:%u: Can't allocate memory", lnum);
		}
		return;
	}
	if(str_compare(name, "kernel", STR_NOCASE)) {
		target.kernel = my_sprintf("/mnt/%s", value);
		if(!target.kernel) {
			debug("config:%u: Can't allocate memory\n", lnum);
			printm("config:%u: Can't allocate memory", lnum);
		}
		
		return;
	}
	if(str_compare(name, "initrd", STR_NOCASE)) {
		target.initrd = my_sprintf("/mnt/%s", value);
		if(!target.initrd) {
			debug("config:%u: Can't allocate memory\n", lnum);
			printm("config:%u: Can't allocate memory", lnum);
		}
		
		return;
	}
	if(str_compare(name, "append", STR_NOCASE)) {
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
		config_add_mount(&(target.mounts), value, "/");
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
		
		if(strlen(mpoint) == 0) {
			debug("config:%u: Invalid mount\n", lnum);
			printm("config:%u: Invalid mount", lnum);
			return;
		}
		
		config_add_mount(&(target.mounts), value, mpoint);
		return;
	}
	
	debug("config:%u: Unknown directive '%s'\n", lnum, name);
	printm("config:%u: Unknown directive '%s'", lnum, name);
}

/* Add the remaining target, if it exists */
void config_finish(void) {
	if(target.name != NULL) {
		if(target_add(&(config.targets), &target) == NULL) {
			debug("Can't add target: %s\n", strerror(errno));
		}
		
		TARGET_DEFAULTS(&target);
	}
}

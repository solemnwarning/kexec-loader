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

#include "config.h"
#include "../config.h"
#include "main.h"
#include "misc.h"

struct kl_config config = CONFIG_DEFAULTS_DEFINE;
static struct kl_target target = TARGET_DEFAULTS_DEFINE;

/* Add a mount */
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
	strncpy(nmount.mpoint, mpoint, 1023);
	strncpy(nmount.fstype, fstype, 63);
	
	mount_add(mounts, &nmount);
}

/* Create a device */
static void config_create_device(mode_t type, char* value, unsigned int lnum) {
	
}

/* Read configuration file, one line at a time and call config_parse() for each
 * individual line, in the event of an error an incomplete configuration may,
 * or may not be loaded.
*/
void config_load(void) {
	TARGET_DEFAULTS(&target);
	
	FILE* cfg_handle = NULL;
	while(cfg_handle == NULL) {
		if((cfg_handle = fopen("/boot/" CONFIG_FILE, "r")) != NULL) {
			break;
		}
		if(errno == EINTR) {
			continue;
		}
		
		eprintf("Can't open " CONFIG_FILE ": %s\n", strerror(errno));
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
		
		eprintf("Can't close " CONFIG_FILE ": %s\n", strerror(errno));
		eprintf("Discarding cfg_handle!\n");
		return;
	}
}

/* Parse a single line from the configuration file, handles newlines, spaces,
 * tabs and carridge-return characters.
*/
void config_parse(char* line, unsigned int lnum) {
	/* Remove leading whitespace from line */
	while(IS_WHITESPACE(line[0])) {
		line++;
	}
	
	/* Skip line if it's a comment, or empty */
	if(line[0] == '#' || line[0] == '\0') {
		return;
	}
	
	/* Remove trailing whitespace from line */
	size_t count = strlen(line)-1;
	while(IS_WHITESPACE(line[count])) {
		line[count--] = '\0';
	}
	
	char* name = line;
	char* value = line;
	
	/* If there are any whitespace characters remaining in the string treat
	 * them as name/value seperators, replace the first one with nil to
	 * terminate the string pointed to by name, also offset value to the
	 * first non-whitespace character after the whitespace which should be
	 * the beginning of the value.
	*/
	while(!IS_WHITESPACE(value[0]) && value[0] != '\0') {
		value++;
	}
	if(value[0] != '\0') {
		value[0] = '\0';
		value++;
		
		while(IS_WHITESPACE(value[0])) {
			value++;
		}
	}
	
	if(str_compare(name, "timeout", STR_NOCASE)) {
		config.timeout = strtoul(value, NULL, 10);
		debug("timeout='%s' (%u)", value, config.timeout);
		
		return;
	}
	if(str_compare(name, "title", STR_NOCASE)) {
		if(target.name[0] != '\0') {
			if(target_add(&(config.targets), &target) == NULL) {
				fatal("Can't load config: %s", strerror(errno));
			}
			
			TARGET_DEFAULTS(&target);
		}
		
		strncpy(target.name, value, 63);
		return;
	}
	if(str_compare(name, "kernel", STR_NOCASE)) {
		strncpy(target.kernel, value, 1023);
		return;
	}
	if(str_compare(name, "initrd", STR_NOCASE)) {
		strncpy(target.initrd, value, 1023);
		return;
	}
	if(str_compare(name, "append", STR_NOCASE)) {
		strncpy(target.append, value, 511);
		return;
	}
	if(str_compare(name, "default", STR_NOCASE)) {
		target.flags |= TARGET_DEFAULT;
		return;
	}
	if(str_compare(name, "rootfs", STR_NOCASE)) {
		config_add_mount(&(target.mounts), value, "/target");
		return;
	}
	if(str_compare(name, "mount", STR_NOCASE)) {
		char* tmp = strchr(value, ' ');
		if(tmp == NULL) {
			fatal("Invalid mount at line %u", lnum);
		}
		while(tmp[0] == ' ') {
			tmp[0] = '\0';
			tmp++;
		}
		
		char mpoint[1024] = {'\0'};
		snprintf(mpoint, 1023, "/target/%s", tmp);
		
		config_add_mount(&(target.mounts), value, mpoint);
		return;
	}
	if(str_compare(name, "chardev", STR_NOCASE)) {
		config_create_device(S_IFCHR, value, lnum);
		return;
	}
	if(str_compare(name, "blkdev", STR_NOCASE)) {
		config_create_device(S_IFBLK, value, lnum);
		return;
	}
	
	printf("Unknown configuration variable '%s' at line %u\n", name, lnum);
}

/* Add the remaining target, if it exists */
void config_finish(void) {
	if(target.name[0] != '\0') {
		if(target_add(&(config.targets), &target) == NULL) {
			fatal("Can't load config: %s", strerror(errno));
		}
		
		TARGET_DEFAULTS(&target);
	}
}

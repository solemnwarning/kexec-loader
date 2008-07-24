/* kexec-loader - Configuration header
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

#ifndef KEXEC_LOADER_CONF_H
#define KEXEC_LOADER_CONF_H
#include <time.h>
#include <string.h>
#include "../config.h"

#define TARGET_DEFAULT (1<<0)
#define TARGET_RESET_VGA (1<<1)

typedef struct kl_target kl_target;
typedef struct kl_mount kl_mount;

#define INIT_MOUNT(ptr) \
	(ptr)->device = NULL;\
	(ptr)->mpoint = NULL;\
	(ptr)->depth = 0;\
	(ptr)->next = NULL;

#define DINIT_MOUNT {NULL,NULL,0,NULL}

struct kl_mount {
	char *device;
	char *mpoint;
	unsigned int depth;
	
	struct kl_mount* next;
};

#define TARGET_DEFAULTS(ptr) \
	memset((ptr)->name, '\0', NAME_SIZE);\
	(ptr)->flags = 0;\
	memset((ptr)->kernel, '\0', KERNEL_SIZE);\
	memset((ptr)->initrd, '\0', INITRD_SIZE);\
	memset((ptr)->append, '\0', APPEND_SIZE);\
	memset((ptr)->cmdline, '\0', APPEND_SIZE);\
	(ptr)->n_modules = 0;\
	(ptr)->mounts = NULL;\
	(ptr)->next = NULL;

#define TARGET_DEFAULTS_DEFINE {{'\0'},0,{'\0'},{'\0'},{'\0'},{'\0'},{NULL},0,NULL,NULL}

struct kl_target {
	char name[NAME_SIZE];
	int flags;
	
	char kernel[KERNEL_SIZE];
	char initrd[INITRD_SIZE];
	char append[APPEND_SIZE];
	char cmdline[APPEND_SIZE];
	
	char *modules[MAX_MODULES];
	int n_modules;
	
	struct kl_mount* mounts;
	struct kl_target* next;
};

#define CONFIG_DEFAULTS_DEFINE {0,{'\0'},hdx,NULL}

struct kl_config {
	unsigned int timeout;
	
	char grub_root[DEVICE_SIZE];
	enum {hdx,sdx} grub_first;
	
	struct kl_target* targets;
};

extern struct kl_config config;

void config_load(void);
void config_parse(char* line, unsigned int lnum);
void config_finish(void);

#endif /* !KEXEC_LOADER_CONF_H */

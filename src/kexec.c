/* kexec-loader - kexec functions
 * Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
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
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <linux/reboot.h>

#include "kexec.h"
#include "misc.h"
#include "../config.h"
#include "console.h"
#include "config.h"
#include "mystring.h"

#define argv_append(str) \
	kexec_argv[argc++] = str_copy(NULL, str, -1);\
	kexec_argv[argc] = NULL;

#define argv_appendf(...) \
	kexec_argv[argc++] = str_printf(__VA_ARGS__);\
	kexec_argv[argc] = NULL;

int kexec_main(int argc, char **argv);

static int argc;

/* Run the kexec program
 *
 * Returns the exit status, or -1 on failure
*/
static int run_kexec(char** kexec_argv) {
	pid_t newpid = fork();
	if(newpid == -1) {
		printD(RED, 2, "Can't fork: %s", strerror(errno));
		return -1;
	}
	if(newpid == 0) {
		exit(kexec_main(argc, kexec_argv));
	}
	
	int status = 0;
	wait(&status);
	
	if(!WIFEXITED(status)) {
		return(-1);
	}
	
	return(WEXITSTATUS(status));
}

/* Load a kernel using the kexec program
 *
 * Returns 1 on success, zero on error
*/
int load_kernel(kl_target *target) {
	int n = 0, retval = 1;
	
	kl_module *module = target->modules;
	while(module) {
		n++;
		module = module->next;
	}
	
	char* kexec_argv[8+n];
	argc = 0;
	
	argv_append("kexec");
	argv_append("-l");
	argv_append(target->kernel);
	
	if(target->flags & TARGET_RESET_VGA) {
		argv_append("--reset-vga");
	}
	
	printd(GREEN, 1, "Loading kernel...");
	printd(GREEN, 2, "kernel: %s", target->kernel);
	
	if(target->append) {
		argv_appendf("--append=%s", target->append);
		printd(GREEN, 2, "append: %s", target->append);
	}
	if(target->cmdline) {
		argv_appendf("--command-line=%s", target->cmdline);
		printd(GREEN, 2, "cmdline: %s", target->cmdline);
	}
	if(target->initrd) {
		argv_appendf("--initrd=%s", target->initrd);
		printd(GREEN, 2, "initrd: %s", target->initrd);
	}
	
	module = target->modules;
	while(module) {
		argv_appendf("--module=%s", module->module);
		printd(GREEN, 2, "module: %s", module->module);
		
		module = module->next;
	}
	
	if(run_kexec(kexec_argv) != 0) {
		retval = 1;
	}
	
	for(n = 0; n < argc; n++) {
		free(kexec_argv[n]);
	}
	
	return retval;
}

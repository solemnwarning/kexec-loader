/* kexec-loader - kexec functions
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

#define argv_append(str) kexec_argv[argn++] = str;

/* Run the kexec program
 *
 * Returns the exit status, or -1 on failure
*/
static int run_kexec(char** kexec_argv) {
	pid_t newpid = fork();
	if(newpid == -1) {
		printD("Can't fork: %s", strerror(errno));
		return(-1);
	}
	if(newpid == 0) {
		kexec_argv[0] = KEXEC_PATH;
		execv(KEXEC_PATH, kexec_argv);
		
		printD("Can't run kexec: %s", strerror(errno));
		exit(-1);
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
int load_kernel(char const* kernel, char const* append, char const* initrd) {
	char append_arg[STACK_BUF];
	char initrd_arg[STACK_BUF];
	
	char* kexec_argv[6] = {NULL};
	int argn = 1;
	
	argv_append("-l");
	argv_append((char*)kernel);
	
	printd("Loading kernel...");
	printd("\tkernel: %s", kernel);
	
	if(append[0] != '\0') {
		snprintf(append_arg, STACK_BUF, "--append=%s", append);
		argv_append(append_arg);
		
		printd("\tappend: %s", append);
	}
	if(initrd[0] != '\0') {
		snprintf(initrd_arg, STACK_BUF, "--initrd=%s", initrd);
		argv_append(initrd_arg);
		
		printd("\tinitrd: %s", initrd);
	}
	
	if(run_kexec(kexec_argv) != 0) {
		return 0;
	}
	
	return 1;
}

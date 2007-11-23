/* kexec-loader - Console functions
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

#include "ctconfig.h"
#include "main.h"

static FILE* tty_files[TTY_MAX+1] = {NULL};

/* Open a /dev/ttyN device */
static void tty_open(int ttyn) {
	if(ttyn > TTY_MAX) {
		fatal("ttyn = %d", ttyn);
	}
	
	char path[32] = {'\0'};
	sprintf(path, "/dev/tty%d", ttyn);
	
	while(tty_files[ttyn] != NULL) {
		tty_files[ttyn] = fopen(path, "w");
		
		if(tty_files[ttyn] == NULL && errno != EINTR) {
			fatal("Can't open tty%d: %s", ttyn, strerror(errno));
		}
	}
}

/* Close a /dev/ttyN device */
static void tty_close(int ttyn) {
	if(ttyn > TTY_MAX) {
		fatal("ttyn = %d", ttyn);
	}
	if(tty_files[ttyn] == NULL) {
		return;
	}
	
	while(fclose(tty_files[ttyn]) != 0) {
		if(errno == EINTR) {
			continue;
		}
		fatal("Can't close tty%d: %s", ttyn, strerror(errno));
	}
	tty_files[ttyn] = NULL;
}

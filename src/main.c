/* kexec-loader - Main source
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
#include <unistd.h>

#include "mount.h"
#include "../config.h"
#include "console.h"
#include "config.h"

int main(int argc, char** argv) {
	console_init();
	console_clear();
	
	unsigned int rows, cols, n;
	console_getsize(&rows, &cols);
	
	console_attrib(CONS_INVERT);
	console_setpos(1,1);
	for(n = 0; n < cols; n++) { printf(" "); }
	
	console_setpos(1,2);
	printf("kexec-loader " VERSION);
	
	console_setpos(1, cols - (strlen(COPYRIGHT " ")-1));
	printf(COPYRIGHT " ");
	
	console_attrib(CONS_RESET);
	console_setpos(3,1);
	
	mount_boot();
	mount_virt();
	config_load();
	
	unmount_tree("/");
	
	while(1) {
		sleep(9999);
	}
	return(1);
}

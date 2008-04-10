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
#include <sys/select.h>
#include <sys/mount.h>

#include "mount.h"
#include "../config.h"
#include "console.h"
#include "config.h"
#include "kexec.h"

static kl_target* target_menu(void);

int main(int argc, char** argv) {
	if(mount("proc", "/proc", "proc", 0, NULL) == -1) {
		fatal("Can't mount /proc filesystem: %s", strerror(errno));
	}
	
	console_init();
	mount_boot();
	config_load();
	
	while(1) {
		unmount_tree("/target");
		
		kl_target* target = target_menu();
		
		console_clear();
		console_setpos(1,1);
		
		if(!mount_list(target->mounts)) {
			continue;
		}
		
		char* append = NULL;
		char* initrd = NULL;
		
		if(target->append[0] != '\0') {
			append = target->append;
		}
		if(target->initrd[0] != '\0') {
			initrd = target->initrd;
		}
		
		if(!kexec_load(target->kernel, append, initrd)) {
			continue;
		}
		
		break;
	}
	
	unmount_tree("/");
	kexec_boot();
	
	while(1) {
		sleep(9999);
	}
	return(1);
}

/* Display the target list and return the selected target */
static kl_target* target_menu(void) {
	static int first_call = 1;
	
	unsigned int rows, cols;
	console_getsize(&rows, &cols);
	
	unsigned int mpos = 1, mmpos = (rows-3), ddefault = 0, wpos, n;
	int gotchar, i;
	
	kl_target* ctarget = config.targets;
	kl_target* starget = config.targets;
	
	unsigned int tremain = config.timeout;
	struct timeval timeout = {0,0};
	struct timeval* timeptr = NULL;
	if(first_call && config.timeout > 0) {
		timeptr = &timeout;
	}
	
	char timestr[16] = {'\0'};
	
	fd_set read_fds;
	
	while(ctarget != NULL) {
		if(ctarget->flags & TARGET_DEFAULT) {
			ddefault = 1;
			break;
		}
		ctarget = ctarget->next;
		
		if(mpos < mmpos) {
			mpos++;
		}else{
			starget = starget->next;
		}
	}
	if(!ddefault) {
		ctarget = config.targets;
		starget = config.targets;
		mpos = 1;
		
		ddefault = 1;
	}
	
	while(1) {
		console_clear();
		
		console_attrib(CONS_INVERT);
		console_setpos(1,1);
		for(n = 0; n < cols; n++) { putchar(' '); }
		
		console_setpos(1,2);
		printf("kexec-loader " VERSION);
		
		console_setpos(1, cols - strlen(COPYRIGHT));
		printf(COPYRIGHT);
		
		console_attrib(CONS_RESET);
		
		if(starget == NULL) {
			console_setpos(3, 2);
			printf("No targets defined!");
			
			while(1) { sleep(999); }
		}
		
		ctarget = starget;
		
		for(wpos = 1; wpos <= mmpos; wpos++) {
			if(ctarget == NULL) {
				break;
			}
			
			console_setpos(wpos+2, 2);
			
			if(wpos == mpos) {
				console_attrib(CONS_INVERT);
			}
			
			for(n = 0; n < (cols-2); n++) {
				if(ctarget->name[n] == '\0') {
					break;
				}
				
				putchar(ctarget->name[n]);
			}
			
			if(wpos == mpos) {
				if(timeptr != NULL) {
					snprintf(timestr, 15, " Timeout: %u", tremain);
					console_setpos(rows-1, cols - strlen(timestr));
					
					printf("%s", timestr);
				}
				
				console_attrib(CONS_RESET);
			}
			ctarget = ctarget->next;
		}
		
		MENU_INPUT:
		FD_ZERO(&read_fds);
		FD_SET(fileno(stdin), &read_fds);
		timeout.tv_sec = 1;
		
		i = select(fileno(stdin)+1, &read_fds, NULL, NULL, timeptr);
		if(i <= 0) {
			if(--tremain == 0) {
				ctarget = starget;
				for(n = 1; n < mpos; n++) { ctarget = ctarget->next; }
				
				goto ENDMENU;
			}else{
				continue;
			}
		}
		timeptr = NULL;
		
		gotchar = getchar();
		if(gotchar == '\n') {
			ctarget = starget;
			for(n = 1; n < mpos; n++) { ctarget = ctarget->next; }
			
			goto ENDMENU;
		}
		if(gotchar == 0x1B && getchar() == '[') {
			gotchar = getchar();
			
			if(gotchar == 65 || gotchar == 66) {
				ctarget = starget;
				for(n = 1; n < mpos; n++) {
					ctarget = ctarget->next;
				}
			}
			if(gotchar == 65) {
				if(ctarget == config.targets) {
					goto MENU_INPUT;
				}
				
				if(mpos > 1) {
					mpos--;
				}else{
					starget = config.targets;
					while(starget != NULL && starget->next != ctarget) {
						starget = starget->next;
					}
				}
				
				continue;
			}
			if(gotchar == 66) {
				if(ctarget->next == NULL) {
					goto MENU_INPUT;
				}
				
				if(mpos < mmpos) {
					mpos++;
				}else{
					starget = starget->next;
				}
			}
		}
	}
	
	ENDMENU:
	first_call = 0;
	return(ctarget);
}

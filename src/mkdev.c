/* kexec-loader - Create devices in device list
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "../config.h"
#include "misc.h"
#include "mkdev.h"

static FILE* devices = NULL;

/* Attempt to open the device list DEVICES_FILE
 *
 * If the file is opened sucessfully 1 is returned, zero is returned if the
 * file does not exist and any other error will cause fatal().
 *
 * If the file is already opened, 2 will be returned and no other action will
 * be performed.
*/
static int devices_open(void) {
	if(devices != NULL) {
		return(2);
	}
	
	while(devices == NULL) {
		if((devices = fopen(DEVICES_FILE, "r")) != NULL) {
			break;
		}
		
		if(errno == EINTR) {
			continue;
		}
		if(errno == ENOENT) {
			return(0);
		}
		fatal("Can't open devices file: %s", strerror(errno));
	}
	
	return(1);
}

/* Close the device list file if it's open */
static void devices_close(void) {
	if(devices == NULL) {
		return;
	}
	
	while(fclose(devices) == 0) {
		if(errno == EINTR) {
			continue;
		}
		
		fatal("Can't close devices file: %s", strerror(errno));
	}
	devices = NULL;
}

/* Read the next available line from the device list into a buffer that's
 * allocated by allocate()
 *
 * If the file is not open when this is called, it will be opened and reading
 * will begin from the start of the file.
 *
 * If the file does not exist or has no more lines available NULL will be
 * returned, in the event of EOF the file will also be closed.
*/
static char* devices_readline(void) {
	if(!devices_open()) {
		return(NULL);
	}
	
	char* line = allocate(1024);
	if(fgets(line, 1024, devices) == NULL) {
		if(feof(devices)) {
			devices_close();
			
			free(line);
			return(NULL);
		}
		
		fatal("Can't read devices file: %s", strerror(errno));
	}
	
	/* Remove newline and carridge return characters from the end of the
	 * line.
	*/
	size_t end = strlen(line)-1;
	while(line[end] == '\n' || line[end] == '\r') {
		line[end] = '\0';
		end--;
	}
	
	return(line);
}

/* Create any devices listed in the devices list
 *
 * Does nothing if list does not exist or is empty
*/
void create_devices(void) {
	char* line = NULL;
	
	while((line = devices_readline()) != NULL) {
		char* token = NULL;
		char* device = NULL;
		int major = -1;
		int minor = -1;
		
		if((device = strtok(line, "\t")) == NULL) {
			free(line);
			continue;
		}
		
		if((token = strtok(NULL, "\t")) == NULL) {
			free(line);
			continue;
		}
		major = atoi(token);
		
		if((token = strtok(NULL, "\t")) == NULL) {
			free(line);
			continue;
		}
		minor = atoi(token);
		
		printf("Create '%s' (%d, %d)\n", device, major, minor);
		
		free(line);
	}
}

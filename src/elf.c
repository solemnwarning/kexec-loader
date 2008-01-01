/* kexec-loader - ELF format functions
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

#include "elf.h"
#include "misc.h"

/* Checks if a file has the ELF magic number
 *
 * Returns 1 if the ELF magic number was found in the file, zero if the file
 * did not contain the ELF magic number or an error occurred.
*/
int elf_has_magic(char const* filename) {
	FILE* filehandle = NULL;
	while((filehandle = fopen(filename, "rb")) == NULL) {
		if(errno == EINTR) {
			continue;
		}
		
		printm("Can't open file: %s", strerror(errno));
		return(0);
	}
	
	unsigned char buf[4] = {0};
	fread(buf, 1, 4, filehandle);
	if(ferror(filehandle)) {
		printm("Can't read file: %s", strerror(errno));
	}
	
	while(fclose(filehandle) != 0) {
		if(errno == EINTR) {
			continue;
		}
		
		printm("Can't close file: %s", strerror(errno));
		printm("Dropping filehandle!");
		return(0);
	}
	
	if(buf[0] == 0x7F && str_compare((char*)(buf+1), "ELF", 0)) {
		return(1);
	}
	
	return(0);
}

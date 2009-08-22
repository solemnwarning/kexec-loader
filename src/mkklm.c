/* kexec-loader - Create .klm archive
 * Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

struct klm_record {
	char name[56];
	uint32_t offset;
	uint32_t size;
} __attribute__((__packed__));

static void write_data(FILE *fh, void *data, int size) {
	int c, i;
	
	for(c = 0; c < size; c += i) {
		i = fwrite(data, 1, size-c, fh);
		if(!i) {
			eprintf("Failed to write to output file: %s\n", strerror(errno));
			exit(1);
		}
	}
}

int main(int argc, char **argv) {
	if(argc < 3) {
		eprintf("Usage: %s <output file> <input files>\n", argv[0]);
		return 1;
	}
	
	FILE *outfile = fopen(argv[1], "wb");
	if(!outfile) {
		eprintf("%s: open failed (%s)\n", argv[1], strerror(errno));
		return 1;
	}
	
	struct klm_record rec;
	int offset = sizeof(rec)*(argc-1), i, n;
	
	for(i = 2; i < argc; i++) {
		char name[256];
		strcpy(name, strchr(argv[i], '/') ? strrchr(argv[i], '/')+1 : argv[i]);
		name[strcspn(name, ".")] = '\0';
		
		memset(rec.name, '\0', 56);
		strncpy(rec.name, name, 55);
		
		struct stat finfo;
		if(stat(argv[i], &finfo) == -1) {
			eprintf("%s: stat failed (%s)\n", argv[i], strerror(errno));
			exit(1);
		}
		
		rec.size = htonl(finfo.st_size);
		rec.offset = htonl(offset);
		offset += finfo.st_size;
		
		write_data(outfile, &rec, sizeof(rec));
	}
	
	memset(rec.name, '\0', 56);
	rec.size = 0;
	rec.offset = 0;
	
	write_data(outfile, &rec, sizeof(rec));
	
	char buf[10240];
	
	for(i = 2; i < argc; i++) {
		char name[256];
		strcpy(name, strchr(argv[i], '/') ? strrchr(argv[i], '/')+1 : argv[i]);
		name[strcspn(name, ".")] = '\0';
		
		memset(rec.name, '\0', 56);
		strncpy(rec.name, name, 55);
		
		FILE *infile = fopen(argv[i], "rb");
		if(!infile) {
			eprintf("%s: open failed (%s)\n", argv[i], strerror(errno));
			return 1;
		}
		
		while(!feof(infile)) {
			n = fread(buf, 1, 10240, infile);
			
			if(ferror(infile)) {
				eprintf("%s: read failed (%s)\n", argv[i], strerror(errno));
				return 1;
			}
			
			write_data(outfile, buf, n);
		}
		
		fclose(infile);
	}
	
	fclose(outfile);
	return 0;
}

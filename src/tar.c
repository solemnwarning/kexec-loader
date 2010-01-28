/* kexec-loader - TAR archive extractor
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lzmadec.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <zlib.h>

#include "globcmp.h"
#include "misc.h"
#include "console.h"

enum comp_type {
	comp_raw,
	comp_lzma,
	comp_gzip
};

struct comp_file {
	void *handle;
	enum comp_type format;
	
	char const *error;
	char errbuf[64];
	
	int (*open)(struct comp_file *file, char const *path);
	size_t (*read)(struct comp_file *file, void* buf, size_t size);
	int (*seek)(struct comp_file *file, size_t offset);
	void (*close)(struct comp_file *file);
	int (*eof)(struct comp_file*);
};

struct tar_header {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char checksum[8];
	char linkflag[1];
	char linkname[100];
	char pad[255];
} __attribute__((__packed__));

#define NULL_HANDLE_CHECK() \
	if(file->handle == NULL) { \
		die("NULL handle passed in file->handle"); \
	}

#define SET_ERROR(s) \
	strncpy(file->errbuf, s, sizeof(file->errbuf)); \
	file->error = file->errbuf;

#define RESET_ERROR() \
	file->error = NULL;

static int test_checksum(struct tar_header *header);
static int tar_mkdirs(char const *rpath);

static int raw_file_open(struct comp_file *file, char const *path);
static size_t raw_file_read(struct comp_file *file, void* buf, size_t size);
static int raw_file_seek(struct comp_file *file, size_t offset);
static void raw_file_close(struct comp_file *file);
static int raw_file_eof(struct comp_file *file);

static int lzma_file_open(struct comp_file *file, char const *path);
static size_t lzma_file_read(struct comp_file *file, void* buf, size_t size);
static int lzma_file_seek(struct comp_file *file, size_t offset);
static void lzma_file_close(struct comp_file *file);
static int lzma_file_eof(struct comp_file *file);

static int gzip_file_open(struct comp_file *file, char const *path);
static size_t gzip_file_read(struct comp_file *file, void* buf, size_t size);
static int gzip_file_seek(struct comp_file *file, size_t offset);
static void gzip_file_close(struct comp_file *file);
static int gzip_file_eof(struct comp_file *file);

#define FAIL(...) \
	printD(__VA_ARGS__); \
	file.close(&file); \
	return 1;

int extract_tar(char const *name, char const *dest) {
	struct comp_file file;
	
	if(kl_streq_end(name, ".tar")) {
		file.format = comp_raw;
		file.open = &raw_file_open;
		file.read = &raw_file_read;
		file.seek = &raw_file_seek;
		file.close = &raw_file_close;
		file.eof = &raw_file_eof;
	}else if(kl_streq_end(name, ".tlz") || kl_streq_end(name, ".tar.lzma")) {
		file.format = comp_lzma;
		file.open = &lzma_file_open;
		file.read = &lzma_file_read;
		file.seek = &lzma_file_seek;
		file.close = &lzma_file_close;
		file.eof = &lzma_file_eof;
	}else if(kl_streq_end(name, ".tgz") || kl_streq_end(name, ".tar.gz")) {
		file.format = comp_gzip;
		file.open = &gzip_file_open;
		file.read = &gzip_file_read;
		file.seek = &gzip_file_seek;
		file.close = &gzip_file_close;
		file.eof = &gzip_file_eof;
	}else{
		printD("Unknown TAR extension: %s", name);
		return 0;
	}
	
	if(!file.open(&file, name)) {
		printD("Error opening %s: %s", name, file.error);
		return 0;
	}
	
	struct tar_header header, zheader;
	memset(&zheader, 0, sizeof(zheader));
	memset(&header, 1, sizeof(header));
	
	char path[256], buf[512];
	
	while(1) {
		size_t r = file.read(&file, &header, sizeof(header));
		
		if(file.error) {
			FAIL("Error reading %s: %s", name, file.error);
		}
		if(r < sizeof(header)) {
			FAIL("Error extracting %s: incomplete file", name);
		}
		
		if(memcmp(&header, &zheader, sizeof(header)) == 0) {
			debug("Zero record encountered");
			break;
		}
		
		if(!test_checksum(&header)) {
			FAIL("Error extracting %s: corrupt header", name);
		}
		
		if(header.name[strlen(header.name)-1] == '/') {
			debug("Skipping TAR header '%s', is a directory", header.name);
			continue;
		}
		if(header.linkflag[0] == '1') {
			debug("Skipping TAR header '%s', is a hardlink", header.name);
			continue;
		}
		
		strncpy(path, dest, sizeof(path));
		strncat(path, "/", sizeof(path));
		strncat(path, header.name, sizeof(path));
		
		if(strchr(header.name, '/') && !tar_mkdirs(path)) {
			return 0;
		}
		
		size_t file_size = strtoul(header.size, NULL, 8);
		size_t file_count = 0;
		
		FILE *outfh = NULL;
		
		if(!check_file(path)) {
			outfh = fopen("/tarfile.tmp", "wb");
			if(!outfh) {
				FAIL("Error opening tarfile.tmp: %s", strerror(errno));
			}
		}else{
			debug("Not extracting file '%s', already exists", header.name);
		}
		
		while(file_count < file_size) {
			r = file.read(&file, buf, 512);
			
			if(file.error) {
				FAIL("Error reading %s: %s", name, file.error);
			}
			if(r < 512) {
				FAIL("Error extracting %s: incomplete file", name);
			}
			
			int wsize = SMALLEST(file_size-file_count, 512), wc = 0;
			file_count += wsize;
			
			while(outfh && wc < wsize) {
				wc += fwrite(buf+wc, 1, wsize-wc, outfh);
				
				if(ferror(outfh)) {
					FAIL("Error writing tarfile.tmp: %s", strerror(errno));
				}
			}
		}
		
		if(outfh) {
			fclose(outfh);
			
			if(rename("/tarfile.tmp", path) == -1) {
				FAIL("Error moving file to %s: %s", path, strerror(errno));
			}
		}
	}
	
	file.close(&file);
	
	return 1;
}

static int test_checksum(struct tar_header *header) {
	struct tar_header test = *header;
	memset(test.checksum, ' ', sizeof(test.checksum));
	
	unsigned int a, b = 0, i;
	
	a = strtoul(header->checksum, NULL, 8);
	
	for(i = 0; i < sizeof(test); i++) {
		b += ((unsigned char*)&test)[i];
	}
	
	return a == b ? 1 : 0;
}

static int tar_mkdirs(char const *rpath) {
	char path[128];
	strncpy(path, rpath, 128);
	
	char *last = strrchr(path, '/');
	
	if(last && last > path) {
		last[0] = '\0';
		
		if(strchr(path, '/') && !tar_mkdirs(path)) {
			return 0;
		}
		
		if(mkdir(path, 0755) == -1 && errno != EEXIST) {
			printD("Error creating %s: %s", path, strerror(errno));
			return 0;
		}
	}
	
	return 1;
}

static int raw_file_open(struct comp_file *file, char const *path) {
	RESET_ERROR();
	
	file->handle = fopen(path, "rb");
	if(!file->handle) {
		SET_ERROR(strerror(errno));
		return 0;
	}
	
	return 1;
}

static size_t raw_file_read(struct comp_file *file, void* buf, size_t size) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	size_t count = 0;
	
	while(count < size) {
		size_t ret = fread(buf+count, 1, size-count, file->handle);
		
		if(feof(file->handle)) {
			break;
		}
		if(!ret) {
			SET_ERROR(strerror(errno));
			break;
		}
		
		count += ret;
	}
	
	return count;
}

static int raw_file_seek(struct comp_file *file, size_t offset) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	if(fseek(file->handle, offset, SEEK_SET)) {
		SET_ERROR(strerror(errno));
		return 0;
	}
	
	return 1;
}

static void raw_file_close(struct comp_file *file) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	fclose(file->handle);
	file->handle = NULL;
}

static int raw_file_eof(struct comp_file *file) {
	NULL_HANDLE_CHECK();
	return feof(file->handle) ? 1 : 0;
}

static int lzma_file_open(struct comp_file *file, char const *path) {
	RESET_ERROR();
	
	file->handle = lzmadec_open(path);
	if(!file->handle) {
		SET_ERROR(strerror(errno));
		return 0;
	}
	
	return 1;
}

static size_t lzma_file_read(struct comp_file *file, void *buf, size_t size) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	size_t count = 0;
	
	while(count < size) {
		ssize_t ret = lzmadec_read(file->handle, buf+count, size-count);
		
		if(lzmadec_eof(file->handle)) {
			break;
		}
		if(ret == -1) {
			SET_ERROR(strerror(errno));
			break;
		}
		
		count += ret;
	}
	
	return count;
}

static int lzma_file_seek(struct comp_file *file, size_t offset) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	if(lzmadec_seek(file->handle, offset, SEEK_SET) == -1) {
		SET_ERROR(strerror(errno));
		return 0;
	}
	
	return 1;
}

static void lzma_file_close(struct comp_file *file) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	lzmadec_close(file->handle);
	file->handle = NULL;
}

static int lzma_file_eof(struct comp_file *file) {
	NULL_HANDLE_CHECK();
	return lzmadec_eof(file->handle) ? 1 : 0;
}

static int gzip_file_open(struct comp_file *file, char const *path) {
	RESET_ERROR();
	
	file->handle = gzopen(path, "rb");
	if(!file->handle) {
		SET_ERROR(gzerror(file->handle, &errno));
		return 0;
	}
	
	return 1;
}

static size_t gzip_file_read(struct comp_file *file, void *buf, size_t size) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	size_t count = 0;
	
	while(count < size) {
		ssize_t ret = gzread(file->handle, buf+count, size-count);
		
		if(gzeof(file->handle)) {
			break;
		}
		if(ret == -1) {
			SET_ERROR(gzerror(file->handle, &errno));
			break;
		}
		
		count += ret;
	}
	
	return count;
}

static int gzip_file_seek(struct comp_file *file, size_t offset) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	if(gzseek(file->handle, offset, SEEK_SET) == -1) {
		SET_ERROR(gzerror(file->handle, &errno));
		return 0;
	}
	
	return 1;
}

static void gzip_file_close(struct comp_file *file) {
	NULL_HANDLE_CHECK();
	RESET_ERROR();
	
	gzclose(file->handle);
	file->handle = NULL;
}

static int gzip_file_eof(struct comp_file *file) {
	NULL_HANDLE_CHECK();
	return gzeof(file->handle);
}

int is_tar_extension(char const *name) {
	char const *exts[] = {".tar", ".tlz", ".tar.lzma", ".tgz", ".tar.gz", NULL};
	int i;
	
	for(i = 0; exts[i]; i++) {
		if(kl_streq_end(name, exts[i])) {
			return 1;
		}
	}
	
	return 0;
}

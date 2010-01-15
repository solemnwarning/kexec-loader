/* kexec-loader - VFS wrapper functions
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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

#include "disk.h"
#include "misc.h"
#include "vfs.h"

static char *vfs_root = NULL;

char *vfs_translate_path(char const *path_in) {
	char const *disk = vfs_root;
	char disk_buf[32];
	kl_disk *disk_s = NULL;
	
	if(path_in[0] == '(') {
		if(!strchr(path_in, ')')) {
			errno = EINFILE;
			return NULL;
		}
		
		strncpy(disk_buf, path_in+1, sizeof(disk_buf));
		disk_buf[strcspn(disk_buf, ")")] = '\0';
		disk = disk_buf;
		
		path_in = strchr(path_in, ')')+1;
	}
	
	if(!disk || *disk == '\0') {
		errno = ENDISK;
		return NULL;
	}
	
	disk_s = find_disk(disk);
	if(!disk_s) {
		errno = ENODEV;
		return NULL;
	}
	
	char const *m_error = mount_disk(disk_s, NULL);
	if(m_error) {
		if(kl_streq(m_error, "Unknown filesystem format")) {
			errno = EBADFS;
		}
		
		free(disk_s);
		return NULL;
	}
	
	char *path = kl_malloc(strlen(disk_s->name) + strlen(path_in) + 7);
	sprintf(path, "/mnt/%s", disk_s->name);
	int path_nodes = 0;
	
	while(*path_in) {
		int len = strcspn(path_in, "/");
		char *node = kl_strndup(path_in, len);
		path_in += len;
		
		if(len) {
			if(!kl_streq(node, "..")) {
				strcat(path, "/");
				strcat(path, node);
				
				path_nodes++;
			}else if(path_nodes) {
				strrchr(path, '/')[0] = '\0';
				path_nodes--;
			}
		}
		
		free(node);
	}
	
	free(disk_s);
	return path;
}

void vfs_set_root(char const *root) {
	free(vfs_root);
	vfs_root = root ? kl_strdup(root) : NULL;
}

int vfs_open(char const *filename, int flags, ...) {
	char *path = vfs_translate_path(filename);
	int ret;
	
	if(!path) {
		return -1;
	}
	
	if(flags & O_CREAT) {
		va_list argv;
		va_start(argv, flags);
		
		ret = open(path, flags, va_arg(argv, mode_t));
		
		va_end(argv);
	}else{
		ret = open(path, flags);
	}
	
	free(path);
	return ret;
}

FILE *vfs_fopen(char const *filename, char const *mode) {
	char *path = vfs_translate_path(filename);
	if(!path) {
		return NULL;
	}
	
	FILE *fh = fopen(path, mode);
	
	free(path);
	return fh;
}

DIR *vfs_opendir(char const *filename) {
	char *path = vfs_translate_path(filename);
	if(!path) {
		return NULL;
	}
	
	DIR *dh = opendir(path);
	
	free(path);
	return dh;
}

int vfs_stat(char const *path, struct stat *buf) {
	char *rpath = vfs_translate_path(path);
	if(!path) {
		return -1;
	}
	
	int ret = stat(rpath, buf);
	
	free(rpath);
	return ret;
}

int vfs_lstat(char const *path, struct stat *buf) {
	char *rpath = vfs_translate_path(path);
	if(!path) {
		return -1;
	}
	
	int ret = lstat(rpath, buf);
	
	free(rpath);
	return ret;
}
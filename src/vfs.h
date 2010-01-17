/* kexec-loader - VFS wrapper header
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

#ifndef KL_VFS_H
#define KL_VFS_H
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>

char *vfs_translate_path(char const *path_in);
void vfs_set_root(char const *root);
int vfs_open(char const *filename, int flags, ...);
FILE *vfs_fopen(char const *filename, char const *mode);
DIR *vfs_opendir(char const *filename);
int vfs_stat(char const *path, struct stat *buf);
int vfs_lstat(char const *path, struct stat *buf);
int vfs_access(char const *path, int mode);
int vfs_exists(char const *path);

#endif /* !KL_VFS_H */

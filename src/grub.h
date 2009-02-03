/* kexec-loader - GRUB compatibility header
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

#ifndef KL_GRUB_H
#define KL_GRUB_H

#define INIT_GDEV(ptr) \
	(ptr)->next = NULL; \
	(ptr)->type[0] = '\0'; \
	(ptr)->p1[0] = '\0'; \
	(ptr)->p2[0] = '\0'; \
	(ptr)->p3[0] = '\0'; \
	(ptr)->device[0] = '\0';

typedef struct kl_gdev {
	struct kl_gdev *next;
	
	char type[16];
	char p1[16];
	char p2[16];
	char p3[16];
	
	char device[16];
} kl_gdev;

extern kl_gdev *grub_devmap;

int parse_gdev(kl_gdev *dest, char const *src);
char *lookup_gdev(char const *dev);
void grub_load(void);

#endif /* !KL_GRUB_H */

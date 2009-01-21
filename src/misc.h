/* kexec-loader - Misc. stuff
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

#ifndef KL_MISC_H
#define KL_MISC_H

void debug(char const *fmt, ...);
void die(char const *fmt, ...);
char *get_cmdline(char const *name);
char *next_value(char *ptr);

void *kl_malloc(size_t size);
char *kl_strdup(char const *src);
char *kl_strndup(char const *src, int max);
char *kl_sprintf(char const *fmt, ...);
int kl_streq(char const *s1, char const *s2);
int kl_strneq(char const *s1, char const *s2, int max);
int kl_strceq(char const *s1, char const *s2);
int kl_strnceq(char const *s1, char const *s2, int max);

void list_add(void *rptr, void *node);
void list_add_copy(void *rptr, void *node, int size);
void list_del(void *rptr, void *node);

#endif /* !KL_MISC_H */

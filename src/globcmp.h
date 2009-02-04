/* Glob matching function v1.01
 * Copyright (C) 2008 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This code is public domain, I grant permission for anyone to use, modify or
 * redistribute this code in any way, for any purpose in any application, under
 * any license.
 *
 * This code has NO WARRANTY, if it fails to work, causes your program to crash
 * or causes any damage, I accept NO RESPONSIBILITY for it. But if you find a
 * bug, email me and I will try to fix it :)
*/

#ifndef SOLEMN_GLOB_H
#define SOLEMN_GLOB_H

#define GLOB_IGNCASE	(int)(1<<0)
#define GLOB_STAR	(int)(1<<1)
#define GLOB_SINGLE	(int)(1<<2)
#define GLOB_HASH	(int)(1<<3)
#define GLOB_ALL	(GLOB_STAR | GLOB_SINGLE | GLOB_HASH)

int globcmp(char const *str, char const *expr, int flags, ...);

#endif /* !SOLEMN_GLOB_H */

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

/* Version history:
 *
 * v1.0 (2008-07-19)
 *	Initial release
 *
 * v1.01 (2008-11-11)
 *	Bugfix: Buffer overrun when matching the expression *foobar*
*/

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "globcmp.h"

#define RCASE(x) (flags & GLOB_IGNCASE ? tolower(x) : x)

/* Compare function used by globcmp() */
static int mycmp(char const *str, char const *expr, size_t len, int flags) {
	size_t n;
	
	for(n = 0; n < len; n++) {
		if(RCASE(str[n]) == RCASE(expr[n])) {
			if(str[n] == '\0') {
				return 1;
			}
			
			continue;
		}
		if(flags & GLOB_SINGLE && expr[n] == '?' && str[n] != '\0') {
			continue;
		}
		if(flags & GLOB_HASH && expr[n] == '#' && isdigit(str[n])) {
			continue;
		}
		
		return 0;
	}
	
	return 1;
}

/* Match a glob (wildcard) expression against a string
 * Returns 1 on match, zero otherwise
*/
int globcmp(char const *str, char const *expr, int flags, ...) {
	size_t n, l;
	
	while(1) {
		if(flags & GLOB_STAR && expr[0] == '*') {
			while(expr[0] == '*') { expr++; }
			
			if(str[0] == '\0' || expr[0] == '\0') {
				return 1;
			}
			
			n = strcspn(expr, "*");
			
			if(expr[n] == '*') {
				while(!mycmp(str, expr, n, flags)) {
					str++;
					
					if(str[0] == '\0') {
						return 0;
					}
				}
			}else{
				if((l = strlen(str)) < n) {
					return 0;
				}
				
				str = (str + l) - n;
				
				if(mycmp(str, expr, n, flags)) {
					return 1;
				}else{
					return 0;
				}
			}
			
			str += n;
			expr += n;
			continue;
		}
		if(!mycmp(str, expr, 1, flags)) {
			return 0;
		}
		if(expr[0] == '\0') {
			return 1;
		}
		
		expr++;
		str++;
	}
	
	return 1;
}

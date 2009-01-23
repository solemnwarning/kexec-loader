/* kexec-loader - Menu code
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
#include <sys/utsname.h>
#include <poll.h>

#include "console.h"
#include "misc.h"

static void draw_static(void);
static void draw_menu(kl_target *selected);

void menu_main(void) {
	struct pollfd inpoll;
	inpoll.fd = fileno(stdin);
	inpoll.events = POLLIN;
	
	kl_target *target = targets;
	kl_target *tptr = target;
	
	while(tptr) {
		if(tptr->flags & TARGET_DEFAULT) {
			target = tptr;
			break;
		}
		
		tptr = tptr->next;
	}
	
	draw_static();
	draw_menu(target);
	
	if(timeout) {
		console_setpos(1, 3);
		puts("Press any key to abort");
		
		while(timeout) {
			console_setpos(1, 2);
			console_erase(ERASE_LINE);
			printf(
				"Booting '%s' in %d %s...", "AAAA", timeout,
				timeout > 1 ? "seconds" : "second"
			);
			
			if(poll(&inpoll, 1, 1000)) {
				getchar();
				break;
			}
			
			if(--timeout == 0) {
				/* TODO */
				break;
			}
		}
	}
	
	console_setpos(0, 3);
	console_erase(ERASE_LINE);
	console_setpos(0, 2);
	console_erase(ERASE_LINE);
	
	puts(
		"Scroll through the list using the up/down arrow keys and press"
		" ENTER to select a target, D to display disks or C to open the"
		" console."
	);
}

/* Draw static parts of the menu screen */
static void draw_static(void) {
	struct utsname kinfo;
	uname(&kinfo);
	
	char version[64];
	snprintf(version, 64, "kexec-loader " VERSION ", Linux %s", kinfo.release);
	
	char *copyright = "Copyright (C) 2007-2009 Daniel Collins";
	int clen = strlen(copyright), i;
	
	console_clear();
	
	console_setpos(0,0);
	console_attrib(CONS_INVERT);
	for(i = 0; i < console_cols; i++) {
		putchar(' ');
	}
	
	console_setpos(1,0);
	fputs(version, stdout);
	
	console_setpos(console_cols-clen-1, 0);
	fputs(copyright, stdout);
	
	console_attrib(0);
	
	console_setpos(0, 5);
	for(i = 0; i < console_cols; i++) {
		putchar(i == 0 || i == console_cols-1 ? '+' : '-');
	}
	
	for(i = 6; i < console_rows-1; i++) {
		console_setpos(0, i);
		putchar('|');
		
		console_setpos(console_cols-1, i);
		putchar('|');
	}
	
	console_setpos(0, console_rows-1);
	for(i = 0; i < console_cols; i++) {
		putchar(i == 0 || i == console_cols-1 ? '+' : '-');
	}
}

/* Draw the menu entries */
static void draw_menu(kl_target *selected) {
	kl_target *start = targets;
	kl_target *tptr = targets;
	
	int erow = console_rows-2;
	int crow = 6;
	
	while(tptr != selected) {
		if(selected->next && crow+1 == erow) {
			start = start->next;
		}else{
			crow++;
		}
		
		tptr = tptr->next;
	}
	
	tptr = start;
	crow = 6;
	
	while(tptr && crow <= erow) {
		console_setpos(console_cols-2, crow);
		console_erase(ERASE_SOL);
		
		console_setpos(0, crow);
		fputs("| ", stdout);
		
		if(tptr == selected) {
			console_attrib(CONS_INVERT);
			fputs(tptr->title, stdout);
			console_attrib(0);
		}else{
			fputs(tptr->title, stdout);
		}
		
		tptr = tptr->next;
		crow++;
	}
}

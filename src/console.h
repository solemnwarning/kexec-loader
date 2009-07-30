/* kexec-loader - Console header
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

#ifndef KL_CONSOLE_H
#define KL_CONSOLE_H

/* Console colours */
#define CONS_BLACK	30
#define CONS_RED	31
#define CONS_GREEN	32
#define CONS_YELLOW	33
#define CONS_BLUE	34
#define CONS_MAGENTA	35
#define CONS_CYAN	36
#define CONS_WHITE	37

/* Console attributes */
#define CONS_RESET	0
#define CONS_BRIGHT	1
#define CONS_DIM	2
#define CONS_UNDERLINE	4
#define CONS_BLINK	5
#define CONS_INVERT	7
#define CONS_HIDDEN	8

/* Erase modes */
#define ERASE_EOL	"K"
#define ERASE_SOL	"1K"
#define ERASE_LINE	"2K"
#define ERASE_DOWN	"J"
#define ERASE_UP	"1J"
#define ERASE_ALL	"2J"

/* Flags for print2() */
#define P2_DEBUG	(1<<0)
#define P2_ALERT	(1<<1)

/* Special keys */
#define KEY_UP		-129
#define KEY_DOWN	-130
#define KEY_LEFT	-131
#define KEY_RIGHT	-132
#define KEY_HOME	-133
#define KEY_END		-134
#define KEY_DEL		-135
#define KEY_BACKSPACE	-136

#define printd(...) print2(P2_DEBUG, __VA_ARGS__);
#define printD(...) print2(P2_DEBUG | P2_ALERT, __VA_ARGS__);
#define printm(...) print2(0, __VA_ARGS__);
#define printM(...) print2(P2_ALERT, __VA_ARGS__);

extern int alert;
extern int console_cols;
extern int console_rows;

void console_init(void);
void console_setpos(int col, int row);
void console_getpos(int *cptr, int *rptr);
void console_clear(void);
void console_fgcolour(int colour);
void console_bgcolour(int colour);
void console_attrib(int attrib);
void console_getsize(int* cols_p, int* rows_p);
void console_erase(char const *mode);
int console_getchar(void);

void print2(int flags, char const *fmt, ...);

#endif /* !KL_CONSOLE_H */

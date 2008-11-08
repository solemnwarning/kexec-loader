/* kexec-loader - Console header
 * Copyright (C) 2007, Daniel Collins <solemnwarning@solemnwarning.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 *	* Neither the name of the software author nor the names of any
 *	  contributors may be used to endorse or promote products derived from
 *	  this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE SOFTWARE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE SOFTWARE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef KEXEC_LOADER_CONSOLE_H
#define KEXEC_LOADER_CONSOLE_H

/* Console colours */
#define CONS_BLACK	30
#define CONS_RED	31
#define CONS_GREEN	32
#define CONS_YELLOW	33
#define CONS_BLUE	34
#define CONS_MAGENTA	35
#define CONS_CYAN	36
#define CONS_WHITE	37

/* I'm lazy */
#define RED CONS_RED
#define GREEN CONS_GREEN

/* Console attributes */
#define CONS_RESET	0
#define CONS_BRIGHT	1
#define CONS_DIM	2
#define CONS_UNDERLINE	4
#define CONS_BLINK	5
#define CONS_INVERT	7
#define CONS_HIDDEN	8

/* Erase line modes */
#define ELINE_TOEND	""
#define ELINE_TOSTART	"1"
#define ELINE_ALL	"2"

/* Flags for print2() */
#define P2_DEBUG	(1<<0)
#define P2_ALERT	(1<<1)

#define printd(...) print2(P2_DEBUG, __VA_ARGS__);
#define printD(...) print2(P2_DEBUG | P2_ALERT, __VA_ARGS__);
#define printm(...) print2(0, __VA_ARGS__);
#define printM(...) print2(P2_ALERT, __VA_ARGS__);

extern int fgcolour;
extern int bgcolour;

void console_init(void);
void console_setpos(int row, int column);
void console_getpos(int *row, int *col);
void console_clear(void);
void console_fgcolour(int colour);
void console_bgcolour(int colour);
void console_attrib(int attrib);
void console_getsize(int* rows, int* cols);
void console_eline(char const* mode);
void console_cback(int n);

void print2(int flags, int fgcolour, int level, char const *fmt, ...);

#endif /* !KEXEC_LOADER_CONSOLE_H */

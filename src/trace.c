/* kexec-loader - Stack trace functions
 * Copyright (C) 2007-2010 Daniel Collins <solemnwarning@solemnwarning.net>
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

/* Stack trace functions written by Alex Smith (AlexExtreme) */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#define __USE_GNU
#include <sys/ucontext.h>

#include "misc.h"
#include "console.h"

/** Structure containing a stack frame. */
typedef struct stack_frame {
	struct stack_frame *next;
	void *addr;
} stack_frame_t;

static const char *lookup_sym(void *addr) {
	static char ret_sym[256] = "???";
	
	FILE *fh = fopen("/symbol_table", "r");
	if(!fh) {
		return strerror(errno);
	}
	
	char buf[256];
	void *cptr = NULL, *ptr;
	
	while(fgets(buf, 256, fh)) {
		ptr = (void*)strtoul(buf, NULL, 16);
		
		if(addr >= ptr && ptr > cptr) {
			char *s = buf+strspn(buf, "1234567890ABCDEFabcdef");
			strcpy(ret_sym, s+strspn(s, "\t"));
			ret_sym[strcspn(ret_sym, "\n")] = '\0';
			cptr = ptr;
		}
	}
	
	fclose(fh);
	
	return ret_sym;
}

#define PRINT_FRAME(p) printd("#%d\t%p\t(%s)", i++, p, lookup_sym(p))

static void stacktrace(ucontext_t *ctx) {
	stack_frame_t *frame = (stack_frame_t*)ctx->uc_mcontext.gregs[REG_EBP];
	int i = 0;
	
	alert = 0;
	console_clear();
	console_setpos(0,0);
	
	printd(
		"Segfault! EIP = %p, EBP = %p, ESP = %p",
		
		(void*)ctx->uc_mcontext.gregs[REG_EIP],
		(void*)ctx->uc_mcontext.gregs[REG_EBP],
		(void*)ctx->uc_mcontext.gregs[REG_ESP]
	);
	
	printm("");
	printd("Stack trace:");
	
	PRINT_FRAME((void*)ctx->uc_mcontext.gregs[REG_EIP]);
	
	while(frame && frame->addr) {
		PRINT_FRAME(frame->addr);
		frame = frame->next;
	}
	
	printm("");
	printd("Aborting. The kernel will now panic.");
}

static void segv(int signo, siginfo_t *info, void *ctx) {
	stacktrace((ucontext_t *)ctx);
	abort();
}

void enable_trace(void) {
	struct sigaction sa;
	
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv;
	sa.sa_flags = SA_SIGINFO;
	
	if(sigaction(SIGSEGV, &sa, NULL) != 0) {
		debug("Failed to set SIGSEGV action: %s", strerror(errno));
	}
}

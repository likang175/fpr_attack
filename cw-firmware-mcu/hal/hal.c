/*
    This file is part of the ChipWhisperer Example Targets
    Copyright (C) 2012-2015 NewAE Technology Inc.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "hal.h"

__attribute__((weak)) void led_ok(unsigned int status)
{
}

__attribute__((weak)) void led_error(unsigned int status)
{
}

#ifdef PLATFORM_ARM

#if HAL_TYPE != HAL_mps2
void hal_setup(const enum clock_mode clock)
{
	platform_init();
	init_uart();
	trigger_setup();
}
#endif

void hal_send_str(const char *in)
{
	const char *cur = in;
	while (*cur)
		putch(*(cur++));
}

__attribute__((weak)) uint64_t hal_get_time(void)
{
	return 0;
}

/* End of BSS is where the heap starts (defined in the linker script) */
extern char end;
static char *heap_end = &end;

void *__wrap__sbrk(int incr)
{
	char *prev_heap_end;

	prev_heap_end = heap_end;
	heap_end += incr;

	return (void *)prev_heap_end;
}

size_t hal_get_stack_size(void)
{
	register char *cur_stack;
	__asm__ volatile("mov %0, sp" : "=r"(cur_stack));
	return cur_stack - heap_end;
}

const uint32_t stackpattern = 0xDEADBEEFlu;

static void *last_sp = NULL;

void hal_spraystack(void)
{
	char *_heap_end = heap_end;
	asm volatile("mov %0, sp\n"
		     ".L%=:\n\t"
		     "str %2, [%1], #4\n\t"
		     "cmp %1, %0\n\t"
		     "blt .L%=\n\t"
		     : "+r"(last_sp), "+r"(_heap_end)
		     : "r"(stackpattern)
		     : "cc", "memory");
}

size_t hal_checkstack(void)
{
	size_t result = 0;
	asm volatile("sub %0, %1, %2\n"
		     ".L%=:\n\t"
		     "ldr ip, [%2], #4\n\t"
		     "cmp ip, %3\n\t"
		     "ite eq\n\t"
		     "subeq %0, #4\n\t"
		     "bne .LE%=\n\t"
		     "cmp %2, %1\n\t"
		     "blt .L%=\n\t"
		     ".LE%=:\n"
		     : "+r"(result)
		     : "r"(last_sp), "r"(heap_end), "r"(stackpattern)
		     : "ip", "cc");
	return result;
}

/* Implement some system calls to shut up the linker warnings */

#include <errno.h>
#undef errno
extern int errno;

int __wrap__open(char *file, int flags, int mode)
{
	(void)file;
	(void)flags;
	(void)mode;
	errno = ENOSYS;
	return -1;
}

#endif

#ifdef __GNUC__
#if ((__GNUC__ > 11) || \
     ((__GNUC__ == 11) && (__GNUC_MINOR__ >= 3)))
__attribute__((weak)) void _close() {}
__attribute__((weak)) void _fstat() {}
__attribute__((weak)) void _getpid() {}
__attribute__((weak)) void _isatty() {}
__attribute__((weak)) void _kill() {}
__attribute__((weak)) void _lseek() {}
__attribute__((weak)) void _read() {}
__attribute__((weak)) void _write() {}
#endif
#endif

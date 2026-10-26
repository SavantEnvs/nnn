/* Shared harness prelude: pull nnn's single translation unit in so its static functions are callable. */
#ifndef NNN_HARNESS_H
#define NNN_HARNESS_H

#define main nnn_real_main
#include "../src/nnn.c"
#undef main

#include <stdint.h>

static void nnn_harness_init(void)
{
	static int done;

	if (done)
		return;
	done = 1;

	setlocale(LC_ALL, "C.UTF-8");
	home = strdup("/home/mayhem");
	homelen = (ushort_t)strlen(home);
}

#endif

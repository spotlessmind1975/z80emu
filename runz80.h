/* runz80.h
 * Header for runz80 example.
 *
 * Copyright (c) 2021 Marco Spedaletti
 *
 * This code is free, do whatever you want with it.
 */

#ifndef __RUNZ80_INCLUDED__
#define __RUNZ80_INCLUDED__

#include "z80emu.h"

typedef struct RUNZ80
{

	Z80_STATE state;
	unsigned char memory[1 << 16];

	int is_done;
	int stop_address;
	int org_address;
	char *output_filename;
	int label_count;
	char *label_name[1024];
	int label_address[1024];
	int inspection_count;
	char *inspection_name[1024];
	int inspection_address[1024];
	int inspection_size[1024];
	char *profile_filename;
	int profile_cycles;
	int profile_heatmap[1 << 16];
	int profile_heatmap_max;

} RUNZ80;

extern void SystemCall(RUNZ80 *runz80);

#endif

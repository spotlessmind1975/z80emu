/* runz80.c
 * Example program using z80emu to run any z80 binary file.
 *
 * Copyright (c) 2021, Marco Spedaletti
 *
 * This code is free, do whatever you want with it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "runz80.h"
#include "z80emu.h"
#include <string.h>

#define Z80_CPU_SPEED           4000000   /* In Hz. */
#define CYCLES_PER_STEP         (Z80_CPU_SPEED / 50)
#define MAXIMUM_STRING_LENGTH   100

static unsigned long htol(char *hex)
{
  char *end;
  unsigned long l= strtol(hex, &end, 16);
  if (*end) {
    fprintf(stderr, "bad hex number: %s", hex);
  }
  return l;
}

static void usage(int status)
{
  FILE *stream= status ? stderr : stdout;
  fprintf(stream, "usage: runz80 [option ...]\n");
  fprintf(stream, "  -h                -- help (print this message)\n");
  fprintf(stream, "  -l addr file      -- load file at addr\n");
  fprintf(stream, "  -R addr           -- run from this address\n");
  fprintf(stream, "  -X addr           -- terminate emulation if PC reaches addr\n");
  fprintf(stream, "  -p file cycles    -- enable profiling\n");
  fprintf(stream, "\n");
  exit(status);
}


static int doHelp(int argc, char **argv, RUNZ80 *context)
{
  usage(0);
  return 0;
}


static int doLoad(int argc, char **argv, RUNZ80 *context)	/* -l addr file */
{
        char * filename;
        FILE * file;
        int offset;
        int size;
        if (argc < 3) usage(1);
        filename = argv[2];
        offset = htol(argv[1]);
        if ((file = fopen(filename, "rb")) == NULL) {
                fprintf(stderr, "Can't open file!\n");
                exit(EXIT_FAILURE);
        }
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);
        fread(context->memory + offset, 1, size, file);
        fclose(file);
        context->org_address = offset;
        return 2;
}

static int doProfiling(int argc, char **argv, RUNZ80 *context)	/* -l addr file */
{
        char * filename = NULL;
        int cycles = 0;
        if (argc < 3) usage(1);
        filename = argv[1];
        cycles = atoi( argv[2] );
        context->profile_filename = strdup( filename );
        context->profile_cycles = cycles;
        memset( context->profile_heatmap, 0, sizeof(int) * (1<<16) );
        return 2;
}

static int doRST(int argc, char **argv, RUNZ80 *context)	/* -l addr file */
{
        int address;
        if (argc < 2) usage(1);
        address = htol(argv[1]);
        Z80Reset(&context->state);
        context->state.pc = address;
        // context->org_address = address;
        return 1;
}

static int doXtrap(int argc, char **argv, RUNZ80 *context)	/* -l addr file */
{
        int address;
        if (argc < 2) usage(1);
        address = htol(argv[1]);
        context->stop_address = address;
        return 1;
}

static int doOutput(int argc, char **argv, RUNZ80 *context)	/* -l addr file */
{
        char * filename;
        if (argc < 2) usage(1);
        filename = argv[1];
        context->output_filename = filename;
        return 1;
}

static int doLabels(int argc, char **argv, RUNZ80 *context)	/* -L file */
{
  if (argc < 2) usage(1);
  char * filename = argv[1];
  FILE * handle = fopen( filename, "rt");
  while(!feof(handle) ) {
    char command[1024];
    char address[128];
    char label[128];
    fscanf( handle, "%s = $%s", label, address);
    fgets( command, 1024, handle );

    context->label_name[context->label_count] = strdup( label );
    context->label_address[context->label_count] = context->org_address + htol( address );
    ++context->label_count;
  }
  fclose(handle);
  return 1;
}

static int doLabels2(int argc, char **argv, RUNZ80 *context)	/* -L file */
{
  if (argc < 2) usage(1);
  char * filename = argv[1];
  FILE * handle = fopen( filename, "rt");
  while(!feof(handle) ) {
    char address[16];
    char label[128];
    fscanf( handle, "%s %s", address, label );
    context->label_name[context->label_count] = strdup( label );
    context->label_address[context->label_count] = context->org_address + htol( address );
    ++context->label_count;
  }
  fclose(handle);
  return 1;
}


static int doInspections(int argc, char **argv, RUNZ80 *context)	/* -L file */
{
  if (argc < 2) usage(1);
  char * filename = argv[1];
  FILE * handle = fopen( filename, "rt");
  while(!feof(handle) ) {
    char address[128];
    char size[128];
    char label[128];
    memset( label, 0, 16 );
    fscanf( handle, "%s %s %s", address, size, label);

    if ( strcmp( label, "" ) == 0 ) {
            continue;
    }
    context->inspection_name[context->inspection_count] = strdup( label );
    context->inspection_size[context->inspection_count] = htol( size );
    context->inspection_address[context->inspection_count] = htol( address );
    ++context->inspection_count;
  }
  fclose(handle);
  return 1;
}

extern int trace;
extern char * listing_instructions[];
extern char * listing_lines[];

static int doListing(int argc, char **argv, RUNZ80 *context)	/* -i file */
{
    char line[256];
  if (argc < 2) usage(1);
  char * filename = argv[1];
  FILE * handle = fopen( filename, "rt");

  int lastLine = 0;

  while(!feof(handle) ) {
	(void)!fgets( line, 256, handle );

        char * sp = line + 6;
	char * sp2 = strchr( sp, ' ' );
	if ( ! sp2 ) continue;
	if ( ( sp2 - sp ) > 5 ) continue;
	
	char address[128];
	memset(address, 0, 16 );
	memcpy(address, sp, (sp2 - sp) );

	int pc = htol( address );

	sp = strchr( sp2+1, 0x9 );
        if ( !sp ) continue;

	sp2 = strchr( sp+1, 0 );
        if ( !sp2 ) continue;

	char instructions[256];
	memset(instructions, 0, 256 );
	memcpy(instructions, sp, (sp2 - sp)-1 );
	if ( strcmp(instructions, "" ) == 0 ) continue;

        sp = strstr( instructions, "; L");

        if ( sp != NULL ) {
                sp += 4;
                lastLine = atoi( sp );
        }

    listing_instructions[context->org_address+pc] = strdup( instructions );
    listing_lines[context->org_address+pc] = lastLine;
//     printf( "%4.4x %s\n", context->org_address+pc, listing_instructions[context->org_address+pc] );

  }
  fclose(handle);
  return 1;
}


int main (int argc, char **argv) {

RUNZ80	context;

    while (++argv, --argc > 0)
      {
	int n= 0;

	if (!strcmp(*argv, "-h"))	n= doHelp(argc, argv, &context);
	else if (!strcmp(*argv, "-l"))	n= doLoad(argc, argv, &context);
	else if (!strcmp(*argv, "-R"))	n= doRST(argc, argv, &context);
	else if (!strcmp(*argv, "-L"))	n= doLabels(argc, argv, &context);
	else if (!strcmp(*argv, "-Li"))	n= doInspections(argc, argv, &context);
	else if (!strcmp(*argv, "-O"))	n= doOutput(argc, argv, &context);
	else if (!strcmp(*argv, "-X"))	n= doXtrap(argc, argv, &context);
	else if (!strcmp(*argv, "-u"))	n= doListing(argc, argv, &context);
	else if (!strcmp(*argv, "-p"))	n= doProfiling(argc, argv, &context);
	else if (!strcmp(*argv, "-t"))	{ trace = 1; }
	else if ('-' == **argv)		usage(1);
	else
	  {

	  }
	argc -= n;
	argv += n;
      }

	context.is_done = 0;

        /* Emulate. */

	do {

                if ( context.profile_filename != NULL ) {
                        printf("%4.4x\n", context.state.pc);
                        ++context.profile_heatmap[context.state.pc];

                        if ( ! context.profile_cycles ) {
                                context.is_done = 1;
                        } else {
                                --context.profile_cycles;
                        }
                }

                Z80Emulate(&context.state, 1, &context);

                if ( context.state.pc == context.stop_address ) {
                        context.is_done = 1;
                }

        } while (!context.is_done&&context.state.status!=Z80_STATUS_FLAG_HALT);

        if ( context.profile_filename != NULL ) {
                FILE * profile_heatmap_file = fopen(context.profile_filename, "wt+");
                for( int i=0; i<(1<<16); ++i) {
                        if ( listing_instructions[i] ) {
                                fprintf( profile_heatmap_file, "%4.4x %4.4x %4.4 %s\n", context.profile_heatmap[i], i, listing_lines[i], listing_instructions[i]  );
                        }
                }
                fclose( profile_heatmap_file );
        }

        if ( context.output_filename ) {
                int i,j,max;
                FILE * handle = fopen( context.output_filename, "wt");
                fprintf(handle,"%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x\n", 
                context.state.registers.byte[Z80_A],
                context.state.registers.byte[Z80_B],
                context.state.registers.byte[Z80_C],
                context.state.registers.byte[Z80_D],
                context.state.registers.byte[Z80_E],
                context.state.registers.byte[Z80_F],
                context.state.registers.byte[Z80_H],
                context.state.registers.byte[Z80_L]
                );
                for( i=0; i<context.label_count; ++i ){
                if ( 
                        strcmp( context.label_name[i], "WORKING") == 0 
                        || strcmp( context.label_name[i], "TEMPORARY") == 0 
                        || strcmp( context.label_name[i], "DESCRIPTORS") == 0 
                        ) {
                                fprintf(handle, "%s\n", context.label_name[i]);
                                if ( strcmp( context.label_name[i], "WORKING") == 0 
                                        || strcmp( context.label_name[i], "TEMPORARY") == 0 ) {
                                max = 1024;
                                fprintf(handle, "%4.4x ", context.label_address[i]);
                        } else {
                                max = 4*255;
                        }
                        for( j=0; j<max; ++j ){
                                fprintf(handle, "%2.2x ", context.memory[context.label_address[i]+j]);
                        }
                        fprintf(handle, "\n");
                } else if ( strcmp( context.label_name[i], "USING") == 0 ) { 
                        fprintf(handle, "%s\n", context.label_name[i]);
                        fprintf(handle, "%2.2x ", context.memory[context.label_address[i]+j]);
                } else {
                }
                }
                for( i=0; i<context.label_count; ++i ){
                        if (     
                                strcmp( context.label_name[i], "WORKING") == 0 
                                || strcmp( context.label_name[i], "TEMPORARY") == 0 
                                || strcmp( context.label_name[i], "DESCRIPTORS") == 0 
                                || strcmp( context.label_name[i], "USING") == 0 
                                ) {

                        } else {
                                fprintf(handle, "%s %4.4x %2.2x %2.2x %2.2x %2.2x\n", context.label_name[i], context.label_address[i], context.memory[context.label_address[i]], context.memory[context.label_address[i]+1], context.memory[context.label_address[i]+2], context.memory[context.label_address[i]+3]);
                        }
                }

                for( i=0; i<context.inspection_count; ++i ){
                        fprintf(handle, "%s\n", context.inspection_name[i]);
                        for( j=0; j<context.inspection_size[i]; ++j ){
                                fprintf(handle, "%2.2x ", context.memory[context.inspection_address[i]+j]);
                        }
                        fprintf(handle, "\n");
                }

                fclose(handle);
        }

}

/* Emulate CP/M bdos call 5 functions 2 (output character on screen) and 9
 * (output $-terminated string to screen).
 */

void SystemCall (RUNZ80 *zextest)
{
        if (zextest->state.registers.byte[Z80_C] == 2)

                printf("%c", zextest->state.registers.byte[Z80_E]);

        else if (zextest->state.registers.byte[Z80_C] == 9) {

                int     i, c;

                for (i = zextest->state.registers.word[Z80_DE], c = 0; 
                        zextest->memory[i] != '$';
                        i++) {

                        printf("%c", zextest->memory[i & 0xffff]);
                        if (c++ > MAXIMUM_STRING_LENGTH) {

                                fprintf(stderr,
                                        "String to print is too long!\n");
                                exit(EXIT_FAILURE);

                        }

                }

        }
}

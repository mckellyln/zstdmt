
/**
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "zstdmt.h"

/**
 * program for testing threaded stuff on zstd
 */

static void perror_exit(const char *msg)
{
	printf("%s\n", msg);
	fflush(stdout);
	exit(1);
}

static void version(void)
{
	printf("zstdmt version 0.1\n");

	exit(0);
}

static void usage(void)
{
	printf("Usage: zstdmt [options] infile outfile\n\n");
	printf("Otions:\n");
	printf(" -l N    set level of compression (default: 3)\n");
	printf(" -t N    set number of (de)compression threads (default: 2)\n");
	printf(" -i N    set number of iterations for testing (default: 1)\n");
	printf(" -b N    set input chunksize to N KiB (default: auto)\n");
	printf(" -c      compress (default mode)\n");
	printf(" -d      use decompress mode\n");
	printf(" -h      show usage\n");
	printf(" -v      show version\n");

	exit(0);
}

#define MODE_COMPRESS    1
#define MODE_DECOMPRESS  2

/* for the -i option */
#define MAX_ITERATIONS   1000

int my_read_loop(void *arg, ZSTDMT_Buffer * in)
{
	FILE *fd = (FILE *) arg;
	ssize_t done = fread(in->buf, 1, in->size, fd);

#if 0
	printf("fread(), todo=%u done=%u\n", in->size, done);
	fflush(stdout);
#endif

	in->size = done;
	return 0;
}

int my_write_loop(void *arg, ZSTDMT_Buffer * out)
{
	FILE *fd = (FILE *) arg;
	ssize_t done = fwrite(out->buf, 1, out->size, fd);

#if 0
	printf("fwrite(), todo=%u done=%u\n", out->size, done);
	fflush(stdout);
#endif

	out->size = done;
	return 0;
}

static void
do_compress(int threads, int level, int bufsize, FILE * fin, FILE * fout)
{
	ZSTDMT_RdWr_t rdwr;
	size_t ret;

	/* 1) setup read/write functions */
	rdwr.fn_read = my_read_loop;
	rdwr.fn_write = my_write_loop;
	rdwr.arg_read = (void *)fin;
	rdwr.arg_write = (void *)fout;

	/* 2) create compression context */
	ZSTDMT_CCtx *ctx = ZSTDMT_createCCtx(threads, level, bufsize);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	/* 3) compress */
	ret = ZSTDMT_CompressCCtx(ctx, &rdwr);
	if (ZSTDMT_isError(ret))
		perror_exit(ZSTDMT_getErrorString(ret));

	/* 5) free resources */
	ZSTDMT_freeCCtx(ctx);
}

static void do_decompress(int threads, int bufsize, FILE * fin, FILE * fout)
{
	ZSTDMT_RdWr_t rdwr;
	size_t ret;

	/* 1) setup read/write functions */
	rdwr.fn_read = my_read_loop;
	rdwr.fn_write = my_write_loop;
	rdwr.arg_read = (void *)fin;
	rdwr.arg_write = (void *)fout;

	/* 2) create compression context */
	ZSTDMT_DCtx *ctx = ZSTDMT_createDCtx(threads, bufsize);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	/* 3) compress */
	ret = ZSTDMT_DecompressDCtx(ctx, &rdwr);
	if (ZSTDMT_isError(ret))
		perror_exit(ZSTDMT_getErrorString(ret));

	/* 4) free resources */
	ZSTDMT_freeDCtx(ctx);
}

int main(int argc, char **argv)
{
	/* default options: */
	int opt, opt_threads = 2, opt_level = 3;
	int opt_mode = MODE_COMPRESS;
	int opt_iterations = 1, opt_bufsize = 0;
	FILE *fin, *fout;

	while ((opt = getopt(argc, argv, "vhHl:t:i:dcb:")) != -1) {
		switch (opt) {
		case 'v':	/* version */
			version();
		case 'h':	/* help */
			usage();
		case 'l':	/* level */
			opt_level = atoi(optarg);
			break;
		case 't':	/* threads */
			opt_threads = atoi(optarg);
			break;
		case 'i':	/* iterations */
			opt_iterations = atoi(optarg);
			break;
		case 'd':	/* mode = decompress */
			opt_mode = MODE_DECOMPRESS;
			break;
		case 'c':	/* mode = compress */
			opt_mode = MODE_COMPRESS;
			break;
		case 'b':	/* input buffer in MB */
			opt_bufsize = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	/* prog [options] infile outfile */
	if (argc != optind + 2)
		usage();

	/**
	 * check parameters
	 */

	/* opt_level = 1..22 */
	if (opt_level < 1)
		opt_level = 1;
	else if (opt_level > ZSTDMT_LEVEL_MAX)
		opt_level = ZSTDMT_LEVEL_MAX;

	/* opt_threads = 1..ZSTDMT_THREAD_MAX */
	if (opt_threads < 1)
		opt_threads = 1;
	else if (opt_threads > ZSTDMT_THREAD_MAX)
		opt_threads = ZSTDMT_THREAD_MAX;

	/* opt_iterations = 1..MAX_ITERATIONS */
	if (opt_iterations < 1)
		opt_iterations = 1;
	else if (opt_iterations > MAX_ITERATIONS)
		opt_iterations = MAX_ITERATIONS;

	/* opt_bufsize is in KiB */
	if (opt_bufsize > 0)
		opt_bufsize *= 1024;

	/* file names */
	fin = fopen(argv[optind], "rb");
	if (fin == NULL)
		perror_exit("Opening infile failed");

	fout = fopen(argv[optind + 1], "wb");
	if (fout == NULL)
		perror_exit("Opening outfile failed");

	for (;;) {
		if (opt_mode == MODE_COMPRESS) {
			do_compress(opt_threads, opt_level, opt_bufsize, fin,
				    fout);
		} else {
			do_decompress(opt_threads, opt_bufsize, fin, fout);
		}

		opt_iterations--;
		if (opt_iterations == 0)
			break;

		fseek(fin, 0, SEEK_SET);
		fseek(fout, 0, SEEK_SET);
	}

	/* exit should flush stdout */
	exit(0);
}

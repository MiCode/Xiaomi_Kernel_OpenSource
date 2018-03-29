/*
 * Copyright (C) 2014 Sergey Senozhatsky.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/lz4k.h>

#include "zcomp_lz4k.h"

static void *zcomp_lz4k_create(void)
{
	return kzalloc(LZ4K_MEM_COMPRESS, GFP_KERNEL);
}

static void zcomp_lz4k_destroy(void *private)
{
	kfree(private);
}
#ifdef CONFIG_ZSM
static int zcomp_lz4k_compress_zram(const unsigned char *src, unsigned char *dst,
		size_t *dst_len, void *private, int *checksum)
{
	/* return  : Success if return 0 */
	return lz4k_compress_zram(src, PAGE_SIZE, dst, dst_len, private, checksum);
}
#else
static int zcomp_lz4k_compress(const unsigned char *src, unsigned char *dst,
		size_t *dst_len, void *private)
{
	/* return  : Success if return 0 */
	return lz4k_compress(src, PAGE_SIZE, dst, dst_len, private);
}
#endif
static int zcomp_lz4k_decompress(const unsigned char *src, size_t src_len,
		unsigned char *dst)
{
	size_t dst_len = PAGE_SIZE;
	/* return  : Success if return 0 */
	return lz4k_decompress_safe(src, src_len, dst, &dst_len);
}
#ifdef CONFIG_ZSM
struct zcomp_backend zcomp_lz4k = {
	.compress = zcomp_lz4k_compress_zram,
	.decompress = zcomp_lz4k_decompress,
	.create = zcomp_lz4k_create,
	.destroy = zcomp_lz4k_destroy,
	.name = "lz4k",
};
#else
struct zcomp_backend zcomp_lz4k = {
	.compress = zcomp_lz4k_compress,
	.decompress = zcomp_lz4k_decompress,
	.create = zcomp_lz4k_create,
	.destroy = zcomp_lz4k_destroy,
	.name = "lz4k",
};
#endif


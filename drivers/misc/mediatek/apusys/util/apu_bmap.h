/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_BMAP_H__
#define __APU_BMAP_H__

#include <linux/spinlock.h>

#define APU_BMAP_NAME_LEN 16

struct apu_bmap {
	/* input */
	uint32_t start;
	uint32_t end;
	uint32_t au;  // allocation unit (in bytes)
	unsigned long align_mask;
	char name[APU_BMAP_NAME_LEN];

	// output
	uint32_t size;
	unsigned long *b;     // bitmap
	unsigned long nbits;  // number of bits
	spinlock_t lock;
};

#define is_au_align(ab, val) (!((val) & (ab->au - 1)))

int apu_bmap_init(struct apu_bmap *ab, const char *name);
void apu_bmap_exit(struct apu_bmap *ab);
uint32_t apu_bmap_alloc(struct apu_bmap *ab, unsigned int size,
	uint32_t given_addr);
void apu_bmap_free(struct apu_bmap *ab, uint32_t addr, unsigned int size);

#endif

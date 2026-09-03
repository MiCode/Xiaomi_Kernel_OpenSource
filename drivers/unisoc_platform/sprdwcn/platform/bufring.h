/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _BUGRING_H
#define _BUGRING_H

#include <linux/mutex.h>
#include <linux/sched.h>

#define MDBG_RING_R		0
#define MDBG_RING_W		1

#define MDBG_RING_MIN_SIZE	(1024 * 4)

#define MDBG_RING_LOCK_SIZE (sizeof(struct mutex))

struct mdbg_ring_t {
	unsigned long int size;
	char *pbuff;
	char *rp;
	char *wp;
	char *end;
	/* 0: WP > RP; 1: RP >WP */
	bool p_order_flag;
	/* is_mem=1:mem, 0:log */
	bool is_mem;
	struct mutex *plock;
};

struct mdbg_ring_t *mdbg_ring_alloc(unsigned long int size);
void mdbg_ring_destroy(struct mdbg_ring_t *ring);
int mdbg_ring_read(struct mdbg_ring_t *ring, void *buf, int len);
int mdbg_ring_write(struct mdbg_ring_t *ring, void *buf, unsigned int len);
int mdbg_ring_write_timeout(struct mdbg_ring_t *ring, void *buf,
			    unsigned int len, unsigned int timeout);
char *mdbg_ring_write_ext(struct mdbg_ring_t *ring, long int len);
bool mdbg_ring_will_full(struct mdbg_ring_t *ring, long int len);
long int mdbg_ring_free_space(struct mdbg_ring_t *ring);
long int mdbg_ring_readable_len(struct mdbg_ring_t *ring);
void mdbg_ring_clear(struct mdbg_ring_t *ring);
void mdbg_ring_reset(struct mdbg_ring_t *ring);
bool mdbg_ring_over_loop(struct mdbg_ring_t *ring, u_long len, int rw);
void mdbg_ring_print(struct mdbg_ring_t *ring);
#endif


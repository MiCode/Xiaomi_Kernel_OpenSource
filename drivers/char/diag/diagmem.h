/* Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DIAGMEM_H
#define DIAGMEM_H
#include "diagchar.h"

#define POOL_TYPE_COPY			0
#define POOL_TYPE_HDLC			1
#define POOL_TYPE_USER			2
#define POOL_TYPE_MUX_APPS		3
#define POOL_TYPE_DCI			4
#define POOL_TYPE_LOCAL_LAST		5

#define POOL_TYPE_REMOTE_BASE		POOL_TYPE_LOCAL_LAST
#define POOL_TYPE_MDM			POOL_TYPE_REMOTE_BASE
#define POOL_TYPE_MDM2			(POOL_TYPE_REMOTE_BASE + 1)
#define POOL_TYPE_MDM_DCI		(POOL_TYPE_REMOTE_BASE + 2)
#define POOL_TYPE_MDM2_DCI		(POOL_TYPE_REMOTE_BASE + 3)
#define POOL_TYPE_MDM_MUX		(POOL_TYPE_REMOTE_BASE + 4)
#define POOL_TYPE_MDM2_MUX		(POOL_TYPE_REMOTE_BASE + 5)
#define POOL_TYPE_MDM_DCI_WRITE		(POOL_TYPE_REMOTE_BASE + 6)
#define POOL_TYPE_MDM2_DCI_WRITE	(POOL_TYPE_REMOTE_BASE + 7)
#define POOL_TYPE_QSC_MUX		(POOL_TYPE_REMOTE_BASE + 8)
#define POOL_TYPE_REMOTE_LAST		(POOL_TYPE_REMOTE_BASE + 9)

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
#define NUM_MEMORY_POOLS		POOL_TYPE_REMOTE_LAST
#else
#define NUM_MEMORY_POOLS		POOL_TYPE_LOCAL_LAST
#endif

#define DIAG_MEMPOOL_NAME_SZ		24
#define DIAG_MEMPOOL_GET_NAME(x)	(diag_mempools[x].name)

struct diag_mempool_t {
	int id;
	char name[DIAG_MEMPOOL_NAME_SZ];
	mempool_t *pool;
	unsigned int itemsize;
	unsigned int poolsize;
	int count;
	spinlock_t lock;
} __packed;

extern struct diag_mempool_t diag_mempools[NUM_MEMORY_POOLS];

void diagmem_setsize(int pool_idx, int itemsize, int poolsize);
void *diagmem_alloc(struct diagchar_dev *driver, int size, int pool_type);
void diagmem_free(struct diagchar_dev *driver, void *buf, int pool_type);
void diagmem_init(struct diagchar_dev *driver, int type);
void diagmem_exit(struct diagchar_dev *driver, int type);

#endif

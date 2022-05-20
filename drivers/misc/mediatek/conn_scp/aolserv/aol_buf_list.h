/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _AOL_BUF_LIST_H_
#define _AOL_BUF_LIST_H_

#include <linux/spinlock.h>

#define MAX_BUF_LEN     (1024)

struct aol_buf {
	char buf[MAX_BUF_LEN];
	uint32_t size;
	uint32_t msg_id;
	struct list_head list;
};

struct aol_buf_list {
	struct mutex lock;
	struct list_head list;
};

struct aol_buf_pool {
	struct aol_buf_list free_buf_list;
	struct aol_buf_list active_buf_list;
	u32 buf_size;
};


struct aol_buf *aol_buffer_alloc(struct aol_buf_pool *pool);
void aol_buffer_free(struct aol_buf_pool *pool, struct aol_buf *buf);
void aol_buffer_active_push(struct aol_buf_pool *pool, struct aol_buf *buf);
struct aol_buf *aol_buffer_active_pop(struct aol_buf_pool *pool);
bool aol_buffer_active_is_empty(struct aol_buf_pool *pool);


int aol_buf_pool_init(struct aol_buf_pool *pool);
int aol_buf_pool_deinit(struct aol_buf_pool *pool);


#endif // _AOL_BUF_LIST_H_

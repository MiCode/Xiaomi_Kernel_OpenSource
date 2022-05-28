// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include "aol_buf_list.h"


struct aol_buf *aol_buffer_alloc(struct aol_buf_pool *pool)
{
	struct aol_buf *buf = NULL;

	if (mutex_lock_killable(&pool->free_buf_list.lock))
		return NULL;
	if (list_empty(&pool->free_buf_list.list)) {
		buf = kmalloc(sizeof(struct aol_buf), GFP_KERNEL);
		if (buf == NULL) {
			mutex_unlock(&pool->free_buf_list.lock);
			return NULL;
		}
		memset(buf, 0, sizeof(struct aol_buf));
		INIT_LIST_HEAD(&buf->list);
		pool->buf_size++;
		pr_info("[%s] bufsize=[%d]", __func__, pool->buf_size);

	} else {
		buf = list_first_entry(&pool->free_buf_list.list, struct aol_buf, list);
		list_del(&buf->list);
	}
	mutex_unlock(&pool->free_buf_list.lock);
	return buf;
}

void aol_buffer_free(struct aol_buf_pool *pool, struct aol_buf *buf)
{
	if (!buf)
		return;
	if (mutex_lock_killable(&pool->free_buf_list.lock))
		return;
	list_add_tail(&buf->list, &pool->free_buf_list.list);
	mutex_unlock(&pool->free_buf_list.lock);
}

void aol_buffer_active_push(struct aol_buf_pool *pool, struct aol_buf *buf)
{
	if (!buf)
		return;

	if (mutex_lock_killable(&pool->active_buf_list.lock))
		return;
	list_add_tail(&buf->list, &pool->active_buf_list.list);
	mutex_unlock(&pool->active_buf_list.lock);
}

struct aol_buf *aol_buffer_active_pop(struct aol_buf_pool *pool)
{
	struct aol_buf *buf = NULL;

	if (mutex_lock_killable(&pool->active_buf_list.lock))
		return NULL;
	if (list_empty(&pool->active_buf_list.list)) {
		mutex_unlock(&pool->active_buf_list.lock);
		return NULL;
	}

	buf = list_first_entry(&pool->active_buf_list.list, struct aol_buf, list);
	list_del(&buf->list);
	mutex_unlock(&pool->active_buf_list.lock);

	return buf;
}

bool aol_buffer_active_is_empty(struct aol_buf_pool *pool)
{
	if (mutex_lock_killable(&pool->active_buf_list.lock))
		return true;

	if (list_empty(&pool->active_buf_list.list)) {
		mutex_unlock(&pool->active_buf_list.lock);
		return true;
	}
	mutex_unlock(&pool->active_buf_list.lock);
	return false;
}


int aol_buf_pool_init(struct aol_buf_pool *pool)
{
	memset(pool, 0, sizeof(*pool));

	mutex_init(&pool->free_buf_list.lock);
	INIT_LIST_HEAD(&pool->free_buf_list.list);

	mutex_init(&pool->active_buf_list.lock);
	INIT_LIST_HEAD(&pool->active_buf_list.list);
	return 0;
}

int aol_buf_pool_deinit(struct aol_buf_pool *pool)
{
	struct aol_buf *buf = NULL;


	if (mutex_lock_killable(&pool->free_buf_list.lock))
		return -1;
	while (!list_empty(&pool->free_buf_list.list)) {
		buf = list_first_entry(&pool->free_buf_list.list, struct aol_buf, list);
		list_del(&buf->list);
		kfree(buf);
	}
	mutex_unlock(&pool->free_buf_list.lock);

	if (mutex_lock_killable(&pool->active_buf_list.lock))
		return -1;
	while (!list_empty(&pool->active_buf_list.list)) {
		buf = list_first_entry(&pool->active_buf_list.list, struct aol_buf, list);
		list_del(&buf->list);
		kfree(buf);
	}
	mutex_unlock(&pool->active_buf_list.lock);

	mutex_destroy(&pool->free_buf_list.lock);
	mutex_destroy(&pool->active_buf_list.lock);

	return 0;
}


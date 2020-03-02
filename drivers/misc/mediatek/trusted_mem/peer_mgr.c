/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "private/mld_helper.h"
#include "private/tmem_priv.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"

#define MGR_SESSION_LOCK() mutex_lock(&sess_data->lock)
#define MGR_SESSION_UNLOCK() mutex_unlock(&sess_data->lock)

static void set_session_ready(struct trusted_peer_session *sess_data,
			      bool ready)
{
	sess_data->opened = ready;
}

static bool is_session_ready(struct trusted_peer_session *sess_data)
{
	bool ret;

	MGR_SESSION_LOCK();
	ret = sess_data->opened;
	MGR_SESSION_UNLOCK();
	return ret;
}

static int peer_mgr_chunk_alloc_locked(
	u32 alignment, u32 size, u32 *refcount, u32 *sec_handle, u8 *owner,
	u32 id, u32 clean, struct trusted_driver_operations *drv_ops,
	struct trusted_peer_session *sess_data, void *peer_priv)
{
	int ret;

	if (!is_session_ready(sess_data)) {
		pr_err("%s:%d peer session is still not ready!\n", __func__,
		       __LINE__);
		return TMEM_MGR_SESSION_IS_NOT_READY;
	}

	MGR_SESSION_LOCK();

	ret = drv_ops->memory_alloc(alignment, size, refcount, sec_handle,
				    owner, id, clean, sess_data->peer_data,
				    peer_priv);
	if (ret != 0) {
		pr_err("peer alloc size: 0x%x failed:%d\n", size, ret);
		MGR_SESSION_UNLOCK();
		return TMEM_MGR_ALLOC_MEM_FAILED;
	}

	*refcount = 1;
	sess_data->ref_chunks++;

	MGR_SESSION_UNLOCK();
	return TMEM_OK;
}

static int peer_mgr_chunk_free_locked(u32 sec_handle, uint8_t *owner, u32 id,
				      struct trusted_driver_operations *drv_ops,
				      struct trusted_peer_session *sess_data,
				      void *peer_priv)
{
	int ret;

	if (!is_session_ready(sess_data)) {
		pr_err("%s:%d peer session is still not ready!\n", __func__,
		       __LINE__);
		return TMEM_MGR_SESSION_IS_NOT_READY;
	}

	MGR_SESSION_LOCK();

	ret = drv_ops->memory_free(sec_handle, owner, id, sess_data->peer_data,
				   peer_priv);
	if (ret != 0) {
		pr_err("peer free chunk memory failed:%d\n", ret);
		MGR_SESSION_UNLOCK();
		return TMEM_MGR_FREE_MEM_FAILED;
	}

	if (IS_ZERO(sess_data->ref_chunks))
		pr_err("system error, please check! (ref_chunks:0)\n");
	else
		sess_data->ref_chunks--;

	MGR_SESSION_UNLOCK();
	return TMEM_OK;
}

static int peer_mgr_mem_add_locked(u64 pa, u32 size,
				   struct trusted_driver_operations *drv_ops,
				   struct trusted_peer_session *sess_data,
				   void *peer_priv)
{
	int ret;

	if (!is_session_ready(sess_data)) {
		pr_err("%s:%d peer session is still not ready!\n", __func__,
		       __LINE__);
		return TMEM_MGR_SESSION_IS_NOT_READY;
	}

	MGR_SESSION_LOCK();

	ret = drv_ops->memory_grant(pa, size, sess_data->peer_data, peer_priv);
	if (ret != 0) {
		pr_err("peer append reg mem failed:%d\n", ret);
		MGR_SESSION_UNLOCK();
		return TMEM_MGR_MEM_ADD_FAILED;
	}

	sess_data->mem_pa_start = pa;
	sess_data->mem_size = size;

	MGR_SESSION_UNLOCK();
	return TMEM_OK;
}

static int peer_mgr_mem_remove_locked(struct trusted_driver_operations *drv_ops,
				      struct trusted_peer_session *sess_data,
				      void *peer_priv)
{
	int ret;

	if (!is_session_ready(sess_data)) {
		pr_err("%s:%d peer session is still not ready!\n", __func__,
		       __LINE__);
		return TMEM_MGR_SESSION_IS_NOT_READY;
	}

	MGR_SESSION_LOCK();

	ret = drv_ops->memory_reclaim(sess_data->peer_data, peer_priv);
	if (ret != 0) {
		pr_err("peer release reg mem failed:%d\n", ret);
		MGR_SESSION_UNLOCK();
		return TMEM_MGR_MEM_REMOVE_FAILED;
	}

	sess_data->mem_pa_start = 0x0ULL;
	sess_data->mem_size = 0x0;

	MGR_SESSION_UNLOCK();
	return TMEM_OK;
}

static int
peer_mgr_session_open_locked(struct trusted_driver_operations *drv_ops,
			     struct trusted_peer_session *sess_data,
			     void *peer_priv)
{
	int ret;

	if (is_session_ready(sess_data)) {
		pr_err("peer session is already opened!\n");
		return TMEM_MGR_SESSION_IS_ALREADY_OPEN;
	}

	MGR_SESSION_LOCK();

	ret = drv_ops->session_open(&sess_data->peer_data, peer_priv);
	if (ret != 0) {
		pr_err("peer open session failed:%d\n", ret);
		MGR_SESSION_UNLOCK();
		return TMEM_MGR_OPEN_SESSION_FAILED;
	}

	pr_debug("peer data is created:%p\n", sess_data->peer_data);

	set_session_ready(sess_data, true);

	MGR_SESSION_UNLOCK();
	return TMEM_OK;
}

static int peer_mgr_session_close_locked(
	bool keep_alive, struct trusted_driver_operations *drv_ops,
	struct trusted_peer_session *sess_data, void *peer_priv)
{
	int ret;

	if (!is_session_ready(sess_data)) {
		pr_err("peer session is already closed!\n");
		return TMEM_MGR_SESSION_IS_ALREADY_CLOSE;
	}

	if (keep_alive) {
		pr_debug("peer session won't close!\n");
		return TMEM_OK;
	}

	MGR_SESSION_LOCK();

	ret = drv_ops->session_close(sess_data->peer_data, peer_priv);
	if (ret != 0) {
		pr_err("peer close session failed:%d\n", ret);
		MGR_SESSION_UNLOCK();
		return TMEM_MGR_CLOSE_SESSION_FAILED;
	}

	set_session_ready(sess_data, false);

	MGR_SESSION_UNLOCK();
	return TMEM_OK;
}


static int peer_mgr_session_invoke_cmd_locked(
	struct trusted_driver_cmd_params *invoke_params,
	struct trusted_driver_operations *drv_ops,
	struct trusted_peer_session *sess_data, void *peer_priv)
{
	int ret;

	if (!is_session_ready(sess_data)) {
		pr_err("%s:%d peer session is still not ready!\n", __func__,
		       __LINE__);
		return TMEM_MGR_SESSION_IS_NOT_READY;
	}

	MGR_SESSION_LOCK();

	ret = drv_ops->invoke_cmd(invoke_params, sess_data->peer_data,
				  peer_priv);
	if (ret != 0) {
		pr_err("peer invoke command failed:%d\n", ret);
		MGR_SESSION_UNLOCK();
		return TMEM_MGR_INVOKE_COMMAND_FAILED;
	}

	MGR_SESSION_UNLOCK();
	return TMEM_OK;
}

struct peer_mgr_desc *create_peer_mgr_desc(void)
{
	struct peer_mgr_desc *t_mgr_desc;

	t_mgr_desc = mld_kmalloc(sizeof(struct peer_mgr_desc), GFP_KERNEL);
	if (INVALID(t_mgr_desc)) {
		pr_err("%s:%d out of memory!\n", __func__, __LINE__);
		return NULL;
	}

	memset(t_mgr_desc, 0x0, sizeof(struct peer_mgr_desc));
	mutex_init(&t_mgr_desc->peer_mgr_data.lock);

	t_mgr_desc->mgr_sess_open = peer_mgr_session_open_locked;
	t_mgr_desc->mgr_sess_close = peer_mgr_session_close_locked;
	t_mgr_desc->mgr_sess_mem_alloc = peer_mgr_chunk_alloc_locked;
	t_mgr_desc->mgr_sess_mem_free = peer_mgr_chunk_free_locked;
	t_mgr_desc->mgr_sess_mem_add = peer_mgr_mem_add_locked;
	t_mgr_desc->mgr_sess_mem_remove = peer_mgr_mem_remove_locked;
	t_mgr_desc->mgr_sess_invoke_cmd = peer_mgr_session_invoke_cmd_locked;

	return t_mgr_desc;
}

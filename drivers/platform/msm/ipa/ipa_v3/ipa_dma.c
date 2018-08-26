/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/msm_ipa.h>
#include <linux/mutex.h>
#include <linux/ipa.h>
#include "linux/msm_gsi.h"
#include <linux/dmapool.h>
#include "ipa_i.h"

#define IPA_DMA_POLLING_MIN_SLEEP_RX 1010
#define IPA_DMA_POLLING_MAX_SLEEP_RX 1050
#define IPA_DMA_SYS_DESC_MAX_FIFO_SZ 0x7FF8
#define IPA_DMA_MAX_PKT_SZ 0xFFFF
#define IPA_DMA_DUMMY_BUFF_SZ 8
#define IPA_DMA_PREFETCH_WA_THRESHOLD 9

#define IPADMA_DRV_NAME "ipa_dma"

#define IPADMA_DBG(fmt, args...) \
	do { \
		pr_debug(IPADMA_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPADMA_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPADMA_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPADMA_ERR(fmt, args...) \
	do { \
		pr_err(IPADMA_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPADMA_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPADMA_FUNC_ENTRY() \
	IPADMA_DBG_LOW("ENTRY\n")

#define IPADMA_FUNC_EXIT() \
	IPADMA_DBG_LOW("EXIT\n")

#ifdef CONFIG_DEBUG_FS
#define IPADMA_MAX_MSG_LEN 1024
static char dbg_buff[IPADMA_MAX_MSG_LEN];
static void ipa3_dma_debugfs_init(void);
static void ipa3_dma_debugfs_destroy(void);
#else
static void ipa3_dma_debugfs_init(void) {}
static void ipa3_dma_debugfs_destroy(void) {}
#endif

/**
 * struct ipa3_dma_ctx -IPADMA driver context information
 * @enable_ref_cnt: ipa dma enable reference count
 * @destroy_pending: destroy ipa_dma after handling all pending memcpy
 * @ipa_dma_xfer_wrapper_cache: cache of ipa3_dma_xfer_wrapper structs
 * @sync_lock: lock for synchronisation in sync_memcpy
 * @async_lock: lock for synchronisation in async_memcpy
 * @enable_lock: lock for is_enabled
 * @pending_lock: lock for synchronize is_enable and pending_cnt
 * @done: no pending works-ipadma can be destroyed
 * @ipa_dma_sync_prod_hdl: handle of sync memcpy producer
 * @ipa_dma_async_prod_hdl:handle of async memcpy producer
 * @ipa_dma_sync_cons_hdl: handle of sync memcpy consumer
 * @sync_memcpy_pending_cnt: number of pending sync memcopy operations
 * @async_memcpy_pending_cnt: number of pending async memcopy operations
 * @uc_memcpy_pending_cnt: number of pending uc memcopy operations
 * @total_sync_memcpy: total number of sync memcpy (statistics)
 * @total_async_memcpy: total number of async memcpy (statistics)
 * @total_uc_memcpy: total number of uc memcpy (statistics)
 */
struct ipa3_dma_ctx {
	unsigned int enable_ref_cnt;
	bool destroy_pending;
	struct kmem_cache *ipa_dma_xfer_wrapper_cache;
	struct mutex sync_lock;
	spinlock_t async_lock;
	struct mutex enable_lock;
	spinlock_t pending_lock;
	struct completion done;
	u32 ipa_dma_sync_prod_hdl;
	u32 ipa_dma_async_prod_hdl;
	u32 ipa_dma_sync_cons_hdl;
	u32 ipa_dma_async_cons_hdl;
	atomic_t sync_memcpy_pending_cnt;
	atomic_t async_memcpy_pending_cnt;
	atomic_t uc_memcpy_pending_cnt;
	atomic_t total_sync_memcpy;
	atomic_t total_async_memcpy;
	atomic_t total_uc_memcpy;
	struct ipa_mem_buffer ipa_dma_dummy_src_sync;
	struct ipa_mem_buffer ipa_dma_dummy_dst_sync;
	struct ipa_mem_buffer ipa_dma_dummy_src_async;
	struct ipa_mem_buffer ipa_dma_dummy_dst_async;
};
static struct ipa3_dma_ctx *ipa3_dma_ctx;

/**
 * struct ipa3_dma_init_refcnt_ctrl -IPADMA driver init control information
 * @ref_cnt: reference count for initialization operations
 * @lock: lock for the reference count
 */
struct ipa3_dma_init_refcnt_ctrl {
	unsigned int ref_cnt;
	struct mutex lock;
};
static struct ipa3_dma_init_refcnt_ctrl *ipa3_dma_init_refcnt_ctrl;

/**
 * ipa3_dma_setup() - One time setup for IPA DMA
 *
 * This function should be called once to setup ipa dma
 *  by creating the init reference count controller
 *
 * Return codes: 0: success
 *		 Negative value: failure
 */
int ipa3_dma_setup(void)
{
	IPADMA_FUNC_ENTRY();

	if (ipa3_dma_init_refcnt_ctrl) {
		IPADMA_ERR("Setup already done\n");
		return -EFAULT;
	}

	ipa3_dma_init_refcnt_ctrl =
		kzalloc(sizeof(*(ipa3_dma_init_refcnt_ctrl)), GFP_KERNEL);

	if (!ipa3_dma_init_refcnt_ctrl) {
		IPADMA_ERR("kzalloc error.\n");
		return -ENOMEM;
	}

	mutex_init(&ipa3_dma_init_refcnt_ctrl->lock);

	IPADMA_FUNC_EXIT();
	return 0;
}

/**
 * ipa3_dma_shutdown() - Clear setup operations.
 *
 * Cleanup for the setup function.
 * Should be called during IPA driver unloading.
 * It assumes all ipa_dma operations are done and ipa_dma is destroyed.
 *
 * Return codes: None.
 */
void ipa3_dma_shutdown(void)
{
	IPADMA_FUNC_ENTRY();

	if (!ipa3_dma_init_refcnt_ctrl)
		return;

	kfree(ipa3_dma_init_refcnt_ctrl);
	ipa3_dma_init_refcnt_ctrl = NULL;

	IPADMA_FUNC_EXIT();
}

/**
 * ipa3_dma_init() -Initialize IPADMA.
 *
 * This function initialize all IPADMA internal data and connect in dma:
 *	MEMCPY_DMA_SYNC_PROD ->MEMCPY_DMA_SYNC_CONS
 *	MEMCPY_DMA_ASYNC_PROD->MEMCPY_DMA_SYNC_CONS
 *
 * Can be executed several times (re-entrant)
 *
 * Return codes: 0: success
 *		-EFAULT: Mismatch between context existence and init ref_cnt
 *		-EINVAL: IPA driver is not initialized
 *		-ENOMEM: allocating memory error
 *		-EPERM: pipe connection failed
 */
int ipa3_dma_init(void)
{
	struct ipa3_dma_ctx *ipa_dma_ctx_t;
	struct ipa_sys_connect_params sys_in;
	int res = 0;
	int sync_sz;
	int async_sz;

	IPADMA_FUNC_ENTRY();

	if (!ipa3_dma_init_refcnt_ctrl) {
		IPADMA_ERR("Setup isn't done yet!\n");
		return -EINVAL;
	}

	mutex_lock(&ipa3_dma_init_refcnt_ctrl->lock);
	if (ipa3_dma_init_refcnt_ctrl->ref_cnt > 0) {
		IPADMA_DBG("Already initialized refcnt=%d\n",
			ipa3_dma_init_refcnt_ctrl->ref_cnt);
		if (!ipa3_dma_ctx) {
			IPADMA_ERR("Context missing. refcnt=%d\n",
				ipa3_dma_init_refcnt_ctrl->ref_cnt);
			res = -EFAULT;
		} else {
			ipa3_dma_init_refcnt_ctrl->ref_cnt++;
		}
		goto init_unlock;
	}

	if (ipa3_dma_ctx) {
		IPADMA_ERR("Context already exist\n");
		res = -EFAULT;
		goto init_unlock;
	}

	if (!ipa3_is_ready()) {
		IPADMA_ERR("IPA is not ready yet\n");
		res = -EINVAL;
		goto init_unlock;
	}

	ipa_dma_ctx_t = kzalloc(sizeof(*(ipa3_dma_ctx)), GFP_KERNEL);

	if (!ipa_dma_ctx_t) {
		res = -ENOMEM;
		goto init_unlock;
	}

	ipa_dma_ctx_t->ipa_dma_xfer_wrapper_cache =
		kmem_cache_create("IPA DMA XFER WRAPPER",
			sizeof(struct ipa3_dma_xfer_wrapper), 0, 0, NULL);
	if (!ipa_dma_ctx_t->ipa_dma_xfer_wrapper_cache) {
		IPAERR(":failed to create ipa dma xfer wrapper cache.\n");
		res = -ENOMEM;
		goto fail_mem_ctrl;
	}

	mutex_init(&ipa_dma_ctx_t->enable_lock);
	spin_lock_init(&ipa_dma_ctx_t->async_lock);
	mutex_init(&ipa_dma_ctx_t->sync_lock);
	spin_lock_init(&ipa_dma_ctx_t->pending_lock);
	init_completion(&ipa_dma_ctx_t->done);
	ipa_dma_ctx_t->enable_ref_cnt = 0;
	ipa_dma_ctx_t->destroy_pending = false;
	atomic_set(&ipa_dma_ctx_t->async_memcpy_pending_cnt, 0);
	atomic_set(&ipa_dma_ctx_t->sync_memcpy_pending_cnt, 0);
	atomic_set(&ipa_dma_ctx_t->uc_memcpy_pending_cnt, 0);
	atomic_set(&ipa_dma_ctx_t->total_async_memcpy, 0);
	atomic_set(&ipa_dma_ctx_t->total_sync_memcpy, 0);
	atomic_set(&ipa_dma_ctx_t->total_uc_memcpy, 0);

	sync_sz = IPA_SYS_DESC_FIFO_SZ;
	async_sz = IPA_DMA_SYS_DESC_MAX_FIFO_SZ;
	/*
	 * for ipav3.5 we need to double the rings and allocate dummy buffers
	 * in order to apply the prefetch WA
	 */
	if (ipa_get_hw_type() == IPA_HW_v3_5) {
		sync_sz *= 2;
		async_sz *= 2;

		ipa_dma_ctx_t->ipa_dma_dummy_src_sync.base =
			dma_alloc_coherent(ipa3_ctx->pdev,
			IPA_DMA_DUMMY_BUFF_SZ * 4,
			&ipa_dma_ctx_t->ipa_dma_dummy_src_sync.phys_base,
			GFP_KERNEL);

		if (!ipa_dma_ctx_t->ipa_dma_dummy_src_sync.base) {
			IPAERR("DMA alloc fail %d bytes for prefetch WA\n",
				IPA_DMA_DUMMY_BUFF_SZ);
			res = -ENOMEM;
			goto fail_alloc_dummy;
		}

		ipa_dma_ctx_t->ipa_dma_dummy_dst_sync.base =
			ipa_dma_ctx_t->ipa_dma_dummy_src_sync.base +
			IPA_DMA_DUMMY_BUFF_SZ;
		ipa_dma_ctx_t->ipa_dma_dummy_dst_sync.phys_base =
			ipa_dma_ctx_t->ipa_dma_dummy_src_sync.phys_base +
			IPA_DMA_DUMMY_BUFF_SZ;
		ipa_dma_ctx_t->ipa_dma_dummy_src_async.base =
			ipa_dma_ctx_t->ipa_dma_dummy_dst_sync.base +
			IPA_DMA_DUMMY_BUFF_SZ;
		ipa_dma_ctx_t->ipa_dma_dummy_src_async.phys_base =
			ipa_dma_ctx_t->ipa_dma_dummy_dst_sync.phys_base +
			IPA_DMA_DUMMY_BUFF_SZ;
		ipa_dma_ctx_t->ipa_dma_dummy_dst_async.base =
			ipa_dma_ctx_t->ipa_dma_dummy_src_async.base +
			IPA_DMA_DUMMY_BUFF_SZ;
		ipa_dma_ctx_t->ipa_dma_dummy_dst_async.phys_base =
			ipa_dma_ctx_t->ipa_dma_dummy_src_async.phys_base +
			IPA_DMA_DUMMY_BUFF_SZ;
	}

	/* IPADMA SYNC PROD-source for sync memcpy */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_MEMCPY_DMA_SYNC_PROD;
	sys_in.desc_fifo_sz = sync_sz;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_MEMCPY_DMA_SYNC_CONS;
	sys_in.skip_ep_cfg = false;
	if (ipa3_setup_sys_pipe(&sys_in,
		&ipa_dma_ctx_t->ipa_dma_sync_prod_hdl)) {
		IPADMA_ERR(":setup sync prod pipe failed\n");
		res = -EPERM;
		goto fail_sync_prod;
	}

	/* IPADMA SYNC CONS-destination for sync memcpy */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_MEMCPY_DMA_SYNC_CONS;
	sys_in.desc_fifo_sz = sync_sz;
	sys_in.skip_ep_cfg = false;
	sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
	sys_in.notify = NULL;
	sys_in.priv = NULL;
	if (ipa3_setup_sys_pipe(&sys_in,
		&ipa_dma_ctx_t->ipa_dma_sync_cons_hdl)) {
		IPADMA_ERR(":setup sync cons pipe failed.\n");
		res = -EPERM;
		goto fail_sync_cons;
	}

	IPADMA_DBG("SYNC MEMCPY pipes are connected\n");

	/* IPADMA ASYNC PROD-source for sync memcpy */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD;
	sys_in.desc_fifo_sz = async_sz;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS;
	sys_in.skip_ep_cfg = false;
	sys_in.notify = NULL;
	if (ipa3_setup_sys_pipe(&sys_in,
		&ipa_dma_ctx_t->ipa_dma_async_prod_hdl)) {
		IPADMA_ERR(":setup async prod pipe failed.\n");
		res = -EPERM;
		goto fail_async_prod;
	}

	/* IPADMA ASYNC CONS-destination for sync memcpy */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS;
	sys_in.desc_fifo_sz = async_sz;
	sys_in.skip_ep_cfg = false;
	sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
	sys_in.notify = ipa3_dma_async_memcpy_notify_cb;
	sys_in.priv = NULL;
	if (ipa3_setup_sys_pipe(&sys_in,
		&ipa_dma_ctx_t->ipa_dma_async_cons_hdl)) {
		IPADMA_ERR(":setup async cons pipe failed.\n");
		res = -EPERM;
		goto fail_async_cons;
	}
	ipa3_dma_debugfs_init();
	ipa3_dma_ctx = ipa_dma_ctx_t;
	ipa3_dma_init_refcnt_ctrl->ref_cnt = 1;
	IPADMA_DBG("ASYNC MEMCPY pipes are connected\n");

	IPADMA_FUNC_EXIT();
	goto init_unlock;

fail_async_cons:
	ipa3_teardown_sys_pipe(ipa_dma_ctx_t->ipa_dma_async_prod_hdl);
fail_async_prod:
	ipa3_teardown_sys_pipe(ipa_dma_ctx_t->ipa_dma_sync_cons_hdl);
fail_sync_cons:
	ipa3_teardown_sys_pipe(ipa_dma_ctx_t->ipa_dma_sync_prod_hdl);
fail_sync_prod:
	dma_free_coherent(ipa3_ctx->pdev, IPA_DMA_DUMMY_BUFF_SZ * 4,
		ipa_dma_ctx_t->ipa_dma_dummy_src_sync.base,
		ipa_dma_ctx_t->ipa_dma_dummy_src_sync.phys_base);
fail_alloc_dummy:
	kmem_cache_destroy(ipa_dma_ctx_t->ipa_dma_xfer_wrapper_cache);
fail_mem_ctrl:
	kfree(ipa_dma_ctx_t);
	ipa3_dma_ctx = NULL;
init_unlock:
	mutex_unlock(&ipa3_dma_init_refcnt_ctrl->lock);
	return res;

}

/**
 * ipa3_dma_enable() -Vote for IPA clocks.
 *
 * Can be executed several times (re-entrant)
 *
 *Return codes: 0: success
 *		-EINVAL: IPADMA is not initialized
 */
int ipa3_dma_enable(void)
{
	IPADMA_FUNC_ENTRY();
	if ((ipa3_dma_ctx == NULL) ||
		(ipa3_dma_init_refcnt_ctrl->ref_cnt < 1)) {
		IPADMA_ERR("IPADMA isn't initialized, can't enable\n");
		return -EINVAL;
	}
	mutex_lock(&ipa3_dma_ctx->enable_lock);
	if (ipa3_dma_ctx->enable_ref_cnt > 0) {
		IPADMA_ERR("Already enabled refcnt=%d\n",
			ipa3_dma_ctx->enable_ref_cnt);
		ipa3_dma_ctx->enable_ref_cnt++;
		mutex_unlock(&ipa3_dma_ctx->enable_lock);
		return 0;
	}
	IPA_ACTIVE_CLIENTS_INC_SPECIAL("DMA");
	ipa3_dma_ctx->enable_ref_cnt = 1;
	mutex_unlock(&ipa3_dma_ctx->enable_lock);

	IPADMA_FUNC_EXIT();
	return 0;
}

static bool ipa3_dma_work_pending(void)
{
	if (atomic_read(&ipa3_dma_ctx->sync_memcpy_pending_cnt)) {
		IPADMA_DBG("pending sync\n");
		return true;
	}
	if (atomic_read(&ipa3_dma_ctx->async_memcpy_pending_cnt)) {
		IPADMA_DBG("pending async\n");
		return true;
	}
	if (atomic_read(&ipa3_dma_ctx->uc_memcpy_pending_cnt)) {
		IPADMA_DBG("pending uc\n");
		return true;
	}
	IPADMA_DBG_LOW("no pending work\n");
	return false;
}

/**
 * ipa3_dma_disable()- Unvote for IPA clocks.
 *
 * enter to power save mode.
 *
 * Return codes: 0: success
 *		-EINVAL: IPADMA is not initialized
 *		-EPERM: Operation not permitted as ipa_dma is already
 *			diabled
 *		-EFAULT: can not disable ipa_dma as there are pending
 *			memcopy works
 */
int ipa3_dma_disable(void)
{
	unsigned long flags;
	int res = 0;
	bool dec_clks = false;

	IPADMA_FUNC_ENTRY();
	if ((ipa3_dma_ctx == NULL) ||
		(ipa3_dma_init_refcnt_ctrl->ref_cnt < 1)) {
		IPADMA_ERR("IPADMA isn't initialized, can't disable\n");
		return -EINVAL;
	}
	mutex_lock(&ipa3_dma_ctx->enable_lock);
	spin_lock_irqsave(&ipa3_dma_ctx->pending_lock, flags);
	if (ipa3_dma_ctx->enable_ref_cnt > 1) {
		IPADMA_DBG("Multiple enablement done. refcnt=%d\n",
			ipa3_dma_ctx->enable_ref_cnt);
		ipa3_dma_ctx->enable_ref_cnt--;
		goto completed;
	}

	if (ipa3_dma_ctx->enable_ref_cnt == 0) {
		IPADMA_ERR("Already disabled\n");
		res = -EPERM;
		goto completed;
	}

	if (ipa3_dma_work_pending()) {
		IPADMA_ERR("There is pending work, can't disable.\n");
		res = -EFAULT;
		goto completed;
	}
	ipa3_dma_ctx->enable_ref_cnt = 0;
	dec_clks = true;
	IPADMA_FUNC_EXIT();

completed:
	spin_unlock_irqrestore(&ipa3_dma_ctx->pending_lock, flags);
	if (dec_clks)
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL("DMA");
	mutex_unlock(&ipa3_dma_ctx->enable_lock);
	return res;
}

/**
 * ipa3_dma_sync_memcpy()- Perform synchronous memcpy using IPA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-gsi_status : on GSI failures
 *		-EFAULT: other
 */
int ipa3_dma_sync_memcpy(u64 dest, u64 src, int len)
{
	int ep_idx;
	int res;
	int i = 0;
	struct ipa3_sys_context *cons_sys;
	struct ipa3_sys_context *prod_sys;
	struct ipa3_dma_xfer_wrapper *xfer_descr = NULL;
	struct ipa3_dma_xfer_wrapper *head_descr = NULL;
	struct gsi_xfer_elem prod_xfer_elem;
	struct gsi_xfer_elem cons_xfer_elem;
	struct gsi_chan_xfer_notify gsi_notify;
	unsigned long flags;
	bool stop_polling = false;
	bool prefetch_wa = false;

	IPADMA_FUNC_ENTRY();
	IPADMA_DBG_LOW("dest =  0x%llx, src = 0x%llx, len = %d\n",
		dest, src, len);
	if (ipa3_dma_ctx == NULL) {
		IPADMA_ERR("IPADMA isn't initialized, can't memcpy\n");
		return -EPERM;
	}
	if ((max(src, dest) - min(src, dest)) < len) {
		IPADMA_ERR("invalid addresses - overlapping buffers\n");
		return -EINVAL;
	}
	if (len > IPA_DMA_MAX_PKT_SZ || len <= 0) {
		IPADMA_ERR("invalid len, %d\n", len);
		return	-EINVAL;
	}
	spin_lock_irqsave(&ipa3_dma_ctx->pending_lock, flags);
	if (!ipa3_dma_ctx->enable_ref_cnt) {
		IPADMA_ERR("can't memcpy, IPADMA isn't enabled\n");
		spin_unlock_irqrestore(&ipa3_dma_ctx->pending_lock, flags);
		return -EPERM;
	}
	atomic_inc(&ipa3_dma_ctx->sync_memcpy_pending_cnt);
	spin_unlock_irqrestore(&ipa3_dma_ctx->pending_lock, flags);

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_SYNC_CONS);
	if (-1 == ep_idx) {
		IPADMA_ERR("Client %u is not mapped\n",
			IPA_CLIENT_MEMCPY_DMA_SYNC_CONS);
		return -EFAULT;
	}
	cons_sys = ipa3_ctx->ep[ep_idx].sys;

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_SYNC_PROD);
	if (-1 == ep_idx) {
		IPADMA_ERR("Client %u is not mapped\n",
			IPA_CLIENT_MEMCPY_DMA_SYNC_PROD);
		return -EFAULT;
	}
	prod_sys = ipa3_ctx->ep[ep_idx].sys;

	xfer_descr = kmem_cache_zalloc(ipa3_dma_ctx->ipa_dma_xfer_wrapper_cache,
					GFP_KERNEL);
	if (!xfer_descr) {
		IPADMA_ERR("failed to alloc xfer descr wrapper\n");
		res = -ENOMEM;
		goto fail_mem_alloc;
	}
	xfer_descr->phys_addr_dest = dest;
	xfer_descr->phys_addr_src = src;
	xfer_descr->len = len;
	init_completion(&xfer_descr->xfer_done);

	mutex_lock(&ipa3_dma_ctx->sync_lock);
	list_add_tail(&xfer_descr->link, &cons_sys->head_desc_list);
	cons_sys->len++;
	cons_xfer_elem.addr = dest;
	cons_xfer_elem.len = len;
	cons_xfer_elem.type = GSI_XFER_ELEM_DATA;
	cons_xfer_elem.flags = GSI_XFER_FLAG_EOT;

	prod_xfer_elem.addr = src;
	prod_xfer_elem.len = len;
	prod_xfer_elem.type = GSI_XFER_ELEM_DATA;
	prod_xfer_elem.xfer_user_data = NULL;

	/*
	 * when copy is less than 9B we need to chain another dummy
	 * copy so the total size will be larger (for ipav3.5)
	 * for the consumer we have to prepare an additional credit
	 */
	prefetch_wa = ((ipa_get_hw_type() == IPA_HW_v3_5) &&
		len < IPA_DMA_PREFETCH_WA_THRESHOLD);
	if (prefetch_wa) {
		cons_xfer_elem.xfer_user_data = NULL;
		res = gsi_queue_xfer(cons_sys->ep->gsi_chan_hdl, 1,
			&cons_xfer_elem, false);
		if (res) {
			IPADMA_ERR(
				"Failed: gsi_queue_xfer dest descr res:%d\n",
				res);
			goto fail_send;
		}
		cons_xfer_elem.addr =
			ipa3_dma_ctx->ipa_dma_dummy_dst_sync.phys_base;
		cons_xfer_elem.len = IPA_DMA_DUMMY_BUFF_SZ;
		cons_xfer_elem.type = GSI_XFER_ELEM_DATA;
		cons_xfer_elem.flags = GSI_XFER_FLAG_EOT;
		cons_xfer_elem.xfer_user_data = xfer_descr;
		res = gsi_queue_xfer(cons_sys->ep->gsi_chan_hdl, 1,
			&cons_xfer_elem, true);
		if (res) {
			IPADMA_ERR(
				"Failed: gsi_queue_xfer dummy dest descr res:%d\n",
				res);
			goto fail_send;
		}
		prod_xfer_elem.flags = GSI_XFER_FLAG_CHAIN;
		res = gsi_queue_xfer(prod_sys->ep->gsi_chan_hdl, 1,
			&prod_xfer_elem, false);
		if (res) {
			IPADMA_ERR(
				"Failed: gsi_queue_xfer src descr res:%d\n",
				res);
			ipa_assert();
			goto fail_send;
		}
		prod_xfer_elem.addr =
			ipa3_dma_ctx->ipa_dma_dummy_src_sync.phys_base;
		prod_xfer_elem.len = IPA_DMA_DUMMY_BUFF_SZ;
		prod_xfer_elem.type = GSI_XFER_ELEM_DATA;
		prod_xfer_elem.flags = GSI_XFER_FLAG_EOT;
		prod_xfer_elem.xfer_user_data = NULL;
		res = gsi_queue_xfer(prod_sys->ep->gsi_chan_hdl, 1,
				&prod_xfer_elem, true);
		if (res) {
			IPADMA_ERR(
					"Failed: gsi_queue_xfer dummy src descr res:%d\n",
					res);
				ipa_assert();
				goto fail_send;
			}
	} else {
		cons_xfer_elem.xfer_user_data = xfer_descr;
		res = gsi_queue_xfer(cons_sys->ep->gsi_chan_hdl, 1,
			&cons_xfer_elem, true);
		if (res) {
			IPADMA_ERR(
				"Failed: gsi_queue_xfer dest descr res:%d\n",
				res);
			goto fail_send;
		}
		prod_xfer_elem.flags = GSI_XFER_FLAG_EOT;
		res = gsi_queue_xfer(prod_sys->ep->gsi_chan_hdl, 1,
			&prod_xfer_elem, true);
		if (res) {
			IPADMA_ERR(
			"Failed: gsi_queue_xfer src descr res:%d\n",
			 res);
			ipa_assert();
			goto fail_send;
		}
	}
	head_descr = list_first_entry(&cons_sys->head_desc_list,
				struct ipa3_dma_xfer_wrapper, link);

	/* in case we are not the head of the list, wait for head to wake us */
	if (xfer_descr != head_descr) {
		mutex_unlock(&ipa3_dma_ctx->sync_lock);
		wait_for_completion(&xfer_descr->xfer_done);
		mutex_lock(&ipa3_dma_ctx->sync_lock);
		head_descr = list_first_entry(&cons_sys->head_desc_list,
					struct ipa3_dma_xfer_wrapper, link);
		/* Unexpected transfer sent from HW */
		BUG_ON(xfer_descr != head_descr);
	}
	mutex_unlock(&ipa3_dma_ctx->sync_lock);

	do {
		/* wait for transfer to complete */
		res = gsi_poll_channel(cons_sys->ep->gsi_chan_hdl,
			&gsi_notify);
		if (res == GSI_STATUS_SUCCESS)
			stop_polling = true;
		else if (res != GSI_STATUS_POLL_EMPTY)
			IPADMA_ERR(
				"Failed: gsi_poll_chanel, returned %d loop#:%d\n",
				res, i);
		usleep_range(IPA_DMA_POLLING_MIN_SLEEP_RX,
			IPA_DMA_POLLING_MAX_SLEEP_RX);
		i++;
	} while (!stop_polling);

	/* for prefetch WA we will receive the length of the dummy
	 * transfer in the event (because it is the second element)
	 */
	if (prefetch_wa)
		ipa_assert_on(gsi_notify.bytes_xfered !=
			IPA_DMA_DUMMY_BUFF_SZ);
	else
		ipa_assert_on(len != gsi_notify.bytes_xfered);

	ipa_assert_on(dest != ((struct ipa3_dma_xfer_wrapper *)
			(gsi_notify.xfer_user_data))->phys_addr_dest);

	mutex_lock(&ipa3_dma_ctx->sync_lock);
	list_del(&head_descr->link);
	cons_sys->len--;
	kmem_cache_free(ipa3_dma_ctx->ipa_dma_xfer_wrapper_cache, xfer_descr);
	/* wake the head of the list */
	if (!list_empty(&cons_sys->head_desc_list)) {
		head_descr = list_first_entry(&cons_sys->head_desc_list,
				struct ipa3_dma_xfer_wrapper, link);
		complete(&head_descr->xfer_done);
	}
	mutex_unlock(&ipa3_dma_ctx->sync_lock);

	atomic_inc(&ipa3_dma_ctx->total_sync_memcpy);
	atomic_dec(&ipa3_dma_ctx->sync_memcpy_pending_cnt);
	if (ipa3_dma_ctx->destroy_pending && !ipa3_dma_work_pending())
		complete(&ipa3_dma_ctx->done);

	IPADMA_FUNC_EXIT();
	return res;

fail_send:
	list_del(&xfer_descr->link);
	cons_sys->len--;
	mutex_unlock(&ipa3_dma_ctx->sync_lock);
	kmem_cache_free(ipa3_dma_ctx->ipa_dma_xfer_wrapper_cache, xfer_descr);
fail_mem_alloc:
	atomic_dec(&ipa3_dma_ctx->sync_memcpy_pending_cnt);
	if (ipa3_dma_ctx->destroy_pending && !ipa3_dma_work_pending())
		complete(&ipa3_dma_ctx->done);
	return res;
}

/**
 * ipa3_dma_async_memcpy()- Perform asynchronous memcpy using IPA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 * @user_cb: callback function to notify the client when the copy was done.
 * @user_param: cookie for user_cb.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-gsi_status : on GSI failures
 *		-EFAULT: descr fifo is full.
 */
int ipa3_dma_async_memcpy(u64 dest, u64 src, int len,
		void (*user_cb)(void *user1), void *user_param)
{
	int ep_idx;
	int res = 0;
	struct ipa3_dma_xfer_wrapper *xfer_descr = NULL;
	struct ipa3_sys_context *prod_sys;
	struct ipa3_sys_context *cons_sys;
	struct gsi_xfer_elem xfer_elem_cons, xfer_elem_prod;
	unsigned long flags;

	IPADMA_FUNC_ENTRY();
	IPADMA_DBG_LOW("dest =  0x%llx, src = 0x%llx, len = %d\n",
		dest, src, len);
	if (ipa3_dma_ctx == NULL) {
		IPADMA_ERR("IPADMA isn't initialized, can't memcpy\n");
		return -EPERM;
	}
	if ((max(src, dest) - min(src, dest)) < len) {
		IPADMA_ERR("invalid addresses - overlapping buffers\n");
		return -EINVAL;
	}
	if (len > IPA_DMA_MAX_PKT_SZ || len <= 0) {
		IPADMA_ERR("invalid len, %d\n", len);
		return	-EINVAL;
	}
	if (!user_cb) {
		IPADMA_ERR("null pointer: user_cb\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&ipa3_dma_ctx->pending_lock, flags);
	if (!ipa3_dma_ctx->enable_ref_cnt) {
		IPADMA_ERR("can't memcpy, IPA_DMA isn't enabled\n");
		spin_unlock_irqrestore(&ipa3_dma_ctx->pending_lock, flags);
		return -EPERM;
	}
	atomic_inc(&ipa3_dma_ctx->async_memcpy_pending_cnt);
	spin_unlock_irqrestore(&ipa3_dma_ctx->pending_lock, flags);

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS);
	if (-1 == ep_idx) {
		IPADMA_ERR("Client %u is not mapped\n",
			IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS);
		return -EFAULT;
	}
	cons_sys = ipa3_ctx->ep[ep_idx].sys;

	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD);
	if (-1 == ep_idx) {
		IPADMA_ERR("Client %u is not mapped\n",
			IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD);
		return -EFAULT;
	}
	prod_sys = ipa3_ctx->ep[ep_idx].sys;

	xfer_descr = kmem_cache_zalloc(ipa3_dma_ctx->ipa_dma_xfer_wrapper_cache,
					GFP_KERNEL);
	if (!xfer_descr) {
		res = -ENOMEM;
		goto fail_mem_alloc;
	}
	xfer_descr->phys_addr_dest = dest;
	xfer_descr->phys_addr_src = src;
	xfer_descr->len = len;
	xfer_descr->callback = user_cb;
	xfer_descr->user1 = user_param;

	spin_lock_irqsave(&ipa3_dma_ctx->async_lock, flags);
	list_add_tail(&xfer_descr->link, &cons_sys->head_desc_list);
	cons_sys->len++;
	/*
	 * when copy is less than 9B we need to chain another dummy
	 * copy so the total size will be larger (for ipav3.5)
	 */
	if ((ipa_get_hw_type() == IPA_HW_v3_5) && len <
		IPA_DMA_PREFETCH_WA_THRESHOLD) {
		xfer_elem_cons.addr = dest;
		xfer_elem_cons.len = len;
		xfer_elem_cons.type = GSI_XFER_ELEM_DATA;
		xfer_elem_cons.flags = GSI_XFER_FLAG_EOT;
		xfer_elem_cons.xfer_user_data = NULL;
		res = gsi_queue_xfer(cons_sys->ep->gsi_chan_hdl, 1,
			&xfer_elem_cons, false);
		if (res) {
			IPADMA_ERR(
				"Failed: gsi_queue_xfer on dest descr res: %d\n",
				res);
			goto fail_send;
		}
		xfer_elem_cons.addr =
			ipa3_dma_ctx->ipa_dma_dummy_dst_async.phys_base;
		xfer_elem_cons.len = IPA_DMA_DUMMY_BUFF_SZ;
		xfer_elem_cons.type = GSI_XFER_ELEM_DATA;
		xfer_elem_cons.flags = GSI_XFER_FLAG_EOT;
		xfer_elem_cons.xfer_user_data = xfer_descr;
		res = gsi_queue_xfer(cons_sys->ep->gsi_chan_hdl, 1,
			&xfer_elem_cons, true);
		if (res) {
			IPADMA_ERR(
				"Failed: gsi_queue_xfer on dummy dest descr res: %d\n",
				res);
			goto fail_send;
		}

		xfer_elem_prod.addr = src;
		xfer_elem_prod.len = len;
		xfer_elem_prod.type = GSI_XFER_ELEM_DATA;
		xfer_elem_prod.flags = GSI_XFER_FLAG_CHAIN;
		xfer_elem_prod.xfer_user_data = NULL;
		res = gsi_queue_xfer(prod_sys->ep->gsi_chan_hdl, 1,
			&xfer_elem_prod, false);
		if (res) {
			IPADMA_ERR(
				"Failed: gsi_queue_xfer on src descr res: %d\n",
				res);
			ipa_assert();
			goto fail_send;
		}
		xfer_elem_prod.addr =
			ipa3_dma_ctx->ipa_dma_dummy_src_async.phys_base;
		xfer_elem_prod.len = IPA_DMA_DUMMY_BUFF_SZ;
		xfer_elem_prod.type = GSI_XFER_ELEM_DATA;
		xfer_elem_prod.flags = GSI_XFER_FLAG_EOT;
		xfer_elem_prod.xfer_user_data = NULL;
		res = gsi_queue_xfer(prod_sys->ep->gsi_chan_hdl, 1,
			&xfer_elem_prod, true);
		if (res) {
			IPADMA_ERR(
				"Failed: gsi_queue_xfer on dummy src descr res: %d\n",
				res);
			ipa_assert();
			goto fail_send;
		}
	} else {

		xfer_elem_cons.addr = dest;
		xfer_elem_cons.len = len;
		xfer_elem_cons.type = GSI_XFER_ELEM_DATA;
		xfer_elem_cons.flags = GSI_XFER_FLAG_EOT;
		xfer_elem_cons.xfer_user_data = xfer_descr;
		res = gsi_queue_xfer(cons_sys->ep->gsi_chan_hdl, 1,
			&xfer_elem_cons, true);
		if (res) {
			IPADMA_ERR(
					"Failed: gsi_queue_xfer on dummy dest descr res: %d\n",
				res);
				ipa_assert();
			goto fail_send;
		}
		xfer_elem_prod.addr = src;
		xfer_elem_prod.len = len;
		xfer_elem_prod.type = GSI_XFER_ELEM_DATA;
		xfer_elem_prod.flags = GSI_XFER_FLAG_EOT;
		xfer_elem_prod.xfer_user_data = NULL;
		res = gsi_queue_xfer(prod_sys->ep->gsi_chan_hdl, 1,
			&xfer_elem_prod, true);
		if (res) {
			IPADMA_ERR(
					"Failed: gsi_queue_xfer on dummy src descr res: %d\n",
				res);
			ipa_assert();
			goto fail_send;
		}

	}
	spin_unlock_irqrestore(&ipa3_dma_ctx->async_lock, flags);
	IPADMA_FUNC_EXIT();
	return res;

fail_send:
	list_del(&xfer_descr->link);
	spin_unlock_irqrestore(&ipa3_dma_ctx->async_lock, flags);
	kmem_cache_free(ipa3_dma_ctx->ipa_dma_xfer_wrapper_cache, xfer_descr);
fail_mem_alloc:
	atomic_dec(&ipa3_dma_ctx->async_memcpy_pending_cnt);
	if (ipa3_dma_ctx->destroy_pending && !ipa3_dma_work_pending())
		complete(&ipa3_dma_ctx->done);
	return res;
}

/**
 * ipa3_dma_uc_memcpy() - Perform a memcpy action using IPA uC
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-EBADF: IPA uC is not loaded
 */
int ipa3_dma_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len)
{
	int res;
	unsigned long flags;

	IPADMA_FUNC_ENTRY();
	if (ipa3_dma_ctx == NULL) {
		IPADMA_ERR("IPADMA isn't initialized, can't memcpy\n");
		return -EPERM;
	}
	if ((max(src, dest) - min(src, dest)) < len) {
		IPADMA_ERR("invalid addresses - overlapping buffers\n");
		return -EINVAL;
	}
	if (len > IPA_DMA_MAX_PKT_SZ || len <= 0) {
		IPADMA_ERR("invalid len, %d\n", len);
		return	-EINVAL;
	}

	spin_lock_irqsave(&ipa3_dma_ctx->pending_lock, flags);
	if (!ipa3_dma_ctx->enable_ref_cnt) {
		IPADMA_ERR("can't memcpy, IPADMA isn't enabled\n");
		spin_unlock_irqrestore(&ipa3_dma_ctx->pending_lock, flags);
		return -EPERM;
	}
	atomic_inc(&ipa3_dma_ctx->uc_memcpy_pending_cnt);
	spin_unlock_irqrestore(&ipa3_dma_ctx->pending_lock, flags);

	res = ipa3_uc_memcpy(dest, src, len);
	if (res) {
		IPADMA_ERR("ipa3_uc_memcpy failed %d\n", res);
		goto dec_and_exit;
	}

	atomic_inc(&ipa3_dma_ctx->total_uc_memcpy);
	res = 0;
dec_and_exit:
	atomic_dec(&ipa3_dma_ctx->uc_memcpy_pending_cnt);
	if (ipa3_dma_ctx->destroy_pending && !ipa3_dma_work_pending())
		complete(&ipa3_dma_ctx->done);
	IPADMA_FUNC_EXIT();
	return res;
}

/**
 * ipa3_dma_destroy() -teardown IPADMA pipes and release ipadma.
 *
 * this is a blocking function, returns just after destroying IPADMA.
 */
void ipa3_dma_destroy(void)
{
	int res = 0;

	IPADMA_FUNC_ENTRY();

	if (!ipa3_dma_init_refcnt_ctrl) {
		IPADMA_ERR("Setup isn't done\n");
		return;
	}

	mutex_lock(&ipa3_dma_init_refcnt_ctrl->lock);
	if (ipa3_dma_init_refcnt_ctrl->ref_cnt > 1) {
		IPADMA_DBG("Multiple initialization done. refcnt=%d\n",
			ipa3_dma_init_refcnt_ctrl->ref_cnt);
		ipa3_dma_init_refcnt_ctrl->ref_cnt--;
		goto completed;
	}

	if ((!ipa3_dma_ctx) || (ipa3_dma_init_refcnt_ctrl->ref_cnt == 0)) {
		IPADMA_ERR("IPADMA isn't initialized ctx=%pK\n", ipa3_dma_ctx);
		goto completed;
	}

	if (ipa3_dma_work_pending()) {
		ipa3_dma_ctx->destroy_pending = true;
		IPADMA_DBG("There are pending memcpy, wait for completion\n");
		wait_for_completion(&ipa3_dma_ctx->done);
	}

	if (ipa3_dma_ctx->enable_ref_cnt > 0) {
		IPADMA_ERR("IPADMA still enabled\n");
		goto completed;
	}

	res = ipa3_teardown_sys_pipe(ipa3_dma_ctx->ipa_dma_async_cons_hdl);
	if (res)
		IPADMA_ERR("teardown IPADMA ASYNC CONS failed\n");
	ipa3_dma_ctx->ipa_dma_async_cons_hdl = 0;
	res = ipa3_teardown_sys_pipe(ipa3_dma_ctx->ipa_dma_sync_cons_hdl);
	if (res)
		IPADMA_ERR("teardown IPADMA SYNC CONS failed\n");
	ipa3_dma_ctx->ipa_dma_sync_cons_hdl = 0;
	res = ipa3_teardown_sys_pipe(ipa3_dma_ctx->ipa_dma_async_prod_hdl);
	if (res)
		IPADMA_ERR("teardown IPADMA ASYNC PROD failed\n");
	ipa3_dma_ctx->ipa_dma_async_prod_hdl = 0;
	res = ipa3_teardown_sys_pipe(ipa3_dma_ctx->ipa_dma_sync_prod_hdl);
	if (res)
		IPADMA_ERR("teardown IPADMA SYNC PROD failed\n");
	ipa3_dma_ctx->ipa_dma_sync_prod_hdl = 0;

	ipa3_dma_debugfs_destroy();
	kmem_cache_destroy(ipa3_dma_ctx->ipa_dma_xfer_wrapper_cache);
	dma_free_coherent(ipa3_ctx->pdev, IPA_DMA_DUMMY_BUFF_SZ * 4,
		ipa3_dma_ctx->ipa_dma_dummy_src_sync.base,
		ipa3_dma_ctx->ipa_dma_dummy_src_sync.phys_base);
	kfree(ipa3_dma_ctx);
	ipa3_dma_ctx = NULL;

	ipa3_dma_init_refcnt_ctrl->ref_cnt = 0;
	IPADMA_FUNC_EXIT();

completed:
	mutex_unlock(&ipa3_dma_init_refcnt_ctrl->lock);
}

/**
 * ipa3_dma_async_memcpy_notify_cb() - Callback function which will be called
 * by IPA driver after getting notify on Rx operation is completed (data was
 * written to dest descriptor on async_cons ep).
 *
 * @priv -not in use.
 * @evt - event name - IPA_RECIVE.
 * @data -the ipa_mem_buffer.
 */
void ipa3_dma_async_memcpy_notify_cb(void *priv
			, enum ipa_dp_evt_type evt, unsigned long data)
{
	int ep_idx = 0;
	struct ipa3_dma_xfer_wrapper *xfer_descr_expected;
	struct ipa3_sys_context *sys;
	unsigned long flags;
	struct ipa_mem_buffer *mem_info;

	IPADMA_FUNC_ENTRY();

	mem_info = (struct ipa_mem_buffer *)data;
	ep_idx = ipa3_get_ep_mapping(IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS);
	if (ep_idx < 0) {
		IPADMA_ERR("IPA Client mapping failed\n");
		return;
	}
	sys = ipa3_ctx->ep[ep_idx].sys;

	spin_lock_irqsave(&ipa3_dma_ctx->async_lock, flags);
	xfer_descr_expected = list_first_entry(&sys->head_desc_list,
				 struct ipa3_dma_xfer_wrapper, link);
	list_del(&xfer_descr_expected->link);
	sys->len--;
	spin_unlock_irqrestore(&ipa3_dma_ctx->async_lock, flags);
	atomic_inc(&ipa3_dma_ctx->total_async_memcpy);
	atomic_dec(&ipa3_dma_ctx->async_memcpy_pending_cnt);
	xfer_descr_expected->callback(xfer_descr_expected->user1);

	kmem_cache_free(ipa3_dma_ctx->ipa_dma_xfer_wrapper_cache,
		xfer_descr_expected);

	if (ipa3_dma_ctx->destroy_pending && !ipa3_dma_work_pending())
		complete(&ipa3_dma_ctx->done);

	IPADMA_FUNC_EXIT();
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent;
static struct dentry *dfile_info;

static ssize_t ipa3_dma_debugfs_read(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	int nbytes = 0;

	if (!ipa3_dma_init_refcnt_ctrl) {
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"Setup was not done\n");
		goto completed;

	}

	if (!ipa3_dma_ctx) {
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"Status:\n	Not initialized (ref_cnt=%d)\n",
			ipa3_dma_init_refcnt_ctrl->ref_cnt);
	} else {
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"Status:\n	Initialized (ref_cnt=%d)\n",
			ipa3_dma_init_refcnt_ctrl->ref_cnt);
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"	%s (ref_cnt=%d)\n",
			(ipa3_dma_ctx->enable_ref_cnt > 0) ?
			"Enabled" : "Disabled",
			ipa3_dma_ctx->enable_ref_cnt);
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"Statistics:\n	total sync memcpy: %d\n	",
			atomic_read(&ipa3_dma_ctx->total_sync_memcpy));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"total async memcpy: %d\n	",
			atomic_read(&ipa3_dma_ctx->total_async_memcpy));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"total uc memcpy: %d\n	",
			atomic_read(&ipa3_dma_ctx->total_uc_memcpy));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"pending sync memcpy jobs: %d\n	",
			atomic_read(&ipa3_dma_ctx->sync_memcpy_pending_cnt));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"pending async memcpy jobs: %d\n	",
			atomic_read(&ipa3_dma_ctx->async_memcpy_pending_cnt));
		nbytes += scnprintf(&dbg_buff[nbytes],
			IPADMA_MAX_MSG_LEN - nbytes,
			"pending uc memcpy jobs: %d\n",
			atomic_read(&ipa3_dma_ctx->uc_memcpy_pending_cnt));
	}

completed:
	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa3_dma_debugfs_reset_statistics(struct file *file,
					const char __user *ubuf,
					size_t count,
					loff_t *ppos)
{
	unsigned long missing;
	s8 in_num = 0;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &in_num))
		return -EFAULT;
	switch (in_num) {
	case 0:
		if (ipa3_dma_work_pending())
			IPADMA_ERR("Note, there are pending memcpy\n");

		atomic_set(&ipa3_dma_ctx->total_async_memcpy, 0);
		atomic_set(&ipa3_dma_ctx->total_sync_memcpy, 0);
		break;
	default:
		IPADMA_ERR("invalid argument: To reset statistics echo 0\n");
		break;
	}
	return count;
}

const struct file_operations ipa3_ipadma_stats_ops = {
	.read = ipa3_dma_debugfs_read,
	.write = ipa3_dma_debugfs_reset_statistics,
};

static void ipa3_dma_debugfs_init(void)
{
	const mode_t read_write_mode = 0666;

	dent = debugfs_create_dir("ipa_dma", 0);
	if (IS_ERR(dent)) {
		IPADMA_ERR("fail to create folder ipa_dma\n");
		return;
	}

	dfile_info =
		debugfs_create_file("info", read_write_mode, dent,
				 0, &ipa3_ipadma_stats_ops);
	if (!dfile_info || IS_ERR(dfile_info)) {
		IPADMA_ERR("fail to create file stats\n");
		goto fail;
	}
	return;
fail:
	debugfs_remove_recursive(dent);
}

static void ipa3_dma_debugfs_destroy(void)
{
	debugfs_remove_recursive(dent);
}

#endif /* !CONFIG_DEBUG_FS */

/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/rmt_storage_client.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <mach/msm_rpcrouter.h>
#ifdef CONFIG_MSM_SDIO_SMEM
#include <mach/sdio_smem.h>
#endif
#include "smd_private.h"

enum {
	RMT_STORAGE_EVNT_OPEN = 0,
	RMT_STORAGE_EVNT_CLOSE,
	RMT_STORAGE_EVNT_WRITE_BLOCK,
	RMT_STORAGE_EVNT_GET_DEV_ERROR,
	RMT_STORAGE_EVNT_WRITE_IOVEC,
	RMT_STORAGE_EVNT_SEND_USER_DATA,
	RMT_STORAGE_EVNT_READ_IOVEC,
	RMT_STORAGE_EVNT_ALLOC_RMT_BUF,
} rmt_storage_event;

struct shared_ramfs_entry {
	uint32_t client_id;	/* Client id to uniquely identify a client */
	uint32_t base_addr;	/* Base address of shared RAMFS memory */
	uint32_t size;		/* Size of the shared RAMFS memory */
	uint32_t client_sts;	/* This will be initialized to 1 when
				   remote storage RPC client is ready
				   to process requests */
};
struct shared_ramfs_table {
	uint32_t magic_id;	/* Identify RAMFS details in SMEM */
	uint32_t version;	/* Version of shared_ramfs_table */
	uint32_t entries;	/* Total number of valid entries   */
	/* List all entries */
	struct shared_ramfs_entry ramfs_entry[MAX_RAMFS_TBL_ENTRIES];
};

struct rmt_storage_client_info {
	unsigned long cids;
	struct list_head shrd_mem_list; /* List of shared memory entries */
	int open_excl;
	atomic_t total_events;
	wait_queue_head_t event_q;
	struct list_head event_list;
	struct list_head client_list;	/* List of remote storage clients */
	/* Lock to protect lists */
	spinlock_t lock;
	/* Wakelock to be acquired when processing requests from modem */
	struct wake_lock wlock;
	atomic_t wcount;
	struct workqueue_struct *workq;
};

struct rmt_storage_kevent {
	struct list_head list;
	struct rmt_storage_event event;
};

/* Remote storage server on modem */
struct rmt_storage_srv {
	uint32_t prog;
	int sync_token;
	struct platform_driver plat_drv;
	struct msm_rpc_client *rpc_client;
	struct delayed_work restart_work;
};

/* Remote storage client on modem */
struct rmt_storage_client {
	uint32_t handle;
	uint32_t sid;			/* Storage ID */
	char path[MAX_PATH_NAME];
	struct rmt_storage_srv *srv;
	struct list_head list;
};

struct rmt_shrd_mem {
	struct list_head list;
	struct rmt_shrd_mem_param param;
	struct shared_ramfs_entry *smem_info;
	struct rmt_storage_srv *srv;
};

static struct rmt_storage_srv *rmt_storage_get_srv(uint32_t prog);
static uint32_t rmt_storage_get_sid(const char *path);
#ifdef CONFIG_MSM_SDIO_SMEM
static void rmt_storage_sdio_smem_work(struct work_struct *work);
#endif

static struct rmt_storage_client_info *rmc;

#ifdef CONFIG_MSM_SDIO_SMEM
DECLARE_DELAYED_WORK(sdio_smem_work, rmt_storage_sdio_smem_work);
#endif

#ifdef CONFIG_MSM_SDIO_SMEM
#define MDM_LOCAL_BUF_SZ	0xC0000
static struct sdio_smem_client *sdio_smem;
#endif

#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
struct rmt_storage_op_stats {
	unsigned long count;
	ktime_t start;
	ktime_t min;
	ktime_t max;
	ktime_t total;
};
struct rmt_storage_stats {
       char path[MAX_PATH_NAME];
       struct rmt_storage_op_stats rd_stats;
       struct rmt_storage_op_stats wr_stats;
};
static struct rmt_storage_stats client_stats[MAX_NUM_CLIENTS];
static struct dentry *stats_dentry;
#endif

#define MSM_RMT_STORAGE_APIPROG	0x300000A7
#define MDM_RMT_STORAGE_APIPROG	0x300100A7

#define RMT_STORAGE_OP_FINISH_PROC              2
#define RMT_STORAGE_REGISTER_OPEN_PROC          3
#define RMT_STORAGE_REGISTER_WRITE_IOVEC_PROC   4
#define RMT_STORAGE_REGISTER_CB_PROC            5
#define RMT_STORAGE_UN_REGISTER_CB_PROC         6
#define RMT_STORAGE_FORCE_SYNC_PROC             7
#define RMT_STORAGE_GET_SYNC_STATUS_PROC        8
#define RMT_STORAGE_REGISTER_READ_IOVEC_PROC    9
#define RMT_STORAGE_REGISTER_ALLOC_RMT_BUF_PROC 10

#define RMT_STORAGE_OPEN_CB_TYPE_PROC           1
#define RMT_STORAGE_WRITE_IOVEC_CB_TYPE_PROC    2
#define RMT_STORAGE_EVENT_CB_TYPE_PROC          3
#define RMT_STORAGE_READ_IOVEC_CB_TYPE_PROC     4
#define RMT_STORAGE_ALLOC_RMT_BUF_CB_TYPE_PROC  5

#define RAMFS_INFO_MAGICNUMBER		0x654D4D43
#define RAMFS_INFO_VERSION		0x00000001
#define RAMFS_DEFAULT			0xFFFFFFFF

/* MSM EFS*/
#define RAMFS_MODEMSTORAGE_ID		0x4D454653
#define RAMFS_SHARED_EFS_RAM_BASE	0x46100000
#define RAMFS_SHARED_EFS_RAM_SIZE	(3 * 1024 * 1024)

/* MDM EFS*/
#define RAMFS_MDM_STORAGE_ID		0x4D4583A1
/* SSD */
#define RAMFS_SSD_STORAGE_ID		0x00535344
#define RAMFS_SHARED_SSD_RAM_BASE	0x42E00000
#define RAMFS_SHARED_SSD_RAM_SIZE	0x2000

static struct rmt_storage_client *rmt_storage_get_client(uint32_t handle)
{
	struct rmt_storage_client *rs_client;
	list_for_each_entry(rs_client, &rmc->client_list, list)
		if (rs_client->handle == handle)
			return rs_client;
	return NULL;
}

static struct rmt_storage_client *
rmt_storage_get_client_by_path(const char *path)
{
	struct rmt_storage_client *rs_client;
	list_for_each_entry(rs_client, &rmc->client_list, list)
		if (!strncmp(path, rs_client->path, MAX_PATH_NAME))
			return rs_client;
	return NULL;
}

static struct rmt_shrd_mem_param *rmt_storage_get_shrd_mem(uint32_t sid)
{
	struct rmt_shrd_mem *shrd_mem;
	struct rmt_shrd_mem_param *shrd_mem_param = NULL;

	spin_lock(&rmc->lock);
	list_for_each_entry(shrd_mem, &rmc->shrd_mem_list, list)
		if (shrd_mem->param.sid == sid)
			shrd_mem_param = &shrd_mem->param;
	spin_unlock(&rmc->lock);

	return shrd_mem_param;
}

static int rmt_storage_add_shrd_mem(uint32_t sid, uint32_t start,
				    uint32_t size, void *base,
				    struct shared_ramfs_entry *smem_info,
				    struct rmt_storage_srv *srv)
{
	struct rmt_shrd_mem *shrd_mem;

	shrd_mem = kzalloc(sizeof(struct rmt_shrd_mem), GFP_KERNEL);
	if (!shrd_mem)
		return -ENOMEM;
	shrd_mem->param.sid = sid;
	shrd_mem->param.start = start;
	shrd_mem->param.size = size;
	shrd_mem->param.base = base;
	shrd_mem->smem_info = smem_info;
	shrd_mem->srv = srv;

	spin_lock(&rmc->lock);
	list_add(&shrd_mem->list, &rmc->shrd_mem_list);
	spin_unlock(&rmc->lock);
	return 0;
}

static struct msm_rpc_client *rmt_storage_get_rpc_client(uint32_t handle)
{
	struct rmt_storage_client *rs_client;

	rs_client = rmt_storage_get_client(handle);
	if (!rs_client)
		return NULL;
	return rs_client->srv->rpc_client;
}

static int rmt_storage_validate_iovec(uint32_t handle,
				      struct rmt_storage_iovec_desc *xfer)
{
	struct rmt_storage_client *rs_client;
	struct rmt_shrd_mem_param *shrd_mem;

	rs_client = rmt_storage_get_client(handle);
	if (!rs_client)
		return -EINVAL;
	shrd_mem = rmt_storage_get_shrd_mem(rs_client->sid);
	if (!shrd_mem)
		return -EINVAL;

	if ((xfer->data_phy_addr < shrd_mem->start) ||
	    ((xfer->data_phy_addr + RAMFS_BLOCK_SIZE * xfer->num_sector) >
	     (shrd_mem->start + shrd_mem->size)))
		return -EINVAL;
	return 0;
}

static int rmt_storage_send_sts_arg(struct msm_rpc_client *client,
				struct msm_rpc_xdr *xdr, void *data)
{
	struct rmt_storage_send_sts *args = data;

	xdr_send_uint32(xdr, &args->handle);
	xdr_send_uint32(xdr, &args->err_code);
	xdr_send_uint32(xdr, &args->data);
	return 0;
}

static void put_event(struct rmt_storage_client_info *rmc,
			struct rmt_storage_kevent *kevent)
{
	spin_lock(&rmc->lock);
	list_add_tail(&kevent->list, &rmc->event_list);
	spin_unlock(&rmc->lock);
}

static struct rmt_storage_kevent *get_event(struct rmt_storage_client_info *rmc)
{
	struct rmt_storage_kevent *kevent = NULL;

	spin_lock(&rmc->lock);
	if (!list_empty(&rmc->event_list)) {
		kevent = list_first_entry(&rmc->event_list,
			struct rmt_storage_kevent, list);
		list_del(&kevent->list);
	}
	spin_unlock(&rmc->lock);
	return kevent;
}

static int rmt_storage_event_open_cb(struct rmt_storage_event *event_args,
		struct msm_rpc_xdr *xdr)
{
	uint32_t cid, len, event_type;
	char *path;
	int ret;
	struct rmt_storage_srv *srv;
	struct rmt_storage_client *rs_client = NULL;
#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
	struct rmt_storage_stats *stats;
#endif

	srv = rmt_storage_get_srv(event_args->usr_data);
	if (!srv)
		return -EINVAL;

	xdr_recv_uint32(xdr, &event_type);
	if (event_type != RMT_STORAGE_EVNT_OPEN)
		return -1;

	pr_info("%s: open callback received\n", __func__);

	ret = xdr_recv_bytes(xdr, (void **)&path, &len);
	if (ret || !path) {
		pr_err("%s: Invalid path\n", __func__);
		if (!ret)
			ret = -1;
		goto free_rs_client;
	}

	rs_client = rmt_storage_get_client_by_path(path);
	if (rs_client) {
		pr_debug("%s: Handle %d found for %s\n",
			__func__, rs_client->handle, path);
		event_args->id = RMT_STORAGE_NOOP;
		cid = rs_client->handle;
		goto end_open_cb;
	}

	rs_client = kzalloc(sizeof(struct rmt_storage_client), GFP_KERNEL);
	if (!rs_client) {
		pr_err("%s: Error allocating rmt storage client\n", __func__);
		ret = -ENOMEM;
		goto free_path;
	}

	memcpy(event_args->path, path, len);
	rs_client->sid = rmt_storage_get_sid(event_args->path);
	if (!rs_client->sid) {
		pr_err("%s: No storage id found for %s\n", __func__,
		       event_args->path);
		ret = -EINVAL;
		goto free_path;
	}
	strncpy(rs_client->path, event_args->path, MAX_PATH_NAME);

	cid = find_first_zero_bit(&rmc->cids, sizeof(rmc->cids) * 8);
	if (cid > MAX_NUM_CLIENTS) {
		pr_err("%s: Max clients are reached\n", __func__);
		cid = 0;
		return cid;
	}
	__set_bit(cid, &rmc->cids);
	pr_info("open partition %s handle=%d\n", event_args->path, cid);

#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
	stats = &client_stats[cid - 1];
	memcpy(stats->path, event_args->path, len);
	memset(stats->rd_stats, 0, sizeof(struct rmt_storage_op_stats));
	memset(stats->wr_stats, 0, sizeof(struct rmt_storage_op_stats));
	stats->rd_stats.min.tv64 = KTIME_MAX;
	stats->wr_stats.min.tv64 = KTIME_MAX;
#endif
	event_args->id = RMT_STORAGE_OPEN;
	event_args->sid = rs_client->sid;
	event_args->handle = cid;

	rs_client->handle = event_args->handle;
	rs_client->srv = srv;
	INIT_LIST_HEAD(&rs_client->list);
	spin_lock(&rmc->lock);
	list_add_tail(&rs_client->list, &rmc->client_list);
	spin_unlock(&rmc->lock);

end_open_cb:
	kfree(path);
	return cid;

free_path:
	kfree(path);
free_rs_client:
	kfree(rs_client);
	return ret;
}

struct rmt_storage_close_args {
	uint32_t handle;
};

struct rmt_storage_rw_block_args {
	uint32_t handle;
	uint32_t data_phy_addr;
	uint32_t sector_addr;
	uint32_t num_sector;
};

struct rmt_storage_get_err_args {
	uint32_t handle;
};

struct rmt_storage_user_data_args {
	uint32_t handle;
	uint32_t data;
};

struct rmt_storage_event_params {
	uint32_t type;
	union {
		struct rmt_storage_close_args close;
		struct rmt_storage_rw_block_args block;
		struct rmt_storage_get_err_args get_err;
		struct rmt_storage_user_data_args user_data;
	} params;
};

static int rmt_storage_parse_params(struct msm_rpc_xdr *xdr,
		struct rmt_storage_event_params *event)
{
	xdr_recv_uint32(xdr, &event->type);

	switch (event->type) {
	case RMT_STORAGE_EVNT_CLOSE: {
		struct rmt_storage_close_args *args;
		args = &event->params.close;

		xdr_recv_uint32(xdr, &args->handle);
		break;
	}

	case RMT_STORAGE_EVNT_WRITE_BLOCK: {
		struct rmt_storage_rw_block_args *args;
		args = &event->params.block;

		xdr_recv_uint32(xdr, &args->handle);
		xdr_recv_uint32(xdr, &args->data_phy_addr);
		xdr_recv_uint32(xdr, &args->sector_addr);
		xdr_recv_uint32(xdr, &args->num_sector);
		break;
	}

	case RMT_STORAGE_EVNT_GET_DEV_ERROR: {
		struct rmt_storage_get_err_args *args;
		args = &event->params.get_err;

		xdr_recv_uint32(xdr, &args->handle);
		break;
	}

	case RMT_STORAGE_EVNT_SEND_USER_DATA: {
		struct rmt_storage_user_data_args *args;
		args = &event->params.user_data;

		xdr_recv_uint32(xdr, &args->handle);
		xdr_recv_uint32(xdr, &args->data);
		break;
	}

	default:
		pr_err("%s: unknown event %d\n", __func__, event->type);
		return -1;
	}
	return 0;
}

static int rmt_storage_event_close_cb(struct rmt_storage_event *event_args,
		struct msm_rpc_xdr *xdr)
{
	struct rmt_storage_event_params *event;
	struct rmt_storage_close_args *close;
	struct rmt_storage_client *rs_client;
	uint32_t event_type;
	int ret;

	xdr_recv_uint32(xdr, &event_type);
	if (event_type != RMT_STORAGE_EVNT_CLOSE)
		return -1;

	pr_debug("%s: close callback received\n", __func__);
	ret = xdr_recv_pointer(xdr, (void **)&event,
			sizeof(struct rmt_storage_event_params),
			rmt_storage_parse_params);

	if (ret || !event)
		return -1;

	close = &event->params.close;
	event_args->handle = close->handle;
	event_args->id = RMT_STORAGE_CLOSE;
	__clear_bit(event_args->handle, &rmc->cids);
	rs_client = rmt_storage_get_client(event_args->handle);
	if (rs_client) {
		list_del(&rs_client->list);
		kfree(rs_client);
	}
	kfree(event);
	return RMT_STORAGE_NO_ERROR;
}

static int rmt_storage_event_write_block_cb(
		struct rmt_storage_event *event_args,
		struct msm_rpc_xdr *xdr)
{
	struct rmt_storage_event_params *event;
	struct rmt_storage_rw_block_args *write_block;
	struct rmt_storage_iovec_desc *xfer;
	uint32_t event_type;
	int ret;

	xdr_recv_uint32(xdr, &event_type);
	if (event_type != RMT_STORAGE_EVNT_WRITE_BLOCK)
		return -1;

	pr_debug("%s: write block callback received\n", __func__);
	ret = xdr_recv_pointer(xdr, (void **)&event,
			sizeof(struct rmt_storage_event_params),
			rmt_storage_parse_params);

	if (ret || !event)
		return -1;

	write_block = &event->params.block;
	event_args->handle = write_block->handle;
	xfer = &event_args->xfer_desc[0];
	xfer->sector_addr = write_block->sector_addr;
	xfer->data_phy_addr = write_block->data_phy_addr;
	xfer->num_sector = write_block->num_sector;

	ret = rmt_storage_validate_iovec(event_args->handle, xfer);
	if (ret)
		return -1;
	event_args->xfer_cnt = 1;
	event_args->id = RMT_STORAGE_WRITE;

	if (atomic_inc_return(&rmc->wcount) == 1)
		wake_lock(&rmc->wlock);

	pr_debug("sec_addr = %u, data_addr = %x, num_sec = %d\n\n",
		xfer->sector_addr, xfer->data_phy_addr,
		xfer->num_sector);

	kfree(event);
	return RMT_STORAGE_NO_ERROR;
}

static int rmt_storage_event_get_err_cb(struct rmt_storage_event *event_args,
		struct msm_rpc_xdr *xdr)
{
	struct rmt_storage_event_params *event;
	struct rmt_storage_get_err_args *get_err;
	uint32_t event_type;
	int ret;

	xdr_recv_uint32(xdr, &event_type);
	if (event_type != RMT_STORAGE_EVNT_GET_DEV_ERROR)
		return -1;

	pr_debug("%s: get err callback received\n", __func__);
	ret = xdr_recv_pointer(xdr, (void **)&event,
			sizeof(struct rmt_storage_event_params),
			rmt_storage_parse_params);

	if (ret || !event)
		return -1;

	get_err = &event->params.get_err;
	event_args->handle = get_err->handle;
	kfree(event);
	/* Not implemented */
	return -1;

}

static int rmt_storage_event_user_data_cb(struct rmt_storage_event *event_args,
		struct msm_rpc_xdr *xdr)
{
	struct rmt_storage_event_params *event;
	struct rmt_storage_user_data_args *user_data;
	uint32_t event_type;
	int ret;

	xdr_recv_uint32(xdr, &event_type);
	if (event_type != RMT_STORAGE_EVNT_SEND_USER_DATA)
		return -1;

	pr_info("%s: send user data callback received\n", __func__);
	ret = xdr_recv_pointer(xdr, (void **)&event,
			sizeof(struct rmt_storage_event_params),
			rmt_storage_parse_params);

	if (ret || !event)
		return -1;

	user_data = &event->params.user_data;
	event_args->handle = user_data->handle;
	event_args->usr_data = user_data->data;
	event_args->id = RMT_STORAGE_SEND_USER_DATA;

	kfree(event);
	return RMT_STORAGE_NO_ERROR;
}

static int rmt_storage_event_write_iovec_cb(
		struct rmt_storage_event *event_args,
		struct msm_rpc_xdr *xdr)
{
	struct rmt_storage_iovec_desc *xfer;
	uint32_t i, ent, event_type;
#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
	struct rmt_storage_stats *stats;
#endif

	xdr_recv_uint32(xdr, &event_type);
	if (event_type != RMT_STORAGE_EVNT_WRITE_IOVEC)
		return -EINVAL;

	pr_info("%s: write iovec callback received\n", __func__);
	xdr_recv_uint32(xdr, &event_args->handle);
	xdr_recv_uint32(xdr, &ent);
	pr_debug("handle = %d\n", event_args->handle);

#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
	stats = &client_stats[event_args->handle - 1];
	stats->wr_stats.start = ktime_get();
#endif
	for (i = 0; i < ent; i++) {
		xfer = &event_args->xfer_desc[i];
		xdr_recv_uint32(xdr, &xfer->sector_addr);
		xdr_recv_uint32(xdr, &xfer->data_phy_addr);
		xdr_recv_uint32(xdr, &xfer->num_sector);

		if (rmt_storage_validate_iovec(event_args->handle, xfer))
			return -EINVAL;

		pr_debug("sec_addr = %u, data_addr = %x, num_sec = %d\n",
			xfer->sector_addr, xfer->data_phy_addr,
			xfer->num_sector);
	}
	xdr_recv_uint32(xdr, &event_args->xfer_cnt);
	event_args->id = RMT_STORAGE_WRITE;
	if (atomic_inc_return(&rmc->wcount) == 1)
		wake_lock(&rmc->wlock);

	pr_debug("iovec transfer count = %d\n\n", event_args->xfer_cnt);
	return RMT_STORAGE_NO_ERROR;
}

static int rmt_storage_event_read_iovec_cb(
		struct rmt_storage_event *event_args,
		struct msm_rpc_xdr *xdr)
{
	struct rmt_storage_iovec_desc *xfer;
	uint32_t i, ent, event_type;
#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
	struct rmt_storage_stats *stats;
#endif

	xdr_recv_uint32(xdr, &event_type);
	if (event_type != RMT_STORAGE_EVNT_READ_IOVEC)
		return -EINVAL;

	pr_info("%s: read iovec callback received\n", __func__);
	xdr_recv_uint32(xdr, &event_args->handle);
	xdr_recv_uint32(xdr, &ent);
	pr_debug("handle = %d\n", event_args->handle);

#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
	stats = &client_stats[event_args->handle - 1];
	stats->rd_stats.start = ktime_get();
#endif
	for (i = 0; i < ent; i++) {
		xfer = &event_args->xfer_desc[i];
		xdr_recv_uint32(xdr, &xfer->sector_addr);
		xdr_recv_uint32(xdr, &xfer->data_phy_addr);
		xdr_recv_uint32(xdr, &xfer->num_sector);

		if (rmt_storage_validate_iovec(event_args->handle, xfer))
			return -EINVAL;

		pr_debug("sec_addr = %u, data_addr = %x, num_sec = %d\n",
			xfer->sector_addr, xfer->data_phy_addr,
			xfer->num_sector);
	}
	xdr_recv_uint32(xdr, &event_args->xfer_cnt);
	event_args->id = RMT_STORAGE_READ;
	if (atomic_inc_return(&rmc->wcount) == 1)
		wake_lock(&rmc->wlock);

	pr_debug("iovec transfer count = %d\n\n", event_args->xfer_cnt);
	return RMT_STORAGE_NO_ERROR;
}

#ifdef CONFIG_MSM_SDIO_SMEM
static int sdio_smem_cb(int event)
{
	pr_debug("%s: Received event %d\n", __func__, event);

	switch (event) {
	case SDIO_SMEM_EVENT_READ_DONE:
		pr_debug("Read done\n");
		break;
	case SDIO_SMEM_EVENT_READ_ERR:
		pr_err("Read overflow\n");
		return -EIO;
	default:
		pr_err("Unhandled event\n");
	}
	return 0;
}

static int rmt_storage_sdio_smem_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct rmt_shrd_mem_param *shrd_mem;

	sdio_smem = container_of(pdev, struct sdio_smem_client, plat_dev);

	/* SDIO SMEM is supported only for MDM */
	shrd_mem = rmt_storage_get_shrd_mem(RAMFS_MDM_STORAGE_ID);
	if (!shrd_mem) {
		pr_err("%s: No shared mem entry for sid=0x%08x\n",
		       __func__, (uint32_t)RAMFS_MDM_STORAGE_ID);
		return -ENOMEM;
	}
	sdio_smem->buf = __va(shrd_mem->start);
	sdio_smem->size = shrd_mem->size;
	sdio_smem->cb_func = sdio_smem_cb;
	ret = sdio_smem_register_client();
	if (ret)
		pr_info("%s: Error (%d) registering sdio_smem client\n",
			__func__, ret);
	return ret;
}

static int rmt_storage_sdio_smem_remove(struct platform_device *pdev)
{
	sdio_smem_unregister_client();
	queue_delayed_work(rmc->workq, &sdio_smem_work, 0);
	return 0;
}

static int sdio_smem_drv_registered;
static struct platform_driver sdio_smem_drv = {
	.probe		= rmt_storage_sdio_smem_probe,
	.remove		= rmt_storage_sdio_smem_remove,
	.driver		= {
		.name	= "SDIO_SMEM_CLIENT",
		.owner	= THIS_MODULE,
	},
};

static void rmt_storage_sdio_smem_work(struct work_struct *work)
{
	platform_driver_unregister(&sdio_smem_drv);
	sdio_smem_drv_registered = 0;
}
#endif

static int rmt_storage_event_alloc_rmt_buf_cb(
		struct rmt_storage_event *event_args,
		struct msm_rpc_xdr *xdr)
{
	struct rmt_storage_client *rs_client;
	struct rmt_shrd_mem_param *shrd_mem;
	uint32_t event_type, handle, size;
#ifdef CONFIG_MSM_SDIO_SMEM
	int ret;
#endif
	xdr_recv_uint32(xdr, &event_type);
	if (event_type != RMT_STORAGE_EVNT_ALLOC_RMT_BUF)
		return -EINVAL;

	pr_info("%s: Alloc rmt buf callback received\n", __func__);
	xdr_recv_uint32(xdr, &handle);
	xdr_recv_uint32(xdr, &size);

	pr_debug("%s: handle=0x%x size=0x%x\n", __func__, handle, size);

	rs_client = rmt_storage_get_client(handle);
	if (!rs_client) {
		pr_err("%s: Unable to find client for handle=%d\n",
		       __func__, handle);
		return -EINVAL;
	}

	rs_client->sid = rmt_storage_get_sid(rs_client->path);
	if (!rs_client->sid) {
		pr_err("%s: No storage id found for %s\n",
		       __func__, rs_client->path);
		return -EINVAL;
	}

	shrd_mem = rmt_storage_get_shrd_mem(rs_client->sid);
	if (!shrd_mem) {
		pr_err("%s: No shared memory entry found\n",
		       __func__);
		return -ENOMEM;
	}
	if (shrd_mem->size < size) {
		pr_err("%s: Size mismatch for handle=%d\n",
		       __func__, rs_client->handle);
		return -EINVAL;
	}
	pr_debug("%s: %d bytes at phys=0x%x for handle=%d found\n",
		__func__, size, shrd_mem->start, rs_client->handle);

#ifdef CONFIG_MSM_SDIO_SMEM
	if (rs_client->srv->prog == MDM_RMT_STORAGE_APIPROG) {
		if (!sdio_smem_drv_registered) {
			ret = platform_driver_register(&sdio_smem_drv);
			if (!ret)
				sdio_smem_drv_registered = 1;
			else
				pr_err("%s: Cant register sdio smem client\n",
				       __func__);
		}
	}
#endif
	event_args->id = RMT_STORAGE_NOOP;
	return (int)shrd_mem->start;
}

static int handle_rmt_storage_call(struct msm_rpc_client *client,
				struct rpc_request_hdr *req,
				struct msm_rpc_xdr *xdr)
{
	int rc;
	uint32_t result = RMT_STORAGE_NO_ERROR;
	uint32_t rpc_status = RPC_ACCEPTSTAT_SUCCESS;
	struct rmt_storage_event *event_args;
	struct rmt_storage_kevent *kevent;

	kevent = kzalloc(sizeof(struct rmt_storage_kevent), GFP_KERNEL);
	if (!kevent) {
		rpc_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
		goto out;
	}
	event_args = &kevent->event;

	switch (req->procedure) {
	case RMT_STORAGE_OPEN_CB_TYPE_PROC:
		/* client created in cb needs a ref. to its server */
		event_args->usr_data = client->prog;
		/* fall through */

	case RMT_STORAGE_WRITE_IOVEC_CB_TYPE_PROC:
		/* fall through */

	case RMT_STORAGE_READ_IOVEC_CB_TYPE_PROC:
		/* fall through */

	case RMT_STORAGE_ALLOC_RMT_BUF_CB_TYPE_PROC:
		/* fall through */

	case RMT_STORAGE_EVENT_CB_TYPE_PROC: {
		uint32_t cb_id;
		int (*cb_func)(struct rmt_storage_event *event_args,
				struct msm_rpc_xdr *xdr);

		xdr_recv_uint32(xdr, &cb_id);
		cb_func = msm_rpc_get_cb_func(client, cb_id);

		if (!cb_func) {
			rpc_status = RPC_ACCEPTSTAT_GARBAGE_ARGS;
			kfree(kevent);
			goto out;
		}

		rc = cb_func(event_args, xdr);
		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: Invalid parameters received\n", __func__);
			if (req->procedure == RMT_STORAGE_OPEN_CB_TYPE_PROC)
				result = 0; /* bad handle to signify err */
			else
				result = RMT_STORAGE_ERROR_PARAM;
			kfree(kevent);
			goto out;
		}
		result = (uint32_t) rc;
		break;
	}

	default:
		kfree(kevent);
		pr_err("%s: unknown procedure %d\n", __func__, req->procedure);
		rpc_status = RPC_ACCEPTSTAT_PROC_UNAVAIL;
		goto out;
	}

	if (kevent->event.id != RMT_STORAGE_NOOP) {
		put_event(rmc, kevent);
		atomic_inc(&rmc->total_events);
		wake_up(&rmc->event_q);
	} else
		kfree(kevent);

out:
	pr_debug("%s: Sending result=0x%x\n", __func__, result);
	xdr_start_accepted_reply(xdr, rpc_status);
	xdr_send_uint32(xdr, &result);
	rc = xdr_send_msg(xdr);
	if (rc)
		pr_err("%s: send accepted reply failed: %d\n", __func__, rc);

	return rc;
}

static int rmt_storage_open(struct inode *ip, struct file *fp)
{
	int ret = 0;

	spin_lock(&rmc->lock);
	if (!rmc->open_excl)
		rmc->open_excl = 1;
	else
		ret = -EBUSY;
	spin_unlock(&rmc->lock);

	return ret;
}

static int rmt_storage_release(struct inode *ip, struct file *fp)
{
	spin_lock(&rmc->lock);
	rmc->open_excl = 0;
	spin_unlock(&rmc->lock);

	return 0;
}

static long rmt_storage_ioctl(struct file *fp, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;
	struct rmt_storage_kevent *kevent;
	struct rmt_storage_send_sts status;
	static struct msm_rpc_client *rpc_client;
	struct rmt_shrd_mem_param usr_shrd_mem, *shrd_mem;

#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
	struct rmt_storage_stats *stats;
	struct rmt_storage_op_stats *op_stats;
	ktime_t curr_stat;
#endif

	switch (cmd) {

	case RMT_STORAGE_SHRD_MEM_PARAM:
		pr_debug("%s: get shared memory parameters ioctl\n", __func__);
		if (copy_from_user(&usr_shrd_mem, (void __user *)arg,
				sizeof(struct rmt_shrd_mem_param))) {
			pr_err("%s: copy from user failed\n\n", __func__);
			ret = -EFAULT;
			break;
		}

		shrd_mem = rmt_storage_get_shrd_mem(usr_shrd_mem.sid);
		if (!shrd_mem) {
			pr_err("%s: invalid sid (0x%x)\n", __func__,
			       usr_shrd_mem.sid);
			ret = -EFAULT;
			break;
		}

		if (copy_to_user((void __user *)arg, shrd_mem,
			sizeof(struct rmt_shrd_mem_param))) {
			pr_err("%s: copy to user failed\n\n", __func__);
			ret = -EFAULT;
		}
		break;

	case RMT_STORAGE_WAIT_FOR_REQ:
		pr_debug("%s: wait for request ioctl\n", __func__);
		if (atomic_read(&rmc->total_events) == 0) {
			ret = wait_event_interruptible(rmc->event_q,
				atomic_read(&rmc->total_events) != 0);
		}
		if (ret < 0)
			break;
		atomic_dec(&rmc->total_events);

		kevent = get_event(rmc);
		WARN_ON(kevent == NULL);
		if (copy_to_user((void __user *)arg, &kevent->event,
			sizeof(struct rmt_storage_event))) {
			pr_err("%s: copy to user failed\n\n", __func__);
			ret = -EFAULT;
		}
		kfree(kevent);
		break;

	case RMT_STORAGE_SEND_STATUS:
		pr_info("%s: send status ioctl\n", __func__);
		if (copy_from_user(&status, (void __user *)arg,
				sizeof(struct rmt_storage_send_sts))) {
			pr_err("%s: copy from user failed\n\n", __func__);
			ret = -EFAULT;
			if (atomic_dec_return(&rmc->wcount) == 0)
				wake_unlock(&rmc->wlock);
			break;
		}
#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
		stats = &client_stats[status.handle - 1];
		if (status.xfer_dir == RMT_STORAGE_WRITE)
			op_stats = &stats->wr_stats;
		else
			op_stats = &stats->rd_stats;
		curr_stat = ktime_sub(ktime_get(), op_stats->start);
		op_stats->total = ktime_add(op_stats->total, curr_stat);
		op_stats->count++;
		if (curr_stat.tv64 < stats->min.tv64)
			op_stats->min = curr_stat;
		if (curr_stat.tv64 > stats->max.tv64)
			op_stats->max = curr_stat;
#endif
		pr_debug("%s: \thandle=%d err_code=%d data=0x%x\n", __func__,
			status.handle, status.err_code, status.data);
		rpc_client = rmt_storage_get_rpc_client(status.handle);
		if (rpc_client)
			ret = msm_rpc_client_req2(rpc_client,
				RMT_STORAGE_OP_FINISH_PROC,
				rmt_storage_send_sts_arg,
				&status, NULL, NULL, -1);
		else
			ret = -EINVAL;
		if (ret < 0)
			pr_err("%s: send status failed with ret val = %d\n",
				__func__, ret);
		if (atomic_dec_return(&rmc->wcount) == 0)
			wake_unlock(&rmc->wlock);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

struct rmt_storage_sync_recv_arg {
	int data;
};

static int rmt_storage_receive_sync_arg(struct msm_rpc_client *client,
				struct msm_rpc_xdr *xdr, void *data)
{
	struct rmt_storage_sync_recv_arg *args = data;
	struct rmt_storage_srv *srv;

	srv = rmt_storage_get_srv(client->prog);
	if (!srv)
		return -EINVAL;
	xdr_recv_int32(xdr, &args->data);
	srv->sync_token = args->data;
	return 0;
}

static int rmt_storage_force_sync(struct msm_rpc_client *client)
{
	struct rmt_storage_sync_recv_arg args;
	int rc;
	rc = msm_rpc_client_req2(client,
			RMT_STORAGE_FORCE_SYNC_PROC, NULL, NULL,
			rmt_storage_receive_sync_arg, &args, -1);
	if (rc) {
		pr_err("%s: force sync RPC req failed: %d\n", __func__, rc);
		return rc;
	}
	return 0;
}

struct rmt_storage_sync_sts_arg {
	int token;
};

static int rmt_storage_send_sync_sts_arg(struct msm_rpc_client *client,
				struct msm_rpc_xdr *xdr, void *data)
{
	struct rmt_storage_sync_sts_arg *req = data;

	xdr_send_int32(xdr, &req->token);
	return 0;
}

static int rmt_storage_receive_sync_sts_arg(struct msm_rpc_client *client,
				struct msm_rpc_xdr *xdr, void *data)
{
	struct rmt_storage_sync_recv_arg *args = data;

	xdr_recv_int32(xdr, &args->data);
	return 0;
}

static int rmt_storage_get_sync_status(struct msm_rpc_client *client)
{
	struct rmt_storage_sync_recv_arg recv_args;
	struct rmt_storage_sync_sts_arg send_args;
	struct rmt_storage_srv *srv;
	int rc;

	srv = rmt_storage_get_srv(client->prog);
	if (!srv)
		return -EINVAL;

	if (srv->sync_token < 0)
		return -EINVAL;

	send_args.token = srv->sync_token;
	rc = msm_rpc_client_req2(client,
			RMT_STORAGE_GET_SYNC_STATUS_PROC,
			rmt_storage_send_sync_sts_arg, &send_args,
			rmt_storage_receive_sync_sts_arg, &recv_args, -1);
	if (rc) {
		pr_err("%s: sync status RPC req failed: %d\n", __func__, rc);
		return rc;
	}
	return recv_args.data;
}

static int rmt_storage_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vsize = vma->vm_end - vma->vm_start;
	int ret = -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				 vsize, vma->vm_page_prot);
	if (ret < 0)
		pr_err("%s: failed with return val %d\n", __func__, ret);
	return ret;
}

struct rmt_storage_reg_cb_args {
	uint32_t event;
	uint32_t cb_id;
};

static int rmt_storage_arg_cb(struct msm_rpc_client *client,
		struct msm_rpc_xdr *xdr, void *data)
{
	struct rmt_storage_reg_cb_args *args = data;

	xdr_send_uint32(xdr, &args->event);
	xdr_send_uint32(xdr, &args->cb_id);
	return 0;
}

static int rmt_storage_reg_cb(struct msm_rpc_client *client,
			      uint32_t proc, uint32_t event, void *callback)
{
	struct rmt_storage_reg_cb_args args;
	int rc, cb_id;
	int retries = 10;

	cb_id = msm_rpc_add_cb_func(client, callback);
	if ((cb_id < 0) && (cb_id != MSM_RPC_CLIENT_NULL_CB_ID))
		return cb_id;

	args.event = event;
	args.cb_id = cb_id;

	while (retries) {
		rc = msm_rpc_client_req2(client, proc, rmt_storage_arg_cb,
					 &args, NULL, NULL, -1);
		if (rc != -ETIMEDOUT)
			break;
		retries--;
		udelay(1000);
	}
	if (rc)
		pr_err("%s: Failed to register callback for event %d\n",
				__func__, event);
	return rc;
}

#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
static int rmt_storage_stats_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t rmt_storage_stats_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	uint32_t tot_clients;
	char buf[512];
	int max, j, i = 0;
	struct rmt_storage_stats *stats;

	max = sizeof(buf) - 1;
	tot_clients = find_first_zero_bit(&rmc->cids, sizeof(rmc->cids)) - 1;

	for (j = 0; j < tot_clients; j++) {
		stats = &client_stats[j];
		i += scnprintf(buf + i, max - i, "stats for partition %s:\n",
				stats->path);
		i += scnprintf(buf + i, max - i, "Min read time: %lld us\n",
				ktime_to_us(stats->rd_stats.min));
		i += scnprintf(buf + i, max - i, "Max read time: %lld us\n",
				ktime_to_us(stats->rd_stats.max));
		i += scnprintf(buf + i, max - i, "Total read time: %lld us\n",
				ktime_to_us(stats->rd_stats.total));
		i += scnprintf(buf + i, max - i, "Total read requests: %ld\n",
				stats->rd_stats.count);
		if (stats->count)
			i += scnprintf(buf + i, max - i,
				"Avg read time: %lld us\n",
				div_s64(ktime_to_us(stats->total),
				stats->rd_stats.count));

		i += scnprintf(buf + i, max - i, "Min write time: %lld us\n",
				ktime_to_us(stats->wr_stats.min));
		i += scnprintf(buf + i, max - i, "Max write time: %lld us\n",
				ktime_to_us(stats->wr_stats.max));
		i += scnprintf(buf + i, max - i, "Total write time: %lld us\n",
				ktime_to_us(stats->wr_stats.total));
		i += scnprintf(buf + i, max - i, "Total read requests: %ld\n",
				stats->wr_stats.count);
		if (stats->count)
			i += scnprintf(buf + i, max - i,
				"Avg write time: %lld us\n",
				div_s64(ktime_to_us(stats->total),
				stats->wr_stats.count));
	}
	return simple_read_from_buffer(ubuf, count, ppos, buf, i);
}

static const struct file_operations debug_ops = {
	.owner = THIS_MODULE,
	.open = rmt_storage_stats_open,
	.read = rmt_storage_stats_read,
};
#endif

const struct file_operations rmt_storage_fops = {
	.owner = THIS_MODULE,
	.open = rmt_storage_open,
	.unlocked_ioctl	 = rmt_storage_ioctl,
	.mmap = rmt_storage_mmap,
	.release = rmt_storage_release,
};

static struct miscdevice rmt_storage_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rmt_storage",
	.fops = &rmt_storage_fops,
};

static int rmt_storage_get_ramfs(struct rmt_storage_srv *srv)
{
	struct shared_ramfs_table *ramfs_table;
	struct shared_ramfs_entry *ramfs_entry;
	int index, ret;

	if (srv->prog != MSM_RMT_STORAGE_APIPROG)
		return 0;

	ramfs_table = smem_alloc(SMEM_SEFS_INFO,
			sizeof(struct shared_ramfs_table));

	if (!ramfs_table) {
		pr_err("%s: No RAMFS table in SMEM\n", __func__);
		return -ENOENT;
	}

	if ((ramfs_table->magic_id != (u32) RAMFS_INFO_MAGICNUMBER) ||
	    (ramfs_table->version != (u32) RAMFS_INFO_VERSION)) {
		pr_err("%s: Magic / Version mismatch:, "
		       "magic_id=%#x, format_version=%#x\n", __func__,
		       ramfs_table->magic_id, ramfs_table->version);
		return -ENOENT;
	}

	for (index = 0; index < ramfs_table->entries; index++) {
		ramfs_entry = &ramfs_table->ramfs_entry[index];
		if (!ramfs_entry->client_id ||
		    ramfs_entry->client_id == (u32) RAMFS_DEFAULT)
			break;

		pr_info("%s: RAMFS entry: addr = 0x%08x, size = 0x%08x\n",
			__func__, ramfs_entry->base_addr, ramfs_entry->size);

		ret = rmt_storage_add_shrd_mem(ramfs_entry->client_id,
					       ramfs_entry->base_addr,
					       ramfs_entry->size,
					       NULL,
					       ramfs_entry,
					       srv);
		if (ret) {
			pr_err("%s: Error (%d) adding shared mem\n",
			       __func__, ret);
			return ret;
		}
	}
	return 0;
}

static ssize_t
show_force_sync(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev;
	struct rpcsvr_platform_device *rpc_pdev;
	struct rmt_storage_srv *srv;

	pdev = container_of(dev, struct platform_device, dev);
	rpc_pdev = container_of(pdev, struct rpcsvr_platform_device, base);
	srv = rmt_storage_get_srv(rpc_pdev->prog);
	if (!srv) {
		pr_err("%s: Unable to find prog=0x%x\n", __func__,
		       rpc_pdev->prog);
		return -EINVAL;
	}

	return rmt_storage_force_sync(srv->rpc_client);
}

/* Returns -EINVAL for invalid sync token and an error value for any failure
 * in RPC call. Upon success, it returns a sync status of 1 (sync done)
 * or 0 (sync still pending).
 */
static ssize_t
show_sync_sts(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev;
	struct rpcsvr_platform_device *rpc_pdev;
	struct rmt_storage_srv *srv;

	pdev = container_of(dev, struct platform_device, dev);
	rpc_pdev = container_of(pdev, struct rpcsvr_platform_device, base);
	srv = rmt_storage_get_srv(rpc_pdev->prog);
	if (!srv) {
		pr_err("%s: Unable to find prog=0x%x\n", __func__,
		       rpc_pdev->prog);
		return -EINVAL;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n",
			rmt_storage_get_sync_status(srv->rpc_client));
}

static int rmt_storage_init_ramfs(struct rmt_storage_srv *srv)
{
	struct shared_ramfs_table *ramfs_table;

	if (srv->prog != MSM_RMT_STORAGE_APIPROG)
		return 0;

	ramfs_table = smem_alloc(SMEM_SEFS_INFO,
				 sizeof(struct shared_ramfs_table));

	if (!ramfs_table) {
		pr_err("%s: No RAMFS table in SMEM\n", __func__);
		return -ENOENT;
	}

	if (ramfs_table->magic_id == RAMFS_INFO_MAGICNUMBER) {
		pr_debug("RAMFS table already filled... skipping %s", \
			__func__);
		return 0;
	}

	ramfs_table->ramfs_entry[0].client_id  = RAMFS_MODEMSTORAGE_ID;
	ramfs_table->ramfs_entry[0].base_addr  = RAMFS_SHARED_EFS_RAM_BASE;
	ramfs_table->ramfs_entry[0].size       = RAMFS_SHARED_EFS_RAM_SIZE;
	ramfs_table->ramfs_entry[0].client_sts = RAMFS_DEFAULT;

	ramfs_table->ramfs_entry[1].client_id  = RAMFS_SSD_STORAGE_ID;
	ramfs_table->ramfs_entry[1].base_addr  = RAMFS_SHARED_SSD_RAM_BASE;
	ramfs_table->ramfs_entry[1].size       = RAMFS_SHARED_SSD_RAM_SIZE;
	ramfs_table->ramfs_entry[1].client_sts = RAMFS_DEFAULT;

	ramfs_table->entries  = 2;
	ramfs_table->version  = RAMFS_INFO_VERSION;
	ramfs_table->magic_id = RAMFS_INFO_MAGICNUMBER;

	return 0;
}

static void rmt_storage_set_client_status(struct rmt_storage_srv *srv,
					  int enable)
{
	struct rmt_shrd_mem *shrd_mem;

	spin_lock(&rmc->lock);
	list_for_each_entry(shrd_mem, &rmc->shrd_mem_list, list)
		if (shrd_mem->srv->prog == srv->prog)
			if (shrd_mem->smem_info)
				shrd_mem->smem_info->client_sts = !!enable;
	spin_unlock(&rmc->lock);
}

static DEVICE_ATTR(force_sync, S_IRUGO | S_IWUSR, show_force_sync, NULL);
static DEVICE_ATTR(sync_sts, S_IRUGO | S_IWUSR, show_sync_sts, NULL);
static struct attribute *dev_attrs[] = {
	&dev_attr_force_sync.attr,
	&dev_attr_sync_sts.attr,
	NULL,
};
static struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
};

static void handle_restart_teardown(struct msm_rpc_client *client)
{
	struct rmt_storage_srv *srv;

	srv = rmt_storage_get_srv(client->prog);
	if (!srv)
		return;
	pr_debug("%s: Modem restart for 0x%08x\n", __func__, srv->prog);
	cancel_delayed_work_sync(&srv->restart_work);
}

#define RESTART_WORK_DELAY_MS	1000

static void handle_restart_setup(struct msm_rpc_client *client)
{
	struct rmt_storage_srv *srv;

	srv = rmt_storage_get_srv(client->prog);
	if (!srv)
		return;
	pr_debug("%s: Scheduling restart for 0x%08x\n", __func__, srv->prog);
	queue_delayed_work(rmc->workq, &srv->restart_work,
			msecs_to_jiffies(RESTART_WORK_DELAY_MS));
}

static int rmt_storage_reg_callbacks(struct msm_rpc_client *client)
{
	int ret;

	ret = rmt_storage_reg_cb(client,
				 RMT_STORAGE_REGISTER_OPEN_PROC,
				 RMT_STORAGE_EVNT_OPEN,
				 rmt_storage_event_open_cb);
	if (ret)
		return ret;
	ret = rmt_storage_reg_cb(client,
				 RMT_STORAGE_REGISTER_CB_PROC,
				 RMT_STORAGE_EVNT_CLOSE,
				 rmt_storage_event_close_cb);
	if (ret)
		return ret;
	ret = rmt_storage_reg_cb(client,
				 RMT_STORAGE_REGISTER_CB_PROC,
				 RMT_STORAGE_EVNT_WRITE_BLOCK,
				 rmt_storage_event_write_block_cb);
	if (ret)
		return ret;
	ret = rmt_storage_reg_cb(client,
				 RMT_STORAGE_REGISTER_CB_PROC,
				 RMT_STORAGE_EVNT_GET_DEV_ERROR,
				 rmt_storage_event_get_err_cb);
	if (ret)
		return ret;
	ret = rmt_storage_reg_cb(client,
				 RMT_STORAGE_REGISTER_WRITE_IOVEC_PROC,
				 RMT_STORAGE_EVNT_WRITE_IOVEC,
				 rmt_storage_event_write_iovec_cb);
	if (ret)
		return ret;
	ret = rmt_storage_reg_cb(client,
				 RMT_STORAGE_REGISTER_READ_IOVEC_PROC,
				 RMT_STORAGE_EVNT_READ_IOVEC,
				 rmt_storage_event_read_iovec_cb);
	if (ret)
		return ret;
	ret = rmt_storage_reg_cb(client,
				 RMT_STORAGE_REGISTER_CB_PROC,
				 RMT_STORAGE_EVNT_SEND_USER_DATA,
				 rmt_storage_event_user_data_cb);
	if (ret)
		return ret;
	ret = rmt_storage_reg_cb(client,
				 RMT_STORAGE_REGISTER_ALLOC_RMT_BUF_PROC,
				 RMT_STORAGE_EVNT_ALLOC_RMT_BUF,
				 rmt_storage_event_alloc_rmt_buf_cb);
	if (ret)
		pr_info("%s: Unable (%d) registering aloc_rmt_buf\n",
			__func__, ret);

	pr_debug("%s: Callbacks (re)registered for 0x%08x\n\n", __func__,
		 client->prog);
	return 0;
}

static void rmt_storage_restart_work(struct work_struct *work)
{
	struct rmt_storage_srv *srv;
	int ret;

	srv = container_of((struct delayed_work *)work,
			   struct rmt_storage_srv, restart_work);
	if (!rmt_storage_get_srv(srv->prog)) {
		pr_err("%s: Invalid server\n", __func__);
		return;
	}

	ret = rmt_storage_reg_callbacks(srv->rpc_client);
	if (!ret)
		return;

	pr_err("%s: Error (%d) re-registering callbacks for0x%08x\n",
	       __func__, ret, srv->prog);

	if (!msm_rpc_client_in_reset(srv->rpc_client))
		queue_delayed_work(rmc->workq, &srv->restart_work,
				msecs_to_jiffies(RESTART_WORK_DELAY_MS));
}

static int rmt_storage_probe(struct platform_device *pdev)
{
	struct rpcsvr_platform_device *dev;
	struct rmt_storage_srv *srv;
	int ret;

	dev = container_of(pdev, struct rpcsvr_platform_device, base);
	srv = rmt_storage_get_srv(dev->prog);
	if (!srv) {
		pr_err("%s: Invalid prog = %#x\n", __func__, dev->prog);
		return -ENXIO;
	}

	rmt_storage_init_ramfs(srv);
	rmt_storage_get_ramfs(srv);

	INIT_DELAYED_WORK(&srv->restart_work, rmt_storage_restart_work);

	/* Client Registration */
	srv->rpc_client = msm_rpc_register_client2("rmt_storage",
						   dev->prog, dev->vers, 1,
						   handle_rmt_storage_call);
	if (IS_ERR(srv->rpc_client)) {
		pr_err("%s: Unable to register client (prog %.8x vers %.8x)\n",
				__func__, dev->prog, dev->vers);
		ret = PTR_ERR(srv->rpc_client);
		return ret;
	}

	ret = msm_rpc_register_reset_callbacks(srv->rpc_client,
		handle_restart_teardown,
		handle_restart_setup);
	if (ret)
		goto unregister_client;

	pr_info("%s: Remote storage RPC client (0x%x)initialized\n",
		__func__, dev->prog);

	/* register server callbacks */
	ret = rmt_storage_reg_callbacks(srv->rpc_client);
	if (ret)
		goto unregister_client;

	/* For targets that poll SMEM, set status to ready */
	rmt_storage_set_client_status(srv, 1);

	ret = sysfs_create_group(&pdev->dev.kobj, &dev_attr_grp);
	if (ret)
		pr_err("%s: Failed to create sysfs node: %d\n", __func__, ret);

	return 0;

unregister_client:
	msm_rpc_unregister_client(srv->rpc_client);
	return ret;
}

static void rmt_storage_client_shutdown(struct platform_device *pdev)
{
	struct rpcsvr_platform_device *dev;
	struct rmt_storage_srv *srv;

	dev = container_of(pdev, struct rpcsvr_platform_device, base);
	srv = rmt_storage_get_srv(dev->prog);
	rmt_storage_set_client_status(srv, 0);
}

static void rmt_storage_destroy_rmc(void)
{
	wake_lock_destroy(&rmc->wlock);
}

static void __init rmt_storage_init_client_info(void)
{
	/* Initialization */
	init_waitqueue_head(&rmc->event_q);
	spin_lock_init(&rmc->lock);
	atomic_set(&rmc->total_events, 0);
	INIT_LIST_HEAD(&rmc->event_list);
	INIT_LIST_HEAD(&rmc->client_list);
	INIT_LIST_HEAD(&rmc->shrd_mem_list);
	/* The client expects a non-zero return value for
	 * its open requests. Hence reserve 0 bit.  */
	__set_bit(0, &rmc->cids);
	atomic_set(&rmc->wcount, 0);
	wake_lock_init(&rmc->wlock, WAKE_LOCK_SUSPEND, "rmt_storage");
}

static struct rmt_storage_srv msm_srv = {
	.prog = MSM_RMT_STORAGE_APIPROG,
	.plat_drv = {
		.probe	  = rmt_storage_probe,
		.shutdown = rmt_storage_client_shutdown,
		.driver	  = {
			.name	= "rs300000a7",
			.owner	= THIS_MODULE,
		},
	},
};

static struct rmt_storage_srv mdm_srv = {
	.prog = MDM_RMT_STORAGE_APIPROG,
	.plat_drv = {
		.probe	  = rmt_storage_probe,
		.shutdown = rmt_storage_client_shutdown,
		.driver	  = {
			.name	= "rs300100a7",
			.owner	= THIS_MODULE,
		},
	},
};

static struct rmt_storage_srv *rmt_storage_get_srv(uint32_t prog)
{
	if (prog == MSM_RMT_STORAGE_APIPROG)
		return &msm_srv;
	if (prog == MDM_RMT_STORAGE_APIPROG)
		return &mdm_srv;
	return NULL;
}


static uint32_t rmt_storage_get_sid(const char *path)
{
	if (!strncmp(path, "/boot/modem_fs1", MAX_PATH_NAME))
		return RAMFS_MODEMSTORAGE_ID;
	if (!strncmp(path, "/boot/modem_fs2", MAX_PATH_NAME))
		return RAMFS_MODEMSTORAGE_ID;
	if (!strncmp(path, "/boot/modem_fsg", MAX_PATH_NAME))
		return RAMFS_MODEMSTORAGE_ID;
	if (!strncmp(path, "/q6_fs1_parti_id_0x59", MAX_PATH_NAME))
		return RAMFS_MDM_STORAGE_ID;
	if (!strncmp(path, "/q6_fs2_parti_id_0x5A", MAX_PATH_NAME))
		return RAMFS_MDM_STORAGE_ID;
	if (!strncmp(path, "/q6_fsg_parti_id_0x5B", MAX_PATH_NAME))
		return RAMFS_MDM_STORAGE_ID;
	if (!strncmp(path, "ssd", MAX_PATH_NAME))
		return RAMFS_SSD_STORAGE_ID;
	return 0;
}

static int __init rmt_storage_init(void)
{
#ifdef CONFIG_MSM_SDIO_SMEM
	void *mdm_local_buf;
#endif
	int ret = 0;

	rmc = kzalloc(sizeof(struct rmt_storage_client_info), GFP_KERNEL);
	if (!rmc) {
		pr_err("%s: Unable to allocate memory\n", __func__);
		return  -ENOMEM;
	}
	rmt_storage_init_client_info();

	ret = platform_driver_register(&msm_srv.plat_drv);
	if (ret) {
		pr_err("%s: Unable to register MSM RPC driver\n", __func__);
		goto rmc_free;
	}

	ret = platform_driver_register(&mdm_srv.plat_drv);
	if (ret) {
		pr_err("%s: Unable to register MDM RPC driver\n", __func__);
		goto unreg_msm_rpc;
	}

	ret = misc_register(&rmt_storage_device);
	if (ret) {
		pr_err("%s: Unable to register misc device %d\n", __func__,
				MISC_DYNAMIC_MINOR);
		goto unreg_mdm_rpc;
	}

#ifdef CONFIG_MSM_SDIO_SMEM
	mdm_local_buf = kzalloc(MDM_LOCAL_BUF_SZ, GFP_KERNEL);
	if (!mdm_local_buf) {
		pr_err("%s: Unable to allocate shadow mem\n", __func__);
		ret = -ENOMEM;
		goto unreg_misc;
	}

	ret = rmt_storage_add_shrd_mem(RAMFS_MDM_STORAGE_ID,
				       __pa(mdm_local_buf),
				       MDM_LOCAL_BUF_SZ,
				       NULL, NULL, &mdm_srv);
	if (ret) {
		pr_err("%s: Unable to add shadow mem entry\n", __func__);
		goto free_mdm_local_buf;
	}

	pr_debug("%s: Shadow memory at %p (phys=%lx), %d bytes\n", __func__,
		 mdm_local_buf, __pa(mdm_local_buf), MDM_LOCAL_BUF_SZ);
#endif

	rmc->workq = create_singlethread_workqueue("rmt_storage");
	if (!rmc->workq)
		return -ENOMEM;

#ifdef CONFIG_MSM_RMT_STORAGE_CLIENT_STATS
	stats_dentry = debugfs_create_file("rmt_storage_stats", 0444, 0,
					NULL, &debug_ops);
	if (!stats_dentry)
		pr_err("%s: Failed to create stats debugfs file\n", __func__);
#endif
	return 0;

#ifdef CONFIG_MSM_SDIO_SMEM
free_mdm_local_buf:
	kfree(mdm_local_buf);
unreg_misc:
	misc_deregister(&rmt_storage_device);
#endif
unreg_mdm_rpc:
	platform_driver_unregister(&mdm_srv.plat_drv);
unreg_msm_rpc:
	platform_driver_unregister(&msm_srv.plat_drv);
rmc_free:
	rmt_storage_destroy_rmc();
	kfree(rmc);
	return ret;
}

module_init(rmt_storage_init);
MODULE_DESCRIPTION("Remote Storage RPC Client");
MODULE_LICENSE("GPL v2");

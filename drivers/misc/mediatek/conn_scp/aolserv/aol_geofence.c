// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

/*******************************************************************************/
/*                     E X T E R N A L   R E F E R E N C E S                   */
/*******************************************************************************/
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/poll.h>

#include "conap_scp.h"
#include "aol_geofence.h"
#include "mtk_geofence_def.h"
#include "aol_buf_list.h"

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/

#define MTK_AOL_GEOFENCE_DEVNAME            "aolgeofence"

struct aol_geofence_dev {
	struct class *cls;
	struct device *dev;
	dev_t devno;
	struct cdev chdev;
};

enum aol_geofence_status {
	AOL_GEOFENCE_INACTIVE,
	AOL_GEOFENCE_ACTIVE,
};

struct aol_geofence_ctx {
	struct conap_scp_drv_cb geofence_conap_cb;
	enum aol_geofence_status status;
	bool is_conap_init;
	struct mutex lock;
};

/*******************************************************************************/
/*                             M A C R O S                                     */
/*******************************************************************************/

static struct aol_geofence_dev *g_aol_geofence_devobj;
static struct aol_geofence_ctx g_geofence_ctx;

static wait_queue_head_t g_geofence_wq;
static struct semaphore wr_mtx, rd_mtx;

struct aol_buf_pool g_geo_buf_pool;

#define MAX_BUF_LEN     (1024)
static u8 g_tmp_buf[MAX_BUF_LEN];

static bool g_geo_dev_opened;

/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/

static void aol_geofence_state_change(int state);
static void aol_geofence_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size);


/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/

static int is_scp_ready(void)
{
	int ret = 0;
	unsigned int retry = 10;
	struct aol_geofence_ctx *ctx = &g_geofence_ctx;

	while (--retry > 0) {
		ret = conap_scp_is_drv_ready(DRV_TYPE_GEOFENCE);

		if (ret == 1) {
			ctx->status = AOL_GEOFENCE_ACTIVE;
			break;
		}

		msleep(20);
	}

	if (retry == 0) {
		ctx->status = AOL_GEOFENCE_INACTIVE;
		pr_info("SCP is not yet ready\n");
		return -1;
	}

	return ret;
}

/****************************************************/
/* CONAP callback */
/****************************************************/
void aol_geofence_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
	struct aol_geofence_ctx *ctx = &g_geofence_ctx;
	struct aol_buf *data_buf = NULL;
	u32 buf_idx = 0;
	//struct scp2hal_geofence_transition *trans = NULL;
	//struct scp2hal_geofence_status *status = NULL;

	mutex_lock(&ctx->lock);
	if (g_geo_dev_opened == false) {
		mutex_unlock(&ctx->lock);
		/* TODO: send cleanup */
		pr_notice("[%s] no client is attached", __func__);
		return;
	}
	mutex_unlock(&ctx->lock);

	data_buf = aol_buffer_alloc(&g_geo_buf_pool);

	if (data_buf == NULL) {
		pr_notice("[%s] cannot allocate buffer", __func__);
		return;
	}

	pr_info("[%s] msgId=[%d] size=[%d]", __func__, msg_id, size);

	memcpy(&(data_buf->buf[buf_idx]), &msg_id, sizeof(u32));
	buf_idx += sizeof(u32);
	memcpy(&(data_buf->buf[buf_idx]), &size, sizeof(u32));
	buf_idx += sizeof(u32);
	memcpy(&(data_buf->buf[buf_idx]), buf, size);
	buf_idx += size;

	data_buf->size = buf_idx;
	data_buf->msg_id = msg_id;

	aol_buffer_active_push(&g_geo_buf_pool, data_buf);

	wake_up_interruptible(&g_geofence_wq);
}

void aol_geofence_state_change(int state)
{
	struct aol_geofence_ctx *ctx = &g_geofence_ctx;
	struct aol_buf *data_buf = NULL;
	u32 buf_idx = 0;
	u32 msg_id = GEOFENCE_SCP2HAL_RESTART, size = 0;

	pr_info("[%s] state=[%d]", __func__, state);

	mutex_lock(&g_geofence_ctx.lock);
	/* state = 1: scp ready, state = 0: scp stop */
	if (state == 1) {
		ctx->status = AOL_GEOFENCE_ACTIVE;

		/* if geofence is working, notify HAL to restart */
		data_buf = aol_buffer_alloc(&g_geo_buf_pool);
		if (data_buf) {
			pr_info("[%s] RESTART msg msgId=[%d] size=[%d]", __func__, msg_id, size);
			memcpy(&(data_buf->buf[buf_idx]), &msg_id, sizeof(u32));
			buf_idx += sizeof(u32);
			memcpy(&(data_buf->buf[buf_idx]), &size, sizeof(u32));
			buf_idx += sizeof(u32);
			data_buf->size = buf_idx;

			aol_buffer_active_push(&g_geo_buf_pool, data_buf);

			wake_up_interruptible(&g_geofence_wq);
		}

	} else
		ctx->status = AOL_GEOFENCE_INACTIVE;

	mutex_unlock(&g_geofence_ctx.lock);
}

int aol_geofence_bind_to_conap(void)
{
	int ret = 0;
	struct aol_geofence_ctx *ctx = &g_geofence_ctx;

	if (g_geofence_ctx.is_conap_init)
		return 0;

	pr_info("[%s] ====", __func__);
	ctx->geofence_conap_cb.conap_scp_msg_notify_cb = aol_geofence_msg_notify;
	ctx->geofence_conap_cb.conap_scp_state_notify_cb = aol_geofence_state_change;

	ret = conap_scp_register_drv(DRV_TYPE_GEOFENCE, &ctx->geofence_conap_cb);
	if (ret)
		pr_notice("[%s] register conap fail [%d]", __func__, ret);


	if (is_scp_ready() == 0)
		pr_info("[%s] scp ready", __func__);
	else
		pr_info("[%s] scp not ready", __func__);

	return ret;
}


/**************************************************************/
/* Device ops */
/**************************************************************/
static int mtk_aol_geofence_open(struct inode *inode, struct file *file)
{
	int cur_status;
	struct aol_geofence_ctx *ctx = &g_geofence_ctx;

	mutex_lock(&ctx->lock);
	if (g_geo_dev_opened) {
		mutex_unlock(&ctx->lock);
		pr_notice("[%s] aol_geofence dev was opened", __func__);
		return -EBUSY;
	}
	g_geo_dev_opened = true;

	/* buf pool init */
	aol_buf_pool_init(&g_geo_buf_pool);

	aol_geofence_bind_to_conap();

	if (ctx->status == AOL_GEOFENCE_INACTIVE) {
		mutex_unlock(&ctx->lock);
		pr_notice("[%s] status inactive", __func__);
		return -ENODEV;
	}

	mutex_unlock(&ctx->lock);

	cur_status = conap_scp_is_drv_ready(DRV_TYPE_GEOFENCE);
	if (cur_status == 0) {
		pr_notice("[%s] conap_scp is not ready", __func__);
		return -ENODEV;
	}

	pr_info("[%s] AOL_GEOFENCE open major %d minor %d (pid %d)", __func__,
						imajor(inode), iminor(inode), current->pid);
	return 0;
}

static ssize_t mtk_aol_geofence_read(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	int retval;
	struct aol_buf *data_buf = NULL;

	pr_info("[%s] === ", __func__);
	down(&rd_mtx);

	if (count > MAX_BUF_LEN)
		count = MAX_BUF_LEN;

	data_buf = aol_buffer_active_pop(&g_geo_buf_pool);
	if (data_buf == NULL) {
		up(&rd_mtx);
		return 0;
	}

	pr_info("[%s] msg_id=[%d] size=[%d]", __func__, data_buf->msg_id, data_buf->size);
	if (copy_to_user(buf, &data_buf->buf[0], data_buf->size)) {
		pr_info("[%s] failed,because copy_to_user error\n");
		retval = -EFAULT;
	} else {
		retval = data_buf->size;
	}
	aol_buffer_free(&g_geo_buf_pool, data_buf);

	up(&rd_mtx);
	return retval;
}

static ssize_t mtk_aol_geofence_write(struct file *filp, const char __user *buf,
							size_t count, loff_t *f_pos)
{
	int retval;
	int written = 0;
	u32 msg_id = 0;
	u32 msg_sz = 0;
	int copy_size = 0, idx = 0;

	down(&wr_mtx);
	if (count > 0) {
		copy_size = (count < MAX_BUF_LEN) ? count : MAX_BUF_LEN;
		if (copy_from_user(&g_tmp_buf[0], &buf[0], copy_size)) {
			retval = -EFAULT;
			pr_info("[%s] copy_from_user failed retval=%d", __func__, retval);
			goto out;
		}
	} else {
		retval = -EFAULT;
		pr_info("[%s] packet length:%zd is not allowed, retval = %d",
					__func__, count, retval);
	}

	memcpy(&msg_id, &g_tmp_buf[0], sizeof(u32));
	idx += sizeof(u32);
	memcpy(&msg_sz, &g_tmp_buf[idx], sizeof(u32));
	idx += sizeof(u32);

	pr_info("[%s] msgId=[%u] size=[%u]", __func__, msg_id, msg_sz);

	written = conap_scp_send_message(DRV_TYPE_GEOFENCE, msg_id,
						&g_tmp_buf[idx], count-(2*sizeof(u32)));

	if (written == 0) {
		retval = copy_size;
		pr_info("[%s] conap_send_msg success  [%d][%d]", __func__, msg_id, msg_sz);
	} else {
		retval = -EFAULT;
		pr_info("[%s] conap_scp_send_message failed retval=%d , written=%d", __func__,
					retval, written);
	}

out:
	up(&wr_mtx);
	return retval;
}

static int mtk_aol_geofence_close(struct inode *inode, struct file *file)
{
	struct aol_geofence_ctx *ctx = &g_geofence_ctx;

	mutex_lock(&ctx->lock);
	/* free buffer */
	aol_buf_pool_deinit(&g_geo_buf_pool);
	g_geo_dev_opened = false;
	mutex_unlock(&ctx->lock);

	pr_info("[%s] aolgeofence close major %d minor %d (pid %d)",
			__func__, imajor(inode), iminor(inode), current->pid);

	return 0;
}

static unsigned int mtk_aol_geofence_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	if (!aol_buffer_active_is_empty(&g_geo_buf_pool)) {
		mask = (POLLIN | POLLRDNORM);
		pr_info("[%s] already have data", __func__);
		return mask;
	}

	poll_wait(file, &g_geofence_wq, wait);

	if (!aol_buffer_active_is_empty(&g_geo_buf_pool))
		mask = (POLLIN | POLLRDNORM);
	pr_info("[%s] been return , mask = %d\n", __func__, mask);

	return mask;
}


static const struct file_operations g_aol_geofence_fops = {
	.owner = THIS_MODULE,
	.open = mtk_aol_geofence_open,
	.read = mtk_aol_geofence_read,
	.write = mtk_aol_geofence_write,
	.release = mtk_aol_geofence_close,
	.poll = mtk_aol_geofence_poll,
};



int aol_geofence_init(void)
{
	int ret = 0;
	int err = 0;

	g_aol_geofence_devobj = kzalloc(sizeof(*g_aol_geofence_devobj), GFP_KERNEL);
	if (g_aol_geofence_devobj == NULL) {
		err = -ENOMEM;
		ret = -ENOMEM;
		goto err_out;
	}

	pr_info("Registering aol_geofence chardev\n");

	ret = alloc_chrdev_region(&g_aol_geofence_devobj->devno, 0, 1, MTK_AOL_GEOFENCE_DEVNAME);
	if (ret) {
		pr_info("alloc_chrdev_region fail: %d\n", ret);
		err = -ENOMEM;
		goto err_out;
	} else
		pr_info("major: %d, minor: %d",
						MAJOR(g_aol_geofence_devobj->devno),
						MINOR(g_aol_geofence_devobj->devno));

	cdev_init(&g_aol_geofence_devobj->chdev, &g_aol_geofence_fops);
	g_aol_geofence_devobj->chdev.owner = THIS_MODULE;
	err = cdev_add(&g_aol_geofence_devobj->chdev, g_aol_geofence_devobj->devno, 1);
	if (err) {
		pr_info("cdev_add fail: %d\n", err);
		goto err_out;
	}

	g_aol_geofence_devobj->cls = class_create(THIS_MODULE, "aol_geofence");
	if (IS_ERR(g_aol_geofence_devobj->cls)) {
		pr_info("Unable to create class, err = %d",
						(int)PTR_ERR(g_aol_geofence_devobj->cls));
		goto err_out;
	}


	/* geofence context */
	memset(&g_geofence_ctx, 0, sizeof(struct aol_geofence_ctx));
	mutex_init(&g_geofence_ctx.lock);
	init_waitqueue_head(&g_geofence_wq);

	/* buf pool init */
	//aol_buf_pool_init(&g_geo_buf_pool);

	sema_init(&wr_mtx, 1);
	sema_init(&rd_mtx, 1);

	g_aol_geofence_devobj->dev = device_create(g_aol_geofence_devobj->cls,
		NULL, g_aol_geofence_devobj->devno, g_aol_geofence_devobj, "aol_geofence");
	if (IS_ERR(g_aol_geofence_devobj->dev)) {
		pr_err("device create fail, error code(%ld)\n",
							PTR_ERR(g_aol_geofence_devobj->dev));
		goto err_out;
	}

	pr_info("MTK AOL_GEOFENCE device init Done\n");
	return 0;

err_out:
	if (g_aol_geofence_devobj != NULL) {
		if (err == 0)
			cdev_del(&g_aol_geofence_devobj->chdev);
		if (ret == 0)
			unregister_chrdev_region(g_aol_geofence_devobj->devno, 1);
		kfree(g_aol_geofence_devobj);
		g_aol_geofence_devobj = NULL;
	}
	return -1;
}

void aol_geofence_deinit(void)
{
	//aol_buf_pool_deinit(&g_geo_buf_pool);

	conap_scp_unregister_drv(DRV_TYPE_GEOFENCE);

	pr_info("Unregistering aol_geofence chardev\n");
	cdev_del(&g_aol_geofence_devobj->chdev);
	unregister_chrdev_region(g_aol_geofence_devobj->devno, 1);
	device_destroy(g_aol_geofence_devobj->cls, g_aol_geofence_devobj->devno);
	class_destroy(g_aol_geofence_devobj->cls);
	kfree(g_aol_geofence_devobj);
	g_aol_geofence_devobj = NULL;

}

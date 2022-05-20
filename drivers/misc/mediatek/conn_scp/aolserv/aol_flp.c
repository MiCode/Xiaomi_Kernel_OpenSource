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
#include <linux/workqueue.h>

#include "msg_thread.h"
#include "conap_scp.h"
#include "aol_flp.h"
#include "mtk_flp_def.h"
#include "aol_buf_list.h"
#include "conap_platform_data.h"

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/

#define MTK_AOL_FLP_DEVNAME            "aolflp"

struct aol_flp_dev {
	struct class *cls;
	struct device *dev;
	dev_t devno;
	struct cdev chdev;
};

enum aol_flp_status {
	AOL_FLP_INACTIVE,
	AOL_FLP_ACTIVE,
};

struct aol_flp_ctx {
	struct conap_scp_drv_cb flp_conap_cb;
	enum aol_flp_status status;
	bool is_conap_init;
	struct mutex lock;
	bool is_batching;
};

enum aol_flp_report_state {
	FLP_REPORT_NONE,
	FLP_REPORT_SCHED,
	FLP_REPORT_WORK,
};

struct aol_flp_report_ctx {
	u32 state;
	u32 loc_size;
	void __iomem *addr;
	u32 read_sz;
};


/*******************************************************************************/
/*                             M A C R O S                                     */
/*******************************************************************************/

static struct aol_flp_dev *g_aol_flp_devobj;
static struct aol_flp_ctx g_flp_ctx;

static wait_queue_head_t g_flp_wq;
static struct semaphore wr_mtx, rd_mtx;

struct aol_buf_pool g_flp_buf_pool;

#define MAX_BUF_LEN     (1024)
static u8 g_tmp_buf[MAX_BUF_LEN];

/* report location */
static struct work_struct g_report_loc_work;
static struct work_struct g_report_loc_done_work;
static struct aol_flp_report_ctx g_flp_report_ctx;

static bool g_flp_dev_opened;
static bool g_flp_testing = true;

/*******************************************************************************/
/*                  F U N C T I O N   D E C L A R A T I O N S                  */
/*******************************************************************************/

static void aol_flp_state_change(int state);
static void aol_flp_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size);

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/

static int is_scp_ready(void)
{
	int ret = 0;
	unsigned int retry = 10;
	struct aol_flp_ctx *ctx = &g_flp_ctx;

	while (--retry > 0) {
		ret = conap_scp_is_drv_ready(DRV_TYPE_FLP);

		if (ret == 1) {
			ctx->status = AOL_FLP_ACTIVE;
			break;
		}

		msleep(20);
	}

	if (retry == 0) {
		ctx->status = AOL_FLP_INACTIVE;
		pr_info("SCP is not yet ready\n");
		return -1;
	}

	return ret;
}

/****************************************************/
/* CONAP callback */
/****************************************************/
void aol_flp_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
	struct aol_flp_ctx *ctx = &g_flp_ctx;
	struct aol_buf *data_buf = NULL;
	u32 buf_idx = 0;
	u32 sz = 0;

	pr_info("[%s] msgId=[%d] size=[%d]", __func__, msg_id, size);

	mutex_lock(&ctx->lock);
	if (g_flp_dev_opened == false) {
		mutex_unlock(&ctx->lock);
		/* TODO: send cleanup */
		pr_notice("[%s] no client is attached", __func__);
		return;
	}
	mutex_unlock(&ctx->lock);

	/* schedule worker to transfer to HAL*/
	if (msg_id == FLP_SCP2HAL_LOCATION) {

		memcpy(&sz, buf, sizeof(u32));
		g_flp_report_ctx.loc_size = sz;

		pr_info("[%s] REPORT_LOCATION start sz=[%d]", __func__, sz);
		schedule_work(&g_report_loc_work);
		return;
	}

	data_buf = aol_buffer_alloc(&g_flp_buf_pool);
	if (data_buf == NULL) {
		pr_notice("[%s] cannot allocate buffer", __func__);
		return;
	}

	memcpy(&(data_buf->buf[buf_idx]), &msg_id, sizeof(u32));
	buf_idx += sizeof(u32);
	memcpy(&(data_buf->buf[buf_idx]), &size, sizeof(u32));
	buf_idx += sizeof(u32);
	memcpy(&(data_buf->buf[buf_idx]), buf, size);
	buf_idx += size;

	data_buf->size = buf_idx;
	data_buf->msg_id = msg_id;

	aol_buffer_active_push(&g_flp_buf_pool, data_buf);

	wake_up_interruptible(&g_flp_wq);
}

void aol_flp_state_change(int state)
{
	struct aol_flp_ctx *ctx = &g_flp_ctx;
	struct aol_buf *data_buf = NULL;
	u32 buf_idx = 0;
	u32 msg_id = FLP_SCP2HAL_SCP_RESTART, size = 0;

	pr_info("[%s] state=[%d]", __func__, state);

	mutex_lock(&g_flp_ctx.lock);
	/* state = 1: scp ready, state = 0: scp stop */
	if (state == 1) {
		ctx->status = AOL_FLP_ACTIVE;

		pr_info("[%s] is_batching=[%d]", __func__, g_flp_ctx.is_batching);
		if (g_flp_ctx.is_batching) {
			/* if flp is working, notify HAL to restart */
			data_buf = aol_buffer_alloc(&g_flp_buf_pool);

			if (data_buf) {
				pr_info("[%s] RESTART msg msgId=[%d] size=[%d]",
							__func__, msg_id, size);
				memcpy(&(data_buf->buf[buf_idx]), &msg_id, sizeof(u32));
				buf_idx += sizeof(u32);
				memcpy(&(data_buf->buf[buf_idx]), &size, sizeof(u32));
				buf_idx += sizeof(u32);
				data_buf->size = buf_idx;

				aol_buffer_active_push(&g_flp_buf_pool, data_buf);

				wake_up_interruptible(&g_flp_wq);
			} else
				pr_notice("[%s] can't allocate buffer", __func__);
		}

	} else
		ctx->status = AOL_FLP_INACTIVE;

	mutex_unlock(&g_flp_ctx.lock);
}


static void aol_flp_report_location_ack(void)
{
	int ret;

	ret = conap_scp_send_message(DRV_TYPE_FLP, FLP_KERN2SCP_LOCATION_ACK, NULL, 0);
	pr_info("[%s] REPORT_LOC done send msg=[%d]", __func__, ret);
}

static void aol_flp_report_location_handler(struct work_struct *work)
{
	phys_addr_t batching_phy_addr;
	u32 batching_size;
	u32 loc_size;
	void __iomem *addr;
	void *batching_buffer = NULL;
	struct conn_flp_location *loc;
	int i;

	if (g_flp_testing) {
		batching_buffer = vmalloc(g_flp_report_ctx.loc_size);

		if (batching_buffer == NULL) {
			//pr_notice("[%s] can't allocate buffer", __func__);
			aol_flp_report_location_ack();
			return;
		}
		loc_size = g_flp_report_ctx.loc_size/sizeof(struct conn_flp_location);

		pr_info("[%s] location size=[%d]", __func__, loc_size);
		loc = (struct conn_flp_location *)batching_buffer;

		for (i = 0; i < loc_size; i++) {
			loc[i].size = sizeof(struct conn_flp_location);
			loc[i].flags = FLP_LOCATION_HAS_LAT_LONG | FLP_LOCATION_HAS_ALTITUDE;
			loc[i].timestamp = i;
			loc[i].sources_used = 0x1;
		}

		g_flp_report_ctx.addr = batching_buffer;
		g_flp_report_ctx.state = FLP_REPORT_SCHED;

	} else {
		batching_phy_addr = connsys_scp_shm_get_batching_addr();
		batching_size = connsys_scp_shm_get_batching_size();

		if (batching_phy_addr == 0 || batching_size == 0) {
			pr_notice("[%s] batching addr/size invalid [%llu][%u]", __func__,
						batching_phy_addr, batching_size);

			aol_flp_report_location_ack();
			return;
		}

		pr_info("[%s] batching addr/size [%llu][%u]", __func__,
						batching_phy_addr, batching_size);

		addr = ioremap(batching_phy_addr, batching_size);
		if (!addr) {
			pr_notice("[%s] can't allocate buffer", __func__);
			/* send location ack */
			aol_flp_report_location_ack();
			return;
		}

		g_flp_report_ctx.addr = addr;
		g_flp_report_ctx.state = FLP_REPORT_SCHED;
	}

	pr_info("[%s] REPORT_LOC scheduled", __func__);
	wake_up_interruptible(&g_flp_wq);

}

static void aol_flp_report_location_done_handler(struct work_struct *work)
{
	if (g_flp_report_ctx.addr) {
		pr_info("[%s] REPORT_LOC done unmap", __func__);
		//iounmap(g_flp_report_ctx.addr);
		vfree(g_flp_report_ctx.addr);
		g_flp_report_ctx.addr = 0;
	}

	aol_flp_report_location_ack();
}


int aol_flp_bind_to_conap(void)
{
	int ret = 0;
	struct aol_flp_ctx *ctx = &g_flp_ctx;

	if (g_flp_ctx.is_conap_init)
		return 0;

	pr_info("[%s] ====", __func__);
	ctx->flp_conap_cb.conap_scp_msg_notify_cb = aol_flp_msg_notify;
	ctx->flp_conap_cb.conap_scp_state_notify_cb = aol_flp_state_change;

	ret = conap_scp_register_drv(DRV_TYPE_FLP, &ctx->flp_conap_cb);
	if (ret) {
		pr_notice("[%s] register conap fail [%d]", __func__, ret);
		return ret;
	}


	if (is_scp_ready() == 0)
		pr_info("[%s] scp ready", __func__);
	else
		pr_info("[%s] scp not ready", __func__);

	g_flp_ctx.is_conap_init = true;
	return ret;
}


/**************************************************************/
/* Device ops */
/**************************************************************/
static int mtk_aol_flp_open(struct inode *inode, struct file *file)
{
	int cur_status;
	struct aol_flp_ctx *ctx = &g_flp_ctx;

	pr_info("[%s] +++++", __func__);
	mutex_lock(&ctx->lock);
	if (g_flp_dev_opened) {
		mutex_unlock(&ctx->lock);
		pr_notice("[%s] aol_flp dev was opened", __func__);
		return -EBUSY;
	}
	g_flp_dev_opened = true;

	aol_flp_bind_to_conap();

	if (ctx->status == AOL_FLP_INACTIVE) {
		mutex_unlock(&ctx->lock);
		pr_notice("[%s] status inactive", __func__);
		g_flp_dev_opened = false;
		return -ENODEV;
	}

	/* buffer pool */
	aol_buf_pool_init(&g_flp_buf_pool);
	mutex_unlock(&ctx->lock);

	cur_status = conap_scp_is_drv_ready(DRV_TYPE_FLP);
	if (cur_status == 0) {
		pr_notice("[%s] conap_scp is not ready", __func__);
		return -ENODEV;
	}

	pr_info("[%s] AOL_FLP open major %d minor %d (pid %d)", __func__,
						imajor(inode), iminor(inode), current->pid);
	return 0;
}

static ssize_t mtk_aol_flp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int retval;
	struct aol_buf *data_buf = NULL;
	u32 write_sz = count, msg_id = FLP_SCP2HAL_LOCATION;
	char tmpbuf[sizeof(u32) * 3];
	u32 bufidx = 0, data_size = sizeof(u32);

	down(&rd_mtx);

	if (count > MAX_BUF_LEN)
		count = MAX_BUF_LEN;

	pr_info("[%s] report state [%d]", __func__, g_flp_report_ctx.state);
	if (g_flp_report_ctx.state != FLP_REPORT_NONE) {

		if (g_flp_report_ctx.addr == NULL) {
			g_flp_report_ctx.state = FLP_REPORT_NONE;
			pr_notice("[%s] report location but addr is null", __func__);
			up(&rd_mtx);
			return -EFAULT;
		}

		if (g_flp_report_ctx.state == FLP_REPORT_SCHED) {
			bufidx = 0;
			memcpy(&tmpbuf[bufidx], &msg_id, sizeof(u32));
			bufidx += sizeof(u32);
			memcpy(&tmpbuf[bufidx], &data_size, sizeof(u32));
			bufidx += sizeof(u32);
			memcpy(&tmpbuf[bufidx], &g_flp_report_ctx.loc_size, sizeof(u32));

			if (copy_to_user(buf, &(tmpbuf[0]), sizeof(u32)*3)) {
				g_flp_report_ctx.state = FLP_REPORT_NONE;
				pr_notice("[%s] addr is null", __func__);
				up(&rd_mtx);
				return -EFAULT;
			}
			g_flp_report_ctx.state = FLP_REPORT_WORK;
			up(&rd_mtx);
			return sizeof(u32)*3;
		}


		if (g_flp_report_ctx.read_sz + count > g_flp_report_ctx.loc_size)
			write_sz = g_flp_report_ctx.loc_size - g_flp_report_ctx.read_sz;

		if (copy_to_user(buf, &(g_flp_report_ctx.addr[g_flp_report_ctx.read_sz]),
							write_sz)) {
			pr_info("[%s] failed,because copy_to_user error", __func__);
			up(&rd_mtx);
			return -EFAULT;
		}

		g_flp_report_ctx.read_sz += write_sz;

		pr_info("[%s] report_locaton alread wrote=[%d] [%d]", __func__,
					g_flp_report_ctx.read_sz, g_flp_report_ctx.loc_size);

		if (g_flp_report_ctx.read_sz == g_flp_report_ctx.loc_size) {
			g_flp_report_ctx.read_sz = 0;
			g_flp_report_ctx.loc_size = 0;
			g_flp_report_ctx.state = FLP_REPORT_NONE;

			schedule_work(&g_report_loc_done_work);
		}

		retval = write_sz;
		up(&rd_mtx);
		return retval;
	}

	data_buf = aol_buffer_active_pop(&g_flp_buf_pool);
	if (data_buf == NULL) {
		up(&rd_mtx);
		pr_notice("[%s] data buf is null", __func__);
		return 0;
	}

	pr_info("[%s] msg_id=[%d] size=[%d]", __func__, data_buf->msg_id, data_buf->size);

	if (copy_to_user(buf, &data_buf->buf[0], data_buf->size)) {
		pr_info("[%s] failed,because copy_to_user error\n");
		retval = -EFAULT;
	} else {
		retval = data_buf->size;
	}
	aol_buffer_free(&g_flp_buf_pool, data_buf);

	up(&rd_mtx);
	return retval;
}

static ssize_t mtk_aol_flp_write(struct file *filp, const char __user *buf,
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

	written = conap_scp_send_message(DRV_TYPE_FLP, msg_id,
						&g_tmp_buf[idx], count-(2*sizeof(u32)));

	if (written == 0) {
		retval = copy_size;
		pr_info("[%s] conap_send_msg success  [%d][%d]", __func__, msg_id, msg_sz);

		if (msg_id == FLP_HAL2SCP_START_BATCHING)
			g_flp_ctx.is_batching = true;
		else if (msg_id == FLP_HAL2SCP_STOP_BATCHING)
			g_flp_ctx.is_batching = false;

	} else {
		retval = -EFAULT;
		pr_info("[%s] conap_scp_send_message failed retval=%d , written=%d", __func__,
					retval, written);
	}

out:
	up(&wr_mtx);
	return retval;
}

static int mtk_aol_flp_close(struct inode *inode, struct file *file)
{
	struct aol_flp_ctx *ctx = &g_flp_ctx;

	pr_info("[%s] +++++", __func__);

	mutex_lock(&ctx->lock);
	/* free buffer */
	aol_buf_pool_deinit(&g_flp_buf_pool);
	g_flp_dev_opened = false;
	mutex_unlock(&ctx->lock);
	pr_info("[%s] aol_flp close major %d minor %d (pid %d)", __func__,
				imajor(inode), iminor(inode), current->pid);
	return 0;
}

static unsigned int mtk_aol_flp_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	if (!aol_buffer_active_is_empty(&g_flp_buf_pool) ||
		g_flp_report_ctx.state != FLP_REPORT_NONE) {
		mask = (POLLIN | POLLRDNORM);
		//pr_info("[%s] already have data", __func__);
		return mask;
	}

	poll_wait(file, &g_flp_wq, wait);

	if (!aol_buffer_active_is_empty(&g_flp_buf_pool) ||
		g_flp_report_ctx.state != FLP_REPORT_NONE)
		mask = (POLLIN | POLLRDNORM);

	return mask;
}


static const struct file_operations g_aol_flp_fops = {
	.owner = THIS_MODULE,
	.open = mtk_aol_flp_open,
	.read = mtk_aol_flp_read,
	.write = mtk_aol_flp_write,
	.release = mtk_aol_flp_close,
	.poll = mtk_aol_flp_poll,
};



int aol_flp_init(void)
{
	int ret = 0;
	int err = 0;

	g_aol_flp_devobj = kzalloc(sizeof(*g_aol_flp_devobj), GFP_KERNEL);
	if (g_aol_flp_devobj == NULL) {
		err = -ENOMEM;
		ret = -ENOMEM;
		goto err_out;
	}

	pr_info("Registering aol_flp chardev\n");

	ret = alloc_chrdev_region(&g_aol_flp_devobj->devno, 0, 1, MTK_AOL_FLP_DEVNAME);
	if (ret) {
		pr_info("alloc_chrdev_region fail: %d\n", ret);
		err = -ENOMEM;
		goto err_out;
	} else
		pr_info("major: %d, minor: %d",
					MAJOR(g_aol_flp_devobj->devno),
					MINOR(g_aol_flp_devobj->devno));

	cdev_init(&g_aol_flp_devobj->chdev, &g_aol_flp_fops);
	g_aol_flp_devobj->chdev.owner = THIS_MODULE;
	err = cdev_add(&g_aol_flp_devobj->chdev, g_aol_flp_devobj->devno, 1);
	if (err) {
		pr_info("cdev_add fail: %d\n", err);
		goto err_out;
	}

	g_aol_flp_devobj->cls = class_create(THIS_MODULE, "aol_flp");
	if (IS_ERR(g_aol_flp_devobj->cls)) {
		pr_info("Unable to create class, err = %d\n", (int)PTR_ERR(g_aol_flp_devobj->cls));
		goto err_out;
	}


	/* flp context */
	memset(&g_flp_ctx, 0, sizeof(struct aol_flp_ctx));
	mutex_init(&g_flp_ctx.lock);
	init_waitqueue_head(&g_flp_wq);

	/* buffer pool */
	//aol_buf_pool_init(&g_flp_buf_pool);

	sema_init(&wr_mtx, 1);
	sema_init(&rd_mtx, 1);

	/* schedule worker */
	INIT_WORK(&g_report_loc_work, aol_flp_report_location_handler);
	INIT_WORK(&g_report_loc_done_work, aol_flp_report_location_done_handler);

	g_aol_flp_devobj->dev = device_create(g_aol_flp_devobj->cls,
		NULL, g_aol_flp_devobj->devno, g_aol_flp_devobj, "aol_flp");
	if (IS_ERR(g_aol_flp_devobj->dev)) {
		pr_err("device create fail, error code(%ld)\n",
								PTR_ERR(g_aol_flp_devobj->dev));
		goto err_out;
	}

	pr_info("MTK AOL_FLP device init Done\n");
	return 0;

err_out:
	if (g_aol_flp_devobj != NULL) {
		if (err == 0)
			cdev_del(&g_aol_flp_devobj->chdev);
		if (ret == 0)
			unregister_chrdev_region(g_aol_flp_devobj->devno, 1);
		kfree(g_aol_flp_devobj);
		g_aol_flp_devobj = NULL;
	}
	return -1;
}

void aol_flp_deinit(void)
{

	conap_scp_unregister_drv(DRV_TYPE_FLP);

	pr_info("Unregistering aol_flp chardev");
	cdev_del(&g_aol_flp_devobj->chdev);
	unregister_chrdev_region(g_aol_flp_devobj->devno, 1);
	device_destroy(g_aol_flp_devobj->cls, g_aol_flp_devobj->devno);
	class_destroy(g_aol_flp_devobj->cls);
	kfree(g_aol_flp_devobj);
	g_aol_flp_devobj = NULL;

}

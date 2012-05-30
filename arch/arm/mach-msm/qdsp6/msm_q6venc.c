/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/android_pmem.h>
#include <linux/msm_q6venc.h>
#include <linux/pm_qos.h>

#include <mach/cpuidle.h>

#include "dal.h"

#define DALDEVICEID_VENC_DEVICE         0x0200002D
#define DALDEVICEID_VENC_PORTNAME       "DAL_AQ_VID"

#define VENC_NAME		        "q6venc"
#define VENC_MSG_MAX                    128

#define VENC_INTERFACE_VERSION		0x00020000
#define MAJOR_MASK			0xFFFF0000
#define MINOR_MASK			0x0000FFFF
#define VENC_GET_MAJOR_VERSION(version) ((version & MAJOR_MASK)>>16)
#define VENC_GET_MINOR_VERSION(version) (version & MINOR_MASK)

enum {
	VENC_BUFFER_TYPE_INPUT,
	VENC_BUFFER_TYPE_OUTPUT,
	VENC_BUFFER_TYPE_QDSP6,
	VENC_BUFFER_TYPE_HDR
};
enum {
	VENC_DALRPC_GET_SYNTAX_HEADER = DAL_OP_FIRST_DEVICE_API,
	VENC_DALRPC_UPDATE_INTRA_REFRESH,
	VENC_DALRPC_UPDATE_FRAME_RATE,
	VENC_DALRPC_UPDATE_BITRATE,
	VENC_DALRPC_UPDATE_QP_RANGE,
	VENC_DALRPC_UPDATE_INTRA_PERIOD,
	VENC_DALRPC_REQUEST_IFRAME,
	VENC_DALRPC_START,
	VENC_DALRPC_STOP,
	VENC_DALRPC_SUSPEND,
	VENC_DALRPC_RESUME,
	VENC_DALRPC_FLUSH,
	VENC_DALRPC_QUEUE_INPUT,
	VENC_DALRPC_QUEUE_OUTPUT
};
struct venc_input_payload {
	u32 data;
};
struct venc_output_payload {
	u32 size;
	long long time_stamp;
	u32 flags;
	u32 data;
	u32 client_data_from_input;
};
union venc_payload {
	struct venc_input_payload input_payload;
	struct venc_output_payload output_payload;
};
struct venc_msg_type {
	u32 event;
	u32 status;
	union venc_payload payload;
};
struct venc_input_buf {
	struct venc_buf_type yuv_buf;
	u32 data_size;
	long long time_stamp;
	u32 flags;
	u32 dvs_offsetx;
	u32 dvs_offsety;
	u32 client_data;
	u32 op_client_data;
};
struct venc_output_buf {
	struct venc_buf_type bit_stream_buf;
	u32 client_data;
};

struct venc_msg_list {
	struct list_head list;
	struct venc_msg msg_data;
};
struct venc_buf {
	int fd;
	u32 src;
	u32 offset;
	u32 size;
	u32 btype;
	unsigned long paddr;
	struct file *file;
};
struct venc_pmem_list {
	struct list_head list;
	struct venc_buf buf;
};
struct venc_dev {
	bool is_active;
	bool pmem_freed;
	enum venc_state_type state;
	struct list_head venc_msg_list_head;
	struct list_head venc_msg_list_free;
	spinlock_t venc_msg_list_lock;
	struct list_head venc_pmem_list_head;
	spinlock_t venc_pmem_list_lock;
	struct dal_client *q6_handle;
	wait_queue_head_t venc_msg_evt;
	struct device *class_devp;
};

#define DEBUG_VENC 0
#if DEBUG_VENC
#define TRACE(fmt, x...)     \
	do { pr_debug("%s:%d " fmt, __func__, __LINE__, ##x); } while (0)
#else
#define TRACE(fmt, x...)         do { } while (0)
#endif

static struct cdev cdev;
static dev_t venc_dev_num;
static struct class *venc_class;
static struct venc_dev *venc_device_p;
static int venc_ref;

static DEFINE_MUTEX(idlecount_lock);
static int idlecount;
static struct wake_lock wakelock;
static struct pm_qos_request pm_qos_req;

static void prevent_sleep(void)
{
	mutex_lock(&idlecount_lock);
	if (++idlecount == 1) {
		pm_qos_update_request(&pm_qos_req,
				      msm_cpuidle_get_deep_idle_latency());
		wake_lock(&wakelock);
	}
	mutex_unlock(&idlecount_lock);
}

static void allow_sleep(void)
{
	mutex_lock(&idlecount_lock);
	if (--idlecount == 0) {
		wake_unlock(&wakelock);
		pm_qos_update_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);
	}
	mutex_unlock(&idlecount_lock);
}

static inline int venc_check_version(u32 client, u32 server)
{
	int ret = -EINVAL;

	if ((VENC_GET_MAJOR_VERSION(client) == VENC_GET_MAJOR_VERSION(server))
	     && (VENC_GET_MINOR_VERSION(client) <=
		 VENC_GET_MINOR_VERSION(server)))
		ret = 0;

	return ret;
}

static int venc_get_msg(struct venc_dev *dvenc, void *msg)
{
	struct venc_msg_list *l;
	unsigned long flags;
	int ret = 0;
	struct venc_msg qdsp_msg;

	if (!dvenc->is_active)
		return -EPERM;
	spin_lock_irqsave(&dvenc->venc_msg_list_lock, flags);
	list_for_each_entry_reverse(l, &dvenc->venc_msg_list_head, list) {
		memcpy(&qdsp_msg, &l->msg_data, sizeof(struct venc_msg));
		list_del(&l->list);
		list_add(&l->list, &dvenc->venc_msg_list_free);
		ret = 1;
		break;
	}
	spin_unlock_irqrestore(&dvenc->venc_msg_list_lock, flags);
	if (copy_to_user(msg, &qdsp_msg, sizeof(struct venc_msg)))
		pr_err("%s failed to copy_to_user\n", __func__);
	return ret;
}

static void venc_put_msg(struct venc_dev *dvenc, struct venc_msg *msg)
{
	struct venc_msg_list *l;
	unsigned long flags;
	int found = 0;

	spin_lock_irqsave(&dvenc->venc_msg_list_lock, flags);
	list_for_each_entry(l, &dvenc->venc_msg_list_free, list) {
		memcpy(&l->msg_data, msg, sizeof(struct venc_msg));
		list_del(&l->list);
		list_add(&l->list, &dvenc->venc_msg_list_head);
		found = 1;
		break;
	}
	spin_unlock_irqrestore(&dvenc->venc_msg_list_lock, flags);
	if (found)
		wake_up(&dvenc->venc_msg_evt);
	else
		pr_err("%s: failed to find a free node\n", __func__);

}

static struct venc_pmem_list *venc_add_pmem_to_list(struct venc_dev *dvenc,
						      struct venc_pmem *mptr,
						      u32 btype)
{
	int ret = 0;
	unsigned long flags;
	unsigned long len;
	unsigned long vaddr;
	struct venc_pmem_list *plist = NULL;

	plist = kzalloc(sizeof(struct venc_pmem_list), GFP_KERNEL);
	if (!plist) {
		pr_err("%s: kzalloc failed\n", __func__);
		return NULL;
	}

	ret = get_pmem_file(mptr->fd, &(plist->buf.paddr),
		&vaddr, &len, &(plist->buf.file));
	if (ret) {
		pr_err("%s: get_pmem_file failed for fd=%d offset=%d\n",
			__func__, mptr->fd, mptr->offset);
		goto err_venc_add_pmem;
	} else if (mptr->offset >= len) {
		pr_err("%s: invalid offset (%d > %ld) for fd=%d\n",
		       __func__, mptr->offset, len, mptr->fd);
		ret = -EINVAL;
		goto err_venc_get_pmem;
	}

	plist->buf.fd = mptr->fd;
	plist->buf.paddr += mptr->offset;
	plist->buf.size = mptr->size;
	plist->buf.btype = btype;
	plist->buf.offset = mptr->offset;
	plist->buf.src = mptr->src;

	spin_lock_irqsave(&dvenc->venc_pmem_list_lock, flags);
	list_add(&plist->list, &dvenc->venc_pmem_list_head);
	spin_unlock_irqrestore(&dvenc->venc_pmem_list_lock, flags);
	return plist;

err_venc_get_pmem:
	put_pmem_file(plist->buf.file);
err_venc_add_pmem:
	kfree(plist);
	return NULL;
}

static struct venc_pmem_list *venc_get_pmem_from_list(
		struct venc_dev *dvenc, u32 pmem_fd,
		u32 offset, u32 btype)
{
	struct venc_pmem_list *plist;
	unsigned long flags;
	struct file *file;
	int found = 0;

	file = fget(pmem_fd);
	if (!file) {
		pr_err("%s: invalid encoder buffer fd(%d)\n", __func__,
			pmem_fd);
		return NULL;
	}
	spin_lock_irqsave(&dvenc->venc_pmem_list_lock, flags);
	list_for_each_entry(plist, &dvenc->venc_pmem_list_head, list) {
		if (plist->buf.btype == btype && plist->buf.file == file &&
			plist->buf.offset == offset) {
			found = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&dvenc->venc_pmem_list_lock, flags);
	fput(file);
	if (found)
		return plist;

	else
		return NULL;
}

static int venc_set_buffer(struct venc_dev *dvenc, void *argp,
			     u32 btype)
{
	struct venc_pmem pmem;
	struct venc_pmem_list *plist;
	int ret = 0;

	ret = copy_from_user(&pmem, argp, sizeof(pmem));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	plist = venc_add_pmem_to_list(dvenc, &pmem, btype);
	if (plist == NULL) {
		pr_err("%s: buffer add_to_pmem_list failed\n",
			__func__);
		return -EPERM;
	}
	return ret;
}

static int venc_assign_q6_buffers(struct venc_dev *dvenc,
				    struct venc_buffers *pbufs,
				    struct venc_nonio_buf_config *pcfg)
{
	int ret = 0;
	struct venc_pmem_list *plist;

	plist = venc_add_pmem_to_list(dvenc, &(pbufs->recon_buf[0]),
				  VENC_BUFFER_TYPE_QDSP6);
	if (plist == NULL) {
		pr_err("%s: recon_buf0 failed to add_to_pmem_list\n",
			__func__);
		return -EPERM;
	}
	pcfg->recon_buf1.region = pbufs->recon_buf[0].src;
	pcfg->recon_buf1.phys = plist->buf.paddr;
	pcfg->recon_buf1.size = plist->buf.size;
	pcfg->recon_buf1.offset = 0;

	plist = venc_add_pmem_to_list(dvenc, &(pbufs->recon_buf[1]),
				  VENC_BUFFER_TYPE_QDSP6);
	if (plist == NULL) {
		pr_err("%s: recons_buf1 failed to add_to_pmem_list\n",
			__func__);
		return -EPERM;
	}
	pcfg->recon_buf2.region = pbufs->recon_buf[1].src;
	pcfg->recon_buf2.phys = plist->buf.paddr;
	pcfg->recon_buf2.size = plist->buf.size;
	pcfg->recon_buf2.offset = 0;

	plist = venc_add_pmem_to_list(dvenc, &(pbufs->wb_buf),
				  VENC_BUFFER_TYPE_QDSP6);
	if (plist == NULL) {
		pr_err("%s: wb_buf failed to add_to_pmem_list\n",
			__func__);
		return -EPERM;
	}
	pcfg->wb_buf.region = pbufs->wb_buf.src;
	pcfg->wb_buf.phys = plist->buf.paddr;
	pcfg->wb_buf.size = plist->buf.size;
	pcfg->wb_buf.offset = 0;

	plist = venc_add_pmem_to_list(dvenc, &(pbufs->cmd_buf),
				  VENC_BUFFER_TYPE_QDSP6);
	if (plist == NULL) {
		pr_err("%s: cmd_buf failed to add_to_pmem_list\n",
			__func__);
		return -EPERM;
	}
	pcfg->cmd_buf.region = pbufs->cmd_buf.src;
	pcfg->cmd_buf.phys = plist->buf.paddr;
	pcfg->cmd_buf.size = plist->buf.size;
	pcfg->cmd_buf.offset = 0;

	plist = venc_add_pmem_to_list(dvenc, &(pbufs->vlc_buf),
				  VENC_BUFFER_TYPE_QDSP6);
	if (plist == NULL) {
		pr_err("%s: vlc_buf failed to add_to_pmem_list"
		" failed\n", __func__);
		return -EPERM;
	}
	pcfg->vlc_buf.region = pbufs->vlc_buf.src;
	pcfg->vlc_buf.phys = plist->buf.paddr;
	pcfg->vlc_buf.size = plist->buf.size;
	pcfg->vlc_buf.offset = 0;

	return ret;
}

static int venc_start(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	struct venc_q6_config q6_config;
	struct venc_init_config vconfig;

	dvenc->state = VENC_STATE_START;
	ret = copy_from_user(&vconfig, argp, sizeof(struct venc_init_config));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	memcpy(&q6_config, &(vconfig.q6_config), sizeof(q6_config));
	ret = venc_assign_q6_buffers(dvenc, &(vconfig.q6_bufs),
		&(q6_config.buf_params));
	if (ret != 0) {
		pr_err("%s: assign_q6_buffers failed\n", __func__);
		return -EPERM;
	}

	q6_config.callback_event = dvenc->q6_handle;
	TRACE("%s: parameters: handle:%p, config:%p, callback:%p \n", __func__,
		dvenc->q6_handle, &q6_config, q6_config.callback_event);
	TRACE("%s: parameters:recon1:0x%x, recon2:0x%x,"
		" wb_buf:0x%x, cmd:0x%x, vlc:0x%x\n", __func__,
		q6_config.buf_params.recon_buf1.phys,
		q6_config.buf_params.recon_buf2.phys,
		q6_config.buf_params.wb_buf.phys,
		q6_config.buf_params.cmd_buf.phys,
		q6_config.buf_params.vlc_buf.phys);
	TRACE("%s: size of param:%d \n", __func__, sizeof(q6_config));
	ret = dal_call_f5(dvenc->q6_handle, VENC_DALRPC_START, &q6_config,
		sizeof(q6_config));
	if (ret != 0) {
		pr_err("%s: remote function failed (%d)\n", __func__, ret);
		return ret;
	}
	return ret;
}

static int venc_encode_frame(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	struct venc_pmem buf;
	struct venc_input_buf q6_input;
	struct venc_pmem_list *plist;
	struct venc_buffer input;

	ret = copy_from_user(&input, argp, sizeof(struct venc_buffer));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	ret = copy_from_user(&buf,
			       ((struct venc_buffer *)argp)->ptr_buffer,
			       sizeof(struct venc_pmem));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}

	plist = venc_get_pmem_from_list(dvenc, buf.fd, buf.offset,
			VENC_BUFFER_TYPE_INPUT);
	if (NULL == plist) {
		plist = venc_add_pmem_to_list(dvenc, &buf,
			VENC_BUFFER_TYPE_INPUT);
		if (plist == NULL) {
			pr_err("%s: buffer add_to_pmem_list failed\n",
				__func__);
			return -EPERM;
		}
	}

	q6_input.flags = 0;
	if (input.flags & VENC_FLAG_EOS)
		q6_input.flags |= 0x00000001;
	q6_input.yuv_buf.region = plist->buf.src;
	q6_input.yuv_buf.phys = plist->buf.paddr;
	q6_input.yuv_buf.size = plist->buf.size;
	q6_input.yuv_buf.offset = 0;
	q6_input.data_size = plist->buf.size;
	q6_input.client_data = (u32)input.client_data;
	q6_input.time_stamp = input.time_stamp;
	q6_input.dvs_offsetx = 0;
	q6_input.dvs_offsety = 0;

	TRACE("Pushing down input phys=0x%x fd= %d, client_data: 0x%x,"
		" time_stamp:%lld \n", q6_input.yuv_buf.phys, plist->buf.fd,
		input.client_data, input.time_stamp);
	ret = dal_call_f5(dvenc->q6_handle, VENC_DALRPC_QUEUE_INPUT,
		&q6_input, sizeof(q6_input));

	if (ret != 0)
		pr_err("%s: Q6 queue_input failed (%d)\n", __func__,
		(int)ret);
	return ret;
}

static int venc_fill_output(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	struct venc_pmem buf;
	struct venc_output_buf q6_output;
	struct venc_pmem_list *plist;
	struct venc_buffer output;

	ret = copy_from_user(&output, argp, sizeof(struct venc_buffer));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	ret = copy_from_user(&buf,
			       ((struct venc_buffer *)argp)->ptr_buffer,
			       sizeof(struct venc_pmem));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	plist =	venc_get_pmem_from_list(dvenc, buf.fd, buf.offset,
			VENC_BUFFER_TYPE_OUTPUT);
	if (NULL == plist) {
		plist = venc_add_pmem_to_list(dvenc, &buf,
				VENC_BUFFER_TYPE_OUTPUT);
		if (NULL == plist) {
			pr_err("%s: output buffer failed to add_to_pmem_list"
				"\n", __func__);
			return -EPERM;
		}
	}
	q6_output.bit_stream_buf.region = plist->buf.src;
	q6_output.bit_stream_buf.phys = (u32)plist->buf.paddr;
	q6_output.bit_stream_buf.size = plist->buf.size;
	q6_output.bit_stream_buf.offset = 0;
	q6_output.client_data = (u32)output.client_data;
	ret =
	    dal_call_f5(dvenc->q6_handle, VENC_DALRPC_QUEUE_OUTPUT, &q6_output,
			sizeof(q6_output));
	if (ret != 0)
		pr_err("%s: remote function failed (%d)\n", __func__, ret);
	return ret;
}

static int venc_stop(struct venc_dev *dvenc)
{
	int ret = 0;
	struct venc_msg msg;

	ret = dal_call_f0(dvenc->q6_handle, VENC_DALRPC_STOP, 1);
	if (ret) {
		pr_err("%s: remote runction failed (%d)\n", __func__, ret);
		msg.msg_code = VENC_MSG_STOP;
		msg.msg_data_size = 0;
		msg.status_code = VENC_S_EFAIL;
		venc_put_msg(dvenc, &msg);
	}
	return ret;
}

static int venc_pause(struct venc_dev *dvenc)
{
	int ret = 0;
	struct venc_msg msg;

	ret = dal_call_f0(dvenc->q6_handle, VENC_DALRPC_SUSPEND, 1);
	if (ret) {
		pr_err("%s: remote function failed (%d)\n", __func__, ret);
		msg.msg_code = VENC_MSG_PAUSE;
		msg.status_code = VENC_S_EFAIL;
		msg.msg_data_size = 0;
		venc_put_msg(dvenc, &msg);
	}
	return ret;
}

static int venc_resume(struct venc_dev *dvenc)
{
	int ret = 0;
	struct venc_msg msg;

	ret = dal_call_f0(dvenc->q6_handle, VENC_DALRPC_RESUME, 1);
	if (ret) {
		pr_err("%s: remote function failed (%d)\n", __func__, ret);
		msg.msg_code = VENC_MSG_RESUME;
		msg.msg_data_size = 0;
		msg.status_code = VENC_S_EFAIL;
		venc_put_msg(dvenc, &msg);
	}
	return ret;
}

static int venc_flush(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	struct venc_msg msg;
	union venc_msg_data smsg;
	int status = VENC_S_SUCCESS;
	struct venc_buffer_flush flush;

	if (copy_from_user(&flush, argp, sizeof(struct venc_buffer_flush)))
		return -EFAULT;
	if (flush.flush_mode == VENC_FLUSH_ALL) {
		ret = dal_call_f0(dvenc->q6_handle, VENC_DALRPC_FLUSH, 1);
		if (ret)
			status = VENC_S_EFAIL;
	} else
		status = VENC_S_ENOTSUPP;

	if (status != VENC_S_SUCCESS) {
		if ((flush.flush_mode == VENC_FLUSH_INPUT) ||
		     (flush.flush_mode == VENC_FLUSH_ALL)) {
			smsg.flush_ret.flush_mode = VENC_FLUSH_INPUT;
			msg.msg_data = smsg;
			msg.status_code = status;
			msg.msg_code = VENC_MSG_FLUSH;
			msg.msg_data_size = sizeof(union venc_msg_data);
			venc_put_msg(dvenc, &msg);
		}
		if (flush.flush_mode == VENC_FLUSH_OUTPUT ||
		     (flush.flush_mode == VENC_FLUSH_ALL)) {
			smsg.flush_ret.flush_mode = VENC_FLUSH_OUTPUT;
			msg.msg_data = smsg;
			msg.status_code = status;
			msg.msg_code = VENC_MSG_FLUSH;
			msg.msg_data_size = sizeof(union venc_msg_data);
			venc_put_msg(dvenc, &msg);
		}
		return -EIO;
	}
	return ret;
}

static int venc_get_sequence_hdr(struct venc_dev *dvenc, void *argp)
{
	pr_err("%s not supported\n", __func__);
	return -EIO;
}

static int venc_set_qp_range(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	struct venc_qp_range qp;

	ret = copy_from_user(&qp, argp, sizeof(struct venc_qp_range));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}

	if (dvenc->state == VENC_STATE_START ||
		dvenc->state == VENC_STATE_PAUSE) {
		ret =
		    dal_call_f5(dvenc->q6_handle, VENC_DALRPC_UPDATE_QP_RANGE,
				&qp, sizeof(struct venc_qp_range));
		if (ret) {
			pr_err("%s: remote function failed (%d) \n", __func__,
				ret);
			return ret;
		}
	}
	return ret;
}

static int venc_set_intra_period(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	u32 pnum = 0;

	ret = copy_from_user(&pnum, argp, sizeof(int));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	if (dvenc->state == VENC_STATE_START ||
		dvenc->state == VENC_STATE_PAUSE) {
		ret = dal_call_f0(dvenc->q6_handle,
			VENC_DALRPC_UPDATE_INTRA_PERIOD, pnum);
		if (ret)
			pr_err("%s: remote function failed (%d)\n", __func__,
				ret);
	}
	return ret;
}

static int venc_set_intra_refresh(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	u32 mb_num = 0;

	ret = copy_from_user(&mb_num, argp, sizeof(int));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	if (dvenc->state == VENC_STATE_START ||
		dvenc->state == VENC_STATE_PAUSE) {
		ret = dal_call_f0(dvenc->q6_handle,
			VENC_DALRPC_UPDATE_INTRA_REFRESH, mb_num);
		if (ret)
			pr_err("%s: remote function failed (%d)\n", __func__,
				ret);
	}
	return ret;
}

static int venc_set_frame_rate(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	struct venc_frame_rate pdata;
	ret = copy_from_user(&pdata, argp, sizeof(struct venc_frame_rate));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	if (dvenc->state == VENC_STATE_START ||
		dvenc->state == VENC_STATE_PAUSE) {
		ret = dal_call_f5(dvenc->q6_handle,
				VENC_DALRPC_UPDATE_FRAME_RATE,
				(void *)&(pdata),
				sizeof(struct venc_frame_rate));
		if (ret)
			pr_err("%s: remote function failed (%d)\n", __func__,
				ret);
	}
	return ret;
}

static int venc_set_target_bitrate(struct venc_dev *dvenc, void *argp)
{
	int ret = 0;
	u32 pdata = 0;

	ret = copy_from_user(&pdata, argp, sizeof(int));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	if (dvenc->state == VENC_STATE_START ||
		dvenc->state == VENC_STATE_PAUSE) {
		ret = dal_call_f0(dvenc->q6_handle,
			VENC_DALRPC_UPDATE_BITRATE, pdata);
		if (ret)
			pr_err("%s: remote function failed (%d)\n", __func__,
				ret);
	}
	return ret;
}

static int venc_request_iframe(struct venc_dev *dvenc)
{
	int ret = 0;

	if (dvenc->state != VENC_STATE_START)
		return -EINVAL;

	ret = dal_call_f0(dvenc->q6_handle, VENC_DALRPC_REQUEST_IFRAME, 1);
	if (ret)
		pr_err("%s: remote function failed (%d)\n", __func__, ret);
	return ret;
}

static int venc_stop_read_msg(struct venc_dev *dvenc)
{
	struct venc_msg msg;
	int ret = 0;

	msg.status_code = 0;
	msg.msg_code = VENC_MSG_STOP_READING_MSG;
	msg.msg_data_size = 0;
	venc_put_msg(dvenc, &msg);
	return ret;
}

static int venc_q6_stop(struct venc_dev *dvenc)
{
	int ret = 0;
	struct venc_pmem_list *plist;
	unsigned long flags;

	wake_up(&dvenc->venc_msg_evt);
	spin_lock_irqsave(&dvenc->venc_pmem_list_lock, flags);
	if (!dvenc->pmem_freed) {
		list_for_each_entry(plist, &dvenc->venc_pmem_list_head, list)
			put_pmem_file(plist->buf.file);
		dvenc->pmem_freed = 1;
	}
	spin_unlock_irqrestore(&dvenc->venc_pmem_list_lock, flags);

	dvenc->state = VENC_STATE_STOP;
	return ret;
}

static int venc_translate_error(enum venc_status_code q6_status)
{
	int ret = 0;

	switch (q6_status) {
	case VENC_STATUS_SUCCESS:
		ret = VENC_S_SUCCESS;
		break;
	case VENC_STATUS_ERROR:
		ret = VENC_S_EFAIL;
		break;
	case VENC_STATUS_INVALID_STATE:
		ret = VENC_S_EINVALSTATE;
		break;
	case VENC_STATUS_FLUSHING:
		ret = VENC_S_EFLUSHED;
		break;
	case VENC_STATUS_INVALID_PARAM:
		ret = VENC_S_EBADPARAM;
		break;
	case VENC_STATUS_CMD_QUEUE_FULL:
		ret = VENC_S_ECMDQFULL;
		break;
	case VENC_STATUS_CRITICAL:
		ret = VENC_S_EFATAL;
		break;
	case VENC_STATUS_INSUFFICIENT_RESOURCES:
		ret = VENC_S_ENOHWRES;
		break;
	case VENC_STATUS_TIMEOUT:
		ret = VENC_S_ETIMEOUT;
		break;
	}
	if (q6_status != VENC_STATUS_SUCCESS)
		pr_err("%s: Q6 failed (%d)", __func__, (int)q6_status);
	return ret;
}

static void venc_q6_callback(void *data, int len, void *cookie)
{
	int status = 0;
	struct venc_dev *dvenc = (struct venc_dev *)cookie;
	struct venc_msg_type *q6_msg = NULL;
	struct venc_msg msg, msg1;
	union venc_msg_data smsg1, smsg2;
	unsigned long msg_code = 0;
	struct venc_input_payload *pload1;
	struct venc_output_payload *pload2;
	uint32_t * tmp = (uint32_t *) data;

	if (dvenc == NULL) {
		pr_err("%s: empty driver parameter\n", __func__);
		return;
	}
	if (tmp[2] == sizeof(struct venc_msg_type)) {
		q6_msg = (struct venc_msg_type *)&tmp[3];
	} else {
		pr_err("%s: callback with empty message (%d, %d)\n",
			__func__, tmp[2], sizeof(struct venc_msg_type));
		return;
	}
	msg.msg_data_size = 0;
	status = venc_translate_error(q6_msg->status);
	switch ((enum venc_event_type_enum)q6_msg->event) {
	case VENC_EVENT_START_STATUS:
		dvenc->state = VENC_STATE_START;
		msg_code = VENC_MSG_START;
		break;
	case VENC_EVENT_STOP_STATUS:
		venc_q6_stop(dvenc);
		msg_code = VENC_MSG_STOP;
		break;
	case VENC_EVENT_SUSPEND_STATUS:
		dvenc->state = VENC_STATE_PAUSE;
		msg_code = VENC_MSG_PAUSE;
		break;
	case VENC_EVENT_RESUME_STATUS:
		dvenc->state = VENC_STATE_START;
		msg_code = VENC_MSG_RESUME;
		break;
	case VENC_EVENT_FLUSH_STATUS:
		smsg1.flush_ret.flush_mode = VENC_FLUSH_INPUT;
		msg1.status_code = status;
		msg1.msg_code = VENC_MSG_FLUSH;
		msg1.msg_data = smsg1;
		msg1.msg_data_size = sizeof(union venc_msg_data);
		venc_put_msg(dvenc, &msg1);
		smsg2.flush_ret.flush_mode = VENC_FLUSH_OUTPUT;
		msg_code = VENC_MSG_FLUSH;
		msg.msg_data = smsg2;
		msg.msg_data_size = sizeof(union venc_msg_data);
		break;
	case VENC_EVENT_RELEASE_INPUT:
		pload1 = &((q6_msg->payload).input_payload);
		TRACE("Release_input: data: 0x%x \n", pload1->data);
		if (pload1 != NULL) {
			msg.msg_data.buf.client_data = pload1->data;
			msg_code = VENC_MSG_INPUT_BUFFER_DONE;
			msg.msg_data_size = sizeof(union venc_msg_data);
		}
		break;
	case VENC_EVENT_DELIVER_OUTPUT:
		pload2 = &((q6_msg->payload).output_payload);
		smsg1.buf.flags = 0;
		if (pload2->flags & VENC_FLAG_SYNC_FRAME)
			smsg1.buf.flags |= VENC_FLAG_SYNC_FRAME;
		if (pload2->flags & VENC_FLAG_CODEC_CONFIG)
			smsg1.buf.flags |= VENC_FLAG_CODEC_CONFIG;
		if (pload2->flags & VENC_FLAG_END_OF_FRAME)
			smsg1.buf.flags |= VENC_FLAG_END_OF_FRAME;
		if (pload2->flags & VENC_FLAG_EOS)
			smsg1.buf.flags |= VENC_FLAG_EOS;
		smsg1.buf.len = pload2->size;
		smsg1.buf.offset = 0;
		smsg1.buf.time_stamp = pload2->time_stamp;
		smsg1.buf.client_data = pload2->data;
		msg_code = VENC_MSG_OUTPUT_BUFFER_DONE;
		msg.msg_data = smsg1;
		msg.msg_data_size = sizeof(union venc_msg_data);
		break;
	default:
		pr_err("%s: invalid response from Q6 (%d)\n", __func__,
			(int)q6_msg->event);
		return;
	}
	msg.status_code = status;
	msg.msg_code = msg_code;
	venc_put_msg(dvenc, &msg);
	return;
}

static int venc_get_version(struct venc_dev *dvenc, void *argp)
{
	struct venc_version ver_info;
	int ret = 0;

	ver_info.major = VENC_GET_MAJOR_VERSION(VENC_INTERFACE_VERSION);
	ver_info.minor = VENC_GET_MINOR_VERSION(VENC_INTERFACE_VERSION);

	ret = copy_to_user(((struct venc_version *)argp),
				&ver_info, sizeof(ver_info));
	if (ret)
		pr_err("%s failed to copy_to_user\n", __func__);

	return ret;

}

static long q6venc_ioctl(struct file *file, u32 cmd,
			   unsigned long arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct venc_dev *dvenc = file->private_data;

	if (!dvenc || !dvenc->is_active)
		return -EPERM;

	switch (cmd) {
	case VENC_IOCTL_SET_INPUT_BUFFER:
		ret = venc_set_buffer(dvenc, argp, VENC_BUFFER_TYPE_INPUT);
		break;
	case VENC_IOCTL_SET_OUTPUT_BUFFER:
		ret = venc_set_buffer(dvenc, argp, VENC_BUFFER_TYPE_OUTPUT);
		break;
	case VENC_IOCTL_GET_SEQUENCE_HDR:
		ret = venc_get_sequence_hdr(dvenc, argp);
		break;
	case VENC_IOCTL_SET_QP_RANGE:
		ret = venc_set_qp_range(dvenc, argp);
		break;
	case VENC_IOCTL_SET_INTRA_PERIOD:
		ret = venc_set_intra_period(dvenc, argp);
		break;
	case VENC_IOCTL_SET_INTRA_REFRESH:
		ret = venc_set_intra_refresh(dvenc, argp);
		break;
	case VENC_IOCTL_SET_FRAME_RATE:
		ret = venc_set_frame_rate(dvenc, argp);
		break;
	case VENC_IOCTL_SET_TARGET_BITRATE:
		ret = venc_set_target_bitrate(dvenc, argp);
		break;
	case VENC_IOCTL_CMD_REQUEST_IFRAME:
		if (dvenc->state == VENC_STATE_START)
			ret = venc_request_iframe(dvenc);
		break;
	case VENC_IOCTL_CMD_START:
		ret = venc_start(dvenc, argp);
		break;
	case VENC_IOCTL_CMD_STOP:
		ret = venc_stop(dvenc);
		break;
	case VENC_IOCTL_CMD_PAUSE:
		ret = venc_pause(dvenc);
		break;
	case VENC_IOCTL_CMD_RESUME:
		ret = venc_resume(dvenc);
		break;
	case VENC_IOCTL_CMD_ENCODE_FRAME:
		ret = venc_encode_frame(dvenc, argp);
		break;
	case VENC_IOCTL_CMD_FILL_OUTPUT_BUFFER:
		ret = venc_fill_output(dvenc, argp);
		break;
	case VENC_IOCTL_CMD_FLUSH:
		ret = venc_flush(dvenc, argp);
		break;
	case VENC_IOCTL_CMD_READ_NEXT_MSG:
		wait_event_interruptible(dvenc->venc_msg_evt,
					  venc_get_msg(dvenc, argp));
		break;
	case VENC_IOCTL_CMD_STOP_READ_MSG:
		ret = venc_stop_read_msg(dvenc);
		break;
	case VENC_IOCTL_GET_VERSION:
		ret = venc_get_version(dvenc, argp);
		break;
	default:
		pr_err("%s: invalid ioctl code (%d)\n", __func__, cmd);
		ret = -ENOTTY;
		break;
	}
	return ret;
}

static int q6venc_open(struct inode *inode, struct file *file)
{
	int i;
	int ret = 0;
	struct venc_dev *dvenc;
	struct venc_msg_list *plist, *tmp;
	struct dal_info version_info;

	dvenc = kzalloc(sizeof(struct venc_dev), GFP_KERNEL);
	if (!dvenc) {
		pr_err("%s: unable to allocate memory for struct venc_dev\n",
			__func__);
		return -ENOMEM;
	}
	file->private_data = dvenc;
	INIT_LIST_HEAD(&dvenc->venc_msg_list_head);
	INIT_LIST_HEAD(&dvenc->venc_msg_list_free);
	INIT_LIST_HEAD(&dvenc->venc_pmem_list_head);
	init_waitqueue_head(&dvenc->venc_msg_evt);
	spin_lock_init(&dvenc->venc_msg_list_lock);
	spin_lock_init(&dvenc->venc_pmem_list_lock);
	venc_ref++;
	for (i = 0; i < VENC_MSG_MAX; i++) {
		plist = kzalloc(sizeof(struct venc_msg_list), GFP_KERNEL);
		if (!plist) {
			pr_err("%s: kzalloc failed\n", __func__);
			ret = -ENOMEM;
			goto err_venc_create_msg_list;
		}
		list_add(&plist->list, &dvenc->venc_msg_list_free);
	}
	dvenc->q6_handle =
	    dal_attach(DALDEVICEID_VENC_DEVICE, DALDEVICEID_VENC_PORTNAME, 1,
		       venc_q6_callback, (void *)dvenc);
	if (!(dvenc->q6_handle)) {
		pr_err("%s: daldevice_attach failed (%d)\n", __func__, ret);
		goto err_venc_dal_attach;
	}
	ret = dal_call_f9(dvenc->q6_handle, DAL_OP_INFO, &version_info,
		sizeof(struct dal_info));
	if (ret) {
		pr_err("%s: failed to get version\n", __func__);
		goto err_venc_dal_open;
	}
	if (venc_check_version(VENC_INTERFACE_VERSION, version_info.version)) {
		pr_err("%s: driver version mismatch\n", __func__);
		goto err_venc_dal_open;
	}
	ret = dal_call_f0(dvenc->q6_handle, DAL_OP_OPEN, 1);
	if (ret) {
		pr_err("%s: dal_call_open failed (%d)\n", __func__, ret);
		goto err_venc_dal_open;
	}
	dvenc->state = VENC_STATE_STOP;
	dvenc->is_active = 1;
	prevent_sleep();
	return ret;
err_venc_dal_open:
	dal_detach(dvenc->q6_handle);
err_venc_dal_attach:
	list_for_each_entry_safe(plist, tmp, &dvenc->venc_msg_list_free, list) {
		list_del(&plist->list);
		kfree(plist);
	}
err_venc_create_msg_list:
	kfree(dvenc);
	venc_ref--;
	return ret;
}

static int q6venc_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct venc_msg_list *l, *n;
	struct venc_pmem_list *plist, *m;
	struct venc_dev *dvenc;
	unsigned long flags;

	venc_ref--;
	dvenc = file->private_data;
	dvenc->is_active = 0;
	wake_up_all(&dvenc->venc_msg_evt);
	dal_call_f0(dvenc->q6_handle, VENC_DALRPC_STOP, 1);
	dal_call_f0(dvenc->q6_handle, DAL_OP_CLOSE, 1);
	dal_detach(dvenc->q6_handle);
	list_for_each_entry_safe(l, n, &dvenc->venc_msg_list_free, list) {
		list_del(&l->list);
		kfree(l);
	}
	list_for_each_entry_safe(l, n, &dvenc->venc_msg_list_head, list) {
		list_del(&l->list);
		kfree(l);
	}
	spin_lock_irqsave(&dvenc->venc_pmem_list_lock, flags);
	if (!dvenc->pmem_freed) {
		list_for_each_entry(plist, &dvenc->venc_pmem_list_head, list)
			put_pmem_file(plist->buf.file);
		dvenc->pmem_freed = 1;
	}
	spin_unlock_irqrestore(&dvenc->venc_pmem_list_lock, flags);

	list_for_each_entry_safe(plist, m, &dvenc->venc_pmem_list_head, list) {
		list_del(&plist->list);
		kfree(plist);
	}
	kfree(dvenc);
	allow_sleep();
	return ret;
}

const struct file_operations q6venc_fops = {
	.owner = THIS_MODULE,
	.open = q6venc_open,
	.release = q6venc_release,
	.unlocked_ioctl = q6venc_ioctl,
};

static int __init q6venc_init(void)
{
	int ret = 0;

	pm_qos_add_request(&pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "venc_suspend");

	venc_device_p = kzalloc(sizeof(struct venc_dev), GFP_KERNEL);
	if (!venc_device_p) {
		pr_err("%s: unable to allocate memory for venc_device_p\n",
			__func__);
		return -ENOMEM;
	}
	ret = alloc_chrdev_region(&venc_dev_num, 0, 1, VENC_NAME);
	if (ret < 0) {
		pr_err("%s: alloc_chrdev_region failed (%d)\n", __func__,
			ret);
		return ret;
	}
	venc_class = class_create(THIS_MODULE, VENC_NAME);
	if (IS_ERR(venc_class)) {
		ret = PTR_ERR(venc_class);
		pr_err("%s: failed to create venc_class (%d)\n",
			__func__, ret);
		goto err_venc_class_create;
	}
	venc_device_p->class_devp =
	    device_create(venc_class, NULL, venc_dev_num, NULL,
			  VENC_NAME);
	if (IS_ERR(venc_device_p->class_devp)) {
		ret = PTR_ERR(venc_device_p->class_devp);
		pr_err("%s: failed to create class_device (%d)\n", __func__,
			ret);
		goto err_venc_class_device_create;
	}
	cdev_init(&cdev, &q6venc_fops);
	cdev.owner = THIS_MODULE;
	ret = cdev_add(&cdev, venc_dev_num, 1);
	if (ret < 0) {
		pr_err("%s: cdev_add failed (%d)\n", __func__, ret);
		goto err_venc_cdev_add;
	}
	init_waitqueue_head(&venc_device_p->venc_msg_evt);
	return ret;

err_venc_cdev_add:
	device_destroy(venc_class, venc_dev_num);
err_venc_class_device_create:
	class_destroy(venc_class);
err_venc_class_create:
	unregister_chrdev_region(venc_dev_num, 1);
	return ret;
}

static void __exit q6venc_exit(void)
{
	cdev_del(&(cdev));
	device_destroy(venc_class, venc_dev_num);
	class_destroy(venc_class);
	unregister_chrdev_region(venc_dev_num, 1);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Video encoder driver for QDSP6");
MODULE_VERSION("2.0");
module_init(q6venc_init);
module_exit(q6venc_exit);

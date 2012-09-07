/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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


#include <linux/module.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <media/msm_jpeg.h>
#include "msm_jpeg_sync.h"
#include "msm_jpeg_core.h"
#include "msm_jpeg_platform.h"
#include "msm_jpeg_common.h"

static int release_buf;

inline void msm_jpeg_q_init(char const *name, struct msm_jpeg_q *q_p)
{
	JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, name);
	q_p->name = name;
	spin_lock_init(&q_p->lck);
	INIT_LIST_HEAD(&q_p->q);
	init_waitqueue_head(&q_p->wait);
	q_p->unblck = 0;
}

inline void *msm_jpeg_q_out(struct msm_jpeg_q *q_p)
{
	unsigned long flags;
	struct msm_jpeg_q_entry *q_entry_p = NULL;
	void *data = NULL;

	JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	spin_lock_irqsave(&q_p->lck, flags);
	if (!list_empty(&q_p->q)) {
		q_entry_p = list_first_entry(&q_p->q, struct msm_jpeg_q_entry,
			list);
		list_del_init(&q_entry_p->list);
	}
	spin_unlock_irqrestore(&q_p->lck, flags);

	if (q_entry_p) {
		data = q_entry_p->data;
		kfree(q_entry_p);
	} else {
		JPEG_DBG("%s:%d] %s no entry\n", __func__, __LINE__,
			q_p->name);
	}

	return data;
}

inline int msm_jpeg_q_in(struct msm_jpeg_q *q_p, void *data)
{
	unsigned long flags;

	struct msm_jpeg_q_entry *q_entry_p;

	JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);

	q_entry_p = kmalloc(sizeof(struct msm_jpeg_q_entry), GFP_ATOMIC);
	if (!q_entry_p) {
		JPEG_PR_ERR("%s: no mem\n", __func__);
		return -EFAULT;
	}
	q_entry_p->data = data;

	spin_lock_irqsave(&q_p->lck, flags);
	list_add_tail(&q_entry_p->list, &q_p->q);
	spin_unlock_irqrestore(&q_p->lck, flags);

	return 0;
}

inline int msm_jpeg_q_in_buf(struct msm_jpeg_q *q_p,
	struct msm_jpeg_core_buf *buf)
{
	struct msm_jpeg_core_buf *buf_p;

	JPEG_DBG("%s:%d]\n", __func__, __LINE__);
	buf_p = kmalloc(sizeof(struct msm_jpeg_core_buf), GFP_ATOMIC);
	if (!buf_p) {
		JPEG_PR_ERR("%s: no mem\n", __func__);
		return -EFAULT;
	}

	memcpy(buf_p, buf, sizeof(struct msm_jpeg_core_buf));

	msm_jpeg_q_in(q_p, buf_p);
	return 0;
}

inline int msm_jpeg_q_wait(struct msm_jpeg_q *q_p)
{
	int tm = MAX_SCHEDULE_TIMEOUT; /* 500ms */
	int rc;

	JPEG_DBG("%s:%d] %s wait\n", __func__, __LINE__, q_p->name);
	rc = wait_event_interruptible_timeout(q_p->wait,
		(!list_empty_careful(&q_p->q) || q_p->unblck),
		msecs_to_jiffies(tm));
	JPEG_DBG("%s:%d] %s wait done\n", __func__, __LINE__, q_p->name);
	if (list_empty_careful(&q_p->q)) {
		if (rc == 0) {
			rc = -ETIMEDOUT;
			JPEG_PR_ERR("%s:%d] %s timeout\n", __func__, __LINE__,
				q_p->name);
		} else if (q_p->unblck) {
			JPEG_DBG("%s:%d] %s unblock is true\n", __func__,
				__LINE__, q_p->name);
			q_p->unblck = 0;
			rc = -ECANCELED;
		} else if (rc < 0) {
			JPEG_PR_ERR("%s:%d] %s rc %d\n", __func__, __LINE__,
				q_p->name, rc);
		}
	}
	return rc;
}

inline int msm_jpeg_q_wakeup(struct msm_jpeg_q *q_p)
{
	JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	wake_up(&q_p->wait);
	return 0;
}

inline int msm_jpeg_q_unblock(struct msm_jpeg_q *q_p)
{
	JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	q_p->unblck = 1;
	wake_up(&q_p->wait);
	return 0;
}

inline void msm_jpeg_outbuf_q_cleanup(struct msm_jpeg_q *q_p,
				int domain_num)
{
	struct msm_jpeg_core_buf *buf_p;
	JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	do {
		buf_p = msm_jpeg_q_out(q_p);
		if (buf_p) {
			msm_jpeg_platform_p2v(buf_p->file,
				&buf_p->handle, domain_num);
			JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
			kfree(buf_p);
		}
	} while (buf_p);
	q_p->unblck = 0;
}

inline void msm_jpeg_q_cleanup(struct msm_jpeg_q *q_p)
{
	void *data;
	JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	do {
		data = msm_jpeg_q_out(q_p);
		if (data) {
			JPEG_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
			kfree(data);
		}
	} while (data);
	q_p->unblck = 0;
}

/*************** event queue ****************/

int msm_jpeg_framedone_irq(struct msm_jpeg_device *pgmn_dev,
	struct msm_jpeg_core_buf *buf_in)
{
	int rc = 0;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);

	if (buf_in) {
		buf_in->vbuf.framedone_len = buf_in->framedone_len;
		buf_in->vbuf.type = MSM_JPEG_EVT_SESSION_DONE;
		JPEG_DBG("%s:%d] 0x%08x %d framedone_len %d\n",
			__func__, __LINE__,
			(int) buf_in->y_buffer_addr, buf_in->y_len,
			buf_in->vbuf.framedone_len);
		rc = msm_jpeg_q_in_buf(&pgmn_dev->evt_q, buf_in);
	} else {
		JPEG_PR_ERR("%s:%d] no output return buffer\n",
			__func__, __LINE__);
		rc = -1;
	}

	if (buf_in)
		rc = msm_jpeg_q_wakeup(&pgmn_dev->evt_q);

	return rc;
}

int msm_jpeg_evt_get(struct msm_jpeg_device *pgmn_dev,
	void __user *to)
{
	struct msm_jpeg_core_buf *buf_p;
	struct msm_jpeg_ctrl_cmd ctrl_cmd;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);

	msm_jpeg_q_wait(&pgmn_dev->evt_q);
	buf_p = msm_jpeg_q_out(&pgmn_dev->evt_q);

	if (!buf_p) {
		JPEG_DBG("%s:%d] no buffer\n", __func__, __LINE__);
		return -EAGAIN;
	}

	ctrl_cmd.type = buf_p->vbuf.type;
	kfree(buf_p);

	JPEG_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
		(int) ctrl_cmd.value, ctrl_cmd.len);

	if (copy_to_user(to, &ctrl_cmd, sizeof(ctrl_cmd))) {
		JPEG_PR_ERR("%s:%d]\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int msm_jpeg_evt_get_unblock(struct msm_jpeg_device *pgmn_dev)
{
	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_jpeg_q_unblock(&pgmn_dev->evt_q);
	return 0;
}

void msm_jpeg_reset_ack_irq(struct msm_jpeg_device *pgmn_dev)
{
	JPEG_DBG("%s:%d]\n", __func__, __LINE__);
}

void msm_jpeg_err_irq(struct msm_jpeg_device *pgmn_dev,
	int event)
{
	int rc = 0;
	struct msm_jpeg_core_buf buf;

	JPEG_PR_ERR("%s:%d] error: %d\n", __func__, __LINE__, event);

	buf.vbuf.type = MSM_JPEG_EVT_ERR;
	rc = msm_jpeg_q_in_buf(&pgmn_dev->evt_q, &buf);
	if (!rc)
		rc = msm_jpeg_q_wakeup(&pgmn_dev->evt_q);

	if (!rc)
		JPEG_PR_ERR("%s:%d] err err\n", __func__, __LINE__);

	return;
}

/*************** output queue ****************/

int msm_jpeg_we_pingpong_irq(struct msm_jpeg_device *pgmn_dev,
	struct msm_jpeg_core_buf *buf_in)
{
	int rc = 0;
	struct msm_jpeg_core_buf *buf_out;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (buf_in) {
		JPEG_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
			(int) buf_in->y_buffer_addr, buf_in->y_len);
		rc = msm_jpeg_q_in_buf(&pgmn_dev->output_rtn_q, buf_in);
	} else {
		JPEG_DBG("%s:%d] no output return buffer\n", __func__,
			__LINE__);
		rc = -1;
		return rc;
	}

	buf_out = msm_jpeg_q_out(&pgmn_dev->output_buf_q);

	if (buf_out) {
		JPEG_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
			(int) buf_out->y_buffer_addr, buf_out->y_len);
		rc = msm_jpeg_core_we_buf_update(buf_out);
		kfree(buf_out);
	} else {
		msm_jpeg_core_we_buf_reset(buf_in);
		JPEG_DBG("%s:%d] no output buffer\n", __func__, __LINE__);
		rc = -2;
	}

	if (buf_in)
		rc = msm_jpeg_q_wakeup(&pgmn_dev->output_rtn_q);

	return rc;
}

int msm_jpeg_output_get(struct msm_jpeg_device *pgmn_dev, void __user *to)
{
	struct msm_jpeg_core_buf *buf_p;
	struct msm_jpeg_buf buf_cmd;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);

	msm_jpeg_q_wait(&pgmn_dev->output_rtn_q);
	buf_p = msm_jpeg_q_out(&pgmn_dev->output_rtn_q);

	if (!buf_p) {
		JPEG_DBG("%s:%d] no output buffer return\n",
			__func__, __LINE__);
		return -EAGAIN;
	}

	buf_cmd = buf_p->vbuf;
	msm_jpeg_platform_p2v(buf_p->file, &buf_p->handle,
		pgmn_dev->domain_num);
	kfree(buf_p);

	JPEG_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
		(int) buf_cmd.vaddr, buf_cmd.y_len);

	if (copy_to_user(to, &buf_cmd, sizeof(buf_cmd))) {
		JPEG_PR_ERR("%s:%d]", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int msm_jpeg_output_get_unblock(struct msm_jpeg_device *pgmn_dev)
{
	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_jpeg_q_unblock(&pgmn_dev->output_rtn_q);
	return 0;
}

int msm_jpeg_output_buf_enqueue(struct msm_jpeg_device *pgmn_dev,
	void __user *arg)
{
	struct msm_jpeg_buf buf_cmd;
	struct msm_jpeg_core_buf *buf_p;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (copy_from_user(&buf_cmd, arg, sizeof(struct msm_jpeg_buf))) {
		JPEG_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	buf_p = kmalloc(sizeof(struct msm_jpeg_core_buf), GFP_ATOMIC);
	if (!buf_p) {
		JPEG_PR_ERR("%s:%d] no mem\n", __func__, __LINE__);
		return -EFAULT;
	}

	JPEG_DBG("%s:%d] vaddr = 0x%08x y_len = %d\n, fd = %d",
		__func__, __LINE__, (int) buf_cmd.vaddr, buf_cmd.y_len,
		buf_cmd.fd);

	buf_p->y_buffer_addr = msm_jpeg_platform_v2p(buf_cmd.fd,
		buf_cmd.y_len, &buf_p->file, &buf_p->handle,
		pgmn_dev->domain_num);
	if (!buf_p->y_buffer_addr) {
		JPEG_PR_ERR("%s:%d] v2p wrong\n", __func__, __LINE__);
		kfree(buf_p);
		return -EFAULT;
	}
	JPEG_DBG("%s:%d]After v2p y_address =0x%08x, handle = %p\n",
		__func__, __LINE__, buf_p->y_buffer_addr, buf_p->handle);
	buf_p->y_len = buf_cmd.y_len;
	buf_p->vbuf = buf_cmd;

	msm_jpeg_q_in(&pgmn_dev->output_buf_q, buf_p);
	return 0;
}

/*************** input queue ****************/

int msm_jpeg_fe_pingpong_irq(struct msm_jpeg_device *pgmn_dev,
	struct msm_jpeg_core_buf *buf_in)
{
	struct msm_jpeg_core_buf *buf_out;
	int rc = 0;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (buf_in) {
		JPEG_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
			(int) buf_in->y_buffer_addr, buf_in->y_len);
		rc = msm_jpeg_q_in_buf(&pgmn_dev->input_rtn_q, buf_in);
	} else {
		JPEG_DBG("%s:%d] no input return buffer\n", __func__,
			__LINE__);
		rc = -EFAULT;
	}

	buf_out = msm_jpeg_q_out(&pgmn_dev->input_buf_q);

	if (buf_out) {
		rc = msm_jpeg_core_fe_buf_update(buf_out);
		kfree(buf_out);
		msm_jpeg_core_fe_start();
	} else {
		JPEG_DBG("%s:%d] no input buffer\n", __func__, __LINE__);
		rc = -EFAULT;
	}

	if (buf_in)
		rc = msm_jpeg_q_wakeup(&pgmn_dev->input_rtn_q);

	return rc;
}

int msm_jpeg_input_get(struct msm_jpeg_device *pgmn_dev, void __user *to)
{
	struct msm_jpeg_core_buf *buf_p;
	struct msm_jpeg_buf buf_cmd;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_jpeg_q_wait(&pgmn_dev->input_rtn_q);
	buf_p = msm_jpeg_q_out(&pgmn_dev->input_rtn_q);

	if (!buf_p) {
		JPEG_DBG("%s:%d] no input buffer return\n",
			__func__, __LINE__);
		return -EAGAIN;
	}

	buf_cmd = buf_p->vbuf;
	msm_jpeg_platform_p2v(buf_p->file, &buf_p->handle,
					pgmn_dev->domain_num);
	kfree(buf_p);

	JPEG_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
		(int) buf_cmd.vaddr, buf_cmd.y_len);

	if (copy_to_user(to, &buf_cmd, sizeof(buf_cmd))) {
		JPEG_PR_ERR("%s:%d]\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int msm_jpeg_input_get_unblock(struct msm_jpeg_device *pgmn_dev)
{
	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_jpeg_q_unblock(&pgmn_dev->input_rtn_q);
	return 0;
}

int msm_jpeg_input_buf_enqueue(struct msm_jpeg_device *pgmn_dev,
	void __user *arg)
{
	struct msm_jpeg_core_buf *buf_p;
	struct msm_jpeg_buf buf_cmd;

	if (copy_from_user(&buf_cmd, arg, sizeof(struct msm_jpeg_buf))) {
		JPEG_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	buf_p = kmalloc(sizeof(struct msm_jpeg_core_buf), GFP_ATOMIC);
	if (!buf_p) {
		JPEG_PR_ERR("%s:%d] no mem\n", __func__, __LINE__);
		return -EFAULT;
	}

	JPEG_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
		(int) buf_cmd.vaddr, buf_cmd.y_len);

	buf_p->y_buffer_addr    = msm_jpeg_platform_v2p(buf_cmd.fd,
		buf_cmd.y_len + buf_cmd.cbcr_len, &buf_p->file,
		&buf_p->handle, pgmn_dev->domain_num) + buf_cmd.offset
		+ buf_cmd.y_off;
	buf_p->y_len          = buf_cmd.y_len;
	buf_p->cbcr_buffer_addr = buf_p->y_buffer_addr + buf_cmd.y_len
						+ buf_cmd.cbcr_off;
	buf_p->cbcr_len       = buf_cmd.cbcr_len;
	buf_p->num_of_mcu_rows = buf_cmd.num_of_mcu_rows;
	JPEG_DBG("%s: y_addr=%x, y_len=%x, cbcr_addr=%x, cbcr_len=%x, fd =%d\n",
		__func__, buf_p->y_buffer_addr, buf_p->y_len,
		buf_p->cbcr_buffer_addr, buf_p->cbcr_len, buf_cmd.fd);

	if (!buf_p->y_buffer_addr || !buf_p->cbcr_buffer_addr) {
		JPEG_PR_ERR("%s:%d] v2p wrong\n", __func__, __LINE__);
		kfree(buf_p);
		return -EFAULT;
	}
	buf_p->vbuf           = buf_cmd;

	msm_jpeg_q_in(&pgmn_dev->input_buf_q, buf_p);

	return 0;
}

int msm_jpeg_irq(int event, void *context, void *data)
{
	struct msm_jpeg_device *pgmn_dev =
		(struct msm_jpeg_device *) context;

	switch (event) {
	case MSM_JPEG_EVT_SESSION_DONE:
		msm_jpeg_framedone_irq(pgmn_dev, data);
		msm_jpeg_we_pingpong_irq(pgmn_dev, data);
		break;

	case MSM_JPEG_HW_MASK_COMP_FE:
		msm_jpeg_fe_pingpong_irq(pgmn_dev, data);
		break;

	case MSM_JPEG_HW_MASK_COMP_WE:
		msm_jpeg_we_pingpong_irq(pgmn_dev, data);
		break;

	case MSM_JPEG_HW_MASK_COMP_RESET_ACK:
		msm_jpeg_reset_ack_irq(pgmn_dev);
		break;

	case MSM_JPEG_HW_MASK_COMP_ERR:
	default:
		msm_jpeg_err_irq(pgmn_dev, event);
		break;
	}

	return 0;
}

int __msm_jpeg_open(struct msm_jpeg_device *pgmn_dev)
{
	int rc;

	mutex_lock(&pgmn_dev->lock);
	if (pgmn_dev->open_count) {
		/* only open once */
		JPEG_PR_ERR("%s:%d] busy\n", __func__, __LINE__);
		mutex_unlock(&pgmn_dev->lock);
		return -EBUSY;
	}
	pgmn_dev->open_count++;
	mutex_unlock(&pgmn_dev->lock);

	msm_jpeg_core_irq_install(msm_jpeg_irq);
	rc = msm_jpeg_platform_init(pgmn_dev->pdev,
		&pgmn_dev->mem, &pgmn_dev->base,
		&pgmn_dev->irq, msm_jpeg_core_irq, pgmn_dev);
	if (rc) {
		JPEG_PR_ERR("%s:%d] platform_init fail %d\n", __func__,
			__LINE__, rc);
		return rc;
	}

	JPEG_DBG("%s:%d] platform resources - mem %p, base %p, irq %d\n",
		__func__, __LINE__,
		pgmn_dev->mem, pgmn_dev->base, pgmn_dev->irq);

	msm_jpeg_q_cleanup(&pgmn_dev->evt_q);
	msm_jpeg_q_cleanup(&pgmn_dev->output_rtn_q);
	msm_jpeg_outbuf_q_cleanup(&pgmn_dev->output_buf_q,
	pgmn_dev->domain_num); msm_jpeg_q_cleanup(&pgmn_dev->input_rtn_q);
	msm_jpeg_q_cleanup(&pgmn_dev->input_buf_q);
	msm_jpeg_core_init();

	JPEG_DBG("%s:%d] success\n", __func__, __LINE__);
	return rc;
}

int __msm_jpeg_release(struct msm_jpeg_device *pgmn_dev)
{
	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	mutex_lock(&pgmn_dev->lock);
	if (!pgmn_dev->open_count) {
		JPEG_PR_ERR(KERN_ERR "%s: not opened\n", __func__);
		mutex_unlock(&pgmn_dev->lock);
		return -EINVAL;
	}
	pgmn_dev->open_count--;
	mutex_unlock(&pgmn_dev->lock);

	msm_jpeg_core_release(release_buf, pgmn_dev->domain_num);
	msm_jpeg_q_cleanup(&pgmn_dev->evt_q);
	msm_jpeg_q_cleanup(&pgmn_dev->output_rtn_q);
	msm_jpeg_outbuf_q_cleanup(&pgmn_dev->output_buf_q,
					pgmn_dev->domain_num);
	msm_jpeg_q_cleanup(&pgmn_dev->input_rtn_q);
	msm_jpeg_outbuf_q_cleanup(&pgmn_dev->input_buf_q, pgmn_dev->domain_num);

	JPEG_DBG("%s:%d]\n", __func__, __LINE__);
	if (pgmn_dev->open_count)
		JPEG_PR_ERR(KERN_ERR "%s: multiple opens\n", __func__);

	msm_jpeg_platform_release(pgmn_dev->mem, pgmn_dev->base,
		pgmn_dev->irq, pgmn_dev);

	return 0;
}

int msm_jpeg_ioctl_hw_cmd(struct msm_jpeg_device *pgmn_dev,
	void * __user arg)
{
	struct msm_jpeg_hw_cmd hw_cmd;
	int is_copy_to_user;

	if (copy_from_user(&hw_cmd, arg, sizeof(struct msm_jpeg_hw_cmd))) {
		JPEG_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	is_copy_to_user = msm_jpeg_hw_exec_cmds(&hw_cmd, 1);
	JPEG_DBG("%s:%d] type %d, n %d, offset %d, mask %x, data %x,pdata %x\n",
		__func__, __LINE__, hw_cmd.type, hw_cmd.n, hw_cmd.offset,
		hw_cmd.mask, hw_cmd.data, (int) hw_cmd.pdata);

	if (is_copy_to_user >= 0) {
		if (copy_to_user(arg, &hw_cmd, sizeof(hw_cmd))) {
			JPEG_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
			return -EFAULT;
		}
	}

	return 0;
}

int msm_jpeg_ioctl_hw_cmds(struct msm_jpeg_device *pgmn_dev,
	void * __user arg)
{
	int is_copy_to_user;
	int len;
	uint32_t m;
	struct msm_jpeg_hw_cmds *hw_cmds_p;
	struct msm_jpeg_hw_cmd *hw_cmd_p;

	if (copy_from_user(&m, arg, sizeof(m))) {
		JPEG_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	len = sizeof(struct msm_jpeg_hw_cmds) +
		sizeof(struct msm_jpeg_hw_cmd) * (m - 1);
	hw_cmds_p = kmalloc(len, GFP_KERNEL);
	if (!hw_cmds_p) {
		JPEG_PR_ERR("%s:%d] no mem %d\n", __func__, __LINE__, len);
		return -EFAULT;
	}

	if (copy_from_user(hw_cmds_p, arg, len)) {
		JPEG_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		kfree(hw_cmds_p);
		return -EFAULT;
	}

	hw_cmd_p = (struct msm_jpeg_hw_cmd *) &(hw_cmds_p->hw_cmd);

	is_copy_to_user = msm_jpeg_hw_exec_cmds(hw_cmd_p, m);

	if (is_copy_to_user >= 0) {
		if (copy_to_user(arg, hw_cmds_p, len)) {
			JPEG_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
			kfree(hw_cmds_p);
			return -EFAULT;
		}
	}
	kfree(hw_cmds_p);
	return 0;
}

int msm_jpeg_start(struct msm_jpeg_device *pgmn_dev, void * __user arg)
{
	struct msm_jpeg_core_buf *buf_out;
	struct msm_jpeg_core_buf *buf_out_free[2] = {NULL, NULL};
	int i, rc;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);

	release_buf = 1;
	for (i = 0; i < 2; i++) {
		buf_out = msm_jpeg_q_out(&pgmn_dev->input_buf_q);

		if (buf_out) {
			msm_jpeg_core_fe_buf_update(buf_out);
			kfree(buf_out);
		} else {
			JPEG_DBG("%s:%d] no input buffer\n", __func__,
					__LINE__);
			break;
		}
	}

	for (i = 0; i < 2; i++) {
		buf_out_free[i] = msm_jpeg_q_out(&pgmn_dev->output_buf_q);

		if (buf_out_free[i]) {
			msm_jpeg_core_we_buf_update(buf_out_free[i]);
			release_buf = 0;
		} else {
			JPEG_DBG("%s:%d] no output buffer\n",
			__func__, __LINE__);
			break;
		}
	}

	for (i = 0; i < 2; i++)
		kfree(buf_out_free[i]);

	rc = msm_jpeg_ioctl_hw_cmds(pgmn_dev, arg);
	JPEG_DBG("%s:%d]\n", __func__, __LINE__);
	return rc;
}

int msm_jpeg_ioctl_reset(struct msm_jpeg_device *pgmn_dev,
	void * __user arg)
{
	int rc;
	struct msm_jpeg_ctrl_cmd ctrl_cmd;

	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (copy_from_user(&ctrl_cmd, arg, sizeof(ctrl_cmd))) {
		JPEG_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	pgmn_dev->op_mode = ctrl_cmd.type;

	rc = msm_jpeg_core_reset(pgmn_dev->op_mode, pgmn_dev->base,
		resource_size(pgmn_dev->mem));
	return rc;
}

int msm_jpeg_ioctl_test_dump_region(struct msm_jpeg_device *pgmn_dev,
	unsigned long arg)
{
	JPEG_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_jpeg_io_dump(arg);
	return 0;
}

long __msm_jpeg_ioctl(struct msm_jpeg_device *pgmn_dev,
	unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	switch (cmd) {
	case MSM_JPEG_IOCTL_GET_HW_VERSION:
		JPEG_DBG("%s:%d] VERSION 1\n", __func__, __LINE__);
		rc = msm_jpeg_ioctl_hw_cmd(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_RESET:
		rc = msm_jpeg_ioctl_reset(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_STOP:
		rc = msm_jpeg_ioctl_hw_cmds(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_START:
		rc = msm_jpeg_start(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_INPUT_BUF_ENQUEUE:
		rc = msm_jpeg_input_buf_enqueue(pgmn_dev,
			(void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_INPUT_GET:
		rc = msm_jpeg_input_get(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_INPUT_GET_UNBLOCK:
		rc = msm_jpeg_input_get_unblock(pgmn_dev);
		break;

	case MSM_JPEG_IOCTL_OUTPUT_BUF_ENQUEUE:
		rc = msm_jpeg_output_buf_enqueue(pgmn_dev,
			(void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_OUTPUT_GET:
		rc = msm_jpeg_output_get(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_OUTPUT_GET_UNBLOCK:
		rc = msm_jpeg_output_get_unblock(pgmn_dev);
		break;

	case MSM_JPEG_IOCTL_EVT_GET:
		rc = msm_jpeg_evt_get(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_EVT_GET_UNBLOCK:
		rc = msm_jpeg_evt_get_unblock(pgmn_dev);
		break;

	case MSM_JPEG_IOCTL_HW_CMD:
		rc = msm_jpeg_ioctl_hw_cmd(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_HW_CMDS:
		rc = msm_jpeg_ioctl_hw_cmds(pgmn_dev, (void __user *) arg);
		break;

	case MSM_JPEG_IOCTL_TEST_DUMP_REGION:
		rc = msm_jpeg_ioctl_test_dump_region(pgmn_dev, arg);
		break;

	default:
		JPEG_PR_ERR(KERN_INFO "%s:%d] cmd = %d not supported\n",
			__func__, __LINE__, _IOC_NR(cmd));
		rc = -EINVAL;
		break;
	}
	return rc;
}
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
static int camera_register_domain(void)
{
	struct msm_iova_partition camera_fw_partition = {
		.start = SZ_128K,
		.size = SZ_2G - SZ_128K,
	};

	struct msm_iova_layout camera_fw_layout = {
		.partitions = &camera_fw_partition,
		.npartitions = 1,
		.client_name = "camera_jpeg",
		.domain_flags = 0,
	};
	return msm_register_domain(&camera_fw_layout);
}
#endif

int __msm_jpeg_init(struct msm_jpeg_device *pgmn_dev)
{
	int rc = 0, i = 0;
	int idx = 0;
	char *iommu_name[3] = {"jpeg_enc0", "jpeg_enc1", "jpeg_dec"};

	mutex_init(&pgmn_dev->lock);

	pr_err("%s:%d] Jpeg Device id %d", __func__, __LINE__,
		   pgmn_dev->pdev->id);
	idx = pgmn_dev->pdev->id;
	pgmn_dev->idx = idx;
	pgmn_dev->iommu_cnt = 1;

	msm_jpeg_q_init("evt_q", &pgmn_dev->evt_q);
	msm_jpeg_q_init("output_rtn_q", &pgmn_dev->output_rtn_q);
	msm_jpeg_q_init("output_buf_q", &pgmn_dev->output_buf_q);
	msm_jpeg_q_init("input_rtn_q", &pgmn_dev->input_rtn_q);
	msm_jpeg_q_init("input_buf_q", &pgmn_dev->input_buf_q);

#ifdef CONFIG_MSM_IOMMU
/*get device context for IOMMU*/
	for (i = 0; i < pgmn_dev->iommu_cnt; i++) {
		pgmn_dev->iommu_ctx_arr[i] = msm_iommu_get_ctx(iommu_name[i]);
		JPEG_DBG("%s:%d] name %s", __func__, __LINE__, iommu_name[i]);
		JPEG_DBG("%s:%d] ctx 0x%x", __func__, __LINE__,
					(uint32_t)pgmn_dev->iommu_ctx_arr[i]);
		if (!pgmn_dev->iommu_ctx_arr[i]) {
			JPEG_PR_ERR("%s: No iommu fw context found\n",
					__func__);
			goto error;
		}
	}
	pgmn_dev->domain_num = camera_register_domain();
	JPEG_DBG("%s:%d] dom_num 0x%x", __func__, __LINE__,
				pgmn_dev->domain_num);
	if (pgmn_dev->domain_num < 0) {
		JPEG_PR_ERR("%s: could not register domain\n", __func__);
		goto error;
	}
	pgmn_dev->domain = msm_get_iommu_domain(pgmn_dev->domain_num);
	JPEG_DBG("%s:%d] dom 0x%x", __func__, __LINE__,
					(uint32_t)pgmn_dev->domain);
	if (!pgmn_dev->domain) {
		JPEG_PR_ERR("%s: cannot find domain\n", __func__);
		goto error;
	}
#endif

	return rc;
error:
	mutex_destroy(&pgmn_dev->lock);
	return -EFAULT;
}

int __msm_jpeg_exit(struct msm_jpeg_device *pgmn_dev)
{
	mutex_destroy(&pgmn_dev->lock);
	kfree(pgmn_dev);
	return 0;
}

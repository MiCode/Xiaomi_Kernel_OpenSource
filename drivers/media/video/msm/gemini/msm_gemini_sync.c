/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <media/msm_gemini.h>
#include "msm_gemini_sync.h"
#include "msm_gemini_core.h"
#include "msm_gemini_platform.h"
#include "msm_gemini_common.h"

static int release_buf;

/*************** queue helper ****************/
inline void msm_gemini_q_init(char const *name, struct msm_gemini_q *q_p)
{
	GMN_DBG("%s:%d] %s\n", __func__, __LINE__, name);
	q_p->name = name;
	spin_lock_init(&q_p->lck);
	INIT_LIST_HEAD(&q_p->q);
	init_waitqueue_head(&q_p->wait);
	q_p->unblck = 0;
}

inline void *msm_gemini_q_out(struct msm_gemini_q *q_p)
{
	unsigned long flags;
	struct msm_gemini_q_entry *q_entry_p = NULL;
	void *data = NULL;

	GMN_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	spin_lock_irqsave(&q_p->lck, flags);
	if (!list_empty(&q_p->q)) {
		q_entry_p = list_first_entry(&q_p->q, struct msm_gemini_q_entry,
			list);
		list_del_init(&q_entry_p->list);
	}
	spin_unlock_irqrestore(&q_p->lck, flags);

	if (q_entry_p) {
		data = q_entry_p->data;
		kfree(q_entry_p);
	} else {
		GMN_DBG("%s:%d] %s no entry\n", __func__, __LINE__,
			q_p->name);
	}

	return data;
}

inline int msm_gemini_q_in(struct msm_gemini_q *q_p, void *data)
{
	unsigned long flags;

	struct msm_gemini_q_entry *q_entry_p;

	GMN_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);

	q_entry_p = kmalloc(sizeof(struct msm_gemini_q_entry), GFP_ATOMIC);
	if (!q_entry_p) {
		GMN_PR_ERR("%s: no mem\n", __func__);
		return -1;
	}
	q_entry_p->data = data;

	spin_lock_irqsave(&q_p->lck, flags);
	list_add_tail(&q_entry_p->list, &q_p->q);
	spin_unlock_irqrestore(&q_p->lck, flags);

	return 0;
}

inline int msm_gemini_q_in_buf(struct msm_gemini_q *q_p,
	struct msm_gemini_core_buf *buf)
{
	struct msm_gemini_core_buf *buf_p;

	GMN_DBG("%s:%d]\n", __func__, __LINE__);
	buf_p = kmalloc(sizeof(struct msm_gemini_core_buf), GFP_ATOMIC);
	if (!buf_p) {
		GMN_PR_ERR("%s: no mem\n", __func__);
		return -1;
	}

	memcpy(buf_p, buf, sizeof(struct msm_gemini_core_buf));

	msm_gemini_q_in(q_p, buf_p);
	return 0;
}

inline int msm_gemini_q_wait(struct msm_gemini_q *q_p)
{
	int tm = MAX_SCHEDULE_TIMEOUT; /* 500ms */
	int rc;

	GMN_DBG("%s:%d] %s wait\n", __func__, __LINE__, q_p->name);
	rc = wait_event_interruptible_timeout(q_p->wait,
		(!list_empty_careful(&q_p->q) || q_p->unblck),
		msecs_to_jiffies(tm));
	GMN_DBG("%s:%d] %s wait done\n", __func__, __LINE__, q_p->name);
	if (list_empty_careful(&q_p->q)) {
		if (rc == 0) {
			rc = -ETIMEDOUT;
			GMN_PR_ERR("%s:%d] %s timeout\n", __func__, __LINE__,
				q_p->name);
		} else if (q_p->unblck) {
			GMN_DBG("%s:%d] %s unblock is true\n", __func__,
				__LINE__, q_p->name);
			q_p->unblck = 0;
			rc = -ECANCELED;
		} else if (rc < 0) {
			GMN_PR_ERR("%s:%d] %s rc %d\n", __func__, __LINE__,
				q_p->name, rc);
		}
	}
	return rc;
}

inline int msm_gemini_q_wakeup(struct msm_gemini_q *q_p)
{
	GMN_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	wake_up(&q_p->wait);
	return 0;
}

inline int msm_gemini_q_unblock(struct msm_gemini_q *q_p)
{
	GMN_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	q_p->unblck = 1;
	wake_up(&q_p->wait);
	return 0;
}

inline void msm_gemini_outbuf_q_cleanup(struct msm_gemini_q *q_p)
{
	struct msm_gemini_core_buf *buf_p;
	GMN_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	do {
		buf_p = msm_gemini_q_out(q_p);
		if (buf_p) {
			msm_gemini_platform_p2v(buf_p->file,
				&buf_p->msm_buffer, &buf_p->handle);
			GMN_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
			kfree(buf_p->subsystem_id);
			kfree(buf_p);
		}
	} while (buf_p);
	q_p->unblck = 0;
}

inline void msm_gemini_q_cleanup(struct msm_gemini_q *q_p)
{
	void *data;
	GMN_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	do {
		data = msm_gemini_q_out(q_p);
		if (data) {
			GMN_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
			kfree(data);
		}
	} while (data);
	q_p->unblck = 0;
}

/*************** event queue ****************/

int msm_gemini_framedone_irq(struct msm_gemini_device *pgmn_dev,
	struct msm_gemini_core_buf *buf_in)
{
	int rc = 0;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);

	if (buf_in) {
		buf_in->vbuf.framedone_len = buf_in->framedone_len;
		buf_in->vbuf.type = MSM_GEMINI_EVT_FRAMEDONE;
		GMN_DBG("%s:%d] 0x%08x %d framedone_len %d\n",
			__func__, __LINE__,
			(int) buf_in->y_buffer_addr, buf_in->y_len,
			buf_in->vbuf.framedone_len);
		rc = msm_gemini_q_in_buf(&pgmn_dev->evt_q, buf_in);
	} else {
		GMN_PR_ERR("%s:%d] no output return buffer\n",
			__func__, __LINE__);
		rc = -1;
	}

	if (buf_in)
		rc = msm_gemini_q_wakeup(&pgmn_dev->evt_q);

	return rc;
}

int msm_gemini_evt_get(struct msm_gemini_device *pgmn_dev,
	void __user *to)
{
	struct msm_gemini_core_buf *buf_p;
	struct msm_gemini_ctrl_cmd ctrl_cmd;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);

	msm_gemini_q_wait(&pgmn_dev->evt_q);
	buf_p = msm_gemini_q_out(&pgmn_dev->evt_q);

	if (!buf_p) {
		GMN_DBG("%s:%d] no buffer\n", __func__, __LINE__);
		return -EAGAIN;
	}

	ctrl_cmd.type = buf_p->vbuf.type;
	kfree(buf_p);

	GMN_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
		(int) ctrl_cmd.value, ctrl_cmd.len);

	if (copy_to_user(to, &ctrl_cmd, sizeof(ctrl_cmd))) {
		GMN_PR_ERR("%s:%d]\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int msm_gemini_evt_get_unblock(struct msm_gemini_device *pgmn_dev)
{
	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_gemini_q_unblock(&pgmn_dev->evt_q);
	return 0;
}

void msm_gemini_reset_ack_irq(struct msm_gemini_device *pgmn_dev)
{
	GMN_DBG("%s:%d]\n", __func__, __LINE__);
}

void msm_gemini_err_irq(struct msm_gemini_device *pgmn_dev,
	int event)
{
	int rc = 0;
	struct msm_gemini_core_buf buf;

	GMN_PR_ERR("%s:%d] error: %d\n", __func__, __LINE__, event);

	buf.vbuf.type = MSM_GEMINI_EVT_ERR;
	rc = msm_gemini_q_in_buf(&pgmn_dev->evt_q, &buf);
	if (!rc)
		rc = msm_gemini_q_wakeup(&pgmn_dev->evt_q);

	if (!rc)
		GMN_PR_ERR("%s:%d] err err\n", __func__, __LINE__);

	return;
}

/*************** output queue ****************/

int msm_gemini_we_pingpong_irq(struct msm_gemini_device *pgmn_dev,
	struct msm_gemini_core_buf *buf_in)
{
	int rc = 0;
	struct msm_gemini_core_buf *buf_out;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (buf_in) {
		GMN_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
			(int) buf_in->y_buffer_addr, buf_in->y_len);
		rc = msm_gemini_q_in_buf(&pgmn_dev->output_rtn_q, buf_in);
	} else {
		GMN_DBG("%s:%d] no output return buffer\n", __func__,
			__LINE__);
		rc = -1;
	}

	buf_out = msm_gemini_q_out(&pgmn_dev->output_buf_q);

	if (buf_out) {
		rc = msm_gemini_core_we_buf_update(buf_out);
		kfree(buf_out);
	} else {
		msm_gemini_core_we_buf_reset(buf_in);
		GMN_DBG("%s:%d] no output buffer\n", __func__, __LINE__);
		rc = -2;
	}

	if (buf_in)
		rc = msm_gemini_q_wakeup(&pgmn_dev->output_rtn_q);

	return rc;
}

int msm_gemini_output_get(struct msm_gemini_device *pgmn_dev, void __user *to)
{
	struct msm_gemini_core_buf *buf_p;
	struct msm_gemini_buf buf_cmd;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);

	msm_gemini_q_wait(&pgmn_dev->output_rtn_q);
	buf_p = msm_gemini_q_out(&pgmn_dev->output_rtn_q);

	if (!buf_p) {
		GMN_DBG("%s:%d] no output buffer return\n",
			__func__, __LINE__);
		return -EAGAIN;
	}

	buf_cmd = buf_p->vbuf;
	msm_gemini_platform_p2v(buf_p->file, &buf_p->msm_buffer,
				&buf_p->handle);
	kfree(buf_p->subsystem_id);
	kfree(buf_p);

	GMN_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
		(int) buf_cmd.vaddr, buf_cmd.y_len);

	if (copy_to_user(to, &buf_cmd, sizeof(buf_cmd))) {
		GMN_PR_ERR("%s:%d]", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int msm_gemini_output_get_unblock(struct msm_gemini_device *pgmn_dev)
{
	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_gemini_q_unblock(&pgmn_dev->output_rtn_q);
	return 0;
}

int msm_gemini_output_buf_enqueue(struct msm_gemini_device *pgmn_dev,
	void __user *arg)
{
	struct msm_gemini_buf buf_cmd;
	struct msm_gemini_core_buf *buf_p;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (copy_from_user(&buf_cmd, arg, sizeof(struct msm_gemini_buf))) {
		GMN_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	buf_p = kmalloc(sizeof(struct msm_gemini_core_buf), GFP_ATOMIC);
	if (!buf_p) {
		GMN_PR_ERR("%s:%d] no mem\n", __func__, __LINE__);
		return -1;
	}

	GMN_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__, (int) buf_cmd.vaddr,
		buf_cmd.y_len);

	buf_p->subsystem_id = kmalloc(sizeof(int), GFP_ATOMIC);
	if (!buf_p->subsystem_id) {
		GMN_PR_ERR("%s:%d] no mem\n", __func__, __LINE__);
		kfree(buf_p);
		return -ENOMEM;
	}
	buf_p->y_buffer_addr = msm_gemini_platform_v2p(buf_cmd.fd,
		buf_cmd.y_len, &buf_p->file, &buf_p->msm_buffer,
		buf_p->subsystem_id, &buf_p->handle);
	if (!buf_p->y_buffer_addr) {
		GMN_PR_ERR("%s:%d] v2p wrong\n", __func__, __LINE__);
		kfree(buf_p->subsystem_id);
		kfree(buf_p);
		return -1;
	}
	buf_p->y_len = buf_cmd.y_len;
	buf_p->vbuf = buf_cmd;

	msm_gemini_q_in(&pgmn_dev->output_buf_q, buf_p);
	return 0;
}

/*************** input queue ****************/

int msm_gemini_fe_pingpong_irq(struct msm_gemini_device *pgmn_dev,
	struct msm_gemini_core_buf *buf_in)
{
	struct msm_gemini_core_buf *buf_out;
	int rc = 0;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (buf_in) {
		GMN_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
			(int) buf_in->y_buffer_addr, buf_in->y_len);
		rc = msm_gemini_q_in_buf(&pgmn_dev->input_rtn_q, buf_in);
	} else {
		GMN_DBG("%s:%d] no input return buffer\n", __func__,
			__LINE__);
		rc = -1;
	}

	buf_out = msm_gemini_q_out(&pgmn_dev->input_buf_q);

	if (buf_out) {
		rc = msm_gemini_core_fe_buf_update(buf_out);
		kfree(buf_out);
		msm_gemini_core_fe_start();
	} else {
		GMN_DBG("%s:%d] no input buffer\n", __func__, __LINE__);
		rc = -2;
	}

	if (buf_in)
		rc = msm_gemini_q_wakeup(&pgmn_dev->input_rtn_q);

	return rc;
}

int msm_gemini_input_get(struct msm_gemini_device *pgmn_dev, void __user * to)
{
	struct msm_gemini_core_buf *buf_p;
	struct msm_gemini_buf buf_cmd;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_gemini_q_wait(&pgmn_dev->input_rtn_q);
	buf_p = msm_gemini_q_out(&pgmn_dev->input_rtn_q);

	if (!buf_p) {
		GMN_DBG("%s:%d] no input buffer return\n",
			__func__, __LINE__);
		return -EAGAIN;
	}

	buf_cmd = buf_p->vbuf;
	if (pgmn_dev->op_mode == MSM_GEMINI_MODE_OFFLINE_ENCODE ||
		pgmn_dev->op_mode == MSM_GEMINI_MODE_OFFLINE_ROTATION) {
		msm_gemini_platform_p2v(buf_p->file, &buf_p->msm_buffer,
			&buf_p->handle);
	}
	kfree(buf_p->subsystem_id);
	kfree(buf_p);

	GMN_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
		(int) buf_cmd.vaddr, buf_cmd.y_len);

	if (copy_to_user(to, &buf_cmd, sizeof(buf_cmd))) {
		GMN_PR_ERR("%s:%d]\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int msm_gemini_input_get_unblock(struct msm_gemini_device *pgmn_dev)
{
	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_gemini_q_unblock(&pgmn_dev->input_rtn_q);
	return 0;
}

int msm_gemini_input_buf_enqueue(struct msm_gemini_device *pgmn_dev,
	void __user *arg)
{
	struct msm_gemini_core_buf *buf_p;
	struct msm_gemini_buf buf_cmd;

	if (copy_from_user(&buf_cmd, arg, sizeof(struct msm_gemini_buf))) {
		GMN_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	buf_p = kmalloc(sizeof(struct msm_gemini_core_buf), GFP_ATOMIC);
	if (!buf_p) {
		GMN_PR_ERR("%s:%d] no mem\n", __func__, __LINE__);
		return -1;
	}

	GMN_DBG("%s:%d] 0x%08x %d\n", __func__, __LINE__,
		(int) buf_cmd.vaddr, buf_cmd.y_len);

	buf_p->subsystem_id = kmalloc(sizeof(int), GFP_ATOMIC);
	if (!buf_p->subsystem_id) {
		GMN_PR_ERR("%s:%d] no mem\n", __func__, __LINE__);
		kfree(buf_p);
		return -ENOMEM;
	}
	if (pgmn_dev->op_mode == MSM_GEMINI_MODE_REALTIME_ENCODE) {
		buf_p->y_buffer_addr    = buf_cmd.y_off;
	} else {
	buf_p->y_buffer_addr    = msm_gemini_platform_v2p(buf_cmd.fd,
		buf_cmd.y_len + buf_cmd.cbcr_len, &buf_p->file,
			&buf_p->msm_buffer, buf_p->subsystem_id, &buf_p->handle)
			+ buf_cmd.offset;
	}
	buf_p->y_len          = buf_cmd.y_len;

	buf_p->cbcr_buffer_addr = buf_p->y_buffer_addr + buf_cmd.y_len;
	buf_p->cbcr_len       = buf_cmd.cbcr_len;

	buf_p->num_of_mcu_rows = buf_cmd.num_of_mcu_rows;
	GMN_DBG("%s: y_addr=%x,y_len=%x,cbcr_addr=%x,cbcr_len=%x\n", __func__,
		buf_p->y_buffer_addr, buf_p->y_len, buf_p->cbcr_buffer_addr,
		buf_p->cbcr_len);

	if (!buf_p->y_buffer_addr || !buf_p->cbcr_buffer_addr) {
		GMN_PR_ERR("%s:%d] v2p wrong\n", __func__, __LINE__);
		kfree(buf_p->subsystem_id);
		kfree(buf_p);
		return -1;
	}
	buf_p->vbuf           = buf_cmd;

	msm_gemini_q_in(&pgmn_dev->input_buf_q, buf_p);

	return 0;
}

int msm_gemini_irq(int event, void *context, void *data)
{
	struct msm_gemini_device *pgmn_dev =
		(struct msm_gemini_device *) context;

	switch (event) {
	case MSM_GEMINI_HW_MASK_COMP_FRAMEDONE:
		msm_gemini_framedone_irq(pgmn_dev, data);
		msm_gemini_we_pingpong_irq(pgmn_dev, data);
		break;

	case MSM_GEMINI_HW_MASK_COMP_FE:
		msm_gemini_fe_pingpong_irq(pgmn_dev, data);
		break;

	case MSM_GEMINI_HW_MASK_COMP_WE:
		msm_gemini_we_pingpong_irq(pgmn_dev, data);
		break;

	case MSM_GEMINI_HW_MASK_COMP_RESET_ACK:
		msm_gemini_reset_ack_irq(pgmn_dev);
		break;

	case MSM_GEMINI_HW_MASK_COMP_ERR:
	default:
		msm_gemini_err_irq(pgmn_dev, event);
		break;
	}

	return 0;
}

int __msm_gemini_open(struct msm_gemini_device *pgmn_dev)
{
	int rc;

	mutex_lock(&pgmn_dev->lock);
	if (pgmn_dev->open_count) {
		/* only open once */
		GMN_PR_ERR("%s:%d] busy\n", __func__, __LINE__);
		mutex_unlock(&pgmn_dev->lock);
		return -EBUSY;
	}
	pgmn_dev->open_count++;
	mutex_unlock(&pgmn_dev->lock);

	msm_gemini_core_irq_install(msm_gemini_irq);
	rc = msm_gemini_platform_init(pgmn_dev->pdev,
		&pgmn_dev->mem, &pgmn_dev->base,
		&pgmn_dev->irq, msm_gemini_core_irq, pgmn_dev);
	if (rc) {
		GMN_PR_ERR("%s:%d] platform_init fail %d\n", __func__,
			__LINE__, rc);
		return rc;
	}

	GMN_DBG("%s:%d] platform resources - mem %p, base %p, irq %d\n",
		__func__, __LINE__,
		pgmn_dev->mem, pgmn_dev->base, pgmn_dev->irq);

	msm_gemini_q_cleanup(&pgmn_dev->evt_q);
	msm_gemini_q_cleanup(&pgmn_dev->output_rtn_q);
	msm_gemini_outbuf_q_cleanup(&pgmn_dev->output_buf_q);
	msm_gemini_q_cleanup(&pgmn_dev->input_rtn_q);
	msm_gemini_q_cleanup(&pgmn_dev->input_buf_q);
	msm_gemini_core_init();

	GMN_DBG("%s:%d] success\n", __func__, __LINE__);
	return rc;
}

int __msm_gemini_release(struct msm_gemini_device *pgmn_dev)
{
	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	mutex_lock(&pgmn_dev->lock);
	if (!pgmn_dev->open_count) {
		GMN_PR_ERR(KERN_ERR "%s: not opened\n", __func__);
		mutex_unlock(&pgmn_dev->lock);
		return -EINVAL;
	}
	pgmn_dev->open_count--;
	mutex_unlock(&pgmn_dev->lock);

	msm_gemini_core_release(release_buf);
	msm_gemini_q_cleanup(&pgmn_dev->evt_q);
	msm_gemini_q_cleanup(&pgmn_dev->output_rtn_q);
	msm_gemini_outbuf_q_cleanup(&pgmn_dev->output_buf_q);
	msm_gemini_q_cleanup(&pgmn_dev->input_rtn_q);
	msm_gemini_outbuf_q_cleanup(&pgmn_dev->input_buf_q);

	if (pgmn_dev->open_count)
		GMN_PR_ERR(KERN_ERR "%s: multiple opens\n", __func__);

	msm_gemini_platform_release(pgmn_dev->mem, pgmn_dev->base,
		pgmn_dev->irq, pgmn_dev);

	return 0;
}

int msm_gemini_ioctl_hw_cmd(struct msm_gemini_device *pgmn_dev,
	void * __user arg)
{
	struct msm_gemini_hw_cmd hw_cmd;
	int is_copy_to_user;

	if (copy_from_user(&hw_cmd, arg, sizeof(struct msm_gemini_hw_cmd))) {
		GMN_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	is_copy_to_user = msm_gemini_hw_exec_cmds(&hw_cmd, 1);
	GMN_DBG("%s:%d] type %d, n %d, offset %d, mask %x, data %x, pdata %x\n",
		__func__, __LINE__, hw_cmd.type, hw_cmd.n, hw_cmd.offset,
		hw_cmd.mask, hw_cmd.data, (int) hw_cmd.pdata);

	if (is_copy_to_user >= 0) {
		if (copy_to_user(arg, &hw_cmd, sizeof(hw_cmd))) {
			GMN_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
			return -EFAULT;
		}
	}

	return 0;
}

int msm_gemini_ioctl_hw_cmds(struct msm_gemini_device *pgmn_dev,
	void * __user arg)
{
	int is_copy_to_user;
	int len;
	uint32_t m;
	struct msm_gemini_hw_cmds *hw_cmds_p;
	struct msm_gemini_hw_cmd *hw_cmd_p;

	if (copy_from_user(&m, arg, sizeof(m))) {
		GMN_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	len = sizeof(struct msm_gemini_hw_cmds) +
		sizeof(struct msm_gemini_hw_cmd) * (m - 1);
	hw_cmds_p = kmalloc(len, GFP_KERNEL);
	if (!hw_cmds_p) {
		GMN_PR_ERR("%s:%d] no mem %d\n", __func__, __LINE__, len);
		return -EFAULT;
	}

	if (copy_from_user(hw_cmds_p, arg, len)) {
		GMN_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		kfree(hw_cmds_p);
		return -EFAULT;
	}

	hw_cmd_p = (struct msm_gemini_hw_cmd *) &(hw_cmds_p->hw_cmd);

	is_copy_to_user = msm_gemini_hw_exec_cmds(hw_cmd_p, m);

	if (is_copy_to_user >= 0) {
		if (copy_to_user(arg, hw_cmds_p, len)) {
			GMN_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
			kfree(hw_cmds_p);
			return -EFAULT;
		}
	}
	kfree(hw_cmds_p);
	return 0;
}

int msm_gemini_start(struct msm_gemini_device *pgmn_dev, void * __user arg)
{
	struct msm_gemini_core_buf *buf_out;
	struct msm_gemini_core_buf *buf_out_free[2] = {NULL, NULL};
	int i, rc;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);

	release_buf = 1;
	for (i = 0; i < 2; i++) {
		buf_out = msm_gemini_q_out(&pgmn_dev->input_buf_q);

		if (buf_out) {
			msm_gemini_core_fe_buf_update(buf_out);
			kfree(buf_out);
		} else {
			GMN_DBG("%s:%d] no input buffer\n", __func__, __LINE__);
			break;
		}
	}

	for (i = 0; i < 2; i++) {
		buf_out_free[i] = msm_gemini_q_out(&pgmn_dev->output_buf_q);

		if (buf_out_free[i]) {
			msm_gemini_core_we_buf_update(buf_out_free[i]);
		} else if (i == 1) {
			/* set the pong to same address as ping */
			buf_out_free[0]->y_len >>= 1;
			buf_out_free[0]->y_buffer_addr +=
				buf_out_free[0]->y_len;
			msm_gemini_core_we_buf_update(buf_out_free[0]);
			/* since ping and pong are same buf release only once*/
			release_buf = 0;
		} else {
			GMN_DBG("%s:%d] no output buffer\n",
			__func__, __LINE__);
			break;
		}
	}

	for (i = 0; i < 2; i++)
		kfree(buf_out_free[i]);

	rc = msm_gemini_ioctl_hw_cmds(pgmn_dev, arg);
	GMN_DBG("%s:%d]\n", __func__, __LINE__);
	return rc;
}

int msm_gemini_ioctl_reset(struct msm_gemini_device *pgmn_dev,
	void * __user arg)
{
	int rc;
	struct msm_gemini_ctrl_cmd ctrl_cmd;

	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (copy_from_user(&ctrl_cmd, arg, sizeof(ctrl_cmd))) {
		GMN_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	pgmn_dev->op_mode = ctrl_cmd.type;

	rc = msm_gemini_core_reset(pgmn_dev->op_mode, pgmn_dev->base,
		resource_size(pgmn_dev->mem));
	return rc;
}

int msm_gemini_ioctl_test_dump_region(struct msm_gemini_device *pgmn_dev,
	unsigned long arg)
{
	GMN_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_gemini_hw_region_dump(arg);
	return 0;
}

long __msm_gemini_ioctl(struct msm_gemini_device *pgmn_dev,
	unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	switch (cmd) {
	case MSM_GMN_IOCTL_GET_HW_VERSION:
		GMN_DBG("%s:%d] VERSION 1\n", __func__, __LINE__);
		rc = msm_gemini_ioctl_hw_cmd(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_RESET:
		rc = msm_gemini_ioctl_reset(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_STOP:
		rc = msm_gemini_ioctl_hw_cmds(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_START:
		rc = msm_gemini_start(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_INPUT_BUF_ENQUEUE:
		rc = msm_gemini_input_buf_enqueue(pgmn_dev,
			(void __user *) arg);
		break;

	case MSM_GMN_IOCTL_INPUT_GET:
		rc = msm_gemini_input_get(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_INPUT_GET_UNBLOCK:
		rc = msm_gemini_input_get_unblock(pgmn_dev);
		break;

	case MSM_GMN_IOCTL_OUTPUT_BUF_ENQUEUE:
		rc = msm_gemini_output_buf_enqueue(pgmn_dev,
			(void __user *) arg);
		break;

	case MSM_GMN_IOCTL_OUTPUT_GET:
		rc = msm_gemini_output_get(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_OUTPUT_GET_UNBLOCK:
		rc = msm_gemini_output_get_unblock(pgmn_dev);
		break;

	case MSM_GMN_IOCTL_EVT_GET:
		rc = msm_gemini_evt_get(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_EVT_GET_UNBLOCK:
		rc = msm_gemini_evt_get_unblock(pgmn_dev);
		break;

	case MSM_GMN_IOCTL_HW_CMD:
		rc = msm_gemini_ioctl_hw_cmd(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_HW_CMDS:
		rc = msm_gemini_ioctl_hw_cmds(pgmn_dev, (void __user *) arg);
		break;

	case MSM_GMN_IOCTL_TEST_DUMP_REGION:
		rc = msm_gemini_ioctl_test_dump_region(pgmn_dev, arg);
		break;

	default:
		GMN_PR_ERR(KERN_INFO "%s:%d] cmd = %d not supported\n",
			__func__, __LINE__, _IOC_NR(cmd));
		rc = -EINVAL;
		break;
	}
	return rc;
}

struct msm_gemini_device *__msm_gemini_init(struct platform_device *pdev)
{
	struct msm_gemini_device *pgmn_dev;

	pgmn_dev = kzalloc(sizeof(struct msm_gemini_device), GFP_ATOMIC);
	if (!pgmn_dev) {
		GMN_PR_ERR("%s:%d]no mem\n", __func__, __LINE__);
		return NULL;
	}

	mutex_init(&pgmn_dev->lock);

	pgmn_dev->pdev = pdev;

	msm_gemini_q_init("evt_q", &pgmn_dev->evt_q);
	msm_gemini_q_init("output_rtn_q", &pgmn_dev->output_rtn_q);
	msm_gemini_q_init("output_buf_q", &pgmn_dev->output_buf_q);
	msm_gemini_q_init("input_rtn_q", &pgmn_dev->input_rtn_q);
	msm_gemini_q_init("input_buf_q", &pgmn_dev->input_buf_q);

	return pgmn_dev;
}

int __msm_gemini_exit(struct msm_gemini_device *pgmn_dev)
{
	mutex_destroy(&pgmn_dev->lock);
	kfree(pgmn_dev);
	return 0;
}


/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <media/msm_mercury.h>

#include "msm_mercury_sync.h"
#include "msm_mercury_core.h"
#include "msm_mercury_platform.h"
#include "msm_mercury_common.h"
#include "msm_mercury_macros.h"
#include "msm_mercury_hw_reg.h"

static struct msm_mercury_core_buf out_buf_local;
static struct msm_mercury_core_buf in_buf_local;

/*************** queue helper ****************/
inline void msm_mercury_q_init(char const *name, struct msm_mercury_q *q_p)
{
	MCR_DBG("%s:%d] %s\n", __func__, __LINE__, name);
	q_p->name = name;
	spin_lock_init(&q_p->lck);
	INIT_LIST_HEAD(&q_p->q);
	init_waitqueue_head(&q_p->wait);
	q_p->unblck = 0;
}

inline void *msm_mercury_q_out(struct msm_mercury_q *q_p)
{
	unsigned long flags;
	struct msm_mercury_q_entry *q_entry_p = NULL;
	void *data = NULL;

	MCR_DBG("(%d)%s()  %s\n", __LINE__, __func__, q_p->name);
	spin_lock_irqsave(&q_p->lck, flags);
	if (!list_empty(&q_p->q)) {
		q_entry_p = list_first_entry(&q_p->q,
			struct msm_mercury_q_entry,
			list);
		list_del_init(&q_entry_p->list);
	}
	spin_unlock_irqrestore(&q_p->lck, flags);

	if (q_entry_p) {
		data = q_entry_p->data;
		kfree(q_entry_p);
	} else {
		MCR_DBG("%s:%d] %s no entry\n", __func__, __LINE__, q_p->name);
	}

	return data;
}

inline int msm_mercury_q_in(struct msm_mercury_q *q_p, void *data)
{
	unsigned long flags;

	struct msm_mercury_q_entry *q_entry_p;

	MCR_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);

	q_entry_p = kmalloc(sizeof(struct msm_mercury_q_entry), GFP_ATOMIC);
	if (!q_entry_p) {
		MCR_PR_ERR("%s: no mem\n", __func__);
		return -EFAULT;
	}
	q_entry_p->data = data;

	spin_lock_irqsave(&q_p->lck, flags);
	list_add_tail(&q_entry_p->list, &q_p->q);
	spin_unlock_irqrestore(&q_p->lck, flags);

	return 0;
}

inline int msm_mercury_q_in_buf(struct msm_mercury_q *q_p,
	struct msm_mercury_core_buf *buf)
{
	struct msm_mercury_core_buf *buf_p;

	MCR_DBG("%s:%d]\n", __func__, __LINE__);
	buf_p = kmalloc(sizeof(struct msm_mercury_core_buf), GFP_ATOMIC);
	if (!buf_p) {
		MCR_PR_ERR("%s: no mem\n", __func__);
		return -EFAULT;
	}

	memcpy(buf_p, buf, sizeof(struct msm_mercury_core_buf));

	msm_mercury_q_in(q_p, buf_p);
	return 0;
}

inline int msm_mercury_q_wait(struct msm_mercury_q *q_p)
{
	int tm = MAX_SCHEDULE_TIMEOUT; /* 500ms */
	int rc;

	MCR_DBG("%s:%d %s wait\n", __func__, __LINE__, q_p->name);
	rc = wait_event_interruptible_timeout(q_p->wait,
		(!list_empty_careful(&q_p->q) || q_p->unblck),
		msecs_to_jiffies(tm));

	MCR_DBG("%s:%d %s wait done (rc=%d)\n", __func__,
		__LINE__, q_p->name, rc);
	if (list_empty_careful(&q_p->q)) {
		if (rc == 0) {
			rc = -ETIMEDOUT;
			MCR_PR_ERR("%s:%d] %s timeout\n", __func__,
				__LINE__, q_p->name);
		} else if (q_p->unblck) {
			MCR_DBG("%s:%d %s unblock is true", __func__,
				__LINE__, q_p->name);
			rc = q_p->unblck;
			q_p->unblck = 0;
		} else if (rc < 0) {
			MCR_PR_ERR("%s:%d %s rc %d\n", __func__, __LINE__,
				q_p->name, rc);
		}
	}
	return rc;
}

inline int msm_mercury_q_wakeup(struct msm_mercury_q *q_p)
{
	MCR_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
	wake_up(&q_p->wait);
	return 0;
}

inline int msm_mercury_q_wr_eoi(struct msm_mercury_q *q_p)
{
	MCR_DBG("%s:%d] Wake up %s\n", __func__, __LINE__, q_p->name);
	q_p->unblck = MSM_MERCURY_EVT_FRAMEDONE;
	wake_up(&q_p->wait);
	return 0;
}

inline int msm_mercury_q_wr_err(struct msm_mercury_q *q_p)
{
	MCR_DBG("%s:%d] Wake up %s\n", __func__, __LINE__, q_p->name);
	q_p->unblck = MSM_MERCURY_EVT_ERR;
	wake_up(&q_p->wait);
	return 0;
}

inline int msm_mercury_q_unblock(struct msm_mercury_q *q_p)
{
	MCR_DBG("%s:%d] Wake up %s\n", __func__, __LINE__, q_p->name);
	q_p->unblck = MSM_MERCURY_EVT_UNBLOCK;
	wake_up(&q_p->wait);
	return 0;
}

inline void msm_mercury_q_cleanup(struct msm_mercury_q *q_p)
{
	void *data;
	MCR_DBG("\n%s:%d] %s\n", __func__, __LINE__, q_p->name);
	do {
		data = msm_mercury_q_out(q_p);
		if (data) {
			MCR_DBG("%s:%d] %s\n", __func__, __LINE__, q_p->name);
			kfree(data);
		}
	} while (data);
	q_p->unblck = 0;
}

/*************** event queue ****************/
int msm_mercury_framedone_irq(struct msm_mercury_device *pmercury_dev)
{
	MCR_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_mercury_q_unblock(&pmercury_dev->evt_q);

	MCR_DBG("%s:%d] Exit\n", __func__, __LINE__);
	return 0;
}

int msm_mercury_evt_get(struct msm_mercury_device *pmercury_dev,
	void __user *arg)
{
	struct msm_mercury_ctrl_cmd ctrl_cmd;
	int rc = 0;

	MCR_DBG("(%d)%s() Enter\n", __LINE__, __func__);
	memset(&ctrl_cmd, 0, sizeof(ctrl_cmd));
	ctrl_cmd.type = (uint32_t)msm_mercury_q_wait(&pmercury_dev->evt_q);

	rc = copy_to_user(arg, &ctrl_cmd, sizeof(ctrl_cmd));

	if (rc) {
		MCR_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int msm_mercury_evt_get_unblock(struct msm_mercury_device *pmercury_dev)
{
	MCR_DBG("--(%d)%s() Enter\n", __LINE__, __func__);
	msm_mercury_q_unblock(&pmercury_dev->evt_q);
	return 0;
}

int msm_mercury_output_buf_cfg(struct msm_mercury_device *pmercury_dev,
	void __user *arg)
{
	struct msm_mercury_buf buf_cmd;


	MCR_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (copy_from_user(&buf_cmd, arg, sizeof(struct msm_mercury_buf))) {
		MCR_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	out_buf_local.y_buffer_addr = msm_mercury_platform_v2p(buf_cmd.fd,
		buf_cmd.y_len, &out_buf_local.file, &out_buf_local.handle);
	out_buf_local.cbcr_buffer_addr = out_buf_local.y_buffer_addr +
		buf_cmd.y_len;

	if (!out_buf_local.y_buffer_addr) {
		MCR_PR_ERR("%s:%d] v2p wrong\n", __func__, __LINE__);
		return -EFAULT;
	}

	msm_mercury_hw_output_y_buf_cfg(out_buf_local.y_buffer_addr);
	msm_mercury_hw_output_u_buf_cfg(out_buf_local.cbcr_buffer_addr);

	MCR_DBG("(%d)%s()\n  y_buf=0x%08X, y_len=0x%08X, vaddr=0x%08X\n"
		"  u_buf=0x%08X, u_len=0x%08X\n\n", __LINE__, __func__,
		out_buf_local.y_buffer_addr, buf_cmd.y_len, (int) buf_cmd.vaddr,
		out_buf_local.cbcr_buffer_addr, buf_cmd.cbcr_len);

	return 0;
}

int msm_mercury_input_buf_cfg(struct msm_mercury_device *pmercury_dev,
	void __user *arg)
{
	struct msm_mercury_buf buf_cmd;


	MCR_DBG("%s:%d] Enter\n", __func__, __LINE__);
	if (copy_from_user(&buf_cmd, arg, sizeof(struct msm_mercury_buf))) {
		MCR_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	in_buf_local.y_buffer_addr = msm_mercury_platform_v2p(buf_cmd.fd,
		buf_cmd.y_len, &in_buf_local.file, &in_buf_local.handle);

	if (!in_buf_local.y_buffer_addr) {
		MCR_PR_ERR("%s:%d] v2p wrong\n", __func__, __LINE__);
		return -EFAULT;
	}

	msm_mercury_hw_bitstream_buf_cfg(in_buf_local.y_buffer_addr);

	MCR_DBG("(%d)%s()\n  bitstream_buf=0x%08X, len=0x%08X, vaddr=0x%08X\n",
		__LINE__, __func__, in_buf_local.y_buffer_addr, buf_cmd.y_len,
		(int) buf_cmd.vaddr);

	return 0;
}

int msm_mercury_output_get(struct msm_mercury_device *pmercury_dev,
	void __user *to)
{
	MCR_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_mercury_platform_p2v(out_buf_local.file, &out_buf_local.handle);
	return 0;
}

int msm_mercury_input_get(struct msm_mercury_device *pmercury_dev,
	void __user *to)
{


	MCR_DBG("%s:%d] Enter\n", __func__, __LINE__);
	msm_mercury_platform_p2v(in_buf_local.file, &in_buf_local.handle);
	return 0;
}

int msm_mercury_ioctl_dump_regs(void)
{
	uint32_t mercury_regs[] = {
		0x0000, 0x0008, 0x0010, 0x0014, 0x0018, 0x001C, 0x0020, 0x0024,
		0x0030, 0x0034, 0x0040, 0x0050, 0x0054, 0x0058, 0x005C, 0x0060,
		0x0064, 0x0070, 0x0080, 0x0084, 0x0088, 0x0258, 0x025C, 0x0260,
		0x0264, 0x0200, 0x0204, 0x0208, 0x020C, 0x0210, 0x0214, 0x0218,
		0x021C, 0x0220, 0x0224, 0x0228, 0x0100, 0x0104, 0x010C, 0x0110,
		0x0114, 0x0118, 0x011C, 0x0120, 0x0124, 0x0128, 0x012C};

	struct msm_mercury_hw_cmd hw_cmd;
	int len = sizeof(mercury_regs)/4;
	int i;

	MCR_DBG("\n%s\n  (%d)%s()\n", __FILE__, __LINE__, __func__);

	hw_cmd.mask = 0xFFFFFFFF;
	hw_cmd.type = MSM_MERCURY_HW_CMD_TYPE_READ;
	hw_cmd.n = 1;

	for (i = 0; i < len; i++) {
		hw_cmd.offset = mercury_regs[i];
		msm_mercury_hw_exec_cmds(&hw_cmd, 1);
	}

	return 0;
}

int msm_mercury_ioctl_magic_code(struct msm_mercury_device *pmercury_dev,
	void * __user arg)
{
	struct msm_mercury_hw_cmd hw_cmd;
	int rc = 0;

	rc = copy_from_user(&hw_cmd, arg, sizeof(struct msm_mercury_hw_cmd));
	if (rc) {
		printk(KERN_ERR "%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	hw_cmd.data = 0x600D600D;
	rc = copy_to_user(arg, &hw_cmd, sizeof(hw_cmd));

	if (rc) {
		printk(KERN_ERR "%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

int msm_mercury_irq(int event, void *context, void *data)
{
	struct msm_mercury_device *pmercury_dev =
		(struct msm_mercury_device *) context;

	switch (event) {
	case MSM_MERCURY_HW_IRQ_SW_RESET_ACK:
		/* wake up evt_q*/
		MCR_DBG("(%d)%s Wake up event q from Reset IRQ\n", __LINE__,
			__func__);
		msm_mercury_q_wakeup(&pmercury_dev->evt_q);
		break;
	case MSM_MERCURY_HW_IRQ_WR_EOI_ACK:
		/*wake up evt_q*/
		MCR_DBG("%d%s Wake up eventq from WR_EOI IRQ\n", __LINE__,
			__func__);
		msm_mercury_q_wr_eoi(&pmercury_dev->evt_q);
		break;
	case MSM_MERCURY_HW_IRQ_WR_ERR_ACK:
		MCR_DBG("(%d)%s Wake up eventq from WR_ERR IRQ\n",
			__LINE__, __func__);
		msm_mercury_q_wr_err(&pmercury_dev->evt_q);
		break;
	default:
		MCR_DBG("(%d)%s (default) Wake up event q from WR_ERR IRQ\n",
			__LINE__, __func__);
		msm_mercury_q_wr_err(&pmercury_dev->evt_q);
	}
	return 0;
}

int __msm_mercury_open(struct msm_mercury_device *pmercury_dev)
{
	int rc = 0;

	mutex_lock(&pmercury_dev->lock);
	if (pmercury_dev->open_count) {
		/* only open once */
		MCR_PR_ERR("%s:%d] busy\n", __func__, __LINE__);
		mutex_unlock(&pmercury_dev->lock);
		return -EBUSY;
	}
	pmercury_dev->open_count++;
	mutex_unlock(&pmercury_dev->lock);

	msm_mercury_core_irq_install(msm_mercury_irq);

	rc = msm_mercury_platform_init(pmercury_dev->pdev,
		&pmercury_dev->mem, &pmercury_dev->base,
		&pmercury_dev->irq, msm_mercury_core_irq, pmercury_dev);
	if (rc) {
		MCR_PR_ERR("%s:%d] platform_init fail %d\n", __func__,
			__LINE__, rc);
		return rc;
	}

	MCR_DBG("\n%s:%d] platform resources - mem 0x%p, base 0x%p, irq %d\n",
		__func__, __LINE__, pmercury_dev->mem, pmercury_dev->base,
		pmercury_dev->irq);

	msm_mercury_q_cleanup(&pmercury_dev->evt_q);
	msm_mercury_core_init();

	MCR_DBG("\n%s:%d] success\n", __func__, __LINE__);
	return rc;
}

int __msm_mercury_release(struct msm_mercury_device *pmercury_dev)
{
	MCR_DBG("%s:%d] Enter\n", __func__, __LINE__);
	mutex_lock(&pmercury_dev->lock);
	if (!pmercury_dev->open_count) {
		MCR_PR_ERR(KERN_ERR "%s: not opened\n", __func__);
		mutex_unlock(&pmercury_dev->lock);
		return -EINVAL;
	}
	pmercury_dev->open_count--;
	mutex_unlock(&pmercury_dev->lock);

	msm_mercury_q_cleanup(&pmercury_dev->evt_q);

	if (pmercury_dev->open_count)
		MCR_PR_ERR(KERN_ERR "%s: multiple opens\n", __func__);

	if (pmercury_dev->open_count)
		MCR_PR_ERR(KERN_ERR "%s: multiple opens\n", __func__);


	msm_mercury_platform_release(pmercury_dev->mem, pmercury_dev->base,
		pmercury_dev->irq, pmercury_dev);

	return 0;
}

int msm_mercury_ioctl_hw_cmd(struct msm_mercury_device *pmercury_dev,
	void * __user arg)
{
	struct msm_mercury_hw_cmd hw_cmd;
	int is_copy_to_user;
	int rc = 0;

	rc = copy_from_user(&hw_cmd, arg, sizeof(struct msm_mercury_hw_cmd));
	if (rc) {
		MCR_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	is_copy_to_user = msm_mercury_hw_exec_cmds(&hw_cmd, 1);
	if (is_copy_to_user >= 0) {
		rc = copy_to_user(arg, &hw_cmd, sizeof(hw_cmd));

		if (rc) {
			MCR_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
			return -EFAULT;
		}
	}

	return 0;
}

int msm_mercury_ioctl_hw_cmds(struct msm_mercury_device *pmercury_dev,
	void * __user arg)
{
	int is_copy_to_user;
	int len;
	uint32_t m;
	struct msm_mercury_hw_cmds *hw_cmds_p;
	struct msm_mercury_hw_cmd *hw_cmd_p;

	if (copy_from_user(&m, arg, sizeof(m))) {
		MCR_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	len = sizeof(struct msm_mercury_hw_cmds) +
		sizeof(struct msm_mercury_hw_cmd) * (m - 1);
	hw_cmds_p = kmalloc(len, GFP_KERNEL);
	if (!hw_cmds_p) {
		MCR_PR_ERR("[%d]%s() no mem %d\n", __LINE__, __func__, len);
		return -EFAULT;
	}

	if (copy_from_user(hw_cmds_p, arg, len)) {
		MCR_PR_ERR("[%d]%s Fail to copy hw_cmds of len %d from user\n",
			__LINE__, __func__, len);
		kfree(hw_cmds_p);
		return -EFAULT;
	}

	hw_cmd_p = (struct msm_mercury_hw_cmd *) &(hw_cmds_p->hw_cmd);

	is_copy_to_user = msm_mercury_hw_exec_cmds(hw_cmd_p, m);

	if (is_copy_to_user >= 0) {
		if (copy_to_user(arg, hw_cmds_p, len)) {
			MCR_PR_ERR("%s:%d] failed\n", __func__, __LINE__);
			kfree(hw_cmds_p);
			return -EFAULT;
		}
	}
	kfree(hw_cmds_p);
	return 0;
}

int msm_mercury_ioctl_reset(struct msm_mercury_device *pmercury_dev,
	void * __user arg)
{
	int rc = 0;

	MCR_DBG("(%d)%s() Enter\n", __LINE__, __func__);
	rc = msm_mercury_core_reset();

	return rc;
}

long __msm_mercury_ioctl(struct msm_mercury_device *pmercury_dev,
	unsigned int cmd, unsigned long arg)
{
	int rc = 0;

	switch (cmd) {
	case MSM_MCR_IOCTL_GET_HW_VERSION:
		rc = msm_mercury_ioctl_magic_code(pmercury_dev,
			(void __user *) arg);
		break;

	case MSM_MCR_IOCTL_RESET:
		rc = msm_mercury_ioctl_reset(pmercury_dev, (void __user *) arg);
		break;

	case MSM_MCR_IOCTL_EVT_GET:
		rc = msm_mercury_evt_get(pmercury_dev, (void __user *) arg);
		break;

	case MSM_MCR_IOCTL_EVT_GET_UNBLOCK:
		rc = msm_mercury_evt_get_unblock(pmercury_dev);
		break;

	case MSM_MCR_IOCTL_HW_CMD:
		rc = msm_mercury_ioctl_hw_cmd(pmercury_dev,
			(void __user *) arg);
		break;

	case MSM_MCR_IOCTL_HW_CMDS:
		rc = msm_mercury_ioctl_hw_cmds(pmercury_dev,
			(void __user *) arg);
		break;

	case MSM_MCR_IOCTL_INPUT_BUF_CFG:
		rc = msm_mercury_input_buf_cfg(pmercury_dev,
			(void __user *) arg);
		break;

	case MSM_MCR_IOCTL_OUTPUT_BUF_CFG:
		rc = msm_mercury_output_buf_cfg(pmercury_dev,
			(void __user *) arg);
		break;

	case MSM_MCR_IOCTL_OUTPUT_GET:
		rc = msm_mercury_output_get(pmercury_dev,
			(void __user *) arg);
		break;

	case MSM_MCR_IOCTL_INPUT_GET:
		rc = msm_mercury_input_get(pmercury_dev,
			(void __user *) arg);
		break;

	case MSM_MCR_IOCTL_TEST_DUMP_REGION:
		rc = msm_mercury_ioctl_dump_regs();
		break;

	default:
		printk(KERN_ERR "(%d)%s()  cmd = %d not supported\n",
			__LINE__, __func__, _IOC_NR(cmd));
		rc = -EINVAL;
		break;
	}
	return rc;
}

struct msm_mercury_device *__msm_mercury_init(struct platform_device *pdev)
{
	struct msm_mercury_device *pmercury_dev;
	pmercury_dev = kzalloc(sizeof(struct msm_mercury_device), GFP_ATOMIC);
	if (!pmercury_dev) {
		printk(KERN_ERR "%s:%d]no mem\n", __func__, __LINE__);
		return NULL;
	}

	mutex_init(&pmercury_dev->lock);

	pmercury_dev->pdev = pdev;

	msm_mercury_q_init("evt_q", &pmercury_dev->evt_q);

	return pmercury_dev;
}

int __msm_mercury_exit(struct msm_mercury_device *pmercury_dev)
{
	mutex_destroy(&pmercury_dev->lock);
	kfree(pmercury_dev);
	return 0;
}


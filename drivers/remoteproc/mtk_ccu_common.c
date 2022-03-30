// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>

#include "mtk_ccu_common.h"

static inline unsigned int mtk_ccu_mstojiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

void mtk_ccu_memclr(void *dst, int len)
{
	int i = 0;
	uint32_t *dstPtr = (uint32_t *)dst;

	for (i = 0; i < len/4; i++)
		writel(0, dstPtr + i);
}

void mtk_ccu_memcpy(void *dst, const void *src, uint32_t len)
{
	int i, copy_len;
	uint32_t data = 0;
	uint32_t align_data = 0;

	for (i = 0; i < len/4; ++i)
		writel(*((uint32_t *)src+i), (uint32_t *)dst+i);

	if ((len % 4) != 0) {
		copy_len = len & ~(0x3);
		for (i = 0; i < 4; ++i) {
			if (i < (len%4)) {
				data = *((char *)src + copy_len + i);
				align_data += data << (8 * i);
			}
		}
		writel(align_data, (uint32_t *)dst + len/4);
	}
}

struct mtk_ccu_mem_info *mtk_ccu_get_meminfo(struct mtk_ccu *ccu,
	enum mtk_ccu_buffer_type type)
{
	if (type >= MTK_CCU_BUF_MAX)
		return NULL;
	else
		return &ccu->buffer_handle[type].meminfo;
}

static int mtk_ccu_open(struct inode *inode, struct file *flip)
{
	struct mtk_ccu *ccu = container_of(inode->i_cdev,
					   struct mtk_ccu,
					   ccu_cdev);

	flip->private_data = ccu;
	return 0;
}

static int mtk_ccu_release(struct inode *inode, struct file *flip)
{
	return 0;
}


static int mtk_ccu_wakeup(struct mtk_ccu *ccu)
{
	ccu->bWaitCond = true;
	wake_up_interruptible(&ccu->WaitQueueHead);
	return 0;
}

static int mtk_ccu_waitirq(struct mtk_ccu *ccu)
{
	signed int ret = 0, timeout = 0;

	timeout = wait_event_interruptible_timeout(
		ccu->WaitQueueHead,
		ccu->bWaitCond,
		mtk_ccu_mstojiffies(100));
	ccu->bWaitCond = false;


	if (timeout > 0) {
		dev_info(ccu->dev, "remain time:%d, log_idx: %d\n",
			timeout, ccu->g_LogBufIdx);
		ret = ccu->g_LogBufIdx;
	}

	return ret;
}

static long mtk_ccu_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg)
{
	int ret = 0;
	int log_idx;
	uint32_t log_level[2] = {0};
	struct mtk_ccu_buffer log_info = {0};
	struct mtk_ccu *ccu = flip->private_data;

	switch (cmd) {
	case MTK_CCU_IOCTL_SET_LOG_LEVEL:
	{

		ret = copy_from_user(log_level, (void *)arg, sizeof(log_level));
		if (ret) {
			dev_err(ccu->dev, "set log level failed\n");
			return -EFAULT;
		}
		ccu->log_level = log_level[0];
		ccu->log_taglevel = log_level[1];

		break;
	}
	case MTK_CCU_IOCTL_WAIT_IRQ:
	{
		log_idx = mtk_ccu_waitirq(ccu);
		if (log_idx < 0) {
			log_info = ccu->log_info[3];
			log_info.buf_idx = log_idx;
		} else if (log_idx > 0) {
			log_info = ccu->log_info[log_idx-1];
			log_info.buf_idx = log_idx;
		}
		ret = copy_to_user((void *)arg, &log_info,
			sizeof(struct mtk_ccu_buffer));
		if (ret) {
			dev_err(ccu->dev, "copy_to_user failed\n");
			ret = -EFAULT;
		}
		break;
	}
	case MTK_CCU_IOCTL_FLUSH_LOG:
	{
		mtk_ccu_wakeup(ccu);
		break;
	}

	default:
		dev_warn(ccu->dev, "ioctl:No such command!\n");
		ret = -EINVAL;
		break;
	}

	if (ret != 0) {
		dev_err(ccu->dev, "fail, cmd(%d), cmd_nr(%d)",
			cmd, _IOC_NR(cmd));
		dev_err(ccu->dev, "fail, (process, pid, tgid)=(%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);
	}
	return ret;
}

static int mtk_ccu_mmap(struct file *flip,
	struct vm_area_struct *vma)
{
	unsigned long length = (vma->vm_end - vma->vm_start);
	struct mtk_ccu *ccu = flip->private_data;
	int ret;

	if (length > (MTK_CCU_DRAM_LOG_BUF_SIZE * 4))
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = dma_mmap_attrs(ccu->dev, vma, ccu->ext_buf.meminfo.va,
		ccu->ext_buf.meminfo.mva, length, DMA_ATTR_WRITE_COMBINE);
	if (ret)
		dev_err(ccu->dev, "Remapping memory failed, error: %d\n", ret);
	return ret;
}

static const struct file_operations mtk_ccu_fops = {
	.owner = THIS_MODULE,
	.open = mtk_ccu_open,
	.release = mtk_ccu_release,
	.unlocked_ioctl = mtk_ccu_ioctl,
	.mmap = mtk_ccu_mmap,
};

void mtk_ccu_unreg_chardev(struct mtk_ccu *ccu)
{
	cdev_del(&ccu->ccu_cdev);
	unregister_chrdev_region(ccu->dev_no, 1);
}

int mtk_ccu_reg_chardev(struct mtk_ccu *ccu)
{
	struct device *dev = ccu->dev;
	int cdev_ret = -1;
	int alloc_ret = -1;
	int ret = 0;

	alloc_ret = alloc_chrdev_region(&ccu->dev_no, 0, 1, MTK_CCU_DEV_NAME);
	if (alloc_ret) {
		dev_err(dev, "failed to alloc chr_dev region, %d\n", alloc_ret);
		goto ERR;
	}

	cdev_init(&ccu->ccu_cdev, &mtk_ccu_fops);
	ccu->ccu_cdev.owner = THIS_MODULE;
	cdev_ret = cdev_add(&ccu->ccu_cdev, ccu->dev_no, 1);
	if (cdev_ret) {
		dev_err(dev, "Attach file operation failed, %d\n", cdev_ret);
		goto ERR;
	}

	ccu->ccu_class = class_create(THIS_MODULE, "ccurprocdrv");
	if (IS_ERR(ccu->ccu_class)) {
		ret = PTR_ERR(ccu->ccu_class);
		dev_err(dev, "Unable to create class, err = %d\n", ret);
		goto ERR;
	}
	ccu->ccu_cdev_dev = device_create(
	ccu->ccu_class, dev, ccu->dev_no, NULL, MTK_CCU_DEV_NAME);
	if (IS_ERR(ccu->ccu_cdev_dev)) {
		ret = PTR_ERR(ccu->ccu_cdev_dev);
		dev_err(ccu->dev,
		"Failed to create device: /dev/%s, err = %d",
		MTK_CCU_DEV_NAME, ret);
		goto ERR;
	}


	return 0;
ERR:
	if (alloc_ret == 0)
		unregister_chrdev_region(ccu->dev_no, 1);
	if (cdev_ret == 0)
		cdev_del(&ccu->ccu_cdev);
	return -EFAULT;
}

void mtk_ccu_ipc_log_handle(uint32_t data, uint32_t len, void *priv)
{
	struct mtk_ccu *ccu = priv;

	dev_info(ccu->dev, "got APMCU_FLUSH_LOG:%d\n", data);
	ccu->bWaitCond = true;
	ccu->g_LogBufIdx = (uint32_t)data;
	wake_up_interruptible(&ccu->WaitQueueHead);
}

void mtk_ccu_ipc_assert_handle(uint32_t data, uint32_t len, void *priv)
{
	struct mtk_ccu *ccu = priv;

	dev_err(ccu->dev, "got AP_ISR_CCU_ASSERT:%d\n", data);
	ccu->bWaitCond = true;
	ccu->g_LogBufIdx = 0xFFFFFFFF;
	wake_up_interruptible(&ccu->WaitQueueHead);
}

void mtk_ccu_ipc_warning_handle(uint32_t data, uint32_t len, void *priv)
{
	struct mtk_ccu *ccu = priv;

	dev_err(ccu->dev, "got AP_ISR_CCU_WARNING:%d\n", data);
	ccu->bWaitCond = true;
	ccu->g_LogBufIdx = -2;
	wake_up_interruptible(&ccu->WaitQueueHead);
}

MODULE_DESCRIPTION("MTK CCU Rproc Driver");
MODULE_LICENSE("GPL v2");

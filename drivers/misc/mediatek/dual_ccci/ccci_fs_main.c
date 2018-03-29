/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_fs.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT65XX CCCI FS Proxy Driver
 *
 ****************************************************************************/

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <ccci.h>
#define CCCI_FS_DEVNAME  "ccci_fs"

/* enable fs_tx or fs_rx log */
unsigned int fs_tx_debug_enable[MAX_MD_NUM] = { 0 };
unsigned int fs_rx_debug_enable[MAX_MD_NUM] = { 0 };

struct fs_ctl_block_t {
	unsigned int fs_md_id;
	spinlock_t fs_spinlock;
	dev_t fs_dev_num;
	struct cdev fs_cdev;
	struct fs_stream_buffer_t *fs_buffers;
	int fs_buffers_phys_addr;
	struct kfifo fs_fifo;
	int reset_handle;
	wait_queue_head_t fs_waitq;
	struct wake_lock fs_wake_lock;
	char fs_wakelock_name[16];
	int fs_smem_size;
};

static struct fs_ctl_block_t *fs_ctl_block[MAX_MD_NUM];

/*   will be called when modem sends us something. */
/*   we will then copy it to the tty's buffer. */
/*   this is essentially the "read" fops. */
static void ccci_fs_callback(void *private)
{
	unsigned long flag;
	struct logic_channel_info_t *ch_info = (struct logic_channel_info_t *) private;
	struct ccci_msg_t msg;
	struct fs_ctl_block_t *ctl_b = (struct fs_ctl_block_t *) ch_info->m_owner;

	spin_lock_irqsave(&ctl_b->fs_spinlock, flag);
	while (get_logic_ch_data(ch_info, &msg)) {
		if (msg.channel == CCCI_FS_RX) {
			if (fs_rx_debug_enable[ctl_b->fs_md_id]) {
				CCCI_DBG_MSG(ctl_b->fs_md_id, "fs ",
					     "fs_callback: %08X  %08X  %08X\n",
					     msg.data0, msg.data1,
					     msg.reserved);
			}

			if (kfifo_in
			    (&ctl_b->fs_fifo, (unsigned char *)&msg.reserved,
			     sizeof(msg.reserved)) == sizeof(msg.reserved)) {
				wake_up_interruptible(&ctl_b->fs_waitq);
				wake_lock_timeout(&ctl_b->fs_wake_lock, HZ / 2);
			} else {
				CCCI_DBG_MSG(ctl_b->fs_md_id, "fs ",
					     "[Error]Unable to put new request into fifo\n");
			}
		}
	}
	spin_unlock_irqrestore(&ctl_b->fs_spinlock, flag);
}

static int ccci_fs_get_index(int md_id)
{
	int ret;
	unsigned long flag;
	struct fs_ctl_block_t *ctl_b;

	if (unlikely(fs_ctl_block[md_id] == NULL)) {
		CCCI_MSG_INF(md_id, "fs ",
			     "fs_get_index: fata error, fs_ctl_b is NULL\n");
		return -EPERM;
	}
	ctl_b = fs_ctl_block[md_id];

	CCCI_FS_MSG(md_id, "get_fs_index++\n");

	if (wait_event_interruptible
	    (ctl_b->fs_waitq, kfifo_len(&ctl_b->fs_fifo) != 0) != 0) {
		if (fs_rx_debug_enable[md_id])
			CCCI_MSG_INF(md_id, "fs ",
				     "fs_get_index: Interrupted by syscall.signal_pend\n");
		return -ERESTARTSYS;
	}

	spin_lock_irqsave(&ctl_b->fs_spinlock, flag);
	if (kfifo_out(&ctl_b->fs_fifo, (unsigned char *)&ret, sizeof(int)) !=
	    sizeof(int)) {
		spin_unlock_irqrestore(&ctl_b->fs_spinlock, flag);
		CCCI_MSG_INF(md_id, "fs ", "get fs index fail from fifo\n");
		return -EFAULT;
	}

	spin_unlock_irqrestore(&ctl_b->fs_spinlock, flag);

	if (fs_rx_debug_enable[md_id])
		CCCI_MSG_INF(md_id, "fs ", "fs_index=%d\n", ret);

	CCCI_FS_MSG(md_id, "get_fs_index--\n");
	return ret;
}

static int ccci_fs_send(int md_id, unsigned long arg)
{
	void __user *argp;
	struct ccci_msg_t msg;
	struct fs_stream_msg_t message;
	int ret = 0;
	int xmit_retry = 0;
	struct fs_ctl_block_t *ctl_b;

	CCCI_FS_MSG(md_id, "ccci_fs_send++\n");

	if (unlikely(fs_ctl_block[md_id] == NULL)) {
		CCCI_MSG_INF(md_id, "fs ",
			     "fs_get_index: fatal error, fs_ctl_b is NULL\n");
		return -EPERM;
	}
	ctl_b = fs_ctl_block[md_id];

	argp = (void __user *)arg;
	if (copy_from_user((void *)&message, argp, sizeof(struct fs_stream_msg_t))) {
		CCCI_MSG_INF(md_id, "fs ",
			     "ccci_fs_send: copy_from_user fail!\n");
		return -EFAULT;
	}

	msg.data0 =
	    ctl_b->fs_buffers_phys_addr - get_md2_ap_phy_addr_fixed() +
	    (sizeof(struct fs_stream_buffer_t) * message.index);
	msg.data1 = message.length + 4;
	msg.channel = CCCI_FS_TX;
	msg.reserved = message.index;

	if (fs_tx_debug_enable[md_id]) {
		CCCI_MSG_INF(md_id, "fs ", "fs_send: %08X %08X %08X\n",
			     msg.data0, msg.data1, msg.reserved);
	}

	mb(); /* wait write done */
	do {
		ret = ccci_message_send(md_id, &msg, 1);
		if (ret == sizeof(struct ccci_msg_t))
			break;

		if (ret == -CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL) {
			xmit_retry++;
			msleep(20);
			if ((xmit_retry & 0xF) == 0) {
				CCCI_MSG_INF(md_id, "fs ",
					     "fs_chr has retried %d times\n",
					     xmit_retry);
			}
		} else {
			break;
		}
	} while (1);

	if (ret != sizeof(struct ccci_msg_t)) {
		CCCI_MSG_INF(md_id, "fs ",
			     "ccci_fs_send fail <ret=%d>: %08X, %08X, %08X\n",
			     ret, msg.data0, msg.data1, msg.reserved);
		return ret;
	}

	CCCI_FS_MSG(md_id, "ccci_fs_send--\n");

	return 0;
}

static int ccci_fs_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long off, start, len;
	struct fs_ctl_block_t *ctl_b;
	int md_id;

	ctl_b = (struct fs_ctl_block_t *) file->private_data;
	md_id = ctl_b->fs_md_id;

	CCCI_FS_MSG(md_id, "mmap++\n");

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT)) {
		CCCI_MSG_INF(md_id, "fs ",
			     "ccci_fs_mmap: vm_pgoff too large\n");
		return -EINVAL;
	}

	off = vma->vm_pgoff << PAGE_SHIFT;
	start = (unsigned long)ctl_b->fs_buffers_phys_addr;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + ctl_b->fs_smem_size);

	if ((vma->vm_end - vma->vm_start + off) > len) {
		CCCI_MSG_INF(md_id, "fs ",
			     "ccci_fs_mmap: memory require over ccci_fs_smem size\n");
		return -1;	/*  mmap return -1 when fail */
	}

	off += start & PAGE_MASK;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	CCCI_FS_MSG(md_id, "mmap--\n");

	return remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static long ccci_fs_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	int ret;
	int md_id;
	int md_stage;
	struct fs_ctl_block_t *ctl_b;

	ctl_b = (struct fs_ctl_block_t *) file->private_data;
	md_id = ctl_b->fs_md_id;
	md_stage = get_curr_md_state(md_id);
	switch (cmd) {
	case CCCI_FS_IOCTL_GET_INDEX:
		ret = ccci_fs_get_index(md_id);
		/*Check FS still under working, no need time out, so resched bootup timer*/
		if (ret >= 0 && md_stage != MD_BOOT_STAGE_2)
			ccci_stop_bootup_timer(md_id);
		break;

	case CCCI_FS_IOCTL_SEND:
		ret = ccci_fs_send(md_id, arg);
		/*Check FS still under working, no need time out, so resched bootup timer*/
		if (ret == 0 && md_stage == MD_BOOT_STAGE_1)
			ccci_start_bootup_timer(md_id, BOOT_TIMER_HS2);
		break;

	default:
		CCCI_MSG_INF(md_id, "fs ",
			     "ccci_fs_ioctl: [Error]unknown ioctl:%d\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

/*  clear kfifo invalid data which may not be processed before close operation */
void ccci_fs_resetfifo(int md_id)
{
	struct fs_ctl_block_t *ctl_b = fs_ctl_block[md_id];
	unsigned long flag;

	CCCI_MSG("ccci_fs_resetfifo\n");

	/*  Reset FS KFIFO */
	spin_lock_irqsave(&ctl_b->fs_spinlock, flag);
	kfifo_reset(&ctl_b->fs_fifo);
	spin_unlock_irqrestore(&ctl_b->fs_spinlock, flag);
}

static int ccci_fs_open(struct inode *inode, struct file *file)
{
	int md_id;
	int major;
	struct fs_ctl_block_t *ctl_b;

	major = imajor(inode);
	md_id = get_md_id_by_dev_major(major);
	if (md_id < 0) {
		CCCI_MSG("FS open fail: invalid major id:%d\n", major);
		return -1;
	}
	CCCI_MSG_INF(md_id, "fs ", "FS open by %s\n", current->comm);

	ctl_b = fs_ctl_block[md_id];
	file->private_data = ctl_b;
	nonseekable_open(inode, file);

	/*  modem reset registration. */
	ctl_b->reset_handle = ccci_reset_register(md_id, "CCCI_FS");
	if (ctl_b->reset_handle < 0)
		CCCI_ERR_INF(md_id, "fs ", "ctl_b->reset_handle %d < 0\n", ctl_b->reset_handle);
	return 0;
}

static int ccci_fs_release(struct inode *inode, struct file *file)
{
	int md_id;
	int major;
	struct fs_ctl_block_t *ctl_b;

	major = imajor(inode);
	md_id = get_md_id_by_dev_major(major);
	if (md_id < 0) {
		CCCI_MSG("FS release fail: invalid major id:%d\n", major);
		return -1;
	}
	CCCI_MSG_INF(md_id, "fs ", "FS release by %s\n", current->comm);

	ctl_b = fs_ctl_block[md_id];

	memset(ctl_b->fs_buffers, 0, ctl_b->fs_smem_size);
	ccci_user_ready_to_reset(md_id, ctl_b->reset_handle);
	/*boot up timer maybe not stop*/
	CCCI_MSG_INF(md_id, "fs ", "Try to stop bootup time %s\n", current->comm);
	ccci_stop_bootup_timer(md_id);
	return 0;
}

static int ccci_fs_start(int md_id)
{
	struct fs_ctl_block_t *ctl_b;
	unsigned long flag;

	if (unlikely(fs_ctl_block[md_id] == NULL)) {
		CCCI_MSG_INF(md_id, "fs ",
			     "ccci_fs_start: fatal error, fs_ctl_b is NULL\n");
		return -CCCI_ERR_FATAL_ERR;
	}
	ctl_b = fs_ctl_block[md_id];

	if (0 !=
	    kfifo_alloc(&ctl_b->fs_fifo, sizeof(unsigned) * CCCI_FS_MAX_BUFFERS,
			GFP_KERNEL)) {
		CCCI_MSG_INF(md_id, "fs ", "ccci_fs_start: kfifo alloc fail\n");
		return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
	}

	/*  Reset FS KFIFO */
	spin_lock_irqsave(&ctl_b->fs_spinlock, flag);
	kfifo_reset(&ctl_b->fs_fifo);
	spin_unlock_irqrestore(&ctl_b->fs_spinlock, flag);

	/*  modem related channel registration. */
	ccci_fs_base_req(md_id, (int *)&ctl_b->fs_buffers, &ctl_b->fs_buffers_phys_addr,
		&ctl_b->fs_smem_size);

	register_to_logic_ch(md_id, CCCI_FS_RX, ccci_fs_callback, ctl_b);

	return 0;
}

static void ccci_fs_stop(int md_id)
{
	struct fs_ctl_block_t *ctl_b;

	if (unlikely(fs_ctl_block[md_id] == NULL)) {
		CCCI_MSG_INF(md_id, "fs ",
			     "ccci_fs_stop: fatal error, fs_ctl_b is NULL\n");
		return;
	}
	ctl_b = fs_ctl_block[md_id];
	if (ctl_b->fs_buffers != NULL) {
		kfifo_free(&ctl_b->fs_fifo);
		un_register_to_logic_ch(md_id, CCCI_FS_RX);
		ctl_b->fs_buffers = NULL;
		ctl_b->fs_buffers_phys_addr = 0;
	}
}

static const struct file_operations fs_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ccci_fs_ioctl,
	.open = ccci_fs_open,
	.mmap = ccci_fs_mmap,
	.release = ccci_fs_release,
};

int __init ccci_fs_init(int md_id)
{
	int ret;
	int major, minor;
	struct fs_ctl_block_t *ctl_b;

	ret = get_dev_id_by_md_id(md_id, "fs", &major, &minor);
	if (ret < 0) {
		CCCI_MSG("ccci_fs_init: get md device number failed(%d)\n",
			 ret);
		return ret;
	}
	/*  Allocate fs ctrl struct memory */
	ctl_b = kmalloc(sizeof(struct fs_ctl_block_t), GFP_KERNEL);
	if (ctl_b == NULL)
		return -CCCI_ERR_GET_MEM_FAIL;
	memset(ctl_b, 0, sizeof(struct fs_ctl_block_t));

	fs_ctl_block[md_id] = ctl_b;

	/*  Init ctl_b */
	ctl_b->fs_md_id = md_id;
	spin_lock_init(&ctl_b->fs_spinlock);
	init_waitqueue_head(&ctl_b->fs_waitq);
	ctl_b->fs_dev_num = MKDEV(major, minor);
	snprintf(ctl_b->fs_wakelock_name, sizeof(ctl_b->fs_wakelock_name),
		 "ccci%d_fs", (md_id + 1));
	wake_lock_init(&ctl_b->fs_wake_lock, WAKE_LOCK_SUSPEND,
		       ctl_b->fs_wakelock_name);

	ret =
	    register_chrdev_region(ctl_b->fs_dev_num, 1,
				   ctl_b->fs_wakelock_name);
	if (ret) {
		CCCI_MSG_INF(md_id, "fs ",
			     "ccci_fs_init: Register char device failed(%d)\n",
			     ret);
		goto _REG_CHR_REGION_FAIL;
	}

	cdev_init(&ctl_b->fs_cdev, &fs_fops);
	ctl_b->fs_cdev.owner = THIS_MODULE;
	ctl_b->fs_cdev.ops = &fs_fops;

	ret = cdev_add(&ctl_b->fs_cdev, ctl_b->fs_dev_num, 1);
	if (ret) {
		CCCI_MSG_INF(md_id, "fs ", "cdev_add fail(%d)\n", ret);
		unregister_chrdev_region(ctl_b->fs_dev_num, 1);
		goto _REG_CHR_REGION_FAIL;
	}

	ret = ccci_fs_start(md_id);
	if (ret) {
		CCCI_MSG_INF(md_id, "fs ", "FS initialize fail\n");
		goto _CCCI_FS_START_FAIL;
	}

	CCCI_FS_MSG(md_id, "Init complete, device major number = %d\n",
		    MAJOR(ctl_b->fs_dev_num));

	return 0;
 _CCCI_FS_START_FAIL:
	cdev_del(&ctl_b->fs_cdev);
	unregister_chrdev_region(ctl_b->fs_dev_num, 1);

 _REG_CHR_REGION_FAIL:
	kfree(ctl_b);
	fs_ctl_block[md_id] = NULL;

	return ret;
}

void __exit ccci_fs_exit(int md_id)
{
	struct fs_ctl_block_t *ctl_b = fs_ctl_block[md_id];

	if (unlikely(ctl_b == NULL)) {
		CCCI_MSG_INF(md_id, "fs ", "ccci_fs_exit: fs_ctl_b is NULL\n");
		return;
	}

	ccci_fs_stop(md_id);

	cdev_del(&ctl_b->fs_cdev);
	unregister_chrdev_region(ctl_b->fs_dev_num, 1);
	wake_lock_destroy(&ctl_b->fs_wake_lock);
	kfree(ctl_b);
	fs_ctl_block[md_id] = NULL;
}

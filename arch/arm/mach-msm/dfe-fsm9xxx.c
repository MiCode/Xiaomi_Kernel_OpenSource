/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <mach/msm_iomap.h>

#include <linux/fsm_dfe_hh.h>

/*
 * DFE of FSM9XXX
 */

#define HH_ADDR_MASK			0x000ffffc
#define HH_OFFSET_VALID(offset)		(((offset) & ~HH_ADDR_MASK) == 0)
#define HH_REG_IOADDR(offset)		((uint8_t *) MSM_HH_BASE + (offset))
#define HH_MAKE_OFFSET(blk, adr)	(((blk)&0x1F)<<15|((adr)&0x1FFF)<<2)

#define HH_REG_SCPN_IREQ_MASK		HH_REG_IOADDR(HH_MAKE_OFFSET(5, 0x12))
#define HH_REG_SCPN_IREQ_FLAG		HH_REG_IOADDR(HH_MAKE_OFFSET(5, 0x13))

/*
 * Device private information per device node
 */

#define HH_IRQ_FIFO_SIZE		64
#define HH_IRQ_FIFO_EMPTY(pdev)		((pdev)->irq_fifo_head == \
					(pdev)->irq_fifo_tail)
#define HH_IRQ_FIFO_FULL(pdev)		((((pdev)->irq_fifo_tail + 1) % \
					HH_IRQ_FIFO_SIZE) == \
					(pdev)->irq_fifo_head)

static struct hh_dev_node_info {
	spinlock_t hh_lock;
	char irq_fifo[HH_IRQ_FIFO_SIZE];
	unsigned int irq_fifo_head, irq_fifo_tail;
	wait_queue_head_t wq;
} hh_dev_info;

/*
 * Device private information per file
 */

struct hh_dev_file_info {
	/* Buffer */
	unsigned int *parray;
	unsigned int array_num;

	struct dfe_command_entry *pcmd;
	unsigned int cmd_num;
};

/*
 * File interface
 */

static int hh_open(struct inode *inode, struct file *file)
{
	struct hh_dev_file_info *pdfi;

	/* private data allocation */
	pdfi = kmalloc(sizeof(*pdfi), GFP_KERNEL);
	if (pdfi == NULL)
		return -ENOMEM;
	file->private_data = pdfi;

	/* buffer initialization */
	pdfi->parray = NULL;
	pdfi->array_num = 0;
	pdfi->pcmd = NULL;
	pdfi->cmd_num = 0;

	return 0;
}

static int hh_release(struct inode *inode, struct file *file)
{
	struct hh_dev_file_info *pdfi;

	pdfi = (struct hh_dev_file_info *) file->private_data;

	kfree(pdfi->parray);
	pdfi->parray = NULL;
	pdfi->array_num = 0;

	kfree(pdfi->pcmd);
	pdfi->pcmd = NULL;
	pdfi->cmd_num = 0;

	kfree(file->private_data);
	file->private_data = NULL;

	return 0;
}

static ssize_t hh_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	signed char irq = -1;
	unsigned long irq_flags;

	do {
		spin_lock_irqsave(&hh_dev_info.hh_lock, irq_flags);
		if (!HH_IRQ_FIFO_EMPTY(&hh_dev_info)) {
			irq = hh_dev_info.irq_fifo[hh_dev_info.irq_fifo_head];
			if (++hh_dev_info.irq_fifo_head == HH_IRQ_FIFO_SIZE)
				hh_dev_info.irq_fifo_head = 0;
		}
		spin_unlock_irqrestore(&hh_dev_info.hh_lock, irq_flags);

		if (irq < 0)
			if (wait_event_interruptible(hh_dev_info.wq,
				!HH_IRQ_FIFO_EMPTY(&hh_dev_info)) < 0)
				break;
	} while (irq < 0);

	if (irq < 0) {
		/* No pending interrupt */
		return 0;
	} else {
		put_user(irq, buf);
		return 1;
	}

	return 0;
}

static ssize_t hh_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	return 0;
}

static long hh_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	unsigned int __user *argp = (unsigned int __user *) arg;
	struct hh_dev_file_info *pdfi =
		(struct hh_dev_file_info *) file->private_data;

	switch (cmd) {
	case DFE_IOCTL_READ_REGISTER:
		{
			unsigned int offset, value;

			if (get_user(offset, argp))
				return -EFAULT;
			if (!HH_OFFSET_VALID(offset))
				return -EINVAL;
			value = __raw_readl(HH_REG_IOADDR(offset));
			if (put_user(value, argp))
				return -EFAULT;
		}
		break;

	case DFE_IOCTL_WRITE_REGISTER:
		{
			struct dfe_write_register_param param;

			if (copy_from_user(&param, argp, sizeof param))
				return -EFAULT;
			if (!HH_OFFSET_VALID(param.offset))
				return -EINVAL;
			__raw_writel(param.value,
				HH_REG_IOADDR(param.offset));
		}
		break;

	case DFE_IOCTL_WRITE_REGISTER_WITH_MASK:
		{
			struct dfe_write_register_mask_param param;
			unsigned int value;
			unsigned long irq_flags;

			if (copy_from_user(&param, argp, sizeof param))
				return -EFAULT;
			if (!HH_OFFSET_VALID(param.offset))
				return -EINVAL;
			spin_lock_irqsave(&hh_dev_info.hh_lock,
				irq_flags);
			value = __raw_readl(HH_REG_IOADDR(param.offset));
			value &= ~param.mask;
			value |= param.value & param.mask;
			__raw_writel(value, HH_REG_IOADDR(param.offset));
			spin_unlock_irqrestore(&hh_dev_info.hh_lock,
				irq_flags);
		}
		break;

	case DFE_IOCTL_READ_REGISTER_ARRAY:
	case DFE_IOCTL_WRITE_REGISTER_ARRAY:
		{
			struct dfe_read_write_array_param param;
			unsigned int req_sz;
			unsigned long irq_flags;
			unsigned int i;
			void *addr;

			if (copy_from_user(&param, argp, sizeof param))
				return -EFAULT;
			if (!HH_OFFSET_VALID(param.offset))
				return -EINVAL;
			if (param.num == 0)
				break;
			req_sz = sizeof(unsigned int) * param.num;

			if (pdfi->array_num < param.num) {
				void *pmem;

				pmem = kmalloc(req_sz, GFP_KERNEL);
				if (pmem == NULL)
					return -ENOMEM;
				pdfi->parray = (unsigned int *) pmem;
				pdfi->array_num = param.num;
			}

			if (cmd == DFE_IOCTL_WRITE_REGISTER_ARRAY)
				if (copy_from_user(pdfi->parray,
					param.pArray, req_sz))
					return -EFAULT;

			addr = HH_REG_IOADDR(param.offset);

			spin_lock_irqsave(&hh_dev_info.hh_lock,
				irq_flags);
			for (i = 0; i < param.num; ++i, addr += 4) {
				if (cmd == DFE_IOCTL_READ_REGISTER_ARRAY)
					pdfi->parray[i] = __raw_readl(addr);
				else
					__raw_writel(pdfi->parray[i], addr);
			}
			spin_unlock_irqrestore(&hh_dev_info.hh_lock,
				irq_flags);

			if (cmd == DFE_IOCTL_READ_REGISTER_ARRAY)
				if (copy_to_user(pdfi->parray,
					param.pArray, req_sz))
					return -EFAULT;
		}
		break;

	case DFE_IOCTL_COMMAND:
		{
			struct dfe_command_param param;
			unsigned int req_sz;
			unsigned long irq_flags;
			unsigned int i, value;
			struct dfe_command_entry *pcmd;
			void *addr;

			if (copy_from_user(&param, argp, sizeof param))
				return -EFAULT;
			if (param.num == 0)
				break;
			req_sz = sizeof(struct dfe_command_entry) * param.num;

			if (pdfi->cmd_num < param.num) {
				void *pmem;

				pmem = kmalloc(req_sz, GFP_KERNEL);
				if (pmem == NULL)
					return -ENOMEM;
				pdfi->pcmd = (struct dfe_command_entry *) pmem;
				pdfi->cmd_num = param.num;
			}

			if (copy_from_user(pdfi->pcmd, param.pEntry, req_sz))
				return -EFAULT;

			pcmd = pdfi->pcmd;

			spin_lock_irqsave(&hh_dev_info.hh_lock,
				irq_flags);
			for (i = 0; i < param.num; ++i, ++pcmd) {
				if (!HH_OFFSET_VALID(pcmd->offset))
					return -EINVAL;
				addr = HH_REG_IOADDR(pcmd->offset);

				switch (pcmd->code) {
				case DFE_IOCTL_COMMAND_CODE_WRITE:
					__raw_writel(pcmd->value, addr);
					break;
				case DFE_IOCTL_COMMAND_CODE_WRITE_WITH_MASK:
					value = __raw_readl(addr);
					value &= ~pcmd->mask;
					value |= pcmd->value & pcmd->mask;
					__raw_writel(value, addr);
					break;
				}
			}
			spin_unlock_irqrestore(&hh_dev_info.hh_lock,
				irq_flags);
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int hh_poll(struct file *filp,
	struct poll_table_struct *wait)
{
	unsigned mask = 0;

	if (!HH_IRQ_FIFO_EMPTY(&hh_dev_info))
		mask |= POLLIN;

	if (mask == 0) {
		poll_wait(filp, &hh_dev_info.wq, wait);
		if (!HH_IRQ_FIFO_EMPTY(&hh_dev_info))
			mask |= POLLIN;
	}

	return mask;
}

static const struct file_operations hh_fops = {
	.owner = THIS_MODULE,
	.open = hh_open,
	.release = hh_release,
	.read = hh_read,
	.write = hh_write,
	.unlocked_ioctl = hh_ioctl,
	.poll = hh_poll,
};

/*
 * Interrupt handling
 */

static irqreturn_t hh_irq_handler(int irq, void *data)
{
	unsigned int irq_enable, irq_flag, irq_mask;
	int i;

	irq_enable = __raw_readl(HH_REG_SCPN_IREQ_MASK);
	irq_flag = __raw_readl(HH_REG_SCPN_IREQ_FLAG);
	irq_flag &= irq_enable;

	/* Disables interrupts */
	irq_enable &= ~irq_flag;
	__raw_writel(irq_enable, HH_REG_SCPN_IREQ_MASK);

	/* Adds the pending interrupts to irq_fifo */
	spin_lock(&hh_dev_info.hh_lock);
	for (i = 0, irq_mask = 1; i < 32; ++i, irq_mask <<= 1) {
		if (HH_IRQ_FIFO_FULL(&hh_dev_info))
			break;
		if (irq_flag & irq_mask) {
			hh_dev_info.irq_fifo[hh_dev_info.irq_fifo_tail] = \
				(char) i;
			if (++hh_dev_info.irq_fifo_tail == HH_IRQ_FIFO_SIZE)
				hh_dev_info.irq_fifo_tail = 0;
		}
	}
	spin_unlock(&hh_dev_info.hh_lock);

	/* Wakes up pending processes */
	wake_up_interruptible(&hh_dev_info.wq);

	return IRQ_HANDLED;
}

/*
 * Driver initialization & cleanup
 */

static struct miscdevice hh_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DFE_HH_DEVICE_NAME,
	.fops = &hh_fops,
};

static int __init hh_init(void)
{
	int ret;

	/* lock initialization */
	spin_lock_init(&hh_dev_info.hh_lock);

	/* interrupt handler */
	hh_dev_info.irq_fifo_head = 0;
	hh_dev_info.irq_fifo_tail = 0;
	ret = request_irq(INT_HH_SUPSS_IRQ, hh_irq_handler,
		IRQF_TRIGGER_RISING, "hh_dev", 0);
	if (ret < 0) {
		pr_err("Cannot register HH interrupt handler.\n");
		return ret;
	}

	/* wait queue */
	init_waitqueue_head(&hh_dev_info.wq);

	return misc_register(&hh_misc_dev);
}

static void __exit hh_exit(void)
{
	misc_deregister(&hh_misc_dev);
	free_irq(INT_HH_SUPSS_IRQ, 0);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Rohit Vaswani <rvaswani@codeaurora.org>");
MODULE_DESCRIPTION("Qualcomm Hammerhead Digital Front End driver");
MODULE_VERSION("1.0");

module_init(hh_init);
module_exit(hh_exit);


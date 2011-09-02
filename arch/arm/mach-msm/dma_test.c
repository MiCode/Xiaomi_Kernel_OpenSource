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
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <mach/dma.h>
#include <mach/dma_test.h>


/**********************************************************************
 * User-space testing of the DMA driver.
 * Intended to be loaded as a module.  We have a bunch of static
 * buffers that the user-side can refer to.  The main DMA is simply
 * used memory-to-memory.  Device DMA is best tested with the specific
 * device driver in question.
 */
#define MAX_TEST_BUFFERS 40
#define MAX_TEST_BUFFER_SIZE 65536
static void *(buffers[MAX_TEST_BUFFERS]);
static int sizes[MAX_TEST_BUFFERS];

/* Anything that allocates or deallocates buffers must lock with this
 * mutex. */
static DEFINE_SEMAPHORE(buffer_lock);

/* Each buffer has a semaphore associated with it that will be held
 * for the duration of any operations on that buffer.  It also must be
 * available to free the given buffer. */
static struct semaphore buffer_sems[MAX_TEST_BUFFERS];

#define buffer_up(num)  up(&buffer_sems[num])
#define buffer_down(num)  down(&buffer_sems[num])

/* Use the General Purpose DMA channel as our test channel.  This channel
 * should be available on any target. */
#define TEST_CHANNEL    DMOV_GP_CHAN

struct private {
	/* Each open instance is allowed a single pending
	 * operation. */
	struct semaphore sem;

	/* Simple command buffer.  Allocated and freed by driver. */
	/* TODO: Allocate these together. */
	dmov_s *command_ptr;

	/* Indirect. */
	u32 *command_ptr_ptr;

	/* Indicates completion with pending request. */
	struct completion complete;
};

static void free_buffers(void)
{
	int i;

	for (i = 0; i < MAX_TEST_BUFFERS; i++) {
		if (sizes[i] > 0) {
			kfree(buffers[i]);
			sizes[i] = 0;
		}
	}
}

/* Copy between two buffers, using the DMA. */

/* Allocate a buffer of a requested size. */
static int buffer_req(struct msm_dma_alloc_req *req)
{
	int i;

	if (req->size <= 0 || req->size > MAX_TEST_BUFFER_SIZE)
		return -EINVAL;

	down(&buffer_lock);

	/* Find a free buffer. */
	for (i = 0; i < MAX_TEST_BUFFERS; i++)
		if (sizes[i] == 0)
			break;

	if (i >= MAX_TEST_BUFFERS)
		goto error;

	buffers[i] = kmalloc(req->size, GFP_KERNEL | __GFP_DMA);
	if (buffers[i] == 0)
		goto error;
	sizes[i] = req->size;

	req->bufnum = i;

	up(&buffer_lock);
	return 0;

error:
	up(&buffer_lock);
	return -ENOSPC;
}

static int dma_scopy(struct msm_dma_scopy *scopy, struct private *priv)
{
	int err = 0;
	dma_addr_t mapped_cmd;
	dma_addr_t mapped_cmd_ptr;

	buffer_down(scopy->srcbuf);
	if (scopy->srcbuf != scopy->destbuf)
		buffer_down(scopy->destbuf);

	priv->command_ptr->cmd = CMD_PTR_LP | CMD_MODE_SINGLE;
	priv->command_ptr->src = dma_map_single(NULL, buffers[scopy->srcbuf],
						scopy->size, DMA_TO_DEVICE);
	priv->command_ptr->dst = dma_map_single(NULL, buffers[scopy->destbuf],
						scopy->size, DMA_FROM_DEVICE);
	priv->command_ptr->len = scopy->size;

	mapped_cmd =
	    dma_map_single(NULL, priv->command_ptr, sizeof(*priv->command_ptr),
			   DMA_TO_DEVICE);
	*(priv->command_ptr_ptr) = CMD_PTR_ADDR(mapped_cmd) | CMD_PTR_LP;

	mapped_cmd_ptr = dma_map_single(NULL, priv->command_ptr_ptr,
					sizeof(*priv->command_ptr_ptr),
					DMA_TO_DEVICE);

	msm_dmov_exec_cmd(TEST_CHANNEL,
			  DMOV_CMD_PTR_LIST | DMOV_CMD_ADDR(mapped_cmd_ptr));

	dma_unmap_single(NULL, (dma_addr_t) mapped_cmd_ptr,
			 sizeof(*priv->command_ptr_ptr), DMA_TO_DEVICE);
	dma_unmap_single(NULL, (dma_addr_t) mapped_cmd,
			 sizeof(*priv->command_ptr), DMA_TO_DEVICE);
	dma_unmap_single(NULL, (dma_addr_t) priv->command_ptr->dst,
			 scopy->size, DMA_FROM_DEVICE);
	dma_unmap_single(NULL, (dma_addr_t) priv->command_ptr->src,
			 scopy->size, DMA_TO_DEVICE);

	if (scopy->srcbuf != scopy->destbuf)
		buffer_up(scopy->destbuf);
	buffer_up(scopy->srcbuf);

	return err;
}

static int dma_test_open(struct inode *inode, struct file *file)
{
	struct private *priv;

	printk(KERN_ALERT "%s\n", __func__);

	priv = kmalloc(sizeof(struct private), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;
	file->private_data = priv;

	sema_init(&priv->sem, 1);

	/* Note, that these should be allocated together so we don't
	 * waste 32 bytes for each. */

	/* Allocate the command pointer. */
	priv->command_ptr = kmalloc(sizeof(&priv->command_ptr),
				    GFP_KERNEL | __GFP_DMA);
	if (priv->command_ptr == NULL) {
		kfree(priv);
		return -ENOSPC;
	}

	/* And the indirect pointer. */
	priv->command_ptr_ptr = kmalloc(sizeof(u32), GFP_KERNEL | __GFP_DMA);
	if (priv->command_ptr_ptr == NULL) {
		kfree(priv->command_ptr);
		kfree(priv);
		return -ENOSPC;
	}

	return 0;
}

static int dma_test_release(struct inode *inode, struct file *file)
{
	struct private *priv;

	printk(KERN_ALERT "%s\n", __func__);

	if (file->private_data != NULL) {
		priv = file->private_data;
		kfree(priv->command_ptr_ptr);
		kfree(priv->command_ptr);
	}
	kfree(file->private_data);
	file->private_data = NULL;

	return 0;
}

static long dma_test_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int err = 0;
	int tmp;
	struct msm_dma_alloc_req alloc_req;
	struct msm_dma_bufxfer xfer;
	struct msm_dma_scopy scopy;
	struct private *priv = file->private_data;

	/* Verify user arguments. */
	if (_IOC_TYPE(cmd) != MSM_DMA_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case MSM_DMA_IOALLOC:
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
			       sizeof(alloc_req)))
			return -EFAULT;
		if (__copy_from_user(&alloc_req, (void __user *)arg,
				     sizeof(alloc_req)))
			return -EFAULT;
		err = buffer_req(&alloc_req);
		if (err < 0)
			return err;
		if (__copy_to_user((void __user *)arg, &alloc_req,
				   sizeof(alloc_req)))
			return -EFAULT;
		break;

	case MSM_DMA_IOFREEALL:
		down(&buffer_lock);
		for (tmp = 0; tmp < MAX_TEST_BUFFERS; tmp++) {
			buffer_down(tmp);
			if (sizes[tmp] > 0) {
				kfree(buffers[tmp]);
				sizes[tmp] = 0;
			}
			buffer_up(tmp);
		}
		up(&buffer_lock);
		break;

	case MSM_DMA_IOWBUF:
		if (copy_from_user(&xfer, (void __user *)arg, sizeof(xfer)))
			return -EFAULT;
		if (xfer.bufnum < 0 || xfer.bufnum >= MAX_TEST_BUFFERS)
			return -EINVAL;
		buffer_down(xfer.bufnum);
		if (sizes[xfer.bufnum] == 0 ||
		    xfer.size <= 0 || xfer.size > sizes[xfer.bufnum]) {
			buffer_up(xfer.bufnum);
			return -EINVAL;
		}
		if (copy_from_user(buffers[xfer.bufnum],
				   (void __user *)xfer.data, xfer.size))
			err = -EFAULT;
		buffer_up(xfer.bufnum);
		break;

	case MSM_DMA_IORBUF:
		if (copy_from_user(&xfer, (void __user *)arg, sizeof(xfer)))
			return -EFAULT;
		if (xfer.bufnum < 0 || xfer.bufnum >= MAX_TEST_BUFFERS)
			return -EINVAL;
		buffer_down(xfer.bufnum);
		if (sizes[xfer.bufnum] == 0 ||
		    xfer.size <= 0 || xfer.size > sizes[xfer.bufnum]) {
			buffer_up(xfer.bufnum);
			return -EINVAL;
		}
		if (copy_to_user((void __user *)xfer.data, buffers[xfer.bufnum],
				 xfer.size))
			err = -EFAULT;
		buffer_up(xfer.bufnum);
		break;

	case MSM_DMA_IOSCOPY:
		if (copy_from_user(&scopy, (void __user *)arg, sizeof(scopy)))
			return -EFAULT;
		if (scopy.srcbuf < 0 || scopy.srcbuf >= MAX_TEST_BUFFERS ||
		    sizes[scopy.srcbuf] == 0 ||
		    scopy.destbuf < 0 || scopy.destbuf >= MAX_TEST_BUFFERS ||
		    sizes[scopy.destbuf] == 0 ||
		    scopy.size > sizes[scopy.destbuf] ||
		    scopy.size > sizes[scopy.srcbuf])
			return -EINVAL;
#if 0
		/* Test interface using memcpy. */
		memcpy(buffers[scopy.destbuf],
		       buffers[scopy.srcbuf], scopy.size);
#else
		err = dma_scopy(&scopy, priv);
#endif
		break;

	default:
		return -ENOTTY;
	}

	return err;
}

/**********************************************************************
 * Register ourselves as a misc device to be able to test the DMA code
 * from userspace. */

static const struct file_operations dma_test_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dma_test_ioctl,
	.open = dma_test_open,
	.release = dma_test_release,
};

static struct miscdevice dma_test_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msmdma",
	.fops = &dma_test_fops,
};
static int dma_test_init(void)
{
	int ret, i;

	ret = misc_register(&dma_test_dev);
	if (ret < 0)
		return ret;

	for (i = 0; i < MAX_TEST_BUFFERS; i++)
		sema_init(&buffer_sems[i], 1);

	printk(KERN_ALERT "%s, minor number %d\n", __func__, dma_test_dev.minor);
	return 0;
}

static void dma_test_exit(void)
{
	free_buffers();
	misc_deregister(&dma_test_dev);
	printk(KERN_ALERT "%s\n", __func__);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("David Brown, Qualcomm, Incorporated");
MODULE_DESCRIPTION("Test for MSM DMA driver");
MODULE_VERSION("1.01");

module_init(dma_test_init);
module_exit(dma_test_exit);

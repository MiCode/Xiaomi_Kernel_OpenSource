/*
 * drivers/char/okl4_pipe.c
 *
 * Copyright (c) 2015 General Dynamics
 * Copyright (c) 2015 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * OKL4 Microvisor Pipes driver.
 *
 * Clients using this driver must have vclient names of the form
 * "pipe%d", where %d is the pipe number, which must be
 * unique and less than MAX_PIPES.
 */

/* #define DEBUG 1 */
/* #define VERBOSE_DEBUG 1 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/of.h>
#include <asm/uaccess.h>
#include <asm-generic/okl4_virq.h>

#include <microvisor/microvisor.h>
#if defined(CONFIG_OKL4_VIRTUALISATION)
#include <asm/okl4-microvisor/okl4tags.h>
#include <asm/okl4-microvisor/microvisor_bus.h>
#include <asm/okl4-microvisor/virq.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
#define __devinit
#define __devexit
#define __devexit_p(x) x
#endif

#define DRIVER_NAME "okl4-pipe"
#define DEVICE_NAME "okl4-pipe"

#ifndef CONFIG_OF
#error "okl4-pipe driver only supported on device tree kernels"
#endif

#define MAX_PIPES 8

#ifdef CONFIG_OKL4_INTERLEAVED_PRIORITIES
extern int vcpu_prio_normal;
#endif

static int okl4_pipe_major;
static struct class *okl4_pipe_class;

/* This can be extended if required */
struct okl4_pipe_mv {
	int pipe_id;
};

struct okl4_pipe {
	struct okl4_pipe_data_buffer *write_buf;
	okl4_kcap_t pipe_tx_kcap;
	okl4_kcap_t pipe_rx_kcap;
	int tx_irq;
	int rx_irq;
	size_t max_msg_size;
	int ref_count;
	struct mutex pipe_mutex;
	spinlock_t pipe_lock;

	struct platform_device *pdev;
	struct cdev cdev;

	bool reset;
	bool tx_maybe_avail;
	bool rx_maybe_avail;

	wait_queue_head_t rx_wait_q;
	wait_queue_head_t tx_wait_q;
	wait_queue_head_t poll_wait_q;

	char *rx_buf;
	size_t rx_buf_count;
};
static struct okl4_pipe pipes[MAX_PIPES];

static okl4_error_t
okl4_pipe_control(okl4_kcap_t kcap, uint8_t control)
{
	okl4_pipe_control_t x = 0;

	okl4_pipe_control_setdoop(&x, true);
	okl4_pipe_control_setoperation(&x, control);
	return _okl4_sys_pipe_control(kcap, x);
}

static irqreturn_t
okl4_pipe_tx_irq(int irq, void *dev)
{
	struct okl4_pipe *pipe = dev;
	okl4_pipe_state_t payload = okl4_get_virq_payload(irq);

	spin_lock(&pipe->pipe_lock);
	if (okl4_pipe_state_gettxavailable(&payload))
		pipe->tx_maybe_avail = true;
	if (okl4_pipe_state_getreset(&payload)) {
		pipe->reset = true;
		pipe->tx_maybe_avail = true;
	}
	spin_unlock(&pipe->pipe_lock);

	wake_up_interruptible(&pipe->tx_wait_q);
	wake_up_interruptible(&pipe->poll_wait_q);

	return IRQ_HANDLED;
}

static irqreturn_t
okl4_pipe_rx_irq(int irq, void *dev)
{
	struct okl4_pipe *pipe = dev;
	okl4_pipe_state_t payload = okl4_get_virq_payload(irq);

	spin_lock(&pipe->pipe_lock);
	if (okl4_pipe_state_getrxavailable(&payload))
		pipe->rx_maybe_avail = true;
	if (okl4_pipe_state_getreset(&payload)) {
		pipe->reset = true;
		pipe->rx_maybe_avail = true;
	}
	spin_unlock(&pipe->pipe_lock);

	wake_up_interruptible(&pipe->rx_wait_q);
	wake_up_interruptible(&pipe->poll_wait_q);

	return IRQ_HANDLED;
}

static ssize_t
okl4_pipe_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct okl4_pipe_mv *priv = filp->private_data;
	int pipe_id = priv->pipe_id;
	struct okl4_pipe *pipe = &pipes[pipe_id];
	struct _okl4_sys_pipe_recv_return recv_return;
	uint32_t *buffer = NULL;
	size_t recv = 0;

	if (!count)
		return 0;

again:
	if (pipe->reset)
		return -EPIPE;

	if (!pipe->rx_maybe_avail && (filp->f_flags & O_NONBLOCK))
		return -EAGAIN;

	if (wait_event_interruptible(pipe->rx_wait_q, pipe->rx_maybe_avail))
		return -ERESTARTSYS;

	if (mutex_lock_interruptible(&pipe->pipe_mutex))
		return -ERESTARTSYS;

	/* Receive buffered data first */
	if (pipe->rx_buf_count) {
		recv = min(pipe->rx_buf_count, count);

		if (copy_to_user(buf, pipe->rx_buf, recv)) {
			mutex_unlock(&pipe->pipe_mutex);
			return -EFAULT;
		}

		pipe->rx_buf_count -= recv;

		if (pipe->rx_buf_count) {
			memmove(pipe->rx_buf, pipe->rx_buf + recv,
				pipe->max_msg_size - recv);
		}

		buf += recv;
		count -= recv;
		if (!count) {
			mutex_unlock(&pipe->pipe_mutex);
			return recv;
		}
	}

	buffer = kmalloc(pipe->max_msg_size + sizeof(uint32_t), GFP_KERNEL);

	if (!buffer) {
		mutex_unlock(&pipe->pipe_mutex);
		return -ENOMEM;
	}

	while (count) {
		okl4_error_t ret;
		size_t size;

		spin_lock_irq(&pipe->pipe_lock);
		recv_return = _okl4_sys_pipe_recv(pipe->pipe_rx_kcap,
				pipe->max_msg_size + sizeof(uint32_t),
				(void *)buffer);
		ret = recv_return.error;

		if (ret == OKL4_ERROR_PIPE_NOT_READY ||
				ret == OKL4_ERROR_PIPE_EMPTY) {
			pipe->rx_maybe_avail = false;
			if (!recv) {
				if (!(filp->f_flags & O_NONBLOCK)) {
					spin_unlock_irq(&pipe->pipe_lock);
					mutex_unlock(&pipe->pipe_mutex);
					kfree(buffer);
					goto again;
				}
				recv = -EAGAIN;
			}
			goto error;
		} else if (ret != OKL4_OK) {
			dev_err(&pipe->pdev->dev,
					"pipe send returned error %d in okl4_pipe driver!\n",
					(int)ret);
			if (!recv)
				recv = -ENXIO;
			goto error;
		}

		spin_unlock_irq(&pipe->pipe_lock);

		size = buffer[0];
		if (size > pipe->max_msg_size) {
			/* pipe error */
			if (!recv)
				recv = -EPROTO;
			goto out;
		}

		/* Save extra received data */
		if (size > count) {
			pipe->rx_buf_count = size - count;
			memcpy(pipe->rx_buf, (char*)&buffer[1] + count,
					size - count);
			size = count;
		}

		if (copy_to_user(buf, &buffer[1], size)) {
			if (!recv)
				recv = -EFAULT;
			goto out;
		}


		count -= size;
		buf += size;
		recv += size;
	}
out:
	mutex_unlock(&pipe->pipe_mutex);

	kfree(buffer);
	return recv;
error:
	spin_unlock_irq(&pipe->pipe_lock);
	goto out;
}

static ssize_t
okl4_pipe_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct okl4_pipe_mv *priv = filp->private_data;
	int pipe_id = priv->pipe_id;
	struct okl4_pipe *pipe = &pipes[pipe_id];
	uint32_t *buffer = NULL;
	size_t sent = 0;

	if (!count)
		return 0;

again:
	if (pipe->reset)
		return -EPIPE;

	if (!pipe->tx_maybe_avail && (filp->f_flags & O_NONBLOCK))
		return -EAGAIN;

	if (wait_event_interruptible(pipe->tx_wait_q, pipe->tx_maybe_avail))
		return -ERESTARTSYS;

	if (mutex_lock_interruptible(&pipe->pipe_mutex))
		return -ERESTARTSYS;

	buffer = kmalloc(pipe->max_msg_size + sizeof(uint32_t), GFP_KERNEL);

	if (!buffer) {
		mutex_unlock(&pipe->pipe_mutex);
		return -ENOMEM;
	}

	while (count) {
		okl4_error_t ret;
		size_t size = min(count, pipe->max_msg_size);
		size_t pipe_size = roundup(size + sizeof(uint32_t),
				sizeof(uint32_t));

		if (copy_from_user(&buffer[1], buf, size)) {
			if (!sent)
				sent = -EFAULT;
			break;
		}

		buffer[0] = size;

		spin_lock_irq(&pipe->pipe_lock);
		ret = _okl4_sys_pipe_send(pipe->pipe_tx_kcap, pipe_size,
				(void *)buffer);
		if (ret == OKL4_ERROR_PIPE_NOT_READY ||
				ret == OKL4_ERROR_PIPE_FULL) {
			pipe->tx_maybe_avail = false;
			spin_unlock_irq(&pipe->pipe_lock);
			if (!sent) {
				if (filp->f_flags & O_NONBLOCK) {
					sent = -EAGAIN;
					break;
				}
				mutex_unlock(&pipe->pipe_mutex);
				kfree(buffer);
				goto again;
			}
			break;
		} else if (ret != OKL4_OK) {
			dev_err(&pipe->pdev->dev,
					"pipe send returned error %d in okl4_pipe driver!\n",
					(int)ret);
			if (!sent)
				sent = -ENXIO;
			spin_unlock_irq(&pipe->pipe_lock);
			break;
		}
		spin_unlock_irq(&pipe->pipe_lock);

		count -= size;
		buf += size;
		sent += size;
	}
	mutex_unlock(&pipe->pipe_mutex);

	kfree(buffer);
	return sent;
}


static unsigned int
okl4_pipe_poll(struct file *filp, struct poll_table_struct *poll_table)
{
	struct okl4_pipe_mv *priv = filp->private_data;
	int pipe_id = priv->pipe_id;
	struct okl4_pipe *pipe = &pipes[pipe_id];
	unsigned int ret = 0;

	poll_wait(filp, &pipe->poll_wait_q, poll_table);

	spin_lock_irq(&pipe->pipe_lock);

	if (pipe->rx_maybe_avail)
		ret |= POLLIN | POLLRDNORM;
	if (pipe->tx_maybe_avail)
		ret |= POLLOUT | POLLWRNORM;
	if (pipe->reset)
		ret = POLLHUP;

	spin_unlock_irq(&pipe->pipe_lock);

	return ret;
}

static int
okl4_pipe_open(struct inode *inode, struct file *filp)
{
	struct okl4_pipe *pipe = container_of(inode->i_cdev,
			struct okl4_pipe, cdev);
	struct okl4_pipe_mv *priv = dev_get_drvdata(&pipe->pdev->dev);

	filp->private_data = priv;
	if (!pipe->ref_count) {
		pipe->rx_buf = kmalloc(pipe->max_msg_size, GFP_KERNEL);
		if (!pipe->rx_buf)
			return -ENOMEM;

		mutex_init(&pipe->pipe_mutex);
		spin_lock_init(&pipe->pipe_lock);

		pipe->rx_buf_count = 0;
		pipe->reset = false;
		pipe->tx_maybe_avail = true;
		pipe->rx_maybe_avail = true;

		okl4_pipe_control(pipe->pipe_tx_kcap,
				OKL4_PIPE_CONTROL_OP_SET_TX_READY);
		okl4_pipe_control(pipe->pipe_rx_kcap,
				OKL4_PIPE_CONTROL_OP_SET_RX_READY);
	}
	pipe->ref_count++;
	return 0;
}

static int
okl4_pipe_close(struct inode *inode, struct file *filp)
{
	struct okl4_pipe *pipe = container_of(inode->i_cdev,
			struct okl4_pipe, cdev);

	pipe->ref_count--;
	if (!pipe->ref_count) {
		okl4_pipe_control(pipe->pipe_rx_kcap,
				OKL4_PIPE_CONTROL_OP_RESET);
		okl4_pipe_control(pipe->pipe_tx_kcap,
				OKL4_PIPE_CONTROL_OP_RESET);

		kfree(pipe->rx_buf);
		pipe->rx_buf = NULL;
		pipe->rx_buf_count = 0;
	}

	return 0;
}

struct file_operations okl4_pipe_fops = {
	.owner =	THIS_MODULE,
	.read =		okl4_pipe_read,
	.write =	okl4_pipe_write,
	.open =		okl4_pipe_open,
	.release =	okl4_pipe_close,
	.poll =		okl4_pipe_poll,
};

static int __devinit
okl4_pipe_probe(struct platform_device *pdev)
{
	struct okl4_pipe *pipe;
	int err, pipe_id;
	struct okl4_pipe_mv *priv;
	dev_t dev_num;
	struct device *device = NULL;
	u32 reg[2];
	struct resource *irq;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct okl4_pipe_mv),
			GFP_KERNEL);
	if (priv == NULL) {
		err = -ENOMEM;
		goto fail_alloc_priv;
	}

	dev_set_drvdata(&pdev->dev, priv);

	pipe_id = of_alias_get_id(pdev->dev.of_node, "pipe");
	if (pipe_id < 0) {
		err = -ENXIO;
		goto fail_pipe_id;
	}

	if (pipe_id < 0 || pipe_id >= MAX_PIPES) {
		err = -ENXIO;
		goto fail_pipe_id;
	}

	if (of_property_read_u32_array(pdev->dev.of_node, "reg", reg, 2)) {
		dev_err(&pdev->dev, "need 2 reg resources\n");
		err = -ENODEV;
		goto fail_pipe_id;
	}

	/* Populate the private structure */
	priv->pipe_id = pipe_id;

	pipe = &pipes[pipe_id];

	/* Set up and register the pipe device */
	pipe->pdev = pdev;
	dev_set_name(&pdev->dev, "%s%d", DEVICE_NAME, (int)pipe_id);

	pipe->ref_count = 0;
	pipe->pipe_tx_kcap = reg[0];
	pipe->pipe_rx_kcap = reg[1];
	pipe->max_msg_size = 64;

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq) {
		dev_err(&pdev->dev, "no tx irq resource?\n");
		err = -ENODEV;
		goto fail_irq_resource;
	}
	pipe->tx_irq = irq->start;
	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (!irq) {
		dev_err(&pdev->dev, "no rx irq resource?\n");
		err = -ENODEV;
		goto fail_irq_resource;
	}
	pipe->rx_irq = irq->start;

	pipe->write_buf = kmalloc(sizeof(struct okl4_pipe_data_buffer *),
                                                        GFP_KERNEL);
	if (!pipe->write_buf) {
		dev_err(&pdev->dev, "cannot allocate write buffer\n");
		err = -ENOMEM;
		goto fail_malloc_write;
	}

	init_waitqueue_head(&pipe->rx_wait_q);
	init_waitqueue_head(&pipe->tx_wait_q);
	init_waitqueue_head(&pipe->poll_wait_q);

	err = devm_request_irq(&pdev->dev, pipe->rx_irq,
			okl4_pipe_rx_irq, 0, dev_name(&pdev->dev),
			pipe);
	if (err) {
		dev_err(&pdev->dev, "cannot register rx irq %d: %d\n",
				(int)pipe->rx_irq, (int)err);
		goto fail_request_rx_irq;
	}

	err = devm_request_irq(&pdev->dev, pipe->tx_irq,
			okl4_pipe_tx_irq, 0, dev_name(&pdev->dev),
			pipe);
	if (err) {
		dev_err(&pdev->dev, "cannot register tx irq %d: %d\n",
				(int)pipe->tx_irq, (int)err);
		goto fail_request_tx_irq;
	}

	dev_num = MKDEV(okl4_pipe_major, pipe_id);

	cdev_init(&pipe->cdev, &okl4_pipe_fops);
	pipe->cdev.owner = THIS_MODULE;
	err = cdev_add(&pipe->cdev, dev_num, 1);
	if (err) {
		dev_err(&pdev->dev, "cannot add device: %d\n", (int)err);
		goto fail_cdev_add;
	}

	device = device_create(okl4_pipe_class, NULL, dev_num, NULL,
			DEVICE_NAME "%d", pipe_id);
	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		dev_err(&pdev->dev, "cannot create device: %d\n", (int)err);
		goto fail_device_create;
	}

	return 0;

fail_device_create:
	cdev_del(&pipe->cdev);
fail_cdev_add:
	devm_free_irq(&pdev->dev, pipe->tx_irq, pipe);
fail_request_tx_irq:
	devm_free_irq(&pdev->dev, pipe->rx_irq, pipe);
fail_request_rx_irq:
	kfree(pipe->write_buf);
fail_malloc_write:
fail_irq_resource:
fail_pipe_id:
	dev_set_drvdata(&pdev->dev, NULL);
	devm_kfree(&pdev->dev, priv);
fail_alloc_priv:
	return err;
}

static int __devexit
okl4_pipe_remove(struct platform_device *pdev)
{
	struct okl4_pipe *pipe;
	struct okl4_pipe_mv *priv = dev_get_drvdata(&pdev->dev);

	if (priv->pipe_id < 0 || priv->pipe_id >= MAX_PIPES)
		return -ENXIO;

	pipe = &pipes[priv->pipe_id];

	cdev_del(&pipe->cdev);

	devm_free_irq(&pdev->dev, pipe->tx_irq, pipe);
	devm_free_irq(&pdev->dev, pipe->rx_irq, pipe);

	kfree(pipe->write_buf);

	dev_set_drvdata(&pdev->dev, NULL);
	devm_kfree(&pdev->dev, priv);

	return 0;
}

static const struct of_device_id okl4_pipe_match[] = {
	{
		.compatible = "okl,pipe",
	},
	{},
};
MODULE_DEVICE_TABLE(of, okl4_pipe_match);

static struct platform_driver okl4_pipe_driver = {
	.probe		= okl4_pipe_probe,
	.remove		= __devexit_p(okl4_pipe_remove),
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = okl4_pipe_match,
	},
};

static int __init
okl4_pipe_init(void)
{
	int err;
	dev_t dev_num = 0;

	err = alloc_chrdev_region(&dev_num, 0, MAX_PIPES, DEVICE_NAME);
	if (err < 0) {
		printk("%s: cannot allocate device region\n", __func__);
		goto fail_alloc_chrdev_region;
	}
	okl4_pipe_major = MAJOR(dev_num);

	okl4_pipe_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(okl4_pipe_class)) {
		err = PTR_ERR(okl4_pipe_class);
		goto fail_class_create;
	}

	/* Register the driver with the microvisor bus */
	err = platform_driver_register(&okl4_pipe_driver);
	if (err)
		goto fail_driver_register;

	return 0;

fail_driver_register:
	class_destroy(okl4_pipe_class);
fail_class_create:
	unregister_chrdev_region(dev_num, MAX_PIPES);
fail_alloc_chrdev_region:
	return err;
}

static void __exit
okl4_pipe_exit(void)
{
	dev_t dev_num = MKDEV(okl4_pipe_major, 0);

	platform_driver_unregister(&okl4_pipe_driver);
	class_destroy(okl4_pipe_class);
	unregister_chrdev_region(dev_num, MAX_PIPES);
}

module_init(okl4_pipe_init);
module_exit(okl4_pipe_exit);

MODULE_DESCRIPTION("OKL4 pipe driver");
MODULE_AUTHOR("John Clarke <johnc@cog.systems>");

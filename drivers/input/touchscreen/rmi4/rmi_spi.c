/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 * Copyright (C) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kconfig.h>
#include <linux/module.h>
#include <linux/rmi.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include "rmi_driver.h"

#define RMI_PROTOCOL_VERSION_ADDRESS	0xa0fd
#define SPI_V2_UNIFIED_READ		0xc0
#define SPI_V2_WRITE			0x40
#define SPI_V2_PREPARE_SPLIT_READ	0xc8
#define SPI_V2_EXECUTE_SPLIT_READ	0xca

#define RMI_SPI_BLOCK_DELAY_US		65
#define RMI_SPI_BYTE_DELAY_US		65
#define RMI_SPI_WRITE_DELAY_US		0

#define RMI_V1_READ_FLAG		0x80

#define RMI_PAGE_SELECT_REGISTER 0x00FF
#define RMI_SPI_PAGE(addr) (((addr) >> 8) & 0x80)

static char *spi_v1_proto_name = "spi";
static char *spi_v2_proto_name = "spiv2";

#define BUFFER_SIZE_INCREMENT 32

#if NV_NOTIFY_OUT_OF_IDLE
#define RMI_IDLE_PERIOD (msecs_to_jiffies(50))
#endif

struct rmi_spi_data {
	struct mutex page_mutex;
	int page;
	int (*set_page) (struct rmi_phys_device *phys, u8 page);
	bool split_read_pending;
	int enabled;
	struct rmi_phys_device *phys;
	struct completion irq_comp;

	u8 *xfer_buf;
	int xfer_buf_size;

	bool comms_debug;
	bool ff_debug;
	u8 *debug_buf;
	int debug_buf_size;
#ifdef CONFIG_RMI4_DEBUG
	struct dentry *debugfs_comms;
	struct dentry *debugfs_ff;
#endif
#if NV_NOTIFY_OUT_OF_IDLE
	unsigned long last_irq_jiffies;
#endif
};

#ifdef CONFIG_RMI4_DEBUG


struct i2c_debugfs_data {
	bool done;
	struct rmi_spi_data *spi_data;
};

static int debug_open(struct inode *inodep, struct file *filp)
{
	struct i2c_debugfs_data *data;

	data = kzalloc(sizeof(struct i2c_debugfs_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->spi_data = inodep->i_private;
	filp->private_data = data;
	return 0;
}

static int debug_release(struct inode *inodep, struct file *filp)
{
	kfree(filp->private_data);
	return 0;
}

static ssize_t comms_debug_read(struct file *filp, char __user *buffer,
		size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	struct i2c_debugfs_data *dfs = filp->private_data;
	struct rmi_spi_data *data = dfs->spi_data;

	if (dfs->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	dfs->done = 1;

	retval = snprintf(local_buf, PAGE_SIZE, "%u\n", data->comms_debug);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static ssize_t comms_debug_write(struct file *filp, const char __user *buffer,
			   size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	unsigned int new_value;
	struct i2c_debugfs_data *dfs = filp->private_data;
	struct rmi_spi_data *data = dfs->spi_data;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}

	retval = sscanf(local_buf, "%u", &new_value);
	kfree(local_buf);
	if (retval != 1 || new_value > 1)
		return -EINVAL;

	data->comms_debug = new_value;

	return size;
}

static ssize_t ff_debug_read(struct file *filp, char __user *buffer,
		size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	struct i2c_debugfs_data *dfs = filp->private_data;
	struct rmi_spi_data *data = dfs->spi_data;

	if (dfs->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	dfs->done = 1;

	retval = snprintf(local_buf, PAGE_SIZE, "%u\n", data->ff_debug);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static ssize_t ff_debug_write(struct file *filp, const char __user *buffer,
			   size_t size, loff_t *offset) {
	int retval;
	char *local_buf;
	unsigned int new_value;
	struct i2c_debugfs_data *dfs = filp->private_data;
	struct rmi_spi_data *data = dfs->spi_data;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}

	retval = sscanf(local_buf, "%u", &new_value);
	kfree(local_buf);
	if (retval != 1 || new_value > 1)
		return -EINVAL;

	data->ff_debug = new_value;

	return size;
}

static const struct file_operations comms_debug_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_release,
	.read = comms_debug_read,
	.write = comms_debug_write,
};

static const struct file_operations ff_debug_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_release,
	.read = ff_debug_read,
	.write = ff_debug_write,
};

static int setup_debugfs(struct rmi_device *rmi_dev, struct rmi_spi_data *data)
{
	if (!rmi_dev->debugfs_root)
		return -ENODEV;

	data->debugfs_comms = debugfs_create_file("comms_debug", RMI_RW_ATTR,
			rmi_dev->debugfs_root, data, &comms_debug_fops);
	if (!data->debugfs_comms || IS_ERR(data->debugfs_comms)) {
		dev_warn(&rmi_dev->dev, "Failed to create debugfs comms_debug.\n");
		data->debugfs_comms = NULL;
	}

	data->debugfs_ff = debugfs_create_file("ff_debug", RMI_RW_ATTR,
			rmi_dev->debugfs_root, data, &ff_debug_fops);
	if (!data->debugfs_ff || IS_ERR(data->debugfs_ff)) {
		dev_warn(&rmi_dev->dev, "Failed to create debugfs ff_debug.\n");
		data->debugfs_ff = NULL;
	}


	return 0;
}

static void teardown_debugfs(struct rmi_spi_data *data)
{
	if (data->debugfs_comms)
		debugfs_remove(data->debugfs_comms);
	if (data->debugfs_ff)
		debugfs_remove(data->debugfs_ff);
}
#else
#define setup_debugfs(rmi_dev, data) 0
#define teardown_debugfs(data)
#endif

#if NV_NOTIFY_OUT_OF_IDLE
static void rmi_spi_out_of_idle(struct rmi_driver_data *rmi_data)
{
	struct rmi_function_dev *fn_dev;
	struct rmi_function_driver *fn_drv;

	/* For now we don't care about to which function dev this irq
	 * corresponds, call all registered out_of_idle handlers.
	 */
	list_for_each_entry(fn_dev, &rmi_data->rmi_functions.list, list) {
		if (fn_dev && fn_dev->dev.driver) {
			fn_drv = to_rmi_function_driver(fn_dev->dev.driver);
			if (fn_drv->out_of_idle)
				fn_drv->out_of_idle(fn_dev);
		}
	}
}
#endif

#define COMMS_DEBUG(data) (IS_ENABLED(CONFIG_RMI4_DEBUG) && data->comms_debug)
#define FF_DEBUG(data) (IS_ENABLED(CONFIG_RMI4_DEBUG) && data->ff_debug)
#define IRQ_DEBUG(data) (IS_ENABLED(CONFIG_RMI4_DEBUG) && data->irq_debug)

static irqreturn_t rmi_spi_hard_irq(int irq, void *p)
{
	struct rmi_phys_device *phys = p;
	struct rmi_spi_data *data = phys->data;
	struct rmi_device *rmi_dev = phys->rmi_dev;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;
	struct rmi_driver_data *rmi_data;

	rmi_data = dev_get_drvdata(&rmi_dev->dev);

	if (IRQ_DEBUG(rmi_data))
		dev_dbg(phys->dev, "ATTN gpio, value: %d.\n",
			gpio_get_value(pdata->attn_gpio));

	if (data->split_read_pending &&
		      gpio_get_value(pdata->attn_gpio) ==
		      pdata->attn_polarity) {
		complete(&data->irq_comp);
		return IRQ_HANDLED;
	}

#if NV_NOTIFY_OUT_OF_IDLE
	if (time_after(jiffies, data->last_irq_jiffies + RMI_IDLE_PERIOD))
		rmi_spi_out_of_idle(rmi_data);
	data->last_irq_jiffies = jiffies;
#endif

	return IRQ_WAKE_THREAD;
}

static int copy_to_debug_buf(struct device *dev, struct rmi_spi_data *data,
			     const u8 *buf, int len) {
	int i;
	int n = 0;
	char *temp;
	int dbg_size = 3 * len + 1;

	if (!data->debug_buf || data->debug_buf_size < dbg_size) {
		if (data->debug_buf)
			devm_kfree(dev, data->debug_buf);
		data->debug_buf_size = dbg_size + BUFFER_SIZE_INCREMENT;
		data->debug_buf = devm_kzalloc(dev, data->debug_buf_size,
					       GFP_KERNEL);
		if (!data->debug_buf) {
			data->debug_buf_size = 0;
			return -ENOMEM;
		}
	}
	temp = data->debug_buf;

	for (i = 0; i < len; i++) {
		n = sprintf(temp, " %02x", buf[i]);
		temp += n;
	}

	return 0;
}

static int rmi_spi_xfer(struct rmi_phys_device *phys,
		    const u8 *txbuf, const unsigned n_tx,
		    u8 *rxbuf, const unsigned n_rx)
{
	struct spi_device *client = to_spi_device(phys->dev);
	struct rmi_spi_data *data = phys->data;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;
	int status;
	struct spi_message message;
	struct spi_transfer *xfers;
	u8 *local_buf;
	int total_bytes = n_tx + n_rx;
	int xfer_count = 0;
	int xfer_index = 0;
	int block_delay = n_rx > 0 ? pdata->spi_data.block_delay_us : 0;
	int byte_delay = n_rx > 1 ? pdata->spi_data.read_delay_us : 0;
	int write_delay = n_tx > 1 ? pdata->spi_data.write_delay_us : 0;

	if (data->split_read_pending) {
		block_delay =
		    n_rx > 0 ? pdata->spi_data.split_read_block_delay_us : 0;
		byte_delay =
		    n_rx > 1 ? pdata->spi_data.split_read_byte_delay_us : 0;
		write_delay = 0;
	}

	if (n_tx) {
		phys->info.tx_count++;
		phys->info.tx_bytes += n_tx;
		if (write_delay)
			xfer_count += n_tx;
		else
			xfer_count += 1;
	}

	if (n_rx) {
		phys->info.rx_count++;
		phys->info.rx_bytes += n_rx;
		if (byte_delay)
			xfer_count += n_rx;
		else
			xfer_count += 1;
	}

	xfers = kcalloc(xfer_count,
			    sizeof(struct spi_transfer), GFP_KERNEL);
	if (!xfers)
		return -ENOMEM;

	if (!data->xfer_buf || data->xfer_buf_size < total_bytes) {
		if (data->xfer_buf)
			devm_kfree(&client->dev, data->xfer_buf);
		data->xfer_buf = devm_kzalloc(&client->dev,
			total_bytes + BUFFER_SIZE_INCREMENT, GFP_KERNEL);
		if (!data->xfer_buf) {
			data->xfer_buf_size = 0;
			status = -ENOMEM;
			goto error_exit;
		}
	}
	local_buf = data->xfer_buf;

	spi_message_init(&message);

	if (n_tx) {
		if (write_delay) {
			for (xfer_index = 0; xfer_index < n_tx;
					xfer_index++) {
				memset(&xfers[xfer_index], 0,
				       sizeof(struct spi_transfer));
				xfers[xfer_index].len = 1;
				xfers[xfer_index].delay_usecs = write_delay;
				xfers[xfer_index].cs_change = 1;
				xfers[xfer_index].tx_buf = txbuf + xfer_index;
				spi_message_add_tail(&xfers[xfer_index],
						     &message);
			}
		} else {
			memset(&xfers[0], 0, sizeof(struct spi_transfer));
			xfers[0].len = n_tx;
			spi_message_add_tail(&xfers[0], &message);
			memcpy(local_buf, txbuf, n_tx);
			xfers[0].tx_buf = local_buf;
			xfer_index++;
		}
		if (block_delay) {
			xfers[xfer_index-1].delay_usecs = block_delay;
			xfers[xfer_index].cs_change = 1;
		}
	}
	if (n_rx) {
		if (byte_delay) {
			int buffer_offset = n_tx;
			for (; xfer_index < xfer_count; xfer_index++) {
				memset(&xfers[xfer_index], 0,
				       sizeof(struct spi_transfer));
				xfers[xfer_index].len = 1;
				xfers[xfer_index].delay_usecs = byte_delay;
				xfers[xfer_index].cs_change = 1;
				xfers[xfer_index].rx_buf =
				    local_buf + buffer_offset;
				buffer_offset++;
				spi_message_add_tail(&xfers[xfer_index],
						     &message);
			}
		} else {
			memset(&xfers[xfer_index], 0,
			       sizeof(struct spi_transfer));
			xfers[xfer_index].len = n_rx;
			xfers[xfer_index].rx_buf = local_buf + n_tx;
			spi_message_add_tail(&xfers[xfer_index], &message);
			xfer_index++;
		}
	}

	if (COMMS_DEBUG(data) && n_tx &&
			!copy_to_debug_buf(&client->dev, data, txbuf, n_tx))
		dev_dbg(&client->dev, "sends %d bytes:%s\n",
			n_tx, data->debug_buf);

	/* do the i/o */
	if (pdata->spi_data.cs_assert) {
		status = pdata->spi_data.cs_assert(
			pdata->spi_data.cs_assert_data, true);
		if (status) {
			dev_err(phys->dev, "Failed to assert CS, code %d.\n",
				status);
			/* nonzero means error */
			status = -1;
			goto error_exit;
		} else
			status = 0;
	}

	if (pdata->spi_data.pre_delay_us)
		udelay(pdata->spi_data.pre_delay_us);

	status = spi_sync(client, &message);

	if (pdata->spi_data.post_delay_us)
		udelay(pdata->spi_data.post_delay_us);

	if (pdata->spi_data.cs_assert) {
		status = pdata->spi_data.cs_assert(
			pdata->spi_data.cs_assert_data, false);
		if (status) {
			dev_err(phys->dev, "Failed to deassert CS. code %d.\n",
				status);
			/* nonzero means error */
			status = -1;
			goto error_exit;
		} else
			status = 0;
	}

	if (status == 0) {
		memcpy(rxbuf, local_buf + n_tx, n_rx);
		status = message.status;
	} else {
		if (n_tx)
		phys->info.tx_errs++;
		if (n_rx)
		phys->info.rx_errs++;
		dev_err(phys->dev, "spi_sync failed with error code %d.",
		       status);
		goto error_exit;
	}

	if (COMMS_DEBUG(data) && n_rx &&
			!copy_to_debug_buf(&client->dev, data, rxbuf, n_rx))
		dev_dbg(&client->dev, "received %d bytes:%s\n",
			n_rx, data->debug_buf);

	if (FF_DEBUG(data) && n_rx) {
		bool bad_data = true;
		int i;
		for (i = 0; i < n_rx; i++) {
			if (rxbuf[i] != 0xFF) {
				bad_data = false;
				break;
			}
		}
		if (bad_data) {
			phys->info.rx_errs++;
			dev_err(phys->dev, "BAD READ %lu out of %lu.\n",
				phys->info.rx_errs, phys->info.rx_count);
		}
	}

error_exit:
	kfree(xfers);
	return status;
}

static int rmi_spi_v2_write_block(struct rmi_phys_device *phys, u16 addr,
				  const void *buf, const int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[len + 4];
	int error;

	txbuf[0] = SPI_V2_WRITE;
	txbuf[1] = (addr >> 8) & 0x00FF;
	txbuf[2] = addr & 0x00FF;
	txbuf[3] = len;

	memcpy(&txbuf[4], buf, len);

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	error = rmi_spi_xfer(phys, buf, len + 4, NULL, 0);
	if (error < 0)
		goto exit;
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}


static int rmi_spi_v1_write_block(struct rmi_phys_device *phys, u16 addr,
				  const void *buf, const int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[len + 2];
	int error;

	txbuf[0] = (addr >> 8) & ~RMI_V1_READ_FLAG;
	txbuf[1] = addr;
	memcpy(txbuf+2, buf, len);

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	error = rmi_spi_xfer(phys, txbuf, len + 2, NULL, 0);
	if (error < 0)
		goto exit;
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}


static int rmi_spi_v2_split_read_block(struct rmi_phys_device *phys, u16 addr,
				       void *buf, const int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[4];
	u8 rxbuf[len + 1]; /* one extra byte for read length */
	int error;

	txbuf[0] = SPI_V2_PREPARE_SPLIT_READ;
	txbuf[1] = (addr >> 8) & 0x00FF;
	txbuf[2] = addr & 0x00ff;
	txbuf[3] = len;

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	data->split_read_pending = true;

	error = rmi_spi_xfer(phys, txbuf, 4, NULL, 0);
	if (error < 0) {
		data->split_read_pending = false;
		goto exit;
	}

	wait_for_completion(&data->irq_comp);

	txbuf[0] = SPI_V2_EXECUTE_SPLIT_READ;
	txbuf[1] = 0;

	error = rmi_spi_xfer(phys, txbuf, 2, rxbuf, len + 1);
	data->split_read_pending = false;
	if (error < 0)
		goto exit;

	/* first byte is length */
	if (rxbuf[0] != len) {
		error = -EIO;
		goto exit;
	}

	memcpy(buf, rxbuf + 1, len);
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}

static int rmi_spi_v2_read_block(struct rmi_phys_device *phys, u16 addr,
				 void *buf, const int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[4];
	int error;

	txbuf[0] = SPI_V2_UNIFIED_READ;
	txbuf[1] = (addr >> 8) & 0x00FF;
	txbuf[2] = addr & 0x00ff;
	txbuf[3] = len;

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	error = rmi_spi_xfer(phys, txbuf, 4, (u8 *) buf, len);
	if (error < 0)
		goto exit;
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}


static int rmi_spi_v1_read_block(struct rmi_phys_device *phys, u16 addr,
				 void *buf, const int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[2];
	int error;

	txbuf[0] = (addr >> 8) | RMI_V1_READ_FLAG;
	txbuf[1] = addr;

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	error = rmi_spi_xfer(phys, txbuf, 2, (u8 *) buf, len);
	if (error < 0)
		goto exit;
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}

#define RMI_SPI_PAGE_SELECT_WRITE_LENGTH 1

static int rmi_spi_v1_set_page(struct rmi_phys_device *phys, u8 page)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[] = {RMI_PAGE_SELECT_REGISTER >> 8,
		RMI_PAGE_SELECT_REGISTER & 0xFF, page};
	int error;

	error = rmi_spi_xfer(phys, txbuf, sizeof(txbuf), NULL, 0);
	if (error < 0) {
		dev_err(phys->dev, "Failed to set page select, code: %d.\n",
			error);
		return error;
	}

	data->page = page;

	return RMI_SPI_PAGE_SELECT_WRITE_LENGTH;
}

static int rmi_spi_v2_set_page(struct rmi_phys_device *phys, u8 page)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[] = {SPI_V2_WRITE, RMI_PAGE_SELECT_REGISTER >> 8,
		RMI_PAGE_SELECT_REGISTER & 0xFF,
		RMI_SPI_PAGE_SELECT_WRITE_LENGTH, page};
	int error;

	error = rmi_spi_xfer(phys, txbuf, sizeof(txbuf), NULL, 0);
	if (error < 0) {
		dev_err(phys->dev, "Failed to set page select, code: %d.\n",
			error);
		return error;
	}

	data->page = page;

	return RMI_SPI_PAGE_SELECT_WRITE_LENGTH;
}

#define DUMMY_READ_SLEEP_US 10

static int rmi_spi_check_device(struct rmi_phys_device *rmi_phys)
{
	u8 buf[6];
	int error;
	int i;

	/* Some SPI subsystems return 0 for the very first read you do.  So
	 * we use this dummy read to get that out of the way.
	 */
	error = rmi_spi_v1_read_block(rmi_phys, PDT_START_SCAN_LOCATION,
				      buf, sizeof(buf));
	if (error < 0) {
		dev_err(rmi_phys->dev, "dummy read failed with %d.\n", error);
		return error;
	}
	udelay(DUMMY_READ_SLEEP_US);

	/* Force page select to 0.
	 */
	error = rmi_spi_v1_set_page(rmi_phys, 0x00);
	if (error < 0)
		return error;

	/* Now read the first PDT entry.  We know where this is, and if the
	 * RMI4 device is out there, these 6 bytes will be something other
	 * than all 0x00 or 0xFF.  We need to check for 0x00 and 0xFF,
	 * because many (maybe all) SPI implementations will return all 0x00
	 * or all 0xFF on read if the device is not connected.
	 */
	error = rmi_spi_v1_read_block(rmi_phys, PDT_START_SCAN_LOCATION,
				      buf, sizeof(buf));
	if (error < 0) {
		dev_err(rmi_phys->dev, "probe read failed with %d.\n", error);
		return error;
	}
	for (i = 0; i < sizeof(buf); i++) {
		if (buf[i] != 0x00 && buf[i] != 0xFF)
			return error;
	}

	dev_err(rmi_phys->dev, "probe read returned invalid block.\n");
	return -ENODEV;
}


static int rmi_spi_probe(struct spi_device *spi)
{
	struct rmi_phys_device *rmi_phys;
	struct rmi_spi_data *data;
	struct rmi_device_platform_data *pdata = spi->dev.platform_data;
	u8 buf[2];
	int retval;

	if (!pdata) {
		dev_err(&spi->dev, "no platform data\n");
		return -EINVAL;
	}

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX)
		return -EINVAL;

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;
	retval = spi_setup(spi);
	if (retval < 0) {
		dev_err(&spi->dev, "spi_setup failed!\n");
		return retval;
	}

	if (pdata->gpio_config) {
		retval = pdata->gpio_config(pdata->gpio_data, true);
		if (retval < 0) {
			dev_err(&spi->dev, "Failed to setup GPIOs, code: %d.\n",
				retval);
			return retval;
		}
	}

	rmi_phys = devm_kzalloc(&spi->dev, sizeof(struct rmi_phys_device),
			GFP_KERNEL);
	if (!rmi_phys)
		return -ENOMEM;

	data = devm_kzalloc(&spi->dev, sizeof(struct rmi_spi_data),
			GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->enabled = true;	/* We plan to come up enabled. */
	data->phys = rmi_phys;

#if NV_NOTIFY_OUT_OF_IDLE
	data->last_irq_jiffies = jiffies;
#endif

	rmi_phys->data = data;
	rmi_phys->dev = &spi->dev;

	rmi_phys->write_block = rmi_spi_v1_write_block;
	rmi_phys->read_block = rmi_spi_v1_read_block;
	rmi_phys->hard_irq = rmi_spi_hard_irq;
	data->set_page = rmi_spi_v1_set_page;

	rmi_phys->info.proto = spi_v1_proto_name;

	mutex_init(&data->page_mutex);

	dev_set_drvdata(&spi->dev, rmi_phys);

	pdata->spi_data.block_delay_us = pdata->spi_data.block_delay_us ?
			pdata->spi_data.block_delay_us : RMI_SPI_BLOCK_DELAY_US;
	pdata->spi_data.read_delay_us = pdata->spi_data.read_delay_us ?
			pdata->spi_data.read_delay_us : RMI_SPI_BYTE_DELAY_US;
	pdata->spi_data.write_delay_us = pdata->spi_data.write_delay_us ?
			pdata->spi_data.write_delay_us : RMI_SPI_BYTE_DELAY_US;
	pdata->spi_data.split_read_block_delay_us =
			pdata->spi_data.split_read_block_delay_us ?
			pdata->spi_data.split_read_block_delay_us :
			RMI_SPI_BLOCK_DELAY_US;
	pdata->spi_data.split_read_byte_delay_us =
			pdata->spi_data.split_read_byte_delay_us ?
			pdata->spi_data.split_read_byte_delay_us :
			RMI_SPI_BYTE_DELAY_US;

	retval = rmi_spi_check_device(rmi_phys);
	if (retval < 0)
		goto err_gpio;

	/* check if this is an SPI v2 device */
	retval = rmi_spi_v1_read_block(rmi_phys, RMI_PROTOCOL_VERSION_ADDRESS,
				      buf, 2);
	if (retval < 0) {
		dev_err(&spi->dev, "failed to get SPI version number!\n");
		goto err_gpio;
	}
	dev_dbg(&spi->dev, "SPI version is %d", buf[0]);

	if (buf[0] == 1) {
		/* SPIv2 */
		rmi_phys->write_block	= rmi_spi_v2_write_block;
		data->set_page		= rmi_spi_v2_set_page;

		rmi_phys->info.proto = spi_v2_proto_name;

		if (pdata->attn_gpio > 0) {
			init_completion(&data->irq_comp);
			rmi_phys->read_block = rmi_spi_v2_split_read_block;
		} else {
			dev_warn(&spi->dev, "WARNING: SPI V2 detected, but no attention GPIO was specified. This is unlikely to work well.\n");
			rmi_phys->read_block = rmi_spi_v2_read_block;
		}
	} else if (buf[0] != 0) {
		dev_err(&spi->dev, "Unrecognized SPI version %d.\n", buf[0]);
		retval = -ENODEV;
		goto err_gpio;
	}

	retval = rmi_register_phys_device(rmi_phys);
	if (retval) {
		dev_err(&spi->dev, "failed to register physical driver\n");
		goto err_gpio;
	}

	if (IS_ENABLED(CONFIG_RMI4_DEBUG))
		retval = setup_debugfs(rmi_phys->rmi_dev, data);

	dev_info(&spi->dev, "registered RMI SPI driver\n");
	return 0;

err_gpio:
	if (pdata->gpio_config)
		pdata->gpio_config(pdata->gpio_data, false);
	return retval;
}

static int rmi_spi_remove(struct spi_device *spi)
{
	struct rmi_phys_device *phys = dev_get_drvdata(&spi->dev);
	struct rmi_device_platform_data *pd = spi->dev.platform_data;

	if (IS_ENABLED(CONFIG_RMI4_DEBUG))
		teardown_debugfs(phys->data);

	/* Can I remove this disable_device */
	/* disable_device(phys); */
	rmi_unregister_phys_device(phys);

	if (pd->gpio_config)
		pd->gpio_config(pd->gpio_data, false);

	return 0;
}

static const struct spi_device_id rmi_id[] = {
	{ "rmi_spi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, rmi_id);

static struct spi_driver rmi_spi_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "rmi_spi",
	},
	.id_table	= rmi_id,
	.probe		= rmi_spi_probe,
	.remove		= rmi_spi_remove,
};

static int __init rmi_spi_init(void)
{
	return spi_register_driver(&rmi_spi_driver);
}

static void __exit rmi_spi_exit(void)
{
	spi_unregister_driver(&rmi_spi_driver);
}

module_init(rmi_spi_init);
module_exit(rmi_spi_exit);

MODULE_AUTHOR("Christopher Heiny <cheiny@synaptics.com>");
MODULE_DESCRIPTION("RMI SPI driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RMI_DRIVER_VERSION);

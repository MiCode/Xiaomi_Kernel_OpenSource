/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This driver provides the core support for a single RMI4-based device.
 *
 * The RMI4 specification can be found here (URL split after files/ for
 * style reasons):
 * http://www.synaptics.com/sites/default/files/
 *           511-000136-01-Rev-E-RMI4%20Intrfacing%20Guide.pdf
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kconfig.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/rmi.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "rmi_driver.h"
#include "rmi_f01.h"

#define CREATE_TRACE_POINTS
#include <trace/events/touchscreen_synaptics.h>

#define CONFIG_RMI4_DEBUG 1

#define HAS_NONSTANDARD_PDT_MASK 0x40
#define RMI4_MAX_PAGE 0xff
#define RMI4_PAGE_SIZE 0x100

#define RMI_DEVICE_RESET_CMD	0x01
#define DEFAULT_RESET_DELAY_MS	100

#define DEFAULT_POLL_INTERVAL_MS	13

#define IRQ_DEBUG(data) (IS_ENABLED(CONFIG_RMI4_DEBUG) && data->irq_debug)

#ifdef	CONFIG_RMI4_DEBUG
struct driver_debugfs_data {
	bool done;
	struct rmi_device *rmi_dev;
};

static int debug_open(struct inode *inodep, struct file *filp)
{
	struct driver_debugfs_data *data;
	struct rmi_device *rmi_dev;

	rmi_dev = inodep->i_private;
	data = kzalloc(sizeof(struct driver_debugfs_data),
				GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->rmi_dev = inodep->i_private;
	filp->private_data = data;
	return 0;
}

static int debug_release(struct inode *inodep, struct file *filp)
{
	kfree(filp->private_data);
	return 0;
}

static ssize_t delay_read(struct file *filp, char __user *buffer, size_t size,
		    loff_t *offset) {
	struct driver_debugfs_data *data = filp->private_data;
	struct rmi_device_platform_data *pdata =
			data->rmi_dev->phys->dev->platform_data;
	int retval;
	char *local_buf;

	if (data->done)
		return 0;

	data->done = 1;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = snprintf(local_buf, size, "%d %d %d %d %d\n",
		pdata->spi_data.read_delay_us, pdata->spi_data.write_delay_us,
		pdata->spi_data.block_delay_us,
		pdata->spi_data.pre_delay_us, pdata->spi_data.post_delay_us);

	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static ssize_t delay_write(struct file *filp, const char __user *buffer,
			   size_t size, loff_t *offset) {
	struct driver_debugfs_data *data = filp->private_data;
	struct rmi_device_platform_data *pdata =
			data->rmi_dev->phys->dev->platform_data;
	int retval;
	char *local_buf;
	unsigned int new_read_delay;
	unsigned int new_write_delay;
	unsigned int new_block_delay;
	unsigned int new_pre_delay;
	unsigned int new_post_delay;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	retval = copy_from_user(local_buf, buffer, size);
	if (retval) {
		kfree(local_buf);
		return -EFAULT;
	}

	retval = sscanf(local_buf, "%u %u %u %u %u", &new_read_delay,
			&new_write_delay, &new_block_delay,
			&new_pre_delay, &new_post_delay);
	kfree(local_buf);

	if (retval != 5) {
		dev_err(&data->rmi_dev->dev,
			"Incorrect number of values provided for delay.");
		return -EINVAL;
	}
	dev_dbg(&data->rmi_dev->dev,
		 "Setting delays to %u %u %u %u %u.\n", new_read_delay,
		 new_write_delay, new_block_delay, new_pre_delay,
		 new_post_delay);
	pdata->spi_data.read_delay_us = new_read_delay;
	pdata->spi_data.write_delay_us = new_write_delay;
	pdata->spi_data.block_delay_us = new_block_delay;
	pdata->spi_data.pre_delay_us = new_pre_delay;
	pdata->spi_data.post_delay_us = new_post_delay;

	return size;
}

static const struct file_operations delay_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_release,
	.read = delay_read,
	.write = delay_write,
};

#define PHYS_NAME "phys"

static ssize_t phys_read(struct file *filp, char __user *buffer, size_t size,
		    loff_t *offset) {
	struct driver_debugfs_data *data = filp->private_data;
	struct rmi_phys_info *info = &data->rmi_dev->phys->info;
	int retval;
	char *local_buf;

	if (data->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	data->done = 1;

	retval = snprintf(local_buf, size,
		"%-5s %ld %ld %ld %ld %ld %ld\n",
		 info->proto ? info->proto : "unk",
		 info->tx_count, info->tx_bytes, info->tx_errs,
		 info->rx_count, info->rx_bytes, info->rx_errs);
	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static const struct file_operations phys_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_release,
	.read = phys_read,
};

static ssize_t attn_count_read(struct file *filp, char __user *buffer,
		size_t size, loff_t *offset) {
	struct driver_debugfs_data *data = filp->private_data;
	struct rmi_driver_data *rmi_data = dev_get_drvdata(&data->rmi_dev->dev);
	int retval;
	char *local_buf;

	if (data->done)
		return 0;

	local_buf = kcalloc(size, sizeof(u8), GFP_KERNEL);
	if (!local_buf)
		return -ENOMEM;

	data->done = 1;

	retval = snprintf(local_buf, size, "%d\n",
			  rmi_data->attn_count.counter);
	if (retval <= 0 || copy_to_user(buffer, local_buf, retval))
		retval = -EFAULT;
	kfree(local_buf);

	return retval;
}

static const struct file_operations attn_count_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_release,
	.read = attn_count_read,
};

static int setup_debugfs(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct rmi_phys_info *info = &rmi_dev->phys->info;
	int retval = 0;

	if (!rmi_dev->debugfs_root)
		return -ENODEV;

	if (IS_ENABLED(CONFIG_RMI4_SPI) && !strncmp("spi", info->proto, 3)) {
		data->debugfs_delay = debugfs_create_file("delay",
				RMI_RW_ATTR, rmi_dev->debugfs_root, rmi_dev,
				&delay_fops);
		if (!data->debugfs_delay || IS_ERR(data->debugfs_delay)) {
			dev_warn(&rmi_dev->dev, "Failed to create debugfs delay.\n");
			data->debugfs_delay = NULL;
	}
	}

	data->debugfs_phys = debugfs_create_file(PHYS_NAME, RMI_RO_ATTR,
				rmi_dev->debugfs_root, rmi_dev, &phys_fops);
	if (!data->debugfs_phys || IS_ERR(data->debugfs_phys)) {
		dev_warn(&rmi_dev->dev, "Failed to create debugfs phys.\n");
		data->debugfs_phys = NULL;
	}

	data->debugfs_irq = debugfs_create_bool("irq_debug",
			RMI_RW_ATTR, rmi_dev->debugfs_root, &data->irq_debug);
	if (!data->debugfs_irq || IS_ERR(data->debugfs_irq)) {
		dev_warn(&rmi_dev->dev, "Failed to create debugfs irq_debug.\n");
		data->debugfs_irq = NULL;
	}

	data->debugfs_attn_count = debugfs_create_file("attn_count",
				RMI_RO_ATTR,
				rmi_dev->debugfs_root,
				rmi_dev, &attn_count_fops);
	if (!data->debugfs_phys || IS_ERR(data->debugfs_attn_count)) {
		dev_warn(&rmi_dev->dev, "Failed to create debugfs attn_count.\n");
		data->debugfs_attn_count = NULL;
	}

	return retval;
}

static void teardown_debugfs(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	if (IS_ENABLED(CONFIG_RMI4_SPI) && data->debugfs_delay)
		debugfs_remove(data->debugfs_delay);
	if (data->debugfs_phys)
		debugfs_remove(data->debugfs_phys);
	if (data->debugfs_irq)
		debugfs_remove(data->debugfs_irq);
	if (data->debugfs_attn_count)
		debugfs_remove(data->debugfs_attn_count);
}
#else
#define teardown_debugfs(rmi_dev)
#define setup_debugfs(rmi_dev) 0
#endif

static irqreturn_t rmi_irq_thread(int irq, void *p)
{
	struct rmi_phys_device *phys = p;
	struct rmi_device *rmi_dev = phys->rmi_dev;
	struct rmi_driver *driver = rmi_dev->driver;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;
	struct rmi_driver_data *data;

	data = dev_get_drvdata(&rmi_dev->dev);

	trace_touchscreen_synaptics_irq("Synaptics_interrupt");

	if IRQ_DEBUG(data)
		dev_dbg(phys->dev, "ATTN gpio, value: %d.\n",
				gpio_get_value(pdata->attn_gpio));

	if (gpio_get_value(pdata->attn_gpio) == pdata->attn_polarity) {
		atomic_inc(&data->attn_count);
		if (driver && driver->irq_handler && rmi_dev)
			driver->irq_handler(rmi_dev, irq);
	}

	return IRQ_HANDLED;
}

static int process_interrupt_requests(struct rmi_device *rmi_dev);

static void rmi_poll_work(struct work_struct *work)
{
	struct rmi_driver_data *data =
			container_of(work, struct rmi_driver_data, poll_work);
	struct rmi_device *rmi_dev = data->rmi_dev;

	process_interrupt_requests(rmi_dev);
}

/* This is the timer function for polling - it simply has to schedule work
 * and restart the timer. */
static enum hrtimer_restart rmi_poll_timer(struct hrtimer *timer)
{
	struct rmi_driver_data *data =
			container_of(timer, struct rmi_driver_data, poll_timer);

	if (!data->enabled)
		return HRTIMER_NORESTART;
	if (!work_pending(&data->poll_work))
		schedule_work(&data->poll_work);
	hrtimer_start(&data->poll_timer, data->poll_interval, HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static int enable_polling(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	dev_dbg(&rmi_dev->dev, "Polling enabled.\n");
	INIT_WORK(&data->poll_work, rmi_poll_work);
	hrtimer_init(&data->poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->poll_timer.function = rmi_poll_timer;
	hrtimer_start(&data->poll_timer, data->poll_interval, HRTIMER_MODE_REL);

	return 0;
}

static void disable_polling(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	dev_dbg(&rmi_dev->dev, "Polling disabled.\n");
	hrtimer_cancel(&data->poll_timer);
	cancel_work_sync(&data->poll_work);
}

static void disable_sensor(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	if (!data->enabled)
		return;

	if (!data->irq)
		disable_polling(rmi_dev);

	if (rmi_dev->phys->disable_device)
		rmi_dev->phys->disable_device(rmi_dev->phys);

	if (data->irq) {
		disable_irq(data->irq);
		free_irq(data->irq, rmi_dev->phys);
	}

	data->enabled = false;
}

static int enable_sensor(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct rmi_phys_device *rmi_phys;
	int retval = 0;
	struct rmi_device_platform_data *pdata = to_rmi_platform_data(rmi_dev);

	if (data->enabled)
		return 0;

	if (rmi_dev->phys->enable_device) {
		retval = rmi_dev->phys->enable_device(rmi_dev->phys);
		if (retval)
			return retval;
		}

	rmi_phys = rmi_dev->phys;
	if (data->irq) {
		pr_info("%s: use handler 0x%p\n", __func__,
			rmi_phys->hard_irq ? rmi_phys->hard_irq : NULL);
		retval = request_threaded_irq(data->irq,
				rmi_phys->hard_irq ? rmi_phys->hard_irq : NULL,
				rmi_phys->irq_thread ?
					rmi_phys->irq_thread : rmi_irq_thread,
				data->irq_flags,
				dev_name(&rmi_dev->dev), rmi_phys);
		if (retval)
			return retval;
	} else {
		retval = enable_polling(rmi_dev);
		if (retval < 0)
			return retval;
	}

	data->enabled = true;

	if (!pdata->level_triggered &&
		    gpio_get_value(pdata->attn_gpio) == pdata->attn_polarity)
		retval = process_interrupt_requests(rmi_dev);

	return retval;
}

/* sysfs show and store fns for driver attributes */

static ssize_t rmi_driver_bsr_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;
	rmi_dev = to_rmi_device(dev);
	data = dev_get_drvdata(&rmi_dev->dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->bsr);
}

static ssize_t rmi_driver_bsr_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int retval;
	unsigned long val;
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;

	rmi_dev = to_rmi_device(dev);
	data = dev_get_drvdata(&rmi_dev->dev);

	/* need to convert the string data to an actual value */
	retval = strict_strtoul(buf, 10, &val);
	if (retval < 0 || val > 255) {
		dev_err(dev, "Invalid value '%s' written to BSR.\n", buf);
		return -EINVAL;
	}

	retval = rmi_write(rmi_dev, BSR_LOCATION, (u8)val);
	if (retval < 0) {
		dev_err(dev, "%s : failed to write bsr %lu to %#06x\n",
			__func__, val, BSR_LOCATION);
		return retval;
	}

	data->bsr = val;

	return count;
}

static ssize_t rmi_driver_enabled_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;

	rmi_dev = to_rmi_device(dev);
	data = dev_get_drvdata(&rmi_dev->dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->enabled);
}

static ssize_t rmi_driver_enabled_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int retval;
	int new_value;
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;

	rmi_dev = to_rmi_device(dev);
	data = dev_get_drvdata(&rmi_dev->dev);

	if (sysfs_streq(buf, "0"))
		new_value = false;
	else if (sysfs_streq(buf, "1"))
		new_value = true;
	else
		return -EINVAL;

	if (new_value) {
		retval = enable_sensor(rmi_dev);
		if (retval) {
			dev_err(dev, "Failed to enable sensor, code=%d.\n",
				retval);
			return -EIO;
		}
	} else {
		disable_sensor(rmi_dev);
	}

	return count;
}

/** This sysfs attribute is deprecated, and will be removed in a future release.
 */
static struct device_attribute attrs[] = {
	__ATTR(enabled, RMI_RW_ATTR,
	       rmi_driver_enabled_show, rmi_driver_enabled_store),
};

static struct device_attribute bsr_attribute = __ATTR(bsr, RMI_RW_ATTR,
	       rmi_driver_bsr_show, rmi_driver_bsr_store);

static void rmi_free_function_list(struct rmi_device *rmi_dev)
{
	struct rmi_function_dev *entry, *n;
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	if (!data) {
		dev_err(&rmi_dev->dev, "WTF: No driver data in %s\n", __func__);
		return;
	}

	data->f01_dev = NULL;

	if (list_empty(&data->rmi_functions.list))
		return;

	list_for_each_entry_safe(entry, n, &data->rmi_functions.list, list) {
		device_unregister(&entry->dev);
		list_del(&entry->list);
	}
}

static void release_fndev_device(struct device *dev)
{
	kobject_put(&dev->kobj);
}

static int reset_one_function(struct rmi_function_dev *fn_dev)
{
	struct rmi_function_driver *fn_drv;
	int retval = 0;

	if (!fn_dev || !fn_dev->dev.driver)
		return 0;

	fn_drv = to_rmi_function_driver(fn_dev->dev.driver);
	if (fn_drv->reset) {
		retval = fn_drv->reset(fn_dev);
		if (retval < 0)
			dev_err(&fn_dev->dev, "Reset failed with code %d.\n",
				retval);
	}

	return retval;
}

static int configure_one_function(struct rmi_function_dev *fn_dev)
{
	struct rmi_function_driver *fn_drv;
	int retval = 0;

	if (!fn_dev || !fn_dev->dev.driver)
		return 0;

	fn_drv = to_rmi_function_driver(fn_dev->dev.driver);
	if (fn_drv->config) {
		retval = fn_drv->config(fn_dev);
		if (retval < 0)
			dev_err(&fn_dev->dev, "Config failed with code %d.\n",
				retval);
	}

	return retval;
}

static int rmi_driver_process_reset_requests(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct rmi_function_dev *entry;
	int retval;

	if (list_empty(&data->rmi_functions.list))
		return 0;

	list_for_each_entry(entry, &data->rmi_functions.list, list) {
		retval = reset_one_function(entry);
		if (retval < 0)
			return retval;
	}

	return 0;
}

static int rmi_driver_process_config_requests(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct rmi_function_dev *entry;
	int retval;

	if (list_empty(&data->rmi_functions.list))
		return 0;

	list_for_each_entry(entry, &data->rmi_functions.list, list) {
		retval = configure_one_function(entry);
		if (retval < 0)
			return retval;
	}

	return 0;
}

static void process_one_interrupt(struct rmi_function_dev *fn_dev,
		unsigned long *irq_status, struct rmi_driver_data *data)
{
	struct rmi_function_driver *fn_drv;
	DECLARE_BITMAP(irq_bits, data->num_of_irq_regs);

	if (!fn_dev || !fn_dev->dev.driver)
		return;

	fn_drv = to_rmi_function_driver(fn_dev->dev.driver);
	if (fn_dev->irq_mask && fn_drv->attention) {
		bitmap_and(irq_bits, irq_status, fn_dev->irq_mask,
				data->irq_count);
		if (!bitmap_empty(irq_bits, data->irq_count))
			fn_drv->attention(fn_dev, irq_bits);
	}
}

static int process_interrupt_requests(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct device *dev = &rmi_dev->dev;
	struct rmi_function_dev *entry;
	int error;

	error = rmi_read_block(rmi_dev,
				data->f01_dev->fd.data_base_addr + 1,
				data->irq_status, data->num_of_irq_regs);
	if (error < 0) {
		dev_err(dev, "Failed to read irqs, code=%d\n", error);
		return error;
	}

	mutex_lock(&data->irq_mutex);
	bitmap_and(data->irq_status, data->irq_status, data->current_irq_mask,
	       data->irq_count);
	/* At this point, irq_status has all bits that are set in the
	 * interrupt status register and are enabled.
	 */
	mutex_unlock(&data->irq_mutex);

	/* It would be nice to be able to use irq_chip to handle these
	 * nested IRQs.  Unfortunately, most of the current customers for
	 * this driver are using older kernels (3.0.x) that don't support
	 * the features required for that.  Once they've shifted to more
	 * recent kernels (say, 3.3 and higher), this should be switched to
	 * use irq_chip.
	 */
	list_for_each_entry(entry, &data->rmi_functions.list, list) {
		if (entry->irq_mask)
			process_one_interrupt(entry, data->irq_status, data);
	}

	return 0;
}

/**
 * rmi_driver_set_input_params - set input device id and other data.
 *
 * @rmi_dev: Pointer to an RMI device
 * @input: Pointer to input device
 *
 */
static int rmi_driver_set_input_params(struct rmi_device *rmi_dev,
				struct input_dev *input)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	input->name = SYNAPTICS_INPUT_DEVICE_NAME;
	input->id.vendor  = SYNAPTICS_VENDOR_ID;
	input->id.product = data->board;
	input->id.version = data->rev;
	input->id.bustype = BUS_RMI;
	return 0;
}

/**
 * This pair of functions allows functions like function 54 to request to have
 * other interrupts disabled until the restore function is called. Only one
 * store happens at a time.
 */
static int rmi_driver_irq_save(struct rmi_device *rmi_dev,
				unsigned long *new_ints)
{
	int retval = 0;
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct device *dev = &rmi_dev->dev;

	mutex_lock(&data->irq_mutex);
	if (!data->irq_stored) {
		/* Save current enabled interrupts */
		retval = rmi_read_block(rmi_dev,
				data->f01_dev->fd.control_base_addr+1,
				data->irq_mask_store, data->num_of_irq_regs);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to read enabled interrupts!",
								__func__);
			goto error_unlock;
		}
		/*
		 * Disable every interrupt except for function 54
		 * TODO:Will also want to not disable function 1-like functions.
		 * No need to take care of this now, since there's no good way
		 * to identify them.
		 */
		retval = rmi_write_block(rmi_dev,
				data->f01_dev->fd.control_base_addr+1,
				new_ints, data->num_of_irq_regs);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to change enabled interrupts!",
								__func__);
			goto error_unlock;
		}
		bitmap_copy(data->current_irq_mask, new_ints, data->irq_count);
		data->irq_stored = true;
	} else {
		retval = -ENOSPC; /* No space to store IRQs.*/
		dev_err(dev, "Attempted to save IRQs when already stored!");
	}

error_unlock:
	mutex_unlock(&data->irq_mutex);
	return retval;
}

static int rmi_driver_irq_restore(struct rmi_device *rmi_dev)
{
	int retval = 0;
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct device *dev = &rmi_dev->dev;

	if ( rmi_dev->interrupt_restore_block_flag ) {
		/* in Direct Touch Mode, or other related modes,
		** we do nto restore interrupts -- done automagically
		** by the firmware
		*/
		return retval;
	}


	mutex_lock(&data->irq_mutex);

	if (data->irq_stored) {
		retval = rmi_write_block(rmi_dev,
				data->f01_dev->fd.control_base_addr+1,
				data->irq_mask_store, data->num_of_irq_regs);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to write enabled interupts!",
								__func__);
			goto error_unlock;
		}
		memcpy(data->current_irq_mask, data->irq_mask_store,
					data->num_of_irq_regs * sizeof(u8));
		data->irq_stored = false;
	} else {
		retval = -EINVAL;
		dev_err(dev, "%s: Attempted to restore values when not stored!",
			__func__);
	}

error_unlock:
	mutex_unlock(&data->irq_mutex);
	return retval;
}

static int rmi_driver_irq_handler(struct rmi_device *rmi_dev, int irq)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	//pr_info("%s: rmi_driver_irq_handler IRQ = 0x%x\n", __func__, irq);

	might_sleep();
	/* Can get called before the driver is fully ready to deal with
	 * interrupts.
	 */
	if (!data || !data->f01_dev) {
		dev_dbg(&rmi_dev->dev,
			 "Not ready to handle interrupts yet!\n");
		return 0;
	}

	return process_interrupt_requests(rmi_dev);
}

static int rmi_driver_reset_handler(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	int error;

	/* Can get called before the driver is fully ready to deal with
	 * this situation.
	 */
	if (!data || !data->f01_dev) {
		dev_warn(&rmi_dev->dev,
			 "Not ready to handle reset yet!\n");
		return 0;
	}

	error = rmi_driver_process_reset_requests(rmi_dev);
	if (error < 0)
		return error;


	error = rmi_driver_process_config_requests(rmi_dev);
	if (error < 0)
		return error;

	if (data->irq_stored) {
		error = rmi_driver_irq_restore(rmi_dev);
		if (error < 0)
			return error;
	}

	return 0;
}

/*
 * Construct a function's IRQ mask. This should be called once and stored.
 */
int rmi_driver_irq_get_mask(struct rmi_device *rmi_dev,
		struct rmi_function_dev *fn_dev) {
	int i;
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	/* call devm_kcalloc when it will be defined in kernel in future */
	fn_dev->irq_mask = devm_kzalloc(&rmi_dev->dev,
			BITS_TO_LONGS(data->irq_count)*sizeof(unsigned long),
			GFP_KERNEL);

	if (!fn_dev->irq_mask)
		return -ENOMEM;

	for (i = 0; i < fn_dev->num_of_irqs; i++)
		set_bit(fn_dev->irq_pos+i, fn_dev->irq_mask);
	return 0;
}

static int init_function_device(struct rmi_device *rmi_dev,
			     struct rmi_function_dev *fn_dev)
{
	int retval;

	/* This memset might not be what we want to do... */
	memset(&(fn_dev->dev), 0, sizeof(struct device));
	dev_set_name(&(fn_dev->dev), "%s.fn%02x", dev_name(&rmi_dev->dev),
			fn_dev->fd.function_number);
	fn_dev->dev.release = release_fndev_device;

	fn_dev->dev.parent = &rmi_dev->dev;
	fn_dev->dev.type = &rmi_function_type;
	fn_dev->dev.bus = &rmi_bus_type;

	if (IS_ENABLED(CONFIG_RMI4_DEBUG)) {
		char dirname[12];

		snprintf(dirname, 12, "F%02X", fn_dev->fd.function_number);
		fn_dev->debugfs_root = debugfs_create_dir(dirname,
						      rmi_dev->debugfs_root);
		if (!fn_dev->debugfs_root)
			dev_warn(&fn_dev->dev, "Failed to create debugfs dir.\n");
	}

	dev_dbg(&rmi_dev->dev, "Register F%02X.\n", fn_dev->fd.function_number);
	retval = device_register(&fn_dev->dev);
	if (retval) {
		dev_err(&rmi_dev->dev, "Failed device_register for F%02X.\n",
			fn_dev->fd.function_number);
		return retval;
	}

	return 0;
}

static int create_function_dev(struct rmi_device *rmi_dev,
				     struct pdt_entry *pdt_ptr,
				     int *current_irq_count,
				     u16 page_start)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct rmi_function_dev *fn_dev = NULL;
	int retval = 0;
	struct device *dev = &rmi_dev->dev;
	struct rmi_device_platform_data *pdata;

	pdata = to_rmi_platform_data(rmi_dev);

	dev_dbg(dev, "Initializing F%02X for %s.\n", pdt_ptr->function_number,
		pdata->sensor_name);

	fn_dev = devm_kzalloc(dev, sizeof(struct rmi_function_dev),
			GFP_KERNEL);
	if (!fn_dev) {
		dev_err(dev, "Failed to allocate F%02X device.\n",
			pdt_ptr->function_number);
		return -ENOMEM;
	}

	copy_pdt_entry_to_fd(pdt_ptr, &fn_dev->fd, page_start);

	fn_dev->rmi_dev = rmi_dev;
	fn_dev->num_of_irqs = pdt_ptr->interrupt_source_count;
	fn_dev->irq_pos = *current_irq_count;
	*current_irq_count += fn_dev->num_of_irqs;

	retval = rmi_driver_irq_get_mask(rmi_dev, fn_dev);
	if (retval < 0) {
		dev_err(dev, "%s: Failed to create irq_mask for F%02X.\n",
			__func__, pdt_ptr->function_number);
		return retval;
	}

	retval = init_function_device(rmi_dev, fn_dev);
	if (retval < 0) {
		dev_err(dev, "Failed to initialize F%02X device.\n",
			pdt_ptr->function_number);
		return retval;
	}

	INIT_LIST_HEAD(&fn_dev->list);
	/* we need to ensure that F01 is at the head of the list.
	 */
	if (pdt_ptr->function_number == 0x01) {
		list_add(&fn_dev->list, &data->rmi_functions.list);
		data->f01_dev = fn_dev;
	} else
		list_add_tail(&fn_dev->list, &data->rmi_functions.list);

	return 0;
}

/*
 * Once we find F01, we need to see if we're in bootloader mode.  If we are,
 * we'll stop scanning the PDT with the current page (usually 0x00 in that
 * case).
 */
static void check_bootloader_mode(struct rmi_device *rmi_dev,
				     struct pdt_entry *pdt_ptr,
				     u16 page_start)
{
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);
	struct f01_device_status device_status;
	int retval = 0;

	retval = rmi_read(rmi_dev, pdt_ptr->data_base_addr+page_start,
			  &device_status);
	if (retval < 0) {
		dev_err(&rmi_dev->dev, "Failed to read device status.\n");
		return;
	}
	data->f01_bootloader_mode = device_status.flash_prog;
	if (device_status.flash_prog)
		dev_warn(&rmi_dev->dev,
			 "WARNING: RMI4 device is in bootloader mode!\n");

}

/*
 * We also reflash the device if (a) in kernel reflashing is
 * enabled, and (b) the reflash module decides it requires reflashing.
 *
 * We have to do this before actually building the PDT because the reflash
 * might cause various registers to move around.
 */
static int rmi_device_reflash(struct rmi_device *rmi_dev)
{
	struct pdt_entry pdt_entry;
	int page;
	struct device *dev = &rmi_dev->dev;
	bool done;
	bool has_f01 = false;
	bool has_f34 = false;
	struct pdt_entry f34_pdt, f01_pdt;
	int i;
	int retval;
	struct rmi_device_platform_data *pdata;
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	dev_dbg(dev, "Initial reflash.\n");
	pdata = to_rmi_platform_data(rmi_dev);
	data->f01_bootloader_mode = false;
	for (page = 0; (page <= RMI4_MAX_PAGE); page++) {
		u16 page_start = RMI4_PAGE_SIZE * page;
		u16 pdt_start = page_start + PDT_START_SCAN_LOCATION;
		u16 pdt_end = page_start + PDT_END_SCAN_LOCATION;
		done = true;
		for (i = pdt_start; i >= pdt_end ; i -= sizeof(pdt_entry)) {
			retval = rmi_read_block(rmi_dev, i, &pdt_entry,
					       sizeof(pdt_entry));
			if (retval != sizeof(pdt_entry)) {
				dev_err(dev, "Read PDT entry at %#06x failed, code = %d.\n",
						i, retval);
				return retval;
			}

			if (RMI4_END_OF_PDT(pdt_entry.function_number))
				break;
			done = false;
			if (pdt_entry.function_number == 0x01) {
				memcpy(&f01_pdt, &pdt_entry, sizeof(pdt_entry));
				has_f01 = true;
				check_bootloader_mode(rmi_dev, &pdt_entry,
						      page_start);
			} else if (pdt_entry.function_number == 0x34) {
				memcpy(&f34_pdt, &pdt_entry, sizeof(pdt_entry));
				has_f34 = true;
				}

			if (has_f01 && has_f34) {
				done = true;
				break;
			}
		}

		if (data->f01_bootloader_mode || done)
			break;
	}

	if (!has_f01) {
		dev_warn(dev, "WARNING: Failed to find F01 for initial reflash.\n");
	return -ENODEV;
	}

	if (has_f34)
		rmi4_fw_update(rmi_dev, &f01_pdt, &f34_pdt);
	else
		dev_warn(dev, "WARNING: No F34 , firmware update will not be done.\n");
	return 0;
}

/*
 * Scan the PDT for F01 so we can force a reset before anything else
 * is done.  This forces the sensor into a known state, and also
 * forces application of any pending updates from reflashing the
 * firmware or configuration.
 *
 */
static int rmi_device_reset(struct rmi_device *rmi_dev)
{
	struct pdt_entry pdt_entry;
	int page;
	struct device *dev = &rmi_dev->dev;
	int i;
	int retval;
	bool done = false;
	struct rmi_device_platform_data *pdata;

	dev_dbg(dev, "Initial reset.\n");
	pdata = to_rmi_platform_data(rmi_dev);
	for (page = 0; (page <= RMI4_MAX_PAGE) && !done; page++) {
		u16 page_start = RMI4_PAGE_SIZE * page;
		u16 pdt_start = page_start + PDT_START_SCAN_LOCATION;
		u16 pdt_end = page_start + PDT_END_SCAN_LOCATION;
		done = true;

		for (i = pdt_start; i >= pdt_end; i -= sizeof(pdt_entry)) {
			retval = rmi_read_block(rmi_dev, i, &pdt_entry,
					       sizeof(pdt_entry));
			if (retval != sizeof(pdt_entry)) {
				dev_err(dev, "Read PDT entry at %#06x failed, code = %d.\n",
						i, retval);
				return retval;
			}

			if (RMI4_END_OF_PDT(pdt_entry.function_number))
				break;
			done = false;

			if (pdt_entry.function_number == 0x01) {
				u16 cmd_addr = page_start +
					pdt_entry.command_base_addr;
				u8 cmd_buf = RMI_DEVICE_RESET_CMD;
				retval = rmi_write_block(rmi_dev, cmd_addr,
						&cmd_buf, 1);
				if (retval < 0) {
					dev_err(dev, "Initial reset failed. Code = %d.\n",
						retval);
					return retval;
				}
				mdelay(pdata->reset_delay_ms);
				return 0;
			}
			}
			}

	return -ENODEV;
}

static void get_prod_id(struct rmi_device *rmi_dev,
			struct rmi_driver_data *drvdata)
{
	struct device *dev = &rmi_dev->dev;
	int retval;
	int board = 0, rev = 0;
	int i;
	static const char * const pattern[] = {
		"tm%4d-%d", "s%4d-%d", "s%4d-ver%1d"};
	u8 product_id[RMI_PRODUCT_ID_LENGTH+1];

	retval = rmi_read_block(rmi_dev,
		drvdata->f01_dev->fd.query_base_addr+
		sizeof(struct f01_basic_queries),
		product_id, RMI_PRODUCT_ID_LENGTH);
			if (retval < 0) {
		dev_err(dev, "Failed to read product id, code=%d!", retval);
		return;
	}
	product_id[RMI_PRODUCT_ID_LENGTH] = '\0';

	for (i = 0; i < sizeof(product_id); i++)
		product_id[i] = tolower(product_id[i]);

	for (i = 0; i < ARRAY_SIZE(pattern); i++) {
		retval = sscanf(product_id, pattern[i], &board, &rev);
		if (retval)
			break;
	}
	/* save board and rev data in the rmi_driver_data */
	drvdata->board = board;
	drvdata->rev = rev;
	dev_dbg(dev, "Rmi_driver getProdID, set board: %d rev: %d\n",
		drvdata->board, drvdata->rev);
}

static int rmi_count_irqs(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data;
	struct pdt_entry pdt_entry;
	int page;
	struct device *dev = &rmi_dev->dev;
	int irq_count = 0;
	bool done = false;
	int i;
	int retval;

	data = dev_get_drvdata(&rmi_dev->dev);
	mutex_lock(&data->pdt_mutex);

	for (page = 0; (page <= RMI4_MAX_PAGE) && !done; page++) {
		u16 page_start = RMI4_PAGE_SIZE * page;
		u16 pdt_start = page_start + PDT_START_SCAN_LOCATION;
		u16 pdt_end = page_start + PDT_END_SCAN_LOCATION;

		done = true;
		for (i = pdt_start; i >= pdt_end; i -= sizeof(pdt_entry)) {
			retval = rmi_read_block(rmi_dev, i, &pdt_entry,
					       sizeof(pdt_entry));
			if (retval != sizeof(pdt_entry)) {
				dev_err(dev, "Read of PDT entry at %#06x failed.\n",
					i);
				goto error_exit;
			}

			if (RMI4_END_OF_PDT(pdt_entry.function_number))
				break;
			irq_count += pdt_entry.interrupt_source_count;
			done = false;

			if (pdt_entry.function_number == 0x01)
				check_bootloader_mode(rmi_dev, &pdt_entry,
						      page_start);
		}
		done = done || data->f01_bootloader_mode;
	}
	data->irq_count = irq_count;
	data->num_of_irq_regs = (irq_count + 7) / 8;
	retval = 0;

error_exit:
	mutex_unlock(&data->pdt_mutex);
	return retval;
}

static int rmi_scan_pdt(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data;
	struct pdt_entry pdt_entry;
	int page;
	struct device *dev = &rmi_dev->dev;
	int irq_count = 0;
	bool done = false;
	int i;
	int retval;

	dev_dbg(dev, "Scanning PDT...\n");

	data = dev_get_drvdata(&rmi_dev->dev);
	mutex_lock(&data->pdt_mutex);

	for (page = 0; (page <= RMI4_MAX_PAGE) && !done; page++) {
		u16 page_start = RMI4_PAGE_SIZE * page;
		u16 pdt_start = page_start + PDT_START_SCAN_LOCATION;
		u16 pdt_end = page_start + PDT_END_SCAN_LOCATION;

		done = true;
		for (i = pdt_start; i >= pdt_end; i -= sizeof(pdt_entry)) {
			retval = rmi_read_block(rmi_dev, i, &pdt_entry,
					       sizeof(pdt_entry));
			if (retval != sizeof(pdt_entry)) {
				dev_err(dev, "Read of PDT entry at %#06x failed.\n",
					i);
				goto error_exit;
			}

			if (RMI4_END_OF_PDT(pdt_entry.function_number))
				break;

			dev_dbg(dev, "Found F%02X on page %#04x\n",
					pdt_entry.function_number, page);
			done = false;

			if (pdt_entry.function_number == 0x01)
				check_bootloader_mode(rmi_dev, &pdt_entry,
						      page_start);


			retval = create_function_dev(rmi_dev,
					&pdt_entry, &irq_count, page_start);

			if (retval)
				goto error_exit;

			if (pdt_entry.function_number == 0x01)
				get_prod_id(rmi_dev, data);
	}
		done = done || data->f01_bootloader_mode;
	}
	dev_dbg(dev, "%s: Done with PDT scan.\n", __func__);
	retval = 0;

error_exit:
	mutex_unlock(&data->pdt_mutex);
	return retval;
}

static int f01_notifier_call(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct device *dev = data;
	struct rmi_function_dev *fn_dev;

	if (dev->type != &rmi_function_type)
		return 0;

	fn_dev = to_rmi_function_dev(dev);
	if (fn_dev->fd.function_number != 0x01)
		return 0;

	switch (action) {
	case BUS_NOTIFY_BOUND_DRIVER:
		enable_sensor(fn_dev->rmi_dev);
		break;
	case BUS_NOTIFY_UNBIND_DRIVER:
		disable_sensor(fn_dev->rmi_dev);
		break;
	}
	return 0;
}

static struct notifier_block rmi_bus_notifier = {
	.notifier_call = f01_notifier_call,
};

#ifdef	CONFIG_PM
static int suspend_one_device(struct rmi_function_dev *fn_dev)
{
	struct rmi_function_driver *fn_drv;
	int retval = 0;

	if (!fn_dev->dev.driver)
		return 0;

	fn_drv = to_rmi_function_driver(fn_dev->dev.driver);

	if (fn_drv->suspend) {
		retval = fn_drv->suspend(fn_dev);
		if (retval < 0)
			dev_err(&fn_dev->dev, "Suspend failed, code: %d",
				retval);
	}

	return retval;
}

static int rmi_driver_suspend(struct device *dev)
{
	struct rmi_driver_data *data;
	struct rmi_function_dev *entry;
	int retval = 0;
	struct rmi_device *rmi_dev = to_rmi_device(dev);

	data = dev_get_drvdata(&rmi_dev->dev);

	mutex_lock(&data->suspend_mutex);
	if (data->suspended)
		goto exit;

	if (!IS_ENABLED(CONFIG_HAS_EARLYSUSPEND) && data->pre_suspend) {
		retval = data->pre_suspend(data->pm_data);
		if (retval)
			goto exit;
	}

	disable_sensor(rmi_dev);

	/** Do it backwards so F01 comes last. */
	list_for_each_entry_reverse(entry, &data->rmi_functions.list, list)
		if (suspend_one_device(entry) < 0)
			goto exit;

	data->suspended = true;

	if (data->post_suspend)
		retval = data->post_suspend(data->pm_data);

exit:
	mutex_unlock(&data->suspend_mutex);
	return retval;
}

static int resume_one_device(struct rmi_function_dev *fn_dev)
{
	struct rmi_function_driver *fn_drv;
	int retval = 0;

	if (!fn_dev->dev.driver)
		return 0;

	fn_drv = to_rmi_function_driver(fn_dev->dev.driver);

	if (fn_drv->resume) {
		retval = fn_drv->resume(fn_dev);
		if (retval < 0)
			dev_err(&fn_dev->dev, "Resume failed, code: %d",
				retval);
	}

	return retval;
}

static int rmi_driver_resume(struct device *dev)
{
	struct rmi_driver_data *data;
	struct rmi_function_dev *entry;
	int retval = 0;
	struct rmi_device *rmi_dev = to_rmi_device(dev);

	data = dev_get_drvdata(&rmi_dev->dev);
	mutex_lock(&data->suspend_mutex);
	if (!data->suspended)
		goto exit;

	if (data->pre_resume) {
		retval = data->pre_resume(data->pm_data);
		if (retval)
			goto exit;
	}

	/** Do it forwards, so F01 comes first. */
	list_for_each_entry(entry, &data->rmi_functions.list, list) {
		if (resume_one_device(entry) < 0)
				goto exit;
		}

	retval = enable_sensor(rmi_dev);
	if (retval)
		goto exit;


	if (!IS_ENABLED(CONFIG_HAS_EARLYSUSPEND) && data->post_resume) {
		retval = data->post_resume(data->pm_data);
		if (retval)
			goto exit;
	}

	data->suspended = false;
exit:
	mutex_unlock(&data->suspend_mutex);
	return retval;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)

static int early_suspend_one_device(struct rmi_function_dev *fn_dev)
{
	struct rmi_function_driver *fn_drv;
	int retval = 0;

	if (!fn_dev->dev.driver)
	return 0;

	fn_drv = to_rmi_function_driver(fn_dev->dev.driver);

	if (fn_drv->early_suspend) {
		retval = fn_drv->early_suspend(fn_dev);
		if (retval < 0)
			dev_err(&fn_dev->dev, "Early suspend failed, code: %d",
				retval);
	}

	return retval;
}

static void rmi_driver_early_suspend(struct early_suspend *h)
{
	struct rmi_device *rmi_dev =
	    container_of(h, struct rmi_device, early_suspend_handler);
	struct rmi_driver_data *data;
	struct rmi_function_dev *entry;
	int retval = 0;

	data = dev_get_drvdata(&rmi_dev->dev);

	mutex_lock(&data->suspend_mutex);
	if (data->early_suspended)
		goto exit;

	if (data->pre_suspend) {
		retval = data->pre_suspend(data->pm_data);
		if (retval) {
			dev_err(&rmi_dev->dev, "Presuspend failed with %d.\n",
				retval);
			goto exit;
		}
	}

	/* Do it backwards, so F01 comes last. */
	list_for_each_entry_reverse(entry, &data->rmi_functions.list, list)
		if (early_suspend_one_device(entry) < 0)
			goto exit;

	data->early_suspended = true;
exit:
	mutex_unlock(&data->suspend_mutex);
}

static int late_resume_one_device(struct rmi_function_dev *fn_dev)
{
	struct rmi_function_driver *fn_drv;
	int retval = 0;

	if (!fn_dev->dev.driver)
		return 0;

	fn_drv = to_rmi_function_driver(fn_dev->dev.driver);

	if (fn_drv->late_resume) {
		retval = fn_drv->late_resume(fn_dev);
		if (retval < 0)
			dev_err(&fn_dev->dev, "Late resume failed, code: %d",
				retval);
	}

	return retval;
}

static void rmi_driver_late_resume(struct early_suspend *h)
{
	struct rmi_device *rmi_dev =
	    container_of(h, struct rmi_device, early_suspend_handler);
	struct rmi_driver_data *data;
	struct rmi_function_dev *entry;
	int retval = 0;

	data = dev_get_drvdata(&rmi_dev->dev);

	mutex_lock(&data->suspend_mutex);
	if (!data->early_suspended)
		goto exit;

	/* Do it forwards, so F01 comes first. */
	list_for_each_entry(entry, &data->rmi_functions.list, list) {
		if (late_resume_one_device(entry) < 0)
			goto exit;
	}

	if (data->post_resume) {
		retval = data->post_resume(data->pm_data);
	if (retval) {
			dev_err(&rmi_dev->dev, "Post resume failed with %d.\n",
				retval);
			goto exit;
		}
	}

	data->early_suspended = false;

exit:
	mutex_unlock(&data->suspend_mutex);
}
#endif /* defined(CONFIG_HAS_EARLYSUSPEND) */

#endif /* CONFIG_PM */

static int rmi_driver_remove(struct rmi_device *rmi_dev)
{
	int i;
	struct rmi_driver_data *data = dev_get_drvdata(&rmi_dev->dev);

	disable_sensor(rmi_dev);

#if 0
	if (IS_ENABLED(CONFIG_HAS_EARLYSUSPEND))
		unregister_early_suspend(&rmi_dev->early_suspend_handler);
#endif
	teardown_debugfs(rmi_dev);

	rmi_free_function_list(rmi_dev);
	for (i = 0; i < ARRAY_SIZE(attrs); i++)
		device_remove_file(&rmi_dev->dev, &attrs[i]);
	if (data->pdt_props.has_bsr)
		device_remove_file(&rmi_dev->dev, &bsr_attribute);
	return 0;
}

static int rmi_driver_probe(struct device *dev)
{
	struct rmi_driver *rmi_driver;
	struct rmi_driver_data *data = NULL;
	struct rmi_device_platform_data *pdata;
	int retval = 0;
	int attr_count = 0;
	struct rmi_device *rmi_dev;

	if (!dev->driver) {
		dev_err(dev, "No driver for RMI4 device during probe!\n");
		return -ENODEV;
	}

	if (dev->type != &rmi_sensor_type)
		return -ENODEV;

	rmi_dev = to_rmi_device(dev);
	rmi_driver = to_rmi_driver(dev->driver);
	rmi_dev->driver = rmi_driver;

	pdata = to_rmi_platform_data(rmi_dev);

	data = devm_kzalloc(dev, sizeof(struct rmi_driver_data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "%s: Failed to allocate driver data.\n", __func__);
		return -ENOMEM;
		}
	INIT_LIST_HEAD(&data->rmi_functions.list);
	data->rmi_dev = rmi_dev;
	dev_set_drvdata(&rmi_dev->dev, data);
	mutex_init(&data->pdt_mutex);

	/* Right before a warm boot, the sensor might be in some unusual state,
	 * such as F54 diagnostics, or F34 bootloader mode.  In order to clear
	 * the sensor to a known state, we issue a initial reset to clear any
	 * previous settings and force it into normal operation.
	 *
	 * For a number of reasons, this initial reset may fail to return
	 * within the specified time, but we'll still be able to bring up the
	 * driver normally after that failure.  This occurs most commonly in
	 * a cold boot situation (where then firmware takes longer to come up
	 * than from a warm boot) and the reset_delay_ms in the platform data
	 * has been set too short to accomodate that.  Since the sensor will
	 * eventually come up and be usable, we don't want to just fail here
	 * and leave the customer's device unusable.  So we warn them, and
	 * continue processing.
	 */
	if (!pdata->reset_delay_ms)
		pdata->reset_delay_ms = DEFAULT_RESET_DELAY_MS;
	retval = rmi_device_reset(rmi_dev);
	if (retval)
		dev_warn(dev, "RMI initial reset failed! Continuing in spite of this.\n");

	retval = rmi_device_reflash(rmi_dev);
	if (retval)
		dev_warn(dev, "RMI reflash failed! Continuing in spite of this.\n");

	retval = rmi_read(rmi_dev, PDT_PROPERTIES_LOCATION, &data->pdt_props);
	if (retval < 0) {
		/* we'll print out a warning and continue since
		 * failure to get the PDT properties is not a cause to fail
		 */
		dev_warn(dev, "Could not read PDT properties from %#06x. Assuming 0x00.\n",
			 PDT_PROPERTIES_LOCATION);
	}

	if (pdata->attn_gpio) {
		data->irq = gpio_to_irq(pdata->attn_gpio);
		if (pdata->level_triggered) {
			data->irq_flags = IRQF_ONESHOT |
				((pdata->attn_polarity == RMI_ATTN_ACTIVE_HIGH)
				? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW);
		} else {
			data->irq_flags =
				(pdata->attn_polarity == RMI_ATTN_ACTIVE_HIGH)
				? IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING;
		}
		dev_dbg(dev, "Mapped IRQ %d for GPIO %d.\n",
			data->irq, pdata->attn_gpio);
	} else
		data->poll_interval = ktime_set(0,
			(pdata->poll_interval_ms ? pdata->poll_interval_ms :
			DEFAULT_POLL_INTERVAL_MS) * 1000);

	retval = rmi_count_irqs(rmi_dev);
	if (retval) {
		dev_err(dev, "IRQ counting for %s failed with code %d.\n",
			pdata->sensor_name, retval);
		goto err_free_data;
	}

	mutex_init(&data->irq_mutex);
	data->irq_status = devm_kzalloc(dev,
		BITS_TO_LONGS(data->irq_count)*sizeof(unsigned long),
		GFP_KERNEL);
	if (!data->irq_status) {
		dev_err(dev, "Failed to allocate irq_status.\n");
		retval = -ENOMEM;
		goto err_free_data;
	}

	data->current_irq_mask = devm_kzalloc(dev, data->num_of_irq_regs,
				GFP_KERNEL);
	if (!data->current_irq_mask) {
		dev_err(dev, "Failed to allocate current_irq_mask.\n");
		retval = -ENOMEM;
		goto err_free_data;
	}

	data->irq_mask_store = devm_kzalloc(dev,
		BITS_TO_LONGS(data->irq_count)*sizeof(unsigned long),
		GFP_KERNEL);
	if (!data->irq_mask_store) {
		dev_err(dev, "Failed to allocate mask store.\n");
		retval = -ENOMEM;
		goto err_free_data;
	}

	retval = setup_debugfs(rmi_dev);
	if (retval < 0)
		dev_warn(dev, "Failed to setup debugfs. Code: %d.\n", retval);

	retval = rmi_scan_pdt(rmi_dev);
	if (retval) {
		dev_err(dev, "PDT scan for %s failed with code %d.\n",
			pdata->sensor_name, retval);
		goto err_free_data;
	}

	if (!data->f01_dev) {
		dev_err(dev, "missing F01 device!\n");
		retval = -EINVAL;
		goto err_free_data;
	}

	dev_dbg(dev, "%s: Creating sysfs files.", __func__);
	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = device_create_file(dev, &attrs[attr_count]);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to create sysfs file %s.\n",
				__func__, attrs[attr_count].attr.name);
			goto err_free_data;
		}
	}
	if (data->pdt_props.has_bsr) {
		retval = device_create_file(dev, &bsr_attribute);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to create sysfs file bsr.\n",
				__func__);
			goto err_free_data;
		}
	}

	retval = rmi_read_block(rmi_dev,
				data->f01_dev->fd.control_base_addr+1,
				data->current_irq_mask, data->num_of_irq_regs);
	if (retval < 0) {
		dev_err(dev, "%s: Failed to read current IRQ mask.\n",
			__func__);
		goto err_free_data;
	}

	if (IS_ENABLED(CONFIG_PM)) {
		data->pm_data = pdata->pm_data;
		data->pre_suspend = pdata->pre_suspend;
		data->post_suspend = pdata->post_suspend;
		data->pre_resume = pdata->pre_resume;
		data->post_resume = pdata->post_resume;

		mutex_init(&data->suspend_mutex);

#if 0
		if (IS_ENABLED(CONFIG_HAS_EARLYSUSPEND)) {
			rmi_dev->early_suspend_handler.level =
				EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
			rmi_dev->early_suspend_handler.suspend =
						rmi_driver_early_suspend;
			rmi_dev->early_suspend_handler.resume =
						rmi_driver_late_resume;
			register_early_suspend(&rmi_dev->early_suspend_handler);
	}
#endif
	}

	if (data->f01_dev->dev.driver) {
		/* Driver already bound, so enable ATTN now. */
		enable_sensor(rmi_dev);
	}

	if (IS_ENABLED(CONFIG_RMI4_DEV) && pdata->attn_gpio) {
		retval = gpio_export(pdata->attn_gpio, false);
		if (retval) {
			dev_warn(dev, "WARNING: Failed to export ATTN gpio!\n");
			retval = 0;
		} else {
			retval = gpio_export_link(dev,
						  "attn", pdata->attn_gpio);
			if (retval) {
				dev_warn(dev,
					"WARNING: Failed to symlink ATTN gpio!\n");
				retval = 0;
			} else {
				dev_info(dev, "Exported ATTN GPIO %d.",
					pdata->attn_gpio);
	}
	}
	}

	return 0;

 err_free_data:
	rmi_free_function_list(rmi_dev);
	for (attr_count--; attr_count >= 0; attr_count--)
		device_remove_file(dev, &attrs[attr_count]);
	if (data->pdt_props.has_bsr)
		device_remove_file(dev, &bsr_attribute);
	return retval;
}

static UNIVERSAL_DEV_PM_OPS(rmi_driver_pm, rmi_driver_suspend,
			    rmi_driver_resume, NULL);

struct rmi_driver rmi_sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "rmi_generic",
		.bus = &rmi_bus_type,
		.pm = &rmi_driver_pm,
		.probe = rmi_driver_probe,
	},
	.irq_handler = rmi_driver_irq_handler,
	.reset_handler = rmi_driver_reset_handler,
	.store_irq_mask = rmi_driver_irq_save,
	.restore_irq_mask = rmi_driver_irq_restore,
	.set_input_params = rmi_driver_set_input_params,
	.remove = rmi_driver_remove,
};

int __init rmi_register_sensor_driver(void)
{
	int retval;

	retval = driver_register(&rmi_sensor_driver.driver);
	if (retval) {
		pr_err("%s: driver register failed, code=%d.\n", __func__,
		       retval);
		return retval;
	}

	/* Ask the bus to let us know when drivers are bound to devices. */
	retval = bus_register_notifier(&rmi_bus_type, &rmi_bus_notifier);
	if (retval) {
		pr_err("%s: failed to register bus notifier, code=%d.\n",
		       __func__, retval);
		return retval;
	}

	return 0;
}

void __exit rmi_unregister_sensor_driver(void)
{
	bus_unregister_notifier(&rmi_bus_type, &rmi_bus_notifier);
	driver_unregister(&rmi_sensor_driver.driver);
}

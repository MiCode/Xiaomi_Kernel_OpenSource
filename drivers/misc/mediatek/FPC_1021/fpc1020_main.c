/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/sort.h>

#include <linux/of.h>
#include "fpc1020.h"
#include "fpc1020_common.h"
#include "fpc1020_regs.h"
#include "fpc1020_input.h"
#include "fpc1020_capture.h"
#include "fpc1020_regulator.h"


#include <mach/mt_gpio.h>
#include <mach/mt_spi.h>
#include <mach/eint.h>
#include <cust_eint.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fingerprint Cards AB <tech@fingerprints.com>");
MODULE_DESCRIPTION("FPC1020 touch sensor driver.");


/* -------------------------------------------------------------------- */
/* fpc1020 sensor commands and registers				*/
/* -------------------------------------------------------------------- */
typedef enum {
	FPC_1020_ERROR_REG_BIT_FIFO_UNDERFLOW = 1 << 0
} fpc1020_error_reg_t;



/* -------------------------------------------------------------------- */
/* global variables							*/
/* -------------------------------------------------------------------- */
static int fpc1020_device_count;


/* -------------------------------------------------------------------- */
/* fpc1020 data types							*/
/* -------------------------------------------------------------------- */
struct fpc1020_attribute {
	struct device_attribute attr;
	size_t offset;
};

enum {
	FPC1020_WORKER_IDLE_MODE = 0,
	FPC1020_WORKER_CAPTURE_MODE,
	FPC1020_WORKER_INPUT_MODE,
	FPC1020_WORKER_EXIT
};


/* -------------------------------------------------------------------- */
/* fpc1020 driver constants						*/
/* -------------------------------------------------------------------- */
#define FPC1020_CLASS_NAME                      "fpsensor"
#define FPC1020_WORKER_THREAD_NAME		"fpc1020_worker"
/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
static int __init fpc1020_init(void);

static void __exit fpc1020_exit(void);

static int  fpc1020_probe(struct spi_device *spi);

static int  fpc1020_remove(struct spi_device *spi);

static int fpc1020_suspend(struct device *dev);

static int fpc1020_resume(struct device *dev);

static int fpc1020_open(struct inode *inode, struct file *file);

static ssize_t fpc1020_write(struct file *file, const char *buff,
					size_t count, loff_t *ppos);

static ssize_t fpc1020_read(struct file *file, char *buff,
				size_t count, loff_t *ppos);

static int fpc1020_release(struct inode *inode, struct file *file);

static unsigned int fpc1020_poll(struct file *file, poll_table *wait);

static int fpc1020_cleanup(fpc1020_data_t *fpc1020, struct spi_device *spidev);

static int  fpc1020_param_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata);

static int  fpc1020_supply_init(fpc1020_data_t *fpc1020);

static int  fpc1020_reset_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata);

static int  fpc1020_irq_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata);

static int  fpc1020_spi_setup(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata);

static int fpc1020_worker_init(fpc1020_data_t *fpc1020);

static int  fpc1020_worker_destroy(fpc1020_data_t *fpc1020);

static int  fpc1020_get_of_pdata(struct device *dev,
					struct fpc1020_platform_data *pdata);

static int fpc1020_create_class(fpc1020_data_t *fpc1020);

static int  fpc1020_create_device(fpc1020_data_t *fpc1020);

static int fpc1020_manage_sysfs(fpc1020_data_t *fpc1020,
				struct spi_device *spi, bool create);

static void fpc1020_interrupt(void);

static ssize_t fpc1020_show_attr_setup(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t fpc1020_store_attr_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count);

static ssize_t fpc1020_show_attr_diag(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t fpc1020_store_attr_diag(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count);

static u8 fpc1020_selftest_short(fpc1020_data_t *fpc1020);

static int fpc1020_start_capture(fpc1020_data_t *fpc1020);

static int fpc1020_new_job(fpc1020_data_t *fpc1020, int new_job);

static int fpc1020_worker_goto_idle(fpc1020_data_t *fpc1020);

static int fpc1020_worker_function(void *_fpc1020);

static int fpc1020_start_input(fpc1020_data_t *fpc1020);


/* -------------------------------------------------------------------- */
/* External interface							*/
/* -------------------------------------------------------------------- */

extern struct fpc1020_platform_data fpc1020_pdata_mt65xx;

extern struct mt_chip_conf spi_conf_mt65xx;
static const struct dev_pm_ops fpc1020_pm = {
	.suspend = fpc1020_suspend,
	.resume = fpc1020_resume
};

#ifdef CONFIG_OF
static struct of_device_id fpc1020_of_match[] = {
	{ .compatible = "fpc,fpc1020", },
	{}
};

MODULE_DEVICE_TABLE(of, fpc1020_of_match);
#endif

static struct spi_driver fpc1020_driver = {
	.driver = {
		.name	= FPC1020_DEV_NAME,
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
		.pm     = &fpc1020_pm,
#ifdef CONFIG_OF
		.of_match_table = fpc1020_of_match,
#endif
	},
	.probe	= fpc1020_probe,
	.remove	= fpc1020_remove
};
static const struct file_operations fpc1020_fops = {
	.owner          = THIS_MODULE,
	.open           = fpc1020_open,
	.write          = fpc1020_write,
	.read           = fpc1020_read,
	.release        = fpc1020_release,
	.poll           = fpc1020_poll,
};


/* -------------------------------------------------------------------- */
/* devfs								*/
/* -------------------------------------------------------------------- */
#define FPC1020_ATTR(__grp, __field, __mode)				\
{									\
	.attr = __ATTR(__field, (__mode),				\
	fpc1020_show_attr_##__grp,					\
	fpc1020_store_attr_##__grp),					\
	.offset = offsetof(struct fpc1020_##__grp, __field)		\
}

#define FPC1020_DEV_ATTR(_grp, _field, _mode)				\
struct fpc1020_attribute fpc1020_attr_##_field =			\
					FPC1020_ATTR(_grp, _field, (_mode))

#define DEVFS_SETUP_MODE (S_IWUSR|S_IWGRP|S_IWOTH|S_IRUSR|S_IRGRP|S_IROTH)

static FPC1020_DEV_ATTR(setup, adc_gain,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, adc_shift,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_mode,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_count,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_settings_mux,	DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, pxl_ctrl,		DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_row_start,	DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_row_count,	DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_col_start,	DEVFS_SETUP_MODE);
static FPC1020_DEV_ATTR(setup, capture_col_groups,	DEVFS_SETUP_MODE);

static struct attribute *fpc1020_setup_attrs[] = {
	&fpc1020_attr_adc_gain.attr.attr,
	&fpc1020_attr_adc_shift.attr.attr,
	&fpc1020_attr_capture_mode.attr.attr,
	&fpc1020_attr_capture_count.attr.attr,
	&fpc1020_attr_capture_settings_mux.attr.attr,
	&fpc1020_attr_pxl_ctrl.attr.attr,
	&fpc1020_attr_capture_row_start.attr.attr,
	&fpc1020_attr_capture_row_count.attr.attr,
	&fpc1020_attr_capture_col_start.attr.attr,
	&fpc1020_attr_capture_col_groups.attr.attr,
	NULL
};

static const struct attribute_group fpc1020_setup_attr_group = {
	.attrs = fpc1020_setup_attrs,
	.name = "setup"
};

#define DEVFS_DIAG_MODE_RO (S_IRUSR|S_IRGRP|S_IROTH)
#define DEVFS_DIAG_MODE_RW (S_IWUSR|S_IWGRP|S_IWOTH|S_IRUSR|S_IRGRP|S_IROTH)

static FPC1020_DEV_ATTR(diag, chip_id,		DEVFS_DIAG_MODE_RO);
static FPC1020_DEV_ATTR(diag, selftest,		DEVFS_DIAG_MODE_RO);
static FPC1020_DEV_ATTR(diag, spi_register,	DEVFS_DIAG_MODE_RW);
static FPC1020_DEV_ATTR(diag, spi_regsize,	DEVFS_DIAG_MODE_RO);
static FPC1020_DEV_ATTR(diag, spi_data ,	DEVFS_DIAG_MODE_RW);
static FPC1020_DEV_ATTR(diag, last_capture_time,DEVFS_DIAG_MODE_RO);
static FPC1020_DEV_ATTR(diag, finger_present_status, DEVFS_DIAG_MODE_RO);


static struct attribute *fpc1020_diag_attrs[] = {
	&fpc1020_attr_chip_id.attr.attr,
	&fpc1020_attr_selftest.attr.attr,
	&fpc1020_attr_spi_register.attr.attr,
	&fpc1020_attr_spi_regsize.attr.attr,
	&fpc1020_attr_spi_data.attr.attr,
	&fpc1020_attr_last_capture_time.attr.attr,
	&fpc1020_attr_finger_present_status.attr.attr,
	NULL
};

static const struct attribute_group fpc1020_diag_attr_group = {
	.attrs = fpc1020_diag_attrs,
	.name = "diag"
};

static struct spi_board_info spi_board_devs[] __initdata = {
	[0] = {
		.modalias="fpc1020",
		.bus_num = 0,
		.chip_select=0,
		.mode = SPI_MODE_0,
		.max_speed_hz = 16000000,
	},
};
/* -------------------------------------------------------------------- */
/* SPI debug interface, prototypes					*/
/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_select(fpc1020_data_t *fpc1020,
				fpc1020_reg_t reg);

static int fpc1020_spi_debug_value_write(fpc1020_data_t *fpc1020, u64 data);

static int fpc1020_spi_debug_buffer_write(fpc1020_data_t *fpc1020,
					const char *data,
					size_t count);

static int fpc1020_spi_debug_value_read(fpc1020_data_t *fpc1020,
					u64 *data);

static int fpc1020_spi_debug_buffer_read(fpc1020_data_t *fpc1020,
					u8 *data,
					size_t max_count);

static void fpc1020_spi_debug_buffer_to_hex_string(char *string,
						u8 *buffer,
						size_t bytes);

static int fpc1020_spi_debug_hex_string_to_buffer(u8 *buffer,
						size_t buf_size,
						const char *string,
						size_t chars);

/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */



static int __init fpc1020_init(void)
{
        printk("<%s:%d>\n", __func__, __LINE__);
        spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
	if (spi_register_driver(&fpc1020_driver))
		return -EINVAL;

	return 0;
}


/* -------------------------------------------------------------------- */
static void __exit fpc1020_exit(void)
{
	printk("<%s:%d>\n", __func__, __LINE__);

	spi_unregister_driver(&fpc1020_driver);
}


fpc1020_data_t *g_fpc1020 = NULL;
/* -------------------------------------------------------------------- */
static int  fpc1020_probe(struct spi_device *spi)
{
	struct fpc1020_platform_data *fpc1020_pdata;
	struct fpc1020_platform_data pdata_of;
	struct device *dev = &spi->dev;
	int error = 0;
	fpc1020_data_t *fpc1020 = NULL;
	size_t buffer_size;
	printk("[FPC] fpc1020_probe +++\n");
	fpc1020 = kzalloc(sizeof(*fpc1020), GFP_KERNEL);
	if (!fpc1020) {
		dev_err(&spi->dev,
		"failed to allocate memory for struct fpc1020_data\n");

		return -ENOMEM;
	}

	printk(KERN_INFO "%s\n", __func__);

	buffer_size = fpc1020_calc_huge_buffer_minsize(fpc1020);
	error = fpc1020_manage_huge_buffer(fpc1020, buffer_size);
	if (error)
		goto err;

	spi_set_drvdata(spi, fpc1020);
	fpc1020->spi = spi;
        g_fpc1020 = fpc1020;
	fpc1020->reset_gpio = FPC1020_RESET_GPIO;
	fpc1020->irq_gpio   = FPC1020_IRQ_GPIO;
	fpc1020->cs_gpio    = -EINVAL;
	fpc1020->spi_freq_khz = 5000;
	fpc1020->irq        = FPC1020_INT_IRQNO;

	init_waitqueue_head(&fpc1020->wq_irq_return);

	error = fpc1020_init_capture(fpc1020);
	if (error)
		goto err;

	fpc1020_pdata = &fpc1020_pdata_mt65xx;//spi->dev.platform_data;

	if (!fpc1020_pdata) {
		error = fpc1020_get_of_pdata(dev, &pdata_of);
		fpc1020_pdata = &pdata_of;

		if (error)
			goto err;
	}

	if (!fpc1020_pdata) {
		dev_err(&fpc1020->spi->dev,
				"spi->dev.platform_data is NULL.\n");
		error = -EINVAL;
		goto err;
	}

	//hwPowerOn(MT6328_POWER_LDO_VEFUSE, VOL_2200, "FINGER");

	error = fpc1020_param_init(fpc1020, fpc1020_pdata);
	if (error)
		goto err;

	//error = fpc1020_supply_init(fpc1020);
	//if (error)
	//	goto err;

	error = fpc1020_reset_init(fpc1020, fpc1020_pdata);
	if (error)
		goto err;

	error = fpc1020_irq_init(fpc1020, fpc1020_pdata);
	if (error)
		goto err;

	error = fpc1020_spi_setup(fpc1020, fpc1020_pdata);
	if (error)
		goto err;

	error = fpc1020_reset(fpc1020);
	printk("[fpc] reset error=%d\n", error);
	if (error)
		goto err;

	error = fpc1020_check_hw_id(fpc1020);
	printk("[FPC] read hw id error=%d\n", error);
	if (error)
		goto err;

	buffer_size = fpc1020_calc_huge_buffer_minsize(fpc1020);
	error = fpc1020_manage_huge_buffer(fpc1020, buffer_size);
	if (error)
		goto err;

	error = fpc1020_setup_defaults(fpc1020);
	if (error)
		goto err;

	error = fpc1020_create_class(fpc1020);
	if (error)
		goto err;
	error = fpc1020_create_device(fpc1020);
	if (error)
		goto err;

	sema_init(&fpc1020->mutex, 0);
	error = fpc1020_manage_sysfs(fpc1020, spi, true);
	if (error)
		goto err;
	cdev_init(&fpc1020->cdev, &fpc1020_fops);
	fpc1020->cdev.owner = THIS_MODULE;

	error = cdev_add(&fpc1020->cdev, fpc1020->devno, 1);
	if (error) {
		printk("cdev_add failed.\n");
		goto err_chrdev;
	}

	error = fpc1020_worker_init(fpc1020);
	if (error)
		goto err_cdev;

	mt_eint_set_sens(fpc1020->irq, CUST_EINT_EDGE_SENSITIVE);
        mt_eint_set_polarity(fpc1020->irq, 1);
        mt_eint_set_hw_debounce(fpc1020->irq, 1);
        mt_eint_registration(fpc1020->irq, CUST_EINTF_TRIGGER_RISING, fpc1020_interrupt, 1);
        mt_eint_unmask(fpc1020->irq);


	error = fpc1020_calc_finger_detect_threshold_min(fpc1020);
	if (error < 0)
		goto err_cdev;

	error = fpc1020_set_finger_detect_threshold(fpc1020, error);
	if (error < 0)
		goto err_cdev;

	error = fpc1020_input_init(fpc1020);
	if (error)
		goto err_cdev;

	error = fpc1020_start_input(fpc1020);
	if (error)
		goto err_cdev;

//	error = fpc1020_sleep(fpc1020, 0);
//	if (error)
//		goto err_cdev;

	up(&fpc1020->mutex);

#if 0
	
        mt_eint_set_sens(fpc1020->irq, CUST_EINT_EDGE_SENSITIVE);
        mt_eint_set_polarity(fpc1020->irq, 1);
        mt_eint_set_hw_debounce(fpc1020->irq, 1);
        mt_eint_registration(fpc1020->irq, CUST_EINT_FPC_TYPE, fpc1020_interrupt, 1);
        mt_eint_unmask(fpc1020->irq);
#endif
	printk("[FPC_DBG] fpc1020 probe success\n");
	return 0;

err_cdev:
	cdev_del(&fpc1020->cdev);

err_chrdev:
	unregister_chrdev_region(fpc1020->devno, 1);

	fpc1020_manage_sysfs(fpc1020, spi, false);

err:
	fpc1020_cleanup(fpc1020, spi);
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_remove(struct spi_device *spi)
{
	fpc1020_data_t *fpc1020 = spi_get_drvdata(spi);

	printk(KERN_INFO "%s\n", __func__);

	fpc1020_manage_sysfs(fpc1020, spi, false);

	fpc1020_sleep(fpc1020, true);

	cdev_del(&fpc1020->cdev);

	unregister_chrdev_region(fpc1020->devno, 1);

	fpc1020_cleanup(fpc1020, spi);

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_suspend(struct device *dev)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020_worker_goto_idle(fpc1020);

	return fpc1020_sleep(fpc1020, true);
}


/* -------------------------------------------------------------------- */
static int fpc1020_resume(struct device *dev)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	if (fpc1020->input.enabled)
		fpc1020_start_input(fpc1020);

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_open(struct inode *inode, struct file *file)

{
	fpc1020_data_t *fpc1020;

	printk(KERN_INFO "%s\n", __func__);

	fpc1020 = container_of(inode->i_cdev, fpc1020_data_t, cdev);

	if (down_interruptible(&fpc1020->mutex))
		return -ERESTARTSYS;

	file->private_data = fpc1020;

	up(&fpc1020->mutex);

	return 0;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_write(struct file *file, const char *buff,
					size_t count, loff_t *ppos)
{
	printk(KERN_INFO "%s\n", __func__);

	return -ENOTTY;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_read(struct file *file, char *buff,
				size_t count, loff_t *ppos)
{
	fpc1020_data_t *fpc1020 = file->private_data;
	int error = 0;
	u32 max_data;
	u32 avail_data;

	if (down_interruptible(&fpc1020->mutex))
		return -ERESTARTSYS;

	if (fpc1020->capture.available_bytes > 0) {
		goto copy_data;
	} else {

		if (fpc1020->capture.read_pending_eof) {
			fpc1020->capture.read_pending_eof = false;
			error = 0;
			goto out;
		}

		if (file->f_flags & O_NONBLOCK) {
			if (fpc1020_capture_check_ready(fpc1020)) {
				error = fpc1020_start_capture(fpc1020);
				if (error)
					goto out;
			}

			error = -EWOULDBLOCK;
			goto out;

		} else {
			error = fpc1020_start_capture(fpc1020);
			if (error)
				goto out;
		}
	}

	error = wait_event_interruptible(
			fpc1020->capture.wq_data_avail,
			(fpc1020->capture.available_bytes > 0));

	if (error)
		goto out;

	if (fpc1020->capture.last_error != 0) {
		error = fpc1020->capture.last_error;
		goto out;
	}

copy_data:
	avail_data = fpc1020->capture.available_bytes;
	max_data = (count > avail_data) ? avail_data : count;

	if (max_data) {
		error = copy_to_user(buff,
			&fpc1020->huge_buffer[fpc1020->capture.read_offset],
			max_data);

		if (error)
			goto out;

		fpc1020->capture.read_offset += max_data;
		fpc1020->capture.available_bytes -= max_data;

		error = max_data;

		if (fpc1020->capture.available_bytes == 0)
			fpc1020->capture.read_pending_eof = true;
	}

out:
	up(&fpc1020->mutex);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_release(struct inode *inode, struct file *file)
{
	fpc1020_data_t *fpc1020 = file->private_data;
	int status = 0;

	printk(KERN_INFO "%s\n", __func__);

	if (down_interruptible(&fpc1020->mutex))
		return -ERESTARTSYS;

	fpc1020_start_input(fpc1020);
/*
	fpc1020_worker_goto_idle(fpc1020);

	fpc1020_sleep(fpc1020, true);
*/
	up(&fpc1020->mutex);

	return status;
}


/* -------------------------------------------------------------------- */
static unsigned int fpc1020_poll(struct file *file, poll_table *wait)
{
	fpc1020_data_t *fpc1020 = file->private_data;
	unsigned int ret = 0;
	fpc1020_capture_mode_t mode = fpc1020->setup.capture_mode;
	bool blocking_op;

	if (down_interruptible(&fpc1020->mutex))
		return -ERESTARTSYS;

	if (fpc1020->capture.available_bytes > 0)
		ret |= (POLLIN | POLLRDNORM);
	else if (fpc1020->capture.read_pending_eof)
		ret |= POLLHUP;
	else { /* available_bytes == 0 && !pending_eof */

		blocking_op =
			(mode == FPC1020_MODE_WAIT_AND_CAPTURE) ? true : false;

		switch (fpc1020->capture.state) {
		case FPC1020_CAPTURE_STATE_IDLE:
			if (!blocking_op)
				ret |= POLLIN;
			break;

		case FPC1020_CAPTURE_STATE_STARTED:
		case FPC1020_CAPTURE_STATE_PENDING:
		case FPC1020_CAPTURE_STATE_WRITE_SETTINGS:
		case FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_DOWN:
		case FPC1020_CAPTURE_STATE_ACQUIRE:
		case FPC1020_CAPTURE_STATE_FETCH:
		case FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_UP:
		case FPC1020_CAPTURE_STATE_COMPLETED:
			ret |= POLLIN;

			poll_wait(file, &fpc1020->capture.wq_data_avail, wait);

			if (fpc1020->capture.available_bytes > 0)
				ret |= POLLRDNORM;
			else if (blocking_op)
				ret = 0;

			break;

		case FPC1020_CAPTURE_STATE_FAILED:
			if (!blocking_op)
				ret |= POLLIN;
			break;

		default:
			dev_err(&fpc1020->spi->dev,
				"%s unknown state\n", __func__);
			break;
		}
	}

	up(&fpc1020->mutex);

	return ret;
}


/* -------------------------------------------------------------------- */
static int fpc1020_cleanup(fpc1020_data_t *fpc1020, struct spi_device *spidev)
{
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020_worker_destroy(fpc1020);

	if (!IS_ERR_OR_NULL(fpc1020->device))
		device_destroy(fpc1020->class, fpc1020->devno);

	class_destroy(fpc1020->class);

	//if (fpc1020->irq >= 0)
	//	free_irq(fpc1020->irq, fpc1020);

	//if (gpio_is_valid(fpc1020->irq_gpio))
	//	gpio_free(fpc1020->irq_gpio);

	//if (gpio_is_valid(fpc1020->reset_gpio))
	//	gpio_free(fpc1020->reset_gpio);

	//if (gpio_is_valid(fpc1020->cs_gpio))
	//	gpio_free(fpc1020->cs_gpio);

	fpc1020_manage_huge_buffer(fpc1020, 0);

	fpc1020_input_destroy(fpc1020);

	//fpc1020_regulator_release(fpc1020);

	kfree(fpc1020);

	spi_set_drvdata(spidev, NULL);

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_param_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata)
{
	printk("[FPC] fpc1020_param_init\n");
	if (pdata == NULL)
		printk("[FPC] fpc1020_param_init pdata is NULL\n");
	if (fpc1020 == NULL)
		return -1;
	if (pdata == NULL) {
		fpc1020->vddtx_mv    = pdata->external_supply_mv;
		fpc1020->txout_boost = pdata->txout_boost;

		if (fpc1020->vddtx_mv > 0) {
			dev_info(&fpc1020->spi->dev,
				"External TxOut supply (%d mV)\n",
				fpc1020->vddtx_mv);
		} else {
			dev_info(&fpc1020->spi->dev,
				"Internal TxOut supply (boost %s)\n",
				(fpc1020->txout_boost) ? "ON" : "OFF");
		}

	} else {
		fpc1020->vddtx_mv = 0;
		fpc1020->txout_boost = 0;
	}
	printk("[FPC] fpc1020_param_init ---\n");
	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_supply_init(fpc1020_data_t *fpc1020)
{
	int error = 0;
	return 0;
	error = fpc1020_regulator_configure(fpc1020);
	if (error) {
		dev_err(&fpc1020->spi->dev,
			"fpc1020_probe - regulator configuration failed.\n");
		goto err;
	}

	error = fpc1020_regulator_set(fpc1020, true);
	if (error) {
		dev_err(&fpc1020->spi->dev,
			"fpc1020_probe - regulator enable failed.\n");
		goto err;
	}

err:
	return error;
}


/* -------------------------------------------------------------------- */
static int  fpc1020_reset_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata)
{
	int error = 0;
	printk("[FPC] fpc1020_reset_init +++\n");
	if (pdata == NULL)
		printk("[FPC ERR]fpc1020_reset_init pdata NULL\n");
	if (fpc1020 == NULL) {
		printk("fpc1020_reset_init fpc1020 is NULL\n");
		return -1;
	}
        fpc1020->reset_gpio = FPC1020_RESET_GPIO;  
	//if (gpio_is_valid(pdata->reset_gpio)) 
        {

		printk("[FPC]Assign HW reset -> GPIO: 0x%x\n", fpc1020->reset_gpio);

		fpc1020->soft_reset_enabled = false;

		//error = gpio_request(pdata->reset_gpio, "fpc1020_reset");
		printk("[FPC] fpc1020_reset_init reset_gpio=0x%x\n", fpc1020->reset_gpio);
		error = mt_set_gpio_mode(FPC1020_RESET_GPIO, GPIO_MODE_00);
		printk("[FPC] fpc1020_reset_init set gpio mode end\n");
		if (error != RSUCCESS) {
			printk("[FPC]gpio_request (reset) failed.\n");
			return error;
		}

		//fpc1020->reset_gpio = pdata->reset_gpio;

		//error = gpio_direction_output(fpc1020->reset_gpio, 1);
		error = mt_set_gpio_dir(FPC1020_RESET_GPIO, GPIO_DIR_OUT);
		printk("[FPC] fpc1020_reset_init set gpio direction end\n");
		if (error != RSUCCESS) {
			dev_err(&fpc1020->spi->dev,
			"gpio_direction_output(reset) failed.\n");
			return error;
		}
	}// else {
		//dev_info(&fpc1020->spi->dev, "Using soft reset\n");

		//fpc1020->soft_reset_enabled = true;
	//}

	printk("[FPC] fpc1020_reset_init ---\n");
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_irq_init(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata)
{
	int error = 0;
	#if 0
	printk("[FPC] fpc1020_irq_init +++\n");
	if (fpc1020 == NULL)
		return -EINVAL;
	if (pdata == NULL)
		printk("[FPC] pdata is NULL\n");
	fpc1020->irq_gpio = FPC1020_IRQ_GPIO;
	//if (gpio_is_valid(pdata->irq_gpio)) 
        {

		printk("Assign IRQ -> GPIO 0x%x\n",
			fpc1020->irq_gpio);

		//error = gpio_request(pdata->irq_gpio, "fpc1020_irq");
		error = mt_set_gpio_mode(fpc1020->irq_gpio, GPIO_MODE_01);
		if (error != RSUCCESS) {
			dev_err(&fpc1020->spi->dev,
				"gpio_request (irq) failed.\n");

			return error;
		}

		//fpc1020->irq_gpio = pdata->irq_gpio;

		//error = gpio_direction_input(fpc1020->irq_gpio);
		error = mt_set_gpio_dir(fpc1020->irq_gpio, GPIO_DIR_IN);
		if (error != RSUCCESS) {
			dev_err(&fpc1020->spi->dev,
				"gpio_direction_input (irq) failed.\n");
			return error;
		}
		error = mt_set_gpio_pull_enable(fpc1020->irq_gpio, GPIO_PULL_DISABLE);
		if (error != RSUCCESS) {
			dev_err(&fpc1020->spi->dev,
				"fpc1020_probe - mt_set_gpio_pull_enable (irq) failed.\n");
			return error;
		}
	} //else {
		//return -EINVAL;
	//}

	//fpc1020->irq = gpio_to_irq(fpc1020->irq_gpio);
	fpc1020->irq = FPC1020_INT_IRQNO; //yongqiang comment, need debug
	if (fpc1020->irq < 0) {
		printk("gpio_to_irq failed.\n");
		error = fpc1020->irq;
		return error;
	}
	#endif
	fpc1020->spi->controller_data = (void*)&spi_conf_mt65xx;
#if 0
	error = request_irq(fpc1020->irq, fpc1020_interrupt,
			IRQF_TRIGGER_RISING, "fpc1020", fpc1020);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"request_irq %i failed.\n",
			fpc1020->irq);

		fpc1020->irq = -EINVAL;

		return error;
	}
#endif
#if 0
	mt_eint_set_sens(fpc1020->irq, CUST_EINT_EDGE_SENSITIVE);
	mt_eint_set_polarity(fpc1020->irq, 1);
	mt_eint_set_hw_debounce(fpc1020->irq, 1);
	mt_eint_registration(fpc1020->irq, CUST_EINT_FPC_TYPE, fpc1020_interrupt, 1);
	mt_eint_unmask(fpc1020->irq);
#endif
	printk("[FPC] fpc1020_irq_init ---\n");
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_setup(fpc1020_data_t *fpc1020,
					struct fpc1020_platform_data *pdata)
{
	int error = 0;

	printk(KERN_INFO "%s\n", __func__);
	if (fpc1020 == NULL)
		return -EINVAL;
	fpc1020->spi->mode = SPI_MODE_0;
	fpc1020->spi->bits_per_word = 8;

	error = spi_setup(fpc1020->spi);

	if (error) {
		printk("spi_setup failed\n");
		goto out_err;
	}
#if 0
	//if (gpio_is_valid(pdata->cs_gpio))
        {

		dev_info(&fpc1020->spi->dev,
			"Assign SPI.CS -> GPIO%d\n",
			pdata->cs_gpio);

		error = gpio_request(pdata->cs_gpio, "fpc1020_cs");
		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_request (cs) failed.\n");

			goto out_err;
		}

		fpc1020->cs_gpio = pdata->cs_gpio;

		error = gpio_direction_output(fpc1020->cs_gpio, 1);
		if (error) {
			dev_err(&fpc1020->spi->dev,
				"gpio_direction_output(cs) failed.\n");
			goto out_err;
		}
	} //else {
		//error = -EINVAL;
	//}
#endif
	printk("[FPC] fpc1020_spi_setup---\n");
out_err:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_worker_init(fpc1020_data_t *fpc1020)
{
	int error = 0;

	printk(KERN_INFO "%s\n", __func__);

	init_waitqueue_head(&fpc1020->worker.wq_wait_job);
	sema_init(&fpc1020->worker.sem_idle, 0);

	fpc1020->worker.req_mode = FPC1020_WORKER_IDLE_MODE;

	fpc1020->worker.thread = kthread_run(fpc1020_worker_function,
					   fpc1020, "%s",
					   FPC1020_WORKER_THREAD_NAME);

	if (IS_ERR(fpc1020->worker.thread)) {
		printk("kthread_run failed.\n");
		error = (int)PTR_ERR(fpc1020->worker.thread);
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int  fpc1020_worker_destroy(fpc1020_data_t *fpc1020)
{
	int error = 0;

	printk(KERN_INFO "%s\n", __func__);

	if (fpc1020->worker.thread) {
		fpc1020_worker_goto_idle(fpc1020);

		fpc1020->worker.req_mode = FPC1020_WORKER_EXIT;
		wake_up_interruptible(&fpc1020->worker.wq_wait_job);
		kthread_stop(fpc1020->worker.thread);
	}

	return error;
}


/* -------------------------------------------------------------------- */
#ifndef CONFIG_OF
static int  fpc1020_get_of_pdata(struct device *dev,
					struct fpc1020_platform_data *pdata)
{
	const struct device_node *node = dev->of_node;
	/* required properties */
	const void *irq_prop = of_get_property(node, "fpc,gpio_irq",   NULL);
	const void *rst_prop = of_get_property(node, "fpc,gpio_reset", NULL);
	const void *cs_prop  = of_get_property(node, "fpc,gpio_cs",    NULL);

	/* optional properties */
	const void *vddtx_prop = of_get_property(node, "fpc,vddtx_mv", NULL);
	const void *boost_prop =
			of_get_property(node, "fpc,txout_boost_enable", NULL);

	if (node == NULL) {
		dev_err(dev, "%s: Could not find OF device node\n", __func__);
		goto of_err;
	}

	if (!irq_prop || !rst_prop || !cs_prop) {
		dev_err(dev, "%s: Missing OF property\n", __func__);
		goto of_err;
	}

	pdata->irq_gpio   = be32_to_cpup(irq_prop);
	pdata->reset_gpio = be32_to_cpup(rst_prop);
	pdata->cs_gpio    = be32_to_cpup(cs_prop);

	pdata->external_supply_mv =
			(vddtx_prop != NULL) ? be32_to_cpup(vddtx_prop) : 0;

	pdata->txout_boost = (boost_prop != NULL) ? 1 : 0;

	return 0;

of_err:
	pdata->reset_gpio = -EINVAL;
	pdata->irq_gpio   = -EINVAL;
	pdata->cs_gpio    = -EINVAL;

	pdata->external_supply_mv = 0;
	pdata->txout_boost = 0;

	return -ENODEV;
}

#else
static int  fpc1020_get_of_pdata(struct device *dev,
					struct fpc1020_platform_data *pdata)
{
        pdata->reset_gpio = FPC1020_RESET_GPIO;
	pdata->irq_gpio   = FPC1020_IRQ_GPIO;
	pdata->cs_gpio    = -EINVAL;

	pdata->external_supply_mv = 0;
	pdata->txout_boost = 0;

	return -ENODEV;
}
#endif


/* -------------------------------------------------------------------- */
static int  fpc1020_create_class(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->class = class_create(THIS_MODULE, FPC1020_CLASS_NAME);

	if (IS_ERR(fpc1020->class)) {
		printk("failed to create class.\n");
		error = PTR_ERR(fpc1020->class);
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int  fpc1020_create_device(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

#if 0
	error = register_chrdev_region(fpc1020->devno, 1, FPC1020_DEV_NAME);

        if (error) {
                dev_err(&fpc1020->spi->dev,
                  "fpc1020_probe - register_chrdev_region failed.\n");

                goto out;
        }

#else
	if (FPC1020_MAJOR > 0) {
		fpc1020->devno = MKDEV(FPC1020_MAJOR, fpc1020_device_count++);

		error = register_chrdev_region(fpc1020->devno,
						1,
						FPC1020_DEV_NAME);
	} else {
		error = alloc_chrdev_region(&fpc1020->devno,
					fpc1020_device_count++,
					1,
					FPC1020_DEV_NAME);
	}

	if (error < 0) {
		goto out;

	} else {
		printk("[FPC_DBG]%s: major=%d, minor=%d\n",
						__func__,
						MAJOR(fpc1020->devno),
						MINOR(fpc1020->devno));
	}

	fpc1020->device = device_create(fpc1020->class, NULL, fpc1020->devno,
						NULL, "%s", FPC1020_DEV_NAME);

	if (IS_ERR(fpc1020->device)) {
		printk("[FPC_ERR] fpc1020 device_create failed.\n");
		error = PTR_ERR(fpc1020->device);
	}
#endif
out:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_manage_sysfs(fpc1020_data_t *fpc1020,
				struct spi_device *spi, bool create)
{
	int error = 0;

	if (create) {
		dev_dbg(&fpc1020->spi->dev, "%s create\n", __func__);

		error = sysfs_create_group(&spi->dev.kobj,
					&fpc1020_setup_attr_group);

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"sysf_create_group failed.\n");
			return error;
		}

		error = sysfs_create_group(&spi->dev.kobj,
					&fpc1020_diag_attr_group);

		if (error) {
			sysfs_remove_group(&spi->dev.kobj,
					&fpc1020_setup_attr_group);

			dev_err(&fpc1020->spi->dev,
				"sysf_create_group failed.\n");

			return error;
		}
	} else {
		dev_dbg(&fpc1020->spi->dev, "%s remove\n", __func__);

		sysfs_remove_group(&spi->dev.kobj, &fpc1020_setup_attr_group);
		sysfs_remove_group(&spi->dev.kobj, &fpc1020_diag_attr_group);
	}

	return error;
}


/* -------------------------------------------------------------------- */
static void fpc1020_interrupt(void)
{
	//fpc1020_data_t *fpc1020 = _fpc1020;
	printk("[FPC] interrupt\n");
	if (mt_get_gpio_in(g_fpc1020->irq_gpio)) {
		g_fpc1020->interrupt_done = true;
		wake_up_interruptible(&g_fpc1020->wq_irq_return);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_show_attr_setup(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);
	struct fpc1020_attribute *fpc_attr;
	int val = -1;
	int mux;

	fpc_attr = container_of(attr, struct fpc1020_attribute, attr);

	mux = fpc1020->setup.capture_settings_mux;

	if (fpc_attr->offset == offsetof(fpc1020_setup_t, adc_gain))
		val = fpc1020->setup.adc_gain[mux];

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, adc_shift))
		val = fpc1020->setup.adc_shift[mux];

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, pxl_ctrl))
		val = fpc1020->setup.pxl_ctrl[mux];

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_mode))
		val = fpc1020->setup.capture_mode;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_count))
		val = fpc1020->setup.capture_count;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_settings_mux))
		val = fpc1020->setup.capture_settings_mux;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_row_start))
		val = fpc1020->setup.capture_row_start;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_row_count))
		val = fpc1020->setup.capture_row_count;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_col_start))
		val = fpc1020->setup.capture_col_start;

	else if (fpc_attr->offset == offsetof(fpc1020_setup_t, capture_col_groups))
		val = fpc1020->setup.capture_col_groups;

	if (val >= 0)
		return scnprintf(buf, PAGE_SIZE, "%i\n", val);

	return -ENOENT;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_store_attr_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);
	u64 val;
	int error = kstrtou64(buf, 0, &val);
	int mux;
	int column_groups = fpc1020->chip.pixel_columns / fpc1020->chip.adc_group_size;

	struct fpc1020_attribute *fpc_attr;
	fpc_attr = container_of(attr, struct fpc1020_attribute, attr);

	mux = fpc1020->setup.capture_settings_mux;

	if (!error) {
		if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, adc_gain)) {

			fpc1020->setup.adc_gain[mux] = (u8)val;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, adc_shift)) {

			fpc1020->setup.adc_shift[mux] = (u8)val;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, pxl_ctrl)) {

			fpc1020->setup.pxl_ctrl[mux] = (u16)val;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_mode)) {

			fpc1020->setup.capture_mode =
					(fpc1020_capture_mode_t)val;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_count)) {

			if (fpc1020_check_in_range_u64
				(val, 1, FPC1020_BUFFER_MAX_IMAGES)) {

				fpc1020->setup.capture_count = (u8)val;
			} else
				return -EINVAL;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_settings_mux)) {

			if (fpc1020_check_in_range_u64
				(val, 0, (FPC1020_BUFFER_MAX_IMAGES - 1))) {

				fpc1020->setup.capture_settings_mux = (u8)val;
			} else
				return -EINVAL;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_row_start)) {

			if (fpc1020_check_in_range_u64
				(val, 0, (fpc1020->chip.pixel_rows - 1))) {

				fpc1020->setup.capture_row_start = (u8)val;
			} else
				return -EINVAL;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_row_count)) {

			if (fpc1020_check_in_range_u64
				(val, 1, fpc1020->chip.pixel_rows)) {

				fpc1020->setup.capture_row_count = (u8)val;
			} else
				return -EINVAL;

		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_col_start)) {

			if (fpc1020_check_in_range_u64
				(val, 0, (column_groups - 1))) {

				fpc1020->setup.capture_col_start = (u8)val;
			} else
				return -EINVAL;


		} else if (fpc_attr->offset ==
			offsetof(fpc1020_setup_t, capture_col_groups)) {

			if (fpc1020_check_in_range_u64
				(val, 1, column_groups)) {

				fpc1020->setup.capture_col_groups = (u8)val;
			} else
				return -EINVAL;

		} else
			return -ENOENT;

		return strnlen(buf, count);
	}
	return error;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_show_attr_diag(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	fpc1020_data_t *fpc1020;
	struct fpc1020_attribute *fpc_attr;
	u64 val;
	int error = 0;
	bool is_buffer = false;
	u8 u8_buffer[FPC1020_REG_MAX_SIZE];
	char hex_string[sizeof("0x") + (FPC1020_REG_MAX_SIZE * 2)];

	fpc1020 = dev_get_drvdata(dev);

	fpc_attr = container_of(attr, struct fpc1020_attribute, attr);

	switch (fpc_attr->offset) {
	case offsetof(fpc1020_diag_t, chip_id):
		return scnprintf(buf,
				PAGE_SIZE,
				"%s rev.%d\n",
				fpc1020_hw_id_text(fpc1020),
				fpc1020->chip.revision);
		break;
	case offsetof(fpc1020_diag_t, selftest):
		val = (u64)fpc1020_selftest_short(fpc1020);
		break;
	case offsetof(fpc1020_diag_t, spi_register):
		val = (int)fpc1020->diag.spi_register;
		break;
	case offsetof(fpc1020_diag_t, spi_regsize):
		val = (int)fpc1020->diag.spi_regsize;
		break;
	case offsetof(fpc1020_diag_t, spi_data):
		is_buffer = (fpc1020->diag.spi_regsize > sizeof(val));

		if (!is_buffer) {
			error = fpc1020_spi_debug_value_read(fpc1020, &val);
		} else {
			error = fpc1020_spi_debug_buffer_read(fpc1020,
							u8_buffer,
							sizeof(u8_buffer));
		}
		break;
	case offsetof(fpc1020_diag_t, last_capture_time):
		val = (int)fpc1020->diag.last_capture_time;
		break;
	case offsetof(fpc1020_diag_t, finger_present_status):
		error = fpc1020_get_finger_present_status(fpc1020);
		if (error >= 0) {
			val = (int)error;
			error = 0;
		}
		break;
	}

	if (error >= 0 && !is_buffer) {
		return scnprintf(buf,
				PAGE_SIZE,
				"%lu\n",
				(long unsigned int)val);
	}

	if (error >= 0 && is_buffer) {
		fpc1020_spi_debug_buffer_to_hex_string(hex_string,
						u8_buffer,
						fpc1020->diag.spi_regsize);

		return scnprintf(buf, PAGE_SIZE, "%s\n", hex_string);
	}

	return -ENOTTY;
}


/* -------------------------------------------------------------------- */
static ssize_t fpc1020_store_attr_diag(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	fpc1020_data_t *fpc1020 = dev_get_drvdata(dev);
	u64 val;
	int error = 0;

	struct fpc1020_attribute *fpc_attr;
	fpc_attr = container_of(attr, struct fpc1020_attribute, attr);

	if (fpc_attr->offset == offsetof(fpc1020_diag_t, spi_register)) {
		error = kstrtou64(buf, 0, &val);

		if (!error) {
			error = fpc1020_spi_debug_select(fpc1020,
							(fpc1020_reg_t)val);
		}
	} else if (fpc_attr->offset == offsetof(fpc1020_diag_t, spi_data)) {

		if (fpc1020->diag.spi_regsize <= sizeof(val)) {
			error = kstrtou64(buf, 0, &val);

			if (!error)
				error = fpc1020_spi_debug_value_write(fpc1020,
									 val);
		} else {
			error = fpc1020_spi_debug_buffer_write(fpc1020,
								buf,
								count);
		}
	} else
		error = -EPERM;

	return (error < 0) ? error : strnlen(buf, count);
}


/* -------------------------------------------------------------------- */
#ifdef  DEAD_PIXEL_TEST
int compare(const void *a, const void *b)
{
	int c = (int)(*(u8 *)a);
	int d = (int)(*(u8 *)b);

	if(c < d) return -1;            //-1: a < b
	else if (c == d) return 0;      // 0: a == b
	else return 1;                  // 1: a > b
}


/* -------------------------------------------------------------------- */
#define CB_TYPE1_MEDIAN_BOUNDARY_VALUE_UPPER_LIMIT 240
#define CB_TYPE1_MEDIAN_BOUNDARY_VALUE_LOWER_LIMIT 160

#define CB_TYPE2_MEDIAN_BOUNDARY_VALUE_UPPER_LIMIT 140
#define CB_TYPE2_MEDIAN_BOUNDARY_VALUE_LOWER_LIMIT 50

#define ICB_TYPE1_MEDIAN_BOUNDARY_VALUE_UPPER_LIMIT 210
#define ICB_TYPE1_MEDIAN_BOUNDARY_VALUE_LOWER_LIMIT 130

#define ICB_TYPE2_MEDIAN_BOUNDARY_VALUE_UPPER_LIMIT 80
#define ICB_TYPE2_MEDIAN_BOUNDARY_VALUE_LOWER_LIMIT 0

#define FPC102X_DEADPIXEL_THRESHOLD 11

static int fpc1020_check_for_deadPixels(fpc1020_data_t *fpc1020, u8* ripPixels, bool bCB)
{
	int deadpixels = 0;
	int x = 0, y = 0;
	int i1 = 0, i2 = 0;
	u8 *buff = fpc1020->huge_buffer;
	int pixel_columns = fpc1020->chip.pixel_columns;
	int pixel_rows = fpc1020->chip.pixel_rows;
	int buffersize = pixel_columns * pixel_rows;
	bool odd;
	int maxDev = 0;
	int dev1 = 0, dev2 = 0;
	int m1 = 0, m2 = 0;

	int config_max_deviation = 25;
	int config_min_checker_diff = 60;
	//int config_max_dead_pixels = 11;
	int config_max_dead_pixels_to_list = 11;

	int sum1 = 0;
	int sum2 = 0;
	int i = 0;

	u8 *p1 = (u8 *)kmalloc(sizeof(u8)*(buffersize/2), GFP_KERNEL);
	u8 *p2 = (u8 *)kmalloc(sizeof(u8)*(buffersize/2), GFP_KERNEL);
	memset(p1, 0, (buffersize/2));
	memset(p2, 0, (buffersize/2));


	//for (i = 0; i < FPC1020_FRAME_SIZE_MAX; i += 8)
	//	printk("%s: %2x %2x %2x %2x %2x %2x %2x %x\n", __func__,
	//		buff[i], buff[i+1], buff[i+2], buff[i+3], buff[i+4], buff[i+5], buff[i+6], buff[i+7]);

	for (y = 0; y < pixel_rows; y++) {
		for (x = 0; x < pixel_columns; x += 2) {
			odd = (y % 2) != 0;
			i1 = y * pixel_columns + x + ((odd) ? 1 : 0);
			i2 = y * pixel_columns + x + ((odd) ? 0 : 1);
			sum1 += buff[i1];
			sum2 += buff[i2];

			p1[i] = buff[i1];
			p2[i] = buff[i2];
			i++;
		}
	}

	sum1 /= (buffersize / 2);
	sum2 /= (buffersize / 2);

	sort((void *)p1, buffersize/2, sizeof(u8), &compare, NULL);
	sort((void *)p2, buffersize/2, sizeof(u8), &compare, NULL);

	m1 = ((int)p1[buffersize/4-1] + (int)p1[buffersize/4])/2;
	m2 = ((int)p2[buffersize/4-1] + (int)p2[buffersize/4])/2;

	kfree(p1);
	kfree(p2);

	dev_info(&fpc1020->spi->dev, 
		"%s: Average (median) pixel values = %d (%d) and %d (%d)\n", __func__, sum1, m1, sum2, m2);

	if (bCB) {
		if (m1 < CB_TYPE1_MEDIAN_BOUNDARY_VALUE_LOWER_LIMIT ||
			m1 > CB_TYPE1_MEDIAN_BOUNDARY_VALUE_UPPER_LIMIT) {
			printk("Type1 median is out of bound\n");
			return 1;
		}

		if (m2 < CB_TYPE2_MEDIAN_BOUNDARY_VALUE_LOWER_LIMIT ||
			m2 > CB_TYPE2_MEDIAN_BOUNDARY_VALUE_UPPER_LIMIT) {
			printk("Type2 median is out of bound\n");
			return 1;
		}

	} else {

		// Here m2 = median of type1 pattern, m1 = median of type2 pattern
		if (m2 < ICB_TYPE1_MEDIAN_BOUNDARY_VALUE_LOWER_LIMIT ||
			m2 > ICB_TYPE1_MEDIAN_BOUNDARY_VALUE_UPPER_LIMIT) {
			printk("Type1 median is out of bound\n");
			return 1;
		}

		if (m1 < ICB_TYPE2_MEDIAN_BOUNDARY_VALUE_LOWER_LIMIT ||
			m1 > ICB_TYPE2_MEDIAN_BOUNDARY_VALUE_UPPER_LIMIT) {
			printk("Type2 median is out of bound\n");
			return 1;
		}
	}

	if (abs(sum1 - sum2) < config_min_checker_diff) {
		printk("%s: Too small differance between white and black\n", __func__);
		return 1;
	}


	for (y = 0; y < pixel_rows; y++) {
		for (x = 0; x < pixel_columns; x += 2) {
			odd = (y % 2) != 0;
			i1 = y * pixel_columns + x + ((odd) ? 1 : 0);
			i2 = y * pixel_columns + x + ((odd) ? 0 : 1);

			dev1 = abs(sum1 - (int)buff[i1]);
			dev2 = abs(sum2 - (int)buff[i2]);

			if (dev1 > maxDev) maxDev = dev1;
			if (dev2 > maxDev) maxDev = dev2;

			if (dev1 > config_max_deviation) {
				if (deadpixels < config_max_dead_pixels_to_list) {
					dev_err(&fpc1020->spi->dev,
						"%s: Dead pixel found @ ImgXY[%d, %d]\n", __func__, (x + ((odd) ? 1 : 0)), y);
				}
				ripPixels[i1] = 1;
				deadpixels++;
			}
			if (dev2 > config_max_deviation) {
				if (deadpixels < config_max_dead_pixels_to_list) {
					dev_err(&fpc1020->spi->dev,
						"%s: Dead pixel found @ imgXY[%d, %d]\n", __func__, (x + ((odd) ? 0 : 1)), y);
				}
				ripPixels[i2] = 1;
				deadpixels++;
			}
		}
	}

	if (deadpixels > config_max_dead_pixels_to_list) {
		dev_err(&fpc1020->spi->dev,
			"%s: More dead pixels found... Not listing all, deadpixel num =%d .n", __func__,deadpixels);
		return deadpixels;
	}

	if (deadpixels == 0) {
		printk("%s: Found no dead pixels, highest deviation = %d\n", __func__, maxDev);
	}

	return deadpixels > FPC102X_DEADPIXEL_THRESHOLD ? 1 : 0;
}


/* -------------------------------------------------------------------- */
static int CheckDeadPixelInDetectZone(fpc1020_data_t *fpc1020, int index)
{
	int xpos = 0, ypos = 0;
	int xp_1020[] = { 16, 64, 120, 168 }, yp_1020[] = { 28, 92, 156 };
	int xp_1021[] = { 16, 56, 88, 128 }, yp_1021[] = { 20, 76, 132 };
	int error = -1;
	
	int* xp;
	int* yp;
	int x = 0, y = 0;
	
	if (fpc1020->chip.type == FPC1020_CHIP_1021A) {

		ypos = index / 192;
		xpos = index % 192;
		xp = xp_1020;
		yp = yp_1020;

	} else if (fpc1020->chip.type == FPC1020_CHIP_1021A || 
			fpc1020->chip.type == FPC1020_CHIP_1021B) {

		ypos = index / 160;
		xpos = index % 160;
		xp = xp_1021;
		yp = yp_1021;

	} else {

		return 0;

	}
	
	
	for (x = 0; x < 4; x++) {
		for (y = 0; y < 3; y++) {
			//<=  8
			if (xpos >= xp[x] && xpos < (xp[x] + 8) && 
				ypos >= yp[y] && ypos < (yp[y] + 8)) {
				return error;
			}
		}
	}
	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_test_deadpixels(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8* ripPixels = NULL;
	int pixel_columns = fpc1020->chip.pixel_columns;
	int pixel_rows = fpc1020->chip.pixel_rows;
	int buffersize = pixel_columns * pixel_rows;
	int i = 0;

	// Checkerboard Test
	fpc1020->setup.capture_mode = FPC1020_MODE_CHECKERBOARD_TEST_NORM;
	error = fpc1020_start_capture(fpc1020);
	if (error)
		goto out;

	error = wait_event_interruptible(
			fpc1020->capture.wq_data_avail,
			(fpc1020->capture.available_bytes > 0));

	if (error)
		goto out;

	if (fpc1020->capture.last_error != 0) {
		error = fpc1020->capture.last_error;
		goto out;
	}

	ripPixels = (u8 *)kmalloc(sizeof(u8)*buffersize, GFP_KERNEL);
	memset(ripPixels, 0, buffersize);
	
	error = fpc1020_check_for_deadPixels(fpc1020, ripPixels, true);
	if (error)
		goto out;

	for (i = 0; i < buffersize; i++) {
		if (ripPixels[i] != 0) {
			if ((error = CheckDeadPixelInDetectZone(fpc1020, i))) {
				// At least one pixel is in the not-allowed finger detect zone
				printk("Dead pixel found in finger detect zone\n");
				goto out;
			}
		}
	}
		
	// INV Checkerboard Test
	fpc1020->setup.capture_mode = FPC1020_MODE_CHECKERBOARD_TEST_INV;
	error = fpc1020_start_capture(fpc1020);
	if (error)
		goto out;

	error = wait_event_interruptible(
			fpc1020->capture.wq_data_avail,
			(fpc1020->capture.available_bytes > 0));

	if (error)
		goto out;

	if (fpc1020->capture.last_error != 0) {
		error = fpc1020->capture.last_error;
		goto out;
	}

	memset(ripPixels, 0, buffersize);

	error = fpc1020_check_for_deadPixels(fpc1020, ripPixels, false);
	if (error)
		goto out;

	for (i = 0; i < buffersize; i++) {
		if (ripPixels[i] != 0) {
			if ((error = CheckDeadPixelInDetectZone(fpc1020, i))) {
				// At least one pixel is in the not-allowed finger detect zone
				printk("Dead pixel found in finger detect zone\n");
				goto out;
			}
		}
	}
		
out:
	kfree(ripPixels);
	//up(&fpc1020->mutex);
	if (!error)
		dev_info(&fpc1020->spi->dev, "%s: PASS\n", __func__);

	return error;
}

#endif
/* -------------------------------------------------------------------- */
static u8 fpc1020_selftest_short(fpc1020_data_t *fpc1020)
{
	const char *id_str = "selftest,";
	int error = 0;

	bool resume_input = false;
	printk("linson input = %d\n",fpc1020->input.enabled);

	if(fpc1020->input.enabled) {
		resume_input = true;
		fpc1020_worker_goto_idle(fpc1020);
	}

	if(!fpc1020->input.enabled) {
		resume_input = true;
		fpc1020_worker_goto_idle(fpc1020);
	}

	fpc1020->diag.selftest = 0;

	error = fpc1020_wake_up(fpc1020);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s wake up fail on entry.\n", id_str);
		goto out;
	}

	error = fpc1020_reset(fpc1020);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s reset fail on entry.\n", id_str);
		goto out;
	}

	error = fpc1020_check_hw_id(fpc1020);

	if (error)
		goto out;

	error = fpc1020_cmd(fpc1020, FPC1020_CMD_CAPTURE_IMAGE, false);

	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s capture command failed.\n", id_str);
		goto out;
	}

	error = mt_get_gpio_in(fpc1020->irq_gpio) ? 0 : -EIO;

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s IRQ not HIGH after capture.\n", id_str);
		goto out;
	}

	error = fpc1020_wait_for_irq(fpc1020, FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s IRQ-wait after capture failed.\n", id_str);
		goto out;
	}

	error = fpc1020_read_irq(fpc1020, true);

	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s IRQ clear fail\n", id_str);
		goto out;
	} else
		error = 0;

	error = (mt_get_gpio_in(fpc1020->irq_gpio) == 0) ? 0 : -EIO;

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s IRQ not LOW after clear.\n", id_str);
		goto out;
	}

	error = fpc1020_reset(fpc1020);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s reset fail on exit.\n", id_str);
		goto out;
	}

	error = fpc1020_read_status_reg(fpc1020);

	dev_err(&fpc1020->spi->dev,
			"fpc status reg :%d \n", error);

	if (error != FPC1020_STATUS_REG_RESET_VALUE)  {
		dev_err(&fpc1020->spi->dev,
			 "%s status check fail on exit.\n", id_str);
		//goto out;
	}
	
#ifdef  DEAD_PIXEL_TEST
	error = fpc1020_test_deadpixels(fpc1020);
	if (error){
		dev_err(&fpc1020->spi->dev,
			"%s fpc1020_test_deadpixels failed.\n", id_str);
		goto out;
	}
#endif
	error = 0;

out:
	fpc1020->diag.selftest = (error == 0) ? 1 : 0;

	dev_info(&fpc1020->spi->dev, "%s %s\n", id_str,
				(fpc1020->diag.selftest) ? "PASS" : "FAIL");

	if (resume_input && fpc1020->diag.selftest)
		fpc1020_start_input(fpc1020);

	return fpc1020->diag.selftest;
};


/* -------------------------------------------------------------------- */
static int fpc1020_start_capture(fpc1020_data_t *fpc1020)
{
	fpc1020_capture_mode_t mode = fpc1020->setup.capture_mode;
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s mode= %d\n", __func__, mode);

	/* Mode check (and pre-conditions if required) ? */
	switch (mode) {
	case FPC1020_MODE_WAIT_AND_CAPTURE:
	case FPC1020_MODE_SINGLE_CAPTURE:
	case FPC1020_MODE_CHECKERBOARD_TEST_NORM:
	case FPC1020_MODE_CHECKERBOARD_TEST_INV:
	case FPC1020_MODE_BOARD_TEST_ONE:
	case FPC1020_MODE_BOARD_TEST_ZERO:
	case FPC1020_MODE_WAIT_FINGER_DOWN:
	case FPC1020_MODE_WAIT_FINGER_UP:
	case FPC1020_MODE_SINGLE_CAPTURE_CAL:
	case FPC1020_MODE_CAPTURE_AND_WAIT_FINGER_UP:
		break;

	case FPC1020_MODE_IDLE:
	default:
		error = -EINVAL;
		break;
	}

	fpc1020->capture.current_mode = (error >= 0) ? mode : FPC1020_MODE_IDLE;

	fpc1020->capture.state = FPC1020_CAPTURE_STATE_STARTED;
	fpc1020->capture.available_bytes  = 0;
	fpc1020->capture.read_offset = 0;
	fpc1020->capture.read_pending_eof = false;

	fpc1020_new_job(fpc1020, FPC1020_WORKER_CAPTURE_MODE);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_worker_goto_idle(fpc1020_data_t *fpc1020)
{
	const int wait_idle_us = 100;

	if (down_trylock(&fpc1020->worker.sem_idle)) {
		dev_dbg(&fpc1020->spi->dev, "%s, stop_request\n", __func__);
		printk("%s, stop_request\n", __func__);

		fpc1020->worker.stop_request = true;
		fpc1020->worker.req_mode = FPC1020_WORKER_IDLE_MODE;

		while (down_trylock(&fpc1020->worker.sem_idle))	{

			fpc1020->worker.stop_request = true;
			fpc1020->worker.req_mode = FPC1020_WORKER_IDLE_MODE;

			udelay(wait_idle_us);
		}
		dev_dbg(&fpc1020->spi->dev, "%s, is idle\n", __func__);
		printk("%s, is idle\n", __func__);
		up(&fpc1020->worker.sem_idle);

	} else {
		dev_dbg(&fpc1020->spi->dev, "%s, already idle\n", __func__);
		printk("%s, already idle\n", __func__);
		up(&fpc1020->worker.sem_idle);
	}

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_new_job(fpc1020_data_t *fpc1020, int new_job)
{
	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, new_job);

	fpc1020_worker_goto_idle(fpc1020);

	fpc1020->worker.req_mode = new_job;
	fpc1020->worker.stop_request = false;

	wake_up_interruptible(&fpc1020->worker.wq_wait_job);

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_worker_function(void *_fpc1020)
{
	fpc1020_data_t *fpc1020 = _fpc1020;

	while (!kthread_should_stop()) {

		up(&fpc1020->worker.sem_idle);

		wait_event_interruptible(fpc1020->worker.wq_wait_job,
			fpc1020->worker.req_mode != FPC1020_WORKER_IDLE_MODE);

		down(&fpc1020->worker.sem_idle);

		switch (fpc1020->worker.req_mode) {
		case FPC1020_WORKER_CAPTURE_MODE:
			fpc1020->capture.state = FPC1020_CAPTURE_STATE_PENDING;
			fpc1020_capture_task(fpc1020);
			break;

		case FPC1020_WORKER_INPUT_MODE:
			if (fpc1020_capture_deferred_task(fpc1020) != -EINTR) {
				fpc1020_input_enable(fpc1020, true);
				fpc1020_input_task(fpc1020);
			}
			break;

		case FPC1020_WORKER_IDLE_MODE:
		case FPC1020_WORKER_EXIT:
		default:
			break;
		}

		if (fpc1020->worker.req_mode != FPC1020_WORKER_EXIT)
			fpc1020->worker.req_mode = FPC1020_WORKER_IDLE_MODE;
	}

	return 0;
}


/* -------------------------------------------------------------------- */
/* SPI debug interface, implementation					*/
/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_select(fpc1020_data_t *fpc1020, fpc1020_reg_t reg)
{
	u8 size = FPC1020_REG_SIZE(reg);

	if (size) {
		fpc1020->diag.spi_register = reg;
		fpc1020->diag.spi_regsize  = size;

		dev_dbg(&fpc1020->spi->dev, "%s : selected %d (%d byte(s))\n",
						 __func__
						, fpc1020->diag.spi_register
						, fpc1020->diag.spi_regsize);
		return 0;
	} else {
		dev_dbg(&fpc1020->spi->dev,
			"%s : reg %d not available\n", __func__, reg);

		return -ENOENT;
	}
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_value_write(fpc1020_data_t *fpc1020, u64 data)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	FPC1020_MK_REG_WRITE_BYTES(reg,
				fpc1020->diag.spi_register,
				fpc1020->diag.spi_regsize,
				(u8 *)&data);

	error = fpc1020_reg_access(fpc1020, &reg);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_buffer_write(fpc1020_data_t *fpc1020,
						const char *data, size_t count)
{
	int error = 0;
	fpc1020_reg_access_t reg;
	u8 u8_buffer[FPC1020_REG_MAX_SIZE];

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_spi_debug_hex_string_to_buffer(u8_buffer,
						sizeof(u8_buffer),
						data,
						count);

	if (error < 0)
		return error;

	FPC1020_MK_REG_WRITE_BYTES(reg,
				fpc1020->diag.spi_register,
				fpc1020->diag.spi_regsize,
				u8_buffer);

	error = fpc1020_reg_access(fpc1020, &reg);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_value_read(fpc1020_data_t *fpc1020, u64 *data)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	*data = 0;

	FPC1020_MK_REG_READ_BYTES(reg,
				fpc1020->diag.spi_register,
				fpc1020->diag.spi_regsize,
				(u8 *)data);

	error = fpc1020_reg_access(fpc1020, &reg);

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_buffer_read(fpc1020_data_t *fpc1020,
						u8 *data, size_t max_count)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	if (max_count < fpc1020->diag.spi_regsize)
		return -ENOMEM;

	FPC1020_MK_REG_READ_BYTES(reg,
				fpc1020->diag.spi_register,
				fpc1020->diag.spi_regsize,
				data);

	error = fpc1020_reg_access(fpc1020, &reg);

	return error;
}


/* -------------------------------------------------------------------- */
static void fpc1020_spi_debug_buffer_to_hex_string(char *string,
						u8 *buffer,
						size_t bytes)
{
	int count = bytes;
	int pos = 0;
	int src = (target_little_endian) ? (bytes - 1) : 0;
	u8 v1, v2;

	string[pos++] = '0';
	string[pos++] = 'x';

	while (count) {
		v1 = buffer[src] >> 4;
		v2 = buffer[src] & 0x0f;

		string[pos++] = (v1 >= 0x0a) ? ('a' - 0x0a + v1) : ('0' + v1);
		string[pos++] = (v2 >= 0x0a) ? ('a' - 0x0a + v2) : ('0' + v2);

		src += (target_little_endian) ? -1 : 1;

		count--;
	}

	string[pos] = '\0';
}


/* -------------------------------------------------------------------- */
static u8 fpc1020_char_to_u8(char in_char)
{
	if ((in_char >= 'A') && (in_char <= 'F'))
		return (u8)(in_char - 'A' + 0xa);

	if ((in_char >= 'a') && (in_char <= 'f'))
		return (u8)(in_char - 'a' + 0xa);

	if ((in_char >= '0') && (in_char <= '9'))
		return (u8)(in_char - '0');

	return 0;
}


/* -------------------------------------------------------------------- */
static int fpc1020_spi_debug_hex_string_to_buffer(u8 *buffer,
						size_t buf_size,
						const char *string,
						size_t chars)
{
	int bytes = 0;
	int count;
	int dst = (target_little_endian) ? 0 : (buf_size - 1);
	int pos;
	u8 v1, v2;

	if (string[1] != 'x' && string[1] != 'X')
		return -EINVAL;

	if (string[0] != '0')
		return -EINVAL;

	if (chars < sizeof("0x1"))
		return -EINVAL;

	count = buf_size;
	while (count)
		buffer[--count] = 0;

	count = chars - sizeof("0x");

	bytes = ((count % 2) == 0) ? (count / 2) : (count / 2) + 1;

	if (bytes > buf_size)
		return -EINVAL;

	pos = chars - 2;

	while (pos >= 2) {
		v1 = fpc1020_char_to_u8(string[pos--]);
		v2 = (pos >= 2) ? fpc1020_char_to_u8(string[pos--]) : 0;

		buffer[dst] = (v2 << 4) | v1;

		dst += (target_little_endian) ? 1 : -1;
	}
	return bytes;
}


/* -------------------------------------------------------------------- */
static int fpc1020_start_input(fpc1020_data_t *fpc1020)
{
	return fpc1020_new_job(fpc1020, FPC1020_WORKER_INPUT_MODE);
}


/* -------------------------------------------------------------------- */

module_init(fpc1020_init);
module_exit(fpc1020_exit);

MODULE_DESCRIPTION("Texas Instruments FPC"); 
MODULE_AUTHOR("Daniel Jeong <daniel.jeong@ti.com>"); 
MODULE_LICENSE("GPL v2"); 

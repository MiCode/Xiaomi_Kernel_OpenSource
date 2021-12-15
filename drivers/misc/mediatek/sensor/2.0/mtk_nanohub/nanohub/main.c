// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/rt.h>

#include "main.h"
#include "comms.h"
#include "nanohub-mtk.h"
#include "nanohub.h"

#define WAKEUP_TIMEOUT_MS 1000

static struct nanohub_data *g_nanohub_data_p;

int nanohub_wait_for_interrupt(struct nanohub_data *data)
{
	return 0;
}

int nanohub_wakeup_eom(struct nanohub_data *data, bool repeat)
{
	return 0;
}

static void __nanohub_interrupt_cfg(struct nanohub_data *data,
				    u8 interrupt, bool mask)
{
	int ret;
	u8 mask_ret = 0;
	int cnt = 10;
	int cmd = mask ? CMD_COMMS_MASK_INTR : CMD_COMMS_UNMASK_INTR;

	do {
		ret = request_wakeup_timeout(data, WAKEUP_TIMEOUT_MS);
		if (ret)
			return;

		ret =
		    nanohub_comms_tx_rx_retrans(data, cmd,
						&interrupt, sizeof(interrupt),
						&mask_ret, sizeof(mask_ret),
						false, 10, 0);
		release_wakeup(data);
	} while ((ret != 1 || mask_ret != 1) && --cnt > 0);
}

static inline void nanohub_mask_interrupt(struct nanohub_data *data,
					  u8 interrupt)
{
	__nanohub_interrupt_cfg(data, interrupt, true);
}

static inline void nanohub_unmask_interrupt(struct nanohub_data *data,
					    u8 interrupt)
{
	__nanohub_interrupt_cfg(data, interrupt, false);
}

ssize_t nanohub_external_write(const char *buffer, size_t length)
{
	struct nanohub_data *data = g_nanohub_data_p;
	int ret;
	u8 ret_data;

	if (request_wakeup(data))
		return -ERESTARTSYS;

	if (nanohub_comms_tx_rx_retrans
		(data, CMD_COMMS_WRITE, buffer, length, &ret_data,
		sizeof(ret_data), false,
		10, 10) == sizeof(ret_data)) {
		if (ret_data)
			ret = length;
		else
			ret = 0;
	} else {
		ret = ERROR_NACK;
	}

	release_wakeup(data);

	return ret;
}

struct nanohub_device *
nanohub_probe(struct device *dev, struct nanohub_device *nano_dev)
{
	/* const struct nanohub_platform_data *pdata;*/
	struct nanohub_data *data;

	data = nano_dev->drv_data;
	g_nanohub_data_p = data;
	data->nanohub_dev = nano_dev;
	/* data->pdata = pdata; */
	data->pdata = devm_kzalloc(dev, sizeof(struct nanohub_platform_data),
		GFP_KERNEL);

	mutex_init(&data->comms_lock);

	usleep_range(25, 30);

	return nano_dev;
}

int nanohub_suspend(struct nanohub_device *nano_dev)
{
	struct nanohub_data *data = nano_dev->drv_data;

	nanohub_mask_interrupt(data, 2);
	return 0;
}

int nanohub_resume(struct nanohub_device *nano_dev)
{
	struct nanohub_data *data = nano_dev->drv_data;

	nanohub_unmask_interrupt(data, 2);
	return 0;
}

int nanohub_init(void)
{
	int ret = 0;

#ifdef CONFIG_NANOHUB_I2C
	if (ret == 0)
		ret = nanohub_i2c_init();
#endif
#ifdef CONFIG_NANOHUB_SPI
	if (ret == 0)
		ret = nanohub_spi_init();
#endif
#ifdef CONFIG_NANOHUB_MTK_IPI
		ret = nanohub_ipi_init();
#endif
	return ret;
}

MODULE_AUTHOR("Ben Fennema");
MODULE_LICENSE("GPL");

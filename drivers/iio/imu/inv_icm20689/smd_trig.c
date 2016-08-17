/*
* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <soc/qcom/smd.h>

#include "inv_icm20689_iio.h"

#if INV20689_SMD_IRQ_TRIGGER

#define TS_IMU_PORT_NAME        "imu_timestamp"
#define TS_CHANNEL_TYPE         SMD_APPS_MODEM

static smd_channel_t *imu_chan;
static struct completion work;
struct iio_trigger *inv_trig = NULL;

/* event handler of imu, called in smd_irq */
static void imu_smd_handler(void *priv, unsigned event)
{
	uint64_t patten;

	switch (event) {
	case SMD_EVENT_OPEN:
		complete(&work);
		break;
	case SMD_EVENT_CLOSE:
		break;
	case SMD_EVENT_DATA:
		/* need to read data otherwise it would stop*/
		smd_read_from_cb(imu_chan, &patten, sizeof(patten));
		if (inv_trig)
			iio_trigger_poll(inv_trig);
		break;
	}
}

int imu_ts_smd_channel_init(void)
{
	int ret;

	init_completion(&work);
	ret = smd_named_open_on_edge(TS_IMU_PORT_NAME, TS_CHANNEL_TYPE,
			&imu_chan, NULL, imu_smd_handler);
	if (ret) {
		pr_err("open channel %s failed\n", TS_IMU_PORT_NAME);
		return ret;
	}

	ret = wait_for_completion_timeout(&work, 5*HZ);
	if (!ret) {
		pr_err("%s wait for completion failed\n", TS_IMU_PORT_NAME);
		smd_close(imu_chan);
		return ret;
	}
	return 0;
}

void imu_ts_smd_channel_close(void)
{
	smd_close(imu_chan);
	imu_chan = NULL;
}

#endif

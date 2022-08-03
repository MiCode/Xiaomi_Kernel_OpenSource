/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef SLATERSB_H
#define SLATERSB_H
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include "slatersb_rpmsg.h"

#define	SLATERSB_GLINK_INTENT_SIZE	0x04
#define SLATERSB_MSG_SIZE 0x08
#define TIMEOUT_MS 2000

#define SLATERSB_SLATE_SUBSYS "slatefw"

#define	SLATERSB_POWER_DISABLE	0
#define	SLATERSB_POWER_ENABLE	1
#define	SLATERSB_POWER_CALIBRATION	2
#define	SLATERSB_BTTN_CONFIGURE	5
#define	SLATERSB_IN_TWM	8
#define	SLATERSB_OUT_TWM	9

enum slatersb_state {
	SLATERSB_STATE_UNKNOWN,
	SLATERSB_STATE_INIT,
	SLATERSB_STATE_RSB_CONFIGURED,
	SLATERSB_STATE_RSB_ENABLED
};

enum slate_rsb {
	SLATERSB_CONFIGR_RSB = 1,
	SLATERSB_ENABLE,
	SLATERSB_CALIBRATION_RESOLUTION,
	SLATERSB_CALIBRATION_INTERVAL,
	SLATERSB_BUTTN_CONFIGRATION
};

struct slatersb_msg {
	uint32_t cmd_id;
	uint32_t data;
};

void slatersb_notify_glink_channel_state(bool state);
void slatersb_rx_msg(void *data, int len);
#endif /* SLATECOM_H */

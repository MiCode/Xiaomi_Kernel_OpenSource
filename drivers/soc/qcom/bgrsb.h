/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#ifndef BGRSB_H
#define BGRSB_H
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include "bgrsb_rpmsg.h"
struct event {
	uint8_t sub_id;
	int16_t evnt_data;
	uint32_t evnt_tm;
};

#define	BGRSB_GLINK_INTENT_SIZE	0x04
#define BGRSB_MSG_SIZE 0x08
#define TIMEOUT_MS 2000
#define	BGRSB_LDO15_VTG_MIN_UV	3000000
#define BGRSB_LDO15_VTG_MAX_UV	3000000
#define	BGRSB_LDO11_VTG_MIN_UV	1800000
#define	BGRSB_LDO11_VTG_MAX_UV	1800000

#define BGRSB_BGWEAR_SUBSYS "bg-wear"

#define	BGRSB_POWER_DISABLE	0
#define	BGRSB_POWER_ENABLE	1
#define	BGRSB_POWER_CALIBRATION	2
#define	BGRSB_BTTN_CONFIGURE	5
#define	BGRSB_IN_TWM	8
#define	BGRSB_OUT_TWM	9


struct bgrsb_regulator {
	struct regulator *regldo11;
	struct regulator *regldo15;
};

enum ldo_task {
	BGRSB_HW_TURN_ON,
	BGRSB_ENABLE_WHEEL_EVENTS,
	BGRSB_HW_TURN_OFF,
	BGRSB_DISABLE_WHEEL_EVENTS,
	BGRSB_NO_ACTION
};

enum bgrsb_state {
	BGRSB_STATE_UNKNOWN,
	BGRSB_STATE_INIT,
	BGRSB_STATE_LDO11_ENABLED,
	BGRSB_STATE_RSB_CONFIGURED,
	BGRSB_STATE_RSB_ENABLED
};

struct bgrsb_msg {
	uint32_t cmd_id;
	uint32_t data;
};

void bgrsb_send_input(struct event *evnt);
void bgrsb_notify_glink_channel_state(bool state);
void bgrsb_rx_msg(void *data, int len);
#endif /* BGCOM_H */

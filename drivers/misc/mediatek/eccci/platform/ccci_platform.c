// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"

#ifdef FEATURE_USING_4G_MEMORY_API
#include <mt-plat/mtk_lpae.h>
#endif

#define TAG "plat"

#ifdef FEATURE_LOW_BATTERY_SUPPORT
static int ccci_md_low_power_notify(
	struct ccci_modem *md, enum LOW_POEWR_NOTIFY_TYPE type, int level)
{
	unsigned int md_throttle_cmd = 0;
	int ret = 0;

	CCCI_NORMAL_LOG(md->index, TAG,
		"low power notification type=%d, level=%d\n", type, level);

	switch (type) {
	case LOW_BATTERY:
		if (level <= LOW_BATTERY_LEVEL_2 &&
			level >= LOW_BATTERY_LEVEL_0) {
			md_throttle_cmd = TMC_CTRL_CMD_TX_POWER |
			LOW_BATTERY << 8 | level << 16;
		}
		break;
	case OVER_CURRENT:
		if (level <= BATTERY_OC_LEVEL_1 &&
			level >= BATTERY_OC_LEVEL_0) {
			md_throttle_cmd = TMC_CTRL_CMD_TX_POWER |
			OVER_CURRENT << 8 | level << 16;
		}
		break;
	default:
		break;
	};

	if (md_throttle_cmd)
		ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
				ID_THROTTLING_CFG,
				(char *) &md_throttle_cmd, 4);

	if (ret || !md_throttle_cmd)
		CCCI_ERROR_LOG(md->index, TAG,
			"%s: error, ret=%d, t=%d l=%d\n",
			__func__, ret, type, level);
	return ret;
}

static void ccci_md_low_battery_cb(LOW_BATTERY_LEVEL level)
{
	int idx = 0;
	struct ccci_modem *md = NULL;

	for (idx = 0; idx < MAX_MD_NUM; idx++) {
		md = ccci_md_get_modem_by_id(idx);
		if (md != NULL)
			ccci_md_low_power_notify(md, LOW_BATTERY, level);
	}
}

static void ccci_md_over_current_cb(BATTERY_OC_LEVEL level)
{
	int idx = 0;
	struct ccci_modem *md = NULL;

	for (idx = 0; idx < MAX_MD_NUM; idx++) {
		md = ccci_md_get_modem_by_id(idx);
		if (md != NULL)
			ccci_md_low_power_notify(md, OVER_CURRENT, level);
	}
}
#endif

static int ccci_platform_init(struct ccci_modem *md)
{
#ifdef FEATURE_LOW_BATTERY_SUPPORT
	register_low_battery_notify(
		&ccci_md_low_battery_cb, LOW_BATTERY_PRIO_MD);
	register_battery_percent_notify(
		&ccci_md_battery_percent_cb, BATTERY_PERCENT_PRIO_MD);
#endif
	return 0;
}

void ccci_platform_init_6873(struct ccci_modem *md)
{

	ccci_platform_init(md);
}


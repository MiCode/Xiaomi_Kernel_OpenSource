// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <mt-plat/mtk_ccci_common.h>
#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"

#ifdef FEATURE_USING_4G_MEMORY_API
#include <mt-plat/mtk_lpae.h>
#endif

#define TAG "plat"

int Is_MD_EMI_voilation(void)
{
	return 1;
}
#ifdef FEATURE_LOW_BATTERY_SUPPORT
static int ccci_md_low_power_notify(struct ccci_modem *md,
	enum LOW_POEWR_NOTIFY_TYPE type, int level)
{
	unsigned int reserve = 0xFFFFFFFF;
	int ret = 0;

	CCCI_NORMAL_LOG(md->index, TAG,
		"low power notification type=%d, level=%d\n", type, level);
	/*
	 * byte3 byte2 byte1 byte0
	 *    0   4G   3G   2G
	 */
	switch (type) {
	case LOW_BATTERY:
		if (level == LOW_BATTERY_LEVEL_0)
			reserve = 0;	/* 0 */
		else if (level == LOW_BATTERY_LEVEL_1
						|| level == LOW_BATTERY_LEVEL_2)
			reserve = (1 << 6);	/* 64 */
		ret = port_proxy_send_msg_to_md(md->port_proxy,
			CCCI_SYSTEM_TX, MD_LOW_BATTERY_LEVEL, reserve, 1);
		if (ret)
			CCCI_ERROR_LOG(md->index, TAG,
			 "send low battery notification fail, ret=%d\n", ret);
		break;
	case BATTERY_PERCENT:
		if (level == BATTERY_PERCENT_LEVEL_0)
			reserve = 0;	/* 0 */
		else if (level == BATTERY_PERCENT_LEVEL_1)
			reserve = (1 << 6);	/* 64 */
		ret = port_proxy_send_msg_to_md(md->port_proxy,
			CCCI_SYSTEM_TX, MD_LOW_BATTERY_LEVEL, reserve, 1);
		if (ret)
			CCCI_ERROR_LOG(md->index, TAG,
			"send battery percent info fail, ret=%d\n", ret);
		break;
	default:
		break;
	};

	return ret;
}

static void ccci_md_low_battery_cb(LOW_BATTERY_LEVEL level)
{
	int idx = 0;
	struct ccci_modem *md;

	for (idx = 0; idx < MAX_MD_NUM; idx++) {
		md = ccci_md_get_modem_by_id(idx);
		if (md != NULL)
			ccci_md_low_power_notify(md, LOW_BATTERY, level);
	}
}

static void ccci_md_battery_percent_cb(BATTERY_PERCENT_LEVEL level)
{
	int idx = 0;
	struct ccci_modem *md;

	for (idx = 0; idx < MAX_MD_NUM; idx++) {
		md = ccci_md_get_modem_by_id(idx);
		if (md != NULL)
			ccci_md_low_power_notify(md, BATTERY_PERCENT, level);
	}
}
#endif

int ccci_platform_init(struct ccci_modem *md)
{
	struct device_node *node;
	/* Get infra cfg ao base */
	node = of_find_compatible_node(NULL, NULL,
		"mediatek,infracfg_ao");
	md_cd_plat_val_ptr.infra_ao_base = of_iomap(node, 0);
	if (!md_cd_plat_val_ptr.infra_ao_base) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s: infra_ao_base of_iomap failed\n", node->full_name);
		return -1;
	}
	CCCI_INIT_LOG(-1, TAG, "infra_ao_base:0x%p\n",
		(void *)md_cd_plat_val_ptr.infra_ao_base);
#ifdef FEATURE_LOW_BATTERY_SUPPORT
	register_low_battery_notify(
		&ccci_md_low_battery_cb, LOW_BATTERY_PRIO_MD);
	register_battery_percent_notify(
		&ccci_md_battery_percent_cb, BATTERY_PERCENT_PRIO_MD);
#endif
	return 0;
}


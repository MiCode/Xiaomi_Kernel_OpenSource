/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <asm/setup.h>
#include <linux/of_fdt.h>
#include <mt-plat/mt_ccci_common.h>
#include "ccci_config.h"
#include "ccci_support.h"
#include "port_proxy.h"

static struct ccci_setting ccci_cfg_setting;
struct ccci_setting *ccci_get_common_setting(int md_id)
{
#ifdef CONFIG_EVDO_DT_SUPPORT
	ccci_cfg_setting.slot1_mode = CONFIG_MTK_TELEPHONY_BOOTUP_MODE_SLOT1;
	ccci_cfg_setting.slot2_mode = CONFIG_MTK_TELEPHONY_BOOTUP_MODE_SLOT2;
#endif
	return &ccci_cfg_setting;
}

int ccci_store_sim_switch_mode(struct ccci_modem *md, int simmode)
{
	if (ccci_cfg_setting.sim_mode != simmode) {
		ccci_cfg_setting.sim_mode = simmode;
		port_proxy_send_msg_to_user(md->port_proxy_obj, CCCI_MONITOR_CH, CCCI_MD_MSG_CFG_UPDATE, 1);
	} else {
		CCCI_NORMAL_LOG(md->index, CORE, "same sim mode as last time(0x%x)\n", simmode);
	}
	return 0;
}

int ccci_get_sim_switch_mode(void)
{
	return ccci_cfg_setting.sim_mode;
}

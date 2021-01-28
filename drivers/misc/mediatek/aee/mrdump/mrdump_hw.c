// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kconfig.h>
#include <linux/module.h>
#include <linux/string.h>
#ifdef CONFIG_MTK_DFD_INTERNAL_DUMP
#include <mtk_platform_debug.h>
#endif

static char mrdump_lk_ddr_reserve_ready[4];

static int __init mrdump_get_ddr_reserve_status(char *str)
{
	strlcpy(mrdump_lk_ddr_reserve_ready, str,
			sizeof(mrdump_lk_ddr_reserve_ready));
	return 0;
}

early_param("mrdump_ddrsv", mrdump_get_ddr_reserve_status);

#ifdef CONFIG_MTK_WATCHDOG
#include <mtk_wd_api.h>

#ifdef CONFIG_MTK_LASTPC_V2
static void mrdump_set_sram_lastpc_flag(void)
{
	if (!set_sram_flag_lastpc_valid())
		pr_info("OK: set sram flag lastpc valid.\n");
}

static void mrdump_wd_mcu_cache_preserve(bool enabled)
{
	int res;
	struct wd_api *wd_api;

	res = get_wd_api(&wd_api);
	if (res < 0) {
		pr_notice("%s: get wd api error %d\n", __func__, res);
	} else {
		res = wd_api->wd_mcu_cache_preserve(enabled);
		if (!res) {
			if (enabled == true)
				pr_notice("%s: MCU Cache Preserve enabled\n",
						__func__);
			else
				pr_notice("%s: MCU Cache Preserve disabled\n",
						__func__);
		}
	}
}
#endif /* CONFIG_MTK_LASTPC_V2 */

static bool mrdump_ddr_reserve_is_ready(void)
{
	if (!strncmp(mrdump_lk_ddr_reserve_ready, "yes", 3))
		return true;
	else
		return false;
}

static void mrdump_wd_dram_reserved_mode(bool enabled)
{
	int res;
	struct wd_api *wd_api;

	pr_notice("%s: DDR Reserved Mode ready or not? (%s)\n", __func__,
			mrdump_lk_ddr_reserve_ready);
	res = get_wd_api(&wd_api);
	if (res < 0) {
		pr_notice("%s: get wd api error (%d)\n", __func__, res);
	} else {
		if (mrdump_ddr_reserve_is_ready()) {
			res = wd_api->wd_dram_reserved_mode(enabled);
			if (!res) {
				if (enabled) {
					pr_notice("%s: DDR reserved mode enabled\n",
							__func__);
#ifdef CONFIG_MTK_DFD_INTERNAL_DUMP
					res = dfd_setup();
					if (res == -1)
						pr_notice("%s: DFD disabled\n",
								__func__);
					else
						pr_notice("%s: DFD enabled\n",
								__func__);
#else
			pr_notice("%s: config is not enabled yet\n",
					__func__);
#endif
				} else {
					pr_notice("%s: DDR reserved mode disabled(%d)\n",
							__func__, res);
				}
			}
		}
	}
}

int __init mrdump_hw_init(void)
{
	mrdump_wd_dram_reserved_mode(true);
#ifdef CONFIG_MTK_LASTPC_V2
	mrdump_wd_mcu_cache_preserve(true);
	mrdump_set_sram_lastpc_flag();
#endif /* CONFIG_MTK_LASTPC_V2 */
	pr_info("%s: init_done.\n", __func__);
	return 0;
}

#else

int __init mrdump_hw_init(void)
{
	pr_notice("%s: DDR Reserved Mode ready or not? (%s)\n", __func__,
			mrdump_lk_ddr_reserve_ready);

	return 0;
}

#endif /* CONFIG_MTK_WATCHDOG */

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek AEE module");
MODULE_AUTHOR("MediaTek Inc.");

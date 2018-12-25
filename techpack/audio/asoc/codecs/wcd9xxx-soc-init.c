/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <sound/wcd-dsp-mgr.h>
#include "audio-ext-clk-up.h"

static int __init wcd9xxx_soc_init(void)
{
	int ret = 0;

	ret = wcd_dsp_mgr_init();
	if (!ret) {
		ret = audio_ref_clk_platform_init();
		if (ret) {
			pr_err("%s: init extclk fail: %d\n", __func__, ret);
			wcd_dsp_mgr_exit();
		}
	} else {
		pr_err("%s: init dsp mgr fail: %d\n", __func__, ret);
	}

	return ret;
}
module_init(wcd9xxx_soc_init);

static void __exit wcd9xxx_soc_exit(void)
{
	audio_ref_clk_platform_exit();
	wcd_dsp_mgr_exit();
}
module_exit(wcd9xxx_soc_exit);

MODULE_DESCRIPTION("WCD9XXX CODEC soc init driver");
MODULE_LICENSE("GPL v2");

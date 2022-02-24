/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "mtk_ppm_platform.h"
#include "mtk_ppm_internal.h"


static int __init ppm_power_data_init(void)
{
	ppm_lock(&ppm_main_info.lock);

	ppm_cobra_init();

	ppm_platform_init();

#ifdef PPM_SSPM_SUPPORT
	ppm_ipi_init(0, PPM_COBRA_TBL_SRAM_ADDR);
#endif

	ppm_unlock(&ppm_main_info.lock);

	/* let PPM apply setting issued earlier*/
	mt_ppm_main();

	ppm_info("power data init done!\n");

	return 0;
}

/* should be run after cpufreq and upower init */
late_initcall(ppm_power_data_init);


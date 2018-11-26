/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef __SDM660_INTERNAL
#define __SDM660_INTERNAL

#include <sound/soc.h>

#if IS_ENABLED(CONFIG_SND_SOC_INT_CODEC)
int msm_int_cdc_init(struct platform_device *pdev,
		     struct msm_asoc_mach_data *pdata,
		     struct snd_soc_card **card,
		     struct wcd_mbhc_config *mbhc_cfg);
#else
int msm_int_cdc_init(struct platform_device *pdev,
		     struct msm_asoc_mach_data *pdata,
		     struct snd_soc_card **card,
		     struct wcd_mbhc_config *mbhc_cfg)
{
	return 0;
}
#endif
#endif

/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _AUDIO_ANC_DEV_MGR_H_
#define _AUDIO_ANC_DEV_MGR_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/clk/msm-clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/msm-dai-q6-v2.h>
#include <linux/msm_audio_anc.h>

int msm_anc_dev_init(void);
int msm_anc_dev_deinit(void);

int msm_anc_dev_start(void);
int msm_anc_dev_stop(void);

int msm_anc_dev_set_info(void *info_p, int32_t anc_cmd);

int msm_anc_dev_get_info(void *info_p, int32_t anc_cmd);

int msm_anc_dev_create(struct platform_device *pdev);

int msm_anc_dev_destroy(struct platform_device *pdev);

#endif

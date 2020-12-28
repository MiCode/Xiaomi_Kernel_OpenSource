/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#ifndef AQT1000_API_H
#define AQT1000_API_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/soc.h>

extern int aqt_mbhc_micb_adjust_voltage(struct snd_soc_component *component,
					int volt, int micb_num);
extern int aqt_cdc_mclk_enable(struct snd_soc_component *component,
			       bool enable);
extern int aqt_get_micb_vout_ctl_val(u32 micb_mv);
extern int aqt_micbias_control(struct snd_soc_component *component,
			       int micb_num, int req, bool is_dapm);

#endif /* AQT1000_API_H */

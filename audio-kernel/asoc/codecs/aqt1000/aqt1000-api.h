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

#ifndef AQT1000_API_H
#define AQT1000_API_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/soc.h>

extern int aqt_mbhc_micb_adjust_voltage(struct snd_soc_codec *codec,
					int volt, int micb_num);
extern int aqt_cdc_mclk_enable(struct snd_soc_codec *codec, bool enable);
extern int aqt_get_micb_vout_ctl_val(u32 micb_mv);
extern int aqt_micbias_control(struct snd_soc_codec *codec, int micb_num,
			       int req, bool is_dapm);

#endif /* AQT1000_API_H */

/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MACH_QDSP5_V2_SNDDEV_ICODEC_H
#define __MACH_QDSP5_V2_SNDDEV_ICODEC_H
#include <linux/mfd/msm-adie-codec.h>
#include <mach/qdsp5v2/audio_def.h>
#include <mach/pmic.h>

struct snddev_icodec_data {
	u32 capability; /* RX or TX */
	const char *name;
	u32 copp_id; /* audpp routing */
	u32 acdb_id; /* Audio Cal purpose */
	/* Adie profile */
	struct adie_codec_dev_profile *profile;
	/* Afe setting */
	u8 channel_mode;
	enum hsed_controller *pmctl_id; /* tx only enable mic bias */
	u32 pmctl_id_sz;
	u32 default_sample_rate;
	void (*pamp_on) (void);
	void (*pamp_off) (void);
	void (*voltage_on) (void);
	void (*voltage_off) (void);
	s32 max_voice_rx_vol[VOC_RX_VOL_ARRAY_NUM]; /* [0]: NB,[1]: WB */
	s32 min_voice_rx_vol[VOC_RX_VOL_ARRAY_NUM];
	u32 dev_vol_type;
	u32 property; /*variable used to hold the properties
				internal to the device*/
};
#endif

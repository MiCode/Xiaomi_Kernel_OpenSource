/* Copyright (c) 2009-2011, The Linux Foundation. All rights reserved.
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
#ifndef _MACH_QDSP5_V2_AFE_H
#define _MACH_QDSP5_V2_AFE_H

#include <asm/types.h>
#include <mach/qdsp5v2/audio_acdbi.h>

#define AFE_HW_PATH_CODEC_RX    1
#define AFE_HW_PATH_CODEC_TX    2
#define AFE_HW_PATH_AUXPCM_RX   3
#define AFE_HW_PATH_AUXPCM_TX   4
#define AFE_HW_PATH_MI2S_RX     5
#define AFE_HW_PATH_MI2S_TX     6

#define AFE_VOLUME_UNITY 0x4000 /* Based on Q14 */

struct msm_afe_config {
	u16 sample_rate;
	u16 channel_mode;
	u16 volume;
	/* To be expaned for AUX CODEC */
};

int afe_enable(u8 path_id, struct msm_afe_config *config);

int afe_disable(u8 path_id);

int afe_config_aux_codec(int pcm_ctl_value, int aux_codec_intf_value,
			int data_format_pad);
int afe_config_fm_codec(int fm_enable, uint16_t source);

int afe_config_fm_volume(uint16_t volume);
int afe_config_fm_calibration_gain(uint16_t device_id,
			uint16_t calibration_gain);
void afe_loopback(int enable);
void afe_ext_loopback(int enable, int rx_copp_id, int tx_copp_id);

void afe_device_volume_ctrl(u16 device_id, u16 device_volume);

int afe_config_rmc_block(struct acdb_rmc_block *acdb_rmc);
#endif

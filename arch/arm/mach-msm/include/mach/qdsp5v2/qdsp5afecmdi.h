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
#ifndef __MACH_QDSP5_V2_QDSP5AFECMDI_H
#define __MACH_QDSP5_V2_QDSP5AFECMDI_H

#define QDSP5_DEVICE_mI2S_CODEC_RX 1     /* internal codec rx path  */
#define QDSP5_DEVICE_mI2S_CODEC_TX 2     /* internal codec tx path  */
#define QDSP5_DEVICE_AUX_CODEC_RX  3     /* external codec rx path  */
#define QDSP5_DEVICE_AUX_CODEC_TX  4     /* external codec tx path  */
#define QDSP5_DEVICE_mI2S_HDMI_RX  5     /* HDMI/FM block rx path   */
#define QDSP5_DEVICE_mI2S_HDMI_TX  6     /* HDMI/FM block tx path   */
#define QDSP5_DEVICE_ID_MAX        7

#define AFE_CMD_CODEC_CONFIG_CMD     0x1
#define AFE_CMD_CODEC_CONFIG_LEN sizeof(struct afe_cmd_codec_config)

struct afe_cmd_codec_config{
	uint16_t cmd_id;
	uint16_t device_id;
	uint16_t activity;
	uint16_t sample_rate;
	uint16_t channel_mode;
	uint16_t volume;
	uint16_t reserved;
} __attribute__ ((packed));

#define AFE_CMD_DEVICE_VOLUME_CTRL	0x2
#define AFE_CMD_DEVICE_VOLUME_CTRL_LEN \
		sizeof(struct afe_cmd_device_volume_ctrl)

struct afe_cmd_device_volume_ctrl {
	uint16_t cmd_id;
	uint16_t device_id;
	uint16_t device_volume;
	uint16_t reserved;
} __attribute__ ((packed));

#define AFE_CMD_AUX_CODEC_CONFIG_CMD 	0x3
#define AFE_CMD_AUX_CODEC_CONFIG_LEN sizeof(struct afe_cmd_aux_codec_config)

struct afe_cmd_aux_codec_config{
	uint16_t cmd_id;
	uint16_t dma_path_ctl;
	uint16_t pcm_ctl;
	uint16_t eight_khz_int_mode;
	uint16_t aux_codec_intf_ctl;
	uint16_t data_format_padding_info;
} __attribute__ ((packed));

#define AFE_CMD_FM_RX_ROUTING_CMD	0x6
#define AFE_CMD_FM_RX_ROUTING_LEN sizeof(struct afe_cmd_fm_codec_config)

struct afe_cmd_fm_codec_config{
	uint16_t cmd_id;
	uint16_t enable;
	uint16_t device_id;
} __attribute__ ((packed));

#define AFE_CMD_FM_PLAYBACK_VOLUME_CMD	0x8
#define AFE_CMD_FM_PLAYBACK_VOLUME_LEN sizeof(struct afe_cmd_fm_volume_config)

struct afe_cmd_fm_volume_config{
	uint16_t cmd_id;
	uint16_t volume;
	uint16_t reserved;
} __attribute__ ((packed));

#define AFE_CMD_FM_CALIBRATION_GAIN_CMD	0x11
#define AFE_CMD_FM_CALIBRATION_GAIN_LEN \
	sizeof(struct afe_cmd_fm_calibgain_config)

struct afe_cmd_fm_calibgain_config{
	uint16_t cmd_id;
	uint16_t device_id;
	uint16_t calibration_gain;
} __attribute__ ((packed));

#define AFE_CMD_LOOPBACK	0xD
#define AFE_CMD_EXT_LOOPBACK	0xE
#define AFE_CMD_LOOPBACK_LEN sizeof(struct afe_cmd_loopback)
#define AFE_LOOPBACK_ENABLE_COMMAND 0xFFFF
#define AFE_LOOPBACK_DISABLE_COMMAND 0x0000

struct afe_cmd_loopback {
	uint16_t cmd_id;
	uint16_t enable_flag;
	uint16_t reserved[2];
} __attribute__ ((packed));

struct afe_cmd_ext_loopback {
	uint16_t cmd_id;
	uint16_t enable_flag;
	uint16_t source_id;
	uint16_t dst_id;
	uint16_t reserved[2];
} __packed;

#define AFE_CMD_CFG_RMC_PARAMS 0x12
#define AFE_CMD_CFG_RMC_LEN \
	sizeof(struct afe_cmd_cfg_rmc)

struct afe_cmd_cfg_rmc {
	unsigned short cmd_id;
	signed short   rmc_mode;
	unsigned short rmc_ipw_length_ms;
	unsigned short rmc_peak_length_ms;
	unsigned short rmc_init_pulse_length_ms;
	unsigned short rmc_total_int_length_ms;
	unsigned short rmc_rampupdn_length_ms;
	unsigned short rmc_delay_length_ms;
	unsigned short rmc_detect_start_threshdb;
	signed short   rmc_init_pulse_threshdb;
}  __attribute__((packed));

#endif

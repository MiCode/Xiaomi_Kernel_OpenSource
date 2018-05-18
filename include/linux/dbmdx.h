/*
 * dbmdx-interface.h  --  DBMDX interface definitions
 *
 * Copyright (C) 2014 DSP Group
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMDX_H
#define _DBMDX_H

#include <linux/device.h>
#include <sound/soc.h>

#define MAX_NUM_OF_INTERFACES 4

enum dbmdx_va_speeds {
	DBMDX_VA_SPEED_NORMAL = 0,
	DBMDX_VA_SPEED_BUFFERING,
	DBMDX_VA_SPEED_MAX,
	DBMDX_VA_NR_OF_SPEEDS,
};

struct va_speed {
	u32	cfg;
	u32	uart_baud;
	u32	i2c_rate;
	u32	spi_rate;
};

enum dbmdx_clocks {
	DBMDX_CLK_CONSTANT = 0,
	DBMDX_CLK_MASTER,
	DBMDX_NR_OF_CLKS,
};

enum dbmdx_interface_type {
	DBMDX_PREBOOT_INTERFACE = 0,
	DBMDX_BOOT_INTERFACE,
	DBMDX_CMD_INTERFACE,
	DBMDX_DEBUG_INTERFACE,
	DBMDX_MAX_INTERFACES,
};

struct dbmdx_platform_data {
	int				gpio_wakeup;
	int				gpio_reset;
	int				gpio_sv;
	int				gpio_d2strap1;
	int				gpio_d2strap1_rst_val;
	int				wakeup_disabled;
	int				wakeup_set_value;
	int				send_wakeup_seq;
	int				use_gpio_for_wakeup;

	const char			*cd_interfaces[MAX_NUM_OF_INTERFACES];
	const char			*va_firmware_name;
	const char			*va_preboot_firmware_name;
	const char			*vqe_firmware_name;
	const char			*vqe_non_overlay_firmware_name;
	int				firmware_id;

	int				clock_rates[DBMDX_NR_OF_CLKS];

	int				feature_va;
	int				feature_vqe;
	int				feature_fw_overlay;

	unsigned int			va_cfg_values;
	u32				*va_cfg_value;

	unsigned int			vqe_cfg_values;
	u32				*vqe_cfg_value;
	unsigned int			vqe_modes_values;
	u32				*vqe_modes_value;

	int				auto_buffering;
	int				auto_detection;
	int				detection_after_buffering;
	int				send_uevent_on_detection;
	int				send_uevent_after_buffering;
	int				uart_low_speed_enabled;

	int				max_detection_buffer_size;

	int				detection_buffer_channels;
	u32				min_samples_chunk_size;
	u32				pcm_streaming_mode;
	int				buffering_timeout;

	struct va_speed	va_speed_cfg[DBMDX_VA_NR_OF_SPEEDS];
	u32				va_mic_config[5];
	u32				va_initial_mic_config;
	u32				va_mic_gain_config[3];
	int				va_audio_channels;
	u32				vqe_tdm_bypass_config;
	unsigned int			va_buffering_pcm_rate;

	unsigned int			va_recovery_disabled;
	u32				multi_interface_support;
	u32				boot_options;
	u32				amodel_options;

	int	va_interfaces[DBMDX_MAX_INTERFACES];
	int	vqe_interfaces[DBMDX_MAX_INTERFACES];

};

int dbmdx_get_samples(struct snd_soc_codec *codec,
	char *buffer, unsigned int samples);
int dbmdx_codec_lock(struct snd_soc_codec *codec);
int dbmdx_start_pcm_streaming(struct snd_soc_codec *codec,
	struct snd_pcm_substream *substream);
int dbmdx_stop_pcm_streaming(struct snd_soc_codec *codec);
int dbmdx_codec_unlock(struct snd_soc_codec *codec);

#endif

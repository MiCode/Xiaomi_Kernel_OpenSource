/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <sound/soc.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>

#define SITAR_VERSION_1_0	0x00

#define SITAR_NUM_REGISTERS 0x3E0
#define SITAR_MAX_REGISTER (SITAR_NUM_REGISTERS-1)
#define SITAR_CACHE_SIZE SITAR_NUM_REGISTERS

#define SITAR_REG_VAL(reg, val)		{reg, 0, val}

/* Local to the core only */
#define SITAR_SLIM_MAX_RX_PORTS 5
#define SITAR_SLIM_MAX_TX_PORTS 5

extern const u8 sitar_reg_readable[SITAR_CACHE_SIZE];
extern const u8 sitar_reg_defaults[SITAR_CACHE_SIZE];

enum sitar_micbias_num {
	SITAR_MICBIAS1,
	SITAR_MICBIAS2,
};

enum sitar_pid_current {
	SITAR_PID_MIC_2P5_UA,
	SITAR_PID_MIC_5_UA,
	SITAR_PID_MIC_10_UA,
	SITAR_PID_MIC_20_UA,
};

struct sitar_mbhc_calibration {
	enum sitar_micbias_num bias;
	int tldoh;
	int bg_fast_settle;
	enum sitar_pid_current mic_current;
	int mic_pid;
	enum sitar_pid_current hph_current;
	int setup_plug_removal_delay;
	int shutdown_plug_removal;
};

struct sitar_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

extern int sitar_hs_detect(struct snd_soc_codec *codec,
	struct snd_soc_jack *headset_jack, struct snd_soc_jack *button_jack,
	struct sitar_mbhc_calibration *calibration);

#ifndef anc_header_dec
struct anc_header {
	u32 reserved[3];
	u32 num_anc_slots;
};
#define anc_header_dec
#endif

extern int sitar_mclk_enable(struct snd_soc_codec *codec, int mclk_enable);

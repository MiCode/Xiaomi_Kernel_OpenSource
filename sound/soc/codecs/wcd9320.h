/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef WCD9320_H
#define WCD9320_H

#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>
#include "wcd9xxx-mbhc.h"
#include "wcd9xxx-resmgr.h"

#define TAIKO_NUM_REGISTERS 0x400
#define TAIKO_MAX_REGISTER (TAIKO_NUM_REGISTERS-1)
#define TAIKO_CACHE_SIZE TAIKO_NUM_REGISTERS

#define TAIKO_REG_VAL(reg, val)		{reg, 0, val}
#define TAIKO_MCLK_ID 0

extern const u8 taiko_reg_readable[TAIKO_CACHE_SIZE];
extern const u8 taiko_reset_reg_defaults[TAIKO_CACHE_SIZE];
struct taiko_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};

enum taiko_pid_current {
	TAIKO_PID_MIC_2P5_UA,
	TAIKO_PID_MIC_5_UA,
	TAIKO_PID_MIC_10_UA,
	TAIKO_PID_MIC_20_UA,
};

struct taiko_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

enum taiko_mbhc_analog_pwr_cfg {
	TAIKO_ANALOG_PWR_COLLAPSED = 0,
	TAIKO_ANALOG_PWR_ON,
	TAIKO_NUM_ANALOG_PWR_CONFIGS,
};

/* Number of input and output Slimbus port */
enum {
	TAIKO_RX1 = 0,
	TAIKO_RX2,
	TAIKO_RX3,
	TAIKO_RX4,
	TAIKO_RX5,
	TAIKO_RX6,
	TAIKO_RX7,
	TAIKO_RX8,
	TAIKO_RX9,
	TAIKO_RX10,
	TAIKO_RX11,
	TAIKO_RX12,
	TAIKO_RX13,
	TAIKO_RX_MAX,
};

enum {
	TAIKO_TX1 = 0,
	TAIKO_TX2,
	TAIKO_TX3,
	TAIKO_TX4,
	TAIKO_TX5,
	TAIKO_TX6,
	TAIKO_TX7,
	TAIKO_TX8,
	TAIKO_TX9,
	TAIKO_TX10,
	TAIKO_TX11,
	TAIKO_TX12,
	TAIKO_TX13,
	TAIKO_TX14,
	TAIKO_TX15,
	TAIKO_TX16,
	TAIKO_TX_MAX,
};

struct anc_header {
	u32 reserved[3];
	u32 num_anc_slots;
};

extern int taiko_mclk_enable(struct snd_soc_codec *codec, int mclk_enable,
			     bool dapm);
extern int taiko_hs_detect(struct snd_soc_codec *codec,
			   struct wcd9xxx_mbhc_config *mbhc_cfg);

#endif

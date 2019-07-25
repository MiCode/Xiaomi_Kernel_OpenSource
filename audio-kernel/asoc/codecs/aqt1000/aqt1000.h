/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#ifndef AQT1000_H
#define AQT1000_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include "pdata.h"
#include "aqt1000-clsh.h"

#define AQT1000_MAX_MICBIAS 1
#define AQT1000_NUM_INTERPOLATORS 2
#define AQT1000_NUM_DECIMATORS 3
#define  AQT1000_VOUT_CTL_TO_MICB(v)  (1000 + v * 50)
#define AQT1000_RX_PATH_CTL_OFFSET 20

#define AQT1000_CLK_24P576MHZ 24576000
#define AQT1000_CLK_19P2MHZ 19200000
#define AQT1000_CLK_12P288MHZ 12288000
#define AQT1000_CLK_9P6MHZ 9600000

#define AQT1000_ST_IIR_COEFF_MAX 5

enum {
	AQT1000_RX0 = 0,
	AQT1000_RX1,
	AQT1000_RX_MAX,
};

enum {
	AQT_NONE,
	AQT_MCLK,
	AQT_RCO,
};

enum {
	AQT_TX0 = 0,
	AQT_TX1,
};

enum {
	ASRC0,
	ASRC1,
	ASRC_MAX,
};

/* Each IIR has 5 Filter Stages */
enum {
	BAND1 = 0,
	BAND2,
	BAND3,
	BAND4,
	BAND5,
	BAND_MAX,
};

enum {
	AQT1000_TX0 = 0,
	AQT1000_TX1,
	AQT1000_TX2,
	AQT1000_TX_MAX,
};

enum {
	INTERP_HPHL,
	INTERP_HPHR,
	INTERP_MAX,
};

enum {
	INTERP_MAIN_PATH,
	INTERP_MIX_PATH,
};

enum {
	COMPANDER_1, /* HPH_L */
	COMPANDER_2, /* HPH_R */
	COMPANDER_MAX,
};

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	NUM_CODEC_DAIS,
};

struct aqt_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};

struct aqt_idle_detect_config {
	u8 hph_idle_thr;
	u8 hph_idle_detect_en;
};

struct aqt1000_i2c {
	struct i2c_client *client;
	struct i2c_msg xfer_msg[2];
	struct mutex xfer_lock;
	int mod_id;
};

struct aqt1000_cdc_dai_data {
	u32 rate;		/* sample rate */
	u32 bit_width;		/* sit width 16,24,32 */
	struct list_head ch_list;
	wait_queue_head_t dai_wait;
};

struct tx_mute_work {
	struct aqt1000 *aqt;
	u8 decimator;
	struct delayed_work dwork;
};

struct hpf_work {
	struct aqt1000 *aqt;
	u8 decimator;
	u8 hpf_cut_off_freq;
	struct delayed_work dwork;
};

struct aqt1000 {
	struct device *dev;
	struct mutex io_lock;
	struct mutex xfer_lock;
	struct mutex reset_lock;

	struct device_node *aqt_rst_np;

	int (*read_dev)(struct aqt1000 *aqt, unsigned short reg,
			void *dest, int bytes);
	int (*write_dev)(struct aqt1000 *aqt, unsigned short reg,
			 void *src, int bytes);

	u32 num_of_supplies;
	struct regulator_bulk_data *supplies;

	u32 mclk_rate;
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	bool dev_up;
	bool prev_pg_valid;
	u8 prev_pg;

	struct aqt1000_i2c i2c_dev;

	/* Codec params */

	/* ANC related */
	u32 anc_slot;
	bool anc_func;

	/* compander */
	int comp_enabled[COMPANDER_MAX];

	/* class h specific data */
	struct aqt_clsh_cdc_data clsh_d;

	/* Interpolator Mode Select for HPH_L and HPH_R */
	u32 hph_mode;

	unsigned long status_mask;

	struct aqt1000_cdc_dai_data dai[NUM_CODEC_DAIS];

	struct mutex micb_lock;

	struct clk *ext_clk;

	/* mbhc module */
	struct aqt1000_mbhc *mbhc;

	struct mutex codec_mutex;

	/* cal info for codec */
	struct fw_info *fw_data;

	int native_clk_users;
	/* ASRC users count */
	int asrc_users[ASRC_MAX];
	int asrc_output_mode[ASRC_MAX];
	/* Main path clock users count */
	int main_clk_users[AQT1000_NUM_INTERPOLATORS];

	struct aqt_idle_detect_config idle_det_cfg;
	u32 rx_bias_count;

	s32 micb_ref;
	s32 pullup_ref;
	int master_bias_users;
	int mclk_users;
	int i2s_users;

	struct hpf_work tx_hpf_work[AQT1000_NUM_DECIMATORS];
	struct tx_mute_work tx_mute_dwork[AQT1000_NUM_DECIMATORS];

	struct mutex master_bias_lock;
	struct mutex cdc_bg_clk_lock;
	struct mutex i2s_lock;

	/* Interrupt */
	struct regmap_irq_chip_data *irq_chip;
	int num_irq_regs;
	struct irq_domain *virq;
	int irq;
	int irq_base;

	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
};

#endif /* AQT1000_H */

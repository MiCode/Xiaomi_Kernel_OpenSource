/* SPDX-License-Identifier: GPL-2.0 */

/*
 * linux/sound/cs35l43.h -- Platform data for CS35L43
 *
 * Copyright (c) 2021 Cirrus Logic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CS35L43_H
#define __CS35L43_H


#define CS35L43_RX_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE\
				| SNDRV_PCM_FMTBIT_S32_LE)
#define CS35L43_TX_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE \
				| SNDRV_PCM_FMTBIT_S32_LE)

#define CS35L43_VALID_PDATA		0x80000000

struct cs35l43_platform_data {
	bool gpio1_out_enable;
	bool gpio2_out_enable;
	bool classh_disable;
	bool dsp_ng_enable;
	int asp_sdout_hiz;
	int dsp_ng_pcm_thld;
	int dsp_ng_delay;
	int dout_hiz;
	int bst_vctrl;
	int hw_ng_sel;
	int hw_ng_delay;
	int hw_ng_thld;
	int gpio1_src_sel;
	int gpio2_src_sel;
};

struct cs35l43_pll_sysclk_config {
	int freq;
	int clk_cfg;
};

struct cs35l43_fs_mon_config {
	int freq;
	unsigned int fs1;
	unsigned int fs2;
};

extern const struct cs35l43_pll_sysclk_config cs35l43_pll_sysclk[64];
extern const struct cs35l43_fs_mon_config cs35l43_fs_mon[7];
extern const unsigned int cs35l43_hibernate_update_regs[CS35L43_POWER_SEQ_LENGTH];
extern const u8 cs35l43_write_seq_op_sizes[CS35L43_POWER_SEQ_NUM_OPS][2];

enum cs35l43_hibernate_state {
	CS35L43_HIBERNATE_AWAKE		= 0,
	CS35L43_HIBERNATE_STANDBY	= 1,
	CS35L43_HIBERNATE_UPDATE	= 2,
	CS35L43_HIBERNATE_NOT_LOADED	= 3,
	CS35L43_HIBERNATE_DISABLED	= 4,
};

struct cs35l43_write_seq_elem {
	u8 size;
	u16 offset; /* offset in words from pseq_base */
	u8 operation;
	u32 *words;
	struct list_head list;
};

struct cs35l43_write_seq {
	const char *name;
	struct list_head list_head;
	unsigned int num_ops;
	unsigned int length;
};

enum cs35l43_hibernate_mode {
	CS35L43_ULTRASONIC_MODE_DISABLED = 0,
	CS35L43_ULTRASONIC_MODE_INBAND = 1,
	CS35L43_ULTRASONIC_MODE_OUT_OF_BAND = 2,
};

struct cs35l43_private {
	struct wm_adsp dsp; /* needs to be first member */
	struct snd_soc_component *component;
	struct cs35l43_platform_data pdata;
	struct device *dev;
	struct regmap *regmap;
	struct regulator_bulk_data supplies[2];
	int num_supplies;
	int irq;
	int extclk_cfg;
	int clk_id;
	int lrclk_fmt;
	int sclk_fmt;
	int asp_fmt;
	int hibernate_state;
	int hibernate_delay_ms;
	int ultrasonic_mode;
	struct gpio_desc *reset_gpio;
	struct delayed_work hb_work;
	struct workqueue_struct *wq;
	struct mutex hb_lock;
	struct mutex rate_lock;
	struct cs35l43_write_seq power_on_seq;
};

int cs35l43_probe(struct cs35l43_private *cs35l43,
				struct cs35l43_platform_data *pdata);
int cs35l43_remove(struct cs35l43_private *cs35l43);

bool cs35l43_readable_reg(struct device *dev, unsigned int reg);
bool cs35l43_precious_reg(struct device *dev, unsigned int reg);
bool cs35l43_volatile_reg(struct device *dev, unsigned int reg);

extern const struct reg_default cs35l43_reg[1];

#endif /* __CS35L43_H */

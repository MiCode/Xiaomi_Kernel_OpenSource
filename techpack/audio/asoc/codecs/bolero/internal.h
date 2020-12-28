/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _BOLERO_INTERNAL_H
#define _BOLERO_INTERNAL_H

#include "bolero-cdc-registers.h"

#define BOLERO_CDC_CHILD_DEVICES_MAX 6

enum {
	REG_NO_ACCESS,
	RD_REG,
	WR_REG,
	RD_WR_REG
};

struct wcd_ctrl_platform_data {
	void *handle;
	int (*update_wcd_event)(void *handle, u16 event, u32 data);
	int (*register_notifier)(void *handle,
				 struct notifier_block *nblock,
				 bool enable);
};

struct bolero_priv {
	struct device *dev;
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct mutex io_lock;
	struct mutex clk_lock;
	struct mutex vote_lock;
	bool va_without_decimation;
	bool macros_supported[MAX_MACRO];
	bool dev_up;
	bool initial_boot;
	struct macro_ops macro_params[MAX_MACRO];
	struct snd_soc_dai_driver *bolero_dais;
	u16 num_dais;
	u16 num_macros_registered;
	u16 num_macros;
	u16 current_mclk_mux_macro[MAX_MACRO];
	struct work_struct bolero_add_child_devices_work;
	u32 version;
	struct clk *lpass_core_hw_vote;
	struct clk *lpass_audio_hw_vote;
	int core_hw_vote_count;
	int core_audio_vote_count;

	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;

	int (*read_dev)(struct bolero_priv *priv,
			u16 macro_id, u16 reg, u8 *val);
	int (*write_dev)(struct bolero_priv *priv,
			 u16 macro_id, u16 reg, u8 val);
	struct platform_device *pdev_child_devices
			[BOLERO_CDC_CHILD_DEVICES_MAX];
	u16 child_count;
	struct wcd_ctrl_platform_data plat_data;
	struct device *wcd_dev;
	struct blocking_notifier_head notifier;
	struct device *clk_dev;
	rsc_clk_cb_t rsc_clk_cb;
	s32 dmic_0_1_clk_cnt;
	s32 dmic_2_3_clk_cnt;
	s32 dmic_4_5_clk_cnt;
	s32 dmic_6_7_clk_cnt;
	u8 dmic_0_1_clk_div;
	u8 dmic_2_3_clk_div;
	u8 dmic_4_5_clk_div;
	u8 dmic_6_7_clk_div;
};

struct regmap *bolero_regmap_init(struct device *dev,
				  const struct regmap_config *config);
int bolero_get_macro_id(bool va_no_dec_flag, u16 reg);

extern const struct regmap_config bolero_regmap_config;
extern u8 *bolero_reg_access[MAX_MACRO];
extern u8 bolero_va_top_reg_access[BOLERO_CDC_VA_MACRO_TOP_MAX];
extern u8 bolero_va_reg_access_v2[BOLERO_CDC_VA_MACRO_MAX];
extern u8 bolero_va_reg_access_v3[BOLERO_CDC_VA_MACRO_MAX];
extern u8 bolero_tx_reg_access_v2[BOLERO_CDC_TX_MACRO_MAX];
extern const u16 macro_id_base_offset[MAX_MACRO];

#endif

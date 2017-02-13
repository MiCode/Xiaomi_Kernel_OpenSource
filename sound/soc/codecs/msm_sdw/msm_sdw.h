/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#ifndef MSM_SDW_H
#define MSM_SDW_H

#include <sound/soc.h>
#include <sound/q6afe-v2.h>
#include "msm_sdw_registers.h"

#define MSM_SDW_MAX_REGISTER 0x400

extern const struct regmap_config msm_sdw_regmap_config;
extern const u8 msm_sdw_page_map[MSM_SDW_MAX_REGISTER];
extern const u8 msm_sdw_reg_readable[MSM_SDW_MAX_REGISTER];

enum {
	MSM_SDW_RX4 = 0,
	MSM_SDW_RX5,
	MSM_SDW_RX_MAX,
};

enum {
	MSM_SDW_TX0 = 0,
	MSM_SDW_TX1,
	MSM_SDW_TX_MAX,
};

enum {
	COMP1, /* SPK_L */
	COMP2, /* SPK_R */
	COMP_MAX
};

/*
 * Structure used to update codec
 * register defaults after reset
 */
struct msm_sdw_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

/*
 * Selects compander and smart boost settings
 * for a given speaker mode
 */
enum {
	SPKR_MODE_DEFAULT,
	SPKR_MODE_1,          /* COMP Gain = 12dB, Smartboost Max = 5.5V */
};

/* Rx path gain offsets */
enum {
	RX_GAIN_OFFSET_M1P5_DB,
	RX_GAIN_OFFSET_0_DB,
};

struct msm_sdw_reg_val {
	unsigned short reg; /* register address */
	u8 *buf;            /* buffer to be written to reg. addr */
	int bytes;          /* number of bytes to be written */
};

/* Hold instance to soundwire platform device */
struct msm_sdw_ctrl_data {
	struct platform_device *sdw_pdev;
};

struct wcd_sdw_ctrl_platform_data {
	void *handle; /* holds codec private data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*handle_irq)(void *handle,
			  irqreturn_t (*swrm_irq_handler)(int irq,
							  void *data),
			  void *swrm_handle,
			  int action);
};

struct msm_sdw_priv {
	struct device *dev;
	struct mutex io_lock;

	int (*read_dev)(struct msm_sdw_priv *msm_sdw, unsigned short reg,
			int bytes, void *dest);
	int (*write_dev)(struct msm_sdw_priv *msm_sdw, unsigned short reg,
			 int bytes, void *src);
	int (*multi_reg_write)(struct msm_sdw_priv *msm_sdw, const void *data,
			       size_t count);
	struct snd_soc_codec *codec;
	struct device_node *sdw_gpio_p; /* used by pinctrl API */
	/* SoundWire data structure */
	struct msm_sdw_ctrl_data *sdw_ctrl_data;
	int nr;

	/* compander */
	int comp_enabled[COMP_MAX];
	int ear_spkr_gain;

	/* to track the status */
	unsigned long status_mask;

	struct work_struct msm_sdw_add_child_devices_work;
	struct wcd_sdw_ctrl_platform_data sdw_plat_data;

	unsigned int vi_feed_value;

	struct mutex sdw_read_lock;
	struct mutex sdw_write_lock;
	struct mutex sdw_clk_lock;
	int sdw_clk_users;
	int sdw_mclk_users;

	int sdw_irq;
	int int_mclk1_rsc_ref;
	bool int_mclk1_enabled;
	bool sdw_npl_clk_enabled;
	struct mutex cdc_int_mclk1_mutex;
	struct mutex sdw_npl_clk_mutex;
	struct delayed_work disable_int_mclk1_work;
	struct afe_clk_set sdw_cdc_core_clk;
	struct afe_clk_set sdw_npl_clk;
	struct notifier_block service_nb;
	int (*sdw_cdc_gpio_fn)(bool enable, struct snd_soc_codec *codec);
	bool dev_up;

	int spkr_gain_offset;
	int spkr_mode;
	struct mutex codec_mutex;
	int rx_4_count;
	int rx_5_count;
	u32 mclk_rate;
	struct regmap *regmap;

	bool prev_pg_valid;
	u8 prev_pg;
	u32 sdw_base_addr;
	char __iomem *sdw_base;
	u32 version;

	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
};

extern int msm_sdw_set_spkr_mode(struct snd_soc_codec *codec, int mode);
extern int msm_sdw_set_spkr_gain_offset(struct snd_soc_codec *codec,
					int offset);
extern void msm_sdw_gpio_cb(
	int (*sdw_cdc_gpio_fn)(bool enable, struct snd_soc_codec *codec),
	struct snd_soc_codec *codec);
extern struct regmap *msm_sdw_regmap_init(struct device *dev,
					  const struct regmap_config *config);
extern int msm_sdw_codec_info_create_codec_entry(struct snd_info_entry *,
						 struct snd_soc_codec *);
#endif

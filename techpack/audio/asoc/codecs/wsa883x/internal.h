/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef WSA883X_INTERNAL_H
#define WSA883X_INTERNAL_H

#include <asoc/wcd-irq.h>
#include "wsa883x.h"
#include "wsa883x-registers.h"

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define SWR_SLV_MAX_REG_ADDR	0x2009
#define SWR_SLV_START_REG_ADDR	0x40
#define SWR_SLV_MAX_BUF_LEN	20
#define BYTES_PER_LINE		12
#define SWR_SLV_RD_BUF_LEN	8
#define SWR_SLV_WR_BUF_LEN	32
#define SWR_SLV_MAX_DEVICES	2
#endif /* CONFIG_DEBUG_FS */

#define WSA883X_DRV_NAME "wsa883x-codec"
#define WSA883X_NUM_RETRY	5

#define WSA883X_VERSION_ENTRY_SIZE 32
#define WSA883X_VARIANT_ENTRY_SIZE 32

#define WSA883X_VERSION_1_0 0
#define WSA883X_VERSION_1_1 1

enum {
	G_18DB = 0,
	G_16P5DB,
	G_15DB,
	G_13P5DB,
	G_12DB,
	G_10P5DB,
	G_9DB,
	G_7P5DB,
	G_6DB,
	G_4P5DB,
	G_3DB,
	G_1P5DB,
	G_0DB,
};

enum {
	DISABLE = 0,
	ENABLE,
};

enum {
	SWR_DAC_PORT,
	SWR_COMP_PORT,
	SWR_BOOST_PORT,
	SWR_VISENSE_PORT,
};

struct wsa_ctrl_platform_data {
	void *handle;
	int (*update_wsa_event)(void *handle, u16 event, u32 data);
	int (*register_notifier)(void *handle, struct notifier_block *nblock,
				bool enable);
};

struct swr_port {
	u8 port_id;
	u8 ch_mask;
	u32 ch_rate;
	u8 num_ch;
	u8 port_type;
};

extern struct regmap_config wsa883x_regmap_config;

/*
 * Private data Structure for wsa883x. All parameters related to
 * WSA883X codec needs to be defined here.
 */
struct wsa883x_priv {
	struct regmap *regmap;
	struct device *dev;
	struct swr_device *swr_slave;
	struct snd_soc_component *component;
	bool comp_enable;
	bool visense_enable;
	bool ext_vdd_spk;
	bool dapm_bias_off;
	struct swr_port port[WSA883X_MAX_SWR_PORTS];
	int global_pa_cnt;
	int dev_mode;
	struct mutex res_lock;
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
	struct snd_info_entry *variant_entry;
	struct device_node *wsa_rst_np;
	int pa_mute;
	int curr_temp;
	int variant;
	int version;
	u8 pa_gain;
	struct irq_domain *virq;
	struct wcd_irq_info irq_info;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dent;
	struct dentry *debugfs_peek;
	struct dentry *debugfs_poke;
	struct dentry *debugfs_reg_dump;
	unsigned int read_data;
#endif
	struct device_node *parent_np;
	struct platform_device *parent_dev;
	struct notifier_block parent_nblock;
	void *handle;
	int (*register_notifier)(void *handle,
				struct notifier_block *nblock, bool enable);
	struct cdc_regulator *regulator;
	int num_supplies;
	struct regulator_bulk_data *supplies;
	unsigned long status_mask;
	char *wsa883x_name_prefix;
	struct snd_soc_dai_driver *dai_driver;
	struct snd_soc_component_driver *driver;
};

#endif /* WSA883X_INTERNAL_H */

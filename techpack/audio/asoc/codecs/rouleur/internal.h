/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _ROULEUR_INTERNAL_H
#define _ROULEUR_INTERNAL_H

#include <asoc/wcd-clsh.h>
#include <asoc/wcd-mbhc-v2.h>
#include <asoc/wcd-irq.h>
#include "rouleur-mbhc.h"

#define ROULEUR_MAX_MICBIAS 3

/* Convert from vout ctl to micbias voltage in mV */
#define  WCD_VOUT_CTL_TO_MICB(v)  (1600 + v * 50)
#define MAX_PORT 8
#define MAX_CH_PER_PORT 8

extern struct regmap_config rouleur_regmap_config;

struct codec_port_info {
	u32 slave_port_type;
	u32 master_port_type;
	u32 ch_mask;
	u32 num_ch;
	u32 ch_rate;
};

struct rouleur_priv {
	struct device *dev;

	int variant;
	struct snd_soc_component *component;
	struct device_node *spmi_np;
	struct regmap *regmap;

	struct swr_device *rx_swr_dev;
	struct swr_device *tx_swr_dev;

	s32 micb_ref[ROULEUR_MAX_MICBIAS];
	s32 pullup_ref[ROULEUR_MAX_MICBIAS];

	struct fw_info *fw_data;

	struct mutex micb_lock;
	s32 dmic_0_1_clk_cnt;
	/* mbhc module */
	struct rouleur_mbhc *mbhc;

	bool comp1_enable;
	bool comp2_enable;
	bool dapm_bias_off;

	struct irq_domain *virq;
	struct wcd_irq_info irq_info;
	u32 rx_clk_cnt;
	int num_irq_regs;
	/* to track the status */
	unsigned long status_mask;

	u8 num_tx_ports;
	u8 num_rx_ports;
	struct codec_port_info
			tx_port_mapping[MAX_PORT][MAX_CH_PER_PORT];
	struct codec_port_info
			rx_port_mapping[MAX_PORT][MAX_CH_PER_PORT];
	struct regulator_bulk_data *supplies;
	struct notifier_block nblock;
	/* wcd callback to bolero */
	void *handle;
	int (*update_wcd_event)(void *handle, u16 event, u32 data);
	int (*register_notifier)(void *handle,
				struct notifier_block *nblock,
				bool enable);
	int (*wakeup)(void *handle, bool enable);
	u32 version;
	/* Entry for version info */
	struct snd_info_entry *entry;
	struct snd_info_entry *version_entry;
	struct device *spmi_dev;
	int reset_reg;
	int mbias_cnt;
	struct mutex rx_clk_lock;
	struct mutex main_bias_lock;
	bool dev_up;
	bool usbc_hs_status;
	struct notifier_block psy_nb;
	struct work_struct soc_eval_work;
	bool low_soc;
	int foundry_id_reg;
	int foundry_id;
};

struct rouleur_micbias_setting {
	u32 micb1_mv;
	u32 micb2_mv;
	u32 micb3_mv;
};

struct rouleur_pdata {
	struct device_node *spmi_np;
	struct device_node *rx_slave;
	struct device_node *tx_slave;
	struct rouleur_micbias_setting micbias;

	struct cdc_regulator *regulator;
	int num_supplies;
	int reset_reg;
	int foundry_id_reg;
};

struct wcd_ctrl_platform_data {
	void *handle;
	int (*update_wcd_event)(void *handle, u16 event, u32 data);
	int (*register_notifier)(void *handle,
				 struct notifier_block *nblock,
				 bool enable);
};

enum {
	WCD_RX1,
	WCD_RX2,
	WCD_RX3
};

enum {
	/* INTR_CTRL_INT_MASK_0 */
	ROULEUR_IRQ_MBHC_BUTTON_PRESS_DET = 0,
	ROULEUR_IRQ_MBHC_BUTTON_RELEASE_DET,
	ROULEUR_IRQ_MBHC_ELECT_INS_REM_DET,
	ROULEUR_IRQ_MBHC_ELECT_INS_REM_LEG_DET,
	ROULEUR_IRQ_MBHC_SW_DET,
	ROULEUR_IRQ_HPHR_OCP_INT,
	ROULEUR_IRQ_HPHR_CNP_INT,
	ROULEUR_IRQ_HPHL_OCP_INT,

	/* INTR_CTRL_INT_MASK_1 */
	ROULEUR_IRQ_HPHL_CNP_INT,
	ROULEUR_IRQ_EAR_CNP_INT,
	ROULEUR_IRQ_EAR_OCP_INT,
	ROULEUR_IRQ_LO_CNP_INT,
	ROULEUR_IRQ_LO_OCP_INT,
	ROULEUR_IRQ_HPHL_PDM_WD_INT,
	ROULEUR_IRQ_HPHR_PDM_WD_INT,
	ROULEUR_IRQ_RESERVED_0,

	/* INTR_CTRL_INT_MASK_2 */
	ROULEUR_IRQ_RESERVED_1,
	ROULEUR_IRQ_RESERVED_2,
	ROULEUR_IRQ_HPHL_SURGE_DET_INT,
	ROULEUR_IRQ_HPHR_SURGE_DET_INT,
	ROULEUR_NUM_IRQS,
};

extern void rouleur_disable_bcs_before_slow_insert(
				struct snd_soc_component *component,
				bool bcs_disable);
extern struct rouleur_mbhc *rouleur_soc_get_mbhc(
				struct snd_soc_component *component);
extern int rouleur_mbhc_micb_adjust_voltage(struct snd_soc_component *component,
					int volt, int micb_num);
extern int rouleur_get_micb_vout_ctl_val(u32 micb_mv);
extern int rouleur_micbias_control(struct snd_soc_component *component,
			int micb_num, int req, bool is_dapm);
extern int rouleur_global_mbias_enable(struct snd_soc_component *component);
extern int rouleur_global_mbias_disable(struct snd_soc_component *component);
#endif

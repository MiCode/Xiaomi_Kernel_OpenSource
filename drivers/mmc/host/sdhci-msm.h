/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef __SDHCI_MSM_H__
#define __SDHCI_MSM_H__

#include <linux/mmc/mmc.h>
#include <linux/pm_qos.h>
#include "sdhci-pltfm.h"

/* This structure keeps information per regulator */
struct sdhci_msm_reg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage level to be set */
	u32 low_vol_level;
	u32 high_vol_level;
	/* Load values for low power and high power mode */
	u32 lpm_uA;
	u32 hpm_uA;

	/* is this regulator enabled? */
	bool is_enabled;
	/* is this regulator needs to be always on? */
	bool is_always_on;
	/* is low power mode setting required for this regulator? */
	bool lpm_sup;
	bool set_voltage_sup;
};

/*
 * This structure keeps information for all the
 * regulators required for a SDCC slot.
 */
struct sdhci_msm_slot_reg_data {
	/* keeps VDD/VCC regulator info */
	struct sdhci_msm_reg_data *vdd_data;
	 /* keeps VDD IO regulator info */
	struct sdhci_msm_reg_data *vdd_io_data;
};

struct sdhci_msm_gpio {
	u32 no;
	const char *name;
	bool is_enabled;
};

struct sdhci_msm_gpio_data {
	struct sdhci_msm_gpio *gpio;
	u8 size;
};

struct sdhci_msm_pin_data {
	/*
	 * = 1 if controller pins are using gpios
	 * = 0 if controller has dedicated MSM pads
	 */
	u8 is_gpio;
	struct sdhci_msm_gpio_data *gpio_data;
};

struct sdhci_pinctrl_data {
	struct pinctrl          *pctrl;
	struct pinctrl_state    *pins_active;
	struct pinctrl_state    *pins_sleep;
};

struct sdhci_msm_bus_voting_data {
	struct msm_bus_scale_pdata *bus_pdata;
	unsigned int *bw_vecs;
	unsigned int bw_vecs_size;
};

struct sdhci_msm_cpu_group_map {
	int nr_groups;
	cpumask_t *mask;
};

struct sdhci_msm_pm_qos_latency {
	s32 latency[SDHCI_POWER_POLICY_NUM];
};

struct sdhci_msm_pm_qos_data {
	struct sdhci_msm_cpu_group_map cpu_group_map;
	enum pm_qos_req_type irq_req_type;
	int irq_cpu;
	struct sdhci_msm_pm_qos_latency irq_latency;
	struct sdhci_msm_pm_qos_latency *cmdq_latency;
	struct sdhci_msm_pm_qos_latency *latency;
	bool irq_valid;
	bool cmdq_valid;
	bool legacy_valid;
};

/*
 * PM QoS for group voting management - each cpu group defined is associated
 * with 1 instance of this structure.
 */
struct sdhci_msm_pm_qos_group {
	struct pm_qos_request req;
	struct work_struct unvote_work;
	atomic_t counter;
	s32 latency;
};

/* PM QoS HW IRQ voting */
struct sdhci_msm_pm_qos_irq {
	struct pm_qos_request req;
	struct work_struct unvote_work;
	struct device_attribute enable_attr;
	struct device_attribute status_attr;
	atomic_t counter;
	s32 latency;
	bool enabled;
};

struct sdhci_msm_pltfm_data {
	/* Supported UHS-I Modes */
	u32 caps;

	/* More capabilities */
	u32 caps2;

	unsigned long mmc_bus_width;
	struct sdhci_msm_slot_reg_data *vreg_data;
	bool nonremovable;
	bool nonhotplug;
	bool largeaddressbus;
	bool pin_cfg_sts;
	struct sdhci_msm_pin_data *pin_data;
	struct sdhci_pinctrl_data *pctrl_data;
	int status_gpio; /* card detection GPIO that is configured as IRQ */
	struct sdhci_msm_bus_voting_data *voting_data;
	u32 *sup_clk_table;
	unsigned char sup_clk_cnt;
	int sdiowakeup_irq;
	u32 *sup_ice_clk_table;
	unsigned char sup_ice_clk_cnt;
	u32 ice_clk_max;
	u32 ice_clk_min;
	struct sdhci_msm_pm_qos_data pm_qos_data;
	bool core_3_0v_support;
};

struct sdhci_msm_bus_vote {
	uint32_t client_handle;
	uint32_t curr_vote;
	int min_bw_vote;
	int max_bw_vote;
	bool is_max_bw_needed;
	struct delayed_work vote_work;
	struct device_attribute max_bus_bw;
};

struct sdhci_msm_ice_data {
	struct qcom_ice_variant_ops *vops;
	struct platform_device *pdev;
	int state;
};

struct sdhci_msm_host {
	struct platform_device	*pdev;
	void __iomem *core_mem;    /* MSM SDCC mapped address */
	int	pwr_irq;	/* power irq */
	struct clk	 *clk;     /* main SD/MMC bus clock */
	struct clk	 *pclk;    /* SDHC peripheral bus clock */
	struct clk	 *bus_clk; /* SDHC bus voter clock */
	struct clk	 *ff_clk; /* CDC calibration fixed feedback clock */
	struct clk	 *sleep_clk; /* CDC calibration sleep clock */
	struct clk	 *ice_clk; /* SDHC peripheral ICE clock */
	atomic_t clks_on; /* Set if clocks are enabled */
	struct sdhci_msm_pltfm_data *pdata;
	struct mmc_host  *mmc;
	struct sdhci_pltfm_data sdhci_msm_pdata;
	u32 curr_pwr_state;
	u32 curr_io_level;
	struct completion pwr_irq_completion;
	struct sdhci_msm_bus_vote msm_bus_vote;
	struct device_attribute	polling;
	u32 clk_rate; /* Keeps track of current clock rate that is set */
	bool tuning_done;
	bool calibration_done;
	u8 saved_tuning_phase;
	bool en_auto_cmd21;
	struct device_attribute auto_cmd21_attr;
	bool is_sdiowakeup_enabled;
	atomic_t controller_clock;
	bool use_cdclp533;
	bool use_updated_dll_reset;
	bool use_14lpp_dll;
	bool enhanced_strobe;
	bool rclk_delay_fix;
	u32 caps_0;
	struct sdhci_msm_ice_data ice;
	u32 ice_clk_rate;
	struct sdhci_msm_pm_qos_group *pm_qos;
	int pm_qos_prev_cpu;
	struct device_attribute pm_qos_group_enable_attr;
	struct device_attribute pm_qos_group_status_attr;
	bool pm_qos_group_enable;
	struct sdhci_msm_pm_qos_irq pm_qos_irq;
};

extern char *saved_command_line;

void sdhci_msm_pm_qos_irq_init(struct sdhci_host *host);
void sdhci_msm_pm_qos_irq_vote(struct sdhci_host *host);
void sdhci_msm_pm_qos_irq_unvote(struct sdhci_host *host, bool async);

void sdhci_msm_pm_qos_cpu_init(struct sdhci_host *host,
		struct sdhci_msm_pm_qos_latency *latency);
void sdhci_msm_pm_qos_cpu_vote(struct sdhci_host *host,
		struct sdhci_msm_pm_qos_latency *latency, int cpu);
bool sdhci_msm_pm_qos_cpu_unvote(struct sdhci_host *host, int cpu, bool async);


#endif /* __SDHCI_MSM_H__ */

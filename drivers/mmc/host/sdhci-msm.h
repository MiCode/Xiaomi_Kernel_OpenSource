/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __SDHCI_MSM_H__
#define __SDHCI_MSM_H__

#include <linux/mmc/mmc.h>
#include <linux/pm_qos.h>
#include <linux/reset.h>
#include "sdhci-pltfm.h"

/* check IP CATALOG version */
#define SDCC_IP_CATALOG 0x328

/* DBG register offsets */
#define SDCC_TESTBUS_CONFIG 0x32C
#define SDCC_DEBUG_EN_DIS_REG 0x390
#define SDCC_DEBUG_FEATURE_CFG_REG 0x394
#define SDCC_DEBUG_FSM_TRACE_CFG_REG 0x398
#define SDCC_DEBUG_FSM_TRACE_RD_REG 0x39C
#define SDCC_DEBUG_FSM_TRACE_FIFO_FLUSH_REG 0x3A0
#define SDCC_DEBUG_PANIC_ERROR_EN_REG 0x3A4
#define SDCC_DEBUG_ERROR_STATE_EXIT_REG 0x3B8
#define SDCC_CURR_DESC_ADDR 0x3EC
#define SDCC_CURR_DESC_INFO 0x3F0
#define SDCC_PROC_DESC0_ADDR 0x3E4
#define SDCC_PROC_DESC0_INFO 0x3E8
#define SDCC_PROC_DESC1_ADDR 0x3DC
#define SDCC_PROC_DESC1_INFO 0x3E0
#define SDCC_DEBUG_IIB_REG 0x980
#define SDCC_DEBUG_MASK_PATTERN_REG 0x3C0
#define SDCC_DEBUG_MATCH_PATTERN_REG 0x3C4
#define SDCC_DEBUG_MM_TB_CFG_REG 0x3BC

#define ENABLE_DBG 0x35350000
#define DISABLE_DBG 0x26260000

#define DUMMY 0x1 /* value doesn't matter */

/* Panic on Err */
#define BOOT_ACK_REC_EN BIT(0)
#define BOOT_ACK_ERR_EN BIT(1)
#define BOOT_TIMEOUT_EN BIT(2)
#define AUTO_CMD19_TOUT_EN BIT(3)
#define STBITE_EN BIT(4)
#define CTOUT_EN BIT(5)
#define CCRCF_EN BIT(6)
#define CMD_END_BIT_ERR_EN BIT(7)
#define CMD_INDEX_ERR_EN BIT(8)
#define DTOUT_EN BIT(9)
#define DCRCF_EN BIT(10)
#define DATA_END_BIT_ERR_EN BIT(11)
#define CMDQ_HALT_ACK_INT_EN BIT(16)
#define CMDQ_TASK_COMPLETED_INT_EN BIT(17)
#define CMDQ_RESP_ERR_INT_EN BIT(18)
#define CMDQ_TASK_CLEARED_INT_EN BIT(19)
#define CMDQ_GENERAL_CRYPTO_ERROR_EN BIT(20)
#define CMDQ_INVALID_CRYPTO_CFG_ERROR_EN BIT(21)
#define CMDQ_DEVICE_EXCEPTION_INT_EN BIT(22)
#define ADMA_ERROR_EXT_EN BIT(23)
#define HC_NONCQ_ICE_INT_STATUS_MASKED_EN BIT(24)

/* Select debug Feature */
#define FSM_HISTORY BIT(0)
#define PANIC_ALERT BIT(1)
#define AUTO_RECOVERY_DISABLE BIT(2)
#define MM_TRIGGER_DISABLE BIT(3)
#define DESC_HISTORY BIT(4)
#define IIB_EN BIT(6)

#define TESTBUS_EN BIT(31)

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
	 /* Keeps VDD IO parent regulator info*/
	struct sdhci_msm_reg_data *vdd_io_bias_data;
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
	struct pinctrl_state    *pins_drv_type_400KHz;
	struct pinctrl_state    *pins_drv_type_50MHz;
	struct pinctrl_state    *pins_drv_type_100MHz;
	struct pinctrl_state    *pins_drv_type_200MHz;
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
	struct delayed_work unvote_work;
	atomic_t counter;
	s32 latency;
};

/* PM QoS HW IRQ voting */
struct sdhci_msm_pm_qos_irq {
	struct pm_qos_request req;
	struct delayed_work unvote_work;
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
	int testbus_trigger_irq;
	struct sdhci_msm_pm_qos_data pm_qos_data;
	u32 *bus_clk_table;
	unsigned char bus_clk_cnt;
	u32 *sup_ice_clk_table;
	unsigned char sup_ice_clk_cnt;
	u32 ice_clk_max;
	u32 ice_clk_min;
};

struct sdhci_msm_bus_vote {
	uint32_t client_handle;
	uint32_t curr_vote;
	int min_bw_vote;
	int max_bw_vote;
	bool is_max_bw_needed;
	struct device_attribute max_bus_bw;
};

struct sdhci_msm_regs_restore {
	bool is_supported;
	bool is_valid;
	u32 vendor_pwrctl_mask;
	u32 vendor_pwrctl_ctl;
	u32 vendor_caps_0;
	u32 vendor_func;
	u32 vendor_func2;
	u32 vendor_func3;
	u32 hc_2c_2e;
	u32 hc_28_2a;
	u32 hc_34_36;
	u32 hc_38_3a;
	u32 hc_3c_3e;
	u32 hc_caps_1;
	u32 testbus_config;
	u32 dll_config;
	u32 dll_config2;
	u32 dll_config3;
	u32 dll_usr_ctl;
};

/*
 * DLL registers which needs be programmed with HSR settings.
 * Add any new register only at the end and don't change the
 * sequence.
 */
struct sdhci_msm_dll_hsr {
	u32 dll_config;
	u32 dll_config_2;
	u32 dll_config_3;
	u32 dll_usr_ctl;
	u32 ddr_config;
};

struct sdhci_msm_debug_data {
	struct mmc_host copy_mmc;
	struct mmc_card copy_card;
	struct sdhci_host copy_host;
};

struct sdhci_msm_host {
	struct platform_device	*pdev;
	void __iomem *core_mem;    /* MSM SDCC mapped address */
	int	pwr_irq;	/* power irq */
	struct clk	 *clk;     /* main SD/MMC bus clock */
	struct clk	 *pclk;    /* SDHC peripheral bus clock */
	struct clk	 *bus_aggr_clk; /* Axi clock shared with UFS */
	struct clk	 *bus_clk; /* SDHC bus voter clock */
	struct clk	 *ff_clk; /* CDC calibration fixed feedback clock */
	struct clk	 *sleep_clk; /* CDC calibration sleep clock */
	struct clk	 *ice_clk; /* SDHC peripheral ICE clock */
	atomic_t clks_on; /* Set if clocks are enabled */
	struct sdhci_msm_pltfm_data *pdata;
	struct mmc_host  *mmc;
	struct cqhci_host *cq_host;
	struct sdhci_msm_debug_data cached_data;
	struct sdhci_pltfm_data sdhci_msm_pdata;
	u32 curr_pwr_state;
	u32 curr_io_level;
	struct completion pwr_irq_completion;
	struct sdhci_msm_bus_vote msm_bus_vote;
	struct device_attribute	polling;
	struct device_attribute mask_and_match;
	u32 clk_rate; /* Keeps track of current clock rate that is set */
	bool tuning_done;
	bool calibration_done;
	u8 saved_tuning_phase;
	bool en_auto_cmd21;
	struct device_attribute auto_cmd21_attr;
	bool is_sdiowakeup_enabled;
	bool sdio_pending_processing;
	atomic_t controller_clock;
	bool use_cdclp533;
	bool use_updated_dll_reset;
	bool use_14lpp_dll;
	bool enhanced_strobe;
	bool rclk_delay_fix;
	u32 caps_0;
	struct sdhci_msm_pm_qos_group *pm_qos;
	int pm_qos_prev_cpu;
	struct device_attribute pm_qos_group_enable_attr;
	struct device_attribute pm_qos_group_status_attr;
	bool pm_qos_group_enable;
	struct sdhci_msm_pm_qos_irq pm_qos_irq;
	bool tuning_in_progress;
	bool mci_removed;
	const struct sdhci_msm_offset *offset;
	bool core_3_0v_support;
	bool pltfm_init_done;
	struct sdhci_msm_regs_restore regs_restore;
	bool use_7nm_dll;
	int soc_min_rev;
	struct workqueue_struct *pm_qos_wq;
	struct sdhci_msm_dll_hsr *dll_hsr;
	u32 ice_clk_rate;
	bool debug_mode_enabled;
	bool reg_store;
	struct reset_control *core_reset;
	u32 minor;
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

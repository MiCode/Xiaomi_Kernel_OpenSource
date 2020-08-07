/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
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

#ifndef UFS_QCOM_H_
#define UFS_QCOM_H_

#include <linux/phy/phy.h>
#include <linux/pm_qos.h>
#include "ufshcd.h"

#define MAX_UFS_QCOM_HOSTS	2
#define MAX_U32                 (~(u32)0)
#define MPHY_TX_FSM_STATE       0x41
#define MPHY_RX_FSM_STATE       0xC1
#define TX_FSM_HIBERN8          0x1
#define HBRN8_POLL_TOUT_MS      100
#define DEFAULT_CLK_RATE_HZ     1000000
#define BUS_VECTOR_NAME_LEN     32

#define UFS_HW_VER_MAJOR_SHFT	(28)
#define UFS_HW_VER_MAJOR_MASK	(0x000F << UFS_HW_VER_MAJOR_SHFT)
#define UFS_HW_VER_MINOR_SHFT	(16)
#define UFS_HW_VER_MINOR_MASK	(0x0FFF << UFS_HW_VER_MINOR_SHFT)
#define UFS_HW_VER_STEP_SHFT	(0)
#define UFS_HW_VER_STEP_MASK	(0xFFFF << UFS_HW_VER_STEP_SHFT)

/* vendor specific pre-defined parameters */
#define SLOW 1
#define FAST 2

#define UFS_QCOM_LIMIT_NUM_LANES_RX	2
#define UFS_QCOM_LIMIT_NUM_LANES_TX	2
#define UFS_QCOM_LIMIT_HSGEAR_RX	UFS_HS_G4
#define UFS_QCOM_LIMIT_HSGEAR_TX	UFS_HS_G4
#define UFS_QCOM_LIMIT_PWMGEAR_RX	UFS_PWM_G4
#define UFS_QCOM_LIMIT_PWMGEAR_TX	UFS_PWM_G4
#define UFS_QCOM_LIMIT_RX_PWR_PWM	SLOW_MODE
#define UFS_QCOM_LIMIT_TX_PWR_PWM	SLOW_MODE
#define UFS_QCOM_LIMIT_RX_PWR_HS	FAST_MODE
#define UFS_QCOM_LIMIT_TX_PWR_HS	FAST_MODE
#define UFS_QCOM_LIMIT_HS_RATE		PA_HS_MODE_B
#define UFS_QCOM_LIMIT_DESIRED_MODE	FAST

/* QCOM UFS host controller vendor specific registers */
enum {
	REG_UFS_SYS1CLK_1US                 = 0xC0,
	REG_UFS_TX_SYMBOL_CLK_NS_US         = 0xC4,
	REG_UFS_LOCAL_PORT_ID_REG           = 0xC8,
	REG_UFS_PA_ERR_CODE                 = 0xCC,
	REG_UFS_RETRY_TIMER_REG             = 0xD0,
	REG_UFS_PA_LINK_STARTUP_TIMER       = 0xD8,
	REG_UFS_CFG1                        = 0xDC,
	REG_UFS_CFG2                        = 0xE0,
	REG_UFS_HW_VERSION                  = 0xE4,

	UFS_TEST_BUS				= 0xE8,
	UFS_TEST_BUS_CTRL_0			= 0xEC,
	UFS_TEST_BUS_CTRL_1			= 0xF0,
	UFS_TEST_BUS_CTRL_2			= 0xF4,
	UFS_UNIPRO_CFG				= 0xF8,

	/*
	 * QCOM UFS host controller vendor specific registers
	 * added in HW Version 3.0.0
	 */
	UFS_AH8_CFG				= 0xFC,
};

/* QCOM UFS host controller vendor specific debug registers */
enum {
	UFS_DBG_RD_REG_UAWM			= 0x100,
	UFS_DBG_RD_REG_UARM			= 0x200,
	UFS_DBG_RD_REG_TXUC			= 0x300,
	UFS_DBG_RD_REG_RXUC			= 0x400,
	UFS_DBG_RD_REG_DFC			= 0x500,
	UFS_DBG_RD_REG_TRLUT			= 0x600,
	UFS_DBG_RD_REG_TMRLUT			= 0x700,
	UFS_UFS_DBG_RD_REG_OCSC			= 0x800,

	UFS_UFS_DBG_RD_DESC_RAM			= 0x1500,
	UFS_UFS_DBG_RD_PRDT_RAM			= 0x1700,
	UFS_UFS_DBG_RD_RESP_RAM			= 0x1800,
	UFS_UFS_DBG_RD_EDTL_RAM			= 0x1900,
};

#define UFS_CNTLR_2_x_x_VEN_REGS_OFFSET(x)	(0x000 + x)
#define UFS_CNTLR_3_x_x_VEN_REGS_OFFSET(x)	(0x400 + x)

/* bit definitions for REG_UFS_CFG1 register */
#define QUNIPRO_SEL	UFS_BIT(0)
#define TEST_BUS_EN		BIT(18)
#define TEST_BUS_SEL		0x780000
#define UFS_REG_TEST_BUS_EN	BIT(30)

/* bit definitions for REG_UFS_CFG2 register */
#define UAWM_HW_CGC_EN		(1 << 0)
#define UARM_HW_CGC_EN		(1 << 1)
#define TXUC_HW_CGC_EN		(1 << 2)
#define RXUC_HW_CGC_EN		(1 << 3)
#define DFC_HW_CGC_EN		(1 << 4)
#define TRLUT_HW_CGC_EN		(1 << 5)
#define TMRLUT_HW_CGC_EN	(1 << 6)
#define OCSC_HW_CGC_EN		(1 << 7)

/* bit definition for UFS_UFS_TEST_BUS_CTRL_n */
#define TEST_BUS_SUB_SEL_MASK	0x1F  /* All XXX_SEL fields are 5 bits wide */

#define REG_UFS_CFG2_CGC_EN_ALL (UAWM_HW_CGC_EN | UARM_HW_CGC_EN |\
				 TXUC_HW_CGC_EN | RXUC_HW_CGC_EN |\
				 DFC_HW_CGC_EN | TRLUT_HW_CGC_EN |\
				 TMRLUT_HW_CGC_EN | OCSC_HW_CGC_EN)

/* bit definitions for UFS_AH8_CFG register */
#define CC_UFS_HCLK_REQ_EN		BIT(1)
#define CC_UFS_SYS_CLK_REQ_EN		BIT(2)
#define CC_UFS_ICE_CORE_CLK_REQ_EN	BIT(3)
#define CC_UFS_UNIPRO_CORE_CLK_REQ_EN	BIT(4)
#define CC_UFS_AUXCLK_REQ_EN		BIT(5)

#define UFS_HW_CLK_CTRL_EN	(CC_UFS_SYS_CLK_REQ_EN |\
				 CC_UFS_ICE_CORE_CLK_REQ_EN |\
				 CC_UFS_UNIPRO_CORE_CLK_REQ_EN |\
				 CC_UFS_AUXCLK_REQ_EN)
/* bit offset */
enum {
	OFFSET_UFS_PHY_SOFT_RESET           = 1,
	OFFSET_CLK_NS_REG                   = 10,
};

/* bit masks */
enum {
	MASK_UFS_PHY_SOFT_RESET             = 0x2,
	MASK_TX_SYMBOL_CLK_1US_REG          = 0x3FF,
	MASK_CLK_NS_REG                     = 0xFFFC00,
};

enum ufs_qcom_phy_init_type {
	UFS_PHY_INIT_FULL,
	UFS_PHY_INIT_CFG_RESTORE,
};

/* QCOM UFS debug print bit mask */
#define UFS_QCOM_DBG_PRINT_REGS_EN	BIT(0)
#define UFS_QCOM_DBG_PRINT_ICE_REGS_EN	BIT(1)
#define UFS_QCOM_DBG_PRINT_TEST_BUS_EN	BIT(2)

#define UFS_QCOM_DBG_PRINT_ALL	\
	(UFS_QCOM_DBG_PRINT_REGS_EN | UFS_QCOM_DBG_PRINT_ICE_REGS_EN | \
	 UFS_QCOM_DBG_PRINT_TEST_BUS_EN)

/* QUniPro Vendor specific attributes */
#define PA_VS_CONFIG_REG1		0x9000
#define SAVECONFIGTIME_MODE_MASK	0x6000

#define PA_VS_CLK_CFG_REG	0x9004
#define PA_VS_CLK_CFG_REG_MASK	0x1FF

#define PA_VS_CORE_CLK_40NS_CYCLES	0x9007
#define PA_VS_CORE_CLK_40NS_CYCLES_MASK	0xF

#define DL_VS_CLK_CFG		0xA00B
#define DL_VS_CLK_CFG_MASK	0x3FF

#define DME_VS_CORE_CLK_CTRL	0xD002
/* bit and mask definitions for DME_VS_CORE_CLK_CTRL attribute */
#define DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_MASK_V4	0xFFF
#define DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_OFFSET_V4	0x10
#define DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_MASK	0xFF
#define DME_VS_CORE_CLK_CTRL_CORE_CLK_DIV_EN_BIT		BIT(8)
#define DME_VS_CORE_CLK_CTRL_DME_HW_CGC_EN			BIT(9)

static inline void
ufs_qcom_get_controller_revision(struct ufs_hba *hba,
				 u8 *major, u16 *minor, u16 *step)
{
	u32 ver = ufshcd_readl(hba, REG_UFS_HW_VERSION);

	*major = (ver & UFS_HW_VER_MAJOR_MASK) >> UFS_HW_VER_MAJOR_SHFT;
	*minor = (ver & UFS_HW_VER_MINOR_MASK) >> UFS_HW_VER_MINOR_SHFT;
	*step = (ver & UFS_HW_VER_STEP_MASK) >> UFS_HW_VER_STEP_SHFT;
};

static inline void ufs_qcom_assert_reset(struct ufs_hba *hba)
{
	ufshcd_rmwl(hba, MASK_UFS_PHY_SOFT_RESET,
			1 << OFFSET_UFS_PHY_SOFT_RESET, REG_UFS_CFG1);

	/*
	 * Make sure assertion of ufs phy reset is written to
	 * register before returning
	 */
	mb();
}

static inline void ufs_qcom_deassert_reset(struct ufs_hba *hba)
{
	ufshcd_rmwl(hba, MASK_UFS_PHY_SOFT_RESET,
			0 << OFFSET_UFS_PHY_SOFT_RESET, REG_UFS_CFG1);

	/*
	 * Make sure de-assertion of ufs phy reset is written to
	 * register before returning
	 */
	mb();
}

struct ufs_qcom_bus_vote {
	uint32_t client_handle;
	uint32_t curr_vote;
	int min_bw_vote;
	int max_bw_vote;
	int saved_vote;
	bool is_max_bw_needed;
	struct device_attribute max_bus_bw;
};

/* Host controller hardware version: major.minor.step */
struct ufs_hw_version {
	u16 step;
	u16 minor;
	u8 major;
};

struct ufs_qcom_testbus {
	u8 select_major;
	u8 select_minor;
};

#ifdef CONFIG_DEBUG_FS
struct qcom_debugfs_files {
	struct dentry *debugfs_root;
	struct dentry *dbg_print_en;
	struct dentry *testbus;
	struct dentry *testbus_en;
	struct dentry *testbus_cfg;
	struct dentry *testbus_bus;
	struct dentry *dbg_regs;
	struct dentry *pm_qos;
};
#endif

/* PM QoS voting state  */
enum ufs_qcom_pm_qos_state {
	PM_QOS_UNVOTED,
	PM_QOS_VOTED,
	PM_QOS_REQ_VOTE,
	PM_QOS_REQ_UNVOTE,
};

/**
 * struct ufs_qcom_pm_qos_cpu_group - data related to cluster PM QoS voting
 *	logic
 * @req: request object for PM QoS
 * @vote_work: work object for voting procedure
 * @unvote_work: work object for un-voting procedure
 * @host: back pointer to the main structure
 * @state: voting state machine current state
 * @latency_us: requested latency value used for cluster voting, in
 *	microseconds
 * @mask: cpu mask defined for this cluster
 * @active_reqs: number of active requests on this cluster
 */
struct ufs_qcom_pm_qos_cpu_group {
	struct pm_qos_request req;
	struct work_struct vote_work;
	struct work_struct unvote_work;
	struct ufs_qcom_host *host;
	enum ufs_qcom_pm_qos_state state;
	s32 latency_us;
	cpumask_t mask;
	int active_reqs;
};

/**
 * struct ufs_qcom_pm_qos - data related to PM QoS voting logic
 * @groups: PM QoS cpu group state array
 * @enable_attr: sysfs attribute to enable/disable PM QoS voting logic
 * @latency_attr: sysfs attribute to set latency value
 * @workq: single threaded workqueue to run PM QoS voting/unvoting
 * @num_clusters: number of clusters defined
 * @default_cpu: cpu to use for voting for request not specifying a cpu
 * @is_enabled: flag specifying whether voting logic is enabled
 */
struct ufs_qcom_pm_qos {
	struct ufs_qcom_pm_qos_cpu_group *groups;
	struct device_attribute enable_attr;
	struct device_attribute latency_attr;
	struct workqueue_struct *workq;
	int num_groups;
	int default_cpu;
	bool is_enabled;
};

struct ufs_qcom_host {
	/*
	 * Set this capability if host controller supports the QUniPro mode
	 * and if driver wants the Host controller to operate in QUniPro mode.
	 * Note: By default this capability will be kept enabled if host
	 * controller supports the QUniPro mode.
	 */
	#define UFS_QCOM_CAP_QUNIPRO	UFS_BIT(0)

	/*
	 * Set this capability if host controller can retain the secure
	 * configuration even after UFS controller core power collapse.
	 */
	#define UFS_QCOM_CAP_RETAIN_SEC_CFG_AFTER_PWR_COLLAPSE	UFS_BIT(1)

	/*
	 * Set this capability if host controller supports Qunipro internal
	 * clock gating.
	 */
	#define UFS_QCOM_CAP_QUNIPRO_CLK_GATING		UFS_BIT(2)

	/*
	 * Set this capability if host controller supports SVS2 frequencies.
	 */
	#define UFS_QCOM_CAP_SVS2	UFS_BIT(3)
	u32 caps;

	struct phy *generic_phy;
	struct ufs_hba *hba;
	struct ufs_qcom_bus_vote bus_vote;
	struct ufs_pa_layer_attr dev_req_params;
	struct clk *rx_l0_sync_clk;
	struct clk *tx_l0_sync_clk;
	struct clk *rx_l1_sync_clk;
	struct clk *tx_l1_sync_clk;

	/* PM Quality-of-Service (QoS) data */
	struct ufs_qcom_pm_qos pm_qos;

	bool disable_lpm;
	bool is_lane_clks_enabled;
	bool sec_cfg_updated;

	void __iomem *dev_ref_clk_ctrl_mmio;
	bool is_dev_ref_clk_enabled;
	struct ufs_hw_version hw_ver;
#ifdef CONFIG_DEBUG_FS
	struct qcom_debugfs_files debugfs_files;
#endif

	u32 dev_ref_clk_en_mask;

	/* Bitmask for enabling debug prints */
	u32 dbg_print_en;
	struct ufs_qcom_testbus testbus;

	struct request *req_pending;
	struct ufs_vreg *vddp_ref_clk;
	struct ufs_vreg *vccq_parent;
	bool work_pending;
	bool is_phy_pwr_on;
};

static inline u32
ufs_qcom_get_debug_reg_offset(struct ufs_qcom_host *host, u32 reg)
{
	if (host->hw_ver.major <= 0x02)
		return UFS_CNTLR_2_x_x_VEN_REGS_OFFSET(reg);

	return UFS_CNTLR_3_x_x_VEN_REGS_OFFSET(reg);
};

#define ufs_qcom_is_link_off(hba) ufshcd_is_link_off(hba)
#define ufs_qcom_is_link_active(hba) ufshcd_is_link_active(hba)
#define ufs_qcom_is_link_hibern8(hba) ufshcd_is_link_hibern8(hba)

bool ufs_qcom_testbus_cfg_is_ok(struct ufs_qcom_host *host, u8 select_major,
		u8 select_minor);
int ufs_qcom_testbus_config(struct ufs_qcom_host *host);
void ufs_qcom_print_hw_debug_reg_all(struct ufs_hba *hba, void *priv,
		void (*print_fn)(struct ufs_hba *hba, int offset, int num_regs,
				char *str, void *priv));

static inline bool ufs_qcom_cap_qunipro(struct ufs_qcom_host *host)
{
	if (host->caps & UFS_QCOM_CAP_QUNIPRO)
		return true;
	else
		return false;
}

static inline bool ufs_qcom_cap_qunipro_clk_gating(struct ufs_qcom_host *host)
{
	return !!(host->caps & UFS_QCOM_CAP_QUNIPRO_CLK_GATING);
}

static inline bool ufs_qcom_cap_svs2(struct ufs_qcom_host *host)
{
	return !!(host->caps & UFS_QCOM_CAP_SVS2);
}

#endif /* UFS_QCOM_H_ */

/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef UFS_QCOM_H_
#define UFS_QCOM_H_

#include <linux/reset-controller.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>
#include <linux/pm_qos.h>
#include <linux/nvmem-consumer.h>
#include "ufshcd.h"
#include "unipro.h"

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

#define UFS_VENDOR_MICRON	0x12C

/* vendor specific pre-defined parameters */
#define UFS_HS_G4	4		/* HS Gear 4 */

#define SLOW 1
#define FAST 2

#define UFS_QCOM_PHY_SUBMODE_NON_G4	0
#define UFS_QCOM_PHY_SUBMODE_G4		1

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
#define UFS_QCOM_LIMIT_PHY_SUBMODE	UFS_QCOM_PHY_SUBMODE_G4
#define UFS_QCOM_DEFAULT_TURBO_FREQ     300000000
#define UFS_QCOM_DEFAULT_TURBO_L1_FREQ  300000000
#define UFS_NOM_THRES_FREQ	300000000

/* default value of auto suspend is 3 seconds */
#define UFS_QCOM_AUTO_SUSPEND_DELAY	3000
#define UFS_QCOM_CLK_GATING_DELAY_MS_PWR_SAVE	10
#define UFS_QCOM_CLK_GATING_DELAY_MS_PERF	50

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

/* QCOM UFS host controller vendor specific H8 count registers */
enum {
	REG_UFS_HW_H8_ENTER_CNT				= 0x2700,
	REG_UFS_SW_H8_ENTER_CNT				= 0x2704,
	REG_UFS_SW_AFTER_HW_H8_ENTER_CNT	= 0x2708,
	REG_UFS_HW_H8_EXIT_CNT				= 0x270C,
	REG_UFS_SW_H8_EXIT_CNT				= 0x2710,
};

#define UFS_CNTLR_2_x_x_VEN_REGS_OFFSET(x)	(0x000 + x)
#define UFS_CNTLR_3_x_x_VEN_REGS_OFFSET(x)	(0x400 + x)

/* bit definitions for REG_UFS_CFG1 register */
#define QUNIPRO_SEL		0x1
#define UTP_DBG_RAMS_EN		0x20000
#define TEST_BUS_EN		BIT(18)
#define TEST_BUS_SEL		GENMASK(22, 19)
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
#define PA_VS_CONFIG_REG1	0x9000
#define BIT_TX_EOB_COND         BIT(23)
#define PA_VS_CONFIG_REG2       0x9005
#define H8_ENTER_COND_OFFSET 0x6
#define H8_ENTER_COND_MASK GENMASK(7, 6)
#define BIT_RX_EOB_COND		BIT(5)
#define BIT_LINKCFG_WAIT_LL1_RX_CFG_RDY BIT(26)
#define SAVECONFIGTIME_MODE_MASK        0x6000
#define DME_VS_CORE_CLK_CTRL    0xD002


/* bit and mask definitions for DME_VS_CORE_CLK_CTRL attribute */
#define DME_VS_CORE_CLK_CTRL_CORE_CLK_DIV_EN_BIT		BIT(8)
#define DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_MASK	0xFF

#define PA_VS_CLK_CFG_REG	0x9004
#define PA_VS_CLK_CFG_REG_MASK	0x1FF
#define PA_VS_CLK_CFG_REG_MASK1 0xFF

#define DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_MASK_V4	0xFFF
#define DME_VS_CORE_CLK_CTRL_MAX_CORE_CLK_1US_CYCLES_OFFSET_V4	0x10

#define PA_VS_CLK_CFG_REG_MASK_TURBO 0x100
#define ATTR_HW_CGC_EN_TURBO 0x100
#define ATTR_HW_CGC_EN_NON_TURBO 0x000

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

#define TEST_BUS_CTRL_2_HCI_SEL_TURBO_MASK 0x010
#define TEST_BUS_CTRL_2_HCI_SEL_TURBO 0x010
#define TEST_BUS_CTRL_2_HCI_SEL_NONTURBO 0x000

/* Device Quirks */
/*
 * Some ufs devices may need more time to be in hibern8 before exiting.
 * Enable this quirk to give it an additional 100us.
 */
#define UFS_DEVICE_QUIRK_PA_HIBER8TIME          (1 << 15)

/*
 * Some ufs device vendors need a different TSync length.
 * Enable this quirk to give an additional TX_HS_SYNC_LENGTH.
 */
#define UFS_DEVICE_QUIRK_PA_TX_HSG1_SYNC_LENGTH (1 << 16)

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

struct gpio_desc;

struct qcom_bus_vectors {
	uint32_t ab;
	uint32_t ib;
};

struct qcom_bus_path {
	unsigned int num_paths;
	struct qcom_bus_vectors *vec;
};

struct qcom_bus_scale_data {
	struct qcom_bus_path *usecase;
	unsigned int num_usecase;
	struct icc_path *ufs_ddr;
	struct icc_path *cpu_ufs;

	const char *name;
};

struct qos_cpu_group {
	cpumask_t mask;
	unsigned int *votes;
	struct dev_pm_qos_request *qos_req;
	bool voted;
	struct work_struct vwork;
	struct ufs_qcom_host *host;
	unsigned int curr_vote;
	bool perf_core;
};

struct ufs_qcom_qos_req {
	struct qos_cpu_group *qcg;
	unsigned int num_groups;
	struct workqueue_struct *workq;
};

/* Check for QOS_POWER when added to DT */
enum constraint {
	QOS_PERF,
	QOS_POWER,
	QOS_MAX,
};

enum ufs_qcom_therm_lvl {
	UFS_QCOM_LVL_NO_THERM, /* No thermal mitigation */
	UFS_QCOM_LVL_AGGR_THERM, /* Aggressive thermal mitigation */
	UFS_QCOM_LVL_MAX_THERM, /* Max thermal mitigation */
};

struct ufs_qcom_thermal {
	struct thermal_cooling_device *tcd;
	unsigned long curr_state;
};

struct ufs_qcom_host {
	/*
	 * Set this capability if host controller supports the QUniPro mode
	 * and if driver wants the Host controller to operate in QUniPro mode.
	 * Note: By default this capability will be kept enabled if host
	 * controller supports the QUniPro mode.
	 */
	#define UFS_QCOM_CAP_QUNIPRO	0x1

	/*
	 * Set this capability if host controller can retain the secure
	 * configuration even after UFS controller core power collapse.
	 */
	#define UFS_QCOM_CAP_RETAIN_SEC_CFG_AFTER_PWR_COLLAPSE	0x2

	/*
	 * Set this capability if host controller supports Qunipro internal
	 * clock gating.
	 */
	#define UFS_QCOM_CAP_QUNIPRO_CLK_GATING		0x4

	/*
	 * Set this capability if host controller supports SVS2 frequencies.
	 */
	#define UFS_QCOM_CAP_SVS2	0x8

	u32 caps;

	struct phy *generic_phy;
	struct ufs_hba *hba;
	struct ufs_qcom_bus_vote bus_vote;
	struct ufs_pa_layer_attr dev_req_params;
	struct clk *rx_l0_sync_clk;
	struct clk *tx_l0_sync_clk;
	struct clk *rx_l1_sync_clk;
	struct clk *tx_l1_sync_clk;
	bool is_lane_clks_enabled;

	void __iomem *dev_ref_clk_ctrl_mmio;
	bool is_dev_ref_clk_enabled;
	struct ufs_hw_version hw_ver;
#ifdef CONFIG_SCSI_UFS_CRYPTO
	void __iomem *ice_mmio;
#endif
#if IS_ENABLED(CONFIG_QTI_HW_KEY_MANAGER)
	void __iomem *ice_hwkm_mmio;
#endif

	bool reset_in_progress;
	u32 dev_ref_clk_en_mask;

	/* Bitmask for enabling debug prints */
	u32 dbg_print_en;
	struct ufs_qcom_testbus testbus;

	/* Reset control of HCI */
	struct reset_control *core_reset;
	struct reset_controller_dev rcdev;

	struct gpio_desc *device_reset;

	int limit_tx_hs_gear;
	int limit_rx_hs_gear;
	int limit_tx_pwm_gear;
	int limit_rx_pwm_gear;
	int limit_rate;
	int limit_phy_submode;
	int ufs_dev_types;
	bool ufs_dev_revert;

	bool disable_lpm;
	struct qcom_bus_scale_data *qbsd;
	struct ufs_vreg *vddp_ref_clk;
	struct ufs_vreg *vccq_parent;
	bool work_pending;
	bool bypass_g4_cfgready;
	bool is_dt_pm_level_read;
	bool is_phy_pwr_on;
	/* Protect the usage of is_phy_pwr_on against racing */
	struct mutex phy_mutex;
	struct ufs_qcom_qos_req *ufs_qos;
	struct ufs_qcom_thermal uqt;
	/* FlashPVL entries */
	bool err_occurred;
	bool crash_on_err;
	atomic_t scale_up;
	atomic_t clks_on;
	unsigned long load_delay_ms;
#define NUM_REQS_HIGH_THRESH 64
#define NUM_REQS_LOW_THRESH 32
	atomic_t num_reqs_threshold;
	bool cur_freq_vote;
	struct delayed_work fwork;
	struct workqueue_struct *fworkq;
	struct mutex cpufreq_lock;
	bool cpufreq_dis;
	bool active;
	unsigned int min_cpu_scale_freq;
	unsigned int max_cpu_scale_freq;
	int config_cpu;
	void *ufs_ipc_log_ctx;
	bool dbg_en;
	struct nvmem_cell *nvmem_cell;

	/* Multi level clk scaling Support */
	bool ml_scale_sup;
	bool is_turbo_enabled;
	/* threshold count to scale down from turbo to NOM */
	u32 turbo_down_thres_cnt;
	/* turbo freq for UFS clocks read from DT */
	u32 axi_turbo_clk_freq;
	u32 axi_turbo_l1_clk_freq;
	u32 ice_turbo_clk_freq;
	u32 ice_turbo_l1_clk_freq;
	u32 unipro_turbo_clk_freq;
	u32 unipro_turbo_l1_clk_freq;
	bool turbo_unipro_attr_applied;
	/* some target need additional setting to support turbo mode*/
	bool turbo_additional_conf_req;
	/* current UFS clocks freq */
	u32 curr_axi_freq;
	u32 curr_ice_freq;
	u32 curr_unipro_freq;
	/* Indicates curr and next clk mode */
	u32 clk_next_mode;
	u32 clk_curr_mode;
	bool is_clk_scale_enabled;
	atomic_t hi_pri_en;
	atomic_t therm_mitigation;
	cpumask_t perf_mask;
	cpumask_t def_mask;
	bool irq_affinity_support;
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

int ufs_qcom_testbus_config(struct ufs_qcom_host *host);
void ufs_qcom_print_hw_debug_reg_all(struct ufs_hba *hba, void *priv,
		void (*print_fn)(struct ufs_hba *hba, int offset, int num_regs,
				const char *str, void *priv));

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

/**
 * ufshcd_dme_rmw - get modify set a dme attribute
 * @hba - per adapter instance
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @attr - dme attribute
 */
static inline int ufshcd_dme_rmw(struct ufs_hba *hba, u32 mask,
				 u32 val, u32 attr)
{
	u32 cfg = 0;
	int err = 0;

	err = ufshcd_dme_get(hba, UIC_ARG_MIB(attr), &cfg);
	if (err)
		goto out;

	cfg &= ~mask;
	cfg |= (val & mask);

	err = ufshcd_dme_set(hba, UIC_ARG_MIB(attr), cfg);

out:
	return err;
}

/*
 *  IOCTL opcode for ufs queries has the following opcode after
 *  SCSI_IOCTL_GET_PCI
 */
#define UFS_IOCTL_QUERY			0x5388

/**
 * struct ufs_ioctl_query_data - used to transfer data to and from user via
 * ioctl
 * @opcode: type of data to query (descriptor/attribute/flag)
 * @idn: id of the data structure
 * @buf_size: number of allocated bytes/data size on return
 * @buffer: data location
 *
 * Received: buffer and buf_size (available space for transferred data)
 * Submitted: opcode, idn, length, buf_size
 */
struct ufs_ioctl_query_data {
	/*
	 * User should select one of the opcode defined in "enum query_opcode".
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 * Note that only UPIU_QUERY_OPCODE_READ_DESC,
	 * UPIU_QUERY_OPCODE_READ_ATTR & UPIU_QUERY_OPCODE_READ_FLAG are
	 * supported as of now. All other query_opcode would be considered
	 * invalid.
	 * As of now only read query operations are supported.
	 */
	__u32 opcode;
	/*
	 * User should select one of the idn from "enum flag_idn" or "enum
	 * attr_idn" or "enum desc_idn" based on whether opcode above is
	 * attribute, flag or descriptor.
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 */
	__u8 idn;
	/*
	 * User should specify the size of the buffer (buffer[0] below) where
	 * it wants to read the query data (attribute/flag/descriptor).
	 * As we might end up reading less data then what is specified in
	 * buf_size. So we are updating buf_size to what exactly we have read.
	 */
	__u16 buf_size;
	/*
	 * placeholder for the start of the data buffer where kernel will copy
	 * the query data (attribute/flag/descriptor) read from the UFS device
	 * Note:
	 * For Read/Write Attribute you will have to allocate 4 bytes
	 * For Read/Write Flag you will have to allocate 1 byte
	 */
	__u8 buffer[0];
};

/* ufs-qcom-ice.c */

#ifdef CONFIG_SCSI_UFS_CRYPTO
int ufs_qcom_ice_init(struct ufs_qcom_host *host);
int ufs_qcom_ice_enable(struct ufs_qcom_host *host);
int ufs_qcom_ice_resume(struct ufs_qcom_host *host);
int ufs_qcom_ice_program_key(struct ufs_hba *hba,
			     const union ufs_crypto_cfg_entry *cfg, int slot);
void ufs_qcom_ice_disable(struct ufs_qcom_host *host);
#else
static inline int ufs_qcom_ice_init(struct ufs_qcom_host *host)
{
	return 0;
}
static inline int ufs_qcom_ice_enable(struct ufs_qcom_host *host)
{
	return 0;
}
static inline int ufs_qcom_ice_resume(struct ufs_qcom_host *host)
{
	return 0;
}
static inline void ufs_qcom_ice_disable(struct ufs_qcom_host *host)
{
	return 0;
}
#define ufs_qcom_ice_program_key NULL
#endif /* !CONFIG_SCSI_UFS_CRYPTO */

#endif /* UFS_QCOM_H_ */

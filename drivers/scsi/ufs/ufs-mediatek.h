/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_H
#define _UFS_MEDIATEK_H

#include <linux/bitops.h>
#include <linux/pm_qos.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>

#include "ufshcd.h"

/*
 * Vendor specific UFSHCI Registers
 */
#define REG_UFS_REFCLK_CTRL         0x144
#define REG_UFS_EXTREG              0x2100
#define REG_UFS_MPHYCTRL            0x2200
#define REG_UFS_REJECT_MON          0x22AC
#define REG_UFS_DEBUG_SEL           0x22C0
#define REG_UFS_PROBE               0x22C8

/*
 * Ref-clk control
 *
 * Values for register REG_UFS_REFCLK_CTRL
 */
#define REFCLK_RELEASE              0x0
#define REFCLK_REQUEST              BIT(0)
#define REFCLK_ACK                  BIT(1)

#define REFCLK_REQ_TIMEOUT_US       3000

/*
 * Vendor specific pre-defined parameters
 */
#define UFS_MTK_LIMIT_NUM_LANES_RX  2
#define UFS_MTK_LIMIT_NUM_LANES_TX  2
#define UFS_MTK_LIMIT_HSGEAR_RX     UFS_HS_G4
#define UFS_MTK_LIMIT_HSGEAR_TX     UFS_HS_G4
#define UFS_MTK_LIMIT_PWMGEAR_RX    UFS_PWM_G4
#define UFS_MTK_LIMIT_PWMGEAR_TX    UFS_PWM_G4
#define UFS_MTK_LIMIT_RX_PWR_PWM    SLOW_MODE
#define UFS_MTK_LIMIT_TX_PWR_PWM    SLOW_MODE
#define UFS_MTK_LIMIT_RX_PWR_HS     FAST_MODE
#define UFS_MTK_LIMIT_TX_PWR_HS     FAST_MODE
#define UFS_MTK_LIMIT_HS_RATE       PA_HS_MODE_B
#define UFS_MTK_LIMIT_DESIRED_MODE  UFS_HS_MODE

/*
 * Other attributes
 */
#define VS_DEBUGCLOCKENABLE         0xD0A1
#define VS_SAVEPOWERCONTROL         0xD0A6
#define VS_UNIPROPOWERDOWNCONTROL   0xD0A8

/*
 * Vendor specific link state
 */
enum {
	VS_LINK_DISABLED            = 0,
	VS_LINK_DOWN                = 1,
	VS_LINK_UP                  = 2,
	VS_LINK_HIBERN8             = 3,
	VS_LINK_LOST                = 4,
	VS_LINK_CFG                 = 5,
};

/*
 * SiP commands
 */
#define MTK_SIP_UFS_CONTROL               MTK_SIP_SMC_CMD(0x276)
#define UFS_MTK_SIP_VA09_PWR_CTRL         BIT(0)
#define UFS_MTK_SIP_DEVICE_RESET          BIT(1)
#define UFS_MTK_SIP_CRYPTO_CTRL           BIT(2)
#define UFS_MTK_SIP_REF_CLK_NOTIFICATION  BIT(3)

/*
 * Quirks
 */
enum ufs_mtk_host_quirks {
	/*
	 * Auto-hibern8 shall be disabled while doorbell is not empty
	 */
	UFS_MTK_HOST_QUIRK_BROKEN_AUTO_HIBERN8 = BIT(0),

	/*
	 * This quirk needs to be enabled if we apply performance heuristic
	 * to UFS host.
	 */
	UFS_MTK_HOST_QUIRK_UFS_HCI_PERF_HEURISTIC = BIT(1),

	/*
	 * This quirk needs to be enabled if VCC drop slow
	 */
	UFS_MTK_HOST_QUIRK_UFS_VCC_ALWAYS_ON = BIT(2),
};

/*
 * VS_DEBUGCLOCKENABLE
 */
enum {
	TX_SYMBOL_CLK_REQ_FORCE = 5,
};

/*
 * VS_SAVEPOWERCONTROL
 */
enum {
	RX_SYMBOL_CLK_GATE_EN   = 0,
	SYS_CLK_GATE_EN         = 2,
	TX_CLK_GATE_EN          = 3,
};

/*
 * Ref-clk control mode
 */
enum {
	REF_CLK_SW_MODE         = 0,
	REF_CLK_HALF_HW_MODE    = 1,
	REF_CLK_HW_MODE         = 2
};

/*
 * perf control mode
 */
enum perf_mode {
	PERF_FORCE_DISABLE   = 0,
	PERF_FORCE_ENABLE    = 1,
	PERF_AUTO            = 2,
};

struct ufs_mtk_host_cfg {
	enum ufs_mtk_host_quirks quirks;
};

struct ufs_mtk_hw_ver {
	u8 step;
	u8 minor;
	u8 major;
};

struct ufs_mtk_host {
	struct ufs_mtk_host_cfg *cfg;
	struct ufs_hba *hba;
	struct phy *mphy;
	struct regulator *reg_va09;
	struct reset_control *hci_reset;
	struct reset_control *unipro_reset;
	struct reset_control *crypto_reset;
	struct ufs_mtk_hw_ver hw_ver;
	bool mphy_powered_on;
	bool unipro_lpm;
	bool ref_clk_enabled;
	bool auto_hibern_enabled;
	u16 ref_clk_ungating_wait_us;
	u16 ref_clk_gating_wait_us;
	u32 refclk_ctrl;

	bool pm_qos_init;
	struct pm_qos_request req_cpu_dma_latency;

	/* performance mode */
	enum perf_mode perf_mode;
	bool perf_enable;
	int crypto_vcore_opp;
	struct clk *crypto_clk_mux;
	struct clk *crypto_parent_clk_normal;
	struct clk *crypto_parent_clk_perf;
	struct mtk_pm_qos_request *req_vcore;

	/* vufs regulator lpm */
	bool vreg_lpm_supported;

	/*
	 * RPMB device
	 */
	struct scsi_device *sdev_ufs_rpmb;
	struct rpmb_dev *rawdev_ufs_rpmb;
	struct mutex rpmb_lock;

	bool qos_allowed;
	bool qos_enabled;
};

struct ufs_hba *ufs_mtk_get_hba(void);

struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev(void);
void ufs_mtk_rpmb_add(struct ufs_hba *hba, struct scsi_device *sdev_rpmb);
void ufs_mtk_rpmb_remove(struct ufs_hba *hba);
void ufs_mtk_rpmb_quiesce(struct ufs_hba *hba);
int ufs_mtk_ioctl_rpmb(struct ufs_hba *hba, const void __user *buf_user);
bool ufs_mtk_perf_is_supported(struct ufs_mtk_host *host);
int ufs_mtk_perf_setup_crypto_clk(struct ufs_mtk_host *host, bool perf);
int ufs_mtk_perf_heurisic_if_allow_cmd(struct ufs_hba *hba, struct scsi_cmnd *cmd);
void ufs_mtk_perf_heurisic_req_done(struct ufs_hba *hba, struct scsi_cmnd *cmd);

#endif /* !_UFS_MEDIATEK_H */

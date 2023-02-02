/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_H
#define _UFS_MEDIATEK_H

#include <linux/bitops.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/pm_qos.h>

#ifdef CONFIG_UFSFEATURE
#include "ufshcd.h"
#include "ufsfeature.h"
#endif

/* UFSHCD error handling flags */
enum {
	UFSHCD_EH_IN_PROGRESS = (1 << 0),
};

#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)

/*
 * Vendor specific UFSHCI Registers
 */
#define REG_UFS_XOUFS_CTRL          0x140
#define REG_UFS_REFCLK_CTRL         0x144
#define REG_UFS_EXTREG              0x2100
#define REG_UFS_MPHYCTRL            0x2200
#define REG_UFS_MTK_HW_VER          0x2240
#define REG_UFS_REJECT_MON          0x22AC
#define REG_UFS_DEBUG_SEL           0x22C0
#define REG_UFS_PROBE               0x22C8
#define REG_UFS_DEBUG_SEL_B0        0x22D0
#define REG_UFS_DEBUG_SEL_B1        0x22D4
#define REG_UFS_DEBUG_SEL_B2        0x22D8
#define REG_UFS_DEBUG_SEL_B3        0x22DC

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
 * Vendor specific host controller state
 */
enum {
	VS_HCE_RESET                = 0,
	VS_HCE_BASE                 = 1,
	VS_HCE_OOCPR_WAIT           = 2,
	VS_HCE_DME_RESET            = 3,
	VS_HCE_MIDDLE               = 4,
	VS_HCE_DME_ENABLE           = 5,
	VS_HCE_DEFAULTS             = 6,
	VS_HIB_IDLEEN               = 7,
	VS_HIB_ENTER                = 8,
	VS_HIB_ENTER_CONF           = 9,
	VS_HIB_MIDDLE               = 10,
	VS_HIB_WAITTIMER            = 11,
	VS_HIB_EXIT_CONF            = 12,
	VS_HIB_EXIT                 = 13,
};

/*
 * SiP commands
 */
#define MTK_SIP_UFS_CONTROL               MTK_SIP_SMC_CMD(0x276)
#define UFS_MTK_SIP_VA09_PWR_CTRL         BIT(0)
#define UFS_MTK_SIP_DEVICE_RESET          BIT(1)
#define UFS_MTK_SIP_CRYPTO_CTRL           BIT(2)
#define UFS_MTK_SIP_REF_CLK_NOTIFICATION  BIT(3)
#define UFS_MTK_SIP_HOST_PWR_CTRL         BIT(5)
#define UFS_MTK_SIP_GET_VCC_INFO          BIT(6)
#define UFS_MTK_SIP_DEVICE_PWR_CTRL       BIT(7)

/*
 * SMC call wapper function
 */
#define _ufs_mtk_smc(cmd, res, v1, v2, v3, v4, v5, v6) \
		arm_smccc_smc(MTK_SIP_UFS_CONTROL, \
				  cmd, v1, v2, v3, v4, v5, v6, &(res))

#define _ufs_mtk_smc_0(cmd, res) \
	_ufs_mtk_smc(cmd, res, 0, 0, 0, 0, 0, 0)

#define _ufs_mtk_smc_1(cmd, res, v1) \
	_ufs_mtk_smc(cmd, res, v1, 0, 0, 0, 0, 0)

#define _ufs_mtk_smc_2(cmd, res, v1, v2) \
	_ufs_mtk_smc(cmd, res, v1, v2, 0, 0, 0, 0)

#define _ufs_mtk_smc_3(cmd, res, v1, v2, v3) \
	_ufs_mtk_smc(cmd, res, v1, v2, v3, 0, 0, 0)

#define _ufs_mtk_smc_4(cmd, res, v1, v2, v3, v4) \
	_ufs_mtk_smc(cmd, res, v1, v2, v3, v4, 0, 0)

#define _ufs_mtk_smc_5(cmd, res, v1, v2, v3, v4, v5) \
	_ufs_mtk_smc(cmd, res, v1, v2, v3, v4, v5, 0)

#define _ufs_mtk_smc_6(cmd, res, v1, v2, v3, v4, v5, v6) \
	_ufs_mtk_smc(cmd, res, v1, v2, v3, v4, v5, v6)

#define _ufs_mtk_smc_selector(cmd, res, v1, v2, v3, v4, v5, v6, FUNC, ...) FUNC

#define ufs_mtk_smc(...) \
	_ufs_mtk_smc_selector(__VA_ARGS__, \
	_ufs_mtk_smc_6(__VA_ARGS__), \
	_ufs_mtk_smc_5(__VA_ARGS__), \
	_ufs_mtk_smc_4(__VA_ARGS__), \
	_ufs_mtk_smc_3(__VA_ARGS__), \
	_ufs_mtk_smc_2(__VA_ARGS__), \
	_ufs_mtk_smc_1(__VA_ARGS__), \
	_ufs_mtk_smc_0(__VA_ARGS__) \
	)

/* Sip UFS GET VCC INFO */
enum {
	VCC_NONE = 0,
	VCC_1,
	VCC_2
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
 * Host capability
 */
enum ufs_mtk_host_caps {
	UFS_MTK_CAP_BOOST_CRYPT_ENGINE         = 1 << 0,
	UFS_MTK_CAP_VA09_PWR_CTRL              = 1 << 1,
	UFS_MTK_CAP_DISABLE_AH8                = 1 << 2,
	UFS_MTK_CAP_BROKEN_VCC                 = 1 << 3,
	UFS_MTK_CAP_DEALY_AFTER_VCC_OFF        = 1 << 4,

	/* Override UFS_MTK_CAP_BROKEN_VCC's behavior to
	 * allow vccqx upstream to enter LPM
	 */
	UFS_MTK_CAP_FORCE_VSx_LPM              = 1 << 5,
};

struct ufs_mtk_crypt_cfg {
	struct regulator *reg_vcore;
	struct clk *clk_crypt_perf;
	struct clk *clk_crypt_mux;
	struct clk *clk_crypt_lp;
	int vcore_volt;
};

struct ufs_mtk_hw_ver {
	u8 step;
	u8 minor;
	u8 major;
};

struct ufs_mtk_host {
	struct phy *mphy;
	struct regulator *reg_va09;
	struct reset_control *hci_reset;
	struct reset_control *unipro_reset;
	struct reset_control *crypto_reset;
	struct ufs_hba *hba;
	struct ufs_mtk_crypt_cfg *crypt;
	struct ufs_mtk_hw_ver hw_ver;
	enum ufs_mtk_host_caps caps;
	bool mphy_powered_on;
	bool unipro_lpm;
	bool ref_clk_enabled;
	u16 ref_clk_ungating_wait_us;
	u16 ref_clk_gating_wait_us;
	bool pm_qos_init;
	struct pm_qos_request pm_qos_req;
	bool qos_allowed;
	bool qos_enabled;
	bool boot_device;
	struct ufs_vreg *vcc;

	struct completion luns_added;

	struct semaphore rpmb_sem;
#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature ufsf;
#endif
};

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

enum {
	BOOTDEV_SDMMC = 1,
	BOOTDEV_UFS   = 2
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

#if IS_ENABLED(CONFIG_RPMB)
struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev(void);
#endif

#if defined(CONFIG_UFSFEATURE)
static inline struct ufsf_feature *ufs_mtk_get_ufsf(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return &host->ufsf;
}
#endif
#endif /* !_UFS_MEDIATEK_H */

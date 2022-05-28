/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6985_APUPWR_H__
#define __MT6985_APUPWR_H__

#include <linux/io.h>
#include <linux/clk.h>

#define APU_POWER_INIT		(0)	// 1: init in kernel ; 0: init in lk2
#define APU_POWER_BRING_UP	(0)
#define APU_PWR_SOC_PATH	(0)	// 1: do not run apu pll/acc init
#define ENABLE_SW_BUCK_CTL	(0)	// 1: enable regulator in rpm resume
#define ENABLE_SOC_CLK_MUX	(0)	// 1: enable soc clk in rpm resume
#define DEBUG_DUMP_REG		(0)	// dump overall apu registers for debug
#define APMCU_REQ_RPC_SLEEP	(0)	// rpm suspend trigger sleep req to rpc
#define APUPW_DUMP_FROM_APMCU	(0)	// 1: dump reg from APMCU, 0: from ATF
#define SUPPORT_VSRAM_0P75_VB	(1)

#define VAPU_DEF_VOLT		(750000)	// 0.75v
#define USER_MAX_OPP_VAL	(0) // fastest speed user can specify
#define USER_MIN_OPP_VAL	(11) // slowest speed user can specify
#define TURBO_BOOST_OPP		USER_MAX_OPP_VAL
#define TURBO_BOOST_VAL		(110)
#define MTK_POLL_DELAY_US	(10)
#define MTK_POLL_TIMEOUT	USEC_PER_SEC
#define HW_SEMA_TIMEOUT_CNT	(7) // 7 * 10 = 70 us

enum smc_rcx_pwr_op {
	SMC_RCX_PWR_AFC_EN = 0,
	SMC_RCX_PWR_WAKEUP_RPC,
	SMC_RCX_PWR_CG_EN,
	SMC_RCX_PWR_HW_SEMA,
	SMC_HW_SEMA_PWR_CTL_LOCK,
	SMC_HW_SEMA_PWR_CTL_UNLOCK,
};

enum smc_pwr_dump {
	SMC_PWR_DUMP_RPC = 0,
	SMC_PWR_DUMP_PCU,
	SMC_PWR_DUMP_ARE,
	SMC_PWR_DUMP_ALL,
};

enum t_acx_id {
	ACX0 = 0,
	ACX1,
	NCX,
	CLUSTER_NUM,
	RCX,
};

enum t_dev_id {
	VPU0 = 0,
	DLA0,
	DLA1,
	APS,
	DEVICE_NUM,
};

enum apu_clksrc_id {
	PLL_CONN = 0, // MNOC
	PLL_UP,
	PLL_VPU,
	PLL_DLA,
	PLL_APS,
	PLL_NUM,
};

enum apu_buck_id {
	BUCK_VAPU = 0,
	BUCK_VSRAM,
	BUCK_VCORE,
	BUCK_NUM,
};

enum apupw_reg {
	sys_vlp,
	sys_spm,
	apu_rcx,
	apu_vcore,
	apu_md32_mbox,
	apu_rpc, /* 5 */
	apu_pcu,
	apu_ao_ctl,
	apu_pll,
	apu_acc,
	apu_are, /* 0xa */
	apu_acx0,
	apu_acx0_rpc_lite, /* 0xc */
	apu_acx1,
	apu_acx1_rpc_lite, /* 0xe */
	apu_ncx,
	apu_ncx_rpc_lite, /* 0x10 */
	APUPW_MAX_REGS,
};

enum mode {
	FPGA,
	AO,
	MP,
};

enum pwr_on {
	RPC_HW,
	CE_FW,
};

struct apu_power {
	void __iomem *regs[APUPW_MAX_REGS];
	unsigned int phy_addr[APUPW_MAX_REGS];
	enum mode env;
	enum pwr_on rcx;
};

struct rpc_status_dump {
	uint32_t rpc_reg_status;
	uint32_t conn_reg_status;
	uint32_t vcore_reg_status;	// rpc_lite bypss this
};

/*
 * Only for Apusys 6.0 PLL/ACC initialize
 * and need to sync with LK2 setting
 */
enum rcx_ao_range {
RCX_AO_BEGIN = 0,
	PLL_ENTRY_BEGIN = 0, //5(pll)*6(steps) = 30
	PLL_ENTRY_END = 29,
	ACC_ENTRY_BEGIN = 30, //21 ARE entries, ARDCM(8) + ACC(4+9=13)
	ACC_ENTRY_END = 50,
	MBX_DBG = 51,
RCX_AO_END = 51,
};

/* SW ARE entry i = (HW are entry i) + (HW are entry i+1) */
#define ARE_ENTRIES(x, y) ((((y) - (x)) + 1) * 2)
#define ARE_ENTRY(x) (((x) * 2) + 16)
#define ARE_RCX_AO_CONFIG    0x0008

void mt6985_apu_dump_rpc_status(enum t_acx_id id, struct rpc_status_dump *dump);

/* RPC offset define */
#define APU_RPC_TOP_CON           0x0000
#define APU_RPC_TOP_SEL           0x0004
#define APU_RPC_SW_FIFO_WE        0x0008
#define APU_RPC_IO_DEBUG          0x000C
#define APU_RPC_STATUS            0x0014
#define APU_RPC_TOP_SEL_1         0x0018
#define APU_RPC_HW_CON            0x001C
#define APU_RPC_LITE_CON          0x0020
#define APU_RPC_HW_CON1           0x0030
#define APU_RPC_INTF_PWR_RDY_REG  0x0040
#define APU_RPC_INTF_PWR_RDY      0x0044
#define APU_RPC_MTCMOS_SW_CTRL0   0x0140

#define RPC_TOP_SEL_HW_DEF	(0x012b0000) // cfg in hw default
#define RPC_TOP_SEL_SW_CFG1	(0x1800531e) // cfg in cold boot
#define RPC_TOP_SEL_SW_CFG2	(0x192b531e) // cfg in warm boot

/* APU PLL1U offset define */
#define MDLA_PLL_BASE       0x000 // 0x190F3000
#define MVPU_PLL_BASE       0x400 // 0x190F3400
#define MNOC_PLL_BASE       0x800 // 0x190F3800
#define UP_PLL_BASE         0xC00 // 0x190F3C00
#define APS_PLL_BASE		0x1000 // 0x190F4000

/* APU ACCGRP offset define */
#define APU_ACCGRP_0_BASE         0x000  // mdla:0x190F3000
#define APU_ACCGRP_1_BASE         0x400  // vmpu:0x190F3400
#define APU_ACCGRP_2_BASE         0x800  // mnoc:0x190F3800
#define APU_ACCGRP_3_BASE         0xC00  // up:0x190F3C00
#define APU_ACCGRP_4_BASE         0x1000 // nve:0x190F4000

#define MDLA_ACC_BASE       APU_ACCGRP_0_BASE
#define MVPU_ACC_BASE       APU_ACCGRP_1_BASE
#define MNOC_ACC_BASE       APU_ACCGRP_2_BASE
#define UP_ACC_BASE         APU_ACCGRP_3_BASE
#define APS_ACC_BASE        APU_ACCGRP_4_BASE

// ACC offset
#define APU_ACC_CONFG_SET0        0x000
#define APU_ACC_CONFG_CLR0        0x010
#define APU_ACC_FM_CONFG_SET      0x020
#define APU_ACC_FM_CONFG_CLR      0x024
#define APU_ACC_FM_SEL            0x028
#define APU_ACC_FM_CNT            0x02C
#define APU_ACC_AUTO_CONFG0       0x080
#define APU_ACC_AUTO_CTRL_SET0    0x084
#define APU_ACC_AUTO_CTRL_CLR0    0x088
#define APU_ACC_AUTO_STATUS0      0x08C
#define APU_ARDCM_CTRL0           0x100
#define APU_ARDCM_CTRL1           0x104

// APU PLL1C offset
#define PLL1C_PLL1_CON1           0x20C
#define PLL1CPLL_FHCTL_HP_EN      0x300
#define PLL1CPLL_FHCTL_CLK_CON    0x308
#define PLL1CPLL_FHCTL_RST_CON    0x30C
#define PLL1CPLL_FHCTL0_CFG       0x314
#define PLL1CPLL_FHCTL0_DDS       0x31C

/* ARE offset define */
#define APU_ARE_INI_CTRL        0x0000

/* vcore offset define */
#define APUSYS_VCORE_CG_CON     0x0000
#define APUSYS_VCORE_CG_SET     0x0004
#define APUSYS_VCORE_CG_CLR     0x0008
#define APUSYS_VCORE_SW_RST     0x000C

/* APU_ARE_REG */
#define APU_ARE_GCONFIG         0x3000
#define APU_ARE_STATUS          0x3010
#define APU_CE_IF_PC            0x3420

/* rcx offset define */
#define APU_RCX_CG_CON          0x0000
#define APU_RCX_CG_SET          0x0004
#define APU_RCX_CG_CLR          0x0008
#define APU_RCX_SW_RST          0x000C

/* acx 0/1 offset define */
#define APU_ACX_CONN_CG_CON     0x3C000
#define APU_ACX_CONN_CG_CLR     0x3C008
#define APU_NCX_CONN_CG_CON     0x3F000
#define APU_NCX_CONN_CG_CLR     0x3F008
#define APU_ACX_MVPU_CG_CON     0x2B000
#define APU_ACX_MVPU_CG_CLR     0x2B008
#define APU_ACX_MVPU_SW_RST     0x2B00C
#define APU_ACX_MVPU_RV55_CTRL0 0x2B018
#define APU_ACX_MDLA0_CG_CON    0x30000
#define APU_ACX_MDLA0_CG_CLR    0x30008
#define APU_ACX_MDLA1_CG_CON    0x34000
#define APU_ACX_MDLA1_CG_CLR    0x34008
#define APU_NCX_APS_CG_CON      0x3E000
#define APU_NCX_APS_CG_CLR      0x3E008

// vlp offset define
#define APUSYS_AO_CTRL_ADDR   (0x200)
#define APUSYS_AO_SRAM_CONFIG (0x70)
#define APUSYS_AO_SRAM_SET    (0x74)
#define APUSYS_AO_SRAM_CLR    (0x78)

// spm offset define
#define APUSYS_BUCK_ISOLATION		(0x39C)
#define SPM_SEMA_M0			(0x69C)
#define SPM_HW_SEMA_MASTER		SPM_SEMA_M0

// PCU initial data
#define APU_PCUTOP_CTRL_SET	0x0

// mt6319GP_buck1 (in mt6985 for vapu)
#define MT6319_SLAVE_ID         (0xF)
#define TOP_VRCTL_VR0_EN                                0x1140
#define TOP_VRCTL_VR0_EN_SET                            0x1141
#define TOP_VRCTL_VR0_EN_CLR                            0x1142
#define MT6319_RG_BUCK_VBUCK1_SET  TOP_VRCTL_VR0_EN_SET
#define MT6319_RG_BUCK_VBUCK1_CLR  TOP_VRCTL_VR0_EN_CLR
#define MT6319_RG_BUCK_VBUCK1_EN_SHIFT             (0)
#define MT6319_RG_BUCK_VBUCK1_VOSEL_ADDR                0x1449

// mt6363 (in mt6985 for vsram/vcore)
#define MT6363_SLAVE_ID         (0x4)
// vcore: mt6363_vbuck2
#define MT6363_RG_BUCK_VBUCK6_EN_ADDR                   0x240
#define MT6363_RG_BUCK_VBUCK6_EN_SHIFT                  (6)
#define MT6363_RG_BUCK_VBUCK6_VOSEL_ADDR                0x252
// sram_core: mt6363_vbuck4
#define MT6363_RG_BUCK_VBUCK4_EN_ADDR                   0x240
#define MT6363_RG_BUCK_VBUCK4_EN_SHIFT                  (4)
#define MT6363_RG_BUCK_VBUCK4_VOSEL_ADDR                0x250

// sub_pmic
#define SUB_PMIC_ID                     MT6319_SLAVE_ID
#define BUCK_VAPU_PMIC_REG              MT6319_RG_BUCK_VBUCK1_VOSEL_ADDR
#define BUCK_VAPU_PMIC_REG_EN_SET_ADDR  MT6319_RG_BUCK_VBUCK1_SET
#define BUCK_VAPU_PMIC_REG_EN_CLR_ADDR  MT6319_RG_BUCK_VBUCK1_CLR
#define BUCK_VAPU_PMIC_REG_EN_SHIFT     MT6319_RG_BUCK_VBUCK1_EN_SHIFT

#define APU_PCU_BUCK_STEP_SEL		0x0030
#define APU_PCU_BUCK_ON_DAT0_L		0x0080
#define APU_PCU_BUCK_ON_DAT0_H		0x0084
#define APU_PCU_BUCK_OFF_DAT0_L		0x00A0
#define APU_PCU_BUCK_OFF_DAT0_H		0x00A4
#define APU_PCU_BUCK_ON_SLE0		0x00C0
#define VAPU_BUCK_ON_SETTLE_TIME	0x12C

// apu hw sema (in PCU)
#define APU_PCU_SEMA_CTRL0             0x0200
#define APU_HW_SEMA_PWR_CTL            APU_PCU_SEMA_CTRL0

int mt6985_all_on(struct platform_device *pdev, struct apu_power *papw);
void mt6985_all_off(struct platform_device *pdev);
#endif // __mt6985_APUPWR_H__

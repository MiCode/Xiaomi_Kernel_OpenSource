/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6983_APUPWR_H__
#define __MT6983_APUPWR_H__

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
#define USER_MIN_OPP_VAL	(8) // slowest speed user can specify
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
	CLUSTER_NUM,
	RCX,
};

enum t_dev_id {
	VPU0 = 0,
	DLA0,
	DLA1,
	DEVICE_NUM,
};

enum apu_clksrc_id {
	PLL_CONN = 0, // MNOC
	PLL_UP,
	PLL_VPU,
	PLL_DLA,
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
	apu_rpc,
	apu_pcu,
	apu_ao_ctl,
	apu_pll,
	apu_acc,
	apu_are0,
	apu_are1,
	apu_are2,
	apu_acx0,
	apu_acx0_rpc_lite,
	apu_acx1,
	apu_acx1_rpc_lite,
	APUPW_MAX_REGS,
};

struct apu_power {
	void __iomem *regs[APUPW_MAX_REGS];
	unsigned int phy_addr[APUPW_MAX_REGS];
};

struct rpc_status_dump {
	uint32_t rpc_reg_status;
	uint32_t conn_reg_status;
	uint32_t vcore_reg_status;	// rpc_lite bypss this
};

void mt6983_apu_dump_rpc_status(enum t_acx_id id, struct rpc_status_dump *dump);

/* RPC offset define */
#define APU_RPC_TOP_CON           0x0000
#define APU_RPC_TOP_SEL           0x0004
#define APU_RPC_SW_FIFO_WE        0x0008
#define APU_RPC_IO_DEBUG          0x000C
#define APU_RPC_STATUS            0x0014
#define APU_RPC_TOP_SEL_1         0x0018
#define APU_RPC_HW_CON            0x001C
#define APU_RPC_LITE_CON          0x0020
#define APU_RPC_INTF_PWR_RDY_REG  0x0040
#define APU_RPC_INTF_PWR_RDY      0x0044
#define RPC_TOP_SEL_HW_DEF	(0x012b0000) // cfg in hw default
#define RPC_TOP_SEL_SW_CFG1	(0x1800531e) // cfg in cold boot
#define RPC_TOP_SEL_SW_CFG2	(0x192b531e) // cfg in warm boot

/* APU PLL1U offset define */
#define MDLA_PLL_BASE		0x000 // 0x190F3000
#define MVPU_PLL_BASE		0x400 // 0x190F3400
#define MNOC_PLL_BASE		0x800 // 0x190F3800
#define UP_PLL_BASE		0xC00 // 0x190F3C00

#define PLL1U_PLL1_CON1		0x00C
#define PLL1UPLL_FHCTL_HP_EN	0x100
#define PLL1UPLL_FHCTL_CLK_CON	0x108
#define PLL1UPLL_FHCTL_RST_CON	0x10C
#define PLL1UPLL_FHCTL0_CFG	0x114
#define PLL1UPLL_FHCTL0_DDS	0x11C


/* ACC offset define */
#define APU_ACC_CONFG_SET0      0x0000
#define APU_ACC_CONFG_SET1      0x0004
#define APU_ACC_CONFG_SET2      0x0008
#define APU_ACC_CONFG_SET3      0x000C
#define APU_ACC_CONFG_CLR0      0x0040
#define APU_ACC_CONFG_CLR1      0x0044
#define APU_ACC_CONFG_CLR2      0x0048
#define APU_ACC_CONFG_CLR3      0x004C
#define APU_ACC_FM_CONFG_SET    0x00C0
#define APU_ACC_FM_CONFG_CLR    0x00C4
#define APU_ACC_FM_SEL          0x00C8
#define APU_ACC_FM_CNT          0x00CC
#define APU_ACC_CLK_EN_SET      0x00E0
#define APU_ACC_CLK_EN_CLR      0x00E4
#define APU_ACC_CLK_INV_EN_SET  0x00E8
#define APU_ACC_CLK_INV_EN_CLR  0x00EC
#define APU_ACC_AUTO_CONFG0     0x0100
#define APU_ACC_AUTO_CONFG1     0x0104
#define APU_ACC_AUTO_CONFG2     0x0108
#define APU_ACC_AUTO_CONFG3     0x010C
#define APU_ACC_AUTO_CTRL_SET0  0x0120
#define APU_ACC_AUTO_CTRL_SET1  0x0124
#define APU_ACC_AUTO_CTRL_SET2  0x0128
#define APU_ACC_AUTO_CTRL_SET3  0x012C
#define APU_ACC_AUTO_CTRL_CLR0  0x0140
#define APU_ACC_AUTO_CTRL_CLR1  0x0144
#define APU_ACC_AUTO_CTRL_CLR2  0x0148
#define APU_ACC_AUTO_CTRL_CLR3  0x014C
#define APU_ACC_AUTO_STATUS0    0x0160
#define APU_ACC_AUTO_STATUS1    0x0164
#define APU_ACC_AUTO_STATUS2    0x0168

#define APU_ACC_AUTO_STATUS3    0x016C

/* ARE offset define */
#define APU_ARE_INI_CTRL        0x0000
#define APU_ARE_SRAM_CON        0x0004
#define APU_ARE_CONFG0          0x0040
#define APU_ARE_CONFG1          0x0044
#define APU_ARE_GLO_FSM         0x0048
#define APU_ARE_APB_FSM         0x004C
#define APU_ARE_ETRY0_SRAM_H    0x0C00
#define APU_ARE_ETRY0_SRAM_L    0x0800
#define APU_ARE_ETRY1_SRAM_H    0x0C04
#define APU_ARE_ETRY1_SRAM_L    0x0804
#define APU_ARE_ETRY2_SRAM_H    0x0C08
#define APU_ARE_ETRY2_SRAM_L    0x0808
#define APU_ARE_ETRY3_SRAM_H	0x0C0C
#define APU_ARE_ETRY3_SRAM_L	0x080C


/* vcore offset define */
#define APUSYS_VCORE_CG_CON     0x0000
#define APUSYS_VCORE_CG_SET     0x0004
#define APUSYS_VCORE_CG_CLR     0x0008
#define APUSYS_VCORE_SW_RST     0x000C

/* rcx offset define */
#define APU_RCX_CG_CON          0x0000
#define APU_RCX_CG_SET          0x0004
#define APU_RCX_CG_CLR          0x0008
#define APU_RCX_SW_RST          0x000C

/* acx 0/1 offset define */
#define APU_ACX_CONN_CG_CON     0x3C000
#define APU_ACX_CONN_CG_CLR     0x3C008
#define APU_ACX_MVPU_CG_CON     0x2B000
#define APU_ACX_MVPU_CG_CLR     0x2B008
#define APU_ACX_MVPU_SW_RST     0x2B00C
#define APU_ACX_MVPU_RV55_CTRL0 0x2B018
#define APU_ACX_MDLA0_CG_CON    0x30000
#define APU_ACX_MDLA0_CG_CLR    0x30008
#define APU_ACX_MDLA1_CG_CON    0x34000
#define APU_ACX_MDLA1_CG_CLR    0x34008

// vlp offset define
#define APUSYS_AO_CTRL_ADDR	(0x200)

// spm offset define
#define APUSYS_BUCK_ISOLATION		(0x39C)
#define SPM_SEMA_M0			(0x69C)
#define SPM_HW_SEMA_MASTER		SPM_SEMA_M0

// PCU initial data
#define APU_PCUTOP_CTRL_SET	0x0

#define TOP_VRCTL_VR0_EN_SET	0x241
#define TOP_VRCTL_VR0_EN_CLR	0x242
#define MT6373_PMIC_RG_BUCK_VBUCK6_EN_ADDR_SET	TOP_VRCTL_VR0_EN_SET
#define MT6373_PMIC_RG_BUCK_VBUCK6_EN_ADDR_CLR	TOP_VRCTL_VR0_EN_CLR
#define MT6373_PMIC_RG_BUCK_VBUCK6_EN_SHIFT	(6)
#define BUCK_VAPU_PMIC_REG_EN_SET_ADDR	MT6373_PMIC_RG_BUCK_VBUCK6_EN_ADDR_SET
#define BUCK_VAPU_PMIC_REG_EN_CLR_ADDR	MT6373_PMIC_RG_BUCK_VBUCK6_EN_ADDR_CLR
#define BUCK_VAPU_PMIC_REG_EN_SHIFT	MT6373_PMIC_RG_BUCK_VBUCK6_EN_SHIFT
#define MT6363_SLAVE_ID	(0x4)
#define MT6373_SLAVE_ID	(0x5)
#define MAIN_PMIC_ID	MT6363_SLAVE_ID
#define SUB_PMIC_ID	MT6373_SLAVE_ID
#define APU_PCU_BUCK_STEP_SEL		0x0030
#define APU_PCU_BUCK_ON_DAT0_L		0x0080
#define APU_PCU_BUCK_ON_DAT0_H		0x0084
#define APU_PCU_BUCK_OFF_DAT0_L		0x00A0
#define APU_PCU_BUCK_OFF_DAT0_H		0x00A4
#define APU_PCU_BUCK_ON_SLE0		0x00C0
#define VAPU_BUCK_ON_SETTLE_TIME	0x12C

#endif // __mt6983_APUPWR_H__

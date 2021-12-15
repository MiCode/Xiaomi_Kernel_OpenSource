/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6879_APUPWR_H__
#define __MT6879_APUPWR_H__

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

#define VAPU_DEF_VOLT		(750000)	// 0.75v
#define USER_MAX_OPP_VAL	(0) // fastest speed user can specify
#define USER_MIN_OPP_VAL	(6) // slowest speed user can specify
#define TURBO_BOOST_OPP		USER_MAX_OPP_VAL
#define TURBO_BOOST_VAL		(110)
#define MTK_POLL_DELAY_US	(10)
#define MTK_POLL_TIMEOUT	USEC_PER_SEC

enum smc_rcx_pwr_op {
	SMC_RCX_PWR_AFC_EN = 0,
	SMC_RCX_PWR_WAKEUP_RPC,
	SMC_RCX_PWR_CG_EN,
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
	DEVICE_NUM,
};

enum apu_clksrc_id {
	PLL_CONN = 0, // MNOC
	/* PLL_UP, */
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

void mt6879_apu_dump_rpc_status(enum t_acx_id id, struct rpc_status_dump *dump);

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

/* APU PLL4H offset define */
#define PLL4H_GRP0_CON	0x004
#define PLL4H_PLL1_CON0	0x008	//APUPLL; MDLA use
#define PLL4H_PLL1_CON1	0x00C
#define PLL4H_PLL1_CON3	0x014
#define PLL4H_PLL2_CON0	0x018	//NPUPLL; MVUP use
#define PLL4H_PLL2_CON1	0x01C
#define PLL4H_PLL2_CON3	0x024
#define PLL4H_PLL3_CON0	0x028	//APUPLL1; MNOC, uP use
#define PLL4H_PLL3_CON1	0x02C
#define PLL4H_PLL3_CON3	0x034
#define PLL4H_PLL4_CON0	0x038	//APUPLL2; parking use only
#define PLL4H_PLL4_CON1	0x03C
#define PLL4H_PLL4_CON3	0x044
#define PLL4H_FQMTR_CON0	0x200
#define PLL4H_FQMTR_CON1	0x204
#define PLL4HPLL_FHCTL_HP_EN	0xE00
#define PLL4HPLL_FHCTL_UNITSLOPE_EN	0xE04
#define PLL4HPLL_FHCTL_CLK_CON	0xE08
#define PLL4HPLL_FHCTL_RST_CON	0xE0C
#define PLL4HPLL_FHCTL_SLOPE0	0xE10
#define PLL4HPLL_FHCTL_SLOPE1	0xE14
#define PLL4HPLL_FHCTL_DSSC_CFG	0xE18
#define PLL4HPLL_FHCTL_DSSC0_CON	0xE1C
#define PLL4HPLL_FHCTL_DSSC1_CON	0xE20
#define PLL4HPLL_FHCTL_DSSC2_CON	0xE24
#define PLL4HPLL_FHCTL_DSSC3_CON	0xE28
#define PLL4HPLL_FHCTL_DSSC4_CON	0xE2C
#define PLL4HPLL_FHCTL_DSSC5_CON	0xE30
#define PLL4HPLL_FHCTL_DSSC6_CON	0xE34
#define PLL4HPLL_FHCTL_DSSC7_CON	0xE38
#define PLL4HPLL_FHCTL0_CFG	0xE3C
#define PLL4HPLL_FHCTL0_UPDNLMT	0xE40
#define PLL4HPLL_FHCTL0_DDS	0xE44
#define PLL4HPLL_FHCTL0_DVFS	0xE48
#define PLL4HPLL_FHCTL0_MON	0xE4C
#define PLL4HPLL_FHCTL1_CFG	0xE50
#define PLL4HPLL_FHCTL1_UPDNLMT	0xE54
#define PLL4HPLL_FHCTL1_DDS	0xE58
#define PLL4HPLL_FHCTL1_DVFS	0xE5C
#define PLL4HPLL_FHCTL1_MON	0xE60
#define PLL4HPLL_FHCTL2_CFG	0xE64
#define PLL4HPLL_FHCTL2_UPDNLMT	0xE68
#define PLL4HPLL_FHCTL2_DDS	0xE6C
#define PLL4HPLL_FHCTL2_DVFS	0xE70
#define PLL4HPLL_FHCTL2_MON	0xE74
#define PLL4HPLL_FHCTL3_CFG	0xE78
#define PLL4HPLL_FHCTL3_UPDNLMT	0xE7C
#define PLL4HPLL_FHCTL3_DDS	0xE80
#define PLL4HPLL_FHCTL3_DVFS	0xE84
#define PLL4HPLL_FHCTL3_MON	0xE88


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


// vlp offset define
#define APUSYS_AO_CTRL_ADDR	(0x200)

// spm offset define
#define APUSYS_BUCK_ISOLATION		(0x39C)

// PCU initial data
#define APU_PCUTOP_CTRL_SET	0x0

#define TOP_VRCTL_VR0_EN_SET	0x241
#define TOP_VRCTL_VR0_EN_CLR	0x242
#define MT6368_PMIC_RG_BUCK_VBUCK3_EN_ADDR_SET	TOP_VRCTL_VR0_EN_SET
#define MT6368_PMIC_RG_BUCK_VBUCK3_EN_ADDR_CLR	TOP_VRCTL_VR0_EN_CLR
#define MT6368_PMIC_RG_BUCK_VBUCK3_EN_SHIFT	(3)
#define BUCK_VAPU_PMIC_REG_EN_SET_ADDR	MT6368_PMIC_RG_BUCK_VBUCK3_EN_ADDR_SET
#define BUCK_VAPU_PMIC_REG_EN_CLR_ADDR	MT6368_PMIC_RG_BUCK_VBUCK3_EN_ADDR_CLR
#define BUCK_VAPU_PMIC_REG_EN_SHIFT	MT6368_PMIC_RG_BUCK_VBUCK3_EN_SHIFT
#define MT6363_SLAVE_ID	(0x4)
#define MT6368_SLAVE_ID	(0x5)
#define MAIN_PMIC_ID	MT6363_SLAVE_ID
#define SUB_PMIC_ID	MT6368_SLAVE_ID
#define APU_PCU_BUCK_STEP_SEL		0x0030
#define APU_PCU_BUCK_ON_DAT0_L		0x0080
#define APU_PCU_BUCK_ON_DAT0_H		0x0084
#define APU_PCU_BUCK_OFF_DAT0_L		0x00A0
#define APU_PCU_BUCK_OFF_DAT0_H		0x00A4
#define APU_PCU_BUCK_ON_SLE0		0x00C0
#define VAPU_BUCK_ON_SETTLE_TIME	0x12C	//300us

#endif // __mt6879_APUPWR_H__

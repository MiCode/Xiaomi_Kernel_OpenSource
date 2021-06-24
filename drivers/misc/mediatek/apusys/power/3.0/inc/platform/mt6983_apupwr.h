/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT6983_APUPWR_H__
#define __MT6983_APUPWR_H__

#include <linux/io.h>
#include <linux/clk.h>

#define MTK_POLL_DELAY_US	(10)
#define MTK_POLL_TIMEOUT	USEC_PER_SEC
#define DEBUG_DUMP_REG		(0)
#define APU_POWER_BRING_UP	(1)
#define CFG_FPGA		(1)

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

enum apupw_reg {
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

/* APU PLL1U offset define */
// APUPLL for MNOC usage
#define MNOC_PLL_BASE		0x000 // 0x190F3000
// NPUPLL for uP usage
#define UP_PLL_BASE		0x400 // 0x190F3400
// APUPLL1 for MDLA usage
#define MDLA_PLL_BASE		0x800 // 0x190F3800
// APUPLL2 for MVPU usage
#define MVPU_PLL_BASE		0xC00 // 0x190F3C00

#define PLL1U_PLL1_CON1		0x00C
#define PLL1UPLL_FHCTL_HP_EN	0x100
#define PLL1UPLL_FHCTL_CLK_CON	0x108
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

// spm offset define
#define APUSYS_BUCK_ISOLATION		(0x39C)

// mbox offset define (for data exchange with remote)
#define SPARE0_MBOX_DUMMY_0_ADDR        0x640
#define SPARE0_MBOX_DUMMY_1_ADDR        0x644
#define SPARE0_MBOX_DUMMY_2_ADDR        0x648
#define SPARE0_MBOX_DUMMY_3_ADDR        0x64C

// PCU initial data
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
#define VAPU_BUCK_ON_SETTLE_TIME	(0x300) // FIXME: check HEX or DEC

#endif // __mt6983_APUPWR_H__

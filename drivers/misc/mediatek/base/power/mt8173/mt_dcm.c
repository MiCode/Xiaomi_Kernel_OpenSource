/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <mt-plat/mt_chip.h>

#include "mt_dcm.h"

#define DCM_HAVE_CHIP_VER	0
#define DCM_SYS_POWER		1
#define DCM_CPU_2		1
#define INFRA_DCM		0

#define TAG	"[Power/dcm] "

#define dcm_err(fmt, args...)	pr_err(TAG fmt, ##args)
#define dcm_warn(fmt, args...)	pr_warn(TAG fmt, ##args)
#define dcm_info(fmt, args...)	pr_debug(TAG fmt, ##args)
#define dcm_dbg(fmt, args...)	pr_debug(TAG fmt, ##args)
#define dcm_ver(fmt, args...)	pr_debug(TAG fmt, ##args)

#define dcm_readl(addr)		readl(addr)
#define dcm_writel(addr, val)	writel(val, addr)
#define dcm_setl(addr, val)	dcm_writel(addr, dcm_readl(addr) | (val))
#define dcm_clrl(addr, val)	dcm_writel(addr, dcm_readl(addr) & ~(val))

#if !DCM_HAVE_CHIP_VER /* TODO: remove it when mt_get_chip_id() done */

#endif /* !DCM_HAVE_CHIP_VER */

static void __iomem *topckgen_base;	/* 0x10000000 */
static void __iomem *infrasys_base;	/* 0x10001000 */
static void __iomem *perisys_base;	/* 0x10003000 */
static void __iomem *dramc0_base;	/* 0x10004000 */
static void __iomem *scpsys_base;	/* 0x10006000 */
static void __iomem *pwrap_base;	/* 0x1000D000 */
static void __iomem *dramc1_base;	/* 0x10011000 */
static void __iomem *mcucfg_base;	/* 0x10200000 */
static void __iomem *emi_base;		/* 0x10203000 */
static void __iomem *m4u_base;		/* 0x10205000 */
static void __iomem *peri_iommu_base;	/* 0x10214000 */
static void __iomem *i2c0_base;		/* 0x11007000 */
static void __iomem *i2c1_base;		/* 0x11008000 */
static void __iomem *i2c2_base;		/* 0x11009000 */
static void __iomem *i2c3_base;		/* 0x11010000 */
static void __iomem *i2c4_base;		/* 0x11011000 */
static void __iomem *usb0_base;		/* 0x11200000 */
static void __iomem *msdc0_base;	/* 0x11230000 */
static void __iomem *msdc1_base;	/* 0x11240000 */
static void __iomem *msdc2_base;	/* 0x11250000 */
static void __iomem *msdc3_base;	/* 0x11260000 */
static void __iomem *mmsys_base;	/* 0x14000000 */
static void __iomem *smi_larb0_base;	/* 0x14021000 */
static void __iomem *smi_common_base;	/* 0x14022000 */
static void __iomem *smi_larb4_base;	/* 0x14027000 */
static void __iomem *smi_larb2_base;	/* 0x15001000 */
static void __iomem *cam1_base;		/* 0x15004000 */
static void __iomem *fdvt_base;		/* 0x1500B000 */
static void __iomem *vdecsys_base;	/* 0x16000000 */
static void __iomem *smi_larb1_base;	/* 0x16010000 */
static void __iomem *smi_larb3_base;	/* 0x18001000 */
static void __iomem *venc_base;		/* 0x18002000 */
static void __iomem *jpgenc_base;	/* 0x18003000 */
static void __iomem *jpgdec_base;	/* 0x18004000 */
static void __iomem *venc_lt_base;	/* 0x19002000 */
static void __iomem *smi_larb5_base;	/* 0x19001000 */

#define TOPCKGEN_REG(ofs)	(topckgen_base + ofs)
#define INFRA_REG(ofs)		(infrasys_base + ofs)
#define PREI_REG(ofs)		(perisys_base + ofs)
#define MM_REG(ofs)		(mmsys_base + ofs)
#define VDEC_REG(ofs)		(vdecsys_base + ofs)
#define VENC_REG(ofs)		(venc_base + ofs)
#define VENC_LT_REG(ofs)	(venc_lt_base + ofs)

#define DRAMC0_REG(ofs)		(dramc0_base + ofs)
#define PWRAP_REG(ofs)		(pwrap_base + ofs)
#define DRAMC1_REG(ofs)		(dramc1_base + ofs)
#define MCUCFG_REG(ofs)		(mcucfg_base + ofs)
#define EMI_REG(ofs)		(emi_base + ofs)
#define M4U_REG(ofs)		(m4u_base + ofs)
#define PERI_IOMMU_REG(ofs)	(peri_iommu_base + ofs)
#define I2C0_REG(ofs)		(i2c0_base + ofs)
#define I2C1_REG(ofs)		(i2c1_base + ofs)
#define I2C2_REG(ofs)		(i2c2_base + ofs)
#define I2C3_REG(ofs)		(i2c3_base + ofs)
#define I2C4_REG(ofs)		(i2c4_base + ofs)
#define USB0_REG(ofs)		(usb0_base + ofs)
#define MSDC0_REG(ofs)		(msdc0_base + ofs)
#define MSDC1_REG(ofs)		(msdc1_base + ofs)
#define MSDC2_REG(ofs)		(msdc2_base + ofs)
#define MSDC3_REG(ofs)		(msdc3_base + ofs)
#define SMI_LARB0_REG(ofs)	(smi_larb0_base + ofs)
#define SMI_COMMON_REG(ofs)	(smi_common_base + ofs)
#define SMI_LARB2_REG(ofs)	(smi_larb2_base + ofs)
#define CAM1_REG(ofs)		(cam1_base + ofs)
#define FDVT_REG(ofs)		(fdvt_base + ofs)
#define SMI_LARB1_REG(ofs)	(smi_larb1_base + ofs)
#define SMI_LARB4_REG(ofs)	(smi_larb4_base + ofs)
#define SMI_LARB3_REG(ofs)	(smi_larb3_base + ofs)
#define JPGENC_REG(ofs)		(jpgenc_base + ofs)
#define JPGDEC_REG(ofs)		(jpgdec_base + ofs)
#define SMI_LARB5_REG(ofs)	(smi_larb5_base + ofs)
#define SCP_REG(ofs)		(scpsys_base + ofs)

#define USB0_DCM		USB0_REG(0x700)

#define MSDC0_PATCH_BIT1	MSDC0_REG(0x00B4)
#define MSDC1_PATCH_BIT1	MSDC1_REG(0x00B4)
#define MSDC2_PATCH_BIT1	MSDC2_REG(0x00B4)
#define MSDC3_PATCH_BIT1	MSDC3_REG(0x00B4)

#define PMIC_WRAP_DCM_EN	PWRAP_REG(0x144)

#define I2C0_I2CREG_HW_CG_EN	I2C0_REG(0x054)
#define I2C1_I2CREG_HW_CG_EN	I2C1_REG(0x054)
#define I2C2_I2CREG_HW_CG_EN	I2C2_REG(0x054)
#define I2C3_I2CREG_HW_CG_EN	I2C3_REG(0x054)
#define I2C4_I2CREG_HW_CG_EN	I2C4_REG(0x054)

#define BUS_FABRIC_DCM_CTRL	MCUCFG_REG(0x0668)
#define L2C_SRAM_CTRL		MCUCFG_REG(0x0648)
#define CCI_CLK_CTRL		MCUCFG_REG(0x0660)
#define MP0_SYNC_DCM_DIV	MCUCFG_REG(0x0080)
#define MP1_SYNC_DCM_DIV	MCUCFG_REG(0x029C)

#define DCM_CFG			TOPCKGEN_REG(0x0004)

#define CA7_CKDIV1		INFRA_REG(0x0008)
#define INFRA_TOPCKGEN_DCMCTL	INFRA_REG(0x0010)
#define INFRA_TOPCKGEN_DCMDBC	INFRA_REG(0x0014)

#define INFRA_GLOBALCON_DCMCTL	INFRA_REG(0x0050)
#define INFRA_GLOBALCON_DCMDBC	INFRA_REG(0x0054)
#define INFRA_GLOBALCON_DCMFSEL	INFRA_REG(0x0058)
#define MM_MMU_DCM_DIS		M4U_REG(0x0050)
#define PERISYS_MMU_DCM_DIS	PERI_IOMMU_REG(0x0050)

#define PERI_GLOBALCON_DCMCTL	PREI_REG(0x0050)
#define PERI_GLOBALCON_DCMDBC	PREI_REG(0x0054)
#define PERI_GLOBALCON_DCMFSEL	PREI_REG(0x0058)

#define channel_A_DRAMC_PD_CTRL	DRAMC0_REG(0x01DC)
#define channel_B_DRAMC_PD_CTRL	DRAMC1_REG(0x01DC)

#define SMI_COMMON_SMI_DCM	SMI_COMMON_REG(0x300)

#define SMI_LARB0_STA		SMI_LARB0_REG(0x00)
#define SMI_LARB0_CON		SMI_LARB0_REG(0x10)
#define SMI_LARB0_CON_SET	SMI_LARB0_REG(0x14)
#define SMI_LARB0_CON_CLR	SMI_LARB0_REG(0x18)

#define SMI_LARB1_STA		SMI_LARB1_REG(0x00)
#define SMI_LARB1_CON		SMI_LARB1_REG(0x10)
#define SMI_LARB1_CON_SET	SMI_LARB1_REG(0x14)
#define SMI_LARB1_CON_CLR	SMI_LARB1_REG(0x18)

#define SMI_LARB2_STA		SMI_LARB2_REG(0x00)
#define SMI_LARB2_CON		SMI_LARB2_REG(0x10)
#define SMI_LARB2_CON_SET	SMI_LARB2_REG(0x14)
#define SMI_LARB2_CON_CLR	SMI_LARB2_REG(0x18)

#define SMI_LARB3_STA		SMI_LARB3_REG(0x00)
#define SMI_LARB3_CON		SMI_LARB3_REG(0x10)
#define SMI_LARB3_CON_SET	SMI_LARB3_REG(0x14)
#define SMI_LARB3_CON_CLR	SMI_LARB3_REG(0x18)

#define SMI_LARB4_STA		SMI_LARB4_REG(0x00)
#define SMI_LARB4_CON		SMI_LARB4_REG(0x10)
#define SMI_LARB4_CON_SET	SMI_LARB4_REG(0x14)
#define SMI_LARB4_CON_CLR	SMI_LARB4_REG(0x18)

#define SMI_LARB5_STA		SMI_LARB5_REG(0x00)
#define SMI_LARB5_CON		SMI_LARB5_REG(0x10)
#define SMI_LARB5_CON_SET	SMI_LARB5_REG(0x14)
#define SMI_LARB5_CON_CLR	SMI_LARB5_REG(0x18)

#define EMI_CONM		EMI_REG(0x60)

#define CTL_RAW_DCM_DIS		CAM1_REG(0x188)
#define CTL_RAW_D_DCM_DIS	CAM1_REG(0x18C)
#define CTL_DMA_DCM_DIS		CAM1_REG(0x190)
#define CTL_RGB_DCM_DIS		CAM1_REG(0x194)
#define CTL_YUV_DCM_DIS		CAM1_REG(0x198)
#define CTL_TOP_DCM_DIS		CAM1_REG(0x19C)

#define FDVT_CTRL		FDVT_REG(0x19C)

#define JPGENC_DCM_CTRL		JPGENC_REG(0x300)
#define JPGDEC_DCM_CTRL		JPGDEC_REG(0x300)

#define MMSYS_HW_DCM_DIS0	MM_REG(0x120)
#define MMSYS_HW_DCM_DIS_SET0	MM_REG(0x124)
#define MMSYS_HW_DCM_DIS_CLR0	MM_REG(0x128)

#define MMSYS_HW_DCM_DIS1	MM_REG(0x130)
#define MMSYS_HW_DCM_DIS_SET1	MM_REG(0x134)
#define MMSYS_HW_DCM_DIS_CLR1	MM_REG(0x138)

#define VENC_CLK_CG_CTRL	VENC_REG(0xFC)
#define VENC_CLK_DCM_CTRL	VENC_REG(0xF4)

#define VENC_LT_CLK_CG_CTRL	VENC_LT_REG(0xFC)
#define VENC_LT_CLK_DCM_CTRL	VENC_LT_REG(0xF4)

#define VDEC_DCM_CON		VDEC_REG(0x18)

#define SPM_PWR_STATUS		SCP_REG(0x060c)
#define SPM_PWR_STATUS_2ND	SCP_REG(0x0610)

static DEFINE_MUTEX(dcm_lock);

static uint32_t dcm_reg_init;
static uint32_t dcm_sta;

struct clk *mm_sel;
struct clk *peri_usb0;

#define MT_MUX_MM	0

#define DIS_PWR_STA_MASK		BIT(3)
#define MFG_PWR_STA_MASK		BIT(4)
#define ISP_PWR_STA_MASK		BIT(5)
#define VDE_PWR_STA_MASK		BIT(7)
#define VEN2_PWR_STA_MASK		BIT(20)
#define VEN_PWR_STA_MASK		BIT(21)
#define MFG_2D_PWR_STA_MASK		BIT(22)
#define MFG_ASYNC_PWR_STA_MASK		BIT(23)
#define AUDIO_PWR_STA_MASK		BIT(24)
#define USB_PWR_STA_MASK		BIT(25)

enum subsys_id {
	SYS_VDE,
	SYS_MFG,
	SYS_VEN,
	SYS_ISP,
	SYS_DIS,
	SYS_VEN2,
	SYS_AUDIO,
	SYS_MFG_2D,
	SYS_MFG_ASYNC,
	SYS_USB,
	NR_SYSS,
};

static int subsys_is_on(enum subsys_id id)
{
	u32 pwr_sta_mask[] = {
		VDE_PWR_STA_MASK,
		MFG_PWR_STA_MASK,
		VEN_PWR_STA_MASK,
		ISP_PWR_STA_MASK,
		DIS_PWR_STA_MASK,
		VEN2_PWR_STA_MASK,
		AUDIO_PWR_STA_MASK,
		MFG_2D_PWR_STA_MASK,
		MFG_ASYNC_PWR_STA_MASK,
		USB_PWR_STA_MASK,
	};

	u32 mask = pwr_sta_mask[id];
	u32 sta = dcm_readl(SPM_PWR_STATUS);
	u32 sta_s = dcm_readl(SPM_PWR_STATUS_2ND);

	return (sta & mask) && (sta_s & mask);
}

static void enable_subsys_clocks(void)
{
	clk_prepare_enable(mm_sel);
	/* To avoid system-hung by reading usb0 regiser,
	 * peri_usb0 clock needs to be enabled first.
	 */
	clk_prepare_enable(peri_usb0);
}

static void disable_subsys_clocks(void)
{
	clk_disable_unprepare(mm_sel);
	clk_disable_unprepare(peri_usb0);
}

static void print_dcm_reg(const char *type, const char *regname,
		void __iomem *addr)
{
	dcm_info("[%7s] %-23s: [0x%p]: 0x%08x\n",
		type, regname, addr, dcm_readl(addr));
}

#define DUMP(type, regname)	\
	print_dcm_reg(type, #regname, regname)

void dcm_dump_regs(uint32_t type)
{
	const char *t;
	enum chip_sw_ver chip_sw_ver = mt_get_chip_sw_ver();

	if (!dcm_reg_init)
		return;

	mutex_lock(&dcm_lock);

	if (type & CPU_DCM) {
		t = "CPU_DCM";

		DUMP(t, BUS_FABRIC_DCM_CTRL);
		DUMP(t, L2C_SRAM_CTRL);
		DUMP(t, CCI_CLK_CTRL);
#if DCM_CPU_2
		if (chip_sw_ver >= CHIP_SW_VER_02) {
			DUMP(t, MP0_SYNC_DCM_DIV);
			DUMP(t, MP1_SYNC_DCM_DIV);
		}
#endif /* DCM_CPU_2 */
	}

	if (type & IFR_DCM) {
		t = "IFR_DCM";

		DUMP(t, CA7_CKDIV1);
		DUMP(t, INFRA_TOPCKGEN_DCMCTL);
		DUMP(t, INFRA_TOPCKGEN_DCMDBC);
		DUMP(t, INFRA_GLOBALCON_DCMCTL);
		DUMP(t, INFRA_GLOBALCON_DCMDBC);
		DUMP(t, INFRA_GLOBALCON_DCMFSEL);
		DUMP(t, channel_A_DRAMC_PD_CTRL);
		DUMP(t, channel_B_DRAMC_PD_CTRL);

		DUMP(t, DCM_CFG);
	}

	if (type & PER_DCM) {
		t = "PER_DCM";

		DUMP(t, PERI_GLOBALCON_DCMCTL);
		DUMP(t, PERI_GLOBALCON_DCMDBC);
		DUMP(t, PERI_GLOBALCON_DCMFSEL);
		DUMP(t, MSDC0_PATCH_BIT1);
		DUMP(t, MSDC1_PATCH_BIT1);
		DUMP(t, MSDC2_PATCH_BIT1);
		DUMP(t, MSDC3_PATCH_BIT1);
		DUMP(t, USB0_DCM);
		DUMP(t, PMIC_WRAP_DCM_EN);
		DUMP(t, I2C0_I2CREG_HW_CG_EN);
		DUMP(t, I2C1_I2CREG_HW_CG_EN);
		DUMP(t, I2C2_I2CREG_HW_CG_EN);
	}

	if (type & SMI_DCM)
		DUMP("SMI_DCM", SMI_COMMON_SMI_DCM);

	if (type & DIS_DCM) {
		if (subsys_is_on(SYS_DIS)) {
			t = "DIS_DCM";

			DUMP(t, MMSYS_HW_DCM_DIS0);
			DUMP(t, MMSYS_HW_DCM_DIS_SET0);
			DUMP(t, MMSYS_HW_DCM_DIS_CLR0);
			DUMP(t, MMSYS_HW_DCM_DIS1);
			DUMP(t, MMSYS_HW_DCM_DIS_SET1);
			DUMP(t, MMSYS_HW_DCM_DIS_CLR1);
			DUMP(t, SMI_LARB0_STA);
			DUMP(t, SMI_LARB0_CON);
			DUMP(t, SMI_LARB0_CON_SET);
			DUMP(t, SMI_LARB0_CON_CLR);
			DUMP(t, SMI_LARB4_STA);
			DUMP(t, SMI_LARB4_CON);
			DUMP(t, SMI_LARB4_CON_SET);
			DUMP(t, SMI_LARB4_CON_CLR);
		}
	}

	if (type & ISP_DCM) {
		t = "ISP_DCM";

		if (subsys_is_on(SYS_ISP)) {
			DUMP(t, CTL_RAW_DCM_DIS);
			DUMP(t, CTL_RAW_D_DCM_DIS);
			DUMP(t, CTL_DMA_DCM_DIS);
			DUMP(t, CTL_RGB_DCM_DIS);
			DUMP(t, CTL_YUV_DCM_DIS);
			DUMP(t, CTL_TOP_DCM_DIS);
			DUMP(t, SMI_LARB2_CON_SET);
		}

		if (subsys_is_on(SYS_VEN)) {
			DUMP(t, VENC_CLK_CG_CTRL);
			DUMP(t, VENC_CLK_DCM_CTRL);
			DUMP(t, JPGENC_DCM_CTRL);
			DUMP(t, SMI_LARB3_CON_SET);
		}

		if (subsys_is_on(SYS_VEN2)) {
			DUMP(t, VENC_LT_CLK_CG_CTRL);
			DUMP(t, VENC_LT_CLK_DCM_CTRL);
			DUMP(t, SMI_LARB5_CON_SET);
		}
	}

	if (type & VDE_DCM) {
		if (subsys_is_on(SYS_VDE)) {
			t = "VDE_DCM";

			DUMP(t, VDEC_DCM_CON);
			DUMP(t, SMI_LARB1_CON_SET);
		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_enable(uint32_t type)
{
	enum chip_sw_ver chip_sw_ver = mt_get_chip_sw_ver();

	if (!dcm_reg_init)
		return;

	dcm_dbg("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	if (type & CPU_DCM) {
		dcm_dbg("[%s][CPU_DCM]=0x%08x\n", __func__, CPU_DCM);

		dcm_writel(BUS_FABRIC_DCM_CTRL, 0xffeffdaf);
		dcm_setl(L2C_SRAM_CTRL, BIT(0));
		dcm_setl(CCI_CLK_CTRL, BIT(8));

#if DCM_CPU_2
		if (chip_sw_ver >= CHIP_SW_VER_02) {
			dcm_setl(MP0_SYNC_DCM_DIV, 0x00000006);
			dcm_setl(MP0_SYNC_DCM_DIV, 0x00000040);
			dcm_clrl(MP0_SYNC_DCM_DIV, 0x00000040);
			dcm_setl(MP0_SYNC_DCM_DIV, 0x00000001);
			dcm_setl(MP1_SYNC_DCM_DIV, 0x000C0000);
			dcm_setl(MP1_SYNC_DCM_DIV, 0x00020000);
			dcm_clrl(MP1_SYNC_DCM_DIV, 0x00020000);
			dcm_setl(MP1_SYNC_DCM_DIV, 0x00010000);
		}
#endif /* DCM_CPU_2 */

		dcm_sta |= CPU_DCM;
	}

	if (type & IFR_DCM) {
		dcm_dbg("[%s][IFR_DCM]=0x%08x\n", __func__, IFR_DCM);
		dcm_dbg("[%s] chip_sw_ver: %d\n", __func__, chip_sw_ver);

		dcm_clrl(CA7_CKDIV1, 0x0000001F);

		if (chip_sw_ver == CHIP_SW_VER_01) {
			dcm_setl(INFRA_TOPCKGEN_DCMCTL, 0x00000001);
			dcm_clrl(INFRA_TOPCKGEN_DCMCTL, 0x00000770);
		} else {
			dcm_setl(INFRA_TOPCKGEN_DCMCTL, 0x00000771);
		}

#if INFRA_DCM
		dcm_setl(INFRA_GLOBALCON_DCMCTL, 0x00000303);
		dcm_setl(INFRA_GLOBALCON_DCMDBC, 0x01000100);
		dcm_clrl(INFRA_GLOBALCON_DCMDBC, 0x007F007F);
		dcm_setl(INFRA_GLOBALCON_DCMFSEL, 0x10100000);
		dcm_clrl(INFRA_GLOBALCON_DCMFSEL, 0x0F0F0F07);
#endif /* INFRA_DCM */

		dcm_clrl(MM_MMU_DCM_DIS, 0x0000007F);

		dcm_clrl(PERISYS_MMU_DCM_DIS, 0x0000007F);

		/* DRAMC */
		dcm_setl(channel_A_DRAMC_PD_CTRL, 0xC3000000);
		dcm_clrl(channel_A_DRAMC_PD_CTRL, 0x00000008);
		dcm_setl(channel_B_DRAMC_PD_CTRL, 0xC3000000);
		dcm_clrl(channel_B_DRAMC_PD_CTRL, 0x00000008);

		dcm_sta |= IFR_DCM;
	}

	if (type & PER_DCM) {
		dcm_dbg("[%s][PER_DCM]=0x%08x\n", __func__, PER_DCM);

		dcm_setl(PERI_GLOBALCON_DCMCTL, 0x000000F3);
		dcm_clrl(PERI_GLOBALCON_DCMCTL, 0x00001F00);
		dcm_clrl(PERI_GLOBALCON_DCMDBC, 0x0000000F);
		dcm_setl(PERI_GLOBALCON_DCMDBC, 0x000000F0);
		dcm_clrl(PERI_GLOBALCON_DCMFSEL, 0x001F0F07);

		/* MSDC module */
		dcm_setl(MSDC0_PATCH_BIT1, 0x00200000);
		dcm_clrl(MSDC0_PATCH_BIT1, 0xFF800000);
		dcm_setl(MSDC1_PATCH_BIT1, 0x00200000);
		dcm_clrl(MSDC1_PATCH_BIT1, 0xFF800000);
		dcm_setl(MSDC2_PATCH_BIT1, 0x00200000);
		dcm_clrl(MSDC2_PATCH_BIT1, 0xFF800000);
		dcm_setl(MSDC3_PATCH_BIT1, 0x00200000);
		dcm_clrl(MSDC3_PATCH_BIT1, 0xFF800000);

		/* USB */
		dcm_clrl(USB0_DCM, 0x00070000);

		/* PMIC */
		dcm_setl(PMIC_WRAP_DCM_EN, 0x00000001);

		/* I2C */
		dcm_setl(I2C0_I2CREG_HW_CG_EN, 0x00000001);
		dcm_setl(I2C1_I2CREG_HW_CG_EN, 0x00000001);
		dcm_setl(I2C2_I2CREG_HW_CG_EN, 0x00000001);
		dcm_setl(I2C3_I2CREG_HW_CG_EN, 0x00000001);
		dcm_setl(I2C4_I2CREG_HW_CG_EN, 0x00000001);

		dcm_sta |= PER_DCM;
	}

	if (type & SMI_DCM) {
		dcm_dbg("[%s][SMI_DCM]=0x%08x\n", __func__, SMI_DCM);

		dcm_writel(SMI_COMMON_SMI_DCM, 0x00000001);

		dcm_sta |= SMI_DCM;
	}

	if (type & EMI_DCM) {
		dcm_dbg("[%s][EMI_DCM]=0x%08x\n", __func__, EMI_DCM);

		dcm_setl(EMI_CONM, 0x40000000);
		dcm_clrl(EMI_CONM, 0xBF000000);

		dcm_sta |= EMI_DCM;
	}

	if (type & DIS_DCM) {
		dcm_dbg("[%s][DIS_DCM]=0x%08x,subsys_is_on(SYS_DIS)=%d\n",
			__func__, DIS_DCM, subsys_is_on(SYS_DIS));

		if (subsys_is_on(SYS_DIS)) {
			dcm_writel(MMSYS_HW_DCM_DIS0, 0x00000000);
			dcm_writel(MMSYS_HW_DCM_DIS_SET0, 0x00000000);
			dcm_writel(MMSYS_HW_DCM_DIS_CLR0, 0xFFFFFFFF);

			dcm_writel(MMSYS_HW_DCM_DIS1, 0x00000000);
			dcm_writel(MMSYS_HW_DCM_DIS_SET1, 0x00000000);
			dcm_writel(MMSYS_HW_DCM_DIS_CLR1, 0xFFFFFFFF);

			dcm_setl(SMI_LARB0_CON_SET, 0x00000010);
			dcm_setl(SMI_LARB4_CON_SET, 0x00000010);

			dcm_sta |= DIS_DCM;
		}
	}

	if (type & ISP_DCM) {
		/* video encoder : sensor=>ISP=>VENC */
		dcm_dbg("[%s][ISP_DCM]=0x%08x,SYS_(ISP,VEN,VEN2)=(%d,%d,%d)\n",
			__func__, ISP_DCM, subsys_is_on(SYS_ISP),
			subsys_is_on(SYS_VEN), subsys_is_on(SYS_VEN2));

		if (subsys_is_on(SYS_ISP) &&
		    subsys_is_on(SYS_VEN) &&
		    subsys_is_on(SYS_VEN2)) {
			dcm_clrl(CTL_RAW_D_DCM_DIS, 0x024EAFE8);
			dcm_clrl(CTL_DMA_DCM_DIS, 0x07FFFFFF);
			dcm_clrl(CTL_RGB_DCM_DIS, 0x0000007F);
			dcm_clrl(CTL_YUV_DCM_DIS, 0x000FFFFF);
			dcm_clrl(CTL_TOP_DCM_DIS, 0x0000000F);

			dcm_clrl(FDVT_CTRL, 0x0000001F);

			dcm_writel(VENC_CLK_CG_CTRL, 0xFFFFFFFF);
			dcm_setl(VENC_CLK_DCM_CTRL, 0x00000001);

			dcm_writel(VENC_LT_CLK_CG_CTRL, 0xFFFFFFFF);
			dcm_setl(VENC_LT_CLK_DCM_CTRL, 0x00000001);

			dcm_clrl(JPGENC_DCM_CTRL, 0x00000001);
			dcm_clrl(JPGDEC_DCM_CTRL, 0x00000001);

			dcm_setl(SMI_LARB2_CON_SET, 0x00000010);
			dcm_setl(SMI_LARB3_CON_SET, 0x00000010);
			dcm_setl(SMI_LARB5_CON_SET, 0x00000010);

			dcm_sta |= ISP_DCM;
		}
	}

	if (type & VDE_DCM) {
		dcm_dbg("[%s][VDE_DCM]=0x%08x,subsys_is_on(SYS_VDE)=%d\n",
			__func__, VDE_DCM, subsys_is_on(SYS_VDE));

		if (subsys_is_on(SYS_VDE)) {
			dcm_clrl(VDEC_DCM_CON, 0x00000001);

			dcm_setl(SMI_LARB1_CON_SET, 0x00000010);

			dcm_sta |= VDE_DCM;
		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_disable(uint32_t type)
{
	enum chip_sw_ver chip_sw_ver = mt_get_chip_sw_ver();

	if (!dcm_reg_init)
		return;

	dcm_dbg("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	if (type & CPU_DCM) {
		dcm_dbg("[%s][CPU_DCM]=0x%08x\n", __func__, CPU_DCM);

		dcm_writel(BUS_FABRIC_DCM_CTRL, 0x00000000);
		dcm_clrl(L2C_SRAM_CTRL, BIT(0));
		dcm_clrl(CCI_CLK_CTRL, BIT(8));

#if DCM_CPU_2
		if (chip_sw_ver >= CHIP_SW_VER_02) {
			dcm_clrl(MP0_SYNC_DCM_DIV, 0x00000001);
			dcm_clrl(MP1_SYNC_DCM_DIV, 0x00010000);
		}
#endif /* DCM_CPU_2 */

		dcm_sta &= ~CPU_DCM;
	}

	if (type & PER_DCM) {
		dcm_dbg("[%s][PER_DCM]=0x%08x\n", __func__, PER_DCM);

		dcm_clrl(PERI_GLOBALCON_DCMCTL, 0x00001FF3);
		dcm_clrl(PERI_GLOBALCON_DCMDBC, 0x0000000F);
		dcm_setl(PERI_GLOBALCON_DCMDBC, 0x000000F0);
		dcm_clrl(PERI_GLOBALCON_DCMFSEL, 0x001F0F07);

		/* MSDC module */
		dcm_clrl(MSDC0_PATCH_BIT1, 0x00200000);
		dcm_setl(MSDC0_PATCH_BIT1, 0xFF800000);
		dcm_clrl(MSDC1_PATCH_BIT1, 0x00200000);
		dcm_setl(MSDC1_PATCH_BIT1, 0xFF800000);
		dcm_clrl(MSDC2_PATCH_BIT1, 0x00200000);
		dcm_setl(MSDC2_PATCH_BIT1, 0xFF800000);
		dcm_clrl(MSDC3_PATCH_BIT1, 0x00200000);
		dcm_setl(MSDC3_PATCH_BIT1, 0xFF800000);

		/* USB */
		dcm_setl(USB0_DCM, 0x00070000);

		/* PMIC */
		dcm_clrl(PMIC_WRAP_DCM_EN, 0x00000001);

		/* I2C */
		dcm_clrl(I2C0_I2CREG_HW_CG_EN, 0x00000001);
		dcm_clrl(I2C1_I2CREG_HW_CG_EN, 0x00000001);
		dcm_clrl(I2C2_I2CREG_HW_CG_EN, 0x00000001);
		dcm_clrl(I2C3_I2CREG_HW_CG_EN, 0x00000001);
		dcm_clrl(I2C4_I2CREG_HW_CG_EN, 0x00000001);

		dcm_sta &= ~PER_DCM;
	}

	if (type & IFR_DCM) {
		dcm_dbg("[%s][IFR_DCM]=0x%08x\n", __func__, IFR_DCM);

		/* turn off DRAMC DCM before TOP_DCMCTL */

		/* DRAMC */
		dcm_setl(channel_A_DRAMC_PD_CTRL, 0x01000000);
		dcm_clrl(channel_A_DRAMC_PD_CTRL, 0xC2000008);
		dcm_setl(channel_B_DRAMC_PD_CTRL, 0x01000000);
		dcm_clrl(channel_B_DRAMC_PD_CTRL, 0xC2000008);

#if INFRA_DCM
		dcm_clrl(INFRA_TOPCKGEN_DCMCTL, 0x00000771);
		dcm_clrl(INFRA_TOPCKGEN_DCMDBC, 0x00000001);
		dcm_clrl(INFRA_GLOBALCON_DCMCTL, 0x00000303);
#endif /* INFRA_DCM */

		dcm_setl(MM_MMU_DCM_DIS, 0x0000007F);

		dcm_setl(PERISYS_MMU_DCM_DIS, 0x0000007F);

		dcm_sta &= ~IFR_DCM;
	}

	if (type & SMI_DCM) {
		dcm_dbg("[%s][SMI_DCM]=0x%08x\n", __func__, SMI_DCM);

		dcm_clrl(SMI_COMMON_SMI_DCM, 0x00000001);

		dcm_sta &= ~SMI_DCM;
	}

	if (type & DIS_DCM) {
		dcm_dbg("[%s][DIS_DCM]=0x%08x\n", __func__, DIS_DCM);

		dcm_writel(MMSYS_HW_DCM_DIS0, 0xFFFFFFFF);
		dcm_writel(MMSYS_HW_DCM_DIS_SET0, 0xFFFFFFFF);
		dcm_writel(MMSYS_HW_DCM_DIS_CLR0, 0x00000000);

		dcm_writel(MMSYS_HW_DCM_DIS1, 0xFFFFFFFF);
		dcm_writel(MMSYS_HW_DCM_DIS_SET1, 0xFFFFFFFF);
		dcm_writel(MMSYS_HW_DCM_DIS_CLR1, 0x00000000);

		dcm_setl(SMI_LARB0_CON_CLR, 0x00000010);

		dcm_setl(SMI_LARB4_CON_CLR, 0x00000010);

		dcm_sta &= ~DIS_DCM;
	}

	if (type & ISP_DCM) {
		dcm_dbg("[%s][ISP_DCM]=0x%08x\n", __func__, ISP_DCM);

		if (subsys_is_on(SYS_ISP) &&
		    subsys_is_on(SYS_VEN) &&
		    subsys_is_on(SYS_VEN2)) {
			dcm_setl(CTL_RAW_D_DCM_DIS, 0x024EAFE8);
			dcm_setl(CTL_DMA_DCM_DIS, 0x07FFFFFF);
			dcm_setl(CTL_RGB_DCM_DIS, 0x0000007F);
			dcm_setl(CTL_YUV_DCM_DIS, 0x000FFFFF);
			dcm_setl(CTL_TOP_DCM_DIS, 0x0000000F);

			dcm_setl(FDVT_CTRL, 0x0000001F);

			dcm_setl(JPGENC_DCM_CTRL, 0x00000001);
			dcm_setl(JPGDEC_DCM_CTRL, 0x00000001);

			dcm_writel(VENC_CLK_CG_CTRL, 0x00000000);
			dcm_clrl(VENC_CLK_DCM_CTRL, 0x00000001);

			dcm_writel(VENC_LT_CLK_CG_CTRL, 0x00000000);
			dcm_clrl(VENC_LT_CLK_DCM_CTRL, 0x00000001);

			dcm_setl(SMI_LARB2_CON_CLR, 0x00000010);
			dcm_setl(SMI_LARB3_CON_CLR, 0x00000010);
			dcm_setl(SMI_LARB5_CON_CLR, 0x00000010);

			dcm_sta &= ~ISP_DCM;
		}
	}

	if (type & VDE_DCM) {
		dcm_dbg("[%s][VDE_DCM]=0x%08x\n", __func__, VDE_DCM);

		dcm_setl(VDEC_DCM_CON, 0x00000001);

		dcm_setl(SMI_LARB1_CON_CLR, 0x00000010);

		dcm_sta &= ~VDE_DCM;
	}

	if (type & EMI_DCM) {
		dcm_dbg("[%s][EMI_DCM]=0x%08x\n", __func__, EMI_DCM);

		dcm_setl(EMI_CONM, 0xFF000000);

		dcm_sta &= ~EMI_DCM;
	}

	mutex_unlock(&dcm_lock);
}

void disable_cpu_dcm(void)
{
	if (!dcm_reg_init)
		return;

	dcm_setl(INFRA_TOPCKGEN_DCMCTL, 0x00000001);
	dcm_clrl(INFRA_TOPCKGEN_DCMCTL, 0x00000770);
}

void enable_cpu_dcm(void)
{
	if (!dcm_reg_init)
		return;

	dcm_setl(INFRA_TOPCKGEN_DCMCTL, 0x00000771);
}

/**
 * bus_dcm_set_freq_div - Decide axi's frequency divisor when entering
 * slow-idle.
 * @cmd: Command for axi's frequency divisor.
 * @request: It shows which driver calls bus_dcm_set_freq_div last time.
 * The value of request needs to be > 0. 0 means no request.
 *
 * API usage: for example
 * 1. bus_dcm_set_freq_div(BUS_DCM_FREQ_DIV_16, BUS_DCM_AUDIO); (freq_lock_requested > 0)
 * 2. bus_dcm_set_freq_div(BUS_DCM_FREQ_DEFAULT, BUS_DCM_AUDIO); (freq_lock_requested = 0)
 */
int bus_dcm_set_freq_div(unsigned int cmd, unsigned int request)
{
	static unsigned int freq_lock_requested;
	const unsigned int no_request = 0;

	if (!dcm_reg_init) {
		dcm_err("dcm is not initialized\n");
		return -EPROBE_DEFER;
	}

	mutex_lock(&dcm_lock);

	/* If one driver ever requests to set div, it needs to call BUS_DCM_FREQ_DEFAULT so that
	 * other drivers are able to set their freq_div.
	 */
	if ((freq_lock_requested != request) && (freq_lock_requested != no_request)) {
		dcm_err("bus dcm freq is locked by %d\n", freq_lock_requested);
		mutex_unlock(&dcm_lock);
		return -EBUSY;
	}

	switch (cmd) {
	case BUS_DCM_FREQ_DIV_16:
		/* DCM_CFG's bit 4:0 controls axi's freq divisor */
		dcm_writel(DCM_CFG, (dcm_readl(DCM_CFG) & ~(0x1F)) | 0x1);
		freq_lock_requested = request;
		break;
	case BUS_DCM_FREQ_DEFAULT:
		dcm_writel(DCM_CFG, (dcm_readl(DCM_CFG) & ~(0x1F)));
		freq_lock_requested = no_request;
		break;
	case BUS_DCM_FREQ_DIV_1:
	case BUS_DCM_FREQ_DIV_2:
	case BUS_DCM_FREQ_DIV_4:
	case BUS_DCM_FREQ_DIV_8:
	default:
		dcm_err("file: %s, func:%s: unknown command\n", __FILE__, __func__);
		BUG_ON(1);
		break;
	}

	mutex_unlock(&dcm_lock);

	return 0;
}
EXPORT_SYMBOL(bus_dcm_set_freq_div);

void bus_dcm_enable(void)
{
	if (!dcm_reg_init)
		return;

	dcm_setl(DCM_CFG, 0x1 << 7);
}

void bus_dcm_disable(void)
{
	if (!dcm_reg_init)
		return;

	dcm_clrl(DCM_CFG, 0x1 << 7);
}

#if INFRA_DCM
static uint32_t infra_dcm;
#endif

void disable_infra_dcm(void)
{
	if (!dcm_reg_init)
		return;

#if INFRA_DCM
	infra_dcm = dcm_readl(INFRA_GLOBALCON_DCMCTL);
	dcm_clrl(INFRA_GLOBALCON_DCMCTL, 0x100);
#endif
}

void restore_infra_dcm(void)
{
	if (!dcm_reg_init)
		return;

#if INFRA_DCM
	dcm_writel(INFRA_GLOBALCON_DCMCTL, infra_dcm);
#endif
}

static uint32_t peri_dcm;

void disable_peri_dcm(void)
{
	if (!dcm_reg_init)
		return;

	peri_dcm = dcm_readl(PERI_GLOBALCON_DCMCTL);
	dcm_clrl(PERI_GLOBALCON_DCMCTL, 0x1);
}

void restore_peri_dcm(void)
{
	if (!dcm_reg_init)
		return;

	dcm_writel(PERI_GLOBALCON_DCMCTL, peri_dcm);
}

#define dcm_attr(_name)				\
static struct kobj_attribute _name##_attr = {	\
	.attr = {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show = _name##_show,			\
	.store = _name##_store,			\
}

static const char *dcm_name[NR_DCMS] = {
	"CPU_DCM",
	"IFR_DCM",
	"PER_DCM",
	"SMI_DCM",
	"EMI_DCM",
	"DIS_DCM",
	"ISP_DCM",
	"VDE_DCM",
};

static ssize_t dcm_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	int i;
	uint32_t sta;

	len = snprintf(buf, PAGE_SIZE, "********** dcm_state dump **********\n");
	mutex_lock(&dcm_lock);

	for (i = 0; i < NR_DCMS; i++) {
		sta = dcm_sta & (0x1 << i);
		len += snprintf(buf+len, PAGE_SIZE-len,
				"[%d][%s]%s\n", i, dcm_name[i], sta ? "on" : "off");
	}

	mutex_unlock(&dcm_lock);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"\n********** dcm_state help *********\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"enable dcm:  echo enable mask(hex) > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"disable dcm: echo disable mask(hex) > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"dump reg:    echo dump mask(hex) > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"for example: echo dump 0xFF > /sys/power/dcm_state\n");

	return len;
}

static ssize_t dcm_state_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	char cmd[10];
	uint32_t mask;

	if (sscanf(buf, "%9s %x", cmd, &mask) == 2) {
		mask &= ALL_DCM;

		/*
		Need to enable MM clock before setting Smi_secure register
		to avoid system crash while screen is off
		(screen off with USB cable)
		*/
		enable_subsys_clocks();

		if (!strcmp(cmd, "enable")) {
			dcm_dump_regs(mask);
			dcm_enable(mask);
			dcm_dump_regs(mask);
		} else if (!strcmp(cmd, "disable")) {
			dcm_dump_regs(mask);
			dcm_disable(mask);
			dcm_dump_regs(mask);
		} else if (!strcmp(cmd, "dump")) {
			dcm_dump_regs(mask);
		} else {
			dcm_err("Please: cat /sys/power/dcm_state\n");
		}

		disable_subsys_clocks();

		return n;
	}

	return -EINVAL;
}
dcm_attr(dcm_state);

static struct device_node * __init get_dcm_node(void)
{
	const char *cmp = "mediatek,mt8173-dcm";
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, cmp);

	if (!node)
		dcm_err("node '%s' not found!\n", cmp);

	return node;
}

static int __init init_clk(struct device_node *node)
{
	struct clk **pclk[] = {
		&mm_sel,
		&peri_usb0,
	};

	int i;

	for (i = 0; i < ARRAY_SIZE(pclk); i++) {
		*pclk[i] = of_clk_get(node, i);
		if (IS_ERR(*pclk[i]))
			return -1;
	}

	return 0;
}

static int __init init_reg_base(struct device_node *node)
{
	void __iomem **pbase[] = {
		&topckgen_base,
		&infrasys_base,
		&perisys_base,
		&dramc0_base,
		&scpsys_base,
		&pwrap_base,
		&dramc1_base,
		&mcucfg_base,
		&emi_base,
		&m4u_base,
		&peri_iommu_base,
		&i2c0_base,
		&i2c1_base,
		&i2c2_base,
		&i2c3_base,
		&i2c4_base,
		&usb0_base,
		&msdc0_base,
		&msdc1_base,
		&msdc2_base,
		&msdc3_base,
		&mmsys_base,
		&smi_larb0_base,
		&smi_common_base,
		&smi_larb4_base,
		&smi_larb2_base,
		&cam1_base,
		&fdvt_base,
		&vdecsys_base,
		&smi_larb1_base,
		&smi_larb3_base,
		&venc_base,
		&jpgenc_base,
		&jpgdec_base,
		&smi_larb5_base,
		&venc_lt_base,
	};

	int i;

	if (!node)
		return -1;

	for (i = 0; i < ARRAY_SIZE(pbase); i++) {
		*pbase[i] = of_iomap(node, i);
		if (!*pbase[i])
			return -1;
	}

	dcm_reg_init = 1;

	return 0;
}

static int __init init_from_dt(void)
{
	struct device_node *node;
	int err;

	node = get_dcm_node();

	err = init_clk(node);
	if (err) {
		WARN(1, "init_clk(): %d", err);

		dcm_info("mm_sel:    [%p]\n", mm_sel);
		dcm_info("peri_usb0: [%p]\n", peri_usb0);

		return err;
	}

	err = init_reg_base(node);
	if (err) {
		WARN(1, "init_reg_base(): %d", err);

		dcm_info("TOPCKGEN_BASE  : [%p]\n", topckgen_base);
		dcm_info("INFRASYS_BASE  : [%p]\n", infrasys_base);
		dcm_info("PERISYS_BASE   : [%p]\n", perisys_base);
		dcm_info("DRAMC0_BASE    : [%p]\n", dramc0_base);
		dcm_info("SCPSYS_BASE    : [%p]\n", scpsys_base);
		dcm_info("PWRAP_BASE     : [%p]\n", pwrap_base);
		dcm_info("DRAMC1_BASE    : [%p]\n", dramc1_base);
		dcm_info("MCUCFG_BASE    : [%p]\n", mcucfg_base);
		dcm_info("EMI_BASE       : [%p]\n", emi_base);
		dcm_info("M4U_BASE       : [%p]\n", m4u_base);
		dcm_info("PERI_IOMMU_BASE: [%p]\n", peri_iommu_base);
		dcm_info("I2C0_BASE      : [%p]\n", i2c0_base);
		dcm_info("I2C1_BASE      : [%p]\n", i2c1_base);
		dcm_info("I2C2_BASE      : [%p]\n", i2c2_base);
		dcm_info("I2C3_BASE      : [%p]\n", i2c3_base);
		dcm_info("I2C4_BASE      : [%p]\n", i2c4_base);
		dcm_info("USB0_BASE      : [%p]\n", usb0_base);
		dcm_info("MSDC0_BASE     : [%p]\n", msdc0_base);
		dcm_info("MSDC1_BASE     : [%p]\n", msdc1_base);
		dcm_info("MSDC2_BASE     : [%p]\n", msdc2_base);
		dcm_info("MSDC3_BASE     : [%p]\n", msdc3_base);
		dcm_info("MMSYS_BASE     : [%p]\n", mmsys_base);
		dcm_info("SMI_LARB0_BASE : [%p]\n", smi_larb0_base);
		dcm_info("SMI_COMMON_BASE: [%p]\n", smi_common_base);
		dcm_info("SMI_LARB4_BASE : [%p]\n", smi_larb4_base);
		dcm_info("SMI_LARB2_BASE : [%p]\n", smi_larb2_base);
		dcm_info("CAM1_BASE      : [%p]\n", cam1_base);
		dcm_info("FDVT_BASE      : [%p]\n", fdvt_base);
		dcm_info("VDECSYS_BASE   : [%p]\n", vdecsys_base);
		dcm_info("SMI_LARB1_BASE : [%p]\n", smi_larb1_base);
		dcm_info("SMI_LARB3_BASE : [%p]\n", smi_larb3_base);
		dcm_info("VENC_BASE      : [%p]\n", venc_base);
		dcm_info("JPGENC_BASE    : [%p]\n", jpgenc_base);
		dcm_info("JPGDEC_BASE    : [%p]\n", jpgdec_base);
		dcm_info("SMI_LARB5_BASE : [%p]\n", smi_larb5_base);
		dcm_info("VENC_LT_BASE   : [%p]\n", venc_lt_base);

		return err;
	}

	return 0;
}

static int __init dcm_init(void)
{
	/* dcm_init() must be called after CCF init or init_clk() fail. */

	int err = 0;

	err = init_from_dt();
	if (err) {
		dcm_err("init_from_dt(): %d\n", err);
		return err;
	}

	dcm_enable(ALL_DCM);

#if DCM_SYS_POWER
	err = sysfs_create_file(power_kobj, &dcm_state_attr.attr);
#else
	err = sysfs_create_file(kernel_kobj, &dcm_state_attr.attr);
#endif

	if (err)
		dcm_err("[%s]: fail to create sysfs\n", __func__);

	return err;
}
module_init(dcm_init);

void mt_dcm_init(void)
{
}


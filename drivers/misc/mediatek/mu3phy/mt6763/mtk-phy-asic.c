/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mtk-phy.h"

#ifdef CONFIG_PROJECT_PHY
/*#include <mach/mt_pm_ldo.h>*/
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#include <linux/clk.h>
#endif
#include <linux/io.h>
#include "mtk-phy-asic.h"
#include "mu3d_hal_osal.h"
#ifdef CONFIG_MTK_UART_USB_SWITCH
#include "mu3d_hal_usb_drv.h"
#endif

#ifdef FOR_BRING_UP
#define enable_clock(x, y)
#define disable_clock(x, y)
#define hwPowerOn(x, y, z)
#define hwPowerDown(x, y)
#define set_ada_ssusb_xtal_ck(x)
#endif

#include <mt-plat/upmu_common.h>

#ifdef CONFIG_OF
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mt-plat/mtk_chip.h>


#ifndef CONFIG_MTK_CLKMGR
static struct clk *musb_clk;
#endif

bool sib_mode;
int clk_count;
static DEFINE_SPINLOCK(musb_reg_clock_lock);


#ifdef USB_CLK_DEBUG
void __iomem *usb_debug_clk_infracfg_base;
#define MODULE_SW_CG_2_SET	(usb_debug_clk_infracfg_base + 0xa4)
#define MODULE_SW_CG_2_CLR	(usb_debug_clk_infracfg_base + 0xa8)
#define MODULE_SW_CG_2_STA	(usb_debug_clk_infracfg_base + 0xac)
static bool get_clk_io = true;
#endif

static unsigned int verion;
static int usb20_phy_rev6;

void usb_hal_dpidle_request(int mode)
{

	/* no SPM related required */
	return;

}

static bool usb_enable_clock(bool enable)
{
	unsigned long flags;

	spin_lock_irqsave(&musb_reg_clock_lock, flags);
		if (enable && clk_count == 0) {
#ifdef CONFIG_MTK_CLKMGR
			writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000010),
			(void __iomem *)AP_PLL_CON0);
			enable_clock(MT_CG_PERI_USB0, "USB30");
#else
			writel(readl(ap_pll_con0) | (0x00000010),
			(void __iomem *)ap_pll_con0);
			clk_enable(musb_clk);
#endif
		} else if (!enable && clk_count == 1) {
#ifdef CONFIG_MTK_CLKMGR
			disable_clock(MT_CG_PERI_USB0, "USB30");
			writel(readl((void __iomem *)AP_PLL_CON0) & ~(0x00000010),
			(void __iomem *)AP_PLL_CON0);
#else
			clk_disable(musb_clk);
			writel(readl((void __iomem *)ap_pll_con0) & ~(0x00000010),
			(void __iomem *)ap_pll_con0);

#endif
		}

		if (enable)
			clk_count++;
		else
			clk_count = (clk_count == 0) ? 0 : (clk_count-1);

			spin_unlock_irqrestore(&musb_reg_clock_lock, flags);

#ifdef USB_CLK_DEBUG
	if (get_clk_io) {
		struct device_node *node;

		get_clk_io = false;
		node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-infrasys");
		usb_debug_clk_infracfg_base = of_iomap(node, 0);
		if (!usb_debug_clk_infracfg_base)
			pr_err("[CLK_INFRACFG_AO] base failed\n");
	}
	if (!IS_ERR(musb_clk))
		pr_err("SSUSB musb clock is okay, enabel: %d\n", enable);
	else
		pr_err("SSUSB musb clock is fail, enabel: %d\n", enable);
	/*bit1: ssusb_top_cg_sta  (0: clock enable  1: clock disable)*/
	pr_err("SSUSB MODULE_SW_CG_2_STA  = 0x%08x\n", DRV_Reg32(MODULE_SW_CG_2_STA));
#endif
	return 1;
}


#ifdef NEVER
/*Turn on/off ADA_SSUSB_XTAL_CK 26MHz*/
void enable_ssusb_xtal_clock(bool enable)
{
	if (enable) {
		/*
		 * 1 *AP_PLL_CON0 =| 0x1 [0]=1: RG_LTECLKSQ_EN
		 * 2 Wait PLL stable (100us)
		 * 3 *AP_PLL_CON0 =| 0x2 [1]=1: RG_LTECLKSQ_LPF_EN
		 * 4 *AP_PLL_CON2 =| 0x1 [0]=1: DA_REF2USB_TX_EN
		 * 5 Wait PLL stable (100us)
		 * 6 *AP_PLL_CON2 =| 0x2 [1]=1: DA_REF2USB_TX_LPF_EN
		 * 7 *AP_PLL_CON2 =| 0x4 [2]=1: DA_REF2USB_TX_OUT_EN
		 */
		writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000001),
		       (void __iomem *)AP_PLL_CON0);
		/*Wait 100 usec */
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000002),
		       (void __iomem *)AP_PLL_CON0);

		writel(readl((void __iomem *)AP_PLL_CON2) | (0x00000001),
		       (void __iomem *)AP_PLL_CON2);

		/*Wait 100 usec */
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON2) | (0x00000002),
		       (void __iomem *)AP_PLL_CON2);

		writel(readl((void __iomem *)AP_PLL_CON2) | (0x00000004),
		       (void __iomem *)AP_PLL_CON2);
	} else {
		/*
		 * AP_PLL_CON2 &= 0xFFFFFFF8        [2]=0: DA_REF2USB_TX_OUT_EN
		 *                                  [1]=0: DA_REF2USB_TX_LPF_EN
		 *                                  [0]=0: DA_REF2USB_TX_EN
		 */
		/* writel(readl((void __iomem *)AP_PLL_CON2)&~(0x00000007), */
		/* (void __iomem *)AP_PLL_CON2); */
	}
}

/*Turn on/off AD_LTEPLL_SSUSB26M_CK 26MHz*/
void enable_ssusb26m_ck(bool enable)
{
	if (enable) {
		/*
		 * 1 *AP_PLL_CON0 =| 0x1 [0]=1: RG_LTECLKSQ_EN
		 * 2 Wait PLL stable (100us)
		 * 3 *AP_PLL_CON0 =| 0x2 [1]=1: RG_LTECLKSQ_LPF_EN
		 */
		writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000001),
		       (void __iomem *)AP_PLL_CON0);
		/*Wait 100 usec */
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000002),
		       (void __iomem *)AP_PLL_CON0);

	} else {
		/*
		 * AP_PLL_CON2 &= 0xFFFFFFF8        [2]=0: DA_REF2USB_TX_OUT_EN
		 *                                  [1]=0: DA_REF2USB_TX_LPF_EN
		 *                                  [0]=0: DA_REF2USB_TX_EN
		 */
		/* writel(readl((void __iomem *)AP_PLL_CON2)&~(0x00000007), */
		/* (void __iomem *)AP_PLL_CON2); */
	}
}
#endif				/* NEVER */

void usb20_pll_settings(bool host, bool forceOn)
{
	if (host) {
		if (forceOn) {
			U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYA_REG6, RG_SSUSB_RESERVE6_OFST,
				RG_SSUSB_RESERVE6, 0x1);
			if (verion < CHIP_SW_VER_02) {
				U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_TX_BIAS_EN_OFST,
					RG_USB20_TX_BIAS_EN, 0x1);
				U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_PHY_REV_6_OFST,
					RG_USB20_PHY_REV_6, 0x1);
			}
		} else {
			U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYA_REG6, RG_SSUSB_RESERVE6_OFST,
				RG_SSUSB_RESERVE6, 0x0);
			if (verion < CHIP_SW_VER_02) {
				U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_TX_BIAS_EN_OFST,
					RG_USB20_TX_BIAS_EN, 0x0);
				U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_PHY_REV_6_OFST,
					RG_USB20_PHY_REV_6, 0x0);
			}
		}
	}
}

void usb20_rev6_setting(int value, bool is_update)
{
	if (is_update)
		usb20_phy_rev6 = value;

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_PHY_REV_6_OFST,
		RG_USB20_PHY_REV_6, value);
}


#ifdef CONFIG_MTK_UART_USB_SWITCH
bool in_uart_mode;
void uart_usb_switch_dump_register(void)
{
	/* ADA_SSUSB_XTAL_CK:26MHz */
	/*set_ada_ssusb_xtal_ck(1); */

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(true);
	udelay(50);

# ifdef CONFIG_MTK_FPGA
	pr_debug("[MUSB]addr: 0x6B, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYDTM0 + 0x3));
	pr_debug("[MUSB]addr: 0x6E, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYDTM1 + 0x2));
	pr_debug("[MUSB]addr: 0x22, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYACR4 + 0x2));
	pr_debug("[MUSB]addr: 0x68, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYDTM0));
	pr_debug("[MUSB]addr: 0x6A, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYDTM0 + 0x2));
	pr_debug("[MUSB]addr: 0x1A, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYACR6 + 0x2));
#else
#if 0
	os_printk(K_INFO, "[MUSB]addr: 0x6B, value: %x\n", U3PhyReadReg8(U3D_U2PHYDTM0 + 0x3));
	os_printk(K_INFO, "[MUSB]addr: 0x6E, value: %x\n", U3PhyReadReg8(U3D_U2PHYDTM1 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x22, value: %x\n", U3PhyReadReg8(U3D_U2PHYACR4 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x68, value: %x\n", U3PhyReadReg8(U3D_U2PHYDTM0));
	os_printk(K_INFO, "[MUSB]addr: 0x6A, value: %x\n", U3PhyReadReg8(U3D_U2PHYDTM0 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x1A, value: %x\n", U3PhyReadReg8(U3D_USBPHYACR6 + 0x2));
#else
	os_printk(K_INFO, "[MUSB]addr: 0x18, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6));
	os_printk(K_INFO, "[MUSB]addr: 0x20, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4));
	os_printk(K_INFO, "[MUSB]addr: 0x68, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0));
	os_printk(K_INFO, "[MUSB]addr: 0x6C, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1));
#endif
#endif

	/* ADA_SSUSB_XTAL_CK:26MHz */
	/*set_ada_ssusb_xtal_ck(0); */

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(false);

	/*os_printk(K_INFO, "[MUSB]addr: 0x110020B0 (UART0), value: %x\n\n",
	 *	  DRV_Reg8(ap_uart0_base + 0xB0));
	*/
}

bool usb_phy_check_in_uart_mode(void)
{
	PHY_INT32 usb_port_mode;

	/* ADA_SSUSB_XTAL_CK:26MHz */
	/*set_ada_ssusb_xtal_ck(1); */

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(true);

	udelay(50);
	usb_port_mode = U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0) >> RG_UART_MODE_OFST;

	/* ADA_SSUSB_XTAL_CK:26MHz */
	/*set_ada_ssusb_xtal_ck(0);*/

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(false);
	os_printk(K_INFO, "%s+ usb_port_mode = %d\n", __func__, usb_port_mode);

	if (usb_port_mode == 0x1)
		return true;
	else
		return false;
}

void usb_phy_switch_to_uart(void)
{
	PHY_INT32 ret;

	if (usb_phy_check_in_uart_mode()) {
		os_printk(K_INFO, "%s+ UART_MODE\n", __func__);
		return;
	}

	os_printk(K_INFO, "%s+ USB_MODE\n", __func__);

	/*---POWER-----*/
	/*AVDD18_USB_P0 is always turned on. The driver does _NOT_ need to control it. */
	/*hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_3300, "VDD33_USB_P0"); */
	ret = pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
	if (ret)
		pr_debug("VUSB33 enable FAIL!!!\n");

	/* Set RG_VUSB10_ON as 1 after VDD10 Ready */
	/*hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1000, "VDD10_USB_P0"); */
	ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
	if (ret)
		pr_debug("VA10 enable FAIL!!!\n");

	ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x00);
	if (ret)
		pr_debug("VA10 output selection to 0.9v FAIL!!!\n");

	/* ADA_SSUSB_XTAL_CK:26MHz */
	/*set_ada_ssusb_xtal_ck(1); */

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(true);
	udelay(50);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);

	/* Set RG_SUSPENDM to 1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST, RG_SUSPENDM, 1);

	/* force suspendm = 1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 1);

	/* Set ru_uart_mode to 2'b01 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_UART_MODE_OFST, RG_UART_MODE, 1);

	/* Set RG_UART_EN to 1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_TX_OE_OFST, FORCE_UART_TX_OE, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_BIAS_EN_OFST, FORCE_UART_BIAS_EN,
			  1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_TX_OE_OFST, RG_UART_TX_OE, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_BIAS_EN_OFST, RG_UART_BIAS_EN, 1);

	/* Set RG_USB20_DM_100K_EN to 1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DM_100K_EN_OFST,
			  RG_USB20_DM_100K_EN, 1);

	/* ADA_SSUSB_XTAL_CK:26MHz */
	/*set_ada_ssusb_xtal_ck(0); */

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(false);

	/* GPIO Selection */
	DRV_WriteReg32(ap_uart0_base + 0xB0, 0x1);

	in_uart_mode = true;
}


void usb_phy_switch_to_usb(void)
{
	in_uart_mode = false;
	/* GPIO Selection */
	DRV_WriteReg32(ap_uart0_base + 0xB0, 0x0);	/* set */

	/* clear force_uart_en */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 0);

	phy_init_soc(u3phy);

	/* disable the USB clock turned on in phy_init_soc() */
	/* ADA_SSUSB_XTAL_CK:26MHz */
	/*set_ada_ssusb_xtal_ck(0); */

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(false);
}
#endif

#define RG_SSUSB_VUSB10_ON (1<<5)
#define RG_SSUSB_VUSB10_ON_OFST (5)

#ifdef CONFIG_MTK_SIB_USB_SWITCH
void usb_phy_sib_enable_switch(bool enable)
{
	/*
	 * It's MD debug usage. No need to care low power.
	 * Thus, no power off BULK and Clock at the end of function.
	 * MD SIB still needs these power and clock source.
	 */
	pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
	pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
	pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x00);

	usb_enable_clock(true);
	udelay(50);
	/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
			  RG_SSUSB_VUSB10_ON, 1);
	/* SSUSB_IP_SW_RST = 0 */
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x700), 0x00031000);
	/* SSUSB_IP_HOST_PDN = 0 */
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x704), 0x00000000);
	/* SSUSB_IP_DEV_PDN = 0 */
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x708), 0x00000000);
	/* SSUSB_IP_PCIE_PDN = 0 */
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x70C), 0x00000000);
	/* SSUSB_U3_PORT_DIS/SSUSB_U3_PORT_PDN = 0*/
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x730), 0x0000000C);

	/*
	 * USBMAC mode is 0x62910002 (bit 1)
	 * MDSIB  mode is 0x62910008 (bit 3)
	 * 0x0629 just likes a signature. Can't be removed.
	 */
	if (enable) {
		U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif2_base+0x300), 0x62910008);
		sib_mode = true;
	} else {
		U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif2_base+0x300), 0x62910002);
		sib_mode = false;
	}
}

bool usb_phy_sib_enable_switch_status(void)
{
	int reg;
	bool ret;

	pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
	pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
	pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x00);

	usb_enable_clock(true);
	udelay(50);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
			  RG_SSUSB_VUSB10_ON, 1);

	reg = U3PhyReadReg32((phys_addr_t) (uintptr_t) (u3_sif2_base+0x300));
	if (reg == 0x62910008)
		ret = true;
	else
		ret = false;

	return ret;
}
#endif


/*This "power on/initial" sequence refer to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
PHY_INT32 phy_init_soc(struct u3phy_info *info)
{
	PHY_INT32 ret;

	os_printk(K_INFO, "%s+\n", __func__);

	/*This power on sequence refers to Sheet .1 of "6593_USB_PORT0_PWR Sequence 20130729.xls" */

	/*---POWER-----*/
	/*AVDD18_USB_P0 is always turned on. The driver does _NOT_ need to control it. */
	/*hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_3300, "VDD33_USB_P0"); */
	ret = pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
	if (ret)
		pr_debug("VUSB33 enable FAIL!!!\n");

	/* Set RG_VUSB10_ON as 1 after VDD10 Ready */
	/*hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1000, "VDD10_USB_P0"); */
	ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
	if (ret)
		pr_debug("VA10 enable FAIL!!!\n");

	ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x00);
	if (ret)
		pr_debug("VA10 output selection to 0.9v FAIL!!!\n");

	/*---CLOCK-----*/
	/* ADA_SSUSB_XTAL_CK:26MHz */
	/*set_ada_ssusb_xtal_ck(1); */

	/* AD_LTEPLL_SSUSB26M_CK:26MHz always on */
	/* It seems that when turning on ADA_SSUSB_XTAL_CK, AD_LTEPLL_SSUSB26M_CK will also turn on. */
	/* enable_ssusb26m_ck(true); */

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(true);

	/* AD_SSUSB_48M_CK:48MHz */
	/* It seems that when turning on f_fusb30_ck, AD_SSUSB_48M_CK will also turn on. */

	/*Wait 50 usec */
	udelay(50);

	/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
			  RG_SSUSB_VUSB10_ON, 1);

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!in_uart_mode) {
		/*switch to USB function. (system register, force ip into usb mode) */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN,
				  0);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST,
				  RG_USB20_GPIO_CTL, 0);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST,
				  USB20_GPIO_MODE, 0);
		/* Set ru_uart_mode to 2'b00 */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_UART_MODE_OFST, RG_UART_MODE, 0);
		/*dm_100k disable */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DM_100K_EN_OFST,
				  RG_USB20_DM_100K_EN, 0);
		/*Release force suspendm.  (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll) */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM,
				  0);
	}
	/*DP/DM BC1.1 path Disable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);
	/*dp_100k disable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DP_100K_MODE_OFST,
			  RG_USB20_DP_100K_MODE, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_DP_100K_EN_OFST, USB20_DP_100K_EN, 0);
#if defined(CONFIG_MTK_HDMI_SUPPORT) || defined(MTK_USB_MODE1)
	os_printk(K_INFO, "%s- USB PHY Driving Tuning Mode 1 Settings.\n", __func__);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
			  RG_USB20_HS_100U_U3_EN, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR1, RG_USB20_VRT_VREF_SEL_OFST,
			  RG_USB20_VRT_VREF_SEL, 5);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR1, RG_USB20_TERM_VREF_SEL_OFST,
			  RG_USB20_TERM_VREF_SEL, 5);
#else
	/*Change 100uA current switch to SSUSB */
	/* U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
	 *		  RG_USB20_HS_100U_U3_EN, 1);
	 */
#endif
	/*OTG Enable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_OTG_VBUSCMP_EN_OFST,
			  RG_USB20_OTG_VBUSCMP_EN, 1);
	/*Pass RX sensitivity HQA requirement */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH, 0x2);
#else
	/*switch to USB function. (system register, force ip into usb mode) */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);
	/*DP/DM BC1.1 path Disable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);
	/*dp_100k disable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DP_100K_MODE_OFST,
			  RG_USB20_DP_100K_MODE, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_DP_100K_EN_OFST, USB20_DP_100K_EN, 0);
	/*dm_100k disable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DM_100K_EN_OFST,
			  RG_USB20_DM_100K_EN, 0);
#if !defined(CONFIG_MTK_HDMI_SUPPORT) && !defined(MTK_USB_MODE1)
	/*Change 100uA current switch to SSUSB */
	/*U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
	 *		  RG_USB20_HS_100U_U3_EN, 1);
	 */
#endif
	/*OTG Enable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_OTG_VBUSCMP_EN_OFST,
			  RG_USB20_OTG_VBUSCMP_EN, 1);
	/*Pass RX sensitivity HQA requirement */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH, 0x2);
	/*Release force suspendm.  (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll) */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 0);
#endif

	/*Wait 800 usec */
	udelay(800);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, FORCE_VBUSVALID_OFST, FORCE_VBUSVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, FORCE_AVALID_OFST, FORCE_AVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, FORCE_SESSEND_OFST, FORCE_SESSEND, 1);

	/* USB PLL Force settings */
	usb20_pll_settings(false, false);

	os_printk(K_DEBUG, "%s-\n", __func__);

	return PHY_TRUE;
}

PHY_INT32 u2_slew_rate_calibration(struct u3phy_info *info)
{
	PHY_INT32 i = 0;
	PHY_INT32 fgRet = 0;
	PHY_INT32 u4FmOut = 0;
	PHY_INT32 u4Tmp = 0;

	os_printk(K_DEBUG, "%s\n", __func__);

	/* => RG_USB20_HSTX_SRCAL_EN = 1 */
	/* enable USB ring oscillator */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HSTX_SRCAL_EN_OFST,
			  RG_USB20_HSTX_SRCAL_EN, 1);

	/* wait 1us */
	udelay(1);

	/* => USBPHY base address + 0x110 = 1 */
	/* Enable free run clock */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x110)
			  , RG_FRCK_EN_OFST, RG_FRCK_EN, 0x1);

	/* => USBPHY base address + 0x100 = 0x04 */
	/* Setting cyclecnt */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x100)
			  , RG_CYCLECNT_OFST, RG_CYCLECNT, 0x400);

	/* => USBPHY base address + 0x100 = 0x01 */
	/* Enable frequency meter */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x100)
			  , RG_FREQDET_EN_OFST, RG_FREQDET_EN, 0x1);

	os_printk(K_DEBUG, "Freq_Valid=(0x%08X)\n",
		  U3PhyReadReg32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x110)));

	mdelay(1);

	/* wait for FM detection done, set 10ms timeout */
	for (i = 0; i < 10; i++) {
		/* => USBPHY base address + 0x10C = FM_OUT */
		/* Read result */
		u4FmOut = U3PhyReadReg32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x10C));
		os_printk(K_DEBUG, "FM_OUT value: u4FmOut = %d(0x%08X)\n", u4FmOut, u4FmOut);

		/* check if FM detection done */
		if (u4FmOut != 0) {
			fgRet = 0;
			os_printk(K_DEBUG, "FM detection done! loop = %d\n", i);
			break;
		}
		fgRet = 1;
		mdelay(1);
	}
	/* => USBPHY base address + 0x100 = 0x00 */
	/* Disable Frequency meter */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x100)
			  , RG_FREQDET_EN_OFST, RG_FREQDET_EN, 0);

	/* => USBPHY base address + 0x110 = 0x00 */
	/* Disable free run clock */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x110)
			  , RG_FRCK_EN_OFST, RG_FRCK_EN, 0);

	/* RG_USB20_HSTX_SRCTRL[2:0] = (1024/FM_OUT) * reference clock frequency * 0.028 */
	if (u4FmOut == 0) {
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HSTX_SRCTRL_OFST,
				  RG_USB20_HSTX_SRCTRL, 0x4);
		fgRet = 1;
	} else {
		/* set reg = (1024/FM_OUT) * REF_CK * U2_SR_COEF_E60802 / 1000 (round to the nearest digits) */
		/* u4Tmp = (((1024 * REF_CK * U2_SR_COEF_E60802) / u4FmOut) + 500) / 1000; */
		u4Tmp = (1024 * REF_CK * U2_SR_COEF_E60802) / (u4FmOut * 1000);
		os_printk(K_DEBUG, "SR calibration value u1SrCalVal = %d\n", (PHY_UINT8) u4Tmp);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HSTX_SRCTRL_OFST,
				  RG_USB20_HSTX_SRCTRL, u4Tmp);
	}

	/* => RG_USB20_HSTX_SRCAL_EN = 0 */
	/* disable USB ring oscillator */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HSTX_SRCAL_EN_OFST,
			  RG_USB20_HSTX_SRCAL_EN, 0);

	return fgRet;
}

/*This "save current" sequence refers to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
void usb_phy_savecurrent(unsigned int clk_on)
{
	PHY_INT32 ret;

	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	if (sib_mode) {
		pr_err("%s sib_mode can't savecurrent\n", __func__);
		return;
	}

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!in_uart_mode) {
		/*switch to USB function. (system register, force ip into usb mode) */
		/* force_uart_en      1'b0 */
		/* U3D_U2PHYDTM0 FORCE_UART_EN */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN,
				  0);
		/* RG_UART_EN         1'b0 */
		/* U3D_U2PHYDTM1 RG_UART_EN */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);

		/*let suspendm=1, enable usb 480MHz pll */
		/* RG_SUSPENDM 1'b1 */
		/* U3D_U2PHYDTM0 RG_SUSPENDM */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST, RG_SUSPENDM, 1);

		/*force_suspendm=1 */
		/* force_suspendm        1'b1 */
		/* U3D_U2PHYDTM0 FORCE_SUSPENDM */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM,
				  1);
	}
	/* rg_usb20_gpio_ctl  1'b0 */
	/* U3D_U2PHYACR4 RG_USB20_GPIO_CTL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	/* usb20_gpio_mode       1'b0 */
	/* U3D_U2PHYACR4 USB20_GPIO_MODE */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);
#else
	/*switch to USB function. (system register, force ip into usb mode) */
	/* force_uart_en      1'b0 */
	/* U3D_U2PHYDTM0 FORCE_UART_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	/* RG_UART_EN         1'b0 */
	/* U3D_U2PHYDTM1 RG_UART_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
	/* rg_usb20_gpio_ctl  1'b0 */
	/* U3D_U2PHYACR4 RG_USB20_GPIO_CTL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	/* usb20_gpio_mode       1'b0 */
	/* U3D_U2PHYACR4 USB20_GPIO_MODE */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);

	/*let suspendm=1, enable usb 480MHz pll */
	/* RG_SUSPENDM 1'b1 */
	/* U3D_U2PHYDTM0 RG_SUSPENDM */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST, RG_SUSPENDM, 1);

	/*force_suspendm=1 */
	/* force_suspendm        1'b1 */
	/* U3D_U2PHYDTM0 FORCE_SUSPENDM */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 1);
#endif
	/*Wait USBPLL stable. */
	/* Wait 2 ms. */
	udelay(2000);

	/* RG_DPPULLDOWN 1'b1 */
	/* U3D_U2PHYDTM0 RG_DPPULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DPPULLDOWN_OFST, RG_DPPULLDOWN, 1);

	/* RG_DMPULLDOWN 1'b1 */
	/* U3D_U2PHYDTM0 RG_DMPULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DMPULLDOWN_OFST, RG_DMPULLDOWN, 1);

	/* RG_XCVRSEL[1:0] 2'b01 */
	/* U3D_U2PHYDTM0 RG_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_XCVRSEL_OFST, RG_XCVRSEL, 0x1);

	/* RG_TERMSEL    1'b1 */
	/* U3D_U2PHYDTM0 RG_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_TERMSEL_OFST, RG_TERMSEL, 1);

	/* RG_DATAIN[3:0]        4'b0000 */
	/* U3D_U2PHYDTM0 RG_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DATAIN_OFST, RG_DATAIN, 0);

	/* force_dp_pulldown     1'b1 */
	/* U3D_U2PHYDTM0 FORCE_DP_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DP_PULLDOWN_OFST, FORCE_DP_PULLDOWN,
			  1);

	/* force_dm_pulldown     1'b1 */
	/* U3D_U2PHYDTM0 FORCE_DM_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DM_PULLDOWN_OFST, FORCE_DM_PULLDOWN,
			  1);

	/* force_xcversel        1'b1 */
	/* U3D_U2PHYDTM0 FORCE_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_XCVRSEL_OFST, FORCE_XCVRSEL, 1);

	/* force_termsel 1'b1 */
	/* U3D_U2PHYDTM0 FORCE_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_TERMSEL_OFST, FORCE_TERMSEL, 1);

	/* force_datain  1'b1 */
	/* U3D_U2PHYDTM0 FORCE_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DATAIN_OFST, FORCE_DATAIN, 1);

	/*DP/DM BC1.1 path Disable */
	/* RG_USB20_BC11_SW_EN 1'b0 */
	/* U3D_USBPHYACR6 RG_USB20_BC11_SW_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);

	/*OTG Disable */
	/* RG_USB20_OTG_VBUSCMP_EN 1b0 */
	/* U3D_USBPHYACR6 RG_USB20_OTG_VBUSCMP_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_OTG_VBUSCMP_EN_OFST,
			  RG_USB20_OTG_VBUSCMP_EN, 0);

	/*Change 100uA current switch to USB2.0 */
	/* RG_USB20_HS_100U_U3_EN        1'b0 */
	/* U3D_USBPHYACR5 RG_USB20_HS_100U_U3_EN */
	/* U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
	 *		  RG_USB20_HS_100U_U3_EN, 0);
	 */

	/* wait 800us */
	udelay(800);

	/*let suspendm=0, set utmi into analog power down */
	/* RG_SUSPENDM 1'b0 */
	/* U3D_U2PHYDTM0 RG_SUSPENDM */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST, RG_SUSPENDM, 0);

	/* wait 1us */
	udelay(1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_VBUSVALID_OFST, RG_VBUSVALID, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_AVALID_OFST, RG_AVALID, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_SESSEND_OFST, RG_SESSEND, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_PHY_REV_6_OFST,
		RG_USB20_PHY_REV_6, 0);

	/* USB PLL Force settings */
	usb20_pll_settings(false, false);


	/* TODO:
	 * Turn off internal 48Mhz PLL if there is no other hardware module is
	 * using the 48Mhz clock -the control register is in clock document
	 * Turn off SSUSB reference clock (26MHz)
	 */
	if (clk_on) {
		/*---CLOCK-----*/
		/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
				  RG_SSUSB_VUSB10_ON, 0);

		/* Wait 10 usec. */
		udelay(10);

		/* f_fusb30_ck:125MHz */
		usb_enable_clock(false);

		/* ADA_SSUSB_XTAL_CK:26MHz */
		/*set_ada_ssusb_xtal_ck(0); */

		/*---POWER-----*/
		/* Set RG_VUSB10_ON as 1 after VDD10 Ready */
		/*hwPowerDown(MT6331_POWER_LDO_VUSB10, "VDD10_USB_P0"); */
		ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x00);
	}

	os_printk(K_INFO, "%s-\n", __func__);
}

/*This "recovery" sequence refers to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
void usb_phy_recover(unsigned int clk_on)
{
	PHY_INT32 ret;
	PHY_INT32 evalue;

	os_printk(K_DEBUG, "%s clk_on=%d+\n", __func__, clk_on);

	if (!clk_on) {
		/*---POWER-----*/
		/*AVDD18_USB_P0 is always turned on. The driver does _NOT_ need to control it. */
		/*hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_3300, "VDD33_USB_P0"); */
		ret = pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
		if (ret)
			pr_debug("VUSB33 enable FAIL!!!\n");

		/* Set RG_VUSB10_ON as 1 after VDD10 Ready */
		/*hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1000, "VDD10_USB_P0"); */
		ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
		if (ret)
			pr_debug("VA10 enable FAIL!!!\n");

		ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x00);
		if (ret)
			pr_debug("VA10 output selection to 0.9v FAIL!!!\n");

		/*---CLOCK-----*/
		/* ADA_SSUSB_XTAL_CK:26MHz */
		/*set_ada_ssusb_xtal_ck(1); */

		/* AD_LTEPLL_SSUSB26M_CK:26MHz always on */
		/* It seems that when turning on ADA_SSUSB_XTAL_CK, AD_LTEPLL_SSUSB26M_CK will also turn on. */
		/* enable_ssusb26m_ck(true); */

		/* f_fusb30_ck:125MHz */
		usb_enable_clock(true);

		/* AD_SSUSB_48M_CK:48MHz */
		/* It seems that when turning on f_fusb30_ck, AD_SSUSB_48M_CK will also turn on. */

		/* Wait 50 usec. (PHY 3.3v & 1.8v power stable time) */
		udelay(50);

		/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
				  RG_SSUSB_VUSB10_ON, 1);
	}

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!in_uart_mode) {
		/*switch to USB function. (system register, force ip into usb mode) */
		/* force_uart_en      1'b0 */
		/* U3D_U2PHYDTM0 FORCE_UART_EN */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN,
				  0);
		/* RG_UART_EN         1'b0 */
		/* U3D_U2PHYDTM1 RG_UART_EN */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
		/*force_suspendm        1'b0 */
		/* U3D_U2PHYDTM0 FORCE_SUSPENDM */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM,
				  0);
	}
	/* rg_usb20_gpio_ctl  1'b0 */
	/* U3D_U2PHYACR4 RG_USB20_GPIO_CTL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	/* usb20_gpio_mode       1'b0 */
	/* U3D_U2PHYACR4 USB20_GPIO_MODE */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);
#else
	/*switch to USB function. (system register, force ip into usb mode) */
	/* force_uart_en      1'b0 */
	/* U3D_U2PHYDTM0 FORCE_UART_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	/* RG_UART_EN         1'b0 */
	/* U3D_U2PHYDTM1 RG_UART_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
	/* rg_usb20_gpio_ctl  1'b0 */
	/* U3D_U2PHYACR4 RG_USB20_GPIO_CTL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	/* usb20_gpio_mode       1'b0 */
	/* U3D_U2PHYACR4 USB20_GPIO_MODE */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);

	/*Release force suspendm. (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll) */
	/*force_suspendm        1'b0 */
	/* U3D_U2PHYDTM0 FORCE_SUSPENDM */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 0);
#endif

	/* RG_DPPULLDOWN 1'b0 */
	/* U3D_U2PHYDTM0 RG_DPPULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DPPULLDOWN_OFST, RG_DPPULLDOWN, 0);

	/* RG_DMPULLDOWN 1'b0 */
	/* U3D_U2PHYDTM0 RG_DMPULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DMPULLDOWN_OFST, RG_DMPULLDOWN, 0);

	/* RG_XCVRSEL[1:0]       2'b00 */
	/* U3D_U2PHYDTM0 RG_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_XCVRSEL_OFST, RG_XCVRSEL, 0);

	/* RG_TERMSEL    1'b0 */
	/* U3D_U2PHYDTM0 RG_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_TERMSEL_OFST, RG_TERMSEL, 0);

	/* RG_DATAIN[3:0]        4'b0000 */
	/* U3D_U2PHYDTM0 RG_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DATAIN_OFST, RG_DATAIN, 0);

	/* force_dp_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DP_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DP_PULLDOWN_OFST, FORCE_DP_PULLDOWN,
			  0);

	/* force_dm_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DM_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DM_PULLDOWN_OFST, FORCE_DM_PULLDOWN,
			  0);

	/* force_xcversel        1'b0 */
	/* U3D_U2PHYDTM0 FORCE_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_XCVRSEL_OFST, FORCE_XCVRSEL, 0);

	/* force_termsel 1'b0 */
	/* U3D_U2PHYDTM0 FORCE_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_TERMSEL_OFST, FORCE_TERMSEL, 0);

	/* force_datain  1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DATAIN_OFST, FORCE_DATAIN, 0);

	/*DP/DM BC1.1 path Disable */
	/* RG_USB20_BC11_SW_EN   1'b0 */
	/* U3D_USBPHYACR6 RG_USB20_BC11_SW_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);

	/*OTG Enable */
	/* RG_USB20_OTG_VBUSCMP_EN       1b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_OTG_VBUSCMP_EN_OFST,
			  RG_USB20_OTG_VBUSCMP_EN, 1);
	/*Pass RX sensitivity HQA requirement */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH, 0x2);

#if defined(CONFIG_MTK_HDMI_SUPPORT) || defined(MTK_USB_MODE1)
	os_printk(K_INFO, "%s- USB PHY Driving Tuning Mode 1 Settings.\n", __func__);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
			  RG_USB20_HS_100U_U3_EN, 0);
#else
	/*Change 100uA current switch to SSUSB */
	/* RG_USB20_HS_100U_U3_EN        1'b1 */
	/* U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
	 *		  RG_USB20_HS_100U_U3_EN, 1);
	 */
#endif

#if 0
	/*
	 * 1 RG_SSUSB_TX_EIDLE_CM<3:0> / 1100-->1110 / low-power
	 *   E-idle common mode(650mV to 600mV) - 0x11290b18 bit [31:28]
	 * 2 RG_SSUSB_CDR_BIR_LTD0[4:0] / 5'b01000-->5'b01100 / Increase BW - 0x1128095c bit [12:8]
	 * 3 RG_XXX_CDR_BIR_LTD1[4:0] / 5'b00010-->5'b00011 / Increase BW - 0x1128095c bit [28:24]
	 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG6, RG_SSUSB_TX_EIDLE_CM_OFST,
			  RG_SSUSB_TX_EIDLE_CM, 0xE);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYD_CDR1, RG_SSUSB_CDR_BIR_LTD0_OFST,
			  RG_SSUSB_CDR_BIR_LTD0, 0xC);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYD_CDR1, RG_SSUSB_CDR_BIR_LTD1_OFST,
			  RG_SSUSB_CDR_BIR_LTD1, 0x3);

	/*
	 * 1.DA_SSUSB_XTAL_EXT_EN[1:0]  2'b01-->2'b10 - 0x11290c00 bit[11:10]
	 * 2.DA_SSUSB_XTAL_RX_PWD[9:9]  -->1'b1 - 0x11280018 bit[9]
	 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U3PHYA_DA_REG0, RG_SSUSB_XTAL_EXT_EN_U3_OFST,
			  RG_SSUSB_XTAL_EXT_EN_U3, 2);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_SPLLC_XTALCTL3, RG_SSUSB_XTAL_RX_PWD_OFST,
			  RG_SSUSB_XTAL_RX_PWD, 1);
#else
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_SPLLC_XTALCTL3, RG_SSUSB_XTAL_RX_PWD_OFST,
			RG_SSUSB_XTAL_RX_PWD, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_SPLLC_XTALCTL3, RG_SSUSB_FRC_XTAL_RX_PWD_OFST,
			RG_SSUSB_FRC_XTAL_RX_PWD, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U3PHYA_DA_REG36, RG_SSUSB_DA_SSUSB_PLL_BAND_OFST,
			RG_SSUSB_DA_SSUSB_PLL_BAND, 0x2D);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYD_RXDET2, RG_SSUSB_RXDET_STB2_SET_P3_OFST,
			RG_SSUSB_RXDET_STB2_SET_P3, 0x10);
#endif
	/* Wait 800 usec */
	udelay(800);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_VBUSVALID_OFST, RG_VBUSVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_AVALID_OFST, RG_AVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_SESSEND_OFST, RG_SESSEND, 0);

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (in_uart_mode) {
		os_printk(K_INFO,
			  "%s- Switch to UART mode when UART cable in inserted before boot.\n",
			  __func__);
		usb_phy_switch_to_uart();
	}
#endif

    /* efuse setting */
	evalue = (get_devinfo_with_index(44) & 0x1F);
	if (evalue) {
		os_printk(K_INFO, "apply efuse setting, RG_USB20_INTR_CAL=0x%x\n", evalue);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR1, RG_USB20_INTR_CAL_OFST,
		RG_USB20_INTR_CAL, evalue);
	}

	evalue = (get_devinfo_with_index(28) & (0x1f<<8)) >> 8;
	if (evalue) {
		os_printk(K_INFO, "apply efuse setting, RG_SSUSB_RX_IMPSEL=0x%x\n", evalue);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYD_IMPCAL1, RG_SSUSB_RX_IMPSEL_OFST,
		RG_SSUSB_RX_IMPSEL, evalue);
	}

	evalue = (get_devinfo_with_index(28) & (0x3f<<16)) >> 16;
	if (evalue) {
		os_printk(K_INFO, "apply efuse setting, SSUSB_IEXT_INTR_CTRL=0x%x\n", evalue);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) (U3D_USB30_PHYA_REG0), RG_SSUSB_IEXT_INTR_CTRL_OFST,
		RG_SSUSB_IEXT_INTR_CTRL, evalue);
	}

	evalue = (get_devinfo_with_index(28) & (0xf<<1));
	if (evalue) {
		os_printk(K_INFO, "apply efuse setting, RG_SSUSB_TX_IMPSEL=0x%x\n", evalue);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYD_IMPCAL0, RG_SSUSB_TX_IMPSEL_OFST,
		RG_SSUSB_TX_IMPSEL, evalue);
	}

	/* Set host disconnect threshold*/
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_DISCTH_OFST, RG_USB20_DISCTH, 0xF);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_PHY_REV_6_OFST,
		RG_USB20_PHY_REV_6, usb20_phy_rev6);

	/* USB PLL Force settings */
	usb20_pll_settings(false, false);

	os_printk(K_DEBUG, "%s-\n", __func__);
}

/*
 * This function is to solve the condition described below.
 * The system boot has 3 situations.
 * 1. Booting without cable, so connection work called by musb_gadget_start()
 *    would turn off pwr/clk by musb_stop(). [REF CNT = 0]
 * 2. Booting with normal cable, the pwr/clk has already turned on at initial stage.
 *    and also set the flag (musb->is_clk_on=1).
 *    So musb_start() would not turn on again. [REF CNT = 1]
 * 3. Booting with OTG cable, the pwr/clk would be turned on by host one more time.[REF CNT=2]
 *    So device should turn off pwr/clk which are turned on during the initial stage.
 *    However, does _NOT_ touch the PHY registers. So we need this fake function to keep the REF CNT correct.
 *    NOT FOR TURN OFF PWR/CLK.
 */
void usb_fake_powerdown(unsigned int clk_on)
{
	PHY_INT32 ret;

	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	if (clk_on) {
		/*---CLOCK-----*/
		/* f_fusb30_ck:125MHz */
		usb_enable_clock(false);

		/*---POWER-----*/
		/* Set RG_VUSB10_ON as 1 after VDD10 Ready */
		/*hwPowerDown(MT6331_POWER_LDO_VUSB10, "VDD10_USB_P0"); */
		ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x00);
	}

	os_printk(K_INFO, "%s-\n", __func__);
}

#ifdef CONFIG_USBIF_COMPLIANCE
static bool charger_det_en = true;

void Charger_Detect_En(bool enable)
{
	charger_det_en = enable;
}
#endif


/* BC1.2 */
void Charger_Detect_Init(void)
{
	os_printk(K_DEBUG, "%s+\n", __func__);

#ifdef CONFIG_USBIF_COMPLIANCE
	if (charger_det_en == true) {
#endif
		/* turn on USB reference clock. */
		usb_enable_clock(true);

		/* wait 50 usec. */
		udelay(50);

		/* RG_USB20_BC11_SW_EN = 1'b1 */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
				  RG_USB20_BC11_SW_EN, 1);

		udelay(1);

		/* 4 14. turn off internal 48Mhz PLL. */
		usb_enable_clock(false);

#ifdef CONFIG_USBIF_COMPLIANCE
	} else {
		os_printk(K_INFO, "%s do not init detection as charger_det_en is false\n",
			  __func__);
	}
#endif

	os_printk(K_DEBUG, "%s-\n", __func__);
}

void Charger_Detect_Release(void)
{
	os_printk(K_DEBUG, "%s+\n", __func__);

#ifdef CONFIG_USBIF_COMPLIANCE
	if (charger_det_en == true) {
#endif
		/* turn on USB reference clock. */
		usb_enable_clock(true);

		/* wait 50 usec. */
		udelay(50);

		/* RG_USB20_BC11_SW_EN = 1'b0 */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
				  RG_USB20_BC11_SW_EN, 0);

		udelay(1);

		/* 4 14. turn off internal 48Mhz PLL. */
		usb_enable_clock(false);

#ifdef CONFIG_USBIF_COMPLIANCE
	} else {
		os_printk(K_DEBUG, "%s do not release detection as charger_det_en is false\n",
			  __func__);
	}
#endif

	os_printk(K_INFO, "%s-\n", __func__);
}



#ifdef CONFIG_OF
static int mt_usb_dts_probe(struct platform_device *pdev)
{
	int retval = 0;
#ifndef CONFIG_MTK_CLKMGR
	musb_clk = devm_clk_get(&pdev->dev, "sssub_clk");
	if (IS_ERR(musb_clk)) {
		os_printk(K_ERR, "SSUSB cannot get musb clock\n");
		return PTR_ERR(musb_clk);
	}

	os_printk(K_DEBUG, "SSUSB get musb clock ok, prepare it\n");
	retval = clk_prepare(musb_clk);
	if (retval == 0)
		os_printk(K_DEBUG, "musb clock prepare done\n");
	else
		os_printk(K_ERR, "musb clock prepare fail\n");
#endif
	verion = mt_get_chip_sw_ver();
	if (verion >= CHIP_SW_VER_02)
		usb20_phy_rev6 = 1;
	else
		usb20_phy_rev6 = 0;

	return retval;
}

static int mt_usb_dts_remove(struct platform_device *pdev)
{
#ifndef CONFIG_MTK_CLKMGR
	clk_unprepare(musb_clk);
#endif
	return 0;
}


static const struct of_device_id apusb_of_ids[] = {
	{.compatible = "mediatek,usb3_phy",},
	{},
};

MODULE_DEVICE_TABLE(of, apusb_of_ids);

static struct platform_driver mt_usb_dts_driver = {
	.remove = mt_usb_dts_remove,
	.probe = mt_usb_dts_probe,
	.driver = {
		   .name = "mt_dts_mu3phy",
		   .of_match_table = apusb_of_ids,
		   },
};
MODULE_DESCRIPTION("mtu3phy MUSB PHY Layer");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
module_platform_driver(mt_usb_dts_driver);

#endif

#endif

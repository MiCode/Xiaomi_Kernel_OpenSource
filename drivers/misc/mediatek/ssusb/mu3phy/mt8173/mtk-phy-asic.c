/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/io.h>

#include <mu3phy/mtk-phy.h>
#include <mu3phy/mtk-phy-asic.h>
#include <mu3d/ssusb_hw_regs.h>

#ifdef CONFIG_SSUSB_PHY0_U2_CURRENT_DETECT
#include "../../../auxadc/mt_auxadc.h"
#endif

struct u3p_project_regs {
	void __iomem *sif_base;
	/* void __iomem *sif2_base; */
	int phy_num;
	int is_u3_current;
};
#define u3p_dbg printk

static struct u3p_project_regs g_u3p_regs;
#define RG_SSUSB_VUSB10_ON (1<<5)
#define RG_SSUSB_VUSB10_ON_OFST (5)

#define RX_SENS 1
#ifdef CONFIG_SSUSB_PHY0_U2_CURRENT_DETECT
/* return 0: internal current(proto pcb), else u3 current */
static int magna_pcb_version_detect(void)
{
	int ret = 0, data[4], rawvalue;

	ret = IMM_GetOneChannelValue(0, data, &rawvalue);
	if (ret) {
		u3p_dbg("%s failed %d\n", __func__, ret);
		return -1;
	}
	u3p_dbg("%s raw value:%d\n", __func__, rawvalue);
	return !!(rawvalue < 410 || rawvalue > 683);
}
#endif
/*
* power and clock should be ready before
* refers to "MT8137_USB_PORT1_PWR Sequeuece 20140220.els"
*/
static int phy_port1_init(struct u3phy_info *info)
{
	void __iomem *sif_base = info->phy_regs.sif_base + SSUSB_PORT1_BASE;
	/* void __iomem *sif2_base = info->phy_regs.sif2_base; */

#if RX_SENS
	u3phy_writel(sif_base, U3D_U2PHYDCR0, E60802_RG_SIFSLV_USB20_PLL_FORCE_ON_OFST,
					 E60802_RG_SIFSLV_USB20_PLL_FORCE_ON, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_SUSPENDM_OFST, E60802_RG_SUSPENDM, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 1);
#endif

	/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
	u3phy_writel(sif_base, U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST, RG_SSUSB_VUSB10_ON, 1);

	/*switch to USB function. (system register, force ip into usb mode) */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);

	/*DP/DM BC1.1 path Disable */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST,
		     E60802_RG_USB20_BC11_SW_EN, 0);
	/*dp_100k disable */
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_RG_USB20_DP_100K_MODE_OFST,
		     E60802_RG_USB20_DP_100K_MODE, 1);
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_USB20_DP_100K_EN_OFST, E60802_USB20_DP_100K_EN,
		     0);
	/*dm_100k disable */
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_RG_USB20_DM_100K_EN_OFST,
		     E60802_RG_USB20_DM_100K_EN, 0);

	/*Release force suspendm. ?3 (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll) */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 0);
	/*Wait 800 usec */
	udelay(800);
	u3phy_writel(sif_base, U3D_U2PHYDCR1, E60802_RG_USB20_SW_PLLMODE_OFST, E60802_RG_USB20_SW_PLLMODE, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_FORCE_VBUSVALID_OFST, E60802_FORCE_VBUSVALID,
		     1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_FORCE_AVALID_OFST, E60802_FORCE_AVALID, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_FORCE_SESSEND_OFST, E60802_FORCE_SESSEND, 1);


	return PHY_TRUE;
}

static int u2_port1_slew_rate(struct u3phy_info *info)
{
	void __iomem *sif_base = info->phy_regs.sif_base + SSUSB_PORT1_BASE;
	/* void __iomem *sif2_base = info->phy_regs.sif2_base; */


	u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCTRL_OFST,
		     E60802_RG_USB20_HSTX_SRCTRL, 4);
	return 0;
}

static void usb_port1_phy_save(struct u3phy_info *info, unsigned int clk_on)
{
	void __iomem *sif_base = info->phy_regs.sif_base + SSUSB_PORT1_BASE;
	/* void __iomem *sif2_base = info->phy_regs.sif2_base; */


	/*switch to USB function. (system register, force ip into usb mode) */
	/* force_uart_en      1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_UART_EN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	/* RG_UART_EN         1'b0 */
	/* U3D_U2PHYDTM1 E60802_RG_UART_EN */
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);

	/*force_suspendm=1 */
	/* force_suspendm        1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_SUSPENDM */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 1);

	/*Wait USBPLL stable. */
	/* Wait 2 ms. */
	udelay(2000);

	/* RG_DPPULLDOWN 1'b1 */
	/* U3D_U2PHYDTM0 E60802_RG_DPPULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DPPULLDOWN_OFST, E60802_RG_DPPULLDOWN, 1);

	/* RG_DMPULLDOWN 1'b1 */
	/* U3D_U2PHYDTM0 E60802_RG_DMPULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DMPULLDOWN_OFST, E60802_RG_DMPULLDOWN, 1);

	/* RG_XCVRSEL[1:0] 2'b01 */
	/* U3D_U2PHYDTM0 E60802_RG_XCVRSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_XCVRSEL_OFST, E60802_RG_XCVRSEL, 0x1);

	/* RG_TERMSEL    1'b1 */
	/* U3D_U2PHYDTM0 E60802_RG_TERMSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_TERMSEL_OFST, E60802_RG_TERMSEL, 1);

	/* RG_DATAIN[3:0]        4'b0000 */
	/* U3D_U2PHYDTM0 E60802_RG_DATAIN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DATAIN_OFST, E60802_RG_DATAIN, 0);

	/* force_dp_pulldown     1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DP_PULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DP_PULLDOWN_OFST,
		     E60802_FORCE_DP_PULLDOWN, 1);

	/* force_dm_pulldown     1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DM_PULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DM_PULLDOWN_OFST,
		     E60802_FORCE_DM_PULLDOWN, 1);

	/* force_xcversel        1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_XCVRSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_XCVRSEL_OFST, E60802_FORCE_XCVRSEL, 1);

	/* force_termsel 1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_TERMSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_TERMSEL_OFST, E60802_FORCE_TERMSEL, 1);

	/* force_datain  1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DATAIN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DATAIN_OFST, E60802_FORCE_DATAIN, 1);

	/*DP/DM BC1.1 path Disable */
	/* RG_USB20_BC11_SW_EN 1'b0 */
	/* U3D_USBPHYACR6 E60802_RG_USB20_BC11_SW_EN */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST,
		     E60802_RG_USB20_BC11_SW_EN, 0);

	/*OTG Disable */
	/* RG_USB20_OTG_VBUSCMP_EN 1?|b0 */
	/* U3D_USBPHYACR6 E60802_RG_USB20_OTG_VBUSCMP_EN */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_OTG_VBUSCMP_EN_OFST,
		     E60802_RG_USB20_OTG_VBUSCMP_EN, 0);

	/* wait 800us */
	udelay(800);

	/*let suspendm=0, set utmi into analog power down */
	/* RG_SUSPENDM 1'b0 */
	/* U3D_U2PHYDTM0 E60802_RG_SUSPENDM */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_SUSPENDM_OFST, E60802_RG_SUSPENDM, 0);

	/* wait 1us */
	udelay(1);

	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 0);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 0);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 1);
#if RX_SENS
	u3phy_writel(sif_base, U3D_U2PHYDCR0, E60802_RG_SIFSLV_USB20_PLL_FORCE_ON_OFST,
					 E60802_RG_SIFSLV_USB20_PLL_FORCE_ON, 0);
#endif
}


static void usb_port1_phy_restore(struct u3phy_info *info, unsigned int clk_on)
{
	void __iomem *sif_base = info->phy_regs.sif_base + SSUSB_PORT1_BASE;
	/* void __iomem *sif2_base = info->phy_regs.sif2_base; */


	/*switch to USB function. (system register, force ip into usb mode) */
	/* force_uart_en      1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_UART_EN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	/* RG_UART_EN         1'b0 */
	/* U3D_U2PHYDTM1 E60802_RG_UART_EN */
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);

	/*Release force suspendm. (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll) */
	/*force_suspendm        1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_SUSPENDM */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 0);

	/* RG_DPPULLDOWN 1'b0 */
	/* U3D_U2PHYDTM0 E60802_RG_DPPULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DPPULLDOWN_OFST, E60802_RG_DPPULLDOWN, 0);

	/* RG_DMPULLDOWN 1'b0 */
	/* U3D_U2PHYDTM0 E60802_RG_DMPULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DMPULLDOWN_OFST, E60802_RG_DMPULLDOWN, 0);

	/* RG_XCVRSEL[1:0]       2'b00 */
	/* U3D_U2PHYDTM0 E60802_RG_XCVRSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_XCVRSEL_OFST, E60802_RG_XCVRSEL, 0);

	/* RG_TERMSEL    1'b0 */
	/* U3D_U2PHYDTM0 E60802_RG_TERMSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_TERMSEL_OFST, E60802_RG_TERMSEL, 0);

	/* RG_DATAIN[3:0]        4'b0000 */
	/* U3D_U2PHYDTM0 E60802_RG_DATAIN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DATAIN_OFST, E60802_RG_DATAIN, 0);

	/* force_dp_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DP_PULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DP_PULLDOWN_OFST,
		     E60802_FORCE_DP_PULLDOWN, 0);

	/* force_dm_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DM_PULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DM_PULLDOWN_OFST,
		     E60802_FORCE_DM_PULLDOWN, 0);

	/* force_xcversel        1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_XCVRSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_XCVRSEL_OFST, E60802_FORCE_XCVRSEL, 0);

	/* force_termsel 1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_TERMSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_TERMSEL_OFST, E60802_FORCE_TERMSEL, 0);

	/* force_datain  1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DATAIN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DATAIN_OFST, E60802_FORCE_DATAIN, 0);

	/*DP/DM BC1.1 path Disable */
	/* RG_USB20_BC11_SW_EN   1'b0 */
	/* U3D_USBPHYACR6 E60802_RG_USB20_BC11_SW_EN */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST,
		     E60802_RG_USB20_BC11_SW_EN, 0);

	/*OTG Enable */
	/* RG_USB20_OTG_VBUSCMP_EN       1?|b1 */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_OTG_VBUSCMP_EN_OFST,
		     E60802_RG_USB20_OTG_VBUSCMP_EN, 1);

	u3phy_writel(sif_base, U3D_U3PHYA_DA_REG0, E60802_RG_SSUSB_XTAL_EXT_EN_U3_OFST,
		     E60802_RG_SSUSB_XTAL_EXT_EN_U3, 2);

	u3phy_writel(sif_base, U3D_USB30_PHYA_REG9, E60802_RG_SSUSB_RX_DAC_MUX_OFST,
		     E60802_RG_SSUSB_RX_DAC_MUX, 4);

	/*
	 * 1 RG_SSUSB_TX_EIDLE_CM<3:0>
	 / 1100-->1110 / low-power E-idle common mode(650mV to 600mV) - 0x11290b18 bit [31:28]
	 * 2 RG_SSUSB_CDR_BIR_LTD0[4:0] / 5'b01000-->5'b01100 / Increase BW - 0x1128095c bit [12:8]
	 * 3 RG_XXX_CDR_BIR_LTD1[4:0] / 5'b00010-->5'b00011 / Increase BW - 0x1128095c bit [28:24]
	 */
	u3phy_writel(sif_base, U3D_USB30_PHYA_REG6, E60802_RG_SSUSB_TX_EIDLE_CM_OFST,
		     E60802_RG_SSUSB_TX_EIDLE_CM, 0xE);
	u3phy_writel(sif_base, U3D_PHYD_CDR1, E60802_RG_SSUSB_CDR_BIR_LTD0_OFST,
		     E60802_RG_SSUSB_CDR_BIR_LTD0, 0xC);
	u3phy_writel(sif_base, U3D_PHYD_CDR1, E60802_RG_SSUSB_CDR_BIR_LTD1_OFST,
		     E60802_RG_SSUSB_CDR_BIR_LTD1, 0x3);

	/* Wait 800 usec */
	udelay(800);

	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 0);

	u3phy_writel(sif_base, U3D_USBPHYACR4, E60802_RG_USB20_FS_SR_OFST, E60802_RG_USB20_FS_SR, 2);
	u3phy_writel(sif_base, U3D_USBPHYACR4, E60802_RG_USB20_FS_CR_OFST, E60802_RG_USB20_FS_CR, 2);
#if RX_SENS
	u3phy_writel(sif_base, U3D_U2PHYDCR0, E60802_RG_SIFSLV_USB20_PLL_FORCE_ON_OFST,
		E60802_RG_SIFSLV_USB20_PLL_FORCE_ON, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_SUSPENDM_OFST,
		E60802_RG_SUSPENDM, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST,
		E60802_FORCE_SUSPENDM, 1);
	udelay(100);
#endif
}


/*This "power on/initial" sequence refer to "MT8137_USB_PORT0_PWR Sequence 20140220.xls"*/
PHY_INT32 phy_port0_init(struct u3phy_info *info)
{
	PHY_INT32 u4FmOut1 = 0;
	PHY_INT32 u4FmOut2 = 0;
	void __iomem *sif_base = info->phy_regs.sif_base;
	/* void __iomem *sif2_base = info->phy_regs.sif2_base; */


#if RX_SENS
		u3phy_writel(sif_base, U3D_USBPHYACR2, E60802_RG_SIFSLV_USB20_PLL_FORCE_MODE_OFST,
					 E60802_RG_SIFSLV_USB20_PLL_FORCE_MODE_EN, 1);
		u3phy_writel(sif_base, U3D_U2PHYDCR0, E60802_RG_SIFSLV_USB20_PLL_FORCE_ON_OFST,
						 E60802_RG_SIFSLV_USB20_PLL_FORCE_ON, 0);
#endif

	/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
	u3phy_writel(sif_base, U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST, RG_SSUSB_VUSB10_ON, 1);

	/*power domain iso disable */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_ISO_EN_OFST, E60802_RG_USB20_ISO_EN,
		     0);

	/*switch to USB function. (system register, force ip into usb mode) */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_RG_USB20_GPIO_CTL_OFST,
		     E60802_RG_USB20_GPIO_CTL, 0);
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_USB20_GPIO_MODE_OFST, E60802_USB20_GPIO_MODE,
		     0);
	/*DP/DM BC1.1 path Disable */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST,
		     E60802_RG_USB20_BC11_SW_EN, 0);
	/*dp_100k disable */
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_RG_USB20_DP_100K_MODE_OFST,
		     E60802_RG_USB20_DP_100K_MODE, 1);
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_USB20_DP_100K_EN_OFST, E60802_USB20_DP_100K_EN,
		     0);
	/*dm_100k disable */
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_RG_USB20_DM_100K_EN_OFST,
		     E60802_RG_USB20_DM_100K_EN, 0);
	/*can't Change 100uA current switch to SSUSB for 8173(6595 should do) */
	u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HS_100U_U3_EN_OFST,
		     E60802_RG_USB20_HS_100U_U3_EN, 0);
	/*OTG Enable */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_OTG_VBUSCMP_EN_OFST,
		     E60802_RG_USB20_OTG_VBUSCMP_EN, 1);
	/*Release force suspendm. ?3 (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll) */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 0);
	/*Wait 800 usec */
	udelay(800);
	u3phy_writel(sif_base, U3D_U2PHYDCR1, E60802_RG_USB20_SW_PLLMODE_OFST, E60802_RG_USB20_SW_PLLMODE, 1);

	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_FORCE_VBUSVALID_OFST, E60802_FORCE_VBUSVALID,
		     1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_FORCE_AVALID_OFST, E60802_FORCE_AVALID, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_FORCE_SESSEND_OFST, E60802_FORCE_SESSEND, 1);
	u3phy_writel(sif_base, U3D_USBPHYACR1, E60802_RG_USB20_TERM_VREF_SEL_OFST, E60802_RG_USB20_TERM_VREF_SEL, 0x7);
	u3phy_writel(sif_base, U3D_USBPHYACR1, E60802_RG_USB20_VRT_VREF_SEL_OFST, E60802_RG_USB20_VRT_VREF_SEL, 0x7);
	u4FmOut1 = u3phy_readl(sif_base, U3D_USBPHYACR1, E60802_RG_USB20_TERM_VREF_SEL_OFST, E60802_RG_USB20_TERM_VREF_SEL);
	u4FmOut2 = u3phy_readl(sif_base, U3D_USBPHYACR1, E60802_RG_USB20_VRT_VREF_SEL_OFST, E60802_RG_USB20_VRT_VREF_SEL);

	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_DISCTH_OFST, E60802_RG_USB20_DISCTH, 0xf);
	return PHY_TRUE;
}

static int u2_port0_slew_rate(struct u3phy_info *info)
{
	void __iomem *sif_base = info->phy_regs.sif_base;
	/* void __iomem *sif2_base = info->phy_regs.sif2_base; */

	PHY_INT32 i = 0;
	PHY_INT32 fgRet = 0;
	PHY_INT32 u4FmOut = 0;
	PHY_INT32 u4Tmp = 0;


	/* => RG_USB20_HSTX_SRCAL_EN = 1 */
	/* enable USB ring oscillator */
	u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCAL_EN_OFST,
		     E60802_RG_USB20_HSTX_SRCAL_EN, 1);

	/* wait 1us */
	udelay(1);

	/* => USBPHY base address + 0xf11 = 1 */
	/* Enable free run clock */
	u3phy_writel(sif_base, U3D_U2FREQ_CLK, E60802_RG_FRCK_EN_OFST, E60802_RG_FRCK_EN, 0x1);

	/* => USBPHY base address + 0xf01 = 0x04 */
	/* Setting cyclecnt */
	u3phy_writel(sif_base, U3D_U2FREQ_CYCLE, E60802_RG_CYCLECNT_OFST, E60802_RG_CYCLECNT, 0x04);

	/* => USBPHY base address + 0xf03 = 0x01 */
	/* Enable frequency meter */
	u3phy_writel(sif_base, U3D_U2FREQ_ENFREQ, E60802_RG_FREQDET_EN_OFST, E60802_RG_FREQDET_EN,
		     0x1);


	/* wait for FM detection done, set 10ms timeout */
	for (i = 0; i < 10; i++) {
		/* => USBPHY base address + 0xf0c = FM_OUT */
		/* Read result */
		u4FmOut = u3phy_readl(sif_base, U3D_U2FREQ_VALUE, 0, ~0x0);

		/* check if FM detection done */
		if (u4FmOut != 0) {
			fgRet = 0;
			break;
		}

		fgRet = 1;
		udelay(1000);
	}
	/* => USBPHY base address + 0xf03 = 0x00 */
	/* Disable Frequency meter */
	u3phy_writel(sif_base, U3D_U2FREQ_ENFREQ, E60802_RG_FREQDET_EN_OFST, E60802_RG_FREQDET_EN,
		     0);

	/* => USBPHY base address + 0xf11 = 0x00 */
	/* Disable free run clock */
	u3phy_writel(sif_base, U3D_U2FREQ_CLK, E60802_RG_FRCK_EN_OFST, E60802_RG_FRCK_EN, 0);

	/* RG_USB20_HSTX_SRCTRL[2:0] = (1024/FM_OUT) * reference clock frequency * 0.028 */
	if (u4FmOut == 0) {
		u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCTRL_OFST,
			     E60802_RG_USB20_HSTX_SRCTRL, 0x4);
		fgRet = 1;
	} else {
		/* set reg = (1024/FM_OUT) * REF_CK * U2_SR_COEF_E60802 / 1000 (round to the nearest digits) */
		/* u4Tmp = (((1024 * REF_CK * U2_SR_COEF_E60802) / u4FmOut) + 500) / 1000; */
		u4Tmp = (1024 * REF_CK * U2_SR_COEF_E60802) / (u4FmOut * 1000);
		u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCTRL_OFST,
			     E60802_RG_USB20_HSTX_SRCTRL, u4Tmp);
	}

	/* => RG_USB20_HSTX_SRCAL_EN = 0 */
	/* disable USB ring oscillator */
	u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCAL_EN_OFST,
		     E60802_RG_USB20_HSTX_SRCAL_EN, 0x0);
	u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCTRL_OFST,
		     E60802_RG_USB20_HSTX_SRCTRL, 0x4);
	u4FmOut = u3phy_readl(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCTRL_OFST, E60802_RG_USB20_HSTX_SRCTRL);

	return fgRet;
}

/*This "save current" sequence refers to "MT8137_USB_PORT1_PWR Sequeuece 20140220.xls"*/
static void usb_port0_phy_save(struct u3phy_info *info, unsigned int clk_on)
{
	void __iomem *sif_base = info->phy_regs.sif_base;
	/* void __iomem *sif2_base = info->phy_regs.sif2_base; */


	/*switch to USB function. (system register, force ip into usb mode) */
	/* force_uart_en      1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_UART_EN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	/* RG_UART_EN         1'b0 */
	/* U3D_U2PHYDTM1 E60802_RG_UART_EN */
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);
	/* rg_usb20_gpio_ctl  1'b0 */
	/* U3D_U2PHYACR4 E60802_RG_USB20_GPIO_CTL */
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_RG_USB20_GPIO_CTL_OFST,
		     E60802_RG_USB20_GPIO_CTL, 0);
	/* usb20_gpio_mode       1'b0 */
	/* U3D_U2PHYACR4 E60802_USB20_GPIO_MODE */
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_USB20_GPIO_MODE_OFST, E60802_USB20_GPIO_MODE,
		     0);

	/*let suspendm=1, enable usb 480MHz pll */
	/* RG_SUSPENDM 1'b1 */
	/* U3D_U2PHYDTM0 E60802_RG_SUSPENDM */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_SUSPENDM_OFST, E60802_RG_SUSPENDM, 1);

	/*force_suspendm=1 */
	/* force_suspendm        1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_SUSPENDM */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 1);

	/*Wait USBPLL stable. */
	/* Wait 2 ms. */
	udelay(2000);

	/* RG_DPPULLDOWN 1'b1 */
	/* U3D_U2PHYDTM0 E60802_RG_DPPULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DPPULLDOWN_OFST, E60802_RG_DPPULLDOWN, 1);

	/* RG_DMPULLDOWN 1'b1 */
	/* U3D_U2PHYDTM0 E60802_RG_DMPULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DMPULLDOWN_OFST, E60802_RG_DMPULLDOWN, 1);

	/* RG_XCVRSEL[1:0] 2'b01 */
	/* U3D_U2PHYDTM0 E60802_RG_XCVRSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_XCVRSEL_OFST, E60802_RG_XCVRSEL, 0x1);

	/* RG_TERMSEL    1'b1 */
	/* U3D_U2PHYDTM0 E60802_RG_TERMSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_TERMSEL_OFST, E60802_RG_TERMSEL, 1);

	/* RG_DATAIN[3:0]        4'b0000 */
	/* U3D_U2PHYDTM0 E60802_RG_DATAIN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DATAIN_OFST, E60802_RG_DATAIN, 0);

	/* force_dp_pulldown     1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DP_PULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DP_PULLDOWN_OFST,
		     E60802_FORCE_DP_PULLDOWN, 1);

	/* force_dm_pulldown     1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DM_PULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DM_PULLDOWN_OFST,
		     E60802_FORCE_DM_PULLDOWN, 1);

	/* force_xcversel        1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_XCVRSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_XCVRSEL_OFST, E60802_FORCE_XCVRSEL, 1);

	/* force_termsel 1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_TERMSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_TERMSEL_OFST, E60802_FORCE_TERMSEL, 1);

	/* force_datain  1'b1 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DATAIN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DATAIN_OFST, E60802_FORCE_DATAIN, 1);

	/*DP/DM BC1.1 path Disable */
	/* RG_USB20_BC11_SW_EN 1'b0 */
	/* U3D_USBPHYACR6 E60802_RG_USB20_BC11_SW_EN */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST,
		     E60802_RG_USB20_BC11_SW_EN, 0);

	/*OTG Disable */
	/* RG_USB20_OTG_VBUSCMP_EN 1?|b0 */
	/* U3D_USBPHYACR6 E60802_RG_USB20_OTG_VBUSCMP_EN */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_OTG_VBUSCMP_EN_OFST,
		     E60802_RG_USB20_OTG_VBUSCMP_EN, 0);

	/*Change 100uA current switch to USB2.0 */
	/* RG_USB20_HS_100U_U3_EN        1'b0 */
	/* U3D_USBPHYACR5 E60802_RG_USB20_HS_100U_U3_EN */
	u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HS_100U_U3_EN_OFST,
		     E60802_RG_USB20_HS_100U_U3_EN, 0);

	/* wait 800us */
	udelay(800);

	/*let suspendm=0, set utmi into analog power down */
	/* RG_SUSPENDM 1'b0 */
	/* U3D_U2PHYDTM0 E60802_RG_SUSPENDM */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_SUSPENDM_OFST, E60802_RG_SUSPENDM, 0);

	/* wait 1us */
	udelay(1);

	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 0);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 0);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 1);

	/* TODO:
	 * Turn off internal 48Mhz PLL if there is no other hardware module is
	 * using the 48Mhz clock -the control register is in clock document
	 * Turn off SSUSB reference clock (26MHz)
	 */
	if (clk_on) {
		/*---CLOCK-----*/
		/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
		u3phy_writel(sif_base, U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
			     RG_SSUSB_VUSB10_ON, 0);


	}

#if RX_SENS
	u3phy_writel(sif_base, U3D_SSUSB_U2_PHY_PLL, SSUSB_U2_FORCE_PLL_STB_OFST,
		SSUSB_U2_FORCE_PLL_STB, 1);
#endif
}

/*This "recovery" sequence refers to "MT8137_USB_PORT1_PWR Sequeuece 20140220.xls"*/
static void usb_port0_phy_restore(struct u3phy_info *info, unsigned int clk_on)
{
	void __iomem *sif_base = info->phy_regs.sif_base;
	/* void __iomem *sif2_base = info->phy_regs.sif2_base; */


	if (!clk_on) {

		/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
		u3phy_writel(sif_base, U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
			     RG_SSUSB_VUSB10_ON, 1);
	}

	/*[MT6593 only]power domain iso disable */
	/* RG_USB20_ISO_EN       1'b0 */
	/* U3D_USBPHYACR6 E60802_RG_USB20_ISO_EN */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_ISO_EN_OFST, E60802_RG_USB20_ISO_EN,
		     0);

	/*switch to USB function. (system register, force ip into usb mode) */
	/* force_uart_en      1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_UART_EN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	/* RG_UART_EN         1'b0 */
	/* U3D_U2PHYDTM1 E60802_RG_UART_EN */
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);
	/* rg_usb20_gpio_ctl  1'b0 */
	/* U3D_U2PHYACR4 E60802_RG_USB20_GPIO_CTL */
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_RG_USB20_GPIO_CTL_OFST,
		     E60802_RG_USB20_GPIO_CTL, 0);
	/* usb20_gpio_mode       1'b0 */
	/* U3D_U2PHYACR4 E60802_USB20_GPIO_MODE */
	u3phy_writel(sif_base, U3D_U2PHYACR4, E60802_USB20_GPIO_MODE_OFST, E60802_USB20_GPIO_MODE,
		     0);

	/*Release force suspendm. (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll) */
	/*force_suspendm        1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_SUSPENDM */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 0);

	/* RG_DPPULLDOWN 1'b0 */
	/* U3D_U2PHYDTM0 E60802_RG_DPPULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DPPULLDOWN_OFST, E60802_RG_DPPULLDOWN, 0);

	/* RG_DMPULLDOWN 1'b0 */
	/* U3D_U2PHYDTM0 E60802_RG_DMPULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DMPULLDOWN_OFST, E60802_RG_DMPULLDOWN, 0);

	/* RG_XCVRSEL[1:0]       2'b00 */
	/* U3D_U2PHYDTM0 E60802_RG_XCVRSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_XCVRSEL_OFST, E60802_RG_XCVRSEL, 0);

	/* RG_TERMSEL    1'b0 */
	/* U3D_U2PHYDTM0 E60802_RG_TERMSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_TERMSEL_OFST, E60802_RG_TERMSEL, 0);

	/* RG_DATAIN[3:0]        4'b0000 */
	/* U3D_U2PHYDTM0 E60802_RG_DATAIN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_RG_DATAIN_OFST, E60802_RG_DATAIN, 0);

	/* force_dp_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DP_PULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DP_PULLDOWN_OFST,
		     E60802_FORCE_DP_PULLDOWN, 0);

	/* force_dm_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DM_PULLDOWN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DM_PULLDOWN_OFST,
		     E60802_FORCE_DM_PULLDOWN, 0);

	/* force_xcversel        1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_XCVRSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_XCVRSEL_OFST, E60802_FORCE_XCVRSEL, 0);

	/* force_termsel 1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_TERMSEL */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_TERMSEL_OFST, E60802_FORCE_TERMSEL, 0);

	/* force_datain  1'b0 */
	/* U3D_U2PHYDTM0 E60802_FORCE_DATAIN */
	u3phy_writel(sif_base, U3D_U2PHYDTM0, E60802_FORCE_DATAIN_OFST, E60802_FORCE_DATAIN, 0);

	/*DP/DM BC1.1 path Disable */
	/* RG_USB20_BC11_SW_EN   1'b0 */
	/* U3D_USBPHYACR6 E60802_RG_USB20_BC11_SW_EN */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST,
		     E60802_RG_USB20_BC11_SW_EN, 0);

	/*OTG Enable */
	/* RG_USB20_OTG_VBUSCMP_EN       1?|b1 */
	u3phy_writel(sif_base, U3D_USBPHYACR6, E60802_RG_USB20_OTG_VBUSCMP_EN_OFST,
		     E60802_RG_USB20_OTG_VBUSCMP_EN, 1);

	/*can't Change 100uA current switch to SSUSB for 8173(6595 should do) */
	/* RG_USB20_HS_100U_U3_EN        1'b1 */
	u3phy_writel(sif_base, U3D_USBPHYACR5, E60802_RG_USB20_HS_100U_U3_EN_OFST,
		     E60802_RG_USB20_HS_100U_U3_EN, g_u3p_regs.is_u3_current);
	u3phy_writel(sif_base, U3D_U3PHYA_DA_REG0, E60802_RG_SSUSB_XTAL_EXT_EN_U3_OFST,
		     E60802_RG_SSUSB_XTAL_EXT_EN_U3, 2);
	u3phy_writel(sif_base, U3D_XTALCTL3, E60802_RG_SSUSB_XTAL_RX_PWD_OFST,
		     E60802_RG_SSUSB_XTAL_RX_PWD, 1);
	u3phy_writel(sif_base, U3D_XTALCTL3, E60802_RG_SSUSB_FRC_XTAL_RX_PWD_OFST,
		     E60802_RG_SSUSB_FRC_XTAL_RX_PWD, 1);
	u3phy_writel(sif_base, U3D_USB30_PHYA_REG9, E60802_RG_SSUSB_RX_DAC_MUX_OFST,
		     E60802_RG_SSUSB_RX_DAC_MUX, 4);
	/*
	 * 1 RG_SSUSB_TX_EIDLE_CM<3:0>
	 / 1100-->1110 / low-power E-idle common mode(650mV to 600mV) - 0x11290b18 bit [31:28]
	 * 2 RG_SSUSB_CDR_BIR_LTD0[4:0] / 5'b01000-->5'b01100 / Increase BW - 0x1128095c bit [12:8]
	 * 3 RG_XXX_CDR_BIR_LTD1[4:0] / 5'b00010-->5'b00011 / Increase BW - 0x1128095c bit [28:24]
	 */
	u3phy_writel(sif_base, U3D_USB30_PHYA_REG6, E60802_RG_SSUSB_TX_EIDLE_CM_OFST,
		     E60802_RG_SSUSB_TX_EIDLE_CM, 0xE);
	u3phy_writel(sif_base, U3D_PHYD_CDR1, E60802_RG_SSUSB_CDR_BIR_LTD0_OFST,
		     E60802_RG_SSUSB_CDR_BIR_LTD0, 0xC);
	u3phy_writel(sif_base, U3D_PHYD_CDR1, E60802_RG_SSUSB_CDR_BIR_LTD1_OFST,
		     E60802_RG_SSUSB_CDR_BIR_LTD1, 0x3);

	/* Wait 800 usec */
	udelay(800);

	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 1);
	u3phy_writel(sif_base, U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 0);

	u3phy_writel(sif_base, U3D_USBPHYACR4, E60802_RG_USB20_FS_SR_OFST, E60802_RG_USB20_FS_SR, 2);
	u3phy_writel(sif_base, U3D_USBPHYACR4, E60802_RG_USB20_FS_CR_OFST, E60802_RG_USB20_FS_CR, 2);
#if RX_SENS
	u3phy_writel(sif_base, U3D_SSUSB_U2_PHY_PLL, SSUSB_U2_FORCE_PLL_STB_OFST,
		SSUSB_U2_FORCE_PLL_STB, 0);
#endif

}

int phy_init_soc(struct u3phy_info *info)
{
	phy_port0_init(info);
	if (info->phy_regs.phy_num > 1)
		phy_port1_init(info);
	return 0;
}

int u2_slew_rate_calibration(struct u3phy_info *info)
{
	u2_port0_slew_rate(info);
	if (info->phy_regs.phy_num > 1)
		u2_port1_slew_rate(info);
	return 0;
}

void usb_phy_savecurrent(struct u3phy_info *info, unsigned int clk_on)
{
	usb_port0_phy_save(info, clk_on);
	if (info->phy_regs.phy_num > 1)
		usb_port1_phy_save(info, clk_on);
}

void usb_phy_recover(struct u3phy_info *info, unsigned int clk_on)
{
	usb_port0_phy_restore(info, clk_on);
	if (info->phy_regs.phy_num > 1)
		usb_port1_phy_restore(info, clk_on);
}


/* BC1.2 */
void Charger_Detect_Init(void)
{

	/* RG_USB20_BC11_SW_EN = 1'b1 */
	u3phy_writel(g_u3p_regs.sif_base, U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST,
		     E60802_RG_USB20_BC11_SW_EN, 1);

	udelay(1);

}

void Charger_Detect_Release(void)
{

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	u3phy_writel(g_u3p_regs.sif_base, U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST,
		     E60802_RG_USB20_BC11_SW_EN, 0);

	udelay(1);

}

static const struct u3phy_operator u3p_project_ops = {
	.init = phy_init_soc,
	.u2_slew_rate_calibration = u2_slew_rate_calibration,
	.usb_phy_savecurrent = usb_phy_savecurrent,
	.usb_phy_recover = usb_phy_recover,
};

int u3p_project_init(struct u3phy_info *info)
{
	struct u3phy_reg_base *phy = &info->phy_regs;
	int is_u3_current = 0;	/* u2 temp. modify it later to use u3 */

#ifdef CONFIG_SSUSB_PHY0_U2_CURRENT_DETECT
	is_u3_current = magna_pcb_version_detect();
#endif
	g_u3p_regs.sif_base = phy->sif_base;
	g_u3p_regs.phy_num = phy->phy_num;
	g_u3p_regs.is_u3_current = ((is_u3_current < 0) ? 1 : is_u3_current);
	u3p_dbg("%s phy0 make use of u3_current:%d\n", __func__, g_u3p_regs.is_u3_current);
	/* g_u3p_regs.sif2_base = info->phy_regs.sif2_base; */
	info->reg_info = (void *)&g_u3p_regs;
	info->u3p_ops = &u3p_project_ops;
	return 0;
}

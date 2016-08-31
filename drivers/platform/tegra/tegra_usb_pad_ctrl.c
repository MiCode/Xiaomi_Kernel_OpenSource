/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */


#include <linux/export.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <mach/tegra_usb_pad_ctrl.h>

#include "../../../arch/arm/mach-tegra/iomap.h"

static DEFINE_SPINLOCK(utmip_pad_lock);
static DEFINE_SPINLOCK(xusb_padctl_lock);
static int utmip_pad_count;
static struct clk *utmi_pad_clk;

void tegra_xhci_release_otg_port(bool release)
{
	void __iomem *padctl_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 reg;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	reg = readl(padctl_base + XUSB_PADCTL_USB2_PAD_MUX_0);

	reg &= ~USB2_OTG_PAD_PORT0_MASK;
	if (release)
		reg |= USB2_OTG_PAD_PORT0_SNPS;
	else
		reg |= USB2_OTG_PAD_PORT0_XUSB;

	writel(reg, padctl_base + XUSB_PADCTL_USB2_PAD_MUX_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_release_otg_port);

void tegra_xhci_ss_wake_on_interrupts(u32 portmap, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 elpg_program0;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	/* clear any event */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	elpg_program0 |= (SS_PORT0_WAKEUP_EVENT | SS_PORT1_WAKEUP_EVENT);
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	/* enable ss wake interrupts */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	if (enable) {
		/* enable interrupts */
		if (portmap & TEGRA_XUSB_SS_P0)
			elpg_program0 |= SS_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_SS_P1)
			elpg_program0 |= SS_PORT1_WAKE_INTERRUPT_ENABLE;
	} else {
		/* disable interrupts */
		if (portmap & TEGRA_XUSB_SS_P0)
			elpg_program0 &= ~SS_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_SS_P1)
			elpg_program0 &= ~SS_PORT1_WAKE_INTERRUPT_ENABLE;
	}
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_ss_wake_on_interrupts);

void tegra_xhci_hs_wake_on_interrupts(u32 portmap, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 elpg_program0;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	elpg_program0 |= (USB2_PORT0_WAKEUP_EVENT |
			USB2_PORT1_WAKEUP_EVENT |
			USB2_PORT2_WAKEUP_EVENT |
			USB2_HSIC_PORT0_WAKEUP_EVENT |
			USB2_HSIC_PORT1_WAKEUP_EVENT);
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	/* Enable the wake interrupts */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	if (enable) {
		/* enable interrupts */
		if (portmap & TEGRA_XUSB_USB2_P0)
			elpg_program0 |= USB2_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P1)
			elpg_program0 |= USB2_PORT1_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P2)
			elpg_program0 |= USB2_PORT2_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_HSIC_P0)
			elpg_program0 |= USB2_HSIC_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_HSIC_P1)
			elpg_program0 |= USB2_HSIC_PORT1_WAKE_INTERRUPT_ENABLE;
	} else {
		if (portmap & TEGRA_XUSB_USB2_P0)
			elpg_program0 &= ~USB2_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P1)
			elpg_program0 &= ~USB2_PORT1_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_USB2_P2)
			elpg_program0 &= ~USB2_PORT2_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_HSIC_P0)
			elpg_program0 &= ~USB2_HSIC_PORT0_WAKE_INTERRUPT_ENABLE;
		if (portmap & TEGRA_XUSB_HSIC_P1)
			elpg_program0 &= ~USB2_HSIC_PORT1_WAKE_INTERRUPT_ENABLE;
	}
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_hs_wake_on_interrupts);

void tegra_xhci_ss_wake_signal(u32 portmap, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 elpg_program0;
	unsigned long flags;

	/* DO NOT COMBINE BELOW 2 WRITES */

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	/* Assert/Deassert clamp_en_early signals to SSP0/1 */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	if (enable) {
		if (portmap & TEGRA_XUSB_SS_P0)
			elpg_program0 |= SSP0_ELPG_CLAMP_EN_EARLY;
		if (portmap & TEGRA_XUSB_SS_P1)
			elpg_program0 |= SSP1_ELPG_CLAMP_EN_EARLY;
	} else {
		if (portmap & TEGRA_XUSB_SS_P0)
			elpg_program0 &= ~SSP0_ELPG_CLAMP_EN_EARLY;
		if (portmap & TEGRA_XUSB_SS_P1)
			elpg_program0 &= ~SSP1_ELPG_CLAMP_EN_EARLY;
	}
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	/*
	 * Check the LP0 figure and leave gap bw writes to
	 * clamp_en_early and clamp_en
	 */
	udelay(100);

	/* Assert/Deassert clam_en signal */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	if (enable) {
		if (portmap & TEGRA_XUSB_SS_P0)
			elpg_program0 |= SSP0_ELPG_CLAMP_EN;
		if (portmap & TEGRA_XUSB_SS_P1)
			elpg_program0 |= SSP1_ELPG_CLAMP_EN;
	} else {
		if (portmap & TEGRA_XUSB_SS_P0)
			elpg_program0 &= ~SSP0_ELPG_CLAMP_EN;
		if (portmap & TEGRA_XUSB_SS_P1)
			elpg_program0 &= ~SSP1_ELPG_CLAMP_EN;
	}

	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);

	/* wait for 250us for the writes to propogate */
	if (enable)
		udelay(250);
}
EXPORT_SYMBOL_GPL(tegra_xhci_ss_wake_signal);

void tegra_xhci_ss_vcore(u32 portmap, bool enable)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	u32 elpg_program0;
	unsigned long flags;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	/* Assert vcore_off signal */
	elpg_program0 = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	if (enable) {
		if (portmap & TEGRA_XUSB_SS_P0)
			elpg_program0 |= SSP0_ELPG_VCORE_DOWN;
		if (portmap & TEGRA_XUSB_SS_P1)
			elpg_program0 |= SSP1_ELPG_VCORE_DOWN;
	} else {
		if (portmap & TEGRA_XUSB_SS_P0)
			elpg_program0 &= ~SSP0_ELPG_VCORE_DOWN;
		if (portmap & TEGRA_XUSB_SS_P1)
			elpg_program0 &= ~SSP1_ELPG_VCORE_DOWN;
	}
	writel(elpg_program0, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_xhci_ss_vcore);

int utmi_phy_iddq_override(bool set)
{
	unsigned long val, flags;
	void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

	spin_lock_irqsave(&utmip_pad_lock, flags);
	val = readl(clk_base + UTMIPLL_HW_PWRDN_CFG0);
	if (set && !utmip_pad_count)
		val |= UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE;
	else if (!set && utmip_pad_count)
		val &= ~UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE;
	else
		goto out1;
	val |= UTMIPLL_HW_PWRDN_CFG0_IDDQ_SWCTL;
	writel(val, clk_base + UTMIPLL_HW_PWRDN_CFG0);

out1:
	spin_unlock_irqrestore(&utmip_pad_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(utmi_phy_iddq_override);

int utmi_phy_pad_enable(void)
{
	unsigned long val, flags;
	void __iomem *pad_base =  IO_ADDRESS(TEGRA_USB_BASE);

	if (!utmi_pad_clk)
		utmi_pad_clk = clk_get_sys("utmip-pad", NULL);

	clk_enable(utmi_pad_clk);

	spin_lock_irqsave(&utmip_pad_lock, flags);
	utmip_pad_count++;

	val = readl(pad_base + UTMIP_BIAS_CFG0);
	val &= ~(UTMIP_OTGPD | UTMIP_BIASPD);
	val |= UTMIP_HSSQUELCH_LEVEL(0x2) | UTMIP_HSDISCON_LEVEL(0x1) |
		UTMIP_HSDISCON_LEVEL_MSB;
	writel(val, pad_base + UTMIP_BIAS_CFG0);

	tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0
			, PD_MASK , 0);
	spin_unlock_irqrestore(&utmip_pad_lock, flags);

	clk_disable(utmi_pad_clk);

	return 0;
}
EXPORT_SYMBOL_GPL(utmi_phy_pad_enable);

int utmi_phy_pad_disable(void)
{
	unsigned long val, flags;
	void __iomem *pad_base =  IO_ADDRESS(TEGRA_USB_BASE);

	if (!utmi_pad_clk)
		utmi_pad_clk = clk_get_sys("utmip-pad", NULL);

	clk_enable(utmi_pad_clk);
	spin_lock_irqsave(&utmip_pad_lock, flags);

	if (!utmip_pad_count) {
		pr_err("%s: utmip pad already powered off\n", __func__);
		goto out;
	}
	if (--utmip_pad_count == 0) {
		val = readl(pad_base + UTMIP_BIAS_CFG0);
		val |= UTMIP_OTGPD | UTMIP_BIASPD;
		val &= ~(UTMIP_HSSQUELCH_LEVEL(~0) | UTMIP_HSDISCON_LEVEL(~0) |
			UTMIP_HSDISCON_LEVEL_MSB);
		writel(val, pad_base + UTMIP_BIAS_CFG0);
		tegra_usb_pad_reg_update(XUSB_PADCTL_USB2_BIAS_PAD_CTL_0
			, PD_MASK , 1);
	}
out:
	spin_unlock_irqrestore(&utmip_pad_lock, flags);
	clk_disable(utmi_pad_clk);

	return 0;
}
EXPORT_SYMBOL_GPL(utmi_phy_pad_disable);

int usb3_phy_pad_enable(u8 lane_owner)
{
	unsigned long val, flags;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	spin_lock_irqsave(&xusb_padctl_lock, flags);

	/* Program SATA pad phy */
	if (lane_owner & BIT(0)) {
		void __iomem *clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);

		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);
		val &= ~XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0_PLL0_REFCLK_NDIV_MASK;
		val |= XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0_PLL0_REFCLK_NDIV;
		writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);

		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0);
		val &= ~(XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_XDIGCLK_SEL_MASK |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_TXCLKREF_SEL |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_TCLKOUT_EN |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL0_CP_CNTL_MASK |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL1_CP_CNTL_MASK);
		val |= XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_XDIGCLK_SEL |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_TXCLKREF_SEL |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL0_CP_CNTL |
			XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0_PLL1_CP_CNTL;

		writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL2_0);

		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL3_0);
		val &= ~XUSB_PADCTL_IOPHY_PLL_S0_CTL3_0_RCAL_BYPASS;
		writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_S0_CTL3_0);

		/* Enable SATA PADPLL clocks */
		val = readl(clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
		val &= ~SATA_PADPLL_RESET_SWCTL;
		val |= SATA_PADPLL_USE_LOCKDET | SATA_SEQ_START_STATE;
		writel(val, clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);

		udelay(1);

		val = readl(clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
		val |= SATA_SEQ_ENABLE;
		writel(val, clk_base + CLK_RST_CONTROLLER_SATA_PLL_CFG0_0);
	}

	/*
	 * program ownership of lanes owned by USB3 based on odmdata[28:30]
	 * odmdata[28] = 0 (SATA lane owner = SATA),
	 * odmdata[28] = 1 (SATA lane owner = USB3_SS port1)
	 * odmdata[29] = 0 (PCIe lane1 owner = PCIe),
	 * odmdata[29] = 1 (PCIe lane1 owner = USB3_SS port1)
	 * odmdata[30] = 0 (PCIe lane0 owner = PCIe),
	 * odmdata[30] = 1 (PCIe lane0 owner = USB3_SS port0)
	 * FIXME: Check any GPIO settings needed ?
	 */
	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	/* USB3_SS port1 can either be mapped to SATA lane or PCIe lane1 */
	if (lane_owner & BIT(0)) {
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_SATA_PAD_LANE0;
		val |= XUSB_PADCTL_USB3_PAD_MUX_SATA_PAD_LANE0_OWNER_USB3_SS;
	} else if (lane_owner & BIT(1)) {
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1;
		val |= XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1_OWNER_USB3_SS;
	}
	/* USB_SS port0 is alwasy mapped to PCIe lane0 */
	if (lane_owner & BIT(2)) {
		val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0;
		val |= XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0_OWNER_USB3_SS;
	}
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);

	/* Bring enabled lane out of IDDQ */
	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	if (lane_owner & BIT(0))
		val |= XUSB_PADCTL_USB3_PAD_MUX_FORCE_SATA_PAD_IDDQ_DISABLE_MASK0;
	else if (lane_owner & BIT(1))
		val |= XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1;
	if (lane_owner & BIT(2))
		val |= XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);

	udelay(1);

	/* clear AUX_MUX_LP0 related bits in ELPG_PROGRAM */
	val = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	udelay(100);

	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	udelay(100);

	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN;
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(usb3_phy_pad_enable);

void tegra_usb_pad_reg_update(u32 reg_offset, u32 mask, u32 val)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&xusb_padctl_lock, flags);

	reg = readl(pad_base + reg_offset);
	reg &= ~mask;
	reg |= val;
	writel(reg, pad_base + reg_offset);

	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_usb_pad_reg_update);

u32 tegra_usb_pad_reg_read(u32 reg_offset)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&xusb_padctl_lock, flags);
	reg = readl(pad_base + reg_offset);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);

	return reg;
}
EXPORT_SYMBOL_GPL(tegra_usb_pad_reg_read);

void tegra_usb_pad_reg_write(u32 reg_offset, u32 val)
{
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	unsigned long flags;
	spin_lock_irqsave(&xusb_padctl_lock, flags);
	writel(val, pad_base + reg_offset);
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
}
EXPORT_SYMBOL_GPL(tegra_usb_pad_reg_write);

static int tegra_xusb_padctl_phy_enable(void)
{
	unsigned long val, timeout;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	/* set up PLL inputs in PLL_CTL1 */
	val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
	val &= ~XUSB_PADCTL_IOPHY_PLL_P0_CTL1_REFCLK_SEL_MASK;
	val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL1_REFCLK_SEL;
	writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);

	/* set TX ref sel to div5 in PLL_CTL2 */
	/* T124 is div5 while T30 is div10,beacuse CAR will divide 2 */
	/* in GEN1 mode in T124,and if div10,it will be 125MHZ */
	val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0);
	val |= (XUSB_PADCTL_IOPHY_PLL_P0_CTL2_REFCLKBUF_EN |
		XUSB_PADCTL_IOPHY_PLL_P0_CTL2_TXCLKREF_EN |
		XUSB_PADCTL_IOPHY_PLL_P0_CTL2_TXCLKREF_SEL);
	writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL2_0);

	/* take PLL out of reset */
	val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
	val |= XUSB_PADCTL_IOPHY_PLL_P0_CTL1_PLL_RST_;
	writel(val, pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);

	/* Wait for the PLL to lock */
	timeout = 300;
	do {
		val = readl(pad_base + XUSB_PADCTL_IOPHY_PLL_P0_CTL1_0);
		udelay(100);
		if (--timeout == 0) {
			pr_err("Tegra PCIe error: timeout waiting for PLL\n");
			return -EBUSY;
		}
	} while (!(val & XUSB_PADCTL_IOPHY_PLL_P0_CTL1_PLL0_LOCKDET));

	return 0;
}

static int tegra_pcie_lane_iddq(bool enable, int lane_owner)
{
	unsigned long val;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);

	/* disable IDDQ for all lanes based on odmdata */
	/* in same way as for lane ownership done below */
	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	switch (lane_owner) {
		case PCIE_LANES_X4_X1:
		if (enable)
			val |=
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
		else
			val &=
			~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK0;
		case PCIE_LANES_X4_X0:
		if (enable)
			val |=
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1;
		else
			val &=
			~XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK1;
		case PCIE_LANES_X2_X1:
		if (enable)
			val |=
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2 |
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3 |
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4;
		else
			val &=
			~(XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK2 |
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK3 |
			XUSB_PADCTL_USB3_PAD_MUX_FORCE_PCIE_PAD_IDDQ_DISABLE_MASK4);
		break;
		default:
			pr_err("Tegra PCIe error: wrong lane config\n");
			return -ENXIO;
	}
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	return 0;
}

int pcie_phy_pad_enable(bool enable, int lane_owner)
{
	unsigned long val, flags;
	void __iomem *pad_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
	int ret = 0;

	spin_lock_irqsave(&xusb_padctl_lock, flags);

	if (enable) {
		ret = tegra_xusb_padctl_phy_enable();
		if (ret)
			goto exit;
	}
	ret = tegra_pcie_lane_iddq(enable, lane_owner);
	if (ret || !enable)
		goto exit;

	/* clear AUX_MUX_LP0 related bits in ELPG_PROGRAM */
	val = readl(pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN;
	udelay(1);
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_CLAMP_EN_EARLY;
	udelay(100);
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);
	val &= ~XUSB_PADCTL_ELPG_PROGRAM_AUX_MUX_LP0_VCORE_DOWN;
	udelay(100);
	writel(val, pad_base + XUSB_PADCTL_ELPG_PROGRAM_0);

	/* program ownership of all lanes based on odmdata as below */
	/* odm = 0x0, all 5 lanes owner = PCIe */
	/* odm = 0x2, 1st lane owner = USB3 else all lanes owner = PCIe */
	/* odm = 0x3, 1st 2 lane owner = USB3 else all lanes owner = PCIe */
	val = readl(pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);
	switch (lane_owner) {
		case PCIE_LANES_X4_X1:
			val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE0;
		case PCIE_LANES_X4_X0:
			val &= ~XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE1;
		case PCIE_LANES_X2_X1:
			val &= ~(XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE2 |
				XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE3 |
				XUSB_PADCTL_USB3_PAD_MUX_PCIE_PAD_LANE4);
			break;
		default:
			pr_err("Tegra PCIe error: wrong lane config\n");
			ret = -ENXIO;
			goto exit;
	}
	writel(val, pad_base + XUSB_PADCTL_USB3_PAD_MUX_0);

exit:
	spin_unlock_irqrestore(&xusb_padctl_lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(pcie_phy_pad_enable);

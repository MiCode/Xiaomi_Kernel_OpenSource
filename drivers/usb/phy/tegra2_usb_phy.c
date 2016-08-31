/*
 * arch/arm/mach-tegra/tegra2_usb_phy.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/resource.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/usb/otg.h>
#include <linux/usb/ulpi.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/clk/tegra.h>

#include <asm/mach-types.h>

#include <mach/iomap.h>
#include <mach/pinmux.h>
#include <mach/pinmux-tegra20.h>
#include <mach/usb_phy.h>

#include "tegra_usb_phy.h"
#include "gpio-names.h"
#include "fuse.h"

#define USB_USBCMD		0x140
#define   USB_USBCMD_RS		(1 << 0)
#define   USB_USBCMD_RESET	(1 << 1)

#define USB_USBSTS		0x144
#define   USB_USBSTS_PCI	(1 << 2)
#define   USB_USBSTS_SRI	(1 << 7)
#define   USB_USBSTS_HCH	(1 << 12)

#define USB_USBINTR		0x148

#define USB_ASYNCLISTADDR	0x158

#define USB_TXFILLTUNING		0x164
#define USB_FIFO_TXFILL_THRES(x)   (((x) & 0x1f) << 16)
#define USB_FIFO_TXFILL_MASK	0x3f0000

#define ULPI_VIEWPORT		0x170
#define   ULPI_WAKEUP		(1 << 31)
#define   ULPI_RUN		(1 << 30)
#define   ULPI_RD_WR		(1 << 29)

#define USB_PORTSC		0x184
#define   USB_PORTSC_PTS(x)	(((x) & 0x3) << 30)
#define   USB_PORTSC_PSPD(x)	(((x) & 0x3) << 26)
#define   USB_PORTSC_PHCD	(1 << 23)
#define   USB_PORTSC_WKOC	(1 << 22)
#define   USB_PORTSC_WKDS	(1 << 21)
#define   USB_PORTSC_WKCN	(1 << 20)
#define   USB_PORTSC_PTC(x)	(((x) & 0xf) << 16)
#define   USB_PORTSC_PP	(1 << 12)
#define   USB_PORTSC_LS(x) (((x) & 0x3) << 10)
#define   USB_PORTSC_SUSP	(1 << 7)
#define   USB_PORTSC_RESUME	(1 << 6)
#define   USB_PORTSC_OCC	(1 << 5)
#define   USB_PORTSC_PEC	(1 << 3)
#define   USB_PORTSC_PE		(1 << 2)
#define   USB_PORTSC_CSC	(1 << 1)
#define   USB_PORTSC_CCS	(1 << 0)
#define   USB_PORTSC_RWC_BITS (USB_PORTSC_CSC | USB_PORTSC_PEC | USB_PORTSC_OCC)
#define   USB_PORTSC_PSPD_MASK	3

#define USB_USBMODE_REG_OFFSET	0x1a8
#define   USB_USBMODE_MASK		(3 << 0)
#define   USB_USBMODE_HOST		(3 << 0)
#define   USB_USBMODE_DEVICE	(2 << 0)

#define USB_SUSP_CTRL		0x400
#define   USB_WAKE_ON_CNNT_EN_DEV	(1 << 3)
#define   USB_WAKE_ON_DISCON_EN_DEV (1 << 4)
#define   USB_SUSP_CLR		(1 << 5)
#define   USB_CLKEN		(1 << 6)
#define   USB_PHY_CLK_VALID (1 << 7)
#define   USB_PHY_CLK_VALID_INT_ENB    (1 << 9)
#define   UTMIP_RESET		(1 << 11)
#define   UHSIC_RESET		(1 << 11)
#define   UTMIP_PHY_ENABLE	(1 << 12)
#define   UHSIC_PHY_ENABLE	(1 << 12)
#define   ULPI_PHY_ENABLE	(1 << 13)
#define   USB_SUSP_SET		(1 << 14)
#define   USB_WAKEUP_DEBOUNCE_COUNT(x)	(((x) & 0x7) << 16)
#define   USB_PHY_CLK_VALID_INT_STS	(1 << 8)

#define USB_PHY_VBUS_WAKEUP_ID	0x408
#define   VDAT_DET_INT_EN	(1 << 16)
#define   VDAT_DET_CHG_DET	(1 << 17)
#define   VDAT_DET_STS		(1 << 18)
#define   USB_ID_STATUS		(1 << 2)

#define USB1_LEGACY_CTRL	0x410
#define   USB1_NO_LEGACY_MODE			(1 << 0)
#define   USB1_VBUS_SENSE_CTL_MASK		(3 << 1)
#define   USB1_VBUS_SENSE_CTL_VBUS_WAKEUP	(0 << 1)
#define   USB1_VBUS_SENSE_CTL_AB_SESS_VLD_OR_VBUS_WAKEUP \
						(1 << 1)
#define   USB1_VBUS_SENSE_CTL_AB_SESS_VLD	(2 << 1)
#define   USB1_VBUS_SENSE_CTL_A_SESS_VLD	(3 << 1)

#define ULPIS2S_CTRL		0x418
#define   ULPIS2S_ENA			(1 << 0)
#define   ULPIS2S_SUPPORT_DISCONNECT	(1 << 2)
#define   ULPIS2S_PLLU_MASTER_BLASTER60 (1 << 3)
#define   ULPIS2S_SPARE(x)		(((x) & 0xF) << 8)
#define   ULPIS2S_FORCE_ULPI_CLK_OUT	(1 << 12)
#define   ULPIS2S_DISCON_DONT_CHECK_SE0 (1 << 13)
#define   ULPIS2S_SUPPORT_HS_KEEP_ALIVE (1 << 14)
#define   ULPIS2S_DISABLE_STP_PU	(1 << 15)
#define   ULPIS2S_SLV0_CLAMP_XMIT	(1 << 16)

#define ULPI_TIMING_CTRL_0	0x424
#define   ULPI_CLOCK_OUT_DELAY(x)	((x) & 0x1F)
#define   ULPI_OUTPUT_PINMUX_BYP	(1 << 10)
#define   ULPI_CLKOUT_PINMUX_BYP	(1 << 11)
#define   ULPI_SHADOW_CLK_LOOPBACK_EN	(1 << 12)
#define   ULPI_SHADOW_CLK_SEL		(1 << 13)
#define   ULPI_CORE_CLK_SEL		(1 << 14)
#define   ULPI_SHADOW_CLK_DELAY(x)	(((x) & 0x1F) << 16)
#define   ULPI_LBK_PAD_EN		(1 << 26)
#define   ULPI_LBK_PAD_E_INPUT_OR	(1 << 27)
#define   ULPI_CLK_OUT_ENA		(1 << 28)
#define   ULPI_CLK_PADOUT_ENA		(1 << 29)

#define ULPI_TIMING_CTRL_1	0x428
#define   ULPI_DATA_TRIMMER_LOAD	(1 << 0)
#define   ULPI_DATA_TRIMMER_SEL(x)	(((x) & 0x7) << 1)
#define   ULPI_STPDIRNXT_TRIMMER_LOAD	(1 << 16)
#define   ULPI_STPDIRNXT_TRIMMER_SEL(x) (((x) & 0x7) << 17)
#define   ULPI_DIR_TRIMMER_LOAD		(1 << 24)
#define   ULPI_DIR_TRIMMER_SEL(x)	(((x) & 0x7) << 25)

#define UTMIP_PLL_CFG1		0x804
#define UHSIC_PLL_CFG1				0x804
#define   UTMIP_XTAL_FREQ_COUNT(x)		(((x) & 0xfff) << 0)
#define   UTMIP_PLLU_ENABLE_DLY_COUNT(x)	(((x) & 0x1f) << 27)
#define   UHSIC_XTAL_FREQ_COUNT(x)		(((x) & 0xfff) << 0)
#define   UHSIC_PLLU_ENABLE_DLY_COUNT(x)	(((x) & 0x1f) << 14)

#define UTMIP_XCVR_UHSIC_HSRX_CFG0		0x808
#define   UTMIP_XCVR_SETUP(x)			(((x) & 0xf) << 0)
#define   UHSIC_ELASTIC_UNDERRUN_LIMIT(x)	(((x) & 0x1f) << 2)
#define   UHSIC_ELASTIC_OVERRUN_LIMIT(x)	(((x) & 0x1f) << 8)
#define   UTMIP_XCVR_LSRSLEW(x)			(((x) & 0x3) << 8)
#define   UTMIP_XCVR_LSFSLEW(x)			(((x) & 0x3) << 10)
#define   UHSIC_IDLE_WAIT(x)			(((x) & 0x1f) << 13)
#define   UTMIP_FORCE_PD_POWERDOWN		(1 << 14)
#define   UTMIP_FORCE_PD2_POWERDOWN		(1 << 16)
#define   UTMIP_FORCE_PDZI_POWERDOWN		(1 << 18)
#define   UTMIP_XCVR_LSBIAS_SEL			(1 << 21)
#define   UTMIP_XCVR_SETUP_MSB(x)		(((x) & 0x7) << 22)
#define   UTMIP_XCVR_HSSLEW_MSB(x)		(((x) & 0x7f) << 25)
#define   UTMIP_XCVR_MAX_OFFSET		2
#define   UTMIP_XCVR_SETUP_MAX_VALUE	0x7f
#define   UTMIP_XCVR_SETUP_MIN_VALUE	0
#define   XCVR_SETUP_MSB_CALIB(x) ((x) >> 4)

#define UTMIP_BIAS_CFG0			0x80c
#define   UTMIP_OTGPD			(1 << 11)
#define   UTMIP_BIASPD			(1 << 10)

#define UHSIC_HSRX_CFG1				0x80c
#define   UHSIC_HS_SYNC_START_DLY(x)		(((x) & 0x1f) << 1)

#define UTMIP_HSRX_CFG0			0x810
#define   UTMIP_ELASTIC_LIMIT(x)	(((x) & 0x1f) << 10)
#define   UTMIP_IDLE_WAIT(x)		(((x) & 0x1f) << 15)

#define UTMIP_HSRX_CFG1			0x814
#define UHSIC_MISC_CFG0			0x814
#define   UTMIP_HS_SYNC_START_DLY(x)	(((x) & 0x1f) << 1)
#define   UHSIC_SUSPEND_EXIT_ON_EDGE		(1 << 7)
#define   UHSIC_DETECT_SHORT_CONNECT		(1 << 8)
#define   UHSIC_FORCE_XCVR_MODE			(1 << 15)

#define UHSIC_MISC_CFG1				0x818
#define   UHSIC_PLLU_STABLE_COUNT(x)		(((x) & 0xfff) << 2)

#define UHSIC_PADS_CFG0				0x81c
#define   UHSIC_TX_RTUNEN			0xf000
#define   UHSIC_TX_RTUNE(x)			(((x) & 0xf) << 12)

#define UTMIP_TX_CFG0				0x820
#define UHSIC_PADS_CFG1				0x820
#define   UHSIC_PD_BG				(1 << 2)
#define   UHSIC_PD_TX				(1 << 3)
#define   UHSIC_PD_TRK				(1 << 4)
#define   UHSIC_PD_RX				(1 << 5)
#define   UHSIC_PD_ZI				(1 << 6)
#define   UHSIC_RX_SEL				(1 << 7)
#define   UHSIC_RPD_DATA			(1 << 9)
#define   UHSIC_RPD_STROBE			(1 << 10)
#define   UHSIC_RPU_DATA			(1 << 11)
#define   UHSIC_RPU_STROBE			(1 << 12)
#define   UTMIP_FS_PREABMLE_J		(1 << 19)
#define   UTMIP_HS_DISCON_DISABLE	(1 << 8)

#define UTMIP_MISC_CFG0		0x824
#define   UTMIP_DPDM_OBSERVE		(1 << 26)
#define   UTMIP_DPDM_OBSERVE_SEL(x) (((x) & 0xf) << 27)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_J	UTMIP_DPDM_OBSERVE_SEL(0xf)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_K	UTMIP_DPDM_OBSERVE_SEL(0xe)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_SE1 UTMIP_DPDM_OBSERVE_SEL(0xd)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_SE0 UTMIP_DPDM_OBSERVE_SEL(0xc)
#define   UTMIP_SUSPEND_EXIT_ON_EDGE	(1 << 22)
#define   FORCE_PULLDN_DM	(1 << 8)
#define   FORCE_PULLDN_DP	(1 << 9)
#define   COMB_TERMS		(1 << 0)
#define   ALWAYS_FREE_RUNNING_TERMS (1 << 1)

#define USB1_PREFETCH_ID               6
#define USB2_PREFETCH_ID               18
#define USB3_PREFETCH_ID               17

#define UTMIP_MISC_CFG1		0x828
#define   UTMIP_PLL_ACTIVE_DLY_COUNT(x) (((x) & 0x1f) << 18)
#define   UTMIP_PLLU_STABLE_COUNT(x)	(((x) & 0xfff) << 6)

#define UHSIC_STAT_CFG0		0x828
#define   UHSIC_CONNECT_DETECT		(1 << 0)

#define UTMIP_DEBOUNCE_CFG0 0x82c
#define   UTMIP_BIAS_DEBOUNCE_A(x)	(((x) & 0xffff) << 0)

#define UTMIP_BAT_CHRG_CFG0	0x830
#define   UTMIP_PD_CHRG			(1 << 0)
#define   UTMIP_ON_SINK_EN		(1 << 2)
#define   UTMIP_OP_SRC_EN		(1 << 3)

#define UTMIP_SPARE_CFG0	0x834
#define   FUSE_SETUP_SEL		(1 << 3)
#define   FUSE_ATERM_SEL		(1 << 4)

#define UTMIP_XCVR_CFG1		0x838
#define   UTMIP_FORCE_PDDISC_POWERDOWN	(1 << 0)
#define   UTMIP_FORCE_PDCHRP_POWERDOWN	(1 << 2)
#define   UTMIP_FORCE_PDDR_POWERDOWN	(1 << 4)
#define   UTMIP_XCVR_TERM_RANGE_ADJ(x)	(((x) & 0xf) << 18)

#define UTMIP_BIAS_CFG1		0x83c
#define   UTMIP_BIAS_PDTRK_COUNT(x) (((x) & 0x1f) << 3)

#define FUSE_USB_CALIB_0		0x1F0
#define FUSE_USB_CALIB_XCVR_SETUP(x)	(((x) & 0x7F) << 0)

#define APB_MISC_GP_OBSCTRL_0   0x818
#define APB_MISC_GP_OBSDATA_0   0x81c

/* ULPI GPIO */
#define ULPI_STP	TEGRA_GPIO_PY3
#define ULPI_DIR	TEGRA_GPIO_PY1
#define ULPI_D0		TEGRA_GPIO_PO1
#define ULPI_D1		TEGRA_GPIO_PO2

/* These values (in milli second) are taken from the battery charging spec */
#define TDP_SRC_ON_MS	 100
#define TDPSRC_CON_MS	 40

#ifdef DEBUG
#define DBG(stuff...)	pr_info("tegra2_usb_phy: " stuff)
#else
#define DBG(stuff...)	do {} while (0)
#endif

/* define HSIC phy params */
#define HSIC_SYNC_START_DELAY		9
#define HSIC_IDLE_WAIT_DELAY		17
#define HSIC_ELASTIC_UNDERRUN_LIMIT	16
#define HSIC_ELASTIC_OVERRUN_LIMIT	16

static DEFINE_SPINLOCK(utmip_pad_lock);
static int utmip_pad_count;
static int utmip_pad_state_on;

static struct tegra_xtal_freq utmip_freq_table[] = {
	{
		.freq = 12000000,
		.enable_delay = 0x02,
		.stable_count = 0x2F,
		.active_delay = 0x04,
		.xtal_freq_count = 0x76,
		.debounce = 0x7530,
		.pdtrk_count = 5,
	},
	{
		.freq = 13000000,
		.enable_delay = 0x02,
		.stable_count = 0x33,
		.active_delay = 0x05,
		.xtal_freq_count = 0x7F,
		.debounce = 0x7EF4,
		.pdtrk_count = 5,
	},
	{
		.freq = 19200000,
		.enable_delay = 0x03,
		.stable_count = 0x4B,
		.active_delay = 0x06,
		.xtal_freq_count = 0xBB,
		.debounce = 0xBB80,
		.pdtrk_count = 7,
	},
	{
		.freq = 26000000,
		.enable_delay = 0x04,
		.stable_count = 0x66,
		.active_delay = 0x09,
		.xtal_freq_count = 0xFE,
		.debounce = 0xFDE8,
		.pdtrk_count = 9,
	},
};

static struct tegra_xtal_freq uhsic_freq_table[] = {
	{
		.freq = 12000000,
		.enable_delay = 0x02,
		.stable_count = 0x2F,
		.active_delay = 0x0,
		.xtal_freq_count = 0x1CA,
	},
	{
		.freq = 13000000,
		.enable_delay = 0x02,
		.stable_count = 0x33,
		.active_delay = 0x0,
		.xtal_freq_count = 0x1F0,
	},
	{
		.freq = 19200000,
		.enable_delay = 0x03,
		.stable_count = 0x4B,
		.active_delay = 0x0,
		.xtal_freq_count = 0x2DD,
	},
	{
		.freq = 26000000,
		.enable_delay = 0x04,
		.stable_count = 0x66,
		.active_delay = 0x0,
		.xtal_freq_count = 0x3E0,
	},
};

static void usb_phy_fence_read(struct tegra_usb_phy *phy)
{
	/* Fence read for coherency of AHB master intiated writes */
	if (phy->inst == 0)
		readb(IO_ADDRESS(IO_PPCS_PHYS + USB1_PREFETCH_ID));
	else if (phy->inst == 1)
		readb(IO_ADDRESS(IO_PPCS_PHYS + USB2_PREFETCH_ID));
	else if (phy->inst == 2)
		readb(IO_ADDRESS(IO_PPCS_PHYS + USB3_PREFETCH_ID));

	return;
}

static int usb_phy_bringup_host_controller(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	DBG("USB_USBSTS[0x%x] USB_PORTSC[0x%x] port_speed[%d] - 0\n",
			readl(base + USB_USBSTS), readl(base + USB_PORTSC),
							phy->port_speed);

	/* enable host mode */
	val = readl(base + USB_USBMODE_REG_OFFSET);
	val |= USB_USBMODE_HOST;
	writel(val, base + USB_USBMODE_REG_OFFSET);

	/* Enable Port Power */
	val = readl(base + USB_PORTSC);
	val |= USB_PORTSC_PP;
	writel(val, base + USB_PORTSC);
	udelay(10);

	/* Check if the phy resume from LP0. When the phy resume from LP0
	 * USB register will be reset.to zero */
	if (!readl(base + USB_ASYNCLISTADDR)) {
		val = readl(base + USB_USBCMD);
		val |= USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);

		/* Program the field PTC based on the saved speed mode */
		val = readl(base + USB_PORTSC);
		val &= ~(USB_PORTSC_PTC(~0));
		if (phy->port_speed == USB_PHY_PORT_SPEED_HIGH)
			val |= USB_PORTSC_PTC(5);
		else if (phy->port_speed == USB_PHY_PORT_SPEED_FULL)
			val |= USB_PORTSC_PTC(6);
		else if (phy->port_speed == USB_PHY_PORT_SPEED_LOW)
			val |= USB_PORTSC_PTC(7);
		writel(val, base + USB_PORTSC);
		udelay(10);

		/* Disable test mode by setting PTC field to NORMAL_OP */
		val = readl(base + USB_PORTSC);
		val &= ~(USB_PORTSC_PTC(~0));
		writel(val, base + USB_PORTSC);
		udelay(10);
	}

	/* Poll until CCS is enabled */
	if (usb_phy_reg_status_wait(base + USB_PORTSC, USB_PORTSC_CCS,
						 USB_PORTSC_CCS, 2000)) {
		pr_err("%s: timeout waiting for USB_PORTSC_CCS\n", __func__);
	}

	/* Poll until PE is enabled */
	if (usb_phy_reg_status_wait(base + USB_PORTSC, USB_PORTSC_PE,
						 USB_PORTSC_PE, 2000)) {
		pr_err("%s: timeout waiting for USB_PORTSC_PE\n", __func__);
	}

	/* Clear the PCI status, to avoid an interrupt taken upon resume */
	val = readl(base + USB_USBSTS);
	val |= USB_USBSTS_PCI;
	writel(val, base + USB_USBSTS);

	/* Put controller in suspend mode by writing 1 to SUSP bit of PORTSC */
	val = readl(base + USB_PORTSC);
	if ((val & USB_PORTSC_PP) && (val & USB_PORTSC_PE)) {
		val |= USB_PORTSC_SUSP;
		writel(val, base + USB_PORTSC);
		/* Need a 4ms delay before the controller goes to suspend */
		mdelay(4);

		/* Wait until port suspend completes */
		if (usb_phy_reg_status_wait(base + USB_PORTSC, USB_PORTSC_SUSP,
						 USB_PORTSC_SUSP, 1000)) {
			pr_err("%s: timeout waiting for PORT_SUSPEND\n",
								__func__);
		}
	}

	DBG("USB_USBSTS[0x%x] USB_PORTSC[0x%x]\n",
			readl(base + USB_USBSTS), readl(base + USB_PORTSC));

	return 0;
}

static void usb_phy_wait_for_sof(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(base + USB_USBSTS);
	writel(val, base + USB_USBSTS);
	udelay(20);
	/* wait for two SOFs */
	if (usb_phy_reg_status_wait(base + USB_USBSTS, USB_USBSTS_SRI,
		USB_USBSTS_SRI, 2500))
		pr_err("%s: timeout waiting for SOF\n", __func__);

	val = readl(base + USB_USBSTS);
	writel(val, base + USB_USBSTS);
	if (usb_phy_reg_status_wait(base + USB_USBSTS, USB_USBSTS_SRI, 0, 2500))
		pr_err("%s: timeout waiting for SOF\n", __func__);

	if (usb_phy_reg_status_wait(base + USB_USBSTS, USB_USBSTS_SRI,
			USB_USBSTS_SRI, 2500))
		pr_err("%s: timeout waiting for SOF\n", __func__);

	udelay(20);
}

static unsigned int utmi_phy_xcvr_setup_value(struct tegra_usb_phy *phy)
{
	struct tegra_utmi_config *cfg = &phy->pdata->u_cfg.utmi;
	signed long val;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (cfg->xcvr_use_fuses) {
		val = FUSE_USB_CALIB_XCVR_SETUP(
				tegra_fuse_readl(FUSE_USB_CALIB_0));
		if (cfg->xcvr_setup_offset <= UTMIP_XCVR_MAX_OFFSET)
			val = val + cfg->xcvr_setup_offset;

		if (val > UTMIP_XCVR_SETUP_MAX_VALUE) {
			val = UTMIP_XCVR_SETUP_MAX_VALUE;
			pr_info("%s: reset XCVR_SETUP to max value\n",
				 __func__);
		} else if (val < UTMIP_XCVR_SETUP_MIN_VALUE) {
			val = UTMIP_XCVR_SETUP_MIN_VALUE;
			pr_info("%s: reset XCVR_SETUP to min value\n",
				 __func__);
		}
	} else {
		val = cfg->xcvr_setup;
	}

	return (unsigned int) val;
}


static int utmi_phy_open(struct tegra_usb_phy *phy)
{
	unsigned long parent_rate;
	int i;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	phy->utmi_pad_clk = clk_get_sys("utmip-pad", NULL);
	if (IS_ERR(phy->utmi_pad_clk)) {
		pr_err("%s: can't get utmip pad clock\n", __func__);
		return PTR_ERR(phy->utmi_pad_clk);
	}

	phy->utmi_xcvr_setup = utmi_phy_xcvr_setup_value(phy);

	parent_rate = clk_get_rate(clk_get_parent(phy->pllu_clk));
	for (i = 0; i < ARRAY_SIZE(utmip_freq_table); i++) {
		if (utmip_freq_table[i].freq == parent_rate) {
			phy->freq = &utmip_freq_table[i];
			break;
		}
	}
	if (!phy->freq) {
		pr_err("invalid pll_u parent rate %ld\n", parent_rate);
		return -EINVAL;
	}

	return 0;
}

static void utmi_phy_close(struct tegra_usb_phy *phy)
{
	DBG("%s inst:[%d]\n", __func__, phy->inst);

	clk_put(phy->utmi_pad_clk);
}

static int utmi_phy_pad_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val, flags;
	void __iomem *pad_base =  IO_ADDRESS(TEGRA_USB_BASE);

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	clk_enable(phy->utmi_pad_clk);

	spin_lock_irqsave(&utmip_pad_lock, flags);

	utmip_pad_count++;
	val = readl(pad_base + UTMIP_BIAS_CFG0);
	val &= ~(UTMIP_OTGPD | UTMIP_BIASPD);
	writel(val, pad_base + UTMIP_BIAS_CFG0);
	utmip_pad_state_on = true;

	spin_unlock_irqrestore(&utmip_pad_lock, flags);

	clk_disable(phy->utmi_pad_clk);

	return 0;
}

static int utmi_phy_pad_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val, flags;
	void __iomem *pad_base =  IO_ADDRESS(TEGRA_USB_BASE);

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	clk_enable(phy->utmi_pad_clk);
	spin_lock_irqsave(&utmip_pad_lock, flags);

	if (!utmip_pad_count) {
		pr_err("%s: utmip pad already powered off\n", __func__);
		goto out;
	}
	if (--utmip_pad_count == 0) {
		val = readl(pad_base + UTMIP_BIAS_CFG0);
		val |= UTMIP_OTGPD | UTMIP_BIASPD;
		writel(val, pad_base + UTMIP_BIAS_CFG0);
		utmip_pad_state_on = false;
	}
out:
	spin_unlock_irqrestore(&utmip_pad_lock, flags);
	clk_disable(phy->utmi_pad_clk);

	return 0;
}

static int utmi_phy_irq(struct tegra_usb_phy *phy)
{
	void __iomem *base = phy->regs;
	unsigned long val = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	usb_phy_fence_read(phy);
	if (phy->hot_plug) {
		val = readl(base + USB_SUSP_CTRL);
		if ((val  & USB_PHY_CLK_VALID_INT_STS)) {
			val &= ~USB_PHY_CLK_VALID_INT_ENB |
					USB_PHY_CLK_VALID_INT_STS;
			writel(val , (base + USB_SUSP_CTRL));

			val = readl(base + USB_USBSTS);
			if (!(val  & USB_USBSTS_PCI))
				return IRQ_NONE;

			val = readl(base + USB_PORTSC);
			if (val & USB_PORTSC_CCS)
				val &= ~USB_PORTSC_WKCN;
			else
				val &= ~USB_PORTSC_WKDS;
			val &= ~USB_PORTSC_RWC_BITS;
			writel(val , (base + USB_PORTSC));

		} else if (!phy->phy_clk_on) {
			return IRQ_NONE;
		}
	}

	return IRQ_HANDLED;
}

static int phy_post_suspend(struct tegra_usb_phy *phy)
{

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	/* Need a 4ms delay for controller to suspend */
	mdelay(4);
	return 0;
}

static int utmi_phy_post_resume(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	val = readl(base + UTMIP_TX_CFG0);
	val &= ~UTMIP_HS_DISCON_DISABLE;
	writel(val, base + UTMIP_TX_CFG0);
	return 0;
}

static int utmi_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(base + UTMIP_TX_CFG0);
	val |= UTMIP_HS_DISCON_DISABLE;
	writel(val, base + UTMIP_TX_CFG0);

	usb_phy_wait_for_sof(phy);

	return 0;
}

static int utmi_phy_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (!phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already off\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		val = readl(base + USB_SUSP_CTRL);
		val &= ~(USB_WAKEUP_DEBOUNCE_COUNT(~0));
		val |= USB_WAKE_ON_CNNT_EN_DEV | USB_WAKEUP_DEBOUNCE_COUNT(5);
		writel(val, base + USB_SUSP_CTRL);

		val = readl(base + UTMIP_BAT_CHRG_CFG0);
		val |= UTMIP_PD_CHRG;
		writel(val, base + UTMIP_BAT_CHRG_CFG0);
	}

	if (!phy->hot_plug) {
		val = readl(base + UTMIP_XCVR_UHSIC_HSRX_CFG0);
		val |= (UTMIP_FORCE_PD_POWERDOWN | UTMIP_FORCE_PD2_POWERDOWN |
			 UTMIP_FORCE_PDZI_POWERDOWN);
		writel(val, base + UTMIP_XCVR_UHSIC_HSRX_CFG0);
	}

	val = readl(base + UTMIP_XCVR_CFG1);
	val |= UTMIP_FORCE_PDDISC_POWERDOWN | UTMIP_FORCE_PDCHRP_POWERDOWN |
		   UTMIP_FORCE_PDDR_POWERDOWN;
	writel(val, base + UTMIP_XCVR_CFG1);

	if (phy->inst != 0) {
		val = readl(base + UTMIP_BIAS_CFG0);
		val |= UTMIP_OTGPD;
		writel(val, base + UTMIP_BIAS_CFG0);
	}

	phy->port_speed = (readl(base + USB_PORTSC) >> 26) &
			USB_PORTSC_PSPD_MASK;

	if (phy->hot_plug) {
		bool enable_hotplug = true;
		/* if it is OTG port then make sure to enable hot-plug feature
		   only if host adaptor is connected, i.e id is low */
		if (phy->pdata->port_otg) {
			val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
			enable_hotplug = (val & USB_ID_STATUS) ? false : true;
		}
		if (enable_hotplug) {
			/* Enable wakeup event of device plug-in/plug-out */
			val = readl(base + USB_PORTSC);
			if (val & USB_PORTSC_CCS)
				val |= USB_PORTSC_WKDS;
			else
				val |= USB_PORTSC_WKCN;
			writel(val, base + USB_PORTSC);

			val = readl(base + USB_SUSP_CTRL);
			val |= USB_PHY_CLK_VALID_INT_ENB;
			writel(val, base + USB_SUSP_CTRL);
		} else {
			/* Disable PHY clock valid interrupts
				while going into suspend*/
			val = readl(base + USB_SUSP_CTRL);
			val &= ~USB_PHY_CLK_VALID_INT_ENB;
			writel(val, base + USB_SUSP_CTRL);
		}
	}

	/* Disable PHY clock */
	if (phy->inst == 2) {
		val = readl(base + USB_PORTSC);
		val |= USB_PORTSC_PHCD;
		writel(val, base + USB_PORTSC);
	} else {
		val = readl(base + USB_SUSP_CTRL);
		val |= USB_SUSP_SET;
		writel(val, base + USB_SUSP_CTRL);
		udelay(10);
		val = readl(base + USB_SUSP_CTRL);
		val &= ~USB_SUSP_SET;
		writel(val, base + USB_SUSP_CTRL);
	}

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL, USB_PHY_CLK_VALID,
								0, 2500))
		pr_warn("%s: timeout waiting for phy to stabilize\n", __func__);

	utmi_phy_pad_power_off(phy);

	phy->phy_clk_on = false;
	phy->hw_accessible = false;

	DBG("%s(%d) inst:[%d]END\n", __func__, __LINE__, phy->inst);

	return 0;
}

static int utmi_phy_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	struct tegra_utmi_config *config = &phy->pdata->u_cfg.utmi;
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already on\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}

	val = readl(base + USB_SUSP_CTRL);
	val |= UTMIP_RESET;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + UTMIP_TX_CFG0);
	val |= UTMIP_FS_PREABMLE_J;
	writel(val, base + UTMIP_TX_CFG0);

	val = readl(base + UTMIP_HSRX_CFG0);
	val &= ~(UTMIP_IDLE_WAIT(~0) | UTMIP_ELASTIC_LIMIT(~0));
	val |= UTMIP_IDLE_WAIT(config->idle_wait_delay);
	val |= UTMIP_ELASTIC_LIMIT(config->elastic_limit);
	writel(val, base + UTMIP_HSRX_CFG0);

	val = readl(base + UTMIP_HSRX_CFG1);
	val &= ~(UTMIP_HS_SYNC_START_DLY(~0));
	val |= UTMIP_HS_SYNC_START_DLY(config->hssync_start_delay);
	writel(val, base + UTMIP_HSRX_CFG1);

	val = readl(base + UTMIP_DEBOUNCE_CFG0);
	val &= ~UTMIP_BIAS_DEBOUNCE_A(~0);
	val |= UTMIP_BIAS_DEBOUNCE_A(phy->freq->debounce);
	writel(val, base + UTMIP_DEBOUNCE_CFG0);

	val = readl(base + UTMIP_MISC_CFG0);
	val &= ~UTMIP_SUSPEND_EXIT_ON_EDGE;
	writel(val, base + UTMIP_MISC_CFG0);

	val = readl(base + UTMIP_MISC_CFG1);
	val &= ~(UTMIP_PLL_ACTIVE_DLY_COUNT(~0) | UTMIP_PLLU_STABLE_COUNT(~0));
	val |= UTMIP_PLL_ACTIVE_DLY_COUNT(phy->freq->active_delay) |
		UTMIP_PLLU_STABLE_COUNT(phy->freq->stable_count);
	writel(val, base + UTMIP_MISC_CFG1);

	val = readl(base + UTMIP_PLL_CFG1);
	val &= ~(UTMIP_XTAL_FREQ_COUNT(~0) | UTMIP_PLLU_ENABLE_DLY_COUNT(~0));
	val |= UTMIP_XTAL_FREQ_COUNT(phy->freq->xtal_freq_count) |
		UTMIP_PLLU_ENABLE_DLY_COUNT(phy->freq->enable_delay);
	writel(val, base + UTMIP_PLL_CFG1);

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		val = readl(base + USB_SUSP_CTRL);
		val &= ~(USB_WAKE_ON_CNNT_EN_DEV | USB_WAKE_ON_DISCON_EN_DEV);
		writel(val, base + USB_SUSP_CTRL);

		val = readl(base + UTMIP_BAT_CHRG_CFG0);
		val &= ~UTMIP_PD_CHRG;
		writel(val, base + UTMIP_BAT_CHRG_CFG0);
	} else {
		val = readl(base + UTMIP_BAT_CHRG_CFG0);
		val |= UTMIP_PD_CHRG;
		writel(val, base + UTMIP_BAT_CHRG_CFG0);
	}

	utmi_phy_pad_power_on(phy);

	val = readl(base + UTMIP_XCVR_UHSIC_HSRX_CFG0);
	val &= ~(UTMIP_XCVR_LSBIAS_SEL | UTMIP_FORCE_PD_POWERDOWN |
		 UTMIP_FORCE_PD2_POWERDOWN | UTMIP_FORCE_PDZI_POWERDOWN |
		 UTMIP_XCVR_SETUP(~0) | UTMIP_XCVR_LSFSLEW(~0) |
		 UTMIP_XCVR_LSRSLEW(~0) | UTMIP_XCVR_HSSLEW_MSB(~0));
	val |= UTMIP_XCVR_SETUP(phy->utmi_xcvr_setup);
	val |= UTMIP_XCVR_SETUP_MSB(XCVR_SETUP_MSB_CALIB(phy->utmi_xcvr_setup));
	val |= UTMIP_XCVR_LSFSLEW(config->xcvr_lsfslew);
	val |= UTMIP_XCVR_LSRSLEW(config->xcvr_lsrslew);

	writel(val, base + UTMIP_XCVR_UHSIC_HSRX_CFG0);

	val = readl(base + UTMIP_XCVR_CFG1);
	val &= ~(UTMIP_FORCE_PDDISC_POWERDOWN | UTMIP_FORCE_PDCHRP_POWERDOWN |
		 UTMIP_FORCE_PDDR_POWERDOWN | UTMIP_XCVR_TERM_RANGE_ADJ(~0));
	val |= UTMIP_XCVR_TERM_RANGE_ADJ(config->term_range_adj);
	writel(val, base + UTMIP_XCVR_CFG1);


	val = readl(base + UTMIP_BIAS_CFG1);
	val &= ~(UTMIP_BIAS_PDTRK_COUNT(~0));
	val |= UTMIP_BIAS_PDTRK_COUNT(phy->freq->pdtrk_count);
	writel(val, base + UTMIP_BIAS_CFG1);

	val = readl(base + UTMIP_SPARE_CFG0);
	val &= ~FUSE_SETUP_SEL;
	writel(val, base + UTMIP_SPARE_CFG0);

	val = readl(base + USB_SUSP_CTRL);
	val |= UTMIP_PHY_ENABLE;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + USB_SUSP_CTRL);
	val &= ~UTMIP_RESET;
	writel(val, base + USB_SUSP_CTRL);

	if (phy->inst == 0) {
		val = readl(base + USB_SUSP_CTRL);
		val &= ~USB_SUSP_SET;
		writel(val, base + USB_SUSP_CTRL);
	} else {
		val = readl(base + USB_PORTSC);
		val &= ~USB_PORTSC_PHCD;
		writel(val, base + USB_PORTSC);
	}

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
		USB_PHY_CLK_VALID, USB_PHY_CLK_VALID, 2500))
		pr_warn("%s: timeout waiting for phy to stabilize\n", __func__);

	if (phy->inst == 2) {
		val = readl(base + USB_PORTSC);
		val &= ~(USB_PORTSC_PTS(~0));
		writel(val, base + USB_PORTSC);
	}

	phy->phy_clk_on = true;
	phy->hw_accessible = true;

	return 0;
}

static void utmi_phy_restore_start(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(base + UTMIP_MISC_CFG0);
	val &= ~(UTMIP_DPDM_OBSERVE_SEL(~0));

	if (phy->port_speed == USB_PHY_PORT_SPEED_LOW)
		val |= UTMIP_DPDM_OBSERVE_SEL_FS_K;
	else
		val |= UTMIP_DPDM_OBSERVE_SEL_FS_J;
	writel(val, base + UTMIP_MISC_CFG0);
	udelay(1);

	val = readl(base + UTMIP_MISC_CFG0);
	val |= UTMIP_DPDM_OBSERVE;
	writel(val, base + UTMIP_MISC_CFG0);
	udelay(10);
}

static void utmi_phy_restore_end(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(base + UTMIP_MISC_CFG0);
	val &= ~UTMIP_DPDM_OBSERVE;
	writel(val, base + UTMIP_MISC_CFG0);
	udelay(10);
}


static int utmi_phy_resume(struct tegra_usb_phy *phy)
{
	int status = 0;
	unsigned long val, flags;
	void __iomem *base = phy->regs;
	void __iomem *pad_base =  IO_ADDRESS(TEGRA_USB_BASE);

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST) {
		if (readl(base + USB_ASYNCLISTADDR) &&
			!phy->pdata->u_data.host.power_off_on_suspend)
			return 0;

		if (phy->port_speed < USB_PHY_PORT_SPEED_UNKNOWN) {
			utmi_phy_restore_start(phy);
			usb_phy_bringup_host_controller(phy);
			utmi_phy_restore_end(phy);
		} else {
			/* device is plugged in when system is in LP0 */
			/* bring up the controller from LP0*/
			val = readl(base + USB_USBCMD);
			val |= USB_USBCMD_RESET;
			writel(val, base + USB_USBCMD);

			if (usb_phy_reg_status_wait(base + USB_USBCMD,
				USB_USBCMD_RESET, 0, 2500) < 0) {
				pr_err("%s: timeout waiting for reset\n",
					__func__);
			}

			val = readl(base + USB_USBMODE_REG_OFFSET);
			val &= ~USB_USBMODE_MASK;
			val |= USB_USBMODE_HOST;
			writel(val, base + USB_USBMODE_REG_OFFSET);

			if (phy->inst == 2) {
				val = readl(base + USB_PORTSC);
				val &= ~USB_PORTSC_PTS(~0);
				writel(val, base + USB_PORTSC);
			}
			writel(USB_USBCMD_RS, base + USB_USBCMD);

			if (usb_phy_reg_status_wait(base + USB_USBCMD,
				USB_USBCMD_RS, USB_USBCMD_RS, 2500) < 0) {
				pr_err("%s: timeout waiting for run bit\n",
					__func__);
			}

			/* Enable Port Power */
			val = readl(base + USB_PORTSC);
			val |= USB_PORTSC_PP;
			writel(val, base + USB_PORTSC);
			udelay(10);

			DBG("USB_USBSTS[0x%x] USB_PORTSC[0x%x]\n",
				readl(base + USB_USBSTS),
				readl(base + USB_PORTSC));
		}
	} else {
		/* Restoring the pad powers */
		clk_enable(phy->utmi_pad_clk);

		spin_lock_irqsave(&utmip_pad_lock, flags);

		val = readl(pad_base + UTMIP_BIAS_CFG0);
		if (utmip_pad_state_on)
			val &= ~(UTMIP_OTGPD | UTMIP_BIASPD);
		else
			val |= (UTMIP_OTGPD | UTMIP_BIASPD);
		writel(val, pad_base + UTMIP_BIAS_CFG0);

		spin_unlock_irqrestore(&utmip_pad_lock, flags);
		clk_disable(phy->utmi_pad_clk);
	}

	return status;
}

static bool utmi_phy_charger_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	bool status;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	/* Enable charger detection logic */
	val = readl(base + UTMIP_BAT_CHRG_CFG0);
	val |= UTMIP_OP_SRC_EN | UTMIP_ON_SINK_EN;
	writel(val, base + UTMIP_BAT_CHRG_CFG0);

	/* Source should be on for 100 ms as per USB charging spec */
	msleep(TDP_SRC_ON_MS);

	val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
	/* If charger is not connected disable the interrupt */
	val &= ~VDAT_DET_INT_EN;
	val |= VDAT_DET_CHG_DET;
	writel(val, base + USB_PHY_VBUS_WAKEUP_ID);

	val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
	if (val & VDAT_DET_STS)
		status = true;
	else
		status = false;

	/* Disable charger detection logic */
	val = readl(base + UTMIP_BAT_CHRG_CFG0);
	val &= ~(UTMIP_OP_SRC_EN | UTMIP_ON_SINK_EN);
	writel(val, base + UTMIP_BAT_CHRG_CFG0);

	/* Delay of 40 ms before we pull the D+ as per battery charger spec */
	msleep(TDPSRC_CON_MS);

	return status;
}


static int uhsic_phy_open(struct tegra_usb_phy *phy)
{
	unsigned long parent_rate;
	int i;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	parent_rate = clk_get_rate(clk_get_parent(phy->pllu_clk));
	for (i = 0; i < ARRAY_SIZE(uhsic_freq_table); i++) {
		if (uhsic_freq_table[i].freq == parent_rate) {
			phy->freq = &uhsic_freq_table[i];
			break;
		}
	}
	if (!phy->freq) {
		pr_err("invalid pll_u parent rate %ld\n", parent_rate);
		return -EINVAL;
	}

	return 0;
}

static int uhsic_phy_irq(struct tegra_usb_phy *phy)
{
	usb_phy_fence_read(phy);
	return IRQ_HANDLED;
}

static int uhsic_phy_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already On\n", __func__,
							__LINE__, phy->inst);
		return 0;
	}

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~(UHSIC_PD_BG | UHSIC_PD_TX | UHSIC_PD_TRK | UHSIC_PD_RX |
			UHSIC_PD_ZI | UHSIC_RPD_DATA | UHSIC_RPD_STROBE);
	val |= UHSIC_RX_SEL;
	writel(val, base + UHSIC_PADS_CFG1);
	udelay(2);

	val = readl(base + USB_SUSP_CTRL);
	val |= UHSIC_RESET;
	writel(val, base + USB_SUSP_CTRL);
	udelay(30);

	val = readl(base + USB_SUSP_CTRL);
	val |= UHSIC_PHY_ENABLE;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + UTMIP_XCVR_UHSIC_HSRX_CFG0);
	val |= UHSIC_IDLE_WAIT(HSIC_IDLE_WAIT_DELAY);
	val |= UHSIC_ELASTIC_UNDERRUN_LIMIT(HSIC_ELASTIC_UNDERRUN_LIMIT);
	val |= UHSIC_ELASTIC_OVERRUN_LIMIT(HSIC_ELASTIC_OVERRUN_LIMIT);
	writel(val, base + UTMIP_XCVR_UHSIC_HSRX_CFG0);

	val = readl(base + UHSIC_HSRX_CFG1);
	val |= UHSIC_HS_SYNC_START_DLY(HSIC_SYNC_START_DELAY);
	writel(val, base + UHSIC_HSRX_CFG1);

	val = readl(base + UHSIC_MISC_CFG0);
	val |= UHSIC_SUSPEND_EXIT_ON_EDGE;
	writel(val, base + UHSIC_MISC_CFG0);

	val = readl(base + UHSIC_MISC_CFG1);
	val |= UHSIC_PLLU_STABLE_COUNT(phy->freq->stable_count);
	writel(val, base + UHSIC_MISC_CFG1);

	val = readl(base + UHSIC_PLL_CFG1);
	val |= UHSIC_PLLU_ENABLE_DLY_COUNT(phy->freq->enable_delay);
	val |= UHSIC_XTAL_FREQ_COUNT(phy->freq->xtal_freq_count);
	writel(val, base + UHSIC_PLL_CFG1);

	val = readl(base + USB_SUSP_CTRL);
	val &= ~(UHSIC_RESET);
	writel(val, base + USB_SUSP_CTRL);
	udelay(2);

	val = readl(base + USB_PORTSC);
	val &= ~(USB_PORTSC_PTS(~0));
	writel(val, base + USB_PORTSC);

	val = readl(base + USB_TXFILLTUNING);
	if ((val & USB_FIFO_TXFILL_MASK) != USB_FIFO_TXFILL_THRES(0x10)) {
		val = USB_FIFO_TXFILL_THRES(0x10);
		writel(val, base + USB_TXFILLTUNING);
	}

	val = readl(base + USB_PORTSC);
	val &= ~(USB_PORTSC_WKOC | USB_PORTSC_WKDS | USB_PORTSC_WKCN);
	writel(val, base + USB_PORTSC);

	val = readl(base + UHSIC_PADS_CFG0);
	val &= ~(UHSIC_TX_RTUNEN);
	/* set Rtune impedance to 40 ohm */
	val |= UHSIC_TX_RTUNE(0);
	writel(val, base + UHSIC_PADS_CFG0);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL, USB_PHY_CLK_VALID,
						USB_PHY_CLK_VALID, 2500)) {
		pr_err("%s: timeout waiting for phy to stabilize\n", __func__);
		return -ETIMEDOUT;
	}

	phy->phy_clk_on = true;
	phy->hw_accessible = true;

	return 0;
}

static int uhsic_phy_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (!phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already off\n", __func__,
							__LINE__, phy->inst);
		return 0;
	}

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~UHSIC_RPU_STROBE;
	val |= UHSIC_RPD_STROBE;
	writel(val, base + UHSIC_PADS_CFG1);

	val = readl(base + USB_SUSP_CTRL);
	val |= UHSIC_RESET;
	writel(val, base + USB_SUSP_CTRL);
	udelay(30);

	val = readl(base + USB_SUSP_CTRL);
	val &= ~UHSIC_PHY_ENABLE;
	writel(val, base + USB_SUSP_CTRL);

	phy->phy_clk_on = false;
	phy->hw_accessible = false;

	return 0;
}

static int uhsic_phy_bus_port_power(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	val = readl(base + UHSIC_MISC_CFG0);
	val |= UHSIC_DETECT_SHORT_CONNECT;
	writel(val, base + UHSIC_MISC_CFG0);
	udelay(1);

	val = readl(base + UHSIC_MISC_CFG0);
	val |= UHSIC_FORCE_XCVR_MODE;
	writel(val, base + UHSIC_MISC_CFG0);

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~UHSIC_RPD_STROBE;
	val |= UHSIC_RPU_STROBE;
	writel(val, base + UHSIC_PADS_CFG1);

	val = readl(base + USB_USBCMD);
	val &= ~USB_USBCMD_RS;
	writel(val, base + USB_USBCMD);

	if (phy->pdata->ops && phy->pdata->ops->port_power)
		phy->pdata->ops->port_power();

	if (usb_phy_reg_status_wait(base + UHSIC_STAT_CFG0,
			UHSIC_CONNECT_DETECT, UHSIC_CONNECT_DETECT, 2000)) {
		pr_err("%s: timeout waiting for UHSIC_CONNECT_DETECT\n",
								__func__);
		return -ETIMEDOUT;
	}

/* FIXME : need to check whether this piece is required or not
	if (usb_phy_reg_status_wait(base + USB_PORTSC, USB_PORTSC_LS(2),
						USB_PORTSC_LS(2), 2000)) {
		pr_err("%s: timeout waiting for dplus state\n", __func__);
		return -ETIMEDOUT;
	}
*/
	return 0;
}


static int uhsic_phy_bus_reset(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	val = readl(base + USB_PORTSC);
	val |= USB_PORTSC_PTC(5);
	writel(val, base + USB_PORTSC);
	udelay(2);

	val = readl(base + USB_PORTSC);
	val &= ~(USB_PORTSC_PTC(~0));
	writel(val, base + USB_PORTSC);
	udelay(2);

	if (usb_phy_reg_status_wait(base + USB_PORTSC, USB_PORTSC_LS(0),
						 0, 2000)) {
		pr_err("%s: timeout waiting for USB_PORTSC_LS\n", __func__);
		return -ETIMEDOUT;
	}

	/* Poll until CCS is enabled */
	if (usb_phy_reg_status_wait(base + USB_PORTSC, USB_PORTSC_CCS,
						 USB_PORTSC_CCS, 2000)) {
		pr_err("%s: timeout waiting for USB_PORTSC_CCS\n", __func__);
		return -ETIMEDOUT;
	}

	if (usb_phy_reg_status_wait(base + USB_PORTSC, USB_PORTSC_PSPD(2),
						 USB_PORTSC_PSPD(2), 2000)) {
		pr_err("%s: timeout waiting for USB_PORTSC_PSPD\n", __func__);
		return -ETIMEDOUT;
	}

	val = readl(base + USB_USBCMD);
	val &= ~USB_USBCMD_RS;
	writel(val, base + USB_USBCMD);

	if (usb_phy_reg_status_wait(base + USB_USBSTS, USB_USBSTS_HCH,
						 USB_USBSTS_HCH, 2000)) {
		pr_err("%s: timeout waiting for USB_USBSTS_HCH\n", __func__);
		return -ETIMEDOUT;
	}

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~UHSIC_RPU_STROBE;
	val |= UHSIC_RPD_STROBE;
	writel(val, base + UHSIC_PADS_CFG1);

	mdelay(50);

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~UHSIC_RPD_STROBE;
	val |= UHSIC_RPU_STROBE;
	writel(val, base + UHSIC_PADS_CFG1);

	val = readl(base + USB_USBCMD);
	val |= USB_USBCMD_RS;
	writel(val, base + USB_USBCMD);

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~UHSIC_RPU_STROBE;
	writel(val, base + UHSIC_PADS_CFG1);

	if (usb_phy_reg_status_wait(base + USB_USBCMD, USB_USBCMD_RS,
						 USB_USBCMD_RS, 2000)) {
		pr_err("%s: timeout waiting for USB_USBCMD_RS\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}


static int uhsic_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	usb_phy_wait_for_sof(phy);

	return 0;
}

static int uhsic_phy_resume(struct tegra_usb_phy *phy)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	uhsic_phy_bus_port_power(phy);

	return 0;
}


static int uhsic_phy_post_resume(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(base + USB_TXFILLTUNING);
	if ((val & USB_FIFO_TXFILL_MASK) != USB_FIFO_TXFILL_THRES(0x10)) {
		val = USB_FIFO_TXFILL_THRES(0x10);
		writel(val, base + USB_TXFILLTUNING);
	}

	return 0;
}

static void ulpi_set_trimmer(struct tegra_usb_phy *phy)
{
	struct tegra_ulpi_config *config = &phy->pdata->u_cfg.ulpi;
	void __iomem *base = phy->regs;
	unsigned long val;

	val = ULPI_DATA_TRIMMER_SEL(config->data_trimmer);
	val |= ULPI_STPDIRNXT_TRIMMER_SEL(config->stpdirnxt_trimmer);
	val |= ULPI_DIR_TRIMMER_SEL(config->dir_trimmer);
	writel(val, base + ULPI_TIMING_CTRL_1);
	udelay(10);

	val |= ULPI_DATA_TRIMMER_LOAD;
	val |= ULPI_STPDIRNXT_TRIMMER_LOAD;
	val |= ULPI_DIR_TRIMMER_LOAD;
	writel(val, base + ULPI_TIMING_CTRL_1);
}


static int ulpi_link_phy_open(struct tegra_usb_phy *phy)
{
	struct tegra_ulpi_config *config = &phy->pdata->u_cfg.ulpi;
	int err = 0;

	phy->ulpi_clk = NULL;

	if (config->clk) {
		phy->ulpi_clk = clk_get_sys(NULL, config->clk);
		if (IS_ERR(phy->ulpi_clk)) {
			pr_err("%s: can't get ulpi clock\n", __func__);
			err = -ENXIO;
		}
	}

	phy->ulpi_vp = otg_ulpi_create(&ulpi_viewport_access_ops, 0);
	phy->ulpi_vp->io_priv = phy->regs + ULPI_VIEWPORT;
	phy->linkphy_init = true;
	return err;
}

static void ulpi_link_phy_close(struct tegra_usb_phy *phy)
{
	DBG("%s inst:[%d]\n", __func__, phy->inst);
	if (phy->ulpi_clk)
		clk_put(phy->ulpi_clk);
}

static int ulpi_link_phy_irq(struct tegra_usb_phy *phy)
{
	usb_phy_fence_read(phy);
	return IRQ_HANDLED;
}

static int ulpi_link_phy_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	int ret;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (!phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already off\n", __func__,
							__LINE__, phy->inst);
		return 0;
	}

	/* Disable VbusValid, SessEnd comparators */
	ret = usb_phy_io_write(phy->ulpi_vp, 0x00, 0x0D);
	if (ret)
		pr_err("%s: ulpi write 0x0D failed\n", __func__);

	ret = usb_phy_io_write(phy->ulpi_vp, 0x00, 0x10);
	if (ret)
		pr_err("%s: ulpi write 0x10 failed\n", __func__);

	/* Disable IdFloat comparator */
	ret = usb_phy_io_write(phy->ulpi_vp, 0x00, 0x19);
	if (ret)
		pr_err("%s: ulpi write 0x19 failed\n", __func__);

	ret = usb_phy_io_write(phy->ulpi_vp, 0x00, 0x1D);
	if (ret)
		pr_err("%s: ulpi write 0x1D failed\n", __func__);

	phy->port_speed = (readl(base + USB_PORTSC) >> 26) &
			USB_PORTSC_PSPD_MASK;

	/* Clear WKCN/WKDS/WKOC wake-on events that can cause the USB
	 * Controller to immediately bring the ULPI PHY out of low power
	 */
	val = readl(base + USB_PORTSC);
	val &= ~(USB_PORTSC_WKOC | USB_PORTSC_WKDS | USB_PORTSC_WKCN);
	writel(val, base + USB_PORTSC);

	/* Put the PHY in the low power mode */
	val = readl(base + USB_PORTSC);
	val |= USB_PORTSC_PHCD;
	writel(val, base + USB_PORTSC);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL, USB_PHY_CLK_VALID,
							 0, 2500)) {
		pr_err("%s: timeout waiting for phy to stop\n", __func__);
	}

	if (phy->ulpi_clk)
		clk_disable(phy->ulpi_clk);

	phy->phy_clk_on = false;
	phy->hw_accessible = false;

	return 0;
}

static int ulpi_link_phy_power_on(struct tegra_usb_phy *phy)
{
	int ret;
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already On\n", __func__,
							__LINE__, phy->inst);
		return 0;
	}

	if (phy->ulpi_clk) {
		clk_enable(phy->ulpi_clk);
		mdelay(1);
	}

	val = readl(base + USB_SUSP_CTRL);

	/* Case for lp0 */
	if (!(val & UHSIC_RESET)) {
		val |= UHSIC_RESET;
		writel(val, base + USB_SUSP_CTRL);

		val = 0;
		writel(val, base + ULPI_TIMING_CTRL_1);

		ulpi_set_trimmer(phy);

		val = readl(base + ULPI_TIMING_CTRL_0);
		val |= ULPI_OUTPUT_PINMUX_BYP | ULPI_CLKOUT_PINMUX_BYP;
		writel(val, base + ULPI_TIMING_CTRL_0);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_UAA, TEGRA_TRI_NORMAL);
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_UAB, TEGRA_TRI_NORMAL);
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_UDA, TEGRA_TRI_NORMAL);
#endif
		val = readl(base + USB_SUSP_CTRL);
		val |= ULPI_PHY_ENABLE;
		writel(val, base + USB_SUSP_CTRL);

		if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
			USB_PHY_CLK_VALID, USB_PHY_CLK_VALID, 2500) < 0)
			pr_err("%s: timeout waiting for phy" \
			"to stabilize\n", __func__);

		val = readl(base + USB_TXFILLTUNING);
		if ((val & USB_FIFO_TXFILL_MASK) !=
				USB_FIFO_TXFILL_THRES(0x10)) {
			val = USB_FIFO_TXFILL_THRES(0x10);
			writel(val, base + USB_TXFILLTUNING);
		}
	} else {
	/* Case for auto resume*/
		val = readl(base + USB_SUSP_CTRL);
		val |= USB_SUSP_CLR;
		writel(val, base + USB_SUSP_CTRL);

		if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
			USB_PHY_CLK_VALID, USB_PHY_CLK_VALID, 2500) < 0)
			pr_err("%s: timeout waiting for phy" \
					"to stabilize\n", __func__);

		if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
			USB_CLKEN, USB_CLKEN, 2500) < 0)
			pr_err("%s: timeout waiting for AHB clock\n", __func__);

		val = readl(base + USB_SUSP_CTRL);
		val &= ~USB_SUSP_CLR;
		writel(val, base + USB_SUSP_CTRL);
	}
	if (phy->linkphy_init) {
		/* To be done only incase of coldboot*/
		/* Fix VbusInvalid due to floating VBUS */
		ret = usb_phy_io_write(phy->ulpi_vp, 0x40, 0x08);
		if (ret) {
			pr_err("%s: ulpi write failed\n", __func__);
			return ret;
		}

		ret = usb_phy_io_write(phy->ulpi_vp, 0x80, 0x0B);
		if (ret) {
			pr_err("%s: ulpi write failed\n", __func__);
			return ret;
		}
		phy->linkphy_init = false;
	}

	val = readl(base + USB_PORTSC);
	val |= USB_PORTSC_WKOC | USB_PORTSC_WKDS | USB_PORTSC_WKCN;
	writel(val, base + USB_PORTSC);

	phy->phy_clk_on = true;
	phy->hw_accessible = true;

	return 0;
}

static inline void ulpi_link_phy_set_tristate(bool enable)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	int tristate = (enable) ? TEGRA_TRI_TRISTATE : TEGRA_TRI_NORMAL;

	tegra_pinmux_set_tristate(TEGRA_PINGROUP_CDEV2, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_UAA, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_UAB, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_UDA, tristate);
#endif
}

static void ulpi_link_phy_restore_start(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	/*Tristate ulpi interface before USB controller resume*/
	ulpi_link_phy_set_tristate(true);

	val = readl(base + ULPI_TIMING_CTRL_0);
	val &= ~ULPI_OUTPUT_PINMUX_BYP;
	writel(val, base + ULPI_TIMING_CTRL_0);
}

static void ulpi_link_phy_restore_end(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	int ret;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	val = readl(base + ULPI_TIMING_CTRL_0);
	val |= ULPI_OUTPUT_PINMUX_BYP;
	writel(val, base + ULPI_TIMING_CTRL_0);

	ulpi_link_phy_set_tristate(false);

	udelay(10);
	ret = usb_phy_io_write(phy->ulpi_vp, 0x55, 0x04);
	if (ret) {
		pr_err("%s: ulpi write failed\n", __func__);
		return;
	}
}

static int ulpi_link_phy_resume(struct tegra_usb_phy *phy)
{
	int status = 0;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->pdata->u_data.host.power_off_on_suspend) {
		status = ulpi_link_phy_power_on(phy);
		if (phy->port_speed < USB_PHY_PORT_SPEED_UNKNOWN) {
			ulpi_link_phy_restore_start(phy);
			usb_phy_bringup_host_controller(phy);
			ulpi_link_phy_restore_end(phy);
		}
	}

	return status;
}

static int ulpi_link_phy_pre_resume(struct tegra_usb_phy *phy,
			bool remote_wakeup)
{
	int status = 0;
	unsigned long val;
	void __iomem *base = phy->regs;
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	val = readl(base + USB_PORTSC);
	if (val & USB_PORTSC_RESUME) {

		val = readl(base + USB_USBCMD);
		val &= ~USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);

		/* detect remote wakeup */
		msleep(20);

		val = readl(base + USB_PORTSC);

		/* Poll until the controller clears RESUME and SUSPEND */
		if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
			USB_PORTSC_RESUME, 0, 2500))
			pr_err("%s: timeout waiting for RESUME\n", __func__);
		if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
			USB_PORTSC_SUSP, 0, 2500))
			pr_err("%s: timeout waiting for SUSPEND\n", __func__);

		/* Since we skip remote wakeup event,
		put controller in suspend again and
		resume port later */
		val = readl(base + USB_PORTSC);
		val |= USB_PORTSC_SUSP;
		writel(val, base + USB_PORTSC);
		mdelay(4);
		/* Wait until port suspend completes */
		if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
			USB_PORTSC_SUSP, USB_PORTSC_SUSP, 2500))
			pr_err("%s: timeout waiting for" \
				"PORT_SUSPEND\n", __func__);

		/* Disable interrupts */
		writel(0, base + USB_USBINTR);
		/* Clear the run bit to stop SOFs - 2LS WAR */
		val = readl(base + USB_USBCMD);
		val &= ~USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);
		if (usb_phy_reg_status_wait(base + USB_USBSTS,
			USB_USBSTS_HCH, USB_USBSTS_HCH, 2000)) {
			pr_err("%s: timeout waiting for" \
				"USB_USBSTS_HCH\n", __func__);
		}
		usb_phy_wait_for_sof(phy);

		val = readl(base + USB_USBCMD);
		val |= USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);
	}
	return status;
}


static inline void ulpi_pinmux_bypass(struct tegra_usb_phy *phy,
	bool enable)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	val = readl(base + ULPI_TIMING_CTRL_0);

	if (enable)
		val |= ULPI_OUTPUT_PINMUX_BYP;
	else
		val &= ~ULPI_OUTPUT_PINMUX_BYP;

	writel(val, base + ULPI_TIMING_CTRL_0);
}

static inline void ulpi_null_phy_set_tristate(bool enable)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	int tristate = (enable) ? TEGRA_TRI_TRISTATE : TEGRA_TRI_NORMAL;

	tegra_pinmux_set_tristate(TEGRA_PINGROUP_UDA, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_UAA, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_UAB, tristate);
#endif
}

static void ulpi_null_phy_obs_read(void)
{
	static void __iomem *apb_misc;
	unsigned slv0_obs, s2s_obs;

	if (!apb_misc)
		apb_misc = ioremap(TEGRA_APB_MISC_BASE, TEGRA_APB_MISC_SIZE);

	writel(0x80b10034, apb_misc + APB_MISC_GP_OBSCTRL_0);
	slv0_obs = readl(apb_misc + APB_MISC_GP_OBSDATA_0);

	writel(0x80b10038, apb_misc + APB_MISC_GP_OBSCTRL_0);
	s2s_obs = readl(apb_misc + APB_MISC_GP_OBSDATA_0);

	pr_debug("slv0 obs: %08x\ns2s obs: %08x\n", slv0_obs, s2s_obs);
}

static const struct gpio ulpi_gpios[] = {
	{ULPI_STP, GPIOF_IN, "ULPI_STP"},
	{ULPI_DIR, GPIOF_OUT_INIT_LOW, "ULPI_DIR"},
	{ULPI_D0, GPIOF_OUT_INIT_LOW, "ULPI_D0"},
	{ULPI_D1, GPIOF_OUT_INIT_LOW, "ULPI_D1"},
};

static int ulpi_null_phy_open(struct tegra_usb_phy *phy)
{
	struct tegra_ulpi_config *config = &phy->pdata->u_cfg.ulpi;
	int ret;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	ret = gpio_request_array(ulpi_gpios, ARRAY_SIZE(ulpi_gpios));
	if (ret)
		return ret;

	if (gpio_is_valid(config->phy_restore_gpio)) {
		ret = gpio_request(config->phy_restore_gpio, "phy_restore");
		if (ret)
			goto err_gpio_free;

		gpio_direction_input(config->phy_restore_gpio);
	}

	tegra_periph_reset_assert(phy->ctrlr_clk);
	udelay(10);
	tegra_periph_reset_deassert(phy->ctrlr_clk);

	return 0;

err_gpio_free:
	gpio_free_array(ulpi_gpios, ARRAY_SIZE(ulpi_gpios));
	return ret;
}

static void ulpi_null_phy_close(struct tegra_usb_phy *phy)
{
	struct tegra_ulpi_config *config = &phy->pdata->u_cfg.ulpi;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (gpio_is_valid(config->phy_restore_gpio))
		gpio_free(config->phy_restore_gpio);

	gpio_free_array(ulpi_gpios, ARRAY_SIZE(ulpi_gpios));
}

static int ulpi_null_phy_power_off(struct tegra_usb_phy *phy)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (!phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already off\n", __func__,
							__LINE__, phy->inst);
		return 0;
	}

	phy->phy_clk_on = false;
	phy->hw_accessible = false;
	ulpi_null_phy_set_tristate(true);
	return 0;
}

static int ulpi_null_phy_irq(struct tegra_usb_phy *phy)
{
	usb_phy_fence_read(phy);
	return IRQ_HANDLED;
}

static int ulpi_null_phy_restore(struct tegra_usb_phy *phy)
{
	struct tegra_ulpi_config *config = &phy->pdata->u_cfg.ulpi;
	unsigned long timeout;
	int ulpi_stp = ULPI_STP;

	if (gpio_is_valid(config->phy_restore_gpio))
		ulpi_stp = config->phy_restore_gpio;

	/* disable ULPI pinmux bypass */
	ulpi_pinmux_bypass(phy, false);

	/* driving linstate by GPIO */
	gpio_set_value(ULPI_D0, 0);
	gpio_set_value(ULPI_D1, 0);

	/* driving DIR high */
	gpio_set_value(ULPI_DIR, 1);

	/* remove ULPI tristate */
	ulpi_null_phy_set_tristate(false);

	/* wait for STP high */
	timeout = jiffies + msecs_to_jiffies(25);

	while (!gpio_get_value(ulpi_stp)) {
		if (time_after(jiffies, timeout)) {
			pr_warn("phy restore timeout\n");
			return 1;
		}
	}

	return 0;
}

static int ulpi_null_phy_lp0_resume(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	val = readl(base + USB_USBCMD);
	val |= USB_USBCMD_RESET;
	writel(val, base + USB_USBCMD);

	if (usb_phy_reg_status_wait(base + USB_USBCMD,
		USB_USBCMD_RESET, 0, 2500) < 0) {
		pr_err("%s: timeout waiting for reset\n", __func__);
	}

	val = readl(base + USB_USBMODE_REG_OFFSET);
	val &= ~USB_USBMODE_MASK;
	val |= USB_USBMODE_HOST;
	writel(val, base + USB_USBMODE_REG_OFFSET);

	val = readl(base + USB_USBCMD);
	val |= USB_USBCMD_RS;
	writel(val, base + USB_USBCMD);
	if (usb_phy_reg_status_wait(base + USB_USBCMD, USB_USBCMD_RS,
						 USB_USBCMD_RS, 2000)) {
		pr_err("%s: timeout waiting for USB_USBCMD_RS\n", __func__);
		return -ETIMEDOUT;
	}

	/* Enable Port Power */
	val = readl(base + USB_PORTSC);
	val |= USB_PORTSC_PP;
	writel(val, base + USB_PORTSC);
	udelay(10);

	ulpi_null_phy_restore(phy);

	return 0;
}

static int ulpi_null_phy_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	struct tegra_ulpi_config *config = &phy->pdata->u_cfg.ulpi;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already On\n", __func__,
							__LINE__, phy->inst);
		return 0;
	}

	val = readl(base + USB_SUSP_CTRL);
	val |= UHSIC_RESET;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + ULPI_TIMING_CTRL_0);
	val |= ULPI_OUTPUT_PINMUX_BYP | ULPI_CLKOUT_PINMUX_BYP;
	writel(val, base + ULPI_TIMING_CTRL_0);

	val = readl(base + USB_SUSP_CTRL);
	val |= ULPI_PHY_ENABLE;
	writel(val, base + USB_SUSP_CTRL);
	udelay(10);

	/* set timming parameters */
	val = readl(base + ULPI_TIMING_CTRL_0);
	val |= ULPI_SHADOW_CLK_LOOPBACK_EN;
	val |= ULPI_SHADOW_CLK_SEL;
	val |= ULPI_LBK_PAD_EN;
	val |= ULPI_SHADOW_CLK_DELAY(config->shadow_clk_delay);
	val |= ULPI_CLOCK_OUT_DELAY(config->clock_out_delay);
	val |= ULPI_LBK_PAD_E_INPUT_OR;
	writel(val, base + ULPI_TIMING_CTRL_0);

	writel(0, base + ULPI_TIMING_CTRL_1);
	udelay(10);

	/* start internal 60MHz clock */
	val = readl(base + ULPIS2S_CTRL);
	val |= ULPIS2S_ENA;
	val |= ULPIS2S_SUPPORT_DISCONNECT;
	val |= ULPIS2S_SPARE((phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST)
				? 3 : 1);
	val |= ULPIS2S_PLLU_MASTER_BLASTER60;
	writel(val, base + ULPIS2S_CTRL);

	/* select ULPI_CORE_CLK_SEL to SHADOW_CLK */
	val = readl(base + ULPI_TIMING_CTRL_0);
	val |= ULPI_CORE_CLK_SEL;
	writel(val, base + ULPI_TIMING_CTRL_0);
	udelay(10);

	/* enable ULPI null phy clock - can't set the trimmers before this */
	val = readl(base + ULPI_TIMING_CTRL_0);
	val |= ULPI_CLK_OUT_ENA;
	writel(val, base + ULPI_TIMING_CTRL_0);
	udelay(10);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL, USB_PHY_CLK_VALID,
						 USB_PHY_CLK_VALID, 2500)) {
		pr_err("%s: timeout waiting for phy to stabilize\n", __func__);
		return -ETIMEDOUT;
	}

	/* set ULPI trimmers */
	ulpi_set_trimmer(phy);

	if (!phy->ulpi_clk_padout_ena) {
		val = readl(base + ULPI_TIMING_CTRL_0);
		val |= ULPI_CLK_PADOUT_ENA;
		writel(val, base + ULPI_TIMING_CTRL_0);
		phy->ulpi_clk_padout_ena = true;
	} else {
		if (!readl(base + USB_ASYNCLISTADDR))
			ulpi_null_phy_lp0_resume(phy);
	}
	udelay(10);

	phy->phy_clk_on = true;
	phy->hw_accessible = true;

	return 0;
}

static int ulpi_null_phy_pre_resume(struct tegra_usb_phy *phy,
				    bool remote_wakeup)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	ulpi_null_phy_obs_read();
	usb_phy_wait_for_sof(phy);
	ulpi_null_phy_obs_read();
	return 0;
}

static int ulpi_null_phy_post_resume(struct tegra_usb_phy *phy)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	ulpi_null_phy_obs_read();
	return 0;
}

static int ulpi_null_phy_resume(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	if (!readl(base + USB_ASYNCLISTADDR)) {
		/* enable ULPI CLK output pad */
		val = readl(base + ULPI_TIMING_CTRL_0);
		val |= ULPI_CLK_PADOUT_ENA;
		writel(val, base + ULPI_TIMING_CTRL_0);

		/* enable ULPI pinmux bypass */
		ulpi_pinmux_bypass(phy, true);
		udelay(5);
	}

	return 0;
}



static struct tegra_usb_phy_ops utmi_phy_ops = {
	.open		= utmi_phy_open,
	.close		= utmi_phy_close,
	.irq		= utmi_phy_irq,
	.power_on	= utmi_phy_power_on,
	.power_off	= utmi_phy_power_off,
	.pre_resume = utmi_phy_pre_resume,
	.resume	= utmi_phy_resume,
	.post_resume	= utmi_phy_post_resume,
	.charger_detect = utmi_phy_charger_detect,
	.post_suspend   = phy_post_suspend,
};

static struct tegra_usb_phy_ops uhsic_phy_ops = {
	.open		= uhsic_phy_open,
	.irq		= uhsic_phy_irq,
	.power_on	= uhsic_phy_power_on,
	.power_off	= uhsic_phy_power_off,
	.pre_resume	= uhsic_phy_pre_resume,
	.resume		= uhsic_phy_resume,
	.post_resume	= uhsic_phy_post_resume,
	.port_power	= uhsic_phy_bus_port_power,
	.bus_reset	= uhsic_phy_bus_reset,
	.post_suspend   = phy_post_suspend,
};

static struct tegra_usb_phy_ops ulpi_link_phy_ops = {
	.open		= ulpi_link_phy_open,
	.close	= ulpi_link_phy_close,
	.irq		= ulpi_link_phy_irq,
	.power_on	= ulpi_link_phy_power_on,
	.power_off	= ulpi_link_phy_power_off,
	.resume		= ulpi_link_phy_resume,
	.post_suspend   = phy_post_suspend,
	.pre_resume	= ulpi_link_phy_pre_resume,
};

static struct tegra_usb_phy_ops ulpi_null_phy_ops = {
	.open		= ulpi_null_phy_open,
	.close		= ulpi_null_phy_close,
	.irq		= ulpi_null_phy_irq,
	.power_on	= ulpi_null_phy_power_on,
	.power_off	= ulpi_null_phy_power_off,
	.pre_resume = ulpi_null_phy_pre_resume,
	.resume = ulpi_null_phy_resume,
	.post_suspend   = phy_post_suspend,
	.post_resume = ulpi_null_phy_post_resume,
};

static struct tegra_usb_phy_ops icusb_phy_ops;


static struct tegra_usb_phy_ops *phy_ops[] = {
	[TEGRA_USB_PHY_INTF_UTMI] = &utmi_phy_ops,
	[TEGRA_USB_PHY_INTF_ULPI_LINK] = &ulpi_link_phy_ops,
	[TEGRA_USB_PHY_INTF_ULPI_NULL] = &ulpi_null_phy_ops,
	[TEGRA_USB_PHY_INTF_HSIC] = &uhsic_phy_ops,
	[TEGRA_USB_PHY_INTF_ICUSB] = &icusb_phy_ops,
};

int tegra2_usb_phy_init_ops(struct tegra_usb_phy *phy)
{
	phy->ops = phy_ops[phy->pdata->phy_intf];

	return 0;
}

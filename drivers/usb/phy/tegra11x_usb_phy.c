/*
 * drivers/usb/phy/tegra11x_usb_phy.c
 *
 * Copyright (c) 2012-2014 NVIDIA Corporation. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-fuse.h>
#include <mach/pinmux.h>
#include <mach/tegra_usb_pmc.h>
#include <mach/tegra_usb_pad_ctrl.h>

#ifdef CONFIG_ARCH_TEGRA_14x_SOC
#include <mach/pinmux-t14.h>
#else
#include <mach/pinmux-t11.h>
#endif
#include "tegra_usb_phy.h"
#include "../../../arch/arm/mach-tegra/gpio-names.h"

/* HACK! This needs to come from DT */
#include "../../../arch/arm/mach-tegra/iomap.h"

#define USB_USBCMD		0x130
#define   USB_USBCMD_RS		(1 << 0)
#define   USB_CMD_RESET	(1<<1)

#define USB_USBSTS		0x134
#define   USB_USBSTS_PCI	(1 << 2)
#define   USB_USBSTS_SRI	(1 << 7)
#define   USB_USBSTS_HCH	(1 << 12)

#define USB_TXFILLTUNING        0x154
#define   USB_FIFO_TXFILL_THRES(x)   (((x) & 0x1f) << 16)
#define   USB_FIFO_TXFILL_MASK    0x1f0000

#define USB_ASYNCLISTADDR	0x148

#define ICUSB_CTRL		0x15c

#define USB_USBMODE		0x1f8
#define   USB_USBMODE_MASK		(3 << 0)
#define   USB_USBMODE_HOST		(3 << 0)
#define   USB_USBMODE_DEVICE		(2 << 0)

#define USB_SUSP_CTRL		0x400
#define   USB_WAKE_ON_CNNT_EN_DEV	(1 << 3)
#define   USB_WAKE_ON_DISCON_EN_DEV (1 << 4)
#define   USB_SUSP_CLR			(1 << 5)
#define   USB_CLKEN		(1 << 6)
#define   USB_PHY_CLK_VALID		(1 << 7)
#define   USB_PHY_CLK_VALID_INT_ENB	(1 << 9)
#define   USB_PHY_CLK_VALID_INT_STS	(1 << 8)
#define   UTMIP_RESET			(1 << 11)
#define   UTMIP_PHY_ENABLE		(1 << 12)
#define   UHSIC_RESET			(1 << 14)
#define   USB_WAKEUP_DEBOUNCE_COUNT(x)	(((x) & 0x7) << 16)
#define   UHSIC_PHY_ENABLE		(1 << 19)

#define USB_PHY_VBUS_WAKEUP_ID	0x408
#define   DIV_DET_EN		(1 << 31)
#define   VDCD_DET_STS		(1 << 26)
#define   VDCD_DET_CHG_DET	(1 << 25)
#define   VOP_DIV2P7_DET	(1 << 23)
#define   VOP_DIV2P0_DET	(1 << 22)
#define   VON_DIV2P7_DET	(1 << 15)
#define   VON_DIV2P0_DET	(1 << 14)
#define   VDAT_DET_INT_EN	(1 << 16)
#define   VDAT_DET_CHG_DET	(1 << 17)
#define   VDAT_DET_STS		(1 << 18)
#define   USB_ID_STATUS		(1 << 2)

#define USB_IF_SPARE	0x498
#define   USB_HS_RSM_EOP_EN         (1 << 4)
#define   USB_PORT_SUSPEND_EN       (1 << 5)

#define USB_NEW_CONTROL  0x4c0
#define   USB_COHRENCY_EN           (1 << 0)
#define   USB_MEM_ALLIGNMENT_MUX_EN (1 << 1)

#define UTMIP_XCVR_CFG0		0x808
#define   UTMIP_XCVR_SETUP(x)			(((x) & 0xf) << 0)
#define   UTMIP_XCVR_LSRSLEW(x)			(((x) & 0x3) << 8)
#define   UTMIP_XCVR_LSFSLEW(x)			(((x) & 0x3) << 10)
#define   UTMIP_FORCE_PD_POWERDOWN		(1 << 14)
#define   UTMIP_FORCE_PD2_POWERDOWN		(1 << 16)
#define   UTMIP_FORCE_PDZI_POWERDOWN		(1 << 18)
#define   UTMIP_XCVR_LSBIAS_SEL			(1 << 21)
#define   UTMIP_XCVR_SETUP_MSB(x)		(((x) & 0x7) << 22)
#define   UTMIP_XCVR_HSSLEW_MSB(x)		(((x) & 0x7f) << 25)
#define   UTMIP_XCVR_HSSLEW_LSB(x)		(((x) & 0x3) << 4)
#define   UTMIP_XCVR_MAX_OFFSET		2
#define   UTMIP_XCVR_SETUP_MAX_VALUE	0x7f
#define   UTMIP_XCVR_SETUP_MIN_VALUE	0
#define   XCVR_SETUP_MSB_CALIB(x) ((x) >> 4)

#define UTMIP_HSRX_CFG0		0x810
#define   UTMIP_ELASTIC_LIMIT(x)	(((x) & 0x1f) << 10)
#define   UTMIP_IDLE_WAIT(x)		(((x) & 0x1f) << 15)

#define UTMIP_HSRX_CFG1		0x814
#define   UTMIP_HS_SYNC_START_DLY(x)	(((x) & 0x1f) << 1)

#define UTMIP_TX_CFG0		0x820
#define   UTMIP_FS_PREABMLE_J		(1 << 19)
#define   UTMIP_HS_DISCON_DISABLE	(1 << 8)

#define UTMIP_DEBOUNCE_CFG0 0x82c
#define   UTMIP_BIAS_DEBOUNCE_A(x)	(((x) & 0xffff) << 0)

#define UTMIP_BAT_CHRG_CFG0 0x830
#define   UTMIP_PD_CHRG			(1 << 0)
#define   UTMIP_OP_SINK_EN		(1 << 1)
#define   UTMIP_ON_SINK_EN		(1 << 2)
#define   UTMIP_OP_SRC_EN		(1 << 3)
#define   UTMIP_ON_SRC_EN		(1 << 4)
#define   UTMIP_OP_I_SRC_EN		(1 << 5)

#define UTMIP_XCVR_CFG1		0x838
#define   UTMIP_FORCE_PDDISC_POWERDOWN	(1 << 0)
#define   UTMIP_FORCE_PDCHRP_POWERDOWN	(1 << 2)
#define   UTMIP_FORCE_PDDR_POWERDOWN	(1 << 4)
#define   UTMIP_XCVR_TERM_RANGE_ADJ(x)	(((x) & 0xf) << 18)

#define UTMIP_MISC_CFG0		0x824
#define   UTMIP_DPDM_OBSERVE		(1 << 26)
#define   UTMIP_DPDM_OBSERVE_SEL(x) (((x) & 0xf) << 27)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_J	UTMIP_DPDM_OBSERVE_SEL(0xf)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_K	UTMIP_DPDM_OBSERVE_SEL(0xe)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_SE1 UTMIP_DPDM_OBSERVE_SEL(0xd)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_SE0 UTMIP_DPDM_OBSERVE_SEL(0xc)
#define   UTMIP_SUSPEND_EXIT_ON_EDGE	(1 << 22)
#define   DISABLE_PULLUP_DP (1 << 15)
#define   DISABLE_PULLUP_DM (1 << 14)
#define   DISABLE_PULLDN_DP (1 << 13)
#define   DISABLE_PULLDN_DM (1 << 12)
#define   FORCE_PULLUP_DP	(1 << 11)
#define   FORCE_PULLUP_DM	(1 << 10)
#define   FORCE_PULLDN_DP	(1 << 9)
#define   FORCE_PULLDN_DM	(1 << 8)
#define   COMB_TERMS		(1 << 0)
#define   ALWAYS_FREE_RUNNING_TERMS (1 << 1)
#define   MASK_ALL_PULLUP_PULLDOWN (0xff << 8)

#define UTMIP_SPARE_CFG0	0x834
#define   FUSE_SETUP_SEL		(1 << 3)
#define   FUSE_ATERM_SEL		(1 << 4)

#define UHSIC_PLL_CFG1				0xc04
#define   UHSIC_XTAL_FREQ_COUNT(x)		(((x) & 0xfff) << 0)
#define   UHSIC_PLLU_ENABLE_DLY_COUNT(x)	(((x) & 0x1f) << 14)

#define UHSIC_HSRX_CFG0				0xc08
#define   UHSIC_ELASTIC_UNDERRUN_LIMIT(x)	(((x) & 0x1f) << 2)
#define   UHSIC_ELASTIC_OVERRUN_LIMIT(x)	(((x) & 0x1f) << 8)
#define   UHSIC_IDLE_WAIT(x)			(((x) & 0x1f) << 13)

#define UHSIC_HSRX_CFG1				0xc0c
#define   UHSIC_HS_SYNC_START_DLY(x)		(((x) & 0x1f) << 1)

#define UHSIC_TX_CFG0				0xc10
#define   UHSIC_HS_READY_WAIT_FOR_VALID	(1 << 9)
#define   UHSIC_HS_POSTAMBLE_OUTPUT_ENABLE	(1 << 6)


#define UHSIC_MISC_CFG0				0xc14
#define   UHSIC_SUSPEND_EXIT_ON_EDGE		(1 << 7)
#define   UHSIC_DETECT_SHORT_CONNECT		(1 << 8)
#define   UHSIC_FORCE_XCVR_MODE			(1 << 15)
#define   UHSIC_DISABLE_BUSRESET		(1 << 20)

#define UHSIC_MISC_CFG1				0xc18
#define   UHSIC_PLLU_STABLE_COUNT(x)		(((x) & 0xfff) << 2)

#define UHSIC_PADS_CFG0				0xc1c
#define   UHSIC_TX_RTUNEN			0xf000
#define   UHSIC_TX_RTUNEP			0xf00
#define   UHSIC_TX_RTUNE_P(x)			(((x) & 0xf) << 8)
#define   UHSIC_TX_SLEWP			(0xf << 16)
#define   UHSIC_TX_SLEWN			(0xf << 20)

#define UHSIC_PADS_CFG1				0xc20
#define   UHSIC_AUTO_RTERM_EN			(1 << 0)
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

#define UHSIC_SPARE_CFG0 			0xc2c
#define   FORCE_BK_ON				(1 << 12)

#define UHSIC_STAT_CFG0			0xc28
#define   UHSIC_CONNECT_DETECT		(1 << 0)

#define UHSIC_STATUS(inst)		UHSIC_INST(inst, 0x214, 0x290)
#define   UHSIC_WAKE_ALARM(inst)	(1 << UHSIC_INST(inst, 19, 4))
#define   UHSIC_WALK_PTR_VAL(inst)	(0x3 << UHSIC_INST(inst, 6, 0))
#define   UHSIC_DATA_VAL(inst)		(1 << UHSIC_INST(inst, 15, 3))
#define   UHSIC_STROBE_VAL(inst)	(1 << UHSIC_INST(inst, 14, 2))

#define UHSIC_CMD_CFG0			0xc24
#define   UHSIC_PRETEND_CONNECT_DETECT	(1 << 5)

#define USB_USBINTR						0x138

#define UTMIP_STATUS		0x214
#define   UTMIP_WALK_PTR_VAL(inst)	(0x3 << ((inst)*2))
#define   UTMIP_USBOP_VAL(inst)		(1 << ((2*(inst)) + 8))
#define   UTMIP_USBOP_VAL_P2		(1 << 12)
#define   UTMIP_USBOP_VAL_P1		(1 << 10)
#define   UTMIP_USBOP_VAL_P0		(1 << 8)
#define   UTMIP_USBON_VAL(inst)		(1 << ((2*(inst)) + 9))
#define   UTMIP_USBON_VAL_P2		(1 << 13)
#define   UTMIP_USBON_VAL_P1		(1 << 11)
#define   UTMIP_USBON_VAL_P0		(1 << 9)
#define   UTMIP_WAKE_ALARM(inst)		(1 << ((inst) + 16))
#define   UTMIP_WAKE_ALARM_P2	(1 << 18)
#define   UTMIP_WAKE_ALARM_P1	(1 << 17)
#define   UTMIP_WAKE_ALARM_P0	(1 << 16)
#define   UTMIP_WALK_PTR(inst)	(1 << ((inst)*2))
#define   UTMIP_WALK_PTR_P2	(1 << 4)
#define   UTMIP_WALK_PTR_P1	(1 << 2)
#define   UTMIP_WALK_PTR_P0	(1 << 0)

#define USB1_PREFETCH_ID               6
#define USB2_PREFETCH_ID               18
#define USB3_PREFETCH_ID               17

#define FUSE_USB_CALIB_0		0x1F0
#define   XCVR_SETUP(x)	(((x) & 0x7F) << 0)
#define   XCVR_SETUP_LSB_MASK	0xF
#define   XCVR_SETUP_MSB_MASK	0x70
#define   XCVR_SETUP_LSB_MAX_VAL	0xF

#define APB_MISC_GP_OBSCTRL_0	0x818
#define APB_MISC_GP_OBSDATA_0	0x81c

#define PADCTL_SNPS_OC_MAP	0xC
#define   CONTROLLER_OC(inst, x)	(((x) & 0x7) << (3 * (inst)))
#define   CONTROLLER_OC_P0(x)	(((x) & 0x7) << 0)
#define   CONTROLLER_OC_P1(x)	(((x) & 0x7) << 3)
#define   CONTROLLER_OC_P2(x)	(((x) & 0x7) << 6)

#define PADCTL_OC_DET		0x18
#define   ENABLE0_OC_MAP(x)	(((x) & 0x7) << 10)
#define   ENABLE1_OC_MAP(x)	(((x) & 0x7) << 13)

#define TEGRA_STREAM_DISABLE	0x1f8
#define TEGRA_STREAM_DISABLE_OFFSET	(1 << 4)

/* These values (in milli second) are taken from the battery charging spec */
#define TDP_SRC_ON_MS	 100
#define TDPSRC_CON_MS	 40

/* Force port resume wait time in micro second on remote resume */
#define FPR_WAIT_TIME_US 25000

/* define HSIC phy params */
#define HSIC_SYNC_START_DELAY		9
#define HSIC_IDLE_WAIT_DELAY		17
#define HSIC_ELASTIC_UNDERRUN_LIMIT	16
#define HSIC_ELASTIC_OVERRUN_LIMIT	16

struct tegra_usb_pmc_data pmc_data[3];

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

static void pmc_init(struct tegra_usb_phy *phy)
{
	pmc_data[phy->inst].instance = phy->inst;
	pmc_data[phy->inst].phy_type = phy->pdata->phy_intf;
	pmc_data[phy->inst].controller_type = TEGRA_USB_2_0;
	pmc_data[phy->inst].usb_base = phy->regs;
	tegra_usb_pmc_init(&pmc_data[phy->inst]);
}

static int _usb_phy_init(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	val = readl(base + HOSTPC1_DEVLC);
	val &= ~HOSTPC1_DEVLC_STS;
	writel(val, base + HOSTPC1_DEVLC);

	val = readl(base + USB_IF_SPARE);
	val |= USB_HS_RSM_EOP_EN;
	val |= USB_PORT_SUSPEND_EN;
	writel(val, base + USB_IF_SPARE);

	if (phy->pdata->unaligned_dma_buf_supported == true) {
		val = readl(base + USB_NEW_CONTROL);
		val |= USB_COHRENCY_EN;
		val |= USB_MEM_ALLIGNMENT_MUX_EN;
		writel(val, base + USB_NEW_CONTROL);
	}
	val =  readl(base + TEGRA_STREAM_DISABLE);
	if (!tegra_platform_is_silicon())
		val |= TEGRA_STREAM_DISABLE_OFFSET;
	else
		val &= ~TEGRA_STREAM_DISABLE_OFFSET;
	writel(val , base + TEGRA_STREAM_DISABLE);

	return 0;
}

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

static int usb_phy_reset(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (!tegra_platform_is_silicon()) {
		val =  readl(base + TEGRA_STREAM_DISABLE);
		val |= TEGRA_STREAM_DISABLE_OFFSET;
		writel(val , base + TEGRA_STREAM_DISABLE);
	}
	val = readl(base + USB_TXFILLTUNING);
	if ((val & USB_FIFO_TXFILL_MASK) != USB_FIFO_TXFILL_THRES(0x10)) {
		val = USB_FIFO_TXFILL_THRES(0x10);
		writel(val, base + USB_TXFILLTUNING);
	}

return 0;
}

static bool utmi_phy_remotewake_detected(struct tegra_usb_phy *phy)
{
	void __iomem *base = phy->regs;
	unsigned  int inst = phy->inst;
	u32 val;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(base + UTMIP_PMC_WAKEUP0);
	if (val & EVENT_INT_ENB) {
		val = tegra_usb_pmc_reg_read(UTMIP_STATUS);
		if (UTMIP_WAKE_ALARM(inst) & val) {
			tegra_usb_pmc_reg_update(PMC_SLEEP_CFG,
				UTMIP_WAKE_VAL(inst, 0xF),
				UTMIP_WAKE_VAL(inst, WAKE_VAL_NONE));

			tegra_usb_pmc_reg_update(PMC_TRIGGERS, 0,
				UTMIP_CLR_WAKE_ALARM(inst));

			val = readl(base + UTMIP_PMC_WAKEUP0);
			val &= ~EVENT_INT_ENB;
			writel(val, base + UTMIP_PMC_WAKEUP0);

			val = tegra_usb_pmc_reg_read(UTMIP_STATUS);
			if (phy->port_speed < USB_PHY_PORT_SPEED_UNKNOWN)
				phy->pmc_remote_wakeup = true;
			else
				phy->pmc_hotplug_wakeup = true;

			DBG("%s: utmip PMC interrupt detected\n", __func__);
			return true;
		}
	}
	return false;
}

static int usb_phy_bringup_host_controller(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	PHY_DBG("[%d] USB_USBSTS[0x%x] USB_PORTSC[0x%x] port_speed[%d]\n",
	__LINE__, readl(base + USB_USBSTS), readl(base + USB_PORTSC),
							phy->port_speed);

	/* Device is plugged in when system is in LP0 */
	/* Bring up the controller from LP0*/
	val = readl(base + USB_USBCMD);
	val |= USB_CMD_RESET;
	writel(val, base + USB_USBCMD);

	if (usb_phy_reg_status_wait(base + USB_USBCMD,
		USB_CMD_RESET, 0, 2500) < 0) {
		pr_err("%s: timeout waiting for reset\n", __func__);
	}

	val = readl(base + USB_USBMODE);
	val &= ~USB_USBMODE_MASK;
	val |= USB_USBMODE_HOST;
	writel(val, base + USB_USBMODE);
	val = readl(base + HOSTPC1_DEVLC);
	val &= ~HOSTPC1_DEVLC_PTS(~0);

	if (phy->pdata->phy_intf == TEGRA_USB_PHY_INTF_HSIC)
		val |= HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_HSIC);
	else
		val |= HOSTPC1_DEVLC_STS;
	writel(val, base + HOSTPC1_DEVLC);

	/* Enable Port Power */
	val = readl(base + USB_PORTSC);
	val |= USB_PORTSC_PP;
	writel(val, base + USB_PORTSC);
	udelay(10);

	/* Check if the phy resume from LP0. When the phy resume from LP0
	 * USB register will be reset.to zero */
	if (!readl(base + USB_ASYNCLISTADDR)) {
		/* Program the field PTC based on the saved speed mode */
		val = readl(base + USB_PORTSC);
		val &= ~USB_PORTSC_PTC(~0);
		if ((phy->port_speed == USB_PHY_PORT_SPEED_HIGH) ||
			(phy->pdata->phy_intf == TEGRA_USB_PHY_INTF_HSIC))
			val |= USB_PORTSC_PTC(5);
		else if (phy->port_speed == USB_PHY_PORT_SPEED_FULL)
			val |= USB_PORTSC_PTC(6);
		else if (phy->port_speed == USB_PHY_PORT_SPEED_LOW)
			val |= USB_PORTSC_PTC(7);
		writel(val, base + USB_PORTSC);
		udelay(10);

		/* Disable test mode by setting PTC field to NORMAL_OP */
		val = readl(base + USB_PORTSC);
		val &= ~USB_PORTSC_PTC(~0);
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

	phy->ctrlr_suspended = false;
	if (!phy->pmc_remote_wakeup) {
		/* Put controller in suspend mode by writing 1
		 * to SUSP bit of PORTSC */
		val = readl(base + USB_PORTSC);
		if ((val & USB_PORTSC_PP) && (val & USB_PORTSC_PE)) {
			val |= USB_PORTSC_SUSP;
			writel(val, base + USB_PORTSC);
			phy->ctrlr_suspended = true;
			/* Wait until port suspend completes */
			if (usb_phy_reg_status_wait(base + USB_PORTSC,
				USB_PORTSC_SUSP, USB_PORTSC_SUSP, 4000)) {
				pr_err("%s: timeout waiting for PORT_SUSPEND\n",
					__func__);
			}
		}
	}
	PHY_DBG("[%d] USB_USBSTS[0x%x] USB_PORTSC[0x%x] port_speed[%d]\n",
		__LINE__, readl(base + USB_USBSTS), readl(base + USB_PORTSC),
		phy->port_speed);

	DBG("USB_USBSTS[0x%x] USB_PORTSC[0x%x]\n",
			readl(base + USB_USBSTS), readl(base + USB_PORTSC));
	return 0;
}

static unsigned int utmi_phy_xcvr_setup_value(struct tegra_usb_phy *phy)
{
	struct tegra_utmi_config *cfg = &phy->pdata->u_cfg.utmi;
	signed long val;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (cfg->xcvr_use_fuses) {
		val = XCVR_SETUP(tegra_fuse_readl(FUSE_USB_CALIB_0));
		if (cfg->xcvr_use_lsb) {
			val = min((unsigned int) ((val & XCVR_SETUP_LSB_MASK)
				+ cfg->xcvr_setup_offset),
				(unsigned int) XCVR_SETUP_LSB_MAX_VAL);
			val |= (cfg->xcvr_setup & XCVR_SETUP_MSB_MASK);
		} else {
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
		}
	} else {
		val = cfg->xcvr_setup;
	}

	return (unsigned int) val;
}

static int utmi_phy_open(struct tegra_usb_phy *phy)
{
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	unsigned long parent_rate;
	int i;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	phy->utmi_pad_clk = clk_get_sys("utmip-pad", NULL);
	if (IS_ERR(phy->utmi_pad_clk)) {
		pr_err("%s: can't get utmip pad clock\n", __func__);
		return PTR_ERR(phy->utmi_pad_clk);
	}
	pmc_init(phy);

	phy->utmi_xcvr_setup = utmi_phy_xcvr_setup_value(phy);

	if (tegra_platform_is_silicon()) {
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
	}

	/* Power-up the VBUS, ID detector for UTMIP PHY */
	if (phy->pdata->id_det_type == TEGRA_USB_ID)
		tegra_usb_pmc_reg_update(PMC_USB_AO,
			PMC_USB_AO_VBUS_WAKEUP_PD_P0 | PMC_USB_AO_ID_PD_P0, 0);
	else
		tegra_usb_pmc_reg_update(PMC_USB_AO,
			PMC_USB_AO_VBUS_WAKEUP_PD_P0, PMC_USB_AO_ID_PD_P0);

	pmc->pmc_ops->powerup_pmc_wake_detect(pmc);

	return 0;
}

static void utmi_phy_close(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	DBG("%s inst:[%d]\n", __func__, phy->inst);

	/* Disable PHY clock valid interrupts while going into suspend*/
	if (phy->hot_plug) {
		val = readl(base + USB_SUSP_CTRL);
		val &= ~USB_PHY_CLK_VALID_INT_ENB;
		writel(val, base + USB_SUSP_CTRL);
	}

	val = tegra_usb_pmc_reg_read(PMC_SLEEP_CFG);
	if (val & UTMIP_MASTER_ENABLE(phy->inst)) {
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);

		phy->pmc_remote_wakeup = false;
		phy->pmc_hotplug_wakeup = false;
		PHY_DBG("%s DISABLE_PMC inst = %d\n", __func__, phy->inst);
	}

	clk_put(phy->utmi_pad_clk);
}

static int utmi_phy_irq(struct tegra_usb_phy *phy)
{
	void __iomem *base = phy->regs;
	unsigned long val = 0;
	bool remote_wakeup = false;
	int irq_status = IRQ_HANDLED;

	if (phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
		DBG("USB_USBSTS[0x%x] USB_PORTSC[0x%x]\n",
			readl(base + USB_USBSTS), readl(base + USB_PORTSC));
		DBG("USB_USBMODE[0x%x] USB_USBCMD[0x%x]\n",
			readl(base + USB_USBMODE), readl(base + USB_USBCMD));
	}
	if (!phy->pdata->unaligned_dma_buf_supported)
		usb_phy_fence_read(phy);
	/* check if it is pmc wake event */
	if (utmi_phy_remotewake_detected(phy))
		remote_wakeup = phy->pmc_remote_wakeup;

	if (phy->hot_plug) {
		val = readl(base + USB_SUSP_CTRL);
		if ((val  & USB_PHY_CLK_VALID_INT_STS) &&
			(val  & USB_PHY_CLK_VALID_INT_ENB)) {
			val &= ~USB_PHY_CLK_VALID_INT_ENB |
					USB_PHY_CLK_VALID_INT_STS;
			writel(val , (base + USB_SUSP_CTRL));

			/* In case of remote wakeup PHY clock will not up
			 * immediately, so should not access any controller
			 * register but normal plug-in/plug-out should be
			 * executed
			 */
			if (!remote_wakeup) {
				val = readl(base + USB_USBSTS);
				if (!(val  & USB_USBSTS_PCI)) {
					irq_status = IRQ_NONE;
					goto exit;
				}

				val = readl(base + USB_PORTSC);
				if (val & USB_PORTSC_CCS)
					val &= ~USB_PORTSC_WKCN;
				else
					val &= ~USB_PORTSC_WKDS;
				val &= ~USB_PORTSC_RWC_BITS;
				writel(val , (base + USB_PORTSC));
			}
		} else if (!phy->phy_clk_on) {
			if (remote_wakeup)
				irq_status = IRQ_HANDLED;
			else
				irq_status = IRQ_NONE;
			goto exit;
		}
	}
exit:
	return irq_status;
}

static int utmi_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
{
	unsigned long val;
	unsigned  int inst = phy->inst;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = tegra_usb_pmc_reg_read(PMC_SLEEP_CFG);
	if (val & UTMIP_MASTER_ENABLE(inst)) {
		if (!remote_wakeup) {
			pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);

			phy->pmc_remote_wakeup = false;
			phy->pmc_hotplug_wakeup = false;
			PHY_DBG("%s DISABLE_PMC inst = %d\n", __func__, inst);
		}
	}

	return 0;
}

static int utmi_phy_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	PHY_DBG("%s(%d) inst:[%d] BEGIN\n", __func__, __LINE__, phy->inst);
	if (!phy->phy_clk_on) {
		PHY_DBG("%s(%d) inst:[%d] phy clk is already off\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		pmc_data[phy->inst].is_xhci = phy->pdata->u_data.dev.is_xhci;
		pmc->pmc_ops->powerdown_pmc_wake_detect(pmc);
		val = readl(base + USB_SUSP_CTRL);
		val &= ~USB_WAKEUP_DEBOUNCE_COUNT(~0);
		val |= USB_WAKE_ON_CNNT_EN_DEV | USB_WAKEUP_DEBOUNCE_COUNT(5);
		writel(val, base + USB_SUSP_CTRL);

		val = readl(base + UTMIP_BAT_CHRG_CFG0);
		val |= UTMIP_PD_CHRG;
		writel(val, base + UTMIP_BAT_CHRG_CFG0);
	} else {
		phy->port_speed = (readl(base + HOSTPC1_DEVLC) >> 25) &
				HOSTPC1_DEVLC_PSPD_MASK;

		/* Disable interrupts */
		writel(0, base + USB_USBINTR);

		/* Clear the run bit to stop SOFs when USB is suspended */
		val = readl(base + USB_USBCMD);
		val &= ~USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);

		if (usb_phy_reg_status_wait(base + USB_USBSTS, USB_USBSTS_HCH,
						 USB_USBSTS_HCH, 2000)) {
			pr_err("%s: timeout waiting for USB_USBSTS_HCH\n"
							, __func__);
		}

		pmc_data[phy->inst].port_speed = (readl(base +
				HOSTPC1_DEVLC) >> 25) &
				HOSTPC1_DEVLC_PSPD_MASK;

		if (pmc_data[phy->inst].port_speed <
					USB_PMC_PORT_SPEED_UNKNOWN)
			phy->pmc_remote_wakeup = false;
		else
			phy->pmc_hotplug_wakeup = false;

		if (phy->pdata->port_otg) {
			bool id_present = false;
			val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
			id_present = (val & USB_ID_STATUS) ? false : true;
			if (id_present)
				pmc->pmc_ops->setup_pmc_wake_detect(pmc);
		} else {
			pmc->pmc_ops->setup_pmc_wake_detect(pmc);
		}

		val = readl(base + UTMIP_PMC_WAKEUP0);
		val |= EVENT_INT_ENB;
		writel(val, base + UTMIP_PMC_WAKEUP0);
		PHY_DBG("%s ENABLE_PMC inst = %d\n", __func__, phy->inst);

		val = readl(base + USB_SUSP_CTRL);
		val &= ~USB_WAKE_ON_CNNT_EN_DEV;
		writel(val, base + USB_SUSP_CTRL);
	}

	if (!phy->hot_plug) {
		val = readl(base + UTMIP_XCVR_CFG0);
		val |= (UTMIP_FORCE_PD_POWERDOWN | UTMIP_FORCE_PD2_POWERDOWN |
			 UTMIP_FORCE_PDZI_POWERDOWN);
		writel(val, base + UTMIP_XCVR_CFG0);
	}

	val = readl(base + UTMIP_XCVR_CFG1);
	val |= UTMIP_FORCE_PDDISC_POWERDOWN | UTMIP_FORCE_PDCHRP_POWERDOWN |
		   UTMIP_FORCE_PDDR_POWERDOWN;
	writel(val, base + UTMIP_XCVR_CFG1);

	val = readl(base + UTMIP_BIAS_CFG1);
	val |= UTMIP_BIAS_PDTRK_COUNT(0x5);
	writel(val, base + UTMIP_BIAS_CFG1);

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

			if (val & USB_PORTSC_CCS) {
				val = readl(base + USB_SUSP_CTRL);
				val &= ~USB_PHY_CLK_VALID_INT_ENB;
			} else {
				val = readl(base + USB_SUSP_CTRL);
				val |= USB_PHY_CLK_VALID_INT_ENB;
			}
			writel(val, base + USB_SUSP_CTRL);
		} else {
			/* Disable PHY clock valid interrupts while going into suspend*/
			val = readl(base + USB_SUSP_CTRL);
			val &= ~USB_PHY_CLK_VALID_INT_ENB;
			writel(val, base + USB_SUSP_CTRL);
		}
	}

	/* Disable PHY clock */
	val = readl(base + HOSTPC1_DEVLC);
	val |= HOSTPC1_DEVLC_PHCD;
	writel(val, base + HOSTPC1_DEVLC);

	if (!phy->hot_plug) {
		val = readl(base + USB_SUSP_CTRL);
		val |= UTMIP_RESET;
		writel(val, base + USB_SUSP_CTRL);
	}

	utmi_phy_pad_disable();
	utmi_phy_iddq_override(true);
	phy->phy_clk_on = false;
	phy->hw_accessible = false;

	PHY_DBG("%s(%d) inst:[%d] END\n", __func__, __LINE__, phy->inst);

	return 0;
}


static int utmi_phy_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	void __iomem *padctl_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
#endif
	struct tegra_utmi_config *config = &phy->pdata->u_cfg.utmi;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	PHY_DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->phy_clk_on) {
		PHY_DBG("%s(%d) inst:[%d] phy clk is already On\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}
	val = readl(base + USB_SUSP_CTRL);
	val |= UTMIP_RESET;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + UTMIP_TX_CFG0);
	val |= UTMIP_FS_PREABMLE_J;
	writel(val, base + UTMIP_TX_CFG0);

	val = readl(base + USB_USBMODE);
	val &= ~USB_USBMODE_MASK;
	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST)
		val |= USB_USBMODE_HOST;
	else
		val |= USB_USBMODE_DEVICE;
	writel(val, base + USB_USBMODE);

	val = readl(base + UTMIP_HSRX_CFG0);
	val &= ~(UTMIP_IDLE_WAIT(~0) | UTMIP_ELASTIC_LIMIT(~0));
	val |= UTMIP_IDLE_WAIT(config->idle_wait_delay);
	val |= UTMIP_ELASTIC_LIMIT(config->elastic_limit);
	writel(val, base + UTMIP_HSRX_CFG0);

	val = readl(base + UTMIP_HSRX_CFG1);
	val &= ~UTMIP_HS_SYNC_START_DLY(~0);
	val |= UTMIP_HS_SYNC_START_DLY(config->hssync_start_delay);
	writel(val, base + UTMIP_HSRX_CFG1);

	if (tegra_platform_is_silicon()) {
		val = readl(base + UTMIP_DEBOUNCE_CFG0);
		val &= ~UTMIP_BIAS_DEBOUNCE_A(~0);
		val |= UTMIP_BIAS_DEBOUNCE_A(phy->freq->debounce);
		writel(val, base + UTMIP_DEBOUNCE_CFG0);
	}

	val = readl(base + UTMIP_MISC_CFG0);
	val &= ~UTMIP_SUSPEND_EXIT_ON_EDGE;
	writel(val, base + UTMIP_MISC_CFG0);

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

	utmi_phy_pad_enable();

	val = readl(base + UTMIP_XCVR_CFG0);
	val &= ~(UTMIP_XCVR_LSBIAS_SEL | UTMIP_FORCE_PD_POWERDOWN |
		 UTMIP_FORCE_PD2_POWERDOWN | UTMIP_FORCE_PDZI_POWERDOWN |
		 UTMIP_XCVR_SETUP(~0) | UTMIP_XCVR_LSFSLEW(~0) |
		 UTMIP_XCVR_LSRSLEW(~0) | UTMIP_XCVR_HSSLEW_MSB(~0));
	val |= UTMIP_XCVR_SETUP(phy->utmi_xcvr_setup);
	val |= UTMIP_XCVR_SETUP_MSB(XCVR_SETUP_MSB_CALIB(phy->utmi_xcvr_setup));
	val |= UTMIP_XCVR_LSFSLEW(config->xcvr_lsfslew);
	val |= UTMIP_XCVR_LSRSLEW(config->xcvr_lsrslew);
	if (!config->xcvr_use_lsb)
		val |= UTMIP_XCVR_HSSLEW_MSB(0x3);
	if (config->xcvr_hsslew_lsb)
		val |= UTMIP_XCVR_HSSLEW_LSB(config->xcvr_hsslew_lsb);
	writel(val, base + UTMIP_XCVR_CFG0);

	val = readl(base + UTMIP_XCVR_CFG1);
	val &= ~(UTMIP_FORCE_PDDISC_POWERDOWN | UTMIP_FORCE_PDCHRP_POWERDOWN |
		 UTMIP_FORCE_PDDR_POWERDOWN | UTMIP_XCVR_TERM_RANGE_ADJ(~0));
	val |= UTMIP_XCVR_TERM_RANGE_ADJ(config->term_range_adj);
	writel(val, base + UTMIP_XCVR_CFG1);

	if (tegra_platform_is_silicon()) {
		val = readl(base + UTMIP_BIAS_CFG1);
		val &= ~UTMIP_BIAS_PDTRK_COUNT(~0);
		val |= UTMIP_BIAS_PDTRK_COUNT(phy->freq->pdtrk_count);
		writel(val, base + UTMIP_BIAS_CFG1);
	}

	val = readl(base + UTMIP_SPARE_CFG0);
	val &= ~FUSE_SETUP_SEL;
	val |= FUSE_ATERM_SEL;
	writel(val, base + UTMIP_SPARE_CFG0);

	val = readl(base + USB_SUSP_CTRL);
	val |= UTMIP_PHY_ENABLE;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + USB_SUSP_CTRL);
	val &= ~UTMIP_RESET;
	writel(val, base + USB_SUSP_CTRL);

	/* Bring UTMIPLL out of IDDQ mode while exiting from reset/suspend */
	utmi_phy_iddq_override(false);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
		USB_PHY_CLK_VALID, USB_PHY_CLK_VALID, 2500))
		pr_warn("%s: timeout waiting for phy to stabilize\n", __func__);

	val = readl(base + HOSTPC1_DEVLC);
	val &= ~HOSTPC1_DEVLC_PHCD;
	writel(val, base + HOSTPC1_DEVLC);

	utmi_phy_set_snps_trking_data();

	if (phy->inst == 2)
		writel(0, base + ICUSB_CTRL);

	val = readl(base + USB_USBMODE);
	val &= ~USB_USBMODE_MASK;
	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST)
		val |= USB_USBMODE_HOST;
	else
		val |= USB_USBMODE_DEVICE;
	writel(val, base + USB_USBMODE);

	val = readl(base + HOSTPC1_DEVLC);
	val &= ~HOSTPC1_DEVLC_PTS(~0);
	val |= HOSTPC1_DEVLC_STS;
	writel(val, base + HOSTPC1_DEVLC);

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		pmc_data[phy->inst].is_xhci = phy->pdata->u_data.dev.is_xhci;
		pmc->pmc_ops->powerup_pmc_wake_detect(pmc);
	}

	if (!readl(base + USB_ASYNCLISTADDR))
		_usb_phy_init(phy);
	val = readl(base + USB_TXFILLTUNING);
	if ((val & USB_FIFO_TXFILL_MASK) !=
		USB_FIFO_TXFILL_THRES(0x10)) {
		val = USB_FIFO_TXFILL_THRES(0x10);
		writel(val, base + USB_TXFILLTUNING);
	}
	phy->phy_clk_on = true;
	phy->hw_accessible = true;

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	val = readl(padctl_base + PADCTL_SNPS_OC_MAP);
	val |= CONTROLLER_OC(phy->inst, 0x4);
	writel(val, padctl_base + PADCTL_SNPS_OC_MAP);

	val = readl(padctl_base + PADCTL_OC_DET);
	if (phy->inst == 0)
		val |= ENABLE0_OC_MAP(config->vbus_oc_map);
	if (phy->inst == 2)
		val |= ENABLE1_OC_MAP(config->vbus_oc_map);
	writel(val, padctl_base + PADCTL_OC_DET);
#endif

	PHY_DBG("%s(%d) End inst:[%d]\n", __func__, __LINE__, phy->inst);
	return 0;
}

static void utmi_phy_restore_start(struct tegra_usb_phy *phy)
{
	unsigned long val;
	int inst = phy->inst;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = tegra_usb_pmc_reg_read(UTMIP_STATUS);
	/* Check whether we wake up from the remote resume.
	   For lp1 case, pmc is not responsible for waking the
	   system, it's the flow controller and hence
	   UTMIP_WALK_PTR_VAL(inst) will return 0.
	   Also, for lp1 case phy->pmc_remote_wakeup will already be set
	   to true by utmi_phy_irq() when the remote wakeup happens.
	   Hence change the logic in the else part to enter only
	   if phy->pmc_remote_wakeup is not set to true by the
	   utmi_phy_irq(). */
	if (UTMIP_WALK_PTR_VAL(inst) & val) {
		phy->pmc_remote_wakeup = true;
	}
}

static void utmi_phy_restore_end(struct tegra_usb_phy *phy)
{
	unsigned long val, flags = 0;
	void __iomem *base = phy->regs;
	int wait_time_us = 25000; /* FPR should be set by this time */
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	/* check whether we wake up from the remote resume */
	if (phy->pmc_remote_wakeup) {
		/* wait until SUSPEND and RESUME bit
		 * is cleared on remote resume */
		do {
			val = readl(base + USB_PORTSC);
			udelay(1);
			if (wait_time_us == 0) {
				PHY_DBG("%s PMC FPR" \
				"timeout val = 0x%x instance = %d\n", \
				__func__, (u32)val, phy->inst);
				pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
				phy->pmc_remote_wakeup = false;
				phy->pmc_hotplug_wakeup = false;
				return;
			}
			wait_time_us--;
		} while (val & (USB_PORTSC_RESUME | USB_PORTSC_SUSP));

		/* Add delay sothat resume will be driven for more than 20 ms */
		mdelay(10);
		local_irq_save(flags);
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 1);
		phy->pmc_remote_wakeup = false;
		phy->pmc_hotplug_wakeup = false;
		local_irq_restore(flags);

		PHY_DBG("%s DISABLE_PMC inst = %d\n", __func__, phy->inst);

		if (usb_phy_reg_status_wait(base + USB_USBCMD, USB_USBCMD_RS,
							 USB_USBCMD_RS, 2000)) {
			pr_err("%s: timeout waiting for USB_USBCMD_RS\n",\
			__func__);
		}

		/* Clear PCI and SRI bits to avoid an interrupt upon resume */
		val = readl(base + USB_USBSTS);
		writel(val, base + USB_USBSTS);
		/* wait to avoid SOF if there is any */
		if (usb_phy_reg_status_wait(base + USB_USBSTS,
			USB_USBSTS_SRI, USB_USBSTS_SRI, 2500) < 0) {
			pr_err("%s: timeout waiting for SOF\n", __func__);
		}
	} else {
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
		phy->pmc_remote_wakeup = false;
		phy->pmc_hotplug_wakeup = false;
		PHY_DBG("%s DISABLE_PMC inst = %d\n", __func__, phy->inst);
	}
}

static int utmi_phy_resume(struct tegra_usb_phy *phy)
{
	int status = 0;
	unsigned long val;
	int port_connected = 0;
	int is_lp0;
	void __iomem *base = phy->regs;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST) {
		if (readl(base + USB_ASYNCLISTADDR) &&
			!phy->pdata->u_data.host.power_off_on_suspend)
			return 0;

		val = readl(base + USB_PORTSC);
		port_connected = val & USB_PORTSC_CCS;
		is_lp0 = !(readl(base + USB_ASYNCLISTADDR));

		if ((phy->port_speed < USB_PHY_PORT_SPEED_UNKNOWN) &&
			(port_connected ^ is_lp0)) {
			utmi_phy_restore_start(phy);
			usb_phy_bringup_host_controller(phy);
			utmi_phy_restore_end(phy);
		} else {
			pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
			phy->pmc_remote_wakeup = false;
			phy->pmc_hotplug_wakeup = false;

			/* bring up the controller from suspend*/
			val = readl(base + USB_USBCMD);
			val |= USB_CMD_RESET;
			writel(val, base + USB_USBCMD);

			if (usb_phy_reg_status_wait(base + USB_USBCMD,
				USB_CMD_RESET, 0, 2500) < 0) {
				pr_err("%s: timeout waiting for reset\n", __func__);
			}

			val = readl(base + USB_USBMODE);
			val &= ~USB_USBMODE_MASK;
			val |= USB_USBMODE_HOST;
			writel(val, base + USB_USBMODE);

			val = readl(base + HOSTPC1_DEVLC);
			val &= ~HOSTPC1_DEVLC_PTS(~0);
			val |= HOSTPC1_DEVLC_STS;
			writel(val, base + HOSTPC1_DEVLC);

			writel(USB_USBCMD_RS, base + USB_USBCMD);

			if (usb_phy_reg_status_wait(base + USB_USBCMD,
				USB_USBCMD_RS, USB_USBCMD_RS, 2500) < 0) {
				pr_err("%s: timeout waiting for run bit\n", __func__);
			}

			/* Enable Port Power */
			val = readl(base + USB_PORTSC);
			val |= USB_PORTSC_PP;
			writel(val, base + USB_PORTSC);
			udelay(10);

			DBG("USB_USBSTS[0x%x] USB_PORTSC[0x%x]\n",
			readl(base + USB_USBSTS), readl(base + USB_PORTSC));
		}
		val = readl(base + USB_TXFILLTUNING);
		if ((val & USB_FIFO_TXFILL_MASK) !=
			USB_FIFO_TXFILL_THRES(0x10)) {
			val = USB_FIFO_TXFILL_THRES(0x10);
				writel(val, base + USB_TXFILLTUNING);
		}
	}

	return status;
}

static unsigned long utmi_phy_set_dp_dm_pull_up_down(struct tegra_usb_phy *phy,
		unsigned long pull_up_down_flags)
{
	unsigned long val;
	unsigned long org;
	void __iomem *base = phy->regs;

	org = readl(base + UTMIP_MISC_CFG0);

	val = org & ~MASK_ALL_PULLUP_PULLDOWN;
	val |= pull_up_down_flags;

	writel(val, base + UTMIP_MISC_CFG0);

	usleep_range(500, 2000);
	return org;
}

unsigned long utmi_phy_get_dp_dm_status(struct tegra_usb_phy *phy,
		unsigned long pull_up_down_flags)
{
	void __iomem *base = phy->regs;
	unsigned long org_flags;
	unsigned long ret;

	org_flags = utmi_phy_set_dp_dm_pull_up_down(phy, pull_up_down_flags);
	ret = USB_PORTSC_LINE_STATE(readl(base + USB_PORTSC));
	utmi_phy_set_dp_dm_pull_up_down(phy, org_flags);
	return ret;
}

static void disable_charger_detection(void __iomem *base)
{
	unsigned long val;

	/* Disable charger detection logic */
	val = readl(base + UTMIP_BAT_CHRG_CFG0);
	val &= ~(UTMIP_OP_SRC_EN | UTMIP_ON_SINK_EN);
	writel(val, base + UTMIP_BAT_CHRG_CFG0);

	/* Delay of 40 ms before we pull the D+ as per battery charger spec */
	msleep(TDPSRC_CON_MS);
}

/*
 * Per Battery Charging Specification 1.2  section 3.2.3:
 * We check Data Contact Detect (DCD) before we check the USB cable type.
 */
static bool utmi_phy_dcd_detect(struct tegra_usb_phy *phy)
{
	void __iomem *base = phy->regs;
	unsigned long val;
	unsigned long org_flags;
	bool ret;

	val = readl(base + UTMIP_BAT_CHRG_CFG0);
	/* inject IDP_SRC */
	val |= UTMIP_OP_I_SRC_EN;
	writel(val, base + UTMIP_BAT_CHRG_CFG0);
	/* enable pull down resistor RDM_DWN on D- */
	org_flags = utmi_phy_set_dp_dm_pull_up_down(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | FORCE_PULLDN_DM);
	usleep_range(20000, 30000);

	ret = false;
	if (0 == USB_PORTSC_LINE_STATE(readl(base + USB_PORTSC))) {
		/* minimum debounce time is 10mS per TDCD_DBNC */
		usleep_range(10000, 12000);
		if (0 == USB_PORTSC_LINE_STATE(readl(base + USB_PORTSC)))
			ret = true;
	}

	val &= ~UTMIP_OP_I_SRC_EN;
	writel(val, base + UTMIP_BAT_CHRG_CFG0);
	utmi_phy_set_dp_dm_pull_up_down(phy, org_flags);
	return ret;
}

static bool utmi_phy_charger_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	bool status;
	int dcd_timeout_ms;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->pdata->op_mode != TEGRA_USB_OPMODE_DEVICE) {
		/* Charger detection is not there for ULPI
		 * return Charger not available */
		return false;
	}

	/* ensure we start from an initial state */
	writel(0, base + UTMIP_BAT_CHRG_CFG0);
	utmi_phy_set_dp_dm_pull_up_down(phy, 0);

	/* log initial values */
	DBG("%s(%d) inst:[%d], UTMIP_BAT_CHRG_CFG0 = %08X\n",
		__func__, __LINE__,
		phy->inst, readl(base + UTMIP_BAT_CHRG_CFG0));
	DBG("%s(%d) inst:[%d], UTMIP_MISC_CFG0 = %08X\n",
		__func__, __LINE__,
		phy->inst, readl(base + UTMIP_MISC_CFG0));

	/* DCD timeout max value is 900mS */
	dcd_timeout_ms = 0;
	while (dcd_timeout_ms < 900) {
		/* standard DCD detect for SDP/DCP/CDP */
		if (utmi_phy_dcd_detect(phy))
			break;
		/* for NV-charger, we wait D+/D- both set */
		if ((USB_PORTSC_LINE_DP_SET | USB_PORTSC_LINE_DM_SET) ==
			utmi_phy_get_dp_dm_status(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM))
			break;
		usleep_range(20000, 22000);
		dcd_timeout_ms += 22;
	}

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
	else {
		status = false;
		disable_charger_detection(base);
	}
	DBG("%s(%d) inst:[%d] DONE Status = %d\n",
		__func__, __LINE__, phy->inst, status);
	return status;
}

static bool utmi_phy_qc2_charger_detect(struct tegra_usb_phy *phy,
		int max_voltage)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	int status;
	int qc2_timeout_ms;
	int vbus_stat = 0;
#ifdef QC_MEASURE_VOLTAGES
	int timeout;
#endif

	DBG("%s(%d) inst:[%d] max_voltage = %d\n",
		__func__, __LINE__, phy->inst, max_voltage);

	status = false;

	/* no need to detect qc2 if operating at 5V */
	switch (max_voltage) {
	case TEGRA_USB_QC2_9V:
	case TEGRA_USB_QC2_12V:
	case TEGRA_USB_QC2_20V:
		break;

	case TEGRA_USB_QC2_5V:
	default:
		disable_charger_detection(base);
		return status;
	}

	/* if vbus drops we are connected to quick charge 2 */
	qc2_timeout_ms = 0;
	while (qc2_timeout_ms < 2000) {
		usleep_range(1000, 1200);
		qc2_timeout_ms += 1;
		val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
		if (!(val & VDAT_DET_STS)) {
			vbus_stat = 1;
			break;
		}
	}

	if (vbus_stat) {
		unsigned long org_flags;

		status = true;

		DBG("%s(%d) inst:[%d], QC2 DETECTED !!!",
			__func__, __LINE__, phy->inst);

		switch (max_voltage) {
		case TEGRA_USB_QC2_9V:
			/* Set the input voltage to 9V */
			/* D+ 3.3v -- D- 0.6v */
			DBG("%s(%d) inst:[%d], QC 9V D+ 3.3v D- 0.6v",
				__func__, __LINE__, phy->inst);
			org_flags = utmi_phy_set_dp_dm_pull_up_down(phy,
				FORCE_PULLUP_DP   | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);
			val = readl(base + UTMIP_BAT_CHRG_CFG0);
			val &= ~(UTMIP_OP_SINK_EN | UTMIP_ON_SINK_EN);
			val |= UTMIP_ON_SRC_EN;
			val &= ~UTMIP_OP_SRC_EN;
			writel(val, base + UTMIP_BAT_CHRG_CFG0);
			break;

		case TEGRA_USB_QC2_12V:
			/* 0.6v v on D+ and D- */
			DBG("%s(%d) inst:[%d], QC 12V D+ 0.6v D- 0.6v",
				__func__, __LINE__, phy->inst);
			org_flags = utmi_phy_set_dp_dm_pull_up_down(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);
			val = readl(base + UTMIP_BAT_CHRG_CFG0);
			val &= ~(UTMIP_OP_SINK_EN | UTMIP_ON_SINK_EN);
			val |= (UTMIP_OP_SRC_EN | UTMIP_ON_SRC_EN);
			writel(val, base + UTMIP_BAT_CHRG_CFG0);
			break;

		case TEGRA_USB_QC2_20V:
			/* 3.3 v on D+ and D- */
			DBG("%s(%d) inst:[%d], QC 20V D+ 3.3v D- 3.3v",
				__func__, __LINE__, phy->inst);
			org_flags = utmi_phy_set_dp_dm_pull_up_down(phy,
				FORCE_PULLUP_DP   | FORCE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);
			val = readl(base + UTMIP_BAT_CHRG_CFG0);
			val &= ~(UTMIP_OP_SINK_EN | UTMIP_ON_SINK_EN);
			val &= ~(UTMIP_OP_SRC_EN | UTMIP_ON_SRC_EN);
			writel(val, base + UTMIP_BAT_CHRG_CFG0);
			break;

		case TEGRA_USB_QC2_5V:
		default:
			DBG("%s(%d) inst:[%d], QC 5V D+ 0.6v D- GND",
				__func__, __LINE__, phy->inst);
			org_flags = utmi_phy_set_dp_dm_pull_up_down(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | FORCE_PULLDN_DM);
			val = readl(base + UTMIP_BAT_CHRG_CFG0);
			val &= ~(UTMIP_OP_SINK_EN | UTMIP_ON_SINK_EN);
			val |= UTMIP_OP_SRC_EN;
			val &= ~UTMIP_ON_SRC_EN;
			writel(val, base + UTMIP_BAT_CHRG_CFG0);
			break;
		}

#ifdef QC_MEASURE_VOLTAGES
		timeout = 60;
		DBG("%s(%d) MEASURE VOLTAGES -- you have %d seconds",
			__func__, __LINE__, timeout);
		while (timeout-- > 0) {
			DBG("%s(%d) COUNT = %d",
				__func__, __LINE__, timeout);
			ssleep(1);
		}
#endif
	}


	DBG("%s(%d) inst:[%d], UTMIP_BAT_CHRG_CFG0 = %08X\n",
		__func__, __LINE__,
		phy->inst, readl(base + UTMIP_BAT_CHRG_CFG0));
	DBG("%s(%d) inst:[%d], UTMIP_MISC_CFG0 = %08X\n",
		__func__, __LINE__,
		phy->inst, readl(base + UTMIP_MISC_CFG0));

	disable_charger_detection(base);
	DBG("%s(%d) inst:[%d] DONE Status = %d\n",
		__func__, __LINE__, phy->inst, status);
	return status;
}

static bool utmi_phy_is_non_std_charger(struct tegra_usb_phy *phy)
{
	/*
	 * non std charger has D+/D- line float, we can apply pull up/down on
	 * each line and verify if line status change.
	 */
	/* pull up DP only */
	if (USB_PORTSC_LINE_DP_SET != utmi_phy_get_dp_dm_status(phy,
				FORCE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM))
		goto NOT_NON_STD_CHARGER;

	/* pull down DP only */
	if (0x0 != utmi_phy_get_dp_dm_status(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				FORCE_PULLDN_DP | DISABLE_PULLDN_DM))
		goto NOT_NON_STD_CHARGER;

	/* pull up DM only */
	if (USB_PORTSC_LINE_DM_SET != utmi_phy_get_dp_dm_status(phy,
				DISABLE_PULLUP_DP | FORCE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM))
		goto NOT_NON_STD_CHARGER;

	/* pull down DM only */
	if (0x0 != utmi_phy_get_dp_dm_status(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | FORCE_PULLDN_DM))
		goto NOT_NON_STD_CHARGER;

	utmi_phy_set_dp_dm_pull_up_down(phy, 0);
	return true;

NOT_NON_STD_CHARGER:
	utmi_phy_set_dp_dm_pull_up_down(phy, 0);
	return false;

}

static void utmi_phy_pmc_disable(struct tegra_usb_phy *phy)
{
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];
	if (phy->pdata->u_data.host.turn_off_vbus_on_lp0 &&
					phy->pdata->port_otg) {
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
		pmc->pmc_ops->powerdown_pmc_wake_detect(pmc);
	}
}
static bool utmi_phy_nv_charger_detect(struct tegra_usb_phy *phy)
{
	int status1;
	int status2;
	int status3;
	bool ret;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (utmi_phy_is_non_std_charger(phy))
		return false;

	ret = false;
	/* Turn off all terminations except DP pulldown */
	status1 = utmi_phy_get_dp_dm_status(phy,
			DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
			FORCE_PULLDN_DP | DISABLE_PULLDN_DM);

	/* Turn off all terminations except for DP pullup */
	status2 = utmi_phy_get_dp_dm_status(phy,
			FORCE_PULLUP_DP | DISABLE_PULLUP_DM |
			DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);

	/* Check for NV charger DISABLE all terminations */
	status3 = utmi_phy_get_dp_dm_status(phy,
			DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
			DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);

	if ((status1 == (USB_PORTSC_LINE_DP_SET | USB_PORTSC_LINE_DM_SET)) &&
	    (status2 == (USB_PORTSC_LINE_DP_SET | USB_PORTSC_LINE_DM_SET)) &&
	    (status3 == (USB_PORTSC_LINE_DP_SET | USB_PORTSC_LINE_DM_SET)))
		ret = true;

	/* Restore standard termination by hardware. */
	utmi_phy_set_dp_dm_pull_up_down(phy, 0);
	return ret;
}

static bool utmi_phy_apple_charger_1000ma_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	bool ret;
	void __iomem *base = phy->regs;
	unsigned long org_flags;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	ret = false;

	org_flags = utmi_phy_set_dp_dm_pull_up_down(phy,
				FORCE_PULLUP_DP | FORCE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);
	usleep_range(20000, 30000);
	utmi_phy_set_dp_dm_pull_up_down(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);

	val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
	val |= DIV_DET_EN;
	writel(val, base + USB_PHY_VBUS_WAKEUP_ID);

	usleep_range(10000, 20000);

	val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
	val &= ~DIV_DET_EN;
	writel(val, base + USB_PHY_VBUS_WAKEUP_ID);

	if ((val & VOP_DIV2P0_DET) && (val & VON_DIV2P7_DET))
		ret = true;

	utmi_phy_set_dp_dm_pull_up_down(phy, org_flags);

	return ret;
}

static bool utmi_phy_apple_charger_2000ma_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	bool ret;
	void __iomem *base = phy->regs;
	unsigned long org_flags;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	ret = false;

	org_flags = utmi_phy_set_dp_dm_pull_up_down(phy,
				FORCE_PULLUP_DP | FORCE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);
	usleep_range(20000, 30000);
	utmi_phy_set_dp_dm_pull_up_down(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);

	val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
	val |= DIV_DET_EN;
	writel(val, base + USB_PHY_VBUS_WAKEUP_ID);

	usleep_range(10000, 20000);

	val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
	val &= ~DIV_DET_EN;
	writel(val, base + USB_PHY_VBUS_WAKEUP_ID);

	if ((val & VOP_DIV2P7_DET) && (val & VON_DIV2P0_DET))
		ret = true;

	utmi_phy_set_dp_dm_pull_up_down(phy, org_flags);

	return ret;
}

static bool utmi_phy_apple_charger_500ma_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	bool ret;
	void __iomem *base = phy->regs;
	unsigned long org_flags;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	ret = false;

	org_flags = utmi_phy_set_dp_dm_pull_up_down(phy,
				FORCE_PULLUP_DP | FORCE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);
	usleep_range(20000, 30000);
	utmi_phy_set_dp_dm_pull_up_down(phy,
				DISABLE_PULLUP_DP | DISABLE_PULLUP_DM |
				DISABLE_PULLDN_DP | DISABLE_PULLDN_DM);

	val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
	val |= DIV_DET_EN;
	writel(val, base + USB_PHY_VBUS_WAKEUP_ID);

	usleep_range(10000, 20000);

	val = readl(base + USB_PHY_VBUS_WAKEUP_ID);
	val &= ~DIV_DET_EN;
	writel(val, base + USB_PHY_VBUS_WAKEUP_ID);

	if ((val & VOP_DIV2P0_DET) && (val & VON_DIV2P0_DET))
		ret = true;

	utmi_phy_set_dp_dm_pull_up_down(phy, org_flags);

	return ret;
}

static bool uhsic_phy_remotewake_detected(struct tegra_usb_phy *phy)
{
	void __iomem *base = phy->regs;
	u32 val;
	unsigned int inst = phy->inst;

	val = readl(base + UHSIC_PMC_WAKEUP0);
	if (!(val & EVENT_INT_ENB))
		return false;
	val = tegra_usb_pmc_reg_read(UHSIC_STATUS(inst));
	if (!(UHSIC_WAKE_ALARM(inst) & val))
		return false;
	tegra_usb_pmc_reg_update(PMC_UHSIC_SLEEP_CFG(inst),
		UHSIC_WAKE_VAL(inst, WAKE_VAL_ANY),
		UHSIC_WAKE_VAL(inst, WAKE_VAL_NONE));

	tegra_usb_pmc_reg_update(PMC_UHSIC_TRIGGERS(inst),
		0, UHSIC_CLR_WAKE_ALARM(inst));

	val = readl(base + UHSIC_PMC_WAKEUP0);
	val &= ~EVENT_INT_ENB;
	writel(val, base + UHSIC_PMC_WAKEUP0);
	phy->pmc_remote_wakeup = true;
	DBG("%s:PMC interrupt detected for HSIC\n", __func__);
	return true;
}

static int uhsic_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	if (remote_wakeup) {
		/* Set RUN bit */
		val = readl(base + USB_USBCMD);
		val |= USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);
	}

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	return 0;
}

static void uhsic_phy_restore_start(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	unsigned int inst = phy->inst;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	val = tegra_usb_pmc_reg_read(UHSIC_STATUS(inst));

	/* check whether we wake up from the remote resume */
	if (UHSIC_WALK_PTR_VAL(inst) & val) {
		phy->pmc_remote_wakeup = true;
		DBG("%s: uhsic remote wakeup detected\n", __func__);
	} else {
		if (!((UHSIC_STROBE_VAL(inst) | UHSIC_DATA_VAL(inst)) & val)) {

			/*
			 * If pmc wakeup is detected after putting controller
			 * in suspend in usb_phy_bringup_host_cotroller,
			 * restart bringing up host controller as
			 * in case of only pmc wakeup.
			 */
			if (phy->pmc_remote_wakeup && phy->ctrlr_suspended) {
				usb_phy_bringup_host_controller(phy);
				if (usb_phy_reg_status_wait(base + USB_PORTSC,
					(USB_PORTSC_RESUME | USB_PORTSC_SUSP),
						0, FPR_WAIT_TIME_US) < 0)
					pr_err("%s: timeout waiting" \
					"for SUSPEND to clear\n",
						__func__);
				phy->ctrlr_suspended = false;
			}

			pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
			phy->pmc_remote_wakeup = false;
		} else {
			DBG("%s(%d): setting pretend connect\n", __func__, __LINE__);
			val = readl(base + UHSIC_CMD_CFG0);
			val |= UHSIC_PRETEND_CONNECT_DETECT;
			writel(val, base + UHSIC_CMD_CFG0);
		}
	}
}

static void uhsic_phy_restore_end(struct tegra_usb_phy *phy)
{

	unsigned long val, flags = 0;
	void __iomem *base = phy->regs;
	int wait_time_us = FPR_WAIT_TIME_US; /* FPR should be set by this time */
	bool irq_disabled = false;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];
	bool remote_wakeup_detected;

	DBG("%s(%d)\n", __func__, __LINE__);

	remote_wakeup_detected = phy->pmc_remote_wakeup;
	/*
	 * check whether we wake up from the remote wake detected before putting
	 * controller in suspend in usb_phy_bringup_host_controller.
	 */
	if (!phy->ctrlr_suspended) {
		/* wait until FPR bit is set automatically on remote resume */
		do {
			val = readl(base + USB_PORTSC);
			udelay(1);
			if (wait_time_us == 0) {
			/*
			 * If pmc wakeup is detected after putting controller
			 * in suspend in usb_phy_bringup_host_cotroller,
			 * restart bringing up host controller as
			 * in case of only pmc wakeup.
			 */
			if (phy->pmc_remote_wakeup && phy->ctrlr_suspended) {
				usb_phy_bringup_host_controller(phy);
				if (usb_phy_reg_status_wait(base + USB_PORTSC,
					(USB_PORTSC_RESUME | USB_PORTSC_SUSP),
						0, FPR_WAIT_TIME_US) < 0)
					pr_err("%s: timeout waiting" \
						" for SUSPEND to clear\n",
						__func__);
				phy->ctrlr_suspended = false;
			}

				pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 0);
				phy->pmc_remote_wakeup = false;
				return;
			}
			wait_time_us--;
		} while (val & (USB_PORTSC_RESUME | USB_PORTSC_SUSP));
		/* In case of remote wakeup, disable local irq to prevent
		 * context switch b/t disable PMC and set RUN bit ops */
		local_irq_save(flags);
		irq_disabled = true;
	}

	/*
	 * If pmc wakeup is detected after putting controller in suspend
	 * in usb_phy_bringup_host_cotroller, restart bringing up host
	 * controller as in case of only pmc wakeup.
	 */
	if (phy->pmc_remote_wakeup && phy->ctrlr_suspended) {
		usb_phy_bringup_host_controller(phy);
		if (usb_phy_reg_status_wait(base + USB_PORTSC,
			(USB_PORTSC_RESUME | USB_PORTSC_SUSP), 0,
				FPR_WAIT_TIME_US) < 0)
			pr_err("%s: timeout waiting for SUSPEND to clear\n",
				__func__);
		phy->ctrlr_suspended = false;
	}

	pmc->pmc_ops->disable_pmc_bus_ctrl(pmc, 1);
	phy->pmc_remote_wakeup = false;

	/* Restore local irq if disabled before */
	if (irq_disabled)
		local_irq_restore(flags);
	if (usb_phy_reg_status_wait(base + USB_USBCMD, USB_USBCMD_RS,
						 USB_USBCMD_RS, 2000)) {
		pr_err("%s: timeout waiting for USB_USBCMD_RS\n", __func__);
		return;
	}
	if (remote_wakeup_detected && phy->pdata->ops &&
					phy->pdata->ops->post_remote_wakeup)
		phy->pdata->ops->post_remote_wakeup();
}

static int uhsic_rail_enable(struct tegra_usb_phy *phy)
{
	int ret;

	if (phy->hsic_reg == NULL) {
		phy->hsic_reg = regulator_get(&phy->pdev->dev, "vddio_hsic");
		if (IS_ERR_OR_NULL(phy->hsic_reg)) {
			pr_err("UHSIC: Could not get regulator vddio_hsic\n");
			ret = PTR_ERR(phy->hsic_reg);
			phy->hsic_reg = NULL;
			return ret;
		}
	}

	ret = regulator_enable(phy->hsic_reg);
	if (ret < 0) {
		pr_err("%s vddio_hsic could not be enabled\n", __func__);
		return ret;
	}

	return 0;
}

static int uhsic_rail_disable(struct tegra_usb_phy *phy)
{
	int ret;

	if (phy->hsic_reg == NULL) {
		pr_warn("%s: unbalanced disable\n", __func__);
		return -EIO;
	}

	ret = regulator_disable(phy->hsic_reg);
	if (ret < 0) {
		pr_err("HSIC regulator vddio_hsic cannot be disabled\n");
		return ret;
	}
	regulator_put(phy->hsic_reg);
	phy->hsic_reg = NULL;
	return 0;
}

static int uhsic_phy_open(struct tegra_usb_phy *phy)
{
	unsigned long parent_rate;
	int i;
	int ret;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	phy->hsic_reg = NULL;
	ret = uhsic_rail_enable(phy);
	if (ret < 0) {
		pr_err("%s vddio_hsic could not be enabled\n", __func__);
		return ret;
	}

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
	/* reset controller for reenumerating hsic device */
	tegra_periph_reset_assert(phy->ctrlr_clk);
	udelay(2);
	tegra_periph_reset_deassert(phy->ctrlr_clk);
	udelay(2);

	pmc_init(phy);
	pmc->pmc_ops->powerup_pmc_wake_detect(pmc);

	return 0;
}

static void uhsic_phy_close(struct tegra_usb_phy *phy)
{
	int ret;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	pmc->pmc_ops->powerdown_pmc_wake_detect(pmc);

	ret = uhsic_rail_disable(phy);
	if (ret < 0)
		pr_err("%s vddio_hsic could not be disabled\n", __func__);
}

static int uhsic_phy_irq(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	int irq_status = IRQ_HANDLED;

	/* check if there is any remote wake event */
	if (!phy->pdata->unaligned_dma_buf_supported)
		usb_phy_fence_read(phy);
	if (uhsic_phy_remotewake_detected(phy))
		DBG("%s: uhsic remote wake detected\n", __func__);

	val = readl(base + USB_SUSP_CTRL);
	if ((val  & USB_PHY_CLK_VALID_INT_STS) &&
		(val  & USB_PHY_CLK_VALID_INT_ENB)) {
		val &= ~USB_PHY_CLK_VALID_INT_ENB |
				USB_PHY_CLK_VALID_INT_STS;
		writel(val , (base + USB_SUSP_CTRL));

		val = readl(base + USB_USBSTS);
		if (!(val  & USB_USBSTS_PCI)) {
			irq_status = IRQ_NONE;
			goto exit;
		}

		val = readl(base + USB_PORTSC);
		if (val & USB_PORTSC_CCS)
			val &= ~USB_PORTSC_WKCN;
		val &= ~USB_PORTSC_RWC_BITS;
		writel(val , (base + USB_PORTSC));
		irq_status = IRQ_HANDLED;
	}
exit:
	return irq_status;
}

static int uhsic_phy_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	void __iomem *padctl_base = IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE);
#endif

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already On\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	val = readl(padctl_base + PADCTL_SNPS_OC_MAP);
	val |= CONTROLLER_OC(phy->inst, 0x7);
	writel(val, padctl_base + PADCTL_SNPS_OC_MAP);
#endif

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~(UHSIC_PD_BG | UHSIC_PD_RX |
			UHSIC_PD_ZI | UHSIC_RPD_DATA | UHSIC_RPD_STROBE);
	writel(val, base + UHSIC_PADS_CFG1);

	val |= (UHSIC_RX_SEL | UHSIC_PD_TX);
	val |= UHSIC_AUTO_RTERM_EN;
	writel(val, base + UHSIC_PADS_CFG1);

	val = readl(base + USB_SUSP_CTRL);
	val |= UHSIC_RESET;
	writel(val, base + USB_SUSP_CTRL);
	udelay(1);

	val = readl(base + USB_SUSP_CTRL);
	val |= UHSIC_PHY_ENABLE;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + UHSIC_HSRX_CFG0);
	val |= UHSIC_IDLE_WAIT(HSIC_IDLE_WAIT_DELAY);
	val |= UHSIC_ELASTIC_UNDERRUN_LIMIT(HSIC_ELASTIC_UNDERRUN_LIMIT);
	val |= UHSIC_ELASTIC_OVERRUN_LIMIT(HSIC_ELASTIC_OVERRUN_LIMIT);
	writel(val, base + UHSIC_HSRX_CFG0);

	val = readl(base + UHSIC_HSRX_CFG1);
	val |= UHSIC_HS_SYNC_START_DLY(HSIC_SYNC_START_DELAY);
	writel(val, base + UHSIC_HSRX_CFG1);

	/* WAR HSIC TX */
	val = readl(base + UHSIC_TX_CFG0);
	val &= ~UHSIC_HS_READY_WAIT_FOR_VALID;
	writel(val, base + UHSIC_TX_CFG0);

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
	udelay(1);

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~(UHSIC_PD_TX);
	writel(val, base + UHSIC_PADS_CFG1);

	/* HSIC pad tracking circuit power down sequence */
	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~(UHSIC_PD_TRK);
	writel(val, base + UHSIC_PADS_CFG1);
	/* Wait for 25usec */
	udelay(25);
	val |= UHSIC_PD_TRK;
	writel(val, base + UHSIC_PADS_CFG1);

	/* Enable bus keepers always */
	val = readl(base + UHSIC_SPARE_CFG0);
	val |= FORCE_BK_ON;
	writel(val, base + UHSIC_SPARE_CFG0);

	/*SUSP_CTRL has to be toggled to enable host PHY clock */
	val = readl(base + USB_SUSP_CTRL);
	val |= USB_SUSP_CLR;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + USB_SUSP_CTRL);
	val &= ~USB_SUSP_CLR;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + USB_USBMODE);
	val |= USB_USBMODE_HOST;
	writel(val, base + USB_USBMODE);

	/* Change the USB controller PHY type to HSIC */
	val = readl(base + HOSTPC1_DEVLC);
	val &= ~HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_MASK);
	val |= HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_HSIC);
	val &= ~HOSTPC1_DEVLC_PSPD(HOSTPC1_DEVLC_PSPD_MASK);
	val |= HOSTPC1_DEVLC_PSPD(HOSTPC1_DEVLC_PSPD_HIGH_SPEED);
	val &= ~HOSTPC1_DEVLC_STS;
	writel(val, base + HOSTPC1_DEVLC);

	val = readl(base + USB_PORTSC);
	val &= ~(USB_PORTSC_WKOC | USB_PORTSC_WKDS);
	writel(val, base + USB_PORTSC);

	val = readl(base + UHSIC_TX_CFG0);
	val |= UHSIC_HS_POSTAMBLE_OUTPUT_ENABLE;
	writel(val, base + UHSIC_TX_CFG0);

	val = readl(base + UHSIC_PADS_CFG0);
	/* Clear RTUNEP, SLEWP & SLEWN bit fields */
	val &= ~(UHSIC_TX_RTUNEP | UHSIC_TX_SLEWP | UHSIC_TX_SLEWN);
	/* set Rtune impedance to 50 ohm */
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
	val |= UHSIC_TX_RTUNE_P(0xA);
#else
	val |= UHSIC_TX_RTUNE_P(0xC);
#endif
	writel(val, base + UHSIC_PADS_CFG0);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
				USB_PHY_CLK_VALID, USB_PHY_CLK_VALID, 2500)) {
		pr_err("%s: timeout waiting for phy to stabilize\n", __func__);
		return -ETIMEDOUT;
	}

	phy->phy_clk_on = true;
	phy->hw_accessible = true;

	if (!readl(base + USB_ASYNCLISTADDR))
		_usb_phy_init(phy);

	if (phy->pmc_sleepwalk) {
		DBG("%s(%d) inst:[%d] restore phy\n", __func__, __LINE__,
					phy->inst);
		uhsic_phy_restore_start(phy);
		usb_phy_bringup_host_controller(phy);
		uhsic_phy_restore_end(phy);
		phy->pmc_sleepwalk = false;
	}

	val = readl(base + USB_TXFILLTUNING);
	if ((val & USB_FIFO_TXFILL_MASK) != USB_FIFO_TXFILL_THRES(0x10)) {
		val = USB_FIFO_TXFILL_THRES(0x10);
		writel(val, base + USB_TXFILLTUNING);
	}

	return 0;
}

static int uhsic_phy_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	bool port_connected;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (!phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already off\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}

	/* Disable interrupts */
	writel(0, base + USB_USBINTR);

	/* check for port connect status */
	val = readl(base + USB_PORTSC);
	port_connected = val & USB_PORTSC_CCS;

	if (phy->pmc_sleepwalk == false && port_connected) {
		pmc->pmc_ops->setup_pmc_wake_detect(pmc);

		phy->pmc_remote_wakeup = false;
		phy->pmc_sleepwalk = true;

		val = readl(base + UHSIC_PMC_WAKEUP0);
		val |= EVENT_INT_ENB;
		writel(val, base + UHSIC_PMC_WAKEUP0);
	} else {
		phy->pmc_sleepwalk = true;
	}

	val = readl(base + HOSTPC1_DEVLC);
	val |= HOSTPC1_DEVLC_PHCD;
	writel(val, base + HOSTPC1_DEVLC);

	/* Remove power downs for HSIC from PADS CFG1 register */
	val = readl(base + UHSIC_PADS_CFG1);
	val |= (UHSIC_PD_BG | UHSIC_PD_TRK |
			UHSIC_PD_ZI | UHSIC_PD_TX);
	writel(val, base + UHSIC_PADS_CFG1);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
		USB_PHY_CLK_VALID, 0, 2500))
		pr_warn("%s: timeout waiting for phy to disable\n", __func__);

	DBG("%s(%d) inst:[%d] End\n", __func__, __LINE__, phy->inst);

	phy->phy_clk_on = false;
	phy->hw_accessible = false;

	return 0;
}

static int uhsic_phy_bus_port_power(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	val = readl(base + USB_USBMODE);
	val |= USB_USBMODE_HOST;
	writel(val, base + USB_USBMODE);

	/* Change the USB controller PHY type to HSIC */
	val = readl(base + HOSTPC1_DEVLC);
	val &= ~(HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_MASK));
	val |= HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_HSIC);
	val &= ~(HOSTPC1_DEVLC_PSPD(HOSTPC1_DEVLC_PSPD_MASK));
	val |= HOSTPC1_DEVLC_PSPD(HOSTPC1_DEVLC_PSPD_HIGH_SPEED);
	writel(val, base + HOSTPC1_DEVLC);

	val = readl(base + UHSIC_MISC_CFG0);
	val |= UHSIC_DETECT_SHORT_CONNECT;
	writel(val, base + UHSIC_MISC_CFG0);
	udelay(1);
	if (phy->pdata->ops && phy->pdata->ops->port_power)
		phy->pdata->ops->port_power();

	return 0;
}

static struct tegra_usb_phy_ops utmi_phy_ops = {
	.init		= _usb_phy_init,
	.reset		= usb_phy_reset,
	.open		= utmi_phy_open,
	.close		= utmi_phy_close,
	.irq		= utmi_phy_irq,
	.power_on	= utmi_phy_power_on,
	.power_off	= utmi_phy_power_off,
	.pre_resume = utmi_phy_pre_resume,
	.resume	= utmi_phy_resume,
	.charger_detect = utmi_phy_charger_detect,
	.qc2_charger_detect = utmi_phy_qc2_charger_detect,
	.nv_charger_detect = utmi_phy_nv_charger_detect,
	.apple_charger_1000ma_detect = utmi_phy_apple_charger_1000ma_detect,
	.apple_charger_2000ma_detect = utmi_phy_apple_charger_2000ma_detect,
	.apple_charger_500ma_detect = utmi_phy_apple_charger_500ma_detect,
	.pmc_disable = utmi_phy_pmc_disable,
};

static struct tegra_usb_phy_ops uhsic_phy_ops = {
	.init		= _usb_phy_init,
	.open		= uhsic_phy_open,
	.close		= uhsic_phy_close,
	.irq		= uhsic_phy_irq,
	.power_on	= uhsic_phy_power_on,
	.power_off	= uhsic_phy_power_off,
	.pre_resume	= uhsic_phy_pre_resume,
	.port_power = uhsic_phy_bus_port_power,
};

static struct tegra_usb_phy_ops *phy_ops[] = {
	[TEGRA_USB_PHY_INTF_UTMI] = &utmi_phy_ops,
	[TEGRA_USB_PHY_INTF_HSIC] = &uhsic_phy_ops,
};

int tegra11x_usb_phy_init_ops(struct tegra_usb_phy *phy)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	phy->ops = phy_ops[phy->pdata->phy_intf];

	/* FIXME: uncommenting below line to make USB host mode fail*/
	/* pmc->pmc_ops->power_down_pmc(pmc); */

	return 0;
}

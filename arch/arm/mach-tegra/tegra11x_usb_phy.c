/*
 * arch/arm/mach-tegra/tegra11x_usb_phy.c
 *
 * Copyright (c) 2012-2013 NVIDIA Corporation. All rights reserved.
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
#include <linux/usb/ulpi.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/tegra_usb.h>
#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/pinmux.h>
#include <mach/tegra_usb_pmc.h>
#include <mach/tegra_usb_pad_ctrl.h>
#include <mach/pinmux-t11.h>
#include <asm/mach-types.h>
#include "tegra_usb_phy.h"
#include "gpio-names.h"
#include "fuse.h"

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

#define ULPI_VIEWPORT           0x170
#define   ULPI_WAKEUP           (1 << 31)
#define   ULPI_RUN              (1 << 30)
#define   ULPI_RD_WR            (1 << 29)

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
#define   ULPI_PHY_ENABLE		(1 << 13)
#define   UHSIC_RESET			(1 << 14)
#define   USB_WAKEUP_DEBOUNCE_COUNT(x)	(((x) & 0x7) << 16)
#define   UHSIC_PHY_ENABLE		(1 << 19)
#define   ULPIS2S_SLV0_RESET		(1 << 20)
#define   ULPIS2S_SLV1_RESET		(1 << 21)
#define   ULPIS2S_LINE_RESET		(1 << 22)
#define   ULPI_PADS_RESET		(1 << 23)
#define   ULPI_PADS_CLKEN_RESET		(1 << 24)

#define USB_PHY_VBUS_WAKEUP_ID	0x408
#define   VDCD_DET_STS		(1 << 26)
#define   VDCD_DET_CHG_DET	(1 << 25)
#define   VDAT_DET_INT_EN	(1 << 16)
#define   VDAT_DET_CHG_DET	(1 << 17)
#define   VDAT_DET_STS		(1 << 18)
#define   USB_ID_STATUS		(1 << 2)

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

#define UTMIP_BIAS_CFG1		0x83c
#define   UTMIP_BIAS_PDTRK_COUNT(x) (((x) & 0x1f) << 3)
#define   UTMIP_BIAS_PDTRK_POWERDOWN	(1 << 0)
#define   UTMIP_BIAS_PDTRK_POWERUP	(1 << 1)

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

#define UTMIP_BIAS_STS0			0x840
#define   UTMIP_RCTRL_VAL(x)		(((x) & 0xffff) << 0)
#define   UTMIP_TCTRL_VAL(x)		(((x) & (0xffff << 16)) >> 16)

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

/* ULPI GPIO */
#define ULPI_STP	TEGRA_GPIO_PY3
#define ULPI_DIR	TEGRA_GPIO_PY1
#define ULPI_D0		TEGRA_GPIO_PO1
#define ULPI_D1		TEGRA_GPIO_PO2

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
#if !defined(CONFIG_TEGRA_SILICON_PLATFORM)
	val |= TEGRA_STREAM_DISABLE_OFFSET;
#else
	val &= ~TEGRA_STREAM_DISABLE_OFFSET;
#endif
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

#if !defined(CONFIG_TEGRA_SILICON_PLATFORM)
        val =  readl(base + TEGRA_STREAM_DISABLE);
        val |= TEGRA_STREAM_DISABLE_OFFSET;
        writel(val , base + TEGRA_STREAM_DISABLE);
#endif
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
			if (phy->port_speed < USB_PHY_PORT_SPEED_UNKNOWN) {
				pr_info("%s: utmip remote wake detected\n",
								__func__);
				phy->pmc_remote_wakeup = true;
			} else {
				phy->pmc_hotplug_wakeup = true;
			}
			return true;
		}
	}
	return false;
}

static void utmi_phy_enable_trking_data(struct tegra_usb_phy *phy)
{
	void __iomem *base = IO_ADDRESS(TEGRA_USB_BASE);
	static bool init_done = false;
	u32 val;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	/* Should be done only once after system boot */
	if (init_done)
		return;

	clk_enable(phy->utmi_pad_clk);
	/* Bias pad MASTER_ENABLE=1 */
	tegra_usb_pmc_reg_update(PMC_UTMIP_BIAS_MASTER_CNTRL, 0,
			BIAS_MASTER_PROG_VAL);

	/* Setting the tracking length time */
	val = readl(base + UTMIP_BIAS_CFG1);
	val &= ~UTMIP_BIAS_PDTRK_COUNT(~0);
	val |= UTMIP_BIAS_PDTRK_COUNT(5);
	writel(val, base + UTMIP_BIAS_CFG1);

	/* Bias PDTRK is Shared and MUST be done from USB1 ONLY, PD_TRK=0 */
	val = readl(base + UTMIP_BIAS_CFG1);
	val &= ~UTMIP_BIAS_PDTRK_POWERDOWN;
	writel(val, base + UTMIP_BIAS_CFG1);

	val = readl(base + UTMIP_BIAS_CFG1);
	val |= UTMIP_BIAS_PDTRK_POWERUP;
	writel(val, base + UTMIP_BIAS_CFG1);

	/* Wait for 25usec */
	udelay(25);

	/* Bias pad MASTER_ENABLE=0 */
	tegra_usb_pmc_reg_update(PMC_UTMIP_BIAS_MASTER_CNTRL,
			BIAS_MASTER_PROG_VAL, 0);

	/* Wait for 1usec */
	udelay(1);

	/* Bias pad MASTER_ENABLE=1 */
	tegra_usb_pmc_reg_update(PMC_UTMIP_BIAS_MASTER_CNTRL, 0,
			BIAS_MASTER_PROG_VAL);

	/* Read RCTRL and TCTRL from UTMIP space */
	val = readl(base + UTMIP_BIAS_STS0);
	pmc_data[phy->inst].utmip_rctrl_val = 0xf + ffz(UTMIP_RCTRL_VAL(val));
	pmc_data[phy->inst].utmip_tctrl_val = 0xf + ffz(UTMIP_TCTRL_VAL(val));

	/* PD_TRK=1 */
	val = readl(base + UTMIP_BIAS_CFG1);
	val |= UTMIP_BIAS_PDTRK_POWERDOWN;
	writel(val, base + UTMIP_BIAS_CFG1);

	/* Program thermally encoded RCTRL_VAL, TCTRL_VAL into PMC space */
	val = PMC_TCTRL_VAL(pmc_data[phy->inst].utmip_tctrl_val) |
		PMC_RCTRL_VAL(pmc_data[phy->inst].utmip_rctrl_val);
	tegra_usb_pmc_reg_update(PMC_UTMIP_TERM_PAD_CFG, 0xffffffff, val);
	clk_disable(phy->utmi_pad_clk);
	init_done = true;
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
	unsigned long parent_rate;
	int i;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	phy->utmi_pad_clk = clk_get_sys("utmip-pad", NULL);
	if (IS_ERR(phy->utmi_pad_clk)) {
		pr_err("%s: can't get utmip pad clock\n", __func__);
		return PTR_ERR(phy->utmi_pad_clk);
	}
	pmc_init(phy);

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
		val = readl(base + UTMIP_PMC_WAKEUP0);
		val &= ~EVENT_INT_ENB;
		writel(val, base + UTMIP_PMC_WAKEUP0);

		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);

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
	void __iomem *base = phy->regs;
	struct tegra_usb_pmc_data *pmc = &pmc_data[phy->inst];

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = tegra_usb_pmc_reg_read(PMC_SLEEP_CFG);
	if (val & UTMIP_MASTER_ENABLE(inst)) {
		if (!remote_wakeup) {
			val = readl(base + UTMIP_PMC_WAKEUP0);
			val &= ~EVENT_INT_ENB;
			writel(val, base + UTMIP_PMC_WAKEUP0);

			pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);

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

		pmc->pmc_ops->setup_pmc_wake_detect(pmc);
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

	val = readl(base + UTMIP_DEBOUNCE_CFG0);
	val &= ~UTMIP_BIAS_DEBOUNCE_A(~0);
	val |= UTMIP_BIAS_DEBOUNCE_A(phy->freq->debounce);
	writel(val, base + UTMIP_DEBOUNCE_CFG0);

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

	val = readl(base + UTMIP_BIAS_CFG1);
	val &= ~UTMIP_BIAS_PDTRK_COUNT(~0);
	val |= UTMIP_BIAS_PDTRK_COUNT(phy->freq->pdtrk_count);
	writel(val, base + UTMIP_BIAS_CFG1);

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

	utmi_phy_enable_trking_data(phy);

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

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE)
		pmc->pmc_ops->powerup_pmc_wake_detect(pmc);

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
				val = readl(base + UTMIP_PMC_WAKEUP0);
				val &= ~EVENT_INT_ENB;
				writel(val, base + UTMIP_PMC_WAKEUP0);
				pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);
				phy->pmc_remote_wakeup = false;
				phy->pmc_hotplug_wakeup = false;
				return;
			}
			wait_time_us--;
		} while (val & (USB_PORTSC_RESUME | USB_PORTSC_SUSP));

		local_irq_save(flags);
		/* disable PMC master control */
		val = readl(base + UTMIP_PMC_WAKEUP0);
		val &= ~EVENT_INT_ENB;
		writel(val, base + UTMIP_PMC_WAKEUP0);
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);
		phy->pmc_remote_wakeup = false;
		phy->pmc_hotplug_wakeup = false;
		PHY_DBG("%s DISABLE_PMC inst = %d\n", __func__, phy->inst);

		val = readl(base + USB_USBCMD);
		val |= USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);

		local_irq_restore(flags);

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
		val = readl(base + UTMIP_PMC_WAKEUP0);
		val &= ~EVENT_INT_ENB;
		writel(val, base + UTMIP_PMC_WAKEUP0);
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);
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
			val = readl(base + UTMIP_PMC_WAKEUP0);
			val &= ~EVENT_INT_ENB;
			writel(val, base + UTMIP_PMC_WAKEUP0);
			pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);
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
		pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);
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
	DBG("%s:PMC remote wakeup detected for HSIC\n", __func__);
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
			val = readl(base + UHSIC_PMC_WAKEUP0);
			val &= ~EVENT_INT_ENB;
			writel(val, base + UHSIC_PMC_WAKEUP0);

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

			pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);
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

	DBG("%s(%d)\n", __func__, __LINE__);

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
				val = readl(base + UHSIC_PMC_WAKEUP0);
				val &= ~EVENT_INT_ENB;
				writel(val, base + UHSIC_PMC_WAKEUP0);

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

				pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);
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
	/* disable PMC master control */
	val = readl(base + UHSIC_PMC_WAKEUP0);
	val &= ~EVENT_INT_ENB;
	writel(val, base + UHSIC_PMC_WAKEUP0);

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

	pmc->pmc_ops->disable_pmc_bus_ctrl(pmc);
	phy->pmc_remote_wakeup = false;

	/* Set RUN bit */
	val = readl(base + USB_USBCMD);
	val |= USB_USBCMD_RS;
	writel(val, base + USB_USBCMD);
	/* Restore local irq if disabled before */
	if (irq_disabled)
		local_irq_restore(flags);
	if (usb_phy_reg_status_wait(base + USB_USBCMD, USB_USBCMD_RS,
						 USB_USBCMD_RS, 2000)) {
		pr_err("%s: timeout waiting for USB_USBCMD_RS\n", __func__);
		return;
	}
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
	/* check if there is any remote wake event */
	if (!phy->pdata->unaligned_dma_buf_supported)
		usb_phy_fence_read(phy);
	if (uhsic_phy_remotewake_detected(phy))
		DBG("%s: uhsic remote wake detected\n", __func__);
	return IRQ_HANDLED;
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
	val &= ~(USB_PORTSC_WKOC | USB_PORTSC_WKDS | USB_PORTSC_WKCN);
	writel(val, base + USB_PORTSC);

	val = readl(base + UHSIC_PADS_CFG0);
	/* Clear RTUNEP SLEWP & SLEWN bit fields */
	val &= ~(UHSIC_TX_RTUNEP | UHSIC_TX_SLEWP | UHSIC_TX_SLEWN);
	/* set Rtune impedance to 50 ohm */
	val |= UHSIC_TX_RTUNE_P(0xC);
	writel(val, base + UHSIC_PADS_CFG0);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
				USB_PHY_CLK_VALID, USB_PHY_CLK_VALID, 2500)) {
		pr_err("%s: timeout waiting for phy to stabilize\n", __func__);
		return -ETIMEDOUT;
	}

	phy->phy_clk_on = true;
	phy->hw_accessible = true;

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
#if defined(CONFIG_TEGRA_SILICON_PLATFORM)
	struct tegra_ulpi_config *config = &phy->pdata->u_cfg.ulpi;
#endif
	int err = 0;

	phy->ulpi_clk = NULL;
	DBG("%s inst:[%d]\n", __func__, phy->inst);

#if defined(CONFIG_TEGRA_SILICON_PLATFORM)
	if (config->clk) {
		phy->ulpi_clk = clk_get_sys(NULL, config->clk);
		if (IS_ERR(phy->ulpi_clk)) {
			pr_err("%s: can't get ulpi clock\n", __func__);
			err = -ENXIO;
		}
	}
#endif
	phy->ulpi_vp = otg_ulpi_create(&ulpi_viewport_access_ops, 0);
	phy->ulpi_vp->io_priv = phy->regs + ULPI_VIEWPORT;

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
	DBG("%s inst:[%d]\n", __func__, phy->inst);
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
	val |= UHSIC_RESET;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + ULPI_TIMING_CTRL_0);
	val |= ULPI_OUTPUT_PINMUX_BYP | ULPI_CLKOUT_PINMUX_BYP;
	writel(val, base + ULPI_TIMING_CTRL_0);

	val = readl(base + USB_SUSP_CTRL);
	val |= ULPI_PHY_ENABLE;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + USB_SUSP_CTRL);
	val |= USB_SUSP_CLR;
	writel(val, base + USB_SUSP_CTRL);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL, USB_PHY_CLK_VALID,
						USB_PHY_CLK_VALID, 2500))
		pr_err("%s: timeout waiting for phy to stabilize\n", __func__);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL, USB_CLKEN,
						USB_CLKEN, 2500))
		pr_err("%s: timeout waiting for AHB clock\n", __func__);

	val = readl(base + USB_SUSP_CTRL);
	val &= ~USB_SUSP_CLR;
	writel(val, base + USB_SUSP_CTRL);

	val = 0;
	writel(val, base + ULPI_TIMING_CTRL_1);

	ulpi_set_trimmer(phy);

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

	val = readl(base + USB_PORTSC);
	val |= USB_PORTSC_WKOC | USB_PORTSC_WKDS | USB_PORTSC_WKCN;
	writel(val, base + USB_PORTSC);

	phy->phy_clk_on = true;
	phy->hw_accessible = true;

	return 0;
}

static inline void ulpi_link_phy_set_tristate(bool enable)
{

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

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	val = readl(base + ULPI_TIMING_CTRL_0);
	val |= ULPI_OUTPUT_PINMUX_BYP;
	writel(val, base + ULPI_TIMING_CTRL_0);

	ulpi_link_phy_set_tristate(false);
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

static void reset_utmip_uhsic(void __iomem *base)
{
	unsigned long val;

	val = readl(base + USB_SUSP_CTRL);
	val |= UHSIC_RESET;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + USB_SUSP_CTRL);
	val |= UTMIP_RESET;
	writel(val, base + USB_SUSP_CTRL);
}

static void ulpi_set_host(void __iomem *base)
{
	unsigned long val;

	val = readl(base + USB_USBMODE);
	val &= ~USB_USBMODE_MASK;
	val |= USB_USBMODE_HOST;
	writel(val, base + USB_USBMODE);

	val = readl(base + HOSTPC1_DEVLC);
	val |= HOSTPC1_DEVLC_PTS(2);
	writel(val, base + HOSTPC1_DEVLC);
}

static inline void ulpi_pinmux_bypass(struct tegra_usb_phy *phy, bool enable)
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
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	int tristate = (enable) ? TEGRA_TRI_TRISTATE : TEGRA_TRI_NORMAL;
	DBG("%s(%d) inst:[%s] FIXME enable pin group +++\n", __func__,
				__LINE__, enable ? "TRISTATE" : "NORMAL");

	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DATA0, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DATA1, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DATA2, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DATA3, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DATA4, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DATA5, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DATA6, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DATA7, tristate);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_NXT, tristate);

	if (enable)
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DIR, tristate);
#endif
}

static void ulpi_null_phy_obs_read(void)
{
	static void __iomem *apb_misc;
	unsigned slv0_obs, s2s_obs;

	if (!apb_misc)
		apb_misc = ioremap(TEGRA_APB_MISC_BASE, TEGRA_APB_MISC_SIZE);

	writel(0x80d1003c, apb_misc + APB_MISC_GP_OBSCTRL_0);
	slv0_obs = readl(apb_misc + APB_MISC_GP_OBSDATA_0);

	writel(0x80d10040, apb_misc + APB_MISC_GP_OBSCTRL_0);
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

/* NOTE: this function must be called before ehci reset */
static int ulpi_null_phy_init(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	_usb_phy_init(phy);
	val = readl(base + ULPIS2S_CTRL);
	val |=	ULPIS2S_SLV0_CLAMP_XMIT;
	writel(val, base + ULPIS2S_CTRL);

	val = readl(base + USB_SUSP_CTRL);
	val |= ULPIS2S_SLV0_RESET;
	writel(val, base + USB_SUSP_CTRL);
	udelay(10);

	return 0;
}

static int ulpi_null_phy_irq(struct tegra_usb_phy *phy)
{
	return IRQ_HANDLED;
}

/* NOTE: this function must be called after ehci reset */
static int ulpi_null_phy_cmd_reset(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	ulpi_set_host(base);

	/* remove slave0 reset */
	val = readl(base + USB_SUSP_CTRL);
	val &= ~ULPIS2S_SLV0_RESET;
	writel(val, base + USB_SUSP_CTRL);

	val = readl(base + ULPIS2S_CTRL);
	val &=	~ULPIS2S_SLV0_CLAMP_XMIT;
	writel(val, base + ULPIS2S_CTRL);
	udelay(10);

	return 0;
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
	ulpi_null_phy_init(phy);

	val = readl(base + USB_USBCMD);
	val |= USB_CMD_RESET;
	writel(val, base + USB_USBCMD);

	if (usb_phy_reg_status_wait(base + USB_USBCMD,
		USB_CMD_RESET, 0, 2500) < 0) {
		pr_err("%s: timeout waiting for reset\n", __func__);
	}

	ulpi_null_phy_cmd_reset(phy);

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
	reset_utmip_uhsic(base);

	/* remove ULPI PADS CLKEN reset */
	val = readl(base + USB_SUSP_CTRL);
	val &= ~ULPI_PADS_CLKEN_RESET;
	writel(val, base + USB_SUSP_CTRL);
	udelay(10);

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
	val &= ~ULPI_SHADOW_CLK_SEL;
	val &= ~ULPI_LBK_PAD_EN;
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
	val |= ULPIS2S_SPARE((phy->pdata->op_mode == TEGRA_USB_OPMODE_HOST) ? 3 : 1);
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

	ulpi_set_host(base);

	/* remove slave0 reset */
	val = readl(base + USB_SUSP_CTRL);
	val &= ~ULPIS2S_SLV0_RESET;
	writel(val, base + USB_SUSP_CTRL);

	/* remove slave1 and line reset */
	val = readl(base + USB_SUSP_CTRL);
	val &= ~ULPIS2S_SLV1_RESET;
	val &= ~ULPIS2S_LINE_RESET;

	/* remove ULPI PADS reset */
	val &= ~ULPI_PADS_RESET;
	writel(val, base + USB_SUSP_CTRL);

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
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
		/* remove DIR tristate */
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DIR,
					  TEGRA_TRI_NORMAL);
#endif
	}
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
	.nv_charger_detect = utmi_phy_nv_charger_detect,
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

static struct tegra_usb_phy_ops ulpi_link_phy_ops = {
	.init		= _usb_phy_init,
	.reset		= usb_phy_reset,
        .open           = ulpi_link_phy_open,
        .close		= ulpi_link_phy_close,
        .irq            = ulpi_link_phy_irq,
        .power_on       = ulpi_link_phy_power_on,
        .power_off      = ulpi_link_phy_power_off,
        .resume         = ulpi_link_phy_resume,
};

static struct tegra_usb_phy_ops ulpi_null_phy_ops = {
	.open		= ulpi_null_phy_open,
	.close		= ulpi_null_phy_close,
	.init		= ulpi_null_phy_init,
	.irq		= ulpi_null_phy_irq,
	.power_on	= ulpi_null_phy_power_on,
	.power_off	= ulpi_null_phy_power_off,
	.pre_resume = ulpi_null_phy_pre_resume,
	.resume = ulpi_null_phy_resume,
	.post_resume = ulpi_null_phy_post_resume,
	.reset		= ulpi_null_phy_cmd_reset,
};

static struct tegra_usb_phy_ops icusb_phy_ops;

static struct tegra_usb_phy_ops *phy_ops[] = {
	[TEGRA_USB_PHY_INTF_UTMI] = &utmi_phy_ops,
	[TEGRA_USB_PHY_INTF_ULPI_LINK] = &ulpi_link_phy_ops,
	[TEGRA_USB_PHY_INTF_ULPI_NULL] = &ulpi_null_phy_ops,
	[TEGRA_USB_PHY_INTF_HSIC] = &uhsic_phy_ops,
	[TEGRA_USB_PHY_INTF_ICUSB] = &icusb_phy_ops,
};

int tegra11x_usb_phy_init_ops(struct tegra_usb_phy *phy)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	phy->ops = phy_ops[phy->pdata->phy_intf];

	/* FIXME: uncommenting below line to make USB host mode fail*/
	/* pmc->pmc_ops->power_down_pmc(pmc); */

	return 0;
}

/*
 * arch/arm/mach-tegra/tegra3_usb_phy.c
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
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/clk/tegra.h>
#include <mach/pinmux.h>
#include <mach/pinmux-tegra30.h>
#include "tegra_usb_phy.h"
#include "../../../arch/arm/mach-tegra/gpio-names.h"
#include "../../../arch/arm/mach-tegra/fuse.h"
#include "../../../arch/arm/mach-tegra/clock.h"

/* HACK! This needs to come from DT */
#include "../../../arch/arm/mach-tegra/iomap.h"

#define USB_USBCMD		0x130
#define   USB_USBCMD_RS		(1 << 0)
#define   USB_CMD_RESET	(1<<1)

#define USB_USBSTS		0x134
#define   USB_USBSTS_PCI	(1 << 2)
#define   USB_USBSTS_SRI	(1 << 7)
#define   USB_USBSTS_HCH	(1 << 12)

#define USB_USBINTR		0x138

#define USB_TXFILLTUNING        0x154
#define USB_FIFO_TXFILL_THRES(x)   (((x) & 0x1f) << 16)
#define USB_FIFO_TXFILL_MASK    0x1f0000

#define USB_ASYNCLISTADDR	0x148

#define ICUSB_CTRL		0x15c

#define USB_PORTSC		0x174
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

#define HOSTPC1_DEVLC		0x1b4
#define   HOSTPC1_DEVLC_PHCD		(1 << 22)
#define   HOSTPC1_DEVLC_PTS(x)		(((x) & 0x7) << 29)
#define   HOSTPC1_DEVLC_PTS_MASK	7
#define   HOSTPC1_DEVLC_PTS_HSIC	4
#define   HOSTPC1_DEVLC_STS		(1 << 28)
#define   HOSTPC1_DEVLC_PSPD(x)		(((x) & 0x3) << 25)
#define   HOSTPC1_DEVLC_PSPD_MASK	3
#define   HOSTPC1_DEVLC_PSPD_HIGH_SPEED 2

#define USB_USBMODE		0x1f8
#define   USB_USBMODE_MASK		(3 << 0)
#define   USB_USBMODE_HOST		(3 << 0)
#define   USB_USBMODE_DEVICE		(2 << 0)

#define USB_SUSP_CTRL		0x400
#define   USB_WAKE_ON_CNNT_EN_DEV	(1 << 3)
#define   USB_WAKE_ON_DISCON_EN_DEV (1 << 4)
#define   USB_SUSP_CLR			(1 << 5)
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
#define   UTMIP_XCVR_MAX_OFFSET		2
#define   UTMIP_XCVR_SETUP_MAX_VALUE	0x7f
#define   UTMIP_XCVR_SETUP_MIN_VALUE	0
#define   XCVR_SETUP_MSB_CALIB(x) ((x) >> 4)

#define UTMIP_BIAS_CFG0		0x80c
#define   UTMIP_OTGPD			(1 << 11)
#define   UTMIP_BIASPD			(1 << 10)
#define   UTMIP_HSSQUELCH_LEVEL(x)	(((x) & 0x3) << 0)
#define   UTMIP_HSDISCON_LEVEL(x)	(((x) & 0x3) << 2)
#define   UTMIP_HSDISCON_LEVEL_MSB	(1 << 24)

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
#define   UTMIP_ON_SINK_EN		(1 << 2)
#define   UTMIP_OP_SRC_EN		(1 << 3)

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
#define   FORCE_PULLDN_DM	(1 << 8)
#define   FORCE_PULLDN_DP	(1 << 9)
#define   COMB_TERMS		(1 << 0)
#define   ALWAYS_FREE_RUNNING_TERMS (1 << 1)

#define UTMIP_SPARE_CFG0	0x834
#define   FUSE_SETUP_SEL		(1 << 3)
#define   FUSE_ATERM_SEL		(1 << 4)

#define UTMIP_PMC_WAKEUP0		0x84c
#define   EVENT_INT_ENB			(1 << 0)

#define UHSIC_PMC_WAKEUP0		0xc34

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
#define UHSIC_HS_READY_WAIT_FOR_VALID	(1 << 9)
#define UHSIC_MISC_CFG0				0xc14
#define   UHSIC_SUSPEND_EXIT_ON_EDGE		(1 << 7)
#define   UHSIC_DETECT_SHORT_CONNECT		(1 << 8)
#define   UHSIC_FORCE_XCVR_MODE			(1 << 15)
#define   UHSIC_DISABLE_BUSRESET		(1 << 20)
#define UHSIC_MISC_CFG1				0xc18
#define   UHSIC_PLLU_STABLE_COUNT(x)		(((x) & 0xfff) << 2)

#define UHSIC_PADS_CFG0				0xc1c
#define   UHSIC_TX_RTUNEN			0xf000
#define   UHSIC_TX_RTUNE(x)			(((x) & 0xf) << 12)

#define UHSIC_PADS_CFG1				0xc20
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

#define UHSIC_CMD_CFG0			0xc24
#define UHSIC_PRETEND_CONNECT_DETECT	(1 << 5)

#define UHSIC_STAT_CFG0		0xc28
#define UHSIC_CONNECT_DETECT		(1 << 0)

#define PMC_USB_DEBOUNCE			0xec
#define UTMIP_LINE_DEB_CNT(x)		(((x) & 0xf) << 16)
#define UHSIC_LINE_DEB_CNT(x)		(((x) & 0xf) << 20)
#define PMC_USB_DEBOUNCE_VAL(x)		((x) & 0xffff)

#define PMC_USB_AO				0xf0

#define PMC_POWER_DOWN_MASK			0xffff
#define HSIC_RESERVED_P0			(3 << 14)
#define STROBE_VAL_PD_P0			(1 << 12)
#define DATA_VAL_PD_P0				(1 << 13)

#define USB_ID_PD(inst)			(1 << ((4*(inst))+3))
#define VBUS_WAKEUP_PD(inst)			(1 << ((4*(inst))+2))
#define   USBON_VAL_PD(inst)			(1 << ((4*(inst))+1))
#define   USBON_VAL_PD_P2			(1 << 9)
#define   USBON_VAL_PD_P1			(1 << 5)
#define   USBON_VAL_PD_P0			(1 << 1)
#define   USBOP_VAL_PD(inst)			(1 << (4*(inst)))
#define   USBOP_VAL_PD_P2			(1 << 8)
#define   USBOP_VAL_PD_P1			(1 << 4)
#define   USBOP_VAL_PD_P0			(1 << 0)
#define   PMC_USB_AO_ID_PD_P0			(1 << 3)
#define   PMC_USB_AO_VBUS_WAKEUP_PD_P0	(1 << 2)

#define PMC_TRIGGERS			0x1ec

#define   UHSIC_CLR_WALK_PTR_P0		(1 << 3)
#define   UTMIP_CLR_WALK_PTR(inst)	(1 << (inst))
#define   UTMIP_CLR_WALK_PTR_P2		(1 << 2)
#define   UTMIP_CLR_WALK_PTR_P1		(1 << 1)
#define   UTMIP_CLR_WALK_PTR_P0		(1 << 0)
#define   UTMIP_CAP_CFG(inst)	(1 << ((inst)+4))
#define   UTMIP_CAP_CFG_P2		(1 << 6)
#define   UTMIP_CAP_CFG_P1		(1 << 5)
#define   UTMIP_CAP_CFG_P0		(1 << 4)
#define   UTMIP_CLR_WAKE_ALARM(inst)	(1 << ((inst)+12))
#define   UHSIC_CLR_WAKE_ALARM_P0	(1 << 15)
#define   UTMIP_CLR_WAKE_ALARM_P2	(1 << 14)

#define PMC_PAD_CFG		(0x1f4)

#define PMC_UTMIP_TERM_PAD_CFG	0x1f8
#define   PMC_TCTRL_VAL(x)	(((x) & 0x1f) << 5)
#define   PMC_RCTRL_VAL(x)	(((x) & 0x1f) << 0)

#define PMC_SLEEP_CFG			0x1fc

#define   UHSIC_MASTER_ENABLE			(1 << 24)
#define   UHSIC_WAKE_VAL(x)		(((x) & 0xf) << 28)
#define   WAKE_VAL_SD10			0x2
#define   UTMIP_TCTRL_USE_PMC(inst) (1 << ((8*(inst))+3))
#define   UTMIP_TCTRL_USE_PMC_P2		(1 << 19)
#define   UTMIP_TCTRL_USE_PMC_P1		(1 << 11)
#define   UTMIP_TCTRL_USE_PMC_P0		(1 << 3)
#define   UTMIP_RCTRL_USE_PMC(inst) (1 << ((8*(inst))+2))
#define   UTMIP_RCTRL_USE_PMC_P2		(1 << 18)
#define   UTMIP_RCTRL_USE_PMC_P1		(1 << 10)
#define   UTMIP_RCTRL_USE_PMC_P0		(1 << 2)
#define   UTMIP_FSLS_USE_PMC(inst)	(1 << ((8*(inst))+1))
#define   UTMIP_FSLS_USE_PMC_P2		(1 << 17)
#define   UTMIP_FSLS_USE_PMC_P1		(1 << 9)
#define   UTMIP_FSLS_USE_PMC_P0		(1 << 1)
#define   UTMIP_MASTER_ENABLE(inst) (1 << (8*(inst)))
#define   UTMIP_MASTER_ENABLE_P2		(1 << 16)
#define   UTMIP_MASTER_ENABLE_P1		(1 << 8)
#define   UTMIP_MASTER_ENABLE_P0		(1 << 0)
#define UHSIC_MASTER_ENABLE_P0		(1 << 24)
#define UHSIC_WAKE_VAL_P0(x)		(((x) & 0xf) << 28)

#define PMC_SLEEPWALK_CFG		0x200

#define   UHSIC_WAKE_WALK_EN_P0	(1 << 30)
#define   UHSIC_LINEVAL_WALK_EN	(1 << 31)
#define   UTMIP_LINEVAL_WALK_EN(inst) (1 << ((8*(inst))+7))
#define   UTMIP_LINEVAL_WALK_EN_P2	(1 << 23)
#define   UTMIP_LINEVAL_WALK_EN_P1	(1 << 15)
#define   UTMIP_LINEVAL_WALK_EN_P0	(1 << 7)
#define   UTMIP_WAKE_VAL(inst, x) (((x) & 0xf) << ((8*(inst))+4))
#define   UTMIP_WAKE_VAL_P2(x)		(((x) & 0xf) << 20)
#define   UTMIP_WAKE_VAL_P1(x)		(((x) & 0xf) << 12)
#define   UTMIP_WAKE_VAL_P0(x)		(((x) & 0xf) << 4)
#define   WAKE_VAL_NONE		0xc
#define   WAKE_VAL_ANY			0xF
#define   WAKE_VAL_FSJ			0x2
#define   WAKE_VAL_FSK			0x1
#define   WAKE_VAL_SE0			0x0

#define PMC_SLEEPWALK_REG(inst)		(0x204 + (4*(inst)))
#define   UTMIP_USBOP_RPD_A	(1 << 0)
#define   UTMIP_USBON_RPD_A	(1 << 1)
#define   UTMIP_AP_A			(1 << 4)
#define   UTMIP_AN_A			(1 << 5)
#define   UTMIP_HIGHZ_A		(1 << 6)
#define   UTMIP_USBOP_RPD_B	(1 << 8)
#define   UTMIP_USBON_RPD_B	(1 << 9)
#define   UTMIP_AP_B			(1 << 12)
#define   UTMIP_AN_B			(1 << 13)
#define   UTMIP_HIGHZ_B		(1 << 14)
#define   UTMIP_USBOP_RPD_C	(1 << 16)
#define   UTMIP_USBON_RPD_C	(1 << 17)
#define   UTMIP_AP_C		(1 << 20)
#define   UTMIP_AN_C		(1 << 21)
#define   UTMIP_HIGHZ_C		(1 << 22)
#define   UTMIP_USBOP_RPD_D	(1 << 24)
#define   UTMIP_USBON_RPD_D	(1 << 25)
#define   UTMIP_AP_D		(1 << 28)
#define   UTMIP_AN_D		(1 << 29)
#define   UTMIP_HIGHZ_D		(1 << 30)

#define PMC_SLEEPWALK_UHSIC		0x210

#define UHSIC_STROBE_RPD_A		(1 << 0)
#define UHSIC_DATA_RPD_A		(1 << 1)
#define UHSIC_STROBE_RPU_A		(1 << 2)
#define UHSIC_DATA_RPU_A		(1 << 3)
#define UHSIC_STROBE_RPD_B		(1 << 8)
#define UHSIC_DATA_RPD_B		(1 << 9)
#define UHSIC_STROBE_RPU_B		(1 << 10)
#define UHSIC_DATA_RPU_B		(1 << 11)
#define UHSIC_STROBE_RPD_C		(1 << 16)
#define UHSIC_DATA_RPD_C		(1 << 17)
#define UHSIC_STROBE_RPU_C		(1 << 18)
#define UHSIC_DATA_RPU_C		(1 << 19)
#define UHSIC_STROBE_RPD_D		(1 << 24)
#define UHSIC_DATA_RPD_D		(1 << 25)
#define UHSIC_STROBE_RPU_D		(1 << 26)
#define UHSIC_DATA_RPU_D		(1 << 27)

#define UTMIP_UHSIC_STATUS		0x214

#define UTMIP_USBOP_VAL(inst)		(1 << ((2*(inst)) + 8))
#define UTMIP_USBOP_VAL_P2		(1 << 12)
#define UTMIP_USBOP_VAL_P1		(1 << 10)
#define UTMIP_USBOP_VAL_P0		(1 << 8)
#define UTMIP_USBON_VAL(inst)		(1 << ((2*(inst)) + 9))
#define UTMIP_USBON_VAL_P2		(1 << 13)
#define UTMIP_USBON_VAL_P1		(1 << 11)
#define UTMIP_USBON_VAL_P0		(1 << 9)
#define UHSIC_WAKE_ALARM		(1 << 19)
#define UTMIP_WAKE_ALARM(inst)		(1 << ((inst) + 16))
#define UTMIP_WAKE_ALARM_P2		(1 << 18)
#define UTMIP_WAKE_ALARM_P1		(1 << 17)
#define UTMIP_WAKE_ALARM_P0		(1 << 16)
#define UHSIC_DATA_VAL_P0		(1 << 15)
#define UHSIC_STROBE_VAL_P0		(1 << 14)
#define UTMIP_WALK_PTR_VAL(inst)	(0x3 << ((inst)*2))
#define UHSIC_WALK_PTR_VAL		(0x3 << 6)
#define UTMIP_WALK_PTR(inst)		(1 << ((inst)*2))
#define UTMIP_WALK_PTR_P2		(1 << 4)
#define UTMIP_WALK_PTR_P1		(1 << 2)
#define UTMIP_WALK_PTR_P0		(1 << 0)

#define USB1_PREFETCH_ID			   6
#define USB2_PREFETCH_ID			   18
#define USB3_PREFETCH_ID			   17

#define PMC_UTMIP_UHSIC_FAKE		0x218

#define UHSIC_STROBE_VAL		(1 << 12)
#define UHSIC_DATA_VAL			(1 << 13)
#define UHSIC_STROBE_ENB		(1 << 14)
#define UHSIC_DATA_ENB			(1 << 15)
#define   USBON_VAL(inst)	(1 << ((4*(inst))+1))
#define   USBON_VAL_P2			(1 << 9)
#define   USBON_VAL_P1			(1 << 5)
#define   USBON_VAL_P0			(1 << 1)
#define   USBOP_VAL(inst)	(1 << (4*(inst)))
#define   USBOP_VAL_P2			(1 << 8)
#define   USBOP_VAL_P1			(1 << 4)
#define   USBOP_VAL_P0			(1 << 0)

#define PMC_UTMIP_BIAS_MASTER_CNTRL 0x30c
#define   BIAS_MASTER_PROG_VAL		(1 << 1)

#define PMC_UTMIP_MASTER_CONFIG	0x310

#define UTMIP_PWR(inst)		(1 << (inst))
#define UHSIC_PWR			(1 << 3)

#define FUSE_USB_CALIB_0		0x1F0
#define   XCVR_SETUP(x)	(((x) & 0x7F) << 0)
#define	  XCVR_SETUP_LSB_MASK	0xF
#define	  XCVR_SETUP_MSB_MASK	0x70
#define   XCVR_SETUP_LSB_MAX_VAL	0xF

#define APB_MISC_GP_OBSCTRL_0	0x818
#define APB_MISC_GP_OBSDATA_0	0x81c

/* ULPI GPIO */
#define ULPI_STP	TEGRA_GPIO_PY3
#define ULPI_DIR	TEGRA_GPIO_PY1
#define ULPI_D0		TEGRA_GPIO_PO1
#define ULPI_D1		TEGRA_GPIO_PO2

/* These values (in milli second) are taken from the battery charging spec */
#define TDP_SRC_ON_MS	 100
#define TDPSRC_CON_MS	 40

#ifdef DEBUG
#define DBG(stuff...)	pr_info("tegra3_usb_phy: " stuff)
#else
#define DBG(stuff...)	do {} while (0)
#endif

#if 0
#define PHY_DBG(stuff...)	pr_info("tegra3_usb_phy: " stuff)
#else
#define PHY_DBG(stuff...)	do {} while (0)
#endif

/* define HSIC phy params */
#define HSIC_SYNC_START_DELAY		9
#define HSIC_IDLE_WAIT_DELAY		17
#define HSIC_ELASTIC_UNDERRUN_LIMIT	16
#define HSIC_ELASTIC_OVERRUN_LIMIT	16

static u32 utmip_rctrl_val, utmip_tctrl_val;
static DEFINE_SPINLOCK(utmip_pad_lock);
static int utmip_pad_count;

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

static void utmip_setup_pmc_wake_detect(struct tegra_usb_phy *phy)
{
	unsigned long val, pmc_pad_cfg_val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned  int inst = phy->inst;
	void __iomem *base = phy->regs;
	bool port_connected;
	enum usb_phy_port_speed port_speed;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	/* check for port connect status */
	val = readl(base + USB_PORTSC);
	port_connected = val & USB_PORTSC_CCS;

	if (!port_connected)
		return;

	port_speed = (readl(base + HOSTPC1_DEVLC) >> 25) &
		HOSTPC1_DEVLC_PSPD_MASK;
	/*Set PMC MASTER bits to do the following
	* a. Take over the UTMI drivers
	* b. set up such that it will take over resume
	*	 if remote wakeup is detected
	* Prepare PMC to take over suspend-wake detect-drive resume until USB
	* controller ready
	*/

	/* disable master enable in PMC */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_MASTER_ENABLE(inst);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* UTMIP_PWR_PX=1 for power savings mode */
	val = readl(pmc_base + PMC_UTMIP_MASTER_CONFIG);
	val |= UTMIP_PWR(inst);
	writel(val, pmc_base + PMC_UTMIP_MASTER_CONFIG);

	/* config debouncer */
	val = readl(pmc_base + PMC_USB_DEBOUNCE);
	val &= ~UTMIP_LINE_DEB_CNT(~0);
	val |= UTMIP_LINE_DEB_CNT(1);
	val |= PMC_USB_DEBOUNCE_VAL(2);
	writel(val, pmc_base + PMC_USB_DEBOUNCE);

	/* Make sure nothing is happening on the line with respect to PMC */
	val = readl(pmc_base + PMC_UTMIP_UHSIC_FAKE);
	val &= ~USBOP_VAL(inst);
	val &= ~USBON_VAL(inst);
	writel(val, pmc_base + PMC_UTMIP_UHSIC_FAKE);

	/* Make sure wake value for line is none */
	val = readl(pmc_base + PMC_SLEEPWALK_CFG);
	val &= ~UTMIP_LINEVAL_WALK_EN(inst);
	writel(val, pmc_base + PMC_SLEEPWALK_CFG);
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(inst, ~0);
	val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* turn off pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val |= (USBOP_VAL_PD(inst) | USBON_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	/* Remove fake values and make synchronizers work a bit */
	val = readl(pmc_base + PMC_UTMIP_UHSIC_FAKE);
	val &= ~USBOP_VAL(inst);
	val &= ~USBON_VAL(inst);
	writel(val, pmc_base + PMC_UTMIP_UHSIC_FAKE);

	/* Enable which type of event can trigger a walk,
	* in this case usb_line_wake */
	val = readl(pmc_base + PMC_SLEEPWALK_CFG);
	val |= UTMIP_LINEVAL_WALK_EN(inst);
	writel(val, pmc_base + PMC_SLEEPWALK_CFG);

	/* Capture FS/LS pad configurations */
	pmc_pad_cfg_val = readl(pmc_base + PMC_PAD_CFG);
	val = readl(pmc_base + PMC_TRIGGERS);
	val |= UTMIP_CAP_CFG(inst);
	writel(val, pmc_base + PMC_TRIGGERS);
	udelay(1);
	pmc_pad_cfg_val = readl(pmc_base + PMC_PAD_CFG);

	/* BIAS MASTER_ENABLE=0 */
	val = readl(pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);
	val &= ~BIAS_MASTER_PROG_VAL;
	writel(val, pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);

	/* program walk sequence, maintain a J, followed by a driven K
	* to signal a resume once an wake event is detected */
	val = readl(pmc_base + PMC_SLEEPWALK_REG(inst));
	val &= ~UTMIP_AP_A;
	val |= UTMIP_USBOP_RPD_A | UTMIP_USBON_RPD_A | UTMIP_AN_A |UTMIP_HIGHZ_A |
		UTMIP_USBOP_RPD_B | UTMIP_USBON_RPD_B | UTMIP_AP_B | UTMIP_AN_B |
		UTMIP_USBOP_RPD_C | UTMIP_USBON_RPD_C | UTMIP_AP_C | UTMIP_AN_C |
		UTMIP_USBOP_RPD_D | UTMIP_USBON_RPD_D | UTMIP_AP_D | UTMIP_AN_D;
	writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));

	if (port_speed == USB_PHY_PORT_SPEED_LOW) {
		val = readl(pmc_base + PMC_SLEEPWALK_REG(inst));
		val &= ~(UTMIP_AN_B | UTMIP_HIGHZ_B | UTMIP_AN_C |
			UTMIP_HIGHZ_C | UTMIP_AN_D | UTMIP_HIGHZ_D);
		writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));
	} else {
		val = readl(pmc_base + PMC_SLEEPWALK_REG(inst));
		val &= ~(UTMIP_AP_B | UTMIP_HIGHZ_B | UTMIP_AP_C |
			UTMIP_HIGHZ_C | UTMIP_AP_D | UTMIP_HIGHZ_D);
		writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));
	}

	/* turn on pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val &= ~(USBOP_VAL_PD(inst) | USBON_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	/* Add small delay before usb detectors provide stable line values */
	mdelay(1);

	/* Program thermally encoded RCTRL_VAL, TCTRL_VAL into PMC space */
	val = readl(pmc_base + PMC_UTMIP_TERM_PAD_CFG);
	val = PMC_TCTRL_VAL(utmip_tctrl_val) | PMC_RCTRL_VAL(utmip_rctrl_val);
	writel(val, pmc_base + PMC_UTMIP_TERM_PAD_CFG);

	phy->pmc_remote_wakeup = false;

	/* Turn over pad configuration to PMC  for line wake events*/
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(inst, ~0);
	val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_ANY);
	val |= UTMIP_RCTRL_USE_PMC(inst) | UTMIP_TCTRL_USE_PMC(inst);
	val |= UTMIP_MASTER_ENABLE(inst) | UTMIP_FSLS_USE_PMC(inst);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	val = readl(base + UTMIP_PMC_WAKEUP0);
	val |= EVENT_INT_ENB;
	writel(val, base + UTMIP_PMC_WAKEUP0);
	PHY_DBG("%s ENABLE_PMC inst = %d\n", __func__, inst);
}

static void utmip_phy_disable_pmc_bus_ctrl(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned  int inst = phy->inst;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(inst, 0xF);
	val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	val = readl(base + UTMIP_PMC_WAKEUP0);
	val &= ~EVENT_INT_ENB;
	writel(val, base + UTMIP_PMC_WAKEUP0);

	/* Disable PMC master mode by clearing MASTER_EN */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~(UTMIP_RCTRL_USE_PMC(inst) | UTMIP_TCTRL_USE_PMC(inst) |
			UTMIP_FSLS_USE_PMC(inst) | UTMIP_MASTER_ENABLE(inst));
	writel(val, pmc_base + PMC_SLEEP_CFG);

	val = readl(pmc_base + PMC_TRIGGERS);
	val &= ~UTMIP_CAP_CFG(inst);
	writel(val, pmc_base + PMC_TRIGGERS);

	/* turn off pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val |= (USBOP_VAL_PD(inst) | USBON_VAL_PD(inst));
	writel(val, pmc_base + PMC_USB_AO);

	val = readl(pmc_base + PMC_TRIGGERS);
	val |= UTMIP_CLR_WALK_PTR(inst);
	val |= UTMIP_CLR_WAKE_ALARM(inst);
	writel(val, pmc_base + PMC_TRIGGERS);

	phy->pmc_remote_wakeup = false;
	PHY_DBG("%s DISABLE_PMC inst = %d\n", __func__, inst);
}

static bool utmi_phy_remotewake_detected(struct tegra_usb_phy *phy)
{
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *base = phy->regs;
	unsigned  int inst = phy->inst;
	u32 val;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(base + UTMIP_PMC_WAKEUP0);
	if (val & EVENT_INT_ENB) {
		val = readl(pmc_base + UTMIP_UHSIC_STATUS);
		if (UTMIP_WAKE_ALARM(inst) & val) {
			val = readl(pmc_base + PMC_SLEEP_CFG);
			val &= ~UTMIP_WAKE_VAL(inst, 0xF);
			val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_NONE);
			writel(val, pmc_base + PMC_SLEEP_CFG);

			val = readl(pmc_base + PMC_TRIGGERS);
			val |= UTMIP_CLR_WAKE_ALARM(inst);
			writel(val, pmc_base + PMC_TRIGGERS);

			val = readl(base + UTMIP_PMC_WAKEUP0);
			val &= ~EVENT_INT_ENB;
			writel(val, base + UTMIP_PMC_WAKEUP0);
			phy->pmc_remote_wakeup = true;
			return true;
		}
	}
	return false;
}

static void utmi_phy_enable_trking_data(struct tegra_usb_phy *phy)
{
	void __iomem *base = IO_ADDRESS(TEGRA_USB_BASE);
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	static bool init_done = false;
	u32 val;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	/* Should be done only once after system boot */
	if (init_done)
		return;

	tegra_clk_prepare_enable(phy->utmi_pad_clk);
	/* Bias pad MASTER_ENABLE=1 */
	val = readl(pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);
	val |= BIAS_MASTER_PROG_VAL;
	writel(val, pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);

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
	val = readl(pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);
	val &= ~BIAS_MASTER_PROG_VAL;
	writel(val, pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);

	/* Wait for 1usec */
	udelay(1);

	/* Bias pad MASTER_ENABLE=1 */
	val = readl(pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);
	val |= BIAS_MASTER_PROG_VAL;
	writel(val, pmc_base + PMC_UTMIP_BIAS_MASTER_CNTRL);

	/* Read RCTRL and TCTRL from UTMIP space */
	val = readl(base + UTMIP_BIAS_STS0);
	utmip_rctrl_val = ffz(UTMIP_RCTRL_VAL(val));
	utmip_tctrl_val = ffz(UTMIP_TCTRL_VAL(val));

	/* PD_TRK=1 */
	val = readl(base + UTMIP_BIAS_CFG1);
	val |= UTMIP_BIAS_PDTRK_POWERDOWN;
	writel(val, base + UTMIP_BIAS_CFG1);

	/* Program thermally encoded RCTRL_VAL, TCTRL_VAL into PMC space */
	val = readl(pmc_base + PMC_UTMIP_TERM_PAD_CFG);
	val = PMC_TCTRL_VAL(utmip_tctrl_val) | PMC_RCTRL_VAL(utmip_rctrl_val);
	writel(val, pmc_base + PMC_UTMIP_TERM_PAD_CFG);
	tegra_clk_disable_unprepare(phy->utmi_pad_clk);
	init_done = true;
}

static void utmip_powerdown_pmc_wake_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned  int inst = phy->inst;

	/* power down UTMIP interfaces */
	val = readl(pmc_base + PMC_UTMIP_MASTER_CONFIG);
	val |= UTMIP_PWR(inst);
	writel(val, pmc_base + PMC_UTMIP_MASTER_CONFIG);

	/* setup sleep walk usb controller */
	val = UTMIP_USBOP_RPD_A | UTMIP_USBON_RPD_A | UTMIP_HIGHZ_A |
		UTMIP_USBOP_RPD_B | UTMIP_USBON_RPD_B | UTMIP_HIGHZ_B |
		UTMIP_USBOP_RPD_C | UTMIP_USBON_RPD_C | UTMIP_HIGHZ_C |
		UTMIP_USBOP_RPD_D | UTMIP_USBON_RPD_D | UTMIP_HIGHZ_D;
	writel(val, pmc_base + PMC_SLEEPWALK_REG(inst));

	/* Program thermally encoded RCTRL_VAL, TCTRL_VAL into PMC space */
	val = readl(pmc_base + PMC_UTMIP_TERM_PAD_CFG);
	val = PMC_TCTRL_VAL(utmip_tctrl_val) | PMC_RCTRL_VAL(utmip_rctrl_val);
	writel(val, pmc_base + PMC_UTMIP_TERM_PAD_CFG);

	/* Turn over pad configuration to PMC */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(inst, ~0);
	val |= UTMIP_WAKE_VAL(inst, WAKE_VAL_NONE) |
		UTMIP_RCTRL_USE_PMC(inst) | UTMIP_TCTRL_USE_PMC(inst) |
		UTMIP_FSLS_USE_PMC(inst) | UTMIP_MASTER_ENABLE(inst);
	writel(val, pmc_base + PMC_SLEEP_CFG);
	PHY_DBG("%s ENABLE_PMC inst = %d\n", __func__, inst);
}

static void utmip_powerup_pmc_wake_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned  int inst = phy->inst;

	/* Disable PMC master mode by clearing MASTER_EN */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~(UTMIP_RCTRL_USE_PMC(inst) | UTMIP_TCTRL_USE_PMC(inst) |
			UTMIP_FSLS_USE_PMC(inst) | UTMIP_MASTER_ENABLE(inst));
	writel(val, pmc_base + PMC_SLEEP_CFG);
	mdelay(1);
	PHY_DBG("%s DISABLE_PMC inst = %d\n", __func__, inst);
}


#ifdef KERNEL_WARNING
static void usb_phy_power_down_pmc(void)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

	/* power down all 3 UTMIP interfaces */
	val = readl(pmc_base + PMC_UTMIP_MASTER_CONFIG);
	val |= UTMIP_PWR(0) | UTMIP_PWR(1) | UTMIP_PWR(2);
	writel(val, pmc_base + PMC_UTMIP_MASTER_CONFIG);

	/* turn on pad detectors */
	writel(PMC_POWER_DOWN_MASK, pmc_base + PMC_USB_AO);

	/* setup sleep walk fl all 3 usb controllers */
	val = UTMIP_USBOP_RPD_A | UTMIP_USBON_RPD_A | UTMIP_HIGHZ_A |
		UTMIP_USBOP_RPD_B | UTMIP_USBON_RPD_B | UTMIP_HIGHZ_B |
		UTMIP_USBOP_RPD_C | UTMIP_USBON_RPD_C | UTMIP_HIGHZ_C |
		UTMIP_USBOP_RPD_D | UTMIP_USBON_RPD_D | UTMIP_HIGHZ_D;
	writel(val, pmc_base + PMC_SLEEPWALK_REG(0));
	writel(val, pmc_base + PMC_SLEEPWALK_REG(1));
	writel(val, pmc_base + PMC_SLEEPWALK_REG(2));

	/* enable pull downs on HSIC PMC */
	val = UHSIC_STROBE_RPD_A | UHSIC_DATA_RPD_A | UHSIC_STROBE_RPD_B |
		UHSIC_DATA_RPD_B | UHSIC_STROBE_RPD_C | UHSIC_DATA_RPD_C |
		UHSIC_STROBE_RPD_D | UHSIC_DATA_RPD_D;
	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC);

	/* Turn over pad configuration to PMC */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UTMIP_WAKE_VAL(0, ~0);
	val &= ~UTMIP_WAKE_VAL(1, ~0);
	val &= ~UTMIP_WAKE_VAL(2, ~0);
	val &= ~UHSIC_WAKE_VAL_P0(~0);
	val |= UTMIP_WAKE_VAL(0, WAKE_VAL_NONE) | UHSIC_WAKE_VAL_P0(WAKE_VAL_NONE) |
	UTMIP_WAKE_VAL(1, WAKE_VAL_NONE) | UTMIP_WAKE_VAL(2, WAKE_VAL_NONE) |
	UTMIP_RCTRL_USE_PMC(0) | UTMIP_RCTRL_USE_PMC(1) | UTMIP_RCTRL_USE_PMC(2) |
	UTMIP_TCTRL_USE_PMC(0) | UTMIP_TCTRL_USE_PMC(1) | UTMIP_TCTRL_USE_PMC(2) |
	UTMIP_FSLS_USE_PMC(0) | UTMIP_FSLS_USE_PMC(1) | UTMIP_FSLS_USE_PMC(2) |
	UTMIP_MASTER_ENABLE(0) | UTMIP_MASTER_ENABLE(1) | UTMIP_MASTER_ENABLE(2) |
	UHSIC_MASTER_ENABLE_P0;
	writel(val, pmc_base + PMC_SLEEP_CFG);
}
#endif

static int usb_phy_bringup_host_controller(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	PHY_DBG("[%d] USB_USBSTS[0x%x] USB_PORTSC[0x%x] port_speed[%d]\n", __LINE__,
		readl(base + USB_USBSTS), readl(base + USB_PORTSC),
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

	if (!phy->pmc_remote_wakeup) {
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
	}
	PHY_DBG("[%d] USB_USBSTS[0x%x] USB_PORTSC[0x%x] port_speed[%d]\n", __LINE__,
		readl(base + USB_USBSTS), readl(base + USB_PORTSC),
							phy->port_speed);

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
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned long parent_rate, val;
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

	/* Power-up the VBUS detector for UTMIP PHY */
	val = readl(pmc_base + PMC_USB_AO);
	val &= ~(PMC_USB_AO_VBUS_WAKEUP_PD_P0 | PMC_USB_AO_ID_PD_P0);
	writel(val, (pmc_base + PMC_USB_AO));

	utmip_powerup_pmc_wake_detect(phy);

	return 0;
}

static void utmi_phy_close(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

	DBG("%s inst:[%d]\n", __func__, phy->inst);

	/* Disable PHY clock valid interrupts while going into suspend*/
	if (phy->hot_plug) {
		val = readl(base + USB_SUSP_CTRL);
		val &= ~USB_PHY_CLK_VALID_INT_ENB;
		writel(val, base + USB_SUSP_CTRL);
	}

	val = readl(pmc_base + PMC_SLEEP_CFG);
	if (val & UTMIP_MASTER_ENABLE(phy->inst))
		utmip_phy_disable_pmc_bus_ctrl(phy);

	clk_put(phy->utmi_pad_clk);
}

static int utmi_phy_pad_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val, flags;
	void __iomem *pad_base =  IO_ADDRESS(TEGRA_USB_BASE);

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	tegra_clk_prepare_enable(phy->utmi_pad_clk);

	spin_lock_irqsave(&utmip_pad_lock, flags);
	utmip_pad_count++;

	val = readl(pad_base + UTMIP_BIAS_CFG0);
	val &= ~(UTMIP_OTGPD | UTMIP_BIASPD);
	val |= UTMIP_HSSQUELCH_LEVEL(0x2) | UTMIP_HSDISCON_LEVEL(0x1) |
		UTMIP_HSDISCON_LEVEL_MSB;
	writel(val, pad_base + UTMIP_BIAS_CFG0);

	spin_unlock_irqrestore(&utmip_pad_lock, flags);

	tegra_clk_disable_unprepare(phy->utmi_pad_clk);

	return 0;
}

static int utmi_phy_pad_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val, flags;
	void __iomem *pad_base =  IO_ADDRESS(TEGRA_USB_BASE);

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	tegra_clk_prepare_enable(phy->utmi_pad_clk);
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
	}
out:
	spin_unlock_irqrestore(&utmip_pad_lock, flags);
	tegra_clk_disable_unprepare(phy->utmi_pad_clk);

	return 0;
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

	usb_phy_fence_read(phy);
	/* check if there is any remote wake event */
	if (utmi_phy_remotewake_detected(phy)) {
		pr_info("%s: utmip remote wake detected\n", __func__);
		remote_wakeup = true;
	}

	if (phy->hot_plug) {
		val = readl(base + USB_SUSP_CTRL);
		if ((val  & USB_PHY_CLK_VALID_INT_STS)) {
			val &= ~USB_PHY_CLK_VALID_INT_ENB |
					USB_PHY_CLK_VALID_INT_STS;
			writel(val , (base + USB_SUSP_CTRL));

			/* In case of remote wakeup PHY clock will not up
			   immediately, so should not access any controller
			   register but normal plug-in/plug-out should be
			   executed */
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

static void utmi_phy_enable_obs_bus(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	/* (2LS WAR)is not required for LS and FS devices and is only for HS */
	if ((phy->port_speed == USB_PHY_PORT_SPEED_LOW) ||
		(phy->port_speed == USB_PHY_PORT_SPEED_FULL)) {
		/* do not enable the OBS bus */
		val = readl(base + UTMIP_MISC_CFG0);
		val &= ~(UTMIP_DPDM_OBSERVE_SEL(~0));
		writel(val, base + UTMIP_MISC_CFG0);
		DBG("%s(%d) Disable OBS bus\n", __func__, __LINE__);
		return;
	}
	/* Force DP/DM pulldown active for Host mode */
	val = readl(base + UTMIP_MISC_CFG0);
	val |= FORCE_PULLDN_DM | FORCE_PULLDN_DP |
			COMB_TERMS | ALWAYS_FREE_RUNNING_TERMS;
	writel(val, base + UTMIP_MISC_CFG0);
	val = readl(base + UTMIP_MISC_CFG0);
	val &= ~UTMIP_DPDM_OBSERVE_SEL(~0);
	if (phy->port_speed == USB_PHY_PORT_SPEED_LOW)
		val |= UTMIP_DPDM_OBSERVE_SEL_FS_J;
	else
		val |= UTMIP_DPDM_OBSERVE_SEL_FS_K;
	writel(val, base + UTMIP_MISC_CFG0);
	udelay(1);

	val = readl(base + UTMIP_MISC_CFG0);
	val |= UTMIP_DPDM_OBSERVE;
	writel(val, base + UTMIP_MISC_CFG0);
	udelay(10);
	DBG("%s(%d) Enable OBS bus\n", __func__, __LINE__);
	PHY_DBG("ENABLE_OBS_BUS\n");
}

static int utmi_phy_disable_obs_bus(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	unsigned long flags;

	/* check if OBS bus is already enabled */
	val = readl(base + UTMIP_MISC_CFG0);
	if (val & UTMIP_DPDM_OBSERVE) {
		PHY_DBG("DISABLE_OBS_BUS\n");

		/* disable ALL interrupts on current CPU */
		local_irq_save(flags);

		/* Change the UTMIP OBS bus to drive SE0 */
		val = readl(base + UTMIP_MISC_CFG0);
		val &= ~UTMIP_DPDM_OBSERVE_SEL(~0);
		val |= UTMIP_DPDM_OBSERVE_SEL_FS_SE0;
		writel(val, base + UTMIP_MISC_CFG0);

		/* Wait for 3us(2 LS bit times) */
		udelay(3);

		/* Release UTMIP OBS bus */
		val = readl(base + UTMIP_MISC_CFG0);
		val &= ~UTMIP_DPDM_OBSERVE;
		writel(val, base + UTMIP_MISC_CFG0);

		/* Release DP/DM pulldown for Host mode */
		val = readl(base + UTMIP_MISC_CFG0);
		val &= ~(FORCE_PULLDN_DM | FORCE_PULLDN_DP |
				COMB_TERMS | ALWAYS_FREE_RUNNING_TERMS);
		writel(val, base + UTMIP_MISC_CFG0);

		val = readl(base + USB_USBCMD);
		val |= USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);

		/* restore ALL interrupts on current CPU */
		local_irq_restore(flags);

		if (usb_phy_reg_status_wait(base + USB_USBCMD, USB_USBCMD_RS,
							 USB_USBCMD_RS, 2000)) {
			pr_err("%s: timeout waiting for USB_USBCMD_RS\n", __func__);
			return -ETIMEDOUT;
		}
	}
	return 0;
}

static int utmi_phy_post_resume(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned  int inst = phy->inst;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(pmc_base + PMC_SLEEP_CFG);
	/* if PMC is not disabled by now then disable it */
	if (val & UTMIP_MASTER_ENABLE(inst)) {
		utmip_phy_disable_pmc_bus_ctrl(phy);
	}

	utmi_phy_disable_obs_bus(phy);

	return 0;
}

static int phy_post_suspend(struct tegra_usb_phy *phy)
{

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	/* Need a 4ms delay for controller to suspend */
	mdelay(4);

	return 0;

}

static int utmi_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *base = phy->regs;
	unsigned  int inst = phy->inst;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	phy->port_speed = (readl(base + HOSTPC1_DEVLC) >> 25) &
			HOSTPC1_DEVLC_PSPD_MASK;

	if (phy->port_speed == USB_PHY_PORT_SPEED_HIGH) {
		/* Disable interrupts */
		writel(0, base + USB_USBINTR);
		/* Clear the run bit to stop SOFs - 2LS WAR */
		val = readl(base + USB_USBCMD);
		val &= ~USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);
		if (usb_phy_reg_status_wait(base + USB_USBSTS, USB_USBSTS_HCH,
							 USB_USBSTS_HCH, 2000)) {
			pr_err("%s: timeout waiting for USB_USBSTS_HCH\n", __func__);
		}
	}

	val = readl(pmc_base + PMC_SLEEP_CFG);
	if (val & UTMIP_MASTER_ENABLE(inst)) {
		if (!remote_wakeup)
			utmip_phy_disable_pmc_bus_ctrl(phy);
	} else {
		utmi_phy_enable_obs_bus(phy);
	}

	return 0;
}

static int utmi_phy_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	PHY_DBG("%s(%d) inst:[%d] BEGIN\n", __func__, __LINE__, phy->inst);
	if (!phy->phy_clk_on) {
		PHY_DBG("%s(%d) inst:[%d] phy clk is already off\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}

	if (phy->pdata->op_mode == TEGRA_USB_OPMODE_DEVICE) {
		utmip_powerdown_pmc_wake_detect(phy);

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

		/* Clear the run bit to stop SOFs - 2LS WAR */
		val = readl(base + USB_USBCMD);
		val &= ~USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);

		if (usb_phy_reg_status_wait(base + USB_USBSTS, USB_USBSTS_HCH,
							 USB_USBSTS_HCH, 2000)) {
			pr_err("%s: timeout waiting for USB_USBSTS_HCH\n", __func__);
		}
		utmip_setup_pmc_wake_detect(phy);

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

	utmi_phy_pad_power_off(phy);

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

	phy->phy_clk_on = false;
	phy->hw_accessible = false;

	PHY_DBG("%s(%d) inst:[%d] END\n", __func__, __LINE__, phy->inst);

	return 0;
}


static int utmi_phy_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	struct tegra_utmi_config *config = &phy->pdata->u_cfg.utmi;

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

	utmi_phy_pad_power_on(phy);

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
		val |= UTMIP_XCVR_HSSLEW_MSB(0x8);
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

	val = readl(base + HOSTPC1_DEVLC);
	val &= ~HOSTPC1_DEVLC_PHCD;
	writel(val, base + HOSTPC1_DEVLC);

	if (usb_phy_reg_status_wait(base + USB_SUSP_CTRL,
		USB_PHY_CLK_VALID, USB_PHY_CLK_VALID, 2500))
		pr_warn("%s: timeout waiting for phy to stabilize\n", __func__);

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
		utmip_powerup_pmc_wake_detect(phy);
	phy->phy_clk_on = true;
	phy->hw_accessible = true;
	PHY_DBG("%s(%d) End inst:[%d]\n", __func__, __LINE__, phy->inst);
	return 0;
}

static void utmi_phy_restore_start(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	int inst = phy->inst;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	val = readl(pmc_base + UTMIP_UHSIC_STATUS);
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
	} else if (!phy->pmc_remote_wakeup) {
		val = readl(pmc_base + PMC_SLEEP_CFG);
		if (val & UTMIP_MASTER_ENABLE(inst))
			utmip_phy_disable_pmc_bus_ctrl(phy);
	}

	utmi_phy_enable_obs_bus(phy);
}

static void utmi_phy_restore_end(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	int wait_time_us = 25000; /* FPR should be set by this time */

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	/* check whether we wake up from the remote resume */
	if (phy->pmc_remote_wakeup) {
		/* wait until SUSPEND and RESUME bit is cleared on remote resume */
		do {
			val = readl(base + USB_PORTSC);
			udelay(1);
			if (wait_time_us == 0) {
				PHY_DBG("%s PMC FPR timeout val = 0x%lx ",
							__func__, val);
				utmip_phy_disable_pmc_bus_ctrl(phy);
				utmi_phy_post_resume(phy);
				return;
			}
			wait_time_us--;
		} while (val & (USB_PORTSC_RESUME | USB_PORTSC_SUSP));

		/* wait for 25 ms to port resume complete */
		msleep(25);
		/* disable PMC master control */
		utmip_phy_disable_pmc_bus_ctrl(phy);

		/* Clear PCI and SRI bits to avoid an interrupt upon resume */
		val = readl(base + USB_USBSTS);
		writel(val, base + USB_USBSTS);
		/* wait to avoid SOF if there is any */
		if (usb_phy_reg_status_wait(base + USB_USBSTS,
			USB_USBSTS_SRI, USB_USBSTS_SRI, 2500) < 0) {
			pr_err("%s: timeout waiting for SOF\n", __func__);
		}
		utmi_phy_post_resume(phy);
	}
}

static int utmi_phy_resume(struct tegra_usb_phy *phy)
{
	int status = 0;
	unsigned long val;
	int port_connected = 0;
	int is_lp0;
	void __iomem *base = phy->regs;

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
			utmip_phy_disable_pmc_bus_ctrl(phy);

			/* device is plugged in when system is in LP0 */
			/* bring up the controller from LP0*/
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

static bool utmi_phy_charger_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	bool status;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (phy->pdata->op_mode != TEGRA_USB_OPMODE_DEVICE) {
		/* Charger detection is not there for ULPI
		 * return Charger not available */
		return false;
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

static void uhsic_powerup_pmc_wake_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

	/* turn on pad detectors for HSIC*/
	val = readl(pmc_base + PMC_USB_AO);
	val &= ~(HSIC_RESERVED_P0 | STROBE_VAL_PD_P0 | DATA_VAL_PD_P0);
	writel(val, pmc_base + PMC_USB_AO);

	/* Disable PMC master mode by clearing MASTER_EN */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~(UHSIC_MASTER_ENABLE_P0);
	writel(val, pmc_base + PMC_SLEEP_CFG);
	mdelay(1);
}

static void uhsic_powerdown_pmc_wake_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

	DBG("%s:%d\n", __func__, __LINE__);

	/* turn off pad detectors for HSIC*/
	val = readl(pmc_base + PMC_USB_AO);
	val |= (HSIC_RESERVED_P0 | STROBE_VAL_PD_P0 | DATA_VAL_PD_P0);
	writel(val, pmc_base + PMC_USB_AO);

	/* enable pull downs on HSIC PMC */
	val = UHSIC_STROBE_RPD_A | UHSIC_DATA_RPD_A | UHSIC_STROBE_RPD_B |
		UHSIC_DATA_RPD_B | UHSIC_STROBE_RPD_C | UHSIC_DATA_RPD_C |
		UHSIC_STROBE_RPD_D | UHSIC_DATA_RPD_D;
	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC);

	/* Turn over pad configuration to PMC */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UHSIC_WAKE_VAL_P0(~0);
	val |= UHSIC_WAKE_VAL_P0(WAKE_VAL_NONE) | UHSIC_MASTER_ENABLE_P0;
	writel(val, pmc_base + PMC_SLEEP_CFG);
}

static void uhsic_setup_pmc_wake_detect(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *base = phy->regs;
	bool port_connected;

	DBG("%s:%d\n", __func__, __LINE__);

	/* check for port connect status */
	val = readl(base + USB_PORTSC);
	port_connected = val & USB_PORTSC_CCS;

	if (!port_connected)
		return;

	/*Set PMC MASTER bits to do the following
	* a. Take over the hsic drivers
	* b. set up such that it will take over resume
	*	 if remote wakeup is detected
	* Prepare PMC to take over suspend-wake detect-drive resume until USB
	* controller ready
	*/

	/* disable master enable in PMC */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UHSIC_MASTER_ENABLE_P0;
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* UTMIP_PWR_PX=1 for power savings mode */
	val = readl(pmc_base + PMC_UTMIP_MASTER_CONFIG);
	val |= UHSIC_PWR;
	writel(val, pmc_base + PMC_UTMIP_MASTER_CONFIG);

	/* config debouncer */
	val = readl(pmc_base + PMC_USB_DEBOUNCE);
	val |= PMC_USB_DEBOUNCE_VAL(2);
	writel(val, pmc_base + PMC_USB_DEBOUNCE);

	/* Make sure nothing is happening on the line with respect to PMC */
	val = readl(pmc_base + PMC_UTMIP_UHSIC_FAKE);
	val &= ~UHSIC_STROBE_VAL;
	val &= ~UHSIC_DATA_VAL;
	writel(val, pmc_base + PMC_UTMIP_UHSIC_FAKE);

	/* Clear walk enable */
	val = readl(pmc_base + PMC_SLEEPWALK_CFG);
	val &= ~UHSIC_LINEVAL_WALK_EN;
	writel(val, pmc_base + PMC_SLEEPWALK_CFG);

	/* Make sure wake value for line is none */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UHSIC_WAKE_VAL(WAKE_VAL_ANY);
	val |= UHSIC_WAKE_VAL(WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* turn on pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val &= ~(STROBE_VAL_PD_P0 | DATA_VAL_PD_P0);
	writel(val, pmc_base + PMC_USB_AO);

	/* Add small delay before usb detectors provide stable line values */
	udelay(1);

	/* Enable which type of event can trigger a walk,
	* in this case usb_line_wake */
	val = readl(pmc_base + PMC_SLEEPWALK_CFG);
	val |= UHSIC_LINEVAL_WALK_EN;
	writel(val, pmc_base + PMC_SLEEPWALK_CFG);

	/* program walk sequence, maintain a J, followed by a driven K
	* to signal a resume once an wake event is detected */

	val = readl(pmc_base + PMC_SLEEPWALK_UHSIC);

	val &= ~UHSIC_DATA_RPU_A;
	val |=  UHSIC_DATA_RPD_A;
	val &= ~UHSIC_STROBE_RPD_A;
	val |=  UHSIC_STROBE_RPU_A;

	val &= ~UHSIC_DATA_RPD_B;
	val |=  UHSIC_DATA_RPU_B;
	val &= ~UHSIC_STROBE_RPU_B;
	val |=  UHSIC_STROBE_RPD_B;

	val &= ~UHSIC_DATA_RPD_C;
	val |=  UHSIC_DATA_RPU_C;
	val &= ~UHSIC_STROBE_RPU_C;
	val |=  UHSIC_STROBE_RPD_C;

	val &= ~UHSIC_DATA_RPD_D;
	val |=  UHSIC_DATA_RPU_D;
	val &= ~UHSIC_STROBE_RPU_D;
	val |=  UHSIC_STROBE_RPD_D;
	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC);

	phy->pmc_remote_wakeup = false;

	/* Setting Wake event*/
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UHSIC_WAKE_VAL(WAKE_VAL_ANY);
	val |= UHSIC_WAKE_VAL(WAKE_VAL_SD10);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* Clear the walk pointers and wake alarm */
	val = readl(pmc_base + PMC_TRIGGERS);
	val |= UHSIC_CLR_WAKE_ALARM_P0 | UHSIC_CLR_WALK_PTR_P0;
	writel(val, pmc_base + PMC_TRIGGERS);

	/* Turn over pad configuration to PMC  for line wake events*/
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val |= UHSIC_MASTER_ENABLE;
	writel(val, pmc_base + PMC_SLEEP_CFG);

	val = readl(base + UHSIC_PMC_WAKEUP0);
	val |= EVENT_INT_ENB;
	writel(val, base + UHSIC_PMC_WAKEUP0);

	PHY_DBG("%s ENABLE_PMC\n", __func__);
}

static void uhsic_phy_pmc_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *base = phy->regs;
	bool port_connected;

	DBG("%s:%d\n", __func__, __LINE__);

	/* check for port connect status */
	val = readl(base + USB_PORTSC);
	port_connected = val & USB_PORTSC_CCS;

	if (!port_connected)
		return;

	/* Make sure wake value for line is none */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UHSIC_WAKE_VAL(WAKE_VAL_ANY);
	val |= UHSIC_WAKE_VAL(WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_SLEEP_CFG);


	/* turn on pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val &= ~(STROBE_VAL_PD_P0 | DATA_VAL_PD_P0);
	writel(val, pmc_base + PMC_USB_AO);

	/* Add small delay before usb detectors provide stable line values */
	udelay(1);

	/* If it is during remote wakeup, set resume state on the BUS.
	   If it is during AP resume, set suspend state on the BUS. */
	val = readl(pmc_base + PMC_SLEEPWALK_UHSIC);

	if (remote_wakeup) {
		/* Switching PMC from SUSPEND to resume.*/
		val &= ~UHSIC_DATA_RPD_A;
		val |=  UHSIC_DATA_RPU_A;
		val &= ~UHSIC_STROBE_RPU_A;
		val |=  UHSIC_STROBE_RPD_A;
	} else {
		val &= ~UHSIC_DATA_RPU_A;
		val |=  UHSIC_DATA_RPD_A;
		val &= ~UHSIC_STROBE_RPD_A;
		val |=  UHSIC_STROBE_RPU_A;
	}

	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC);

	/* Turn over pad configuration to PMC  for line wake events*/
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val |= UHSIC_MASTER_ENABLE;
	writel(val, pmc_base + PMC_SLEEP_CFG);

}

static void uhsic_phy_disable_pmc_bus_ctrl(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *base = phy->regs;

	DBG("%s (%d)\n", __func__, __LINE__);
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~UHSIC_WAKE_VAL(WAKE_VAL_ANY);
	val |= UHSIC_WAKE_VAL(WAKE_VAL_NONE);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	val = readl(base + UHSIC_PMC_WAKEUP0);
	val &= ~EVENT_INT_ENB;
	writel(val, base + UHSIC_PMC_WAKEUP0);

	/* Disable PMC master mode by clearing MASTER_EN */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~(UHSIC_MASTER_ENABLE);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* turn off pad detectors */
	val = readl(pmc_base + PMC_USB_AO);
	val |= (STROBE_VAL_PD_P0 | DATA_VAL_PD_P0);
	writel(val, pmc_base + PMC_USB_AO);

	val = readl(pmc_base + PMC_TRIGGERS);
	val |= (UHSIC_CLR_WALK_PTR_P0 | UHSIC_CLR_WAKE_ALARM_P0);
	writel(val, pmc_base + PMC_TRIGGERS);

	phy->pmc_remote_wakeup = false;
}

static bool uhsic_phy_remotewake_detected(struct tegra_usb_phy *phy)
{
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *base = phy->regs;
	u32 val;

	val = readl(base + UHSIC_PMC_WAKEUP0);
	if (val & EVENT_INT_ENB) {
		val = readl(pmc_base + UTMIP_UHSIC_STATUS);
		if (UHSIC_WAKE_ALARM & val) {
			val = readl(pmc_base + PMC_SLEEP_CFG);
			val &= ~UHSIC_WAKE_VAL(WAKE_VAL_ANY);
			val |= UHSIC_WAKE_VAL(WAKE_VAL_NONE);
			writel(val, pmc_base + PMC_SLEEP_CFG);

			val = readl(pmc_base + PMC_TRIGGERS);
			val |= UHSIC_CLR_WAKE_ALARM_P0;
			writel(val, pmc_base + PMC_TRIGGERS);

			val = readl(base + UHSIC_PMC_WAKEUP0);
			val &= ~EVENT_INT_ENB;
			writel(val, base + UHSIC_PMC_WAKEUP0);
			phy->pmc_remote_wakeup = true;
			DBG("%s:PMC remote wakeup detected for HSIC\n", __func__);
			return true;
		}
	}
	return false;
}

static int uhsic_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
{
	void __iomem *base = phy->regs;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned long val;

	DBG("%s(%d)\n", __func__, __LINE__);

	/* Clear the run bit to stop SOFs - 2LS WAR */
	val = readl(base + USB_USBCMD);
	val &= ~USB_USBCMD_RS;
	writel(val, base + USB_USBCMD);

	if (usb_phy_reg_status_wait(base + USB_USBSTS, USB_USBSTS_HCH,
						 USB_USBSTS_HCH, 2000)) {
		pr_err("%s: timeout waiting for USB_USBSTS_HCH\n", __func__);
	}

	/* disable USB interrupts */
	writel(0, base + USB_USBINTR);

	/* If PMC is not enabled, enable it. */
	val = readl(pmc_base + PMC_SLEEP_CFG);
	if (!(val & UHSIC_MASTER_ENABLE_P0)) {
		/** set PMC lines in suspend and enable the master */
		uhsic_phy_pmc_resume(phy, remote_wakeup);
	} else {
		val = readl(pmc_base + PMC_SLEEP_CFG);
		val &= ~UHSIC_WAKE_VAL(WAKE_VAL_ANY);
		val |= UHSIC_WAKE_VAL(WAKE_VAL_NONE);
		writel(val, pmc_base + PMC_SLEEP_CFG);
	}

	val = readl(base + UHSIC_PADS_CFG1);
	val |= UHSIC_PD_TX;
	writel(val, base + UHSIC_PADS_CFG1);

	/* Driving Resume using PMC */
	val = readl(pmc_base + PMC_SLEEPWALK_UHSIC);
	val &= ~UHSIC_DATA_RPD_A;
	val |=  UHSIC_DATA_RPU_A;
	val &= ~UHSIC_STROBE_RPU_A;
	val |=  UHSIC_STROBE_RPD_A;
	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC);
	return 0;
}

static int uhsic_phy_post_resume(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned long flags;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	local_irq_save(flags);
	val = readl(pmc_base + PMC_SLEEPWALK_UHSIC);
	val &= ~(UHSIC_DATA_RPU_A | UHSIC_STROBE_RPD_A);
	val |= (UHSIC_DATA_RPD_A | UHSIC_STROBE_RPU_A);
	writel(val, pmc_base + PMC_SLEEPWALK_UHSIC);

	udelay(3);

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~UHSIC_PD_TX;
	writel(val, base + UHSIC_PADS_CFG1);

	val = readl(pmc_base + PMC_SLEEP_CFG);
	val &= ~(UHSIC_MASTER_ENABLE);
	writel(val, pmc_base + PMC_SLEEP_CFG);

	/* clear pending SRI, PCI interrupts. ehci will enable interrupts */
	val = readl(base + USB_USBSTS);
	writel(val, base + USB_USBSTS);

	val = readl(base + USB_USBCMD);
	val |= USB_USBCMD_RS;
	writel(val, base + USB_USBCMD);
	local_irq_restore(flags);

	uhsic_phy_disable_pmc_bus_ctrl(phy);

	val = readl(base + USB_TXFILLTUNING);
	if ((val & USB_FIFO_TXFILL_MASK) != USB_FIFO_TXFILL_THRES(0x10)) {
		val = USB_FIFO_TXFILL_THRES(0x10);
		writel(val, base + USB_TXFILLTUNING);
	}
	return 0;
}

static void uhsic_phy_restore_start(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	void __iomem *base = phy->regs;

	val = readl(pmc_base + UTMIP_UHSIC_STATUS);

	/* check whether we wake up from the remote resume */
	if (UHSIC_WALK_PTR_VAL & val) {
		phy->pmc_remote_wakeup = true;
	} else {
		DBG("%s(%d): setting pretend connect\n", __func__, __LINE__);
		val = readl(base + UHSIC_CMD_CFG0);
		val |= UHSIC_PRETEND_CONNECT_DETECT;
		writel(val, base + UHSIC_CMD_CFG0);
	}
}

static void uhsic_phy_restore_end(struct tegra_usb_phy *phy)
{

	unsigned long val;
	void __iomem *base = phy->regs;
	int wait_time_us = 25000; /* FPR should be set by this time */

	DBG("%s(%d)\n", __func__, __LINE__);

	/* check whether we wake up from the remote resume */
	if (phy->pmc_remote_wakeup) {
		/* wait until FPR bit is set automatically on remote resume */
		do {
			val = readl(base + USB_PORTSC);
			udelay(1);
			if (wait_time_us == 0) {
				uhsic_phy_disable_pmc_bus_ctrl(phy);
				uhsic_phy_post_resume(phy);
				return;
			}
			wait_time_us--;
		} while (val & (USB_PORTSC_RESUME | USB_PORTSC_SUSP));

		/* Clear PCI and SRI bits to avoid an interrupt upon resume */
		val = readl(base + USB_USBSTS);
		writel(val, base + USB_USBSTS);
		/* wait to avoid SOF if there is any */
		if (usb_phy_reg_status_wait(base + USB_USBSTS,
			USB_USBSTS_SRI, USB_USBSTS_SRI, 2500)) {
			pr_warn("%s: timeout waiting for SOF\n", __func__);
		}
		uhsic_phy_post_resume(phy);
	}
}

static int hsic_rail_enable(struct tegra_usb_phy *phy)
{
	int ret;

	if (phy->hsic_reg == NULL) {
		phy->hsic_reg = regulator_get(&phy->pdev->dev, "avdd_hsic");
		if (IS_ERR_OR_NULL(phy->hsic_reg)) {
			pr_err("HSIC: Could not get regulator avdd_hsic\n");
			ret = PTR_ERR(phy->hsic_reg);
			phy->hsic_reg = NULL;
			return ret;
		}
	}

	ret = regulator_enable(phy->hsic_reg);
	if (ret < 0) {
		pr_err("%s avdd_hsic could not be enabled\n", __func__);
		return ret;
	}

	return 0;
}

static int hsic_rail_disable(struct tegra_usb_phy *phy)
{
	int ret;

	if (phy->hsic_reg == NULL) {
		pr_warn("%s: unbalanced disable\n", __func__);
		return -EIO;
	}

	ret = regulator_disable(phy->hsic_reg);
	if (ret < 0) {
		pr_err("HSIC regulator avdd_hsic cannot be disabled\n");
		return ret;
	}

	return 0;
}

static int uhsic_phy_open(struct tegra_usb_phy *phy)
{
	unsigned long parent_rate;
	int i;
	int ret;

	phy->hsic_reg = NULL;
	ret = hsic_rail_enable(phy);
	if (ret < 0) {
		pr_err("%s avdd_hsic could not be enabled\n", __func__);
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

	uhsic_powerup_pmc_wake_detect(phy);

	return 0;
}

static void uhsic_phy_close(struct tegra_usb_phy *phy)
{
	int ret;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	uhsic_powerdown_pmc_wake_detect(phy);

	ret = hsic_rail_disable(phy);
	if (ret < 0)
		pr_err("%s avdd_hsic could not be disabled\n", __func__);
}

static int uhsic_phy_irq(struct tegra_usb_phy *phy)
{
	usb_phy_fence_read(phy);
	/* check if there is any remote wake event */
	if (uhsic_phy_remotewake_detected(phy))
		pr_info("%s: uhsic remote wake detected\n", __func__);
	return IRQ_HANDLED;
}

static int uhsic_phy_power_on(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already On\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~(UHSIC_PD_BG | UHSIC_PD_TRK | UHSIC_PD_RX |
			UHSIC_PD_ZI | UHSIC_RPD_DATA | UHSIC_RPD_STROBE);
	val |= (UHSIC_RX_SEL | UHSIC_PD_TX);
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
	/* Disable generic bus reset, to allow AP30 specific bus reset*/
	val |= UHSIC_DISABLE_BUSRESET;
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

	val = readl(base + USB_USBMODE);
	val |= USB_USBMODE_HOST;
	writel(val, base + USB_USBMODE);

	/* Change the USB controller PHY type to HSIC */
	val = readl(base + HOSTPC1_DEVLC);
	val &= ~HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_MASK);
	val |= HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_HSIC);
	val &= ~HOSTPC1_DEVLC_STS;
	writel(val, base + HOSTPC1_DEVLC);

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
	/* set Rtune impedance to 50 ohm */
	val |= UHSIC_TX_RTUNE(8);
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

	return 0;
}

static int uhsic_phy_power_off(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
	if (!phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already off\n",
					__func__, __LINE__, phy->inst);
		return 0;
	}

	/* Disable interrupts */
	writel(0, base + USB_USBINTR);

	if (phy->pmc_sleepwalk == false) {
		uhsic_setup_pmc_wake_detect(phy);
		phy->pmc_sleepwalk = true;
	}

	val = readl(base + HOSTPC1_DEVLC);
	val |= HOSTPC1_DEVLC_PHCD;
	writel(val, base + HOSTPC1_DEVLC);

	/* Remove power downs for HSIC from PADS CFG1 register */
	val = readl(base + UHSIC_PADS_CFG1);
	val |= (UHSIC_PD_BG |UHSIC_PD_TRK | UHSIC_PD_RX |
			UHSIC_PD_ZI | UHSIC_PD_TX);
	writel(val, base + UHSIC_PADS_CFG1);
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
	writel(val, base + HOSTPC1_DEVLC);

	val = readl(base + UHSIC_MISC_CFG0);
	val |= UHSIC_DETECT_SHORT_CONNECT;
	writel(val, base + UHSIC_MISC_CFG0);
	udelay(1);

	val = readl(base + UHSIC_MISC_CFG0);
	val |= UHSIC_FORCE_XCVR_MODE;
	writel(val, base + UHSIC_MISC_CFG0);

	val = readl(base + UHSIC_PADS_CFG1);
	val &= ~UHSIC_RPD_STROBE;
	writel(val, base + UHSIC_PADS_CFG1);

	if (phy->pdata->ops && phy->pdata->ops->port_power)
		phy->pdata->ops->port_power();

	if (usb_phy_reg_status_wait(base + UHSIC_STAT_CFG0,
			UHSIC_CONNECT_DETECT, UHSIC_CONNECT_DETECT, 25000)) {
		pr_err("%s: timeout waiting for UHSIC_CONNECT_DETECT\n",
								__func__);
		return -ETIMEDOUT;
	}

	return 0;
}

static int uhsic_phy_bus_reset(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	/* Change the USB controller PHY type to HSIC */
	val = readl(base + HOSTPC1_DEVLC);
	val &= ~HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_MASK);
	val |= HOSTPC1_DEVLC_PTS(HOSTPC1_DEVLC_PTS_HSIC);
	val &= ~HOSTPC1_DEVLC_STS;
	writel(val, base + HOSTPC1_DEVLC);
	/* wait here, otherwise HOSTPC1_DEVLC_PSPD will timeout */
	mdelay(5);

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

	if (usb_phy_reg_status_wait(base + HOSTPC1_DEVLC,
			HOSTPC1_DEVLC_PSPD(2),
			HOSTPC1_DEVLC_PSPD(2), 2000) < 0) {
		pr_err("%s: timeout waiting hsic high speed configuration\n",
						__func__);
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

int uhsic_phy_resume(struct tegra_usb_phy *phy)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

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
	unsigned int val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	if (!phy->phy_clk_on) {
		DBG("%s(%d) inst:[%d] phy clk is already off\n", __func__,
							__LINE__, phy->inst);
		return 0;
	}

	phy->phy_clk_on = false;
	phy->hw_accessible = false;
	ulpi_null_phy_set_tristate(true);
	val = readl(base + ULPIS2S_CTRL);
	val &= ~ULPIS2S_PLLU_MASTER_BLASTER60;
	writel(val, base + ULPIS2S_CTRL);
	return 0;
}

/* NOTE: this function must be called before ehci reset */
static int ulpi_null_phy_init(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);
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
	unsigned long val;
	void __iomem *base = phy->regs;

	usb_phy_fence_read(phy);
	if (phy->bus_reseting){
		val = readl(base + USB_USBCMD);
		val |= USB_USBCMD_RS;
		writel(val, base + USB_USBCMD);
		phy->bus_reseting = false;
	}
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

static int ulpi_phy_bus_reset(struct tegra_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	/*DISABLE RUN BIT */

	val = readl(base + USB_USBCMD);
	val &= ~USB_USBCMD_RS;
	writel(val, base + USB_USBCMD);
	phy->bus_reseting = true;

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
		mdelay(1);
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

	phy->bus_reseting = false;
	phy->phy_clk_on = true;
	phy->hw_accessible = true;

	return 0;
}

static int ulpi_null_phy_pre_resume(struct tegra_usb_phy *phy, bool remote_wakeup)
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
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
		/* remove DIR tristate */
		tegra_pinmux_set_tristate(TEGRA_PINGROUP_ULPI_DIR,
					  TEGRA_TRI_NORMAL);
#endif
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
	.close		= uhsic_phy_close,
	.irq		= uhsic_phy_irq,
	.power_on	= uhsic_phy_power_on,
	.power_off	= uhsic_phy_power_off,
	.pre_resume = uhsic_phy_pre_resume,
	.resume = uhsic_phy_resume,
	.post_resume = uhsic_phy_post_resume,
	.port_power = uhsic_phy_bus_port_power,
	.bus_reset	= uhsic_phy_bus_reset,
	.post_suspend   = phy_post_suspend,
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
	.post_suspend   = phy_post_suspend,
	.bus_reset	= ulpi_phy_bus_reset,
};

static struct tegra_usb_phy_ops ulpi_link_phy_ops;
static struct tegra_usb_phy_ops icusb_phy_ops;

static struct tegra_usb_phy_ops *phy_ops[] = {
	[TEGRA_USB_PHY_INTF_UTMI] = &utmi_phy_ops,
	[TEGRA_USB_PHY_INTF_ULPI_LINK] = &ulpi_link_phy_ops,
	[TEGRA_USB_PHY_INTF_ULPI_NULL] = &ulpi_null_phy_ops,
	[TEGRA_USB_PHY_INTF_HSIC] = &uhsic_phy_ops,
	[TEGRA_USB_PHY_INTF_ICUSB] = &icusb_phy_ops,
};

int tegra3_usb_phy_init_ops(struct tegra_usb_phy *phy)
{
	DBG("%s(%d) inst:[%d]\n", __func__, __LINE__, phy->inst);

	phy->ops = phy_ops[phy->pdata->phy_intf];

	/* FIXME: uncommenting below line to make USB host mode fail*/
	/* usb_phy_power_down_pmc(); */

	return 0;
}

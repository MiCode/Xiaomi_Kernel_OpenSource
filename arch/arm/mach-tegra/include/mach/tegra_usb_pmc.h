/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_USB_PMC_INTERFACE_H_
#define _TEGRA_USB_PMC_INTERFACE_H_

#define UHSIC_INST(inst, x, y)	((inst == 1) ? x : y)

#define USB_PORTSC		0x174
#define USB_PORTSC_PHCD	(1 << 23)
#define USB_PORTSC_WKOC	(1 << 22)
#define USB_PORTSC_WKDS	(1 << 21)
#define USB_PORTSC_WKCN	(1 << 20)
#define USB_PORTSC_PTC(x)	(((x) & 0xf) << 16)
#define USB_PORTSC_PP	(1 << 12)
#define USB_PORTSC_LS(x) (((x) & 0x3) << 10)
#define USB_PORTSC_SUSP	(1 << 7)
#define USB_PORTSC_RESUME	(1 << 6)
#define USB_PORTSC_OCC	(1 << 5)
#define USB_PORTSC_PEC	(1 << 3)
#define USB_PORTSC_PE		(1 << 2)
#define USB_PORTSC_CSC	(1 << 1)
#define USB_PORTSC_CCS	(1 << 0)
#define USB_PORTSC_RWC_BITS (USB_PORTSC_CSC | USB_PORTSC_PEC | USB_PORTSC_OCC)
#define USB_PORTSC_PSPD_MASK	3
#define USB_PORTSC_LINE_STATE(x) (((x) & (0x3 << 10)) >> 10)
#define USB_PORTSC_LINE_DM_SET (1 << 0)
#define USB_PORTSC_LINE_DP_SET (1 << 1)

#define USB_USBCMD		0x130
#define   USB_USBCMD_RS		(1 << 0)
#define   USB_CMD_RESET	(1<<1)

#define HOSTPC1_DEVLC		0x1b4
#define HOSTPC1_DEVLC_PHCD		(1 << 22)
#define HOSTPC1_DEVLC_PTS(x)		(((x) & 0x7) << 29)
#define HOSTPC1_DEVLC_PTS_MASK	7
#define HOSTPC1_DEVLC_PTS_HSIC	4
#define HOSTPC1_DEVLC_STS		(1 << 28)
#define HOSTPC1_DEVLC_PSPD(x)		(((x) & 0x3) << 25)
#define HOSTPC1_DEVLC_PSPD_MASK	3
#define HOSTPC1_DEVLC_PSPD_HIGH_SPEED 2
#define HOSTPC1_DEVLC_NYT_ASUS	1

#define PMC_UHSIC_SLEEP_CFG(inst)	UHSIC_INST(inst, 0x1fc, 0x284)
#define UHSIC_MASTER_ENABLE(inst)	(1 << UHSIC_INST(inst, 24, 0))
#define UHSIC_WAKE_VAL(inst, x)	(((x) & 0xf) << UHSIC_INST(inst, 28, 4))
#define WAKE_VAL_SD10			0x2
#define UTMIP_PMC_WAKEUP0		0x84c
#define UHSIC_PMC_WAKEUP0		0xc34
#define EVENT_INT_ENB			(1 << 0)
#define PMC_UHSIC_TRIGGERS(inst)	UHSIC_INST(inst, 0x1ec, 0x27c)
#define UHSIC_CLR_WALK_PTR(inst)	(1 << UHSIC_INST(inst, 3, 0))
#define UHSIC_CLR_WAKE_ALARM(inst)	(1 << UHSIC_INST(inst, 15, 3))

#define PMC_UHSIC_SLEEPWALK_CFG(inst)	UHSIC_INST(inst, 0x200, 0x288)
#define UHSIC_LINEVAL_WALK_EN(inst)	(1 << UHSIC_INST(inst, 31, 7))
#define PMC_UHSIC_MASTER_CONFIG(inst)	UHSIC_INST(inst, 0x274, 0x29c)
#define UHSIC_PWR(inst)		(1 << UHSIC_INST(inst, 3, 0))

#define PMC_UHSIC_FAKE(inst)		UHSIC_INST(inst, 0x218, 0x294)
#define UHSIC_FAKE_STROBE_VAL(inst)		(1 << UHSIC_INST(inst, 12, 0))
#define UHSIC_FAKE_DATA_VAL(inst)		(1 << UHSIC_INST(inst, 13, 1))

#define PMC_SLEEPWALK_UHSIC(inst)	UHSIC_INST(inst, 0x210, 0x28c)
#define UHSIC_STROBE_RPD_A				(1 << 0)
#define UHSIC_DATA_RPD_A				(1 << 1)
#define UHSIC_STROBE_RPU_A				(1 << 2)
#define UHSIC_DATA_RPU_A				(1 << 3)
#define UHSIC_STROBE_RPD_B				(1 << 8)
#define UHSIC_DATA_RPD_B				(1 << 9)
#define UHSIC_STROBE_RPU_B				(1 << 10)
#define UHSIC_DATA_RPU_B				(1 << 11)
#define UHSIC_STROBE_RPD_C				(1 << 16)
#define UHSIC_DATA_RPD_C				(1 << 17)
#define UHSIC_STROBE_RPU_C				(1 << 18)
#define UHSIC_DATA_RPU_C				(1 << 19)
#define UHSIC_STROBE_RPD_D				(1 << 24)
#define UHSIC_DATA_RPD_D				(1 << 25)
#define UHSIC_STROBE_RPU_D				(1 << 26)
#define UHSIC_DATA_RPU_D				(1 << 27)
#define UHSIC_LINE_DEB_CNT(x)			(((x) & 0xf) << 20)

#define PMC_USB_DEBOUNCE			0xec
#define UTMIP_LINE_DEB_CNT(x)			(((x) & 0xf) << 16)
#define PMC_USB_DEBOUNCE_VAL(x)		((x) & 0xffff)

#define PMC_USB_AO				0xf0
#define HSIC_RESERVED(inst)			(3 << UHSIC_INST(inst, 14, 18))
#define STROBE_VAL_PD(inst)			(1 << UHSIC_INST(inst, 12, 16))
#define DATA_VAL_PD(inst)			(1 << UHSIC_INST(inst, 13, 17))
#define PMC_POWER_DOWN_MASK			0xffff
#define USB_ID_PD(inst)				(1 << ((4*(inst))+3))
#define VBUS_WAKEUP_PD(inst)			(1 << ((4*(inst))+2))
#define USBON_VAL_PD(inst)			(1 << ((4*(inst))+1))
#define USBON_VAL_PD_P2			(1 << 9)
#define USBON_VAL_PD_P1			(1 << 5)
#define USBON_VAL_PD_P0			(1 << 1)
#define USBOP_VAL_PD(inst)			(1 << (4*(inst)))
#define USBOP_VAL_PD_P2			(1 << 8)
#define USBOP_VAL_PD_P1			(1 << 4)
#define USBOP_VAL_PD_P0			(1 << 0)
#define PMC_USB_AO_PD_P2			(0xf << 8)
#define PMC_USB_AO_ID_PD_P0			(1 << 3)
#define PMC_USB_AO_VBUS_WAKEUP_PD_P0	(1 << 2)

#define PMC_TRIGGERS			0x1ec
#define UTMIP_CLR_WALK_PTR(inst)	(1 << (inst))
#define UTMIP_CLR_WALK_PTR_P2		(1 << 2)
#define UTMIP_CLR_WALK_PTR_P1		(1 << 1)
#define UTMIP_CLR_WALK_PTR_P0		(1 << 0)
#define UTMIP_CAP_CFG(inst)	(1 << ((inst)+4))
#define UTMIP_CAP_CFG_P2		(1 << 6)
#define UTMIP_CAP_CFG_P1		(1 << 5)
#define UTMIP_CAP_CFG_P0		(1 << 4)
#define UTMIP_CLR_WAKE_ALARM(inst)		(1 << ((inst)+12))
#define UTMIP_CLR_WAKE_ALARM_P2	(1 << 14)

#define PMC_PAD_CFG		(0x1f4)

#define PMC_UTMIP_TERM_PAD_CFG	0x1f8
#define PMC_TCTRL_VAL(x)	(((x) & 0x1f) << 5)
#define PMC_RCTRL_VAL(x)	(((x) & 0x1f) << 0)

#define PMC_SLEEP_CFG			0x1fc
#define UTMIP_TCTRL_USE_PMC(inst) (1 << ((8*(inst))+3))
#define UTMIP_TCTRL_USE_PMC_P2		(1 << 19)
#define UTMIP_TCTRL_USE_PMC_P1		(1 << 11)
#define UTMIP_TCTRL_USE_PMC_P0		(1 << 3)
#define UTMIP_RCTRL_USE_PMC(inst) (1 << ((8*(inst))+2))
#define UTMIP_RCTRL_USE_PMC_P2		(1 << 18)
#define UTMIP_RCTRL_USE_PMC_P1		(1 << 10)
#define UTMIP_RCTRL_USE_PMC_P0		(1 << 2)
#define UTMIP_FSLS_USE_PMC(inst)	(1 << ((8*(inst))+1))
#define UTMIP_FSLS_USE_PMC_P2		(1 << 17)
#define UTMIP_FSLS_USE_PMC_P1		(1 << 9)
#define UTMIP_FSLS_USE_PMC_P0		(1 << 1)
#define UTMIP_MASTER_ENABLE(inst) (1 << (8*(inst)))
#define UTMIP_MASTER_ENABLE_P2		(1 << 16)
#define UTMIP_MASTER_ENABLE_P1		(1 << 8)
#define UTMIP_MASTER_ENABLE_P0		(1 << 0)

#define PMC_SLEEPWALK_CFG		0x200
#define UTMIP_LINEVAL_WALK_EN(inst) (1 << ((8*(inst))+7))
#define UTMIP_LINEVAL_WALK_EN_P2	(1 << 23)
#define UTMIP_LINEVAL_WALK_EN_P1	(1 << 15)
#define UTMIP_LINEVAL_WALK_EN_P0	(1 << 7)
#define UTMIP_WAKE_VAL(inst, x) (((x) & 0xf) << ((8*(inst))+4))
#define UTMIP_WAKE_VAL_P2(x)		(((x) & 0xf) << 20)
#define UTMIP_WAKE_VAL_P1(x)		(((x) & 0xf) << 12)
#define UTMIP_WAKE_VAL_P0(x)		(((x) & 0xf) << 4)
#define WAKE_VAL_NONE		0xc
#define WAKE_VAL_ANY			0xF
#define WAKE_VAL_FSJ			0x2
#define WAKE_VAL_FSK			0x1
#define WAKE_VAL_SE0			0x0

#define PMC_SLEEPWALK_REG(inst)		(0x204 + (4*(inst)))
#define UTMIP_USBOP_RPD_A	(1 << 0)
#define UTMIP_USBON_RPD_A	(1 << 1)
#define UTMIP_AP_A			(1 << 4)
#define UTMIP_AN_A			(1 << 5)
#define UTMIP_HIGHZ_A		(1 << 6)
#define UTMIP_USBOP_RPD_B	(1 << 8)
#define UTMIP_USBON_RPD_B	(1 << 9)
#define UTMIP_AP_B			(1 << 12)
#define UTMIP_AN_B			(1 << 13)
#define UTMIP_HIGHZ_B		(1 << 14)
#define UTMIP_USBOP_RPD_C	(1 << 16)
#define UTMIP_USBON_RPD_C	(1 << 17)
#define UTMIP_AP_C		(1 << 20)
#define UTMIP_AN_C		(1 << 21)
#define UTMIP_HIGHZ_C		(1 << 22)
#define UTMIP_USBOP_RPD_D	(1 << 24)
#define UTMIP_USBON_RPD_D	(1 << 25)
#define UTMIP_AP_D		(1 << 28)
#define UTMIP_AN_D		(1 << 29)
#define UTMIP_HIGHZ_D		(1 << 30)

#define PMC_UTMIP_FAKE		0x218
#define USBON_VAL(inst)	(1 << ((4*(inst))+1))
#define USBON_VAL_P2			(1 << 9)
#define USBON_VAL_P1			(1 << 5)
#define USBON_VAL_P0			(1 << 1)
#define USBOP_VAL(inst)	(1 << (4*(inst)))
#define USBOP_VAL_P2			(1 << 8)
#define USBOP_VAL_P1			(1 << 4)
#define USBOP_VAL_P0			(1 << 0)

#define PMC_UTMIP_BIAS_MASTER_CNTRL 0x270
#define BIAS_MASTER_PROG_VAL		(1 << 1)

#define UTMIP_BIAS_CFG1		0x83c
#define   UTMIP_BIAS_PDTRK_COUNT(x) (((x) & 0x1f) << 3)
#define   UTMIP_BIAS_PDTRK_POWERDOWN	(1 << 0)
#define   UTMIP_BIAS_PDTRK_POWERUP	(1 << 1)

#define UTMIP_BIAS_STS0			0x840
#define   UTMIP_RCTRL_VAL(x)		(((x) & 0xffff) << 0)
#define   UTMIP_TCTRL_VAL(x)		(((x) & (0xffff << 16)) >> 16)

#define PMC_UTMIP_MASTER_CONFIG		0x274
#define UTMIP_PWR(inst)		(1 << (inst))

#ifdef DEBUG
#define DBG(stuff...)	pr_info("\n"stuff)
#else
#define DBG(stuff...)	do {} while (0)
#endif

#if 0
#define PHY_DBG(stuff...)	pr_info("\n" stuff)
#else
#define PHY_DBG(stuff...)	do {} while (0)
#endif

/**
 * defines USB controller type
 */
enum tegra_usb_controller_type {
	TEGRA_USB_2_0 = 0,
	TEGRA_USB_3_0,
};

/**
 * defines USB port speeds
 */
enum tegra_usb_port_speed {
	USB_PMC_PORT_SPEED_FULL = 0,
	USB_PMC_PORT_SPEED_LOW,
	USB_PMC_PORT_SPEED_HIGH,
	USB_PMC_PORT_SPEED_UNKNOWN,
	USB_PMC_PORT_SPEED_SUPER,
};

struct tegra_usb_pmc_data;

/**
 * defines function pointers used for differnt pmc operations
 */
struct tegra_usb_pmc_ops {
	void (*setup_pmc_wake_detect)(struct tegra_usb_pmc_data *pmc_data);
	void (*powerup_pmc_wake_detect)(struct tegra_usb_pmc_data *pmc_data);
	void (*powerdown_pmc_wake_detect)(struct tegra_usb_pmc_data *pmc_data);
	void (*disable_pmc_bus_ctrl)(struct tegra_usb_pmc_data *pmc_data,
		  int enable_sof);
	void (*power_down_pmc)(struct tegra_usb_pmc_data *pmc_data);
};

/**
 * defines a structure that can be used by USB and XUSB for handing PMC ops.
 */
struct tegra_usb_pmc_data {
	u8 instance;
	enum tegra_usb_controller_type controller_type;
	enum tegra_usb_phy_interface phy_type;
	enum tegra_usb_port_speed port_speed;
	struct tegra_usb_pmc_ops *pmc_ops;
	bool is_xhci;
	void __iomem *usb_base;
};

void tegra_usb_pmc_init(struct tegra_usb_pmc_data *pmc_data);
int utmi_phy_set_snps_trking_data(void);
void utmi_phy_update_trking_data(u32 tctrl, u32 rctrl);
void tegra_usb_pmc_reg_update(u32 reg_offset, u32 mask, u32 val);
u32 tegra_usb_pmc_reg_read(u32 reg_offset);
void tegra_usb_pmc_reg_write(u32 reg_offset, u32 val);

#endif

/* linux/include/mach/hsusb.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#ifndef __ASM_ARCH_MSM_HSUSB_H
#define __ASM_ARCH_MSM_HSUSB_H

#include <linux/types.h>
#include <linux/pm_qos_params.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#define PHY_TYPE_MASK		0x0F
#define PHY_TYPE_MODE		0xF0
#define PHY_MODEL_MASK		0xFF00
#define PHY_TYPE(x)		((x) & PHY_TYPE_MASK)
#define PHY_MODEL(x)		((x) & PHY_MODEL_MASK)

#define USB_PHY_MODEL_65NM	0x100
#define USB_PHY_MODEL_180NM	0x200
#define USB_PHY_MODEL_45NM	0x400
#define USB_PHY_UNDEFINED	0x00
#define USB_PHY_INTEGRATED	0x01
#define USB_PHY_EXTERNAL	0x02
#define USB_PHY_SERIAL_PMIC     0x04

#define REQUEST_STOP		0
#define REQUEST_START		1
#define REQUEST_RESUME		2
#define REQUEST_HNP_SUSPEND	3
#define REQUEST_HNP_RESUME	4

/* Flags required to read ID state of PHY for ACA */
#define PHY_ID_MASK		0xB0
#define PHY_ID_GND		0
#define PHY_ID_C		0x10
#define PHY_ID_B		0x30
#define PHY_ID_A		0x90

#define phy_id_state(ints)	((ints) & PHY_ID_MASK)
#define phy_id_state_gnd(ints)	(phy_id_state((ints)) == PHY_ID_GND)
#define phy_id_state_a(ints)	(phy_id_state((ints)) == PHY_ID_A)
/* RID_B and RID_C states does not exist with standard ACA */
#ifdef CONFIG_USB_MSM_STANDARD_ACA
#define phy_id_state_b(ints)	0
#define phy_id_state_c(ints)	0
#else
#define phy_id_state_b(ints)	(phy_id_state((ints)) == PHY_ID_B)
#define phy_id_state_c(ints)	(phy_id_state((ints)) == PHY_ID_C)
#endif

/*
 * The following are bit fields describing the usb_request.udc_priv word.
 * These bit fields are set by function drivers that wish to queue
 * usb_requests with sps/bam parameters.
 */
#define MSM_PIPE_ID_MASK		(0x1F)
#define MSM_TX_PIPE_ID_OFS		(16)
#define MSM_SPS_MODE			BIT(5)
#define MSM_IS_FINITE_TRANSFER		BIT(6)
#define MSM_PRODUCER			BIT(7)
#define MSM_DISABLE_WB			BIT(8)
#define MSM_ETD_IOC			BIT(9)
#define MSM_INTERNAL_MEM		BIT(10)
#define MSM_VENDOR_ID			BIT(16)

/* used to detect the OTG Mode */
enum otg_mode {
	OTG_ID = 0,   		/* ID pin detection */
	OTG_USER_CONTROL,  	/* User configurable */
	OTG_VCHG,     		/* Based on VCHG interrupt */
};

/* used to configure the default mode,if otg_mode is USER_CONTROL */
enum usb_mode {
	USB_HOST_MODE,
	USB_PERIPHERAL_MODE,
};

enum chg_type {
	USB_CHG_TYPE__SDP,
	USB_CHG_TYPE__CARKIT,
	USB_CHG_TYPE__WALLCHARGER,
	USB_CHG_TYPE__INVALID
};

enum pre_emphasis_level {
	PRE_EMPHASIS_DEFAULT,
	PRE_EMPHASIS_DISABLE,
	PRE_EMPHASIS_WITH_10_PERCENT = (1 << 5),
	PRE_EMPHASIS_WITH_20_PERCENT = (3 << 4),
};
enum cdr_auto_reset {
	CDR_AUTO_RESET_DEFAULT,
	CDR_AUTO_RESET_ENABLE,
	CDR_AUTO_RESET_DISABLE,
};

enum se1_gate_state {
	SE1_GATING_DEFAULT,
	SE1_GATING_ENABLE,
	SE1_GATING_DISABLE,
};

enum hs_drv_amplitude {
	HS_DRV_AMPLITUDE_DEFAULT,
	HS_DRV_AMPLITUDE_ZERO_PERCENT,
	HS_DRV_AMPLITUDE_25_PERCENTI = (1 << 2),
	HS_DRV_AMPLITUDE_5_PERCENT = (1 << 3),
	HS_DRV_AMPLITUDE_75_PERCENT = (3 << 2),
};

#define HS_DRV_SLOPE_DEFAULT	(-1)

/* used to configure the analog switch to select b/w host and peripheral */
enum usb_switch_control {
	USB_SWITCH_PERIPHERAL = 0,	/* Configure switch in peripheral mode*/
	USB_SWITCH_HOST,		/* Host mode */
	USB_SWITCH_DISABLE,		/* No mode selected, shutdown power */
};

struct msm_hsusb_gadget_platform_data {
	int *phy_init_seq;
	void (*phy_reset)(void);

	int self_powered;
	int is_phy_status_timer_on;
};

struct msm_otg_platform_data {
	int (*rpc_connect)(int);
	int (*phy_reset)(void __iomem *);
	int pmic_vbus_irq;
	int pmic_id_irq;
	/* if usb link is in sps there is no need for
	 * usb pclk as dayatona fabric clock will be
	 * used instead
	 */
	int usb_in_sps;
	enum pre_emphasis_level	pemp_level;
	enum cdr_auto_reset	cdr_autoreset;
	enum hs_drv_amplitude	drv_ampl;
	enum se1_gate_state	se1_gating;
	int			hsdrvslope;
	int			phy_reset_sig_inverted;
	int			phy_can_powercollapse;
	int			pclk_required_during_lpm;
	int			bam_disable;
	/* HSUSB core in 8660 has the capability to gate the
	 * pclk when not being used. Though this feature is
	 * now being disabled because of H/w issues
	 */
	int			pclk_is_hw_gated;

	int (*ldo_init) (int init);
	int (*ldo_enable) (int enable);
	int (*ldo_set_voltage) (int mV);

	u32 			swfi_latency;
	/* pmic notfications apis */
	int (*pmic_vbus_notif_init) (void (*callback)(int online), int init);
	int (*pmic_id_notif_init) (void (*callback)(int online), int init);
	int (*phy_id_setup_init) (int init);
	int (*pmic_register_vbus_sn) (void (*callback)(int online));
	void (*pmic_unregister_vbus_sn) (void (*callback)(int online));
	int (*pmic_enable_ldo) (int);
	int (*init_gpio)(int on);
	void (*setup_gpio)(enum usb_switch_control mode);
	u8      otg_mode;
	u8	usb_mode;
	void (*vbus_power) (unsigned phy_info, int on);

	/* charger notification apis */
	void (*chg_connected)(enum chg_type chg_type);
	void (*chg_vbus_draw)(unsigned ma);
	int  (*chg_init)(int init);
	int (*config_vddcx)(int high);
	int (*init_vddcx)(int init);

	struct pm_qos_request_list pm_qos_req_dma;
};

struct msm_usb_host_platform_data {
	unsigned phy_info;
	unsigned int power_budget;
	void (*config_gpio)(unsigned int config);
	void (*vbus_power) (unsigned phy_info, int on);
	int  (*vbus_init)(int init);
	struct clk *ebi1_clk;
};

int msm_ep_config(struct usb_ep *ep);
int msm_ep_unconfig(struct usb_ep *ep);
int msm_data_fifo_config(struct usb_ep *ep, u32 addr, u32 size);

#endif

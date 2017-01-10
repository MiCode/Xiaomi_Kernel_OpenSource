/* include/linux/usb/msm_hsusb.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>
 * Copyright (c) 2009-2016, The Linux Foundation. All rights reserved.
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

#include <linux/extcon.h>
#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
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


/**
 * OTG control
 *
 * OTG_NO_CONTROL	Id/VBUS notifications not required. Useful in host
 *                      only configuration.
 * OTG_PHY_CONTROL	Id/VBUS notifications comes form USB PHY.
 * OTG_PMIC_CONTROL	Id/VBUS notifications comes from PMIC hardware.
 * OTG_USER_CONTROL	Id/VBUS notifcations comes from User via sysfs.
 *
 */
enum otg_control_type {
	OTG_NO_CONTROL = 0,
	OTG_PHY_CONTROL,
	OTG_PMIC_CONTROL,
	OTG_USER_CONTROL,
};

/**
 * PHY used in
 *
 * INVALID_PHY			Unsupported PHY
 * CI_45NM_INTEGRATED_PHY	Chipidea 45nm integrated PHY
 * SNPS_28NM_INTEGRATED_PHY	Synopsis 28nm integrated PHY
 *
 */
enum msm_usb_phy_type {
	INVALID_PHY = 0,
	CI_45NM_INTEGRATED_PHY,
	SNPS_28NM_INTEGRATED_PHY,
};

#define IDEV_CHG_MAX	1500
#define IUNIT		100

/**
 * Different states involved in USB charger detection.
 *
 * USB_CHG_STATE_UNDEFINED	USB charger is not connected or detection
 *                              process is not yet started.
 * USB_CHG_STATE_WAIT_FOR_DCD	Waiting for Data pins contact.
 * USB_CHG_STATE_DCD_DONE	Data pin contact is detected.
 * USB_CHG_STATE_PRIMARY_DONE	Primary detection is completed (Detects
 *                              between SDP and DCP/CDP).
 * USB_CHG_STATE_SECONDARY_DONE	Secondary detection is completed (Detects
 *                              between DCP and CDP).
 * USB_CHG_STATE_DETECTED	USB charger type is determined.
 *
 */
enum usb_chg_state {
	USB_CHG_STATE_UNDEFINED = 0,
	USB_CHG_STATE_WAIT_FOR_DCD,
	USB_CHG_STATE_DCD_DONE,
	USB_CHG_STATE_PRIMARY_DONE,
	USB_CHG_STATE_SECONDARY_DONE,
	USB_CHG_STATE_DETECTED,
};

/**
 * USB charger types
 *
 * USB_INVALID_CHARGER	Invalid USB charger.
 * USB_SDP_CHARGER	Standard downstream port. Refers to a downstream port
 *                      on USB2.0 compliant host/hub.
 * USB_DCP_CHARGER	Dedicated charger port (AC charger/ Wall charger).
 * USB_CDP_CHARGER	Charging downstream port. Enumeration can happen and
 *                      IDEV_CHG_MAX can be drawn irrespective of USB state.
 *
 */
enum usb_chg_type {
	USB_INVALID_CHARGER = 0,
	USB_SDP_CHARGER,
	USB_DCP_CHARGER,
	USB_CDP_CHARGER,
};

/**
 * Supported USB controllers
 */
enum usb_ctrl {
	DWC3_CTRL = 0,	/* DWC3 controller */
	CI_CTRL,	/* ChipIdea controller */
	HSIC_CTRL,	/* HSIC controller */
	NUM_CTRL,
};


/**
 * USB ID state
 */
enum usb_id_state {
	USB_ID_GROUND = 0,
	USB_ID_FLOAT,
};

/**
 * struct msm_otg_platform_data - platform device data
 *              for msm_otg driver.
 * @phy_init_seq: PHY configuration sequence values. Value of -1 is reserved as
 *              "do not overwrite default vaule at this address".
 * @phy_init_sz: PHY configuration sequence size.
 * @vbus_power: VBUS power on/off routine.
 * @power_budget: VBUS power budget in mA (0 will be treated as 500mA).
 * @mode: Supported mode (OTG/peripheral/host).
 * @otg_control: OTG switch controlled by user/Id pin
 */
struct msm_otg_platform_data {
	int *phy_init_seq;
	int phy_init_sz;
	void (*vbus_power)(bool on);
	unsigned power_budget;
	enum usb_dr_mode mode;
	enum otg_control_type otg_control;
	enum msm_usb_phy_type phy_type;
	void (*setup_gpio)(enum usb_otg_state state);
};

/**
 * struct msm_usb_cable - structure for exteternal connector cable
 *			  state tracking
 * @nb: hold event notification callback
 * @conn: used for notification registration
 */
struct msm_usb_cable {
	struct notifier_block		nb;
	struct extcon_dev		*extcon;
};


/* phy related flags */
#define ENABLE_DP_MANUAL_PULLUP		BIT(0)
#define ENABLE_SECONDARY_PHY		BIT(1)
#define PHY_HOST_MODE			BIT(2)
#define PHY_CHARGER_CONNECTED		BIT(3)
#define PHY_VBUS_VALID_OVERRIDE		BIT(4)
#define DEVICE_IN_SS_MODE		BIT(5)
#define PHY_LANE_A			BIT(6)
#define PHY_LANE_B			BIT(7)
#define PHY_HSFS_MODE			BIT(8)
#define PHY_LS_MODE			BIT(9)

#define USB_NUM_BUS_CLOCKS      3

/**
 * struct msm_otg: OTG driver data. Shared by HCD and DCD.
 * @otg: USB OTG Transceiver structure.
 * @pdata: otg device platform data.
 * @irq: IRQ number assigned for HSUSB controller.
 * @clk: clock struct of usb_hs_clk.
 * @pclk: clock struct of usb_hs_pclk.
 * @core_clk: clock struct of usb_hs_core_clk.
 * @regs: ioremapped register base address.
 * @inputs: OTG state machine inputs(Id, SessValid etc).
 * @sm_work: OTG state machine work.
 * @in_lpm: indicates low power mode (LPM) state.
 * @async_int: Async interrupt arrived.
 * @cur_power: The amount of mA available from downstream port.
 * @chg_work: Charger detection work.
 * @chg_state: The state of charger detection process.
 * @chg_type: The type of charger attached.
 * @dcd_retires: The retry count used to track Data contact
 *               detection process.
 * @manual_pullup: true if VBUS is not routed to USB controller/phy
 *	and controller driver therefore enables pull-up explicitly before
 *	starting controller using usbcmd run/stop bit.
 * @vbus: VBUS signal state trakining, using extcon framework
 * @id: ID signal state trakining, using extcon framework
 * @switch_gpio: Descriptor for GPIO used to control external Dual
 *               SPDT USB Switch.
 * @reboot: Used to inform the driver to route USB D+/D- line to Device
 *	    connector
 */
struct msm_otg {
	struct usb_phy phy;
	struct msm_otg_platform_data *pdata;
	int irq;
	struct clk *clk;
	struct clk *pclk;
	struct clk *core_clk;
	void __iomem *regs;
#define ID		0
#define B_SESS_VLD	1
	unsigned long inputs;
	struct work_struct sm_work;
	atomic_t in_lpm;
	int async_int;
	unsigned cur_power;
	int phy_number;
	struct delayed_work chg_work;
	enum usb_chg_state chg_state;
	enum usb_chg_type chg_type;
	u8 dcd_retries;
	struct regulator *v3p3;
	struct regulator *v1p8;
	struct regulator *vddcx;

	struct reset_control *phy_rst;
	struct reset_control *link_rst;
	int vdd_levels[3];

	bool manual_pullup;

	struct msm_usb_cable vbus;
	struct msm_usb_cable id;

	struct gpio_desc *switch_gpio;
	struct notifier_block reboot;
};

#ifdef CONFIG_USB_BAM
void msm_bam_set_usb_host_dev(struct device *dev);
void msm_bam_set_hsic_host_dev(struct device *dev);
void msm_bam_wait_for_usb_host_prod_granted(void);
void msm_bam_wait_for_hsic_host_prod_granted(void);
bool msm_bam_hsic_lpm_ok(void);
void msm_bam_usb_host_notify_on_resume(void);
void msm_bam_hsic_host_notify_on_resume(void);
bool msm_bam_hsic_host_pipe_empty(void);
bool msm_usb_bam_enable(enum usb_ctrl ctrl, bool bam_enable);
#else
static inline void msm_bam_set_usb_host_dev(struct device *dev) {}
static inline void msm_bam_set_hsic_host_dev(struct device *dev) {}
static inline void msm_bam_wait_for_usb_host_prod_granted(void) {}
static inline void msm_bam_wait_for_hsic_host_prod_granted(void) {}
static inline bool msm_bam_hsic_lpm_ok(void) { return true; }
static inline void msm_bam_hsic_host_notify_on_resume(void) {}
static inline void msm_bam_usb_host_notify_on_resume(void) {}
static inline bool msm_bam_hsic_host_pipe_empty(void) { return true; }
static inline bool msm_usb_bam_enable(enum usb_ctrl ctrl, bool bam_enable)
{
	return true;
}
#endif

/* CONFIG_PM */
#ifdef CONFIG_PM
static inline int get_pm_runtime_counter(struct device *dev)
{
	return atomic_read(&dev->power.usage_count);
}
#else /* !CONFIG_PM */
static inline int get_pm_runtime_counter(struct device *dev) { return -ENOSYS; }
#endif

#ifdef CONFIG_USB_CI13XXX_MSM
void msm_hw_bam_disable(bool bam_disable);
void msm_usb_irq_disable(bool disable);
#else
static inline void msm_hw_bam_disable(bool bam_disable)
{
}

static inline void msm_usb_irq_disable(bool disable)
{
}
#endif

#ifdef CONFIG_USB_DWC3_QCOM
int msm_ep_config(struct usb_ep *ep, struct usb_request *request);
int msm_ep_unconfig(struct usb_ep *ep);
void dwc3_tx_fifo_resize_request(struct usb_ep *ep, bool qdss_enable);
int msm_data_fifo_config(struct usb_ep *ep, phys_addr_t addr, u32 size,
	u8 dst_pipe_idx);
bool msm_dwc3_reset_ep_after_lpm(struct usb_gadget *gadget);
int msm_dwc3_reset_dbm_ep(struct usb_ep *ep);

#else
static inline int msm_data_fifo_config(struct usb_ep *ep, phys_addr_t addr,
	u32 size, u8 dst_pipe_idx)
{
	return -ENODEV;
}

static inline int msm_ep_config(struct usb_ep *ep, struct usb_request *request)
{
	return -ENODEV;
}

static inline int msm_ep_unconfig(struct usb_ep *ep)
{
	return -ENODEV;
}

static inline void dwc3_tx_fifo_resize_request(
					struct usb_ep *ep, bool qdss_enable)
{
}

static inline bool msm_dwc3_reset_ep_after_lpm(struct usb_gadget *gadget)
{
	return false;
}

static inline int msm_dwc3_reset_dbm_ep(struct usb_ep *ep)
{
	return -ENODEV;
}

#endif
#endif

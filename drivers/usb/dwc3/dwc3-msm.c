/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/pm_runtime.h>
#include <linux/ratelimit.h>
#include <linux/interrupt.h>
#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/of.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/msm-bus.h>
#include <linux/irq.h>
#include <linux/extcon.h>
#include <linux/reset.h>
#include <linux/clk/qcom.h>

#include "power.h"
#include "core.h"
#include "gadget.h"
#include "dbm.h"
#include "debug.h"
#include "xhci.h"

#define SDP_CONNETION_CHECK_TIME 10000 /* in ms */

/* time out to wait for USB cable status notification (in ms)*/
#define SM_INIT_TIMEOUT 30000

/* AHB2PHY register offsets */
#define PERIPH_SS_AHB2PHY_TOP_CFG 0x10

/* AHB2PHY read/write waite value */
#define ONE_READ_WRITE_WAIT 0x11

/* XHCI registers */
#define USB3_HCSPARAMS1		(0x4)
#define USB3_PORTSC		(0x420)

/**
 *  USB QSCRATCH Hardware registers
 *
 */
#define QSCRATCH_REG_OFFSET	(0x000F8800)
#define QSCRATCH_GENERAL_CFG	(QSCRATCH_REG_OFFSET + 0x08)
#define CGCTL_REG		(QSCRATCH_REG_OFFSET + 0x28)
#define PWR_EVNT_IRQ_STAT_REG    (QSCRATCH_REG_OFFSET + 0x58)
#define PWR_EVNT_IRQ_MASK_REG    (QSCRATCH_REG_OFFSET + 0x5C)

#define PWR_EVNT_POWERDOWN_IN_P3_MASK		BIT(2)
#define PWR_EVNT_POWERDOWN_OUT_P3_MASK		BIT(3)
#define PWR_EVNT_LPM_IN_L2_MASK			BIT(4)
#define PWR_EVNT_LPM_OUT_L2_MASK		BIT(5)
#define PWR_EVNT_LPM_OUT_L1_MASK		BIT(13)

/* QSCRATCH_GENERAL_CFG register bit offset */
#define PIPE_UTMI_CLK_SEL	BIT(0)
#define PIPE3_PHYSTATUS_SW	BIT(3)
#define PIPE_UTMI_CLK_DIS	BIT(8)

#define HS_PHY_CTRL_REG		(QSCRATCH_REG_OFFSET + 0x10)
#define UTMI_OTG_VBUS_VALID	BIT(20)
#define SW_SESSVLD_SEL		BIT(28)

#define SS_PHY_CTRL_REG		(QSCRATCH_REG_OFFSET + 0x30)
#define LANE0_PWR_PRESENT	BIT(24)

/* GSI related registers */
#define GSI_TRB_ADDR_BIT_53_MASK	(1 << 21)
#define GSI_TRB_ADDR_BIT_55_MASK	(1 << 23)

#define	GSI_GENERAL_CFG_REG		(QSCRATCH_REG_OFFSET + 0xFC)
#define	GSI_RESTART_DBL_PNTR_MASK	BIT(20)
#define	GSI_CLK_EN_MASK			BIT(12)
#define	BLOCK_GSI_WR_GO_MASK		BIT(1)
#define	GSI_EN_MASK			BIT(0)

#define GSI_DBL_ADDR_L(n)	((QSCRATCH_REG_OFFSET + 0x110) + (n*4))
#define GSI_DBL_ADDR_H(n)	((QSCRATCH_REG_OFFSET + 0x120) + (n*4))
#define GSI_RING_BASE_ADDR_L(n)	((QSCRATCH_REG_OFFSET + 0x130) + (n*4))
#define GSI_RING_BASE_ADDR_H(n)	((QSCRATCH_REG_OFFSET + 0x144) + (n*4))

#define	GSI_IF_STS	(QSCRATCH_REG_OFFSET + 0x1A4)
#define	GSI_WR_CTRL_STATE_MASK	BIT(15)

#define DWC3_GEVNTCOUNT_EVNTINTRPTMASK		(1 << 31)
#define DWC3_GEVNTADRHI_EVNTADRHI_GSI_EN(n)	(n << 22)
#define DWC3_GEVNTADRHI_EVNTADRHI_GSI_IDX(n)	(n << 16)
#define DWC3_GEVENT_TYPE_GSI			0x3

struct dwc3_msm_req_complete {
	struct list_head list_item;
	struct usb_request *req;
	void (*orig_complete)(struct usb_ep *ep,
			      struct usb_request *req);
};

enum dwc3_id_state {
	DWC3_ID_GROUND = 0,
	DWC3_ID_FLOAT,
};

/* for type c cable */
enum plug_orientation {
	ORIENTATION_NONE,
	ORIENTATION_CC1,
	ORIENTATION_CC2,
};

enum msm_usb_irq {
	HS_PHY_IRQ,
	PWR_EVNT_IRQ,
	DP_HS_PHY_IRQ,
	DM_HS_PHY_IRQ,
	SS_PHY_IRQ,
	USB_MAX_IRQ
};

enum bus_vote {
	BUS_VOTE_NONE,
	BUS_VOTE_NOMINAL,
	BUS_VOTE_SVS,
	BUS_VOTE_MIN,
	BUS_VOTE_MAX
};

struct usb_irq {
	char *name;
	int irq;
	bool enable;
};

static const struct usb_irq usb_irq_info[USB_MAX_IRQ] = {
	{"hs_phy_irq", 0},
	{"pwr_event_irq", 0},
	{"dp_hs_phy_irq", 0},
	{"dm_hs_phy_irq", 0},
	{"ss_phy_irq", 0},
};

struct dwc3_msm;

struct extcon_nb {
	struct extcon_dev	*edev;
	struct dwc3_msm		*mdwc;
	int			idx;
	struct notifier_block	vbus_nb;
	struct notifier_block	id_nb;
	struct notifier_block	host_restart_nb;
};

/* Input bits to state machine (mdwc->inputs) */

#define ID			0
#define B_SESS_VLD		1
#define B_SUSPEND		2

#define PM_QOS_SAMPLE_SEC	2
#define PM_QOS_THRESHOLD	400

struct dwc3_msm {
	struct device *dev;
	void __iomem *base;
	void __iomem *ahb2phy_base;
	struct platform_device	*dwc3;
	struct dma_iommu_mapping *iommu_map;
	const struct usb_ep_ops *original_ep_ops[DWC3_ENDPOINTS_NUM];
	struct list_head req_complete_list;
	struct clk		*xo_clk;
	struct clk		*core_clk;
	long			core_clk_rate;
	long			core_clk_rate_hs;
	struct clk		*iface_clk;
	struct clk		*sleep_clk;
	struct clk		*utmi_clk;
	unsigned int		utmi_clk_rate;
	struct clk		*utmi_clk_src;
	struct clk		*bus_aggr_clk;
	struct clk		*noc_aggr_clk;
	struct clk		*cfg_ahb_clk;
	struct reset_control	*core_reset;
	struct regulator	*dwc3_gdsc;

	struct usb_phy		*hs_phy, *ss_phy;

	struct dbm		*dbm;

	/* VBUS regulator for host mode */
	struct regulator	*vbus_reg;
	int			vbus_retry_count;
	bool			resume_pending;
	atomic_t                pm_suspended;
	struct usb_irq		wakeup_irq[USB_MAX_IRQ];
	struct work_struct	resume_work;
	struct work_struct	restart_usb_work;
	bool			in_restart;
	struct workqueue_struct *dwc3_wq;
	struct delayed_work	sm_work;
	unsigned long		inputs;
	unsigned int		max_power;
	bool			charging_disabled;
	enum usb_otg_state	otg_state;
	enum bus_vote		override_bus_vote;
	u32			bus_perf_client;
	struct msm_bus_scale_pdata	*bus_scale_table;
	struct power_supply	*usb_psy;
	struct work_struct	vbus_draw_work;
	bool			in_host_mode;
	bool			in_device_mode;
	enum usb_device_speed	max_rh_port_speed;
	unsigned int		tx_fifo_size;
	bool			vbus_active;
	bool			suspend;
	bool			use_pdc_interrupts;
	enum dwc3_id_state	id_state;
	unsigned long		lpm_flags;
#define MDWC3_SS_PHY_SUSPEND		BIT(0)
#define MDWC3_ASYNC_IRQ_WAKE_CAPABILITY	BIT(1)
#define MDWC3_POWER_COLLAPSE		BIT(2)

	struct notifier_block	usbdev_nb;
	bool			hc_died;

	struct extcon_nb	*extcon;
	int			ext_idx;
	struct notifier_block	host_nb;

	atomic_t                in_p3;
	unsigned int		lpm_to_suspend_delay;
	enum plug_orientation	typec_orientation;
	u32			num_gsi_event_buffers;
	struct dwc3_event_buffer **gsi_ev_buff;
	int pm_qos_latency;
	struct pm_qos_request pm_qos_req_dma;
	struct delayed_work perf_vote_work;
	struct delayed_work sdp_check;
	bool usb_compliance_mode;
	struct mutex suspend_resume_mutex;

	enum usb_device_speed override_usb_speed;
};

#define USB_HSPHY_3P3_VOL_MIN		3050000 /* uV */
#define USB_HSPHY_3P3_VOL_MAX		3300000 /* uV */
#define USB_HSPHY_3P3_HPM_LOAD		16000	/* uA */

#define USB_HSPHY_1P8_VOL_MIN		1800000 /* uV */
#define USB_HSPHY_1P8_VOL_MAX		1800000 /* uV */
#define USB_HSPHY_1P8_HPM_LOAD		19000	/* uA */

#define USB_SSPHY_1P8_VOL_MIN		1800000 /* uV */
#define USB_SSPHY_1P8_VOL_MAX		1800000 /* uV */
#define USB_SSPHY_1P8_HPM_LOAD		23000	/* uA */

static void dwc3_pwr_event_handler(struct dwc3_msm *mdwc);
static int dwc3_msm_gadget_vbus_draw(struct dwc3_msm *mdwc, unsigned int mA);
static void dwc3_msm_notify_event(struct dwc3 *dwc, unsigned int event,
						unsigned int value);
static int dwc3_restart_usb_host_mode(struct notifier_block *nb,
					unsigned long event, void *ptr);

/**
 *
 * Read register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 *
 * @return u32
 */
static inline u32 dwc3_msm_read_reg(void __iomem *base, u32 offset)
{
	u32 val = ioread32(base + offset);
	return val;
}

/**
 * Read register masked field with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 *
 * @return u32
 */
static inline u32 dwc3_msm_read_reg_field(void __iomem *base,
					  u32 offset,
					  const u32 mask)
{
	u32 shift = __ffs(mask);
	u32 val = ioread32(base + offset);

	val &= mask;		/* clear other bits */
	val >>= shift;
	return val;
}

/**
 *
 * Write register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @val - value to write.
 *
 */
static inline void dwc3_msm_write_reg(void __iomem *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
}

/**
 * Write register masked field with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 * @val - value to write.
 *
 */
static inline void dwc3_msm_write_reg_field(void __iomem *base, u32 offset,
					    const u32 mask, u32 val)
{
	u32 shift = __ffs(mask);
	u32 tmp = ioread32(base + offset);

	tmp &= ~mask;		/* clear written bits */
	val = tmp | (val << shift);
	iowrite32(val, base + offset);
}

/**
 * Write register and read back masked value to confirm it is written
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @mask - register bitmask specifying what should be updated
 * @val - value to write.
 *
 */
static inline void dwc3_msm_write_readback(void __iomem *base, u32 offset,
					    const u32 mask, u32 val)
{
	u32 write_val, tmp = ioread32(base + offset);

	tmp &= ~mask;		/* retain other bits */
	write_val = tmp | val;

	iowrite32(write_val, base + offset);

	/* Read back to see if val was written */
	tmp = ioread32(base + offset);
	tmp &= mask;		/* clear other bits */

	if (tmp != val)
		pr_err("%s: write: %x to QSCRATCH: %x FAILED\n",
			__func__, val, offset);
}

static bool dwc3_msm_is_ss_rhport_connected(struct dwc3_msm *mdwc)
{
	int i, num_ports;
	u32 reg;

	reg = dwc3_msm_read_reg(mdwc->base, USB3_HCSPARAMS1);
	num_ports = HCS_MAX_PORTS(reg);

	for (i = 0; i < num_ports; i++) {
		reg = dwc3_msm_read_reg(mdwc->base, USB3_PORTSC + i*0x10);
		if ((reg & PORT_CONNECT) && DEV_SUPERSPEED_ANY(reg))
			return true;
	}

	return false;
}

static bool dwc3_msm_is_host_superspeed(struct dwc3_msm *mdwc)
{
	int i, num_ports;
	u32 reg;

	reg = dwc3_msm_read_reg(mdwc->base, USB3_HCSPARAMS1);
	num_ports = HCS_MAX_PORTS(reg);

	for (i = 0; i < num_ports; i++) {
		reg = dwc3_msm_read_reg(mdwc->base, USB3_PORTSC + i*0x10);
		if ((reg & PORT_PE) && DEV_SUPERSPEED_ANY(reg))
			return true;
	}

	return false;
}

static inline bool dwc3_msm_is_dev_superspeed(struct dwc3_msm *mdwc)
{
	u8 speed;

	speed = dwc3_msm_read_reg(mdwc->base, DWC3_DSTS) & DWC3_DSTS_CONNECTSPD;
	if ((speed & DWC3_DSTS_SUPERSPEED) ||
			(speed & DWC3_DSTS_SUPERSPEED_PLUS))
		return true;

	return false;
}

static inline bool dwc3_msm_is_superspeed(struct dwc3_msm *mdwc)
{
	if (mdwc->in_host_mode)
		return dwc3_msm_is_host_superspeed(mdwc);

	return dwc3_msm_is_dev_superspeed(mdwc);
}

static int dwc3_msm_dbm_disable_updxfer(struct dwc3 *dwc, u8 usb_ep)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	dev_dbg(mdwc->dev, "%s\n", __func__);
	dwc3_dbm_disable_update_xfer(mdwc->dbm, usb_ep);

	return 0;
}

#if IS_ENABLED(CONFIG_USB_DWC3_GADGET) || IS_ENABLED(CONFIG_USB_DWC3_DUAL_ROLE)
/**
 * Configure the DBM with the BAM's data fifo.
 * This function is called by the USB BAM Driver
 * upon initialization.
 *
 * @ep - pointer to usb endpoint.
 * @addr - address of data fifo.
 * @size - size of data fifo.
 *
 */
int msm_data_fifo_config(struct usb_ep *ep, unsigned long addr,
			 u32 size, u8 dst_pipe_idx)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	dev_dbg(mdwc->dev, "%s\n", __func__);

	return	dbm_data_fifo_config(mdwc->dbm, dep->number, addr, size,
						dst_pipe_idx);
}


/**
 * Cleanups for msm endpoint on request complete.
 *
 * Also call original request complete.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to usb_request instance.
 *
 * @return int - 0 on success, negative on error.
 */
static void dwc3_msm_req_complete_func(struct usb_ep *ep,
				       struct usb_request *request)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_msm_req_complete *req_complete = NULL;

	/* Find original request complete function and remove it from list */
	list_for_each_entry(req_complete, &mdwc->req_complete_list, list_item) {
		if (req_complete->req == request)
			break;
	}
	if (!req_complete || req_complete->req != request) {
		dev_err(dep->dwc->dev, "%s: could not find the request\n",
					__func__);
		return;
	}
	list_del(&req_complete->list_item);

	/*
	 * Release another one TRB to the pool since DBM queue took 2 TRBs
	 * (normal and link), and the dwc3/gadget.c :: dwc3_gadget_giveback
	 * released only one.
	 */
	dep->trb_dequeue++;

	/* Unconfigure dbm ep */
	dbm_ep_unconfig(mdwc->dbm, dep->number);

	/*
	 * If this is the last endpoint we unconfigured, than reset also
	 * the event buffers; unless unconfiguring the ep due to lpm,
	 * in which case the event buffer only gets reset during the
	 * block reset.
	 */
	if (dbm_get_num_of_eps_configured(mdwc->dbm) == 0 &&
		!dbm_reset_ep_after_lpm(mdwc->dbm))
		dbm_event_buffer_config(mdwc->dbm, 0, 0, 0);

	/*
	 * Call original complete function, notice that dwc->lock is already
	 * taken by the caller of this function (dwc3_gadget_giveback()).
	 */
	request->complete = req_complete->orig_complete;
	if (request->complete)
		request->complete(ep, request);

	kfree(req_complete);
}


/**
 * Helper function
 *
 * Reset  DBM endpoint.
 *
 * @mdwc - pointer to dwc3_msm instance.
 * @dep - pointer to dwc3_ep instance.
 *
 * @return int - 0 on success, negative on error.
 */
static int __dwc3_msm_dbm_ep_reset(struct dwc3_msm *mdwc, struct dwc3_ep *dep)
{
	int ret;

	dev_dbg(mdwc->dev, "Resetting dbm endpoint %d\n", dep->number);

	/* Reset the dbm endpoint */
	ret = dbm_ep_soft_reset(mdwc->dbm, dep->number, true);
	if (ret) {
		dev_err(mdwc->dev, "%s: failed to assert dbm ep reset\n",
				__func__);
		return ret;
	}

	/*
	 * The necessary delay between asserting and deasserting the dbm ep
	 * reset is based on the number of active endpoints. If there is more
	 * than one endpoint, a 1 msec delay is required. Otherwise, a shorter
	 * delay will suffice.
	 */
	if (dbm_get_num_of_eps_configured(mdwc->dbm) > 1)
		usleep_range(1000, 1200);
	else
		udelay(10);
	ret = dbm_ep_soft_reset(mdwc->dbm, dep->number, false);
	if (ret) {
		dev_err(mdwc->dev, "%s: failed to deassert dbm ep reset\n",
				__func__);
		return ret;
	}

	return 0;
}

/**
 * Reset the DBM endpoint which is linked to the given USB endpoint.
 *
 * @usb_ep - pointer to usb_ep instance.
 *
 * @return int - 0 on success, negative on error.
 */
int msm_dwc3_reset_dbm_ep(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	return __dwc3_msm_dbm_ep_reset(mdwc, dep);
}
EXPORT_SYMBOL(msm_dwc3_reset_dbm_ep);


/**
 * Helper function.
 * See the header of the dwc3_msm_ep_queue function.
 *
 * @dwc3_ep - pointer to dwc3_ep instance.
 * @req - pointer to dwc3_request instance.
 *
 * @return int - 0 on success, negative on error.
 */
static int __dwc3_msm_ep_queue(struct dwc3_ep *dep, struct dwc3_request *req)
{
	struct dwc3_trb *trb;
	struct dwc3_trb *trb_link;
	struct dwc3_gadget_ep_cmd_params params;
	u32 cmd;
	int ret = 0;

	/* We push the request to the dep->started_list list to indicate that
	 * this request is issued with start transfer. The request will be out
	 * from this list in 2 cases. The first is that the transfer will be
	 * completed (not if the transfer is endless using a circular TRBs with
	 * with link TRB). The second case is an option to do stop stransfer,
	 * this can be initiated by the function driver when calling dequeue.
	 */
	req->started = true;
	list_add_tail(&req->list, &dep->started_list);

	/* First, prepare a normal TRB, point to the fake buffer */
	trb = &dep->trb_pool[dep->trb_enqueue];
	dwc3_ep_inc_enq(dep);
	memset(trb, 0, sizeof(*trb));

	req->trb = trb;
	trb->bph = DBM_TRB_BIT | DBM_TRB_DMA | DBM_TRB_EP_NUM(dep->number);
	trb->size = DWC3_TRB_SIZE_LENGTH(req->request.length);
	trb->ctrl = DWC3_TRBCTL_NORMAL | DWC3_TRB_CTRL_HWO |
		DWC3_TRB_CTRL_CHN | (req->direction ? 0 : DWC3_TRB_CTRL_CSP);
	req->trb_dma = dwc3_trb_dma_offset(dep, trb);

	/* Second, prepare a Link TRB that points to the first TRB*/
	trb_link = &dep->trb_pool[dep->trb_enqueue];
	dwc3_ep_inc_enq(dep);
	memset(trb_link, 0, sizeof(*trb_link));

	trb_link->bpl = lower_32_bits(req->trb_dma);
	trb_link->bph = DBM_TRB_BIT |
			DBM_TRB_DMA | DBM_TRB_EP_NUM(dep->number);
	trb_link->size = 0;
	trb_link->ctrl = DWC3_TRBCTL_LINK_TRB | DWC3_TRB_CTRL_HWO;

	/*
	 * Now start the transfer
	 */
	memset(&params, 0, sizeof(params));
	params.param0 = 0; /* TDAddr High */
	params.param1 = lower_32_bits(req->trb_dma); /* DAddr Low */

	/* DBM requires IOC to be set */
	cmd = DWC3_DEPCMD_STARTTRANSFER | DWC3_DEPCMD_CMDIOC;
	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	if (ret < 0) {
		dev_dbg(dep->dwc->dev,
			"%s: failed to send STARTTRANSFER command\n",
			__func__);

		list_del(&req->list);
		return ret;
	}
	dep->flags |= DWC3_EP_BUSY;
	dep->resource_index = dwc3_gadget_ep_get_transfer_index(dep);

	return ret;
}

/**
 * Queue a usb request to the DBM endpoint.
 * This function should be called after the endpoint
 * was enabled by the ep_enable.
 *
 * This function prepares special structure of TRBs which
 * is familiar with the DBM HW, so it will possible to use
 * this endpoint in DBM mode.
 *
 * The TRBs prepared by this function, is one normal TRB
 * which point to a fake buffer, followed by a link TRB
 * that points to the first TRB.
 *
 * The API of this function follow the regular API of
 * usb_ep_queue (see usb_ep_ops in include/linuk/usb/gadget.h).
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to usb_request instance.
 * @gfp_flags - possible flags.
 *
 * @return int - 0 on success, negative on error.
 */
static int dwc3_msm_ep_queue(struct usb_ep *ep,
			     struct usb_request *request, gfp_t gfp_flags)
{
	struct dwc3_request *req = to_dwc3_request(request);
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_msm_req_complete *req_complete;
	unsigned long flags;
	int ret = 0, size;
	bool superspeed;

	/*
	 * We must obtain the lock of the dwc3 core driver,
	 * including disabling interrupts, so we will be sure
	 * that we are the only ones that configure the HW device
	 * core and ensure that we queuing the request will finish
	 * as soon as possible so we will release back the lock.
	 */
	spin_lock_irqsave(&dwc->lock, flags);
	if (!dep->endpoint.desc) {
		dev_err(mdwc->dev,
			"%s: trying to queue request %pK to disabled ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}

	if (!request) {
		dev_err(mdwc->dev, "%s: request is NULL\n", __func__);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}

	if (!(request->udc_priv & MSM_SPS_MODE)) {
		/* Not SPS mode, call original queue */
		dev_vdbg(mdwc->dev, "%s: not sps mode, use regular queue\n",
					__func__);

		spin_unlock_irqrestore(&dwc->lock, flags);
		return (mdwc->original_ep_ops[dep->number])->queue(ep,
								request,
								gfp_flags);
	}

	/* HW restriction regarding TRB size (8KB) */
	if (req->request.length < 0x2000) {
		dev_err(mdwc->dev, "%s: Min TRB size is 8KB\n", __func__);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}

	if (dep->number == 0 || dep->number == 1) {
		dev_err(mdwc->dev,
			"%s: trying to queue dbm request %pK to ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}

	if (dep->trb_dequeue != dep->trb_enqueue
					|| !list_empty(&dep->pending_list)
					|| !list_empty(&dep->started_list)) {
		dev_err(mdwc->dev,
			"%s: trying to queue dbm request %pK tp ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}
	dep->trb_dequeue = 0;
	dep->trb_enqueue = 0;

	/*
	 * Override req->complete function, but before doing that,
	 * store it's original pointer in the req_complete_list.
	 */
	req_complete = kzalloc(sizeof(*req_complete), gfp_flags);

	if (!req_complete) {
		dev_err(mdwc->dev, "%s: not enough memory\n", __func__);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -ENOMEM;
	}

	req_complete->req = request;
	req_complete->orig_complete = request->complete;
	list_add_tail(&req_complete->list_item, &mdwc->req_complete_list);
	request->complete = dwc3_msm_req_complete_func;

	dev_vdbg(dwc->dev, "%s: queing request %p to ep %s length %d\n",
			__func__, request, ep->name, request->length);
	size = dwc3_msm_read_reg(mdwc->base, DWC3_GEVNTSIZ(0));
	dbm_event_buffer_config(mdwc->dbm,
		dwc3_msm_read_reg(mdwc->base, DWC3_GEVNTADRLO(0)),
		dwc3_msm_read_reg(mdwc->base, DWC3_GEVNTADRHI(0)),
		DWC3_GEVNTSIZ_SIZE(size));

	ret = __dwc3_msm_ep_queue(dep, req);
	if (ret < 0) {
		dev_err(mdwc->dev,
			"error %d after calling __dwc3_msm_ep_queue\n", ret);
		goto err;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);
	superspeed = dwc3_msm_is_dev_superspeed(mdwc);
	dbm_set_speed(mdwc->dbm, (u8)superspeed);

	return 0;

err:
	spin_unlock_irqrestore(&dwc->lock, flags);
	kfree(req_complete);
	return ret;
}

/**
 * Returns XferRscIndex for the EP. This is stored at StartXfer GSI EP OP
 *
 * @usb_ep - pointer to usb_ep instance.
 *
 * @return int - XferRscIndex
 */
static inline int gsi_get_xfer_index(struct usb_ep *ep)
{
	struct dwc3_ep			*dep = to_dwc3_ep(ep);

	return dep->resource_index;
}

/**
 * Fills up the GSI channel information needed in call to IPA driver
 * for GSI channel creation.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @ch_info - output parameter with requested channel info
 */
static void gsi_get_channel_info(struct usb_ep *ep,
			struct gsi_channel_info *ch_info)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	int last_trb_index = 0;
	struct dwc3	*dwc = dep->dwc;
	struct usb_gsi_request *request = ch_info->ch_req;

	/* Provide physical USB addresses for DEPCMD and GEVENTCNT registers */
	ch_info->depcmd_low_addr = (u32)(dwc->reg_phys +
				DWC3_DEP_BASE(dep->number) + DWC3_DEPCMD);

	ch_info->depcmd_hi_addr = 0;

	ch_info->xfer_ring_base_addr = dwc3_trb_dma_offset(dep,
							&dep->trb_pool[0]);
	/* Convert to multipled of 1KB */
	ch_info->const_buffer_size = request->buf_len/1024;

	/* IN direction */
	if (dep->direction) {
		/*
		 * Multiply by size of each TRB for xfer_ring_len in bytes.
		 * 2n + 2 TRBs as per GSI h/w requirement. n Xfer TRBs + 1
		 * extra Xfer TRB followed by n ZLP TRBs + 1 LINK TRB.
		 */
		ch_info->xfer_ring_len = (2 * request->num_bufs + 2) * 0x10;
		last_trb_index = 2 * request->num_bufs + 2;
	} else { /* OUT direction */
		/*
		 * Multiply by size of each TRB for xfer_ring_len in bytes.
		 * n + 1 TRBs as per GSI h/w requirement. n Xfer TRBs + 1
		 * LINK TRB.
		 */
		ch_info->xfer_ring_len = (request->num_bufs + 2) * 0x10;
		last_trb_index = request->num_bufs + 2;
	}

	/* Store last 16 bits of LINK TRB address as per GSI hw requirement */
	ch_info->last_trb_addr = (dwc3_trb_dma_offset(dep,
			&dep->trb_pool[last_trb_index - 1]) & 0x0000FFFF);
	ch_info->gevntcount_low_addr = (u32)(dwc->reg_phys +
			DWC3_GEVNTCOUNT(ep->ep_intr_num));
	ch_info->gevntcount_hi_addr = 0;

	dev_dbg(dwc->dev,
	"depcmd_laddr=%x last_trb_addr=%x gevtcnt_laddr=%x gevtcnt_haddr=%x",
		ch_info->depcmd_low_addr, ch_info->last_trb_addr,
		ch_info->gevntcount_low_addr, ch_info->gevntcount_hi_addr);
}

/**
 * Perform StartXfer on GSI EP. Stores XferRscIndex.
 *
 * @usb_ep - pointer to usb_ep instance.
 *
 * @return int - 0 on success
 */
static int gsi_startxfer_for_ep(struct usb_ep *ep)
{
	int ret;
	struct dwc3_gadget_ep_cmd_params params;
	u32				cmd;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3	*dwc = dep->dwc;

	memset(&params, 0, sizeof(params));
	params.param0 = GSI_TRB_ADDR_BIT_53_MASK | GSI_TRB_ADDR_BIT_55_MASK;
	params.param0 |= (ep->ep_intr_num << 16);
	params.param1 = lower_32_bits(dwc3_trb_dma_offset(dep,
						&dep->trb_pool[0]));
	cmd = DWC3_DEPCMD_STARTTRANSFER;
	cmd |= DWC3_DEPCMD_PARAM(0);
	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);

	if (ret < 0)
		dev_dbg(dwc->dev, "Fail StrtXfr on GSI EP#%d\n", dep->number);
	dep->resource_index = dwc3_gadget_ep_get_transfer_index(dep);
	dev_dbg(dwc->dev, "XferRsc = %x", dep->resource_index);
	return ret;
}

/**
 * Store Ring Base and Doorbell Address for GSI EP
 * for GSI channel creation.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - USB GSI request to get Doorbell address obtained from IPA driver
 */
static void gsi_store_ringbase_dbl_info(struct usb_ep *ep,
			struct usb_gsi_request *request)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3	*dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	int n = ep->ep_intr_num - 1;

	dwc3_msm_write_reg(mdwc->base, GSI_RING_BASE_ADDR_L(n),
			dwc3_trb_dma_offset(dep, &dep->trb_pool[0]));

	if (request->mapped_db_reg_phs_addr_lsb)
		dma_unmap_resource(dwc->sysdev,
			request->mapped_db_reg_phs_addr_lsb,
			PAGE_SIZE, DMA_BIDIRECTIONAL, 0);

	request->mapped_db_reg_phs_addr_lsb = dma_map_resource(dwc->sysdev,
			(phys_addr_t)request->db_reg_phs_addr_lsb, PAGE_SIZE,
			DMA_BIDIRECTIONAL, 0);
	if (dma_mapping_error(dwc->sysdev, request->mapped_db_reg_phs_addr_lsb))
		dev_err(mdwc->dev, "mapping error for db_reg_phs_addr_lsb\n");

	dev_dbg(mdwc->dev, "ep:%s dbl_addr_lsb:%x mapped_dbl_addr_lsb:%llx\n",
		ep->name, request->db_reg_phs_addr_lsb,
		(unsigned long long)request->mapped_db_reg_phs_addr_lsb);

	dwc3_msm_write_reg(mdwc->base, GSI_DBL_ADDR_L(n),
			(u32)request->mapped_db_reg_phs_addr_lsb);
	dev_dbg(mdwc->dev, "Ring Base Addr %d: %x (LSB)\n", n,
			dwc3_msm_read_reg(mdwc->base, GSI_RING_BASE_ADDR_L(n)));
	dev_dbg(mdwc->dev, "GSI DB Addr %d: %x (LSB)\n", n,
			dwc3_msm_read_reg(mdwc->base, GSI_DBL_ADDR_L(n)));
}

/**
 * Rings Doorbell for GSI Channel
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request. This is used to pass in the
 * address of the GSI doorbell obtained from IPA driver
 */
static void gsi_ring_db(struct usb_ep *ep, struct usb_gsi_request *request)
{
	void __iomem *gsi_dbl_address_lsb;
	void __iomem *gsi_dbl_address_msb;
	dma_addr_t offset;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3	*dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	int num_trbs = (dep->direction) ? (2 * (request->num_bufs) + 2)
					: (request->num_bufs + 2);

	gsi_dbl_address_lsb = devm_ioremap_nocache(mdwc->dev,
				request->db_reg_phs_addr_lsb, sizeof(u32));
	if (!gsi_dbl_address_lsb)
		dev_dbg(mdwc->dev, "Failed to get GSI DBL address LSB\n");

	gsi_dbl_address_msb = devm_ioremap_nocache(mdwc->dev,
			request->db_reg_phs_addr_msb, sizeof(u32));
	if (!gsi_dbl_address_msb)
		dev_dbg(mdwc->dev, "Failed to get GSI DBL address MSB\n");

	offset = dwc3_trb_dma_offset(dep, &dep->trb_pool[num_trbs-1]);
	dev_dbg(mdwc->dev, "Writing link TRB addr: %pa to %p (%x) for ep:%s\n",
		&offset, gsi_dbl_address_lsb, request->db_reg_phs_addr_lsb,
		ep->name);

	writel_relaxed(offset, gsi_dbl_address_lsb);
	writel_relaxed(0, gsi_dbl_address_msb);
}

/**
 * Sets HWO bit for TRBs and performs UpdateXfer for OUT EP.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request. Used to determine num of TRBs for OUT EP.
 *
 * @return int - 0 on success
 */
static int gsi_updatexfer_for_ep(struct usb_ep *ep,
					struct usb_gsi_request *request)
{
	int i;
	int ret;
	u32				cmd;
	int num_trbs = request->num_bufs + 1;
	struct dwc3_trb *trb;
	struct dwc3_gadget_ep_cmd_params params;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;

	for (i = 0; i < num_trbs - 1; i++) {
		trb = &dep->trb_pool[i];
		trb->ctrl |= DWC3_TRB_CTRL_HWO;
	}

	memset(&params, 0, sizeof(params));
	cmd = DWC3_DEPCMD_UPDATETRANSFER;
	cmd |= DWC3_DEPCMD_PARAM(dep->resource_index);
	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	dep->flags |= DWC3_EP_BUSY;
	if (ret < 0)
		dev_dbg(dwc->dev, "UpdateXfr fail on GSI EP#%d\n", dep->number);
	return ret;
}

/**
 * Perform EndXfer on particular GSI EP.
 *
 * @usb_ep - pointer to usb_ep instance.
 */
static void gsi_endxfer_for_ep(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3	*dwc = dep->dwc;

	dwc3_stop_active_transfer(dwc, dep->number, true);
}

/**
 * Allocates and configures TRBs for GSI EPs.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request.
 *
 * @return int - 0 on success
 */
static int gsi_prepare_trbs(struct usb_ep *ep, struct usb_gsi_request *req)
{
	int i = 0;
	dma_addr_t buffer_addr = req->dma;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3		*dwc = dep->dwc;
	struct dwc3_trb *trb;
	int num_trbs = (dep->direction) ? (2 * (req->num_bufs) + 2)
					: (req->num_bufs + 2);
	struct scatterlist *sg;
	struct sg_table *sgt;

	dep->trb_pool = dma_zalloc_coherent(dwc->sysdev,
				num_trbs * sizeof(struct dwc3_trb),
				&dep->trb_pool_dma, GFP_KERNEL);

	if (!dep->trb_pool) {
		dev_err(dep->dwc->dev, "failed to alloc trb dma pool for %s\n",
				dep->name);
		return -ENOMEM;
	}

	dep->num_trbs = num_trbs;
	dma_get_sgtable(dwc->sysdev, &req->sgt_trb_xfer_ring, dep->trb_pool,
		dep->trb_pool_dma, num_trbs * sizeof(struct dwc3_trb));

	sgt = &req->sgt_trb_xfer_ring;
	dev_dbg(dwc->dev, "%s(): trb_pool:%pK trb_pool_dma:%lx\n",
		__func__, dep->trb_pool, (unsigned long)dep->trb_pool_dma);

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		dev_dbg(dwc->dev,
			"%i: page_link:%lx offset:%x length:%x address:%lx\n",
			i, sg->page_link, sg->offset, sg->length,
			(unsigned long)sg->dma_address);

	/* IN direction */
	if (dep->direction) {
		for (i = 0; i < num_trbs ; i++) {
			trb = &dep->trb_pool[i];
			memset(trb, 0, sizeof(*trb));
			/* Set up first n+1 TRBs for ZLPs */
			if (i < (req->num_bufs + 1)) {
				trb->bpl = 0;
				trb->bph = 0;
				trb->size = 0;
				trb->ctrl = DWC3_TRBCTL_NORMAL
						| DWC3_TRB_CTRL_IOC;
				continue;
			}

			/* Setup n TRBs pointing to valid buffers */
			trb->bpl = lower_32_bits(buffer_addr);
			trb->bph = 0;
			trb->size = 0;
			trb->ctrl = DWC3_TRBCTL_NORMAL
					| DWC3_TRB_CTRL_IOC;
			buffer_addr += req->buf_len;

			/* Set up the Link TRB at the end */
			if (i == (num_trbs - 1)) {
				trb->bpl = dwc3_trb_dma_offset(dep,
							&dep->trb_pool[0]);
				trb->bph = (1 << 23) | (1 << 21)
						| (ep->ep_intr_num << 16);
				trb->size = 0;
				trb->ctrl = DWC3_TRBCTL_LINK_TRB
						| DWC3_TRB_CTRL_HWO;
			}
		}
	} else { /* OUT direction */

		for (i = 0; i < num_trbs ; i++) {

			trb = &dep->trb_pool[i];
			memset(trb, 0, sizeof(*trb));
			/* Setup LINK TRB to start with TRB ring */
			if (i == 0) {
				trb->bpl = dwc3_trb_dma_offset(dep,
							&dep->trb_pool[1]);
				trb->ctrl = DWC3_TRBCTL_LINK_TRB;
			} else if (i == (num_trbs - 1)) {
				/* Set up the Link TRB at the end */
				trb->bpl = dwc3_trb_dma_offset(dep,
						&dep->trb_pool[0]);
				trb->bph = (1 << 23) | (1 << 21)
						| (ep->ep_intr_num << 16);
				trb->ctrl = DWC3_TRBCTL_LINK_TRB
						| DWC3_TRB_CTRL_HWO;
			} else {
				trb->bpl = lower_32_bits(buffer_addr);
				trb->size = req->buf_len;
				buffer_addr += req->buf_len;
				trb->ctrl = DWC3_TRBCTL_NORMAL
					| DWC3_TRB_CTRL_IOC
					| DWC3_TRB_CTRL_CSP
					| DWC3_TRB_CTRL_ISP_IMI;
			}
		}
	}

	dev_dbg(dwc->dev, "%s: Initialized TRB Ring for %s\n",
					__func__, dep->name);
	trb = &dep->trb_pool[0];
	if (trb) {
		for (i = 0; i < num_trbs; i++) {
			dev_dbg(dwc->dev,
				"TRB %d: ADDR:%lx bpl:%x bph:%x sz:%x ctl:%x\n",
				i, (unsigned long)dwc3_trb_dma_offset(dep,
				&dep->trb_pool[i]), trb->bpl, trb->bph,
				trb->size, trb->ctrl);
			trb++;
		}
	}

	return 0;
}

/**
 * Frees TRBs for GSI EPs.
 *
 * @usb_ep - pointer to usb_ep instance.
 *
 */
static void gsi_free_trbs(struct usb_ep *ep, struct usb_gsi_request *req)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;

	if (dep->endpoint.ep_type == EP_TYPE_NORMAL)
		return;

	/*  Free TRBs and TRB pool for EP */
	if (dep->trb_pool_dma) {
		dma_free_coherent(dwc->sysdev,
			dep->num_trbs * sizeof(struct dwc3_trb),
			dep->trb_pool,
			dep->trb_pool_dma);
		dep->trb_pool = NULL;
		dep->trb_pool_dma = 0;
	}
	sg_free_table(&req->sgt_trb_xfer_ring);
}
/**
 * Configures GSI EPs. For GSI EPs we need to set interrupter numbers.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request.
 */
static void gsi_configure_ep(struct usb_ep *ep, struct usb_gsi_request *request)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3		*dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_gadget_ep_cmd_params params;
	const struct usb_endpoint_descriptor *desc = ep->desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc = ep->comp_desc;
	u32 reg;
	int ret;

	memset(&params, 0x00, sizeof(params));

	/* Configure GSI EP */
	params.param0 = DWC3_DEPCFG_EP_TYPE(usb_endpoint_type(desc))
		| DWC3_DEPCFG_MAX_PACKET_SIZE(usb_endpoint_maxp(desc));

	/* Burst size is only needed in SuperSpeed mode */
	if (dwc->gadget.speed >= USB_SPEED_SUPER) {
		u32 burst = dep->endpoint.maxburst - 1;

		params.param0 |= DWC3_DEPCFG_BURST_SIZE(burst);
	}

	if (usb_ss_max_streams(comp_desc) && usb_endpoint_xfer_bulk(desc)) {
		params.param1 |= DWC3_DEPCFG_STREAM_CAPABLE
					| DWC3_DEPCFG_STREAM_EVENT_EN;
		dep->stream_capable = true;
	}

	/* Set EP number */
	params.param1 |= DWC3_DEPCFG_EP_NUMBER(dep->number);

	/* Set interrupter number for GSI endpoints */
	params.param1 |= DWC3_DEPCFG_INT_NUM(ep->ep_intr_num);

	/* Enable XferInProgress and XferComplete Interrupts */
	params.param1 |= DWC3_DEPCFG_XFER_COMPLETE_EN;
	params.param1 |= DWC3_DEPCFG_XFER_IN_PROGRESS_EN;
	params.param1 |= DWC3_DEPCFG_FIFO_ERROR_EN;
	/*
	 * We must use the lower 16 TX FIFOs even though
	 * HW might have more
	 */
	/* Remove FIFO Number for GSI EP*/
	if (dep->direction)
		params.param0 |= DWC3_DEPCFG_FIFO_NUMBER(dep->number >> 1);

	params.param0 |= DWC3_DEPCFG_ACTION_INIT;

	dev_dbg(mdwc->dev, "Set EP config to params = %x %x %x, for %s\n",
	params.param0, params.param1, params.param2, dep->name);

	dwc3_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETEPCONFIG, &params);

	/* Set XferRsc Index for GSI EP */
	if (!(dep->flags & DWC3_EP_ENABLED)) {
		ret = dwc3_gadget_resize_tx_fifos(dwc, dep);
		if (ret)
			return;

		memset(&params, 0x00, sizeof(params));
		params.param0 = DWC3_DEPXFERCFG_NUM_XFER_RES(1);
		dwc3_send_gadget_ep_cmd(dep,
				DWC3_DEPCMD_SETTRANSFRESOURCE, &params);

		dep->endpoint.desc = desc;
		dep->endpoint.comp_desc = comp_desc;
		dep->type = usb_endpoint_type(desc);
		dep->flags |= DWC3_EP_ENABLED;
		reg = dwc3_readl(dwc->regs, DWC3_DALEPENA);
		reg |= DWC3_DALEPENA_EP(dep->number);
		dwc3_writel(dwc->regs, DWC3_DALEPENA, reg);
	}

}

/**
 * Enables USB wrapper for GSI
 *
 * @usb_ep - pointer to usb_ep instance.
 */
static void gsi_enable(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	dwc3_msm_write_reg_field(mdwc->base,
			GSI_GENERAL_CFG_REG, GSI_CLK_EN_MASK, 1);
	dwc3_msm_write_reg_field(mdwc->base,
			GSI_GENERAL_CFG_REG, GSI_RESTART_DBL_PNTR_MASK, 1);
	dwc3_msm_write_reg_field(mdwc->base,
			GSI_GENERAL_CFG_REG, GSI_RESTART_DBL_PNTR_MASK, 0);
	dev_dbg(mdwc->dev, "%s: Enable GSI\n", __func__);
	dwc3_msm_write_reg_field(mdwc->base,
			GSI_GENERAL_CFG_REG, GSI_EN_MASK, 1);
}

/**
 * Block or allow doorbell towards GSI
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request. In this case num_bufs is used as a bool
 * to set or clear the doorbell bit
 */
static void gsi_set_clear_dbell(struct usb_ep *ep,
					bool block_db)
{

	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	dwc3_msm_write_reg_field(mdwc->base,
		GSI_GENERAL_CFG_REG, BLOCK_GSI_WR_GO_MASK, block_db);
}

/**
 * Performs necessary checks before stopping GSI channels
 *
 * @usb_ep - pointer to usb_ep instance to access DWC3 regs
 */
static bool gsi_check_ready_to_suspend(struct usb_ep *ep, bool f_suspend)
{
	u32	timeout = 1500;
	u32	reg = 0;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	while (dwc3_msm_read_reg_field(mdwc->base,
		GSI_IF_STS, GSI_WR_CTRL_STATE_MASK)) {
		if (!timeout--) {
			dev_err(mdwc->dev,
			"Unable to suspend GSI ch. WR_CTRL_STATE != 0\n");
			return false;
		}
	}
	/* Check for U3 only if we are not handling Function Suspend */
	if (!f_suspend) {
		reg = dwc3_readl(dwc->regs, DWC3_DSTS);
		if (DWC3_DSTS_USBLNKST(reg) != DWC3_LINK_STATE_U3) {
			dev_err(mdwc->dev, "Unable to suspend GSI ch\n");
			return false;
		}
	}

	return true;
}


/**
 * Performs GSI operations or GSI EP related operations.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @op_data - pointer to opcode related data.
 * @op - GSI related or GSI EP related op code.
 *
 * @return int - 0 on success, negative on error.
 * Also returns XferRscIdx for GSI_EP_OP_GET_XFER_IDX.
 */
static int dwc3_msm_gsi_ep_op(struct usb_ep *ep,
		void *op_data, enum gsi_ep_op op)
{
	u32 ret = 0;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct usb_gsi_request *request;
	struct gsi_channel_info *ch_info;
	bool block_db, f_suspend;
	unsigned long flags;

	switch (op) {
	case GSI_EP_OP_PREPARE_TRBS:
		request = (struct usb_gsi_request *)op_data;
		dev_dbg(mdwc->dev, "EP_OP_PREPARE_TRBS for %s\n", ep->name);
		ret = gsi_prepare_trbs(ep, request);
		break;
	case GSI_EP_OP_FREE_TRBS:
		dev_dbg(mdwc->dev, "EP_OP_FREE_TRBS for %s\n", ep->name);
		request = (struct usb_gsi_request *)op_data;
		gsi_free_trbs(ep, request);
		break;
	case GSI_EP_OP_CONFIG:
		request = (struct usb_gsi_request *)op_data;
		dev_dbg(mdwc->dev, "EP_OP_CONFIG for %s\n", ep->name);
		spin_lock_irqsave(&dwc->lock, flags);
		gsi_configure_ep(ep, request);
		spin_unlock_irqrestore(&dwc->lock, flags);
		break;
	case GSI_EP_OP_STARTXFER:
		dev_dbg(mdwc->dev, "EP_OP_STARTXFER for %s\n", ep->name);
		spin_lock_irqsave(&dwc->lock, flags);
		ret = gsi_startxfer_for_ep(ep);
		spin_unlock_irqrestore(&dwc->lock, flags);
		break;
	case GSI_EP_OP_GET_XFER_IDX:
		dev_dbg(mdwc->dev, "EP_OP_GET_XFER_IDX for %s\n", ep->name);
		ret = gsi_get_xfer_index(ep);
		break;
	case GSI_EP_OP_STORE_DBL_INFO:
		dev_dbg(mdwc->dev, "EP_OP_STORE_DBL_INFO\n");
		request = (struct usb_gsi_request *)op_data;
		gsi_store_ringbase_dbl_info(ep, request);
		break;
	case GSI_EP_OP_ENABLE_GSI:
		dev_dbg(mdwc->dev, "EP_OP_ENABLE_GSI\n");
		gsi_enable(ep);
		break;
	case GSI_EP_OP_GET_CH_INFO:
		ch_info = (struct gsi_channel_info *)op_data;
		gsi_get_channel_info(ep, ch_info);
		break;
	case GSI_EP_OP_RING_DB:
		request = (struct usb_gsi_request *)op_data;
		dbg_print(0xFF, "RING_DB", 0, ep->name);
		gsi_ring_db(ep, request);
		break;
	case GSI_EP_OP_UPDATEXFER:
		request = (struct usb_gsi_request *)op_data;
		dev_dbg(mdwc->dev, "EP_OP_UPDATEXFER\n");
		spin_lock_irqsave(&dwc->lock, flags);
		ret = gsi_updatexfer_for_ep(ep, request);
		spin_unlock_irqrestore(&dwc->lock, flags);
		break;
	case GSI_EP_OP_ENDXFER:
		request = (struct usb_gsi_request *)op_data;
		dev_dbg(mdwc->dev, "EP_OP_ENDXFER for %s\n", ep->name);
		spin_lock_irqsave(&dwc->lock, flags);
		gsi_endxfer_for_ep(ep);
		spin_unlock_irqrestore(&dwc->lock, flags);
		break;
	case GSI_EP_OP_SET_CLR_BLOCK_DBL:
		block_db = *((bool *)op_data);
		dev_dbg(mdwc->dev, "EP_OP_SET_CLR_BLOCK_DBL %d\n",
						block_db);
		gsi_set_clear_dbell(ep, block_db);
		break;
	case GSI_EP_OP_CHECK_FOR_SUSPEND:
		dev_dbg(mdwc->dev, "EP_OP_CHECK_FOR_SUSPEND\n");
		f_suspend = *((bool *)op_data);
		ret = gsi_check_ready_to_suspend(ep, f_suspend);
		break;
	case GSI_EP_OP_DISABLE:
		dev_dbg(mdwc->dev, "EP_OP_DISABLE\n");
		ret = ep->ops->disable(ep);
		break;
	default:
		dev_err(mdwc->dev, "%s: Invalid opcode GSI EP\n", __func__);
	}

	return ret;
}

/**
 * Configure MSM endpoint.
 * This function do specific configurations
 * to an endpoint which need specific implementaion
 * in the MSM architecture.
 *
 * This function should be called by usb function/class
 * layer which need a support from the specific MSM HW
 * which wrap the USB3 core. (like GSI or DBM specific endpoints)
 *
 * @ep - a pointer to some usb_ep instance
 *
 * @return int - 0 on success, negetive on error.
 */
int msm_ep_config(struct usb_ep *ep, struct usb_request *request,
							gfp_t gfp_flags)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct usb_ep_ops *new_ep_ops;
	int ret = 0;
	u8 bam_pipe;
	bool producer;
	bool disable_wb;
	bool internal_mem;
	bool ioc;


	/* Save original ep ops for future restore*/
	if (mdwc->original_ep_ops[dep->number]) {
		dev_err(mdwc->dev,
			"ep [%s,%d] already configured as msm endpoint\n",
			ep->name, dep->number);
		return -EPERM;
	}
	mdwc->original_ep_ops[dep->number] = ep->ops;

	/* Set new usb ops as we like */
	new_ep_ops = kzalloc(sizeof(struct usb_ep_ops), gfp_flags);
	if (!new_ep_ops)
		return -ENOMEM;

	(*new_ep_ops) = (*ep->ops);
	new_ep_ops->queue = dwc3_msm_ep_queue;
	new_ep_ops->gsi_ep_op = dwc3_msm_gsi_ep_op;
	ep->ops = new_ep_ops;

	if (!mdwc->dbm || !request || (dep->endpoint.ep_type == EP_TYPE_GSI))
		return 0;

	/*
	 * Configure the DBM endpoint if required.
	 */
	bam_pipe = request->udc_priv & MSM_PIPE_ID_MASK;
	producer = ((request->udc_priv & MSM_PRODUCER) ? true : false);
	disable_wb = ((request->udc_priv & MSM_DISABLE_WB) ? true : false);
	internal_mem = ((request->udc_priv & MSM_INTERNAL_MEM) ? true : false);
	ioc = ((request->udc_priv & MSM_ETD_IOC) ? true : false);

	ret = dbm_ep_config(mdwc->dbm, dep->number, bam_pipe, producer,
					disable_wb, internal_mem, ioc);
	if (ret < 0) {
		dev_err(mdwc->dev,
			"error %d after calling dbm_ep_config\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(msm_ep_config);

/**
 * Un-configure MSM endpoint.
 * Tear down configurations done in the
 * dwc3_msm_ep_config function.
 *
 * @ep - a pointer to some usb_ep instance
 *
 * @return int - 0 on success, negative on error.
 */
int msm_ep_unconfig(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct usb_ep_ops *old_ep_ops;

	/* Restore original ep ops */
	if (!mdwc->original_ep_ops[dep->number]) {
		dev_err(mdwc->dev,
			"ep [%s,%d] was not configured as msm endpoint\n",
			ep->name, dep->number);
		return -EINVAL;
	}
	old_ep_ops = (struct usb_ep_ops	*)ep->ops;
	ep->ops = mdwc->original_ep_ops[dep->number];
	mdwc->original_ep_ops[dep->number] = NULL;
	kfree(old_ep_ops);

	/*
	 * Do HERE more usb endpoint un-configurations
	 * which are specific to MSM.
	 */
	if (!mdwc->dbm || (dep->endpoint.ep_type == EP_TYPE_GSI))
		return 0;

	if (dep->trb_dequeue == dep->trb_enqueue
					&& list_empty(&dep->pending_list)
					&& list_empty(&dep->started_list)) {
		dev_dbg(mdwc->dev,
			"%s: request is not queued, disable DBM ep for ep %s\n",
			__func__, ep->name);
		/* Unconfigure dbm ep */
		dbm_ep_unconfig(mdwc->dbm, dep->number);

		/*
		 * If this is the last endpoint we unconfigured, than reset also
		 * the event buffers; unless unconfiguring the ep due to lpm,
		 * in which case the event buffer only gets reset during the
		 * block reset.
		 */
		if (dbm_get_num_of_eps_configured(mdwc->dbm) == 0 &&
				!dbm_reset_ep_after_lpm(mdwc->dbm))
			dbm_event_buffer_config(mdwc->dbm, 0, 0, 0);
	}

	return 0;
}
EXPORT_SYMBOL(msm_ep_unconfig);
#endif /* (CONFIG_USB_DWC3_GADGET) || (CONFIG_USB_DWC3_DUAL_ROLE) */

static void dwc3_resume_work(struct work_struct *w);

static void dwc3_restart_usb_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm,
						restart_usb_work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	unsigned int timeout = 50;

	dev_dbg(mdwc->dev, "%s\n", __func__);

	if (atomic_read(&dwc->in_lpm) || dwc->dr_mode != USB_DR_MODE_OTG) {
		dev_dbg(mdwc->dev, "%s failed!!!\n", __func__);
		return;
	}

	/* guard against concurrent VBUS handling */
	mdwc->in_restart = true;

	if (!mdwc->vbus_active) {
		dev_dbg(mdwc->dev, "%s bailing out in disconnect\n", __func__);
		dwc->err_evt_seen = false;
		mdwc->in_restart = false;
		return;
	}

	dbg_event(0xFF, "RestartUSB", 0);
	/* Reset active USB connection */
	dwc3_resume_work(&mdwc->resume_work);

	/* Make sure disconnect is processed before sending connect */
	while (--timeout && !pm_runtime_suspended(mdwc->dev))
		msleep(20);

	if (!timeout) {
		dev_dbg(mdwc->dev,
			"Not in LPM after disconnect, forcing suspend...\n");
		dbg_event(0xFF, "ReStart:RT SUSP",
			atomic_read(&mdwc->dev->power.usage_count));
		pm_runtime_suspend(mdwc->dev);
	}

	mdwc->in_restart = false;
	/* Force reconnect only if cable is still connected */
	if (mdwc->vbus_active)
		dwc3_resume_work(&mdwc->resume_work);

	dwc->err_evt_seen = false;
	flush_delayed_work(&mdwc->sm_work);
}

static int msm_dwc3_usbdev_notify(struct notifier_block *self,
			unsigned long action, void *priv)
{
	struct dwc3_msm *mdwc = container_of(self, struct dwc3_msm, usbdev_nb);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	struct usb_bus *bus = priv;

	/* Interested only in recovery when HC dies */
	if (action != USB_BUS_DIED)
		return 0;

	dev_dbg(mdwc->dev, "%s initiate recovery from hc_died\n", __func__);
	/* Recovery already under process */
	if (mdwc->hc_died)
		return 0;

	if (bus->controller != &dwc->xhci->dev) {
		dev_dbg(mdwc->dev, "%s event for diff HCD\n", __func__);
		return 0;
	}

	mdwc->hc_died = true;
	schedule_delayed_work(&mdwc->sm_work, 0);
	return 0;
}


/*
 * Check whether the DWC3 requires resetting the ep
 * after going to Low Power Mode (lpm)
 */
bool msm_dwc3_reset_ep_after_lpm(struct usb_gadget *gadget)
{
	struct dwc3 *dwc = container_of(gadget, struct dwc3, gadget);
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	return dbm_reset_ep_after_lpm(mdwc->dbm);
}
EXPORT_SYMBOL(msm_dwc3_reset_ep_after_lpm);

/*
 * Config Global Distributed Switch Controller (GDSC)
 * to support controller power collapse
 */
static int dwc3_msm_config_gdsc(struct dwc3_msm *mdwc, int on)
{
	int ret;

	if (IS_ERR_OR_NULL(mdwc->dwc3_gdsc))
		return -EPERM;

	if (on) {
		ret = regulator_enable(mdwc->dwc3_gdsc);
		if (ret) {
			dev_err(mdwc->dev, "unable to enable usb3 gdsc\n");
			return ret;
		}
	} else {
		ret = regulator_disable(mdwc->dwc3_gdsc);
		if (ret) {
			dev_err(mdwc->dev, "unable to disable usb3 gdsc\n");
			return ret;
		}
	}

	return ret;
}

static int dwc3_msm_link_clk_reset(struct dwc3_msm *mdwc, bool assert)
{
	int ret = 0;

	if (assert) {
		disable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);
		/* Using asynchronous block reset to the hardware */
		dev_dbg(mdwc->dev, "block_reset ASSERT\n");
		clk_disable_unprepare(mdwc->utmi_clk);
		clk_disable_unprepare(mdwc->sleep_clk);
		clk_disable_unprepare(mdwc->core_clk);
		clk_disable_unprepare(mdwc->iface_clk);
		ret = reset_control_assert(mdwc->core_reset);
		if (ret)
			dev_err(mdwc->dev, "dwc3 core_reset assert failed\n");
	} else {
		dev_dbg(mdwc->dev, "block_reset DEASSERT\n");
		ret = reset_control_deassert(mdwc->core_reset);
		if (ret)
			dev_err(mdwc->dev, "dwc3 core_reset deassert failed\n");
		ndelay(200);
		clk_prepare_enable(mdwc->iface_clk);
		clk_prepare_enable(mdwc->core_clk);
		clk_prepare_enable(mdwc->sleep_clk);
		clk_prepare_enable(mdwc->utmi_clk);
		enable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);
	}

	return ret;
}

/* Initialize QSCRATCH registers for HSPHY and SSPHY operation */
static void dwc3_msm_qscratch_reg_init(struct dwc3_msm *mdwc)
{
	if (dwc3_msm_read_reg(mdwc->base, DWC3_GSNPSID) < DWC3_REVISION_250A)
		/* On older cores set XHCI_REV bit to specify revision 1.0 */
		dwc3_msm_write_reg_field(mdwc->base, QSCRATCH_GENERAL_CFG,
					 BIT(2), 1);

	/*
	 * Enable master clock for RAMs to allow BAM to access RAMs when
	 * RAM clock gating is enabled via DWC3's GCTL. Otherwise issues
	 * are seen where RAM clocks get turned OFF in SS mode
	 */
	dwc3_msm_write_reg(mdwc->base, CGCTL_REG,
		dwc3_msm_read_reg(mdwc->base, CGCTL_REG) | 0x18);

}

static void dwc3_msm_vbus_draw_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm,
			vbus_draw_work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dwc3_msm_gadget_vbus_draw(mdwc, dwc->vbus_draw);
}

static void dwc3_msm_notify_event(struct dwc3 *dwc, unsigned int event,
							unsigned int value)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_event_buffer *evt;
	u32 reg;
	int i;

	switch (event) {
	case DWC3_CONTROLLER_ERROR_EVENT:
		dev_info(mdwc->dev,
			"DWC3_CONTROLLER_ERROR_EVENT received, irq cnt %lu\n",
			dwc->irq_cnt);

		dwc3_gadget_disable_irq(dwc);

		/* prevent core from generating interrupts until recovery */
		reg = dwc3_msm_read_reg(mdwc->base, DWC3_GCTL);
		reg |= DWC3_GCTL_CORESOFTRESET;
		dwc3_msm_write_reg(mdwc->base, DWC3_GCTL, reg);

		/* restart USB which performs full reset and reconnect */
		schedule_work(&mdwc->restart_usb_work);
		break;
	case DWC3_CONTROLLER_RESET_EVENT:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_RESET_EVENT received\n");
		/* HS & SSPHYs get reset as part of core soft reset */
		dwc3_msm_qscratch_reg_init(mdwc);
		break;
	case DWC3_CONTROLLER_POST_RESET_EVENT:
		dev_dbg(mdwc->dev,
				"DWC3_CONTROLLER_POST_RESET_EVENT received\n");

		/*
		 * Below sequence is used when controller is working without
		 * having ssphy and only USB high/full speed is supported.
		 */
		if (dwc->maximum_speed == USB_SPEED_HIGH ||
					dwc->maximum_speed == USB_SPEED_FULL) {
			dwc3_msm_write_reg(mdwc->base, QSCRATCH_GENERAL_CFG,
				dwc3_msm_read_reg(mdwc->base,
				QSCRATCH_GENERAL_CFG)
				| PIPE_UTMI_CLK_DIS);

			usleep_range(2, 5);


			dwc3_msm_write_reg(mdwc->base, QSCRATCH_GENERAL_CFG,
				dwc3_msm_read_reg(mdwc->base,
				QSCRATCH_GENERAL_CFG)
				| PIPE_UTMI_CLK_SEL
				| PIPE3_PHYSTATUS_SW);

			usleep_range(2, 5);

			dwc3_msm_write_reg(mdwc->base, QSCRATCH_GENERAL_CFG,
				dwc3_msm_read_reg(mdwc->base,
				QSCRATCH_GENERAL_CFG)
				& ~PIPE_UTMI_CLK_DIS);
		}

		dwc->tx_fifo_size = mdwc->tx_fifo_size;
		break;
	case DWC3_CONTROLLER_CONNDONE_EVENT:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_CONNDONE_EVENT received\n");
		/*
		 * Add power event if the dbm indicates coming out of L1 by
		 * interrupt
		 */
		if (mdwc->dbm && dbm_l1_lpm_interrupt(mdwc->dbm))
			dwc3_msm_write_reg_field(mdwc->base,
					PWR_EVNT_IRQ_MASK_REG,
					PWR_EVNT_LPM_OUT_L1_MASK, 1);

		atomic_set(&dwc->in_lpm, 0);
		break;
	case DWC3_CONTROLLER_NOTIFY_OTG_EVENT:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_NOTIFY_OTG_EVENT received\n");
		if (dwc->enable_bus_suspend) {
			mdwc->suspend = dwc->b_suspend;
			queue_work(mdwc->dwc3_wq, &mdwc->resume_work);
		}
		break;
	case DWC3_CONTROLLER_SET_CURRENT_DRAW_EVENT:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_SET_CURRENT_DRAW_EVENT received\n");
		schedule_work(&mdwc->vbus_draw_work);
		break;
	case DWC3_CONTROLLER_RESTART_USB_SESSION:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_RESTART_USB_SESSION received\n");
		schedule_work(&mdwc->restart_usb_work);
		break;
	case DWC3_GSI_EVT_BUF_ALLOC:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_ALLOC\n");

		if (!mdwc->num_gsi_event_buffers)
			break;

		mdwc->gsi_ev_buff = devm_kzalloc(dwc->dev,
			sizeof(*dwc->ev_buf) * mdwc->num_gsi_event_buffers,
			GFP_KERNEL);
		if (!mdwc->gsi_ev_buff) {
			dev_err(dwc->dev, "can't allocate gsi_ev_buff\n");
			break;
		}

		for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {

			evt = devm_kzalloc(dwc->dev, sizeof(*evt), GFP_KERNEL);
			if (!evt)
				break;
			evt->dwc	= dwc;
			evt->length	= DWC3_EVENT_BUFFERS_SIZE;
			evt->buf	= dma_alloc_coherent(dwc->sysdev,
						DWC3_EVENT_BUFFERS_SIZE,
						&evt->dma, GFP_KERNEL);
			if (!evt->buf) {
				dev_err(dwc->dev,
					"can't allocate gsi_evt_buf(%d)\n", i);
				break;
			}
			mdwc->gsi_ev_buff[i] = evt;
		}
		break;
	case DWC3_GSI_EVT_BUF_SETUP:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_SETUP\n");
		for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {
			evt = mdwc->gsi_ev_buff[i];
			if (!evt)
				break;

			dev_dbg(mdwc->dev, "Evt buf %p dma %08llx length %d\n",
				evt->buf, (unsigned long long) evt->dma,
				evt->length);
			memset(evt->buf, 0, evt->length);
			evt->lpos = 0;
			/*
			 * Primary event buffer is programmed with registers
			 * DWC3_GEVNT*(0). Hence use DWC3_GEVNT*(i+1) to
			 * program USB GSI related event buffer with DWC3
			 * controller.
			 */
			dwc3_writel(dwc->regs, DWC3_GEVNTADRLO((i+1)),
				lower_32_bits(evt->dma));
			dwc3_writel(dwc->regs, DWC3_GEVNTADRHI((i+1)),
				DWC3_GEVNTADRHI_EVNTADRHI_GSI_EN(
				DWC3_GEVENT_TYPE_GSI) |
				DWC3_GEVNTADRHI_EVNTADRHI_GSI_IDX((i+1)));
			dwc3_writel(dwc->regs, DWC3_GEVNTSIZ((i+1)),
				DWC3_GEVNTCOUNT_EVNTINTRPTMASK |
				((evt->length) & 0xffff));
			dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT((i+1)), 0);
		}
		break;
	case DWC3_GSI_EVT_BUF_CLEANUP:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_CLEANUP\n");
		if (!mdwc->gsi_ev_buff)
			break;

		for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {
			evt = mdwc->gsi_ev_buff[i];
			evt->lpos = 0;
			/*
			 * Primary event buffer is programmed with registers
			 * DWC3_GEVNT*(0). Hence use DWC3_GEVNT*(i+1) to
			 * program USB GSI related event buffer with DWC3
			 * controller.
			 */
			dwc3_writel(dwc->regs, DWC3_GEVNTADRLO((i+1)), 0);
			dwc3_writel(dwc->regs, DWC3_GEVNTADRHI((i+1)), 0);
			dwc3_writel(dwc->regs, DWC3_GEVNTSIZ((i+1)),
					DWC3_GEVNTSIZ_INTMASK |
					DWC3_GEVNTSIZ_SIZE((i+1)));
			dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT((i+1)), 0);
		}
		break;
	case DWC3_GSI_EVT_BUF_FREE:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_FREE\n");
		if (!mdwc->gsi_ev_buff)
			break;

		for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {
			evt = mdwc->gsi_ev_buff[i];
			if (evt)
				dma_free_coherent(dwc->sysdev, evt->length,
							evt->buf, evt->dma);
		}
		break;
	case DWC3_CONTROLLER_NOTIFY_DISABLE_UPDXFER:
		dwc3_msm_dbm_disable_updxfer(dwc, value);
		break;
	default:
		dev_dbg(mdwc->dev, "unknown dwc3 event\n");
		break;
	}
}

static void dwc3_msm_block_reset(struct dwc3_msm *mdwc, bool core_reset)
{
	int ret  = 0;

	if (core_reset) {
		ret = dwc3_msm_link_clk_reset(mdwc, 1);
		if (ret)
			return;

		usleep_range(1000, 1200);
		ret = dwc3_msm_link_clk_reset(mdwc, 0);
		if (ret)
			return;

		usleep_range(10000, 12000);
	}

	if (mdwc->dbm) {
		/* Reset the DBM */
		dbm_soft_reset(mdwc->dbm, 1);
		usleep_range(1000, 1200);
		dbm_soft_reset(mdwc->dbm, 0);

		/*enable DBM*/
		dwc3_msm_write_reg_field(mdwc->base, QSCRATCH_GENERAL_CFG,
			DBM_EN_MASK, 0x1);
		dbm_enable(mdwc->dbm);
	}
}


static void dwc3_msm_power_collapse_por(struct dwc3_msm *mdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	u32 val;
	int ret;

	/* Configure AHB2PHY for one wait state read/write */
	if (mdwc->ahb2phy_base) {
		clk_prepare_enable(mdwc->cfg_ahb_clk);
		val = readl_relaxed(mdwc->ahb2phy_base +
				PERIPH_SS_AHB2PHY_TOP_CFG);
		if (val != ONE_READ_WRITE_WAIT) {
			writel_relaxed(ONE_READ_WRITE_WAIT,
				mdwc->ahb2phy_base + PERIPH_SS_AHB2PHY_TOP_CFG);
			/* complete above write before configuring USB PHY. */
			mb();
		}
		clk_disable_unprepare(mdwc->cfg_ahb_clk);
	}

	ret = dwc3_core_init(dwc);
	if (ret)
		dev_err(mdwc->dev, "%s: dwc3_core init failed (%d)\n",
							__func__, ret);
}

static int dwc3_msm_prepare_suspend(struct dwc3_msm *mdwc)
{
	unsigned long timeout;
	u32 reg = 0;

	if ((mdwc->in_host_mode || mdwc->in_device_mode)
			&& dwc3_msm_is_superspeed(mdwc) && !mdwc->in_restart) {
		if (!atomic_read(&mdwc->in_p3)) {
			dev_err(mdwc->dev, "Not in P3,aborting LPM sequence\n");
			return -EBUSY;
		}
	}

	/* Clear previous L2 events */
	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG,
		PWR_EVNT_LPM_IN_L2_MASK | PWR_EVNT_LPM_OUT_L2_MASK);

	/* Prepare HSPHY for suspend */
	reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(0));
	dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(0),
		reg | DWC3_GUSB2PHYCFG_ENBLSLPM | DWC3_GUSB2PHYCFG_SUSPHY);

	/* Wait for PHY to go into L2 */
	timeout = jiffies + msecs_to_jiffies(5);
	while (!time_after(jiffies, timeout)) {
		reg = dwc3_msm_read_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG);
		if (reg & PWR_EVNT_LPM_IN_L2_MASK)
			break;
	}
	if (!(reg & PWR_EVNT_LPM_IN_L2_MASK))
		dev_err(mdwc->dev, "could not transition HS PHY to L2\n");

	/* Clear L2 event bit */
	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG,
		PWR_EVNT_LPM_IN_L2_MASK);

	return 0;
}

static void dwc3_set_phy_speed_flags(struct dwc3_msm *mdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	int i, num_ports;
	u32 reg;

	mdwc->hs_phy->flags &= ~(PHY_HSFS_MODE | PHY_LS_MODE);
	if (mdwc->in_host_mode) {
		reg = dwc3_msm_read_reg(mdwc->base, USB3_HCSPARAMS1);
		num_ports = HCS_MAX_PORTS(reg);
		for (i = 0; i < num_ports; i++) {
			reg = dwc3_msm_read_reg(mdwc->base,
					USB3_PORTSC + i*0x10);
			if (reg & PORT_PE) {
				if (DEV_HIGHSPEED(reg) || DEV_FULLSPEED(reg))
					mdwc->hs_phy->flags |= PHY_HSFS_MODE;
				else if (DEV_LOWSPEED(reg))
					mdwc->hs_phy->flags |= PHY_LS_MODE;
			}
		}
	} else {
		if (dwc->gadget.speed == USB_SPEED_HIGH ||
			dwc->gadget.speed == USB_SPEED_FULL)
			mdwc->hs_phy->flags |= PHY_HSFS_MODE;
		else if (dwc->gadget.speed == USB_SPEED_LOW)
			mdwc->hs_phy->flags |= PHY_LS_MODE;
	}
}

static void msm_dwc3_perf_vote_update(struct dwc3_msm *mdwc,
						bool perf_mode);

static void configure_usb_wakeup_interrupt(struct dwc3_msm *mdwc,
	struct usb_irq *uirq, unsigned int polarity, bool enable)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (uirq && enable && !uirq->enable) {
		dbg_event(0xFF, "PDC_IRQ_EN", uirq->irq);
		dbg_event(0xFF, "PDC_IRQ_POL", polarity);
		/* clear any pending interrupt */
		irq_set_irqchip_state(uirq->irq, IRQCHIP_STATE_PENDING, 0);
		irq_set_irq_type(uirq->irq, polarity);
		enable_irq_wake(uirq->irq);
		enable_irq(uirq->irq);
		uirq->enable = true;
	}

	if (uirq && !enable && uirq->enable) {
		dbg_event(0xFF, "PDC_IRQ_DIS", uirq->irq);
		disable_irq_wake(uirq->irq);
		disable_irq_nosync(uirq->irq);
		uirq->enable = false;
	}
}

static void enable_usb_pdc_interrupt(struct dwc3_msm *mdwc, bool enable)
{
	if (!enable)
		goto disable_usb_irq;

	if (mdwc->hs_phy->flags & PHY_LS_MODE) {
		configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DM_HS_PHY_IRQ],
			IRQ_TYPE_EDGE_FALLING, enable);
	} else if (mdwc->hs_phy->flags & PHY_HSFS_MODE) {
		configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DP_HS_PHY_IRQ],
			IRQ_TYPE_EDGE_FALLING, enable);
	} else {
		configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DP_HS_PHY_IRQ],
			IRQ_TYPE_EDGE_RISING, true);
		configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DM_HS_PHY_IRQ],
			IRQ_TYPE_EDGE_RISING, true);
	}

	configure_usb_wakeup_interrupt(mdwc,
		&mdwc->wakeup_irq[SS_PHY_IRQ],
		IRQF_TRIGGER_HIGH | IRQ_TYPE_LEVEL_HIGH, enable);
	return;

disable_usb_irq:
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DP_HS_PHY_IRQ], 0, enable);
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DM_HS_PHY_IRQ], 0, enable);
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[SS_PHY_IRQ], 0, enable);
}

static void configure_nonpdc_usb_interrupt(struct dwc3_msm *mdwc,
		struct usb_irq *uirq, bool enable)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (uirq && enable && !uirq->enable) {
		dbg_event(0xFF, "IRQ_EN", uirq->irq);
		enable_irq_wake(uirq->irq);
		enable_irq(uirq->irq);
		uirq->enable = true;
	}

	if (uirq && !enable && uirq->enable) {
		dbg_event(0xFF, "IRQ_DIS", uirq->irq);
		disable_irq_wake(uirq->irq);
		disable_irq_nosync(uirq->irq);
		uirq->enable = false;
	}
}

static int dwc3_msm_update_bus_bw(struct dwc3_msm *mdwc, enum bus_vote bv)
{
	int ret = 0;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	unsigned int bv_index = mdwc->override_bus_vote ?: bv;

	if (!mdwc->bus_perf_client)
		return 0;

	dbg_event(0xFF, "bus_vote_start", bv);

	/* On some platforms SVS does not have separate vote.
	 * Vote for nominal if svs usecase does not exist.
	 * If the request is to set the bus_vote to _NONE,
	 * set it to _NONE irrespective of the requested vote
	 * from userspace.
	 */
	if (bv >= mdwc->bus_scale_table->num_usecases)
		bv_index = BUS_VOTE_NOMINAL;
	else if (bv == BUS_VOTE_NONE)
		bv_index = BUS_VOTE_NONE;

	ret = msm_bus_scale_client_update_request(
			mdwc->bus_perf_client, bv_index);
	if (ret)
		dev_err(mdwc->dev, "bus bw voting %d failed %d\n",
				bv_index, ret);

	dbg_event(0xFF, "bus_vote_end", bv_index);

	return ret;
}

static int dwc3_msm_suspend(struct dwc3_msm *mdwc)
{
	int ret;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	struct dwc3_event_buffer *evt;
	struct usb_irq *uirq;

	mutex_lock(&mdwc->suspend_resume_mutex);
	if (atomic_read(&dwc->in_lpm)) {
		dev_dbg(mdwc->dev, "%s: Already suspended\n", __func__);
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return 0;
	}

	cancel_delayed_work_sync(&mdwc->perf_vote_work);
	msm_dwc3_perf_vote_update(mdwc, false);

	if (!mdwc->in_host_mode) {
		evt = dwc->ev_buf;
		if ((evt->flags & DWC3_EVENT_PENDING)) {
			dev_dbg(mdwc->dev,
				"%s: %d device events pending, abort suspend\n",
				__func__, evt->count / 4);
			mutex_unlock(&mdwc->suspend_resume_mutex);
			return -EBUSY;
		}
	}

	if (!mdwc->vbus_active && (dwc->dr_mode == USB_DR_MODE_OTG) &&
		mdwc->otg_state == OTG_STATE_B_PERIPHERAL) {
		/*
		 * In some cases, the pm_runtime_suspend may be called by
		 * usb_bam when there is pending lpm flag. However, if this is
		 * done when cable was disconnected and otg state has not
		 * yet changed to IDLE, then it means OTG state machine
		 * is running and we race against it. So cancel LPM for now,
		 * and OTG state machine will go for LPM later, after completing
		 * transition to IDLE state.
		 */
		dev_dbg(mdwc->dev,
			"%s: cable disconnected while not in idle otg state\n",
			__func__);
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return -EBUSY;
	}

	/*
	 * Check if device is not in CONFIGURED state
	 * then check controller state of L2 and break
	 * LPM sequence. Check this for device bus suspend case.
	 */
	if (((dwc->dr_mode == USB_DR_MODE_OTG) &&
			mdwc->otg_state == OTG_STATE_B_SUSPEND) &&
			(dwc->gadget.state != USB_STATE_CONFIGURED)) {
		pr_err("%s(): Trying to go in LPM with state:%d\n",
					__func__, dwc->gadget.state);
		pr_err("%s(): LPM is not performed.\n", __func__);
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return -EBUSY;
	}

	ret = dwc3_msm_prepare_suspend(mdwc);
	if (ret) {
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return ret;
	}

	/* Disable core irq */
	if (dwc->irq)
		disable_irq(dwc->irq);

	if (work_busy(&dwc->bh_work))
		dbg_event(0xFF, "pend evt", 0);

	/* disable power event irq, hs and ss phy irq is used as wake up src */
	disable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);

	dwc3_set_phy_speed_flags(mdwc);
	/* Suspend HS PHY */
	usb_phy_set_suspend(mdwc->hs_phy, 1);

	/* Suspend SS PHY */
	if (dwc->maximum_speed >= USB_SPEED_SUPER) {
		/* indicate phy about SS mode */
		if (dwc3_msm_is_superspeed(mdwc))
			mdwc->ss_phy->flags |= DEVICE_IN_SS_MODE;
		usb_phy_set_suspend(mdwc->ss_phy, 1);
		mdwc->lpm_flags |= MDWC3_SS_PHY_SUSPEND;
	}

	/* make sure above writes are completed before turning off clocks */
	wmb();

	/* Disable clocks */
	if (mdwc->bus_aggr_clk)
		clk_disable_unprepare(mdwc->bus_aggr_clk);
	clk_disable_unprepare(mdwc->utmi_clk);

	/* Memory core: OFF, Memory periphery: OFF */
	if (!mdwc->in_host_mode && !mdwc->vbus_active) {
		clk_set_flags(mdwc->core_clk, CLKFLAG_NORETAIN_MEM);
		clk_set_flags(mdwc->core_clk, CLKFLAG_NORETAIN_PERIPH);
	}

	clk_set_rate(mdwc->core_clk, 19200000);
	clk_disable_unprepare(mdwc->core_clk);
	if (mdwc->noc_aggr_clk)
		clk_disable_unprepare(mdwc->noc_aggr_clk);
	/*
	 * Disable iface_clk only after core_clk as core_clk has FSM
	 * depedency on iface_clk. Hence iface_clk should be turned off
	 * after core_clk is turned off.
	 */
	clk_disable_unprepare(mdwc->iface_clk);
	/* USB PHY no more requires TCXO */
	clk_disable_unprepare(mdwc->xo_clk);

	/* Perform controller power collapse */
	if (!mdwc->in_host_mode && (!mdwc->in_device_mode ||
					mdwc->in_restart)) {
		mdwc->lpm_flags |= MDWC3_POWER_COLLAPSE;
		dev_dbg(mdwc->dev, "%s: power collapse\n", __func__);
		dwc3_msm_config_gdsc(mdwc, 0);
		clk_disable_unprepare(mdwc->sleep_clk);

		if (mdwc->iommu_map) {
			arm_iommu_detach_device(mdwc->dev);
			dev_dbg(mdwc->dev, "IOMMU detached\n");
		}
	}

	dwc3_msm_update_bus_bw(mdwc, BUS_VOTE_NONE);

	/*
	 * release wakeup source with timeout to defer system suspend to
	 * handle case where on USB cable disconnect, SUSPEND and DISCONNECT
	 * event is received.
	 */
	if (mdwc->lpm_to_suspend_delay) {
		dev_dbg(mdwc->dev, "defer suspend with %d(msecs)\n",
					mdwc->lpm_to_suspend_delay);
		pm_wakeup_event(mdwc->dev, mdwc->lpm_to_suspend_delay);
	} else {
		pm_relax(mdwc->dev);
	}

	atomic_set(&dwc->in_lpm, 1);

	/*
	 * with DCP or during cable disconnect, we dont require wakeup
	 * using HS_PHY_IRQ or SS_PHY_IRQ. Hence enable wakeup only in
	 * case of host bus suspend and device bus suspend.
	 */
	if (mdwc->in_device_mode || mdwc->in_host_mode) {
		if (mdwc->use_pdc_interrupts) {
			enable_usb_pdc_interrupt(mdwc, true);
		} else {
			uirq = &mdwc->wakeup_irq[HS_PHY_IRQ];
			configure_nonpdc_usb_interrupt(mdwc, uirq, true);
			uirq = &mdwc->wakeup_irq[SS_PHY_IRQ];
			configure_nonpdc_usb_interrupt(mdwc, uirq, true);
		}
		mdwc->lpm_flags |= MDWC3_ASYNC_IRQ_WAKE_CAPABILITY;
	}

	dev_info(mdwc->dev, "DWC3 in low power mode\n");
	dbg_event(0xFF, "Ctl Sus", atomic_read(&dwc->in_lpm));
	mutex_unlock(&mdwc->suspend_resume_mutex);
	return 0;
}

static int dwc3_msm_resume(struct dwc3_msm *mdwc)
{
	int ret;
	long core_clk_rate;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	struct usb_irq *uirq;

	dev_dbg(mdwc->dev, "%s: exiting lpm\n", __func__);

	mutex_lock(&mdwc->suspend_resume_mutex);
	if (!atomic_read(&dwc->in_lpm)) {
		dev_dbg(mdwc->dev, "%s: Already resumed\n", __func__);
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return 0;
	}

	pm_stay_awake(mdwc->dev);

	if (mdwc->in_host_mode && mdwc->max_rh_port_speed == USB_SPEED_HIGH)
		dwc3_msm_update_bus_bw(mdwc, BUS_VOTE_SVS);
	else
		dwc3_msm_update_bus_bw(mdwc, BUS_VOTE_NOMINAL);

	/* Vote for TCXO while waking up USB HSPHY */
	ret = clk_prepare_enable(mdwc->xo_clk);
	if (ret)
		dev_err(mdwc->dev, "%s failed to vote TCXO buffer%d\n",
						__func__, ret);

	/* Restore controller power collapse */
	if (mdwc->lpm_flags & MDWC3_POWER_COLLAPSE) {
		dev_dbg(mdwc->dev, "%s: exit power collapse\n", __func__);
		dwc3_msm_config_gdsc(mdwc, 1);
		ret = reset_control_assert(mdwc->core_reset);
		if (ret)
			dev_err(mdwc->dev, "%s:core_reset assert failed\n",
					__func__);
		/* HW requires a short delay for reset to take place properly */
		usleep_range(1000, 1200);
		ret = reset_control_deassert(mdwc->core_reset);
		if (ret)
			dev_err(mdwc->dev, "%s:core_reset deassert failed\n",
					__func__);
		clk_prepare_enable(mdwc->sleep_clk);
	}

	/*
	 * Enable clocks
	 * Turned ON iface_clk before core_clk due to FSM depedency.
	 */
	clk_prepare_enable(mdwc->iface_clk);
	if (mdwc->noc_aggr_clk)
		clk_prepare_enable(mdwc->noc_aggr_clk);

	core_clk_rate = mdwc->core_clk_rate;
	if (mdwc->in_host_mode && mdwc->max_rh_port_speed == USB_SPEED_HIGH) {
		core_clk_rate = mdwc->core_clk_rate_hs;
		dev_dbg(mdwc->dev, "%s: set hs core clk rate %ld\n", __func__,
			core_clk_rate);
	}

	clk_set_rate(mdwc->core_clk, core_clk_rate);
	clk_prepare_enable(mdwc->core_clk);

	/* set Memory core: ON, Memory periphery: ON */
	clk_set_flags(mdwc->core_clk, CLKFLAG_RETAIN_MEM);
	clk_set_flags(mdwc->core_clk, CLKFLAG_RETAIN_PERIPH);

	clk_prepare_enable(mdwc->utmi_clk);
	if (mdwc->bus_aggr_clk)
		clk_prepare_enable(mdwc->bus_aggr_clk);

	/* Resume SS PHY */
	if (dwc->maximum_speed >= USB_SPEED_SUPER &&
			mdwc->lpm_flags & MDWC3_SS_PHY_SUSPEND) {
		mdwc->ss_phy->flags &= ~(PHY_LANE_A | PHY_LANE_B);
		if (mdwc->typec_orientation == ORIENTATION_CC1)
			mdwc->ss_phy->flags |= PHY_LANE_A;
		if (mdwc->typec_orientation == ORIENTATION_CC2)
			mdwc->ss_phy->flags |= PHY_LANE_B;
		usb_phy_set_suspend(mdwc->ss_phy, 0);
		mdwc->ss_phy->flags &= ~DEVICE_IN_SS_MODE;
		mdwc->lpm_flags &= ~MDWC3_SS_PHY_SUSPEND;
	}

	mdwc->hs_phy->flags &= ~(PHY_HSFS_MODE | PHY_LS_MODE);
	/* Resume HS PHY */
	usb_phy_set_suspend(mdwc->hs_phy, 0);

	/* Recover from controller power collapse */
	if (mdwc->lpm_flags & MDWC3_POWER_COLLAPSE) {
		u32 tmp;

		if (mdwc->iommu_map) {
			ret = arm_iommu_attach_device(mdwc->dev,
					mdwc->iommu_map);
			if (ret)
				dev_err(mdwc->dev, "IOMMU attach failed (%d)\n",
						ret);
			else
				dev_dbg(mdwc->dev, "attached to IOMMU\n");
		}

		dev_dbg(mdwc->dev, "%s: exit power collapse\n", __func__);

		dwc3_msm_power_collapse_por(mdwc);

		/* Get initial P3 status and enable IN_P3 event */
		if (dwc3_is_usb31(dwc))
			tmp = dwc3_msm_read_reg_field(mdwc->base,
				DWC31_LINK_GDBGLTSSM,
				DWC3_GDBGLTSSM_LINKSTATE_MASK);
		else
			tmp = dwc3_msm_read_reg_field(mdwc->base,
				DWC3_GDBGLTSSM, DWC3_GDBGLTSSM_LINKSTATE_MASK);
		atomic_set(&mdwc->in_p3, tmp == DWC3_LINK_STATE_U3);
		dwc3_msm_write_reg_field(mdwc->base, PWR_EVNT_IRQ_MASK_REG,
					PWR_EVNT_POWERDOWN_IN_P3_MASK, 1);

		mdwc->lpm_flags &= ~MDWC3_POWER_COLLAPSE;
	}

	atomic_set(&dwc->in_lpm, 0);

	/* enable power evt irq for IN P3 detection */
	enable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);

	/* Disable HSPHY auto suspend */
	dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(0),
		dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(0)) &
				~DWC3_GUSB2PHYCFG_SUSPHY);

	/* Disable wakeup capable for HS_PHY IRQ & SS_PHY_IRQ if enabled */
	if (mdwc->lpm_flags & MDWC3_ASYNC_IRQ_WAKE_CAPABILITY) {
		if (mdwc->use_pdc_interrupts) {
			enable_usb_pdc_interrupt(mdwc, false);
		} else {
			uirq = &mdwc->wakeup_irq[HS_PHY_IRQ];
			configure_nonpdc_usb_interrupt(mdwc, uirq, false);
			uirq = &mdwc->wakeup_irq[SS_PHY_IRQ];
			configure_nonpdc_usb_interrupt(mdwc, uirq, false);
		}
		mdwc->lpm_flags &= ~MDWC3_ASYNC_IRQ_WAKE_CAPABILITY;
	}

	dev_info(mdwc->dev, "DWC3 exited from low power mode\n");

	/* Enable core irq */
	if (dwc->irq)
		enable_irq(dwc->irq);

	/*
	 * Handle other power events that could not have been handled during
	 * Low Power Mode
	 */
	dwc3_pwr_event_handler(mdwc);

	if (pm_qos_request_active(&mdwc->pm_qos_req_dma))
		schedule_delayed_work(&mdwc->perf_vote_work,
			msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));

	dbg_event(0xFF, "Ctl Res", atomic_read(&dwc->in_lpm));
	mutex_unlock(&mdwc->suspend_resume_mutex);

	return 0;
}

/**
 * dwc3_ext_event_notify - callback to handle events from external transceiver
 *
 * Returns 0 on success
 */
static void dwc3_ext_event_notify(struct dwc3_msm *mdwc)
{
	/* Flush processing any pending events before handling new ones */
	flush_delayed_work(&mdwc->sm_work);

	if (mdwc->id_state == DWC3_ID_FLOAT) {
		dev_dbg(mdwc->dev, "XCVR: ID set\n");
		set_bit(ID, &mdwc->inputs);
	} else {
		dev_dbg(mdwc->dev, "XCVR: ID clear\n");
		clear_bit(ID, &mdwc->inputs);
	}

	if (mdwc->vbus_active && !mdwc->in_restart) {
		dev_dbg(mdwc->dev, "XCVR: BSV set\n");
		set_bit(B_SESS_VLD, &mdwc->inputs);
	} else {
		dev_dbg(mdwc->dev, "XCVR: BSV clear\n");
		clear_bit(B_SESS_VLD, &mdwc->inputs);
	}

	if (mdwc->suspend) {
		dev_dbg(mdwc->dev, "XCVR: SUSP set\n");
		set_bit(B_SUSPEND, &mdwc->inputs);
	} else {
		dev_dbg(mdwc->dev, "XCVR: SUSP clear\n");
		clear_bit(B_SUSPEND, &mdwc->inputs);
	}

	schedule_delayed_work(&mdwc->sm_work, 0);
}

static void dwc3_resume_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm, resume_work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	union extcon_property_value val;
	unsigned int extcon_id;
	struct extcon_dev *edev = NULL;
	int ret = 0;

	dev_dbg(mdwc->dev, "%s: dwc3 resume work\n", __func__);

	if (mdwc->extcon && mdwc->vbus_active && !mdwc->in_restart) {
		extcon_id = EXTCON_USB;
		edev = mdwc->extcon[mdwc->ext_idx].edev;
	} else if (mdwc->extcon && mdwc->id_state == DWC3_ID_GROUND) {
		extcon_id = EXTCON_USB_HOST;
		edev = mdwc->extcon[mdwc->ext_idx].edev;
	}

	/* Check speed and Type-C polarity values in order to configure PHY */
	if (edev && extcon_get_state(edev, extcon_id)) {
		/* Use default dwc->maximum_speed if speed isn't reported */
		if (dwc->maximum_speed > dwc->max_hw_supp_speed)
			dwc->maximum_speed = dwc->max_hw_supp_speed;

		ret = extcon_get_property(edev, extcon_id,
				EXTCON_PROP_USB_SS, &val);

		if (!ret && val.intval == 0)
			dwc->maximum_speed = USB_SPEED_HIGH;

		if (mdwc->override_usb_speed) {
			dwc->maximum_speed = mdwc->override_usb_speed;
			dwc->gadget.max_speed = dwc->maximum_speed;
			dbg_event(0xFF, "override_speed",
					mdwc->override_usb_speed);
			mdwc->override_usb_speed = 0;
		}

		dbg_event(0xFF, "speed", dwc->maximum_speed);

		ret = extcon_get_property(edev, extcon_id,
				EXTCON_PROP_USB_TYPEC_POLARITY, &val);
		if (ret)
			mdwc->typec_orientation = ORIENTATION_NONE;
		else
			mdwc->typec_orientation = val.intval ?
					ORIENTATION_CC2 : ORIENTATION_CC1;

		dbg_event(0xFF, "cc_state", mdwc->typec_orientation);
	}

	/*
	 * exit LPM first to meet resume timeline from device side.
	 * resume_pending flag would prevent calling
	 * dwc3_msm_resume() in case we are here due to system
	 * wide resume without usb cable connected. This flag is set
	 * only in case of power event irq in lpm.
	 */
	if (mdwc->resume_pending) {
		dwc3_msm_resume(mdwc);
		mdwc->resume_pending = false;
	}

	if (atomic_read(&mdwc->pm_suspended)) {
		dbg_event(0xFF, "RWrk PMSus", 0);
		/* let pm resume kick in resume work later */
		return;
	}
	dwc3_ext_event_notify(mdwc);
}

static void dwc3_pwr_event_handler(struct dwc3_msm *mdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	u32 irq_stat, irq_clear = 0;

	irq_stat = dwc3_msm_read_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG);
	dev_dbg(mdwc->dev, "%s irq_stat=%X\n", __func__, irq_stat);

	/* Check for P3 events */
	if ((irq_stat & PWR_EVNT_POWERDOWN_OUT_P3_MASK) &&
			(irq_stat & PWR_EVNT_POWERDOWN_IN_P3_MASK)) {
		u32 ls;

		/* Can't tell if entered or exit P3, so check LINKSTATE */
		if (dwc3_is_usb31(dwc))
			ls = dwc3_msm_read_reg_field(mdwc->base,
				DWC31_LINK_GDBGLTSSM,
				DWC3_GDBGLTSSM_LINKSTATE_MASK);
		else
			ls = dwc3_msm_read_reg_field(mdwc->base,
				DWC3_GDBGLTSSM, DWC3_GDBGLTSSM_LINKSTATE_MASK);
		dev_dbg(mdwc->dev, "%s link state = 0x%04x\n", __func__, ls);
		atomic_set(&mdwc->in_p3, ls == DWC3_LINK_STATE_U3);

		irq_stat &= ~(PWR_EVNT_POWERDOWN_OUT_P3_MASK |
				PWR_EVNT_POWERDOWN_IN_P3_MASK);
		irq_clear |= (PWR_EVNT_POWERDOWN_OUT_P3_MASK |
				PWR_EVNT_POWERDOWN_IN_P3_MASK);
	} else if (irq_stat & PWR_EVNT_POWERDOWN_OUT_P3_MASK) {
		atomic_set(&mdwc->in_p3, 0);
		irq_stat &= ~PWR_EVNT_POWERDOWN_OUT_P3_MASK;
		irq_clear |= PWR_EVNT_POWERDOWN_OUT_P3_MASK;
	} else if (irq_stat & PWR_EVNT_POWERDOWN_IN_P3_MASK) {
		atomic_set(&mdwc->in_p3, 1);
		irq_stat &= ~PWR_EVNT_POWERDOWN_IN_P3_MASK;
		irq_clear |= PWR_EVNT_POWERDOWN_IN_P3_MASK;
	}

	/* Clear L2 exit */
	if (irq_stat & PWR_EVNT_LPM_OUT_L2_MASK) {
		irq_stat &= ~PWR_EVNT_LPM_OUT_L2_MASK;
		irq_stat |= PWR_EVNT_LPM_OUT_L2_MASK;
	}

	/* Handle exit from L1 events */
	if (irq_stat & PWR_EVNT_LPM_OUT_L1_MASK) {
		dev_dbg(mdwc->dev, "%s: handling PWR_EVNT_LPM_OUT_L1_MASK\n",
				__func__);
		if (usb_gadget_wakeup(&dwc->gadget))
			dev_err(mdwc->dev, "%s failed to take dwc out of L1\n",
					__func__);
		irq_stat &= ~PWR_EVNT_LPM_OUT_L1_MASK;
		irq_clear |= PWR_EVNT_LPM_OUT_L1_MASK;
	}

	/* Unhandled events */
	if (irq_stat)
		dev_dbg(mdwc->dev, "%s: unexpected PWR_EVNT, irq_stat=%X\n",
			__func__, irq_stat);

	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG, irq_clear);
}

static irqreturn_t msm_dwc3_pwr_irq_thread(int irq, void *_mdwc)
{
	struct dwc3_msm *mdwc = _mdwc;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(mdwc->dev, "%s\n", __func__);

	if (atomic_read(&dwc->in_lpm))
		dwc3_resume_work(&mdwc->resume_work);
	else
		dwc3_pwr_event_handler(mdwc);

	dbg_event(0xFF, "PWR IRQ", atomic_read(&dwc->in_lpm));
	return IRQ_HANDLED;
}

static irqreturn_t msm_dwc3_pwr_irq(int irq, void *data)
{
	struct dwc3_msm *mdwc = data;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dwc->t_pwr_evt_irq = ktime_get();
	dev_dbg(mdwc->dev, "%s received\n", __func__);
	/*
	 * When in Low Power Mode, can't read PWR_EVNT_IRQ_STAT_REG to acertain
	 * which interrupts have been triggered, as the clocks are disabled.
	 * Resume controller by waking up pwr event irq thread.After re-enabling
	 * clocks, dwc3_msm_resume will call dwc3_pwr_event_handler to handle
	 * all other power events.
	 */
	if (atomic_read(&dwc->in_lpm)) {
		/* set this to call dwc3_msm_resume() */
		mdwc->resume_pending = true;
		return IRQ_WAKE_THREAD;
	}

	dwc3_pwr_event_handler(mdwc);
	return IRQ_HANDLED;
}

static void dwc3_otg_sm_work(struct work_struct *w);

static int dwc3_msm_get_clk_gdsc(struct dwc3_msm *mdwc)
{
	int ret;

	mdwc->dwc3_gdsc = devm_regulator_get(mdwc->dev, "USB3_GDSC");
	if (IS_ERR(mdwc->dwc3_gdsc)) {
		if (PTR_ERR(mdwc->dwc3_gdsc) == -EPROBE_DEFER)
			return PTR_ERR(mdwc->dwc3_gdsc);
		mdwc->dwc3_gdsc = NULL;
	}

	mdwc->xo_clk = devm_clk_get(mdwc->dev, "xo");
	if (IS_ERR(mdwc->xo_clk)) {
		dev_err(mdwc->dev, "%s unable to get TCXO buffer handle\n",
								__func__);
		ret = PTR_ERR(mdwc->xo_clk);
		return ret;
	}
	clk_set_rate(mdwc->xo_clk, 19200000);

	mdwc->iface_clk = devm_clk_get(mdwc->dev, "iface_clk");
	if (IS_ERR(mdwc->iface_clk)) {
		dev_err(mdwc->dev, "failed to get iface_clk\n");
		ret = PTR_ERR(mdwc->iface_clk);
		return ret;
	}

	/*
	 * DWC3 Core requires its CORE CLK (aka master / bus clk) to
	 * run at 125Mhz in SSUSB mode and >60MHZ for HSUSB mode.
	 * On newer platform it can run at 150MHz as well.
	 */
	mdwc->core_clk = devm_clk_get(mdwc->dev, "core_clk");
	if (IS_ERR(mdwc->core_clk)) {
		dev_err(mdwc->dev, "failed to get core_clk\n");
		ret = PTR_ERR(mdwc->core_clk);
		return ret;
	}

	mdwc->core_reset = devm_reset_control_get(mdwc->dev, "core_reset");
	if (IS_ERR(mdwc->core_reset)) {
		dev_err(mdwc->dev, "failed to get core_reset\n");
		return PTR_ERR(mdwc->core_reset);
	}

	if (of_property_read_u32(mdwc->dev->of_node, "qcom,core-clk-rate",
				(u32 *)&mdwc->core_clk_rate)) {
		dev_err(mdwc->dev, "USB core-clk-rate is not present\n");
		return -EINVAL;
	}

	mdwc->core_clk_rate = clk_round_rate(mdwc->core_clk,
							mdwc->core_clk_rate);
	dev_dbg(mdwc->dev, "USB core frequency = %ld\n",
						mdwc->core_clk_rate);
	ret = clk_set_rate(mdwc->core_clk, mdwc->core_clk_rate);
	if (ret)
		dev_err(mdwc->dev, "fail to set core_clk freq:%d\n", ret);

	if (of_property_read_u32(mdwc->dev->of_node, "qcom,core-clk-rate-hs",
				(u32 *)&mdwc->core_clk_rate_hs)) {
		dev_dbg(mdwc->dev, "USB core-clk-rate-hs is not present\n");
		mdwc->core_clk_rate_hs = mdwc->core_clk_rate;
	}

	mdwc->sleep_clk = devm_clk_get(mdwc->dev, "sleep_clk");
	if (IS_ERR(mdwc->sleep_clk)) {
		dev_err(mdwc->dev, "failed to get sleep_clk\n");
		ret = PTR_ERR(mdwc->sleep_clk);
		return ret;
	}

	clk_set_rate(mdwc->sleep_clk, 32000);
	mdwc->utmi_clk_rate = 19200000;
	mdwc->utmi_clk = devm_clk_get(mdwc->dev, "utmi_clk");
	if (IS_ERR(mdwc->utmi_clk)) {
		dev_err(mdwc->dev, "failed to get utmi_clk\n");
		ret = PTR_ERR(mdwc->utmi_clk);
		return ret;
	}

	clk_set_rate(mdwc->utmi_clk, mdwc->utmi_clk_rate);
	mdwc->bus_aggr_clk = devm_clk_get(mdwc->dev, "bus_aggr_clk");
	if (IS_ERR(mdwc->bus_aggr_clk))
		mdwc->bus_aggr_clk = NULL;

	mdwc->noc_aggr_clk = devm_clk_get(mdwc->dev, "noc_aggr_clk");
	if (IS_ERR(mdwc->noc_aggr_clk))
		mdwc->noc_aggr_clk = NULL;

	if (of_property_match_string(mdwc->dev->of_node,
				"clock-names", "cfg_ahb_clk") >= 0) {
		mdwc->cfg_ahb_clk = devm_clk_get(mdwc->dev, "cfg_ahb_clk");
		if (IS_ERR(mdwc->cfg_ahb_clk)) {
			ret = PTR_ERR(mdwc->cfg_ahb_clk);
			mdwc->cfg_ahb_clk = NULL;
			if (ret != -EPROBE_DEFER)
				dev_err(mdwc->dev,
					"failed to get cfg_ahb_clk ret %d\n",
					ret);
			return ret;
		}
	}

	return 0;
}

static int dwc3_msm_id_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct dwc3 *dwc;
	struct extcon_dev *edev = ptr;
	struct extcon_nb *enb = container_of(nb, struct extcon_nb, id_nb);
	struct dwc3_msm *mdwc = enb->mdwc;
	enum dwc3_id_state id;

	if (!edev || !mdwc)
		return NOTIFY_DONE;

	dwc = platform_get_drvdata(mdwc->dwc3);

	dbg_event(0xFF, "extcon idx", enb->idx);

	id = event ? DWC3_ID_GROUND : DWC3_ID_FLOAT;

	if (mdwc->id_state == id)
		return NOTIFY_DONE;

	mdwc->ext_idx = enb->idx;

	dev_dbg(mdwc->dev, "host:%ld (id:%d) event received\n", event, id);

	mdwc->id_state = id;
	dbg_event(0xFF, "id_state", mdwc->id_state);
	queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

	return NOTIFY_DONE;
}


static void check_for_sdp_connection(struct work_struct *w)
{
	struct dwc3_msm *mdwc =
		container_of(w, struct dwc3_msm, sdp_check.work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (!mdwc->vbus_active)
		return;

	/* USB 3.1 compliance equipment usually repoted as floating
	 * charger as HS dp/dm lines are never connected. Do not
	 * tear down USB stack if compliance parameter is set
	 */
	if (mdwc->usb_compliance_mode)
		return;

	/* floating D+/D- lines detected */
	if (dwc->gadget.state < USB_STATE_DEFAULT &&
		dwc3_gadget_get_link_state(dwc) != DWC3_LINK_STATE_CMPLY) {
		mdwc->vbus_active = 0;
		dbg_event(0xFF, "Q RW SPD CHK", mdwc->vbus_active);
		queue_work(mdwc->dwc3_wq, &mdwc->resume_work);
	}
}

static int dwc3_msm_vbus_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct dwc3 *dwc;
	struct extcon_dev *edev = ptr;
	struct extcon_nb *enb = container_of(nb, struct extcon_nb, vbus_nb);
	struct dwc3_msm *mdwc = enb->mdwc;

	if (!edev || !mdwc)
		return NOTIFY_DONE;

	dwc = platform_get_drvdata(mdwc->dwc3);

	dbg_event(0xFF, "extcon idx", enb->idx);

	if (mdwc->vbus_active == event)
		return NOTIFY_DONE;

	mdwc->ext_idx = enb->idx;

	dev_dbg(mdwc->dev, "vbus:%ld event received\n", event);

	mdwc->vbus_active = event;
	if ((dwc->dr_mode == USB_DR_MODE_OTG) && !mdwc->in_restart)
		queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

	return NOTIFY_DONE;
}

static int dwc3_msm_extcon_register(struct dwc3_msm *mdwc)
{
	struct device_node *node = mdwc->dev->of_node;
	struct extcon_dev *edev;
	int idx, extcon_cnt, ret = 0;
	bool check_vbus_state, check_id_state, phandle_found = false;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (!of_property_read_bool(node, "extcon")) {
		if (dwc->dr_mode == USB_DR_MODE_OTG) {
			dev_dbg(mdwc->dev, "%s: no extcon, simulate vbus connect\n",
								__func__);
			mdwc->vbus_active = true;
			queue_work(mdwc->dwc3_wq, &mdwc->resume_work);
		}
		return 0;
	}

	extcon_cnt = of_count_phandle_with_args(node, "extcon", NULL);
	if (extcon_cnt < 0) {
		dev_err(mdwc->dev, "of_count_phandle_with_args failed\n");
		return -ENODEV;
	}

	mdwc->extcon = devm_kcalloc(mdwc->dev, extcon_cnt,
					sizeof(*mdwc->extcon), GFP_KERNEL);
	if (!mdwc->extcon)
		return -ENOMEM;

	for (idx = 0; idx < extcon_cnt; idx++) {
		edev = extcon_get_edev_by_phandle(mdwc->dev, idx);
		if (IS_ERR(edev) && PTR_ERR(edev) != -ENODEV)
			return PTR_ERR(edev);

		if (IS_ERR_OR_NULL(edev))
			continue;

		check_vbus_state = check_id_state = true;
		phandle_found = true;

		mdwc->extcon[idx].mdwc = mdwc;
		mdwc->extcon[idx].edev = edev;
		mdwc->extcon[idx].idx = idx;

		mdwc->extcon[idx].vbus_nb.notifier_call =
						dwc3_msm_vbus_notifier;
		ret = extcon_register_notifier(edev, EXTCON_USB,
						&mdwc->extcon[idx].vbus_nb);
		if (ret < 0)
			check_vbus_state = false;

		mdwc->extcon[idx].id_nb.notifier_call = dwc3_msm_id_notifier;
		ret = extcon_register_notifier(edev, EXTCON_USB_HOST,
						&mdwc->extcon[idx].id_nb);
		if (ret < 0)
			check_id_state = false;

		mdwc->extcon[idx].host_restart_nb.notifier_call =
					dwc3_restart_usb_host_mode;
		extcon_register_blocking_notifier(edev, EXTCON_USB_HOST,
					&mdwc->extcon[idx].host_restart_nb);

		/* Update initial VBUS/ID state */
		if (check_vbus_state && extcon_get_state(edev, EXTCON_USB))
			dwc3_msm_vbus_notifier(&mdwc->extcon[idx].vbus_nb,
						true, edev);
		else  if (check_id_state &&
				extcon_get_state(edev, EXTCON_USB_HOST))
			dwc3_msm_id_notifier(&mdwc->extcon[idx].id_nb,
						true, edev);
	}

	if (!phandle_found) {
		dev_err(mdwc->dev, "no extcon device found\n");
		return -ENODEV;
	}

	return 0;
}

#define SMMU_BASE	0x60000000 /* Device address range base */
#define SMMU_SIZE	0x90000000 /* Device address range size */

static int dwc3_msm_init_iommu(struct dwc3_msm *mdwc)
{
	struct device_node *node = mdwc->dev->of_node;
	int atomic_ctx = 1, s1_bypass;
	int ret;

	if (!of_property_read_bool(node, "iommus"))
		return 0;

	mdwc->iommu_map = arm_iommu_create_mapping(&platform_bus_type,
			SMMU_BASE, SMMU_SIZE);
	if (IS_ERR_OR_NULL(mdwc->iommu_map)) {
		ret = PTR_ERR(mdwc->iommu_map) ?: -ENODEV;
		dev_err(mdwc->dev, "Failed to create IOMMU mapping (%d)\n",
				ret);
		return ret;
	}
	dev_dbg(mdwc->dev, "IOMMU mapping created: %pK\n", mdwc->iommu_map);

	ret = iommu_domain_set_attr(mdwc->iommu_map->domain, DOMAIN_ATTR_ATOMIC,
			&atomic_ctx);
	if (ret) {
		dev_err(mdwc->dev, "IOMMU set atomic attribute failed (%d)\n",
			ret);
		goto release_mapping;
	}

	s1_bypass = of_property_read_bool(node, "qcom,smmu-s1-bypass");
	ret = iommu_domain_set_attr(mdwc->iommu_map->domain,
			DOMAIN_ATTR_S1_BYPASS, &s1_bypass);
	if (ret) {
		dev_err(mdwc->dev, "IOMMU set s1 bypass (%d) failed (%d)\n",
			s1_bypass, ret);
		goto release_mapping;
	}

	ret = arm_iommu_attach_device(mdwc->dev, mdwc->iommu_map);
	if (ret) {
		dev_err(mdwc->dev, "IOMMU attach failed (%d)\n", ret);
		goto release_mapping;
	}
	dev_dbg(mdwc->dev, "attached to IOMMU\n");

	return 0;

release_mapping:
	arm_iommu_release_mapping(mdwc->iommu_map);
	mdwc->iommu_map = NULL;
	return ret;
}

static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	if (mdwc->vbus_active)
		return snprintf(buf, PAGE_SIZE, "peripheral\n");
	if (mdwc->id_state == DWC3_ID_GROUND)
		return snprintf(buf, PAGE_SIZE, "host\n");

	return snprintf(buf, PAGE_SIZE, "none\n");
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "peripheral")) {
		mdwc->vbus_active = true;
		mdwc->id_state = DWC3_ID_FLOAT;
	} else if (sysfs_streq(buf, "host")) {
		mdwc->vbus_active = false;
		mdwc->id_state = DWC3_ID_GROUND;
	} else {
		mdwc->vbus_active = false;
		mdwc->id_state = DWC3_ID_FLOAT;
	}

	dwc3_ext_event_notify(mdwc);

	return count;
}

static DEVICE_ATTR_RW(mode);
static void msm_dwc3_perf_vote_work(struct work_struct *w);

/* This node only shows max speed supported dwc3 and it should be
 * same as what is reported in udc/core.c max_speed node. For current
 * operating gadget speed, query current_speed node which is implemented
 * by udc/core.c
 */
static ssize_t speed_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			usb_speed_string(dwc->maximum_speed));
}

static ssize_t speed_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	enum usb_device_speed req_speed = USB_SPEED_UNKNOWN;

	/* DEVSPD can only have values SS(0x4), HS(0x0) and FS(0x1).
	 * per 3.20a data book. Allow only these settings. Note that,
	 * xhci does not support full-speed only mode.
	 */
	if (sysfs_streq(buf, "full"))
		req_speed = USB_SPEED_FULL;
	else if (sysfs_streq(buf, "high"))
		req_speed = USB_SPEED_HIGH;
	else if (sysfs_streq(buf, "super"))
		req_speed = USB_SPEED_SUPER;
	else
		return -EINVAL;

	/* restart usb only works for device mode. Perform manual cable
	 * plug in/out for host mode restart.
	 */
	if (req_speed != dwc->maximum_speed &&
			req_speed <= dwc->max_hw_supp_speed) {
		mdwc->override_usb_speed = req_speed;
		schedule_work(&mdwc->restart_usb_work);
	}

	return count;
}
static DEVICE_ATTR_RW(speed);

static ssize_t usb_compliance_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%c\n",
			mdwc->usb_compliance_mode ? 'Y' : 'N');
}

static ssize_t usb_compliance_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	ret = strtobool(buf, &mdwc->usb_compliance_mode);

	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(usb_compliance_mode);

static ssize_t bus_vote_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	if (mdwc->override_bus_vote == BUS_VOTE_MIN)
		return snprintf(buf, PAGE_SIZE, "%s\n",
			"Fixed bus vote: min");
	else if (mdwc->override_bus_vote == BUS_VOTE_MAX)
		return snprintf(buf, PAGE_SIZE, "%s\n",
			"Fixed bus vote: max");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n",
			"Do not have fixed bus vote");
}

static ssize_t bus_vote_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	bool bv_fixed = false;
	enum bus_vote bv;

	if (sysfs_streq(buf, "min")
			&& (mdwc->bus_scale_table->num_usecases
			>= (BUS_VOTE_MIN + 1))) {
		bv_fixed = true;
		mdwc->override_bus_vote = BUS_VOTE_MIN;
	} else if (sysfs_streq(buf, "max")
			&& (mdwc->bus_scale_table->num_usecases
			>= (BUS_VOTE_MAX + 1))) {
		bv_fixed = true;
		mdwc->override_bus_vote = BUS_VOTE_MAX;
	} else if (sysfs_streq(buf, "cancel")) {
		bv_fixed = false;
		mdwc->override_bus_vote = BUS_VOTE_NONE;
	} else {
		dev_err(dev, "min/max/cancel only.\n");
		return -EINVAL;
	}

	/* Update bus vote value only when not suspend */
	if (!atomic_read(&dwc->in_lpm)) {
		if (bv_fixed)
			bv = mdwc->override_bus_vote;
		else if (mdwc->in_host_mode
			&& (mdwc->max_rh_port_speed == USB_SPEED_HIGH))
			bv = BUS_VOTE_SVS;
		else
			bv = BUS_VOTE_NOMINAL;

		dwc3_msm_update_bus_bw(mdwc, bv);
	}

	return count;
}
static DEVICE_ATTR_RW(bus_vote);

static int dwc3_msm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node, *dwc3_node;
	struct device	*dev = &pdev->dev;
	struct dwc3_msm *mdwc;
	struct dwc3	*dwc;
	struct resource *res;
	bool host_mode;
	int ret = 0, i;
	u32 val;
	unsigned long irq_type;

	mdwc = devm_kzalloc(&pdev->dev, sizeof(*mdwc), GFP_KERNEL);
	if (!mdwc)
		return -ENOMEM;

	platform_set_drvdata(pdev, mdwc);
	mdwc->dev = &pdev->dev;

	INIT_LIST_HEAD(&mdwc->req_complete_list);
	INIT_WORK(&mdwc->resume_work, dwc3_resume_work);
	INIT_WORK(&mdwc->restart_usb_work, dwc3_restart_usb_work);
	INIT_WORK(&mdwc->vbus_draw_work, dwc3_msm_vbus_draw_work);
	INIT_DELAYED_WORK(&mdwc->sm_work, dwc3_otg_sm_work);
	INIT_DELAYED_WORK(&mdwc->perf_vote_work, msm_dwc3_perf_vote_work);
	INIT_DELAYED_WORK(&mdwc->sdp_check, check_for_sdp_connection);

	mdwc->dwc3_wq = alloc_ordered_workqueue("dwc3_wq", 0);
	if (!mdwc->dwc3_wq) {
		pr_err("%s: Unable to create workqueue dwc3_wq\n", __func__);
		return -ENOMEM;
	}

	/* Get all clks and gdsc reference */
	ret = dwc3_msm_get_clk_gdsc(mdwc);
	if (ret) {
		dev_err(&pdev->dev, "error getting clock or gdsc.\n");
		goto err;
	}

	mdwc->id_state = DWC3_ID_FLOAT;
	set_bit(ID, &mdwc->inputs);

	mdwc->charging_disabled = of_property_read_bool(node,
				"qcom,charging-disabled");

	ret = of_property_read_u32(node, "qcom,lpm-to-suspend-delay-ms",
				&mdwc->lpm_to_suspend_delay);
	if (ret) {
		dev_dbg(&pdev->dev, "setting lpm_to_suspend_delay to zero.\n");
		mdwc->lpm_to_suspend_delay = 0;
	}

	memcpy(mdwc->wakeup_irq, usb_irq_info, sizeof(usb_irq_info));
	for (i = 0; i < USB_MAX_IRQ; i++) {
		irq_type = IRQF_TRIGGER_RISING | IRQF_EARLY_RESUME |
						IRQF_ONESHOT;
		mdwc->wakeup_irq[i].irq = platform_get_irq_byname(pdev,
					mdwc->wakeup_irq[i].name);
		if (mdwc->wakeup_irq[i].irq < 0) {
			/* pwr_evnt_irq is only mandatory irq */
			if (!strcmp(mdwc->wakeup_irq[i].name,
						"pwr_event_irq")) {
				dev_err(&pdev->dev, "get_irq for %s failed\n\n",
						mdwc->wakeup_irq[i].name);
				ret = -EINVAL;
				goto err;
			}
			mdwc->wakeup_irq[i].irq = 0;
		} else {
			irq_set_status_flags(mdwc->wakeup_irq[i].irq,
						IRQ_NOAUTOEN);
			/* ss_phy_irq is level trigger interrupt */
			if (!strcmp(mdwc->wakeup_irq[i].name, "ss_phy_irq"))
				irq_type = IRQF_TRIGGER_HIGH | IRQF_ONESHOT |
					IRQ_TYPE_LEVEL_HIGH | IRQF_EARLY_RESUME;

			ret = devm_request_threaded_irq(&pdev->dev,
					mdwc->wakeup_irq[i].irq,
					msm_dwc3_pwr_irq,
					msm_dwc3_pwr_irq_thread,
					irq_type,
					mdwc->wakeup_irq[i].name, mdwc);
			if (ret) {
				dev_err(&pdev->dev, "irq req %s failed: %d\n\n",
						mdwc->wakeup_irq[i].name, ret);
				goto err;
			}
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "core_base");
	if (!res) {
		dev_err(&pdev->dev, "missing memory base resource\n");
		ret = -ENODEV;
		goto err;
	}

	mdwc->base = devm_ioremap_nocache(&pdev->dev, res->start,
			resource_size(res));
	if (!mdwc->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENODEV;
		goto err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"ahb2phy_base");
	if (res) {
		mdwc->ahb2phy_base = devm_ioremap_nocache(&pdev->dev,
					res->start, resource_size(res));
		if (IS_ERR_OR_NULL(mdwc->ahb2phy_base)) {
			dev_err(dev, "couldn't find ahb2phy_base addr.\n");
			mdwc->ahb2phy_base = NULL;
		} else {
			/*
			 * On some targets cfg_ahb_clk depends upon usb gdsc
			 * regulator. If cfg_ahb_clk is enabled without
			 * turning on usb gdsc regulator clk is stuck off.
			 */
			dwc3_msm_config_gdsc(mdwc, 1);
			clk_prepare_enable(mdwc->cfg_ahb_clk);
			/* Configure AHB2PHY for one wait state read/write*/
			val = readl_relaxed(mdwc->ahb2phy_base +
					PERIPH_SS_AHB2PHY_TOP_CFG);
			if (val != ONE_READ_WRITE_WAIT) {
				writel_relaxed(ONE_READ_WRITE_WAIT,
					mdwc->ahb2phy_base +
					PERIPH_SS_AHB2PHY_TOP_CFG);
				/* complete above write before using USB PHY */
				mb();
			}
			clk_disable_unprepare(mdwc->cfg_ahb_clk);
			dwc3_msm_config_gdsc(mdwc, 0);
		}
	}

	mdwc->dbm = dwc3_init_dbm(&pdev->dev, mdwc->base);
	if (IS_ERR(mdwc->dbm)) {
		dev_warn(&pdev->dev, "unable to init dbm device, skipping...\n");
		mdwc->dbm = NULL;
	}

	/* Add power event if the dbm indicates coming out of L1 by interrupt */
	if (mdwc->dbm && dbm_l1_lpm_interrupt(mdwc->dbm)) {
		if (!mdwc->wakeup_irq[PWR_EVNT_IRQ].irq) {
			dev_err(&pdev->dev,
				"need pwr_event_irq exiting L1\n");
			ret = -EINVAL;
			goto err;
		}
	}

	if (of_property_read_u32(node, "qcom,dwc-usb3-msm-tx-fifo-size",
				 &mdwc->tx_fifo_size))
		dev_err(&pdev->dev,
			"unable to read platform data tx fifo size\n");

	ret = of_property_read_u32(node, "qcom,num-gsi-evt-buffs",
				&mdwc->num_gsi_event_buffers);

	mdwc->use_pdc_interrupts = of_property_read_bool(node,
				"qcom,use-pdc-interrupts");
	dwc3_set_notifier(&dwc3_msm_notify_event);

	ret = dwc3_msm_init_iommu(mdwc);
	if (ret)
		goto err;

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64))) {
		dev_err(&pdev->dev, "setting DMA mask to 64 failed.\n");
		if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32))) {
			dev_err(&pdev->dev, "setting DMA mask to 32 failed.\n");
			ret = -EOPNOTSUPP;
			goto uninit_iommu;
		}
	}

	/* Assumes dwc3 is the first DT child of dwc3-msm */
	dwc3_node = of_get_next_available_child(node, NULL);
	if (!dwc3_node) {
		dev_err(&pdev->dev, "failed to find dwc3 child\n");
		ret = -ENODEV;
		goto uninit_iommu;
	}

	ret = of_platform_populate(node, NULL, NULL, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev,
				"failed to add create dwc3 core\n");
		of_node_put(dwc3_node);
		goto uninit_iommu;
	}

	mdwc->dwc3 = of_find_device_by_node(dwc3_node);
	of_node_put(dwc3_node);
	if (!mdwc->dwc3) {
		dev_err(&pdev->dev, "failed to get dwc3 platform device\n");
		goto put_dwc3;
	}

	mdwc->hs_phy = devm_usb_get_phy_by_phandle(&mdwc->dwc3->dev,
							"usb-phy", 0);
	if (IS_ERR(mdwc->hs_phy)) {
		dev_err(&pdev->dev, "unable to get hsphy device\n");
		ret = PTR_ERR(mdwc->hs_phy);
		goto put_dwc3;
	}
	mdwc->ss_phy = devm_usb_get_phy_by_phandle(&mdwc->dwc3->dev,
							"usb-phy", 1);
	if (IS_ERR(mdwc->ss_phy)) {
		dev_err(&pdev->dev, "unable to get ssphy device\n");
		ret = PTR_ERR(mdwc->ss_phy);
		goto put_dwc3;
	}

	mdwc->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (mdwc->bus_scale_table) {
		mdwc->bus_perf_client =
			msm_bus_scale_register_client(mdwc->bus_scale_table);
	}

	dwc = platform_get_drvdata(mdwc->dwc3);
	if (!dwc) {
		dev_err(&pdev->dev, "Failed to get dwc3 device\n");
		goto put_dwc3;
	}

	/* IOMMU will be reattached upon each resume/connect */
	if (mdwc->iommu_map)
		arm_iommu_detach_device(mdwc->dev);

	/*
	 * Clocks and regulators will not be turned on until the first time
	 * runtime PM resume is called. This is to allow for booting up with
	 * charger already connected so as not to disturb PHY line states.
	 */
	mdwc->lpm_flags = MDWC3_POWER_COLLAPSE | MDWC3_SS_PHY_SUSPEND;
	atomic_set(&dwc->in_lpm, 1);
	pm_runtime_set_autosuspend_delay(mdwc->dev, 1000);
	pm_runtime_use_autosuspend(mdwc->dev);
	device_init_wakeup(mdwc->dev, 1);

	if (of_property_read_bool(node, "qcom,disable-dev-mode-pm"))
		pm_runtime_get_noresume(mdwc->dev);

	ret = of_property_read_u32(node, "qcom,pm-qos-latency",
				&mdwc->pm_qos_latency);
	if (ret) {
		dev_dbg(&pdev->dev, "setting pm-qos-latency to zero.\n");
		mdwc->pm_qos_latency = 0;
	}

	mutex_init(&mdwc->suspend_resume_mutex);

	ret = dwc3_msm_extcon_register(mdwc);
	if (ret)
		goto put_dwc3;

	schedule_delayed_work(&mdwc->sm_work, 0);

	device_create_file(&pdev->dev, &dev_attr_mode);
	device_create_file(&pdev->dev, &dev_attr_speed);
	device_create_file(&pdev->dev, &dev_attr_usb_compliance_mode);
	device_create_file(&pdev->dev, &dev_attr_bus_vote);

	host_mode = usb_get_dr_mode(&mdwc->dwc3->dev) == USB_DR_MODE_HOST;
	if (host_mode) {
		dev_dbg(&pdev->dev, "DWC3 in host only mode\n");
		mdwc->id_state = DWC3_ID_GROUND;
		dwc3_ext_event_notify(mdwc);
	}

	return 0;

put_dwc3:
	if (mdwc->bus_perf_client)
		msm_bus_scale_unregister_client(mdwc->bus_perf_client);

uninit_iommu:
	if (mdwc->iommu_map) {
		arm_iommu_detach_device(mdwc->dev);
		arm_iommu_release_mapping(mdwc->iommu_map);
	}
	of_platform_depopulate(&pdev->dev);
err:
	destroy_workqueue(mdwc->dwc3_wq);
	return ret;
}

static int dwc3_msm_remove(struct platform_device *pdev)
{
	struct dwc3_msm	*mdwc = platform_get_drvdata(pdev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	int ret_pm;

	device_remove_file(&pdev->dev, &dev_attr_mode);
	if (mdwc->usb_psy)
		power_supply_put(mdwc->usb_psy);

	/*
	 * In case of system suspend, pm_runtime_get_sync fails.
	 * Hence turn ON the clocks manually.
	 */
	ret_pm = pm_runtime_get_sync(mdwc->dev);
	dbg_event(0xFF, "Remov gsyn", ret_pm);
	if (ret_pm < 0) {
		dev_err(mdwc->dev,
			"pm_runtime_get_sync failed with %d\n", ret_pm);
		if (mdwc->noc_aggr_clk)
			clk_prepare_enable(mdwc->noc_aggr_clk);
		clk_prepare_enable(mdwc->utmi_clk);
		clk_prepare_enable(mdwc->core_clk);
		clk_prepare_enable(mdwc->iface_clk);
		clk_prepare_enable(mdwc->sleep_clk);
		if (mdwc->bus_aggr_clk)
			clk_prepare_enable(mdwc->bus_aggr_clk);
		clk_prepare_enable(mdwc->xo_clk);
	}

	cancel_delayed_work_sync(&mdwc->perf_vote_work);
	cancel_delayed_work_sync(&mdwc->sm_work);

	if (mdwc->hs_phy)
		mdwc->hs_phy->flags &= ~PHY_HOST_MODE;
	of_platform_depopulate(&pdev->dev);

	dbg_event(0xFF, "Remov put", 0);
	pm_runtime_disable(mdwc->dev);
	pm_runtime_barrier(mdwc->dev);
	pm_runtime_put_sync(mdwc->dev);
	pm_runtime_set_suspended(mdwc->dev);
	device_wakeup_disable(mdwc->dev);

	if (mdwc->bus_perf_client)
		msm_bus_scale_unregister_client(mdwc->bus_perf_client);

	if (!IS_ERR_OR_NULL(mdwc->vbus_reg))
		regulator_disable(mdwc->vbus_reg);

	if (mdwc->wakeup_irq[HS_PHY_IRQ].irq)
		disable_irq(mdwc->wakeup_irq[HS_PHY_IRQ].irq);
	if (mdwc->wakeup_irq[DP_HS_PHY_IRQ].irq)
		disable_irq(mdwc->wakeup_irq[DP_HS_PHY_IRQ].irq);
	if (mdwc->wakeup_irq[DM_HS_PHY_IRQ].irq)
		disable_irq(mdwc->wakeup_irq[DM_HS_PHY_IRQ].irq);
	if (mdwc->wakeup_irq[SS_PHY_IRQ].irq)
		disable_irq(mdwc->wakeup_irq[SS_PHY_IRQ].irq);
	disable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);

	clk_disable_unprepare(mdwc->utmi_clk);
	clk_set_rate(mdwc->core_clk, 19200000);
	clk_disable_unprepare(mdwc->core_clk);
	clk_disable_unprepare(mdwc->iface_clk);
	clk_disable_unprepare(mdwc->sleep_clk);
	clk_disable_unprepare(mdwc->xo_clk);
	clk_put(mdwc->xo_clk);

	dwc3_msm_config_gdsc(mdwc, 0);

	if (mdwc->iommu_map) {
		if (!atomic_read(&dwc->in_lpm))
			arm_iommu_detach_device(mdwc->dev);
		arm_iommu_release_mapping(mdwc->iommu_map);
	}

	return 0;
}

static int dwc3_msm_host_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct dwc3_msm *mdwc = container_of(nb, struct dwc3_msm, host_nb);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	struct usb_device *udev = ptr;

	if (event != USB_DEVICE_ADD && event != USB_DEVICE_REMOVE)
		return NOTIFY_DONE;

	/*
	 * For direct-attach devices, new udev is direct child of root hub
	 * i.e. dwc -> xhci -> root_hub -> udev
	 * root_hub's udev->parent==NULL, so traverse struct device hierarchy
	 */
	if (udev->parent && !udev->parent->parent &&
			udev->dev.parent->parent == &dwc->xhci->dev) {
		if (event == USB_DEVICE_ADD && udev->actconfig) {
			if (!dwc3_msm_is_ss_rhport_connected(mdwc)) {
				/*
				 * Core clock rate can be reduced only if root
				 * hub SS port is not enabled/connected.
				 */
				clk_set_rate(mdwc->core_clk,
				mdwc->core_clk_rate_hs);
				dev_dbg(mdwc->dev,
					"set hs core clk rate %ld\n",
					mdwc->core_clk_rate_hs);
				mdwc->max_rh_port_speed = USB_SPEED_HIGH;
				dwc3_msm_update_bus_bw(mdwc, BUS_VOTE_SVS);
			} else {
				mdwc->max_rh_port_speed = USB_SPEED_SUPER;
			}
		} else {
			/* set rate back to default core clk rate */
			clk_set_rate(mdwc->core_clk, mdwc->core_clk_rate);
			dev_dbg(mdwc->dev, "set core clk rate %ld\n",
				mdwc->core_clk_rate);
			mdwc->max_rh_port_speed = USB_SPEED_UNKNOWN;
			dwc3_msm_update_bus_bw(mdwc, BUS_VOTE_NOMINAL);
		}
	}

	return NOTIFY_DONE;
}

static void msm_dwc3_perf_vote_update(struct dwc3_msm *mdwc, bool perf_mode)
{
	static bool curr_perf_mode;
	int latency = mdwc->pm_qos_latency;

	if ((curr_perf_mode == perf_mode) || !latency)
		return;

	if (perf_mode)
		pm_qos_update_request(&mdwc->pm_qos_req_dma, latency);
	else
		pm_qos_update_request(&mdwc->pm_qos_req_dma,
						PM_QOS_DEFAULT_VALUE);

	curr_perf_mode = perf_mode;
	pr_debug("%s: latency updated to: %d\n", __func__,
			perf_mode ? latency : PM_QOS_DEFAULT_VALUE);
}

static void msm_dwc3_perf_vote_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm,
						perf_vote_work.work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	static unsigned long	last_irq_cnt;
	bool in_perf_mode = false;

	if (dwc->irq_cnt - last_irq_cnt >= PM_QOS_THRESHOLD)
		in_perf_mode = true;

	pr_debug("%s: in_perf_mode:%u, interrupts in last sample:%lu\n",
		 __func__, in_perf_mode, (dwc->irq_cnt - last_irq_cnt));

	last_irq_cnt = dwc->irq_cnt;
	msm_dwc3_perf_vote_update(mdwc, in_perf_mode);
	schedule_delayed_work(&mdwc->perf_vote_work,
			msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
}

#define VBUS_REG_CHECK_DELAY	(msecs_to_jiffies(1000))

/**
 * dwc3_otg_start_host -  helper function for starting/stoping the host
 * controller driver.
 *
 * @mdwc: Pointer to the dwc3_msm structure.
 * @on: start / stop the host controller driver.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_start_host(struct dwc3_msm *mdwc, int on)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	int ret = 0;

	/*
	 * The vbus_reg pointer could have multiple values
	 * NULL: regulator_get() hasn't been called, or was previously deferred
	 * IS_ERR: regulator could not be obtained, so skip using it
	 * Valid pointer otherwise
	 */
	if (!mdwc->vbus_reg) {
		mdwc->vbus_reg = devm_regulator_get_optional(mdwc->dev,
					"vbus_dwc3");
		if (IS_ERR(mdwc->vbus_reg) &&
				PTR_ERR(mdwc->vbus_reg) == -EPROBE_DEFER) {
			/* regulators may not be ready, so retry again later */
			mdwc->vbus_reg = NULL;
			return -EPROBE_DEFER;
		}
	}

	if (on) {
		dev_dbg(mdwc->dev, "%s: turn on host\n", __func__);

		mdwc->hs_phy->flags |= PHY_HOST_MODE;
		if (dwc->maximum_speed >= USB_SPEED_SUPER) {
			mdwc->ss_phy->flags |= PHY_HOST_MODE;
			usb_phy_notify_connect(mdwc->ss_phy,
						USB_SPEED_SUPER);
		}

		usb_phy_notify_connect(mdwc->hs_phy, USB_SPEED_HIGH);
		pm_runtime_get_sync(mdwc->dev);
		dbg_event(0xFF, "StrtHost gync",
			atomic_read(&mdwc->dev->power.usage_count));
		if (!IS_ERR_OR_NULL(mdwc->vbus_reg))
			ret = regulator_enable(mdwc->vbus_reg);
		if (ret) {
			dev_err(mdwc->dev, "unable to enable vbus_reg\n");
			mdwc->hs_phy->flags &= ~PHY_HOST_MODE;
			mdwc->ss_phy->flags &= ~PHY_HOST_MODE;
			pm_runtime_put_sync(mdwc->dev);
			dbg_event(0xFF, "vregerr psync",
				atomic_read(&mdwc->dev->power.usage_count));
			return ret;
		}


		mdwc->host_nb.notifier_call = dwc3_msm_host_notifier;
		usb_register_notify(&mdwc->host_nb);

		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_HOST);
		dwc3_en_sleep_mode(dwc);
		mdwc->usbdev_nb.notifier_call = msm_dwc3_usbdev_notify;
		usb_register_atomic_notify(&mdwc->usbdev_nb);
		ret = dwc3_host_init(dwc);
		if (ret) {
			dev_err(mdwc->dev,
				"%s: failed to add XHCI pdev ret=%d\n",
				__func__, ret);
			if (!IS_ERR_OR_NULL(mdwc->vbus_reg))
				regulator_disable(mdwc->vbus_reg);

			mdwc->hs_phy->flags &= ~PHY_HOST_MODE;
			mdwc->ss_phy->flags &= ~PHY_HOST_MODE;
			pm_runtime_put_sync(mdwc->dev);
			dbg_event(0xFF, "pdeverr psync",
				atomic_read(&mdwc->dev->power.usage_count));
			usb_unregister_notify(&mdwc->host_nb);
			return ret;
		}

		mdwc->in_host_mode = true;
		dwc3_usb3_phy_suspend(dwc, true);

		/* Reduce the U3 exit handshake timer from 8us to approximately
		 * 300ns to avoid lfps handshake interoperability issues
		 */
		if (dwc->revision == DWC3_USB31_REVISION_170A) {
			dwc3_msm_write_reg_field(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0),
					GEN2_U3_EXIT_RSP_RX_CLK_MASK, 6);
			dwc3_msm_write_reg_field(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0),
					GEN1_U3_EXIT_RSP_RX_CLK_MASK, 5);
			dev_dbg(mdwc->dev, "LU3:%08x\n",
				dwc3_msm_read_reg(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0)));
		}

		/* xHCI should have incremented child count as necessary */
		dbg_event(0xFF, "StrtHost psync",
			atomic_read(&mdwc->dev->power.usage_count));
		pm_runtime_mark_last_busy(mdwc->dev);
		pm_runtime_put_sync_autosuspend(mdwc->dev);
#ifdef CONFIG_SMP
		mdwc->pm_qos_req_dma.type = PM_QOS_REQ_AFFINE_IRQ;
		mdwc->pm_qos_req_dma.irq = dwc->irq;
#endif
		pm_qos_add_request(&mdwc->pm_qos_req_dma,
				PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		/* start in perf mode for better performance initially */
		msm_dwc3_perf_vote_update(mdwc, true);
		schedule_delayed_work(&mdwc->perf_vote_work,
				msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
	} else {
		dev_dbg(mdwc->dev, "%s: turn off host\n", __func__);

		usb_unregister_atomic_notify(&mdwc->usbdev_nb);
		if (!IS_ERR_OR_NULL(mdwc->vbus_reg))
			ret = regulator_disable(mdwc->vbus_reg);
		if (ret) {
			dev_err(mdwc->dev, "unable to disable vbus_reg\n");
			return ret;
		}

		cancel_delayed_work_sync(&mdwc->perf_vote_work);
		msm_dwc3_perf_vote_update(mdwc, false);
		pm_qos_remove_request(&mdwc->pm_qos_req_dma);

		pm_runtime_get_sync(mdwc->dev);
		dbg_event(0xFF, "StopHost gsync",
			atomic_read(&mdwc->dev->power.usage_count));
		usb_phy_notify_disconnect(mdwc->hs_phy, USB_SPEED_HIGH);
		if (mdwc->ss_phy->flags & PHY_HOST_MODE) {
			usb_phy_notify_disconnect(mdwc->ss_phy,
					USB_SPEED_SUPER);
			mdwc->ss_phy->flags &= ~PHY_HOST_MODE;
		}

		mdwc->hs_phy->flags &= ~PHY_HOST_MODE;
		dwc3_host_exit(dwc);
		usb_unregister_notify(&mdwc->host_nb);

		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_DEVICE);
		dwc3_usb3_phy_suspend(dwc, false);
		mdwc->in_host_mode = false;

		pm_runtime_put_sync_suspend(mdwc->dev);
		dbg_event(0xFF, "StopHost psync",
			atomic_read(&mdwc->dev->power.usage_count));
	}

	return 0;
}

static void dwc3_override_vbus_status(struct dwc3_msm *mdwc, bool vbus_present)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	/* Update OTG VBUS Valid from HSPHY to controller */
	dwc3_msm_write_readback(mdwc->base, HS_PHY_CTRL_REG,
		vbus_present ? UTMI_OTG_VBUS_VALID | SW_SESSVLD_SEL :
		UTMI_OTG_VBUS_VALID,
		vbus_present ? UTMI_OTG_VBUS_VALID | SW_SESSVLD_SEL : 0);

	/* Update only if Super Speed is supported */
	if (dwc->maximum_speed >= USB_SPEED_SUPER) {
		/* Update VBUS Valid from SSPHY to controller */
		dwc3_msm_write_readback(mdwc->base, SS_PHY_CTRL_REG,
			LANE0_PWR_PRESENT,
			vbus_present ? LANE0_PWR_PRESENT : 0);
	}
}

/**
 * dwc3_otg_start_peripheral -  bind/unbind the peripheral controller.
 *
 * @mdwc: Pointer to the dwc3_msm structure.
 * @on:   Turn ON/OFF the gadget.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_start_peripheral(struct dwc3_msm *mdwc, int on)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	pm_runtime_get_sync(mdwc->dev);
	dbg_event(0xFF, "StrtGdgt gsync",
		atomic_read(&mdwc->dev->power.usage_count));

	if (on) {
		dev_dbg(mdwc->dev, "%s: turn on gadget %s\n",
					__func__, dwc->gadget.name);

		dwc3_override_vbus_status(mdwc, true);
		usb_phy_notify_connect(mdwc->hs_phy, USB_SPEED_HIGH);
		usb_phy_notify_connect(mdwc->ss_phy, USB_SPEED_SUPER);

		/*
		 * Core reset is not required during start peripheral. Only
		 * DBM reset is required, hence perform only DBM reset here.
		 */
		dwc3_msm_block_reset(mdwc, false);
		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_DEVICE);
		dwc3_dis_sleep_mode(dwc);
		mdwc->in_device_mode = true;

		/* Reduce the U3 exit handshake timer from 8us to approximately
		 * 300ns to avoid lfps handshake interoperability issues
		 */
		if (dwc->revision == DWC3_USB31_REVISION_170A) {
			dwc3_msm_write_reg_field(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0),
					GEN2_U3_EXIT_RSP_RX_CLK_MASK, 6);
			dwc3_msm_write_reg_field(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0),
					GEN1_U3_EXIT_RSP_RX_CLK_MASK, 5);
			dev_dbg(mdwc->dev, "LU3:%08x\n",
				dwc3_msm_read_reg(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0)));
		}

		usb_gadget_vbus_connect(&dwc->gadget);
#ifdef CONFIG_SMP
		mdwc->pm_qos_req_dma.type = PM_QOS_REQ_AFFINE_IRQ;
		mdwc->pm_qos_req_dma.irq = dwc->irq;
#endif
		pm_qos_add_request(&mdwc->pm_qos_req_dma,
				PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		/* start in perf mode for better performance initially */
		msm_dwc3_perf_vote_update(mdwc, true);
		schedule_delayed_work(&mdwc->perf_vote_work,
				msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
	} else {
		dev_dbg(mdwc->dev, "%s: turn off gadget %s\n",
					__func__, dwc->gadget.name);
		cancel_delayed_work_sync(&mdwc->perf_vote_work);
		msm_dwc3_perf_vote_update(mdwc, false);
		pm_qos_remove_request(&mdwc->pm_qos_req_dma);

		mdwc->in_device_mode = false;
		usb_gadget_vbus_disconnect(&dwc->gadget);
		usb_phy_notify_disconnect(mdwc->hs_phy, USB_SPEED_HIGH);
		usb_phy_notify_disconnect(mdwc->ss_phy, USB_SPEED_SUPER);
		dwc3_override_vbus_status(mdwc, false);
		dwc3_usb3_phy_suspend(dwc, false);
	}

	pm_runtime_put_sync(mdwc->dev);
	dbg_event(0xFF, "StopGdgt psync",
		atomic_read(&mdwc->dev->power.usage_count));

	return 0;
}

/* speed: 0 - USB_SPEED_HIGH, 1 - USB_SPEED_SUPER */
static int dwc3_restart_usb_host_mode(struct notifier_block *nb,
				unsigned long event, void *ptr)
{
	struct dwc3 *dwc;
	struct extcon_dev *edev = ptr;
	struct extcon_nb *enb = container_of(nb, struct extcon_nb,
						host_restart_nb);
	struct dwc3_msm *mdwc = enb->mdwc;
	int ret = -EINVAL, usb_speed;

	if (!edev || !mdwc)
		return NOTIFY_DONE;

	dwc = platform_get_drvdata(mdwc->dwc3);

	usb_speed = (event == 0 ? USB_SPEED_HIGH : USB_SPEED_SUPER);
	if (dwc->maximum_speed == usb_speed)
		return 0;

	dbg_event(0xFF, "fw_restarthost", 0);
	flush_delayed_work(&mdwc->sm_work);

	if (!mdwc->in_host_mode)
		goto err;

	dbg_event(0xFF, "stop_host_mode", dwc->maximum_speed);
	ret = dwc3_otg_start_host(mdwc, 0);
	if (ret)
		goto err;

	dbg_event(0xFF, "USB_lpm_state", atomic_read(&dwc->in_lpm));
	/*
	 * stop host mode functionality performs autosuspend with mdwc
	 * device, and it may take sometime to call PM runtime suspend.
	 * Hence call pm_runtime_suspend() API to invoke PM runtime
	 * suspend immediately to put USB controller and PHYs into suspend.
	 */
	ret = pm_runtime_suspend(mdwc->dev);
	/*
	 * If mdwc device is already suspended, pm_runtime_suspend() API
	 * returns 1, which is not error. Overwrite with zero if it is.
	 */
	if (ret > 0)
		ret = 0;
	dbg_event(0xFF, "pm_runtime_sus", ret);

	dwc->maximum_speed = usb_speed;
	mdwc->otg_state = OTG_STATE_B_IDLE;
	schedule_delayed_work(&mdwc->sm_work, 0);
	dbg_event(0xFF, "complete_host_change", dwc->maximum_speed);
err:
	return ret;
}

static int get_psy_type(struct dwc3_msm *mdwc)
{
	union power_supply_propval pval = {0};

	if (mdwc->charging_disabled)
		return -EINVAL;

	if (!mdwc->usb_psy) {
		mdwc->usb_psy = power_supply_get_by_name("usb");
		if (!mdwc->usb_psy) {
			dev_err(mdwc->dev, "Could not get usb psy\n");
			return -ENODEV;
		}
	}

	power_supply_get_property(mdwc->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE,
			&pval);

	return pval.intval;
}

static int dwc3_msm_gadget_vbus_draw(struct dwc3_msm *mdwc, unsigned int mA)
{
	union power_supply_propval pval = {0};
	int ret, psy_type;

	psy_type = get_psy_type(mdwc);
	if (psy_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		if (!mA)
			pval.intval = -ETIMEDOUT;
		else
			pval.intval = 1000 * mA;
		goto set_prop;
	}

	if (mdwc->max_power == mA || psy_type != POWER_SUPPLY_TYPE_USB)
		return 0;

	dev_info(mdwc->dev, "Avail curr from USB = %u\n", mA);
	/* Set max current limit in uA */
	pval.intval = 1000 * mA;

set_prop:
	ret = power_supply_set_property(mdwc->usb_psy,
				POWER_SUPPLY_PROP_SDP_CURRENT_MAX, &pval);
	if (ret) {
		dev_dbg(mdwc->dev, "power supply error when setting property\n");
		return ret;
	}

	mdwc->max_power = mA;
	return 0;
}


/**
 * dwc3_otg_sm_work - workqueue function.
 *
 * @w: Pointer to the dwc3 otg workqueue
 *
 * NOTE: After any change in otg_state, we must reschdule the state machine.
 */
static void dwc3_otg_sm_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm, sm_work.work);
	struct dwc3 *dwc = NULL;
	bool work = 0;
	int ret = 0;
	unsigned long delay = 0;
	const char *state;

	if (mdwc->dwc3)
		dwc = platform_get_drvdata(mdwc->dwc3);

	if (!dwc) {
		dev_err(mdwc->dev, "dwc is NULL.\n");
		return;
	}

	state = usb_otg_state_string(mdwc->otg_state);
	dev_dbg(mdwc->dev, "%s state\n", state);
	dbg_event(0xFF, state, 0);

	/* Check OTG state */
	switch (mdwc->otg_state) {
	case OTG_STATE_UNDEFINED:
		/* put controller and phy in suspend if no cable connected */
		if (test_bit(ID, &mdwc->inputs) &&
				!test_bit(B_SESS_VLD, &mdwc->inputs)) {
			dbg_event(0xFF, "undef_id_!bsv", 0);
			pm_runtime_set_active(mdwc->dev);
			pm_runtime_enable(mdwc->dev);
			pm_runtime_get_noresume(mdwc->dev);
			dwc3_msm_resume(mdwc);
			pm_runtime_put_sync(mdwc->dev);
			dbg_event(0xFF, "Undef NoUSB",
				atomic_read(&mdwc->dev->power.usage_count));
			mdwc->otg_state = OTG_STATE_B_IDLE;
			break;
		}

		dbg_event(0xFF, "Exit UNDEF", 0);
		mdwc->otg_state = OTG_STATE_B_IDLE;
		pm_runtime_set_suspended(mdwc->dev);
		pm_runtime_enable(mdwc->dev);
		/* fall-through */
	case OTG_STATE_B_IDLE:
		if (!test_bit(ID, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "!id\n");
			mdwc->otg_state = OTG_STATE_A_IDLE;
			work = 1;
		} else if (test_bit(B_SESS_VLD, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "b_sess_vld\n");
			if (get_psy_type(mdwc) == POWER_SUPPLY_TYPE_USB_FLOAT)
				queue_delayed_work(mdwc->dwc3_wq,
						&mdwc->sdp_check,
				msecs_to_jiffies(SDP_CONNETION_CHECK_TIME));

			/*
			 * Increment pm usage count upon cable connect. Count
			 * is decremented in OTG_STATE_B_PERIPHERAL state on
			 * cable disconnect or in bus suspend.
			 */
			pm_runtime_get_sync(mdwc->dev);
			dbg_event(0xFF, "BIDLE gsync",
				atomic_read(&mdwc->dev->power.usage_count));
			dwc3_otg_start_peripheral(mdwc, 1);
			mdwc->otg_state = OTG_STATE_B_PERIPHERAL;
			work = 1;
		} else {
			dwc3_msm_gadget_vbus_draw(mdwc, 0);
			dev_dbg(mdwc->dev, "Cable disconnected\n");
		}
		break;

	case OTG_STATE_B_PERIPHERAL:
		if (!test_bit(B_SESS_VLD, &mdwc->inputs) ||
				!test_bit(ID, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "!id || !bsv\n");
			mdwc->otg_state = OTG_STATE_B_IDLE;
			cancel_delayed_work_sync(&mdwc->sdp_check);
			dwc3_otg_start_peripheral(mdwc, 0);
			/*
			 * Decrement pm usage count upon cable disconnect
			 * which was incremented upon cable connect in
			 * OTG_STATE_B_IDLE state
			 */
			pm_runtime_put_sync_suspend(mdwc->dev);
			dbg_event(0xFF, "!BSV psync",
				atomic_read(&mdwc->dev->power.usage_count));
			work = 1;
		} else if (test_bit(B_SUSPEND, &mdwc->inputs) &&
			test_bit(B_SESS_VLD, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "BPER bsv && susp\n");
			mdwc->otg_state = OTG_STATE_B_SUSPEND;
			/*
			 * Decrement pm usage count upon bus suspend.
			 * Count was incremented either upon cable
			 * connect in OTG_STATE_B_IDLE or host
			 * initiated resume after bus suspend in
			 * OTG_STATE_B_SUSPEND state
			 */
			pm_runtime_mark_last_busy(mdwc->dev);
			pm_runtime_put_autosuspend(mdwc->dev);
			dbg_event(0xFF, "SUSP put",
				atomic_read(&mdwc->dev->power.usage_count));
		}
		break;

	case OTG_STATE_B_SUSPEND:
		if (!test_bit(B_SESS_VLD, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "BSUSP: !bsv\n");
			mdwc->otg_state = OTG_STATE_B_IDLE;
			cancel_delayed_work_sync(&mdwc->sdp_check);
			dwc3_otg_start_peripheral(mdwc, 0);
		} else if (!test_bit(B_SUSPEND, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "BSUSP !susp\n");
			mdwc->otg_state = OTG_STATE_B_PERIPHERAL;
			/*
			 * Increment pm usage count upon host
			 * initiated resume. Count was decremented
			 * upon bus suspend in
			 * OTG_STATE_B_PERIPHERAL state.
			 */
			pm_runtime_get_sync(mdwc->dev);
			dbg_event(0xFF, "!SUSP gsync",
				atomic_read(&mdwc->dev->power.usage_count));
		}
		break;

	case OTG_STATE_A_IDLE:
		/* Switch to A-Device*/
		if (test_bit(ID, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "id\n");
			mdwc->otg_state = OTG_STATE_B_IDLE;
			mdwc->vbus_retry_count = 0;
			work = 1;
		} else {
			mdwc->otg_state = OTG_STATE_A_HOST;

			ret = dwc3_otg_start_host(mdwc, 1);
			if ((ret == -EPROBE_DEFER) &&
						mdwc->vbus_retry_count < 3) {
				/*
				 * Get regulator failed as regulator driver is
				 * not up yet. Will try to start host after 1sec
				 */
				mdwc->otg_state = OTG_STATE_A_IDLE;
				dev_dbg(mdwc->dev, "Unable to get vbus regulator. Retrying...\n");
				delay = VBUS_REG_CHECK_DELAY;
				work = 1;
				mdwc->vbus_retry_count++;
			} else if (ret) {
				dev_err(mdwc->dev, "unable to start host\n");
				mdwc->otg_state = OTG_STATE_A_IDLE;
				goto ret;
			}
		}
		break;

	case OTG_STATE_A_HOST:
		if (test_bit(ID, &mdwc->inputs) || mdwc->hc_died) {
			dev_dbg(mdwc->dev, "id || hc_died\n");
			dwc3_otg_start_host(mdwc, 0);
			mdwc->otg_state = OTG_STATE_B_IDLE;
			mdwc->vbus_retry_count = 0;
			mdwc->hc_died = false;
			work = 1;
		} else {
			dev_dbg(mdwc->dev, "still in a_host state. Resuming root hub.\n");
			dbg_event(0xFF, "XHCIResume", 0);
			if (dwc)
				pm_runtime_resume(&dwc->xhci->dev);
		}
		break;

	default:
		dev_err(mdwc->dev, "%s: invalid otg-state\n", __func__);

	}

	if (work)
		schedule_delayed_work(&mdwc->sm_work, delay);

ret:
	return;
}

#ifdef CONFIG_PM_SLEEP
static int dwc3_msm_pm_suspend(struct device *dev)
{
	int ret = 0;
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "dwc3-msm PM suspend\n");
	dbg_event(0xFF, "PM Sus", 0);

	flush_workqueue(mdwc->dwc3_wq);
	if (!atomic_read(&dwc->in_lpm)) {
		dev_err(mdwc->dev, "Abort PM suspend!! (USB is outside LPM)\n");
		return -EBUSY;
	}

	ret = dwc3_msm_suspend(mdwc);
	if (!ret)
		atomic_set(&mdwc->pm_suspended, 1);

	return ret;
}

static int dwc3_msm_pm_resume(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "dwc3-msm PM resume\n");
	dbg_event(0xFF, "PM Res", 0);

	/* flush to avoid race in read/write of pm_suspended */
	flush_workqueue(mdwc->dwc3_wq);
	atomic_set(&mdwc->pm_suspended, 0);

	/* kick in otg state machine */
	queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

	return 0;
}
#endif

#ifdef CONFIG_PM
static int dwc3_msm_runtime_idle(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "DWC3-msm runtime idle\n");
	dbg_event(0xFF, "RT Idle", 0);

	return 0;
}

static int dwc3_msm_runtime_suspend(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "DWC3-msm runtime suspend\n");
	dbg_event(0xFF, "RT Sus", 0);

	return dwc3_msm_suspend(mdwc);
}

static int dwc3_msm_runtime_resume(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "DWC3-msm runtime resume\n");
	dbg_event(0xFF, "RT Res", 0);

	return dwc3_msm_resume(mdwc);
}
#endif

static const struct dev_pm_ops dwc3_msm_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_msm_pm_suspend, dwc3_msm_pm_resume)
	SET_RUNTIME_PM_OPS(dwc3_msm_runtime_suspend, dwc3_msm_runtime_resume,
				dwc3_msm_runtime_idle)
};

static const struct of_device_id of_dwc3_matach[] = {
	{
		.compatible = "qcom,dwc-usb3-msm",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_dwc3_matach);

static struct platform_driver dwc3_msm_driver = {
	.probe		= dwc3_msm_probe,
	.remove		= dwc3_msm_remove,
	.driver		= {
		.name	= "msm-dwc3",
		.pm	= &dwc3_msm_dev_pm_ops,
		.of_match_table	= of_dwc3_matach,
	},
};

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 MSM Glue Layer");

static int dwc3_msm_init(void)
{
	return platform_driver_register(&dwc3_msm_driver);
}
module_init(dwc3_msm_init);

static void __exit dwc3_msm_exit(void)
{
	platform_driver_unregister(&dwc3_msm_driver);
}
module_exit(dwc3_msm_exit);

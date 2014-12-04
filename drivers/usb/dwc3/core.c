/**
 * core.c - DesignWare USB3 DRD Controller Core file
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/of.h>
#include <linux/usb/otg.h>
#include <linux/usb/ulpi.h>

#include "platform_data.h"
#include "core.h"
#include "gadget.h"
#include "io.h"

#include "debug.h"

/* -------------------------------------------------------------------------- */

void dwc3_set_mode(struct dwc3 *dwc, u32 mode)
{
	u32 reg;

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~(DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG));
	reg |= DWC3_GCTL_PRTCAPDIR(mode);
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);
}

#define GUSB2PHYCFG0                            0xc200
#define GUSB2PHYCFG_SUS_PHY                     0x40
#define GUSB2PHYCFG_PHYSOFTRST (1 << 31)
#define GUSB2PHYCFG_ULPI_AUTO_RESUME (1 << 15)
#define GUSB3PIPECTL0                           0xc2c0
#define GUSB3PIPECTL_SUS_EN                     0x20000

#define EXTEND_ULPI_REGISTER_ACCESS_MASK        0xC0
#define GUSB2PHYACC0    0xc280
#define GUSB2PHYACC0_DISULPIDRVR  (1 << 26)
#define GUSB2PHYACC0_NEWREGREQ  (1 << 25)
#define GUSB2PHYACC0_VSTSDONE  (1 << 24)
#define GUSB2PHYACC0_VSTSBSY  (1 << 23)
#define GUSB2PHYACC0_REGWR  (1 << 22)
#define GUSB2PHYACC0_REGADDR(v)  ((v & 0x3F) << 16)
#define GUSB2PHYACC0_EXTREGADDR(v)  ((v & 0x3F) << 8)
#define GUSB2PHYACC0_VCTRL(v)  ((v & 0xFF) << 8)
#define GUSB2PHYACC0_REGDATA(v)  (v & 0xFF)
#define GUSB2PHYACC0_REGDATA_MASK  0xFF

static int ulpi_read(struct dwc3 *dwc, u32 reg)
{
	u32 val32 = 0, count = 200;
	u8 val, tmp;

	reg &= 0xFF;

	while (count) {
		if (dwc3_readl(dwc->regs, GUSB2PHYACC0) & GUSB2PHYACC0_VSTSBSY)
			udelay(5);
		else
			break;

		count--;
	}

	if (!count) {
		dev_err(dwc->dev, "USB2 PHY always busy!!\n");
		return -EBUSY;
	}

	count = 200;
	/* Determine if use extend registers access */
	if (reg & EXTEND_ULPI_REGISTER_ACCESS_MASK) {
		dev_dbg(dwc->dev, "Access extend registers 0x%x\n", reg);
		val32 = GUSB2PHYACC0_NEWREGREQ
			| GUSB2PHYACC0_REGADDR(ULPI_ACCESS_EXTENDED)
			| GUSB2PHYACC0_VCTRL(reg);
	} else {
		dev_dbg(dwc->dev, "Access normal registers 0x%x\n", reg);
		val32 = GUSB2PHYACC0_NEWREGREQ | GUSB2PHYACC0_REGADDR(reg)
			| GUSB2PHYACC0_VCTRL(0x00);
	}
	dwc3_writel(dwc->regs, GUSB2PHYACC0, val32);

	while (count) {
		if (dwc3_readl(dwc->regs, GUSB2PHYACC0) & GUSB2PHYACC0_VSTSDONE) {
			val = dwc3_readl(dwc->regs, GUSB2PHYACC0) &
				GUSB2PHYACC0_REGDATA_MASK;
			dev_dbg(dwc->dev, "%s - reg 0x%x data 0x%x\n",
					__func__, reg, val);
			goto cleanup;
		}

		count--;
	}
	dev_err(dwc->dev, "%s read PHY data failed.\n", __func__);

	return -ETIMEDOUT;

cleanup:
	/* Clear GUSB2PHYACC0[16:21] before return.
	 * Otherwise, it will cause PHY can't in workable
	 * state. This is one dwc3 controller silicon bug. */
	tmp = dwc3_readl(dwc->regs, GUSB2PHYACC0);
	dwc3_writel(dwc->regs, GUSB2PHYACC0, tmp &
			~GUSB2PHYACC0_REGADDR(0x3F));
	return val;

}

static int ulpi_write(struct dwc3 *dwc, u32 val, u32 reg)
{
	u32 val32 = 0, count = 200;
	u8 tmp;

	while (count) {
		if (dwc3_readl(dwc->regs, GUSB2PHYACC0) & GUSB2PHYACC0_VSTSBSY)
			udelay(1);
		else
			break;
		count--;
	}

	if (!count) {
		dev_err(dwc->dev, "USB2 PHY always busy!!\n");
		return -EBUSY;
	}

	count = 10000;
	if (reg & EXTEND_ULPI_REGISTER_ACCESS_MASK) {
		dev_dbg(dwc->dev, "Access extend registers 0x%x\n", reg);
		val32 = GUSB2PHYACC0_NEWREGREQ
			| GUSB2PHYACC0_REGADDR(ULPI_ACCESS_EXTENDED)
			| GUSB2PHYACC0_VCTRL(reg)
			| GUSB2PHYACC0_REGWR | GUSB2PHYACC0_REGDATA(val);
	} else {
		dev_dbg(dwc->dev, "Access normal registers 0x%x\n", reg);
		val32 = GUSB2PHYACC0_NEWREGREQ
			| GUSB2PHYACC0_REGADDR(reg)
			| GUSB2PHYACC0_REGWR
			| GUSB2PHYACC0_REGDATA(val);
	}
	dwc3_writel(dwc->regs, GUSB2PHYACC0, val32);

	while (count) {
		if (dwc3_readl(dwc->regs, GUSB2PHYACC0) &
			GUSB2PHYACC0_VSTSDONE) {
			dev_dbg(dwc->dev, "%s - reg 0x%x data 0x%x write done\n",
				__func__, reg, val);
			goto cleanup;
		}

		udelay(1);
		count--;
	}

	dev_err(dwc->dev, "%s write PHY data failed.\n", __func__);

	return -ETIMEDOUT;

cleanup:
	/* Clear GUSB2PHYACC0[16:21] before return.
	 * Otherwise, it will cause PHY can't in workable
	 * state. This is one dwc3 controller silicon bug. */
	tmp = dwc3_readl(dwc->regs, GUSB2PHYACC0);
	dwc3_writel(dwc->regs, GUSB2PHYACC0, tmp &
		~GUSB2PHYACC0_REGADDR(0x3F));
	return 0;
}

/* HACK: set optimal drive strength in phy, two improvements needed:
 * 1. this code should be in a separate phy driver such that phy specific
 *    setting is not tied to controllers.
 * 2. platform specific tuning data should come from platform data, ACPI, etc.
 *    Currently use max drive strength with default impedence.
 */
#define TUSB1211_VENDOR_SPECIFIC1_SET      0x81
#define PWCTRL_SW_CONTROL (1 << 0)
#define TUSB1211_POWER_CONTROL_SET 0x3E
#define TUSB1211_EYE_DIAGRAM_TUNING 0x4f
#define TUSB1211_OTG_CTRL		0xa
#define TUSB1211_OTG_CTRL_DPPULLDOWN	(1 << 1)
#define TUSB1211_OTG_CTRL_DMPULLDOWN	(1 << 2)

static void set_phy_eye_optim(struct dwc3 *dwc)
{
	if (ulpi_write(dwc, PWCTRL_SW_CONTROL, TUSB1211_POWER_CONTROL_SET))
		return;
	/* Modify VS1 for better quality in eye diagram */
	if (ulpi_write(dwc, 0x4f, TUSB1211_VENDOR_SPECIFIC1_SET))
		dev_err(dwc->dev, "Tuning ULPI phy eye diagram failed.\n");
}

/*
 * This is a tricky situation that can only be cleanly solved when ULPI bus
 * is available for usb phy driver:
 * When cable is removed, dwc3 will enter in autosuspend mode (if pm runtime is
 * enabled). If USB cable is reconnected before suspend is actually called,
 * the charger detection module will be unable to detect CDP charging mode
 * because D+/D- will still be in connected state. In order to allow CDP
 * connection again, we need to pull down D+/D- to notify USB host we are
 * disconnected.
 */
void dwc3_set_phy_dpm_pulldown(struct dwc3 *dwc, int pull_down)
{
	u32 reg;

	reg = ulpi_read(dwc, TUSB1211_OTG_CTRL);
	if (pull_down)
		reg |= TUSB1211_OTG_CTRL_DPPULLDOWN | TUSB1211_OTG_CTRL_DMPULLDOWN;
	else
		reg &= ~(TUSB1211_OTG_CTRL_DPPULLDOWN | TUSB1211_OTG_CTRL_DMPULLDOWN);
	ulpi_write(dwc, reg, TUSB1211_OTG_CTRL);
}

static void dwc3_check_ulpi(struct dwc3 *dwc)
{
	if (ulpi_read(dwc, ULPI_VENDOR_ID_LOW) < 0)
		dev_err(dwc->dev, "ULPI not working after DCTL soft reset\n");
	else
		dev_info(dwc->dev, "ULPI is working well");
}

/**
 * dwc3_core_soft_reset - Issues core soft reset and PHY reset
 * @dwc: pointer to our context structure
 */
static void dwc3_core_soft_reset(struct dwc3 *dwc)
{
	u32		reg;

	/* Before Resetting PHY, put Core in Reset */
	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg |= DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	/* Assert USB3 PHY reset */
	reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
	reg |= DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);

	/* Assert USB2 PHY reset */
	reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
	reg |= DWC3_GUSB2PHYCFG_PHYSOFTRST;
	dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);

	usb_phy_init(dwc->usb2_phy);
	usb_phy_init(dwc->usb3_phy);
	mdelay(100);

	/* Clear USB3 PHY reset */
	reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
	reg &= ~DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);

	/* Clear USB2 PHY reset */
	reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
	reg &= ~DWC3_GUSB2PHYCFG_PHYSOFTRST;
	dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);

	mdelay(100);

	/* After PHYs are stable we can take Core out of reset state */
	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	dwc3_check_ulpi(dwc);
	set_phy_eye_optim(dwc);
 }

/**
 * dwc3_free_one_event_buffer - Frees one event buffer
 * @dwc: Pointer to our controller context structure
 * @evt: Pointer to event buffer to be freed
 */
static void dwc3_free_one_event_buffer(struct dwc3 *dwc,
		struct dwc3_event_buffer *evt)
{
	dma_free_coherent(dwc->dev, evt->length, evt->buf, evt->dma);
}

/**
 * dwc3_alloc_one_event_buffer - Allocates one event buffer structure
 * @dwc: Pointer to our controller context structure
 * @length: size of the event buffer
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
static struct dwc3_event_buffer *dwc3_alloc_one_event_buffer(struct dwc3 *dwc,
		unsigned length)
{
	struct dwc3_event_buffer	*evt;

	evt = devm_kzalloc(dwc->dev, sizeof(*evt), GFP_KERNEL);
	if (!evt)
		return ERR_PTR(-ENOMEM);

	evt->dwc	= dwc;
	evt->length	= length;
	evt->buf	= dma_alloc_coherent(dwc->dev, length,
			&evt->dma, GFP_KERNEL);
	if (!evt->buf)
		return ERR_PTR(-ENOMEM);

	return evt;
}

/**
 * dwc3_free_event_buffers - frees all allocated event buffers
 * @dwc: Pointer to our controller context structure
 */
static void dwc3_free_event_buffers(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int i;

	for (i = 0; i < dwc->num_event_buffers; i++) {
		evt = dwc->ev_buffs[i];
		if (evt)
			dwc3_free_one_event_buffer(dwc, evt);
	}
}

/**
 * dwc3_alloc_event_buffers - Allocates @num event buffers of size @length
 * @dwc: pointer to our controller context structure
 * @length: size of event buffer
 *
 * Returns 0 on success otherwise negative errno. In the error case, dwc
 * may contain some buffers allocated but not all which were requested.
 */
static int dwc3_alloc_event_buffers(struct dwc3 *dwc, unsigned length)
{
	int			num;
	int			i;

	num = DWC3_NUM_INT(dwc->hwparams.hwparams1);
	dwc->num_event_buffers = num;

	dwc->ev_buffs = devm_kzalloc(dwc->dev, sizeof(*dwc->ev_buffs) * num,
			GFP_KERNEL);
	if (!dwc->ev_buffs) {
		dev_err(dwc->dev, "can't allocate event buffers array\n");
		return -ENOMEM;
	}

	for (i = 0; i < num; i++) {
		struct dwc3_event_buffer	*evt;

		evt = dwc3_alloc_one_event_buffer(dwc, length);
		if (IS_ERR(evt)) {
			dev_err(dwc->dev, "can't allocate event buffer\n");
			return PTR_ERR(evt);
		}
		dwc->ev_buffs[i] = evt;
	}

	return 0;
}

/**
 * dwc3_event_buffers_setup - setup our allocated event buffers
 * @dwc: pointer to our controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_event_buffers_setup(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int				n;

	for (n = 0; n < dwc->num_event_buffers; n++) {
		evt = dwc->ev_buffs[n];
		dev_dbg(dwc->dev, "Event buf %p dma %08llx length %d\n",
				evt->buf, (unsigned long long) evt->dma,
				evt->length);

		evt->lpos = 0;

		dwc3_writel(dwc->regs, DWC3_GEVNTADRLO(n),
				lower_32_bits(evt->dma));
		dwc3_writel(dwc->regs, DWC3_GEVNTADRHI(n),
				upper_32_bits(evt->dma));
		dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(n),
				DWC3_GEVNTSIZ_SIZE(evt->length));
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(n), 0);
	}

	return 0;
}

static void dwc3_event_buffers_cleanup(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int				n;

	for (n = 0; n < dwc->num_event_buffers; n++) {
		evt = dwc->ev_buffs[n];

		evt->lpos = 0;

		dwc3_writel(dwc->regs, DWC3_GEVNTADRLO(n), 0);
		dwc3_writel(dwc->regs, DWC3_GEVNTADRHI(n), 0);
		dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(n), DWC3_GEVNTSIZ_INTMASK
				| DWC3_GEVNTSIZ_SIZE(0));
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(n), 0);
	}
}

static void dwc3_core_num_eps(struct dwc3 *dwc)
{
	struct dwc3_hwparams	*parms = &dwc->hwparams;

	dwc->num_in_eps = DWC3_NUM_IN_EPS(parms);
	dwc->num_out_eps = DWC3_NUM_EPS(parms) - dwc->num_in_eps;

	dev_vdbg(dwc->dev, "found %d IN and %d OUT endpoints\n",
			dwc->num_in_eps, dwc->num_out_eps);
}

static void dwc3_cache_hwparams(struct dwc3 *dwc)
{
	struct dwc3_hwparams	*parms = &dwc->hwparams;

	parms->hwparams0 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS0);
	parms->hwparams1 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS1);
	parms->hwparams2 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS2);
	parms->hwparams3 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS3);
	parms->hwparams4 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS4);
	parms->hwparams5 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS5);
	parms->hwparams6 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS6);
	parms->hwparams7 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS7);
	parms->hwparams8 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS8);
}

/**
 * dwc3_core_init - Low-level initialization of DWC3 Core
 * @dwc: Pointer to our controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_core_init(struct dwc3 *dwc)
{
	unsigned long		timeout;
	u32			reg;
	int			ret;

	reg = dwc3_readl(dwc->regs, DWC3_GSNPSID);
	/* This should read as U3 followed by revision number */
	if ((reg & DWC3_GSNPSID_MASK) != 0x55330000) {
		dev_err(dwc->dev, "this is not a DesignWare USB3 DRD Core\n");
		ret = -ENODEV;
		goto err0;
	}
	dwc->revision = reg;

	/* issue device SoftReset too */
	timeout = jiffies + msecs_to_jiffies(500);
	dwc3_writel(dwc->regs, DWC3_DCTL, DWC3_DCTL_CSFTRST);
	do {
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		if (!(reg & DWC3_DCTL_CSFTRST))
			break;

		if (time_after(jiffies, timeout)) {
			dev_err(dwc->dev, "Reset Timed Out\n");
			ret = -ETIMEDOUT;
			goto err0;
		}

		cpu_relax();
	} while (true);

	dwc3_core_soft_reset(dwc);

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~DWC3_GCTL_SCALEDOWN_MASK;
	reg &= ~DWC3_GCTL_DISSCRAMBLE;

	switch (DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1)) {
	case DWC3_GHWPARAMS1_EN_PWROPT_CLK:
		reg &= ~DWC3_GCTL_DSBLCLKGTNG;
		break;
	default:
		dev_dbg(dwc->dev, "No power optimization available\n");
	}

	/*
	 * WORKAROUND: DWC3 revisions <1.90a have a bug
	 * where the device can fail to connect at SuperSpeed
	 * and falls back to high-speed mode which causes
	 * the device to enter a Connect/Disconnect loop
	 */
	if (dwc->revision < DWC3_REVISION_190A)
		reg |= DWC3_GCTL_U2RSTECN;

	dwc3_core_num_eps(dwc);

	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	return 0;

err0:
	return ret;
}

static void dwc3_core_exit(struct dwc3 *dwc)
{
	usb_phy_shutdown(dwc->usb2_phy);
	usb_phy_shutdown(dwc->usb3_phy);
}

static void dwc3_suspend_phy(struct dwc3 *dwc, bool suspend)
{
	u32 data = 0;

	data = dwc3_readl(dwc->regs, GUSB2PHYCFG0);
	if (suspend)
		data |= GUSB2PHYCFG_SUS_PHY;
	else
		data &= ~GUSB2PHYCFG_SUS_PHY;

	dwc3_writel(dwc->regs, GUSB2PHYCFG0, data);

	data = dwc3_readl(dwc->regs, GUSB3PIPECTL0);
	if (suspend)
		data |= GUSB3PIPECTL_SUS_EN;
	else
		data &= ~GUSB3PIPECTL_SUS_EN;

	dwc3_writel(dwc->regs, GUSB3PIPECTL0, data);
}

/*
 * dwc3_disable_multi_packet - disable reception multi-packet
 * thresholding in order to support burst size 0 per SYNOPSIS
 * requirement.
 */
static void dwc3_disable_multi_packet(struct dwc3 *dwc)
{
	u32			reg;

	reg = dwc3_readl(dwc->regs, DWC3_GRXTHRCFG);
	if (reg) {
		reg &= ~DWC3_GRXTHRCFG_USBRXPKTCNTSEL;
		reg &= ~DWC3_GRXTHRCFG_USBRXPKTCNT_MASK;
		reg &= ~DWC3_GRXTHRCFG_USBMAXRXBURSTSIZE_MASK;

		dwc3_writel(dwc->regs, DWC3_GRXTHRCFG, reg);
	}
}

static int dwc3_handle_otg_notification(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct dwc3* dwc = container_of(nb, struct dwc3, nb);
	unsigned long flags;
	int state = NOTIFY_DONE;
	static int last_value = -1;

	if (last_value == event)
		goto out;

	spin_lock_irqsave(&dwc->lock, flags);
	switch (event) {
	case USB_EVENT_VBUS:
		dev_info(dwc->dev, "DWC3 OTG Notify USB_EVENT_VBUS\n");
		last_value = event;
		if (dwc->dpm_pulled_down) {
			dwc3_set_phy_dpm_pulldown(dwc, 0);
			dwc->dpm_pulled_down = 0;
		}
		pm_runtime_get(dwc->dev);
		state = NOTIFY_OK;
		break;
	case USB_EVENT_NONE:
		dev_info(dwc->dev, "DWC3 OTG Notify USB_EVENT_NONE\n");
		last_value = event;
		state = NOTIFY_OK;
		break;
	default:
		dev_dbg(dwc->dev, "DWC3 OTG Notify unknow notify message\n");
	}
	spin_unlock_irqrestore(&dwc->lock, flags);

out:
	return state;
}

#define DWC3_ALIGN_MASK		(16 - 1)

static int dwc3_probe(struct platform_device *pdev)
{
	struct device		*dev = &pdev->dev;
	struct dwc3_platform_data *pdata = dev_get_platdata(dev);
	struct device_node	*node = dev->of_node;
	struct resource		*res;
	struct dwc3		*dwc;

	int			ret = -ENOMEM;

	void __iomem		*regs;
	void			*mem;

	mem = devm_kzalloc(dev, sizeof(*dwc) + DWC3_ALIGN_MASK, GFP_KERNEL);
	if (!mem) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}
	dwc = PTR_ALIGN(mem, DWC3_ALIGN_MASK + 1);
	dwc->mem = mem;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "missing IRQ\n");
		return -ENODEV;
	}
	dwc->xhci_resources[1].start = res->start;
	dwc->xhci_resources[1].end = res->end;
	dwc->xhci_resources[1].flags = res->flags;
	dwc->xhci_resources[1].name = res->name;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing memory resource\n");
		return -ENODEV;
	}

	if (node) {
		dwc->maximum_speed = of_usb_get_maximum_speed(node);

		dwc->usb2_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 0);
		dwc->usb3_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 1);

		dwc->needs_fifo_resize = of_property_read_bool(node, "tx-fifo-resize");
		dwc->dr_mode = of_usb_get_dr_mode(node);
	} else if (pdata) {
		dwc->maximum_speed = pdata->maximum_speed;

		dwc->usb2_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
		dwc->usb3_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB3);

		dwc->needs_fifo_resize = pdata->tx_fifo_resize;
		dwc->dr_mode = pdata->dr_mode;
		dwc->runtime_suspend = pdata->runtime_suspend;
	} else {
		dwc->usb2_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
		dwc->usb3_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB3);
	}

	/* default to superspeed if no maximum_speed passed */
	if (dwc->maximum_speed == USB_SPEED_UNKNOWN)
		dwc->maximum_speed = USB_SPEED_SUPER;

	if (IS_ERR(dwc->usb2_phy)) {
		ret = PTR_ERR(dwc->usb2_phy);

		/*
		 * if -ENXIO is returned, it means PHY layer wasn't
		 * enabled, so it makes no sense to return -EPROBE_DEFER
		 * in that case, since no PHY driver will ever probe.
		 */
		if (ret == -ENXIO)
			return ret;

		dev_err(dev, "no usb2 phy configured\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(dwc->usb3_phy)) {
		ret = PTR_ERR(dwc->usb3_phy);

		/*
		 * if -ENXIO is returned, it means PHY layer wasn't
		 * enabled, so it makes no sense to return -EPROBE_DEFER
		 * in that case, since no PHY driver will ever probe.
		 */
		if (ret == -ENXIO)
			return ret;

		dev_err(dev, "no usb3 phy configured\n");
		return -EPROBE_DEFER;
	}

	dwc->xhci_resources[0].start = res->start;
	dwc->xhci_resources[0].end = dwc->xhci_resources[0].start +
					DWC3_XHCI_REGS_END;
	dwc->xhci_resources[0].flags = res->flags;
	dwc->xhci_resources[0].name = res->name;

	res->start += DWC3_GLOBALS_REGS_START;

	/*
	 * Request memory region but exclude xHCI regs,
	 * since it will be requested by the xhci-plat driver.
	 */
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	spin_lock_init(&dwc->lock);
	platform_set_drvdata(pdev, dwc);

	dwc->regs	= regs;
	dwc->regs_size	= resource_size(res);
	dwc->dev	= dev;

	dev->dma_mask	= dev->parent->dma_mask;
	dev->dma_parms	= dev->parent->dma_parms;
	dma_set_coherent_mask(dev, dev->parent->coherent_dma_mask);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	pm_runtime_forbid(dev);

	dwc3_cache_hwparams(dwc);

	ret = dwc3_alloc_event_buffers(dwc, DWC3_EVENT_BUFFERS_SIZE);
	if (ret) {
		dev_err(dwc->dev, "failed to allocate event buffers\n");
		ret = -ENOMEM;
		goto err0;
	}

	ret = dwc3_core_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize core\n");
		goto err0;
	}

	usb_phy_set_suspend(dwc->usb2_phy, 0);
	usb_phy_set_suspend(dwc->usb3_phy, 0);

	ret = dwc3_event_buffers_setup(dwc);
	if (ret) {
		dev_err(dwc->dev, "failed to setup event buffers\n");
		goto err1;
	}

	if (IS_ENABLED(CONFIG_USB_DWC3_HOST))
		dwc->dr_mode = USB_DR_MODE_HOST;
	else if (IS_ENABLED(CONFIG_USB_DWC3_GADGET))
		dwc->dr_mode = USB_DR_MODE_PERIPHERAL;

	if (dwc->dr_mode == USB_DR_MODE_UNKNOWN)
		dwc->dr_mode = USB_DR_MODE_OTG;

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_DEVICE);
		ret = dwc3_gadget_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto err2;
		}
		break;
	case USB_DR_MODE_HOST:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_HOST);
		ret = dwc3_host_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			goto err2;
		}
		break;
	case USB_DR_MODE_OTG:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_OTG);
		ret = dwc3_host_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			goto err2;
		}

		ret = dwc3_gadget_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto err2;
		}
		break;
	default:
		dev_err(dev, "Unsupported mode of operation %d\n", dwc->dr_mode);
		goto err2;
	}

	ret = dwc3_debugfs_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize debugfs\n");
		goto err3;
	}

	atomic_set(&dwc->suspend_depth, 0);

	if (dwc->runtime_suspend) {
		pm_runtime_set_autosuspend_delay(dev, 10000);
		
		/*
		 * Autosuspend seems to be the root cause of few rare issues
		 * on this driver. We're disabling it while investigate why.
		 */
		/* pm_runtime_use_autosuspend(dev); */
		
		pm_runtime_mark_last_busy(dev);
		pm_runtime_put_autosuspend(dev);

		/* Register otg notifier to monitor VBus change events */
		// FIXME: usb3_phy notification
		dwc->nb.notifier_call = dwc3_handle_otg_notification;
		ret = usb_register_notifier(dwc->usb2_phy, &dwc->nb);
		if (ret) {
			dev_err(dev, "failed to register otg notifier\n");
			goto err4;
		}
	}

	pm_runtime_allow(dev);

	return 0;

err4:
	dwc3_debugfs_exit(dwc);

err3:
	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_gadget_exit(dwc);
		break;
	case USB_DR_MODE_HOST:
		dwc3_host_exit(dwc);
		break;
	case USB_DR_MODE_OTG:
		dwc3_host_exit(dwc);
		dwc3_gadget_exit(dwc);
		break;
	default:
		/* do nothing */
		break;
	}

err2:
	dwc3_event_buffers_cleanup(dwc);

err1:
	usb_phy_set_suspend(dwc->usb2_phy, 1);
	usb_phy_set_suspend(dwc->usb3_phy, 1);
	dwc3_core_exit(dwc);

err0:
	dwc3_free_event_buffers(dwc);

	return ret;
}

static int dwc3_remove(struct platform_device *pdev)
{
	struct dwc3	*dwc = platform_get_drvdata(pdev);

	dwc3_debugfs_exit(dwc);

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_gadget_exit(dwc);
		break;
	case USB_DR_MODE_HOST:
		dwc3_host_exit(dwc);
		break;
	case USB_DR_MODE_OTG:
		dwc3_host_exit(dwc);
		dwc3_gadget_exit(dwc);
		break;
	default:
		/* do nothing */
		break;
	}

	dwc3_event_buffers_cleanup(dwc);
	dwc3_free_event_buffers(dwc);

	usb_phy_set_suspend(dwc->usb2_phy, 1);
	usb_phy_set_suspend(dwc->usb3_phy, 1);

	dwc3_core_exit(dwc);

	if (dwc->runtime_suspend) {
		usb_unregister_notifier(dwc->usb2_phy, &dwc->nb);
	} else {
		pm_runtime_put_sync(&pdev->dev);
	}

	pm_runtime_disable(&pdev->dev);

	return 0;
}


#ifdef CONFIG_PM_SLEEP

static int dwc3_suspend_common(struct device *dev)
{
	struct dwc3	*dwc = dev_get_drvdata(dev);
	unsigned long	flags;

	if (atomic_inc_return(&dwc->suspend_depth) > 1) {
		dev_info(dev, "%s: skipping suspend. suspend_depth = %d\n",
			 __func__, atomic_read(&dwc->suspend_depth));
		return 0;
	}

	dev_info(dev, "%s\n", __func__);

	spin_lock_irqsave(&dwc->lock, flags);

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		dwc3_gadget_prepare(dwc);
		dwc3_gadget_suspend(dwc);
		/* FALLTHROUGH */
	case USB_DR_MODE_HOST:
	default:
		/* do nothing */
		break;
	}

	dwc3_event_buffers_cleanup(dwc);

	dwc->gctl = dwc3_readl(dwc->regs, DWC3_GCTL);

	dwc3_suspend_phy(dwc, true);

	dwc->dpm_pulled_down = 0;

	spin_unlock_irqrestore(&dwc->lock, flags);

	usb_phy_shutdown(dwc->usb3_phy);
	usb_phy_shutdown(dwc->usb2_phy);

	return 0;
}

static int dwc3_resume_common(struct device *dev)
{
	struct dwc3	*dwc = dev_get_drvdata(dev);
	unsigned long	flags;

	if (atomic_dec_return(&dwc->suspend_depth) > 0) {
		dev_info(dev, "%s: skipping resume. suspend_depth = %d\n",
			 __func__, atomic_read(&dwc->suspend_depth));
		return 0;
	}

	dev_info(dev, "%s\n", __func__);

	usb_phy_init(dwc->usb3_phy);
	usb_phy_init(dwc->usb2_phy);

	spin_lock_irqsave(&dwc->lock, flags);

	dwc3_disable_multi_packet(dwc);

	dwc3_suspend_phy(dwc, false);

	dwc3_writel(dwc->regs, DWC3_GCTL, dwc->gctl);

	dwc3_event_buffers_setup(dwc);

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		dwc3_gadget_resume(dwc);
		dwc3_gadget_complete(dwc);
		/* FALLTHROUGH */
	case USB_DR_MODE_HOST:
	default:
		/* do nothing */
		break;
	}
	set_phy_eye_optim(dwc);

	spin_unlock_irqrestore(&dwc->lock, flags);

	dwc3_check_ulpi(dwc);

	return 0;
}


#ifdef CONFIG_PM_RUNTIME

static int dwc3_runtime_suspend(struct device *dev)
{
	return dwc3_suspend_common(dev);
}

static int dwc3_runtime_resume(struct device *dev)
{
	return dwc3_resume_common(dev);
}

#else

#define dwc3_runtime_suspend NULL
#define dwc3_runtime_resume NULL

#endif

static int dwc3_suspend(struct device *dev)
{
	return dwc3_suspend_common(dev);
}

static int dwc3_resume(struct device *dev)
{
	return dwc3_resume_common(dev);
}

static const struct dev_pm_ops dwc3_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_suspend, dwc3_resume)
	SET_RUNTIME_PM_OPS(dwc3_runtime_suspend, dwc3_runtime_resume, NULL)
};

#define DWC3_PM_OPS	&(dwc3_dev_pm_ops)
#else
#define DWC3_PM_OPS	NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id of_dwc3_match[] = {
	{
		.compatible = "snps,dwc3"
	},
	{
		.compatible = "synopsys,dwc3"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_dwc3_match);
#endif

static struct platform_driver dwc3_driver = {
	.probe		= dwc3_probe,
	.remove		= dwc3_remove,
	.driver		= {
		.name	= "dwc3",
		.of_match_table	= of_match_ptr(of_dwc3_match),
		.pm	= DWC3_PM_OPS,
	},
};

module_platform_driver(dwc3_driver);

MODULE_ALIAS("platform:dwc3");
MODULE_AUTHOR("Felipe Balbi <balbi@ti.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 DRD Controller Driver");

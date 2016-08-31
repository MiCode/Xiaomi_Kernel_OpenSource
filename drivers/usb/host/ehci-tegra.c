/*
 * EHCI-compliant USB host controller driver for NVIDIA Tegra SoCs
 *
 * Copyright (c) 2010 Google, Inc.
 * Copyright (c) 2009-2013 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/irq.h>
#include <linux/usb/otg.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/tegra-soc.h>

#include <linux/usb/tegra_usb_phy.h>

#include <mach/pm_domains.h>
#include <linux/pm_qos.h>
#include <linux/wakelock.h>

/* HACK! This needs to come from DT */
#include "../../../arch/arm/mach-tegra/iomap.h"

#if 0
#define EHCI_DBG(stuff...)	pr_info("ehci-tegra: " stuff)
#else
#define EHCI_DBG(stuff...)	do {} while (0)
#endif

static const char driver_name[] = "tegra-ehci";

#define TEGRA_USB_DMA_ALIGN 32

#define HOSTPC_REG_OFFSET		0x1b4

#define HOSTPC1_DEVLC_STS 		(1 << 28)
#define HOSTPC1_DEVLC_NYT_ASUS		1
#define TEGRA_STREAM_DISABLE	0x1f8
#define TEGRA_STREAM_DISABLE_OFFSET (1 << 4)

struct tegra_ehci_hcd {
	struct ehci_hcd *ehci;
	struct tegra_usb_phy *phy;
	struct usb_phy *transceiver;
	struct mutex sync_lock;
	struct wake_lock ehci_wake_lock;
	bool port_resuming;
	unsigned int irq;
	bool bus_suspended_fail;
	bool unaligned_dma_buf_supported;
	bool has_hostpc;
#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	bool boost_enable;
	bool boost_requested;
	bool cpu_boost_in_work;
	struct delayed_work boost_cpu_freq_work;
	struct pm_qos_request boost_cpu_freq_req;
#endif
};

struct dma_align_buffer {
	void *kmalloc_ptr;
	void *old_xfer_buffer;
	u8 data[0];
};

static struct usb_phy *get_usb_phy(struct tegra_usb_phy *x)
{
	return (struct usb_phy *)x;
}

static int tegra_ehci_port_speed(struct ehci_hcd *ehci)
{
	u32 hostpc = ehci_readl(ehci, &ehci->regs->hostpc);
	enum usb_device_speed port_speed;

	switch ((hostpc >> (ehci->has_hostpc ? 25 : 26)) & 3) {
	case 0:
		port_speed = USB_SPEED_LOW;
		break;
	case 1:
		port_speed = USB_SPEED_FULL;
		break;
	case 2:
		port_speed = USB_SPEED_HIGH;
		break;
	default:
		port_speed = USB_SPEED_UNKNOWN;
	}
	return port_speed;
}

static void tegra_ehci_notify_event(struct tegra_ehci_hcd *tegra, int event)
{
	tegra->transceiver->last_event = event;
	atomic_notifier_call_chain(&tegra->transceiver->notifier, event,
					 tegra->transceiver->otg->gadget);
}

static void free_align_buffer(struct urb *urb, struct usb_hcd *hcd)
{
	struct dma_align_buffer *temp;
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(hcd->self.controller);
	
	if (tegra->unaligned_dma_buf_supported)
		return;

	if (!(urb->transfer_flags & URB_ALIGNED_TEMP_BUFFER))
		return;

	temp = container_of(urb->transfer_buffer,
		struct dma_align_buffer, data);

	/* In transaction, DMA from Device */
	if (usb_urb_dir_in(urb))
		memcpy(temp->old_xfer_buffer, temp->data,
					urb->transfer_buffer_length);

	urb->transfer_buffer = temp->old_xfer_buffer;
	urb->transfer_flags &= ~URB_ALIGNED_TEMP_BUFFER;
	kfree(temp->kmalloc_ptr);
}

static int alloc_align_buffer(struct urb *urb, gfp_t mem_flags,
	struct usb_hcd *hcd)
{
	struct dma_align_buffer *temp, *kmalloc_ptr;
	size_t kmalloc_size;
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(hcd->self.controller);

	if (tegra->unaligned_dma_buf_supported)
		return 0;

	if (urb->num_sgs || urb->sg ||
		urb->transfer_buffer_length == 0 ||
		!((uintptr_t)urb->transfer_buffer & (TEGRA_USB_DMA_ALIGN - 1)))
		return 0;

	/* Allocate a buffer with enough padding for alignment */
	kmalloc_size = urb->transfer_buffer_length +
		sizeof(struct dma_align_buffer) + TEGRA_USB_DMA_ALIGN - 1;
	kmalloc_ptr = kmalloc(kmalloc_size, mem_flags);

	if (!kmalloc_ptr)
		return -ENOMEM;

	/* Position our struct dma_align_buffer such that data is aligned */
	temp = PTR_ALIGN(kmalloc_ptr + 1, TEGRA_USB_DMA_ALIGN) - 1;
	temp->kmalloc_ptr = kmalloc_ptr;
	temp->old_xfer_buffer = urb->transfer_buffer;
	/* OUT transaction, DMA to Device */
	if (!usb_urb_dir_in(urb))
		memcpy(temp->data, urb->transfer_buffer,
				urb->transfer_buffer_length);

	urb->transfer_buffer = temp->data;
	urb->transfer_flags |= URB_ALIGNED_TEMP_BUFFER;

	return 0;
}

static int tegra_ehci_map_urb_for_dma(struct usb_hcd *hcd,
	struct urb *urb, gfp_t mem_flags)
{
	int ret;

	ret = alloc_align_buffer(urb, mem_flags, hcd);
	if (ret)
		return ret;

	ret = usb_hcd_map_urb_for_dma(hcd, urb, mem_flags);

	if (ret)
		free_align_buffer(urb, hcd);

	return ret;
}

static void tegra_ehci_unmap_urb_for_dma(struct usb_hcd *hcd,
	struct urb *urb)
{
	usb_hcd_unmap_urb_for_dma(hcd, urb);
	free_align_buffer(urb, hcd);
}

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
static void tegra_ehci_boost_cpu_frequency_work(struct work_struct *work)
{
	struct tegra_ehci_hcd *tegra = container_of(work,
		struct tegra_ehci_hcd, boost_cpu_freq_work.work);
	if (tegra->cpu_boost_in_work) {
		tegra->boost_requested = true;
		if (tegra->boost_enable && (tegra_ehci_port_speed(tegra->ehci)
					== USB_SPEED_HIGH))
			pm_qos_update_request(&tegra->boost_cpu_freq_req,
				(s32)CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ * 1000);
	}
}
#endif

static irqreturn_t tegra_ehci_irq(struct usb_hcd *hcd)
{
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(hcd->self.controller);
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	irqreturn_t irq_status;

	spin_lock(&ehci->lock);
	irq_status = tegra_usb_phy_irq(tegra->phy);
	if (irq_status == IRQ_NONE) {
		spin_unlock(&ehci->lock);
		return irq_status;
	}
	if (tegra_usb_phy_pmc_wakeup(tegra->phy)) {
		ehci_dbg(ehci, "pmc interrupt detected\n");
		wake_lock_timeout(&tegra->ehci_wake_lock, 3 * HZ);
		usb_hcd_resume_root_hub(hcd);
		spin_unlock(&ehci->lock);
		return irq_status;
	}
	spin_unlock(&ehci->lock);

	EHCI_DBG("%s() cmd = 0x%x, int_sts = 0x%x, portsc = 0x%x\n", __func__,
		ehci_readl(ehci, &ehci->regs->command),
		ehci_readl(ehci, &ehci->regs->status),
		ehci_readl(ehci, &ehci->regs->port_status[0]));

	irq_status = ehci_irq(hcd);

	if (ehci->controller_remote_wakeup) {
		ehci->controller_remote_wakeup = false;
		tegra_usb_phy_pre_resume(tegra->phy, true);
		tegra->port_resuming = 1;
	}
	return irq_status;
}


static int tegra_ehci_hub_control(
	struct usb_hcd	*hcd,
	u16	typeReq,
	u16	wValue,
	u16	wIndex,
	char	*buf,
	u16	wLength
)
{
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(hcd->self.controller);
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int	retval = 0;
	u32 __iomem	*status_reg;

	if (!tegra_usb_phy_hw_accessible(tegra->phy)) {
		if (buf)
			memset(buf, 0, wLength);
		return retval;
	}

	/* Do tegra phy specific actions based on the type request */
	switch (typeReq) {
	case GetPortStatus:
		if (tegra->port_resuming) {
			u32 cmd;
			int delay = ehci->reset_done[wIndex-1] - jiffies;
			/* Sometimes it seems we get called too soon... In that case, wait.*/
			if (delay > 0) {
				ehci_dbg(ehci, "GetPortStatus called too soon, waiting %dms...\n", delay);
				mdelay(jiffies_to_msecs(delay));
			}
			status_reg = &ehci->regs->port_status[(wIndex & 0xff) - 1];
			/* Ensure the port PORT_SUSPEND and PORT_RESUME has cleared */
			if (handshake(ehci, status_reg, (PORT_SUSPEND | PORT_RESUME), 0, 25000)) {
				EHCI_DBG("%s: timeout waiting for SUSPEND to clear\n", __func__);
			}
			tegra_usb_phy_post_resume(tegra->phy);
			tegra->port_resuming = 0;
			/* If run bit is not set by now enable it */
			cmd = ehci_readl(ehci, &ehci->regs->command);
			if (!(cmd & CMD_RUN)) {
				cmd |= CMD_RUN;
				ehci->command |= CMD_RUN;
				ehci_writel(ehci, cmd, &ehci->regs->command);
			}
			/* Now we can safely re-enable irqs */
			ehci_writel(ehci, INTR_MASK, &ehci->regs->intr_enable);
		}
		break;
	case ClearPortFeature:
		if (wValue == USB_PORT_FEAT_SUSPEND) {
			tegra_usb_phy_pre_resume(tegra->phy, false);
			tegra->port_resuming = 1;
		} else if (wValue == USB_PORT_FEAT_ENABLE) {
			u32 temp;
			temp = ehci_readl(ehci, &ehci->regs->port_status[0]) & ~PORT_RWC_BITS;
			ehci_writel(ehci, temp & ~PORT_PE, &ehci->regs->port_status[0]);
			return retval;
		}
		break;
	case SetPortFeature:
		if (wValue == USB_PORT_FEAT_SUSPEND)
			tegra_usb_phy_pre_suspend(tegra->phy);
		break;
	}

	/* handle ehci hub control request */
	retval = ehci_hub_control(hcd, typeReq, wValue, wIndex, buf, wLength);

	/* do tegra phy specific actions based on the type request */
	if (!retval) {
		switch (typeReq) {
		case SetPortFeature:
			if (wValue == USB_PORT_FEAT_SUSPEND) {
				tegra_usb_phy_post_suspend(tegra->phy);
			} else if (wValue == USB_PORT_FEAT_RESET) {
				if (wIndex == 1)
					tegra_usb_phy_bus_reset(tegra->phy);
			} else if (wValue == USB_PORT_FEAT_POWER) {
				if (wIndex == 1)
					tegra_usb_phy_port_power(tegra->phy);
			}
			break;
		case ClearPortFeature:
			if (wValue == USB_PORT_FEAT_SUSPEND) {
				/* tegra USB controller needs 25 ms to resume the port */
				ehci->reset_done[wIndex-1] = jiffies + msecs_to_jiffies(25);
			}
			break;
		}
	}

	return retval;
}

static void tegra_ehci_shutdown(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(hcd->self.controller);
	struct platform_device *pdev = to_platform_device(hcd->self.controller);
	struct tegra_usb_platform_data *pdata = dev_get_platdata(&pdev->dev);
	mutex_lock(&tegra->sync_lock);
	if (tegra_usb_phy_hw_accessible(tegra->phy)) {
		ehci_silence_controller(ehci);
	}
	if (pdata->port_otg)
		tegra_usb_enable_vbus(tegra->phy, false);
	mutex_unlock(&tegra->sync_lock);
}

static int tegra_ehci_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(hcd->self.controller);
	int retval;
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	u32 val;
#endif

	/* EHCI registers start at offset 0x100 */
	ehci->caps = hcd->regs + 0x100;

	ehci->has_hostpc = tegra->has_hostpc;
	ehci->broken_hostpc_phcd = true;

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	ehci->has_hostpc = 1;

	val = readl(hcd->regs + HOSTPC_REG_OFFSET);
	val &= ~HOSTPC1_DEVLC_STS;
	val &= ~HOSTPC1_DEVLC_NYT_ASUS;
	writel(val, hcd->regs + HOSTPC_REG_OFFSET);
#endif
	/* switch to host mode */
	hcd->has_tt = 1;

	retval = ehci_setup(hcd);
	if (retval)
		return retval;

	ehci->controller_remote_wakeup = false;
	tegra_usb_phy_reset(tegra->phy);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	if (tegra_platform_is_fpga()) {
		val =  readl(hcd->regs + TEGRA_STREAM_DISABLE);
		val |= TEGRA_STREAM_DISABLE_OFFSET;
		writel(val , hcd->regs + TEGRA_STREAM_DISABLE);
	}
#endif

	return 0;
}


#ifdef CONFIG_PM
static int tegra_ehci_bus_suspend(struct usb_hcd *hcd)
{
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(hcd->self.controller);
	int err = 0;
	EHCI_DBG("%s() BEGIN\n", __func__);

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	tegra->boost_requested = false;
	if (tegra->boost_enable
	    && pm_qos_request_active(&tegra->boost_cpu_freq_req))
		pm_qos_update_request(&tegra->boost_cpu_freq_req,
			PM_QOS_DEFAULT_VALUE);
	tegra->cpu_boost_in_work = false;
#endif

	mutex_lock(&tegra->sync_lock);
	tegra->bus_suspended_fail = false;
	err = ehci_bus_suspend(hcd);
	if (err)
		tegra->bus_suspended_fail = true;
	else
		usb_phy_set_suspend(get_usb_phy(tegra->phy), 1);
	mutex_unlock(&tegra->sync_lock);
	EHCI_DBG("%s() END\n", __func__);

	return err;
}

static int tegra_ehci_bus_resume(struct usb_hcd *hcd)
{
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(hcd->self.controller);
	int err = 0;
	EHCI_DBG("%s() BEGIN\n", __func__);

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	tegra->boost_requested = true;
	if (pm_qos_request_active(&tegra->boost_cpu_freq_req)
		&& tegra->boost_enable) {
		schedule_delayed_work(&tegra->boost_cpu_freq_work, 4000);
		tegra->cpu_boost_in_work = true;
	}

#endif

	mutex_lock(&tegra->sync_lock);
	usb_phy_set_suspend(get_usb_phy(tegra->phy), 0);
	err = ehci_bus_resume(hcd);
	mutex_unlock(&tegra->sync_lock);
	EHCI_DBG("%s() END\n", __func__);

	return err;
}
#endif

static const struct hc_driver tegra_ehci_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "Tegra EHCI Host Controller",
	.hcd_priv_size		= sizeof(struct ehci_hcd),
	.flags			= HCD_USB2 | HCD_MEMORY,

	/* standard ehci functions */
	.start			= ehci_run,
	.stop			= ehci_stop,
	.urb_enqueue		= ehci_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,
	.endpoint_reset	= ehci_endpoint_reset,
	.get_frame_number	= ehci_get_frame,
	.hub_status_data	= ehci_hub_status_data,
	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,
	.relinquish_port	= ehci_relinquish_port,
	.port_handed_over	= ehci_port_handed_over,

	/* modified ehci functions for tegra */
	.reset			= tegra_ehci_setup,
	.irq			= tegra_ehci_irq,
	.shutdown		= tegra_ehci_shutdown,
	.map_urb_for_dma	= tegra_ehci_map_urb_for_dma,
	.unmap_urb_for_dma	= tegra_ehci_unmap_urb_for_dma,
	.hub_control		= tegra_ehci_hub_control,
#ifdef CONFIG_PM
	.bus_suspend	= tegra_ehci_bus_suspend,
	.bus_resume	= tegra_ehci_bus_resume,
#endif
};

static u64 tegra_ehci_dma_mask = DMA_BIT_MASK(32);

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
static ssize_t show_boost_enable(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 tegra->boost_enable ? "Y" : "N");
}


static ssize_t store_boost_enable(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct tegra_ehci_hcd *tegra = dev_get_drvdata(dev);
	bool new_boost;
	bool old_boost = tegra->boost_enable;

	if (strtobool(buf, &new_boost) == 0) {
		tegra->boost_enable = new_boost;
		if (!old_boost && new_boost
		    && tegra->boost_requested
		    && pm_qos_request_active(&tegra->boost_cpu_freq_req))
			pm_qos_update_request(
				&tegra->boost_cpu_freq_req,
				(s32)CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ * 1000);
		else if (old_boost && !new_boost
			 && pm_qos_request_active(&tegra->boost_cpu_freq_req))
			pm_qos_update_request(&tegra->boost_cpu_freq_req,
					      PM_QOS_DEFAULT_VALUE);
	}
	return count;
}
static DEVICE_ATTR(boost_enable, 0644,
		   show_boost_enable, store_boost_enable);
#endif

static int tegra_ehci_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct usb_hcd *hcd;
	struct tegra_ehci_hcd *tegra;
	struct tegra_usb_platform_data *pdata;
	int err = 0;
	int irq;
	int instance = pdev->id;

	/* Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we have dma capability bindings this can go away.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &tegra_ehci_dma_mask;

	pdata = dev_get_platdata(&pdev->dev);

	tegra = devm_kzalloc(&pdev->dev, sizeof(struct tegra_ehci_hcd),
			     GFP_KERNEL);
	if (!tegra) {
		dev_err(&pdev->dev, "memory alloc failed\n");
		return -ENOMEM;
	}

	mutex_init(&tegra->sync_lock);

	hcd = usb_create_hcd(&tegra_ehci_hc_driver, &pdev->dev,
					dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "unable to create HCD\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, tegra);

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	tegra->boost_requested = false;
	/* Add boost enable/disable knob */
	tegra->boost_enable = true;
	err = device_create_file(hcd->self.controller, &dev_attr_boost_enable);
	if (err < 0)
		goto fail_sysfs;
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get I/O memory\n");
		err = -ENXIO;
		goto fail_io;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!hcd->regs) {
		dev_err(&pdev->dev, "failed to remap I/O memory\n");
		err = -ENOMEM;
		goto fail_io;
	}

	/* This is pretty ugly and needs to be fixed when we do only
	 * device-tree probing. Old code relies on the platform_device
	 * numbering that we lack for device-tree-instantiated devices.
	 */
	if (instance < 0) {
		switch (res->start) {
		case TEGRA_USB_BASE:
			instance = 0;
			break;
		case TEGRA_USB2_BASE:
			instance = 1;
			break;
		case TEGRA_USB3_BASE:
			instance = 2;
			break;
		default:
			err = -ENODEV;
			dev_err(&pdev->dev, "unknown usb instance\n");
			goto fail_io;
		}
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		err = -ENODEV;
		goto fail_io;
	}
	tegra->irq = irq;

	tegra->unaligned_dma_buf_supported = pdata->unaligned_dma_buf_supported;
	tegra->has_hostpc = pdata->has_hostpc;

	tegra->phy = tegra_usb_phy_open(pdev);
	if (IS_ERR(tegra->phy)) {
		dev_err(&pdev->dev, "failed to open USB phy\n");
		err = -ENXIO;
		goto fail_io;
	}

	err = tegra_usb_phy_power_on(tegra->phy);
	if (err) {
		dev_err(&pdev->dev, "failed to power on the phy\n");
		goto fail_phy;
	}

	err = usb_phy_init(get_usb_phy(tegra->phy));
	if (err) {
		dev_err(&pdev->dev, "failed to init the phy\n");
		goto fail_phy;
	}

	err = usb_add_hcd(hcd, irq, IRQF_SHARED | IRQF_TRIGGER_HIGH);
	if (err) {
		dev_err(&pdev->dev, "Failed to add USB HCD, error=%d\n", err);
		goto fail_phy;
	}

	err = enable_irq_wake(tegra->irq);
	if (err < 0) {
		dev_warn(&pdev->dev,
				"Couldn't enable USB host mode wakeup, irq=%d, "
				"error=%d\n", irq, err);
		err = 0;
		tegra->irq = 0;
	}

	tegra->ehci = hcd_to_ehci(hcd);

	if (pdata->port_otg) {
		tegra->transceiver =
			devm_usb_get_phy(&pdev->dev, USB_PHY_TYPE_USB2);
		if (!IS_ERR_OR_NULL(tegra->transceiver))
			otg_set_host(tegra->transceiver->otg, &hcd->self);
	}

	tegra_pd_add_device(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	INIT_DELAYED_WORK(&tegra->boost_cpu_freq_work,
					tegra_ehci_boost_cpu_frequency_work);
	pm_qos_add_request(&tegra->boost_cpu_freq_req, PM_QOS_CPU_FREQ_MIN,
					PM_QOS_DEFAULT_VALUE);
	schedule_delayed_work(&tegra->boost_cpu_freq_work, 12000);
	tegra->cpu_boost_in_work = true;
#endif

	wake_lock_init(&tegra->ehci_wake_lock,
		       WAKE_LOCK_SUSPEND, dev_name(&pdev->dev));

	return err;

fail_phy:
	usb_phy_shutdown(get_usb_phy(tegra->phy));
fail_io:
#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	device_remove_file(hcd->self.controller, &dev_attr_boost_enable);
fail_sysfs:
#endif
	usb_put_hcd(hcd);

	return err;
}


#ifdef CONFIG_PM
static int tegra_ehci_resume(struct platform_device *pdev)
{
	struct tegra_ehci_hcd *tegra = platform_get_drvdata(pdev);
	struct tegra_usb_platform_data *pdata = dev_get_platdata(&pdev->dev);
	if (pdata->u_data.host.turn_off_vbus_on_lp0) {
		tegra_usb_enable_vbus(tegra->phy, true);
		tegra_ehci_notify_event(tegra, USB_EVENT_ID);
	}
	return tegra_usb_phy_power_on(tegra->phy);
}

static int tegra_ehci_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tegra_ehci_hcd *tegra = platform_get_drvdata(pdev);
	struct tegra_usb_platform_data *pdata = dev_get_platdata(&pdev->dev);
	int err;

	/* bus suspend could have failed because of remote wakeup resume */
	if (tegra->bus_suspended_fail)
		return -EBUSY;
	else {
		err = tegra_usb_phy_power_off(tegra->phy);
		if (pdata->u_data.host.turn_off_vbus_on_lp0) {
			tegra_usb_enable_vbus(tegra->phy, false);
			tegra_usb_phy_pmc_disable(tegra->phy);
		}
		return err;
	}
}
#endif

static int tegra_ehci_remove(struct platform_device *pdev)
{
	struct tegra_ehci_hcd *tegra = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tegra->ehci);
	struct usb_device *rhdev = NULL;
	struct tegra_usb_platform_data *pdata;
	unsigned long timeout = 0;

	wake_lock_destroy(&tegra->ehci_wake_lock);

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	cancel_delayed_work_sync(&tegra->boost_cpu_freq_work);
	tegra->cpu_boost_in_work = false;
	pm_qos_remove_request(&tegra->boost_cpu_freq_req);
#endif
	rhdev = hcd->self.root_hub;
	pdata = dev_get_platdata(&pdev->dev);

	if (!IS_ERR_OR_NULL(tegra->transceiver))
		otg_set_host(tegra->transceiver->otg, NULL);

	if (tegra->irq)
		disable_irq_wake(tegra->irq);

	/* Make sure phy is powered ON to access USB register */
	if(!tegra_usb_phy_hw_accessible(tegra->phy))
		tegra_usb_phy_power_on(tegra->phy);

	if (pdata->port_otg) {
		timeout = jiffies + 5 * HZ;
		/* wait for devices connected to root hub to disconnect*/
		while (rhdev && usb_hub_find_child(rhdev, 1)) {
			/* wait for any control packets
			sent to root hub to complete */
			if (time_after(jiffies, timeout))
				break;
			msleep(20);
			cpu_relax();
		}
	}

#ifdef CONFIG_TEGRA_EHCI_BOOST_CPU_FREQ
	device_remove_file(hcd->self.controller, &dev_attr_boost_enable);
#endif
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
	tegra_usb_phy_power_off(tegra->phy);
	usb_phy_shutdown(get_usb_phy(tegra->phy));

	mutex_destroy(&tegra->sync_lock);
	tegra_pd_remove_device(&pdev->dev);

	return 0;
}

static void tegra_ehci_hcd_shutdown(struct platform_device *pdev)
{
	struct tegra_ehci_hcd *tegra = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tegra->ehci);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

static struct of_device_id tegra_ehci_of_match[] = {
	{ .compatible = "nvidia,tegra20-ehci", },
	{ },
};

static struct platform_driver tegra_ehci_driver = {
	.probe		= tegra_ehci_probe,
	.remove		= tegra_ehci_remove,
	.shutdown	= tegra_ehci_hcd_shutdown,
#ifdef CONFIG_PM
	.suspend = tegra_ehci_suspend,
	.resume  = tegra_ehci_resume,
#endif
	.driver	= {
		.name	= driver_name,
		.of_match_table = tegra_ehci_of_match,
	}
};

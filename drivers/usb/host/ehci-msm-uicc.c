/* ehci-msm-uicc.c - UICC (Full-Speed) Host Controller Driver Implementation
 *
 * Copyright (c) 2008-2014, The Linux Foundation. All rights reserved.
 *
 * Partly derived from ehci-fsl.c and ehci-hcd.c
 * Copyright (c) 2000-2004 by David Brownell
 * Copyright (c) 2005 MontaVista Software
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk.h>
#include <linux/msm-bus.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>

#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/msm_hsusb_hw.h>

#include "ehci.h"

static bool uicc_card_present;
module_param(uicc_card_present, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(uicc_card_present, "UICC card is inserted");

#define MSM_USB_BASE (hcd->regs)

struct uicc_hcd {
	struct ehci_hcd ehci;
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *active_ps;
	struct pinctrl_state *sleep_ps;
	struct clk *core_clk;
	struct clk *iface_clk;
	struct clk *alt_core_clk;
	bool clocks_on;
	uint32_t bus_voter;
};

static struct uicc_hcd *hcd_to_uhcd(struct usb_hcd *hcd)
{
	return (struct uicc_hcd *) (hcd->hcd_priv);
}

static void ehci_msm_uicc_disable_clocks(struct uicc_hcd *uhcd)
{
	if (!uhcd->clocks_on)
		return;

	clk_disable_unprepare(uhcd->core_clk);
	clk_disable_unprepare(uhcd->iface_clk);
	clk_disable_unprepare(uhcd->alt_core_clk);
	uhcd->clocks_on = false;
}

static void ehci_msm_uicc_enable_clocks(struct uicc_hcd *uhcd)
{
	if (uhcd->clocks_on)
		return;

	clk_prepare_enable(uhcd->core_clk);
	clk_prepare_enable(uhcd->iface_clk);
	clk_prepare_enable(uhcd->alt_core_clk);
	uhcd->clocks_on = true;
}

static int ehci_msm_uicc_block_reset(struct uicc_hcd *uhcd)
{
	int ret;

	ehci_msm_uicc_disable_clocks(uhcd);
	ret = clk_reset(uhcd->core_clk, CLK_RESET_ASSERT);
	if (ret) {
		pr_err("Fail to assert the core clock %d\n", ret);
		goto out;
	}

	/*
	 * 10 msec delay is required between assert and de-assert
	 * as per the hardware data book.
	 */
	usleep_range(10000, 12000);

	ret = clk_reset(uhcd->core_clk, CLK_RESET_DEASSERT);
	if (ret) {
		pr_err("Fail to de-assert the core clock %d\n", ret);
		goto out;
	}

	/*
	 * 200 nsec delay is required before enabling the clocks
	 * as per the hardware databook.
	 */
	ndelay(200);

	ehci_msm_uicc_enable_clocks(uhcd);
out:
	return ret;
}

static int ehci_msm_uicc_init_clocks(struct uicc_hcd *uhcd)
{
	long rate;
	int ret;

	uhcd->core_clk = devm_clk_get(uhcd->dev, "core_clk");
	if (IS_ERR(uhcd->core_clk)) {
		ret = PTR_ERR(uhcd->iface_clk);
		goto out;
	}

	rate = clk_round_rate(uhcd->core_clk, LONG_MAX);
	if (IS_ERR_VALUE(rate)) {
		ret = rate;
		pr_err("Fail to get core clk rate %d\n", ret);
		goto out;
	}

	ret = clk_set_rate(uhcd->core_clk, rate);
	if (ret) {
		pr_err("Fail to set core clk rate %ld %d\n", rate, ret);
		goto out;
	}

	uhcd->iface_clk = devm_clk_get(uhcd->dev, "iface_clk");
	if (IS_ERR(uhcd->iface_clk)) {
		ret = PTR_ERR(uhcd->core_clk);
		goto out;
	}

	/*
	 * UICC controller does not have a PHY. The 60 MHZ clock
	 * comes from the clock controller.
	 */
	uhcd->alt_core_clk = devm_clk_get(uhcd->dev, "alt_core_clk");
	if (IS_ERR(uhcd->alt_core_clk)) {
		ret = PTR_ERR(uhcd->iface_clk);
		goto out;
	}

	ret = clk_set_rate(uhcd->alt_core_clk, 60000000);
	if (ret)
		pr_err("Fail to set alt core clk rate %d\n", ret);

out:
	return ret;
}

static int ehci_msm_uicc_bus_suspend(struct usb_hcd *hcd)
{
	struct uicc_hcd *uhcd = hcd_to_uhcd(hcd);
	int ret;

	ret = ehci_bus_suspend(hcd);
	if (ret < 0) {
		pr_err("Fail to suspend the bus\n");
		goto out;
	}
	ehci_msm_uicc_disable_clocks(uhcd);
	pinctrl_select_state(uhcd->pinctrl, uhcd->sleep_ps);

	if (uhcd->bus_voter)
		msm_bus_scale_client_update_request(uhcd->bus_voter, 0);
	pm_relax(uhcd->dev);
out:
	return ret;
}

static int ehci_msm_uicc_bus_resume(struct usb_hcd *hcd)
{
	struct uicc_hcd *uhcd = hcd_to_uhcd(hcd);
	int ret;

	pm_stay_awake(uhcd->dev);
	if (uhcd->bus_voter)
		msm_bus_scale_client_update_request(uhcd->bus_voter, 1);

	pinctrl_select_state(uhcd->pinctrl, uhcd->active_ps);

	ehci_msm_uicc_enable_clocks(uhcd);

	ret = ehci_bus_resume(hcd);
	if (ret < 0)
		pr_err("Fail to resume the bus\n");

	return ret;
}

static int ehci_msm_uicc_reset(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	struct uicc_hcd *uhcd = hcd_to_uhcd(hcd);
	int ret;

	ret = ehci_msm_uicc_block_reset(uhcd);
	if (ret < 0) {
		pr_err("Fail to reset the uicc block %d\n", ret);
		goto out;
	}
	ehci->caps = USB_CAPLENGTH;
	/*
	 * This is Full-Speed only controller. The TT
	 * does not apply. We set this flag to
	 * ehci_is_TDI() return true for some quirks.
	 */
	hcd->has_tt = 1;

	ret = ehci_setup(hcd);
	if (ret) {
		pr_err("Fail to setup EHCI %d\n", ret);
		goto out;
	}

	/*
	 * UICC specific initialization:
	 * - Select serial PHY
	 * - Enable short port reset i.e disable chirp to
	 *   save few msec during reset
	 * - Apply Port power
	 * - Enable FS 3-wire protocol
	 */

	writel_relaxed(readl_relaxed(USB_PORTSC) |
			PORTSC_PTS_SERIAL | PORTSC_SPRT |
			PORTSC_PP, USB_PORTSC);
	ehci->command |= USBCMD_FS_SELECT;

	/* bursts of unspecified length. */
	writel_relaxed(0, USB_AHBBURST);
	/* Use the AHB transactor */
	writel_relaxed(0x08, USB_AHBMODE);
	/* Disable streaming mode and select host mode */
	writel_relaxed(0x13, USB_USBMODE);
out:
	return ret;
}

static const struct ehci_driver_overrides ehci_msm_uicc_overrides = {
	.extra_priv_size = sizeof(struct uicc_hcd),
	.flags = HCD_MEMORY | HCD_USB11,
	.reset = ehci_msm_uicc_reset,
	.bus_suspend = ehci_msm_uicc_bus_suspend,
	.bus_resume = ehci_msm_uicc_bus_resume,
};

static u64 ehci_msm_uicc_dma_mask = DMA_BIT_MASK(64);
static struct hc_driver ehci_msm_uicc_hc_driver;

static int ehci_msm_uicc_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct uicc_hcd *uhcd;
	struct resource *res;
	struct msm_bus_scale_pdata *table;
	int ret;

	if (!uicc_card_present) {
		pr_debug("UICC card is not inserted\n");
		ret = -ENODEV;
		goto out;
	}

	pdev->dev.dma_mask = &ehci_msm_uicc_dma_mask;
	ehci_init_driver(&ehci_msm_uicc_hc_driver, &ehci_msm_uicc_overrides);

	hcd = usb_create_hcd(&ehci_msm_uicc_hc_driver, &pdev->dev,
			     dev_name(&pdev->dev));
	if (!hcd) {
		pr_err("Fail to create HCD\n");
		ret = -ENOMEM;
		goto out;
	}
	uhcd = hcd_to_uhcd(hcd);
	uhcd->dev = &pdev->dev;

	ret = ehci_msm_uicc_init_clocks(uhcd);
	if ((ret < 0) && (ret != -EPROBE_DEFER)) {
		pr_err("Fail to init clocks %d\n", ret);
		goto put_hcd;
	}

	uhcd->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(uhcd->pinctrl)) {
		ret = PTR_ERR(uhcd->pinctrl);
		pr_err("Fail to get pinctrl info %d\n", ret);
		goto put_hcd;
	}

	uhcd->active_ps = pinctrl_lookup_state(uhcd->pinctrl, "uicc_active");
	if (IS_ERR(uhcd->active_ps)) {
		ret = PTR_ERR(uhcd->active_ps);
		pr_err("Fail to get active pinctrl %d\n", ret);
		goto put_hcd;
	}

	uhcd->sleep_ps = pinctrl_lookup_state(uhcd->pinctrl, "uicc_sleep");
	if (IS_ERR(uhcd->sleep_ps)) {
		ret = PTR_ERR(uhcd->sleep_ps);
		pr_err("Fail to get sleep pinctrl %d\n", ret);
		goto put_hcd;
	}

	ret = pinctrl_select_state(uhcd->pinctrl, uhcd->active_ps);
	if (ret) {
		pr_err("Fail to select active pinctrl %d\n", ret);
		goto put_hcd;
	}

	hcd->irq = platform_get_irq(pdev, 0);
	if (hcd->irq < 0) {
		ret = hcd->irq;
		pr_err("Fail to get the USB IRQ %d\n", ret);
		goto select_sleep;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get memory resource\n");
		pr_err("Fail to get the iomemory\n");
		ret = -ENODEV;
		goto select_sleep;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (!hcd->regs) {
		pr_err("Fail to ioremap\n");
		ret = -ENOMEM;
		goto select_sleep;
	}

	ehci_msm_uicc_enable_clocks(uhcd);

	table = msm_bus_cl_get_pdata(pdev);
	if (table) {
		uhcd->bus_voter = msm_bus_scale_register_client(table);
		if (!uhcd->bus_voter)
			pr_debug("Fail to get bus voter handle\n");
		else
			msm_bus_scale_client_update_request(uhcd->bus_voter, 1);
	}

	ret = usb_add_hcd(hcd, hcd->irq, 0);
	if (ret < 0) {
		pr_err("Fail to add the HCD\n");
		goto disable_clk_bus;
	}

	/*
	 * We manage the clocks and gpio as part of
	 * the root hub PM. The platform driver runtime
	 * and system sleep methods are not used.
	 */
	pm_runtime_no_callbacks(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	/*
	 * This does not mean this controller can wakeup the
	 * system from sleep. It's activity can prevent
	 * or abort the system sleep. The device_init_wakeup
	 * creates the wakeup source for us which we will
	 * use to control the system sleep.
	 */
	device_init_wakeup(&pdev->dev, 1);
	pm_stay_awake(&pdev->dev);

	return 0;

disable_clk_bus:
	if (uhcd->bus_voter)
		msm_bus_scale_unregister_client(uhcd->bus_voter);
	ehci_msm_uicc_disable_clocks(uhcd);
select_sleep:
	pinctrl_select_state(uhcd->pinctrl, uhcd->sleep_ps);
put_hcd:
	usb_put_hcd(hcd);
out:
	return ret;
}

static int ehci_msm_uicc_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct uicc_hcd *uhcd = hcd_to_uhcd(hcd);
	struct usb_device *rhdev = hcd->self.root_hub;

	pm_runtime_get_sync(&rhdev->dev);

	usb_remove_hcd(hcd);
	ehci_msm_uicc_disable_clocks(uhcd);

	if (uhcd->bus_voter) {
		msm_bus_scale_client_update_request(uhcd->bus_voter, 0);
		msm_bus_scale_unregister_client(uhcd->bus_voter);
	}

	pinctrl_select_state(uhcd->pinctrl, uhcd->sleep_ps);
	usb_put_hcd(hcd);

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	pm_relax(&pdev->dev);

	return 0;
}

static const struct of_device_id ehci_msm2_dt_match[] = {
	{ .compatible = "qcom,ehci-uicc-host",
	},
	{}
};

static struct platform_driver ehci_msm_uicc_driver = {
	.probe = ehci_msm_uicc_probe,
	.remove = ehci_msm_uicc_remove,
	.driver = {
		.name = "msm_ehci_uicc",
		.owner = THIS_MODULE,
		.of_match_table = ehci_msm2_dt_match,
	},
};

module_platform_driver(ehci_msm_uicc_driver);

MODULE_DESCRIPTION("MSM UICC (Full-Speed) HCD");
MODULE_LICENSE("GPL v2");

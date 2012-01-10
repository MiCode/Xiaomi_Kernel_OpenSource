/* ehci-msm-hsic.c - HSUSB Host Controller Driver Implementation
 *
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/wakelock.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include <linux/usb/msm_hsusb_hw.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/gpio.h>
#include <mach/clk.h>
#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>
#include <linux/spinlock.h>

#define MSM_USB_BASE (hcd->regs)

struct msm_hsic_hcd {
	struct ehci_hcd		ehci;
	struct device		*dev;
	struct clk		*ahb_clk;
	struct clk		*core_clk;
	struct clk		*alt_core_clk;
	struct clk		*phy_clk;
	struct clk		*cal_clk;
	struct regulator	*hsic_vddcx;
	bool			async_int;
	atomic_t                in_lpm;
	struct msm_xo_voter	*xo_handle;
	struct wake_lock	wlock;
	int			peripheral_status_irq;
};

static inline struct msm_hsic_hcd *hcd_to_hsic(struct usb_hcd *hcd)
{
	return (struct msm_hsic_hcd *) (hcd->hcd_priv);
}

static inline struct usb_hcd *hsic_to_hcd(struct msm_hsic_hcd *mehci)
{
	return container_of((void *) mehci, struct usb_hcd, hcd_priv);
}

#define ULPI_IO_TIMEOUT_USEC	(10 * 1000)

#define USB_PHY_VDD_DIG_VOL_SUSP_MIN	500000 /* uV */
#define USB_PHY_VDD_DIG_VOL_MIN		1000000 /* uV */
#define USB_PHY_VDD_DIG_VOL_MAX		1320000 /* uV */
#define USB_PHY_VDD_DIG_LOAD		49360	/* uA */

static int msm_hsic_init_vddcx(struct msm_hsic_hcd *mehci, int init)
{
	int ret = 0;

	if (!init)
		goto disable_reg;

	mehci->hsic_vddcx = regulator_get(mehci->dev, "HSIC_VDDCX");
	if (IS_ERR(mehci->hsic_vddcx)) {
		dev_err(mehci->dev, "unable to get hsic vddcx\n");
		return PTR_ERR(mehci->hsic_vddcx);
	}

	ret = regulator_set_voltage(mehci->hsic_vddcx,
			USB_PHY_VDD_DIG_VOL_MIN,
			USB_PHY_VDD_DIG_VOL_MAX);
	if (ret) {
		dev_err(mehci->dev, "unable to set the voltage"
				"for hsic vddcx\n");
		goto reg_set_voltage_err;
	}

	ret = regulator_set_optimum_mode(mehci->hsic_vddcx,
				USB_PHY_VDD_DIG_LOAD);
	if (ret < 0) {
		pr_err("%s: Unable to set optimum mode of the regulator:"
					"VDDCX\n", __func__);
		goto reg_optimum_mode_err;
	}

	ret = regulator_enable(mehci->hsic_vddcx);
	if (ret) {
		dev_err(mehci->dev, "unable to enable hsic vddcx\n");
		goto reg_enable_err;
	}

	return 0;

disable_reg:
	regulator_disable(mehci->hsic_vddcx);
reg_enable_err:
	regulator_set_optimum_mode(mehci->hsic_vddcx, 0);
reg_optimum_mode_err:
	regulator_set_voltage(mehci->hsic_vddcx, 0,
				USB_PHY_VDD_DIG_VOL_MIN);
reg_set_voltage_err:
	regulator_put(mehci->hsic_vddcx);

	return ret;

}

static int ulpi_write(struct msm_hsic_hcd *mehci, u32 val, u32 reg)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	int cnt = 0;

	/* initiate write operation */
	writel_relaxed(ULPI_RUN | ULPI_WRITE |
	       ULPI_ADDR(reg) | ULPI_DATA(val),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	while (cnt < ULPI_IO_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_RUN))
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= ULPI_IO_TIMEOUT_USEC) {
		dev_err(mehci->dev, "ulpi_write: timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

#define HSIC_HUB_VDD_VOL_MIN	1650000 /* uV */
#define HSIC_HUB_VDD_VOL_MAX	1950000 /* uV */
#define HSIC_HUB_VDD_LOAD	36000	/* uA */
static int msm_hsic_config_hub(struct msm_hsic_hcd *mehci, int init)
{
	int ret = 0;
	struct msm_hsic_host_platform_data *pdata;
	static struct regulator *hsic_hub_reg;

	pdata = mehci->dev->platform_data;
	if (!pdata->hub_reset)
		return ret;

	if (!init)
		goto disable_reg;

	hsic_hub_reg = regulator_get(mehci->dev, "EXT_HUB_VDDIO");
	if (IS_ERR(hsic_hub_reg)) {
		dev_err(mehci->dev, "unable to get ext hub vddcx\n");
		return PTR_ERR(hsic_hub_reg);
	}

	ret = gpio_request(pdata->hub_reset, "HSIC_HUB_RESET_GPIO");
	if (ret < 0) {
		dev_err(mehci->dev, "gpio request failed for GPIO%d\n",
							pdata->hub_reset);
		goto gpio_req_fail;
	}

	ret = regulator_set_voltage(hsic_hub_reg,
			HSIC_HUB_VDD_VOL_MIN,
			HSIC_HUB_VDD_VOL_MAX);
	if (ret) {
		dev_err(mehci->dev, "unable to set the voltage"
				"for hsic hub reg\n");
		goto reg_set_voltage_fail;
	}

	ret = regulator_set_optimum_mode(hsic_hub_reg,
				HSIC_HUB_VDD_LOAD);
	if (ret < 0) {
		pr_err("%s: Unable to set optimum mode of the regulator:"
					"VDDCX\n", __func__);
		goto reg_optimum_mode_fail;
	}

	ret = regulator_enable(hsic_hub_reg);
	if (ret) {
		dev_err(mehci->dev, "unable to enable ext hub vddcx\n");
		goto reg_enable_fail;
	}

	gpio_direction_output(pdata->hub_reset, 0);
	/* Hub reset should be asserted for minimum 2usec before deasserting */
	udelay(5);
	gpio_direction_output(pdata->hub_reset, 1);

	return 0;

disable_reg:
	regulator_disable(hsic_hub_reg);
reg_enable_fail:
	regulator_set_optimum_mode(hsic_hub_reg, 0);
reg_optimum_mode_fail:
	regulator_set_voltage(hsic_hub_reg, 0,
				HSIC_HUB_VDD_VOL_MIN);
reg_set_voltage_fail:
	gpio_free(pdata->hub_reset);
gpio_req_fail:
	regulator_put(hsic_hub_reg);

	return ret;

}

static int msm_hsic_config_gpios(struct msm_hsic_hcd *mehci, int gpio_en)
{
	int rc = 0;
	struct msm_hsic_host_platform_data *pdata;
	static int gpio_status;

	pdata = mehci->dev->platform_data;

	if (!pdata->strobe || !pdata->data)
		return rc;

	if (gpio_status == gpio_en)
		return 0;

	gpio_status = gpio_en;

	if (!gpio_en)
		goto free_gpio;

	rc = gpio_request(pdata->strobe, "HSIC_STROBE_GPIO");
	if (rc < 0) {
		dev_err(mehci->dev, "gpio request failed for HSIC STROBE\n");
		return rc;
	}

	rc = gpio_request(pdata->data, "HSIC_DATA_GPIO");
	if (rc < 0) {
		dev_err(mehci->dev, "gpio request failed for HSIC DATA\n");
		goto free_strobe;
		}

	return 0;

free_gpio:
	gpio_free(pdata->data);
free_strobe:
	gpio_free(pdata->strobe);

	return rc;
}

static int msm_hsic_phy_clk_reset(struct msm_hsic_hcd *mehci)
{
	int ret;

	clk_enable(mehci->alt_core_clk);

	ret = clk_reset(mehci->core_clk, CLK_RESET_ASSERT);
	if (ret) {
		clk_disable(mehci->alt_core_clk);
		dev_err(mehci->dev, "usb phy clk assert failed\n");
		return ret;
	}
	usleep_range(10000, 12000);
	clk_disable(mehci->alt_core_clk);

	ret = clk_reset(mehci->core_clk, CLK_RESET_DEASSERT);
	if (ret)
		dev_err(mehci->dev, "usb phy clk deassert failed\n");

	return ret;
}

static int msm_hsic_phy_reset(struct msm_hsic_hcd *mehci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	u32 val;
	int ret;

	ret = msm_hsic_phy_clk_reset(mehci);
	if (ret)
		return ret;

	val = readl_relaxed(USB_PORTSC) & ~PORTSC_PTS_MASK;
	writel_relaxed(val | PORTSC_PTS_ULPI, USB_PORTSC);

	/* Ensure that RESET operation is completed before turning off clock */
	mb();
	dev_dbg(mehci->dev, "phy_reset: success\n");

	return 0;
}

#define HSIC_GPIO150_PAD_CTL   (MSM_TLMM_BASE+0x20C0)
#define HSIC_GPIO151_PAD_CTL   (MSM_TLMM_BASE+0x20C4)
#define HSIC_CAL_PAD_CTL       (MSM_TLMM_BASE+0x20C8)
#define HSIC_LV_MODE		0x04
#define HSIC_PAD_CALIBRATION	0xA8
#define HSIC_GPIO_PAD_VAL	0x0A0AAA10
#define LINK_RESET_TIMEOUT_USEC		(250 * 1000)
static int msm_hsic_reset(struct msm_hsic_hcd *mehci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	int cnt = 0;
	int ret;

	ret = msm_hsic_phy_reset(mehci);
	if (ret) {
		dev_err(mehci->dev, "phy_reset failed\n");
		return ret;
	}

	writel_relaxed(USBCMD_RESET, USB_USBCMD);
	while (cnt < LINK_RESET_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_USBCMD) & USBCMD_RESET))
			break;
		udelay(1);
		cnt++;
	}
	if (cnt >= LINK_RESET_TIMEOUT_USEC)
		return -ETIMEDOUT;

	/* select ULPI phy */
	writel_relaxed(0x80000000, USB_PORTSC);

	/* TODO: Need to confirm if HSIC PHY also requires delay after RESET */
	msleep(100);

	/* HSIC PHY Initialization */
	/* Enable LV_MODE in HSIC_CAL_PAD_CTL register */
	writel_relaxed(HSIC_LV_MODE, HSIC_CAL_PAD_CTL);

	/*set periodic calibration interval to ~2.048sec in HSIC_IO_CAL_REG */
	ulpi_write(mehci, 0xFF, 0x33);

	/* Enable periodic IO calibration in HSIC_CFG register */
	ulpi_write(mehci, HSIC_PAD_CALIBRATION, 0x30);

	/* Configure GPIO 150/151 pins for HSIC functionality mode */
	ret = msm_hsic_config_gpios(mehci, 1);
	if (ret) {
		dev_err(mehci->dev, " gpio configuarion failed\n");
		return ret;
	}

	/* Set LV_MODE=0x1 and DCC=0x2 in HSIC_GPIO150/151_PAD_CTL register */
	writel_relaxed(HSIC_GPIO_PAD_VAL, HSIC_GPIO150_PAD_CTL);
	writel_relaxed(HSIC_GPIO_PAD_VAL, HSIC_GPIO151_PAD_CTL);

	/* Enable HSIC mode in HSIC_CFG register */
	ulpi_write(mehci, 0x01, 0x31);

	return 0;
}

#define PHY_SUSPEND_TIMEOUT_USEC	(500 * 1000)
#define PHY_RESUME_TIMEOUT_USEC		(100 * 1000)

#ifdef CONFIG_PM_SLEEP
static int msm_hsic_suspend(struct msm_hsic_hcd *mehci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	int cnt = 0, ret;
	u32 val;
	struct msm_hsic_host_platform_data *pdata;

	if (atomic_read(&mehci->in_lpm)) {
		dev_dbg(mehci->dev, "%s called in lpm\n", __func__);
		return 0;
	}

	disable_irq(hcd->irq);
	/*
	 * PHY may take some time or even fail to enter into low power
	 * mode (LPM). Hence poll for 500 msec and reset the PHY and link
	 * in failure case.
	 */
	val = readl_relaxed(USB_PORTSC) | PORTSC_PHCD;
	writel_relaxed(val, USB_PORTSC);
	while (cnt < PHY_SUSPEND_TIMEOUT_USEC) {
		if (readl_relaxed(USB_PORTSC) & PORTSC_PHCD)
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= PHY_SUSPEND_TIMEOUT_USEC) {
		dev_err(mehci->dev, "Unable to suspend PHY\n");
		msm_hsic_config_gpios(mehci, 0);
		msm_hsic_reset(mehci);
	}

	/*
	 * PHY has capability to generate interrupt asynchronously in low
	 * power mode (LPM). This interrupt is level triggered. So USB IRQ
	 * line must be disabled till async interrupt enable bit is cleared
	 * in USBCMD register. Assert STP (ULPI interface STOP signal) to
	 * block data communication from PHY.
	 */
	writel_relaxed(readl_relaxed(USB_USBCMD) | ASYNC_INTR_CTRL |
				ULPI_STP_CTRL, USB_USBCMD);

	/*
	 * Ensure that hardware is put in low power mode before
	 * clocks are turned OFF and VDD is allowed to minimize.
	 */
	mb();

	clk_disable(mehci->core_clk);
	clk_disable(mehci->phy_clk);
	clk_disable(mehci->cal_clk);
	clk_disable(mehci->ahb_clk);
	pdata = mehci->dev->platform_data;
	if (pdata->hub_reset) {
		ret = msm_xo_mode_vote(mehci->xo_handle, MSM_XO_MODE_OFF);
		if (ret)
			pr_err("%s failed to devote for"
				"TCXO D1 buffer%d\n", __func__, ret);
	}

	ret = regulator_set_voltage(mehci->hsic_vddcx,
				USB_PHY_VDD_DIG_VOL_SUSP_MIN,
				USB_PHY_VDD_DIG_VOL_MAX);
	if (ret < 0)
		dev_err(mehci->dev, "unable to set vddcx voltage: min:0.5v max:1.3v\n");

	atomic_set(&mehci->in_lpm, 1);
	enable_irq(hcd->irq);
	wake_unlock(&mehci->wlock);

	dev_info(mehci->dev, "HSIC-USB in low power mode\n");

	return 0;
}

static int msm_hsic_resume(struct msm_hsic_hcd *mehci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	int cnt = 0, ret;
	unsigned temp;
	struct msm_hsic_host_platform_data *pdata;

	if (!atomic_read(&mehci->in_lpm)) {
		dev_dbg(mehci->dev, "%s called in !in_lpm\n", __func__);
		return 0;
	}

	wake_lock(&mehci->wlock);

	ret = regulator_set_voltage(mehci->hsic_vddcx,
				USB_PHY_VDD_DIG_VOL_MIN,
				USB_PHY_VDD_DIG_VOL_MAX);
	if (ret < 0)
		dev_err(mehci->dev, "unable to set vddcx voltage: min:1v max:1.3v\n");

	pdata = mehci->dev->platform_data;
	if (pdata->hub_reset) {
		ret = msm_xo_mode_vote(mehci->xo_handle, MSM_XO_MODE_ON);
		if (ret)
			pr_err("%s failed to vote for"
				"TCXO D1 buffer%d\n", __func__, ret);
	}
	clk_enable(mehci->core_clk);
	clk_enable(mehci->phy_clk);
	clk_enable(mehci->cal_clk);
	clk_enable(mehci->ahb_clk);

	temp = readl_relaxed(USB_USBCMD);
	temp &= ~ASYNC_INTR_CTRL;
	temp &= ~ULPI_STP_CTRL;
	writel_relaxed(temp, USB_USBCMD);

	if (!(readl_relaxed(USB_PORTSC) & PORTSC_PHCD))
		goto skip_phy_resume;

	temp = readl_relaxed(USB_PORTSC) & ~PORTSC_PHCD;
	writel_relaxed(temp, USB_PORTSC);
	while (cnt < PHY_RESUME_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_PORTSC) & PORTSC_PHCD) &&
			(readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_SYNC_STATE))
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= PHY_RESUME_TIMEOUT_USEC) {
		/*
		 * This is a fatal error. Reset the link and
		 * PHY to make hsic working.
		 */
		dev_err(mehci->dev, "Unable to resume USB. Reset the hsic\n");
		msm_hsic_config_gpios(mehci, 0);
		msm_hsic_reset(mehci);
	}

skip_phy_resume:

	atomic_set(&mehci->in_lpm, 0);

	if (mehci->async_int) {
		mehci->async_int = false;
		pm_runtime_put_noidle(mehci->dev);
		enable_irq(hcd->irq);
	}

	dev_info(mehci->dev, "HSIC-USB exited from low power mode\n");

	return 0;
}
#endif

static irqreturn_t msm_hsic_irq(struct usb_hcd *hcd)
{
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	if (atomic_read(&mehci->in_lpm)) {
		disable_irq_nosync(hcd->irq);
		mehci->async_int = true;
		pm_runtime_get(mehci->dev);
		return IRQ_HANDLED;
	}

	return ehci_irq(hcd);
}

static int ehci_hsic_reset(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int retval;

	ehci->caps = USB_CAPLENGTH;
	ehci->regs = USB_CAPLENGTH +
		HC_LENGTH(ehci, ehci_readl(ehci, &ehci->caps->hc_capbase));
	dbg_hcs_params(ehci, "reset");
	dbg_hcc_params(ehci, "reset");

	/* cache the data to minimize the chip reads*/
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);

	hcd->has_tt = 1;
	ehci->sbrn = HCD_USB2;

	retval = ehci_halt(ehci);
	if (retval)
		return retval;

	/* data structure init */
	retval = ehci_init(hcd);
	if (retval)
		return retval;

	retval = ehci_reset(ehci);
	if (retval)
		return retval;

	/* bursts of unspecified length. */
	writel_relaxed(0, USB_AHBBURST);
	/* Use the AHB transactor */
	writel_relaxed(0, USB_AHBMODE);
	/* Disable streaming mode and select host mode */
	writel_relaxed(0x13, USB_USBMODE);

	ehci_port_power(ehci, 1);
	return 0;
}

static struct hc_driver msm_hsic_driver = {
	.description		= hcd_name,
	.product_desc		= "Qualcomm EHCI Host Controller using HSIC",
	.hcd_priv_size		= sizeof(struct msm_hsic_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq			= msm_hsic_irq,
	.flags			= HCD_USB2 | HCD_MEMORY,

	.reset			= ehci_hsic_reset,
	.start			= ehci_run,

	.stop			= ehci_stop,
	.shutdown		= ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ehci_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,
	.endpoint_reset		= ehci_endpoint_reset,
	.clear_tt_buffer_complete	 = ehci_clear_tt_buffer_complete,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ehci_hub_status_data,
	.hub_control		= ehci_hub_control,
	.relinquish_port	= ehci_relinquish_port,
	.port_handed_over	= ehci_port_handed_over,

	/*
	 * PM support
	 */
	.bus_suspend		= ehci_bus_suspend,
	.bus_resume		= ehci_bus_resume,
};

static int msm_hsic_init_clocks(struct msm_hsic_hcd *mehci, u32 init)
{
	int ret = 0;

	if (!init)
		goto put_clocks;

	/*core_clk is required for LINK protocol engine,it should be at 60MHz */
	mehci->core_clk = clk_get(mehci->dev, "core_clk");
	if (IS_ERR(mehci->core_clk)) {
		dev_err(mehci->dev, "failed to get core_clk\n");
		ret = PTR_ERR(mehci->core_clk);
		return ret;
	}
	clk_set_rate(mehci->core_clk, 60000000);

	/* 60MHz alt_core_clk is for LINK to be used during PHY RESET  */
	mehci->alt_core_clk = clk_get(mehci->dev, "alt_core_clk");
	if (IS_ERR(mehci->alt_core_clk)) {
		dev_err(mehci->dev, "failed to core_clk\n");
		ret = PTR_ERR(mehci->alt_core_clk);
		goto put_core_clk;
	}
	clk_set_rate(mehci->alt_core_clk, 60000000);

	/* 480MHz phy_clk is required for HSIC PHY operation */
	mehci->phy_clk = clk_get(mehci->dev, "phy_clk");
	if (IS_ERR(mehci->phy_clk)) {
		dev_err(mehci->dev, "failed to get phy_clk\n");
		ret = PTR_ERR(mehci->phy_clk);
		goto put_alt_core_clk;
	}
	clk_set_rate(mehci->phy_clk, 480000000);

	/* 10MHz cal_clk is required for calibration of I/O pads */
	mehci->cal_clk = clk_get(mehci->dev, "cal_clk");
	if (IS_ERR(mehci->cal_clk)) {
		dev_err(mehci->dev, "failed to get cal_clk\n");
		ret = PTR_ERR(mehci->cal_clk);
		goto put_phy_clk;
	}
	clk_set_rate(mehci->cal_clk, 10000000);

	/* ahb_clk is required for data transfers */
	mehci->ahb_clk = clk_get(mehci->dev, "iface_clk");
	if (IS_ERR(mehci->ahb_clk)) {
		dev_err(mehci->dev, "failed to get iface_clk\n");
		ret = PTR_ERR(mehci->ahb_clk);
		goto put_cal_clk;
	}

	clk_enable(mehci->core_clk);
	clk_enable(mehci->phy_clk);
	clk_enable(mehci->cal_clk);
	clk_enable(mehci->ahb_clk);

	return 0;

put_clocks:
	clk_disable(mehci->core_clk);
	clk_disable(mehci->phy_clk);
	clk_disable(mehci->cal_clk);
	clk_disable(mehci->ahb_clk);
	clk_put(mehci->ahb_clk);
put_cal_clk:
	clk_put(mehci->cal_clk);
put_phy_clk:
	clk_put(mehci->phy_clk);
put_alt_core_clk:
	clk_put(mehci->alt_core_clk);
put_core_clk:
	clk_put(mehci->core_clk);

	return ret;
}
static irqreturn_t hsic_peripheral_status_change(int irq, void *dev_id)
{
	struct msm_hsic_hcd *mehci = dev_id;

	pr_debug("%s: mechi:%p dev_id:%p\n", __func__, mehci, dev_id);

	if (mehci)
		msm_hsic_config_gpios(mehci, 0);

	return IRQ_HANDLED;
}

static int __devinit ehci_hsic_msm_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct resource *res;
	struct msm_hsic_hcd *mehci;
	struct msm_hsic_host_platform_data *pdata;
	int ret;

	dev_dbg(&pdev->dev, "ehci_msm-hsic probe\n");

	hcd = usb_create_hcd(&msm_hsic_driver, &pdev->dev,
				dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		return  -ENOMEM;
	}

	hcd->irq = platform_get_irq(pdev, 0);
	if (hcd->irq < 0) {
		dev_err(&pdev->dev, "Unable to get IRQ resource\n");
		ret = hcd->irq;
		goto put_hcd;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get memory resource\n");
		ret = -ENODEV;
		goto put_hcd;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto put_hcd;
	}

	mehci = hcd_to_hsic(hcd);
	mehci->dev = &pdev->dev;

	res = platform_get_resource_byname(pdev,
			IORESOURCE_IRQ,
			"peripheral_status_irq");
	if (res)
		mehci->peripheral_status_irq = res->start;

	ret = msm_hsic_init_clocks(mehci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize clocks\n");
		ret = -ENODEV;
		goto unmap;
	}

	ret = msm_hsic_init_vddcx(mehci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize VDDCX\n");
		ret = -ENODEV;
		goto deinit_clocks;
	}

	pdata = mehci->dev->platform_data;
	if (pdata->hub_reset) {
		mehci->xo_handle = msm_xo_get(MSM_XO_TCXO_D1, "hsic");
		if (IS_ERR(mehci->xo_handle)) {
			pr_err(" %s not able to get the handle"
				"to vote for TCXO D1 buffer\n", __func__);
			ret = PTR_ERR(mehci->xo_handle);
			goto deinit_vddcx;
		}

		ret = msm_xo_mode_vote(mehci->xo_handle, MSM_XO_MODE_ON);
		if (ret) {
			pr_err("%s failed to vote for TCXO"
				"D1 buffer%d\n", __func__, ret);
			goto free_xo_handle;
		}
	}

	ret = msm_hsic_config_hub(mehci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize hsic hub");
		goto free_xo_handle;
	}

	ret = msm_hsic_reset(mehci);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize PHY\n");
		goto deinit_hub;
	}

	ret = usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
	if (ret) {
		dev_err(&pdev->dev, "unable to register HCD\n");
		goto unconfig_gpio;
	}

	device_init_wakeup(&pdev->dev, 1);
	wake_lock_init(&mehci->wlock, WAKE_LOCK_SUSPEND, dev_name(&pdev->dev));
	wake_lock(&mehci->wlock);

	if (mehci->peripheral_status_irq) {
		ret = request_threaded_irq(mehci->peripheral_status_irq,
			NULL, hsic_peripheral_status_change,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
						| IRQF_SHARED,
			"hsic_peripheral_status", mehci);
		if (ret)
			dev_err(&pdev->dev, "%s:request_irq:%d failed:%d",
				__func__, mehci->peripheral_status_irq, ret);
	}

	/*
	 * This pdev->dev is assigned parent of root-hub by USB core,
	 * hence, runtime framework automatically calls this driver's
	 * runtime APIs based on root-hub's state.
	 */
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

unconfig_gpio:
	msm_hsic_config_gpios(mehci, 0);
deinit_hub:
	msm_hsic_config_hub(mehci, 0);
free_xo_handle:
	if (pdata->hub_reset)
		msm_xo_put(mehci->xo_handle);
deinit_vddcx:
	msm_hsic_init_vddcx(mehci, 0);
deinit_clocks:
	msm_hsic_init_clocks(mehci, 0);
unmap:
	iounmap(hcd->regs);
put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static int __devexit ehci_hsic_msm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);
	struct msm_hsic_host_platform_data *pdata;

	if (mehci->peripheral_status_irq)
		free_irq(mehci->peripheral_status_irq, mehci);

	device_init_wakeup(&pdev->dev, 0);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	usb_remove_hcd(hcd);
	msm_hsic_config_gpios(mehci, 0);
	msm_hsic_config_hub(mehci, 0);
	pdata = mehci->dev->platform_data;
	if (pdata->hub_reset)
		msm_xo_put(mehci->xo_handle);
	msm_hsic_init_vddcx(mehci, 0);

	msm_hsic_init_clocks(mehci, 0);
	wake_lock_destroy(&mehci->wlock);
	iounmap(hcd->regs);
	usb_put_hcd(hcd);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int msm_hsic_pm_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	dev_dbg(dev, "ehci-msm-hsic PM suspend\n");

	if (device_may_wakeup(dev))
		enable_irq_wake(hcd->irq);

	return msm_hsic_suspend(mehci);

}

static int msm_hsic_pm_resume(struct device *dev)
{
	int ret;
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	dev_dbg(dev, "ehci-msm-hsic PM resume\n");

	if (device_may_wakeup(dev))
		disable_irq_wake(hcd->irq);

	ret = msm_hsic_resume(mehci);
	if (ret)
		return ret;

	/* Bring the device to full powered state upon system resume */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int msm_hsic_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "EHCI runtime idle\n");

	return 0;
}

static int msm_hsic_runtime_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	dev_dbg(dev, "EHCI runtime suspend\n");
	return msm_hsic_suspend(mehci);
}

static int msm_hsic_runtime_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	dev_dbg(dev, "EHCI runtime resume\n");
	return msm_hsic_resume(mehci);
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops msm_hsic_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msm_hsic_pm_suspend, msm_hsic_pm_resume)
	SET_RUNTIME_PM_OPS(msm_hsic_runtime_suspend, msm_hsic_runtime_resume,
				msm_hsic_runtime_idle)
};
#endif

static struct platform_driver ehci_msm_hsic_driver = {
	.probe	= ehci_hsic_msm_probe,
	.remove	= __devexit_p(ehci_hsic_msm_remove),
	.driver = {
		.name = "msm_hsic_host",
#ifdef CONFIG_PM
		.pm = &msm_hsic_dev_pm_ops,
#endif
	},
};

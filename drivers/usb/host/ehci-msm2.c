/* ehci-msm2.c - HSUSB Host Controller Driver Implementation
 *
 * Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/usb/ulpi.h>
#include <linux/usb/msm_hsusb_hw.h>
#include <linux/usb/msm_hsusb.h>
#include <mach/clk.h>
#include <mach/msm_xo.h>
#include <mach/msm_iomap.h>

#define MSM_USB_BASE (hcd->regs)

#define PDEV_NAME_LEN 20

struct msm_hcd {
	struct ehci_hcd				ehci;
	struct device				*dev;
	struct clk				*iface_clk;
	struct clk				*core_clk;
	struct clk				*alt_core_clk;
	struct regulator			*hsusb_vddcx;
	struct regulator			*hsusb_3p3;
	struct regulator			*hsusb_1p8;
	struct regulator			*vbus;
	struct msm_xo_voter			*xo_handle;
	bool					async_int;
	bool					vbus_on;
	atomic_t				in_lpm;
	int					pmic_gpio_dp_irq;
	bool					pmic_gpio_dp_irq_enabled;
	uint32_t				pmic_gpio_int_cnt;
	atomic_t				pm_usage_cnt;
	struct wake_lock			wlock;
	struct work_struct			phy_susp_fail_work;
};

static inline struct msm_hcd *hcd_to_mhcd(struct usb_hcd *hcd)
{
	return (struct msm_hcd *) (hcd->hcd_priv);
}

static inline struct usb_hcd *mhcd_to_hcd(struct msm_hcd *mhcd)
{
	return container_of((void *) mhcd, struct usb_hcd, hcd_priv);
}

#define HSUSB_PHY_3P3_VOL_MIN		3050000 /* uV */
#define HSUSB_PHY_3P3_VOL_MAX		3300000 /* uV */
#define HSUSB_PHY_3P3_HPM_LOAD		50000	/* uA */

#define HSUSB_PHY_1P8_VOL_MIN		1800000 /* uV */
#define HSUSB_PHY_1P8_VOL_MAX		1800000 /* uV */
#define HSUSB_PHY_1P8_HPM_LOAD		50000	/* uA */

#define HSUSB_PHY_VDD_DIG_VOL_MIN	1045000	/* uV */
#define HSUSB_PHY_VDD_DIG_VOL_MAX	1320000	/* uV */
#define HSUSB_PHY_VDD_DIG_LOAD		49360	/* uA */

static int msm_ehci_init_vddcx(struct msm_hcd *mhcd, int init)
{
	int ret = 0;

	if (!init)
		goto disable_reg;

	mhcd->hsusb_vddcx = devm_regulator_get(mhcd->dev, "HSUSB_VDDCX");
	if (IS_ERR(mhcd->hsusb_vddcx)) {
		dev_err(mhcd->dev, "unable to get ehci vddcx\n");
		return PTR_ERR(mhcd->hsusb_vddcx);
	}

	ret = regulator_set_voltage(mhcd->hsusb_vddcx,
			HSUSB_PHY_VDD_DIG_VOL_MIN,
			HSUSB_PHY_VDD_DIG_VOL_MAX);
	if (ret) {
		dev_err(mhcd->dev, "unable to set the voltage"
				"for ehci vddcx\n");
		return ret;
	}

	ret = regulator_set_optimum_mode(mhcd->hsusb_vddcx,
				HSUSB_PHY_VDD_DIG_LOAD);
	if (ret < 0) {
		dev_err(mhcd->dev, "%s: Unable to set optimum mode of the"
				" regulator: VDDCX\n", __func__);
		goto reg_optimum_mode_err;
	}

	ret = regulator_enable(mhcd->hsusb_vddcx);
	if (ret) {
		dev_err(mhcd->dev, "unable to enable ehci vddcx\n");
		goto reg_enable_err;
	}

	return 0;

disable_reg:
	regulator_disable(mhcd->hsusb_vddcx);
reg_enable_err:
	regulator_set_optimum_mode(mhcd->hsusb_vddcx, 0);
reg_optimum_mode_err:
	regulator_set_voltage(mhcd->hsusb_vddcx, 0,
				HSUSB_PHY_VDD_DIG_VOL_MIN);
	return ret;

}

static int msm_ehci_ldo_init(struct msm_hcd *mhcd, int init)
{
	int rc = 0;

	if (!init)
		goto put_1p8;

	mhcd->hsusb_3p3 = devm_regulator_get(mhcd->dev, "HSUSB_3p3");
	if (IS_ERR(mhcd->hsusb_3p3)) {
		dev_err(mhcd->dev, "unable to get hsusb 3p3\n");
		return PTR_ERR(mhcd->hsusb_3p3);
	}

	rc = regulator_set_voltage(mhcd->hsusb_3p3,
			HSUSB_PHY_3P3_VOL_MIN, HSUSB_PHY_3P3_VOL_MAX);
	if (rc) {
		dev_err(mhcd->dev, "unable to set voltage level for"
				"hsusb 3p3\n");
		return rc;
	}
	mhcd->hsusb_1p8 = devm_regulator_get(mhcd->dev, "HSUSB_1p8");
	if (IS_ERR(mhcd->hsusb_1p8)) {
		dev_err(mhcd->dev, "unable to get hsusb 1p8\n");
		rc = PTR_ERR(mhcd->hsusb_1p8);
		goto put_3p3_lpm;
	}
	rc = regulator_set_voltage(mhcd->hsusb_1p8,
			HSUSB_PHY_1P8_VOL_MIN, HSUSB_PHY_1P8_VOL_MAX);
	if (rc) {
		dev_err(mhcd->dev, "unable to set voltage level for"
				"hsusb 1p8\n");
		goto put_1p8;
	}

	return 0;

put_1p8:
	regulator_set_voltage(mhcd->hsusb_1p8, 0, HSUSB_PHY_1P8_VOL_MAX);
put_3p3_lpm:
	regulator_set_voltage(mhcd->hsusb_3p3, 0, HSUSB_PHY_3P3_VOL_MAX);

	return rc;
}

#ifdef CONFIG_PM_SLEEP
#define HSUSB_PHY_SUSP_DIG_VOL_P50  500000
#define HSUSB_PHY_SUSP_DIG_VOL_P75  750000
static int msm_ehci_config_vddcx(struct msm_hcd *mhcd, int high)
{
	struct msm_usb_host_platform_data *pdata;
	int max_vol = HSUSB_PHY_VDD_DIG_VOL_MAX;
	int min_vol;
	int ret;

	pdata = mhcd->dev->platform_data;

	if (high)
		min_vol = HSUSB_PHY_VDD_DIG_VOL_MIN;
	else if (pdata && pdata->dock_connect_irq &&
			!irq_read_line(pdata->dock_connect_irq))
		min_vol = HSUSB_PHY_SUSP_DIG_VOL_P75;
	else
		min_vol = HSUSB_PHY_SUSP_DIG_VOL_P50;

	ret = regulator_set_voltage(mhcd->hsusb_vddcx, min_vol, max_vol);
	if (ret) {
		dev_err(mhcd->dev, "%s: unable to set the voltage of regulator"
			" HSUSB_VDDCX\n", __func__);
		return ret;
	}

	dev_dbg(mhcd->dev, "%s: min_vol:%d max_vol:%d\n", __func__, min_vol,
								max_vol);

	return ret;
}
#else
static int msm_ehci_config_vddcx(struct msm_hcd *mhcd, int high)
{
	return 0;
}
#endif

static void msm_ehci_vbus_power(struct msm_hcd *mhcd, bool on)
{
	int ret;

	if (!mhcd->vbus) {
		pr_err("vbus is NULL.");
		return;
	}

	if (mhcd->vbus_on == on)
		return;

	if (on) {
		ret = regulator_enable(mhcd->vbus);
		if (ret) {
			pr_err("unable to enable vbus\n");
			return;
		}
		mhcd->vbus_on = true;
	} else {
		ret = regulator_disable(mhcd->vbus);
		if (ret) {
			pr_err("unable to disable vbus\n");
			return;
		}
		mhcd->vbus_on = false;
	}
}

static irqreturn_t msm_ehci_dock_connect_irq(int irq, void *data)
{
	const struct msm_usb_host_platform_data *pdata;
	struct msm_hcd *mhcd = data;
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);

	pdata = mhcd->dev->platform_data;

	if (atomic_read(&mhcd->in_lpm))
		usb_hcd_resume_root_hub(hcd);

	if (irq_read_line(pdata->dock_connect_irq)) {
		dev_dbg(mhcd->dev, "%s:Dock removed disable vbus\n", __func__);
		msm_ehci_vbus_power(mhcd, 0);
	} else {
		dev_dbg(mhcd->dev, "%s:Dock connected enable vbus\n", __func__);
		msm_ehci_vbus_power(mhcd, 1);
	}

	return IRQ_HANDLED;
}

static int msm_ehci_init_vbus(struct msm_hcd *mhcd, int init)
{
	int rc = 0;
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	const struct msm_usb_host_platform_data *pdata;

	pdata = mhcd->dev->platform_data;

	if (!init) {
		if (pdata && pdata->dock_connect_irq)
			free_irq(pdata->dock_connect_irq, mhcd);
		return rc;
	}

	mhcd->vbus = devm_regulator_get(mhcd->dev, "vbus");
	if (IS_ERR(mhcd->vbus)) {
		pr_err("Unable to get vbus\n");
		return -ENODEV;
	}

	if (pdata) {
		hcd->power_budget = pdata->power_budget;

		if (pdata->dock_connect_irq) {
			rc = request_threaded_irq(pdata->dock_connect_irq, NULL,
					msm_ehci_dock_connect_irq,
					IRQF_TRIGGER_FALLING |
					IRQF_TRIGGER_RISING |
					IRQF_ONESHOT, "msm_ehci_host", mhcd);
			if (!rc)
				enable_irq_wake(pdata->dock_connect_irq);
		}
	}
	return rc;
}

static int msm_ehci_ldo_enable(struct msm_hcd *mhcd, int on)
{
	int ret = 0;

	if (IS_ERR(mhcd->hsusb_1p8)) {
		dev_err(mhcd->dev, "%s: HSUSB_1p8 is not initialized\n",
								__func__);
		return -ENODEV;
	}

	if (IS_ERR(mhcd->hsusb_3p3)) {
		dev_err(mhcd->dev, "%s: HSUSB_3p3 is not initialized\n",
								__func__);
		return -ENODEV;
	}

	if (on) {
		ret = regulator_set_optimum_mode(mhcd->hsusb_1p8,
						HSUSB_PHY_1P8_HPM_LOAD);
		if (ret < 0) {
			dev_err(mhcd->dev, "%s: Unable to set HPM of the"
				" regulator: HSUSB_1p8\n", __func__);
			return ret;
		}

		ret = regulator_enable(mhcd->hsusb_1p8);
		if (ret) {
			dev_err(mhcd->dev, "%s: unable to enable the hsusb"
						" 1p8\n", __func__);
			regulator_set_optimum_mode(mhcd->hsusb_1p8, 0);
			return ret;
		}

		ret = regulator_set_optimum_mode(mhcd->hsusb_3p3,
						HSUSB_PHY_3P3_HPM_LOAD);
		if (ret < 0) {
			dev_err(mhcd->dev, "%s: Unable to set HPM of the "
				"regulator: HSUSB_3p3\n", __func__);
			regulator_set_optimum_mode(mhcd->hsusb_1p8, 0);
			regulator_disable(mhcd->hsusb_1p8);
			return ret;
		}

		ret = regulator_enable(mhcd->hsusb_3p3);
		if (ret) {
			dev_err(mhcd->dev, "%s: unable to enable the "
					"hsusb 3p3\n", __func__);
			regulator_set_optimum_mode(mhcd->hsusb_3p3, 0);
			regulator_set_optimum_mode(mhcd->hsusb_1p8, 0);
			regulator_disable(mhcd->hsusb_1p8);
			return ret;
		}

	} else {
		ret = regulator_disable(mhcd->hsusb_1p8);
		if (ret) {
			dev_err(mhcd->dev, "%s: unable to disable the "
					"hsusb 1p8\n", __func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(mhcd->hsusb_1p8, 0);
		if (ret < 0)
			dev_err(mhcd->dev, "%s: Unable to set LPM of the "
				"regulator: HSUSB_1p8\n", __func__);

		ret = regulator_disable(mhcd->hsusb_3p3);
		if (ret) {
			dev_err(mhcd->dev, "%s: unable to disable the "
					"hsusb 3p3\n", __func__);
			return ret;
		}
		ret = regulator_set_optimum_mode(mhcd->hsusb_3p3, 0);
		if (ret < 0)
			dev_err(mhcd->dev, "%s: Unable to set LPM of the "
					"regulator: HSUSB_3p3\n", __func__);
	}

	dev_dbg(mhcd->dev, "reg (%s)\n", on ? "HPM" : "LPM");

	return ret < 0 ? ret : 0;
}


#define ULPI_IO_TIMEOUT_USECS	(10 * 1000)
static int msm_ulpi_read(struct msm_hcd *mhcd, u32 reg)
{
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	unsigned long timeout;

	/* initiate read operation */
	writel_relaxed(ULPI_RUN | ULPI_READ | ULPI_ADDR(reg),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	timeout = jiffies + usecs_to_jiffies(ULPI_IO_TIMEOUT_USECS);
	while (readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_RUN) {
		if (time_after(jiffies, timeout)) {
			dev_err(mhcd->dev, "msm_ulpi_read: timeout %08x\n",
				readl_relaxed(USB_ULPI_VIEWPORT));
			return -ETIMEDOUT;
		}
		udelay(1);
	}

	return ULPI_DATA_READ(readl_relaxed(USB_ULPI_VIEWPORT));
}


static int msm_ulpi_write(struct msm_hcd *mhcd, u32 val, u32 reg)
{
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	unsigned long timeout;

	/* initiate write operation */
	writel_relaxed(ULPI_RUN | ULPI_WRITE |
	       ULPI_ADDR(reg) | ULPI_DATA(val),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	timeout = jiffies + usecs_to_jiffies(ULPI_IO_TIMEOUT_USECS);
	while (readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_RUN) {
		if (time_after(jiffies, timeout)) {
			dev_err(mhcd->dev, "msm_ulpi_write: timeout\n");
			return -ETIMEDOUT;
		}
		udelay(1);
	}

	return 0;
}

static int msm_ehci_link_clk_reset(struct msm_hcd *mhcd, bool assert)
{
	int ret;

	if (assert) {
		ret = clk_reset(mhcd->alt_core_clk, CLK_RESET_ASSERT);
		if (ret)
			dev_err(mhcd->dev, "usb alt_core_clk assert failed\n");
	} else {
		ret = clk_reset(mhcd->alt_core_clk, CLK_RESET_DEASSERT);
		if (ret)
			dev_err(mhcd->dev, "usb alt_core_clk deassert failed\n");
	}

	return ret;
}

static int msm_ehci_phy_reset(struct msm_hcd *mhcd)
{
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	u32 val;
	int ret;
	int retries;

	ret = msm_ehci_link_clk_reset(mhcd, 1);
	if (ret)
		return ret;

	udelay(1);

	ret = msm_ehci_link_clk_reset(mhcd, 0);
	if (ret)
		return ret;

	val = readl_relaxed(USB_PORTSC) & ~PORTSC_PTS_MASK;
	writel_relaxed(val | PORTSC_PTS_ULPI, USB_PORTSC);

	for (retries = 3; retries > 0; retries--) {
		ret = msm_ulpi_write(mhcd, ULPI_FUNC_CTRL_SUSPENDM,
				ULPI_CLR(ULPI_FUNC_CTRL));
		if (!ret)
			break;
	}
	if (!retries)
		return -ETIMEDOUT;

	/* Wakeup the PHY with a reg-access for calibration */
	for (retries = 3; retries > 0; retries--) {
		ret = msm_ulpi_read(mhcd, ULPI_DEBUG);
		if (ret != -ETIMEDOUT)
			break;
	}
	if (!retries)
		return -ETIMEDOUT;

	dev_info(mhcd->dev, "phy_reset: success\n");

	return 0;
}

#define LINK_RESET_TIMEOUT_USEC		(250 * 1000)
static int msm_hsusb_reset(struct msm_hcd *mhcd)
{
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	unsigned long timeout;
	int ret;

	clk_prepare_enable(mhcd->alt_core_clk);
	ret = msm_ehci_phy_reset(mhcd);
	if (ret) {
		dev_err(mhcd->dev, "phy_reset failed\n");
		return ret;
	}

	writel_relaxed(USBCMD_RESET, USB_USBCMD);

	timeout = jiffies + usecs_to_jiffies(LINK_RESET_TIMEOUT_USEC);
	while (readl_relaxed(USB_USBCMD) & USBCMD_RESET) {
		if (time_after(jiffies, timeout))
			return -ETIMEDOUT;
		udelay(1);
	}

	/* select ULPI phy */
	writel_relaxed(0x80000000, USB_PORTSC);

	msleep(100);

	writel_relaxed(0x0, USB_AHBBURST);
	writel_relaxed(0x08, USB_AHBMODE);

	/* Ensure that RESET operation is completed before turning off clock */
	mb();
	clk_disable_unprepare(mhcd->alt_core_clk);

	/*rising edge interrupts with Dp rise and fall enabled*/
	msm_ulpi_write(mhcd, ULPI_INT_DP, ULPI_USB_INT_EN_RISE);
	msm_ulpi_write(mhcd, ULPI_INT_DP, ULPI_USB_INT_EN_FALL);

	/*Clear the PHY interrupts by reading the PHY interrupt latch register*/
	msm_ulpi_read(mhcd, ULPI_USB_INT_LATCH);

	return 0;
}

static void msm_ehci_phy_susp_fail_work(struct work_struct *w)
{
	struct msm_hcd *mhcd = container_of(w, struct msm_hcd,
					phy_susp_fail_work);
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);

	msm_ehci_vbus_power(mhcd, 0);
	usb_remove_hcd(hcd);
	msm_hsusb_reset(mhcd);
	usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
	msm_ehci_vbus_power(mhcd, 1);
}

#define PHY_SUSPEND_TIMEOUT_USEC	(500 * 1000)
#define PHY_RESUME_TIMEOUT_USEC		(100 * 1000)

#ifdef CONFIG_PM_SLEEP
static int msm_ehci_suspend(struct msm_hcd *mhcd)
{
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	unsigned long timeout;
	int ret;
	u32 portsc;

	if (atomic_read(&mhcd->in_lpm)) {
		dev_dbg(mhcd->dev, "%s called in lpm\n", __func__);
		return 0;
	}

	disable_irq(hcd->irq);

	/* Set the PHCD bit, only if it is not set by the controller.
	 * PHY may take some time or even fail to enter into low power
	 * mode (LPM). Hence poll for 500 msec and reset the PHY and link
	 * in failure case.
	 */
	portsc = readl_relaxed(USB_PORTSC);
	if (!(portsc & PORTSC_PHCD)) {
		writel_relaxed(portsc | PORTSC_PHCD,
				USB_PORTSC);

		timeout = jiffies + usecs_to_jiffies(PHY_SUSPEND_TIMEOUT_USEC);
		while (!(readl_relaxed(USB_PORTSC) & PORTSC_PHCD)) {
			if (time_after(jiffies, timeout)) {
				dev_err(mhcd->dev, "Unable to suspend PHY\n");
				schedule_work(&mhcd->phy_susp_fail_work);
				return -ETIMEDOUT;
			}
			udelay(1);
		}
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

	clk_disable_unprepare(mhcd->iface_clk);
	clk_disable_unprepare(mhcd->core_clk);

	/* usb phy does not require TCXO clock, hence vote for TCXO disable */
	ret = msm_xo_mode_vote(mhcd->xo_handle, MSM_XO_MODE_OFF);
	if (ret)
		dev_err(mhcd->dev, "%s failed to devote for "
			"TCXO D0 buffer%d\n", __func__, ret);

	msm_ehci_config_vddcx(mhcd, 0);

	atomic_set(&mhcd->in_lpm, 1);
	enable_irq(hcd->irq);
	if (mhcd->pmic_gpio_dp_irq) {
		mhcd->pmic_gpio_dp_irq_enabled = 1;
		enable_irq_wake(mhcd->pmic_gpio_dp_irq);
		enable_irq(mhcd->pmic_gpio_dp_irq);
	}
	wake_unlock(&mhcd->wlock);

	dev_info(mhcd->dev, "EHCI USB in low power mode\n");

	return 0;
}

static int msm_ehci_resume(struct msm_hcd *mhcd)
{
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	unsigned long timeout;
	unsigned temp;
	int ret;

	if (!atomic_read(&mhcd->in_lpm)) {
		dev_dbg(mhcd->dev, "%s called in !in_lpm\n", __func__);
		return 0;
	}

	if (mhcd->pmic_gpio_dp_irq_enabled) {
		disable_irq_wake(mhcd->pmic_gpio_dp_irq);
		disable_irq_nosync(mhcd->pmic_gpio_dp_irq);
		mhcd->pmic_gpio_dp_irq_enabled = 0;
	}
	wake_lock(&mhcd->wlock);

	/* Vote for TCXO when waking up the phy */
	ret = msm_xo_mode_vote(mhcd->xo_handle, MSM_XO_MODE_ON);
	if (ret)
		dev_err(mhcd->dev, "%s failed to vote for "
			"TCXO D0 buffer%d\n", __func__, ret);

	clk_prepare_enable(mhcd->core_clk);
	clk_prepare_enable(mhcd->iface_clk);

	msm_ehci_config_vddcx(mhcd, 1);

	temp = readl_relaxed(USB_USBCMD);
	temp &= ~ASYNC_INTR_CTRL;
	temp &= ~ULPI_STP_CTRL;
	writel_relaxed(temp, USB_USBCMD);

	if (!(readl_relaxed(USB_PORTSC) & PORTSC_PHCD))
		goto skip_phy_resume;

	temp = readl_relaxed(USB_PORTSC) & ~PORTSC_PHCD;
	writel_relaxed(temp, USB_PORTSC);

	timeout = jiffies + usecs_to_jiffies(PHY_RESUME_TIMEOUT_USEC);
	while ((readl_relaxed(USB_PORTSC) & PORTSC_PHCD) ||
			!(readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_SYNC_STATE)) {
		if (time_after(jiffies, timeout)) {
			/*This is a fatal error. Reset the link and PHY*/
			dev_err(mhcd->dev, "Unable to resume USB. Resetting the h/w\n");
			msm_hsusb_reset(mhcd);
			break;
		}
		udelay(1);
	}

skip_phy_resume:

	usb_hcd_resume_root_hub(hcd);
	atomic_set(&mhcd->in_lpm, 0);

	if (mhcd->async_int) {
		mhcd->async_int = false;
		pm_runtime_put_noidle(mhcd->dev);
		enable_irq(hcd->irq);
	}

	if (atomic_read(&mhcd->pm_usage_cnt)) {
		atomic_set(&mhcd->pm_usage_cnt, 0);
		pm_runtime_put_noidle(mhcd->dev);
	}

	dev_info(mhcd->dev, "EHCI USB exited from low power mode\n");

	return 0;
}
#endif

static irqreturn_t msm_ehci_irq(struct usb_hcd *hcd)
{
	struct msm_hcd *mhcd = hcd_to_mhcd(hcd);

	if (atomic_read(&mhcd->in_lpm)) {
		disable_irq_nosync(hcd->irq);
		mhcd->async_int = true;
		pm_runtime_get(mhcd->dev);
		return IRQ_HANDLED;
	}

	return ehci_irq(hcd);
}

static irqreturn_t msm_ehci_host_wakeup_irq(int irq, void *data)
{

	struct msm_hcd *mhcd = data;

	mhcd->pmic_gpio_int_cnt++;
	dev_dbg(mhcd->dev, "%s: hsusb host remote wakeup interrupt cnt: %u\n",
			__func__, mhcd->pmic_gpio_int_cnt);


	wake_lock(&mhcd->wlock);

	if (mhcd->pmic_gpio_dp_irq_enabled) {
		mhcd->pmic_gpio_dp_irq_enabled = 0;
		disable_irq_wake(irq);
		disable_irq_nosync(irq);
	}

	if (!atomic_read(&mhcd->pm_usage_cnt)) {
		atomic_set(&mhcd->pm_usage_cnt, 1);
		pm_runtime_get(mhcd->dev);
	}

	return IRQ_HANDLED;
}

static int msm_ehci_reset(struct usb_hcd *hcd)
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
	writel_relaxed(0x08, USB_AHBMODE);
	/* Disable streaming mode and select host mode */
	writel_relaxed(0x13, USB_USBMODE);

	ehci_port_power(ehci, 1);
	return 0;
}

static struct hc_driver msm_hc2_driver = {
	.description		= hcd_name,
	.product_desc		= "Qualcomm EHCI Host Controller",
	.hcd_priv_size		= sizeof(struct msm_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq			= msm_ehci_irq,
	.flags			= HCD_USB2 | HCD_MEMORY,

	.reset			= msm_ehci_reset,
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

static int msm_ehci_init_clocks(struct msm_hcd *mhcd, u32 init)
{
	int ret = 0;

	if (!init)
		goto put_clocks;

	/* 60MHz alt_core_clk is for LINK to be used during PHY RESET  */
	mhcd->alt_core_clk = clk_get(mhcd->dev, "alt_core_clk");
	if (IS_ERR(mhcd->alt_core_clk)) {
		dev_err(mhcd->dev, "failed to get alt_core_clk\n");
		ret = PTR_ERR(mhcd->alt_core_clk);
		return ret;
	}
	clk_set_rate(mhcd->alt_core_clk, 60000000);

	/* iface_clk is required for data transfers */
	mhcd->iface_clk = clk_get(mhcd->dev, "iface_clk");
	if (IS_ERR(mhcd->iface_clk)) {
		dev_err(mhcd->dev, "failed to get iface_clk\n");
		ret = PTR_ERR(mhcd->iface_clk);
		goto put_alt_core_clk;
	}

	/* Link's protocol engine is based on pclk which must
	 * be running >55Mhz and frequency should also not change.
	 * Hence, vote for maximum clk frequency on its source
	 */
	mhcd->core_clk = clk_get(mhcd->dev, "core_clk");
	if (IS_ERR(mhcd->core_clk)) {
		dev_err(mhcd->dev, "failed to get core_clk\n");
		ret = PTR_ERR(mhcd->core_clk);
		goto put_iface_clk;
	}
	clk_set_rate(mhcd->core_clk, INT_MAX);

	clk_prepare_enable(mhcd->core_clk);
	clk_prepare_enable(mhcd->iface_clk);

	return 0;

put_clocks:
	clk_disable_unprepare(mhcd->iface_clk);
	clk_disable_unprepare(mhcd->core_clk);
	clk_put(mhcd->core_clk);
put_iface_clk:
	clk_put(mhcd->iface_clk);
put_alt_core_clk:
	clk_put(mhcd->alt_core_clk);

	return ret;
}

static int __devinit ehci_msm2_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct resource *res;
	struct msm_hcd *mhcd;
	const struct msm_usb_host_platform_data *pdata;
	char pdev_name[PDEV_NAME_LEN];
	int ret;

	dev_dbg(&pdev->dev, "ehci_msm2 probe\n");

	hcd = usb_create_hcd(&msm_hc2_driver, &pdev->dev,
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

	mhcd = hcd_to_mhcd(hcd);
	mhcd->dev = &pdev->dev;

	snprintf(pdev_name, PDEV_NAME_LEN, "%s.%d", pdev->name, pdev->id);
	mhcd->xo_handle = msm_xo_get(MSM_XO_TCXO_D0, pdev_name);
	if (IS_ERR(mhcd->xo_handle)) {
		dev_err(&pdev->dev, "%s not able to get the handle "
			"to vote for TCXO D0 buffer\n", __func__);
		ret = PTR_ERR(mhcd->xo_handle);
		goto unmap;
	}

	ret = msm_xo_mode_vote(mhcd->xo_handle, MSM_XO_MODE_ON);
	if (ret) {
		dev_err(&pdev->dev, "%s failed to vote for TCXO "
			"D0 buffer%d\n", __func__, ret);
		goto free_xo_handle;
	}

	ret = msm_ehci_init_clocks(mhcd, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize clocks\n");
		ret = -ENODEV;
		goto devote_xo_handle;
	}

	ret = msm_ehci_init_vddcx(mhcd, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize VDDCX\n");
		ret = -ENODEV;
		goto deinit_clocks;
	}

	ret = msm_ehci_config_vddcx(mhcd, 1);
	if (ret) {
		dev_err(&pdev->dev, "hsusb vddcx configuration failed\n");
		goto deinit_vddcx;
	}

	ret = msm_ehci_ldo_init(mhcd, 1);
	if (ret) {
		dev_err(&pdev->dev, "hsusb vreg configuration failed\n");
		goto deinit_vddcx;
	}

	ret = msm_ehci_ldo_enable(mhcd, 1);
	if (ret) {
		dev_err(&pdev->dev, "hsusb vreg enable failed\n");
		goto deinit_ldo;
	}

	ret = msm_ehci_init_vbus(mhcd, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to get vbus\n");
		goto disable_ldo;
	}

	ret = msm_hsusb_reset(mhcd);
	if (ret) {
		dev_err(&pdev->dev, "hsusb PHY initialization failed\n");
		goto vbus_deinit;
	}

	ret = usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
	if (ret) {
		dev_err(&pdev->dev, "unable to register HCD\n");
		goto vbus_deinit;
	}

	pdata = mhcd->dev->platform_data;
	if (pdata && (!pdata->dock_connect_irq ||
				!irq_read_line(pdata->dock_connect_irq)))
		msm_ehci_vbus_power(mhcd, 1);

	device_init_wakeup(&pdev->dev, 1);
	wake_lock_init(&mhcd->wlock, WAKE_LOCK_SUSPEND, dev_name(&pdev->dev));
	wake_lock(&mhcd->wlock);
	INIT_WORK(&mhcd->phy_susp_fail_work, msm_ehci_phy_susp_fail_work);
	/*
	 * This pdev->dev is assigned parent of root-hub by USB core,
	 * hence, runtime framework automatically calls this driver's
	 * runtime APIs based on root-hub's state.
	 */
	/* configure pmic_gpio_irq for D+ change */
	if (pdata && pdata->pmic_gpio_dp_irq)
		mhcd->pmic_gpio_dp_irq = pdata->pmic_gpio_dp_irq;
	if (mhcd->pmic_gpio_dp_irq) {
		ret = request_threaded_irq(mhcd->pmic_gpio_dp_irq, NULL,
				msm_ehci_host_wakeup_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"msm_ehci_host_wakeup", mhcd);
		if (!ret) {
			disable_irq_nosync(mhcd->pmic_gpio_dp_irq);
		} else {
			dev_err(&pdev->dev, "request_irq(%d) failed: %d\n",
					mhcd->pmic_gpio_dp_irq, ret);
			mhcd->pmic_gpio_dp_irq = 0;
		}
	}
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

vbus_deinit:
	msm_ehci_init_vbus(mhcd, 0);
disable_ldo:
	msm_ehci_ldo_enable(mhcd, 0);
deinit_ldo:
	msm_ehci_ldo_init(mhcd, 0);
deinit_vddcx:
	msm_ehci_init_vddcx(mhcd, 0);
deinit_clocks:
	msm_ehci_init_clocks(mhcd, 0);
devote_xo_handle:
	msm_xo_mode_vote(mhcd->xo_handle, MSM_XO_MODE_OFF);
free_xo_handle:
	msm_xo_put(mhcd->xo_handle);
unmap:
	iounmap(hcd->regs);
put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static int __devexit ehci_msm2_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct msm_hcd *mhcd = hcd_to_mhcd(hcd);

	if (mhcd->pmic_gpio_dp_irq) {
		if (mhcd->pmic_gpio_dp_irq_enabled)
			disable_irq_wake(mhcd->pmic_gpio_dp_irq);
		free_irq(mhcd->pmic_gpio_dp_irq, mhcd);
	}
	device_init_wakeup(&pdev->dev, 0);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	usb_remove_hcd(hcd);

	msm_xo_put(mhcd->xo_handle);
	msm_ehci_vbus_power(mhcd, 0);
	msm_ehci_init_vbus(mhcd, 0);
	msm_ehci_ldo_enable(mhcd, 0);
	msm_ehci_ldo_init(mhcd, 0);
	msm_ehci_init_vddcx(mhcd, 0);

	msm_ehci_init_clocks(mhcd, 0);
	wake_lock_destroy(&mhcd->wlock);
	iounmap(hcd->regs);
	usb_put_hcd(hcd);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ehci_msm2_pm_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hcd *mhcd = hcd_to_mhcd(hcd);

	dev_dbg(dev, "ehci-msm2 PM suspend\n");

	if (device_may_wakeup(dev))
		enable_irq_wake(hcd->irq);

	return msm_ehci_suspend(mhcd);

}

static int ehci_msm2_pm_resume(struct device *dev)
{
	int ret;
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hcd *mhcd = hcd_to_mhcd(hcd);

	dev_dbg(dev, "ehci-msm2 PM resume\n");

	if (device_may_wakeup(dev))
		disable_irq_wake(hcd->irq);

	ret = msm_ehci_resume(mhcd);
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
static int ehci_msm2_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "EHCI runtime idle\n");

	return 0;
}

static int ehci_msm2_runtime_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hcd *mhcd = hcd_to_mhcd(hcd);

	dev_dbg(dev, "EHCI runtime suspend\n");
	return msm_ehci_suspend(mhcd);
}

static int ehci_msm2_runtime_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hcd *mhcd = hcd_to_mhcd(hcd);

	dev_dbg(dev, "EHCI runtime resume\n");
	return msm_ehci_resume(mhcd);
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops ehci_msm2_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ehci_msm2_pm_suspend, ehci_msm2_pm_resume)
	SET_RUNTIME_PM_OPS(ehci_msm2_runtime_suspend, ehci_msm2_runtime_resume,
				ehci_msm2_runtime_idle)
};
#endif

static struct platform_driver ehci_msm2_driver = {
	.probe	= ehci_msm2_probe,
	.remove	= __devexit_p(ehci_msm2_remove),
	.driver = {
		.name = "msm_ehci_host",
#ifdef CONFIG_PM
		.pm = &ehci_msm2_dev_pm_ops,
#endif
	},
};

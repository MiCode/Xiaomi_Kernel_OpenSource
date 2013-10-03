/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/pm_wakeup.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <linux/workqueue.h>

#include <mach/msm_bus.h>
#include <mach/rpm-regulator.h>
#include <mach/clk.h>
#include <mach/msm_iomap.h>

#include "xhci.h"

#define MSM_HSIC_BASE			(hcd->regs)

#define MSM_HSIC_PORTSC			(MSM_HSIC_BASE + 0x0420)
#define MSM_HSIC_GCTL			(MSM_HSIC_BASE + 0xc110)
#define MSM_HSIC_GUSB2PHYCFG		(MSM_HSIC_BASE + 0xc200)
#define MSM_HSIC_GUSB2PHYACC		(MSM_HSIC_BASE + 0xc280)
#define MSM_HSIC_CTRL_REG		(MSM_HSIC_BASE + 0xf8800)
#define MSM_HSIC_PWR_EVENT_IRQ_STAT	(MSM_HSIC_BASE + 0xf8858)
#define MSM_HSIC_PWR_EVNT_IRQ_MASK	(MSM_HSIC_BASE + 0xf885c)

#define TLMM_GPIO_HSIC_STROBE_PAD_CTL	(MSM_TLMM_BASE + 0x2050)
#define TLMM_GPIO_HSIC_DATA_PAD_CTL	(MSM_TLMM_BASE + 0x2054)

#define GCTL_CORESOFTRESET	BIT(11)

/* Global USB2 PHY Configuration Register */
#define GUSB2PHYCFG_PHYSOFTRST	BIT(31)

/* Global USB2 PHY Vendor Control Register */
#define GUSB2PHYACC_NEWREGREQ	BIT(25)
#define GUSB2PHYACC_VSTSDONE	BIT(24)
#define GUSB2PHYACC_VSTSBUSY	BIT(23)
#define GUSB2PHYACC_REGWR	BIT(22)
#define GUSB2PHYACC_REGADDR(n)	(((n) & 0x3F) << 16)
#define GUSB2PHYACC_REGDATA(n)	((n) & 0xFF)

/* QSCRATCH ctrl reg */
#define CTRLREG_PLL_CTRL_SUSP	BIT(31)
#define CTRLREG_PLL_CTRL_SLEEP	BIT(30)

/* HSPHY registers*/
#define MSM_HSIC_CFG		0x30
#define MSM_HSIC_CFG_SET	0x31
#define MSM_HSIC_IO_CAL_PER	0x33

/* PWR_EVENT_IRQ_STAT reg */
#define LPM_IN_L2_IRQ_STAT	BIT(4)

/* PWR_EVENT_IRQ_MASK reg */
#define LPM_IN_L2_IRQ_MASK	BIT(4)

#define PHY_LPM_WAIT_TIMEOUT_MS	500
#define ULPI_IO_TIMEOUT_USECS	(10 * 1000)

static u64 dma_mask = DMA_BIT_MASK(64);

struct mxhci_hsic_hcd {
	struct xhci_hcd		*xhci;
	spinlock_t		wakeup_lock;
	struct device		*dev;

	struct clk		*core_clk;
	struct clk		*phy_sleep_clk;
	struct clk		*utmi_clk;
	struct clk		*hsic_clk;
	struct clk		*cal_clk;
	struct clk		*system_clk;

	struct regulator	*hsic_vddcx;
	struct regulator	*hsic_gdsc;

	struct wakeup_source	ws;

	u32			bus_perf_client;
	struct msm_bus_scale_pdata	*bus_scale_table;
	struct work_struct	bus_vote_w;
	bool			bus_vote;
	struct workqueue_struct	*wq;

	bool			wakeup_irq_enabled;
	int			strobe;
	int			data;
	int			wakeup_irq;
	int			pwr_event_irq;
	unsigned int		vdd_no_vol_level;
	unsigned int		vdd_low_vol_level;
	unsigned int		vdd_high_vol_level;
	unsigned int		in_lpm;
	unsigned int		pm_usage_cnt;
	struct completion	phy_in_lpm;

	uint32_t		wakeup_int_cnt;
};

#define SYNOPSIS_DWC3_VENDOR	0x5533

static inline struct mxhci_hsic_hcd *hcd_to_hsic(struct usb_hcd *hcd)
{
	return (struct mxhci_hsic_hcd *) (hcd->hcd_priv);
}

static inline struct usb_hcd *hsic_to_hcd(struct mxhci_hsic_hcd *mxhci)
{
	return container_of((void *) mxhci, struct usb_hcd, hcd_priv);
}

static void mxhci_hsic_bus_vote_w(struct work_struct *w)
{
	struct mxhci_hsic_hcd *mxhci =
			container_of(w, struct mxhci_hsic_hcd, bus_vote_w);
	int ret;

	ret = msm_bus_scale_client_update_request(mxhci->bus_perf_client,
			mxhci->bus_vote);
	if (ret)
		dev_err(mxhci->dev, "%s: Failed to vote for bus bandwidth %d\n",
				__func__, ret);
}

static int mxhci_hsic_init_clocks(struct mxhci_hsic_hcd *mxhci, u32 init)
{
	int ret = 0;

	if (!init)
		goto disable_all_clks;

	/* 75Mhz system_clk required for normal hsic operation */
	mxhci->system_clk = devm_clk_get(mxhci->dev, "system_clk");
	if (IS_ERR(mxhci->system_clk)) {
		dev_err(mxhci->dev, "failed to get system_clk\n");
		ret = PTR_ERR(mxhci->system_clk);
		goto out;
	}
	clk_set_rate(mxhci->system_clk, 75000000);

	/* 60Mhz core_clk required for LINK protocol engine */
	mxhci->core_clk = devm_clk_get(mxhci->dev, "core_clk");
	if (IS_ERR(mxhci->core_clk)) {
		dev_err(mxhci->dev, "failed to get core_clk\n");
		ret = PTR_ERR(mxhci->core_clk);
		goto out;
	}
	clk_set_rate(mxhci->core_clk, 60000000);

	/* 480Mhz main HSIC phy clk */
	mxhci->hsic_clk = devm_clk_get(mxhci->dev, "hsic_clk");
	if (IS_ERR(mxhci->hsic_clk)) {
		dev_err(mxhci->dev, "failed to get hsic_clk\n");
		ret = PTR_ERR(mxhci->hsic_clk);
		goto out;
	}
	clk_set_rate(mxhci->hsic_clk, 480000000);

	/* 19.2Mhz utmi_clk ref_clk required to shut off HSIC PLL */
	mxhci->utmi_clk = devm_clk_get(mxhci->dev, "utmi_clk");
	if (IS_ERR(mxhci->utmi_clk)) {
		dev_err(mxhci->dev, "failed to get utmi_clk\n");
		ret = PTR_ERR(mxhci->utmi_clk);
		goto out;
	}
	clk_set_rate(mxhci->utmi_clk, 19200000);

	/* 32Khz phy sleep clk */
	mxhci->phy_sleep_clk = devm_clk_get(mxhci->dev, "phy_sleep_clk");
	if (IS_ERR(mxhci->phy_sleep_clk)) {
		dev_err(mxhci->dev, "failed to get phy_sleep_clk\n");
		ret = PTR_ERR(mxhci->phy_sleep_clk);
		goto out;
	}
	clk_set_rate(mxhci->phy_sleep_clk, 32000);

	/* 10MHz cal_clk required for calibration of I/O pads */
	mxhci->cal_clk = devm_clk_get(mxhci->dev, "cal_clk");
	if (IS_ERR(mxhci->cal_clk)) {
		dev_err(mxhci->dev, "failed to get cal_clk\n");
		ret = PTR_ERR(mxhci->cal_clk);
		goto out;
	}
	clk_set_rate(mxhci->cal_clk, 9600000);

	ret = clk_prepare_enable(mxhci->system_clk);
	if (ret) {
		dev_err(mxhci->dev, "failed to enable system_clk\n");
		goto out;
	}

	ret = clk_prepare_enable(mxhci->core_clk);
	if (ret) {
		dev_err(mxhci->dev, "failed to enable core_clk\n");
		goto err_core_clk;
	}

	ret = clk_prepare_enable(mxhci->hsic_clk);
	if (ret) {
		dev_err(mxhci->dev, "failed to enable hsic_clk\n");
		goto err_hsic_clk;
	}

	ret = clk_prepare_enable(mxhci->utmi_clk);
	if (ret) {
		dev_err(mxhci->dev, "failed to enable utmi_clk\n");
		goto err_utmi_clk;
	}

	ret = clk_prepare_enable(mxhci->cal_clk);
	if (ret) {
		dev_err(mxhci->dev, "failed to enable cal_clk\n");
		goto err_cal_clk;
	}

	ret = clk_prepare_enable(mxhci->phy_sleep_clk);
	if (ret) {
		dev_err(mxhci->dev, "failed to enable phy_sleep_clk\n");
		goto err_phy_sleep_clk;
	}

	return 0;

disable_all_clks:
	clk_disable_unprepare(mxhci->phy_sleep_clk);
	if (mxhci->in_lpm)
		goto out;
err_phy_sleep_clk:
	clk_disable_unprepare(mxhci->cal_clk);
err_cal_clk:
	clk_disable_unprepare(mxhci->utmi_clk);
err_utmi_clk:
	clk_disable_unprepare(mxhci->hsic_clk);
err_hsic_clk:
	clk_disable_unprepare(mxhci->core_clk);
err_core_clk:
	clk_disable_unprepare(mxhci->system_clk);
out:
	return ret;
}

static int mxhci_hsic_init_vddcx(struct mxhci_hsic_hcd *mxhci, int init)
{
	int ret = 0;

	if (!init)
		goto disable_reg;

	if (!mxhci->hsic_vddcx) {
		mxhci->hsic_vddcx = devm_regulator_get(mxhci->dev,
			"hsic-vdd-dig");
		if (IS_ERR(mxhci->hsic_vddcx)) {
			dev_err(mxhci->dev, "unable to get hsic vddcx\n");
			ret = PTR_ERR(mxhci->hsic_vddcx);
			goto out;
		}
	}

	ret = regulator_set_voltage(mxhci->hsic_vddcx, mxhci->vdd_low_vol_level,
			mxhci->vdd_high_vol_level);
	if (ret) {
		dev_err(mxhci->dev,
				"unable to set the voltage for hsic vddcx\n");
		goto out;
	}

	ret = regulator_enable(mxhci->hsic_vddcx);
	if (ret) {
		dev_err(mxhci->dev, "unable to enable hsic vddcx\n");
		goto reg_enable_err;
	}

	return 0;

disable_reg:
	regulator_disable(mxhci->hsic_vddcx);
reg_enable_err:
	regulator_set_voltage(mxhci->hsic_vddcx, mxhci->vdd_no_vol_level,
			mxhci->vdd_high_vol_level);

out:
	return ret;
}

/*
 * Config Global Distributed Switch Controller (GDSC)
 * to turn on/off HSIC controller
 */
static int mxhci_msm_config_gdsc(struct mxhci_hsic_hcd *mxhci, int on)
{
	int ret = 0;

	if (!mxhci->hsic_gdsc) {
		mxhci->hsic_gdsc = devm_regulator_get(mxhci->dev, "hsic-gdsc");
			if (IS_ERR(mxhci->hsic_gdsc))
				return PTR_ERR(mxhci->hsic_gdsc);
	}

	if (on) {
		ret = regulator_enable(mxhci->hsic_gdsc);
		if (ret) {
			dev_err(mxhci->dev, "unable to enable hsic gdsc\n");
			return ret;
		}
	} else {
		regulator_disable(mxhci->hsic_gdsc);
	}

	return 0;
}

static int mxhci_hsic_config_gpios(struct mxhci_hsic_hcd *mxhci)
{
	int rc = 0;

	rc = devm_gpio_request(mxhci->dev, mxhci->strobe, "HSIC_STROBE_GPIO");
	if (rc < 0) {
		dev_err(mxhci->dev, "gpio request failed for HSIC STROBE\n");
		goto out;
	}

	rc = devm_gpio_request(mxhci->dev, mxhci->data, "HSIC_DATA_GPIO");
	if (rc < 0) {
		dev_err(mxhci->dev, "gpio request failed for HSIC DATA\n");
		goto out;
	}

out:
	return rc;
}

static int mxhci_hsic_ulpi_write(struct mxhci_hsic_hcd *mxhci, u32 val,
		u32 reg)
{
	struct usb_hcd *hcd = hsic_to_hcd(mxhci);
	unsigned long timeout;

	/* set the reg write request and perfom ULPI phy reg write */
	writel_relaxed(GUSB2PHYACC_NEWREGREQ | GUSB2PHYACC_REGWR
		| GUSB2PHYACC_REGADDR(reg) | GUSB2PHYACC_REGDATA(val),
		MSM_HSIC_GUSB2PHYACC);

	/* poll for write done */
	timeout = jiffies + usecs_to_jiffies(ULPI_IO_TIMEOUT_USECS);
	while (!(readl_relaxed(MSM_HSIC_GUSB2PHYACC) & GUSB2PHYACC_VSTSDONE)) {
		if (time_after(jiffies, timeout)) {
			dev_err(mxhci->dev, "mxhci_hsic_ulpi_write: timeout\n");
			return -ETIMEDOUT;
		}
		udelay(1);
	}

	return 0;
}

static void mxhci_hsic_reset(struct mxhci_hsic_hcd *mxhci)
{
	u32 reg;
	int ret;
	struct usb_hcd *hcd = hsic_to_hcd(mxhci);

	/* start controller reset */
	reg = readl_relaxed(MSM_HSIC_GCTL);
	reg |= GCTL_CORESOFTRESET;
	writel_relaxed(reg, MSM_HSIC_GCTL);

	usleep(1000);

	/* phy reset using asynchronous block reset */

	clk_disable_unprepare(mxhci->cal_clk);
	clk_disable_unprepare(mxhci->utmi_clk);
	clk_disable_unprepare(mxhci->hsic_clk);
	clk_disable_unprepare(mxhci->core_clk);
	clk_disable_unprepare(mxhci->system_clk);
	clk_disable_unprepare(mxhci->phy_sleep_clk);

	ret = clk_reset(mxhci->hsic_clk, CLK_RESET_ASSERT);
	if (ret) {
		dev_err(mxhci->dev, "hsic clk assert failed:%d\n", ret);
		return;
	}
	usleep_range(10000, 12000);

	ret = clk_reset(mxhci->hsic_clk, CLK_RESET_DEASSERT);
	if (ret)
		dev_err(mxhci->dev, "hsic clk deassert failed:%d\n",
				ret);
	/*
	 * Required delay between the deassertion and
	 *	clock enablement.
	*/
	ndelay(200);
	clk_prepare_enable(mxhci->phy_sleep_clk);
	clk_prepare_enable(mxhci->system_clk);
	clk_prepare_enable(mxhci->core_clk);
	clk_prepare_enable(mxhci->hsic_clk);
	clk_prepare_enable(mxhci->utmi_clk);
	clk_prepare_enable(mxhci->cal_clk);

	/* After PHY is stable we can take Core out of reset state */
	reg = readl_relaxed(MSM_HSIC_GCTL);
	reg &= ~GCTL_CORESOFTRESET;
	writel_relaxed(reg, MSM_HSIC_GCTL);

	usleep(1000);
}

static void mxhci_hsic_plat_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	struct xhci_plat_data *pdata = dev->platform_data;
	struct mxhci_hsic_hcd *mxhci = hcd_to_hsic(xhci_to_hcd(xhci));

	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_BROKEN_MSI;

	/* Single port controller using out of band remote wakeup */
	if (mxhci->wakeup_irq)
		xhci->quirks |= XHCI_NO_SELECTIVE_SUSPEND;

	if (!pdata)
		return;
	if (pdata->vendor == SYNOPSIS_DWC3_VENDOR &&
			pdata->revision < 0x230A)
		xhci->quirks |= XHCI_PORTSC_DELAY;
}

/* called during probe() after chip reset completes */
static int mxhci_hsic_plat_setup(struct usb_hcd *hcd)
{
	return xhci_gen_setup(hcd, mxhci_hsic_plat_quirks);
}

static irqreturn_t mxhci_hsic_wakeup_irq(int irq, void *data)
{
	struct mxhci_hsic_hcd *mxhci = data;
	int ret;

	mxhci->wakeup_int_cnt++;
	dev_dbg(mxhci->dev, "%s: remote wakeup interrupt cnt: %u\n",
			__func__, mxhci->wakeup_int_cnt);

	pm_stay_awake(mxhci->dev);

	spin_lock(&mxhci->wakeup_lock);
	if (mxhci->wakeup_irq_enabled) {
		mxhci->wakeup_irq_enabled = 0;
		disable_irq_wake(irq);
		disable_irq_nosync(irq);
	}

	if (!mxhci->pm_usage_cnt) {
		ret = pm_runtime_get(mxhci->dev);
		/*
		 * HSIC runtime resume can race with us.
		 * if we are active (ret == 1) or resuming
		 * (ret == -EINPROGRESS), decrement the
		 * PM usage counter before returning.
		 */
		if ((ret == 1) || (ret == -EINPROGRESS))
			pm_runtime_put_noidle(mxhci->dev);
		else
			mxhci->pm_usage_cnt = 1;
	}
	spin_unlock(&mxhci->wakeup_lock);

	return IRQ_HANDLED;
}

static irqreturn_t mxhci_hsic_pwr_event_irq(int irq, void *data)
{
	struct mxhci_hsic_hcd *mxhci = data;
	struct usb_hcd *hcd = hsic_to_hcd(mxhci);
	u32 stat;

	stat = readl_relaxed(MSM_HSIC_PWR_EVENT_IRQ_STAT);
	if (stat & LPM_IN_L2_IRQ_STAT) {
		writel_relaxed(stat, MSM_HSIC_PWR_EVENT_IRQ_STAT);
		complete(&mxhci->phy_in_lpm);
	} else {
		dev_info(mxhci->dev,
			"%s: spurious interrupt.pwr_event_irq stat = %x\n",
			__func__, stat);
	}

	return IRQ_HANDLED;
}

static int mxhci_hsic_bus_suspend(struct usb_hcd *hcd)
{
	struct mxhci_hsic_hcd *mxhci = hcd_to_hsic(hcd->primary_hcd);

	/* don't miss connect bus state from peripheral for USB 2.0 root hub */
	if (usb_hcd_is_primary_hcd(hcd) &&
			!(readl_relaxed(MSM_HSIC_PORTSC) & PORT_PE)) {
		dev_dbg(mxhci->dev, "%s:port is not enabled skip suspend\n",
				__func__);
		return -EAGAIN;
	}

	return xhci_bus_suspend(hcd);
}

static int mxhci_hsic_suspend(struct mxhci_hsic_hcd *mxhci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mxhci);
	int ret;

	if (mxhci->in_lpm) {
		dev_dbg(mxhci->dev, "%s called in lpm\n", __func__);
		return 0;
	}

	disable_irq(hcd->irq);

	/* make sure we don't race against a remote wakeup */
	if (test_bit(HCD_FLAG_WAKEUP_PENDING, &hcd->flags) ||
	    (readl_relaxed(MSM_HSIC_PORTSC) & PORT_PLS_MASK) == XDEV_RESUME) {
		dev_dbg(mxhci->dev, "wakeup pending, aborting suspend\n");
		enable_irq(hcd->irq);
		return -EBUSY;
	}

	/* make sure HSIC phy is in LPM */
	ret = wait_for_completion_timeout(
			&mxhci->phy_in_lpm,
			msecs_to_jiffies(PHY_LPM_WAIT_TIMEOUT_MS));
	if (!ret) {
		dev_err(mxhci->dev, "HSIC phy failed to enter lpm\n");
		init_completion(&mxhci->phy_in_lpm);
		enable_irq(hcd->irq);
		return -EBUSY;
	}

	init_completion(&mxhci->phy_in_lpm);

	clk_disable_unprepare(mxhci->system_clk);
	clk_disable_unprepare(mxhci->core_clk);
	clk_disable_unprepare(mxhci->utmi_clk);
	clk_disable_unprepare(mxhci->hsic_clk);
	clk_disable_unprepare(mxhci->cal_clk);

	ret = regulator_set_voltage(mxhci->hsic_vddcx, mxhci->vdd_no_vol_level,
			mxhci->vdd_high_vol_level);
	if (ret < 0)
		dev_err(mxhci->dev, "unable to set vddcx voltage for VDD MIN\n");

	if (mxhci->bus_perf_client) {
		mxhci->bus_vote = false;
		queue_work(mxhci->wq, &mxhci->bus_vote_w);
	}

	mxhci->in_lpm = 1;

	enable_irq(hcd->irq);

	if (mxhci->wakeup_irq) {
		mxhci->wakeup_irq_enabled = 1;
		enable_irq_wake(mxhci->wakeup_irq);
		enable_irq(mxhci->wakeup_irq);
	}

	pm_relax(mxhci->dev);

	dev_info(mxhci->dev, "HSIC-USB in low power mode\n");

	return 0;
}

static int mxhci_hsic_resume(struct mxhci_hsic_hcd *mxhci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mxhci);
	int ret;
	unsigned long flags;

	if (!mxhci->in_lpm) {
		dev_dbg(mxhci->dev, "%s called in !in_lpm\n", __func__);
		return 0;
	}

	pm_stay_awake(mxhci->dev);

	if (mxhci->bus_perf_client) {
		mxhci->bus_vote = true;
		queue_work(mxhci->wq, &mxhci->bus_vote_w);
	}

	spin_lock_irqsave(&mxhci->wakeup_lock, flags);
	if (mxhci->wakeup_irq_enabled) {
		disable_irq_wake(mxhci->wakeup_irq);
		disable_irq_nosync(mxhci->wakeup_irq);
		mxhci->wakeup_irq_enabled = 0;
	}

	if (mxhci->pm_usage_cnt) {
		mxhci->pm_usage_cnt = 0;
		pm_runtime_put_noidle(mxhci->dev);
	}
	spin_unlock_irqrestore(&mxhci->wakeup_lock, flags);


	ret = regulator_set_voltage(mxhci->hsic_vddcx, mxhci->vdd_low_vol_level,
			mxhci->vdd_high_vol_level);
	if (ret < 0)
		dev_err(mxhci->dev,
			"unable to set nominal vddcx voltage (no VDD MIN)\n");

	clk_prepare_enable(mxhci->system_clk);
	clk_prepare_enable(mxhci->core_clk);
	clk_prepare_enable(mxhci->utmi_clk);
	clk_prepare_enable(mxhci->hsic_clk);
	clk_prepare_enable(mxhci->cal_clk);

	if (mxhci->wakeup_irq)
		usb_hcd_resume_root_hub(hcd);

	mxhci->in_lpm = 0;

	dev_info(mxhci->dev, "HSIC-USB exited from low power mode\n");

	return 0;
}


static struct hc_driver mxhci_hsic_hc_driver = {
	.description =		"xhci-hcd",
	.product_desc =		"Qualcomm xHCI Host Controller using HSIC",

	/*
	 * generic hardware linkage
	 */
	.irq =			xhci_irq,
	.flags =		HCD_MEMORY | HCD_USB3,

	/*
	 * basic lifecycle operations
	 */
	.reset =		mxhci_hsic_plat_setup,
	.start =		xhci_run,
	.stop =			xhci_stop,
	.shutdown =		xhci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		xhci_urb_enqueue,
	.urb_dequeue =		xhci_urb_dequeue,
	.alloc_dev =		xhci_alloc_dev,
	.free_dev =		xhci_free_dev,
	.alloc_streams =	xhci_alloc_streams,
	.free_streams =		xhci_free_streams,
	.add_endpoint =		xhci_add_endpoint,
	.drop_endpoint =	xhci_drop_endpoint,
	.endpoint_reset =	xhci_endpoint_reset,
	.check_bandwidth =	xhci_check_bandwidth,
	.reset_bandwidth =	xhci_reset_bandwidth,
	.address_device =	xhci_address_device,
	.update_hub_device =	xhci_update_hub_device,
	.reset_device =		xhci_discover_or_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =	xhci_get_frame,

	/* Root hub support */
	.hub_control =		xhci_hub_control,
	.hub_status_data =	xhci_hub_status_data,
	.bus_suspend =		mxhci_hsic_bus_suspend,
	.bus_resume =		xhci_bus_resume,
};

static int mxhci_hsic_probe(struct platform_device *pdev)
{
	struct hc_driver *driver;
	struct device_node *node = pdev->dev.of_node;
	struct mxhci_hsic_hcd *mxhci;
	struct xhci_hcd		*xhci;
	struct resource *res;
	struct usb_hcd *hcd;
	unsigned int reg;
	int ret;
	int irq;
	u32 tmp[3];

	if (usb_disabled())
		return -ENODEV;

	driver = &mxhci_hsic_hc_driver;

	pdev->dev.dma_mask = &dma_mask;

	/* usb2.0 root hub */
	driver->hcd_priv_size =	sizeof(struct mxhci_hsic_hcd);
	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = -ENODEV;
		goto put_hcd;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto put_hcd;
	}

	hcd_to_bus(hcd)->skip_resume = true;
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	hcd->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "error mapping memory\n");
		ret = -EFAULT;
		goto put_hcd;
	}

	mxhci = hcd_to_hsic(hcd);
	mxhci->dev = &pdev->dev;

	mxhci->strobe = of_get_named_gpio(node, "hsic,strobe-gpio", 0);
	if (mxhci->strobe < 0) {
		ret = -EINVAL;
		goto put_hcd;
	}

	mxhci->data  = of_get_named_gpio(node, "hsic,data-gpio", 0);
	if (mxhci->data < 0) {
		ret = -EINVAL;
		goto put_hcd;
	}

	ret = of_property_read_u32_array(node, "qcom,vdd-voltage-level",
							tmp, ARRAY_SIZE(tmp));
	if (!ret) {
		mxhci->vdd_no_vol_level = tmp[0];
		mxhci->vdd_low_vol_level = tmp[1];
		mxhci->vdd_high_vol_level = tmp[2];
	} else {
		dev_err(&pdev->dev,
			"failed to read qcom,vdd-voltage-level property\n");
		ret = -EINVAL;
		goto put_hcd;
	}

	ret = mxhci_msm_config_gdsc(mxhci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to configure hsic gdsc\n");
		goto put_hcd;
	}

	ret = mxhci_hsic_init_clocks(mxhci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize clocks\n");
		goto put_hcd;
	}

	ret = mxhci_hsic_init_vddcx(mxhci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize vddcx\n");
		goto deinit_clocks;
	}

	mxhci_hsic_reset(mxhci);

	/* HSIC phy caliberation:set periodic caliberation interval ~2.048sec */
	mxhci_hsic_ulpi_write(mxhci, 0xFF, MSM_HSIC_IO_CAL_PER);

	/* Enable periodic IO calibration in HSIC_CFG register */
	mxhci_hsic_ulpi_write(mxhci, 0xA8, MSM_HSIC_CFG);

	/* Configure Strobe and Data GPIOs to enable HSIC */
	ret = mxhci_hsic_config_gpios(mxhci);
	if (ret) {
		dev_err(mxhci->dev, " gpio configuarion failed\n");
		goto deinit_vddcx;
	}

	/* enable STROBE_PAD_CTL */
	reg = readl_relaxed(TLMM_GPIO_HSIC_STROBE_PAD_CTL);
	writel_relaxed(reg | 0x2000000, TLMM_GPIO_HSIC_STROBE_PAD_CTL);

	/* enable DATA_PAD_CTL */
	reg = readl_relaxed(TLMM_GPIO_HSIC_DATA_PAD_CTL);
	writel_relaxed(reg | 0x2000000, TLMM_GPIO_HSIC_DATA_PAD_CTL);

	mb();

	/* Enable LPM in Sleep mode and suspend mode */
	reg = readl_relaxed(MSM_HSIC_CTRL_REG);
	reg |= CTRLREG_PLL_CTRL_SLEEP | CTRLREG_PLL_CTRL_SUSP;
	writel_relaxed(reg, MSM_HSIC_CTRL_REG);

	/* enable pwr event irq for LPM_IN_L2_IRQ */
	writel_relaxed(LPM_IN_L2_IRQ_MASK, MSM_HSIC_PWR_EVNT_IRQ_MASK);

	mxhci->wakeup_irq = platform_get_irq_byname(pdev, "wakeup_irq");
	if (mxhci->wakeup_irq < 0) {
		mxhci->wakeup_irq = 0;
		dev_err(&pdev->dev, "failed to init wakeup_irq\n");
	} else {
		/* enable wakeup irq only when entering lpm */
		irq_set_status_flags(mxhci->wakeup_irq, IRQ_NOAUTOEN);
		ret = devm_request_irq(&pdev->dev, mxhci->wakeup_irq,
			mxhci_hsic_wakeup_irq, 0, "mxhci_hsic_wakeup", mxhci);
		if (ret) {
			dev_err(&pdev->dev,
					"request irq failed (wakeup irq)\n");
			goto deinit_vddcx;
		}
	}

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto deinit_vddcx;

	hcd = dev_get_drvdata(&pdev->dev);
	xhci = hcd_to_xhci(hcd);

	/* USB 3.0 roothub */

	/* no need for another instance of mxhci */
	driver->hcd_priv_size = sizeof(struct xhci_hcd *);

	xhci->shared_hcd = usb_create_shared_hcd(driver, &pdev->dev,
			dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto remove_usb2_hcd;
	}

	hcd_to_bus(xhci->shared_hcd)->skip_resume = true;
	/*
	 * Set the xHCI pointer before xhci_plat_setup() (aka hcd_driver.reset)
	 * is called by usb_add_hcd().
	 */
	*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv) = xhci;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto put_usb3_hcd;

	spin_lock_init(&mxhci->wakeup_lock);

	mxhci->pwr_event_irq = platform_get_irq_byname(pdev, "pwr_event_irq");
	if (mxhci->pwr_event_irq < 0) {
		dev_err(&pdev->dev,
				"platform_get_irq for pwr_event_irq failed\n");
		goto remove_usb3_hcd;
	}

	ret = devm_request_irq(&pdev->dev, mxhci->pwr_event_irq,
				mxhci_hsic_pwr_event_irq,
				0, "mxhci_hsic_pwr_evt", mxhci);
	if (ret) {
		dev_err(&pdev->dev, "request irq failed (pwr event irq)\n");
		goto remove_usb3_hcd;
	}

	init_completion(&mxhci->phy_in_lpm);

	mxhci->wq = create_singlethread_workqueue("mxhci_wq");
	if (!mxhci->wq) {
		dev_err(&pdev->dev, "unable to create workqueue\n");
		ret = -ENOMEM;
		goto remove_usb3_hcd;
	}

	INIT_WORK(&mxhci->bus_vote_w, mxhci_hsic_bus_vote_w);

	mxhci->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!mxhci->bus_scale_table) {
		dev_dbg(&pdev->dev, "bus scaling is disabled\n");
	} else {
		mxhci->bus_perf_client =
			msm_bus_scale_register_client(mxhci->bus_scale_table);
		/* Configure BUS performance parameters for MAX bandwidth */
		if (mxhci->bus_perf_client) {
			mxhci->bus_vote = true;
			queue_work(mxhci->wq, &mxhci->bus_vote_w);
		} else {
			dev_err(&pdev->dev, "%s: bus scaling client reg err\n",
					__func__);
			ret = -ENODEV;
			goto delete_wq;
		}
	}

	/* Enable HSIC PHY */
	mxhci_hsic_ulpi_write(mxhci, 0x01, MSM_HSIC_CFG_SET);

	device_init_wakeup(&pdev->dev, 1);
	wakeup_source_init(&mxhci->ws, dev_name(&pdev->dev));
	pm_stay_awake(mxhci->dev);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

delete_wq:
	destroy_workqueue(mxhci->wq);
remove_usb3_hcd:
	usb_remove_hcd(xhci->shared_hcd);
put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);
remove_usb2_hcd:
	usb_remove_hcd(hcd);
deinit_vddcx:
	mxhci_hsic_init_vddcx(mxhci, 0);
deinit_clocks:
	mxhci_hsic_init_clocks(mxhci, 0);
put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static int mxhci_hsic_remove(struct platform_device *pdev)
{
	struct usb_hcd	*hcd = platform_get_drvdata(pdev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct mxhci_hsic_hcd *mxhci = hcd_to_hsic(hcd);
	u32 reg;

	/* disable STROBE_PAD_CTL */
	reg = readl_relaxed(TLMM_GPIO_HSIC_STROBE_PAD_CTL);
	writel_relaxed(reg & 0xfdffffff, TLMM_GPIO_HSIC_STROBE_PAD_CTL);

	/* disable DATA_PAD_CTL */
	reg = readl_relaxed(TLMM_GPIO_HSIC_DATA_PAD_CTL);
	writel_relaxed(reg & 0xfdffffff, TLMM_GPIO_HSIC_DATA_PAD_CTL);

	mb();

	/* If the device was removed no need to call pm_runtime_disable */
	if (pdev->dev.power.power_state.event != PM_EVENT_INVALID)
		pm_runtime_disable(&pdev->dev);

	pm_runtime_set_suspended(&pdev->dev);

	usb_remove_hcd(xhci->shared_hcd);
	usb_put_hcd(xhci->shared_hcd);

	usb_remove_hcd(hcd);

	if (mxhci->wakeup_irq_enabled)
		disable_irq_wake(mxhci->wakeup_irq);

	mxhci->bus_vote = false;
	cancel_work_sync(&mxhci->bus_vote_w);

	if (mxhci->bus_perf_client)
		msm_bus_scale_unregister_client(mxhci->bus_perf_client);

	destroy_workqueue(mxhci->wq);

	device_init_wakeup(&pdev->dev, 0);
	mxhci_hsic_init_vddcx(mxhci, 0);
	mxhci_hsic_init_clocks(mxhci, 0);
	mxhci_msm_config_gdsc(mxhci, 0);
	wakeup_source_trash(&mxhci->ws);
	usb_put_hcd(hcd);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int mxhci_hsic_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "xhci msm runtime idle\n");
	return 0;
}

static int mxhci_hsic_runtime_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct mxhci_hsic_hcd *mxhci = hcd_to_hsic(hcd);

	dev_dbg(dev, "xhci msm runtime suspend\n");

	return mxhci_hsic_suspend(mxhci);
}

static int mxhci_hsic_runtime_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct mxhci_hsic_hcd *mxhci = hcd_to_hsic(hcd);

	dev_dbg(dev, "xhci msm runtime resume\n");

	return mxhci_hsic_resume(mxhci);
}
#endif

#ifdef CONFIG_PM_SLEEP
static int mxhci_hsic_pm_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct mxhci_hsic_hcd *mxhci = hcd_to_hsic(hcd);

	dev_dbg(dev, "xhci-msm PM suspend\n");

	if (!mxhci->in_lpm) {
		dev_dbg(dev, "abort suspend\n");
		return -EBUSY;
	}

	if (device_may_wakeup(dev))
		enable_irq_wake(hcd->irq);

	return 0;
}

static int mxhci_hsic_pm_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct mxhci_hsic_hcd *mxhci = hcd_to_hsic(hcd);
	unsigned long flags;
	int ret;

	dev_dbg(dev, "xhci-msm PM resume\n");

	if (device_may_wakeup(dev))
		disable_irq_wake(hcd->irq);

	/*
	 * Keep HSIC in Low Power Mode if system is resumed
	 * by any other wakeup source.  HSIC is resumed later
	 * when remote wakeup is received or interface driver
	 * start I/O.
	 */
	spin_lock_irqsave(&mxhci->wakeup_lock, flags);
	if (!mxhci->pm_usage_cnt &&
			pm_runtime_suspended(dev)) {
		spin_unlock_irqrestore(&mxhci->wakeup_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&mxhci->wakeup_lock, flags);

	ret = mxhci_hsic_resume(mxhci);
	if (ret)
		return ret;

	/* Bring the device to full powered state upon system resume */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}
#endif

static const struct dev_pm_ops xhci_msm_hsic_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mxhci_hsic_pm_suspend, mxhci_hsic_pm_resume)
	SET_RUNTIME_PM_OPS(mxhci_hsic_runtime_suspend,
			mxhci_hsic_runtime_resume, mxhci_hsic_runtime_idle)
};

static const struct of_device_id of_mxhci_hsic_matach[] = {
	{ .compatible = "qcom,xhci-msm-hsic",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_mxhci_hsic_matach);

static struct platform_driver mxhci_hsic_driver = {
	.probe	= mxhci_hsic_probe,
	.remove	= mxhci_hsic_remove,
	.driver	= {
		.owner  = THIS_MODULE,
		.name = "xhci_msm_hsic",
		.pm = &xhci_msm_hsic_dev_pm_ops,
		.of_match_table	= of_mxhci_hsic_matach,
	},
};

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("XHCI MSM HSIC Glue Layer");

module_platform_driver(mxhci_hsic_driver);

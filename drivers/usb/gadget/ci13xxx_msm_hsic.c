/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/wakelock.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/usb.h>
#include <linux/clk/msm-clk.h>

#include <linux/usb/gadget.h>
#include <linux/usb/msm_hsusb_hw.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/msm-bus.h>

#include "ci13xxx_udc.c"

#define MSM_USB_BASE	(mhsic->regs)

#define ULPI_IO_TIMEOUT_USEC			(10 * 1000)
#define LINK_RESET_TIMEOUT_USEC			(250 * 1000)
#define PHY_SUSPEND_TIMEOUT_USEC		(500 * 1000)
#define PHY_RESUME_TIMEOUT_USEC			(100 * 1000)
#define HSIC_CFG_REG					0x30
#define HSIC_IO_CAL_PER_REG				0x33
#define HSIC_DBG1_REG					0x38

struct msm_hsic_per *the_mhsic;
static u64 msm_hsic_peripheral_dma_mask = DMA_BIT_MASK(32);

struct msm_hsic_per {
	struct device			*dev;
	struct clk			*iface_clk;
	struct clk			*core_clk;
	struct clk			*cal_sleep_clk;
	struct clk			*phy_clk;
	struct clk			*cal_clk;
	struct regulator		*hsic_vdd;
	int				async_int;
	int				vdd_val[3];
	struct regulator		*hsic_gdsc;
	void __iomem			*regs;
	void __iomem			*tlmm_regs;
	int				irq;
	int				async_irq_no;
	atomic_t			in_lpm;
	struct workqueue_struct		*wq;
	struct work_struct		suspend_w;
	struct ci13xxx_platform_data	*pdata;
	u32				bus_perf_client;
	struct msm_bus_scale_pdata	*bus_scale_table;
	enum usb_vdd_type		vdd_type;
	bool				connected;
};

#define NONE 0
#define MIN  1
#define MAX  2

static int msm_hsic_init_vdd(struct msm_hsic_per *mhsic, int init)
{
	int ret = 0;

	if (!mhsic->hsic_vdd) {
		mhsic->hsic_vdd = devm_regulator_get(mhsic->dev, "vdd");
		if (IS_ERR(mhsic->hsic_vdd)) {
			dev_err(mhsic->dev, "unable to get hsic vdd\n");
				return PTR_ERR(mhsic->hsic_vdd);
			}
	}

	if (mhsic->dev->of_node) {
		ret = of_property_read_u32_array(mhsic->dev->of_node,
			"qcom,vdd-voltage-level",
			mhsic->vdd_val, ARRAY_SIZE(mhsic->vdd_val));

		if (ret == -EINVAL)
			dev_err(mhsic->dev, "invalid vdd-level.\n");
		else if (ret == -ENODATA)
			dev_err(mhsic->dev, "no data for vdd-level.\n");
		else if (ret == -EOVERFLOW)
			dev_err(mhsic->dev, "overflow with vdd-level.\n");

		if (ret)
			return ret;
	} else {
		dev_err(mhsic->dev, "vdd config is not provided.\n");
		return -EINVAL;
	}

	if (!init)
		goto disable_reg;

	dev_dbg(mhsic->dev, "vdd[NONE]:%d vdd[MIN]:%d vdd[MAX]:%d\n",
		mhsic->vdd_val[NONE], mhsic->vdd_val[MIN], mhsic->vdd_val[MAX]);

	ret = regulator_set_voltage(mhsic->hsic_vdd, mhsic->vdd_val[MIN],
							mhsic->vdd_val[MAX]);
	if (ret) {
		dev_err(mhsic->dev, "unable to set the voltage for hsic vdd\n");
		goto reg_set_voltage_err;
	}

	ret = regulator_enable(mhsic->hsic_vdd);
	if (ret) {
		dev_err(mhsic->dev, "unable to enable hsic vddx\n");
		goto reg_enable_err;
	}

	return 0;

disable_reg:
	regulator_disable(mhsic->hsic_vdd);
reg_enable_err:
	regulator_set_voltage(mhsic->hsic_vdd, mhsic->vdd_val[NONE],
						mhsic->vdd_val[MAX]);
reg_set_voltage_err:

	return ret;

}

static int ulpi_write(struct msm_hsic_per *mhsic, u32 val, u32 reg)
{
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
		dev_err(mhsic->dev, "ulpi_write: timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int msm_hsic_phy_clk_reset(struct msm_hsic_per *mhsic)
{
	int ret;

	clk_disable_unprepare(mhsic->iface_clk);
	clk_disable_unprepare(mhsic->core_clk);
	clk_disable_unprepare(mhsic->phy_clk);
	clk_disable_unprepare(mhsic->cal_sleep_clk);
	clk_disable_unprepare(mhsic->cal_clk);

	ret = clk_reset(mhsic->core_clk, CLK_RESET_ASSERT);
	if (ret) {
		dev_err(mhsic->dev, "usb phy clk assert failed\n");
		return ret;
	}
	usleep_range(10000, 12000);

	ret = clk_reset(mhsic->core_clk, CLK_RESET_DEASSERT);
	if (ret)
		dev_err(mhsic->dev, "usb phy clk deassert failed\n");

	usleep_range(10000, 12000);

	clk_prepare_enable(mhsic->iface_clk);
	clk_prepare_enable(mhsic->core_clk);
	clk_prepare_enable(mhsic->phy_clk);
	clk_prepare_enable(mhsic->cal_sleep_clk);
	clk_prepare_enable(mhsic->cal_clk);

	return ret;
}

static int msm_hsic_phy_reset(struct msm_hsic_per *mhsic)
{
	u32 val;
	int ret;

	ret = msm_hsic_phy_clk_reset(mhsic);
	if (ret)
		return ret;

	val = readl_relaxed(USB_PORTSC) & ~PORTSC_PTS_MASK;
	writel_relaxed(val | PORTSC_PTS_ULPI, USB_PORTSC);

	/*
	 * Ensure that RESET operation is completed before
	 * turning off clock.
	 */
	mb();
	dev_dbg(mhsic->dev, "phy_reset: success\n");

	return 0;
}

static int msm_hsic_config_gdsc(struct platform_device *pdev,
			struct msm_hsic_per *mhsic, bool enable)
{
	int ret = 0;

	if (!mhsic->hsic_gdsc) {
		mhsic->hsic_gdsc = devm_regulator_get(&pdev->dev, "GDSC");
		if (IS_ERR(mhsic->hsic_gdsc))
			return 0;
	}

	if (enable) {
		ret = regulator_enable(mhsic->hsic_gdsc);
		if (ret) {
			dev_err(mhsic->dev, "unable to enable hsic gdsc\n");
			return ret;
		}
	} else {
		regulator_disable(mhsic->hsic_gdsc);
	}

	return 0;
}

static int msm_hsic_enable_clocks(struct platform_device *pdev,
				struct msm_hsic_per *mhsic, bool enable)
{
	int ret = 0;

	if (!enable)
		goto put_clocks;

	mhsic->iface_clk = clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(mhsic->iface_clk)) {
		dev_err(mhsic->dev, "failed to get iface_clk\n");
		ret = PTR_ERR(mhsic->iface_clk);
		goto error_enable_clocks;
	}

	mhsic->core_clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(mhsic->core_clk)) {
		dev_err(mhsic->dev, "failed to get core_clk\n");
		ret = PTR_ERR(mhsic->core_clk);
		goto put_iface_clk;
	}

	ret = clk_set_rate(mhsic->core_clk,
			clk_round_rate(mhsic->core_clk, LONG_MAX));
	if (ret)
		dev_err(mhsic->dev, "failed to set core_clk rate\n");

	mhsic->phy_clk = clk_get(&pdev->dev, "phy_clk");
	if (IS_ERR(mhsic->phy_clk)) {
		dev_err(mhsic->dev, "failed to get phy_clk\n");
		ret = PTR_ERR(mhsic->phy_clk);
		goto put_core_clk;
	}

	ret = clk_set_rate(mhsic->phy_clk,
			clk_round_rate(mhsic->phy_clk, LONG_MAX));
	if (ret)
		dev_err(mhsic->dev, "failed to set phy_clk rate\n");

	mhsic->cal_sleep_clk = clk_get(&pdev->dev, "cal_sleep_clk");
	if (IS_ERR(mhsic->cal_sleep_clk)) {
		dev_err(mhsic->dev, "!!!!failed to get cal_sleep_clk\n");
		ret = PTR_ERR(mhsic->cal_sleep_clk);
		goto put_phy_clk;
	}

	ret = clk_set_rate(mhsic->cal_sleep_clk, 32000);
	if (ret)
		dev_err(mhsic->dev, "failed to set cal_sleep_clk rate\n");

	mhsic->cal_clk = clk_get(&pdev->dev, "cal_clk");
	if (IS_ERR(mhsic->cal_clk)) {
		dev_err(mhsic->dev, "failed to get cal_clk\n");
		ret = PTR_ERR(mhsic->cal_clk);
		goto put_cal_sleep_clk;
	}

	ret = clk_set_rate(mhsic->cal_clk,
			clk_round_rate(mhsic->cal_clk, LONG_MAX));
	if (ret)
		dev_err(mhsic->dev, "failed to set cal_clk rate\n");

	clk_prepare_enable(mhsic->iface_clk);
	clk_prepare_enable(mhsic->core_clk);
	clk_prepare_enable(mhsic->phy_clk);
	clk_prepare_enable(mhsic->cal_sleep_clk);
	clk_prepare_enable(mhsic->cal_clk);

	return 0;

put_clocks:
	clk_disable_unprepare(mhsic->iface_clk);
	clk_disable_unprepare(mhsic->core_clk);
	clk_disable_unprepare(mhsic->phy_clk);
	clk_disable_unprepare(mhsic->cal_sleep_clk);
	clk_disable_unprepare(mhsic->cal_clk);

	clk_put(mhsic->cal_clk);
put_cal_sleep_clk:
	clk_put(mhsic->cal_sleep_clk);
put_phy_clk:
	clk_put(mhsic->phy_clk);
put_core_clk:
	clk_put(mhsic->core_clk);
put_iface_clk:
	clk_put(mhsic->iface_clk);
error_enable_clocks:

	return ret;
}

static int msm_hsic_reset(struct msm_hsic_per *mhsic)
{
	int cnt = 0;
	int ret;

	ret = msm_hsic_phy_reset(mhsic);
	if (ret) {
		dev_err(mhsic->dev, "phy_reset failed\n");
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

	/* Reset PORTSC and select ULPI phy */
	writel_relaxed(0x80000000, USB_PORTSC);
	return 0;
}

static void msm_hsic_wakeup(void)
{
	if (atomic_read(&the_mhsic->in_lpm))
		pm_runtime_resume(the_mhsic->dev);
}

static void msm_hsic_start(void)
{
	struct msm_hsic_per *mhsic = the_mhsic;
	int ret, *seq, seq_count;

	/* Program TLMM pad configuration for HSIC */
	seq = mhsic->pdata->tlmm_init_seq;
	seq_count = mhsic->pdata->tlmm_seq_count;
	if (seq && seq_count) {
		while (seq[0] >= 0 && seq_count > 0) {
			writel_relaxed(seq[1],
					mhsic->tlmm_regs + seq[0]);
			seq += 2;
			seq_count -= 2;
		}
	}
	/* ensure above writes are completed before programming PHY */
	wmb();

	/* programmable length of connect signaling (33.2ns) */
	ret = ulpi_write(the_mhsic, 3, HSIC_DBG1_REG);
	if (ret) {
		pr_err("%s: Unable to program length of connect signaling\n",
			    __func__);
	}

	/*set periodic calibration interval to ~2.048sec in HSIC_IO_CAL_REG */
	ret = ulpi_write(the_mhsic, 0xFF, HSIC_IO_CAL_PER_REG);

	if (ret) {
		pr_err("%s: Unable to set periodic calibration interval\n",
			    __func__);
	}

	/* Enable periodic IO calibration in HSIC_CFG register */
	ret = ulpi_write(the_mhsic, 0xE9, HSIC_CFG_REG);
	if (ret) {
		pr_err("%s: Unable to enable periodic IO calibration\n",
			    __func__);
	}
}


#ifdef CONFIG_PM_SLEEP
static int msm_hsic_suspend(struct msm_hsic_per *mhsic)
{
	int cnt = 0, ret;
	u32 val;

	if (atomic_read(&mhsic->in_lpm)) {
		dev_dbg(mhsic->dev, "%s called while in lpm\n", __func__);
		return 0;
	}
	disable_irq(mhsic->irq);

	/* Don't try to put PHY into suspend if it is not in CONNECT state. */
	if (the_mhsic->connected) {
		/*
		 * PHY may take some time or even fail to enter into low power
		 * mode (LPM). Hence poll for 500 msec and reset the PHY and
		 * link in failure case.
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
			dev_err(mhsic->dev, "Unable to suspend PHY\n");
			msm_hsic_reset(mhsic);
		}

		/*
		 * PHY has capability to generate interrupt asynchronously in
		 * low power mode (LPM). This interrupt is level triggered. So
		 * USB IRQ line must be disabled till async interrupt enable bit
		 * is cleared in USBCMD register. Assert STP (ULPI interface
		 * STOP signal) to block data communication from PHY.
		 */
		writel_relaxed(readl_relaxed(USB_USBCMD) | ASYNC_INTR_CTRL |
					ULPI_STP_CTRL, USB_USBCMD);

		/*
		 * Ensure that hardware is put in low power mode before
		 * clocks are turned OFF and VDD is allowed to minimize.
		 */
		mb();
	} else {
		dev_dbg(mhsic->dev, "%s SKIP PHY suspend\n", __func__);
	}

	clk_disable_unprepare(mhsic->iface_clk);
	clk_disable_unprepare(mhsic->core_clk);

	clk_disable_unprepare(mhsic->phy_clk);
	clk_disable_unprepare(mhsic->cal_clk);

	ret = regulator_set_voltage(mhsic->hsic_vdd, mhsic->vdd_val[NONE],
							mhsic->vdd_val[MAX]);
	if (ret < 0)
		dev_err(mhsic->dev, "unable to set vdd voltage for VDD MIN\n");
	if (mhsic->bus_perf_client) {
		ret = msm_bus_scale_client_update_request(
				mhsic->bus_perf_client, 0);
		if (ret)
			dev_err(mhsic->dev, "Failed to vote for bus scaling\n");
	}

	if (device_may_wakeup(mhsic->dev)) {
		enable_irq_wake(mhsic->irq);
		if (mhsic->async_irq_no)
			enable_irq_wake(mhsic->async_irq_no);
	}

	atomic_set(&mhsic->in_lpm, 1);
	/* If async irq present, enable while going into LPM */
	if (mhsic->async_irq_no)
		enable_irq(mhsic->async_irq_no);

	enable_irq(mhsic->irq);
	pm_relax(mhsic->dev);

	dev_info(mhsic->dev, "HSIC-USB in low power mode\n");

	return 0;
}

static int msm_hsic_resume(struct msm_hsic_per *mhsic)
{
	int cnt = 0, ret;
	unsigned temp;

	if (!atomic_read(&mhsic->in_lpm)) {
		dev_dbg(mhsic->dev, "%s called while not in lpm\n", __func__);
		return 0;
	}

	pm_stay_awake(mhsic->dev);

	if (mhsic->bus_perf_client) {
		ret = msm_bus_scale_client_update_request(
				mhsic->bus_perf_client, 1);
		if (ret)
			dev_err(mhsic->dev, "Failed to vote for bus scaling\n");
	}
	ret = regulator_set_voltage(mhsic->hsic_vdd, mhsic->vdd_val[MIN],
							mhsic->vdd_val[MAX]);
	if (ret < 0)
		dev_err(mhsic->dev,
			"unable to set nominal vddcx voltage (no VDD MIN)\n");

	clk_prepare_enable(mhsic->iface_clk);
	clk_prepare_enable(mhsic->core_clk);

	clk_prepare_enable(mhsic->phy_clk);
	clk_prepare_enable(mhsic->cal_clk);

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
		dev_err(mhsic->dev, "Unable to resume USB. Reset the hsic\n");
		msm_hsic_reset(mhsic);
	}
skip_phy_resume:
	if (device_may_wakeup(mhsic->dev)) {
		disable_irq_wake(mhsic->irq);
		if (mhsic->async_irq_no)
			disable_irq_wake(mhsic->async_irq_no);
	}

	atomic_set(&mhsic->in_lpm, 0);

	if (mhsic->async_int) {
		enable_irq(mhsic->async_int);
		mhsic->async_int = 0;
	}

	/* If Async irq present, keep it disable once out of LPM */
	if (mhsic->async_irq_no)
		disable_irq(mhsic->async_irq_no);

	dev_info(mhsic->dev, "HSIC-USB exited from low power mode\n");

	return 0;
}

static int msm_hsic_pm_suspend(struct device *dev)
{
	struct msm_hsic_per *mhsic = dev_get_drvdata(dev);

	dev_dbg(dev, "MSM HSIC Peripheral PM suspend\n");

	return msm_hsic_suspend(mhsic);
}

#ifdef CONFIG_PM_RUNTIME
static int msm_hsic_pm_resume(struct device *dev)
{
	dev_dbg(dev, "MSM HSIC Peripheral PM resume\n");

	/*
	 * Do not resume hardware as part of system resume,
	 * rather, wait for the ASYNC INT from the h/w
	 */
	return 0;
}
#else
static int msm_hsic_pm_resume(struct device *dev)
{
	struct msm_hsic_per *mhsic = dev_get_drvdata(dev);

	dev_dbg(dev, "MSM HSIC Peripheral PM resume\n");

	return msm_hsic_resume(mhsic);
}
#endif

static void msm_hsic_pm_suspend_work(struct work_struct *w)
{
	pm_runtime_put_noidle(the_mhsic->dev);
	pm_runtime_suspend(the_mhsic->dev);
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_RUNTIME
static int msm_hsic_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "MSM HSIC Peripheral runtime idle\n");

	return 0;
}

static int msm_hsic_runtime_suspend(struct device *dev)
{
	struct msm_hsic_per *mhsic = dev_get_drvdata(dev);

	dev_dbg(dev, "MSM HSIC Peripheral runtime suspend\n");

	return msm_hsic_suspend(mhsic);
}

static int msm_hsic_runtime_resume(struct device *dev)
{
	struct msm_hsic_per *mhsic = dev_get_drvdata(dev);

	dev_dbg(dev, "MSM HSIC Peripheral runtime resume\n");
	pm_runtime_get_noresume(mhsic->dev);

	return msm_hsic_resume(mhsic);
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops msm_hsic_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msm_hsic_pm_suspend, msm_hsic_pm_resume)
	SET_RUNTIME_PM_OPS(msm_hsic_runtime_suspend, msm_hsic_runtime_resume,
				msm_hsic_runtime_idle)
};
#endif

/**
 * Dummy match function - will be called only for HSIC msm
 * device (msm_device_gadget_hsic_peripheral).
 */
static inline int __match(struct device *dev, void *data) { return 1; }

static void msm_hsic_connect_peripheral(struct device *msm_udc_dev)
{
	struct device *dev;
	struct usb_gadget *gadget;

	dev = device_find_child(msm_udc_dev, NULL, __match);
	gadget = dev_to_usb_gadget(dev);
	usb_gadget_vbus_connect(gadget);
}

static void msm_hsic_disconnect_peripheral(struct device *msm_udc_dev)
{
	struct device *dev;
	struct usb_gadget *gadget;

	dev = device_find_child(msm_udc_dev, NULL, __match);
	gadget = dev_to_usb_gadget(dev);
	usb_gadget_vbus_disconnect(gadget);
}


static irqreturn_t msm_udc_hsic_irq(int irq, void *data)
{
	struct msm_hsic_per *mhsic = data;

	if (atomic_read(&mhsic->in_lpm)) {
		pr_debug("%s(): HSIC IRQ:%d in LPM\n", __func__, irq);
		disable_irq_nosync(irq);
		mhsic->async_int = irq;
		pm_request_resume(mhsic->dev);
		return IRQ_HANDLED;
	}

	return udc_irq();
}

/**
 * store_hsic_init: initialize hsic interface to state passed
 */
static ssize_t store_hsic_init(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct msm_hsic_per *mhsic = the_mhsic;
	int init_state, ret;

	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		goto done;
	}

	if (kstrtoint(buf, 10, &init_state) < 0) {
		dev_err(dev, "scan init_state failed\n");
		goto done;
	}

	dev_dbg(dev, "Value for init_state = %d\n", init_state);

	if (init_state == 1) {
		pm_runtime_resume(mhsic->dev);
		ret = msm_hsic_reset(mhsic);
		if (ret)
			pr_err("msm_hsic_reset failed\n");
		msm_hsic_start();
		usleep_range(10000, 10010);
		msm_hsic_connect_peripheral(mhsic->dev);
		the_mhsic->connected = true;
	} else if (init_state == 0) {
		msm_hsic_disconnect_peripheral(mhsic->dev);
		mhsic->connected = false;
		pm_runtime_put_noidle(mhsic->dev);
		pm_runtime_suspend(mhsic->dev);
	} else {
		pr_err("Invalid value : no action taken\n");
	}

done:
	return count;
}

static DEVICE_ATTR(hsic_init, S_IWUSR, NULL, store_hsic_init);

static void ci13xxx_msm_hsic_notify_event(struct ci13xxx *udc, unsigned event)
{
	struct device *dev = udc->gadget.dev.parent;
	struct msm_hsic_per *mhsic = the_mhsic;
	int	temp;

	switch (event) {
	case CI13XXX_CONTROLLER_RESET_EVENT:
		dev_info(dev, "CI13XXX_CONTROLLER_RESET_EVENT received\n");
		writel_relaxed(0, USB_AHBBURST);
		writel_relaxed(0x08, USB_AHBMODE);

		/* workaround for rx buffer collision issue */
		temp = readl_relaxed(USB_GENCONFIG);
		temp &= ~GENCONFIG_TXFIFO_IDLE_FORCE_DISABLE;
		writel_relaxed(temp, USB_GENCONFIG);
		/*
		 * Ensure that register write for workaround is completed
		 * before configuring USBMODE.
		 */
		mb();
		break;
	case CI13XXX_CONTROLLER_CONNECT_EVENT:
		dev_info(dev, "CI13XXX_CONTROLLER_CONNECT_EVENT received\n");
		/* bring HSIC core out of LPM */
		pm_runtime_get_sync(the_mhsic->dev);
		msm_hsic_start();
		the_mhsic->connected = true;
		break;
	case CI13XXX_CONTROLLER_SUSPEND_EVENT:
		dev_info(dev, "CI13XXX_CONTROLLER_SUSPEND_EVENT received\n");
		queue_work(mhsic->wq, &mhsic->suspend_w);
		break;
	case CI13XXX_CONTROLLER_REMOTE_WAKEUP_EVENT:
		dev_info(dev,
			 "CI13XXX_CONTROLLER_REMOTE_WAKEUP_EVENT received\n");
		msm_hsic_wakeup();
		break;
	case CI13XXX_CONTROLLER_UDC_STARTED_EVENT:
		dev_info(dev,
			 "CI13XXX_CONTROLLER_UDC_STARTED_EVENT received\n");
		mhsic->connected = false;
		/* put HSIC core into LPM */
		pm_runtime_put_noidle(the_mhsic->dev);
		pm_runtime_suspend(the_mhsic->dev);
		break;
	default:
		dev_dbg(dev, "unknown ci13xxx_udc event\n");
		break;
	}
}

static struct ci13xxx_udc_driver ci13xxx_msm_udc_hsic_driver = {
	.name			= "ci13xxx_msm_hsic",
	.flags			= CI13XXX_REGS_SHARED |
				  CI13XXX_PULLUP_ON_VBUS |
				  CI13XXX_DISABLE_STREAMING |
				  CI13XXX_ZERO_ITC,

	.notify_event		= ci13xxx_msm_hsic_notify_event,
};

struct ci13xxx_platform_data *msm_hsic_peripheral_dt_to_pdata(
					struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct ci13xxx_platform_data *pdata;
	u32 core_id;
	int ret, len;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	ret = of_property_read_u32(node, "qcom,hsic-usb-core-id", &core_id);
	if (ret)
		dev_err(&pdev->dev, "hsic usb core id is not provided.\n");
	else
		pdata->usb_core_id = (u8)core_id;

	of_get_property(node, "qcom,hsic-tlmm-init-seq", &len);
	if (len) {
		pdata->tlmm_init_seq = devm_kzalloc(&pdev->dev, len,
						    GFP_KERNEL);
		if (!pdata->tlmm_init_seq)
			return NULL;

		pdata->tlmm_seq_count = len / sizeof(*pdata->tlmm_init_seq);
		ret = of_property_read_u32_array(node,
				"qcom,hsic-tlmm-init-seq",
				pdata->tlmm_init_seq, pdata->tlmm_seq_count);
		if (ret) {
			dev_err(&pdev->dev, "hsic init-seq failed:%d\n", ret);
			pdata->tlmm_seq_count = 0;
		}
	}

	return pdata;
}

static int msm_hsic_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct msm_hsic_per *mhsic;
	int ret = 0;
	struct ci13xxx_platform_data *pdata;

	dev_dbg(&pdev->dev, "msm-hsic probe\n");

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		pdev->dev.platform_data = msm_hsic_peripheral_dt_to_pdata(pdev);
	}

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "No platform data given. Bailing out\n");
		return -ENODEV;
	}

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &msm_hsic_peripheral_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	pdata = pdev->dev.platform_data;

	mhsic = kzalloc(sizeof(struct msm_hsic_per), GFP_KERNEL);
	if (!mhsic)
		return -ENOMEM;
	the_mhsic = mhsic;
	platform_set_drvdata(pdev, mhsic);
	mhsic->dev = &pdev->dev;
	mhsic->pdata = pdata;

	mhsic->irq = platform_get_irq(pdev, 0);
	if (mhsic->irq < 0) {
		dev_err(&pdev->dev, "Unable to get IRQ resource\n");
		ret = mhsic->irq;
		goto error;
	}

	mhsic->async_irq_no = platform_get_irq(pdev, 1);
	if (mhsic->async_irq_no < 0) {
		dev_err(&pdev->dev, "Unable to get async IRQ resource\n");
		ret = mhsic->async_irq_no;
		goto error;
	}

	mhsic->wq = alloc_workqueue("mhsic_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!mhsic->wq) {
		pr_err("%s: Unable to create workqueue mhsic wq\n",
				__func__);
		ret = -ENOMEM;
		goto error;
	}
	INIT_WORK(&mhsic->suspend_w, msm_hsic_pm_suspend_work);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get memory resource\n");
		ret = -ENODEV;
		goto error;
	}
	mhsic->regs = ioremap(res->start, resource_size(res));
	if (!mhsic->regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto error;
	}
	dev_info(&pdev->dev, "HSIC Peripheral regs = %p\n", mhsic->regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res && pdata->tlmm_init_seq) {
		dev_err(&pdev->dev, "Unable to get TLMM memory resource\n");
		ret = -ENODEV;
		goto unmap;
	} else if (res) {
		mhsic->tlmm_regs =  devm_ioremap(&pdev->dev, res->start,
						 resource_size(res));
		if (IS_ERR(mhsic->tlmm_regs)) {
			ret = PTR_ERR(mhsic->tlmm_regs);
			goto unmap;
		}
	}

	ret = msm_hsic_config_gdsc(pdev, mhsic, true);
	if (ret) {
		dev_err(&pdev->dev, "unable to configure hsic gdsc\n");
		goto unmap;
	}

	ret = msm_hsic_enable_clocks(pdev, mhsic, true);

	if (ret) {
		dev_err(&pdev->dev, "msm_hsic_enable_clocks failed\n");
		ret = -ENODEV;
		goto unconfig_gdsc;
	}
	ret = msm_hsic_init_vdd(mhsic, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize VDDCX\n");
		ret = -ENODEV;
		goto deinit_clocks;
	}

	ret = msm_hsic_reset(mhsic);
	if (ret) {
		dev_err(&pdev->dev, "msm_hsic_reset failed\n");
		ret = -ENODEV;
		goto deinit_vddcx;
	}

	mhsic->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!mhsic->bus_scale_table) {
		dev_err(&pdev->dev, "bus scaling is disabled\n");
	} else {
		mhsic->bus_perf_client =
			msm_bus_scale_register_client(mhsic->bus_scale_table);
		ret = msm_bus_scale_client_update_request(
						mhsic->bus_perf_client, 1);
		dev_dbg(&pdev->dev, "bus scaling is enabled\n");
		if (ret)
			dev_err(mhsic->dev, "Failed to vote for bus scaling\n");
	}

	ret = udc_probe(&ci13xxx_msm_udc_hsic_driver, &pdev->dev, mhsic->regs);
	if (ret < 0) {
		dev_err(&pdev->dev, "udc_probe failed\n");
		ret = -ENODEV;
		goto deinit_vddcx;
	}

	ret = device_create_file(mhsic->dev, &dev_attr_hsic_init);
	if (ret)
		goto udc_remove;
	msm_hsic_connect_peripheral(&pdev->dev);

	device_init_wakeup(&pdev->dev, 1);
	pm_stay_awake(mhsic->dev);

	ret = request_irq(mhsic->irq, msm_udc_hsic_irq,
					  IRQF_SHARED, pdev->name, mhsic);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq failed\n");
		ret = -ENODEV;
		goto remove_sysfs;
	}

	ret = request_irq(mhsic->async_irq_no, msm_udc_hsic_irq,
					  IRQF_TRIGGER_HIGH, pdev->name, mhsic);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq failed\n");
		ret = -ENODEV;
		goto free_core_irq;
	}

	disable_irq(mhsic->async_irq_no);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	return 0;
free_core_irq:
	free_irq(mhsic->irq, mhsic);
remove_sysfs:
	device_remove_file(mhsic->dev, &dev_attr_hsic_init);
udc_remove:
	udc_remove();
	if (mhsic->bus_perf_client)
		msm_bus_scale_unregister_client(mhsic->bus_perf_client);
deinit_vddcx:
	msm_hsic_init_vdd(mhsic, 0);
deinit_clocks:
	msm_hsic_enable_clocks(pdev, mhsic, 0);
unconfig_gdsc:
	msm_hsic_config_gdsc(pdev, mhsic, false);
unmap:
	iounmap(mhsic->regs);
error:
	if (mhsic->wq)
		destroy_workqueue(mhsic->wq);
	kfree(mhsic);
	return ret;
}

static int hsic_msm_remove(struct platform_device *pdev)
{
	struct msm_hsic_per *mhsic = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	free_irq(mhsic->irq, mhsic);
	free_irq(mhsic->async_irq_no, mhsic);

	msm_hsic_init_vdd(mhsic, 0);
	msm_hsic_enable_clocks(pdev, mhsic, 0);
	device_wakeup_disable(mhsic->dev);
	destroy_workqueue(mhsic->wq);
	if (mhsic->bus_perf_client)
		msm_bus_scale_unregister_client(mhsic->bus_perf_client);
	device_remove_file(mhsic->dev, &dev_attr_hsic_init);
	udc_remove();
	iounmap(mhsic->regs);
	kfree(mhsic);

	return 0;
}

static const struct of_device_id hsic_peripheral_dt_match[] = {
	{ .compatible = "qcom,hsic-peripheral",
	},
	{}
};

static struct platform_driver msm_hsic_peripheral_driver = {
	.probe	= msm_hsic_probe,
	.remove	= hsic_msm_remove,
	.driver = {
		.name = "msm_hsic_peripheral",
#ifdef CONFIG_PM
		.pm = &msm_hsic_dev_pm_ops,
#endif
		.of_match_table = hsic_peripheral_dt_match,
	},
};

static int __init msm_hsic_peripheral_init(void)
{
	return platform_driver_register(&msm_hsic_peripheral_driver);
}

static void __exit msm_hsic_peripheral_exit(void)
{
	platform_driver_unregister(&msm_hsic_peripheral_driver);
}

module_init(msm_hsic_peripheral_init);
module_exit(msm_hsic_peripheral_exit);

MODULE_LICENSE("GPL v2");

/* Copyright (c) 2009-2018, Linux Foundation. All rights reserved.
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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/clk/msm-clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/irqchip/msm-mpm-irq.h>
#include <soc/qcom/scm.h>

#include <linux/usb.h>
#include <linux/usb/otg.h>
#include <linux/usb/ulpi.h>
#include <linux/usb/gadget.h>
#include <linux/usb/hcd.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/usb/msm_hsusb_hw.h>
#include <linux/usb/msm_ext_chg.h>
#include <linux/regulator/consumer.h>
#include <linux/qpnp/qpnp-adc.h>

#include <linux/msm-bus.h>

#define MSM_USB_BASE	(motg->regs)
#define MSM_USB_PHY_CSR_BASE (motg->phy_csr_regs)

#define DRIVER_NAME	"msm_otg"

#define CHG_RECHECK_DELAY	(jiffies + msecs_to_jiffies(2000))
#define ULPI_IO_TIMEOUT_USEC	(10 * 1000)
#define USB_PHY_3P3_VOL_MIN	3050000 /* uV */
#define USB_PHY_3P3_VOL_MAX	3300000 /* uV */
#define USB_PHY_3P3_HPM_LOAD	50000	/* uA */
#define USB_PHY_3P3_LPM_LOAD	4000	/* uA */

#define USB_PHY_1P8_VOL_MIN	1800000 /* uV */
#define USB_PHY_1P8_VOL_MAX	1800000 /* uV */
#define USB_PHY_1P8_HPM_LOAD	50000	/* uA */
#define USB_PHY_1P8_LPM_LOAD	4000	/* uA */

#define USB_PHY_VDD_DIG_VOL_NONE	0 /*uV */
#define USB_PHY_VDD_DIG_VOL_MIN	1045000 /* uV */
#define USB_PHY_VDD_DIG_VOL_MAX	1320000 /* uV */

#define USB_SUSPEND_DELAY_TIME	(500 * HZ/1000) /* 500 msec */

#define USB_DEFAULT_SYSTEM_CLOCK 80000000	/* 80 MHz */

#define PM_QOS_SAMPLE_SEC	2
#define PM_QOS_THRESHOLD	400

#define MICRO_5V 5000000
#define MICRO_9V 9000000

#define SDP_CURRENT_UA 500000
#define CDP_CURRENT_UA 1500000
#define DCP_CURRENT_UA 1500000
#define HVDCP_CURRENT_UA 3000000

enum msm_otg_phy_reg_mode {
	USB_PHY_REG_OFF,
	USB_PHY_REG_ON,
	USB_PHY_REG_LPM_ON,
	USB_PHY_REG_LPM_OFF,
	USB_PHY_REG_3P3_ON,
	USB_PHY_REG_3P3_OFF,
};

static char *override_phy_init;
module_param(override_phy_init, charp, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(override_phy_init,
	"Override HSUSB PHY Init Settings");

unsigned int lpm_disconnect_thresh = 1000;
module_param(lpm_disconnect_thresh , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(lpm_disconnect_thresh,
	"Delay before entering LPM on USB disconnect");

static bool floated_charger_enable;
module_param(floated_charger_enable , bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(floated_charger_enable,
	"Whether to enable floated charger");

/* by default debugging is enabled */
static unsigned int enable_dbg_log = 1;
module_param(enable_dbg_log, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_dbg_log, "Debug buffer events");

/* Max current to be drawn for HVDCP charger */
static int hvdcp_max_current = IDEV_HVDCP_CHG_MAX;
module_param(hvdcp_max_current, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(hvdcp_max_current, "max current drawn for HVDCP charger");

/* Max current to be drawn for DCP charger */
static int dcp_max_current = IDEV_CHG_MAX;
module_param(dcp_max_current, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dcp_max_current, "max current drawn for DCP charger");

static DECLARE_COMPLETION(pmic_vbus_init);
static struct msm_otg *the_msm_otg;
static bool debug_bus_voting_enabled;

static struct regulator *hsusb_3p3;
static struct regulator *hsusb_1p8;
static struct regulator *hsusb_vdd;
static struct regulator *vbus_otg;
static struct power_supply *psy;

static int vdd_val[VDD_VAL_MAX];
static u32 bus_freqs[USB_NOC_NUM_VOTE][USB_NUM_BUS_CLOCKS]  /*bimc,snoc,pcnoc*/;
static char bus_clkname[USB_NUM_BUS_CLOCKS][20] = {"bimc_clk", "snoc_clk",
						"pcnoc_clk"};
static bool bus_clk_rate_set;

static void dbg_inc(unsigned *idx)
{
	*idx = (*idx + 1) & (DEBUG_MAX_MSG-1);
}

static void
msm_otg_dbg_log_event(struct usb_phy *phy, char *event, int d1, int d2)
{
	struct msm_otg *motg = container_of(phy, struct msm_otg, phy);
	unsigned long flags;
	unsigned long long t;
	unsigned long nanosec;

	if (!enable_dbg_log)
		return;

	write_lock_irqsave(&motg->dbg_lock, flags);
	t = cpu_clock(smp_processor_id());
	nanosec = do_div(t, 1000000000)/1000;
	scnprintf(motg->buf[motg->dbg_idx], DEBUG_MSG_LEN,
			"[%5lu.%06lu]: %s :%d:%d",
			(unsigned long)t, nanosec, event, d1, d2);

	motg->dbg_idx++;
	motg->dbg_idx = motg->dbg_idx % DEBUG_MAX_MSG;
	write_unlock_irqrestore(&motg->dbg_lock, flags);
}

static int msm_hsusb_ldo_init(struct msm_otg *motg, int init)
{
	int rc = 0;

	if (init) {
		hsusb_3p3 = devm_regulator_get(motg->phy.dev, "HSUSB_3p3");
		if (IS_ERR(hsusb_3p3)) {
			dev_err(motg->phy.dev, "unable to get hsusb 3p3\n");
			return PTR_ERR(hsusb_3p3);
		}

		rc = regulator_set_voltage(hsusb_3p3, USB_PHY_3P3_VOL_MIN,
				USB_PHY_3P3_VOL_MAX);
		if (rc) {
			dev_err(motg->phy.dev, "unable to set voltage level for"
					"hsusb 3p3\n");
			return rc;
		}
		hsusb_1p8 = devm_regulator_get(motg->phy.dev, "HSUSB_1p8");
		if (IS_ERR(hsusb_1p8)) {
			dev_err(motg->phy.dev, "unable to get hsusb 1p8\n");
			rc = PTR_ERR(hsusb_1p8);
			goto put_3p3_lpm;
		}
		rc = regulator_set_voltage(hsusb_1p8, USB_PHY_1P8_VOL_MIN,
				USB_PHY_1P8_VOL_MAX);
		if (rc) {
			dev_err(motg->phy.dev, "unable to set voltage level "
					"for hsusb 1p8\n");
			goto put_1p8;
		}

		return 0;
	}

put_1p8:
	regulator_set_voltage(hsusb_1p8, 0, USB_PHY_1P8_VOL_MAX);
put_3p3_lpm:
	regulator_set_voltage(hsusb_3p3, 0, USB_PHY_3P3_VOL_MAX);
	return rc;
}

static int msm_hsusb_config_vddcx(int high)
{
	struct msm_otg *motg = the_msm_otg;
	int max_vol = vdd_val[VDD_MAX];
	int min_vol;
	int ret;

	min_vol = vdd_val[!!high];
	ret = regulator_set_voltage(hsusb_vdd, min_vol, max_vol);
	if (ret) {
		pr_err("%s: unable to set the voltage for regulator "
			"HSUSB_VDDCX\n", __func__);
		return ret;
	}

	pr_debug("%s: min_vol:%d max_vol:%d\n", __func__, min_vol, max_vol);
	msm_otg_dbg_log_event(&motg->phy, "CONFIG VDDCX", min_vol, max_vol);

	return ret;
}

static int msm_hsusb_ldo_enable(struct msm_otg *motg,
	enum msm_otg_phy_reg_mode mode)
{
	int ret = 0;

	if (IS_ERR(hsusb_1p8)) {
		pr_err("%s: HSUSB_1p8 is not initialized\n", __func__);
		return -ENODEV;
	}

	if (IS_ERR(hsusb_3p3)) {
		pr_err("%s: HSUSB_3p3 is not initialized\n", __func__);
		return -ENODEV;
	}

	switch (mode) {
	case USB_PHY_REG_ON:
		ret = regulator_set_optimum_mode(hsusb_1p8,
				USB_PHY_1P8_HPM_LOAD);
		if (ret < 0) {
			pr_err("%s: Unable to set HPM of the regulator "
				"HSUSB_1p8\n", __func__);
			return ret;
		}

		ret = regulator_enable(hsusb_1p8);
		if (ret) {
			dev_err(motg->phy.dev, "%s: unable to enable the hsusb 1p8\n",
				__func__);
			regulator_set_optimum_mode(hsusb_1p8, 0);
			return ret;
		}

	/* fall through */
	case USB_PHY_REG_3P3_ON:
		ret = regulator_set_optimum_mode(hsusb_3p3,
				USB_PHY_3P3_HPM_LOAD);
		if (ret < 0) {
			pr_err("%s: Unable to set HPM of the regulator "
				"HSUSB_3p3\n", __func__);
			if (mode == USB_PHY_REG_ON) {
				regulator_set_optimum_mode(hsusb_1p8, 0);
				regulator_disable(hsusb_1p8);
			}
			return ret;
		}

		ret = regulator_enable(hsusb_3p3);
		if (ret) {
			dev_err(motg->phy.dev, "%s: unable to enable the hsusb 3p3\n",
				__func__);
			regulator_set_optimum_mode(hsusb_3p3, 0);
			if (mode == USB_PHY_REG_ON) {
				regulator_set_optimum_mode(hsusb_1p8, 0);
				regulator_disable(hsusb_1p8);
			}
			return ret;
		}

		break;

	case USB_PHY_REG_OFF:
		ret = regulator_disable(hsusb_1p8);
		if (ret) {
			dev_err(motg->phy.dev, "%s: unable to disable the hsusb 1p8\n",
				__func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(hsusb_1p8, 0);
		if (ret < 0)
			pr_err("%s: Unable to set LPM of the regulator "
				"HSUSB_1p8\n", __func__);

	/* fall through */
	case USB_PHY_REG_3P3_OFF:
		ret = regulator_disable(hsusb_3p3);
		if (ret) {
			dev_err(motg->phy.dev, "%s: unable to disable the hsusb 3p3\n",
				 __func__);
			return ret;
		}
		ret = regulator_set_optimum_mode(hsusb_3p3, 0);
		if (ret < 0)
			pr_err("%s: Unable to set LPM of the regulator "
				"HSUSB_3p3\n", __func__);

		break;

	case USB_PHY_REG_LPM_ON:
		ret = regulator_set_optimum_mode(hsusb_1p8,
				USB_PHY_1P8_LPM_LOAD);
		if (ret < 0) {
			pr_err("%s: Unable to set LPM of the regulator: HSUSB_1p8\n",
				__func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(hsusb_3p3,
				USB_PHY_3P3_LPM_LOAD);
		if (ret < 0) {
			pr_err("%s: Unable to set LPM of the regulator: HSUSB_3p3\n",
				__func__);
			regulator_set_optimum_mode(hsusb_1p8, USB_PHY_REG_ON);
			return ret;
		}

		break;

	case USB_PHY_REG_LPM_OFF:
		ret = regulator_set_optimum_mode(hsusb_1p8,
				USB_PHY_1P8_HPM_LOAD);
		if (ret < 0) {
			pr_err("%s: Unable to set HPM of the regulator: HSUSB_1p8\n",
				__func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(hsusb_3p3,
				USB_PHY_3P3_HPM_LOAD);
		if (ret < 0) {
			pr_err("%s: Unable to set HPM of the regulator: HSUSB_3p3\n",
				__func__);
			regulator_set_optimum_mode(hsusb_1p8, USB_PHY_REG_ON);
			return ret;
		}

		break;

	default:
		pr_err("%s: Unsupported mode (%d).", __func__, mode);
		return -ENOTSUPP;
	}

	pr_debug("%s: USB reg mode (%d) (OFF/HPM/LPM)\n", __func__, mode);
	msm_otg_dbg_log_event(&motg->phy, "USB REG MODE", mode, ret);
	return ret < 0 ? ret : 0;
}

static int ulpi_read(struct usb_phy *phy, u32 reg)
{
	struct msm_otg *motg = container_of(phy, struct msm_otg, phy);
	int cnt = 0;

	if (motg->pdata->emulation)
		return 0;

	if (motg->pdata->phy_type == QUSB_ULPI_PHY && reg > 0x3F) {
		pr_debug("%s: ULPI vendor-specific reg 0x%02x not supported\n",
			__func__, reg);
		return 0;
	}

	/* initiate read operation */
	writel_relaxed(ULPI_RUN | ULPI_READ | ULPI_ADDR(reg),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	while (cnt < ULPI_IO_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_RUN))
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= ULPI_IO_TIMEOUT_USEC) {
		dev_err(phy->dev, "ulpi_read: timeout %08x\n",
			readl_relaxed(USB_ULPI_VIEWPORT));
		dev_err(phy->dev, "PORTSC: %08x USBCMD: %08x\n",
			readl_relaxed(USB_PORTSC), readl_relaxed(USB_USBCMD));
		return -ETIMEDOUT;
	}
	return ULPI_DATA_READ(readl_relaxed(USB_ULPI_VIEWPORT));
}

static int ulpi_write(struct usb_phy *phy, u32 val, u32 reg)
{
	struct msm_otg *motg = container_of(phy, struct msm_otg, phy);
	int cnt = 0;

	if (motg->pdata->emulation)
		return 0;

	if (motg->pdata->phy_type == QUSB_ULPI_PHY && reg > 0x3F) {
		pr_debug("%s: ULPI vendor-specific reg 0x%02x not supported\n",
			__func__, reg);
		return 0;
	}

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
		dev_err(phy->dev, "ulpi_write: timeout\n");
		dev_err(phy->dev, "PORTSC: %08x USBCMD: %08x\n",
			readl_relaxed(USB_PORTSC), readl_relaxed(USB_USBCMD));
		return -ETIMEDOUT;
	}
	return 0;
}

static struct usb_phy_io_ops msm_otg_io_ops = {
	.read = ulpi_read,
	.write = ulpi_write,
};

static void ulpi_init(struct msm_otg *motg)
{
	struct msm_otg_platform_data *pdata = motg->pdata;
	int aseq[10];
	int *seq = NULL;

	if (override_phy_init) {
		pr_debug("%s(): HUSB PHY Init:%s\n", __func__,
				override_phy_init);
		get_options(override_phy_init, ARRAY_SIZE(aseq), aseq);
		seq = &aseq[1];
	} else {
		seq = pdata->phy_init_seq;
	}

	if (!seq)
		return;

	while (seq[0] >= 0) {
		if (override_phy_init)
			pr_debug("ulpi: write 0x%02x to 0x%02x\n",
					seq[0], seq[1]);

		dev_vdbg(motg->phy.dev, "ulpi: write 0x%02x to 0x%02x\n",
				seq[0], seq[1]);
		msm_otg_dbg_log_event(&motg->phy, "ULPI WRITE", seq[0], seq[1]);
		ulpi_write(&motg->phy, seq[0], seq[1]);
		seq += 2;
	}
}

static int msm_otg_phy_clk_reset(struct msm_otg *motg)
{
	int ret;

	if (!motg->phy_reset_clk)
		return 0;

	if (motg->sleep_clk)
		clk_disable_unprepare(motg->sleep_clk);
	if (motg->phy_csr_clk)
		clk_disable_unprepare(motg->phy_csr_clk);

	ret = clk_reset(motg->phy_reset_clk, CLK_RESET_ASSERT);
	if (ret < 0) {
		pr_err("phy_reset_clk assert failed %d\n", ret);
		return ret;
	}
	/*
	 * As per databook, 10 usec delay is required between
	 * PHY POR assert and de-assert.
	 */
	usleep_range(10, 15);
	ret = clk_reset(motg->phy_reset_clk, CLK_RESET_DEASSERT);
	if (ret < 0) {
		pr_err("phy_reset_clk de-assert failed %d\n", ret);
		return ret;
	}
	/*
	 * As per databook, it takes 75 usec for PHY to stabilize
	 * after the reset.
	 */
	usleep_range(80, 100);

	if (motg->phy_csr_clk)
		clk_prepare_enable(motg->phy_csr_clk);
	if (motg->sleep_clk)
		clk_prepare_enable(motg->sleep_clk);

	return 0;
}

static int msm_otg_link_clk_reset(struct msm_otg *motg, bool assert)
{
	int ret;

	if (assert) {
		/* Using asynchronous block reset to the hardware */
		dev_dbg(motg->phy.dev, "block_reset ASSERT\n");
		clk_disable_unprepare(motg->pclk);
		clk_disable_unprepare(motg->core_clk);
		ret = clk_reset(motg->core_clk, CLK_RESET_ASSERT);
		if (ret)
			dev_err(motg->phy.dev, "usb hs_clk assert failed\n");
	} else {
		dev_dbg(motg->phy.dev, "block_reset DEASSERT\n");
		ret = clk_reset(motg->core_clk, CLK_RESET_DEASSERT);
		ndelay(200);
		ret = clk_prepare_enable(motg->core_clk);
		WARN(ret, "USB core_clk enable failed\n");
		ret = clk_prepare_enable(motg->pclk);
		WARN(ret, "USB pclk enable failed\n");
		if (ret)
			dev_err(motg->phy.dev, "usb hs_clk deassert failed\n");
	}
	return ret;
}

static int msm_otg_phy_reset(struct msm_otg *motg)
{
	u32 val;
	int ret;
	struct msm_otg_platform_data *pdata = motg->pdata;

	/*
	 * AHB2AHB Bypass mode shouldn't be enable before doing
	 * async clock reset. If it is enable, disable the same.
	 */
	val = readl_relaxed(USB_AHBMODE);
	if (val & AHB2AHB_BYPASS) {
		pr_err("%s(): AHB2AHB_BYPASS SET: AHBMODE:%x\n",
						__func__, val);
		val &= ~AHB2AHB_BYPASS_BIT_MASK;
		writel_relaxed(val | AHB2AHB_BYPASS_CLEAR, USB_AHBMODE);
		pr_err("%s(): AHBMODE: %x\n", __func__,
				readl_relaxed(USB_AHBMODE));
	}

	ret = msm_otg_link_clk_reset(motg, 1);
	if (ret)
		return ret;

	msm_otg_phy_clk_reset(motg);

	/* wait for 1ms delay as suggested in HPG. */
	usleep_range(1000, 1200);

	ret = msm_otg_link_clk_reset(motg, 0);
	if (ret)
		return ret;

	if (pdata && pdata->enable_sec_phy)
		writel_relaxed(readl_relaxed(USB_PHY_CTRL2) | (1<<16),
							USB_PHY_CTRL2);
	val = readl_relaxed(USB_PORTSC) & ~PORTSC_PTS_MASK;
	writel_relaxed(val | PORTSC_PTS_ULPI, USB_PORTSC);

	dev_info(motg->phy.dev, "phy_reset: success\n");
	msm_otg_dbg_log_event(&motg->phy, "PHY RESET SUCCESS",
			motg->inputs, motg->phy.state);
	return 0;
}

#define LINK_RESET_TIMEOUT_USEC		(250 * 1000)
static int msm_otg_link_reset(struct msm_otg *motg)
{
	int cnt = 0;
	struct msm_otg_platform_data *pdata = motg->pdata;

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
	writel_relaxed(0x0, USB_AHBBURST);
	writel_relaxed(0x08, USB_AHBMODE);

	if (pdata && pdata->enable_sec_phy)
		writel_relaxed(readl_relaxed(USB_PHY_CTRL2) | (1<<16),
								USB_PHY_CTRL2);
	return 0;
}

#define QUSB2PHY_PORT_POWERDOWN		0xB4
#define QUSB2PHY_PORT_UTMI_CTRL2	0xC4

static void msm_usb_phy_reset(struct msm_otg *motg)
{
	u32 val;
	int ret, *seq;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
		/* Assert USB PHY_PON */
		val =  readl_relaxed(motg->usb_phy_ctrl_reg);
		val &= ~PHY_POR_BIT_MASK;
		val |= PHY_POR_ASSERT;
		writel_relaxed(val, motg->usb_phy_ctrl_reg);

		/* wait for minimum 10 microseconds as
		 * suggested in HPG.
		 */
		usleep_range(10, 15);

		/* Deassert USB PHY_PON */
		val =  readl_relaxed(motg->usb_phy_ctrl_reg);
		val &= ~PHY_POR_BIT_MASK;
		val |= PHY_POR_DEASSERT;
		writel_relaxed(val, motg->usb_phy_ctrl_reg);
		break;
	case QUSB_ULPI_PHY:
		ret = clk_reset(motg->phy_reset_clk, CLK_RESET_ASSERT);
		if (ret) {
			pr_err("phy_reset_clk assert failed %d\n", ret);
			break;
		}

		/* need to delay 10us for PHY to reset */
		usleep_range(10, 20);

		ret = clk_reset(motg->phy_reset_clk, CLK_RESET_DEASSERT);
		if (ret) {
			pr_err("phy_reset_clk de-assert failed %d\n", ret);
			break;
		}

		/* Ensure that RESET operation is completed. */
		mb();

		writel_relaxed(0x23,
				motg->phy_csr_regs + QUSB2PHY_PORT_POWERDOWN);
		writel_relaxed(0x0,
				motg->phy_csr_regs + QUSB2PHY_PORT_UTMI_CTRL2);

		/* Program tuning parameters for PHY */
		seq = motg->pdata->phy_init_seq;
		if (seq) {
			while (seq[0] >= 0) {
				writel_relaxed(seq[1],
						motg->phy_csr_regs + seq[0]);
				seq += 2;
			}
		}

		/* ensure above writes are completed before re-enabling PHY */
		wmb();
		writel_relaxed(0x22,
				motg->phy_csr_regs + QUSB2PHY_PORT_POWERDOWN);
		break;
	case SNPS_FEMTO_PHY:
		if (!motg->phy_por_clk) {
			pr_err("phy_por_clk missing\n");
			break;
		}
		ret = clk_reset(motg->phy_por_clk, CLK_RESET_ASSERT);
		if (ret) {
			pr_err("phy_por_clk assert failed %d\n", ret);
			break;
		}
		/*
		 * The Femto PHY is POR reset in the following scenarios.
		 *
		 * 1. After overriding the parameter registers.
		 * 2. Low power mode exit from PHY retention.
		 *
		 * Ensure that SIDDQ is cleared before bringing the PHY
		 * out of reset.
		 *
		 */

		val = readb_relaxed(USB_PHY_CSR_PHY_CTRL_COMMON0);
		val &= ~SIDDQ;
		writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL_COMMON0);

		/*
		 * As per databook, 10 usec delay is required between
		 * PHY POR assert and de-assert.
		 */
		usleep_range(10, 20);
		ret = clk_reset(motg->phy_por_clk, CLK_RESET_DEASSERT);
		if (ret) {
			pr_err("phy_por_clk de-assert failed %d\n", ret);
			break;
		}
		/*
		 * As per databook, it takes 75 usec for PHY to stabilize
		 * after the reset.
		 */
		usleep_range(80, 100);
		break;
	default:
		break;
	}
	/* Ensure that RESET operation is completed. */
	mb();
}

static int msm_otg_reset(struct usb_phy *phy)
{
	struct msm_otg *motg = container_of(phy, struct msm_otg, phy);
	struct msm_otg_platform_data *pdata = motg->pdata;
	int ret;
	u32 val = 0;
	u32 ulpi_val = 0;

	msm_otg_dbg_log_event(&motg->phy, "USB RESET", phy->state,
			get_pm_runtime_counter(phy->dev));
	/*
	 * USB PHY and Link reset also reset the USB BAM.
	 * Thus perform reset operation only once to avoid
	 * USB BAM reset on other cases e.g. USB cable disconnections.
	 * If hardware reported error then it must be reset for recovery.
	 */
	if (motg->err_event_seen) {
		dev_info(phy->dev, "performing USB h/w reset for recovery\n");
	} else if (pdata->disable_reset_on_disconnect && motg->reset_counter) {
		return 0;
	}
	motg->reset_counter++;

	disable_irq(motg->irq);
	if (motg->phy_irq)
		disable_irq(motg->phy_irq);

	ret = msm_otg_phy_reset(motg);
	if (ret) {
		dev_err(phy->dev, "phy_reset failed\n");
		if (motg->phy_irq)
			enable_irq(motg->phy_irq);

		enable_irq(motg->irq);
		return ret;
	}

	if (motg->phy_irq)
		enable_irq(motg->phy_irq);

	enable_irq(motg->irq);
	ret = msm_otg_link_reset(motg);
	if (ret) {
		dev_err(phy->dev, "link reset failed\n");
		return ret;
	}

	msleep(100);

	/* Reset USB PHY after performing USB Link RESET */
	msm_usb_phy_reset(motg);

	/* Program USB PHY Override registers. */
	ulpi_init(motg);

	/*
	 * It is required to reset USB PHY after programming
	 * the USB PHY Override registers to get the new
	 * values into effect.
	 */
	msm_usb_phy_reset(motg);

	if (pdata->otg_control == OTG_PHY_CONTROL) {
		val = readl_relaxed(USB_OTGSC);
		if (pdata->mode == USB_OTG) {
			ulpi_val = ULPI_INT_IDGRD | ULPI_INT_SESS_VALID;
			val |= OTGSC_IDIE | OTGSC_BSVIE;
		} else if (pdata->mode == USB_PERIPHERAL) {
			ulpi_val = ULPI_INT_SESS_VALID;
			val |= OTGSC_BSVIE;
		}
		writel_relaxed(val, USB_OTGSC);
		ulpi_write(phy, ulpi_val, ULPI_USB_INT_EN_RISE);
		ulpi_write(phy, ulpi_val, ULPI_USB_INT_EN_FALL);
	} else if (pdata->otg_control == OTG_PMIC_CONTROL) {
		ulpi_write(phy, OTG_COMP_DISABLE,
			ULPI_SET(ULPI_PWR_CLK_MNG_REG));
		if (motg->phy_irq)
			writeb_relaxed(USB_PHY_ID_MASK,
				USB2_PHY_USB_PHY_INTERRUPT_MASK1);
	}

	if (motg->caps & ALLOW_VDD_MIN_WITH_RETENTION_DISABLED)
		writel_relaxed(readl_relaxed(USB_OTGSC) & ~(OTGSC_IDPU),
				USB_OTGSC);

	msm_otg_dbg_log_event(&motg->phy, "USB RESET DONE", phy->state,
			get_pm_runtime_counter(phy->dev));

	if (pdata->enable_axi_prefetch)
		writel_relaxed(readl_relaxed(USB_HS_APF_CTRL) | (APF_CTRL_EN),
							USB_HS_APF_CTRL);

	/*
	 * Disable USB BAM as block reset resets USB BAM registers.
	 */
	msm_usb_bam_enable(CI_CTRL, false);

	return 0;
}

static void msm_otg_kick_sm_work(struct msm_otg *motg)
{
	if (atomic_read(&motg->in_lpm))
		motg->resume_pending = true;

	/* For device mode, resume now. Let pm_resume handle other cases */
	if (atomic_read(&motg->pm_suspended) &&
			motg->phy.state != OTG_STATE_B_SUSPEND) {
		motg->sm_work_pending = true;
	} else if (!motg->sm_work_pending) {
		/* process event only if previous one is not pending */
		queue_work(motg->otg_wq, &motg->sm_work);
	}
}

/*
 * UDC calls usb_phy_set_suspend() to notify during bus suspend/resume.
 * Update relevant state-machine inputs and queue sm_work.
 * LPM enter/exit doesn't happen directly from this routine.
 */

static int msm_otg_set_suspend(struct usb_phy *phy, int suspend)
{
	struct msm_otg *motg = container_of(phy, struct msm_otg, phy);

	pr_debug("%s(%d) in %s state\n", __func__, suspend,
				usb_otg_state_string(phy->state));
	msm_otg_dbg_log_event(phy, "SET SUSPEND", suspend, phy->state);

	if (!(motg->caps & ALLOW_LPM_ON_DEV_SUSPEND))
		return 0;

	if (suspend) {
		/* called in suspend interrupt context */
		pr_debug("peripheral bus suspend\n");
		msm_otg_dbg_log_event(phy, "PERIPHERAL BUS SUSPEND",
				motg->inputs, phy->state);

		set_bit(A_BUS_SUSPEND, &motg->inputs);
	} else {
		/* host resume or remote-wakeup */
		pr_debug("peripheral bus resume\n");
		msm_otg_dbg_log_event(phy, "PERIPHERAL BUS RESUME",
				motg->inputs, phy->state);

		clear_bit(A_BUS_SUSPEND, &motg->inputs);
	}
	/* use kick_sm_work to handle race with pm_resume */
	msm_otg_kick_sm_work(motg);

	return 0;
}

static int msm_otg_bus_freq_set(struct msm_otg *motg, enum usb_noc_mode mode)
{
	int i, ret;
	long rate;

	for (i = 0; i < USB_NUM_BUS_CLOCKS; i++) {
		rate = bus_freqs[mode][i];
		if (!rate) {
			pr_debug("%s rate not available\n", bus_clkname[i]);
			continue;
		}

		ret = clk_set_rate(motg->bus_clks[i], rate);
		if (ret) {
			pr_err("%s set rate failed: %d\n", bus_clkname[i], ret);
			return ret;
		}
		pr_debug("%s set to %lu Hz\n", bus_clkname[i],
			 clk_get_rate(motg->bus_clks[i]));
		msm_otg_dbg_log_event(&motg->phy, "OTG BUS FREQ SET", i, rate);
	}

	bus_clk_rate_set = true;

	return 0;
}

static int msm_otg_bus_freq_get(struct msm_otg *motg)
{
	struct device *dev = motg->phy.dev;
	struct device_node *np = dev->of_node;
	int len = 0, i, count = USB_NUM_BUS_CLOCKS;

	if (!np)
		return -EINVAL;

	of_find_property(np, "qcom,bus-clk-rate", &len);
	/* SVS requires extra set of frequencies for perf_mode sysfs node */
	if (motg->default_noc_mode == USB_NOC_SVS_VOTE)
		count *= 2;

	if (!len || (len / sizeof(u32) != count)) {
		pr_err("Invalid bus rate:%d %u\n", len, motg->default_noc_mode);
		return -EINVAL;
	}
	of_property_read_u32_array(np, "qcom,bus-clk-rate", bus_freqs[0],
				   count);
	for (i = 0; i < USB_NUM_BUS_CLOCKS; i++) {
		if (bus_freqs[0][i] == 0) {
			motg->bus_clks[i] = NULL;
			pr_debug("%s not available\n", bus_clkname[i]);
			continue;
		}

		motg->bus_clks[i] = devm_clk_get(dev, bus_clkname[i]);
		if (IS_ERR(motg->bus_clks[i])) {
			pr_err("%s get failed\n", bus_clkname[i]);
			return PTR_ERR(motg->bus_clks[i]);
		}
	}
	return 0;
}

static void msm_otg_bus_clks_enable(struct msm_otg *motg)
{
	int i;
	int ret;

	if (!bus_clk_rate_set || motg->bus_clks_enabled)
		return;

	for (i = 0; i < USB_NUM_BUS_CLOCKS; i++) {
		if (motg->bus_clks[i] == NULL)
			continue;
		ret = clk_prepare_enable(motg->bus_clks[i]);
		if (ret) {
			pr_err("%s enable rate failed: %d\n", bus_clkname[i],
				ret);
			goto err_clk_en;
		}
	}
	motg->bus_clks_enabled = true;
	return;
err_clk_en:
	for (--i; i >= 0; --i) {
		if (motg->bus_clks[i] != NULL)
			clk_disable_unprepare(motg->bus_clks[i]);
	}
}

static void msm_otg_bus_clks_disable(struct msm_otg *motg)
{
	int i;

	if (!bus_clk_rate_set || !motg->bus_clks_enabled)
		return;

	for (i = 0; i < USB_NUM_BUS_CLOCKS; i++) {
		if (motg->bus_clks[i] != NULL)
			clk_disable_unprepare(motg->bus_clks[i]);
	}
	motg->bus_clks_enabled = false;
}

static void msm_otg_bus_vote(struct msm_otg *motg, enum usb_bus_vote vote)
{
	int ret;
	struct msm_otg_platform_data *pdata = motg->pdata;

	msm_otg_dbg_log_event(&motg->phy, "BUS VOTE", vote, motg->phy.state);
	/* Check if target allows min_vote to be same as no_vote */
	if (pdata->bus_scale_table &&
	    vote >= pdata->bus_scale_table->num_usecases)
		vote = USB_NO_PERF_VOTE;

	if (motg->bus_perf_client) {
		ret = msm_bus_scale_client_update_request(
			motg->bus_perf_client, vote);
		if (ret)
			dev_err(motg->phy.dev, "%s: Failed to vote (%d)\n"
				   "for bus bw %d\n", __func__, vote, ret);
	}

	if (vote == USB_MAX_PERF_VOTE)
		msm_otg_bus_clks_enable(motg);
	else
		msm_otg_bus_clks_disable(motg);
}

static void msm_otg_enable_phy_hv_int(struct msm_otg *motg)
{
	bool bsv_id_hv_int = false;
	bool dp_dm_hv_int = false;
	u32 val;

	if (motg->pdata->otg_control == OTG_PHY_CONTROL ||
				motg->phy_irq)
		bsv_id_hv_int = true;
	if (motg->host_bus_suspend || motg->device_bus_suspend)
		dp_dm_hv_int = true;

	if (!bsv_id_hv_int && !dp_dm_hv_int)
		return;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
		val = readl_relaxed(motg->usb_phy_ctrl_reg);
		if (bsv_id_hv_int)
			val |= (PHY_IDHV_INTEN | PHY_OTGSESSVLDHV_INTEN);
		if (dp_dm_hv_int)
			val |= PHY_CLAMP_DPDMSE_EN;
		writel_relaxed(val, motg->usb_phy_ctrl_reg);
		break;
	case SNPS_FEMTO_PHY:
		if (bsv_id_hv_int) {
			val = readb_relaxed(USB_PHY_CSR_PHY_CTRL1);
			val |= ID_HV_CLAMP_EN_N;
			writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL1);
		}

		if (dp_dm_hv_int) {
			val = readb_relaxed(USB_PHY_CSR_PHY_CTRL3);
			val |= CLAMP_MPM_DPSE_DMSE_EN_N;
			writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL3);
		}
	default:
		break;
	}
	pr_debug("%s: bsv_id_hv = %d dp_dm_hv_int = %d\n",
			__func__, bsv_id_hv_int, dp_dm_hv_int);
	msm_otg_dbg_log_event(&motg->phy, "PHY HV INTR ENABLED",
			bsv_id_hv_int, dp_dm_hv_int);
}

static void msm_otg_disable_phy_hv_int(struct msm_otg *motg)
{
	bool bsv_id_hv_int = false;
	bool dp_dm_hv_int = false;
	u32 val;

	if (motg->pdata->otg_control == OTG_PHY_CONTROL ||
				motg->phy_irq)
		bsv_id_hv_int = true;
	if (motg->host_bus_suspend || motg->device_bus_suspend)
		dp_dm_hv_int = true;

	if (!bsv_id_hv_int && !dp_dm_hv_int)
		return;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
		val = readl_relaxed(motg->usb_phy_ctrl_reg);
		if (bsv_id_hv_int)
			val &= ~(PHY_IDHV_INTEN | PHY_OTGSESSVLDHV_INTEN);
		if (dp_dm_hv_int)
			val &= ~PHY_CLAMP_DPDMSE_EN;
		writel_relaxed(val, motg->usb_phy_ctrl_reg);
		break;
	case SNPS_FEMTO_PHY:
		if (bsv_id_hv_int) {
			val = readb_relaxed(USB_PHY_CSR_PHY_CTRL1);
			val &= ~ID_HV_CLAMP_EN_N;
			writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL1);
		}

		if (dp_dm_hv_int) {
			val = readb_relaxed(USB_PHY_CSR_PHY_CTRL3);
			val &= ~CLAMP_MPM_DPSE_DMSE_EN_N;
			writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL3);
		}
		break;
	default:
		break;
	}
	pr_debug("%s: bsv_id_hv = %d dp_dm_hv_int = %d\n",
			__func__, bsv_id_hv_int, dp_dm_hv_int);
	msm_otg_dbg_log_event(&motg->phy, "PHY HV INTR DISABLED",
			bsv_id_hv_int, dp_dm_hv_int);
}

static void msm_otg_enter_phy_retention(struct msm_otg *motg)
{
	u32 val;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
		val = readl_relaxed(motg->usb_phy_ctrl_reg);
		val &= ~PHY_RETEN;
		writel_relaxed(val, motg->usb_phy_ctrl_reg);
		break;
	case SNPS_FEMTO_PHY:
		/* Retention is supported via SIDDQ */
		val = readb_relaxed(USB_PHY_CSR_PHY_CTRL_COMMON0);
		val |= SIDDQ;
		writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL_COMMON0);
		break;
	default:
		break;
	}
	pr_debug("USB PHY is in retention\n");
	msm_otg_dbg_log_event(&motg->phy, "USB PHY ENTER RETENTION",
			motg->pdata->phy_type, 0);
}

static void msm_otg_exit_phy_retention(struct msm_otg *motg)
{
	int val;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
		val = readl_relaxed(motg->usb_phy_ctrl_reg);
		val |= PHY_RETEN;
		writel_relaxed(val, motg->usb_phy_ctrl_reg);
		break;
	case SNPS_FEMTO_PHY:
		/*
		 * It is required to do USB block reset to bring Femto PHY out
		 * of retention.
		 */
		msm_otg_reset(&motg->phy);
		break;
	default:
		break;
	}
	pr_debug("USB PHY is exited from retention\n");
	msm_otg_dbg_log_event(&motg->phy, "USB PHY EXIT RETENTION",
			motg->pdata->phy_type, 0);
}

static void msm_id_status_w(struct work_struct *w);
static irqreturn_t msm_otg_phy_irq_handler(int irq, void *data)
{
	struct msm_otg *motg = data;

	msm_otg_dbg_log_event(&motg->phy, "PHY ID IRQ",
			atomic_read(&motg->in_lpm), motg->phy.state);
	if (atomic_read(&motg->in_lpm)) {
		pr_debug("PHY ID IRQ in LPM\n");
		motg->phy_irq_pending = true;
		msm_otg_kick_sm_work(motg);
	} else {
		pr_debug("PHY ID IRQ outside LPM\n");
		msm_id_status_w(&motg->id_status_work.work);
	}

	return IRQ_HANDLED;
}

#define PHY_SUSPEND_TIMEOUT_USEC (5 * 1000)
#define PHY_DEVICE_BUS_SUSPEND_TIMEOUT_USEC 100
#define PHY_RESUME_TIMEOUT_USEC	(100 * 1000)

#define PHY_SUSPEND_RETRIES_MAX 3

static void msm_otg_set_vbus_state(int online);
static void msm_otg_perf_vote_update(struct msm_otg *motg, bool perf_mode);

#ifdef CONFIG_PM_SLEEP
static int msm_otg_suspend(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;
	struct usb_bus *bus = phy->otg->host;
	struct msm_otg_platform_data *pdata = motg->pdata;
	int cnt;
	bool host_bus_suspend, device_bus_suspend, dcp, prop_charger;
	bool floated_charger, sm_work_busy;
	u32 cmd_val;
	u32 portsc, config2;
	u32 func_ctrl;
	int phcd_retry_cnt = 0, ret;
	unsigned phy_suspend_timeout;

	cnt = 0;
	msm_otg_dbg_log_event(phy, "LPM ENTER START",
			motg->inputs, phy->state);

	if (atomic_read(&motg->in_lpm))
		return 0;

	cancel_delayed_work_sync(&motg->perf_vote_work);

	disable_irq(motg->irq);
	if (motg->phy_irq)
		disable_irq(motg->phy_irq);
lpm_start:
	host_bus_suspend = phy->otg->host && !test_bit(ID, &motg->inputs);
	device_bus_suspend = phy->otg->gadget && test_bit(ID, &motg->inputs) &&
		test_bit(A_BUS_SUSPEND, &motg->inputs) &&
		motg->caps & ALLOW_LPM_ON_DEV_SUSPEND;

	if (host_bus_suspend)
		msm_otg_perf_vote_update(motg, false);
	/*
	 * Allow putting PHY into SIDDQ with wall charger connected in
	 * case of external charger detection.
	 */
	dcp = (motg->chg_type == USB_DCP_CHARGER) && !motg->is_ext_chg_dcp;
	prop_charger = motg->chg_type == USB_PROPRIETARY_CHARGER;
	floated_charger = motg->chg_type == USB_FLOATED_CHARGER;

	/* !BSV, but its handling is in progress by otg sm_work */
	sm_work_busy = !test_bit(B_SESS_VLD, &motg->inputs) &&
			phy->state == OTG_STATE_B_PERIPHERAL;

	/* Perform block reset to recover from UDC error events on disconnect */
	if (motg->err_event_seen)
		msm_otg_reset(phy);

	/* Enable line state difference wakeup fix for only device and host
	 * bus suspend scenarios.  Otherwise PHY can not be suspended when
	 * a charger that pulls DP/DM high is connected.
	 */
	config2 = readl_relaxed(USB_GENCONFIG2);
	if (device_bus_suspend)
		config2 |= GENCFG2_LINESTATE_DIFF_WAKEUP_EN;
	else
		config2 &= ~GENCFG2_LINESTATE_DIFF_WAKEUP_EN;
	writel_relaxed(config2, USB_GENCONFIG2);

	/*
	 * Abort suspend when,
	 * 1. charging detection in progress due to cable plug-in
	 * 2. host mode activation in progress due to Micro-A cable insertion
	 * 3. !BSV, but its handling is in progress by otg sm_work
	 * Don't abort suspend in case of dcp detected by PMIC
	 */

	if ((test_bit(B_SESS_VLD, &motg->inputs) && !device_bus_suspend &&
		!dcp && !motg->is_ext_chg_dcp && !prop_charger &&
			!floated_charger) || sm_work_busy) {
		msm_otg_dbg_log_event(phy, "LPM ENTER ABORTED",
				motg->inputs, motg->chg_type);
		enable_irq(motg->irq);
		if (motg->phy_irq)
			enable_irq(motg->phy_irq);
		return -EBUSY;
	}

	if (motg->caps & ALLOW_VDD_MIN_WITH_RETENTION_DISABLED) {
		/* put the controller in non-driving mode */
		func_ctrl = ulpi_read(phy, ULPI_FUNC_CTRL);
		func_ctrl &= ~ULPI_FUNC_CTRL_OPMODE_MASK;
		func_ctrl |= ULPI_FUNC_CTRL_OPMODE_NONDRIVING;
		ulpi_write(phy, func_ctrl, ULPI_FUNC_CTRL);
		ulpi_write(phy, ULPI_IFC_CTRL_AUTORESUME,
						ULPI_CLR(ULPI_IFC_CTRL));
	}

	/*
	 * PHY suspend sequence as mentioned in the databook.
	 *
	 * Device bus suspend: The controller may abort PHY suspend if
	 * there is an incoming reset or resume from the host. If PHCD
	 * is not set within 100 usec. Abort the LPM sequence.
	 *
	 * Host bus suspend: If the peripheral is attached, PHY is already
	 * put into suspend along with the peripheral bus suspend. poll for
	 * PHCD upto 5 msec. If the peripheral is not attached i.e entering
	 * LPM with Micro-A cable, set the PHCD and poll for it for 5 msec.
	 *
	 * No cable connected: Set the PHCD to suspend the PHY. Poll for PHCD
	 * upto 5 msec.
	 *
	 * The controller aborts PHY suspend only in device bus suspend case.
	 * In other cases, it is observed that PHCD may not get set within
	 * the timeout. If so, set the PHCD again and poll for it before
	 * reset recovery.
	 */

phcd_retry:
	if (device_bus_suspend)
		phy_suspend_timeout = PHY_DEVICE_BUS_SUSPEND_TIMEOUT_USEC;
	else
		phy_suspend_timeout = PHY_SUSPEND_TIMEOUT_USEC;

	cnt = 0;
	portsc = readl_relaxed(USB_PORTSC);
	if (!(portsc & PORTSC_PHCD)) {
		writel_relaxed(portsc | PORTSC_PHCD,
				USB_PORTSC);
		while (cnt < phy_suspend_timeout) {
			if (readl_relaxed(USB_PORTSC) & PORTSC_PHCD)
				break;
			udelay(1);
			cnt++;
		}
	}

	if (cnt >= phy_suspend_timeout) {
		if (phcd_retry_cnt > PHY_SUSPEND_RETRIES_MAX) {
			msm_otg_dbg_log_event(phy, "PHY SUSPEND FAILED",
				phcd_retry_cnt, phy->state);
			dev_err(phy->dev, "PHY suspend failed\n");
			ret = -EBUSY;
			goto phy_suspend_fail;
		}

		if (device_bus_suspend) {
			dev_dbg(phy->dev, "PHY suspend aborted\n");
			ret = -EBUSY;
			goto phy_suspend_fail;
		} else {
			if (phcd_retry_cnt++ < PHY_SUSPEND_RETRIES_MAX) {
				dev_dbg(phy->dev, "PHY suspend retry\n");
				goto phcd_retry;
			} else {
				dev_err(phy->dev, "reset attempt during PHY suspend\n");
				phcd_retry_cnt++;
				motg->reset_counter = 0;
				msm_otg_reset(phy);
				goto lpm_start;
			}
		}
	}

	/*
	 * PHY has capability to generate interrupt asynchronously in low
	 * power mode (LPM). This interrupt is level triggered. So USB IRQ
	 * line must be disabled till async interrupt enable bit is cleared
	 * in USBCMD register. Assert STP (ULPI interface STOP signal) to
	 * block data communication from PHY.
	 *
	 * PHY retention mode is disallowed while entering to LPM with wall
	 * charger connected.  But PHY is put into suspend mode. Hence
	 * enable asynchronous interrupt to detect charger disconnection when
	 * PMIC notifications are unavailable.
	 */
	cmd_val = readl_relaxed(USB_USBCMD);
	if (host_bus_suspend || device_bus_suspend ||
		(motg->pdata->otg_control == OTG_PHY_CONTROL))
		cmd_val |= ASYNC_INTR_CTRL | ULPI_STP_CTRL;
	else
		cmd_val |= ULPI_STP_CTRL;
	writel_relaxed(cmd_val, USB_USBCMD);

	/*
	 * BC1.2 spec mandates PD to enable VDP_SRC when charging from DCP.
	 * PHY retention and collapse can not happen with VDP_SRC enabled.
	 */


	/*
	 * We come here in 3 scenarios.
	 *
	 * (1) No cable connected (out of session):
	 *	- BSV/ID HV interrupts are enabled for PHY based detection.
	 *	- PHY is put in retention.
	 *	- If allowed (PMIC based detection), PHY is power collapsed.
	 *	- DVDD (CX/MX) minimization and XO shutdown are allowed.
	 *	- The wakeup is through VBUS/ID interrupt from PHY/PMIC/user.
	 * (2) USB wall charger:
	 *	- BSV/ID HV interrupts are enabled for PHY based detection.
	 *	- For BC1.2 compliant charger, retention is not allowed to
	 *	keep VDP_SRC on. XO shutdown is allowed.
	 *	- The wakeup is through VBUS/ID interrupt from PHY/PMIC/user.
	 * (3) Device/Host Bus suspend (if LPM is enabled):
	 *	- BSV/ID HV interrupts are enabled for PHY based detection.
	 *	- D+/D- MPM pin are configured to wakeup from line state
	 *	change through PHY HV interrupts. PHY HV interrupts are
	 *	also enabled. If MPM pins are not available, retention and
	 *	XO is not allowed.
	 *	- PHY is put into retention only if a gpio is used to keep
	 *	the D+ pull-up. ALLOW_BUS_SUSPEND_WITHOUT_REWORK capability
	 *	is set means, PHY can enable D+ pull-up or D+/D- pull-down
	 *	without any re-work and PHY should not be put into retention.
	 *	- DVDD (CX/MX) minimization and XO shutdown is allowed if
	 *	ALLOW_BUS_SUSPEND_WITHOUT_REWORK is set (PHY DVDD is supplied
	 *	via PMIC LDO) or board level re-work is present.
	 *	- The wakeup is through VBUS/ID interrupt from PHY/PMIC/user
	 *	or USB link asynchronous interrupt for line state change.
	 *
	 */
	motg->host_bus_suspend = host_bus_suspend;
	motg->device_bus_suspend = device_bus_suspend;

	if (motg->caps & ALLOW_PHY_RETENTION && !device_bus_suspend && !dcp &&
		 (!host_bus_suspend || (motg->caps &
		ALLOW_BUS_SUSPEND_WITHOUT_REWORK) ||
		  ((motg->caps & ALLOW_HOST_PHY_RETENTION)
		&& (pdata->dpdm_pulldown_added || !(portsc & PORTSC_CCS))))) {
		msm_otg_enable_phy_hv_int(motg);
		if ((!host_bus_suspend || !(motg->caps &
			ALLOW_BUS_SUSPEND_WITHOUT_REWORK)) &&
			!(motg->caps & ALLOW_VDD_MIN_WITH_RETENTION_DISABLED)) {
			msm_otg_enter_phy_retention(motg);
			motg->lpm_flags |= PHY_RETENTIONED;
		}
	} else if (device_bus_suspend && !dcp &&
			(pdata->mpm_dpshv_int || pdata->mpm_dmshv_int)) {
		/* DP DM HV interrupts are used for bus resume from XO off */
		msm_otg_enable_phy_hv_int(motg);
		if (motg->caps & ALLOW_PHY_RETENTION && pdata->vddmin_gpio) {

			/*
			 * This is HW WA needed when PHY_CLAMP_DPDMSE_EN is
			 * enabled and we put the phy in retention mode.
			 * Without this WA, the async_irq will be fired right
			 * after suspending whithout any bus resume.
			 */
			config2 = readl_relaxed(USB_GENCONFIG2);
			config2 &= ~GENCFG2_DPSE_DMSE_HV_INTR_EN;
			writel_relaxed(config2, USB_GENCONFIG2);

			msm_otg_enter_phy_retention(motg);
			motg->lpm_flags |= PHY_RETENTIONED;
			gpio_direction_output(pdata->vddmin_gpio, 1);
		}
	}

	/* Ensure that above operation is completed before turning off clocks */
	mb();
	/* Consider clocks on workaround flag only in case of bus suspend */
	if (!(phy->state == OTG_STATE_B_PERIPHERAL &&
			test_bit(A_BUS_SUSPEND, &motg->inputs)) ||
			!motg->pdata->core_clk_always_on_workaround) {
		clk_disable_unprepare(motg->pclk);
		clk_disable_unprepare(motg->core_clk);
		if (motg->phy_csr_clk)
			clk_disable_unprepare(motg->phy_csr_clk);
		motg->lpm_flags |= CLOCKS_DOWN;
	}

	/* usb phy no more require TCXO clock, hence vote for TCXO disable */
	if (!host_bus_suspend || (motg->caps &
		ALLOW_BUS_SUSPEND_WITHOUT_REWORK) ||
		((motg->caps & ALLOW_HOST_PHY_RETENTION) &&
		(pdata->dpdm_pulldown_added || !(portsc & PORTSC_CCS)))) {
		if (motg->xo_clk) {
			clk_disable_unprepare(motg->xo_clk);
			motg->lpm_flags |= XO_SHUTDOWN;
		}
	}

	if (motg->caps & ALLOW_PHY_POWER_COLLAPSE &&
			!host_bus_suspend && !dcp && !device_bus_suspend) {
		msm_hsusb_ldo_enable(motg, USB_PHY_REG_OFF);
		motg->lpm_flags |= PHY_PWR_COLLAPSED;
	} else if (motg->caps & ALLOW_PHY_REGULATORS_LPM &&
			!host_bus_suspend && !device_bus_suspend && !dcp) {
		msm_hsusb_ldo_enable(motg, USB_PHY_REG_LPM_ON);
		motg->lpm_flags |= PHY_REGULATORS_LPM;
	}

	if (motg->lpm_flags & PHY_RETENTIONED ||
		(motg->caps & ALLOW_VDD_MIN_WITH_RETENTION_DISABLED)) {
		regulator_disable(hsusb_vdd);
		msm_hsusb_config_vddcx(0);
	}

	if (device_may_wakeup(phy->dev)) {
		if (host_bus_suspend || device_bus_suspend) {
			enable_irq_wake(motg->async_irq);
			enable_irq_wake(motg->irq);
		}

		if (motg->phy_irq)
			enable_irq_wake(motg->phy_irq);
		if (motg->pdata->pmic_id_irq)
			enable_irq_wake(motg->pdata->pmic_id_irq);
		if (motg->ext_id_irq)
			enable_irq_wake(motg->ext_id_irq);
		if (pdata->otg_control == OTG_PHY_CONTROL &&
			pdata->mpm_otgsessvld_int)
			msm_mpm_set_pin_wake(pdata->mpm_otgsessvld_int, 1);
		if ((host_bus_suspend || device_bus_suspend) &&
				pdata->mpm_dpshv_int)
			msm_mpm_set_pin_wake(pdata->mpm_dpshv_int, 1);
		if ((host_bus_suspend || device_bus_suspend) &&
				pdata->mpm_dmshv_int)
			msm_mpm_set_pin_wake(pdata->mpm_dmshv_int, 1);
	}
	if (bus)
		clear_bit(HCD_FLAG_HW_ACCESSIBLE, &(bus_to_hcd(bus))->flags);

	msm_otg_bus_vote(motg, USB_NO_PERF_VOTE);

	atomic_set(&motg->in_lpm, 1);

	/* Enable ASYNC IRQ during LPM */
	enable_irq(motg->async_irq);
	if (motg->phy_irq)
		enable_irq(motg->phy_irq);

	enable_irq(motg->irq);
	wake_unlock(&motg->wlock);

	dev_dbg(phy->dev, "LPM caps = %lu flags = %lu\n",
			motg->caps, motg->lpm_flags);
	dev_info(phy->dev, "USB in low power mode\n");
	msm_otg_dbg_log_event(phy, "LPM ENTER DONE",
			motg->caps, motg->lpm_flags);

	if (motg->err_event_seen) {
		motg->err_event_seen = false;
		if (motg->vbus_state != test_bit(B_SESS_VLD, &motg->inputs))
			msm_otg_set_vbus_state(motg->vbus_state);
		if (motg->id_state != test_bit(ID, &motg->inputs))
			msm_id_status_w(&motg->id_status_work.work);
	}

	return 0;

phy_suspend_fail:
	enable_irq(motg->irq);
	if (motg->phy_irq)
		enable_irq(motg->phy_irq);
	return ret;
}

static int msm_otg_resume(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;
	struct usb_bus *bus = phy->otg->host;
	struct usb_hcd *hcd = bus_to_hcd(phy->otg->host);
	struct msm_otg_platform_data *pdata = motg->pdata;
	int cnt = 0;
	unsigned temp;
	unsigned ret;
	u32 func_ctrl;

	msm_otg_dbg_log_event(phy, "LPM EXIT START", motg->inputs, phy->state);
	if (!atomic_read(&motg->in_lpm)) {
		msm_otg_dbg_log_event(phy, "USB NOT IN LPM",
				atomic_read(&motg->in_lpm), phy->state);
		return 0;
	}

	disable_irq(motg->irq);
	if (motg->phy_irq)
		disable_irq(motg->phy_irq);
	wake_lock(&motg->wlock);

	/*
	 * If we are resuming from the device bus suspend, restore
	 * the max performance bus vote. Otherwise put a minimum
	 * bus vote to satisfy the requirement for enabling clocks.
	 */

	if (motg->device_bus_suspend && debug_bus_voting_enabled)
		msm_otg_bus_vote(motg, USB_MAX_PERF_VOTE);
	else
		msm_otg_bus_vote(motg, USB_MIN_PERF_VOTE);

	/* Vote for TCXO when waking up the phy */
	if (motg->lpm_flags & XO_SHUTDOWN) {
		if (motg->xo_clk)
			clk_prepare_enable(motg->xo_clk);
		motg->lpm_flags &= ~XO_SHUTDOWN;
	}

	if (motg->lpm_flags & CLOCKS_DOWN) {
		if (motg->phy_csr_clk) {
			ret = clk_prepare_enable(motg->phy_csr_clk);
			WARN(ret, "USB phy_csr_clk enable failed\n");
		}
		ret = clk_prepare_enable(motg->core_clk);
		WARN(ret, "USB core_clk enable failed\n");
		ret = clk_prepare_enable(motg->pclk);
		WARN(ret, "USB pclk enable failed\n");
		motg->lpm_flags &= ~CLOCKS_DOWN;
	}

	if (motg->lpm_flags & PHY_PWR_COLLAPSED) {
		msm_hsusb_ldo_enable(motg, USB_PHY_REG_ON);
		motg->lpm_flags &= ~PHY_PWR_COLLAPSED;
	} else if (motg->lpm_flags & PHY_REGULATORS_LPM) {
		msm_hsusb_ldo_enable(motg, USB_PHY_REG_LPM_OFF);
		motg->lpm_flags &= ~PHY_REGULATORS_LPM;
	}

	if (motg->lpm_flags & PHY_RETENTIONED ||
		(motg->caps & ALLOW_VDD_MIN_WITH_RETENTION_DISABLED)) {
		msm_hsusb_config_vddcx(1);
		ret = regulator_enable(hsusb_vdd);
		WARN(ret, "hsusb_vdd LDO enable failed\n");
		msm_otg_disable_phy_hv_int(motg);
		msm_otg_exit_phy_retention(motg);
		motg->lpm_flags &= ~PHY_RETENTIONED;
		if (pdata->vddmin_gpio && motg->device_bus_suspend)
			gpio_direction_input(pdata->vddmin_gpio);
	} else if (motg->device_bus_suspend) {
		msm_otg_disable_phy_hv_int(motg);
	}

	temp = readl_relaxed(USB_USBCMD);
	temp &= ~ASYNC_INTR_CTRL;
	temp &= ~ULPI_STP_CTRL;
	writel_relaxed(temp, USB_USBCMD);

	/*
	 * PHY comes out of low power mode (LPM) in case of wakeup
	 * from asynchronous interrupt.
	 */
	if (!(readl_relaxed(USB_PORTSC) & PORTSC_PHCD))
		goto skip_phy_resume;

	writel_relaxed(readl_relaxed(USB_PORTSC) & ~PORTSC_PHCD, USB_PORTSC);

	while (cnt < PHY_RESUME_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_PORTSC) & PORTSC_PHCD))
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= PHY_RESUME_TIMEOUT_USEC) {
		/*
		 * This is a fatal error. Reset the link and
		 * PHY. USB state can not be restored. Re-insertion
		 * of USB cable is the only way to get USB working.
		 */
		dev_err(phy->dev, "Unable to resume USB."
				"Re-plugin the cable\n");
		msm_otg_reset(phy);
	}

skip_phy_resume:
	if (motg->caps & ALLOW_VDD_MIN_WITH_RETENTION_DISABLED) {
		/* put the controller in normal mode */
		func_ctrl = ulpi_read(phy, ULPI_FUNC_CTRL);
		func_ctrl &= ~ULPI_FUNC_CTRL_OPMODE_MASK;
		func_ctrl |= ULPI_FUNC_CTRL_OPMODE_NORMAL;
		ulpi_write(phy, func_ctrl, ULPI_FUNC_CTRL);
	}

	if (device_may_wakeup(phy->dev)) {
		if (motg->host_bus_suspend || motg->device_bus_suspend) {
			disable_irq_wake(motg->async_irq);
			disable_irq_wake(motg->irq);
		}

		if (motg->phy_irq)
			disable_irq_wake(motg->phy_irq);
		if (motg->pdata->pmic_id_irq)
			disable_irq_wake(motg->pdata->pmic_id_irq);
		if (motg->ext_id_irq)
			disable_irq_wake(motg->ext_id_irq);
		if (pdata->otg_control == OTG_PHY_CONTROL &&
			pdata->mpm_otgsessvld_int)
			msm_mpm_set_pin_wake(pdata->mpm_otgsessvld_int, 0);
		if ((motg->host_bus_suspend || motg->device_bus_suspend) &&
			pdata->mpm_dpshv_int)
			msm_mpm_set_pin_wake(pdata->mpm_dpshv_int, 0);
		if ((motg->host_bus_suspend || motg->device_bus_suspend) &&
			pdata->mpm_dmshv_int)
			msm_mpm_set_pin_wake(pdata->mpm_dmshv_int, 0);
	}
	if (bus)
		set_bit(HCD_FLAG_HW_ACCESSIBLE, &(bus_to_hcd(bus))->flags);

	atomic_set(&motg->in_lpm, 0);

	if (motg->async_int) {
		/* Match the disable_irq call from ISR */
		enable_irq(motg->async_int);
		motg->async_int = 0;
	}
	if (motg->phy_irq)
		enable_irq(motg->phy_irq);
	enable_irq(motg->irq);

	/* Enable ASYNC_IRQ only during LPM */
	disable_irq(motg->async_irq);

	if (motg->phy_irq_pending) {
		motg->phy_irq_pending = false;
		msm_id_status_w(&motg->id_status_work.work);
	}

	if (motg->host_bus_suspend) {
		usb_hcd_resume_root_hub(hcd);
		schedule_delayed_work(&motg->perf_vote_work,
			msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
	}

	dev_info(phy->dev, "USB exited from low power mode\n");
	msm_otg_dbg_log_event(phy, "LPM EXIT DONE",
			motg->caps, motg->lpm_flags);

	return 0;
}
#endif

static void msm_otg_notify_host_mode(struct msm_otg *motg, bool host_mode)
{
	if (!psy) {
		pr_err("No USB power supply registered!\n");
		return;
	}

	motg->host_mode = host_mode;
	power_supply_changed(psy);
}

static int msm_otg_notify_chg_type(struct msm_otg *motg)
{
	static int charger_type;
	union power_supply_propval propval;
	/*
	 * TODO
	 * Unify OTG driver charger types and power supply charger types
	 */
	if (charger_type == motg->chg_type)
		return 0;

	if (motg->chg_type == USB_SDP_CHARGER)
		charger_type = POWER_SUPPLY_TYPE_USB;
	else if (motg->chg_type == USB_CDP_CHARGER)
		charger_type = POWER_SUPPLY_TYPE_USB_CDP;
	else if (motg->chg_type == USB_DCP_CHARGER ||
			motg->chg_type == USB_PROPRIETARY_CHARGER ||
			motg->chg_type == USB_FLOATED_CHARGER)
		charger_type = POWER_SUPPLY_TYPE_USB_DCP;
	else
		charger_type = POWER_SUPPLY_TYPE_UNKNOWN;

	if (!psy) {
		pr_err("No USB power supply registered!\n");
		return -EINVAL;
	}

	pr_debug("setting usb power supply type %d\n", charger_type);
	msm_otg_dbg_log_event(&motg->phy, "SET USB PWR SUPPLY TYPE",
			motg->chg_type, charger_type);

	propval.intval = charger_type;
	psy->set_property(psy, POWER_SUPPLY_PROP_REAL_TYPE, &propval);

	return 0;
}

static int msm_otg_notify_power_supply(struct msm_otg *motg, unsigned mA)
{
	if (!psy) {
		dev_dbg(motg->phy.dev, "no usb power supply registered\n");
		goto psy_error;
	}

	if (motg->cur_power == 0 && mA > 2) {
		/* Enable charging */
		if (power_supply_set_online(psy, true))
			goto psy_error;
		if (power_supply_set_current_limit(psy, 1000*mA))
			goto psy_error;
	} else if (motg->cur_power >= 0 && (mA == 0 || mA == 2)) {
		/* Disable charging */
		if (power_supply_set_online(psy, false))
			goto psy_error;
		/* Set max current limit in uA */
		if (power_supply_set_current_limit(psy, 1000*mA))
			goto psy_error;
	} else {
		if (power_supply_set_online(psy, true))
			goto psy_error;
		/* Current has changed (100/2 --> 500) */
		if (power_supply_set_current_limit(psy, 1000*mA))
			goto psy_error;
	}

	power_supply_changed(psy);
	return 0;

psy_error:
	dev_dbg(motg->phy.dev, "power supply error when setting property\n");
	return -ENXIO;
}

static void msm_otg_set_online_status(struct msm_otg *motg)
{
	if (!psy) {
		dev_dbg(motg->phy.dev, "no usb power supply registered\n");
		return;
	}

	/* Set power supply online status to false */
	if (power_supply_set_online(psy, false))
		dev_dbg(motg->phy.dev, "error setting power supply property\n");
}

static void msm_otg_notify_charger(struct msm_otg *motg, unsigned mA)
{
	struct usb_gadget *g = motg->phy.otg->gadget;
	struct msm_otg_platform_data *pdata = motg->pdata;

	if (g && g->is_a_peripheral)
		return;

	dev_dbg(motg->phy.dev, "Requested curr from USB = %u, max-type-c:%u\n",
					mA, motg->typec_current_max);
	/* Save bc1.2 max_curr if type-c charger later moves to diff mode */
	motg->bc1p2_current_max = mA;

	/*
	 * Limit type-c charger current to 500 for SDP charger to avoid more
	 * current drawn than 500 with Hosts that don't support type C due to
	 * non compliant type-c to standard A cables.
	 */
	if (pdata->enable_sdp_typec_current_limit &&
			(motg->chg_type == USB_SDP_CHARGER) &&
					motg->typec_current_max > 500)
		motg->typec_current_max = 500;

	/* Override mA if type-c charger used (use hvdcp/bc1.2 if it is 500) */
	if (motg->typec_current_max > 500 && mA < motg->typec_current_max)
		mA = motg->typec_current_max;

	if (msm_otg_notify_chg_type(motg))
		dev_err(motg->phy.dev,
			"Failed notifying %d charger type to PMIC\n",
							motg->chg_type);

	/*
	 * This condition will be true when usb cable is disconnected
	 * during bootup before enumeration. Check charger type also
	 * to avoid clearing online flag in case of valid charger.
	 */
	if (motg->online && motg->cur_power == 0 && mA == 0 &&
			(motg->chg_type == USB_INVALID_CHARGER))
		msm_otg_set_online_status(motg);

	if (motg->cur_power == mA)
		return;

	dev_info(motg->phy.dev, "Avail curr from USB = %u\n", mA);
	msm_otg_dbg_log_event(&motg->phy, "AVAIL CURR FROM USB",
			mA, motg->chg_type);

	msm_otg_notify_power_supply(motg, mA);

	motg->cur_power = mA;
}

static int msm_otg_set_power(struct usb_phy *phy, unsigned mA)
{
	struct msm_otg *motg = container_of(phy, struct msm_otg, phy);

	/*
	 * Gadget driver uses set_power method to notify about the
	 * available current based on suspend/configured states.
	 *
	 * IDEV_CHG can be drawn irrespective of suspend/un-configured
	 * states when CDP/ACA is connected.
	 */
	if (motg->chg_type == USB_SDP_CHARGER)
		msm_otg_notify_charger(motg, mA);

	return 0;
}

static void msm_hsusb_vbus_power(struct msm_otg *motg, bool on);

static void msm_otg_perf_vote_update(struct msm_otg *motg, bool perf_mode)
{
	static bool curr_perf_mode;
	int ret, latency = motg->pm_qos_latency;
	long clk_rate;

	if (curr_perf_mode == perf_mode)
		return;

	if (perf_mode) {
		if (latency)
			pm_qos_update_request(&motg->pm_qos_req_dma, latency);
		msm_otg_bus_vote(motg, USB_MAX_PERF_VOTE);
		clk_rate = motg->core_clk_rate;
	} else {
		if (latency)
			pm_qos_update_request(&motg->pm_qos_req_dma,
						PM_QOS_DEFAULT_VALUE);
		msm_otg_bus_vote(motg, USB_MIN_PERF_VOTE);
		clk_rate = motg->core_clk_svs_rate;
	}

	if (clk_rate) {
		ret = clk_set_rate(motg->core_clk, clk_rate);
		if (ret)
			dev_err(motg->phy.dev, "sys_clk set_rate fail:%d %ld\n",
					ret, clk_rate);
	}
	curr_perf_mode = perf_mode;
	pr_debug("%s: latency updated to: %d, core_freq to: %ld\n", __func__,
					latency, clk_rate);
}

static void msm_otg_perf_vote_work(struct work_struct *w)
{
	struct msm_otg *motg = container_of(w, struct msm_otg,
						perf_vote_work.work);
	unsigned curr_sample_int_count;
	bool in_perf_mode = false;

	curr_sample_int_count = motg->usb_irq_count;
	motg->usb_irq_count = 0;

	if (curr_sample_int_count >= PM_QOS_THRESHOLD)
		in_perf_mode = true;

	msm_otg_perf_vote_update(motg, in_perf_mode);
	pr_debug("%s: in_perf_mode:%u, interrupts in last sample:%u\n",
		 __func__, in_perf_mode, curr_sample_int_count);

	schedule_delayed_work(&motg->perf_vote_work,
			msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
}

static void msm_otg_start_host(struct usb_otg *otg, int on)
{
	struct msm_otg *motg = container_of(otg->phy, struct msm_otg, phy);
	struct msm_otg_platform_data *pdata = motg->pdata;
	struct usb_hcd *hcd;
	u32 val;

	if (!otg->host)
		return;

	hcd = bus_to_hcd(otg->host);

	msm_otg_dbg_log_event(&motg->phy, "PM RT: StartHost GET",
				     get_pm_runtime_counter(motg->phy.dev), 0);
	pm_runtime_get_sync(otg->phy->dev);
	if (on) {
		dev_dbg(otg->phy->dev, "host on\n");
		msm_otg_dbg_log_event(&motg->phy, "HOST ON",
				motg->inputs, otg->phy->state);
		msm_hsusb_vbus_power(motg, 1);
		msm_otg_reset(&motg->phy);

		if (pdata->otg_control == OTG_PHY_CONTROL)
			ulpi_write(otg->phy, OTG_COMP_DISABLE,
				ULPI_SET(ULPI_PWR_CLK_MNG_REG));

		if (pdata->enable_axi_prefetch) {
			val = readl_relaxed(USB_HS_APF_CTRL);
			val &= ~APF_CTRL_EN;
			writel_relaxed(val, USB_HS_APF_CTRL);
		}
		usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
#ifdef CONFIG_SMP
		motg->pm_qos_req_dma.type = PM_QOS_REQ_AFFINE_IRQ;
		motg->pm_qos_req_dma.irq = motg->irq;
#endif
		pm_qos_add_request(&motg->pm_qos_req_dma,
				PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		/* start in perf mode for better performance initially */
		msm_otg_perf_vote_update(motg, true);
		schedule_delayed_work(&motg->perf_vote_work,
				msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
	} else {
		dev_dbg(otg->phy->dev, "host off\n");
		msm_otg_dbg_log_event(&motg->phy, "HOST OFF",
				motg->inputs, otg->phy->state);
		msm_hsusb_vbus_power(motg, 0);

		cancel_delayed_work_sync(&motg->perf_vote_work);
		msm_otg_perf_vote_update(motg, false);
		pm_qos_remove_request(&motg->pm_qos_req_dma);

		pm_runtime_disable(&hcd->self.root_hub->dev);
		pm_runtime_barrier(&hcd->self.root_hub->dev);
		usb_remove_hcd(hcd);
		msm_otg_reset(&motg->phy);

		if (pdata->enable_axi_prefetch)
			writel_relaxed(readl_relaxed(USB_HS_APF_CTRL)
					| (APF_CTRL_EN), USB_HS_APF_CTRL);

		/* HCD core reset all bits of PORTSC. select ULPI phy */
		writel_relaxed(0x80000000, USB_PORTSC);

		if (pdata->otg_control == OTG_PHY_CONTROL)
			ulpi_write(otg->phy, OTG_COMP_DISABLE,
				ULPI_CLR(ULPI_PWR_CLK_MNG_REG));
	}
	msm_otg_dbg_log_event(&motg->phy, "PM RT: StartHost PUT",
				     get_pm_runtime_counter(motg->phy.dev), 0);

	pm_runtime_mark_last_busy(otg->phy->dev);
	pm_runtime_put_autosuspend(otg->phy->dev);
}

static void msm_hsusb_vbus_power(struct msm_otg *motg, bool on)
{
	int ret;
	static bool vbus_is_on;

	msm_otg_dbg_log_event(&motg->phy, "VBUS POWER", on, vbus_is_on);
	if (vbus_is_on == on)
		return;

	if (motg->pdata->vbus_power) {
		ret = motg->pdata->vbus_power(on);
		if (!ret)
			vbus_is_on = on;
		return;
	}

	if (!vbus_otg) {
		pr_err("vbus_otg is NULL.");
		return;
	}

	/*
	 * if entering host mode tell the charger to not draw any current
	 * from usb before turning on the boost.
	 * if exiting host mode disable the boost before enabling to draw
	 * current from the source.
	 */
	if (on) {
		msm_otg_notify_host_mode(motg, on);
		ret = regulator_enable(vbus_otg);
		if (ret) {
			pr_err("unable to enable vbus_otg\n");
			return;
		}
		vbus_is_on = true;
	} else {
		ret = regulator_disable(vbus_otg);
		if (ret) {
			pr_err("unable to disable vbus_otg\n");
			return;
		}
		msm_otg_notify_host_mode(motg, on);
		vbus_is_on = false;
	}
}

static int msm_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct msm_otg *motg = container_of(otg->phy, struct msm_otg, phy);
	struct usb_hcd *hcd;

	/*
	 * Fail host registration if this board can support
	 * only peripheral configuration.
	 */
	if (motg->pdata->mode == USB_PERIPHERAL) {
		dev_info(otg->phy->dev, "Host mode is not supported\n");
		return -ENODEV;
	}

	if (!motg->pdata->vbus_power && host) {
		vbus_otg = devm_regulator_get(motg->phy.dev, "vbus_otg");
		if (IS_ERR(vbus_otg)) {
			msm_otg_dbg_log_event(&motg->phy,
					"UNABLE TO GET VBUS_OTG",
					otg->phy->state, 0);
			pr_err("Unable to get vbus_otg\n");
			return PTR_ERR(vbus_otg);
		}
	}

	if (!host) {
		if (otg->phy->state == OTG_STATE_A_HOST) {
			msm_otg_start_host(otg, 0);
			otg->host = NULL;
			otg->phy->state = OTG_STATE_UNDEFINED;
			queue_work(motg->otg_wq, &motg->sm_work);
		} else {
			otg->host = NULL;
		}

		return 0;
	}

	hcd = bus_to_hcd(host);
	hcd->power_budget = motg->pdata->power_budget;

	otg->host = host;
	dev_dbg(otg->phy->dev, "host driver registered w/ tranceiver\n");
	msm_otg_dbg_log_event(&motg->phy, "HOST DRIVER REGISTERED",
			hcd->power_budget, motg->pdata->mode);

	/*
	 * Kick the state machine work, if peripheral is not supported
	 * or peripheral is already registered with us.
	 */
	if (motg->pdata->mode == USB_HOST || otg->gadget)
		queue_work(motg->otg_wq, &motg->sm_work);

	return 0;
}

static void msm_otg_start_peripheral(struct usb_otg *otg, int on)
{
	struct msm_otg *motg = container_of(otg->phy, struct msm_otg, phy);
	struct msm_otg_platform_data *pdata = motg->pdata;
	struct pinctrl_state *set_state;
	int ret;

	if (!otg->gadget)
		return;

	msm_otg_dbg_log_event(&motg->phy, "PM RT: StartPeri GET",
				     get_pm_runtime_counter(motg->phy.dev), 0);
	pm_runtime_get_sync(otg->phy->dev);
	if (on) {
		dev_dbg(otg->phy->dev, "gadget on\n");
		msm_otg_dbg_log_event(&motg->phy, "GADGET ON",
				motg->inputs, otg->phy->state);

		/* Configure BUS performance parameters for MAX bandwidth */
		if (debug_bus_voting_enabled)
			msm_otg_bus_vote(motg, USB_MAX_PERF_VOTE);
		/* bump up usb core_clk to default */
		clk_set_rate(motg->core_clk, motg->core_clk_rate);

		usb_gadget_vbus_connect(otg->gadget);

		/*
		 * Request VDD min gpio, if need to support VDD
		 * minimazation during peripheral bus suspend.
		 */
		if (pdata->vddmin_gpio) {
			if (motg->phy_pinctrl) {
				set_state =
					pinctrl_lookup_state(motg->phy_pinctrl,
							"hsusb_active");
				if (IS_ERR(set_state)) {
					pr_err("cannot get phy pinctrl active state\n");
				} else {
					pinctrl_select_state(motg->phy_pinctrl,
								set_state);
				}
			}

			ret = gpio_request(pdata->vddmin_gpio,
					"MSM_OTG_VDD_MIN_GPIO");
			if (ret < 0) {
				dev_err(otg->phy->dev, "gpio req failed for vdd min:%d\n",
						ret);
				pdata->vddmin_gpio = 0;
			}
		}
	} else {
		dev_dbg(otg->phy->dev, "gadget off\n");
		msm_otg_dbg_log_event(&motg->phy, "GADGET OFF",
			motg->inputs, otg->phy->state);
		usb_gadget_vbus_disconnect(otg->gadget);
		clear_bit(A_BUS_SUSPEND, &motg->inputs);
		/* Configure BUS performance parameters to default */
		msm_otg_bus_vote(motg, USB_MIN_PERF_VOTE);

		if (pdata->vddmin_gpio) {
			gpio_free(pdata->vddmin_gpio);
			if (motg->phy_pinctrl) {
				set_state =
					pinctrl_lookup_state(motg->phy_pinctrl,
							"hsusb_sleep");
				if (IS_ERR(set_state))
					pr_err("cannot get phy pinctrl sleep state\n");
				else
					pinctrl_select_state(motg->phy_pinctrl,
						set_state);
			}
		}
	}
	msm_otg_dbg_log_event(&motg->phy, "PM RT: StartPeri PUT",
				     get_pm_runtime_counter(motg->phy.dev), 0);
	pm_runtime_mark_last_busy(otg->phy->dev);
	pm_runtime_put_autosuspend(otg->phy->dev);
}

static int msm_otg_set_peripheral(struct usb_otg *otg,
					struct usb_gadget *gadget)
{
	struct msm_otg *motg = container_of(otg->phy, struct msm_otg, phy);

	/*
	 * Fail peripheral registration if this board can support
	 * only host configuration.
	 */
	if (motg->pdata->mode == USB_HOST) {
		dev_info(otg->phy->dev, "Peripheral mode is not supported\n");
		return -ENODEV;
	}

	if (!gadget) {
		if (otg->phy->state == OTG_STATE_B_PERIPHERAL) {
			msm_otg_dbg_log_event(&motg->phy,
				"PM RUNTIME: PERIPHERAL GET1",
				get_pm_runtime_counter(otg->phy->dev), 0);
			msm_otg_start_peripheral(otg, 0);
			otg->gadget = NULL;
			otg->phy->state = OTG_STATE_UNDEFINED;
			queue_work(motg->otg_wq, &motg->sm_work);
		} else {
			otg->gadget = NULL;
		}

		return 0;
	}
	otg->gadget = gadget;
	dev_dbg(otg->phy->dev, "peripheral driver registered w/ tranceiver\n");
	msm_otg_dbg_log_event(&motg->phy, "PERIPHERAL DRIVER REGISTERED",
			otg->phy->state, motg->pdata->mode);

	/*
	 * Kick the state machine work, if host is not supported
	 * or host is already registered with us.
	 */
	if (motg->pdata->mode == USB_PERIPHERAL || otg->host)
		queue_work(motg->otg_wq, &motg->sm_work);

	return 0;
}

static bool msm_otg_read_pmic_id_state(struct msm_otg *motg)
{
	unsigned long flags;
	int id;

	if (!motg->pdata->pmic_id_irq)
		return -ENODEV;

	local_irq_save(flags);
	id = irq_read_line(motg->pdata->pmic_id_irq);
	local_irq_restore(flags);

	/*
	 * If we can not read ID line state for some reason, treat
	 * it as float. This would prevent MHL discovery and kicking
	 * host mode unnecessarily.
	 */
	return !!id;
}

static bool msm_otg_read_phy_id_state(struct msm_otg *motg)
{
	u8 val;

	/*
	 * clear the pending/outstanding interrupts and
	 * read the ID status from the SRC_STATUS register.
	 */
	writeb_relaxed(USB_PHY_ID_MASK, USB2_PHY_USB_PHY_INTERRUPT_CLEAR1);

	writeb_relaxed(0x1, USB2_PHY_USB_PHY_IRQ_CMD);
	/*
	 * Databook says 200 usec delay is required for
	 * clearing the interrupts.
	 */
	udelay(200);
	writeb_relaxed(0x0, USB2_PHY_USB_PHY_IRQ_CMD);

	val = readb_relaxed(USB2_PHY_USB_PHY_INTERRUPT_SRC_STATUS);
	if (val & USB_PHY_IDDIG_1_0)
		return false; /* ID is grounded */
	else
		return true;
}

static void msm_otg_chg_check_timer_func(unsigned long data)
{
	struct msm_otg *motg = (struct msm_otg *) data;
	struct usb_otg *otg = motg->phy.otg;

	if (atomic_read(&motg->in_lpm) ||
		!test_bit(B_SESS_VLD, &motg->inputs) ||
		otg->phy->state != OTG_STATE_B_PERIPHERAL ||
		otg->gadget->speed != USB_SPEED_UNKNOWN) {
		dev_dbg(otg->phy->dev, "Nothing to do in chg_check_timer\n");
		return;
	}

	if ((readl_relaxed(USB_PORTSC) & PORTSC_LS) == PORTSC_LS) {
		dev_dbg(otg->phy->dev, "DCP is detected as SDP\n");
		msm_otg_dbg_log_event(&motg->phy, "DCP IS DETECTED AS SDP",
				otg->phy->state, 0);
		set_bit(B_FALSE_SDP, &motg->inputs);
		queue_work(motg->otg_wq, &motg->sm_work);
	}
}

static bool msm_chg_check_secondary_det(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;
	u32 chg_det;
	bool ret = false;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
	case SNPS_FEMTO_PHY:
		chg_det = ulpi_read(phy, 0x87);
		ret = chg_det & 1;
		break;
	default:
		break;
	}
	return ret;
}

static void msm_chg_enable_secondary_det(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
	case SNPS_FEMTO_PHY:
		/*
		 * Configure DM as current source, DP as current sink
		 * and enable battery charging comparators.
		 */
		ulpi_write(phy, 0x8, 0x85);
		ulpi_write(phy, 0x2, 0x85);
		ulpi_write(phy, 0x1, 0x85);
		break;
	default:
		break;
	}
}

static bool msm_chg_check_primary_det(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;
	u32 chg_det;
	bool ret = false;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
	case SNPS_FEMTO_PHY:
		chg_det = ulpi_read(phy, 0x87);
		ret = chg_det & 1;
		/* Turn off VDP_SRC */
		ulpi_write(phy, 0x3, 0x86);
		msleep(20);
		break;
	default:
		break;
	}
	return ret;
}

static void msm_chg_enable_primary_det(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
	case SNPS_FEMTO_PHY:
		/*
		 * Configure DP as current source, DM as current sink
		 * and enable battery charging comparators.
		 */
		ulpi_write(phy, 0x2, 0x85);
		ulpi_write(phy, 0x1, 0x85);
		break;
	default:
		break;
	}
}

static bool msm_chg_check_dcd(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;
	u32 line_state;
	bool ret = false;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
	case SNPS_FEMTO_PHY:
		line_state = ulpi_read(phy, 0x87);
		ret = line_state & 2;
		break;
	default:
		break;
	}
	return ret;
}

static void msm_chg_disable_dcd(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
		ulpi_write(phy, 0x10, 0x86);
		break;
	case SNPS_FEMTO_PHY:
		ulpi_write(phy, 0x10, 0x86);
		/*
		 * Disable the Rdm_down after
		 * the DCD is completed.
		 */
		ulpi_write(phy, 0x04, 0x0C);
		break;
	default:
		break;
	}
}

static void msm_chg_enable_dcd(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
		/* Data contact detection enable */
		ulpi_write(phy, 0x10, 0x85);
		break;
	case SNPS_FEMTO_PHY:
		/*
		 * Idp_src and Rdm_down are de-coupled
		 * on Femto PHY. If Idp_src alone is
		 * enabled, DCD timeout is observed with
		 * wall charger. But a genuine DCD timeout
		 * may be incorrectly interpreted. Also
		 * BC1.2 compliance testers expect Rdm_down
		 * to enabled during DCD. Enable Rdm_down
		 * explicitly before enabling the DCD.
		 */
		ulpi_write(phy, 0x04, 0x0B);
		ulpi_write(phy, 0x10, 0x85);
		break;
	default:
		break;
	}
}

static void msm_chg_block_on(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;
	u32 func_ctrl;

	/* put the controller in non-driving mode */
	func_ctrl = ulpi_read(phy, ULPI_FUNC_CTRL);
	func_ctrl &= ~ULPI_FUNC_CTRL_OPMODE_MASK;
	func_ctrl |= ULPI_FUNC_CTRL_OPMODE_NONDRIVING;
	ulpi_write(phy, func_ctrl, ULPI_FUNC_CTRL);

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
	case SNPS_FEMTO_PHY:
		/* disable DP and DM pull down resistors */
		ulpi_write(phy, 0x6, 0xC);
		/* Clear charger detecting control bits */
		ulpi_write(phy, 0x1F, 0x86);
		/* Clear alt interrupt latch and enable bits */
		ulpi_write(phy, 0x1F, 0x92);
		ulpi_write(phy, 0x1F, 0x95);
		udelay(100);
		break;
	default:
		break;
	}
}

static void msm_chg_block_off(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;
	u32 func_ctrl;

	switch (motg->pdata->phy_type) {
	case SNPS_PICO_PHY:
	case SNPS_FEMTO_PHY:
		/* Clear charger detecting control bits */
		ulpi_write(phy, 0x3F, 0x86);
		/* Clear alt interrupt latch and enable bits */
		ulpi_write(phy, 0x1F, 0x92);
		ulpi_write(phy, 0x1F, 0x95);
		/* re-enable DP and DM pull down resistors */
		ulpi_write(phy, 0x6, 0xB);
		break;
	default:
		break;
	}

	/* put the controller in normal mode */
	func_ctrl = ulpi_read(phy, ULPI_FUNC_CTRL);
	func_ctrl &= ~ULPI_FUNC_CTRL_OPMODE_MASK;
	func_ctrl |= ULPI_FUNC_CTRL_OPMODE_NORMAL;
	ulpi_write(phy, func_ctrl, ULPI_FUNC_CTRL);
}

static const char *chg_to_string(enum usb_chg_type chg_type)
{
	switch (chg_type) {
	case USB_SDP_CHARGER:		return "USB_SDP_CHARGER";
	case USB_DCP_CHARGER:		return "USB_DCP_CHARGER";
	case USB_CDP_CHARGER:		return "USB_CDP_CHARGER";
	case USB_PROPRIETARY_CHARGER:	return "USB_PROPRIETARY_CHARGER";
	case USB_FLOATED_CHARGER:	return "USB_FLOATED_CHARGER";
	default:			return "INVALID_CHARGER";
	}
}

#define MSM_CHG_DCD_TIMEOUT		(750 * HZ/1000) /* 750 msec */
#define MSM_CHG_DCD_POLL_TIME		(50 * HZ/1000) /* 50 msec */
#define MSM_CHG_PRIMARY_DET_TIME	(50 * HZ/1000) /* TVDPSRC_ON */
#define MSM_CHG_SECONDARY_DET_TIME	(50 * HZ/1000) /* TVDMSRC_ON */
static void msm_chg_detect_work(struct work_struct *w)
{
	struct msm_otg *motg = container_of(w, struct msm_otg, chg_work.work);
	struct usb_phy *phy = &motg->phy;
	bool is_dcd = false, tmout, vout;
	static bool dcd;
	u32 line_state, dm_vlgc;
	unsigned long delay;

	dev_dbg(phy->dev, "chg detection work\n");
	msm_otg_dbg_log_event(phy, "CHG DETECTION WORK",
			motg->chg_state, get_pm_runtime_counter(phy->dev));

	switch (motg->chg_state) {
	case USB_CHG_STATE_UNDEFINED:
	case USB_CHG_STATE_IN_PROGRESS:
		msm_chg_block_on(motg);
		msm_chg_enable_dcd(motg);
		motg->chg_state = USB_CHG_STATE_WAIT_FOR_DCD;
		motg->dcd_time = 0;
		delay = MSM_CHG_DCD_POLL_TIME;
		break;
	case USB_CHG_STATE_WAIT_FOR_DCD:
		is_dcd = msm_chg_check_dcd(motg);
		motg->dcd_time += MSM_CHG_DCD_POLL_TIME;
		tmout = motg->dcd_time >= MSM_CHG_DCD_TIMEOUT;
		if (is_dcd || tmout) {
			if (is_dcd)
				dcd = true;
			else
				dcd = false;
			msm_chg_disable_dcd(motg);
			msm_chg_enable_primary_det(motg);
			delay = MSM_CHG_PRIMARY_DET_TIME;
			motg->chg_state = USB_CHG_STATE_DCD_DONE;
		} else {
			delay = MSM_CHG_DCD_POLL_TIME;
		}
		break;
	case USB_CHG_STATE_DCD_DONE:
		vout = msm_chg_check_primary_det(motg);
		line_state = readl_relaxed(USB_PORTSC) & PORTSC_LS;
		dm_vlgc = line_state & PORTSC_LS_DM;
		if (vout && !dm_vlgc) { /* VDAT_REF < DM < VLGC */
			if (line_state) { /* DP > VLGC */
				motg->chg_type = USB_PROPRIETARY_CHARGER;
				motg->chg_state = USB_CHG_STATE_DETECTED;
				delay = 0;
			} else {
				msm_chg_enable_secondary_det(motg);
				delay = MSM_CHG_SECONDARY_DET_TIME;
				motg->chg_state = USB_CHG_STATE_PRIMARY_DONE;
			}
		} else { /* DM < VDAT_REF || DM > VLGC */
			if (line_state) /* DP > VLGC or/and DM > VLGC */
				motg->chg_type = USB_PROPRIETARY_CHARGER;
			else if (!dcd && floated_charger_enable)
				motg->chg_type = USB_FLOATED_CHARGER;
			else
				motg->chg_type = USB_SDP_CHARGER;

			motg->chg_state = USB_CHG_STATE_DETECTED;
			delay = 0;
			goto state_detected;
		}
		break;
	case USB_CHG_STATE_PRIMARY_DONE:
		vout = msm_chg_check_secondary_det(motg);
		if (vout)
			motg->chg_type = USB_DCP_CHARGER;
		else
			motg->chg_type = USB_CDP_CHARGER;
		motg->chg_state = USB_CHG_STATE_SECONDARY_DONE;
		/* fall through */
	case USB_CHG_STATE_SECONDARY_DONE:
		motg->chg_state = USB_CHG_STATE_DETECTED;
	case USB_CHG_STATE_DETECTED:
state_detected:
		/*
		 * Notify the charger type to power supply
		 * owner as soon as we determine the charger.
		 */
		if (motg->chg_type == USB_DCP_CHARGER &&
			motg->ext_chg_opened) {
				init_completion(&motg->ext_chg_wait);
				motg->ext_chg_active = DEFAULT;
		}
		msm_otg_notify_chg_type(motg);
		msm_chg_block_off(motg);

		/* Enable VDP_SRC in case of DCP charger */
		if (motg->chg_type == USB_DCP_CHARGER)
			ulpi_write(phy, 0x2, 0x85);

		dev_dbg(phy->dev, "chg_type = %s\n",
			chg_to_string(motg->chg_type));
		msm_otg_dbg_log_event(phy, "CHG WORK PUT: CHG_TYPE",
			motg->chg_type, get_pm_runtime_counter(phy->dev));
		/* to match _get from sm_work before starting chg_det_work */
		pm_runtime_mark_last_busy(phy->dev);
		pm_runtime_put_autosuspend(phy->dev);

		queue_work(motg->otg_wq, &motg->sm_work);
		return;
	default:
		return;
	}

	msm_otg_dbg_log_event(phy, "CHG WORK: QUEUE", motg->chg_type, delay);
	queue_delayed_work(motg->otg_wq, &motg->chg_work, delay);
}

#define VBUS_INIT_TIMEOUT	msecs_to_jiffies(5000)

/*
 * We support OTG, Peripheral only and Host only configurations. In case
 * of OTG, mode switch (host-->peripheral/peripheral-->host) can happen
 * via Id pin status or user request (debugfs). Id/BSV interrupts are not
 * enabled when switch is controlled by user and default mode is supplied
 * by board file, which can be changed by userspace later.
 */
static void msm_otg_init_sm(struct msm_otg *motg)
{
	struct msm_otg_platform_data *pdata = motg->pdata;
	u32 otgsc = readl_relaxed(USB_OTGSC);
	int ret;

	switch (pdata->mode) {
	case USB_OTG:
		if (pdata->otg_control == OTG_USER_CONTROL) {
			if (pdata->default_mode == USB_HOST) {
				clear_bit(ID, &motg->inputs);
			} else if (pdata->default_mode == USB_PERIPHERAL) {
				set_bit(ID, &motg->inputs);
				set_bit(B_SESS_VLD, &motg->inputs);
			} else {
				set_bit(ID, &motg->inputs);
				clear_bit(B_SESS_VLD, &motg->inputs);
			}
		} else if (pdata->otg_control == OTG_PHY_CONTROL) {
			if (otgsc & OTGSC_ID)
				set_bit(ID, &motg->inputs);
			else
				clear_bit(ID, &motg->inputs);
			if (otgsc & OTGSC_BSV)
				set_bit(B_SESS_VLD, &motg->inputs);
			else
				clear_bit(B_SESS_VLD, &motg->inputs);
		} else if (pdata->otg_control == OTG_PMIC_CONTROL) {
			if (pdata->pmic_id_irq) {
				if (msm_otg_read_pmic_id_state(motg))
					set_bit(ID, &motg->inputs);
				else
					clear_bit(ID, &motg->inputs);
			} else if (motg->ext_id_irq) {
				if (gpio_get_value(pdata->usb_id_gpio))
					set_bit(ID, &motg->inputs);
				else
					clear_bit(ID, &motg->inputs);
			} else if (motg->phy_irq) {
				if (msm_otg_read_phy_id_state(motg))
					set_bit(ID, &motg->inputs);
				else
					clear_bit(ID, &motg->inputs);
			}
			/*
			 * VBUS initial state is reported after PMIC
			 * driver initialization. Wait for it.
			 */
			ret = wait_for_completion_timeout(&pmic_vbus_init,
							  VBUS_INIT_TIMEOUT);
			if (!ret) {
				dev_dbg(motg->phy.dev, "%s: timeout waiting for PMIC VBUS\n",
					__func__);
				msm_otg_dbg_log_event(&motg->phy,
						"PMIC VBUS WAIT TMOUT",
						motg->inputs, motg->phy.state);
				clear_bit(B_SESS_VLD, &motg->inputs);
				pmic_vbus_init.done = 1;
			}
		}
		break;
	case USB_HOST:
		clear_bit(ID, &motg->inputs);
		break;
	case USB_PERIPHERAL:
		set_bit(ID, &motg->inputs);
		if (pdata->otg_control == OTG_PHY_CONTROL) {
			if (otgsc & OTGSC_BSV)
				set_bit(B_SESS_VLD, &motg->inputs);
			else
				clear_bit(B_SESS_VLD, &motg->inputs);
		} else if (pdata->otg_control == OTG_PMIC_CONTROL) {
			/*
			 * VBUS initial state is reported after PMIC
			 * driver initialization. Wait for it.
			 */
			ret = wait_for_completion_timeout(&pmic_vbus_init,
							  VBUS_INIT_TIMEOUT);
			if (!ret) {
				dev_dbg(motg->phy.dev, "%s: timeout waiting for PMIC VBUS\n",
					__func__);
				msm_otg_dbg_log_event(&motg->phy,
						"PMIC VBUS WAIT TMOUT",
						motg->inputs, motg->phy.state);
				clear_bit(B_SESS_VLD, &motg->inputs);
				pmic_vbus_init.done = 1;
			}
		} else if (pdata->otg_control == OTG_USER_CONTROL) {
			set_bit(ID, &motg->inputs);
			set_bit(B_SESS_VLD, &motg->inputs);
		}
		break;
	default:
		break;
	}
	msm_otg_dbg_log_event(&motg->phy, "SM INIT", pdata->mode, motg->inputs);
	if (motg->id_state != USB_ID_GROUND)
		motg->id_state = (test_bit(ID, &motg->inputs)) ? USB_ID_FLOAT :
							USB_ID_GROUND;
}

static void msm_otg_wait_for_ext_chg_done(struct msm_otg *motg)
{
	struct usb_phy *phy = &motg->phy;
	unsigned long t;

	/*
	 * Defer next cable connect event till external charger
	 * detection is completed.
	 */

	if (motg->ext_chg_active == ACTIVE) {

do_wait:
		pr_debug("before msm_otg ext chg wait\n");
		msm_otg_dbg_log_event(&motg->phy, "EXT CHG: WAIT", 0, 0);

		t = wait_for_completion_timeout(&motg->ext_chg_wait,
				msecs_to_jiffies(3000));
		msm_otg_dbg_log_event(&motg->phy, "EXT CHG: DONE", t, 0);

		if (!t)
			pr_err("msm_otg ext chg wait timeout\n");
		else if (motg->ext_chg_active == ACTIVE)
			goto do_wait;
		else
			pr_debug("msm_otg ext chg wait done\n");
	}

	if (motg->ext_chg_opened) {
		if (phy->flags & ENABLE_DP_MANUAL_PULLUP) {
			ulpi_write(phy, ULPI_MISC_A_VBUSVLDEXT |
					ULPI_MISC_A_VBUSVLDEXTSEL,
					ULPI_CLR(ULPI_MISC_A));
		}
		/* clear charging register bits */
		ulpi_write(phy, 0x3F, 0x86);
		/* re-enable DP and DM pull-down resistors*/
		ulpi_write(phy, 0x6, 0xB);
	}
}

static void msm_otg_sm_work(struct work_struct *w)
{
	struct msm_otg *motg = container_of(w, struct msm_otg, sm_work);
	struct usb_otg *otg = motg->phy.otg;
	struct device *dev = otg->phy->dev;
	bool work = 0, dcp;
	int ret;

	pr_debug("%s work\n", usb_otg_state_string(otg->phy->state));
	msm_otg_dbg_log_event(&motg->phy, "SM WORK:",
			otg->phy->state, motg->inputs);

	/* Just resume h/w if reqd, pm_count is handled based on state/inputs */
	if (motg->resume_pending) {
		pm_runtime_get_sync(otg->phy->dev);
		if (atomic_read(&motg->in_lpm)) {
			dev_err(dev, "SM WORK: USB is in LPM\n");
			msm_otg_dbg_log_event(&motg->phy,
					"SM WORK: USB IS IN LPM",
					otg->phy->state, motg->inputs);
			msm_otg_resume(motg);
		}
		motg->resume_pending = false;
		pm_runtime_put_noidle(otg->phy->dev);
	}

	switch (otg->phy->state) {
	case OTG_STATE_UNDEFINED:
		pm_runtime_get_sync(otg->phy->dev);
		msm_otg_reset(otg->phy);
		/* Add child device only after block reset */
		ret = of_platform_populate(motg->pdev->dev.of_node, NULL, NULL,
					&motg->pdev->dev);
		if (ret)
			dev_dbg(&motg->pdev->dev, "failed to add BAM core\n");

		msm_otg_init_sm(motg);
		otg->phy->state = OTG_STATE_B_IDLE;
		if (!test_bit(B_SESS_VLD, &motg->inputs) &&
				test_bit(ID, &motg->inputs)) {
			msm_otg_dbg_log_event(&motg->phy,
				"PM RUNTIME: UNDEF PUT",
				get_pm_runtime_counter(otg->phy->dev), 0);
			pm_runtime_put_sync(otg->phy->dev);
			break;
		}
		pm_runtime_put(otg->phy->dev);
		/* FALL THROUGH */
	case OTG_STATE_B_IDLE:
		if (!test_bit(ID, &motg->inputs) && otg->host) {
			pr_debug("!id\n");
			msm_otg_dbg_log_event(&motg->phy, "!ID",
					motg->inputs, otg->phy->state);

			msm_otg_start_host(otg, 1);
			otg->phy->state = OTG_STATE_A_HOST;
		} else if (test_bit(B_SESS_VLD, &motg->inputs)) {
			pr_debug("b_sess_vld\n");
			msm_otg_dbg_log_event(&motg->phy, "B_SESS_VLD",
					motg->inputs, otg->phy->state);
			switch (motg->chg_state) {
			case USB_CHG_STATE_UNDEFINED:
				/* put at the end of chg_det or disconnect */
				pm_runtime_get_sync(otg->phy->dev);
				msm_otg_dbg_log_event(&motg->phy, "PM CHG GET",
						get_pm_runtime_counter(dev), 0);
				motg->chg_state = USB_CHG_STATE_IN_PROGRESS;
				msm_chg_detect_work(&motg->chg_work.work);
				break;
			case USB_CHG_STATE_DETECTED:
				switch (motg->chg_type) {
				case USB_DCP_CHARGER:
					/* fall through */
				case USB_PROPRIETARY_CHARGER:
					msm_otg_notify_charger(motg,
							dcp_max_current);
					if (!motg->is_ext_chg_dcp)
						otg->phy->state =
							OTG_STATE_B_CHARGER;
					break;
				case USB_FLOATED_CHARGER:
					msm_otg_notify_charger(motg,
							IDEV_CHG_MAX);
					otg->phy->state = OTG_STATE_B_CHARGER;
					break;
				case USB_CDP_CHARGER:
					msm_otg_notify_charger(motg,
							IDEV_CHG_MAX);
					/* fall through */
				case USB_SDP_CHARGER:
					pm_runtime_get_sync(otg->phy->dev);
					msm_otg_start_peripheral(otg, 1);
					otg->phy->state =
						OTG_STATE_B_PERIPHERAL;
					mod_timer(&motg->chg_check_timer,
							CHG_RECHECK_DELAY);
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		} else {
			pr_debug("chg_work cancel");
			msm_otg_dbg_log_event(&motg->phy, "CHG_WORK CANCEL",
					motg->inputs, otg->phy->state);
			del_timer_sync(&motg->chg_check_timer);
			clear_bit(B_FALSE_SDP, &motg->inputs);
			cancel_delayed_work_sync(&motg->chg_work);
			/*
			 * Find out whether chg_w couldn't start or finished.
			 * In both the cases, runtime ref_count vote is missing
			 */
			if (motg->chg_state == USB_CHG_STATE_UNDEFINED ||
			    motg->chg_state == USB_CHG_STATE_DETECTED) {
				msm_otg_dbg_log_event(&motg->phy, "RT !CHG GET",
				      get_pm_runtime_counter(otg->phy->dev), 0);
				pm_runtime_get_sync(dev);
			}

			dcp = (motg->chg_type == USB_DCP_CHARGER);
			motg->chg_state = USB_CHG_STATE_UNDEFINED;
			motg->chg_type = USB_INVALID_CHARGER;
			msm_otg_notify_charger(motg, 0);
			if (dcp) {
				if (motg->ext_chg_active == DEFAULT)
					motg->ext_chg_active = INACTIVE;
				msm_otg_wait_for_ext_chg_done(motg);
				/* Turn off VDP_SRC */
				ulpi_write(otg->phy, 0x2, 0x86);
			}
			msm_chg_block_off(motg);
			msm_otg_dbg_log_event(&motg->phy, "RT: CHG A PUT",
				get_pm_runtime_counter(otg->phy->dev), 0);
			/* Delay used only if autosuspend enabled */
			pm_runtime_mark_last_busy(dev);
			pm_runtime_put_autosuspend(dev);
		}
		break;
	case OTG_STATE_B_PERIPHERAL:
		if (test_bit(B_SESS_VLD, &motg->inputs) &&
				test_bit(B_FALSE_SDP, &motg->inputs)) {
			pr_debug("B_FALSE_SDP\n");
			msm_otg_start_peripheral(otg, 0);
			motg->chg_type = USB_DCP_CHARGER;
			clear_bit(B_FALSE_SDP, &motg->inputs);
			otg->phy->state = OTG_STATE_B_IDLE;
			msm_otg_dbg_log_event(&motg->phy, "B_FALSE_SDP PUT",
				get_pm_runtime_counter(dev), motg->inputs);
			pm_runtime_put_sync(dev);
			/* schedule work to update charging current */
			work = 1;
		} else if (!test_bit(B_SESS_VLD, &motg->inputs)) {
			msm_otg_start_peripheral(otg, 0);
			msm_otg_dbg_log_event(&motg->phy, "RT PM: B_PERI A PUT",
				get_pm_runtime_counter(dev), 0);
			/* Schedule work to finish cable disconnect processing*/
			otg->phy->state = OTG_STATE_B_IDLE;
			/* _put for _get done on cable connect in B_IDLE */
			pm_runtime_put_noidle(dev);
			work = 1;
		} else if (test_bit(A_BUS_SUSPEND, &motg->inputs)) {
			pr_debug("a_bus_suspend\n");
			msm_otg_dbg_log_event(&motg->phy,
				"BUS_SUSPEND: PM RT PUT",
				get_pm_runtime_counter(dev), 0);
			otg->phy->state = OTG_STATE_B_SUSPEND;
			/* _get on connect in B_IDLE or host resume in B_SUSP */
			pm_runtime_mark_last_busy(dev);
			pm_runtime_put_autosuspend(dev);
		}
		break;
	case OTG_STATE_B_SUSPEND:
		if (!test_bit(B_SESS_VLD, &motg->inputs)) {
			msm_otg_start_peripheral(otg, 0);
			otg->phy->state = OTG_STATE_B_IDLE;
			/* Schedule work to finish cable disconnect processing*/
			work = 1;
		} else if (!test_bit(A_BUS_SUSPEND, &motg->inputs)) {
			pr_debug("!a_bus_suspend\n");
			otg->phy->state = OTG_STATE_B_PERIPHERAL;
			msm_otg_dbg_log_event(&motg->phy,
				"BUS_RESUME: PM RT GET",
				get_pm_runtime_counter(dev), 0);
			pm_runtime_get_sync(dev);
		}
		break;

	case OTG_STATE_B_CHARGER:
		if (test_bit(B_SESS_VLD, &motg->inputs)) {
			pr_debug("BSV set again\n");
			msm_otg_dbg_log_event(&motg->phy, "BSV SET AGAIN",
					motg->inputs, otg->phy->state);
		} else if (!test_bit(B_SESS_VLD, &motg->inputs)) {
			otg->phy->state = OTG_STATE_B_IDLE;
			work = 1;
		}
		break;
	case OTG_STATE_A_HOST:
		if (test_bit(ID, &motg->inputs)) {
			msm_otg_start_host(otg, 0);
			otg->phy->state = OTG_STATE_B_IDLE;
			work = 1;
		}
		break;
	default:
		break;
	}

	if (work)
		queue_work(motg->otg_wq, &motg->sm_work);
}

static irqreturn_t msm_otg_irq(int irq, void *data)
{
	struct msm_otg *motg = data;
	struct usb_otg *otg = motg->phy.otg;
	u32 otgsc = 0;
	bool work = 0;

	if (atomic_read(&motg->in_lpm)) {
		pr_debug("OTG IRQ: %d in LPM\n", irq);
		msm_otg_dbg_log_event(&motg->phy, "OTG IRQ IS IN LPM",
				irq, otg->phy->state);
		/*Ignore interrupt if one interrupt already seen in LPM*/
		if (motg->async_int)
			return IRQ_HANDLED;

		disable_irq_nosync(irq);
		motg->async_int = irq;
		msm_otg_kick_sm_work(motg);

		return IRQ_HANDLED;
	}
	motg->usb_irq_count++;

	otgsc = readl_relaxed(USB_OTGSC);
	if (!(otgsc & (OTGSC_IDIS | OTGSC_BSVIS)))
		return IRQ_NONE;

	if ((otgsc & OTGSC_IDIS) && (otgsc & OTGSC_IDIE)) {
		if (otgsc & OTGSC_ID) {
			dev_dbg(otg->phy->dev, "ID set\n");
			msm_otg_dbg_log_event(&motg->phy, "ID SET",
				motg->inputs, otg->phy->state);
			set_bit(ID, &motg->inputs);
		} else {
			dev_dbg(otg->phy->dev, "ID clear\n");
			msm_otg_dbg_log_event(&motg->phy, "ID CLEAR",
					motg->inputs, otg->phy->state);
			clear_bit(ID, &motg->inputs);
		}
		work = 1;
	} else if ((otgsc & OTGSC_BSVIE) && (otgsc & OTGSC_BSVIS)) {
		if (otgsc & OTGSC_BSV) {
			dev_dbg(otg->phy->dev, "BSV set\n");
			msm_otg_dbg_log_event(&motg->phy, "BSV SET",
					motg->inputs, otg->phy->state);
			set_bit(B_SESS_VLD, &motg->inputs);
		} else {
			dev_dbg(otg->phy->dev, "BSV clear\n");
			msm_otg_dbg_log_event(&motg->phy, "BSV CLEAR",
					motg->inputs, otg->phy->state);
			clear_bit(B_SESS_VLD, &motg->inputs);
			clear_bit(A_BUS_SUSPEND, &motg->inputs);
		}
		work = 1;
	}
	if (work)
		queue_work(motg->otg_wq, &motg->sm_work);

	writel_relaxed(otgsc, USB_OTGSC);

	return IRQ_HANDLED;
}

static void msm_otg_set_vbus_state(int online)
{
	struct msm_otg *motg = the_msm_otg;
	static bool init;

	motg->vbus_state = online;

	if (motg->err_event_seen)
		return;

	if (online) {
		pr_debug("PMIC: BSV set\n");
		msm_otg_dbg_log_event(&motg->phy, "PMIC: BSV SET",
				init, motg->inputs);
		if (test_and_set_bit(B_SESS_VLD, &motg->inputs) && init)
			return;
	} else {
		pr_debug("PMIC: BSV clear\n");
		msm_otg_dbg_log_event(&motg->phy, "PMIC: BSV CLEAR",
				init, motg->inputs);
		motg->is_ext_chg_dcp = false;
		if (!test_and_clear_bit(B_SESS_VLD, &motg->inputs) && init)
			return;
	}

	/* do not queue state m/c work if id is grounded */
	if (!test_bit(ID, &motg->inputs) &&
		!motg->pdata->vbus_low_as_hostmode) {
		/*
		 * state machine work waits for initial VBUS
		 * completion in UNDEFINED state.  Process
		 * the initial VBUS event in ID_GND state.
		 */
		if (init)
			return;
	}

	if (!init) {
		init = true;
		if (pmic_vbus_init.done &&
				test_bit(B_SESS_VLD, &motg->inputs)) {
			pr_debug("PMIC: BSV came late\n");
			msm_otg_dbg_log_event(&motg->phy, "PMIC: BSV CAME LATE",
					init, motg->inputs);
			goto out;
		}

		if (motg->pdata->vbus_low_as_hostmode &&
			!test_bit(B_SESS_VLD, &motg->inputs)) {
			motg->id_state = USB_ID_GROUND;
			clear_bit(ID, &motg->inputs);
		}
		complete(&pmic_vbus_init);
		pr_debug("PMIC: BSV init complete\n");
		msm_otg_dbg_log_event(&motg->phy, "PMIC: BSV INIT COMPLETE",
				init, motg->inputs);
		return;
	}

out:
	if (motg->is_ext_chg_dcp) {
		if (test_bit(B_SESS_VLD, &motg->inputs)) {
			msm_otg_notify_charger(motg, IDEV_CHG_MAX);
		} else {
			motg->is_ext_chg_dcp = false;
			motg->chg_state = USB_CHG_STATE_UNDEFINED;
			motg->chg_type = USB_INVALID_CHARGER;
			msm_otg_notify_charger(motg, 0);
		}
		return;
	}

	msm_otg_dbg_log_event(&motg->phy, "CHECK VBUS EVENT DURING SUSPEND",
			atomic_read(&motg->pm_suspended),
			motg->sm_work_pending);

	/* Move to host mode on vbus low if required */
	if (motg->pdata->vbus_low_as_hostmode) {
		if (!test_bit(B_SESS_VLD, &motg->inputs)) {
			clear_bit(ID, &motg->inputs);
		} else {
			set_bit(ID, &motg->inputs);
		}
	}
	msm_otg_kick_sm_work(motg);
}

static void msm_id_status_w(struct work_struct *w)
{
	struct msm_otg *motg = container_of(w, struct msm_otg,
						id_status_work.work);
	int work = 0;

	dev_dbg(motg->phy.dev, "ID status_w\n");

	if (motg->pdata->pmic_id_irq)
		motg->id_state = msm_otg_read_pmic_id_state(motg);
	else if (motg->ext_id_irq)
		motg->id_state = gpio_get_value(motg->pdata->usb_id_gpio);
	else if (motg->phy_irq)
		motg->id_state = msm_otg_read_phy_id_state(motg);

	if (motg->err_event_seen)
		return;

	if (motg->id_state) {
		if (gpio_is_valid(motg->pdata->switch_sel_gpio))
			gpio_direction_input(motg->pdata->switch_sel_gpio);
		if (!test_and_set_bit(ID, &motg->inputs)) {
			pr_debug("ID set\n");
			msm_otg_dbg_log_event(&motg->phy, "ID SET",
					motg->inputs, motg->phy.state);
			work = 1;
		}
	} else {
		if (gpio_is_valid(motg->pdata->switch_sel_gpio))
			gpio_direction_output(motg->pdata->switch_sel_gpio, 1);
		if (test_and_clear_bit(ID, &motg->inputs)) {
			pr_debug("ID clear\n");
			msm_otg_dbg_log_event(&motg->phy, "ID CLEAR",
					motg->inputs, motg->phy.state);
			work = 1;
		}
	}

	if (work && (motg->phy.state != OTG_STATE_UNDEFINED)) {
		msm_otg_dbg_log_event(&motg->phy,
				"CHECK ID EVENT DURING SUSPEND",
				atomic_read(&motg->pm_suspended),
				motg->sm_work_pending);
		msm_otg_kick_sm_work(motg);
	}
}

#define MSM_ID_STATUS_DELAY	5 /* 5msec */
static irqreturn_t msm_id_irq(int irq, void *data)
{
	struct msm_otg *motg = data;

	/*schedule delayed work for 5msec for ID line state to settle*/
	queue_delayed_work(motg->otg_wq, &motg->id_status_work,
			msecs_to_jiffies(MSM_ID_STATUS_DELAY));

	return IRQ_HANDLED;
}

int msm_otg_pm_notify(struct notifier_block *notify_block,
					unsigned long mode, void *unused)
{
	struct msm_otg *motg = container_of(
		notify_block, struct msm_otg, pm_notify);

	dev_dbg(motg->phy.dev, "OTG PM notify:%lx, sm_pending:%u\n", mode,
					motg->sm_work_pending);
	msm_otg_dbg_log_event(&motg->phy, "PM NOTIFY",
			mode, motg->sm_work_pending);

	switch (mode) {
	case PM_POST_SUSPEND:
		/* OTG sm_work can be armed now */
		atomic_set(&motg->pm_suspended, 0);

		/* Handle any deferred wakeup events from USB during suspend */
		if (motg->sm_work_pending) {
			motg->sm_work_pending = false;
			queue_work(motg->otg_wq, &motg->sm_work);
		}
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static int msm_otg_mode_show(struct seq_file *s, void *unused)
{
	struct msm_otg *motg = s->private;
	struct usb_otg *otg = motg->phy.otg;

	switch (otg->phy->state) {
	case OTG_STATE_A_HOST:
		seq_printf(s, "host\n");
		break;
	case OTG_STATE_B_IDLE:
	case OTG_STATE_B_PERIPHERAL:
	case OTG_STATE_B_SUSPEND:
		seq_printf(s, "peripheral\n");
		break;
	default:
		seq_printf(s, "none\n");
		break;
	}

	return 0;
}

static int msm_otg_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_otg_mode_show, inode->i_private);
}

static ssize_t msm_otg_mode_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct msm_otg *motg = s->private;
	char buf[16];
	struct usb_phy *phy = &motg->phy;
	int status = count;
	enum usb_mode_type req_mode;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		status = -EFAULT;
		goto out;
	}

	if (!strncmp(buf, "host", 4)) {
		req_mode = USB_HOST;
	} else if (!strncmp(buf, "peripheral", 10)) {
		req_mode = USB_PERIPHERAL;
	} else if (!strncmp(buf, "none", 4)) {
		req_mode = USB_NONE;
	} else {
		status = -EINVAL;
		goto out;
	}

	switch (req_mode) {
	case USB_NONE:
		switch (phy->state) {
		case OTG_STATE_A_HOST:
		case OTG_STATE_B_PERIPHERAL:
		case OTG_STATE_B_SUSPEND:
			set_bit(ID, &motg->inputs);
			clear_bit(B_SESS_VLD, &motg->inputs);
			break;
		default:
			goto out;
		}
		break;
	case USB_PERIPHERAL:
		switch (phy->state) {
		case OTG_STATE_B_IDLE:
		case OTG_STATE_A_HOST:
			set_bit(ID, &motg->inputs);
			set_bit(B_SESS_VLD, &motg->inputs);
			break;
		default:
			goto out;
		}
		break;
	case USB_HOST:
		switch (phy->state) {
		case OTG_STATE_B_IDLE:
		case OTG_STATE_B_PERIPHERAL:
		case OTG_STATE_B_SUSPEND:
			clear_bit(ID, &motg->inputs);
			break;
		default:
			goto out;
		}
		break;
	default:
		goto out;
	}

	motg->id_state = (test_bit(ID, &motg->inputs)) ? USB_ID_FLOAT :
							USB_ID_GROUND;
	queue_work(motg->otg_wq, &motg->sm_work);
out:
	return status;
}

const struct file_operations msm_otg_mode_fops = {
	.open = msm_otg_mode_open,
	.read = seq_read,
	.write = msm_otg_mode_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int msm_otg_show_otg_state(struct seq_file *s, void *unused)
{
	struct msm_otg *motg = s->private;
	struct usb_phy *phy = &motg->phy;

	seq_printf(s, "%s\n", usb_otg_state_string(phy->state));
	return 0;
}

static int msm_otg_otg_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_otg_show_otg_state, inode->i_private);
}

const struct file_operations msm_otg_state_fops = {
	.open = msm_otg_otg_state_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int msm_otg_show_chg_type(struct seq_file *s, void *unused)
{
	struct msm_otg *motg = s->private;

	seq_printf(s, "%s\n", chg_to_string(motg->chg_type));
	return 0;
}

static int msm_otg_chg_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_otg_show_chg_type, inode->i_private);
}

const struct file_operations msm_otg_chg_fops = {
	.open = msm_otg_chg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int msm_otg_bus_show(struct seq_file *s, void *unused)
{
	if (debug_bus_voting_enabled)
		seq_printf(s, "enabled\n");
	else
		seq_printf(s, "disabled\n");

	return 0;
}

static int msm_otg_bus_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_otg_bus_show, inode->i_private);
}

static ssize_t msm_otg_bus_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char buf[8];
	struct seq_file *s = file->private_data;
	struct msm_otg *motg = s->private;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "enable", 6)) {
		/* Do not vote here. Let OTG statemachine decide when to vote */
		debug_bus_voting_enabled = true;
	} else {
		debug_bus_voting_enabled = false;
		msm_otg_bus_vote(motg, USB_MIN_PERF_VOTE);
	}

	return count;
}

static int msm_otg_dbg_buff_show(struct seq_file *s, void *unused)
{
	struct msm_otg *motg = s->private;
	unsigned long	flags;
	unsigned	i;

	read_lock_irqsave(&motg->dbg_lock, flags);

	i = motg->dbg_idx;
	if (strnlen(motg->buf[i], DEBUG_MSG_LEN))
		seq_printf(s, "%s\n", motg->buf[i]);
	for (dbg_inc(&i); i != motg->dbg_idx;  dbg_inc(&i)) {
		if (!strnlen(motg->buf[i], DEBUG_MSG_LEN))
			continue;
		seq_printf(s, "%s\n", motg->buf[i]);
	}
	read_unlock_irqrestore(&motg->dbg_lock, flags);

	return 0;
}

static int msm_otg_dbg_buff_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_otg_dbg_buff_show, inode->i_private);
}

const struct file_operations msm_otg_dbg_buff_fops = {
	.open = msm_otg_dbg_buff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int
otg_get_prop_usbin_voltage_now(struct msm_otg *motg)
{
	int rc = 0;
	struct qpnp_vadc_result results;

	if (IS_ERR_OR_NULL(motg->vadc_dev)) {
		motg->vadc_dev = qpnp_get_vadc(motg->phy.dev, "usbin");
		if (IS_ERR(motg->vadc_dev))
			return PTR_ERR(motg->vadc_dev);
	}

	rc = qpnp_vadc_read(motg->vadc_dev, USBIN, &results);
	if (rc) {
		pr_err("Unable to read usbin rc=%d\n", rc);
		return 0;
	} else {
		return results.physical;
	}
}

static int msm_otg_pmic_dp_dm(struct msm_otg *motg, int value)
{
	int ret = 0;

	switch (value) {
	case POWER_SUPPLY_DP_DM_DPF_DMF:
		if (!motg->rm_pulldown) {
			ret = msm_hsusb_ldo_enable(motg, USB_PHY_REG_3P3_ON);
			if (!ret) {
				motg->rm_pulldown = true;
				msm_otg_dbg_log_event(&motg->phy, "RM Pulldown",
						motg->rm_pulldown, 0);
			}
		}
		break;
	case POWER_SUPPLY_DP_DM_DPR_DMR:
		if (motg->rm_pulldown) {
			ret = msm_hsusb_ldo_enable(motg, USB_PHY_REG_3P3_OFF);
			if (!ret) {
				motg->rm_pulldown = false;
				msm_otg_dbg_log_event(&motg->phy, "RM Pulldown",
						motg->rm_pulldown, 0);
			}
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int otg_power_get_property_usb(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct msm_otg *motg = container_of(psy, struct msm_otg, usb_psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_SCOPE:
		if (motg->host_mode)
			val->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		else
			val->intval = POWER_SUPPLY_SCOPE_DEVICE;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = motg->voltage_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = motg->current_max;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		val->intval = motg->typec_current_max;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = !!test_bit(B_SESS_VLD, &motg->inputs);
		break;
	case POWER_SUPPLY_PROP_DP_DM:
		val->intval = motg->rm_pulldown;
		break;
	/* Reflect USB enumeration */
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = motg->online;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = motg->usb_supply_type;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = psy->type;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = motg->usbin_health;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = otg_get_prop_usbin_voltage_now(motg);
		break;
	case POWER_SUPPLY_PROP_USB_OTG:
		val->intval = !motg->id_state;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int otg_power_set_property_usb(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct msm_otg *motg = container_of(psy, struct msm_otg, usb_psy);
	struct msm_otg_platform_data *pdata = motg->pdata;

	msm_otg_dbg_log_event(&motg->phy, "SET PWR PROPERTY",
				psp, motg->usb_supply_type);
	switch (psp) {
	case POWER_SUPPLY_PROP_USB_OTG:
		motg->id_state = val->intval ? USB_ID_GROUND : USB_ID_FLOAT;
		queue_delayed_work(motg->otg_wq, &motg->id_status_work, 0);
		break;
	/* PMIC notification for DP DM state */
	case POWER_SUPPLY_PROP_DP_DM:
		msm_otg_pmic_dp_dm(motg, val->intval);
		break;
	/* Process PMIC notification in PRESENT prop */
	case POWER_SUPPLY_PROP_PRESENT:
		msm_otg_set_vbus_state(val->intval);
		break;
	/* The ONLINE property reflects if usb has enumerated */
	case POWER_SUPPLY_PROP_ONLINE:
		motg->online = val->intval;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		motg->voltage_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		motg->current_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		/*
		 * Limit type-c charger current to 500 for SDP charger
		 * to avoid more current drawn than 500 with legacy Hosts.
		 */
		if (pdata->enable_sdp_typec_current_limit &&
				(motg->chg_type == USB_SDP_CHARGER)
					&& val->intval > 500)
			motg->typec_current_max = 500;
		else
			motg->typec_current_max = val->intval;

		msm_otg_dbg_log_event(&motg->phy, "type-c charger",
					val->intval, motg->bc1p2_current_max);
		/* Update chg_current as per type-c charger detection on VBUS */
		if (motg->chg_type != USB_INVALID_CHARGER) {
			dev_dbg(motg->phy.dev, "update type-c charger\n");
			msm_otg_notify_charger(motg, motg->bc1p2_current_max);
		}
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		motg->usb_supply_type = val->intval;
		/*
		 * Update TYPE property to DCP for HVDCP/HVDCP3 charger types
		 * so that they can be recongized as AC chargers by healthd.
		 * Don't report UNKNOWN charger type to prevent healthd missing
		 * detecting this power_supply status change.
		 */
		if (motg->usb_supply_type == POWER_SUPPLY_TYPE_USB_HVDCP_3
			|| motg->usb_supply_type == POWER_SUPPLY_TYPE_USB_HVDCP)
			psy->type = POWER_SUPPLY_TYPE_USB_DCP;
		else if (motg->usb_supply_type == POWER_SUPPLY_TYPE_UNKNOWN)
			psy->type = POWER_SUPPLY_TYPE_USB;
		else
			psy->type = motg->usb_supply_type;
		/*
		 * If charger detection is done by the USB driver,
		 * motg->chg_type is already assigned in the
		 * charger detection work.
		 *
		 * There is a possibility of overriding the
		 * actual charger type with power supply type
		 * charger. For example USB PROPRIETARY charger
		 * does not exist in power supply enum and it
		 * gets overridden as DCP.
		 */
		if (motg->chg_state == USB_CHG_STATE_DETECTED)
			break;

		switch (motg->usb_supply_type) {
		case POWER_SUPPLY_TYPE_USB:
			motg->chg_type = USB_SDP_CHARGER;
			motg->voltage_max = MICRO_5V;
			motg->current_max = SDP_CURRENT_UA;
			break;
		case POWER_SUPPLY_TYPE_USB_DCP:
			motg->chg_type = USB_DCP_CHARGER;
			motg->voltage_max = MICRO_5V;
			motg->current_max = DCP_CURRENT_UA;
			break;
		case POWER_SUPPLY_TYPE_USB_HVDCP:
			motg->chg_type = USB_DCP_CHARGER;
			motg->voltage_max = MICRO_9V;
			motg->current_max = HVDCP_CURRENT_UA;
			msm_otg_notify_charger(motg, hvdcp_max_current);
			break;
		case POWER_SUPPLY_TYPE_USB_CDP:
			motg->chg_type = USB_CDP_CHARGER;
			motg->voltage_max = MICRO_5V;
			motg->current_max = CDP_CURRENT_UA;
			break;
		case POWER_SUPPLY_TYPE_USB_ACA:
			motg->chg_type = USB_PROPRIETARY_CHARGER;
			break;
		default:
			motg->chg_type = USB_INVALID_CHARGER;
			break;
		}

		if (motg->chg_type != USB_INVALID_CHARGER) {
			if (motg->chg_type == USB_DCP_CHARGER)
				motg->is_ext_chg_dcp = true;
			motg->chg_state = USB_CHG_STATE_DETECTED;
		}

		dev_dbg(motg->phy.dev, "%s: charger type = %s\n", __func__,
			chg_to_string(motg->chg_type));
		msm_otg_dbg_log_event(&motg->phy, "SET CHARGER TYPE ",
				motg->chg_type, motg->usb_supply_type);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval > POWER_SUPPLY_HEALTH_HOT)
			motg->usbin_health = 0;
		else
			motg->usbin_health = val->intval;
		break;
	default:
		return -EINVAL;
	}

	power_supply_changed(&motg->usb_psy);
	return 0;
}

static int otg_power_property_is_writeable_usb(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_DP_DM:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
	case POWER_SUPPLY_PROP_USB_OTG:
	case POWER_SUPPLY_PROP_REAL_TYPE:
		return 1;
	default:
		break;
	}

	return 0;
}

static char *otg_pm_power_supplied_to[] = {
	"battery",
};

static enum power_supply_property otg_pm_power_props_usb[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_DP_DM,
	POWER_SUPPLY_PROP_USB_OTG,
	POWER_SUPPLY_PROP_REAL_TYPE,
};

const struct file_operations msm_otg_bus_fops = {
	.open = msm_otg_bus_open,
	.read = seq_read,
	.write = msm_otg_bus_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *msm_otg_dbg_root;

static int msm_otg_debugfs_init(struct msm_otg *motg)
{
	struct dentry *msm_otg_dentry;
	struct msm_otg_platform_data *pdata = motg->pdata;

	msm_otg_dbg_root = debugfs_create_dir("msm_otg", NULL);

	if (!msm_otg_dbg_root || IS_ERR(msm_otg_dbg_root))
		return -ENODEV;

	if ((pdata->mode == USB_OTG || pdata->mode == USB_PERIPHERAL) &&
		pdata->otg_control == OTG_USER_CONTROL) {

		msm_otg_dentry = debugfs_create_file("mode", S_IRUGO |
			S_IWUSR, msm_otg_dbg_root, motg,
			&msm_otg_mode_fops);

		if (!msm_otg_dentry) {
			debugfs_remove(msm_otg_dbg_root);
			msm_otg_dbg_root = NULL;
			return -ENODEV;
		}
	}

	msm_otg_dentry = debugfs_create_file("chg_type", S_IRUGO,
		msm_otg_dbg_root, motg,
		&msm_otg_chg_fops);

	if (!msm_otg_dentry) {
		debugfs_remove_recursive(msm_otg_dbg_root);
		return -ENODEV;
	}

	msm_otg_dentry = debugfs_create_file("bus_voting", S_IRUGO | S_IWUSR,
		msm_otg_dbg_root, motg,
		&msm_otg_bus_fops);

	if (!msm_otg_dentry) {
		debugfs_remove_recursive(msm_otg_dbg_root);
		return -ENODEV;
	}

	msm_otg_dentry = debugfs_create_file("otg_state", S_IRUGO,
				msm_otg_dbg_root, motg, &msm_otg_state_fops);

	if (!msm_otg_dentry) {
		debugfs_remove_recursive(msm_otg_dbg_root);
		return -ENODEV;
	}

	msm_otg_dentry = debugfs_create_file("dbg_buff", S_IRUGO,
		msm_otg_dbg_root, motg, &msm_otg_dbg_buff_fops);

	if (!msm_otg_dentry) {
		debugfs_remove_recursive(msm_otg_dbg_root);
		return -ENODEV;
	}
	return 0;
}

static void msm_otg_debugfs_cleanup(void)
{
	debugfs_remove_recursive(msm_otg_dbg_root);
}

static ssize_t
set_msm_otg_perf_mode(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct msm_otg *motg = the_msm_otg;
	int ret;
	long clk_rate;

	pr_debug("%s: enable:%d\n", __func__, !strnicmp(buf, "enable", 6));

	if (!strnicmp(buf, "enable", 6)) {
		clk_rate = motg->core_clk_nominal_rate;
		msm_otg_bus_freq_set(motg, USB_NOC_NOM_VOTE);
	} else {
		clk_rate = motg->core_clk_svs_rate;
		msm_otg_bus_freq_set(motg, USB_NOC_SVS_VOTE);
	}

	if (clk_rate) {
		pr_debug("Set usb sys_clk rate:%ld\n", clk_rate);
		ret = clk_set_rate(motg->core_clk, clk_rate);
		if (ret)
			pr_err("sys_clk set_rate fail:%d %ld\n", ret, clk_rate);
		msm_otg_dbg_log_event(&motg->phy, "OTG PERF SET",
							clk_rate, ret);
	} else {
		pr_err("usb sys_clk rate is undefined\n");
	}

	return count;
}

static DEVICE_ATTR(perf_mode, S_IWUSR, NULL, set_msm_otg_perf_mode);

#define MSM_OTG_CMD_ID		0x09
#define MSM_OTG_DEVICE_ID	0x04
#define MSM_OTG_VMID_IDX	0xFF
#define MSM_OTG_MEM_TYPE	0x02
struct msm_otg_scm_cmd_buf {
	unsigned int device_id;
	unsigned int vmid_idx;
	unsigned int mem_type;
} __attribute__ ((__packed__));

static void msm_otg_pnoc_errata_fix(struct msm_otg *motg)
{
	int ret;
	struct msm_otg_platform_data *pdata = motg->pdata;
	struct msm_otg_scm_cmd_buf cmd_buf;

	if (!pdata->pnoc_errata_fix)
		return;

	dev_dbg(motg->phy.dev, "applying fix for pnoc h/w issue\n");

	cmd_buf.device_id = MSM_OTG_DEVICE_ID;
	cmd_buf.vmid_idx = MSM_OTG_VMID_IDX;
	cmd_buf.mem_type = MSM_OTG_MEM_TYPE;

	ret = scm_call(SCM_SVC_MP, MSM_OTG_CMD_ID, &cmd_buf,
				sizeof(cmd_buf), NULL, 0);

	if (ret)
		dev_err(motg->phy.dev, "scm command failed to update VMIDMT\n");
}

static u64 msm_otg_dma_mask = DMA_BIT_MASK(32);
static struct platform_device *msm_otg_add_pdev(
		struct platform_device *ofdev, const char *name)
{
	struct platform_device *pdev;
	const struct resource *res = ofdev->resource;
	unsigned int num = ofdev->num_resources;
	int retval;
	struct ci13xxx_platform_data ci_pdata;
	struct msm_otg_platform_data *otg_pdata;
	struct msm_otg *motg;

	pdev = platform_device_alloc(name, -1);
	if (!pdev) {
		retval = -ENOMEM;
		goto error;
	}

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	pdev->dev.dma_mask = &msm_otg_dma_mask;
	pdev->dev.parent = &ofdev->dev;

	if (num) {
		retval = platform_device_add_resources(pdev, res, num);
		if (retval)
			goto error;
	}

	if (!strcmp(name, "msm_hsusb")) {
		otg_pdata =
			(struct msm_otg_platform_data *)
				ofdev->dev.platform_data;
		motg = platform_get_drvdata(ofdev);
		ci_pdata.log2_itc = otg_pdata->log2_itc;
		ci_pdata.usb_core_id = 0;
		ci_pdata.l1_supported = otg_pdata->l1_supported;
		ci_pdata.enable_ahb2ahb_bypass =
				otg_pdata->enable_ahb2ahb_bypass;
		ci_pdata.enable_streaming = otg_pdata->enable_streaming;
		ci_pdata.enable_axi_prefetch = otg_pdata->enable_axi_prefetch;
		retval = platform_device_add_data(pdev, &ci_pdata,
			sizeof(ci_pdata));
		if (retval)
			goto error;
	}

	retval = platform_device_add(pdev);
	if (retval)
		goto error;

	return pdev;

error:
	platform_device_put(pdev);
	return ERR_PTR(retval);
}

static int msm_otg_setup_devices(struct platform_device *ofdev,
		enum usb_mode_type mode, bool init)
{
	const char *gadget_name = "msm_hsusb";
	const char *host_name = "msm_hsusb_host";
	static struct platform_device *gadget_pdev;
	static struct platform_device *host_pdev;
	int retval = 0;

	if (!init) {
		if (gadget_pdev) {
			platform_device_unregister(gadget_pdev);
			device_remove_file(&gadget_pdev->dev,
					   &dev_attr_perf_mode);
		}
		if (host_pdev)
			platform_device_unregister(host_pdev);
		return 0;
	}

	switch (mode) {
	case USB_OTG:
		/* fall through */
	case USB_PERIPHERAL:
		gadget_pdev = msm_otg_add_pdev(ofdev, gadget_name);
		if (IS_ERR(gadget_pdev)) {
			retval = PTR_ERR(gadget_pdev);
			break;
		}
		if (device_create_file(&gadget_pdev->dev, &dev_attr_perf_mode))
			dev_err(&gadget_pdev->dev, "perf_mode file failed\n");
		if (mode == USB_PERIPHERAL)
			break;
		/* fall through */
	case USB_HOST:
		host_pdev = msm_otg_add_pdev(ofdev, host_name);
		if (IS_ERR(host_pdev)) {
			retval = PTR_ERR(host_pdev);
			if (mode == USB_OTG) {
				platform_device_unregister(gadget_pdev);
				device_remove_file(&gadget_pdev->dev,
						   &dev_attr_perf_mode);
			}
		}
		break;
	default:
		break;
	}

	return retval;
}

static int msm_otg_register_power_supply(struct platform_device *pdev,
					struct msm_otg *motg)
{
	int ret;

	ret = power_supply_register(&pdev->dev, &motg->usb_psy);
	if (ret < 0) {
		dev_err(motg->phy.dev,
			"%s:power_supply_register usb failed\n",
			__func__);
		return ret;
	}

	return 0;
}

static int msm_otg_ext_chg_open(struct inode *inode, struct file *file)
{
	struct msm_otg *motg = the_msm_otg;

	pr_debug("msm_otg ext chg open\n");
	msm_otg_dbg_log_event(&motg->phy, "EXT CHG: OPEN",
			motg->inputs, motg->phy.state);

	motg->ext_chg_opened = true;
	file->private_data = (void *)motg;
	return 0;
}

static long
msm_otg_ext_chg_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct msm_otg *motg = file->private_data;
	struct msm_usb_chg_info info = {0};
	int ret = 0, val;

	msm_otg_dbg_log_event(&motg->phy, "EXT CHG: IOCTL", cmd, 0);
	switch (cmd) {
	case MSM_USB_EXT_CHG_INFO:
		info.chg_block_type = USB_CHG_BLOCK_ULPI;
		info.page_offset = motg->io_res->start & ~PAGE_MASK;
		/* mmap() works on PAGE granularity */
		info.length = PAGE_SIZE;

		if (copy_to_user((void __user *)arg, &info, sizeof(info))) {
			pr_err("%s: copy to user failed\n\n", __func__);
			ret = -EFAULT;
		}
		break;
	case MSM_USB_EXT_CHG_BLOCK_LPM:
		if (get_user(val, (int __user *)arg)) {
			pr_err("%s: get_user failed\n\n", __func__);
			ret = -EFAULT;
			break;
		}
		pr_debug("%s: LPM block request %d\n", __func__, val);
		msm_otg_dbg_log_event(&motg->phy, "LPM BLOCK REQ", val, 0);
		if (val) { /* block LPM */
			if (motg->chg_type == USB_DCP_CHARGER) {
				motg->ext_chg_active = ACTIVE;
				msm_otg_dbg_log_event(&motg->phy,
				      "PM RUNTIME: EXT_CHG GET",
				      get_pm_runtime_counter(motg->phy.dev), 0);
				pm_runtime_get_sync(motg->phy.dev);
			} else {
				motg->ext_chg_active = INACTIVE;
				complete(&motg->ext_chg_wait);
				ret = -ENODEV;
			}
		} else {
			motg->ext_chg_active = INACTIVE;
			complete(&motg->ext_chg_wait);
			/*
			 * If usb cable is disconnected and then userspace
			 * calls ioctl to unblock low power mode, make sure
			 * otg_sm work for usb disconnect is processed first
			 * followed by decrementing the PM usage counters.
			 */
			flush_work(&motg->sm_work);
			msm_otg_dbg_log_event(&motg->phy,
				"PM RUNTIME: EXT_CHG PUT",
				get_pm_runtime_counter(motg->phy.dev), 0);
			pm_runtime_put_sync(motg->phy.dev);
		}
		break;
	case MSM_USB_EXT_CHG_VOLTAGE_INFO:
		if (get_user(val, (int __user *)arg)) {
			pr_err("%s: get_user failed\n\n", __func__);
			ret = -EFAULT;
			break;
		}
		msm_otg_dbg_log_event(&motg->phy, "EXT CHG: VOL REQ", cmd, val);

		if (val == USB_REQUEST_5V)
			pr_debug("%s:voting 5V voltage request\n", __func__);
		else if (val == USB_REQUEST_9V)
			pr_debug("%s:voting 9V voltage request\n", __func__);
		break;
	case MSM_USB_EXT_CHG_RESULT:
		if (get_user(val, (int __user *)arg)) {
			pr_err("%s: get_user failed\n\n", __func__);
			ret = -EFAULT;
			break;
		}
		msm_otg_dbg_log_event(&motg->phy, "EXT CHG: VOL REQ", cmd, val);

		if (!val)
			pr_debug("%s:voltage request successful\n", __func__);
		else
			pr_debug("%s:voltage request failed\n", __func__);
		break;
	case MSM_USB_EXT_CHG_TYPE:
		if (get_user(val, (int __user *)arg)) {
			pr_err("%s: get_user failed\n\n", __func__);
			ret = -EFAULT;
			break;
		}
		msm_otg_dbg_log_event(&motg->phy, "EXT CHG: VOL REQ", cmd, val);

		if (val)
			pr_debug("%s:charger is external charger\n", __func__);
		else
			pr_debug("%s:charger is not ext charger\n", __func__);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int msm_otg_ext_chg_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct msm_otg *motg = file->private_data;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	int ret;

	if (vma->vm_pgoff || vsize > PAGE_SIZE)
		return -EINVAL;

	vma->vm_pgoff = __phys_to_pfn(motg->io_res->start);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				 vsize, vma->vm_page_prot);
	if (ret < 0) {
		pr_err("%s: failed with return val %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int msm_otg_ext_chg_release(struct inode *inode, struct file *file)
{
	struct msm_otg *motg = file->private_data;

	pr_debug("msm_otg ext chg release\n");
	msm_otg_dbg_log_event(&motg->phy, "EXT CHG: RELEASE",
			motg->inputs, motg->phy.state);

	motg->ext_chg_opened = false;

	return 0;
}

static const struct file_operations msm_otg_ext_chg_fops = {
	.owner = THIS_MODULE,
	.open = msm_otg_ext_chg_open,
	.unlocked_ioctl = msm_otg_ext_chg_ioctl,
	.mmap = msm_otg_ext_chg_mmap,
	.release = msm_otg_ext_chg_release,
};

static int msm_otg_setup_ext_chg_cdev(struct msm_otg *motg)
{
	int ret;

	if (motg->pdata->enable_sec_phy || motg->pdata->mode == USB_HOST ||
			motg->pdata->otg_control != OTG_PMIC_CONTROL ||
			psy != &motg->usb_psy) {
		pr_debug("usb ext chg is not supported by msm otg\n");
		return -ENODEV;
	}

	ret = alloc_chrdev_region(&motg->ext_chg_dev, 0, 1, "usb_ext_chg");
	if (ret < 0) {
		pr_err("Fail to allocate usb ext char dev region\n");
		return ret;
	}
	motg->ext_chg_class = class_create(THIS_MODULE, "msm_ext_chg");
	if (ret < 0) {
		pr_err("Fail to create usb ext chg class\n");
		goto unreg_chrdev;
	}
	cdev_init(&motg->ext_chg_cdev, &msm_otg_ext_chg_fops);
	motg->ext_chg_cdev.owner = THIS_MODULE;

	ret = cdev_add(&motg->ext_chg_cdev, motg->ext_chg_dev, 1);
	if (ret < 0) {
		pr_err("Fail to add usb ext chg cdev\n");
		goto destroy_class;
	}
	motg->ext_chg_device = device_create(motg->ext_chg_class,
					NULL, motg->ext_chg_dev, NULL,
					"usb_ext_chg");
	if (IS_ERR(motg->ext_chg_device)) {
		pr_err("Fail to create usb ext chg device\n");
		ret = PTR_ERR(motg->ext_chg_device);
		motg->ext_chg_device = NULL;
		goto del_cdev;
	}

	init_completion(&motg->ext_chg_wait);
	pr_debug("msm otg ext chg cdev setup success\n");
	return 0;

del_cdev:
	cdev_del(&motg->ext_chg_cdev);
destroy_class:
	class_destroy(motg->ext_chg_class);
unreg_chrdev:
	unregister_chrdev_region(motg->ext_chg_dev, 1);

	return ret;
}

static ssize_t dpdm_pulldown_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct msm_otg *motg = the_msm_otg;
	struct msm_otg_platform_data *pdata = motg->pdata;

	return snprintf(buf, PAGE_SIZE, "%s\n", pdata->dpdm_pulldown_added ?
							"enabled" : "disabled");
}

static ssize_t dpdm_pulldown_enable_store(struct device *dev,
		struct device_attribute *attr, const char
		*buf, size_t size)
{
	struct msm_otg *motg = the_msm_otg;
	struct msm_otg_platform_data *pdata = motg->pdata;

	if (!strnicmp(buf, "enable", 6)) {
		pdata->dpdm_pulldown_added = true;
		return size;
	} else if (!strnicmp(buf, "disable", 7)) {
		pdata->dpdm_pulldown_added = false;
		return size;
	}

	return -EINVAL;
}

static DEVICE_ATTR(dpdm_pulldown_enable, S_IRUGO | S_IWUSR,
		dpdm_pulldown_enable_show, dpdm_pulldown_enable_store);

struct msm_otg_platform_data *msm_otg_dt_to_pdata(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct msm_otg_platform_data *pdata;
	int len = 0;
	int res_gpio;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("unable to allocate platform data\n");
		return NULL;
	}
	of_get_property(node, "qcom,hsusb-otg-phy-init-seq", &len);
	if (len) {
		pdata->phy_init_seq = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
		if (!pdata->phy_init_seq)
			return NULL;
		of_property_read_u32_array(node, "qcom,hsusb-otg-phy-init-seq",
				pdata->phy_init_seq,
				len/sizeof(*pdata->phy_init_seq));
	}
	of_property_read_u32(node, "qcom,hsusb-otg-power-budget",
				&pdata->power_budget);
	of_property_read_u32(node, "qcom,hsusb-otg-mode",
				&pdata->mode);
	of_property_read_u32(node, "qcom,hsusb-otg-otg-control",
				&pdata->otg_control);
	of_property_read_u32(node, "qcom,hsusb-otg-default-mode",
				&pdata->default_mode);
	of_property_read_u32(node, "qcom,hsusb-otg-phy-type",
				&pdata->phy_type);
	pdata->disable_reset_on_disconnect = of_property_read_bool(node,
				"qcom,hsusb-otg-disable-reset");
	pdata->pnoc_errata_fix = of_property_read_bool(node,
				"qcom,hsusb-otg-pnoc-errata-fix");
	pdata->enable_lpm_on_dev_suspend = of_property_read_bool(node,
				"qcom,hsusb-otg-lpm-on-dev-suspend");
	pdata->core_clk_always_on_workaround = of_property_read_bool(node,
				"qcom,hsusb-otg-clk-always-on-workaround");
	pdata->delay_lpm_on_disconnect = of_property_read_bool(node,
				"qcom,hsusb-otg-delay-lpm");
	pdata->dp_manual_pullup = of_property_read_bool(node,
				"qcom,dp-manual-pullup");
	pdata->enable_sec_phy = of_property_read_bool(node,
					"qcom,usb2-enable-hsphy2");
	of_property_read_u32(node, "qcom,hsusb-log2-itc",
				&pdata->log2_itc);

	of_property_read_u32(node, "qcom,hsusb-otg-mpm-dpsehv-int",
				&pdata->mpm_dpshv_int);
	of_property_read_u32(node, "qcom,hsusb-otg-mpm-dmsehv-int",
				&pdata->mpm_dmshv_int);
	pdata->pmic_id_irq = platform_get_irq_byname(pdev, "pmic_id_irq");
	if (pdata->pmic_id_irq < 0)
		pdata->pmic_id_irq = 0;

	pdata->hub_reset_gpio = of_get_named_gpio(
			node, "qcom,hub-reset-gpio", 0);
	if (pdata->hub_reset_gpio < 0)
		pr_debug("hub_reset_gpio is not available\n");

	pdata->usbeth_reset_gpio = of_get_named_gpio(
			node, "qcom,usbeth-reset-gpio", 0);
	if (pdata->usbeth_reset_gpio < 0)
		pr_debug("usbeth_reset_gpio is not available\n");

	pdata->switch_sel_gpio =
			of_get_named_gpio(node, "qcom,sw-sel-gpio", 0);
	if (pdata->switch_sel_gpio < 0)
		pr_debug("switch_sel_gpio is not available\n");

	pdata->usb_id_gpio =
			of_get_named_gpio(node, "qcom,usbid-gpio", 0);
	if (pdata->usb_id_gpio < 0)
		pr_debug("usb_id_gpio is not available\n");

	pdata->l1_supported = of_property_read_bool(node,
				"qcom,hsusb-l1-supported");
	pdata->enable_ahb2ahb_bypass = of_property_read_bool(node,
				"qcom,ahb-async-bridge-bypass");
	pdata->disable_retention_with_vdd_min = of_property_read_bool(node,
				"qcom,disable-retention-with-vdd-min");
	pdata->enable_phy_id_pullup = of_property_read_bool(node,
				"qcom,enable-phy-id-pullup");
	pdata->phy_dvdd_always_on = of_property_read_bool(node,
				"qcom,phy-dvdd-always-on");

	res_gpio = of_get_named_gpio(node, "qcom,hsusb-otg-vddmin-gpio", 0);
	if (res_gpio < 0)
		res_gpio = 0;
	pdata->vddmin_gpio = res_gpio;

	pdata->emulation = of_property_read_bool(node,
						"qcom,emulation");

	pdata->enable_streaming = of_property_read_bool(node,
					"qcom,boost-sysclk-with-streaming");

	pdata->enable_axi_prefetch = of_property_read_bool(node,
						"qcom,axi-prefetch-enable");

	pdata->enable_sdp_typec_current_limit = of_property_read_bool(node,
					"qcom,enable-sdp-typec-current-limit");
	pdata->vbus_low_as_hostmode = of_property_read_bool(node,
					"qcom,vbus-low-as-hostmode");
	return pdata;
}

static int msm_otg_probe(struct platform_device *pdev)
{
	int ret = 0;
	int len = 0;
	u32 tmp[3];
	struct resource *res;
	struct msm_otg *motg;
	struct usb_phy *phy;
	struct msm_otg_platform_data *pdata;
	void __iomem *tcsr;
	int id_irq = 0;

	dev_info(&pdev->dev, "msm_otg probe\n");

	motg = kzalloc(sizeof(struct msm_otg), GFP_KERNEL);
	if (!motg) {
		dev_err(&pdev->dev, "unable to allocate msm_otg\n");
		ret = -ENOMEM;
		return ret;
	}

	/*
	 * USB Core is running its protocol engine based on CORE CLK,
	 * CORE CLK  must be running at >55Mhz for correct HSUSB
	 * operation and USB core cannot tolerate frequency changes on
	 * CORE CLK. For such USB cores, vote for maximum clk frequency
	 * on pclk source
	 */
	motg->core_clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(motg->core_clk)) {
		ret = PTR_ERR(motg->core_clk);
		motg->core_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to get core_clk\n");
		goto free_motg;
	}

	/*
	 * USB Core CLK can run at max freq if streaming is enabled. Hence,
	 * get Max supported clk frequency for USB Core CLK and request to set
	 * the same. Otherwise set USB Core CLK to defined default value.
	 */
	if (of_property_read_u32(pdev->dev.of_node,
					"qcom,max-nominal-sysclk-rate", &ret)) {
		ret = -EINVAL;
		goto put_core_clk;
	} else {
		motg->core_clk_nominal_rate = clk_round_rate(motg->core_clk,
							     ret);
	}

	if (of_property_read_u32(pdev->dev.of_node,
					"qcom,max-svs-sysclk-rate", &ret)) {
		dev_dbg(&pdev->dev, "core_clk svs freq not specified\n");
	} else {
		motg->core_clk_svs_rate = clk_round_rate(motg->core_clk, ret);
	}

	motg->default_noc_mode = USB_NOC_NOM_VOTE;
	if (of_property_read_bool(pdev->dev.of_node, "qcom,default-mode-svs")) {
		motg->core_clk_rate = motg->core_clk_svs_rate;
		motg->default_noc_mode = USB_NOC_SVS_VOTE;
	} else if (of_property_read_bool(pdev->dev.of_node,
					"qcom,boost-sysclk-with-streaming")) {
		motg->core_clk_rate = motg->core_clk_nominal_rate;
	} else {
		motg->core_clk_rate = clk_round_rate(motg->core_clk,
						USB_DEFAULT_SYSTEM_CLOCK);
	}

	if (IS_ERR_VALUE(motg->core_clk_rate)) {
		dev_err(&pdev->dev, "fail to get core clk max freq.\n");
	} else {
		ret = clk_set_rate(motg->core_clk, motg->core_clk_rate);
		if (ret)
			dev_err(&pdev->dev, "fail to set core_clk freq:%d\n",
									ret);
	}

	motg->pclk = clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(motg->pclk)) {
		ret = PTR_ERR(motg->pclk);
		motg->pclk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to get iface_clk\n");
		goto put_core_clk;
	}

	motg->xo_clk = clk_get(&pdev->dev, "xo");
	if (IS_ERR(motg->xo_clk)) {
		ret = PTR_ERR(motg->xo_clk);
		motg->xo_clk = NULL;
		if (ret == -EPROBE_DEFER)
			goto put_pclk;
	}

	/*
	 * On few platforms USB PHY is fed with sleep clk.
	 * Hence don't fail probe.
	 */
	motg->sleep_clk = devm_clk_get(&pdev->dev, "sleep_clk");
	if (IS_ERR(motg->sleep_clk)) {
		ret = PTR_ERR(motg->sleep_clk);
		motg->sleep_clk = NULL;
		if (ret == -EPROBE_DEFER)
			goto put_xo_clk;
		else
			dev_dbg(&pdev->dev, "failed to get sleep_clk\n");
	} else {
		ret = clk_prepare_enable(motg->sleep_clk);
		if (ret) {
			dev_err(&pdev->dev, "%s failed to vote sleep_clk%d\n",
						__func__, ret);
			goto put_xo_clk;
		}
	}

	/*
	 * If present, phy_reset_clk is used to reset the PHY, ULPI bridge
	 * and CSR Wrapper. This is a reset only clock.
	 */

	if (of_property_match_string(pdev->dev.of_node,
			"clock-names", "phy_reset_clk") >= 0) {
		motg->phy_reset_clk = devm_clk_get(&pdev->dev, "phy_reset_clk");
		if (IS_ERR(motg->phy_reset_clk)) {
			ret = PTR_ERR(motg->phy_reset_clk);
			goto disable_sleep_clk;
		}
	}

	/*
	 * If present, phy_por_clk is used to assert/de-assert phy POR
	 * input. This is a reset only clock. phy POR must be asserted
	 * after overriding the parameter registers via CSR wrapper or
	 * ULPI bridge.
	 */
	if (of_property_match_string(pdev->dev.of_node,
				"clock-names", "phy_por_clk") >= 0) {
		motg->phy_por_clk = devm_clk_get(&pdev->dev, "phy_por_clk");
		if (IS_ERR(motg->phy_por_clk)) {
			ret = PTR_ERR(motg->phy_por_clk);
			goto disable_sleep_clk;
		}
	}

	/*
	 * If present, phy_csr_clk is required for accessing PHY
	 * CSR registers via AHB2PHY interface.
	 */
	if (of_property_match_string(pdev->dev.of_node,
				"clock-names", "phy_csr_clk") >= 0) {
		motg->phy_csr_clk = devm_clk_get(&pdev->dev, "phy_csr_clk");
		if (IS_ERR(motg->phy_csr_clk)) {
			ret = PTR_ERR(motg->phy_csr_clk);
			goto disable_sleep_clk;
		} else {
			ret = clk_prepare_enable(motg->phy_csr_clk);
			if (ret) {
				dev_err(&pdev->dev,
					"fail to enable phy csr clk %d\n", ret);
				goto disable_sleep_clk;
			}
		}
	}

	of_property_read_u32(pdev->dev.of_node, "qcom,pm-qos-latency",
				&motg->pm_qos_latency);

	pdata = msm_otg_dt_to_pdata(pdev);
	if (!pdata) {
		ret = -ENOMEM;
		goto disable_phy_csr_clk;
	}
	pdev->dev.platform_data = pdata;

	pdata->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!pdata->bus_scale_table)
		dev_dbg(&pdev->dev, "bus scaling is disabled\n");

	if (pdata->phy_type == QUSB_ULPI_PHY) {
		if (of_property_match_string(pdev->dev.of_node,
					"clock-names", "phy_ref_clk") >= 0) {
			motg->phy_ref_clk = devm_clk_get(&pdev->dev,
						"phy_ref_clk");
			if (IS_ERR(motg->phy_ref_clk)) {
				ret = PTR_ERR(motg->phy_ref_clk);
				goto disable_phy_csr_clk;
			} else {
				ret = clk_prepare_enable(motg->phy_ref_clk);
				if (ret) {
					dev_err(&pdev->dev,
						"fail to enable phy ref clk %d\n",
						ret);
					goto disable_phy_csr_clk;
				}
			}
		}
	}

	motg->phy.otg = devm_kzalloc(&pdev->dev, sizeof(struct usb_otg),
							GFP_KERNEL);
	if (!motg->phy.otg) {
		dev_err(&pdev->dev, "unable to allocate usb_otg\n");
		ret = -ENOMEM;
		goto otg_remove_devices;
	}

	the_msm_otg = motg;
	motg->pdata = pdata;
	phy = &motg->phy;
	phy->dev = &pdev->dev;
	motg->pdev = pdev;
	motg->dbg_idx = 0;
	motg->dbg_lock = __RW_LOCK_UNLOCKED(lck);

	if (motg->pdata->bus_scale_table) {
		motg->bus_perf_client =
		    msm_bus_scale_register_client(motg->pdata->bus_scale_table);
		if (!motg->bus_perf_client) {
			dev_err(motg->phy.dev, "%s: Failed to register BUS\n"
						"scaling client!!\n", __func__);
		} else {
			debug_bus_voting_enabled = true;
			/* Some platforms require BUS vote to control clocks */
			msm_otg_bus_vote(motg, USB_MIN_PERF_VOTE);
		}
	}

	ret = msm_otg_bus_freq_get(motg);
	if (ret) {
		pr_err("failed to get noc clocks: %d\n", ret);
	} else {
		ret = msm_otg_bus_freq_set(motg, motg->default_noc_mode);
		if (ret)
			pr_err("failed to vote explicit noc rates: %d\n", ret);
	}

	/* initialize reset counter */
	motg->reset_counter = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "core");
	if (!res) {
		dev_err(&pdev->dev, "failed to get core iomem resource\n");
		ret = -ENODEV;
		goto devote_bus_bw;
	}

	motg->io_res = res;
	motg->regs = ioremap(res->start, resource_size(res));
	if (!motg->regs) {
		dev_err(&pdev->dev, "core iomem ioremap failed\n");
		ret = -ENOMEM;
		goto devote_bus_bw;
	}
	dev_info(&pdev->dev, "OTG regs = %pK\n", motg->regs);

	if (pdata->enable_sec_phy) {
		res = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tcsr");
		if (!res) {
			dev_dbg(&pdev->dev, "missing TCSR memory resource\n");
		} else {
			tcsr = devm_ioremap_nocache(&pdev->dev, res->start,
				resource_size(res));
			if (!tcsr) {
				dev_dbg(&pdev->dev, "tcsr ioremap failed\n");
			} else {
				/* Enable USB2 on secondary HSPHY. */
				writel_relaxed(0x1, tcsr);
				/*
				 * Ensure that TCSR write is completed before
				 * USB registers initialization.
				 */
				mb();
			}
		}
	}

	if (pdata->enable_sec_phy)
		motg->usb_phy_ctrl_reg = USB_PHY_CTRL2;
	else
		motg->usb_phy_ctrl_reg = USB_PHY_CTRL;

	/*
	 * The USB PHY wrapper provides a register interface
	 * through AHB2PHY for performing PHY related operations
	 * like retention, HV interrupts and overriding parameter
	 * registers etc. The registers start at 4 byte boundary
	 * but only the first byte is valid and remaining are not
	 * used. Relaxed versions of readl/writel should be used.
	 *
	 * The link does not have any PHY specific registers.
	 * Hence set motg->usb_phy_ctrl_reg to.
	 */
	if (motg->pdata->phy_type == SNPS_FEMTO_PHY ||
		pdata->phy_type == QUSB_ULPI_PHY) {
		res = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "phy_csr");
		if (!res) {
			dev_err(&pdev->dev, "PHY CSR IOMEM missing!\n");
			ret = -ENODEV;
			goto free_regs;
		}
		motg->phy_csr_regs = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(motg->phy_csr_regs)) {
			ret = PTR_ERR(motg->phy_csr_regs);
			dev_err(&pdev->dev, "PHY CSR ioremap failed!\n");
			goto free_regs;
		}
		motg->usb_phy_ctrl_reg = 0;
	}

	motg->irq = platform_get_irq(pdev, 0);
	if (!motg->irq) {
		dev_err(&pdev->dev, "platform_get_irq failed\n");
		ret = -ENODEV;
		goto free_regs;
	}

	motg->async_irq = platform_get_irq_byname(pdev, "async_irq");
	if (motg->async_irq < 0) {
		dev_err(&pdev->dev, "platform_get_irq for async_int failed\n");
		motg->async_irq = 0;
		goto free_regs;
	}

	if (motg->xo_clk) {
		ret = clk_prepare_enable(motg->xo_clk);
		if (ret) {
			dev_err(&pdev->dev,
				"%s failed to vote for TCXO %d\n",
					__func__, ret);
			goto free_xo_handle;
		}
	}


	clk_prepare_enable(motg->pclk);

	hsusb_vdd = devm_regulator_get(motg->phy.dev, "hsusb_vdd_dig");
	if (IS_ERR(hsusb_vdd)) {
		hsusb_vdd = devm_regulator_get(motg->phy.dev, "HSUSB_VDDCX");
		if (IS_ERR(hsusb_vdd)) {
			dev_err(motg->phy.dev, "unable to get hsusb vddcx\n");
			ret = PTR_ERR(hsusb_vdd);
			goto devote_xo_handle;
		}
	}

	if (of_get_property(pdev->dev.of_node,
			"qcom,vdd-voltage-level",
			&len)){
		if (len == sizeof(tmp)) {
			of_property_read_u32_array(pdev->dev.of_node,
					"qcom,vdd-voltage-level",
					tmp, len/sizeof(*tmp));
			vdd_val[0] = tmp[0];
			vdd_val[1] = tmp[1];
			vdd_val[2] = tmp[2];
		} else {
			dev_dbg(&pdev->dev,
				"Using default hsusb vdd config.\n");
			goto devote_xo_handle;
		}
	} else {
		goto devote_xo_handle;
	}

	ret = msm_hsusb_config_vddcx(1);
	if (ret) {
		dev_err(&pdev->dev, "hsusb vddcx configuration failed\n");
		goto devote_xo_handle;
	}

	ret = regulator_enable(hsusb_vdd);
	if (ret) {
		dev_err(&pdev->dev, "unable to enable the hsusb vddcx\n");
		goto free_config_vddcx;
	}

	ret = msm_hsusb_ldo_init(motg, 1);
	if (ret) {
		dev_err(&pdev->dev, "hsusb vreg configuration failed\n");
		goto free_hsusb_vdd;
	}

	/* Get pinctrl if target uses pinctrl */
	motg->phy_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(motg->phy_pinctrl)) {
		if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
			dev_err(&pdev->dev, "Error encountered while getting pinctrl");
			ret = PTR_ERR(motg->phy_pinctrl);
			goto free_ldo_init;
		}
		dev_dbg(&pdev->dev, "Target does not use pinctrl\n");
		motg->phy_pinctrl = NULL;
	}

	ret = msm_hsusb_ldo_enable(motg, USB_PHY_REG_ON);
	if (ret) {
		dev_err(&pdev->dev, "hsusb vreg enable failed\n");
		goto free_ldo_init;
	}
	clk_prepare_enable(motg->core_clk);

	/* Check if USB mem_type change is needed to workaround PNOC hw issue */
	msm_otg_pnoc_errata_fix(motg);

	writel_relaxed(0, USB_USBINTR);
	writel_relaxed(0, USB_OTGSC);
	/* Ensure that above STOREs are completed before enabling interrupts */
	mb();

	motg->id_state = USB_ID_FLOAT;
	set_bit(ID, &motg->inputs);
	wake_lock_init(&motg->wlock, WAKE_LOCK_SUSPEND, "msm_otg");
	INIT_WORK(&motg->sm_work, msm_otg_sm_work);
	INIT_DELAYED_WORK(&motg->chg_work, msm_chg_detect_work);
	INIT_DELAYED_WORK(&motg->id_status_work, msm_id_status_w);
	INIT_DELAYED_WORK(&motg->perf_vote_work, msm_otg_perf_vote_work);
	setup_timer(&motg->chg_check_timer, msm_otg_chg_check_timer_func,
				(unsigned long) motg);
	motg->otg_wq = alloc_ordered_workqueue("k_otg", 0);
	if (!motg->otg_wq) {
		pr_err("%s: Unable to create workqueue otg_wq\n",
			__func__);
		goto destroy_wlock;
	}

	ret = request_irq(motg->irq, msm_otg_irq, IRQF_SHARED,
					"msm_otg", motg);
	if (ret) {
		dev_err(&pdev->dev, "request irq failed\n");
		goto destroy_wq;
	}

	motg->phy_irq = platform_get_irq_byname(pdev, "phy_irq");
	if (motg->phy_irq < 0) {
		dev_dbg(&pdev->dev, "phy_irq is not present\n");
		motg->phy_irq = 0;
	} else {

		/* clear all interrupts before enabling the IRQ */
		writeb_relaxed(0xFF, USB2_PHY_USB_PHY_INTERRUPT_CLEAR0);
		writeb_relaxed(0xFF, USB2_PHY_USB_PHY_INTERRUPT_CLEAR1);

		writeb_relaxed(0x1, USB2_PHY_USB_PHY_IRQ_CMD);
		/*
		 * Databook says 200 usec delay is required for
		 * clearing the interrupts.
		 */
		udelay(200);
		writeb_relaxed(0x0, USB2_PHY_USB_PHY_IRQ_CMD);

		ret = request_irq(motg->phy_irq, msm_otg_phy_irq_handler,
				IRQF_TRIGGER_RISING, "msm_otg_phy_irq", motg);
		if (ret < 0) {
			dev_err(&pdev->dev, "phy_irq request fail %d\n", ret);
			goto free_irq;
		}
	}

	ret = request_irq(motg->async_irq, msm_otg_irq,
				IRQF_TRIGGER_RISING, "msm_otg", motg);
	if (ret) {
		dev_err(&pdev->dev, "request irq failed (ASYNC INT)\n");
		goto free_phy_irq;
	}
	disable_irq(motg->async_irq);

	if (pdata->otg_control == OTG_PHY_CONTROL && pdata->mpm_otgsessvld_int)
		msm_mpm_enable_pin(pdata->mpm_otgsessvld_int, 1);

	if (pdata->mpm_dpshv_int)
		msm_mpm_enable_pin(pdata->mpm_dpshv_int, 1);
	if (pdata->mpm_dmshv_int)
		msm_mpm_enable_pin(pdata->mpm_dmshv_int, 1);

	phy->init = msm_otg_reset;
	phy->set_power = msm_otg_set_power;
	phy->set_suspend = msm_otg_set_suspend;
	phy->dbg_event = msm_otg_dbg_log_event;

	phy->io_ops = &msm_otg_io_ops;

	phy->otg->phy = &motg->phy;
	phy->otg->set_host = msm_otg_set_host;
	phy->otg->set_peripheral = msm_otg_set_peripheral;
	if (pdata->dp_manual_pullup)
		phy->flags |= ENABLE_DP_MANUAL_PULLUP;

	if (pdata->enable_sec_phy)
		phy->flags |= ENABLE_SECONDARY_PHY;

	ret = usb_add_phy(&motg->phy, USB_PHY_TYPE_USB2);
	if (ret) {
		dev_err(&pdev->dev, "usb_add_phy failed\n");
		goto free_async_irq;
	}

	if (motg->pdata->mode == USB_OTG &&
		motg->pdata->otg_control == OTG_PMIC_CONTROL &&
		!motg->phy_irq) {

		if (gpio_is_valid(motg->pdata->usb_id_gpio)) {
			/* usb_id_gpio request */
			ret = gpio_request(motg->pdata->usb_id_gpio,
							"USB_ID_GPIO");
			if (ret < 0) {
				dev_err(&pdev->dev, "gpio req failed for id\n");
				motg->pdata->usb_id_gpio = 0;
				goto remove_phy;
			}

			/* The following code implements switch between the HOST
			 * mode to device mode when used diferent HW components
			 * on the same port: USB HUB and the usb jack type B
			 * for device mode In this case HUB should be gone
			 * only once out of reset at the boot time and after
			 * that always stay on*/
			if (gpio_is_valid(motg->pdata->hub_reset_gpio)) {
				ret = devm_gpio_request(&pdev->dev,
						motg->pdata->hub_reset_gpio,
						"qcom,hub-reset-gpio");
				if (ret < 0) {
					dev_err(&pdev->dev, "gpio req failed for hub reset\n");
					goto remove_phy;
				}
				gpio_direction_output(
					motg->pdata->hub_reset_gpio, 1);
			}

			if (gpio_is_valid(motg->pdata->switch_sel_gpio)) {
				ret = devm_gpio_request(&pdev->dev,
						motg->pdata->switch_sel_gpio,
						"qcom,sw-sel-gpio");
				if (ret < 0) {
					dev_err(&pdev->dev, "gpio req failed for switch sel\n");
					goto remove_phy;
				}
				if (gpio_get_value(motg->pdata->usb_id_gpio))
					gpio_direction_input(
						motg->pdata->switch_sel_gpio);

				else
					gpio_direction_output(
					    motg->pdata->switch_sel_gpio,
					    1);
			}

			/* usb_id_gpio to irq */
			id_irq = gpio_to_irq(motg->pdata->usb_id_gpio);
			motg->ext_id_irq = id_irq;
		} else if (motg->pdata->pmic_id_irq) {
			id_irq = motg->pdata->pmic_id_irq;
		}

		if (id_irq) {
			ret = request_irq(id_irq,
					  msm_id_irq,
					  IRQF_TRIGGER_RISING |
					  IRQF_TRIGGER_FALLING,
					  "msm_otg", motg);
			if (ret) {
				dev_err(&pdev->dev, "request irq failed for ID\n");
				goto remove_phy;
			}
		} else {
			/* PMIC does USB ID detection and notifies through
			 * USB_OTG property of USB powersupply.
			 */
			dev_dbg(&pdev->dev, "PMIC does ID detection\n");
		}
	}

	platform_set_drvdata(pdev, motg);
	device_init_wakeup(&pdev->dev, 1);

	ret = msm_otg_debugfs_init(motg);
	if (ret)
		dev_dbg(&pdev->dev, "mode debugfs file is"
			"not available\n");

	if (motg->pdata->otg_control == OTG_PMIC_CONTROL &&
			(!(motg->pdata->mode == USB_OTG) ||
			 motg->pdata->pmic_id_irq || motg->ext_id_irq ||
								!motg->phy_irq))
		motg->caps = ALLOW_PHY_POWER_COLLAPSE | ALLOW_PHY_RETENTION;

	if (motg->pdata->otg_control == OTG_PHY_CONTROL || motg->phy_irq ||
				motg->pdata->enable_phy_id_pullup)
		motg->caps = ALLOW_PHY_RETENTION | ALLOW_PHY_REGULATORS_LPM;

	if (motg->pdata->mpm_dpshv_int || motg->pdata->mpm_dmshv_int)
		motg->caps |= ALLOW_HOST_PHY_RETENTION;

	device_create_file(&pdev->dev, &dev_attr_dpdm_pulldown_enable);

	if (motg->pdata->enable_lpm_on_dev_suspend)
		motg->caps |= ALLOW_LPM_ON_DEV_SUSPEND;

	if (motg->pdata->disable_retention_with_vdd_min)
		motg->caps |= ALLOW_VDD_MIN_WITH_RETENTION_DISABLED;

	/*
	 * PHY DVDD is supplied by a always on PMIC LDO (unlike
	 * vddcx/vddmx). PHY can keep D+ pull-up and D+/D-
	 * pull-down during suspend without any additional
	 * hardware re-work.
	 */
	if (motg->pdata->phy_type == SNPS_FEMTO_PHY)
		motg->caps |= ALLOW_BUS_SUSPEND_WITHOUT_REWORK;

	wake_lock(&motg->wlock);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	if (motg->pdata->delay_lpm_on_disconnect) {
		pm_runtime_set_autosuspend_delay(&pdev->dev,
			lpm_disconnect_thresh);
		pm_runtime_use_autosuspend(&pdev->dev);
	}

	motg->usb_psy.name = "usb";
	motg->usb_psy.type = POWER_SUPPLY_TYPE_USB;
	motg->usb_psy.supplied_to = otg_pm_power_supplied_to;
	motg->usb_psy.num_supplicants = ARRAY_SIZE(otg_pm_power_supplied_to);
	motg->usb_psy.properties = otg_pm_power_props_usb;
	motg->usb_psy.num_properties = ARRAY_SIZE(otg_pm_power_props_usb);
	motg->usb_psy.get_property = otg_power_get_property_usb;
	motg->usb_psy.set_property = otg_power_set_property_usb;
	motg->usb_psy.property_is_writeable
		= otg_power_property_is_writeable_usb;

	if (!msm_otg_register_power_supply(pdev, motg))
		psy = &motg->usb_psy;

	ret = msm_otg_setup_ext_chg_cdev(motg);
	if (ret)
		dev_dbg(&pdev->dev, "fail to setup cdev\n");

	if (pdev->dev.of_node) {
		ret = msm_otg_setup_devices(pdev, pdata->mode, true);
		if (ret) {
			dev_err(&pdev->dev, "devices setup failed\n");
			goto remove_cdev;
		}
	}

	if (gpio_is_valid(motg->pdata->hub_reset_gpio)) {
		ret = devm_gpio_request(&pdev->dev,
				motg->pdata->hub_reset_gpio,
				"HUB_RESET");
		if (ret < 0) {
			dev_err(&pdev->dev, "gpio req failed for hub_reset\n");
		} else {
			gpio_direction_output(
				motg->pdata->hub_reset_gpio, 0);
			/* 5 microsecs reset signaling to usb hub */
			usleep_range(5, 10);
			gpio_direction_output(
				motg->pdata->hub_reset_gpio, 1);
		}
	}

	if (gpio_is_valid(motg->pdata->usbeth_reset_gpio)) {
		ret = devm_gpio_request(&pdev->dev,
				motg->pdata->usbeth_reset_gpio,
				"ETH_RESET");
		if (ret < 0) {
			dev_err(&pdev->dev, "gpio req failed for usbeth_reset\n");
		} else {
			gpio_direction_output(
				motg->pdata->usbeth_reset_gpio, 0);
			/* 100 microsecs reset signaling to usb-to-eth */
			usleep_range(100, 110);
			gpio_direction_output(
				motg->pdata->usbeth_reset_gpio, 1);
		}
	}

	motg->pm_notify.notifier_call = msm_otg_pm_notify;
	register_pm_notifier(&motg->pm_notify);
	msm_otg_dbg_log_event(phy, "OTG PROBE", motg->caps, motg->lpm_flags);

	return 0;

remove_cdev:
	if (!motg->ext_chg_device) {
		device_destroy(motg->ext_chg_class, motg->ext_chg_dev);
		cdev_del(&motg->ext_chg_cdev);
		class_destroy(motg->ext_chg_class);
		unregister_chrdev_region(motg->ext_chg_dev, 1);
	}
	if (psy)
		power_supply_unregister(psy);
remove_phy:
	usb_remove_phy(&motg->phy);
free_async_irq:
	free_irq(motg->async_irq, motg);
free_phy_irq:
	if (motg->phy_irq)
		free_irq(motg->phy_irq, motg);
free_irq:
	free_irq(motg->irq, motg);
destroy_wq:
	destroy_workqueue(motg->otg_wq);
destroy_wlock:
	wake_lock_destroy(&motg->wlock);
	clk_disable_unprepare(motg->core_clk);
	msm_hsusb_ldo_enable(motg, USB_PHY_REG_OFF);
free_ldo_init:
	msm_hsusb_ldo_init(motg, 0);
free_hsusb_vdd:
	regulator_disable(hsusb_vdd);
free_config_vddcx:
	regulator_set_voltage(hsusb_vdd,
		vdd_val[VDD_NONE],
		vdd_val[VDD_MAX]);
devote_xo_handle:
	clk_disable_unprepare(motg->pclk);
	if (motg->xo_clk)
		clk_disable_unprepare(motg->xo_clk);
free_xo_handle:
	if (motg->xo_clk) {
		clk_put(motg->xo_clk);
		motg->xo_clk = NULL;
	}
free_regs:
	iounmap(motg->regs);
devote_bus_bw:
	if (motg->bus_perf_client) {
		msm_otg_bus_vote(motg, USB_NO_PERF_VOTE);
		msm_bus_scale_unregister_client(motg->bus_perf_client);
	}
otg_remove_devices:
	if (pdev->dev.of_node)
		msm_otg_setup_devices(pdev, motg->pdata->mode, false);
disable_phy_csr_clk:
	if (motg->phy_csr_clk)
		clk_disable_unprepare(motg->phy_csr_clk);
disable_sleep_clk:
	if (motg->sleep_clk)
		clk_disable_unprepare(motg->sleep_clk);
put_xo_clk:
	if (motg->xo_clk)
		clk_put(motg->xo_clk);
put_pclk:
	if (motg->pclk)
		clk_put(motg->pclk);
put_core_clk:
	if (motg->core_clk)
		clk_put(motg->core_clk);
free_motg:
	kfree(motg);
	return ret;
}

static int msm_otg_remove(struct platform_device *pdev)
{
	struct msm_otg *motg = platform_get_drvdata(pdev);
	struct usb_phy *phy = &motg->phy;
	int cnt = 0;

	if (phy->otg->host || phy->otg->gadget)
		return -EBUSY;

	unregister_pm_notifier(&motg->pm_notify);

	if (!motg->ext_chg_device) {
		device_destroy(motg->ext_chg_class, motg->ext_chg_dev);
		cdev_del(&motg->ext_chg_cdev);
		class_destroy(motg->ext_chg_class);
		unregister_chrdev_region(motg->ext_chg_dev, 1);
	}

	if (pdev->dev.of_node)
		msm_otg_setup_devices(pdev, motg->pdata->mode, false);
	if (psy)
		power_supply_unregister(psy);
	msm_otg_debugfs_cleanup();
	cancel_delayed_work_sync(&motg->chg_work);
	cancel_delayed_work_sync(&motg->id_status_work);
	cancel_delayed_work_sync(&motg->perf_vote_work);
	msm_otg_perf_vote_update(motg, false);
	cancel_work_sync(&motg->sm_work);
	destroy_workqueue(motg->otg_wq);

	pm_runtime_resume(&pdev->dev);

	device_init_wakeup(&pdev->dev, 0);
	pm_runtime_disable(&pdev->dev);
	wake_lock_destroy(&motg->wlock);

	if (motg->phy_irq)
		free_irq(motg->phy_irq, motg);
	if (motg->pdata->pmic_id_irq)
		free_irq(motg->pdata->pmic_id_irq, motg);
	usb_remove_phy(phy);
	free_irq(motg->irq, motg);

	if (motg->pdata->mpm_dpshv_int || motg->pdata->mpm_dmshv_int)
		device_remove_file(&pdev->dev,
				&dev_attr_dpdm_pulldown_enable);
	if (motg->pdata->otg_control == OTG_PHY_CONTROL &&
		motg->pdata->mpm_otgsessvld_int)
		msm_mpm_enable_pin(motg->pdata->mpm_otgsessvld_int, 0);

	if (motg->pdata->mpm_dpshv_int)
		msm_mpm_enable_pin(motg->pdata->mpm_dpshv_int, 0);
	if (motg->pdata->mpm_dmshv_int)
		msm_mpm_enable_pin(motg->pdata->mpm_dmshv_int, 0);

	/*
	 * Put PHY in low power mode.
	 */
	ulpi_read(phy, 0x14);
	ulpi_write(phy, 0x08, 0x09);

	writel_relaxed(readl_relaxed(USB_PORTSC) | PORTSC_PHCD, USB_PORTSC);
	while (cnt < PHY_SUSPEND_TIMEOUT_USEC) {
		if (readl_relaxed(USB_PORTSC) & PORTSC_PHCD)
			break;
		udelay(1);
		cnt++;
	}
	if (cnt >= PHY_SUSPEND_TIMEOUT_USEC)
		dev_err(phy->dev, "Unable to suspend PHY\n");

	clk_disable_unprepare(motg->pclk);
	clk_disable_unprepare(motg->core_clk);
	if (motg->phy_csr_clk)
		clk_disable_unprepare(motg->phy_csr_clk);
	if (motg->xo_clk) {
		clk_disable_unprepare(motg->xo_clk);
		clk_put(motg->xo_clk);
	}

	if (!IS_ERR(motg->sleep_clk))
		clk_disable_unprepare(motg->sleep_clk);

	msm_hsusb_ldo_enable(motg, USB_PHY_REG_OFF);
	msm_hsusb_ldo_init(motg, 0);
	regulator_disable(hsusb_vdd);
	regulator_set_voltage(hsusb_vdd,
		vdd_val[VDD_NONE],
		vdd_val[VDD_MAX]);

	iounmap(motg->regs);
	pm_runtime_set_suspended(&pdev->dev);

	clk_put(motg->pclk);
	clk_put(motg->core_clk);

	if (motg->bus_perf_client) {
		msm_otg_bus_vote(motg, USB_NO_PERF_VOTE);
		msm_bus_scale_unregister_client(motg->bus_perf_client);
	}

	return 0;
}

static void msm_otg_shutdown(struct platform_device *pdev)
{
	struct msm_otg *motg = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "OTG shutdown\n");
	msm_hsusb_vbus_power(motg, 0);
}

#ifdef CONFIG_PM_RUNTIME
static int msm_otg_runtime_idle(struct device *dev)
{
	struct msm_otg *motg = dev_get_drvdata(dev);
	struct usb_phy *phy = &motg->phy;

	dev_dbg(dev, "OTG runtime idle\n");
	msm_otg_dbg_log_event(phy, "RUNTIME IDLE",
			phy->state, motg->ext_chg_active);

	if (phy->state == OTG_STATE_UNDEFINED)
		return -EAGAIN;

	if (motg->ext_chg_active == DEFAULT) {
		dev_dbg(dev, "Deferring LPM\n");
		/*
		 * Charger detection may happen in user space.
		 * Delay entering LPM by 3 sec.  Otherwise we
		 * have to exit LPM when user space begins
		 * charger detection.
		 *
		 * This timer will be canceled when user space
		 * votes against LPM by incrementing PM usage
		 * counter.  We enter low power mode when
		 * PM usage counter is decremented.
		 */
		pm_schedule_suspend(dev, 3000);
		return -EAGAIN;
	}

	return 0;
}

static int msm_otg_runtime_suspend(struct device *dev)
{
	struct msm_otg *motg = dev_get_drvdata(dev);

	dev_dbg(dev, "OTG runtime suspend\n");
	msm_otg_dbg_log_event(&motg->phy, "RUNTIME SUSPEND",
			get_pm_runtime_counter(dev), 0);
	return msm_otg_suspend(motg);
}

static int msm_otg_runtime_resume(struct device *dev)
{
	struct msm_otg *motg = dev_get_drvdata(dev);

	dev_dbg(dev, "OTG runtime resume\n");
	msm_otg_dbg_log_event(&motg->phy, "RUNTIME RESUME",
			get_pm_runtime_counter(dev), 0);

	return msm_otg_resume(motg);
}
#endif

#ifdef CONFIG_PM_SLEEP
static int msm_otg_pm_suspend(struct device *dev)
{
	struct msm_otg *motg = dev_get_drvdata(dev);

	dev_dbg(dev, "OTG PM suspend\n");
	msm_otg_dbg_log_event(&motg->phy, "PM SUSPEND START",
			get_pm_runtime_counter(dev),
			atomic_read(&motg->pm_suspended));

	/* flush any pending sm_work first */
	flush_work(&motg->sm_work);
	if (!atomic_read(&motg->in_lpm)) {
		dev_err(dev, "Abort PM suspend!! (USB is outside LPM)\n");
		return -EBUSY;
	}
	atomic_set(&motg->pm_suspended, 1);

	return 0;
}

static int msm_otg_pm_resume(struct device *dev)
{
	int ret = 0;
	struct msm_otg *motg = dev_get_drvdata(dev);

	dev_dbg(dev, "OTG PM resume\n");
	msm_otg_dbg_log_event(&motg->phy, "PM RESUME START",
			get_pm_runtime_counter(dev), pm_runtime_suspended(dev));

	if (motg->resume_pending || motg->phy_irq_pending) {
		msm_otg_dbg_log_event(&motg->phy, "PM RESUME BY USB",
				motg->async_int, motg->resume_pending);
		/* sm work if pending will start in pm notify to exit LPM */
	}

	return ret;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops msm_otg_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msm_otg_pm_suspend, msm_otg_pm_resume)
	SET_RUNTIME_PM_OPS(msm_otg_runtime_suspend, msm_otg_runtime_resume,
				msm_otg_runtime_idle)
};
#endif

static struct of_device_id msm_otg_dt_match[] = {
	{	.compatible = "qcom,hsusb-otg",
	},
	{}
};

static struct platform_driver msm_otg_driver = {
	.probe = msm_otg_probe,
	.remove = msm_otg_remove,
	.shutdown = msm_otg_shutdown,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &msm_otg_dev_pm_ops,
#endif
		.of_match_table = msm_otg_dt_match,
	},
};

module_platform_driver(msm_otg_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM USB transceiver driver");

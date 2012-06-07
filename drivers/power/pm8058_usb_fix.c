/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/mfd/pmic8058.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/slab.h>

#include <mach/msm_xo.h>
#include <mach/msm_hsusb.h>

/* Config Regs  and their bits*/
#define PM8058_CHG_TEST			0x75
#define IGNORE_LL                       2

#define PM8058_CHG_TEST_2		0xEA
#define PM8058_CHG_TEST_3		0xEB
#define PM8058_OVP_TEST_REG		0xF6
#define FORCE_OVP_OFF			3

#define PM8058_CHG_CNTRL		0x1E
#define CHG_TRICKLE_EN			7
#define CHG_USB_SUSPEND			6
#define CHG_IMON_CAL			5
#define CHG_IMON_GAIN			4
#define CHG_VBUS_FROM_BOOST_OVRD	2
#define CHG_CHARGE_DIS			1
#define CHG_VCP_EN			0

#define PM8058_CHG_CNTRL_2		0xD8
#define ATC_DIS				7	/* coincell backed */
#define CHARGE_AUTO_DIS			6
#define DUMB_CHG_OVRD			5	/* coincell backed */
#define ENUM_DONE			4
#define CHG_TEMP_MODE			3
#define CHG_BATT_TEMP_DIS		1	/* coincell backed */
#define CHG_FAILED_CLEAR		0

#define PM8058_CHG_VMAX_SEL		0x21
#define PM8058_CHG_VBAT_DET		0xD9
#define PM8058_CHG_IMAX			0x1F
#define PM8058_CHG_TRICKLE		0xDB
#define PM8058_CHG_ITERM		0xDC
#define PM8058_CHG_TTRKL_MAX		0xE1
#define PM8058_CHG_TCHG_MAX		0xE4
#define PM8058_CHG_TEMP_THRESH		0xE2
#define PM8058_CHG_TEMP_REG		0xE3
#define PM8058_CHG_PULSE		0x22

/* IRQ STATUS and CLEAR */
#define PM8058_CHG_STATUS_CLEAR_IRQ_1	0x31
#define PM8058_CHG_STATUS_CLEAR_IRQ_3	0x33
#define PM8058_CHG_STATUS_CLEAR_IRQ_10	0xB3
#define PM8058_CHG_STATUS_CLEAR_IRQ_11	0xB4

/* IRQ MASKS */
#define PM8058_CHG_MASK_IRQ_1		0x38

#define PM8058_CHG_MASK_IRQ_3		0x3A
#define PM8058_CHG_MASK_IRQ_10		0xBA
#define PM8058_CHG_MASK_IRQ_11		0xBB

/* IRQ Real time status regs */
#define PM8058_CHG_STATUS_RT_1		0x3F
#define STATUS_RTCHGVAL			7
#define STATUS_RTCHGINVAL		6
#define STATUS_RTBATT_REPLACE		5
#define STATUS_RTVBATDET_LOW		4
#define STATUS_RTCHGILIM		3
#define STATUS_RTPCTDONE		1
#define STATUS_RTVCP			0
#define PM8058_CHG_STATUS_RT_3		0x41
#define PM8058_CHG_STATUS_RT_10		0xC1
#define PM8058_CHG_STATUS_RT_11		0xC2

/* VTRIM */
#define PM8058_CHG_VTRIM		0x1D
#define PM8058_CHG_VBATDET_TRIM		0x1E
#define PM8058_CHG_ITRIM		0x1F
#define PM8058_CHG_TTRIM		0x20

#define AUTO_CHARGING_VMAXSEL				4200
#define AUTO_CHARGING_FAST_TIME_MAX_MINUTES		512
#define AUTO_CHARGING_TRICKLE_TIME_MINUTES		30
#define AUTO_CHARGING_VEOC_ITERM			100
#define AUTO_CHARGING_IEOC_ITERM			160

#define AUTO_CHARGING_VBATDET				4150
#define AUTO_CHARGING_VEOC_VBATDET			4100
#define AUTO_CHARGING_VEOC_TCHG				16
#define AUTO_CHARGING_VEOC_TCHG_FINAL_CYCLE		32
#define AUTO_CHARGING_VEOC_BEGIN_TIME_MS		5400000

#define AUTO_CHARGING_VEOC_VBAT_LOW_CHECK_TIME_MS	60000
#define AUTO_CHARGING_RESUME_CHARGE_DETECTION_COUNTER	5

#define PM8058_CHG_I_STEP_MA 50
#define PM8058_CHG_I_MIN_MA 50
#define PM8058_CHG_T_TCHG_SHIFT 2
#define PM8058_CHG_I_TERM_STEP_MA 10
#define PM8058_CHG_V_STEP_MV 25
#define PM8058_CHG_V_MIN_MV  2400
/*
 * enum pmic_chg_interrupts: pmic interrupts
 * @CHGVAL_IRQ: charger V between 3.3 and 7.9
 * @CHGINVAL_IRQ: charger V outside 3.3 and 7.9
 * @VBATDET_LOW_IRQ: VBAT < VBATDET
 * @VCP_IRQ: VDD went below VBAT: BAT_FET is turned on
 * @CHGILIM_IRQ: mA consumed>IMAXSEL: chgloop draws less mA
 * @ATC_DONE_IRQ: Auto Trickle done
 * @ATCFAIL_IRQ: Auto Trickle fail
 * @AUTO_CHGDONE_IRQ: Auto chg done
 * @AUTO_CHGFAIL_IRQ: time exceeded w/o reaching term current
 * @CHGSTATE_IRQ: something happend causing a state change
 * @FASTCHG_IRQ: trkl charging completed: moving to fastchg
 * @CHG_END_IRQ: mA has dropped to termination current
 * @BATTTEMP_IRQ: batt temp is out of range
 * @CHGHOT_IRQ: the pass device is too hot
 * @CHGTLIMIT_IRQ: unused
 * @CHG_GONE_IRQ: charger was removed
 * @VCPMAJOR_IRQ: vcp major
 * @VBATDET_IRQ: VBAT >= VBATDET
 * @BATFET_IRQ: BATFET closed
 * @BATT_REPLACE_IRQ:
 * @BATTCONNECT_IRQ:
 */
enum pmic_chg_interrupts {
	CHGVAL_IRQ,
	CHGINVAL_IRQ,
	VBATDET_LOW_IRQ,
	VCP_IRQ,
	CHGILIM_IRQ,
	ATC_DONE_IRQ,
	ATCFAIL_IRQ,
	AUTO_CHGDONE_IRQ,
	AUTO_CHGFAIL_IRQ,
	CHGSTATE_IRQ,
	FASTCHG_IRQ,
	CHG_END_IRQ,
	BATTTEMP_IRQ,
	CHGHOT_IRQ,
	CHGTLIMIT_IRQ,
	CHG_GONE_IRQ,
	VCPMAJOR_IRQ,
	VBATDET_IRQ,
	BATFET_IRQ,
	BATT_REPLACE_IRQ,
	BATTCONNECT_IRQ,
	PMIC_CHG_MAX_INTS
};

struct pm8058_charger {
	struct pmic_charger_pdata *pdata;
	struct pm8058_chip *pm_chip;
	struct device *dev;

	int pmic_chg_irq[PMIC_CHG_MAX_INTS];
	DECLARE_BITMAP(enabled_irqs, PMIC_CHG_MAX_INTS);

	struct delayed_work check_vbat_low_work;
	struct delayed_work veoc_begin_work;
	int waiting_for_topoff;
	int waiting_for_veoc;
	int current_charger_current;

	struct msm_xo_voter *voter;
	struct dentry *dent;
};

static struct pm8058_charger pm8058_chg;

static int pm_chg_get_rt_status(int irq)
{
	int count = 3;
	int ret;

	while ((ret =
		pm8058_irq_get_rt_status(pm8058_chg.pm_chip, irq)) == -EAGAIN
	       && count--) {
		dev_info(pm8058_chg.dev, "%s trycount=%d\n", __func__, count);
		cpu_relax();
	}
	if (ret == -EAGAIN)
		return 0;
	else
		return ret;
}

static int is_chg_plugged_in(void)
{
	return pm_chg_get_rt_status(pm8058_chg.pmic_chg_irq[CHGVAL_IRQ]);
}

static irqreturn_t pm8058_chg_chgval_handler(int irq, void *dev_id)
{
	u8 old, temp;
	int ret;

	if (!is_chg_plugged_in()) {	/*this debounces it */
		ret = pm8058_read(pm8058_chg.pm_chip, PM8058_OVP_TEST_REG,
					&old, 1);
		temp = old | BIT(FORCE_OVP_OFF);
		ret = pm8058_write(pm8058_chg.pm_chip, PM8058_OVP_TEST_REG,
					&temp, 1);
		temp = 0xFC;
		ret = pm8058_write(pm8058_chg.pm_chip, PM8058_CHG_TEST,
					&temp, 1);
		pr_debug("%s forced wrote 0xFC to test ret=%d\n",
							__func__, ret);
		/* 20 ms sleep is for the VCHG to discharge */
		msleep(20);
		temp = 0xF0;
		ret = pm8058_write(pm8058_chg.pm_chip, PM8058_CHG_TEST,
					&temp, 1);
		ret = pm8058_write(pm8058_chg.pm_chip, PM8058_OVP_TEST_REG,
					&old, 1);
	}

	return IRQ_HANDLED;
}

static void free_irqs(void)
{
	int i;

	for (i = 0; i < PMIC_CHG_MAX_INTS; i++)
		if (pm8058_chg.pmic_chg_irq[i]) {
			free_irq(pm8058_chg.pmic_chg_irq[i], NULL);
			pm8058_chg.pmic_chg_irq[i] = 0;
		}
}

static int __devinit request_irqs(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	ret = 0;
	bitmap_fill(pm8058_chg.enabled_irqs, PMIC_CHG_MAX_INTS);

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "CHGVAL");
	if (res == NULL) {
		dev_err(pm8058_chg.dev,
			"%s:couldnt find resource CHGVAL\n", __func__);
		goto err_out;
	} else {
		ret = request_any_context_irq(res->start,
				  pm8058_chg_chgval_handler,
				  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				  res->name, NULL);
		if (ret < 0) {
			dev_err(pm8058_chg.dev, "%s:couldnt request %d %d\n",
				__func__, res->start, ret);
			goto err_out;
		} else {
			pm8058_chg.pmic_chg_irq[CHGVAL_IRQ] = res->start;
		}
	}

	return 0;

err_out:
	free_irqs();
	return -EINVAL;
}

static int pm8058_usb_voltage_lower_limit(void)
{
	u8 temp, old;
	int ret = 0;

	temp = 0x10;
	ret |= pm8058_write(pm8058_chg.pm_chip, PM8058_CHG_TEST, &temp, 1);
	ret |= pm8058_read(pm8058_chg.pm_chip, PM8058_CHG_TEST, &old, 1);
	old = old & ~BIT(IGNORE_LL);
	temp = 0x90  | (0xF & old);
	pr_debug("%s writing 0x%x to test\n", __func__, temp);
	ret |= pm8058_write(pm8058_chg.pm_chip, PM8058_CHG_TEST, &temp, 1);

	return ret;
}

static int __devinit pm8058_charger_probe(struct platform_device *pdev)
{
	struct pm8058_chip *pm_chip;

	pm_chip = dev_get_drvdata(pdev->dev.parent);
	if (pm_chip == NULL) {
		pr_err("%s:no parent data passed in.\n", __func__);
		return -EFAULT;
	}

	pm8058_chg.pm_chip = pm_chip;
	pm8058_chg.pdata = pdev->dev.platform_data;
	pm8058_chg.dev = &pdev->dev;

	if (request_irqs(pdev)) {
		pr_err("%s: couldnt register interrupts\n", __func__);
		return -EINVAL;
	}

	if (pm8058_usb_voltage_lower_limit()) {
		pr_err("%s: couldnt write to IGNORE_LL\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int __devexit pm8058_charger_remove(struct platform_device *pdev)
{
	free_irqs();
	return 0;
}

static struct platform_driver pm8058_charger_driver = {
	.probe = pm8058_charger_probe,
	.remove = __devexit_p(pm8058_charger_remove),
	.driver = {
		   .name = "pm-usb-fix",
		   .owner = THIS_MODULE,
	},
};

static int __init pm8058_charger_init(void)
{
	return platform_driver_register(&pm8058_charger_driver);
}

static void __exit pm8058_charger_exit(void)
{
	platform_driver_unregister(&pm8058_charger_driver);
}

late_initcall(pm8058_charger_init);
module_exit(pm8058_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 BATTERY driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8058_charger");

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

/*
 * Bluetooth Power Switch Module
 * controls power to external Bluetooth device
 * with interface to power management device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/btpower.h>
#include <linux/of_device.h>
#include <linux/thermal.h>
#include <soc/qcom/cmd-db.h>
#ifdef CONFIG_ARCH_NEO
#if IS_ENABLED(CONFIG_ICNSS2)
#include <soc/qcom/icnss2.h>
#endif
#endif

#if defined CONFIG_BT_SLIM_QCA6390 || \
	defined CONFIG_BT_SLIM_QCA6490 || \
	defined CONFIG_BTFM_SLIM_WCN3990  || \
	defined CONFIG_BTFM_SLIM_WCN7850
#include "btfm_slim.h"
#endif
#include <linux/fs.h>

#define PWR_SRC_NOT_AVAILABLE -2
#define DEFAULT_INVALID_VALUE -1
#define PWR_SRC_INIT_STATE_IDX 0
#define BTPOWER_MBOX_MSG_MAX_LEN 64
#define BTPOWER_MBOX_TIMEOUT_MS 1000
#define XO_CLK_RETRY_COUNT_MAX 5
#define BT_EN_MAX_DELAY 500
#define BT_EN_DEFAULT_DELAY 100
#define TEMP_THRESHOLD 5000

/**
 * enum btpower_vreg_param: Voltage regulator TCS param
 * @BTPOWER_VREG_VOLTAGE: Provides voltage level to be configured in TCS
 * @BTPOWER_VREG_MODE: Regulator mode
 * @BTPOWER_VREG_TCS_ENABLE: Set Voltage regulator enable config in TCS
 */
enum btpower_vreg_param {
	BTPOWER_VREG_VOLTAGE,
	BTPOWER_VREG_MODE,
	BTPOWER_VREG_ENABLE,
};

/**
 * enum btpower_tcs_seq: TCS sequence ID for trigger
 * BTPOWER_TCS_UP_SEQ: TCS Sequence based on up trigger / Wake TCS
 * BTPOWER_TCS_DOWN_SEQ: TCS Sequence based on down trigger / Sleep TCS
 * BTPOWER_TCS_ALL_SEQ: Update for both up and down triggers
 */
enum btpower_tcs_seq {
	BTPOWER_TCS_UP_SEQ,
	BTPOWER_TCS_DOWN_SEQ,
	BTPOWER_TCS_ALL_SEQ,
};

enum power_src_pos {
	BT_RESET_GPIO = PWR_SRC_INIT_STATE_IDX,
	BT_SW_CTRL_GPIO,
	BT_VDD_AON_LDO,
	BT_VDD_DIG_LDO,
	BT_VDD_RFA1_LDO,
	BT_VDD_RFA2_LDO,
	BT_VDD_ASD_LDO,
	BT_VDD_XTAL_LDO,
	BT_VDD_PA_LDO,
	BT_VDD_CORE_LDO,
	BT_VDD_IO_LDO,
	BT_VDD_LDO,
	BT_VDD_RFA_0p8,
	BT_VDD_RFACMN,
	// these indexes GPIOs/regs value are fetched during crash.
	BT_RESET_GPIO_CURRENT,
	BT_SW_CTRL_GPIO_CURRENT,
	BT_VDD_AON_LDO_CURRENT,
	BT_VDD_DIG_LDO_CURRENT,
	BT_VDD_RFA1_LDO_CURRENT,
	BT_VDD_RFA2_LDO_CURRENT,
	BT_VDD_ASD_LDO_CURRENT,
	BT_VDD_XTAL_LDO_CURRENT,
	BT_VDD_PA_LDO_CURRENT,
	BT_VDD_CORE_LDO_CURRENT,
	BT_VDD_IO_LDO_CURRENT,
	BT_VDD_LDO_CURRENT,
	BT_VDD_RFA_0p8_CURRENT,
	BT_VDD_RFACMN_CURRENT,
	BT_VDD_IPA_2p2,
	BT_VDD_IPA_2p2_CURRENT,
	/* The below bucks are voted for HW WAR on some platform which supports
	 * WNC39xx.
	 */
	BT_VDD_SMPS,
	BT_VDD_SMPS_CURRENT,
	/* New entries need to be added before PWR_SRC_SIZE.
	 * Its hold the max size of power sources states.
	 */
	BT_POWER_SRC_SIZE,
};

// Regulator structure for QCA6174/QCA9377/QCA9379 BT SoC series
static struct bt_power_vreg_data bt_vregs_info_qca61x4_937x[] = {
	{NULL, "qcom,bt-vdd-aon", 928000, 928000, 0, false, false,
		{BT_VDD_AON_LDO, BT_VDD_AON_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-io", 1710000, 3460000, 0, false, false,
		{BT_VDD_IO_LDO, BT_VDD_IO_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-core", 3135000, 3465000, 0, false, false,
		{BT_VDD_CORE_LDO, BT_VDD_CORE_LDO_CURRENT}},
};

// Regulator structure for QCA6390,QCA6490 and WCN6750 BT SoC series
static struct bt_power_vreg_data bt_vregs_info_qca6xx0[] = {
	{NULL, "qcom,bt-vdd-io",      1800000, 1800000, 0, false, true,
		{BT_VDD_IO_LDO, BT_VDD_IO_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-aon",     966000,  966000,  0, false, true,
		{BT_VDD_AON_LDO, BT_VDD_AON_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-rfacmn",  950000,  950000,  0, false, true,
		{BT_VDD_RFACMN, BT_VDD_RFACMN_CURRENT}},
	/* BT_CX_MX */
	{NULL, "qcom,bt-vdd-dig",      966000,  966000,  0, false, true,
		{BT_VDD_DIG_LDO, BT_VDD_DIG_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-rfa-0p8",  950000,  952000,  0, false, true,
		{BT_VDD_RFA_0p8, BT_VDD_RFA_0p8_CURRENT}},
	{NULL, "qcom,bt-vdd-rfa1",     1900000, 1900000, 0, false, true,
		{BT_VDD_RFA1_LDO, BT_VDD_RFA1_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-rfa2",     1900000, 1900000, 0, false, true,
		{BT_VDD_RFA2_LDO, BT_VDD_RFA2_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-asd",      2800000, 2800000, 0, false, true,
		{BT_VDD_ASD_LDO, BT_VDD_ASD_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-ipa-2p2",  2200000, 2210000, 0, false, true,
		{BT_VDD_IPA_2p2, BT_VDD_IPA_2p2_CURRENT}},
};


// Regulator structure for WCN7850 BT SoC series
static struct bt_power_vreg_data bt_vregs_info_kiwi[] = {
	{NULL, "qcom,bt-vdd-io",      1800000, 1800000, 0, false, true,
		{BT_VDD_IO_LDO, BT_VDD_IO_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-aon",     950000,  950000,  0, false, true,
		{BT_VDD_AON_LDO, BT_VDD_AON_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-rfacmn",  950000,  950000,  0, false, true,
		{BT_VDD_RFACMN, BT_VDD_RFACMN_CURRENT}},
	/* BT_CX_MX */
	{NULL, "qcom,bt-vdd-dig",      950000,  950000,  0, false, true,
		{BT_VDD_DIG_LDO, BT_VDD_DIG_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-rfa-0p8",  950000,  952000,  0, false, true,
		{BT_VDD_RFA_0p8, BT_VDD_RFA_0p8_CURRENT}},
	{NULL, "qcom,bt-vdd-rfa1",     1900000, 1900000, 0, false, true,
		{BT_VDD_RFA1_LDO, BT_VDD_RFA1_LDO_CURRENT}},
	{NULL, "qcom,bt-vdd-rfa2",     1350000, 1350000, 0, false, true,
		{BT_VDD_RFA2_LDO, BT_VDD_RFA2_LDO_CURRENT}},
};

// Regulator structure for WCN399x BT SoC series
static struct bt_power bt_vreg_info_wcn399x = {
	.compatible = "qcom,wcn3990",
	.vregs = (struct bt_power_vreg_data []) {
		{NULL, "qcom,bt-vdd-smps", 984000,  984000, 0, false, false,
			{BT_VDD_SMPS, BT_VDD_SMPS_CURRENT}},
		{NULL, "qcom,bt-vdd-io",   1700000, 1900000, 0, false, false,
			{BT_VDD_IO_LDO, BT_VDD_IO_LDO_CURRENT}},
		{NULL, "qcom,bt-vdd-core", 1304000, 1304000, 0, false, false,
			{BT_VDD_CORE_LDO, BT_VDD_CORE_LDO_CURRENT}},
		{NULL, "qcom,bt-vdd-pa",   3000000, 3312000, 0, false, false,
			{BT_VDD_PA_LDO, BT_VDD_PA_LDO_CURRENT}},
		{NULL, "qcom,bt-vdd-xtal", 1700000, 1900000, 0, false, false,
			{BT_VDD_XTAL_LDO, BT_VDD_XTAL_LDO_CURRENT}},
	},
	.num_vregs = 5,
};


static struct bt_power bt_vreg_info_qca6174 = {
	.compatible = "qcom,qca6174",
	.vregs = bt_vregs_info_qca61x4_937x,
	.num_vregs = ARRAY_SIZE(bt_vregs_info_qca61x4_937x),
};

static struct bt_power bt_vreg_info_qca6390 = {
	.compatible = "qcom,qca6390",
	.vregs = bt_vregs_info_qca6xx0,
	.num_vregs = ARRAY_SIZE(bt_vregs_info_qca6xx0),
};

static struct bt_power bt_vreg_info_qca6490 = {
	.compatible = "qcom,qca6490",
	.vregs = bt_vregs_info_qca6xx0,
	.num_vregs = ARRAY_SIZE(bt_vregs_info_qca6xx0),
};

static struct bt_power bt_vreg_info_kiwi = {
	.compatible = "qcom,kiwi",
	.vregs = bt_vregs_info_kiwi,
	.num_vregs = ARRAY_SIZE(bt_vregs_info_kiwi),
};


static struct bt_power bt_vreg_info_wcn6750 = {
	.compatible = "qcom,wcn6750-bt",
	.vregs = bt_vregs_info_qca6xx0,
	.num_vregs = ARRAY_SIZE(bt_vregs_info_qca6xx0),
};

static const struct of_device_id bt_power_match_table[] = {
	{	.compatible = "qcom,qca6174", .data = &bt_vreg_info_qca6174},
	{	.compatible = "qcom,wcn3990", .data = &bt_vreg_info_wcn399x},
	{	.compatible = "qcom,qca6390", .data = &bt_vreg_info_qca6390},
	{	.compatible = "qcom,qca6490", .data = &bt_vreg_info_qca6490},
	{	.compatible = "qcom,kiwi",    .data = &bt_vreg_info_kiwi},
	{	.compatible = "qcom,wcn6750-bt", .data = &bt_vreg_info_wcn6750},
	{},
};

static int bt_power_vreg_set(enum bt_power_modes mode);
static int btpower_enable_ipa_vreg(struct btpower_platform_data *pdata);

static int bt_power_src_status[BT_POWER_SRC_SIZE];
static struct btpower_platform_data *bt_power_pdata;
static bool previous;
static int pwr_state;
static struct class *bt_class;
static int bt_major;
static int soc_id;
static bool probe_finished;

static int btpower_get_temperature(struct btpower_platform_data *pdata,
				   int *temp)
{
	struct thermal_zone_device *thermal_dev;
	int ret;

	/* Temperature sensor is not provided in dts */
	if (!pdata->tsens)
		return -ENODEV;

	thermal_dev = thermal_zone_get_zone_by_name(pdata->tsens);
	if (IS_ERR(thermal_dev)) {
		pr_err("Fail to get thermal zone. ret: %d\n",
			PTR_ERR(thermal_dev));
		return PTR_ERR(thermal_dev);
	}

	ret = thermal_zone_get_temp(thermal_dev, temp);
	if (ret)
		pr_err("Fail to get temperature. ret: %d\n", ret);

	return ret;
}

#ifdef CONFIG_MSM_BT_OOBS
static void btpower_uart_transport_locked(struct btpower_platform_data *drvdata,
					  bool locked)
{
	pr_debug("%s: %s\n", __func__, (locked ? "busy" : "idle"));
}

static irqreturn_t btpower_host_wake_isr(int irq, void *data)
{
	struct btpower_platform_data *drvdata = data;
	int host_waking = gpio_get_value(drvdata->bt_gpio_host_wake);
	struct kernel_siginfo siginfo;
	int rc = 0;

	pr_debug("%s: bt-hostwake-gpio(%d) IRQ(%d) value(%d)\n", __func__,
		drvdata->bt_gpio_host_wake, drvdata->irq, host_waking);

	if (drvdata->reftask_obs == NULL) {
		pr_info("%s: ignore BT-HOSTWAKE IRQ\n", __func__);
		return IRQ_HANDLED;
	}

	// Sending signal to HAL layer
	memset(&siginfo, 0, sizeof(siginfo));
	siginfo.si_signo = SIGIO;
	siginfo.si_code = SI_QUEUE;
	siginfo.si_int = host_waking;
	rc = send_sig_info(siginfo.si_signo, &siginfo, drvdata->reftask_obs);
	if (rc < 0) {
		pr_err("%s: failed (%d) to send SIG to HAL(%d)\n", __func__,
			rc, drvdata->reftask_obs->pid);
	}
	return IRQ_HANDLED;
}
#endif

static int bt_vreg_enable(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	pr_debug("%s: vreg_en for : %s\n", __func__, vreg->name);

	if (!vreg->is_enabled) {
		if ((vreg->min_vol != 0) && (vreg->max_vol != 0)) {
			rc = regulator_set_voltage(vreg->reg,
						vreg->min_vol,
						vreg->max_vol);
			if (rc < 0) {
				pr_err("%s: regulator_set_voltage(%s) failed rc=%d\n",
						__func__, vreg->name, rc);
				goto out;
			}
		}

		if (vreg->load_curr >= 0) {
			rc = regulator_set_load(vreg->reg,
					vreg->load_curr);
			if (rc < 0) {
				pr_err("%s: regulator_set_load(%s) failed rc=%d\n",
				__func__, vreg->name, rc);
				goto out;
			}
		}

		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			pr_err("%s: regulator_enable(%s) failed. rc=%d\n",
					__func__, vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = true;
	}
out:
	return rc;
}

static int bt_vreg_enable_retention(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	if (!vreg)
		return rc;

	pr_debug("%s: enable_retention for : %s\n", __func__, vreg->name);

	if ((vreg->is_enabled) && (vreg->is_retention_supp)) {
		if ((vreg->min_vol != 0) && (vreg->max_vol != 0)) {
			/* Set the min voltage to 0 */
			rc = regulator_set_voltage(vreg->reg, 0, vreg->max_vol);
			if (rc < 0) {
				pr_err("%s: regulator_set_voltage(%s) failed rc=%d\n",
				__func__, vreg->name, rc);
				goto out;
			}
		}
		if (vreg->load_curr >= 0) {
			rc = regulator_set_load(vreg->reg, 0);
			if (rc < 0) {
				pr_err("%s: regulator_set_load(%s) failed rc=%d\n",
				__func__, vreg->name, rc);
			}
		}
	}
out:
	return rc;
}

static int bt_vreg_disable(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	if (!vreg)
		return rc;

	pr_debug("%s for : %s\n", __func__, vreg->name);

	if (vreg->is_enabled) {
		rc = regulator_disable(vreg->reg);
		if (rc < 0) {
			pr_err("%s, regulator_disable(%s) failed. rc=%d\n",
					__func__, vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = false;

		if ((vreg->min_vol != 0) && (vreg->max_vol != 0)) {
			/* Set the min voltage to 0 */
			rc = regulator_set_voltage(vreg->reg, 0,
					vreg->max_vol);
			if (rc < 0) {
				pr_err("%s: regulator_set_voltage(%s) failed rc=%d\n",
				__func__, vreg->name, rc);
				goto out;
			}
		}
		if (vreg->load_curr >= 0) {
			rc = regulator_set_load(vreg->reg, 0);
			if (rc < 0) {
				pr_err("%s: regulator_set_load(%s) failed rc=%d\n",
				__func__, vreg->name, rc);
			}
		}
	}
out:
	return rc;
}

static int bt_clk_enable(struct bt_power_clk_data *clk)
{
	int rc = 0;

	pr_info("%s: %s\n", __func__, clk->name);

	/* Get the clock handle for vreg */
	if (!clk->clk || clk->is_enabled) {
		pr_err("%s: error - node: %p, clk->is_enabled:%d\n",
			__func__, clk->clk, clk->is_enabled);
		return -EINVAL;
	}

	rc = clk_prepare_enable(clk->clk);
	if (rc) {
		pr_err("%s: failed to enable %s, rc(%d)\n",
				__func__, clk->name, rc);
		return rc;
	}

	clk->is_enabled = true;
	return rc;
}

static int bt_clk_disable(struct bt_power_clk_data *clk)
{
	int rc = 0;

	pr_debug("%s: %s\n", __func__, clk->name);

	/* Get the clock handle for vreg */
	if (!clk->clk || !clk->is_enabled) {
		pr_err("%s: error - node: %p, clk->is_enabled:%d\n",
			__func__, clk->clk, clk->is_enabled);
		return -EINVAL;
	}
	clk_disable_unprepare(clk->clk);

	clk->is_enabled = false;
	return rc;
}

static void btpower_set_xo_clk_gpio_state(bool enable)
{
	int xo_clk_gpio =  bt_power_pdata->xo_gpio_clk;
	int retry = 0;
	int rc = 0;

	if (xo_clk_gpio < 0)
		return;

retry_gpio_req:
	rc = gpio_request(xo_clk_gpio, "bt_xo_clk_gpio");
	if (rc) {
		if (retry++ < XO_CLK_RETRY_COUNT_MAX) {
			/* wait for ~(10 - 20) ms */
			usleep_range(10000, 20000);
			goto retry_gpio_req;
		}
	}

	if (rc) {
		pr_err("%s: unable to request XO clk gpio %d (%d)\n",
			__func__, xo_clk_gpio, rc);
		return;
	}

	if (enable) {
		gpio_direction_output(xo_clk_gpio, 1);
		/*XO CLK must be asserted for some time before BT_EN */
		usleep_range(100, 200);
	} else {
		/* Assert XO CLK ~(2-5)ms before off for valid latch in HW */
		usleep_range(4000, 6000);
		gpio_direction_output(xo_clk_gpio, 0);
	}

	pr_info("%s:gpio(%d) success\n", __func__, xo_clk_gpio);

	gpio_free(xo_clk_gpio);
}

#ifdef CONFIG_MSM_BT_OOBS
void bt_configure_wakeup_gpios(int on)
{
	int bt_gpio_dev_wake = bt_power_pdata->bt_gpio_dev_wake;
	int bt_host_wake_gpio = bt_power_pdata->bt_gpio_host_wake;
	int rc;

	if (on) {
		if (gpio_is_valid(bt_gpio_dev_wake)) {
			gpio_set_value(bt_gpio_dev_wake, 1);
			pr_debug("%s: BT-ON asserting BT_WAKE(%d)\n", __func__,
				 bt_gpio_dev_wake);
		}

		if (gpio_is_valid(bt_host_wake_gpio)) {
			bt_power_pdata->irq = gpio_to_irq(bt_host_wake_gpio);
			pr_debug("%s: BT-ON bt-host_wake-gpio(%d) IRQ(%d)\n",
				__func__, bt_host_wake_gpio, bt_power_pdata->irq);
			rc = request_irq(bt_power_pdata->irq,
					 btpower_host_wake_isr,
					 IRQF_TRIGGER_FALLING |
					 IRQF_TRIGGER_RISING,
					 "btpower_hostwake_isr", bt_power_pdata);
			if (rc)
				pr_err("%s: unable to request IRQ %d (%d)\n",
				__func__, bt_host_wake_gpio, rc);
		}
	} else {
		if (gpio_is_valid(bt_host_wake_gpio)) {
			pr_debug("%s: BT-OFF bt-hostwake-gpio(%d) IRQ(%d) value(%d)\n",
				 __func__, bt_host_wake_gpio, bt_power_pdata->irq,
				 gpio_get_value(bt_host_wake_gpio));
			free_irq(bt_power_pdata->irq, bt_power_pdata);
		}

		if (gpio_is_valid(bt_gpio_dev_wake))
			gpio_set_value(bt_gpio_dev_wake, 0);
	}
}
#endif

static int bt_configure_gpios(int on)
{
	int rc = 0;
	int bt_reset_gpio = bt_power_pdata->bt_gpio_sys_rst;
	int wl_reset_gpio = bt_power_pdata->wl_gpio_sys_rst;
	int bt_sw_ctrl_gpio  =  bt_power_pdata->bt_gpio_sw_ctrl;
	int bt_debug_gpio  =  bt_power_pdata->bt_gpio_debug;
	int assert_dbg_gpio = 0;

	if (on) {
		rc = gpio_request(bt_reset_gpio, "bt_sys_rst_n");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, bt_reset_gpio, rc);
			return rc;
		}

		pr_info("BTON:Turn Bt OFF asserting BT_EN to low\n");
		pr_info("bt-reset-gpio(%d) value(%d)\n", bt_reset_gpio,
			gpio_get_value(bt_reset_gpio));
		rc = gpio_direction_output(bt_reset_gpio, 0);
		if (rc) {
			pr_err("%s: Unable to set direction\n", __func__);
			return rc;
		}
		bt_power_src_status[BT_RESET_GPIO] =
			gpio_get_value(bt_reset_gpio);

		msleep(50);
		pr_info("BTON:Turn Bt OFF post asserting BT_EN to low\n");
		pr_info("bt-reset-gpio(%d) value(%d)\n", bt_reset_gpio,
			gpio_get_value(bt_reset_gpio));

		if (bt_sw_ctrl_gpio >= 0) {
			bt_power_src_status[BT_SW_CTRL_GPIO] =
			gpio_get_value(bt_sw_ctrl_gpio);
			pr_info("BTON:Turn Bt OFF bt-sw-ctrl-gpio(%d) value(%d)\n",
				bt_sw_ctrl_gpio,
				bt_power_src_status[BT_SW_CTRL_GPIO]);
		}
		if (wl_reset_gpio >= 0)
			pr_info("BTON:Turn Bt ON wl-reset-gpio(%d) value(%d)\n",
				wl_reset_gpio, gpio_get_value(wl_reset_gpio));

		if ((wl_reset_gpio < 0) ||
			((wl_reset_gpio >= 0) && gpio_get_value(wl_reset_gpio))) {

			btpower_set_xo_clk_gpio_state(true);
			pr_info("BTON: WLAN ON Asserting BT_EN to high\n");
			rc = gpio_direction_output(bt_reset_gpio, 1);
			if (rc) {
				pr_err("%s: Unable to set direction\n", __func__);
				return rc;
			}
			bt_power_src_status[BT_RESET_GPIO] =
				gpio_get_value(bt_reset_gpio);
			btpower_set_xo_clk_gpio_state(false);
		}
		if ((wl_reset_gpio >= 0) && (gpio_get_value(wl_reset_gpio) == 0)) {
			int temp;
			int bt_en_delay = BT_EN_DEFAULT_DELAY;
			if (gpio_get_value(bt_reset_gpio)) {
				pr_info("BTON: WLAN OFF and BT ON are too close\n");
				pr_info("reset BT_EN, enable it after delay\n");
				rc = gpio_direction_output(bt_reset_gpio, 0);
				if (rc) {
					pr_err("%s: Unable to set direction\n",
						 __func__);
					return rc;
				}
				bt_power_src_status[BT_RESET_GPIO] =
					gpio_get_value(bt_reset_gpio);
			}
			/* BT_EN delay will decided based on the current temperature */
			if (!btpower_get_temperature(bt_power_pdata, &temp)) {
				if (temp < TEMP_THRESHOLD)
					bt_en_delay = BT_EN_MAX_DELAY;
				pr_info("%s: current temperature:%d and bt_en delay %d\n",
				__func__, temp, bt_en_delay);
			}
			pr_info("BTON: WLAN OFF waiting for %d ms delay\n", bt_en_delay);
			pr_info("for AON output to fully discharge\n");
			msleep(bt_en_delay);
			pr_info("BTON: WLAN OFF Asserting BT_EN to high\n");
			btpower_set_xo_clk_gpio_state(true);
			rc = gpio_direction_output(bt_reset_gpio, 1);
			if (rc) {
				pr_err("%s: Unable to set direction\n", __func__);
				return rc;
			}
			bt_power_src_status[BT_RESET_GPIO] =
				gpio_get_value(bt_reset_gpio);
			btpower_set_xo_clk_gpio_state(false);
		}
		/* Below block of code executes if WL_EN is pulled high when
		 * BT_EN is about to pull high. so above two if conditions are
		 * not executed.
		 */
		if (!gpio_get_value(bt_reset_gpio)) {
			btpower_set_xo_clk_gpio_state(true);
			pr_info("BTON: WLAN ON and BT ON are too close\n");
			pr_info("Asserting BT_EN to high\n");
			rc = gpio_direction_output(bt_reset_gpio, 1);
			if (rc) {
				pr_err("%s: Unable to set direction\n", __func__);
				return rc;
			}
			bt_power_src_status[BT_RESET_GPIO] =
				gpio_get_value(bt_reset_gpio);
			btpower_set_xo_clk_gpio_state(false);
		}
		msleep(50);
#ifdef CONFIG_MSM_BT_OOBS
		bt_configure_wakeup_gpios(on);
#endif
		/*  Check  if  SW_CTRL  is  asserted  */
		if  (bt_sw_ctrl_gpio  >=  0)  {
			rc  =  gpio_direction_input(bt_sw_ctrl_gpio);
			if  (rc)  {
				pr_err("%s:SWCTRL Dir Set Problem:%d\n",
					__func__, rc);
			}  else  if  (!gpio_get_value(bt_sw_ctrl_gpio))  {
				/* SW_CTRL not asserted, assert debug GPIO */
				if  (bt_debug_gpio  >=  0)
					assert_dbg_gpio = 1;
			}
		}
		if (assert_dbg_gpio) {
			rc  =  gpio_request(bt_debug_gpio, "bt_debug_n");
			if  (rc)  {
				pr_err("unable to request Debug Gpio\n");
			}  else  {
				rc = gpio_direction_output(bt_debug_gpio,  1);
				if (rc)
					pr_err("%s:Prob Set Debug-Gpio\n",
						__func__);
			}
		}
		pr_info("BTON:Turn Bt On bt-reset-gpio(%d) value(%d)\n",
			bt_reset_gpio, gpio_get_value(bt_reset_gpio));
		if (bt_sw_ctrl_gpio >= 0) {
			bt_power_src_status[BT_SW_CTRL_GPIO] =
			gpio_get_value(bt_sw_ctrl_gpio);
			pr_info("BTON: Turn BT ON bt-sw-ctrl-gpio(%d) value(%d)\n",
				bt_sw_ctrl_gpio,
				bt_power_src_status[BT_SW_CTRL_GPIO]);
		}
	} else {
#ifdef CONFIG_MSM_BT_OOBS
		bt_configure_wakeup_gpios(on);
#endif
		gpio_set_value(bt_reset_gpio, 0);
		msleep(100);
		pr_info("BT-OFF:bt-reset-gpio(%d) value(%d)\n",
			bt_reset_gpio, gpio_get_value(bt_reset_gpio));
		if (bt_sw_ctrl_gpio >= 0) {
			pr_info("BT-OFF:bt-sw-ctrl-gpio(%d) value(%d)\n",
				bt_sw_ctrl_gpio,
				gpio_get_value(bt_sw_ctrl_gpio));
		}
	}

	pr_info("%s: bt_gpio= %d on: %d\n", __func__, bt_reset_gpio, on);

	return rc;
}

static int bluetooth_power(int on)
{
	int rc = 0;

	pr_err("%s: on: %d\n", __func__, on);

	if (on == 1) {
#ifdef CONFIG_ARCH_NEO
#if IS_ENABLED(CONFIG_ICNSS2)
		icnss_power_trigger_pinctrl(NULL, ICNSS_PINCTRL_OWNER_BT,
					    ICNSS_PINCTRL_SEQ_ON);
#endif
#endif
		rc = bt_power_vreg_set(BT_POWER_ENABLE);
		if (rc < 0) {
			pr_err("%s: bt_power regulators config failed\n",
				__func__);
			goto regulator_fail;
		}
		/* Parse dt_info and check if a target requires clock voting.
		 * Enable BT clock when BT is on and disable it when BT is off
		 */
		if (bt_power_pdata->bt_chip_clk) {
			rc = bt_clk_enable(bt_power_pdata->bt_chip_clk);
			if (rc < 0) {
				pr_err("%s: bt_power gpio config failed\n",
					__func__);
				goto clk_fail;
			}
		}
		if (bt_power_pdata->bt_gpio_sys_rst > 0) {
			bt_power_src_status[BT_RESET_GPIO] =
				DEFAULT_INVALID_VALUE;
			bt_power_src_status[BT_SW_CTRL_GPIO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_gpios(on);
			if (rc < 0) {
				pr_err("%s: bt_power gpio config failed\n",
					__func__);
				goto gpio_fail;
			}
		}
	} else if (on == 0) {
		// Power Off
		if (bt_power_pdata->bt_gpio_sys_rst > 0)
			bt_configure_gpios(on);
gpio_fail:
		if (bt_power_pdata->bt_gpio_sys_rst > 0)
			gpio_free(bt_power_pdata->bt_gpio_sys_rst);
		if (bt_power_pdata->bt_gpio_debug  >  0)
			gpio_free(bt_power_pdata->bt_gpio_debug);
		if (bt_power_pdata->bt_chip_clk)
			bt_clk_disable(bt_power_pdata->bt_chip_clk);
clk_fail:
regulator_fail:
#ifdef CONFIG_ARCH_NEO
#if IS_ENABLED(CONFIG_ICNSS2)
		icnss_power_trigger_pinctrl(NULL, ICNSS_PINCTRL_OWNER_BT,
					    ICNSS_PINCTRL_SEQ_OFF);
#endif
#endif
		bt_power_vreg_set(BT_POWER_DISABLE);
	} else if (on == 2) {
		/* Retention mode */
		bt_power_vreg_set(BT_POWER_RETENTION);
	} else {
		pr_err("%s: Invalid power mode: %d\n", __func__, on);
		rc = -1;
	}
	return rc;
}

static int btpower_toggle_radio(void *data, bool blocked)
{
	int ret = 0;
	int (*power_control)(int enable);

	power_control =
		((struct btpower_platform_data *)data)->bt_power_setup;

	if (previous != blocked)
		ret = (*power_control)(!blocked);
	if (!ret)
		previous = blocked;
	return ret;
}

static const struct rfkill_ops btpower_rfkill_ops = {
	.set_block = btpower_toggle_radio,
};

static ssize_t extldo_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return scnprintf(buf, 6, "false\n");
}

static DEVICE_ATTR_RO(extldo);

static int btpower_rfkill_probe(struct platform_device *pdev)
{
	struct rfkill *rfkill;
	int ret;

	rfkill = rfkill_alloc("bt_power", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
						&btpower_rfkill_ops,
						pdev->dev.platform_data);

	if (!rfkill) {
		dev_err(&pdev->dev, "rfkill allocate failed\n");
		return -ENOMEM;
	}

	/* add file into rfkill0 to handle LDO27 */
	ret = device_create_file(&pdev->dev, &dev_attr_extldo);
	if (ret < 0)
		pr_err("%s: device create file error\n", __func__);

	/* force Bluetooth off during init to allow for user control */
	rfkill_init_sw_state(rfkill, 1);
	previous = true;

	ret = rfkill_register(rfkill);
	if (ret) {
		dev_err(&pdev->dev, "rfkill register failed=%d\n", ret);
		rfkill_destroy(rfkill);
		return ret;
	}

	platform_set_drvdata(pdev, rfkill);

	return 0;
}

static void btpower_rfkill_remove(struct platform_device *pdev)
{
	struct rfkill *rfkill;

	pr_debug("%s\n", __func__);

	rfkill = platform_get_drvdata(pdev);
	if (rfkill)
		rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);
	platform_set_drvdata(pdev, NULL);
}

#define MAX_PROP_SIZE 32
static int bt_dt_parse_vreg_info(struct device *dev,
		struct bt_power_vreg_data *vreg_data)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct bt_power_vreg_data *vreg = vreg_data;
	struct device_node *np = dev->of_node;
	const char *vreg_name = vreg_data->name;

	pr_debug("%s: vreg dev tree parse for %s\n", __func__, vreg_name);

	snprintf(prop_name, sizeof(prop_name), "%s-supply", vreg_name);
	if (of_parse_phandle(np, prop_name, 0)) {
		vreg->reg = regulator_get(dev, vreg_name);
		if (IS_ERR(vreg->reg)) {
			ret = PTR_ERR(vreg->reg);
			vreg->reg = NULL;
			pr_warn("%s: failed to get: %s error:%d\n", __func__,
				vreg_name, ret);
			return ret;
		}

		snprintf(prop_name, sizeof(prop_name), "%s-config", vreg->name);
		prop = of_get_property(dev->of_node, prop_name, &len);
		if (!prop || len != (4 * sizeof(__be32))) {
			pr_debug("%s: Property %s %s, use default\n",
				__func__, prop_name,
				prop ? "invalid format" : "doesn't exist");
		} else {
			vreg->min_vol = be32_to_cpup(&prop[0]);
			vreg->max_vol = be32_to_cpup(&prop[1]);
			vreg->load_curr = be32_to_cpup(&prop[2]);
			vreg->is_retention_supp = be32_to_cpup(&prop[3]);
		}

		pr_debug("%s: Got regulator: %s, min_vol: %u, max_vol: %u, load_curr: %u,is_retention_supp: %u\n",
			__func__, vreg->name, vreg->min_vol, vreg->max_vol,
			vreg->load_curr, vreg->is_retention_supp);
	} else {
		pr_info("%s: %s is not provided in device tree\n", __func__,
			vreg_name);
	}

	return ret;
}

static int bt_dt_parse_clk_info(struct device *dev,
		struct bt_power_clk_data **clk_data)
{
	int ret = -EINVAL;
	struct bt_power_clk_data *clk = NULL;
	struct device_node *np = dev->of_node;

	pr_debug("%s\n", __func__);

	*clk_data = NULL;
	if (of_parse_phandle(np, "clocks", 0)) {
		clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
		if (!clk) {
			ret = -ENOMEM;
			goto err;
		}

		/* Allocated 20 bytes size buffer for clock name string */
		clk->name = devm_kzalloc(dev, 20, GFP_KERNEL);

		/* Parse clock name from node */
		ret = of_property_read_string_index(np, "clock-names", 0,
				&(clk->name));
		if (ret < 0) {
			pr_err("%s: reading \"clock-names\" failed\n",
				__func__);
			return ret;
		}

		clk->clk = devm_clk_get(dev, clk->name);
		if (IS_ERR(clk->clk)) {
			ret = PTR_ERR(clk->clk);
			pr_err("%s: failed to get %s, ret (%d)\n",
				__func__, clk->name, ret);
			clk->clk = NULL;
			return ret;
		}

		*clk_data = clk;
	} else {
		pr_err("%s: clocks is not provided in device tree\n", __func__);
	}

err:
	return ret;
}

static int bt_power_vreg_get(struct platform_device *pdev)
{
	int num_vregs, i = 0, ret = 0;
	const struct bt_power *data;

	data = of_device_get_match_data(&pdev->dev);
	if (!data) {
		pr_err("%s: failed to get dev node\n", __func__);
		return -EINVAL;
	}

	memcpy(&bt_power_pdata->compatible, &data->compatible, MAX_PROP_SIZE);
	bt_power_pdata->vreg_info = data->vregs;
	num_vregs = bt_power_pdata->num_vregs = data->num_vregs;
	for (; i < num_vregs; i++) {
		ret = bt_dt_parse_vreg_info(&(pdev->dev),
					    &bt_power_pdata->vreg_info[i]);
		/* No point to go further if failed to get regulator handler */
		if (ret)
			break;
	}

	return ret;
}

static int bt_power_vreg_set(enum bt_power_modes mode)
{
	int num_vregs, i = 0, ret = 0;
	int log_indx;
	struct bt_power_vreg_data *vreg_info = NULL;

	num_vregs =  bt_power_pdata->num_vregs;
	if (mode == BT_POWER_ENABLE) {
		for (; i < num_vregs; i++) {
			vreg_info = &bt_power_pdata->vreg_info[i];
			log_indx = vreg_info->indx.init;
			if (vreg_info->reg) {
				bt_power_src_status[log_indx] =
					DEFAULT_INVALID_VALUE;
				ret = bt_vreg_enable(vreg_info);
				if (ret < 0)
					goto out;
				if (vreg_info->is_enabled) {
					bt_power_src_status[log_indx] =
						regulator_get_voltage(
							vreg_info->reg);
				}
			}
		}
	} else if (mode == BT_POWER_DISABLE) {
		for (; i < num_vregs; i++) {
			vreg_info = &bt_power_pdata->vreg_info[i];
			ret = bt_vreg_disable(vreg_info);
		}
	} else if (mode == BT_POWER_RETENTION) {
		for (; i < num_vregs; i++) {
			vreg_info = &bt_power_pdata->vreg_info[i];
			ret = bt_vreg_enable_retention(vreg_info);
		}
	} else {
		pr_err("%s: Invalid power mode: %d\n", __func__, mode);
		ret = -1;
	}

out:
	return ret;
}

static void bt_power_vreg_put(void)
{
	int i = 0;
	struct bt_power_vreg_data *vreg_info = NULL;
	int num_vregs = bt_power_pdata->num_vregs;

	for (; i < num_vregs; i++) {
		vreg_info = &bt_power_pdata->vreg_info[i];
		if (vreg_info->reg)
			regulator_put(vreg_info->reg);
	}
}


static int bt_power_populate_dt_pinfo(struct platform_device *pdev)
{
	int rc;

	pr_debug("%s\n", __func__);

	if (!bt_power_pdata)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		rc = bt_power_vreg_get(pdev);
		if (rc)
			return rc;

		bt_power_pdata->bt_gpio_sys_rst =
			of_get_named_gpio(pdev->dev.of_node,
						"qcom,bt-reset-gpio", 0);
		if (bt_power_pdata->bt_gpio_sys_rst < 0)
			pr_warn("bt-reset-gpio not provided in devicetree\n");

		bt_power_pdata->wl_gpio_sys_rst =
			of_get_named_gpio(pdev->dev.of_node,
						"qcom,wl-reset-gpio", 0);
		if (bt_power_pdata->wl_gpio_sys_rst < 0)
			pr_err("%s: wl-reset-gpio not provided in device tree\n",
				__func__);


		bt_power_pdata->bt_gpio_sw_ctrl  =
			of_get_named_gpio(pdev->dev.of_node,
						"qcom,bt-sw-ctrl-gpio",  0);
		if (bt_power_pdata->bt_gpio_sw_ctrl < 0)
			pr_warn("bt-sw-ctrl-gpio not provided in devicetree\n");

		bt_power_pdata->bt_gpio_debug  =
			of_get_named_gpio(pdev->dev.of_node,
						"qcom,bt-debug-gpio",  0);
		if (bt_power_pdata->bt_gpio_debug < 0)
			pr_warn("bt-debug-gpio not provided in devicetree\n");

		bt_power_pdata->xo_gpio_clk =
			of_get_named_gpio(pdev->dev.of_node,
						"qcom,xo-clk-gpio", 0);
		if (bt_power_pdata->xo_gpio_clk < 0)
			pr_warn("xo-clk-gpio not provided in devicetree\n");

		rc = bt_dt_parse_clk_info(&pdev->dev,
					&bt_power_pdata->bt_chip_clk);
		if (rc < 0)
			pr_warn("%s: clock not provided in device tree\n",
				__func__);
#ifdef CONFIG_MSM_BT_OOBS
		bt_power_pdata->bt_gpio_dev_wake =
			of_get_named_gpio(pdev->dev.of_node,
					  "qcom,btwake_gpio", 0);
		if (bt_power_pdata->bt_gpio_dev_wake < 0)
			pr_warn("%s: btwake-gpio not provided in device tree\n",
				__func__);


		bt_power_pdata->bt_gpio_host_wake =
			of_get_named_gpio(pdev->dev.of_node,
					  "qcom,bthostwake_gpio", 0);
		if (bt_power_pdata->bt_gpio_host_wake < 0)
			pr_warn("%s: bthostwake_gpio not provided in device tree\n",
				__func__);
#endif
		rc = of_property_read_string(pdev->dev.of_node,
					      "tsens",
					      &bt_power_pdata->tsens);
		if (rc)
			pr_warn("%s: temperature sensor is not provided in dts\n",
				__func__);
		else
			pr_info("%s: temperature sensor is provided in dts\n",
				__func__);
	}

	bt_power_pdata->bt_power_setup = bluetooth_power;

	return 0;
}

static int bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;
	int itr;

	pr_debug("%s\n", __func__);

	/* Fill whole array with -2 i.e NOT_AVAILABLE state by default
	 * for any GPIO or Reg handle.
	 */
	for (itr = PWR_SRC_INIT_STATE_IDX;
		itr < BT_POWER_SRC_SIZE; ++itr)
		bt_power_src_status[itr] = PWR_SRC_NOT_AVAILABLE;

	bt_power_pdata = kzalloc(sizeof(*bt_power_pdata), GFP_KERNEL);

	if (!bt_power_pdata)
		return -ENOMEM;

	bt_power_pdata->pdev = pdev;
	if (pdev->dev.of_node) {
		ret = bt_power_populate_dt_pinfo(pdev);
		if (ret < 0) {
			pr_err("%s, Failed to populate device tree info\n",
				__func__);
			goto free_pdata;
		}
		pdev->dev.platform_data = bt_power_pdata;
	} else if (pdev->dev.platform_data) {
		/* Optional data set to default if not provided */
		if (!((struct btpower_platform_data *)
			(pdev->dev.platform_data))->bt_power_setup)
			((struct btpower_platform_data *)
				(pdev->dev.platform_data))->bt_power_setup =
						bluetooth_power;

		memcpy(bt_power_pdata, pdev->dev.platform_data,
			sizeof(struct btpower_platform_data));
		pwr_state = 0;
	} else {
		pr_err("%s: Failed to get platform data\n", __func__);
		goto free_pdata;
	}

	if (btpower_rfkill_probe(pdev) < 0)
		goto free_pdata;

	btpower_aop_mbox_init(bt_power_pdata);

	probe_finished = true;
	return 0;

free_pdata:
	kfree(bt_power_pdata);
	return ret;
}

static int bt_power_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);

	probe_finished = false;
	btpower_rfkill_remove(pdev);
	bt_power_vreg_put();

	kfree(bt_power_pdata);

	return 0;
}

int btpower_register_slimdev(struct device *dev)
{
	pr_debug("%s\n", __func__);
	if (!bt_power_pdata || (dev == NULL)) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return -EINVAL;
	}
	bt_power_pdata->slim_dev = dev;
	return 0;
}
EXPORT_SYMBOL(btpower_register_slimdev);

int btpower_get_chipset_version(void)
{
	pr_debug("%s\n", __func__);
	return soc_id;
}
EXPORT_SYMBOL(btpower_get_chipset_version);

static void  set_pwr_srcs_status(struct bt_power_vreg_data *handle)
{
	int ldo_index;

	if (handle) {
		ldo_index = handle->indx.crash;
		bt_power_src_status[ldo_index] =
			DEFAULT_INVALID_VALUE;
		if (handle->is_enabled &&
			(regulator_is_enabled(handle->reg))) {
			bt_power_src_status[ldo_index] =
				(int)regulator_get_voltage(handle->reg);
			pr_err("%s(%p) value(%d)\n", handle->name,
				handle, bt_power_src_status[ldo_index]);
		} else {
			pr_err("%s:%s is_enabled: %d\n",
				__func__, handle->name,
				handle->is_enabled);
		}
	}
}

static void  set_gpios_srcs_status(char *gpio_name,
		int gpio_index, int handle)
{
	if (handle >= 0) {
		bt_power_src_status[gpio_index] =
			DEFAULT_INVALID_VALUE;
		bt_power_src_status[gpio_index] =
			gpio_get_value(handle);
		pr_err("%s(%d) value(%d)\n", gpio_name,
			handle, bt_power_src_status[gpio_index]);
	} else {
		pr_err("%s: %s not configured\n",
			__func__, gpio_name);
	}
}

static long bt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0, pwr_cntrl = 0;
	int chipset_version = 0;
	int itr, num_vregs;
	struct bt_power_vreg_data *vreg_info = NULL;
#ifdef CONFIG_MSM_BT_OOBS
	enum btpower_obs_param clk_cntrl;
#endif
	if (!bt_power_pdata || !probe_finished) {
		pr_err("%s: BTPower Probing Pending.Try Again\n", __func__);
		return -EAGAIN;
	}

	switch (cmd) {
#ifdef CONFIG_MSM_BT_OOBS
	case BT_CMD_OBS_SIGNAL_TASK:
		bt_power_pdata->reffilp_obs = file;
		bt_power_pdata->reftask_obs = get_current();
		pr_info("%s: BT_CMD_OBS_SIGNAL_TASK tid %d file %pK\n",
			__func__, bt_power_pdata->reftask_obs->pid, file);
		break;
	case BT_CMD_OBS_VOTE_CLOCK:
		if (!gpio_is_valid(bt_power_pdata->bt_gpio_dev_wake)) {
			pr_debug("%s: BT_CMD_OBS_VOTE_CLOCK bt_dev_wake_n(%d) not configured\n",
				__func__, bt_power_pdata->bt_gpio_dev_wake);
			return -EIO;
		}
		clk_cntrl = (enum btpower_obs_param)arg;
		switch (clk_cntrl) {
		case BTPOWER_OBS_CLK_OFF:
			btpower_uart_transport_locked(bt_power_pdata, false);
			ret = 0;
			break;
		case BTPOWER_OBS_CLK_ON:
			btpower_uart_transport_locked(bt_power_pdata, true);
			ret = 0;
			break;
		case BTPOWER_OBS_DEV_OFF:
			gpio_set_value(bt_power_pdata->bt_gpio_dev_wake, 0);
			ret = 0;
			break;
		case BTPOWER_OBS_DEV_ON:
			gpio_set_value(bt_power_pdata->bt_gpio_dev_wake, 1);
			ret = 0;
			break;
		default:
			pr_debug("%s: BT_CMD_OBS_VOTE_CLOCK clk_cntrl(%d)\n",
				__func__, clk_cntrl);
			return -EINVAL;
		}
		pr_debug("%s: BT_CMD_OBS_VOTE_CLOCK clk_cntrl(%d) %s\n",
			__func__, clk_cntrl,
			gpio_get_value(bt_power_pdata->bt_gpio_dev_wake) ?
				"Assert" : "Deassert");
		break;
#endif
	case BT_CMD_SLIM_TEST:
#if (defined CONFIG_BT_SLIM_QCA6390 || \
	defined CONFIG_BT_SLIM_QCA6490 || \
	defined CONFIG_BTFM_SLIM_WCN3990 || \
	defined CONFIG_BTFM_SLIM_WCN7850)
		if (!bt_power_pdata->slim_dev) {
			pr_err("%s: slim_dev is null\n", __func__);
			return -EINVAL;
		}
		ret = btfm_slim_hw_init(
			bt_power_pdata->slim_dev->platform_data
		);
#endif
		break;
	case BT_CMD_PWR_CTRL:
		pwr_cntrl = (int)arg;
		pr_warn("%s: BT_CMD_PWR_CTRL pwr_cntrl: %d\n",
			__func__, pwr_cntrl);
		if (pwr_state != pwr_cntrl) {
			ret = bluetooth_power(pwr_cntrl);
			if (!ret)
				pwr_state = pwr_cntrl;
		} else {
			pr_err("%s: BT chip state is already: %d no change\n",
				__func__, pwr_state);
			ret = 0;
		}
		break;
	case BT_CMD_CHIPSET_VERS:
		chipset_version = (int)arg;
		pr_warn("%s: unified Current SOC Version : %x\n", __func__,
			chipset_version);
		if (chipset_version) {
			soc_id = chipset_version;
		} else {
			pr_err("%s: got invalid soc version\n", __func__);
			soc_id = 0;
		}
		break;
	case BT_CMD_GET_CHIPSET_ID:
		if (copy_to_user((void __user *)arg, bt_power_pdata->compatible,
		    MAX_PROP_SIZE)) {
			pr_err("%s: copy to user failed\n", __func__);
			ret = -EFAULT;
		}
		break;
	case BT_CMD_CHECK_SW_CTRL:
		/*  Check  if  SW_CTRL  is  asserted  */
		pr_info("BT_CMD_CHECK_SW_CTRL\n");
		if (bt_power_pdata->bt_gpio_sw_ctrl > 0) {
			bt_power_src_status[BT_SW_CTRL_GPIO] =
				DEFAULT_INVALID_VALUE;
			ret  =  gpio_direction_input(
				bt_power_pdata->bt_gpio_sw_ctrl);
			if (ret) {
				pr_err("%s:gpio_direction_input api\n",
					 __func__);
				pr_err("%s:failed for SW_CTRL:%d\n",
					__func__, ret);
			} else {
				bt_power_src_status[BT_SW_CTRL_GPIO] =
					gpio_get_value(
					bt_power_pdata->bt_gpio_sw_ctrl);
				pr_info("bt-sw-ctrl-gpio(%d) value(%d)\n",
					bt_power_pdata->bt_gpio_sw_ctrl,
					bt_power_src_status[BT_SW_CTRL_GPIO]);
			}
		} else {
			pr_err("bt_gpio_sw_ctrl not configured\n");
			return -EINVAL;
		}
		break;
	case BT_CMD_GETVAL_POWER_SRCS:
		pr_err("BT_CMD_GETVAL_POWER_SRCS\n");
		set_gpios_srcs_status("BT_RESET_GPIO", BT_RESET_GPIO_CURRENT,
			bt_power_pdata->bt_gpio_sys_rst);
		set_gpios_srcs_status("SW_CTRL_GPIO", BT_SW_CTRL_GPIO_CURRENT,
			bt_power_pdata->bt_gpio_sw_ctrl);

		num_vregs =  bt_power_pdata->num_vregs;
		for (itr = 0; itr < num_vregs; itr++) {
			vreg_info = &bt_power_pdata->vreg_info[itr];
			set_pwr_srcs_status(vreg_info);
		}
		if (copy_to_user((void __user *)arg,
			bt_power_src_status, sizeof(bt_power_src_status))) {
			pr_err("%s: copy to user failed\n", __func__);
			ret = -EFAULT;
		}
		break;
	case BT_CMD_SET_IPA_TCS_INFO:
		pr_err("%s: BT_CMD_SET_IPA_TCS_INFO\n", __func__);
		btpower_enable_ipa_vreg(bt_power_pdata);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return ret;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = bt_power_remove,
	.driver = {
		.name = "bt_power",
		.of_match_table = bt_power_match_table,
	},
};

static const struct file_operations bt_dev_fops = {
	.unlocked_ioctl = bt_ioctl,
	.compat_ioctl = bt_ioctl,
};

static int __init btpower_init(void)
{
	int ret = 0;

	probe_finished = false;
	ret = platform_driver_register(&bt_power_driver);
	if (ret) {
		pr_err("%s: platform_driver_register error: %d\n",
			__func__, ret);
		goto driver_err;
	}

	bt_major = register_chrdev(0, "bt", &bt_dev_fops);
	if (bt_major < 0) {
		pr_err("%s: failed to allocate char dev\n", __func__);
		ret = -1;
		goto chrdev_err;
	}

	bt_class = class_create(THIS_MODULE, "bt-dev");
	if (IS_ERR(bt_class)) {
		pr_err("%s: coudn't create class\n", __func__);
		ret = -1;
		goto class_err;
	}


	if (device_create(bt_class, NULL, MKDEV(bt_major, 0),
		NULL, "btpower") == NULL) {
		pr_err("%s: failed to allocate char dev\n", __func__);
		goto device_err;
	}
	return 0;

device_err:
	class_destroy(bt_class);
class_err:
	unregister_chrdev(bt_major, "bt");
chrdev_err:
	platform_driver_unregister(&bt_power_driver);
driver_err:
	return ret;
}

int btpower_aop_mbox_init(struct btpower_platform_data *pdata)
{
	struct mbox_client *mbox = &pdata->mbox_client_data;
	struct mbox_chan *chan;
	struct bt_power_vreg_data *vreg_ipa_info;
	int ret = 0;

	mbox->dev = &pdata->pdev->dev;
	mbox->tx_block = true;
	mbox->tx_tout = BTPOWER_MBOX_TIMEOUT_MS;
	mbox->knows_txdone = false;

	pdata->mbox_chan = NULL;
	chan = mbox_request_channel(mbox, 0);
	if (IS_ERR(chan)) {
		pr_err("%s: failed to get mbox channel\n", __func__);
		return PTR_ERR(chan);
	}
	pdata->mbox_chan = chan;

	ret = of_property_read_string(pdata->pdev->dev.of_node,
				      "qcom,vreg_ipa",
				      &pdata->vreg_ipa);
	if (ret)
		pr_info("%s: vreg for iPA not configured\n", __func__);
	else {
		pr_info("%s: Mbox channel initialized\n", __func__);
		vreg_ipa_info = devm_kzalloc(&pdata->pdev->dev,
					sizeof(*vreg_ipa_info),
					GFP_KERNEL);
		if (!vreg_ipa_info)
			return -ENOMEM;

		vreg_ipa_info->name = devm_kzalloc(&pdata->pdev->dev,
						20,
						GFP_KERNEL);
		if (!vreg_ipa_info->name)
			return -ENOMEM;

		strlcpy((char *)vreg_ipa_info->name, "qcom,vreg_ipa", MAX_PROP_SIZE);
		vreg_ipa_info->min_vol = 0;
		vreg_ipa_info->max_vol = 0;
		vreg_ipa_info->load_curr = 0;
		ret = bt_dt_parse_vreg_info(&pdata->pdev->dev, vreg_ipa_info);
		if (!ret && vreg_ipa_info->reg) {
			pr_info("%s: obtained IPA reg handler\n", __func__);
			pdata->vreg_ipa_info = vreg_ipa_info;
		} else {
			pr_warn("%s: failed to obtain IPA reg handler\n",
				__func__);
		}
		return ret;
	}
	return 0;
}

static int btpower_aop_set_vreg_param(struct btpower_platform_data *pdata,
				   const char *vreg_name,
				   enum btpower_vreg_param param,
				   enum btpower_tcs_seq seq, int val)
{
	struct qmp_pkt pkt;
	char mbox_msg[BTPOWER_MBOX_MSG_MAX_LEN];
	static const char * const vreg_param_str[] = {"v", "m", "e"};
	static const char *const tcs_seq_str[] = {"upval", "dwnval", "enable"};
	struct bt_power_vreg_data *vreg_ipa_info = pdata->vreg_ipa_info;
	int ret = 0;

	if (param > BTPOWER_VREG_ENABLE || seq > BTPOWER_TCS_ALL_SEQ || !vreg_name)
		return -EINVAL;

	snprintf(mbox_msg, BTPOWER_MBOX_MSG_MAX_LEN,
		 "{class: wlan_pdc, res: %s.%s, %s: %d}", vreg_name,
		 vreg_param_str[param], tcs_seq_str[seq], val);

	pr_info("%s: sending AOP Mbox msg: %s\n", __func__, mbox_msg);
	pkt.size = BTPOWER_MBOX_MSG_MAX_LEN;
	pkt.data = mbox_msg;

	ret = mbox_send_message(pdata->mbox_chan, &pkt);
	if (ret < 0)
		pr_err("%s:Failed to send AOP mbox msg(%s), err(%d)\n",
					__func__, mbox_msg, ret);

	if (!vreg_ipa_info) {
		pr_warn("%s: IPA regulator entry is missing in dts\n", __func__);
		return -EINVAL;
	}

	if (vreg_ipa_info->reg) {
		ret = bt_vreg_enable(vreg_ipa_info);
		if (ret < 0)
			pr_err("%s: Failed to enable IPA vreg\n", __func__);
	}
	return ret;
}

static int btpower_enable_ipa_vreg(struct btpower_platform_data *pdata)
{
	int ret = 0;
	static bool config_done;

	if (config_done) {
		pr_info("%s: IPA Vreg already configured\n", __func__);
		return 0;
	}

	if (!pdata->vreg_ipa || !pdata->mbox_chan) {
		pr_info("%s: mbox/iPA vreg not configured\n", __func__);
	} else {
		ret = btpower_aop_set_vreg_param(pdata,
					       pdata->vreg_ipa,
					       BTPOWER_VREG_ENABLE,
					       BTPOWER_TCS_UP_SEQ, 1);
		if (ret >=  0) {
			pr_info("%s:Enabled iPA\n", __func__);
			config_done = true;
		}
	}

	return ret;
}

static void __exit btpower_exit(void)
{
	platform_driver_unregister(&bt_power_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM Bluetooth power control driver");

#ifdef CONFIG_ARCH_NEO
#if IS_ENABLED(CONFIG_ICNSS2)
MODULE_SOFTDEP("pre: icnss2");
#endif
#endif

module_init(btpower_init);
module_exit(btpower_exit);

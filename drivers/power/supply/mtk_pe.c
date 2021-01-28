// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include "mtk_pe.h"
#include "mtk_charger_algorithm_class.h"


static int pe_leave(struct chg_alg_device *alg, bool disable_charging)
{
	int ret = 0, ret_value;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	chr_debug("%s: starts\n", __func__);

	/* CV point reached, disable charger */
	ret = pe_hal_enable_charging(alg, disable_charging);
	if (ret < 0) {
		pr_notice("%s enable charging fail:%d\n",
			ret);
		ret_value = -EHAL;
	}

	/* Decrease TA voltage to 5V */
	ret = mtk_pe_reset_ta_vchr(alg);
	if (ret < 0) {
		pr_notice("%s reset TA fail:%d\n",
			ret);
		ret_value = -EHAL;
	}

	pr_debug("%s: OK\n", __func__);
	return ret;
}

static int pe_check_leave_status(struct chg_alg_device *alg)
{
	int ret = 0;
	u32 ichg = 0, vchr = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	chr_debug("%s: starts\n", __func__);

	/* PE+ leaves unexpectedly */
#ifdef FIXME
	vchr = pe_hal_get_vbus(alg);
	if (abs(vchr - pe->ta_vchr_org) < 1000000) {
		pr_notice("%s: PE+ leave unexpectedly,
			recheck TA vbus:%d vchg_org:%d !\n",
			__func__, vchr, pe->ta_vchr_org);
		ret = pe_leave(alg, true);
		if (ret < 0)
			goto _err;

		return ret;
	}
#endif
	ichg = pe_hal_get_ibat(alg);

	/* Check SOC & Ichg */
	if (pe->ta_stop_battery_soc < pe_hal_get_uisoc(alg) &&
	    ichg > 0 && ichg < pe->pe_ichg_level_threshold) {
		ret = pe_leave(alg, true);

		pr_debug("%s: OK, SOC = (%d,%d), Ichg:%dmA,ret:%d stop PE+\n",
			__func__, pe_hal_get_uisoc(alg),
			pe->ta_stop_battery_soc, ichg / 1000,
			ret);
		return 1;
	}
	return 0;
}

int __pe_increase_ta_vchr(struct chg_alg_device *alg)
{
	int ret = 0;
	bool increase = true; /* Increase */

	if (pe_hal_get_charger_type(alg) != CHARGER_UNKNOWN) {
		ret = pe_hal_send_ta_current_pattern(alg,
							increase);

		if (ret < 0)
			pr_notice("%s: failed, ret = %d\n", __func__, ret);
		else
			pr_debug("%s: OK\n", __func__);
		return ret;
	}

	/* TA is not exist */
	ret = -EIO;
	pr_notice("%s: failed, cable out\n", __func__);
	return ret;
}

static int pe_increase_ta_vchr(struct chg_alg_device *alg, u32 vchr_target)
{
	int ret = 0;
	int vchr_before, vchr_after;
	u32 retry_cnt = 0;
	bool is_chip_enabled = false;
	int chg_cnt, i;

	do {

		chg_cnt = pe_hal_get_charger_cnt(alg);
		if (chg_cnt > 1) {
			for (i = CHG2; i < CHG_MAX; i++) {
				is_chip_enabled = pe_hal_is_chip_enable(alg, i);
				if (is_chip_enabled)
					pe_hal_enable_chip(alg, i, false);
			}
		}

		vchr_before = pe_hal_get_vbus(alg);
		__pe_increase_ta_vchr(alg);
		vchr_after = pe_hal_get_vbus(alg);

		if (abs(vchr_after - vchr_target) <= 1000000) {
			pr_debug("%s: OK\n", __func__);
			return ret;
		}
		pr_notice("%s: retry, cnt = %d, vchr = (%d, %d), vchr_target = %d\n",
			__func__, retry_cnt, vchr_before / 1000,
			vchr_after / 1000, vchr_target / 1000);

		retry_cnt++;
	} while (pe_hal_get_charger_type(alg) != CHARGER_UNKNOWN &&
		 retry_cnt < 3);

	ret = -EIO;
	pr_notice("%s: failed, vchr = (%d, %d), vchr_target = %d\n",
		__func__, vchr_before / 1000, vchr_after / 1000,
		vchr_target / 1000);

	return ret;
}

static int pe_detect_ta(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	chr_debug("%s: starts\n", __func__);

	/* Disable OVP */
	ret = pe_hal_enable_vbus_ovp(alg, false);
	if (ret < 0)
		goto _err;

	pe->ta_vchr_org = pe_hal_get_vbus(alg);
	ret = pe_increase_ta_vchr(alg, 7000000); /* uv */

	if (ret == 0) {
		pr_debug("%s: OK, is_connect = %d\n", __func__,
			pe->is_connect);
		return ret;
	}

	/* Detect PE+ TA failed */
	ret = -1;

	/* Enable OVP */
	pe_hal_enable_vbus_ovp(alg, true);

	/* Set MIVR for vbus 5V */
	pe_hal_set_mivr(alg, pe->min_charger_voltage);

_err:
	pr_notice("%s: failed, is_connect = %d\n",
		__func__, pe->is_connect);

	return ret;
}

static int pe_init_ta(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	chr_debug("%s: starts\n", __func__);

	return ret;
}

static int mtk_pe_plugout_reset(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	chr_debug("%s: starts\n", __func__);

	if (pe->state != PE2_HW_FAIL)
		pe->state = PE2_HW_READY;

	ret = mtk_pe_reset_ta_vchr(alg);
	if (ret < 0)
		goto _err;

	/* Set cable out occur to false */
	mtk_pe_set_is_cable_out_occur(alg, false);
	pr_debug("%s: OK\n", __func__);
	return ret;

_err:
	pr_notice("%s: failed, ret = %d\n", __func__, ret);

	return ret;
}

int mtk_pe_reset_ta_vchr(struct chg_alg_device *alg)
{
	int ret = -1, chr_volt = 0;
	u32 retry_cnt = 0;
	struct mtk_pe *pe;
	bool is_chip_enabled = false;
	int chg_cnt, i;

	pe = dev_get_drvdata(&alg->dev);
	chr_debug("%s: starts\n", __func__);

	pe_hal_set_mivr(alg, pe->min_charger_voltage);

	/* Reset TA's charging voltage */
	do {

		chg_cnt = pe_hal_get_charger_cnt(alg);
		if (chg_cnt > 1) {
			for (i = CHG2; i < CHG_MAX; i++) {
				is_chip_enabled = pe_hal_is_chip_enable(alg, i);
				if (is_chip_enabled)
					pe_hal_enable_chip(alg, i, false);
			}
		}

		ret = pe_hal_reset_ta(alg, CHG1);
		msleep(250);

		/* Check charger's voltage */
		chr_volt = pe_hal_get_vbus(alg);
		if (abs(chr_volt - pe->ta_vchr_org) <= 1000000) {
			ret = 0;
			break;
		}

		retry_cnt++;
	} while (retry_cnt < 3);

	if (ret != 0) {
		pr_notice("%s: failed, ret = %d\n", __func__, ret);
		pe_hal_set_mivr(alg, CHG1, chr_volt - 1000000);
		/*
		 * SET_INPUT_CURRENT success but chr_volt does not reset to 5V
		 * set ret = -EIO to represent the case
		 */
		ret = -EIO;
		return ret;
	}

	/* Enable OVP */
	ret = pe_hal_enable_vbus_ovp(alg, true);
	pr_debug("%s: OK\n", __func__);

	return ret;
}

int mtk_pe_check_charger(struct chg_alg_device *alg)
{
	int ret = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);

	/* Lock */
	mutex_lock(&pe->access_lock);
	__pm_stay_awake(&pe->suspend_lock);

	chr_debug("%s: starts\n", __func__);

	if (pe_hal_get_charger_type(alg) == CHARGER_UNKNOWN ||
	    pe->is_cable_out_occur)
		mtk_pe_plugout_reset(alg);

	/* Not to check charger type or
	 * Not standard charger or
	 * SOC is not in range
	 */
	if (pe_hal_get_charger_type(alg) != STANDARD_CHARGER ||
	    pe->ta_start_battery_soc > pe_hal_get_uisoc(alg) ||
	    pe->ta_stop_battery_soc <= pe_hal_get_uisoc(alg))
		goto _err;

	/* Reset/Init/Detect TA */
	ret = mtk_pe_reset_ta_vchr(alg);
	if (ret < 0)
		goto _err;

	ret = pe_init_ta(alg);
	if (ret < 0)
		goto _err;

	ret = pe_detect_ta(alg);
	if (ret < 0) {
		ret = -2;
		goto _err;
	}

	/* Unlock */
	__pm_relax(&pe->suspend_lock);
	mutex_unlock(&pe->access_lock);
	pr_debug("%s: OK\n",
		__func__);

	return ret;

_err:
	if (ret == 0)
		ret = -1;

	/* Unlock */
	__pm_relax(&pe->suspend_lock);
	mutex_unlock(&pe->access_lock);
	pr_debug("%s: stop, SOC = %d, chr_type = %d, ret = %d\n",
		__func__, pe_hal_get_uisoc(alg),
		pe_hal_get_charger_type(alg), ret);

	return ret;
}

int mtk_pe_set_charging_current(struct chg_alg_device *alg,
	unsigned int *ichg, unsigned int *aicr)
{
	int ret = 0, chr_volt = 0;
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	if (!pe->is_connect)
		return -ENOTSUPP;

	chr_volt = pe_hal_get_vbus();
	if ((chr_volt - pe->ta_vchr_org) > 6000000) { /* TA = 12V */
		*aicr = pe->ta_ac_12v_input_current;
		*ichg = pe->ta_ac_charger_current;
	} else if ((chr_volt - pe->ta_vchr_org) > 3000000) { /* TA = 9V */
		*aicr = pe->ta_ac_9v_input_current;
		*ichg = pe->ta_ac_charger_current;
	} else if ((chr_volt - pe->ta_vchr_org) > 1000000) { /* TA = 7V */
		*aicr = pe->ta_ac_7v_input_current;
		*ichg = pe->ta_ac_charger_current;
	}

	pr_debug("%s: Ichg= %dmA, AICR = %dmA, chr_org = %d, chr_after = %d\n",
		__func__, *ichg / 1000, *aicr / 1000, pe->ta_vchr_org / 1000,
		chr_volt / 1000);
	return ret;
}

void mtk_pe_set_is_cable_out_occur(struct chg_alg_device *alg, bool out)
{
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	pr_debug("%s: out = %d\n", __func__, out);
	mutex_lock(&pe->pmic_sync_lock);
	pe->is_cable_out_occur = out;
	mutex_unlock(&pe->pmic_sync_lock);
}

static char *pe_state_to_str(int state)
{
	switch (state) {
	case PE_HW_UNINIT:
		return "PE_HW_UNINIT";
	case PE_HW_FAIL:
		return "PE_HW_FAIL";
	case PE_HW_READY:
		return "PE_HW_READY";
	case PE_TA_NOT_SUPPORT:
		return "PE_TA_NOT_SUPPORT";
	case PE_STOP:
		return "PE_STOP";
	case PE_RUN:
		return "PE_RUN";
	case PE_DONE:
		return "PE_DONE";
	default:
		break;
	}
	pr_notice("%s unknown state:%d\n", __func__
		, state);
	return "PE_UNKNOWN";
}

static int _pe_is_algo_ready(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;
	int ret_value, ret;

	pe = dev_get_drvdata(&alg->dev);
	pr_notice("%s state:%s\n", __func__,
		pe_state_to_str(pe->state));

	switch (pe->state) {
	case PE_HW_UNINIT:
	case PE_HW_FAIL:
		ret_value = ALG_INIT_FAIL;
		break;
	case PE_HW_READY:
		ret = mtk_pe_check_charger(alg);
		if (ret == 0) {
			pe->state = PE_STOP;
			ret_value = ALG_READY;
		} else if (ret == -2) {
			pe->state = PE_TA_NOT_SUPPORT;
			ret_value = ALG_TA_NOT_SUPPORT;
		} else
			ret_value = ALG_TA_CHECKING;
		break;
	case PE_TA_NOT_SUPPORT:
		ret_value = ALG_TA_NOT_SUPPORT;
		break;
	case PE_STOP:
		ret_value = ALG_READY;
		break;
	case PE_RUN:
		ret_value = ALG_RUNNING;
		break;
	case PE_DONE:
		ret_value = ALG_DONE;
		break;
	default:
		break;
	}

	return ret_value;
}

static int _pe_init_algo(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;
	int ret;

	pe = dev_get_drvdata(&alg->dev);
	pr_notice("%s\n", __func__);

	if (pe2_hal_init_hardware(alg) != 0)
		pe->state = PE_HW_FAIL;
	else
		pe->state = PE_HW_READY;

	return 0;
}

static bool _pe_is_algo_running(struct chg_alg_device *alg)
{
	struct mtk_pe *pe;

	pr_notice("%s\n", __func__);
	pe = dev_get_drvdata(&alg->dev);

	if (pe->state == PE2_RUN)
		return true;

	return false;
}

static int _pe_stop_algo(struct chg_alg_device *alg)
{
	pr_notice("%s\n", __func__);

	return 0;
}

static int _pe_notifier_call(struct chg_alg_device *alg,
			 struct chg_alg_notify *notify)
{
	struct mtk_pe *pe;

	pe = dev_get_drvdata(&alg->dev);
	pr_notice("%s evt:%d\n", __func__, notify->evt);
	switch (notify->evt) {
	case EVT_PLUG_OUT:
		mutex_lock(&pe->cable_out_lock);
		pe->is_cable_out_occur = true;
		mutex_unlock(&pe->cable_out_lock);
		break;
	case EVT_PLUG_IN:
		mutex_lock(&pe->cable_out_lock);
		pe->is_cable_out_occur = false;
		mutex_unlock(&pe->cable_out_lock);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int _pe_get_status(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value)
{
	pr_notice("%s\n", __func__);
	if (s == ALG_MAX_VBUS)
		*value = 12000;
	else
		pr_notice("%s does not support prop:%d\n", __func__, s);
	return 0;
}

int _pe_set_setting(struct chg_alg_device *alg_dev,
	struct chg_alg_setting *setting)
{
	struct mtk_pe *pe;

	pr_notice("%s cv:%d icl:%d cc:%d\n",
		__func__,
		setting->cv,
		setting->input_current_limit,
		setting->charging_current_limit);
	pe = dev_get_drvdata(&alg_dev->dev);
	pe->cv = setting->cv;
	pe->input_current_limit = setting->input_current_limit;
	pe->charging_current_limit = setting->charging_current_limit;

	return 0;
}

int _pe_start_algo(struct chg_alg_device *alg)
{
	int ret = 0, chr_volt, tune;
	struct mtk_pe *pe;
	bool tune = false;

	pe = dev_get_drvdata(&alg->dev);
	pr_notice("%s state:%d %s\n", __func__,
		pe->state,
		pe_state_to_str(pe->state));

	switch (pe->state) {
	case PE_HW_UNINIT:
	case PE_HW_FAIL:
		ret = ALG_INIT_FAIL;
		break;
	case PE_HW_READY:
		ret = ALG_TA_CHECKING;
		break;
	case PE_TA_NOT_SUPPORT:
		ret = ALG_TA_NOT_SUPPORT;
		break;
	case PE_STOP:
		ret = ALG_READY;
		break;
	case PE_RUN:
		ret = ALG_RUNNING;
		break;
	case PE_DONE:
		ret = ALG_DONE;
		break;
	default:
		pr_notice("PE unknown state:%d\n", pe->state);
		ret = ALG_INIT_FAIL;
		break;
	}

	if (ret != ALG_RUNNING && ret != ALG_READY) {
		pr_notice("test1\n");
		return ret;
	}

	/* Lock */
	mutex_lock(&pe->access_lock);
	__pm_stay_awake(&pe->suspend_lock);


	if (pe->state == PE_STOP)
		pe->state = PE_RUN;

	chr_debug("%s: starts\n", __func__);

	if (pe_hal_get_charger_type(alg) == CHARGER_UNKNOWN ||
	    pe->is_cable_out_occur)
		mtk_pe_plugout_reset(alg);

	chr_volt = pe_hal_get_vbus(alg);

	if (pe->ta_9v_support && pe->ta_12v_support) {
		if (abs(chr_volt - 12000000) > 1000000)
			tune = true;
		pr_notice("%s: vbus:%d target:%d 9v:%d 12v:%d tune:%d\n",
			__func__, chr_volt,
			12000000, pe->ta_9v_support,
			pe->ta_12v_support, tune);

	} else if (pe->ta_9v_support && !pe->ta_12v_support) {
		if (abs(chr_volt - 9000000) > 1000000)
			tune = true;
		pr_notice("%s: vbus:%d target:%d 9v:%d 12v:%d tune:%d\n",
			__func__, chr_volt,
			9000000, pe->ta_9v_support,
			pe->ta_12v_support, tune);
	} else {
		tune = false;
		pr_notice("%s:error setting vbus:%d 9v:%d 12v:%d tune:%d\n",
			__func__, chr_volt,
			pe->ta_9v_support, pe->ta_12v_support,
			tune);
		goto _err;
	}

	if (tune == true) {
		/* Increase TA voltage to 9V */
		if (pe->ta_9v_support || pe->ta_12v_support) {
			ret = pe_increase_ta_vchr(alg, 9000000); /* uv */
			if (ret < 0) {
				pr_notice("%s: failed, cannot increase to 9V\n",
					__func__);
				goto _err;
			}

			/* Successfully, increase to 9V */
			pr_debug("%s: output 9V ok\n", __func__);

		}

		/* Increase TA voltage to 12V */
		if (pe->ta_12v_support) {
			ret = pe_increase_ta_vchr(alg, 12000000); /* uv */
			if (ret < 0) {
				pr_notice("%s: failed, cannot increase to 12V\n",
					__func__);
				goto _err;
			}

			/* Successfully, increase to 12V */
			pr_debug("%s: output 12V ok\n", __func__);
		}
	} else {
		ret = pe_check_leave_status(alg);
		/* if (ret == 0) */
	}

	chr_volt = pe_hal_get_vbus(alg);
	ret = pe_hal_set_mivr(alg, CHG1, chr_volt - 1000000);
	if (ret < 0)
		goto _err;

	pr_debug("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
		__func__, pe->ta_vchr_org / 1000, chr_volt / 1000,
		(chr_volt - pe->ta_vchr_org) / 1000);
	pr_debug("%s: OK\n", __func__);

	__pm_relax(&pe->suspend_lock);
	mutex_unlock(&pe->access_lock);
	return ret;

_err:
	pe_leave(alg, false);
_out:
	chr_volt = pe_hal_get_vbus();
	pr_debug("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
		__func__, pe->ta_vchr_org / 1000, chr_volt / 1000,
		(chr_volt - pe->ta_vchr_org) / 1000);

	__pm_relax(&pe->suspend_lock);
	mutex_unlock(&pe->access_lock);

	return ret;
}


static struct chg_alg_ops pe_alg_ops = {
	.init_algo = _pe_init_algo,
	.is_algo_ready = _pe_is_algo_ready,
	.start_algo = _pe_start_algo,
	.is_algo_running = _pe_is_algo_running,
	.stop_algo = _pe_stop_algo,
	.notifier_call = _pe_notifier_call,
	.get_status = _pe_get_status,
	.set_setting = _pe_set_setting,
};

static void mtk_pe_parse_dt(struct mtk_pe *pe,
				struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val;

	/* PE */

	pe->ta_12v_support = of_property_read_bool(np, "ta_12v_support");
	pe->ta_9v_support = of_property_read_bool(np, "ta_9v_support");

	if (of_property_read_u32(np, "pe_ichg_level_threshold", &val) >= 0)
		pe->pe_ichg_level_threshold = val;
	else {
		pr_notice("use default PE_ICHG_LEAVE_THRESHOLD:%d\n",
			PE_ICHG_LEAVE_THRESHOLD);
		pe->pe_ichg_level_threshold =
						PE_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "ta_start_battery_soc", &val) >= 0)
		pe->ta_start_battery_soc = val;
	else {
		pr_notice("use default TA_START_BATTERY_SOC:%d\n",
			TA_START_BATTERY_SOC);
		pe->ta_start_battery_soc = TA_START_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "ta_stop_battery_soc", &val) >= 0)
		pe->ta_stop_battery_soc = val;
	else {
		pr_notice("use default TA_STOP_BATTERY_SOC:%d\n",
			TA_STOP_BATTERY_SOC);
		pe->ta_stop_battery_soc = TA_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "min_charger_voltage", &val) >= 0)
		pe->min_charger_voltage = val;
	else {
		pr_notice("use default V_CHARGER_MIN:%d\n", PE_V_CHARGER_MIN);
		pe->min_charger_voltage = PE_V_CHARGER_MIN;
	}
}


static int mtk_pe_probe(struct platform_device *pdev)
{
	struct mtk_pe *pe = NULL;

	pr_notice("%s: starts\n", __func__);

	pe = devm_kzalloc(&pdev->dev, sizeof(*pe), GFP_KERNEL);
	if (!pe)
		return -ENOMEM;
	platform_set_drvdata(pdev, pe);
	pe->pdev = pdev;

	wakeup_source_init(&pe->suspend_lock, "PE+ suspend wakelock");
	mutex_init(&pe->access_lock);
	mutex_init(&pe->cable_out_lock);

	pe->ta_vchr_org = 5000000;

	mtk_pe_parse_dt(pe, &pdev->dev);


	return 0;

}

static int mtk_pe_remove(struct platform_device *dev)
{
	return 0;
}

static void mtk_pe_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id mtk_pe_of_match[] = {
	{.compatible = "mediatek,charger,pe",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_pe_of_match);

struct platform_device pe_device = {
	.name = "pe",
	.id = -1,
};

static struct platform_driver pe_driver = {
	.probe = mtk_pe_probe,
	.remove = mtk_pe_remove,
	.shutdown = mtk_pe_shutdown,
	.driver = {
		   .name = "pe",
		   .of_match_table = mtk_pe_of_match,
	},
};

static int __init mtk_pe_init(void)
{
	return platform_driver_register(&pe_driver);
}
late_initcall(mtk_pe_init);

static void __exit mtk_pe_exit(void)
{
	platform_driver_unregister(&pe_driver);
}
module_exit(mtk_pe_exit);


MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Pump Express algorithm Driver");
MODULE_LICENSE("GPL");



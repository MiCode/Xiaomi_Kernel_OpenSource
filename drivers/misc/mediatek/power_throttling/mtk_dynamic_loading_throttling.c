// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/iio/consumer.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/sort.h>
#include <linux/suspend.h>
#include "mtk_low_battery_throttling.h"
#include "mtk_dynamic_loading_throttling.h"

#define POWER_UVLO_VOLT_LEVEL		2600
#define IMAX_MAX_VALUE			5500
#define DLPT_NOTIFY_FAST_UISOC		30
#define	DLPT_VOLT_MIN			3100

struct reg_t {
	unsigned int addr;
	unsigned int mask;
	unsigned int shift;
};

struct dlpt_regs_t {
	struct reg_t rgs_chrdet;
	struct reg_t uvlo_reg;
};

struct dlpt_regs_t mt6357_dlpt_regs = {
	.rgs_chrdet = {
		MT6357_RGS_CHRDET_ADDR,
		MT6357_RGS_CHRDET_MASK << MT6357_RGS_CHRDET_SHIFT,
		MT6357_RGS_CHRDET_SHIFT
	},
	.uvlo_reg = {
		MT6357_RG_UVLO_VTHL_ADDR,
		MT6357_RG_UVLO_VTHL_MASK << MT6357_RG_UVLO_VTHL_SHIFT,
		MT6357_RG_UVLO_VTHL_SHIFT
	},
};

struct dlpt_priv {
	struct regmap *regmap;
	enum LOW_BATTERY_LEVEL_TAG lbat_level;
	const struct dlpt_regs_t *regs;
	/* dlpt notify */
	struct mutex notify_lock;
	struct wakeup_source *notify_ws;
	struct timer_list notify_timer;
	struct wait_queue_head notify_waiter;
	struct task_struct *notify_thread;
	bool notify_flag;
	/* Imix */
	int imix;
	int imix_r;
	/* others */
	int is_power_path_supported;
	int is_isense_supported;
	struct iio_channel *chan_ptim;
	struct iio_channel *chan_imix_r;
	struct iio_channel *chan_zcv;
};

struct dlpt_callback_table {
	void (*dlptcb)(int value);
};

static struct dlpt_priv dlpt = {
	.notify_lock	=  __MUTEX_INITIALIZER(dlpt.notify_lock),
	.notify_waiter	= __WAIT_QUEUE_HEAD_INITIALIZER(dlpt.notify_waiter),
};
#define DLPTCB_MAX_NUM 16
static struct dlpt_callback_table dlptcb_tb[DLPTCB_MAX_NUM] = { {0} };

/*
 * Get ZCV/imix_r Auxadc function
 */
static void update_dlpt_imix_r(void)
{
	if (!PTR_ERR_OR_ZERO(dlpt.chan_imix_r))
		iio_read_channel_raw(dlpt.chan_imix_r, &dlpt.imix_r);
	pr_info("[dlpt] imix_r=%d\n", dlpt.imix_r);
}

static int dlpt_adc_chan_init(struct platform_device *pdev)
{
	int ret = 0;

	dlpt.chan_ptim = devm_iio_channel_get(&pdev->dev, "pmic_ptim");
	ret = PTR_ERR_OR_ZERO(dlpt.chan_ptim);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			pr_notice("%s ptim fail, ret=%d\n", __func__, ret);
		return ret;
	}

	dlpt.chan_imix_r = devm_iio_channel_get(&pdev->dev, "pmic_imix_r");
	ret = PTR_ERR_OR_ZERO(dlpt.chan_imix_r);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			pr_notice("%s imix_r fail, ret=%d\n", __func__, ret);
		return ret;
	}
	update_dlpt_imix_r();

	/* direct point to BATADC or ISENSE phandle in DTS */
	if (dlpt.is_isense_supported && dlpt.is_power_path_supported)
		dlpt.chan_zcv = devm_iio_channel_get(&pdev->dev, "pmic_isense");
	else
		dlpt.chan_zcv = devm_iio_channel_get(&pdev->dev, "pmic_batadc");
	ret = PTR_ERR_OR_ZERO(dlpt.chan_zcv);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			pr_notice("%s pmic_zcv fail, ret=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

/*
 * DLPT notify function
 */
void register_dlpt_notify(dlpt_callback dlpt_cb,
			  enum DLPT_PRIO_TAG prio_val)
{
	if (prio_val >= DLPTCB_MAX_NUM || prio_val < 0) {
		pr_notice("[%s] prio_val=%d, out of boundary\n",
			  __func__, prio_val);
		return;
	}
	dlptcb_tb[prio_val].dlptcb = dlpt_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (dlpt.imix != 0) {
		pr_notice("[%s] happen\n", __func__);
		if (dlpt_cb != NULL)
			dlpt_cb(dlpt.imix);
	}
}

static void exec_dlpt_callback(int dlpt_val)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(dlptcb_tb); i++) {
		if (dlptcb_tb[i].dlptcb)
			dlptcb_tb[i].dlptcb(dlpt_val);
	}
	pr_debug("[%s] dlpt imix_val=%d\n", __func__, dlpt_val);
}

static int dlpt_get_rgs_chrdet(void)
{
	int ret = 0;
	unsigned int regval = 0;

	ret = regmap_read(dlpt.regmap, dlpt.regs->rgs_chrdet.addr, &regval);
	if (ret != 0) {
		pr_info("%s Failed to get chrdet status\n", __func__);
		return ret;
	}
	if (regval & dlpt.regs->rgs_chrdet.mask)
		ret = 1;

	return ret;
}

static int dlpt_check_power_off(void)
{
	int ret = 0;
	static int dlpt_power_off_cnt;

	if (dlpt.lbat_level == LOW_BATTERY_LEVEL_2) {
		if (dlpt_power_off_cnt == 0)
			ret = 0; /* 1st time get VBAT < 3.1V, record it */
		else
			ret = 1; /* 2nd time get VBAT < 3.1V */
		dlpt_power_off_cnt++;
		pr_info("[%s] %d ret:%d\n", __func__, dlpt_power_off_cnt, ret);
	} else
		dlpt_power_off_cnt = 0;

	if (dlpt_power_off_cnt >= 4 &&
	    mutex_trylock(&system_transition_mutex)) {
		kernel_restart("DLPT reboot system");
		mutex_unlock(&system_transition_mutex);
	}
	return ret;
}

static void dlpt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level)
{
	dlpt.lbat_level = level;
}

static struct power_supply *get_mtk_gauge_psy(void)
{
	static struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	if (!psy) {
		psy = power_supply_get_by_name("mtk-gauge");
		if (!psy) {
			pr_info("%s psy is not rdy\n", __func__);
			return NULL;
		}
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&prop);
	if (!ret && prop.intval == 0)
		return psy; /* gauge enabled */
	return NULL;
}

static void dlpt_set_shutdown_condition(void)
{
	struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	psy = get_mtk_gauge_psy();
	/* gauge disabled */
	if (!psy)
		return;

	prop.intval = 1;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ENERGY_EMPTY,
					&prop);
	if (ret)
		pr_info("%s fail\n", __func__);
}

static int dlpt_get_uisoc(void)
{
	struct power_supply *psy;
	union power_supply_propval prop;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -ENODEV;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY,
					&prop);
	if (ret || prop.intval < 0)
		return -EINVAL;
	else
		return prop.intval;
}

static int cmpint(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

static int get_dlpt_imix_charging(void)
{
	int zcv_val = 0;
	int vsys_min_1_val = DLPT_VOLT_MIN;
	int imix = 0;
	int ret = 0;

	ret = iio_read_channel_processed(dlpt.chan_zcv, &zcv_val);
	if (ret < 0) {
		pr_notice("[%s] iio_read_channel_processed error\n",
			  __func__);
		return 0;
	}
	imix = (zcv_val - vsys_min_1_val) * 1000 / dlpt.imix_r * 9 / 10;
	pr_debug("[%s] %d %d %d %d\n", __func__,
		 imix, zcv_val, vsys_min_1_val, dlpt.imix_r);

	return imix;
}

static int get_dlpt_imix(void)
{
	int volt[5], curr[5], volt_avg = 0, curr_avg = 0;
	int vbat = 0, ibat = 0, imix = 0;
	int lbatInt1 = DLPT_VOLT_MIN * 10;
	int i = 0, ret = 0;

	for (i = 0; i < 5; i++) {
		ret = iio_read_channel_attribute(dlpt.chan_ptim, &vbat, &ibat,
						 IIO_CHAN_INFO_PROCESSED);
		if (ret < 0) {
			pr_notice("[%s] iio_read_channel_processed error\n",
				  __func__);
			return 0;
		}
		volt[i] = vbat;
		curr[i] = ibat;
	}

	sort(volt, 5, sizeof(int), cmpint, NULL);
	sort(curr, 5, sizeof(int), cmpint, NULL);
	volt_avg = volt[1] + volt[2] + volt[3];
	curr_avg = curr[1] + curr[2] + curr[3];
	volt_avg = volt_avg / 3;
	curr_avg = curr_avg / 3;

	imix = (curr_avg + (volt_avg - lbatInt1) * 1000 / dlpt.imix_r) / 10;

	pr_info("[%s] %d,%d,%d,%d\n"
		, __func__, volt_avg, curr_avg, dlpt.imix_r, imix);

	if (imix < 0) {
		pr_notice("[dlpt] imix= %d < 0\n", imix);
		return dlpt.imix;
	}
	return imix;
}

static int dlpt_notify_handler(void *unused)
{
	unsigned long dlpt_notify_interval;
	int pre_ui_soc = 0;
	int cur_ui_soc = 0;

	pre_ui_soc = dlpt_get_uisoc();
	cur_ui_soc = pre_ui_soc;

	do {
		if (pre_ui_soc > DLPT_NOTIFY_FAST_UISOC)
			dlpt_notify_interval = HZ * 20;
		else
			dlpt_notify_interval = HZ * 10;

		wait_event_interruptible(dlpt.notify_waiter,
					 (dlpt.notify_flag == true));
		__pm_stay_awake(dlpt.notify_ws);
		mutex_lock(&dlpt.notify_lock);

		cur_ui_soc = dlpt_get_uisoc();

		if (dlpt.imix_r == 0)
			pr_info("[DLPT] imix_r==0, skip\n");
		else if (!get_mtk_gauge_psy())
			pr_info("[DLPT] gauge disabled, skip\n");
		else {
			if (dlpt_get_rgs_chrdet())
				dlpt.imix = get_dlpt_imix_charging();
			else
				dlpt.imix = get_dlpt_imix();

			if (dlpt.imix > IMAX_MAX_VALUE)
				dlpt.imix = IMAX_MAX_VALUE;
			exec_dlpt_callback(dlpt.imix);

			pr_info("[DLPT_final] %d,%d,%d,%d\n"
				, dlpt.imix, pre_ui_soc
				, cur_ui_soc, IMAX_MAX_VALUE);
		}
		pre_ui_soc = cur_ui_soc;
		dlpt.notify_flag = false;

		/* Check low battery volt < 3.1V */
		if (dlpt_check_power_off()) {
			/* notify battery driver to power off by SOC=0 */
			dlpt_set_shutdown_condition();
			pr_info("[DLPT] notify battery SOC=0 to power off.\n");
		}
		mutex_unlock(&dlpt.notify_lock);
		__pm_relax(dlpt.notify_ws);

		mod_timer(&dlpt.notify_timer, jiffies + dlpt_notify_interval);

	} while (!kthread_should_stop());

	return 0;
}

static void dlpt_timer_func(struct timer_list *t)
{
	dlpt.notify_flag = true;
	wake_up_interruptible(&dlpt.notify_waiter);
}

static void dlpt_notify_init(void)
{
	unsigned long dlpt_notify_interval;

	dlpt_notify_interval = HZ * 30;
	timer_setup(&dlpt.notify_timer, dlpt_timer_func, TIMER_DEFERRABLE);
	mod_timer(&dlpt.notify_timer, jiffies + dlpt_notify_interval);

	dlpt.notify_ws = wakeup_source_register(NULL,
						"dlpt_notify_ws wakelock");
	if (!dlpt.notify_ws)
		pr_notice("dlpt_notify_ws wakeup source fail\n");

	dlpt.notify_thread = kthread_run(dlpt_notify_handler, 0,
					 "dlpt_notify_thread");
	if (IS_ERR(dlpt.notify_thread))
		pr_notice("Failed to create dlpt_notify_thread\n");

	register_low_battery_notify(&dlpt_low_battery_cb,
				    LOW_BATTERY_PRIO_DLPT);
}

static void pmic_uvlo_init(int uvlo_level)
{
	int val;

	/*re-init UVLO volt */
	switch (uvlo_level) {
	case 2500:
		val = 0;
		break;
	case 2550:
		val = 1;
		break;
	case 2600:
		val = 2;
		break;
	case 2650:
		val = 3;
		break;
	case 2700:
		val = 4;
		break;
	case 2750:
		val = 5;
		break;
	case 2800:
		val = 6;
		break;
	case 2850:
		val = 7;
		break;
	case 2900:
		val = 8;
		break;
	default:
		val = 0;
		pr_notice("[dlpt] Invalid uvlo_level (%d)\n", uvlo_level);
		break;
	}
	regmap_update_bits(dlpt.regmap, dlpt.regs->uvlo_reg.addr,
			   dlpt.regs->uvlo_reg.mask,
			   val << dlpt.regs->uvlo_reg.shift);
	pr_info("[dlpt] UVLO_VOLT_LEVEL = %d, RG_UVLO_VTHL = 0x%x\n"
		, uvlo_level, val);
}

static void dlpt_parse_dt(struct platform_device *pdev)
{
	struct device_node *np;
	int uvlo_level;
	int ret;

	/* get power_path_support */
	np = of_parse_phandle(pdev->dev.of_node, "mediatek,charger", 0);
	if (!np)
		dev_notice(&pdev->dev, "get charger node fail\n");
	else
		dlpt.is_power_path_supported =
			of_property_read_bool(np, "power_path_support");

	/* get dlpt device node */
	np = of_find_node_by_name(pdev->dev.parent->of_node,
				  "mtk_dynamic_loading_throttling");
	if (!np)
		dev_notice(&pdev->dev, "get dlpt node fail\n");
	else {
		/* get isense support */
		dlpt.is_isense_supported =
			of_property_read_bool(np, "isense_support");
		/* get uvlo-level */
		ret = of_property_read_u32(np, "uvlo-level", &uvlo_level);
		if (ret)
			uvlo_level = POWER_UVLO_VOLT_LEVEL;
		pmic_uvlo_init(uvlo_level);
	}
	dev_notice(&pdev->dev, "power_path_support:%d isense_support:%d\n"
		   , dlpt.is_power_path_supported, dlpt.is_isense_supported);
}

static int dlpt_probe(struct platform_device *pdev)
{
	struct mt6397_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int ret;

	dlpt.regmap = chip->regmap;
	if (!dlpt.regmap) {
		dev_notice(&pdev->dev, "%s: invalid regmap.\n", __func__);
		return -EINVAL;
	}
	dlpt.regs = of_device_get_match_data(&pdev->dev);
	dlpt_parse_dt(pdev);

	ret = dlpt_adc_chan_init(pdev);
	if (ret)
		return ret;
	dlpt_notify_init();

	return 0;
}

static int __maybe_unused dlpt_resume(struct device *d)
{
	update_dlpt_imix_r();
	return 0;
}

static SIMPLE_DEV_PM_OPS(dlpt_pm_ops,
			 NULL,
			 dlpt_resume);

static const struct of_device_id dynamic_loading_throttling_of_match[] = {
	{
		.compatible = "mediatek,mt6357-dynamic_loading_throttling",
		.data = &mt6357_dlpt_regs,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, dynamic_loading_throttling_of_match);

static struct platform_driver dynamic_loading_throttling_driver = {
	.driver = {
		.name = "mtk_dynamic_loading_throttling",
		.of_match_table = dynamic_loading_throttling_of_match,
		.pm = &dlpt_pm_ops,
	},
	.probe = dlpt_probe,
};
module_platform_driver(dynamic_loading_throttling_driver);

MODULE_AUTHOR("Wen Su <Wen.Su@mediatek.com>");
MODULE_DESCRIPTION("MTK dynamic loading throttling driver");
MODULE_LICENSE("GPL");

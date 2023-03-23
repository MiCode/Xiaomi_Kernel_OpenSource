// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "mtk_low_battery_throttling.h"
#include "pmic_lbat_service.h"
#include "pmic_lvsys_notify.h"

#define LBCB_MAX_NUM 16
#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100
#define POWER_INT3_VOLT 2700


struct lbat_intr_tbl {
	unsigned int volt_thd;
	unsigned int lt_en;
	unsigned int lt_lv;
	unsigned int ht_en;
	unsigned int ht_lv;
};

struct low_bat_thl_priv {
	unsigned int hv_thd_volt;
	unsigned int lv1_thd_volt;
	unsigned int lv2_thd_volt;
	unsigned int *thd_volts;
	int thd_volts_size;
	int low_bat_thl_level;
	int low_bat_thl_pmic_lv;
	int low_bat_thl_stop;
	struct lbat_user *lbat_pt;
	struct lbat_intr_tbl *lbat_intr_info;
};

struct low_battery_callback_table {
	void (*lbcb)(enum LOW_BATTERY_LEVEL_TAG);
};

static int isThreeLevel;
static unsigned int *volt_l_thd, *volt_h_thd;
static struct low_bat_thl_priv *low_bat_thl_data;
static struct low_battery_callback_table lbcb_tb[LBCB_MAX_NUM] = { {0} };

static int rearrange_volt(struct lbat_intr_tbl *intr_info, unsigned int *volt_l,
	unsigned int *volt_h, unsigned int num)
{

	unsigned int idx_l = 0, idx_h = 0, idx_t = 0, i;
	unsigned int volt_l_next, volt_h_next;

	for (i = 0; i < num - 1; i++) {
		if (volt_l[i] < volt_l[i+1] || volt_h[i] < volt_h[i+1]) {
			pr_notice("[%s] i=%d volt_l(%d, %d) volt_h(%d, %d) error\n",
				__func__, volt_l[i], volt_l[i+1], volt_h[i], volt_h[i+1]);
			return -EINVAL;
		}
	}

	for (i = 0; i < num * 2; i++) {
		volt_l_next = (idx_l < num) ? volt_l[idx_l] : 0;
		volt_h_next = (idx_h < num) ? volt_h[idx_h] : 0;

		if (volt_l_next > volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt_thd = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = idx_l + 1;
			idx_l++;
			idx_t++;
		} else if (volt_l_next == volt_h_next && volt_l_next > 0) {
			intr_info[idx_t].volt_thd = volt_l_next;
			intr_info[idx_t].lt_en = 1;
			intr_info[idx_t].lt_lv = idx_l + 1;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = idx_h;
			idx_l++;
			idx_h++;
			idx_t++;
		} else if (volt_h_next > 0) {
			intr_info[idx_t].volt_thd = volt_h_next;
			intr_info[idx_t].ht_en = 1;
			intr_info[idx_t].ht_lv = idx_h;
			idx_h++;
			idx_t++;
		} else
			break;

	}

	for (i = 0; i < idx_t; i++) {
		pr_info("[%s] intr_info[%d] = (%d, trig l[%d %d] h[%d %d])\n",
				__func__, i, intr_info[i].volt_thd, intr_info[i].lt_en,
				intr_info[i].lt_lv, intr_info[i].ht_en, intr_info[i].ht_lv);
	}

	return idx_t;
}

int register_low_battery_notify(low_battery_callback lb_cb,
				enum LOW_BATTERY_PRIO_TAG prio_val)
{
	int ret = 2;

	if (isThreeLevel)
		ret = 3;

	if (prio_val >= LBCB_MAX_NUM) {
		pr_notice("[%s] prio_val=%d, out of boundary\n",
			  __func__, prio_val);
		return -EINVAL;
	}
	lbcb_tb[prio_val].lbcb = lb_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (!low_bat_thl_data) {
		pr_info("[%s] low_bat_thl_data not allocate\n", __func__);
		return ret;
	}

	if (low_bat_thl_data->low_bat_thl_level && lbcb_tb[prio_val].lbcb) {
		lbcb_tb[prio_val].lbcb(low_bat_thl_data->low_bat_thl_level);
		pr_info("[%s] notify lv=%d\n", __func__, low_bat_thl_data->low_bat_thl_level);
	}

	return ret;
}
EXPORT_SYMBOL(register_low_battery_notify);

void exec_throttle(unsigned int level)
{
	int i;

	pr_info("[%s] throttle level = %d\n", __func__, level);
	for (i = 0; i < ARRAY_SIZE(lbcb_tb); i++) {
		if (lbcb_tb[i].lbcb)
			lbcb_tb[i].lbcb(low_bat_thl_data->low_bat_thl_level);
	}
}

static unsigned int thd_to_level(unsigned int thd)
{
	unsigned int i, level = 0;
	struct lbat_intr_tbl *info;

	for (i = 0; i < low_bat_thl_data->thd_volts_size; i++) {
		info = &(low_bat_thl_data->lbat_intr_info[i]);
		if (thd == low_bat_thl_data->thd_volts[i]) {
			if (info->ht_en == 1)
				level = info->ht_lv;
			else if (info->lt_en == 1)
				level = info->lt_lv;
			break;
		}
	}

	pr_info("[%s] level = %d\n", __func__, level);
	return level;
}

void exec_low_battery_throttle(unsigned int level)
{
	int cur_lv = 0;

	if (!low_bat_thl_data)
		return;
	if (low_bat_thl_data->low_bat_thl_stop == 1) {
		pr_info("[%s] low_bat_thl_stop=%d\n",
			__func__, low_bat_thl_data->low_bat_thl_stop);
		return;
	}
	if (low_bat_thl_data->thd_volts_size > 0) {
		cur_lv = low_bat_thl_data->low_bat_thl_level;
		if (cur_lv == level) {
			pr_info("[%s] same throttle level %d\n", __func__, level);
			return;
		}
		low_bat_thl_data->low_bat_thl_level = level;
		exec_throttle(low_bat_thl_data->low_bat_thl_level);
		pr_info("[%s] cur_lv=%d new_l=%d\n",
			__func__, cur_lv, low_bat_thl_data->low_bat_thl_level);
	} else {
		low_bat_thl_data->low_bat_thl_level = level;
		exec_throttle(low_bat_thl_data->low_bat_thl_level);
	}
}

void low_battery_vbat_callback(unsigned int thd)
{
	unsigned int level;

	pr_notice("[%s] [vbat] thd = %d\n", __func__, thd);

	if (isThreeLevel) {
		level = thd_to_level(thd);
		exec_low_battery_throttle(level);
	} else {
		if (thd == low_bat_thl_data->hv_thd_volt)
			low_bat_thl_data->low_bat_thl_level = LOW_BATTERY_LEVEL_0;
		else if (thd == low_bat_thl_data->lv1_thd_volt)
			low_bat_thl_data->low_bat_thl_level = LOW_BATTERY_LEVEL_1;
		else if (thd == low_bat_thl_data->lv2_thd_volt)
			low_bat_thl_data->low_bat_thl_level = LOW_BATTERY_LEVEL_2;
		else {
			pr_notice("[%s] wrong threshold=%d\n", __func__, thd);
			return;
		}

		exec_low_battery_throttle(low_bat_thl_data->low_bat_thl_level);
	}
}

static int low_battery_vsys_callback(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	pr_notice("[%s] [lvsys] thd = %d\n", __func__, event);
	if (event == LVSYS_F_3400)
		exec_low_battery_throttle(LOW_BATTERY_LEVEL_2);
	else if (event == LVSYS_R_3500)
		exec_low_battery_throttle(LOW_BATTERY_LEVEL_0);

	return NOTIFY_DONE;
}

/*****************************************************************************
 * low battery protect ktf
 ******************************************************************************/
void exec_low_battery_callback(unsigned int thd)
{
	pr_notice("[%s] for ktf testing\n", __func__, thd);
}

/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t low_battery_protect_ut_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "low_bat_thl_level=%d\n",
		low_bat_thl_data->low_bat_thl_level);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_level);
}

static ssize_t low_battery_protect_ut_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "Utest", 5))
		return -EINVAL;

	if (isThreeLevel) {
		if (val > LOW_BATTERY_LEVEL_3) {
			dev_info(dev, "wrong number (%d)\n", val);
			return size;
		}
	} else {
		if (val > LOW_BATTERY_LEVEL_2) {
			dev_info(dev, "wrong number (%d)\n", val);
			return size;
		}
	}

	low_bat_thl_data->low_bat_thl_level = val;
	dev_info(dev, "your input is %d\n", val);
	exec_throttle(val);
	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_ut);

/*****************************************************************************
 * low battery protect stop
 ******************************************************************************/
static ssize_t low_battery_protect_stop_show(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	dev_dbg(dev, "low_bat_thl_stop=%d\n",
		low_bat_thl_data->low_bat_thl_stop);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_stop);
}

static ssize_t low_battery_protect_stop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val = 0;
	char cmd[21];

	if (sscanf(buf, "%20s %u\n", cmd, &val) != 2) {
		dev_info(dev, "parameter number not correct\n");
		return -EINVAL;
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if ((val != 0) && (val != 1))
		val = 0;

	low_bat_thl_data->low_bat_thl_stop = val;
	dev_info(dev, "low_bat_thl_stop=%d\n",
		 low_bat_thl_data->low_bat_thl_stop);

	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_stop);

/*****************************************************************************
 * low battery protect level
 ******************************************************************************/
static ssize_t low_battery_protect_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_dbg(dev, "low_bat_thl_level=%d\n",
		low_bat_thl_data->low_bat_thl_level);
	return sprintf(buf, "%u\n", low_bat_thl_data->low_bat_thl_level);
}

static ssize_t low_battery_protect_level_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	dev_dbg(dev, "low_bat_thl_level = %d\n",
		low_bat_thl_data->low_bat_thl_level);
	return size;
}

static DEVICE_ATTR_RW(low_battery_protect_level);

static void dump_thd_volts(struct device *dev, unsigned int *thd_volts, unsigned int size)
{
	int i, r = 0;
	char str[128] = "";
	size_t len = sizeof(str) - 1;

	for (i = 0; i < size; i++) {
		r += snprintf(str + r, len - r, "%s%d mV", i ? ", " : "", thd_volts[i]);
		if (r >= len)
			return;
	}
	dev_notice(dev, "%s Done\n", str);
}

static struct notifier_block lbat_vsys_notifier = {
	.notifier_call = low_battery_vsys_callback,
};

static int check_duplicate(unsigned int *volt_thd)
{
	int i, j;

	for (i = 0; i < LOW_BATTERY_LEVEL_NUM - 1; i++) {
		for (j = i + 1; j < LOW_BATTERY_LEVEL_NUM - 1; j++) {
			if (volt_thd[i] == volt_thd[j]) {
				pr_notice("[%s] volt_thd duplicate = %d\n", __func__, volt_thd[i]);
				return -1;
			}
		}
	}
	return 0;
}

static int low_battery_throttling_probe(struct platform_device *pdev)
{
	int ret, i;
	struct low_bat_thl_priv *priv;
	struct device_node *np = pdev->dev.of_node;
	int vol_l_size, vol_h_size, vol_t_size;
	int lvsys_thd_enable, vbat_thd_enable;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, priv);

	vol_l_size = of_property_count_elems_of_size(np, "thd-volts-l", sizeof(u32));
	if (vol_l_size < 0) {
		dev_notice(&pdev->dev, "[%s] No thd-volts-l\n", __func__);
		vol_l_size = 0;
	}
	vol_h_size = of_property_count_elems_of_size(np, "thd-volts-h", sizeof(u32));
	if (vol_h_size < 0) {
		dev_notice(&pdev->dev, "[%s] No thd-volts-h\n", __func__);
		vol_h_size = 0;
	}

	if (vol_l_size > 0 && vol_h_size > 0)
		priv->thd_volts_size = vol_l_size + vol_h_size;

	if (priv->thd_volts_size > 0) {
		isThreeLevel = 1;

		ret = of_property_read_u32(np, "lvsys-thd-enable", &lvsys_thd_enable);
		if (ret) {
			dev_notice(&pdev->dev,
				"[%s] failed to get lvsys-thd-enable ret=%d\n", __func__, ret);
			lvsys_thd_enable = 0;
		}

		ret = of_property_read_u32(np, "vbat-thd-enable", &vbat_thd_enable);
		if (ret) {
			dev_notice(&pdev->dev,
				"[%s] failed to get vbat-thd-enable ret=%d\n", __func__, ret);
			vbat_thd_enable = 1;
		}

		if (lvsys_thd_enable == 1 && vbat_thd_enable == 1) {
			lvsys_thd_enable = 0;
			vbat_thd_enable = 1;
			dev_notice(&pdev->dev,
				"[%s] default only enable vbat throttle\n", __func__);
		}

		priv->lbat_intr_info = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
			sizeof(struct lbat_intr_tbl), GFP_KERNEL);

		if (!priv->lbat_intr_info)
			return -ENOMEM;

		volt_l_thd = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
						     sizeof(u32), GFP_KERNEL);
		volt_h_thd = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
						     sizeof(u32), GFP_KERNEL);

		ret = of_property_read_u32_array(np, "thd-volts-l", volt_l_thd,
						 vol_l_size);
		ret |= of_property_read_u32_array(np, "thd-volts-h", volt_h_thd,
						 vol_h_size);
		ret |= check_duplicate(volt_l_thd);
		ret |= check_duplicate(volt_h_thd);

		if (vol_l_size != vol_h_size)
			ret = -1;

		if (ret) {
			dev_notice(&pdev->dev,
				"[%s] failed to get correct thd-volt ret=%d\n", __func__, ret);
			priv->thd_volts_size = LOW_BATTERY_LEVEL_NUM;
			priv->thd_volts = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
						sizeof(u32), GFP_KERNEL);

			if (!priv->thd_volts)
				return -ENOMEM;

			priv->thd_volts[0] = POWER_INT0_VOLT;
			priv->thd_volts[1] = POWER_INT1_VOLT;
			priv->thd_volts[2] = POWER_INT2_VOLT;
			priv->thd_volts[3] = POWER_INT3_VOLT;
		} else {
			vol_t_size = rearrange_volt(priv->lbat_intr_info,
				volt_l_thd, volt_h_thd, vol_l_size);

			if (vol_t_size <= 0) {
				dev_notice(&pdev->dev, "[%s] Failed to rearrange_volt\n", __func__);
				return -ENODATA;
			}

			priv->thd_volts_size = vol_t_size;
			priv->thd_volts = devm_kmalloc_array(&pdev->dev, priv->thd_volts_size,
						sizeof(u32), GFP_KERNEL);
			if (!priv->thd_volts)
				return -ENOMEM;

			for (i = 0; i < vol_t_size; i++)
				priv->thd_volts[i] = priv->lbat_intr_info[i].volt_thd;
		}

		if (vbat_thd_enable)
			priv->lbat_pt = lbat_user_register_ext("power throttling", priv->thd_volts,
								priv->thd_volts_size,
								low_battery_vbat_callback);

		if (lvsys_thd_enable)
			lvsys_register_notifier(&lbat_vsys_notifier);

	} else {
		isThreeLevel = 0;
		ret = of_property_read_u32(np, "hv-thd-volt", &priv->hv_thd_volt);
		if (ret)
			priv->hv_thd_volt = POWER_INT0_VOLT;

		ret = of_property_read_u32(np, "lv1-thd-volt", &priv->lv1_thd_volt);
		if (ret)
			priv->lv1_thd_volt = POWER_INT1_VOLT;

		ret = of_property_read_u32(np, "lv2-thd-volt", &priv->lv2_thd_volt);
		if (ret)
			priv->lv2_thd_volt = POWER_INT2_VOLT;

		priv->lbat_pt = lbat_user_register("power throttling", priv->hv_thd_volt,
						   priv->lv1_thd_volt, priv->lv2_thd_volt,
						   low_battery_vbat_callback);
	}
	if (IS_ERR(priv->lbat_pt)) {
		ret = PTR_ERR(priv->lbat_pt);
		if (ret != -EPROBE_DEFER) {
			dev_notice(&pdev->dev,
				"[%s] error ret=%d\n", __func__, ret);
		}
		return ret;
	}
	if (priv->thd_volts_size > 0)
		dump_thd_volts(&pdev->dev, priv->thd_volts, priv->thd_volts_size);
	else {
		/* lbat_dump_reg(); */
		dev_notice(&pdev->dev, "%d mV, %d mV, %d mV Done\n",
			   priv->hv_thd_volt, priv->lv1_thd_volt, priv->lv2_thd_volt);
	}
	low_bat_thl_data = priv;
	ret = device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_ut);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_stop);
	ret |= device_create_file(&(pdev->dev),
		&dev_attr_low_battery_protect_level);
	if (ret)
		dev_notice(&pdev->dev, "create file error ret=%d\n", ret);

	return ret;
}

static const struct of_device_id low_bat_thl_of_match[] = {
	{ .compatible = "mediatek,low_battery_throttling", },
	{ },
};
MODULE_DEVICE_TABLE(of, low_bat_thl_of_match);

static struct platform_driver low_battery_throttling_driver = {
	.driver = {
		.name = "low_battery_throttling",
		.of_match_table = low_bat_thl_of_match,
	},
	.probe = low_battery_throttling_probe,
};
module_platform_driver(low_battery_throttling_driver);

MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK low battery throttling driver");
MODULE_LICENSE("GPL");

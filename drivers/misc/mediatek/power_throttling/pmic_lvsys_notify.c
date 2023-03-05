// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/export.h>
#include <linux/linear_range.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "pmic_lvsys_notify.h"

#define LVSYS_DBG	0

#define EVENT_LVSYS_F	0
#define EVENT_LVSYS_R	BIT(15)

#define MT6363_RG_LVSYS_INT_EN		0xa18
#define MT6363_RG_LVSYS_INT_VTHL	0xa8b
#define MT6363_RG_LVSYS_INT_VTHH	0xa8c

struct pmic_lvsys_info {
	u32 lvsys_int_en_reg;
	u32 lvsys_int_en_mask;
	u32 lvsys_int_fdb_sel_mask;
	u32 lvsys_int_rdb_sel_mask;
	u32 lvsys_int_vthl_reg;
	u32 lvsys_int_vthh_reg;
	const struct linear_range vthl_range;
	const struct linear_range vthh_range;
};

static const struct pmic_lvsys_info mt6363_lvsys_info = {
	.lvsys_int_en_reg = MT6363_RG_LVSYS_INT_EN,
	.lvsys_int_en_mask = 0x1,
	.lvsys_int_fdb_sel_mask = 0x6,
	.lvsys_int_rdb_sel_mask = 0x18,
	.lvsys_int_vthl_reg = MT6363_RG_LVSYS_INT_VTHL,
	.lvsys_int_vthh_reg = MT6363_RG_LVSYS_INT_VTHH,
	.vthl_range = {
		.min = 2500,
		.min_sel = 0,
		.max_sel = 9,
		.step = 100,
	},
	.vthh_range = {
		.min = 2600,
		.min_sel = 0,
		.max_sel = 9,
		.step = 100,
	},
};

struct pmic_lvsys_notify {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	const struct pmic_lvsys_info *info;
	unsigned int *thd_volts_l;
	unsigned int *thd_volts_h;
	unsigned int *cur_lv_ptr;
	unsigned int *cur_hv_ptr;
	int thd_volts_l_size;
	int thd_volts_h_size;
};

static BLOCKING_NOTIFIER_HEAD(lvsys_notifier_list);

/**
 *	lvsys_register_notifier - register a lvsys notifier
 *	@nb: notifier block to callback on events
 *
 *	Return: 0 on success, negative error code on failure.
 */
int lvsys_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lvsys_notifier_list, nb);
}
EXPORT_SYMBOL(lvsys_register_notifier);

/**
 *	lvsys_unregister_notifier - unregister a lvsys notifier
 *	@nb: notifier block to callback on events
 *
 *	Return: 0 on success, negative error code on failure.
 */
int lvsys_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lvsys_notifier_list, nb);
}
EXPORT_SYMBOL(lvsys_unregister_notifier);

struct vio18_ctrl_t {
	struct notifier_block nb;
	struct regmap *main_regmap;
	struct regmap *second_regmap;
	unsigned int main_switch;
	unsigned int second_switch;
};

static int vio18_switch_handler(struct notifier_block *nb, unsigned long event, void *v)
{
	int ret;
	struct vio18_ctrl_t *vio18_ctrl = container_of(nb, struct vio18_ctrl_t, nb);

	if (event == LVSYS_F_3400) {
		ret = regmap_write(vio18_ctrl->main_regmap, vio18_ctrl->main_switch, 0);
		if (ret)
			pr_notice("Failed to set main vio18_switch, ret=%d\n", ret);

		ret = regmap_write(vio18_ctrl->second_regmap, vio18_ctrl->second_switch, 0);
		if (ret)
			pr_notice("Failed to set second vio18_switch, ret=%d\n", ret);
	} else if (event == LVSYS_R_3500) {
		ret = regmap_write(vio18_ctrl->main_regmap, vio18_ctrl->main_switch, 1);
		if (ret)
			pr_notice("Failed to set main vio18_switch, ret=%d\n", ret);

		ret = regmap_write(vio18_ctrl->second_regmap, vio18_ctrl->second_switch, 1);
		if (ret)
			pr_notice("Failed to set second vio18_switch, ret=%d\n", ret);
	}
	return 0;
}

static struct regmap *vio18_switch_get_regmap(const char *name)
{
	struct device_node *np;
	struct platform_device *pdev;

	np = of_find_node_by_name(NULL, name);
	if (!np)
		return NULL;

	pdev = of_find_device_by_node(np->child);
	if (!pdev)
		return NULL;

	return dev_get_regmap(pdev->dev.parent, NULL);
}

static int vio18_switch_init(struct device *dev, struct regmap *main_regmap)
{
	int ret = 0;
	unsigned int val_arr[2] = {0};
	struct vio18_ctrl_t *vio18_ctrl;

	vio18_ctrl = devm_kzalloc(dev, sizeof(*vio18_ctrl), GFP_KERNEL);
	if (!vio18_ctrl)
		return -ENOMEM;

	vio18_ctrl->main_regmap = main_regmap;
	vio18_ctrl->second_regmap = vio18_switch_get_regmap("second_pmic");
	if (!vio18_ctrl->second_regmap)
		return -EINVAL;

	ret = of_property_read_u32_array(dev->of_node, "vio18-switch-reg", val_arr, 2);
	if (ret)
		return -EINVAL;

	vio18_ctrl->main_switch = val_arr[0];
	vio18_ctrl->second_switch = val_arr[1];
	vio18_ctrl->nb.notifier_call = vio18_switch_handler;

	return lvsys_register_notifier(&vio18_ctrl->nb);
}


static void enable_lvsys_int(struct pmic_lvsys_notify *lvsys_notify, bool en)
{
	int ret;
	unsigned int val;
	const struct pmic_lvsys_info *info = lvsys_notify->info;

	val = en ? info->lvsys_int_en_mask : 0;
	ret = regmap_update_bits(lvsys_notify->regmap, info->lvsys_int_en_reg,
				 info->lvsys_int_en_mask, val);
	if (ret)
		dev_notice(lvsys_notify->dev, "Failed to enable LVSYS_INT, ret=%d\n", ret);
}

static int lvsys_vth_get_selector_high(const struct linear_range *r,
				       unsigned int val, unsigned int *selector)
{
	if ((r->min + (r->max_sel - r->min_sel) * r->step) < val)
		return -EINVAL;

	if (r->min > val) {
		*selector = r->min_sel;
		return 0;
	}
	if (r->step == 0)
		*selector = r->max_sel;
	else
		*selector = DIV_ROUND_UP(val - r->min, r->step) + r->min_sel;

	return 0;
}

static int lvsys_vth_get_selector_low(const struct linear_range *r,
				      unsigned int val, unsigned int *selector)
{
	if (r->min > val)
		return -EINVAL;

	if ((r->min + (r->max_sel - r->min_sel) * r->step) < val) {
		*selector = r->max_sel;
		return 0;
	}
	if (r->step == 0)
		*selector = r->min_sel;
	else
		*selector = (val - r->min) / r->step + r->min_sel;

	return 0;
}

static void update_lvsys_vth(struct pmic_lvsys_notify *lvsys_notify)
{
	int ret;
	unsigned int *last_lv = lvsys_notify->thd_volts_l + lvsys_notify->thd_volts_l_size - 1;
	unsigned int *first_hv = lvsys_notify->thd_volts_h;
	unsigned int vth_sel = 0;
	const struct pmic_lvsys_info *info = lvsys_notify->info;

	if (lvsys_notify->cur_lv_ptr && lvsys_notify->cur_lv_ptr <= last_lv) {
		ret = lvsys_vth_get_selector_high(&info->vthl_range, *(lvsys_notify->cur_lv_ptr),
						  &vth_sel);
		if (ret)
			dev_notice(lvsys_notify->dev, "Failed to get selector high(%d)\n", ret);
#if LVSYS_DBG
		dev_info(lvsys_notify->dev, "set INT_VTHL=%d(%d)\n",
			 vth_sel, *(lvsys_notify->cur_lv_ptr));
#endif
		ret = regmap_write(lvsys_notify->regmap, info->lvsys_int_vthl_reg, vth_sel);
		if (ret)
			dev_notice(lvsys_notify->dev, "Failed to set INT_VTHL=%d, ret=%d\n",
				   vth_sel, ret);
	} else
		lvsys_notify->cur_lv_ptr = NULL;
	if (lvsys_notify->cur_hv_ptr && lvsys_notify->cur_hv_ptr >= first_hv) {
		ret = lvsys_vth_get_selector_low(&info->vthh_range, *(lvsys_notify->cur_hv_ptr),
						 &vth_sel);
		if (ret)
			dev_notice(lvsys_notify->dev, "Failed to get selector low(%d)\n", ret);
#if LVSYS_DBG
		dev_info(lvsys_notify->dev, "set INT_VTHH=%d(%d)\n",
			 vth_sel, *(lvsys_notify->cur_hv_ptr));
#endif
		ret = regmap_write(lvsys_notify->regmap, info->lvsys_int_vthh_reg, vth_sel);
		if (ret)
			dev_notice(lvsys_notify->dev, "Failed to set INT_VTHH=%d, ret=%d\n",
				   vth_sel, ret);
	} else
		lvsys_notify->cur_hv_ptr = NULL;
}

static unsigned int *get_next_lv_ptr(struct pmic_lvsys_notify *lvsys_notify)
{
	int thd_volts_size = lvsys_notify->thd_volts_l_size;
	unsigned int *next_lv_ptr = lvsys_notify->thd_volts_l;

	for (; next_lv_ptr < lvsys_notify->thd_volts_l + thd_volts_size; next_lv_ptr++) {
		if (*next_lv_ptr < *(lvsys_notify->cur_hv_ptr))
			break;
	}
	if (next_lv_ptr == lvsys_notify->thd_volts_l + thd_volts_size)
		return NULL; /* not found */
	return next_lv_ptr;
}

static unsigned int *get_next_hv_ptr(struct pmic_lvsys_notify *lvsys_notify)
{
	int thd_volts_size = lvsys_notify->thd_volts_h_size;
	unsigned int *next_hv_ptr = lvsys_notify->thd_volts_h + thd_volts_size - 1;

	for (; next_hv_ptr >= lvsys_notify->thd_volts_h; next_hv_ptr--) {
		if (*next_hv_ptr > *(lvsys_notify->cur_lv_ptr))
			break;
	}
	if (next_hv_ptr == lvsys_notify->thd_volts_h - 1)
		return NULL; /* not found */
	return next_hv_ptr;
}

static irqreturn_t lvsys_f_int_handler(int irq, void *data)
{
	struct pmic_lvsys_notify *lvsys_notify = (struct pmic_lvsys_notify *)data;
	unsigned int event;

	if (!lvsys_notify)
		return IRQ_HANDLED;

	mutex_lock(&lvsys_notify->lock);
	if (!lvsys_notify->cur_lv_ptr) {
		mutex_unlock(&lvsys_notify->lock);
		return IRQ_HANDLED;
	}
	event = EVENT_LVSYS_F | *(lvsys_notify->cur_lv_ptr);
	dev_notice(lvsys_notify->dev, "event: falling %d\n", *(lvsys_notify->cur_lv_ptr));
	blocking_notifier_call_chain(&lvsys_notifier_list, event, NULL);

	lvsys_notify->cur_hv_ptr = get_next_hv_ptr(lvsys_notify);
	lvsys_notify->cur_lv_ptr++;
	update_lvsys_vth(lvsys_notify);
	mutex_unlock(&lvsys_notify->lock);

	return IRQ_HANDLED;
}

static irqreturn_t lvsys_r_int_handler(int irq, void *data)
{
	struct pmic_lvsys_notify *lvsys_notify = (struct pmic_lvsys_notify *)data;
	unsigned int event;

	if (!lvsys_notify)
		return IRQ_HANDLED;

	mutex_lock(&lvsys_notify->lock);
	if (!lvsys_notify->cur_hv_ptr) {
		mutex_unlock(&lvsys_notify->lock);
		return IRQ_HANDLED;
	}
	event = EVENT_LVSYS_R | *(lvsys_notify->cur_hv_ptr);
	dev_notice(lvsys_notify->dev, "event: rising %d\n", *(lvsys_notify->cur_hv_ptr));
	blocking_notifier_call_chain(&lvsys_notifier_list, event, NULL);

	lvsys_notify->cur_lv_ptr = get_next_lv_ptr(lvsys_notify);
	lvsys_notify->cur_hv_ptr--;
	update_lvsys_vth(lvsys_notify);
	mutex_unlock(&lvsys_notify->lock);

	return IRQ_HANDLED;
}

static int pmic_lvsys_parse_dt(struct pmic_lvsys_notify *lvsys_notify, struct device_node *np)
{
	int ret;
	unsigned int deb_sel = 0;
	const struct pmic_lvsys_info *info = lvsys_notify->info;

	lvsys_notify->thd_volts_l_size =
		of_property_count_elems_of_size(np, "thd-volts-l", sizeof(u32));
	if (lvsys_notify->thd_volts_l_size <= 0)
		return -EINVAL;

	lvsys_notify->thd_volts_l = devm_kmalloc_array(lvsys_notify->dev,
						       lvsys_notify->thd_volts_l_size,
						       sizeof(u32), GFP_KERNEL);
	if (!lvsys_notify->thd_volts_l)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "thd-volts-l", lvsys_notify->thd_volts_l,
					 lvsys_notify->thd_volts_l_size);

	lvsys_notify->thd_volts_h_size =
		of_property_count_elems_of_size(np, "thd-volts-h", sizeof(u32));
	if (lvsys_notify->thd_volts_h_size <= 0)
		return -EINVAL;

	lvsys_notify->thd_volts_h = devm_kmalloc_array(lvsys_notify->dev,
						       lvsys_notify->thd_volts_h_size,
						       sizeof(u32), GFP_KERNEL);
	if (!lvsys_notify->thd_volts_h)
		return -ENOMEM;

	ret |= of_property_read_u32_array(np, "thd-volts-h", lvsys_notify->thd_volts_h,
					 lvsys_notify->thd_volts_h_size);
	if (ret)
		return ret;

	lvsys_notify->cur_lv_ptr = lvsys_notify->thd_volts_l;
	lvsys_notify->cur_hv_ptr = get_next_hv_ptr(lvsys_notify);
	update_lvsys_vth(lvsys_notify);

	ret = of_property_read_u32(np, "lv-deb-sel", &deb_sel);
	if (ret)
		deb_sel = 0;
	else
		deb_sel <<= __ffs(info->lvsys_int_fdb_sel_mask);
	ret = regmap_update_bits(lvsys_notify->regmap, info->lvsys_int_en_reg,
				 info->lvsys_int_fdb_sel_mask, deb_sel);
	if (ret)
		dev_notice(lvsys_notify->dev, "Failed to set LVSYS_INT_FDB_SEL, ret=%d\n", ret);
	ret = of_property_read_u32(np, "hv-deb-sel", &deb_sel);
	if (ret)
		deb_sel = 0;
	else
		deb_sel <<= __ffs(info->lvsys_int_rdb_sel_mask);
	ret = regmap_update_bits(lvsys_notify->regmap, info->lvsys_int_en_reg,
				 info->lvsys_int_rdb_sel_mask, deb_sel);
	if (ret)
		dev_notice(lvsys_notify->dev, "Failed to set LVSYS_INT_RDB_SEL, ret=%d\n", ret);

	return ret;
}

static int pmic_lvsys_notify_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct device_node *np = pdev->dev.of_node;
	struct pmic_lvsys_notify *lvsys_notify;
	const struct pmic_lvsys_info *info;

	lvsys_notify = devm_kzalloc(&pdev->dev, sizeof(*lvsys_notify), GFP_KERNEL);
	if (!lvsys_notify)
		return -ENOMEM;

	info = of_device_get_match_data(&pdev->dev);
	if (!info) {
		dev_notice(&pdev->dev, "no platform data\n");
		return -EINVAL;
	}
	lvsys_notify->dev = &pdev->dev;
	lvsys_notify->info = info;
	lvsys_notify->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!lvsys_notify->regmap) {
		dev_notice(&pdev->dev, "failed to get regmap\n");
		return -ENODEV;
	}
	mutex_init(&lvsys_notify->lock);
	pmic_lvsys_parse_dt(lvsys_notify, np);

	irq = platform_get_irq_byname_optional(pdev, "LVSYS_F");
	if (irq < 0) {
		dev_notice(&pdev->dev, "failed to get LVSYS_F irq, ret=%d\n", irq);
		return irq;
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL, lvsys_f_int_handler, IRQF_ONESHOT,
					"LVSYS_F", lvsys_notify);
	if (ret < 0) {
		dev_notice(&pdev->dev, "failed to request LVSYS_F irq, ret=%d\n", ret);
		return ret;
	}

	irq = platform_get_irq_byname_optional(pdev, "LVSYS_R");
	if (irq < 0) {
		dev_notice(&pdev->dev, "failed to get LVSYS_R irq, ret=%d\n", irq);
		return irq;
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL, lvsys_r_int_handler, IRQF_ONESHOT,
					"LVSYS_R", lvsys_notify);
	if (ret < 0) {
		dev_notice(&pdev->dev, "failed to request LVSYS_R irq, ret=%d\n", ret);
		return ret;
	}
	/* RG_LVSYS_INT_EN = 0x1 */
	enable_lvsys_int(lvsys_notify, true);

	ret = vio18_switch_init(&pdev->dev, lvsys_notify->regmap);
	if (ret)
		dev_notice(&pdev->dev, "vio18_switch_init failed, ret=%d\n", ret);

	return 0;
}

static const struct of_device_id pmic_lvsys_notify_of_match[] = {
	{
		.compatible = "mediatek,mt6363-lvsys-notify",
		.data = &mt6363_lvsys_info,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, pmic_lvsys_notify_of_match);

static struct platform_driver pmic_lvsys_notify_driver = {
	.driver = {
		.name = "pmic_lvsys_notify",
		.of_match_table = pmic_lvsys_notify_of_match,
	},
	.probe	= pmic_lvsys_notify_probe,
};
module_platform_driver(pmic_lvsys_notify_driver);

MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK pmic lvsys notify driver");
MODULE_LICENSE("GPL");

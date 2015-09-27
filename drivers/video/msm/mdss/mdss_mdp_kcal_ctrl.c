/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2013, LGE Inc. All rights reserved
 * Copyright (c) 2014 savoca <adeddo27@gmail.com>
 * Copyright (c) 2014 Paul Reioux <reioux@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>

#include "mdss_mdp.h"

#define DEF_PCC 0x100
#define DEF_PA 0xff
#define PCC_ADJ 0x80

struct kcal_lut_data {
	int red;
	int green;
	int blue;
	int minimum;
	int enable;
	int invert;
	int sat;
	int hue;
	int val;
	int cont;
};

static int mdss_mdp_kcal_display_commit(void)
{
	int i;
	int ret = 0;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		/* pp setup requires mfd */
		if ((mdss_mdp_ctl_is_power_on(ctl)) && (ctl->mfd)) {
			ret = mdss_mdp_pp_setup(ctl);
			if (ret)
				pr_err("%s: setup failed: %d\n", __func__, ret);
		}
	}

	return ret;
}

static void mdss_mdp_kcal_update_pcc(struct kcal_lut_data *lut_data)
{
	u32 copyback = 0;
	struct mdp_pcc_cfg_data pcc_config;

	memset(&pcc_config, 0, sizeof(struct mdp_pcc_cfg_data));

	lut_data->red = lut_data->red < lut_data->minimum ?
		lut_data->minimum : lut_data->red;
	lut_data->green = lut_data->green < lut_data->minimum ?
		lut_data->minimum : lut_data->green;
	lut_data->blue = lut_data->blue < lut_data->minimum ?
		lut_data->minimum : lut_data->blue;

	pcc_config.block = MDP_LOGICAL_BLOCK_DISP_0;
	pcc_config.ops = lut_data->enable ?
		MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE :
			MDP_PP_OPS_WRITE | MDP_PP_OPS_DISABLE;
	pcc_config.r.r = lut_data->red * PCC_ADJ;
	pcc_config.g.g = lut_data->green * PCC_ADJ;
	pcc_config.b.b = lut_data->blue * PCC_ADJ;

	if (lut_data->invert) {
		pcc_config.r.c = pcc_config.g.c =
			pcc_config.b.c = 0x7ff8;
		pcc_config.r.r |= (0xffff << 16);
		pcc_config.g.g |= (0xffff << 16);
		pcc_config.b.b |= (0xffff << 16);
	}

	mdss_mdp_pcc_config(&pcc_config, &copyback);
}

static void mdss_mdp_kcal_read_pcc(struct kcal_lut_data *lut_data)
{
	u32 copyback = 0;
	struct mdp_pcc_cfg_data pcc_config;

	memset(&pcc_config, 0, sizeof(struct mdp_pcc_cfg_data));

	pcc_config.block = MDP_LOGICAL_BLOCK_DISP_0;
	pcc_config.ops = MDP_PP_OPS_READ;

	mdss_mdp_pcc_config(&pcc_config, &copyback);

	/* LiveDisplay disables pcc when using default values and regs
	 * are zeroed on pp resume, so throw these values out.
	 */
	if (!pcc_config.r.r && !pcc_config.g.g && !pcc_config.b.b)
		return;

	lut_data->red = (pcc_config.r.r & 0xffff) / PCC_ADJ;
	lut_data->green = (pcc_config.g.g & 0xffff) / PCC_ADJ;
	lut_data->blue = (pcc_config.b.b & 0xffff) / PCC_ADJ;
}

static void mdss_mdp_kcal_update_pa(struct kcal_lut_data *lut_data)
{
	u32 copyback = 0;
	struct mdp_pa_cfg_data pa_config;
	struct mdp_pa_v2_cfg_data pa_v2_config;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (mdata->mdp_rev < MDSS_MDP_HW_REV_103) {
		memset(&pa_config, 0, sizeof(struct mdp_pa_cfg_data));

		pa_config.block = MDP_LOGICAL_BLOCK_DISP_0;
		pa_config.pa_data.flags = lut_data->enable ?
			MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE :
				MDP_PP_OPS_WRITE | MDP_PP_OPS_DISABLE;
		pa_config.pa_data.hue_adj = lut_data->hue;
		pa_config.pa_data.sat_adj = lut_data->sat;
		pa_config.pa_data.val_adj = lut_data->val;
		pa_config.pa_data.cont_adj = lut_data->cont;

		mdss_mdp_pa_config(&pa_config, &copyback);
	} else {
		memset(&pa_v2_config, 0, sizeof(struct mdp_pa_v2_cfg_data));

		pa_v2_config.block = MDP_LOGICAL_BLOCK_DISP_0;
		pa_v2_config.pa_v2_data.flags = lut_data->enable ?
			MDP_PP_OPS_WRITE | MDP_PP_OPS_ENABLE :
				MDP_PP_OPS_WRITE | MDP_PP_OPS_DISABLE;
		pa_v2_config.pa_v2_data.flags |= MDP_PP_PA_HUE_ENABLE;
		pa_v2_config.pa_v2_data.flags |= MDP_PP_PA_HUE_MASK;
		pa_v2_config.pa_v2_data.flags |= MDP_PP_PA_SAT_ENABLE;
		pa_v2_config.pa_v2_data.flags |= MDP_PP_PA_SAT_MASK;
		pa_v2_config.pa_v2_data.flags |= MDP_PP_PA_VAL_ENABLE;
		pa_v2_config.pa_v2_data.flags |= MDP_PP_PA_VAL_MASK;
		pa_v2_config.pa_v2_data.flags |= MDP_PP_PA_CONT_ENABLE;
		pa_v2_config.pa_v2_data.flags |= MDP_PP_PA_CONT_MASK;
		pa_v2_config.pa_v2_data.global_hue_adj = lut_data->hue;
		pa_v2_config.pa_v2_data.global_sat_adj = lut_data->sat;
		pa_v2_config.pa_v2_data.global_val_adj = lut_data->val;
		pa_v2_config.pa_v2_data.global_cont_adj = lut_data->cont;

		mdss_mdp_pa_v2_config(&pa_v2_config, &copyback);
	}
}

static ssize_t kcal_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	int kcal_r, kcal_g, kcal_b, r;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	r = sscanf(buf, "%d %d %d", &kcal_r, &kcal_g, &kcal_b);
	if ((r != 3) || (kcal_r < 1 || kcal_r > 256) ||
		(kcal_g < 1 || kcal_g > 256) || (kcal_b < 1 || kcal_b > 256))
		return -EINVAL;

	lut_data->red = kcal_r;
	lut_data->green = kcal_g;
	lut_data->blue = kcal_b;

	mdss_mdp_kcal_update_pcc(lut_data);
	mdss_mdp_kcal_display_commit();

	return count;
}

static ssize_t kcal_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	mdss_mdp_kcal_read_pcc(lut_data);

	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n",
		lut_data->red, lut_data->green, lut_data->blue);
}

static ssize_t kcal_min_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_min, r;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	r = kstrtoint(buf, 10, &kcal_min);
	if ((r) || (kcal_min < 1 || kcal_min > 256))
		return -EINVAL;

	lut_data->minimum = kcal_min;

	mdss_mdp_kcal_update_pcc(lut_data);
	mdss_mdp_kcal_display_commit();

	return count;
}

static ssize_t kcal_min_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", lut_data->minimum);
}

static ssize_t kcal_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_enable, r;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	r = kstrtoint(buf, 10, &kcal_enable);
	if ((r) || (kcal_enable != 0 && kcal_enable != 1) ||
		(lut_data->enable == kcal_enable))
		return -EINVAL;

	lut_data->enable = kcal_enable;

	mdss_mdp_kcal_update_pcc(lut_data);
	mdss_mdp_kcal_update_pa(lut_data);
	mdss_mdp_kcal_display_commit();

	return count;
}

static ssize_t kcal_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", lut_data->enable);
}

static ssize_t kcal_invert_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_invert, r;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	r = kstrtoint(buf, 10, &kcal_invert);
	if ((r) || (kcal_invert != 0 && kcal_invert != 1) ||
		(lut_data->invert == kcal_invert))
		return -EINVAL;

	lut_data->invert = kcal_invert;

	mdss_mdp_kcal_update_pcc(lut_data);
	mdss_mdp_kcal_display_commit();

	return count;
}

static ssize_t kcal_invert_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", lut_data->invert);
}

static ssize_t kcal_sat_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_sat, r;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	r = kstrtoint(buf, 10, &kcal_sat);
	if ((r) || ((kcal_sat < 224 || kcal_sat > 383) && kcal_sat != 128))
		return -EINVAL;

	lut_data->sat = kcal_sat;

	mdss_mdp_kcal_update_pa(lut_data);
	mdss_mdp_kcal_display_commit();

	return count;
}

static ssize_t kcal_sat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", lut_data->sat);
}

static ssize_t kcal_hue_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_hue, r;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	r = kstrtoint(buf, 10, &kcal_hue);
	if ((r) || (kcal_hue < 0 || kcal_hue > 1536))
		return -EINVAL;

	lut_data->hue = kcal_hue;

	mdss_mdp_kcal_update_pa(lut_data);
	mdss_mdp_kcal_display_commit();

	return count;
}

static ssize_t kcal_hue_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", lut_data->hue);
}

static ssize_t kcal_val_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_val, r;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	r = kstrtoint(buf, 10, &kcal_val);
	if ((r) || (kcal_val < 128 || kcal_val > 383))
		return -EINVAL;

	lut_data->val = kcal_val;

	mdss_mdp_kcal_update_pa(lut_data);
	mdss_mdp_kcal_display_commit();

	return count;
}

static ssize_t kcal_val_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", lut_data->val);
}

static ssize_t kcal_cont_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_cont, r;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	r = kstrtoint(buf, 10, &kcal_cont);
	if ((r) || (kcal_cont < 128 || kcal_cont > 383))
		return -EINVAL;

	lut_data->cont = kcal_cont;

	mdss_mdp_kcal_update_pa(lut_data);
	mdss_mdp_kcal_display_commit();

	return count;
}

static ssize_t kcal_cont_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", lut_data->cont);
}

static DEVICE_ATTR(kcal, S_IWUSR | S_IRUGO, kcal_show, kcal_store);
static DEVICE_ATTR(kcal_min, S_IWUSR | S_IRUGO, kcal_min_show, kcal_min_store);
static DEVICE_ATTR(kcal_enable, S_IWUSR | S_IRUGO, kcal_enable_show,
	kcal_enable_store);
static DEVICE_ATTR(kcal_invert, S_IWUSR | S_IRUGO, kcal_invert_show,
	kcal_invert_store);
static DEVICE_ATTR(kcal_sat, S_IWUSR | S_IRUGO, kcal_sat_show, kcal_sat_store);
static DEVICE_ATTR(kcal_hue, S_IWUSR | S_IRUGO, kcal_hue_show, kcal_hue_store);
static DEVICE_ATTR(kcal_val, S_IWUSR | S_IRUGO, kcal_val_show, kcal_val_store);
static DEVICE_ATTR(kcal_cont, S_IWUSR | S_IRUGO, kcal_cont_show,
	kcal_cont_store);

static int kcal_ctrl_probe(struct platform_device *pdev)
{
	int ret;
	struct kcal_lut_data *lut_data;

	lut_data = devm_kzalloc(&pdev->dev, sizeof(*lut_data), GFP_KERNEL);
	if (!lut_data) {
		pr_err("%s: failed to allocate memory for lut_data\n",
			__func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, lut_data);

	lut_data->enable = 0x1;
	lut_data->red = DEF_PCC;
	lut_data->green = DEF_PCC;
	lut_data->blue = DEF_PCC;
	lut_data->minimum = 0x23;
	lut_data->invert = 0x0;
	lut_data->hue = 0x0;
	lut_data->sat = DEF_PA;
	lut_data->val = DEF_PA;
	lut_data->cont = DEF_PA;

	mdss_mdp_kcal_update_pcc(lut_data);
	mdss_mdp_kcal_update_pa(lut_data);
	mdss_mdp_kcal_display_commit();

	ret = device_create_file(&pdev->dev, &dev_attr_kcal);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_min);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_enable);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_invert);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_sat);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_hue);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_val);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_cont);
	if (ret) {
		pr_err("%s: unable to create sysfs entries\n", __func__);
		return ret;
	}

	return 0;
}

static int kcal_ctrl_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_kcal);
	device_remove_file(&pdev->dev, &dev_attr_kcal_min);
	device_remove_file(&pdev->dev, &dev_attr_kcal_enable);
	device_remove_file(&pdev->dev, &dev_attr_kcal_invert);
	device_remove_file(&pdev->dev, &dev_attr_kcal_sat);
	device_remove_file(&pdev->dev, &dev_attr_kcal_hue);
	device_remove_file(&pdev->dev, &dev_attr_kcal_val);
	device_remove_file(&pdev->dev, &dev_attr_kcal_cont);

	return 0;
}

static struct platform_driver kcal_ctrl_driver = {
	.probe = kcal_ctrl_probe,
	.remove = kcal_ctrl_remove,
	.driver = {
		.name = "kcal_ctrl",
	},
};

static struct platform_device kcal_ctrl_device = {
	.name = "kcal_ctrl",
};

static int __init kcal_ctrl_init(void)
{
	if (platform_driver_register(&kcal_ctrl_driver))
		return -ENODEV;

	if (platform_device_register(&kcal_ctrl_device))
		return -ENODEV;

	pr_info("%s: registered\n", __func__);

	return 0;
}

static void __exit kcal_ctrl_exit(void)
{
	platform_device_unregister(&kcal_ctrl_device);
	platform_driver_unregister(&kcal_ctrl_driver);
}

module_init(kcal_ctrl_init);
module_exit(kcal_ctrl_exit);

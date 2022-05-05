// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/spmi.h>
#include <dt-bindings/spmi/spmi.h>

struct mtk_spmi_pmic_debug_data {
	struct mutex lock;
	struct regmap *regmap;
	unsigned int reg_value;
	u8 usid;
};

static struct mtk_spmi_pmic_debug_data *mtk_spmi_pmic_debug[SPMI_MAX_SLAVE_ID];

static ssize_t pmic_access_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct mtk_spmi_pmic_debug_data *data = dev_get_drvdata(dev);

	dev_info(dev, "0x%x\n", data->reg_value);
	return sprintf(buf, "0x%x\n", data->reg_value);
}

static ssize_t pmic_access_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	struct mtk_spmi_pmic_debug_data *data;
	struct regmap *regmap;
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_val = 0;
	unsigned int reg_adr = 0;
	u8 usid = 0;

	if (dev) {
		data = dev_get_drvdata(dev);
		if (!data)
			return -ENODEV;
	} else
		return -ENODEV;

	if (buf != NULL && size != 0) {
		dev_info(dev, "size is %d, buf is %s\n", (int)size, buf);

		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		if (addr)
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_adr);
		if (reg_adr & 0xF0000) {
			usid = (u8)((reg_adr & 0xF0000) >> 16);
			if (!mtk_spmi_pmic_debug[usid]) {
				data->reg_value = 0;
				dev_info(dev, "invalid slave-%d\n", usid);
				return -ENODEV;
			}
			regmap = mtk_spmi_pmic_debug[usid]->regmap;
			reg_adr &= (0xFFFF);
		} else {
			usid = data->usid;
			regmap = data->regmap;
		}
		mutex_lock(&data->lock);

		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_val);
			ret = regmap_write(regmap, reg_adr, reg_val);
		} else
			ret = regmap_read(regmap, reg_adr, &data->reg_value);

		mutex_unlock(&data->lock);
		dev_info(dev, "%s slave-%d PMIC Reg[0x%x]=0x%x!\n",
			 (val ? "write" : "read"), usid, reg_adr,
			 (val ? reg_val : data->reg_value));
	}
	return size;
}
static DEVICE_ATTR_RW(pmic_access);

static int mtk_spmi_debug_parse_dt(struct device *dev)
{
	struct device_node *node;
	int err;
	u32 reg[2] = {0};

	if (!dev || !dev->of_node)
		return -ENODEV;
	node = dev->of_node;
	err = of_property_read_u32_array(node, "reg", reg, 2);
	if (err) {
		pr_info("%s does not have 'reg' property\n", __func__);
		return -EINVAL;
	}

	if (reg[1] != SPMI_USID) {
		pr_info("%s node contains unsupported 'reg' entry\n", __func__);
		return -EINVAL;
	}

	if (reg[0] >= SPMI_MAX_SLAVE_ID) {
		pr_info("%s invalid usid=%d\n", __func__, reg[0]);
		return -EINVAL;
	}

	return reg[0];
}

static int mtk_spmi_pmic_debug_probe(struct platform_device *pdev)
{
	struct mtk_spmi_pmic_debug_data *data;
	int ret = 0, usid = 0;

	data = devm_kzalloc(&pdev->dev, sizeof(struct mtk_spmi_pmic_debug_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->lock);
	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap)
		return -ENODEV;

	platform_set_drvdata(pdev, data);

	/* Create sysfs entry */
	ret = device_create_file(&pdev->dev, &dev_attr_pmic_access);
	if (ret < 0)
		dev_info(&pdev->dev, "failed to create sysfs file\n");
	usid = mtk_spmi_debug_parse_dt(pdev->dev.parent);
	if (usid >= 0) {
		data->usid = (u8) usid;
		mtk_spmi_pmic_debug[usid] = data;
	}
	dev_info(&pdev->dev, "success to create %s slave-%d sysfs file\n", pdev->name, usid);

	return ret;
}

static int mtk_spmi_pmic_debug_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_pmic_access);

	return 0;
}

static const struct of_device_id mtk_spmi_pmic_debug_of_match[] = {
	{ .compatible = "mediatek,spmi-pmic-debug", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_spmi_pmic_debug_of_match);

static struct platform_driver mtk_spmi_pmic_debug_driver = {
	.driver = {
		.name = "mtk-spmi-pmic-debug",
		.of_match_table = mtk_spmi_pmic_debug_of_match,
	},
	.probe = mtk_spmi_pmic_debug_probe,
	.remove = mtk_spmi_pmic_debug_remove,
};
module_platform_driver(mtk_spmi_pmic_debug_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_AUTHOR("Jeter Chen <jeter.chen@mediatek.com>");
MODULE_DESCRIPTION("Debug driver for MediaTek SPMI PMIC");
MODULE_LICENSE("GPL v2");

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/mtk_auxadc_intf.h>

static struct mtk_auxadc_intf *auxadc_intf;

const char *pmic_get_auxadc_name(u8 list)
{
	return pmic_auxadc_channel_name[list];
}
EXPORT_SYMBOL(pmic_get_auxadc_name);

int pmic_get_auxadc_value(u8 list)
{
	int value = 0;

	if (list >= AUXADC_LIST_START && list <= AUXADC_LIST_END) {
		value = mt_get_auxadc_value(list);
		return value;
	}
	pr_info("%s Invalid AUXADC LIST\n", __func__);
	return -EINVAL;
}

static struct mtk_auxadc_ops pmic_auxadc_ops = {
	.get_channel_value = pmic_get_auxadc_value,
	.dump_regs = pmic_auxadc_dump_regs,
};

static struct mtk_auxadc_intf pmic_auxadc_intf = {
	.ops = &pmic_auxadc_ops,
	.name = "mtk_auxadc_intf",
	.channel_name = pmic_auxadc_channel_name,
};

int register_mtk_auxadc_intf(struct mtk_auxadc_intf *intf)
{
	auxadc_intf = intf;
	platform_device_register_simple(auxadc_intf->name, -1, NULL, 0);
	return 0;
}
EXPORT_SYMBOL(register_mtk_auxadc_intf);

void mtk_auxadc_init(void)
{
#if 0
	pmic_auxadc_init();
#endif

	pmic_auxadc_intf.channel_num = pmic_get_auxadc_channel_max();

	if (register_mtk_auxadc_intf(&pmic_auxadc_intf) < 0)
		pr_notice("[%s] register MTK Auxadc Intf Fail\n", __func__);
}

static ssize_t mtk_auxadc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t mtk_auxadc_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static struct device_attribute mtk_auxadc_attrs[] = {
	__ATTR(dump_channel, 0444, mtk_auxadc_show, NULL),
	__ATTR(channel, 0644, mtk_auxadc_show, mtk_auxadc_store),
	__ATTR(value, 0444, mtk_auxadc_show, NULL),
	__ATTR(regs, 0444, mtk_auxadc_show, NULL),
	__ATTR(md_channel, 0644, mtk_auxadc_show, mtk_auxadc_store),
	__ATTR(md_value, 0444, mtk_auxadc_show, NULL),
	__ATTR(md_dump_channel, 0444, mtk_auxadc_show, NULL),
};

static int get_parameters(char *buf, long int *param, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");
	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;
			if (kstrtoul(token, base, &param[cnt]) != 0)
				return -EINVAL;
			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static ssize_t mtk_auxadc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	ptrdiff_t cmd;
	int ret;
	long int val;

	cmd = attr - mtk_auxadc_attrs;
	switch (cmd) {
	case AUXADC_CHANNEL:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			pr_notice("get parameter fail\n");
			return -EINVAL;
		}
		auxadc_intf->dbg_chl = val;
		break;
	default:
		break;

	}
	return count;
}

static ssize_t mtk_auxadc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ptrdiff_t cmd;
	int i;
	int value;

	cmd = attr - mtk_auxadc_attrs;
	buf[0] = '\0';

	switch (cmd) {
	case AUXADC_DUMP:
		snprintf(buf+strlen(buf),
			256, "=== Channel Dump Start ===\n");
		for (i = 0; i < auxadc_intf->channel_num; i++) {
			value = auxadc_intf->ops->get_channel_value(i);
			if (value >= 0)
				snprintf(buf+strlen(buf), 256,
					"Channel%d (%s) = 0x%x(%d)\n",
					i, auxadc_intf->channel_name[i],
					value, value);
		}
		snprintf(buf+strlen(buf),
			256, "==== Channel Dump End ====\n");
		break;
	case AUXADC_CHANNEL:
		snprintf(buf + strlen(buf), 256, "%d (%s)\n"
			, auxadc_intf->dbg_chl
			, auxadc_intf->channel_name[auxadc_intf->dbg_chl]);
		break;
	case AUXADC_VALUE:
		value = auxadc_intf->ops->get_channel_value(
				auxadc_intf->dbg_chl);
		snprintf(buf + strlen(buf), 256, "Channel%d (%s) = 0x%x(%d)\n"
			 , auxadc_intf->dbg_chl
			 , auxadc_intf->channel_name[auxadc_intf->dbg_chl]
			 , value, value);
		break;
	case AUXADC_REGS:
		auxadc_intf->ops->dump_regs(buf);
		break;
	default:
		break;
	}
	return strlen(buf);
}

static int create_sysfs_interface(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_auxadc_attrs); i++)
		if (device_create_file(dev, mtk_auxadc_attrs + i))
			return -EINVAL;
	return 0;
}

static int remove_sysfs_interface(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_auxadc_attrs); i++)
		device_remove_file(dev, mtk_auxadc_attrs + i);
	return 0;
}

static int mtk_auxadc_intf_probe(struct platform_device *pdev)
{
	int ret;

	pr_info("%s\n", __func__);

	platform_set_drvdata(pdev, auxadc_intf);
	ret = create_sysfs_interface(&pdev->dev);
	if (ret < 0) {
		pr_notice("%s create sysfs fail\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int mtk_auxadc_intf_remove(struct platform_device *pdev)
{
	int ret;

	ret = remove_sysfs_interface(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_auxadc_intf_driver = {
	.driver = {
		.name = "mtk_auxadc_intf",
	},
	.probe = mtk_auxadc_intf_probe,
	.remove = mtk_auxadc_intf_remove,
};
module_platform_driver(mtk_auxadc_intf_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK PMIC AUXADC Interface Driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_VERSION("1.0.0_M");

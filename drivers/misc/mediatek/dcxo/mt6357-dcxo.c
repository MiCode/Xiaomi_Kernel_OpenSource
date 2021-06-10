// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <mach/upmu_hw.h>
#include <upmu_common.h>

#define XO_CDAC_FPM_ADDR	PMIC_XO_CDAC_FPM_ADDR
#define XO_CDAC_FPM_MASK	PMIC_XO_CDAC_FPM_MASK
#define XO_CDAC_FPM_SHIFT	PMIC_XO_CDAC_FPM_SHIFT

#define XO_COFST_FPM_ADDR	PMIC_XO_COFST_FPM_ADDR
#define XO_COFST_FPM_MASK	PMIC_XO_COFST_FPM_MASK
#define XO_COFST_FPM_SHIFT	PMIC_XO_COFST_FPM_SHIFT

#define XO_CORE_IDAC_FPM_ADDR	PMIC_XO_CORE_IDAC_FPM_ADDR
#define XO_CORE_IDAC_FPM_MASK	PMIC_XO_CORE_IDAC_FPM_MASK
#define XO_CORE_IDAC_FPM_SHIFT	PMIC_XO_CORE_IDAC_FPM_SHIFT

#define XO_AAC_FPM_SWEN_ADDR	PMIC_XO_AAC_FPM_SWEN_ADDR
#define XO_AAC_FPM_SWEN_MASK	PMIC_XO_AAC_FPM_SWEN_MASK
#define XO_AAC_FPM_SWEN_SHIFT	PMIC_XO_AAC_FPM_SWEN_SHIFT

#define XO_CDAC_TOTAL_MASK	0xff
#define XO_CDAC_VALUE_MASK	0x7f
#define XO_CDAC_SIGN_BIT	7
#define XO_CDAC_FPM_RESET	0x88
#define XO_CDAC_FPM_DEFAULT	0x7d


struct mt6357_dcxo {
	struct device	*dev;
	struct mutex	lock;
	uint32_t	cur_dcxo_capid;
	uint32_t	ori_dcxo_capid;
	uint32_t	nvram_offset;
};

static const struct of_device_id mt6357_dcxo_of_match[] = {
	{ .compatible = "mediatek,mt6357-dcxo", },
	{ },
};

MODULE_DEVICE_TABLE(of, mt6357_dcxo_of_match);

static void dcxo_trim_write(uint32_t cap_code)
{
	pmic_config_interface(XO_COFST_FPM_ADDR, 0x0, XO_COFST_FPM_MASK,
			      XO_COFST_FPM_SHIFT);
	pmic_config_interface(XO_CDAC_FPM_ADDR, cap_code, XO_CDAC_FPM_MASK,
			      XO_CDAC_FPM_SHIFT);
	pmic_config_interface(XO_CORE_IDAC_FPM_ADDR, 0x2, XO_CORE_IDAC_FPM_MASK,
			      XO_CORE_IDAC_FPM_SHIFT);
	pmic_config_interface(XO_AAC_FPM_SWEN_ADDR, 0x0, XO_AAC_FPM_SWEN_MASK,
			      XO_AAC_FPM_SWEN_SHIFT);
	mdelay(1);
	pmic_config_interface(XO_AAC_FPM_SWEN_ADDR, 0x1, XO_AAC_FPM_SWEN_MASK,
			      XO_AAC_FPM_SWEN_SHIFT);
	mdelay(5);
}

static uint32_t dcxo_trim_read(void)
{
	uint32_t cap_code = 0;

	pmic_read_interface(XO_CDAC_FPM_ADDR, &cap_code, XO_CDAC_FPM_MASK,
			    XO_CDAC_FPM_SHIFT);

	return cap_code;
}

static uint32_t dcxo_capid_add_offset(uint32_t capid, uint32_t offset)
{
	uint32_t capid_value;
	uint32_t offset_sign, offset_value;
	int32_t tmp_value;
	uint32_t final_capid;

	/* capid don't have sign bit, value from 0x00 to 0xFF */
	capid_value = capid & XO_CDAC_TOTAL_MASK;
	/* offset bit 7 is sign bit. bit7=1 means minus */
	offset_sign = !!(offset & BIT(XO_CDAC_SIGN_BIT));
	offset_value = offset & XO_CDAC_VALUE_MASK;

	/* process plus/minus overflow */
	if (offset_sign) { /* negetive offset sign, minus */
		tmp_value = (int32_t)capid_value - (int32_t)offset_value;
		if (tmp_value < 0)
			tmp_value = 0;
		final_capid = (uint32_t)tmp_value;
	} else { /* positive offset sign, plus */
		tmp_value = (int32_t)capid_value + (int32_t)offset_value;
		if (tmp_value > XO_CDAC_TOTAL_MASK) /* value overflow */
			tmp_value = XO_CDAC_TOTAL_MASK;
		final_capid = (uint32_t)tmp_value;
	}

	return final_capid;
}

static uint32_t dcxo_capid_sub_offset(uint32_t cur_capid, uint32_t ori_capid)
{
	uint32_t cur_capid_value;
	uint32_t ori_capid_value;
	int32_t tmp_value;
	uint32_t final_offset;

	cur_capid_value = cur_capid & XO_CDAC_TOTAL_MASK;
	ori_capid_value = ori_capid & XO_CDAC_TOTAL_MASK;

	/* process plus/minus error */
	if (cur_capid_value >= ori_capid_value) {
		/* offset sign bit is positive */
		tmp_value = (int32_t)cur_capid_value - (int32_t)ori_capid_value;
		if (tmp_value > XO_CDAC_VALUE_MASK) /* value overflow */
			tmp_value = XO_CDAC_VALUE_MASK;
		final_offset = (uint32_t)tmp_value;
	} else {
		/* offset sign bit is negative */
		tmp_value = (int32_t)ori_capid_value - (int32_t)cur_capid_value;
		if (tmp_value > XO_CDAC_VALUE_MASK) /* value overflow */
			tmp_value = XO_CDAC_VALUE_MASK;
		final_offset = BIT(XO_CDAC_SIGN_BIT) | (uint32_t)tmp_value;
	}

	return final_offset;
}

static ssize_t show_dcxo_capid(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	uint32_t capid;

	capid = dcxo_trim_read();

	return sprintf(buf, "dcxo capid: 0x%x\n", capid);
}

static ssize_t store_dcxo_capid(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mt6357_dcxo *dcxo = dev_get_drvdata(dev);
	uint32_t capid;
	int ret;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 0, &capid);
		if (ret) {
			pr_info("wrong format!\n");
			return ret;
		}
		if (capid > XO_CDAC_TOTAL_MASK) {
			pr_info("cap code should be within %x!\n",
				XO_CDAC_TOTAL_MASK);
			return -EINVAL;
		}

		pr_info("original cap code: 0x%x\n", dcxo->ori_dcxo_capid);
		dcxo_trim_write(capid);
		mdelay(1);
		dcxo->cur_dcxo_capid = dcxo_trim_read();
		pr_info("write capid 0x%x done. current capid: 0x%x\n",
			capid, dcxo->cur_dcxo_capid);
	}

	return size;
}

static DEVICE_ATTR(dcxo_capid, 0664, show_dcxo_capid, store_dcxo_capid);

static ssize_t show_dcxo_board_offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mt6357_dcxo *dcxo = dev_get_drvdata(dev);
	uint32_t offset;

	offset = dcxo_capid_sub_offset(dcxo->cur_dcxo_capid,
				       dcxo->ori_dcxo_capid);

	return sprintf(buf, "dcxo capid offset: 0x%x\n", offset);
}

static ssize_t store_dcxo_board_offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mt6357_dcxo *dcxo = dev_get_drvdata(dev);
	uint32_t offset, capid;
	int ret;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 0, &offset);
		if (ret) {
			pr_info("wrong format!\n");
			return ret;
		}
		if (offset > XO_CDAC_TOTAL_MASK) {
			pr_info("offset should be within %x!\n",
				XO_CDAC_TOTAL_MASK);
			return -EINVAL;
		}

		capid = dcxo->ori_dcxo_capid;
		pr_info("original cap code: 0x%x\n", capid);

		capid = dcxo_capid_add_offset(capid, offset);
		dcxo_trim_write(capid);
		mdelay(1);
		dcxo->cur_dcxo_capid = dcxo_trim_read();
		pr_info("write capid offset 0x%x done. current capid: 0x%x\n",
			offset, dcxo->cur_dcxo_capid);
	}

	return size;
}

static DEVICE_ATTR(dcxo_board_offset, 0664, show_dcxo_board_offset,
		   store_dcxo_board_offset);

static ssize_t show_nvram_board_offset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mt6357_dcxo *dcxo = dev_get_drvdata(dev);

	/* Should not modify sprintf format. Otherwise DcxoSetCap will fail. */
	return sprintf(buf, "0x%x", dcxo->nvram_offset);
}

static ssize_t store_nvram_board_offset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mt6357_dcxo *dcxo = dev_get_drvdata(dev);
	uint32_t offset;
	int ret;

	if (buf != NULL && size != 0) {
		ret = kstrtouint(buf, 0, &offset);
		if (ret) {
			pr_info("wrong format!\n");
			return ret;
		}
		if (offset > XO_CDAC_TOTAL_MASK) {
			pr_info("offset should be within %x!\n",
				XO_CDAC_TOTAL_MASK);
			return -EINVAL;
		}

		dcxo->nvram_offset = offset;
		pr_info("write nvram_board_offset 0x%x\n", offset);
	}

	return size;
}

static DEVICE_ATTR(nvram_board_offset, 0664, show_nvram_board_offset,
		   store_nvram_board_offset);

static int mt_dcxo_probe(struct platform_device *pdev)
{
	struct mt6357_dcxo *dcxo;
	int ret;
	uint32_t default_capid = 0;

	dcxo = devm_kzalloc(&pdev->dev, sizeof(*dcxo), GFP_KERNEL);
	if (!dcxo)
		return -ENOMEM;

	mutex_init(&dcxo->lock);
	dcxo->dev = &pdev->dev;
	platform_set_drvdata(pdev, dcxo);

	ret = device_create_file(&pdev->dev, &dev_attr_dcxo_capid);
	if (ret)
		dev_info(&pdev->dev, "Failed to create capid file: %d\n", ret);

	ret = device_create_file(&pdev->dev, &dev_attr_dcxo_board_offset);
	if (ret)
		dev_info(&pdev->dev, "Failed to create offset file: %d\n", ret);

	ret = device_create_file(&pdev->dev, &dev_attr_nvram_board_offset);
	if (ret)
		dev_info(&pdev->dev, "Failed to create nvram file: %d\n", ret);

	/* get original cap code */
	dcxo->ori_dcxo_capid = dcxo_trim_read();
	dev_info(&pdev->dev, "Original cap code: 0x%x\n", dcxo->ori_dcxo_capid);

	/* get default cap code */
	dcxo->ori_dcxo_capid = dcxo_trim_read();
	dev_info(&pdev->dev, "Default cap code: 0x%x\n", dcxo->ori_dcxo_capid);

	ret = of_property_read_u32(dcxo->dev->of_node, "default-capid",
				   &default_capid);
	if (ret) {
		dev_info(&pdev->dev,
			 "Failed to get default-capid from dts: %d\n", ret);
		default_capid = 0;
		ret = 0;
	}
	default_capid &= XO_CDAC_TOTAL_MASK;
	dev_info(&pdev->dev, "Default dts capid: 0x%x\n", default_capid);

	if (default_capid)
		dcxo_trim_write(default_capid);
	else
		dcxo_trim_write(XO_CDAC_FPM_DEFAULT);

	dcxo->cur_dcxo_capid = dcxo_trim_read();
	dev_info(&pdev->dev, "Current cap code: 0x%x\n", dcxo->cur_dcxo_capid);

	return ret;
}

static int mt_dcxo_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mt_dcxo_driver = {
	.remove		= mt_dcxo_remove,
	.probe		= mt_dcxo_probe,
	.driver		= {
		.name		= "mt6357-dcxo",
		.of_match_table	= mt6357_dcxo_of_match,
	},
};

module_platform_driver(mt_dcxo_driver);

MODULE_LICENSE("GPL");

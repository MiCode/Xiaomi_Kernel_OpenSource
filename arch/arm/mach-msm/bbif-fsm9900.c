/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <linux/fsm_rfic.h>

#define COMBODAC_CSR            0x3000
#define BBIF_MISC               0x2000
#define COMBODAC_CONFIG_0       0x0
#define COMBODAC_CONFIG_1       0x20
#define COMBODAC_CTRL_0         0x40
#define COMBODAC_STATUS_0       0x60

#define CAL_RESET                    0x80
#define COMBODAC_CTRL_C__I_EN        0x1
#define COMBODAC_CTRL_C__Q_EN        0x2
#define COMBODAC_CTRL_C__CAL_EN      0x100
#define COMBODAC_CTRL_C__DCCAL_EN    0x20000000
#define COMBODAC_CTRL_C__DCCAL_RESET 0x40000000
#define CAL_DONE                     0x00060000

#define BBIF_PRI_MODE 0
#define BBIF_ADC_CFG  0x20

#define BBIF_MAP_SIGMA_DELTA_0 0x10
#define BBIF_MAP_SIGMA_DELTA_1 0x14
#define BBIF_MAP_SIGMA_DELTA_2 0x18
#define BBIF_MAP_SIGMA_DELTA_3 0x1C

#define BBIF_BBRX_TEST1_BASE   0x800
#define BBIF_BBRX_TEST2_BASE   0x820
#define BBIF_BBRX_TEST3_BASE   0x840
#define BBIF_BBRX_CONTROL_BASE 0x880
#define BBIF_BBRX_CONFIG_BASE  0x8a0

#define SIGMA_DELTA_VAL_0   0xF5F7F3F1
#define SIGMA_DELTA_VAL_1   0xFBF9FDFF
#define SIGMA_DELTA_VAL_2   0x0B090D0F
#define SIGMA_DELTA_VAL_3   0x05070301

#define BBIF_CONFIG_WCDMA_5_DPD 0x80802100
#define BBIF_CONFIG_LTE_5_DPD   0x80802100
#define BBIF_CONFIG_LTE_10      0x80803900
#define BBIF_CONFIG_LTE_15      0x80803500
#define BBIF_CONFIG_LTE_20      0x80803200
#define BBIF_CONFIG_LTE_10_DPD  0x98800F00
#define BBIF_CONFIG_LTE_15_DPD  0x98800800
#define BBIF_CONFIG_LTE_20_DPD  0x98800600

#define BBIF_CONFIG_LTE_20_DPD_AAF_BYPASS  0x98800602
#define BBIF_CONFIG_LTE_10_AAF_ENABLED     0x80807900

#define BBIF_BBRX_TEST1		0x00000000
#define BBIF_BBRX_TEST2		0x00000000
#define BBIF_BBRX_TEST3		0x1b000000

#define BBIF_CONFIG_1		0x0CAB4C00;
#define BBIF_CONFIG_MEAS	0x41178206;
#define BBIF_CONFIG_DCCAL	0x41178226
#define BBIF_MAX_OFFSET		0x2000
#define BBIF_MAX_ADC		6

void __iomem *bbif_base;

static void reset_msbcal(void)
{
	int read_data;
	int i;
	void __iomem *dac_base = bbif_base + COMBODAC_CSR;
	pr_debug("%s: COMBODAC MSB CAL  Reset Module\n", __func__);

	/*  RESET COMBODAC 0 - 4 */
	for (i = 0; i < 4; i++) {
		read_data = __raw_readl(dac_base + COMBODAC_CTRL_0 + i*4);
		/* reset CAL */
		read_data |= CAL_RESET | COMBODAC_CTRL_C__I_EN |
			COMBODAC_CTRL_C__Q_EN;
		__raw_writel(read_data, dac_base + COMBODAC_CTRL_0 + i*4);

		udelay(100);

		/* DEASSERT reset */
		read_data &= ~CAL_RESET;
		__raw_writel(read_data, dac_base + COMBODAC_CTRL_0 + i*4);

		/* KICK OFF MSB CALIBRATION FOR DAC */

		read_data |= COMBODAC_CTRL_C__CAL_EN;
		__raw_writel(read_data, dac_base + COMBODAC_CTRL_0 + i*4);
		read_data &= ~COMBODAC_CTRL_C__CAL_EN;
		__raw_writel(read_data, dac_base + COMBODAC_CTRL_0 + i*4);

		/* CHECK MSB CALIBRATION FOR DAC */
		udelay(1500);
		read_data = __raw_readl(dac_base + COMBODAC_STATUS_0 + i*4);
		if ((read_data & 0x18) != 0x18)
			pr_info("COMBODAC0:  CAL SAR AND REF FAILED\n");
	}
}

static void reset_dccal(void)
{
	int read_data;
	int i;
	void __iomem *dac_base = bbif_base + COMBODAC_CSR;

	pr_debug("%s: COMBODAC DCCAL  Reset Module\n", __func__);

	/* Reset COMBODAC DCCAL 0 - 3 */
	for (i = 0; i < 4; i++) {
		read_data = __raw_readl(dac_base + COMBODAC_CTRL_0 + i*4);

		read_data |= COMBODAC_CTRL_C__I_EN | COMBODAC_CTRL_C__Q_EN |
			COMBODAC_CTRL_C__DCCAL_RESET;
		__raw_writel(read_data, dac_base + COMBODAC_CTRL_0 + i*4);

		udelay(100);

		read_data &= ~COMBODAC_CTRL_C__DCCAL_RESET;
		__raw_writel(read_data, dac_base + COMBODAC_CTRL_0 + i*4);

		udelay(100);

		read_data |= COMBODAC_CTRL_C__DCCAL_EN;
		__raw_writel(read_data, dac_base + COMBODAC_CTRL_0 + i*4);

		udelay(100);

		read_data &= ~COMBODAC_CTRL_C__DCCAL_EN;
		__raw_writel(read_data, dac_base + COMBODAC_CTRL_0 + i*4);

		udelay(100);
		read_data = __raw_readl(dac_base + COMBODAC_STATUS_0 + i*4);

		if (!(read_data & CAL_DONE))
			pr_info("%s: DCCAL failed for DAC %d\n", __func__, i);
	}
}

static void set_combodac_cfg(int config_type)
{
	int wdata = 0;
	int config_0, config_1;
	int i;
	void __iomem *dac_base = bbif_base + COMBODAC_CSR;

	pr_debug("%s: ComboDAC0-1 Config Module\n", __func__);

	if (config_type == 2) {
		config_0 = BBIF_CONFIG_MEAS;
		config_1 = BBIF_CONFIG_1;
	} else {
		config_0 = BBIF_CONFIG_DCCAL;
		config_1 = BBIF_CONFIG_1;
	}

	for (i = 0; i < 4; i++) {
		wdata = __raw_readl(dac_base + COMBODAC_CTRL_0 + i*4);
		wdata |= COMBODAC_CTRL_C__I_EN | COMBODAC_CTRL_C__Q_EN;
		__raw_writel(wdata, dac_base + COMBODAC_CTRL_0 + i*4);
		__raw_writel(config_0, dac_base + COMBODAC_CONFIG_0 + i*4);
		__raw_writel(config_1, dac_base + COMBODAC_CONFIG_1 + i*4);
	}
}


/*
 * File interface
 */

static int bbif_open(struct inode *inode, struct file *file)
{

	return 0;
}

static int bbif_release(struct inode *inode, struct file *file)
{

	return 0;
}

static long bbif_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	void __iomem *argp = (void __iomem *) arg;
	void __iomem *bbif_adc_base;
	bbif_adc_base = bbif_base + BBIF_MISC;

	switch (cmd) {
	case BBIF_IOCTL_GET:
		{
			struct bbif_param param;

			if (copy_from_user(&param, argp, sizeof(param))) {
				pr_err("%s: copy_from_user error\n", __func__);
				return -EFAULT;
			}

			if (param.offset > BBIF_MAX_OFFSET) {
				pr_err("%s: Exceeds max offset\n", __func__);
				return -EFAULT;
			}

			param.value = __raw_readl(bbif_adc_base + param.offset);

			if (copy_to_user(argp, &param, sizeof(param))) {
				pr_err("%s: copy_to_user error\n", __func__);
				return -EFAULT;
			}
		}
		break;

	case BBIF_IOCTL_SET:
		{
			struct bbif_param param;

			if (copy_from_user(&param, argp, sizeof(param))) {
				pr_err("%s: copy_from_user error\n", __func__);
				return -EFAULT;
			}

			if (param.offset > BBIF_MAX_OFFSET) {
				pr_err("%s: Exceeds max offset\n", __func__);
				return -EFAULT;
			}

			__raw_writel(param.value, bbif_adc_base +
					param.offset);
			mb();
		}
		break;

	case BBIF_IOCTL_SET_ADC_BW:
		{
			struct bbif_bw_config param;

			if (copy_from_user(&param, argp, sizeof(param))) {
				pr_err("%s: copy_from_user error\n", __func__);
				return -EFAULT;
			}

			if (param.adc_number > BBIF_MAX_ADC) {
				pr_err("%s: Exceeds max offset\n", __func__);
				return -EFAULT;
			}

			__raw_writel(param.bbrx_test1, bbif_adc_base +
				BBIF_BBRX_TEST1_BASE + param.adc_number*4);
			__raw_writel(param.bbrx_test1, bbif_adc_base +
				BBIF_BBRX_TEST2_BASE + param.adc_number*4);
			__raw_writel(param.bbrx_test1, bbif_adc_base +
				BBIF_BBRX_TEST3_BASE + param.adc_number*4);
			__raw_writel(param.bbrx_config, bbif_adc_base +
				BBIF_BBRX_CONFIG_BASE + param.adc_number*4);
		}
		break;

	default:
		pr_err("%s: Invalid IOCTL\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations bbif_fops = {
	.owner = THIS_MODULE,
	.open = bbif_open,
	.release = bbif_release,
	.unlocked_ioctl = bbif_ioctl,
};

/* Driver initialization & cleanup */

struct miscdevice bbif_misc_dev[] = {
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = BBIF_DEVICE_NAME,
		.fops = &bbif_fops,
	},
};

static int bbif_regulator_init(struct platform_device *pdev)
{
	int ret;
	struct regulator *vreg_ldo4, *vreg_ldo14;

	vreg_ldo4 = regulator_get(&pdev->dev, "vdd-lbbrx");
	if (IS_ERR(vreg_ldo4)) {
		pr_err("%s: vreg get l4-reg failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo4));
		return PTR_ERR(vreg_ldo4);
	}

	ret = regulator_enable(vreg_ldo4);
	if (ret)
		pr_err("%s: unable to enable ldo4 voltage\n", __func__);

	vreg_ldo14 = regulator_get(&pdev->dev, "vdd-hbbrx");
	if (IS_ERR(vreg_ldo14)) {
		pr_err("%s: vreg get l14-reg failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo14));
		return PTR_ERR(vreg_ldo14);
	}

	ret = regulator_enable(vreg_ldo14);
	if (ret)
		pr_err("%s: unable to enable ldo14 voltage\n", __func__);

	return 0;
}

static int bbif_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *mem_res;
	int ret;
	int i;
	void __iomem *bbif_misc_base;

	pr_debug("%s: Entry\n", __func__);

	bbif_regulator_init(pdev);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	bbif_base = devm_ioremap_resource(&pdev->dev, mem_res);
	if (IS_ERR(bbif_base))
		return PTR_ERR(bbif_base);

	ret = misc_register(bbif_misc_dev);

	if (ret < 0) {
		misc_deregister(bbif_misc_dev);
		return ret;
	}

	/*ADC config */
	bbif_misc_base = bbif_base + BBIF_MISC;

	__raw_writel(SIGMA_DELTA_VAL_0, bbif_misc_base +
		BBIF_MAP_SIGMA_DELTA_0);
	__raw_writel(SIGMA_DELTA_VAL_1, bbif_misc_base +
		BBIF_MAP_SIGMA_DELTA_1);
	__raw_writel(SIGMA_DELTA_VAL_2, bbif_misc_base +
		BBIF_MAP_SIGMA_DELTA_2);
	__raw_writel(SIGMA_DELTA_VAL_3, bbif_misc_base +
		BBIF_MAP_SIGMA_DELTA_3);
	__raw_writel(0, bbif_misc_base + BBIF_PRI_MODE);

	/* If the values are different make sure i=1 & i=2 are reversed */

	for (i = 0; i < 6; i++) {
		__raw_writel(0, bbif_misc_base + BBIF_ADC_CFG + i*4);
		__raw_writel(BBIF_BBRX_TEST1, bbif_misc_base +
			BBIF_BBRX_TEST1_BASE + i*4);
		__raw_writel(BBIF_BBRX_TEST2, bbif_misc_base +
			BBIF_BBRX_TEST2_BASE + i*4);
		__raw_writel(BBIF_BBRX_TEST3, bbif_misc_base +
			BBIF_BBRX_TEST3_BASE + i*4);
		__raw_writel(BBIF_CONFIG_LTE_20_DPD, bbif_misc_base +
			BBIF_BBRX_CONFIG_BASE + i*4);
		__raw_writel(0, bbif_misc_base + BBIF_BBRX_CONTROL_BASE + i*4);
		usleep(100000);
		__raw_writel(7, bbif_misc_base + BBIF_BBRX_CONTROL_BASE + i*4);
	}

	/* DAC config */
	set_combodac_cfg(0);
	reset_msbcal();
	reset_dccal();
	set_combodac_cfg(2);

	return of_platform_populate(np, NULL, NULL, &pdev->dev);
}

static int bbif_remove(struct platform_device *pdev)
{

	misc_deregister(bbif_misc_dev);

	return 0;
}

static struct of_device_id bbif_match_table[] = {
	{ .compatible = "qcom,bbif" },
	{}
};
MODULE_DEVICE_TABLE(of, bbif_match_table);

static struct platform_driver bbif_driver = {
	.probe          = bbif_probe,
	.remove         = bbif_remove,
	.driver         = {
			.name   = "bbif",
			.owner   = THIS_MODULE,
			.of_match_table = bbif_match_table,
	},
};

int __init bbif_init(void)
{
	int rc;

	pr_debug("%s: bbif-fsm9900 driver init\n",  __func__);
	rc = platform_driver_register(&bbif_driver);
	return rc;

}

void __exit bbif_exit(void)
{
	pr_debug("%s: bbif-fsm9900 driver exit\n",  __func__);
	platform_driver_unregister(&bbif_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. fsm9900 BBIF driver");

module_init(bbif_init);
module_exit(bbif_exit);

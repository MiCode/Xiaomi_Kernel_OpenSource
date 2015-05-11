/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/fsm_rfic.h>
#include <mach/gpiomux.h>

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
#define BBIF_MAX_ADC		5

#define BBIF_MAP_SIGMA_DELTA_0 0x10
#define BBIF_MAP_SIGMA_DELTA_1 0x14
#define BBIF_MAP_SIGMA_DELTA_2 0x18
#define BBIF_MAP_SIGMA_DELTA_3 0x1C

#define BBIF_BBRX_TEST1_BASE   0x800
#define BBIF_BBRX_TEST2_BASE   0x820
#define BBIF_BBRX_TEST3_BASE   0x840
#define BBIF_BBRX_CONTROL_BASE 0x880
#define BBIF_BBRX_CONFIG_BASE  0x8a0
#define BBIF_BBRX_CONFIG2_BASE 0x8c0

#define SIGMA_DELTA_VAL_0   0xF5F7F3F1
#define SIGMA_DELTA_VAL_1   0xFBF9FDFF
#define SIGMA_DELTA_VAL_2   0x0B090D0F
#define SIGMA_DELTA_VAL_3   0x05070301

#define BBIF_BBRX_TEST1		0x020000a0
#define BBIF_BBRX_TEST2		0x0004b400
#define BBIF_BBRX_TEST3		0x84000988

#define BBIF_CONFIG_1		0x83030104
#define BBIF_CONFIG_2		0x0000001f
#define BBIF_CONTROL_1		0x00000001
#define BBIF_CONTROL_2		0x00000003

#define BBIF_MAX_DAC		4
#define DAC_CTRL_VAL		0x00000003
#define DAC_CTRL_VAL1		0x00000083
#define DAC_CTRL_VAL2		0x00000103
#define DAC_CFG0_VAL		0x41948206
#define DAC_CFG1_VAL		0x00000d00
#define BBIF_MAX_OFFSET		0x2000

/* CAL_SAR_DONE=bit[4], CAL_REF_DONE=bit[3], CAL_MACHINE=bit[2:1]*/
#define DAC_CAL_DONE		0x1f

void __iomem *bbif_base;

static void bbif_combodac_cfg(void)
{
	int read_data = 0;
	void __iomem *dac_base = bbif_base + COMBODAC_CSR;
	int i;

	for (i = 0; i < BBIF_MAX_DAC; i++) {
		__raw_writel(DAC_CTRL_VAL, dac_base + COMBODAC_CTRL_0 + i*4);
		__raw_writel(DAC_CFG0_VAL, dac_base + COMBODAC_CONFIG_0 + i*4);
		__raw_writel(DAC_CFG1_VAL, dac_base + COMBODAC_CONFIG_1 + i*4);

		__raw_writel(DAC_CTRL_VAL1, dac_base + COMBODAC_CTRL_0 + i*4);
		mb(); /* Acts as delay and ensures compiler does not optimize*/
		__raw_writel(DAC_CTRL_VAL, dac_base + COMBODAC_CTRL_0 + i*4);
		mb(); /* Acts as delay and ensures compiler does not optimize*/
		__raw_writel(DAC_CTRL_VAL2, dac_base + COMBODAC_CTRL_0 + i*4);
		mb(); /* Acts as delay and ensures compiler does not optimize*/
		__raw_writel(DAC_CTRL_VAL, dac_base + COMBODAC_CTRL_0 + i*4);
		usleep(1000);

		read_data = __raw_readl(dac_base + COMBODAC_STATUS_0 + i*4);

		if (read_data != DAC_CAL_DONE)
			pr_info("%s: DAC %d = 0x%x\n", __func__, i, read_data);

		__raw_writel(DAC_CTRL_VAL, dac_base + COMBODAC_CTRL_0 + i*4);
		__raw_writel(DAC_CFG0_VAL, dac_base + COMBODAC_CONFIG_0 + i*4);
		__raw_writel(DAC_CFG1_VAL, dac_base + COMBODAC_CONFIG_1 + i*4);
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
	case BBIF_IOCTL_GET: {
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
		break;
	}
	case BBIF_IOCTL_SET: {
		struct bbif_param param;

		if (copy_from_user(&param, argp, sizeof(param))) {
			pr_err("%s: copy_from_user error\n", __func__);
			return -EFAULT;
		}

		if (param.offset > BBIF_MAX_OFFSET) {
			pr_err("%s: Exceeds max offset\n", __func__);
			return -EFAULT;
		}

		__raw_writel(param.value, bbif_adc_base + param.offset);
		break;
	}
	case BBIF_IOCTL_SET_ADC_BW: {
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
		break;
	}
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

static int bbif_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *mem_res;
	int ret;
	int i;
	void __iomem *bbif_misc_base;

	pr_debug("%s: Entry\n", __func__);

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

	for (i = 0; i < BBIF_MAX_ADC; i++) {
		__raw_writel(0, bbif_misc_base + BBIF_ADC_CFG + i*4);
		__raw_writel(BBIF_BBRX_TEST1, bbif_misc_base +
			BBIF_BBRX_TEST1_BASE + i*4);
		__raw_writel(BBIF_BBRX_TEST2, bbif_misc_base +
			BBIF_BBRX_TEST2_BASE + i*4);
		__raw_writel(BBIF_BBRX_TEST3, bbif_misc_base +
			BBIF_BBRX_TEST3_BASE + i*4);
		__raw_writel(BBIF_CONFIG_1, bbif_misc_base +
			BBIF_BBRX_CONFIG_BASE + i*4);
		__raw_writel(BBIF_CONFIG_2, bbif_misc_base +
			BBIF_BBRX_CONFIG2_BASE + i*4);
		__raw_writel(BBIF_CONTROL_1, bbif_misc_base +
			BBIF_BBRX_CONTROL_BASE + i*4);
		usleep(100000);
		__raw_writel(BBIF_CONTROL_2, bbif_misc_base +
			BBIF_BBRX_CONTROL_BASE + i*4);
	}

	/* DAC config */
	bbif_combodac_cfg();

	return of_platform_populate(np, NULL, NULL, &pdev->dev);
}

static int bbif_remove(struct platform_device *pdev)
{

	misc_deregister(bbif_misc_dev);

	return 0;
}

static struct of_device_id bbif_match_table[] = {
	{ .compatible = "qcom,bbif-fsm9010" },
	{}
};
MODULE_DEVICE_TABLE(of, bbif_match_table);

static struct platform_driver bbif_driver = {
	.probe          = bbif_probe,
	.remove         = bbif_remove,
	.driver         = {
			.name   = "bbif-fsm9010",
			.owner   = THIS_MODULE,
			.of_match_table = bbif_match_table,
	},
};

static int __init bbif_init(void)
{
	int rc;

	pr_debug("%s: bbif-fsm9010 driver init\n",  __func__);
	rc = platform_driver_register(&bbif_driver);
	return rc;

}

static void __exit bbif_exit(void)
{
	pr_debug("%s: bbif-fsm9010 driver exit\n",  __func__);
	platform_driver_unregister(&bbif_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. fsm9010 BBIF driver");

module_init(bbif_init);
module_exit(bbif_exit);

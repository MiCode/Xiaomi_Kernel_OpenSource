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
 *
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
#include <linux/gpio.h>
#include <linux/ssbi.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/fsm_rfic.h>
#include <mach/msm_iomap.h>
#include <mach/gpiomux.h>
#include <mach/board.h>

#define GENI_OFFSET			  0x4000
#define GENI_CFG_REG0_ADDR		  0x100
#define GENI_RAM_ADDR			  0x200

#define GENI_CLK_CONTROL_ADDR		  0x0
#define GENI_FW_REVISION_ADDR		  0x8
#define GENI_S_FW_REVISION_ADDR		  0xC
#define GENI_FORCE_DEFAULT_REG_ADDR       0x10
#define GENI_OUTPUT_CONTROL_ADDR	  0x14
#define GENI_ARB_MISC_CONFIG_ADDR	  0x3004
#define GENI_ARB_CHNL_CONFIG_ADDDR	  0x3804

#define PVC_INTF_EN	0x1
#define PVC_INTF_CTL	0x3008
#define PVC_PORT_EN	0x3
#define PVC_PORT_CTL	0x3200
#define ARB_PRI_VAL	0x3f
#define SSBI_ARB_PRI	0x3100
#define PVC_ADDR1	0x3400
#define PVC_ADDR2	0x3404
#define PVC_INDEX_MAX   9

#define FTR_RXGAIN_REG1  0xB8
#define FTR_RXGAIN_REG2  0xB9
#define WTR_RXGAIN_REG1  0x80
#define WTR_RXGAIN_REG2  0x81
#define MTR_RXGAIN_REG   0x7E
#define MTR_RXGAIN_NL    0x102
/*
 * FTR8700, WTR1605  RFIC
 */
#define RFIC_DEVICE_NUM			6
#define RFIC_GRFC_REG_NUM		6

#define ANY_BUS				0x0
#define TX1_BUS				0x0
#define TX2_BUS				0x1
#define MISC_BUS			0x2
#define RX_BUS				0x3
#define BUS_BITS			0x3

#define FTR2_HB_NL_SEL			96
#define FTR2_LB_NL_SEL			97
#define FTR1_HB_NL_SEL			98
#define FTR1_LB_NL_SEL			99
#define FTR1_SBI_SEL0			103
#define FTR1_SBI_SEL1			104
#define FTR2_SBI_SEL0			105
#define FTR2_SBI_SEL1			106

#define PDM_1_0_CTL	0x0000
#define PDM_1_1_CTL	0x1000
#define PDM_1_2_CTL	0x2000
#define PDM_2_0_CTL	0x4000
#define PDM_2_1_CTL	0x5000
#define PDM_2_2_CTL	0x6000
#define PDM_OE		1
#define MSM_MAX_GPIO    32

#define RF_MAX_WF_SIZE  0xA00000
#define RF_MAX_I2C_SIZE  8192

#define GPIO_DIR_SHIFT	9
#define GPIO_DRV_SHIFT	6
#define GPIO_FUNC_SHIFT	2
#define GPIO_OFFSET	0x1000
#define GPIO_MAX_FIELD	0x10
#define GPIO_MAX_DIR    1
#define GPIO_MAX_DRV    8
#define GPIO_MAX_PULL   3

uint16_t slave_address = 0x52;
uint8_t rfbid = 0x33;
static void __iomem *grfc_base;
static void __iomem *pdm_base;
static void __iomem *wf_base;
static void __iomem *gpio_base;
/*
 * Device private information per device node
 */
static struct mutex rficlock;

static struct ftr_dev_node_info {
	void *grfcctrladdr;
	void *grfcmaskaddr;
	void *pvcaddr;
	unsigned int busselect[4];
	int maskvalue;
	struct device *dev;
	struct mutex lock;
} ftr_dev_info[RFIC_DEVICE_NUM];

/*
 * Device private information per file
 */
struct ftr_dev_file_info {
	int ftrid;
};

static int n_dev;
/*
 * File interface
 */

static int ftr_find_id(int minor);

static ssize_t rfb_id_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, sizeof(int), "%d\n", (int)rfbid);
}

static DEVICE_ATTR(rfboard_id, S_IRUGO, rfb_id_show, NULL);


static int ftr_open(struct inode *inode, struct file *file)
{
	struct ftr_dev_file_info *pdfi;

	/* private data allocation */
	pdfi = kmalloc(sizeof(*pdfi), GFP_KERNEL);
	if (pdfi == NULL)
		return -ENOMEM;
	file->private_data = pdfi;

	/* FTR ID */
	pdfi->ftrid = ftr_find_id(MINOR(inode->i_rdev));

	return 0;
}

static int ftr_release(struct inode *inode, struct file *file)
{
	struct ftr_dev_file_info *pdfi;

	pdfi = (struct ftr_dev_file_info *) file->private_data;

	kfree(file->private_data);
	file->private_data = NULL;

	return 0;
}

static long ftr_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	unsigned int __user *argp = (unsigned int __user *) arg;
	struct ftr_dev_file_info *pdfi =
		(struct ftr_dev_file_info *) file->private_data;
	struct ftr_dev_node_info *pdev;

	if (pdfi->ftrid < 0 || pdfi->ftrid >= RFIC_DEVICE_NUM)
		return -EINVAL;

	pdev = ftr_dev_info + pdfi->ftrid;

	switch (cmd) {
	case RFIC_IOCTL_READ_REGISTER: {
		int ret = 0;
		int rficaddr;
		u8 value = 0;

		if (get_user(rficaddr, argp))
			return -EFAULT;

		mutex_lock(&pdev->lock);

		ret = ssbi_read(pdev->dev->parent,
			RFIC_FTR_GET_ADDR(rficaddr), &value, 1);

		mutex_unlock(&pdev->lock);

		if (ret)
			return ret;

		if (put_user(value, argp))
			return -EFAULT;
		break;
	}
	case RFIC_IOCTL_WRITE_REGISTER: {
		int ret;
		struct rfic_write_register_param param;
		unsigned int rficaddr;
		u8 value;

		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;
		rficaddr = param.rficaddr;
		value = (u8) param.value;

		mutex_lock(&pdev->lock);

		ret = ssbi_write(pdev->dev->parent,
			RFIC_FTR_GET_ADDR(rficaddr), &value, 1);

		mutex_unlock(&pdev->lock);

		if (ret)
			return ret;
		break;
	}
	case RFIC_IOCTL_WRITE_REGISTER_WITH_MASK: {
		int ret;
		struct rfic_write_register_mask_param param;
		unsigned int rficaddr;
		u8 value;

		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;

		rficaddr = param.rficaddr;

		mutex_lock(&pdev->lock);

		ret = ssbi_read(pdev->dev->parent,
			RFIC_FTR_GET_ADDR(rficaddr), &value, 1);

		value &= (u8) ~param.mask;
		value |= (u8) (param.value & param.mask);
		ret = ssbi_write(pdev->dev->parent,
			RFIC_FTR_GET_ADDR(rficaddr), &value, 1);

		mutex_unlock(&pdev->lock);
		break;
	}
	case RFIC_IOCTL_WRITE_PVC_REGISTER: {
		struct rfic_write_register_param param;
		unsigned int ssbiaddr;
		u8 pvc_index;

		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;

		ssbiaddr = param.rficaddr;
		pvc_index = (u8) param.value;

		if (pvc_index > PVC_INDEX_MAX) {
			pr_err("%s: %d PVC index exceeds max (10)\n",
				__func__, pvc_index);
			return -EINVAL;
		}

		mutex_lock(&pdev->lock);

		writel_relaxed(PVC_INTF_EN, pdev->pvcaddr + PVC_INTF_CTL);
		writel_relaxed(PVC_PORT_EN, pdev->pvcaddr + PVC_PORT_CTL);
		writel_relaxed(ARB_PRI_VAL, pdev->pvcaddr + SSBI_ARB_PRI);
		writel_relaxed(ssbiaddr, pdev->pvcaddr + PVC_ADDR1 +
			pvc_index * 4);

		mutex_unlock(&pdev->lock);

		return 0;
		break;
	}
	case RFIC_IOCTL_WRITE_PVC_REGISTER_WITH_BUS: {
		struct rfic_write_register_param param;
		unsigned int ssbiaddr;
		u8 pvc_index;

		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;

		ssbiaddr = param.rficaddr;
		pvc_index = (u8) param.value;

		if (pvc_index > PVC_INDEX_MAX) {
			pr_err("%s: %d PVC index exceeds max (10)\n",
				__func__, pvc_index);
			return -EINVAL;
		}

		mutex_lock(&pdev->lock);

		if (((pdfi->ftrid == 1) || (pdfi->ftrid == 2))
				&& (rfbid < RF_TYPE_48)) {
			__raw_writel(
				pdev->busselect[RFIC_FTR_GET_BUS(ssbiaddr)],
				pdev->grfcctrladdr);
		}
		writel_relaxed(PVC_INTF_EN, pdev->pvcaddr + PVC_INTF_CTL);
		writel_relaxed(PVC_PORT_EN, pdev->pvcaddr + PVC_PORT_CTL);
		writel_relaxed(ARB_PRI_VAL, pdev->pvcaddr + SSBI_ARB_PRI);
		writel_relaxed(ssbiaddr, pdev->pvcaddr + PVC_ADDR1 +
			pvc_index * 4);

		mutex_unlock(&pdev->lock);

		return 0;
		break;
	}
	case RFIC_IOCTL_GPIO_SETTING: {
		struct rfic_gpio_param param;
		struct gpio_alt_config *alt_config;
		int gp_size, i;
		void *pGP;
		int value;

		if (pdfi->ftrid != 0)
			return -EINVAL;

		if (copy_from_user(&param, argp, sizeof(param))) {
			pr_err("%s: cfu fail for param\n", __func__);
			return -EFAULT;
		}

		if ((param.num < 1) || (param.num > MSM_MAX_GPIO)) {
			pr_err("Invalid GPIO count %d\n", param.num);
			return -EINVAL;
		}

		gp_size = sizeof(struct gpio_alt_config) * param.num;

		pGP = kmalloc(gp_size, GFP_KERNEL);
		if (pGP == NULL)
			return -ENOMEM;

		if (copy_from_user(pGP, param.pArray, gp_size)) {
			pr_err("%s: cfu fail for pGP\n", __func__);
			kfree(pGP);
			return -EFAULT;
		}

		alt_config = (struct gpio_alt_config *)pGP;

		if ((alt_config->dir > GPIO_MAX_DIR) ||
			(alt_config->drv > GPIO_MAX_DRV) ||
			(alt_config->func > GPIO_MAX_FIELD) ||
			(alt_config->pull > GPIO_MAX_PULL)) {
			pr_err("%s: Field vaules incorrect!\n", __func__);
			kfree(pGP);
			return -EFAULT;
		}

		for (i = 0; i < param.num; i++) {
			value = (alt_config->dir << GPIO_DIR_SHIFT) |
				(alt_config->drv << GPIO_DRV_SHIFT) |
				(alt_config->func << GPIO_FUNC_SHIFT) |
				(alt_config->pull);
			__raw_writel(value, (gpio_base + GPIO_OFFSET +
					(alt_config->gpio * GPIO_MAX_FIELD)));
			alt_config++;
		}
		kfree(pGP);
		break;
	}
	case RFIC_IOCTL_GET_GRFC: {
		struct rfic_grfc_param param;

		if (pdfi->ftrid != 0)
			return -EINVAL;

		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;

		if (param.grfcid >= RFIC_GRFC_REG_NUM)
			return -EINVAL;

		param.maskvalue = __raw_readl(
			grfc_base + 0x20 + param.grfcid * 4);
		param.ctrlvalue = __raw_readl(
			grfc_base + param.grfcid * 4);

		if (copy_to_user(argp, &param, sizeof(param)))
			return -EFAULT;
		break;
	}
	case RFIC_IOCTL_SET_GRFC: {
		struct rfic_grfc_param param;

		if (pdfi->ftrid != 0)
			return -EINVAL;

		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;

		if (param.grfcid >= RFIC_GRFC_REG_NUM)
			return -EINVAL;

		__raw_writel(0, grfc_base + 0x20 + param.grfcid * 4);
		__raw_writel(param.ctrlvalue,
			grfc_base + param.grfcid * 4);
		__raw_writel(param.maskvalue,
			grfc_base + 0x20 + param.grfcid * 4);
		break;
	}
	case RFIC_IOCTL_SET_WFM: {
		struct rfic_wfm_param param;
		unsigned int p_sum;

		if (pdfi->ftrid != 0)
			return -EINVAL;

		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;

		/* Check for integer overflow */
		if (param.offset > UINT_MAX - param.num)
			return -EINVAL;

		p_sum = param.offset + param.num;

		if (p_sum < param.offset || p_sum < param.num)
			return -EINVAL;

		if (p_sum  >  RF_MAX_WF_SIZE)
			return -EINVAL;

		if (copy_from_user(wf_base + param.offset,
					param.pArray, param.num))
			return -EFAULT;
		break;
	}
	case RFIC_IOCTL_SET_BOARDID:
		if (pdfi->ftrid != 0)
			return -EINVAL;

		if (get_user(rfbid, argp))
			return -EFAULT;
		break;

	case RFIC_IOCTL_GET_BOARDID:
		if (pdfi->ftrid != 0)
			return -EINVAL;

		if (put_user(rfbid, argp))
			return -EFAULT;
		break;

	case RFIC_IOCTL_GET_PDM: {
		struct pdm_param param;
		void __iomem *pdmaddr;
		int num;

		if (copy_from_user(&param, argp, sizeof(param))) {
			pr_err("%s: CFU\n", __func__);
			return -EFAULT;
		}

		if ((pdfi->ftrid != 0)  || (param.num > 5)) {
			pr_err("%s: ftrid %d num =%d\n", __func__,
				pdfi->ftrid, param.num);
			return -EINVAL;
		}

		mutex_lock(&pdev->lock);

		if (param.num > 2)
			num = param.num + 1;
		else
			num = param.num;

		pdmaddr = pdm_base + PDM_1_1_CTL * num;
		param.enable = __raw_readl(pdmaddr);
		param.value = __raw_readl(pdmaddr + 4);

		mutex_unlock(&pdev->lock);

		if (copy_to_user(argp, &param, sizeof(param))) {
			pr_err("%s: CTU\n", __func__);
			return -EFAULT;
		}
		return 0;
		break;
	}
	case RFIC_IOCTL_SET_PDM: {
		struct pdm_param param;
		void __iomem *pdmaddr;
		u16 value;
		int num;

		if (copy_from_user(&param, argp, sizeof(param))) {
			pr_err("%s: CFU\n", __func__);
			return -EFAULT;
		}

		if ((pdfi->ftrid != 0)  || (param.num > 5)) {
			pr_err("%s: Invalid id or num\n", __func__);
			return -EINVAL;
		}

		mutex_lock(&pdev->lock);

		if (param.num > 2)
			num = param.num + 1;
		else
			num = param.num;

		value = (u16) param.value;
		pdmaddr = pdm_base + PDM_1_1_CTL * num;
		__raw_writel(param.enable, pdmaddr);
		if (param.enable)
			__raw_writel(value, pdmaddr + 4);
		mutex_unlock(&pdev->lock);
		return 0;
		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations ftr_fops = {
	.owner = THIS_MODULE,
	.open = ftr_open,
	.release = ftr_release,
	.unlocked_ioctl = ftr_ioctl,
};

/* Driver initialization & cleanup */
static struct miscdevice mtr_misc_dev[RFIC_DEVICE_NUM] = {
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = GRFC_DEVICE_NAME,
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_MTR_DEVICE_NAME "0",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_MTR_DEVICE_NAME "1",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_WGR_DEVICE_NAME,
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_MTR_DEVICE_NAME "2",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_MTR_DEVICE_NAME "3",
		.fops = &ftr_fops,
	},
};

static int ftr_find_id(int minor)
{
	int i;

	for (i = 0; i < RFIC_DEVICE_NUM; ++i)
		if (mtr_misc_dev[i].minor == minor)
			break;
	return i;
}

static void pdm_mtr_enable(void)
{
	__raw_writel(PDM_OE, pdm_base + PDM_1_1_CTL);
	__raw_writel(0xb2, pdm_base + PDM_1_1_CTL + 4);
	__raw_writel(PDM_OE, pdm_base + PDM_1_2_CTL);
	__raw_writel(0xb200, pdm_base + PDM_1_2_CTL + 4);
}

static void rfic_pvc_enable(void __iomem *pvc_addr)
{
	writel_relaxed(PVC_INTF_EN, pvc_addr + PVC_INTF_CTL);
	writel_relaxed(PVC_PORT_EN, pvc_addr + PVC_PORT_CTL);
	writel_relaxed(ARB_PRI_VAL, pvc_addr + SSBI_ARB_PRI);
	writel_relaxed(MTR_RXGAIN_REG, pvc_addr + PVC_ADDR1);
	writel_relaxed(MTR_RXGAIN_NL, pvc_addr + PVC_ADDR2);
}

static int ftr_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ftr_dev_node_info *ptr;
	struct resource *mem_res;
	struct clk *pdm_clk, *pdm2_clk;
	int ret;

	pr_debug("%s: me = %p, parent = %p\n",
		__func__, pdev, pdev->dev.parent);

	pdm_clk = NULL;
	pdm2_clk = NULL;

	mutex_lock(&rficlock);

	if (n_dev >= RFIC_DEVICE_NUM) {
		pr_warn("%s: Invalid devices %d\n", __func__, n_dev);
		mutex_unlock(&rficlock);
		return -EINVAL;
	}

	if (!n_dev) {
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		grfc_base = devm_ioremap_resource(&pdev->dev, mem_res);
		if (IS_ERR(grfc_base)) {
			mutex_unlock(&rficlock);
			return PTR_ERR(grfc_base);
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		wf_base = devm_ioremap_resource(&pdev->dev, mem_res);
		if (IS_ERR(wf_base)) {
			mutex_unlock(&rficlock);
			return PTR_ERR(wf_base);
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		pdm_base = devm_ioremap_resource(&pdev->dev, mem_res);
		if (IS_ERR(pdm_base)) {
			mutex_unlock(&rficlock);
			return PTR_ERR(pdm_base);
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
		gpio_base = devm_ioremap_resource(&pdev->dev, mem_res);
		if (IS_ERR(gpio_base)) {
			mutex_unlock(&rficlock);
			return PTR_ERR(gpio_base);
		}

		ret = device_create_file(&pdev->dev, &dev_attr_rfboard_id);
		WARN_ON(ret);

		pdm_clk = clk_get(&pdev->dev, "ahb_clk");
		if (IS_ERR(pdm_clk)) {
			pdm_clk = NULL;
			pr_err("%s: AHB CLK is  NULL\n", __func__);
		} else {
			pr_debug("%s: AHB CLK is  0x%x\n", __func__,
				(unsigned int)pdm_clk);
			clk_prepare_enable(pdm_clk);
		}

		pdm2_clk = clk_get(&pdev->dev, "pdm2_clk");
		if (IS_ERR(pdm2_clk)) {
			pdm2_clk = NULL;
			pr_err("%s: PDM2 CLK is  NULL\n", __func__);
		} else {
			pr_debug("%s: PDM2 CLK is  0x%x\n", __func__,
				(unsigned int)pdm2_clk);
			clk_prepare_enable(pdm2_clk);
		}

		pdm_mtr_enable();
		pr_info("%s: BoardId = 0x%x\n", __func__, rfbid);
	}

	ptr = ftr_dev_info + n_dev;
	ptr->dev = &pdev->dev;

	if ((n_dev >= 1)  && (n_dev <= 5)) {
		struct ssbi *ssbi =
		platform_get_drvdata(to_platform_device(pdev->dev.parent));
		ptr->pvcaddr = ssbi->base;
		if ((n_dev == 3)) {
			ssbi->controller_type =
				FSM_SBI_CTRL_GENI_SSBI_ARBITER;
			set_ssbi_mode_1(ssbi->base);
			pr_debug("%s: SSBI1 = 0x%x\n", __func__,
				ssbi->controller_type);
		} else {
			ssbi->controller_type =
				FSM_SBI_CTRL_GENI_SSBI2_ARBITER;
			set_ssbi_mode_2(ssbi->base);
			pr_debug("%s: SSBI2 = 0x%x\n", __func__,
				ssbi->controller_type);
			rfic_pvc_enable(ssbi->base);
		}
		platform_set_drvdata(to_platform_device(pdev->dev.parent),
				ssbi);
	}

	mutex_init(&ptr->lock);

	ret = misc_register(mtr_misc_dev + n_dev);

	if (ret < 0) {
		misc_deregister(mtr_misc_dev + n_dev);
		if (pdm_clk)
			clk_disable(pdm_clk);
		if (pdm2_clk)
			clk_disable(pdm2_clk);
		mutex_unlock(&rficlock);
		return ret;
	}

	n_dev++;
	mutex_unlock(&rficlock);
	pr_debug("%s: dev = %d\n", __func__, n_dev);

	return of_platform_populate(np, NULL, NULL, &pdev->dev);
}


static int ftr_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < RFIC_DEVICE_NUM; ++i)
		misc_deregister(mtr_misc_dev + i);
	return 0;
}

static struct of_device_id rfic_match_table[] = {
	{ .compatible = "qcom,rfic-fsm9010" },
	{}
};
MODULE_DEVICE_TABLE(of, rfic_match_table);

static struct platform_driver ftr_driver = {
	.probe          = ftr_probe,
	.remove         = ftr_remove,
	.driver         = {
			.name   = "rfic-fsm9010",
			.owner   = THIS_MODULE,
			.of_match_table = rfic_match_table,
	},
};

static int __init ftr_init(void)
{
	int rc;

	n_dev = 0;
	mutex_init(&rficlock);
	pr_debug("%s: rfic-fsm9010 driver init\n",  __func__);
	rc = platform_driver_register(&ftr_driver);
	return rc;
}

static void __exit ftr_exit(void)
{
	pr_debug("%s: rfic-fsm9010 driver exit\n",  __func__);
	platform_driver_unregister(&ftr_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. fsm9010 RFIC driver");

module_init(ftr_init);
module_exit(ftr_exit);

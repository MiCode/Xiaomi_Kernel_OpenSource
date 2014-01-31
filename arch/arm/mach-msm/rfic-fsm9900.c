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
#include <mach/msm_iomap.h>
#include <mach/gpiomux.h>
#include <mach/board.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <linux/fsm_rfic.h>

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

/*
 * FTR8700, WTR1605  RFIC
 */

#define RFIC_MTR_DEVICE_NUM		5
#define RFIC_DEVICE_NUM			9
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
#define PDM_BYTE	0x80
#define PDM_WORD	0x8000
#define MSM_MAX_GPIO    32

uint8_t rfbid;
void __iomem *grfc_base;
void __iomem *pdm_base;
/*
 * Device private information per device node
 */
struct mutex rficlock;

static struct ftr_dev_node_info {
	void *grfcctrladdr;
	void *grfcmaskaddr;
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

int nDev;
/*
 * File interface
 */

static int ftr_find_id(int minor);

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
	case RFIC_IOCTL_READ_REGISTER:
		{
		    int ret = 0;
		    unsigned int rficaddr;
		    u8 value = 0;

		    if (get_user(rficaddr, argp))
			return -EFAULT;

		    mutex_lock(&pdev->lock);

		    if (((pdfi->ftrid == 2) || (pdfi->ftrid == 3))
				&& (rfbid < RF_TYPE_48)) {
			__raw_writel(
				pdev->busselect[RFIC_FTR_GET_BUS(rficaddr)],
				pdev->grfcctrladdr);
			mb();
		    }

		    ret = ssbi_read(pdev->dev->parent,
			RFIC_FTR_GET_ADDR(rficaddr), &value, 1);

		    mutex_unlock(&pdev->lock);

		    if (ret)
			return ret;

		    if (put_user(value, argp))
			return -EFAULT;
}

	break;

	case RFIC_IOCTL_WRITE_REGISTER:
		{
		    int ret;
		    struct rfic_write_register_param param;
		    unsigned int rficaddr;
		    u8 value;

		    if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;
		    rficaddr = param.rficaddr;
		    value = (u8) param.value;

		    mutex_lock(&pdev->lock);

		    if (((pdfi->ftrid == 2) || (pdfi->ftrid == 3))
				&& (rfbid < RF_TYPE_48)) {
			__raw_writel(
				pdev->busselect[RFIC_FTR_GET_BUS(rficaddr)],
				pdev->grfcctrladdr);
			mb();
		    }

		    ret = ssbi_write(pdev->dev->parent,
			RFIC_FTR_GET_ADDR(rficaddr), &value, 1);

		    mutex_unlock(&pdev->lock);

		    if (ret)
			return ret;
		}
		break;

	case RFIC_IOCTL_WRITE_REGISTER_WITH_MASK:
		{
			int ret;
			struct rfic_write_register_mask_param param;
			unsigned int rficaddr;
			u8 value;

			if (copy_from_user(&param, argp, sizeof(param)))
				return -EFAULT;

			rficaddr = param.rficaddr;

			mutex_lock(&pdev->lock);

			if (((pdfi->ftrid == 2) || (pdfi->ftrid == 3))
				&& (rfbid < RF_TYPE_48)) {
				__raw_writel(
				pdev->busselect[RFIC_FTR_GET_BUS(rficaddr)],
				pdev->grfcctrladdr);
				mb();
			}

			ret = ssbi_read(pdev->dev->parent,
				RFIC_FTR_GET_ADDR(rficaddr), &value, 1);

			value &= (u8) ~param.mask;
			value |= (u8) (param.value & param.mask);
			ret = ssbi_write(pdev->dev->parent,
				RFIC_FTR_GET_ADDR(rficaddr), &value, 1);

			mutex_unlock(&pdev->lock);

			if (ret)
				return ret;
		}
		break;

	case RFIC_IOCTL_GPIO_SETTING:
		{
			struct rfic_gpio_param param;
			struct msm_gpiomux_config rf_config[MSM_MAX_GPIO];
			struct gpiomux_setting rf_setting[MSM_MAX_GPIO];
			struct gpio_alt_config *alt_config;
			int gp_size, i;
			void *pGP;

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
			for (i = 0; i < param.num; i++) {
				rf_config[i].gpio = (unsigned)alt_config->gpio;
				rf_setting[i].func = alt_config->func;
				rf_setting[i].drv = alt_config->drv;
				rf_setting[i].pull = alt_config->pull;
				rf_setting[i].dir = alt_config->dir;
				rf_config[i].settings[GPIOMUX_ACTIVE] =
					&rf_setting[i];
				rf_config[i].settings[GPIOMUX_SUSPENDED] =
					&rf_setting[i];
				alt_config++;
			}
			msm_gpiomux_install(rf_config, param.num);
			kfree(pGP);
		}
		break;

	case RFIC_IOCTL_GET_GRFC:
		{
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
		}
		break;

	case RFIC_IOCTL_SET_GRFC:
		{
			struct rfic_grfc_param param;

			if (pdfi->ftrid != 0)
				return -EINVAL;

			if (copy_from_user(&param, argp, sizeof(param)))
				return -EFAULT;

			if (param.grfcid >= RFIC_GRFC_REG_NUM)
				return -EINVAL;

			__raw_writel(param.maskvalue,
				grfc_base + 0x20 + param.grfcid * 4);
			__raw_writel(param.ctrlvalue,
				grfc_base + param.grfcid * 4);
			mb();
		}
		break;

	case RFIC_IOCTL_GET_BOARDID:
		{
			if (pdfi->ftrid != 0)
				return -EINVAL;

			if (put_user(rfbid, argp))
				return -EFAULT;
		}
		break;

	case RFIC_IOCTL_PDM_READ:
		{
			unsigned int pdmaddr;
			u32 value = 0;

			if (get_user(pdmaddr, argp))
				return -EFAULT;

			mutex_lock(&pdev->lock);

			if (pdfi->ftrid == 1)
				value = __raw_readl(pdm_base + pdmaddr);

			mutex_unlock(&pdev->lock);

			if (put_user(value, argp))
				return -EFAULT;

			return 0;
		}
		break;

	case RFIC_IOCTL_PDM_WRITE:
		{
			struct pdm_write_param param;
			unsigned int pdmaddr;
			u8 value;

			if (copy_from_user(&param, argp, sizeof(param)))
				return -EFAULT;

			pdmaddr = (unsigned int)pdm_base + param.offset;
			value = (u8) param.value;

			mutex_lock(&pdev->lock);

			if (pdfi->ftrid == 1) {
				__raw_writel(value, pdmaddr);
				mb();
			}

			mutex_unlock(&pdev->lock);

			return 0;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

uint8_t rf_interface_id(void)
{
	uint8_t tempid = 0;

	tempid = gpio_get_value(FTR1_LB_NL_SEL);
	rfbid = tempid << 7;
	tempid = gpio_get_value(FTR1_HB_NL_SEL);
	rfbid |= tempid << 6;
	tempid = gpio_get_value(FTR2_LB_NL_SEL);
	rfbid |= tempid << 5;
	tempid = gpio_get_value(FTR2_HB_NL_SEL);
	rfbid |= tempid << 4;
	tempid = gpio_get_value(FTR1_SBI_SEL0);
	rfbid |= tempid << 3;
	tempid = gpio_get_value(FTR1_SBI_SEL1);
	rfbid |= tempid << 2;
	tempid = gpio_get_value(FTR2_SBI_SEL0);
	rfbid |= tempid << 1;
	tempid = gpio_get_value(FTR2_SBI_SEL1);
	rfbid |= tempid;

	return rfbid;
}

static const struct file_operations ftr_fops = {
	.owner = THIS_MODULE,
	.open = ftr_open,
	.release = ftr_release,
	.unlocked_ioctl = ftr_ioctl,
};

/* Driver initialization & cleanup */

struct miscdevice ftr_misc_dev[RFIC_DEVICE_NUM] = {
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = GRFC_DEVICE_NAME,
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = PDM_DEVICE_NAME,
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_FTR_DEVICE_NAME "0",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_FTR_DEVICE_NAME "1",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_WTR_DEVICE_NAME "1-rx",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_WTR_DEVICE_NAME "1-tx",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_WTR_DEVICE_NAME "2-tx",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_WTR_DEVICE_NAME "2-rx",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_WGR_DEVICE_NAME,
		.fops = &ftr_fops,
	},
};

struct miscdevice mtr_misc_dev[RFIC_MTR_DEVICE_NUM] = {
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = GRFC_DEVICE_NAME,
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = PDM_DEVICE_NAME,
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
};

int ftr_find_id(int minor)
{
	int i;

	for (i = 0; i < RFIC_DEVICE_NUM; ++i)
		if (ftr_misc_dev[i].minor == minor)
			break;

	return i;
}

static ssize_t rfb_id_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, sizeof(int), "%d\n", (int)rfbid);
}

static DEVICE_ATTR(rfboard_id, S_IRUGO, rfb_id_show, NULL);

static int ftr_regulator_init(struct platform_device *pdev)
{
	int ret;
	struct regulator *vreg_ldo18, *vreg_ldo23, *vreg_ldo25;

	vreg_ldo23 = regulator_get(&pdev->dev, "vdd-ftr1");
	if (IS_ERR(vreg_ldo23)) {
		pr_err("%s: vreg get l23-reg failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo23));
		return PTR_ERR(vreg_ldo23);
	}

	ret = regulator_set_optimum_mode(vreg_ldo23, 250000);
	if (ret < 0) {
		pr_err("%s: unable to set optimum mode for ldo23\n", __func__);
		return PTR_ERR(vreg_ldo23);
	}

	ret = regulator_enable(vreg_ldo23);
	if (ret) {
		pr_err("%s: unable to enable ldo23 voltage\n", __func__);
		return PTR_ERR(vreg_ldo23);
	}

	vreg_ldo25 = regulator_get(&pdev->dev, "vdd-ftr2");
	if (IS_ERR(vreg_ldo25)) {
		pr_err("%s: vreg get l25-reg failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo25));
		return PTR_ERR(vreg_ldo25);
	}

	ret = regulator_set_optimum_mode(vreg_ldo25, 250000);
	if (ret < 0) {
		pr_err("%s: unable to set optimum mode for ldo25\n", __func__);
		return PTR_ERR(vreg_ldo25);
	}

	ret = regulator_enable(vreg_ldo25);
	if (ret) {
		pr_err("%s: unable to enable ldo25 voltage\n", __func__);
		return PTR_ERR(vreg_ldo25);
	}

	vreg_ldo18 = regulator_get(&pdev->dev, "vdd-switch");
	if (IS_ERR(vreg_ldo18)) {
		pr_err("%s: vreg get l18-reg failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo18));
		return PTR_ERR(vreg_ldo18);
	}

	ret = regulator_enable(vreg_ldo18);
	if (ret) {
		pr_err("%s: unable to enable ldo18 voltage\n", __func__);
		return PTR_ERR(vreg_ldo18);
	}

	return 0;
}

static int wtr_regulator_init(struct platform_device *pdev)
{
	int ret;
	struct regulator *vreg_ldo19;

	vreg_ldo19 = regulator_get(&pdev->dev, "vdd-wtr");
	if (IS_ERR(vreg_ldo19)) {
		pr_err("%s: vreg get l19-reg failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo19));
		return PTR_ERR(vreg_ldo19);
	}

	ret = regulator_enable(vreg_ldo19);
	if (ret) {
		pr_err("%s: unable to enable ldo19 voltage\n", __func__);
		return PTR_ERR(vreg_ldo19);
	}

	return 0;
}

static int mantaray_regulator_init(struct platform_device *pdev)
{

	return 0;
}

static void pdm_enable(void)
{
	__raw_writel(PDM_OE, pdm_base + PDM_1_2_CTL);
	__raw_writel(PDM_OE, pdm_base + PDM_2_0_CTL);
	__raw_writel(PDM_OE, pdm_base + PDM_2_1_CTL);
	__raw_writel(PDM_OE, pdm_base + PDM_2_2_CTL);
}


void pdm_mtr_enable(void)
{
	__raw_writel(PDM_OE, pdm_base + PDM_1_2_CTL);
	__raw_writel(0xb200, pdm_base + PDM_1_2_CTL + 4);
	__raw_writel(PDM_OE, pdm_base + PDM_2_0_CTL);
	__raw_writel(0xb200, pdm_base + PDM_2_0_CTL + 4);
	__raw_writel(PDM_OE, pdm_base + PDM_2_1_CTL);
	__raw_writel(0xb2, pdm_base + PDM_2_1_CTL + 4);
	__raw_writel(PDM_OE, pdm_base + PDM_2_2_CTL);
	__raw_writel(0xb200, pdm_base + PDM_2_2_CTL + 4);
}


static int ftr_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ftr_dev_node_info *ptr;
	struct resource *mem_res;
	struct clk *pdm_clk;
	int ret;
	u8 version = 0;

	pr_debug("%s: me = %p, parent = %p\n",
		__func__, pdev, pdev->dev.parent);

	mutex_lock(&rficlock);

	if (nDev >= RFIC_DEVICE_NUM) {
		pr_warn("%s: Invalid devices %d\n", __func__, nDev);
		mutex_unlock(&rficlock);
		return -EINVAL;
	}

	if (!nDev) {
		rfbid = rf_interface_id();
		rfbid = rfbid & RF_TYPE_48;
		pr_info("%s: RF Board Type 0x%x\n", __func__, rfbid);

		switch (rfbid) {
		case RF_TYPE_16:
			ftr_regulator_init(pdev);
			wtr_regulator_init(pdev);
			break;
		case RF_TYPE_32:
			ftr_regulator_init(pdev);
			break;
		case RF_TYPE_48:
			mantaray_regulator_init(pdev);
			break;
		default:
			pr_warn("%s:Regulators may not be turned ON %d\n",
					__func__, rfbid);
		}

		rfbid = rf_interface_id();
		pr_info("%s: RF Board Id 0x%x\n", __func__, rfbid);

		switch (rfbid) {
		case RF_TYPE_33:
			fsm9900_gluon_init();
			break;
		case RF_TYPE_17:
		case RF_TYPE_18:
			fsm9900_rfic_init();
			break;
		default:
			pr_warn("%s:GPIOs not configured %d\n",
					__func__, rfbid);
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		grfc_base = devm_ioremap_resource(&pdev->dev, mem_res);
		if (IS_ERR(grfc_base)) {
			mutex_unlock(&rficlock);
			return PTR_ERR(grfc_base);
		}
		ret = device_create_file(&pdev->dev, &dev_attr_rfboard_id);
		WARN_ON(ret);

	} else if ((nDev == 1) && (rfbid > RF_TYPE_48)) {
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		pdm_base = devm_ioremap_resource(&pdev->dev, mem_res);
		if (IS_ERR(pdm_base)) {
			mutex_unlock(&rficlock);
			return PTR_ERR(pdm_base);
		}
		pdm_clk = clk_get(&pdev->dev, "ahb_clk");
		if (IS_ERR(pdm_clk)) {
			pdm_clk = NULL;
			pr_err("%s: AHB CLK is  NULL\n", __func__);
		} else {
			pr_debug("%s: AHB CLK is  0x%x\n", __func__,
				(unsigned int)pdm_clk);
			clk_prepare(pdm_clk);
			clk_enable(pdm_clk);
		}

		pdm_clk = clk_get(&pdev->dev, "pdm2_clk");
		if (IS_ERR(pdm_clk)) {
			pdm_clk = NULL;
			pr_err("%s: PDM2 CLK is  NULL\n", __func__);
		} else {
			pr_debug("%s: PDM2 CLK is  0x%x\n", __func__,
				(unsigned int)pdm_clk);
			clk_prepare(pdm_clk);
			clk_enable(pdm_clk);
		}

		pdm_mtr_enable();
		pr_info("%s: MTR PDM Enabled\n", __func__);
	} else if ((nDev == 1) && (rfbid > RF_TYPE_16) &&
			(rfbid < RF_TYPE_32)) {
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		pdm_base = devm_ioremap_resource(&pdev->dev, mem_res);
		if (IS_ERR(pdm_base)) {
			mutex_unlock(&rficlock);
			return PTR_ERR(pdm_base);
		}

		pdm_clk = clk_get(&pdev->dev, "ahb_clk");
		if (IS_ERR(pdm_clk)) {
			pdm_clk = NULL;
			pr_err("%s: AHB CLK is  NULL\n", __func__);
		} else {
			clk_prepare(pdm_clk);
			clk_enable(pdm_clk);
		}

		pdm_clk = clk_get(&pdev->dev, "pdm2_clk");
		if (IS_ERR(pdm_clk)) {
			pdm_clk = NULL;
			pr_err("%s: PDM2 CLK is  NULL\n", __func__);
		} else {
			clk_prepare(pdm_clk);
			clk_enable(pdm_clk);
		}

		pdm_enable();
		pr_info("%s: PDM Enabled\n", __func__);
	}

	ptr = ftr_dev_info + nDev;
	ptr->dev = &pdev->dev;

	if (((rfbid & RF_TYPE_16) || ((rfbid & RF_TYPE_32))) && (nDev == 2)) {
		ssbi_write(pdev->dev.parent, 0xff, &version, 1);
		ssbi_read(pdev->dev.parent, 0x2, &version, 1);
		pr_info("%s: FTR1 Version = %02x\n", __func__, version);

		ptr->grfcctrladdr = grfc_base + 0x10; /* grp 4 */
		ptr->grfcmaskaddr = grfc_base + 0x30;
		__raw_writel(0x00001800, ptr->grfcmaskaddr);
		ptr->maskvalue = 0x00001800;
		ptr->busselect[TX1_BUS] = 0x00000000;
		ptr->busselect[TX2_BUS] = 0x00001000;
		ptr->busselect[MISC_BUS] = 0x00000800;
		ptr->busselect[RX_BUS] = 0x00001800;
	} else if (((rfbid & RF_TYPE_16) || ((rfbid & RF_TYPE_32))) &&
		(nDev == 3)) {
		ssbi_write(pdev->dev.parent, 0xff, &version, 1);
		ssbi_read(pdev->dev.parent, 0x2, &version, 1);
		pr_info("%s: FTR2 Version = %02x\n", __func__, version);

		ptr->grfcctrladdr = grfc_base + 0x14; /* grp 5*/
		ptr->grfcmaskaddr = grfc_base + 0x34;
		__raw_writel(0x00000600, ptr->grfcmaskaddr);
		ptr->maskvalue = 0x00000600;
		ptr->busselect[TX1_BUS] = 0x000000;
		ptr->busselect[TX2_BUS] = 0x00000400;
		ptr->busselect[MISC_BUS] = 0x00000200;
		ptr->busselect[RX_BUS] = 0x00000600;
	}

	mutex_init(&ptr->lock);

	if (rfbid < RF_TYPE_48) {
		ret = misc_register(ftr_misc_dev + nDev);

		if (ret < 0) {
			misc_deregister(ftr_misc_dev + nDev);
			mutex_unlock(&rficlock);
			return ret;
		}
	} else {
		ret = misc_register(mtr_misc_dev + nDev);

		if (ret < 0) {
			misc_deregister(mtr_misc_dev + nDev);
			mutex_unlock(&rficlock);
			return ret;
		}
	}

	nDev++;
	pr_info("%s: num_of_ssbi_devices = %d\n", __func__, nDev);
	mutex_unlock(&rficlock);

	return of_platform_populate(np, NULL, NULL, &pdev->dev);
}


static int ftr_remove(struct platform_device *pdev)
{
	int i;

	if (rfbid < RF_TYPE_48) {
		for (i = 0; i < RFIC_DEVICE_NUM; ++i)
			misc_deregister(ftr_misc_dev + i);
	} else {
		for (i = 0; i < RFIC_MTR_DEVICE_NUM; ++i)
			misc_deregister(mtr_misc_dev + i);
	}
	return 0;
}

static struct of_device_id rfic_match_table[] = {
	{ .compatible = "qcom,rfic" },
	{}
};
MODULE_DEVICE_TABLE(of, rfic_match_table);

static struct platform_driver ftr_driver = {
	.probe          = ftr_probe,
	.remove         = ftr_remove,
	.driver         = {
			.name   = "rfic",
			.owner   = THIS_MODULE,
			.of_match_table = rfic_match_table,
	},
};

int __init ftr_init(void)
{
	int rc;

	nDev = 0;
	mutex_init(&rficlock);
	pr_debug("%s: rfic-fsm9900 driver init\n",  __func__);
	rc = platform_driver_register(&ftr_driver);
	return rc;

}

void __exit ftr_exit(void)
{
	pr_debug("%s: rfic-fsm9900 driver exit\n",  __func__);
	platform_driver_unregister(&ftr_driver);
}


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm fsm9900 RFIC driver");

module_init(ftr_init);
module_exit(ftr_exit);

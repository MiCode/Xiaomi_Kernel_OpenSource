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

#define PVC_INTF_EN	0x1
#define PVC_INTF_CTL	0x3008
#define PVC_PORT_EN	0x3
#define PVC_PORT_CTL	0x3200
#define ARB_PRI_VAL	0x3f
#define SSBI_ARB_PRI	0x3100
#define PVC_ADDR1	0x3400
#define PVC_ADDR2	0x3404

#define FTR_RXGAIN_REG1  0xB8
#define FTR_RXGAIN_REG2  0xB9
#define WTR_RXGAIN_REG1  0x80
#define WTR_RXGAIN_REG2  0x81
#define MTR_RXGAIN_REG   0x7E
#define MTR_RXGAIN_NL    0x102
/*
 * FTR8700, WTR1605  RFIC
 */

#define RFIC_DEVICE_NUM			8
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

#define RF_MAX_WF_SIZE  0xA00000

uint8_t rfbid;
void __iomem *grfc_base;
void __iomem *pdm_base;
void __iomem *wf_base;
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

static int n_dev;
/*
 * File interface
 */

static int ftr_find_id(int minor);

static int rf_regulator_init(struct platform_device *pdev, char *reg_name,
		int opt_mode)
{
	int ret;
	struct regulator *vreg_ldo;

	vreg_ldo = regulator_get(&pdev->dev, reg_name);
	if (IS_ERR(vreg_ldo)) {
		pr_err("%s: vreg get %s failed (%ld)\n",
			__func__, reg_name, PTR_ERR(vreg_ldo));
		return PTR_ERR(vreg_ldo);
	}

	if (opt_mode) {
		ret = regulator_set_optimum_mode(vreg_ldo, 325000);
		if (ret < 0) {
			pr_err("%s: unable to set optimum mode for %s\n",
				__func__, reg_name);
			return PTR_ERR(vreg_ldo);
		}
	}
	ret = regulator_enable(vreg_ldo);
	if (ret) {
		pr_err("%s: unable to enable %s voltage\n", __func__, reg_name);
		return PTR_ERR(vreg_ldo);
	}

	return 0;
}

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
	case RFIC_IOCTL_READ_REGISTER:
		{
		    int ret = 0;
		    unsigned int rficaddr;
		    u8 value = 0;

		    if (get_user(rficaddr, argp))
			return -EFAULT;

		    mutex_lock(&pdev->lock);

		    if (((pdfi->ftrid == 1) || (pdfi->ftrid == 2))
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

		    if (((pdfi->ftrid == 1) || (pdfi->ftrid == 2))
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

			if (((pdfi->ftrid == 1) || (pdfi->ftrid == 2))
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

			__raw_writel(0, grfc_base + 0x20 + param.grfcid * 4);
			__raw_writel(param.ctrlvalue,
				grfc_base + param.grfcid * 4);
			__raw_writel(param.maskvalue,
				grfc_base + 0x20 + param.grfcid * 4);
			mb();
		}
		break;

	case RFIC_IOCTL_SET_WFM:
		{
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
		}
		break;

	case RFIC_IOCTL_SET_LDO:
		{
			unsigned int ldo;

			if (pdfi->ftrid != 0) {
				pr_err("%s: Invalid id %d\n", __func__,
					pdfi->ftrid);
				return -EINVAL;
			}

			if (get_user(ldo, argp)) {
				pr_err("%s: Invalid ldo %d\n", __func__, ldo);
				return -EFAULT;
			}

			switch (ldo) {
			case LDO11:
				if (rf_regulator_init(to_platform_device
					(pdev->dev), "vdd-1v3", 0) != 0)
					pr_err("%s: LDO11 fail\n", __func__);
					break;
			case LDO18:
				if (rf_regulator_init(to_platform_device
					(pdev->dev), "vdd-switch", 0) != 0)
					pr_err("%s: LDO18 fail\n", __func__);
					break;
			case LDO19:
				if (rf_regulator_init(to_platform_device
					(pdev->dev), "vdd-wtr", 0) != 0)
					pr_err("%s: LDO19 fail\n", __func__);
					break;
			case LDO23:
				if (rf_regulator_init(to_platform_device
					(pdev->dev), "vdd-ftr1", 1) != 0)
					pr_err("%s: LDO23 fail\n", __func__);
					break;
			case LDO25:
				if (rf_regulator_init(to_platform_device
					(pdev->dev), "vdd-ftr2", 1) != 0)
					pr_err("%s: LDO25 fail\n", __func__);
					break;
			case LDO26:
				if (rf_regulator_init(to_platform_device
					(pdev->dev), "vdd-1v8", 1) != 0)
					pr_err("%s: LDO26 fail\n", __func__);
					break;
			default:
					pr_err("%s: Unknown LDO\n", __func__);
					break;
			}
		}
		break;

	case RFIC_IOCTL_SET_BOARDID:
		{
			if (pdfi->ftrid != 0)
				return -EINVAL;

			if (get_user(rfbid, argp))
				return -EFAULT;
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

	case RFIC_IOCTL_GET_PDM:
		{
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
		}
		break;

	case RFIC_IOCTL_SET_PDM:
		{
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

struct miscdevice mtr_misc_dev[RFIC_DEVICE_NUM] = {
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
		.name = RFIC_MTR_DEVICE_NAME "2",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_MTR_DEVICE_NAME "3",
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
		.name = RFIC_WGR_DEVICE_NAME,
		.fops = &ftr_fops,
	},
};

int ftr_find_id(int minor)
{
	int i;

	if (rfbid < RF_TYPE_48) {
		for (i = 0; i < RFIC_DEVICE_NUM; ++i)
			if (ftr_misc_dev[i].minor == minor)
				break;
	} else {
		for (i = 0; i < RFIC_DEVICE_NUM; ++i)
			if (mtr_misc_dev[i].minor == minor)
				break;
	}

	return i;
}

static int ftr_regulator_init(struct platform_device *pdev)
{
	int ret;

	ret = (rf_regulator_init(pdev, "vdd-1v8", 1));
		if (ret)
			return ret;
	ret = (rf_regulator_init(pdev, "vdd-1v3", 0));
		if (ret)
			return ret;
	ret = (rf_regulator_init(pdev, "vdd-ftr1", 1));
		if (ret)
			return ret;
	ret = (rf_regulator_init(pdev, "vdd-ftr2", 1));
		if (ret)
			return ret;
	ret = (rf_regulator_init(pdev, "vdd-switch", 0));
		if (ret)
			return ret;
	ret = (rf_regulator_init(pdev, "vdd-wtr", 0));
		if (ret)
			return ret;

	return 0;
}

static int glu_regulator_init(struct platform_device *pdev)
{
	int ret;

	ret = (rf_regulator_init(pdev, "vdd-1v3", 0));
		if (ret)
			return ret;
	ret = (rf_regulator_init(pdev, "vdd-ftr1", 1));
		if (ret)
			return ret;
	ret = (rf_regulator_init(pdev, "vdd-ftr2", 1));
		if (ret)
			return ret;
	ret = (rf_regulator_init(pdev, "vdd-switch", 0));
		if (ret)
			return ret;

	return 0;
}

static int mtr_regulator_init(struct platform_device *pdev)
{
	int ret;

	ret = (rf_regulator_init(pdev, "vdd-1v8", 1));
		if (ret)
			return ret;

	udelay(500); /* Power-up sequence as per Mray spec */

	ret = (rf_regulator_init(pdev, "vdd-ftr1", 1));
		if (ret)
			return ret;

	ret = (rf_regulator_init(pdev, "vdd-ftr2", 1));
		if (ret)
			return ret;

	udelay(500); /* Power-up sequence as per Mray spec */

	ret = (rf_regulator_init(pdev, "vdd-1v3", 0));
		if (ret)
			return ret;

	ret = (rf_regulator_init(pdev, "vdd-switch", 0));
		if (ret)
			return ret;

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
	__raw_writel(PDM_OE, pdm_base + PDM_2_0_CTL);
	__raw_writel(0xb200, pdm_base + PDM_2_0_CTL + 4);
	__raw_writel(PDM_OE, pdm_base + PDM_2_1_CTL);
	__raw_writel(0xb2, pdm_base + PDM_2_1_CTL + 4);
}

void rfic_pvc_enable(void __iomem *pvc_addr, int rfic)
{
	writel_relaxed(PVC_INTF_EN, pvc_addr + PVC_INTF_CTL);
	writel_relaxed(PVC_PORT_EN, pvc_addr + PVC_PORT_CTL);
	writel_relaxed(ARB_PRI_VAL, pvc_addr + SSBI_ARB_PRI);
	switch (rfic) {
	case 1:
		writel_relaxed(FTR_RXGAIN_REG1, pvc_addr + PVC_ADDR1);
		writel_relaxed(FTR_RXGAIN_REG2, pvc_addr + PVC_ADDR2);
		break;
	case 2:
		writel_relaxed(WTR_RXGAIN_REG1, pvc_addr + PVC_ADDR1);
		writel_relaxed(WTR_RXGAIN_REG2, pvc_addr + PVC_ADDR2);
		break;
	case 3:
		writel_relaxed(MTR_RXGAIN_REG, pvc_addr + PVC_ADDR1);
		writel_relaxed(MTR_RXGAIN_NL, pvc_addr + PVC_ADDR2);
		break;
	default:
		pr_err("%s: Unsupported RFIC\n", __func__);
		break;
	}
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

	if (n_dev >= RFIC_DEVICE_NUM) {
		pr_warn("%s: Invalid devices %d\n", __func__, n_dev);
		mutex_unlock(&rficlock);
		return -EINVAL;
	}

	if (!n_dev) {
		rfbid = rf_interface_id();
		if ((rfbid != 0xff) && (rfbid != 0))
			rfbid = rfbid & RF_TYPE_48;

		switch (rfbid) {
		case RF_TYPE_16:
			ftr_regulator_init(pdev);
			break;
		case RF_TYPE_32:
			glu_regulator_init(pdev);
			break;
		case RF_TYPE_48:
			mtr_regulator_init(pdev);
			break;
		default:
			pr_warn("%s:Regulators not turned ON %d\n",
					__func__, rfbid);
		}

		rfbid = rf_interface_id();
		pr_info("%s: RF Board Id 0x%x\n", __func__, rfbid);

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

		ret = device_create_file(&pdev->dev, &dev_attr_rfboard_id);
		WARN_ON(ret);

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

		if ((rfbid > RF_TYPE_48) && (rfbid != 0xff)) {
			fsm9900_mtr_init();
			pdm_mtr_enable();
			pr_info("%s: MTR PDM Enabled\n", __func__);
		} else if ((rfbid > RF_TYPE_16) && (rfbid < RF_TYPE_32)) {
			fsm9900_rfic_init();
			pdm_enable();
			pr_info("%s: PDM Enabled\n", __func__);
		} else if ((rfbid > RF_TYPE_32) && (rfbid < RF_TYPE_48)) {
			fsm9900_gluon_init();
			pr_info("%s: Gluon Enabled\n", __func__);
		} else {
			pr_warn("%s:PDMs not configured %d\n",
					__func__, rfbid);
		}

	}

	ptr = ftr_dev_info + n_dev;
	ptr->dev = &pdev->dev;

	if ((n_dev >= 1)  && (n_dev <= 7)) {
		struct ssbi *ssbi =
		platform_get_drvdata(to_platform_device(pdev->dev.parent));
		if ((rfbid > RF_TYPE_48) && (n_dev <= 4)) {
			ssbi->controller_type =
				FSM_SBI_CTRL_GENI_SSBI2_ARBITER;
			set_ssbi_mode_2(ssbi->base);
			pr_debug("%s: SSBI2 = 0x%x\n", __func__,
				ssbi->controller_type);
			rfic_pvc_enable(ssbi->base, 3);
		} else {
			ssbi->controller_type =
				FSM_SBI_CTRL_GENI_SSBI_ARBITER;
			set_ssbi_mode_1(ssbi->base);
			pr_debug("%s: SSBI1 = 0x%x\n", __func__,
				ssbi->controller_type);
			if ((n_dev == 1) || (n_dev == 2))
				rfic_pvc_enable(ssbi->base, 1);
			if ((n_dev == 3) && (rfbid > RF_TYPE_16)
					&& (rfbid < RF_TYPE_32))
				rfic_pvc_enable(ssbi->base, 2);
		}
		platform_set_drvdata(to_platform_device(pdev->dev.parent),
				ssbi);
	}

	if ((rfbid > RF_TYPE_16) && (rfbid < RF_TYPE_48) && (n_dev == 1)) {
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
	} else if ((rfbid > RF_TYPE_16) && (rfbid < RF_TYPE_48) &&
		(n_dev == 2)) {
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
		ret = misc_register(ftr_misc_dev + n_dev);

		if (ret < 0) {
			misc_deregister(ftr_misc_dev + n_dev);
			mutex_unlock(&rficlock);
			return ret;
		}
	} else {
		ret = misc_register(mtr_misc_dev + n_dev);

		if (ret < 0) {
			misc_deregister(mtr_misc_dev + n_dev);
			mutex_unlock(&rficlock);
			return ret;
		}
	}

	n_dev++;
	pr_debug("%s: num_of_ssbi_devices = %d\n", __func__, n_dev);
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
		for (i = 0; i < RFIC_DEVICE_NUM; ++i)
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

	n_dev = 0;
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
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. fsm9900 RFIC driver");

module_init(ftr_init);
module_exit(ftr_exit);

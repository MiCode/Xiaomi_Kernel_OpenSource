/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clk.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/pinctrl/consumer.h>

#include <mt-plat/mtk_pwm.h>
#include <mach/mtk_pwm_hal.h>

#include "mtk_irtx.h"

#define irtx_driver_name "mt_irtx"
#define IRTX_GPIO_MODE_LED_DEFAULT 0
#define IRTX_GPIO_MODE_LED_SET 1

static atomic_t ir_usage_cnt;
char *irtx_gpio_cfg[] = {  "irtx_gpio_led_default", "irtx_gpio_led_set"};
static u64 irtx_dma_mask = DMA_BIT_MASK(32);

struct mt_irtx mt_irtx_dev;
struct pwm_spec_config irtx_pwm_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_MEMORY,
	.clk_div = CLK_DIV1,
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31,
	/* 1 microseconds, assume clock source is 26M */
	.PWM_MODE_MEMORY_REGS.HDURATION = 25,
	.PWM_MODE_MEMORY_REGS.LDURATION = 25,
	.PWM_MODE_MEMORY_REGS.GDURATION = 0,
	.PWM_MODE_MEMORY_REGS.WAVE_NUM = 1,
};

int get_ir_device(void)
{
	if (atomic_cmpxchg(&ir_usage_cnt, 0, 1) != 0)
		return -EBUSY;
	return 0;
}

int put_ir_device(void)
{
	if (atomic_cmpxchg(&ir_usage_cnt, 1, 0) != 1)
		return -EFAULT;
	return 0;
}

void switch_irtx_gpio(int mode)
{
	int ret;
	struct pinctrl *ppinctrl_irtx = mt_irtx_dev.ppinctrl_irtx;
	struct pinctrl_state *pins_irtx = NULL;

	if (mode >= (ARRAY_SIZE(irtx_gpio_cfg))) {
		pr_notice("%s() IRTX[PinC](%d) fail!! - invalid parameter!\n",
			__func__, mode);
		return;
	}

	if (IS_ERR(ppinctrl_irtx)) {
		pr_notice("%s() IRTX[PinC] ppinctrl_irtx:%p Error! err:%ld\n",
		       __func__, ppinctrl_irtx, PTR_ERR(ppinctrl_irtx));
		return;
	}

	if (mode == IRTX_GPIO_MODE_LED_SET) {
		ret = regulator_enable(mt_irtx_dev.buck);
		if (ret < 0) {
			pr_notice("%s() IRTX regulator_enable fail!\n",
				__func__);
			return;
		}
	} else {
		ret = regulator_disable(mt_irtx_dev.buck);
		if (ret < 0) {
			pr_notice("%s() IRTX regulator_disable fail!\n",
				__func__);
			return;
		}
	}

	pins_irtx = pinctrl_lookup_state(ppinctrl_irtx, irtx_gpio_cfg[mode]);
	if (IS_ERR(pins_irtx)) {
		pr_notice("%s() IRTX[PinC] pinctrl_lockup(%p, %s) fail!\n",
			__func__, ppinctrl_irtx, irtx_gpio_cfg[mode]);
		pr_notice("%s() IRTX[PinC] ppinctrl:%p, err:%ld\n",
			__func__, pins_irtx, PTR_ERR(pins_irtx));
		return;
	}

	pinctrl_select_state(ppinctrl_irtx, pins_irtx);
	pr_info("%s() IRTX[PinC] to mode:%d done.\n", __func__, mode);
}

static int dev_char_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	ret = get_ir_device();
	if (ret) {
		pr_notice("%s() IRTX device busy.\n", __func__);
		goto exit;
	}

	pr_info("%s() IRTX open by %s\n", __func__, current->comm);
	nonseekable_open(inode, file);
exit:
	return ret;
}

static int dev_char_close(struct inode *inode, struct file *file)
{
	int ret = 0;

	ret = put_ir_device();
	if (ret) {
		pr_notice("%s() IRTX device close without open\n", __func__);
		goto exit;
	}

	pr_info("%s() IRTX close by %s\n", __func__, current->comm);
exit:
	return ret;
}

static ssize_t dev_char_read(struct file *file, char *buf, size_t count,
	loff_t *ppos)
{
	return 0;
}

static ssize_t dev_char_write(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	dma_addr_t wave_phy;
	void *wave_vir;
	int ret, i;
	int buf_size = (count + 3) / 4;
	unsigned char *data_ptr;

	pr_info("%s() irtx write len=0x%x, pwm=%d\n", __func__,
		(unsigned int)count, (unsigned int)irtx_pwm_config.pwm_no);
	if (count == 0) {
		ret = 0;
		goto exit_1;
	}

	wave_vir = dma_alloc_coherent(&mt_irtx_dev.plat_dev->dev, count,
		&wave_phy, GFP_KERNEL | GFP_DMA);
	if (!wave_vir) {
		pr_notice("%s() IRTX alloc memory fail\n", __func__);
		ret = -ENOMEM;
		goto exit_1;
	}

	ret = copy_from_user(wave_vir, buf, count);
	if (ret) {
		pr_notice("%s() IRTX copy from user fail %d\n", __func__, ret);
		goto exit_2;
	}

	/* invert bit */
	if (mt_irtx_dev.pwm_data_invert) {
		pr_notice("%s() IRTX invert data\n", __func__);
		for (i = 0; i < count; i++) {
			data_ptr = (unsigned char *)wave_vir + i;
			*data_ptr = ~(*data_ptr);
		}
	}

	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = wave_phy;
	buf_size = (buf_size ? (buf_size - 1) : 0);
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = buf_size;

	switch_irtx_gpio(IRTX_GPIO_MODE_LED_SET);

	ret = pwm_set_spec_config(&irtx_pwm_config);
	pr_info("%s() IRTX pwm is triggered, %d\n", __func__, ret);

	msleep(count * 8 / 1000);
	msleep(100);
	ret = count;

	pr_info("[IRTX] done, clean up\n");
	mt_pwm_disable(irtx_pwm_config.pwm_no, irtx_pwm_config.pmic_pad);
	switch_irtx_gpio(IRTX_GPIO_MODE_LED_DEFAULT);

exit_2:
	dma_free_coherent(&mt_irtx_dev.plat_dev->dev, count, wave_vir,
			wave_phy);
exit_1:
	return ret;
}

static long dev_char_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	unsigned int para = 0, gpio_id = -1, en = 0;

	switch (cmd) {
	case IRTX_IOC_GET_SOLUTTION_TYPE:
		ret = put_user(1, (unsigned int __user *)arg);
		break;
	case IRTX_IOC_SET_IRTX_LED_EN:
		ret = copy_from_user(&para, (void __user *)arg,
			sizeof(unsigned int));
		if (ret) {
			pr_notice("%s() IRTX SET LED EN,copy_from_user fail\n",
				__func__);
			ret = -EFAULT;
		} else {
			/* en: bit 12; */
			/* gpio: bit 0-11 */
			gpio_id = (unsigned long)((para & 0x0FFF0000) > 16);
			en = (para & 0xF);
			pr_info("%s IRTX SET LED EN:0x%x,gpioid:%ul,en:%ul\n",
				__func__, para, gpio_id, en);

			if (en)
				switch_irtx_gpio(IRTX_GPIO_MODE_LED_SET);
			else
				switch_irtx_gpio(IRTX_GPIO_MODE_LED_DEFAULT);
		}
		break;
	default:
		pr_notice("%s() IRTX invalid ioctl cmd 0x%x\n", __func__, cmd);
		ret = -ENOTTY;
		break;
	}
	return ret;
}

static struct file_operations const char_dev_fops = {
	.owner = THIS_MODULE,
	.open = &dev_char_open,
	.release = &dev_char_close,
	.read = &dev_char_read,
	.write = &dev_char_write,
	.unlocked_ioctl = &dev_char_ioctl,
};
static int irtx_probe(struct platform_device *plat_dev)
{
	struct cdev *c_dev;
	dev_t dev_t_irtx;
	struct device *dev = NULL;
	static void *dev_class;
	u32 major = 0, minor = 0;
	int ret = 0;

	if (plat_dev->dev.of_node == NULL) {
		pr_notice("%s() irtx OF node is NULL!\n", __func__);
		ret = -1;
		goto exit;
	}

	of_property_read_u32(plat_dev->dev.of_node, "major", &major);
	of_property_read_u32(plat_dev->dev.of_node, "pwm_ch",
		&mt_irtx_dev.pwm_ch);
	of_property_read_u32(plat_dev->dev.of_node, "pwm_data_invert",
		&mt_irtx_dev.pwm_data_invert);
	pr_info("%s() device tree info: major=%d pwm=%d invert=%d\n", __func__,
		major, mt_irtx_dev.pwm_ch, mt_irtx_dev.pwm_data_invert);

	mt_irtx_dev.ppinctrl_irtx = devm_pinctrl_get(&plat_dev->dev);
	if (IS_ERR(mt_irtx_dev.ppinctrl_irtx)) {
		pr_notice("%s() [PinC]cannot find pinctrl! ptr_err:%ld.\n",
			__func__, PTR_ERR(mt_irtx_dev.ppinctrl_irtx));
		ret = PTR_ERR(mt_irtx_dev.ppinctrl_irtx);
		goto exit;
	}

	mt_irtx_dev.buck = regulator_get(NULL, "irtx_ldo");
	if (mt_irtx_dev.buck == NULL) {
		pr_notice("%s() regulator_get fail!\n", __func__);
		ret = -1;
		goto exit;
	}

	ret = regulator_set_voltage(mt_irtx_dev.buck, 2800000, 2800000);
	if (ret < 0) {
		pr_notice("%s() regulator_set_voltage fail! ret:%d.\n",
			__func__, ret);
		goto exit;
	}

	switch_irtx_gpio(IRTX_GPIO_MODE_LED_DEFAULT);

	if (!major) {
		ret = alloc_chrdev_region(&dev_t_irtx, 0, 1, irtx_driver_name);
		if (ret) {
			pr_notice("%s() alloc_chrdev_region fail! ret=%d.\n",
				__func__, ret);
			goto exit;
		} else {
			major = MAJOR(dev_t_irtx);
			minor = MINOR(dev_t_irtx);
		}
	} else {
		dev_t_irtx = MKDEV(major, minor);
		ret = register_chrdev_region(dev_t_irtx, 1, irtx_driver_name);
		if (ret) {
			pr_notice("%s() register_chrdev_region fail! ret=%d.\n",
				__func__, ret);
			goto exit;
		}
	}

	irtx_pwm_config.pwm_no = mt_irtx_dev.pwm_ch;

	mt_irtx_dev.plat_dev = plat_dev;
	mt_irtx_dev.plat_dev->dev.dma_mask = &irtx_dma_mask;
	mt_irtx_dev.plat_dev->dev.coherent_dma_mask = irtx_dma_mask;

	c_dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!c_dev) {
		ret = -ENOMEM;
		goto exit;
	}

	cdev_init(c_dev, &char_dev_fops);
	c_dev->owner = THIS_MODULE;
	ret = cdev_add(c_dev, dev_t_irtx, 1);
	if (ret) {
		pr_notice("%s() cdev_add fail! ret=%d\n", __func__, ret);
		goto exit;
	}

	dev_class = class_create(THIS_MODULE, irtx_driver_name);
	dev = device_create(dev_class, NULL, dev_t_irtx, NULL, "irtx");
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_notice("%s() device_create fail! ret=%d\n", __func__, ret);
		goto exit;
	}
	pr_info("%s() Done.\n", __func__);
	return 0;

 exit:
	pr_info("%s() fail! ret=%d\n", __func__, ret);
	return ret;
}

static const struct of_device_id irtx_of_ids[] = {
	{.compatible = "mediatek,irtx-pwm",},
	{}
};

static struct platform_driver irtx_driver = {
	.driver = {
		.name = "mt_irtx",
		},
	.probe = irtx_probe,
};

static int __init irtx_init(void)
{
	int ret = 0;

	pr_info("%s()\n", __func__);
	irtx_driver.driver.of_match_table = irtx_of_ids;

	ret = platform_driver_register(&irtx_driver);
	if (ret) {
		pr_notice("%s() platform driver register fail ret:%d.\n",
			__func__, ret);
		goto exit;
	}

 exit:
	return ret;
}

late_initcall(irtx_init);

MODULE_AUTHOR("Luhua Xu <luhua.xu@mediatek.com>");
MODULE_DESCRIPTION("Consumer IR transmitter driver v0.1");

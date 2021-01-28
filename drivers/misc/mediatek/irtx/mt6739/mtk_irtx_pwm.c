/*
* Copyright (C) 2016 MediaTek Inc.
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

#include <mt-plat/mtk_pwm.h>
#include <mach/mtk_pwm_hal.h>

#include "mtk_irtx.h"

struct mt_irtx mt_irtx_dev;
void __iomem *irtx_reg_base;
unsigned int irtx_irq;

static atomic_t ir_usage_cnt;

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

struct pwm_spec_config irtx_pwm_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_MEMORY,
	.clk_div = CLK_DIV1,
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_MEMORY_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_MEMORY_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE = 31,
	.PWM_MODE_MEMORY_REGS.HDURATION = 25,	/* 1 microseconds, assume clock source is 26M */
	.PWM_MODE_MEMORY_REGS.LDURATION = 25,
	.PWM_MODE_MEMORY_REGS.GDURATION = 0,
	.PWM_MODE_MEMORY_REGS.WAVE_NUM = 1,
};

#define IRTX_GPIO_MODE_LED_DEFAULT 0
#define IRTX_GPIO_MODE_LED_SET 1
char *irtx_gpio_cfg[] = {  "irtx_gpio_led_default", "irtx_gpio_led_set"};

void switch_irtx_gpio(int mode)
{
	struct pinctrl *ppinctrl_irtx = mt_irtx_dev.ppinctrl_irtx;
	struct pinctrl_state *pins_irtx = NULL;

	pr_notice("[IRTX][PinC]%s(%d)+\n", __func__, mode);

	if (mode >= (ARRAY_SIZE(irtx_gpio_cfg))) {
		pr_notice("[IRTX][PinC]%s(%d) fail!! - parameter error!\n", __func__, mode);
		return;
	}

	if (IS_ERR(ppinctrl_irtx)) {
		pr_notice("[IRTX][PinC]%s ppinctrl_irtx:%p is error! err:%ld\n",
		       __func__, ppinctrl_irtx, PTR_ERR(ppinctrl_irtx));
		return;
	}

	pins_irtx = pinctrl_lookup_state(ppinctrl_irtx, irtx_gpio_cfg[mode]);
	if (IS_ERR(pins_irtx)) {
		pr_notice("[IRTX][PinC]%s pinctrl_lockup(%p, %s) fail!! ppinctrl:%p, err:%ld\n", __func__,
		       ppinctrl_irtx, irtx_gpio_cfg[mode], pins_irtx, PTR_ERR(pins_irtx));
		return;
	}

	pinctrl_select_state(ppinctrl_irtx, pins_irtx);
	pr_notice("[IRTX][PinC]%s(%d)-\n", __func__, mode);
}

static int dev_char_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	ret = get_ir_device();
	if (ret) {
		pr_err("[IRTX] device busy\n");
		goto exit;
	}

	pr_debug("[IRTX] open by %s\n", current->comm);
	nonseekable_open(inode, file);
exit:
	return ret;
}

static int dev_char_close(struct inode *inode, struct file *file)
{
	int ret = 0;

	ret = put_ir_device();
	if (ret) {
		pr_err("[IRTX] device close without open\n");
		goto exit;
	}

	pr_debug("[IRTX] close by %s\n", current->comm);
exit:
	return ret;
}

static ssize_t dev_char_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static long dev_char_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int para = 0, gpio_id = -1, en = 0;

	switch (cmd) {
	case IRTX_IOC_GET_SOLUTTION_TYPE:
		ret = put_user(1, (unsigned int __user *)arg);
		break;
	case IRTX_IOC_SET_IRTX_LED_EN:
		if (copy_from_user(&para, (void __user *)arg, sizeof(unsigned int))) {
			pr_err("[IRTX] IRTX_IOC_SET_IRTX_LED_EN: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			/* en: bit 12; */
			/* gpio: bit 0-11 */
			gpio_id = (unsigned long)((para & 0x0FFF0000) > 16);
			en = (para & 0xF);
			pr_info("[IRTX] IRTX_IOC_SET_IRTX_LED_EN: 0x%x, gpio_id:%ul, en:%ul\n", para, gpio_id, en);

			if (en)
				switch_irtx_gpio(IRTX_GPIO_MODE_LED_SET);
			else
				switch_irtx_gpio(IRTX_GPIO_MODE_LED_DEFAULT);
		}
		break;
	default:
		pr_err("[IRTX] unknown ioctl cmd 0x%x\n", cmd);
		ret = -ENOTTY;
		break;
	}
	return ret;
}

static ssize_t dev_char_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	dma_addr_t wave_phy;
	void *wave_vir;
	int ret, i;
	int buf_size = (count + 3) / 4;	/* when count is 5... */
	unsigned char *data_ptr;

	pr_debug("[IRTX] irtx write len=0x%x, pwm=%d\n", (unsigned int)count, (unsigned int)irtx_pwm_config.pwm_no);
	if (count == 0)
		return 0;

	wave_vir = dma_alloc_coherent(&mt_irtx_dev.plat_dev->dev, count, &wave_phy, GFP_KERNEL | GFP_DMA);
	if (!wave_vir) {
		pr_err("[IRTX] alloc memory fail\n");
		return -ENOMEM;
	}
	ret = copy_from_user(wave_vir, buf, count);
	if (ret) {
		pr_err("[IRTX] write, copy from user fail %d\n", ret);
		goto exit;
	}
	/* invert bit */
	if (mt_irtx_dev.pwm_data_invert) {
		pr_debug("[IRTX] invert data\n");
		for (i = 0; i < count; i++) {
			data_ptr = (unsigned char *)wave_vir + i;
			*data_ptr = ~(*data_ptr);
		}
	}

	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR = wave_phy;
	irtx_pwm_config.PWM_MODE_MEMORY_REGS.BUF0_SIZE = (buf_size ? (buf_size - 1) : 0);

	switch_irtx_gpio(IRTX_GPIO_MODE_LED_SET);

	ret = pwm_set_spec_config(&irtx_pwm_config);
	pr_debug("[IRTX] pwm is triggered, %d\n", ret);

	msleep(count * 8 / 1000);
	msleep(100);
	ret = count;
exit:
	pr_debug("[IRTX] done, clean up\n");
	dma_free_coherent(&mt_irtx_dev.plat_dev->dev, count, wave_vir, wave_phy);
	mt_pwm_disable(irtx_pwm_config.pwm_no, irtx_pwm_config.pmic_pad);

	switch_irtx_gpio(IRTX_GPIO_MODE_LED_DEFAULT);

	return ret;
}

static u64 irtx_dma_mask = DMA_BIT_MASK(32);	/* HW is 32bit */

static struct file_operations const char_dev_fops = {
	.owner = THIS_MODULE,
	.open = &dev_char_open,
	.read = &dev_char_read,
	.write = &dev_char_write,
	.release = &dev_char_close,
	.unlocked_ioctl = &dev_char_ioctl,
};

#define irtx_driver_name "mt_irtx"
static int irtx_probe(struct platform_device *plat_dev)
{
	struct cdev *c_dev;
	dev_t dev_t_irtx;
	struct device *dev = NULL;
	static void *dev_class;
	u32 major = 0, minor = 0;
	int ret = 0;

	if (plat_dev->dev.of_node == NULL) {
		pr_err("[IRTX] irtx OF node is NULL\n");
		return -1;
	}

	of_property_read_u32(plat_dev->dev.of_node, "major", &major);
	mt_irtx_dev.reg_base = of_iomap(plat_dev->dev.of_node, 0);
	mt_irtx_dev.irq = irq_of_parse_and_map(plat_dev->dev.of_node, 0);
	of_property_read_u32(plat_dev->dev.of_node, "pwm_ch", &mt_irtx_dev.pwm_ch);
	of_property_read_u32(plat_dev->dev.of_node, "pwm_data_invert", &mt_irtx_dev.pwm_data_invert);
	pr_debug("[IRTX] device tree info: major=%d pwm=%d invert=%d\n",
			major, mt_irtx_dev.pwm_ch, mt_irtx_dev.pwm_data_invert);

	mt_irtx_dev.ppinctrl_irtx = devm_pinctrl_get(&plat_dev->dev);
	if (IS_ERR(mt_irtx_dev.ppinctrl_irtx)) {
		pr_err("[IRTX][PinC]cannot find pinctrl. ptr_err:%ld\n", PTR_ERR(mt_irtx_dev.ppinctrl_irtx));
		return PTR_ERR(mt_irtx_dev.ppinctrl_irtx);
	}
	pr_notice("[IRTX][PinC]devm_pinctrl_get ppinctrl:%p\n", mt_irtx_dev.ppinctrl_irtx);

	switch_irtx_gpio(IRTX_GPIO_MODE_LED_DEFAULT);

	if (!major) {
		ret = alloc_chrdev_region(&dev_t_irtx, 0, 1, irtx_driver_name);
		if (ret) {
			pr_err("[IRTX] alloc_chrdev_region fail ret=%d\n", ret);
			goto exit;
		} else {
			major = MAJOR(dev_t_irtx);
			minor = MINOR(dev_t_irtx);
		}
	} else {
		dev_t_irtx = MKDEV(major, minor);
		ret = register_chrdev_region(dev_t_irtx, 1, irtx_driver_name);
		if (ret) {
			pr_err("[IRTX] register_chrdev_region fail ret=%d\n", ret);
			goto exit;
		}
	}

	irtx_pwm_config.pwm_no = mt_irtx_dev.pwm_ch;

	mt_irtx_dev.plat_dev = plat_dev;
	mt_irtx_dev.plat_dev->dev.dma_mask = &irtx_dma_mask;
	mt_irtx_dev.plat_dev->dev.coherent_dma_mask = irtx_dma_mask;

	c_dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!c_dev)
		goto exit;

	cdev_init(c_dev, &char_dev_fops);
	c_dev->owner = THIS_MODULE;
	ret = cdev_add(c_dev, dev_t_irtx, 1);
	if (ret) {
		pr_err("[IRTX] cdev_add fail ret=%d\n", ret);
		goto exit;
	}
	dev_class = class_create(THIS_MODULE, irtx_driver_name);
	dev = device_create(dev_class, NULL, dev_t_irtx, NULL, "irtx");
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("[IRTX] device_create fail ret=%d\n", ret);
		goto exit;
	}

 exit:
	pr_debug("[IRTX] irtx probe ret=%d\n", ret);
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

	pr_info("[IRTX] irtx init\n");
	irtx_driver.driver.of_match_table = irtx_of_ids;

	ret = platform_driver_register(&irtx_driver);
	if (ret) {
		pr_err("[IRTX] irtx platform driver register fail %d\n", ret);
		goto exit;
	}

 exit:
	return ret;
}

late_initcall(irtx_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("Consumer IR transmitter driver v0.1");

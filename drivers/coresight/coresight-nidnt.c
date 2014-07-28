/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/interrupt.h>
#include "coresight-priv.h"
#include "coresight-nidnt.h"

#define nidnt_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define nidnt_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define NIDNT_MAX_PINS				6
#define NIDNT_BOOT_FROM_SD			0
#define NIDNT_SDCARD_ONLY			1
#define NIDNT_CARD_DETECT_GPIO_POLARITY		BIT(8)
#define NIDNT_CARD_DETECT_GPIO_SHIFT		0
#define NIDNT_DEBOUNCE_MASK			0xfffff000UL
#define NIDNT_DEBUGMODE_MASK			0x3fcUL
#define NIDNT_STATUS_DEBUGMODE_MASK		0x7f8UL
#define NIDNT_TIMEOUT_MASK			0xffffffUL

/* NIDnT Registers */
#define TLMM_QDSD_HDRV_PULL_DEBUG_GPIO_CTL		(0x9c000)
#define TLMM_QDSD_BOOT_CTL				(0x9d000)
#define TLMM_QDSD_CONFIG_CTL				(0x9e000)
#define TLMM_QDSD_STATUS_CTL				(0x9f000)
#define TLMM_QDSD_DEBUG_HDRV_PULL_CTL			(0xa0000)
#define TLMM_QDSD_GPIO_CTL				(0xa1000)
#define TLMM_QDSD_INTR_ENABLE_CTL			(0xa2000)
#define TLMM_QDSD_INTR_CLEAR_CTL			(0xa3000)
#define TLMM_QDSD_TIMEOUT_VALUE_CTL			(0xa4000)
#define TLMM_QDSD_SPARE1_CTL				(0xa5000)
#define TLMM_QDSD_SPARE2_CTL				(0xa6000)

struct nidnt_pinctrl_data {
	struct pinctrl          *pctrl;
	struct pinctrl_state    *pins_active[NIDNT_MAX_PINS];
};

struct nidnt_drvdata {
	void __iomem			*base;
	struct nidnt_pinctrl_data	*nidnt_pctrl_data;
	unsigned int			nidnt_gpio;
	unsigned int			debounce_value;
	unsigned int			timeout_value;
	bool				is_active_low;
	/* nidnt functionality enable */
	bool				enable;
	/* nidnt hw detect enable */
	bool				nidnt_hwdetect_enable;
	int				nidnt_irq;
	int				sd_irq;
	spinlock_t			spinlock;
};
struct nidnt_drvdata *nidnt_drvdata;

static int nidnt_timeout_value = 0xFFFFFF;
module_param_named(nidnt_timeout_value,
	nidnt_timeout_value, int, S_IRUGO | S_IWUSR | S_IWGRP);

void coresight_nidnt_writel(unsigned int val, unsigned int off)
{
	nidnt_writel(nidnt_drvdata, val, off);
}
EXPORT_SYMBOL(coresight_nidnt_writel);

static int coresight_nidnt_setup_pinctrl(struct nidnt_drvdata *nidnt_drvdata,
					 enum nidnt_debug_mode mode)
{
	int ret = 0;

	switch (mode) {
	case NIDNT_MODE_SDCARD:
	default:
		ret = pinctrl_select_state(
			nidnt_drvdata->nidnt_pctrl_data->pctrl,
			nidnt_drvdata->nidnt_pctrl_data->pins_active[0]);
		break;
	case NIDNT_MODE_SDC_TRACE:
		ret = pinctrl_select_state(
			nidnt_drvdata->nidnt_pctrl_data->pctrl,
			nidnt_drvdata->nidnt_pctrl_data->pins_active[1]);
		break;
	case NIDNT_MODE_SDC_SWDUART:
		ret = pinctrl_select_state(
			nidnt_drvdata->nidnt_pctrl_data->pctrl,
			nidnt_drvdata->nidnt_pctrl_data->pins_active[2]);
		break;
	case NIDNT_MODE_SDC_SWDTRC:
		ret = pinctrl_select_state(
			nidnt_drvdata->nidnt_pctrl_data->pctrl,
			nidnt_drvdata->nidnt_pctrl_data->pins_active[3]);
		break;
	case NIDNT_MODE_SDC_JTAG:
		ret = pinctrl_select_state(
			nidnt_drvdata->nidnt_pctrl_data->pctrl,
			nidnt_drvdata->nidnt_pctrl_data->pins_active[4]);
		break;
	case NIDNT_MODE_SDC_SPMI:
		ret = pinctrl_select_state(
			nidnt_drvdata->nidnt_pctrl_data->pctrl,
			nidnt_drvdata->nidnt_pctrl_data->pins_active[5]);
		break;
	}

	if (ret < 0)
		pr_err("Setting %x state for pinctrl failed with %d\n",
				mode, ret);

	return ret;
}

int coresight_nidnt_config_swoverride(enum nidnt_debug_mode mode)
{
	unsigned int reg;
	int ret;

	if (!nidnt_drvdata->enable)
		return -EPERM;

	ret = coresight_nidnt_setup_pinctrl(nidnt_drvdata, mode);
	if (ret < 0)
		return ret;

	spin_lock(&nidnt_drvdata->spinlock);

	reg = nidnt_readl(nidnt_drvdata, TLMM_QDSD_CONFIG_CTL);
	reg &= ~(NIDNT_DEBUGMODE_MASK | NIDNT_DEBOUNCE_MASK);
	/* Configure debug mode and set sw override bit */
	reg |= ((mode << 2) & NIDNT_DEBUGMODE_MASK) | BIT(0);
	reg |= (nidnt_drvdata->debounce_value << 12 | BIT(11));
	/* Add the pin configuration */
	nidnt_writel(nidnt_drvdata, reg, TLMM_QDSD_CONFIG_CTL);

	/* Configure the timeout value */
	nidnt_writel(nidnt_drvdata, nidnt_drvdata->timeout_value | BIT(31),
		     TLMM_QDSD_TIMEOUT_VALUE_CTL);

	spin_unlock(&nidnt_drvdata->spinlock);

	return 0;
}
EXPORT_SYMBOL(coresight_nidnt_config_swoverride);

static void coresight_nidnt_config_hwdetect(struct nidnt_drvdata *nidnt_drvdata)
{
	uint32_t nidnt_gpio_reg;

	nidnt_gpio_reg = (nidnt_drvdata->nidnt_gpio & 0xff)
			  << NIDNT_CARD_DETECT_GPIO_SHIFT;
	nidnt_gpio_reg |= nidnt_drvdata->is_active_low ?
			  0 : NIDNT_CARD_DETECT_GPIO_POLARITY;
	nidnt_gpio_reg |= BIT(15);

	spin_lock(&nidnt_drvdata->spinlock);
	nidnt_writel(nidnt_drvdata, nidnt_gpio_reg, TLMM_QDSD_GPIO_CTL);

	/* enable SD and QDSD interrupt */
	nidnt_writel(nidnt_drvdata, 0x3, TLMM_QDSD_INTR_ENABLE_CTL);
	spin_unlock(&nidnt_drvdata->spinlock);

	pr_info("nidnt gpio reg: 0x%x\n", (unsigned int)nidnt_gpio_reg);
}

void coresight_nidnt_set_hwdetect_param(bool val)
{
	if (nidnt_drvdata->enable)
		nidnt_drvdata->nidnt_hwdetect_enable = val;
}
EXPORT_SYMBOL(coresight_nidnt_set_hwdetect_param);

static void __coresight_nidnt_enable_hwdetect(struct nidnt_drvdata
					     *nidnt_drvdata)
{
	unsigned int regval;

	spin_lock(&nidnt_drvdata->spinlock);

	regval = nidnt_readl(nidnt_drvdata, TLMM_QDSD_CONFIG_CTL);
	/* clear the soft override bit to support the hardware detect */
	regval &= ~(BIT(0) | BIT(11));
	nidnt_writel(nidnt_drvdata, regval, TLMM_QDSD_CONFIG_CTL);

	/* set the timeout value */
	regval = nidnt_readl(nidnt_drvdata, TLMM_QDSD_TIMEOUT_VALUE_CTL);
	regval = nidnt_drvdata->timeout_value | BIT(31);
	nidnt_writel(nidnt_drvdata, regval, TLMM_QDSD_TIMEOUT_VALUE_CTL);

	spin_unlock(&nidnt_drvdata->spinlock);
}

int coresight_nidnt_enable_hwdetect(void)
{
	if (!nidnt_drvdata->nidnt_hwdetect_enable)
		return -EPERM;

	coresight_nidnt_config_hwdetect(nidnt_drvdata);
	/* Enable the TLMM debug mode for nidnt detect.*/
	__coresight_nidnt_enable_hwdetect(nidnt_drvdata);

	return 0;
}
EXPORT_SYMBOL(coresight_nidnt_enable_hwdetect);

ssize_t coresight_nidnt_show_timeout_value(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	unsigned int val = nidnt_drvdata->timeout_value;

	if (!nidnt_drvdata->enable)
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%#x", val);
}
EXPORT_SYMBOL(coresight_nidnt_show_timeout_value);

ssize_t coresight_nidnt_store_timeout_value(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	unsigned int val;

	if (!nidnt_drvdata->enable)
		return -EPERM;

	if (sscanf(buf, "%x", &val) != 1)
		return -EINVAL;

	nidnt_drvdata->timeout_value = val & NIDNT_TIMEOUT_MASK;

	return size;
}
EXPORT_SYMBOL(coresight_nidnt_store_timeout_value);

ssize_t coresight_nidnt_show_debounce_value(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	unsigned int val = nidnt_drvdata->debounce_value;

	if (!nidnt_drvdata->enable)
		return -EPERM;

	return scnprintf(buf, PAGE_SIZE, "%x\n", val);
}
EXPORT_SYMBOL(coresight_nidnt_show_debounce_value);

ssize_t coresight_nidnt_store_debounce_value(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	unsigned int val;

	if (!nidnt_drvdata->enable)
		return -EPERM;

	if (sscanf(buf, "%x", &val) != 1)
		return -EINVAL;

	nidnt_drvdata->debounce_value = val;

	return size;
}
EXPORT_SYMBOL(coresight_nidnt_store_debounce_value);

int coresight_nidnt_get_status(void)
{
	unsigned int reg;
	uint32_t nidnt_mode = 0;

	reg = nidnt_readl(nidnt_drvdata, TLMM_QDSD_STATUS_CTL);
	if ((reg & BIT(1)) || !(reg & BIT(2)) || !(reg & BIT(0)))
		nidnt_mode = 0;
	else
		nidnt_mode = (reg & NIDNT_STATUS_DEBUGMODE_MASK);

	return nidnt_mode;

}
EXPORT_SYMBOL(coresight_nidnt_get_status);

static bool coresight_nidnt_is_boot_from_sd(struct nidnt_drvdata *nidnt_drvdata)
{
	uint16_t nidnt_boot_config;
	bool ret;

	nidnt_boot_config = nidnt_readl(nidnt_drvdata, TLMM_QDSD_BOOT_CTL);

	pr_info("nidnt boot config: %x\n",
		(unsigned int)nidnt_boot_config);

	if (BVAL(nidnt_boot_config, NIDNT_BOOT_FROM_SD))
		ret = true;
	else if (BVAL(nidnt_boot_config, NIDNT_SDCARD_ONLY))
		ret = true;
	else
		ret = false;

	if (ret)
		pr_err("NIDnT disabled, only sd mode supported.\n");

	return ret;
}

static int coresight_nidnt_parse_pinctrl_info(struct device *dev,
					      struct nidnt_drvdata *
					      nidnt_drvdata)
{
	struct nidnt_pinctrl_data *pctrl_data;
	struct pinctrl *pctrl;
	int ret = 0;

	/* Try to obtain pinctrl handle */
	pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pctrl)) {
		ret = PTR_ERR(pctrl);
		goto err;
	}
	pctrl_data = devm_kzalloc(dev, sizeof(*pctrl_data), GFP_KERNEL);
	if (!pctrl_data) {
		ret = -ENOMEM;
		goto err;
	}
	pctrl_data->pctrl = pctrl;
	/* Look-up and keep the states handy to be used later */
	pctrl_data->pins_active[0] = pinctrl_lookup_state(
			pctrl_data->pctrl, "sdcard");
	if (IS_ERR(pctrl_data->pins_active[0])) {
		ret = PTR_ERR(pctrl_data->pins_active[0]);
		dev_err(dev, "Could not get sdcard pinstates, err:%d\n", ret);
		goto err;
	}
	pctrl_data->pins_active[1] = pinctrl_lookup_state(
			pctrl_data->pctrl, "trace");
	if (IS_ERR(pctrl_data->pins_active[1])) {
		ret = PTR_ERR(pctrl_data->pins_active[1]);
		dev_err(dev, "Could not get trace pinstates, err:%d\n", ret);
		goto err;
	}
	pctrl_data->pins_active[2] = pinctrl_lookup_state(
			pctrl_data->pctrl, "swduart");
	if (IS_ERR(pctrl_data->pins_active[2])) {
		ret = PTR_ERR(pctrl_data->pins_active[2]);
		dev_err(dev, "Could not get swduart pinstates, err:%d\n", ret);
		goto err;
	}
	pctrl_data->pins_active[3] = pinctrl_lookup_state(
			pctrl_data->pctrl, "swdtrc");
	if (IS_ERR(pctrl_data->pins_active[3])) {
		ret = PTR_ERR(pctrl_data->pins_active[3]);
		dev_err(dev, "Could not get swdtrc pinstates, err:%d\n", ret);
		goto err;
	}
	pctrl_data->pins_active[4] = pinctrl_lookup_state(
			pctrl_data->pctrl, "jtag");
	if (IS_ERR(pctrl_data->pins_active[4])) {
		ret = PTR_ERR(pctrl_data->pins_active[4]);
		dev_err(dev, "Could not get jtag pinstates, err:%d\n", ret);
		goto err;
	}
	pctrl_data->pins_active[5] = pinctrl_lookup_state(
			pctrl_data->pctrl, "spmi");
	if (IS_ERR(pctrl_data->pins_active[5])) {
		ret = PTR_ERR(pctrl_data->pins_active[5]);
		dev_err(dev, "Could not get spmi pinstates, err:%d\n", ret);
		goto err;
	}
	nidnt_drvdata->nidnt_pctrl_data = pctrl_data;
err:
	return ret;
}

static void dump_nidnt_reg(struct nidnt_drvdata *nidnt_drvdata)
{
	unsigned int val;

	val = nidnt_readl(nidnt_drvdata, TLMM_QDSD_CONFIG_CTL);
	pr_debug("TLMM_QDSD_CONFIG_CTL: 0x%08x\n", val);
	val = nidnt_readl(nidnt_drvdata, TLMM_QDSD_STATUS_CTL);
	pr_debug("TLMM_QDSD_STATUS_CTL: 0x%08x\n", val);
	val = nidnt_readl(nidnt_drvdata, TLMM_QDSD_DEBUG_HDRV_PULL_CTL);
	pr_debug("TLMM_QDSD_DEBUG_HDRV_PULL_CTL: 0x%08x\n", val);
}

static irqreturn_t nidnt_qdsd_irq(int irq, void *data)
{
	struct nidnt_drvdata *nidnt_drvdata = data;

	nidnt_writel(nidnt_drvdata, 0x1, TLMM_QDSD_INTR_CLEAR_CTL);
	dump_nidnt_reg(nidnt_drvdata);

	return IRQ_HANDLED;
}

int coresight_nidnt_init(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;
	uint32_t nidnt_gpio;
	uint32_t nidnt_gpio_polarity;
	bool nidnthw = false;

	nidnt_drvdata = devm_kzalloc(dev,
				     sizeof(struct nidnt_drvdata),
				     GFP_KERNEL);
	if (!nidnt_drvdata)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev,
					   IORESOURCE_MEM,
					   "nidnt-base");
	if (!res)
		return -ENODEV;

	nidnt_drvdata->base = devm_ioremap(dev, res->start,
					   resource_size(res));
	if (!nidnt_drvdata->base)
		return -ENOMEM;

	nidnthw = of_property_read_bool(node, "qcom,nidnthw");
	if (!nidnthw)
		return 0;

	spin_lock_init(&nidnt_drvdata->spinlock);

	/*
	 * NIDnT can be disabled in the following cases:
	 *    1. NIDnT fuse is blown
	 *    2. If TLMM_QDSD_BOOT_CTL 0,1 bit is set.
	 */
	if (coresight_nidnt_is_boot_from_sd(nidnt_drvdata) ||
			coresight_fuse_nidnt_access_disabled()) {
		dev_info(dev, "NIDnT hw support disabled\n");
		/*
		 * Do not return error an error if NIDnT support
		 * is disabled to allow continuing with other
		 * TPIU functionalities.
		 */
		return 0;
	}
	nidnt_drvdata->enable = true;

	ret = coresight_nidnt_parse_pinctrl_info(&pdev->dev, nidnt_drvdata);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "nidnt-gpio", &nidnt_gpio);
	nidnt_drvdata->nidnt_gpio = nidnt_gpio;

	ret = of_property_read_u32(node,
				   "nidnt-gpio-polarity",
				   &nidnt_gpio_polarity);
	nidnt_drvdata->is_active_low = nidnt_gpio_polarity &
				       OF_GPIO_ACTIVE_LOW ? true : false;

	nidnt_drvdata->timeout_value = nidnt_timeout_value;

	nidnt_drvdata->nidnt_irq = platform_get_irq_byname(pdev, "nidnt-irq");
	if (nidnt_drvdata->nidnt_irq < 0) {
		/*
		 * Even though this is an error condition, we do not fail
		 * the probe as the byte counter feature is optional
		 */
		dev_info(dev, "nidnt-irq not specified\n");
		return 0;
	}

	ret = devm_request_irq(&pdev->dev, nidnt_drvdata->nidnt_irq,
			       nidnt_qdsd_irq,
			       IRQF_TRIGGER_RISING | IRQF_SHARED,
			       "nidnt_nidnt", nidnt_drvdata);
	if (ret) {
		dev_info(&pdev->dev, "request for nidnt irq failed\n");
		return -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(coresight_nidnt_init);


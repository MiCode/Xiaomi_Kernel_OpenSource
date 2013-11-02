/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <mach/gpiomux.h>

#include "coresight-priv.h"

#define tpiu_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define tpiu_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define TPIU_LOCK(drvdata)						\
do {									\
	mb();								\
	tpiu_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define TPIU_UNLOCK(drvdata)						\
do {									\
	tpiu_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define TPIU_SUPP_PORTSZ	(0x000)
#define TPIU_CURR_PORTSZ	(0x004)
#define TPIU_SUPP_TRIGMODES	(0x100)
#define TPIU_TRIG_CNTRVAL	(0x104)
#define TPIU_TRIG_MULT		(0x108)
#define TPIU_SUPP_TESTPATM	(0x200)
#define TPIU_CURR_TESTPATM	(0x204)
#define TPIU_TEST_PATREPCNTR	(0x208)
#define TPIU_FFSR		(0x300)
#define TPIU_FFCR		(0x304)
#define TPIU_FSYNC_CNTR		(0x308)
#define TPIU_EXTCTL_INPORT	(0x400)
#define TPIU_EXTCTL_OUTPORT	(0x404)
#define TPIU_ITTRFLINACK	(0xEE4)
#define TPIU_ITTRFLIN		(0xEE8)
#define TPIU_ITATBDATA0		(0xEEC)
#define TPIU_ITATBCTR2		(0xEF0)
#define TPIU_ITATBCTR1		(0xEF4)
#define TPIU_ITATBCTR0		(0xEF8)

enum tpiu_out_mode {
	TPIU_OUT_MODE_NONE,
	TPIU_OUT_MODE_MICTOR,
	TPIU_OUT_MODE_SDC_TRACE,
	TPIU_OUT_MODE_SDC_SWDUART,
	TPIU_OUT_MODE_SDC_SWDTRC,
};

enum tpiu_set {
	TPIU_SET_NONE,
	TPIU_SET_A,
	TPIU_SET_B,
};

struct tpiu_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct clk		*clk;
	struct mutex		mutex;
	enum tpiu_out_mode	out_mode;
	struct regulator	*reg;
	unsigned int		reg_low;
	unsigned int		reg_high;
	unsigned int		reg_lpm;
	unsigned int		reg_hpm;
	struct regulator        *reg_io;
	unsigned int            reg_low_io;
	unsigned int            reg_high_io;
	unsigned int            reg_lpm_io;
	unsigned int            reg_hpm_io;
	enum tpiu_set		set;
	int			seta_gpiocnt;
	int			*seta_gpios;
	struct gpiomux_setting	*seta_cfgs;
	int			setb_gpiocnt;
	int			*setb_gpios;
	struct gpiomux_setting	*setb_cfgs;
	bool			enable;
	bool			nidnt;
};

struct gpiomux_setting old_cfg;

static void tpiu_flush_and_stop(struct tpiu_drvdata *drvdata)
{
	int count;
	uint32_t ffcr;

	ffcr = tpiu_readl(drvdata, TPIU_FFCR);
	ffcr |= BIT(12);
	tpiu_writel(drvdata, ffcr, TPIU_FFCR);
	ffcr |= BIT(6);
	tpiu_writel(drvdata, ffcr, TPIU_FFCR);
	/* Ensure flush completes */
	for (count = TIMEOUT_US; BVAL(tpiu_readl(drvdata, TPIU_FFCR), 6) != 0
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while flushing TPIU, TPIU_FFCR: %#x\n",
	     tpiu_readl(drvdata, TPIU_FFCR));
}

static void __tpiu_enable(struct tpiu_drvdata *drvdata, uint32_t portsz,
			  uint32_t ffcr)
{
	TPIU_UNLOCK(drvdata);

	tpiu_writel(drvdata, portsz, TPIU_CURR_PORTSZ);
	tpiu_writel(drvdata, ffcr, TPIU_FFCR);

	TPIU_LOCK(drvdata);
}

static int __tpiu_enable_seta(struct tpiu_drvdata *drvdata)
{
	int i, ret;

	if (!drvdata->seta_gpiocnt)
		return -EINVAL;

	for (i = 0; i < drvdata->seta_gpiocnt; i++) {
		ret = gpio_request(drvdata->seta_gpios[i], NULL);
		if (ret) {
			dev_err(drvdata->dev,
				"gpio_request failed for seta_gpio: %u\n",
				drvdata->seta_gpios[i]);
			goto err0;
		}
		ret = msm_gpiomux_write(drvdata->seta_gpios[i],
					GPIOMUX_ACTIVE,
					&drvdata->seta_cfgs[i],
					&old_cfg);
		if (ret < 0) {
			dev_err(drvdata->dev,
				"gpio write failed for seta_gpio: %u\n",
				drvdata->seta_gpios[i]);
			goto err1;
		}
	}
	return 0;
err1:
	gpio_free(drvdata->seta_gpios[i]);
err0:
	i--;
	while (i >= 0) {
		gpio_free(drvdata->seta_gpios[i]);
		i--;
	}
	return ret;
}

static int __tpiu_enable_setb(struct tpiu_drvdata *drvdata)
{
	int i, ret;

	if (!drvdata->setb_gpiocnt)
		return -EINVAL;

	for (i = 0; i < drvdata->setb_gpiocnt; i++) {
		ret = gpio_request(drvdata->setb_gpios[i], NULL);
		if (ret) {
			dev_err(drvdata->dev,
				"gpio_request failed for setb_gpio: %u\n",
				drvdata->setb_gpios[i]);
			goto err0;
		}
		ret = msm_gpiomux_write(drvdata->setb_gpios[i],
					GPIOMUX_ACTIVE,
					&drvdata->setb_cfgs[i],
					&old_cfg);
		if (ret < 0) {
			dev_err(drvdata->dev,
				"gpio write failed for setb_gpio: %u\n",
				drvdata->setb_gpios[i]);
			goto err1;
		}
	}
	return 0;
err1:
	gpio_free(drvdata->setb_gpios[i]);
err0:
	i--;
	while (i >= 0) {
		gpio_free(drvdata->setb_gpios[i]);
		i--;
	}
	return ret;
}

static int __tpiu_enable_to_mictor(struct tpiu_drvdata *drvdata)
{
	int ret;

	if (drvdata->set == TPIU_SET_A) {
		ret = __tpiu_enable_seta(drvdata);
		if (ret)
			return ret;
	} else if (drvdata->set == TPIU_SET_B) {
		ret = __tpiu_enable_setb(drvdata);
		if (ret)
			return ret;
	}

	__tpiu_enable(drvdata, 0x8000, 0x101);

	return 0;
}

static int tpiu_reg_set_optimum_mode(struct regulator *reg,
				     unsigned int reg_hpm)
{
	if (regulator_count_voltages(reg) <= 0)
		return 0;

	return regulator_set_optimum_mode(reg, reg_hpm);
}

static int tpiu_reg_set_voltage(struct regulator *reg, unsigned int reg_low,
				unsigned int reg_high)
{
	if (regulator_count_voltages(reg) <= 0)
		return 0;

	return regulator_set_voltage(reg, reg_low, reg_high);
}

static int __tpiu_enable_to_sdc(struct tpiu_drvdata *drvdata)
{
	int ret;

	if (!drvdata->reg || !drvdata->reg_io)
		return -EINVAL;

	ret = tpiu_reg_set_optimum_mode(drvdata->reg, drvdata->reg_hpm);
	if (ret < 0)
		return ret;
	ret = tpiu_reg_set_voltage(drvdata->reg, drvdata->reg_low,
				   drvdata->reg_high);
	if (ret)
		goto err0;
	ret = regulator_enable(drvdata->reg);
	if (ret)
		goto err1;
	ret = tpiu_reg_set_optimum_mode(drvdata->reg_io, drvdata->reg_hpm_io);
	if (ret < 0)
		goto err2;
	ret = tpiu_reg_set_voltage(drvdata->reg_io, drvdata->reg_low_io,
				   drvdata->reg_high_io);
	if (ret)
		goto err3;
	ret = regulator_enable(drvdata->reg_io);
	if (ret)
		goto err4;

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_FIXED);
	if (ret)
		goto err5;

	return 0;
err5:
	regulator_disable(drvdata->reg_io);
err4:
	tpiu_reg_set_voltage(drvdata->reg_io, 0, drvdata->reg_high_io);
err3:
	tpiu_reg_set_optimum_mode(drvdata->reg_io, 0);
err2:
	regulator_disable(drvdata->reg);
err1:
	tpiu_reg_set_voltage(drvdata->reg, 0, drvdata->reg_high);
err0:
	tpiu_reg_set_optimum_mode(drvdata->reg, 0);
	return ret;
}

static int __tpiu_enable_to_sdc_trace(struct tpiu_drvdata *drvdata)
{
	int ret;

	ret = __tpiu_enable_to_sdc(drvdata);
	if (ret)
		return ret;

	__tpiu_enable(drvdata, 0x8, 0x103);

	msm_tlmm_misc_reg_write(TLMM_SDC2_HDRV_PULL_CTL, 0x16D);
	msm_tlmm_misc_reg_write(TLMM_ETM_MODE_REG, 1);
	return 0;
}

static int __tpiu_enable_to_sdc_swduart(struct tpiu_drvdata *drvdata)
{
	int ret;

	/*
	 * Vote for clk on since tracing may or may not be enabled in
	 * swduart mode and hence the clk is not guaranteed to be enabled.
	 */
	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	ret = __tpiu_enable_to_sdc(drvdata);
	if (ret)
		goto err;

	/*
	 * Required sequence to prevent SRST asserstion: set trace to
	 * continuous mode followed by setting ETM MODE to 1 before switching
	 * to swd.
	 */
	__tpiu_enable(drvdata, 0x8, 0x103);

	msm_tlmm_misc_reg_write(TLMM_ETM_MODE_REG, 1);

	/* Pull down sdc cmd line */
	msm_tlmm_misc_reg_write(TLMM_SDC2_HDRV_PULL_CTL, 0x96D);
	msm_tlmm_misc_reg_write(TLMM_ETM_MODE_REG, 2);
err:
	return ret;
}

static int __tpiu_enable_to_sdc_swdtrc(struct tpiu_drvdata *drvdata)
{
	int ret;

	/*
	 * Vote for clk on since tracing may or may not be enabled in
	 * swdtrc mode and hence the clk is not guaranteed to be enabled.
	 */
	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	ret = __tpiu_enable_to_sdc(drvdata);
	if (ret)
		goto err;

	/*
	 * Required sequence to prevent SRST asserstion: set trace to
	 * continuous mode followed by setting ETM MODE to 1 before switching
	 * to swd.
	 */
	__tpiu_enable(drvdata, 0x2, 0x103);

	msm_tlmm_misc_reg_write(TLMM_ETM_MODE_REG, 1);

	/* Pull down sdc cmd line */
	msm_tlmm_misc_reg_write(TLMM_SDC2_HDRV_PULL_CTL, 0x96D);
	msm_tlmm_misc_reg_write(TLMM_ETM_MODE_REG, 3);
err:
	return ret;
}

static int tpiu_enable(struct coresight_device *csdev)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	mutex_lock(&drvdata->mutex);

	/*
	 * swd modes are enabled when stored in out_mode to allow debugging
	 * in swd modes.
	 */
	if (drvdata->out_mode == TPIU_OUT_MODE_MICTOR)
		ret = __tpiu_enable_to_mictor(drvdata);
	else if (drvdata->out_mode == TPIU_OUT_MODE_SDC_TRACE)
		ret = __tpiu_enable_to_sdc_trace(drvdata);
	if (ret)
		goto err;
	drvdata->enable = true;

	mutex_unlock(&drvdata->mutex);

	dev_info(drvdata->dev, "TPIU enabled\n");
	return 0;
err:
	mutex_unlock(&drvdata->mutex);
	clk_disable_unprepare(drvdata->clk);
	return ret;
}

static void __tpiu_disable(struct tpiu_drvdata *drvdata)
{
	TPIU_UNLOCK(drvdata);

	tpiu_flush_and_stop(drvdata);

	TPIU_LOCK(drvdata);
}

static void __tpiu_disable_seta(struct tpiu_drvdata *drvdata)
{
	int i;

	for (i = 0; i < drvdata->seta_gpiocnt; i++)
		gpio_free(drvdata->seta_gpios[i]);
}

static void __tpiu_disable_setb(struct tpiu_drvdata *drvdata)
{
	int i;

	for (i = 0; i < drvdata->setb_gpiocnt; i++)
		gpio_free(drvdata->setb_gpios[i]);
}

static void __tpiu_disable_to_mictor(struct tpiu_drvdata *drvdata)
{
	/* mictor mode needs to be disbled only when tracing is enabled */
	if (!drvdata->enable)
		return;

	__tpiu_disable(drvdata);

	if (drvdata->set == TPIU_SET_A)
		__tpiu_disable_seta(drvdata);
	else if (drvdata->set == TPIU_SET_B)
		__tpiu_disable_setb(drvdata);
}

static void __tpiu_disable_to_sdc(struct tpiu_drvdata *drvdata)
{
	msm_tlmm_misc_reg_write(TLMM_ETM_MODE_REG, 0);

	clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);

	regulator_disable(drvdata->reg);
	tpiu_reg_set_voltage(drvdata->reg, 0, drvdata->reg_high);
	tpiu_reg_set_optimum_mode(drvdata->reg, 0);

	regulator_disable(drvdata->reg_io);
	tpiu_reg_set_voltage(drvdata->reg_io, 0, drvdata->reg_high_io);
	tpiu_reg_set_optimum_mode(drvdata->reg_io, 0);
}

static void __tpiu_disable_to_sdc_trace(struct tpiu_drvdata *drvdata)
{
	/* sdc mode needs to be disabled only when tracing is enabled */
	if (!drvdata->enable)
		return;

	__tpiu_disable(drvdata);

	__tpiu_disable_to_sdc(drvdata);
}

static void __tpiu_disable_to_sdc_swduart(struct tpiu_drvdata *drvdata)
{
	__tpiu_disable(drvdata);

	__tpiu_disable_to_sdc(drvdata);

	clk_disable_unprepare(drvdata->clk);
}

static void __tpiu_disable_to_sdc_swdtrc(struct tpiu_drvdata *drvdata)
{
	__tpiu_disable(drvdata);

	__tpiu_disable_to_sdc(drvdata);

	clk_disable_unprepare(drvdata->clk);
}

static void __tpiu_disable_to_out_mode(struct tpiu_drvdata *drvdata)
{
	if (drvdata->out_mode == TPIU_OUT_MODE_MICTOR)
		__tpiu_disable_to_mictor(drvdata);
	else if (drvdata->out_mode == TPIU_OUT_MODE_SDC_TRACE)
		__tpiu_disable_to_sdc_trace(drvdata);
	else if (drvdata->out_mode == TPIU_OUT_MODE_SDC_SWDUART)
		__tpiu_disable_to_sdc_swduart(drvdata);
	else if (drvdata->out_mode == TPIU_OUT_MODE_SDC_SWDTRC)
		__tpiu_disable_to_sdc_swdtrc(drvdata);
}

static void tpiu_disable(struct coresight_device *csdev)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	mutex_lock(&drvdata->mutex);

	if (drvdata->out_mode == TPIU_OUT_MODE_MICTOR)
		__tpiu_disable_to_mictor(drvdata);
	else if (drvdata->out_mode == TPIU_OUT_MODE_SDC_TRACE)
		__tpiu_disable_to_sdc_trace(drvdata);
	drvdata->enable = false;

	mutex_unlock(&drvdata->mutex);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "TPIU disabled\n");
}

static void tpiu_abort(struct coresight_device *csdev)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	__tpiu_disable(drvdata);

	dev_info(drvdata->dev, "TPIU aborted\n");
}

static const struct coresight_ops_sink tpiu_sink_ops = {
	.enable		= tpiu_enable,
	.disable	= tpiu_disable,
	.abort		= tpiu_abort,
};

static ssize_t tpiu_show_out_mode(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t len;

	mutex_lock(&drvdata->mutex);

	len = scnprintf(buf, PAGE_SIZE, "%s\n",
			drvdata->out_mode == TPIU_OUT_MODE_MICTOR ?
			"mictor" : (drvdata->out_mode ==
			TPIU_OUT_MODE_SDC_TRACE ? "sdc" :
			(drvdata->out_mode == TPIU_OUT_MODE_SDC_SWDUART ?
			"swduart" : "swdtrc")));

	mutex_unlock(&drvdata->mutex);
	return len;
}

static ssize_t tpiu_store_out_mode(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";
	int ret;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->mutex);

	if (!strcmp(str, "mictor")) {
		if (drvdata->out_mode == TPIU_OUT_MODE_MICTOR)
			goto out;

		__tpiu_disable_to_out_mode(drvdata);

		if (!drvdata->enable) {
			drvdata->out_mode = TPIU_OUT_MODE_MICTOR;
			goto out;
		}

		ret = __tpiu_enable_to_mictor(drvdata);
		if (ret) {
			dev_err(drvdata->dev, "failed to enable mictor\n");
			goto err;
		}
		drvdata->out_mode = TPIU_OUT_MODE_MICTOR;
	} else if (!strcmp(str, "sdc")) {
		if (drvdata->out_mode == TPIU_OUT_MODE_SDC_TRACE)
			goto out;

		__tpiu_disable_to_out_mode(drvdata);

		if (!drvdata->enable) {
			drvdata->out_mode = TPIU_OUT_MODE_SDC_TRACE;
			goto out;
		}

		ret = __tpiu_enable_to_sdc_trace(drvdata);
		if (ret) {
			dev_err(drvdata->dev, "failed to enable sdc\n");
			goto err;
		}
		drvdata->out_mode = TPIU_OUT_MODE_SDC_TRACE;
	} else if (!strcmp(str, "swduart")) {
		if (!drvdata->nidnt) {
			ret = -EINVAL;
			goto err;
		}

		if (drvdata->out_mode == TPIU_OUT_MODE_SDC_SWDUART)
			goto out;

		/* Allow enabling swd modes even without tracing enabled */
		__tpiu_disable_to_out_mode(drvdata);

		ret = __tpiu_enable_to_sdc_swduart(drvdata);
		if (ret) {
			dev_err(drvdata->dev, "failed to enable swd uart\n");
			goto err;
		}
		drvdata->out_mode = TPIU_OUT_MODE_SDC_SWDUART;
	} else if (!strcmp(str, "swdtrc")) {
		if (!drvdata->nidnt) {
			ret = -EINVAL;
			goto err;
		}

		if (drvdata->out_mode == TPIU_OUT_MODE_SDC_SWDTRC)
			goto out;

		/* Allow enabling swd modes even without tracing enabled */
		__tpiu_disable_to_out_mode(drvdata);

		ret = __tpiu_enable_to_sdc_swdtrc(drvdata);
		if (ret) {
			dev_err(drvdata->dev, "failed to enable swd trace\n");
			goto err;
		}
		drvdata->out_mode = TPIU_OUT_MODE_SDC_SWDTRC;
	}
out:
	mutex_unlock(&drvdata->mutex);
	return size;
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
static DEVICE_ATTR(out_mode, S_IRUGO | S_IWUSR, tpiu_show_out_mode,
		   tpiu_store_out_mode);

static const struct coresight_ops tpiu_cs_ops = {
	.sink_ops	= &tpiu_sink_ops,
};

static ssize_t tpiu_show_set(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 drvdata->set == TPIU_SET_A ?
			 "a" : "b");
}

static ssize_t tpiu_store_set(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(dev->parent);
	char str[10] = "";
	int ret;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	mutex_lock(&drvdata->mutex);
	if (!strcmp(str, "a")) {
		if (drvdata->set == TPIU_SET_A)
			goto out;

		if (!drvdata->enable || drvdata->out_mode !=
					TPIU_OUT_MODE_MICTOR) {
			drvdata->set = TPIU_SET_A;
			goto out;
		}
		__tpiu_disable_setb(drvdata);
		ret = __tpiu_enable_seta(drvdata);
		if (ret) {
			dev_err(drvdata->dev, "failed to enable set A\n");
			goto err;
		}
		drvdata->set = TPIU_SET_A;
	} else if (!strcmp(str, "b")) {
		if (drvdata->set == TPIU_SET_B)
			goto out;

		if (!drvdata->enable || drvdata->out_mode !=
					TPIU_OUT_MODE_MICTOR) {
			drvdata->set = TPIU_SET_B;
			goto out;
		}
		__tpiu_disable_seta(drvdata);
		ret = __tpiu_enable_setb(drvdata);
		if (ret) {
			dev_err(drvdata->dev, "failed to enable set B\n");
			goto err;
		}
		drvdata->set = TPIU_SET_B;
	}
out:
	mutex_unlock(&drvdata->mutex);
	return size;
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
static DEVICE_ATTR(set, S_IRUGO | S_IWUSR, tpiu_show_set, tpiu_store_set);

static struct attribute *tpiu_attrs[] = {
	&dev_attr_out_mode.attr,
	&dev_attr_set.attr,
	NULL,
};

static struct attribute_group tpiu_attr_grp = {
	.attrs = tpiu_attrs,
};

static const struct attribute_group *tpiu_attr_grps[] = {
	&tpiu_attr_grp,
	NULL,
};

static int tpiu_parse_of_data(struct platform_device *pdev,
					struct tpiu_drvdata *drvdata)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *reg_node = NULL;
	struct device *dev = &pdev->dev;
	const __be32 *prop;
	int i, len, gpio, ret;
	uint32_t *seta_cfgs, *setb_cfgs;

	reg_node = of_parse_phandle(node, "vdd-supply", 0);
	if (reg_node) {
		drvdata->reg = devm_regulator_get(dev, "vdd");
		if (IS_ERR(drvdata->reg))
			return PTR_ERR(drvdata->reg);

		prop = of_get_property(node, "qcom,vdd-voltage-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(dev, "sdc voltage levels not specified\n");
		} else {
			drvdata->reg_low = be32_to_cpup(&prop[0]);
			drvdata->reg_high = be32_to_cpup(&prop[1]);
		}

		prop = of_get_property(node, "qcom,vdd-current-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(dev, "sdc current levels not specified\n");
		} else {
			drvdata->reg_lpm = be32_to_cpup(&prop[0]);
			drvdata->reg_hpm = be32_to_cpup(&prop[1]);
		}
		of_node_put(reg_node);
	} else {
		dev_err(dev, "sdc voltage supply not specified or available\n");
	}

	reg_node = of_parse_phandle(node, "vdd-io-supply", 0);
	if (reg_node) {
		drvdata->reg_io = devm_regulator_get(dev, "vdd-io");
		if (IS_ERR(drvdata->reg_io))
			return PTR_ERR(drvdata->reg_io);

		prop = of_get_property(node, "qcom,vdd-io-voltage-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(dev, "sdc io voltage levels not specified\n");
		} else {
			drvdata->reg_low_io = be32_to_cpup(&prop[0]);
			drvdata->reg_high_io = be32_to_cpup(&prop[1]);
		}

		prop = of_get_property(node, "qcom,vdd-io-current-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(dev, "sdc io current levels not specified\n");
		} else {
			drvdata->reg_lpm_io = be32_to_cpup(&prop[0]);
			drvdata->reg_hpm_io = be32_to_cpup(&prop[1]);
		}
		of_node_put(reg_node);
	} else {
		dev_err(dev,
			"sdc io voltage supply not specified or available\n");
	}

	drvdata->out_mode = TPIU_OUT_MODE_MICTOR;
	drvdata->set = TPIU_SET_B;

	drvdata->seta_gpiocnt = of_gpio_named_count(node, "qcom,seta-gpios");
	if (drvdata->seta_gpiocnt > 0) {
		drvdata->seta_gpios = devm_kzalloc(dev,
				sizeof(*drvdata->seta_gpios) *
				drvdata->seta_gpiocnt, GFP_KERNEL);
		if (!drvdata->seta_gpios)
			return -ENOMEM;

		for (i = 0; i < drvdata->seta_gpiocnt; i++) {
			gpio = of_get_named_gpio(node, "qcom,seta-gpios", i);
			if (!gpio_is_valid(gpio))
				return gpio;

			drvdata->seta_gpios[i] = gpio;
		}

		drvdata->seta_cfgs = devm_kzalloc(dev,
				sizeof(*drvdata->seta_cfgs) *
				drvdata->seta_gpiocnt, GFP_KERNEL);
		if (!drvdata->seta_cfgs)
			return -ENOMEM;

		seta_cfgs = devm_kzalloc(dev, sizeof(*seta_cfgs) *
					 drvdata->seta_gpiocnt, GFP_KERNEL);
		if (!seta_cfgs)
			return -ENOMEM;

		ret = of_property_read_u32_array(node, "qcom,seta-gpios-func",
						 (u32 *)seta_cfgs,
						 drvdata->seta_gpiocnt);
		if (ret)
			return ret;

		for (i = 0; i < drvdata->seta_gpiocnt; i++)
			drvdata->seta_cfgs[i].func = seta_cfgs[i];

		ret = of_property_read_u32_array(node, "qcom,seta-gpios-drv",
						 (u32 *)seta_cfgs,
						 drvdata->seta_gpiocnt);
		if (ret)
			return ret;

		for (i = 0; i < drvdata->seta_gpiocnt; i++)
			drvdata->seta_cfgs[i].drv = seta_cfgs[i];

		ret = of_property_read_u32_array(node, "qcom,seta-gpios-pull",
						 (u32 *)seta_cfgs,
						 drvdata->seta_gpiocnt);
		if (ret)
			return ret;

		for (i = 0; i < drvdata->seta_gpiocnt; i++)
			drvdata->seta_cfgs[i].pull = seta_cfgs[i];

		ret = of_property_read_u32_array(node, "qcom,seta-gpios-dir",
						 (u32 *)seta_cfgs,
						 drvdata->seta_gpiocnt);
		if (ret)
			return ret;

		for (i = 0; i < drvdata->seta_gpiocnt; i++)
			drvdata->seta_cfgs[i].dir = seta_cfgs[i];

		devm_kfree(dev, seta_cfgs);
	} else {
		dev_err(dev, "seta gpios not specified\n");
	}

	drvdata->setb_gpiocnt = of_gpio_named_count(node, "qcom,setb-gpios");
	if (drvdata->setb_gpiocnt > 0) {
		drvdata->setb_gpios = devm_kzalloc(dev,
				sizeof(*drvdata->setb_gpios) *
				drvdata->setb_gpiocnt, GFP_KERNEL);
		if (!drvdata->setb_gpios)
			return -ENOMEM;

		for (i = 0; i < drvdata->setb_gpiocnt; i++) {
			gpio = of_get_named_gpio(node, "qcom,setb-gpios", i);
			if (!gpio_is_valid(gpio))
				return gpio;

			drvdata->setb_gpios[i] = gpio;
		}

		drvdata->setb_cfgs = devm_kzalloc(dev,
				sizeof(*drvdata->setb_cfgs) *
				drvdata->setb_gpiocnt, GFP_KERNEL);
		if (!drvdata->setb_cfgs)
			return -ENOMEM;

		setb_cfgs = devm_kzalloc(dev, sizeof(*setb_cfgs) *
					 drvdata->setb_gpiocnt, GFP_KERNEL);
		if (!setb_cfgs)
			return -ENOMEM;

		ret = of_property_read_u32_array(node, "qcom,setb-gpios-func",
						 (u32 *)setb_cfgs,
						 drvdata->setb_gpiocnt);
		if (ret)
			return ret;

		for (i = 0; i < drvdata->setb_gpiocnt; i++)
			drvdata->setb_cfgs[i].func = setb_cfgs[i];

		ret = of_property_read_u32_array(node, "qcom,setb-gpios-drv",
						 (u32 *)setb_cfgs,
						 drvdata->setb_gpiocnt);
		if (ret)
			return ret;

		for (i = 0; i < drvdata->setb_gpiocnt; i++)
			drvdata->setb_cfgs[i].drv = setb_cfgs[i];

		ret = of_property_read_u32_array(node, "qcom,setb-gpios-pull",
						 (u32 *)setb_cfgs,
						 drvdata->setb_gpiocnt);
		if (ret)
			return ret;

		for (i = 0; i < drvdata->setb_gpiocnt; i++)
			drvdata->setb_cfgs[i].pull = setb_cfgs[i];

		ret = of_property_read_u32_array(node, "qcom,setb-gpios-dir",
						 (u32 *)setb_cfgs,
						 drvdata->setb_gpiocnt);
		if (ret)
			return ret;

		for (i = 0; i < drvdata->setb_gpiocnt; i++)
			drvdata->setb_cfgs[i].dir = setb_cfgs[i];

		devm_kfree(dev, setb_cfgs);
	} else {
		dev_err(dev, "setb gpios not specified\n");
	}

	drvdata->nidnt = of_property_read_bool(pdev->dev.of_node,
					       "qcom,nidnt");
	return 0;
}

static int tpiu_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct tpiu_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;

	if (coresight_fuse_access_disabled())
		return -EPERM;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		pdev->dev.platform_data = pdata;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tpiu-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	mutex_init(&drvdata->mutex);

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	/* Disable tpiu to support older targets that need this */
	__tpiu_disable(drvdata);

	clk_disable_unprepare(drvdata->clk);

	if (pdev->dev.of_node) {
		ret = tpiu_parse_of_data(pdev, drvdata);
		if (ret)
			return ret;
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_SINK;
	desc->subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_PORT;
	desc->ops = &tpiu_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = tpiu_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_info(dev, "TPIU initialized\n");
	return 0;
}

static int tpiu_remove(struct platform_device *pdev)
{
	struct tpiu_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id tpiu_match[] = {
	{.compatible = "arm,coresight-tpiu"},
	{}
};

static struct platform_driver tpiu_driver = {
	.probe          = tpiu_probe,
	.remove         = tpiu_remove,
	.driver         = {
		.name   = "coresight-tpiu",
		.owner	= THIS_MODULE,
		.of_match_table = tpiu_match,
	},
};

static int __init tpiu_init(void)
{
	return platform_driver_register(&tpiu_driver);
}
module_init(tpiu_init);

static void __exit tpiu_exit(void)
{
	platform_driver_unregister(&tpiu_driver);
}
module_exit(tpiu_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Trace Port Interface Unit driver");

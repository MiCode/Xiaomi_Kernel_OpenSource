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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/topology.h>
#include <linux/of.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <linux/coresight-cti.h>

#include "coresight-priv.h"

#define cti_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define cti_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define CTI_LOCK(drvdata)						\
do {									\
	mb();								\
	cti_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define CTI_UNLOCK(drvdata)						\
do {									\
	cti_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define CTICONTROL		(0x000)
#define CTIINTACK		(0x010)
#define CTIAPPSET		(0x014)
#define CTIAPPCLEAR		(0x018)
#define CTIAPPPULSE		(0x01C)
#define CTIINEN(n)		(0x020 + (n * 4))
#define CTIOUTEN(n)		(0x0A0 + (n * 4))
#define CTITRIGINSTATUS		(0x130)
#define CTITRIGOUTSTATUS	(0x134)
#define CTICHINSTATUS		(0x138)
#define CTICHOUTSTATUS		(0x13C)
#define CTIGATE			(0x140)
#define ASICCTL			(0x144)
#define ITCHINACK		(0xEDC)
#define ITTRIGINACK		(0xEE0)
#define ITCHOUT			(0xEE4)
#define ITTRIGOUT		(0xEE8)
#define ITCHOUTACK		(0xEEC)
#define ITTRIGOUTACK		(0xEF0)
#define ITCHIN			(0xEF4)
#define ITTRIGIN		(0xEF8)

#define CTI_MAX_TRIGGERS	(8)
#define CTI_MAX_CHANNELS	(4)

#define to_cti_drvdata(c) container_of(c, struct cti_drvdata, cti)

struct cti_state {
	unsigned int cticontrol;
	unsigned int ctiappset;
	unsigned int ctigate;
	unsigned int ctiinen[CTI_MAX_TRIGGERS];
	unsigned int ctiouten[CTI_MAX_TRIGGERS];
};

struct cti_pctrl {
	struct pinctrl			*pctrl;
	int				trig;
};

struct cti_drvdata {
	void __iomem			*base;
	struct device			*dev;
	struct coresight_device		*csdev;
	struct clk			*clk;
	spinlock_t			spinlock;
	struct mutex			mutex;
	struct coresight_cti		cti;
	int				refcnt;
	int				cpu;
	bool				cti_save;
	bool				cti_hwclk;
	bool				cti_ack_atomic;
	bool				l2_off;
	struct cti_state		*state;
	struct cti_pctrl		*gpio_trigin;
	struct cti_pctrl		*gpio_trigout;
};

static LIST_HEAD(cti_list);
static DEFINE_MUTEX(cti_lock);

static int cti_verify_trigger_bound(int trig)
{
	if (trig < 0 || trig >= CTI_MAX_TRIGGERS)
		return -EINVAL;

	return 0;
}

static int cti_verify_channel_bound(int ch)
{
	if (ch < 0 || ch >= CTI_MAX_CHANNELS)
		return -EINVAL;

	return 0;
}

static int cti_cpu_verify_access(struct cti_drvdata *drvdata)
{
	if (drvdata->cti_save && drvdata->l2_off)
		return -EPERM;

	return 0;
}

static int cti_enable(struct cti_drvdata *drvdata)
{
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	CTI_UNLOCK(drvdata);

	cti_writel(drvdata, 0x1, CTICONTROL);

	CTI_LOCK(drvdata);
	return 0;
}

int cti_trigin_gpio_enable(struct cti_drvdata *drvdata)
{
	int ret;
	struct pinctrl *pctrl;
	struct pinctrl_state *pctrl_state;

	if (drvdata->gpio_trigin->pctrl)
		return 0;

	pctrl = devm_pinctrl_get(drvdata->dev);
	if (IS_ERR(pctrl)) {
		dev_err(drvdata->dev, "pinctrl get failed\n");
		return PTR_ERR(pctrl);
	}

	pctrl_state = pinctrl_lookup_state(pctrl, "cti-trigin-pctrl");
	if (IS_ERR(pctrl_state)) {
		dev_err(drvdata->dev,
			"pinctrl get state failed\n");
		ret = PTR_ERR(pctrl_state);
		goto err;
	}

	ret = pinctrl_select_state(pctrl, pctrl_state);
	if (ret) {
		dev_err(drvdata->dev,
			"pinctrl enable state failed\n");
		goto err;
	}

	drvdata->gpio_trigin->pctrl = pctrl;
	return 0;
err:
	devm_pinctrl_put(pctrl);
	return ret;
}

int cti_trigout_gpio_enable(struct cti_drvdata *drvdata)
{
	int ret;
	struct pinctrl *pctrl;
	struct pinctrl_state *pctrl_state;

	if (drvdata->gpio_trigout->pctrl)
		return 0;

	pctrl = devm_pinctrl_get(drvdata->dev);
	if (IS_ERR(pctrl)) {
		dev_err(drvdata->dev, "pinctrl get failed\n");
		return PTR_ERR(pctrl);
	}

	pctrl_state = pinctrl_lookup_state(pctrl, "cti-trigout-pctrl");
	if (IS_ERR(pctrl_state)) {
		dev_err(drvdata->dev,
			"pinctrl get state failed\n");
		ret = PTR_ERR(pctrl_state);
		goto err;
	}

	ret = pinctrl_select_state(pctrl, pctrl_state);
	if (ret) {
		dev_err(drvdata->dev,
			"pinctrl enable state failed\n");
		goto err;
	}

	drvdata->gpio_trigout->pctrl = pctrl;
	return 0;
err:
	devm_pinctrl_put(pctrl);
	return ret;
}

void cti_trigin_gpio_disable(struct cti_drvdata *drvdata)
{
	if (!drvdata->gpio_trigin->pctrl)
		return;

	devm_pinctrl_put(drvdata->gpio_trigin->pctrl);
	drvdata->gpio_trigin->pctrl = NULL;
}

void cti_trigout_gpio_disable(struct cti_drvdata *drvdata)
{
	if (!drvdata->gpio_trigout->pctrl)
		return;

	devm_pinctrl_put(drvdata->gpio_trigout->pctrl);
	drvdata->gpio_trigout->pctrl = NULL;
}

static int __cti_map_trigin(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;
	int ret;

	if (drvdata->refcnt == 0) {
		ret = cti_enable(drvdata);
		if (ret)
			return ret;
	}

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIINEN(trig));
	if (ctien & (0x1 << ch))
		goto out;
	cti_writel(drvdata, (ctien | 0x1 << ch), CTIINEN(trig));

	CTI_LOCK(drvdata);

	drvdata->refcnt++;
	return 0;
out:
	CTI_LOCK(drvdata);
	return 0;
}

int coresight_cti_map_trigin(struct coresight_cti *cti, int trig, int ch)
{
	struct cti_drvdata *drvdata;
	int ret;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return -EINVAL;
	ret = cti_verify_trigger_bound(trig);
	if (ret)
		return ret;
	ret = cti_verify_channel_bound(ch);
	if (ret)
		return ret;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);

	if (drvdata->gpio_trigin->trig == trig) {
		ret = cti_trigin_gpio_enable(drvdata);
		if (ret)
			goto err0;
	}

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err1;

	ret = __cti_map_trigin(drvdata, trig, ch);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	mutex_unlock(&drvdata->mutex);
	return 0;
err1:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	cti_trigin_gpio_disable(drvdata);
err0:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_map_trigin);

static int __cti_map_trigout(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;
	int ret;

	if (drvdata->refcnt == 0) {
		ret = cti_enable(drvdata);
		if (ret)
			return ret;
	}

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIOUTEN(trig));
	if (ctien & (0x1 << ch))
		goto out;
	cti_writel(drvdata, (ctien | 0x1 << ch), CTIOUTEN(trig));

	CTI_LOCK(drvdata);

	drvdata->refcnt++;
	return 0;
out:
	CTI_LOCK(drvdata);
	return 0;
}

int coresight_cti_map_trigout(struct coresight_cti *cti, int trig, int ch)
{
	struct cti_drvdata *drvdata;
	int ret;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return -EINVAL;
	ret = cti_verify_trigger_bound(trig);
	if (ret)
		return ret;
	ret = cti_verify_channel_bound(ch);
	if (ret)
		return ret;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);

	if (drvdata->gpio_trigout->trig == trig) {
		ret = cti_trigout_gpio_enable(drvdata);
		if (ret)
			goto err0;
	}

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err1;

	ret = __cti_map_trigout(drvdata, trig, ch);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	mutex_unlock(&drvdata->mutex);
	return 0;
err1:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	cti_trigout_gpio_disable(drvdata);
err0:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_map_trigout);

static void cti_disable(struct cti_drvdata *drvdata)
{
	CTI_UNLOCK(drvdata);

	/* Clear any pending triggers and ensure gate is enabled */
	cti_writel(drvdata, BM(0, (CTI_MAX_CHANNELS - 1)), CTIAPPCLEAR);
	cti_writel(drvdata, BM(0, (CTI_MAX_CHANNELS - 1)), CTIGATE);

	cti_writel(drvdata, 0x0, CTICONTROL);

	CTI_LOCK(drvdata);

	clk_disable_unprepare(drvdata->clk);
}

static void __cti_unmap_trigin(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIINEN(trig));
	if (!(ctien & (0x1 << ch)))
		goto out;
	cti_writel(drvdata, (ctien & ~(0x1 << ch)), CTIINEN(trig));

	CTI_LOCK(drvdata);

	if (drvdata->refcnt == 1)
		cti_disable(drvdata);
	drvdata->refcnt--;
	return;
out:
	CTI_LOCK(drvdata);
	return;
}

void coresight_cti_unmap_trigin(struct coresight_cti *cti, int trig, int ch)
{
	struct cti_drvdata *drvdata;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return;
	if (cti_verify_trigger_bound(trig))
		return;
	if (cti_verify_channel_bound(ch))
		return;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);

	spin_lock_irqsave(&drvdata->spinlock, flag);
	if (cti_cpu_verify_access(drvdata))
		goto err;

	__cti_unmap_trigin(drvdata, trig, ch);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	if (drvdata->gpio_trigin->trig == trig)
		cti_trigin_gpio_disable(drvdata);

	mutex_unlock(&drvdata->mutex);
	return;
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	mutex_unlock(&drvdata->mutex);
}
EXPORT_SYMBOL(coresight_cti_unmap_trigin);

static void __cti_unmap_trigout(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIOUTEN(trig));
	if (!(ctien & (0x1 << ch)))
		goto out;
	cti_writel(drvdata, (ctien & ~(0x1 << ch)), CTIOUTEN(trig));

	CTI_LOCK(drvdata);

	if (drvdata->refcnt == 1)
		cti_disable(drvdata);
	drvdata->refcnt--;
	return;
out:
	CTI_LOCK(drvdata);
	return;
}

void coresight_cti_unmap_trigout(struct coresight_cti *cti, int trig, int ch)
{
	struct cti_drvdata *drvdata;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return;
	if (cti_verify_trigger_bound(trig))
		return;
	if (cti_verify_channel_bound(ch))
		return;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);

	spin_lock_irqsave(&drvdata->spinlock, flag);
	if (cti_cpu_verify_access(drvdata))
		goto err;

	__cti_unmap_trigout(drvdata, trig, ch);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	if (drvdata->gpio_trigout->trig == trig)
		cti_trigout_gpio_disable(drvdata);

	mutex_unlock(&drvdata->mutex);
	return;
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	mutex_unlock(&drvdata->mutex);
}
EXPORT_SYMBOL(coresight_cti_unmap_trigout);

static void __cti_reset(struct cti_drvdata *drvdata)
{
	int trig;

	if (!drvdata->refcnt)
		return;

	CTI_UNLOCK(drvdata);

	for (trig = 0; trig < CTI_MAX_TRIGGERS; trig++) {
		cti_writel(drvdata, 0, CTIINEN(trig));
		cti_writel(drvdata, 0, CTIOUTEN(trig));
	}

	CTI_LOCK(drvdata);

	cti_disable(drvdata);
	drvdata->refcnt = 0;
}

void coresight_cti_reset(struct coresight_cti *cti)
{
	struct cti_drvdata *drvdata;
	unsigned long flag;
	int trig;

	if (IS_ERR_OR_NULL(cti))
		return;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);

	spin_lock_irqsave(&drvdata->spinlock, flag);
	if (cti_cpu_verify_access(drvdata))
		goto err;

	__cti_reset(drvdata);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	for (trig = 0; trig < CTI_MAX_TRIGGERS; trig++) {
		if (drvdata->gpio_trigin->trig == trig)
			cti_trigin_gpio_disable(drvdata);
		if (drvdata->gpio_trigout->trig == trig)
			cti_trigout_gpio_disable(drvdata);
	}

	mutex_unlock(&drvdata->mutex);
	return;
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	mutex_unlock(&drvdata->mutex);
}
EXPORT_SYMBOL(coresight_cti_reset);

static int __cti_set_trig(struct cti_drvdata *drvdata, int ch)
{
	if (!drvdata->refcnt)
		return -EINVAL;

	CTI_UNLOCK(drvdata);

	cti_writel(drvdata, (1 << ch), CTIAPPSET);

	CTI_LOCK(drvdata);

	return 0;
}

int coresight_cti_set_trig(struct coresight_cti *cti, int ch)
{
	struct cti_drvdata *drvdata;
	int ret;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return -EINVAL;
	ret = cti_verify_channel_bound(ch);
	if (ret)
		return ret;

	drvdata = to_cti_drvdata(cti);

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err;

	ret = __cti_set_trig(drvdata, ch);
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_set_trig);

static void __cti_clear_trig(struct cti_drvdata *drvdata, int ch)
{
	if (!drvdata->refcnt)
		return;

	CTI_UNLOCK(drvdata);

	cti_writel(drvdata, (1 << ch), CTIAPPCLEAR);

	CTI_LOCK(drvdata);
}

void coresight_cti_clear_trig(struct coresight_cti *cti, int ch)
{
	struct cti_drvdata *drvdata;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return;
	if (cti_verify_channel_bound(ch))
		return;

	drvdata = to_cti_drvdata(cti);

	spin_lock_irqsave(&drvdata->spinlock, flag);
	if (cti_cpu_verify_access(drvdata))
		goto err;

	__cti_clear_trig(drvdata, ch);
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
}
EXPORT_SYMBOL(coresight_cti_clear_trig);

static int __cti_pulse_trig(struct cti_drvdata *drvdata, int ch)
{
	if (!drvdata->refcnt)
		return -EINVAL;

	CTI_UNLOCK(drvdata);

	cti_writel(drvdata, (1 << ch), CTIAPPPULSE);

	CTI_LOCK(drvdata);

	return 0;
}

int coresight_cti_pulse_trig(struct coresight_cti *cti, int ch)
{
	struct cti_drvdata *drvdata;
	int ret;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return -EINVAL;
	ret = cti_verify_channel_bound(ch);
	if (ret)
		return ret;

	drvdata = to_cti_drvdata(cti);

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err;

	ret = __cti_pulse_trig(drvdata, ch);
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_pulse_trig);

static int __cti_ack_trig(struct cti_drvdata *drvdata, int trig)
{
	if (!drvdata->refcnt)
		return -EINVAL;

	CTI_UNLOCK(drvdata);

	cti_writel(drvdata, (0x1 << trig), CTIINTACK);

	CTI_LOCK(drvdata);

	return 0;
}

int coresight_cti_ack_trig(struct coresight_cti *cti, int trig)
{
	struct cti_drvdata *drvdata;
	int ret;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return -EINVAL;
	ret = cti_verify_trigger_bound(trig);
	if (ret)
		return ret;

	drvdata = to_cti_drvdata(cti);

	if (!drvdata->cti_ack_atomic) {
		ret = clk_prepare_enable(drvdata->clk);
		if (ret)
			return ret;
	}

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err;

	ret = __cti_ack_trig(drvdata, trig);
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	if (!drvdata->cti_ack_atomic)
		clk_disable_unprepare(drvdata->clk);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_ack_trig);

static int __cti_enable_gate(struct cti_drvdata *drvdata, int ch)
{
	uint32_t ctigate;

	if (!drvdata->refcnt)
		return -EINVAL;

	CTI_UNLOCK(drvdata);

	ctigate = cti_readl(drvdata, CTIGATE);
	cti_writel(drvdata, (ctigate & ~(1 << ch)), CTIGATE);

	CTI_LOCK(drvdata);

	return 0;
}

int coresight_cti_enable_gate(struct coresight_cti *cti, int ch)
{
	struct cti_drvdata *drvdata;
	int ret;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return -EINVAL;
	ret = cti_verify_channel_bound(ch);
	if (ret)
		return ret;

	drvdata = to_cti_drvdata(cti);

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err;

	ret = __cti_enable_gate(drvdata, ch);
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_enable_gate);

static void __cti_disable_gate(struct cti_drvdata *drvdata, int ch)
{
	uint32_t ctigate;

	if (!drvdata->refcnt)
		return;

	CTI_UNLOCK(drvdata);

	ctigate = cti_readl(drvdata, CTIGATE);
	cti_writel(drvdata, (ctigate | (1 << ch)), CTIGATE);

	CTI_LOCK(drvdata);
}

void coresight_cti_disable_gate(struct coresight_cti *cti, int ch)
{
	struct cti_drvdata *drvdata;
	unsigned long flag;

	if (IS_ERR_OR_NULL(cti))
		return;
	if (cti_verify_channel_bound(ch))
		return;

	drvdata = to_cti_drvdata(cti);

	spin_lock_irqsave(&drvdata->spinlock, flag);
	if (cti_cpu_verify_access(drvdata))
		goto err;

	__cti_disable_gate(drvdata, ch);
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
}
EXPORT_SYMBOL(coresight_cti_disable_gate);

struct coresight_cti *coresight_cti_get(const char *name)
{
	struct coresight_cti *cti;

	mutex_lock(&cti_lock);
	list_for_each_entry(cti, &cti_list, link) {
		if (!strncmp(cti->name, name, strlen(cti->name) + 1)) {
			mutex_unlock(&cti_lock);
			return cti;
		}
	}
	mutex_unlock(&cti_lock);

	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(coresight_cti_get);

void coresight_cti_put(struct coresight_cti *cti)
{
}
EXPORT_SYMBOL(coresight_cti_put);

static ssize_t cti_show_trigin(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long trig, ch, flag;
	uint32_t ctien;
	ssize_t size = 0;

	mutex_lock(&cti_lock);
	if (!drvdata->refcnt)
		goto err;

	for (trig = 0; trig < CTI_MAX_TRIGGERS; trig++) {
		spin_lock_irqsave(&drvdata->spinlock, flag);
		if (!cti_cpu_verify_access(drvdata))
			ctien = cti_readl(drvdata, CTIINEN(trig));
		else
			ctien = drvdata->state->ctiinen[trig];
		spin_unlock_irqrestore(&drvdata->spinlock, flag);
		for (ch = 0; ch < CTI_MAX_CHANNELS; ch++) {
			if (ctien & (1 << ch)) {
				/* Ensure we do not write more than PAGE_SIZE
				 * bytes of data including \n character and null
				 * terminator
				 */
				size += scnprintf(&buf[size], PAGE_SIZE - size -
						  1, " %#lx %#lx,", trig, ch);
				if (size >= PAGE_SIZE - 2) {
					dev_err(dev, "show buffer full\n");
					goto err;
				}

			}
		}
	}
err:
	size += scnprintf(&buf[size], 2, "\n");
	mutex_unlock(&cti_lock);
	return size;
}
static DEVICE_ATTR(show_trigin, S_IRUGO, cti_show_trigin, NULL);

static ssize_t cti_show_trigout(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long trig, ch, flag;
	uint32_t ctien;
	ssize_t size = 0;

	mutex_lock(&cti_lock);
	if (!drvdata->refcnt)
		goto err;

	for (trig = 0; trig < CTI_MAX_TRIGGERS; trig++) {
		spin_lock_irqsave(&drvdata->spinlock, flag);
		if (!cti_cpu_verify_access(drvdata))
			ctien = cti_readl(drvdata, CTIOUTEN(trig));
		else
			ctien = drvdata->state->ctiouten[trig];
		spin_unlock_irqrestore(&drvdata->spinlock, flag);
		for (ch = 0; ch < CTI_MAX_CHANNELS; ch++) {
			if (ctien & (1 << ch)) {
				/* Ensure we do not write more than PAGE_SIZE
				 * bytes of data including \n character and null
				 * terminator
				 */
				size += scnprintf(&buf[size], PAGE_SIZE - size -
						  1, " %#lx %#lx,", trig, ch);
				if (size >= PAGE_SIZE - 2) {
					dev_err(dev, "show buffer full\n");
					goto err;
				}

			}
		}
	}
err:
	size += scnprintf(&buf[size], 2, "\n");
	mutex_unlock(&cti_lock);
	return size;
}
static DEVICE_ATTR(show_trigout, S_IRUGO, cti_show_trigout, NULL);

static ssize_t cti_store_map_trigin(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;
	int ret;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	ret = coresight_cti_map_trigin(&drvdata->cti, val1, val2);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(map_trigin, S_IWUSR, NULL, cti_store_map_trigin);

static ssize_t cti_store_map_trigout(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;
	int ret;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	ret = coresight_cti_map_trigout(&drvdata->cti, val1, val2);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(map_trigout, S_IWUSR, NULL, cti_store_map_trigout);

static ssize_t cti_store_unmap_trigin(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	coresight_cti_unmap_trigin(&drvdata->cti, val1, val2);

	return size;
}
static DEVICE_ATTR(unmap_trigin, S_IWUSR, NULL, cti_store_unmap_trigin);

static ssize_t cti_store_unmap_trigout(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	coresight_cti_unmap_trigout(&drvdata->cti, val1, val2);

	return size;
}
static DEVICE_ATTR(unmap_trigout, S_IWUSR, NULL, cti_store_unmap_trigout);

static ssize_t cti_store_reset(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (!val)
		return -EINVAL;

	coresight_cti_reset(&drvdata->cti);
	return size;
}
static DEVICE_ATTR(reset, S_IWUSR, NULL, cti_store_reset);

static ssize_t cti_show_trig(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long ch, flag;
	uint32_t ctiset;
	ssize_t size = 0;

	mutex_lock(&cti_lock);
	if (!drvdata->refcnt)
		goto err;

	spin_lock_irqsave(&drvdata->spinlock, flag);
	if (!cti_cpu_verify_access(drvdata))
		ctiset = cti_readl(drvdata, CTIAPPSET);
	else
		ctiset = drvdata->state->ctiappset;
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	for (ch = 0; ch < CTI_MAX_CHANNELS; ch++) {
		if (ctiset & (1 << ch)) {
			/* Ensure we do not write more than PAGE_SIZE
			 * bytes of data including \n character and null
			 * terminator
			 */
			size += scnprintf(&buf[size], PAGE_SIZE - size -
					  1, " %#lx,", ch);
			if (size >= PAGE_SIZE - 2) {
				dev_err(dev, "show buffer full\n");
				goto err;
			}

		}
	}
err:
	size += scnprintf(&buf[size], 2, "\n");
	mutex_unlock(&cti_lock);
	return size;
}
static DEVICE_ATTR(show_trig, S_IRUGO, cti_show_trig, NULL);

static ssize_t cti_store_set_trig(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	ret = coresight_cti_set_trig(&drvdata->cti, val);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(set_trig, S_IWUSR, NULL, cti_store_set_trig);

static ssize_t cti_store_clear_trig(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	coresight_cti_clear_trig(&drvdata->cti, val);

	return size;
}
static DEVICE_ATTR(clear_trig, S_IWUSR, NULL, cti_store_clear_trig);

static ssize_t cti_store_pulse_trig(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	ret = coresight_cti_pulse_trig(&drvdata->cti, val);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(pulse_trig, S_IWUSR, NULL, cti_store_pulse_trig);

static ssize_t cti_store_ack_trig(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	ret = coresight_cti_ack_trig(&drvdata->cti, val);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(ack_trig, S_IWUSR, NULL, cti_store_ack_trig);

static ssize_t cti_show_gate(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long ch, flag;
	uint32_t ctigate;
	ssize_t size = 0;

	mutex_lock(&cti_lock);
	if (!drvdata->refcnt)
		goto err;

	spin_lock_irqsave(&drvdata->spinlock, flag);
	if (!cti_cpu_verify_access(drvdata))
		ctigate = cti_readl(drvdata, CTIGATE);
	else
		ctigate = drvdata->state->ctigate;
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
	for (ch = 0; ch < CTI_MAX_CHANNELS; ch++) {
		if (ctigate & (1 << ch)) {
			/* Ensure we do not write more than PAGE_SIZE
			 * bytes of data including \n character and null
			 * terminator
			 */
			size += scnprintf(&buf[size], PAGE_SIZE - size -
					  1, " %#lx,", ch);
			if (size >= PAGE_SIZE - 2) {
				dev_err(dev, "show buffer full\n");
				goto err;
			}

		}
	}
err:
	size += scnprintf(&buf[size], 2, "\n");
	mutex_unlock(&cti_lock);
	return size;
}
static DEVICE_ATTR(show_gate, S_IRUGO, cti_show_gate, NULL);

static ssize_t cti_store_enable_gate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	ret = coresight_cti_enable_gate(&drvdata->cti, val);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(enable_gate, S_IWUSR, NULL, cti_store_enable_gate);

static ssize_t cti_store_disable_gate(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	coresight_cti_disable_gate(&drvdata->cti, val);

	return size;
}
static DEVICE_ATTR(disable_gate, S_IWUSR, NULL, cti_store_disable_gate);

static struct attribute *cti_attrs[] = {
	&dev_attr_show_trigin.attr,
	&dev_attr_show_trigout.attr,
	&dev_attr_map_trigin.attr,
	&dev_attr_map_trigout.attr,
	&dev_attr_unmap_trigin.attr,
	&dev_attr_unmap_trigout.attr,
	&dev_attr_reset.attr,
	&dev_attr_show_trig.attr,
	&dev_attr_set_trig.attr,
	&dev_attr_clear_trig.attr,
	&dev_attr_pulse_trig.attr,
	&dev_attr_ack_trig.attr,
	&dev_attr_show_gate.attr,
	&dev_attr_enable_gate.attr,
	&dev_attr_disable_gate.attr,
	NULL,
};

static struct attribute_group cti_attr_grp = {
	.attrs = cti_attrs,
};

static const struct attribute_group *cti_attr_grps[] = {
	&cti_attr_grp,
	NULL,
};

static int cti_probe(struct platform_device *pdev)
{
	int ret, cpu;
	int trig;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct cti_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;
	struct device_node *cpu_node;

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
	/* Store the driver data pointer for use in exported functions */
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cti-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	spin_lock_init(&drvdata->spinlock);

	mutex_init(&drvdata->mutex);

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	drvdata->gpio_trigin = devm_kzalloc(dev, sizeof(struct cti_pctrl),
					    GFP_KERNEL);
	if (!drvdata->gpio_trigin)
		return -ENOMEM;

	drvdata->gpio_trigin->trig = -1;
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,cti-gpio-trigin", &trig);
		if (!ret)
			drvdata->gpio_trigin->trig = trig;
		else if (ret != -EINVAL)
			return ret;
	}

	drvdata->gpio_trigout = devm_kzalloc(dev, sizeof(struct cti_pctrl),
					     GFP_KERNEL);
	if (!drvdata->gpio_trigout)
		return -ENOMEM;

	drvdata->gpio_trigout->trig = -1;
	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,cti-gpio-trigout", &trig);
		if (!ret)
			drvdata->gpio_trigout->trig = trig;
		else if (ret != -EINVAL)
			return ret;
	}

	if (pdev->dev.of_node) {
		drvdata->cti_ack_atomic =
			 of_property_read_bool(pdev->dev.of_node,
					       "qcom,cti-ack-atomic");

		drvdata->cti_save = of_property_read_bool(pdev->dev.of_node,
							  "qcom,cti-save");
	}

	if (drvdata->cti_save)
		drvdata->cti_hwclk = of_property_read_bool(pdev->dev.of_node,
							   "qcom,cti-hwclk");

	if ((drvdata->cti_save && !drvdata->cti_hwclk) ||
	    drvdata->cti_ack_atomic) {
		ret = clk_prepare_enable(drvdata->clk);
		if (ret)
			return ret;
	}

	drvdata->cpu = -1;

	if (drvdata->cti_save) {
		cpu_node = of_parse_phandle(pdev->dev.of_node,
					    "coresight-cti-cpu", 0);
		if (!cpu_node) {
			dev_err(drvdata->dev,
				"CTI cpu phandle not specified\n");
			ret = -ENODEV;
			goto err;
		}
		for_each_possible_cpu(cpu) {
			if (cpu_node == of_get_cpu_node(cpu, NULL)) {
				drvdata->cpu = cpu;
				break;
			}
		}
		if (drvdata->cpu == -1) {
			dev_err(drvdata->dev,
				"invalid CTI cpu phandle\n");
			ret = -EINVAL;
			goto err;
		}
		drvdata->state = devm_kzalloc(dev,
					      sizeof(struct cti_state),
					      GFP_KERNEL);
		if (!drvdata->state) {
			ret = -ENOMEM;
			goto err;
		}
	}

	mutex_lock(&cti_lock);
	drvdata->cti.name = ((struct coresight_platform_data *)
			     (pdev->dev.platform_data))->name;
	list_add_tail(&drvdata->cti.link, &cti_list);
	mutex_unlock(&cti_lock);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto err;
	}
	desc->type = CORESIGHT_DEV_TYPE_NONE;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = cti_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto err;
	}

	dev_info(dev, "CTI initialized\n");
	return 0;
err:
	if ((drvdata->cti_save && !drvdata->cti_hwclk) ||
	    drvdata->cti_ack_atomic)
		clk_disable_unprepare(drvdata->clk);
	return ret;
}

static int cti_remove(struct platform_device *pdev)
{
	struct cti_drvdata *drvdata = platform_get_drvdata(pdev);

	if ((drvdata->cti_save && !drvdata->cti_hwclk) ||
	    drvdata->cti_ack_atomic)
		clk_disable_unprepare(drvdata->clk);
	coresight_unregister(drvdata->csdev);
	return 0;
}

void coresight_cti_ctx_save(void)
{
	struct cti_drvdata *drvdata;
	struct coresight_cti *cti;
	int trig, cpuid, cpu;
	unsigned long flag;

	cpu = raw_smp_processor_id();

	list_for_each_entry(cti, &cti_list, link) {
		drvdata = to_cti_drvdata(cti);
		if (!drvdata->cti_save)
			continue;

		for_each_cpu_mask(cpuid, *topology_core_cpumask(cpu)) {
			if (drvdata->cpu == cpuid)
				goto out;
		}
		continue;
out:
		spin_lock_irqsave(&drvdata->spinlock, flag);
		drvdata->l2_off = true;
		drvdata->state->cticontrol = cti_readl(drvdata, CTICONTROL);
		drvdata->state->ctiappset = cti_readl(drvdata, CTIAPPSET);
		drvdata->state->ctigate = cti_readl(drvdata, CTIGATE);
		for (trig = 0; trig < CTI_MAX_TRIGGERS; trig++) {
			drvdata->state->ctiinen[trig] =
				cti_readl(drvdata, CTIINEN(trig));
			drvdata->state->ctiouten[trig] =
				cti_readl(drvdata, CTIOUTEN(trig));
		}
		spin_unlock_irqrestore(&drvdata->spinlock, flag);
	}
}
EXPORT_SYMBOL(coresight_cti_ctx_save);

void coresight_cti_ctx_restore(void)
{
	struct cti_drvdata *drvdata;
	struct coresight_cti *cti;
	int trig, cpuid, cpu;
	unsigned long flag;

	cpu = raw_smp_processor_id();

	list_for_each_entry(cti, &cti_list, link) {
		drvdata = to_cti_drvdata(cti);
		if (!drvdata->cti_save)
			continue;

		for_each_cpu_mask(cpuid, *topology_core_cpumask(cpu)) {
			if (drvdata->cpu == cpuid)
				goto out;
		}
		continue;
out:
		spin_lock_irqsave(&drvdata->spinlock, flag);
		CTI_UNLOCK(drvdata);
		cti_writel(drvdata, drvdata->state->ctiappset, CTIAPPSET);
		cti_writel(drvdata, drvdata->state->ctigate, CTIGATE);
		for (trig = 0; trig < CTI_MAX_TRIGGERS; trig++) {
			cti_writel(drvdata, drvdata->state->ctiinen[trig],
				   CTIINEN(trig));
			cti_writel(drvdata, drvdata->state->ctiouten[trig],
				   CTIOUTEN(trig));
		}
		cti_writel(drvdata, drvdata->state->cticontrol, CTICONTROL);
		CTI_LOCK(drvdata);
		drvdata->l2_off = false;
		spin_unlock_irqrestore(&drvdata->spinlock, flag);
	}
}
EXPORT_SYMBOL(coresight_cti_ctx_restore);

static struct of_device_id cti_match[] = {
	{.compatible = "arm,coresight-cti"},
	{}
};

static struct platform_driver cti_driver = {
	.probe          = cti_probe,
	.remove         = cti_remove,
	.driver         = {
		.name   = "coresight-cti",
		.owner	= THIS_MODULE,
		.of_match_table = cti_match,
	},
};

static int __init cti_init(void)
{
	return platform_driver_register(&cti_driver);
}
module_init(cti_init);

static void __exit cti_exit(void)
{
	platform_driver_unregister(&cti_driver);
}
module_exit(cti_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight CTI driver");

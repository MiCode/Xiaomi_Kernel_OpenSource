// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
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
#include <linux/amba/bus.h>
#include <linux/cpu_pm.h>
#include <linux/topology.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/coresight-cti.h>

#include "coresight-priv.h"

#define cti_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define cti_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define CTI_LOCK(drvdata)						\
do {									\
	mb(); /* ensure configuration take effect before we lock it */	\
	cti_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define CTI_UNLOCK(drvdata)						\
do {									\
	cti_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb(); /* ensure unlock take effect before we configure */	\
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
#define DEVID			(0xFC8)

#define CTI_MAX_TRIGGERS	(32)
#define CTI_MAX_CHANNELS	(4)
#define AFFINITY_LEVEL_L2	1

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
	bool				l2_off;
	struct cti_state		*state;
	struct cti_pctrl		*gpio_trigin;
	struct cti_pctrl		*gpio_trigout;
};

static struct notifier_block cti_cpu_pm_notifier;
static int registered;

DEFINE_CORESIGHT_DEVLIST(cti_devs, "cti");
static LIST_HEAD(cti_list);
static DEFINE_MUTEX(cti_lock);
#ifdef CONFIG_CORESIGHT_CTI_SAVE_DISABLE
static int cti_save_disable = 1;
#else
static int cti_save_disable;
#endif

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

void coresight_cti_ctx_save(void)
{
	struct cti_drvdata *drvdata;
	struct coresight_cti *cti;
	int trig, cpuid, cpu;
	unsigned long flag;

	/*
	 * Explicitly check and return to avoid latency associated with
	 * traversing the linked list of all CTIs and checking for their
	 * respective cti_save flag.
	 */
	if (cti_save_disable)
		return;

	cpu = raw_smp_processor_id();

	list_for_each_entry(cti, &cti_list, link) {
		drvdata = to_cti_drvdata(cti);
		if (!drvdata->cti_save)
			continue;

		for_each_cpu(cpuid, topology_core_cpumask(cpu)) {
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

	/*
	 * Explicitly check and return to avoid latency associated with
	 * traversing the linked list of all CTIs and checking for their
	 * respective cti_save flag.
	 */
	if (cti_save_disable)
		return;

	cpu = raw_smp_processor_id();

	list_for_each_entry(cti, &cti_list, link) {
		drvdata = to_cti_drvdata(cti);
		if (!drvdata->cti_save)
			continue;

		for_each_cpu(cpuid, topology_core_cpumask(cpu)) {
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

static void cti_enable(struct cti_drvdata *drvdata)
{
	CTI_UNLOCK(drvdata);

	cti_writel(drvdata, 0x1, CTICONTROL);

	CTI_LOCK(drvdata);
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

static void __cti_map_trigin(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;

	if (drvdata->refcnt == 0)
		cti_enable(drvdata);

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIINEN(trig));
	if (ctien & (0x1 << ch))
		goto out;
	cti_writel(drvdata, (ctien | 0x1 << ch), CTIINEN(trig));

	CTI_LOCK(drvdata);

	drvdata->refcnt++;
	return;
out:
	CTI_LOCK(drvdata);
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

	/*
	 * refcnt can be used here since in all cases its value is modified only
	 * within the mutex lock region in addition to within the spinlock.
	 */
	if (drvdata->refcnt == 0) {
		ret = pm_runtime_get_sync(drvdata->dev);
		if (ret < 0)
			goto err1;
		ret = coresight_enable_reg_clk(drvdata->csdev);
		if (ret)
			goto err2;
	}

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err3;

	__cti_map_trigin(drvdata, trig, ch);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	mutex_unlock(&drvdata->mutex);
	return 0;
err3:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	if (drvdata->refcnt == 0)
		coresight_disable_reg_clk(drvdata->csdev);
err2:
	/*
	 * We come here before refcnt is potentially modified in
	 * __cti_map_trigin so it is safe to check it against 0 without
	 * adjusting its value.
	 */
	if (drvdata->refcnt == 0)
		pm_runtime_put(drvdata->dev);
err1:
	cti_trigin_gpio_disable(drvdata);
err0:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_map_trigin);

static void __cti_map_trigout(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;

	if (drvdata->refcnt == 0)
		cti_enable(drvdata);

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIOUTEN(trig));
	if (ctien & (0x1 << ch))
		goto out;
	cti_writel(drvdata, (ctien | 0x1 << ch), CTIOUTEN(trig));

	CTI_LOCK(drvdata);

	drvdata->refcnt++;
	return;
out:
	CTI_LOCK(drvdata);
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

	/*
	 * refcnt can be used here since in all cases its value is modified only
	 * within the mutex lock region in addition to within the spinlock.
	 */
	if (drvdata->refcnt == 0) {
		ret = pm_runtime_get_sync(drvdata->dev);
		if (ret < 0)
			goto err1;
		ret = coresight_enable_reg_clk(drvdata->csdev);
		if (ret)
			goto err2;
	}

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err3;

	__cti_map_trigout(drvdata, trig, ch);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	mutex_unlock(&drvdata->mutex);
	return 0;
err3:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	if (drvdata->refcnt == 0)
		coresight_disable_reg_clk(drvdata->csdev);
err2:
	/*
	 * We come here before refcnt is potentially incremented in
	 * __cti_map_trigout so it is safe to check it against 0.
	 */
	if (drvdata->refcnt == 0)
		pm_runtime_put(drvdata->dev);
err1:
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

	drvdata->refcnt--;

	if (drvdata->refcnt == 0)
		cti_disable(drvdata);
	return;
out:
	CTI_LOCK(drvdata);
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
	/*
	 * This is required to avoid clk_disable_unprepare call from being made
	 * when unmap is called without the corresponding map function call.
	 */
	if (!drvdata->refcnt)
		goto err;

	__cti_unmap_trigin(drvdata, trig, ch);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	/*
	 * refcnt can be used here since in all cases its value is modified only
	 * within the mutex lock region in addition to within the spinlock.
	 */
	if (drvdata->refcnt == 0) {
		pm_runtime_put(drvdata->dev);
		coresight_disable_reg_clk(drvdata->csdev);
	}

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

	drvdata->refcnt--;

	if (drvdata->refcnt == 0)
		cti_disable(drvdata);
	return;
out:
	CTI_LOCK(drvdata);
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
	/*
	 * This is required to avoid clk_disable_unprepare call from being made
	 * when unmap is called without the corresponding map function call.
	 */
	if (!drvdata->refcnt)
		goto err;

	__cti_unmap_trigout(drvdata, trig, ch);
	spin_unlock_irqrestore(&drvdata->spinlock, flag);

	/*
	 * refcnt can be used here since in all cases its value is modified only
	 * within the mutex lock region in addition to within the spinlock.
	 */
	if (drvdata->refcnt == 0) {
		pm_runtime_put(drvdata->dev);
		coresight_disable_reg_clk(drvdata->csdev);
	}

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
	int refcnt;

	if (IS_ERR_OR_NULL(cti))
		return;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);

	refcnt = drvdata->refcnt;
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

	if (refcnt) {
		pm_runtime_put(drvdata->dev);
		coresight_disable_reg_clk(drvdata->csdev);
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

	spin_lock_irqsave(&drvdata->spinlock, flag);
	ret = cti_cpu_verify_access(drvdata);
	if (ret)
		goto err;

	ret = __cti_ack_trig(drvdata, trig);
err:
	spin_unlock_irqrestore(&drvdata->spinlock, flag);
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
		if (!strcmp(cti->name, name)) {
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

static ssize_t show_trigin_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long trig, ch, flag;
	uint32_t ctien;
	ssize_t size = 0;

	mutex_lock(&drvdata->mutex);
	/*
	 * refcnt can be used here since in all cases its value is modified only
	 * within the mutex lock region in addition to within the spinlock.
	 */
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
	mutex_unlock(&drvdata->mutex);
	return size;
}
static DEVICE_ATTR_RO(show_trigin);

static ssize_t show_trigout_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long trig, ch, flag;
	uint32_t ctien;
	ssize_t size = 0;

	mutex_lock(&drvdata->mutex);
	/*
	 * refcnt can be used here since in all cases its value is modified only
	 * within the mutex lock region in addition to within the spinlock.
	 */
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
	mutex_unlock(&drvdata->mutex);
	return size;
}
static DEVICE_ATTR_RO(show_trigout);

static ssize_t map_trigin_store(struct device *dev,
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
static DEVICE_ATTR_WO(map_trigin);

static ssize_t map_trigout_store(struct device *dev,
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
static DEVICE_ATTR_WO(map_trigout);

static ssize_t unmap_trigin_store(struct device *dev,
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
static DEVICE_ATTR_WO(unmap_trigin);

static ssize_t unmap_trigout_store(struct device *dev,
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
static DEVICE_ATTR_WO(unmap_trigout);

static ssize_t reset_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	if (!val)
		return -EINVAL;

	coresight_cti_reset(&drvdata->cti);
	return size;
}
static DEVICE_ATTR_WO(reset);

static ssize_t show_trig_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long ch, flag;
	uint32_t ctiset;
	ssize_t size = 0;

	mutex_lock(&drvdata->mutex);
	/*
	 * refcnt can be used here since in all cases its value is modified only
	 * within the mutex lock region in addition to within the spinlock.
	 */
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
	mutex_unlock(&drvdata->mutex);
	return size;
}
static DEVICE_ATTR_RO(show_trig);

static ssize_t set_trig_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	ret = coresight_cti_set_trig(&drvdata->cti, val);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR_WO(set_trig);

static ssize_t clear_trig_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	coresight_cti_clear_trig(&drvdata->cti, val);

	return size;
}
static DEVICE_ATTR_WO(clear_trig);

static ssize_t pulse_trig_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	ret = coresight_cti_pulse_trig(&drvdata->cti, val);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR_WO(pulse_trig);

static ssize_t ack_trig_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	ret = coresight_cti_ack_trig(&drvdata->cti, val);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR_WO(ack_trig);

static ssize_t show_gate_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long ch, flag;
	uint32_t ctigate;
	ssize_t size = 0;

	mutex_lock(&drvdata->mutex);
	/*
	 * refcnt can be used here since in all cases its value is modified only
	 * within the mutex lock region in addition to within the spinlock.
	 */
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
	mutex_unlock(&drvdata->mutex);
	return size;
}
static DEVICE_ATTR_RO(show_gate);

static ssize_t enable_gate_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	ret = coresight_cti_enable_gate(&drvdata->cti, val);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR_WO(enable_gate);

static ssize_t disable_gate_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	coresight_cti_disable_gate(&drvdata->cti, val);

	return size;
}
static DEVICE_ATTR_WO(disable_gate);

static ssize_t show_info_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t size = 0;
	unsigned int ctidevid, trig_num_max, ch_num_max;

	pm_runtime_get_sync(drvdata->dev);

	ctidevid = cti_readl(drvdata, DEVID);
	trig_num_max = (ctidevid & GENMASK(15, 8)) >> 8;
	ch_num_max = (ctidevid & GENMASK(21, 16)) >> 16;

	pm_runtime_put(drvdata->dev);

	size = scnprintf(&buf[size], PAGE_SIZE, "%d %d\n",
			trig_num_max, ch_num_max);

	return size;
}
static DEVICE_ATTR_RO(show_info);

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
	&dev_attr_show_info.attr,
	NULL,
};

static struct attribute_group cti_attr_grp = {
	.attrs = cti_attrs,
};

static const struct attribute_group *cti_attr_grps[] = {
	&cti_attr_grp,
	NULL,
};

static int cti_cpu_pm_callback(struct notifier_block *self,
			       unsigned long cmd, void *v)
{
	unsigned long aff_level = (unsigned long) v;

	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		if (aff_level == AFFINITY_LEVEL_L2)
			coresight_cti_ctx_save();
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		if (aff_level == AFFINITY_LEVEL_L2)
			coresight_cti_ctx_restore();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cti_cpu_pm_notifier = {
	.notifier_call = cti_cpu_pm_callback,
};

static int cti_parse_gpio(struct cti_drvdata *drvdata, struct amba_device *adev)
{
	int ret;
	int trig;

	drvdata->gpio_trigin = devm_kzalloc(&adev->dev,
			sizeof(struct cti_pctrl), GFP_KERNEL);
	if (!drvdata->gpio_trigin)
		return -ENOMEM;

	drvdata->gpio_trigin->trig = -1;
	ret = of_property_read_u32(adev->dev.of_node,
			"qcom,cti-gpio-trigin", &trig);
	if (!ret)
		drvdata->gpio_trigin->trig = trig;
	else if (ret != -EINVAL)
		return ret;

	drvdata->gpio_trigout = devm_kzalloc(&adev->dev,
			sizeof(struct cti_pctrl), GFP_KERNEL);
	if (!drvdata->gpio_trigout)
		return -ENOMEM;

	drvdata->gpio_trigout->trig = -1;
	ret = of_property_read_u32(adev->dev.of_node,
			"qcom,cti-gpio-trigout", &trig);
	if (!ret)
		drvdata->gpio_trigout->trig = trig;
	else if (ret != -EINVAL)
		return ret;

	return 0;
}

static int cti_init_save(struct cti_drvdata *drvdata,
		struct amba_device *adev, bool cti_save_disable)
{
	int ret;

	if (!cti_save_disable)
		drvdata->cti_save = of_property_read_bool(adev->dev.of_node,
							"qcom,cti-save");
	if (drvdata->cti_save) {
		drvdata->state = devm_kzalloc(&adev->dev,
					sizeof(struct cti_state), GFP_KERNEL);
		if (!drvdata->state)
			return -ENOMEM;

		drvdata->cti_hwclk = of_property_read_bool(adev->dev.of_node,
							"qcom,cti-hwclk");
	}
	if (drvdata->cti_save && !drvdata->cti_hwclk) {
		ret = pm_runtime_get_sync(drvdata->dev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

struct coresight_cti_data *of_get_coresight_cti_data(
				struct device *dev, struct device_node *node)
{
	int i, ret;
	uint32_t ctis_len;
	struct device_node *child_node;
	struct coresight_cti_data *ctidata;

	ctidata = devm_kzalloc(dev, sizeof(*ctidata), GFP_KERNEL);
	if (!ctidata)
		return ERR_PTR(-ENOMEM);

	if (of_get_property(node, "coresight-ctis", &ctis_len))
		ctidata->nr_ctis = ctis_len/sizeof(uint32_t);
	else
		return ERR_PTR(-EINVAL);

	if (ctidata->nr_ctis) {
		ctidata->names = devm_kzalloc(dev, ctidata->nr_ctis *
					      sizeof(*ctidata->names),
					      GFP_KERNEL);
		if (!ctidata->names)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < ctidata->nr_ctis; i++) {
			child_node = of_parse_phandle(node, "coresight-ctis",
						      i);
			if (!child_node)
				return ERR_PTR(-EINVAL);

			ret = of_property_read_string(child_node,
						      "coresight-name",
						      &ctidata->names[i]);
			of_node_put(child_node);
			if (ret)
				return ERR_PTR(ret);
		}
	}
	return ctidata;
}
EXPORT_SYMBOL(of_get_coresight_cti_data);

static int cti_probe(struct amba_device *adev, const struct amba_id *id)
{
	int ret;
	struct device *dev = &adev->dev;
	struct coresight_platform_data *pdata;
	struct cti_drvdata *drvdata;
	struct coresight_desc desc = { 0 };
	struct device_node *cpu_node;

	if (coresight_fuse_access_disabled())
		return -EPERM;

	desc.name = coresight_alloc_device_name(&cti_devs, dev);
	if (!desc.name)
		return -ENOMEM;
	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	adev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	/* Store the driver data pointer for use in exported functions */
	drvdata->dev = &adev->dev;
	dev_set_drvdata(dev, drvdata);

	drvdata->base = devm_ioremap_resource(dev, &adev->res);
	if (!drvdata->base)
		return -ENOMEM;
	cpu_node = of_parse_phandle(adev->dev.of_node, "cpu", 0);
	if (cpu_node) {
		drvdata->cpu = coresight_get_cpu(dev);
		if (drvdata->cpu < 0)
			return drvdata->cpu;
	}
	of_node_put(cpu_node);
	spin_lock_init(&drvdata->spinlock);

	mutex_init(&drvdata->mutex);

	ret = cti_parse_gpio(drvdata, adev);
	if (ret)
		return ret;

	ret = cti_init_save(drvdata, adev, cti_save_disable);
	if (ret)
		return ret;

	mutex_lock(&cti_lock);
	drvdata->cti.name = desc.name;
	list_add_tail(&drvdata->cti.link, &cti_list);
	mutex_unlock(&cti_lock);

	desc.type = CORESIGHT_DEV_TYPE_NONE;
	desc.pdata = adev->dev.platform_data;
	desc.dev = &adev->dev;
	desc.groups = cti_attr_grps;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto err;
	}

	if (drvdata->cti_save) {
		if (!registered)
			cpu_pm_register_notifier(&cti_cpu_pm_notifier);
		registered++;
	}
	pm_runtime_put(&adev->dev);
	dev_dbg(dev, "CTI initialized\n");
	return 0;
err:
	if (drvdata->cti_save && !drvdata->cti_hwclk)
		pm_runtime_put(&adev->dev);
	return ret;
}

static struct amba_id cti_ids[] = {
	{
		.id     = 0x0003b966,
		.mask   = 0x0003ffff,
		.data	= "CTI",
	},
	{ 0, 0},
};

static struct amba_driver cti_driver = {
	.drv = {
		.name   = "coresight-cti",
		.owner	= THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe          = cti_probe,
	.id_table	= cti_ids,
};

builtin_amba_driver(cti_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight CTI driver");

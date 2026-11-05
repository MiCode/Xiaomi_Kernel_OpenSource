// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2011-2012, 2017, The Linux Foundation. All rights reserved.
 *
 * Description: CoreSight Funnel driver
 */

#include <linux/acpi.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/coresight.h>
#include <linux/amba/bus.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/cpu_pm.h>
#include <linux/pm_domain.h>

#include "coresight-priv.h"

#define FUNNEL_FUNCTL		0x000
#define FUNNEL_PRICTL		0x004

#define FUNNEL_HOLDTIME_MASK	0xf00
#define FUNNEL_HOLDTIME_SHFT	0x8
#define FUNNEL_HOLDTIME		(0x7 << FUNNEL_HOLDTIME_SHFT)
#define FUNNEL_ENSx_MASK	0xff

DEFINE_CORESIGHT_DEVLIST(funnel_devs, "funnel");
static enum cpuhp_state hp_online;
static LIST_HEAD(cpu_pm_list);
static DEFINE_SPINLOCK(delay_lock);
/**
 * struct funnel_drvdata - specifics associated to a funnel component
 * @base:	memory mapped base address for this component.
 * @atclk:	optional clock for the core parts of the funnel.
 * @csdev:	component vitals needed by the framework.
 * @priority:	port selection order.
 * @spinlock:	serialize enable/disable operations.
 */
struct funnel_drvdata {
	void __iomem		*base;
	struct clk		*atclk;
	struct coresight_device	*csdev;
	unsigned long		priority;
	spinlock_t		spinlock;
	struct pm_config	pm_config;
	struct list_head	link;
};

static int dynamic_funnel_enable_hw(struct funnel_drvdata *drvdata, int port)
{
	u32 functl;
	int rc = 0;
	struct coresight_device *csdev = drvdata->csdev;

	CS_UNLOCK(drvdata->base);

	functl = readl_relaxed(drvdata->base + FUNNEL_FUNCTL);
	/* Claim the device only when we enable the first slave */
	if (!(functl & FUNNEL_ENSx_MASK)) {
		rc = coresight_claim_device_unlocked(csdev);
		if (rc)
			goto done;
	}

	functl &= ~FUNNEL_HOLDTIME_MASK;
	functl |= FUNNEL_HOLDTIME;
	functl |= (1 << port);
	writel_relaxed(functl, drvdata->base + FUNNEL_FUNCTL);
	writel_relaxed(drvdata->priority, drvdata->base + FUNNEL_PRICTL);
done:
	CS_LOCK(drvdata->base);
	return rc;
}

static int funnel_enable(struct coresight_device *csdev,
			 struct coresight_connection *in,
			 struct coresight_connection *out)
{
	int rc = 0;
	struct funnel_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;
	bool first_enable = false;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	if (!drvdata->pm_config.hw_powered) {
		rc = -EINVAL;
		goto out;
	}

	if (atomic_read(&in->dest_refcnt) == 0) {
		if (drvdata->base)
			rc = dynamic_funnel_enable_hw(drvdata, in->dest_port);
		if (!rc)
			first_enable = true;
	}
	if (!rc)
		atomic_inc(&in->dest_refcnt);
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (first_enable)
		dev_dbg(&csdev->dev, "FUNNEL inport %d enabled\n",
			in->dest_port);
	return rc;
}

static void dynamic_funnel_disable_hw(struct funnel_drvdata *drvdata,
				      int inport)
{
	u32 functl;
	struct coresight_device *csdev = drvdata->csdev;

	CS_UNLOCK(drvdata->base);

	functl = readl_relaxed(drvdata->base + FUNNEL_FUNCTL);
	functl &= ~(1 << inport);
	writel_relaxed(functl, drvdata->base + FUNNEL_FUNCTL);

	/* Disclaim the device if none of the slaves are now active */
	if (!(functl & FUNNEL_ENSx_MASK))
		coresight_disclaim_device_unlocked(csdev);

	CS_LOCK(drvdata->base);
}

static void funnel_disable(struct coresight_device *csdev,
			   struct coresight_connection *in,
			   struct coresight_connection *out)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;
	bool last_disable = false;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	if (!drvdata->pm_config.hw_powered)
		goto out;

	if (atomic_dec_return(&in->dest_refcnt) == 0) {
		if (drvdata->base)
			dynamic_funnel_disable_hw(drvdata, in->dest_port);
		last_disable = true;
	}

out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (last_disable)
		dev_dbg(&csdev->dev, "FUNNEL inport %d disabled\n",
			in->dest_port);
}

static const struct coresight_ops_link funnel_link_ops = {
	.enable		= funnel_enable,
	.disable	= funnel_disable,
};

static const struct coresight_ops funnel_cs_ops = {
	.link_ops	= &funnel_link_ops,
};

static ssize_t priority_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->priority;

	return sprintf(buf, "%#lx\n", val);
}

static ssize_t priority_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev->parent);

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	drvdata->priority = val;
	return size;
}
static DEVICE_ATTR_RW(priority);

static u32 get_funnel_ctrl_hw(struct funnel_drvdata *drvdata)
{
	u32 functl;

	CS_UNLOCK(drvdata->base);
	functl = readl_relaxed(drvdata->base + FUNNEL_FUNCTL);
	CS_LOCK(drvdata->base);

	return functl;
}

static ssize_t funnel_ctrl_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	u32 val;
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev->parent);
	int ret;
	unsigned long flags;

	ret = pm_runtime_resume_and_get(dev->parent);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	if (!drvdata->pm_config.hw_powered) {
		ret = -EINVAL;
		goto out;
	}
	val = get_funnel_ctrl_hw(drvdata);
out:
	spin_unlock_irqrestore(&drvdata->spinlock, flags);
	pm_runtime_put_sync(dev->parent);

	if (ret)
		return ret;
	else
		return scnprintf(buf, PAGE_SIZE, "%#x\n", val);
}
static DEVICE_ATTR_RO(funnel_ctrl);

static struct attribute *coresight_funnel_attrs[] = {
	&dev_attr_funnel_ctrl.attr,
	&dev_attr_priority.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_funnel);

static int funnel_get_resource_byname(struct device_node *np,
				   char *ch_base, struct resource *res)
{
	const char *name = NULL;
	int index = 0, found = 0;

	while (!of_property_read_string_index(np, "reg-names", index, &name)) {
		if (strcmp(ch_base, name)) {
			index++;
			continue;
		}

		/* We have a match and @index is where it's at */
		found = 1;
		break;
	}

	if (!found)
		return -EINVAL;

	return of_address_to_resource(np, index, res);
}

static void funnel_init_power_state(struct device *dev, struct funnel_drvdata *drvdata)
{
	int cpu;
	struct cpumask *cpumask;
	struct pm_config *pm_config = &drvdata->pm_config;
	struct generic_pm_domain *pd;

	if (dev->pm_domain) {
		pd = pd_to_genpd(dev->pm_domain);
		cpumask = pd->cpus;

		if (cpumask_empty(cpumask)) {
			pm_config->hw_powered = true;
			return;
		}

		cpus_read_lock();
		for_each_online_cpu(cpu) {
			if (cpumask_test_cpu(cpu, cpumask)) {
				pm_config->hw_powered = true;
				break;
			}
		}

		pm_config->pd_cpumask = cpumask;
		cpumask_and(&pm_config->powered_cpus, cpumask, cpu_online_mask);
		cpumask_copy(&pm_config->online_cpus, &pm_config->powered_cpus);
		spin_lock(&delay_lock);
		list_add(&drvdata->link, &cpu_pm_list);
		spin_unlock(&delay_lock);
		cpus_read_unlock();
		return;
	}

	pm_config->hw_powered = true;
}

static int funnel_probe(struct device *dev, struct resource *res)
{
	int ret;
	void __iomem *base;
	struct coresight_platform_data *pdata = NULL;
	struct funnel_drvdata *drvdata;
	struct resource res_real;
	struct coresight_desc desc = { 0 };
	struct device_node *np = dev->of_node;

	if (is_of_node(dev_fwnode(dev)) &&
	    of_device_is_compatible(dev->of_node, "arm,coresight-funnel"))
		dev_warn_once(dev, "Uses OBSOLETE CoreSight funnel binding\n");

	desc.name = coresight_alloc_device_name(&funnel_devs, dev);
	if (!desc.name)
		return -ENOMEM;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->atclk = devm_clk_get(dev, "atclk"); /* optional */
	if (IS_ERR(drvdata->atclk) &&
			of_property_read_bool(dev->of_node, "qcom,atclk-dependence")) {
		dev_err(dev, "get atclk fail %ld\n",  PTR_ERR(drvdata->atclk));
		return  PTR_ERR(drvdata->atclk);
	}

	if (!IS_ERR(drvdata->atclk)) {
		ret = clk_prepare_enable(drvdata->atclk);
		if (ret)
			return ret == -ETIMEDOUT ? -EPROBE_DEFER : ret;
	}

	if (of_property_read_bool(np, "qcom,duplicate-funnel")) {
		ret = funnel_get_resource_byname(np, "funnel-base-real",
						 &res_real);
		if (ret)
			goto out_disable_clk;

		res = &res_real;
		base = devm_ioremap(dev, res->start, resource_size(res));
		if (IS_ERR(base)) {
			ret = PTR_ERR(base);
			goto out_disable_clk;
		}
		drvdata->base = base;
		desc.groups = coresight_funnel_groups;
	} else if (res) {
		/*
		 * Map the device base for dynamic-funnel, which has been
		 * validated by AMBA core.
		 */
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base)) {
			ret = PTR_ERR(base);
			goto out_disable_clk;
		}
		drvdata->base = base;
		desc.groups = coresight_funnel_groups;
		desc.access = CSDEV_ACCESS_IOMEM(base);
	}

	dev_set_drvdata(dev, drvdata);

	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata)) {
		ret = PTR_ERR(pdata);
		goto out_disable_clk;
	}
	dev->platform_data = pdata;

	spin_lock_init(&drvdata->spinlock);
	desc.type = CORESIGHT_DEV_TYPE_LINK;
	desc.subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_MERG;
	desc.ops = &funnel_cs_ops;
	desc.pdata = pdata;
	desc.dev = dev;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto out_disable_clk;
	}
	funnel_init_power_state(dev, drvdata);
	pm_runtime_put_sync(dev);
	ret = 0;

out_disable_clk:
	if (ret && !IS_ERR_OR_NULL(drvdata->atclk))
		clk_disable_unprepare(drvdata->atclk);
	return ret;
}

static int funnel_remove(struct device *dev)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev);

	spin_lock(&delay_lock);
	if (drvdata->pm_config.pm_enable)
		list_del(&drvdata->link);
	spin_unlock(&delay_lock);

	coresight_unregister(drvdata->csdev);

	return 0;
}

#ifdef CONFIG_PM
static int funnel_runtime_suspend(struct device *dev)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_disable_unprepare(drvdata->atclk);

	return 0;
}

static int funnel_runtime_resume(struct device *dev)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_prepare_enable(drvdata->atclk);

	return 0;
}
#endif

static const struct dev_pm_ops funnel_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(funnel_runtime_suspend, funnel_runtime_resume, NULL)
};

static int static_funnel_probe(struct platform_device *pdev)
{
	int ret;

	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	/* Static funnel do not have programming base */
	ret = funnel_probe(&pdev->dev, NULL);

	if (ret) {
		pm_runtime_put_noidle(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
	}

	return ret;
}

static int static_funnel_remove(struct platform_device *pdev)
{
	funnel_remove(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id static_funnel_match[] = {
	{.compatible = "arm,coresight-static-funnel"},
	{}
};

MODULE_DEVICE_TABLE(of, static_funnel_match);

#ifdef CONFIG_ACPI
static const struct acpi_device_id static_funnel_ids[] = {
	{"ARMHC9FE", 0},
	{},
};

MODULE_DEVICE_TABLE(acpi, static_funnel_ids);
#endif

static struct platform_driver static_funnel_driver = {
	.probe          = static_funnel_probe,
	.remove          = static_funnel_remove,
	.driver         = {
		.name   = "coresight-static-funnel",
		/* THIS_MODULE is taken care of by platform_driver_register() */
		.of_match_table = static_funnel_match,
		.acpi_match_table = ACPI_PTR(static_funnel_ids),
		.pm	= &funnel_dev_pm_ops,
		.suppress_bind_attrs = true,
	},
};

static int dynamic_funnel_probe(struct amba_device *adev,
				const struct amba_id *id)
{
	return funnel_probe(&adev->dev, &adev->res);
}

static void dynamic_funnel_remove(struct amba_device *adev)
{
	funnel_remove(&adev->dev);
}

static int funnel_cpu_pm_notify(struct notifier_block *nb, unsigned long cmd,
			      void *v)
{
	unsigned int cpu = smp_processor_id();
	struct funnel_drvdata *drvdata, *tmp;
	struct pm_config *pm_config;
	unsigned long flags;

	switch (cmd) {
	case CPU_PM_ENTER:
		list_for_each_entry_safe(drvdata, tmp, &cpu_pm_list, link) {
			pm_config = &drvdata->pm_config;
			if (!cpumask_test_cpu(cpu, pm_config->pd_cpumask))
				continue;
			spin_lock_irqsave(&drvdata->spinlock, flags);
			if (!cpumask_test_cpu(cpu, &pm_config->online_cpus)) {
				spin_unlock_irqrestore(&drvdata->spinlock, flags);
				continue;
			}
			cpumask_clear_cpu(cpu, &pm_config->powered_cpus);
			if (cpumask_empty(&pm_config->powered_cpus))
				pm_config->hw_powered = false;
			spin_unlock_irqrestore(&drvdata->spinlock, flags);
		}
		break;
	case CPU_PM_EXIT:
	case CPU_PM_ENTER_FAILED:
		list_for_each_entry_safe(drvdata, tmp, &cpu_pm_list, link) {
			pm_config = &drvdata->pm_config;
			if (!cpumask_test_cpu(cpu, pm_config->pd_cpumask))
				continue;
			spin_lock_irqsave(&drvdata->spinlock, flags);
			if (!cpumask_test_cpu(cpu, &pm_config->online_cpus)) {
				spin_unlock_irqrestore(&drvdata->spinlock, flags);
				continue;
			}
			pm_config->hw_powered = true;
			cpumask_set_cpu(cpu, &pm_config->powered_cpus);
			spin_unlock_irqrestore(&drvdata->spinlock, flags);
		}
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block funnel_cpu_pm_nb = {
	.notifier_call = funnel_cpu_pm_notify,
};

static int funnel_offline_cpu(unsigned int cpu)
{
	struct funnel_drvdata *drvdata, *tmp;
	struct pm_config *pm_config;
	unsigned long flags;

	list_for_each_entry_safe(drvdata, tmp, &cpu_pm_list, link) {
		pm_config = &drvdata->pm_config;
		if (!cpumask_test_cpu(cpu, pm_config->pd_cpumask))
			continue;

		spin_lock_irqsave(&drvdata->spinlock, flags);
		cpumask_clear_cpu(cpu, &pm_config->online_cpus);
		cpumask_clear_cpu(cpu, &pm_config->powered_cpus);
		if (cpumask_empty(&pm_config->powered_cpus))
			pm_config->hw_powered = false;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
	}
	return 0;
}

static int funnel_online_cpu(unsigned int cpu)
{
	struct funnel_drvdata *drvdata, *tmp;
	struct pm_config *pm_config;
	unsigned long flags;

	list_for_each_entry_safe(drvdata, tmp, &cpu_pm_list, link) {
		pm_config = &drvdata->pm_config;
		if (!cpumask_test_cpu(cpu, pm_config->pd_cpumask))
			continue;
		spin_lock_irqsave(&drvdata->spinlock, flags);
		cpumask_set_cpu(cpu, &pm_config->powered_cpus);
		cpumask_set_cpu(cpu, &pm_config->online_cpus);
		pm_config->hw_powered = true;
		spin_unlock_irqrestore(&drvdata->spinlock, flags);
	}

	return 0;
}


static int __init funnel_pm_setup(void)
{
	int ret;

	ret = cpu_pm_register_notifier(&funnel_cpu_pm_nb);
	if (ret)
		return ret;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
				"arm/coresight-funnel:online",
				funnel_online_cpu, funnel_offline_cpu);

	if (ret > 0) {
		hp_online = ret;
		return 0;
	}

	cpu_pm_unregister_notifier(&funnel_cpu_pm_nb);
	return ret;
}

static void funnel_pm_clear(void)
{
	cpu_pm_unregister_notifier(&funnel_cpu_pm_nb);
	if (hp_online) {
		cpuhp_remove_state_nocalls(hp_online);
		hp_online = 0;
	}
}

static const struct amba_id dynamic_funnel_ids[] = {
	{
		.id     = 0x000bb908,
		.mask   = 0x000fffff,
	},
	{
		/* Coresight SoC-600 */
		.id     = 0x000bb9eb,
		.mask   = 0x000fffff,
	},
	{ 0, 0},
};

MODULE_DEVICE_TABLE(amba, dynamic_funnel_ids);

static struct amba_driver dynamic_funnel_driver = {
	.drv = {
		.name	= "coresight-dynamic-funnel",
		.owner	= THIS_MODULE,
		.pm	= &funnel_dev_pm_ops,
		.suppress_bind_attrs = true,
	},
	.probe		= dynamic_funnel_probe,
	.remove		= dynamic_funnel_remove,
	.id_table	= dynamic_funnel_ids,
};

static int __init funnel_init(void)
{
	int ret;

	ret = funnel_pm_setup();
	if (ret)
		return ret;

	ret = platform_driver_register(&static_funnel_driver);
	if (ret) {
		pr_info("Error registering platform driver\n");
		goto pm_clear;
	}

	ret = amba_driver_register(&dynamic_funnel_driver);
	if (ret) {
		pr_info("Error registering amba driver\n");
		platform_driver_unregister(&static_funnel_driver);
		goto pm_clear;
	}
	return ret;
pm_clear:
	funnel_pm_clear();
	return ret;
}

static void __exit funnel_exit(void)
{
	platform_driver_unregister(&static_funnel_driver);
	amba_driver_unregister(&dynamic_funnel_driver);
	funnel_pm_clear();
}

module_init(funnel_init);
module_exit(funnel_exit);

MODULE_AUTHOR("Pratik Patel <pratikp@codeaurora.org>");
MODULE_AUTHOR("Mathieu Poirier <mathieu.poirier@linaro.org>");
MODULE_DESCRIPTION("Arm CoreSight Funnel Driver");
MODULE_LICENSE("GPL v2");

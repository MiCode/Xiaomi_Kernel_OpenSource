/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include "proc_comm.h"
#include "proccomm-regulator.h"

#define MV_TO_UV(mv) ((mv)*1000)
#define UV_TO_MV(uv) (((uv)+999)/1000)

/*
 * Wrappers for the msm_proc_comm() calls.
 * Does basic impedance matching between what the proccomm interface
 * expects and how the driver sees the world.
 */

/* Converts a proccomm error to an errno value. */
static int _pcom_err_to_linux_errno(unsigned error)
{
	if (!error)		/* 0 == no error */
		return 0;
	else if (error & 0x1F)  /* bits 0..4 => parameter 1..5 out of range */
		return -EDOM;
	else if (error & 0x100) /* bit 8 => feature not supported */
		return -ENOSYS;
	else			/* anything else non-zero: unknown error */
		return -EINVAL;
}

/* vreg_switch: (vreg ID, on/off) => (return code, <null>) */
static int _vreg_switch(int vreg_id, bool enable)
{
	unsigned _id		= (unsigned)vreg_id;
	unsigned _enable	= !!enable;

	return msm_proc_comm(PCOM_VREG_SWITCH, &_id, &_enable);
}

/* vreg_set_level: (vreg ID, mV) => (return code, <null>) */
static int _vreg_set_level(int vreg_id, int level_mV)
{
	unsigned _id		= (unsigned)vreg_id;
	unsigned _level		= (unsigned)level_mV;
	int	 rc;

	rc = msm_proc_comm(PCOM_VREG_SET_LEVEL, &_id, &_level);

	if (rc)
		return rc;

	return _pcom_err_to_linux_errno(_id);
}

/* vreg_pull_down: (pull down, vreg ID) => (<null>, <null>) */
/* Returns error code from msm_proc_comm. */
static int _vreg_pull_down(int vreg_id, bool pull_down)
{
	unsigned _id		= (unsigned)vreg_id;
	unsigned _enable	= !!pull_down;

	return msm_proc_comm(PCOM_VREG_PULLDOWN, &_enable, &_id);
}

struct proccomm_regulator_drvdata {
	struct regulator_desc	rdesc;
	int			rise_time;
	int			last_voltage;
	bool			enabled;
	bool			negative;
};

static int proccomm_vreg_enable(struct regulator_dev *rdev)
{
	struct proccomm_regulator_drvdata *ddata;
	int rc;

	ddata = rdev_get_drvdata(rdev);
	rc = _vreg_switch(rdev_get_id(rdev), VREG_SWITCH_ENABLE);

	if (rc) {
		dev_err(rdev_get_dev(rdev),
			"could not enable regulator %d (%s): %d\n",
			rdev_get_id(rdev), ddata->rdesc.name, rc);
	} else {
		dev_dbg(rdev_get_dev(rdev),
			"enabled regulator %d (%s)\n",
			rdev_get_id(rdev), ddata->rdesc.name);
		ddata->enabled = 1;
	}

	return rc;
}

static int proccomm_vreg_disable(struct regulator_dev *rdev)
{
	struct proccomm_regulator_drvdata *ddata;
	int rc;

	ddata = rdev_get_drvdata(rdev);
	rc = _vreg_switch(rdev_get_id(rdev), VREG_SWITCH_DISABLE);

	if (rc) {
		dev_err(rdev_get_dev(rdev),
			"could not disable regulator %d (%s): %d\n",
			rdev_get_id(rdev), ddata->rdesc.name, rc);
	} else {
		dev_dbg(rdev_get_dev(rdev),
			"disabled regulator %d (%s)\n",
			rdev_get_id(rdev), ddata->rdesc.name);
		ddata->enabled = 0;
	}

	return rc;
}

static int proccomm_vreg_is_enabled(struct regulator_dev *rdev)
{
	struct proccomm_regulator_drvdata *ddata = rdev_get_drvdata(rdev);

	return ddata->enabled;
}

static int proccomm_vreg_rise_time(struct regulator_dev *rdev)
{
	struct proccomm_regulator_drvdata *ddata = rdev_get_drvdata(rdev);

	return ddata->rise_time;
}

static int proccomm_vreg_get_voltage(struct regulator_dev *rdev)
{

	struct proccomm_regulator_drvdata *ddata = rdev_get_drvdata(rdev);

	return MV_TO_UV(ddata->last_voltage);
}

static int proccomm_vreg_set_voltage(struct regulator_dev *rdev,
					int min_uV, int max_uV, unsigned *sel)
{
	struct proccomm_regulator_drvdata *ddata = rdev_get_drvdata(rdev);
	int level_mV = UV_TO_MV(min_uV);
	int rc;

	rc = _vreg_set_level(rdev_get_id(rdev),
			ddata->negative ? -level_mV : level_mV);

	if (rc) {
		dev_err(rdev_get_dev(rdev),
			"could not set voltage for regulator %d (%s) "
			"to %d mV: %d\n",
			rdev_get_id(rdev), ddata->rdesc.name, level_mV, rc);
	} else {
		dev_dbg(rdev_get_dev(rdev),
			"voltage for regulator %d (%s) set to %d mV\n",
			rdev_get_id(rdev), ddata->rdesc.name, level_mV);
		ddata->last_voltage = level_mV;
	}

	return rc;
}

static struct regulator_ops proccomm_regulator_ops = {
	.enable		= proccomm_vreg_enable,
	.disable	= proccomm_vreg_disable,
	.is_enabled	= proccomm_vreg_is_enabled,
	.get_voltage	= proccomm_vreg_get_voltage,
	.set_voltage	= proccomm_vreg_set_voltage,
	.enable_time	= proccomm_vreg_rise_time,
};

/*
 * Create and register a struct regulator_dev based on the information in
 * a struct proccomm_regulator_info.
 * Fills in the rdev field in struct proccomm_regulator_info.
 */
static struct regulator_dev *__devinit create_proccomm_rdev(
	struct proccomm_regulator_info *info, struct device *parent)
{
	char *name;
	struct proccomm_regulator_drvdata *d;
	struct regulator_dev *rdev;
	int rc = 0;

	if (info->id < 0) {
		dev_err(parent, "invalid regulator id %d\n", info->id);
		rc = -EINVAL;
		goto out;
	}

	name = info->init_data.constraints.name;

	if (!name) {
		dev_err(parent,
			"could not register regulator with id %d: "
			"no name specified\n", info->id);
		rc = -EINVAL;
		goto out;
	}

	if (info->pulldown > 0) {
		rc = _vreg_pull_down(info->id, info->pulldown);
		if (rc) {
			dev_err(parent,
				"probing for regulator %d (%s) failed\n",
				info->id, name);
			goto out;
		}
	}

	d = kzalloc(sizeof(*d), GFP_KERNEL);

	if (!d) {
		dev_err(parent,
			"could not allocate struct proccomm_regulator_drvdata "
			"for regulator %d (%s)\n", info->id, name);
		rc = -ENOMEM;
		goto out;
	}

	d->rdesc.name	= name;
	d->rdesc.id	= info->id;
	d->rdesc.ops	= &proccomm_regulator_ops;
	d->rdesc.type	= REGULATOR_VOLTAGE;
	d->rdesc.owner	= THIS_MODULE;
	d->rise_time	= info->rise_time;
	d->enabled	= 0;
	d->negative	= info->negative;

	rdev = regulator_register(&d->rdesc, parent, &info->init_data, d);

	if (IS_ERR(rdev)) {
		rc = PTR_ERR(rdev);
		dev_err(parent, "error registering regulator %d (%s): %d\n",
				info->id, name, rc);
		goto clean;
	}

	dev_dbg(parent, "registered regulator %d (%s)\n", info->id, name);

	return rdev;

clean:
	kfree(d);
out:
	return ERR_PTR(rc);
}

/*
 * Unregister and destroy a struct regulator_dev created by
 * create_proccomm_rdev.
 */
static void destroy_proccomm_rdev(struct regulator_dev *rdev)
{
	struct proccomm_regulator_drvdata *d;

	if (!rdev)
		return;

	d = rdev_get_drvdata(rdev);

	regulator_unregister(rdev);

	dev_dbg(rdev_get_dev(rdev)->parent,
		"unregistered regulator %d (%s)\n",
		d->rdesc.id, d->rdesc.name);

	kfree(d);
}


static int __devinit proccomm_vreg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct proccomm_regulator_platform_data *pdata = dev->platform_data;
	struct regulator_dev **rdevs;
	int rc = 0;
	size_t i = 0;

	if (!pdata) {
		dev_err(dev, "invalid platform data\n");
		rc = -EINVAL;
		goto check_fail;
	}

	if (pdata->nregs == 0) {
		dev_err(dev, "registering an empty regulator list; "
				"this is probably not what you want\n");
		rc = -EINVAL;
		goto check_fail;
	}

	rdevs = kcalloc(pdata->nregs, sizeof(*rdevs), GFP_KERNEL);

	if (!rdevs) {
		dev_err(dev, "could not allocate storage for "
				"struct regulator_dev array\n");
		rc = -ENOMEM;
		goto check_fail;
	}

	platform_set_drvdata(pdev, rdevs);

	dev_dbg(dev, "registering %d proccomm regulators\n", pdata->nregs);

	for (i = 0; i < pdata->nregs; i++) {
		rdevs[i] = create_proccomm_rdev(&pdata->regs[i], dev);
		if (IS_ERR(rdevs[i])) {
			rc = PTR_ERR(rdevs[i]);
			goto backout;
		}
	}

	dev_dbg(dev, "%d proccomm regulators registered\n", pdata->nregs);

	return rc;

backout:
	while (--i >= 0)
		destroy_proccomm_rdev(rdevs[i]);

	kfree(rdevs);

check_fail:
	return rc;
}

static int __devexit proccomm_vreg_remove(struct platform_device *pdev)
{
	struct proccomm_regulator_platform_data *pdata;
	struct regulator_dev **rdevs;
	size_t i;

	pdata = pdev->dev.platform_data;
	rdevs = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->nregs; i++)
		destroy_proccomm_rdev(rdevs[i]);

	kfree(rdevs);

	return 0;
}

static struct platform_driver proccomm_vreg_driver = {
	.probe	= proccomm_vreg_probe,
	.remove = __devexit_p(proccomm_vreg_remove),
	.driver = {
		.name	= PROCCOMM_REGULATOR_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init proccomm_vreg_init(void)
{
	return platform_driver_register(&proccomm_vreg_driver);
}
postcore_initcall(proccomm_vreg_init);

static void __exit proccomm_vreg_exit(void)
{
	platform_driver_unregister(&proccomm_vreg_driver);
}
module_exit(proccomm_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ProcComm regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PROCCOMM_REGULATOR_DEV_NAME);

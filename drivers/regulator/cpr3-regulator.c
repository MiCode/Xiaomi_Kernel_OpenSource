/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#include "cpr3-regulator.h"

#define CPR3_REGULATOR_CORNER_INVALID	(-1)

static DEFINE_MUTEX(cpr3_controller_list_mutex);
static LIST_HEAD(cpr3_controller_list);

/**
 * cpr3_regulator_scale_vdd_voltage() - scale the CPR controlled VDD supply
 *		voltage to the new level while satisfying any other hardware
 *		requirements
 * @thread:		Pointer to the CPR3 thread
 * @new_volt:		New voltage in microvolts that VDD needs to end up at
 * @max_volt:		Maximum voltage in microvolts that VDD may be set to
 *
 * This function scales the CPR controlled VDD supply voltage from its
 * current level to the new voltage that is specified.  If the supply is
 * configured to use the APM and the APM threshold is crossed as a result of
 * the voltage scaling, then this function also stops at the APM threshold,
 * switches the APM source, and finally sets the final new voltage.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_scale_vdd_voltage(struct cpr3_thread *thread,
						int new_volt, int max_volt)
{
	struct cpr3_controller *ctrl = thread->ctrl;
	struct regulator *vdd = ctrl->vdd_regulator;
	bool apm_crossing = false;
	int apm_volt = ctrl->apm_threshold_volt;
	int last_volt = ctrl->aggr_corner.last_volt;
	int rc;

	if (ctrl->apm && apm_volt > 0
		&& ((last_volt < apm_volt && apm_volt <= new_volt)
			|| (last_volt >= apm_volt && apm_volt > new_volt)))
		apm_crossing = true;

	if (apm_crossing) {
		rc = regulator_set_voltage(vdd, apm_volt, apm_volt);
		if (rc) {
			cpr3_err(thread, "regulator_set_voltage(vdd) == %d failed, rc=%d\n",
				apm_volt, rc);
			return rc;
		}

		rc = msm_apm_set_supply(ctrl->apm, new_volt >= apm_volt
				? ctrl->apm_high_supply : ctrl->apm_low_supply);
		if (rc) {
			cpr3_err(thread, "APM switch failed, rc=%d\n", rc);
			/* Roll back the voltage. */
			regulator_set_voltage(vdd, last_volt, INT_MAX);
			return rc;
		}
	}

	rc = regulator_set_voltage(vdd, new_volt, max_volt);
	if (rc) {
		cpr3_err(thread, "regulator_set_voltage(vdd) == %d failed, rc=%d\n",
			new_volt, rc);
		return rc;
	}

	return 0;
}

/**
 * _cpr3_regulator_set_voltage() - set the voltage sufficient for the corner
 *		requested for the CPR3 thread
 * @thread:		Pointer to the CPR3 thread
 * @corner:		New corner to use for the thread
 *
 * This function aggregates the CPR parameters for all threads associated with
 * the VDD supply using the specified corner for the thread.  After that, it
 * sets the aggregated last known good voltage.
 *
 * The VDD supply voltage will not be physically configured unless this
 * condition is met by at least one of the threads of the controller:
 * thread->vreg_enabled == true &&
 * thread->current_corner != CPR3_REGULATOR_CORNER_INVALID
 *
 * Note, CPR3 controller lock must be held by the caller.
 *
 * Return: 0 on success, errno on failure
 */
static int _cpr3_regulator_set_voltage(struct cpr3_thread *thread, int corner)
{
	struct cpr3_corner aggr_corner = {};
	struct cpr3_corner *corn;
	struct cpr3_thread *thrd;
	bool valid = false;
	int i, rc;

	if (corner == CPR3_REGULATOR_CORNER_INVALID)
		return 0;

	if (thread->vreg_enabled) {
		valid = true;
		aggr_corner = thread->corner[corner];
	}

	for (i = 0; i < thread->ctrl->thread_count; i++) {
		thrd = &thread->ctrl->thread[i];
		if (thread == thrd) {
			/* Already handled before the loop. */
			continue;
		} else if (!thrd->vreg_enabled || thrd->current_corner
					== CPR3_REGULATOR_CORNER_INVALID) {
			/* Cannot participate in aggregation. */
			continue;
		} else {
			valid = true;
		}

		corn = &thrd->corner[thrd->current_corner];

		aggr_corner.ceiling_volt = max(aggr_corner.ceiling_volt,
						corn->ceiling_volt);
		aggr_corner.floor_volt = max(aggr_corner.floor_volt,
						corn->floor_volt);
		aggr_corner.last_volt = max(aggr_corner.last_volt,
						corn->last_volt);
	}

	if (!valid) {
		/* No threads are enabled with a valid corner. */
		return 0;
	}

	rc = cpr3_regulator_scale_vdd_voltage(thread, aggr_corner.last_volt,
						aggr_corner.ceiling_volt);
	if (rc) {
		cpr3_err(thread, "vdd voltage scaling failed, rc=%d\n", rc);
		return rc;
	}

	thread->current_corner = corner;
	thread->ctrl->aggr_corner = aggr_corner;

	cpr3_debug(thread, "set corner=%d\n", corner);

	return 0;
}

/**
 * cpr3_regulator_set_voltage() - set the voltage corner for the CPR3 thread
 *			associated with the regulator device
 * @rdev:		Regulator device pointer for the cpr3-regulator
 * @corner:		New voltage corner to set (offset by CPR3_CORNER_OFFSET)
 * @corner_max:		Maximum voltage corner allowed (offset by
 *			CPR3_CORNER_OFFSET)
 * @selector:		Pointer which is filled with the selector value for the
 *			corner
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.  The VDD voltage will not be
 * physically configured until both this function and cpr3_regulator_enable()
 * are called.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_set_voltage(struct regulator_dev *rdev,
		int corner, int corner_max, unsigned *selector)
{
	struct cpr3_thread *thread = rdev_get_drvdata(rdev);
	int rc = 0;

	corner -= CPR3_CORNER_OFFSET;
	corner_max -= CPR3_CORNER_OFFSET;
	*selector = corner;

	if (!thread->vreg_enabled) {
		thread->current_corner = corner;
		cpr3_debug(thread, "stored corner=%d\n", corner);
		return 0;
	} else if (thread->current_corner == corner) {
		return 0;
	}

	mutex_lock(&thread->ctrl->lock);
	rc = _cpr3_regulator_set_voltage(thread, corner);
	if (rc)
		cpr3_err(thread, "set voltage failed, rc=%d\n", rc);
	mutex_unlock(&thread->ctrl->lock);

	return rc;
}

/**
 * cpr3_regulator_get_voltage() - get the voltage corner for the CPR3 thread
 *			associated with the regulator device
 * @rdev:		Regulator device pointer for the cpr3-regulator
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: voltage corner value offset by CPR3_CORNER_OFFSET
 */
static int cpr3_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct cpr3_thread *thread = rdev_get_drvdata(rdev);

	if (thread->current_corner == CPR3_REGULATOR_CORNER_INVALID)
		return CPR3_CORNER_OFFSET;
	else
		return thread->current_corner + CPR3_CORNER_OFFSET;
}

/**
 * cpr3_regulator_list_voltage() - return the voltage corner mapped to the
 *			specified selector
 * @rdev:		Regulator device pointer for the cpr3-regulator
 * @selector:		Regulator selector
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: voltage corner value offset by CPR3_CORNER_OFFSET
 */
static int cpr3_regulator_list_voltage(struct regulator_dev *rdev,
		unsigned selector)
{
	struct cpr3_thread *thread = rdev_get_drvdata(rdev);

	if (selector < thread->corner_count)
		return selector + CPR3_CORNER_OFFSET;
	else
		return 0;
}

/**
 * cpr3_regulator_is_enabled() - return the enable state of the CPR3 thread
 * @rdev:		Regulator device pointer for the cpr3-regulator
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: true if regulator is enabled, false if regulator is disabled
 */
static int cpr3_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct cpr3_thread *thread = rdev_get_drvdata(rdev);

	return thread->vreg_enabled;
}

/**
 * cpr3_regulator_enable() - enable the CPR3 thread
 * @rdev:		Regulator device pointer for the cpr3-regulator
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_enable(struct regulator_dev *rdev)
{
	struct cpr3_thread *thread = rdev_get_drvdata(rdev);
	int rc = 0;

	if (thread->vreg_enabled == true)
		return 0;

	mutex_lock(&thread->ctrl->lock);

	rc = regulator_enable(thread->ctrl->vdd_regulator);
	if (rc) {
		cpr3_err(thread, "regulator_enable(vdd) failed, rc=%d\n", rc);
		goto done;
	}

	thread->vreg_enabled = true;
	rc = _cpr3_regulator_set_voltage(thread, thread->current_corner);
	if (rc) {
		cpr3_err(thread, "set voltage failed, rc=%d\n", rc);
		goto done;
	}

	cpr3_debug(thread, "Enabled\n");
done:
	mutex_unlock(&thread->ctrl->lock);

	return rc;
}

/**
 * cpr3_regulator_disable() - disable the CPR3 thread
 * @rdev:		Regulator device pointer for the cpr3-regulator
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_disable(struct regulator_dev *rdev)
{
	struct cpr3_thread *thread = rdev_get_drvdata(rdev);
	int rc = 0;

	if (thread->vreg_enabled == false)
		return 0;

	mutex_lock(&thread->ctrl->lock);

	rc = regulator_disable(thread->ctrl->vdd_regulator);
	if (rc) {
		cpr3_err(thread, "regulator_disable(vdd) failed, rc=%d\n", rc);
		goto done;
	}

	thread->vreg_enabled = false;
	rc = _cpr3_regulator_set_voltage(thread, thread->current_corner);
	if (rc) {
		cpr3_err(thread, "set voltage failed, rc=%d\n", rc);
		goto done;
	}

	cpr3_debug(thread, "Disabled\n");
done:
	mutex_unlock(&thread->ctrl->lock);

	return rc;
}

static struct regulator_ops cpr3_regulator_ops = {
	.enable		= cpr3_regulator_enable,
	.disable	= cpr3_regulator_disable,
	.is_enabled	= cpr3_regulator_is_enabled,
	.set_voltage	= cpr3_regulator_set_voltage,
	.get_voltage	= cpr3_regulator_get_voltage,
	.list_voltage	= cpr3_regulator_list_voltage,
};

/**
 * cpr3_regulator_thread_register() - register a regulator device for a CPR3
 *				      thread
 * @thread:		Pointer to the CPR3 thread
 *
 * This function initializes all regulator framework related structures and then
 * calls regulator_register() for the thread.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_thread_register(struct cpr3_thread *thread)
{
	struct regulator_config config = {};
	struct regulator_desc *rdesc;
	struct regulator_init_data *init_data;
	int rc;

	init_data = of_get_regulator_init_data(thread->ctrl->dev,
						thread->of_node);
	if (!init_data) {
		cpr3_err(thread, "regulator init data is missing\n");
		return -EINVAL;
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS;

	rdesc			= &thread->rdesc;
	rdesc->n_voltages	= thread->corner_count;
	rdesc->name		= init_data->constraints.name;
	rdesc->ops		= &cpr3_regulator_ops;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;

	config.dev		= thread->ctrl->dev;
	config.driver_data	= thread;
	config.init_data	= init_data;
	config.of_node		= thread->of_node;

	thread->rdev = regulator_register(rdesc, &config);
	if (IS_ERR(thread->rdev)) {
		rc = PTR_ERR(thread->rdev);
		cpr3_err(thread, "regulator_register failed, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

/**
 * cpr3_regulator_init_ctrl_data() - performs initialization of CPR controller
 *					elements
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_init_ctrl_data(struct cpr3_controller *ctrl)
{
	/* Read the initial vdd voltage from hardware. */
	ctrl->aggr_corner.last_volt
		= regulator_get_voltage(ctrl->vdd_regulator);
	if (ctrl->aggr_corner.last_volt < 0) {
		cpr3_err(ctrl, "regulator_get_voltage(vdd) failed, rc=%d\n",
				ctrl->aggr_corner.last_volt);
		return ctrl->aggr_corner.last_volt;
	}

	return 0;
}

/**
 * cpr3_regulator_init_thread_data() - performs initialization of common thread
 *					elements
 * @thread:		Pointer to the CPR3 thread
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_init_thread_data(struct cpr3_thread *thread)
{
	int i;

	thread->current_corner = CPR3_REGULATOR_CORNER_INVALID;

	for (i = 0; i < thread->corner_count; i++)
		thread->corner[i].last_volt = thread->corner[i].open_loop_volt;

	return 0;
}

/**
 * cpr3_regulator_suspend() - perform common required CPR3 power down steps
 *		before the system enters suspend
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_regulator_suspend(struct cpr3_controller *ctrl)
{
	return 0;
}

/**
 * cpr3_regulator_resume() - perform common required CPR3 power up steps after
 *		the system resumes from suspend
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_regulator_resume(struct cpr3_controller *ctrl)
{
	return 0;
}

/**
 * cpr3_regulator_register() - register the regulators for a CPR3 controller and
 *		perform CPR hardware initialization
 * @pdev:		Platform device pointer for the CPR3 controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_regulator_register(struct platform_device *pdev,
			struct cpr3_controller *ctrl)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int i, rc;

	if (!dev->of_node) {
		dev_err(dev, "%s: Device tree node is missing\n", __func__);
		return -EINVAL;
	}

	if (!ctrl || !ctrl->name) {
		dev_err(dev, "%s: CPR controller data is missing\n", __func__);
		return -EINVAL;
	}

	if (!ctrl->vdd_regulator) {
		cpr3_err(ctrl, "vdd regulator missing\n");
		return -EINVAL;
	}

	mutex_init(&ctrl->lock);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpr_ctrl");
	if (!res || !res->start) {
		cpr3_err(ctrl, "CPR controller address is missing\n");
		return -ENXIO;
	}
	ctrl->cpr_ctrl_base = devm_ioremap(dev, res->start, resource_size(res));

	rc = cpr3_regulator_init_ctrl_data(ctrl);
	if (rc) {
		cpr3_err(ctrl, "CPR controller data initialization failed, rc=%d\n",
			rc);
		return rc;
	}

	for (i = 0; i < ctrl->thread_count; i++) {
		rc = cpr3_regulator_init_thread_data(&ctrl->thread[i]);
		if (rc) {
			cpr3_err(&ctrl->thread[i], "failed to initialize thread data, rc=%d\n",
				rc);
			return rc;
		}
	}

	/* Register regulator devices for all threads. */
	for (i = 0; i < ctrl->thread_count; i++) {
		rc = cpr3_regulator_thread_register(&ctrl->thread[i]);
		if (rc) {
			cpr3_err(&ctrl->thread[i], "failed to register regulator, rc=%d\n",
				rc);
			return rc;
		}
	}

	mutex_lock(&cpr3_controller_list_mutex);
	list_add(&ctrl->list, &cpr3_controller_list);
	mutex_unlock(&cpr3_controller_list_mutex);

	return 0;
}

/**
 * cpr3_regulator_unregister() - unregister the regulators for a CPR3 controller
 *		and perform CPR hardware shutdown
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
int cpr3_regulator_unregister(struct cpr3_controller *ctrl)
{
	mutex_lock(&cpr3_controller_list_mutex);
	list_del(&ctrl->list);
	mutex_unlock(&cpr3_controller_list_mutex);

	return 0;
}

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
#include <linux/regulator/kryo-regulator.h>

#include "cpr3-regulator.h"

#define CPR3_REGULATOR_CORNER_INVALID	(-1)
#define CPR3_RO_MASK			GENMASK(CPR3_RO_COUNT - 1, 0)

/* CPR3 registers */
#define CPR3_REG_CPR_CTL		0x4
#define CPR3_CPR_CTL_LOOP_EN_MASK	BIT(0)
#define CPR3_CPR_CTL_LOOP_ENABLE	BIT(0)
#define CPR3_CPR_CTL_LOOP_DISABLE	0
#define CPR3_CPR_CTL_IDLE_CLOCKS_MASK	GENMASK(5, 1)
#define CPR3_CPR_CTL_IDLE_CLOCKS_SHIFT	1
#define CPR3_CPR_CTL_COUNT_MODE_MASK	GENMASK(7, 6)
#define CPR3_CPR_CTL_COUNT_MODE_SHIFT	6
#define CPR3_CPR_CTL_COUNT_REPEAT_MASK	GENMASK(31, 9)
#define CPR3_CPR_CTL_COUNT_REPEAT_SHIFT	9

/* This register is not present on controllers that support HW closed-loop. */
#define CPR3_REG_CPR_TIMER_AUTO_CONT	0xC

#define CPR3_REG_CPR_STEP_QUOT		0x14
#define CPR3_CPR_STEP_QUOT_MIN_MASK	GENMASK(5, 0)
#define CPR3_CPR_STEP_QUOT_MIN_SHIFT	0
#define CPR3_CPR_STEP_QUOT_MAX_MASK	GENMASK(11, 6)
#define CPR3_CPR_STEP_QUOT_MAX_SHIFT	6

#define CPR3_REG_GCNT(ro)		(0xA0 + 0x4 * (ro))

#define CPR3_REG_SENSOR_OWNER(sensor)	(0x200 + 0x4 * (sensor))

#define CPR3_REG_CONT_CMD		0x800
#define CPR3_CONT_CMD_ACK		0x1
#define CPR3_CONT_CMD_NACK		0x0

#define CPR3_REG_THRESH(thread)		(0x808 + 0x440 * (thread))
#define CPR3_THRESH_CONS_DOWN_MASK	GENMASK(3, 0)
#define CPR3_THRESH_CONS_DOWN_SHIFT	0
#define CPR3_THRESH_CONS_UP_MASK	GENMASK(7, 4)
#define CPR3_THRESH_CONS_UP_SHIFT	4
#define CPR3_THRESH_DOWN_THRESH_MASK	GENMASK(12, 8)
#define CPR3_THRESH_DOWN_THRESH_SHIFT	8
#define CPR3_THRESH_UP_THRESH_MASK	GENMASK(17, 13)
#define CPR3_THRESH_UP_THRESH_SHIFT	13

#define CPR3_REG_RO_MASK(thread)	(0x80C + 0x440 * (thread))

#define CPR3_REG_RESULT0(thread)	(0x810 + 0x440 * (thread))
#define CPR3_RESULT0_BUSY_MASK		BIT(0)
#define CPR3_RESULT0_STEP_DN_MASK	BIT(1)
#define CPR3_RESULT0_STEP_UP_MASK	BIT(2)
#define CPR3_RESULT0_ERROR_STEPS_MASK	GENMASK(7, 3)
#define CPR3_RESULT0_ERROR_STEPS_SHIFT	3
#define CPR3_RESULT0_ERROR_MASK		GENMASK(19, 8)
#define CPR3_RESULT0_ERROR_SHIFT	8
#define CPR3_RESULT0_NEGATIVE_MASK	BIT(20)

#define CPR3_REG_RESULT1(thread)	(0x814 + 0x440 * (thread))
#define CPR3_RESULT1_QUOT_MIN_MASK	GENMASK(11, 0)
#define CPR3_RESULT1_QUOT_MIN_SHIFT	0
#define CPR3_RESULT1_QUOT_MAX_MASK	GENMASK(23, 12)
#define CPR3_RESULT1_QUOT_MAX_SHIFT	12
#define CPR3_RESULT1_RO_MIN_MASK	GENMASK(27, 24)
#define CPR3_RESULT1_RO_MIN_SHIFT	24
#define CPR3_RESULT1_RO_MAX_MASK	GENMASK(31, 28)
#define CPR3_RESULT1_RO_MAX_SHIFT	28

#define CPR3_REG_RESULT2(thread)		(0x818 + 0x440 * (thread))
#define CPR3_RESULT2_STEP_QUOT_MIN_MASK		GENMASK(5, 0)
#define CPR3_RESULT2_STEP_QUOT_MIN_SHIFT	0
#define CPR3_RESULT2_STEP_QUOT_MAX_MASK		GENMASK(11, 6)
#define CPR3_RESULT2_STEP_QUOT_MAX_SHIFT	6
#define CPR3_RESULT2_SENSOR_MIN_MASK		GENMASK(23, 16)
#define CPR3_RESULT2_SENSOR_MIN_SHIFT		16
#define CPR3_RESULT2_SENSOR_MAX_MASK		GENMASK(31, 24)
#define CPR3_RESULT2_SENSOR_MAX_SHIFT		24

#define CPR3_REG_IRQ_EN			0x81C
#define CPR3_REG_IRQ_CLEAR		0x820
#define CPR3_REG_IRQ_STATUS		0x824
#define CPR3_IRQ_UP			BIT(3)
#define CPR3_IRQ_DOWN			BIT(1)

#define CPR3_REG_TARGET_QUOT(thread, ro) \
					(0x840 + 0x440 * (thread) + 0x4 * (ro))

/* Registers found only on controllers that support HW closed-loop. */
#define CPR3_REG_PD_THROTTLE		0xE8

#define CPR3_REG_HW_CLOSED_LOOP		0x3000
#define CPR3_HW_CLOSED_LOOP_ENABLE	0x0
#define CPR3_HW_CLOSED_LOOP_DISABLE	0x1

#define CPR3_REG_CPR_TIMER_MID_CONT	0x3004
#define CPR3_REG_CPR_TIMER_UP_DN_CONT	0x3008

static DEFINE_MUTEX(cpr3_controller_list_mutex);
static LIST_HEAD(cpr3_controller_list);
static struct dentry *cpr3_debugfs_base;

/**
 * cpr3_read() - read four bytes from the memory address specified
 * @ctrl:		Pointer to the CPR3 controller
 * @offset:		Offset in bytes from the CPR3 controller's base address
 *
 * Return: memory address value
 */
static inline u32 cpr3_read(struct cpr3_controller *ctrl, u32 offset)
{
	if (!ctrl->cpr_enabled) {
		cpr3_err(ctrl, "CPR register reads are not possible when CPR clocks are disabled\n");
		return 0;
	}

	return readl_relaxed(ctrl->cpr_ctrl_base + offset);
}

/**
 * cpr3_write() - write four bytes to the memory address specified
 * @ctrl:		Pointer to the CPR3 controller
 * @offset:		Offset in bytes from the CPR3 controller's base address
 * @value:		Value to write to the memory address
 *
 * Return: none
 */
static inline void cpr3_write(struct cpr3_controller *ctrl, u32 offset,
				u32 value)
{
	if (!ctrl->cpr_enabled) {
		cpr3_err(ctrl, "CPR register writes are not possible when CPR clocks are disabled\n");
		return;
	}

	writel_relaxed(value, ctrl->cpr_ctrl_base + offset);
}

/**
 * cpr3_masked_write() - perform a read-modify-write sequence so that only
 *		masked bits are modified
 * @ctrl:		Pointer to the CPR3 controller
 * @offset:		Offset in bytes from the CPR3 controller's base address
 * @mask:		Mask identifying the bits that should be modified
 * @value:		Value to write to the memory address
 *
 * Return: none
 */
static inline void cpr3_masked_write(struct cpr3_controller *ctrl, u32 offset,
				u32 mask, u32 value)
{
	u32 reg_val, orig_val;

	if (!ctrl->cpr_enabled) {
		cpr3_err(ctrl, "CPR register writes are not possible when CPR clocks are disabled\n");
		return;
	}

	reg_val = orig_val = readl_relaxed(ctrl->cpr_ctrl_base + offset);
	reg_val &= ~mask;
	reg_val |= value & mask;

	if (reg_val != orig_val)
		writel_relaxed(reg_val, ctrl->cpr_ctrl_base + offset);
}

/**
 * cpr3_ctrl_loop_enable() - enable the CPR sensing loop for a given controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: none
 */
static inline void cpr3_ctrl_loop_enable(struct cpr3_controller *ctrl)
{
	if (ctrl->cpr_enabled)
		cpr3_masked_write(ctrl, CPR3_REG_CPR_CTL,
			CPR3_CPR_CTL_LOOP_EN_MASK, CPR3_CPR_CTL_LOOP_ENABLE);
}

/**
 * cpr3_ctrl_loop_disable() - disable the CPR sensing loop for a given
 *		controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: none
 */
static inline void cpr3_ctrl_loop_disable(struct cpr3_controller *ctrl)
{
	if (ctrl->cpr_enabled)
		cpr3_masked_write(ctrl, CPR3_REG_CPR_CTL,
			CPR3_CPR_CTL_LOOP_EN_MASK, CPR3_CPR_CTL_LOOP_DISABLE);
}

/**
 * cpr3_clock_enable() - prepare and enable all clocks used by this CPR3
 *		controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_clock_enable(struct cpr3_controller *ctrl)
{
	int rc;

	rc = clk_prepare_enable(ctrl->bus_clk);
	if (rc) {
		cpr3_err(ctrl, "failed to enable bus clock, rc=%d\n", rc);
		return rc;
	}

	rc = clk_prepare_enable(ctrl->iface_clk);
	if (rc) {
		cpr3_err(ctrl, "failed to enable interface clock, rc=%d\n", rc);
		clk_disable_unprepare(ctrl->bus_clk);
		return rc;
	}

	rc = clk_prepare_enable(ctrl->core_clk);
	if (rc) {
		cpr3_err(ctrl, "failed to enable core clock, rc=%d\n", rc);
		clk_disable_unprepare(ctrl->iface_clk);
		clk_disable_unprepare(ctrl->bus_clk);
		return rc;
	}

	return 0;
}

/**
 * cpr3_clock_disable() - disable and unprepare all clocks used by this CPR3
 *		controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: none
 */
static void cpr3_clock_disable(struct cpr3_controller *ctrl)
{
	clk_disable_unprepare(ctrl->core_clk);
	clk_disable_unprepare(ctrl->iface_clk);
	clk_disable_unprepare(ctrl->bus_clk);
}

/**
 * cpr3_closed_loop_enable() - enable logical CPR closed-loop operation
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_closed_loop_enable(struct cpr3_controller *ctrl)
{
	int rc;

	if (!ctrl->cpr_allowed_hw || !ctrl->cpr_allowed_sw) {
		cpr3_err(ctrl, "cannot enable closed-loop CPR operation because it is disallowed\n");
		return -EPERM;
	} else if (ctrl->cpr_enabled) {
		/* Already enabled */
		return 0;
	} else if (ctrl->cpr_suspended) {
		/*
		 * CPR must remain disabled as the system is entering suspend.
		 */
		return 0;
	}

	rc = cpr3_clock_enable(ctrl);
	if (rc) {
		cpr3_err(ctrl, "unable to enable CPR clocks, rc=%d\n", rc);
		return rc;
	}

	ctrl->cpr_enabled = true;
	cpr3_debug(ctrl, "CPR closed-loop operation enabled\n");

	return 0;
}

/**
 * cpr3_closed_loop_disable() - disable logical CPR closed-loop operation
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static inline int cpr3_closed_loop_disable(struct cpr3_controller *ctrl)
{
	if (!ctrl->cpr_enabled) {
		/* Already disabled */
		return 0;
	}

	cpr3_clock_disable(ctrl);
	ctrl->cpr_enabled = false;
	cpr3_debug(ctrl, "CPR closed-loop operation disabled\n");

	return 0;
}

/**
 * cpr3_regulator_init_thread() - performs hardware initialization of CPR
 *		thread registers
 * @thread:		Pointer to the CPR3 thread
 *
 * CPR interface/bus clocks must be enabled before calling this function.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_init_thread(struct cpr3_thread *thread)
{
	u32 reg;

	reg = (thread->consecutive_up << CPR3_THRESH_CONS_UP_SHIFT)
		& CPR3_THRESH_CONS_UP_MASK;
	reg |= (thread->consecutive_down << CPR3_THRESH_CONS_DOWN_SHIFT)
		& CPR3_THRESH_CONS_DOWN_MASK;
	reg |= (thread->up_threshold << CPR3_THRESH_UP_THRESH_SHIFT)
		& CPR3_THRESH_UP_THRESH_MASK;
	reg |= (thread->down_threshold << CPR3_THRESH_DOWN_THRESH_SHIFT)
		& CPR3_THRESH_DOWN_THRESH_MASK;

	cpr3_write(thread->ctrl, CPR3_REG_THRESH(thread->thread_id), reg);

	/*
	 * Mask all RO's initially so that unused thread doesn't contribute
	 * to closed-loop voltage.
	 */
	cpr3_write(thread->ctrl, CPR3_REG_RO_MASK(thread->thread_id),
		CPR3_RO_MASK);

	return 0;
}

/**
 * cpr3_regulator_init_ctrl() - performs hardware initialization of CPR
 *		controller registers
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_init_ctrl(struct cpr3_controller *ctrl)
{
	int i, j, k, rc;
	u32 ro_used = 0;
	u32 gcnt, cont_dly, up_down_dly, val;
	unsigned int remainder;
	u64 temp;
	char *mode;

	rc = clk_set_rate(ctrl->core_clk, ctrl->cpr_clock_rate);
	if (rc) {
		cpr3_err(ctrl, "clk_set_rate(core_clk, %u) failed, rc=%d\n",
			ctrl->cpr_clock_rate, rc);
		return rc;
	}

	rc = cpr3_clock_enable(ctrl);
	if (rc) {
		cpr3_err(ctrl, "clock enable failed, rc=%d\n", rc);
		return rc;
	}
	ctrl->cpr_enabled = true;

	/* Find all RO's used by any corner of any thread. */
	for (i = 0; i < ctrl->thread_count; i++)
		for (j = 0; j < ctrl->thread[i].corner_count; j++)
			for (k = 0; k < CPR3_RO_COUNT; k++)
				if (ctrl->thread[i].corner[j].target_quot[k])
					ro_used |= BIT(k);

	/* Configure the GCNT of the RO's that will be used */
	temp = (u64)ctrl->cpr_clock_rate * (u64)ctrl->sensor_time;
	remainder = do_div(temp, 1000000000);
	if (remainder)
		temp++;
	/*
	 * GCNT == 0 corresponds to a single ref clock measurement interval so
	 * offset GCNT values by 1.
	 */
	gcnt = temp - 1;
	for (i = 0; i < CPR3_RO_COUNT; i++)
		if (ro_used & BIT(i))
			cpr3_write(ctrl, CPR3_REG_GCNT(i), gcnt);

	/* Configure the loop delay time */
	temp = (u64)ctrl->cpr_clock_rate * (u64)ctrl->loop_time;
	do_div(temp, 1000000000);
	cont_dly = temp;
	if (ctrl->supports_hw_closed_loop)
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_MID_CONT, cont_dly);
	else
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_AUTO_CONT, cont_dly);

	temp = (u64)ctrl->cpr_clock_rate * (u64)ctrl->up_down_delay_time;
	do_div(temp, 1000000000);
	up_down_dly = temp;
	if (ctrl->supports_hw_closed_loop)
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_UP_DN_CONT, up_down_dly);

	cpr3_debug(ctrl, "cpr_clock_rate=%u HZ, sensor_time=%u ns, loop_time=%u ns, up_down_delay_time=%u ns\n",
		ctrl->cpr_clock_rate, ctrl->sensor_time, ctrl->loop_time,
		ctrl->up_down_delay_time);
	cpr3_debug(ctrl, "gcnt=%u, cont_dly=%u, up_down_dly=%u\n",
		gcnt, cont_dly, up_down_dly);

	/* Configure CPR sensor operation */
	val = (ctrl->idle_clocks << CPR3_CPR_CTL_IDLE_CLOCKS_SHIFT)
		& CPR3_CPR_CTL_IDLE_CLOCKS_MASK;
	val |= (ctrl->count_mode << CPR3_CPR_CTL_COUNT_MODE_SHIFT)
		& CPR3_CPR_CTL_COUNT_MODE_MASK;
	val |= (ctrl->count_repeat << CPR3_CPR_CTL_COUNT_REPEAT_SHIFT)
		& CPR3_CPR_CTL_COUNT_REPEAT_MASK;
	cpr3_write(ctrl, CPR3_REG_CPR_CTL, val);

	cpr3_debug(ctrl, "idle_clocks=%u, count_mode=%u, count_repeat=%u; CPR_CTL=0x%08X\n",
		ctrl->idle_clocks, ctrl->count_mode, ctrl->count_repeat, val);

	/* Configure CPR default step quotients */
	val = (ctrl->step_quot_init_min << CPR3_CPR_STEP_QUOT_MIN_SHIFT)
		& CPR3_CPR_STEP_QUOT_MIN_MASK;
	val |= (ctrl->step_quot_init_max << CPR3_CPR_STEP_QUOT_MAX_SHIFT)
		& CPR3_CPR_STEP_QUOT_MAX_MASK;
	cpr3_write(ctrl, CPR3_REG_CPR_STEP_QUOT, val);

	cpr3_debug(ctrl, "step_quot_min=%u, step_quot_max=%u; STEP_QUOT=0x%08X\n",
		ctrl->step_quot_init_min, ctrl->step_quot_init_max, val);

	/* Configure the CPR sensor ownership */
	for (i = 0; i < ctrl->sensor_count; i++)
		cpr3_write(ctrl, CPR3_REG_SENSOR_OWNER(i),
			   ctrl->sensor_owner[i]);

	/* Configure per-thread registers */
	for (i = 0; i < ctrl->thread_count; i++) {
		rc = cpr3_regulator_init_thread(&ctrl->thread[i]);
		if (rc) {
			cpr3_err(ctrl, "CPR thread register initialization failed, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (ctrl->supports_hw_closed_loop) {
		cpr3_write(ctrl, CPR3_REG_HW_CLOSED_LOOP,
			ctrl->use_hw_closed_loop
				? CPR3_HW_CLOSED_LOOP_ENABLE
				: CPR3_HW_CLOSED_LOOP_DISABLE);

		val = ctrl->proc_clock_throttle;
		cpr3_write(ctrl, CPR3_REG_PD_THROTTLE, val);
		cpr3_debug(ctrl, "PD_THROTTLE=0x%08X\n", val);
	}

	if (ctrl->use_hw_closed_loop) {
		rc = regulator_enable(ctrl->vdd_limit_regulator);
		if (rc) {
			cpr3_err(ctrl, "CPR limit regulator enable failed, rc=%d\n",
				rc);
			return rc;
		}
	}

	/* Ensure that all register writes complete before disabling clocks. */
	wmb();

	cpr3_clock_disable(ctrl);
	ctrl->cpr_enabled = false;

	if (!ctrl->cpr_allowed_sw || !ctrl->cpr_allowed_hw)
		mode = "open-loop";
	else if (ctrl->supports_hw_closed_loop)
		mode = ctrl->use_hw_closed_loop
			? "HW closed-loop" : "SW closed-loop";
	else
		mode = "closed-loop";

	cpr3_info(ctrl, "Default CPR mode = %s", mode);

	return 0;
}

/**
 * cpr3_regulator_set_target_quot() - configure the target quotient for each
 *		RO of the CPR3 thread and set the RO mask
 * @thread:		Pointer to the CPR3 thread
 * @new_corner:		The new corner that is being switched to
 * @last_corner:	The previous corner that was configured
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_set_target_quot(struct cpr3_thread *thread,
					int new_corner, int last_corner)
{
	u32 new_quot, last_quot;
	int i;
	bool initial = (last_corner == CPR3_REGULATOR_CORNER_INVALID);

	if (new_corner == last_corner)
		return 0;

	/* Avoid out-of-bound array access. */
	if (last_corner < 0)
		last_corner = 0;

	for (i = 0; i < CPR3_RO_COUNT; i++) {
		new_quot = thread->corner[new_corner].target_quot[i];
		last_quot = thread->corner[last_corner].target_quot[i];
		if (initial || new_quot != last_quot)
			cpr3_write(thread->ctrl,
				CPR3_REG_TARGET_QUOT(thread->thread_id, i),
				new_quot);
	}

	if (initial ||
	    thread->corner[new_corner].ro_mask
				!= thread->corner[last_corner].ro_mask)
		cpr3_write(thread->ctrl, CPR3_REG_RO_MASK(thread->thread_id),
			thread->corner[new_corner].ro_mask);

	return 0;
}

/**
 * cpr3_regulator_config_ldo_retention() - configure per-thread LDO retention mode
 * @thread:		Pointer to the CPR3 thread to configure
 * @floor_volt:		vdd floor voltage
 *
 * This function determines if a CPR thread's configuration satisfies safe
 * operating voltages for LDO retention and uses the regulator_allow_bypass()
 * interface on the LDO retention regulator to enable or disable such feature
 * accordingly.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_config_ldo_retention(struct cpr3_thread *thread,
					int floor_volt)
{
	struct regulator *ldo_ret_reg = thread->ldo_ret_regulator;
	int retention_volt, rc;
	enum kryo_supply_mode mode;

	retention_volt = regulator_get_voltage(ldo_ret_reg);
	if (retention_volt < 0) {
		cpr3_err(thread, "regulator_get_voltage(ldo_ret) failed, rc=%d\n",
			 retention_volt);
		return retention_volt;

	}

	mode = floor_volt >= retention_volt + thread->ldo_headroom_volt
		? LDO_MODE : BHS_MODE;

	rc = regulator_allow_bypass(ldo_ret_reg, mode);
	if (rc)
		cpr3_err(thread, "regulator_allow_bypass(ldo_ret) == %s failed, rc=%d\n",
			 mode ? "true" : "false", rc);

	return rc;
}

/**
 * cpr3_regulator_set_bhs_mode() - configure the LDO regulator associated with
 *		a CPR thread to BHS mode
 * @thread:		Pointer to the CPR thread
 * @vdd_volt:		Last known settled voltage in microvolts for the VDD
 *			supply
 * @vdd_ceiling_volt:	Last known aggregated ceiling voltage in microvolts for
 *			the VDD supply
 *
 * This function performs the necessary steps to switch an LDO regulator
 * to BHS mode (LDO bypassed mode).
 */
static int cpr3_regulator_set_bhs_mode(struct cpr3_thread *thread,
			       int vdd_volt, int vdd_ceiling_volt)
{
	struct regulator *ldo_reg = thread->ldo_regulator;
	int bhs_volt, rc;

	bhs_volt = vdd_volt - thread->ldo_headroom_volt;
	if (bhs_volt > thread->ldo_max_volt) {
		cpr3_debug(thread, "limited to LDO output of %d uV when switching to BHS mode\n",
			   thread->ldo_max_volt);
		bhs_volt = thread->ldo_max_volt;
	}

	rc = regulator_set_voltage(ldo_reg, bhs_volt, vdd_ceiling_volt);
	if (rc) {
		cpr3_err(thread, "regulator_set_voltage(ldo) == %d failed, rc=%d\n",
			 bhs_volt, rc);
		return rc;
	}

	rc = regulator_allow_bypass(ldo_reg, BHS_MODE);
	if (rc) {
		cpr3_err(thread, "regulator_allow_bypass(ldo) == %s failed, rc=%d\n",
			 BHS_MODE ? "true" : "false", rc);
		return rc;
	}
	thread->ldo_regulator_bypass = BHS_MODE;

	return rc;
}

/**
 * cpr3_regulator_config_ldo() - configure the voltage and bypass state for the
 *		LDO regulator associated with each CPR thread
 * @ctrl:		Pointer to the CPR3 controller
 * @vdd_floor_volt:	Last known aggregated floor voltage in microvolts for
 *			the VDD supply
 * @vdd_ceiling_volt:	Last known aggregated ceiling voltage in microvolts for
 *			the VDD supply
 *
 * This function performs all relevant LDO or BHS configurations if per-thread
 * LDO regulators are supported.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_config_ldo(struct cpr3_controller *ctrl,
			     int vdd_floor_volt, int vdd_ceiling_volt)
{
	struct regulator *ldo_reg;
	struct cpr3_thread *thread;
	int i, rc, ldo_volt, bhs_volt, vdd_volt = 0;

	for (i = 0; i < ctrl->thread_count; i++) {
		thread = &ctrl->thread[i];
		ldo_reg = thread->ldo_regulator;

		if (!ldo_reg)
			continue;

		rc = cpr3_regulator_config_ldo_retention(thread,
							 vdd_floor_volt);
		if (rc)
			return rc;

		if (!thread->vreg_enabled || !thread->ldo_mode_allowed
		    || thread->current_corner == CPR3_REGULATOR_CORNER_INVALID)
			continue;

		/* Avoid unnecessary requests */
		if (!vdd_volt) {
			vdd_volt  = regulator_get_voltage(ctrl->vdd_regulator);
			if (vdd_volt < 0) {
				cpr3_err(ctrl, "regulator_get_voltage(vdd) failed, rc=%d\n",
					 vdd_volt);
				return vdd_volt;
			}
		}

		ldo_volt = thread->corner[thread->current_corner].open_loop_volt
			- thread->ldo_adjust_volt;
		bhs_volt = vdd_volt - thread->ldo_headroom_volt;

		if (vdd_floor_volt >= ldo_volt
		    + thread->ldo_headroom_volt) {
			if (thread->ldo_regulator_bypass == BHS_MODE) {
				if (bhs_volt > thread->ldo_max_volt ||
				    ldo_volt > thread->ldo_max_volt) {
					cpr3_debug(ctrl, "cannot support LDO mode with bhs_volt=%d uV, ldo_volt=%d uV\n",
						   bhs_volt, ldo_volt);
					/* Skip thread, can't switch to LDO */
					continue;
				}

				/*
				 * BHS to LDO transition. Configure LDO output
				 * to VDD minus LDO headroom voltage then
				 * switch the regulator mode.
				 */
				rc = regulator_set_voltage(ldo_reg, bhs_volt,
							   vdd_ceiling_volt);
				if (rc) {
					cpr3_err(ctrl, "regulator_set_voltage(ldo) == %d failed, rc=%d\n",
						 bhs_volt, rc);
					return rc;
				}

				rc = regulator_allow_bypass(ldo_reg, LDO_MODE);
				if (rc) {
					cpr3_err(ctrl, "regulator_allow_bypass(ldo) == %s failed, rc=%d\n",
						 LDO_MODE ?
						 "true" : "false", rc);
					return rc;
				}
				thread->ldo_regulator_bypass = LDO_MODE;
			}

			/* Configure final LDO output voltage */
			rc = regulator_set_voltage(ldo_reg, ldo_volt,
						   vdd_ceiling_volt);
			if (rc) {
				cpr3_err(ctrl, "regulator_set_voltage(ldo) == %d failed, rc=%d\n",
					 ldo_volt, rc);
				return rc;
			}
		} else {
			if (thread->ldo_regulator_bypass == LDO_MODE) {
				rc = cpr3_regulator_set_bhs_mode(thread,
						 vdd_volt, vdd_ceiling_volt);
				if (rc)
					return rc;
			}
		}
	}

	return 0;
}

/**
 * cpr3_regulator_scale_vdd_voltage() - scale the CPR controlled VDD supply
 *		voltage to the new level while satisfying any other hardware
 *		requirements
 * @ctrl:		Pointer to the CPR3 controller
 * @new_volt:		New voltage in microvolts that VDD needs to end up at
 * @aggr_corner:	Pointer to the CPR3 corner which corresponds to the max
 *			corner aggregate of all CPR3 threads managed by the CPR3
 *			controller
 *
 * This function scales the CPR controlled VDD supply voltage from its
 * current level to the new voltage that is specified.  If the supply is
 * configured to use the APM and the APM threshold is crossed as a result of
 * the voltage scaling, then this function also stops at the APM threshold,
 * switches the APM source, and finally sets the final new voltage.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_scale_vdd_voltage(struct cpr3_controller *ctrl,
				int new_volt, struct cpr3_corner *aggr_corner)
{
	struct regulator *vdd = ctrl->vdd_regulator;
	bool apm_crossing = false;
	int apm_volt = ctrl->apm_threshold_volt;
	int last_max_volt = ctrl->aggr_corner.ceiling_volt;
	int max_volt = aggr_corner->ceiling_volt;
	int last_volt, rc;

	last_volt = (ctrl->cpr_enabled && !ctrl->use_hw_closed_loop)
			? ctrl->aggr_corner.last_volt
			: ctrl->aggr_corner.open_loop_volt;

	if (ctrl->apm && apm_volt > 0
		&& ((last_volt < apm_volt && apm_volt <= new_volt)
			|| (last_volt >= apm_volt && apm_volt > new_volt)))
		apm_crossing = true;

	if (apm_crossing) {
		rc = regulator_set_voltage(vdd, apm_volt, apm_volt);
		if (rc) {
			cpr3_err(ctrl, "regulator_set_voltage(vdd) == %d failed, rc=%d\n",
				apm_volt, rc);
			return rc;
		}

		rc = msm_apm_set_supply(ctrl->apm, new_volt >= apm_volt
				? ctrl->apm_high_supply : ctrl->apm_low_supply);
		if (rc) {
			cpr3_err(ctrl, "APM switch failed, rc=%d\n", rc);
			/* Roll back the voltage. */
			regulator_set_voltage(vdd, last_volt, INT_MAX);
			return rc;
		}
	}

	if (new_volt < last_volt) {
		rc = cpr3_regulator_config_ldo(ctrl, aggr_corner->floor_volt,
					       last_max_volt);
		if (rc) {
			cpr3_err(ctrl, "unable to configure LDO state, rc=%d\n",
				 rc);
			return rc;
		}
	}

	/*
	 * Subtract a small amount from the min_uV parameter so that the
	 * set voltage request is not dropped by the framework due to being
	 * duplicate.  This is needed in order to switch from hardware
	 * closed-loop to open-loop successfully.
	 */
	rc = regulator_set_voltage(vdd, new_volt - (ctrl->cpr_enabled ? 0 : 1),
				max_volt);
	if (rc) {
		cpr3_err(ctrl, "regulator_set_voltage(vdd) == %d failed, rc=%d\n",
			new_volt, rc);
		return rc;
	}

	if (new_volt >= last_volt) {
		rc = cpr3_regulator_config_ldo(ctrl, aggr_corner->floor_volt,
					       max_volt);
		if (rc) {
			cpr3_err(ctrl, "unable to configure LDO state, rc=%d\n",
				 rc);
			return rc;
		}
	}

	return 0;
}

/**
 * cpr3_regulator_update_ctrl_state() - update the state of the CPR controller
 *		to reflect the corners used by all threads as well as the
 *		CPR operating mode
 * @ctrl:		Pointer to the CPR3 controller
 *
 * This function aggregates the CPR parameters for all threads associated with
 * the VDD supply.  Upon success, it sets the aggregated last known good
 * voltage.
 *
 * The VDD supply voltage will not be physically configured unless this
 * condition is met by at least one of the threads of the controller:
 * thread->vreg_enabled == true &&
 * thread->current_corner != CPR3_REGULATOR_CORNER_INVALID
 *
 * CPR registers for the controller and each thread are updated as long as
 * ctrl->cpr_enabled == true.
 *
 * Note, CPR3 controller lock must be held by the caller.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_update_ctrl_state(struct cpr3_controller *ctrl)
{
	struct cpr3_corner aggr_corner = {};
	struct cpr3_corner *corn;
	struct cpr3_thread *thread;
	bool valid = false;
	int i, rc, new_volt;

	/* Aggregate the requests of all threads */
	for (i = 0; i < ctrl->thread_count; i++) {
		thread = &ctrl->thread[i];

		if (!thread->vreg_enabled || thread->current_corner
					== CPR3_REGULATOR_CORNER_INVALID) {
			/* Cannot participate in aggregation. */
			continue;
		} else {
			valid = true;
		}

		corn = &thread->corner[thread->current_corner];

		aggr_corner.ceiling_volt = max(aggr_corner.ceiling_volt,
						corn->ceiling_volt);
		aggr_corner.floor_volt = max(aggr_corner.floor_volt,
						corn->floor_volt);
		aggr_corner.last_volt = max(aggr_corner.last_volt,
						corn->last_volt);
		aggr_corner.open_loop_volt = max(aggr_corner.open_loop_volt,
						corn->open_loop_volt);
		aggr_corner.irq_en |= corn->irq_en;
	}

	cpr3_ctrl_loop_disable(ctrl);

	if (valid && ctrl->cpr_allowed_hw && ctrl->cpr_allowed_sw) {
		rc = cpr3_closed_loop_enable(ctrl);
		if (rc) {
			cpr3_err(ctrl, "could not enable CPR, rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = cpr3_closed_loop_disable(ctrl);
		if (rc) {
			cpr3_err(ctrl, "could not disable CPR, rc=%d\n", rc);
			return rc;
		}
	}

	/* No threads are enabled with a valid corner so exit. */
	if (!valid)
		return 0;

	new_volt = (ctrl->cpr_enabled && !ctrl->use_hw_closed_loop)
			? aggr_corner.last_volt
			: aggr_corner.open_loop_volt;

	cpr3_debug(ctrl, "setting new voltage=%d uV\n", new_volt);
	rc = cpr3_regulator_scale_vdd_voltage(ctrl, new_volt,
						&aggr_corner);
	if (rc) {
		cpr3_err(ctrl, "vdd voltage scaling failed, rc=%d\n", rc);
		return rc;
	}

	/* Only update registers if CPR is enabled. */
	if (ctrl->cpr_enabled) {
		if (ctrl->use_hw_closed_loop) {
			/* Hardware closed-loop */

			/* Set ceiling and floor limits in hardware */
			rc = regulator_set_voltage(ctrl->vdd_limit_regulator,
				aggr_corner.floor_volt,
				aggr_corner.ceiling_volt);
			if (rc) {
				cpr3_err(ctrl, "could not configure HW closed-loop voltage limits, rc=%d\n",
					rc);
				return rc;
			}
		} else {
			/* Software closed-loop */

			/*
			 * Disable UP or DOWN interrupts when at ceiling or
			 * floor respectively.
			 */
			if (new_volt == aggr_corner.floor_volt)
				aggr_corner.irq_en &= ~CPR3_IRQ_DOWN;
			if (new_volt == aggr_corner.ceiling_volt)
				aggr_corner.irq_en &= ~CPR3_IRQ_UP;

			cpr3_write(ctrl, CPR3_REG_IRQ_CLEAR,
				CPR3_IRQ_UP | CPR3_IRQ_DOWN);
			cpr3_write(ctrl, CPR3_REG_IRQ_EN, aggr_corner.irq_en);
		}

		for (i = 0; i < ctrl->thread_count; i++) {
			thread = &ctrl->thread[i];

			if (thread->current_corner
					== CPR3_REGULATOR_CORNER_INVALID)
				continue;

			rc = cpr3_regulator_set_target_quot(thread,
					thread->current_corner,
					thread->last_closed_loop_corner);
			if (rc) {
				cpr3_err(thread, "could not configure target quotients, rc=%d\n",
					rc);
				return rc;
			}

			thread->last_closed_loop_corner
				= thread->current_corner;
		}

		/*
		 * Ensure that all CPR register writes complete before
		 * re-enabling CPR loop operation.
		 */
		wmb();
	}

	/*
	 * Only enable the CPR controller if it is possible to set more than
	 * one vdd-supply voltage.
	 */
	if (aggr_corner.ceiling_volt > aggr_corner.floor_volt)
		cpr3_ctrl_loop_enable(ctrl);

	ctrl->aggr_corner = aggr_corner;

	cpr3_debug(ctrl, "CPR configuration updated\n");

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
	int last_corner;

	corner -= CPR3_CORNER_OFFSET;
	corner_max -= CPR3_CORNER_OFFSET;
	*selector = corner;

	mutex_lock(&thread->ctrl->lock);

	if (!thread->vreg_enabled) {
		thread->current_corner = corner;
		cpr3_debug(thread, "stored corner=%d\n", corner);
		goto done;
	} else if (thread->current_corner == corner) {
		goto done;
	}

	last_corner = thread->current_corner;
	thread->current_corner = corner;

	rc = cpr3_regulator_update_ctrl_state(thread->ctrl);
	if (rc) {
		cpr3_err(thread, "could not update CPR state, rc=%d\n", rc);
		thread->current_corner = last_corner;
	}

	cpr3_debug(thread, "set corner=%d\n", corner);
done:
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
 * cpr3_regulator_list_corner_voltage() - return the ceiling voltage mapped to
 *			the specified voltage corner
 * @rdev:		Regulator device pointer for the cpr3-regulator
 * @corner:		Voltage corner
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: voltage value in microvolts or -EINVAL if the corner is out of range
 */
static int cpr3_regulator_list_corner_voltage(struct regulator_dev *rdev,
		int corner)
{
	struct cpr3_thread *thread = rdev_get_drvdata(rdev);

	corner -= CPR3_CORNER_OFFSET;

	if (corner >= 0 && corner < thread->corner_count)
		return thread->corner[corner].ceiling_volt;
	else
		return -EINVAL;
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

	if (thread->ldo_regulator) {
		rc = regulator_enable(thread->ldo_regulator);
		if (rc) {
			cpr3_err(thread, "regulator_enable(ldo) failed, rc=%d\n",
				 rc);
			goto done;
		}
	}

	thread->vreg_enabled = true;
	rc = cpr3_regulator_update_ctrl_state(thread->ctrl);
	if (rc) {
		cpr3_err(thread, "could not update CPR state, rc=%d\n", rc);
		regulator_disable(thread->ctrl->vdd_regulator);
		thread->vreg_enabled = false;
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
	int rc, rc2;

	if (thread->vreg_enabled == false)
		return 0;

	mutex_lock(&thread->ctrl->lock);

	if (thread->ldo_regulator && thread->ldo_regulator_bypass == LDO_MODE) {
		rc = regulator_get_voltage(thread->ctrl->vdd_regulator);
		if (rc < 0) {
			cpr3_err(thread, "regulator_get_voltage(vdd) failed, rc=%d\n",
				 rc);
			goto done;
		}

		/* Switch back to BHS for safe operation */
		rc = cpr3_regulator_set_bhs_mode(thread, rc,
				       thread->ctrl->aggr_corner.ceiling_volt);
		if (rc) {
			cpr3_err(thread, "unable to switch to BHS mode, rc=%d\n",
				 rc);
			return rc;
		}
	}

	if (thread->ldo_regulator) {
		rc = regulator_disable(thread->ldo_regulator);
		if (rc) {
			cpr3_err(thread, "regulator_disable(ldo) failed, rc=%d\n",
				 rc);
			goto done;
		}
	}
	rc = regulator_disable(thread->ctrl->vdd_regulator);
	if (rc) {
		cpr3_err(thread, "regulator_disable(vdd) failed, rc=%d\n", rc);
		goto done;
	}

	thread->vreg_enabled = false;
	rc = cpr3_regulator_update_ctrl_state(thread->ctrl);
	if (rc) {
		cpr3_err(thread, "could not update CPR state, rc=%d\n", rc);
		rc2 = regulator_enable(thread->ctrl->vdd_regulator);
		thread->vreg_enabled = true;
		goto done;
	}

	cpr3_debug(thread, "Disabled\n");
done:
	mutex_unlock(&thread->ctrl->lock);

	return rc;
}

static struct regulator_ops cpr3_regulator_ops = {
	.enable			= cpr3_regulator_enable,
	.disable		= cpr3_regulator_disable,
	.is_enabled		= cpr3_regulator_is_enabled,
	.set_voltage		= cpr3_regulator_set_voltage,
	.get_voltage		= cpr3_regulator_get_voltage,
	.list_voltage		= cpr3_regulator_list_voltage,
	.list_corner_voltage	= cpr3_regulator_list_corner_voltage,
};

/**
 * cpr3_print_result() - print CPR measurement results to the kernel log for
 *		debugging purposes
 * @thread:		Pointer to the CPR3 thread
 *
 * Return: None
 */
static void cpr3_print_result(struct cpr3_thread *thread)
{
	struct cpr3_controller *ctrl = thread->ctrl;
	u32 result[3], busy, step_dn, step_up, error_steps, error, negative;
	u32 quot_min, quot_max, ro_min, ro_max, step_quot_min, step_quot_max;
	u32 sensor_min, sensor_max;
	char *sign;

	result[0] = cpr3_read(ctrl, CPR3_REG_RESULT0(thread->thread_id));
	result[1] = cpr3_read(ctrl, CPR3_REG_RESULT1(thread->thread_id));
	result[2] = cpr3_read(ctrl, CPR3_REG_RESULT2(thread->thread_id));

	busy = !!(result[0] & CPR3_RESULT0_BUSY_MASK);
	step_dn = !!(result[0] & CPR3_RESULT0_STEP_DN_MASK);
	step_up = !!(result[0] & CPR3_RESULT0_STEP_UP_MASK);
	error_steps = (result[0] & CPR3_RESULT0_ERROR_STEPS_MASK)
			>> CPR3_RESULT0_ERROR_STEPS_SHIFT;
	error = (result[0] & CPR3_RESULT0_ERROR_MASK)
			>> CPR3_RESULT0_ERROR_SHIFT;
	negative = !!(result[0] & CPR3_RESULT0_NEGATIVE_MASK);

	quot_min = (result[1] & CPR3_RESULT1_QUOT_MIN_MASK)
			>> CPR3_RESULT1_QUOT_MIN_SHIFT;
	quot_max = (result[1] & CPR3_RESULT1_QUOT_MAX_MASK)
			>> CPR3_RESULT1_QUOT_MAX_SHIFT;
	ro_min = (result[1] & CPR3_RESULT1_RO_MIN_MASK)
			>> CPR3_RESULT1_RO_MIN_SHIFT;
	ro_max = (result[1] & CPR3_RESULT1_RO_MAX_MASK)
			>> CPR3_RESULT1_RO_MAX_SHIFT;

	step_quot_min = (result[2] & CPR3_RESULT2_STEP_QUOT_MIN_MASK)
			>> CPR3_RESULT2_STEP_QUOT_MIN_SHIFT;
	step_quot_max = (result[2] & CPR3_RESULT2_STEP_QUOT_MAX_MASK)
			>> CPR3_RESULT2_STEP_QUOT_MAX_SHIFT;
	sensor_min = (result[2] & CPR3_RESULT2_SENSOR_MIN_MASK)
			>> CPR3_RESULT2_SENSOR_MIN_SHIFT;
	sensor_max = (result[2] & CPR3_RESULT2_SENSOR_MAX_MASK)
			>> CPR3_RESULT2_SENSOR_MAX_SHIFT;

	sign = negative ? "-" : "";
	cpr3_debug(thread, "busy=%u, step_dn=%u, step_up=%u, error_steps=%s%u, error=%s%u\n",
		busy, step_dn, step_up, sign, error_steps, sign, error);
	cpr3_debug(thread, "quot_min=%u, quot_max=%u, ro_min=%u, ro_max=%u\n",
		quot_min, quot_max, ro_min, ro_max);
	cpr3_debug(thread, "step_quot_min=%u, step_quot_max=%u, sensor_min=%u, sensor_max=%u\n",
		step_quot_min, step_quot_max, sensor_min, sensor_max);
}

/**
 * cpr3_thread_busy() - returns if the specified CPR3 thread is busy taking
 *		a measurement
 * @thread:		Pointer to the CPR3 thread
 *
 * Return: CPR3 busy status
 */
static bool cpr3_thread_busy(struct cpr3_thread *thread)
{
	u32 result;

	result = cpr3_read(thread->ctrl, CPR3_REG_RESULT0(thread->thread_id));

	return !!(result & CPR3_RESULT0_BUSY_MASK);
}

/**
 * cpr3_update_last_volt() - update the closed-loop last_volt value for a CPR3
 *		thread if this thread was responsible for the aggregated
 *		change in voltage (up or down)
 * @thread:		Pointer to the CPR3 thread
 * @new_volt:		The new voltage being set at the PMIC as a result of a
 *			CPR interrupt
 * @aggr_step_up:	Flag indicating that the aggregated change in voltage
 *			was up as opposed to down
 *
 * Return: None
 */
static void cpr3_update_last_volt(struct cpr3_thread *thread, int new_volt,
				bool aggr_step_up)
{
	bool step_dn, step_up;
	struct cpr3_corner *corner;
	u32 result;

	result = cpr3_read(thread->ctrl, CPR3_REG_RESULT0(thread->thread_id));

	step_dn = !!(result & CPR3_RESULT0_STEP_DN_MASK);
	step_up = !!(result & CPR3_RESULT0_STEP_UP_MASK);

	corner = &thread->corner[thread->current_corner];
	if (aggr_step_up && step_up && new_volt > corner->last_volt
				&& new_volt <= corner->ceiling_volt)
		corner->last_volt = new_volt;
	else if (!aggr_step_up && step_dn && new_volt < corner->last_volt
				&& new_volt >= corner->floor_volt)
		corner->last_volt = new_volt;
}

/**
 * cpr3_irq_handler() - CPR interrupt handler callback function used for
 *		software closed-loop operation
 * @irq:		CPR interrupt number
 * @data:		Private data corresponding to the CPR3 controller
 *			pointer
 *
 * This function increases or decreases the vdd supply voltage based upon the
 * CPR controller recommendation.
 *
 * Return: IRQ_HANDLED
 */
static irqreturn_t cpr3_irq_handler(int irq, void *data)
{
	struct cpr3_controller *ctrl = data;
	struct cpr3_corner *aggr = &ctrl->aggr_corner;
	u32 cont = CPR3_CONT_CMD_NACK;
	struct cpr3_corner *corner;
	int i, new_volt, last_volt, rc;
	u32 irq_en, status;
	bool up, down;

	mutex_lock(&ctrl->lock);

	if (!ctrl->cpr_enabled) {
		cpr3_debug(ctrl, "CPR interrupt received but CPR is disabled\n");
		mutex_unlock(&ctrl->lock);
		return IRQ_HANDLED;
	} else if (ctrl->use_hw_closed_loop) {
		cpr3_debug(ctrl, "CPR interrupt received but CPR is using HW closed-loop\n");
		goto done;
	}

	status = cpr3_read(ctrl, CPR3_REG_IRQ_STATUS);
	up = status & CPR3_IRQ_UP;
	down = status & CPR3_IRQ_DOWN;

	irq_en = aggr->irq_en;
	last_volt = aggr->last_volt;

	if (!up && !down) {
		cpr3_debug(ctrl, "CPR interrupt received but no up or down status bit is set\n");
		goto done;
	} else if (up && down) {
		cpr3_err(ctrl, "both up and down status bits set\n");
		goto done;
	}

	for (i = 0; i < ctrl->thread_count; i++) {
		if (cpr3_thread_busy(&ctrl->thread[i])) {
			cpr3_err(&ctrl->thread[i], "CPR thread busy when it should be waiting for SW cont\n");
			goto done;
		}
	}

	new_volt = up ? last_volt + ctrl->thread[0].step_volt
		      : last_volt - ctrl->thread[0].step_volt;

	/* Re-enable UP/DOWN interrupt when its opposite is received. */
	irq_en |= up ? CPR3_IRQ_DOWN : CPR3_IRQ_UP;

	if (new_volt > aggr->ceiling_volt) {
		new_volt = aggr->ceiling_volt;
		irq_en &= ~CPR3_IRQ_UP;
		cpr3_debug(ctrl, "limiting to ceiling=%d uV\n",
			aggr->ceiling_volt);
	} else if (new_volt < aggr->floor_volt) {
		new_volt = aggr->floor_volt;
		irq_en &= ~CPR3_IRQ_DOWN;
		cpr3_debug(ctrl, "limiting to floor=%d uV\n", aggr->floor_volt);
	}

	for (i = 0; i < ctrl->thread_count; i++)
		cpr3_print_result(&ctrl->thread[i]);

	cpr3_debug(ctrl, "%s: new_volt=%d uV, last_volt=%d uV\n",
		up ? "UP" : "DN", new_volt, last_volt);

	if (new_volt != last_volt) {
		rc = regulator_set_voltage(ctrl->vdd_regulator, new_volt,
						aggr->ceiling_volt);
		if (rc) {
			cpr3_err(ctrl, "failed to set vdd=%d uV, rc=%d\n",
				new_volt, rc);
			goto done;
		}
		cont = CPR3_CONT_CMD_ACK;
	}

	corner = &ctrl->thread[0].corner[ctrl->thread[0].current_corner];

	if (irq_en != aggr->irq_en) {
		aggr->irq_en = irq_en;
		cpr3_write(ctrl, CPR3_REG_IRQ_EN, irq_en);
		if (ctrl->thread_count == 1)
			corner->irq_en = irq_en;
	}

	aggr->last_volt = new_volt;
	for (i = 0; i < ctrl->thread_count; i++)
		cpr3_update_last_volt(&ctrl->thread[i], new_volt, up);

done:
	/* Clear interrupt status */
	cpr3_write(ctrl, CPR3_REG_IRQ_CLEAR, CPR3_IRQ_UP | CPR3_IRQ_DOWN);

	/* ACK or NACK the CPR controller */
	cpr3_write(ctrl, CPR3_REG_CONT_CMD, cont);

	mutex_unlock(&ctrl->lock);
	return IRQ_HANDLED;
}

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
 * cpr3_debug_closed_loop_enable_set() - debugfs callback used to change the
 *		value of the CPR controller cpr_allowed_sw flag which enables or
 *		disables closed-loop operation
 * @data:		Pointer to private data which is equal to the CPR
 *			controller pointer
 * @val:		New value for cpr_allowed_sw
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_closed_loop_enable_set(void *data, u64 val)
{
	struct cpr3_controller *ctrl = data;
	bool enable = !!val;
	int rc;

	mutex_lock(&ctrl->lock);

	if (ctrl->cpr_allowed_sw == enable)
		goto done;

	if (enable && !ctrl->cpr_allowed_hw) {
		cpr3_err(ctrl, "CPR closed-loop operation is not allowed\n");
		goto done;
	}

	ctrl->cpr_allowed_sw = enable;

	rc = cpr3_regulator_update_ctrl_state(ctrl);
	if (rc) {
		cpr3_err(ctrl, "could not change CPR enable state=%u, rc=%d\n",
			enable, rc);
		goto done;
	}

	cpr3_debug(ctrl, "closed-loop=%s\n", enable ? "enabled" : "disabled");

done:
	mutex_unlock(&ctrl->lock);
	return 0;
}

/**
 * cpr3_debug_closed_loop_enable_get() - debugfs callback used to retrieve
 *		the value of the CPR controller cpr_allowed_sw flag which
 *		indicates if closed-loop operation is enabled
 * @data:		Pointer to private data which is equal to the CPR
 *			controller pointer
 * @val:		Output parameter written with the value of
 *			cpr_allowed_sw
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_closed_loop_enable_get(void *data, u64 *val)
{
	struct cpr3_controller *ctrl = data;

	*val = ctrl->cpr_allowed_sw;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr3_debug_closed_loop_enable_fops,
			cpr3_debug_closed_loop_enable_get,
			cpr3_debug_closed_loop_enable_set,
			"%llu\n");

/**
 * cpr3_debug_hw_closed_loop_enable_set() - debugfs callback used to change the
 *		value of the CPR controller use_hw_closed_loop flag which
 *		switches between software closed-loop and hardware closed-loop
 *		operation
 * @data:		Pointer to private data which is equal to the CPR
 *			controller pointer
 * @val:		New value for use_hw_closed_loop
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_hw_closed_loop_enable_set(void *data, u64 val)
{
	struct cpr3_controller *ctrl = data;
	bool use_hw_closed_loop = !!val;
	bool cpr_enabled;
	int rc;

	mutex_lock(&ctrl->lock);

	if (ctrl->use_hw_closed_loop == use_hw_closed_loop)
		goto done;

	cpr3_ctrl_loop_disable(ctrl);

	ctrl->use_hw_closed_loop = use_hw_closed_loop;

	cpr_enabled = ctrl->cpr_enabled;

	/* Ensure that CPR clocks are enabled before writing to registers. */
	if (!cpr_enabled) {
		rc = cpr3_clock_enable(ctrl);
		if (rc) {
			cpr3_err(ctrl, "clock enable failed, rc=%d\n", rc);
			goto done;
		}
		ctrl->cpr_enabled = true;
	}

	if (ctrl->use_hw_closed_loop)
		cpr3_write(ctrl, CPR3_REG_IRQ_EN, 0);

	cpr3_write(ctrl, CPR3_REG_HW_CLOSED_LOOP,
		ctrl->use_hw_closed_loop
			? CPR3_HW_CLOSED_LOOP_ENABLE
			: CPR3_HW_CLOSED_LOOP_DISABLE);

	/* Turn off CPR clocks if they were off before this function call. */
	if (!cpr_enabled) {
		cpr3_clock_disable(ctrl);
		ctrl->cpr_enabled = false;
	}

	if (ctrl->use_hw_closed_loop) {
		rc = regulator_enable(ctrl->vdd_limit_regulator);
		if (rc) {
			cpr3_err(ctrl, "CPR limit regulator enable failed, rc=%d\n",
				rc);
			goto done;
		}
	} else {
		rc = regulator_disable(ctrl->vdd_limit_regulator);
		if (rc) {
			cpr3_err(ctrl, "CPR limit regulator disable failed, rc=%d\n",
				rc);
			goto done;
		}
	}

	rc = cpr3_regulator_update_ctrl_state(ctrl);
	if (rc) {
		cpr3_err(ctrl, "could not change CPR HW closed-loop enable state=%u, rc=%d\n",
			use_hw_closed_loop, rc);
		goto done;
	}

	cpr3_debug(ctrl, "closed-loop mode=%s\n",
		use_hw_closed_loop ? "HW" : "SW");

done:
	mutex_unlock(&ctrl->lock);
	return 0;
}

/**
 * cpr3_debug_hw_closed_loop_enable_get() - debugfs callback used to retrieve
 *		the value of the CPR controller use_hw_closed_loop flag which
 *		indicates if hardware closed-loop operation is being used in
 *		place of software closed-loop operation
 * @data:		Pointer to private data which is equal to the CPR
 *			controller pointer
 * @val:		Output parameter written with the value of
 *			use_hw_closed_loop
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_hw_closed_loop_enable_get(void *data, u64 *val)
{
	struct cpr3_controller *ctrl = data;

	*val = ctrl->use_hw_closed_loop;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr3_debug_hw_closed_loop_enable_fops,
			cpr3_debug_hw_closed_loop_enable_get,
			cpr3_debug_hw_closed_loop_enable_set,
			"%llu\n");

/**
 * cpr3_regulator_debugfs_ctrl_add() - add debugfs files to expose configuration
 *		data for the CPR controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: none
 */
static void cpr3_regulator_debugfs_ctrl_add(struct cpr3_controller *ctrl)
{
	struct dentry *temp;

	/* Add cpr3-regulator base directory if it isn't present already. */
	if (cpr3_debugfs_base == NULL) {
		cpr3_debugfs_base = debugfs_create_dir("cpr3-regulator", NULL);
		if (IS_ERR_OR_NULL(cpr3_debugfs_base)) {
			cpr3_err(ctrl, "cpr3-regulator debugfs base directory creation failed\n");
			cpr3_debugfs_base = NULL;
			return;
		}
	}

	ctrl->debugfs = debugfs_create_dir(ctrl->name, cpr3_debugfs_base);
	if (IS_ERR_OR_NULL(ctrl->debugfs)) {
		cpr3_err(ctrl, "cpr3-regulator controller debugfs directory creation failed\n");
		return;
	}

	temp = debugfs_create_file("cpr_closed_loop_enable", S_IRUSR | S_IWUSR,
					ctrl->debugfs, ctrl,
					&cpr3_debug_closed_loop_enable_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "cpr_closed_loop_enable debugfs directory creation failed\n");
		return;
	}

	if (ctrl->supports_hw_closed_loop) {
		temp = debugfs_create_file("use_hw_closed_loop",
					S_IRUSR | S_IWUSR, ctrl->debugfs, ctrl,
					&cpr3_debug_hw_closed_loop_enable_fops);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(ctrl, "use_hw_closed_loop debugfs directory creation failed\n");
			return;
		}
	}
}

/**
 * cpr3_regulator_debugfs_ctrl_remove() - remove debugfs files for the CPR
 *		controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Note, this function must be called after the controller has been removed from
 * cpr3_controller_list and while the cpr3_controller_list_mutex lock is held.
 *
 * Return: none
 */
static void cpr3_regulator_debugfs_ctrl_remove(struct cpr3_controller *ctrl)
{
	if (list_empty(&cpr3_controller_list)) {
		debugfs_remove_recursive(cpr3_debugfs_base);
		cpr3_debugfs_base = NULL;
	} else {
		debugfs_remove_recursive(ctrl->debugfs);
	}
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
	ctrl->aggr_corner.open_loop_volt = ctrl->aggr_corner.last_volt;

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
	int i, j;

	thread->current_corner = CPR3_REGULATOR_CORNER_INVALID;
	thread->last_closed_loop_corner = CPR3_REGULATOR_CORNER_INVALID;

	for (i = 0; i < thread->corner_count; i++) {
		thread->corner[i].last_volt = thread->corner[i].open_loop_volt;
		thread->corner[i].irq_en = CPR3_IRQ_UP | CPR3_IRQ_DOWN;

		thread->corner[i].ro_mask = 0;
		for (j = 0; j < CPR3_RO_COUNT; j++)
			if (thread->corner[i].target_quot[j] == 0)
				thread->corner[i].ro_mask |= BIT(j);
	}

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
	int rc;

	mutex_lock(&ctrl->lock);

	cpr3_ctrl_loop_disable(ctrl);

	rc = cpr3_closed_loop_disable(ctrl);
	if (rc)
		cpr3_err(ctrl, "could not disable CPR, rc=%d\n", rc);

	ctrl->cpr_suspended = true;

	mutex_unlock(&ctrl->lock);
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
	int rc;

	mutex_lock(&ctrl->lock);

	ctrl->cpr_suspended = false;

	rc = cpr3_regulator_update_ctrl_state(ctrl);
	if (rc)
		cpr3_err(ctrl, "could not enable CPR, rc=%d\n", rc);

	mutex_unlock(&ctrl->lock);
	return 0;
}

/**
 * cpr3_regulator_validate_controller() - verify the data passed in via the
 *		cpr3_controller data structure
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_validate_controller(struct cpr3_controller *ctrl)
{
	if (!ctrl->vdd_regulator) {
		cpr3_err(ctrl, "vdd regulator missing\n");
		return -EINVAL;
	} else if (!ctrl->core_clk) {
		cpr3_err(ctrl, "core clock missing\n");
		return -EINVAL;
	} else if (ctrl->sensor_count <= 0
		   || ctrl->sensor_count > CPR3_MAX_SENSOR_COUNT) {
		cpr3_err(ctrl, "invalid CPR sensor count=%d\n",
			ctrl->sensor_count);
		return -EINVAL;
	} else if (!ctrl->sensor_owner) {
		cpr3_err(ctrl, "CPR sensor ownership table missing\n");
		return -EINVAL;
	}

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

	rc = cpr3_regulator_validate_controller(ctrl);
	if (rc) {
		cpr3_err(ctrl, "controller validation failed, rc=%d\n", rc);
		return rc;
	}

	mutex_init(&ctrl->lock);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cpr_ctrl");
	if (!res || !res->start) {
		cpr3_err(ctrl, "CPR controller address is missing\n");
		return -ENXIO;
	}
	ctrl->cpr_ctrl_base = devm_ioremap(dev, res->start, resource_size(res));

	ctrl->irq = platform_get_irq(pdev, 0);
	if (ctrl->irq < 0) {
		cpr3_err(ctrl, "missing CPR interrupt\n");
		return -EINVAL;
	}

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
		cpr3_print_quots(&ctrl->thread[i]);
	}

	rc = cpr3_regulator_init_ctrl(ctrl);
	if (rc) {
		cpr3_err(ctrl, "CPR controller initialization failed, rc=%d\n",
			rc);
		return rc;
	}

	/* Register regulator devices for all threads. */
	for (i = 0; i < ctrl->thread_count; i++) {
		rc = cpr3_regulator_thread_register(&ctrl->thread[i]);
		if (rc) {
			cpr3_err(&ctrl->thread[i], "failed to register regulator, rc=%d\n",
				rc);
			goto free_regulators;
		}
	}

	rc = devm_request_threaded_irq(dev, ctrl->irq, NULL, cpr3_irq_handler,
		IRQF_ONESHOT | IRQF_TRIGGER_RISING, "cpr3", ctrl);
	if (rc) {
		cpr3_err(ctrl, "could not request IRQ %d, rc=%d\n",
			ctrl->irq, rc);
		goto free_regulators;
	}

	mutex_lock(&cpr3_controller_list_mutex);
	cpr3_regulator_debugfs_ctrl_add(ctrl);
	list_add(&ctrl->list, &cpr3_controller_list);
	mutex_unlock(&cpr3_controller_list_mutex);

	return 0;

free_regulators:
	for (i = 0; i < ctrl->thread_count; i++)
		if (!IS_ERR_OR_NULL(ctrl->thread[i].rdev))
			regulator_unregister(ctrl->thread[i].rdev);
	return rc;
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
	int i;

	mutex_lock(&cpr3_controller_list_mutex);
	list_del(&ctrl->list);
	cpr3_regulator_debugfs_ctrl_remove(ctrl);
	mutex_unlock(&cpr3_controller_list_mutex);

	cpr3_ctrl_loop_disable(ctrl);
	cpr3_closed_loop_disable(ctrl);

	if (ctrl->use_hw_closed_loop)
		regulator_disable(ctrl->vdd_limit_regulator);

	for (i = 0; i < ctrl->thread_count; i++)
		regulator_unregister(ctrl->thread[i].rdev);

	return 0;
}

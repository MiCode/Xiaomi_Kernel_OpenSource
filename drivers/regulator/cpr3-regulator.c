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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/kryo-regulator.h>

#include <soc/qcom/spm.h>

#include "cpr3-regulator.h"

#define CPR3_REGULATOR_CORNER_INVALID	(-1)
#define CPR3_RO_MASK			GENMASK(CPR3_RO_COUNT - 1, 0)

/* CPR3 registers */
#define CPR3_REG_CPR_CTL			0x4
#define CPR3_CPR_CTL_LOOP_EN_MASK		BIT(0)
#define CPR3_CPR_CTL_LOOP_ENABLE		BIT(0)
#define CPR3_CPR_CTL_LOOP_DISABLE		0
#define CPR3_CPR_CTL_IDLE_CLOCKS_MASK		GENMASK(5, 1)
#define CPR3_CPR_CTL_IDLE_CLOCKS_SHIFT		1
#define CPR3_CPR_CTL_COUNT_MODE_MASK		GENMASK(7, 6)
#define CPR3_CPR_CTL_COUNT_MODE_SHIFT		6
#define CPR3_CPR_CTL_COUNT_MODE_ALL_AT_ONCE_MIN	0
#define CPR3_CPR_CTL_COUNT_MODE_ALL_AT_ONCE_MAX	1
#define CPR3_CPR_CTL_COUNT_MODE_STAGGERED	2
#define CPR3_CPR_CTL_COUNT_MODE_ALL_AT_ONCE_AGE	3
#define CPR3_CPR_CTL_COUNT_REPEAT_MASK		GENMASK(31, 9)
#define CPR3_CPR_CTL_COUNT_REPEAT_SHIFT		9

#define CPR3_REG_CPR_STATUS			0x8
#define CPR3_CPR_STATUS_BUSY_MASK		BIT(0)
#define CPR3_CPR_STATUS_AGING_MEASUREMENT_MASK	BIT(1)

/*
 * This register is not present on controllers that support HW closed-loop
 * except CPR4 APSS controller.
 */
#define CPR3_REG_CPR_TIMER_AUTO_CONT		0xC

#define CPR3_REG_CPR_STEP_QUOT			0x14
#define CPR3_CPR_STEP_QUOT_MIN_MASK		GENMASK(5, 0)
#define CPR3_CPR_STEP_QUOT_MIN_SHIFT		0
#define CPR3_CPR_STEP_QUOT_MAX_MASK		GENMASK(11, 6)
#define CPR3_CPR_STEP_QUOT_MAX_SHIFT		6

#define CPR3_REG_GCNT(ro)			(0xA0 + 0x4 * (ro))

#define CPR3_REG_SENSOR_BYPASS_WRITE(sensor)	(0xE0 + 0x4 * ((sensor) / 32))
#define CPR3_REG_SENSOR_BYPASS_WRITE_BANK(bank)	(0xE0 + 0x4 * (bank))

#define CPR3_REG_SENSOR_MASK_WRITE(sensor)	(0x120 + 0x4 * ((sensor) / 32))
#define CPR3_REG_SENSOR_MASK_WRITE_BANK(bank)	(0x120 + 0x4 * (bank))
#define CPR3_REG_SENSOR_MASK_READ(sensor)	(0x140 + 0x4 * ((sensor) / 32))

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
#define CPR3_IRQ_MID			BIT(2)
#define CPR3_IRQ_DOWN			BIT(1)

#define CPR3_REG_TARGET_QUOT(thread, ro) \
					(0x840 + 0x440 * (thread) + 0x4 * (ro))

/* Registers found only on controllers that support HW closed-loop. */
#define CPR3_REG_PD_THROTTLE		0xE8
#define CPR3_PD_THROTTLE_DISABLE	0x0

#define CPR3_REG_HW_CLOSED_LOOP		0x3000
#define CPR3_HW_CLOSED_LOOP_ENABLE	0x0
#define CPR3_HW_CLOSED_LOOP_DISABLE	0x1

#define CPR3_REG_CPR_TIMER_MID_CONT	0x3004
#define CPR3_REG_CPR_TIMER_UP_DN_CONT	0x3008

#define CPR3_REG_LAST_MEASUREMENT		0x7F8
#define CPR3_LAST_MEASUREMENT_THREAD_DN_SHIFT	0
#define CPR3_LAST_MEASUREMENT_THREAD_UP_SHIFT	4
#define CPR3_LAST_MEASUREMENT_THREAD_DN(thread) \
		(BIT(thread) << CPR3_LAST_MEASUREMENT_THREAD_DN_SHIFT)
#define CPR3_LAST_MEASUREMENT_THREAD_UP(thread) \
		(BIT(thread) << CPR3_LAST_MEASUREMENT_THREAD_UP_SHIFT)
#define CPR3_LAST_MEASUREMENT_AGGR_DN		BIT(8)
#define CPR3_LAST_MEASUREMENT_AGGR_MID		BIT(9)
#define CPR3_LAST_MEASUREMENT_AGGR_UP		BIT(10)
#define CPR3_LAST_MEASUREMENT_VALID		BIT(11)
#define CPR3_LAST_MEASUREMENT_SAW_ERROR		BIT(12)
#define CPR3_LAST_MEASUREMENT_PD_BYPASS_MASK	GENMASK(23, 16)
#define CPR3_LAST_MEASUREMENT_PD_BYPASS_SHIFT	16

/* CPR4 controller specific registers and bit definitions */
#define CPR4_REG_SAW_ERROR_STEP_LIMIT		0x7A4
#define CPR4_SAW_ERROR_STEP_LIMIT_UP_MASK	GENMASK(4, 0)
#define CPR4_SAW_ERROR_STEP_LIMIT_UP_SHIFT	0
#define CPR4_SAW_ERROR_STEP_LIMIT_DN_MASK	GENMASK(9, 5)
#define CPR4_SAW_ERROR_STEP_LIMIT_DN_SHIFT	5

#define CPR4_REG_MARGIN_ADJ_CTL				0x7F8
#define CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_EN_MASK	BIT(4)
#define CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_ENABLE	BIT(4)
#define CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_DISABLE	0
#define CPR4_MARGIN_ADJ_CTL_PMIC_STEP_SIZE_MASK		GENMASK(16, 12)
#define CPR4_MARGIN_ADJ_CTL_PMIC_STEP_SIZE_SHIFT	12

/*
 * The amount of time to wait for the CPR controller to become idle when
 * performing an aging measurement.
 */
#define CPR3_AGING_MEASUREMENT_TIMEOUT_NS	5000000

/*
 * The number of individual aging measurements to perform which are then
 * averaged together in order to determine the final aging adjustment value.
 */
#define CPR3_AGING_MEASUREMENT_ITERATIONS	16

/*
 * Aging measurements for the aged and unaged ring oscillators take place a few
 * microseconds apart.  If the vdd-supply voltage fluctuates between the two
 * measurements, then the difference between them will be incorrect.  The
 * difference could end up too high or too low.  This constant defines the
 * number of lowest and highest measurements to ignore when averaging.
 */
#define CPR3_AGING_MEASUREMENT_FILTER		3

/*
 * The number of times to attempt the full aging measurement sequence before
 * declaring a measurement failure.
 */
#define CPR3_AGING_RETRY_COUNT			5

/*
 * The maximum time to wait in microseconds for a CPR register write to
 * complete.
 */
#define CPR3_REGISTER_WRITE_DELAY_US		200

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
 * cpr3_regulator_get_gcnt() - returns the GCNT register value corresponding
 *		to the clock rate and sensor time of the CPR3 controller
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: GCNT value
 */
static u32 cpr3_regulator_get_gcnt(struct cpr3_controller *ctrl)
{
	u64 temp;
	unsigned int remainder;
	u32 gcnt;

	temp = (u64)ctrl->cpr_clock_rate * (u64)ctrl->sensor_time;
	remainder = do_div(temp, 1000000000);
	if (remainder)
		temp++;
	/*
	 * GCNT == 0 corresponds to a single ref clock measurement interval so
	 * offset GCNT values by 1.
	 */
	gcnt = temp - 1;

	return gcnt;
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
 * cpr3_regulator_init_cpr4() - performs hardware initialization at the
 *		controller and thread level required for CPR4 operation.
 * @ctrl:		Pointer to the CPR3 controller
 *
 * CPR interface/bus clocks must be enabled before calling this function.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_init_cpr4(struct cpr3_controller *ctrl)
{
	u32 pmic_step_size = 1;

	if (ctrl->saw_use_unit_mV)
		pmic_step_size = ctrl->step_volt / 1000;
	cpr3_masked_write(ctrl, CPR4_REG_MARGIN_ADJ_CTL,
				CPR4_MARGIN_ADJ_CTL_PMIC_STEP_SIZE_MASK,
				(pmic_step_size
				<< CPR4_MARGIN_ADJ_CTL_PMIC_STEP_SIZE_SHIFT));

	cpr3_masked_write(ctrl, CPR4_REG_SAW_ERROR_STEP_LIMIT,
				CPR4_SAW_ERROR_STEP_LIMIT_DN_MASK,
				(ctrl->down_error_step_limit
				<< CPR4_SAW_ERROR_STEP_LIMIT_DN_SHIFT));

	cpr3_masked_write(ctrl, CPR4_REG_SAW_ERROR_STEP_LIMIT,
				CPR4_SAW_ERROR_STEP_LIMIT_UP_MASK,
				(ctrl->up_error_step_limit
				<< CPR4_SAW_ERROR_STEP_LIMIT_UP_SHIFT));
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
	int i, j, k, m, rc;
	u32 ro_used = 0;
	u32 gcnt, cont_dly, up_down_dly, val;
	u64 temp;
	char *mode;

	if (ctrl->core_clk) {
		rc = clk_set_rate(ctrl->core_clk, ctrl->cpr_clock_rate);
		if (rc) {
			cpr3_err(ctrl, "clk_set_rate(core_clk, %u) failed, rc=%d\n",
				ctrl->cpr_clock_rate, rc);
			return rc;
		}
	}

	rc = cpr3_clock_enable(ctrl);
	if (rc) {
		cpr3_err(ctrl, "clock enable failed, rc=%d\n", rc);
		return rc;
	}
	ctrl->cpr_enabled = true;

	/* Find all RO's used by any corner of any regulator. */
	for (i = 0; i < ctrl->thread_count; i++)
		for (j = 0; j < ctrl->thread[i].vreg_count; j++)
			for (k = 0; k < ctrl->thread[i].vreg[j].corner_count;
			     k++)
				for (m = 0; m < CPR3_RO_COUNT; m++)
					if (ctrl->thread[i].vreg[j].corner[k].
					    target_quot[m])
						ro_used |= BIT(m);

	/* Configure the GCNT of the RO's that will be used */
	gcnt = cpr3_regulator_get_gcnt(ctrl);
	for (i = 0; i < CPR3_RO_COUNT; i++)
		if (ro_used & BIT(i))
			cpr3_write(ctrl, CPR3_REG_GCNT(i), gcnt);

	/* Configure the loop delay time */
	temp = (u64)ctrl->cpr_clock_rate * (u64)ctrl->loop_time;
	do_div(temp, 1000000000);
	cont_dly = temp;
	if (ctrl->supports_hw_closed_loop
		&& ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3)
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_MID_CONT, cont_dly);
	else
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_AUTO_CONT, cont_dly);

	if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3) {
		temp = (u64)ctrl->cpr_clock_rate *
				(u64)ctrl->up_down_delay_time;
		do_div(temp, 1000000000);
		up_down_dly = temp;
		if (ctrl->supports_hw_closed_loop)
			cpr3_write(ctrl, CPR3_REG_CPR_TIMER_UP_DN_CONT,
				up_down_dly);
		cpr3_debug(ctrl, "up_down_dly=%u, up_down_delay_time=%u ns\n",
			up_down_dly, ctrl->up_down_delay_time);
	}

	cpr3_debug(ctrl, "cpr_clock_rate=%u HZ, sensor_time=%u ns, loop_time=%u ns, gcnt=%u, cont_dly=%u\n",
		ctrl->cpr_clock_rate, ctrl->sensor_time, ctrl->loop_time,
		gcnt, cont_dly);

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
		if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR4) {
			cpr3_masked_write(ctrl, CPR4_REG_MARGIN_ADJ_CTL,
				CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_EN_MASK,
				ctrl->use_hw_closed_loop
				? CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_ENABLE
				: CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_DISABLE);
		} else if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3) {
			cpr3_write(ctrl, CPR3_REG_HW_CLOSED_LOOP,
				ctrl->use_hw_closed_loop
				? CPR3_HW_CLOSED_LOOP_ENABLE
				: CPR3_HW_CLOSED_LOOP_DISABLE);

			cpr3_debug(ctrl, "PD_THROTTLE=0x%08X\n",
				ctrl->proc_clock_throttle);
		}
	}

	if (ctrl->use_hw_closed_loop) {
		rc = regulator_enable(ctrl->vdd_limit_regulator);
		if (rc) {
			cpr3_err(ctrl, "CPR limit regulator enable failed, rc=%d\n",
				rc);
			return rc;
		}

		rc = msm_spm_avs_enable_irq(0, MSM_SPM_AVS_IRQ_MAX);
		if (rc) {
			cpr3_err(ctrl, "could not enable max IRQ, rc=%d\n", rc);
			return rc;
		}
	}

	if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR4) {
		rc = cpr3_regulator_init_cpr4(ctrl);
		if (rc) {
			cpr3_err(ctrl, "CPR4-specific controller initialization failed, rc=%d\n",
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
 *
 * Return: none
 */
static void cpr3_regulator_set_target_quot(struct cpr3_thread *thread)
{
	u32 new_quot, last_quot;
	int i;

	if (thread->aggr_corner.ro_mask == CPR3_RO_MASK
	    && thread->last_closed_loop_aggr_corner.ro_mask == CPR3_RO_MASK) {
		/* Avoid writing target quotients since all RO's are masked. */
		return;
	} else if (thread->aggr_corner.ro_mask == CPR3_RO_MASK) {
		cpr3_write(thread->ctrl, CPR3_REG_RO_MASK(thread->thread_id),
			CPR3_RO_MASK);
		thread->last_closed_loop_aggr_corner.ro_mask = CPR3_RO_MASK;
		/*
		 * Only the RO_MASK register needs to be written since all
		 * RO's are masked.
		 */
		return;
	} else if (thread->aggr_corner.ro_mask
			!= thread->last_closed_loop_aggr_corner.ro_mask) {
		cpr3_write(thread->ctrl, CPR3_REG_RO_MASK(thread->thread_id),
			thread->aggr_corner.ro_mask);
	}

	for (i = 0; i < CPR3_RO_COUNT; i++) {
		new_quot = thread->aggr_corner.target_quot[i];
		last_quot = thread->last_closed_loop_aggr_corner.target_quot[i];
		if (new_quot != last_quot)
			cpr3_write(thread->ctrl,
				CPR3_REG_TARGET_QUOT(thread->thread_id, i),
				new_quot);
	}

	thread->last_closed_loop_aggr_corner = thread->aggr_corner;

	return;
}

/**
 * cpr3_update_vreg_closed_loop_volt() - update the last known settled
 *		closed loop voltage for a CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 * @vdd_volt:		Last known settled voltage in microvolts for the
 *			VDD supply
 * @reg_last_measurement: Value read from the LAST_MEASUREMENT register
 *
 * Return: none
 */
static void cpr3_update_vreg_closed_loop_volt(struct cpr3_regulator *vreg,
				int vdd_volt, u32 reg_last_measurement)
{
	bool step_dn, step_up, aggr_step_up, aggr_step_dn, aggr_step_mid;
	bool valid, pd_valid, saw_error;
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	struct cpr3_corner *corner;
	u32 id;

	if (vreg->last_closed_loop_corner == CPR3_REGULATOR_CORNER_INVALID)
		return;
	else
		corner = &vreg->corner[vreg->last_closed_loop_corner];

	if (vreg->thread->last_closed_loop_aggr_corner.ro_mask
	    == CPR3_RO_MASK  || !vreg->aggregated) {
		return;
	} else if (!ctrl->cpr_enabled || !ctrl->last_corner_was_closed_loop) {
		return;
	} else if (ctrl->thread_count == 1
		 && vdd_volt >= corner->floor_volt
		 && vdd_volt <= corner->ceiling_volt) {
		corner->last_volt = vdd_volt;
		cpr3_debug(vreg, "last_volt updated: last_volt[%d]=%d, ceiling_volt[%d]=%d, floor_volt[%d]=%d\n",
			   vreg->last_closed_loop_corner, corner->last_volt,
			   vreg->last_closed_loop_corner,
			   corner->ceiling_volt,
			   vreg->last_closed_loop_corner,
			   corner->floor_volt);
		return;
	} else if (!ctrl->supports_hw_closed_loop) {
		return;
	} else if (ctrl->ctrl_type != CPR_CTRL_TYPE_CPR3) {
		corner->last_volt = vdd_volt;
		cpr3_debug(vreg, "last_volt updated: last_volt[%d]=%d, ceiling_volt[%d]=%d, floor_volt[%d]=%d\n",
			   vreg->last_closed_loop_corner, corner->last_volt,
			   vreg->last_closed_loop_corner,
			   corner->ceiling_volt,
			   vreg->last_closed_loop_corner,
			   corner->floor_volt);
		return;
	}

	/* CPR clocks are on and HW closed loop is supported */
	valid = !!(reg_last_measurement & CPR3_LAST_MEASUREMENT_VALID);
	if (!valid) {
		cpr3_debug(vreg, "CPR_LAST_VALID_MEASUREMENT=0x%X valid bit not set\n",
			   reg_last_measurement);
		return;
	}

	id = vreg->thread->thread_id;

	step_dn
	       = !!(reg_last_measurement & CPR3_LAST_MEASUREMENT_THREAD_DN(id));
	step_up
	       = !!(reg_last_measurement & CPR3_LAST_MEASUREMENT_THREAD_UP(id));
	aggr_step_dn = !!(reg_last_measurement & CPR3_LAST_MEASUREMENT_AGGR_DN);
	aggr_step_mid
		= !!(reg_last_measurement & CPR3_LAST_MEASUREMENT_AGGR_MID);
	aggr_step_up = !!(reg_last_measurement & CPR3_LAST_MEASUREMENT_AGGR_UP);
	saw_error = !!(reg_last_measurement & CPR3_LAST_MEASUREMENT_SAW_ERROR);
	pd_valid
	     = !((((reg_last_measurement & CPR3_LAST_MEASUREMENT_PD_BYPASS_MASK)
		       >> CPR3_LAST_MEASUREMENT_PD_BYPASS_SHIFT)
		      & vreg->pd_bypass_mask) == vreg->pd_bypass_mask);

	if (!pd_valid) {
		cpr3_debug(vreg, "CPR_LAST_VALID_MEASUREMENT=0x%X, all power domains bypassed\n",
			   reg_last_measurement);
		return;
	} else if (step_dn && step_up) {
		cpr3_err(vreg, "both up and down status bits set, CPR_LAST_VALID_MEASUREMENT=0x%X\n",
			 reg_last_measurement);
		return;
	} else if (aggr_step_dn && step_dn && vdd_volt < corner->last_volt
		   && vdd_volt >= corner->floor_volt) {
		corner->last_volt = vdd_volt;
	} else if (aggr_step_up && step_up && vdd_volt > corner->last_volt
		   && vdd_volt <= corner->ceiling_volt) {
		corner->last_volt = vdd_volt;
	} else if (aggr_step_mid
		   && vdd_volt >= corner->floor_volt
		   && vdd_volt <= corner->ceiling_volt) {
		corner->last_volt = vdd_volt;
	} else if (saw_error && (vdd_volt == corner->ceiling_volt
				 || vdd_volt == corner->floor_volt)) {
		corner->last_volt = vdd_volt;
	} else {
		cpr3_debug(vreg, "last_volt not updated: last_volt[%d]=%d, ceiling_volt[%d]=%d, floor_volt[%d]=%d, vdd_volt=%d, CPR_LAST_VALID_MEASUREMENT=0x%X\n",
			   vreg->last_closed_loop_corner, corner->last_volt,
			   vreg->last_closed_loop_corner,
			   corner->ceiling_volt,
			   vreg->last_closed_loop_corner, corner->floor_volt,
			   vdd_volt, reg_last_measurement);
		return;
	}

	cpr3_debug(vreg, "last_volt updated: last_volt[%d]=%d, ceiling_volt[%d]=%d, floor_volt[%d]=%d, CPR_LAST_VALID_MEASUREMENT=0x%X\n",
		   vreg->last_closed_loop_corner, corner->last_volt,
		   vreg->last_closed_loop_corner, corner->ceiling_volt,
		   vreg->last_closed_loop_corner, corner->floor_volt,
		   reg_last_measurement);
}

/**
 * cpr3_regulator_config_ldo_retention() - configure per-regulator LDO retention
 *		mode
 * @vreg:		Pointer to the CPR3 regulator to configure
 * @ref_volt:		Reference voltage used to determine if LDO retention
 *			mode can be allowed. It corresponds either to the
 *			aggregated floor voltage or the next VDD supply setpoint
 *
 * This function determines if a CPR3 regulator's configuration satisfies safe
 * operating voltages for LDO retention and uses the regulator_allow_bypass()
 * interface on the LDO retention regulator to enable or disable such feature
 * accordingly.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_config_ldo_retention(struct cpr3_regulator *vreg,
					int ref_volt)
{
	struct regulator *ldo_ret_reg = vreg->ldo_ret_regulator;
	int retention_volt, rc;
	enum kryo_supply_mode mode;

	retention_volt = regulator_get_voltage(ldo_ret_reg);
	if (retention_volt < 0) {
		cpr3_err(vreg, "regulator_get_voltage(ldo_ret) failed, rc=%d\n",
			 retention_volt);
		return retention_volt;

	}

	mode = ref_volt >= retention_volt + vreg->ldo_min_headroom_volt
		? LDO_MODE : BHS_MODE;

	rc = regulator_allow_bypass(ldo_ret_reg, mode);
	if (rc)
		cpr3_err(vreg, "regulator_allow_bypass(ldo_ret) == %s failed, rc=%d\n",
			 mode ? "true" : "false", rc);

	return rc;
}

/**
 * cpr3_regulator_config_ldo_mem_acc() - configure the mem-acc regulator
 *		corner based upon a future LDO regulator voltage setpoint
 * @vreg:		Pointer to the CPR3 regulator
 * @new_volt:		New voltage in microvolts that the LDO regulator needs
 *			to end up at
 *
 * This function determines if a new LDO regulator set point will result
 * in crossing the voltage threshold that requires reconfiguration of
 * the mem-acc regulator associated with a CPR3 regulator and if so, performs
 * the correct sequence to select the correct mem-acc corner.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_config_ldo_mem_acc(struct cpr3_regulator *vreg,
					     int new_volt)
{
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	struct regulator *ldo_reg = vreg->ldo_regulator;
	struct regulator *mem_acc_reg = vreg->mem_acc_regulator;
	int mem_acc_volt = ctrl->mem_acc_threshold_volt;
	int last_volt, safe_volt, mem_acc_corn, rc;
	enum msm_apm_supply apm_mode;

	if (!mem_acc_reg || !mem_acc_volt || !ldo_reg)
		return 0;

	apm_mode = msm_apm_get_supply(ctrl->apm);
	if (apm_mode < 0) {
		cpr3_err(ctrl, "APM get supply failed, rc=%d\n",
			 apm_mode);
		return apm_mode;
	}

	last_volt = regulator_get_voltage(ldo_reg);
	if (last_volt < 0) {
		cpr3_err(vreg, "regulator_get_voltage(ldo) failed, rc=%d\n",
			 last_volt);
		return last_volt;
	}

	if (((last_volt < mem_acc_volt && mem_acc_volt <= new_volt)
	     || (last_volt >= mem_acc_volt && mem_acc_volt > new_volt))) {

		if (apm_mode == ctrl->apm_high_supply)
			safe_volt = min(vreg->ldo_max_volt, mem_acc_volt);
		else
			safe_volt = min(max(ctrl->system_supply_max_volt -
					    vreg->ldo_max_headroom_volt,
					    mem_acc_volt), vreg->ldo_max_volt);

		rc = regulator_set_voltage(ldo_reg, safe_volt,
					   max(new_volt, last_volt));
		if (rc) {
			cpr3_err(ctrl, "regulator_set_voltage(ldo) == %d failed, rc=%d\n",
				 mem_acc_volt, rc);
			return rc;
		}

		mem_acc_corn = new_volt < mem_acc_volt ?
			ctrl->mem_acc_corner_map[CPR3_MEM_ACC_LOW_CORNER] :
			ctrl->mem_acc_corner_map[CPR3_MEM_ACC_HIGH_CORNER];

		rc = regulator_set_voltage(mem_acc_reg, mem_acc_corn,
					   mem_acc_corn);
		if (rc) {
			cpr3_err(ctrl, "regulator_set_voltage(mem_acc) == %d failed, rc=%d\n",
				 0, rc);
			return rc;
		}
	}

	return 0;
}

/**
 * cpr3_regulator_set_bhs_mode() - configure the LDO regulator associated with
 *		a CPR3 regulator to BHS mode
 * @vreg:		Pointer to the CPR3 regulator
 * @vdd_volt:		Last known settled voltage in microvolts for the VDD
 *			supply
 * @vdd_ceiling_volt:	Last known aggregated ceiling voltage in microvolts for
 *			the VDD supply
 *
 * This function performs the necessary steps to switch an LDO regulator
 * to BHS mode (LDO bypassed mode).
 */
static int cpr3_regulator_set_bhs_mode(struct cpr3_regulator *vreg,
			       int vdd_volt, int vdd_ceiling_volt)
{
	struct regulator *ldo_reg = vreg->ldo_regulator;
	int bhs_volt, rc;

	bhs_volt = vdd_volt - vreg->ldo_min_headroom_volt;
	if (bhs_volt > vreg->ldo_max_volt) {
		cpr3_debug(vreg, "limited to LDO output of %d uV when switching to BHS mode\n",
			   vreg->ldo_max_volt);
		bhs_volt = vreg->ldo_max_volt;
	}

	rc = cpr3_regulator_config_ldo_mem_acc(vreg, bhs_volt);
	if (rc) {
		cpr3_err(vreg, "failed to configure mem-acc settings\n");
		return rc;
	}

	rc = regulator_set_voltage(ldo_reg, bhs_volt, min(vdd_ceiling_volt,
							  vreg->ldo_max_volt));
	if (rc) {
		cpr3_err(vreg, "regulator_set_voltage(ldo) == %d failed, rc=%d\n",
			 bhs_volt, rc);
		return rc;
	}

	rc = regulator_allow_bypass(ldo_reg, BHS_MODE);
	if (rc) {
		cpr3_err(vreg, "regulator_allow_bypass(ldo) == %s failed, rc=%d\n",
			 BHS_MODE ? "true" : "false", rc);
		return rc;
	}
	vreg->ldo_regulator_bypass = BHS_MODE;

	return rc;
}

/**
 * cpr3_regulator_ldo_apm_prepare() - configure LDO regulators associated
 *		with each CPR3 regulator of a CPR3 controller in preparation
 *		for an APM switch.
 * @ctrl:		Pointer to the CPR3 controller
 * @new_volt:		New voltage in microvolts that the VDD supply
 *			needs to end up at
 * @last_volt:		Last known voltage in microvolts for the VDD supply
 * @aggr_corner:	Pointer to the CPR3 corner which corresponds to the max
 *			corner aggregated from all CPR3 threads managed by the
 *			CPR3 controller
 *
 * This function ensures LDO regulator hardware requirements are met before
 * an APM switch is requested. The function must be called as the last step
 * before switching the APM mode.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_ldo_apm_prepare(struct cpr3_controller *ctrl,
				int new_volt, int last_volt,
				struct cpr3_corner *aggr_corner)
{
	struct cpr3_regulator *vreg;
	struct cpr3_corner *current_corner;
	enum msm_apm_supply apm_mode;
	int i, j, safe_volt, max_volt, ldo_volt, ref_volt, rc;

	apm_mode = msm_apm_get_supply(ctrl->apm);
	if (apm_mode < 0) {
		cpr3_err(ctrl, "APM get supply failed, rc=%d\n", apm_mode);
		return apm_mode;
	}

	if (apm_mode == ctrl->apm_low_supply ||
	    new_volt >= ctrl->apm_threshold_volt)
		return 0;

	/*
	 * Guarantee LDO maximum headroom is not violated when the APM is
	 * switched to the system-supply source.
	 */
	for (i = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			vreg = &ctrl->thread[i].vreg[j];

			if (!vreg->vreg_enabled || vreg->current_corner
			    == CPR3_REGULATOR_CORNER_INVALID)
				continue;

			if (!vreg->ldo_regulator || !vreg->ldo_mode_allowed ||
			    vreg->ldo_regulator_bypass == BHS_MODE)
				continue;

			/*
			 * If the new VDD configuration does not satisfy
			 * requirements for LDO usage, switch the regulator
			 * to BHS mode. By doing so, the LDO maximum headroom
			 * does not need to be enforced.
			 */
			current_corner = &vreg->corner[vreg->current_corner];
			ldo_volt = current_corner->open_loop_volt
				- vreg->ldo_adjust_volt;
			ref_volt = ctrl->use_hw_closed_loop ?
				aggr_corner->floor_volt :
				new_volt;

			if (ref_volt < ldo_volt + vreg->ldo_min_headroom_volt
			    || ldo_volt < ctrl->system_supply_max_volt -
			    vreg->ldo_max_headroom_volt ||
			    ldo_volt > vreg->ldo_max_volt) {
				rc = cpr3_regulator_set_bhs_mode(vreg,
					 last_volt, aggr_corner->ceiling_volt);
				if (rc)
					return rc;
				/*
				 * Do not enforce LDO maximum headroom since the
				 * regulator is now configured to BHS mode.
				 */
				continue;
			}

			safe_volt = min(max(ldo_volt,
					    ctrl->system_supply_max_volt
					    - vreg->ldo_max_headroom_volt),
					vreg->ldo_max_volt);
			max_volt = min(ctrl->system_supply_max_volt,
				       vreg->ldo_max_volt);

			rc = regulator_set_voltage(vreg->ldo_regulator,
						   safe_volt, max_volt);
			if (rc) {
				cpr3_err(vreg, "regulator_set_voltage(ldo) == %d failed, rc=%d\n",
					 safe_volt, rc);
				return rc;
			}
		}
	}

	return 0;
}

/**
 * cpr3_regulator_config_vreg_ldo() - configure the voltage and bypass state for
 *		the LDO regulator associated with a single CPR3 regulator.
 *
 * @vreg:		Pointer to the CPR3 regulator
 * @vdd_floor_volt:	Last known aggregated floor voltage in microvolts for
 *			the VDD supply
 * @vdd_ceiling_volt:	Last known aggregated ceiling voltage in microvolts for
 *			the VDD supply
 * @new_volt:		New voltage in microvolts that VDD supply needs to
 *			end up at
 * @last_volt:		Last known voltage in microvolts for the VDD supply
 *
 * This function performs all relevant LDO or BHS configurations if an LDO
 * regulator is specified.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_config_vreg_ldo(struct cpr3_regulator *vreg,
			  int vdd_floor_volt, int vdd_ceiling_volt,
			  int new_volt, int last_volt)
{
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	struct regulator *ldo_reg = vreg->ldo_regulator;
	struct cpr3_corner *current_corner;
	enum msm_apm_supply apm_mode;
	int rc, ldo_volt, final_ldo_volt, bhs_volt, max_volt, safe_volt;
	int ref_volt;

	ref_volt = ctrl->use_hw_closed_loop ? vdd_floor_volt :
		new_volt;

	rc = cpr3_regulator_config_ldo_retention(vreg, ref_volt);
	if (rc)
		return rc;

	if (!vreg->vreg_enabled || vreg->current_corner
	    == CPR3_REGULATOR_CORNER_INVALID)
		return 0;

	current_corner = &vreg->corner[vreg->current_corner];
	ldo_volt = current_corner->open_loop_volt
		- vreg->ldo_adjust_volt;
	bhs_volt = last_volt - vreg->ldo_min_headroom_volt;
	max_volt = min(vdd_ceiling_volt, vreg->ldo_max_volt);

	if (ref_volt >= ldo_volt + vreg->ldo_min_headroom_volt &&
	    ldo_volt >= ctrl->system_supply_max_volt -
	    vreg->ldo_max_headroom_volt &&
	    bhs_volt >= ctrl->system_supply_max_volt -
	    vreg->ldo_max_headroom_volt &&
	    ldo_volt <= vreg->ldo_max_volt) {
		/* LDO minimum and maximum headrooms satisfied */
		apm_mode = msm_apm_get_supply(ctrl->apm);
		if (apm_mode < 0) {
			cpr3_err(ctrl, "APM get supply failed, rc=%d\n",
				 apm_mode);
			return apm_mode;
		}

		if (vreg->ldo_regulator_bypass == BHS_MODE) {
			/*
			 * BHS to LDO transition. Configure LDO output
			 * to min(max LDO output, VDD - LDO headroom)
			 * voltage if APM is on high supply source or
			 * min(max(system-supply ceiling - LDO max headroom,
			 * VDD - LDO headroom), max LDO output) if
			 * APM is on low supply source, then switch
			 * regulator mode.
			 */
			if (apm_mode == ctrl->apm_high_supply)
				safe_volt = min(vreg->ldo_max_volt, bhs_volt);
			else
				safe_volt =
					min(max(ctrl->system_supply_max_volt -
						vreg->ldo_max_headroom_volt,
						bhs_volt),
					    vreg->ldo_max_volt);

			rc = cpr3_regulator_config_ldo_mem_acc(vreg,
							       safe_volt);
			if (rc) {
				cpr3_err(vreg, "failed to configure mem-acc settings\n");
				return rc;
			}

			rc = regulator_set_voltage(ldo_reg, safe_volt,
						   max_volt);
			if (rc) {
				cpr3_err(vreg, "regulator_set_voltage(ldo) == %d failed, rc=%d\n",
					 safe_volt, rc);
				return rc;
			}

			rc = regulator_allow_bypass(ldo_reg, LDO_MODE);
			if (rc) {
				cpr3_err(vreg, "regulator_allow_bypass(ldo) == %s failed, rc=%d\n",
					 LDO_MODE ? "true" : "false", rc);
				return rc;
			}
			vreg->ldo_regulator_bypass = LDO_MODE;
		}

		/* Configure final LDO output voltage */
		if (apm_mode == ctrl->apm_high_supply)
			final_ldo_volt = max(ldo_volt,
					     vdd_ceiling_volt -
					     vreg->ldo_max_headroom_volt);
		else
			final_ldo_volt = ldo_volt;

		rc = cpr3_regulator_config_ldo_mem_acc(vreg,
						       final_ldo_volt);
		if (rc) {
			cpr3_err(vreg, "failed to configure mem-acc settings\n");
			return rc;
		}

		rc = regulator_set_voltage(ldo_reg, final_ldo_volt, max_volt);
		if (rc) {
			cpr3_err(vreg, "regulator_set_voltage(ldo) == %d failed, rc=%d\n",
				 final_ldo_volt, rc);
			return rc;
		}
	} else {
		if (vreg->ldo_regulator_bypass == LDO_MODE) {
			/* LDO to BHS transition */
			rc = cpr3_regulator_set_bhs_mode(vreg, last_volt,
							 vdd_ceiling_volt);
			if (rc)
				return rc;
		}
	}

	return 0;
}

/**
 * cpr3_regulator_config_ldo() - configure the voltage and bypass state for the
 *		LDO regulator associated with each CPR3 regulator of a CPR3
 *		controller
 * @ctrl:		Pointer to the CPR3 controller
 * @vdd_floor_volt:	Last known aggregated floor voltage in microvolts for
 *			the VDD supply
 * @vdd_ceiling_volt:	Last known aggregated ceiling voltage in microvolts for
 *			the VDD supply
 * @new_volt:		New voltage in microvolts that VDD supply needs to
 *			end up at
 * @last_volt:		Last known voltage in microvolts for the VDD supply
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_config_ldo(struct cpr3_controller *ctrl,
				int vdd_floor_volt, int vdd_ceiling_volt,
				int new_volt, int last_volt)
{
	struct cpr3_regulator *vreg;
	int i, j, rc;

	for (i = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			vreg = &ctrl->thread[i].vreg[j];

			if (!vreg->ldo_regulator || !vreg->ldo_mode_allowed)
				continue;

			rc = cpr3_regulator_config_vreg_ldo(vreg,
					vdd_floor_volt, vdd_ceiling_volt,
					new_volt, last_volt);
			if (rc)
				return rc;
		}
	}

	return 0;
}

/**
 * cpr3_regulator_mem_acc_bhs_used() - determines if mem-acc regulators powered
 *		through a BHS are associated with the CPR3 controller or any of
 *		the CPR3 regulators it controls.
 * @ctrl:		Pointer to the CPR3 controller
 *
 * This function determines if the CPR3 controller or any of its CPR3 regulators
 * need to manage mem-acc regulators that are currently powered through a BHS
 * and whose corner selection is based upon a particular voltage threshold.
 *
 * Return: true or false
 */
static bool cpr3_regulator_mem_acc_bhs_used(struct cpr3_controller *ctrl)
{
	struct cpr3_regulator *vreg;
	int i, j;

	if (!ctrl->mem_acc_threshold_volt)
		return false;

	if (ctrl->mem_acc_regulator)
		return true;

	for (i = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			vreg = &ctrl->thread[i].vreg[j];

			if (vreg->mem_acc_regulator &&
			    (!vreg->ldo_regulator ||
			     vreg->ldo_regulator_bypass
			     == BHS_MODE))
				return true;
		}
	}

	return false;
}

/**
 * cpr3_regulator_config_bhs_mem_acc() - configure the mem-acc regulator
 *		settings for hardware blocks currently powered through the BHS.
 * @ctrl:		Pointer to the CPR3 controller
 * @new_volt:		New voltage in microvolts that VDD supply needs to
 *			end up at
 * @last_volt:		Pointer to the last known voltage in microvolts for the
 *			VDD supply
 * @aggr_corner:	Pointer to the CPR3 corner which corresponds to the max
 *			corner aggregated from all CPR3 threads managed by the
 *			CPR3 controller
 *
 * This function programs the mem-acc regulator corners for CPR3 regulators
 * whose LDO regulators are in bypassed state. The function also handles
 * CPR3 controllers which utilize mem-acc regulators that operate independently
 * from the LDO hardware and that must be programmed when the VDD supply
 * crosses a particular voltage threshold.
 *
 * Return: 0 on success, errno on failure. If the VDD supply voltage is
 * modified, last_volt is updated to reflect the new voltage setpoint.
 */
static int cpr3_regulator_config_bhs_mem_acc(struct cpr3_controller *ctrl,
				     int new_volt, int *last_volt,
				     struct cpr3_corner *aggr_corner)
{
	struct cpr3_regulator *vreg;
	int i, j, rc, mem_acc_corn, safe_volt;
	int mem_acc_volt = ctrl->mem_acc_threshold_volt;
	int ref_volt;

	if (!cpr3_regulator_mem_acc_bhs_used(ctrl))
		return 0;

	ref_volt = ctrl->use_hw_closed_loop ? aggr_corner->floor_volt :
		new_volt;

	if (((*last_volt < mem_acc_volt && mem_acc_volt <= ref_volt) ||
	     (*last_volt >= mem_acc_volt && mem_acc_volt > ref_volt))) {
		if (ref_volt < *last_volt)
			safe_volt = max(mem_acc_volt, aggr_corner->last_volt);
		else
			safe_volt = max(mem_acc_volt, *last_volt);

		rc = regulator_set_voltage(ctrl->vdd_regulator, safe_volt,
					   new_volt < *last_volt ?
					   ctrl->aggr_corner.ceiling_volt :
					   new_volt);
		if (rc) {
			cpr3_err(ctrl, "regulator_set_voltage(vdd) == %d failed, rc=%d\n",
				 safe_volt, rc);
			return rc;
		}

		*last_volt = safe_volt;

		mem_acc_corn = ref_volt < mem_acc_volt ?
			ctrl->mem_acc_corner_map[CPR3_MEM_ACC_LOW_CORNER] :
			ctrl->mem_acc_corner_map[CPR3_MEM_ACC_HIGH_CORNER];

		if (ctrl->mem_acc_regulator) {
			rc = regulator_set_voltage(ctrl->mem_acc_regulator,
						   mem_acc_corn, mem_acc_corn);
			if (rc) {
				cpr3_err(ctrl, "regulator_set_voltage(mem_acc) == %d failed, rc=%d\n",
					 mem_acc_corn, rc);
				return rc;
			}
		}

		for (i = 0; i < ctrl->thread_count; i++) {
			for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
				vreg = &ctrl->thread[i].vreg[j];

				if (!vreg->mem_acc_regulator ||
				    (vreg->ldo_regulator &&
				     vreg->ldo_regulator_bypass
				     == LDO_MODE))
					continue;

				rc = regulator_set_voltage(
					vreg->mem_acc_regulator, mem_acc_corn,
					mem_acc_corn);
				if (rc) {
					cpr3_err(vreg, "regulator_set_voltage(mem_acc) == %d failed, rc=%d\n",
						 mem_acc_corn, rc);
					return rc;
				}
			}
		}
	}

	return 0;
}

/**
 * cpr3_regulator_switch_apm_mode() - switch the mode of the APM controller
 *		associated with a given CPR3 controller
 * @ctrl:		Pointer to the CPR3 controller
 * @new_volt:		New voltage in microvolts that VDD supply needs to
 *			end up at
 * @last_volt:		Pointer to the last known voltage in microvolts for the
 *			VDD supply
 * @aggr_corner:	Pointer to the CPR3 corner which corresponds to the max
 *			corner aggregated from all CPR3 threads managed by the
 *			CPR3 controller
 *
 * This function requests a switch of the APM mode while guaranteeing
 * any LDO regulator hardware requirements are satisfied. The function must
 * be called once it is known a new VDD supply setpoint crosses the APM
 * voltage threshold.
 *
 * Return: 0 on success, errno on failure. If the VDD supply voltage is
 * modified, last_volt is updated to reflect the new voltage setpoint.
 */
static int cpr3_regulator_switch_apm_mode(struct cpr3_controller *ctrl,
					  int new_volt, int *last_volt,
					  struct cpr3_corner *aggr_corner)
{
	struct regulator *vdd = ctrl->vdd_regulator;
	int apm_volt = ctrl->apm_threshold_volt;
	int orig_last_volt = *last_volt;
	int rc;

	rc = regulator_set_voltage(vdd, apm_volt, apm_volt);
	if (rc) {
		cpr3_err(ctrl, "regulator_set_voltage(vdd) == %d failed, rc=%d\n",
			 apm_volt, rc);
		return rc;
	}

	*last_volt = apm_volt;

	rc = cpr3_regulator_ldo_apm_prepare(ctrl, new_volt, *last_volt,
					    aggr_corner);
	if (rc) {
		cpr3_err(ctrl, "unable to prepare LDO state for APM switch, rc=%d\n",
			 rc);
		return rc;
	}

	rc = msm_apm_set_supply(ctrl->apm, new_volt >= apm_volt
				? ctrl->apm_high_supply : ctrl->apm_low_supply);
	if (rc) {
		cpr3_err(ctrl, "APM switch failed, rc=%d\n", rc);
		/* Roll back the voltage. */
		regulator_set_voltage(vdd, orig_last_volt, INT_MAX);
		*last_volt = orig_last_volt;
		return rc;
	}
	return 0;
}

/**
 * cpr3_regulator_config_voltage_crossings() - configure APM and mem-acc
 *		settings depending upon a new VDD supply setpoint
 *
 * @ctrl:		Pointer to the CPR3 controller
 * @new_volt:		New voltage in microvolts that VDD supply needs to
 *			end up at
 * @last_volt:		Pointer to the last known voltage in microvolts for the
 *			VDD supply
 * @aggr_corner:	Pointer to the CPR3 corner which corresponds to the max
 *			corner aggregated from all CPR3 threads managed by the
 *			CPR3 controller
 *
 * This function handles the APM and mem-acc regulator reconfiguration if
 * the new VDD supply voltage will result in crossing their respective voltage
 * thresholds.
 *
 * Return: 0 on success, errno on failure. If the VDD supply voltage is
 * modified, last_volt is updated to reflect the new voltage setpoint.
 */
static int cpr3_regulator_config_voltage_crossings(struct cpr3_controller *ctrl,
				   int new_volt, int *last_volt,
				   struct cpr3_corner *aggr_corner)
{
	bool apm_crossing = false, mem_acc_crossing = false;
	bool mem_acc_bhs_used;
	int apm_volt = ctrl->apm_threshold_volt;
	int mem_acc_volt = ctrl->mem_acc_threshold_volt;
	int ref_volt, rc;

	if (ctrl->apm && apm_volt > 0
	    && ((*last_volt < apm_volt && apm_volt <= new_volt)
		|| (*last_volt >= apm_volt && apm_volt > new_volt)))
		apm_crossing = true;

	mem_acc_bhs_used = cpr3_regulator_mem_acc_bhs_used(ctrl);

	ref_volt = ctrl->use_hw_closed_loop ? aggr_corner->floor_volt :
		new_volt;

	if (mem_acc_bhs_used &&
	    (((*last_volt < mem_acc_volt && mem_acc_volt <= ref_volt) ||
	      (*last_volt >= mem_acc_volt && mem_acc_volt > ref_volt))))
		mem_acc_crossing = true;

	if (apm_crossing && mem_acc_crossing) {
		if ((new_volt < *last_volt && apm_volt >= mem_acc_volt) ||
		    (new_volt >= *last_volt && apm_volt < mem_acc_volt)) {
			rc = cpr3_regulator_switch_apm_mode(ctrl, new_volt,
							    last_volt,
							    aggr_corner);
			if (rc) {
				cpr3_err(ctrl, "unable to switch APM mode\n");
				return rc;
			}

			rc = cpr3_regulator_config_bhs_mem_acc(ctrl, new_volt,
						       last_volt, aggr_corner);
			if (rc) {
				cpr3_err(ctrl, "unable to configure BHS mem-acc settings\n");
				return rc;
			}
		} else {
			rc = cpr3_regulator_config_bhs_mem_acc(ctrl, new_volt,
						       last_volt, aggr_corner);
			if (rc) {
				cpr3_err(ctrl, "unable to configure BHS mem-acc settings\n");
				return rc;
			}

			rc = cpr3_regulator_switch_apm_mode(ctrl, new_volt,
							    last_volt,
							    aggr_corner);
			if (rc) {
				cpr3_err(ctrl, "unable to switch APM mode\n");
				return rc;
			}
		}
	} else if (apm_crossing) {
		rc = cpr3_regulator_switch_apm_mode(ctrl, new_volt, last_volt,
						    aggr_corner);
		if (rc) {
			cpr3_err(ctrl, "unable to switch APM mode\n");
			return rc;
		}
	} else if (mem_acc_crossing) {
		rc = cpr3_regulator_config_bhs_mem_acc(ctrl, new_volt,
						       last_volt, aggr_corner);
		if (rc) {
			cpr3_err(ctrl, "unable to configure BHS mem-acc settings\n");
			return rc;
		}
	}

	return 0;
}

/**
 * cpr3_regulator_config_mem_acc() - configure the corner of the mem-acc
 *			regulator associated with the CPR3 controller
 * @ctrl:		Pointer to the CPR3 controller
 * @aggr_corner:	Pointer to the CPR3 corner which corresponds to the max
 *			corner aggregated from all CPR3 threads managed by the
 *			CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_config_mem_acc(struct cpr3_controller *ctrl,
					 struct cpr3_corner *aggr_corner)
{
	int rc;

	if (ctrl->mem_acc_regulator && aggr_corner->mem_acc_volt) {
		rc = regulator_set_voltage(ctrl->mem_acc_regulator,
					   aggr_corner->mem_acc_volt,
					   aggr_corner->mem_acc_volt);
		if (rc) {
			cpr3_err(ctrl, "regulator_set_voltage(mem_acc) == %d failed, rc=%d\n",
				 aggr_corner->mem_acc_volt, rc);
			return rc;
		}
	}

	return 0;
}

/**
 * cpr3_regulator_scale_vdd_voltage() - scale the CPR controlled VDD supply
 *		voltage to the new level while satisfying any other hardware
 *		requirements
 * @ctrl:		Pointer to the CPR3 controller
 * @new_volt:		New voltage in microvolts that VDD supply needs to end
 *			up at
 * @last_volt:		Last known voltage in microvolts for the VDD supply
 * @aggr_corner:	Pointer to the CPR3 corner which corresponds to the max
 *			corner aggregated from all CPR3 threads managed by the
 *			CPR3 controller
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
				int new_volt, int last_volt,
				struct cpr3_corner *aggr_corner)
{
	struct regulator *vdd = ctrl->vdd_regulator;
	int rc;

	if (new_volt < last_volt) {
		/* Decreasing VDD voltage */
		rc = cpr3_regulator_config_ldo(ctrl, aggr_corner->floor_volt,
					       ctrl->aggr_corner.ceiling_volt,
					       new_volt, last_volt);
		if (rc) {
			cpr3_err(ctrl, "unable to configure LDO state, rc=%d\n",
				 rc);
			return rc;
		}

		rc = cpr3_regulator_config_mem_acc(ctrl, aggr_corner);
		if (rc)
			return rc;

	} else {
		/* Increasing VDD voltage */
		if (ctrl->system_regulator) {
			rc = regulator_set_voltage(ctrl->system_regulator,
				aggr_corner->system_volt, INT_MAX);
			if (rc) {
				cpr3_err(ctrl, "regulator_set_voltage(system) == %d failed, rc=%d\n",
					aggr_corner->system_volt, rc);
				return rc;
			}
		}
	}

	rc = cpr3_regulator_config_voltage_crossings(ctrl, new_volt, &last_volt,
						     aggr_corner);
	if (rc) {
		cpr3_err(ctrl, "unable to handle voltage threshold crossing configurations, rc=%d\n",
			 rc);
		return rc;
	}

	/*
	 * Subtract a small amount from the min_uV parameter so that the
	 * set voltage request is not dropped by the framework due to being
	 * duplicate.  This is needed in order to switch from hardware
	 * closed-loop to open-loop successfully.
	 */
	rc = regulator_set_voltage(vdd, new_volt - (ctrl->cpr_enabled ? 0 : 1),
				   aggr_corner->ceiling_volt);
	if (rc) {
		cpr3_err(ctrl, "regulator_set_voltage(vdd) == %d failed, rc=%d\n",
			new_volt, rc);
		return rc;
	}

	if (new_volt >= last_volt) {
		/* Increasing VDD voltage */
		rc = cpr3_regulator_config_ldo(ctrl, aggr_corner->floor_volt,
					       aggr_corner->ceiling_volt,
					       new_volt, new_volt);
		if (rc) {
			cpr3_err(ctrl, "unable to configure LDO state, rc=%d\n",
				 rc);
			return rc;
		}

		rc = cpr3_regulator_config_mem_acc(ctrl, aggr_corner);
		if (rc)
			return rc;
	} else {
		/* Decreasing VDD voltage */
		if (ctrl->system_regulator) {
			rc = regulator_set_voltage(ctrl->system_regulator,
				aggr_corner->system_volt, INT_MAX);
			if (rc) {
				cpr3_err(ctrl, "regulator_set_voltage(system) == %d failed, rc=%d\n",
					aggr_corner->system_volt, rc);
				return rc;
			}
		}
	}

	return 0;
}

/**
 * cpr3_regulator_get_dynamic_floor_volt() - returns the current dynamic floor
 *		voltage based upon static configurations and the state of all
 *		power domains during the last CPR measurement
 * @ctrl:		Pointer to the CPR3 controller
 * @reg_last_measurement: Value read from the LAST_MEASUREMENT register
 *
 * When using HW closed-loop, the dynamic floor voltage is always returned
 * regardless of the current state of the power domains.
 *
 * Return: dynamic floor voltage in microvolts or 0 if dynamic floor is not
 *         currently required
 */
static int cpr3_regulator_get_dynamic_floor_volt(struct cpr3_controller *ctrl,
		u32 reg_last_measurement)
{
	int dynamic_floor_volt = 0;
	struct cpr3_regulator *vreg;
	bool valid, pd_valid;
	u32 bypass_bits;
	int i, j;

	if (!ctrl->supports_hw_closed_loop)
		return 0;

	if (likely(!ctrl->use_hw_closed_loop)) {
		valid = !!(reg_last_measurement & CPR3_LAST_MEASUREMENT_VALID);
		bypass_bits
		 = (reg_last_measurement & CPR3_LAST_MEASUREMENT_PD_BYPASS_MASK)
			>> CPR3_LAST_MEASUREMENT_PD_BYPASS_SHIFT;
	} else {
		/*
		 * Ensure that the dynamic floor voltage is always used for
		 * HW closed-loop since the conditions below cannot be evaluated
		 * after each CPR measurement.
		 */
		valid = false;
		bypass_bits = 0;
	}

	for (i = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			vreg = &ctrl->thread[i].vreg[j];

			if (!vreg->uses_dynamic_floor)
				continue;

			pd_valid = !((bypass_bits & vreg->pd_bypass_mask)
					== vreg->pd_bypass_mask);

			if (!valid || !pd_valid)
				dynamic_floor_volt = max(dynamic_floor_volt,
					vreg->corner[
					 vreg->dynamic_floor_corner].last_volt);
		}
	}

	return dynamic_floor_volt;
}

/**
 * cpr3_regulator_aggregate_corners() - aggregate two corners together
 * @aggr_corner:		Pointer to accumulated aggregated corner which
 *				is both an input and an output
 * @corner:			Pointer to the corner to be aggregated with
 *				aggr_corner
 * @aggr_quot:			Flag indicating that target quotients should be
 *				aggregated as well.
 *
 * Return: none
 */
static void cpr3_regulator_aggregate_corners(struct cpr3_corner *aggr_corner,
			const struct cpr3_corner *corner, bool aggr_quot)
{
	int i;

	aggr_corner->ceiling_volt
		= max(aggr_corner->ceiling_volt, corner->ceiling_volt);
	aggr_corner->floor_volt
		= max(aggr_corner->floor_volt, corner->floor_volt);
	aggr_corner->last_volt
		= max(aggr_corner->last_volt, corner->last_volt);
	aggr_corner->open_loop_volt
		= max(aggr_corner->open_loop_volt, corner->open_loop_volt);
	aggr_corner->system_volt
		= max(aggr_corner->system_volt, corner->system_volt);
	aggr_corner->mem_acc_volt
		= max(aggr_corner->mem_acc_volt, corner->mem_acc_volt);
	aggr_corner->irq_en |= corner->irq_en;

	if (aggr_quot) {
		aggr_corner->ro_mask &= corner->ro_mask;

		for (i = 0; i < CPR3_RO_COUNT; i++)
			aggr_corner->target_quot[i]
				= max(aggr_corner->target_quot[i],
				      corner->target_quot[i]);
	}
}

/**
 * cpr3_regulator_update_ctrl_state() - update the state of the CPR controller
 *		to reflect the corners used by all CPR3 regulators as well as
 *		the CPR operating mode
 * @ctrl:		Pointer to the CPR3 controller
 *
 * This function aggregates the CPR parameters for all CPR3 regulators
 * associated with the VDD supply.  Upon success, it sets the aggregated last
 * known good voltage.
 *
 * The VDD supply voltage will not be physically configured unless this
 * condition is met by at least one of the regulators of the controller:
 * regulator->vreg_enabled == true &&
 * regulator->current_corner != CPR3_REGULATOR_CORNER_INVALID
 *
 * CPR registers for the controller and each thread are updated as long as
 * ctrl->cpr_enabled == true.
 *
 * Note, CPR3 controller lock must be held by the caller.
 *
 * Return: 0 on success, errno on failure
 */
static int _cpr3_regulator_update_ctrl_state(struct cpr3_controller *ctrl)
{
	struct cpr3_corner aggr_corner = {};
	struct cpr3_thread *thread;
	struct cpr3_regulator *vreg;
	bool valid = false;
	bool thread_valid;
	int i, j, rc, new_volt, vdd_volt, dynamic_floor_volt;
	u32 reg_last_measurement = 0;

	cpr3_ctrl_loop_disable(ctrl);

	vdd_volt = regulator_get_voltage(ctrl->vdd_regulator);
	if (vdd_volt < 0) {
		cpr3_err(ctrl, "regulator_get_voltage(vdd) failed, rc=%d\n",
			 vdd_volt);
		return vdd_volt;
	}

	if (ctrl->cpr_enabled && ctrl->use_hw_closed_loop)
		reg_last_measurement
			= cpr3_read(ctrl, CPR3_REG_LAST_MEASUREMENT);

	/* Aggregate the requests of all threads */
	for (i = 0; i < ctrl->thread_count; i++) {
		thread = &ctrl->thread[i];
		thread_valid = false;
		memset(&thread->aggr_corner, 0, sizeof(thread->aggr_corner));
		thread->aggr_corner.ro_mask = CPR3_RO_MASK;

		for (j = 0; j < thread->vreg_count; j++) {
			vreg = &thread->vreg[j];

			if (ctrl->cpr_enabled && ctrl->use_hw_closed_loop)
				cpr3_update_vreg_closed_loop_volt(vreg,
						vdd_volt, reg_last_measurement);

			if (!vreg->vreg_enabled
			    || vreg->current_corner
					    == CPR3_REGULATOR_CORNER_INVALID) {
				/* Cannot participate in aggregation. */
				vreg->aggregated = false;
				continue;
			} else {
				vreg->aggregated = true;
				thread_valid = true;
			}

			cpr3_regulator_aggregate_corners(&thread->aggr_corner,
				&vreg->corner[vreg->current_corner], true);
		}

		valid |= thread_valid;

		if (thread_valid)
			cpr3_regulator_aggregate_corners(&aggr_corner,
					&thread->aggr_corner, false);
	}

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

	/*
	 * When using CPR hardware closed-loop, the voltage may vary anywhere
	 * between the floor and ceiling voltage without software notification.
	 * Therefore, it is required that the floor to ceiling range for the
	 * aggregated corner not intersect the APM threshold voltage.  Adjust
	 * the floor to ceiling range if this requirement is violated.
	 *
	 * The following algorithm is applied in the case that
	 * floor < threshold <= ceiling:
	 *	if open_loop >= threshold - adj, then floor = threshold
	 *	else ceiling = threshold - step
	 * where adj = an adjustment factor to ensure sufficient voltage margin
	 * and step = VDD output step size
	 *
	 * The open-loop and last known voltages are also bounded by the new
	 * floor or ceiling value as needed.
	 */
	if (ctrl->use_hw_closed_loop
	    && aggr_corner.ceiling_volt >= ctrl->apm_threshold_volt
	    && aggr_corner.floor_volt < ctrl->apm_threshold_volt) {

		if (aggr_corner.open_loop_volt
		    >= ctrl->apm_threshold_volt - ctrl->apm_adj_volt)
			aggr_corner.floor_volt = ctrl->apm_threshold_volt;
		else
			aggr_corner.ceiling_volt
				= ctrl->apm_threshold_volt - ctrl->step_volt;

		aggr_corner.last_volt
		    = max(aggr_corner.last_volt, aggr_corner.floor_volt);
		aggr_corner.last_volt
		    = min(aggr_corner.last_volt, aggr_corner.ceiling_volt);
		aggr_corner.open_loop_volt
		    = max(aggr_corner.open_loop_volt, aggr_corner.floor_volt);
		aggr_corner.open_loop_volt
		    = min(aggr_corner.open_loop_volt, aggr_corner.ceiling_volt);
	}

	if (ctrl->use_hw_closed_loop
	    && aggr_corner.ceiling_volt >= ctrl->mem_acc_threshold_volt
	    && aggr_corner.floor_volt < ctrl->mem_acc_threshold_volt) {
		aggr_corner.floor_volt = ctrl->mem_acc_threshold_volt;
		aggr_corner.last_volt = max(aggr_corner.last_volt,
					     aggr_corner.floor_volt);
		aggr_corner.open_loop_volt = max(aggr_corner.open_loop_volt,
						  aggr_corner.floor_volt);
	}

	if (ctrl->use_hw_closed_loop) {
		dynamic_floor_volt
			= cpr3_regulator_get_dynamic_floor_volt(ctrl,
							reg_last_measurement);
		if (aggr_corner.floor_volt < dynamic_floor_volt) {
			aggr_corner.floor_volt = dynamic_floor_volt;
			aggr_corner.last_volt = max(aggr_corner.last_volt,
							aggr_corner.floor_volt);
			aggr_corner.open_loop_volt
				= max(aggr_corner.open_loop_volt,
					aggr_corner.floor_volt);
			aggr_corner.ceiling_volt = max(aggr_corner.ceiling_volt,
							aggr_corner.floor_volt);
		}
	}

	if (ctrl->cpr_enabled && ctrl->last_corner_was_closed_loop) {
		new_volt = aggr_corner.last_volt;
	} else {
		new_volt = aggr_corner.open_loop_volt;
		aggr_corner.last_volt = aggr_corner.open_loop_volt;
	}

	cpr3_debug(ctrl, "setting new voltage=%d uV\n", new_volt);
	rc = cpr3_regulator_scale_vdd_voltage(ctrl, new_volt,
					      vdd_volt, &aggr_corner);
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
			cpr3_regulator_set_target_quot(&ctrl->thread[i]);

			for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
				vreg = &ctrl->thread[i].vreg[j];

				if (vreg->vreg_enabled)
					vreg->last_closed_loop_corner
						= vreg->current_corner;
			}
		}

		if (ctrl->proc_clock_throttle) {
			if (aggr_corner.ceiling_volt > aggr_corner.floor_volt
			    && (ctrl->use_hw_closed_loop
					|| new_volt < aggr_corner.ceiling_volt))
				cpr3_write(ctrl, CPR3_REG_PD_THROTTLE,
						ctrl->proc_clock_throttle);
			else
				cpr3_write(ctrl, CPR3_REG_PD_THROTTLE,
						CPR3_PD_THROTTLE_DISABLE);
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
	ctrl->last_corner_was_closed_loop = ctrl->cpr_enabled;

	cpr3_debug(ctrl, "CPR configuration updated\n");

	return 0;
}

/**
 * cpr3_regulator_wait_for_idle() - wait for the CPR controller to no longer be
 *		busy
 * @ctrl:		Pointer to the CPR3 controller
 * @max_wait_ns:	Max wait time in nanoseconds
 *
 * Return: 0 on success or -ETIMEDOUT if the controller was still busy after
 *	   the maximum delay time
 */
static int cpr3_regulator_wait_for_idle(struct cpr3_controller *ctrl,
					s64 max_wait_ns)
{
	ktime_t start, end;
	s64 time_ns;
	u32 reg;

	/*
	 * Ensure that all previous CPR register writes have completed before
	 * checking the status register.
	 */
	mb();

	start = ktime_get();
	do {
		end = ktime_get();
		time_ns = ktime_to_ns(ktime_sub(end, start));
		if (time_ns > max_wait_ns) {
			cpr3_err(ctrl, "CPR controller still busy after %lld us\n",
				time_ns / 1000);
			return -ETIMEDOUT;
		}
		usleep_range(50, 100);
		reg = cpr3_read(ctrl, CPR3_REG_CPR_STATUS);
	} while (reg & CPR3_CPR_STATUS_BUSY_MASK);

	return 0;
}

/**
 * cmp_int() - int comparison function to be passed into the sort() function
 *		which leads to ascending sorting
 * @a:			First int value
 * @b:			Second int value
 *
 * Return: >0 if a > b, 0 if a == b, <0 if a < b
 */
static int cmp_int(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

/**
 * cpr3_regulator_measure_aging() - measure the quotient difference for the
 *		specified CPR aging sensor
 * @ctrl:		Pointer to the CPR3 controller
 * @aging_sensor:	Aging sensor to measure
 *
 * Note that vdd-supply must be configured to the aging reference voltage before
 * calling this function.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_measure_aging(struct cpr3_controller *ctrl,
				struct cpr3_aging_sensor_info *aging_sensor)
{
	u32 mask, reg, result, quot_min, quot_max, sel_min, sel_max;
	u32 quot_min_scaled, quot_max_scaled;
	u32 gcnt, gcnt_ref, gcnt0_restore, gcnt1_restore, irq_restore;
	u32 cont_dly_restore, up_down_dly_restore;
	int quot_delta, quot_delta_scaled, quot_delta_scaled_sum;
	int *quot_delta_results;
	int rc, i, aging_measurement_count, filtered_count;
	bool is_aging_measurement;

	quot_delta_results = kcalloc(CPR3_AGING_MEASUREMENT_ITERATIONS,
			sizeof(*quot_delta_results), GFP_KERNEL);
	if (!quot_delta_results)
		return -ENOMEM;

	cpr3_ctrl_loop_disable(ctrl);

	/* Enable up, down, and mid CPR interrupts */
	irq_restore = cpr3_read(ctrl, CPR3_REG_IRQ_EN);
	cpr3_write(ctrl, CPR3_REG_IRQ_EN,
			CPR3_IRQ_UP | CPR3_IRQ_DOWN | CPR3_IRQ_MID);

	/* Ensure that the aging sensor is assigned to CPR thread 0 */
	cpr3_write(ctrl, CPR3_REG_SENSOR_OWNER(aging_sensor->sensor_id), 0);

	/* Switch from HW to SW closed-loop if necessary */
	if (ctrl->supports_hw_closed_loop) {
		if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR4) {
			cpr3_masked_write(ctrl, CPR4_REG_MARGIN_ADJ_CTL,
				CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_EN_MASK,
				CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_DISABLE);
		} else if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3) {
			cpr3_write(ctrl, CPR3_REG_HW_CLOSED_LOOP,
				CPR3_HW_CLOSED_LOOP_DISABLE);
		}
	}

	/* Configure the GCNT for RO0 and RO1 that are used for aging */
	gcnt0_restore = cpr3_read(ctrl, CPR3_REG_GCNT(0));
	gcnt1_restore = cpr3_read(ctrl, CPR3_REG_GCNT(1));
	gcnt_ref = cpr3_regulator_get_gcnt(ctrl);
	gcnt = gcnt_ref * 3 / 2;
	cpr3_write(ctrl, CPR3_REG_GCNT(0), gcnt);
	cpr3_write(ctrl, CPR3_REG_GCNT(1), gcnt);

	/*
	 * Mask all sensors except for the one to measure and bypass all
	 * sensors in collapsible domains.
	 */
	for (i = 0; i <= ctrl->sensor_count / 32; i++) {
		mask = GENMASK(min(31, ctrl->sensor_count - i * 32), 0);
		if (aging_sensor->sensor_id / 32 >= i
		    && aging_sensor->sensor_id / 32 < (i + 1))
			mask &= ~BIT(aging_sensor->sensor_id % 32);
		cpr3_write(ctrl, CPR3_REG_SENSOR_MASK_WRITE_BANK(i), mask);
		cpr3_write(ctrl, CPR3_REG_SENSOR_BYPASS_WRITE_BANK(i),
				aging_sensor->bypass_mask[i]);
	}

	/* Set CPR loop delays to 0 us */
	if (ctrl->supports_hw_closed_loop
		&& ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3) {
		cont_dly_restore = cpr3_read(ctrl, CPR3_REG_CPR_TIMER_MID_CONT);
		up_down_dly_restore = cpr3_read(ctrl,
						CPR3_REG_CPR_TIMER_UP_DN_CONT);
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_MID_CONT, 0);
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_UP_DN_CONT, 0);
	} else {
		cont_dly_restore = cpr3_read(ctrl,
						CPR3_REG_CPR_TIMER_AUTO_CONT);
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_AUTO_CONT, 0);
	}

	/* Set count mode to all-at-once min with no repeat */
	cpr3_masked_write(ctrl, CPR3_REG_CPR_CTL,
		CPR3_CPR_CTL_COUNT_MODE_MASK | CPR3_CPR_CTL_COUNT_REPEAT_MASK,
		CPR3_CPR_CTL_COUNT_MODE_ALL_AT_ONCE_MIN
			<< CPR3_CPR_CTL_COUNT_MODE_SHIFT);

	cpr3_ctrl_loop_enable(ctrl);

	rc = cpr3_regulator_wait_for_idle(ctrl,
					CPR3_AGING_MEASUREMENT_TIMEOUT_NS);
	if (rc)
		goto cleanup;

	/* Set count mode to all-at-once aging */
	cpr3_masked_write(ctrl, CPR3_REG_CPR_CTL, CPR3_CPR_CTL_COUNT_MODE_MASK,
			CPR3_CPR_CTL_COUNT_MODE_ALL_AT_ONCE_AGE
				<< CPR3_CPR_CTL_COUNT_MODE_SHIFT);

	aging_measurement_count = 0;
	for (i = 0; i < CPR3_AGING_MEASUREMENT_ITERATIONS; i++) {
		/* Send CONT_NACK */
		cpr3_write(ctrl, CPR3_REG_CONT_CMD, CPR3_CONT_CMD_NACK);

		rc = cpr3_regulator_wait_for_idle(ctrl,
					CPR3_AGING_MEASUREMENT_TIMEOUT_NS);
		if (rc)
			goto cleanup;

		/* Check for PAGE_IS_AGE flag in status register */
		reg = cpr3_read(ctrl, CPR3_REG_CPR_STATUS);
		is_aging_measurement
			= reg & CPR3_CPR_STATUS_AGING_MEASUREMENT_MASK;

		/* Read CPR measurement results */
		result = cpr3_read(ctrl, CPR3_REG_RESULT1(0));
		quot_min = (result & CPR3_RESULT1_QUOT_MIN_MASK)
				>> CPR3_RESULT1_QUOT_MIN_SHIFT;
		quot_max = (result & CPR3_RESULT1_QUOT_MAX_MASK)
				>> CPR3_RESULT1_QUOT_MAX_SHIFT;
		sel_min = (result & CPR3_RESULT1_RO_MIN_MASK)
				>> CPR3_RESULT1_RO_MIN_SHIFT;
		sel_max = (result & CPR3_RESULT1_RO_MAX_MASK)
				>> CPR3_RESULT1_RO_MAX_SHIFT;

		/*
		 * Scale the quotients so that they are equivalent to the fused
		 * values.  This accounts for the difference in measurement
		 * interval times.
		 */
		quot_min_scaled = quot_min * (gcnt_ref + 1) / (gcnt + 1);
		quot_max_scaled = quot_max * (gcnt_ref + 1) / (gcnt + 1);

		if (sel_max == 1) {
			quot_delta = quot_max - quot_min;
			quot_delta_scaled = quot_max_scaled - quot_min_scaled;
		} else {
			quot_delta = quot_min - quot_max;
			quot_delta_scaled = quot_min_scaled - quot_max_scaled;
		}

		if (is_aging_measurement)
			quot_delta_results[aging_measurement_count++]
				= quot_delta_scaled;

		cpr3_debug(ctrl, "aging results: page_is_age=%u, sel_min=%u, sel_max=%u, quot_min=%u, quot_max=%u, quot_delta=%d, quot_min_scaled=%u, quot_max_scaled=%u, quot_delta_scaled=%d\n",
			is_aging_measurement, sel_min, sel_max, quot_min,
			quot_max, quot_delta, quot_min_scaled, quot_max_scaled,
			quot_delta_scaled);
	}

	filtered_count
		= aging_measurement_count - CPR3_AGING_MEASUREMENT_FILTER * 2;
	if (filtered_count > 0) {
		sort(quot_delta_results, aging_measurement_count,
			sizeof(*quot_delta_results), cmp_int, NULL);

		quot_delta_scaled_sum = 0;
		for (i = 0; i < filtered_count; i++)
			quot_delta_scaled_sum
				+= quot_delta_results[i
					+ CPR3_AGING_MEASUREMENT_FILTER];

		aging_sensor->measured_quot_diff
			= quot_delta_scaled_sum / filtered_count;
		cpr3_info(ctrl, "average quotient delta=%d (count=%d)\n",
			aging_sensor->measured_quot_diff,
			filtered_count);
	} else {
		cpr3_err(ctrl, "%d aging measurements completed after %d iterations\n",
			aging_measurement_count,
			CPR3_AGING_MEASUREMENT_ITERATIONS);
		rc = -EBUSY;
	}

cleanup:
	kfree(quot_delta_results);

	cpr3_ctrl_loop_disable(ctrl);

	cpr3_write(ctrl, CPR3_REG_IRQ_EN, irq_restore);

	cpr3_write(ctrl, CPR3_REG_GCNT(0), gcnt0_restore);
	cpr3_write(ctrl, CPR3_REG_GCNT(1), gcnt1_restore);

	if (ctrl->supports_hw_closed_loop
		&& ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3) {
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_MID_CONT, cont_dly_restore);
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_UP_DN_CONT,
				up_down_dly_restore);
	} else {
		cpr3_write(ctrl, CPR3_REG_CPR_TIMER_AUTO_CONT,
				cont_dly_restore);
	}

	for (i = 0; i <= ctrl->sensor_count / 32; i++) {
		cpr3_write(ctrl, CPR3_REG_SENSOR_MASK_WRITE_BANK(i), 0);
		cpr3_write(ctrl, CPR3_REG_SENSOR_BYPASS_WRITE_BANK(i), 0);
	}

	cpr3_masked_write(ctrl, CPR3_REG_CPR_CTL,
		CPR3_CPR_CTL_COUNT_MODE_MASK | CPR3_CPR_CTL_COUNT_REPEAT_MASK,
		(ctrl->count_mode << CPR3_CPR_CTL_COUNT_MODE_SHIFT)
		| (ctrl->count_repeat << CPR3_CPR_CTL_COUNT_REPEAT_SHIFT));

	cpr3_write(ctrl, CPR3_REG_SENSOR_OWNER(aging_sensor->sensor_id),
			ctrl->sensor_owner[aging_sensor->sensor_id]);

	cpr3_write(ctrl, CPR3_REG_IRQ_CLEAR,
			CPR3_IRQ_UP | CPR3_IRQ_DOWN | CPR3_IRQ_MID);

	if (ctrl->supports_hw_closed_loop) {
		if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR4) {
			cpr3_masked_write(ctrl, CPR4_REG_MARGIN_ADJ_CTL,
				CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_EN_MASK,
				ctrl->use_hw_closed_loop
				? CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_ENABLE
				: CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_DISABLE);
		} else if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3) {
			cpr3_write(ctrl, CPR3_REG_HW_CLOSED_LOOP,
				ctrl->use_hw_closed_loop
				? CPR3_HW_CLOSED_LOOP_ENABLE
				: CPR3_HW_CLOSED_LOOP_DISABLE);
		}
	}

	return rc;
}

/**
 * cpr3_regulator_readjust_volt_and_quot() - readjust the target quotients as
 *		well as the floor, ceiling, and open-loop voltages for the
 *		regulator by removing the old adjustment and adding the new one
 * @vreg:		Pointer to the CPR3 regulator
 * @old_adjust_volt:	Old aging adjustment voltage in microvolts
 * @new_adjust_volt:	New aging adjustment voltage in microvolts
 *
 * Also reset the cached closed loop voltage (last_volt) to equal the open-loop
 * voltage for each corner.
 *
 * Return: None
 */
static void cpr3_regulator_readjust_volt_and_quot(struct cpr3_regulator *vreg,
		int old_adjust_volt, int new_adjust_volt)
{
	unsigned long long temp;
	int i, j, old_volt, new_volt, rounded_volt;

	if (!vreg->aging_allowed)
		return;

	for (i = 0; i < vreg->corner_count; i++) {
		temp = (unsigned long long)old_adjust_volt
			* (unsigned long long)vreg->corner[i].aging_derate;
		do_div(temp, 1000);
		old_volt = temp;

		temp = (unsigned long long)new_adjust_volt
			* (unsigned long long)vreg->corner[i].aging_derate;
		do_div(temp, 1000);
		new_volt = temp;

		old_volt = min(vreg->aging_max_adjust_volt, old_volt);
		new_volt = min(vreg->aging_max_adjust_volt, new_volt);

		for (j = 0; j < CPR3_RO_COUNT; j++) {
			if (vreg->corner[i].target_quot[j] != 0) {
				vreg->corner[i].target_quot[j]
					+= cpr3_quot_adjustment(
						vreg->corner[i].ro_scale[j],
						new_volt)
					   - cpr3_quot_adjustment(
						vreg->corner[i].ro_scale[j],
						old_volt);
			}
		}

		rounded_volt = CPR3_ROUND(new_volt,
					vreg->thread->ctrl->step_volt);

		if (!vreg->aging_allow_open_loop_adj)
			rounded_volt = 0;

		vreg->corner[i].ceiling_volt
			= vreg->corner[i].unaged_ceiling_volt + rounded_volt;
		vreg->corner[i].ceiling_volt = min(vreg->corner[i].ceiling_volt,
					      vreg->corner[i].abs_ceiling_volt);
		vreg->corner[i].floor_volt
			= vreg->corner[i].unaged_floor_volt + rounded_volt;
		vreg->corner[i].floor_volt = min(vreg->corner[i].floor_volt,
						vreg->corner[i].ceiling_volt);
		vreg->corner[i].open_loop_volt
			= vreg->corner[i].unaged_open_loop_volt + rounded_volt;
		vreg->corner[i].open_loop_volt
			= min(vreg->corner[i].open_loop_volt,
				vreg->corner[i].ceiling_volt);

		vreg->corner[i].last_volt = vreg->corner[i].open_loop_volt;

		cpr3_debug(vreg, "corner %d: applying %d uV closed-loop and %d uV open-loop voltage margin adjustment\n",
			i, new_volt, rounded_volt);
	}
}

/**
 * cpr3_regulator_set_aging_ref_adjustment() - adjust target quotients for the
 *		regulators managed by this CPR controller to account for aging
 * @ctrl:		Pointer to the CPR3 controller
 * @ref_adjust_volt:	New aging reference adjustment voltage in microvolts to
 *			apply to all regulators managed by this CPR controller
 *
 * The existing aging adjustment as defined by ctrl->aging_ref_adjust_volt is
 * first removed and then the adjustment is applied.  Lastly, the value of
 * ctrl->aging_ref_adjust_volt is updated to ref_adjust_volt.
 */
static void cpr3_regulator_set_aging_ref_adjustment(
		struct cpr3_controller *ctrl, int ref_adjust_volt)
{
	int i, j;

	for (i = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			cpr3_regulator_readjust_volt_and_quot(
				&ctrl->thread[i].vreg[j],
				ctrl->aging_ref_adjust_volt,
				ref_adjust_volt);
		}
	}

	ctrl->aging_ref_adjust_volt = ref_adjust_volt;
}

/**
 * cpr3_regulator_aging_adjust() - adjust the target quotients for regulators
 *		based on the output of CPR aging sensors
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_aging_adjust(struct cpr3_controller *ctrl)
{
	struct cpr3_regulator *vreg;
	struct cpr3_corner restore_aging_corner;
	struct cpr3_corner *corner;
	int *restore_current_corner;
	bool *restore_vreg_enabled;
	int i, j, id, rc, rc2, vreg_count, aging_volt, max_aging_volt;
	u32 reg;

	if (!ctrl->aging_required || !ctrl->cpr_enabled
	    || ctrl->aggr_corner.ceiling_volt == 0
	    || ctrl->aggr_corner.ceiling_volt > ctrl->aging_ref_volt)
		return 0;

	for (i = 0, vreg_count = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			vreg = &ctrl->thread[i].vreg[j];
			vreg_count++;

			if (vreg->aging_allowed && vreg->vreg_enabled
			    && vreg->current_corner > vreg->aging_corner)
				return 0;
		}
	}

	/* Verify that none of the aging sensors are currently masked. */
	for (i = 0; i < ctrl->aging_sensor_count; i++) {
		id = ctrl->aging_sensor[i].sensor_id;
		reg = cpr3_read(ctrl, CPR3_REG_SENSOR_MASK_READ(id));
		if (reg & BIT(id % 32))
			return 0;
	}

	restore_current_corner = kcalloc(vreg_count,
				sizeof(*restore_current_corner), GFP_KERNEL);
	restore_vreg_enabled = kcalloc(vreg_count,
				sizeof(*restore_vreg_enabled), GFP_KERNEL);
	if (!restore_current_corner || !restore_vreg_enabled) {
		kfree(restore_current_corner);
		kfree(restore_vreg_enabled);
		return -ENOMEM;
	}

	/* Force all regulators to the aging corner */
	for (i = 0, vreg_count = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++, vreg_count++) {
			vreg = &ctrl->thread[i].vreg[j];

			restore_current_corner[vreg_count]
				= vreg->current_corner;
			restore_vreg_enabled[vreg_count]
				= vreg->vreg_enabled;

			vreg->current_corner = vreg->aging_corner;
			vreg->vreg_enabled = true;
		}
	}

	/* Force one of the regulators to require the aging reference voltage */
	vreg = &ctrl->thread[0].vreg[0];
	corner = &vreg->corner[vreg->current_corner];
	restore_aging_corner = *corner;
	corner->ceiling_volt = ctrl->aging_ref_volt;
	corner->floor_volt = ctrl->aging_ref_volt;
	corner->open_loop_volt = ctrl->aging_ref_volt;
	corner->last_volt = ctrl->aging_ref_volt;

	/* Skip last_volt caching */
	ctrl->last_corner_was_closed_loop = false;

	/* Set the vdd supply voltage to the aging reference voltage */
	rc = _cpr3_regulator_update_ctrl_state(ctrl);
	if (rc) {
		cpr3_err(ctrl, "unable to force vdd-supply to the aging reference voltage=%d uV, rc=%d\n",
			ctrl->aging_ref_volt, rc);
		goto cleanup;
	}

	if (ctrl->aging_vdd_mode) {
		rc = regulator_set_mode(ctrl->vdd_regulator,
					ctrl->aging_vdd_mode);
		if (rc) {
			cpr3_err(ctrl, "unable to configure vdd-supply for mode=%u, rc=%d\n",
				ctrl->aging_vdd_mode, rc);
			goto cleanup;
		}
	}

	/* Perform aging measurement on all aging sensors */
	max_aging_volt = 0;
	for (i = 0; i < ctrl->aging_sensor_count; i++) {
		for (j = 0; j < CPR3_AGING_RETRY_COUNT; j++) {
			rc = cpr3_regulator_measure_aging(ctrl,
					&ctrl->aging_sensor[i]);
			if (!rc)
				break;
		}

		if (!rc) {
			aging_volt =
				cpr3_voltage_adjustment(
					ctrl->aging_sensor[i].ro_scale,
					ctrl->aging_sensor[i].measured_quot_diff
					- ctrl->aging_sensor[i].init_quot_diff);
			max_aging_volt = max(max_aging_volt, aging_volt);
		} else {
			cpr3_err(ctrl, "CPR aging measurement failed after %d tries, rc=%d\n",
				rc, CPR3_AGING_RETRY_COUNT);
			ctrl->aging_failed = true;
			ctrl->aging_required = false;
			goto cleanup;
		}
	}

cleanup:
	vreg = &ctrl->thread[0].vreg[0];
	vreg->corner[vreg->current_corner] = restore_aging_corner;

	for (i = 0, vreg_count = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++, vreg_count++) {
			vreg = &ctrl->thread[i].vreg[j];
			vreg->current_corner
				= restore_current_corner[vreg_count];
			vreg->vreg_enabled = restore_vreg_enabled[vreg_count];
		}
	}

	kfree(restore_current_corner);
	kfree(restore_vreg_enabled);

	/* Adjust the CPR target quotients according to the aging measurement */
	if (!rc) {
		cpr3_regulator_set_aging_ref_adjustment(ctrl, max_aging_volt);

		cpr3_info(ctrl, "aging measurement successful; aging reference adjustment voltage=%d uV\n",
			ctrl->aging_ref_adjust_volt);
		ctrl->aging_succeeded = true;
		ctrl->aging_required = false;
	}

	if (ctrl->aging_complete_vdd_mode) {
		rc = regulator_set_mode(ctrl->vdd_regulator,
					ctrl->aging_complete_vdd_mode);
		if (rc)
			cpr3_err(ctrl, "unable to configure vdd-supply for mode=%u, rc=%d\n",
				ctrl->aging_complete_vdd_mode, rc);
	}

	/* Skip last_volt caching */
	ctrl->last_corner_was_closed_loop = false;

	/*
	 * Restore vdd-supply to the voltage before the aging measurement and
	 * restore the CPR3 controller hardware state.
	 */
	rc2 = _cpr3_regulator_update_ctrl_state(ctrl);

	/* Stop last_volt caching on for the next request */
	ctrl->last_corner_was_closed_loop = false;

	return rc ? rc : rc2;
}

/**
 * cpr3_regulator_update_ctrl_state() - update the state of the CPR controller
 *		to reflect the corners used by all CPR3 regulators as well as
 *		the CPR operating mode and perform aging adjustments if needed
 * @ctrl:		Pointer to the CPR3 controller
 *
 * Note, CPR3 controller lock must be held by the caller.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_update_ctrl_state(struct cpr3_controller *ctrl)
{
	int rc;

	rc = _cpr3_regulator_update_ctrl_state(ctrl);
	if (rc)
		return rc;

	return cpr3_regulator_aging_adjust(ctrl);
}

/**
 * cpr3_regulator_set_voltage() - set the voltage corner for the CPR3 regulator
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
	struct cpr3_regulator *vreg = rdev_get_drvdata(rdev);
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	int rc = 0;
	int last_corner;

	corner -= CPR3_CORNER_OFFSET;
	corner_max -= CPR3_CORNER_OFFSET;
	*selector = corner;

	mutex_lock(&ctrl->lock);

	if (!vreg->vreg_enabled) {
		vreg->current_corner = corner;
		cpr3_debug(vreg, "stored corner=%d\n", corner);
		goto done;
	} else if (vreg->current_corner == corner) {
		goto done;
	}

	last_corner = vreg->current_corner;
	vreg->current_corner = corner;

	rc = cpr3_regulator_update_ctrl_state(ctrl);
	if (rc) {
		cpr3_err(vreg, "could not update CPR state, rc=%d\n", rc);
		vreg->current_corner = last_corner;
	}

	cpr3_debug(vreg, "set corner=%d\n", corner);
done:
	mutex_unlock(&ctrl->lock);

	return rc;
}

/**
 * cpr3_regulator_get_voltage() - get the voltage corner for the CPR3 regulator
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
	struct cpr3_regulator *vreg = rdev_get_drvdata(rdev);

	if (vreg->current_corner == CPR3_REGULATOR_CORNER_INVALID)
		return CPR3_CORNER_OFFSET;
	else
		return vreg->current_corner + CPR3_CORNER_OFFSET;
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
	struct cpr3_regulator *vreg = rdev_get_drvdata(rdev);

	if (selector < vreg->corner_count)
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
	struct cpr3_regulator *vreg = rdev_get_drvdata(rdev);

	corner -= CPR3_CORNER_OFFSET;

	if (corner >= 0 && corner < vreg->corner_count)
		return vreg->corner[corner].ceiling_volt;
	else
		return -EINVAL;
}

/**
 * cpr3_regulator_is_enabled() - return the enable state of the CPR3 regulator
 * @rdev:		Regulator device pointer for the cpr3-regulator
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: true if regulator is enabled, false if regulator is disabled
 */
static int cpr3_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct cpr3_regulator *vreg = rdev_get_drvdata(rdev);

	return vreg->vreg_enabled;
}

/**
 * cpr3_regulator_enable() - enable the CPR3 regulator
 * @rdev:		Regulator device pointer for the cpr3-regulator
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_enable(struct regulator_dev *rdev)
{
	struct cpr3_regulator *vreg = rdev_get_drvdata(rdev);
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	int rc = 0;

	if (vreg->vreg_enabled == true)
		return 0;

	mutex_lock(&ctrl->lock);

	if (ctrl->system_regulator) {
		rc = regulator_enable(ctrl->system_regulator);
		if (rc) {
			cpr3_err(ctrl, "regulator_enable(system) failed, rc=%d\n",
				rc);
			goto done;
		}
	}

	rc = regulator_enable(ctrl->vdd_regulator);
	if (rc) {
		cpr3_err(vreg, "regulator_enable(vdd) failed, rc=%d\n", rc);
		goto done;
	}

	if (vreg->ldo_regulator) {
		rc = regulator_enable(vreg->ldo_regulator);
		if (rc) {
			cpr3_err(vreg, "regulator_enable(ldo) failed, rc=%d\n",
				 rc);
			goto done;
		}
	}

	vreg->vreg_enabled = true;
	rc = cpr3_regulator_update_ctrl_state(ctrl);
	if (rc) {
		cpr3_err(vreg, "could not update CPR state, rc=%d\n", rc);
		regulator_disable(ctrl->vdd_regulator);
		vreg->vreg_enabled = false;
		goto done;
	}

	cpr3_debug(vreg, "Enabled\n");
done:
	mutex_unlock(&ctrl->lock);

	return rc;
}

/**
 * cpr3_regulator_disable() - disable the CPR3 regulator
 * @rdev:		Regulator device pointer for the cpr3-regulator
 *
 * This function is passed as a callback function into the regulator ops that
 * are registered for each cpr3-regulator device.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_disable(struct regulator_dev *rdev)
{
	struct cpr3_regulator *vreg = rdev_get_drvdata(rdev);
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	int rc, rc2;

	if (vreg->vreg_enabled == false)
		return 0;

	mutex_lock(&ctrl->lock);

	if (vreg->ldo_regulator && vreg->ldo_regulator_bypass == LDO_MODE) {
		rc = regulator_get_voltage(ctrl->vdd_regulator);
		if (rc < 0) {
			cpr3_err(vreg, "regulator_get_voltage(vdd) failed, rc=%d\n",
				 rc);
			goto done;
		}

		/* Switch back to BHS for safe operation */
		rc = cpr3_regulator_set_bhs_mode(vreg, rc,
				       ctrl->aggr_corner.ceiling_volt);
		if (rc) {
			cpr3_err(vreg, "unable to switch to BHS mode, rc=%d\n",
				 rc);
			goto done;
		}
	}

	if (vreg->ldo_regulator) {
		rc = regulator_disable(vreg->ldo_regulator);
		if (rc) {
			cpr3_err(vreg, "regulator_disable(ldo) failed, rc=%d\n",
				 rc);
			goto done;
		}
	}
	rc = regulator_disable(ctrl->vdd_regulator);
	if (rc) {
		cpr3_err(vreg, "regulator_disable(vdd) failed, rc=%d\n", rc);
		goto done;
	}

	vreg->vreg_enabled = false;
	rc = cpr3_regulator_update_ctrl_state(ctrl);
	if (rc) {
		cpr3_err(vreg, "could not update CPR state, rc=%d\n", rc);
		rc2 = regulator_enable(ctrl->vdd_regulator);
		vreg->vreg_enabled = true;
		goto done;
	}

	if (ctrl->system_regulator) {
		rc = regulator_disable(ctrl->system_regulator);
		if (rc) {
			cpr3_err(ctrl, "regulator_disable(system) failed, rc=%d\n",
				rc);
			goto done;
		}
	}

	cpr3_debug(vreg, "Disabled\n");
done:
	mutex_unlock(&ctrl->lock);

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
	cpr3_debug(ctrl, "thread %u: busy=%u, step_dn=%u, step_up=%u, error_steps=%s%u, error=%s%u\n",
		thread->thread_id, busy, step_dn, step_up, sign, error_steps,
		sign, error);
	cpr3_debug(ctrl, "thread %u: quot_min=%u, quot_max=%u, ro_min=%u, ro_max=%u\n",
		thread->thread_id, quot_min, quot_max, ro_min, ro_max);
	cpr3_debug(ctrl, "thread %u: step_quot_min=%u, step_quot_max=%u, sensor_min=%u, sensor_max=%u\n",
		thread->thread_id, step_quot_min, step_quot_max, sensor_min,
		sensor_max);
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
	u32 reg_last_measurement = 0;
	struct cpr3_regulator *vreg;
	struct cpr3_corner *corner;
	unsigned long flags;
	int i, j, new_volt, last_volt, dynamic_floor_volt, rc;
	u32 irq_en, status, cpr_status, ctl;
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

	/*
	 * CPR IRQ status checking and CPR controller disabling must happen
	 * atomically and without invening delay in order to avoid an interrupt
	 * storm caused by the handler racing with the CPR controller.
	 */
	local_irq_save(flags);
	preempt_disable();

	status = cpr3_read(ctrl, CPR3_REG_IRQ_STATUS);
	up = status & CPR3_IRQ_UP;
	down = status & CPR3_IRQ_DOWN;

	if (!up && !down) {
		/*
		 * Toggle the CPR controller off and then back on since the
		 * hardware and software states are out of sync.  This condition
		 * occurs after an aging measurement completes as the CPR IRQ
		 * physically triggers during the aging measurement but the
		 * handler is stuck waiting on the mutex lock.
		 */
		cpr3_ctrl_loop_disable(ctrl);

		local_irq_restore(flags);
		preempt_enable();

		/* Wait for the loop disable write to complete */
		mb();

		/* Wait for BUSY=1 and LOOP_EN=0 in CPR controller registers. */
		for (i = 0; i < CPR3_REGISTER_WRITE_DELAY_US / 10; i++) {
			cpr_status = cpr3_read(ctrl, CPR3_REG_CPR_STATUS);
			ctl = cpr3_read(ctrl, CPR3_REG_CPR_CTL);
			if (cpr_status & CPR3_CPR_STATUS_BUSY_MASK
			    && (ctl & CPR3_CPR_CTL_LOOP_EN_MASK)
					== CPR3_CPR_CTL_LOOP_DISABLE)
				break;
			udelay(10);
		}
		if (i == CPR3_REGISTER_WRITE_DELAY_US / 10)
			cpr3_debug(ctrl, "CPR controller not disabled after %d us\n",
				CPR3_REGISTER_WRITE_DELAY_US);

		/* Clear interrupt status */
		cpr3_write(ctrl, CPR3_REG_IRQ_CLEAR,
			CPR3_IRQ_UP | CPR3_IRQ_DOWN);

		/* Wait for the interrupt clearing write to complete */
		mb();

		/* Wait for IRQ_STATUS register to be cleared. */
		for (i = 0; i < CPR3_REGISTER_WRITE_DELAY_US / 10; i++) {
			status = cpr3_read(ctrl, CPR3_REG_IRQ_STATUS);
			if (!(status & (CPR3_IRQ_UP | CPR3_IRQ_DOWN)))
				break;
			udelay(10);
		}
		if (i == CPR3_REGISTER_WRITE_DELAY_US / 10)
			cpr3_debug(ctrl, "CPR interrupts not cleared after %d us\n",
				CPR3_REGISTER_WRITE_DELAY_US);

		cpr3_ctrl_loop_enable(ctrl);

		cpr3_debug(ctrl, "CPR interrupt received but no up or down status bit is set\n");

		mutex_unlock(&ctrl->lock);
		return IRQ_HANDLED;
	} else if (up && down) {
		cpr3_debug(ctrl, "both up and down status bits set\n");
		/* The up flag takes precedence over the down flag. */
		down = false;
	}

	if (ctrl->supports_hw_closed_loop)
		reg_last_measurement
			= cpr3_read(ctrl, CPR3_REG_LAST_MEASUREMENT);
	dynamic_floor_volt = cpr3_regulator_get_dynamic_floor_volt(ctrl,
							reg_last_measurement);

	local_irq_restore(flags);
	preempt_enable();

	irq_en = aggr->irq_en;
	last_volt = aggr->last_volt;

	for (i = 0; i < ctrl->thread_count; i++) {
		if (cpr3_thread_busy(&ctrl->thread[i])) {
			cpr3_debug(ctrl, "CPR thread %u busy when it should be waiting for SW cont\n",
				ctrl->thread[i].thread_id);
			goto done;
		}
	}

	new_volt = up ? last_volt + ctrl->step_volt
		      : last_volt - ctrl->step_volt;

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

	if (down && new_volt < dynamic_floor_volt) {
		/*
		 * The vdd-supply voltage should not be decreased below the
		 * dynamic floor voltage.  However, it is not necessary (and
		 * counter productive) to force the voltage up to this level
		 * if it happened to be below it since the closed-loop voltage
		 * must have gotten there in a safe manner while the power
		 * domains for the CPR3 regulator imposing the dynamic floor
		 * were not bypassed.
		 */
		new_volt = last_volt;
		irq_en &= ~CPR3_IRQ_DOWN;
		cpr3_debug(ctrl, "limiting to dynamic floor=%d uV\n",
			dynamic_floor_volt);
	}

	for (i = 0; i < ctrl->thread_count; i++)
		cpr3_print_result(&ctrl->thread[i]);

	cpr3_debug(ctrl, "%s: new_volt=%d uV, last_volt=%d uV\n",
		up ? "UP" : "DN", new_volt, last_volt);

	if (ctrl->proc_clock_throttle && last_volt == aggr->ceiling_volt
	    && new_volt < last_volt)
		cpr3_write(ctrl, CPR3_REG_PD_THROTTLE,
				ctrl->proc_clock_throttle);

	if (new_volt != last_volt) {
		rc = cpr3_regulator_scale_vdd_voltage(ctrl, new_volt,
						      last_volt,
						      aggr);
		if (rc) {
			cpr3_err(ctrl, "scale_vdd() failed to set vdd=%d uV, rc=%d\n",
				 new_volt, rc);
			goto done;
		}
		cont = CPR3_CONT_CMD_ACK;

		/*
		 * Update the closed-loop voltage for all regulators managed
		 * by this CPR controller.
		 */
		for (i = 0; i < ctrl->thread_count; i++) {
			for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
				vreg = &ctrl->thread[i].vreg[j];
				cpr3_update_vreg_closed_loop_volt(vreg,
					new_volt, reg_last_measurement);
			}
		}
	}

	if (ctrl->proc_clock_throttle && new_volt == aggr->ceiling_volt)
		cpr3_write(ctrl, CPR3_REG_PD_THROTTLE,
				CPR3_PD_THROTTLE_DISABLE);

	corner = &ctrl->thread[0].vreg[0].corner[
			ctrl->thread[0].vreg[0].current_corner];

	if (irq_en != aggr->irq_en) {
		aggr->irq_en = irq_en;
		cpr3_write(ctrl, CPR3_REG_IRQ_EN, irq_en);
	}

	aggr->last_volt = new_volt;

done:
	/* Clear interrupt status */
	cpr3_write(ctrl, CPR3_REG_IRQ_CLEAR, CPR3_IRQ_UP | CPR3_IRQ_DOWN);

	/* ACK or NACK the CPR controller */
	cpr3_write(ctrl, CPR3_REG_CONT_CMD, cont);

	mutex_unlock(&ctrl->lock);
	return IRQ_HANDLED;
}

/**
 * cpr3_ceiling_irq_handler() - CPR ceiling reached interrupt handler callback
 *		function used for hardware closed-loop operation
 * @irq:		CPR ceiling interrupt number
 * @data:		Private data corresponding to the CPR3 controller
 *			pointer
 *
 * This function disables processor clock throttling and closed-loop operation
 * when the ceiling voltage is reached.
 *
 * Return: IRQ_HANDLED
 */
static irqreturn_t cpr3_ceiling_irq_handler(int irq, void *data)
{
	struct cpr3_controller *ctrl = data;
	int rc, volt;

	mutex_lock(&ctrl->lock);

	if (!ctrl->cpr_enabled) {
		cpr3_debug(ctrl, "CPR ceiling interrupt received but CPR is disabled\n");
		goto done;
	} else if (!ctrl->use_hw_closed_loop) {
		cpr3_debug(ctrl, "CPR ceiling interrupt received but CPR is using SW closed-loop\n");
		goto done;
	}

	volt = regulator_get_voltage(ctrl->vdd_regulator);
	if (volt < 0) {
		cpr3_err(ctrl, "could not get vdd voltage, rc=%d\n", volt);
		goto done;
	} else if (volt != ctrl->aggr_corner.ceiling_volt) {
		cpr3_debug(ctrl, "CPR ceiling interrupt received but vdd voltage: %d uV != ceiling voltage: %d uV\n",
			volt, ctrl->aggr_corner.ceiling_volt);
		goto done;
	}

	if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3) {
		/*
		 * Since the ceiling voltage has been reached, disable processor
		 * clock throttling as well as CPR closed-loop operation.
		 */
		cpr3_write(ctrl, CPR3_REG_PD_THROTTLE,
				CPR3_PD_THROTTLE_DISABLE);
		cpr3_ctrl_loop_disable(ctrl);
		cpr3_debug(ctrl, "CPR closed-loop and throttling disabled\n");
	}

done:
	rc = msm_spm_avs_clear_irq(0, MSM_SPM_AVS_IRQ_MAX);
	if (rc)
		cpr3_err(ctrl, "could not clear max IRQ, rc=%d\n", rc);

	mutex_unlock(&ctrl->lock);
	return IRQ_HANDLED;
}

/**
 * cpr3_regulator_vreg_register() - register a regulator device for a CPR3
 *		regulator
 * @vreg:		Pointer to the CPR3 regulator
 *
 * This function initializes all regulator framework related structures and then
 * calls regulator_register() for the CPR3 regulator.
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_vreg_register(struct cpr3_regulator *vreg)
{
	struct regulator_config config = {};
	struct regulator_desc *rdesc;
	struct regulator_init_data *init_data;
	int rc;

	init_data = of_get_regulator_init_data(vreg->thread->ctrl->dev,
						vreg->of_node);
	if (!init_data) {
		cpr3_err(vreg, "regulator init data is missing\n");
		return -EINVAL;
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS;

	rdesc			= &vreg->rdesc;
	rdesc->n_voltages	= vreg->corner_count;
	rdesc->name		= init_data->constraints.name;
	rdesc->ops		= &cpr3_regulator_ops;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;

	config.dev		= vreg->thread->ctrl->dev;
	config.driver_data	= vreg;
	config.init_data	= init_data;
	config.of_node		= vreg->of_node;

	vreg->rdev = regulator_register(rdesc, &config);
	if (IS_ERR(vreg->rdev)) {
		rc = PTR_ERR(vreg->rdev);
		cpr3_err(vreg, "regulator_register failed, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int debugfs_int_set(void *data, u64 val)
{
	*(int *)data = val;
	return 0;
}

static int debugfs_int_get(void *data, u64 *val)
{
	*val = *(int *)data;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_int, debugfs_int_get, debugfs_int_set, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_int_ro, debugfs_int_get, NULL, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_int_wo, NULL, debugfs_int_set, "%lld\n");

/**
 * debugfs_create_int - create a debugfs file that is used to read and write a
 *		signed int value
 * @name:		Pointer to a string containing the name of the file to
 *			create
 * @mode:		The permissions that the file should have
 * @parent:		Pointer to the parent dentry for this file.  This should
 *			be a directory dentry if set.  If this parameter is
 *			%NULL, then the file will be created in the root of the
 *			debugfs filesystem.
 * @value:		Pointer to the variable that the file should read to and
 *			write from
 *
 * This function creates a file in debugfs with the given name that
 * contains the value of the variable @value.  If the @mode variable is so
 * set, it can be read from, and written to.
 *
 * This function will return a pointer to a dentry if it succeeds.  This
 * pointer must be passed to the debugfs_remove() function when the file is
 * to be removed.  If an error occurs, %NULL will be returned.
 */
static struct dentry *debugfs_create_int(const char *name, umode_t mode,
				struct dentry *parent, int *value)
{
	/* if there are no write bits set, make read only */
	if (!(mode & S_IWUGO))
		return debugfs_create_file(name, mode, parent, value,
					   &fops_int_ro);
	/* if there are no read bits set, make write only */
	if (!(mode & S_IRUGO))
		return debugfs_create_file(name, mode, parent, value,
					   &fops_int_wo);

	return debugfs_create_file(name, mode, parent, value, &fops_int);
}

static int debugfs_bool_get(void *data, u64 *val)
{
	*val = *(bool *)data;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_bool_ro, debugfs_bool_get, NULL, "%lld\n");

/**
 * cpr3_debug_ldo_mode_allowed_set() - debugfs callback used to change the
 *		value of the CPR3 regulator ldo_mode_allowed flag
 * @data:		Pointer to private data which is equal to the CPR3
 *			regulator pointer
 * @val:		New value for ldo_mode_allowed
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_ldo_mode_allowed_set(void *data, u64 val)
{
	struct cpr3_regulator *vreg = data;
	struct cpr3_controller *ctrl = vreg->thread->ctrl;
	bool allow = !!val;
	int rc, vdd_volt;

	mutex_lock(&ctrl->lock);

	if (vreg->ldo_mode_allowed == allow)
		goto done;

	vreg->ldo_mode_allowed = allow;

	if (!allow && vreg->ldo_regulator_bypass == LDO_MODE) {
		vdd_volt = regulator_get_voltage(ctrl->vdd_regulator);
		if (vdd_volt < 0) {
			cpr3_err(vreg, "regulator_get_voltage(vdd) failed, rc=%d\n",
				 vdd_volt);
			goto done;
		}

		/* Switch back to BHS */
		rc = cpr3_regulator_set_bhs_mode(vreg, vdd_volt,
				       ctrl->aggr_corner.ceiling_volt);
		if (rc) {
			cpr3_err(vreg, "unable to switch to BHS mode, rc=%d\n",
				 rc);
			goto done;
		}
	} else {
		rc = cpr3_regulator_update_ctrl_state(ctrl);
		if (rc) {
			cpr3_err(vreg, "could not change LDO mode=%s, rc=%d\n",
				allow ? "allowed" : "disallowed", rc);
			goto done;
		}
	}

	cpr3_debug(vreg, "LDO mode=%s\n", allow ? "allowed" : "disallowed");

done:
	mutex_unlock(&ctrl->lock);
	return 0;
}

/**
 * cpr3_debug_ldo_mode_allowed_get() - debugfs callback used to retrieve the
 *		value of the CPR3 regulator ldo_mode_allowed flag
 * @data:		Pointer to private data which is equal to the CPR3
 *			regulator pointer
 * @val:		Output parameter written with a value of the
 *			ldo_mode_allowed flag
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_ldo_mode_allowed_get(void *data, u64 *val)
{
	struct cpr3_regulator *vreg = data;

	*val = vreg->ldo_mode_allowed;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr3_debug_ldo_mode_allowed_fops,
			cpr3_debug_ldo_mode_allowed_get,
			cpr3_debug_ldo_mode_allowed_set,
			"%llu\n");

/**
 * cpr3_debug_ldo_mode_get() - debugfs callback used to retrieve the state of
 *		the CPR3 regulator's LDO
 * @data:		Pointer to private data which is equal to the CPR3
 *			regulator pointer
 * @val:		Output parameter written with a value of 1 if using
 *			LDO mode or 0 if the LDO is bypassed
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_ldo_mode_get(void *data, u64 *val)
{
	struct cpr3_regulator *vreg = data;

	*val = (vreg->ldo_regulator_bypass == LDO_MODE);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr3_debug_ldo_mode_fops, cpr3_debug_ldo_mode_get,
			NULL, "%llu\n");

/**
 * struct cpr3_debug_corner_info - data structure used by the
 *		cpr3_debugfs_create_corner_int function
 * @vreg:		Pointer to the CPR3 regulator
 * @index:		Pointer to the corner array index
 * @member_offset:	Offset in bytes from the beginning of struct cpr3_corner
 *			to the beginning of the value to be read from
 * @corner:		Pointer to the CPR3 corner array
 */
struct cpr3_debug_corner_info {
	struct cpr3_regulator	*vreg;
	int			*index;
	size_t			member_offset;
	struct cpr3_corner	*corner;
};

static int cpr3_debug_corner_int_get(void *data, u64 *val)
{
	struct cpr3_debug_corner_info *info = data;
	struct cpr3_controller *ctrl = info->vreg->thread->ctrl;
	int i;

	mutex_lock(&ctrl->lock);

	i = *info->index;
	if (i < 0)
		i = 0;

	*val = *(int *)((char *)&info->vreg->corner[i] + info->member_offset);

	mutex_unlock(&ctrl->lock);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr3_debug_corner_int_fops, cpr3_debug_corner_int_get,
			NULL, "%lld\n");

/**
 * cpr3_debugfs_create_corner_int - create a debugfs file that is used to read
 *		a signed int value out of a CPR3 regulator's corner array
 * @vreg:		Pointer to the CPR3 regulator
 * @name:		Pointer to a string containing the name of the file to
 *			create
 * @mode:		The permissions that the file should have
 * @parent:		Pointer to the parent dentry for this file.  This should
 *			be a directory dentry if set.  If this parameter is
 *			%NULL, then the file will be created in the root of the
 *			debugfs filesystem.
 * @index:		Pointer to the corner array index
 * @member_offset:	Offset in bytes from the beginning of struct cpr3_corner
 *			to the beginning of the value to be read from
 *
 * This function creates a file in debugfs with the given name that
 * contains the value of the int type variable vreg->corner[index].member
 * where member_offset == offsetof(struct cpr3_corner, member).
 */
static struct dentry *cpr3_debugfs_create_corner_int(
		struct cpr3_regulator *vreg, const char *name, umode_t mode,
		struct dentry *parent, int *index, size_t member_offset)
{
	struct cpr3_debug_corner_info *info;

	info = devm_kzalloc(vreg->thread->ctrl->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return NULL;

	info->vreg = vreg;
	info->index = index;
	info->member_offset = member_offset;

	return debugfs_create_file(name, mode, parent, info,
				   &cpr3_debug_corner_int_fops);
}

static int cpr3_debug_quot_open(struct inode *inode, struct file *file)
{
	struct cpr3_debug_corner_info *info = inode->i_private;
	struct cpr3_thread *thread = info->vreg->thread;
	int size, i, pos;
	u32 *quot;
	char *buf;

	/*
	 * Max size:
	 *  - 10 digits + ' ' or '\n' = 11 bytes per number
	 *  - terminating '\0'
	 */
	size = CPR3_RO_COUNT * 11;
	buf = kzalloc(size + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	file->private_data = buf;

	mutex_lock(&thread->ctrl->lock);

	quot = info->corner[*info->index].target_quot;

	for (i = 0, pos = 0; i < CPR3_RO_COUNT; i++)
		pos += scnprintf(buf + pos, size - pos, "%u%c",
			quot[i], i < CPR3_RO_COUNT - 1 ? ' ' : '\n');

	mutex_unlock(&thread->ctrl->lock);

	return nonseekable_open(inode, file);
}

static ssize_t cpr3_debug_quot_read(struct file *file, char __user *buf,
		size_t len, loff_t *ppos)
{
	return simple_read_from_buffer(buf, len, ppos, file->private_data,
					strlen(file->private_data));
}

static int cpr3_debug_quot_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);

	return 0;
}

static const struct file_operations cpr3_debug_quot_fops = {
	.owner	 = THIS_MODULE,
	.open	 = cpr3_debug_quot_open,
	.release = cpr3_debug_quot_release,
	.read	 = cpr3_debug_quot_read,
	.llseek  = no_llseek,
};

/**
 * cpr3_regulator_debugfs_corner_add() - add debugfs files to expose
 *		configuration data for the CPR corner
 * @vreg:		Pointer to the CPR3 regulator
 * @corner_dir:		Pointer to the parent corner dentry for the new files
 * @index:		Pointer to the corner array index
 *
 * Return: none
 */
static void cpr3_regulator_debugfs_corner_add(struct cpr3_regulator *vreg,
		struct dentry *corner_dir, int *index)
{
	struct cpr3_debug_corner_info *info;
	struct dentry *temp;

	temp = cpr3_debugfs_create_corner_int(vreg, "floor_volt", S_IRUGO,
		corner_dir, index, offsetof(struct cpr3_corner, floor_volt));
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "floor_volt debugfs file creation failed\n");
		return;
	}

	temp = cpr3_debugfs_create_corner_int(vreg, "ceiling_volt", S_IRUGO,
		corner_dir, index, offsetof(struct cpr3_corner, ceiling_volt));
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "ceiling_volt debugfs file creation failed\n");
		return;
	}

	temp = cpr3_debugfs_create_corner_int(vreg, "open_loop_volt", S_IRUGO,
		corner_dir, index,
		offsetof(struct cpr3_corner, open_loop_volt));
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "open_loop_volt debugfs file creation failed\n");
		return;
	}

	temp = cpr3_debugfs_create_corner_int(vreg, "last_volt", S_IRUGO,
		corner_dir, index, offsetof(struct cpr3_corner, last_volt));
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "last_volt debugfs file creation failed\n");
		return;
	}

	info = devm_kzalloc(vreg->thread->ctrl->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return;

	info->vreg = vreg;
	info->index = index;
	info->corner = vreg->corner;

	temp = debugfs_create_file("target_quots", S_IRUGO, corner_dir,
				info, &cpr3_debug_quot_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "target_quots debugfs file creation failed\n");
		return;
	}
}

/**
 * cpr3_debug_corner_index_set() - debugfs callback used to change the
 *		value of the CPR3 regulator debug_corner index
 * @data:		Pointer to private data which is equal to the CPR3
 *			regulator pointer
 * @val:		New value for debug_corner
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_corner_index_set(void *data, u64 val)
{
	struct cpr3_regulator *vreg = data;

	if (val < CPR3_CORNER_OFFSET || val > vreg->corner_count) {
		cpr3_err(vreg, "invalid corner index %llu; allowed values: %d-%d\n",
			val, CPR3_CORNER_OFFSET, vreg->corner_count);
		return -EINVAL;
	}

	mutex_lock(&vreg->thread->ctrl->lock);
	vreg->debug_corner = val - CPR3_CORNER_OFFSET;
	mutex_unlock(&vreg->thread->ctrl->lock);

	return 0;
}

/**
 * cpr3_debug_corner_index_get() - debugfs callback used to retrieve
 *		the value of the CPR3 regulator debug_corner index
 * @data:		Pointer to private data which is equal to the CPR3
 *			regulator pointer
 * @val:		Output parameter written with the value of
 *			debug_corner
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_corner_index_get(void *data, u64 *val)
{
	struct cpr3_regulator *vreg = data;

	*val = vreg->debug_corner + CPR3_CORNER_OFFSET;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr3_debug_corner_index_fops,
			cpr3_debug_corner_index_get,
			cpr3_debug_corner_index_set,
			"%llu\n");

/**
 * cpr3_debug_current_corner_index_get() - debugfs callback used to retrieve
 *		the value of the CPR3 regulator current_corner index
 * @data:		Pointer to private data which is equal to the CPR3
 *			regulator pointer
 * @val:		Output parameter written with the value of
 *			current_corner
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_current_corner_index_get(void *data, u64 *val)
{
	struct cpr3_regulator *vreg = data;

	*val = vreg->current_corner + CPR3_CORNER_OFFSET;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr3_debug_current_corner_index_fops,
			cpr3_debug_current_corner_index_get,
			NULL, "%llu\n");

/**
 * cpr3_regulator_debugfs_vreg_add() - add debugfs files to expose configuration
 *		data for the CPR3 regulator
 * @vreg:		Pointer to the CPR3 regulator
 * @thread_dir		CPR3 thread debugfs directory handle
 *
 * Return: none
 */
static void cpr3_regulator_debugfs_vreg_add(struct cpr3_regulator *vreg,
				struct dentry *thread_dir)
{
	struct dentry *temp, *corner_dir, *vreg_dir;

	vreg_dir = debugfs_create_dir(vreg->name, thread_dir);
	if (IS_ERR_OR_NULL(vreg_dir)) {
		cpr3_err(vreg, "%s debugfs directory creation failed\n",
			vreg->name);
		return;
	}

	temp = debugfs_create_int("speed_bin_fuse", S_IRUGO, vreg_dir,
				  &vreg->speed_bin_fuse);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "speed_bin_fuse debugfs file creation failed\n");
		return;
	}

	temp = debugfs_create_int("cpr_rev_fuse", S_IRUGO, vreg_dir,
				  &vreg->cpr_rev_fuse);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "cpr_rev_fuse debugfs file creation failed\n");
		return;
	}

	temp = debugfs_create_int("fuse_combo", S_IRUGO, vreg_dir,
				  &vreg->fuse_combo);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "fuse_combo debugfs file creation failed\n");
		return;
	}

	if (vreg->ldo_regulator) {
		temp = debugfs_create_file("ldo_mode", S_IRUGO, vreg_dir,
				vreg, &cpr3_debug_ldo_mode_fops);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(vreg, "ldo_mode debugfs file creation failed\n");
			return;
		}

		temp = debugfs_create_file("ldo_mode_allowed",
				S_IRUGO | S_IWUSR, vreg_dir, vreg,
				&cpr3_debug_ldo_mode_allowed_fops);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(vreg, "ldo_mode_allowed debugfs file creation failed\n");
			return;
		}
	}

	temp = debugfs_create_int("corner_count", S_IRUGO, vreg_dir,
				  &vreg->corner_count);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "corner_count debugfs file creation failed\n");
		return;
	}

	corner_dir = debugfs_create_dir("corner", vreg_dir);
	if (IS_ERR_OR_NULL(corner_dir)) {
		cpr3_err(vreg, "corner debugfs directory creation failed\n");
		return;
	}

	temp = debugfs_create_file("index", S_IRUGO | S_IWUSR, corner_dir,
				vreg, &cpr3_debug_corner_index_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "index debugfs file creation failed\n");
		return;
	}

	cpr3_regulator_debugfs_corner_add(vreg, corner_dir,
					&vreg->debug_corner);

	corner_dir = debugfs_create_dir("current_corner", vreg_dir);
	if (IS_ERR_OR_NULL(corner_dir)) {
		cpr3_err(vreg, "current_corner debugfs directory creation failed\n");
		return;
	}

	temp = debugfs_create_file("index", S_IRUGO, corner_dir,
				vreg, &cpr3_debug_current_corner_index_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(vreg, "index debugfs file creation failed\n");
		return;
	}

	cpr3_regulator_debugfs_corner_add(vreg, corner_dir,
					  &vreg->current_corner);
}

/**
 * cpr3_regulator_debugfs_thread_add() - add debugfs files to expose
 *		configuration data for the CPR thread
 * @thread:		Pointer to the CPR3 thread
 *
 * Return: none
 */
static void cpr3_regulator_debugfs_thread_add(struct cpr3_thread *thread)
{
	struct cpr3_controller *ctrl = thread->ctrl;
	struct dentry *aggr_dir, *temp, *thread_dir;
	struct cpr3_debug_corner_info *info;
	char buf[20];
	int *index;
	int i;

	scnprintf(buf, sizeof(buf), "thread%u", thread->thread_id);
	thread_dir = debugfs_create_dir(buf, thread->ctrl->debugfs);
	if (IS_ERR_OR_NULL(thread_dir)) {
		cpr3_err(ctrl, "thread %u %s debugfs directory creation failed\n",
			thread->thread_id, buf);
		return;
	}

	aggr_dir = debugfs_create_dir("max_aggregated_params", thread_dir);
	if (IS_ERR_OR_NULL(aggr_dir)) {
		cpr3_err(ctrl, "thread %u max_aggregated_params debugfs directory creation failed\n",
			thread->thread_id);
		return;
	}

	temp = debugfs_create_int("floor_volt", S_IRUGO, aggr_dir,
				  &thread->aggr_corner.floor_volt);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "thread %u aggr floor_volt debugfs file creation failed\n",
			thread->thread_id);
		return;
	}

	temp = debugfs_create_int("ceiling_volt", S_IRUGO, aggr_dir,
				  &thread->aggr_corner.ceiling_volt);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "thread %u aggr ceiling_volt debugfs file creation failed\n",
			thread->thread_id);
		return;
	}

	temp = debugfs_create_int("open_loop_volt", S_IRUGO, aggr_dir,
				  &thread->aggr_corner.open_loop_volt);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "thread %u aggr open_loop_volt debugfs file creation failed\n",
			thread->thread_id);
		return;
	}

	temp = debugfs_create_int("last_volt", S_IRUGO, aggr_dir,
				  &thread->aggr_corner.last_volt);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "thread %u aggr last_volt debugfs file creation failed\n",
			thread->thread_id);
		return;
	}

	info = devm_kzalloc(thread->ctrl->dev, sizeof(*info), GFP_KERNEL);
	index = devm_kzalloc(thread->ctrl->dev, sizeof(*index), GFP_KERNEL);
	if (!info || !index)
		return;
	*index = 0;
	info->vreg = &thread->vreg[0];
	info->index = index;
	info->corner = &thread->aggr_corner;

	temp = debugfs_create_file("target_quots", S_IRUGO, aggr_dir,
				info, &cpr3_debug_quot_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "thread %u target_quots debugfs file creation failed\n",
			thread->thread_id);
		return;
	}

	for (i = 0; i < thread->vreg_count; i++)
		cpr3_regulator_debugfs_vreg_add(&thread->vreg[i], thread_dir);
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

	if (ctrl->proc_clock_throttle && !ctrl->cpr_enabled) {
		rc = cpr3_clock_enable(ctrl);
		if (rc) {
			cpr3_err(ctrl, "clock enable failed, rc=%d\n", rc);
			goto done;
		}
		ctrl->cpr_enabled = true;

		cpr3_write(ctrl, CPR3_REG_PD_THROTTLE,
			   CPR3_PD_THROTTLE_DISABLE);

		cpr3_clock_disable(ctrl);
		ctrl->cpr_enabled = false;
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
	struct cpr3_regulator *vreg;
	bool cpr_enabled;
	int i, j, k, rc;

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

	if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR4) {
		cpr3_masked_write(ctrl, CPR4_REG_MARGIN_ADJ_CTL,
			CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_EN_MASK,
			ctrl->use_hw_closed_loop
			? CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_ENABLE
			: CPR4_MARGIN_ADJ_CTL_HW_CLOSED_LOOP_DISABLE);
	} else if (ctrl->ctrl_type == CPR_CTRL_TYPE_CPR3) {
		cpr3_write(ctrl, CPR3_REG_HW_CLOSED_LOOP,
			ctrl->use_hw_closed_loop
			? CPR3_HW_CLOSED_LOOP_ENABLE
			: CPR3_HW_CLOSED_LOOP_DISABLE);
	}

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

		rc = msm_spm_avs_enable_irq(0, MSM_SPM_AVS_IRQ_MAX);
		if (rc) {
			cpr3_err(ctrl, "could not enable max IRQ, rc=%d\n", rc);
			goto done;
		}
	} else {
		rc = regulator_disable(ctrl->vdd_limit_regulator);
		if (rc) {
			cpr3_err(ctrl, "CPR limit regulator disable failed, rc=%d\n",
				rc);
			goto done;
		}

		rc = msm_spm_avs_disable_irq(0, MSM_SPM_AVS_IRQ_MAX);
		if (rc) {
			cpr3_err(ctrl, "could not disable max IRQ, rc=%d\n",
				rc);
			goto done;
		}
	}

	/*
	 * Due to APM and mem-acc floor restriction constraints, the closed-loop
	 * voltage may be different when using software closed-loop vs hardware
	 * closed-loop.  Therefore, reset the cached closed-loop voltage for all
	 * corners to the corresponding open-loop voltage when switching between
	 * SW and HW closed-loop mode.
	 */
	for (i = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			vreg = &ctrl->thread[i].vreg[j];
			for (k = 0; k < vreg->corner_count; k++)
				vreg->corner[k].last_volt
					= vreg->corner[k].open_loop_volt;
		}
	}

	/* Skip last_volt caching */
	ctrl->last_corner_was_closed_loop = false;

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
 * cpr3_debug_trigger_aging_measurement_set() - debugfs callback used to trigger
 *		another CPR measurement
 * @data:		Pointer to private data which is equal to the CPR
 *			controller pointer
 * @val:		Unused
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_debug_trigger_aging_measurement_set(void *data, u64 val)
{
	struct cpr3_controller *ctrl = data;
	int rc;

	mutex_lock(&ctrl->lock);

	cpr3_ctrl_loop_disable(ctrl);

	cpr3_regulator_set_aging_ref_adjustment(ctrl, INT_MAX);
	ctrl->aging_required = true;
	ctrl->aging_succeeded = false;
	ctrl->aging_failed = false;

	rc = cpr3_regulator_update_ctrl_state(ctrl);
	if (rc) {
		cpr3_err(ctrl, "could not update the CPR controller state, rc=%d\n",
			rc);
		goto done;
	}

done:
	mutex_unlock(&ctrl->lock);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cpr3_debug_trigger_aging_measurement_fops,
			NULL,
			cpr3_debug_trigger_aging_measurement_set,
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
	struct dentry *temp, *aggr_dir;
	int i;

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

	temp = debugfs_create_file("cpr_closed_loop_enable", S_IRUGO | S_IWUSR,
					ctrl->debugfs, ctrl,
					&cpr3_debug_closed_loop_enable_fops);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "cpr_closed_loop_enable debugfs file creation failed\n");
		return;
	}

	if (ctrl->supports_hw_closed_loop) {
		temp = debugfs_create_file("use_hw_closed_loop",
					S_IRUGO | S_IWUSR, ctrl->debugfs, ctrl,
					&cpr3_debug_hw_closed_loop_enable_fops);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(ctrl, "use_hw_closed_loop debugfs file creation failed\n");
			return;
		}
	}

	temp = debugfs_create_int("thread_count", S_IRUGO, ctrl->debugfs,
				  &ctrl->thread_count);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "thread_count debugfs file creation failed\n");
		return;
	}

	if (ctrl->apm) {
		temp = debugfs_create_int("apm_threshold_volt", S_IRUGO,
				ctrl->debugfs, &ctrl->apm_threshold_volt);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(ctrl, "apm_threshold_volt debugfs file creation failed\n");
			return;
		}
	}

	if (ctrl->aging_required || ctrl->aging_succeeded
	    || ctrl->aging_failed) {
		temp = debugfs_create_int("aging_adj_volt", S_IRUGO,
				ctrl->debugfs, &ctrl->aging_ref_adjust_volt);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(ctrl, "aging_adj_volt debugfs file creation failed\n");
			return;
		}

		temp = debugfs_create_file("aging_succeeded", S_IRUGO,
			ctrl->debugfs, &ctrl->aging_succeeded, &fops_bool_ro);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(ctrl, "aging_succeeded debugfs file creation failed\n");
			return;
		}

		temp = debugfs_create_file("aging_failed", S_IRUGO,
			ctrl->debugfs, &ctrl->aging_failed, &fops_bool_ro);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(ctrl, "aging_failed debugfs file creation failed\n");
			return;
		}

		temp = debugfs_create_file("aging_trigger", S_IWUSR,
			ctrl->debugfs, ctrl,
			&cpr3_debug_trigger_aging_measurement_fops);
		if (IS_ERR_OR_NULL(temp)) {
			cpr3_err(ctrl, "aging_trigger debugfs file creation failed\n");
			return;
		}
	}

	aggr_dir = debugfs_create_dir("max_aggregated_voltages", ctrl->debugfs);
	if (IS_ERR_OR_NULL(aggr_dir)) {
		cpr3_err(ctrl, "max_aggregated_voltages debugfs directory creation failed\n");
		return;
	}

	temp = debugfs_create_int("floor_volt", S_IRUGO, aggr_dir,
				  &ctrl->aggr_corner.floor_volt);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "aggr floor_volt debugfs file creation failed\n");
		return;
	}

	temp = debugfs_create_int("ceiling_volt", S_IRUGO, aggr_dir,
				  &ctrl->aggr_corner.ceiling_volt);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "aggr ceiling_volt debugfs file creation failed\n");
		return;
	}

	temp = debugfs_create_int("open_loop_volt", S_IRUGO, aggr_dir,
				  &ctrl->aggr_corner.open_loop_volt);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "aggr open_loop_volt debugfs file creation failed\n");
		return;
	}

	temp = debugfs_create_int("last_volt", S_IRUGO, aggr_dir,
				  &ctrl->aggr_corner.last_volt);
	if (IS_ERR_OR_NULL(temp)) {
		cpr3_err(ctrl, "aggr last_volt debugfs file creation failed\n");
		return;
	}

	for (i = 0; i < ctrl->thread_count; i++)
		cpr3_regulator_debugfs_thread_add(&ctrl->thread[i]);
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
 * cpr3_regulator_init_vreg_data() - performs initialization of common CPR3
 *		regulator elements and validate aging configurations
 * @vreg:		Pointer to the CPR3 regulator
 *
 * Return: 0 on success, errno on failure
 */
static int cpr3_regulator_init_vreg_data(struct cpr3_regulator *vreg)
{
	int i, j;
	bool init_aging;

	vreg->current_corner = CPR3_REGULATOR_CORNER_INVALID;
	vreg->last_closed_loop_corner = CPR3_REGULATOR_CORNER_INVALID;

	init_aging = vreg->aging_allowed && vreg->thread->ctrl->aging_required;

	for (i = 0; i < vreg->corner_count; i++) {
		vreg->corner[i].last_volt = vreg->corner[i].open_loop_volt;
		vreg->corner[i].irq_en = CPR3_IRQ_UP | CPR3_IRQ_DOWN;

		vreg->corner[i].ro_mask = 0;
		for (j = 0; j < CPR3_RO_COUNT; j++) {
			if (vreg->corner[i].target_quot[j] == 0)
				vreg->corner[i].ro_mask |= BIT(j);
		}

		if (init_aging) {
			vreg->corner[i].unaged_floor_volt
				= vreg->corner[i].floor_volt;
			vreg->corner[i].unaged_ceiling_volt
				= vreg->corner[i].ceiling_volt;
			vreg->corner[i].unaged_open_loop_volt
				= vreg->corner[i].open_loop_volt;
		}

		if (vreg->aging_allowed) {
			if (vreg->corner[i].unaged_floor_volt <= 0) {
				cpr3_err(vreg, "invalid unaged_floor_volt[%d] = %d\n",
					i, vreg->corner[i].unaged_floor_volt);
				return -EINVAL;
			}
			if (vreg->corner[i].unaged_ceiling_volt <= 0) {
				cpr3_err(vreg, "invalid unaged_ceiling_volt[%d] = %d\n",
					i, vreg->corner[i].unaged_ceiling_volt);
				return -EINVAL;
			}
			if (vreg->corner[i].unaged_open_loop_volt <= 0) {
				cpr3_err(vreg, "invalid unaged_open_loop_volt[%d] = %d\n",
				      i, vreg->corner[i].unaged_open_loop_volt);
				return -EINVAL;
			}
		}
	}

	if (vreg->aging_allowed && vreg->corner[vreg->aging_corner].ceiling_volt
	    > vreg->thread->ctrl->aging_ref_volt) {
		cpr3_err(vreg, "aging corner %d ceiling voltage = %d > aging ref voltage = %d uV\n",
			vreg->aging_corner,
			vreg->corner[vreg->aging_corner].ceiling_volt,
			vreg->thread->ctrl->aging_ref_volt);
		return -EINVAL;
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
 * cpr3_regulator_cpu_hotplug_callback() - reset CPR IRQ affinity when a CPU is
 *		brought online via hotplug
 * @nb:			Pointer to the notifier block
 * @action:		hotplug action
 * @hcpu:		long value corresponding to the CPU number
 *
 * Return: NOTIFY_OK
 */
static int cpr3_regulator_cpu_hotplug_callback(struct notifier_block *nb,
					    unsigned long action, void *hcpu)
{
	struct cpr3_controller *ctrl = container_of(nb, struct cpr3_controller,
					cpu_hotplug_notifier);
	int cpu = (long)hcpu;

	action &= ~CPU_TASKS_FROZEN;

	if (action == CPU_ONLINE
	    && cpumask_test_cpu(cpu, &ctrl->irq_affinity_mask))
		irq_set_affinity(ctrl->irq, &ctrl->irq_affinity_mask);

	return NOTIFY_OK;
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
	int i;

	if (!ctrl->vdd_regulator) {
		cpr3_err(ctrl, "vdd regulator missing\n");
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

	if (ctrl->aging_required) {
		for (i = 0; i < ctrl->aging_sensor_count; i++) {
			if (ctrl->aging_sensor[i].sensor_id
			    >= ctrl->sensor_count) {
				cpr3_err(ctrl, "aging_sensor[%d] id=%u is not in the value range 0-%d",
					i, ctrl->aging_sensor[i].sensor_id,
					ctrl->sensor_count - 1);
				return -EINVAL;
			}
		}
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
	int i, j, rc;

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

	ctrl->irq = platform_get_irq_byname(pdev, "cpr");
	if (ctrl->irq < 0) {
		cpr3_err(ctrl, "missing CPR interrupt\n");
		return ctrl->irq;
	}

	if (ctrl->supports_hw_closed_loop) {
		rc = msm_spm_probe_done();
		if (rc) {
			if (rc != -EPROBE_DEFER)
				cpr3_err(ctrl, "spm unavailable, rc=%d\n", rc);
			return rc;
		}

		ctrl->ceiling_irq = platform_get_irq_byname(pdev, "ceiling");
		if (ctrl->ceiling_irq < 0) {
			cpr3_err(ctrl, "missing ceiling interrupt\n");
			return ctrl->ceiling_irq;
		}
	}

	rc = cpr3_regulator_init_ctrl_data(ctrl);
	if (rc) {
		cpr3_err(ctrl, "CPR controller data initialization failed, rc=%d\n",
			rc);
		return rc;
	}

	for (i = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			rc = cpr3_regulator_init_vreg_data(
						&ctrl->thread[i].vreg[j]);
			if (rc)
				return rc;
			cpr3_print_quots(&ctrl->thread[i].vreg[j]);
		}
	}

	/*
	 * Add the maximum possible aging voltage margin until it is possible
	 * to perform an aging measurement.
	 */
	if (ctrl->aging_required)
		cpr3_regulator_set_aging_ref_adjustment(ctrl, INT_MAX);

	rc = cpr3_regulator_init_ctrl(ctrl);
	if (rc) {
		cpr3_err(ctrl, "CPR controller initialization failed, rc=%d\n",
			rc);
		return rc;
	}

	/* Register regulator devices for all threads. */
	for (i = 0; i < ctrl->thread_count; i++) {
		for (j = 0; j < ctrl->thread[i].vreg_count; j++) {
			rc = cpr3_regulator_vreg_register(
					&ctrl->thread[i].vreg[j]);
			if (rc) {
				cpr3_err(&ctrl->thread[i].vreg[j], "failed to register regulator, rc=%d\n",
					rc);
				goto free_regulators;
			}
		}
	}

	rc = devm_request_threaded_irq(dev, ctrl->irq, NULL, cpr3_irq_handler,
		IRQF_ONESHOT | IRQF_TRIGGER_RISING, "cpr3", ctrl);
	if (rc) {
		cpr3_err(ctrl, "could not request IRQ %d, rc=%d\n",
			ctrl->irq, rc);
		goto free_regulators;
	}

	if (ctrl->supports_hw_closed_loop) {
		rc = devm_request_threaded_irq(dev, ctrl->ceiling_irq, NULL,
			cpr3_ceiling_irq_handler,
			IRQF_ONESHOT | IRQF_TRIGGER_RISING,
			"cpr3_ceiling", ctrl);
		if (rc) {
			cpr3_err(ctrl, "could not request ceiling IRQ %d, rc=%d\n",
				ctrl->ceiling_irq, rc);
			goto free_regulators;
		}
	}

	if (ctrl->irq && !cpumask_empty(&ctrl->irq_affinity_mask)) {
		irq_set_affinity(ctrl->irq, &ctrl->irq_affinity_mask);

		ctrl->cpu_hotplug_notifier.notifier_call
			= cpr3_regulator_cpu_hotplug_callback;
		register_hotcpu_notifier(&ctrl->cpu_hotplug_notifier);
	}

	mutex_lock(&cpr3_controller_list_mutex);
	cpr3_regulator_debugfs_ctrl_add(ctrl);
	list_add(&ctrl->list, &cpr3_controller_list);
	mutex_unlock(&cpr3_controller_list_mutex);

	return 0;

free_regulators:
	for (i = 0; i < ctrl->thread_count; i++)
		for (j = 0; j < ctrl->thread[i].vreg_count; j++)
			if (!IS_ERR_OR_NULL(ctrl->thread[i].vreg[j].rdev))
				regulator_unregister(
					ctrl->thread[i].vreg[j].rdev);
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
	int i, j;

	mutex_lock(&cpr3_controller_list_mutex);
	list_del(&ctrl->list);
	cpr3_regulator_debugfs_ctrl_remove(ctrl);
	mutex_unlock(&cpr3_controller_list_mutex);

	cpr3_ctrl_loop_disable(ctrl);
	cpr3_closed_loop_disable(ctrl);

	if (ctrl->use_hw_closed_loop) {
		regulator_disable(ctrl->vdd_limit_regulator);
		msm_spm_avs_disable_irq(0, MSM_SPM_AVS_IRQ_MAX);
	}

	for (i = 0; i < ctrl->thread_count; i++)
		for (j = 0; j < ctrl->thread[i].vreg_count; j++)
			regulator_unregister(ctrl->thread[i].vreg[j].rdev);

	return 0;
}

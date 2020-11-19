/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APU_REGULATOR_H_
#define _APU_REGULATOR_H_

#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/module.h>

#include "apu_devfreq.h"

/**
 * struct apu_regulator_ops - regulator operations.
 *
 * @set_voltage: Set the voltage for the regulator within the range specified.
 *               The driver should select the voltage closest to min_uV.
 * @get_voltage: Return the currently configured voltage;
 *               return -ENOTRECOVERABLE if regulator can't
 *               be read at bootup and hasn't been set yet.
 * @enable:      Enable apu regulators.
 * @disable:     Disable apu regulators.
 *
 * This struct describes regulator operations for apu device.
 */
struct apu_regulator_ops {
	/* get/set regulator voltage */
	int (*set_voltage)(struct apu_regulator_gp *argul, int min_uV, int max_uV);
	int (*get_voltage)(struct apu_regulator_gp *argul);

	/* enable/disable regulator */
	int (*enable)(struct apu_regulator_gp *argul);
	int (*disable)(struct apu_regulator_gp *argul);
};

/**
 * struct apu_regulator - wrapper of kernel regulators for apu.
 *
 * @vdd:    Kernel regulator.
 * @cstr:   constraints for this regulator.
 *          settling_time:
 *              Latency for programing this regulator.
 *          settling_time_up:
 *              Time to settle down after voltage increase when voltage
 *		        change is non-linear (unit: microseconds).
 *          settling_time_down:
 *              Time to settle down after voltage decrease when
 *			    voltage change is non-linear (unit: microseconds).
 *          always_on:
 *              Set if the regulator should never be disabled.
 * @def_volt: Default voltage it should be.
 * @shut_volt:Shutdown voltage it should be.
 * @volt_nb:  notification hook for vdd.
 *
 * This struct describes regulator need for apu device.
 * (Most of property could be defined in kernel regulator driver)
 */
struct apu_regulator {
	char *name;
	struct device *dev;
	struct regulator *vdd;
	struct regulation_constraints cstr;
	struct mutex reg_lock;

	int enabled;
	int cur_volt;
	int def_volt;
	int shut_volt;
	int supply_trans_uV;
	int supply_trans_next_uV;

	/* Below info are for notification */
	struct apu_regulator *notify_reg;
	int (*notify_func)(struct notifier_block *nb, ulong event, void *data);
	struct notifier_block nb;

	int constrain_band;
	int constrain_volt;
	int floor_volt;
	/* deferred works settings */
	struct work_struct deffer_work;
	void (*deffer_func)(struct work_struct *work);

	struct blocking_notifier_head nf_head;
};

#define to_regulator_apu(x) \
		container_of(x, struct apu_regulator, nb)

struct apu_regulator_gp {
	struct device *dev;
	struct mutex rgulgp_lock;
	struct apu_regulator *rgul_sup;
	struct apu_regulator *rgul;
	struct apu_regulator_ops *ops;
};

struct apu_regulator_array {
	char *name;
	struct apu_regulator_gp *argul_gp;
};

struct apu_regulator_gp *regulator_apu_gp_get(struct apu_dev *ad, const char *id);
int regulator_apu_unregister_notifier(struct apu_regulator *reg,
				struct notifier_block *nb);
int regulator_apu_register_notifier(struct apu_regulator *reg,
			      struct notifier_block *nb);

#endif

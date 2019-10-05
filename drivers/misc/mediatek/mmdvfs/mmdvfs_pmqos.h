/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MMDVFS_PMQOS_H__
#define __MMDVFS_PMQOS_H__

#define MAX_FREQ_STEP 6

/**
 * mmdvfs_qos_get_freq_steps - get available freq steps of each pmqos class
 * @pm_qos_class: pm_qos_class of each mm freq domain
 * @freq_steps: output available freq_step settings, size is MAX_FREQ_STEP.
 *    If the entry is 0, it means step not available, size of available items
 *    is in step_size.
 *    The order of freq steps is from high to low.
 * @step_size: size of available items in freq_steps
 *
 * Returns 0, or -errno
 */
int mmdvfs_qos_get_freq_steps(
	u32 pm_qos_class, u64 *freq_steps, u32 *step_size);

/**
 * mmdvfs_qos_force_step - function to force mmdvfs setting ignore PMQoS update
 * @step: force step of mmdvfs
 *
 * Returns 0, or -errno
 */
int mmdvfs_qos_force_step(int step);

/**
 * mmdvfs_qos_enable - function to enable or disable mmdvfs
 * @enable: mmdvfs enable or disable
 */
void mmdvfs_qos_enable(bool enable);

/**
 * mmdvfs_qos_get_freq - get current freq of each pmqos class
 * @pm_qos_class: pm_qos_class of each mm freq domain
 *
 * Returns {Freq} in MHz
 */
u64 mmdvfs_qos_get_freq(u32 pm_qos_class);

enum mmdvfs_prepare_event {
	MMDVFS_PREPARE_CALIBRATION_START, MMDVFS_PREPARE_CALIBRATION_END
};

void mmdvfs_prepare_action(enum mmdvfs_prepare_event event);

#endif /* __MMDVFS_PMQOS_H__ */

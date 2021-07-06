/* SPDX-License-Identifier: GPL-2.0 OR MIT */

/*
 * (C) COPYRIGHT 2020 Arm Limited or its affiliates. All rights reserved.
 */

/**
 * @file
 * Part of the Mali arbiter interface related to platform operations.
 */

#ifndef _MALI_ARB_PLAT_H_
#define _MALI_ARB_PLAT_H_

#define MALI_ARB_PLAT_DT_NAME "arm,mali-arbiter-platform"

struct mali_arb_plat_dev;

/**
 * struct mali_arb_plat_arbiter_cb_ops - callback functions provided by the
 * Arbiter for the platform integrator
 */
struct mali_arb_plat_arbiter_cb_ops {
	/**
	 * dvfs_arb_get_utilisation() - Request GPU utilization
	 * @dev: the arbiter device (passed when we registered).
	 *
	 * Called by a platform integration device to request GPU
	 * utilization since previous call.  The Arbiter should
	 * return the busytime and availtime since last call
	 */
	void (*dvfs_arb_get_utilisation)(struct device *dev, u32 *busytime,
		u32 *availtime);
};

/**
 * struct mali_arb_plat_ops - callback functions provided by the
 * Arbiter for the platform integration device
 */
struct mali_arb_plat_ops {
	/**
	 * mali_arb_plat_register() - Register an arbiter device
	 * @plat_dev: Platform integration device obtained using device tree
	 * @dev: The Arbiter device
	 * @arbiter_cb_ops: Arbiter Callbacks operation struct
	 *
	 * This function must be called before initializing the platform device
	 * The platform device will then ask the arbiter for utilization info
	 * to make decisions about frequency changes
	 *
	 * Return: 0 is successful, or a standard Linux error code
	 */
	int (*mali_arb_plat_register)(struct mali_arb_plat_dev *plat_dev,
			struct device *dev,
			struct mali_arb_plat_arbiter_cb_ops *arbiter_cb_ops);

	/**
	 * mali_arb_plat_unregister() - Unregister an arbiter device
	 * @plat_dev: Platform integration device
	 *
	 * This function must be called as last operation on the platform device
	 * The platform device will then be unregistered by the system and
	 * all its resources cleaned up
	 */
	void (*mali_arb_plat_unregister)(struct mali_arb_plat_dev *plat_dev);

	/**
	 * mali_arb_start_dvfs() - Register the device with devfreq framework
	 * @plat_dev: Platform integration device
	 *
	 * Read device tree and register with the devfreq framework
	 * see @kbase_devfreq_init and @power_control_init
	 */
	int (*mali_arb_start_dvfs)(struct mali_arb_plat_dev *plat_dev);

	/**
	 * mali_arb_stop_dvfs() - Unregister the device with devfreq framework
	 * @plat_dev: Platform integration device
	 *
	 * Unregister the device from the devfreq framework and
	 * release the resources
	 */
	void (*mali_arb_stop_dvfs)(struct mali_arb_plat_dev *plat_dev);

	/**
	 * mali_arb_gpu_power_on() - Power on the GPU
	 * @plat_dev: Platform integration device
	 *
	 * Enable the GPU power to the device
	 */
	void (*mali_arb_gpu_power_on)(struct mali_arb_plat_dev *plat_dev);

	/**
	 * mali_arb_gpu_power_off() - Power off the GPU
	 * @plat_dev: The arbiter platform device
	 *
	 * Disable the GPU power of the device
	 */
	void (*mali_arb_gpu_power_off)(struct mali_arb_plat_dev *plats_dev);
};

/**
 * struct mali_arb_plat_dev - Platform integration device data
 *                            (use platform_get_drvdata)
 * @plat_ops:  Platform integration device functions.
 *
 * Struct which contain the arbiter platform device and used by the arbiter
 * as interface to trigger platform specific (PM related) operations
 */
struct mali_arb_plat_dev {
	struct mali_arb_plat_ops plat_ops;
};

#endif /* _MALI_ARB_PLAT_H_ */

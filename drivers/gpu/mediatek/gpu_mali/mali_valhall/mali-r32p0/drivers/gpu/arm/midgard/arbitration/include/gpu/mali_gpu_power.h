/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT */

/*
 * (C) COPYRIGHT 2019-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * Part of the Mali arbiter interface related to GPU power operations.
 */

#ifndef _MALI_GPU_POWER_H_
#define _MALI_GPU_POWER_H_

/*
 * This define specifies the current version of the gpu power interface.
 * Whenever the gpu power changes in its interface, so that integration effort
 * is required, the version number will be increased.
 * Each module which interact with the arbiter must make an effort
 * to check that it implements the correct version.
 *
 * Version history:
 * 1 - First gpu power interface versioned
 */
#define MALI_GPU_POWER_VERSION 1

/* Forward Declarations */
struct mali_gpu_power_data;

/**
 * struct mali_gpu_power_arbiter_cb_ops - callback functions provided by the
 *                                        arbiter to the GPU power device
 */
struct mali_gpu_power_arbiter_cb_ops {
	/**
	 * @arb_get_utilisation: Request GPU utilization
	 * @priv: the arbiter private data (passed during registration)
	 * @gpu_busytime: will contain the GPU busy time
	 * @gpu_totaltime: will contain the total time assigned to GPU
	 *
	 * Called by a GPU power integration device to request GPU
	 * utilization since previous call.  The Arbiter should
	 * return the busytime and availtime since last call
	 */
	void (*arb_get_utilisation)(void *priv, u32 *busytime, u32 *availtime);

	/**
	 * @update_freq: Get notified on frequency change
	 * @priv: the arbiter private data (passed during registration)
	 * @new_freq: updated frequency in kHz
	 *
	 * Notify the subscriber with the new frequency selected
	 * for the GPU by a change of its operating point.
	 *
	 */
	void (*update_freq)(void *priv, u32 new_freq);
};

/**
 * struct mali_gpu_power_ops - callback functions provided by the
 * GPU power integration device for use by the arbiter
 */
struct mali_gpu_power_ops {
	/**
	 * @register_arb: Register an arbiter device
	 * @dev: GPU Power device
	 * @priv: the arbiter private data (passed during registration)
	 * @arbiter_cb_ops: arbiter Callbacks operation struct
	 * @arbiter_id: arbiter id assigned by the GPU power
	 *
	 * This function register an arbiter with the GPU power device and
	 * shall be called before performing any interaction between the
	 * arbiter and this device itself.
	 * After registration, the GPU power device will then ask to the arbiter
	 * utilization info to make decisions about frequency changes
	 *
	 * Return: 0 is successful, or a standard Linux error code
	 */
	int (*register_arb)(struct device *dev, void *priv,
			struct mali_gpu_power_arbiter_cb_ops *arbiter_cb_ops,
			u32 *arbiter_id);

	/**
	 * @unregister_arb: Unregister an arbiter device
	 * @dev: GPU Power device
	 * @arbiter_id: arbiter id assigned by the GPU power
	 *
	 * This function must be called as last operation by the arbiter to
	 * terminate any further interaction with the GPU power device.
	 * The GPU power device will then unregister the arbiter and clean up
	 * the internal resources associated to it.
	 * After this call the arbiter should not attempt to interact with the
	 * GPU power device.
	 */
	void (*unregister_arb)(struct device *dev, u32 arbiter_id);

	/**
	 * @gpu_active: Activate the GPU
	 * @dev: GPU Power device
	 *
	 * After this call, the arbiter expect the GPU to be powered.
	 */
	void (*gpu_active)(struct device *dev);

	/**
	 * @gpu_idle: Deactivate the GPU
	 * @dev: GPU Power device
	 *
	 * After this call the GPU may not be powered and could be
	 * power off.
	 * NOTE: this function does not explicitly need to power down the GPU
	 * which can therefore be left ON according implementation needs
	 */
	void (*gpu_idle)(struct device *dev);
};

/**
 * struct mali_gpu_power_data - GPU power integration device data
 * @ops: GPU power integration device functions.
 *
 * Struct which contains the public GPU power integration device data registered
 * by the GPU power driver and used by the arbiter as interface to trigger
 * platform specific (PM related) operations.
 * This structure can be expanded with other field which will be visible
 * to the arbiter as well.
 * The mali GPU power driver must set it as its device data on any platform
 * config devices using dev_set_drvdata() so that can be retrieved by modules
 * interfacing with the device using dev_get_drvdata().
 */
struct mali_gpu_power_data {
	struct mali_gpu_power_ops ops;
};

#endif /* _MALI_GPU_POWER_H_ */

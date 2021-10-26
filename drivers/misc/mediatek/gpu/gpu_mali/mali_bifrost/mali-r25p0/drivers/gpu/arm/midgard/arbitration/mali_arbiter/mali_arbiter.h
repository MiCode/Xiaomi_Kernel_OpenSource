/* SPDX-License-Identifier: GPL-2.0 OR MIT */

/*
 * (C) COPYRIGHT 2020 Arm Limited or its affiliates. All rights reserved.
 */

/**
 * @file
 * Part of the Mali reference arbiter
 */

#ifndef _MALI_ARBITER_H_
#define _MALI_ARBITER_H_

/* Forward Declarations - see mali_arbiter.c */
struct mali_arb_dom;
struct mali_arb_dev;

/* Device tree compatible ID of the Arbiter */
#define MALI_ARBITER_DT_NAME "arm,mali-arbiter"

/**
 * struct mali_arb_dom_info - Connected domain information
 * @dev:        Comms device associated with connected domain.
 * @domain_id:  Domain ID of guest (can be 0 for host).
 * @flags:      Flags (for example ARB_DOMAIN_FLAG_GPU_RESETTING)
 */
struct mali_arb_dom_info {
	struct device *dev;
	u32 domain_id;
	u32 flags;
	/**
	 * arb_vm_gpu_granted() - GPU is granted to domain
	 * @dev: Domain device (passed in during registration)
	 *
	 * Callback to notify VM that the GPU can be used
	 */
	void (*arb_vm_gpu_granted)(struct device *dev);

	/**
	 * arb_vm_gpu_stop() - Domain must stop using GPU
	 * @dev: Domain device (passed in during registration)
	 *
	 * Callback to notify VM that the GPU should no longer be used.
	 */
	void (*arb_vm_gpu_stop)(struct device *dev);

	/**
	 * arb_vm_gpu_lost() - Domain must stop using GPU
	 * @dev: Domain device (passed in during registration)
	 *
	 * Callback to notify VM that something has gone wrong and the GPU
	 * has been lost.  The VM should attempt recovery by failing all
	 * outstanding work and then issue a fresh request for the GPU
	 * when ready.
	 */
	void (*arb_vm_gpu_lost)(struct device *dev);
};

/**
 * struct mali_arb_hyp_callbacks - Connected domain information
 * @dev:        Comms device associated with connected domain.
 * @domain_id:  Domain ID of guest (can be 0 for host).
 * @flags:      Flags (for example ARB_DOMAIN_FLAG_GPU_RESETTING)
 */
struct mali_arb_hyp_callbacks {
	/**
	 * arb_hyp_assign_vm_gpu() - Assign GPU to given domain.
	 * @ctx: Caller context (not used by Arbiter)
	 * @domain_id: Domain ID to assign the GPU
	 *
	 * This is an optimized hypercall and can assume that the
	 * VM has cleaned up, powered down the GPU and released any IRQs.
	 * The hypercall should reset the Mali before returning.
	 *
	 * Return: 0 is successful, or a standard Linux error code
	 */
	int (*arb_hyp_assign_vm_gpu)(void *ctx, u32 domain_id);

	/**
	 * arb_hyp_force_assign_vm_gpu() - Forcibly re-assign GPU to given
	 *                                 domain.
	 * @ctx: Caller context (not used by Arbiter)
	 * @domain_id: Domain ID to assign the GPU
	 *
	 * This function should assume the VM is still using the
	 * GPU and will need to do a full cleanup, forcibly remove IRQs and
	 * map dummy RAM pages in place of the real physical Mali registers
	 * (to prevent the VM from faulting).  The hypercall should reset
	 * the Mali before returning.
	 *
	 * @Note: This function should not fail or the Arbiter will not
	 * be able to continue sharing the GPU.
	 *
	 * Return: 0 is successful, or a standard Linux error code
	 */
	int (*arb_hyp_force_assign_vm_gpu)(void *ctx, u32 domain_id);
};

#if MALI_ARBITER_TEST_API

/* Forward declaration for testing purposes */
struct mali_arb_plat_dev;

/**
 * struct mali_arb_test_callbacks - Test functions provided to the Arbiter
 *                                  to verify hypercall and platform
 *                                  functionality (test builds only)
 */
struct mali_arb_test_callbacks {
	/**
	 * arb_test_power_on() - Replace gpu_power_on platform call to verify
	 * arbiter behavior (test builds only)
	 * @plat_dev: Not used when testing
	 */
	void (*arb_test_power_on)(struct mali_arb_plat_dev *plat_dev);

	/**
	 * arb_test_power_off() - Replace gpu_power_on platform call to verify
	 * arbiter behavior (test builds only)
	 * @plat_dev: Not used when testing
	 */
	void (*arb_test_power_off)(struct mali_arb_plat_dev *plat_dev);
};
#endif /* MALI_ARBITER_TEST_API */

/**
 * struct mali_arb_ops - Functions provided by the Arbiter
 */
struct mali_arb_ops {
	/**
	 * vm_arb_register_hyp() - Register hypervisor callbacks for use by
	 *                         the Arbiter.
	 * @arb_dev: Arbiter device
	 * @ctx: Caller context (not used by the Arbiter)
	 * @cbs: Function callbacks
	 *
	 * Return: 0 is successful, or a standard Linux error code
	 */
	int (*vm_arb_register_hyp)(struct mali_arb_dev *arb_dev,
		void *ctx,
		const struct mali_arb_hyp_callbacks *cbs);

	/**
	 * vm_arb_unregister_hyp() - Unregister hypervisor callbacks
	 * @arb_dev: Arbiter device
	 *
	 * The arbiter will ensure that the hypervisor callbacks are not called
	 * once this function has been completed.
	 */
	void (*vm_arb_unregister_hyp)(struct mali_arb_dev *arb_dev);

	/**
	 * vm_arb_register_dom() - Register a domain comms device
	 * @arb_dev: Arbiter device obtained using device tree
	 * @info: The domain info structure populated by caller
	 * @dom_out: On success, a pointer to a domain structure.
	 *
	 * This function must be called first before a domain comms device
	 * instance can use the arbiter functionality.
	 *
	 * Return: 0 is successful, or a standard Linux error code
	 */
	int (*vm_arb_register_dom)(
		struct mali_arb_dev *arb_dev,
		struct mali_arb_dom_info *info,
		struct mali_arb_dom **dom_out);

	/**
	 * vm_arb_unregister_dom() - Unregister a domain comms device
	 * @dom: Domain structure returned by registration.
	 *
	 * This function must be called before the domain comms device
	 * is unloaded to sever the connection.
	 */
	void (*vm_arb_unregister_dom)(struct mali_arb_dom *dom);

	/**
	 * vm_arb_gpu_request() - Request GPU time
	 * @dom: Domain structure returned by registration.
	 *
	 * Called by a domain comms device to request GPU time.  The
	 * Arbiter should respond when ready with a callback to
	 * #arb_vm_gpu_granted.
	 */
	void (*vm_arb_gpu_request)(struct mali_arb_dom *dom);

	/**
	 * vm_arb_gpu_active() - Notify Arbiter that GPU is being used
	 * @dom: Domain structure returned by registration.
	 *
	 * Called by a domain comms device to notify the Arbiter that
	 * the VM is busy using the GPU.  This information can be useful
	 * in making scheduling decisions.
	 */
	void (*vm_arb_gpu_active)(struct mali_arb_dom *dom);

	/**
	 * vm_arb_gpu_idle() - Notify Arbiter that GPU is not being used
	 * @dom: Domain structure returned by registration.
	 *
	 * Called by a domain comms device to notify the Arbiter that
	 * the VM is no longer using the GPU.  This information can be
	 * useful in making scheduling decisions.
	 */
	void (*vm_arb_gpu_idle)(struct mali_arb_dom *dom);

	/**
	 * vm_arb_gpu_stopped() - VM KBase driver is in stopped state
	 *                        GPU is not being used
	 * @dom: Domain structure returned by registration.
	 * @req_again: VM has work pending and still wants the GPU
	 *
	 * VM must no longer use the GPU once this function has been
	 * called until #arb_vm_gpu_granted has been called by the
	 * Arbiter.
	 */
	void (*vm_arb_gpu_stopped)(struct mali_arb_dom *dom, bool req_again);

#if MALI_ARBITER_TEST_API
	/**
	 * vm_arb_test_set_callbacks() - Set test framework callbacks
	 * @arb_dev: Arbiter device
	 * @cbs: Function callbacks
	 *
	 * Use callbacks into test framework instead of making
	 * the real hyper and platform calls (test builds only)
	 */
	void (*vm_arb_test_set_callbacks)(struct mali_arb_dev *arb_dev,
		struct mali_arb_test_callbacks *cbs);

	/**
	 * vm_arb_test_utilisation() - Get utilization data test callback
	 * @arb_dev: Arbiter device obtained using device tree
	 * @gpu_busytime: time the GPU has been active since last reported
	 * @gpu_totaltime: total time elapsed since last reported
	 *
	 * Callback function to request utilization data from Arbiter
	 */
	void (*vm_arb_test_utilisation)(struct mali_arb_dev *arb_dev,
		u32 *gpu_busytime, u32 *gpu_totaltime);
#endif /* MALI_ARBITER_TEST_API */
};

/**
 * struct mali_arb_dev - Arbiter device data (use platform_get_drvdata)
 * @ops:  Arbiter functions.
 */
struct mali_arb_dev {
	struct mali_arb_ops ops;
};

#endif /* _MALI_ARBITER_H_ */

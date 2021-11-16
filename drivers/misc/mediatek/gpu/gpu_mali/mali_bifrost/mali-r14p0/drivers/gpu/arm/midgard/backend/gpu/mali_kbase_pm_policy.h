/*
 *
 * (C) COPYRIGHT 2010-2015, 2018 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

/*
 * Power policy API definitions
 */

#ifndef _KBASE_PM_POLICY_H_
#define _KBASE_PM_POLICY_H_

/**
 * kbase_pm_policy_init - Initialize power policy framework
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Must be called before calling any other policy function
 *
 * Return: 0 if the power policy framework was successfully
 *         initialized, -errno otherwise.
 */
int kbase_pm_policy_init(struct kbase_device *kbdev);

/**
 * kbase_pm_policy_term - Terminate power policy framework
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 */
void kbase_pm_policy_term(struct kbase_device *kbdev);

/**
 * kbase_pm_update_active - Update the active power state of the GPU
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Calls into the current power policy
 */
void kbase_pm_update_active(struct kbase_device *kbdev);

/**
 * kbase_pm_update_cores - Update the desired core state of the GPU
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Calls into the current power policy
 */
void kbase_pm_update_cores(struct kbase_device *kbdev);


enum kbase_pm_cores_ready {
	KBASE_CORES_NOT_READY = 0,
	KBASE_NEW_AFFINITY = 1,
	KBASE_CORES_READY = 2
};


/**
 * kbase_pm_request_cores - Request the desired cores to be powered up.
 * @kbdev:           Kbase device
 * @tiler_required:  true if tiler is required
 * @shader_required: true if shaders are required
 *
 * Called by the scheduler to request power to the desired cores.
 *
 * There is no guarantee that the HW will be powered up on return. Use
 * kbase_pm_cores_requested()/kbase_pm_cores_ready() to verify that cores are
 * now powered, or instead call kbase_pm_request_cores_sync().
 */
void kbase_pm_request_cores(struct kbase_device *kbdev, bool tiler_required,
		bool shader_required);

/**
 * kbase_pm_request_cores_sync - Synchronous variant of kbase_pm_request_cores()
 * @kbdev:           Kbase device
 * @tiler_required:  true if tiler is required
 * @shader_required: true if shaders are required
 *
 * When this function returns, the @shader_cores will be in the READY state.
 *
 * This is safe variant of kbase_pm_check_transitions_sync(): it handles the
 * work of ensuring the requested cores will remain powered until a matching
 * call to kbase_pm_unrequest_cores()/kbase_pm_release_cores() (as appropriate)
 * is made.
 */
void kbase_pm_request_cores_sync(struct kbase_device *kbdev,
		bool tiler_required, bool shader_required);

/**
 * kbase_pm_release_cores - Request the desired cores to be powered down.
 * @kbdev:           Kbase device
 * @tiler_required:  true if tiler is required
 * @shader_required: true if shaders are required
 *
 * Called by the scheduler to release its power reference on the desired cores.
 */
void kbase_pm_release_cores(struct kbase_device *kbdev, bool tiler_required,
		bool shader_required);

/**
 * kbase_pm_cores_requested - Check that a power request has been locked into
 *                            the HW.
 * @kbdev:           Kbase device
 * @tiler_required:  true if tiler is required
 * @shader_required: true if shaders are required
 *
 * Called by the scheduler to check if a power on request has been locked into
 * the HW.
 *
 * Note that there is no guarantee that the cores are actually ready, however
 * when the request has been locked into the HW, then it is safe to submit work
 * since the HW will wait for the transition to ready.
 *
 * A reference must first be taken prior to making this call.
 *
 * Caller must hold the hwaccess_lock.
 *
 * Return: true if the request to the HW was successfully made else false if the
 *         request is still pending.
 */
static inline bool kbase_pm_cores_requested(struct kbase_device *kbdev,
		bool tiler_required, bool shader_required)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

	if ((shader_required && !kbdev->shader_available_bitmap) ||
			(tiler_required && !kbdev->tiler_available_bitmap))
		return false;

	return true;
}

/**
 * kbase_pm_cores_ready -  Check that the required cores have been powered on by
 *                         the HW.
 * @kbdev:           Kbase device
 * @tiler_required:  true if tiler is required
 * @shader_required: true if shaders are required
 *
 * Called by the scheduler to check if cores are ready.
 *
 * Note that the caller should ensure that they have first requested cores
 * before calling this function.
 *
 * Caller must hold the hwaccess_lock.
 *
 * Return: true if the cores are ready.
 */
static inline bool kbase_pm_cores_ready(struct kbase_device *kbdev,
		bool tiler_required, bool shader_required)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

	if ((shader_required && !kbdev->shader_ready_bitmap) ||
			(tiler_required && !kbdev->tiler_available_bitmap))
		return false;

	return true;
}

/**
 * kbase_pm_request_l2_caches - Request l2 caches
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Request the use of l2 caches for all core groups, power up, wait and prevent
 * the power manager from powering down the l2 caches.
 *
 * This tells the power management that the caches should be powered up, and
 * they should remain powered, irrespective of the usage of shader cores. This
 * does not return until the l2 caches are powered up.
 *
 * The caller must call kbase_pm_release_l2_caches() when they are finished
 * to allow normal power management of the l2 caches to resume.
 *
 * This should only be used when power management is active.
 */
void kbase_pm_request_l2_caches(struct kbase_device *kbdev);

/**
 * kbase_pm_request_l2_caches_nolock - Request l2 caches, nolock version
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Request the use of l2 caches for all core groups and power up without
 * waiting for power manager to actually power up the cores. This is done
 * because the call to this function is done from within the atomic context
 * and the actual l2 caches being powered up is checked at a later stage.
 * The reference taken on l2 caches is removed when the protected mode atom
 * is released so there is no need to make a call to a matching
 * release_l2_caches().
 *
 * This function is used specifically for the case when l2 caches are
 * to be powered up as part of the sequence for entering protected mode.
 *
 * This should only be used when power management is active.
 */
void kbase_pm_request_l2_caches_nolock(struct kbase_device *kbdev);

/**
 * kbase_pm_request_l2_caches_l2_is_on - Request l2 caches but don't power on
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Increment the count of l2 users but do not attempt to power on the l2
 *
 * It is the callers responsibility to ensure that the l2 is already powered up
 * and to eventually call kbase_pm_release_l2_caches()
 */
void kbase_pm_request_l2_caches_l2_is_on(struct kbase_device *kbdev);

/**
 * kbase_pm_release_l2_caches - Release l2 caches
 *
 * @kbdev: The kbase device structure for the device (must be a valid pointer)
 *
 * Release the use of l2 caches for all core groups and allow the power manager
 * to power them down when necessary.
 *
 * This tells the power management that the caches can be powered down if
 * necessary, with respect to the usage of shader cores.
 *
 * The caller must have called kbase_pm_request_l2_caches() prior to a call
 * to this.
 *
 * This should only be used when power management is active.
 */
void kbase_pm_release_l2_caches(struct kbase_device *kbdev);

#endif /* _KBASE_PM_POLICY_H_ */

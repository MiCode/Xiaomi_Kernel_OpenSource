/*
 *
 * (C) COPYRIGHT 2010-2018 ARM Limited. All rights reserved.
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
 * Power policy API implementations
 */

#include <mali_kbase.h>
#include <mali_midg_regmap.h>
#include <mali_kbase_pm.h>
#include <mali_kbase_config_defaults.h>
#include <backend/gpu/mali_kbase_pm_internal.h>

static const struct kbase_pm_policy *const policy_list[] = {
#ifdef CONFIG_MALI_NO_MALI
	&kbase_pm_always_on_policy_ops,
	&kbase_pm_demand_policy_ops,
	&kbase_pm_coarse_demand_policy_ops,
#if !MALI_CUSTOMER_RELEASE
	&kbase_pm_demand_always_powered_policy_ops,
	&kbase_pm_fast_start_policy_ops,
#endif
#else				/* CONFIG_MALI_NO_MALI */
#if !PLATFORM_POWER_DOWN_ONLY
	&kbase_pm_demand_policy_ops,
#endif /* !PLATFORM_POWER_DOWN_ONLY */
	&kbase_pm_coarse_demand_policy_ops,
	&kbase_pm_always_on_policy_ops,
#if !MALI_CUSTOMER_RELEASE
#if !PLATFORM_POWER_DOWN_ONLY
	&kbase_pm_demand_always_powered_policy_ops,
	&kbase_pm_fast_start_policy_ops,
#endif /* !PLATFORM_POWER_DOWN_ONLY */
#endif
#endif /* CONFIG_MALI_NO_MALI */
};

/* The number of policies available in the system.
 * This is derived from the number of functions listed in policy_get_functions.
 */
#define POLICY_COUNT (sizeof(policy_list)/sizeof(*policy_list))


/* Function IDs for looking up Timeline Trace codes in
 * kbase_pm_change_state_trace_code */
enum kbase_pm_func_id {
	KBASE_PM_FUNC_ID_REQUEST_CORES_START,
	KBASE_PM_FUNC_ID_REQUEST_CORES_END,
	KBASE_PM_FUNC_ID_RELEASE_CORES_START,
	KBASE_PM_FUNC_ID_RELEASE_CORES_END,
	/* Note: kbase_pm_unrequest_cores() is on the slow path, and we neither
	 * expect to hit it nor tend to hit it very much anyway. We can detect
	 * whether we need more instrumentation by a difference between
	 * PM_CHECKTRANS events and PM_SEND/HANDLE_EVENT. */

	/* Must be the last */
	KBASE_PM_FUNC_ID_COUNT
};


/* State changes during request/unrequest/release-ing cores */
enum {
	KBASE_PM_CHANGE_STATE_SHADER = (1u << 0),
	KBASE_PM_CHANGE_STATE_TILER  = (1u << 1),

	/* These two must be last */
	KBASE_PM_CHANGE_STATE_MASK = (KBASE_PM_CHANGE_STATE_TILER |
						KBASE_PM_CHANGE_STATE_SHADER),
	KBASE_PM_CHANGE_STATE_COUNT = KBASE_PM_CHANGE_STATE_MASK + 1
};
typedef u32 kbase_pm_change_state;

/**
 * kbasep_pm_do_poweroff_cores - Process a poweroff request and power down any
 *                               requested shader cores
 * @kbdev: Device pointer
 */
static void kbasep_pm_do_poweroff_cores(struct kbase_device *kbdev)
{
	u64 prev_shader_state = kbdev->pm.backend.desired_shader_state;
	u64 prev_tiler_state = kbdev->pm.backend.desired_tiler_state;

	lockdep_assert_held(&kbdev->hwaccess_lock);

	kbdev->pm.backend.desired_shader_state &=
			~kbdev->pm.backend.shader_poweroff_pending;
	kbdev->pm.backend.desired_tiler_state &=
			~kbdev->pm.backend.tiler_poweroff_pending;

	kbdev->pm.backend.shader_poweroff_pending = 0;
	kbdev->pm.backend.tiler_poweroff_pending = 0;

	if (prev_shader_state != kbdev->pm.backend.desired_shader_state ||
			prev_tiler_state !=
				kbdev->pm.backend.desired_tiler_state ||
			kbdev->pm.backend.ca_in_transition) {
		bool cores_are_available;

		cores_are_available = kbase_pm_check_transitions_nolock(kbdev);

		/* Don't need 'cores_are_available',
		 * because we don't return anything */
		CSTD_UNUSED(cores_are_available);
	}
}

static enum hrtimer_restart
kbasep_pm_do_gpu_poweroff_callback(struct hrtimer *timer)
{
	struct kbase_device *kbdev;
	unsigned long flags;

	kbdev = container_of(timer, struct kbase_device,
						pm.backend.gpu_poweroff_timer);

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);

	/* It is safe for this call to do nothing if the work item is already
	 * queued. The worker function will read the must up-to-date state of
	 * kbdev->pm.backend.gpu_poweroff_pending under lock.
	 *
	 * If a state change occurs while the worker function is processing,
	 * this call will succeed as a work item can be requeued once it has
	 * started processing.
	 */
	if (kbdev->pm.backend.gpu_poweroff_pending)
		queue_work(kbdev->pm.backend.gpu_poweroff_wq,
					&kbdev->pm.backend.gpu_poweroff_work);

	if (kbdev->pm.backend.shader_poweroff_pending ||
			kbdev->pm.backend.tiler_poweroff_pending) {
		kbdev->pm.backend.shader_poweroff_pending_time--;

		KBASE_DEBUG_ASSERT(
				kbdev->pm.backend.shader_poweroff_pending_time
									>= 0);

		if (!kbdev->pm.backend.shader_poweroff_pending_time)
			kbasep_pm_do_poweroff_cores(kbdev);
	}

	if (kbdev->pm.backend.poweroff_timer_needed) {
		spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

		hrtimer_add_expires(timer, kbdev->pm.gpu_poweroff_time);

		return HRTIMER_RESTART;
	}

	kbdev->pm.backend.poweroff_timer_running = false;
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	return HRTIMER_NORESTART;
}

static void kbasep_pm_do_gpu_poweroff_wq(struct work_struct *data)
{
	unsigned long flags;
	struct kbase_device *kbdev;
	bool do_poweroff = false;

	kbdev = container_of(data, struct kbase_device,
						pm.backend.gpu_poweroff_work);

	mutex_lock(&kbdev->pm.lock);

	if (kbdev->pm.backend.gpu_poweroff_pending == 0) {
		mutex_unlock(&kbdev->pm.lock);
		return;
	}

	kbdev->pm.backend.gpu_poweroff_pending--;

	if (kbdev->pm.backend.gpu_poweroff_pending > 0) {
		mutex_unlock(&kbdev->pm.lock);
		return;
	}

	KBASE_DEBUG_ASSERT(kbdev->pm.backend.gpu_poweroff_pending == 0);

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);

	/* Only power off the GPU if a request is still pending */
	if (!kbdev->pm.backend.pm_current_policy->get_core_active(kbdev))
		do_poweroff = true;

	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	if (do_poweroff) {
		kbdev->pm.backend.poweroff_timer_needed = false;
		hrtimer_cancel(&kbdev->pm.backend.gpu_poweroff_timer);
		kbdev->pm.backend.poweroff_timer_running = false;

		/* Power off the GPU */
		kbase_pm_do_poweroff(kbdev, false);
	}

	mutex_unlock(&kbdev->pm.lock);
}

int kbase_pm_policy_init(struct kbase_device *kbdev)
{
	struct workqueue_struct *wq;

	wq = alloc_workqueue("kbase_pm_do_poweroff",
			WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!wq)
		return -ENOMEM;

	kbdev->pm.backend.gpu_poweroff_wq = wq;
	INIT_WORK(&kbdev->pm.backend.gpu_poweroff_work,
			kbasep_pm_do_gpu_poweroff_wq);
	hrtimer_init(&kbdev->pm.backend.gpu_poweroff_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	kbdev->pm.backend.gpu_poweroff_timer.function =
			kbasep_pm_do_gpu_poweroff_callback;
	kbdev->pm.backend.pm_current_policy = policy_list[0];
	kbdev->pm.backend.pm_current_policy->init(kbdev);
	kbdev->pm.gpu_poweroff_time =
			HR_TIMER_DELAY_NSEC(DEFAULT_PM_GPU_POWEROFF_TICK_NS);
	kbdev->pm.poweroff_shader_ticks = DEFAULT_PM_POWEROFF_TICK_SHADER;
	kbdev->pm.poweroff_gpu_ticks = DEFAULT_PM_POWEROFF_TICK_GPU;

	return 0;
}

void kbase_pm_policy_term(struct kbase_device *kbdev)
{
	kbdev->pm.backend.pm_current_policy->term(kbdev);
	destroy_workqueue(kbdev->pm.backend.gpu_poweroff_wq);
}

void kbase_pm_cancel_deferred_poweroff(struct kbase_device *kbdev)
{
	unsigned long flags;

	lockdep_assert_held(&kbdev->pm.lock);

	kbdev->pm.backend.poweroff_timer_needed = false;
	hrtimer_cancel(&kbdev->pm.backend.gpu_poweroff_timer);
	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	kbdev->pm.backend.poweroff_timer_running = false;

	/* If wq is already running but is held off by pm.lock, make sure it has
	 * no effect */
	kbdev->pm.backend.gpu_poweroff_pending = 0;

	kbdev->pm.backend.shader_poweroff_pending = 0;
	kbdev->pm.backend.tiler_poweroff_pending = 0;
	kbdev->pm.backend.shader_poweroff_pending_time = 0;

	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
}

void kbase_pm_update_active(struct kbase_device *kbdev)
{
	struct kbase_pm_device_data *pm = &kbdev->pm;
	struct kbase_pm_backend_data *backend = &pm->backend;
	unsigned long flags;
	bool active;

	lockdep_assert_held(&pm->lock);

	/* pm_current_policy will never be NULL while pm.lock is held */
	KBASE_DEBUG_ASSERT(backend->pm_current_policy);

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);

	active = backend->pm_current_policy->get_core_active(kbdev);
	WARN((kbase_pm_is_active(kbdev) && !active),
		"GPU is active but policy '%s' is indicating that it can be powered off",
		kbdev->pm.backend.pm_current_policy->name);

	if (active) {
		if (backend->gpu_poweroff_pending) {
			/* Cancel any pending power off request */
			backend->gpu_poweroff_pending = 0;

			/* If a request was pending then the GPU was still
			 * powered, so no need to continue */
			if (!kbdev->poweroff_pending) {
				spin_unlock_irqrestore(&kbdev->hwaccess_lock,
						flags);
				return;
			}
		}

		if (!backend->poweroff_timer_running && !backend->gpu_powered &&
				(pm->poweroff_gpu_ticks ||
				pm->poweroff_shader_ticks)) {
			backend->poweroff_timer_needed = true;
			backend->poweroff_timer_running = true;
			hrtimer_start(&backend->gpu_poweroff_timer,
					pm->gpu_poweroff_time,
					HRTIMER_MODE_REL);
		}

		/* Power on the GPU and any cores requested by the policy */
		if (pm->backend.poweroff_wait_in_progress) {
			KBASE_DEBUG_ASSERT(kbdev->pm.backend.gpu_powered);
			pm->backend.poweron_required = true;
			spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
		} else {
			spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
			kbase_pm_do_poweron(kbdev, false);
		}
	} else {
		/* It is an error for the power policy to power off the GPU
		 * when there are contexts active */
		KBASE_DEBUG_ASSERT(pm->active_count == 0);

		if (backend->shader_poweroff_pending ||
				backend->tiler_poweroff_pending) {
			backend->shader_poweroff_pending = 0;
			backend->tiler_poweroff_pending = 0;
			backend->shader_poweroff_pending_time = 0;
		}

		/* Request power off */
		if (pm->backend.gpu_powered) {
			if (pm->poweroff_gpu_ticks) {
				backend->gpu_poweroff_pending =
						pm->poweroff_gpu_ticks;
				backend->poweroff_timer_needed = true;
				if (!backend->poweroff_timer_running) {
					/* Start timer if not running (eg if
					 * power policy has been changed from
					 * always_on to something else). This
					 * will ensure the GPU is actually
					 * powered off */
					backend->poweroff_timer_running
							= true;
					hrtimer_start(
						&backend->gpu_poweroff_timer,
						pm->gpu_poweroff_time,
						HRTIMER_MODE_REL);
				}
				spin_unlock_irqrestore(&kbdev->hwaccess_lock,
						flags);
			} else {
				spin_unlock_irqrestore(&kbdev->hwaccess_lock,
						flags);

				/* Power off the GPU immediately */
				kbase_pm_do_poweroff(kbdev, false);
			}
		} else {
			spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
		}
	}
}

/**
 * get_desired_shader_bitmap - Get the desired shader bitmap, based on the
 *                             current power policy
 *
 * @kbdev: The kbase device structure for the device
 *
 * Queries the current power policy to determine if shader cores will be
 * required in the current state, and apply any HW workarounds.
 *
 * Return: bitmap of desired shader cores
 */

static u64 get_desired_shader_bitmap(struct kbase_device *kbdev)
{
	u64 desired_bitmap = 0u;

	lockdep_assert_held(&kbdev->hwaccess_lock);

	if (kbdev->pm.backend.pm_current_policy->shaders_needed(kbdev))
		desired_bitmap = kbase_pm_ca_get_core_mask(kbdev);

	WARN(!desired_bitmap && kbdev->shader_needed_cnt,
			"Shader cores are needed but policy '%s' did not make them needed",
			kbdev->pm.backend.pm_current_policy->name);

	if (!kbase_hw_has_feature(kbdev, BASE_HW_FEATURE_XAFFINITY)) {
		/* Unless XAFFINITY is supported, enable core 0 if tiler
		 * required, regardless of core availability
		 */
		if (kbdev->tiler_needed_cnt > 0)
			desired_bitmap |= 1;
	}

	return desired_bitmap;
}

void kbase_pm_update_cores_state_nolock(struct kbase_device *kbdev)
{
	u64 desired_bitmap;
	u64 desired_tiler_bitmap;
	bool cores_are_available;
	bool do_poweroff = false;

	lockdep_assert_held(&kbdev->hwaccess_lock);

	if (kbdev->pm.backend.pm_current_policy == NULL)
		return;
	if (kbdev->pm.backend.poweroff_wait_in_progress)
		return;

	if (kbdev->protected_mode_transition && !kbdev->shader_needed_cnt &&
			!kbdev->tiler_needed_cnt) {
		/* We are trying to change in/out of protected mode - force all
		 * cores off so that the L2 powers down */
		desired_bitmap = 0;
		desired_tiler_bitmap = 0;
	} else {
		desired_bitmap = get_desired_shader_bitmap(kbdev);

		if (kbdev->tiler_needed_cnt > 0)
			desired_tiler_bitmap = 1;
		else
			desired_tiler_bitmap = 0;
	}

	if (kbdev->pm.backend.desired_shader_state != desired_bitmap)
		KBASE_TRACE_ADD(kbdev, PM_CORES_CHANGE_DESIRED, NULL, NULL, 0u,
							(u32)desired_bitmap);
	/* Are any cores being powered on? */
	if (~kbdev->pm.backend.desired_shader_state & desired_bitmap ||
	    ~kbdev->pm.backend.desired_tiler_state & desired_tiler_bitmap ||
	    kbdev->pm.backend.ca_in_transition) {
		/* Check if we are powering off any cores before updating shader
		 * state */
		if (kbdev->pm.backend.desired_shader_state & ~desired_bitmap ||
				kbdev->pm.backend.desired_tiler_state &
				~desired_tiler_bitmap) {
			/* Start timer to power off cores */
			kbdev->pm.backend.shader_poweroff_pending |=
				(kbdev->pm.backend.desired_shader_state &
							~desired_bitmap);
			kbdev->pm.backend.tiler_poweroff_pending |=
				(kbdev->pm.backend.desired_tiler_state &
							~desired_tiler_bitmap);

			if (kbdev->pm.poweroff_shader_ticks &&
					!kbdev->protected_mode_transition)
				kbdev->pm.backend.shader_poweroff_pending_time =
						kbdev->pm.poweroff_shader_ticks;
			else
				do_poweroff = true;
		}

		kbdev->pm.backend.desired_shader_state = desired_bitmap;
		kbdev->pm.backend.desired_tiler_state = desired_tiler_bitmap;

		/* If any cores are being powered on, transition immediately */
		cores_are_available = kbase_pm_check_transitions_nolock(kbdev);
	} else if (kbdev->pm.backend.desired_shader_state & ~desired_bitmap ||
				kbdev->pm.backend.desired_tiler_state &
				~desired_tiler_bitmap) {
		/* Start timer to power off cores */
		kbdev->pm.backend.shader_poweroff_pending |=
				(kbdev->pm.backend.desired_shader_state &
							~desired_bitmap);
		kbdev->pm.backend.tiler_poweroff_pending |=
				(kbdev->pm.backend.desired_tiler_state &
							~desired_tiler_bitmap);
		if (kbdev->pm.poweroff_shader_ticks &&
				!kbdev->protected_mode_transition)
			kbdev->pm.backend.shader_poweroff_pending_time =
					kbdev->pm.poweroff_shader_ticks;
		else
			kbasep_pm_do_poweroff_cores(kbdev);
	} else if (kbdev->pm.active_count == 0 && desired_bitmap != 0 &&
			desired_tiler_bitmap != 0 &&
			kbdev->pm.backend.poweroff_timer_needed) {
		/* If power policy is keeping cores on despite there being no
		 * active contexts then disable poweroff timer as it isn't
		 * required.
		 * Only reset poweroff_timer_needed if we're not in the middle
		 * of the power off callback */
		kbdev->pm.backend.poweroff_timer_needed = false;
	}

	/* Ensure timer does not power off wanted cores and make sure to power
	 * off unwanted cores */
	if (kbdev->pm.backend.shader_poweroff_pending ||
			kbdev->pm.backend.tiler_poweroff_pending) {
		kbdev->pm.backend.shader_poweroff_pending &=
				~(kbdev->pm.backend.desired_shader_state &
								desired_bitmap);
		kbdev->pm.backend.tiler_poweroff_pending &=
				~(kbdev->pm.backend.desired_tiler_state &
				desired_tiler_bitmap);

		if (!kbdev->pm.backend.shader_poweroff_pending &&
				!kbdev->pm.backend.tiler_poweroff_pending)
			kbdev->pm.backend.shader_poweroff_pending_time = 0;
	}

	/* Shader poweroff is deferred to the end of the function, to eliminate
	 * issues caused by the core availability policy recursing into this
	 * function */
	if (do_poweroff)
		kbasep_pm_do_poweroff_cores(kbdev);

	/* Don't need 'cores_are_available', because we don't return anything */
	CSTD_UNUSED(cores_are_available);
}

void kbase_pm_update_cores_state(struct kbase_device *kbdev)
{
	unsigned long flags;

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);

	kbase_pm_update_cores_state_nolock(kbdev);

	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
}

int kbase_pm_list_policies(const struct kbase_pm_policy * const **list)
{
	if (!list)
		return POLICY_COUNT;

	*list = policy_list;

	return POLICY_COUNT;
}

KBASE_EXPORT_TEST_API(kbase_pm_list_policies);

const struct kbase_pm_policy *kbase_pm_get_policy(struct kbase_device *kbdev)
{
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	return kbdev->pm.backend.pm_current_policy;
}

KBASE_EXPORT_TEST_API(kbase_pm_get_policy);

void kbase_pm_set_policy(struct kbase_device *kbdev,
				const struct kbase_pm_policy *new_policy)
{
	struct kbasep_js_device_data *js_devdata = &kbdev->js_data;
	const struct kbase_pm_policy *old_policy;
	unsigned long flags;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(new_policy != NULL);

	KBASE_TRACE_ADD(kbdev, PM_SET_POLICY, NULL, NULL, 0u, new_policy->id);

	/* During a policy change we pretend the GPU is active */
	/* A suspend won't happen here, because we're in a syscall from a
	 * userspace thread */
	kbase_pm_context_active(kbdev);

	mutex_lock(&js_devdata->runpool_mutex);
	mutex_lock(&kbdev->pm.lock);

	/* Remove the policy to prevent IRQ handlers from working on it */
	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	old_policy = kbdev->pm.backend.pm_current_policy;
	kbdev->pm.backend.pm_current_policy = NULL;
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	KBASE_TRACE_ADD(kbdev, PM_CURRENT_POLICY_TERM, NULL, NULL, 0u,
								old_policy->id);
	if (old_policy->term)
		old_policy->term(kbdev);

	KBASE_TRACE_ADD(kbdev, PM_CURRENT_POLICY_INIT, NULL, NULL, 0u,
								new_policy->id);
	if (new_policy->init)
		new_policy->init(kbdev);

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	kbdev->pm.backend.pm_current_policy = new_policy;
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	/* If any core power state changes were previously attempted, but
	 * couldn't be made because the policy was changing (current_policy was
	 * NULL), then re-try them here. */
	kbase_pm_update_active(kbdev);
	kbase_pm_update_cores_state(kbdev);

	mutex_unlock(&kbdev->pm.lock);
	mutex_unlock(&js_devdata->runpool_mutex);

	/* Now the policy change is finished, we release our fake context active
	 * reference */
	kbase_pm_context_idle(kbdev);
}

KBASE_EXPORT_TEST_API(kbase_pm_set_policy);

void kbase_pm_request_cores(struct kbase_device *kbdev,
				bool tiler_required, bool shader_required)
{
	kbase_pm_change_state change_gpu_state = 0u;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	lockdep_assert_held(&kbdev->hwaccess_lock);

	if (shader_required) {
		int cnt = ++kbdev->shader_needed_cnt;

		if (cnt == 1)
			change_gpu_state |= KBASE_PM_CHANGE_STATE_SHADER;

		KBASE_DEBUG_ASSERT(kbdev->shader_needed_cnt != 0);
	}

	if (tiler_required) {
		int cnt = ++kbdev->tiler_needed_cnt;

		if (cnt == 1)
			change_gpu_state |= KBASE_PM_CHANGE_STATE_TILER;

		KBASE_DEBUG_ASSERT(kbdev->tiler_needed_cnt != 0);
	}

	if (change_gpu_state) {
		KBASE_TRACE_ADD(kbdev, PM_REQUEST_CHANGE_SHADER_NEEDED, NULL,
				NULL, 0u, kbdev->shader_needed_cnt);
		KBASE_TRACE_ADD(kbdev, PM_REQUEST_CHANGE_TILER_NEEDED, NULL,
				NULL, 0u, kbdev->tiler_needed_cnt);

		kbase_pm_update_cores_state_nolock(kbdev);
	}
}

KBASE_EXPORT_TEST_API(kbase_pm_request_cores);

void kbase_pm_release_cores(struct kbase_device *kbdev,
				bool tiler_required, bool shader_required)
{
	kbase_pm_change_state change_gpu_state = 0u;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	lockdep_assert_held(&kbdev->hwaccess_lock);

	if (shader_required) {
		int cnt;

		KBASE_DEBUG_ASSERT(kbdev->shader_needed_cnt > 0);

		cnt = --kbdev->shader_needed_cnt;

		if (0 == cnt) {
			change_gpu_state |= KBASE_PM_CHANGE_STATE_SHADER;
		}
	}

	if (tiler_required) {
		int cnt;

		KBASE_DEBUG_ASSERT(kbdev->tiler_needed_cnt > 0);

		cnt = --kbdev->tiler_needed_cnt;

		if (0 == cnt)
			change_gpu_state |= KBASE_PM_CHANGE_STATE_TILER;
	}

	if (change_gpu_state) {
		KBASE_TRACE_ADD(kbdev, PM_RELEASE_CHANGE_SHADER_NEEDED, NULL,
				NULL, 0u, kbdev->shader_needed_cnt);
		KBASE_TRACE_ADD(kbdev, PM_RELEASE_CHANGE_TILER_NEEDED, NULL,
				NULL, 0u, kbdev->tiler_needed_cnt);

		kbase_pm_update_cores_state_nolock(kbdev);
	}
}

KBASE_EXPORT_TEST_API(kbase_pm_release_cores);

void kbase_pm_request_cores_sync(struct kbase_device *kbdev,
		bool tiler_required, bool shader_required)
{
	unsigned long flags;

	kbase_pm_wait_for_poweroff_complete(kbdev);

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	kbase_pm_request_cores(kbdev, tiler_required, shader_required);
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	kbase_pm_check_transitions_sync(kbdev);
}

KBASE_EXPORT_TEST_API(kbase_pm_request_cores_sync);

static void kbase_pm_l2_caches_ref(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

	kbdev->l2_users_count++;

	KBASE_DEBUG_ASSERT(kbdev->l2_users_count != 0);

	/* Check for the required L2 transitions.
	 * Caller would block here for the L2 caches of all core groups to be
	 * powered on, so need to inform the Hw to power up all the L2 caches.
	 * Can't rely on the l2_users_count value being non-zero previously to
	 * avoid checking for the transition, as the count could be non-zero
	 * even if not all the instances of L2 cache are powered up since
	 * currently the power status of L2 is not tracked separately for each
	 * core group. Also if the GPU is reset while the L2 is on, L2 will be
	 * off but the count will be non-zero.
	 */
	kbase_pm_check_transitions_nolock(kbdev);
}

void kbase_pm_request_l2_caches(struct kbase_device *kbdev)
{
	unsigned long flags;

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);

	/* Take the reference on l2_users_count and check core transitions.
	 */
	kbase_pm_l2_caches_ref(kbdev);

	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);

	wait_event(kbdev->pm.backend.l2_powered_wait,
					kbdev->pm.backend.l2_powered == 1);
}

KBASE_EXPORT_TEST_API(kbase_pm_request_l2_caches);

void kbase_pm_request_l2_caches_nolock(struct kbase_device *kbdev)
{
	/* Take the reference on l2_users_count and check core transitions.
	 */
	kbase_pm_l2_caches_ref(kbdev);
}

void kbase_pm_request_l2_caches_l2_is_on(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

	kbdev->l2_users_count++;
}

KBASE_EXPORT_TEST_API(kbase_pm_request_l2_caches_l2_is_on);

void kbase_pm_release_l2_caches(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->hwaccess_lock);

	KBASE_DEBUG_ASSERT(kbdev->l2_users_count > 0);

	--kbdev->l2_users_count;

	if (!kbdev->l2_users_count)
		kbase_pm_check_transitions_nolock(kbdev);
}

KBASE_EXPORT_TEST_API(kbase_pm_release_l2_caches);

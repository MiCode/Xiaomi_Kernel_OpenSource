// SPDX-License-Identifier: GPL-2.0 OR MIT

/*
 * (C) COPYRIGHT 2020 Arm Limited or its affiliates. All rights reserved.
 */

/**
 * @file
 * Part of the Mali reference arbiter
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/completion.h>
#include <gpu/mali_arb_plat.h>
#include "mali_arbiter.h"

/* GPU time in ms given before switching to another VM */
static int request_timeout = 4;

/* Maximum time in ms given for VM to stop before resetting GPU */
static int yield_timeout = 8;

module_param(request_timeout, int, 0644);
MODULE_PARM_DESC(request_timeout,
	"GPU time in ms given before switching to another VM");
module_param(yield_timeout, int, 0644);
MODULE_PARM_DESC(yield_timeout,
	"Max time(ms) for a VM to stop before GPU lost handling is invoked");

#if MALI_ARBITER_TEST_API
static int arb_test_mode;

/* Using 0644 instead of S_IRUGO | S_IWUSR in
 * the following line fixes a check patch warning
 */
module_param(arb_test_mode, int, 0644);
MODULE_PARM_DESC(arb_test_mode, "Switch on test mode functionality");
#endif /* MALI_ARBITER_TEST_API */


/* Shift used for dvfs.time busy/idle units of (1 << 8) ns
 * This gives a maximum period between utilisation reports of 2^(32+8) ns
 * Exceeding this will cause overflow */
#define ARBITER_TIME_SHIFT 8

/* Represents the value of an unassigned domain id */
#define NULL_DOMAIN_ID U32_MAX

/**
 * enum arb_state - Arbiter states
 * @ARB_NO_REQ:       No VMs are requesting GPU from the Arbiter.
 * @ARB_SINGLE_REQ:   A single VM has requested GPU.
 * @ARB_RUNNING:      Multiple VMs are requesting GPU and arbitration
 *                    is required between them.
 * @ARB_GPU_STOPPING: Active VM has been told to stop using GPU and
 *                    the arbiter is waiting for a stopped event.
 * @ARB_GPU_LOST:     Active VM has not stopped within yield_timeout
 *                    so GPU will be reset.
 *
 * Definition of the possible arbiter's states
 */
enum arb_state {
	ARB_NO_REQ,
	ARB_SINGLE_REQ,
	ARB_RUNNING,
	ARB_GPU_STOPPING,
	ARB_GPU_LOST,
};

/**
 * struct mali_arb_dom - Internal Arbiter state information for domain
 * @entry:   Entry for req_list.
 * @info:    Domain information supplied during registration.
 * @arb:     Arbiter state.
 *
 * Keep track of the information for each domain registered with the arbiter
 */
struct mali_arb_dom {
	struct list_head entry;
	struct mali_arb_dom_info info;
	struct mali_arb *arb;
};

/**
 * struct mali_arb - Internal Arbiter state information
 * @arb_dev: Embedded public arbiter device data.
 * @dev: Device for arbiter (only single Arbiter device
 *       supported).
 * @active_dom: Active domain or NULL
 * @tmr: Timer used during RUNNING and STOPPING states for
 *       scheduler purposes.
 * @req_list: Request list of domains
 * @mutex: Mutex to protect Arbiter state
 * @state: Current arbiter state
 * @wq: Workqueue for safely handling timer events
 * @timeout_work: Timeout work item
 * @timer_running: TRUE if timer is active, FALSE otherwise
 * @plat_module: Optional Arbiter integration module (or NULL)
 * @plat_dev:   Optional platform integration device for DVFS/power
 *              management (or NULL)
 * @last_state_transition: time of last dvfs state transition
 * @last_dvfs_report: time of last utilisation report
 * @gpu_active_sum: running total of active time since last report
 * @gpu_switching_sum: running total of switching time since last report
 * @arb_vm_assign_gpu: Callback function to assign GPU to VM in hypervisor
 * @arb_vm_force_gpu: Callback function to force assign GPU to VM in hypervisor
 *
 * Main arbiter struct which contains references to all the resources and
 * interfaces needed at runtime
 */
struct mali_arb {
	struct mali_arb_dev arb_dev;
	struct device *dev;
	struct mali_arb_dom *active_dom;
	u32 current_domain_id;
	struct hrtimer tmr;
	struct list_head req_list;
	struct mutex mutex;
	enum arb_state state;
	struct workqueue_struct *wq;
	struct work_struct timeout_work;
	bool timer_running;
	struct module *plat_module;
	struct mali_arb_plat_dev *plat_dev;
	ktime_t last_state_transition;
	ktime_t last_dvfs_report;
	u64 gpu_active_sum;
	u64 gpu_switching_sum;
	bool force_gpu_required;
	void *hyp_ctx;
	int (*arb_hyp_assign_vm_gpu)(void *ctx, u32 domain_id);
	int (*arb_hyp_force_assign_vm_gpu)(void *ctx, u32 domain_id);
	void (*arb_gpu_power_on)(struct mali_arb_plat_dev *plat_dev);
	void (*arb_gpu_power_off)(struct mali_arb_plat_dev *plats_dev);


};

/**
 * arb_from_dev() - Convert arb_dev to arb
 * @arb_dev: The arbiter device
 *
 * Return: arb data or NULL if input parameter was NULL
 */
static inline struct mali_arb *arb_from_dev(
	struct mali_arb_dev *arb_dev)
{
	if (likely(arb_dev))
		return container_of(arb_dev, struct mali_arb, arb_dev);
	return NULL;
}

/**
 * arb_state_text() - Helper to convert state to string (for debug)
 * @state: State to convert.
 *
 * Return: A null terminated string
 */
static inline const char *arb_state_text(enum arb_state state)
{
	switch (state) {
	case ARB_NO_REQ:
		return "ARB_NO_REQ";
	case ARB_SINGLE_REQ:
		return "ARB_SINGLE_REQ";
	case ARB_RUNNING:
		return "ARB_RUNNING";
	case ARB_GPU_STOPPING:
		return "ARB_GPU_STOPPING";
	case ARB_GPU_LOST:
		return "ARB_GPU_LOST";
	default:
		return "UNKNOWN";
	}
}

/*
 * @arb_dvfs_update() - update dvfs data.
 * @arb: Arbiter data.
 * @new_state: state being transition to.
 *
 * this function updates the running totals for active and switching times
 * and/or restarts the dvfs timestamp as necessary before we transition state.
 */
static void arb_dvfs_update(struct mali_arb *arb, enum arb_state new_state)
{
	ktime_t now = ktime_get();
	u64 since_last;

	if (WARN_ON(!arb))
		return;

	since_last = ktime_to_ns(ktime_sub(now,
				arb->last_state_transition));

	switch (arb->state) {
	case ARB_NO_REQ:
		switch (new_state) {
		/* idle --> active */
		case ARB_SINGLE_REQ:
			dev_dbg(arb->dev, "dvfs: idle --> active\n");
			arb->last_state_transition = now;
			break;
		default:
			break;
		}
		break;
	case ARB_SINGLE_REQ:
	case ARB_RUNNING:
		switch (new_state) {
		/* active --> switching */
		case ARB_GPU_STOPPING:
			dev_dbg(arb->dev, "dvfs: active --> switching\n");
			arb->gpu_active_sum += since_last;
			arb->last_state_transition = now;
			break;
		default:
			break;
		}
		break;
	case ARB_GPU_LOST:
	case ARB_GPU_STOPPING:
		switch (new_state) {
		/* switching --> active */
		case ARB_RUNNING:
		case ARB_SINGLE_REQ:
			dev_dbg(arb->dev, "dvfs: switching --> active\n");
			arb->gpu_switching_sum += since_last;
			arb->last_state_transition = now;
			break;
		/* switching --> idle */
		case ARB_NO_REQ:
			arb->gpu_switching_sum += since_last;
			dev_dbg(arb->dev, "dvfs: switching --> idle\n");
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

}

/**
 * set_state_locked() - Used to modify the arbiter state
 * @arb: Arbiter data
 * @new: New state.
 *
 * Mutex must be held when calling this function.
 */
static inline void set_state_locked(struct mali_arb *arb,
	enum arb_state new)
{
	if (WARN_ON(!arb))
		return;

	lockdep_assert_held(&arb->mutex);
	if (arb->state != new) {
		arb_dvfs_update(arb, new);
		arb->state = new;
	}
}

/**
 * start_timer_locked() - Start the timer with a given timeout
 * @arb: Arbiter data
 * @timeout: Timeout in milliseconds.
 */
static void start_timer_locked(struct mali_arb *arb,
	unsigned long timeout)
{
	if (WARN_ON(!arb))
		return;

	lockdep_assert_held(&arb->mutex);
	hrtimer_start(&arb->tmr, ms_to_ktime(timeout), HRTIMER_MODE_REL);
	arb->timer_running = true;
}

/**
 * stop_timer_locked() - Stop the timer
 * @arb: Arbiter data
 *
 * @note mutex is temporarily released by the function to clear timer
 */
static void stop_timer_locked(struct mali_arb *arb)
{
	if (WARN_ON(!arb))
		return;

	lockdep_assert_held(&arb->mutex);
	arb->timer_running = false;
	hrtimer_cancel(&arb->tmr);
	mutex_unlock(&arb->mutex);
	cancel_work_sync(&arb->timeout_work);
	mutex_lock(&arb->mutex);
}

/**
 * timer_isr() - Called when timer fires as ISR
 * @hrtimer: timer data
 *
 * Queues a workqueue item to handle the event
 */
static enum hrtimer_restart timer_isr(struct hrtimer *timer)
{
	struct mali_arb *arb;
	arb = container_of(timer, struct mali_arb, tmr);

	dev_dbg(arb->dev, "hrtimer_isr\n");
	queue_work(arb->wq, &arb->timeout_work);
	return HRTIMER_NORESTART;
}

static void remove_active_domain(struct mali_arb *arb)
{
	if (WARN_ON(!arb))
		return;

	lockdep_assert_held(&arb->mutex);
	arb->active_dom = NULL;
}

static void assign_to_domain(struct mali_arb *arb, u32 domain_id)
{
	int ret = -EPERM;

	if (WARN_ON(!arb))
		return;

	if (arb->current_domain_id == domain_id)
		return;

	if (!arb->force_gpu_required) {
		if (arb->arb_hyp_assign_vm_gpu)
			ret = arb->arb_hyp_assign_vm_gpu(arb->hyp_ctx,
					domain_id);

		if (ret) {
			dev_err(arb->dev, "Failed to assign GPU to guest %u\n",
					domain_id);
			arb->force_gpu_required = true;
		}
	}

	if (arb->force_gpu_required) {
		if (arb->arb_hyp_force_assign_vm_gpu)
			ret = arb->arb_hyp_force_assign_vm_gpu(arb->hyp_ctx,
					domain_id);
		if (ret) {
			dev_err(arb->dev,
				"Failed to force assign GPU to guest %u\n",
				domain_id);

			if (arb->active_dom != NULL) {
				/* Try assigning to different guest next time */
				list_move_tail(&arb->active_dom->entry,
					&arb->req_list);
				arb->active_dom = NULL;
			}
		} else
			arb->force_gpu_required = false;
	}

	if (!ret)
		arb->current_domain_id = domain_id;
}

/**
 * grant_gpu_to_next_locked() - Grant the GPU to the next domain
 * @arb: Arbiter data
 *
 * Assigns the GPU to the first domain on the queue. The mutex lock
 * must already be held when calling this function.
 */
static void grant_gpu_to_next_locked(struct mali_arb *arb)
{
	struct mali_arb_dom *prev_dom;

	if (WARN_ON(!arb))
		return;

	prev_dom = arb->active_dom;

	lockdep_assert_held(&arb->mutex);
	if (list_empty(&arb->req_list)) {
		arb->active_dom = NULL;
		return;
	}

	arb->active_dom = list_first_entry(&arb->req_list,
		struct mali_arb_dom, entry);
	if (prev_dom == arb->active_dom)
		return;

	assign_to_domain(arb, arb->active_dom->info.domain_id);

	if (arb->active_dom) {
		arb->active_dom->info.arb_vm_gpu_granted(
			arb->active_dom->info.dev);
	}
}

/**
 * run_next_guest_locked() - Run the next guest
 * @arb: Arbiter data
 *
 * Makes the necessary state transitions based on the current state and
 * domain list.  The mutex lock must already be held when calling this
 * function.
 */
static void run_next_guest_locked(struct mali_arb *arb)
{
	struct mali_arb_plat_dev *plat_dev;

	if (WARN_ON(!arb))
		return;

	plat_dev = arb->plat_dev;

	lockdep_assert_held(&arb->mutex);

	switch (arb->state) {
	case ARB_RUNNING:
	case ARB_GPU_STOPPING:
		stop_timer_locked(arb);
		break;
	default:
		break;
	}

	if (arb->state != ARB_NO_REQ) {
		if (list_empty(&arb->req_list)) {
			set_state_locked(arb, ARB_NO_REQ);
			assign_to_domain(arb, 0);
			remove_active_domain(arb);
			if (arb->arb_gpu_power_off)
				arb->arb_gpu_power_off(plat_dev);
		} else if (list_is_singular(&arb->req_list))
			set_state_locked(arb, ARB_SINGLE_REQ);
		else if (arb->state != ARB_RUNNING)
			set_state_locked(arb, ARB_RUNNING);
	} else if (!list_empty(&arb->req_list))
		set_state_locked(arb, ARB_SINGLE_REQ);

	if (arb->state != ARB_NO_REQ) {
		grant_gpu_to_next_locked(arb);
		if (arb->state == ARB_RUNNING)
			start_timer_locked(arb, request_timeout);
	}
}

/**
 * gpu_request() - See #vm_arb_gpu_request in mali_arbiter.h
 */
static void gpu_request(struct mali_arb_dom *dom)
{
	struct mali_arb_dom *cur_dom;
	struct mali_arb *arb;
	struct mali_arb_plat_dev *plat_dev;

	WARN_ON(!dom || !dom->arb);
	if (!dom || !dom->arb)
		return;
	arb = dom->arb;
	plat_dev = arb->plat_dev;
	dev_dbg(arb->dev, "%s %u\n", __func__, dom->info.domain_id);
	mutex_lock(&arb->mutex);
	if (list_empty(&arb->req_list) && arb->arb_gpu_power_on)
		arb->arb_gpu_power_on(plat_dev);

	list_for_each_entry(cur_dom, &arb->req_list, entry) {
		if (cur_dom == dom) {
			dev_warn(arb->dev,
				"Domain %u has already requested GPU\n",
				dom->info.domain_id);
			mutex_unlock(&arb->mutex);
			return;
		}
	}
	list_add_tail(&dom->entry, &arb->req_list);
	if (arb->state == ARB_NO_REQ || arb->state == ARB_SINGLE_REQ)
		run_next_guest_locked(arb);
	mutex_unlock(&arb->mutex);
}

/**
 * gpu_stopped() - See #vm_arb_gpu_stopped in mali_arbiter.h
 */
static void gpu_stopped(struct mali_arb_dom *dom, bool req_again)
{
	struct mali_arb *arb;
	struct mali_arb_plat_dev *plat_dev;

	WARN_ON(!dom || !dom->arb);
	if (!dom || !dom->arb)
		return;
	arb = dom->arb;
	plat_dev = arb->plat_dev;
	mutex_lock(&arb->mutex);

	dev_dbg(arb->dev,
		"%s %u, req_again: %d\n", __func__,
		dom->info.domain_id, req_again);

	if (!list_empty(&dom->entry)) {
		if (req_again)
			list_move_tail(&dom->entry, &arb->req_list);
		else
			list_del_init(&dom->entry);
		if (dom == arb->active_dom) {
			remove_active_domain(arb);
			run_next_guest_locked(arb);
		}
	} else if (req_again) {
		/* The domain was previous forcibly removed because it didn't
		 * release the GPU in time. The domain is no longer in the
		 * arbiter request list so it needs to be added again if
		 * the req_again parameter is true, otherwise the domain will
		 * not be able to access the GPU.
		 */
		dev_dbg(arb->dev,
			"%s - dom->entry is empty. requesting gpu for dom%u\n",
			__func__, dom->info.domain_id);
		if (list_empty(&arb->req_list) && arb->arb_gpu_power_on)
			arb->arb_gpu_power_on(plat_dev);
		list_add_tail(&dom->entry, &arb->req_list);
		run_next_guest_locked(arb);
	}
	mutex_unlock(&arb->mutex);
}

/**
 * timeout_worker() - Called when timer expires
 * @data: Used to obtain arbiter data
 *
 * Handles the timeout cases in the RUNNING and GPU_STOPPING states
 */
static void timeout_worker(struct work_struct *data)
{
	struct mali_arb_dom *dom;
	struct mali_arb *arb;

	if (WARN_ON(!data))
		return;

	arb = container_of(data, struct mali_arb, timeout_work);
	if (WARN_ON(!arb))
		return;

	dev_dbg(arb->dev, "Timer has expired...\n");

	mutex_lock(&arb->mutex);
	if (!arb->timer_running)
		goto cleanup_mutex;

	dom = arb->active_dom;
	if (!dom) {
		dev_dbg(arb->dev,
		"Timer cannot be pending without an active domain\n");
		goto cleanup_mutex;
	}
	switch (arb->state) {
	case ARB_RUNNING:
		dev_dbg(arb->dev, "GPU time up - suspending\n");
		set_state_locked(arb, ARB_GPU_STOPPING);
		start_timer_locked(arb, yield_timeout);
		dom->info.arb_vm_gpu_stop(dom->info.dev);
		break;
	case ARB_GPU_STOPPING:
		dev_warn(arb->dev, "GPU took too long to yield for dom%u\n",
				dom->info.domain_id);
		set_state_locked(arb, ARB_GPU_LOST);
		if (arb->active_dom)
			list_del_init(&arb->active_dom->entry);
		remove_active_domain(arb);
		arb->force_gpu_required = true;
		run_next_guest_locked(arb);
		dom->info.arb_vm_gpu_lost(dom->info.dev);
		break;
	default:
		break;
	}

cleanup_mutex:
	mutex_unlock(&arb->mutex);
}


/**
 * register_hyp() - See #vm_arb_register_hyp in mali_arbiter.h
 */
static int register_hyp(struct mali_arb_dev *arb_dev,
	void *ctx, const struct mali_arb_hyp_callbacks *cbs)
{
	int ret = 0;
	struct mali_arb *arb;

	if (!arb_dev || !cbs) {
		WARN(1, "vm_arb_register_hyp parameter cannot be NULL");
		return -EFAULT;
	}

	arb = arb_from_dev(arb_dev);
	dev_dbg(arb->dev, "%s callbacks\n", __func__);
	if (unlikely(cbs->arb_hyp_assign_vm_gpu == NULL ||
			cbs->arb_hyp_force_assign_vm_gpu == NULL)) {
		WARN(1, "vm_arb_register_hyp callback cannot be NULL");
		return -EINVAL;
	}

	mutex_lock(&arb->mutex);
	if (unlikely(arb->hyp_ctx ||
			arb->arb_hyp_assign_vm_gpu ||
			arb->arb_hyp_force_assign_vm_gpu)) {
		WARN(1, "vm_arb_register_hyp already called");
		ret = -EBUSY;
		goto cleanup_mutex;
	}
	arb->hyp_ctx = ctx;
	arb->arb_hyp_assign_vm_gpu = cbs->arb_hyp_assign_vm_gpu;
	arb->arb_hyp_force_assign_vm_gpu = cbs->arb_hyp_force_assign_vm_gpu;
cleanup_mutex:
	mutex_unlock(&arb->mutex);
	return ret;
}

/**
 * unregister_hyp() - See #vm_arb_unregister_hyp in mali_arbiter.h
 */
static void unregister_hyp(struct mali_arb_dev *arb_dev)
{
	struct mali_arb *arb;

	if (likely(arb_dev)) {
		arb = arb_from_dev(arb_dev);
		dev_dbg(arb->dev, "%s callbacks\n", __func__);
		mutex_lock(&arb->mutex);
		arb->hyp_ctx = NULL;
		arb->arb_hyp_assign_vm_gpu = NULL;
		arb->arb_hyp_force_assign_vm_gpu = NULL;
		mutex_unlock(&arb->mutex);
	}
}

/**
 * register_dom() - See #vm_arb_register_dom in mali_arbiter.h
 */
static int register_dom(struct mali_arb_dev *arb_dev,
	struct mali_arb_dom_info *info, struct mali_arb_dom **dom_out)
{
	struct mali_arb *arb;
	struct mali_arb_dom *dom;

	WARN_ON(!arb_dev || !info || !dom_out);
	if (!arb_dev || !info || !dom_out) {
		if (dom_out)
			*dom_out = NULL;
		return -EFAULT;
	}

	arb = arb_from_dev(arb_dev);
	dev_info(arb->dev, "%s %u\n", __func__, info->domain_id);
	dom = devm_kzalloc(arb->dev, sizeof(struct mali_arb_dom),
		GFP_KERNEL);
	if (!dom)
		return -ENOMEM;

	if (unlikely(info->dev == NULL ||
			info->arb_vm_gpu_granted == NULL ||
			info->arb_vm_gpu_stop == NULL ||
			info->arb_vm_gpu_lost == NULL))
		return -EINVAL;

	dom->arb = arb;
	memcpy(&dom->info, info, sizeof(dom->info));
	INIT_LIST_HEAD(&dom->entry);
	*dom_out = dom;
	return 0;
}

/**
 * register_dom() - See #vm_arb_unregister_dom in mali_arbiter.h
 */
static void unregister_dom(struct mali_arb_dom *dom)
{
	struct mali_arb *arb;
	u32 dom_id;

	WARN_ON(!dom || !dom->arb);
	if (!dom || !dom->arb)
		return;
	arb = dom->arb;
	dom_id = dom->info.domain_id;

	mutex_lock(&arb->mutex);
	if (!list_empty(&dom->entry))
		list_del(&dom->entry);

	if (arb->current_domain_id == dom_id)
		arb->current_domain_id = NULL_DOMAIN_ID;

	if (dom == arb->active_dom) {
		remove_active_domain(arb);
		run_next_guest_locked(arb);
	}
	mutex_unlock(&arb->mutex);
	dev_info(arb->dev, "%s %u\n", __func__, dom_id);
}

/**
 * gpu_active() - See #vm_arb_gpu_active in mali_arbiter.h
 */
static void gpu_active(struct mali_arb_dom *dom)
{
	WARN_ON(!dom || !dom->arb);
	if (!dom || !dom->arb)
		return;
	dev_dbg(dom->arb->dev, "GPU %u is active\n", dom->info.domain_id);
}

/**
 * gpu_idle() - See #vm_arb_gpu_idle in mali_arbiter.h
 */
static void gpu_idle(struct mali_arb_dom *dom)
{
	struct mali_arb *arb;

	WARN_ON(!dom || !dom->arb);
	if (!dom || !dom->arb)
		return;
	arb = dom->arb;

	mutex_lock(&arb->mutex);
	if (dom == arb->active_dom && (arb->state == ARB_RUNNING ||
			arb->state == ARB_SINGLE_REQ)) {
		dev_dbg(arb->dev,
			"GPU %u is idle - giving up...\n",
			dom->info.domain_id);
		if (arb->state == ARB_RUNNING)
			stop_timer_locked(arb);
		set_state_locked(arb, ARB_GPU_STOPPING);
		start_timer_locked(arb, yield_timeout);
		dom->info.arb_vm_gpu_stop(dom->info.dev);
	} else
		dev_dbg(dom->arb->dev, "GPU %u is idle\n", dom->info.domain_id);
	mutex_unlock(&arb->mutex);
}

/**
 * get_utilisation() - Get utilization information
 * @dev: The device structure
 * @gpu_busytime: will contain the GPU busy time
 * @gpu_totaltime: will contain the GPU total time
 *
 * Called from platform integration driver for DVFS to get GPU busytime and
 * totaltime since last request
 */
static void get_utilisation(struct device *dev, u32 *gpu_busytime,
	u32 *gpu_totaltime)
{
	struct mali_arb_dev *arb_dev;
	struct mali_arb *arb;
	ktime_t now = ktime_get();
	u64 time_since_last_report, time_since_last_transition;

	WARN_ON(!dev || !gpu_busytime || !gpu_totaltime);
	if (!dev || !gpu_busytime || !gpu_totaltime)
		return;

	arb_dev = dev_get_drvdata(dev);
	if (!arb_dev) {
		dev_warn(dev, "The arbiter device was removed unexpectedly.");
		return;
	}

	arb = arb_from_dev(arb_dev);
	if (WARN_ON(!arb))
		return;

	mutex_lock(&arb->mutex);

	time_since_last_report = ktime_to_ns(ktime_sub(now,
					arb->last_dvfs_report));
	time_since_last_transition = ktime_to_ns(ktime_sub(now,
		arb->last_state_transition));

	switch (arb->state) {
	case ARB_RUNNING:
	case ARB_SINGLE_REQ: /* active */
		arb->gpu_active_sum += time_since_last_transition;
		break;
	case ARB_GPU_STOPPING: /* switching */
	case ARB_GPU_LOST:
		arb->gpu_switching_sum += time_since_last_transition;
		break;
	default:
		break;
	}

	*gpu_busytime = arb->gpu_active_sum >> ARBITER_TIME_SHIFT;
	*gpu_totaltime = (time_since_last_report - arb->gpu_switching_sum)
		>> ARBITER_TIME_SHIFT;

	WARN_ON(*gpu_busytime > *gpu_totaltime);
	if (arb->gpu_active_sum)
		dev_dbg(arb->dev, "reporting busy %u, total %u (~ %llu%%)\n",
			*gpu_busytime, *gpu_totaltime,
			(((u64)(*gpu_busytime) * 100) / (*gpu_totaltime)));

	/* reset counters for next time */
	arb->gpu_active_sum = 0;
	arb->gpu_switching_sum = 0;
	arb->last_state_transition = arb->last_dvfs_report = now;

	mutex_unlock(&arb->mutex);
}

static int arbiter_init_plat(struct mali_arb *arb)
{
	struct mali_arb_plat_arbiter_cb_ops ops;
	struct mali_arb_plat_dev *plat_dev;
	struct device_node *plat_node;
	struct platform_device *plat_pdev;
	int err;

	dev_dbg(arb->dev, "%s\n", __func__);

	/* find the platform integration device in Device Tree */
	plat_node = of_parse_phandle(arb->dev->of_node,
		"platform", 0);
	if (!plat_node) {
		dev_info(arb->dev, "No arbiter platform module in DT\n");
		arb->plat_module = NULL;
		arb->plat_dev = NULL;
		return 0;
	}

	plat_pdev = of_find_device_by_node(plat_node);
	if (!plat_pdev) {
		dev_err(arb->dev, "Failed to find arbiter platform device\n");
		return -EPROBE_DEFER;
	}

	if (!plat_pdev->dev.driver ||
			!try_module_get(plat_pdev->dev.driver->owner)) {
		dev_err(arb->dev, "Arbiter platform module not available\n");
		return -EPROBE_DEFER;
	}

	arb->plat_module = plat_pdev->dev.driver->owner;
	plat_dev = platform_get_drvdata(plat_pdev);
	if (!plat_dev) {
		dev_err(arb->dev, "Arbiter platform device not ready\n");
		err = -EPROBE_DEFER;
		goto cleanup_module;
	}

	arb->plat_dev = plat_dev;
	ops.dvfs_arb_get_utilisation = get_utilisation;

	/* register arbiter callbacks */
	if (plat_dev->plat_ops.mali_arb_plat_register == NULL ||
			plat_dev->plat_ops.mali_arb_plat_unregister == NULL) {
		dev_err(arb->dev, "Platform callbacks are not valid\n");
		err = -EFAULT;
		goto cleanup_dev;
	}

	err = plat_dev->plat_ops.mali_arb_plat_register(plat_dev,
			arb->dev, &ops);
	if (err) {
		dev_err(arb->dev,
			"Failed to register arbiter with platform device\n");
		goto cleanup_dev;
	}

	arb->arb_gpu_power_on = plat_dev->plat_ops.mali_arb_gpu_power_on;
	arb->arb_gpu_power_off = plat_dev->plat_ops.mali_arb_gpu_power_off;

	/* initialize DVFS */
	arb->gpu_active_sum = 0;
	arb->gpu_switching_sum = 0;
	arb->last_state_transition = arb->last_dvfs_report = ktime_get();
	if (plat_dev->plat_ops.mali_arb_start_dvfs) {
		WARN_ON(!plat_dev->plat_ops.mali_arb_stop_dvfs);
		err = plat_dev->plat_ops.mali_arb_start_dvfs(plat_dev);
		if (err) {
			dev_err(arb->dev, "Failed to start dvfs\n");
			goto cleanup_plat;
		}
		dev_info(arb->dev, "DVFS initialized\n");
	} else
		dev_info(arb->dev, "DVFS not supported by platform module\n");

	return 0;

cleanup_plat:
	plat_dev->plat_ops.mali_arb_plat_unregister(plat_dev);
cleanup_dev:
	arb->plat_dev = NULL;
cleanup_module:
	module_put(arb->plat_module);
	arb->plat_module = NULL;
	return err;
}

#if MALI_ARBITER_TEST_API
void test_set_callbacks(struct mali_arb_dev *arb_dev,
	struct mali_arb_test_callbacks *cbs)
{
	struct mali_arb *arb = arb_from_dev(arb_dev);

	arb->arb_gpu_power_on = cbs->arb_test_power_on;
	arb->arb_gpu_power_off = cbs->arb_test_power_off;
}

void test_utilisation(struct mali_arb_dev *arb_dev,
	u32 *gpu_busytime, u32 *gpu_totaltime)
{
	if (!arb_dev)
		return;
	get_utilisation(arb_from_dev(arb_dev)->dev, gpu_busytime,
			gpu_totaltime);
}
#endif /* MALI_ARBITER_TEST_API */

/**
 * arbiter_probe() - Called when device is matched in device tree
 * @pdev: Platform device
 *
 * Probe and initialize the arbiter device and all its resources
 *
 * Return: 0 if success or a Linux error code
 */
static int arbiter_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mali_arb *arb;
	int err;

	dev_info(&pdev->dev, "%s\n", __func__);
	arb = devm_kzalloc(dev, sizeof(struct mali_arb), GFP_KERNEL);
	if (!arb)
		return -ENOMEM;
	arb->arb_dev.ops.vm_arb_register_hyp = register_hyp;
	arb->arb_dev.ops.vm_arb_unregister_hyp = unregister_hyp;
	arb->arb_dev.ops.vm_arb_register_dom = register_dom;
	arb->arb_dev.ops.vm_arb_unregister_dom = unregister_dom;
	arb->arb_dev.ops.vm_arb_gpu_request = gpu_request;
	arb->arb_dev.ops.vm_arb_gpu_active = gpu_active;
	arb->arb_dev.ops.vm_arb_gpu_idle = gpu_idle;
	arb->arb_dev.ops.vm_arb_gpu_stopped = gpu_stopped;
	arb->dev = dev;
	arb->timer_running = false;
	mutex_init(&arb->mutex);
	INIT_LIST_HEAD(&arb->req_list);
	hrtimer_init(&arb->tmr, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	arb->tmr.function = timer_isr;
	arb->state = ARB_NO_REQ;
	arb->force_gpu_required = false;
	arb->wq = alloc_workqueue("mali_arb", WQ_UNBOUND, 0);
	INIT_WORK(&arb->timeout_work, timeout_worker);
	arb->current_domain_id = NULL_DOMAIN_ID;
	arb->active_dom = NULL;


#if MALI_ARBITER_TEST_API
	if (arb_test_mode) {
		arb->arb_dev.ops.vm_arb_test_set_callbacks =
			test_set_callbacks;
		arb->arb_dev.ops.vm_arb_test_utilisation =
			test_utilisation;
		goto probe_exit;
	}
	arb->arb_dev.ops.vm_arb_test_set_callbacks = NULL;
	arb->arb_dev.ops.vm_arb_test_utilisation = NULL;
#endif /* MALI_ARBITER_TEST_API */

	err = arbiter_init_plat(arb);
	if (err) {
		dev_err(arb->dev, "Failed to initialize arbiter dvfs\n");
		destroy_workqueue(arb->wq);
		mutex_unlock(&arb->mutex);
		return err;
	}

#if MALI_ARBITER_TEST_API
probe_exit:
#endif /* MALI_ARBITER_TEST_API */
	platform_set_drvdata(pdev, &arb->arb_dev);
	return 0;
}

/**
 * arbiter_destroy() - Release the resources of the arbiter device
 * @pdev: Platform device
 *
 * This function is called when the device is removed to free up
 * all the resources
 */
static void arbiter_destroy(struct mali_arb *arb)
{
	struct mali_arb_dom *dom, *tmp_dom;

	if (WARN_ON(!arb))
		return;

	dev_info(arb->dev, "%s\n", __func__);
	mutex_lock(&arb->mutex);

	if (arb->plat_dev) {
		struct mali_arb_plat_dev *plat_dev = arb->plat_dev;

		if (plat_dev->plat_ops.mali_arb_start_dvfs &&
				plat_dev->plat_ops.mali_arb_stop_dvfs)
			plat_dev->plat_ops.mali_arb_stop_dvfs(plat_dev);
		plat_dev->plat_ops.mali_arb_plat_unregister(plat_dev);
		arb->plat_dev = NULL;
	}

	if (arb->plat_module) {
		module_put(arb->plat_module);
		arb->plat_module = NULL;
	}

	list_for_each_entry_safe(dom, tmp_dom, &arb->req_list, entry) {
		devm_kfree(arb->dev, dom);
	}
	mutex_unlock(&arb->mutex);
	destroy_workqueue(arb->wq);
}

/**
 * arbiter_remove() - Called when device is removed
 * @pdev: Platform device
 */
static int arbiter_remove(struct platform_device *pdev)
{
	struct mali_arb_dev *arb_dev;

	arb_dev = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	if (arb_dev) {
		struct mali_arb *arb = arb_from_dev(arb_dev);

		arbiter_destroy(arb);
		devm_kfree(&pdev->dev, arb);
	}
	dev_info(&pdev->dev, "%s done\n", __func__);
	return 0;
}

/**
 * @arbiter_dt_match: Match the platform device with the Device Tree.
 */
static const struct of_device_id arbiter_dt_match[] = {
	{ .compatible = MALI_ARBITER_DT_NAME },
	{}
};

/**
 * @arbiter_driver: Platform driver data.
 */
static struct platform_driver arbiter_driver = {
	.probe = arbiter_probe,
	.remove = arbiter_remove,
	.driver = {
		.name = "mali arbiter",
		.of_match_table = arbiter_dt_match,
	},
};

/**
 * arbiter_init - Register platform driver
 */
static int __init arbiter_init(void)
{
	return platform_driver_register(&arbiter_driver);
}
module_init(arbiter_init);

/**
 * arbiter_exit - Unregister platform driver
 */
static void __exit arbiter_exit(void)
{
	platform_driver_unregister(&arbiter_driver);
}
module_exit(arbiter_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("mali-arbiter");

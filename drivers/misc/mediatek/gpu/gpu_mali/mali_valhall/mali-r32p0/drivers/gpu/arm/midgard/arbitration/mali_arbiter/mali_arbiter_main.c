// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2019-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
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
#include <linux/of_address.h>
#include <linux/sched.h>

#include <gpu/mali_gpu_power.h>
#include "arb_vm_protocol/mali_arb_vm_protocol.h"
#include "mali_arbiter.h"
#include "mali_arbiter_sysfs.h"
#include "mali_gpu_resource_group.h"
#include "mali_gpu_partition_config.h"
#include "mali_gpu_assign.h"


/* Arbiter version against which was implemented this file */
#define MALI_ARBITER_IMPLEMENTATION_VERSION 2
#if MALI_ARBITER_IMPLEMENTATION_VERSION != MALI_ARBITER_VERSION
#error "Unsupported Mali Arbiter version."
#endif

/* Partition config version against which was implemented this module */
#define MALI_REQUIRED_PARTITION_CONFIG_VERSION 1
#if MALI_REQUIRED_PARTITION_CONFIG_VERSION != MALI_PARTITION_CONFIG_VERSION
#error "Unsupported partition config API version."
#endif

/* Resource group version against which was implemented this module */
#define MALI_REQUIRED_RESOURCE_GROUP_VERSION 1
#if MALI_REQUIRED_RESOURCE_GROUP_VERSION != MALI_RESOURCE_GROUP_VERSION
#error "Unsupported resource group API version."
#endif

/* GPU Power version against which was implemented this module */
#define MALI_REQUIRED_GPU_POWER_VERSION 1
#if MALI_REQUIRED_GPU_POWER_VERSION != MALI_GPU_POWER_VERSION
#error "Unsupported gpu power version."
#endif

#define MAX_AW_MASK	0xFFFF

/* GPU time in ms given before switching to another VM */
static int request_timeout = 7;

/* Maximum time in ms given for VM to stop before resetting GPU */
static int yield_timeout = 7;

/* Maximum time (in ms) we wait before powering off the slices */
static int slice_power_off_wait_time = 100;

/* Maximum time in ms given for a driver to stop before resetting GPU,
 * when it is not timesliced with other drivers.
 */
static int no_timeslice_yield_timeout = 5000;

module_param(request_timeout, int, 0644);
MODULE_PARM_DESC(request_timeout,
	"GPU time in ms given before switching to another VM");
module_param(yield_timeout, int, 0644);
MODULE_PARM_DESC(yield_timeout,
	"Max time(ms) for a VM to stop before GPU lost handling is invoked");
module_param(slice_power_off_wait_time, int, 0644);
MODULE_PARM_DESC(slice_power_off_wait_time,
	"Max time(ms) to wait before powering off slices");
module_param(no_timeslice_yield_timeout, int, 0644);
MODULE_PARM_DESC(no_timeslice_yield_timeout,
	"Max time(ms) for a driver to stop before GPU lost handling is invoked,when it is not timesliced with other drivers");


/* Device tree compatible ID of the Arbiter */
#define MALI_ARBITER_DT_NAME "arm,mali-arbiter"

/* Shift used for dvfs.time busy/idle units of (1 << 8) ns
 * This gives a maximum period between utilisation reports of 2^(32+8) ns
 * Exceeding this will cause overflow
 */
#define ARBITER_TIME_SHIFT 8

/* Index into the partitions array for arbitration data when no Partition
 * Manager is in use
 */
#define NO_PTM_IDX 0

/* Mask representing all the slices */
#define MAX_SLICES_MASK 0xFF

/**
 * struct mali_arb_priv_data - private Arbiter VM data used to identify and work
 *                             with each VM
 * @entry:    List entry for req_lists.
 * @info:     Public VM data supplied during registration.
 * @arb:      Pointer to the internal Arbiter data.
 * @vm:       Opaque pointer to the private VM data supplied during
 *            registration.
 * @gpu_lost: true if GPU has currently been lost in this VM.
 *
 * Keep track of the information for each VM registered with the arbiter.
 */
struct mali_arb_priv_data {
	struct list_head entry;
	struct mali_vm_data info;
	struct mali_arb *arb;
	struct mali_vm_priv_data *vm;
	bool gpu_lost;
};

/**
 * struct registered_vm - Stores a pointer to a registered VM
 * @entry:    List entry for reg_vms_list.
 * @vm:       private Arbiter VM data used to identify and work with each VM.
 *
 * Keep track of each VM registered with the arbiter.
 */
struct registered_vm {
	struct list_head entry;
	struct mali_arb_priv_data *vm;
};

/**
 * enum arb_state - Arbiter states
 * @ARB_NO_REQ:       No VMs are requesting GPU from the Arbiter.
 * @ARB_SINGLE_REQ:   A single VM has requested GPU.
 * @ARB_RUNNING:      Multiple VMs are requesting GPU and arbitration
 *                    is required between them.
 * @ARB_GPU_STOPPING: Active VM has been told to stop using GPU and
 *                    the arbiter is waiting for a stopped event.
 *
 * Definition of the possible arbiter's states
 */
enum arb_state {
	ARB_NO_REQ,
	ARB_SINGLE_REQ,
	ARB_RUNNING,
	ARB_GPU_STOPPING,
};

/**
 * struct slice - Arbiter representation of a slice
 * @index: Index of the slice
 * @tmr: Timer to control the slice power
 * @slice_timeout_work: Job to power down the slice
 * @timer_running: Represents whether the time is running
 * @arb: Arbiter handle
 */
struct slice {
	int index;
	struct hrtimer tmr;
	struct work_struct slice_timeout_work;
	bool timer_running;
	struct mali_arb *arb;
};

/**
 * enum mali_arb_partition_reassignment - reassignment states
 * @ARB_REASSIGN_NONE:       No reassignment being done.
 * @ARB_REASSIGN_AW:         Currently reassigning access windows.
 * @ARB_REASSIGN_SLICE:      Currently reassigning slices.
 *
 * Definition of the possible reassignment states
 */
enum mali_arb_partition_reassignment {
	ARB_REASSIGN_NONE,
	ARB_REASSIGN_AW,
	ARB_REASSIGN_SLICE
};

/**
 * struct mali_arb_partition - Arbiter representation of a GPU partition
 * @index: Partition index within its Resource Group
 * @mutex: Mutex to protect partition state
 * @arb: Arbiter instance
 * @active_vm: The currently active VM, or NULL if none are active
 * @hw_virtualization: Whether this partition supports hardware virtualization
 *                     and may be sub-divided into access windows.
 * @access_windows: A bitmask of the access windows assigned to this partition.
 *                  If there is no Partition Manager in use, this field is not
 *                  used.  Set via sysfs
 * @slices: A bitmask of the slices assigned to this partition. If there is no
 *          Partition Manager in use, this field is not used.  Set via sysfs
 * @tmr: Timer used during RUNNING and STOPPING states for
 *       scheduler purposes.
 * @req_list: GPU request list
 * @state: Current arbiter state
 * @wq: Workqueue for safely handling timer events
 * @timeout_work: Timeout work item
 * @timer_running: TRUE if timer is active, FALSE otherwise
 * @last_state_transition: Time of last dvfs state transition
 * @last_dvfs_report: Time of last utilisation report
 * @gpu_active_sum: Running total of active time since last report
 * @gpu_switching_sum: Running total of switching time since last report
 * @reassignment_wait: Wait queue for blocking the calling thread whilst the
 *                     active set AW for the partition is stopped
 * @reassigning: True if a slice or AW assignment is being performed
 * @vm_assign_if: VM assign interfaces
 * @repart_if: Repartition interfaces
 * @aggr_lost_work: Work item for the aggressive GPU Lost timer (only present
 *                  if built with MALI_ARBITER_LOST_TEST define)
 */
struct mali_arb_partition {
	u32 index;
	struct mutex mutex;

	struct mali_arb *arb;
	struct mali_arb_priv_data *active_vm;
	bool hw_virtualization;
	u32 access_windows;
	u32 slices;

	struct hrtimer tmr;
	struct list_head req_list;
	enum arb_state state;
	struct workqueue_struct *wq;
	struct work_struct timeout_work;

	bool timer_running;
	ktime_t last_state_transition;
	ktime_t last_dvfs_report;
	u64 gpu_active_sum;
	u64 gpu_switching_sum;

	wait_queue_head_t reassignment_wait;
	enum mali_arb_partition_reassignment reassigning;

	struct vm_assign_interface vm_assign_if;
	struct repartition_interface repart_if;

};

/**
 * struct max_config - contain max config data
 * @l2_slices: Number of l2 slices assigned.
 * @core_mask: Mask of number of cores in each slice assigned.
 * @updated: True if the max config data has been calculated.
 */
struct max_config {
	uint32_t l2_slices;
	uint32_t core_mask;
	bool updated;
};


/**
 * struct mali_arb - Internal Arbiter state information
 * @arb_data: Embedded public arbiter device data.
 * @dev: Device for arbiter (only single Arbiter device
 *       supported).
 * @mutex: Mutex to protect Arbiter state
 * @sysfs: SysFS data
 * @partitions: Partition representations
 * @curr_gpu_freq: Store the current frequency on which the GPU is running
 * @max_config_info: Max config data computed by the Arbiter
 * @slices: Slices representations
 * @slices_mutex: Mutex to protect slice power and clock data
 * @slices_wq: Work queue to push slice power off jobs
 * @pwr_if: GPU power interface data
 * @pwr_module: GPU power module reference
 * @arb_gpu_power_on: Callback to enable GPU power
 * @arb_gpu_power_off: Callback to disenable GPU power
 * @arb_id: arbiter unique id set by GPU power module
 * @wait_list: Partition-less GPU request list
 * @reg_vms_list: List of VMs registered with the Arbiter
 * @buslogger: Arbiter bus logger data
 * @rg_ops: Resource group operations,
 *          available when hardware separation is supported.
 */
struct mali_arb {
	struct mali_arb_data arb_data;
	struct device *dev;
	struct mutex mutex;
	struct mali_arb_sysfs *sysfs;

	struct mali_arb_partition *partitions[MALI_PTM_PARTITION_COUNT];
	u32 curr_gpu_freq;

	struct max_config max_config_info;
	struct slice *slices[MALI_PTM_SLICES_COUNT];
	struct mutex slices_mutex;
	struct workqueue_struct *slices_wq;

	struct power_interface pwr_if;
	struct module *pwr_module;
	void (*arb_gpu_power_on)(struct device *dev);
	void (*arb_gpu_power_off)(struct device *dev);

	u32 arb_id;
	struct list_head wait_list;
	struct list_head reg_vms_list;
	struct mali_ptm_rg_ops *rg_ops;

};

/* Forward declaration */
static void unregister_vm(struct mali_arb_priv_data *vm);
static void stop_gpu_locked(struct mali_arb_partition *partition);
static void handle_gpu_lost_locked(struct mali_arb_partition *partition);
static void run_next_vm_locked(struct mali_arb_partition *partition);
static void stop_timer_locked(struct mali_arb_partition *partition);
static void gpu_lost_complete_locked(struct mali_arb_priv_data *vm,
	struct mali_arb_partition *partition,
	bool req_again);

/**
 * mali_gpu_get_powered_slices_mask() - Get a mask of the currently powered on
 *                                      slices
 * @arb: Arbiter handle
 * @slice_mask: Mask of slices powered up
 *
 * Return: 0 on success and non-zero on failure
 */
static int mali_gpu_get_powered_slices_mask(struct mali_arb *arb,
	uint32_t *slice_mask)
{
	int ret;

	if (!arb || !slice_mask)
		return -EINVAL;

	if (!arb->rg_ops || !arb->rg_ops->get_powered_slices_mask)
		return -ENODEV;

	ret = arb->rg_ops->get_powered_slices_mask(arb->dev, slice_mask);
	return ret;
}

/**
 * mali_gpu_get_enabled_slices_mask() - Get a mask of the currently enabled
 *                                      slices
 * @arb: Arbiter handle
 * @slice_mask: Mask of slices enabled
 *
 * Return: 0 on success and non-zero on failure
 */
static int mali_gpu_get_enabled_slices_mask(struct mali_arb *arb,
	uint32_t *slice_mask)
{
	int ret;

	if (!arb || !slice_mask)
		return -EINVAL;

	if (!arb->rg_ops || !arb->rg_ops->get_enabled_slices_mask)
		return -ENODEV;

	ret = arb->rg_ops->get_enabled_slices_mask(arb->dev, slice_mask);
	return ret;
}

/**
 * mali_gpu_arbiter_poweron_slices() - Turn on the power to a set of slices
 * @arb: Arbiter handle
 * @slice_mask: mask of slices to be powered up
 *
 * Requests resource group module to power on the requested slices. And if the
 * power down timer is currently running for any requested slice, cancels the
 * timer.
 *
 * Return: 0 on success and non-zero on failure
 */
static int mali_gpu_arbiter_poweron_slices(struct mali_arb *arb,
	uint32_t slice_mask)
{
	int i, ret = 0;
	uint32_t powered_mask = 0;

	if (!arb->rg_ops || !arb->rg_ops->poweron_slices)
		return -ENODEV;

	if (!slice_mask)
		return ret;

	mutex_lock(&arb->slices_mutex);
	for (i = 0; i < MALI_PTM_SLICES_COUNT; i++) {
		if (slice_mask & (0x1 << i)) {
			if (arb->slices[i]->timer_running) {
				arb->slices[i]->timer_running = false;
				hrtimer_cancel(&arb->slices[i]->tmr);
				cancel_work_sync(
					&arb->slices[i]->slice_timeout_work);
			}
		}
	}

	ret = mali_gpu_get_powered_slices_mask(arb, &powered_mask);
	if (ret)
		goto cleanup;

	if ((powered_mask & slice_mask) == slice_mask)
		goto cleanup;

	ret = arb->rg_ops->poweron_slices(arb->dev, slice_mask);

cleanup:
	mutex_unlock(&arb->slices_mutex);
	return ret;
}

/**
 * mali_gpu_arbiter_poweroff_slices() - Turn off the power to a set of slices
 * @arb: Arbiter handle
 * @slice_mask: mask of slices to be powered down
 *
 * This function iteratively checks and cancels the power down timer if running
 * and if the slice is not already OFF, start a timer to power it down at a
 * later point of time.
 *
 * Return: 0 on success and non-zero on failure
 */
static int mali_gpu_arbiter_poweroff_slices(struct mali_arb *arb,
	uint32_t slice_mask)
{
	int i, ret = 0;
	uint32_t powered_mask = 0;

	if (!slice_mask)
		return ret;

	mutex_lock(&arb->slices_mutex);
	ret = mali_gpu_get_powered_slices_mask(arb, &powered_mask);
	if (ret)
		goto cleanup;

	for (i = 0; i < MALI_PTM_SLICES_COUNT; i++) {
		if (slice_mask & (0x1 << i)) {
			if (arb->slices[i]->timer_running) {
				arb->slices[i]->timer_running = false;
				hrtimer_cancel(&arb->slices[i]->tmr);
				cancel_work_sync(
					&arb->slices[i]->slice_timeout_work);
			}
			if (powered_mask & (0x1 << i)) {
				hrtimer_start(&arb->slices[i]->tmr,
					ms_to_ktime(slice_power_off_wait_time),
						HRTIMER_MODE_REL);
				arb->slices[i]->timer_running = true;
			}
		}
	}

cleanup:
	mutex_unlock(&arb->slices_mutex);
	return ret;
}

/**
 * mali_gpu_arbiter_nodelay_poweroff_slices() - Turn off the power to a set
 *                                              of slices with no delay
 * @arb: Arbiter handle
 * @slice_mask: mask of slices to be powered down
 *
 * This function turns off the power to a set of slices immediately with no
 * delay
 *
 * Return: 0 on success and non-zero on failure
 */
static int mali_gpu_arbiter_nodelay_poweroff_slices(struct mali_arb *arb,
	uint32_t slice_mask)
{
	uint32_t powered_mask;
	int res = 0;

	if (!arb->rg_ops || !arb->rg_ops->poweroff_slices)
		return -ENODEV;

	if (!slice_mask)
		return res;

	mutex_lock(&arb->slices_mutex);

	res = mali_gpu_get_powered_slices_mask(arb, &powered_mask);
	if (!res && ((powered_mask & slice_mask) == 0))
		goto cleanup;

	res = arb->rg_ops->poweroff_slices(arb->dev, slice_mask);

cleanup:
	mutex_unlock(&arb->slices_mutex);
	if (res)
		dev_err(arb->dev, "Failed to power down slices");
	return res;
}

/**
 * mali_gpu_arbiter_enable_slices() - Turn on the clock to a set of slices
 *                                    and bring them out of reset
 * @arb: Arbiter handle
 * @slice_mask: mask of slices to be enabled
 *
 * Requests resource group module to enable slices.
 *
 * Return: 0 on success and non-zero on failure
 */
static int mali_gpu_arbiter_enable_slices(struct mali_arb *arb, uint32_t
	slice_mask)
{
	int i, ret = 0;
	uint32_t powered_mask = 0, enabled_mask = 0;
	bool slice_enabled, slice_powered;

	if (!slice_mask)
		return ret;

	if (!arb->rg_ops || !arb->rg_ops->enable_slices)
		return -ENODEV;

	mutex_lock(&arb->slices_mutex);
	ret = mali_gpu_get_powered_slices_mask(arb, &powered_mask);
	if (ret)
		goto cleanup;

	for (i = 0; i < MALI_PTM_SLICES_COUNT; i++) {
		slice_enabled = (slice_mask   & (0x1 << i)) > 0;
		slice_powered = (powered_mask & (0x1 << i)) > 0;

		if (slice_enabled && !slice_powered) {
			dev_err(arb->dev, "S%u: Can't enable this slice\n",
				arb->slices[i]->index);
			ret = -EPERM;
			goto cleanup;
		}
	}

	ret = mali_gpu_get_enabled_slices_mask(arb, &enabled_mask);
	if (ret)
		goto cleanup;

	if ((enabled_mask & slice_mask) == slice_mask)
		goto cleanup;

	ret = arb->rg_ops->enable_slices(arb->dev, slice_mask);

cleanup:
	mutex_unlock(&arb->slices_mutex);
	return ret;
}

/**
 * mali_gpu_arbiter_disable_slices() - Turn off the clock to a set of slices
 *                                     and put them in reset
 * @arb: Arbiter handle
 * @slice_mask: mask of slices to be disabled
 *
 * This function iteratively checks and disables the slices if
 * they are not already disabled. Requests resource group
 * module to disable required slices, post which sets the clock
 * state of each slice if it goes successful.
 *
 * Return: 0 on success and non-zero on failure
 */
static int mali_gpu_arbiter_disable_slices(struct mali_arb *arb,
	uint32_t slice_mask)
{
	int ret = 0;
	uint32_t enabled_mask = 0;

	if (!slice_mask)
		return ret;

	if (!arb->rg_ops || !arb->rg_ops->reset_slices)
		return -ENODEV;

	mutex_lock(&arb->slices_mutex);
	ret = mali_gpu_get_enabled_slices_mask(arb, &enabled_mask);
	if (ret)
		goto cleanup;

	if ((~enabled_mask & slice_mask) == slice_mask)
		goto cleanup;

	ret = arb->rg_ops->reset_slices(arb->dev, slice_mask);

cleanup:
	mutex_unlock(&arb->slices_mutex);
	return ret;
}

/**
 * slice_timer_isr() - Slice timer ISR routine
 * @timer: pointer to the timer object
 *
 * This function queues the slice power down job to the
 * specific slice work queue.
 *
 * Return: HRTIMER_NORESTART, as we don't need this timer to restart
 */
static enum hrtimer_restart slice_timer_isr(struct hrtimer *timer)
{
	struct slice *this_slice;

	this_slice = container_of(timer, struct slice, tmr);

	if (WARN_ON(!this_slice) || WARN_ON(!this_slice->arb))
		return HRTIMER_NORESTART;

	dev_dbg(this_slice->arb->dev, "S%u: hrtimer_isr\n", this_slice->index);

	queue_work(this_slice->arb->slices_wq, &this_slice->slice_timeout_work);
	return HRTIMER_NORESTART;
}

/**
 * slice_timeout_worker() - Job to power down the slice
 * @data: pointer to the work
 *
 * This function requests the resource group module to
 * power off a slice on which the timer had expired and
 * changes the power state if it goes successful.
 *
 * Return: void
 */
static void slice_timeout_worker(struct work_struct *data)
{
	struct slice *this_slice;
	struct mali_arb *arb;
	int ret;

	this_slice = container_of(data, struct slice, slice_timeout_work);
	arb = this_slice->arb;
	dev_dbg(arb->dev, "S%u: Timer has expired...\n", this_slice->index);

	ret = mali_gpu_arbiter_nodelay_poweroff_slices(arb,
			(0x1 << this_slice->index));
	if (ret)
		dev_err(arb->dev, "S%u: Failed to power off slice\n",
			this_slice->index);

	mutex_lock(&arb->slices_mutex);
	this_slice->timer_running = false;
	mutex_unlock(&arb->slices_mutex);
}

/**
 * is_partition_timesliced() - Checks if the Partition is timesliced
 * @access_window_mask: Bitmask of the access windows assigned to the partition
 *
 * Return: True if partition is timesliced with multiple AW otherwise False
 */
static bool is_partition_timesliced(u32 access_window_mask)
{
	return (access_window_mask &&
		(access_window_mask & (access_window_mask - 1)));
}

/**
 * arb_from_data() - Convert arb_data to arb
 * @arb_data: The arbiter device
 *
 * Return: arb data or NULL if input parameter was NULL
 */
static inline struct mali_arb *arb_from_data(struct mali_arb_data *arb_data)
{
	if (likely(arb_data))
		return container_of(arb_data, struct mali_arb, arb_data);
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
	default:
		return "UNKNOWN";
	}
}

/**
 * arb_dvfs_update() - update dvfs data.
 * @partition: Partition
 * @new_state: state being transition to.
 *
 * this function updates the running totals for active and switching times
 * and/or restarts the dvfs timestamp as necessary before we transition state.
 */
static void arb_dvfs_update(struct mali_arb_partition *partition,
	enum arb_state new_state)
{
	struct mali_arb *arb;
	ktime_t now = ktime_get();
	u64 since_last;

	if (WARN_ON(!partition) || WARN_ON(!partition->arb))
		return;

	arb = partition->arb;
	lockdep_assert_held(&arb->mutex);

	since_last = ktime_to_ns(ktime_sub(now,
		partition->last_state_transition));

	switch (partition->state) {
	case ARB_NO_REQ:
		switch (new_state) {
		/* idle --> active */
		case ARB_SINGLE_REQ:
			dev_dbg(arb->dev, "P%u: dvfs: idle --> active\n",
				partition->index);
			partition->last_state_transition = now;
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
			dev_dbg(arb->dev, "P%u dvfs: active --> switching\n",
				partition->index);
			partition->gpu_active_sum += since_last;
			partition->last_state_transition = now;
			break;
		default:
			break;
		}
		break;
	case ARB_GPU_STOPPING:
		switch (new_state) {
		/* switching --> active */
		case ARB_RUNNING:
		case ARB_SINGLE_REQ:
			dev_dbg(arb->dev, "P%u dvfs: switching --> active\n",
				partition->index);
			partition->gpu_switching_sum += since_last;
			partition->last_state_transition = now;
			break;
		/* switching --> idle */
		case ARB_NO_REQ:
			partition->gpu_switching_sum += since_last;
			dev_dbg(arb->dev, "P%u dvfs: switching --> idle\n",
				partition->index);
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
 * stop_aw_for_partition_reassignment_locked() - Stop an active set AW on a
 *                                               partition.
 * @partition: Partition
 * @reassigning_state: New reassigning state to be set
 *
 * If the partition has an access window assigned, we need to get into a state
 * where it is not using the GPU. If the partition state machine is
 * - ARB_RUNNING: Then we need to tell the using KBase to stop, and wait for it
 *                to stop or a GPU_LOST state.
 * - ARB_SINGLE_REQ: Same as ARB_RUNNING, but there is a only AW requesting GPU
 *                   time.
 * - ARB_STOPPING: The using KBase was already in the process of stopping so we
 *                 just need to wait for it, or hit a GPU_LOST.
 * - Anything else: There is no KBase instance using the GPU, so we can just do
 *                  the reassignment.
 *
 * As the sysfs calls need to be synchonous (so an error code can be returned),
 * we need to block this thread whilst the request completes.
 */
static void stop_aw_for_partition_reassignment_locked(
	struct mali_arb_partition *partition,
	enum mali_arb_partition_reassignment reassigning_state)
{
	bool needs_blocking, is_running;

	if (WARN_ON(!partition))
		return;

	if (WARN_ON(reassigning_state == ARB_REASSIGN_NONE))
		return;

	lockdep_assert_held(&partition->mutex);

	is_running = (partition->state == ARB_RUNNING ||
		partition->state == ARB_SINGLE_REQ) && partition->slices;
	needs_blocking = is_running || partition->state == ARB_GPU_STOPPING;

	partition->reassigning = reassigning_state;

	if (needs_blocking) {
		if (is_running) {
			/* Issue a stop command */
			if (partition->timer_running)
				stop_timer_locked(partition);

			stop_gpu_locked(partition);
		}

		/* Wait for the partition to be relinquished */
		mutex_unlock(&partition->mutex);
		wait_event(partition->reassignment_wait,
			!partition->reassigning);
		mutex_lock(&partition->mutex);
	}
	partition->reassigning = ARB_REASSIGN_NONE;
}

/**
 * locked_partition_from_vm_id() - Returns the assigned partition from the
 *                                 given VM id.
 * @arb: Arbiter data
 * @vm_id: VM index
 *
 * Be aware that if found, the partition will be returned locked so the
 * assigned AWs cannot change.
 *
 * Return: Locked owning partition, or NULL if no partition is found
 */
static struct mali_arb_partition *locked_partition_from_vm_id(
	struct mali_arb *arb, u32 vm_id)
{
	struct mali_arb_partition *partition;
	u32 i;

	for (i = 0; i < MALI_PTM_PARTITION_COUNT; ++i) {
		partition = arb->partitions[i];

		if (!partition)
			continue;

		mutex_lock(&partition->mutex);
		if (!partition->hw_virtualization)
			return partition;
		else if (partition->access_windows & (1 << vm_id))
			return partition;

		mutex_unlock(&partition->mutex);
	}

	return NULL;
}

/**
 * get_slice_assignment() - Get the slice assignment for a partition
 * @arb: Arbiter data
 * @partition_index: Partition index
 * @buf: Pointer to a u32 for the resulting assignment mask
 *
 * Return: 0 on success, or error code
 */
static int get_slice_assignment(struct mali_arb *arb, uint8_t partition_index,
	u32 *buf)
{
	struct mali_arb_partition *partition;

	if (partition_index >= MALI_PTM_PARTITION_COUNT)
		return -EINVAL;

	partition = arb->partitions[partition_index];
	if (partition) {
		mutex_lock(&partition->mutex);
		*buf = partition->slices;
		mutex_unlock(&partition->mutex);
	}

	return 0;
}

/**
 * remove_active_vm() - Remove active VM from the partition
 * @partition: Partition
 *
 * Removes the active domain on the arbiter and attempts to unassign and reset
 * the partition or GPU.
 *
 * Return: 0 on success, or error code
 */
static int remove_active_vm(struct mali_arb_partition *partition)
{
	int ret;
	struct vm_assign_ops *ops;

	if (WARN_ON(!partition))
		return -EINVAL;

	lockdep_assert_held(&partition->mutex);
	partition->active_vm = NULL;
	ops = partition->vm_assign_if.ops;
	if (!ops)
		return -EPERM;

	ret = ops->unassign_vm(partition->vm_assign_if.dev);

	return ret;
}

/**
 * set_slice_assignment() - Set the slice assignment for a partition
 * @arb: Arbiter data
 * @partition_index: Partition index
 * @slices: Assigned slice mask
 * @old_slices: Previous slice mask
 *
 * Return: 0 on success, or error code
 */
static int set_slice_assignment(struct mali_arb *arb, uint8_t partition_index,
		u32 slices, u32 *old_slices)
{
	struct mali_arb_partition *partition, *other_partition;
	struct part_cfg_ops *cfg_ops;
	u32 all_slices, slice_mask;
	int ret = 0;
	u32 s_index, p_index;
	struct vm_assign_ops *assign_ops;
	int32_t aw_id;

	if (partition_index >= MALI_PTM_PARTITION_COUNT)
		return -EINVAL;

	if (!old_slices)
		return -EINVAL;

	/* Basic check that the mask only contains bits for the whole GPU */
	if (slices >= (1 << MALI_PTM_SLICES_COUNT))
		return -EINVAL;

	partition = arb->partitions[partition_index];
	if (!partition)
		return -EINVAL;

	if (!arb->rg_ops || !arb->rg_ops->get_slice_mask)
		return -ENODEV;

	mutex_lock(&partition->mutex);
	*old_slices = partition->slices;
	all_slices = partition->slices | slices;

	if (*old_slices == slices)
		goto exit;

	ret = arb->rg_ops->get_slice_mask(arb->dev, &slice_mask);
	if (ret)
		goto exit;

	/* Check the request is allowed */
	for (s_index = 0; s_index < MALI_PTM_SLICES_COUNT; ++s_index) {
		if (!(slices & (1 << s_index)))
			continue;

		/* Check that the requested slice is in this RG */
		if (!(slice_mask & (1 << s_index))) {
			ret = -EINVAL;
			goto exit;
		}

		/* Check that the requested slice is not in another partition */
		for (p_index = 0; p_index < MALI_PTM_PARTITION_COUNT;
			++p_index) {
			other_partition = arb->partitions[p_index];
			if (p_index != partition_index && other_partition &&
				(other_partition->slices & (1 << s_index))) {
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	stop_aw_for_partition_reassignment_locked(partition,
			ARB_REASSIGN_SLICE);

	/* Unlock the partition before repartitioning */
	assign_ops = partition->vm_assign_if.ops;
	if (!assign_ops || !assign_ops->get_assigned_vm) {
		dev_err(arb->dev,
			"P%u: VM assign interface not available.\n",
			partition->index);
		goto exit;
	}
	ret = assign_ops->get_assigned_vm(partition->vm_assign_if.dev, &aw_id);
	if (ret) {
		dev_err(arb->dev, "Failed to get assigned AW");
		goto exit;
	}

	if (aw_id != VM_UNASSIGNED) {
		ret = remove_active_vm(partition);
		if (ret) {
			dev_err(arb->dev, "Partition reset failed");
			goto exit;
		}
	}

	/* Disable the slice clocks, put into reset and turn off the slices */
	ret = mali_gpu_arbiter_disable_slices(arb, all_slices);
	if (ret)
		goto exit;

	ret = mali_gpu_arbiter_poweroff_slices(arb, all_slices);
	if (ret) {
		/* Enable the slices again */
		mali_gpu_arbiter_enable_slices(arb, all_slices);
		goto exit;
	}

	/* Assign the slices */
	cfg_ops = partition->repart_if.ops;
	if (!cfg_ops || !cfg_ops->assign_slices) {
		dev_err(arb->dev,
			"P%u: Repartition interface not available.\n",
			partition->index);
		goto exit;
	}
	ret = cfg_ops->assign_slices(partition->repart_if.dev, slices);
	if (ret) {
		/* Power the slices and bring out of reset on again */
		mali_gpu_arbiter_poweron_slices(arb, all_slices);
		mali_gpu_arbiter_enable_slices(arb, all_slices);
		goto exit;
	}

	partition->slices = slices;
	if (slices) {
		/* Power the slices and bring out of reset on again */
		ret = mali_gpu_arbiter_poweron_slices(arb, slices);
		if (ret)
			goto exit;

		/* Enable the slices again */
		ret = mali_gpu_arbiter_enable_slices(arb, slices);
	}
exit:
	partition->reassigning = ARB_REASSIGN_NONE;
	/* Continue arbitration */
	run_next_vm_locked(partition);

	mutex_unlock(&partition->mutex);
	return ret;
}

/**
 * get_aw_assignment() - Get the per-partition AW assignment
 * @arb: Arbiter data
 * @partition_index: Partition index
 * @buf: Pointer to a u32 for the resulting assignment mask
 *
 * Return: 0 on success, or error code
 */
static int get_aw_assignment(struct mali_arb *arb, uint8_t partition_index,
	u32 *buf)
{
	struct mali_arb_partition *partition;

	if (partition_index >= MALI_PTM_PARTITION_COUNT)
		return -EINVAL;

	partition = arb->partitions[partition_index];
	if (partition) {
		mutex_lock(&partition->mutex);
		*buf = partition->access_windows;
		mutex_unlock(&partition->mutex);
	}

	return 0;
}

/**
 * set_aw_assignment() - Set the AW assignment for a partition
 * @arb: Arbiter data
 * @partition_index: Partition index
 * @access_windows: Assigned AW mask
 * @old_access_windows: Previously assigned AW mask
 *
 * Return: 0 on success, or error code
 */
static int set_aw_assignment(struct mali_arb *arb, uint8_t partition_index,
	u32 access_windows, u32 *old_access_windows)
{
	struct mali_arb_partition *partition, *other_partition;
	int ret = 0;
	u32 current_aws, aw_mask;
	u32 aw, p;
	struct mali_arb_priv_data *cur_vm, *tmp_vm;

	if (access_windows > MAX_AW_MASK) {
		dev_err(arb->dev, "Invalid AW mask = %d\n", access_windows);
		return -EINVAL;
	}

	if (partition_index >= MALI_PTM_PARTITION_COUNT)
		return -EINVAL;

	if (!old_access_windows)
		return -EINVAL;

	partition = arb->partitions[partition_index];
	if (!partition)
		return -EINVAL;

	if (!arb->rg_ops || !arb->rg_ops->get_aw_mask)
		return -ENODEV;

	mutex_lock(&partition->mutex);
	current_aws = partition->access_windows;
	*old_access_windows = current_aws;

	if (current_aws == access_windows)
		goto exit;

	ret = arb->rg_ops->get_aw_mask(arb->dev, &aw_mask);
	if (ret)
		goto exit;

	/* Check the request is allowed */
	for (aw = 0; aw < MALI_PTM_ACCESS_WINDOW_COUNT; ++aw) {
		if (!(access_windows & (1 << aw)))
			continue;

		/* Check that the requested AW is in this RG */
		if (!(aw_mask & (1 << aw))) {
			ret = -EINVAL;
			dev_err(arb->dev, "AW%u not in RG\n", aw);
			goto exit;
		}

		/* Check that the requested AW is not in another partition */
		for (p = 0; p < MALI_PTM_PARTITION_COUNT; ++p) {
			other_partition = arb->partitions[p];
			if (p != partition_index && other_partition &&
				(other_partition->access_windows & (1 << aw))) {
				dev_err(arb->dev, "AW%u already assigned to P%u\n",
					aw, p);
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	stop_aw_for_partition_reassignment_locked(partition,
			ARB_REASSIGN_AW);

	partition->access_windows = access_windows;

	/* Move requests of just assigned AWs from wait queue to request
	 * queue
	 */
	list_for_each_entry_safe(cur_vm, tmp_vm, &arb->wait_list, entry) {
		if ((1 << cur_vm->info.id) & access_windows) {
			list_del_init(&cur_vm->entry);
			list_add_tail(&cur_vm->entry, &partition->req_list);
		}
	}

	/* Move requests of just unassigned AWs from request queue to wait
	 * queue.
	 */
	for (aw = 0; aw < MALI_PTM_ACCESS_WINDOW_COUNT; ++aw) {
		if ((*old_access_windows & (1 << aw)) &&
			!(access_windows & (1 << aw))) {
			list_for_each_entry_safe(cur_vm, tmp_vm,
						&partition->req_list, entry) {
				if (cur_vm->info.id == aw) {
					list_del_init(&cur_vm->entry);
					list_add_tail(&cur_vm->entry,
						&arb->wait_list);
					break;
				}
			}
		}
	}

	/* Continue arbitration */
	run_next_vm_locked(partition);

exit:
	mutex_unlock(&partition->mutex);
	return ret;
}

/**
 * timer_isr() - Called when timer fires as ISR
 * @timer: timer data
 *
 * Queues a workqueue item to handle the event
 *
 * Return: enum hrtimer_restart, which tells status of the timer.
 */
static enum hrtimer_restart timer_isr(struct hrtimer *timer)
{
	struct mali_arb *arb;
	struct mali_arb_partition *partition;

	partition = container_of(timer, struct mali_arb_partition, tmr);

	if (WARN_ON(!partition) || WARN_ON(!partition->arb))
		return HRTIMER_NORESTART;

	arb = partition->arb;

	dev_dbg(arb->dev, "P%u: hrtimer_isr\n", partition->index);
	queue_work(partition->wq, &partition->timeout_work);
	return HRTIMER_NORESTART;
}

/**
 * assign_to_vm() - Assign a partition to a VM
 * @partition: Partition
 * @vm_id:     VM to assign the GPU or partition to
 *
 * If another VM already has the partition, the partition will
 * be reset before assigning to the new VM (or unassigning).
 */
static void assign_to_vm(struct mali_arb_partition *partition, uint32_t vm_id)
{
	int ret = -EPERM;
	struct mali_arb *arb;
	struct vm_assign_ops *ops;

	if (WARN_ON(!partition) || WARN_ON(!partition->arb))
		return;

	arb = partition->arb;
	ops = partition->vm_assign_if.ops;

	if (!ops) {
		dev_err(arb->dev,
			"P%u: VM assign interface not available.\n",
			partition->index);
		goto drop_active;
	}

	ret = ops->assign_vm(partition->vm_assign_if.dev, vm_id);
	if (ret) {
		dev_err(arb->dev,
			"P%u: Failed to assign VM%u to partition/GPU\n",
			partition->index,
			vm_id);
		goto drop_active;
	}
	return;

drop_active:
	if (partition->active_vm != NULL) {
		/* Try assigning to different VM next time */
		list_move_tail(&partition->active_vm->entry,
			&partition->req_list);
		partition->active_vm = NULL;
	}
}

/**
 * grant_gpu_to_next_locked() - Grant the GPU to the next VM.
 * @partition: Partition
 *
 * Assigns the GPU to the first VM on the queue. The mutex lock must already be
 * held when calling this function.
 */
static void grant_gpu_to_next_locked(struct mali_arb_partition *partition)
{
	struct mali_arb_priv_data *prev_vm;
	struct mali_arb *arb;

	if (WARN_ON(!partition) || WARN_ON(!partition->arb))
		return;

	arb = partition->arb;
	prev_vm = partition->active_vm;

	lockdep_assert_held(&partition->mutex);
	if (WARN_ON(list_empty(&partition->req_list))) {
		partition->active_vm = NULL;
		return;
	}

	partition->active_vm = list_first_entry(&partition->req_list,
		struct mali_arb_priv_data, entry);
	if (prev_vm == partition->active_vm)
		return;

	assign_to_vm(partition, partition->active_vm->info.id);

	if (partition->active_vm) {
		partition->active_vm->info.ops.gpu_granted(
				partition->active_vm->vm,
				arb->curr_gpu_freq);
	}
}

/**
 * set_state_locked() - Used to modify the arbiter state
 * @partition: Partition
 * @new: New state.
 *
 * Mutex must be held when calling this function.
 */
static inline void set_state_locked(struct mali_arb_partition *partition,
	enum arb_state new)
{
	struct mali_arb *arb;

	if (WARN_ON(!partition))
		return;

	arb = partition->arb;
	if (WARN_ON(!arb))
		return;

	lockdep_assert_held(&partition->mutex);
	if (partition->state != new) {
		dev_dbg(arb->dev, "%s -> %s\n",
			arb_state_text(partition->state),
			arb_state_text(new));
		arb_dvfs_update(partition, new);
		partition->state = new;
	}
}

/**
 * start_timer_locked() - Start the timer with a given timeout
 * @partition: Partition
 * @timeout: Timeout in milliseconds.
 */
static void start_timer_locked(struct mali_arb_partition *partition,
	unsigned long timeout)
{
	if (WARN_ON(!partition))
		return;

	lockdep_assert_held(&partition->mutex);
	hrtimer_start(&partition->tmr, ms_to_ktime(timeout),
		HRTIMER_MODE_REL);
	partition->timer_running = true;
}

/**
 * stop_timer_locked() - Stop the timer
 * @partition: Partition
 *
 * Note: mutex is temporarily released by the function to clear outstanding work
 */
static void stop_timer_locked(struct mali_arb_partition *partition)
{
	if (WARN_ON(!partition))
		return;

	lockdep_assert_held(&partition->mutex);
	partition->timer_running = false;
	hrtimer_cancel(&partition->tmr);
	mutex_unlock(&partition->mutex);
	cancel_work_sync(&partition->timeout_work);
	mutex_lock(&partition->mutex);
}

/**
 * run_next_vm_locked() - Run the next VM
 * @partition: Partition
 *
 * Makes the necessary state transitions based on the current state and VM list.
 * The mutex lock must already be held when calling this function.
 */
static void run_next_vm_locked(struct mali_arb_partition *partition)
{
	struct mali_arb *arb;
	struct device *pwr_dev;

	if (WARN_ON(!partition) || WARN_ON(!partition->arb))
		return;

	arb = partition->arb;
	pwr_dev = arb->pwr_if.dev;

	lockdep_assert_held(&partition->mutex);

	switch (partition->state) {
	case ARB_RUNNING:
	case ARB_GPU_STOPPING:
		if (partition->timer_running)
			stop_timer_locked(partition);
		break;
	default:
		break;
	}

	if (list_empty(&partition->req_list)) {
		if (partition->state != ARB_NO_REQ) {
			remove_active_vm(partition);
			if (arb->rg_ops)
				mali_gpu_arbiter_nodelay_poweroff_slices(arb,
						partition->slices);
			else if (arb->arb_gpu_power_off)
				arb->arb_gpu_power_off(pwr_dev);
		}
		set_state_locked(partition, ARB_NO_REQ);
	} else if (list_is_singular(&partition->req_list))
		set_state_locked(partition, ARB_SINGLE_REQ);
	else
		set_state_locked(partition, ARB_RUNNING);

	if (partition->state != ARB_NO_REQ && partition->slices) {
		grant_gpu_to_next_locked(partition);
		if (partition->state == ARB_RUNNING)
			start_timer_locked(partition, request_timeout);
	}
}

/**
 * gpu_stopped() - VM KBase driver is in stopped state GPU is not being used
 * @arb_vm: Opaque pointer to the Arbiter VM data populated by the arbiter
 *          during the registration.
 * @req_again: VM has work pending and still wants the GPU
 *
 * VM must no longer use the GPU once this function has been called until
 * gpu_granted has been called by the Arbiter.
 */
static void gpu_stopped(struct mali_arb_priv_data *arb_vm, bool req_again)
{
	struct mali_arb *arb;
	struct device *pwr_dev;
	struct mali_arb_partition *partition;
	struct mali_arb_priv_data *cur_vm;

	if (WARN_ON(!arb_vm) || WARN_ON(!arb_vm->arb))
		return;
	arb = arb_vm->arb;
	pwr_dev = arb->pwr_if.dev;

	partition = locked_partition_from_vm_id(arb, arb_vm->info.id);
	if (req_again && !partition) {
		/* We may have received this stopped after an arbiter restart
		 * and before a partition has been re-assigned to vm.
		 */
		list_for_each_entry(cur_vm, &arb->wait_list, entry) {
			if (cur_vm == arb_vm)
				return;
		}
		dev_dbg(arb->dev,
		  "Adding VM %u to partitionless wait list (after a restart)\n",
		  arb_vm->info.id);
		list_add_tail(&arb_vm->entry, &arb->wait_list);
		return;
	} else if (!partition) {
		dev_warn(arb->dev,
			"GPU_STOPPED from VM %u not assigned to a partition\n",
			arb_vm->info.id);
		return;
	}

	dev_dbg(arb->dev, "P%u: %u, req_again: %d\n", partition->index,
		arb_vm->info.id, req_again);


	if (!list_empty(&arb_vm->entry)) {
		if (!req_again)
			list_del_init(&arb_vm->entry);

		if (arb_vm == partition->active_vm) {
			if (req_again)
				/* ensure GRANTED is sent again */
				list_move_tail(&arb_vm->entry,
						&partition->req_list);
			remove_active_vm(partition);

			if (!partition->reassigning)
				run_next_vm_locked(partition);
		}
	} else if (req_again) {
		/* The VM was previously forcibly removed because it didn't
		 * release the GPU in time. The VM is no longer in the arbiter
		 * request list so it needs to be added again if the req_again
		 * parameter is true, otherwise the VM will not be able to
		 * access the GPU.
		 */
		dev_dbg(arb->dev,
			"P%u - vm->entry is empty. requesting gpu for VM %u\n",
			partition->index, arb_vm->info.id);
		if (list_empty(&partition->req_list)) {
			if (arb->rg_ops)
				mali_gpu_arbiter_poweron_slices(arb,
						partition->slices);
			else if (arb->arb_gpu_power_on)
				arb->arb_gpu_power_on(pwr_dev);
		}
		list_add_tail(&arb_vm->entry, &partition->req_list);
		if (partition->state == ARB_NO_REQ ||
			partition->state == ARB_SINGLE_REQ) {
			WARN_ON(partition->timer_running);

			if (!partition->reassigning)
				run_next_vm_locked(partition);
		}
	}

	gpu_lost_complete_locked(arb_vm, partition, req_again);

	/* Wake up the slice assignment thread */
	if (partition->reassigning) {
		partition->reassigning = ARB_REASSIGN_NONE;
		wake_up(&partition->reassignment_wait);
	}

	mutex_unlock(&partition->mutex);
}

/**
 * gpu_request() - Request GPU time
 * @arb_vm: Opaque pointer to the Arbiter VM data populated by the arbiter
 *          during the registration.
 *
 * Called by a VM comms device to request GPU time. The Arbiter should respond
 * when ready with a callback to gpu_granted.
 */
static void gpu_request(struct mali_arb_priv_data *arb_vm)
{
	struct mali_arb_priv_data *cur_vm;
	struct mali_arb *arb;
	struct device *pwr_dev;
	struct mali_arb_partition *partition;

	if (WARN_ON(!arb_vm) || WARN_ON(!arb_vm->arb))
		return;

	arb = arb_vm->arb;
	pwr_dev = arb->pwr_if.dev;

	partition = locked_partition_from_vm_id(arb, arb_vm->info.id);
	if (!partition) {
		list_for_each_entry(cur_vm, &arb->wait_list, entry) {
			if (cur_vm == arb_vm)
				return;
		}
		list_add_tail(&arb_vm->entry, &arb->wait_list);
		dev_warn(arb->dev,
			"GPU_REQUEST from VM %u not assigned to a partition\n",
			arb_vm->info.id);
		return;
	}

	gpu_lost_complete_locked(arb_vm, partition, true);


	if (list_empty(&partition->req_list)) {
		if (arb->rg_ops)
			mali_gpu_arbiter_poweron_slices(arb,
					partition->slices);
		else if (arb->arb_gpu_power_on)
			arb->arb_gpu_power_on(pwr_dev);
	}

	list_for_each_entry(cur_vm, &partition->req_list, entry) {
		if (cur_vm == arb_vm) {
			dev_dbg(arb->dev,
			    "P%u: VM %u requested GPU when already requested\n",
			    partition->index, arb_vm->info.id);
			/* If GPU_REQUEST arrives after sending GPU_STOP
			 * then it is likely that a GPU_STOPPED message was
			 * missed.
			 * Adjust the state to stopped and add this VM to the
			 * stopped-requested list
			 */
			mutex_unlock(&partition->mutex);
			gpu_stopped(arb_vm, true);
			return;
		}
	}
	list_add_tail(&arb_vm->entry, &partition->req_list);

	/* It might be the case that the active VM was requested to stop
	 * but did not do so in time. If there were no other VMs waiting
	 * to run then GPU lost will not have been done. Now there is a VM
	 * requesting the GPU we should forcibly remove it.
	 */
	if (partition->state == ARB_GPU_STOPPING && !partition->timer_running) {
		dev_dbg(arb->dev, "New VM request, GPU lost now required\n");
		handle_gpu_lost_locked(partition);
	}

	if (partition->state == ARB_NO_REQ ||
		partition->state == ARB_SINGLE_REQ)
		run_next_vm_locked(partition);

	mutex_unlock(&partition->mutex);
}

static void stop_gpu_locked(struct mali_arb_partition *partition)
{
	struct mali_arb *arb;
	struct mali_arb_priv_data *arb_vm;
	int new_yield_timeout = yield_timeout;

	arb = partition->arb;
	if (WARN_ON(!arb))
		return;

	lockdep_assert_held(&partition->mutex);

	arb_vm = partition->active_vm;
	if (!arb_vm) {
		dev_warn(arb->dev,
				"P%u: GPU_STOP cannot be issued without an active VM.\n",
				partition->index);
		return;
	}

	set_state_locked(partition, ARB_GPU_STOPPING);

	if ((partition->reassigning == ARB_REASSIGN_SLICE) &&
			!(is_partition_timesliced(partition->access_windows))) {
		new_yield_timeout = no_timeslice_yield_timeout;
	}

	start_timer_locked(partition, new_yield_timeout);
	arb_vm->info.ops.gpu_stop(partition->active_vm->vm);
}

/**
 * handle_gpu_lost_locked() - Remove GPU from misbehaving VM
 * @partition: Partition
 *
 * Forcible assign the GPU away from the currently active domain to
 * another VM in the request queue, then send a GPU lost event.
 */
static void handle_gpu_lost_locked(struct mali_arb_partition *partition)
{
	struct mali_arb *arb;
	struct mali_arb_priv_data *arb_vm;
	struct mali_vm_priv_data *vm;

	arb = partition->arb;
	if (WARN_ON(!arb))
		return;

	lockdep_assert_held(&partition->mutex);

	arb_vm = partition->active_vm;
	if (!arb_vm) {
		dev_warn(arb->dev,
		       "P%u: GPU_LOST cannot be issued without an active VM.\n",
		       partition->index);
		return;
	}

	vm = arb_vm->vm;
	if (!vm) {
		dev_warn(arb->dev, "P%u: Private VM data not available.\n",
			partition->index);
		return;
	}

	dev_info(arb->dev, "P%u: GPU took too long to yield for VM%u\n",
			partition->index, arb_vm->info.id);

	list_del_init(&partition->active_vm->entry);
	remove_active_vm(partition);

	/* Only switch to another AW if not performing a slice reassignment.
	 * partition->reassigning is always false when running in Xen/PV mode
	 */
	if (!partition->reassigning) {
		run_next_vm_locked(partition);
	} else {
		partition->reassigning = ARB_REASSIGN_NONE;
		wake_up(&partition->reassignment_wait);
	}

	arb_vm->gpu_lost = true;

	arb_vm->info.ops.gpu_lost(vm);
}

/**
 * gpu_lost_complete_locked() - Cleanup after GPU lost completion
 * @arb_vm:     Internal Arbiter data for VM
 * @partition:  Partition
 * @req_again:  GPU still required
 *
 * Clear GPU lost flags and power down GPU if necessary.
 */
static void gpu_lost_complete_locked(struct mali_arb_priv_data *arb_vm,
				struct mali_arb_partition *partition,
				bool req_again)
{
	struct mali_arb *arb;

	lockdep_assert_held(&partition->mutex);

	if (!arb_vm || !arb_vm->arb)
		return;

	arb = arb_vm->arb;
	if (arb_vm->gpu_lost) {
		arb_vm->gpu_lost = false;
		dev_dbg(arb->dev, "VM%u GPU lost complete\n", arb_vm->info.id);
	}
}

/**
 * timeout_worker() - Called when timer expires
 * @data: Used to obtain arbiter data
 *
 * Handles the timeout cases in the RUNNING and GPU_STOPPING states
 */
static void timeout_worker(struct work_struct *data)
{
	struct mali_arb_partition *partition;
	struct mali_arb_priv_data *arb_vm;
	struct mali_arb *arb;

	if (WARN_ON(!data))
		return;

	partition = container_of(data, struct mali_arb_partition, timeout_work);

	arb = partition->arb;
	if (WARN_ON(!arb))
		return;

	dev_dbg(arb->dev, "P%u: Timer has expired...\n", partition->index);

	mutex_lock(&partition->mutex);
	if (!partition->timer_running)
		goto cleanup_mutex;

	partition->timer_running = false;
	arb_vm = partition->active_vm;
	if (!arb_vm) {
		dev_dbg(arb->dev,
			"P%u: Timer cannot be pending without an active VM.\n",
			partition->index);
		goto cleanup_mutex;
	}
	switch (partition->state) {
	case ARB_RUNNING:
		dev_dbg(arb->dev, "P%u: GPU time up - suspending.\n",
			partition->index);
		stop_gpu_locked(partition);
		break;
	case ARB_GPU_STOPPING:
		/* Only forcibly remove the GPU if there is another VM waiting
		 * or the partition is under reassignment. Otherwise, allow the
		 * VM to take longer.
		 */
		if (partition->reassigning ||
			!list_is_singular(&partition->req_list))
			handle_gpu_lost_locked(partition);
		else
			dev_dbg(arb->dev,
				"GPU time is up but no VMs waiting.\n");
		break;
	default:
		break;
	}

cleanup_mutex:
	mutex_unlock(&partition->mutex);
}


/**
 * reg_assign_if() - Register VM assign interface
 * @arb_data:  Arbiter public data exposed (see mali_arb_data).
 * @vm_assign: GPU to VM assign interfaces.
 *
 * Register a VM assign interface to be used by the Arbiter to assign a GPU or
 * Partition to a specific VM. If hardware separation is not supported, just the
 * interface index zero (see vm_assign_interface) should be registered.
 *
 * Return: 0 is successful, or a standard Linux error code
 */
static int reg_assign_if(struct mali_arb_data *arb_data,
				struct vm_assign_interface vm_assign)
{
	int ret = 0;
	struct mali_arb_partition *partition;
	struct mali_arb *arb;

	if (WARN_ON(!arb_data || !vm_assign.ops))
		return -EINVAL;

	if (unlikely(!vm_assign.ops->assign_vm ||
				!vm_assign.ops->unassign_vm)) {
		pr_err("%s: Missing VM assign interface callbacks", __func__);
		return -EINVAL;
	}

	arb = arb_from_data(arb_data);
	if (!arb) {
		pr_err("%s: Arbiter not initialized or arb_data invalid",
			__func__);
		return -EPERM;
	}

	partition = arb->partitions[vm_assign.if_id];
	if (!partition) {
		dev_err(arb->dev,
			"No GPU/Partition built for the interface ID %d",
			vm_assign.if_id);
		ret = -EPERM;
		goto cleanup_mutex;
	}
	mutex_lock(&partition->mutex);
	if (unlikely(partition->vm_assign_if.ops)) {
		dev_warn(arb->dev, "VM assign interface already registered");
		ret = -EBUSY;
		goto cleanup_mutex;
	}

	/* register VM Assign interface */
	partition->vm_assign_if = vm_assign;
cleanup_mutex:
	mutex_unlock(&partition->mutex);
	return ret;
}

/**
 * unreg_assign_if() - Unregister VM assign interface
 * @arb_data: Arbiter public data exposed (see mali_arb_data).
 * @if_id:    Index of the interface to be unregistered. Should be zero in case
 *            hardware separation is not supported.
 *
 * Unregister the VM assign interface of index if_id from the Arbiter. If
 * hardware separation is not supported, just the interface index zero (see
 * vm_assign_interface) should have been registered (see arb_reg_assign_if).
 */
static void unreg_assign_if(struct mali_arb_data *arb_data, uint32_t if_id)
{
	struct mali_arb *arb;
	struct mali_arb_partition *partition;

	if (likely(arb_data)) {
		arb = arb_from_data(arb_data);
		if (!arb)
			return;
		partition = arb->partitions[if_id];
		if (!partition)
			return;
		/* unregister VM Assign interface */
		if (!partition->vm_assign_if.ops)
			return;
		mutex_lock(&partition->mutex);
		partition->vm_assign_if.if_id = NO_PTM_IDX;
		partition->vm_assign_if.dev = NULL;
		partition->vm_assign_if.ops = NULL;
		mutex_unlock(&partition->mutex);
	}
}

/**
 * is_vm_active() - Checks if the VM is active in any partition
 * @arb:       Arbiter data
 * @vm_id:     VM index
 * @partition: Partition which the VM is active and assigned to
 *
 * Checks if the VM is active in any partition and returns it.
 *
 * Return: true if the vm_id is active in any partition, false if not.
 */
static bool is_vm_active(struct mali_arb *arb, uint32_t vm_id,
	struct mali_arb_partition **partition)
{
	struct mali_arb_partition *tmp_partition;
	struct vm_assign_ops *assign_ops;
	int32_t aw_id;
	int i, err;

	if (!arb || !partition)
		return false;

	for (i = 0; i < MALI_PTM_PARTITION_COUNT; ++i) {
		tmp_partition = arb->partitions[i];

		if (tmp_partition) {
			assign_ops = tmp_partition->vm_assign_if.ops;
			if (!assign_ops || !assign_ops->get_assigned_vm) {
				dev_dbg(arb->dev,
					"P%u: get assigned VM not available.\n",
					tmp_partition->index);
				continue;
			}
			mutex_lock(&tmp_partition->mutex);
			err = assign_ops->get_assigned_vm(
					tmp_partition->vm_assign_if.dev,
					&aw_id);
			mutex_unlock(&tmp_partition->mutex);
			if (vm_id == aw_id) {
				*partition = tmp_partition;
				return true;
			}
		}
	}
	return false;
}

/**
 * register_vm() - Register a VM comms device
 * @arb_data: Arbiter public data exposed (see mali_arb_data).
 * @vm_info: The VM info data populated by caller (see mali_vm_data).
 * @vm: Opaque pointer to the private VM data used by the backend to identify
 *      the VMs. The Arbiter must pass this pointer back in the VM callbacks
 *      every time it needs to communicate to this VM being registered by this
 *      function.
 * @arb_vm: Opaque pointer to the private Arbiter VM data used by the Arbiter to
 *          identify and work with each VM. This is populated by the Arbiter in
 *          this function. The backend must pass this pointer back in the
 *          Arbiter callbacks every time this VM being registered needs to
 *          communicate with the Arbiter.
 *
 * This function must be called first before a VM comms device instance can use
 * the arbiter functionality.
 *
 * Return: 0 is successful, or a standard Linux error code
 */
static int register_vm(struct mali_arb_data *arb_data,
	struct mali_vm_data *vm_info,
	struct mali_vm_priv_data *vm,
	struct mali_arb_priv_data **arb_vm)
{
	struct mali_arb *arb;
	struct mali_arb_priv_data *priv_vm;
	struct registered_vm *reg_vm;
	struct mali_arb_partition *partition;

	if (WARN_ON(!arb_data || !vm_info || !vm || !arb_vm)) {
		if (arb_vm)
			*arb_vm = NULL;
		return -EINVAL;
	}

	if (unlikely(vm_info->dev == NULL ||
			vm_info->ops.gpu_granted == NULL ||
			vm_info->ops.gpu_stop == NULL ||
			vm_info->ops.gpu_lost == NULL))
		return -EINVAL;

	arb = arb_from_data(arb_data);

	list_for_each_entry(reg_vm, &arb->reg_vms_list, entry) {
		if (reg_vm->vm->info.id == vm_info->id) {
			dev_warn(arb->dev, "VM %u already registered.\n",
				vm_info->id);
			*arb_vm = reg_vm->vm;
			return 0;
		}
	}

	dev_dbg(arb->dev, "Regstering VM %u\n", vm_info->id);

	priv_vm = devm_kzalloc(arb->dev, sizeof(struct mali_arb_priv_data),
		GFP_KERNEL);
	if (!priv_vm)
		return -ENOMEM;

	priv_vm->info = *vm_info;
	priv_vm->arb = arb;
	priv_vm->gpu_lost = false;
	priv_vm->vm = vm;
	INIT_LIST_HEAD(&priv_vm->entry);

	reg_vm = devm_kzalloc(arb->dev, sizeof(struct registered_vm),
		GFP_KERNEL);
	if (!reg_vm)
		return -ENOMEM;

	reg_vm->vm = priv_vm;
	INIT_LIST_HEAD(&reg_vm->entry);
	list_add_tail(&reg_vm->entry, &arb->reg_vms_list);

	*arb_vm = priv_vm;

	/* If any AWs already assigned then we've restarted, so STOP them */
	if (is_vm_active(arb, vm_info->id, &partition) && partition) {
		mutex_lock(&partition->mutex);
		partition->active_vm = priv_vm;
		stop_gpu_locked(partition);
		mutex_unlock(&partition->mutex);
	}

	return 0;
}

/**
 * unregister_vm() - Unregister a VM comms device
 * @arb_vm: Opaque pointer to the Arbiter VM data populated by the arbiter
 * during the registration.
 *
 * This function must be called before the VM comms device is unloaded to sever
 * the connection.
 */
static void unregister_vm(struct mali_arb_priv_data *arb_vm)
{
	struct mali_arb *arb;
	uint32_t vm_id;
	struct mali_arb_partition *partition;
	struct registered_vm *reg_vm;

	if (WARN_ON(!arb_vm) || WARN_ON(!arb_vm->arb))
		return;
	arb = arb_vm->arb;
	vm_id = arb_vm->info.id;

	partition = locked_partition_from_vm_id(arb, vm_id);
	if (!partition)
		return;

	if (!list_empty(&arb_vm->entry))
		list_del(&arb_vm->entry);

	if (arb_vm == partition->active_vm) {
		remove_active_vm(partition);
		run_next_vm_locked(partition);
	}
	gpu_lost_complete_locked(arb_vm, partition, false);

	mutex_unlock(&partition->mutex);

	list_for_each_entry(reg_vm, &arb->reg_vms_list, entry) {
		if (reg_vm->vm->info.id == vm_id) {
			list_del(&reg_vm->entry);
			break;
		}
	}
	devm_kfree(arb->dev, reg_vm);
	devm_kfree(arb->dev, arb_vm);

	dev_dbg(arb->dev, "%s %u\n", __func__, vm_id);
}

/**
 * gpu_active() - Notify Arbiter that GPU is being used
 * @arb_vm: Opaque pointer to the Arbiter VM data populated by the arbiter
 *          during the registration.
 *
 * Called by a VM comms device to notify the Arbiter that the VM is busy using
 * the GPU. This information can be useful in making scheduling decisions.
 */
static void gpu_active(struct mali_arb_priv_data *arb_vm)
{
	struct mali_arb_partition *partition;
	struct mali_arb *arb;

	if (WARN_ON(!arb_vm) || WARN_ON(!arb_vm->arb))
		return;

	arb = arb_vm->arb;

	partition = locked_partition_from_vm_id(arb, arb_vm->info.id);
	if (!partition) {
		dev_warn(arb->dev,
			"GPU_ACTIVE from VM %u not assigned to a partition\n",
			arb_vm->info.id);
		return;
	}

	dev_dbg(arb->dev, "P%u: GPU %u is active\n", partition->index,
		arb_vm->info.id);
	mutex_unlock(&partition->mutex);
}

/**
 * gpu_idle() - Notify Arbiter that GPU is not being used
 * @arb_vm: Opaque pointer to the Arbiter VM data populated by the arbiter
 *          during the registration.
 *
 * Called by a VM to notify the Arbiter that it is no longer using the GPU. This
 * information can be useful in making scheduling decisions.
 */
static void gpu_idle(struct mali_arb_priv_data *arb_vm)
{
	struct mali_arb *arb;
	struct mali_arb_partition *partition;
	struct mali_arb_priv_data *cur_vm, *tmp_vm;

	if (WARN_ON(!arb_vm) || WARN_ON(!arb_vm->arb))
		return;
	arb = arb_vm->arb;

	partition = locked_partition_from_vm_id(arb, arb_vm->info.id);
	if (!partition) {
		/* Remove from wait list if exists */
		list_for_each_entry_safe(cur_vm, tmp_vm,
			&arb->wait_list, entry) {
			if (cur_vm == arb_vm) {
				list_del_init(&cur_vm->entry);
				break;
			}
		}
		dev_warn(arb->dev,
			"GPU_IDLE from VM %u not assigned to a partition\n",
			arb_vm->info.id);
		return;
	}


	if (arb_vm == partition->active_vm &&
		(partition->state == ARB_RUNNING ||
			partition->state == ARB_SINGLE_REQ)) {
		dev_dbg(arb->dev, "P%u: GPU %u is idle - giving up...\n",
			partition->index, arb_vm->info.id);
		if (partition->state == ARB_RUNNING) {
			stop_timer_locked(partition);

			/* The act of stopping the timer requires that any
			 * outstanding work be completed which involves
			 * unlocking the mutex.  If the timer has already
			 * expired, this will cause a GPU_STOP event to be sent
			 * to the client, so we have to check again here
			 */
			if (partition->state == ARB_GPU_STOPPING)
				goto exit;
		}

		stop_gpu_locked(partition);
	} else
		dev_dbg(arb->dev, "P%u: GPU %u is idle\n", partition->index,
			arb_vm->info.id);

exit:
	mutex_unlock(&partition->mutex);
}

/**
 * get_utilisation() - Get utilization information
 * @dev: Pointer to mali_arb structure
 * @gpu_busytime: will contain the GPU busy time
 * @gpu_totaltime: will contain the GPU total time
 *
 * Called from GPU power integration driver for DVFS to get GPU busytime and
 * totaltime since last request
 * This callback should not call back the power module or it will cause
 * deadlocks.
 */
static void get_utilisation(void *dev, u32 *gpu_busytime,
	u32 *gpu_totaltime)
{
	struct mali_arb *arb = (struct mali_arb *) dev;
	struct mali_arb_partition *partition;
	ktime_t now;
	u64 time_since_last_report, time_since_last_transition, last_percent;
	u32 i;

	if (WARN_ON(!dev || !gpu_busytime || !gpu_totaltime))
		return;

	*gpu_busytime = 0;
	*gpu_totaltime = 0;

	/* Find the partition with the maximum utilisation */
	last_percent = 0;
	for (i = 0; i < ARRAY_SIZE(arb->partitions); ++i) {
		u64 percent = 0;
		u32 busytime, totaltime;

		partition = arb->partitions[i];
		if (!partition)
			continue;

		mutex_lock(&partition->mutex);
		now = ktime_get();
		time_since_last_report = ktime_to_ns(ktime_sub(now,
			partition->last_dvfs_report));
		time_since_last_transition = ktime_to_ns(ktime_sub(now,
			partition->last_state_transition));

		switch (partition->state) {
		case ARB_RUNNING:
		case ARB_SINGLE_REQ: /* active */
			partition->gpu_active_sum += time_since_last_transition;
			break;
		case ARB_GPU_STOPPING: /* switching */
			partition->gpu_switching_sum +=
				time_since_last_transition;
			break;
		default:
			break;
		}

		/* get GPU time and log utilisation info */
		busytime = partition->gpu_active_sum >> ARBITER_TIME_SHIFT;
		totaltime = (time_since_last_report -
			partition->gpu_switching_sum) >> ARBITER_TIME_SHIFT;

		/* Do not divide by zero... */
		if (totaltime == 0) {
			mutex_unlock(&partition->mutex);
			continue;
		}

		percent = ((u64)busytime * 100) / totaltime;
		if (last_percent < percent) {
			last_percent = percent;
			*gpu_busytime = busytime;
			*gpu_totaltime = totaltime;
		}

		dev_dbg(arb->dev,
			"Arbiter_%d P%u reporting busy %u, total %u (~ %llu%%)\n",
			arb->arb_id, partition->index, busytime,
			totaltime, percent);

		/* Reset counters for next time */
		partition->gpu_active_sum = 0;
		partition->gpu_switching_sum = 0;
		partition->last_state_transition = now;
		partition->last_dvfs_report = now;

		mutex_unlock(&partition->mutex);
	}

	WARN_ON(*gpu_busytime > *gpu_totaltime);
}

/**
 * update_freq() - Get notified on frequency change
 * @dev: Pointer to mali_arb structure
 * @new_freq: updated frequency
 *
 * Called from GPU power integration driver for DVFS to get
 * notification when frequency changes.
 * This callback should not call back the power module or it will cause
 * deadlocks.
 */
static void update_freq(void *dev, u32 new_freq)
{
	struct mali_arb *arb = (struct mali_arb *) dev;
	struct mali_arb_partition *partition;
	int i, num_partition = 0;

	if (WARN_ON(!dev) || WARN_ON(!arb))
		return;

	arb->curr_gpu_freq = new_freq;

	num_partition = ARRAY_SIZE(arb->partitions) - 1;
	i = 0;

	for (; i <= num_partition; ++i) {
		partition = arb->partitions[i];
		if (!partition)
			continue;

		mutex_lock(&partition->mutex);

		/*
		 * Notify only a PE currently active on the partition and only
		 * if the partition is not stopping, otherwise there is a risk
		 * that the granted message could overwrite the stop message or
		 * give the VM the idea that the GPU is granted again, which is
		 * not the case.
		 */
		if (partition->active_vm &&
					partition->state != ARB_GPU_STOPPING)
			partition->active_vm->info.ops.gpu_granted(
				partition->active_vm->vm,
				arb->curr_gpu_freq);

		mutex_unlock(&partition->mutex);
	}
}

/**
 * arbiter_get_pwr() - gets the GPU Power interfaces
 * @arb: Arbiter data
 *
 * This function searches in the DT the presence of the mali_gpu_power node.
 * If the definition of the node is found, the function will attempt to take
 * a reference to its module and in case of error will return a EPROBE_DEFER.
 * If the definition of the node is not found, the function will return success
 * without any further operation.
 *
 * NOTE:If this function finds the GPU power device in the DT and the module is
 * loaded, it will take a referece count to the underling module which therefore
 * will need to be release outside the function whenever not used anymore.
 *
 * Return: 0 on success, or error code
 *
 */
static int arbiter_get_pwr(struct mali_arb *arb)
{
	struct device_node *pwr_node;
	struct platform_device *pwr_pdev;
	struct mali_gpu_power_data *pwr_data;
	int err = 0;

	if (WARN_ON(!arb))
		return -EINVAL;

	/* find the gpu power device in Device Tree */
	pwr_node = of_parse_phandle(arb->dev->of_node, "platform", 0);
	if (!pwr_node) {
		dev_info(arb->dev,
			"No GPU Power in Arbiter Device Tree Node\n");
		/* no arbiter interface defined in device tree */
		goto exit;
	}

	pwr_pdev = of_find_device_by_node(pwr_node);
	if (!pwr_pdev) {
		dev_err(arb->dev, "Failed to find GPU Power device\n");
		err = -EPROBE_DEFER;
		goto exit;
	}

	if (!pwr_pdev->dev.driver ||
			!pwr_pdev->dev.driver->owner ||
			!try_module_get(pwr_pdev->dev.driver->owner)) {
		dev_err(arb->dev, "GPU Power module not available\n");
		err = -EPROBE_DEFER;
		goto exit;
	}
	arb->pwr_if.dev = &pwr_pdev->dev;
	arb->pwr_module = pwr_pdev->dev.driver->owner;

	pwr_data = platform_get_drvdata(pwr_pdev);
	if (!pwr_data) {
		dev_err(arb->dev, "GPU Power device not ready\n");
		err = -EPROBE_DEFER;
		goto cleanup_module;
	}
	arb->pwr_if.ops = &pwr_data->ops;

	return 0;

cleanup_module:
	module_put(arb->pwr_module);
	arb->pwr_module = NULL;
exit:
	return err;
}

/**
 * arbiter_init_pwr() - Initializes the GPU Power interfaces
 * @arb: Arbiter data
 *
 * This function initializes the GPU Power interfaces in the arbiter and
 * registers relevant Arbiter ops with the GPU Power.
 *
 * Return: 0 on success, or error code
 *
 */
static int arbiter_init_pwr(struct mali_arb *arb)
{
	struct mali_gpu_power_arbiter_cb_ops ops;
	int err;

	if (WARN_ON(!arb))
		return -EINVAL;

	if (!arb->pwr_if.ops || !arb->pwr_if.dev) {
		dev_err(arb->dev, "GPU Power interface data not valid.\n");
		return -EINVAL;
	}

	ops.arb_get_utilisation = get_utilisation;
	ops.update_freq = update_freq;

	/* register arbiter callbacks */
	if (arb->pwr_if.ops->register_arb == NULL ||
			arb->pwr_if.ops->unregister_arb == NULL) {
		dev_err(arb->dev, "GPU Power callbacks are not available.\n");
		return -EFAULT;
	}

	/*
	 * arb->arb_id will be updated by GPU power node when arbiter registers
	 */
	err = arb->pwr_if.ops->register_arb(arb->pwr_if.dev,
						arb, &ops, &arb->arb_id);
	if (err) {
		dev_err(arb->dev,
			"Failed to register arbiter with GPU power device\n");
		return -err;
	}

	arb->arb_gpu_power_on = arb->pwr_if.ops->gpu_active;
	arb->arb_gpu_power_off = arb->pwr_if.ops->gpu_idle;

	return 0;
}


/**
 * get_max_l2_slices() - Calculates max l2 slices data
 * @dev: Resource Group module data
 * @max_l2_slices: pointer to variable to receive max l2 slices data
 *
 * Calculate max_l2_slices value which is considered the number of L2 slices
 * allocated to the resource group which this arbiter is associated with
 * (1 to 1 relationship between arbiter and resource group)
 *
 * Return: 0 on success, standard linux error code otherwise
 */
static int get_max_l2_slices(struct device *dev,
		uint32_t *max_l2_slices)
{
	uint32_t slice_mask;
	uint32_t block_l2_num = 0;
	struct mali_ptm_rg_ops *rg_ops;
	int err;

	if (!dev || !max_l2_slices)
		return -EINVAL;

	rg_ops = dev_get_drvdata(dev);
	if (!rg_ops || !rg_ops->get_slice_mask) {
		dev_dbg(dev,
			"Setup does not support variable L2 slices number.\n");
		*max_l2_slices = 0;
		return 0;
	}

	err = rg_ops->get_slice_mask(dev, &slice_mask);
	if (err)
		return err;

	*max_l2_slices = 0;

	while (slice_mask) {
		if (slice_mask & 0x1) {
			block_l2_num++;
		} else {
			if (block_l2_num > *max_l2_slices)
				*max_l2_slices = block_l2_num;
			block_l2_num = 0;
		}
		slice_mask >>= 1;
	}

	if (block_l2_num > *max_l2_slices)
		*max_l2_slices = block_l2_num;

	return 0;
}

/**
 * get_max_core_mask() - Calculates max core mask data
 * @dev: Resource Group module data
 * @max_core_mask: pointer to variable to receive max core mask data
 *
 * Calculate max_core_mask value which is considered to be all the l2 slices
 * assigned to the resource group and the shader_cores on each of them and we
 * calculate the max_core_mask bitmap as the smallest superset that would
 * represent all of them. So any possible combination of contiguous
 * l2 slices word have a core_mask that would be a subgroup of
 * the max_core_mask.
 *
 * Return: 0 on success, standard linux error code otherwise
 */
static int get_max_core_mask(struct device *dev,
		uint32_t *max_core_mask)
{
	uint32_t slice_mask;
	uint8_t core_mask_stride;
	uint32_t block_core_mask = 0;
	uint32_t core_mask;
	uint64_t slices_core_mask = 0;
	struct mali_ptm_rg_ops *rg_ops;
	int err;
	int slice_count = 0;

	if (!max_core_mask)
		return -EINVAL;

	rg_ops = dev_get_drvdata(dev);
	if (!rg_ops || !rg_ops->get_slice_mask ||
			!rg_ops->get_slices_core_mask) {
		dev_dbg(dev,
			"Setup does not support variable Core Mask.\n");
		*max_core_mask = 0;
		return 0;
	}

	err = rg_ops->get_slice_mask(dev, &slice_mask);
	if (err)
		return err;

	err = rg_ops->get_slices_core_mask(dev, &slices_core_mask,
		&core_mask_stride);
	if (err)
		return err;

	*max_core_mask = 0;

	while (slice_mask) {
		core_mask = slices_core_mask & ((0x1ll << core_mask_stride)-1);
		slices_core_mask >>= core_mask_stride;

		if (slice_mask & 0x1) {
			core_mask <<= core_mask_stride * slice_count;
			block_core_mask |= core_mask;
			slice_count++;
		} else {
			*max_core_mask |= block_core_mask;
			block_core_mask = 0;
			slice_count = 0;
		}
		slice_mask >>= 1;
	}
	*max_core_mask |= block_core_mask;

	return 0;
}

/**
 * calculate_max_config() - Calculates max config data
 * @arb: Arbiter data
 *
 * Calculate and save max_l2_slices and max_core_mask values
 *
 * Return: 0 on success, error code otherwise
 */
static int calculate_max_config(struct mali_arb *arb)
{
	int err;

	if (!arb)
		return -EINVAL;

	err = get_max_l2_slices(arb->dev, &arb->max_config_info.l2_slices);
	if (err) {
		dev_err(arb->dev, "Cannot calculate max l2 slices\n");
		return err;
	}

	err = get_max_core_mask(arb->dev, &arb->max_config_info.core_mask);
	if (err) {
		dev_err(arb->dev, "Cannot calculate max core mask\n");
		return err;
	}

	arb->max_config_info.updated = true;
	return 0;
}

/**
 * get_max_config() - Notify Arbiter of a request for max config
 * @arb_vm: Opaque pointer to the Arbiter VM data populated by the arbiter
 *          during the registration.
 * @max_l2_slices: Returns the maximum number of GPU slices that can be assigned
 *                 to a partition. Caller must ensure not NULL.
 * @max_core_mask: Returns intersection of any possible partition core mask for
 *                 that resource group.
 *
 * Called by a Resource group module to Notify Arbiter that a request for
 * getting max config have been made, and that the arbiter needs to supply max
 * config data.
 *
 * Return: 0 is successful, or a standard Linux error code.
 */
static int get_max_config(struct mali_arb_priv_data *arb_vm,
	uint32_t *max_l2_slices, uint32_t *max_core_mask)
{
	int err;

	if (!arb_vm || !arb_vm->arb || !max_l2_slices || !max_core_mask)
		return -EINVAL;

	if (!arb_vm->arb->max_config_info.updated) {
		err = calculate_max_config(arb_vm->arb);
		if (err)
			return -EPERM;
	}
	*max_l2_slices = arb_vm->arb->max_config_info.l2_slices;
	*max_core_mask = arb_vm->arb->max_config_info.core_mask;

	return 0;
}

/**
 * build_partition() - Creates a partition instance
 * @arb: Arbiter data
 * @index: Partition index
 *
 * This function does not set the module fields.
 *
 * Return: Partition instance, or NULL if the memory could not be allocated
 *
 */
static struct mali_arb_partition *build_partition(struct mali_arb *arb,
	u32 index)
{
	struct mali_arb_partition *partition;

	partition = kzalloc(sizeof(struct mali_arb_partition), GFP_KERNEL);
	if (!partition) {
		dev_err(arb->dev,
			"P%u: Failed to allocate partition memory\n", index);
		return NULL;
	}

	partition->index = index;
	mutex_init(&partition->mutex);
	partition->arb = arb;
	hrtimer_init(&partition->tmr, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	partition->tmr.function = timer_isr;
	INIT_LIST_HEAD(&partition->req_list);
	partition->wq = alloc_workqueue("mali_arb_D%uP%u",
				WQ_UNBOUND, 0, arb->dev->id, index);
	INIT_WORK(&partition->timeout_work, timeout_worker);
	partition->last_dvfs_report = ktime_get();
	partition->last_state_transition = partition->last_dvfs_report;
	init_waitqueue_head(&partition->reassignment_wait);


	arb->partitions[index] = partition;

	return partition;
}

/**
 * destroy_partition() - Destroy a partition instance
 * @partition: Partition to be destroyed
 *
 * This function release all the resources associated to a partition
 *
 * Return: Partition instance, or NULL if the memory could not be allocated
 *
 */
void destroy_partition(struct mali_arb_partition *partition)
{
	if (!partition)
		return;

	if (partition->timer_running)
		hrtimer_cancel(&partition->tmr);

	if (partition->wq)
		destroy_workqueue(partition->wq);

	kfree(partition);
}

/**
 * update_arbiter_resources() - Updated the Arbiter internal data based on the
 *                              resources managed by the Arbiter.
 * @arb: Internal Arbiter data
 * @res: Resources interfaces provided by the backend
 *
 * This function takes the resource information and updates the Arbiter internal
 * data. It builds the partitions, initialize the slices' information and
 * creates sysfs entries for all resources.
 *
 * Return: 0 on success, or error code
 */
static int update_arbiter_resources(struct mali_arb *arb,
	struct resource_interfaces res)
{
	struct mali_arb_partition *partition;
	int err, i;
	struct vm_assign_ops *ops;

	if (res.vm_assign) {
		struct mali_ptm_rg_ops *rg_ops;
		u32 slice_mask, aw_mask;

		rg_ops = dev_get_drvdata(arb->dev);
		if (!rg_ops || !rg_ops->get_slice_mask ||
		    !rg_ops->get_aw_mask) {
			dev_err(arb->dev,
				"Cannot get Resource Group Operations data\n");
			return -ENODEV;
		}
		dev_info(arb->dev, "Arbiter running with Partition Manager\n");

		arb->rg_ops = rg_ops;

		/* Disable GPU-wide power control */
		arb->arb_gpu_power_on = NULL;
		arb->arb_gpu_power_off = NULL;

		/* Create the SysFS root */
		arb->sysfs = mali_arb_sysfs_create_root(arb,
				arb->dev,
				get_slice_assignment,
				set_slice_assignment,
				get_aw_assignment,
				set_aw_assignment);
		if (!arb->sysfs) {
			dev_err(arb->dev, "Cannot create SysFS root\n");
			err = -ENOMEM;
			goto fail;
		}

		/* Get the Resource Group masks so we can work out the global
		 * indices of the resources
		 */
		err = arb->rg_ops->get_slice_mask(arb->dev, &slice_mask);
		if (err) {
			dev_err(arb->dev, "Cannot get Resource Group masks\n");
			err = -EPERM;
			goto clean_root;
		}
		err = arb->rg_ops->get_aw_mask(arb->dev, &aw_mask);
		if (err) {
			dev_err(arb->dev, "Cannot get Resource Group masks\n");
			err = -EPERM;
			goto clean_root;
		}

		for (i = 0; i < MALI_PTM_SLICES_COUNT; ++i) {
			if (!(slice_mask & (1 << i)))
				continue;
			/* Create SysFs entry for the slice */
			if (mali_arb_sysfs_add_slice(arb->sysfs, i, arb->dev)) {
				dev_err(arb->dev,
					"Failed to create slice %u SysFS entry\n",
					i);
				err = -ENODEV;
				goto clean_slices;
			}

			arb->slices[i] = kzalloc(sizeof(struct slice),
				GFP_KERNEL);
			if (!arb->slices[i]) {
				err = -ENOMEM;
				goto clean_slices;
			}

			arb->slices[i]->arb = arb;
			arb->slices[i]->index = i;
			hrtimer_init(&arb->slices[i]->tmr,
				CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			arb->slices[i]->tmr.function = slice_timer_isr;
			arb->slices[i]->timer_running = false;
			INIT_WORK(&arb->slices[i]->slice_timeout_work,
				slice_timeout_worker);
		}

		arb->slices_wq = alloc_ordered_workqueue("mali_arb_slices",
			WQ_HIGHPRI);
		if (!arb->slices_wq) {
			dev_err(arb->dev, "Failed to create slices wq\n");
			goto clean_slices;
		}
		mutex_init(&arb->slices_mutex);

		/* Iterate over the resource information, and create partitions
		 * that are in the related Resource Group
		 */
		for (i = 0; i < res.num_if; i++) {
			uint32_t partition_id;

			partition_id = res.repartition[i]->if_id;

			if (!arb->partitions[i]) {
				partition = build_partition(arb, partition_id);
				if (!partition) {
					err = -ENOMEM;
					goto clean_partitions;
				}
			} else {
				partition = arb->partitions[i];
			}
			partition->hw_virtualization = true;

			/* Config module */
			partition->repart_if = *res.repartition[i];

			/* SysFS partition entry */
			err = mali_arb_sysfs_add_partition(arb->sysfs,
				partition_id,
				partition->repart_if.dev);
			if (err) {
				dev_err(arb->dev,
					"P%u: Cannot create SysFS entry\n",
					partition_id);
				goto clean_partitions;
			}

			/* Control module - It assumes that interface ID in
			 * repartition and vm_assign are matching.
			 */
			partition->vm_assign_if = *res.vm_assign[i];

			/* Interface invalid without assign_vm and
			 * unassign_vm.
			 */
			ops = partition->vm_assign_if.ops;
			if (unlikely(!ops->assign_vm || !ops->unassign_vm)) {
				pr_err("%s: Missing VM assign interface callbacks",
						__func__);
				err = -EINVAL;
				goto clean_partitions;
			}
		}
	} else {
		dev_info(arb->dev,
			"Arbiter running without Partition Manager\n");
		arb->sysfs = NULL;

		/* non-ptm mode implictly has one partition with one slice */
		partition = build_partition(arb, NO_PTM_IDX);
		if (!partition)
			return -ENOMEM;

		partition->hw_virtualization = false;
		partition->slices = 0x1;

		/* Connect the Xen/PV-specific callbacks */
		arb->arb_data.ops.arb_reg_assign_if = reg_assign_if;
		arb->arb_data.ops.arb_unreg_assign_if = unreg_assign_if;
	}

	INIT_LIST_HEAD(&arb->wait_list);
	INIT_LIST_HEAD(&arb->reg_vms_list);

	return 0;

clean_partitions:
	for (i = 0; i < res.num_if; i++) {
		partition = arb->partitions[i];
		if (partition) {
			destroy_partition(partition);
			arb->partitions[i] = NULL;
		}
	}

	if (arb->slices_wq)
		destroy_workqueue(arb->slices_wq);

clean_slices:
	for (i = 0; i < MALI_PTM_SLICES_COUNT; i++) {
		if (arb->slices[i]) {
			kfree(arb->slices[i]);
			arb->slices[i] = NULL;
		}
	}
	mali_arb_sysfs_free(arb->sysfs);
	arb->sysfs = NULL;
clean_root:
	if (arb->sysfs)
		mali_arb_sysfs_destroy_root(arb->sysfs);
fail:
	return err;
}


/**
 * init_arbiter() - Initializes the Arbiter internal data.
 * @dev: Device that contains the arbiter instance. This could either be a
 *       dedicated virtual device for the arbiter or contained in the
 *       platform-specific virtualization device, such as resource group or
 *       xenbus driver
 * @arb: Internal Arbiter data
 *
 * Initializes the arbiter internal data with all the common internal data to
 * any Arbiter configuration.
 *
 * Return: 0 on success, or error code
 */
static int init_arbiter(struct device *dev, struct mali_arb *arb)
{
	if (!arb || !dev)
		return -EINVAL;

	mutex_init(&arb->mutex);

	/* Callbacks common to Partition Manager and Xen/PV */
	arb->arb_data.ops.gpu_request = gpu_request;
	arb->arb_data.ops.gpu_active = gpu_active;
	arb->arb_data.ops.gpu_idle = gpu_idle;
	arb->arb_data.ops.gpu_stopped = gpu_stopped;
	arb->arb_data.ops.get_max = get_max_config;
	arb->arb_data.ops.register_vm = register_vm;
	arb->arb_data.ops.unregister_vm = unregister_vm;
	arb->dev = dev;

	return 0;
}

int arbiter_create(struct device *dev,
			struct power_interface pwr,
			struct resource_interfaces res,
			struct mali_arb_data **arb_data)
{
	struct mali_arb *arb;
	int err;

	if (!dev || !arb_data)
		return -EINVAL;

	arb = devm_kzalloc(dev, sizeof(struct mali_arb), GFP_KERNEL);
	if (!arb)
		return -ENOMEM;

	err = init_arbiter(dev, arb);
	if (err)
		goto cleanup;

	err = update_arbiter_resources(arb, res);
	if (err)
		goto cleanup;

	if (pwr.dev) {
		arb->pwr_if = pwr;
		err = arbiter_init_pwr(arb);
		if (err) {
			dev_err(arb->dev,
				"Failed to initialize GPU Power interfaces.\n");
			goto cleanup;
		}
	}

	/*
	 * After initalizing the arbiter it is necessary to send the
	 * max config information to all VMs assigned to this resource group.
	 * Sending it automatically in the beggining helps to avoid round trip
	 * during the kbase initialization.
	 */
	arb->max_config_info.updated = false;
	err = calculate_max_config(arb);
	if (err)
		dev_dbg(arb->dev, "Failed to calculate max config.\n");


	*arb_data = &arb->arb_data;

	dev_info(arb->dev, "Arbiter Probed\n");
	return 0;

cleanup:
	devm_kfree(dev, arb);
	return err;
}
EXPORT_SYMBOL(arbiter_create);

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
	struct mali_arb *arb;
	struct resource_interfaces no_res = {
		.num_if = 0,
		.vm_assign = NULL,
		.repartition = NULL
	};
	int err;

	arb = devm_kzalloc(&pdev->dev, sizeof(struct mali_arb), GFP_KERNEL);
	if (!arb)
		return -ENOMEM;

	err = init_arbiter(&pdev->dev, arb);
	if (err)
		goto cleanup;

	err = update_arbiter_resources(arb, no_res);
	if (err)
		goto cleanup;

	/* Try to get get the platform specific arbiter integration device
	 * if specified in the DT, otherwise move forward with the probe
	 */
	err = arbiter_get_pwr(arb);
	if (err) {
		dev_err(&pdev->dev, "Failed to fetch GPU Power device\n");
		goto cleanup;
	}

	if (arb->pwr_if.dev) {
		err = arbiter_init_pwr(arb);
		if (err) {
			dev_err(arb->dev,
				"Failed to initialize GPU Power device\n");
			goto cleanup_module;
		}
	}

	if (no_timeslice_yield_timeout < yield_timeout)
		dev_warn(arb->dev,
		"no_timeslicing_yield_timeout is shorter than yield_timeout");

	platform_set_drvdata(pdev, &arb->arb_data);
	dev_info(arb->dev, "Arbiter Probed\n");
	return 0;
cleanup_module:
	module_put(arb->pwr_module);
	arb->pwr_module = NULL;
cleanup:
	devm_kfree(&pdev->dev, arb);
	return err;
}

/**
 * term_arbiter() - Terminates the arbiter device and release its resources
 * @arb_data: Public arbiter data exposed to the backend module.
 * @dev:      Device that contains the arbiter instance. This could either be a
 *            dedicated virtual device for the arbiter or contained in the
 *            platform-specific virtualization device, such as resource group or
 *            xenbus driver
 *
 * This function is called when the device is removed to free up all the
 * resources.
 */
void term_arbiter(struct mali_arb_data *arb_data, struct device *dev)
{
	struct mali_arb_partition *partition;
	struct mali_arb *arb;
	uint32_t i;

	if (WARN_ON(!arb_data || !dev))
		return;

	arb = arb_from_data(arb_data);
	/* sysfs are published only in PTM mode */
	if (arb->sysfs)
		mali_arb_sysfs_free(arb->sysfs);

	if (arb->pwr_if.dev)
		arb->pwr_if.ops->unregister_arb(arb->pwr_if.dev, arb->arb_id);
	if (arb->pwr_module)
		module_put(arb->pwr_module);

	for (i = 0; i < ARRAY_SIZE(arb->partitions); ++i) {
		partition = arb->partitions[i];
		if (partition) {
			mutex_lock(&partition->mutex);
			destroy_workqueue(partition->wq);
			mutex_unlock(&partition->mutex);

			kfree(partition);
		}
	}

	for (i = 0; i < MALI_PTM_SLICES_COUNT; i++) {
		if (arb->slices[i]) {
			if (arb->slices[i]->timer_running)
				hrtimer_cancel(&arb->slices[i]->tmr);
			kfree(arb->slices[i]);
		}
	}
	if (arb->slices_wq)
		destroy_workqueue(arb->slices_wq);
	devm_kfree(dev, arb);
}

void arbiter_destroy(struct mali_arb_data *arb_data, struct device *dev)
{
	if (WARN_ON(!arb_data || !dev))
		return;

	term_arbiter(arb_data, dev);
}
EXPORT_SYMBOL(arbiter_destroy);

/**
 * arbiter_remove() - Called when device is removed
 * @pdev: Platform device
 */
static int arbiter_remove(struct platform_device *pdev)
{
	struct mali_arb_data *arb_data;

	arb_data = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	if (arb_data)
		term_arbiter(arb_data, &pdev->dev);

	return 0;
}

static const struct of_device_id arbiter_dt_match[] = {
	{ .compatible = MALI_ARBITER_DT_NAME },
	{}
};

static struct platform_driver arbiter_driver = {
	.probe = arbiter_probe,
	.remove = arbiter_remove,
	.driver = {
		.name = "mali_arbiter",
		.of_match_table = arbiter_dt_match,
	},
};

/**
 * arbiter_init() - Register platform driver
 */
static int __init arbiter_init(void)
{
	return platform_driver_register(&arbiter_driver);
}
module_init(arbiter_init);

/**
 * arbiter_exit() - Unregister platform driver
 */
static void __exit arbiter_exit(void)
{
	platform_driver_unregister(&arbiter_driver);
}
module_exit(arbiter_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("mali-arbiter");

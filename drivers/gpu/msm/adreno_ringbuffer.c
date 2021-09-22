// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/interconnect.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <soc/qcom/dcvs.h>

#include "a3xx_reg.h"
#include "a5xx_reg.h"
#include "a6xx_reg.h"
#include "adreno.h"
#include "adreno_pm4types.h"
#include "adreno_ringbuffer.h"
#include "adreno_trace.h"
#include "kgsl_trace.h"


#define RB_HOSTPTR(_rb, _pos) \
	((unsigned int *) ((_rb)->buffer_desc->hostptr + \
		((_pos) * sizeof(unsigned int))))

#define RB_GPUADDR(_rb, _pos) \
	((_rb)->buffer_desc->gpuaddr + ((_pos) * sizeof(unsigned int)))

void adreno_get_submit_time(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	unsigned long flags;
	struct adreno_context *drawctxt = rb->drawctxt_active;
	struct kgsl_context *context = &drawctxt->base;

	if (!time)
		return;

	/*
	 * Here we are attempting to create a mapping between the
	 * GPU time domain (alwayson counter) and the CPU time domain
	 * (local_clock) by sampling both values as close together as
	 * possible. This is useful for many types of debugging and
	 * profiling. In order to make this mapping as accurate as
	 * possible, we must turn off interrupts to avoid running
	 * interrupt handlers between the two samples.
	 */

	local_irq_save(flags);

	time->ticks = gpudev->read_alwayson(adreno_dev);

	/* Trace the GPU time to create a mapping to ftrace time */
	trace_adreno_cmdbatch_sync(context->id, context->priority,
		drawctxt->timestamp, time->ticks);

	/* Get the kernel clock for time since boot */
	time->ktime = local_clock();

	/* Get the timeofday for the wall time (for the user) */
	ktime_get_real_ts64(&time->utime);

	local_irq_restore(flags);
}

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
		unsigned int dwords)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	unsigned int rptr = adreno_get_rptr(rb);
	unsigned int ret;

	if (rptr <= rb->_wptr) {
		unsigned int *cmds;

		if (rb->_wptr + dwords <= (KGSL_RB_DWORDS - 2)) {
			ret = rb->_wptr;
			rb->_wptr = (rb->_wptr + dwords) % KGSL_RB_DWORDS;
			return RB_HOSTPTR(rb, ret);
		}

		/*
		 * There isn't enough space toward the end of ringbuffer. So
		 * look for space from the beginning of ringbuffer upto the
		 * read pointer.
		 */
		if (dwords < rptr) {
			cmds = RB_HOSTPTR(rb, rb->_wptr);
			*cmds = cp_packet(adreno_dev, CP_NOP,
				KGSL_RB_DWORDS - rb->_wptr - 1);
			rb->_wptr = dwords;
			return RB_HOSTPTR(rb, 0);
		}
	}

	if (rb->_wptr + dwords < rptr) {
		ret = rb->_wptr;
		rb->_wptr = (rb->_wptr + dwords) % KGSL_RB_DWORDS;
		return RB_HOSTPTR(rb, ret);
	}

	return ERR_PTR(-ENOSPC);
}

void adreno_ringbuffer_stop(struct adreno_device *adreno_dev)
{
	struct adreno_ringbuffer *rb;
	int i;

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i)
		kgsl_cancel_events(KGSL_DEVICE(adreno_dev), &(rb->events));
}

static int _rb_readtimestamp(struct kgsl_device *device,
		void *priv, enum kgsl_timestamp_type type,
		unsigned int *timestamp)
{
	return adreno_rb_readtimestamp(ADRENO_DEVICE(device), priv, type,
		timestamp);
}

int adreno_ringbuffer_setup(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, int id)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int priv = 0;
	int ret;

	/* allocate a chunk of memory to create user profiling IB1s */
	adreno_allocate_global(device, &rb->profile_desc, PAGE_SIZE,
		0, KGSL_MEMFLAGS_GPUREADONLY, 0, "profile_desc");

	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		priv |= KGSL_MEMDESC_PRIVILEGED;

	ret = adreno_allocate_global(device, &rb->buffer_desc, KGSL_RB_SIZE,
		SZ_4K, KGSL_MEMFLAGS_GPUREADONLY, priv, "ringbuffer");
	if (ret)
		return ret;

	if (!list_empty(&rb->events.group))
		return 0;

	rb->id = id;
	kgsl_add_event_group(device, &rb->events, NULL, _rb_readtimestamp, rb,
		"rb_events-%d", id);

	rb->timestamp = 0;
	init_waitqueue_head(&rb->ts_expire_waitq);

	spin_lock_init(&rb->preempt_lock);

	return 0;
}

void adreno_preemption_timer(struct timer_list *t)
{
	struct adreno_preemption *preempt = from_timer(preempt, t, timer);
	struct adreno_device *adreno_dev = container_of(preempt,
						struct adreno_device, preempt);

	/* We should only be here from a triggered state */
	if (!adreno_move_preempt_state(adreno_dev,
		ADRENO_PREEMPT_TRIGGERED, ADRENO_PREEMPT_FAULTED))
		return;

	/* Schedule the worker to take care of the details */
	queue_work(system_unbound_wq, &adreno_dev->preempt.work);
}

void adreno_drawobj_set_constraint(struct kgsl_device *device,
			struct kgsl_drawobj *drawobj)
{
	struct kgsl_context *context = drawobj->context;
	unsigned long flags = drawobj->flags;

	/*
	 * Check if the context has a constraint and constraint flags are
	 * set.
	 */
	if (context->pwr_constraint.type &&
		((context->flags & KGSL_CONTEXT_PWR_CONSTRAINT) ||
			(drawobj->flags & KGSL_CONTEXT_PWR_CONSTRAINT)))
		kgsl_pwrctrl_set_constraint(device, &context->pwr_constraint,
					context->id, drawobj->timestamp);

	if (context->l3_pwr_constraint.type &&
		((context->flags & KGSL_CONTEXT_PWR_CONSTRAINT) ||
			(flags & KGSL_CONTEXT_PWR_CONSTRAINT))) {

		if (!device->num_l3_pwrlevels) {
			dev_err_once(device->dev,
				"l3 voting not available\n");
			return;
		}

		switch (context->l3_pwr_constraint.type) {
		case KGSL_CONSTRAINT_L3_PWRLEVEL: {
			unsigned int sub_type;
			unsigned int new_l3;
			int ret = 0;
			struct dcvs_freq freq = {0};

			if (!device->l3_vote)
				return;

			sub_type = context->l3_pwr_constraint.sub_type;

			/*
			 * If an L3 constraint is already set, set the new
			 * one only if it is higher.
			 */
			new_l3 = max_t(unsigned int, sub_type + 1,
					device->cur_l3_pwrlevel);
			new_l3 = min_t(unsigned int, new_l3,
					device->num_l3_pwrlevels - 1);

			if (device->cur_l3_pwrlevel == new_l3)
				return;

			freq.ib = device->l3_freq[new_l3];
			freq.hw_type = DCVS_L3;
			ret = qcom_dcvs_update_votes(KGSL_L3_DEVICE, &freq, 1,
				DCVS_SLOW_PATH);
			if (!ret) {
				trace_kgsl_constraint(device,
					KGSL_CONSTRAINT_L3_PWRLEVEL, new_l3, 1);
				device->cur_l3_pwrlevel = new_l3;
			} else {
				dev_err_ratelimited(device->dev,
						       "Could not set l3_vote: %d\n",
						       ret);
			}
			break;
			}
		}
	}
}

int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj,
		struct adreno_submit_time *time)
{
	struct adreno_submit_time local = { 0 };
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	struct adreno_ringbuffer *rb = drawctxt->rb;
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	u32 flags = 0;
	int ret;

	/*
	 * If SKIP CMD flag is set for current context
	 * a) set SKIPCMD as fault_recovery for current commandbatch
	 * b) store context's commandbatch fault_policy in current
	 *    commandbatch fault_policy and clear context's commandbatch
	 *    fault_policy
	 * c) force preamble for commandbatch
	 */
	if (test_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->base.priv) &&
		(!test_bit(CMDOBJ_SKIP, &cmdobj->priv))) {

		set_bit(KGSL_FT_SKIPCMD, &cmdobj->fault_recovery);
		cmdobj->fault_policy = drawctxt->fault_policy;
		set_bit(CMDOBJ_FORCE_PREAMBLE, &cmdobj->priv);

		/* if context is detached print fault recovery */
		adreno_fault_skipcmd_detached(adreno_dev, drawctxt, drawobj);

		/* clear the drawctxt flags */
		clear_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->base.priv);
		drawctxt->fault_policy = 0;
	}

	/* Check if user profiling should be enabled */

	if ((drawobj->flags & KGSL_DRAWOBJ_PROFILING) &&
		cmdobj->profiling_buf_entry) {
		flags |= F_USER_PROFILE;

		/*
		 * we want to use an adreno_submit_time struct to get the
		 * precise moment when the command is submitted to the
		 * ringbuffer.  If an upstream caller already passed down a
		 * pointer piggyback on that otherwise use a local struct
		 */
		if (!time)
			time = &local;

		time->drawobj = drawobj;
	}

	flags |= F_PREAMBLE;

	/*
	 * When preamble is enabled, the preamble buffer with state restoration
	 * commands are stored in the first node of the IB chain.
	 * We can skip that if a context switch hasn't occurred.
	 */
	if ((drawctxt->base.flags & KGSL_CONTEXT_PREAMBLE) &&
		!test_bit(CMDOBJ_FORCE_PREAMBLE, &cmdobj->priv) &&
		(rb->drawctxt_active == drawctxt))
		flags &= ~F_PREAMBLE;

	/*
	 * In skip mode don't issue the draw IBs but keep all the other
	 * accoutrements of a submision (including the interrupt) to keep
	 * the accounting sane. Set start_index and numibs to 0 to just
	 * generate the start and end markers and skip everything else
	 */
	if (test_bit(CMDOBJ_SKIP, &cmdobj->priv)) {
		flags &= ~F_PREAMBLE;
		flags |= F_SKIP;
	}

	/* Enable kernel profiling */
	if (test_bit(CMDOBJ_PROFILE, &cmdobj->priv))
		flags |= F_KERNEL_PROFILE;

	/* Add a WFI to the end of the submission */
	if (test_bit(CMDOBJ_WFI, &cmdobj->priv))
		flags |= F_WFI;

	/*
	 * For some targets, we need to execute a dummy shader operation after a
	 * power collapse
	 */
	if (test_and_clear_bit(ADRENO_DEVICE_PWRON, &adreno_dev->priv) &&
		test_bit(ADRENO_DEVICE_PWRON_FIXUP, &adreno_dev->priv))
		flags |= F_PWRON_FIXUP;

	/* Check to see the submission should be secure */
	if (drawobj->context->flags & KGSL_CONTEXT_SECURE)
		flags |= F_SECURE;

	/* process any profiling results that are available into the log_buf */
	adreno_profile_process_results(adreno_dev);

	ret = gpudev->ringbuffer_submitcmd(adreno_dev, cmdobj,
		flags, time);

	if (!ret) {
		set_bit(KGSL_CONTEXT_PRIV_SUBMITTED, &drawobj->context->priv);
		cmdobj->global_ts = drawctxt->internal_timestamp;
	}

	return ret;
}

/**
 * adreno_ringbuffer_wait_callback() - Callback function for event registered
 * on a ringbuffer timestamp
 * @device: Device for which the the callback is valid
 * @context: The context of the event
 * @priv: The private parameter of the event
 * @result: Result of the event trigger
 */
static void adreno_ringbuffer_wait_callback(struct kgsl_device *device,
		struct kgsl_event_group *group,
		void *priv, int result)
{
	struct adreno_ringbuffer *rb = group->priv;

	wake_up_all(&rb->ts_expire_waitq);
}

/* check if timestamp is greater than the current rb timestamp */
static inline int adreno_ringbuffer_check_timestamp(
			struct adreno_ringbuffer *rb,
			unsigned int timestamp, int type)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	unsigned int ts;

	adreno_rb_readtimestamp(adreno_dev, rb, type, &ts);
	return (timestamp_cmp(ts, timestamp) >= 0);
}


/**
 * adreno_ringbuffer_waittimestamp() - Wait for a RB timestamp
 * @rb: The ringbuffer to wait on
 * @timestamp: The timestamp to wait for
 * @msecs: The wait timeout period
 */
int adreno_ringbuffer_waittimestamp(struct adreno_ringbuffer *rb,
					unsigned int timestamp,
					unsigned int msecs)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;
	unsigned long wait_time;

	/* check immediately if timeout is 0 */
	if (msecs == 0)
		return adreno_ringbuffer_check_timestamp(rb,
			timestamp, KGSL_TIMESTAMP_RETIRED) ? 0 : -EBUSY;

	ret = kgsl_add_event(device, &rb->events, timestamp,
		adreno_ringbuffer_wait_callback, NULL);
	if (ret)
		return ret;

	mutex_unlock(&device->mutex);

	wait_time = msecs_to_jiffies(msecs);
	if (wait_event_timeout(rb->ts_expire_waitq,
		!kgsl_event_pending(device, &rb->events, timestamp,
				adreno_ringbuffer_wait_callback, NULL),
		wait_time) == 0)
		ret  = -ETIMEDOUT;

	mutex_lock(&device->mutex);
	/*
	 * after wake up make sure that expected timestamp has retired
	 * because the wakeup could have happened due to a cancel event
	 */
	if (!ret && !adreno_ringbuffer_check_timestamp(rb,
		timestamp, KGSL_TIMESTAMP_RETIRED)) {
		ret = -EAGAIN;
	}

	return ret;
}

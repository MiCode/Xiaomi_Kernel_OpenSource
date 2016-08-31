#undef DEBUG

/*
 * ARM performance counter support.
 *
 * Copyright (C) 2009 picoChip Designs, Ltd., Jamie Iles
 * Copyright (C) 2010 ARM Ltd., Will Deacon <will.deacon@arm.com>
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This code is based on the sparc64 perf event code, which is in turn based
 * on the x86 code. Callchain code is based on the ARM OProfile backtrace
 * code.
 */
#define pr_fmt(fmt) "hw perfevents: " fmt

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>

#include <asm/irq_regs.h>
#include <asm/pmu.h>
#include <asm/stacktrace.h>

static int
armpmu_map_cache_event(const unsigned (*cache_map)
				      [PERF_COUNT_HW_CACHE_MAX]
				      [PERF_COUNT_HW_CACHE_OP_MAX]
				      [PERF_COUNT_HW_CACHE_RESULT_MAX],
		       u64 config)
{
	unsigned int cache_type, cache_op, cache_result, ret;

	cache_type = (config >>  0) & 0xff;
	if (cache_type >= PERF_COUNT_HW_CACHE_MAX)
		return -EINVAL;

	cache_op = (config >>  8) & 0xff;
	if (cache_op >= PERF_COUNT_HW_CACHE_OP_MAX)
		return -EINVAL;

	cache_result = (config >> 16) & 0xff;
	if (cache_result >= PERF_COUNT_HW_CACHE_RESULT_MAX)
		return -EINVAL;

	ret = (int)(*cache_map)[cache_type][cache_op][cache_result];

	if (ret == CACHE_OP_UNSUPPORTED)
		return -ENOENT;

	return ret;
}

static int
armpmu_map_hw_event(const unsigned (*event_map)[PERF_COUNT_HW_MAX], u64 config)
{
	int mapping;

	if (config >= PERF_COUNT_HW_MAX)
		return -ENOENT;

	mapping = (*event_map)[config];
	return mapping == HW_OP_UNSUPPORTED ? -ENOENT : mapping;
}

static int
armpmu_map_raw_event(u32 raw_event_mask, u64 config)
{
	return (int)(config & raw_event_mask);
}

int
armpmu_map_event(struct perf_event *event,
		 const unsigned (*event_map)[PERF_COUNT_HW_MAX],
		 const unsigned (*cache_map)
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX],
		 u32 raw_event_mask)
{
	u64 config = event->attr.config;

	switch (event->attr.type) {
	case PERF_TYPE_HARDWARE:
		return armpmu_map_hw_event(event_map, config);
	case PERF_TYPE_HW_CACHE:
		return armpmu_map_cache_event(cache_map, config);
	case PERF_TYPE_RAW:
		return armpmu_map_raw_event(raw_event_mask, config);
	}

	return -ENOENT;
}

int armpmu_event_set_period(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	s64 left = local64_read(&hwc->period_left);
	s64 period = hwc->sample_period;
	int ret = 0;

	/* The period may have been changed by PERF_EVENT_IOC_PERIOD */
	if (unlikely(period != hwc->last_period))
		left = period - (hwc->last_period - left);

	if (unlikely(left <= -period)) {
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (unlikely(left <= 0)) {
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (left > (s64)armpmu->max_period)
		left = armpmu->max_period;

	local64_set(&hwc->prev_count, (u64)-left);

	armpmu->write_counter(event, (u64)(-left) & 0xffffffff);

	perf_event_update_userpage(event);

	return ret;
}

u64 armpmu_event_update(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	u64 delta, prev_raw_count, new_raw_count;

again:
	prev_raw_count = local64_read(&hwc->prev_count);
	new_raw_count = armpmu->read_counter(event);

	if (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
			     new_raw_count) != prev_raw_count)
		goto again;

	delta = (new_raw_count - prev_raw_count) & armpmu->max_period;

	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);

	return new_raw_count;
}

static void
armpmu_read(struct perf_event *event)
{
	armpmu_event_update(event);
}

static void
armpmu_stop(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	/*
	 * ARM pmu always has to update the counter, so ignore
	 * PERF_EF_UPDATE, see comments in armpmu_start().
	 */
	if (!(hwc->state & PERF_HES_STOPPED)) {
		armpmu->disable(event);
		armpmu_event_update(event);
		hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
	}
}

static void armpmu_start(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	/*
	 * ARM pmu always has to reprogram the period, so ignore
	 * PERF_EF_RELOAD, see the comment below.
	 */
	if (flags & PERF_EF_RELOAD)
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));

	hwc->state = 0;
	/*
	 * Set the period again. Some counters can't be stopped, so when we
	 * were stopped we simply disabled the IRQ source and the counter
	 * may have been left counting. If we don't do this step then we may
	 * get an interrupt too soon or *way* too late if the overflow has
	 * happened since disabling.
	 */
	armpmu_event_set_period(event);
	armpmu->enable(event);
}

static void
armpmu_del(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *hw_events = armpmu->get_hw_events();
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	armpmu_stop(event, PERF_EF_UPDATE);
	hw_events->events[idx] = NULL;
	clear_bit(idx, hw_events->used_mask);

	perf_event_update_userpage(event);
}

static int
armpmu_add(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *hw_events = armpmu->get_hw_events();
	struct hw_perf_event *hwc = &event->hw;
	int idx;
	int err = 0;

	perf_pmu_disable(event->pmu);

	/* If we don't have a space for the counter then finish early. */
	idx = armpmu->get_event_idx(hw_events, event);
	if (idx < 0) {
		err = idx;
		goto out;
	}

	/*
	 * If there is an event in the counter we are going to use then make
	 * sure it is disabled.
	 */
	event->hw.idx = idx;
	armpmu->disable(event);
	hw_events->events[idx] = event;

	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	if (flags & PERF_EF_START)
		armpmu_start(event, PERF_EF_RELOAD);

	/* Propagate our changes to the userspace mapping. */
	perf_event_update_userpage(event);

out:
	perf_pmu_enable(event->pmu);
	return err;
}

static int
validate_event(struct pmu_hw_events *hw_events,
	       struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct pmu *leader_pmu = event->group_leader->pmu;

	if (is_software_event(event))
		return 1;

	if (event->pmu != leader_pmu || event->state < PERF_EVENT_STATE_OFF)
		return 1;

	if (event->state == PERF_EVENT_STATE_OFF && !event->attr.enable_on_exec)
		return 1;

	return armpmu->get_event_idx(hw_events, event) >= 0;
}

static int
validate_group(struct perf_event *event)
{
	struct perf_event *sibling, *leader = event->group_leader;
	struct pmu_hw_events fake_pmu;
	DECLARE_BITMAP(fake_used_mask, ARMPMU_MAX_HWEVENTS);

	/*
	 * Initialise the fake PMU. We only need to populate the
	 * used_mask for the purposes of validation.
	 */
	memset(fake_used_mask, 0, sizeof(fake_used_mask));
	fake_pmu.used_mask = fake_used_mask;

	if (!validate_event(&fake_pmu, leader))
		return -EINVAL;

	list_for_each_entry(sibling, &leader->sibling_list, group_entry) {
		if (!validate_event(&fake_pmu, sibling))
			return -EINVAL;
	}

	if (!validate_event(&fake_pmu, event))
		return -EINVAL;

	return 0;
}

static irqreturn_t armpmu_dispatch_irq(int irq, void *dev)
{
	struct arm_pmu *armpmu = (struct arm_pmu *) dev;
	struct platform_device *plat_device = armpmu->plat_device;
	struct arm_pmu_platdata *plat = dev_get_platdata(&plat_device->dev);

	if (plat && plat->handle_irq)
		return plat->handle_irq(irq, dev, armpmu->handle_irq);
	else
		return armpmu->handle_irq(irq, dev);
}

static void
armpmu_release_hardware(struct arm_pmu *armpmu)
{
	armpmu->free_irq(armpmu);
	pm_runtime_put_sync(&armpmu->plat_device->dev);
}

static int
armpmu_reserve_hardware(struct arm_pmu *armpmu)
{
	int err;
	struct platform_device *pmu_device = armpmu->plat_device;

	if (!pmu_device)
		return -ENODEV;

	pm_runtime_get_sync(&pmu_device->dev);
	err = armpmu->request_irq(armpmu, armpmu_dispatch_irq);
	if (err) {
		armpmu_release_hardware(armpmu);
		return err;
	}

	return 0;
}

static void
hw_perf_event_destroy(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	atomic_t *active_events	 = &armpmu->active_events;
	struct mutex *pmu_reserve_mutex = &armpmu->reserve_mutex;

	if (atomic_dec_and_mutex_lock(active_events, pmu_reserve_mutex)) {
		armpmu_release_hardware(armpmu);
		mutex_unlock(pmu_reserve_mutex);
	}
}

static int
event_requires_mode_exclusion(struct perf_event_attr *attr)
{
	return attr->exclude_idle || attr->exclude_user ||
	       attr->exclude_kernel || attr->exclude_hv;
}

static int
__hw_perf_event_init(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int mapping;

	mapping = armpmu->map_event(event);

	if (mapping < 0) {
		pr_debug("event %x:%llx not supported\n", event->attr.type,
			 event->attr.config);
		return mapping;
	}

	/*
	 * We don't assign an index until we actually place the event onto
	 * hardware. Use -1 to signify that we haven't decided where to put it
	 * yet. For SMP systems, each core has it's own PMU so we can't do any
	 * clever allocation or constraints checking at this point.
	 */
	hwc->idx		= -1;
	hwc->config_base	= 0;
	hwc->config		= 0;
	hwc->event_base		= 0;

	/*
	 * Check whether we need to exclude the counter from certain modes.
	 */
	if ((!armpmu->set_event_filter ||
	     armpmu->set_event_filter(hwc, &event->attr)) &&
	     event_requires_mode_exclusion(&event->attr)) {
		pr_debug("ARM performance counters do not support "
			 "mode exclusion\n");
		return -EOPNOTSUPP;
	}

	/*
	 * Store the event encoding into the config_base field.
	 */
	hwc->config_base	    |= (unsigned long)mapping;

	if (!hwc->sample_period) {
		/*
		 * For non-sampling runs, limit the sample_period to half
		 * of the counter width. That way, the new counter value
		 * is far less likely to overtake the previous one unless
		 * you have some serious IRQ latency issues.
		 */
		hwc->sample_period  = armpmu->max_period >> 1;
		hwc->last_period    = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	if (event->group_leader != event) {
		if (validate_group(event) != 0)
			return -EINVAL;
	}

	return 0;
}

static int armpmu_event_init(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	int err = 0;
	atomic_t *active_events = &armpmu->active_events;

	/* does not support taken branch sampling */
	if (has_branch_stack(event))
		return -EOPNOTSUPP;

	if (armpmu->map_event(event) == -ENOENT)
		return -ENOENT;

	event->destroy = hw_perf_event_destroy;

	if (!atomic_inc_not_zero(active_events)) {
		mutex_lock(&armpmu->reserve_mutex);
		if (atomic_read(active_events) == 0)
			err = armpmu_reserve_hardware(armpmu);

		if (!err)
			atomic_inc(active_events);
		mutex_unlock(&armpmu->reserve_mutex);
	}

	if (err)
		return err;

	err = __hw_perf_event_init(event);
	if (err)
		hw_perf_event_destroy(event);

	return err;
}

static void armpmu_enable(struct pmu *pmu)
{
	struct arm_pmu *armpmu = to_arm_pmu(pmu);
	struct pmu_hw_events *hw_events = armpmu->get_hw_events();
	int enabled = bitmap_weight(hw_events->used_mask, armpmu->num_events);

	if (enabled)
		armpmu->start(armpmu);
}

static void armpmu_disable(struct pmu *pmu)
{
	struct arm_pmu *armpmu = to_arm_pmu(pmu);
	armpmu->stop(armpmu);
}

#ifdef CONFIG_PM_RUNTIME
static int armpmu_runtime_resume(struct device *dev)
{
	struct arm_pmu_platdata *plat = dev_get_platdata(dev);

	if (plat && plat->runtime_resume)
		return plat->runtime_resume(dev);

	return 0;
}

static int armpmu_runtime_suspend(struct device *dev)
{
	struct arm_pmu_platdata *plat = dev_get_platdata(dev);

	if (plat && plat->runtime_suspend)
		return plat->runtime_suspend(dev);

	return 0;
}
#endif

const struct dev_pm_ops armpmu_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(armpmu_runtime_suspend, armpmu_runtime_resume, NULL)
};

static void armpmu_init(struct arm_pmu *armpmu)
{
	atomic_set(&armpmu->active_events, 0);
	mutex_init(&armpmu->reserve_mutex);

	armpmu->pmu = (struct pmu) {
		.pmu_enable	= armpmu_enable,
		.pmu_disable	= armpmu_disable,
		.event_init	= armpmu_event_init,
		.add		= armpmu_add,
		.del		= armpmu_del,
		.start		= armpmu_start,
		.stop		= armpmu_stop,
		.read		= armpmu_read,
	};
}

int armpmu_register(struct arm_pmu *armpmu, int type)
{
	armpmu_init(armpmu);
	pm_runtime_enable(&armpmu->plat_device->dev);
	pr_info("enabled with %s PMU driver, %d counters available\n",
			armpmu->name, armpmu->num_events);
	return perf_pmu_register(&armpmu->pmu, armpmu->name, type);
}

/*
 * Callchain handling code.
 */

/*
 * The registers we're interested in are at the end of the variable
 * length saved register structure. The fp points at the end of this
 * structure so the address of this struct is:
 * (struct frame_tail *)(xxx->fp)-1
 *
 * This code has been adapted from the ARM OProfile support.
 */
#ifndef CONFIG_PERF_ANDROID_BACKTRACE
struct frame_tail {
	struct frame_tail __user *fp;
	unsigned long sp;
	unsigned long lr;
} __packed;
#else
struct frame_tail {
	unsigned long fp;
	unsigned long lr;
} __packed;
#endif

/*
 * Get the return address for a single stackframe and return a pointer to the
 * next frame tail.
 */
static struct frame_tail __user *
user_backtrace(struct frame_tail __user *tail,
	       struct perf_callchain_entry *entry)
{
	struct frame_tail buftail;

	/* Also check accessibility of one struct frame_tail beyond */
	if (!access_ok(VERIFY_READ, tail, sizeof(buftail)))
		return NULL;
	if (__copy_from_user_inatomic(&buftail, tail, sizeof(buftail)))
		return NULL;

	perf_callchain_store(entry, buftail.lr);

#ifndef CONFIG_PERF_ANDROID_BACKTRACE
	/*
	 * Frame pointers should strictly progress back up the stack
	 * (towards higher addresses).
	 */
	if (tail + 1 >= buftail.fp)
		return NULL;

	return buftail.fp - 1;
#else
	/*
	 * Frame pointers should strictly progress back up the stack
	 * (towards higher addresses).
	 */
	if ((unsigned long *)(tail + 1) >= (unsigned long *)buftail.fp)
		return NULL;

	return  (struct frame_tail *)(buftail.fp - sizeof(unsigned long));
#endif
}


#ifdef CONFIG_PERF_ANDROID_BACKTRACE

#define AB_USER_SPACE_MIN_ADDR	0x8000
#define PERF_CALLCHAIN_CHECK_PROLOG_EPILOG 1

#ifdef PERF_CALLCHAIN_CHECK_PROLOG_EPILOG

enum regs {
#ifdef CONFIG_THUMB2_KERNEL
	FP = 7,
#else
	FP = 11,
#endif
	SP = 13,
	LR = 14,
	PC = 15
};

#define INSTR_MASK_STR_FP	0xffff0000
#define INSTR_STR_FP		0xe52d0000

#define INSTR_MASK_STMDB	0xffff0000
#define INSTR_STMDB_SP		0xe92d0000

#define INSTR_MASK_LDMIA	0x0fff0000
#define INSTR_LDMIA_SP		0x08bd0000

#define INSTR_MASK_ADD_RN_RD	0xfffff000
#define INSTR_ADD_FP_SP		0xe28db000

#define INSTR_MASK_BRANCH	0x0f000000
#define INSTR_BRANCH		0x0a000000

#define INSTR_MASK_BRANCH_L	0x0f000000
#define INSTR_BRANCH_L		0x0b000000

#define INSTR_MASK_BRANCH_X	0x0ffffff0
#define INSTR_BRANCH_X		0x012fff10

#define INSTR_MASK_BRANCH_LX	0x0ffffff0
#define INSTR_BRANCH_LX		0x012fff30

#define check_in_register_list(val, reg) \
	(((val) & 0xffff) & (1 << (reg)))

static inline int is_instr_push_with_fp(unsigned long instr)
{
	if ((instr & INSTR_MASK_STMDB) == INSTR_STMDB_SP) {
		if (check_in_register_list(instr, SP) ||
			check_in_register_list(instr, PC)) {
			return -1;
		}
		if (check_in_register_list(instr, FP))
			return 1;
	}

	if ((instr & INSTR_MASK_STR_FP) == INSTR_STR_FP)
		return 1;

	return 0;
}

static inline int is_instr_push_with_fp_lr(unsigned long instr)
{
	if ((instr & INSTR_MASK_STMDB) == INSTR_STMDB_SP) {
		if (check_in_register_list(instr, SP) ||
			check_in_register_list(instr, PC)) {
			return -1;
		}
		if (check_in_register_list(instr, FP) &&
			check_in_register_list(instr, LR)) {
			return 1;
		}
	}
	return 0;
}

#define is_instr_add_fp(instr) \
	((((instr) & INSTR_MASK_ADD_RN_RD) == INSTR_ADD_FP_SP))

#define is_instr_pop(instr) \
	((((instr) & INSTR_MASK_LDMIA) == INSTR_LDMIA_SP))

#define is_instr_pop_fp(instr) \
	((((instr) & INSTR_MASK_LDMIA) == INSTR_LDMIA_SP) && \
	check_in_register_list(instr, FP)) \

#define is_instr_branch(instr) \
	((((instr) & INSTR_MASK_BRANCH) == INSTR_BRANCH) || \
	(((instr) & INSTR_MASK_BRANCH_L) == INSTR_BRANCH_L) || \
	(((instr) & INSTR_MASK_BRANCH_X) == INSTR_BRANCH_X) || \
	(((instr) & INSTR_MASK_BRANCH_LX) == INSTR_BRANCH_LX))

enum {
	INSTR_TYPE_PUSH_FP,
	INSTR_TYPE_PUSH_FP_LR,
	INSTR_TYPE_ADD_FP,
	INSTR_TYPE_POP,
	INSTR_TYPE_POP_FP,
	INSTR_TYPE_BRANCH,
	INSTR_TYPE_UNKNOWN,
};

#define MAX_CHECK_INSTR_LENGTH	5

static unsigned long instr_before[MAX_CHECK_INSTR_LENGTH];
static int instr_before_nr;

static unsigned long instr_after[MAX_CHECK_INSTR_LENGTH];
static int instr_after_nr;

static int read_instructions(unsigned long *addr, int direction)
{
	int i, delta, type, nr_instr = 0;
	unsigned long instr, *c_addr = addr;
	unsigned long *instr_p;

	if (direction) {
		delta = 1;
		instr_p = instr_after;
	} else {
		delta = -1;
		instr_p = instr_before;
	}

	for (i = 0; i < MAX_CHECK_INSTR_LENGTH; i++) {
		if (get_user(instr, c_addr))
			return -1;

		type = INSTR_TYPE_UNKNOWN;
		if (is_instr_push_with_fp(instr))
			type = INSTR_TYPE_PUSH_FP;
		else if (is_instr_push_with_fp_lr(instr))
			type = INSTR_TYPE_PUSH_FP_LR;
		else if (is_instr_add_fp(instr))
			type = INSTR_TYPE_ADD_FP;
		else if (is_instr_pop_fp(instr))
			type = INSTR_TYPE_POP_FP;
		else if (is_instr_pop(instr))
			type = INSTR_TYPE_POP;
		else if (is_instr_branch(instr))
			type = INSTR_TYPE_BRANCH;
		else
			type = INSTR_TYPE_UNKNOWN;

		if (type != INSTR_TYPE_UNKNOWN)
			instr_p[nr_instr++] = type;

		c_addr += delta;
	}

	return nr_instr;
}

static int is_func_prologue_epilogue(unsigned long *addr)
{
	unsigned long instr_prev, instr_curr, instr_next;

	instr_before_nr = 0;
	instr_after_nr = 0;

	if (get_user(instr_curr, addr))
		return 1;

	if (get_user(instr_prev, addr - 1))
		return 1;

	if (get_user(instr_next, addr + 1))
		return 1;

	if (is_instr_pop_fp(instr_curr))
		return 0;

	if (is_instr_push_with_fp(instr_curr) ||
		is_instr_push_with_fp(instr_next) ||
		is_instr_push_with_fp(instr_prev) ||
		is_instr_add_fp(instr_curr) ||
		is_instr_add_fp(instr_next) ||
		is_instr_pop_fp(instr_prev)) {
		return 1;
	}

	instr_before_nr = read_instructions(addr, 0);
	if (instr_before_nr < 0)
		return 1;

	instr_after_nr = read_instructions(addr, 1);
	if (instr_after_nr < 0)
		return 1;

	if (!instr_before_nr && !instr_after_nr)
		return 0;

	if (instr_before_nr) {
		if (instr_before[0] == INSTR_TYPE_ADD_FP)
			return 0;
		else if (instr_before[0] == INSTR_TYPE_PUSH_FP)
			return 1;
	}

	if (instr_after_nr) {
		if (instr_after[0] == INSTR_TYPE_PUSH_FP)
			return 1;
		else if (instr_after[0] == INSTR_TYPE_ADD_FP)
			return 1;
	}

	return 0;
}
#endif	/* PERF_CALLCHAIN_CHECK_PROLOG_EPILOG */

#endif  /* CONFIG_PERF_ANDROID_BACKTRACE */

#ifndef CONFIG_PERF_ANDROID_BACKTRACE
void
perf_callchain_user(struct perf_callchain_entry *entry, struct pt_regs *regs)
{
	struct frame_tail __user *tail;

	if (perf_guest_cbs && perf_guest_cbs->is_in_guest()) {
		/* We don't support guest os callchain now */
		return;
	}

	perf_callchain_store(entry, regs->ARM_pc);
	tail = (struct frame_tail __user *)regs->ARM_fp - 1;

	while ((entry->nr < PERF_MAX_STACK_DEPTH) &&
	       tail && !((unsigned long)tail & 0x3))
		tail = user_backtrace(tail, entry);
}
#else	/* CONFIG_PERF_ANDROID_BACKTRACE */
static inline int
perf_check_frame_address(unsigned long addr, struct vm_area_struct *vma)
{
	unsigned long start, end;

	if (vma) {
		start = vma->vm_start;
		end = vma->vm_end;
		if (addr >= start && addr <= end - 2 * sizeof(unsigned long))
			return 0;
	}
	return -EINVAL;
}

void
perf_callchain_user(struct perf_callchain_entry *entry, struct pt_regs *regs)
{
	struct frame_tail __user *tail;
	unsigned long *fp, reg;
	struct vm_area_struct *vma;
	int pe_flag = 0;

	if (thumb_mode(regs))
		return;

	fp = (unsigned long *)regs->ARM_fp;
	if (!access_ok(VERIFY_READ, fp, sizeof(unsigned long)))
		return;

	vma = find_vma(current->mm, regs->ARM_sp);

	if (fp < (unsigned long *)regs->ARM_sp)
		return;

	if (perf_check_frame_address((unsigned long)fp, vma))
		return;

	if (__copy_from_user_inatomic(&reg, fp, sizeof(unsigned long)))
		return;

#ifdef PERF_CALLCHAIN_CHECK_PROLOG_EPILOG
	if (is_func_prologue_epilogue((unsigned long *)regs->ARM_pc))
		pe_flag = 1;
#endif

	if (pe_flag) {
		/* fp->prev frame tail (fp, lr) */
		if (regs->ARM_lr < AB_USER_SPACE_MIN_ADDR)
			return;
		perf_callchain_store(entry, (unsigned long)regs->ARM_lr);
		tail = (struct frame_tail *)(regs->ARM_fp
			- sizeof(unsigned long));
	} else {
		if ((unsigned long *)reg > fp &&
			!perf_check_frame_address(reg, vma)) {
			/* fp->short frame tail (fp) */

			if (regs->ARM_lr < AB_USER_SPACE_MIN_ADDR)
				return;
			perf_callchain_store(entry, regs->ARM_lr);
			tail = (struct frame_tail *)(reg
				- sizeof(unsigned long));
		} else {
			/* fp->current frame tail (fp, lr) */
			tail = (struct frame_tail *)(regs->ARM_fp
				- sizeof(unsigned long));
		}
	}

	if ((unsigned long *)tail->fp <= fp ||
		perf_check_frame_address(tail->fp, vma)) {
		return;
	}

	while (tail && !((unsigned long)tail & 0x3)) {
		if (perf_check_frame_address((unsigned long)tail, vma) ||
			tail->lr < AB_USER_SPACE_MIN_ADDR) {
			return;
		}
		tail = user_backtrace(tail, entry);
	}
}
#endif	/* CONFIG_PERF_ANDROID_BACKTRACE */

/*
 * Gets called by walk_stackframe() for every stackframe. This will be called
 * whist unwinding the stackframe and is like a subroutine return so we use
 * the PC.
 */
static int
callchain_trace(struct stackframe *fr,
		void *data)
{
	struct perf_callchain_entry *entry = data;
	perf_callchain_store(entry, fr->pc);
	return 0;
}

void
perf_callchain_kernel(struct perf_callchain_entry *entry, struct pt_regs *regs)
{
	struct stackframe fr;

	if (perf_guest_cbs && perf_guest_cbs->is_in_guest()) {
		/* We don't support guest os callchain now */
		return;
	}

	fr.fp = regs->ARM_fp;
	fr.sp = regs->ARM_sp;
	fr.lr = regs->ARM_lr;
	fr.pc = regs->ARM_pc;
	walk_stackframe(&fr, callchain_trace, entry);
}

unsigned long perf_instruction_pointer(struct pt_regs *regs)
{
	if (perf_guest_cbs && perf_guest_cbs->is_in_guest())
		return perf_guest_cbs->get_guest_ip();

	return instruction_pointer(regs);
}

unsigned long perf_misc_flags(struct pt_regs *regs)
{
	int misc = 0;

	if (perf_guest_cbs && perf_guest_cbs->is_in_guest()) {
		if (perf_guest_cbs->is_user_mode())
			misc |= PERF_RECORD_MISC_GUEST_USER;
		else
			misc |= PERF_RECORD_MISC_GUEST_KERNEL;
	} else {
		if (user_mode(regs))
			misc |= PERF_RECORD_MISC_USER;
		else
			misc |= PERF_RECORD_MISC_KERNEL;
	}

	return misc;
}

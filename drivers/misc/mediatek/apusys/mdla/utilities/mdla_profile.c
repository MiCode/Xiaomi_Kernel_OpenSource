// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/seq_file.h>

#include <common/mdla_device.h>

#include <utilities/mdla_profile.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_debug.h>
#include <utilities/mdla_trace.h>


/* FIXME: Remove */
#define TIMER_CTRL_IN_CMD_FLOW 0

static u32 mdla_prof_core_bitmask;

struct mdla_prof_dev {
	int id;
	struct hrtimer polling_pmu_timer;
	struct mutex lock;
	u32 timer_started;
};

#define mdla_prof_trace_core_set(id) (mdla_prof_core_bitmask |= (1 << (id)))
#define mdla_prof_trace_core_clr(id) (mdla_prof_core_bitmask &= ~(1 << (id)))
#define mdla_prof_trace_core_get()   (mdla_prof_core_bitmask)

static void mdla_prof_dump_pmu_count(struct mdla_dev *mdla_device)
{
	u32 c[MDLA_PMU_COUNTERS] = {0};

	if (mdla_device->power_is_on == false)
		return;

	mdla_util_pmu_ops_get()->reg_counter_read(mdla_device->mdla_id, c);

	mdla_perf_debug("_id=c%d, c1=%u, c2=%u, c3=%u, c4=%u, c5=%u, c6=%u, c7=%u, c8=%u, c9=%u, c10=%u, c11=%u, c12=%u, c13=%u, c14=%u, c15=%u\n",
		mdla_device->mdla_id,
		c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
		c[8], c[9], c[10], c[11], c[12], c[13], c[14]);

	mdla_trace_pmu_polling(mdla_device->mdla_id, c);
}

static enum hrtimer_restart mdla_prof_pmu_polling(struct hrtimer *timer)
{
	struct mdla_prof_dev *prof;
	u64 period;

	prof = container_of(timer, struct mdla_prof_dev, polling_pmu_timer);
	period = mdla_dbg_read_u64(FS_CFG_PERIOD);

	if (!period)
		return HRTIMER_NORESTART;

	/* call functions need to be called periodically */
	mdla_prof_dump_pmu_count(mdla_get_device(prof->id));

	if (!prof->timer_started)
		return HRTIMER_NORESTART;

	hrtimer_forward_now(&prof->polling_pmu_timer,
		ns_to_ktime(period * 1000));

	return HRTIMER_RESTART;
}

static int mdla_prof_pmu_polling_start(struct mdla_prof_dev *prof)
{
	hrtimer_start(&prof->polling_pmu_timer,
		ns_to_ktime(mdla_dbg_read_u64(FS_CFG_PERIOD) * 1000),
		HRTIMER_MODE_REL);
	mdla_perf_debug("%s: hrtimer_start()\n", __func__);

	return 0;
}

static int mdla_prof_pmu_polling_stop(struct mdla_prof_dev *prof, int wait)
{
	int ret = 0;

	if (wait) {
		hrtimer_cancel(&prof->polling_pmu_timer);
		mdla_perf_debug("%s: hrtimer_cancel()\n", __func__);
	} else {
		ret = hrtimer_try_to_cancel(&prof->polling_pmu_timer);
		mdla_perf_debug("%s: hrtimer_try_to_cancel(): %d\n",
					   __func__, ret);
	}
	return ret;
}

static void mdla_prof_pmu_timer_enable(unsigned int core_id, bool en)
{
	struct mdla_dev *mdla_device;

	mdla_device = mdla_get_device(core_id);

	if (!mdla_device->prof)
		return;

	mutex_lock(&mdla_device->prof->lock);

	if (en && !mdla_device->prof->timer_started) {
		mdla_prof_pmu_polling_start(mdla_device->prof);
		mdla_device->prof->timer_started = 1;
	} else if (!en && mdla_device->prof->timer_started) {
		mdla_device->prof->timer_started = 0;
		mdla_prof_pmu_polling_stop(mdla_device->prof, 1);
	}

	mutex_unlock(&mdla_device->prof->lock);
}

bool mdla_prof_pmu_timer_is_running(unsigned int core_id)
{
	return mdla_get_device(core_id)->prof->timer_started;
}

void mdla_prof_info_show(struct seq_file *s)
{
	int i;

	for_each_mdla_core(i) {
		seq_printf(s, "pmu timer%d enable = %d\n",
			i, mdla_get_device(i)->prof->timer_started);
	}
}

void mdla_prof_pmu_timer_start(void)
{
	int i;

	for_each_mdla_core(i)
		mdla_prof_pmu_timer_enable(i, true);
}

void mdla_prof_pmu_timer_stop(void)
{
	int i;

	for_each_mdla_core(i)
		mdla_prof_pmu_timer_enable(i, false);
}

bool mdla_prof_use_dbgfs_pmu_event(void)
{
	return !mdla_dbg_read_u32(FS_NN_PMU_POLLING);
}

void mdla_prof_start(unsigned int core_id)
{
#if TIMER_CTRL_IN_CMD_FLOW
	struct mdla_dev *mdla_device;

	if (!met_pmu_timer_en())
		return;

	mdla_device = mdla_get_device(core_id);

	if (!mdla_device->prof)
		return;

	mutex_lock(&mdla_device->prof->lock);

	if (mdla_device->prof->timer_started)
		goto out;

	mdla_prof_trace_core_set(core_id);
	mdla_prof_pmu_polling_start(mdla_device);
	mdla_device->prof->timer_started = 1;

out:
	mutex_unlock(&mdla_device->prof->lock);
#endif
}

void mdla_prof_stop(unsigned int core_id, int wait)
{
#if TIMER_CTRL_IN_CMD_FLOW
	struct mdla_dev *mdla_device;

	if (!met_pmu_timer_en())
		return;

	mdla_device = mdla_get_device(core_id);

	if (!mdla_device->prof)
		return;

	mutex_lock(&mdla_device->prof->lock);

	if (!mdla_device->prof->timer_started)
		goto out;

	mdla_prof_trace_core_clr(core_id);
	mdla_prof_pmu_polling_stop(mdla_device, wait);
	mdla_device->prof->timer_started = 0;

out:
	mutex_unlock(&mdla_device->prof->lock);
#endif
}


void mdla_prof_iter(int core_id)
{
#if TIMER_CTRL_IN_CMD_FLOW
	if (met_pmu_timer_en())
		mdla_prof_dump_pmu_count(mdla_get_device(core_id));
#endif
}

void mdla_prof_init(void)
{
	struct mdla_dev *mdla_device;
	int i;

	mdla_prof_core_bitmask = 0;

	for_each_mdla_core(i) {
		mdla_device = mdla_get_device(i);

		mdla_device->prof = kzalloc(sizeof(struct mdla_prof_dev),
					GFP_KERNEL);
		if (!mdla_device->prof)
			goto err;

		mdla_device->prof->id = i;
		mdla_device->prof->timer_started = 0;

		hrtimer_init(&mdla_device->prof->polling_pmu_timer,
					CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		mdla_device->prof->polling_pmu_timer.function
					= mdla_prof_pmu_polling;

		mutex_init(&mdla_device->prof->lock);
	}
	return;

err:
	for (i = i - 1; i >= 0; i--) {
		kfree(mdla_device->prof);
		mdla_device->prof = NULL;
	}
}

void mdla_prof_deinit(void)
{
	struct mdla_dev *mdla_device;
	int i;

	mdla_prof_core_bitmask = 0;

	for_each_mdla_core(i) {
		mdla_device = mdla_get_device(i);
		mutex_destroy(&mdla_device->prof->lock);
		kfree(mdla_device->prof);
		mdla_device->prof = NULL;
	}
}



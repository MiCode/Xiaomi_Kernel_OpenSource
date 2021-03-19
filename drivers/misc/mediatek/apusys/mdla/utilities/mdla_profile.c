// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>

#include <common/mdla_device.h>
#include <common/mdla_scheduler.h>

#include <utilities/mdla_profile.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_debug.h>
#include <utilities/mdla_trace.h>

#define LAT_BRKDN_NUM (4)
#define LAT_NUM       (LAT_BRKDN_NUM + 3)

enum MDLA_DEBUG_FS_PROF {
	PROF_PMU_TIMER_STOP,
	PROF_PMU_TIMER_START,
	PROF_LATENCY_ENABLE,
	PROF_LATENCY_DISABLE,
};

static u32 mdla_prof_core_bitmask;
static bool mdla_prof_lat_en;
static u32 fs_cfg_timer_en;

struct mdla_prof_lat {
	u64 max;
	u64 sum;
	u32 cnt;
};

struct mdla_prof_dev {
	int id;
	struct hrtimer polling_pmu_timer;
	struct mutex lock;
	u32 timer_started;

	union {
		struct mdla_prof_lat lat[LAT_NUM];
		struct {
			struct mdla_prof_lat pre_cmd;
			struct mdla_prof_lat post_cmd;
			struct mdla_prof_lat ip_time;
			struct mdla_prof_lat lat_brkdn[LAT_BRKDN_NUM];
		};
	};

	u64 ts[NR_PROF_TS];
	u64 lat_brkdn_ts[LAT_BRKDN_NUM];
};

#define mdla_prof_trace_core_set(id) (mdla_prof_core_bitmask |= (1 << (id)))
#define mdla_prof_trace_core_clr(id) (mdla_prof_core_bitmask &= ~(1 << (id)))
#define mdla_prof_trace_core_get()   (mdla_prof_core_bitmask)

static void mdla_prof_dummy_start(u32 core_id) {}
static void mdla_prof_dummy_stop(u32 core_id, int wait) {}
static void mdla_prof_dummy_iter(u32 core_id) {}
static bool mdla_prof_dummy_use_dbgfs_pmu_event(u32 core_id)
{
	return false;
}
static bool mdla_prof_dummy_pmu_tmr_en(u32 core_id)
{
	return false;
}

static void (*prof_start)(u32 core_id) = mdla_prof_dummy_start;
static void (*prof_stop)(u32 core_id, int wait) = mdla_prof_dummy_stop;
static void (*prof_iter)(u32 core_id) = mdla_prof_dummy_iter;
static bool (*prof_use_dbgfs_pmu_event)(u32 core_id)
					= mdla_prof_dummy_use_dbgfs_pmu_event;
static bool (*prof_pmu_tmr_en)(u32 core_id) = mdla_prof_dummy_pmu_tmr_en;


static void mdla_prof_lat_clear(struct mdla_prof_dev *prof)
{
	int i;

	for (i = 0; i < LAT_NUM; i++) {
		prof->lat[i].max = 0;
		prof->lat[i].sum = 0;
		prof->lat[i].cnt = 0;
	}

	for (i = 0; i < NR_PROF_TS; i++)
		prof->ts[i] = 0;

	for (i = 0; i < LAT_BRKDN_NUM; i++)
		prof->lat_brkdn_ts[i] = 0;
}

static void mdla_prof_lat_save(struct mdla_prof_lat *lat, s64 dur)
{
	if (dur < 0)
		return;

	if (dur > lat->max)
		lat->max = dur;

	lat->sum += dur;
	lat->cnt++;
}

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
	period = mdla_dbg_read_u64(FS_CFG_PMU_PERIOD);

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
		ns_to_ktime(mdla_dbg_read_u64(FS_CFG_PMU_PERIOD) * 1000),
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

static void mdla_prof_pmu_timer_enable(u32 core_id)
{
	struct mdla_prof_dev *prof = mdla_get_device(core_id)->prof;

	if (!prof)
		return;

	mutex_lock(&prof->lock);

	if (!prof->timer_started) {
		mdla_prof_pmu_polling_start(prof);
		prof->timer_started = 1;
	}

	mutex_unlock(&prof->lock);
}

static void mdla_prof_pmu_timer_disable(u32 core_id)
{
	struct mdla_prof_dev *prof = mdla_get_device(core_id)->prof;

	if (!prof)
		return;

	mutex_lock(&prof->lock);

	if (prof->timer_started) {
		prof->timer_started = 0;
		mdla_prof_pmu_polling_stop(prof, 1);
	}

	mutex_unlock(&prof->lock);
}

/* profiling mechanism - v1 */
static inline bool mdla_prof_v1_pmu_tmr_en(u32 core_id)
{
	return fs_cfg_timer_en;
}

static void mdla_prof_v1_start(u32 core_id)
{
	struct mdla_prof_dev *prof = mdla_get_device(core_id)->prof;

	if (!prof)
		return;

	if (!mdla_prof_v1_pmu_tmr_en(core_id))
		return;

	mutex_lock(&prof->lock);

	if (prof->timer_started)
		goto out;

	mdla_prof_trace_core_set(core_id);
	mdla_prof_pmu_polling_start(prof);
	prof->timer_started = 1;

out:
	mutex_unlock(&prof->lock);
}

static void mdla_prof_v1_stop(u32 core_id, int wait)
{
	struct mdla_prof_dev *prof = mdla_get_device(core_id)->prof;

	if (!prof)
		return;

	if (!mdla_prof_v1_pmu_tmr_en(core_id))
		return;

	mutex_lock(&prof->lock);

	if (!prof->timer_started)
		goto out;

	mdla_prof_trace_core_clr(core_id);
	mdla_prof_pmu_polling_stop(prof, wait);
	prof->timer_started = 0;

out:
	mutex_unlock(&prof->lock);
}

static void mdla_prof_v1_iter(u32 core_id)
{
	if (mdla_prof_v1_pmu_tmr_en(core_id))
		mdla_prof_dump_pmu_count(mdla_get_device(core_id));
}

static bool mdla_prof_v1_use_dbgfs_pmu_event(u32 core_id)
{
	return mdla_trace_enable();
}

static int mdla_prof_v1_show(struct seq_file *s, void *data)
{
	int i;

	seq_printf(s, "period=%llu\n", mdla_dbg_read_u64(FS_CFG_PMU_PERIOD));

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		seq_printf(s, "c%d=0x%x\n",
			(i + 1), mdla_dbg_read_u32(FS_C1 + i));

	return 0;
}

static int mdla_prof_v1_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdla_prof_v1_show, inode->i_private);
}

static ssize_t mdla_prof_v1_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	int i, prio;
	struct mdla_util_pmu_ops *pmu_ops;

	pmu_ops = mdla_util_pmu_ops_get();

	for_each_mdla_core(i) {
		for (prio = 0; prio < PRIORITY_LEVEL; prio++) {
			struct mdla_pmu_info *pmu;

			pmu = pmu_ops->get_info(i, prio);

			if (!pmu)
				continue;

			pmu_ops->clr_counter_variable(pmu);
			pmu_ops->set_percmd_mode(pmu, NORMAL);
		}

		pmu_ops->reset_counter(i);
	}

	return count;
}

static const struct file_operations mdla_prof_v1_fops = {
	.open = mdla_prof_v1_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mdla_prof_v1_write,
};



/* profiling mechanism - v2 */

static inline bool mdla_prof_v2_pmu_tmr_en(u32 core_id)
{
	return mdla_get_device(core_id)->prof->timer_started;
}

static void mdla_prof_v2_start(u32 core_id)
{
}

static void mdla_prof_v2_stop(u32 core_id, int wait)
{
}

#define lat_brkdn_idx_is_invalid(prof, idx)\
(idx < 1 || !prof->lat_brkdn_ts[idx] || !prof->lat_brkdn_ts[idx - 1])

static void mdla_prof_v2_iter(u32 core_id)
{
	int i;
	struct mdla_prof_dev *prof;

	if (!mdla_prof_lat_en)
		return;

	prof = mdla_get_device(core_id)->prof;

	mdla_prof_lat_save(&prof->pre_cmd,
			prof->ts[TS_HW_FIRST_TRIGGER] - prof->ts[TS_CMD_START]);
	mdla_prof_lat_save(&prof->post_cmd,
			prof->ts[TS_CMD_FINISH] - prof->ts[TS_HW_LAST_INTR]);
	mdla_prof_lat_save(&prof->ip_time,
			prof->ts[TS_HW_INTR] - prof->ts[TS_HW_TRIGGER]);

	for (i = 1; i < LAT_BRKDN_NUM; i++) {
		if (lat_brkdn_idx_is_invalid(prof, i))
			continue;
		mdla_prof_lat_save(&prof->lat_brkdn[i],
			prof->lat_brkdn_ts[i] - prof->lat_brkdn_ts[i - 1]);
	}
}

static bool mdla_prof_v2_use_dbgfs_pmu_event(u32 core_id)
{
	return mdla_get_device(core_id)->prof->timer_started
			&& !mdla_dbg_read_u32(FS_PMU_EVT_BY_APU);
}

static void mdla_prof_v2_show_lat(struct seq_file *s,
		struct mdla_prof_lat *lat, char *str)
{
	u64 avg_ns;
	u32 avg, rem, max;

	if (!lat->cnt) {
		seq_printf(s, "%20s: %7u.%03u us, %7u us, %6d\n",
			str, 0, 0, 0, 0);
		return;
	}

	avg_ns = div64_u64(lat->sum, lat->cnt);
	max = (u32)div64_u64(lat->max, 1000);
	avg = (u32)div_u64_rem(avg_ns, 1000, &rem);

	seq_printf(s, "%20s: %7u.%03u us, %7u us, %6d\n",
			str,
			avg,
			rem,
			max,
			lat->cnt);
}

static void mdla_prof_lat_brkdn_show(struct seq_file *s,
		struct mdla_prof_dev *prof)
{
	char str[16] = {0};
	int i;

	for (i = 1; i < LAT_BRKDN_NUM; i++) {
		if (lat_brkdn_idx_is_invalid(prof, i))
			continue;

		if (snprintf(str, sizeof(str), "idx %2d -> %2d", i - 1, i) > 0)
			mdla_prof_v2_show_lat(s, &prof->lat_brkdn[i], str);
	}
}

static int mdla_prof_v2_show(struct seq_file *s, void *data)
{
	int i;
	struct mdla_prof_dev *prof;

	seq_printf(s, "get pmu data period=%llu\n",
			mdla_dbg_read_u64(FS_CFG_PMU_PERIOD));

	for (i = 0; i < MDLA_PMU_COUNTERS; i++) {
		seq_printf(s, "c%-2d = 0x%-8x",
			(i + 1), mdla_dbg_read_u32(FS_C1 + i));
		if (((i + 1) % 4) == 0)
			seq_puts(s, "\n");
		else
			seq_puts(s, ", ");
	}
	seq_puts(s, "\n");

	for_each_mdla_core(i) {
		prof = mdla_get_device(i)->prof;

		seq_printf(s, "\npmu timer%d enable = %d\n",
			i, prof->timer_started);

		if (mdla_prof_lat_en)
			continue;

		seq_printf(s, "%18s %d: %14s, %10s, %6s\n",
			"core", i, "avg", "max", "cnt");
		mdla_prof_v2_show_lat(s, &prof->pre_cmd, "pre-cmd latency");
		mdla_prof_v2_show_lat(s, &prof->post_cmd, "post-cmd latency");
		mdla_prof_v2_show_lat(s, &prof->ip_time, "HW IP time");

		mdla_prof_lat_brkdn_show(s, prof);
	}

	seq_printf(s, "\nlatency profiling: %s\n\n",
			mdla_prof_lat_en ? "running" : "stopped");

	seq_puts(s, "==== usage ====\n");
	seq_printf(s, "echo [param] > /d/mdla/%s\n", DBGFS_PROF_NAME_V2);
	seq_puts(s, "param:\n");
	seq_printf(s, " %2d: stop pmu polling timer\n",
			PROF_PMU_TIMER_STOP);
	seq_printf(s, " %2d: start pmu polling timer\n",
			PROF_PMU_TIMER_START);
	seq_printf(s, " %2d: enable latency profile\n",
			PROF_LATENCY_ENABLE);
	seq_printf(s, " %2d: disable latency profile\n",
			PROF_LATENCY_DISABLE);

	return 0;
}

static int mdla_prof_v2_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdla_prof_v2_show, inode->i_private);
}

static ssize_t mdla_prof_v2_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *buf;
	u32 param;
	int i, prio, ret = 0;
	struct mdla_util_pmu_ops *pmu_ops;

	buf = kzalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = copy_from_user(buf, buffer, count);
	if (ret)
		goto out;

	buf[count] = '\0';

	if (kstrtouint(buf, 10, &param) != 0) {
		ret = -EINVAL;
		goto out;
	}

	switch (param) {
	case PROF_PMU_TIMER_STOP:
		for_each_mdla_core(i)
			mdla_prof_pmu_timer_disable(i);
		break;

	case PROF_PMU_TIMER_START:
		for_each_mdla_core(i)
			mdla_prof_pmu_timer_disable(i);

		pmu_ops = mdla_util_pmu_ops_get();

		for_each_mdla_core(i) {
			for (prio = 0; prio < PRIORITY_LEVEL; prio++) {
				struct mdla_pmu_info *pmu;

				pmu = pmu_ops->get_info(i, prio);

				if (!pmu)
					continue;

				pmu_ops->clr_counter_variable(pmu);
				pmu_ops->set_percmd_mode(pmu, NORMAL);
			}

			pmu_ops->reset_counter(i);
		}

		for_each_mdla_core(i)
			mdla_prof_pmu_timer_enable(i);
		break;

	case PROF_LATENCY_ENABLE:
		if (!mdla_prof_lat_en) {
			for_each_mdla_core(i)
				mdla_prof_lat_clear(mdla_get_device(i)->prof);
			mdla_prof_lat_en = true;
		}
		break;

	case PROF_LATENCY_DISABLE:
		mdla_prof_lat_en = false;
		break;

	default:
		break;
	}

out:
	kfree(buf);
	return count;
}

static const struct file_operations mdla_prof_v2_fops = {
	.open = mdla_prof_v2_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mdla_prof_v2_write,
};

bool mdla_prof_pmu_tmr_en(u32 core_id)
{
	return prof_pmu_tmr_en(core_id);
}

void mdla_prof_start(u32 core_id)
{
	prof_start(core_id);
}

void mdla_prof_stop(u32 core_id, int wait)
{
	prof_stop(core_id, wait);
}

void mdla_prof_iter(u32 core_id)
{
	prof_iter(core_id);
}

bool mdla_prof_use_dbgfs_pmu_event(u32 core_id)
{
	return prof_use_dbgfs_pmu_event(core_id);
}

void mdla_prof_set_ts(u32 core_id, int index, u64 time_ns)
{
	if (unlikely(index < 0 || index >= NR_PROF_TS))
		return;

	mdla_get_device(core_id)->prof->ts[index] = time_ns;
}

u64 mdla_prof_get_ts(u32 core_id, int index)
{
	if (unlikely(index < 0 || index >= NR_PROF_TS))
		return 0;

	return mdla_get_device(core_id)->prof->ts[index];
}

/**
 *  Latency breakdown (v2 version)
 *  --- usage ---
 *    1. modify LAT_BRKDN_NUM and add mdla_prof_ts()
 *       <e.g.>
 *       aa.c                    bb.c
 *         ...                     ...
 *         mdla_prof_ts(id, 0);    mdla_prof_ts(id, 1);
 *         ...                     ...
 *         mdla_prof_ts(id, 2);
 *         ...
 *
 *    2. start profile latency (duration of idx[N] -> idx[N-1]) and get
 *       result by shell command
 *         echo 3 > /d/mdla/profile
 *         cat /d/mdla/profile
 */
void mdla_prof_ts(u32 core_id, int idx)
{
	if ((idx < 0 || idx >= LAT_BRKDN_NUM))
		return;

	mdla_get_device(core_id)->prof->lat_brkdn_ts[idx] = sched_clock();
}

static void mdla_prof_fs_init(int mode)
{
	struct dentry *d = mdla_dbg_get_fs_root();

	if (!d)
		return;

	switch (mode) {
	case PROF_V1:
		prof_start               = mdla_prof_v1_start;
		prof_stop                = mdla_prof_v1_stop;
		prof_iter                = mdla_prof_v1_iter;
		prof_pmu_tmr_en          = mdla_prof_v1_pmu_tmr_en;
		prof_use_dbgfs_pmu_event = mdla_prof_v1_use_dbgfs_pmu_event;
		debugfs_create_file(DBGFS_PROF_NAME_V1, 0644, d, NULL,
				&mdla_prof_v1_fops);
		debugfs_create_u32("prof_start", 0660, d, &fs_cfg_timer_en);
		break;
	case PROF_V2:
		prof_start               = mdla_prof_v2_start;
		prof_stop                = mdla_prof_v2_stop;
		prof_iter                = mdla_prof_v2_iter;
		prof_pmu_tmr_en          = mdla_prof_v2_pmu_tmr_en;
		prof_use_dbgfs_pmu_event = mdla_prof_v2_use_dbgfs_pmu_event;
		debugfs_create_file(DBGFS_PROF_NAME_V2, 0644, d, NULL,
				&mdla_prof_v2_fops);
		break;
	default:
		break;
	}

}

void mdla_prof_init(int mode)
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
	mdla_prof_fs_init(mode);

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
		mdla_prof_pmu_timer_disable(i);
		mdla_device = mdla_get_device(i);
		mutex_destroy(&mdla_device->prof->lock);
		kfree(mdla_device->prof);
		mdla_device->prof = NULL;
	}
}

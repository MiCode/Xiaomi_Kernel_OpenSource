/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include "mdss.h"
#include "mdss_mdp.h"
#include "mdss_debug.h"

#ifdef CONFIG_FB_MSM_MDSS_FRC_DEBUG

#define FRC_DEFAULT_ENABLE 1
#define FRC_DEFAULT_LOG_ENABLE 0

#define FRC_DEBUG_STAT_MAX_SLOT 1024

DEFINE_SPINLOCK(frc_lock);

struct cadence {
	int repeat;
	int kickoff_idx;
	int vsync_idx;
};

struct vsync_stat {
	int vsync_cnt;
	s64 vsync_ts;
};

struct kick_stat {
	s64 kickoff_ts;
	u32 vsync;
	int remain;
	struct mdss_mdp_frc_info frc_info;
};

struct circ_buf {
	int index;
	int size;
	int cnt;
};

struct vsync_samples {
	struct circ_buf cbuf;
	struct vsync_stat samples[FRC_DEBUG_STAT_MAX_SLOT];
};

struct kickoff_samples {
	struct circ_buf cbuf;
	struct kick_stat samples[FRC_DEBUG_STAT_MAX_SLOT];
};

#define cbuf_init(cbuf, len) { \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	cb->index = 0; \
	cb->cnt = 0; \
	cb->size = (len); }

#define cbuf_begin(cbuf, start) ({ \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	(cb->index > cb->size) ? (cb->index + (start)) % cb->size : (start); })

#define cbuf_end(cbuf, end) ({ \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	((cb->index - 1 - (end)) % cb->size); })

#define cbuf_cur(cbuf) ({ \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	(cb->index % cb->size); })

#define cbuf_next(cbuf, idx) ({ \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	(((idx)+1) % cb->size); })

#define cbuf_prev(cbuf, idx) ({ \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	(((idx)-1) % cb->size); })

#define current_sample(cbuf) ({ \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	int idx = cb->index % cb->size; \
	&((cbuf)->samples[idx]); })

#define insert_sample(cbuf, sample) { \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	int idx = cb->index % cb->size; \
	(cbuf)->samples[idx] = (sample); \
	cb->cnt++; \
	cb->index++; }

#define advance_sample(cbuf) { \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	cb->cnt++; \
	cb->index++; }

#define sample_cnt(cbuf) ({ \
	struct circ_buf *cb = (struct circ_buf *)(cbuf); \
	(cb->cnt % cb->size); })

struct mdss_dbg_frc_stat {
	int cadence_id;
	int display_fp1000s;
	struct vsync_samples vs;
	struct kickoff_samples ks;
	struct cadence cadence_info[FRC_DEBUG_STAT_MAX_SLOT];
};

struct mdss_dbg_frc {
	struct dentry *frc;
	int frc_enable;
	int log_enable;
	struct mdss_dbg_frc_stat frc_stat[2];
	int index;
} mdss_dbg_frc;

static struct mdss_dbg_frc_stat *__current_frc_stat(
	struct mdss_dbg_frc *dbg_frc)
{
	return &dbg_frc->frc_stat[dbg_frc->index];
}

static void __init_frc_stat(struct mdss_dbg_frc *dbg_frc)
{
	struct mdss_dbg_frc_stat *frc_stat = __current_frc_stat(dbg_frc);

	memset(frc_stat, 0, sizeof(struct mdss_dbg_frc_stat));

	/* TODO: increase vsync buffer to avoid wrap around */
	cbuf_init(&frc_stat->ks, FRC_DEBUG_STAT_MAX_SLOT);
	cbuf_init(&frc_stat->vs, FRC_DEBUG_STAT_MAX_SLOT/2);
}

static struct mdss_dbg_frc_stat *__swap_frc_stat(
	struct mdss_dbg_frc *dbg_frc)
{
	int prev_index = dbg_frc->index;

	dbg_frc->index = (dbg_frc->index + 1) % 2;
	__init_frc_stat(dbg_frc);

	return &dbg_frc->frc_stat[prev_index];
}

void mdss_debug_frc_add_vsync_sample(struct mdss_mdp_ctl *ctl,
	ktime_t vsync_time)
{
	if (mdss_dbg_frc.log_enable) {
		unsigned long flags;
		struct mdss_dbg_frc_stat *frc_stat;
		struct vsync_stat vstat;

		spin_lock_irqsave(&frc_lock, flags);
		frc_stat = __current_frc_stat(&mdss_dbg_frc);
		vstat.vsync_cnt = ctl->vsync_cnt;
		vstat.vsync_ts = ktime_to_us(vsync_time);
		insert_sample(&frc_stat->vs, vstat);
		spin_unlock_irqrestore(&frc_lock, flags);
	}
}

/* collect FRC data for debug ahead of repeat */
void mdss_debug_frc_add_kickoff_sample_pre(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_frc_info *frc_info, int remaining)
{
	if (mdss_dbg_frc.log_enable) {
		unsigned long flags;
		struct mdss_dbg_frc_stat *frc_stat;

		spin_lock_irqsave(&frc_lock, flags);
		frc_stat = __current_frc_stat(&mdss_dbg_frc);

		/* Don't update statistics when video repeats */
		if (frc_info->cur_frc.frame_cnt
				!= frc_info->last_frc.frame_cnt) {
			struct kick_stat *kstat = current_sample(&frc_stat->ks);

			kstat->vsync = ctl->vsync_cnt;
		}

		frc_stat->cadence_id = frc_info->cadence_id;
		frc_stat->display_fp1000s = frc_info->display_fp1000s;
		spin_unlock_irqrestore(&frc_lock, flags);
	}
}

/* collect FRC data for debug later than repeat */
void mdss_debug_frc_add_kickoff_sample_post(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_frc_info *frc_info, int remaining)
{
	if (mdss_dbg_frc.log_enable) {
		unsigned long flags;

		spin_lock_irqsave(&frc_lock, flags);
		/* Don't update statistics when video repeats */
		if (frc_info->cur_frc.frame_cnt
				!= frc_info->last_frc.frame_cnt) {
			struct mdss_dbg_frc_stat *frc_stat
				= __current_frc_stat(&mdss_dbg_frc);
			struct kick_stat *kstat = current_sample(&frc_stat->ks);
			ktime_t kickoff_time = ktime_get();

			kstat->kickoff_ts = ktime_to_us(kickoff_time);
			kstat->frc_info = *frc_info;
			kstat->remain = remaining;

			advance_sample(&frc_stat->ks);
		}
		spin_unlock_irqrestore(&frc_lock, flags);
	}
}

int mdss_debug_frc_frame_repeat_disabled(void)
{
	return !mdss_dbg_frc.frc_enable;
}

/* find the closest vsync right to this kickoff time */
static int __find_right_vsync(struct vsync_samples *vs, s64 kick)
{
	int idx = cbuf_begin(vs, 0);

	for (; idx != cbuf_end(vs, 0); idx = cbuf_next(vs, idx)) {
		if (vs->samples[idx].vsync_ts >= kick)
			return idx;
	}

	return -EBADSLT;
}

/*
 * These repeat number might start from any position in the sequence. E.g.,
 * given cadence 23223, the first repeat might be 3 and the repeating pattern
 * might be 32232, also, the first repeat could be the 4th 3, so the repeating
 * pattern will be 32322. Below predefined patterns are going to be used to
 * find the position of the first repeat in the full sequence, then we can
 * easily known what the remaining expected repeats.
 */
#define CADENCE_22_LEN 2
static int pattern_22[CADENCE_22_LEN] = {2, 2};

#define CADENCE_23_LEN 2
static int pattern_23[CADENCE_23_LEN][CADENCE_23_LEN] = {
	{2, 3},
	{3, 2}
};

#define CADENCE_23223_LEN 5
static int pattern_23223[CADENCE_23223_LEN][CADENCE_23223_LEN] = {
	{2, 3, 2, 2, 3},
	{3, 2, 2, 3, 2},
	{2, 2, 3, 2, 3},
	{2, 3, 2, 3, 2},
	{3, 2, 3, 2, 2}
};

static int __compare_init_pattern(struct mdss_dbg_frc_stat *frc_stat,
	int *pattern, int s_idx, int e_idx)
{
	int i;

	for (i = 0; i < min(CADENCE_23223_LEN, e_idx-s_idx+1); i++) {
		if (frc_stat->cadence_info[i].repeat != pattern[i])
			break;
	}

	return i == min(CADENCE_23223_LEN, e_idx-s_idx+1);
}

static int __pattern_len(int cadence_id)
{
	switch (cadence_id) {
	case FRC_CADENCE_22:
		return CADENCE_22_LEN;
	case FRC_CADENCE_23:
		return CADENCE_23_LEN;
	case FRC_CADENCE_23223:
		return CADENCE_23223_LEN;
	}

	return 0;
}

static int *__select_pattern(struct mdss_dbg_frc_stat *frc_stat,
	int s_idx, int e_idx)
{
	int i;

	switch (frc_stat->cadence_id) {
	case FRC_CADENCE_22:
		return pattern_22;
	case FRC_CADENCE_23:
		return frc_stat->cadence_info[s_idx].repeat == 2 ?
			pattern_23[0] : pattern_23[1];
	case FRC_CADENCE_23223:
		for (i = 0; i < CADENCE_23223_LEN; i++) {
			if (__compare_init_pattern(frc_stat,
					pattern_23223[i], s_idx, e_idx))
				return pattern_23223[i];
		}
	}

	return NULL;
}

static void __check_cadence_pattern(struct mdss_dbg_frc_stat *frc_stat,
	int s_idx, int e_idx)
{
	if (s_idx < e_idx) {
		int *pattern = __select_pattern(frc_stat, s_idx, e_idx);
		int pattern_len = __pattern_len(frc_stat->cadence_id);
		struct vsync_samples *vs = &frc_stat->vs;
		struct kickoff_samples *ks = &frc_stat->ks;
		int i;

		if (!pattern) {
			pr_info("Can't match pattern in the beginning\n");
			return;
		}

		for (i = s_idx; i < e_idx; i++) {
			if (frc_stat->cadence_info[i].repeat !=
					pattern[i % pattern_len]) {
				int kidx =
					frc_stat->cadence_info[i].kickoff_idx;
				pr_info("\tUnexpected Sample: repeat=%d, kickoff=%lld, vsync=%lld\n",
					frc_stat->cadence_info[i].repeat,
					ks->samples[kidx].kickoff_ts,
					vs->samples[kidx].vsync_ts);
				break;
			}
		}

		/* init check */
		if (i < e_idx)
			__check_cadence_pattern(frc_stat, i+1, e_idx);
	}
}

static void __check_unexpected_delay(struct mdss_dbg_frc_stat *frc_stat,
	int s_idx, int e_idx)
{
	int i = 0;
	struct kickoff_samples *ks = &frc_stat->ks;

	pr_info("===== Check Unexpected Delay: =====\n");

	for (i = s_idx; i < e_idx; i++) {
		struct cadence *p_info = &frc_stat->cadence_info[i];
		struct kick_stat *kickoff = &ks->samples[p_info->kickoff_idx];

		if (kickoff->remain + kickoff->vsync
			!= kickoff->frc_info.last_vsync_cnt)
			pr_info("\tUnexpected Delay: timestamp=%lld, vsync=%d\n",
				kickoff->frc_info.cur_frc.timestamp,
				kickoff->vsync);
	}

	pr_info("===== Check Unexpected Delay End =====\n");
}

static int __is_cadence_check_supported(struct mdss_dbg_frc_stat *frc_stat)
{
	int cadence = frc_stat->cadence_id;

	return cadence == FRC_CADENCE_22 ||
		cadence == FRC_CADENCE_23 ||
		cadence == FRC_CADENCE_23223;
}

static int __find_first_valid_sample(struct mdss_dbg_frc_stat *frc_stat)
{
	int i = -EBADSLT;
	struct kickoff_samples *ks = &frc_stat->ks;
	struct vsync_samples *vs = &frc_stat->vs;
	struct kick_stat *cur_kstat = &ks->samples[cbuf_begin(ks, 0)];
	s64 cur_kick = cur_kstat->kickoff_ts;
	int cur_disp = __find_right_vsync(vs, cur_kick);

	if (cur_disp >= 0) {
		struct vsync_stat *vstat = &vs->samples[cur_disp];

		i = cbuf_begin(ks, 0);
		for (; i != cbuf_end(ks, 1); i = cbuf_next(ks, i)) {
			if (vstat->vsync_ts < ks->samples[i].kickoff_ts)
				break;
		}
	}

	return i;
}

static int __analyze_frc_samples(struct mdss_dbg_frc_stat *frc_stat, int start)
{
	struct kickoff_samples *ks = &frc_stat->ks;
	struct vsync_samples *vs = &frc_stat->vs;
	int i = start;
	int cnt = 0;

	/* analyze kickoff & vsync samples */
	for (; i != cbuf_end(ks, 1); i = cbuf_next(ks, i)) {
		/*
		 * TODO: vsync buffer is not enough so it might
		 * wrap around and drop the samples in the beginning.
		 * skip the first/last sample.
		 */
		s64 cur_kick = ks->samples[i].kickoff_ts;
		s64 right_kick = ks->samples[cbuf_next(ks, i)].kickoff_ts;
		int cur_disp = __find_right_vsync(vs, cur_kick);
		int right_disp = __find_right_vsync(vs, right_kick);

		if ((cur_disp < 0) || (right_disp < 0))
			break;

		frc_stat->cadence_info[cnt].repeat =
			right_disp >= cur_disp ? right_disp - cur_disp :
			right_disp - cur_disp + vs->cbuf.size;
		frc_stat->cadence_info[cnt].kickoff_idx = i;
		frc_stat->cadence_info[cnt].vsync_idx = cur_disp;
		cnt++;
	}

	return cnt;
}

static void __dump_frc_samples(struct mdss_dbg_frc_stat *frc_stat, int cnt)
{
	struct kickoff_samples *ks = &frc_stat->ks;
	struct vsync_samples *vs = &frc_stat->vs;
	int i = 0;

	pr_info("===== Collected FRC statistics: Cadence %d, FPS %d =====\n",
		frc_stat->cadence_id, frc_stat->display_fp1000s);
	pr_info("\tKickoff VS. VSYNC:\n");
	for (i = 0; i < cnt; i++) {
		struct cadence *p_info = &frc_stat->cadence_info[i];
		struct kick_stat *kickoff = &ks->samples[p_info->kickoff_idx];
		struct vsync_stat *vsync = &vs->samples[p_info->vsync_idx];

		pr_info("\t[K: %lld V: (%d)%lld R: %d] c_ts: %lld c_cnt: %d b_ts: %lld b_cnt: %d l_ts: %lld l_cnt: %d b_v: %d l_v: %d l_r: %d pos: %d vs: %d remain: %d\n",
			kickoff->kickoff_ts,
			vsync->vsync_cnt,
			vsync->vsync_ts,
			p_info->repeat,
			kickoff->frc_info.cur_frc.timestamp,
			kickoff->frc_info.cur_frc.frame_cnt,
			kickoff->frc_info.base_frc.timestamp,
			kickoff->frc_info.base_frc.frame_cnt,
			kickoff->frc_info.last_frc.timestamp,
			kickoff->frc_info.last_frc.frame_cnt,
			kickoff->frc_info.base_vsync_cnt,
			kickoff->frc_info.last_vsync_cnt,
			kickoff->frc_info.last_repeat,
			kickoff->frc_info.gen.pos,
			kickoff->vsync,
			kickoff->remain);
	}

	pr_info("===== End FRC statistics: =====\n");
}

static bool __is_frc_stat_empty(struct mdss_dbg_frc_stat *frc_stat)
{
	return sample_cnt(&frc_stat->vs) == 0
		|| sample_cnt(&frc_stat->ks) == 0;
}

static void mdss_frc_dump_debug_stat(struct mdss_dbg_frc *frc_debug)
{
	int i = 0;
	int cnt = 0;
	struct mdss_dbg_frc_stat *frc_stat = NULL;
	unsigned long flags;

	/* swap buffer of collect & analyze */
	spin_lock_irqsave(&frc_lock, flags);
	frc_stat = __swap_frc_stat(frc_debug);
	spin_unlock_irqrestore(&frc_lock, flags);

	if (__is_frc_stat_empty(frc_stat))
		return;

	/* find the first valid kickoff sample */
	i = __find_first_valid_sample(frc_stat);
	if (i < 0) {
		pr_debug("can't find valid sample\n");
		return;
	}

	/* analyze kickoff & vsync samples */
	cnt = __analyze_frc_samples(frc_stat, i);

	/* print collected statistics FRC data */
	__dump_frc_samples(frc_stat, cnt);

	if (__is_cadence_check_supported(frc_stat)) {
		__check_unexpected_delay(frc_stat, 0, cnt);

		pr_info("===== Check Cadence Pattern: =====\n");
		__check_cadence_pattern(frc_stat, 0, cnt);
		pr_info("===== Check Cadence Pattern End =====\n");
	}
}

static ssize_t mdss_frc_log_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	int len = 0;
	char buf[32] = {'\0'};

	if (*ppos)
		return 0; /* the end */

	len = snprintf(buf, sizeof(buf), "%d\n", mdss_dbg_frc.log_enable);
	if (len < 0 || len >= sizeof(buf))
		return 0;

	if ((count < sizeof(buf)) || copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;	/* increase offset */

	return len;
}

static ssize_t mdss_frc_log_write(struct file *file,
		    const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int enable;
	unsigned long flags;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';
	if (kstrtoint(buf, 0, &enable))
		return -EFAULT;

	if (enable && !mdss_dbg_frc.log_enable) {
		spin_lock_irqsave(&frc_lock, flags);
		__init_frc_stat(&mdss_dbg_frc);
		spin_unlock_irqrestore(&frc_lock, flags);
	}
	mdss_dbg_frc.log_enable = enable;

	pr_info("log_enable = %d\n", mdss_dbg_frc.log_enable);

	return count;
}

static const struct file_operations mdss_dbg_frc_log_fops = {
	.read = mdss_frc_log_read,
	.write = mdss_frc_log_write,
};

static ssize_t mdss_frc_dump_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	mdss_frc_dump_debug_stat(&mdss_dbg_frc);

	return count;
}

static const struct file_operations mdss_dbg_frc_dump_fops = {
	.read = NULL,
	.write = mdss_frc_dump_write,
};

int mdss_create_frc_debug(struct mdss_debug_data *mdd)
{
	mdss_dbg_frc.frc = debugfs_create_dir("frc", mdd->root);
	if (IS_ERR_OR_NULL(mdss_dbg_frc.frc)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
				PTR_ERR(mdss_dbg_frc.frc));
		mdss_dbg_frc.frc = NULL;
		return -ENODEV;
	}

	debugfs_create_u32("enable", 0644, mdss_dbg_frc.frc,
			&mdss_dbg_frc.frc_enable);
	debugfs_create_file("log", 0644, mdss_dbg_frc.frc, NULL,
						&mdss_dbg_frc_log_fops);
	debugfs_create_file("dump", 0644, mdss_dbg_frc.frc, NULL,
						&mdss_dbg_frc_dump_fops);

	mdss_dbg_frc.frc_enable = FRC_DEFAULT_ENABLE;
	mdss_dbg_frc.log_enable = FRC_DEFAULT_LOG_ENABLE;
	mdss_dbg_frc.index = 0;

	pr_debug("frc_dbg: frc_enable:%d log_enable:%d\n",
		mdss_dbg_frc.frc_enable, mdss_dbg_frc.log_enable);

	return 0;
}
#else
int mdss_create_frc_debug(struct mdss_debug_data *mdd) {return 0; }
#endif /* CONFIG_FB_MSM_MDSS_FRC_DEBUG */

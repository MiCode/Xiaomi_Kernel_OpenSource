/*
 * drivers/video/tegra/host/nvhost_syncpt.h
 *
 * Tegra Graphics Host Syncpoints
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_SYNCPT_H
#define __NVHOST_SYNCPT_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/nvhost.h>
#include <linux/atomic.h>

/* host managed and invalid syncpt id */
#define NVSYNCPT_GRAPHICS_HOST		     (0)

/* Attribute struct for sysfs min and max attributes */
struct nvhost_syncpt_attr {
	struct kobj_attribute attr;
	struct nvhost_master *host;
	int id;
};

struct nvhost_syncpt {
	struct kobject *kobj;
	atomic_t *min_val;
	atomic_t *max_val;
	u32 *base_val;
	atomic_t *lock_counts;
	const char **syncpt_names;
	struct nvhost_syncpt_attr *syncpt_attrs;
};

int nvhost_syncpt_init(struct platform_device *, struct nvhost_syncpt *);
void nvhost_syncpt_deinit(struct nvhost_syncpt *);

#define syncpt_to_dev(sp) container_of(sp, struct nvhost_master, syncpt)
#define SYNCPT_CHECK_PERIOD (2 * HZ)
#define MAX_STUCK_CHECK_COUNT 15

/**
 * Updates the value sent to hardware.
 */
static inline u32 nvhost_syncpt_incr_max(struct nvhost_syncpt *sp,
					u32 id, u32 incrs)
{
	return (u32)atomic_add_return(incrs, &sp->max_val[id]);
}

/**
 * Updated the value sent to hardware.
 */
static inline u32 nvhost_syncpt_set_max(struct nvhost_syncpt *sp,
					u32 id, u32 val)
{
	atomic_set(&sp->max_val[id], val);
	smp_wmb();
	return val;
}

static inline u32 nvhost_syncpt_read_max(struct nvhost_syncpt *sp, u32 id)
{
	smp_rmb();
	return (u32)atomic_read(&sp->max_val[id]);
}

static inline u32 nvhost_syncpt_read_min(struct nvhost_syncpt *sp, u32 id)
{
	smp_rmb();
	return (u32)atomic_read(&sp->min_val[id]);
}

int nvhost_syncpt_client_managed(struct nvhost_syncpt *sp, u32 id);
int nvhost_syncpt_nb_pts(struct nvhost_syncpt *sp);
int nvhost_syncpt_nb_bases(struct nvhost_syncpt *sp);
int nvhost_syncpt_nb_mlocks(struct nvhost_syncpt *sp);

static inline bool nvhost_syncpt_check_max(struct nvhost_syncpt *sp,
		u32 id, u32 real)
{
	u32 max;
	if (nvhost_syncpt_client_managed(sp, id))
		return true;
	max = nvhost_syncpt_read_max(sp, id);
	return (s32)(max - real) >= 0;
}

/**
 * Returns true if syncpoint min == max
 */
static inline bool nvhost_syncpt_min_eq_max(struct nvhost_syncpt *sp, u32 id)
{
	int min, max;
	smp_rmb();
	min = atomic_read(&sp->min_val[id]);
	max = atomic_read(&sp->max_val[id]);
	return (min == max);
}

void nvhost_syncpt_cpu_incr(struct nvhost_syncpt *sp, u32 id);

u32 nvhost_syncpt_update_min(struct nvhost_syncpt *sp, u32 id);
bool nvhost_syncpt_is_expired(struct nvhost_syncpt *sp, u32 id, u32 thresh);

void nvhost_syncpt_save(struct nvhost_syncpt *sp);

void nvhost_syncpt_reset(struct nvhost_syncpt *sp);
void nvhost_syncpt_reset_client(struct platform_device *pdev);

u32 nvhost_syncpt_read(struct nvhost_syncpt *sp, u32 id);
u32 nvhost_syncpt_read_wait_base(struct nvhost_syncpt *sp, u32 id);

void nvhost_syncpt_incr(struct nvhost_syncpt *sp, u32 id);

int nvhost_syncpt_wait_timeout(struct nvhost_syncpt *sp, u32 id, u32 thresh,
			u32 timeout, u32 *value);

static inline int nvhost_syncpt_wait(struct nvhost_syncpt *sp, u32 id, u32 thresh)
{
	return nvhost_syncpt_wait_timeout(sp, id, thresh,
					  MAX_SCHEDULE_TIMEOUT, NULL);
}

int nvhost_syncpt_patch_wait(struct nvhost_syncpt *sp, void *patch_addr);

void nvhost_syncpt_debug(struct nvhost_syncpt *sp);

static inline int nvhost_syncpt_is_valid(struct nvhost_syncpt *sp, u32 id)
{
	return id != NVSYNCPT_INVALID && id < nvhost_syncpt_nb_pts(sp);
}

int nvhost_mutex_try_lock(struct nvhost_syncpt *sp, int idx);

void nvhost_mutex_unlock(struct nvhost_syncpt *sp, int idx);

#endif

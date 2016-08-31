/*
 * drivers/video/tegra/host/gk20a/channel_gk20a.h
 *
 * GK20A graphics channel
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef __CHANNEL_GK20A_H__
#define __CHANNEL_GK20A_H__

#include <linux/log2.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/nvhost_ioctl.h>
struct gk20a;
struct gr_gk20a;
struct mem_mgr;
struct mem_handle;
struct dbg_session_gk20a;

#include "nvhost_channel.h"
#include "nvhost_hwctx.h"

#include "cdma_gk20a.h"
#include "mm_gk20a.h"
#include "gr_gk20a.h"

struct gpfifo {
	u32 entry0;
	u32 entry1;
};

struct notification {
	struct {
		u32 nanoseconds[2];
	} timestamp;
	u32 info32;
	u16 info16;
	u16 status;
};

struct fence {
	u32 hw_chid;
	u32 syncpt_val;
};

/* contexts associated with a channel */
struct channel_ctx_gk20a {
	struct gr_ctx_desc	gr_ctx;
	struct pm_ctx_desc	pm_ctx;
	struct patch_desc	patch_ctx;
	struct zcull_ctx_desc	zcull_ctx;
	u64	global_ctx_buffer_va[NR_GLOBAL_CTX_BUF_VA];
	bool	global_ctx_buffer_mapped;
};

struct channel_gk20a_job {
	struct mapped_buffer_node **mapped_buffers;
	int num_mapped_buffers;
	struct nvhost_fence fence;
	struct list_head list;
};

/* this is the priv element of struct nvhost_channel */
struct channel_gk20a {
	struct gk20a *g;
	bool in_use;
	int hw_chid;
	bool bound;
	bool first_init;
	bool vpr;
	pid_t pid;

	struct mem_mgr *memmgr;
	struct nvhost_channel *ch;
	struct nvhost_hwctx *hwctx;

	struct list_head jobs;
	struct mutex jobs_lock;

	struct vm_gk20a *vm;

	struct gpfifo_desc gpfifo;

	struct channel_ctx_gk20a ch_ctx;

	struct inst_desc inst_block;
	struct mem_desc_sub ramfc;

	void *userd_cpu_va;
	u64 userd_iova;
	u64 userd_gpu_va;

	s32 num_objects;
	u32 obj_class;	/* we support only one obj per channel */

	struct priv_cmd_queue priv_cmd_q;

	wait_queue_head_t notifier_wq;
	wait_queue_head_t semaphore_wq;
	wait_queue_head_t submit_wq;

	u32 timeout_accumulated_ms;
	u32 timeout_gpfifo_get;

	bool cmds_pending;
	struct {
		bool valid;
		bool wfi; /* was issued with preceding wfi */
		u32 syncpt_value;
		u32 syncpt_id;
	} last_submit_fence;

	void (*remove_support)(struct channel_gk20a *);
#if defined(CONFIG_TEGRA_GPU_CYCLE_STATS)
	struct {
	void *cyclestate_buffer;
	u32 cyclestate_buffer_size;
	struct mem_handle *cyclestate_buffer_handler;
	struct mutex cyclestate_buffer_mutex;
	} cyclestate;
#endif
	struct mutex dbg_s_lock;
	struct list_head dbg_s_list;
};

static inline bool gk20a_channel_as_bound(struct channel_gk20a *ch)
{
	return !!ch->hwctx->as_share;
}
int channel_gk20a_commit_va(struct channel_gk20a *c);

struct nvhost_unmap_buffer_args;
struct nvhost_zbc_query_table_args;
struct nvhost_fence;
struct nvhost_alloc_gpfifo_args;
struct nvhost_map_buffer_args;
struct nvhost_wait_args;
struct nvhost_zcull_bind_args;
struct nvhost_gpfifo;
struct nvhost_zbc_set_table_args;
struct nvhost_cycle_stats_args;
struct nvhost_set_priority_args;

#if defined(CONFIG_TEGRA_GK20A)
void gk20a_channel_update(struct channel_gk20a *c);
#else
static inline void gk20a_channel_update(struct channel_gk20a *c)
{
}
#endif

int gk20a_init_channel_support(struct gk20a *, u32 chid);
int gk20a_channel_init(struct nvhost_channel *ch, struct nvhost_master *host,
		       int index);
int gk20a_channel_alloc_obj(struct nvhost_channel *channel,
			u32 class_num, u32 *obj_id, u32 vaspace_share);
int gk20a_channel_free_obj(struct nvhost_channel *channel,
			u32 obj_id);
struct nvhost_hwctx *gk20a_open_channel(struct nvhost_channel *ch,
			struct nvhost_hwctx *ctx);
int gk20a_alloc_channel_gpfifo(struct channel_gk20a *c,
			struct nvhost_alloc_gpfifo_args *args);
int gk20a_submit_channel_gpfifo(struct channel_gk20a *c,
			struct nvhost_gpfifo *gpfifo, u32 num_entries,
			struct nvhost_fence *fence, u32 flags);
void gk20a_free_channel(struct nvhost_hwctx *ctx, bool finish);
int gk20a_init_error_notifier(struct nvhost_hwctx *ctx, u32 memhandle,
			u64 offset);
bool gk20a_channel_update_and_check_timeout(struct channel_gk20a *ch,
		u32 timeout_delta_ms);
void gk20a_free_error_notifiers(struct nvhost_hwctx *ctx);
void gk20a_disable_channel(struct channel_gk20a *ch,
			   bool wait_for_finish,
			   unsigned long finish_timeout);
void gk20a_disable_channel_no_update(struct channel_gk20a *ch);
int gk20a_channel_finish(struct channel_gk20a *ch, unsigned long timeout);
void gk20a_set_error_notifier(struct nvhost_hwctx *ctx, __u32 error);
int gk20a_channel_wait(struct channel_gk20a *ch,
		       struct nvhost_wait_args *args);
void gk20a_channel_semaphore_wakeup(struct gk20a *g);
int gk20a_channel_zcull_bind(struct channel_gk20a *ch,
			    struct nvhost_zcull_bind_args *args);
int gk20a_channel_zbc_set_table(struct channel_gk20a *ch,
			    struct nvhost_zbc_set_table_args *args);
int gk20a_channel_zbc_query_table(struct channel_gk20a *ch,
			    struct nvhost_zbc_query_table_args *args);
int gk20a_channel_set_priority(struct channel_gk20a *ch,
		       u32 priority);
#if defined(CONFIG_TEGRA_GPU_CYCLE_STATS)
int gk20a_channel_cycle_stats(struct channel_gk20a *ch,
			struct nvhost_cycle_stats_args *args);
#endif

int gk20a_channel_suspend(struct gk20a *g);
int gk20a_channel_resume(struct gk20a *g);

static inline
struct mem_mgr *gk20a_channel_mem_mgr(struct channel_gk20a *ch)
{
	return ch->hwctx->memmgr;
}

static inline
struct nvhost_master *host_from_gk20a_channel(struct channel_gk20a *ch)
{
	return nvhost_get_host(ch->ch->dev);
}

#endif /*__CHANNEL_GK20A_H__*/

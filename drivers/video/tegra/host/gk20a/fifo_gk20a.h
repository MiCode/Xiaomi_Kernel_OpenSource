/*
 * drivers/video/tegra/host/gk20a/fifo_gk20a.h
 *
 * GK20A graphics fifo (gr host)
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef __FIFO_GK20A_H__
#define __FIFO_GK20A_H__

#include "channel_gk20a.h"

#define MAX_RUNLIST_BUFFERS	2

/* generally corresponds to the "pbdma" engine */

struct fifo_runlist_info_gk20a {
	unsigned long *active_channels;
	/* Each engine has its own SW and HW runlist buffer.*/
	struct runlist_mem_desc mem[MAX_RUNLIST_BUFFERS];
	u32  cur_buffer;
	u32  total_entries;
	bool stopped;
	bool support_tsg;
	struct mutex mutex; /* protect channel preempt and runlist upate */
	wait_queue_head_t runlist_wq;
};

/* so far gk20a has two engines: gr and ce2(gr_copy) */
enum {
	ENGINE_GR_GK20A	    = 0,
	ENGINE_CE2_GK20A    = 1,
	ENGINE_INVAL_GK20A
};

struct fifo_pbdma_exception_info_gk20a {
	u32 status_r; /* raw register value from hardware */
	u32 id, next_id;
	u32 chan_status_v; /* raw value from hardware */
	bool id_is_chid, next_id_is_chid;
	bool chsw_in_progress;
};

struct fifo_engine_exception_info_gk20a {
	u32 status_r; /* raw register value from hardware */
	u32 id, next_id;
	u32 ctx_status_v; /* raw value from hardware */
	bool id_is_chid, next_id_is_chid;
	bool faulted, idle, ctxsw_in_progress;
};

struct fifo_mmu_fault_info_gk20a {
	u32 fault_info_v;
	u32 fault_type_v;
	u32 engine_subid_v;
	u32 client_v;
	u32 fault_hi_v;
	u32 fault_lo_v;
	u64 inst_ptr;
	const char *fault_type_desc;
	const char *engine_subid_desc;
	const char *client_desc;
};

struct fifo_engine_info_gk20a {
	u32 sw_id;
	const char *name;
	u32 dev_info_id;
	u32 engine_id;
	u32 runlist_id;
	u32 pbdma_id;
	u32 mmu_fault_id;
	u32 rc_mask;
	struct fifo_pbdma_exception_info_gk20a pbdma_exception_info;
	struct fifo_engine_exception_info_gk20a engine_exception_info;
	struct fifo_mmu_fault_info_gk20a mmu_fault_info;

};

struct fifo_gk20a {
	struct gk20a *g;
	int num_channels;

	int num_pbdma;
	u32 *pbdma_map;

	struct fifo_engine_info_gk20a *engine_info;
	u32 max_engines;
	u32 num_engines;

	struct fifo_runlist_info_gk20a *runlist_info;
	u32 max_runlists;

	struct userd_desc userd;
	u32 userd_entry_size;
	u32 userd_total_size;

	struct channel_gk20a *channel;
	struct mutex ch_inuse_mutex; /* protect unused chid look up */

	void (*remove_support)(struct fifo_gk20a *);
	bool sw_ready;
	struct {
		/* share info between isrs and non-isr code */
		struct {
			struct mutex mutex;
		} isr;
		struct {
			u32 device_fatal_0;
			u32 channel_fatal_0;
			u32 restartable_0;
		} pbdma;
		struct {

		} engine;


	} intr;

	u32 mmu_fault_engines;
	bool deferred_reset_pending;
	struct mutex deferred_reset_mutex;

	struct work_struct fault_restore_thread;
};

int gk20a_init_fifo_support(struct gk20a *g);

void gk20a_fifo_isr(struct gk20a *g);
void gk20a_fifo_nonstall_isr(struct gk20a *g);

int gk20a_fifo_preempt_channel(struct gk20a *g, u32 hw_chid);

int gk20a_fifo_enable_engine_activity(struct gk20a *g,
			struct fifo_engine_info_gk20a *eng_info);
int gk20a_fifo_disable_engine_activity(struct gk20a *g,
			struct fifo_engine_info_gk20a *eng_info,
			bool wait_for_idle);

int gk20a_fifo_update_runlist(struct gk20a *g, u32 engine_id, u32 hw_chid,
			      bool add, bool wait_for_finish);

int gk20a_fifo_suspend(struct gk20a *g);

bool gk20a_fifo_mmu_fault_pending(struct gk20a *g);
void gk20a_fifo_recover(struct gk20a *g, u32 engine_ids, bool verbose);
int gk20a_init_fifo_reset_enable_hw(struct gk20a *g);

void fifo_gk20a_finish_mmu_fault_handling(struct gk20a *g,
		unsigned long fault_id);
#endif /*__GR_GK20A_H__*/

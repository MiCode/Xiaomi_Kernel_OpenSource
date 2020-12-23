/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_PERFORMANCE_H
#define __MSM_PERFORMANCE_H

enum gfx_evt_t {
	MSM_PERF_INVAL,
	MSM_PERF_QUEUE,
	MSM_PERF_SUBMIT,
	MSM_PERF_RETIRED
};

enum evt_update_t {
	MSM_PERF_GFX,
};

#if IS_ENABLED(CONFIG_MSM_PERFORMANCE) && IS_ENABLED(CONFIG_MSM_PERFORMANCE_QGKI)
void msm_perf_events_update(enum evt_update_t update_typ,
			enum gfx_evt_t evt_typ, pid_t pid,
			uint32_t ctx_id, uint32_t timestamp, bool end_of_frame);
#else
static inline void msm_perf_events_update(enum evt_update_t update_typ,
			enum gfx_evt_t evt_typ, pid_t pid,
			uint32_t ctx_id, uint32_t timestamp, bool end_of_frame){}
#endif
#endif

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_thermal

#if !defined(_TRACE_MTK_THERMAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_THERMAL_H

#include <linux/tracepoint.h>
#if IS_ENABLED(CONFIG_MTK_MD_THERMAL)
#include "md_cooling.h"
#endif
#include "thermal_interface.h"
#include "thermal_trace_local.h"

#if IS_ENABLED(CONFIG_MTK_MD_THERMAL)
TRACE_DEFINE_ENUM(MD_LV_THROTTLE_DISABLED);
TRACE_DEFINE_ENUM(MD_SCG_OFF);
TRACE_DEFINE_ENUM(MD_LV_THROTTLE_ENABLED);
TRACE_DEFINE_ENUM(MD_IMS_ONLY);
TRACE_DEFINE_ENUM(MD_NO_IMS);
TRACE_DEFINE_ENUM(MD_OFF);

#define show_md_status(status)						\
	__print_symbolic(status,					\
		{ MD_LV_THROTTLE_DISABLED, "LV_THROTTLE_DISABLED"},	\
		{ MD_SCG_OFF,	"SCG_OFF"},				\
		{ MD_LV_THROTTLE_ENABLED,  "LV_THROTTLE_ENABLED"},	\
		{ MD_IMS_ONLY,  "IMS_ONLY"},				\
		{ MD_NO_IMS,    "NO_IMS"},				\
		{ MD_OFF,       "MD_OFF"})

TRACE_EVENT(md_mutt_limit,

	TP_PROTO(struct md_cooling_device *md_cdev, enum md_cooling_status status),

	TP_ARGS(md_cdev, status),

	TP_STRUCT__entry(
		__field(unsigned long, state)
		__field(unsigned int, id)
		__field(enum md_cooling_status, status)
	),

	TP_fast_assign(
		__entry->state = md_cdev->target_state;
		__entry->id = md_cdev->pa_id;
		__entry->status = status;
	),

	TP_printk("mutt_lv=%ld pa_id=%d status=%s",
		__entry->state, __entry->id, show_md_status(__entry->status))
);

TRACE_EVENT(md_tx_pwr_limit,

	TP_PROTO(struct md_cooling_device *md_cdev, enum md_cooling_status status),

	TP_ARGS(md_cdev, status),

	TP_STRUCT__entry(
		__field(unsigned int, state)
		__field(unsigned int, pwr)
		__field(unsigned int, id)
		__field(enum md_cooling_status, status)
	),

	TP_fast_assign(
		__entry->state = md_cdev->target_state;
		__entry->pwr =
			md_cdev->throttle_tx_power[md_cdev->target_state];
		__entry->id = md_cdev->pa_id;
		__entry->status = status;
	),

	TP_printk("tx_pwr_lv=%u tx_pwr=%u pa_id=%u status=%s",
		__entry->state, __entry->pwr, __entry->id,
		show_md_status(__entry->status))
);

TRACE_EVENT(md_scg_off,

	TP_PROTO(struct md_cooling_device *md_cdev, enum md_cooling_status status),

	TP_ARGS(md_cdev, status),

	TP_STRUCT__entry(
		__field(unsigned long, off)
		__field(unsigned int, id)
		__field(enum md_cooling_status, status)
	),

	TP_fast_assign(
		__entry->off = md_cdev->target_state;
		__entry->id = md_cdev->pa_id;
		__entry->status = status;
	),

	TP_printk("scg_off=%ld pa_id=%d status=%s",
		__entry->off, __entry->id, show_md_status(__entry->status))
);
#endif /* CONFIG_MTK_MD_THERMAL */

TRACE_EVENT(network_tput,
	TP_PROTO(unsigned int md_tput, unsigned int wifi_tput),

	TP_ARGS(md_tput, wifi_tput),

	TP_STRUCT__entry(
		__field(unsigned int, md_tput)
		__field(unsigned int, wifi_tput)
	),

	TP_fast_assign(
		__entry->md_tput = md_tput;
		__entry->wifi_tput = wifi_tput;
	),

	TP_printk("MD_Tput=%d Wifi_Tput=%d (Kb/s)",
		__entry->md_tput, __entry->wifi_tput)
);

TRACE_EVENT(thermal_cpu,

	TP_PROTO(struct thermal_cpu_info *cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(int, ttj)
		__field(int, limit_powerbudget)
		__field(int, LL_min_opp_hint)
		__field(unsigned int, LL_cur_freq)
		__field(unsigned int, LL_limit_freq)
		__field(int, LL_limit_opp)
		__field(int, LL_max_temp)
		__field(int, BL_min_opp_hint)
		__field(unsigned int, BL_cur_freq)
		__field(unsigned int, BL_limit_freq)
		__field(int, BL_limit_opp)
		__field(int, BL_max_temp)
		__field(int, B_min_opp_hint)
		__field(unsigned int, B_cur_freq)
		__field(unsigned int, B_limit_freq)
		__field(int, B_limit_opp)
		__field(int, B_max_temp)
	),

	TP_fast_assign(
		__entry->ttj = cpu->ttj;
		__entry->limit_powerbudget = cpu->limit_powerbudget;
		__entry->LL_min_opp_hint = cpu->LL_min_opp_hint;
		__entry->LL_cur_freq = cpu->LL_cur_freq;
		__entry->LL_limit_freq = cpu->LL_limit_freq;
		__entry->LL_limit_opp = cpu->LL_limit_opp;
		__entry->LL_max_temp = cpu->LL_max_temp;
		__entry->BL_min_opp_hint = cpu->BL_min_opp_hint;
		__entry->BL_cur_freq = cpu->BL_cur_freq;
		__entry->BL_limit_freq = cpu->BL_limit_freq;
		__entry->BL_limit_opp = cpu->BL_limit_opp;
		__entry->BL_max_temp = cpu->BL_max_temp;
		__entry->B_min_opp_hint = cpu->B_min_opp_hint;
		__entry->B_cur_freq = cpu->B_cur_freq;
		__entry->B_limit_freq = cpu->B_limit_freq;
		__entry->B_limit_opp = cpu->B_limit_opp;
		__entry->B_max_temp = cpu->B_max_temp;
	),

	TP_printk("ttj=%d limit_pb=%d LL_min_opp_h=%d LL_cur_freq=%d LL_limit_freq=%d LL_limit_opp=%d LL_max_t=%d BL_min_opp_h=%d BL_cur_freq=%d BL_limit_freq=%d BL_limit_opp=%d BL_max_t=%d B_min_opp_h=%d B_cur_freq=%d B_limit_freq=%d B_limit_opp=%d B_max_t=%d",
		__entry->ttj, __entry->limit_powerbudget,
		__entry->LL_min_opp_hint, __entry->LL_cur_freq, __entry->LL_limit_freq,
		__entry->LL_limit_opp, __entry->LL_max_temp,
		__entry->BL_min_opp_hint, __entry->BL_cur_freq, __entry->BL_limit_freq,
		__entry->BL_limit_opp, __entry->BL_max_temp,
		__entry->B_min_opp_hint, __entry->B_cur_freq, __entry->B_limit_freq,
		__entry->B_limit_opp, __entry->B_max_temp)
);

TRACE_EVENT(thermal_gpu,

	TP_PROTO(struct thermal_gpu_info *gpu),

	TP_ARGS(gpu),

	TP_STRUCT__entry(
		__field(int, ttj)
		__field(int, limit_powerbudget)
		__field(int, temp)
		__field(unsigned int, limit_freq)
		__field(unsigned int, cur_freq)
	),

	TP_fast_assign(
		__entry->ttj = gpu->ttj;
		__entry->limit_powerbudget = gpu->limit_powerbudget;
		__entry->temp = gpu->temp;
		__entry->limit_freq = gpu->limit_freq;
		__entry->cur_freq = gpu->cur_freq;
	),

	TP_printk("ttj=%d limit_pb=%d t=%d limit_freq=%d cur_freq=%d",
		__entry->ttj, __entry->limit_powerbudget, __entry->temp,
		__entry->limit_freq, __entry->cur_freq)
);

TRACE_EVENT(thermal_apu,

	TP_PROTO(struct thermal_apu_info *apu),

	TP_ARGS(apu),

	TP_STRUCT__entry(
		__field(int, ttj)
		__field(int, limit_powerbudget)
		__field(int, temp)
		__field(int, limit_opp)
		__field(int, cur_opp)
	),

	TP_fast_assign(
		__entry->ttj = apu->ttj;
		__entry->limit_powerbudget = apu->limit_powerbudget;
		__entry->temp = apu->temp;
		__entry->limit_opp = apu->limit_opp;
		__entry->cur_opp = apu->cur_opp;
	),

	TP_printk("ttj=%d limit_pb=%d t=%d limit_opp=%d cur_opp=%d",
		__entry->ttj, __entry->limit_powerbudget, __entry->temp, __entry->limit_opp, __entry->cur_opp)
);

TRACE_EVENT(cpu_hr_info_0,

	TP_PROTO(struct headroom_info *c0,
		struct headroom_info *c1,
		struct headroom_info *c2,
		struct headroom_info *c3),

	TP_ARGS(c0, c1, c2, c3),

	TP_STRUCT__entry(
		__field(int, cpu0_temp)
		__field(int, cpu0_pdt_temp)
		__field(int, cpu0_headroom)
		__field(int, cpu0_ratio)
		__field(int, cpu1_temp)
		__field(int, cpu1_pdt_temp)
		__field(int, cpu1_headroom)
		__field(int, cpu1_ratio)
		__field(int, cpu2_temp)
		__field(int, cpu2_pdt_temp)
		__field(int, cpu2_headroom)
		__field(int, cpu2_ratio)
		__field(int, cpu3_temp)
		__field(int, cpu3_pdt_temp)
		__field(int, cpu3_headroom)
		__field(int, cpu3_ratio)
	),

	TP_fast_assign(
		__entry->cpu0_temp = c0->temp;
		__entry->cpu0_pdt_temp = c0->predict_temp;
		__entry->cpu0_headroom = c0->headroom;
		__entry->cpu0_ratio = c0->ratio;
		__entry->cpu1_temp = c1->temp;
		__entry->cpu1_pdt_temp = c1->predict_temp;
		__entry->cpu1_headroom = c1->headroom;
		__entry->cpu1_ratio = c1->ratio;
		__entry->cpu2_temp = c2->temp;
		__entry->cpu2_pdt_temp = c2->predict_temp;
		__entry->cpu2_headroom = c2->headroom;
		__entry->cpu2_ratio = c2->ratio;
		__entry->cpu3_temp = c3->temp;
		__entry->cpu3_pdt_temp = c3->predict_temp;
		__entry->cpu3_headroom = c3->headroom;
		__entry->cpu3_ratio = c3->ratio;
	),

	TP_printk("0t=%d 0p=%d 0h=%d 0r=%d 1t=%d 1p=%d 1h=%d 1r=%d 2t=%d 2p=%d 2h=%d 2r=%d 3t=%d 3p=%d 3h=%d 3r=%d",
		__entry->cpu0_temp, __entry->cpu0_pdt_temp, __entry->cpu0_headroom, __entry->cpu0_ratio,
		__entry->cpu1_temp, __entry->cpu1_pdt_temp, __entry->cpu1_headroom, __entry->cpu1_ratio,
		__entry->cpu2_temp, __entry->cpu2_pdt_temp, __entry->cpu2_headroom, __entry->cpu2_ratio,
		__entry->cpu3_temp, __entry->cpu3_pdt_temp, __entry->cpu3_headroom, __entry->cpu3_ratio)
);

TRACE_EVENT(cpu_hr_info_1,

	TP_PROTO(struct headroom_info *c0,
		struct headroom_info *c1,
		struct headroom_info *c2,
		struct headroom_info *c3),

	TP_ARGS(c0, c1, c2, c3),

	TP_STRUCT__entry(
		__field(int, cpu0_temp)
		__field(int, cpu0_pdt_temp)
		__field(int, cpu0_headroom)
		__field(int, cpu0_ratio)
		__field(int, cpu1_temp)
		__field(int, cpu1_pdt_temp)
		__field(int, cpu1_headroom)
		__field(int, cpu1_ratio)
		__field(int, cpu2_temp)
		__field(int, cpu2_pdt_temp)
		__field(int, cpu2_headroom)
		__field(int, cpu2_ratio)
		__field(int, cpu3_temp)
		__field(int, cpu3_pdt_temp)
		__field(int, cpu3_headroom)
		__field(int, cpu3_ratio)
	),

	TP_fast_assign(
		__entry->cpu0_temp = c0->temp;
		__entry->cpu0_pdt_temp = c0->predict_temp;
		__entry->cpu0_headroom = c0->headroom;
		__entry->cpu0_ratio = c0->ratio;
		__entry->cpu1_temp = c1->temp;
		__entry->cpu1_pdt_temp = c1->predict_temp;
		__entry->cpu1_headroom = c1->headroom;
		__entry->cpu1_ratio = c1->ratio;
		__entry->cpu2_temp = c2->temp;
		__entry->cpu2_pdt_temp = c2->predict_temp;
		__entry->cpu2_headroom = c2->headroom;
		__entry->cpu2_ratio = c2->ratio;
		__entry->cpu3_temp = c3->temp;
		__entry->cpu3_pdt_temp = c3->predict_temp;
		__entry->cpu3_headroom = c3->headroom;
		__entry->cpu3_ratio = c3->ratio;
	),

	TP_printk("4t=%d 4p=%d 4h=%d 4r=%d 5t=%d 5p=%d 5h=%d 5r=%d 6t=%d 6p=%d 6h=%d 6r=%d 7t=%d 7p=%d 7h=%d 7r=%d",
		__entry->cpu0_temp, __entry->cpu0_pdt_temp, __entry->cpu0_headroom, __entry->cpu0_ratio,
		__entry->cpu1_temp, __entry->cpu1_pdt_temp, __entry->cpu1_headroom, __entry->cpu1_ratio,
		__entry->cpu2_temp, __entry->cpu2_pdt_temp, __entry->cpu2_headroom, __entry->cpu2_ratio,
		__entry->cpu3_temp, __entry->cpu3_pdt_temp, __entry->cpu3_headroom, __entry->cpu3_ratio)
);

TRACE_EVENT(frs,

	TP_PROTO(struct frs_info *frs),

	TP_ARGS(frs),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, target_fps)
		__field(int, diff)
		__field(int, tpcb)
		__field(int, tpcb_slope)
		__field(int, ap_headroom)
		__field(int, n_sec_to_ttpcb)
	),

	TP_fast_assign(
		__entry->pid = frs->pid;
		__entry->target_fps = frs->target_fps;
		__entry->diff = frs->diff;
		__entry->tpcb = frs->tpcb;
		__entry->tpcb_slope = frs->tpcb_slope;
		__entry->ap_headroom = frs->ap_headroom;
		__entry->n_sec_to_ttpcb = frs->n_sec_to_ttpcb;
	),

	TP_printk("pid=%d target_fps=%d diff=%d tpcb=%d tpcb_slope=%d ap_headroom=%d target_n=%d",
		__entry->pid, __entry->target_fps, __entry->diff, __entry->tpcb,
		__entry->tpcb_slope, __entry->ap_headroom, __entry->n_sec_to_ttpcb)
);

#if IS_ENABLED(CONFIG_MTK_THERMAL_JATM)
TRACE_EVENT(jatm_enable,

	TP_PROTO(int jatm_budget),

	TP_ARGS(jatm_budget),

	TP_STRUCT__entry(
		__field(int, jatm_budget)
	),

	TP_fast_assign(
		__entry->jatm_budget = jatm_budget;
	),

	TP_printk("jatm enabled and remaining budget=%d", __entry->jatm_budget)
);

TRACE_EVENT(jatm_disable,

	TP_PROTO(int reason, int frame_length, int real_usage, int jatm_budget),

	TP_ARGS(reason, frame_length, real_usage, jatm_budget),

	TP_STRUCT__entry(
		__field(int, reason)
		__field(int, frame_length)
		__field(int, real_usage)
		__field(int, jatm_budget)
	),

	TP_fast_assign(
		__entry->reason = reason;
		__entry->frame_length = frame_length;
		__entry->real_usage = real_usage;
		__entry->jatm_budget = jatm_budget;
	),

	TP_printk("stop reason=%d, frame_length=%d, real_usage=%d, remaining=%d",
		__entry->reason, __entry->frame_length, __entry->real_usage, __entry->jatm_budget)
);

TRACE_EVENT(not_start_reason,

	TP_PROTO(int reason),

	TP_ARGS(reason),

	TP_STRUCT__entry(
		__field(int, reason)
	),

	TP_fast_assign(
		__entry->reason = reason;
	),

	TP_printk("jatm not start reason=%d", __entry->reason)
);

TRACE_EVENT(try_enable_jatm,

	TP_PROTO(int elapsed, int delay),

	TP_ARGS(elapsed, delay),

	TP_STRUCT__entry(
		__field(int, elapsed)
		__field(int, delay)
	),

	TP_fast_assign(
		__entry->elapsed = elapsed;
		__entry->delay = delay;
	),

	TP_printk("Try to enable JATM after %d ms and delay start %d ms",
		__entry->elapsed, __entry->delay)
);

#endif


#endif /* _TRACE_MTK_THERMAL_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE thermal_trace
#include <trace/define_trace.h>

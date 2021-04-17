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

	TP_printk("tx_pwr_lv=%ld tx_pwr=%d pa_id=%d status=%s",
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

TRACE_EVENT(ap_ntc_hr,

	TP_PROTO(int temp, int predict_temp, int headroom, int ratio),

	TP_ARGS(temp, predict_temp, headroom, ratio),

	TP_STRUCT__entry(
		__field(int, temp)
		__field(int, predict_temp)
		__field(int, headroom)
		__field(int, ratio)
	),

	TP_fast_assign(
		__entry->temp = temp;
		__entry->predict_temp = predict_temp;
		__entry->headroom = headroom;
		__entry->ratio = ratio;
	),

	TP_printk("temp=%d predict_temp=%d headroom=%d ratio=%d",
		__entry->temp, __entry->predict_temp, __entry->headroom, __entry->ratio)
);

#endif /* _TRACE_MTK_THERMAL_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE thermal_trace
#include <trace/define_trace.h>

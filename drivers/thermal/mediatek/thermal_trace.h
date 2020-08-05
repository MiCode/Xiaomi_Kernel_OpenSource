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

#if IS_ENABLED(CONFIG_MTK_MD_THERMAL)
TRACE_DEFINE_ENUM(MD_LV_THROTTLE_DISABLED);
TRACE_DEFINE_ENUM(MD_LV_THROTTLE_ENABLED);
TRACE_DEFINE_ENUM(MD_IMS_ONLY);
TRACE_DEFINE_ENUM(MD_NO_IMS);
TRACE_DEFINE_ENUM(MD_OFF);

#define show_md_status(status)						\
	__print_symbolic(status,					\
		{ MD_LV_THROTTLE_DISABLED, "LV_THROTTLE_DISABLED"},	\
		{ MD_LV_THROTTLE_ENABLED,  "LV_THROTTLE_ENABLED"},	\
		{ MD_IMS_ONLY,  "IMS_ONLY"},				\
		{ MD_NO_IMS,    "NO_IMS"},				\
		{ MD_OFF,       "MD_OFF"})

TRACE_EVENT(md_mutt_limit,

	TP_PROTO(struct md_cooling_device *md_cdev, enum md_status status),

	TP_ARGS(md_cdev, status),

	TP_STRUCT__entry(
		__field(unsigned long, lv)
		__field(unsigned int, id)
		__field(enum md_status, status)
	),

	TP_fast_assign(
		__entry->lv = md_cdev->target_level;
		__entry->id = md_cdev->pa_id;
		__entry->status = status;
	),

	TP_printk("mutt_lv=%ld pa_id=%d status=%s",
		__entry->lv, __entry->id, show_md_status(__entry->status))
);

TRACE_EVENT(md_tx_pwr_limit,

	TP_PROTO(struct md_cooling_device *md_cdev, enum md_status status),

	TP_ARGS(md_cdev, status),

	TP_STRUCT__entry(
		__field(unsigned int, lv)
		__field(unsigned int, pwr)
		__field(unsigned int, id)
		__field(enum md_status, status)
	),

	TP_fast_assign(
		__entry->lv = md_cdev->target_level;
		__entry->pwr =
			md_cdev->throttle_tx_power[md_cdev->target_level];
		__entry->id = md_cdev->pa_id;
		__entry->status = status;
	),

	TP_printk("tx_pwr_lv=%ld tx_pwr=%d pa_id=%d status=%s",
		__entry->lv, __entry->pwr, __entry->id,
		show_md_status(__entry->status))
);

TRACE_EVENT(md_scg_off,

	TP_PROTO(struct md_cooling_device *md_cdev, enum md_status status),

	TP_ARGS(md_cdev, status),

	TP_STRUCT__entry(
		__field(unsigned long, off)
		__field(unsigned int, id)
		__field(enum md_status, status)
	),

	TP_fast_assign(
		__entry->off = md_cdev->target_level;
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
#endif /* _TRACE_MTK_THERMAL_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE thermal_trace
#include <trace/define_trace.h>

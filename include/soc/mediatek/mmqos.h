/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef MTK_MMQOS_H
#define MTK_MMQOS_H

enum hrt_type {
	HRT_MD,
	HRT_CAM,
	HRT_DISP,
	HRT_MML,
	HRT_TYPE_NUM,
	HRT_NONE = HRT_TYPE_NUM,
	HRT_MAX_BWL,
};

enum {
	BW_THROTTLE_START = 1,
	BW_THROTTLE_END
};

enum hrt_scen{
	CAM_SCEN_CHANGE,
	MD_SPEECH,
};

#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON)
void mtk_mmqos_wait_throttle_done(void);
s32 mtk_mmqos_set_hrt_bw(enum hrt_type type, u32 bw);
s32 mtk_mmqos_get_hrt_ratio(enum hrt_type type);
s32 mtk_mmqos_get_avail_hrt_bw(enum hrt_type type);
s32 mtk_mmqos_register_bw_throttle_notifier(struct notifier_block *nb);
s32 mtk_mmqos_unregister_bw_throttle_notifier(struct notifier_block *nb);
s32 mtk_mmqos_hrt_scen(enum hrt_scen, bool is_start);
s32 mtk_mmqos_system_qos_update(unsigned short qos_status);
void mtk_mmqos_set_md_type(u32 md_type);
#else
static inline void
mtk_mmqos_wait_throttle_done(void) { return; }

static inline s32
mtk_mmqos_set_hrt_bw(enum hrt_type type, u32 bw) { return 0; }

static inline s32
mtk_mmqos_get_hrt_ratio(enum hrt_type type) { return 0; }

static inline s32
mtk_mmqos_get_avail_hrt_bw(enum hrt_type type) { return -1; }

static inline s32
mtk_mmqos_register_bw_throttle_notifier(struct notifier_block *nb) { return 0; }

static inline s32
mtk_mmqos_unregister_bw_throttle_notifier(struct notifier_block *nb)
{ return 0; }

static inline s32
mtk_mmqos_hrt_scen(enum hrt_scen, bool is_start) { return 0; }

static inline s32
mtk_mmqos_system_qos_update(unsigned short qos_status) { return 0; }

static inline void
mtk_mmqos_set_md_type(u32 md_type) { return; }
#endif

#endif /* MTK_MMQOS_H */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_PBM_
#define _MTK_PBM_

enum pbm_kicker {
	KR_DLPT,
	KR_MD1,
	KR_MD3,
	KR_CPU,
	KR_GPU,
	KR_FLASH
};

enum PBM_PRIO_TAG {
	PBM_PRIO_CPU_B = 0,
	PBM_PRIO_CPU_L = 1,
	PBM_PRIO_GPU = 2,
	PBM_PRIO_MD = 3,
	PBM_PRIO_MD5 = 4,
	PBM_PRIO_FLASHLIGHT = 5,
	PBM_PRIO_VIDEO = 6,
	PBM_PRIO_WIFI = 7,
	PBM_PRIO_BACKLIGHT = 8,
	PBM_PRIO_DLPT = 9
};

extern void kicker_pbm_by_md(enum pbm_kicker kicker, bool status);
extern void kicker_pbm_by_flash(bool status);

extern void register_pbm_notify(void *oc_cb, enum PBM_PRIO_TAG prio_val);


#endif

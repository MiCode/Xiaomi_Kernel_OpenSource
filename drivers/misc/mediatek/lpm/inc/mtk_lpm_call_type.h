/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_CALL_TYPE_H__
#define __MTK_LPM_CALL_TYPE_H__


enum mtk_lpm_callee_spm_type {
	mtk_lpm_callee_spm_reg,
	mtk_lpm_callee_spm_wakesta,
};

enum mtk_lpm_callee_type {
	MTK_LPM_CALLEE_SSPM,
	MTK_LPM_CALLEE_SPM,
	/* Maybe this method use no longer*/
	MTK_LPM_CALLEE_EXTENSION,
};

#endif

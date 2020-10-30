/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_CALL_TYPE_H__
#define __LPM_CALL_TYPE_H__


enum lpm_callee_spm_type {
	lpm_callee_spm_reg,
	lpm_callee_spm_wakesta,
};

enum lpm_callee_type {
	LPM_CALLEE_SSPM,
	LPM_CALLEE_SPM,
	/* Maybe this method use no longer*/
	LPM_CALLEE_EXTENSION,
};

#endif

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#ifndef __EMI_BWL_H__
#define __EMI_BWL_H__

enum BWL_ENV {
	BWL_ENV_LPDDR3_1CH,
	BWL_ENV_LPDDR4_2CH,
	BWL_ENV_MAX
};

enum BWL_SCN {
	BWL_SCN_VPWFD,
	BWL_SCN_VR4K,
	BWL_SCN_ICFP,
	BWL_SCN_UI,
	BWL_SCN_MAX
};

enum BWL_CEN_REG {
	BWL_CEN_MAX
};

enum BWL_CHN_REG {
	BWL_CHN_MAX
};

#define SCN_DEFAULT	BWL_SCN_UI

#endif /*__EMI_BWL_H__*/

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_PQ_BIRSZ_FW_H__
#define __MTK_MML_PQ_BIRSZ_FW_H__

#include <linux/types.h>

struct birsz_fw_in {
	u32 in_width;
	u32 in_height;
	u32 out_width;
	u32 out_height;
};

struct birsz_fw_out {
	s32 vert_step;
	s32 vert_int_ofst;
	s32 vert_sub_ofst;
	s32 hori_step;
	s32 hori_int_ofst;
	s32 hori_sub_ofst;
	u32 precision;
};

/* birsz_fw - biRSZ firmware calculate biRSZ settings
 *
 * @in:		struct birsz_fw_in contains size information.
 * @out:	struct birsz_fw_out contains biRSZ setting results.
 */
void birsz_fw(struct birsz_fw_in *in, struct birsz_fw_out *out);

#endif	/* __MTK_MML_PQ_BIRSZ_FW_H__ */

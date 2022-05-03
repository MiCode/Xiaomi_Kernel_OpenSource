// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk-mml-pq-birsz-fw.h"
#include "mtk-mml-core.h"

#define BIRSZ_FW_UNIT 15

static s64 get_fix_tap_offset(u32 in, u32 out, u32 step, s32 bitnum)
{
	s64 offset;

	offset = ((s64)(in - 1) * (1 << bitnum) - (s64)(out - 1) * step);

	if (offset >= 0)
		offset = (offset + 1) / 2;
	else
		offset = -((-offset + 1) / 2);

	return offset;
}

static s32 get_fix_tap_step(u32 in, u32 out, s32 bitnum)
{
	s32 step;

	mml_msg("%s in_size = %d, bit_num = %d, out_size = %d",
		__func__, in, bitnum, out);
	step = (s32)(((s64)(in) * (1 << bitnum) + ((out) / 2)) / (out));

	return step;
}

void birsz_fw(struct birsz_fw_in *in, struct birsz_fw_out *out)
{
	s64 hori_ofst, vert_ofst;

	mml_msg("%s in_width %d in_height %d out_width %d out_height %d unit %d",
		__func__, in->in_width, in->in_height,
		in->out_width, in->out_height, BIRSZ_FW_UNIT);

	out->hori_step = get_fix_tap_step(in->in_width, in->out_width, BIRSZ_FW_UNIT);
	out->vert_step = get_fix_tap_step(in->in_height, in->out_height, BIRSZ_FW_UNIT);
	mml_msg("hori_step %#010x vert_step %#010x", out->hori_step, out->vert_step);

	hori_ofst = get_fix_tap_offset(
		in->in_width, in->out_width, out->hori_step, BIRSZ_FW_UNIT);
	vert_ofst = get_fix_tap_offset(
		in->in_height, in->out_height, out->vert_step, BIRSZ_FW_UNIT);

	if (hori_ofst >= 0) {
		out->hori_int_ofst = hori_ofst / (1 << BIRSZ_FW_UNIT);
		out->hori_sub_ofst = hori_ofst % (1 << BIRSZ_FW_UNIT);
	} else {
		out->hori_int_ofst =
			-(((-hori_ofst) + ((1 << BIRSZ_FW_UNIT) - 1)) / (1 << BIRSZ_FW_UNIT));
		out->hori_sub_ofst =
			(1 << BIRSZ_FW_UNIT) * (-out->hori_int_ofst) + hori_ofst;
	}
	mml_msg("hori_int_ofst %d hori_sub_ofst %d",
		out->hori_int_ofst, out->hori_sub_ofst);

	if (vert_ofst >= 0) {
		out->vert_int_ofst = vert_ofst / (1 << BIRSZ_FW_UNIT);
		out->vert_sub_ofst = vert_ofst % (1 << BIRSZ_FW_UNIT);
	} else {
		out->vert_int_ofst =
			-(((-vert_ofst) + ((1 << BIRSZ_FW_UNIT) - 1)) / (1 << BIRSZ_FW_UNIT));
		out->vert_sub_ofst =
			(1 << BIRSZ_FW_UNIT) * (-out->vert_int_ofst) + vert_ofst;
	}
	mml_msg("vert_int_ofst %d vert_sub_ofst %d",
		out->vert_int_ofst, out->vert_sub_ofst);
	out->precision = 1 << BIRSZ_FW_UNIT;
}

MODULE_DESCRIPTION("MTK MML PQ BIRSZ FW");
MODULE_AUTHOR("Chris-YC Chen<chris-yc.chen@mediatek.com>");
MODULE_LICENSE("GPL");

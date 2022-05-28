/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_PADS_H
#define __MTK_CAM_RAW_PADS_H

/* enum for pads of raw pipeline */
enum {
	MTK_RAW_SINK_BEGIN = 0,
	MTK_RAW_SINK = MTK_RAW_SINK_BEGIN,
	MTK_RAW_SINK_NUM,
	MTK_RAW_META_IN = MTK_RAW_SINK_NUM,
	MTK_RAW_RAWI_2_IN,
	MTK_RAW_RAWI_3_IN,
	MTK_RAW_RAWI_4_IN,
	MTK_RAW_SOURCE_BEGIN,
	MTK_RAW_MAIN_STREAM_OUT = MTK_RAW_SOURCE_BEGIN,
	MTK_RAW_YUVO_1_OUT,
	MTK_RAW_YUVO_2_OUT,
	MTK_RAW_YUVO_3_OUT,
	MTK_RAW_YUVO_4_OUT,
	MTK_RAW_YUVO_5_OUT,
	MTK_RAW_DRZS4NO_1_OUT,
	MTK_RAW_DRZS4NO_2_OUT,
	MTK_RAW_DRZS4NO_3_OUT,
	MTK_RAW_RZH1N2TO_1_OUT,
	MTK_RAW_RZH1N2TO_2_OUT,
	MTK_RAW_RZH1N2TO_3_OUT,
	MTK_RAW_MAIN_STREAM_SV_1_OUT,
	MTK_RAW_MAIN_STREAM_SV_2_OUT,
	MTK_RAW_META_OUT_BEGIN,
	MTK_RAW_META_OUT_0 = MTK_RAW_META_OUT_BEGIN,
	MTK_RAW_META_OUT_1,
	MTK_RAW_META_OUT_2,
	MTK_RAW_META_SV_OUT_0,
	MTK_RAW_META_SV_OUT_1,
	MTK_RAW_META_SV_OUT_2,
	MTK_RAW_PIPELINE_PADS_NUM,
};

static inline bool raw_is_sink_pad(unsigned int id)
{
	return id >= MTK_RAW_SINK && id < MTK_RAW_SOURCE_BEGIN;
}

static inline bool raw_is_valid_pad(unsigned int id)
{
	return id < MTK_RAW_PIPELINE_PADS_NUM;
}

static inline int raw_pad_to_node_idx(unsigned int id)
{
	WARN_ON(id < MTK_RAW_SINK_NUM);
	return id - MTK_RAW_SINK_NUM;
}

#define _raw_pad_name(PAD)	\
	[MTK_RAW_ ## PAD] = #PAD

static inline const char *raw_get_pad_name(unsigned int id)
{
	static const char * const names[] = {
		_raw_pad_name(SINK),
		_raw_pad_name(META_IN),
		_raw_pad_name(RAWI_2_IN),
		_raw_pad_name(RAWI_3_IN),
		_raw_pad_name(RAWI_4_IN),
		_raw_pad_name(MAIN_STREAM_OUT),
		_raw_pad_name(YUVO_1_OUT),
		_raw_pad_name(YUVO_2_OUT),
		_raw_pad_name(YUVO_3_OUT),
		_raw_pad_name(YUVO_4_OUT),
		_raw_pad_name(YUVO_5_OUT),
		_raw_pad_name(DRZS4NO_1_OUT),
		_raw_pad_name(DRZS4NO_2_OUT),
		_raw_pad_name(DRZS4NO_3_OUT),
		_raw_pad_name(RZH1N2TO_1_OUT),
		_raw_pad_name(RZH1N2TO_2_OUT),
		_raw_pad_name(RZH1N2TO_3_OUT),
		_raw_pad_name(MAIN_STREAM_SV_1_OUT),
		_raw_pad_name(MAIN_STREAM_SV_2_OUT),
		_raw_pad_name(META_OUT_0),
		_raw_pad_name(META_OUT_1),
		_raw_pad_name(META_OUT_2),
		_raw_pad_name(META_SV_OUT_0),
		_raw_pad_name(META_SV_OUT_1),
		_raw_pad_name(META_SV_OUT_2),
	};

	if (id >= ARRAY_SIZE(names))
		return "not-found";
	return names[id];
}

#endif /*__MTK_CAM_RAW_PADS_H*/

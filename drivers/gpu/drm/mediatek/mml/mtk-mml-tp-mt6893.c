/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <dt-bindings/mml/mml-mt6893.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"

#define TOPOLOGY_PLATFORM	"mt6893"
#define MML_DUAL_FRAME		(3840 * 2160)
#define AAL_MIN_WIDTH		50	/* TODO: define in tile? */
#define MML_IR_MIN_FRAME	(2560 * 1440)
#define MML_IR_MAX_FRAME	(3840 * 2176)
#define MML_IR_MAX_WIDTH	3200

int mml_force_rsz;
module_param(mml_force_rsz, int, 0644);

enum topology_dual {
	MML_DUAL_NORMAL,
	MML_DUAL_DISABLE,
	MML_DUAL_ALWAYS,
};

int mml_dual;
module_param(mml_dual, int, 0644);

enum topology_scenario {
	PATH_MML_NOPQ_P0 = 0,
	PATH_MML_NOPQ_P1,
	PATH_MML_PQ_P0,
	PATH_MML_PQ_P1,
	PATH_MML_PQ2_P0,
	PATH_MML_PQ2_P1,
	PATH_MML_2OUT_P0,
	PATH_MML_2OUT_P1,
	PATH_MML_MAX
};

struct path_node {
	u8 eng;
	u8 next0;
	u8 next1;
};

/* check if engine is output dma engine */
static inline bool engine_wrot(u32 id)
{
	return id == MML_WROT0 || id == MML_WROT1;
}

static const struct path_node path_map[PATH_MML_MAX][MML_MAX_PATH_NODES] = {
	[PATH_MML_NOPQ_P0] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_NOPQ_P1] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA1, MML_WROT1,},
		{MML_WROT1,},
	},

	[PATH_MML_PQ_P0] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_FG0,},
		{MML_FG0, MML_PQ0_SOUT,},
		{MML_PQ0_SOUT, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_TCC0,},
		{MML_TCC0, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_PQ_P1] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA1, MML_FG1,},
		{MML_FG1, MML_PQ1_SOUT,},
		{MML_PQ1_SOUT, MML_RSZ1,},
		{MML_RSZ1, MML_TDSHP1,},
		{MML_TDSHP1, MML_COLOR1,},
		{MML_COLOR1, MML_TCC1,},
		{MML_TCC1, MML_WROT1,},
		{MML_WROT1,},
	},

	[PATH_MML_PQ2_P0] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_FG0,},
		{MML_FG0, MML_PQ0_SOUT,},
		{MML_PQ0_SOUT, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_TCC0,},
		{MML_TCC0, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_PQ2_P1] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA1, MML_FG1,},
		{MML_FG1, MML_PQ1_SOUT,},
		{MML_PQ1_SOUT, MML_HDR1,},
		{MML_HDR1, MML_AAL1,},
		{MML_AAL1, MML_RSZ1,},
		{MML_RSZ1, MML_TDSHP1,},
		{MML_TDSHP1, MML_COLOR1,},
		{MML_COLOR1, MML_TCC1,},
		{MML_TCC1, MML_WROT1,},
		{MML_WROT1,},
	},
	[PATH_MML_2OUT_P0] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_FG0,},
		{MML_FG0, MML_PQ0_SOUT,},
		{MML_PQ0_SOUT, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_RSZ0, MML_RSZ2},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_RSZ2, MML_TDSHP2,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_TDSHP2, MML_TCC2,},
		{MML_COLOR0, MML_TCC0,},
		{MML_TCC0, MML_WROT0,},
		{MML_TCC2, MML_WROT2,},
		{MML_WROT0,},
		{MML_WROT2,},
	},
	[PATH_MML_2OUT_P1] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA1, MML_FG1,},
		{MML_FG1, MML_PQ1_SOUT,},
		{MML_PQ1_SOUT, MML_HDR1,},
		{MML_HDR1, MML_AAL1,},
		{MML_AAL1, MML_RSZ1, MML_RSZ3},
		{MML_RSZ1, MML_TDSHP1,},
		{MML_RSZ3, MML_TDSHP3,},
		{MML_TDSHP1, MML_COLOR1,},
		{MML_TDSHP3, MML_TCC3,},
		{MML_COLOR1, MML_TCC1,},
		{MML_TCC1, MML_WROT1,},
		{MML_TCC3, MML_WROT3,},
		{MML_WROT1,},
		{MML_WROT3,},
	},
};

enum cmdq_clt_usage {
	MML_CLT_PIPE0,
	MML_CLT_PIPE1,
	MML_CLT_MAX
};

static const u8 clt_dispatch[PATH_MML_MAX] = {
	[PATH_MML_NOPQ_P0] = MML_CLT_PIPE0,
	[PATH_MML_NOPQ_P1] = MML_CLT_PIPE1,
	[PATH_MML_PQ_P0] = MML_CLT_PIPE0,
	[PATH_MML_PQ_P1] = MML_CLT_PIPE1,
	[PATH_MML_PQ2_P0] = MML_CLT_PIPE0,
	[PATH_MML_PQ2_P1] = MML_CLT_PIPE1,
	[PATH_MML_2OUT_P0] = MML_CLT_PIPE0,
	[PATH_MML_2OUT_P1] = MML_CLT_PIPE1,
};

/* reset bit to each engine,
 * reverse of MMSYS_SW0_RST_B_REG and MMSYS_SW1_RST_B_REG
 */
static u8 engine_reset_bit[MML_ENGINE_TOTAL] = {
	[MML_RDMA0] = 0,
	[MML_FG0] = 1,
	[MML_HDR0] = 2,
	[MML_AAL0] = 3,
	[MML_RSZ0] = 4,
	[MML_TDSHP0] = 5,
	[MML_TCC0] = 6,
	[MML_WROT0] = 7,
	[MML_RDMA2] = 8,
	[MML_AAL2] = 9,
	[MML_RSZ2] = 10,
	[MML_COLOR0] = 11,
	[MML_TDSHP2] = 12,
	[MML_TCC2] = 13,
	[MML_WROT2] = 14,
	[MML_RDMA1] = 16,
	[MML_FG1] = 17,
	[MML_HDR1] = 18,
	[MML_AAL1] = 19,
	[MML_RSZ1] = 20,
	[MML_TDSHP1] = 21,
	[MML_TCC1] = 22,
	[MML_WROT1] = 23,
	[MML_RDMA3] = 24,
	[MML_AAL3] = 25,
	[MML_RSZ3] = 26,
	[MML_COLOR1] = 27,
	[MML_TDSHP3] = 28,
	[MML_TCC3] = 29,
	[MML_WROT3] = 30,
	[MML_CAMIN] = 37,
	[MML_CAMIN2] = 38,
	[MML_CAMIN3] = 39,
	[MML_CAMIN4] = 41,
};

static void tp_dump_path(struct mml_topology_path *path)
{
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		mml_log(
			"[topology]engine %hhu (%p) prev %p next %p %p comp %p tile idx %hhu out %hhu",
			path->nodes[i].id, &path->nodes[i],
			path->nodes[i].prev,
			path->nodes[i].next[0], path->nodes[i].next[1],
			path->nodes[i].comp,
			path->nodes[i].tile_eng_idx,
			path->nodes[i].out_idx);
	}
}

static void tp_dump_path_short(struct mml_topology_path *path)
{
	char path_desc[64];
	u32 len = 0;
	u8 i;

	for (i = 0; i < path->node_cnt; i++)
		len += snprintf(path_desc + len, sizeof(path_desc) - len, " %hhu",
			path->nodes[i].id);
	mml_log("[topology]engines:%s", path_desc);
}

static void tp_parse_path(struct mml_dev *mml, struct mml_topology_path *path,
	const struct path_node *route)
{
	struct mml_path_node *prev[2] = {0};
	u8 connect_eng[2] = {0};
	u8 i, tile_idx, out_eng_idx;

	for (i = 0; i < MML_MAX_PATH_NODES; i++) {
		const u8 eng = route[i].eng;
		const u8 next0 = route[i].next0;
		const u8 next1 = route[i].next1;

		if (!eng) {
			path->node_cnt = i;
			break;
		}

		/* assign current engine */
		path->nodes[i].id = eng;
		path->nodes[i].comp = mml_dev_get_comp_by_id(mml, eng);
		if (!path->nodes[i].comp)
			mml_err("[topology]no comp idx:%hhu engine:%hhu", i, eng);


		/* assign reset bits for this path */
		path->reset_bits |= 1LL << engine_reset_bit[eng];

		if (eng == MML_MMLSYS) {
			path->mmlsys = path->nodes[i].comp;
			path->mmlsys_idx = i;
			continue;
		} else if (eng == MML_MUTEX) {
			path->mutex = path->nodes[i].comp;
			path->mutex_idx = i;
			continue;
		}

		/* check cursor for 2 out and link if id match */
		if (!connect_eng[0] && next0) {
			/* First engine case, set current out 0 to this engine.
			 * And config next engines
			 */
			prev[0] = &path->nodes[i];
			connect_eng[0] = next0;
			path->nodes[i].out_idx = 0;
		} else if (connect_eng[0] == eng) {
			/* connect out 0 */
			prev[0]->next[0] = &path->nodes[i];
			/* replace current out 0 to this engine */
			path->nodes[i].prev = prev[0];
			prev[0] = &path->nodes[i];
			connect_eng[0] = next0;
			path->nodes[i].out_idx = 0;

			/* also assign 1 in 2 out case,
			 * must branch from line 0
			 */
			if (!connect_eng[1] && next1) {
				prev[1] = &path->nodes[i];
				connect_eng[1] = next1;
			}
		} else if (connect_eng[1] == eng) {
			/* connect out 1 */
			if (!prev[1]->next[0])
				prev[1]->next[0] = &path->nodes[i];
			else
				prev[1]->next[1] = &path->nodes[i];
			/* replace current out 1 to this engine */
			path->nodes[i].prev = prev[1];
			prev[1] = &path->nodes[i];
			connect_eng[1] = next0;
			path->nodes[i].out_idx = 1;

			/* at most 2 out in one path,
			 * cannot branch from line 1
			 */
			if (next1)
				mml_err("[topology]%s wrong path index %hhu engine %hhu",
					__func__, i, eng);
		} else {
			mml_err("[topology]connect fail idx:%hhu engine:%hhu"
				" next0:%hhu next1:%hhu from:%hhu %hhu",
				i, eng, next0, next1,
				connect_eng[0], connect_eng[1]);
		}
	}
	path->node_cnt = i;

	/* 0: reset
	 * 1: not reset
	 * so we need to reverse the bits
	 */
	path->reset_bits = ~path->reset_bits;
	mml_msg("[topology]reset bits %#llx", path->reset_bits);

	/* collect tile engines */
	tile_idx = 0;
	for (i = 0; i < path->node_cnt; i++) {
		if (!path->nodes[i].prev && !path->nodes[i].next[0]) {
			path->nodes[i].tile_eng_idx = ~0;
			continue;
		}
		path->nodes[i].tile_eng_idx = tile_idx;
		path->tile_engines[tile_idx++] = i;
	}
	path->tile_engine_cnt = tile_idx;

	/* scan out engines */
	out_eng_idx = 0;
	for (i = 0; i < path->node_cnt && out_eng_idx < MML_MAX_OUTPUTS; i++) {
		if (!engine_wrot(path->nodes[i].id))
			continue;
		path->out_engine_ids[out_eng_idx++] = path->nodes[i].id;
	}

	if (path->tile_engine_cnt == 2)
		path->alpharot = true;
}

static s32 tp_init_cache(struct mml_dev *mml, struct mml_topology_cache *cache,
	struct cmdq_client **clts, u32 clt_cnt)
{
	u32 i;

	if (clt_cnt < MML_CLT_MAX) {
		mml_err("[topology]%s not enough cmdq clients to all path", __func__);
		return -ECHILD;
	}

	for (i = 0; i < PATH_MML_MAX; i++) {
		tp_parse_path(mml, &cache->paths[i], path_map[i]);
		if (mtk_mml_msg) {
			mml_log("[topology]dump path %hhu count %u",
				i, cache->paths[i].node_cnt);
			tp_dump_path(&cache->paths[i]);
		}

		/* now dispatch cmdq client (channel) to path */
		cache->paths[i].clt_id = clt_dispatch[i];
		cache->paths[i].clt = clts[clt_dispatch[i]];
	}

	return 0;
}

static inline bool tp_need_resize(struct mml_frame_info *info)
{
	u32 w = info->dest[0].data.width;
	u32 h = info->dest[0].data.height;

	if (info->dest[0].rotate == MML_ROT_90 ||
		info->dest[0].rotate == MML_ROT_270)
		swap(w, h);

	mml_msg("[topology]%s target %ux%u crop %ux%u",
		__func__, w, h,
		info->dest[0].crop.r.width,
		info->dest[0].crop.r.height);

	return info->dest_cnt != 1 ||
		info->dest[0].crop.r.width != w ||
		info->dest[0].crop.r.height != h ||
		info->dest[0].crop.x_sub_px ||
		info->dest[0].crop.y_sub_px ||
		info->dest[0].crop.w_sub_px ||
		info->dest[0].crop.h_sub_px;
}

static void tp_select_path(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg,
	struct mml_topology_path **path)
{
	enum topology_scenario scene[2] = {0};
	bool en_rsz;

	if (cfg->info.mode == MML_MODE_RACING) {
		/* always rdma to wrot for racing case */
		scene[0] = PATH_MML_NOPQ_P0;
		scene[1] = PATH_MML_NOPQ_P1;
		goto done;
	}

	en_rsz = tp_need_resize(&cfg->info);
	if (mml_force_rsz)
		en_rsz = true;

	if (!en_rsz && !cfg->info.dest[0].pq_config.en) {
		/* dual pipe, rdma0 to wrot0 / rdma1 to wrot1 */
		scene[0] = PATH_MML_NOPQ_P0;
		scene[1] = PATH_MML_NOPQ_P1;
	} else if ((en_rsz || cfg->info.dest[0].pq_config.en) &&
		   cfg->info.dest_cnt == 1) {
		/* 1 in 1 out with PQs */
		if (cfg->info.dest[0].pq_config.en_dre ||
			cfg->info.dest[0].pq_config.en_hdr) {
			/* and with HDR/AAL */
			scene[0] = PATH_MML_PQ2_P0;
			scene[1] = PATH_MML_PQ2_P1;
		} else {
			scene[0] = PATH_MML_PQ_P0;
			scene[1] = PATH_MML_PQ_P1;
		}
	} else if (cfg->info.dest_cnt == 2) {
		scene[0] = PATH_MML_2OUT_P0;
		scene[1] = PATH_MML_2OUT_P1;
	}

done:
	path[0] = &cache->paths[scene[0]];
	path[1] = &cache->paths[scene[1]];
}

static bool tp_need_dual(struct mml_frame_config *cfg)
{
	const struct mml_frame_data *src = &cfg->info.src;
	u32 min_crop_w, i;
	u32 min_pixel = cfg->info.mode == MML_MODE_RACING ?
		MML_IR_MIN_FRAME : MML_DUAL_FRAME;

	if (src->width * src->height < min_pixel)
		return false;

	min_crop_w = cfg->info.dest[0].crop.r.width;
	for (i = 1; i < cfg->info.dest_cnt; i++)
		min_crop_w = min(min_crop_w, cfg->info.dest[i].crop.r.width);

	if (min_crop_w <= AAL_MIN_WIDTH)
		return false;

	return true;
}

static s32 tp_select(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg)
{
	struct mml_topology_path *path[2] = {0};

	if (mml_dual == MML_DUAL_NORMAL)
		cfg->dual = tp_need_dual(cfg);
	else if (mml_dual == MML_DUAL_DISABLE)
		cfg->dual = false;
	else if (mml_dual == MML_DUAL_ALWAYS)
		cfg->dual = true;
	else
		cfg->dual = tp_need_dual(cfg);

	tp_select_path(cache, cfg, path);

	if (!path[0])
		return -EPERM;

	cfg->path[0] = path[0];
	cfg->path[1] = path[1];

	if (path[0]->alpharot) {
		u8 fmt_in = MML_FMT_HW_FORMAT(cfg->info.src.format);
		u8 i;

		cfg->alpharot = MML_FMT_IS_ARGB(fmt_in);
		for (i = 0; i < cfg->info.dest_cnt && cfg->alpharot; i++)
			if (!MML_FMT_IS_ARGB(cfg->info.dest[i].data.format))
				cfg->alpharot = false;
	}

	tp_dump_path_short(path[0]);
	if (cfg->dual && path[1])
		tp_dump_path_short(path[1]);

	return 0;
}

static enum mml_mode tp_query_mode(struct mml_dev *mml, struct mml_frame_info *info)
{
	return MML_MODE_MML_DECOUPLE;
}

static struct cmdq_client *get_racing_clt(struct mml_topology_cache *cache, u32 pipe)
{
	/* not support inline rot in this platform */
	return NULL;
}

static const struct mml_topology_ops tp_ops_mt6893 = {
	.query_mode = tp_query_mode,
	.init_cache = tp_init_cache,
	.select = tp_select,
	.get_racing_clt = get_racing_clt,
};

static __init int mml_topology_ip_init(void)
{
	return mml_topology_register_ip(TOPOLOGY_PLATFORM, &tp_ops_mt6893);
}
module_init(mml_topology_ip_init);

static __exit void mml_topology_ip_exit(void)
{
	mml_topology_unregister_ip(TOPOLOGY_PLATFORM);
}
module_exit(mml_topology_ip_exit);

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML for MT6893");
MODULE_LICENSE("GPL v2");

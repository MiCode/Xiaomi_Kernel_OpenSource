// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <dt-bindings/mml/mml-mt6886.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"

#define TOPOLOGY_PLATFORM	"mt6886"
#define AAL_MIN_WIDTH		50	/* TODO: define in tile? */

/* fhd size and pixel as upper bound */
#define MML_IR_WIDTH_UPPER	(1920 + 30)
#define MML_IR_HEIGHT_UPPER	(1088 + 30)
#define MML_IR_MAX		(MML_IR_WIDTH_UPPER * MML_IR_HEIGHT_UPPER)
/* fhd size and pixel as lower bound */
#define MML_IR_WIDTH_LOWER	(1920 - 30)
#define MML_IR_HEIGHT_LOWER	(1088 - 30)
#define MML_IR_MIN		(MML_IR_WIDTH_LOWER * MML_IR_HEIGHT_LOWER)
#define MML_IR_RSZ_MIN_RATIO	375	/* resize must lower than this ratio */

#define MML_IR_MAX_OPP		1	/* use OPP index 0(229Mhz) 1(273Mhz) */

int mml_force_rsz;
module_param(mml_force_rsz, int, 0644);

int mml_dual;
module_param(mml_dual, int, 0644);

int mml_path_swap;
module_param(mml_path_swap, int, 0644);

int mml_path_mode;
module_param(mml_path_mode, int, 0644);

/* debug param
 * 0: (default)don't care, check dts property to enable racing
 * 1: force enable
 * 2: force disable
 */
int mml_racing;
module_param(mml_racing, int, 0644);

int mml_racing_rsz = 1;
module_param(mml_racing_rsz, int, 0644);

enum topology_scenario {
	PATH_MML_NOPQ_P0 = 0,
	PATH_MML_PQ_P0,
	PATH_MML_PQ_DD0,
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
	return id == MML_WROT0;
}

static const struct path_node path_map[PATH_MML_MAX][MML_MAX_PATH_NODES] = {
	[PATH_MML_NOPQ_P0] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_PQ_P0] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_DLO0_SOUT,},
		{MML_DLO0_SOUT, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_PQ_DD0] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_DLI0, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_DLO0_SOUT,},
		{MML_DLO0_SOUT, MML_DLO0,},
		{MML_DLO0,},
	},
};

enum cmdq_clt_usage {
	MML_CLT_PIPE0,
	MML_CLT_MAX
};

static const u8 clt_dispatch[PATH_MML_MAX] = {
	[PATH_MML_NOPQ_P0] = MML_CLT_PIPE0,
	[PATH_MML_PQ_P0] = MML_CLT_PIPE0,
	[PATH_MML_PQ_DD0] = MML_CLT_PIPE0,
};

/* mux sof group of mmlsys mout/sel */
enum mux_sof_group {
	MUX_SOF_GRP0 = 0,
	MUX_SOF_GRP1,
	MUX_SOF_GRP2,
	MUX_SOF_GRP3,
	MUX_SOF_GRP4,
	MUX_SOF_GRP5,
	MUX_SOF_GRP6,
	MUX_SOF_GRP7,
};

static const u8 grp_dispatch[PATH_MML_MAX] = {
	[PATH_MML_NOPQ_P0] = MUX_SOF_GRP1,
	[PATH_MML_PQ_P0] = MUX_SOF_GRP1,
	[PATH_MML_PQ_DD0] = MUX_SOF_GRP3,
};

/* reset bit to each engine,
 * reverse of MMSYS_SW0_RST_B_REG and MMSYS_SW1_RST_B_REG
 */
static u8 engine_reset_bit[MML_ENGINE_TOTAL] = {
	[MML_RDMA0] = 3,
	[MML_HDR0] = 5,
	[MML_AAL0] = 6,
	[MML_RSZ0] = 7,
	[MML_TDSHP0] = 8,
	[MML_COLOR0] = 9,
	[MML_WROT0] = 10,
	[MML_DLI0] = 12,
	[MML_DLO0] = 26,
};

static void tp_dump_path(const struct mml_topology_path *path)
{
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		mml_log(
			"[topology]engine %u (%p) prev %p %p next %p %p comp %p tile idx %u out %u",
			path->nodes[i].id, &path->nodes[i],
			path->nodes[i].prev[0], path->nodes[i].prev[1],
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
		len += snprintf(path_desc + len, sizeof(path_desc) - len, " %u",
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
			mml_err("[topology]no comp idx:%u engine:%u", i, eng);

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
		path->hw_pipe = 0;

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
			path->nodes[i].prev[0] = prev[0];
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
			path->nodes[i].prev[0] = prev[1];
			prev[1] = &path->nodes[i];
			connect_eng[1] = next0;
			path->nodes[i].out_idx = 1;

			/* at most 2 out in one path,
			 * cannot branch from line 1
			 */
			if (next1)
				mml_err("[topology]%s wrong path index %u engine %u",
					__func__, i, eng);
		} else {
			mml_err("[topology]connect fail idx:%u engine:%u next0:%u next1:%u from:%u %u",
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
		if (!path->nodes[i].prev[0] && !path->nodes[i].next[0]) {
			path->nodes[i].tile_eng_idx = ~0;
			continue;
		}
		path->nodes[i].tile_eng_idx = tile_idx;
		path->tile_engines[tile_idx++] = i;
	}
	path->tile_engine_cnt = tile_idx;

	/* scan out engines */
	for (i = 0; i < path->node_cnt; i++) {
		if (!engine_wrot(path->nodes[i].id))
			continue;
		out_eng_idx = path->nodes[i].out_idx;
		if (path->out_engine_ids[out_eng_idx])
			mml_err("[topology]multiple out engines: was %u now %u on out idx:%u",
				path->out_engine_ids[out_eng_idx],
				path->nodes[i].id, out_eng_idx);
		path->out_engine_ids[out_eng_idx] = path->nodes[i].id;
	}

	if (path->tile_engine_cnt == 2)
		path->alpharot = true;
}

static s32 tp_init_cache(struct mml_dev *mml, struct mml_topology_cache *cache,
	struct cmdq_client **clts, u32 clt_cnt)
{
	u32 i;

	if (clt_cnt < MML_CLT_MAX) {
		mml_err("[topology]%s not enough cmdq clients to all paths",
			__func__);
		return -ECHILD;
	}
	if (ARRAY_SIZE(cache->paths) < PATH_MML_MAX) {
		mml_err("[topology]%s not enough path cache for all paths",
			__func__);
		return -ECHILD;
	}

	for (i = 0; i < PATH_MML_MAX; i++) {
		struct mml_topology_path *path = &cache->paths[i];

		tp_parse_path(mml, path, path_map[i]);
		if (mtk_mml_msg) {
			mml_log("[topology]dump path %u count %u clt id %u",
				i, path->node_cnt, clt_dispatch[i]);
			tp_dump_path(path);
		}

		/* now dispatch cmdq client (channel) to path */
		path->clt = clts[clt_dispatch[i]];
		path->clt_id = clt_dispatch[i];
		path->mux_group = grp_dispatch[i];
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
		info->dest[0].crop.h_sub_px ||
		info->dest[0].compose.width != info->dest[0].data.width ||
		info->dest[0].compose.height != info->dest[0].data.height;
}

static void tp_select_path(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg,
	struct mml_topology_path **path)
{
	enum topology_scenario scene[2] = {0};
	bool en_rsz, en_pq;

	if (cfg->info.mode == MML_MODE_RACING) {
		/* always rdma to wrot for racing case */
		scene[0] = PATH_MML_NOPQ_P0;
		scene[1] = PATH_MML_NOPQ_P0;
		goto done;
	}
	en_rsz = tp_need_resize(&cfg->info);
	if (mml_force_rsz)
		en_rsz = true;
	en_pq = en_rsz || cfg->info.dest[0].pq_config.en;

	if (cfg->info.mode == MML_MODE_DDP_ADDON) {
		scene[0] = PATH_MML_PQ_DD0;
		scene[1] = PATH_MML_PQ_DD0;
	} else if (!en_pq) {
		/* rdma0 to wrot0 */
		scene[0] = PATH_MML_NOPQ_P0;
		scene[1] = PATH_MML_NOPQ_P0;
	} else {
		/* 1 in 1 out with PQs */
		scene[0] = PATH_MML_PQ_P0;
		scene[1] = PATH_MML_PQ_P0;
	}

done:
	path[0] = &cache->paths[scene[0]];
	path[1] = &cache->paths[scene[1]];
}

static s32 tp_select(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg)
{
	struct mml_topology_path *path[2] = {0};

	cfg->dual = false;
	if (cfg->info.mode == MML_MODE_DDP_ADDON) {
		cfg->dual = cfg->disp_dual;
		cfg->framemode = true;
		cfg->nocmd = true;
	}

	cfg->shadow = false;

	mml_msg("[topology]%s info->mode %d, dual %d, framemode %d, nocmd %d, shadow %d",
		__func__,
		cfg->info.mode, cfg->dual, cfg->framemode, cfg->nocmd, cfg->shadow);

	tp_select_path(cache, cfg, path);

	if (!path[0])
		return -EPERM;

	cfg->path[0] = path[0];
	cfg->path[1] = path[1];

	if (path[0]->alpharot) {
		u32 i;

		cfg->alpharot = MML_FMT_IS_ARGB(cfg->info.src.format);
		for (i = 0; i < cfg->info.dest_cnt && cfg->alpharot; i++)
			if (!MML_FMT_IS_ARGB(cfg->info.dest[i].data.format))
				cfg->alpharot = false;
	}

	tp_dump_path_short(path[0]);

	return 0;
}

static enum mml_mode tp_query_mode(struct mml_dev *mml, struct mml_frame_info *info,
	u32 *reason)
{
	struct mml_topology_cache *tp;
	u32 pixel;
	u32 freq;

	mml_msg("[topology]%s mml_path_mode %d, info->mode %d, mml_racing %d %d, secure %d %d",
		__func__,
		mml_path_mode, info->mode,
		mml_racing, mml_racing_enable(mml),
		info->src.secure, info->dest[0].data.secure);

	mml_msg("[topology]%s dc %d, color %d, hdr %d, ccorr %d, dre %d, region_pq %d, en_sharp %d",
		__func__,
		info->dest[0].pq_config.en_dc,
		info->dest[0].pq_config.en_color,
		info->dest[0].pq_config.en_hdr,
		info->dest[0].pq_config.en_ccorr,
		info->dest[0].pq_config.en_dre,
		info->dest[0].pq_config.en_region_pq,
		info->dest[0].pq_config.en_sharp);

	if (unlikely(mml_path_mode))
		return mml_path_mode;

	if (unlikely(mml_racing)) {
		if (mml_racing == 2)
			goto decouple;
	} else if (!mml_racing_enable(mml))
		goto decouple;

	/* skip all racing mode check if use prefer dc */
	if (info->mode == MML_MODE_MML_DECOUPLE ||
		info->mode == MML_MODE_MDP_DECOUPLE)
		goto decouple_user;

	/* TODO: should REMOVE after inlinerot resize ready */
	if (unlikely(!mml_racing_rsz) && tp_need_resize(info))
		goto decouple;

	/* secure content cannot output to sram */
	if (info->src.secure || info->dest[0].data.secure)
		goto decouple;

	/* no pq support for racing mode */
	if (info->dest[0].pq_config.en_dc ||
		info->dest[0].pq_config.en_color ||
		info->dest[0].pq_config.en_hdr ||
		info->dest[0].pq_config.en_ccorr ||
		info->dest[0].pq_config.en_dre ||
		info->dest[0].pq_config.en_region_pq)
		goto decouple;

	/* racing only support 1 out */
	if (info->dest_cnt > 1)
		goto decouple;

	/* rgb should not go racing mode */
	if (MML_FMT_IS_RGB(info->dest[0].data.format))
		goto decouple;

	if (MML_FMT_PLANE(info->src.format) > 2)
		goto decouple;

	/* get mid opp frequency */
	tp = mml_topology_get_cache(mml);
	if (!tp || !tp->opp_cnt) {
		mml_err("not support racing due to opp not ready");
		goto decouple;
	}

	/* TODO: remove after disp support calc time 6.75 * 1000 * height */
	if (!info->act_time)
		info->act_time = 3375 * info->dest[0].data.height;

	freq = tp->opp_speeds[MML_IR_MAX_OPP];

	mml_msg("[topology]%s act_time %u, freq:%d, src %ux%u, dst %ux%u, crop %ux%u, rot %d",
		__func__, info->act_time, freq,
		info->src.width, info->src.height,
		info->dest[0].data.width, info->dest[0].data.height,
		info->dest[0].crop.r.width, info->dest[0].crop.r.height,
		info->dest[0].rotate);

	if (!freq) {
		mml_err("no opp table support");
		goto decouple;
	}
	pixel = max(info->src.width * info->src.height,
		info->dest[0].data.width * info->dest[0].data.height);
	if (div_u64(pixel, freq) >= info->act_time)
		goto decouple;

	/* only support FHD/2K with rotate 90/270 case for now */
	if (info->dest[0].rotate == MML_ROT_0 || info->dest[0].rotate == MML_ROT_180)
		goto decouple;
	if (info->dest[0].crop.r.width > MML_IR_WIDTH_UPPER ||
		info->dest[0].crop.r.height > MML_IR_HEIGHT_UPPER ||
		pixel > MML_IR_MAX)
		goto decouple;
	if (info->dest[0].crop.r.width < MML_IR_WIDTH_LOWER ||
		info->dest[0].crop.r.height < MML_IR_HEIGHT_LOWER ||
		pixel < MML_IR_MIN)
		goto decouple;

	/* destination width must cross display pipe width */
	if (info->dest[0].data.width < MML_IR_HEIGHT_LOWER)
		goto decouple;

	if (info->dest[0].data.width * info->dest[0].data.height * 1000 /
		info->dest[0].crop.r.width / info->dest[0].crop.r.height <
		MML_IR_RSZ_MIN_RATIO)
		goto decouple;

	return MML_MODE_RACING;

decouple:
	return MML_MODE_MML_DECOUPLE;
decouple_user:
	return info->mode;
}

static struct cmdq_client *get_racing_clt(struct mml_topology_cache *cache, u32 pipe)
{
	/* use NO PQ path as inline rot path for this platform */
	return cache->paths[PATH_MML_NOPQ_P0 + pipe].clt;
}

static const struct mml_topology_ops tp_ops_mt6886 = {
	.query_mode = tp_query_mode,
	.init_cache = tp_init_cache,
	.select = tp_select,
	.get_racing_clt = get_racing_clt,
};

static __init int mml_topology_ip_init(void)
{
	return mml_topology_register_ip(TOPOLOGY_PLATFORM, &tp_ops_mt6886);
}
module_init(mml_topology_ip_init);

static __exit void mml_topology_ip_exit(void)
{
	mml_topology_unregister_ip(TOPOLOGY_PLATFORM);
}
module_exit(mml_topology_ip_exit);

MODULE_AUTHOR("Eason Yen <eason.yen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML for mt6886");
MODULE_LICENSE("GPL v2");

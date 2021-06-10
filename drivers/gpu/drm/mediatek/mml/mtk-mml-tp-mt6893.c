/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <dt-bindings/mml/mml-mt6893.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"

#define TOPOLOGY_PLATFORM	"mt6893"

/* TODO: change after dual pipe ready */
#define TOPOLOGY_FORCE_SINGLE	1

enum topology_scenario {
	PATH_MML_DC_NOPQ_P0 = 0,
	PATH_MML_DC_NOPQ_P1,
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

static const struct path_node path_map[PATH_MML_MAX][MML_MAX_PATH_CACHES] = {
	[PATH_MML_DC_NOPQ_P0] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_DC_NOPQ_P1] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA1, MML_WROT1,},
		{MML_WROT1,},
	}
};

enum cmdq_clt_usage {
	MML_CLT_PIPE0,
	MML_CLT_PIPE1,
	MML_CLT_MAX
};

static const u8 clt_dispatch[PATH_MML_MAX] = {
	[PATH_MML_DC_NOPQ_P0] = MML_CLT_PIPE0,
	[PATH_MML_DC_NOPQ_P1] = MML_CLT_PIPE1
};

static void tp_dump_path(struct mml_topology_path *path)
{
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		mml_log("engine %hhu (%p) prev %p next %p %p comp %p tile idx %hhu",
			path->nodes[i].id, &path->nodes[i],
			path->nodes[i].prev,
			path->nodes[i].next[0], path->nodes[i].next[1],
			path->nodes[i].comp,
			path->nodes[i].tile_eng_idx);
	}
}

static void tp_parse_path(struct mml_dev *mml, struct mml_topology_path *path,
	const struct path_node *route)
{
	struct mml_path_node *prev[2] = {0};
	u8 connect_eng[2] = {0};
	u8 i, tile_idx, out_eng_idx;

	for (i = 0; i < MML_MAX_PATH_CACHES; i++) {
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
			mml_err("no comp idx:%hhu engine:%hhu", i, eng);

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
		} else if (connect_eng[0] == eng) {
			/* connect out 0 */
			prev[0]->next[0] = &path->nodes[i];
			/* replace current out 0 to this engine */
			path->nodes[i].prev = prev[0];
			prev[0] = &path->nodes[i];
			connect_eng[0] = next0;

			/* also assign 1 in 2 out case,
			 * must branch from line 0
			 */
			if (!connect_eng[1] && next1) {
				prev[1] = &path->nodes[i];
				connect_eng[1] = next1;
			}
		} else if (connect_eng[1] == eng) {
			/* connect out 1 */
			prev[1]->next[0] = &path->nodes[i];
			/* replace current out 1 to this engine */
			path->nodes[i].prev = prev[1];
			prev[1] = &path->nodes[i];
			connect_eng[1] = next0;

			/* at most 2 out in one path,
			 * cannot branch from line 1
			 */
			if (next1)
				mml_err("%s wrong path index %hhu engine %hhu",
					i, eng);
		} else {
			mml_err("connect fail idx:%hhu engine:%hhu"
				" next0:%hhu next1:%hhu from:%hhu %hhu",
				i, eng, next0, next1,
				connect_eng[0], connect_eng[1]);
		}
	}
	path->node_cnt = i;

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
	struct cmdq_client **clts, u8 clt_cnt)
{
	u8 i;

	if (clt_cnt < MML_CLT_MAX) {
		mml_err("%s not enough cmdq clients to all path", __func__);
		return -ECHILD;
	}

	for (i = 0; i < PATH_MML_MAX; i++) {
		tp_parse_path(mml, &cache->path[i], path_map[i]);
		mml_log("dump path %hhu count %hhu",
			i, cache->path[i].node_cnt);
		tp_dump_path(&cache->path[i]);

		/* now dispatch cmdq client (channel) to path */
		cache->path[i].clt = clts[clt_dispatch[i]];
	}

	return 0;
}

static s32 tp_select(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg)
{
	struct mml_topology_path *path[2] = {0};

	if (cfg->info.mode == MML_MODE_MML_DECOUPLE) {
		if (cfg->info.dest_cnt == 1 &&
		    !cfg->info.dest[0].pq_config.en) {
			/* dual pipe, rdma0 to wrot0 / rdma1 to wrot1 */
#ifdef TOPOLOGY_FORCE_SINGLE
			cfg->dual = false;
#else
			cfg->dual = true;
#endif
			path[0] = &cache->path[PATH_MML_DC_NOPQ_P0];
			path[1] = &cache->path[PATH_MML_DC_NOPQ_P1];
		}
	}

	if (!path[0])
		return -EPERM;

	cfg->path[0] = path[0];
	cfg->path[1] = path[1];

	if (path[0]->alpharot) {
		u8 fmt_in = MML_FMT_HW_FORMAT(cfg->info.src.format);
		u8 fmt_out = MML_FMT_HW_FORMAT(cfg->info.dest[0].data.format);

		if ((fmt_in == 2 || fmt_in == 3) &&
			(fmt_out == 2 || fmt_out == 3))
			cfg->alpharot = true;
	}

	return 0;
}

static const struct mml_topology_ops tp_ops_mt6893 = {
	.init_cache = tp_init_cache,
	.select = tp_select
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

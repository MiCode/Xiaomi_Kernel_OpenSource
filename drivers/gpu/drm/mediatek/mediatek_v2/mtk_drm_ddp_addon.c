// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_ddp_addon.h"
#include "mtk_drm_drv.h"
#include "mtk_rect.h"
#include "mtk_dump.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"

static const int disp_rsz_path[] = {
	DDP_COMPONENT_RSZ0,
};

static const int disp_rsz_path_v2[] = {
	DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_RSZ0,
};

static const int disp_rsz_path_v3[] = {
	DDP_COMPONENT_OVL1_2L_VIRTUAL0,
	DDP_COMPONENT_RSZ1,
};

static const int disp_rsz_path_v4[] = {
	DDP_COMPONENT_OVL2_2L_VIRTUAL0,
	DDP_COMPONENT_RSZ1,
};

static const int disp_rsz_path_v5[] = {
	DDP_COMPONENT_OVL1_2L_VIRTUAL0,
	DDP_COMPONENT_RSZ0,
};

static const int disp_rsz_path_v6[] = {
	DDP_COMPONENT_OVL3_2L_VIRTUAL0,
	DDP_COMPONENT_RSZ1,
};

static const int ovl_rsz_path[] = {
	DDP_COMPONENT_OVLSYS_RSZ1, /* left pipe */
};

static const int ovl_rsz_path_1[] = {
	DDP_COMPONENT_OVLSYS_RSZ2, /* right pipe */
};

static const int dmdp_pq_with_rdma_path[] = {
	DDP_COMPONENT_DMDP_RDMA0,  DDP_COMPONENT_DMDP_HDR0,
	DDP_COMPONENT_DMDP_AAL0,   DDP_COMPONENT_DMDP_RSZ0,
	DDP_COMPONENT_DMDP_TDSHP0,
};

static const int disp_wdma0_path[] = {
	DDP_COMPONENT_WDMA0,
};

static const int disp_wdma0_path_v2[] = {
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};

static const int disp_wdma0_path_v3[] = {
	/* real case(WFD w/o TDSHP): need wait need PQ and VDCM */
	/* DDP_COMPONENT_PANEL0_COMP_OUT_CB2, */
	/* DDP_COMPONENT_VDCM0, */
	/* DDP_COMPONENT_COMP0_OUT_CB0, */
	/* DDP_COMPONENT_MERGE1, */
	/* DDP_COMPONENT_MERGE0_OUT_CB6, */
	/* DDP_COMPONENT_WDMA0, */
	DDP_COMPONENT_COMP0_OUT_CB3,
	DDP_COMPONENT_MERGE1,
	DDP_COMPONENT_MERGE0_OUT_CB6,
	DDP_COMPONENT_WDMA0,
	// DDP_COMPONENT_COMP0_OUT_CB3, DDP_COMPONENT_MERGE0_OUT_CB1,
	// DDP_COMPONENT_WDMA0,
};

static const int disp_wdma1_path[] = {
	DDP_COMPONENT_WDMA1,
};

/* real case(WFD w/o TDSHP): need wait need PQ and VDCM */
static const int disp_wdma1_path_v2[] = {
	/* DDP_COMPONENT_PANEL1_COMP_OUT_CB2, */
	/* DDP_COMPONENT_COMP1_OUT_CB4, */
	/* DDP_COMPONENT_MERGE1_OUT_CB2 */
	DDP_COMPONENT_WDMA1,
};

static const int disp_wdma2_path[] = {
	DDP_COMPONENT_WDMA2,
};

static const int disp_ovlsys_wdma0_path[] = {
	/* real case: need wait */
	/* DDP_COMPONENT_OVLSYS_DRSZ0, */
	/* DDP_COMPONENT_OVLSYS_WDMA1, */
	DDP_COMPONENT_OVLSYS_WDMA0,
};

static const int disp_ovlsys_wdma2_path[] = {
	/* real case: need wait */
	/* DDP_COMPONENT_OVLSYS_DRSZ1, */
	/* DDP_COMPONENT_OVLSYS_WDMA3, */
	DDP_COMPONENT_OVLSYS_WDMA2,
};

static const int disp_wdma2_path_v2[] = {
	DDP_COMPONENT_MAIN_OVL_DISP1_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA2,
};

static const int mml_rsz_path[] = {
	DDP_COMPONENT_MML_MML0, DDP_COMPONENT_MML_MUTEX0,
	DDP_COMPONENT_MML_DLI0,
	DDP_COMPONENT_MML_HDR0, DDP_COMPONENT_MML_AAL0, DDP_COMPONENT_MML_RSZ0,
	DDP_COMPONENT_MML_TDSHP0, DDP_COMPONENT_MML_COLOR0,
	DDP_COMPONENT_MML_DLO0,
};

static const int mml_rsz_path_v2[] = {
	DDP_COMPONENT_MML_DLI1,
	DDP_COMPONENT_MML_HDR1, DDP_COMPONENT_MML_AAL1, DDP_COMPONENT_MML_RSZ1,
	DDP_COMPONENT_MML_TDSHP1, DDP_COMPONENT_MML_COLOR1,
	DDP_COMPONENT_MML_DLO1,
};

static const int disp_mml_rsz_path[] = {
	DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_MML_MML0, DDP_COMPONENT_MML_MUTEX0,
	DDP_COMPONENT_MML_DLI0,
	DDP_COMPONENT_MML_HDR0, DDP_COMPONENT_MML_AAL0, DDP_COMPONENT_MML_RSZ0,
	DDP_COMPONENT_MML_TDSHP0, DDP_COMPONENT_MML_COLOR0,
	DDP_COMPONENT_MML_DLO0,
	DDP_COMPONENT_INLINE_ROTATE0,
	DDP_COMPONENT_DLI_ASYNC3,
	DDP_COMPONENT_Y2R0,
	DDP_COMPONENT_Y2R0_VIRTUAL0,	/* to describe ufod in */
};

static const int disp_mml_rsz_path_1[] = {
	DDP_COMPONENT_OVL2_2L_VIRTUAL0,
	DDP_COMPONENT_DLO_ASYNC7,
	DDP_COMPONENT_MML_MML0, DDP_COMPONENT_MML_MUTEX0,
	DDP_COMPONENT_MML_DLI1,
	DDP_COMPONENT_MML_HDR1, DDP_COMPONENT_MML_AAL1, DDP_COMPONENT_MML_RSZ1,
	DDP_COMPONENT_MML_TDSHP1, DDP_COMPONENT_MML_COLOR1,
	DDP_COMPONENT_MML_DLO1,
	DDP_COMPONENT_INLINE_ROTATE1,
	DDP_COMPONENT_DLI_ASYNC7,
	DDP_COMPONENT_Y2R1,
	DDP_COMPONENT_Y2R1_VIRTUAL0,	/* to describe ufod in */
};

static const int disp_mml_rsz_path_v2[] = {
	DDP_COMPONENT_OVLSYS_DLO_ASYNC0,
	DDP_COMPONENT_MML_MML0, DDP_COMPONENT_MML_MUTEX0,
	DDP_COMPONENT_INLINE_ROTATE0,
	DDP_COMPONENT_OVLSYS_DLI_ASYNC0,
	DDP_COMPONENT_OVLSYS_Y2R0,
};

static const int disp_mml_rsz_path_v2_1[] = {
	DDP_COMPONENT_OVLSYS_DLO_ASYNC7,
	DDP_COMPONENT_MML_MML0, DDP_COMPONENT_MML_MUTEX0,
	DDP_COMPONENT_INLINE_ROTATE1,
	DDP_COMPONENT_OVLSYS_DLI_ASYNC3,
	DDP_COMPONENT_OVLSYS_Y2R2,
};

static const int disp_mml_rsz_path_v3[] = {
	DDP_COMPONENT_OVL1_2L_VIRTUAL0,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_MML_MML0, DDP_COMPONENT_MML_MUTEX0,
	DDP_COMPONENT_MML_DLI0,
	DDP_COMPONENT_MML_HDR0, DDP_COMPONENT_MML_AAL0, DDP_COMPONENT_MML_RSZ0,
	DDP_COMPONENT_MML_TDSHP0, DDP_COMPONENT_MML_COLOR0,
	DDP_COMPONENT_MML_DLO0,
	DDP_COMPONENT_INLINE_ROTATE0,
	DDP_COMPONENT_DLI_ASYNC3,
	DDP_COMPONENT_Y2R0,
	DDP_COMPONENT_Y2R0_VIRTUAL0,	/* to describe ufod in */
};

static const int disp_mml_dl_path[] = {
	DDP_COMPONENT_OVLSYS_DLI_ASYNC0,
	DDP_COMPONENT_OVLSYS_Y2R0,
	DDP_COMPONENT_Y2R0_VIRTUAL0,	/* to describe ufod in */
};

static const int disp_mml_dl_path_1[] = {
	DDP_COMPONENT_OVLSYS_DLI_ASYNC3,
	DDP_COMPONENT_OVLSYS_Y2R2,
	DDP_COMPONENT_Y2R1_VIRTUAL0,	/* to describe ufod in */
};

static const int disp_mml_sram_only_path[] = {
	DDP_COMPONENT_INLINE_ROTATE0,
};

static const int disp_mml_sram_only_path_1[] = {
	DDP_COMPONENT_INLINE_ROTATE1,
};

static const struct mtk_addon_path_data addon_module_path[ADDON_MODULE_NUM] = {
		[DISP_RSZ] = {
				.path = disp_rsz_path,
				.path_len = ARRAY_SIZE(disp_rsz_path),
			},
		[DISP_RSZ_v2] = {
				.path = disp_rsz_path_v2,
				.path_len = ARRAY_SIZE(disp_rsz_path_v2),
			},
		[DISP_RSZ_v3] = {
				.path = disp_rsz_path_v3,
				.path_len = ARRAY_SIZE(disp_rsz_path_v3),
			},
		[DISP_RSZ_v4] = {
				.path = disp_rsz_path_v4,
				.path_len = ARRAY_SIZE(disp_rsz_path_v4),
			},
		[DISP_RSZ_v5] = {
				.path = disp_rsz_path_v5,
				.path_len = ARRAY_SIZE(disp_rsz_path_v5),
			},
		[DISP_RSZ_v6] = {
				.path = disp_rsz_path_v6,
				.path_len = ARRAY_SIZE(disp_rsz_path_v6),
			},
		[OVL_RSZ] = {
				.path = ovl_rsz_path,
				.path_len = ARRAY_SIZE(ovl_rsz_path),
			},
		[OVL_RSZ_1] = {
				.path = ovl_rsz_path_1,
				.path_len = ARRAY_SIZE(ovl_rsz_path_1),
			},
		[DMDP_PQ_WITH_RDMA] = {
				.path = dmdp_pq_with_rdma_path,
				.path_len = ARRAY_SIZE(dmdp_pq_with_rdma_path),
			},
		[DISP_WDMA0] = {
				.path = disp_wdma0_path,
				.path_len = ARRAY_SIZE(disp_wdma0_path),
			},
		[DISP_WDMA0_v2] = {
				.path = disp_wdma0_path_v2,
				.path_len = ARRAY_SIZE(disp_wdma0_path_v2),
			},
		[DISP_WDMA0_v3] = {
				.path = disp_wdma0_path_v3,
				.path_len = ARRAY_SIZE(disp_wdma0_path_v3),
			},
		[DISP_WDMA1] = {
				.path = disp_wdma1_path,
				.path_len = ARRAY_SIZE(disp_wdma1_path),
			},
		[DISP_WDMA1_v2] = {
				.path = disp_wdma1_path_v2,
				.path_len = ARRAY_SIZE(disp_wdma1_path_v2),
			},
		[DISP_WDMA2] = {
				.path = disp_wdma2_path,
				.path_len = ARRAY_SIZE(disp_wdma2_path),
			},
		[DISP_OVLSYS_WDMA0] = {
				.path = disp_ovlsys_wdma0_path,
				.path_len = ARRAY_SIZE(disp_ovlsys_wdma0_path),
			},
		[DISP_OVLSYS_WDMA2] = {
				.path = disp_ovlsys_wdma2_path,
				.path_len = ARRAY_SIZE(disp_ovlsys_wdma2_path),
			},
		[DISP_WDMA2_v2] = {
				.path = disp_wdma2_path_v2,
				.path_len = ARRAY_SIZE(disp_wdma2_path_v2),
			},
		[DISP_MML_IR_PQ] = {
				.path = disp_mml_rsz_path,
				.path_len = ARRAY_SIZE(disp_mml_rsz_path),
			},
		[DISP_MML_IR_PQ_1] = {
				.path = disp_mml_rsz_path_1,
				.path_len = ARRAY_SIZE(disp_mml_rsz_path_1),
			},
		[DISP_MML_IR_PQ_v2] = {
				.path = disp_mml_rsz_path_v2,
				.path_len = ARRAY_SIZE(disp_mml_rsz_path_v2),
			},
		[DISP_MML_IR_PQ_v2_1] = {
				.path = disp_mml_rsz_path_v2_1,
				.path_len = ARRAY_SIZE(disp_mml_rsz_path_v2_1),
			},
		[DISP_MML_SRAM_ONLY] = {
				.path = disp_mml_sram_only_path,
				.path_len = ARRAY_SIZE(disp_mml_sram_only_path),
			},
		[DISP_MML_SRAM_ONLY_1] = {
				.path = disp_mml_sram_only_path_1,
				.path_len = ARRAY_SIZE(disp_mml_sram_only_path_1),
			},
		[DISP_MML_DL] = {
				.path = disp_mml_dl_path,
				.path_len = ARRAY_SIZE(disp_mml_dl_path),
			},
		[DISP_MML_DL_1] = {
				.path = disp_mml_dl_path_1,
				.path_len = ARRAY_SIZE(disp_mml_dl_path_1),
			},
		[DISP_MML_IR_PQ_v3] = {
				.path = disp_mml_rsz_path_v3,
				.path_len = ARRAY_SIZE(disp_mml_rsz_path_v3),
			},
};

const struct mtk_addon_path_data *
mtk_addon_module_get_path(enum addon_module module)
{
	return &addon_module_path[module];
}

const struct mtk_addon_scenario_data *
mtk_addon_get_scenario_data(const char *source, struct drm_crtc *crtc,
			    enum addon_scenario scn)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (scn < NONE || scn >= ADDON_SCN_NR)
		goto err;

	if (mtk_crtc->path_data && mtk_crtc->path_data->addon_data)
		return &mtk_crtc->path_data->addon_data[scn];

err:
	DDPPR_ERR("[%s] crtc%d cannot get addon data scn[%u]\n", source,
		  drm_crtc_index(crtc), scn);
	return NULL;
}
const struct mtk_addon_scenario_data *
mtk_addon_get_scenario_data_dual(const char *source, struct drm_crtc *crtc,
			    enum addon_scenario scn)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if ((scn < NONE) || (scn > ADDON_SCN_NR)) {
		DDPPR_ERR("[%s] crtc%d scn is wrong\n", source,
				drm_crtc_index(crtc));
		return NULL;
	}

	if (mtk_crtc->path_data && mtk_crtc->path_data->addon_data_dual)
		return &mtk_crtc->path_data->addon_data_dual[scn];

	return NULL;
}

bool mtk_addon_scenario_support(struct drm_crtc *crtc, enum addon_scenario scn)
{
	const struct mtk_addon_scenario_data *data =
		mtk_addon_get_scenario_data(__func__, crtc, scn);

	if (data && (data->module_num > 0 || scn == NONE))
		return true;

	return false;
}

static void mtk_addon_path_start(struct drm_crtc *crtc,
				 const struct mtk_addon_path_data *path_data,
				 struct cmdq_pkt *cmdq_handle)
{
	int i;
	struct mtk_ddp_comp *add_comp = NULL;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	for (i = 0; i < path_data->path_len; i++) {
		if (mtk_ddp_comp_get_type(path_data->path[i])
			== MTK_DISP_VIRTUAL)
			continue;

		add_comp = priv->ddp_comp[path_data->path[i]];
		mtk_ddp_comp_start(add_comp, cmdq_handle);
	}
}

static void mtk_addon_path_stop(struct drm_crtc *crtc,
				const struct mtk_addon_path_data *path_data,
				union mtk_addon_config *addon_config,
				struct cmdq_pkt *cmdq_handle)
{
	int i;
	struct mtk_ddp_comp *add_comp = NULL;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	for (i = 0; i < path_data->path_len; i++) {
		if (mtk_ddp_comp_get_type(path_data->path[i])
			== MTK_DISP_VIRTUAL)
			continue;

		add_comp = priv->ddp_comp[path_data->path[i]];
		mtk_ddp_comp_stop(add_comp, cmdq_handle);

		if (addon_config &&
			(addon_config->config_type.module == DISP_MML_IR_PQ ||
			addon_config->config_type.module == DISP_MML_IR_PQ_1 ||
			addon_config->config_type.module == DISP_MML_IR_PQ_v2 ||
			addon_config->config_type.module == DISP_MML_IR_PQ_v3) &&
			addon_config->config_type.type == ADDON_DISCONNECT)
			mtk_ddp_comp_addon_config(add_comp, -1, -1, addon_config, cmdq_handle);
	}
}

void mtk_addon_path_config(struct drm_crtc *crtc,
			const struct mtk_addon_module_data *module_data,
			union mtk_addon_config *addon_config,
			struct cmdq_pkt *cmdq_handle)
{
	int i;
	struct mtk_ddp_comp *add_comp = NULL;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	enum mtk_ddp_comp_id prev_comp_id = -1, next_comp_id = -1;
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module_data->module);

	for (i = 0; i < path_data->path_len; i++) {
		if (mtk_ddp_comp_get_type(path_data->path[i])
			== MTK_DISP_VIRTUAL)
			continue;

		add_comp = priv->ddp_comp[path_data->path[i]];
		prev_comp_id = (i > 0) ? path_data->path[(i - 1)] : -1;
		next_comp_id = (i < path_data->path_len - 1) ? path_data->path[(i + 1)] : -1;
		mtk_ddp_comp_addon_config(add_comp, prev_comp_id, next_comp_id,
					  addon_config, cmdq_handle);
	}
}

static void
mtk_addon_path_connect_and_add_mutex(struct drm_crtc *crtc,
				     unsigned int ddp_mode,
				     const struct mtk_addon_module_data *module,
				     struct cmdq_pkt *cmdq_handle)
{
	int i;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int mutex_id =
		mtk_crtc_get_mutex_id(crtc, ddp_mode, module->attach_comp);
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module->module);

	if (mutex_id < 0) {
		DDPPR_ERR("invalid mutex id:%d\n", mutex_id);
		return;
	}

	for (i = 0; i < path_data->path_len - 1; i++) {
		mtk_ddp_add_comp_to_path_with_cmdq(mtk_crtc, path_data->path[i],
						   path_data->path[i + 1],
						   cmdq_handle);
		mtk_disp_mutex_add_comp_with_cmdq(
			mtk_crtc, path_data->path[i],
			mtk_crtc_is_frame_trigger_mode(crtc), cmdq_handle,
			mutex_id);
	}

	mtk_disp_mutex_add_comp_with_cmdq(
		mtk_crtc, path_data->path[path_data->path_len - 1],
		mtk_crtc_is_frame_trigger_mode(crtc), cmdq_handle, mutex_id);
}

static void mtk_addon_path_disconnect_and_remove_mutex(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module,
	struct cmdq_pkt *cmdq_handle)
{
	int i;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int mutex_id =
		mtk_crtc_get_mutex_id(crtc, ddp_mode, module->attach_comp);
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module->module);

	if (mutex_id < 0) {
		DDPPR_ERR("invalid mutex id:%d\n", mutex_id);
		return;
	}

	for (i = 0; i < path_data->path_len - 1; i++) {
		mtk_ddp_remove_comp_from_path_with_cmdq(
			mtk_crtc, path_data->path[i], path_data->path[i + 1],
			cmdq_handle);
		mtk_disp_mutex_remove_comp_with_cmdq(
			mtk_crtc, path_data->path[i], cmdq_handle, mutex_id);
	}

	mtk_disp_mutex_remove_comp_with_cmdq(
		mtk_crtc, path_data->path[path_data->path_len - 1], cmdq_handle,
		mutex_id);
}

void mtk_addon_connect_between(struct drm_crtc *crtc, unsigned int ddp_mode,
			       const struct mtk_addon_module_data *module_data,
			       union mtk_addon_config *addon_config,
			       struct cmdq_pkt *cmdq_handle)

{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp = NULL;
	enum mtk_ddp_comp_id attach_comp_id, next_attach_comp_id, prev_comp_id,
		cur_comp_id, next_comp_id;
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module_data->module);
	int i, j;
	unsigned int addon_idx;

	attach_comp_id =
		mtk_crtc_find_comp(crtc, ddp_mode, module_data->attach_comp);

	if (attach_comp_id == -1 || attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s is not in path mode %d attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, attach_comp_id);
		return;
	}

	next_attach_comp_id = mtk_crtc_find_next_comp(crtc, ddp_mode,
						      module_data->attach_comp);

	if (next_attach_comp_id == -1 || next_attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s has not a next comp in path mode next_attach_comp_id %d\n",
		mtk_dump_comp_str(comp), next_attach_comp_id);
		return;
	}

	/* 1. remove original path*/
	mtk_crtc_disconnect_path_between_component(
		crtc, ddp_mode, attach_comp_id, next_attach_comp_id,
		cmdq_handle);

	/* 2. connect subpath and add mutex*/
	mtk_addon_path_connect_and_add_mutex(crtc, ddp_mode, module_data,
					     cmdq_handle);

	/* 3. add subpath to main path */
	mtk_ddp_add_comp_to_path_with_cmdq(mtk_crtc, attach_comp_id,
					   path_data->path[0], cmdq_handle);
	mtk_ddp_add_comp_to_path_with_cmdq(
		mtk_crtc, path_data->path[path_data->path_len - 1],
		next_attach_comp_id, cmdq_handle);

	/* 4. config module */
	/* config attach comp */
	comp = priv->ddp_comp[attach_comp_id];
	prev_comp_id = mtk_crtc_find_prev_comp(crtc, ddp_mode, attach_comp_id);
	cur_comp_id = comp->id;
	next_comp_id = path_data->path[0];
	for (i = 0; i < path_data->path_len; i++)
		if (mtk_ddp_comp_get_type(path_data->path[i]) !=
		    MTK_DISP_VIRTUAL) {
			next_comp_id = path_data->path[i];
			break;
		}

	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);
	/* config subpath comp without last comp*/
	addon_idx = i;
	for (i = addon_idx; i < path_data->path_len; i++) {
		if (mtk_ddp_comp_get_type(path_data->path[i]) ==
		    MTK_DISP_VIRTUAL)
			continue;

		addon_idx = i;
		comp = priv->ddp_comp[path_data->path[i]];
		prev_comp_id = cur_comp_id;
		cur_comp_id = comp->id;
		comp->mtk_crtc = mtk_crtc;

		for (j = i + 1; j < path_data->path_len; j++)
			if (mtk_ddp_comp_get_type(path_data->path[j]) !=
			    MTK_DISP_VIRTUAL) {
				next_comp_id = path_data->path[j];
				mtk_ddp_comp_addon_config(
					comp, prev_comp_id, next_comp_id,
					addon_config, cmdq_handle);
				break;
			}
	}
	/* config subpath last comp */
	comp = priv->ddp_comp[path_data->path[addon_idx]];
	prev_comp_id = cur_comp_id;
	cur_comp_id = comp->id;
	next_comp_id = next_attach_comp_id;
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);
	/* config next attach comp */
	comp = priv->ddp_comp[next_attach_comp_id];
	prev_comp_id = cur_comp_id;
	cur_comp_id = comp->id;
	next_comp_id =
		mtk_crtc_find_next_comp(crtc, ddp_mode, next_attach_comp_id);
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);

	/* 5. start addon module */
	mtk_addon_path_start(crtc, path_data, cmdq_handle);
}

void mtk_addon_disconnect_between(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp = NULL;
	enum mtk_ddp_comp_id attach_comp_id, next_attach_comp_id, prev_comp_id,
		next_comp_id;
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module_data->module);

	attach_comp_id =
		mtk_crtc_find_comp(crtc, ddp_mode, module_data->attach_comp);

	if (attach_comp_id == -1 || attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s is not in path mode %d attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, attach_comp_id);
		return;
	}

	next_attach_comp_id = mtk_crtc_find_next_comp(crtc, ddp_mode,
						      module_data->attach_comp);

	if (next_attach_comp_id == -1 || next_attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s has not a next comp in path mode %d next_attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, next_attach_comp_id);
		return;
	}

	/* 1. stop addon module*/
	mtk_addon_path_stop(crtc, path_data, addon_config, cmdq_handle);

	/* 2. remove subpath from main path */
	mtk_ddp_remove_comp_from_path_with_cmdq(
		mtk_crtc, attach_comp_id, path_data->path[0], cmdq_handle);
	mtk_ddp_remove_comp_from_path_with_cmdq(
		mtk_crtc, path_data->path[path_data->path_len - 1],
		next_attach_comp_id, cmdq_handle);

	/* 3. disconnect subpath and remove mutex */
	mtk_addon_path_disconnect_and_remove_mutex(crtc, ddp_mode, module_data,
						   cmdq_handle);

	/* 4. connect original path*/
	mtk_crtc_connect_path_between_component(crtc, ddp_mode, attach_comp_id,
						next_attach_comp_id,
						cmdq_handle);

	/* 5. config module*/
	/* config attach comp */
	comp = priv->ddp_comp[attach_comp_id];
	prev_comp_id = mtk_crtc_find_prev_comp(crtc, ddp_mode, attach_comp_id);
	next_comp_id = next_attach_comp_id;
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);

	comp = priv->ddp_comp[next_attach_comp_id];
	prev_comp_id = attach_comp_id;
	next_comp_id =
		mtk_crtc_find_next_comp(crtc, ddp_mode, next_attach_comp_id);
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);
}

void mtk_addon_connect_before(struct drm_crtc *crtc, unsigned int ddp_mode,
			      const struct mtk_addon_module_data *module_data,
			      union mtk_addon_config *addon_config,
			      struct cmdq_pkt *cmdq_handle)

{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp = NULL;
	enum mtk_ddp_comp_id next_attach_comp_id, prev_comp_id, cur_comp_id,
		next_comp_id;
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module_data->module);
	int i, j;
	unsigned int addon_idx;

	next_attach_comp_id =
		mtk_crtc_find_comp(crtc, ddp_mode, module_data->attach_comp);

	if (module_data->attach_comp >= DDP_COMPONENT_ID_MAX) {
		DDPPR_ERR("Invalid attach_comp value\n");
		return;
	}

	if (next_attach_comp_id == -1 || next_attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s is not in path mode %d next_attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, next_attach_comp_id);
		return;
	}

	/* 1. connect subpath and add mutex*/
	mtk_addon_path_connect_and_add_mutex(crtc, ddp_mode, module_data,
					     cmdq_handle);

	/* 2. add subpath to main path */
	mtk_ddp_add_comp_to_path_with_cmdq(
		mtk_crtc, path_data->path[path_data->path_len - 1],
		next_attach_comp_id, cmdq_handle);

	/* 3. config module */
	/* config subpath comp without last comp*/
	addon_idx = 0;
	cur_comp_id = -1;
	for (i = 0; i < path_data->path_len; i++) {
		if (mtk_ddp_comp_get_type(path_data->path[i]) ==
		    MTK_DISP_VIRTUAL)
			continue;

		addon_idx = i;
		comp = priv->ddp_comp[path_data->path[i]];
		prev_comp_id = cur_comp_id;
		cur_comp_id = comp->id;

		for (j = i + 1; j < path_data->path_len; j++)
			if (mtk_ddp_comp_get_type(path_data->path[j]) !=
			    MTK_DISP_VIRTUAL) {
				next_comp_id = path_data->path[j];

				mtk_ddp_comp_addon_config(
					comp, prev_comp_id, next_comp_id,
					addon_config, cmdq_handle);
				break;
			}
	}
	/* config subpath last comp */
	if (path_data->path[addon_idx] >= DDP_COMPONENT_ID_MAX) {
		DDPPR_ERR("%s path_data->path[addon_idx] %d\n",
				__func__, path_data->path[addon_idx]);
		return;
	}

	comp = priv->ddp_comp[path_data->path[addon_idx]];
	prev_comp_id = cur_comp_id;
	cur_comp_id = comp->id;
	next_comp_id = next_attach_comp_id;
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);
	comp = priv->ddp_comp[next_attach_comp_id];
	prev_comp_id = cur_comp_id;
	cur_comp_id = comp->id;
	next_comp_id =
		mtk_crtc_find_next_comp(crtc, ddp_mode, next_attach_comp_id);
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);

	/* 4. start addon module */
	mtk_addon_path_start(crtc, path_data, cmdq_handle);
}

void mtk_addon_disconnect_before(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp = NULL;
	enum mtk_ddp_comp_id prev_comp_id, next_comp_id, next_attach_comp_id;
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module_data->module);

	next_attach_comp_id =
		mtk_crtc_find_comp(crtc, ddp_mode, module_data->attach_comp);

	if (next_attach_comp_id == -1 || next_attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s is not in path mode %d next_attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, next_attach_comp_id);
		return;
	}

	/* 1. stop addon module*/
	mtk_addon_path_stop(crtc, path_data, addon_config, cmdq_handle);

	/* 2. remove subpath from main path */
	mtk_ddp_remove_comp_from_path_with_cmdq(
		mtk_crtc, path_data->path[path_data->path_len - 1],
		next_attach_comp_id, cmdq_handle);

	/* 3. disconnect subpath and remove mutex */
	mtk_addon_path_disconnect_and_remove_mutex(crtc, ddp_mode, module_data,
						   cmdq_handle);
	/* 4. config module */
	/* config attach comp */
	comp = priv->ddp_comp[next_attach_comp_id];
	prev_comp_id =
		mtk_crtc_find_prev_comp(crtc, ddp_mode, next_attach_comp_id);
	next_comp_id =
		mtk_crtc_find_next_comp(crtc, ddp_mode, next_attach_comp_id);
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);
}

void mtk_addon_connect_after(struct drm_crtc *crtc, unsigned int ddp_mode,
			      const struct mtk_addon_module_data *module_data,
			      union mtk_addon_config *addon_config,
			      struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp = NULL;
	enum mtk_ddp_comp_id prev_attach_comp_id = 0, path_attach_id = 0,
		prev_comp_id, cur_comp_id, next_comp_id;
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module_data->module);
	int i, j;
	unsigned int addon_idx;

	if (mtk_ddp_comp_get_type(module_data->attach_comp) ==
			MTK_DISP_VIRTUAL) {
		path_attach_id = module_data->attach_comp;
		prev_attach_comp_id = mtk_crtc_find_prev_comp(crtc, ddp_mode,
				path_attach_id);
	} else {
		prev_attach_comp_id =
			mtk_crtc_find_comp(crtc, ddp_mode, module_data->attach_comp);
		path_attach_id = prev_attach_comp_id;
	}

	if (prev_attach_comp_id == -1 || prev_attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s is not in path mode %d prev_attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, prev_attach_comp_id);
		return;
	}

	/* 0. attach subpath to crtc*/
	/* some comp need crtc info in addon_config or irq */
	mtk_crtc_attach_addon_path_comp(crtc, module_data, true);

	/* 1. connect subpath and add mutex*/
	mtk_addon_path_connect_and_add_mutex(crtc, ddp_mode, module_data,
					     cmdq_handle);

	/* 2. add subpath to main path */
	mtk_ddp_add_comp_to_path_with_cmdq(
		mtk_crtc, path_attach_id, path_data->path[0],
		cmdq_handle);

	/* 3. config module */
	/* config attach comp */
	prev_comp_id = mtk_crtc_find_prev_comp(crtc, ddp_mode,
				prev_attach_comp_id);
	comp = priv->ddp_comp[prev_attach_comp_id];
	cur_comp_id = comp->id;
	next_comp_id = -1;
	for (i = 0; i < path_data->path_len; i++)
		if (mtk_ddp_comp_get_type(path_data->path[i]) !=
		    MTK_DISP_VIRTUAL) {
			next_comp_id = path_data->path[i];
			break;
		}
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);
	/* config subpath comp without last comp*/
	addon_idx = i;
	for (; i < path_data->path_len; i++) {
		if (mtk_ddp_comp_get_type(path_data->path[i]) ==
		    MTK_DISP_VIRTUAL)
			continue;

		addon_idx = i;
		comp = priv->ddp_comp[path_data->path[i]];

		for (j = i + 1; j < path_data->path_len; j++)
			if (mtk_ddp_comp_get_type(path_data->path[j]) !=
			    MTK_DISP_VIRTUAL) {
				prev_comp_id = cur_comp_id;
				cur_comp_id = comp->id;
				next_comp_id = path_data->path[j];
				mtk_ddp_comp_addon_config(
					comp, prev_comp_id, next_comp_id,
					addon_config, cmdq_handle);
				break;
			}
	}
	/* config subpath last comp */
	prev_comp_id = cur_comp_id;
	comp = priv->ddp_comp[path_data->path[addon_idx]];
	cur_comp_id = comp->id;
	next_comp_id = -1;
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);

	/* 4. start addon module */
	mtk_addon_path_start(crtc, path_data, cmdq_handle);
}

void mtk_addon_disconnect_after(
	struct drm_crtc *crtc, unsigned int ddp_mode,
	const struct mtk_addon_module_data *module_data,
	union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp = NULL;
	enum mtk_ddp_comp_id prev_attach_comp_id = 0, path_attach_id = 0,
		prev_comp_id, next_comp_id;
	const struct mtk_addon_path_data *path_data =
		mtk_addon_module_get_path(module_data->module);

	if (mtk_ddp_comp_get_type(module_data->attach_comp) ==
			MTK_DISP_VIRTUAL) {
		path_attach_id = module_data->attach_comp;
		prev_attach_comp_id = mtk_crtc_find_prev_comp(crtc, ddp_mode,
				path_attach_id);
	} else {
		prev_attach_comp_id =
			mtk_crtc_find_comp(crtc, ddp_mode, module_data->attach_comp);
		path_attach_id = prev_attach_comp_id;
	}

	if (prev_attach_comp_id == -1 || prev_attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s is not in path mode %d prev_attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, prev_attach_comp_id);
		return;
	}

	/* 0. attach subpath to crtc*/
	/* some comp need crtc info in stop*/
	mtk_crtc_attach_addon_path_comp(crtc, module_data, true);

	/* 1. stop addon module*/
	mtk_addon_path_stop(crtc, path_data, addon_config, cmdq_handle);

	/* 2. remove subpath from main path */
	mtk_ddp_remove_comp_from_path_with_cmdq(
		mtk_crtc, path_attach_id, path_data->path[0],
		cmdq_handle);

	/* 3. disconnect subpath and remove mutex */
	mtk_addon_path_disconnect_and_remove_mutex(crtc, ddp_mode, module_data,
						   cmdq_handle);

	/* 4. config module */
	/* config attach comp */
	comp = priv->ddp_comp[prev_attach_comp_id];
	prev_comp_id =
		mtk_crtc_find_prev_comp(crtc, ddp_mode, prev_attach_comp_id);
	next_comp_id =
		mtk_crtc_find_next_comp(crtc, ddp_mode, prev_attach_comp_id);
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
				  addon_config, cmdq_handle);
}

void mtk_addon_connect_embed(struct drm_crtc *crtc, unsigned int ddp_mode,
			     const struct mtk_addon_module_data *module_data,
			     union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp = NULL;
	enum mtk_ddp_comp_id attach_comp_id, prev_comp_id, cur_comp_id, next_comp_id;
	const struct mtk_addon_path_data *path_data =
	    mtk_addon_module_get_path(module_data->module);
	int i, j;
	unsigned int addon_idx;

	attach_comp_id = mtk_crtc_find_comp(crtc, ddp_mode, module_data->attach_comp);
	if (attach_comp_id == -1 || attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		if (module_data->attach_comp >= 0)
			comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s is not in path mode %d attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, attach_comp_id);
		return;
	}

	/* 1. connect subpath and add mutex*/
	mtk_addon_path_connect_and_add_mutex(crtc, ddp_mode, module_data, cmdq_handle);

	/* 2. embed subpath into attached comp */
	mtk_ddp_add_comp_to_path_with_cmdq(mtk_crtc, attach_comp_id, path_data->path[0],
					   cmdq_handle);
	mtk_ddp_add_comp_to_path_with_cmdq(mtk_crtc, path_data->path[path_data->path_len - 1],
					   attach_comp_id, cmdq_handle);

	/* 3. config module */
	/* 3.1 config attached comp */
	comp = priv->ddp_comp[attach_comp_id];
	prev_comp_id = path_data->path[path_data->path_len - 1];
	cur_comp_id = comp->id;
	next_comp_id = -1;
	for (i = 0; i < path_data->path_len; i++)
		if (mtk_ddp_comp_get_type(path_data->path[i]) != MTK_DISP_VIRTUAL) {
			next_comp_id = path_data->path[i];
			break;
		}
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id, addon_config, cmdq_handle);

	/* 3.2 config subpath without last comp */
	addon_idx = i;
	for (; i < path_data->path_len; i++) {
		if (mtk_ddp_comp_get_type(path_data->path[i]) == MTK_DISP_VIRTUAL)
			continue;

		addon_idx = i;
		comp = priv->ddp_comp[path_data->path[i]];

		for (j = addon_idx + 1; j < path_data->path_len; j++)
			if (mtk_ddp_comp_get_type(path_data->path[j]) != MTK_DISP_VIRTUAL) {
				prev_comp_id = cur_comp_id;
				cur_comp_id = comp->id;
				next_comp_id = path_data->path[j];
				comp->mtk_crtc = mtk_crtc;
				mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id,
							  addon_config, cmdq_handle);
				break;
			}
	}
	/* 3.3 config subpath last comp */
	prev_comp_id = cur_comp_id;
	comp = priv->ddp_comp[path_data->path[addon_idx]];
	cur_comp_id = comp->id;
	next_comp_id = attach_comp_id;
	comp->mtk_crtc = mtk_crtc;
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id, addon_config, cmdq_handle);

	/* 4. start addon module */
	mtk_addon_path_start(crtc, path_data, cmdq_handle);
}

void mtk_addon_disconnect_embed(struct drm_crtc *crtc, unsigned int ddp_mode,
				const struct mtk_addon_module_data *module_data,
				union mtk_addon_config *addon_config, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp = NULL;
	enum mtk_ddp_comp_id attach_comp_id, prev_comp_id, next_comp_id;
	const struct mtk_addon_path_data *path_data =
	    mtk_addon_module_get_path(module_data->module);

	attach_comp_id = mtk_crtc_find_comp(crtc, ddp_mode, module_data->attach_comp);

	if (attach_comp_id == -1 || attach_comp_id >= DDP_COMPONENT_ID_MAX) {
		comp = priv->ddp_comp[module_data->attach_comp];
		DDPPR_ERR(
		"Attach module:%s is not in path mode %d attach_comp_id %d\n",
		mtk_dump_comp_str(comp), ddp_mode, attach_comp_id);
		return;
	}

	/* 1. stop addon module*/
	mtk_addon_path_stop(crtc, path_data, addon_config, cmdq_handle);

	/* 2. remove subpath from main path */
	mtk_ddp_remove_comp_from_path_with_cmdq(
		mtk_crtc, attach_comp_id, path_data->path[0], cmdq_handle);
	mtk_ddp_remove_comp_from_path_with_cmdq(
		mtk_crtc, path_data->path[path_data->path_len - 1],
		attach_comp_id, cmdq_handle);

	/* 3. disconnect subpath and remove mutex */
	mtk_addon_path_disconnect_and_remove_mutex(crtc, ddp_mode, module_data, cmdq_handle);

	/* 4. config module*/
	comp = priv->ddp_comp[attach_comp_id];
	prev_comp_id = mtk_crtc_find_prev_comp(crtc, ddp_mode, attach_comp_id);
	next_comp_id = mtk_crtc_find_next_comp(crtc, ddp_mode, attach_comp_id);
	mtk_ddp_comp_addon_config(comp, prev_comp_id, next_comp_id, addon_config, cmdq_handle);
}

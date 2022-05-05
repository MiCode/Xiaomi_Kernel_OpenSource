// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/init.h>

#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-video.h"
#include "mtk_cam-plat-util.h"
#include "mtk_cam-meta-mt6983.h"

#define RAW_STATS_CFG_SIZE \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_cfg), SZ_1K)

/* meta out max size include 1k meta info and dma buffer size */
#define RAW_STATS_0_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_0), SZ_1K) + \
	      MTK_CAM_UAPI_AAO_MAX_BUF_SIZE + MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_LTMSO_SIZE + \
	      MTK_CAM_UAPI_FLK_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_TSFSO_SIZE * 2 + /* r1 & r2 */ \
	      MTK_CAM_UAPI_TNCSYO_SIZE \
	      , (4 * SZ_1K))

#define RAW_STATS_1_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_1), SZ_1K) + \
	      MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, (4 * SZ_1K))

#define RAW_STATS_2_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_2), SZ_1K) + \
	      MTK_CAM_UAPI_ACTSO_SIZE, (4 * SZ_1K))

#define SV_STATS_0_SIZE \
	sizeof(struct mtk_cam_uapi_meta_camsv_stats_0)

#define SV_EXT_STATS_SIZE (1024 * 1024 * 3)
/* FIXME for ISP6 meta format */
static const struct mtk_cam_format_desc meta_fmts[] = {
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_PARAMS,
			.buffersize = RAW_STATS_CFG_SIZE,
		},
	},
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_3A,
			.buffersize = RAW_STATS_0_SIZE,
		},
	},
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_AF,
			.buffersize = RAW_STATS_1_SIZE,
		},
	},
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_LCS,
			.buffersize = RAW_STATS_2_SIZE,
		},
	},
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_3A,
			.buffersize = SV_EXT_STATS_SIZE,
		},
	},
};

static void set_payload(struct mtk_cam_uapi_meta_hw_buf *buf,
			unsigned int size, unsigned long *offset)
{
	buf->offset = *offset;
	buf->size = size;
	*offset += size;
}

static uint64_t *camsys_get_timestamp_addr(void *vaddr)
{
	struct mtk_cam_uapi_meta_raw_stats_0 *stats0;

	stats0 = (struct mtk_cam_uapi_meta_raw_stats_0 *)vaddr;
	return (uint64_t *)(stats0->timestamp.timestamp_buf);
}

static void camsys_set_meta_stats_info(u32 dma_port, void *vaddr,
				 struct mtk_raw_pde_config *pde_cfg)
{
	struct mtk_cam_uapi_meta_raw_stats_0 *stats0;
	struct mtk_cam_uapi_meta_raw_stats_1 *stats1;
	unsigned long offset;

	switch (dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_0:
		stats0 = (struct mtk_cam_uapi_meta_raw_stats_0 *)vaddr;
		offset = sizeof(*stats0);
		set_payload(&stats0->ae_awb_stats.aao_buf, MTK_CAM_UAPI_AAO_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->ae_awb_stats.aaho_buf,
			MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->ltm_stats.ltmso_buf, MTK_CAM_UAPI_LTMSO_SIZE, &offset);
		set_payload(&stats0->flk_stats.flko_buf, MTK_CAM_UAPI_FLK_MAX_BUF_SIZE, &offset);
		set_payload(&stats0->tsf_stats.tsfo_r1_buf, MTK_CAM_UAPI_TSFSO_SIZE, &offset);
		set_payload(&stats0->tsf_stats.tsfo_r2_buf, MTK_CAM_UAPI_TSFSO_SIZE, &offset);
		set_payload(&stats0->tncy_stats.tncsyo_buf, MTK_CAM_UAPI_TNCSYO_SIZE, &offset);
		if (pde_cfg) {
			if (pde_cfg->pde_info[CAM_SET_CTRL].pd_table_offset) {
				set_payload(&stats0->pde_stats.pdo_buf,
					    pde_cfg->pde_info[CAM_SET_CTRL].pdo_max_size,
					    &offset);
			}
		}
		break;
	case MTKCAM_IPI_RAW_META_STATS_1:
		stats1 = (struct mtk_cam_uapi_meta_raw_stats_1 *)vaddr;
		offset = sizeof(*stats1);
		set_payload(&stats1->af_stats.afo_buf, MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, &offset);
		break;
	case MTKCAM_IPI_RAW_META_STATS_2:
		//todo
		pr_info("stats 2 not support");
		break;
	default:
		pr_debug("%s: dma_port err\n", __func__);
		break;
	}
}

/* TODO: support camsv meta header */
#if PDAF_READY
static void camsys_sv_set_meta_stats_info(
	u32 dma_port, void *vaddr, unsigned int width,
	unsigned int height, unsigned int stride)
{
	struct mtk_cam_uapi_meta_camsv_stats_0 *sv_stats0;
	unsigned long offset;
	unsigned int size;

	switch (dma_port) {
	case MTKCAM_IPI_CAMSV_MAIN_OUT:
		size = stride * height;
		sv_stats0 = (struct mtk_cam_uapi_meta_camsv_stats_0 *)vaddr;
		offset = sizeof(*sv_stats0);
		set_payload(&sv_stats0->pd_stats.pdo_buf, size, &offset);
		sv_stats0->pd_stats_enabled = 1;
		sv_stats0->pd_stats.stats_src.width = width;
		sv_stats0->pd_stats.stats_src.height = height;
		sv_stats0->pd_stats.stride = stride;
		break;
	default:
		pr_debug("%s: dma_port err\n", __func__);
		break;
	}
}
#endif

static int camsys_get_meta_version(bool major)
{
	if (major)
		return MTK_CAM_META_VERSION_MAJOR;
	else
		return MTK_CAM_META_VERSION_MINOR;
}

static int camsys_get_meta_size(u32 video_id)
{
	switch (video_id) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
		return RAW_STATS_CFG_SIZE;
	case MTKCAM_IPI_RAW_META_STATS_0:
		return RAW_STATS_0_SIZE;
	case MTKCAM_IPI_RAW_META_STATS_1:
		return RAW_STATS_1_SIZE;
	case MTKCAM_IPI_RAW_META_STATS_2:
		return RAW_STATS_2_SIZE;
	case MTKCAM_IPI_CAMSV_MAIN_OUT:
		return SV_STATS_0_SIZE;
	default:
		pr_debug("%s: no support stats(%d)\n", __func__, video_id);
	}
	return 0;
}

static const struct mtk_cam_format_desc *camsys_get_meta_fmts(void)
{
	return meta_fmts;
}

static int camsys_get_port_bw(
	enum MMQOS_PORT port, unsigned long height, unsigned long fps)
{
	switch (port) {
	case AAO:
		return (MTK_CAM_UAPI_AAO_MAX_BUF_SIZE + MTK_CAM_UAPI_AFO_MAX_BUF_SIZE) * fps;
	case AAHO:
		return MTK_CAM_UAPI_AAHO_HIST_SIZE * fps;
	case TSFSO:
		return (MTK_CAM_UAPI_TSFSO_SIZE * 2 + MTK_CAM_UAPI_LTMSO_SIZE) * fps;
	case FLKO:
		return MTK_CAM_UAPI_FLK_BLK_SIZE * MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM * height * fps;
	default:
		pr_debug("%s: no support port(%d)\n", __func__, port);
	}

	return 0;
}

static bool camsys_support_AFO_independent(unsigned long fps)
{
	(void) fps;
	return true;
}

static struct camsys_plat_fp plat_fp = {
	.get_meta_version = camsys_get_meta_version,
	.get_meta_size = camsys_get_meta_size,
	.get_meta_fmts = camsys_get_meta_fmts,
	.set_meta_stats_info = camsys_set_meta_stats_info,
#if PDAF_READY
	.set_sv_meta_stats_info = camsys_sv_set_meta_stats_info,
#endif
	.get_port_bw = camsys_get_port_bw,
	.get_timestamp_addr = camsys_get_timestamp_addr,
	.support_AFO_independent = camsys_support_AFO_independent,
};

static int __init plat_module_init(void)
{
	pr_info("plat init\n");

	mtk_cam_set_plat_util(&plat_fp);

	return 0;
}

module_init(plat_module_init);
MODULE_DESCRIPTION("Mediatek Camsys plat driver");
MODULE_LICENSE("GPL v2");

// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>

#include "mtk_cam-plat.h"
#include "mtk_cam-meta-mt6985.h"
#include "mtk_cam-ipi.h"

#define RAW_STATS_CFG_SIZE \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_cfg), SZ_1K)

/* meta out max size include 1k meta info and dma buffer size */
#define RAW_STATS_0_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_0), SZ_1K) + \
	      MTK_CAM_UAPI_AAO_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_LTMSO_SIZE + \
	      MTK_CAM_UAPI_LTMSHO_SIZE + \
	      MTK_CAM_UAPI_FLK_MAX_BUF_SIZE + \
	      MTK_CAM_UAPI_TSFSO_SIZE * 2 + \
	      MTK_CAM_UAPI_TCYSO_SIZE \
	      , (4 * SZ_1K))

#define RAW_STATS_1_SIZE \
	ALIGN(ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_1), SZ_1K) + \
	      MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, (4 * SZ_1K))

#define SV_STATS_0_SIZE \
	sizeof(struct mtk_cam_uapi_meta_camsv_stats_0)

static void set_payload(struct mtk_cam_uapi_meta_hw_buf *buf,
			unsigned int size, size_t *offset)
{
	buf->offset = *offset;
	buf->size = size;
	*offset += size;
}

static int set_meta_stat0_info(struct mtk_cam_uapi_meta_raw_stats_0 *stats,
	unsigned int pdo_max_size)
{
	size_t offset = sizeof(*stats);

	set_payload(&stats->ae_awb_stats.aao_buf,
		    MTK_CAM_UAPI_AAO_MAX_BUF_SIZE, &offset);
	set_payload(&stats->ae_awb_stats.aaho_buf,
		    MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE, &offset);
	set_payload(&stats->ltm_stats.ltmso_buf,
		    MTK_CAM_UAPI_LTMSO_SIZE, &offset);
	set_payload(&stats->ltm_stats.ltmsho_buf,
		    MTK_CAM_UAPI_LTMSHO_SIZE, &offset);
	set_payload(&stats->flk_stats.flko_buf,
		    MTK_CAM_UAPI_FLK_MAX_BUF_SIZE, &offset);
	set_payload(&stats->tsf_stats.tsfo_r1_buf,
		    MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	set_payload(&stats->tsf_stats.tsfo_r2_buf,
		    MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	set_payload(&stats->tcys_stats.tcyso_buf,
		    MTK_CAM_UAPI_TCYSO_SIZE, &offset);
	if (pdo_max_size > 0)
		set_payload(&stats->pde_stats.pdo_buf, pdo_max_size, &offset);

	/* w part */
	set_payload(&stats->ae_awb_stats_w.aao_buf, 0, &offset);
	set_payload(&stats->ae_awb_stats_w.aaho_buf, 0, &offset);
	set_payload(&stats->ltm_stats_w.ltmso_buf, 0, &offset);
	set_payload(&stats->ltm_stats_w.ltmsho_buf, 0, &offset);
	set_payload(&stats->flk_stats_w.flko_buf, 0, &offset);
	set_payload(&stats->tsf_stats_w.tsfo_r1_buf, 0, &offset);
	set_payload(&stats->tsf_stats_w.tsfo_r2_buf, 0, &offset);
	set_payload(&stats->tcys_stats_w.tcyso_buf, 0, &offset);
	if (pdo_max_size > 0)
		set_payload(&stats->pde_stats_w.pdo_buf, 0, &offset);

	return 0;
}

static int set_meta_stat1_info(struct mtk_cam_uapi_meta_raw_stats_1 *stats)
{
	size_t offset = sizeof(*stats);

	set_payload(&stats->af_stats.afo_buf,
		    MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, &offset);

	/* w part */
	set_payload(&stats->af_stats_w.afo_buf, 0, &offset);

	return 0;
}

static int set_meta_stats_info(int ipi_id, void *addr, unsigned int pdo_max_size)
{
	if (WARN_ON(!addr))
		return -1;

	switch (ipi_id) {
	case MTKCAM_IPI_RAW_META_STATS_0: return set_meta_stat0_info(addr, pdo_max_size);
	case MTKCAM_IPI_RAW_META_STATS_1: return set_meta_stat1_info(addr);
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, ipi_id);
		break;
	}
	return -1;
}

static int set_sv_meta_stats_info(
	int ipi_id, void *addr, unsigned int width,
	unsigned int height, unsigned int stride)
{
	struct mtk_cam_uapi_meta_camsv_stats_0 *sv_stats0;
	unsigned long offset;
	unsigned int size;

	switch (ipi_id) {
	case MTKCAM_IPI_CAMSV_MAIN_OUT:
		size = stride * height;
		sv_stats0 = (struct mtk_cam_uapi_meta_camsv_stats_0 *)addr;
		/* calculate offset for 16-alignment limitation */
		offset = ((((dma_addr_t)sv_stats0 + SV_STATS_0_SIZE + 15) >> 4) << 4)
			- (dma_addr_t)sv_stats0;
		set_payload(&sv_stats0->pd_stats.pdo_buf, size, &offset);
		sv_stats0->pd_stats_enabled = 1;
		sv_stats0->pd_stats.stats_src.width = width;
		sv_stats0->pd_stats.stats_src.height = height;
		sv_stats0->pd_stats.stride = stride;
		break;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, ipi_id);
		break;
	}
	return -1;
}

static int get_port_bw(
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
		pr_info("%s: no support port(%d)\n", __func__, port);
	}

	return 0;
}

static const struct plat_v4l2_data mt6985_v4l2_data = {
	.raw_pipeline_num = 3,
	.camsv_pipeline_num = 0,
	.mraw_pipeline_num = 0,

	.meta_major = MTK_CAM_META_VERSION_MAJOR,
	.meta_minor = MTK_CAM_META_VERSION_MINOR,

	.meta_cfg_size = RAW_STATS_CFG_SIZE,
	.meta_stats0_size = RAW_STATS_0_SIZE,
	.meta_stats1_size = RAW_STATS_1_SIZE,
	.meta_sv_ext_size = SV_STATS_0_SIZE,

	.timestamp_buffer_ofst = offsetof(struct mtk_cam_uapi_meta_raw_stats_0,
					  timestamp),
	.support_afo_independent = true,

	.set_meta_stats_info = set_meta_stats_info,

	.set_sv_meta_stats_info = set_sv_meta_stats_info,

	.get_port_bw = get_port_bw,
};

struct camsys_platform_data mt6985_data = {
	.platform = "mt6985",
	.v4l2 = &mt6985_v4l2_data,
	.hw = NULL,
};

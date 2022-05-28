// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>

#include "mtk_cam-plat.h"
#include "mtk_cam-meta-mt6983.h"
#include "mtk_cam-ipi_7_1.h"

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

static void set_payload(struct mtk_cam_uapi_meta_hw_buf *buf,
			unsigned int size, size_t *offset)
{
	buf->offset = *offset;
	buf->size = size;
	*offset += size;
}

static int set_meta_stat0_info(struct mtk_cam_uapi_meta_raw_stats_0 *stats)
{
	size_t offset = sizeof(*stats);

	set_payload(&stats->ae_awb_stats.aao_buf,
		    MTK_CAM_UAPI_AAO_MAX_BUF_SIZE, &offset);
	set_payload(&stats->ae_awb_stats.aaho_buf,
		    MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE, &offset);
	set_payload(&stats->ltm_stats.ltmso_buf,
		    MTK_CAM_UAPI_LTMSO_SIZE, &offset);
	set_payload(&stats->flk_stats.flko_buf,
		    MTK_CAM_UAPI_FLK_MAX_BUF_SIZE, &offset);
	set_payload(&stats->tsf_stats.tsfo_r1_buf,
		    MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	set_payload(&stats->tsf_stats.tsfo_r2_buf,
		    MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	set_payload(&stats->tncy_stats.tncsyo_buf,
		    MTK_CAM_UAPI_TNCSYO_SIZE, &offset);
	return 0;
}

static int set_meta_stat1_info(struct mtk_cam_uapi_meta_raw_stats_1 *stats)
{
	size_t offset = sizeof(*stats);

	set_payload(&stats->af_stats.afo_buf,
		    MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, &offset);
	return 0;
}

static int set_meta_stats_info(int ipi_id, void *addr)
{
	if (WARN_ON(!addr))
		return -1;

	switch (ipi_id) {
	case MTKCAM_IPI_RAW_META_STATS_0: return set_meta_stat0_info(addr);
	case MTKCAM_IPI_RAW_META_STATS_1: return set_meta_stat1_info(addr);
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, ipi_id);
		break;
	}
	return -1;
}

static const struct plat_v4l2_data mt6985_v4l2_data = {
	.raw_pipeline_num = 3,
	.camsv_pipeline_num = 16,
	.mraw_pipeline_num = 0,

	.meta_major = MTK_CAM_META_VERSION_MAJOR,
	.meta_minor = MTK_CAM_META_VERSION_MINOR,

	.meta_cfg_size = RAW_STATS_CFG_SIZE,
	.meta_stats0_size = RAW_STATS_0_SIZE,
	.meta_stats1_size = RAW_STATS_1_SIZE,
	.meta_stats2_size = RAW_STATS_2_SIZE,
	.meta_sv_ext_size = SV_STATS_0_SIZE,

	.timestamp_buffer_ofst = offsetof(struct mtk_cam_uapi_meta_raw_stats_0,
					  timestamp),

	.set_meta_stats_info = set_meta_stats_info,
};

struct camsys_platform_data mt6985_data = {
	.platform = "mt6985",
	.v4l2 = &mt6985_v4l2_data,
	.hw = NULL,
};

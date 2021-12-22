/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_PLAT_UTIL_H
#define __MTK_CAM_PLAT_UTIL_H

#define PDAF_READY 1

enum MMQOS_PORT {
	AAO = 0,
	AAHO,
	TSFSO,
	FLKO,
};

typedef int (*plat_camsys_get_meta_version)(bool major);

typedef int (*plat_camsys_get_meta_size)(u32 video_id);

typedef const struct mtk_cam_format_desc* (*plat_camsys_get_meta_fmts)(void);

typedef void (*plat_camsys_set_meta_stats_info)(
			u32 dma_port, void *vaddr,
			struct mtk_raw_pde_config *pde_cfg);

#if PDAF_READY
typedef void (*plat_camsys_sv_set_meta_stats_info)(
			u32 dma_port, void *vaddr,
			unsigned int width, unsigned int height,
			unsigned int stride);
#endif

typedef int (*plat_camsys_get_port_bw)(
			enum MMQOS_PORT port,
			unsigned long height, unsigned long fps);

typedef uint64_t* (*plat_camsys_get_timestamp_addr) (
			void *vaddr);

typedef bool (*plat_camsys_support_AFO_independent)(
			unsigned long fps);

struct camsys_plat_fp {
	plat_camsys_get_meta_version get_meta_version;
	plat_camsys_get_meta_size get_meta_size;
	plat_camsys_get_meta_fmts get_meta_fmts;
	plat_camsys_set_meta_stats_info set_meta_stats_info;
#if PDAF_READY
	plat_camsys_sv_set_meta_stats_info set_sv_meta_stats_info;
#endif
	plat_camsys_get_port_bw get_port_bw;
	plat_camsys_get_timestamp_addr get_timestamp_addr;
	plat_camsys_support_AFO_independent support_AFO_independent;
};

void mtk_cam_set_plat_util(struct camsys_plat_fp *plat_fp);

int mtk_cam_get_meta_version(bool major);

int mtk_cam_get_meta_size(u32 video_id);

const struct mtk_cam_format_desc *mtk_cam_get_meta_fmts(void);

void mtk_cam_set_meta_stats_info(
		u32 dma_port, void *vaddr,
		struct mtk_raw_pde_config *pde_cfg);

void mtk_cam_set_sv_meta_stats_info(
		u32 dma_port, void *vaddr, unsigned int width,
		unsigned int height, unsigned int stride);

int mtk_cam_get_port_bw(
		enum MMQOS_PORT port,
		unsigned long height, unsigned long fps);

uint64_t *mtk_cam_get_timestamp_addr(void *vaddr);

bool mtk_cam_support_AFO_independent(unsigned long fps);

#endif /*__MTK_CAM_PLAT_UTIL_H*/

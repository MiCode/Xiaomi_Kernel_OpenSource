// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/init.h>

#include "mtk_cam-raw.h"
#include "mtk_cam-plat-util.h"

struct camsys_plat_fp *plat_fp;

void mtk_cam_set_plat_util(struct camsys_plat_fp *fp)
{
	if (!fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return;
	}
	plat_fp = fp;
}
EXPORT_SYMBOL(mtk_cam_set_plat_util);

int mtk_cam_get_meta_version(bool major)
{
	if (!plat_fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return 0;
	}
	return plat_fp->get_meta_version(major);
}
EXPORT_SYMBOL(mtk_cam_get_meta_version);

int mtk_cam_get_meta_size(u32 video_id)
{
	if (!plat_fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return 0;
	}
	return plat_fp->get_meta_size(video_id);
}
EXPORT_SYMBOL(mtk_cam_get_meta_size);

const struct mtk_cam_format_desc *mtk_cam_get_meta_fmts(void)
{
	if (!plat_fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return 0;
	}
	return plat_fp->get_meta_fmts();
}
EXPORT_SYMBOL(mtk_cam_get_meta_fmts);

void mtk_cam_set_meta_stats_info(u32 dma_port, void *vaddr,
				 struct mtk_raw_pde_config *pde_cfg)
{
	if (!plat_fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return;
	}
	plat_fp->set_meta_stats_info(dma_port, vaddr, pde_cfg);
}
EXPORT_SYMBOL(mtk_cam_set_meta_stats_info);

void mtk_cam_set_sv_meta_stats_info(
		u32 dma_port, void *vaddr,
		unsigned int width, unsigned int height,
		unsigned int stride)
{
	if (!plat_fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return;
	}
	plat_fp->set_sv_meta_stats_info(
		dma_port, vaddr, width, height, stride);
}
EXPORT_SYMBOL(mtk_cam_set_sv_meta_stats_info);

int mtk_cam_get_port_bw(
		enum MMQOS_PORT port,
		unsigned long height, unsigned long fps)
{
	if (!plat_fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return 0;
	}
	return plat_fp->get_port_bw(port, height, fps);
}
EXPORT_SYMBOL(mtk_cam_get_port_bw);

uint64_t *mtk_cam_get_timestamp_addr(void *vaddr)
{
	if (!plat_fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return 0;
	}
	return plat_fp->get_timestamp_addr(vaddr);
}
EXPORT_SYMBOL(mtk_cam_get_timestamp_addr);

bool mtk_cam_support_AFO_independent(unsigned long fps)
{
	if (!plat_fp) {
		pr_info("%s platform fp is NULL ", __func__);
		return 0;
	}
	return plat_fp->support_AFO_independent(fps);
}
EXPORT_SYMBOL(mtk_cam_support_AFO_independent);

static int __init util_module_init(void)
{
	pr_info("platform util init\n");
	return 0;
}

module_init(util_module_init);
MODULE_DESCRIPTION("Mediatek Camsys plat util driver");
MODULE_LICENSE("GPL v2");

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_PLAT_H
#define __MTK_CAM_PLAT_H

#include <linux/types.h>
#include <linux/media.h>

enum MMQOS_PORT {
	AAO = 0,
	AAHO,
	TSFSO,
	LTMSO,
	FLKO,
	TCYSO,
};

enum raw_qos_port_id {
	cqi_r1 = 0,
	rawi_r2,
	rawi_r3,
	rawi_r5,
	imgo_r1,
	bpci_r1, /* 5 */
	lsci_r1,
	ufeo_r1,
	ltmso_r1,
	drzb2no_r1,
	aao_r1, /* 10 */
	afo_r1,
	yuvo_r1,
	yuvo_r3,
	yuvo_r2,
	yuvo_r5, /* 15 */
	rgbwi_r1,
	tcyso_r1,
	drz4no_r3,
	raw_qos_port_num
};

enum mraw_dmao_id {
	imgo_m1 = 0,
	imgbo_m1,
	cpio_m1,
	mraw_dmao_num
};

struct mraw_stats_cfg_param {
	s8  mqe_en;
	s8  mobc_en;
	s8  plsc_en;

	u32 crop_width;
	u32 crop_height;

	u32 mqe_mode;

	u32 mbn_hei;
	u32 mbn_pow;
	u32 mbn_dir;
	u32 mbn_spar_hei;
	u32 mbn_spar_pow;
	u32 mbn_spar_fac;
	u32 mbn_spar_con1;
	u32 mbn_spar_con0;

	u32 cpi_th;
	u32 cpi_pow;
	u32 cpi_dir;
	u32 cpi_spar_hei;
	u32 cpi_spar_pow;
	u32 cpi_spar_fac;
	u32 cpi_spar_con1;
	u32 cpi_spar_con0;
};

struct raw_mmqos {
	char *port[raw_qos_port_num];
};

struct dma_info {
	u32 width;
	u32 height;
	u32 xsize;
	u32 stride;
};

struct plat_v4l2_data {
	int raw_pipeline_num;
	int camsv_pipeline_num;
	int mraw_pipeline_num;
	u32 camsys_axi_mux;

	unsigned int meta_major;
	unsigned int meta_minor;

	int meta_cfg_size;
	int meta_cfg_size_rgbw;
	int meta_stats0_size;
	int meta_stats0_size_rgbw;
	int meta_stats1_size;
	int meta_stats1_size_rgbw;
	int meta_sv_ext_size;
	int meta_mraw_ext_size;

	int timestamp_buffer_ofst;
	bool support_afo_independent;

	int (*set_meta_stats_info)(int ipi_id, void *addr, unsigned int pdo_max_size,
							   bool is_rgbw);

	int (*set_sv_meta_stats_info)(int ipi_id, void *addr, struct dma_info *info);

	int (*set_mraw_meta_stats_info)(int ipi_id, void *addr, struct dma_info *info);

	u32 (*get_dev_link_flags)(int ipi_id);

	int (*get_port_bw)(enum MMQOS_PORT port,
		unsigned long height, unsigned long fps);
	int cammux_id_raw_start;
	int (*get_mmqos_port)(struct raw_mmqos **mmqos_port);

	int (*get_mraw_stats_cfg_param)(void *addr, struct mraw_stats_cfg_param *param);
	int reserved_camsv_dev_id;
};

struct plat_data_hw {
};

struct camsys_platform_data {
	const char			platform[8];
	const struct plat_v4l2_data	*v4l2;
	const struct plat_data_hw	*hw;
};

extern const struct camsys_platform_data *cur_platform;
void set_platform_data(const struct camsys_platform_data *platform_data);

#define CALL_PLAT_V4L2(ops, ...) \
	((cur_platform && cur_platform->v4l2 && cur_platform->v4l2->ops) ? \
	 cur_platform->v4l2->ops(__VA_ARGS__) : -EINVAL)

#define GET_PLAT_V4L2(member) (cur_platform->v4l2->member)

/* platform data list */
#ifdef CAMSYS_ISP7S_MT6985
extern struct camsys_platform_data mt6985_data;
#endif
#ifdef CAMSYS_ISP7S_MT6886
extern struct camsys_platform_data mt6886_data;
#endif
#endif /*__MTK_CAM_PLAT_H*/

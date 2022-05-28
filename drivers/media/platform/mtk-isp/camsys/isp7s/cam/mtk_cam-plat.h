/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_PLAT_H
#define __MTK_CAM_PLAT_H

struct plat_v4l2_data {
	int raw_pipeline_num;
	int camsv_pipeline_num;
	int mraw_pipeline_num;

	unsigned int meta_major;
	unsigned int meta_minor;

	int meta_cfg_size;
	int meta_stats0_size;
	int meta_stats1_size;
	int meta_stats2_size;
	int meta_sv_ext_size;

	int timestamp_buffer_ofst;

	int (*set_meta_stats_info)(int ipi_id, void *addr);
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

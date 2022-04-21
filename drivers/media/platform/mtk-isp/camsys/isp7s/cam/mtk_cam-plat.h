/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_PLAT_H
#define __MTK_CAM_PLAT_H

struct plat_data_meta {
	int	(*get_version)(struct plat_data_meta *self,
			       unsigned int *major, unsigned int *minor);
	int	(*get_size)(struct plat_data_meta *self,
			    unsigned int video_id);
};

struct plat_data_hw {
};

struct camsys_platform_data {
	const char			platform[8];
	const struct plat_data_meta	*meta;
	const struct plat_data_hw	*hw;
};

extern const struct camsys_platform_data *cur_platform;
void set_platform_data(const struct camsys_platform_data *platform_data);

#define CALL_PLAT_META(ops, ...) \
	((cur_platform && cur_platform->meta && cur_platform->meta->ops) ? \
	 cur_platform->meta->ops(platform->meta, ##__VA_ARGS__) : -EINVAL)

#define CALL_PLAT_HW(ops, ...) \
	((cur_platform && cur_platform->hw && cur_platform->hw->ops) ? \
	 cur_platform->hw->ops(platform->hw, ##__VA_ARGS__) : -EINVAL)

/* platform data list */
extern struct camsys_platform_data mt6985_data;


#endif /*__MTK_CAM_PLAT_H*/

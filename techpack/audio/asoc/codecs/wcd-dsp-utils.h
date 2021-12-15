/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 */

#ifndef __WCD_DSP_UTILS_H__
#define __WCD_DSP_UTILS_H__

#define WDSP_IMG_NAME_LEN_MAX    64

#define WDSP_ELF_FLAG_EXECUTE    (1 << 0)
#define WDSP_ELF_FLAG_WRITE      (1 << 1)
#define WDSP_ELF_FLAG_READ       (1 << 2)

#define WDSP_ELF_FLAG_RE (WDSP_ELF_FLAG_READ | WDSP_ELF_FLAG_EXECUTE)

struct wdsp_img_segment {

	/* Firmware for the slit image */
	const struct firmware *split_fw;

	/* Name of the split firmware file */
	char split_fname[WDSP_IMG_NAME_LEN_MAX];

	/* Address where the segment is to be loaded */
	u32 load_addr;

	/* Buffer to hold the data to be loaded */
	u8 *data;

	/* Size of the data to be loaded */
	size_t size;

	/* List node pointing to next segment */
	struct list_head list;
};

int wdsp_get_segment_list(struct device *dev, const char *img_fname,
			  unsigned int segment_type, struct list_head *seg_list,
			  u32 *entry_point);
void wdsp_flush_segment_list(struct list_head *seg_list);

#endif /* __WCD_DSP_UTILS_H__ */

/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

int wdsp_get_segment_list(struct device *, const char *,
			  unsigned int, struct list_head *,
			  u32 *);
void wdsp_flush_segment_list(struct list_head *);

#endif /* __WCD_DSP_UTILS_H__ */

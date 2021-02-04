/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <mt-plat/mtk_smi.h>
#include "smi_info_util.h"
#include "smi_common.h"

struct MTK_SMI_BWC_MM_INFO g_smi_bwc_mm_info = {
	0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 0, 0, 0,
	SF_HWC_PIXEL_MAX_NORMAL
};

int smi_set_mm_info_ioctl_wrapper(struct file *pFile, unsigned int cmd,
	unsigned long param)
{
	int ret = 0;
	struct MTK_SMI_BWC_INFO_SET cfg;

	ret = copy_from_user(&cfg, (void *)param,
		sizeof(struct MTK_SMI_BWC_INFO_SET));
	if (ret) {
		SMIMSG("IOC_SMI_BWC_INFO_SET, copy_to_user failed: %d\n", ret);
		return -EFAULT;
	}
	/* Set the address to the value assigned by user space program */
	smi_bwc_mm_info_set(cfg.property, cfg.value1, cfg.value2);
	/* SMIMSG("Handle MTK_IOC_SMI_BWC_INFO_SET request... finish"); */
	return ret;
}

int smi_get_mm_info_ioctl_wrapper(struct file *pFile, unsigned int cmd,
	unsigned long param)
{
	int ret = 0;

	ret = copy_to_user((void *)param, (void *)&g_smi_bwc_mm_info,
		sizeof(struct MTK_SMI_BWC_MM_INFO));

	if (ret) {
		SMIMSG("IOC_SMI_BWC_INFO_GET copy_to_user failed: %d\n", ret);
		return -EFAULT;
	}
	/* SMIMSG("Handle MTK_IOC_SMI_BWC_INFO_GET request... finish"); */
	return ret;
}

void smi_bwc_mm_info_set(int property_id, long val1, long val2)
{
	switch (property_id) {
	case SMI_BWC_INFO_CON_PROFILE:
		g_smi_bwc_mm_info.concurrent_profile = (int)val1;
		break;
	case SMI_BWC_INFO_SENSOR_SIZE:
		g_smi_bwc_mm_info.sensor_size[0] = val1;
		g_smi_bwc_mm_info.sensor_size[1] = val2;
		break;
	case SMI_BWC_INFO_VIDEO_RECORD_SIZE:
		g_smi_bwc_mm_info.video_record_size[0] = val1;
		g_smi_bwc_mm_info.video_record_size[1] = val2;
		break;
	case SMI_BWC_INFO_DISP_SIZE:
		g_smi_bwc_mm_info.display_size[0] = val1;
		g_smi_bwc_mm_info.display_size[1] = val2;
		break;
	case SMI_BWC_INFO_TV_OUT_SIZE:
		g_smi_bwc_mm_info.tv_out_size[0] = val1;
		g_smi_bwc_mm_info.tv_out_size[1] = val2;
		break;
	case SMI_BWC_INFO_FPS:
		g_smi_bwc_mm_info.fps = (int)val1;
		break;
	case SMI_BWC_INFO_VIDEO_ENCODE_CODEC:
		g_smi_bwc_mm_info.video_encode_codec = (int)val1;
		break;
	case SMI_BWC_INFO_VIDEO_DECODE_CODEC:
		g_smi_bwc_mm_info.video_decode_codec = (int)val1;
		break;
	}
}

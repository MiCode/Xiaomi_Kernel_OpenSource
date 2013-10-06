/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/types.h>
#include <mach/board.h>
#include "mdss_hdmi_edid.h"

#define DBC_START_OFFSET 4
#define HDMI_VSDB_3D_EVF_DATA_OFFSET(vsd) \
	(!((vsd)[8] & BIT(7)) ? 9 : (!((vsd)[8] & BIT(6)) ? 11 : 13))

/*
 * As per the CEA-861E spec, there can be a total of 10 short audio
 * descriptors with each SAD being 3 bytes long.
 * Thus, the maximum length of the audio data block would be 30 bytes
 */
#define MAX_AUDIO_DATA_BLOCK_SIZE	30
#define MAX_SPKR_ALLOC_DATA_BLOCK_SIZE	3

/* Support for first 5 EDID blocks */
#define MAX_EDID_BLOCK_SIZE (0x80 * 5)

struct hdmi_edid_sink_data {
	u32 disp_mode_list[HDMI_VFRMT_MAX];
	u32 disp_3d_mode_list[HDMI_VFRMT_MAX];
	u32 disp_multi_3d_mode_list[16];
	u32 disp_multi_3d_mode_list_cnt;
	u32 num_of_elements;
	u32 preferred_video_format;
};

struct hdmi_edid_ctrl {
	u8 pt_scan_info;
	u8 it_scan_info;
	u8 ce_scan_info;
	u16 physical_address;
	u32 video_resolution; /* selected by user */
	u32 sink_mode; /* HDMI or DVI */
	u16 audio_latency;
	u16 video_latency;
	u32 present_3d;
	u8 audio_data_block[MAX_AUDIO_DATA_BLOCK_SIZE];
	int adb_size;
	u8 spkr_alloc_data_block[MAX_SPKR_ALLOC_DATA_BLOCK_SIZE];
	int sadb_size;
	u8 edid_buf[MAX_EDID_BLOCK_SIZE];

	struct hdmi_edid_sink_data sink_data;
	struct hdmi_edid_init_data init_data;
};

/* The Logic ID for HDMI TX Core. Currently only support 1 HDMI TX Core. */
struct hdmi_edid_video_mode_property_type {
	u32	video_code;
	u32	active_h;
	u32	active_v;
	u32	interlaced;
	u32	total_h;
	u32	total_blank_h;
	u32	total_v;
	u32	total_blank_v;
	/* Must divide by 1000 to get the frequency */
	u32	freq_h;
	/* Must divide by 1000 to get the frequency */
	u32	freq_v;
	/* Must divide by 1000 to get the frequency */
	u32	pixel_freq;
	/* Must divide by 1000 to get the frequency */
	u32	refresh_rate;
	u32	aspect_ratio_4_3;
};

/* LUT is sorted from lowest Active H to highest Active H - ease searching */
static struct hdmi_edid_video_mode_property_type
	hdmi_edid_disp_mode_lut[] = {

	/* All 640 H Active */
	{HDMI_VFRMT_640x480p60_4_3, 640, 480, false, 800, 160, 525, 45,
	 31465, 59940, 25175, 59940, true},
	{HDMI_VFRMT_640x480p60_4_3, 640, 480, false, 800, 160, 525, 45,
	 31500, 60000, 25200, 60000, true},

	/* All 720 H Active */
	{HDMI_VFRMT_720x576p50_4_3,  720, 576, false, 864, 144, 625, 49,
	 31250, 50000, 27000, 50000, true},
	{HDMI_VFRMT_720x480p60_4_3,  720, 480, false, 858, 138, 525, 45,
	 31465, 59940, 27000, 59940, true},
	{HDMI_VFRMT_720x480p60_4_3,  720, 480, false, 858, 138, 525, 45,
	 31500, 60000, 27030, 60000, true},
	{HDMI_VFRMT_720x576p100_4_3, 720, 576, false, 864, 144, 625, 49,
	 62500, 100000, 54000, 100000, true},
	{HDMI_VFRMT_720x480p120_4_3, 720, 480, false, 858, 138, 525, 45,
	 62937, 119880, 54000, 119880, true},
	{HDMI_VFRMT_720x480p120_4_3, 720, 480, false, 858, 138, 525, 45,
	 63000, 120000, 54054, 120000, true},
	{HDMI_VFRMT_720x576p200_4_3, 720, 576, false, 864, 144, 625, 49,
	 125000, 200000, 108000, 200000, true},
	{HDMI_VFRMT_720x480p240_4_3, 720, 480, false, 858, 138, 525, 45,
	 125874, 239760, 108000, 239000, true},
	{HDMI_VFRMT_720x480p240_4_3, 720, 480, false, 858, 138, 525, 45,
	 126000, 240000, 108108, 240000, true},

	/* All 1280 H Active */
	{HDMI_VFRMT_1280x720p50_16_9,  1280, 720, false, 1980, 700, 750, 30,
	 37500, 50000, 74250, 50000, false},
	{HDMI_VFRMT_1280x720p60_16_9,  1280, 720, false, 1650, 370, 750, 30,
	 44955, 59940, 74176, 59940, false},
	{HDMI_VFRMT_1280x720p60_16_9,  1280, 720, false, 1650, 370, 750, 30,
	 45000, 60000, 74250, 60000, false},
	{HDMI_VFRMT_1280x720p100_16_9, 1280, 720, false, 1980, 700, 750, 30,
	 75000, 100000, 148500, 100000, false},
	{HDMI_VFRMT_1280x720p120_16_9, 1280, 720, false, 1650, 370, 750, 30,
	 89909, 119880, 148352, 119880, false},
	{HDMI_VFRMT_1280x720p120_16_9, 1280, 720, false, 1650, 370, 750, 30,
	 90000, 120000, 148500, 120000, false},
	{HDMI_VFRMT_1280x1024p60_5_4, 1280, 1024, false, 1688, 408, 1066, 42,
	 63981, 60020, 108000, 60000, false},

	/* All 1024 H Active */
	{HDMI_VFRMT_1024x768p60_4_3, 1024, 768, false, 1344, 320, 806, 38,
	 48363, 60004, 65000, 60000, false},

	/* All 1440 H Active */
	{HDMI_VFRMT_1440x576i50_4_3, 1440, 576, true,  1728, 288, 625, 24,
	 15625, 50000, 27000, 50000, true},
	{HDMI_VFRMT_720x288p50_4_3,  1440, 288, false, 1728, 288, 312, 24,
	 15625, 50080, 27000, 50000, true},
	{HDMI_VFRMT_720x288p50_4_3,  1440, 288, false, 1728, 288, 313, 25,
	 15625, 49920, 27000, 50000, true},
	{HDMI_VFRMT_720x288p50_4_3,  1440, 288, false, 1728, 288, 314, 26,
	 15625, 49761, 27000, 50000, true},
	{HDMI_VFRMT_1440x576p50_4_3, 1440, 576, false, 1728, 288, 625, 49,
	 31250, 50000, 54000, 50000, true},
	{HDMI_VFRMT_1440x480i60_4_3, 1440, 480, true,  1716, 276, 525, 22,
	 15734, 59940, 27000, 59940, true},
	{HDMI_VFRMT_1440x240p60_4_3, 1440, 240, false, 1716, 276, 262, 22,
	 15734, 60054, 27000, 59940, true},
	{HDMI_VFRMT_1440x240p60_4_3, 1440, 240, false, 1716, 276, 263, 23,
	 15734, 59826, 27000, 59940, true},
	{HDMI_VFRMT_1440x480p60_4_3, 1440, 480, false, 1716, 276, 525, 45,
	 31469, 59940, 54000, 59940, true},
	{HDMI_VFRMT_1440x480i60_4_3, 1440, 480, true,  1716, 276, 525, 22,
	 15750, 60000, 27027, 60000, true},
	{HDMI_VFRMT_1440x240p60_4_3, 1440, 240, false, 1716, 276, 262, 22,
	 15750, 60115, 27027, 60000, true},
	{HDMI_VFRMT_1440x240p60_4_3, 1440, 240, false, 1716, 276, 263, 23,
	 15750, 59886, 27027, 60000, true},
	{HDMI_VFRMT_1440x480p60_4_3, 1440, 480, false, 1716, 276, 525, 45,
	 31500, 60000, 54054, 60000, true},
	{HDMI_VFRMT_1440x576i100_4_3, 1440, 576, true,  1728, 288, 625, 24,
	 31250, 100000, 54000, 100000, true},
	{HDMI_VFRMT_1440x480i120_4_3, 1440, 480, true,  1716, 276, 525, 22,
	 31469, 119880, 54000, 119880, true},
	{HDMI_VFRMT_1440x480i120_4_3, 1440, 480, true,  1716, 276, 525, 22,
	 31500, 120000, 54054, 120000, true},
	{HDMI_VFRMT_1440x576i200_4_3, 1440, 576, true,  1728, 288, 625, 24,
	 62500, 200000, 108000, 200000, true},
	{HDMI_VFRMT_1440x480i240_4_3, 1440, 480, true,  1716, 276, 525, 22,
	 62937, 239760, 108000, 239000, true},
	{HDMI_VFRMT_1440x480i240_4_3, 1440, 480, true,  1716, 276, 525, 22,
	 63000, 240000, 108108, 240000, true},

	/* All 1920 H Active */
	{HDMI_VFRMT_1920x1080p60_16_9, 1920, 1080, false, 2200, 280, 1125,
	 45, 67433, 59940, 148352, 59940, false},
	{HDMI_VFRMT_1920x1080p60_16_9, 1920, 1080, true,  2200, 280, 1125,
	 45, 67500, 60000, 148500, 60000, false},
	{HDMI_VFRMT_1920x1080p50_16_9, 1920, 1080, false, 2640, 720, 1125,
	 45, 56250, 50000, 148500, 50000, false},
	{HDMI_VFRMT_1920x1080p24_16_9, 1920, 1080, false, 2750, 830, 1125,
	 45, 26973, 23976, 74176, 24000, false},
	{HDMI_VFRMT_1920x1080p24_16_9, 1920, 1080, false, 2750, 830, 1125,
	 45, 27000, 24000, 74250, 24000, false},
	{HDMI_VFRMT_1920x1080p25_16_9, 1920, 1080, false, 2640, 720, 1125,
	 45, 28125, 25000, 74250, 25000, false},
	{HDMI_VFRMT_1920x1080p30_16_9, 1920, 1080, false, 2200, 280, 1125,
	 45, 33716, 29970, 74176, 30000, false},
	{HDMI_VFRMT_1920x1080p30_16_9, 1920, 1080, false, 2200, 280, 1125,
	 45, 33750, 30000, 74250, 30000, false},
	{HDMI_VFRMT_1920x1080i50_16_9, 1920, 1080, true,  2304, 384, 1250,
	 85, 31250, 50000, 72000, 50000, false},
	{HDMI_VFRMT_1920x1080i60_16_9, 1920, 1080, true,  2200, 280, 1125,
	 22, 33716, 59940, 74176, 59940, false},
	{HDMI_VFRMT_1920x1080i60_16_9, 1920, 1080, true,  2200, 280, 1125,
	 22, 33750, 60000, 74250, 60000, false},
	{HDMI_VFRMT_1920x1080i100_16_9, 1920, 1080, true,  2640, 720, 1125,
	 22, 56250, 100000, 148500, 100000, false},
	{HDMI_VFRMT_1920x1080i120_16_9, 1920, 1080, true,  2200, 280, 1125,
	 22, 67432, 119880, 148352, 119980, false},
	{HDMI_VFRMT_1920x1080i120_16_9, 1920, 1080, true,  2200, 280, 1125,
	 22, 67500, 120000, 148500, 120000, false},

	/* All 2560 H Active */
	{HDMI_VFRMT_2560x1600p60_16_9, 2560, 1600, false, 2720, 160, 1646,
	 46, 98700, 60000, 268500, 60000, false},

	/* All 2880 H Active */
	{HDMI_VFRMT_2880x576i50_4_3, 2880, 576, true,  3456, 576, 625, 24,
	 15625, 50000, 54000, 50000, true},
	{HDMI_VFRMT_2880x288p50_4_3, 2880, 576, false, 3456, 576, 312, 24,
	 15625, 50080, 54000, 50000, true},
	{HDMI_VFRMT_2880x288p50_4_3, 2880, 576, false, 3456, 576, 313, 25,
	 15625, 49920, 54000, 50000, true},
	{HDMI_VFRMT_2880x288p50_4_3, 2880, 576, false, 3456, 576, 314, 26,
	 15625, 49761, 54000, 50000, true},
	{HDMI_VFRMT_2880x576p50_4_3, 2880, 576, false, 3456, 576, 625, 49,
	 31250, 50000, 108000, 50000, true},
	{HDMI_VFRMT_2880x480i60_4_3, 2880, 480, true,  3432, 552, 525, 22,
	 15734, 59940, 54000, 59940, true},
	{HDMI_VFRMT_2880x240p60_4_3, 2880, 480, false, 3432, 552, 262, 22,
	 15734, 60054, 54000, 59940, true},
	{HDMI_VFRMT_2880x240p60_4_3, 2880, 480, false, 3432, 552, 263, 23,
	 15734, 59940, 54000, 59940, true},
	{HDMI_VFRMT_2880x480p60_4_3, 2880, 480, false, 3432, 552, 525, 45,
	 31469, 59940, 108000, 59940, true},
	{HDMI_VFRMT_2880x480i60_4_3, 2880, 480, true,  3432, 552, 525, 22,
	 15750, 60000, 54054, 60000, true},
	{HDMI_VFRMT_2880x240p60_4_3, 2880, 240, false, 3432, 552, 262, 22,
	 15750, 60115, 54054, 60000, true},
	{HDMI_VFRMT_2880x240p60_4_3, 2880, 240, false, 3432, 552, 262, 23,
	 15750, 59886, 54054, 60000, true},
	{HDMI_VFRMT_2880x480p60_4_3, 2880, 480, false, 3432, 552, 525, 45,
	 31500, 60000, 108108, 60000, true},
};

static ssize_t hdmi_edid_sysfs_rda_modes(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;
	struct hdmi_edid_ctrl *edid_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_EDID);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	buf[0] = 0;
	if (edid_ctrl->sink_data.num_of_elements) {
		u32 *video_mode = edid_ctrl->sink_data.disp_mode_list;
		for (i = 0; i < edid_ctrl->sink_data.num_of_elements; ++i) {
			if (!hdmi_get_supported_mode(*video_mode))
				continue;
			if (ret > 0)
				ret += snprintf(buf+ret, PAGE_SIZE-ret, ",%d",
					*video_mode++);
			else
				ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d",
					*video_mode++);
		}
	} else {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d",
			edid_ctrl->video_resolution);
	}

	DEV_DBG("%s: '%s'\n", __func__, buf);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");

	return ret;
} /* hdmi_edid_sysfs_rda_modes */
static DEVICE_ATTR(edid_modes, S_IRUGO, hdmi_edid_sysfs_rda_modes, NULL);

static ssize_t hdmi_edid_sysfs_rda_physical_address(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_edid_ctrl *edid_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_EDID);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d\n", edid_ctrl->physical_address);
	DEV_DBG("%s: '%d'\n", __func__, edid_ctrl->physical_address);

	return ret;
} /* hdmi_edid_sysfs_rda_physical_address */
static DEVICE_ATTR(pa, S_IRUSR, hdmi_edid_sysfs_rda_physical_address, NULL);

static ssize_t hdmi_edid_sysfs_rda_scan_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_edid_ctrl *edid_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_EDID);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "%d, %d, %d\n", edid_ctrl->pt_scan_info,
		edid_ctrl->it_scan_info, edid_ctrl->ce_scan_info);
	DEV_DBG("%s: '%s'\n", __func__, buf);

	return ret;
} /* hdmi_edid_sysfs_rda_scan_info */
static DEVICE_ATTR(scan_info, S_IRUGO, hdmi_edid_sysfs_rda_scan_info, NULL);

static ssize_t hdmi_edid_sysfs_rda_3d_modes(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;
	char buff_3d[128];
	struct hdmi_edid_ctrl *edid_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_EDID);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	buf[0] = 0;
	if (edid_ctrl->sink_data.num_of_elements) {
		u32 *video_mode = edid_ctrl->sink_data.disp_mode_list;
		u32 *video_3d_mode = edid_ctrl->sink_data.disp_3d_mode_list;

		for (i = 0; i < edid_ctrl->sink_data.num_of_elements; ++i) {
			ret = hdmi_get_video_3d_fmt_2string(*video_3d_mode++,
				buff_3d);
			if (ret > 0)
				ret += snprintf(buf+ret, PAGE_SIZE-ret,
					",%d=%s", *video_mode++,
					buff_3d);
			else
				ret += snprintf(buf+ret, PAGE_SIZE-ret,
					"%d=%s", *video_mode++,
					buff_3d);
		}
	} else {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d",
			edid_ctrl->video_resolution);
	}

	DEV_DBG("%s: '%s'\n", __func__, buf);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");

	return ret;
} /* hdmi_edid_sysfs_rda_3d_modes */
static DEVICE_ATTR(edid_3d_modes, S_IRUGO, hdmi_edid_sysfs_rda_3d_modes, NULL);

static ssize_t hdmi_common_rda_edid_raw_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmi_edid_ctrl *edid_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_EDID);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	memcpy(buf, edid_ctrl->edid_buf,
		sizeof(edid_ctrl->edid_buf));

	return sizeof(edid_ctrl->edid_buf);
} /* hdmi_common_rda_edid_raw_data */
static DEVICE_ATTR(edid_raw_data, S_IRUGO, hdmi_common_rda_edid_raw_data, NULL);

static struct attribute *hdmi_edid_fs_attrs[] = {
	&dev_attr_edid_modes.attr,
	&dev_attr_pa.attr,
	&dev_attr_scan_info.attr,
	&dev_attr_edid_3d_modes.attr,
	&dev_attr_edid_raw_data.attr,
	NULL,
};

static struct attribute_group hdmi_edid_fs_attrs_group = {
	.attrs = hdmi_edid_fs_attrs,
};

static int hdmi_edid_read_block(struct hdmi_edid_ctrl *edid_ctrl, int block,
	u8 *edid_buf)
{
	const u8 *b = NULL;
	u32 ndx, check_sum, print_len;
	int block_size;
	int i, status;
	int retry_cnt = 0;
	struct hdmi_tx_ddc_data ddc_data;
	b = edid_buf;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

read_retry:
	block_size = 0x80;
	status = 0;
	do {
		DEV_DBG("EDID: reading block(%d) with block-size=%d\n",
			block, block_size);
		for (i = 0; i < 0x80; i += block_size) {
			memset(&ddc_data, 0, sizeof(ddc_data));
			ddc_data.dev_addr    = 0xA0;
			ddc_data.offset      = block*0x80 + i;
			ddc_data.data_buf    = edid_buf+i;
			ddc_data.data_len    = block_size;
			ddc_data.request_len = block_size;
			ddc_data.retry       = 1;
			ddc_data.what        = "EDID";
			ddc_data.no_align    = false;

			/*Read EDID twice with 32bit alighnment too */
			if (block < 2)
				status = hdmi_ddc_read(
					edid_ctrl->init_data.ddc_ctrl,
					&ddc_data);
			else
				status = hdmi_ddc_read_seg(
					edid_ctrl->init_data.ddc_ctrl,
					&ddc_data);
			if (status)
				break;
		}

		block_size /= 2;
	} while (status && (block_size >= 16));

	if (status)
		goto error;

	/* Calculate checksum */
	check_sum = 0;
	for (ndx = 0; ndx < 0x80; ++ndx)
		check_sum += edid_buf[ndx];

	if (check_sum & 0xFF) {
		DEV_ERR("%s: failed CHECKSUM (read:%x, expected:%x)\n",
			__func__, (u8)edid_buf[0x7F], (u8)check_sum);
		for (ndx = 0; ndx < 0x100; ndx += 4)
			DEV_DBG("EDID[%02x-%02x] %02x %02x %02x %02x\n",
				ndx, ndx+3,
				b[ndx+0], b[ndx+1], b[ndx+2], b[ndx+3]);
		status = -EPROTO;
		if (retry_cnt++ < 3) {
			DEV_DBG("Retrying reading EDID %d time\n", retry_cnt);
			goto read_retry;
		}
		goto error;
	}

	print_len = 0x80;
	for (ndx = 0; ndx < print_len; ndx += 4)
		DEV_DBG("EDID[%02x-%02x] %02x %02x %02x %02x\n",
			ndx, ndx+3,
			b[ndx+0], b[ndx+1], b[ndx+2], b[ndx+3]);

error:
	return status;
} /* hdmi_edid_read_block */

static const u8 *hdmi_edid_find_block(const u8 *in_buf, u32 start_offset,
	u8 type, u8 *len)
{
	/* the start of data block collection, start of Video Data Block */
	u32 offset = start_offset;
	u32 end_dbc_offset = in_buf[2];

	*len = 0;

	/*
	 * * edid buffer 1, byte 2 being 4 means no non-DTD/Data block
	 *   collection present.
	 * * edid buffer 1, byte 2 being 0 menas no non-DTD/DATA block
	 *   collection present and no DTD data present.
	 */
	if ((end_dbc_offset == 0) || (end_dbc_offset == 4)) {
		DEV_WARN("EDID: no DTD or non-DTD data present\n");
		return NULL;
	}

	while (offset < end_dbc_offset) {
		u8 block_len = in_buf[offset] & 0x1F;
		if ((in_buf[offset] >> 5) == type) {
			*len = block_len;
			DEV_DBG("%s: EDID: block=%d found @ 0x%x w/ len=%d\n",
				__func__, type, offset, block_len);

			return in_buf + offset;
		}
		offset += 1 + block_len;
	}
	DEV_WARN("%s: EDID: type=%d block not found in EDID block\n",
		__func__, type);

	return NULL;
} /* hdmi_edid_find_block */

static void hdmi_edid_extract_extended_data_blocks(
	struct hdmi_edid_ctrl *edid_ctrl, const u8 *in_buf)
{
	u8 len = 0;
	u32 start_offset = DBC_START_OFFSET;
	u8 const *etag = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	/* A Tage code of 7 identifies extended data blocks */
	etag = hdmi_edid_find_block(in_buf, start_offset, 7, &len);

	while (etag != NULL) {
		/* The extended data block should at least be 2 bytes long */
		if (len < 2) {
			DEV_DBG("%s: data block of len < 2 bytes. Ignor...\n",
				__func__);
		} else {
			/*
			 * The second byte of the extended data block has the
			 * extended tag code
			 */
			switch (etag[1]) {
			case 0:
				/* Video Capability Data Block */
				DEV_DBG("%s: EDID: VCDB=%02X %02X\n", __func__,
					etag[1], etag[2]);

				/*
				 * Check if the sink specifies underscan
				 * support for:
				 * BIT 5: preferred video format
				 * BIT 3: IT video format
				 * BIT 1: CE video format
				 */
				edid_ctrl->pt_scan_info =
					(etag[2] & (BIT(4) | BIT(5))) >> 4;
				edid_ctrl->it_scan_info =
					(etag[2] & (BIT(3) | BIT(2))) >> 2;
				edid_ctrl->ce_scan_info =
					etag[2] & (BIT(1) | BIT(0));
				DEV_DBG("%s: Scan Info (pt|it|ce): (%d|%d|%d)",
					__func__,
					edid_ctrl->pt_scan_info,
					edid_ctrl->it_scan_info,
					edid_ctrl->ce_scan_info);
				break;
			default:
				DEV_DBG("%s: Tag Code %d not supported\n",
					__func__, etag[1]);
				break;
			}
		}

		/* There could be more that one extended data block */
		start_offset = etag - in_buf + len + 1;
		etag = hdmi_edid_find_block(in_buf, start_offset, 7, &len);
	}
} /* hdmi_edid_extract_extended_data_blocks */

static void hdmi_edid_extract_3d_present(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *in_buf)
{
	u8 len, offset;
	const u8 *vsd = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 3, &len);

	edid_ctrl->present_3d = 0;
	if (vsd == NULL || len < 9) {
		DEV_DBG("%s: blk-id 3 not found or not long enough\n",
			__func__);
		return;
	}

	offset = HDMI_VSDB_3D_EVF_DATA_OFFSET(vsd);
	DEV_DBG("%s: EDID: 3D present @ 0x%x = %02x\n", __func__,
		offset, vsd[offset]);

	if (vsd[offset] >> 7) { /* 3D format indication present */
		DEV_INFO("%s: EDID: 3D present, 3D-len=%d\n", __func__,
			vsd[offset+1] & 0x1F);
		edid_ctrl->present_3d = 1;
	}
} /* hdmi_edid_extract_3d_present */

static void hdmi_edid_extract_audio_data_blocks(
	struct hdmi_edid_ctrl *edid_ctrl, const u8 *in_buf)
{
	u8 len, cnt = 0;
	const u8 *adb = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	adb = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 1, &len);
	if ((adb == NULL) || (len > MAX_AUDIO_DATA_BLOCK_SIZE))
		return;

	memcpy(edid_ctrl->audio_data_block, adb + 1, len);
	edid_ctrl->adb_size = len;

	while (len >= 3 && cnt < 16) {
		DEV_DBG("%s: ch=%d fmt=%d sampling=0x%02x bitdepth=0x%02x\n",
			__func__, (adb[1]&0x7)+1, adb[1]>>3, adb[2], adb[3]);

		cnt++;
		len -= 3;
		adb += 3;
	}
} /* hdmi_edid_extract_audio_data_blocks */

static void hdmi_edid_extract_speaker_allocation_data(
	struct hdmi_edid_ctrl *edid_ctrl, const u8 *in_buf)
{
	u8 len;
	const u8 *sadb = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	sadb = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 4, &len);
	if ((sadb == NULL) || (len != MAX_SPKR_ALLOC_DATA_BLOCK_SIZE))
		return;

	memcpy(edid_ctrl->spkr_alloc_data_block, sadb + 1, len);
	edid_ctrl->sadb_size = len;

	DEV_DBG("%s: EDID: speaker alloc data SP byte = %08x %s%s%s%s%s%s%s\n",
		__func__, sadb[1],
		(sadb[1] & BIT(0)) ? "FL/FR," : "",
		(sadb[1] & BIT(1)) ? "LFE," : "",
		(sadb[1] & BIT(2)) ? "FC," : "",
		(sadb[1] & BIT(3)) ? "RL/RR," : "",
		(sadb[1] & BIT(4)) ? "RC," : "",
		(sadb[1] & BIT(5)) ? "FLC/FRC," : "",
		(sadb[1] & BIT(6)) ? "RLC/RRC," : "");
} /* hdmi_edid_extract_speaker_allocation_data */

static void hdmi_edid_extract_latency_fields(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *in_buf)
{
	u8 len;
	const u8 *vsd = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 3, &len);

	if (vsd == NULL || len < 12 || !(vsd[8] & BIT(7))) {
		edid_ctrl->video_latency = (u16)-1;
		edid_ctrl->audio_latency = (u16)-1;
		DEV_DBG("%s: EDID: No audio/video latency present\n", __func__);
	} else {
		edid_ctrl->video_latency = vsd[9];
		edid_ctrl->audio_latency = vsd[10];
		DEV_DBG("%s: EDID: video-latency=%04x, audio-latency=%04x\n",
			__func__, edid_ctrl->video_latency,
			edid_ctrl->audio_latency);
	}
} /* hdmi_edid_extract_latency_fields */

static u32 hdmi_edid_extract_ieee_reg_id(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *in_buf)
{
	u8 len;
	const u8 *vsd = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return 0;
	}

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 3, &len);
	if (vsd == NULL)
		return 0;

	DEV_DBG("%s: EDID: VSD PhyAddr=%04x, MaxTMDS=%dMHz\n", __func__,
		((u32)vsd[4] << 8) + (u32)vsd[5], (u32)vsd[7] * 5);

	edid_ctrl->physical_address = ((u16)vsd[4] << 8) + (u16)vsd[5];

	return ((u32)vsd[3] << 16) + ((u32)vsd[2] << 8) + (u32)vsd[1];
} /* hdmi_edid_extract_ieee_reg_id */

static void hdmi_edid_extract_vendor_id(const u8 *in_buf,
	char *vendor_id)
{
	u32 id_codes = ((u32)in_buf[8] << 8) + in_buf[9];

	vendor_id[0] = 'A' - 1 + ((id_codes >> 10) & 0x1F);
	vendor_id[1] = 'A' - 1 + ((id_codes >> 5) & 0x1F);
	vendor_id[2] = 'A' - 1 + (id_codes & 0x1F);
	vendor_id[3] = 0;
} /* hdmi_edid_extract_vendor_id */

static u32 hdmi_edid_check_header(const u8 *edid_buf)
{
	return (edid_buf[0] == 0x00) && (edid_buf[1] == 0xff)
		&& (edid_buf[2] == 0xff) && (edid_buf[3] == 0xff)
		&& (edid_buf[4] == 0xff) && (edid_buf[5] == 0xff)
		&& (edid_buf[6] == 0xff) && (edid_buf[7] == 0x00);
} /* hdmi_edid_check_header */

static void hdmi_edid_detail_desc(const u8 *data_buf, u32 *disp_mode)
{
	u32	aspect_ratio_4_3    = false;
	u32	interlaced          = false;
	u32	active_h            = 0;
	u32	active_v            = 0;
	u32	blank_h             = 0;
	u32	blank_v             = 0;
	u32	ndx                 = 0;
	u32	max_num_of_elements = 0;
	u32	img_size_h          = 0;
	u32	img_size_v          = 0;

	/*
	 * * See VESA Spec
	 * * EDID_TIMING_DESC_UPPER_H_NIBBLE[0x4]: Relative Offset to the
	 *   EDID detailed timing descriptors - Upper 4 bit for each H
	 *   active/blank field
	 * * EDID_TIMING_DESC_H_ACTIVE[0x2]: Relative Offset to the EDID
	 *   detailed timing descriptors - H active
	 */
	active_h = ((((u32)data_buf[0x4] >> 0x4) & 0xF) << 8)
		| data_buf[0x2];

	/*
	 * EDID_TIMING_DESC_H_BLANK[0x3]: Relative Offset to the EDID detailed
	 *   timing descriptors - H blank
	 */
	blank_h = (((u32)data_buf[0x4] & 0xF) << 8)
		| data_buf[0x3];

	/*
	 * * EDID_TIMING_DESC_UPPER_V_NIBBLE[0x7]: Relative Offset to the
	 *   EDID detailed timing descriptors - Upper 4 bit for each V
	 *   active/blank field
	 * * EDID_TIMING_DESC_V_ACTIVE[0x5]: Relative Offset to the EDID
	 *   detailed timing descriptors - V active
	 */
	active_v = ((((u32)data_buf[0x7] >> 0x4) & 0xF) << 8)
		| data_buf[0x5];

	/*
	 * EDID_TIMING_DESC_V_BLANK[0x6]: Relative Offset to the EDID
	 * detailed timing descriptors - V blank
	 */
	blank_v = (((u32)data_buf[0x7] & 0xF) << 8)
		| data_buf[0x6];

	/*
	 * * EDID_TIMING_DESC_IMAGE_SIZE_UPPER_NIBBLE[0xE]: Relative Offset
	 *   to the EDID detailed timing descriptors - Image Size upper
	 *   nibble V and H
	 * * EDID_TIMING_DESC_H_IMAGE_SIZE[0xC]: Relative Offset to the EDID
	 *   detailed timing descriptors - H image size
	 * * EDID_TIMING_DESC_V_IMAGE_SIZE[0xD]: Relative Offset to the EDID
	 *   detailed timing descriptors - V image size
	 */
	img_size_h = ((((u32)data_buf[0xE] >> 0x4) & 0xF) << 8)
		| data_buf[0xC];
	img_size_v = (((u32)data_buf[0xE] & 0xF) << 8)
		| data_buf[0xD];

	/*
	 * aspect ratio as 4:3 if within specificed range , rathaer than being
	 * absolute value
	 */
	aspect_ratio_4_3 = (abs(img_size_h * 3 - img_size_v * 4) < 5) ? 1 : 0;

	max_num_of_elements = sizeof(hdmi_edid_disp_mode_lut)
		/ sizeof(*hdmi_edid_disp_mode_lut);

	/*
	 * EDID_TIMING_DESC_INTERLACE[0x11:7]: Relative Offset to the EDID
	 * detailed timing descriptors - Interlace flag
	 */
	DEV_DBG("%s: Interlaced mode byte data_buf[0x11]=[%x]\n", __func__,
		data_buf[0x11]);

	/*
	 * CEA 861-D: interlaced bit is bit[7] of byte[0x11]
	 */
	interlaced = (data_buf[0x11] & 0x80) >> 7;

	DEV_DBG("%s: A[%ux%u] B[%ux%u] V[%ux%u] %s\n", __func__,
		active_h, active_v, blank_h, blank_v, img_size_h, img_size_v,
		interlaced ? "i" : "p");

	*disp_mode = HDMI_VFRMT_FORCE_32BIT;
	while (ndx < max_num_of_elements) {
		const struct hdmi_edid_video_mode_property_type *edid =
			hdmi_edid_disp_mode_lut + ndx;

		if ((interlaced    == edid->interlaced)    &&
			(active_h  == edid->active_h)      &&
			(blank_h   == edid->total_blank_h) &&
			(blank_v   == edid->total_blank_v) &&
			((active_v == edid->active_v) ||
			(active_v  == (edid->active_v + 1)))) {
			if (edid->aspect_ratio_4_3 && !aspect_ratio_4_3)
				/* Aspect ratio 16:9 */
				*disp_mode = edid->video_code + 1;
			else
				/* Aspect ratio 4:3 */
				*disp_mode = edid->video_code;

			DEV_DBG("%s: mode found:%d\n", __func__, *disp_mode);
			break;
		}
		++ndx;
	}
	if (ndx == max_num_of_elements)
		DEV_INFO("%s: *no mode* found\n", __func__);
} /* hdmi_edid_detail_desc */

static void hdmi_edid_add_sink_3d_format(struct hdmi_edid_sink_data *sink_data,
	u32 video_format, u32 video_3d_format)
{
	char string[128];
	u32 added = false;
	int i;

	for (i = 0; i < sink_data->num_of_elements; ++i) {
		if (sink_data->disp_mode_list[i] == video_format) {
			sink_data->disp_3d_mode_list[i] |= video_3d_format;
			added = true;
			break;
		}
	}

	hdmi_get_video_3d_fmt_2string(video_3d_format, string);

	DEV_DBG("%s: EDID[3D]: format: %d [%s], %s %s\n", __func__,
		video_format, msm_hdmi_mode_2string(video_format),
		string, added ? "added" : "NOT added");
} /* hdmi_edid_add_sink_3d_format */

static void hdmi_edid_add_sink_video_format(
	struct hdmi_edid_sink_data *sink_data, u32 video_format)
{
	const struct msm_hdmi_mode_timing_info *timing =
		hdmi_get_supported_mode(video_format);
	u32 supported = timing != NULL;

	if (video_format >= HDMI_VFRMT_MAX) {
		DEV_ERR("%s: video format: %s is not supported\n", __func__,
			msm_hdmi_mode_2string(video_format));
		return;
	}

	DEV_DBG("%s: EDID: format: %d [%s], %s\n", __func__,
		video_format, msm_hdmi_mode_2string(video_format),
		supported ? "Supported" : "Not-Supported");

	if (supported) {
		/* todo: MHL */
		sink_data->disp_mode_list[sink_data->num_of_elements++] =
			video_format;
	}
} /* hdmi_edid_add_sink_video_format */

static void hdmi_edid_get_display_vsd_3d_mode(const u8 *data_buf,
	struct hdmi_edid_sink_data *sink_data, u32 num_of_cea_blocks)
{
	u8 len, offset, present_multi_3d, hdmi_vic_len;
	int hdmi_3d_len;
	u16 structure_all, structure_mask;
	const u8 *vsd = num_of_cea_blocks ?
		hdmi_edid_find_block(data_buf+0x80, DBC_START_OFFSET,
				3, &len) : NULL;
	int i;

	offset = HDMI_VSDB_3D_EVF_DATA_OFFSET(vsd);
	present_multi_3d = (vsd[offset] & 0x60) >> 5;

	offset += 1;
	hdmi_vic_len = (vsd[offset] >> 5) & 0x7;
	hdmi_3d_len = vsd[offset] & 0x1F;
	DEV_DBG("%s: EDID[3D]: HDMI_VIC_LEN = %d, HDMI_3D_LEN = %d\n", __func__,
		hdmi_vic_len, hdmi_3d_len);

	offset += (hdmi_vic_len + 1);
	if (present_multi_3d == 1 || present_multi_3d == 2) {
		DEV_DBG("%s: EDID[3D]: multi 3D present (%d)\n", __func__,
			present_multi_3d);
		/* 3d_structure_all */
		structure_all = (vsd[offset] << 8) | vsd[offset + 1];
		offset += 2;
		hdmi_3d_len -= 2;
		if (present_multi_3d == 2) {
			/* 3d_structure_mask */
			structure_mask = (vsd[offset] << 8) | vsd[offset + 1];
			offset += 2;
			hdmi_3d_len -= 2;
		} else
			structure_mask = 0xffff;

		i = 0;
		while (i < 16) {
			if (i >= sink_data->disp_multi_3d_mode_list_cnt)
				break;

			if (!(structure_mask & BIT(i))) {
				++i;
				continue;
			}

			/* BIT0: FRAME PACKING */
			if (structure_all & BIT(0))
				hdmi_edid_add_sink_3d_format(sink_data,
					sink_data->
					disp_multi_3d_mode_list[i],
					FRAME_PACKING);

			/* BIT6: TOP AND BOTTOM */
			if (structure_all & BIT(6))
				hdmi_edid_add_sink_3d_format(sink_data,
					sink_data->
					disp_multi_3d_mode_list[i],
					TOP_AND_BOTTOM);

			/* BIT8: SIDE BY SIDE HALF */
			if (structure_all & BIT(8))
				hdmi_edid_add_sink_3d_format(sink_data,
					sink_data->
					disp_multi_3d_mode_list[i],
					SIDE_BY_SIDE_HALF);

			++i;
		}
	}

	i = 0;
	while (hdmi_3d_len > 0) {
		DEV_DBG("%s: EDID: 3D_Structure_%d @ 0x%x: %02x\n",
			__func__, i + 1, offset, vsd[offset]);

		if ((vsd[offset] >> 4) >=
			sink_data->disp_multi_3d_mode_list_cnt) {
			if ((vsd[offset] & 0x0F) >= 8) {
				offset += 1;
				hdmi_3d_len -= 1;
				DEV_DBG("%s:EDID:3D_Detail_%d @ 0x%x: %02x\n",
					__func__, i + 1, offset, vsd[offset]);
			}
			i += 1;
			offset += 1;
			hdmi_3d_len -= 1;
			continue;
		}

		switch (vsd[offset] & 0x0F) {
		case 0:
			/* 0000b: FRAME PACKING */
			hdmi_edid_add_sink_3d_format(sink_data,
				sink_data->
				disp_multi_3d_mode_list[vsd[offset] >> 4],
				FRAME_PACKING);
			break;
		case 6:
			/* 0110b: TOP AND BOTTOM */
			hdmi_edid_add_sink_3d_format(sink_data,
				sink_data->
				disp_multi_3d_mode_list[vsd[offset] >> 4],
				TOP_AND_BOTTOM);
			break;
		case 8:
			/* 1000b: SIDE BY SIDE HALF */
			hdmi_edid_add_sink_3d_format(sink_data,
				sink_data->
				disp_multi_3d_mode_list[vsd[offset] >> 4],
				SIDE_BY_SIDE_HALF);
			break;
		}
		if ((vsd[offset] & 0x0F) >= 8) {
			offset += 1;
			hdmi_3d_len -= 1;
			DEV_DBG("%s: EDID[3D]: 3D_Detail_%d @ 0x%x: %02x\n",
				__func__, i + 1, offset,
				vsd[offset]);
		}
		i += 1;
		offset += 1;
		hdmi_3d_len -= 1;
	}
} /* hdmi_edid_get_display_vsd_3d_mode */

static void hdmi_edid_get_extended_video_formats(
	struct hdmi_edid_ctrl *edid_ctrl, const u8 *in_buf)
{
	u8 db_len, offset, i;
	u8 hdmi_vic_len;
	u32 video_format;
	const u8 *vsd = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET, 3, &db_len);

	if (vsd == NULL || db_len < 9) {
		DEV_DBG("%s: blk-id 3 not found or not long enough\n",
			__func__);
		return;
	}

	/* check if HDMI_Video_present flag is set or not */
	if (!(vsd[8] & BIT(5))) {
		DEV_DBG("%s: extended vfmts are not supported by the sink.\n",
			__func__);
		return;
	}

	offset = HDMI_VSDB_3D_EVF_DATA_OFFSET(vsd);

	hdmi_vic_len = vsd[offset + 1] >> 5;
	if (hdmi_vic_len) {
		DEV_DBG("%s: EDID: EVFRMT @ 0x%x of block 3, len = %02x\n",
			__func__, offset, hdmi_vic_len);

		for (i = 0; i < hdmi_vic_len; i++) {
			video_format = HDMI_VFRMT_END + vsd[offset + 2 + i];
			hdmi_edid_add_sink_video_format(&edid_ctrl->sink_data,
				video_format);
		}
	}
} /* hdmi_edid_get_extended_video_formats */

static void hdmi_edid_get_display_mode(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *data_buf, u32 num_of_cea_blocks)
{
	u8 i = 0, offset = 0, std_blk = 0;
	u32 video_format = HDMI_VFRMT_640x480p60_4_3;
	u32 has480p = false;
	u8 len;
	const u8 *edid_blk0 = NULL;
	const u8 *edid_blk1 = NULL;
	const u8 *svd = NULL;
	u32 has60hz_mode = false;
	u32 has50hz_mode = false;
	struct hdmi_edid_sink_data *sink_data = NULL;

	if (!edid_ctrl || !data_buf) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	edid_blk0 = &data_buf[0x0];
	edid_blk1 = &data_buf[0x80];
	svd = num_of_cea_blocks ?
		hdmi_edid_find_block(data_buf+0x80, DBC_START_OFFSET, 2,
			&len) : NULL;

	sink_data = &edid_ctrl->sink_data;

	sink_data->num_of_elements = 0;
	sink_data->disp_multi_3d_mode_list_cnt = 0;
	if (svd != NULL) {
		++svd;
		for (i = 0; i < len; ++i, ++svd) {
			/*
			 * Subtract 1 because it is zero based in the driver,
			 * while the Video identification code is 1 based in the
			 * CEA_861D spec
			 */
			video_format = (*svd & 0x7F);
			hdmi_edid_add_sink_video_format(sink_data,
				video_format);
			/* Make a note of the preferred video format */
			if (i == 0)
				sink_data->preferred_video_format =
					video_format;

			if (i < 16) {
				sink_data->disp_multi_3d_mode_list[i]
					= video_format;
				sink_data->disp_multi_3d_mode_list_cnt++;
			}

			if (video_format <= HDMI_VFRMT_1920x1080p60_16_9 ||
				video_format == HDMI_VFRMT_2880x480p60_4_3 ||
				video_format == HDMI_VFRMT_2880x480p60_16_9)
				has60hz_mode = true;

			if ((video_format >= HDMI_VFRMT_720x576p50_4_3 &&
				video_format <= HDMI_VFRMT_1920x1080p50_16_9) ||
				video_format == HDMI_VFRMT_2880x576p50_4_3 ||
				video_format == HDMI_VFRMT_2880x576p50_16_9 ||
				video_format == HDMI_VFRMT_1920x1250i50_16_9)
				has50hz_mode = true;

			if (video_format == HDMI_VFRMT_640x480p60_4_3)
				has480p = true;
		}
	} else if (!num_of_cea_blocks) {
		/* Detailed timing descriptors */
		u32 desc_offset = 0;
		/*
		 * * Maximum 4 timing descriptor in block 0 - No CEA
		 *   extension in this case
		 * * EDID_FIRST_TIMING_DESC[0x36] - 1st detailed timing
		 *   descriptor
		 * * EDID_DETAIL_TIMING_DESC_BLCK_SZ[0x12] - Each detailed
		 *   timing descriptor has block size of 18
		 */
		while (4 > i && 0 != edid_blk0[0x36+desc_offset]) {
			hdmi_edid_detail_desc(edid_blk0+0x36+desc_offset,
				&video_format);

			DEV_DBG("[%s:%d] Block-0 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				msm_hdmi_mode_2string(video_format));

			hdmi_edid_add_sink_video_format(sink_data,
				video_format);

			if (video_format == HDMI_VFRMT_640x480p60_4_3)
				has480p = true;

			/* Make a note of the preferred video format */
			if (i == 0) {
				sink_data->preferred_video_format =
					video_format;
			}
			desc_offset += 0x12;
			++i;
		}
	} else if (1 == num_of_cea_blocks) {
		u32 desc_offset = 0;

		/*
		 * Read from both block 0 and block 1
		 * Read EDID block[0] as above
		 */
		while (4 > i && 0 != edid_blk0[0x36+desc_offset]) {
			hdmi_edid_detail_desc(edid_blk0+0x36+desc_offset,
				&video_format);

			DEV_DBG("[%s:%d] Block-0 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				msm_hdmi_mode_2string(video_format));

			hdmi_edid_add_sink_video_format(sink_data,
				video_format);

			if (video_format == HDMI_VFRMT_640x480p60_4_3)
				has480p = true;

			/* Make a note of the preferred video format */
			if (i == 0) {
				sink_data->preferred_video_format =
					video_format;
			}
			desc_offset += 0x12;
			++i;
		}

		/*
		 * * Parse block 1 - CEA extension byte offset of first
		 *   detailed timing generation - offset is relevant to
		 *   the offset of block 1
		 * * EDID_CEA_EXTENSION_FIRST_DESC[0x82]: Offset to CEA
		 *   extension first timing desc - indicate the offset of
		 *   the first detailed timing descriptor
		 * * EDID_BLOCK_SIZE = 0x80  Each page size in the EDID ROM
		 */
		desc_offset = edid_blk1[0x02];
		while (0 != edid_blk1[desc_offset]) {
			hdmi_edid_detail_desc(edid_blk1+desc_offset,
				&video_format);

			DEV_DBG("[%s:%d] Block-1 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				msm_hdmi_mode_2string(video_format));

			hdmi_edid_add_sink_video_format(sink_data,
				video_format);
			if (video_format == HDMI_VFRMT_640x480p60_4_3)
				has480p = true;

			/* Make a note of the preferred video format */
			if (i == 0) {
				sink_data->preferred_video_format =
					video_format;
			}
			desc_offset += 0x12;
			++i;
		}
	}

	std_blk = 0;
	offset  = 0;
	while (std_blk < 8) {
		if ((edid_blk0[0x26 + offset] == 0x81) &&
		    (edid_blk0[0x26 + offset + 1] == 0x80)) {
			pr_debug("%s: 108MHz: off=[%x] stdblk=[%x]\n",
				 __func__, offset, std_blk);
			hdmi_edid_add_sink_video_format(sink_data,
				HDMI_VFRMT_1280x1024p60_5_4);
		}
		if ((edid_blk0[0x26 + offset] == 0x61) &&
		    (edid_blk0[0x26 + offset + 1] == 0x40)) {
			pr_debug("%s: 65MHz: off=[%x] stdblk=[%x]\n",
				 __func__, offset, std_blk);
			hdmi_edid_add_sink_video_format(sink_data,
				HDMI_VFRMT_1024x768p60_4_3);
			break;
		} else {
			offset += 2;
		}
		std_blk++;
	}
	/* check if the EDID revision is 4 (version 1.4) */
	if (edid_blk0[0x13] == 4) {
		u8  start = 0x36;
		i = 0;
		/* Check each of 4 - 18 bytes descriptors */
		while (i < 4) {
			u8  iter   = start;
			u32 header_1 = 0;
			u8  header_2 = 0;
			header_1 = edid_blk0[iter++];
			header_1 = header_1 << 8 | edid_blk0[iter++];
			header_1 = header_1 << 8 | edid_blk0[iter++];
			header_1 = header_1 << 8 | edid_blk0[iter++];
			header_2 = edid_blk0[iter];
			if (header_1 == 0x000000F7 &&
			    header_2 == 0x00) {
				iter++;
				/* VESA DMT Standard Version (0x0A)*/
				iter++;
				/* First set of supported formats */
				iter++;
				/* Second set of supported formats */
				if (edid_blk0[iter] & 0x02) {
					pr_debug("%s: DMT 1280x1024@60\n",
						 __func__);
					hdmi_edid_add_sink_video_format(
						sink_data,
						HDMI_VFRMT_1280x1024p60_5_4);
					break;
				}
			}
			i++;
			start += 0x12;
		}
	}

	/* Established Timing I and II */
	if (edid_blk0[0x24] & BIT(3)) {
		pr_debug("%s: 65MHz: off=[%x] stdblk=[%x]\n",
			 __func__, offset, std_blk);
		hdmi_edid_add_sink_video_format(sink_data,
				HDMI_VFRMT_1024x768p60_4_3);
	}

	hdmi_edid_get_extended_video_formats(edid_ctrl, data_buf+0x80);

	/* mandaroty 3d format */
	if (edid_ctrl->present_3d) {
		if (has60hz_mode) {
			hdmi_edid_add_sink_3d_format(sink_data,
				HDMI_VFRMT_1920x1080p24_16_9,
				FRAME_PACKING | TOP_AND_BOTTOM);
			hdmi_edid_add_sink_3d_format(sink_data,
				HDMI_VFRMT_1280x720p60_16_9,
				FRAME_PACKING | TOP_AND_BOTTOM);
			hdmi_edid_add_sink_3d_format(sink_data,
				HDMI_VFRMT_1920x1080i60_16_9,
				SIDE_BY_SIDE_HALF);
		}

		if (has50hz_mode) {
			hdmi_edid_add_sink_3d_format(sink_data,
				HDMI_VFRMT_1920x1080p24_16_9,
				FRAME_PACKING | TOP_AND_BOTTOM);
			hdmi_edid_add_sink_3d_format(sink_data,
				HDMI_VFRMT_1280x720p50_16_9,
				FRAME_PACKING | TOP_AND_BOTTOM);
			hdmi_edid_add_sink_3d_format(sink_data,
				HDMI_VFRMT_1920x1080i50_16_9,
				SIDE_BY_SIDE_HALF);
		}

		/* 3d format described in Vendor Specific Data */
		hdmi_edid_get_display_vsd_3d_mode(data_buf, sink_data,
			num_of_cea_blocks);
	}

	/*
	 * Need to add default 640 by 480 timings, in case not described
	 * in the EDID structure.
	 * All DTV sink devices should support this mode
	 */
	if (!has480p)
		hdmi_edid_add_sink_video_format(sink_data,
			HDMI_VFRMT_640x480p60_4_3);
} /* hdmi_edid_get_display_mode */

int hdmi_edid_read(void *input)
{
	/* EDID_BLOCK_SIZE[0x80] Each page size in the EDID ROM */
	u8 *edid_buf = NULL;
	u32 cea_extension_ver = 0;
	u32 num_of_cea_blocks = 0;
	u32 ieee_reg_id = 0;
	u32 i = 1;
	int status = 0;
	char vendor_id[5];
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	edid_buf = edid_ctrl->edid_buf;

	edid_ctrl->pt_scan_info = 0;
	edid_ctrl->it_scan_info = 0;
	edid_ctrl->ce_scan_info = 0;
	edid_ctrl->present_3d = 0;
	memset(&edid_ctrl->sink_data, 0, sizeof(edid_ctrl->sink_data));
	memset(edid_buf, 0, sizeof(edid_ctrl->edid_buf));
	memset(edid_ctrl->audio_data_block, 0,
		sizeof(edid_ctrl->audio_data_block));
	memset(edid_ctrl->spkr_alloc_data_block, 0,
		sizeof(edid_ctrl->spkr_alloc_data_block));
	edid_ctrl->adb_size = 0;
	edid_ctrl->sadb_size = 0;

	status = hdmi_edid_read_block(edid_ctrl, 0, edid_buf);
	if (status || !hdmi_edid_check_header(edid_buf)) {
		if (!status)
			status = -EPROTO;
		DEV_ERR("%s: blk0 fail:%d[%02x%02x%02x%02x%02x%02x%02x%02x]\n",
			__func__, status,
			edid_buf[0], edid_buf[1], edid_buf[2], edid_buf[3],
			edid_buf[4], edid_buf[5], edid_buf[6], edid_buf[7]);
		goto error;
	}
	hdmi_edid_extract_vendor_id(edid_buf, vendor_id);

	/* EDID_CEA_EXTENSION_FLAG[0x7E] - CEC extension byte */
	num_of_cea_blocks = edid_buf[0x7E];
	DEV_DBG("%s: No. of CEA blocks is  [%u]\n", __func__,
		num_of_cea_blocks);
	/* Find out any CEA extension blocks following block 0 */
	switch (num_of_cea_blocks) {
	case 0: /* No CEA extension */
		edid_ctrl->sink_mode = false;
		DEV_DBG("HDMI DVI mode: %s\n",
			edid_ctrl->sink_mode ? "no" : "yes");
		break;
	case 1: /* Read block 1 */
		status = hdmi_edid_read_block(edid_ctrl, 1, &edid_buf[0x80]);
		if (status) {
			DEV_ERR("%s: ddc read block(1) failed: %d\n", __func__,
				status);
			goto error;
		}
		if (edid_buf[0x80] != 2)
			num_of_cea_blocks = 0;
		if (num_of_cea_blocks) {
			ieee_reg_id =
				hdmi_edid_extract_ieee_reg_id(edid_ctrl,
					edid_buf+0x80);
			if (ieee_reg_id == 0x0c03)
				edid_ctrl->sink_mode = true;
			else
				edid_ctrl->sink_mode = false;

			hdmi_edid_extract_latency_fields(edid_ctrl,
				edid_buf+0x80);
			hdmi_edid_extract_speaker_allocation_data(
				edid_ctrl, edid_buf+0x80);
			hdmi_edid_extract_audio_data_blocks(edid_ctrl,
				edid_buf+0x80);
			hdmi_edid_extract_3d_present(edid_ctrl,
				edid_buf+0x80);
			hdmi_edid_extract_extended_data_blocks(edid_ctrl,
				edid_buf+0x80);
		}
		break;
	case 2:
	case 3:
	case 4:
		for (i = 1; i <= num_of_cea_blocks; i++) {
			if (!(i % 2)) {
				status = hdmi_edid_read_block(
					edid_ctrl, i, edid_buf + (0x80 * i));
				if (status) {
					DEV_ERR("%s: read blk(%d) failed:%d\n",
						__func__, i, status);
					goto error;
				}
			} else {
				status = hdmi_edid_read_block(
					edid_ctrl, i, edid_buf + (0x80 * i));
				if (status) {
					DEV_ERR("%s: read blk(%d) failed:%d\n",
						__func__, i, status);
					goto error;
				}
			}
		}
		break;
	default:
		DEV_ERR("%s: ddc read failed, not supported multi-blocks: %d\n",
			__func__, num_of_cea_blocks);
		status = -EPROTO;
		goto error;
	}

	if (num_of_cea_blocks) {
		/* EDID_CEA_EXTENSION_VERSION[0x81]: Offset to CEA extension
		 * version number - v1,v2,v3 (v1 is seldom, v2 is obsolete,
		 * v3 most common) */
		cea_extension_ver = edid_buf[0x81];
	}

	/* EDID_VERSION[0x12] - EDID Version */
	/* EDID_REVISION[0x13] - EDID Revision */
	DEV_INFO("%s: V=%d.%d #CEABlks=%d[V%d] ID=%s IEEE=%04x Ext=0x%02x\n",
		__func__, edid_buf[0x12], edid_buf[0x13],
		num_of_cea_blocks, cea_extension_ver, vendor_id, ieee_reg_id,
		edid_buf[0x80]);

	hdmi_edid_get_display_mode(edid_ctrl, edid_buf, num_of_cea_blocks);

	return 0;

error:
	edid_ctrl->sink_data.num_of_elements = 1;
	edid_ctrl->sink_data.disp_mode_list[0] = edid_ctrl->video_resolution;

	return status;
} /* hdmi_edid_read */

/*
 * If the sink specified support for both underscan/overscan then, by default,
 * set the underscan bit. Only checking underscan support for preferred
 * format and cea formats.
 */
u8 hdmi_edid_get_sink_scaninfo(void *input, u32 resolution)
{
	u8 scaninfo = 0;
	int use_ce_scan_info = true;
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto end;
	}

	if (resolution == edid_ctrl->sink_data.preferred_video_format) {
		use_ce_scan_info = false;
		switch (edid_ctrl->pt_scan_info) {
		case 0:
			/*
			 * Need to use the info specified for the corresponding
			 * IT or CE format
			 */
			DEV_DBG("%s: No underscan info for preferred V fmt\n",
				__func__);
			use_ce_scan_info = true;
			break;
		case 3:
			DEV_DBG("%s: Set underscan bit for preferred V fmt\n",
				__func__);
			scaninfo = BIT(1);
			break;
		default:
			DEV_DBG("%s: Underscan not set for preferred V fmt\n",
				__func__);
			break;
		}
	}

	if (use_ce_scan_info) {
		if (3 == edid_ctrl->ce_scan_info) {
			DEV_DBG("%s: Setting underscan bit for CE video fmt\n",
				__func__);
			scaninfo |= BIT(1);
		} else {
			DEV_DBG("%s: Not setting underscan bit for CE V fmt\n",
				__func__);
		}
	}

end:
	return scaninfo;
} /* hdmi_edid_get_sink_scaninfo */

u32 hdmi_edid_get_sink_mode(void *input)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return 0;
	}

	return edid_ctrl->sink_mode;
} /* hdmi_edid_get_sink_mode */

int hdmi_edid_get_audio_blk(void *input, struct msm_hdmi_audio_edid_blk *blk)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl || !blk) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	blk->audio_data_blk = edid_ctrl->audio_data_block;
	blk->audio_data_blk_size = edid_ctrl->adb_size;

	blk->spk_alloc_data_blk = edid_ctrl->spkr_alloc_data_block;
	blk->spk_alloc_data_blk_size = edid_ctrl->sadb_size;

	return 0;
} /* hdmi_edid_get_audio_blk */

void hdmi_edid_set_video_resolution(void *input, u32 resolution)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	edid_ctrl->video_resolution = resolution;

	if (1 == edid_ctrl->sink_data.num_of_elements)
		edid_ctrl->sink_data.disp_mode_list[0] = resolution;
} /* hdmi_edid_set_video_resolution */

void hdmi_edid_deinit(void *input)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (edid_ctrl) {
		sysfs_remove_group(edid_ctrl->init_data.sysfs_kobj,
			&hdmi_edid_fs_attrs_group);
		kfree(edid_ctrl);
	}
} /* hdmi_edid_deinit */

void *hdmi_edid_init(struct hdmi_edid_init_data *init_data)
{
	struct hdmi_edid_ctrl *edid_ctrl = NULL;

	if (!init_data || !init_data->io ||
		!init_data->mutex || !init_data->sysfs_kobj ||
		!init_data->ddc_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto error;
	}

	edid_ctrl = kzalloc(sizeof(*edid_ctrl), GFP_KERNEL);
	if (!edid_ctrl) {
		DEV_ERR("%s: Out of memory\n", __func__);
		goto error;
	}

	edid_ctrl->init_data = *init_data;
	edid_ctrl->sink_mode = false;

	if (sysfs_create_group(init_data->sysfs_kobj,
		&hdmi_edid_fs_attrs_group)) {
		DEV_ERR("%s: EDID sysfs create failed\n", __func__);
		kfree(edid_ctrl);
		edid_ctrl = NULL;
	}

error:
	return (void *)edid_ctrl;
} /* hdmi_edid_deinit */

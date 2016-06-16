/* Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
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
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/device.h>
#include "mdss_fb.h"
#include "mdss_hdmi_edid.h"

#define DBC_START_OFFSET 4
#define EDID_DTD_LEN 18
/*
 * As per CEA-861-E specification 7.5.2, there can be
 * upto 31 bytes following any tag (data block type).
 */
#define MAX_DATA_BLOCK_SIZE 31

#define HDMI_VSDB_3D_EVF_DATA_OFFSET(vsd) \
	(!((vsd)[8] & BIT(7)) ? 9 : (!((vsd)[8] & BIT(6)) ? 11 : 13))

/*
 * As per the CEA-861E spec, there can be a total of 10 short audio
 * descriptors with each SAD being 3 bytes long.
 * Thus, the maximum length of the audio data block would be 30 bytes
 */
#define MAX_NUMBER_ADB                  5
#define MAX_AUDIO_DATA_BLOCK_SIZE	30
#define MAX_SPKR_ALLOC_DATA_BLOCK_SIZE	3

/*
 * As per the HDMI 2.0 spec, the size of the HF-VSDB cannot exceed 31 bytes and
 * the minimum size is 7 bytes.
 */
#define MAX_HF_VSDB_SIZE    31
#define MIN_HF_VSDB_SIZE    7

/* IEEE OUI for HDMI Forum. */
#define HDMI_FORUM_IEEE_OUI 0xD85DC4

/* Support for first 5 EDID blocks */
#define MAX_EDID_SIZE (EDID_BLOCK_SIZE * MAX_EDID_BLOCKS)

#define BUFF_SIZE_3D 128

#define DTD_MAX			0x04
#define DTD_OFFSET		0x36
#define DTD_SIZE		0x12
#define REVISION_OFFSET		0x13
#define EDID_REVISION_FOUR	0x04

#define EDID_VENDOR_ID_SIZE     4
#define EDID_IEEE_REG_ID        0x0c03

enum edid_sink_mode {
	SINK_MODE_DVI,
	SINK_MODE_HDMI
};

enum luminance_value {
	NO_LUMINANCE_DATA = 3,
	MAXIMUM_LUMINANCE = 4,
	FRAME_AVERAGE_LUMINANCE = 5,
	MINIMUM_LUMINANCE = 6
};

enum data_block_types {
	RESERVED,
	AUDIO_DATA_BLOCK,
	VIDEO_DATA_BLOCK,
	VENDOR_SPECIFIC_DATA_BLOCK,
	SPEAKER_ALLOCATION_DATA_BLOCK,
	VESA_DTC_DATA_BLOCK,
	RESERVED2,
	USE_EXTENDED_TAG
};

enum extended_data_block_types {
	VIDEO_CAPABILITY_DATA_BLOCK = 0x0,
	VENDOR_SPECIFIC_VIDEO_DATA_BLOCK = 0x01,
	HDMI_VIDEO_DATA_BLOCK = 0x04,
	HDR_STATIC_METADATA_DATA_BLOCK = 0x06,
	Y420_VIDEO_DATA_BLOCK = 0x0E,
	VIDEO_FORMAT_PREFERENCE_DATA_BLOCK = 0x0D,
	Y420_CAPABILITY_MAP_DATA_BLOCK = 0x0F,
	VENDOR_SPECIFIC_AUDIO_DATA_BLOCK = 0x11,
	INFOFRAME_DATA_BLOCK = 0x20,
};

struct disp_mode_info {
	u32 video_format;
	u32 video_3d_format; /* Flags like SIDE_BY_SIDE_HALF*/
	bool rgb_support;
	bool y420_support;
};

struct hdmi_edid_sink_data {
	struct disp_mode_info disp_mode_list[HDMI_VFRMT_MAX];
	u32 disp_multi_3d_mode_list[16];
	u32 disp_multi_3d_mode_list_cnt;
	u32 num_of_elements;
	u32 preferred_video_format;
};

struct hdmi_edid_sink_caps {
	u32 max_pclk_in_hz;
	bool scdc_present;
	bool scramble_support; /* scramble support for less than 340Mcsc */
	bool read_req_support;
	bool osd_disparity;
	bool dual_view_support;
	bool ind_view_support;
};

struct hdmi_edid_override_data {
	int scramble;
	int sink_mode;
	int format;
	int vic;
};

struct hdmi_edid_ctrl {
	u8 pt_scan_info;
	u8 it_scan_info;
	u8 ce_scan_info;
	u8 cea_blks;
	u8 deep_color;
	u16 physical_address;
	u32 video_resolution; /* selected by user */
	u32 sink_mode; /* HDMI or DVI */
	u32 default_vic;
	u16 audio_latency;
	u16 video_latency;
	u32 present_3d;
	u32 page_id;
	u8 audio_data_block[MAX_NUMBER_ADB * MAX_AUDIO_DATA_BLOCK_SIZE];
	int adb_size;
	u8 spkr_alloc_data_block[MAX_SPKR_ALLOC_DATA_BLOCK_SIZE];
	int sadb_size;
	u8 edid_buf[MAX_EDID_SIZE];
	char vendor_id[EDID_VENDOR_ID_SIZE];
	bool keep_resv_timings;
	bool edid_override;
	bool hdr_supported;

	struct hdmi_edid_sink_data sink_data;
	struct hdmi_edid_init_data init_data;
	struct hdmi_edid_sink_caps sink_caps;
	struct hdmi_edid_override_data override_data;
	struct hdmi_edid_hdr_data hdr_data;
};

static bool hdmi_edid_is_mode_supported(struct hdmi_edid_ctrl *edid_ctrl,
			struct msm_hdmi_mode_timing_info *timing)
{
	if (!timing->supported ||
		timing->pixel_freq > edid_ctrl->init_data.max_pclk_khz)
		return false;

	return true;
}

static int hdmi_edid_reset_parser(struct hdmi_edid_ctrl *edid_ctrl)
{
	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* reset res info read page */
	edid_ctrl->page_id = MSM_HDMI_INIT_RES_PAGE;

	/* reset sink mode to DVI as default */
	edid_ctrl->sink_mode = SINK_MODE_DVI;

	edid_ctrl->sink_data.num_of_elements = 0;

	/* reset scan info data */
	edid_ctrl->pt_scan_info = 0;
	edid_ctrl->it_scan_info = 0;
	edid_ctrl->ce_scan_info = 0;

	/* reset 3d data */
	edid_ctrl->present_3d = 0;

	/* reset number of cea extension blocks to 0 */
	edid_ctrl->cea_blks = 0;

	/* reset resolution related sink data */
	memset(&edid_ctrl->sink_data, 0, sizeof(edid_ctrl->sink_data));

	/* reset audio related data */
	memset(edid_ctrl->audio_data_block, 0,
		sizeof(edid_ctrl->audio_data_block));
	memset(edid_ctrl->spkr_alloc_data_block, 0,
		sizeof(edid_ctrl->spkr_alloc_data_block));
	edid_ctrl->adb_size = 0;
	edid_ctrl->sadb_size = 0;

	hdmi_edid_set_video_resolution(edid_ctrl, edid_ctrl->default_vic, true);

	/* reset new resolution details */
	if (!edid_ctrl->keep_resv_timings)
		hdmi_reset_resv_timing_info();

	/* reset HDR related data */
	edid_ctrl->hdr_supported = false;
	edid_ctrl->hdr_data.eotf = 0;
	edid_ctrl->hdr_data.descriptor = 0;
	edid_ctrl->hdr_data.max_luminance = 0;
	edid_ctrl->hdr_data.avg_luminance = 0;
	edid_ctrl->hdr_data.min_luminance = 0;

	return 0;
}

static struct hdmi_edid_ctrl *hdmi_edid_get_ctrl(struct device *dev)
{
	struct fb_info *fbi;
	struct msm_fb_data_type *mfd;
	struct mdss_panel_info *pinfo;

	if (!dev) {
		pr_err("invlid input\n");
		goto error;
	}

	fbi = dev_get_drvdata(dev);

	if (!fbi) {
		pr_err("invlid fbi\n");
		goto error;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;
	if (!mfd) {
		pr_err("invlid mfd\n");
		goto error;
	}

	pinfo = mfd->panel_info;
	if (!pinfo) {
		pr_err("invlid pinfo\n");
		goto error;
	}

	return pinfo->edid_data;

error:
	return NULL;
}

static ssize_t hdmi_edid_sysfs_rda_audio_data_block(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int adb_size, adb_count;
	ssize_t ret;
	char *data = buf;

	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	adb_count = 1;
	adb_size  = edid_ctrl->adb_size;
	ret       = sizeof(adb_count) + sizeof(adb_size) + adb_size;

	if (ret > PAGE_SIZE) {
		DEV_DBG("%s: Insufficient buffer size\n", __func__);
		return 0;
	}

	/* Currently only extracting one audio data block */
	memcpy(data, &adb_count, sizeof(adb_count));
	data += sizeof(adb_count);
	memcpy(data, &adb_size, sizeof(adb_size));
	data += sizeof(adb_size);
	memcpy(data, edid_ctrl->audio_data_block,
			edid_ctrl->adb_size);

	print_hex_dump(KERN_DEBUG, "AUDIO DATA BLOCK: ", DUMP_PREFIX_NONE,
			32, 8, buf, ret, false);

	return ret;
}
static DEVICE_ATTR(audio_data_block, S_IRUGO,
	hdmi_edid_sysfs_rda_audio_data_block,
	NULL);

static ssize_t hdmi_edid_sysfs_rda_spkr_alloc_data_block(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int sadb_size, sadb_count;
	ssize_t ret;
	char *data = buf;

	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	sadb_count = 1;
	sadb_size  = edid_ctrl->sadb_size;
	ret        = sizeof(sadb_count) + sizeof(sadb_size) + sadb_size;

	if (ret > PAGE_SIZE) {
		DEV_DBG("%s: Insufficient buffer size\n", __func__);
		return 0;
	}

	/* Currently only extracting one speaker allocation data block */
	memcpy(data, &sadb_count, sizeof(sadb_count));
	data += sizeof(sadb_count);
	memcpy(data, &sadb_size, sizeof(sadb_size));
	data += sizeof(sadb_size);
	memcpy(data, edid_ctrl->spkr_alloc_data_block,
			edid_ctrl->sadb_size);

	print_hex_dump(KERN_DEBUG, "SPKR ALLOC DATA BLOCK: ", DUMP_PREFIX_NONE,
			32, 8, buf, ret, false);

	return ret;
}
static DEVICE_ATTR(spkr_alloc_data_block, S_IRUGO,
	hdmi_edid_sysfs_rda_spkr_alloc_data_block, NULL);

static ssize_t hdmi_edid_sysfs_wta_modes(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid ctrl\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	if (sscanf(buf, "%d %d %d %d",
		&edid_ctrl->override_data.scramble,
		&edid_ctrl->override_data.sink_mode,
		&edid_ctrl->override_data.format,
		&edid_ctrl->override_data.vic) != 4) {
		DEV_ERR("could not read input\n");
		ret = -EINVAL;
		goto bail;
	}

	edid_ctrl->edid_override = true;
	return ret;
bail:
	DEV_DBG("%s: reset edid override\n", __func__);
	edid_ctrl->edid_override = false;
error:
	return ret;
}

static ssize_t hdmi_edid_sysfs_rda_modes(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);
	u32 num_of_elements = 0;
	struct disp_mode_info *video_mode;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	num_of_elements = edid_ctrl->sink_data.num_of_elements;
	video_mode = edid_ctrl->sink_data.disp_mode_list;

	if (edid_ctrl->edid_override && (edid_ctrl->override_data.vic > 0)) {
		num_of_elements = 1;
		edid_ctrl->sink_data.disp_mode_list[0].video_format =
			edid_ctrl->override_data.vic;
	}

	buf[0] = 0;
	if (num_of_elements) {
		for (i = 0; i < num_of_elements; i++) {
			if (ret > 0)
				ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					",%d", video_mode[i].video_format);
			else
				ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					"%d", video_mode[i].video_format);
		}
	} else {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%d",
			edid_ctrl->video_resolution);
	}

	DEV_DBG("%s: '%s'\n", __func__, buf);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	return ret;
} /* hdmi_edid_sysfs_rda_modes */
static DEVICE_ATTR(edid_modes, S_IRUGO | S_IWUSR, hdmi_edid_sysfs_rda_modes,
	hdmi_edid_sysfs_wta_modes);

static ssize_t hdmi_edid_sysfs_rda_res_info_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	u32 i, no_of_elem, offset = 0;
	struct msm_hdmi_mode_timing_info info = {0};
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);
	struct disp_mode_info *minfo = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	no_of_elem = edid_ctrl->sink_data.num_of_elements;
	minfo = edid_ctrl->sink_data.disp_mode_list;

	if (edid_ctrl->edid_override && (edid_ctrl->override_data.vic > 0)) {
		no_of_elem = 1;
		minfo[0].video_format = edid_ctrl->override_data.vic;
	}

	for (i = 0; i < no_of_elem; i++) {
		ret = hdmi_get_supported_mode(&info,
			&edid_ctrl->init_data.ds_data,
			minfo->video_format);

		if (edid_ctrl->edid_override &&
			(edid_ctrl->override_data.format > 0))
			info.pixel_formats = edid_ctrl->override_data.format;
		else
			info.pixel_formats =
			    (minfo->rgb_support ?
				MSM_HDMI_RGB_888_24BPP_FORMAT : 0) |
			    (minfo->y420_support ?
				MSM_HDMI_YUV_420_12BPP_FORMAT : 0);

		minfo++;
		if (ret || !info.supported)
			continue;

		offset += scnprintf(buf + offset, PAGE_SIZE - offset,
			"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			info.video_format, info.active_h,
			info.front_porch_h, info.pulse_width_h,
			info.back_porch_h, info.active_low_h,
			info.active_v, info.front_porch_v,
			info.pulse_width_v, info.back_porch_v,
			info.active_low_v, info.pixel_freq,
			info.refresh_rate, info.interlaced,
			info.supported, info.ar,
			info.pixel_formats);
	}

	return offset;
}
static DEVICE_ATTR(res_info_data, S_IRUGO, hdmi_edid_sysfs_rda_res_info_data,
	NULL);

static ssize_t hdmi_edid_sysfs_wta_res_info(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc, page_id;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = kstrtoint(buf, 10, &page_id);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		return rc;
	}

	edid_ctrl->page_id = page_id;

	DEV_DBG("%s: %d\n", __func__, edid_ctrl->page_id);
	return ret;
}

static ssize_t hdmi_edid_sysfs_rda_res_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	u32 no_of_elem;
	u32 i = 0, j, page;
	char *buf_dbg = buf;
	struct msm_hdmi_mode_timing_info info = {0};
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);
	u32 size_to_write = sizeof(info);
	struct disp_mode_info *minfo = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	minfo = edid_ctrl->sink_data.disp_mode_list;
	no_of_elem = edid_ctrl->sink_data.num_of_elements;

	if (edid_ctrl->page_id > MSM_HDMI_INIT_RES_PAGE) {
		page = MSM_HDMI_INIT_RES_PAGE;
		while (page < edid_ctrl->page_id) {
			j = 1;
			while (sizeof(info) * j < PAGE_SIZE) {
				i++;
				j++;
				minfo++;
			}
			page++;
		}
	}

	if (edid_ctrl->edid_override && (edid_ctrl->override_data.vic > 0)) {
		no_of_elem = 1;
		minfo[0].video_format = edid_ctrl->override_data.vic;
	}

	for (; i < no_of_elem && size_to_write < PAGE_SIZE; i++) {
		ret = hdmi_get_supported_mode(&info,
			&edid_ctrl->init_data.ds_data,
			minfo->video_format);

		if (edid_ctrl->edid_override &&
			(edid_ctrl->override_data.format > 0))
			info.pixel_formats = edid_ctrl->override_data.format;
		else
			info.pixel_formats =
			    (minfo->rgb_support ?
				 MSM_HDMI_RGB_888_24BPP_FORMAT : 0) |
			    (minfo->y420_support ?
				MSM_HDMI_YUV_420_12BPP_FORMAT : 0);

		minfo++;
		if (ret || !info.supported)
			continue;

		memcpy(buf, &info, sizeof(info));

		buf += sizeof(info);
		size_to_write += sizeof(info);
	}

	for (i = sizeof(info); i < size_to_write; i += sizeof(info)) {
		struct msm_hdmi_mode_timing_info info_dbg = {0};

		memcpy(&info_dbg, buf_dbg, sizeof(info_dbg));

		DEV_DBG("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			info_dbg.video_format, info_dbg.active_h,
			info_dbg.front_porch_h, info_dbg.pulse_width_h,
			info_dbg.back_porch_h, info_dbg.active_low_h,
			info_dbg.active_v, info_dbg.front_porch_v,
			info_dbg.pulse_width_v, info_dbg.back_porch_v,
			info_dbg.active_low_v, info_dbg.pixel_freq,
			info_dbg.refresh_rate, info_dbg.interlaced,
			info_dbg.supported, info_dbg.ar,
			info_dbg.pixel_formats);

		buf_dbg += sizeof(info_dbg);
	}

	return size_to_write - sizeof(info);
}
static DEVICE_ATTR(res_info, S_IRUGO | S_IWUSR, hdmi_edid_sysfs_rda_res_info,
	hdmi_edid_sysfs_wta_res_info);

static ssize_t hdmi_edid_sysfs_rda_audio_latency(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", edid_ctrl->audio_latency);

	DEV_DBG("%s: '%s'\n", __func__, buf);

	return ret;
} /* hdmi_edid_sysfs_rda_audio_latency */
static DEVICE_ATTR(edid_audio_latency, S_IRUGO,
	hdmi_edid_sysfs_rda_audio_latency, NULL);

static ssize_t hdmi_edid_sysfs_rda_video_latency(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", edid_ctrl->video_latency);

	DEV_DBG("%s: '%s'\n", __func__, buf);

	return ret;
} /* hdmi_edid_sysfs_rda_video_latency */
static DEVICE_ATTR(edid_video_latency, S_IRUGO,
	hdmi_edid_sysfs_rda_video_latency, NULL);

static ssize_t hdmi_edid_sysfs_rda_physical_address(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", edid_ctrl->physical_address);
	DEV_DBG("%s: '%d'\n", __func__, edid_ctrl->physical_address);

	return ret;
} /* hdmi_edid_sysfs_rda_physical_address */
static DEVICE_ATTR(pa, S_IRUSR, hdmi_edid_sysfs_rda_physical_address, NULL);

static ssize_t hdmi_edid_sysfs_rda_scan_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d, %d, %d\n", edid_ctrl->pt_scan_info,
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
	char buff_3d[BUFF_SIZE_3D];

	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	buf[0] = 0;
	if (edid_ctrl->sink_data.num_of_elements) {
		struct disp_mode_info *video_mode =
			edid_ctrl->sink_data.disp_mode_list;

		for (i = 0; i < edid_ctrl->sink_data.num_of_elements; i++) {
			if (!video_mode[i].video_3d_format)
				continue;
			hdmi_get_video_3d_fmt_2string(
					video_mode[i].video_3d_format,
					buff_3d,
					sizeof(buff_3d));
			if (ret > 0)
				ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					",%d=%s", video_mode[i].video_format,
					buff_3d);
			else
				ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					"%d=%s", video_mode[i].video_format,
					buff_3d);
		}
	}

	DEV_DBG("%s: '%s'\n", __func__, buf);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	return ret;
} /* hdmi_edid_sysfs_rda_3d_modes */
static DEVICE_ATTR(edid_3d_modes, S_IRUGO, hdmi_edid_sysfs_rda_3d_modes, NULL);

static ssize_t hdmi_common_rda_edid_raw_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);
	u32 size;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	size  = sizeof(edid_ctrl->edid_buf) < PAGE_SIZE ?
			sizeof(edid_ctrl->edid_buf) : PAGE_SIZE;

	/* buf can have max size of PAGE_SIZE */
	memcpy(buf, edid_ctrl->edid_buf, size);

	return size;
} /* hdmi_common_rda_edid_raw_data */
static DEVICE_ATTR(edid_raw_data, S_IRUGO, hdmi_common_rda_edid_raw_data, NULL);

static ssize_t hdmi_edid_sysfs_wta_add_resolution(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);
	struct msm_hdmi_mode_timing_info timing;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	rc = sscanf(buf,
		"%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
		(unsigned long *) &timing.active_h,
		(unsigned long *) &timing.front_porch_h,
		(unsigned long *) &timing.pulse_width_h,
		(unsigned long *) &timing.back_porch_h,
		(unsigned long *) &timing.active_low_h,
		(unsigned long *) &timing.active_v,
		(unsigned long *) &timing.front_porch_v,
		(unsigned long *) &timing.pulse_width_v,
		(unsigned long *) &timing.back_porch_v,
		(unsigned long *) &timing.active_low_v,
		(unsigned long *) &timing.pixel_freq,
		(unsigned long *) &timing.refresh_rate,
		(unsigned long *) &timing.interlaced,
		(unsigned long *) &timing.supported,
		(unsigned long *) &timing.ar);

	if (rc != 15) {
		DEV_ERR("%s: error reading buf\n", __func__);
		goto err;
	}

	rc = hdmi_set_resv_timing_info(&timing);

	if (!IS_ERR_VALUE(rc)) {
		DEV_DBG("%s: added new res %d\n", __func__, rc);
	} else {
		DEV_ERR("%s: error adding new res %d\n", __func__, rc);
		goto err;
	}

	edid_ctrl->keep_resv_timings = true;
	return ret;

err:
	edid_ctrl->keep_resv_timings = false;
	return -EFAULT;
}
static DEVICE_ATTR(add_res, S_IWUSR, NULL, hdmi_edid_sysfs_wta_add_resolution);

static ssize_t hdmi_edid_sysfs_rda_hdr_data(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_edid_ctrl *edid_ctrl = hdmi_edid_get_ctrl(dev);

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d, %u, %u, %u, %u, %u\n",
			edid_ctrl->hdr_supported,
			edid_ctrl->hdr_data.eotf,
			edid_ctrl->hdr_data.descriptor,
			edid_ctrl->hdr_data.max_luminance,
			edid_ctrl->hdr_data.avg_luminance,
			edid_ctrl->hdr_data.min_luminance);
	DEV_DBG("%s: '%s'\n", __func__, buf);

	return ret;
}
static DEVICE_ATTR(hdr_data, S_IRUGO, hdmi_edid_sysfs_rda_hdr_data, NULL);

static struct attribute *hdmi_edid_fs_attrs[] = {
	&dev_attr_edid_modes.attr,
	&dev_attr_pa.attr,
	&dev_attr_scan_info.attr,
	&dev_attr_edid_3d_modes.attr,
	&dev_attr_edid_raw_data.attr,
	&dev_attr_audio_data_block.attr,
	&dev_attr_spkr_alloc_data_block.attr,
	&dev_attr_edid_audio_latency.attr,
	&dev_attr_edid_video_latency.attr,
	&dev_attr_res_info.attr,
	&dev_attr_res_info_data.attr,
	&dev_attr_add_res.attr,
	&dev_attr_hdr_data.attr,
	NULL,
};

static struct attribute_group hdmi_edid_fs_attrs_group = {
	.attrs = hdmi_edid_fs_attrs,
};

static const u8 *hdmi_edid_find_block(const u8 *in_buf, u32 start_offset,
	u8 type, u8 *len)
{
	/* the start of data block collection, start of Video Data Block */
	u32 offset = start_offset;
	u32 dbc_offset = in_buf[2];

	if (dbc_offset >= EDID_BLOCK_SIZE - EDID_DTD_LEN)
		return NULL;
	*len = 0;

	/*
	 * * edid buffer 1, byte 2 being 4 means no non-DTD/Data block
	 *   collection present.
	 * * edid buffer 1, byte 2 being 0 menas no non-DTD/DATA block
	 *   collection present and no DTD data present.
	 */
	if ((dbc_offset == 0) || (dbc_offset == 4)) {
		DEV_WARN("EDID: no DTD or non-DTD data present\n");
		return NULL;
	}

	while (offset < dbc_offset) {
		u8 block_len = in_buf[offset] & 0x1F;
		if ((offset + block_len <= dbc_offset) &&
		    (in_buf[offset] >> 5) == type) {
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

static void hdmi_edid_set_y420_support(struct hdmi_edid_ctrl *edid_ctrl,
				  u32 video_format)
{
	u32 i = 0;

	if (!edid_ctrl) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return;
	}

	for (i = 0; i < edid_ctrl->sink_data.num_of_elements; ++i) {
		if (video_format ==
		    edid_ctrl->sink_data.disp_mode_list[i].video_format) {
			edid_ctrl->sink_data.disp_mode_list[i].y420_support =
				true;
			DEV_DBG("%s: Yuv420 supported for format %d\n",
			 __func__,
			edid_ctrl->sink_data.disp_mode_list[i].video_format);
		}
	}
}

static void hdmi_edid_add_sink_y420_format(struct hdmi_edid_ctrl *edid_ctrl,
					   u32 video_format)
{
	struct msm_hdmi_mode_timing_info timing = {0};
	u32 ret = hdmi_get_supported_mode(&timing,
				&edid_ctrl->init_data.ds_data,
				video_format);
	u32 supported = hdmi_edid_is_mode_supported(edid_ctrl, &timing);
	struct hdmi_edid_sink_data *sink = &edid_ctrl->sink_data;

	if (video_format >= HDMI_VFRMT_MAX) {
		DEV_ERR("%s: video format: %s is not supported\n", __func__,
			msm_hdmi_mode_2string(video_format));
		return;
	}

	if (!sink) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	DEV_DBG("%s: EDID: format: %d [%s], %s\n", __func__,
		video_format, msm_hdmi_mode_2string(video_format),
		supported ? "Supported" : "Not-Supported");

	if (!ret && supported) {
		sink->disp_mode_list[sink->num_of_elements].video_format
			= video_format;
		sink->disp_mode_list[sink->num_of_elements].y420_support
			= true;
		sink->num_of_elements++;
	}
}

static void hdmi_edid_parse_Y420VDB(struct hdmi_edid_ctrl *edid_ctrl,
				    const u8 *in_buf)
{
	u8 len = 0;
	u8 i = 0;
	u32 video_format = 0;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	len = in_buf[0] & 0x1F;
	/* Offset to byte 3 */
	in_buf += 2;
	for (i = 0; i < len - 1; i++) {
		video_format = *(in_buf + i) & 0x7F;
		hdmi_edid_add_sink_y420_format(edid_ctrl, video_format);
	}
}

static bool hdmi_edid_is_luminance_value_present(u32 block_length,
		enum luminance_value value)
{
	return block_length > NO_LUMINANCE_DATA && value <= block_length;
}

static void hdmi_edid_parse_hdrdb(struct hdmi_edid_ctrl *edid_ctrl,
		const u8 *data_block)
{
	u8 len = 0;

	if (!edid_ctrl || !data_block) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	/* Byte 1: Length of Data Block */
	len = data_block[0] & 0x1F;

	/* Byte 3: Electro-Optical Transfer Functions */
	edid_ctrl->hdr_data.eotf = data_block[2] & 0x3F;

	/* Byte 4: Static Metadata Descriptors */
	edid_ctrl->hdr_data.descriptor = data_block[3] & 0x1;

	/* Byte 5: Desired Content Maximum Luminance */
	if (hdmi_edid_is_luminance_value_present(len, MAXIMUM_LUMINANCE))
		edid_ctrl->hdr_data.max_luminance =
			data_block[MAXIMUM_LUMINANCE];

	/* Byte 6: Desired Content Max Frame-average Luminance */
	if (hdmi_edid_is_luminance_value_present(len, FRAME_AVERAGE_LUMINANCE))
		edid_ctrl->hdr_data.avg_luminance =
			data_block[FRAME_AVERAGE_LUMINANCE];

	/* Byte 7: Desired Content Min Luminance */
	if (hdmi_edid_is_luminance_value_present(len, MINIMUM_LUMINANCE))
		edid_ctrl->hdr_data.min_luminance =
			data_block[MINIMUM_LUMINANCE];
}

static void hdmi_edid_parse_Y420CMDB(struct hdmi_edid_ctrl *edid_ctrl,
				     const u8 *in_buf)
{
	u32 offset = 0;
	u8 svd_len = 0;
	u32 i = 0, j = 0;
	u32 video_format = 0;
	u32 len = 0;
	const u8 *svd = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}
	 /* Byte 3 to L+1 contain SVDs */
	offset += 2;
	len = in_buf[0] & 0x1F;

	/*
	 * The Y420 Capability map data block should be parsed along with the
	 * video data block. Each bit in Y420CMDB maps to each SVD in data
	 * block
	 */
	svd = hdmi_edid_find_block(edid_ctrl->edid_buf+0x80, DBC_START_OFFSET,
			VIDEO_DATA_BLOCK, &svd_len);

	++svd;
	for (i = 0; i < svd_len; i++, j++) {
		video_format = *svd & 0x7F;
		if (in_buf[offset] & (1 << j))
			hdmi_edid_set_y420_support(edid_ctrl, video_format);

		if (j & 0x80) {
			j = j/8;
			offset++;
			if (offset >= len)
				break;
		}
	}
}

static void hdmi_edid_parse_hvdb(struct hdmi_edid_ctrl *edid_ctrl,
				 const u8 *in_buf)
{
	u32 len = 0;
	struct hdmi_edid_sink_caps *sink_caps = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	sink_caps = &edid_ctrl->sink_caps;
	len = in_buf[0] & 0x1F;
	if ((in_buf[1] != HDMI_VIDEO_DATA_BLOCK) ||
	    (len < 5)) {
		DEV_ERR("%s: Not a HVDB tag code\n", __func__);
		return;
	}
	DEV_ERR("FOUND HVDB flags = 0x%x\n", in_buf[4]);
	sink_caps->max_pclk_in_hz = in_buf[3]*5000;
	sink_caps->scdc_present = (in_buf[4] & 0x80) ? true : false;
	sink_caps->read_req_support = (in_buf[4] & 0x40) ? true : false;
	sink_caps->scramble_support = (in_buf[4] & 0x08) ? true : false;
	sink_caps->ind_view_support = (in_buf[4] & 0x04) ? true : false;
	sink_caps->dual_view_support = (in_buf[4] & 0x02) ? true : false;
	sink_caps->osd_disparity = (in_buf[4] * 0x01) ? true : false;

}

static void hdmi_edid_extract_extended_data_blocks(
	struct hdmi_edid_ctrl *edid_ctrl, const u8 *in_buf)
{
	u8 len = 0;
	u32 start_offset = 0;
	u8 const *etag = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	do {
		if (!start_offset && !etag)
			start_offset = DBC_START_OFFSET;
		else
			start_offset = etag - in_buf + len + 1;

		etag = hdmi_edid_find_block(in_buf, start_offset,
			USE_EXTENDED_TAG, &len);

		if (!etag || !len) {
			DEV_DBG("%s: No more extended block found\n", __func__);
			break;
		}

		/* The extended data block should at least be 2 bytes long */
		if (len < 2) {
			DEV_DBG("%s: invalid block size\n", __func__);
			continue;
		}

		/*
		 * The second byte of the extended data block has the
		 * extended tag code
		 */
		switch (etag[1]) {
		case VIDEO_CAPABILITY_DATA_BLOCK:
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
		case HDMI_VIDEO_DATA_BLOCK:
			/* HDMI Video data block defined in HDMI 2.0 */
			DEV_DBG("%s: EDID: HVDB found\n", __func__);
			hdmi_edid_parse_hvdb(edid_ctrl, etag);
			break;
		case Y420_CAPABILITY_MAP_DATA_BLOCK:
			DEV_DBG("%s found Y420CMDB byte 3 = 0x%x",
				__func__, etag[2]);
			hdmi_edid_parse_Y420CMDB(edid_ctrl, etag);
			break;
		case Y420_VIDEO_DATA_BLOCK:
			DEV_DBG("%s found Y420VDB byte 3 = 0x%x",
				__func__, etag[2]);
			hdmi_edid_parse_Y420VDB(edid_ctrl, etag);
			break;
		case HDR_STATIC_METADATA_DATA_BLOCK:
			DEV_DBG("%s found HDR Static Metadata. Byte 3 = 0x%x",
				__func__, etag[2]);
			hdmi_edid_parse_hdrdb(edid_ctrl, etag);
			edid_ctrl->hdr_supported = true;
			break;
		default:
			DEV_DBG("%s: Tag Code %d not supported\n",
				__func__, etag[1]);
			break;
		}
	} while (1);
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

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET,
		VENDOR_SPECIFIC_DATA_BLOCK, &len);

	edid_ctrl->present_3d = 0;
	if (vsd == NULL || len == 0 || len > MAX_DATA_BLOCK_SIZE) {
		DEV_DBG("%s: No/Invalid vendor Specific Data Block\n",
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
	u8 len = 0;
	u8 adb_max = 0;
	const u8 *adb = NULL;
	u32 offset = DBC_START_OFFSET;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	edid_ctrl->adb_size = 0;

	memset(edid_ctrl->audio_data_block, 0,
		sizeof(edid_ctrl->audio_data_block));

	do {
		len = 0;
		adb = hdmi_edid_find_block(in_buf, offset, AUDIO_DATA_BLOCK,
			&len);

		if ((adb == NULL) || (len > MAX_AUDIO_DATA_BLOCK_SIZE ||
			adb_max >= MAX_NUMBER_ADB)) {
			if (!edid_ctrl->adb_size) {
				DEV_DBG("%s: No/Invalid Audio Data Block\n",
					__func__);
				return;
			} else {
				DEV_DBG("%s: No more valid ADB found\n",
					__func__);
			}

			continue;
		}

		memcpy(edid_ctrl->audio_data_block + edid_ctrl->adb_size,
			adb + 1, len);
		offset = (adb - in_buf) + 1 + len;

		edid_ctrl->adb_size += len;
		adb_max++;
	} while (adb);

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

	sadb = hdmi_edid_find_block(in_buf, DBC_START_OFFSET,
		SPEAKER_ALLOCATION_DATA_BLOCK, &len);
	if ((sadb == NULL) || (len != MAX_SPKR_ALLOC_DATA_BLOCK_SIZE)) {
		DEV_DBG("%s: No/Invalid Speaker Allocation Data Block\n",
			__func__);
		return;
	}

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

static void hdmi_edid_extract_sink_caps(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *in_buf)
{
	u8 len = 0, i = 0;
	const u8 *vsd = NULL;
	u32 vsd_offset = DBC_START_OFFSET;
	u32 hf_ieee_oui = 0;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	/* Find HF-VSDB with HF-OUI */
	do {
		vsd = hdmi_edid_find_block(in_buf, vsd_offset,
			   VENDOR_SPECIFIC_DATA_BLOCK, &len);

		if (!vsd || !len || len > MAX_DATA_BLOCK_SIZE) {
			if (i == 0)
				DEV_ERR("%s: VSDB not found\n", __func__);
			else
				DEV_DBG("%s: no more VSDB found\n", __func__);
			break;
		}

		hf_ieee_oui = (vsd[1] << 16) | (vsd[2] << 8) | vsd[3];

		if (hf_ieee_oui == HDMI_FORUM_IEEE_OUI) {
			DEV_DBG("%s: found HF-VSDB\n", __func__);
			break;
		}

		DEV_DBG("%s: Not a HF OUI 0x%x\n", __func__, hf_ieee_oui);

		i++;
		vsd_offset = vsd - in_buf + len + 1;
	} while (1);

	if (!vsd) {
		DEV_DBG("%s: HF-VSDB not found\n", __func__);
		return;
	}

	/* Max pixel clock is in  multiples of 5Mhz. */
	edid_ctrl->sink_caps.max_pclk_in_hz =
			vsd[5]*5000000;
	edid_ctrl->sink_caps.scdc_present =
			(vsd[6] & 0x80) ? true : false;
	edid_ctrl->sink_caps.scramble_support =
			(vsd[6] & 0x08) ? true : false;
	edid_ctrl->sink_caps.read_req_support =
			(vsd[6] & 0x40) ? true : false;
	edid_ctrl->sink_caps.osd_disparity =
			(vsd[6] & 0x01) ? true : false;
	edid_ctrl->sink_caps.dual_view_support =
			(vsd[6] & 0x02) ? true : false;
	edid_ctrl->sink_caps.ind_view_support =
			(vsd[6] & 0x04) ? true : false;
}

static void hdmi_edid_extract_latency_fields(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *in_buf)
{
	u8 len;
	const u8 *vsd = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET,
		VENDOR_SPECIFIC_DATA_BLOCK, &len);

	if (vsd == NULL || len == 0 || len > MAX_DATA_BLOCK_SIZE ||
		!(vsd[8] & BIT(7))) {
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

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET,
		VENDOR_SPECIFIC_DATA_BLOCK, &len);

	if (vsd == NULL || len == 0 || len > MAX_DATA_BLOCK_SIZE) {
		DEV_DBG("%s: No/Invalid Vendor Specific Data Block\n",
			__func__);
		return 0;
	}

	DEV_DBG("%s: EDID: VSD PhyAddr=%04x, MaxTMDS=%dMHz\n", __func__,
		((u32)vsd[4] << 8) + (u32)vsd[5], (u32)vsd[7] * 5);

	edid_ctrl->physical_address = ((u16)vsd[4] << 8) + (u16)vsd[5];

	return ((u32)vsd[3] << 16) + ((u32)vsd[2] << 8) + (u32)vsd[1];
} /* hdmi_edid_extract_ieee_reg_id */

static void hdmi_edid_extract_vendor_id(struct hdmi_edid_ctrl *edid_ctrl)
{
	char *vendor_id;
	u32 id_codes;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	vendor_id = edid_ctrl->vendor_id;
	id_codes = ((u32)edid_ctrl->edid_buf[8] << 8) +
			edid_ctrl->edid_buf[9];

	vendor_id[0] = 'A' - 1 + ((id_codes >> 10) & 0x1F);
	vendor_id[1] = 'A' - 1 + ((id_codes >> 5) & 0x1F);
	vendor_id[2] = 'A' - 1 + (id_codes & 0x1F);
	vendor_id[3] = 0;
} /* hdmi_edid_extract_vendor_id */

static void hdmi_edid_extract_dc(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *in_buf)
{
	u8 len;
	const u8 *vsd = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET,
		VENDOR_SPECIFIC_DATA_BLOCK, &len);

	if (vsd == NULL || len == 0 || len > MAX_DATA_BLOCK_SIZE)
		return;

	edid_ctrl->deep_color = (vsd[6] >> 0x3) & 0xF;

	DEV_DBG("%s: deep color: Y444|RGB30|RGB36|RGB48: (%d|%d|%d|%d)\n",
		__func__,
		(int) (edid_ctrl->deep_color & BIT(0)) >> 0,
		(int) (edid_ctrl->deep_color & BIT(1)) >> 1,
		(int) (edid_ctrl->deep_color & BIT(2)) >> 2,
		(int) (edid_ctrl->deep_color & BIT(3)) >> 3);
}

static u32 hdmi_edid_check_header(const u8 *edid_buf)
{
	return (edid_buf[0] == 0x00) && (edid_buf[1] == 0xff)
		&& (edid_buf[2] == 0xff) && (edid_buf[3] == 0xff)
		&& (edid_buf[4] == 0xff) && (edid_buf[5] == 0xff)
		&& (edid_buf[6] == 0xff) && (edid_buf[7] == 0x00);
} /* hdmi_edid_check_header */

static void hdmi_edid_detail_desc(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *data_buf, u32 *disp_mode)
{
	u32	aspect_ratio_4_3    = false;
	u32	aspect_ratio_5_4    = false;
	u32	interlaced          = false;
	u32	active_h            = 0;
	u32	active_v            = 0;
	u32	blank_h             = 0;
	u32	blank_v             = 0;
	u32	img_size_h          = 0;
	u32	img_size_v          = 0;
	u32	pixel_clk           = 0;
	u32	front_porch_h       = 0;
	u32	front_porch_v       = 0;
	u32	pulse_width_h       = 0;
	u32	pulse_width_v       = 0;
	u32	active_low_h        = 0;
	u32	active_low_v        = 0;
	const u32 khz_to_hz         = 1000;
	u32 frame_data;
	struct msm_hdmi_mode_timing_info timing = {0};
	int rc;

	/*
	 * Pixel clock/ 10,000
	 * LSB stored in byte 0 and MSB stored in byte 1
	 */
	pixel_clk = (u32) (data_buf[0x0] | (data_buf[0x1] << 8));

	/* store pixel clock in /1000 terms */
	pixel_clk *= 10;

	/*
	 * byte 0x8 -- Horizontal Front Porch - contains lower 8 bits
	 * byte 0xb (bits 6, 7) -- contains upper 2 bits
	 */
	front_porch_h = (u32) (data_buf[0x8] |
		(data_buf[0xb] & (0x3 << 6)) << 2);

	/*
	 * byte 0x9 -- Horizontal pulse width - contains lower 8 bits
	 * byte 0xb (bits 4, 5) -- contains upper 2 bits
	 */
	pulse_width_h = (u32) (data_buf[0x9] |
		(data_buf[0xb] & (0x3 << 4)) << 4);

	/*
	 * byte 0xa -- Vertical front porch -- stored in Upper Nibble,
	 * contains lower 4 bits.
	 * byte 0xb (bits 2, 3) -- contains upper 2 bits
	 */
	front_porch_v = (u32) (((data_buf[0xa] & (0xF << 4)) >> 4) |
		(data_buf[0xb] & (0x3 << 2)) << 2);

	/*
	 * byte 0xa -- Vertical pulse width -- stored in Lower Nibble,
	 * contains lower 4 bits.
	 * byte 0xb (bits 0, 1) -- contains upper 2 bits
	 */
	pulse_width_v = (u32) ((data_buf[0xa] & 0xF) |
		((data_buf[0xb] & 0x3) << 4));

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
	 * aspect ratio as 4:3 if within specificed range, rather than being
	 * absolute value
	 */
	aspect_ratio_4_3 = (abs(img_size_h * 3 - img_size_v * 4) < 5) ? 1 : 0;

	aspect_ratio_5_4 = (abs(img_size_h * 4 - img_size_v * 5) < 5) ? 1 : 0;

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

	active_low_v = ((data_buf[0x11] & (0x7 << 2)) >> 2) == 0x7 ? 0 : 1;

	active_low_h = ((data_buf[0x11] & BIT(1)) &&
				(data_buf[0x11] & BIT(4))) ? 0 : 1;

	frame_data = (active_h + blank_h) * (active_v + blank_v);

	if (frame_data) {
		int refresh_rate_khz = (pixel_clk * khz_to_hz) / frame_data;

		timing.active_h      = active_h;
		timing.front_porch_h = front_porch_h;
		timing.pulse_width_h = pulse_width_h;
		timing.back_porch_h  = blank_h -
					(front_porch_h + pulse_width_h);
		timing.active_low_h  = active_low_h;
		timing.active_v      = active_v;
		timing.front_porch_v = front_porch_v;
		timing.pulse_width_v = pulse_width_v;
		timing.back_porch_v  = blank_v -
					(front_porch_v + pulse_width_v);
		timing.active_low_v  = active_low_v;
		timing.pixel_freq    = pixel_clk;
		timing.refresh_rate  = refresh_rate_khz * khz_to_hz;
		timing.interlaced    = interlaced;
		timing.supported     = true;
		timing.ar            = aspect_ratio_4_3 ? HDMI_RES_AR_4_3 :
					(aspect_ratio_5_4 ? HDMI_RES_AR_5_4 :
					HDMI_RES_AR_16_9);

		DEV_DBG("%s: new res: %dx%d%s@%dHz\n", __func__,
			timing.active_h, timing.active_v,
			interlaced ? "i" : "p",
			timing.refresh_rate / khz_to_hz);

		rc = hdmi_set_resv_timing_info(&timing);
	} else {
		DEV_ERR("%s: Invalid frame data\n", __func__);
		rc = -EINVAL;
	}

	if (!IS_ERR_VALUE(rc)) {
		*disp_mode = rc;
		DEV_DBG("%s: DTD mode found: %d\n", __func__, *disp_mode);
	} else {
		*disp_mode = HDMI_VFRMT_UNKNOWN;
		DEV_ERR("%s: error adding mode from DTD: %d\n", __func__, rc);
	}
} /* hdmi_edid_detail_desc */

static void hdmi_edid_add_sink_3d_format(struct hdmi_edid_sink_data *sink_data,
	u32 video_format, u32 video_3d_format)
{
	char string[BUFF_SIZE_3D];
	u32 added = false;
	int i;

	for (i = 0; i < sink_data->num_of_elements; ++i) {
		if (sink_data->disp_mode_list[i].video_format == video_format) {
			sink_data->disp_mode_list[i].video_3d_format |=
				video_3d_format;
			added = true;
			break;
		}
	}

	hdmi_get_video_3d_fmt_2string(video_3d_format, string, sizeof(string));

	DEV_DBG("%s: EDID[3D]: format: %d [%s], %s %s\n", __func__,
		video_format, msm_hdmi_mode_2string(video_format),
		string, added ? "added" : "NOT added");
} /* hdmi_edid_add_sink_3d_format */

static void hdmi_edid_add_sink_video_format(struct hdmi_edid_ctrl *edid_ctrl,
	u32 video_format)
{
	struct msm_hdmi_mode_timing_info timing = {0};
	u32 ret = hdmi_get_supported_mode(&timing,
				&edid_ctrl->init_data.ds_data,
				video_format);
	u32 supported = hdmi_edid_is_mode_supported(edid_ctrl, &timing);
	struct hdmi_edid_sink_data *sink_data = &edid_ctrl->sink_data;
	struct disp_mode_info *disp_mode_list = sink_data->disp_mode_list;

	if (video_format >= HDMI_VFRMT_MAX) {
		DEV_ERR("%s: video format: %s is not supported\n", __func__,
			msm_hdmi_mode_2string(video_format));
		return;
	}

	DEV_DBG("%s: EDID: format: %d [%s], %s\n", __func__,
		video_format, msm_hdmi_mode_2string(video_format),
		supported ? "Supported" : "Not-Supported");

	if (!ret && supported) {
		/* todo: MHL */
		disp_mode_list[sink_data->num_of_elements].video_format =
			video_format;
		disp_mode_list[sink_data->num_of_elements].rgb_support =
			true;
		sink_data->num_of_elements++;
	}
} /* hdmi_edid_add_sink_video_format */

static int hdmi_edid_get_display_vsd_3d_mode(const u8 *data_buf,
	struct hdmi_edid_sink_data *sink_data, u32 num_of_cea_blocks)
{
	u8 len, offset, present_multi_3d, hdmi_vic_len;
	int hdmi_3d_len;
	u16 structure_all, structure_mask;
	const u8 *vsd = num_of_cea_blocks ?
		hdmi_edid_find_block(data_buf+0x80, DBC_START_OFFSET,
			VENDOR_SPECIFIC_DATA_BLOCK, &len) : NULL;
	int i;

	if (vsd == NULL || len == 0 || len > MAX_DATA_BLOCK_SIZE) {
		DEV_DBG("%s: No/Invalid Vendor Specific Data Block\n",
			__func__);
		return -ENXIO;
	}

	offset = HDMI_VSDB_3D_EVF_DATA_OFFSET(vsd);
	if (offset >= len - 1)
		return -ETOOSMALL;

	present_multi_3d = (vsd[offset] & 0x60) >> 5;

	offset += 1;

	hdmi_vic_len = (vsd[offset] >> 5) & 0x7;
	hdmi_3d_len = vsd[offset] & 0x1F;
	DEV_DBG("%s: EDID[3D]: HDMI_VIC_LEN = %d, HDMI_3D_LEN = %d\n", __func__,
		hdmi_vic_len, hdmi_3d_len);

	offset += (hdmi_vic_len + 1);
	if (offset >= len - 1)
		return -ETOOSMALL;

	if (present_multi_3d == 1 || present_multi_3d == 2) {
		DEV_DBG("%s: EDID[3D]: multi 3D present (%d)\n", __func__,
			present_multi_3d);
		/* 3d_structure_all */
		structure_all = (vsd[offset] << 8) | vsd[offset + 1];
		offset += 2;
		if (offset >= len - 1)
			return -ETOOSMALL;
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
		if (offset >= len - 1)
			return -ETOOSMALL;
		DEV_DBG("%s: EDID: 3D_Structure_%d @ 0x%x: %02x\n",
			__func__, i + 1, offset, vsd[offset]);
		if ((vsd[offset] >> 4) >=
			sink_data->disp_multi_3d_mode_list_cnt) {
			if ((vsd[offset] & 0x0F) >= 8) {
				offset += 1;
				hdmi_3d_len -= 1;
				DEV_DBG("%s:EDID:3D_Detail_%d @ 0x%x: %02x\n",
					__func__, i + 1, offset,
					vsd[min_t(u32, offset, (len - 1))]);
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
				vsd[min_t(u32, offset, (len - 1))]);
		}
		i += 1;
		offset += 1;
		hdmi_3d_len -= 1;
	}
	return 0;
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

	vsd = hdmi_edid_find_block(in_buf, DBC_START_OFFSET,
		VENDOR_SPECIFIC_DATA_BLOCK, &db_len);

	if (vsd == NULL || db_len == 0 || db_len > MAX_DATA_BLOCK_SIZE) {
		DEV_DBG("%s: No/Invalid Vendor Specific Data Block\n",
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
			hdmi_edid_add_sink_video_format(edid_ctrl,
				video_format);
		}
	}
} /* hdmi_edid_get_extended_video_formats */

static void hdmi_edid_parse_et3(struct hdmi_edid_ctrl *edid_ctrl,
	const u8 *edid_blk0)
{
	u8  start = DTD_OFFSET, i = 0;
	struct hdmi_edid_sink_data *sink_data = NULL;

	if (!edid_ctrl || !edid_blk0) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	sink_data = &edid_ctrl->sink_data;

	/* check if the EDID revision is 4 (version 1.4) */
	if (edid_blk0[REVISION_OFFSET] != EDID_REVISION_FOUR)
		return;

	/* Check each of 4 - 18 bytes descriptors */
	while (i < DTD_MAX) {
		u8  iter = start;
		u32 header_1 = 0;
		u8  header_2 = 0;

		header_1 = edid_blk0[iter++];
		header_1 = header_1 << 8 | edid_blk0[iter++];
		header_1 = header_1 << 8 | edid_blk0[iter++];
		header_1 = header_1 << 8 | edid_blk0[iter++];
		header_2 = edid_blk0[iter];

		if (header_1 != 0x000000F7 || header_2 != 0x00)
			goto loop_end;

		/* VESA DMT Standard Version (0x0A)*/
		iter++;

		/* First set of supported formats */
		iter++;
		if (edid_blk0[iter] & BIT(3)) {
			pr_debug("%s: DMT 848x480@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_848x480p60_16_9);
		}

		/* Second set of supported formats */
		iter++;
		if (edid_blk0[iter] & BIT(1)) {
			pr_debug("%s: DMT 1280x1024@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1280x1024p60_5_4);
		}

		if (edid_blk0[iter] & BIT(3)) {
			pr_debug("%s: DMT 1280x960@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1280x960p60_4_3);
		}

		/* Third set of supported formats */
		iter++;
		if (edid_blk0[iter] & BIT(1)) {
			pr_debug("%s: DMT 1400x1050@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1400x1050p60_4_3);
		}

		if (edid_blk0[iter] & BIT(5)) {
			pr_debug("%s: DMT 1440x900@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1440x900p60_16_10);
		}

		if (edid_blk0[iter] & BIT(7)) {
			pr_debug("%s: DMT 1360x768@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1360x768p60_16_9);
		}

		/* Fourth set of supported formats */
		iter++;
		if (edid_blk0[iter] & BIT(2)) {
			pr_debug("%s: DMT 1600x1200@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1600x1200p60_4_3);
		}

		if (edid_blk0[iter] & BIT(5)) {
			pr_debug("%s: DMT 1680x1050@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1680x1050p60_16_10);
		}

		/* Fifth set of supported formats */
		iter++;
		if (edid_blk0[iter] & BIT(0)) {
			pr_debug("%s: DMT 1920x1200@60\n", __func__);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1920x1200p60_16_10);
		}

loop_end:
		i++;
		start += DTD_SIZE;
	}
}

static void hdmi_edid_get_display_mode(struct hdmi_edid_ctrl *edid_ctrl)
{
	u8 i = 0, offset = 0, std_blk = 0;
	u32 video_format = HDMI_VFRMT_640x480p60_4_3;
	u32 has480p = false;
	u8 len = 0;
	u8 num_of_cea_blocks;
	u8 *data_buf;
	int rc;
	const u8 *edid_blk0 = NULL;
	const u8 *edid_blk1 = NULL;
	const u8 *svd = NULL;
	u32 has60hz_mode = false;
	u32 has50hz_mode = false;
	bool read_block0_res = false;
	struct hdmi_edid_sink_data *sink_data = NULL;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	data_buf = edid_ctrl->edid_buf;
	num_of_cea_blocks = edid_ctrl->cea_blks;

	edid_blk0 = &data_buf[0x0];
	edid_blk1 = &data_buf[0x80];
	svd = num_of_cea_blocks ?
		hdmi_edid_find_block(data_buf+0x80, DBC_START_OFFSET,
			VIDEO_DATA_BLOCK, &len) : NULL;

	if (num_of_cea_blocks && (len == 0 || len > MAX_DATA_BLOCK_SIZE)) {
		DEV_DBG("%s: fall back to block 0 res\n", __func__);
		svd = NULL;
		read_block0_res = true;
	}

	sink_data = &edid_ctrl->sink_data;

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
			hdmi_edid_add_sink_video_format(edid_ctrl,
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
	} else if (!num_of_cea_blocks || read_block0_res) {
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
			hdmi_edid_detail_desc(edid_ctrl,
				edid_blk0+0x36+desc_offset,
				&video_format);

			DEV_DBG("[%s:%d] Block-0 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				msm_hdmi_mode_2string(video_format));

			hdmi_edid_add_sink_video_format(edid_ctrl,
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
			hdmi_edid_detail_desc(edid_ctrl,
				edid_blk0+0x36+desc_offset,
				&video_format);

			DEV_DBG("[%s:%d] Block-0 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				msm_hdmi_mode_2string(video_format));

			hdmi_edid_add_sink_video_format(edid_ctrl,
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
			hdmi_edid_detail_desc(edid_ctrl,
				edid_blk1+desc_offset,
				&video_format);

			DEV_DBG("[%s:%d] Block-1 Adding vid fmt = [%s]\n",
				__func__, __LINE__,
				msm_hdmi_mode_2string(video_format));

			hdmi_edid_add_sink_video_format(edid_ctrl,
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
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1280x1024p60_5_4);
		}
		if ((edid_blk0[0x26 + offset] == 0x61) &&
		    (edid_blk0[0x26 + offset + 1] == 0x40)) {
			pr_debug("%s: 65MHz: off=[%x] stdblk=[%x]\n",
				 __func__, offset, std_blk);
			hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1024x768p60_4_3);
			break;
		} else {
			offset += 2;
		}
		std_blk++;
	}

	/* Established Timing I */
	if (edid_blk0[0x23] & BIT(0)) {
		pr_debug("%s: DMT: ETI: HDMI_VFRMT_800x600_4_3\n", __func__);
		hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_800x600p60_4_3);
	}

	/* Established Timing II */
	if (edid_blk0[0x24] & BIT(3)) {
		pr_debug("%s: DMT: ETII: HDMI_VFRMT_1024x768p60_4_3\n",
			__func__);
		hdmi_edid_add_sink_video_format(edid_ctrl,
				HDMI_VFRMT_1024x768p60_4_3);
	}

	/* Established Timing III */
	hdmi_edid_parse_et3(edid_ctrl, data_buf);

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
		rc = hdmi_edid_get_display_vsd_3d_mode(data_buf, sink_data,
			num_of_cea_blocks);
		if (!rc)
			pr_debug("%s: 3D formats in VSD\n", __func__);
	}

	/*
	 * Need to add default 640 by 480 timings, in case not described
	 * in the EDID structure.
	 * All DTV sink devices should support this mode
	 */
	if (!has480p)
		hdmi_edid_add_sink_video_format(edid_ctrl,
			HDMI_VFRMT_640x480p60_4_3);
} /* hdmi_edid_get_display_mode */

u32 hdmi_edid_get_raw_data(void *input, u8 *buf, u32 size)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *) input;
	u32 ret = 0;
	u32 buf_size;

	if (!edid_ctrl || !buf) {
		DEV_ERR("%s: invalid input\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	buf_size = sizeof(edid_ctrl->edid_buf);

	size = min(size, buf_size);

	memcpy(buf, edid_ctrl->edid_buf, size);

end:
	return ret;
}

static void hdmi_edid_add_resv_timings(struct hdmi_edid_ctrl *edid_ctrl)
{
	int i = HDMI_VFRMT_RESERVE1;

	while (i <= RESERVE_VFRMT_END) {
		if (hdmi_is_valid_resv_timing(i))
			hdmi_edid_add_sink_video_format(edid_ctrl, i);
		else
			break;
		i++;
	}
}

int hdmi_edid_parser(void *input)
{
	u8 *edid_buf = NULL;
	u32 num_of_cea_blocks = 0;
	u16 ieee_reg_id;
	int status = 0;
	u32 i = 0;
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		status = -EINVAL;
		goto err_invalid_data;
	}

	/* reset edid data for new hdmi connection */
	hdmi_edid_reset_parser(edid_ctrl);

	edid_buf = edid_ctrl->edid_buf;

	DEV_DBG("%s: === HDMI EDID BLOCK 0 ===\n", __func__);
	print_hex_dump(KERN_DEBUG, "HDMI EDID: ", DUMP_PREFIX_NONE, 16, 1,
		edid_buf, EDID_BLOCK_SIZE, false);

	if (!hdmi_edid_check_header(edid_buf)) {
		status = -EPROTO;
		goto err_invalid_header;
	}

	hdmi_edid_extract_vendor_id(edid_ctrl);

	/* EDID_CEA_EXTENSION_FLAG[0x7E] - CEC extension byte */
	num_of_cea_blocks = edid_buf[EDID_BLOCK_SIZE - 2];
	DEV_DBG("%s: No. of CEA blocks is  [%u]\n", __func__,
		num_of_cea_blocks);

	/* Find out any CEA extension blocks following block 0 */
	if (num_of_cea_blocks == 0) {
		/* No CEA extension */
		edid_ctrl->sink_mode = SINK_MODE_DVI;
		DEV_DBG("HDMI DVI mode: %s\n",
			edid_ctrl->sink_mode ? "no" : "yes");
		goto bail;
	}

	/* check for valid CEA block */
	if (edid_buf[EDID_BLOCK_SIZE] != 2) {
		DEV_ERR("%s: Invalid CEA block\n", __func__);
		num_of_cea_blocks = 0;
		goto bail;
	}

	/* goto to CEA extension edid block */
	edid_buf += EDID_BLOCK_SIZE;

	ieee_reg_id = hdmi_edid_extract_ieee_reg_id(edid_ctrl, edid_buf);
	if (ieee_reg_id == EDID_IEEE_REG_ID)
		edid_ctrl->sink_mode = SINK_MODE_HDMI;
	else
		edid_ctrl->sink_mode = SINK_MODE_DVI;

	hdmi_edid_extract_sink_caps(edid_ctrl, edid_buf);
	hdmi_edid_extract_latency_fields(edid_ctrl, edid_buf);
	hdmi_edid_extract_dc(edid_ctrl, edid_buf);
	hdmi_edid_extract_speaker_allocation_data(edid_ctrl, edid_buf);
	hdmi_edid_extract_audio_data_blocks(edid_ctrl, edid_buf);
	hdmi_edid_extract_3d_present(edid_ctrl, edid_buf);
	hdmi_edid_extract_extended_data_blocks(edid_ctrl, edid_buf);

bail:
	for (i = 1; i <= num_of_cea_blocks; i++) {
		DEV_DBG("%s: === HDMI EDID BLOCK %d ===\n", __func__, i);
		print_hex_dump(KERN_DEBUG, "HDMI EDID: ", DUMP_PREFIX_NONE,
			16, 1, edid_ctrl->edid_buf + (i * EDID_BLOCK_SIZE),
			EDID_BLOCK_SIZE, false);
	}

	edid_ctrl->cea_blks = num_of_cea_blocks;

	hdmi_edid_get_display_mode(edid_ctrl);

	if (edid_ctrl->keep_resv_timings)
		hdmi_edid_add_resv_timings(edid_ctrl);

	return 0;

err_invalid_header:
	edid_ctrl->sink_data.num_of_elements = 1;
	edid_ctrl->sink_data.disp_mode_list[0].video_format =
		edid_ctrl->video_resolution;
	edid_ctrl->sink_data.disp_mode_list[0].rgb_support = true;
err_invalid_data:
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
	bool sink_mode;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return 0;
	}

	if (edid_ctrl->edid_override &&
		(edid_ctrl->override_data.sink_mode != -1))
		sink_mode = edid_ctrl->override_data.sink_mode;
	else
		sink_mode = edid_ctrl->sink_mode;

	return sink_mode;
} /* hdmi_edid_get_sink_mode */

/**
 * hdmi_edid_get_deep_color() - get deep color info supported by sink
 * @input: edid parser data
 *
 * This API returns deep color for different formats supported by sink.
 * Deep color support for Y444 (BIT(0)), RGB30 (BIT(1)), RGB36 (BIT(2),
 * RGB 48 (BIT(3)) is provided in a 8 bit integer. The MSB 8 bits are
 * not used.
 *
 * Return: deep color data.
 */
u8 hdmi_edid_get_deep_color(void *input)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return 0;
	}

	return edid_ctrl->deep_color;
}

/**
 * hdmi_edid_get_hdr_data() - get the HDR capabiliies of the sink
 * @input: edid parser data
 *
 * This API returns HDR info associated with the sink:
 * electro-optical transfer function, static metadata descriptor,
 * desired content max luminance data, desired content max
 * frame-average luminance data, and desired content min luminance
 * data.
 *
 * Return: HDR data.
 */
void hdmi_edid_get_hdr_data(void *input,
		struct hdmi_edid_hdr_data *hdr_data)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl || !hdr_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	hdr_data = &edid_ctrl->hdr_data;
}

bool hdmi_edid_is_s3d_mode_supported(void *input, u32 video_mode, u32 s3d_mode)
{
	int i;
	bool ret = false;
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;
	struct hdmi_edid_sink_data *sink_data;

	sink_data = &edid_ctrl->sink_data;
	for (i = 0; i < sink_data->num_of_elements; ++i) {
		if (sink_data->disp_mode_list[i].video_format != video_mode)
			continue;
		if (sink_data->disp_mode_list[i].video_3d_format &
			(1 << s3d_mode))
			ret = true;
		else
			DEV_DBG("%s: return false: vic=%d caps=%x s3d=%d\n",
				__func__, video_mode,
				sink_data->disp_mode_list[i].video_3d_format,
				s3d_mode);
		break;
	}
	return ret;
}

bool hdmi_edid_get_scdc_support(void *input)
{
	struct hdmi_edid_ctrl *edid_ctrl = input;
	bool scdc_present;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return false;
	}

	if (edid_ctrl->edid_override &&
		(edid_ctrl->override_data.scramble != -1))
		scdc_present = edid_ctrl->override_data.scramble;
	else
		scdc_present = edid_ctrl->sink_caps.scdc_present;

	return scdc_present;
}

/**
 * hdmi_edid_sink_scramble_override() - check if override has been enabled
 * @input: edid data
 *
 * Return true if scrambling override is enabled false otherwise.
 */
bool hdmi_edid_sink_scramble_override(void *input)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (edid_ctrl->edid_override &&
		(edid_ctrl->override_data.scramble != -1))
		return true;

	return false;

}

bool hdmi_edid_get_sink_scrambler_support(void *input)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;
	bool scramble_support;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return 0;
	}

	if (edid_ctrl->edid_override &&
		(edid_ctrl->override_data.scramble != -1))
		scramble_support = edid_ctrl->override_data.scramble;
	else
		scramble_support = edid_ctrl->sink_caps.scramble_support;

	return scramble_support;
}

int hdmi_edid_get_audio_blk(void *input,
				struct msm_ext_disp_audio_edid_blk *blk)
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

void hdmi_edid_set_video_resolution(void *input, u32 resolution, bool reset)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (!edid_ctrl) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	edid_ctrl->video_resolution = resolution;

	if (reset) {
		edid_ctrl->default_vic = resolution;
		edid_ctrl->sink_data.num_of_elements = 1;
		edid_ctrl->sink_data.disp_mode_list[0].video_format =
			resolution;
		edid_ctrl->sink_data.disp_mode_list[0].rgb_support = true;
	}
} /* hdmi_edid_set_video_resolution */

void hdmi_edid_deinit(void *input)
{
	struct hdmi_edid_ctrl *edid_ctrl = (struct hdmi_edid_ctrl *)input;

	if (edid_ctrl) {
		if (edid_ctrl->init_data.kobj)
			sysfs_remove_group(edid_ctrl->init_data.kobj,
				&hdmi_edid_fs_attrs_group);

		kfree(edid_ctrl);
	}
}

void *hdmi_edid_init(struct hdmi_edid_init_data *idata)
{
	struct hdmi_edid_ctrl *edid_ctrl = NULL;

	if (!idata) {
		DEV_ERR("%s: invalid input\n", __func__);
		goto error;
	}

	edid_ctrl = kzalloc(sizeof(*edid_ctrl), GFP_KERNEL);
	if (!edid_ctrl)
		goto error;

	edid_ctrl->init_data = *idata;

	if (idata->kobj) {
		if (sysfs_create_group(idata->kobj,
			&hdmi_edid_fs_attrs_group))
			DEV_ERR("%s: EDID sysfs create failed\n",
				__func__);
	} else {
		DEV_DBG("%s: kobj not provided\n", __func__);
	}

	/* provide edid buffer to the client */
	idata->buf = edid_ctrl->edid_buf;
	idata->buf_size = sizeof(edid_ctrl->edid_buf);

	return (void *)edid_ctrl;

error:
	kfree(edid_ctrl);
	return NULL;
}

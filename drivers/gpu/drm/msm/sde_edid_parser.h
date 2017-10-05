/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SDE_EDID_PARSER_H_
#define _SDE_EDID_PARSER_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>


#define MAX_NUMBER_ADB 5
#define MAX_AUDIO_DATA_BLOCK_SIZE 30
#define MAX_SPKR_ALLOC_DATA_BLOCK_SIZE 3
#define EDID_VENDOR_ID_SIZE     4

#define SDE_CEA_EXT    0x02
#define SDE_EXTENDED_TAG 0x07

#define SDE_DRM_MODE_FLAG_FMT_MASK (0x3 << 20)

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

#ifdef SDE_EDID_DEBUG_ENABLE
#define SDE_EDID_DEBUG(fmt, args...)   SDE_ERROR(fmt, ##args)
#else
#define SDE_EDID_DEBUG(fmt, args...)   SDE_DEBUG(fmt, ##args)
#endif

/*
 * struct hdmi_edid_hdr_data - HDR Static Metadata
 * @eotf: Electro-Optical Transfer Function
 * @metadata_type_one: Static Metadata Type 1 support
 * @max_luminance: Desired Content Maximum Luminance
 * @avg_luminance: Desired Content Frame-average Luminance
 * @min_luminance: Desired Content Minimum Luminance
 */
struct sde_edid_hdr_data {
	u32 eotf;
	bool metadata_type_one;
	u32 max_luminance;
	u32 avg_luminance;
	u32 min_luminance;
};

struct sde_edid_sink_caps {
	u32 max_pclk_in_hz;
	bool scdc_present;
	bool scramble_support; /* scramble support for less than 340Mcsc */
	bool read_req_support;
	bool osd_disparity;
	bool dual_view_support;
	bool ind_view_support;
};

struct sde_edid_ctrl {
	struct edid *edid;
	u8 pt_scan_info;
	u8 it_scan_info;
	u8 ce_scan_info;
	u8 audio_data_block[MAX_NUMBER_ADB * MAX_AUDIO_DATA_BLOCK_SIZE];
	int adb_size;
	u8 spkr_alloc_data_block[MAX_SPKR_ALLOC_DATA_BLOCK_SIZE];
	int sadb_size;
	bool hdr_supported;
	char vendor_id[EDID_VENDOR_ID_SIZE];
	struct sde_edid_sink_caps sink_caps;
	struct sde_edid_hdr_data hdr_data;
};

/**
 * sde_edid_init() - init edid structure.
 * @edid_ctrl:     Handle to the edid_ctrl structure.
 * Return: handle to sde_edid_ctrl for the client.
 */
struct sde_edid_ctrl *sde_edid_init(void);

/**
 * sde_edid_deinit() - deinit edid structure.
 * @edid_ctrl:     Handle to the edid_ctrl structure.
 *
 * Return: void.
 */
void sde_edid_deinit(void **edid_ctrl);

/**
 * sde_get_edid() - get edid info.
 * @connector:   Handle to the drm_connector.
 * @adapter:     handle to i2c adapter for DDC read
 * @edid_ctrl:   Handle to the edid_ctrl structure.
 *
 * Return: void.
 */
void sde_get_edid(struct drm_connector *connector,
struct i2c_adapter *adapter,
void **edid_ctrl);

/**
 * sde_free_edid() - free edid structure.
 * @edid_ctrl:     Handle to the edid_ctrl structure.
 *
 * Return: void.
 */
void sde_free_edid(void **edid_ctrl);

/**
 * sde_detect_hdmi_monitor() - detect HDMI mode.
 * @edid_ctrl:     Handle to the edid_ctrl structure.
 *
 * Return: error code.
 */
bool sde_detect_hdmi_monitor(void *edid_ctrl);

/**
 * sde_get_edid_checksum() - return the checksum of last block of EDID.
 * @input:     Handle to the edid_ctrl structure.
 *
 * Return: checksum of the last EDID block.
 */
u8 sde_get_edid_checksum(void *input);

/**
 * _sde_edid_update_modes() - populate EDID modes.
 * @edid_ctrl:     Handle to the edid_ctrl structure.
 *
 * Return: error code.
 */
int _sde_edid_update_modes(struct drm_connector *connector,
							void *edid_ctrl);

#endif /* _SDE_EDID_PARSER_H_ */


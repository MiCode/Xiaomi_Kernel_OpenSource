/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef __MHL_DEVCAP_H__
#define __MHL_DEVCAP_H__

#define SILICON_IMAGE_ADOPTER_ID 322
#define TRANSCODER_DEVICE_ID 0x8334

#define MHL_DEV_LD_AUDIO (0x01 << 2)
#define MHL_DEV_LD_VIDEO (0x01 << 1)
#define MHL_DEV_LD_MEDIA (0x01 << 3)
#define MHL_DEV_LD_GUI (0x01 << 7)
#define	MHL_LOGICAL_DEVICE_MAP		(MHL_DEV_LD_AUDIO |\
	MHL_DEV_LD_VIDEO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_GUI)

#define DEVCAP_VAL_DEV_STATE       0
#define DEVCAP_VAL_MHL_VERSION     MHL_VERSION
#define DEVCAP_VAL_DEV_CAT         (MHL_DEV_CAT_SOURCE |\
	MHL_DEV_CATEGORY_POW_BIT)
#define DEVCAP_VAL_ADOPTER_ID_H    (uint8_t)(SILICON_IMAGE_ADOPTER_ID >>   8)
#define DEVCAP_VAL_ADOPTER_ID_L    (uint8_t)(SILICON_IMAGE_ADOPTER_ID & 0xFF)
#define DEVCAP_VAL_VID_LINK_MODE   MHL_DEV_VID_LINK_SUPPRGB444
#define DEVCAP_VAL_AUD_LINK_MODE   MHL_DEV_AUD_LINK_2CH
#define DEVCAP_VAL_VIDEO_TYPE      0
#define DEVCAP_VAL_LOG_DEV_MAP     MHL_LOGICAL_DEVICE_MAP
#define DEVCAP_VAL_BANDWIDTH       0
#define DEVCAP_VAL_FEATURE_FLAG    (MHL_FEATURE_RCP_SUPPORT |\
	MHL_FEATURE_RAP_SUPPORT | MHL_FEATURE_SP_SUPPORT)
#define DEVCAP_VAL_DEVICE_ID_H     (uint8_t)(TRANSCODER_DEVICE_ID >>   8)
#define DEVCAP_VAL_DEVICE_ID_L     (uint8_t)(TRANSCODER_DEVICE_ID & 0xFF)
#define DEVCAP_VAL_SCRATCHPAD_SIZE MHL_SCRATCHPAD_SIZE
#define DEVCAP_VAL_INT_STAT_SIZE   MHL_INT_AND_STATUS_SIZE
#define DEVCAP_VAL_RESERVED        0

#endif /* __MHL_DEVCAP_H__ */

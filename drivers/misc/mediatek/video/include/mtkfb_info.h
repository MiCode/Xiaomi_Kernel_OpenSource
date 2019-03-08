/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __MTKFB_INFO_H__
#define __MTKFB_INFO_H__

#ifdef __cplusplus
extern "C" {
#endif


enum MTKFB_DISPIF_TYPE {
	DISPIF_TYPE_DBI = 0,
	DISPIF_TYPE_DPI,
	DISPIF_TYPE_DSI,
	DISPIF_TYPE_DPI0,
	DISPIF_TYPE_DPI1,
	DISPIF_TYPE_DSI0,
	DISPIF_TYPE_DSI1,
	HDMI = 7,
	HDMI_SMARTBOOK,
	MHL,
	DISPIF_TYPE_EPD,
	SLIMPORT
};

enum MTKFB_DISPIF_DEVICE_TYPE {
	MTKFB_DISPIF_PRIMARY_LCD = 0,
	MTKFB_DISPIF_HDMI,
	MTKFB_DISPIF_EPD,
	MTKFB_MAX_DISPLAY_COUNT
};

enum MTKFB_DISPIF_FORMAT {
	DISPIF_FORMAT_RGB565 = 0,
	DISPIF_FORMAT_RGB666,
	DISPIF_FORMAT_RGB888
};


enum MTKFB_DISPIF_MODE {
	DISPIF_MODE_VIDEO = 0,
	DISPIF_MODE_COMMAND
};

struct mtk_dispif_info {
	unsigned int display_id;
	unsigned int isHwVsyncAvailable;
	enum MTKFB_DISPIF_TYPE displayType;
	unsigned int displayWidth;
	unsigned int displayHeight;
	unsigned int displayFormat;
	enum MTKFB_DISPIF_MODE displayMode;
	unsigned int vsyncFPS;
	unsigned int physicalWidth;
	unsigned int physicalHeight;
	unsigned int isConnected;
/* it's for DFO Multi-Resolution feature, stores the original LCM Wdith */
	unsigned int lcmOriginalWidth;
/* it's for DFO Multi-Resolution feature, stores the original LCM Height */
	unsigned int lcmOriginalHeight;
};

#define MAKE_MTK_FB_FORMAT_ID(id, bpp)  (((id) << 8) | (bpp))

enum MTK_FB_FORMAT {
	MTK_FB_FORMAT_UNKNOWN = 0,

	MTK_FB_FORMAT_RGB565 = MAKE_MTK_FB_FORMAT_ID(1, 2),
	MTK_FB_FORMAT_RGB888 = MAKE_MTK_FB_FORMAT_ID(2, 3),
	MTK_FB_FORMAT_BGR888 = MAKE_MTK_FB_FORMAT_ID(3, 3),
	MTK_FB_FORMAT_ARGB8888 = MAKE_MTK_FB_FORMAT_ID(4, 4),
	MTK_FB_FORMAT_ABGR8888 = MAKE_MTK_FB_FORMAT_ID(5, 4),
	MTK_FB_FORMAT_YUV422 = MAKE_MTK_FB_FORMAT_ID(6, 2),
	MTK_FB_FORMAT_XRGB8888 = MAKE_MTK_FB_FORMAT_ID(7, 4),
	MTK_FB_FORMAT_XBGR8888 = MAKE_MTK_FB_FORMAT_ID(8, 4),
	MTK_FB_FORMAT_UYVY = MAKE_MTK_FB_FORMAT_ID(9, 2),
	MTK_FB_FORMAT_YUV420_P = MAKE_MTK_FB_FORMAT_ID(10, 2),
	MTK_FB_FORMAT_YUY2 = MAKE_MTK_FB_FORMAT_ID(11, 2),
	MTK_FB_FORMAT_RGBA8888 = MAKE_MTK_FB_FORMAT_ID(12, 4),
	MTK_FB_FORMAT_BGRA8888 = MAKE_MTK_FB_FORMAT_ID(13, 4),
	MTK_FB_FORMAT_BPP_MASK = 0xFF,
};

#define GET_MTK_FB_FORMAT_BPP(f)    ((f) & MTK_FB_FORMAT_BPP_MASK)


#ifdef __cplusplus
}
#endif
#endif				/* __DISP_DRV_H__ */

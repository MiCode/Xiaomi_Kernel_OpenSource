/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */


#ifndef _MTK_EXT_DISP_MGR_H
#define     _MTK_EXT_DISP_MGR_H

#include <linux/io.h>

#include "mtkfb.h"
#include "extd_hdmi.h"
#include "extd_info.h"
#include <linux/compat.h>

#define HDMI_DRV "/dev/hdmitx"

#define HDMI_IOW(num, dtype)     _IOW('H', num, dtype)
#define HDMI_IOR(num, dtype)     _IOR('H', num, dtype)
#define HDMI_IOWR(num, dtype)    _IOWR('H', num, dtype)
#define HDMI_IO(num)             _IO('H', num)

#define MTK_HDMI_AUDIO_VIDEO_ENABLE             HDMI_IO(1)
#define MTK_HDMI_AUDIO_ENABLE                   HDMI_IO(2)
#define MTK_HDMI_VIDEO_ENABLE                   HDMI_IO(3)
#define MTK_HDMI_GET_CAPABILITY                 HDMI_IOWR(4, int)
#define MTK_HDMI_GET_DEVICE_STATUS	HDMI_IOWR(5, struct hdmi_device_status)
#define MTK_HDMI_VIDEO_CONFIG                   HDMI_IOWR(6, int)
#ifdef CONFIG_COMPAT
#define MTK_HDMI_AUDIO_CONFIG                   HDMI_IOWR(7, compat_int_t)
#else
#define MTK_HDMI_AUDIO_CONFIG                   HDMI_IOWR(7, int)
#endif
#define MTK_HDMI_FORCE_FULLSCREEN_ON            HDMI_IOWR(8, int)
#define MTK_HDMI_FORCE_FULLSCREEN_OFF           HDMI_IOWR(9, int)
#define MTK_HDMI_IPO_POWEROFF                   HDMI_IOWR(10, int)
#define MTK_HDMI_IPO_POWERON                    HDMI_IOWR(11, int)
#define MTK_HDMI_POWER_ENABLE                   HDMI_IOW(12, int)
#define MTK_HDMI_PORTRAIT_ENABLE                HDMI_IOW(13, int)
#define MTK_HDMI_FORCE_OPEN                     HDMI_IOWR(14, int)
#define MTK_HDMI_FORCE_CLOSE                    HDMI_IOWR(15, int)
#define MTK_HDMI_IS_FORCE_AWAKE                 HDMI_IOWR(16, int)

#define MTK_HDMI_POST_VIDEO_BUFFER	HDMI_IOW(20,  struct fb_overlay_layer)
#define MTK_HDMI_AUDIO_SETTING	HDMI_IOWR(21, struct HDMITX_AUDIO_PARA)


#define MTK_HDMI_FACTORY_MODE_ENABLE            HDMI_IOW(30, int)
#define MTK_HDMI_FACTORY_GET_STATUS             HDMI_IOWR(31, int)
#define MTK_HDMI_FACTORY_DPI_TEST               HDMI_IOWR(32, int)

#define MTK_HDMI_USBOTG_STATUS                  HDMI_IOWR(33, int)
#define MTK_HDMI_GET_DRM_ENABLE                 HDMI_IOWR(34, int)

#define MTK_HDMI_GET_DEV_INFO	HDMI_IOWR(35, struct mtk_dispif_info)
#define MTK_HDMI_PREPARE_BUFFER	HDMI_IOW(36, struct fb_overlay_buffer)
#define MTK_HDMI_SCREEN_CAPTURE                 HDMI_IOW(37, unsigned long)

#define MTK_HDMI_WRITE_DEV	HDMI_IOWR(52, struct hdmi_device_write)
#define MTK_HDMI_READ_DEV                       HDMI_IOWR(53, unsigned int)
#define MTK_HDMI_ENABLE_LOG                     HDMI_IOWR(54, unsigned int)
#define MTK_HDMI_CHECK_EDID                     HDMI_IOWR(55, unsigned int)
#define MTK_HDMI_INFOFRAME_SETTING	HDMI_IOWR(56, struct hdmi_para_setting)
#define MTK_HDMI_COLOR_DEEP		HDMI_IOWR(57, struct hdmi_para_setting)
#define MTK_HDMI_ENABLE_HDCP                    HDMI_IOWR(58, unsigned int)
#define MTK_HDMI_STATUS                         HDMI_IOWR(59, unsigned int)
#define MTK_HDMI_HDCP_KEY	HDMI_IOWR(60, struct hdmi_hdcp_key)
#define MTK_HDMI_GET_EDID	HDMI_IOWR(61, struct _HDMI_EDID_T)
#define MTK_HDMI_SETLA	HDMI_IOWR(62, struct CEC_DRV_ADDR_CFG)
#define MTK_HDMI_GET_CECCMD	HDMI_IOWR(63, struct CEC_FRAME_DESCRIPTION_IO)
#define MTK_HDMI_SET_CECCMD		HDMI_IOWR(64, struct CEC_SEND_MSG)
#define MTK_HDMI_CEC_ENABLE                     HDMI_IOWR(65, unsigned int)
#define MTK_HDMI_GET_CECADDR	HDMI_IOWR(66, struct CEC_ADDRESS_IO)
#define MTK_HDMI_CECRX_MODE                     HDMI_IOWR(67, unsigned int)
#define MTK_HDMI_SENDSLTDATA	HDMI_IOWR(68, struct send_slt_data)
#define MTK_HDMI_GET_SLTDATA	HDMI_IOWR(69, struct CEC_GETSLT_DATA)
#define MTK_HDMI_VIDEO_MUTE                     HDMI_IOWR(70, int)

#define MTK_HDMI_READ                           HDMI_IOWR(81, unsigned int)
#define MTK_HDMI_WRITE                          HDMI_IOWR(82, unsigned int)
#define MTK_HDMI_CMD                            HDMI_IOWR(83, unsigned int)
#define MTK_HDMI_DUMP                           HDMI_IOWR(84, unsigned int)
#define MTK_HDMI_DUMP6397                       HDMI_IOWR(85, unsigned int)
#define MTK_HDMI_DUMP6397_W                     HDMI_IOWR(86, unsigned int)
#define MTK_HDMI_CBUS_STATUS                    HDMI_IOWR(87, unsigned int)
#define MTK_HDMI_CONNECT_STATUS                 HDMI_IOWR(88, unsigned int)
#define MTK_HDMI_DUMP6397_R                     HDMI_IOWR(89, unsigned int)
#define MTK_MHL_GET_DCAP                        HDMI_IOWR(90, unsigned int)
#define MTK_MHL_GET_3DINFO                      HDMI_IOWR(91, unsigned int)
#define MTK_HDMI_HDCP                           HDMI_IOWR(92, unsigned int)

#define MTK_HDMI_FACTORY_CHIP_INIT              HDMI_IOWR(94, int)
#define MTK_HDMI_FACTORY_JUDGE_CALLBACK         HDMI_IOWR(95, int)
#define MTK_HDMI_FACTORY_START_DPI_AND_CONFIG   HDMI_IOWR(96, int)
#define MTK_HDMI_FACTORY_DPI_STOP_AND_POWER_OFF HDMI_IOWR(97, int)

#endif

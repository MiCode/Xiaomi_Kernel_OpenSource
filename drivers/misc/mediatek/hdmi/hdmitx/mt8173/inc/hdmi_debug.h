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

#ifndef _MT_HDMI_DEBUG_H_
#define  _MT_HDMI_DEBUG_H_

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

extern unsigned char _bHdcpOff;

extern HDMI_AV_INFO_T _stAvdAVInfo;
extern struct HDMI_SINK_AV_CAP_T _HdmiSinkAvCap;
extern unsigned char cDstStr[50];
extern const unsigned char _cFsStr[][7];
extern char debug_buffer[4096];

extern void mt_hdmi_show_info(const char *pbuf);
extern unsigned char vIsDviMode(void);
extern void hdmi_force_plug_out(void);
extern void hdmi_force_plug_in(void);
extern unsigned char hdmi_plug_test_mode;


#endif
#endif

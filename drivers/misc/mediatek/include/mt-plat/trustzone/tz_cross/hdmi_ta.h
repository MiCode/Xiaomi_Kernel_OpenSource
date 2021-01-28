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

#ifndef _HDMI_TA_H_
#define _HDMI_TA_H_

#define TZ_TA_HDMI_NAME "HDMI TA"
#define TZ_TA_HDMI_UUID "eaf800b0-da1b-11e2-a28f-0800200c9a66"

enum HDMI_TA_SERVICE_CMD_T {
	HDMI_TA_WRITE_REG = 0,
	HDMI_TA_DPI1_WRITE_REG,
	HDMI_TA_INSTALL_HDCP_KEY,
	HDMI_TA_LOAD_HDCP_KEY,
	HDMI_TA_GET_HDCP_AKSV,
	HDMI_TA_HDCP_ENC_EN,
	HDMI_TA_HDCP_RST,
	HDMI_TA_VID_UNMUTE,
	HDMI_TA_AUD_UNMUTE,
	HDMI_TA_PROTECT_HDMIREG,
	HDMI_TA_LOAD_ROM,
	HDMI_TA_HDCP_FAIL,
	HDMI_TA_HDCP_OFF,
};

enum TA_RETURN_HDMI_HDCP_STATE {
	TA_RETURN_HDCP_STATE_ENC_EN = 0,
	TA_RETURN_HDCP_STATE_ENC_FAIL,
	TA_RETURN_HDCP_STATE_ENC_UNKNOWN
};

extern unsigned char vTaGetHdcpStatus(void);
extern unsigned char vTaGetHdmiStatus(void);

#endif

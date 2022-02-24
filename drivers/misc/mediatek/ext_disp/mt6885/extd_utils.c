/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/delay.h>


#include "extd_utils.h"
#include "extd_log.h"

int extd_mutex_init(struct mutex *m)
{
	mutex_init(m);
	return 0;
}

int extd_sw_mutex_lock(struct mutex *m)
{
	mutex_lock(m);
	return 0;
}

int extd_mutex_trylock(struct mutex *m)
{
	int ret = 0;

	ret = mutex_trylock(m);
	return ret;
}

int extd_sw_mutex_unlock(struct mutex *m)
{
	mutex_unlock(m);
	return 0;
}

int extd_msleep(unsigned int ms)
{
	EXTDINFO("sleep %dms\n", ms);
	msleep(ms);
	return 0;
}

long int extd_get_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

char *_extd_ioctl_spy(unsigned int cmd)
{
	switch (cmd) {
	case MTK_HDMI_AUDIO_VIDEO_ENABLE:
		return "MTK_HDMI_AUDIO_VIDEO_ENABLE";

	case MTK_HDMI_AUDIO_ENABLE:
		return "MTK_HDMI_AUDIO_ENABLE";

	case MTK_HDMI_VIDEO_ENABLE:
		return "MTK_HDMI_VIDEO_ENABLE";

	case MTK_HDMI_GET_CAPABILITY:
		return "MTK_HDMI_GET_CAPABILITY";

	case MTK_HDMI_GET_DEVICE_STATUS:
		return "MTK_HDMI_GET_DEVICE_STATUS";

	case MTK_HDMI_VIDEO_CONFIG:
		return "MTK_HDMI_VIDEO_CONFIG";

	case MTK_HDMI_AUDIO_CONFIG:
		return "MTK_HDMI_AUDIO_CONFIG";

	case MTK_HDMI_FORCE_FULLSCREEN_ON:
		return "MTK_HDMI_FORCE_FULLSCREEN_ON";

	case MTK_HDMI_FORCE_FULLSCREEN_OFF:
		return "MTK_HDMI_FORCE_FULLSCREEN_OFF";

	case MTK_HDMI_IPO_POWEROFF:
		return "MTK_HDMI_IPO_POWEROFF";

	case MTK_HDMI_IPO_POWERON:
		return "MTK_HDMI_IPO_POWERON";

	case MTK_HDMI_POWER_ENABLE:
		return "MTK_HDMI_POWER_ENABLE";

	case MTK_HDMI_PORTRAIT_ENABLE:
		return "MTK_HDMI_PORTRAIT_ENABLE";

	case MTK_HDMI_FORCE_OPEN:
		return "MTK_HDMI_FORCE_OPEN";

	case MTK_HDMI_FORCE_CLOSE:
		return "MTK_HDMI_FORCE_CLOSE";

	case MTK_HDMI_IS_FORCE_AWAKE:
		return "MTK_HDMI_IS_FORCE_AWAKE";

	case MTK_HDMI_POST_VIDEO_BUFFER:
		return "MTK_HDMI_POST_VIDEO_BUFFER";

	case MTK_HDMI_FACTORY_MODE_ENABLE:
		return "MTK_HDMI_FACTORY_MODE_ENABLE";

	case MTK_HDMI_USBOTG_STATUS:
		return "MTK_HDMI_USBOTG_STATUS";

	case MTK_HDMI_WRITE_DEV:
		return "MTK_HDMI_WRITE_DEV";

	case MTK_HDMI_READ_DEV:
		return "MTK_HDMI_READ_DEV";

	case MTK_HDMI_ENABLE_LOG:
		return "MTK_HDMI_ENABLE_LOG";

	case MTK_HDMI_CHECK_EDID:
		return "MTK_HDMI_CHECK_EDID";

	case MTK_HDMI_INFOFRAME_SETTING:
		return "MTK_HDMI_INFOFRAME_SETTING";

	case MTK_HDMI_ENABLE_HDCP:
		return "MTK_HDMI_ENABLE_HDCP";

	case MTK_HDMI_STATUS:
		return "MTK_HDMI_STATUS";

	case MTK_HDMI_HDCP_KEY:
		return "MTK_HDMI_HDCP_KEY";

	case MTK_HDMI_GET_EDID:
		return "MTK_HDMI_GET_EDID";

	case MTK_HDMI_SETLA:
		return "MTK_HDMI_SETLA";

	case MTK_HDMI_GET_CECCMD:
		return "MTK_HDMI_GET_CECCMD";

	case MTK_HDMI_SET_CECCMD:
		return "MTK_HDMI_SET_CECCMD";

	case MTK_HDMI_CEC_ENABLE:
		return "MTK_HDMI_CEC_ENABLE";

	case MTK_HDMI_GET_CECADDR:
		return "MTK_HDMI_GET_CECADDR";

	case MTK_HDMI_CECRX_MODE:
		return "MTK_HDMI_CECRX_MODE";

	case MTK_HDMI_SENDSLTDATA:
		return "MTK_HDMI_SENDSLTDATA";

	case MTK_HDMI_GET_SLTDATA:
		return "MTK_HDMI_GET_SLTDATA";

	case MTK_HDMI_COLOR_DEEP:
		return "MTK_HDMI_COLOR_DEEP";

	case MTK_HDMI_GET_DEV_INFO:
		return "MTK_HDMI_GET_DEV_INFO";

	case MTK_HDMI_PREPARE_BUFFER:
		return "MTK_HDMI_PREPARE_BUFFER";

	case MTK_HDMI_FACTORY_GET_STATUS:
		return "MTK_HDMI_FACTORY_GET_STATUS";

	case MTK_HDMI_FACTORY_DPI_TEST:
		return "MTK_HDMI_FACTORY_DPI_TEST";

	case MTK_HDMI_SCREEN_CAPTURE:
		return "MTK_HDMI_SCREEN_CAPTURE";

	case MTK_HDMI_FACTORY_CHIP_INIT:
		return "MTK_HDMI_FACTORY_CHIP_INIT";

	case MTK_HDMI_FACTORY_JUDGE_CALLBACK:
		return "MTK_HDMI_FACTORY_JUDGE_CALLBACK";

	case MTK_HDMI_FACTORY_START_DPI_AND_CONFIG:
			return "MTK_HDMI_FACTORY_START_DPI_AND_CONFIG";

	case MTK_HDMI_FACTORY_DPI_STOP_AND_POWER_OFF:
			return "MTK_HDMI_FACTORY_DPI_STOP_AND_POWER_OFF";

	case MTK_HDMI_AUDIO_SETTING:
			return "MTK_HDMI_AUDIO_SETTING";

	default:
		return "unknown ioctl command";
	}
}

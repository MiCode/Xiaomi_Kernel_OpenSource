/*
 * Copyright (c) 2020 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/regmap.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/extcon.h>
#include <../../../extcon/extcon.h>
#include <linux/kthread.h>
#include <linux/errno.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_dp_helper.h>
#include <drm/mediatek_drm.h>

#include "mtk_dp.h"
#include "mtk_dp_hal.h"
#include "mtk_drm_drv.h"
#include "mtk_dp_api.h"
#include "mtk_dp_reg.h"
#ifdef DPTX_HDCP_ENABLE
#include "mtk_dp_hdcp1x.h"
#include "mtk_dp_hdcp2.h"
#include "ca/tlcDpHdcp.h"
#endif

static struct mtk_dp *g_mtk_dp;
static bool fakecablein;
static int fakeres = FAKE_DEFAULT_RES;
static int fakebpc = DP_COLOR_DEPTH_8BIT;
struct mutex dp_lock;
static bool g_hdcp_on = 1;

static const struct drm_display_mode dptx_est_modes[] = {
	/* 2160x3840@60Hz */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 3860, 3860,
		3880, 0, 2160, 2180, 2180, 2200, 0,
		DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC), .vrefresh = 60, },
	/* 2160x3840@30Hz */
	{ DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 3860, 3860,
		3880, 0, 2160, 2180, 2180, 2200, 0,
		DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC), .vrefresh = 30, },
	/* 1080x1920@60Hz */
	{ DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
		148500, 1920, 1940, 1940, 1980, 0, 1080, 1120, 1120, 1220, 0,
		DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC), .vrefresh = 60,},
	/* 1280x720@60Hz */
	{ DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1300, 1300,
		1320, 0, 720, 740, 740, 760, 0,
		DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC), .vrefresh = 60, }
};

enum DPTX_STATE {
	DPTX_STATE_NO_DEVICE,
	DPTX_STATE_ACTIVE,
};

struct notify_dev {
	const char *name;
	struct device *dev;
	int index;
	int state;

	ssize_t (*print_name)(struct notify_dev *sdev, char *buf);
	ssize_t (*print_state)(struct notify_dev *sdev, char *buf);
};

struct extcon_dev *dptx_extcon;
static const unsigned int dptx_cable[] = {
	EXTCON_DISP_HDMI,// audio framework not support DP
	EXTCON_NONE,
};

struct notify_dev dptx_notify_data;
struct class *switch_class;
static atomic_t device_count;

static ssize_t state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_state) {
		ret = sdev->print_state(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%d\n", sdev->state);
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct notify_dev *sdev = (struct notify_dev *)
		dev_get_drvdata(dev);

	if (sdev->print_name) {
		ret = sdev->print_name(sdev, buf);
		if (ret >= 0)
			return ret;
	}
	return sprintf(buf, "%s\n", sdev->name);
}

static DEVICE_ATTR(state, 0444, state_show, NULL);
static DEVICE_ATTR(name, 0444, name_show, NULL);

static int create_switch_class(void)
{
	if (!switch_class) {
		switch_class = class_create(THIS_MODULE, "switch");
		if (IS_ERR(switch_class))
			return PTR_ERR(switch_class);
		atomic_set(&device_count, 0);
	}
	return 0;
}


int dptx_uevent_dev_register(struct notify_dev *sdev)
{
	int ret;

	if (!switch_class) {
		ret = create_switch_class();

		if (ret == 0)
			DPTXMSG("create_switch_class susesess\n");
		else {
			DPTXERR("create_switch_class fail\n");
			return ret;
		}
	}

	sdev->index = atomic_inc_return(&device_count);
	sdev->dev = device_create(switch_class, NULL,
			MKDEV(0, sdev->index), NULL, sdev->name);

	if (sdev->dev) {
		DPTXMSG("device create ok,index:0x%x\n", sdev->index);
		ret = 0;
	} else {
		DPTXERR("device create fail,index:0x%x\n", sdev->index);
		ret = -1;
	}

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0) {
		device_destroy(switch_class, MKDEV(0, sdev->index));
		DPTXERR("switch: Failed to register driver %s\n",
				sdev->name);
	}

	ret = device_create_file(sdev->dev, &dev_attr_name);
	if (ret < 0) {
		device_remove_file(sdev->dev, &dev_attr_state);
		DPTXERR("switch: Failed to register driver %s\n",
				sdev->name);
	}

	dev_set_drvdata(sdev->dev, sdev);
	sdev->state = 0;

	return ret;
}

int notify_uevent_user(struct notify_dev *sdev, int state)
{
	char *envp[3];
	char name_buf[120];
	char state_buf[120];

	if (sdev == NULL)
		return -1;

	if (sdev->state != state)
		sdev->state = state;

	snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", sdev->name);
	envp[0] = name_buf;
	snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%d", sdev->state);
	envp[1] = state_buf;
	envp[2] = NULL;
	DPTXMSG("uevent name:%s ,state:%s\n", envp[0], envp[1]);

	kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);

	return 0;
}

bool mdrv_DPTx_AuxWrite_Bytes(struct mtk_dp *mtk_dp, u8 ubCmd,
	u32  usDPCDADDR, size_t ubLength, BYTE *pData)
{
	bool bReplyStatus = false;
	u8 ubRetryLimit = 0x7;

	if (!mtk_dp->training_info.bCablePlugIn ||
		((mtk_dp->training_info.usPHY_STS & (HPD_DISCONNECT)) != 0x0)) {

		if (!mtk_dp->training_info.bDPTxAutoTest_EN)
			mtk_dp->training_state = DPTX_NTSTATE_CHECKCAP;
		return false;
	}

	do {
		bReplyStatus = mhal_DPTx_AuxWrite_Bytes(mtk_dp, ubCmd,
			usDPCDADDR, ubLength, pData);
		ubRetryLimit--;
		if (!bReplyStatus) {
			udelay(50);
			DPTXFUNC("Retry Num = %d\n", ubRetryLimit);
		} else
			return true;
	} while (ubRetryLimit > 0);

	DPTXERR("Aux Write Fail: cmd = %d, addr = 0x%x, len = %d\n",
		ubCmd, usDPCDADDR, ubLength);

	return false;
}

bool mdrv_DPTx_AuxWrite_DPCD(struct mtk_dp *mtk_dp, u8 ubCmd,
	u32 usDPCDADDR, size_t ubLength, BYTE *pData)
{
	bool bRet = true;
	size_t times = 0;
	size_t remain = 0;
	size_t loop = 0;

	if (ubLength > DP_AUX_MAX_PAYLOAD_BYTES) {
		times = ubLength / DP_AUX_MAX_PAYLOAD_BYTES;
		remain = ubLength % DP_AUX_MAX_PAYLOAD_BYTES;

		for (loop = 0; loop < times; loop++)
			bRet &= mdrv_DPTx_AuxWrite_Bytes(mtk_dp,
				ubCmd,
				usDPCDADDR + (loop * DP_AUX_MAX_PAYLOAD_BYTES),
				DP_AUX_MAX_PAYLOAD_BYTES,
				pData + (loop * DP_AUX_MAX_PAYLOAD_BYTES));

		if (remain > 0)
			bRet &= mdrv_DPTx_AuxWrite_Bytes(mtk_dp,
				ubCmd,
				usDPCDADDR + (times * DP_AUX_MAX_PAYLOAD_BYTES),
				remain,
				pData + (times * DP_AUX_MAX_PAYLOAD_BYTES));
	} else
		bRet &= mdrv_DPTx_AuxWrite_Bytes(mtk_dp,
				ubCmd,
				usDPCDADDR,
				ubLength,
				pData);

	if (mtk_dp_debug_get()) {
		DPTXDBG("Aux write cmd = %d, addr = 0x%x, len = %d, %s\n",
			ubCmd, usDPCDADDR, ubLength, bRet ? "Success" : "Fail");
		for (loop = 0; loop < ubLength; loop++)
			DPTXDBG("DPCD%x:0x%x", usDPCDADDR + loop, pData[loop]);
	}

	return bRet;
}


bool mdrv_DPTx_AuxRead_Bytes(struct mtk_dp *mtk_dp, u8 ubCmd,
	u32 usDPCDADDR, size_t ubLength, BYTE *pData)
{
	bool bReplyStatus = false;
	u8 ubRetryLimit = 7;

	if (!mtk_dp->training_info.bCablePlugIn ||
		((mtk_dp->training_info.usPHY_STS & (HPD_DISCONNECT)) != 0x0)) {
		if (!mtk_dp->training_info.bDPTxAutoTest_EN)
			mtk_dp->training_state = DPTX_NTSTATE_CHECKCAP;
		return false;
	}


	do {
		bReplyStatus = mhal_DPTx_AuxRead_Bytes(mtk_dp, ubCmd,
					usDPCDADDR, ubLength, pData);
		if (!bReplyStatus) {
			udelay(50);
			DPTXFUNC("Retry Num = %d\n", ubRetryLimit);
		} else
			return true;

		ubRetryLimit--;
	} while (ubRetryLimit > 0);

	DPTXERR("Aux Read Fail: cmd = %d, addr = 0x%x, len = %d\n",
		ubCmd, usDPCDADDR, ubLength);

	return false;
}

bool mdrv_DPTx_AuxRead_DPCD(struct mtk_dp *mtk_dp, u8 ubCmd,
	u32 usDPCDADDR, size_t ubLength, BYTE *pRxBuf)
{
	bool bRet = true;
	size_t times = 0;
	size_t remain = 0;
	size_t loop = 0;

	memset(pRxBuf, 0, ubLength);

	if (ubLength > DP_AUX_MAX_PAYLOAD_BYTES) {
		times = ubLength / DP_AUX_MAX_PAYLOAD_BYTES;
		remain = ubLength % DP_AUX_MAX_PAYLOAD_BYTES;

		for (loop = 0; loop < times; loop++)
			bRet &= mdrv_DPTx_AuxRead_Bytes(mtk_dp,
				ubCmd,
				usDPCDADDR + (loop * DP_AUX_MAX_PAYLOAD_BYTES),
				DP_AUX_MAX_PAYLOAD_BYTES,
				pRxBuf + (loop * DP_AUX_MAX_PAYLOAD_BYTES));

		if (remain > 0)
			bRet &= mdrv_DPTx_AuxRead_Bytes(mtk_dp,
				ubCmd,
				usDPCDADDR + (times * DP_AUX_MAX_PAYLOAD_BYTES),
				remain,
				pRxBuf + (times * DP_AUX_MAX_PAYLOAD_BYTES));
	} else
		bRet &= mdrv_DPTx_AuxRead_Bytes(mtk_dp,
				ubCmd,
				usDPCDADDR,
				ubLength,
				pRxBuf);

	if (mtk_dp_debug_get()) {
		DPTXDBG("Aux Read cmd = %d, addr = 0x%x, len = %d, %s\n",
			ubCmd, usDPCDADDR, ubLength, bRet ? "Success" : "Fail");
		for (loop = 0; loop < ubLength; loop++)
			DPTXDBG("DPCD%x:0x%x", usDPCDADDR + loop, pRxBuf[loop]);
	}

	return bRet;
}

void mdrv_DPTx_deinit(struct mtk_dp *mtk_dp)
{
	mdrv_DPTx_VideoMute(mtk_dp, true);
	mdrv_DPTx_AudioMute(mtk_dp, true);
	mhal_DPTx_VideoMuteSW(mtk_dp, true);
	cancel_work(&mtk_dp->hdcp_work);

	mtk_dp->training_info.ucCheckCapTimes = 0;
	mtk_dp->video_enable = false;
	mtk_dp->dp_ready = false;
	mhal_DPTx_PHY_SetIdlePattern(mtk_dp, true);
	if (mtk_dp->has_fec) {
		mhal_DPTx_EnableFEC(mtk_dp, false);
		mtk_dp->has_fec = false;
	}

	if (mtk_dp->edid != NULL) {
		kfree(mtk_dp->edid);
		mtk_dp->edid = NULL;
	}
}

void mdrv_DPTx_InitVariable(struct mtk_dp *mtk_dp)
{
	DPTXFUNC();
	mtk_dp->training_info.ubDPSysVersion = DP_VERSION_14;
	mtk_dp->training_info.ubLinkRate = DP_LINKRATE_HBR3;
	mtk_dp->training_info.ubLinkLaneCount = DP_LANECOUNT_2;
	mtk_dp->training_info.bSinkEXTCAP_En = false;
	mtk_dp->training_info.bSinkSSC_En = false;
	mtk_dp->training_info.bTPS3 = true;
	mtk_dp->training_info.bTPS4 = true;
	mtk_dp->training_info.usPHY_STS = HPD_INITIAL_STATE;
	mtk_dp->training_info.bCablePlugIn = false;
	mtk_dp->training_info.bCableStateChange = false;
	mtk_dp->training_state = DPTX_NTSTATE_STARTUP;
	mtk_dp->training_state_pre = DPTX_NTSTATE_STARTUP;
	mtk_dp->state = DPTXSTATE_INITIAL;
	mtk_dp->state_pre = DPTXSTATE_INITIAL;

	mtk_dp->info.input_src = DPTX_SRC_DPINTF;
	mtk_dp->info.format = DP_COLOR_FORMAT_RGB_444;
	mtk_dp->info.depth = DP_COLOR_DEPTH_8BIT;
	if (!mtk_dp->info.bPatternGen)
		mtk_dp->info.resolution = SINK_1920_1080;
	mtk_dp->info.bSetAudioMute = false;
	mtk_dp->info.bSetVideoMute = false;
	memset(&mtk_dp->info.DPTX_OUTBL, 0,
		sizeof(struct DPTX_TIMING_PARAMETER));
	mtk_dp->info.DPTX_OUTBL.FrameRate = 60;
	mtk_dp->bPowerOn = false;
	mtk_dp->video_enable = false;
	mtk_dp->dp_ready = false;
	mtk_dp->has_dsc = false;
	mtk_dp->has_fec = false;
	mtk_dp->dsc_enable = false;

	if (!mtk_dp->training_info.set_max_linkrate)
		mdrv_DPTx_CheckMaxLinkRate(mtk_dp);
}

void mdrv_DPTx_SetSDP_DownCntinit(struct mtk_dp *mtk_dp,
	u16 Sram_Read_Start)
{
	u16 SDP_Down_Cnt_Init = 0x0000;
	u8 ucDCOffset;

	if (mtk_dp->info.DPTX_OUTBL.PixRateKhz > 0)
		SDP_Down_Cnt_Init = (Sram_Read_Start *
			mtk_dp->training_info.ubLinkRate * 2700 * 8)
			/ (mtk_dp->info.DPTX_OUTBL.PixRateKhz * 4);

	switch (mtk_dp->training_info.ubLinkLaneCount) {
	case DP_LANECOUNT_1:
		SDP_Down_Cnt_Init = (SDP_Down_Cnt_Init > 0x1A) ?
			SDP_Down_Cnt_Init : 0x1A;  //26
		break;
	case DP_LANECOUNT_2:
		// case for LowResolution && High Audio Sample Rate
		ucDCOffset = (mtk_dp->info.DPTX_OUTBL.Vtt <= 525) ?
			0x04 : 0x00;
		SDP_Down_Cnt_Init = (SDP_Down_Cnt_Init > 0x10) ?
			SDP_Down_Cnt_Init : 0x10 + ucDCOffset; //20 or 16
		break;
	case DP_LANECOUNT_4:
		SDP_Down_Cnt_Init = (SDP_Down_Cnt_Init > 0x06) ?
			SDP_Down_Cnt_Init : 0x06; //6
		break;
	default:
		SDP_Down_Cnt_Init = (SDP_Down_Cnt_Init > 0x06) ?
			SDP_Down_Cnt_Init : 0x06;
		break;
	}

	DPTXMSG("PixRateKhz = %d SDP_DC_Init = %x\n",
		mtk_dp->info.DPTX_OUTBL.PixRateKhz, SDP_Down_Cnt_Init);

	mhal_DPTx_SetSDP_DownCntinit(mtk_dp, SDP_Down_Cnt_Init);
}

void mdrv_DPTx_SetSDP_DownCntinitInHblanking(struct mtk_dp *mtk_dp)
{
	int PixClkMhz;
	u8 ucDCOffset;

	PixClkMhz = (mtk_dp->info.format == DP_COLOR_FORMAT_YUV_420) ?
		mtk_dp->info.DPTX_OUTBL.PixRateKhz/2000 :
		mtk_dp->info.DPTX_OUTBL.PixRateKhz/1000;

	switch (mtk_dp->training_info.ubLinkLaneCount) {
	case DP_LANECOUNT_1:
		mhal_DPTx_SetSDP_DownCntinitInHblanking(mtk_dp, 0x0020);
		break;
	case DP_LANECOUNT_2:
		ucDCOffset = (mtk_dp->info.DPTX_OUTBL.Vtt <= 525) ? 0x14 : 0x00;
		mhal_DPTx_SetSDP_DownCntinitInHblanking(mtk_dp,
			0x0018 + ucDCOffset);
		break;
	case DP_LANECOUNT_4:
		ucDCOffset = (mtk_dp->info.DPTX_OUTBL.Vtt <= 525) ? 0x08 : 0x00;
		if (PixClkMhz > (mtk_dp->training_info.ubLinkRate * 27)) {
			mhal_DPTx_SetSDP_DownCntinitInHblanking(mtk_dp, 0x0008);
			DPTXMSG("Pixclk > LinkRateChange\n");
		} else {
			mhal_DPTx_SetSDP_DownCntinitInHblanking(mtk_dp,
				0x0010 + ucDCOffset);
		}
		break;
	}

}

void mdrv_DPTx_SetTU(struct mtk_dp *mtk_dp)
{
	int TU_size = 0;
	int NValue = 0;
	int FValue = 0;
	int PixRateMhz = 0;
	u8 ColorBpp;
	u16 Sram_Read_Start = DPTX_TBC_BUF_ReadStartAdrThrd;

	ColorBpp = mhal_DPTx_GetColorBpp(mtk_dp);
	PixRateMhz = mtk_dp->info.DPTX_OUTBL.PixRateKhz/1000;
	TU_size = (640*(PixRateMhz)*ColorBpp)/
			(mtk_dp->training_info.ubLinkRate * 27 *
				mtk_dp->training_info.ubLinkLaneCount * 8);

	NValue = TU_size / 10;
	FValue = TU_size-NValue * 10;

	DPTXDBG("TU_size %d,\n", TU_size);
	if (mtk_dp->training_info.ubLinkLaneCount > 0) {
		Sram_Read_Start = mtk_dp->info.DPTX_OUTBL.Hde /
			(mtk_dp->training_info.ubLinkLaneCount*4*2*2);
		Sram_Read_Start =
			(Sram_Read_Start < DPTX_TBC_BUF_ReadStartAdrThrd) ?
			Sram_Read_Start : DPTX_TBC_BUF_ReadStartAdrThrd;
		mhal_DPTx_SetTU_SramRdStart(mtk_dp, Sram_Read_Start);
	}

	mhal_DPTx_SetTU_SetEncoder(mtk_dp);
	mdrv_DPTx_SetSDP_DownCntinitInHblanking(mtk_dp);
	mdrv_DPTx_SetSDP_DownCntinit(mtk_dp, Sram_Read_Start);
}

void mdrv_DPTx_CalculateMN(struct mtk_dp *mtk_dp)
{
	int ubTargetFrameRate = 60;
	int ulTargetPixelclk = 148500000; // default set FHD

	if (mtk_dp->info.DPTX_OUTBL.FrameRate > 0) {
		ubTargetFrameRate = mtk_dp->info.DPTX_OUTBL.FrameRate;
		ulTargetPixelclk = (int)mtk_dp->info.DPTX_OUTBL.Htt*
			(int)mtk_dp->info.DPTX_OUTBL.Vtt*ubTargetFrameRate;
	} else if (mtk_dp->info.DPTX_OUTBL.PixRateKhz > 0) {
		ulTargetPixelclk = mtk_dp->info.DPTX_OUTBL.PixRateKhz * 1000;
	} else {
		ulTargetPixelclk = (int)mtk_dp->info.DPTX_OUTBL.Htt *
			(int)mtk_dp->info.DPTX_OUTBL.Vtt * ubTargetFrameRate;
	}

	if (ulTargetPixelclk > 0)
		mtk_dp->info.DPTX_OUTBL.PixRateKhz = ulTargetPixelclk / 1000;
}

void mdrv_DPTx_Set_MISC(struct mtk_dp *mtk_dp)
{
	u8 format, depth;
	union MISC_T DPTX_MISC;

	format = mtk_dp->info.format;
	depth = mtk_dp->info.depth;

	// MISC 0/1 refernce to spec 1.4a p143 Table 2-96
	// MISC0[7:5] color depth
	switch (depth) {
	case DP_COLOR_DEPTH_6BIT:
	case DP_COLOR_DEPTH_8BIT:
	case DP_COLOR_DEPTH_10BIT:
	case DP_COLOR_DEPTH_12BIT:
	case DP_COLOR_DEPTH_16BIT:
	default:
		DPTX_MISC.dp_misc.color_depth = depth;
		break;
	}

	// MISC0[3]: 0->RGB, 1->YUV
	// MISC0[2:1]: 01b->4:2:2, 10b->4:4:4
	switch (format) {
	case DP_COLOR_FORMAT_YUV_444:
		DPTX_MISC.dp_misc.color_format = 0x1;  //01'b
		break;

	case DP_COLOR_FORMAT_YUV_422:
		DPTX_MISC.dp_misc.color_format = 0x2;  //10'b
		break;

	case DP_COLOR_FORMAT_YUV_420:
		//not support
		break;

	case DP_COLOR_FORMAT_RAW:
		DPTX_MISC.dp_misc.color_format = 0x1;
		DPTX_MISC.dp_misc.spec_def2 = 0x1;
		break;
	case DP_COLOR_FORMAT_YONLY:
		DPTX_MISC.dp_misc.color_format = 0x0;
		DPTX_MISC.dp_misc.spec_def2 = 0x1;
		break;

	case DP_COLOR_FORMAT_RGB_444:
	default:
		break;
	}

	mhal_DPTx_SetMISC(mtk_dp, DPTX_MISC.ucMISC);
}

void mdrv_DPTx_SetDPTXOut(struct mtk_dp *mtk_dp)
{
	mhal_DPTx_EnableBypassMSA(mtk_dp, false);
	mdrv_DPTx_CalculateMN(mtk_dp);

	switch (mtk_dp->info.input_src) {
	case DPTX_SRC_PG:
		mhal_DPTx_VideoClock(true, mtk_dp->info.resolution);
		mhal_DPTx_PGEnable(mtk_dp, true);
		mhal_DPTx_Set_MVIDx2(mtk_dp, false);
		DPTXMSG("Set Pattern Gen output\n");
		break;

	case DPTX_SRC_DPINTF:
		mhal_DPTx_PGEnable(mtk_dp, false);
		DPTXMSG("Set dpintf output\n");
		break;

	default:
		mhal_DPTx_PGEnable(mtk_dp, true);
		break;
	}

	mdrv_DPTx_SetTU(mtk_dp);
}

bool mdrv_DPTx_CheckSinkLock(struct mtk_dp *mtk_dp, u8 *pDPCD20x, u8 *pDPCD200C)
{
	bool bLocked = true;

	if (mtk_dp->training_info.bSinkEXTCAP_En) {
		switch (mtk_dp->training_info.ubLinkLaneCount) {
		case DP_LANECOUNT_1:
			if ((pDPCD200C[0] & 0x07) != 0x07) {
				bLocked = false;
				DPTXMSG("2L Lose LCOK\n");
			}
			break;
		case DP_LANECOUNT_2:
			if ((pDPCD200C[0] & 0x77) != 0x77) {
				bLocked = false;
				DPTXMSG("2L Lose LCOK\n");
			}
			break;
		case DP_LANECOUNT_4:
			if ((pDPCD200C[0] != 0x77) || (pDPCD200C[1] != 0x77)) {
				bLocked = false;
				DPTXMSG("4L Lose LCOK\n");
			}
			break;
		}

		if ((pDPCD200C[2]&BIT0) == 0) {
			bLocked = false;
			DPTXMSG("Interskew Lose LCOK\n");
		}
	} else {
		switch (mtk_dp->training_info.ubLinkLaneCount) {
		case DP_LANECOUNT_1:
			if ((pDPCD20x[2] & 0x07) != 0x07) {
				bLocked = false;
				DPTXMSG("1L Lose LCOK\n");
			}
			break;
		case DP_LANECOUNT_2:
			if ((pDPCD20x[2] & 0x77) != 0x77) {
				bLocked = false;
				DPTXMSG("2L Lose LCOK\n");
			}
			break;
		case DP_LANECOUNT_4:
			if (((pDPCD20x[2] != 0x77) || (pDPCD20x[3] != 0x77))) {
				bLocked = false;
				DPTXMSG("4L Lose LCOK\n");
			}
			break;
		}

		if ((pDPCD20x[4]&BIT0) == 0) {
			bLocked = false;
			DPTXMSG("Interskew Lose LCOK\n");
		}
	}


	if (!bLocked) {
		if (!mtk_dp->dp_ready)
			mtk_dp->training_state = DPTX_NTSTATE_CHECKCAP;
		else if (mtk_dp->training_state > DPTX_NTSTATE_TRAINING_PRE)
			mtk_dp->training_state = DPTX_NTSTATE_TRAINING_PRE;
	}

	return bLocked;
}

void mdrv_DPTx_CheckSinkESI(struct mtk_dp *mtk_dp, u8 *pDPCD20x, u8 *pDPCD2002)
{
	u8 ubTempValue;

	if ((pDPCD20x[0x1]&BIT1) || (pDPCD2002[0x1]&BIT1)) {
#if (DPTX_AutoTest_ENABLE == 0x1)
		if (!mdrv_DPTx_PHY_AutoTest(mtk_dp,
			pDPCD20x[0x1] | pDPCD2002[0x1])) {
			if (mtk_dp->training_state > DPTX_NTSTATE_TRAINING_PRE)
				mtk_dp->training_state
					= DPTX_NTSTATE_TRAINING_PRE;
		}
#endif
	}

	if (pDPCD20x[0x1]&BIT0) { // not support, clrear it.
		ubTempValue = BIT0;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, &ubTempValue, 0x1);
	}

#ifdef DPTX_HDCP_ENABLE
	if (!(pDPCD20x[0x1]&BIT2) && !(pDPCD2002[0x1]&BIT2))
		return;

	if (mtk_dp->info.hdcp2_info.bEnable)
		mdrv_DPTx_HDCP2_irq(mtk_dp);
	else if (mtk_dp->info.hdcp1x_info.bEnable)
		mdrv_DPTx_HDCP1X_irq(mtk_dp);
#endif
}


#if (DPTX_AutoTest_ENABLE == 1)
bool mdrv_DPTx_CheckSSC(struct mtk_dp *mtk_dp)
{
#if (ENABLE_DPTX_SSC_OUTPUT == 0x1)
	BYTE ubTempBuffer[0x2];

	drm_dp_dpcd_read(&mtk_dp->aux,
		DPCD_00003 + DPCD_02200*mtk_dp->training_info.bSinkEXTCAP_En,
		ubTempBuffer, 0x1);

	if (ubTempBuffer[0x0] & 0x1) {
		ubTempBuffer[0x0] = 0x10;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00107, ubTempBuffer, 0x1);
		mtk_dp->info.bSinkSSC_En = true;
		mhal_DPTx_SSCOnOffSetting(mtk_dp, true);
	} else {
		mtk_dp->info.bSinkSSC_En = false;
		mhal_DPTx_SSCOnOffSetting(mtk_dp, false);
	}
#endif

	return true;
}
#endif


#if (DPTX_AutoTest_ENABLE == 0x1) && (DPTX_PHY_TEST_PATTERN_EN == 0x1)
bool mdrv_DPTx_PHY_AdjustSwingPre(struct mtk_dp *mtk_dp, BYTE ubLaneCount)
{
	BYTE ubSwingValue;
	BYTE ubPreemphasis;
	BYTE ubTempBuf1[0x2];
	BYTE ubDPCP_Buffer1[0x4];
	bool bReplyStatus = false;

	memset(ubDPCP_Buffer1, 0x0, sizeof(ubDPCP_Buffer1));
	bReplyStatus = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00206,
				ubTempBuf1, 0x2);
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00103, ubDPCP_Buffer1, 0x4);
	if (bReplyStatus) {
		if (ubLaneCount >= 0x1) {
			ubSwingValue = (ubTempBuf1[0x0]&0x3);
			ubPreemphasis = ((ubTempBuf1[0x0]&0x0C)>>2);
			//Adjust the swing and pre-emphasis
			mhal_DPTx_SetSwingtPreEmphasis(mtk_dp, DPTx_LANE0,
				ubSwingValue, ubPreemphasis);
			//Adjust the swing and pre-emphasis done,
				//notify Sink Side
			ubDPCP_Buffer1[0x0] =
				ubSwingValue | (ubPreemphasis << 3);
			//MAX_SWING_REACHED
			if (ubSwingValue == DPTx_SWING3)
				ubDPCP_Buffer1[0x0] |= BIT(2);

			//MAX_PRE-EMPHASIS_REACHED
			if (ubPreemphasis == DPTx_PREEMPHASIS3)
				ubDPCP_Buffer1[0x0] |= BIT(5);

		}

		if (ubLaneCount >= 0x2) {
			ubSwingValue = (ubTempBuf1[0x0]&0x30) >> 4;
			ubPreemphasis = ((ubTempBuf1[0x0]&0xC0)>>6);
			//Adjust the swing and pre-emphasis
			mhal_DPTx_SetSwingtPreEmphasis(mtk_dp, DPTx_LANE1,
				ubSwingValue, ubPreemphasis);
			//Adjust the swing and pre-emphasis done,
				//notify Sink Side
			ubDPCP_Buffer1[0x1] =
				ubSwingValue | (ubPreemphasis << 3);
			if (ubSwingValue == DPTx_SWING3) {  //MAX_SWING_REACHED
				ubDPCP_Buffer1[0x1] |= BIT(2);
			}
			//MAX_PRE-EMPHASIS_REACHED
			if (ubPreemphasis == DPTx_PREEMPHASIS3)
				ubDPCP_Buffer1[0x1] |= BIT(5);

		}

		if (ubLaneCount == 0x4) {
			ubSwingValue = (ubTempBuf1[0x1]&0x3);
			ubPreemphasis = ((ubTempBuf1[0x1]&0x0C)>>2);

			//Adjust the swing and pre-emphasis
			mhal_DPTx_SetSwingtPreEmphasis(mtk_dp, DPTx_LANE2,
				ubSwingValue, ubPreemphasis);
			//Adjust the swing and pre-emphasis done,
				//notify Sink Side
			ubDPCP_Buffer1[0x2] =
				ubSwingValue | (ubPreemphasis << 3);
			if (ubSwingValue == DPTx_SWING3) { //MAX_SWING_REACHED
				ubDPCP_Buffer1[0x2] |= BIT(2);
			}
			//MAX_PRE-EMPHASIS_REACHED
			if (ubPreemphasis == DPTx_PREEMPHASIS3)
				ubDPCP_Buffer1[0x2] |= BIT(5);


			ubSwingValue = (ubTempBuf1[0x1]&0x30) >> 4;
			ubPreemphasis = ((ubTempBuf1[0x1]&0xC0)>>6);

			//Adjust the swing and pre-emphasis
			mhal_DPTx_SetSwingtPreEmphasis(mtk_dp, DPTx_LANE3,
				ubSwingValue, ubPreemphasis);
			//Adjust the swing and pre-emphasis done,
				//notify Sink Side
			ubDPCP_Buffer1[0x3] =
				ubSwingValue | (ubPreemphasis << 3);
			if (ubSwingValue == DPTx_SWING3) { //MAX_SWING_REACHED
				ubDPCP_Buffer1[0x3] |= BIT(2);
			}
			//MAX_PRE-EMPHASIS_REACHED
			if (ubPreemphasis == DPTx_PREEMPHASIS3)
				ubDPCP_Buffer1[0x3] |= BIT(5);

		}
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00103,
			ubDPCP_Buffer1, 0x4);
	}
	return true;
}

bool mdrv_DPTx_PHY_PatternSetting(struct mtk_dp *mtk_dp, BYTE ubPatternType,
	BYTE ubTEST_LANE_COUNT)
{
	BYTE i;
	BYTE PGdata[5] = {0x1F, 0x7C, 0xF0, 0xC1, 0x07};

	mhal_DPTx_DataLanePNSwap(mtk_dp, false);
	mhal_DPTx_PHY_ResetPattern(mtk_dp);
	switch (ubPatternType) {
	case PATTERN_NONE:
		//Disable U02 patch code, add interskew for test pattern,
			//lane1/2/3 select lane0 pipe delay
		break;
	case PATTERN_D10_2:
		mhal_DPTx_SetTxTrainingPattern(mtk_dp, BIT(4));
		mhal_DPTx_SetTxLane(mtk_dp, (ubTEST_LANE_COUNT/2));
		break;
	case PATTERN_SYMBOL_ERR:
		break;
	case PATTERN_PRBS7:
		mhal_DPTx_DataLanePNSwap(mtk_dp, true);
		mhal_DPTx_ProgramPatternEnable(mtk_dp, true);
		mhal_DPTx_PRBSEnable(mtk_dp, true);
		mhal_DPTx_ProgramPatternEnable(mtk_dp, true);
		mhal_DPTx_PRBSEnable(mtk_dp, true);
		mhal_DPTx_PatternSelect(mtk_dp, DPTx_PG_PRBS7);
		break;
	case PATTERN_80B:
		for (i = 0x0; i < 4; i++)
			mhal_DPTx_SetProgramPattern(mtk_dp, i, PGdata);

		mhal_DPTx_ProgramPatternEnable(mtk_dp, true);
		mhal_DPTx_PatternSelect(mtk_dp, DPTx_PG_80bit);
		break;
	case PATTERN_HBR2_COM_EYE:
		mhal_DPTx_ComplianceEyeEnSetting(mtk_dp, true);
		break;
	};
	return true;
}


struct DP_CTS_AUTO_REQ cts_req;
bool mdrv_DPTx_Video_PG_AutoTest(struct mtk_dp *mtk_dp)//, BYTE ubDPCD_201)
{
	BYTE i;
	BYTE dpcd22x[16];  //220~22F
	BYTE dpcd23x[5];   //230~234
	BYTE dpcd27x[10];	//220~22F
	BYTE ucMISC[2] = {0x00};

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00220, dpcd22x, 16);
	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00230, dpcd23x, 5);
	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00271, dpcd27x, 10);

	for (i = 0; i < 10; i++)
		DPTXMSG("dpcd27%d = 0x%x\n", i+1, dpcd27x[i]);

	cts_req.test_aduio_channel_count = (((dpcd27x[0]) & 0xF0) >> 4) + 1;
	cts_req.test_aduio_samling_rate = ((dpcd27x[0]) & 0x0F) + 1;
	DPTXMSG("channel = %d, sr = %d\n",
		cts_req.test_aduio_channel_count,
		cts_req.test_aduio_samling_rate);
	cts_req.test_pattern = (dpcd22x[1]);
	cts_req.test_h_total = ((dpcd22x[2]<<8) + (dpcd22x[3]));
	cts_req.test_v_total = ((dpcd22x[4]<<8) + (dpcd22x[5]));
	cts_req.test_h_start = ((dpcd22x[6]<<8) + (dpcd22x[7]));
	cts_req.test_v_start = ((dpcd22x[8]<<8) + (dpcd22x[9]));
	cts_req.test_hsync_polarity = ((dpcd22x[0xa] & 0x80)>>7);
	cts_req.test_hsync_width =
		(((dpcd22x[0xa] & 0x7f)<<8) + (dpcd22x[0xb]));
	cts_req.test_vsync_polarity = ((dpcd22x[0xc] & 0x80)>>7);
	cts_req.test_vsync_width =
		(((dpcd22x[0xc] & 0x7f)<<8) + (dpcd22x[0xd]));
	cts_req.test_h_width = ((dpcd22x[0xe]<<8) + (dpcd22x[0xf]));
	cts_req.test_v_height = ((dpcd23x[0]<<8) + (dpcd23x[1]));
	cts_req.test_sync_clk = (dpcd23x[2] & 0x1);
	cts_req.test_color_fmt = ((dpcd23x[2] & 0x6)>>1);
	cts_req.test_dynamic_range = ((dpcd23x[2] & 0x8)>>3);
	cts_req.test_YCbCr_coefficient = ((dpcd23x[2] & 0x10)>>4);
	cts_req.test_bit_depth = ((dpcd23x[2] & 0xe0)>>5);
	cts_req.test_refresh_denominator = (dpcd23x[3] & 0x1);
	cts_req.test_interlaced = ((dpcd23x[3] & 0x2)>>1);
	cts_req.test_refresh_rate_numerator = (dpcd23x[4]);

	DPTXMSG("[DPTXCTS] test request:\n");
	DPTXMSG("req.test_pattern = %d\n", cts_req.test_pattern);
	DPTXMSG("req.test_h_total = %d\n", cts_req.test_h_total);
	DPTXMSG("req.test_v_total = %d\n", cts_req.test_v_total);
	DPTXMSG("req.test_h_start = %d\n", cts_req.test_h_start);
	DPTXMSG("req.test_v_start = %d\n", cts_req.test_v_start);
	DPTXMSG("req.test_hsync_polarity = %d\n",
		cts_req.test_hsync_polarity);
	DPTXMSG("req.test_hsync_width = d\n",
		cts_req.test_hsync_width);
	DPTXMSG("req.test_vsync_polarity = %d\n",
		cts_req.test_vsync_polarity);
	DPTXMSG("req.test_vsync_width =  %d\n",
		cts_req.test_vsync_width);
	DPTXMSG("req.test_h_width = %d\n", cts_req.test_h_width);
	DPTXMSG("req.test_v_height = %d\n", cts_req.test_v_height);
	DPTXMSG("req.test_sync_clk = %d\n", cts_req.test_sync_clk);
	DPTXMSG("req.test_color_fmt = %d\n", cts_req.test_color_fmt);
	DPTXMSG("req.test_dynamic_range = %d\n",
		cts_req.test_dynamic_range);
	DPTXMSG("req.test_YCbCr_coefficient = %d\n",
		cts_req.test_YCbCr_coefficient);
	DPTXMSG("req.test_bit_depth = %d\n", cts_req.test_bit_depth);
	DPTXMSG("req.test_refresh_denominator = %d\n",
		cts_req.test_refresh_denominator);
	DPTXMSG("req.test_interlaced = %d\n",
		cts_req.test_interlaced);
	DPTXMSG("req.test_refresh_rate_numerator = %d\n",
		cts_req.test_refresh_rate_numerator);

	mtk_dp->info.DPTX_OUTBL.Htt = cts_req.test_h_total;
	mtk_dp->info.DPTX_OUTBL.Hde = cts_req.test_h_width;
	mtk_dp->info.DPTX_OUTBL.Hfp = cts_req.test_h_total
		- cts_req.test_h_start - cts_req.test_h_width;
	mtk_dp->info.DPTX_OUTBL.Hsw = cts_req.test_hsync_width;
	mtk_dp->info.DPTX_OUTBL.bHsp = cts_req.test_hsync_polarity;
	mtk_dp->info.DPTX_OUTBL.Hbp = cts_req.test_h_start
		- cts_req.test_hsync_width;
	mtk_dp->info.DPTX_OUTBL.Vtt = cts_req.test_v_total;
	mtk_dp->info.DPTX_OUTBL.Vde = cts_req.test_v_height;
	mtk_dp->info.DPTX_OUTBL.Vfp = cts_req.test_v_total
		- cts_req.test_v_start - cts_req.test_v_height;
	mtk_dp->info.DPTX_OUTBL.Vsw = cts_req.test_vsync_width;
	mtk_dp->info.DPTX_OUTBL.bVsp = cts_req.test_vsync_polarity;
	mtk_dp->info.DPTX_OUTBL.Vbp = cts_req.test_v_start
		- cts_req.test_vsync_width;
	mtk_dp->info.DPTX_OUTBL.Hbk = cts_req.test_h_total
		- cts_req.test_h_width;
	mtk_dp->info.DPTX_OUTBL.FrameRate = cts_req.test_refresh_rate_numerator;
	mtk_dp->info.DPTX_OUTBL.PixRateKhz = 0;
	mtk_dp->info.DPTX_OUTBL.Video_ip_mode = DPTX_VIDEO_PROGRESSIVE;
	if (cts_req.test_interlaced)
		DPTXMSG("Warning: not support interlace\n");

	//Clear MISC1&0 except [2:1]Colorfmt & [7:5]ColorDepth
	mhal_DPTx_SetMISC(mtk_dp, ucMISC);

	mtk_dp->info.format = cts_req.test_color_fmt;
	mtk_dp->info.depth = cts_req.test_bit_depth;
	//mtk_dp->info.DPTX_MISC.dp_misc.spec_def1 = cts_req.test_dynamic_range;

	mhal_DPTx_SetColorFormat(mtk_dp, mtk_dp->info.format);
	mhal_DPTx_SetColorDepth(mtk_dp, mtk_dp->info.depth);
	mhal_DPTx_SetMSA(mtk_dp);
	mdrv_DPTx_SetDPTXOut(mtk_dp);
	mdrv_DPTx_VideoMute(mtk_dp, false);

	return 0;
}

void mdrv_DPTx_Audio_PG_AutoTest(struct mtk_dp *mtk_dp)
{
	BYTE SDP_DB[32] = {0};
	BYTE SDP_HB[4] = {0};
	BYTE ucWordlength = WL_24bit;

	mhal_DPTx_Audio_SDP_Setting(mtk_dp, cts_req.test_aduio_channel_count);
	mhal_DPTx_Audio_Ch_Status_Set(mtk_dp, cts_req.test_aduio_channel_count,
		cts_req.test_aduio_samling_rate, ucWordlength);
	mdrv_DPTx_I2S_Audio_Enable(mtk_dp, false);

	SDP_HB[1] = DP_SPEC_SDPTYP_AINFO;
	SDP_HB[2] = 0x1B;
	SDP_HB[3] = 0x48;

	mdrv_DPTx_SPKG_SDP(mtk_dp, 1, 4, SDP_HB, SDP_DB);
}

bool mdrv_DPTx_PHY_AutoTest(struct mtk_dp *mtk_dp, BYTE ubDPCD_201)
{
	bool bAutoTestIRQ = false;
	BYTE ubDPCD_218 = 0x0;
	BYTE ubTempBuffer[0x10];

#if DPTX_PHY_TEST_PATTERN_EN
	BYTE ubDPCD_248;
#if (DPTX_TEST_SYMBERR_EN)
	WORD usSYMERRCNT_N;             // for sym Error Count
#endif
#if (DPTX_TEST_SYMBERR_EN | DPTX_TEST_PRBS7_EN | DPTX_TEST_HBR2EYE_EN | \
DPTX_TEST_PHY80B_EN)
	// for  1.sym Error Count (DPCD0210-0217)  / 2.PHY 80b(DPCD_0250-0259)
	BYTE ubDPCDRDBUF[10];
#endif
#endif

	BYTE ubTEST_LINK_RATE;   // DPCD_219
	BYTE ubTEST_LANE_COUNT;  // DPCD_220

	memset(ubTempBuffer, 0x0, sizeof(ubTempBuffer));
	DPTXMSG("PHY_AutoTest Start\n");
	DPTXMSG("DPCD 201 = 0x%x\n", ubDPCD_201);

	if (ubDPCD_201 & BIT(1)) {
		bAutoTestIRQ = true;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00201, &ubDPCD_201, 0x1);
		mdelay(1);
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00218, &ubDPCD_218, 0x1);
		DPTXMSG("DPCD 218 = 0x%x\n", ubDPCD_218);

		switch (ubDPCD_218 & 0xFF)	{
		case BIT0://TEST_LINK_TRAINING:
#if DPTX_TEST_LINK_TRAINING_EN
			DPTXMSG("TEST_LINK_TRAINING\n");
			ubTempBuffer[0x0] = 0x01;
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
				ubTempBuffer, 0x1);
			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00219,
				&ubTEST_LINK_RATE, 0x1);
			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00220,
				&ubTEST_LANE_COUNT, 0x1);
			mtk_dp->training_info.bDPTxAutoTest_EN = true;

			if ((ubTEST_LINK_RATE != 0x0) &&
				(ubTEST_LANE_COUNT != 0x0)) {
				mtk_dp->training_info.ubLinkRate
					= ubTEST_LINK_RATE;
				mtk_dp->training_info.ubLinkLaneCount =
					ubTEST_LANE_COUNT;
				ubTempBuffer[0x0] = 0x0;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00102,
					ubTempBuffer, 0x1);
				ubTempBuffer[0x0] = 0x2;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00600,
					ubTempBuffer, 0x1);
				mdrv_DPTx_SetTrainingStart(mtk_dp);
			}
			return true;
#endif
			break;

		case BIT1: //TEST_VIDEO_PATTERN
#if DPTX_TEST_PATTERN_EN
			DPTXMSG("TEST_PATTERN\n");
			mtk_dp->training_info.bDPTxAutoTest_EN = true;
			mdrv_DPTx_Video_PG_AutoTest(mtk_dp);
/*
 *			ubTempBuffer[0x0] = 0x01;
 *			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
 *				ubTempBuffer, 0x1);
 */
			return true;
#else
			ubTempBuffer[0x0] = 0x01;
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
				ubTempBuffer, 0x1);
#endif
			break;

		case (BIT1 | BIT5)://(TEST_VIDEO_PATTERN | TEST_AUDIO_PATTERN)
			DPTXMSG("TEST VIDEO/AUDIO PATTERN\n");
			mtk_dp->training_info.bDPTxAutoTest_EN = true;
			mdrv_DPTx_Video_PG_AutoTest(mtk_dp);
			mdrv_DPTx_Audio_PG_AutoTest(mtk_dp);
			ubTempBuffer[0x0] = 0x01;
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
				ubTempBuffer, 0x1);
			return true;

		case BIT2: //TEST_EDID_READ
#if DPTX_TEST_EDID_READ_EN
			DPTXMSG("TEST_EDID_R\n");
			if (mtk_dp->edid)
				kfree(mtk_dp->edid)
			mtk_dp->edid = mtk_dp_handle_edid(mtk_dp);
			mdelay(10);
			ubTempBuffer[0x0] = mtk_dp->edid->checksum;
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00261,
			ubTempBuffer[0x0] = 0x05;
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
				ubTempBuffer, 0x1);
			return true;
#else
			ubTempBuffer[0x0] = 0x01;
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
				ubTempBuffer, 0x1);
#endif
			break;

		case BIT3://TEST_PHY_PATTERN
#if DPTX_PHY_TEST_PATTERN_EN
			DPTXMSG("PHY_TEST_PATTERN");
			drm_dp_dpcd_read(&mtk_dp->aux, 0x00248,
				&ubDPCD_248, 0x1);
			drm_dp_dpcd_read(&mtk_dp->aux, 0x00220,
				&ubTEST_LANE_COUNT, 0x1);
			mdrv_DPTx_PHY_AdjustSwingPre(mtk_dp, ubTEST_LANE_COUNT);
			switch (ubDPCD_248&0x07) {
			case PATTERN_NONE:
				mdrv_DPTx_PHY_PatternSetting(mtk_dp,
					PATTERN_NONE, ubTEST_LANE_COUNT);
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux,
					DPCD_00260, ubTempBuffer, 0x1);
				break;
			case PATTERN_D10_2:
#if DPTX_TEST_D10_2_EN
				mdrv_DPTx_PHY_PatternSetting(mtk_dp,
					PATTERN_D10_2, ubTEST_LANE_COUNT);
				mdelay(2);
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
#else
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
				DPTXMSG("NOT SUPPORT D10.2\n");
#endif
				break;
			case PATTERN_SYMBOL_ERR:
#if DPTX_TEST_SYMBERR_EN
				mdrv_DPTx_PHY_PatternSetting(mtk_dp,
					PATTERN_SYMBOL_ERR, ubTEST_LANE_COUNT);
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
				usSYMERRCNT_N = 0x0005;
				mdelay(1000);
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00210,
					ubDPCDRDBUF, 8);
#else
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
#endif
				break;
			case PATTERN_PRBS7:
#if DPTX_TEST_PRBS7_EN
				mdrv_DPTx_PHY_PatternSetting(mtk_dp,
					PATTERN_PRBS7, ubTEST_LANE_COUNT);
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
				mdelay(1000);
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00210,
					ubDPCDRDBUF, 8);
#else
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
#endif
				break;
			case PATTERN_80B:
#if DPTX_TEST_PHY80B_EN
				mdrv_DPTx_PHY_PatternSetting(mtk_dp,
					PATTERN_80B, ubTEST_LANE_COUNT);
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00250,
					ubDPCDRDBUF, 10);
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
#else
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
#endif
				break;

			case PATTERN_HBR2_COM_EYE:
#if DPTX_TEST_HBR2EYE_EN
				mdrv_DPTx_PHY_PatternSetting(mtk_dp,
					PATTERN_HBR2_COM_EYE,
					ubTEST_LANE_COUNT);
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00210,
					ubDPCDRDBUF, 8);
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
#endif
				break;
			case CP2520_PATTERN3:
#if DPTX_TEST_CP2520_P3_EN
				mhal_DPTx_SetTxTrainingPattern(mtk_dp, BIT(7));
				mhal_DPTx_SetTxLane(mtk_dp,
					ubTEST_LANE_COUNT / 2);
#else
				ubTempBuffer[0x0] = 0x01;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00260,
					ubTempBuffer, 0x1);
#endif
				break;
			default:
				break;
			};
#endif
			break;

		default:
			DPTXMSG("DPCD 218 Not support\n");
			return false;
		}
	} else {
		bAutoTestIRQ = false;
		return false;
	}

	mdelay(1);
	mdrv_DPTx_CheckSSC(mtk_dp);
	return true;
}
#endif

uint8_t mdrv_DPTx_getSinkCount(struct mtk_dp *mtk_dp)
{
	uint8_t temp;

	if (mtk_dp->training_info.bSinkEXTCAP_En)
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02002, &temp, 0x1);
	else
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, &temp, 0x1);

	DPTXMSG("Sink Count = %d\n", DP_GET_SINK_COUNT(temp));
	return DP_GET_SINK_COUNT(temp);
}

void mdrv_DPTx_CheckSinkHPDEvent(struct mtk_dp *mtk_dp)
{
	u8 ubDPCD20x[6];
	u8 ubDPCD2002[2];
	u8 ubDPCD200C[4];
	u8 sink_cnt = 0;
	bool ret;

	memset(ubDPCD20x, 0x0, sizeof(ubDPCD20x));
	memset(ubDPCD2002, 0x0, sizeof(ubDPCD2002));
	memset(ubDPCD200C, 0x0, sizeof(ubDPCD200C));

	if (mtk_dp->training_info.bSinkEXTCAP_En) {
		ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02002,
			ubDPCD2002, 0x2);
		if (!ret) {
			DPTXMSG("Read DPCD_02002 Fail\n");
			return;
		}

		ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_0200C,
			ubDPCD200C, 0x4);
		if (!ret) {
			DPTXMSG("Read DPCD_0200C Fail\n");
			return;
		}
	}

	ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, ubDPCD20x, 0x6);
	if (!ret) {
		DPTXMSG("Read DPCD200 Fail\n");
		return;
	}

	sink_cnt = mdrv_DPTx_getSinkCount(mtk_dp);
	if ((sink_cnt != mtk_dp->training_info.ubSinkCountNum) ||
		(ubDPCD200C[0x2] & BIT6 || ubDPCD20x[0x4] & BIT6)) {
		DPTXMSG("New Branch Device Detection!!\n");

		if (!mtk_dp->bUeventToHwc) {
			mtk_dp->disp_status = DPTX_DISP_NONE;
			mtk_dp_hotplug_uevent(0);
			mtk_dp->bUeventToHwc = true;
		}

		mdrv_DPTx_deinit(mtk_dp);
		mtk_dp->training_info.ubSinkCountNum = sink_cnt;
		mtk_dp->training_state = DPTX_NTSTATE_STARTUP;
		mdelay(20);
		return;
	}

	if (sink_cnt == 0) {
		mtk_dp->training_state = DPTX_NTSTATE_STARTUP;
		mdelay(200);
		return;
	}

	mdrv_DPTx_CheckSinkLock(mtk_dp, ubDPCD20x, ubDPCD200C);
	mdrv_DPTx_CheckSinkESI(mtk_dp, ubDPCD20x, ubDPCD2002);
}

void mdrv_DPTx_CheckMaxLinkRate(struct mtk_dp *mtk_dp)
{
	switch (mtk_dp->training_info.ubDPSysVersion) {
	case DP_VERSION_11:
		mtk_dp->training_info.ubSysMaxLinkRate = DP_LINKRATE_HBR;
		break;
	case DP_VERSION_12:
		mtk_dp->training_info.ubSysMaxLinkRate = DP_LINKRATE_HBR2;
		break;
	case DP_VERSION_14:
		mtk_dp->training_info.ubSysMaxLinkRate = DP_LINKRATE_HBR3;
		break;
	default:
		mtk_dp->training_info.ubSysMaxLinkRate = DP_LINKRATE_HBR2;
		break;
	}
}

void mdrv_DPTx_DisconnetInit(struct mtk_dp *mtk_dp)
{
	mdrv_DPTx_CheckMaxLinkRate(mtk_dp);
}

void mdrv_DPTx_SPKG_SDP(struct mtk_dp *mtk_dp, bool bEnable, u8 ucSDPType,
	u8 *pHB, u8 *pDB)
{
	mhal_DPTx_SPKG_SDP(mtk_dp, bEnable, ucSDPType, pHB, pDB);
}

void mdrv_DPTx_PatternSet(bool enable, int resolution)
{
	g_mtk_dp->info.bPatternGen = enable;
	g_mtk_dp->info.resolution = resolution;
}

void mdrv_DPTx_set_maxlinkrate(bool enable, int maxlinkrate)
{
	g_mtk_dp->training_info.set_max_linkrate = enable;
	g_mtk_dp->training_info.ubSysMaxLinkRate = maxlinkrate;
}

void mdrv_DPTx_PatternGenTypeSel(struct mtk_dp *mtk_dp, int patternType,
	BYTE BGR, DWORD ColorDepth, BYTE Location)
{
	WORD Hde, Vde;

	Hde = mtk_dp->info.DPTX_OUTBL.Hde;
	Vde = mtk_dp->info.DPTX_OUTBL.Vde;

	switch (patternType) {
	case DPTX_PG_PURE_COLOR:
		mhal_DPTx_PG_Pure_Color(mtk_dp, BGR, ColorDepth);
		break;

	case DPTX_PG_VERTICAL_RAMPING:
		mhal_DPTx_PG_VerticalRamping(mtk_dp, BGR, ColorDepth, Location);
		break;

	case DPTX_PG_HORIZONTAL_RAMPING:
		mhal_DPTx_PG_HorizontalRamping(mtk_dp, BGR,
			ColorDepth, Location);
		break;

	case DPTX_PG_VERTICAL_COLOR_BAR:
		mhal_DPTx_PG_VerticalColorBar(mtk_dp, Location);
		break;

	case DPTX_PG_HORIZONTAL_COLOR_BAR:
		mhal_DPTx_PG_HorizontalColorBar(mtk_dp, Location);
		break;

	case DPTX_PG_CHESSBOARD_PATTERN:
		mhal_DPTx_PG_Chessboard(mtk_dp, Location, Hde, Vde);
		break;

	case DPTX_PG_SUB_PIXEL_PATTERN:
		mhal_DPTx_PG_SubPixel(mtk_dp, Location);
		break;

	case DPTX_PG_FRAME_PATTERN:
		mhal_DPTx_PG_Frame(mtk_dp, Location, Hde, Vde);
		break;

	default:
		break;
	}
}

void mdrv_DPTx_StopSentSDP(struct mtk_dp *mtk_dp)
{
	u8 ubPkgType;

	for (ubPkgType = DPTx_SDPTYP_ACM ; ubPkgType < DPTx_SDPTYP_MAX_NUM;
		ubPkgType++)
		mhal_DPTx_SPKG_SDP(mtk_dp, false, ubPkgType, NULL, NULL);

	mhal_DPTx_SPKG_VSC_EXT_VESA(mtk_dp, false, 0x00, NULL);
	mhal_DPTx_SPKG_VSC_EXT_CEA(mtk_dp, false, 0x00, NULL);

	DPTXFUNC();
}

int mdrv_DPTx_HPD_HandleInThread(struct mtk_dp *mtk_dp)
{
	int ret = DPTX_NOERR;

	if (mtk_dp->training_info.bCableStateChange) {
		bool ubCurrentHPD = mhal_DPTx_GetHPDPinLevel(mtk_dp);

		mtk_dp->training_info.bCableStateChange = false;

		if (mtk_dp->training_info.bCablePlugIn && ubCurrentHPD) {
			DPTXMSG("HPD_CON\n");
		} else {
			DPTXMSG("HPD_DISCON\n");
			mdrv_DPTx_VideoMute(mtk_dp, true);
			mdrv_DPTx_AudioMute(mtk_dp, true);

			if (mtk_dp->bUeventToHwc) {
				mtk_dp_hotplug_uevent(0);
				mtk_dp->bUeventToHwc = false;
				mtk_dp->disp_status = DPTX_DISP_NONE;
			} else
				DPTXMSG("Skip uevent(0)\n");

			cancel_work(&mtk_dp->hdcp_work);

#ifdef DPTX_HDCP_ENABLE
			if (mtk_dp->info.hdcp2_info.bEnable)
				mdrv_DPTx_HDCP2_SetStartAuth(mtk_dp, false);
			else if (mtk_dp->info.hdcp1x_info.bEnable)
				mdrv_DPTx_HDCP1X_SetStartAuth(mtk_dp, false);

			tee_removeDevice();
#endif

			mdrv_DPTx_InitVariable(mtk_dp);
			mhal_DPTx_PHY_SetIdlePattern(mtk_dp, true);
			if (mtk_dp->has_fec)
				mhal_DPTx_EnableFEC(mtk_dp, false);
			mdrv_DPTx_StopSentSDP(mtk_dp);
			mhal_DPTx_AnalogPowerOnOff(mtk_dp, false);

			DPTXMSG("Power OFF %d", mtk_dp->bPowerOn);
			clk_disable_unprepare(mtk_dp->dp_tx_clk);
			if (mtk_dp->info.bPatternGen)
				mhal_DPTx_VideoClock(false,
					mtk_dp->info.resolution);

			fakecablein = false;
			fakeres = FAKE_DEFAULT_RES;
			fakebpc = DP_COLOR_DEPTH_8BIT;

			kfree(mtk_dp->edid);
			mtk_dp->edid = NULL;
			ret = DPTX_PLUG_OUT;
		}
	}

	if (mtk_dp->training_info.usPHY_STS & HPD_INT_EVNET) {
		DPTXMSG("HPD_INT_EVNET\n");
		mtk_dp->training_info.usPHY_STS &= ~HPD_INT_EVNET;
		mdrv_DPTx_CheckSinkHPDEvent(mtk_dp);
	}

	return ret;
}

void mdrv_DPTx_VideoMute(struct mtk_dp *mtk_dp, bool bENABLE)
{
	mtk_dp->info.bVideoMute = (mtk_dp->info.bSetVideoMute) ?
		true : bENABLE;
	mhal_DPTx_VideoMute(mtk_dp, mtk_dp->info.bVideoMute);
}

void mdrv_DPTx_AudioMute(struct mtk_dp *mtk_dp, bool bENABLE)
{
	mtk_dp->info.bAudioMute = (mtk_dp->info.bSetAudioMute) ?
		true : bENABLE;
	mhal_DPTx_AudioMute(mtk_dp, mtk_dp->info.bAudioMute);
}

bool mdrv_DPTx_TrainingCheckSwingPre(struct mtk_dp *mtk_dp,
	u8 ubTargetLaneCount, u8 *ubDPCP202_x, u8 *ubDPCP_Buffer1)
{
	u8 ubSwingValue;
	u8 ubPreemphasis;

	if (ubTargetLaneCount >= 0x1) { //lane0
		ubSwingValue = (ubDPCP202_x[0x4]&0x3);
		ubPreemphasis = ((ubDPCP202_x[0x4]&0x0C)>>2);
		//Adjust the swing and pre-emphasis
		mhal_DPTx_SetSwingtPreEmphasis(mtk_dp, DPTx_LANE0,
			ubSwingValue, ubPreemphasis);
		//Adjust the swing and pre-emphasis done, notify Sink Side
		ubDPCP_Buffer1[0x0] = ubSwingValue | (ubPreemphasis << 3);
		if (ubSwingValue == DPTx_SWING3) { //MAX_SWING_REACHED
			ubDPCP_Buffer1[0x0] |= BIT2;
		}
		//MAX_PRE-EMPHASIS_REACHED
		if (ubPreemphasis == DPTx_PREEMPHASIS3)
			ubDPCP_Buffer1[0x0] |= BIT5;

	}

	if (ubTargetLaneCount >= 0x2) { //lane1
		ubSwingValue = (ubDPCP202_x[0x4]&0x30) >> 4;
		ubPreemphasis = ((ubDPCP202_x[0x4]&0xC0)>>6);
		//Adjust the swing and pre-emphasis
		mhal_DPTx_SetSwingtPreEmphasis(mtk_dp, DPTx_LANE1,
			ubSwingValue, ubPreemphasis);
		//Adjust the swing and pre-emphasis done, notify Sink Side
		ubDPCP_Buffer1[0x1] = ubSwingValue | (ubPreemphasis << 3);
		if (ubSwingValue == DPTx_SWING3) { //MAX_SWING_REACHED
			ubDPCP_Buffer1[0x1] |= BIT2;
		}
		//MAX_PRE-EMPHASIS_REACHED
		if (ubPreemphasis == DPTx_PREEMPHASIS3)
			ubDPCP_Buffer1[0x1] |= BIT5;

	}

	if (ubTargetLaneCount == 0x4) { //lane 2,3
		ubSwingValue = (ubDPCP202_x[0x5]&0x3);
		ubPreemphasis = ((ubDPCP202_x[0x5]&0x0C)>>2);

		//Adjust the swing and pre-emphasis
		mhal_DPTx_SetSwingtPreEmphasis(mtk_dp, DPTx_LANE2,
			(ubDPCP202_x[0x5]&0x3), ((ubDPCP202_x[0x5]&0x0C)>>2));
		//Adjust the swing and pre-emphasis done, notify Sink Side
		ubDPCP_Buffer1[0x2] = ubSwingValue | (ubPreemphasis << 3);
		if (ubSwingValue == DPTx_SWING3) { //MAX_SWING_REACHED
			ubDPCP_Buffer1[0x2] |= BIT2;
		}
		//MAX_PRE-EMPHASIS_REACHED
		if (ubPreemphasis == DPTx_PREEMPHASIS3)
			ubDPCP_Buffer1[0x2] |= BIT5;


		ubSwingValue = (ubDPCP202_x[0x5]&0x30) >> 4;
		ubPreemphasis = ((ubDPCP202_x[0x5]&0xC0)>>6);

		//Adjust the swing and pre-emphasis
		mhal_DPTx_SetSwingtPreEmphasis(mtk_dp, DPTx_LANE3,
			((ubDPCP202_x[0x5]&0x30)>>4),
			((ubDPCP202_x[0x5]&0xC0)>>6));
		//Adjust the swing and pre-emphasis done, notify Sink Side
		ubDPCP_Buffer1[0x3] = ubSwingValue | (ubPreemphasis << 3);
		if (ubSwingValue == DPTx_SWING3) { //MAX_SWING_REACHED
			ubDPCP_Buffer1[0x3] |= BIT2;
		}
		//MAX_PRE-EMPHASIS_REACHED
		if (ubPreemphasis == DPTx_PREEMPHASIS3)
			ubDPCP_Buffer1[0x3] |= BIT5;
	}

	//Wait signal stable enough
	mdelay(2);
	return true;
}

void mdrv_DPTx_Print_TrainingState(u8 state)
{
	switch (state) {
	case DPTX_NTSTATE_STARTUP:
		DPTXMSG("NTSTATE_STARTUP!\n");
		break;
	case DPTX_NTSTATE_CHECKCAP:
		DPTXMSG("NTSTATE_CHECKCAP!\n");
		break;
	case DPTX_NTSTATE_CHECKEDID:
		DPTXMSG("NTSTATE_CHECKEDID!\n");
		break;
	case DPTX_NTSTATE_TRAINING_PRE:
		DPTXMSG("NTSTATE_TRAINING_PRE!\n");
		break;
	case DPTX_NTSTATE_TRAINING:
		DPTXMSG("NTSTATE_TRAINING!\n");
		break;
	case DPTX_NTSTATE_CHECKTIMING:
		DPTXMSG("NTSTATE_CHECKTIMING!\n");
		break;
	case DPTX_NTSTATE_NORMAL:
		DPTXMSG("NTSTATE_NORMAL!\n");
		break;
	case DPTX_NTSTATE_POWERSAVE:
		DPTXMSG("NTSTATE_POWERSAVE!\n");
		break;
	case DPTX_NTSTATE_DPIDLE:
		DPTXMSG("NTSTATE_DPIDLE!\n");
		break;
	}
}

int mdrv_DPTx_TrainingFlow(struct mtk_dp *mtk_dp, u8 ubLaneRate, u8 ubLaneCount)
{
	u8  ubTempValue[0x6];
	u8  ubDPCD200C[0x3];
	u8  ubTargetLinkRate = ubLaneRate;
	u8  ubTargetLaneCount = ubLaneCount;
	u8  ubDPCP_Buffer1[0x4];
	u8  bPassTPS1 = false;
	u8  bPassTPS2_3 = false;
	u8  ubTrainRetryTimes;
	u8  ubStatusControl;
	u8  ubIterationCount;
	u8  ubDPCD206;

	memset(ubTempValue, 0x0, sizeof(ubTempValue));
	memset(ubDPCP_Buffer1, 0x0, sizeof(ubDPCP_Buffer1));

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00600, ubTempValue, 0x1);
	if (ubTempValue[0] != 0x01) {
		ubTempValue[0] = 0x01;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00600, ubTempValue, 0x1);
		mdelay(1);
	}

	ubTempValue[0] = ubTargetLinkRate;
	ubTempValue[1] = ubTargetLaneCount | DPTX_AUX_SET_ENAHNCED_FRAME;
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00100, ubTempValue, 0x2);

	if (mtk_dp->training_info.bSinkSSC_En) {
		ubTempValue[0x0] = 0x10;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00107, ubTempValue, 0x1);
	}

	ubTrainRetryTimes = 0x0;
	ubStatusControl = 0x0;
	ubIterationCount = 0x1;
	ubDPCD206 = 0xFF;

	mhal_DPTx_SetTxLane(mtk_dp, ubTargetLaneCount/2);
	mhal_DPTx_SetTxRate(mtk_dp, ubTargetLinkRate);

	do {
		ubTrainRetryTimes++;
		if (!mtk_dp->training_info.bCablePlugIn ||
			((mtk_dp->training_info.usPHY_STS & HPD_DISCONNECT)
				!= 0x0)) {
			DPTXMSG("Training Abort! HPD is low\n");
			return DPTX_PLUG_OUT;
		}

		if (mtk_dp->training_info.usPHY_STS & HPD_INT_EVNET)
			mdrv_DPTx_HPD_HandleInThread(mtk_dp);

		if (mtk_dp->training_state < DPTX_NTSTATE_TRAINING)
			return DPTX_RETRANING;

		if (!bPassTPS1)	{
			DPTXMSG("CR Training START\n");
			mhal_DPTx_SetScramble(mtk_dp, false);

			if (ubStatusControl == 0x0)	{
				mhal_DPTx_SetTxTrainingPattern(mtk_dp, BIT4);
				ubStatusControl = 0x1;
				ubTempValue[0] = 0x21;
				drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00102,
					ubTempValue, 0x1);
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00206,
					(ubTempValue+4), 0x2);
				ubIterationCount++;

				mdrv_DPTx_TrainingCheckSwingPre(mtk_dp,
					ubTargetLaneCount, ubTempValue,
					ubDPCP_Buffer1);
			}
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00103,
				ubDPCP_Buffer1, ubTargetLaneCount);

			drm_dp_link_train_clock_recovery_delay(mtk_dp->rx_cap);
			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00202,
				ubTempValue, 0x6);

			if (mtk_dp->training_info.bSinkEXTCAP_En) {
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_0200C,
					ubDPCD200C, 0x3);
				ubTempValue[0] = ubDPCD200C[0];
				ubTempValue[1] = ubDPCD200C[1];
				ubTempValue[2] = ubDPCD200C[2];
			}

			if (drm_dp_clock_recovery_ok(ubTempValue,
				ubTargetLaneCount)) {
				DPTXMSG("CR Training Success\n");
				mtk_dp->training_info.cr_done = true;
				bPassTPS1 = true;
				ubTrainRetryTimes = 0x0;
				ubIterationCount = 0x1;
			} else {
				//request swing & emp is the same eith last time
				if (ubDPCD206 == ubTempValue[0x4]) {
					ubIterationCount++;
					if (ubDPCD206&0x3)
						ubIterationCount =
						DPTX_TRAIN_MAX_ITERATION;
				} else {
					ubDPCD206 = ubTempValue[0x4];
				}

				DPTXMSG("CR Training Fail\n");
			}
		} else if (bPassTPS1 && !bPassTPS2_3) {
			DPTXMSG("EQ Training START\n");
			if (ubStatusControl == 0x1) {
				if (mtk_dp->training_info.bTPS4) {
					mhal_DPTx_SetTxTrainingPattern(mtk_dp,
						BIT7);
					DPTXMSG("LT TPS4\n");
				} else if (mtk_dp->training_info.bTPS3) {
					mhal_DPTx_SetTxTrainingPattern(mtk_dp,
						BIT6);
					DPTXMSG("LT TP3\n");
				} else {
					mhal_DPTx_SetTxTrainingPattern(mtk_dp,
						BIT5);
					DPTXMSG("LT TPS2\n");
				}

				if (mtk_dp->training_info.bTPS4) {
					ubTempValue[0] = 0x07;
					drm_dp_dpcd_write(&mtk_dp->aux,
						DPCD_00102, ubTempValue, 0x1);
				} else if (mtk_dp->training_info.bTPS3) {
					ubTempValue[0] = 0x23;
					drm_dp_dpcd_write(&mtk_dp->aux,
						DPCD_00102, ubTempValue, 0x1);
				} else {
					ubTempValue[0] = 0x22;
					drm_dp_dpcd_write(&mtk_dp->aux,
						DPCD_00102, ubTempValue, 0x1);
				}

				ubStatusControl = 0x2;
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00206,
					(ubTempValue+4), 0x2);

				ubIterationCount++;
				mdrv_DPTx_TrainingCheckSwingPre(mtk_dp,
					ubTargetLaneCount, ubTempValue,
					ubDPCP_Buffer1);
			}

			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00103,
				ubDPCP_Buffer1, ubTargetLaneCount);
			drm_dp_link_train_channel_eq_delay(mtk_dp->rx_cap);

			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00202,
				ubTempValue, 0x6);
			if (mtk_dp->training_info.bSinkEXTCAP_En) {
				drm_dp_dpcd_read(&mtk_dp->aux, DPCD_0200C,
					ubDPCD200C, 0x3);
				ubTempValue[0] |= ubDPCD200C[0];
				ubTempValue[1] |= ubDPCD200C[1];
				ubTempValue[2] |= ubDPCD200C[2];
			}

			if (!drm_dp_clock_recovery_ok(ubTempValue,
				ubTargetLaneCount)) {
				mtk_dp->training_info.cr_done = false;
				mtk_dp->training_info.eq_done = false;
				break;
			}

			if (drm_dp_channel_eq_ok(ubTempValue,
				ubTargetLaneCount)) {
				mtk_dp->training_info.eq_done = true;
				bPassTPS2_3 = true;
				DPTXMSG("EQ Training Success\n");
				break;
			} else
				DPTXMSG("EQ Training Fail\n");

			if (ubDPCD206 == ubTempValue[0x4])
				ubIterationCount++;
			else
				ubDPCD206 = ubTempValue[0x4];
		}

		mdrv_DPTx_TrainingCheckSwingPre(mtk_dp, ubTargetLaneCount,
			ubTempValue, ubDPCP_Buffer1);
		DPTXMSG("ubTrainRetryTimes = %d, ubIterationCount = %d\n",
			ubTrainRetryTimes, ubIterationCount);

	} while ((ubTrainRetryTimes < DPTX_TRAIN_RETRY_LIMIT) &&
		(ubIterationCount < DPTX_TRAIN_MAX_ITERATION));

	ubTempValue[0] = 0x0;
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00102, ubTempValue, 0x1);
	mhal_DPTx_SetTxTrainingPattern(mtk_dp, 0);

	if (bPassTPS2_3) {
		mtk_dp->training_info.ubLinkRate = ubTargetLinkRate;
		mtk_dp->training_info.ubLinkLaneCount = ubTargetLaneCount;

		mhal_DPTx_SetScramble(mtk_dp, true);

		ubTempValue[0] = ubTargetLaneCount
			| DPTX_AUX_SET_ENAHNCED_FRAME;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00101, ubTempValue, 0x1);
		mhal_DPTx_SetEF_Mode(mtk_dp, ENABLE_DPTX_EF_MODE);

		DPTXMSG("Link Training PASS\n");
		return DPTX_NOERR;
	}

	DPTXMSG("Link Training Fail\n");
	return DPTX_TRANING_FAIL;
}

bool mdrv_DPTx_CheckSinkCap(struct mtk_dp *mtk_dp)
{
	u8 bTempBuffer[0x10];

	if (!mhal_DPTx_GetHPDPinLevel(mtk_dp))
		return false;

	memset(bTempBuffer, 0x0, sizeof(bTempBuffer));

	bTempBuffer[0x0] = 0x1;
	drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00600, bTempBuffer, 0x1);
	mdelay(2);

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00000, bTempBuffer, 0x10);

	mtk_dp->training_info.bSinkEXTCAP_En = (bTempBuffer[0x0E]&BIT7) ?
		true : false;
	if (mtk_dp->training_info.bSinkEXTCAP_En)
		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02200, bTempBuffer, 0x10);

	mtk_dp->training_info.ubDPCD_REV = bTempBuffer[0x0];
	DPTXMSG("SINK DPCD version:0x%x\n", mtk_dp->training_info.ubDPCD_REV);

	memcpy(mtk_dp->rx_cap, bTempBuffer, 0x10);
	mtk_dp->rx_cap[0xe] &= 0x7F;

	if (mtk_dp->training_info.ubDPCD_REV >= 0x14) {
		mdrv_DPTx_FEC_Ready(mtk_dp, FEC_BIT_ERROR_COUNT);
		mdrv_DPTx_DSC_Support(mtk_dp);
	}

	if (!mtk_dp->has_dsc || !mtk_dp->has_fec)
		mtk_dp_enable_4k60(false);
	else
		mtk_dp_enable_4k60(true);

#if !ENABLE_DPTX_FIX_LRLC
	mtk_dp->training_info.ubLinkRate =
		(bTempBuffer[0x1] >= mtk_dp->training_info.ubSysMaxLinkRate) ?
		mtk_dp->training_info.ubSysMaxLinkRate : bTempBuffer[0x1];
	mtk_dp->training_info.ubLinkLaneCount =
		((bTempBuffer[0x2]&0x1F) >= MAX_LANECOUNT) ?
		MAX_LANECOUNT : (bTempBuffer[0x2]&0x1F);
#endif

#if ENABLE_DPTX_FIX_TPS2
	mtk_dp->training_info.bTPS3 = 0;
	mtk_dp->training_info.bTPS4 = 0;
#else
	mtk_dp->training_info.bTPS3 = (bTempBuffer[0x2]&BIT6)>>0x6;
	mtk_dp->training_info.bTPS4 = (bTempBuffer[0x3]&BIT7)>>0x7;
#endif
	mtk_dp->training_info.bDWN_STRM_PORT_PRESENT
			= (bTempBuffer[0x5] & BIT0);

#if (ENABLE_DPTX_SSC_OUTPUT == 0x1)
	if ((bTempBuffer[0x3] & BIT0) == 0x1) {
		mtk_dp->training_info.bSinkSSC_En = true;
		DPTXMSG("SINK SUPPORT SSC!\n");
	} else {
		mtk_dp->training_info.bSinkSSC_En = false;
		DPTXMSG("SINK NOT SUPPORT SSC!\n");
	}
#endif

#if ENABLE_DPTX_SSC_FORCEON
	DPTXMSG("FORCE SSC ON !!!\n");
	mtk_dp->training_info.bSinkSSC_En = true;
#endif

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00021, bTempBuffer, 0x1);
	mtk_dp->training_info.bDPMstCAP = (bTempBuffer[0x0] & BIT0);
	mtk_dp->training_info.bDPMstBranch = false;

	if (mtk_dp->training_info.bDPMstCAP == BIT0) {
		if (mtk_dp->training_info.bDWN_STRM_PORT_PRESENT == 0x1)
			mtk_dp->training_info.bDPMstBranch = true;

		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02003, bTempBuffer, 0x1);

		if (bTempBuffer[0x0] != 0x0)
			drm_dp_dpcd_write(&mtk_dp->aux, DPCD_02003,
				bTempBuffer, 0x1);
	}

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00600, bTempBuffer, 0x1);
	if (bTempBuffer[0x0] != 0x1) {
		bTempBuffer[0x0] = 0x1;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00600, bTempBuffer, 0x1);
	}

	mtk_dp->training_info.ubSinkCountNum = mdrv_DPTx_getSinkCount(mtk_dp);
	//if (mtk_dp->training_info.ubSinkCountNum == 0)
	//	return false;

	if (!mtk_dp->training_info.bDPMstBranch) {
		u8 ubDPCD_201;

		drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00201, &ubDPCD_201, 1);
		if (ubDPCD_201 & BIT1) {
#if (DPTX_AutoTest_ENABLE == 0x1)
			mdrv_DPTx_PHY_AutoTest(mtk_dp, ubDPCD_201);
#endif
		}
	}

	return true;
}

unsigned int force_ch, force_fs, force_len;
unsigned int mdrv_DPTx_getAudioCaps(struct mtk_dp *mtk_dp)
{
	struct cea_sad *sads;
	int sad_count, i, j;
	unsigned int caps = 0;

	if (mtk_dp->edid == NULL) {
		DPTXERR("EDID not found!\n");
		return 0;
	}

	sad_count = drm_edid_to_sad(mtk_dp->edid, &sads);
	if (sad_count <= 0) {
		DPTXMSG("The SADs is NULL\n");
		return 0;
	}

	for (i = 0; i < sad_count; i++) {
		if (sads[i].format == 0x01)	{
			for (j = 0; j < sads[i].channels; j++)
				caps |= ((1 << j) <<
					DP_CAPABILITY_CHANNEL_SFT) &
					(DP_CAPABILITY_CHANNEL_MASK <<
					DP_CAPABILITY_CHANNEL_SFT);

			caps |= (sads[i].freq << DP_CAPABILITY_SAMPLERATE_SFT) &
				(DP_CAPABILITY_SAMPLERATE_MASK <<
				DP_CAPABILITY_SAMPLERATE_SFT);
			caps |= (sads[i].byte2 << DP_CAPABILITY_BITWIDTH_SFT) &
				(DP_CAPABILITY_BITWIDTH_MASK <<
				DP_CAPABILITY_BITWIDTH_SFT);
		}
	}

	DPTXMSG("audio caps:0x%x", caps);
	return caps;
}

bool mdrv_DPTx_TrainingChangeMode(struct mtk_dp *mtk_dp)
{
	mhal_DPTx_PHYD_Reset(mtk_dp);
	mhal_DPTx_ResetSwingtPreEmphasis(mtk_dp);
	mhal_DPTx_SSCOnOffSetting(mtk_dp, mtk_dp->training_info.bSinkSSC_En);

	mdelay(2);
	return true;
}

int mdrv_DPTx_SetTrainingStart(struct mtk_dp *mtk_dp)
{
	u8 ret = DPTX_NOERR;
	u8 ubLaneCount;
	u8 ubLinkRate;
	u8 ubTemp[16];
	u8 ubTrainTimeLimits;
#if !ENABLE_DPTX_FIX_LRLC
	u8 maxLinkRate;
#endif

	if (!mhal_DPTx_GetHPDPinLevel(mtk_dp)) {
		DPTXMSG("Start Training Abort!=> HPD low !\n");

		ubTrainTimeLimits = 6;
		while (ubTrainTimeLimits > 6) {
			if (mhal_DPTx_GetHPDPinLevel(mtk_dp))
				break;

			ubTrainTimeLimits--;
			mdelay(1);
		}

		if (ubTrainTimeLimits == 0)	{
			mtk_dp->training_state = DPTX_NTSTATE_DPIDLE;
			return DPTX_PLUG_OUT;
		}
	}

	if (!mtk_dp->training_info.bDPTxAutoTest_EN) {
		ubTemp[0] = 0x1;
		drm_dp_dpcd_write(&mtk_dp->aux, DPCD_00600, ubTemp, 0x1);

		ubLinkRate = mtk_dp->rx_cap[1];
		ubLaneCount = mtk_dp->rx_cap[2] & 0x1F;
		DPTXMSG("RX support ubLinkRate = 0x%x,ubLaneCount = %x",
			ubLinkRate, ubLaneCount);

#if !ENABLE_DPTX_FIX_LRLC
		mtk_dp->training_info.ubLinkRate =
			(ubLinkRate >= mtk_dp->training_info.ubSysMaxLinkRate) ?
			mtk_dp->training_info.ubSysMaxLinkRate : ubLinkRate;
		mtk_dp->training_info.ubLinkLaneCount =
			(ubLaneCount >= MAX_LANECOUNT) ?
			MAX_LANECOUNT : ubLaneCount;
#endif
	}

	ubLinkRate = mtk_dp->training_info.ubLinkRate;
	ubLaneCount = mtk_dp->training_info.ubLinkLaneCount;

	switch (ubLinkRate) {
	case DP_LINKRATE_RBR:
	case DP_LINKRATE_HBR:
	case DP_LINKRATE_HBR2:
	case DP_LINKRATE_HBR25:
	case DP_LINKRATE_HBR3:
		break;
	default:
		mtk_dp->training_info.ubLinkRate = DP_LINKRATE_HBR3;
		break;
	};

#if !ENABLE_DPTX_FIX_LRLC
	maxLinkRate = ubLinkRate;
	ubTrainTimeLimits = 0x6;
#endif
	do {
		DPTXMSG("LinkRate:0x%x, LaneCount:%x", ubLinkRate, ubLaneCount);

		mtk_dp->training_info.cr_done = false;
		mtk_dp->training_info.eq_done = false;

		mdrv_DPTx_TrainingChangeMode(mtk_dp);
		ret = mdrv_DPTx_TrainingFlow(mtk_dp, ubLinkRate, ubLaneCount);
		if (ret == DPTX_PLUG_OUT || ret == DPTX_RETRANING)
			return ret;

		if (!mtk_dp->training_info.cr_done) {
#if !ENABLE_DPTX_FIX_LRLC
			switch (ubLinkRate) {
			case DP_LINKRATE_RBR:
				ubLaneCount = ubLaneCount/2;
				ubLinkRate = maxLinkRate;
				if (ubLaneCount == 0x0) {
					mtk_dp->training_state
						= DPTX_NTSTATE_DPIDLE;
					return DPTX_TRANING_FAIL;
				}
				break;
			case DP_LINKRATE_HBR:
				ubLinkRate = DP_LINKRATE_RBR;
				break;
			case DP_LINKRATE_HBR2:
				ubLinkRate = DP_LINKRATE_HBR;
				break;
			case DP_LINKRATE_HBR3:
				ubLinkRate = DP_LINKRATE_HBR2;
				break;
			default:
				return DPTX_TRANING_FAIL;
			};
#endif
			ubTrainTimeLimits--;
		} else if (!mtk_dp->training_info.eq_done) {
#if !ENABLE_DPTX_FIX_LRLC
			if (ubLaneCount == DP_LANECOUNT_4)
				ubLaneCount = DP_LANECOUNT_2;
			else if (ubLaneCount == DP_LANECOUNT_2)
				ubLaneCount = DP_LANECOUNT_1;
			else
				return DPTX_TRANING_FAIL;
#endif
			ubTrainTimeLimits--;
		} else
			return DPTX_NOERR;

	} while (ubTrainTimeLimits > 0);

	return DPTX_TRANING_FAIL;
}

int mdrv_DPTx_Training_Handler(struct mtk_dp *mtk_dp)
{
	int ret = DPTX_NOERR;

	if (!mtk_dp->training_info.bCablePlugIn)
		return DPTX_PLUG_OUT;

	if (mtk_dp->training_state == DPTX_NTSTATE_NORMAL)
		return ret;

	if (mtk_dp->training_state_pre != mtk_dp->training_state) {
		mdrv_DPTx_Print_TrainingState(mtk_dp->training_state);

		mtk_dp->training_state_pre = mtk_dp->training_state;
	}

	switch (mtk_dp->training_state) {
	case DPTX_NTSTATE_STARTUP:
		mtk_dp->training_state = DPTX_NTSTATE_CHECKCAP;
		break;

	case DPTX_NTSTATE_CHECKCAP:
		if (mdrv_DPTx_CheckSinkCap(mtk_dp)) {
			mtk_dp->training_info.ucCheckCapTimes = 0;
			mtk_dp->training_state = DPTX_NTSTATE_CHECKEDID;
		} else {
			BYTE uaCheckTimes = 0;

			mtk_dp->training_info.ucCheckCapTimes++;
			uaCheckTimes = mtk_dp->training_info.ucCheckCapTimes;

			if (uaCheckTimes > DPTX_CheckSinkCap_TimeOutCnt) {
				mtk_dp->training_info.ucCheckCapTimes = 0;
				DPTXMSG("CheckCap Fail %d times",
					DPTX_CheckSinkCap_TimeOutCnt);
				mtk_dp->training_state = DPTX_NTSTATE_DPIDLE;
				ret = DPTX_TIMEOUT;
			} else
				DPTXMSG("CheckCap Fail %d times", uaCheckTimes);
		}
		break;

	case DPTX_NTSTATE_CHECKEDID:
		mtk_dp->edid = mtk_dp_handle_edid(mtk_dp);
		if (mtk_dp->edid) {
			DPTXMSG("READ EDID done!\n");
			if (mtk_dp_debug_get()) {
				u8 *raw_edid = (u8 *)mtk_dp->edid;

				DPTXMSG("Raw EDID:\n");
				print_hex_dump(KERN_NOTICE,
						"\t", DUMP_PREFIX_NONE, 16, 1,
						raw_edid, EDID_LENGTH, false);
				if ((raw_edid[0x7E] & 0x01) == 0x01) {
					print_hex_dump(KERN_NOTICE,
						"\t", DUMP_PREFIX_NONE, 16, 1,
						(raw_edid + 128), EDID_LENGTH,
						false);
				}
			}

			mtk_dp->info.audio_caps
				= mdrv_DPTx_getAudioCaps(mtk_dp);
		} else
			DPTXMSG("Read EDID Fail!\n");

		mtk_dp->training_state = DPTX_NTSTATE_TRAINING_PRE;
		break;

	case DPTX_NTSTATE_TRAINING_PRE:
		mtk_dp->training_state = DPTX_NTSTATE_TRAINING;
		break;

	case DPTX_NTSTATE_TRAINING:
		ret = mdrv_DPTx_SetTrainingStart(mtk_dp);
		if (ret == DPTX_NOERR) {
			mdrv_DPTx_VideoMute(mtk_dp, true);
			mdrv_DPTx_AudioMute(mtk_dp, true);
			mtk_dp->training_state = DPTX_NTSTATE_CHECKTIMING;
			mtk_dp->dp_ready = true;
			mhal_DPTx_EnableFEC(mtk_dp, mtk_dp->has_fec);
		} else if (ret == DPTX_RETRANING) {
			ret = DPTX_NOERR;
		} else
			DPTXERR("Handle Training Fail 6 times\n");
		break;

	case DPTX_NTSTATE_CHECKTIMING:
		mtk_dp->training_state = DPTX_NTSTATE_NORMAL;

		if (mtk_dp->training_info.ubSinkCountNum == 0) {
			DPTXMSG("no sink count, skip uevent\n");
			break;
		}

		if (mtk_dp->bUeventToHwc) {
			mtk_dp_hotplug_uevent(1);
			mtk_dp->bUeventToHwc = false;
		} else
			DPTXMSG("Skip Uevent(1)\n");

		break;
	case DPTX_NTSTATE_NORMAL:
		break;
	case DPTX_NTSTATE_POWERSAVE:
		break;
	case DPTX_NTSTATE_DPIDLE:
		break;
	default:
		break;
	}

	return ret;
}

#ifdef DPTX_HDCP_ENABLE
void mdrv_DPTx_reAuthentication(struct mtk_dp *mtk_dp)
{
	if (!mtk_dp->training_info.bCablePlugIn || !mtk_dp->dp_ready)
		return;

	queue_work(mtk_dp->dptx_wq, &mtk_dp->hdcp_work);
}

void mdrv_DPTx_CheckHDCPVersion(struct mtk_dp *mtk_dp, bool only_hdcp1x)
{
	if (g_hdcp_on) {
		if (!only_hdcp1x && mdrv_DPTx_HDCP2_Support(mtk_dp))
			return;

		if (mdrv_DPTx_HDCP1x_Support(mtk_dp))
			return;
	} else
		DPTXMSG("Not enable HDCP Function!\n");

	if (tee_addDevice(HDCP_NONE) != RET_SUCCESS)
		mtk_dp->info.bAuthStatus = AUTH_FAIL;
}

static void mdrv_DPTx_hdcp_handle(struct work_struct *data)
{
	struct mtk_dp *mtk_dp = container_of(data, struct mtk_dp, hdcp_work);

	if (!mtk_dp->training_info.bCablePlugIn || !mtk_dp->dp_ready)
		return;

	if (mtk_dp->info.bAuthStatus == AUTH_ZERO) {
		mdrv_DPTx_CheckHDCPVersion(mtk_dp, false);
		if (mtk_dp->info.hdcp2_info.bEnable)
			mdrv_DPTx_HDCP2_SetStartAuth(mtk_dp, true);
		else if (mtk_dp->info.hdcp1x_info.bEnable)
			mdrv_DPTx_HDCP1X_SetStartAuth(mtk_dp, true);
	}

	if (mtk_dp->info.hdcp2_info.bEnable) {
		HDCPTx_Hdcp2FSM(mtk_dp);

		if (mtk_dp->info.bAuthStatus == AUTH_FAIL) {
			tee_removeDevice();

			mdrv_DPTx_CheckHDCPVersion(mtk_dp, true);
			if (mtk_dp->info.hdcp1x_info.bEnable) {
				mtk_dp->info.hdcp2_info.bEnable = false;
				mdrv_DPTx_HDCP1X_SetStartAuth(mtk_dp, true);
			}
		}
	}

	if (mtk_dp->info.hdcp1x_info.bEnable)
		mdrv_DPTx_HDCP1X_FSM(mtk_dp);

	if ((mtk_dp->info.hdcp1x_info.bEnable
			|| mtk_dp->info.hdcp2_info.bEnable)
		&& (mtk_dp->info.bAuthStatus != AUTH_FAIL)
		&& (mtk_dp->info.bAuthStatus != AUTH_PASS))
		queue_work(mtk_dp->dptx_wq, &mtk_dp->hdcp_work);
}
#else
static void mdrv_DPTx_hdcp_handle(struct work_struct *data)
{
	DPTXMSG("No Support HDCP function\n");
}
#endif

bool mdrv_DPTx_done(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->state != DPTXSTATE_NORMAL)
		return false;

	if (mtk_dp->training_state != DPTX_NTSTATE_NORMAL)
		return false;

	return true;
}

int mdrv_DPTx_Handle(struct mtk_dp *mtk_dp)
{
	int ret = DPTX_NOERR;

	if (!mtk_dp->training_info.bCablePlugIn)
		return DPTX_PLUG_OUT;

	if (mtk_dp->state != mtk_dp->state_pre) {
		DPTXMSG("m_DPTXState %d, m_DPTXStateTemp %d\n",
			mtk_dp->state, mtk_dp->state_pre);
		mtk_dp->state_pre = mtk_dp->state;
	}

	switch (mtk_dp->state) {
	case DPTXSTATE_INITIAL:
		mdrv_DPTx_VideoMute(mtk_dp, true);
		mdrv_DPTx_AudioMute(mtk_dp, true);
		mtk_dp->state = DPTXSTATE_IDLE;
		break;

	case DPTXSTATE_IDLE:
		if (mtk_dp->training_state == DPTX_NTSTATE_NORMAL)
			mtk_dp->state = DPTXSTATE_PREPARE;
		break;

	case DPTXSTATE_PREPARE:
		if (mtk_dp->video_enable) {
			mtk_dp_video_config(mtk_dp);
			mdrv_DPTx_Video_Enable(mtk_dp, true);
		}

		if (mtk_dp->audio_enable && (mtk_dp->info.audio_caps != 0)) {
			mdrv_DPTx_I2S_Audio_Config(mtk_dp);
			mdrv_DPTx_I2S_Audio_Enable(mtk_dp, true);
		}

		mtk_dp->state = DPTXSTATE_NORMAL;
		break;

	case DPTXSTATE_NORMAL:
		if (mtk_dp->training_state != DPTX_NTSTATE_NORMAL) {
			mdrv_DPTx_VideoMute(mtk_dp, true);
			mdrv_DPTx_AudioMute(mtk_dp, true);
			mdrv_DPTx_StopSentSDP(mtk_dp);
			mtk_dp->state = DPTXSTATE_IDLE;
			DPTXMSG("DPTX Link Status Change!\n");
		}
		break;

	default:
		break;
	}

	return ret;
}

void mdrv_DPTx_HPD_HandleInISR(struct mtk_dp *mtk_dp)
{
	bool ubCurrentHPD = mhal_DPTx_GetHPDPinLevel(mtk_dp);

	if (mtk_dp->training_info.usPHY_STS == HPD_INITIAL_STATE)
		return;

	if ((mtk_dp->training_info.usPHY_STS & (HPD_CONNECT|HPD_DISCONNECT))
		== (HPD_CONNECT|HPD_DISCONNECT)) {
		if (ubCurrentHPD)
			mtk_dp->training_info.usPHY_STS &= ~HPD_DISCONNECT;
		else
			mtk_dp->training_info.usPHY_STS &= ~HPD_CONNECT;
	}

	if ((mtk_dp->training_info.usPHY_STS & (HPD_INT_EVNET|HPD_DISCONNECT))
		== (HPD_INT_EVNET|HPD_DISCONNECT)) {
		if (ubCurrentHPD)
			mtk_dp->training_info.usPHY_STS &= ~HPD_DISCONNECT;
	}

	if (mtk_dp->training_info.bCablePlugIn)
		mtk_dp->training_info.usPHY_STS &= ~HPD_CONNECT;
	else
		mtk_dp->training_info.usPHY_STS &= ~HPD_DISCONNECT;


	if (mtk_dp->training_info.usPHY_STS & HPD_CONNECT) {
		mtk_dp->training_info.usPHY_STS &= ~HPD_CONNECT;
		mtk_dp->training_info.bCablePlugIn = true;
		mtk_dp->training_info.bCableStateChange = true;

		DPTXMSG(" HPD_CON_ISR\n");
	}

	if (mtk_dp->training_info.usPHY_STS & HPD_DISCONNECT) {
		mtk_dp->training_info.usPHY_STS &= ~HPD_DISCONNECT;

		mtk_dp->training_info.bCablePlugIn = false;
		mtk_dp->training_info.bCableStateChange = true;

		DPTXMSG("HPD_DISCON_ISR\n");
	}
}

void mdrv_DPTx_USBC_HPD_Event(u16 ubSWStatus)
{
	struct mtk_dp *mtk_dp = g_mtk_dp;

	mtk_dp->training_info.usPHY_STS |= ubSWStatus;
	DPTXMSG("SW status = 0x%x\n", ubSWStatus);

	mdrv_DPTx_HPD_HandleInISR(mtk_dp);

	if (mtk_dp->training_info.bCableStateChange
		|| ubSWStatus == HPD_INT_EVNET)
		queue_work(mtk_dp->dptx_wq, &mtk_dp->dptx_work);
}

void mdrv_DPTx_HPD_ISREvent(struct mtk_dp *mtk_dp)
{
	u8 ubIsrEnable = 0xF1;
	u8 ubHWStatus_undefine = mhal_DPTx_GetHPDIRQStatus(mtk_dp);
	u8 ubHWStatus = ubHWStatus_undefine&(~ubIsrEnable);

	mtk_dp->training_info.usPHY_STS |= ubHWStatus;
	DPTXMSG("HW status = 0x%x\n", ubHWStatus);

	mdrv_DPTx_HPD_HandleInISR(mtk_dp);

	if (ubHWStatus)
		mhal_DPTx_HPDInterruptClr(mtk_dp, ubHWStatus);

	if (mtk_dp->training_info.bCableStateChange
		|| ubHWStatus == HPD_INT_EVNET)
		queue_work(mtk_dp->dptx_wq, &mtk_dp->dptx_work);
}

void mdrv_DPTx_ISR(struct mtk_dp *mtk_dp)
{
	mhal_DPTx_ISR(mtk_dp);
}

void mdrv_DPTx_InitPort(struct mtk_dp *mtk_dp)
{
	mhal_DPTx_PHY_SetIdlePattern(mtk_dp, true);
	mdrv_DPTx_InitVariable(mtk_dp);

	mhal_DPTx_InitialSetting(mtk_dp);
	mhal_DPTx_AuxSetting(mtk_dp);
	mhal_DPTx_DigitalSetting(mtk_dp);
	mhal_DPTx_AnalogPowerOnOff(mtk_dp, true);
	mhal_DPTx_PHYSetting(mtk_dp);
	mhal_DPTx_HPDDetectSetting(mtk_dp);

	mhal_DPTx_DigitalSwReset(mtk_dp);
	mhal_DPTx_Set_Efuse_Value(mtk_dp);
}

void mdrv_DPTx_Video_Enable(struct mtk_dp *mtk_dp, bool bEnable)
{
	DPTXMSG("Output Video %s!\n", bEnable ? "enable" : "disable");

	if (bEnable) {
		mdrv_DPTx_SetDPTXOut(mtk_dp);
		mdrv_DPTx_VideoMute(mtk_dp, false);
		mhal_DPTx_Verify_Clock(mtk_dp);
	} else
		mdrv_DPTx_VideoMute(mtk_dp, true);
}

void mdrv_DPTx_Set_Color_Format(struct mtk_dp *mtk_dp, u8 ucColorFormat)
{
	DPTXMSG("Set Color Format = 0x%x\n", ucColorFormat);

	mtk_dp->info.format = ucColorFormat;
	mhal_DPTx_SetColorFormat(mtk_dp, ucColorFormat);
}

void mdrv_DPTx_Set_Color_Depth(struct mtk_dp *mtk_dp, u8 ucColorDepth)
{
	DPTXMSG("Set Color Depth = %d (1~4=6/8/10/12bpp)\n", ucColorDepth + 1);

	mtk_dp->info.depth = ucColorDepth;
	mhal_DPTx_SetColorDepth(mtk_dp, ucColorDepth);
}

void mdrv_DPTx_I2S_Audio_Enable(struct mtk_dp *mtk_dp, bool bEnable)
{
	if (bEnable) {
		mdrv_DPTx_AudioMute(mtk_dp, false);
		DPTXMSG("I2S Audio Enable!\n");
	} else {
		mdrv_DPTx_AudioMute(mtk_dp, true);
		DPTXMSG("I2S Audio Disable!\n");
	}
}

void mdrv_DPTx_I2S_Audio_Set_MDiv(struct mtk_dp *mtk_dp, u8 ucDiv)
{
	char bTable[7][5] = {"X2", "X4", "X8", "N/A", "/2", "/4", "/8"};

	DPTXMSG("I2S Set Audio M Divider = %s\n", bTable[ucDiv-1]);
	mhal_DPTx_Audio_M_Divider_Setting(mtk_dp, ucDiv);
}

void mdrv_DPTx_I2S_Audio_Config(struct mtk_dp *mtk_dp)
{
	u8 ucChannel, ucFs, ucWordlength;
	unsigned int tmp = mtk_dp->info.audio_config;

	if (!mtk_dp->dp_ready) {
		DPTXERR("%s, DP is not ready!\n", __func__);
		return;
	}

	if (fakecablein) {
		ucChannel = BIT(force_ch);
		ucFs = BIT(force_fs);
		ucWordlength = BIT(force_len);
	} else {
		ucChannel = (tmp >> DP_CAPABILITY_CHANNEL_SFT)
			& DP_CAPABILITY_CHANNEL_MASK;
		ucFs = (tmp >> DP_CAPABILITY_SAMPLERATE_SFT)
			& DP_CAPABILITY_SAMPLERATE_MASK;
		ucWordlength = (tmp >> DP_CAPABILITY_BITWIDTH_SFT)
				& DP_CAPABILITY_BITWIDTH_MASK;
	}

	switch (ucChannel) {
	case DP_CHANNEL_2:
		ucChannel = 2;
		break;
	case DP_CHANNEL_8:
		ucChannel = 8;
		break;
	default:
		ucChannel = 2;
		break;
	}

	switch (ucFs) {
	case DP_SAMPLERATE_32:
		ucFs = FS_32K;
		break;
	case DP_SAMPLERATE_44:
		ucFs = FS_44K;
		break;
	case DP_SAMPLERATE_48:
		ucFs = FS_48K;
		break;
	case DP_SAMPLERATE_96:
		ucFs = FS_96K;
		break;
	case DP_SAMPLERATE_192:
		ucFs = FS_192K;
		break;
	default:
		ucFs = FS_48K;
		break;
	}

	switch (ucWordlength) {
	case DP_BITWIDTH_16:
		ucWordlength = WL_16bit;
		break;
	case DP_BITWIDTH_20:
		ucWordlength = WL_20bit;
		break;
	case DP_BITWIDTH_24:
		ucWordlength = WL_24bit;
		break;
	default:
		ucWordlength = WL_24bit;
		break;
	}

	mdrv_DPTx_I2S_Audio_SDP_Channel_Setting(mtk_dp, ucChannel,
		ucFs, ucWordlength);
	mdrv_DPTx_I2S_Audio_Ch_Status_Set(mtk_dp, ucChannel,
		ucFs, ucWordlength);

	mhal_DPTx_Audio_PG_EN(mtk_dp, ucChannel, ucFs, false);
	mdrv_DPTx_I2S_Audio_Set_MDiv(mtk_dp, 5);
}

void mdrv_DPTx_I2S_Audio_SDP_Channel_Setting(struct mtk_dp *mtk_dp,
	u8 ucChannel, u8 ucFs, u8 ucWordlength)
{
	u8 SDP_DB[32] = {0};
	u8 SDP_HB[4] = {0};

	SDP_HB[1] = DP_SPEC_SDPTYP_AINFO;
	SDP_HB[2] = 0x1B;
	SDP_HB[3] = 0x48;

	SDP_DB[0x0] = 0x10 | (ucChannel-1); //L-PCM[7:4], channel-1[2:0]
	SDP_DB[0x1] = ucFs << 2 | ucWordlength; // fs[4:2], len[1:0]
	SDP_DB[0x2] = 0x0;

	if (ucChannel == 8)
		SDP_DB[0x3] = 0x13;
	else
		SDP_DB[0x3] = 0x00;

	mhal_DPTx_Audio_SDP_Setting(mtk_dp, ucChannel);
	DPTXMSG("I2S Set Audio Channel = %d\n", ucChannel);
	mdrv_DPTx_SPKG_SDP(mtk_dp, true, DPTx_SDPTYP_AUI, SDP_HB, SDP_DB);
}

void mdrv_DPTx_I2S_Audio_Ch_Status_Set(struct mtk_dp *mtk_dp, u8 ucChannel,
	u8 ucFs, u8 ucWordlength)
{
	mhal_DPTx_Audio_Ch_Status_Set(mtk_dp, ucChannel, ucFs, ucWordlength);
}

void mdrv_DPTx_DSC_SetParam(struct mtk_dp *mtk_dp, u8 slice_num, u16 chunk_num)
{
	u8 r8, r16;
	u8 q16[16] = {0x6, 0x01, 0x01, 0x03, 0x03, 0x05, 0x05, 0x07,
		0x07, 0x00, 0x00, 0x02, 0x02, 0x04, 0x04, 0x06};
	u8 q8[8] = {0x6, 0x01, 0x03, 0x05, 0x07, 0x00, 0x02, 0x04};
	u8 hde_last_num, hde_num_even;

	DPTXMSG("lane count = %d\n", mtk_dp->training_info.ubLinkLaneCount);
	if (mtk_dp->training_info.ubLinkLaneCount == DP_LANECOUNT_2) {
		if (chunk_num % 2)
			//r16 = (int) ceil((chunk_num + 1 + 2) *
				//slice_num / 3) % 16;
			r16 = ((chunk_num + 1 + 2) * slice_num / 3) % 16;
		else
			//r16 = (int) ceil((chunk_num + 2) *
				//slice_num / 3) % 16;
			r16 = ((chunk_num + 2) * slice_num / 3) % 16;
		DPTXMSG("r16 = %d\n", r16);
		//r16 = 1; //test for 1080p
		hde_last_num = (q16[r16] & (BIT1|BIT2)) >> 1;
		hde_num_even = q16[r16] & BIT0;
	} else {
		//r8 = (int) ceil((chunk_num + 1) * slice_num / 3) % 8;
		r8 = ((chunk_num + 1) * slice_num / 3) % 8;
		DPTXMSG("r8 = %d\n", r8);
		//r8 = 1; //test for 1080p
		hde_last_num = (q8[r8] & (BIT1|BIT2)) >> 1;
		hde_num_even = q8[r8] & BIT0;
	}

	mhal_DPTx_SetChunkSize(mtk_dp, slice_num-1, chunk_num, chunk_num%12,
		mtk_dp->training_info.ubLinkLaneCount,
		hde_last_num, hde_num_even);

}

void mdrv_DPTx_DSC_SetPPS(struct mtk_dp *mtk_dp, u8 *PPS, bool enable)
{
	u8 HB[4] = {0x0, 0x10, 0x7F, 0x0};

	mdrv_DPTx_SPKG_SDP(mtk_dp, enable, DPTx_SDPTYP_PPS0, HB, PPS +  0);
	mdrv_DPTx_SPKG_SDP(mtk_dp, enable, DPTx_SDPTYP_PPS1, HB, PPS + 32);
	mdrv_DPTx_SPKG_SDP(mtk_dp, enable, DPTx_SDPTYP_PPS2, HB, PPS + 64);
	mdrv_DPTx_SPKG_SDP(mtk_dp, enable, DPTx_SDPTYP_PPS3, HB, PPS + 96);
}

void mdrv_DPTx_DSC_Support(struct mtk_dp *mtk_dp)
{
#if DPTX_SUPPORT_DSC
	u8 Data[3];

	drm_dp_dpcd_read(&mtk_dp->aux, 0x60, Data, 1);
	if (Data[0] & BIT0)
		mtk_dp->has_dsc = true;
	else
		mtk_dp->has_dsc = false;

	DPTXMSG("Sink has_dsc = %d\n", mtk_dp->has_dsc);
#endif
}

void mdrv_DPTx_FEC_Ready(struct mtk_dp *mtk_dp, u8 err_cnt_sel)
{
	u8 i, Data[3];

	drm_dp_dpcd_read(&mtk_dp->aux, 0x90, Data, 0x1);

	/* FEC error count select 120[3:1]:         *
	 * 000b: FEC_ERROR_COUNT_DIS                *
	 * 001b: UNCORRECTED_BLOCK_ERROR_COUNT      *
	 * 010b: CORRECTED_BLOCK_ERROR_COUNT        *
	 * 011b: BIT_ERROR_COUNT                    *
	 * 100b: PARITY_BLOCK_ERROR_COUNT           *
	 * 101b: PARITY_BIT_ERROR_COUNT             *
	 */
	if (Data[0] & BIT0) {
		mtk_dp->has_fec = true;
		Data[0] = (err_cnt_sel << 1) | 0x1;     //FEC Ready
		drm_dp_dpcd_write(&mtk_dp->aux, 0x120, Data, 0x1);
		drm_dp_dpcd_read(&mtk_dp->aux, 0x280, Data, 0x3);
		for (i = 0; i < 3; i++)
			DPTXDBG("FEC status & error Count: 0x%x\n", Data[i]);
	}

	DPTXMSG("SINK has fec (%d)\n", mtk_dp->has_fec);
}

DWORD getTimeDiff(DWORD dwPreTime)
{
	DWORD dwPostTime = getSystemTime();

	if (dwPreTime > dwPostTime)
		return ((1000000 - dwPreTime) + dwPostTime);
	else
		return (dwPostTime - dwPreTime);
}

DWORD getSystemTime(void)
{
	DWORD tms = (DWORD)((sched_clock() / 1000000) % 1000000);
	return tms;
}

static void mdrv_DPTx_main_handle(struct work_struct *data)
{
	struct mtk_dp *mtk_dp = container_of(data, struct mtk_dp, dptx_work);
	unsigned long long starttime = sched_clock();

	do {
		if (abs(sched_clock() - starttime) > 5000000000ULL) {
			DPTXERR("Handle time over 5s\n");
			break;
		}

		if (mdrv_DPTx_HPD_HandleInThread(mtk_dp) != DPTX_NOERR)
			break;

		if (mdrv_DPTx_Training_Handler(mtk_dp) != DPTX_NOERR)
			break;

		if (mdrv_DPTx_Handle(mtk_dp) != DPTX_NOERR)
			break;
	} while (!mdrv_DPTx_done(mtk_dp));
}

u8 PPS_4k60[128] = {
	0x12, 0x00, 0x00, 0x8d, 0x30, 0x80, 0x08, 0x70, 0x0f, 0x00, 0x00, 0x08,
	0x07, 0x80, 0x07, 0x80,	0x02, 0x00, 0x04, 0xc0, 0x00, 0x20, 0x01, 0x1e,
	0x00, 0x1a, 0x00, 0x0c, 0x0d, 0xb7, 0x03, 0x94,	0x18, 0x00, 0x10, 0xf0,
	0x03, 0x0c, 0x20, 0x00, 0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e, 0x01, 0x02,
	0x01, 0x00, 0x09, 0x40,	0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
	0x1a, 0x38, 0x1a, 0x78, 0x22, 0xb6, 0x2a, 0xb6, 0x2a, 0xf6, 0x2a, 0xf4,
	0x43, 0x34, 0x63, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


void mtk_dp_video_config(struct mtk_dp *mtk_dp)
{
	struct DPTX_TIMING_PARAMETER *DPTX_TBL = &mtk_dp->info.DPTX_OUTBL;
	u32 mvid = 0;
	bool overwrite = false;

	if (!mtk_dp->dp_ready) {
		DPTXERR("%s, DP is not ready!\n", __func__);
		return;
	}

	if (mtk_dp->info.resolution >= SINK_MAX) {
		DPTXERR("DPTX doesn't support this resolution(%d)!\n",
			mtk_dp->info.resolution);
		return;
	}

	if (fakecablein) {
		if (mtk_dp->info.resolution == SINK_1280_720) {
			// patch for LLCTS 4.4.4.5
			switch (mtk_dp->training_info.ubLinkRate) {
			case DP_LINKRATE_RBR:
				mvid = 0x3AAB;
				break;
			case DP_LINKRATE_HBR:
				mvid = 0x2333;
				break;
			case DP_LINKRATE_HBR2:
				mvid = 0x1199;
				break;
			case DP_LINKRATE_HBR3:
				mvid = 0xBBB;
				break;
			}
			overwrite = true;
		}
		mtk_dp->info.depth = fakebpc;
	}

	switch (mtk_dp->info.resolution) {
	case SINK_7680_4320:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 8040; DPTX_TBL->Hbp = 240; DPTX_TBL->Hsw = 96;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 24; DPTX_TBL->Hde = 7680;
		DPTX_TBL->Vtt = 4381; DPTX_TBL->Vbp = 6; DPTX_TBL->Vsw = 8;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 47; DPTX_TBL->Vde = 4320;
		break;
	case SINK_3840_2160:
		DPTX_TBL->FrameRate = 60;
		mtk_dp->dsc_enable = true;
		DPTX_TBL->Htt = 4400; DPTX_TBL->Hbp = 296; DPTX_TBL->Hsw = 88;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 176; DPTX_TBL->Hde = 3840;
		DPTX_TBL->Vtt = 2250; DPTX_TBL->Vbp = 72; DPTX_TBL->Vsw = 10;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 8; DPTX_TBL->Vde = 2160;
		break;
	case SINK_3840_2160_30:
		DPTX_TBL->FrameRate = 30;
		DPTX_TBL->Htt = 4400; DPTX_TBL->Hbp = 296; DPTX_TBL->Hsw = 88;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 176; DPTX_TBL->Hde = 3840;
		DPTX_TBL->Vtt = 2250; DPTX_TBL->Vbp = 72; DPTX_TBL->Vsw = 10;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 8; DPTX_TBL->Vde = 2160;
		break;
	case SINK_2560_1600:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 2720; DPTX_TBL->Hbp = 80; DPTX_TBL->Hsw = 32;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 48; DPTX_TBL->Hde = 2560;
		DPTX_TBL->Vtt = 1646; DPTX_TBL->Vbp = 3; DPTX_TBL->Vsw = 6;
		DPTX_TBL->bVsp = 1; DPTX_TBL->Vfp = 37; DPTX_TBL->Vde = 1600;
		break;
	case SINK_1920_1440:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 2600; DPTX_TBL->Hbp = 344; DPTX_TBL->Hsw = 208;
		DPTX_TBL->bHsp = 1; DPTX_TBL->Hfp = 128; DPTX_TBL->Hde = 1920;
		DPTX_TBL->Vtt = 1500; DPTX_TBL->Vbp = 56; DPTX_TBL->Vsw = 3;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 1; DPTX_TBL->Vde = 1440;
		break;
	case SINK_1920_1200:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 2080; DPTX_TBL->Hbp = 80; DPTX_TBL->Hsw = 32;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 48; DPTX_TBL->Hde = 1920;
		DPTX_TBL->Vtt = 1235; DPTX_TBL->Vbp = 26; DPTX_TBL->Vsw = 6;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 3; DPTX_TBL->Vde = 1200;
		break;
	case SINK_1920_1080:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 2200; DPTX_TBL->Hbp = 148; DPTX_TBL->Hsw = 44;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 88; DPTX_TBL->Hde = 1920;
		DPTX_TBL->Vtt = 1125; DPTX_TBL->Vbp = 36; DPTX_TBL->Vsw = 5;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 4; DPTX_TBL->Vde = 1080;
		break;
	case SINK_1080_2460:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1172; DPTX_TBL->Hbp = 30; DPTX_TBL->Hsw = 32;
		DPTX_TBL->bHsp = 1; DPTX_TBL->Hfp = 30; DPTX_TBL->Hde = 1080;
		DPTX_TBL->Vtt = 2476; DPTX_TBL->Vbp = 5; DPTX_TBL->Vsw = 2;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 9; DPTX_TBL->Vde = 2460;
		break;
	case SINK_1280_1024:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1560; DPTX_TBL->Hbp = 148; DPTX_TBL->Hsw = 44;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 88; DPTX_TBL->Hde = 1280;
		DPTX_TBL->Vtt = 1069; DPTX_TBL->Vbp = 36; DPTX_TBL->Vsw = 5;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 4; DPTX_TBL->Vde = 1024;
		break;
	case SINK_1280_960:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1800; DPTX_TBL->Hbp = 312; DPTX_TBL->Hsw = 112;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 96; DPTX_TBL->Hde = 1280;
		DPTX_TBL->Vtt = 1000; DPTX_TBL->Vbp = 36; DPTX_TBL->Vsw = 3;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 1; DPTX_TBL->Vde = 960;
		break;
	case SINK_1280_720:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1650; DPTX_TBL->Hbp = 220; DPTX_TBL->Hsw = 40;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 110; DPTX_TBL->Hde = 1280;
		DPTX_TBL->Vtt = 750; DPTX_TBL->Vbp = 20; DPTX_TBL->Vsw = 5;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 5; DPTX_TBL->Vde = 720;
		break;
	case SINK_800_600:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 1056; DPTX_TBL->Hbp = 88; DPTX_TBL->Hsw = 128;
		DPTX_TBL->bHsp = 0; DPTX_TBL->Hfp = 40; DPTX_TBL->Hde = 800;
		DPTX_TBL->Vtt = 628; DPTX_TBL->Vbp = 23; DPTX_TBL->Vsw = 4;
		DPTX_TBL->bVsp = 0; DPTX_TBL->Vfp = 16; DPTX_TBL->Vde = 600;
		break;
	case SINK_640_480:
	default:
		DPTX_TBL->FrameRate = 60;
		DPTX_TBL->Htt = 800; DPTX_TBL->Hbp = 48; DPTX_TBL->Hsw = 96;
		DPTX_TBL->bHsp = 1; DPTX_TBL->Hfp = 16; DPTX_TBL->Hde = 640;
		DPTX_TBL->Vtt = 525; DPTX_TBL->Vbp = 33; DPTX_TBL->Vsw = 2;
		DPTX_TBL->bVsp = 1; DPTX_TBL->Vfp = 10; DPTX_TBL->Vde = 480;
		break;
	}

	if (mtk_dp->info.resolution == SINK_3840_2160) {
		// patch for 4k@60 with DSC 3 times compress
		switch (mtk_dp->training_info.ubLinkRate) {
		case DP_LINKRATE_HBR3:
			mvid = 0x5DDE;
			break;
		case DP_LINKRATE_HBR2:
			mvid = 0x8CCD;
			break;
		}
		overwrite = true;
	}

	mhal_DPTx_OverWrite_MN(mtk_dp, overwrite, mvid, 0x8000);

	if (mtk_dp->has_dsc) {
		uint8_t Data[1];

		Data[0] = (u8) mtk_dp->dsc_enable;
		drm_dp_dpcd_write(&mtk_dp->aux, 0x160, Data, 0x1);
	}

	//interlace not support
	DPTX_TBL->Video_ip_mode = DPTX_VIDEO_PROGRESSIVE;
	mhal_DPTx_SetMSA(mtk_dp);

	mdrv_DPTx_Set_MISC(mtk_dp);
	if (mtk_dp->info.bPatternGen)
		mdrv_DPTx_PatternGenTypeSel(mtk_dp,
			DPTX_PG_HORIZONTAL_COLOR_BAR,
			DPTX_PG_PURECOLOR_BLUE,
			0xFFF,
			DPTX_PG_LOCATION_ALL);

	if (!mtk_dp->dsc_enable) {
		mdrv_DPTx_Set_Color_Depth(mtk_dp, mtk_dp->info.depth);
		mdrv_DPTx_Set_Color_Format(mtk_dp, mtk_dp->info.format);
	} else {
		mtk_dp_dsc_pps_send(PPS_4k60);
		mhal_DPTx_EnableDSC(mtk_dp, true);
	}
}

void mtk_dp_fec_enable(unsigned int status)
{
	if (status)
		mhal_DPTx_EnableFEC(g_mtk_dp, true);
	else
		mhal_DPTx_EnableFEC(g_mtk_dp, false);
}

void mtk_dp_power_save(unsigned int status)
{
	u8 data;

	g_mtk_dp->training_info.bCableStateChange = true;
	if (status == 1) {
		fakecablein = false;
		data = 0x1;
		drm_dp_dpcd_write(&g_mtk_dp->aux, DPCD_00600, &data, 1);
		g_mtk_dp->training_info.bCablePlugIn = true;
		mhal_DPTx_Fake_Plugin(g_mtk_dp, true);
	} else if (status == 0) {
		fakecablein = true;
		data = 0x2;
		drm_dp_dpcd_write(&g_mtk_dp->aux, DPCD_00600, &data, 1);
		g_mtk_dp->training_info.bCablePlugIn = false;
		mhal_DPTx_Fake_Plugin(g_mtk_dp, false);
	}

	queue_work(g_mtk_dp->dptx_wq, &g_mtk_dp->dptx_work);
}

atomic_t dp_comm_event = ATOMIC_INIT(0);
void mtk_dp_video_trigger(int res)
{
	DPTXFUNC("0x%x\n", res);

	atomic_set(&dp_comm_event, res);
	wake_up_interruptible(&g_mtk_dp->control_wq);
}

static int mtk_dp_control_kthread(void *data)
{
	struct mtk_dp *mtk_dp = data;
	unsigned int videomute = 0;
	unsigned int res = 0;

	init_waitqueue_head(&mtk_dp->control_wq);

	while (!kthread_should_stop()) {
		wait_event_interruptible(mtk_dp->control_wq,
			atomic_read(&dp_comm_event));

		videomute = atomic_read(&dp_comm_event) >> 16;
		res = atomic_read(&dp_comm_event) & 0xff;
		atomic_set(&dp_comm_event, 0);

		if (videomute & video_unmute) {
			if (!fakecablein && mtk_dp->state > DPTXSTATE_PREPARE)
				mtk_dp->state = DPTXSTATE_PREPARE;

			mtk_dp->video_enable = true;
			mtk_dp->info.resolution = res;
			queue_work(mtk_dp->dptx_wq, &mtk_dp->dptx_work);
			queue_work(mtk_dp->dptx_wq, &mtk_dp->hdcp_work);

		} else if (videomute & video_mute) {
			mtk_dp->video_enable = false;
			if (!mtk_dp->dp_ready)
				continue;

			mdrv_DPTx_Video_Enable(mtk_dp, false);
		}
	}

	return 0;
}

int mtk_drm_dp_get_dev_info(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_dispif_info *info = data;
	struct mtk_dp *mtk_dp = g_mtk_dp;

	info->display_id = mtk_dp->id;
	info->displayFormat = mtk_dp->info.format;
	info->displayHeight = mtk_dp->info.DPTX_OUTBL.Vde;
	info->displayWidth = mtk_dp->info.DPTX_OUTBL.Hde;
	info->displayMode = DISPIF_MODE_VIDEO;
	info->displayType = DISPLAYPORT;
	info->isConnected = (mtk_dp->state == DPTXSTATE_NORMAL) ? true : false;
	info->isHwVsyncAvailable = true;
	info->vsyncFPS = g_mtk_dp->info.DPTX_OUTBL.FrameRate * 100;
	DPTXMSG("%s, %d, fake %d\n", __func__, __LINE__, fakecablein);

	return 0;
}

int mtk_drm_dp_audio_enable(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_dp *mtk_dp = g_mtk_dp;

	mtk_dp->audio_enable = *(bool *)data;
	DPTXMSG("audio_enable = %d\n", mtk_dp->audio_enable);

	if (!mtk_dp->dp_ready) {
		DPTXERR("%s, DP is not ready!\n", __func__);
		return 0;
	}

	mdrv_DPTx_I2S_Audio_Enable(mtk_dp, mtk_dp->audio_enable);

	return 0;
}

int mtk_drm_dp_audio_config(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_dp *mtk_dp = g_mtk_dp;

	mtk_dp->info.audio_config = *(unsigned int *)data;
	DPTXMSG("audio_config = 0x%x\n", mtk_dp->info.audio_config);

	if (!mtk_dp->dp_ready) {
		DPTXERR("%s, DP is not ready!\n", __func__);
		return 0;
	}

	mdrv_DPTx_I2S_Audio_Config(mtk_dp);

	return 0;
}

int mtk_drm_dp_get_cap(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	unsigned int ch[7] = {2, 3, 4, 5, 6, 7, 8};
	unsigned int fs[5] = {32, 44, 48, 96, 192};
	unsigned int len[3] = {16, 20, 24};
	unsigned int *dp_cap = data;

	if (fakecablein) {
		DPTXMSG("force audio format %dCH, %dkHz, %dbit\n",
			ch[force_ch], fs[force_fs], len[force_len]);
		*dp_cap = ((BIT(force_ch) << DP_CAPABILITY_CHANNEL_SFT)
			| (BIT(force_fs) << DP_CAPABILITY_SAMPLERATE_SFT)
			| (BIT(force_len) << DP_CAPABILITY_BITWIDTH_SFT));
		return 0;
	}

	if (g_mtk_dp->dp_ready)
		*dp_cap = g_mtk_dp->info.audio_caps;

	if (*dp_cap == 0)
		*dp_cap = ((DP_CHANNEL_2 << DP_CAPABILITY_CHANNEL_SFT)
			| (DP_SAMPLERATE_192 << DP_CAPABILITY_SAMPLERATE_SFT)
			| (DP_BITWIDTH_24 << DP_CAPABILITY_BITWIDTH_SFT));

	return 0;

}

int mtk_drm_dp_get_info(struct drm_device *dev,
		struct drm_mtk_session_info *info)
{
	DPTXDBG("%s, %d\n", __func__, __LINE__);
	info->physicalWidthUm = 900;
	info->physicalHeightUm = 1000;
	info->vsyncFPS = g_mtk_dp->info.DPTX_OUTBL.FrameRate * 100;

	return 0;
}

void mtk_dp_get_dsc_capability(u8 *dsc_cap)
{
	if (!g_mtk_dp->dp_ready) {
		DPTXMSG("%s, DP is not ready!\n", __func__);
		return;
	}

	drm_dp_dpcd_read(&g_mtk_dp->aux, DPCD_00060, dsc_cap, 16);
}

void mtk_dp_dsc_pps_send(u8 *PPS_128)
{
	u8 dsc_cap[16];
	u16 chunk_size = PPS_128[14] << 8 | PPS_128[15];
	u16 pic_width = PPS_128[8] << 8 | PPS_128[9];
	u16 slice_width = PPS_128[12] << 8 | PPS_128[13];

	if (!g_mtk_dp->dp_ready) {
		DPTXMSG("%s, DP is not ready!\n", __func__);
		return;
	}

	mtk_dp_get_dsc_capability(dsc_cap);

	PPS_128[0x0]
		= ((dsc_cap[0x1] & 0xf) << 4) | ((dsc_cap[0x1] & 0xf0) >> 4);
	if (dsc_cap[0x6] & BIT(0))
		PPS_128[0x4] |=  (0x1 << 5);
	else
		PPS_128[0x4] &= ~(0x1 << 5);

	mdrv_DPTx_DSC_SetPPS(g_mtk_dp, PPS_128, true);
	mdrv_DPTx_DSC_SetParam(g_mtk_dp, pic_width/slice_width, chunk_size);
}

struct edid *mtk_dp_handle_edid(struct mtk_dp *mtk_dp)
{
	struct drm_connector *connector = &mtk_dp->conn;

	/* use cached edid if we have one */
	if (mtk_dp->edid) {
		/* invalid edid */
		if (IS_ERR(mtk_dp->edid))
			return NULL;

		DPTXMSG("%s, duplicate edid from mtk_dp->edid!\n");
		return drm_edid_duplicate(mtk_dp->edid);
	}

	DPTXMSG("Get edid from RX!\n");
	return drm_get_edid(connector, &mtk_dp->aux.ddc);
}

irqreturn_t mtk_dp_hpd_event(int hpd, void *dev)
{
	struct mtk_dp *mtk_dp = dev;

	mdrv_DPTx_ISR(mtk_dp);

	return IRQ_HANDLED;
}

void mtk_dp_phy_param_init(struct mtk_dp *mtk_dp, uint32_t *buffer, int size)
{
	int i = 0;
	uint8_t mask = 0x3F;

	if (buffer == NULL || size != DPTX_PHY_REG_COUNT) {
		DPTXERR("invalid param\n");
		return;
	}

	for (i = 0; i < DPTX_PHY_LEVEL_COUNT; i++) {
		mtk_dp->phy_params[i].C0 = (buffer[i/4] >> (8*(i%4))) & mask;
		mtk_dp->phy_params[i].CP1
			= (buffer[i/4 + 3] >> (8*(i%4))) & mask;
	}
}

static int mtk_dp_dt_parse_pdata(struct mtk_dp *mtk_dp,
		struct platform_device *pdev)
{
	struct resource regs;
	struct device *dev = &pdev->dev;
	int ret = 0;
	uint32_t phy_params_int[DPTX_PHY_REG_COUNT] = {
		0x20181410, 0x20241e18, 0x00003028,
		0x10080400, 0x000c0600, 0x00000008
	};
	uint32_t phy_params_dts[DPTX_PHY_REG_COUNT];

	if (of_address_to_resource(dev->of_node, 0, &regs) != 0)
		dev_err(dev, "Missing reg in %s node\n",
		dev->of_node->full_name);

	mtk_dp->regs = of_iomap(dev->of_node, 0);
	mtk_dp->dp_tx_clk = devm_clk_get(dev, "dp_tx_faxi");
	if (IS_ERR(mtk_dp->dp_tx_clk)) {
		ret = PTR_ERR(mtk_dp->dp_tx_clk);
		dev_err(dev, "Failed to get dptx clock: %d\n", ret);
		goto error;
	}

	ret = of_property_read_u32_array(dev->of_node, "dptx,phy_params",
		phy_params_dts, ARRAY_SIZE(phy_params_dts));
	if (ret) {
		DPTXMSG("get phy_params fail, use default val, ret %d\n", ret);
		mtk_dp_phy_param_init(mtk_dp,
			phy_params_int, ARRAY_SIZE(phy_params_int));
	} else
		mtk_dp_phy_param_init(mtk_dp,
			phy_params_dts, ARRAY_SIZE(phy_params_dts));

	DPTXMSG("reg and clock get success!\n");
error:
	return 0;
}

static inline struct mtk_dp *mtk_dp_ctx_from_conn(struct drm_connector *c)
{
	return container_of(c, struct mtk_dp, conn);
}

static enum drm_connector_status mtk_dp_conn_detect(struct drm_connector *conn,
	bool force)
{
	struct mtk_dp *mtk_dp = mtk_dp_ctx_from_conn(conn);

	DPTXFUNC("fakecablein %d\n", fakecablein);
	if (fakecablein)
		return connector_status_connected;

	return ((mtk_dp->dp_ready) ? connector_status_connected :
		connector_status_disconnected);
}

static void mtk_dp_conn_destroy(struct drm_connector *conn)
{
	drm_connector_cleanup(conn);
}

static const struct drm_connector_funcs mtk_dp_connector_funcs = {
	.detect = mtk_dp_conn_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = mtk_dp_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int mtk_dp_conn_get_modes(struct drm_connector *conn)
{
	struct mtk_dp *mtk_dp = mtk_dp_ctx_from_conn(conn);
	int ret;
	struct drm_display_mode *mode;
	struct drm_device *dev = conn->dev;

	DPTXFUNC("fakecablein %d, res %d\n", fakecablein, fakeres);
	if (fakecablein) {
		conn->display_info.width_mm = 900;
		conn->display_info.height_mm = 1100;

		/* driver figures it out in this case */
		conn->display_info.bpc = 8;
		conn->display_info.color_formats = DRM_COLOR_FORMAT_RGB444;
		conn->display_info.cea_rev = 0;
		conn->display_info.max_tmds_clock = 0;
		conn->display_info.dvi_dual = false;

		if (fakeres == SINK_3840_2160)
			mode = drm_mode_duplicate(dev, &dptx_est_modes[0]);
		else if (fakeres == SINK_3840_2160_30)
			mode = drm_mode_duplicate(dev, &dptx_est_modes[1]);
		else if (fakeres == SINK_1920_1080)
			mode = drm_mode_duplicate(dev, &dptx_est_modes[2]);
		else if (fakeres == SINK_1280_720)
			mode = drm_mode_duplicate(dev, &dptx_est_modes[3]);
		else
			mode = drm_mode_duplicate(dev, &dptx_est_modes[4]);

		drm_mode_set_name(mode);
		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(conn, mode);

		return 1;
	}

	if (mtk_dp->edid) {
		drm_mode_connector_update_edid_property(&mtk_dp->conn,
			mtk_dp->edid);
		ret = drm_add_edid_modes(&mtk_dp->conn, mtk_dp->edid);
		drm_edid_to_eld(&mtk_dp->conn, mtk_dp->edid);
		DPTXMSG("%s modes = %d\n", __func__, ret);
		if (ret)
			return ret;
	} else {
		drm_mode_connector_update_edid_property(&mtk_dp->conn, NULL);
		DPTXMSG("%s NULL EDID\n", __func__);
	}

	return 0;
}

struct drm_display_limit_mode {
	int hdisplay;
	int vdisplay;
	int vrefresh;
	int clock;
	int valid;
};

static struct drm_display_limit_mode dp_plat_limit[] = {
	{3840, 2160, 60, 594000, 1},
	{3840, 2160, 30, 297000, 1},
	{1080, 2460, 60, 174110, 1},
	{1920, 1200, 60, 152128, 1},
	{1920, 1080, 60, 148500, 1},
	{1280, 720,  60,  74250, 1},
	{ 640,  480, 60,  25200, 1},
};

void mtk_dp_enable_4k60(int enable)
{
#if DPTX_SUPPORT_DSC
	if (enable > 0)
		dp_plat_limit[0].valid = 1;
	else
		dp_plat_limit[0].valid = 0;
#else
	dp_plat_limit[0].valid = 0;
#endif

	DPTXFUNC("enable = %d\n", dp_plat_limit[0].valid);
}

static enum drm_mode_status mtk_dp_conn_mode_valid(struct drm_connector *conn,
		struct drm_display_mode *mode)
{
	int plat_limit_array = ARRAY_SIZE(dp_plat_limit);
	int i;
	struct mtk_dp *mtk_dp = mtk_dp_ctx_from_conn(conn);
	int bandwidth = mtk_dp->training_info.ubLinkLaneCount *
			mtk_dp->training_info.ubLinkRate * 27000 * 8 / 24;

	if (mode->hdisplay == 3840 && mode->vdisplay == 2160 &&
		mode->vrefresh == 60 && mtk_dp->has_dsc)
		bandwidth = bandwidth * 594 / 202.5;

	if (fakecablein == true)
		bandwidth = dp_plat_limit[0].clock;

	DPTXDBG("Hde:%d,Vde:%d,fps:%d,clk:%d,bandwidth:%d,4k60:%d\n",
		mode->hdisplay, mode->vdisplay, mode->vrefresh, mode->clock,
		bandwidth, dp_plat_limit[0].valid);

	if (mode->clock > (dp_plat_limit[0].clock + 50000))
		return MODE_CLOCK_HIGH;
	if (mode->clock < (dp_plat_limit[plat_limit_array-1].clock - 5000))
		return MODE_CLOCK_LOW;

	for (i = 0; i < plat_limit_array; i++) {
		if (mode->hdisplay == 640 && mode->vdisplay == 480)
			break;

		if (mode->vrefresh == 0 && mode->htotal != 0
			&& mode->vtotal != 0)
			mode->vrefresh
				= mode->clock / mode->htotal / mode->vtotal;

		if (mode->clock == 0)
			mode->clock
				= mode->htotal * mode->vtotal * mode->vrefresh;

		if ((abs(dp_plat_limit[i].vrefresh - mode->vrefresh) <= 1)
			&& (mode->vdisplay == dp_plat_limit[i].vdisplay)
			&& (mode->hdisplay == dp_plat_limit[i].hdisplay)
			&& (dp_plat_limit[i].clock < bandwidth)) {

			if (dp_plat_limit[i].valid)
				break;

			return MODE_BAD_VSCAN;
		}
	}

	if (i >= plat_limit_array)
		return MODE_BAD_VSCAN;

	DPTXDBG("%s xres=%d, yres=%d, refresh=%d, clock=%d\n",
			__func__, mode->hdisplay, mode->vdisplay,
			mode->vrefresh,
			mode->clock);

	return drm_mode_validate_size(mode, 0x1fff, 0x1fff);
}

static const struct drm_connector_helper_funcs mtk_dp_connector_helper_funcs = {
	.get_modes = mtk_dp_conn_get_modes,
	.mode_valid = mtk_dp_conn_mode_valid,
};

static void mtk_dp_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
	kfree(encoder);
}


static const struct drm_encoder_funcs mtk_dp_enc_funcs = {
	.destroy = mtk_dp_encoder_destroy,
};

static ssize_t mtk_dp_aux_transfer(struct drm_dp_aux *mtk_aux,
	struct drm_dp_aux_msg *msg)
{
	u8 ubCmd;
	void *pData;
	size_t ubLength, ret = 0;
	u32 usADDR;
	bool mot, ack = false;
	struct mtk_dp *mtk_dp;

	mtk_dp = container_of(mtk_aux, struct mtk_dp, aux);
	mot = (msg->request & DP_AUX_I2C_MOT) ? true : false;
	ubCmd = msg->request;
	usADDR = msg->address;
	ubLength = msg->size;
	pData = msg->buffer;

	switch (ubCmd) {
	case DP_AUX_I2C_MOT:
	case DP_AUX_I2C_WRITE:
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE | DP_AUX_I2C_MOT:
		ubCmd &= ~DP_AUX_I2C_WRITE_STATUS_UPDATE;
		ack = mdrv_DPTx_AuxWrite_DPCD(mtk_dp, ubCmd,
			usADDR, ubLength, pData);
		break;

	case DP_AUX_I2C_READ:
	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ | DP_AUX_I2C_MOT:
		ack = mdrv_DPTx_AuxRead_DPCD(mtk_dp, ubCmd,
			usADDR, ubLength, pData);
		break;

	default:
		DPTXERR("invalid aux cmd = %d\n", ubCmd);
		ret = -EINVAL;
		break;
	}

	if (ack) {
		msg->reply = DP_AUX_NATIVE_REPLY_ACK | DP_AUX_I2C_REPLY_ACK;
		ret = ubLength;
	} else {
		msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
		ret = -EAGAIN;
	}

	return ret;
}

static void mtk_dp_aux_init(struct mtk_dp *mtk_dp)
{
	drm_dp_aux_init(&mtk_dp->aux);
	DPTXMSG("aux hw_mutex = 0x%x\n", &mtk_dp->aux.hw_mutex);

	mtk_dp->aux.name = kasprintf(GFP_KERNEL, "DPDDC-MTK");
	mtk_dp->aux.transfer = mtk_dp_aux_transfer;
}

void mtk_dp_test(unsigned int status)
{
	DPTXMSG("g_mtk_dp = 0x%x\n", g_mtk_dp);
	mhal_DPTx_SWInterruptEnable(g_mtk_dp, true);
	mhal_DPTx_SWInterruptSet(g_mtk_dp, status);
}

void mtk_dp_hdcp_enable(bool enable)
{
	g_hdcp_on = enable;
}

void mtk_dp_force_hdcp1x(bool enable)
{
	g_mtk_dp->info.bForceHDCP1x = enable;
}

static char *mtk_hdcp_version(void)
{
#ifdef DPTX_HDCP_ENABLE
	if (g_mtk_dp->info.hdcp2_info.bEnable) {
		if (g_mtk_dp->info.hdcp2_info.bRepeater)
			return "DP_HDCP2X_REPEATER";
		else
			return "DP_HDCP2X";
	} else if (g_mtk_dp->info.hdcp1x_info.bEnable) {
		if (g_mtk_dp->info.hdcp1x_info.bRepeater)
			return "DP_HDCP1X_REAPEATER";
		else
			return "DP_HDCP1X";
	}
#endif
	return "DP_HDCP_NONE";
}

static char *mtk_hdcp_status(void)
{
#ifdef DPTX_HDCP_ENABLE
	if (g_mtk_dp->info.bAuthStatus == AUTH_PASS)
		return "DP_AUTH_STATUS_PASS";
	else if (g_mtk_dp->info.bAuthStatus == AUTH_FAIL)
		return "DP_AUTH_STATUS_FAIL";
	else if (g_mtk_dp->info.bAuthStatus != AUTH_ZERO)
		return "DP_AUTH_STATUS_DOING";
#endif
	return "DP_AUTH_STATUS_NONE";
}

int mtk_dp_hdcp_getInfo(char *buffer, int size)
{
	int ret = 0;

	if (!g_hdcp_on)
		ret = snprintf(buffer, size,
			"HDCP Function is disable!\n");
	else if (g_mtk_dp->info.bForceHDCP1x)
		ret = snprintf(buffer, size,
			"Force HDCP1x is enable, not support HDCP2x!\n"
			"HDCP INFO:%s, %s\n",
			mtk_hdcp_version(), mtk_hdcp_status());
	else
		ret = snprintf(buffer, size, "HDCP HINFO:%s, %s\n",
			mtk_hdcp_version(), mtk_hdcp_status());

	return ret;
}

int mtk_dp_phy_getInfo(char *buffer, int size)
{
	int len = 0;
	int i = 0;
	char *phy_names[10] = {
		"L0P0", "L0P1", "L0P2", "L0P3", "L1P0",
		"L1P1", "L1P2", "L2P0", "L2P1", "L3P0"};

	len = snprintf(buffer, size, "PHY INFO:\n");
	for (i = 0; i < DPTX_PHY_LEVEL_COUNT; i++)
		len += snprintf(buffer + len, size - len,
			"#%d(%s):C0 = %#04X(%2d), CP1 = %#04X(%2d)\n", i,
			phy_names[i],
			g_mtk_dp->phy_params[i].C0, g_mtk_dp->phy_params[i].C0,
			g_mtk_dp->phy_params[i].CP1,
			g_mtk_dp->phy_params[i].CP1);

	return len;
}

void mtk_dp_set_adjust_phy(uint8_t index, uint8_t c0, uint8_t cp1)
{
	if (index >= 10) {
		DPTXERR("index(%d) must < 10!", index);
		return;
	}

	g_mtk_dp->phy_params[index].C0 = c0;
	g_mtk_dp->phy_params[index].CP1 = cp1;
}

void mtk_dp_hotplug_uevent(unsigned int event)
{
	if (g_mtk_dp->info.bPatternGen)
		return;

	DPTXFUNC("fake:%d, event:%d\n", fakecablein, event);
	notify_uevent_user(&dptx_notify_data,
		event > 0 ? DPTX_STATE_ACTIVE : DPTX_STATE_NO_DEVICE);

	if (g_mtk_dp->info.audio_caps != 0)
		extcon_set_state_sync(dptx_extcon, EXTCON_DISP_HDMI,
			event > 0 ? true : false);
}

void mtk_dp_force_audio(unsigned int ch, unsigned int fs, unsigned int len)
{
	if (ch != 0xff)
		force_ch = ch;
	if (fs != 0xff)
		force_fs = fs;
	if (len != 0xff)
		force_len = len;

	fakecablein = true;
}

void mtk_dp_force_res(unsigned int res, unsigned int bpc)
{
	DPTXMSG("status:0x%x->0x%x; bpc:0x%x->0x%x\n",
		fakeres, res, fakebpc, bpc);
	fakeres = res;
	fakebpc = bpc;
}

void mtk_dp_fake_plugin(unsigned int status, unsigned int bpc)
{
	if (g_mtk_dp->bPowerOn) {
		mdrv_DPTx_VideoMute(g_mtk_dp, true);
		mdrv_DPTx_AudioMute(g_mtk_dp, true);
	}

	fakeres = FAKE_DEFAULT_RES;
	fakebpc = DP_COLOR_DEPTH_8BIT;

	mtk_dp_hotplug_uevent(0x0);
	kfree(g_mtk_dp->edid);
	g_mtk_dp->edid = NULL;

	if (fakeres == SINK_3840_2160)
		mtk_dp_enable_4k60(true);
	else
		mtk_dp_enable_4k60(false);


	msleep(100);

	mtk_dp_force_res(status, bpc);
	if (status < FAKE_DEFAULT_RES)
		fakecablein = true;
	else
		fakecablein = false;

	g_mtk_dp->state = DPTXSTATE_INITIAL;
	mtk_dp_hotplug_uevent(1);
}

void mtk_dp_HPDInterruptSet(int bstatus)
{
	DDPFUNC("status:%d[2:DISCONNECT, 4:CONNECT, 8:IRQ] Power:%d\n",
		bstatus, g_mtk_dp->bPowerOn);

	if ((bstatus == HPD_CONNECT && !g_mtk_dp->bPowerOn)
		|| (bstatus == HPD_DISCONNECT && g_mtk_dp->bPowerOn)
		|| (bstatus == HPD_INT_EVNET && g_mtk_dp->bPowerOn)) {

		if (bstatus == HPD_CONNECT) {
			int ret;

			ret = clk_prepare_enable(g_mtk_dp->dp_tx_clk);
			if (ret < 0)
				DPTXERR("Fail to enable dptx clock: %d\n", ret);

			mdrv_DPTx_InitPort(g_mtk_dp);
			mhal_DPTx_USBC_HPD(g_mtk_dp, true);
			g_mtk_dp->bPowerOn = true;
		} else if (bstatus == HPD_DISCONNECT)
			mhal_DPTx_USBC_HPD(g_mtk_dp, false);

		mdrv_DPTx_USBC_HPD_Event(bstatus);
		return;
	}
}

void mtk_dp_SWInterruptSet(int bstatus)
{
	mutex_lock(&dp_lock);

	if ((bstatus == HPD_DISCONNECT && g_mtk_dp->bPowerOn)
		|| (bstatus == HPD_CONNECT && !g_mtk_dp->bPowerOn))
		g_mtk_dp->bUeventToHwc = true;

	if (!g_mtk_dp->bPowerOn && bstatus == HPD_DISCONNECT
		&& g_mtk_dp->disp_status == DPTX_DISP_SUSPEND) {
		DPTXMSG("System is sleeping, Plug Out\n");
		mtk_dp_hotplug_uevent(0);
		g_mtk_dp->disp_status = DPTX_DISP_NONE;
		mutex_unlock(&dp_lock);
		return;
	}

	mtk_dp_HPDInterruptSet(bstatus);

	mutex_unlock(&dp_lock);
}

void mtk_dp_poweroff(void)
{
	DPTXFUNC();
	mutex_lock(&dp_lock);
	if (g_mtk_dp->disp_status == DPTX_DISP_NONE) {
		DPTXMSG("DPTX has been powered off\n");
		mutex_unlock(&dp_lock);
		return;
	}

	g_mtk_dp->disp_status = DPTX_DISP_SUSPEND;
	mtk_dp_HPDInterruptSet(HPD_DISCONNECT);
	mutex_unlock(&dp_lock);
}

void mtk_dp_poweron(void)
{
	DPTXFUNC();
	mutex_lock(&dp_lock);
	g_mtk_dp->disp_status = DPTX_DISP_RESUME;
	if (g_mtk_dp->bPowerOn) {
		DPTXMSG("DPTX has been powered on\n");
		mutex_unlock(&dp_lock);
		return;
	}

	mtk_dp_HPDInterruptSet(HPD_CONNECT);
	mutex_unlock(&dp_lock);
}

static int mtk_dp_create_workqueue(struct mtk_dp *mtk_dp)
{
	mtk_dp->dptx_wq = create_singlethread_workqueue("mtk_dptx_wq");
	if (!mtk_dp->dptx_wq) {
		DPTXERR("Failed to create dptx workqueue\n");
		return -ENOMEM;
	}

	INIT_WORK(&mtk_dp->dptx_work, mdrv_DPTx_main_handle);
	INIT_WORK(&mtk_dp->hdcp_work, mdrv_DPTx_hdcp_handle);

	return 0;
}

static int mtk_dp_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);
	struct drm_device *drm = data;
	int ret;

	mtk_dp->drm_dev = drm;

	DPTXDBG("%s, %d, mtk_dp 0x%p\n", __func__, __LINE__, mtk_dp);

	ret = drm_connector_init(drm, &mtk_dp->conn, &mtk_dp_connector_funcs,
		DRM_MODE_CONNECTOR_DisplayPort);

	if (ret) {
		dev_err(mtk_dp->dev, "Failed to initialize connector: %d\n",
			ret);
		return ret;
	}

	drm_connector_helper_add(&mtk_dp->conn, &mtk_dp_connector_helper_funcs);

	if (drm_encoder_init(drm, &mtk_dp->enc,	&mtk_dp_enc_funcs,
		DRM_MODE_ENCODER_DPMST, "DP MST"))
		goto err_encoder_init;

	mtk_dp->enc.possible_crtcs = 2;

	drm_mode_connector_attach_encoder(&mtk_dp->conn, &mtk_dp->enc);

	g_mtk_dp = mtk_dp;

	mtk_dp->conn.kdev = drm->dev;
	mtk_dp->aux.dev = mtk_dp->conn.kdev;
	if (drm_dp_aux_register(&mtk_dp->aux))
		goto err_encoder_init;

	DPTXFUNC("Successful\n");
	return 0;

err_encoder_init:
	drm_connector_cleanup(&mtk_dp->conn);

	DPTXERR("%s failed!  %d\n", __func__, __LINE__);
	return ret;
}

static void mtk_dp_unbind(struct device *dev, struct device *master,
		void *data)
{
	DPTXFUNC();
}

static const struct component_ops mtk_dp_component_ops = {
	.bind = mtk_dp_bind, .unbind = mtk_dp_unbind,
};

static int mtk_drm_dp_probe(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp;
	struct device *dev = &pdev->dev;
	int ret, irq_num = 0;
	int comp_id;
	struct mtk_drm_private *mtk_priv = dev_get_drvdata(dev);

	DPTXFUNC();
	mtk_dp = devm_kmalloc(dev, sizeof(*mtk_dp), GFP_KERNEL | __GFP_ZERO);
	if (!mtk_dp)
		return -ENOMEM;

	memset(mtk_dp, 0, sizeof(struct mtk_dp));
	mtk_dp->id = 0x0;
	mtk_dp->dev = dev;
	mtk_dp->priv = mtk_priv;
	mtk_dp->bUeventToHwc = false;
	mtk_dp->disp_status = DPTX_DISP_NONE;

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		dev_err(&pdev->dev, "failed to request dp irq resource\n");
		return -EPROBE_DEFER;
	}

	ret = mtk_dp_dt_parse_pdata(mtk_dp, pdev);
	if (ret)
		return ret;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DPTX);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto error;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &mtk_dp->ddp_comp, comp_id,
			NULL);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		goto error;
	}

	mtk_dp_aux_init(mtk_dp);

	DPTXMSG("comp_id %d, type %d, irq %d\n", comp_id,
		MTK_DISP_DPTX, irq_num);

	irq_set_status_flags(irq_num, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(&pdev->dev, irq_num, mtk_dp_hpd_event,
			IRQ_TYPE_LEVEL_HIGH, dev_name(&pdev->dev), mtk_dp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request mediatek dptx irq\n");
		return -EPROBE_DEFER;
	}

	dptx_notify_data.name = "hdmi";  // now hwc not support DP
	dptx_notify_data.index = 0;
	dptx_notify_data.state = DPTX_STATE_NO_DEVICE;
	ret = dptx_uevent_dev_register(&dptx_notify_data);
	if (ret)
		DPTXERR("switch_dev_register failed, returned:%d!\n", ret);

	dptx_extcon = devm_extcon_dev_allocate(&pdev->dev, dptx_cable);
	if (IS_ERR(dptx_extcon)) {
		DPTXERR("Couldn't allocate dptx extcon device\n");
		return PTR_ERR(dptx_extcon);
	}

	dptx_extcon->dev.init_name = "dp_audio";
	ret = devm_extcon_dev_register(&pdev->dev, dptx_extcon);
	if (ret) {
		pr_debug("failed to register dptx extcon: %d\n", ret);
		return ret;
	}

	g_mtk_dp = mtk_dp;
	mutex_init(&dp_lock);

	platform_set_drvdata(pdev, mtk_dp);

	mtk_dp->control_task = kthread_run(mtk_dp_control_kthread,
		(void *)mtk_dp, "mtk_dp_video_trigger");

	mtk_dp_create_workqueue(mtk_dp);

	return component_add(&pdev->dev, &mtk_dp_component_ops);

error:
	return -EPROBE_DEFER;
}

static int mtk_drm_dp_remove(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp = platform_get_drvdata(pdev);

	if (mtk_dp->dptx_wq)
		destroy_workqueue(mtk_dp->dptx_wq);

	mutex_destroy(&dp_lock);
	drm_connector_cleanup(&mtk_dp->conn);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_dp_suspend(struct device *dev)
{
	struct mtk_dp *mtk_dp = dev_get_drvdata(dev);

	mutex_lock(&dp_lock);
	if (mtk_dp->bPowerOn) {
		mtk_dp->disp_status = DPTX_DISP_SUSPEND;
		mtk_dp_HPDInterruptSet(HPD_DISCONNECT);
		mdelay(5);
	}
	mutex_unlock(&dp_lock);

	DPTXFUNC();
	return 0;
}

static int mtk_dp_resume(struct device *dev)
{
	DPTXFUNC();
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_dp_pm_ops,
		mtk_dp_suspend, mtk_dp_resume);


static const struct of_device_id mtk_dp_of_match[] = {
	{ .compatible = "mediatek,mt6885-dp_tx", },
	{ },
};


struct platform_driver mtk_dp_tx_driver = {
	.probe = mtk_drm_dp_probe,
	.remove = mtk_drm_dp_remove,
	.driver = {
		.name = "mediatek-drm-dp",
		.of_match_table = mtk_dp_of_match,
		.pm = &mtk_dp_pm_ops,
	},
};


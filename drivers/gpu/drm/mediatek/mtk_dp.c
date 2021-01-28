/*
 * Copyright (c) 2020 MediaTek Inc.
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
static BYTE g_c0 = 32, g_cp1;

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

struct DPTX_DRV_TIMING_PARAMETER DPTX_TIMING;
WORD gbPGSetting[DPTx_PATTERN_RGB_MAX][22] = {
#if DPTx_PATTERN_RGB_640_480_EN
	/*****************************************
	 * DPTx_PATTERN_RGB640_480
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x10 >> 0x2), (0x60 >> 0x2), (0x30 >> 0x2), (0x280 >> 0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE(V active)
		0x10, 0x2, 0x21, (0x1E0+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode, init value
		0x0100, 0x0001, 0x83FF, 0x0800,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x320, 0x90, (0x60<<1), 0x280,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x20D, 0x23, (0x2<<1), 0x1E0,
		//Hpolarity, Vpolarity,
		1, 1 },
#endif

#if DPTx_PATTERN_RGB_720_480_EN
	/*****************************************
	 * DPTx_PATTERN_RGB720_480  (480P)
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x10 >> 0x2), (0x3E >> 0x2), (0x3C >> 0x2), (0x2D0 >> 0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		0x09, 0x6, 0x1B, (0x1E0+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode, init value
		0x0100, 0x0001, 0x83FF, 0x0800,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x35A, 0x7A, (0x3E<<1), 0x2D0,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x20D, 0x24, (0x6<<1), 0x1E0,
		//Hpolarity, Vpolarity,
		1, 1 },
#endif

#if DPTx_PATTERN_RGB_800_600_EN
	/*****************************************
	 * DPTx_PATTERN_RGB800_600
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x28 >> 0x2), (0x80 >> 0x2), (0x58 >> 0x2), (0x320 >> 0x2),
		//Vsync frontporch,  Vsync Width, Vsync backporch, VDE(V active)
		0x10, 0x4, 0x17, (0x258+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0600,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x420, 0xD8, (0x80<<1), 0x320,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x274, 0x1B, (0x4<<1), 0x258,
		//Hpolarity, Vpolarity,
		0, 0 },
#endif

#if DPTx_PATTERN_RGB_1280_720_EN
	/*****************************************
	 * DPTx_PATTERN_RGB1280_720  (720P)
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x104 >> 0x2), (0x28 >> 0x2), (0x46 >> 0x2), (0x500 >> 0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE(V active)
		0x16, 0x05, 0x03, (0x02D0+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0600,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x0672, 0x006E, (0x0028<<1), 0x0500,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x02EE, 0x0008, (0x0005<<1), 0x02D0,
		//Hpolarity, Vpolarity,
		0, 0 },
#endif

#if DPTx_PATTERN_RGB_1280_1024_EN
	/*****************************************
	 * DPTx_PATTERN_RGB1280_1024
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x58 >> 0x2), (0x2C >> 0x2), (0x94 >> 0x2), (0x500 >> 0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		0x04, 0x05, 0x24, (0x400+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0600,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x0618, 0x00C0, (0x002C<<1), 0x0500,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x042D, 0x0029, (0x0005<<1), 0x0400,
		//Hpolarity, Vpolarity,
		0, 0 },
#endif

#if DPTx_PATTERN_RGB_1920_1080_EN
	/*****************************************
	 * DPTx_PATTERN_RGB1920_1080  (1080P)
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x58 >> 0x2), (0x2C >> 0x2), (0x94 >> 0x2), (0x780 >> 0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		0x04, 0x05, 0x24, (0x438+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0300,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x898, 0xC0, (0x2C<<1), 0x780,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x465, 0x29, (0x05<<1), 0x438,
		//Hpolarity, Vpolarity,
		0, 0 },
#endif

#if DPTx_PATTERN_RGB_3840_2160_EN
	/*****************************************
	 * DPTx_PATTERN_RGB3840_2160
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x0058 >> 2), (0x002C >> 2), (0x001C >> 2), (0x0F00 >> 2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		0x000A, 0x0005, 0x002F, (0x0870+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0300,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x0FA0, 0x0048, (0x002C<<1), 0x0F00,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x08AE, 0x0034, (0x0005<<1), 0x0870,
		//Hpolarity, Vpolarity,
		0, 0 },
#endif

#if DPTx_PATTERN_RGB_4096_2160_EN
	/*****************************************
	 * DPTx_PATTERN_RGB4096_2160
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x0030 >> 2), (0x0020 >> 2), (0x0050 >> 2), (0x1000 >> 2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		0x0003, 0x000A, 0x0031, (0x0870+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0300,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x10A0, 0x0070, (0x0020<<1), 0x1000,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x08AE, 0x003B, (0x000A<<1), 0x0870},
#endif

#if DPTx_PATTERN_RGB_7680_4320_EN
	/*****************************************
	 * DPTx_PATTERN_RGB_7680_4320
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch, HDE (H active)
	{(0x0018 >> 2), (0x0060 >> 2), (0x00F0 >> 2), (0x1E00 >> 2),
		//Vsync frontporch, Vsync Width, Vsync backporch,VDE (V active)
		0x002F, 0x0008, 0x0006, (0x10E0+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0300,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x1F68, 0x0150, (0x0060<<1), 0x1E00,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x111D, 0x000E, (0x0008<<1), 0x10E0},
#endif

#if DPTx_PATTERN_RGB_848_480_EN
	/*****************************************
	 * DPTx_PATTERN_RGB848 480
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x10 >> 0x2), (0x60 >> 0x2), (0x30 >> 0x2), (0x350 >> 0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		0x6, 0x8, 0x17, (0x1E0+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode, init value
		0x0100, 0x0001, 0x83FF, 0x0800,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x440, 0x90, (0x60<<1), 0x280,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x205, 0x23, (0x2<<1), 0x1E0,
		//Hpolarity, Vpolarity,
		0, 0 },
#endif

#if DPTx_PATTERN_RGB_1280_960_EN
	/*****************************************
	 * DPTx_PATTERN_RGB1280_960  (720P)
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x60 >> 0x2), (0x70 >> 0x2), (0x138 >> 0x2), (0x500 >> 0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		0x1, 0x03, 0x24, (0x3C0+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0600,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0x0708, 0x006E, (0x0028<<1), 0x0500,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x03E8, 0x0008, (0x0005<<1), 0x02D0,
		//Hpolarity, Vpolarity,
		0, 0 },
#endif

#if DPTx_PATTERN_RGB_1920_1440_EN
	/*****************************************
	 * DPTx_PATTERN_RGB1920_1440  (1080P)
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{(0x80 >> 0x2), (0xD0 >> 0x2), (0x158 >> 0x2), (0x780 >> 0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		0x01, 0x03, 0x38, (0x5a0+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode
		0x0100, 0x0001, 0x83FF, 0x0300,
		//Htotal, Hsync Start, Hsync Width,  HDE
		0xA28, 0xC0, (0x2C<<1), 0x780,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		0x5DC, 0x29, (0x05<<1), 0x438,
		//Hpolarity, Vpolarity,
		1, 0 },
#endif

#if DPTx_PATTERN_RGB_DEFINERES_EN
	/*****************************************
	 * DPTx_PATTERN_RGB_DEFINERES_EN
	 *****************************************/
	// Hsync frontporch,  Hsync Width,   Hsync backporch,  HDE (H active)
	{((DPTXMSA_HFP)>>2), ((DPTXMSA_HSW)>>2), ((DPTXMSA_HBP)>>2),
		((DPTXMSA_Hwidth)>>0x2),
		//Vsync frontporch, Vsync Width, Vsync backporch, VDE (V active)
		DPTXMSA_VFP, DPTXMSA_VSW, DPTXMSA_VBP, (DPTXMSA_Vheight+1),
		// horizontal increase(2 fields), vertical increase,
		//MSA sw overwrite mode, init value
		0x0fff, 0x0001, 0x83FF, 0x0300,
		//Htotal, Hsync Start, Hsync Width,  HDE
		DPTXMSA_Htotal, DPTXMSA_HStart, (DPTXMSA_HSW<<1),
		DPTXMSA_Hwidth,
		//Vtotal, Vsync Start, Vsync Width,  VDE
		DPTXMSA_Vtotal, DPTXMSA_VStart, (DPTXMSA_VSW<<1),
		DPTXMSA_Vheight},
#endif
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

static inline struct mtk_dp *mtk_dp_ctx_from_conn(struct drm_connector *c)
{
	return container_of(c, struct mtk_dp, conn);
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

	mtk_dp->info.ubDPTXInPutTypeSel = DPTXInputSrc_DPINTF;
	mtk_dp->info.ubDPTX_PatternIdx = DPTx_PATTERN_RGB_1920_1080;
	mtk_dp->info.ubDPTXColorFormatSel = DP_COLOR_FORMAT_RGB_444;
	mtk_dp->info.ubDPTXColorDepthSel = DP_COLOR_DEPTH_8BIT;
	mtk_dp->info.bUseTopTimingTBL = false;
	mtk_dp->info.bFixFrameRate = false;
	mtk_dp->info.bSetFreeSync = false;
	mtk_dp->info.bSetAudioMute = false;
	mtk_dp->info.bSetVideoMute = false;
	mtk_dp->info.uiAudioConfig = 0;
	mtk_dp->info.uiVideoConfig = 0;
	mtk_dp->info.DPTX_OUTBL.FrameRate = 60;
	mtk_dp->bPowerOn = false;
	mtk_dp->audio_enable = false;
	mtk_dp->video_enable = false;
	mtk_dp->dp_ready = false;
	mtk_dp->has_dsc = false;
	mtk_dp->has_fec = false;
	mtk_dp->dsc_enable = false;

	mdrv_DPTx_CheckMaxLinkRate(mtk_dp);
}

void mdrv_DPTx_LoadOutputTimingTBL(struct mtk_dp *mtk_dp)
{
	mtk_dp->info.DPTX_OUTBL.Video_ip_mode =
		DPTX_TIMING.Video_ip_mode;

	mtk_dp->info.DPTX_OUTBL.Htt = DPTX_TIMING.Htt;
	mtk_dp->info.DPTX_OUTBL.Hde = DPTX_TIMING.Hde;
	mtk_dp->info.DPTX_OUTBL.Hfp = DPTX_TIMING.Hfp;
	mtk_dp->info.DPTX_OUTBL.Hsw = DPTX_TIMING.Hsw;
	mtk_dp->info.DPTX_OUTBL.bHsp = DPTX_TIMING.bHsp;
	mtk_dp->info.DPTX_OUTBL.Hbp = DPTX_TIMING.Hbp;
	mtk_dp->info.DPTX_OUTBL.Hbk = DPTX_TIMING.Htt - DPTX_TIMING.Hde;
	mtk_dp->info.DPTX_OUTBL.Vtt = DPTX_TIMING.Vtt;
	mtk_dp->info.DPTX_OUTBL.Vde = DPTX_TIMING.Vde;
	mtk_dp->info.DPTX_OUTBL.Vfp = DPTX_TIMING.Vfp;
	mtk_dp->info.DPTX_OUTBL.Vsw = DPTX_TIMING.Vsw;
	mtk_dp->info.DPTX_OUTBL.bVsp = DPTX_TIMING.bVsp;
	mtk_dp->info.DPTX_OUTBL.Vbp = DPTX_TIMING.Vbp;
	if (!mtk_dp->info.bFixFrameRate)
		mtk_dp->info.DPTX_OUTBL.FrameRate = DPTX_TIMING.FrameRate;
	mtk_dp->info.DPTX_OUTBL.PixRateKhz = DPTX_TIMING.PixRateKhz;
}

void mdrv_DPTx_SetSDP_DownCntinit(struct mtk_dp *mtk_dp,
	u16 Sram_Read_Start)
{
	u16 SDP_Down_Cnt_Init = 0x0000;
	u8 ucDCOffset;

	if (mtk_dp->info.DPTX_OUTBL.PixRateKhz > 0)
		SDP_Down_Cnt_Init = (Sram_Read_Start*
			((mtk_dp->training_info.ubLinkRate*2700*8))) /
			((mtk_dp->info.DPTX_OUTBL.PixRateKhz*4));

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

	PixClkMhz =
		(mtk_dp->info.ubDPTXColorFormatSel == DP_COLOR_FORMAT_YUV_420) ?
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

	ColorBpp = mhal_DPTx_GetColorBpp(mtk_dp,
			mtk_dp->info.ubDPTXColorDepthSel,
			mtk_dp->info.ubDPTXColorFormatSel);
	PixRateMhz = mtk_dp->info.DPTX_OUTBL.PixRateKhz/1000;
	TU_size = (640*(PixRateMhz)*ColorBpp)/
			(mtk_dp->training_info.ubLinkRate * 27 *
				mtk_dp->training_info.ubLinkLaneCount * 8);

	NValue = TU_size / 10;
	FValue = TU_size-NValue * 10;

	DPTXMSG("TU_size %d,\n", TU_size);
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
	int ulPll_PixelRate; //Base = 1Khz
	int ulCalcPixelRate;

	mtk_dp->info.DPTX_VPLLx_N = 0x8000;
	mtk_dp->info.DPTX_Video_N  = 0x8000;

	ulPll_PixelRate = (0x00D8<<2)*10;

	if (mtk_dp->info.DPTX_OUTBL.FrameRate > 0) {
		ubTargetFrameRate = mtk_dp->info.DPTX_OUTBL.FrameRate;
		ulTargetPixelclk = (int)mtk_dp->info.DPTX_OUTBL.Htt*
			(int)mtk_dp->info.DPTX_OUTBL.Vtt*(int)ubTargetFrameRate;
	} else if (mtk_dp->info.DPTX_OUTBL.PixRateKhz > 0) {
		ubTargetFrameRate = 60;
		ulTargetPixelclk = mtk_dp->info.DPTX_OUTBL.PixRateKhz*1000;
	} else {
		ubTargetFrameRate = 60;
		ulTargetPixelclk = (int)mtk_dp->info.DPTX_OUTBL.Htt*
			(int)mtk_dp->info.DPTX_OUTBL.Vtt*(int)ubTargetFrameRate;
	}

	ulCalcPixelRate = ulTargetPixelclk / (100000);
	if (ulTargetPixelclk > 0) {
		mtk_dp->info.DPTX_VPLLx_M =
			((ulCalcPixelRate*mtk_dp->info.DPTX_VPLLx_N)/
			ulPll_PixelRate);

		mtk_dp->info.DPTX_OUTBL.PixRateKhz = ulTargetPixelclk / 1000;
	}

	if (mtk_dp->training_info.ubLinkRate > 0)
		mtk_dp->info.DPTX_Video_M =
			(ulCalcPixelRate*mtk_dp->info.DPTX_Video_N)/
			(mtk_dp->training_info.ubLinkRate * 270);
}

void mdrv_DPTx_Set_MISC(struct mtk_dp *mtk_dp)
{
	u8 format, depth;
	union MISC_T DPTX_MISC;

	format = mtk_dp->info.ubDPTXColorFormatSel;
	depth = mtk_dp->info.ubDPTXColorDepthSel;

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

	switch (mtk_dp->info.ubDPTXInPutTypeSel) {
	case DPTXInputSrc_PG:
		mdrv_DPTx_Set_MISC(mtk_dp);
		mhal_DPTx_PGEnable(mtk_dp, true);
		mhal_DPTx_Set_MVIDx2(mtk_dp, false);
		DPTXMSG("Using PG input video\n");
		break;

	case DPTXInputSrc_DPINTF:
		mhal_DPTx_PGEnable(mtk_dp, false);
		DPTXMSG("Using DPI input video\n");
		break;

	default: // PG case
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
		if (mtk_dp->training_state > DPTX_NTSTATE_TRAINING_PRE)
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

	DPTX_TIMING.Htt = cts_req.test_h_total;
	DPTX_TIMING.Hde = cts_req.test_h_width;
	DPTX_TIMING.Hfp = cts_req.test_h_total - cts_req.test_h_start -
		cts_req.test_h_width;
	DPTX_TIMING.Hsw = cts_req.test_hsync_width;
	DPTX_TIMING.bHsp = cts_req.test_hsync_polarity;
	DPTX_TIMING.Hbp = cts_req.test_h_start - cts_req.test_hsync_width;

	DPTX_TIMING.Vtt = cts_req.test_v_total;
	DPTX_TIMING.Vde = cts_req.test_v_height;
	DPTX_TIMING.Vfp = cts_req.test_v_total - cts_req.test_v_start -
		cts_req.test_v_height;
	DPTX_TIMING.Vsw = cts_req.test_vsync_width;
	DPTX_TIMING.bVsp = cts_req.test_vsync_polarity;
	DPTX_TIMING.Vbp = cts_req.test_v_start - cts_req.test_vsync_width;

	DPTX_TIMING.Hbk = DPTX_TIMING.Htt - DPTX_TIMING.Hde;
	DPTX_TIMING.FrameRate = cts_req.test_refresh_rate_numerator;//0;
	DPTX_TIMING.PixRateKhz = 0;
	DPTX_TIMING.Video_ip_mode = DPTX_VIDEO_PROGRESSIVE; // force P mode
	DPTXMSG("interlace = %d\n", cts_req.test_interlaced);

	DPTXMSG("Htt= %d, Hde = %d, Hfp = %d, Hsw = %d, Hbp = %d\n",
			DPTX_TIMING.Htt,
			DPTX_TIMING.Hde,
			DPTX_TIMING.Hfp,
			DPTX_TIMING.Hsw,
			DPTX_TIMING.Hbp);
	DPTXMSG("Vtt= %d, Vde = %d, Vfp = %d, Vsw = %d, Vbp = %d\n",
			DPTX_TIMING.Vtt,
			DPTX_TIMING.Vde,
			DPTX_TIMING.Vfp,
			DPTX_TIMING.Vsw,
			DPTX_TIMING.Vbp);
	//Clear MISC1&0 except [2:1]Colorfmt & [7:5]ColorDepth
	mhal_DPTx_SetMISC(mtk_dp, ucMISC);

	mtk_dp->info.ubDPTXColorFormatSel = cts_req.test_color_fmt;
	mtk_dp->info.ubDPTXColorDepthSel = cts_req.test_bit_depth;
	mtk_dp->info.DPTX_MISC.dp_misc.spec_def1 = cts_req.test_dynamic_range;

	mdrv_DPTx_LoadOutputTimingTBL(mtk_dp);
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


void mdrv_DPTx_CheckSinkHPDEvent(struct mtk_dp *mtk_dp)
{
	u8 ubDPCD20x[6];
	u8 ubDPCD2002[2];
	u8 ubDPCD200C[4];
	bool ret;

	memset(ubDPCD20x, 0x0, sizeof(ubDPCD20x));
	memset(ubDPCD2002, 0x0, sizeof(ubDPCD2002));
	memset(ubDPCD200C, 0x0, sizeof(ubDPCD200C));

	ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02002, ubDPCD2002, 0x2);
	if (!ret) {
		DPTXMSG("Read DPCD_02002 Fail\n");
		return;
	}

	ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_0200C, ubDPCD200C, 0x4);
	if (!ret) {
		DPTXMSG("Read DPCD_0200C Fail\n");
		return;
	}

	ret = drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, ubDPCD20x, 0x6);
	if (!ret) {
		DPTXMSG("Read DPCD200 Fail\n");
		return;
	}

	mdrv_DPTx_CheckSinkLock(mtk_dp, ubDPCD20x, ubDPCD200C);
	mdrv_DPTx_CheckSinkESI(mtk_dp, ubDPCD20x, ubDPCD2002);

	if ((ubDPCD2002[0x0] & 0x1F || ubDPCD20x[0x0] & 0x1F) &&
		(ubDPCD200C[0x2] & BIT6 || ubDPCD20x[0x4] & BIT6)) {
		DPTXMSG("New Branch Device Detection!!\n");

		mtk_dp->training_info.ucCheckCapTimes = 0;
		kfree(mtk_dp->edid);
		mtk_dp->edid = NULL;
		mtk_dp->training_state = DPTX_NTSTATE_CHECKEDID;
		mdelay(20);
	}
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

void mdrv_DPTx_SetPatternGenTimingTBL(struct mtk_dp *mtk_dp,
	u8 ucDPTx_PATTERN_NUM)
{
	u8 ucMISC[2] = {0x00};

	mtk_dp->info.ubDPTX_PatternIdx =
		(ucDPTx_PATTERN_NUM < DPTx_PATTERN_RGB_MAX) ?
		ucDPTx_PATTERN_NUM : DPTx_PATTERN_RGB_DEFINERES;

	DPTX_TIMING.Htt = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][12];
	DPTX_TIMING.Hde = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][3]<<2;
	DPTX_TIMING.Hfp = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][0]<<2;
	DPTX_TIMING.Hsw = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][1]<<2;
	DPTX_TIMING.bHsp = 0;
	DPTX_TIMING.Hbp = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][2]<<2;

	DPTX_TIMING.Vtt = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][16];
	DPTX_TIMING.Vde = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][7]-1;
	DPTX_TIMING.Vfp = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][4];
	DPTX_TIMING.Vsw = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][5];
	DPTX_TIMING.bVsp = 0;
	DPTX_TIMING.Vbp = gbPGSetting[mtk_dp->info.ubDPTX_PatternIdx][6];
	DPTX_TIMING.Hbk = DPTX_TIMING.Htt - DPTX_TIMING.Hde;
	DPTX_TIMING.FrameRate = 0;
	DPTX_TIMING.PixRateKhz = 0;
	DPTX_TIMING.Video_ip_mode = DPTX_VIDEO_PROGRESSIVE;

	DPTXMSG("%s, %d, mtk_dp 0x%p\n", __func__, __LINE__, mtk_dp);
	mhal_DPTx_SetMISC(mtk_dp, ucMISC);
}

void mdrv_DPTx_SetPatternGenMode(struct mtk_dp *mtk_dp,	u8 ucDPTx_PATTERN_NUM)
{
	mtk_dp->info.ubDPTXInPutTypeSel = DPTXInputSrc_PG;

	mtk_dp->info.ubDPTX_PatternIdx =
		(ucDPTx_PATTERN_NUM < DPTx_PATTERN_RGB_MAX) ?
		ucDPTx_PATTERN_NUM : DPTx_PATTERN_RGB_DEFINERES;

	if (!mtk_dp->info.bUseTopTimingTBL)
		mdrv_DPTx_SetPatternGenTimingTBL(mtk_dp, ucDPTx_PATTERN_NUM);

	mdrv_DPTx_LoadOutputTimingTBL(mtk_dp);

	DPTXFUNC("%d\n", ucDPTx_PATTERN_NUM);
}

void mdrv_DPTx_SetOutPutMode(struct mtk_dp *mtk_dp)
{
	switch (mtk_dp->info.ubDPTXInPutTypeSel) {
	case DPTXInputSrc_PG:
		mdrv_DPTx_SetPatternGenMode(mtk_dp,
			mtk_dp->info.ubDPTX_PatternIdx);
		mdrv_DPTx_SetDPTXOut(mtk_dp);
		DPTXMSG("Set Pattern Gen output\n");
		break;

	case DPTXInputSrc_DPINTF:
		mdrv_DPTx_SetDPTXOut(mtk_dp);
		DPTXMSG("Set dp_intf output\n");
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
			if (mtk_dp->bUsbPlug) {
				mtk_dp_hotplug_uevent(0);
				mtk_dp->bUsbPlug = false;
			} else
				DPTXMSG("Skip Uevent(0)\n");

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

void mdrv_DPTx_Print_TrainingState(int dpTx_ID, u8 state)
{
	switch (state) {
	case DPTX_NTSTATE_STARTUP:
		DPTXMSG("[DPTX%d] NTSTATE_STARTUP!\n", dpTx_ID);
		break;
	case DPTX_NTSTATE_CHECKCAP:
		DPTXMSG("[DPTX%d] NTSTATE_CHECKCAP!\n", dpTx_ID);
		break;
	case DPTX_NTSTATE_CHECKEDID:
		DPTXMSG("[DPTX%d] NTSTATE_CHECKEDID!\n", dpTx_ID);
		break;
	case DPTX_NTSTATE_TRAINING_PRE:
		DPTXMSG("[DPTX%d] NTSTATE_TRAINING_PRE!\n", dpTx_ID);
		break;
	case DPTX_NTSTATE_TRAINING:
		DPTXMSG("[DPTX%d] NTSTATE_TRAINING!\n", dpTx_ID);
		break;
	case DPTX_NTSTATE_CHECKTIMING:
		DPTXMSG("[DPTX%d] NTSTATE_CHECKTIMING!\n", dpTx_ID);
		break;
	case DPTX_NTSTATE_NORMAL:
		DPTXMSG("[DPTX%d] NTSTATE_NORMAL!\n", dpTx_ID);
		break;
	case DPTX_NTSTATE_POWERSAVE:
		DPTXMSG("[DPTX%d] NTSTATE_POWERSAVE!\n", dpTx_ID);
		break;
	case DPTX_NTSTATE_DPIDLE:
		DPTXMSG("[DPTX%d] NTSTATE_DPIDLE!\n", dpTx_ID);
		break;
	}
}

int mdrv_DPTx_set_reTraining(struct mtk_dp *mtk_dp)
{
	int ret = 0;

	if (mtk_dp->training_state > DPTX_NTSTATE_STARTUP)
		mtk_dp->training_state = DPTX_NTSTATE_STARTUP;

	if (mtk_dp->state > DPTXSTATE_INITIAL)
		mtk_dp->state = DPTXSTATE_INITIAL;

	mhal_DPTx_DigitalSwReset(mtk_dp);

	return ret;
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
			return DPTX_TRANING_STATE_CHANGE;

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
				mtk_dp->cr_done = true;
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
				mtk_dp->cr_done = false;
				mtk_dp->eq_done = false;
				break;
			}

			if (drm_dp_channel_eq_ok(ubTempValue,
				ubTargetLaneCount)) {
				mtk_dp->eq_done = true;
				bPassTPS2_3 = true;
				DPTXMSG("EQ Training Success\n");
				break;
			}
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

#ifdef DPTX_HDCP_ENABLE
void mdrv_DPTx_CheckHDCPVersion(struct mtk_dp *mtk_dp)
{
	if (g_hdcp_on) {
		if (mdrv_DPTx_HDCP2_Support(mtk_dp))
			return;

		if (mdrv_DPTx_HDCP1x_Support(mtk_dp))
			return;
	} else
		DPTXMSG("Not enable HDCP Function!\n");

	if (tee_addDevice(HDCP_NONE) != RET_SUCCESS)
		mtk_dp->info.bAuthStatus = AUTH_FAIL;
}
#endif

bool mdrv_DPTx_CheckSinkCap(struct mtk_dp *mtk_dp)
{
	u8 bTempBuffer[0x10];
	u8 ubDPCD_201;

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

#ifdef DPTX_HDCP_ENABLE
	mdrv_DPTx_CheckHDCPVersion(mtk_dp);
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

	drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, bTempBuffer, 0x2);
	ubDPCD_201 = bTempBuffer[0x1];
	if (!mtk_dp->training_info.bDPMstBranch) {
		if (ubDPCD_201 & BIT1) {
#if (DPTX_AutoTest_ENABLE == 0x1)
			mdrv_DPTx_PHY_AutoTest(mtk_dp, ubDPCD_201);
#endif
		}
	}

	return true;
}

unsigned int force_ch, force_fs, force_len;
unsigned int mdrv_DPTx_GetAudioCapability(struct mtk_dp *mtk_dp)
{
	struct cea_sad *sads;
	int sad_count, i, j;
	unsigned int config = 0;

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
				config |= ((1 << j) <<
					DP_CAPABILITY_CHANNEL_SFT) &
					(DP_CAPABILITY_CHANNEL_MASK <<
					DP_CAPABILITY_CHANNEL_SFT);
			config |= (sads[i].freq <<
				DP_CAPABILITY_SAMPLERATE_SFT) &
				(DP_CAPABILITY_SAMPLERATE_MASK <<
				DP_CAPABILITY_SAMPLERATE_SFT);
			config |= (sads[i].byte2 <<
				DP_CAPABILITY_BITWIDTH_SFT) &
				(DP_CAPABILITY_BITWIDTH_MASK <<
				DP_CAPABILITY_BITWIDTH_SFT);
		}
	}

	DPTXFUNC("Config:0x%x", config);
	return config;
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

		if (mtk_dp->training_info.bSinkEXTCAP_En)
			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_02002, ubTemp, 0x1);
		else
			drm_dp_dpcd_read(&mtk_dp->aux, DPCD_00200, ubTemp, 0x1);

		if ((ubTemp[0x0] & 0xBF) != 0) {
			mtk_dp->training_info.ubSinkCountNum = ubTemp[0x0]&0xBF;
			DPTXMSG("ExtSink Count = %d\n",
				mtk_dp->training_info.ubSinkCountNum);
		}
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

		mtk_dp->cr_done = false;
		mtk_dp->eq_done = false;

		mdrv_DPTx_TrainingChangeMode(mtk_dp);
		ret = mdrv_DPTx_TrainingFlow(mtk_dp, ubLinkRate, ubLaneCount);
		if (ret == DPTX_PLUG_OUT || ret == DPTX_TRANING_STATE_CHANGE)
			return ret;

		if (!mtk_dp->cr_done) {
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
		} else if (!mtk_dp->eq_done) {
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

	ret = mdrv_DPTx_HPD_HandleInThread(mtk_dp);

	if (!mtk_dp->training_info.bCablePlugIn)
		return DPTX_PLUG_OUT;

	if (mtk_dp->training_state == DPTX_NTSTATE_NORMAL)
		return ret;

	if (mtk_dp->training_state_pre != mtk_dp->training_state) {
		mdrv_DPTx_Print_TrainingState(mtk_dp->id,
			mtk_dp->training_state);

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

			mtk_dp->info.uiAudioConfig
				= mdrv_DPTx_GetAudioCapability(mtk_dp);
			if (mtk_dp->info.uiAudioConfig == 0)
				mtk_dp->info.bSetAudioMute = true;
			else
				mtk_dp->info.bSetAudioMute = false;
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
			mdrv_DPTx_OutPutMute(mtk_dp, true);
			mtk_dp->training_state = DPTX_NTSTATE_CHECKTIMING;
			mtk_dp->dp_ready = true;
			mhal_DPTx_EnableFEC(mtk_dp, mtk_dp->has_fec);
		} else if (ret != DPTX_TRANING_STATE_CHANGE)
			mtk_dp->training_state = DPTX_NTSTATE_DPIDLE;

		ret = DPTX_NOERR;
		break;

	case DPTX_NTSTATE_CHECKTIMING:
		mtk_dp->training_state = DPTX_NTSTATE_NORMAL;

#ifndef DPTX_HDCP_ENABLE
		if (mtk_dp->bUsbPlug) {
			mtk_dp_hotplug_uevent(1);
			mtk_dp->bUsbPlug = false;
		} else
			DPTXMSG("Skip Uevent(1)\n");
#endif
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
int mdrv_DPTx_HDCP_Handle(struct mtk_dp *mtk_dp)
{
	int ret = DPTX_NOERR;

	if (!mtk_dp->training_info.bCablePlugIn)
		return DPTX_PLUG_OUT;

	if (!mtk_dp->info.hdcp1x_info.bEnable
		&& !mtk_dp->info.hdcp2_info.bEnable) {
		if (mtk_dp->info.bAuthStatus == AUTH_FAIL) {
			if (mtk_dp->video_enable)
				mdrv_DPTx_OutPutMute(mtk_dp, true);

			ret = DPTX_AUTH_FAIL;
			return ret;
		} else
			return ret;
	}

	if (mtk_dp->info.hdcp1x_info.bEnable)
		mdrv_DPTx_HDCP1X_FSM(mtk_dp);
	else if (mtk_dp->info.hdcp2_info.bEnable)
		HDCPTx_Hdcp2FSM(mtk_dp);

	switch (mtk_dp->info.bAuthStatus) {
	case AUTH_INIT:
		if (mtk_dp->video_enable)
			mdrv_DPTx_OutPutMute(mtk_dp, true);
		break;
	case AUTH_ENCRYPT:
		if (mtk_dp->video_enable)
			mdrv_DPTx_OutPutMute(mtk_dp, false);
		break;
	case AUTH_FAIL:
		ret = DPTX_AUTH_FAIL;
		if (mtk_dp->video_enable)
			mdrv_DPTx_OutPutMute(mtk_dp, true);
		break;
	default:
		break;
	}

	return ret;
}
#endif

bool mdrv_DPTx_done(struct mtk_dp *mtk_dp)
{
	if (mtk_dp->state != DPTXSTATE_NORMAL)
		return false;

	if (mtk_dp->training_state != DPTX_NTSTATE_NORMAL)
		return false;

#ifdef DPTX_HDCP_ENABLE
	if ((mtk_dp->info.hdcp1x_info.bEnable
		|| mtk_dp->info.hdcp2_info.bEnable)
		&& (mtk_dp->info.bAuthStatus != AUTH_PASS))
		return false;
#endif

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
		mdrv_DPTx_OutPutMute(mtk_dp, true);
		mtk_dp->state = DPTXSTATE_IDLE;
		break;

	case DPTXSTATE_IDLE:
		if (mtk_dp->training_state == DPTX_NTSTATE_NORMAL) {
#ifdef DPTX_HDCP_ENABLE
			mtk_dp->state = DPTXSTATE_HDCP_AUTH;
			if (mtk_dp->info.hdcp2_info.bEnable)
				mdrv_DPTx_HDCP2_SetStartAuth(mtk_dp, true);
			else if (mtk_dp->info.hdcp1x_info.bEnable)
				mdrv_DPTx_HDCP1X_SetStartAuth(mtk_dp, true);
			else
				mtk_dp->state = DPTXSTATE_PREPARE;
#else
			mtk_dp->state = DPTXSTATE_PREPARE;
#endif
		}

		break;

#ifdef DPTX_HDCP_ENABLE
	case DPTXSTATE_HDCP_AUTH:
		if (mtk_dp->info.hdcp1x_info.bEnable
			|| mtk_dp->info.hdcp2_info.bEnable) {
			if (mtk_dp->info.bAuthStatus == AUTH_ENCRYPT)
				mtk_dp->state = DPTXSTATE_PREPARE;
		}  else
			mtk_dp->state = DPTXSTATE_PREPARE;
		break;
#endif
	case DPTXSTATE_PREPARE:
#ifdef DPTX_HDCP_ENABLE
		if (mtk_dp->bUsbPlug) {
			mtk_dp_hotplug_uevent(1);
			mtk_dp->bUsbPlug = false;
		} else
			DPTXMSG("Skip Uevent(1)\n");
#endif
		if (mtk_dp->video_enable) {
			mtk_dp_video_config(mtk_dp, mtk_dp->info.uiVideoConfig);
			mdrv_DPTx_Video_Enable(mtk_dp, true);
		}

		mtk_dp->state = DPTXSTATE_NORMAL;
		break;

	case DPTXSTATE_NORMAL:
		if (mtk_dp->training_state != DPTX_NTSTATE_NORMAL) {
			mdrv_DPTx_OutPutMute(mtk_dp, true);
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

void mdrv_DPTx_HPD_ISREvent(struct mtk_dp *mtk_dp)
{
	u16 ubSWStatus = mhal_DPTx_GetSWIRQStatus(mtk_dp);

	mtk_dp->irq_status = ubSWStatus;
	mtk_dp->training_info.usPHY_STS |= ubSWStatus;
	DPTXMSG("SW status = 0x%x\n", ubSWStatus);

	mdrv_DPTx_HPD_HandleInISR(mtk_dp);

	if (ubSWStatus)
		mhal_DPTx_SWInterruptClr(mtk_dp, ubSWStatus);

	if (mtk_dp->training_info.bCableStateChange
		|| ubSWStatus == HPD_INT_EVNET)
		wake_up(&mtk_dp->irq_wq);
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
	mhal_DPTx_PHYSetting(mtk_dp);
	mhal_DPTx_HPDDetectSetting(mtk_dp);

	mhal_DPTx_DigitalSwReset(mtk_dp);
	//mhal_DPTx_Set_Efuse_Value(mtk_dp);
}

void mdrv_DPTx_OutPutMute(struct mtk_dp *mtk_dp, bool bEnable)
{
	mdrv_DPTx_VideoMute(mtk_dp, bEnable);
	mdrv_DPTx_AudioMute(mtk_dp, bEnable);
	mhal_DPTx_VideoMuteSW(mtk_dp, bEnable);
}

void mdrv_DPTx_Video_Enable(struct mtk_dp *mtk_dp, bool bEnable)
{
	DPTXMSG("Output Video %s!\n", bEnable ? "enable" : "disable");

	if (bEnable) {
		mtk_dp->info.ubDPTXInPutTypeSel = DPTXInputSrc_DPINTF;
		mdrv_DPTx_SetOutPutMode(mtk_dp);
		mdrv_DPTx_OutPutMute(mtk_dp, false);
		mhal_DPTx_Verify_Clock(mtk_dp);
	} else {
		mdrv_DPTx_OutPutMute(mtk_dp, true);
	}
}

void mdrv_DPTx_Set_Color_Format(struct mtk_dp *mtk_dp, u8 ucColorFormat)
{
	DPTXMSG("Set Color Format = 0x%x\n", ucColorFormat);

	mtk_dp->info.ubDPTXColorFormatSel = ucColorFormat;
	mhal_DPTx_SetColorFormat(mtk_dp, ucColorFormat);
}

void mdrv_DPTx_Set_Color_Depth(struct mtk_dp *mtk_dp, u8 ucColorDepth)
{
	DPTXMSG("Set Color Depth = %d (1~4=6/8/10/12bpp)\n", ucColorDepth + 1);

	mtk_dp->info.ubDPTXColorDepthSel = ucColorDepth;
	mhal_DPTx_SetColorDepth(mtk_dp, ucColorDepth);
}

void mdrv_DPTx_Set_MSA(struct mtk_dp *mtk_dp, void *ucInput)
{
	struct DPTX_TIMING_PARAMETER *ucOUTBL
		= (struct DPTX_TIMING_PARAMETER *) ucInput;

	mtk_dp->info.DPTX_OUTBL = *ucOUTBL;
	mhal_DPTx_SetMSA(mtk_dp, ucOUTBL);
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

void mdrv_DPTx_I2S_Audio_Config(struct mtk_dp *mtk_dp, u8 ucChannel, u8 ucFs)
{
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

	DPTXMSG("DSC enable = %d\n", mtk_dp->has_dsc);
#endif
}

void mdrv_DPTx_FEC_Ready(struct mtk_dp *mtk_dp, u8 err_cnt_sel)
{
	u8 i, Data[3];

	drm_dp_dpcd_read(&mtk_dp->aux, 0x90, Data, 0x1);
	DPTXMSG("FEC Capable[0], [0:3] should be 1: 0x%x\n", Data[0]);

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
			DPTXMSG("FEC status & error Count: 0x%x\n", Data[i]);
	}
}

void mapi_DPTx_Set_MISC(struct mtk_dp *mtk_dp, u8 format, u8 depth)
{
	mtk_dp->info.ubDPTXColorFormatSel = format;
	mtk_dp->info.ubDPTXColorDepthSel = depth;
	mdrv_DPTx_Set_MISC(mtk_dp);
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

static int mtk_dp_kthread_init(void *data)
{
	struct mtk_dp *mtk_dp = data;
	unsigned long long starttime = 0;

	init_waitqueue_head(&mtk_dp->irq_wq);

	while (!kthread_should_stop()) {
		wait_event_interruptible(mtk_dp->irq_wq,
			(mtk_dp->irq_status != 0));
		mtk_dp->irq_status = 0;

		starttime = sched_clock();

		do {
			if (abs(sched_clock() - starttime) > 30000000000ULL) {
				DPTXMSG("Link training fail, over 30s\n");
				break;
			}

			if (mdrv_DPTx_Training_Handler(mtk_dp) != DPTX_NOERR)
				break;

			if (mdrv_DPTx_Handle(mtk_dp) != DPTX_NOERR)
				break;
#ifdef DPTX_HDCP_ENABLE
			if (mdrv_DPTx_HDCP_Handle(mtk_dp) != DPTX_NOERR)
				break;
#endif
		} while (!mdrv_DPTx_done(mtk_dp));
	}

	return 0;
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


void mtk_dp_video_config(struct mtk_dp *mtk_dp, unsigned int config)
{
	struct DPTX_TIMING_PARAMETER DPTX_TBL;
	u8 color_depth;
	u8 color_format;
	u8 video_timing;
	u32 mvid = 0;

	if (!mtk_dp->dp_ready) {
		DPTXERR("%s, DP is not ready!\n", __func__);
		return;
	}

	video_timing = (config & DP_VIDEO_TIMING_MASK) >> DP_VIDEO_TIMING_SFT;
	mhal_DPTx_OverWrite_MN(mtk_dp, false, mvid, 0x8000);

	switch (video_timing) {
	case SINK_8K4K60R:
		DPTX_TBL.FrameRate = 60;
		DPTX_TBL.Htt = 8040; DPTX_TBL.Hbp = 240; DPTX_TBL.Hsw = 96;
		DPTX_TBL.bHsp = 0; DPTX_TBL.Hfp = 24; DPTX_TBL.Hde = 7680;
		DPTX_TBL.Vtt = 4381; DPTX_TBL.Vbp = 6; DPTX_TBL.Vsw = 8;
		DPTX_TBL.bVsp = 0; DPTX_TBL.Vfp = 47; DPTX_TBL.Vde = 4320;
		break;
	case SINK_4K2K60R:
		DPTX_TBL.FrameRate = 60;
		mtk_dp->dsc_enable = true;
		DPTX_TBL.Htt = 4400; DPTX_TBL.Hbp = 296; DPTX_TBL.Hsw = 88;
		DPTX_TBL.bHsp = 0; DPTX_TBL.Hfp = 176; DPTX_TBL.Hde = 3840;
		DPTX_TBL.Vtt = 2250; DPTX_TBL.Vbp = 72; DPTX_TBL.Vsw = 10;
		DPTX_TBL.bVsp = 0; DPTX_TBL.Vfp = 8; DPTX_TBL.Vde = 2160;
		break;
	case SINK_4K2K30:
		DPTX_TBL.FrameRate = 30;
		mtk_dp->dsc_enable = false;
		DPTX_TBL.Htt = 4400; DPTX_TBL.Hbp = 296; DPTX_TBL.Hsw = 88;
		DPTX_TBL.bHsp = 0; DPTX_TBL.Hfp = 176; DPTX_TBL.Hde = 3840;
		DPTX_TBL.Vtt = 2250; DPTX_TBL.Vbp = 72; DPTX_TBL.Vsw = 10;
		DPTX_TBL.bVsp = 0; DPTX_TBL.Vfp = 8; DPTX_TBL.Vde = 2160;
		break;
	case SINK_2K2K60:
		DPTX_TBL.FrameRate = 60;
		mtk_dp->dsc_enable = false;
		DPTX_TBL.Htt = 2080; DPTX_TBL.Hbp = 44; DPTX_TBL.Hsw = 44;
		DPTX_TBL.bHsp = 0; DPTX_TBL.Hfp = 72; DPTX_TBL.Hde = 1920;
		DPTX_TBL.Vtt = 2250; DPTX_TBL.Vbp = 3; DPTX_TBL.Vsw = 5;
		DPTX_TBL.bVsp = 0; DPTX_TBL.Vfp = 82; DPTX_TBL.Vde = 2160;
		break;
	case SINK_1080P60:
		DPTX_TBL.FrameRate = 60;
		mtk_dp->dsc_enable = false;
		DPTX_TBL.Htt = 2200; DPTX_TBL.Hbp = 148; DPTX_TBL.Hsw = 44;
		DPTX_TBL.bHsp = 0; DPTX_TBL.Hfp = 88; DPTX_TBL.Hde = 1920;
		DPTX_TBL.Vtt = 1125; DPTX_TBL.Vbp = 36; DPTX_TBL.Vsw = 5;
		DPTX_TBL.bVsp = 0; DPTX_TBL.Vfp = 4; DPTX_TBL.Vde = 1080;
		break;
	case SINK_1200P60_1920:
		DPTX_TBL.FrameRate = 60;
		mtk_dp->dsc_enable = false;
		DPTX_TBL.Htt = 2080; DPTX_TBL.Hbp = 80; DPTX_TBL.Hsw = 32;
		DPTX_TBL.bHsp = 0; DPTX_TBL.Hfp = 48; DPTX_TBL.Hde = 1920;
		DPTX_TBL.Vtt = 1235; DPTX_TBL.Vbp = 26; DPTX_TBL.Vsw = 6;
		DPTX_TBL.bVsp = 0; DPTX_TBL.Vfp = 3; DPTX_TBL.Vde = 1200;
		break;
	case SINK_1080P60_2460:
		DPTX_TBL.FrameRate = 60;
		mtk_dp->dsc_enable = false;
		DPTX_TBL.Htt = 1172; DPTX_TBL.Hbp = 30; DPTX_TBL.Hsw = 32;
		DPTX_TBL.bHsp = 1; DPTX_TBL.Hfp = 30; DPTX_TBL.Hde = 1080;
		DPTX_TBL.Vtt = 2476; DPTX_TBL.Vbp = 5; DPTX_TBL.Vsw = 2;
		DPTX_TBL.bVsp = 0; DPTX_TBL.Vfp = 9; DPTX_TBL.Vde = 2460;
		break;
	case SINK_720P60:
		DPTX_TBL.FrameRate = 60;
		mtk_dp->dsc_enable = false;
		DPTX_TBL.Htt = 1650; DPTX_TBL.Hbp = 220; DPTX_TBL.Hsw = 40;
		DPTX_TBL.bHsp = 0; DPTX_TBL.Hfp = 110; DPTX_TBL.Hde = 1280;
		DPTX_TBL.Vtt = 750; DPTX_TBL.Vbp = 20; DPTX_TBL.Vsw = 5;
		DPTX_TBL.bVsp = 0; DPTX_TBL.Vfp = 5; DPTX_TBL.Vde = 720;
		break;
	case SINK_480P:
	default:
		DPTX_TBL.FrameRate = 60;
		mtk_dp->dsc_enable = false;
		DPTX_TBL.Htt = 800; DPTX_TBL.Hbp = 48; DPTX_TBL.Hsw = 96;
		DPTX_TBL.bHsp = 1; DPTX_TBL.Hfp = 16; DPTX_TBL.Hde = 640;
		DPTX_TBL.Vtt = 525; DPTX_TBL.Vbp = 33; DPTX_TBL.Vsw = 2;
		DPTX_TBL.bVsp = 1; DPTX_TBL.Vfp = 10; DPTX_TBL.Vde = 480;
		break;
	}

	if (fakecablein && video_timing == SINK_720P60) {
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
		mhal_DPTx_OverWrite_MN(mtk_dp, true, mvid, 0x8000);
	}

	if (video_timing == SINK_4K2K60R) {
		// patch for 4k@60 with DSC 3 times compress
		switch (mtk_dp->training_info.ubLinkRate) {
		case DP_LINKRATE_HBR3:
			mvid = 0x5DDE;
			break;
		case DP_LINKRATE_HBR2:
			mvid = 0x8CCD;
			break;
		}
		mhal_DPTx_OverWrite_MN(mtk_dp, true, mvid, 0x8000);
	}

	if (mtk_dp->has_dsc) {
		uint8_t Data[1];

		Data[0] = (u8) mtk_dp->dsc_enable;
		drm_dp_dpcd_write(&mtk_dp->aux, 0x160, Data, 0x1);
	}


	DPTX_TBL.PixRateKhz =
		DPTX_TBL.Htt * DPTX_TBL.Vtt * DPTX_TBL.FrameRate / 1000;
	//interlace not support
	DPTX_TBL.Video_ip_mode = DPTX_VIDEO_PROGRESSIVE;
	mdrv_DPTx_Set_MSA(mtk_dp, (void *)&DPTX_TBL);

	color_depth = (config & DP_COLOR_DEPTH_MASK) >> DP_COLOR_DEPTH_SFT;
	color_format = (config & DP_COLOR_FORMAT_MASK) >> DP_COLOR_FORMAT_SFT;
	mapi_DPTx_Set_MISC(mtk_dp, color_format, color_depth);

	if (!mtk_dp->dsc_enable) {
		mdrv_DPTx_Set_Color_Depth(mtk_dp, color_depth);
		mdrv_DPTx_Set_Color_Format(mtk_dp, color_format);
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

	g_mtk_dp->irq_status = 1;
	wake_up_interruptible(&g_mtk_dp->irq_wq);
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
	unsigned int format = 0;

	init_waitqueue_head(&mtk_dp->control_wq);

	while (!kthread_should_stop()) {
		wait_event_interruptible(mtk_dp->control_wq,
			atomic_read(&dp_comm_event));

		videomute = atomic_read(&dp_comm_event) >> 16;
		res = atomic_read(&dp_comm_event) & 0xff;
		atomic_set(&dp_comm_event, 0);

		format = res | fakebpc << DP_COLOR_DEPTH_SFT
			| DP_COLOR_FORMAT_RGB_444 << DP_COLOR_FORMAT_SFT;

		if (videomute & video_unmute) {
			if (!fakecablein && mtk_dp->state > DPTXSTATE_PREPARE)
				mtk_dp->state = DPTXSTATE_PREPARE;

			mtk_dp->video_enable = true;
			mtk_dp->info.uiVideoConfig = format;
			mtk_dp->irq_status = 1;
			wake_up_interruptible(&mtk_dp->irq_wq);

		} else if (videomute & video_mute) {
			mtk_dp->video_enable = false;
			if (!mtk_dp->dp_ready)
				continue;

			mdrv_DPTx_Video_Enable(mtk_dp, false);
		}
	}

	return 0;
}

#define NOTIFY_OFFSET 0xF0
atomic_t dp_notify_event = ATOMIC_INIT(0);
static unsigned long long last_event_ts;
static unsigned int last_event;

static int mtk_dp_notify_kthread(void *data)
{
	struct mtk_dp *mtk_dp = data;
	int event = 0;
	unsigned long long ts_diff;

	init_waitqueue_head(&mtk_dp->notify_wq);
	while (!kthread_should_stop()) {
		wait_event_interruptible(mtk_dp->notify_wq,
			atomic_read(&dp_notify_event));
		event = atomic_read(&dp_notify_event) - NOTIFY_OFFSET;
		atomic_set(&dp_notify_event, 0);

		DPTXMSG("fake:%d ;event: %d -> %d\n",
			fakecablein, last_event, event);

		if (last_event == event) {
			DPTXMSG("Same event!");
			mutex_unlock(&dp_lock);
			continue;
		}

		ts_diff = abs(sched_clock() - last_event_ts);
		if (ts_diff < 1000000000ULL) /* 1s */
			msleep((1000000000ULL - ts_diff)/1000000);

		last_event_ts = sched_clock();
		last_event = event;

		if (event > 0) {
			notify_uevent_user(&dptx_notify_data,
				DPTX_STATE_ACTIVE);
		} else {
			notify_uevent_user(&dptx_notify_data,
				DPTX_STATE_NO_DEVICE);
		}

		if (!g_mtk_dp->info.bSetAudioMute)
			extcon_set_state_sync(dptx_extcon, EXTCON_DISP_HDMI,
				event > 0 ? true : false);
	}

	return 0;
}


int mtk_drm_dp_get_dev_info(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_dispif_info *info = data;
	struct mtk_dp *mtk_dp = g_mtk_dp;

	info->display_id = mtk_dp->id;
	info->displayFormat = mtk_dp->info.ubDPTXColorFormatSel;
	info->displayHeight = mtk_dp->info.DPTX_OUTBL.Vde;
	info->displayWidth = mtk_dp->info.DPTX_OUTBL.Hde;
	info->displayMode = DISPIF_MODE_VIDEO;
	info->displayType = DISPLAYPORT;
	info->isConnected = (mtk_dp->state == DPTXSTATE_NORMAL) ? true : false;
	info->isHwVsyncAvailable = true;
	info->vsyncFPS = 6000; //should equals to frame rate * 100
	DPTXMSG("%s, %d, fake %d\n", __func__, __LINE__, fakecablein);

	return 0;
}

int mtk_drm_dp_audio_enable(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	unsigned int *audio_enable = data;
	struct mtk_dp *mtk_dp = g_mtk_dp;

	if (!mtk_dp->dp_ready) {
		DPTXMSG("%s, DP is not ready!\n", __func__);
		return 0;
	}

	DPTXFUNC("audio_enable 0x%x\n", *audio_enable);

	mdrv_DPTx_I2S_Audio_Enable(mtk_dp, (*audio_enable == 1));

	return 0;
}

int mtk_drm_dp_audio_config(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	unsigned int *audio_config = data;
	struct mtk_dp *mtk_dp = g_mtk_dp;
	u8 ucChannel = (*audio_config >> DP_CAPABILITY_CHANNEL_SFT) &
		DP_CAPABILITY_CHANNEL_MASK;
	u8 ucFs = (*audio_config >> DP_CAPABILITY_SAMPLERATE_SFT) &
		DP_CAPABILITY_SAMPLERATE_MASK;
	u8 ucWordlength = (*audio_config >> DP_CAPABILITY_BITWIDTH_SFT) &
		DP_CAPABILITY_BITWIDTH_MASK;

	if (!mtk_dp->dp_ready) {
		DPTXFUNC("DP is not ready!\n");
		return 0;
	}

	DPTXFUNC("audio_config 0x%x\n", *audio_config);

	if (fakecablein) {
		ucChannel = BIT(force_ch);
		ucFs = BIT(force_fs);
		ucWordlength = BIT(force_len);
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
	mdrv_DPTx_I2S_Audio_Config(mtk_dp, ucChannel, ucFs);

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
		*dp_cap = g_mtk_dp->info.uiAudioConfig;

	if (*dp_cap == 0)
		*dp_cap = ((DP_CHANNEL_2 << DP_CAPABILITY_CHANNEL_SFT)
			| (DP_SAMPLERATE_192 << DP_CAPABILITY_SAMPLERATE_SFT)
			| (DP_BITWIDTH_24 << DP_CAPABILITY_BITWIDTH_SFT));

	DPTXFUNC("dp_cap 0x%x\n", *dp_cap);
	return 0;

}

int mtk_drm_dp_get_info(struct drm_device *dev,
		struct drm_mtk_session_info *info)
{
	DPTXDBG("%s, %d\n", __func__, __LINE__);
	info->physicalWidthUm = 900;
	info->physicalHeightUm = 1000;
	info->vsyncFPS = 6000;

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

static int mtk_dp_dt_parse_pdata(struct mtk_dp *mtk_dp,
		struct platform_device *pdev)
{
	struct resource regs;
	struct device *dev = &pdev->dev;
	int ret = 0;

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

	DPTXMSG("reg and clock get success!\n");
error:
	return 0;
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
	///struct mtk_dp *mtk_dp = mtk_dp_ctx_from_conn(conn);

	//mtk_dp_set_hpd_event(mtk_dp->dev, NULL, NULL);
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

		if (fakeres == SINK_4K2K60R)
			mode = drm_mode_duplicate(dev, &dptx_est_modes[0]);
		else if (fakeres == SINK_4K2K30)
			mode = drm_mode_duplicate(dev, &dptx_est_modes[1]);
		else if (fakeres == SINK_1080P60)
			mode = drm_mode_duplicate(dev, &dptx_est_modes[2]);
		else if (fakeres == SINK_720P60)
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

static int mtk_dp_conn_mode_valid(struct drm_connector *conn,
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
			mode->clock * 1000);

	return drm_mode_validate_size(mode, 0x1fff, 0x1fff);
}

/*
 *  static struct drm_encoder *mtk_dp_conn_best_enc(struct drm_connector *conn)
 *  {
 *  struct mtk_dp *mtk_dp = mtk_dp_ctx_from_conn(conn);
 *  return mtk_dp->conn.encoder;
 *  }
 */

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

void mtk_dp_set_adjust_phy(uint8_t c0, uint8_t cp1)
{
	g_c0 = c0;
	g_cp1 = cp1;
}

void mtk_dp_hotplug_uevent(unsigned int event)
{
	DPTXFUNC("fake:%d, event:%d\n", fakecablein, event);
	atomic_set(&dp_notify_event, event + NOTIFY_OFFSET);
	wake_up_interruptible(&g_mtk_dp->notify_wq);
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
	if (g_mtk_dp->bUsbPlug == true) {
		mdrv_DPTx_VideoMute(g_mtk_dp, true);
		mdrv_DPTx_AudioMute(g_mtk_dp, true);
	}

	fakeres = FAKE_DEFAULT_RES;
	fakebpc = DP_COLOR_DEPTH_8BIT;

	mtk_dp_hotplug_uevent(0x0);
	kfree(g_mtk_dp->edid);
	g_mtk_dp->edid = NULL;

	if (fakeres == SINK_4K2K60R)
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
			mhal_DPTx_AnalogPowerOnOff(g_mtk_dp, true);
			mhal_DPTx_USBC_HPD(g_mtk_dp, true);
			g_mtk_dp->bPowerOn = true;
		} else if (bstatus == HPD_DISCONNECT)
			mhal_DPTx_USBC_HPD(g_mtk_dp, false);


		mhal_DPTx_SWInterruptEnable(g_mtk_dp, true);
		mhal_DPTx_SWInterruptSet(g_mtk_dp, bstatus);
		return;
	}

	if (!g_mtk_dp->bPowerOn && bstatus == HPD_DISCONNECT) {
		DPTXMSG("System is sleeping, Plug Out\n");
		mtk_dp_hotplug_uevent(0);
	}
}

void mtk_dp_SWInterruptSet(int bstatus)
{
	mutex_lock(&dp_lock);
	if ((bstatus == HPD_CONNECT && !g_mtk_dp->bPowerOn)
		|| (bstatus == HPD_DISCONNECT && g_mtk_dp->bPowerOn)) {
		g_mtk_dp->bUsbPlug = true;
	}

	mtk_dp_HPDInterruptSet(bstatus);
	mutex_unlock(&dp_lock);
}

void mtk_dp_poweroff(void)
{
	DPTXFUNC();
	mutex_lock(&dp_lock);
	mtk_dp_HPDInterruptSet(HPD_DISCONNECT);
	mutex_unlock(&dp_lock);
}

void mtk_dp_poweron(void)
{
	DPTXFUNC();
	mutex_lock(&dp_lock);
	mtk_dp_HPDInterruptSet(HPD_CONNECT);
	mutex_unlock(&dp_lock);
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
		goto err_init_connector;
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
	kfree(&mtk_dp->conn);

err_init_connector:
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

	/* create dptx thread */
	mtk_dp->task = kthread_run(mtk_dp_kthread_init,
		(void *)mtk_dp, "mtk_dp_kthread");
	if (IS_ERR(mtk_dp->task))
		DPTXERR("=====create kthread failed!=====\n");

	mtk_dp->control_task = kthread_run(mtk_dp_control_kthread,
		(void *)mtk_dp, "mtk_dp_control_kthread");

	mtk_dp->notify_task = kthread_run(mtk_dp_notify_kthread,
		(void *)mtk_dp, "mtk_dp_notify_kthread");

	return component_add(&pdev->dev, &mtk_dp_component_ops);

error:
	return -EPROBE_DEFER;
}

static int mtk_drm_dp_remove(struct platform_device *pdev)
{
	struct mtk_dp *mtk_dp = platform_get_drvdata(pdev);
	int ret;

	mdrv_DPTx_OutPutMute(mtk_dp, true);

	drm_connector_cleanup(&mtk_dp->conn);

	if (!IS_ERR(mtk_dp->task)) {
		ret = kthread_stop(mtk_dp->task);
		DPTXMSG("=====thread function has stop %ds======\n", ret);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_dp_suspend(struct device *dev)
{
	DPTXFUNC();

	mutex_lock(&dp_lock);
	if (g_mtk_dp->bPowerOn) {
		mtk_dp_HPDInterruptSet(HPD_DISCONNECT);
		mdelay(5);
	}
	mutex_unlock(&dp_lock);
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


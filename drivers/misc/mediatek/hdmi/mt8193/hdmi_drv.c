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

/*----------------------------------------------------------------------------*/
#ifdef HDMI_MT8193_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <mt-plat/mt_gpio.h>

#include "mt8193_ctrl.h"
#include "mt8193ddc.h"
#include "mt8193hdcp.h"
#include "hdmi_drv.h"
#include "mt8193_iic.h"
#include "mt8193avd.h"
#include "mt8193hdmicmd.h"

#include "extd_factory.h"
#include "ddp_hal.h"
#include "extd_hdmi.h"
#include "mt_boot_common.h"
/*----------------------------------------------------------------------------*/
/* Debug message defination */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* HDMI Timer */
/*----------------------------------------------------------------------------*/

static struct timer_list r_hdmi_timer;
static struct timer_list r_cec_timer;

static uint32_t gHDMI_CHK_INTERVAL = 10;
static uint32_t gCEC_CHK_INTERVAL = 20;

unsigned int mt8193_log_on = hdmideflog;
unsigned int mt8193_cec_on = 0;
unsigned int mt8193_cec_interrupt = 0;
unsigned int mt8193_cecinit = 0;
unsigned char mt8193_hdmiinit = 0;
unsigned char mt8193_hotinit = 0;
unsigned char hdmi_powerenable = 0xff;

unsigned char is_hdmi_plug_out_flag = 0;

unsigned char mt8193_hdmipoweroninit = 0;
size_t mt8193_TmrValue[MAX_HDMI_TMR_NUMBER] = { 0 };

size_t mt8193_hdmiCmd = 0xff;
size_t mt8193_rxcecmode = CEC_NORMAL_MODE;
HDMI_CTRL_STATE_T e_hdmi_ctrl_state = HDMI_STATE_IDLE;
HDCP_CTRL_STATE_T e_hdcp_ctrl_state = HDCP_RECEIVER_NOT_READY;
unsigned int mt8193_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;

#if defined(CONFIG_HAS_EARLYSUSPEND)
size_t mt8193_hdmiearlysuspend = 1;
#endif

static struct task_struct *hdmi_timer_task;
wait_queue_head_t hdmi_timer_wq;
atomic_t hdmi_timer_event = ATOMIC_INIT(0);

static struct task_struct *cec_timer_task;
wait_queue_head_t cec_timer_wq;
atomic_t cec_timer_event = ATOMIC_INIT(0);

static struct task_struct *mt8193_nlh_task;
wait_queue_head_t mt8193_nlh_wq;	/* NFI, LVDS, HDMI */
atomic_t mt8193_nlh_event = ATOMIC_INIT(0);

static struct HDMI_UTIL_FUNCS hdmi_util = { 0 };

static int hdmi_timer_kthread(void *data);
static int cec_timer_kthread(void *data);
static int mt8193_nlh_kthread(void *data);

void (*mt8193_hdmi_factory_callback)(enum HDMI_STATE state);
void (*mt8193_hdmi_unfactory_callback)(enum HDMI_STATE state);

static void vInitAvInfoVar(void)
{
	_stAvdAVInfo.e_resolution = HDMI_VIDEO_1280x720p_60Hz;
	_stAvdAVInfo.fgHdmiOutEnable = TRUE;
	_stAvdAVInfo.fgHdmiTmdsEnable = TRUE;

	_stAvdAVInfo.bMuteHdmiAudio = FALSE;
	_stAvdAVInfo.e_video_color_space = HDMI_RGB;
	_stAvdAVInfo.e_deep_color_bit = HDMI_NO_DEEP_COLOR;
	_stAvdAVInfo.ui1_aud_out_ch_number = 2;
	_stAvdAVInfo.e_hdmi_fs = HDMI_FS_44K;

	_stAvdAVInfo.bhdmiRChstatus[0] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[1] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[2] = 0x02;
	_stAvdAVInfo.bhdmiRChstatus[3] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[4] = 0x00;
	_stAvdAVInfo.bhdmiRChstatus[5] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[0] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[1] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[2] = 0x02;
	_stAvdAVInfo.bhdmiLChstatus[3] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[4] = 0x00;
	_stAvdAVInfo.bhdmiLChstatus[5] = 0x00;

	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);

}

void vSetHDMIMdiTimeOut(unsigned int i4_count)
{
	MT8193_DRV_FUNC();
	mt8193_TmrValue[HDMI_PLUG_DETECT_CMD] = i4_count;

}

/*----------------------------------------------------------------------------*/

static void mt8193_set_util_funcs(const struct HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(struct HDMI_UTIL_FUNCS));
}

/*----------------------------------------------------------------------------*/

static void mt8193_get_params(struct HDMI_PARAMS *params)
{
	enum HDMI_VIDEO_RESOLUTION input_resolution;

	input_resolution = params->init_config.vformat;
	memset(params, 0, sizeof(struct HDMI_PARAMS));

	switch (input_resolution) {
	case HDMI_VIDEO_720x480p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_RISING;
		params->vsync_pol = HDMI_POLARITY_RISING;
		params->hsync_pulse_width = 62;
		params->hsync_back_porch  = 60;
		params->hsync_front_porch = 16;
		params->vsync_pulse_width = 6;
		params->vsync_back_porch  = 30;
		params->vsync_front_porch = 9;
		params->width = 720;
		params->height = 480;
		params->input_clock = 27027;
		params->init_config.vformat = HDMI_VIDEO_720x480p_60Hz;
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 40;
		params->hsync_back_porch  = 220;
		params->hsync_front_porch = 110;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch  = 20;
		params->vsync_front_porch = 5;
		params->width = 1280;
		params->height = 720;
		params->input_clock = 74250;
		params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
		break;
	case HDMI_VIDEO_1920x1080p_30Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 44;
		params->hsync_back_porch  = 148;
		params->hsync_front_porch = 88;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch  = 36;
		params->vsync_front_porch = 4;
		params->width = 1920;
		params->height = 1080;
		params->input_clock = 74250;
		params->init_config.vformat = HDMI_VIDEO_1920x1080p_30Hz;
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 44;
		params->hsync_back_porch  = 148;
		params->hsync_front_porch = 88;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch  = 36;
		params->vsync_front_porch = 4;
		params->width = 1920;
		params->height = 1080;
		params->input_clock = 148500;
		params->init_config.vformat = HDMI_VIDEO_1920x1080p_60Hz;
		break;
	default:
		HDMI_DEF_LOG("Unknown support resolution\n");
		break;
	}

	params->init_config.aformat = HDMI_AUDIO_44K_2CH;
	params->rgb_order = HDMI_COLOR_ORDER_RGB;
	params->io_driving_current = IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num = 4;
	params->scaling_factor = 0;
	params->cabletype = 0;
	params->HDCPSupported = 0;
	params->is_force_awake = 1;

}


static int mt8193_enter(void)
{
	MT8193_DRV_FUNC();
	return 0;

}

static int mt8193_exit(void)
{
	MT8193_DRV_FUNC();
	return 0;
}

/*----------------------------------------------------------------------------*/

static void mt8193_suspend(void)
{
	MT8193_DRV_FUNC();

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
}

/*----------------------------------------------------------------------------*/

static void mt8193_resume(void)
{
	MT8193_DRV_FUNC();

}

/*----------------------------------------------------------------------------*/

static int mt8193_video_config(enum HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin,
			       int vout)
{
	HDMI_DEF_LOG("[hdmi]mt8193_video_config:%d\n", vformat);

	_stAvdAVInfo.e_resolution = vformat;

	vSetHDMITxPLLTrigger();
	vResetHDMIPLL();

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_VPLL, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_SOFT_NCTS, &_stAvdAVInfo, 1);
	av_hdmiset(HDMI_SET_VIDEO_RES_CHG, &_stAvdAVInfo, 1);

	if (get_boot_mode() != FACTORY_BOOT)
		av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);

	mt8193_hdmiinit = 1;

	return 0;
}

/*----------------------------------------------------------------------------*/

static int mt8193_audio_config(enum HDMI_AUDIO_FORMAT aformat, int bitWidth)
{
	MT8193_DRV_FUNC();

	if (aformat == HDMI_AUDIO_32K_2CH) {
		HDMI_DEF_LOG("[hdmi]HDMI_AUDIO_32K_2CH\n");
		_stAvdAVInfo.e_hdmi_fs = HDMI_FS_32K;
	} else if (aformat == HDMI_AUDIO_44K_2CH) {
		HDMI_DEF_LOG("[hdmi]HDMI_AUDIO_44K_2CH\n");
		_stAvdAVInfo.e_hdmi_fs = HDMI_FS_44K;
	} else if (aformat == HDMI_AUDIO_48K_2CH) {
		HDMI_DEF_LOG("[hdmi]HDMI_AUDIO_48K_2CH\n");
		_stAvdAVInfo.e_hdmi_fs = HDMI_FS_48K;
	} else {
		HDMI_DEF_LOG("[hdmi]not support audio format, force to HDMI_AUDIO_44K_2CH\n");
		_stAvdAVInfo.e_hdmi_fs = HDMI_FS_44K;
	}

	av_hdmiset(HDMI_SET_AUDIO_CHG_SETTING, &_stAvdAVInfo, 1);

	return 0;
}

/*----------------------------------------------------------------------------*/

static int mt8193_video_enable(bool enable)
{
	MT8193_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/

static int mt8193_audio_enable(bool enable)
{
	MT8193_DRV_FUNC();

	if (enable)
		hdmi_user_mute_audio(0);
	else
		hdmi_user_mute_audio(1);
	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

void mt8193_set_mode(unsigned char ucMode)
{
	MT8193_DRV_FUNC();
	vSetClk();

}

/*----------------------------------------------------------------------------*/

int mt8193_power_on(void)
{
	struct device_node *dn;
	int bus_switch_pin;

	HDMI_DEF_LOG("[hdmi]mt8193_power_on_\n");

	if (hdmi_powerenable == 1) {
		HDMI_DEF_LOG("[hdmi]already power on, return\n");
		return 0;
	}
	hdmi_powerenable = 1;

#if defined(CONFIG_HAS_EARLYSUSPEND)
	if (mt8193_hdmiearlysuspend == 0)
		return 0;
#endif
	mt8193_hotinit = 0;
	mt8193_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	is_hdmi_plug_out_flag = 0;
#ifdef GPIO_HDMI_POWER_CONTROL
	mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, GPIO_OUT_ONE);
	HDMI_DEF_LOG("[hdmi]hdmi_5v_on\n");
#endif

	dn = of_find_compatible_node(NULL, NULL, "mediatek,mt8193-hdmi");
	bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
	gpio_direction_output(bus_switch_pin, 1);

	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_power_turnon, hdmi_power_turnon);
	vWriteHdmiSYSMsk(HDMI_SYS_PWR_RST_B, hdmi_pwr_sys_sw_unreset, hdmi_pwr_sys_sw_unreset);
	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_iso_dis, hdmi_iso_en);
	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_clock_on, hdmi_clock_off);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, ANLG_ON | HDMI_ON, ANLG_ON | HDMI_ON);

	mt8193_i2c_write(0x1500, 0x20);
	vHotPlugPinInit();
	vInitHdcpKeyGetMethod(NON_HOST_ACCESS_FROM_EEPROM);

	vWriteHdmiIntMask(0xFF);

	mod_timer(&r_hdmi_timer, jiffies + gHDMI_CHK_INTERVAL / (1000 / HZ));
	mod_timer(&r_cec_timer, jiffies + gCEC_CHK_INTERVAL / (1000 / HZ));

	return 0;
}

/*----------------------------------------------------------------------------*/

void mt8193_power_off(void)
{
	struct device_node *dn;
	int bus_switch_pin;

	HDMI_DEF_LOG("[hdmi]mt8193_power_off\n");
	if (hdmi_powerenable == 0) {
		HDMI_DEF_LOG("[hdmi]already power off, return\n");
		return;
	}
	hdmi_powerenable = 0;
	is_hdmi_plug_out_flag = 1;

	mt8193_hotinit = 1;
	mt8193_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	if (mt8193_hdmiearlysuspend == 1)
		is_hdmi_plug_out_flag = 1;
	else
		is_hdmi_plug_out_flag = 0;
#endif

	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
	vWriteHdmiIntMask(0xFF);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, 0, ANLG_ON | HDMI_ON);
#ifdef GPIO_HDMI_POWER_CONTROL
	mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, GPIO_OUT_ZERO);
	HDMI_DEF_LOG("[hdmi]hdmi_5v_off\n");
#endif

	dn = of_find_compatible_node(NULL, NULL, "mediatek,mt8193-hdmi");
	bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
	gpio_direction_output(bus_switch_pin, 0);

	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_clock_off, hdmi_clock_off);
	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_iso_en, hdmi_iso_en);
	vWriteHdmiSYSMsk(HDMI_SYS_PWR_RST_B, hdmi_pwr_sys_sw_reset, hdmi_pwr_sys_sw_unreset);
	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_power_turnoff, hdmi_power_turnon);

}

void mt8193_register_callback(CABLE_INSERT_CALLBACK cb)
{
	mt8193_hdmi_factory_callback = cb;
}

void mt8193_unregister_callback(CABLE_INSERT_CALLBACK cb)
{
	mt8193_hdmi_factory_callback = NULL;
}

/*----------------------------------------------------------------------------*/

void mt8193_dump(void)
{
	MT8193_DRV_FUNC();
	/* mt8193_dump_reg(); */
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

enum HDMI_STATE mt8193_get_state(void)
{
	MT8193_DRV_FUNC();

	if (mt8193_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;

}

/*----------------------------------------------------------------------------*/

void mt8193_log_enable(unsigned short enable)
{
	MT8193_DRV_FUNC();

	if (enable == 0) {
		hdmi_print("hdmi_pll_log =   0x1\n");
		hdmi_print("hdmi_dgi_log =   0x2\n");
		hdmi_print("hdmi_plug_log =  0x4\n");
		hdmi_print("hdmi_video_log = 0x8\n");
		hdmi_print("hdmi_audio_log = 0x10\n");
		hdmi_print("hdmi_hdcp_log =  0x20\n");
		hdmi_print("hdmi_cec_log =   0x40\n");
		hdmi_print("hdmi_ddc_log =   0x80\n");
		hdmi_print("hdmi_edid_log =  0x100\n");
		hdmi_print("hdmi_drv_log =   0x200\n");

		hdmi_print("hdmi_all_log =   0xffff\n");

	}

	mt8193_log_on = enable;

}

/*----------------------------------------------------------------------------*/

void mt8193_enablehdcp(unsigned char u1hdcponoff)
{
	MT8193_DRV_FUNC();
	_stAvdAVInfo.u1hdcponoff = u1hdcponoff;
	av_hdmiset(HDMI_SET_HDCP_OFF, &_stAvdAVInfo, 1);
}

void mt8193_setcecrxmode(unsigned char u1cecrxmode)
{
	MT8193_DRV_FUNC();
	mt8193_rxcecmode = u1cecrxmode;
}

void mt8193_colordeep(unsigned char u1colorspace, unsigned char u1deepcolor)
{
	MT8193_DRV_FUNC();
	if ((u1colorspace == 0xff) && (u1deepcolor == 0xff)) {
		hdmi_print("color_space:HDMI_YCBCR_444 = 2\n");
		hdmi_print("color_space:HDMI_YCBCR_422 = 3\n");

		hdmi_print("deep_color:HDMI_NO_DEEP_COLOR = 1\n");
		hdmi_print("deep_color:HDMI_DEEP_COLOR_10_BIT = 2\n");
		hdmi_print("deep_color:HDMI_DEEP_COLOR_12_BIT = 3\n");
		hdmi_print("deep_color:HDMI_DEEP_COLOR_16_BIT = 4\n");

		return;
	}
	/*
	   if (dReadHdmiSYS(0x2cc) == 0x8193)
	   _stAvdAVInfo.e_video_color_space = HDMI_YCBCR_444;
	   else
	   _stAvdAVInfo.e_video_color_space = HDMI_RGB;
	 */

	_stAvdAVInfo.e_video_color_space = u1colorspace;
	_stAvdAVInfo.e_deep_color_bit = (HDMI_DEEP_COLOR_T) u1deepcolor;
}

void mt8193_read(unsigned short u2Reg, unsigned int *p4Data)
{
	if (u2Reg & 0x8000) {
		/* if ((u2Reg & 0xf000) == 0x8000) */
		/* u2Reg -= 0x8000; */
		/* *p4Data = (*(unsigned int *)(0xf4000000 + u2Reg)); */
	} else
		mt8193_i2c_read(u2Reg, p4Data);

	hdmi_print("Reg read= 0x%04x, data = 0x%08x\n", u2Reg, *p4Data);
}

void mt8193_write(unsigned short u2Reg, unsigned int u4Data)
{
	if (u2Reg & 0x8000) {
		/* if ((u2Reg & 0xf000) == 0x8000) */
		/* u2Reg -= 0x8000; */
		/* *(unsigned int *)(0xf4000000 + u2Reg) = u4Data; */
	} else {
		hdmi_print("Reg write= 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
		mt8193_i2c_write(u2Reg, u4Data);
	}
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mt8193_hdmi_early_suspend(struct early_suspend *h)
{
	MT8193_PLUG_FUNC();
	mt8193_hdmiearlysuspend = 0;
	is_hdmi_plug_out_flag = 0;
}

static void mt8193_hdmi_late_resume(struct early_suspend *h)
{
	MT8193_PLUG_FUNC();
	mt8193_hdmiearlysuspend = 1;
}

static struct early_suspend mt8193_hdmi_early_suspend_desc = {
	.level = 0xFE,
	.suspend = mt8193_hdmi_early_suspend,
	.resume = mt8193_hdmi_late_resume,
};
#endif

/*********************************************************
		mt8193 debug
*********************************************************/
#define HDMI_ATTR_SPRINTF(fmt, arg...)  \
	do { \
		temp_len = sprintf(buf, fmt, ##arg); \
		buf += temp_len; \
		len += temp_len; \
		buf[0] = 0;\
	} while (0)

#define DPI0_BASE_ADDR dispsys_reg[DISP_REG_DPI0]

static void process_dbg_opt(const char *opt)
{
	unsigned int reg;
	unsigned int val;
	unsigned int vadr_regstart;
	unsigned int vadr_regend;
	char *buf;
	int temp_len = 0;
	int len = 0;
	int ret;
	long int p_temp;

	buf = (char *)opt;

	if (strncmp(buf, "dbgtype:", 8) == 0) {
		ret = sscanf(buf + 8, "%x", &val);
		mt8193_log_enable(val);
		pr_debug("hdmidrv_log_on = 0x%08x\n", mt8193_log_on);
	} else if (strncmp(buf, "w:", 2) == 0) {
		ret = sscanf(buf + 2, "%x=%x", &reg, &val);
		pr_debug("w:0x%08x=0x%08x\n", reg, val);
		mt8193_write(reg, val);
	} else if (strncmp(buf, "r:", 2) == 0) {
		ret = sscanf(buf + 2, "%x/%x", &vadr_regstart, &vadr_regend);
		vadr_regend &= 0x3ff;
		pr_debug("r:0x%08x/0x%08x\n", vadr_regstart, vadr_regend);
		vadr_regend = vadr_regstart + vadr_regend;
		while (vadr_regstart <= vadr_regend) {
			mt8193_read(vadr_regstart, &val);
			HDMI_ATTR_SPRINTF("0x%08x = 0x%08x\n", vadr_regstart, val);
			vadr_regstart += 4;
		}
	} else if (strncmp(buf, "dw:", 3) == 0) {
		ret = sscanf(buf + 3, "%x=%x", &reg, &val);
		pr_debug("dw:0x%08x=0x%08x\n", reg, val);
		iowrite32(val, (void __iomem *)DPI0_BASE_ADDR + reg);
	} else if (strncmp(buf, "dr:", 3) == 0) {
		ret = sscanf(buf + 3, "%x/%x", &vadr_regstart, &vadr_regend);
		vadr_regend &= 0x3ff;
		pr_debug("dr:0x%08x/0x%08x\n", vadr_regstart, vadr_regend);
		vadr_regend = vadr_regstart + vadr_regend;
		while (vadr_regstart <= vadr_regend) {
			HDMI_ATTR_SPRINTF("0x%08x = 0x%08x\n", vadr_regstart,
					  ioread32((void __iomem *)DPI0_BASE_ADDR + vadr_regstart));
			vadr_regstart += 4;
		}
	} else if (strncmp(buf, "status", 6) == 0) {
		HDMI_ATTR_SPRINTF("[hdmi]mt8193_log_on=%x\n", mt8193_log_on);
		HDMI_ATTR_SPRINTF("[hdmi]hdmi_powerenable=%x\n", hdmi_powerenable);
		HDMI_ATTR_SPRINTF("[hdmi]mt8193_hdmiinit=%d\n", mt8193_hdmiinit);
		HDMI_ATTR_SPRINTF("[hdmi]mt8193_hotinit=%d\n", mt8193_hotinit);
		HDMI_ATTR_SPRINTF("[hdmi]mt8193_hdmipoweroninit=%d\n", mt8193_hdmipoweroninit);
		HDMI_ATTR_SPRINTF("[hdmi]is_user_mute_hdmi_audio=%x\n", is_user_mute_hdmi_audio);
		HDMI_ATTR_SPRINTF("[hdmi]port=%d\n", hdmi_port_status());
		if (mt8193_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
			HDMI_ATTR_SPRINTF("[hdmi]plug:HDMI_STATE_HOT_PLUGIN_AND_POWER_ON\n");
		else if (mt8193_hotplugstate == HDMI_STATE_HOT_PLUG_OUT)
			HDMI_ATTR_SPRINTF("[hdmi]plug:HDMI_STATE_HOT_PLUG_OUT\n");
		else if (mt8193_hotplugstate == HDMI_STATE_HOT_PLUG_IN_ONLY)
			HDMI_ATTR_SPRINTF("[hdmi]plug:HDMI_STATE_HOT_PLUG_IN_ONLY\n");

		HDMI_ATTR_SPRINTF("[hdmi]video resolution : %d\n", _stAvdAVInfo.e_resolution);
		HDMI_ATTR_SPRINTF("[hdmi]video color space : %d\n",
				  _stAvdAVInfo.e_video_color_space);
		HDMI_ATTR_SPRINTF("[hdmi]video deep color : %d\n", _stAvdAVInfo.e_deep_color_bit);
		HDMI_ATTR_SPRINTF("[hdmi]audio fs : %d\n", _stAvdAVInfo.e_hdmi_fs);

		if (vIsDviMode())
			HDMI_ATTR_SPRINTF("[hdmi]dvi Mode\n");
		else
			HDMI_ATTR_SPRINTF("[hdmi]hdmi Mode\n");

		mt8193_hdmistatus();
	} else if (0 == strncmp(opt, "po", 2)) {
		mt8193_power_on();
	} else if (0 == strncmp(opt, "pd", 2)) {
		mt8193_power_off();
	} else if (0 == strncmp(opt, "cs:", 3)) {
		ret = sscanf(buf + 3, "%x", &val);
		mt8193_colordeep(val, 0);
	} else if (0 == strncmp(opt, "finit", 5)) {
		hdmi_factory_mode_test(STEP1_CHIP_INIT, NULL);
	} else if (0 == strncmp(opt, "fres:", 5)) {
		ret = sscanf(buf + 5, "%x", &val);
		p_temp = (long int)val;
		hdmi_factory_mode_test(STEP3_START_DPI_AND_CONFIG, (void *)p_temp);
	} else {
		HDMI_ATTR_SPRINTF("---hdmi debug help---\n");
		HDMI_ATTR_SPRINTF("please go in to sys/kernel/debug\n");
		HDMI_ATTR_SPRINTF("[debug type] echo dbgtype:VALUE>8193\n");
		HDMI_ATTR_SPRINTF("[8193 read reg] echo r:ADDR/LEN>8193;cat 8193\n");
		HDMI_ATTR_SPRINTF("[8193 write reg] echo w:ADDR=VALUE>8193\n");
		HDMI_ATTR_SPRINTF("[d2 dpi0  read reg] echo dr:ADDR/LEN>8193;cat 8193\n");
		HDMI_ATTR_SPRINTF("[d2 dpi0  write reg] echo dw:ADDR=VALUE>8193\n");
		HDMI_ATTR_SPRINTF("[hdmi status] echo status>8193;cat 8193\n");
		HDMI_ATTR_SPRINTF("[power on] echo po>8193\n");
		HDMI_ATTR_SPRINTF("[power off] echo dn>8193\n");
		HDMI_ATTR_SPRINTF("[color space] echo cs:VALUE>8193\n");
		HDMI_ATTR_SPRINTF("[factory mode init] echo finit>8193\n");
		HDMI_ATTR_SPRINTF("[factory res] echo fres:VALUE>8193\n");
	}
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	pr_debug("[hdmi] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* --------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* --------------------------------------------------------------------------- */

struct dentry *mt8193_hdmi_dbgfs = NULL;

static int mt8193_hdmi_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char mt8193_hdmi_debug_buffer[2048];

static ssize_t mt8193_hdmi_debug_read(struct file *file,
				      char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;

	n = strlen(mt8193_hdmi_debug_buffer);
	mt8193_hdmi_debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, mt8193_hdmi_debug_buffer, n);
}


static ssize_t mt8193_hdmi_debug_write(struct file *file,
				       const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(mt8193_hdmi_debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;


	if (copy_from_user(&mt8193_hdmi_debug_buffer, ubuf, count))
		return -EFAULT;


	mt8193_hdmi_debug_buffer[count] = 0;

	process_dbg_cmd(mt8193_hdmi_debug_buffer);

	return ret;
}

static const struct file_operations mt8193_hdmi_debug_fops = {
	.read = mt8193_hdmi_debug_read,
	.write = mt8193_hdmi_debug_write,
	.open = mt8193_hdmi_debug_open,
};

void mt8193_hdmi_debug_init(void)
{
	mt8193_hdmi_dbgfs = debugfs_create_file("8193",
						S_IFREG | S_IRUGO, NULL, (void *)0,
						&mt8193_hdmi_debug_fops);
}

void mt8193_hdmi_debug_deinit(void)
{
	debugfs_remove(mt8193_hdmi_dbgfs);
}

static void vSetHDMICtrlState(HDMI_CTRL_STATE_T e_state)
{
	MT8193_PLUG_FUNC();
	e_hdmi_ctrl_state = e_state;
}

static void vNotifyAppHdmiState(unsigned char u1hdmistate)
{
	HDMI_EDID_T get_info;

	mt8193_AppGetEdidInfo(&get_info);
#if 0
	if (mt8193_hdmi_factory_callback != NULL)
		mt8193_hdmi_factory_callback(HDMI_STATE_NO_DEVICE);
#endif

	switch (u1hdmistate) {
	case HDMI_PLUG_OUT:
		HDMI_DEF_LOG("[hdmi]notify plug:HDMI_STATE_NO_DEVICE\n");
		is_hdmi_plug_out_flag = 1;
		hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);
		mt8193_SetPhysicCECAddress(0xffff, 0x0);
		break;

	case HDMI_PLUG_IN_AND_SINK_POWER_ON:
		HDMI_DEF_LOG("[hdmi]notify plug:HDMI_STATE_ACTIVE\n");
		is_hdmi_plug_out_flag = 0;
		hdmi_util.state_callback(HDMI_STATE_ACTIVE);
		mt8193_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0x4);
		if ((mt8193_hdmi_factory_callback != NULL) && (mt8193_Check_EdidHeader() == TRUE))
			mt8193_hdmi_factory_callback(HDMI_STATE_ACTIVE);
		break;

	case HDMI_PLUG_IN_ONLY:
		HDMI_DEF_LOG("[hdmi]notify plug:HDMI_STATE_PLUGIN_ONLY\n");
		is_hdmi_plug_out_flag = 1;
		hdmi_util.state_callback(HDMI_STATE_PLUGIN_ONLY);
		mt8193_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0xf);
		break;

	case HDMI_PLUG_IN_CEC:
		hdmi_util.state_callback(HDMI_STATE_CEC_UPDATE);
		break;

	default:
		HDMI_DEF_LOG("[hdmi]notify plug:err\n");
		break;

	}
}

void vcheckhdmiplugstate(void)
{
	unsigned int bMask;

	MT8193_PLUG_FUNC();

	bMask = bReadHdmiIntMask();
	vWriteHdmiIntMask((bMask & 0xfe));
	if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == FALSE) {

		{
			if ((i4SharedInfo(SI_HDMI_RECEIVER_STATUS) ==
			     HDMI_PLUG_IN_AND_SINK_POWER_ON)
			    || (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_ONLY)) {
				bMask = bReadHdmiIntMask();
				vWriteHdmiIntMask((bMask | 0xfE));

				vHDCPReset();
				vTxSignalOnOff(0);

				vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
				vSetHDMICtrlState(HDMI_STATE_HOT_PLUG_OUT);
			} else {
				MT8193_PLUG_LOG("plug out, no action\n");
			}
		}
	} else {
		if ((i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_OUT)
		    || (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_ONLY)) {
			vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON);
			vSetHDMICtrlState(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
		} else {
			MT8193_PLUG_LOG("plug in ok, no action\n");
		}
	}
}

static void vPlugDetectService(HDMI_CTRL_STATE_T e_state)
{
	unsigned char bData = 0xff;

	MT8193_PLUG_FUNC();

	e_hdmi_ctrl_state = HDMI_STATE_IDLE;

	switch (e_state) {
	case HDMI_STATE_HOT_PLUG_OUT:
		vClearEdidInfo();
		vHDCPReset();
		bData = HDMI_PLUG_OUT;

		break;

	case HDMI_STATE_HOT_PLUGIN_AND_POWER_ON:
		mt8193_checkedid(0);
		bData = HDMI_PLUG_IN_AND_SINK_POWER_ON;

		break;

	case HDMI_STATE_HOT_PLUG_IN_ONLY:
		vClearEdidInfo();
		vHDCPReset();
		mt8193_checkedid(0);
		bData = HDMI_PLUG_IN_ONLY;

		break;

	case HDMI_STATE_IDLE:

		break;
	default:
		break;

	}

	if ((bData != 0xff) && (hdmi_powerenable == 1))
		vNotifyAppHdmiState(bData);
}

void hdmi_timer_impl(void)
{
	if (mt8193_hdmiinit == 0) {
		mt8193_hdmiinit = 1;
		/* mt8193_power_off(); */
		vInitAvInfoVar();
		return;
	}

	if (mt8193_hotinit != 1)
		mt8193_hdmiinit++;

#if defined(CONFIG_HAS_EARLYSUSPEND)
	if (mt8193_hdmiearlysuspend == 1) {
#else
	{
#endif
		if (((mt8193_hdmiinit > 5) || (mt8193_hotinit == 0)) && (mt8193_hotinit != 1)) {
			if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == FALSE) {
				if ((mt8193_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
				    && (mt8193_hotinit == 2)) {
					vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
					mt8193_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
					vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
					MT8193_PLUG_LOG
					    ("[detectcable1] mt8193_hotinit = %d,mt8193_hdmiinit=%d\n",
					     mt8193_hotinit, mt8193_hdmiinit);
				}
#if 1
				if ((mt8193_hotinit == 0)
				    && (bCheckPordHotPlug(HOTPLUG_MODE) == TRUE)) {
					vSetSharedInfo(SI_HDMI_RECEIVER_STATUS,
						       HDMI_PLUG_IN_AND_SINK_POWER_ON);
					mt8193_hotinit = 2;
					mt8193_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
					vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
					vWriteHdmiIntMask(0xff);	/* INT mask MDI */
					MT8193_PLUG_LOG
					    ("[detectcable2] mt8193_hotinit = %d,mt8193_hdmiinit=%d\n",
					     mt8193_hotinit, mt8193_hdmiinit);
				}
#endif
				if ((mt8193_hotinit == 0)
				    && (bCheckPordHotPlug(HOTPLUG_MODE) == FALSE)) {
					vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
					mt8193_hotinit = 2;

					vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
					mt8193_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
					vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
					MT8193_PLUG_LOG
					    ("[detectcable1] mt8193_hotinit = %d,mt8193_hdmiinit=%d\n",
					     mt8193_hotinit, mt8193_hdmiinit);
				}

			} else if ((mt8193_hotplugstate == HDMI_STATE_HOT_PLUG_OUT)
				   && (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)) {
				vSetSharedInfo(SI_HDMI_RECEIVER_STATUS,
					       HDMI_PLUG_IN_AND_SINK_POWER_ON);
				mt8193_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
				mt8193_hotinit = 2;
				vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
				vWriteHdmiIntMask(0xff);	/* INT mask MDI */
				MT8193_PLUG_LOG
				    ("[detectcable3] mt8193_hotinit = %d,mt8193_hdmiinit=%d\n",
				     mt8193_hotinit, mt8193_hdmiinit);
			} else if ((mt8193_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
				   && ((e_hdcp_ctrl_state == HDCP_WAIT_RI)
				       || (e_hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY))) {
				if (bCheckHDCPStatus(HDCP_STA_RI_RDY)) {
					vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
					vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
				}
			}
			mt8193_hdmiinit = 1;
		}
	}

	if (mt8193_hdmiCmd == HDMI_PLUG_DETECT_CMD) {
		vClearHdmiCmd();
		/* vcheckhdmiplugstate(); */
		/* vPlugDetectService(e_hdmi_ctrl_state); */
	} else if (mt8193_hdmiCmd == HDMI_HDCP_PROTOCAL_CMD) {
		vClearHdmiCmd();
		HdcpService(e_hdcp_ctrl_state);
	}
}

void cec_timer_impl(void)
{
	if (mt8193_cecinit == 0) {
		mt8193_cecinit = 1;
		mt8193_cec_init();
		return;
	}

	if (mt8193_cec_on == 1)
		mt8193_cec_mainloop(mt8193_rxcecmode);

}

void mt8193_nlh_impl(void)
{
	unsigned int u4Data;
	unsigned char bData;
	unsigned char bMask;

	/*read register and then assert which interrupt occurred*/
	mt8193_i2c_read(0x1508, &u4Data);
	mt8193_i2c_write(0x1504, 0xffffffff);

	MT8193_DRV_LOG("0x1508 = 0x%08x\n", u4Data);

	if (u4Data & 0x20) {
		MT8193_CEC_LOG("cec interrupt\n");

		if (mt8193_cec_on == 1) {
			if (mt8193_cec_isrprocess(mt8193_rxcecmode))
				vNotifyAppHdmiState(HDMI_PLUG_IN_CEC);

		}
	}

	if (u4Data & 0x4) {
		bCheckHDCPStatus(0xfb);
		bData = bReadGRLInt();

		if (bData & INT_HDCP) {
			MT8193_HDCP_LOG("hdcp interrupt\n");
			bClearGRLInt(INT_HDCP);

		} else if (bData & INT_MDI) {
			MT8193_PLUG_LOG("hdmi interrupt\n");
			bClearGRLInt(INT_MDI);
			bMask = bReadHdmiIntMask();
			/* vWriteHdmiIntMask((0xfd));//INT mask MDI */
		}
	}
#ifdef CUST_EINT_EINT_HDMI_HPD_NUM
	mt65xx_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
}

static int hdmi_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 91};

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_timer_wq, atomic_read(&hdmi_timer_event));
		atomic_set(&hdmi_timer_event, 0);
		hdmi_timer_impl();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int cec_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 91};

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(cec_timer_wq, atomic_read(&cec_timer_event));
		atomic_set(&cec_timer_event, 0);
		cec_timer_impl();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int mt8193_nlh_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94};

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(mt8193_nlh_wq, atomic_read(&mt8193_nlh_event));
		atomic_set(&mt8193_nlh_event, 0);
		mt8193_nlh_impl();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

void hdmi_poll_isr(unsigned long n)
{
	unsigned int i;

	if (hdmi_powerenable != 1)
		return;

	for (i = 0; i < MAX_HDMI_TMR_NUMBER; i++) {
		if (mt8193_TmrValue[i] >= AVD_TMR_ISR_TICKS) {
			mt8193_TmrValue[i] -= AVD_TMR_ISR_TICKS;

			if ((i == HDMI_PLUG_DETECT_CMD)
			    && (mt8193_TmrValue[HDMI_PLUG_DETECT_CMD] == 0))
				vSendHdmiCmd(HDMI_PLUG_DETECT_CMD);
			else if ((i == HDMI_HDCP_PROTOCAL_CMD)
				 && (mt8193_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else if (mt8193_TmrValue[i] > 0) {
			mt8193_TmrValue[i] = 0;

			if ((i == HDMI_PLUG_DETECT_CMD)
			    && (mt8193_TmrValue[HDMI_PLUG_DETECT_CMD] == 0))
				vSendHdmiCmd(HDMI_PLUG_DETECT_CMD);
			else if ((i == HDMI_HDCP_PROTOCAL_CMD)
				 && (mt8193_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
	}

	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);
	mod_timer(&r_hdmi_timer, jiffies + gHDMI_CHK_INTERVAL / (1000 / HZ));

}

void cec_poll_isr(unsigned long n)
{
	if (hdmi_powerenable != 1)
		return;

	atomic_set(&cec_timer_event, 1);
	wake_up_interruptible(&cec_timer_wq);
	mod_timer(&r_cec_timer, jiffies + gCEC_CHK_INTERVAL / (1000 / HZ));
}

/*********************************************************
		mt8193 end
*********************************************************/

static int mt8193_init(void)
{

	HDMI_DEF_LOG("[hdmi]mt8193_init\n");

	init_waitqueue_head(&hdmi_timer_wq);
	hdmi_timer_task = kthread_create(hdmi_timer_kthread, NULL, "hdmi_timer_kthread");
	wake_up_process(hdmi_timer_task);

	init_waitqueue_head(&cec_timer_wq);
	cec_timer_task = kthread_create(cec_timer_kthread, NULL, "cec_timer_kthread");
	wake_up_process(cec_timer_task);

	init_waitqueue_head(&mt8193_nlh_wq);
	mt8193_nlh_task = kthread_create(mt8193_nlh_kthread, NULL, "mt8193_nlh_kthread");
	wake_up_process(mt8193_nlh_task);

#if defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&mt8193_hdmi_early_suspend_desc);
#endif

	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	r_hdmi_timer.expires = jiffies + 1000 / (1000 / HZ);	/* wait 1s to stable */
	r_hdmi_timer.function = hdmi_poll_isr;
	r_hdmi_timer.data = 0;
	init_timer(&r_hdmi_timer);
	add_timer(&r_hdmi_timer);

	memset((void *)&r_cec_timer, 0, sizeof(r_cec_timer));
	r_cec_timer.expires = jiffies + 1000 / (1000 / HZ);	/* wait 1s to stable */
	r_cec_timer.function = cec_poll_isr;
	r_cec_timer.data = 0;
	init_timer(&r_cec_timer);
	add_timer(&r_cec_timer);

	mt8193_hdmi_debug_init();
	mt8193_hdmi_factory_callback = NULL;

	return 0;
}

const struct HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const struct HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = mt8193_set_util_funcs,
		.get_params = mt8193_get_params,
		.init = mt8193_init,
		.enter = mt8193_enter,
		.exit = mt8193_exit,
		.suspend = mt8193_suspend,
		.resume = mt8193_resume,
		.video_config = mt8193_video_config,
		.audio_config = mt8193_audio_config,
		.video_enable = mt8193_video_enable,
		.audio_enable = mt8193_audio_enable,
		.power_on = mt8193_power_on,
		.power_off = mt8193_power_off,
		.set_mode = mt8193_set_mode,
		.dump = mt8193_dump,
		.read = mt8193_read,
		.write = mt8193_write,
		.get_state = mt8193_get_state,
		.log_enable = mt8193_log_enable,
		.InfoframeSetting = mt8193_InfoframeSetting,
		.checkedid = mt8193_checkedid,
		.colordeep = mt8193_colordeep,
		.enablehdcp = mt8193_enablehdcp,
		.setcecrxmode = mt8193_setcecrxmode,
		.hdmistatus = mt8193_hdmistatus,
		.hdcpkey = mt8193_hdcpkey,
		.getedid = mt8193_AppGetEdidInfo,
		.setcecla = mt8193_CECMWSetLA,
		.sendsltdata = mt8193_u4CecSendSLTData,
		.getceccmd = mt8193_CECMWGet,
		.getsltdata = mt8193_GetSLTData,
		.setceccmd = mt8193_CECMWSend,
		.cecenable = mt8193_CECMWSetEnableCEC,
		.getcecaddr = mt8193_NotifyApiCECAddress,
		.mutehdmi = mt8193_mutehdmi,
		.checkedidheader = mt8193_Check_EdidHeader,
		.register_callback = mt8193_register_callback,
		.unregister_callback = mt8193_unregister_callback,
	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);
#endif

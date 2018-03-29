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
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
/* #include <linux/earlysuspend.h> */
#include <linux/platform_device.h>
#include <linux/atomic.h>
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
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include "hdmitx.h"
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include "hdmi_ctrl.h"
#include "hdmictrl.h"
#include "hdmiddc.h"
#include "hdmihdcp.h"
#include "hdmicec.h"
#include "hdmiedid.h"

#include "internal_hdmi_drv.h"
/*#include <cust_eint.h>*/
/*#include "cust_gpio_usage.h"*/
/* #include "mach/eint.h" */
/* #include "mach/irqs.h" */
#include "asm-generic/irq.h"


/* #include <mach/devs.h> */
/* #include <mach/mt_typedefs.h> */
/* #include <mach/mt_gpio.h> */
#include <linux/types.h>
/* #include <mach/mt_pm_ldo.h> */
/* #include <mach/mt_boot.h> */
#include "mt_boot_common.h"
/* #include "hdmi_iic.h" */
#include "hdmiavd.h"
#include "hdmicmd.h"
/* #include <mach/mt_pmic_wrap.h> */
#include "mt_smi.h"

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
#include "hdmi_ca.h"
#endif
#include <linux/fb.h>
#include <linux/notifier.h>
#define SAVELENGTH 100
#define SHOW_HDMISTATE_LOG_TIME 500
#define MASK_UNUSE_INTERRUPT_TIME 200
#define HPDTOGGLE_VIDEOCONFIG 0x1
#define SWTICHTOGGLE_VIDEOCONFIG 0x2
#define CLEANTOGGLE_VIDEOCONFIG 0x0
/*----------------------------------------------------------------------------*/
/* Debug message defination */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* HDMI Timer */
/*----------------------------------------------------------------------------*/
static struct timer_list r_hdmi_timer;
static struct timer_list r_cec_timer;
unsigned char hdmi2_debug = 0;
unsigned char hdmi_dpi_output = 0;
unsigned char hdmi2_force_output = 0;
unsigned int hdmistate_debug = 1;

static bool factory_boot_mode;

static uint32_t gHDMI_CHK_INTERVAL = 10;
static uint32_t gCEC_CHK_INTERVAL = 20;

size_t hdmidrv_log_on = hdmialllog;
size_t hdmi_cec_on = 0;
size_t hdmi_powerenable = 0;
size_t hdmi_clockenable = 0;
size_t hdmi_hdmiearlysuspend = 1;

size_t hdmi_TmrValue[MAX_HDMI_TMR_NUMBER] = { 0 };

size_t hdmi_hdmiCmd = 0xff;
size_t hdmi_rxcecmode = CEC_NORMAL_MODE;
enum HDMI_CTRL_STATE_T e_hdmi_ctrl_state = HDMI_STATE_IDLE;
enum HDCP_CTRL_STATE_T e_hdcp_ctrl_state = HDCP_RECEIVER_NOT_READY;
size_t hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;


static unsigned char port_hpd_value_bak = 0xff;
unsigned char hdcp2_version_flag = FALSE;

static struct task_struct *hdmi_timer_task;
wait_queue_head_t hdmi_timer_wq;
atomic_t hdmi_timer_event = ATOMIC_INIT(0);

static struct task_struct *cec_timer_task;
wait_queue_head_t cec_timer_wq;
atomic_t cec_timer_event = ATOMIC_INIT(0);

static struct task_struct *hdmi_irq_task;
wait_queue_head_t hdmi_irq_wq;	/* NFI, LVDS, HDMI */
atomic_t hdmi_irq_event = ATOMIC_INIT(0);

static struct HDMI_UTIL_FUNCS hdmi_util = { 0 };

/* extern struct hdmi_resources hdmi_res; */
unsigned char hdmi_plug_test_mode = 0;
unsigned char hdmi_is_boot_time = 0;

static int hdmi_timer_kthread(void *data);
static int cec_timer_kthread(void *data);
static int hdmi_irq_kthread(void *data);
DEFINE_SEMAPHORE(hdmi_edid_mutex);
static void vPlugDetectService(enum HDMI_CTRL_STATE_T e_state);

const char *szHdmiPordStatusStr[] = {
	"HDMI_PLUG_OUT=0",
	"HDMI_PLUG_IN_AND_SINK_POWER_ON",
	"HDMI_PLUG_IN_ONLY",
	"HDMI_PLUG_IN_EDID",
	"HDMI_PLUG_IN_CEC",
	"HDMI_PLUG_IN_POWER_EDID",
};

const char *szHdmiCecPordStatusStr[] = {
	"HDMI_CEC_STATE_PLUG_OUT=0",
	"HDMI_CEC_STATE_TX_STS",
	"HDMI_CEC_STATE_GET_CMD",
};

/* from DTS, for debug */
unsigned int hdmi_reg_pa_base[HDMI_REG_NUM] = {
	0x14025000, 0x11012000, 0x10013000
};

/*//unsigned int hdmi_ref_reg_pa_base[HDMI_REF_REG_NUM] ={
//0x10209000, AP_CCIF0/pll
0x10000000, TOPCK_GEN
0x10001000 INFRA_SYS
0x14000000, MMSYS_CONFIG
0x10005000}; GPIO_REG
0x1401d000}; DPI0_REG
0x10206000}; EFUSE_REG
0x10003000}; PERISYS_REG

*/
unsigned int hdmi_ref_reg_pa_base[HDMI_REF_REG_NUM] = { 0 };

/* Reigster of HDMI , Include HDMI SHELL,DDC,CEC*/
unsigned long hdmi_reg[HDMI_REG_NUM] = { 0 };

/*Registers which will be used by HDMI   */
unsigned long hdmi_ref_reg[HDMI_REF_REG_NUM] = { 0 };

/* clocks that will be used by hdmi module*/
struct clk *hdmi_ref_clock[HDMI_SEL_CLOCK_NUM] = { 0 };

/*Irq of HDMI */
unsigned int hdmi_irq;
/* 5v ddc power control pin*/
int hdmi_power_control_pin;
struct platform_device *hdmi_pdev;
struct hdmi_internal_device {
	/* base address of HDMI registers */
	void __iomem *regs[HDMI_REG_NUM];
	/** HDMI interrupt */
	unsigned int irq;
	/** pointer to device parent , maybe no need??*/
	struct device *dev;
};

unsigned int resolution_v = 0;

#define vWriteHdmiANA(dAddr, dVal)  (*((unsigned int *)(hdmi_ref_reg[AP_CCIF0] + dAddr)) = (dVal))
#define dReadHdmiANA(dAddr)         (*((unsigned int *)(hdmi_ref_reg[AP_CCIF0] + dAddr)))
#define vWriteHdmiANAMsk(dAddr, dVal, dMsk) \
	(vWriteHdmiANA((dAddr), (dReadHdmiANA(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

void TVD_config_pll(unsigned int resolutionmode)
{
	vWriteHdmiANA(0x40, 0);
	vWriteHdmiANAMsk(MHL_TVDPLL_PWR, 0, RG_TVDPLL_PWR_ON);
	mdelay(5);

	vWriteHdmiANAMsk(0x0, 0x171, 0x171);
	vWriteHdmiANAMsk(MHL_TVDPLL_PWR, RG_TVDPLL_PWR_ON, RG_TVDPLL_PWR_ON);
	mdelay(1);
	vWriteHdmiANAMsk(0x0, 0x173, 0x173);
	mdelay(5);
	/* vWriteHdmiANAMsk(MHL_TVDPLL_CON1,TVDPLL_SDM_PCW_CHG,TVDPLL_SDM_PCW_CHG); */
	switch (resolutionmode) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0x1 << RG_TVDPLL_POSDIV), RG_TVDPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (0xc7627 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);
		/* *( unsigned int*)(0x10209040) = (0xff030000); */
		vWriteHdmiANA(0x40, 0xff030000);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN, RG_TVDPLL_EN);

		break;

	case HDMI_VIDEO_1920x1080p_30Hz:
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
	case HDMI_VIDEO_2160P_30HZ:
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0x0 << RG_TVDPLL_POSDIV), RG_TVDPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (0x112276 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);
		vWriteHdmiANA(0x40, 0xff030000);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN, RG_TVDPLL_EN);

		break;

	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0x0 << RG_TVDPLL_POSDIV), RG_TVDPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (0x111e08 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);
		vWriteHdmiANA(0x40, 0xff030000);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN, RG_TVDPLL_EN);

		break;
	default:
		{
			break;
		}
	}

	mdelay(20);
	vWriteHdmiANAMsk(MHL_TVDPLL_CON1, 0, TVDPLL_SDM_PCW_CHG);
	mdelay(20);
	vWriteHdmiANAMsk(MHL_TVDPLL_CON1, TVDPLL_SDM_PCW_CHG, TVDPLL_SDM_PCW_CHG);
	mdelay(20);
}

void set_dpi_res(unsigned char arg)
{
	switch (arg) {
	case HDMI_VIDEO_720x480p_60Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000007;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410010;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x02000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x01e002d0;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x0000003e;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x0010003c;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000006;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x0009001e;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;


			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000000;


			break;
		}
	case HDMI_VIDEO_720x576p_50Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000007;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410010;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x02000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x024002d0;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x00000040;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x000c0044;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00050027;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x30) = 0x00010001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;


			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000000;

			break;
		}
	case HDMI_VIDEO_1280x720p_60Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410000;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x82000200;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x02d00500;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x00000028;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x006e00dc;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00050014;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;


			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000000;


			break;
		}
	case HDMI_VIDEO_1280x720p_50Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410000;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x82000200;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x02d00500;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x00000028;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x01b800dc;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00050014;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;


			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000000;


			break;
		}
	case HDMI_VIDEO_1920x1080p_24Hz:
		{
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410000;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x82000200;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x04380780;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x0000002c;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x027e0094;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00040024;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;


			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000000;

			break;
		}


	case HDMI_VIDEO_1920x1080p_60Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410000;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x82000200;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x04380780;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x0000002c;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x00580094;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00040024;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;


			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000001;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;



			break;
		}
	case HDMI_VIDEO_1920x1080p_50Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410000;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x82000200;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x04380780;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x0000002c;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x02100094;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00040024;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;



			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000001;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;

			break;
		}
	case HDMI_VIDEO_1920x1080i_60Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00430004;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x82000200;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x021c0780;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x0000002c;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x00580094;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x0002000f;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00010005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00020010;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;


			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000000;


			break;
		}
	case HDMI_VIDEO_1920x1080i_50Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00430004;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x82000200;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x021c0780;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x0000002c;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x02100094;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x0002000f;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00010005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00020010;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;


			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000000;



			break;
		}

	case HDMI_VIDEO_1280x720p3d_60Hz:
		{

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x004100a8;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x02000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006002;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x02d00500;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x00000028;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x006e00dc;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00050014;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00050014;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;



			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000001;
			break;
		}

	case HDMI_VIDEO_1920x1080p3d_24Hz:
		{
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000007;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410078;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x02000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x04380780;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x0000002c;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x027e0094;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00040024;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000005;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00040024;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;



			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000000;

			break;
		}

	case HDMI_VIDEO_2160P_30HZ:
		{
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x4) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x8) = 0x00000001;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xc) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x10) = 0x00410000;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xe0) = 0x82000200;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x14) = 0x00006000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x18) = 0x08700f00;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x20) = 0x00000058;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x24) = 0x00b00128;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x28) = 0x0000000a;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x2c) = 0x00080048;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x68) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x6c) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x70) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x74) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x78) = 0x00000000;
			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x7c) = 0x00000000;



			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xa0) = 0x00000001;

			*(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0xf00) = 0x00000041;


			break;
		}

	default:
		break;
	}

	mdelay(10);
	*((unsigned int *)((hdmi_ref_reg[DPI0_REG] + 0x4))) = (1);
	mdelay(40);
	*((unsigned int *)((hdmi_ref_reg[DPI0_REG] + 0x4))) = (0);

}

const char *hdmi_use_clock_name_spy(enum HDMI_REF_CLOCK_ENUM module)
{
	switch (module) {
	/* case MMSYS_POWER: */
	/*	return "mmsys_power";	 Must be power on first, power off last */
	case INFRA_SYS_CEC:
		return "cec_pdn";	/* cec module clock */
	case MMSYS_HDMI_PLL:
		return "mmsys_hdmi_pll";
	case MMSYS_HDMI_PIXEL:
		return "mmsys_hdmi_pixel";
	case MMSYS_HDMI_AUDIO:
		return "mmsys_hdmi_audio";
	case MMSYS_HDMI_SPIDIF:
		return "mmsys_hdmi_spidif";
	case PERI_DDC:
		return "peri_ddc_pdn";
	case MMSYS_HDMI_HDCP:
		return "mmsys_hdmi_hdcp";
	case MMSYS_HDMI_HDCP24:
		return "mmsys_hdmi_hdcp24";

	case TOP_HDMI_SEL:
		return "top_hdmi_sel";
	case TOP_HDMISEL_DIG_CTS:
		return "top_hdmisel_dig_cts";
	case TOP_HDMISEL_D2:
		return "top_hdmisel_d2";
	case TOP_HDMISEL_D3:
		return "top_hdmisel_d3";

	case TOP_HDCP_SEL:
		return "top_hdcp_sel";
	case TOP_HDCPSEL_SYS4D2:
		return "top_hdcpsel_sys4d2";
	case TOP_HDCPSEL_SYS3D4:
		return "top_hdcpsel_sys3d4";
	case TOP_HDCPSEL_UNIV2D2:
		return "top_hdcpsel_univ2d2";

	case TOP_HDCP24_SEL:
		return "top_hdcp24_sel";
	case TOP_HDCP24SEL_UNIVPD26:
		return "top_hdcp24sel_univpd26";
	case TOP_HDCP24SEL_UNIVPD52:
		return "top_hdcp24sel_univpd52";
	case TOP_HDCP24SEL_UNIVP2D8:
		return "top_hdcp24sel_univp2d8";

	default:
		return "mediatek,HDMI_UNKNOWN_CLOCK";
	}
}


const char *hdmi_use_module_name_spy(enum HDMI_REF_MODULE_ENUM module)
{
	switch (module) {
	case AP_CCIF0:
		return "mediatek,mt8173-apmixedsys";	/* TVD//PLL */
	case TOPCK_GEN:
		return "mediatek,mt8173-topckgen";
	case INFRA_SYS:
		return "mediatek,mt8173-infracfg";
	case MMSYS_CONFIG:
		return "mediatek,mt8173-mmsys";
	case GPIO_REG:
		return "mediatek,mt8173-pinctrl";
	case DPI0_REG:
		return "mediatek,mt8173-disp_dpi0";
	case DISP_CONFIG2:
		return "mediatek,mt8173-disp_config2";
	case GIC_REG:
		return "arm,gic-400";
	case PERISYS_REG:
		return "mediatek,mt8173-perisys-iommu";

	case HDMI_REF_REG_NUM:
		return "mediatek,HDMI_UNKNOWN";
	default:
		return "mediatek,HDMI_UNKNOWN";
	}
}


static void vInitAvInfoVar(void)
{
	_stAvdAVInfo.e_resolution = HDMI_VIDEO_1280x720p_50Hz;
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

	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);



}

void vSetHDMIMdiTimeOut(unsigned int i4_count)
{
	HDMI_DRV_FUNC();
	hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] = i4_count;

}

/*----------------------------------------------------------------------------*/

static void hdmi_set_util_funcs(const struct HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(struct HDMI_UTIL_FUNCS));
}

/*----------------------------------------------------------------------------*/

static void hdmi_get_params(struct HDMI_PARAMS *params)
{
	memset(params, 0, sizeof(struct HDMI_PARAMS));

	HDMI_DRV_LOG("720p\n");
	params->init_config.vformat = HDMI_VIDEO_1280x720p_50Hz;
	params->init_config.aformat = HDMI_AUDIO_PCM_16bit_48000;

	params->clk_pol = HDMI_POLARITY_FALLING;
	params->de_pol = HDMI_POLARITY_RISING;
	params->vsync_pol = HDMI_POLARITY_RISING;
	params->hsync_pol = HDMI_POLARITY_RISING;

	params->hsync_pulse_width = 40;
	params->hsync_back_porch = 220;
	params->hsync_front_porch = 440;
	params->vsync_pulse_width = 5;
	params->vsync_back_porch = 20;
	params->vsync_front_porch = 5;

	params->rgb_order = HDMI_COLOR_ORDER_RGB;

	params->io_driving_current = IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num = 4;
	params->output_mode = HDMI_OUTPUT_MODE_LCD_MIRROR;
	params->is_force_awake = 1;
	params->is_force_landscape = 1;

	params->scaling_factor = 0;
#ifndef CONFIG_MTK_HDMI_HDCP_SUPPORT
	params->NeedSwHDCP = 1;
#endif
}

static int hdmi_internal_enter(void)
{
	HDMI_DRV_FUNC();
	return 0;

}

static int hdmi_internal_exit(void)
{
	HDMI_DRV_FUNC();
	return 0;
}

/*----------------------------------------------------------------------------*/

static void hdmi_internal_suspend(void)
{
	HDMI_DRV_FUNC();

	/* _stAvdAVInfo.fgHdmiTmdsEnable = 0; */
	/* av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1); */
}

/*----------------------------------------------------------------------------*/

static void hdmi_internal_resume(void)
{
	HDMI_DRV_FUNC();


}

/*----------------------------------------------------------------------------*/

void HDMI_DisableIrq(void)
{
	vWriteHdmiIntMask(0xFF);
}

void HDMI_EnableIrq(void)
{
	vWriteHdmiIntMask(0);
}

void SwitchHDMIVersion(unsigned char hdmi2version)
{
	int ret;
	HDMI_DRV_FUNC();

	if (hdmi2version == TRUE) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG20, HDMI2P0_EN, HDMI2P0_EN);
		/* vWriteIoPadMsk(GPIO_MODE4, HDMI2SCK, SCKMASK);
		vWriteIoPadMsk(GPIO_MODE5, HDMI2SDA, SDAMASK); */
		ret = vSet_DDC_Mode(HDCP_DDC_MODE);
		if (ret)
			pr_err("Can not set HDCP_DDC_MODE\n");

		vWriteHdmiGRLMsk(HPD_DDC_CTRL, DDC2_CLOK << DDC_DELAY_CNT_SHIFT, DDC_DELAY_CNT);
	} else {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG20, 0, HDMI2P0_EN);

		ret = vSet_DDC_Mode(HDMI_DDC_MODE);
		if (ret)
			pr_err("Can not set HDCP_DDC_MODE\n");

		/* vWriteIoPadMsk(GPIO_MODE4, HDMISCK, SCKMASK);
		vWriteIoPadMsk(GPIO_MODE5, HDMISDA, SDAMASK); */
	}
}

void read_hdmi1x_reg(void)
{
	unsigned int i;

	HDMI_PLUG_FUNC();
	for (i = 0; i < 0x9; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14025014 + i * 4, bReadByteHdmiGRL(0x14 + i * 4));
	for (i = 0; i < 0x2; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x140250b8 + i * 4, bReadByteHdmiGRL(0xb8 + i * 4));
	for (i = 0; i < 0xa; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14025154 + i * 4, bReadByteHdmiGRL(0x154 + i * 4));
	for (i = 0; i < 0x4; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x1402518c + i * 4, bReadByteHdmiGRL(0x18c + i * 4));
	for (i = 0; i < 0x1; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x140251c4 + i * 4, bReadByteHdmiGRL(0x1c4 + i * 4));
	for (i = 0; i < 0x2; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14025200 + i * 4, bReadByteHdmiGRL(0x200 + i * 4));
	for (i = 0; i < 0x2; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14025260 + i * 4, bReadByteHdmiGRL(0x260 + i * 4));
}

void read_hdmi2x_reg(void)
{
	unsigned int i;

	HDMI_PLUG_FUNC();
	for (i = 0; i < 0x4; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14025000 + i * 4, bReadByteHdmiGRL(i * 4));
	for (i = 0; i < 0x9; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x140251a0 + i * 4, bReadByteHdmiGRL(0x1a0 + i * 4));
	for (i = 0; i < 0x10; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14025400 + i * 4, bReadByteHdmiGRL(0x400 + i * 4));
	for (i = 0; i < 0x1; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14025c00 + i * 4, bReadByteHdmiGRL(0xc00 + i * 4));
	for (i = 0; i < 0x1; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14025c20 + i * 4, bReadByteHdmiGRL(0xc20 + i * 4));
}

void read_dpi_reg(void)
{
	int i = 0;
	HDMI_PLUG_FUNC();
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x10209000, dReadHdmiANA(0x0));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x10209040, dReadHdmiANA(0x40));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x10209270, dReadHdmiANA(0x270));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x10209274, dReadHdmiANA(0x274));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x1020927c, dReadHdmiANA(0x27c));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x1401d000, *(unsigned int *)(hdmi_ref_reg[DPI0_REG] + 0x0));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x100000a0, dReadHdmiTOPCK(0xa0));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x100000c0, dReadHdmiTOPCK(0xc0));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x100000d0, dReadHdmiTOPCK(0xd0));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x10000134, dReadHdmiTOPCK(0x134));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14000900, dReadHdmiSYS(0x900));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x14000904, dReadHdmiSYS(0x904));
	for (i = 0; i < 0x7c; i += 4)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", i, *(unsigned int *)(hdmi_ref_reg[DPI0_REG] + i));


}
int hdmi_internal_video_config(HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin,
			       enum HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	HDMI_PLUG_LOG("vformat = 0x%x\n", vformat);

	_stAvdAVInfo.e_resolution = vformat;

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;

	if (hdcp2_version_flag == TRUE) {

		pr_err("hdmi2_internal_video_config\n");
		av_hdmiset(HDMI2_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
		av_hdmiset(HDMI2_SET_VPLL, &_stAvdAVInfo, 1);
		av_hdmiset(HDMI2_SET_SOFT_NCTS, &_stAvdAVInfo, 1);
		av_hdmiset(HDMI2_SET_VIDEO_RES_CHG, &_stAvdAVInfo, 1);
		if (get_boot_mode() != FACTORY_BOOT)
			av_hdmiset(HDMI2_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);
	} else {
	      pr_err("hdmi_internal_video_config\n");
			av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
			av_hdmiset(HDMI_SET_VPLL, &_stAvdAVInfo, 1);
			av_hdmiset(HDMI_SET_SOFT_NCTS, &_stAvdAVInfo, 1);
			av_hdmiset(HDMI_SET_VIDEO_RES_CHG, &_stAvdAVInfo, 1);
			if (get_boot_mode() != FACTORY_BOOT)
				av_hdmiset(HDMI_SET_HDCP_INITIAL_AUTH, &_stAvdAVInfo, 1);
		}

	hdmistate_debug = 0;
	return 0;

}

int hdmi_audiosetting(HDMITX_AUDIO_PARA *audio_para)
{
	static HDMITX_AUDIO_PARA last_audio_para;
	int skip_audio_setting = 0;

	HDMI_DRV_FUNC();

	_stAvdAVInfo.e_hdmi_aud_in = audio_para->e_hdmi_aud_in;	/* SV_I2S; */
	_stAvdAVInfo.e_iec_frame = audio_para->e_iec_frame;	/* IEC_48K; */
	_stAvdAVInfo.e_hdmi_fs = audio_para->e_hdmi_fs;	/* HDMI_FS_48K; */
	_stAvdAVInfo.e_aud_code = audio_para->e_aud_code;	/* AVD_LPCM; */
	_stAvdAVInfo.u1Aud_Input_Chan_Cnt = audio_para->u1Aud_Input_Chan_Cnt;	/* AUD_INPUT_2_0; */
	_stAvdAVInfo.e_I2sFmt = audio_para->e_I2sFmt;	/* HDMI_I2S_24BIT; */
	_stAvdAVInfo.u1HdmiI2sMclk = audio_para->u1HdmiI2sMclk;	/* MCLK_128FS; */
	_stAvdAVInfo.bhdmiLChstatus[0] = audio_para->bhdmi_LCh_status[0];
	_stAvdAVInfo.bhdmiLChstatus[1] = audio_para->bhdmi_LCh_status[1];
	_stAvdAVInfo.bhdmiLChstatus[2] = audio_para->bhdmi_LCh_status[2];
	_stAvdAVInfo.bhdmiLChstatus[3] = audio_para->bhdmi_LCh_status[3];
	_stAvdAVInfo.bhdmiLChstatus[4] = audio_para->bhdmi_LCh_status[4];
	_stAvdAVInfo.bhdmiRChstatus[0] = audio_para->bhdmi_RCh_status[0];
	_stAvdAVInfo.bhdmiRChstatus[1] = audio_para->bhdmi_RCh_status[1];
	_stAvdAVInfo.bhdmiRChstatus[2] = audio_para->bhdmi_RCh_status[2];
	_stAvdAVInfo.bhdmiRChstatus[3] = audio_para->bhdmi_RCh_status[3];
	_stAvdAVInfo.bhdmiRChstatus[4] = audio_para->bhdmi_RCh_status[4];

		/* back & compare start */
	if ((_stAvdAVInfo.e_hdmi_aud_in == last_audio_para.e_hdmi_aud_in) &&
			(_stAvdAVInfo.e_iec_frame == last_audio_para.e_iec_frame) &&
			(_stAvdAVInfo.e_hdmi_fs ==	last_audio_para.e_hdmi_fs) &&
			(_stAvdAVInfo.e_aud_code == last_audio_para.e_aud_code) &&
			(_stAvdAVInfo.u1Aud_Input_Chan_Cnt == last_audio_para.u1Aud_Input_Chan_Cnt) &&
			(_stAvdAVInfo.e_I2sFmt == last_audio_para.e_I2sFmt) &&
			(_stAvdAVInfo.u1HdmiI2sMclk == last_audio_para.u1HdmiI2sMclk) &&
			(_stAvdAVInfo.bhdmiLChstatus[0] == last_audio_para.bhdmi_LCh_status[0]) &&
			(_stAvdAVInfo.bhdmiLChstatus[1] == last_audio_para.bhdmi_LCh_status[1]) &&
			(_stAvdAVInfo.bhdmiLChstatus[2] == last_audio_para.bhdmi_LCh_status[2]) &&
			(_stAvdAVInfo.bhdmiLChstatus[3] == last_audio_para.bhdmi_LCh_status[3]) &&
			(_stAvdAVInfo.bhdmiLChstatus[4] == last_audio_para.bhdmi_LCh_status[4]) &&
			(_stAvdAVInfo.bhdmiRChstatus[0] == last_audio_para.bhdmi_RCh_status[0]) &&
			(_stAvdAVInfo.bhdmiRChstatus[1] == last_audio_para.bhdmi_RCh_status[1]) &&
		(_stAvdAVInfo.bhdmiRChstatus[2] == last_audio_para.bhdmi_RCh_status[2]) &&
			(_stAvdAVInfo.bhdmiRChstatus[3] == last_audio_para.bhdmi_RCh_status[3]) &&
			(_stAvdAVInfo.bhdmiRChstatus[4] == last_audio_para.bhdmi_RCh_status[4])) {
			skip_audio_setting = 1;
		} else {
			skip_audio_setting = 0;
			last_audio_para.e_hdmi_aud_in = audio_para->e_hdmi_aud_in;	/* SV_I2S; */
		last_audio_para.e_iec_frame = audio_para->e_iec_frame;	/* IEC_48K; */
			last_audio_para.e_hdmi_fs = audio_para->e_hdmi_fs;	/* HDMI_FS_48K; */
			last_audio_para.e_aud_code = audio_para->e_aud_code;	/* AVD_LPCM; */
			last_audio_para.u1Aud_Input_Chan_Cnt = audio_para->u1Aud_Input_Chan_Cnt;
			last_audio_para.e_I2sFmt = audio_para->e_I2sFmt;	/* HDMI_I2S_24BIT; */
			last_audio_para.u1HdmiI2sMclk = audio_para->u1HdmiI2sMclk;	/* MCLK_128FS; */
			last_audio_para.bhdmi_LCh_status[0] = audio_para->bhdmi_LCh_status[0];
			last_audio_para.bhdmi_LCh_status[1] = audio_para->bhdmi_LCh_status[1];
			last_audio_para.bhdmi_LCh_status[2] = audio_para->bhdmi_LCh_status[2];
			last_audio_para.bhdmi_LCh_status[3] = audio_para->bhdmi_LCh_status[3];
			last_audio_para.bhdmi_LCh_status[4] = audio_para->bhdmi_LCh_status[4];
			last_audio_para.bhdmi_RCh_status[0] = audio_para->bhdmi_RCh_status[0];
			last_audio_para.bhdmi_RCh_status[1] = audio_para->bhdmi_RCh_status[1];
			last_audio_para.bhdmi_RCh_status[2] = audio_para->bhdmi_RCh_status[2];
			last_audio_para.bhdmi_RCh_status[3] = audio_para->bhdmi_RCh_status[3];
			last_audio_para.bhdmi_RCh_status[4] = audio_para->bhdmi_RCh_status[4];
		}

	if (skip_audio_setting == 0) {
		pr_info("hdmi apply new audio setting\n");
	if (hdcp2_version_flag == TRUE)
		av_hdmiset(HDMI2_SET_AUDIO_CHG_SETTING, &_stAvdAVInfo, 1);
	else
		av_hdmiset(HDMI_SET_AUDIO_CHG_SETTING, &_stAvdAVInfo, 1);

	HDMI_DRV_LOG("e_hdmi_aud_in=%d,e_iec_frame=%d,e_hdmi_fs=%d\n", _stAvdAVInfo.e_hdmi_aud_in,
		     _stAvdAVInfo.e_iec_frame, _stAvdAVInfo.e_hdmi_fs);
	HDMI_DRV_LOG("e_aud_code=%d,u1Aud_Input_Chan_Cnt=%d,e_I2sFmt=%d\n", _stAvdAVInfo.e_aud_code,
		     _stAvdAVInfo.u1Aud_Input_Chan_Cnt, _stAvdAVInfo.e_I2sFmt);
	HDMI_DRV_LOG("u1HdmiI2sMclk=%d\n", _stAvdAVInfo.u1HdmiI2sMclk);

	HDMI_DRV_LOG("bhdmiLChstatus0=%d\n", _stAvdAVInfo.bhdmiLChstatus[0]);
	HDMI_DRV_LOG("bhdmiLChstatus1=%d\n", _stAvdAVInfo.bhdmiLChstatus[1]);
	HDMI_DRV_LOG("bhdmiLChstatus2=%d\n", _stAvdAVInfo.bhdmiLChstatus[2]);
	HDMI_DRV_LOG("bhdmiLChstatus3=%d\n", _stAvdAVInfo.bhdmiLChstatus[3]);
	HDMI_DRV_LOG("bhdmiLChstatus4=%d\n", _stAvdAVInfo.bhdmiLChstatus[4]);
	HDMI_DRV_LOG("bhdmiRChstatus0=%d\n", _stAvdAVInfo.bhdmiRChstatus[0]);
	HDMI_DRV_LOG("bhdmiRChstatus1=%d\n", _stAvdAVInfo.bhdmiRChstatus[1]);
	HDMI_DRV_LOG("bhdmiRChstatus2=%d\n", _stAvdAVInfo.bhdmiRChstatus[2]);
	HDMI_DRV_LOG("bhdmiRChstatus3=%d\n", _stAvdAVInfo.bhdmiRChstatus[3]);
	HDMI_DRV_LOG("bhdmiRChstatus4=%d\n", _stAvdAVInfo.bhdmiRChstatus[4]);
	} else {
		pr_info("hdmi skip hdmi audio setting\n");
	}


	return 0;
}

int hdmi_tmdsonoff(unsigned char u1ionoff)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.fgHdmiTmdsEnable = u1ionoff;
	if (hdcp2_version_flag == TRUE)
		av_hdmiset(HDMI2_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);
	else
		av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	return 0;
}

int hdmi2_tmdsonoff(unsigned char u1ionoff)
{
	HDMI_DRV_FUNC();

	_stAvdAVInfo.fgHdmiTmdsEnable = u1ionoff;
	av_hdmiset(HDMI2_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	return 0;
}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_audio_config(enum HDMI_AUDIO_FORMAT aformat)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_video_enable(unsigned char enable)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/

static int hdmi_internal_audio_enable(unsigned char enable)
{
	HDMI_DRV_FUNC();

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

void hdmi_internal_set_mode(unsigned char ucMode)
{
	HDMI_DRV_FUNC();
	vSetClk();

}

/*----------------------------------------------------------------------------*/
void print_clock_reg(void)
{
	HDMI_PLUG_LOG("read bit12/13/14/15/19/20: 0x14000110 = 0x%08x\n", dReadHdmiSYS(MMSYS_CG_CON1));
	HDMI_PLUG_LOG("read bit18: 0x10001044 = 0x%08x\n", dReadINFRASYS(0x44));
	HDMI_PLUG_LOG("read bit18: 0x10001048 = 0x%08x\n", dReadINFRASYS(0x48));
	HDMI_PLUG_LOG("read bit30: 0x10003018 = 0x%08x\n", dReadPeriSYS(0x18));
}

int hdmi_internal_power_on(void)
{
	HDMI_PLUG_FUNC();

	hdmi_is_boot_time = 0;
	if ((get_boot_mode() == FACTORY_BOOT) || (get_boot_mode() == ATE_FACTORY_BOOT))
		factory_boot_mode = true;

	if (hdmi_powerenable == 1)
		return 0;
	hdmi_powerenable = 1;
	/* hdmi_clockenable = 1; */
	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;

	if (hdmi_cec_on == 0) {
		clk_prepare(hdmi_ref_clock[INFRA_SYS_CEC]);
		clk_enable(hdmi_ref_clock[INFRA_SYS_CEC]);
	}

	/* hdmi_clock_enable(true); */
	print_clock_reg();

	if (hdmi_power_control_pin > 0) {
		pr_err("[hdmi]hdmi control pin number is %d\n", hdmi_power_control_pin);
		gpio_direction_output(hdmi_power_control_pin, 1);
		gpio_set_value(hdmi_power_control_pin, 1);
	}
	mtk_smi_larb_clock_on(0, true);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG20, HDMI_PCLK_FREE_RUN, HDMI_PCLK_FREE_RUN);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, ANLG_ON | HDMI_ON, ANLG_ON | HDMI_ON);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG20, 0x0 << 15, 0x1 << 15);

	vInitHdcpKeyGetMethod(NON_HOST_ACCESS_FROM_EEPROM);

	port_hpd_value_bak = 0xff;
	HDMI_EnableIrq();

	if (factory_boot_mode == FALSE) {
		memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
		r_hdmi_timer.expires = jiffies + 1000 / (1000 / HZ);	/* wait 1s to stable */
		r_hdmi_timer.function = hdmi_poll_isr;
		r_hdmi_timer.data = 0;
		init_timer(&r_hdmi_timer);
		add_timer(&r_hdmi_timer);
	}

	atomic_set(&hdmi_irq_event, 1);
	wake_up_interruptible(&hdmi_irq_wq);

	return 0;
}

void vReadHdcpVersion(void)
{
	unsigned char bTemp;
	unsigned int hdmi_hdcp_version;

	if (!fgDDCDataRead(RX_ID, RX_REG_HDCP2VERSION, 1, &bTemp)) {
		pr_err("read hdcp version fail from sink\n");
		hdcp2_version_flag = FALSE;
	} else if (bTemp & 0x4) {
		hdcp2_version_flag = TRUE;
		HDMI_PLUG_LOG("sink support hdcp2.2 version\n");
	} else {
		hdcp2_version_flag = FALSE;
		HDMI_PLUG_LOG("sink support hdcp1.x version\n");
	}

/*	if (hdmi2_force_output == 1)
		hdcp2_version_flag = TRUE;
	else if (hdmi2_force_output == 2)
		hdcp2_version_flag = FALSE; */

	hdmi_hdcp_version = get_devinfo_with_index(HDMI_HDCP_VERSION_INDEX);
	if ((hdmi_hdcp_version & 0x40) && (hdcp2_version_flag == TRUE)) {
		HDMI_PLUG_LOG("chip don't support hdcp2\n");
		hdcp2_version_flag = FALSE;
	}

	if (hdcp2_version_flag == TRUE)
		SwitchHDMIVersion(TRUE);
	else
		SwitchHDMIVersion(FALSE);
}

/*----------------------------------------------------------------------------*/

void hdmi_internal_power_off(void)
{
	HDMI_PLUG_FUNC();

	hdmi_is_boot_time = 0;
	if (hdmi_powerenable == 0)
		return;

	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	hdcp2_version_flag = FALSE;

	HDMI_DisableIrq();

	_stAvdAVInfo.fgHdmiTmdsEnable = 0;
	av_hdmiset(HDMI_SET_TURN_OFF_TMDS, &_stAvdAVInfo, 1);

	SwitchHDMIVersion(FALSE);
	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);

	if (hdmi_clockenable == 1) {
		HDMI_PLUG_LOG("hdmi irq for clock:hdmi plug out\n");
		hdmi_clockenable = 0;
		hdmi_clock_enable(false);
	}

	print_clock_reg();
	mtk_smi_larb_clock_off(0, true);

	if (hdmi_power_control_pin > 0) {
		pr_err("[hdmi]hdmi control pin number is %d\n", hdmi_power_control_pin);
		gpio_direction_output(hdmi_power_control_pin, 0);
		gpio_set_value(hdmi_power_control_pin, 0);
	}

	hdmi_powerenable = 0;
	if (factory_boot_mode != true) {
		if (r_hdmi_timer.function)
			del_timer_sync(&r_hdmi_timer);
		memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	}
}

/*----------------------------------------------------------------------------*/

void hdmi_internal_dump(void)
{
	HDMI_DRV_FUNC();
	/* hdmi_dump_reg(); */
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

enum HDMI_STATE hdmi_get_state(void)
{
	HDMI_DRV_FUNC();

	if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;
}

void hdmi_enablehdcp(unsigned char u1hdcponoff)
{
	HDMI_PLUG_LOG("u1hdcponoff = %d\n", u1hdcponoff);
	_stAvdAVInfo.u1hdcponoff = u1hdcponoff;
	if (hdcp2_version_flag == TRUE)
		av_hdmiset(HDMI2_SET_HDCP_OFF, &_stAvdAVInfo, 1);
	else
		av_hdmiset(HDMI_SET_HDCP_OFF, &_stAvdAVInfo, 1);
}

void hdmi_setcecrxmode(unsigned char u1cecrxmode)
{
	HDMI_DRV_FUNC();
	hdmi_rxcecmode = u1cecrxmode;
}

void hdmi_colordeep(unsigned char u1colorspace, unsigned char u1deepcolor)
{
	HDMI_DRV_FUNC();
	if ((u1colorspace == 0xff) && (u1deepcolor == 0xff)) {
		pr_err("color_space:HDMI_YCBCR_444 = 2\n");
		pr_err("color_space:HDMI_YCBCR_422 = 3\n");

		pr_err("deep_color:HDMI_NO_DEEP_COLOR = 1\n");
		pr_err("deep_color:HDMI_DEEP_COLOR_10_BIT = 2\n");
		pr_err("deep_color:HDMI_DEEP_COLOR_12_BIT = 3\n");
		pr_err("deep_color:HDMI_DEEP_COLOR_16_BIT = 4\n");

		return;
	}

	_stAvdAVInfo.e_video_color_space = HDMI_RGB_FULL;

	_stAvdAVInfo.e_deep_color_bit = (HDMI_DEEP_COLOR_T) u1deepcolor;
}

void hdmi_read(unsigned int u2Reg, unsigned int *p4Data)
{
	switch (u2Reg & 0x1ffff000) {
	case 0x14025000:
		internal_hdmi_read(hdmi_reg[HDMI_SHELL] + u2Reg - 0x14025000, p4Data);
		break;

	case 0x11012000:
		internal_hdmi_read(hdmi_reg[HDMI_DDC] + u2Reg - 0x11012000, p4Data);
		break;

	case 0x10013000:
		internal_hdmi_read(hdmi_reg[HDMI_CEC] + u2Reg - 0x10013000, p4Data);
		break;

	case 0x10209000:
		internal_hdmi_read(hdmi_ref_reg[AP_CCIF0] + u2Reg - 0x10209000, p4Data);
		break;

	case 0x10000000:
		internal_hdmi_read(hdmi_ref_reg[TOPCK_GEN] + u2Reg - 0x10000000, p4Data);
		break;

	case 0x10001000:
		internal_hdmi_read(hdmi_ref_reg[INFRA_SYS] + u2Reg - 0x10001000, p4Data);
		break;

	case 0x14000000:
		internal_hdmi_read(hdmi_ref_reg[MMSYS_CONFIG] + u2Reg - 0x14000000, p4Data);
		break;

	case 0x10005000:
		internal_hdmi_read(hdmi_ref_reg[GPIO_REG] + u2Reg - 0x10005000, p4Data);
		break;

	case 0x1401d000:
		internal_hdmi_read(hdmi_ref_reg[DPI0_REG] + u2Reg - 0x1401d000, p4Data);
		break;

	case 0x10206000:
		internal_hdmi_read(hdmi_ref_reg[DISP_CONFIG2] + u2Reg - 0x10206000, p4Data);
		break;

	case 0x10003000:
		internal_hdmi_read(hdmi_ref_reg[PERISYS_REG] + u2Reg - 0x10003000, p4Data);
		break;

	default:
		break;
	}

}


void vWriteReg(unsigned int u2Reg, unsigned int u4Data)
{
	switch (u2Reg & 0x1ffff000) {
	case 0x14025000:
#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
		vCaHDMIWriteReg(u2Reg - 0x14025000, u4Data);
#else
		internal_hdmi_write(hdmi_reg[HDMI_SHELL] + u2Reg - 0x14025000, u4Data);
#endif
		break;

	case 0x11012000:
		internal_hdmi_write(hdmi_reg[HDMI_DDC] + u2Reg - 0x11012000, u4Data);
		break;

	case 0x10013000:
		internal_hdmi_write(hdmi_reg[HDMI_CEC] + u2Reg - 0x10013000, u4Data);
		break;

	case 0x10209000:
		internal_hdmi_write(hdmi_ref_reg[AP_CCIF0] + u2Reg - 0x10209000, u4Data);
		break;

	case 0x10000000:
		internal_hdmi_write(hdmi_ref_reg[TOPCK_GEN] + u2Reg - 0x10000000, u4Data);
		break;

	case 0x10001000:
		internal_hdmi_write(hdmi_ref_reg[INFRA_SYS] + u2Reg - 0x10001000, u4Data);
		break;

	case 0x14000000:
		internal_hdmi_write(hdmi_ref_reg[MMSYS_CONFIG] + u2Reg - 0x14000000, u4Data);
		break;

	case 0x10005000:
		internal_hdmi_write(hdmi_ref_reg[GPIO_REG] + u2Reg - 0x10005000, u4Data);
		break;

	case 0x1401d000:
		internal_hdmi_write(hdmi_ref_reg[DPI0_REG] + u2Reg - 0x1401d000, u4Data);
		break;

	case 0x10206000:
		internal_hdmi_write(hdmi_ref_reg[DISP_CONFIG2] + u2Reg - 0x10206000, u4Data);
		break;

	case 0x10003000:
		internal_hdmi_write(hdmi_ref_reg[PERISYS_REG] + u2Reg - 0x10003000, u4Data);
		break;

	default:
		break;
	}

}

void hdmi_write(unsigned int u2Reg, unsigned int u4Data)
{
	switch (u2Reg & 0x1ffff000) {
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
		vCaHDMIWriteReg(u2Reg - 0x14025000, u4Data);
#else
		internal_hdmi_write(hdmi_reg[HDMI_SHELL] + u2Reg - 0x14025000, u4Data);
#endif
	break;

	case 0x11012000:
		internal_hdmi_write(hdmi_reg[HDMI_DDC] + u2Reg - 0x11012000, u4Data);
		break;

	case 0x10013000:
		internal_hdmi_write(hdmi_reg[HDMI_CEC] + u2Reg - 0x10013000, u4Data);
		break;

	case 0x10209000:
		internal_hdmi_write(hdmi_ref_reg[AP_CCIF0] + u2Reg - 0x10209000, u4Data);
		break;

	case 0x10000000:
		internal_hdmi_write(hdmi_ref_reg[TOPCK_GEN] + u2Reg - 0x10000000, u4Data);
		break;

	case 0x10001000:
		internal_hdmi_write(hdmi_ref_reg[INFRA_SYS] + u2Reg - 0x10001000, u4Data);
		break;

	case 0x14000000:
		internal_hdmi_write(hdmi_ref_reg[MMSYS_CONFIG] + u2Reg - 0x14000000, u4Data);
		break;

	case 0x10005000:
		internal_hdmi_write(hdmi_ref_reg[GPIO_REG] + u2Reg - 0x10005000, u4Data);
		break;

	case 0x1401d000:
		internal_hdmi_write(hdmi_ref_reg[DPI0_REG] + u2Reg - 0x1401d000, u4Data);
		break;

	case 0x10206000:
		internal_hdmi_write(hdmi_ref_reg[DISP_CONFIG2] + u2Reg - 0x10206000, u4Data);
		break;

	case 0x10003000:
		internal_hdmi_write(hdmi_ref_reg[PERISYS_REG] + u2Reg - 0x10003000, u4Data);
		break;

	default:
		break;
	}

	pr_err("Reg write= 0x%08x, data = 0x%08x\n", u2Reg, u4Data);
}

void read_cec_reg(void)
{
	unsigned int i;

	HDMI_PLUG_FUNC();
	for (i = 0; i < 0x2; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x10013000 + i * 4, dReadHdmiCEC(i * 4));
	for (i = 0; i < 0x2; i++)
		HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x10013054 + i * 4, dReadHdmiCEC(0x54 + i * 4));
	HDMI_PLUG_LOG("0x%08x = 0x%08x\n", 0x10013094, dReadHdmiCEC(0x94));
}

static int hdmi_event_notifier_callback(struct notifier_block *self,
				unsigned long action, void *data)
{
	struct fb_event *event = data;
	int blank_mode = 0;

	if (action != FB_EARLY_EVENT_BLANK)
		return 0;

	if ((event == NULL) || (event->data == NULL))
		return 0;

	blank_mode = *((int *)event->data);

		switch (blank_mode) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
			hdmi_hdmiearlysuspend = 1;
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_HSYNC_SUSPEND:
			break;
		case FB_BLANK_POWERDOWN:
			hdmi_hdmiearlysuspend = 0;
			break;
		default:
			return -EINVAL;
		}
	return 0;
}

static struct notifier_block hdmi_event_notifier = {
	.notifier_call  = hdmi_event_notifier_callback,
};

static irqreturn_t hdmi_irq_handler(int irq, void *dev_id)
{
	unsigned char port_hpd_value;
	HDMI_DRV_LOG();
	vClear_cec_irq();
	vCec_clear_INT_onstandby();

	port_hpd_value = hdmi_get_port_hpd_value();
	if (port_hpd_value != port_hpd_value_bak) {
		port_hpd_value_bak = port_hpd_value;
		atomic_set(&hdmi_irq_event, 1);
		wake_up_interruptible(&hdmi_irq_wq);
	}

	if (hdmi_cec_on == 1)
		hdmi_cec_isrprocess(hdmi_rxcecmode);

	return IRQ_HANDLED;
}

static int hdmi_internal_init(void)
{
	HDMI_DRV_FUNC();

	init_waitqueue_head(&hdmi_timer_wq);
	hdmi_timer_task = kthread_create(hdmi_timer_kthread, NULL, "hdmi_timer_kthread");
	wake_up_process(hdmi_timer_task);

	init_waitqueue_head(&cec_timer_wq);
	cec_timer_task = kthread_create(cec_timer_kthread, NULL, "cec_timer_kthread");
	wake_up_process(cec_timer_task);

	init_waitqueue_head(&hdmi_irq_wq);
	hdmi_irq_task = kthread_create(hdmi_irq_kthread, NULL, "hdmi_irq_kthread");
	wake_up_process(hdmi_irq_task);

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
	fgCaHDMICreate();
#endif
	fb_register_client(&hdmi_event_notifier);


	return 0;
}


static void vNotifyAppHdmiState(unsigned char u1hdmistate)
{
	HDMI_EDID_T get_info;

	HDMI_PLUG_LOG("u1hdmistate = %d, %s\n", u1hdmistate, szHdmiPordStatusStr[u1hdmistate]);
	hdmi_AppGetEdidInfo(&get_info);

	switch (u1hdmistate) {
	case HDMI_PLUG_OUT:
		hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);
		if (hdmi_cec_on) {
			hdmi_SetPhysicCECAddress(0xffff, 0x0);
			hdmi_util.cec_state_callback(HDMI_CEC_STATE_PLUG_OUT);
		}
		break;

	case HDMI_PLUG_IN_AND_SINK_POWER_ON:
		hdmi_util.state_callback(HDMI_STATE_ACTIVE);
		if (hdmi_cec_on) {
			hdmi_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0x4);
			hdmi_util.cec_state_callback(HDMI_CEC_STATE_GET_PA);
		}
		break;

	case HDMI_PLUG_IN_ONLY:
		hdmi_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0xf);
		if (hdmi_cec_on)
			hdmi_util.cec_state_callback(HDMI_CEC_STATE_GET_PA);
		hdmi_util.state_callback(HDMI_STATE_PLUGIN_ONLY);
		break;

	case HDMI_PLUG_IN_CEC:
		if (hdmi_cec_on)
			hdmi_util.state_callback(HDMI_STATE_CEC_UPDATE);
		break;

	default:
		break;

	}
}

void vNotifyAppHdmiCecState(enum HDMI_NFY_CEC_STATE_T u1hdmicecstate)
{
	HDMI_CEC_LOG("u1hdmicecstate = %d, %s\n", u1hdmicecstate,
		     szHdmiCecPordStatusStr[u1hdmicecstate]);

	switch (u1hdmicecstate) {
	case HDMI_CEC_TX_STATUS:
		hdmi_util.cec_state_callback(HDMI_CEC_STATE_TX_STS);
		break;

	case HDMI_CEC_GET_CMD:
		hdmi_util.cec_state_callback(HDMI_CEC_STATE_GET_CMD);
		break;

	default:
		break;
	}
}

static void vPlugDetectService(enum HDMI_CTRL_STATE_T e_state)
{
	unsigned char bData = 0xff;
#ifdef CONFIG_SLIMPORT_COLORADO3
		unsigned int Rx_Bandwidth;
#endif

	HDMI_PLUG_FUNC();

	e_hdmi_ctrl_state = HDMI_STATE_IDLE;

	switch (e_state) {
	case HDMI_STATE_HOT_PLUG_OUT:
		vClearEdidInfo();
		vHDCPReset();
		if (hdmi_clockenable == 1) {
			HDMI_PLUG_LOG("hdmi irq for clock:hdmi plug out\n");
			hdmi_clockenable = 0;
			hdmi_clock_enable(false);
		}
		print_clock_reg();
		bData = HDMI_PLUG_OUT;
		break;

	case HDMI_STATE_HOT_PLUGIN_AND_POWER_ON:
		if (hdmi_clockenable == 0) {
			HDMI_PLUG_LOG("hdmi irq for clock:hdmi plug in\n");
			hdmi_clockenable = 1;
			hdmi_clock_enable(true);
		}
		print_clock_reg();
#ifdef CONFIG_SLIMPORT_ANX3618
		hdmi_checkedid(1);
#else
		hdmi_checkedid(0);
#endif

#ifdef CONFIG_SLIMPORT_COLORADO3
		Rx_Bandwidth = sp_bw_in_Mbps();
		HDMI_PLUG_LOG("[HDMI_STATE_HOT_PLUGIN_AND_POWER_ON] bandwidth = %d\n", Rx_Bandwidth);
		_HdmiSinkAvCap.bandwidth = Rx_Bandwidth;
#endif

		bData = HDMI_PLUG_IN_AND_SINK_POWER_ON;
		break;

	case HDMI_STATE_HOT_PLUG_IN_ONLY:
		if (hdmi_clockenable == 0) {
			HDMI_PLUG_LOG("hdmi irq for clock:hdmi plug in");
			hdmi_clockenable = 1;
			hdmi_clock_enable(true);
		}
		print_clock_reg();
		vClearEdidInfo();
		vHDCPReset();
		hdmi_checkedid(0);
		bData = HDMI_PLUG_IN_ONLY;
		break;

	case HDMI_STATE_IDLE:
		break;

	default:
		break;
	}

	if (bData != 0xff)
		vNotifyAppHdmiState(bData);
}

void hdmi_force_plug_out(void)
{
	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
	vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
}

void hdmi_force_plug_in(void)
{
	hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
	atomic_set(&hdmi_irq_event, 1);
	wake_up_interruptible(&hdmi_irq_wq);
}


void hdmi_drvlog_enable(unsigned short enable)
{
	HDMI_DRV_FUNC();

	if (enable == 0) {
		pr_err("hdmiplllog =   0x1\n");
		pr_err("hdmiceccommandlog =   0x2\n");
		pr_err("hdmitxhotpluglog =  0x4\n");
		pr_err("hdmitxvideolog = 0x8\n");
		pr_err("hdmitxaudiolog = 0x10\n");
		pr_err("hdmihdcplog =  0x20\n");
		pr_err("hdmiceclog =   0x40\n");
		pr_err("hdmiddclog =   0x80\n");
		pr_err("hdmiedidlog =  0x100\n");
		pr_err("hdmidrvlog =   0x200\n");
		pr_err("hdmireglog =	0x400\n");

		pr_err("hdmi_all_log =   0x7ff\n");
	}

	hdmidrv_log_on = enable;
}

void hdmi_timer_impl(void)
{
	if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
	    && ((e_hdcp_ctrl_state == HDCP_WAIT_RI)
		|| (e_hdcp_ctrl_state == HDCP_CHECK_LINK_INTEGRITY))) {
		if (bCheckHDCPStatus(HDCP_STA_RI_RDY)) {
			vSetHDCPState(HDCP_CHECK_LINK_INTEGRITY);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
	}

	if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
	    && ((e_hdcp_ctrl_state == HDCP2_AUTHEN_CHECK)
		|| (e_hdcp_ctrl_state == HDCP2_ENCRYPTION))) {
		if (bReadByteHdmiGRL(TOP_INT_STA00) & HDCP2X_RX_REAUTH_REQ_DDCM_INT_STA) {
			/* vHDCP2InitAuth(); */
			vHDMI2ClearINT();
			vSetHDCPState(HDCP2_AUTHENTICATION);
			vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
			HDMI_DRV_LOG("HDCP2X_RX_REAUTH_REQ_DDCM_INT_STA-->fail.\n");
		}
	}

	if (hdmi_hdmiCmd == HDMI_PLUG_DETECT_CMD) {
		vClearHdmiCmd();
		/* vcheckhdmiplugstate(); */
		/* vPlugDetectService(e_hdmi_ctrl_state); */
	} else if (hdmi_hdmiCmd == HDMI_HDCP_PROTOCAL_CMD) {
		vClearHdmiCmd();
		HdcpService();
	}

	if ((hdmi2_debug == 0) && (hdmi_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
	    && (hdmi_dpi_output == 1)) {
		hdmi2_debug++;
		pr_err("HDMI Debug Output\n");
		TVD_config_pll(resolution_v);
		set_dpi_res(resolution_v);

		if (hdcp2_version_flag == TRUE)
			hdmi2_tmdsonoff(0);

		hdmi_internal_video_config(resolution_v, 0, 0);
	}

	hdmistate_debug++;
	if (hdmistate_debug == 300) {
		if (hdcp2_version_flag == TRUE)
			read_hdmi2x_reg();
		else
			read_hdmi1x_reg();
		read_dpi_reg();
		hdmi_hdmistatus();
	}
}

void cec_timer_impl(void)
{
	if (hdmi_cec_on == 1)
		hdmi_cec_mainloop(hdmi_rxcecmode);
}

void hdmi_irq_impl(void)
{
	unsigned char bTemp;
	unsigned int hdmi_hdcp_version;

	msleep(50);
	if (down_interruptible(&hdmi_update_mutex)) {
		pr_err("[hdmi]can't get semaphore in %s() for boot time\n", __func__);
		return;
	}

	if (hdmi_is_boot_time == 1) {
		HDMI_PLUG_LOG("hdmi_is_boot_time\n");
		if ((hdmi_hotplugstate != HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
				   && (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)) {
			hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
			vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON);
			hdmi_checkedid(0);
			hdmi_util.state_callback(HDMI_STATE_ACTIVE_IN_BOOT);
		} else {
			hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
			vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
			vClearEdidInfo();
			hdmi_util.state_callback(HDMI_STATE_NO_DEVICE_IN_BOOT);
		}
	} else if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
		&& (bCheckPordHotPlug(HOTPLUG_MODE) == TRUE)
		&& (bCheckPordHotPlug(PORD_MODE) == FALSE)) {
		bCheckHDCPStatus(0xfb);
		bReadGRLInt();
		bClearGRLInt(0xff);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
		vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
		HDMI_PLUG_LOG("hdmi plug in only, force plug out\n");
	} else if ((hdmi_hotplugstate != HDMI_STATE_HOT_PLUG_OUT)
	    && (bCheckPordHotPlug(HOTPLUG_MODE) == FALSE)
	    && (hdmi_powerenable == 1)) {
		SwitchHDMIVersion(FALSE);
		hdcp2_version_flag = FALSE;
		bCheckHDCPStatus(0xfb);
		bReadGRLInt();
		bClearGRLInt(0xff);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
		vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
		HDMI_PLUG_LOG("hdmi plug out return\n");
	} else if ((hdmi_hotplugstate != HDMI_STATE_HOT_PLUGIN_AND_POWER_ON)
		   && (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE)
		   && (hdmi_powerenable == 1)) {
		bCheckHDCPStatus(0xfb);
		bReadGRLInt();
		bClearGRLInt(0xff);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUGIN_AND_POWER_ON;
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_IN_AND_SINK_POWER_ON);
		vPlugDetectService(HDMI_STATE_HOT_PLUGIN_AND_POWER_ON);
		HDMI_PLUG_LOG("hdmi plug in return\n");

		if (!fgDDCDataRead(RX_ID, RX_REG_HDCP2VERSION, 1, &bTemp)) {
			pr_err("read hdcp version fail from sink\n");
			hdcp2_version_flag = FALSE;
		} else if (bTemp & 0x4) {
			hdcp2_version_flag = TRUE;
			HDMI_PLUG_LOG("sink support hdcp2.2 version\n");
		} else {
			hdcp2_version_flag = FALSE;
			HDMI_PLUG_LOG("sink support hdcp1.x version\n");
		}
		/* force hdmi2 */
	/*	if (hdmi2_force_output == 1)
			hdcp2_version_flag = TRUE; */

		/* force hdmi1 */
/*		else if (hdmi2_force_output == 2)
			hdcp2_version_flag = FALSE;	*/

		/* u4data = *((unsigned int *)(DISP_CONFIG2_BASE + 0x44)); */
		hdmi_hdcp_version = get_devinfo_with_index(HDMI_HDCP_VERSION_INDEX);
		if ((hdmi_hdcp_version & 0x40) && (hdcp2_version_flag == TRUE)) {
			HDMI_PLUG_LOG("chip don't support hdcp2\n");
			hdcp2_version_flag = FALSE;
		}

		if (hdcp2_version_flag == TRUE)
			SwitchHDMIVersion(TRUE);
		else
			SwitchHDMIVersion(FALSE);
	} else if ((hdmi_hotplugstate != HDMI_STATE_HOT_PLUG_IN_ONLY)
		   && (bCheckPordHotPlug(HOTPLUG_MODE) == TRUE)
		   && (bCheckPordHotPlug(PORD_MODE) == FALSE)) {
		SwitchHDMIVersion(FALSE);
		hdcp2_version_flag = FALSE;
		bCheckHDCPStatus(0xfb);
		bReadGRLInt();
		bClearGRLInt(0xff);
		hdmi_hotplugstate = HDMI_STATE_HOT_PLUG_IN_ONLY;
		vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_STATE_HOT_PLUG_IN_ONLY);
		vPlugDetectService(HDMI_STATE_HOT_PLUG_IN_ONLY);
		HDMI_PLUG_LOG("hdmi plug in only return\n");
	} else if ((hdmi_hotplugstate == HDMI_STATE_HOT_PLUG_OUT)
		&& (bCheckPordHotPlug(HOTPLUG_MODE) == FALSE)
		&& (bCheckPordHotPlug(PORD_MODE) == FALSE)
		&& (hdmi_powerenable == 1) && (hdmi_clockenable == 1)) {
		HDMI_PLUG_LOG("first power on from api-->plugout\n");
		vPlugDetectService(HDMI_STATE_HOT_PLUG_OUT);
		hdcp2_version_flag = FALSE;
		SwitchHDMIVersion(FALSE);
	}
	up(&hdmi_update_mutex);
	/* HDMI_EnableIrq(); */
	if (hdmi_hotplugstate == HDMI_STATE_HOT_PLUG_OUT) {
		hdmi2_debug = 0;
		/* hdcp2_version_flag = FALSE; */
		/* SwitchHDMIVersion(FALSE); */
	}
}

static int hdmi_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_CAMERA_PREVIEW };

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
	struct sched_param param = {.sched_priority = RTPM_PRIO_CAMERA_PREVIEW };

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

static int hdmi_irq_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_irq_wq, atomic_read(&hdmi_irq_event));
		atomic_set(&hdmi_irq_event, 0);
		hdmi_irq_impl();
		if (kthread_should_stop())
			break;
	}
	return 0;
}

void hdmi_poll_isr(unsigned long n)
{
	unsigned int i;

	for (i = 0; i < MAX_HDMI_TMR_NUMBER; i++) {
		if (hdmi_TmrValue[i] >= AVD_TMR_ISR_TICKS) {
			hdmi_TmrValue[i] -= AVD_TMR_ISR_TICKS;

			if ((i == HDMI_PLUG_DETECT_CMD)
			    && (hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] == 0))
				vSendHdmiCmd(HDMI_PLUG_DETECT_CMD);
			else if ((i == HDMI_HDCP_PROTOCAL_CMD)
				 && (hdmi_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		} else if (hdmi_TmrValue[i] > 0) {
			hdmi_TmrValue[i] = 0;

			if ((i == HDMI_PLUG_DETECT_CMD)
			    && (hdmi_TmrValue[HDMI_PLUG_DETECT_CMD] == 0))
				vSendHdmiCmd(HDMI_PLUG_DETECT_CMD);
			else if ((i == HDMI_HDCP_PROTOCAL_CMD)
				 && (hdmi_TmrValue[HDMI_HDCP_PROTOCAL_CMD] == 0))
				vSendHdmiCmd(HDMI_HDCP_PROTOCAL_CMD);
		}
	}

	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);
	mod_timer(&r_hdmi_timer, jiffies + gHDMI_CHK_INTERVAL / (1000 / HZ));
}

void cec_timer_wakeup(void)
{
	memset((void *)&r_cec_timer, 0, sizeof(r_cec_timer));
	r_cec_timer.expires = jiffies + 1000 / (1000 / HZ);	/* wait 1s to stable */
	r_cec_timer.function = cec_poll_isr;
	r_cec_timer.data = 0;
	init_timer(&r_cec_timer);
	add_timer(&r_cec_timer);

	hdmi_is_boot_time = 0;
	atomic_set(&hdmi_irq_event, 1);
	wake_up_interruptible(&hdmi_irq_wq);
	HDMI_PLUG_LOG("0x10013054 = 0x%08x\n", dReadHdmiCEC(0x54));
}

void cec_timer_sleep(void)
{
	if (factory_boot_mode != true) {
		if (r_cec_timer.function)
			del_timer_sync(&r_cec_timer);
		memset((void *)&r_cec_timer, 0, sizeof(r_cec_timer));
	}
}

void cec_poll_isr(unsigned long n)
{
	atomic_set(&cec_timer_event, 1);
	wake_up_interruptible(&cec_timer_wq);
	mod_timer(&r_cec_timer, jiffies + gCEC_CHK_INTERVAL / (1000 / HZ));
}

void hdmi_clock_enable(bool bEnable)
{
	int i;

	if (bEnable) {
		HDMI_DRV_LOG("Enable hdmi clocks\n");

		for (i = 0; i < TOP_HDMI_SEL; i++) {
			if (i != INFRA_SYS_CEC) {
				clk_prepare(hdmi_ref_clock[i]);
				clk_enable(hdmi_ref_clock[i]);
			}
		}
	} else {
		HDMI_DRV_LOG("Disable hdmi clocks\n");

		for (i = TOP_HDMI_SEL - 1; i >= 0; i--) {
			if (i != INFRA_SYS_CEC) {
				clk_disable(hdmi_ref_clock[i]);
				clk_unprepare(hdmi_ref_clock[i]);
			}
		}
	}
}

void hdmi_clock_probe(struct platform_device *pdev)
{
	int i;

	HDMI_DRV_LOG("Probe clocks start\n");

	/* ASSERT(pdev != NULL); */
	if (pdev == NULL) {
		pr_err("[HDMI] pdev Error\n");
		return;
	}

	pm_runtime_enable(&pdev->dev);
	hdmi_pdev = pdev;


	for (i = 0; i < HDMI_SEL_CLOCK_NUM; i++) {
		hdmi_ref_clock[i] = devm_clk_get(&pdev->dev, hdmi_use_clock_name_spy(i));
		BUG_ON(IS_ERR(hdmi_ref_clock[i]));

		HDMI_DRV_LOG("Get Clock %s\n", hdmi_use_clock_name_spy(i));
	}
}

int hdmi_internal_probe(struct platform_device *pdev, unsigned long u8Res)
{
	int i;
	unsigned int reg_value;
	struct device_node *np;
	int ret = 0;

	HDMI_DRV_LOG("[hdmi_internal_probe] probe start\n");

	if (pdev->dev.of_node == NULL) {
		pr_err("[hdmi_internal_probe] Device Node Error\n");
		return -1;
	}
	/* hdmi_internal_dev->dev = dev; */
	/* iomap registers and irq of HDMI Module */
	for (i = 0; i < HDMI_REG_NUM; i++) {
		hdmi_reg[i] = (unsigned long)of_iomap(pdev->dev.of_node, i);
		if (!hdmi_reg[i]) {
			HDMI_DRV_LOG("Unable to ioremap registers, of_iomap fail, i=%d\n", i);
			return -ENOMEM;
		}
		HDMI_DRV_LOG("DT, i=%d, map_addr=0x%lx, reg_pa=0x%x\n",
			     i, hdmi_reg[i], hdmi_reg_pa_base[i]);
	}

	/*get IRQ ID and request IRQ */
	pr_err("get IRQ ID and request IRQ\n");
	hdmi_irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	/* Get PLL/TOPCK/INFRA_SYS/MMSSYS /GPIO/EFUSE  phy base address and iomap virtual address */
	for (i = 0; i < HDMI_REF_REG_NUM; i++) {
		np = of_find_compatible_node(NULL, NULL, hdmi_use_module_name_spy(i));
		if (np == NULL)
			continue;

		of_property_read_u32_index(np, "reg", 0, &reg_value);
		hdmi_ref_reg_pa_base[i] = reg_value;
		hdmi_ref_reg[i] = (unsigned long)of_iomap(np, 0);
		HDMI_DRV_LOG("DT HDMI_Ref|%s, reg base: 0x%x --> map:0x%lx\n",
			     np->name, reg_value, hdmi_ref_reg[i]);
	}

	/*Probe Clocks that will be used by hdmi module */
	hdmi_clock_probe(pdev);

	/*from hdmi timer initial*/
	print_clock_reg();
	udelay(2);
	/* power cec module for cec and hpd/port detect */
	hdmi_cec_power_on(1);
	vCec_poweron_32k_26m(cec_clock);
	vCec_clear_INT_onstandby();
	vHotPlugPinInit(pdev);
	vGet_DDC_Mode(pdev);

	np = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-hdmitx");

	hdmi_power_control_pin = of_get_named_gpio(np, "hdmi_power_control", 0);
	pr_err("hdmi_power_control_pin = %d\n", hdmi_power_control_pin);
#if !defined(MTK_ALPS_BOX_SUPPORT)
	if (hdmi_power_control_pin > 0) {
		ret = gpio_request(hdmi_power_control_pin, "hdmi power control pin");
	if (ret)
		pr_err("hdmi power control pin, failure of setting\n");
	}
#endif

	vInitAvInfoVar();
	if (u8Res < HDMI_VIDEO_RESOLUTION_NUM)
		_stAvdAVInfo.e_resolution = (unsigned char)u8Res;

	if (factory_boot_mode != TRUE) {
		if (request_irq
			(hdmi_irq, hdmi_irq_handler, IRQF_TRIGGER_LOW, "mtkhdmi", NULL) < 0) {
			HDMI_DRV_LOG("request interrupt failed.\n");
		}
	}
	/* HDMI_EnableIrq(); */

	hdmi_is_boot_time = 1;
	atomic_set(&hdmi_irq_event, 1);
	wake_up_interruptible(&hdmi_irq_wq);

	return 0;

}


const struct HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const struct HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = hdmi_set_util_funcs,
		.get_params = hdmi_get_params,
		.hdmidrv_probe = hdmi_internal_probe,
		.init = hdmi_internal_init,
		.enter = hdmi_internal_enter,
		.exit = hdmi_internal_exit,
		.suspend = hdmi_internal_suspend,
		.resume = hdmi_internal_resume,
		.video_config = hdmi_internal_video_config,
		.audio_config = hdmi_internal_audio_config,
		.video_enable = hdmi_internal_video_enable,
		.audio_enable = hdmi_internal_audio_enable,
		.power_on = hdmi_internal_power_on,
		.power_off = hdmi_internal_power_off,
		.set_mode = hdmi_internal_set_mode,
		.dump = hdmi_internal_dump,
		.read = hdmi_read,
		.write = hdmi_write,
		.get_state = hdmi_get_state,
		.log_enable = hdmi_drvlog_enable,
		.InfoframeSetting = hdmi_InfoframeSetting,
		.checkedid = hdmi_checkedid,
		.colordeep = hdmi_colordeep,
		.enablehdcp = hdmi_enablehdcp,
		.setcecrxmode = hdmi_setcecrxmode,
		.hdmistatus = hdmi_hdmistatus,
		.hdcpkey = hdmi_hdcpkey,
		.getedid = hdmi_AppGetEdidInfo,
		.setcecla = hdmi_CECMWSetLA,
		.sendsltdata = hdmi_u4CecSendSLTData,
		.getceccmd = hdmi_CECMWGet,
		.getsltdata = hdmi_GetSLTData,
		.setceccmd = hdmi_CECMWSend,
		.cecenable = hdmi_CECMWSetEnableCEC,
		.getcecaddr = hdmi_NotifyApiCECAddress,
		.getcectxstatus = hdmi_cec_api_get_txsts,
		.audiosetting = hdmi_audiosetting,
		.tmdsonoff = hdmi_tmdsonoff,
		.mutehdmi = vDrm_mutehdmi,
		.svpmutehdmi = vSvp_mutehdmi,
		.cecusrcmd = hdmi_cec_usr_cmd,
		.checkedidheader = hdmi_check_edid_header,
		/* .set3dstruct = hdmi_set_3d_struct, */
		.gethdmistatus = hdmi_check_status,
	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);
#endif

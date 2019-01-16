/*----------------------------------------------------------------------------*/
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

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
#include <linux/earlysuspend.h>
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
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include "mt8193_ctrl.h"

#include "mt8193ddc.h"
#include "mt8193hdcp.h"

#include "hdmi_drv.h"
#include <cust_eint.h>
#include "cust_gpio_usage.h"
#include "mach/eint.h"
#include "mach/irqs.h"
#include "mach/mt_boot.h"

#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


#include "mt8193_iic.h"
#include "mt8193avd.h"
#include "mt8193hdmicmd.h"


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

size_t mt8193_log_on = 0;
size_t mt8193_cec_on = 0;
size_t mt8193_cec_interrupt = 0;
size_t mt8193_cecinit = 0;
size_t mt8193_hdmiinit = 0;
size_t mt8193_hotinit = 0;

size_t mt8193_hdmipoweroninit = 0;
size_t mt8193_TmrValue[MAX_HDMI_TMR_NUMBER] = { 0 };

size_t mt8193_hdmiCmd = 0xff;
size_t mt8193_rxcecmode = CEC_NORMAL_MODE;
HDMI_CTRL_STATE_T e_hdmi_ctrl_state = HDMI_STATE_IDLE;
HDCP_CTRL_STATE_T e_hdcp_ctrl_state = HDCP_RECEIVER_NOT_READY;
size_t mt8193_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;

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

static HDMI_UTIL_FUNCS hdmi_util = { 0 };

void hdmi_poll_isr(unsigned long n);
void cec_poll_isr(unsigned long n);

static int hdmi_timer_kthread(void *data);
static int cec_timer_kthread(void *data);
static int mt8193_nlh_kthread(void *data);

static void vInitAvInfoVar(void)
{
	_stAvdAVInfo.e_resolution = HDMI_VIDEO_1280x720p_50Hz;
	_stAvdAVInfo.fgHdmiOutEnable = TRUE;
	_stAvdAVInfo.fgHdmiTmdsEnable = TRUE;

	_stAvdAVInfo.bMuteHdmiAudio = FALSE;
	_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_444;
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

void vSetHDMIMdiTimeOut(u32 i4_count)
{
	MT8193_DRV_FUNC();
	mt8193_TmrValue[HDMI_PLUG_DETECT_CMD] = i4_count;

}

/*----------------------------------------------------------------------------*/

static void mt8193_set_util_funcs(const HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(HDMI_UTIL_FUNCS));
}

/*----------------------------------------------------------------------------*/

static void mt8193_get_params(HDMI_PARAMS *params)
{
	memset(params, 0, sizeof(HDMI_PARAMS));

	MT8193_DRV_LOG("720p\n");
	params->init_config.vformat = HDMI_VIDEO_1280x720p_50Hz;
	params->init_config.aformat = HDMI_AUDIO_PCM_16bit_48000;

	params->clk_pol = HDMI_POLARITY_FALLING;
	params->de_pol = HDMI_POLARITY_RISING;
	params->vsync_pol = HDMI_POLARITY_FALLING;
	params->hsync_pol = HDMI_POLARITY_FALLING;

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

static int mt8193_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
			       HDMI_VIDEO_OUTPUT_FORMAT vout)
{
	MT8193_DRV_FUNC();
	if (r_hdmi_timer.function) {
		del_timer_sync(&r_hdmi_timer);
	}
	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));

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

	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	r_hdmi_timer.expires = jiffies + 1 / (1000 / HZ);	/* wait 1s to stable */
	r_hdmi_timer.function = hdmi_poll_isr;
	r_hdmi_timer.data = 0;
	init_timer(&r_hdmi_timer);
	add_timer(&r_hdmi_timer);
	mt8193_hdmiinit = 1;

	return 0;
}

/*----------------------------------------------------------------------------*/

static int mt8193_audio_config(HDMI_AUDIO_FORMAT aformat)
{
	MT8193_DRV_FUNC();

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
	MT8193_DRV_FUNC();
#if defined(CONFIG_HAS_EARLYSUSPEND)
	if (mt8193_hdmiearlysuspend == 0)
		return 0;
#endif
	mt8193_hotinit = 0;
	mt8193_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, GPIO_OUT_ONE);

	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_power_turnon, hdmi_power_turnon);
	vWriteHdmiSYSMsk(HDMI_SYS_PWR_RST_B, hdmi_pwr_sys_sw_unreset, hdmi_pwr_sys_sw_unreset);
	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_iso_dis, hdmi_iso_en);
	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_clock_on, hdmi_clock_off);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, ANLG_ON | HDMI_ON, ANLG_ON | HDMI_ON);

	mt8193_i2c_write(0x1500, 0x20);
	vHotPlugPinInit();
	vInitHdcpKeyGetMethod(NON_HOST_ACCESS_FROM_EEPROM);


	vWriteHdmiIntMask(0xFF);

	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	r_hdmi_timer.expires = jiffies + 100 / (1000 / HZ);	/* wait 1s to stable */
	r_hdmi_timer.function = hdmi_poll_isr;
	r_hdmi_timer.data = 0;
	init_timer(&r_hdmi_timer);
	add_timer(&r_hdmi_timer);

	memset((void *)&r_cec_timer, 0, sizeof(r_cec_timer));
	r_cec_timer.expires = jiffies + 100 / (1000 / HZ);	/* wait 1s to stable */
	r_cec_timer.function = cec_poll_isr;
	r_cec_timer.data = 0;
	init_timer(&r_cec_timer);
	add_timer(&r_cec_timer);

	return 0;
}

/*----------------------------------------------------------------------------*/

void mt8193_power_off(void)
{
	MT8193_DRV_FUNC();

	mt8193_hotinit = 1;
	mt8193_hotplugstate = HDMI_STATE_HOT_PLUG_OUT;
	vSetSharedInfo(SI_HDMI_RECEIVER_STATUS, HDMI_PLUG_OUT);
	vWriteHdmiIntMask(0xFF);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, 0, ANLG_ON | HDMI_ON);

	mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, GPIO_OUT_ZERO);

	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_clock_off, hdmi_clock_off);
	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_iso_en, hdmi_iso_en);
	vWriteHdmiSYSMsk(HDMI_SYS_PWR_RST_B, hdmi_pwr_sys_sw_reset, hdmi_pwr_sys_sw_unreset);
	vWriteHdmiSYSMsk(HDMI_PWR_CTRL, hdmi_power_turnoff, hdmi_power_turnon);
	if (r_hdmi_timer.function) {
		del_timer_sync(&r_hdmi_timer);
	}
	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	if (r_cec_timer.function) {
		del_timer_sync(&r_cec_timer);
	}
	memset((void *)&r_cec_timer, 0, sizeof(r_cec_timer));
}

/*----------------------------------------------------------------------------*/

void mt8193_dump(void)
{
	MT8193_DRV_FUNC();
	/* mt8193_dump_reg(); */
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

HDMI_STATE mt8193_get_state(void)
{
	MT8193_DRV_FUNC();

	if (bCheckPordHotPlug(PORD_MODE | HOTPLUG_MODE) == TRUE) {
		return HDMI_STATE_ACTIVE;
	} else {
		return HDMI_STATE_NO_DEVICE;
	}
}

/*----------------------------------------------------------------------------*/


void mt8193_log_enable(u16 enable)
{
	MT8193_DRV_FUNC();

	if (enable == 0) {
		pr_debug("hdmi_pll_log =   0x1\n");
		pr_debug("hdmi_dgi_log =   0x2\n");
		pr_debug("hdmi_plug_log =  0x4\n");
		pr_debug("hdmi_video_log = 0x8\n");
		pr_debug("hdmi_audio_log = 0x10\n");
		pr_debug("hdmi_hdcp_log =  0x20\n");
		pr_debug("hdmi_cec_log =   0x40\n");
		pr_debug("hdmi_ddc_log =   0x80\n");
		pr_debug("hdmi_edid_log =  0x100\n");
		pr_debug("hdmi_drv_log =   0x200\n");

		pr_debug("hdmi_all_log =   0xffff\n");

	}

	mt8193_log_on = enable;

}

/*----------------------------------------------------------------------------*/

void mt8193_enablehdcp(u8 u1hdcponoff)
{
	MT8193_DRV_FUNC();
	_stAvdAVInfo.u1hdcponoff = u1hdcponoff;
	av_hdmiset(HDMI_SET_HDCP_OFF, &_stAvdAVInfo, 1);
}

void mt8193_setcecrxmode(u8 u1cecrxmode)
{
	MT8193_DRV_FUNC();
	mt8193_rxcecmode = u1cecrxmode;
}

void mt8193_colordeep(u8 u1colorspace, u8 u1deepcolor)
{
	MT8193_DRV_FUNC();
	if ((u1colorspace == 0xff) && (u1deepcolor == 0xff)) {
		pr_debug("color_space:HDMI_YCBCR_444 = 2\n");
		pr_debug("color_space:HDMI_YCBCR_422 = 3\n");

		pr_debug("deep_color:HDMI_NO_DEEP_COLOR = 1\n");
		pr_debug("deep_color:HDMI_DEEP_COLOR_10_BIT = 2\n");
		pr_debug("deep_color:HDMI_DEEP_COLOR_12_BIT = 3\n");
		pr_debug("deep_color:HDMI_DEEP_COLOR_16_BIT = 4\n");

		return;
	}
	if (dReadHdmiSYS(0x2cc) == 0x8193)
		_stAvdAVInfo.e_video_color_space = HDMI_YCBCR_444;
	else
		_stAvdAVInfo.e_video_color_space = HDMI_RGB;

	_stAvdAVInfo.e_deep_color_bit = (HDMI_DEEP_COLOR_T) u1deepcolor;
}

void mt8193_read(u16 u2Reg, u32 *p4Data)
{
	if (u2Reg & 0x8000) {
		if ((u2Reg & 0xf000) == 0x8000)
			u2Reg -= 0x8000;
		*p4Data = (*(unsigned int *)(0xf4000000 + u2Reg));
	} else
		mt8193_i2c_read(u2Reg, p4Data);

	pr_debug("Reg read= 0x%04x, data = 0x%08x\n", u2Reg, *p4Data);
}

void mt8193_write(u16 u2Reg, u32 u4Data)
{
	if (u2Reg & 0x8000) {
		if ((u2Reg & 0xf000) == 0x8000)
			u2Reg -= 0x8000;
		*(unsigned int *)(0xf4000000 + u2Reg) = u4Data;
	} else {
		pr_debug("Reg write= 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
		mt8193_i2c_write(u2Reg, u4Data);
	}
}

static void _mt8193_irq_handler(void)
{
	MT8193_DRV_FUNC();
	atomic_set(&mt8193_nlh_event, 1);
	wake_up_interruptible(&mt8193_nlh_wq);

	mt65xx_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mt8193_hdmi_early_suspend(struct early_suspend *h)
{
	MT8193_PLUG_FUNC();
	mt8193_hdmiearlysuspend = 0;
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

static int mt8193_init(void)
{
	MT8193_DRV_FUNC();

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


	return 0;
}

static void vSetHDMICtrlState(HDMI_CTRL_STATE_T e_state)
{
	MT8193_PLUG_FUNC();
	e_hdmi_ctrl_state = e_state;
}

static void vNotifyAppHdmiState(u8 u1hdmistate)
{
	HDMI_EDID_INFO_T get_info;

	MT8193_PLUG_LOG("u1hdmistate = %d\n", u1hdmistate);

	mt8193_AppGetEdidInfo(&get_info);

	switch (u1hdmistate) {
	case HDMI_PLUG_OUT:
		hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);
		mt8193_SetPhysicCECAddress(0xffff, 0x0);
		break;

	case HDMI_PLUG_IN_AND_SINK_POWER_ON:
		hdmi_util.state_callback(HDMI_STATE_ACTIVE);
		mt8193_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0x4);
		break;

	case HDMI_PLUG_IN_ONLY:
		hdmi_util.state_callback(HDMI_STATE_PLUGIN_ONLY);
		mt8193_SetPhysicCECAddress(get_info.ui2_sink_cec_address, 0xf);
		break;

	case HDMI_PLUG_IN_CEC:
		hdmi_util.state_callback(HDMI_STATE_CEC_UPDATE);
		break;

	default:
		break;

	}
}

void vcheckhdmiplugstate(void)
{
	u32 bMask;
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
	u8 bData = 0xff;

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

	if (bData != 0xff)
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
	if (mt8193_hdmiearlysuspend == 1)
#endif
	{
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

	if (mt8193_cec_on == 1) {
		mt8193_cec_mainloop(mt8193_rxcecmode);
	}
}

void mt8193_nlh_impl(void)
{
	u32 u4Data;
	u8 bData, bData1;
	u8 bMask;

	/* read register and then assert which interrupt occured */
	mt8193_i2c_read(0x1508, &u4Data);
	mt8193_i2c_write(0x1504, 0xffffffff);

	MT8193_DRV_LOG("0x1508 = 0x%08x\n", u4Data);

	if (u4Data & 0x20) {
		MT8193_CEC_LOG("cec interrupt\n");

		if (mt8193_cec_on == 1) {
			if (mt8193_cec_isrprocess(mt8193_rxcecmode)) {
				vNotifyAppHdmiState(HDMI_PLUG_IN_CEC);
			}
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
	mt65xx_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
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

static int mt8193_nlh_kthread(void *data)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };
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
	u32 i;

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
	atomic_set(&cec_timer_event, 1);
	wake_up_interruptible(&cec_timer_wq);
	mod_timer(&r_cec_timer, jiffies + gCEC_CHK_INTERVAL / (1000 / HZ));

}

const HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const HDMI_DRIVER HDMI_DRV = {
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
	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);
#endif

#if defined(CONFIG_MTK_HDMI_SUPPORT)
#include <linux/kernel.h>

#include <linux/xlog.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include "mhl_linuxdrv.h"
#include "osal/include/osal.h"
#include "si_mhl_tx_api.h"
#include "si_cra.h"
#include "si_drvisrconfig.h"
#include "si_drv_mhl_tx.h"
#include "si_drv_mdt_tx.h"
#include "si_mhl_tx.h"

#include <mtk_kpd.h>		/* custom file */

#include "hdmi_drv.h"
#include "hdmi_cust.h"


#if 1
#include <mach/irqs.h>
#include "mach/eint.h"
#include "mach/irqs.h"
#endif


static size_t hdmi_log_on = true;
#define HDMI_LOG(fmt, arg...) \
	do { \
		if (hdmi_log_on) printk("[hdmi_drv]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); \
	} while (0)

#define HDMI_FUNC()    \
	do { \
		if (hdmi_log_on) printk("[hdmi_drv] %s\n", __func__); \
	} while (0)

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

/* static struct task_struct *hdmi_event_task = NULL; */
/* static struct task_struct *hdmi_hpd_detect_task = NULL; */
/* static int hdmi_event_status = HDMI_STATE_NO_DEVICE; */

wait_queue_head_t hdmi_event_wq;
atomic_t hdmi_event = ATOMIC_INIT(0);

static HDMI_UTIL_FUNCS hdmi_util = { 0 };

#define SET_RESET_PIN(v)    (hdmi_util.set_reset_pin((v)))

#define UDELAY(n) (hdmi_util.udelay(n))
#define MDELAY(n) (hdmi_util.mdelay(n))

/*************************************RCP function report added by garyyuan*********************************/
static struct input_dev *kpd_input_dev = NULL;
HDMI_CABLE_TYPE MHL_Connect_type = MHL_CABLE;

void mhl_init_rmt_input_dev(void)
{
	int ret;
	printk(KERN_INFO "%s:%d:.................................................\n", __func__,
	       __LINE__);
	kpd_input_dev = input_allocate_device();
	if (!kpd_input_dev) {
		printk("\nError!!!failed to allocate input device, no memory!!!%s:%d:.....\n",
		       __func__, __LINE__);
		return /*-ENOMEM*/;
	}
#if 1
	kpd_input_dev->name = "mhl-keyboard";
	set_bit(EV_KEY, kpd_input_dev->evbit);
	set_bit(KEY_SELECT, kpd_input_dev->keybit);
	set_bit(KEY_UP, kpd_input_dev->keybit);
	set_bit(KEY_DOWN, kpd_input_dev->keybit);
	set_bit(KEY_LEFT, kpd_input_dev->keybit);
	set_bit(KEY_RIGHT, kpd_input_dev->keybit);
	/* set_bit(KEY_RIGHT_UP, kpd_input_dev->keybit); */
	/* set_bit(KEY_RIGHT_DOWN , kpd_input_dev->keybit); */
	/* set_bit(KEY_LEFT_UP,kpd_input_dev->keybit); */
	/* set_bit(KEY_LEFT_DOWN, kpd_input_dev->keybit); */

	set_bit(KEY_MENU, kpd_input_dev->keybit);
	/* set_bit(KEY_SETUP, kpd_input_dev->keybit); */

	set_bit(KEY_EXIT, kpd_input_dev->keybit);

	/* set_bit(KEY_CONTEXT_MENU ,kpd_input_dev->keybit); */
	/* set_bit(KEY_FAVORITES, kpd_input_dev->keybit); */
	/* set_bit(KEY_EXIT, kpd_input_dev->keybit); */


	set_bit(KEY_0, kpd_input_dev->keybit);
	set_bit(KEY_1, kpd_input_dev->keybit);
	set_bit(KEY_2, kpd_input_dev->keybit);
	set_bit(KEY_3, kpd_input_dev->keybit);
	set_bit(KEY_4, kpd_input_dev->keybit);
	set_bit(KEY_5, kpd_input_dev->keybit);
	set_bit(KEY_6, kpd_input_dev->keybit);
	set_bit(KEY_7, kpd_input_dev->keybit);
	set_bit(KEY_8, kpd_input_dev->keybit);
	set_bit(KEY_9, kpd_input_dev->keybit);

	set_bit(KEY_DOT, kpd_input_dev->keybit);
	set_bit(KEY_ENTER, kpd_input_dev->keybit);
	set_bit(KEY_CLEAR, kpd_input_dev->keybit);
	/* set_bit(KEY_CHANNELUP, kpd_input_dev->keybit); */
	/* set_bit(KEY_CHANNELDOWN, kpd_input_dev->keybit); */
	/* set_bit(KEY_CHANNEL_PREV, kpd_input_dev->keybit); */

	set_bit(KEY_SOUND, kpd_input_dev->keybit);
	/* set_bit(KEY_INFO, kpd_input_dev->keybit); */
	/* set_bit(KEY_HELP, kpd_input_dev->keybit); */
	/* set_bit(KEY_PAGEUP,kpd_input_dev->keybit); */
	/* set_bit(KEY_PAGEDOWN, kpd_input_dev->keybit); */
	set_bit(KEY_VOLUMEUP, kpd_input_dev->keybit);
	set_bit(KEY_VOLUMEDOWN, kpd_input_dev->keybit);
	set_bit(KEY_MUTE, kpd_input_dev->keybit);

	set_bit(KEY_PLAY, kpd_input_dev->keybit);
	set_bit(KEY_STOP, kpd_input_dev->keybit);
	/* set_bit(KEY_PAUSE, kpd_input_dev->keybit); */
	set_bit(KEY_PAUSECD, kpd_input_dev->keybit);

	/* set_bit(KEY_RECORD,kpd_input_dev->keybit); */

	set_bit(KEY_REWIND, kpd_input_dev->keybit);
	set_bit(KEY_FASTFORWARD, kpd_input_dev->keybit);
	set_bit(KEY_EJECTCD, kpd_input_dev->keybit);
	/* set_bit(KEY_FORWARD, kpd_input_dev->keybit); */
	set_bit(KEY_NEXTSONG, kpd_input_dev->keybit);
	set_bit(KEY_BACK, kpd_input_dev->keybit);
	set_bit(KEY_PREVIOUSSONG, kpd_input_dev->keybit);

	/* set_bit(KEY_ANGLE, kpd_input_dev->keybit); */
	/* set_bit(KEY_RESTART, kpd_input_dev->keybit); */
	set_bit(KEY_PLAYPAUSE, kpd_input_dev->keybit);

	ret = input_register_device(kpd_input_dev);
	if (ret) {
		TX_DEBUG_PRINT(("\nError!!!failed to register input device %s:%d\n", __func__,
				__LINE__));
		input_free_device(kpd_input_dev);
	}
#endif
}

/* input key conversion */
void input_report_rcp_key(uint8_t rcp_keycode, int up_down)
{
	rcp_keycode &= 0x7F;

	TX_DEBUG_PRINT(("\nupdown = %d ", up_down));
	switch (rcp_keycode) {
	case MHL_RCP_CMD_SELECT:	/* error */
		input_report_key(kpd_input_dev, KEY_SELECT, up_down);
		TX_DEBUG_PRINT(("\nSelect received\n\n"));
		break;
	case MHL_RCP_CMD_UP:
		input_report_key(kpd_input_dev, KEY_UP, up_down);
		TX_DEBUG_PRINT(("\nUp received\n\n"));
		break;
	case MHL_RCP_CMD_DOWN:
		input_report_key(kpd_input_dev, KEY_DOWN, up_down);
		TX_DEBUG_PRINT(("\nDown received\n\n"));
		break;
	case MHL_RCP_CMD_LEFT:
		input_report_key(kpd_input_dev, KEY_LEFT, up_down);
		TX_DEBUG_PRINT(("\nLeft received\n\n"));
		break;
	case MHL_RCP_CMD_RIGHT:
		input_report_key(kpd_input_dev, KEY_RIGHT, up_down);
		TX_DEBUG_PRINT(("\nRight received\n\n"));
		break;
	case MHL_RCP_CMD_ROOT_MENU:
		input_report_key(kpd_input_dev, KEY_MENU, up_down);
		TX_DEBUG_PRINT(("\nRoot Menu received\n\n"));
		break;
	case MHL_RCP_CMD_EXIT:
		input_report_key(kpd_input_dev, KEY_BACK, up_down);
		TX_DEBUG_PRINT(("\nExit received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_0:
		input_report_key(kpd_input_dev, KEY_0, up_down);
		TX_DEBUG_PRINT(("\nNumber 0 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_1:
		input_report_key(kpd_input_dev, KEY_1, up_down);
		TX_DEBUG_PRINT(("\nNumber 1 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_2:
		input_report_key(kpd_input_dev, KEY_2, up_down);
		TX_DEBUG_PRINT(("\nNumber 2 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_3:
		input_report_key(kpd_input_dev, KEY_3, up_down);
		TX_DEBUG_PRINT(("\nNumber 3 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_4:
		input_report_key(kpd_input_dev, KEY_4, up_down);
		TX_DEBUG_PRINT(("\nNumber 4 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_5:
		input_report_key(kpd_input_dev, KEY_5, up_down);
		TX_DEBUG_PRINT(("\nNumber 5 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_6:
		input_report_key(kpd_input_dev, KEY_6, up_down);
		TX_DEBUG_PRINT(("\nNumber 6 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_7:
		input_report_key(kpd_input_dev, KEY_7, up_down);
		TX_DEBUG_PRINT(("\nNumber 7 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_8:
		input_report_key(kpd_input_dev, KEY_8, up_down);
		TX_DEBUG_PRINT(("\nNumber 8 received\n\n"));
		break;
	case MHL_RCP_CMD_NUM_9:
		input_report_key(kpd_input_dev, KEY_9, up_down);
		TX_DEBUG_PRINT(("\nNumber 9 received\n\n"));
		break;
	case MHL_RCP_CMD_DOT:
		input_report_key(kpd_input_dev, KEY_DOT, up_down);
		TX_DEBUG_PRINT(("\nDot received\n\n"));
		break;
	case MHL_RCP_CMD_ENTER:
		input_report_key(kpd_input_dev, KEY_ENTER, up_down);
		TX_DEBUG_PRINT(("\nEnter received\n\n"));
		break;
	case MHL_RCP_CMD_CLEAR:
		input_report_key(kpd_input_dev, KEY_CLEAR, up_down);
		TX_DEBUG_PRINT(("\nClear received\n\n"));
		break;
	case MHL_RCP_CMD_SOUND_SELECT:
		input_report_key(kpd_input_dev, KEY_SOUND, up_down);
		TX_DEBUG_PRINT(("\nSound Select received\n\n"));
		break;
	case MHL_RCP_CMD_PLAY:
		input_report_key(kpd_input_dev, KEY_PLAY, up_down);
		TX_DEBUG_PRINT(("\nPlay received\n\n"));
		break;
	case MHL_RCP_CMD_PAUSE:
		/* input_report_key(kpd_input_dev, KEY_PAUSE, up_down); */
		input_report_key(kpd_input_dev, KEY_PAUSECD, up_down);

		TX_DEBUG_PRINT(("\nPause received\n\n"));
		break;
	case MHL_RCP_CMD_STOP:
		input_report_key(kpd_input_dev, KEY_STOP, up_down);
		TX_DEBUG_PRINT(("\nStop received\n\n"));
		break;
	case MHL_RCP_CMD_FAST_FWD:
		input_report_key(kpd_input_dev, KEY_FASTFORWARD, up_down);
		input_report_key(kpd_input_dev, KEY_FASTFORWARD, up_down);
		TX_DEBUG_PRINT(("\nFastfwd received\n\n"));
		break;
	case MHL_RCP_CMD_REWIND:
		input_report_key(kpd_input_dev, KEY_REWIND, up_down);
		TX_DEBUG_PRINT(("\nRewind received\n\n"));
		break;
	case MHL_RCP_CMD_EJECT:
		input_report_key(kpd_input_dev, KEY_EJECTCD, up_down);
		TX_DEBUG_PRINT(("\nEject received\n\n"));
		break;
	case MHL_RCP_CMD_FWD:
		/* input_report_key(kpd_input_dev, KEY_FORWARD, up_down);//next song */
		input_report_key(kpd_input_dev, KEY_NEXTSONG, up_down);
		TX_DEBUG_PRINT(("\nNext song received\n\n"));
		break;
	case MHL_RCP_CMD_BKWD:
		/* input_report_key(kpd_input_dev, KEY_BACK, up_down);//previous song */
		input_report_key(kpd_input_dev, KEY_PREVIOUSSONG, up_down);
		TX_DEBUG_PRINT(("\nPrevious song received\n\n"));
		break;
	case MHL_RCP_CMD_PLAY_FUNC:
		/* input_report_key(kpd_input_dev, KEY_PL, up_down); */
		input_report_key(kpd_input_dev, KEY_PLAY, up_down);
		TX_DEBUG_PRINT(("\nPlay Function received\n\n"));
		break;
	case MHL_RCP_CMD_PAUSE_PLAY_FUNC:
		input_report_key(kpd_input_dev, KEY_PLAYPAUSE, up_down);
		TX_DEBUG_PRINT(("\nPause_Play Function received\n\n"));
		break;
	case MHL_RCP_CMD_STOP_FUNC:
		input_report_key(kpd_input_dev, KEY_STOP, up_down);
		TX_DEBUG_PRINT(("\nStop Function received\n\n"));
		break;
	case MHL_RCP_CMD_F1:
		input_report_key(kpd_input_dev, KEY_F1, up_down);
		TX_DEBUG_PRINT(("\nF1 received\n\n"));
		break;
	case MHL_RCP_CMD_F2:
		input_report_key(kpd_input_dev, KEY_F2, up_down);
		TX_DEBUG_PRINT(("\nF2 received\n\n"));
		break;
	case MHL_RCP_CMD_F3:
		input_report_key(kpd_input_dev, KEY_F3, up_down);
		TX_DEBUG_PRINT(("\nF3 received\n\n"));
		break;
	case MHL_RCP_CMD_F4:
		input_report_key(kpd_input_dev, KEY_F4, up_down);
		TX_DEBUG_PRINT(("\nF4 received\n\n"));
		break;
	case MHL_RCP_CMD_F5:
		input_report_key(kpd_input_dev, KEY_F5, up_down);
		TX_DEBUG_PRINT(("\nF5 received\n\n"));
		break;
	default:
		break;
	}

	/* added  for improving mhl RCP start */
	input_sync(kpd_input_dev);
	/* added  for improving mhl RCP end */
}

void input_report_mhl_rcp_key(uint8_t rcp_keycode)
{
	/* added  for improving mhl RCP start */
	input_report_rcp_key(rcp_keycode & 0x7F, 1);

	if ((MHL_RCP_CMD_FAST_FWD == rcp_keycode) || (MHL_RCP_CMD_REWIND == rcp_keycode)) {
		msleep(100);
	}
	input_report_rcp_key(rcp_keycode & 0x7F, 0);
	/* added  for improving mhl RCP end */
}

/*************************************RCP function report added by garyyuan*********************************/

#define MAX_EVENT_STRING_LEN 40
void AppNotifyMhlDownStreamHPDStatusChange(bool_t connected)
{
	char event_string[MAX_EVENT_STRING_LEN];
	/* char    *envp[] = {event_string, NULL}; */
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
			"AppNotifyMhlDownStreamHPDStatusChange called, "
			"HPD status is: %s\n", connected ? "CONNECTED" : "NOT CONNECTED");
	snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=%s", connected ? "HPD" : "NO_HPD");
	printk("MHLEVENT=%s\n", connected ? "HPD" : "NO_HPD");
#if 0
	kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
}

MhlTxNotifyEventsStatus_e AppNotifyMhlEvent(uint8_t eventCode, uint8_t eventParam)
{
	char event_string[MAX_EVENT_STRING_LEN];
	/* char    *envp[] = {event_string, NULL}; */
	MhlTxNotifyEventsStatus_e retVal = MHL_TX_EVENT_STATUS_PASSTHROUGH;
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
			"AppNotifyEvent called, eventCode: 0x%02x eventParam: 0x%02x\n",
			eventCode, eventParam);

	/* return retVal; */
	switch (eventCode) {
	case MHL_TX_EVENT_CONNECTION:
		gDriverContext.flags |= MHL_STATE_FLAG_CONNECTED;
		strncpy(event_string, "MHLEVENT=connected", MAX_EVENT_STRING_LEN);
#if 0
		kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		break;
	case MHL_TX_EVENT_RCP_READY:
		gDriverContext.flags |= MHL_STATE_FLAG_RCP_READY;
		strncpy(event_string, "MHLEVENT=rcp_ready", MAX_EVENT_STRING_LEN);
#if 0
		kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		break;
	case MHL_TX_EVENT_DISCONNECTION:
		gDriverContext.flags = 0;
		gDriverContext.keyCode = 0;
		gDriverContext.errCode = 0;
		strncpy(event_string, "MHLEVENT=disconnected", MAX_EVENT_STRING_LEN);
#if 0
		kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		break;
	case MHL_TX_EVENT_RCP_RECEIVED:
		gDriverContext.flags &= ~MHL_STATE_FLAG_RCP_SENT;
		gDriverContext.flags |= MHL_STATE_FLAG_RCP_RECEIVED;
		gDriverContext.keyCode = eventParam;
		/* for RCP report function by garyyuan */
		if (eventParam > 0x7F)
			break;
		input_report_mhl_rcp_key(gDriverContext.keyCode);
		snprintf(event_string, MAX_EVENT_STRING_LEN,
			 "MHLEVENT=received_RCP key code=0x%02x", eventParam);
#if 0
		kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		break;
	case MHL_TX_EVENT_RCPK_RECEIVED:
		if ((gDriverContext.flags & MHL_STATE_FLAG_RCP_SENT)
		    && (gDriverContext.keyCode == eventParam)) {
			gDriverContext.flags |= MHL_STATE_FLAG_RCP_ACK;
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Generating RCPK received event, keycode: 0x%02x\n",
					eventParam);
			snprintf(event_string, MAX_EVENT_STRING_LEN,
				 "MHLEVENT=received_RCPK key code=0x%02x", eventParam);
#if 0
			kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		} else {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Ignoring unexpected RCPK received event, keycode: 0x%02x\n",
					eventParam);
		}
		break;
	case MHL_TX_EVENT_RCPE_RECEIVED:
		if (gDriverContext.flags & MHL_STATE_FLAG_RCP_SENT) {
			gDriverContext.errCode = eventParam;
			gDriverContext.flags |= MHL_STATE_FLAG_RCP_NAK;
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Generating RCPE received event, error code: 0x%02x\n",
					eventParam);
			snprintf(event_string, MAX_EVENT_STRING_LEN,
				 "MHLEVENT=received_RCPE error code=0x%02x", eventParam);
#if 0
			kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		} else {
			SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
					"Ignoring unexpected RCPE received event, error code: 0x%02x\n",
					eventParam);
		}
		break;
	case MHL_TX_EVENT_DCAP_CHG:
		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=DEVCAP change");
#if 0
		kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		break;
	case MHL_TX_EVENT_DSCR_CHG:
		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=SCRATCHPAD change");
#if 0
		kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		break;
	case MHL_TX_EVENT_POW_BIT_CHG:
		if (eventParam) {
			snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=MHL VBUS power OFF");
		} else {
			snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=MHL VBUS power ON");
		}
#if 0
		kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		break;
	case MHL_TX_EVENT_RGND_MHL:
		snprintf(event_string, MAX_EVENT_STRING_LEN, "MHLEVENT=MHL device detected");
#if 0
		kobject_uevent_env(&gDriverContext.pDevice->kobj, KOBJ_CHANGE, envp);
#endif
		break;
	default:
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"AppNotifyEvent called with unrecognized event code!\n");
	}
	return retVal;
}


static void hdmi_drv_set_util_funcs(const HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(HDMI_UTIL_FUNCS));
}

static void hdmi_drv_get_params(HDMI_PARAMS *params)
{
	HDMI_FUNC();
	memset(params, 0, sizeof(HDMI_PARAMS));
	params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
	params->init_config.aformat = HDMI_AUDIO_44K_2CH;

	params->clk_pol = HDMI_POLARITY_FALLING;
	params->de_pol = HDMI_POLARITY_RISING;
	params->vsync_pol = HDMI_POLARITY_RISING;
	params->hsync_pol = HDMI_POLARITY_RISING;

	params->hsync_front_porch = 110;
	params->hsync_pulse_width = 40;
	params->hsync_back_porch = 220;

	params->vsync_front_porch = 5;
	params->vsync_pulse_width = 5;
	params->vsync_back_porch = 20;

	params->rgb_order = HDMI_COLOR_ORDER_RGB;

	params->io_driving_current = IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num = 4;
	params->scaling_factor = 4;
	params->cabletype = MHL_Connect_type;
}

void hdmi_drv_suspend(void)
{
}

void hdmi_drv_resume(void)
{
}


static int hdmi_drv_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
				 HDMI_VIDEO_OUTPUT_FORMAT vout)
{

	if (vformat == HDMI_VIDEO_720x480p_60Hz) {
		HDMI_LOG("[hdmi_drv]480p\n");
		siHdmiTx_VideoSel(HDMI_480P60_4X3);
	} else if (vformat == HDMI_VIDEO_1280x720p_60Hz) {
		HDMI_LOG("[hdmi_drv]720p\n");
		siHdmiTx_VideoSel(HDMI_720P60);
	} else if (vformat == HDMI_VIDEO_1920x1080p_30Hz) {
		HDMI_LOG("[hdmi_drv]1080p\n");
		siHdmiTx_VideoSel(HDMI_1080P30);
	} else {
		HDMI_LOG("%s, video format not support now\n", __func__);
	}
	return 0;
}

static int hdmi_drv_audio_config(HDMI_AUDIO_FORMAT aformat)
{
	return 0;
}

static int hdmi_drv_video_enable(bool enable)
{
	return 0;
}

static int hdmi_drv_audio_enable(bool enable)
{
	return 0;
}

#define SiI8338DRIVER_INTERRUPT_MODE   1

static int hdmi_drv_init(void)
{
	halReturn_t halStatus;
	SiiOsStatus_t osalStatus;
	printk("Starting %s\n", MHL_PART_NAME);

    cust_hdmi_power_on(true);
    
	if (!SiiCraInitialize()) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Initialization of CRA layer failed!\n");
		return -EIO;
	}
	osalStatus = SiiOsInit(0);
	if (osalStatus != SII_OS_STATUS_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Initialization of OSAL failed, error code: %d\n", osalStatus);
		return -EIO;
	}
	halStatus = HalInit();
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Initialization of HAL failed, error code: %d\n", halStatus);
		SiiOsTerm();
		return -EIO;
	}
/* #if MTK_project */
	/* I2c_Init(); */
/* #endif */

	halStatus = HalOpenI2cDevice(MHL_PART_NAME, MHL_DRIVER_NAME);
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Opening of I2c device %s failed, error code: %d\n", MHL_PART_NAME,
				halStatus);
		HalTerm();
		SiiOsTerm();
		return -EIO;
	}

	/* msleep(200); */
#ifdef SiI8338DRIVER_INTERRUPT_MODE
	halStatus = HalInstallIrqHandler(SiiMhlTxDeviceIsr);
	if (halStatus != HAL_RET_SUCCESS) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Initialization of HAL interrupt support failed, error code: %d\n",
				halStatus);
		HalCloseI2cDevice();
		HalTerm();
		SiiOsTerm();
		return -EIO;
	}
#else
	StartEventThread();	/* begin monitoring for events if using polling mode */
#endif

#ifdef MDT_SUPPORT
	mhl_init_rmt_input_dev();
#endif

	return 0;
}

static int hdmi_drv_enter(void)
{
	HDMI_FUNC();
	return 0;
}

static int hdmi_drv_exit(void)
{
	HDMI_FUNC();
	return 0;
}

extern int SiiMhlTxInitialize(unsigned char pollIntervalMs);
extern void SiiMhlTxHwReset(uint16_t hwResetPeriod, uint16_t hwResetDelay);
extern unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK,
					  unsigned int SHIFT);
extern void SwitchToD3(void);
extern void SiiMhlTxHwResetKeepLow(void);
extern void ForceSwitchToD3(void);
extern void SiiMhlTxHwGpioSuspend(void);
extern void SiiMhlTxHwGpioResume(void);

int hdmi_drv_power_on(void)
{
	int ret = 0;
	HDMI_FUNC();
#ifdef	CUST_EINT_MHL_NUM
	mt_eint_mask(CUST_EINT_MHL_NUM);
#else
	printk("%s,%d Error: CUST_EINT_MHL_NUM is not defined\n", __func__, __LINE__);
#endif

    cust_hdmi_dpi_gpio_on(true);
    cust_hdmi_i2s_gpio_on(true);

	SiiMhlTxHwReset(5, 5);

	HalInstallCheckDeviceCB(SiiCheckDevice);
	HalAcquireIsrLock();
	siHdmiTx_VideoSel(HDMI_720P60);
	siHdmiTx_AudioSel(I2S_44);
	ret = SiiMhlTxInitialize(EVENT_POLL_INTERVAL_MS);
	HalReleaseIsrLock();
#ifdef	CUST_EINT_MHL_NUM
	mt_eint_unmask(CUST_EINT_MHL_NUM);
#endif
	return ret;
}

void hdmi_drv_power_off(void)
{
	HDMI_FUNC();

    cust_hdmi_dpi_gpio_on(false);
    cust_hdmi_i2s_gpio_on(false);


	SiiMhlTxHwReset(50, 200);
	ForceSwitchToD3();
	/* SwitchToD3(); */
	/* SiiMhlTxHwResetKeepLow(); */
	/* pmic_config_interface(0x87,0x0,0x01,0x0); */
}

extern unsigned char SiiTxReadConnectionStatus(void);
HDMI_STATE hdmi_drv_get_state(void)
{
	int ret = SiiTxReadConnectionStatus();
	HDMI_FUNC();
	HDMI_LOG("sii status:%d\n", ret);
	if (ret == 1)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;
}

void hdmi_drv_log_enable(bool enable)
{
	hdmi_log_on = enable;
}
 
const HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = hdmi_drv_set_util_funcs,
		.get_params = hdmi_drv_get_params,
		.init = hdmi_drv_init,
		.enter = hdmi_drv_enter,
		.exit = hdmi_drv_exit,
		.suspend = hdmi_drv_suspend,
		.resume = hdmi_drv_resume,
		.video_config = hdmi_drv_video_config,
		.audio_config = hdmi_drv_audio_config,
		.video_enable = hdmi_drv_video_enable,
		.audio_enable = hdmi_drv_audio_enable,
		.power_on = hdmi_drv_power_on,
		.power_off = hdmi_drv_power_off,
		.get_state = hdmi_drv_get_state,
		.log_enable = hdmi_drv_log_enable
	};

	HDMI_FUNC();
	return &HDMI_DRV;
}
#endif

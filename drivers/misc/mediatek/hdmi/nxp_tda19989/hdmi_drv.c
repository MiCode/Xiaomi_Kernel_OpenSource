#if defined(CONFIG_MTK_HDMI_SUPPORT)
#include <linux/string.h>

#include <mach/mt_gpio.h>
#include "mach/eint.h"
#include "mach/irqs.h"

#include "hdmi_drv.h"
#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

#include <mach/dma.h>
#include <mach/irqs.h>

#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <asm/tlbflush.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#include <generated/autoconf.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

#include <mach/dma.h>
#include <mach/irqs.h>
#include <linux/vmalloc.h>

#include <asm/uaccess.h>

#ifndef TMFL_TDA19989
#define TMFL_TDA19989
#endif

#ifndef TMFL_NO_RTOS
#define TMFL_NO_RTOS
#endif

#ifndef TMFL_LINUX_OS_KERNEL_DRIVER
#define TMFL_LINUX_OS_KERNEL_DRIVER
#endif


/* HDMI DevLib */
#include "tmNxCompId.h"
#include "tmdlHdmiTx_Types.h"
#include "tmdlHdmiTx_Functions.h"

/* local */
#include "tda998x_version.h"
#include "tda998x.h"
#include "tda998x_ioctl.h"
#include "I2C.h"

#include "hdmi_drv.h"

/* GPIO_HDMI_POWER_CONTROL */
/* for EVB, power is always on, so no need for power control */
#define USE_GPIO_HDMI_POWER_CONTROL 0

#define TDA_TRY(fct) { \
      err = (fct);                                                        \
      if (err) {                                                        \
         pr_debug("%s returned in %s line %d\n", hdmi_tx_err_string(err), __func__, __LINE__); \
	 goto TRY_DONE;                                                 \
      }                                                                 \
   }

static size_t hdmi_log_on = true;
/* static struct switch_dev hdmi_switch_data; */
#define HDMI_LOG(fmt, arg...) \
	do { \
		if (hdmi_log_on) pr_debug("[hdmi_drv]%s,%d ", __func__, __LINE__); pr_debug(fmt, ##arg); \
	} while (0)

#define HDMI_FUNC()	\
	do { \
		if (hdmi_log_on) pr_debug("[hdmi_drv] %s\n", __func__); \
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
static struct task_struct *hdmi_hpd_detect_task;

wait_queue_head_t hdmi_event_wq;
atomic_t hdmi_event = ATOMIC_INIT(0);
/* static int hdmi_event_status = HDMI_STATE_NO_DEVICE; */

tda_instance our_instance, *g_inst;
static HDMI_UTIL_FUNCS hdmi_util = { 0 };

#define SET_RESET_PIN(v)    (hdmi_util.set_reset_pin((v)))

#define UDELAY(n) (hdmi_util.udelay(n))
#define MDELAY(n) (hdmi_util.mdelay(n))


#define CEC_SLAVE_ADDR				0x68
#define HDMI_SLAVE_ADDR				0xE0

#define FRAME_PACKING 200
#define NO_FP(x) ((x) % FRAME_PACKING)
#define IS_FP(x) ((x) > FRAME_PACKING)
#define WITH_FP(x) ((x) + FRAME_PACKING * (this->tda.setio.video_in.structure3D == TMDL_HDMITX_3D_FRAME_PACKING))

/*
 * error handling
 */
char *hdmi_tx_err_string(int err)
{
	switch (err & 0x0FFF) {
	case TM_ERR_COMPATIBILITY:{
			return "SW Interface compatibility";
			break;
		}
	case TM_ERR_MAJOR_VERSION:{
			return "SW Major Version error";
			break;
		}
	case TM_ERR_COMP_VERSION:{
			return "SW component version error";
			break;
		}
	case TM_ERR_BAD_UNIT_NUMBER:{
			return "Invalid device unit number";
			break;
		}
	case TM_ERR_BAD_INSTANCE:{
			return "Bad input instance value  ";
			break;
		}
	case TM_ERR_BAD_HANDLE:{
			return "Bad input handle";
			break;
		}
	case TM_ERR_BAD_PARAMETER:{
			return "Invalid input parameter";
			break;
		}
	case TM_ERR_NO_RESOURCES:{
			return "Resource is not available ";
			break;
		}
	case TM_ERR_RESOURCE_OWNED:{
			return "Resource is already in use";
			break;
		}
	case TM_ERR_RESOURCE_NOT_OWNED:{
			return "Caller does not own resource";
			break;
		}
	case TM_ERR_INCONSISTENT_PARAMS:{
			return "Inconsistent input params";
			break;
		}
	case TM_ERR_NOT_INITIALIZED:{
			return "Component is not initialised";
			break;
		}
	case TM_ERR_NOT_SUPPORTED:{
			return "Function is not supported";
			break;
		}
	case TM_ERR_INIT_FAILED:{
			return "Initialization failed";
			break;
		}
	case TM_ERR_BUSY:{
			return "Component is busy";
			break;
		}
	case TMDL_ERR_DLHDMITX_I2C_READ:{
			return "Read error";
			break;
		}
	case TMDL_ERR_DLHDMITX_I2C_WRITE:{
			return "Write error";
			break;
		}
	case TM_ERR_FULL:{
			return "Queue is full";
			break;
		}
	case TM_ERR_NOT_STARTED:{
			return "Function is not started";
			break;
		}
	case TM_ERR_ALREADY_STARTED:{
			return "Function is already starte";
			break;
		}
	case TM_ERR_ASSERTION:{
			return "Assertion failure";
			break;
		}
	case TM_ERR_INVALID_STATE:{
			return "Invalid state for function";
			break;
		}
	case TM_ERR_OPERATION_NOT_PERMITTED:{
			return "Corresponds to posix EPERM";
			break;
		}
	case TMDL_ERR_DLHDMITX_RESOLUTION_UNKNOWN:{
			return "Bad format";
			break;
		}
	case TM_OK:{
			return "OK";
			break;
		}
	default:{
			pr_debug("(err:%x) ", err);
			return "unknown";
			break;
		}
	}
}

static char *tda_spy_event(int event)
{
	switch (event) {
	case TMDL_HDMITX_HDCP_ACTIVE:{
			return "HDCP active";
			break;
		}
	case TMDL_HDMITX_HDCP_INACTIVE:{
			return "HDCP inactive";
			break;
		}
	case TMDL_HDMITX_HPD_ACTIVE:{
			return "HPD active";
			break;
		}
	case TMDL_HDMITX_HPD_INACTIVE:{
			return "HPD inactive";
			break;
		}
	case TMDL_HDMITX_RX_KEYS_RECEIVED:{
			return "Rx keys received";
			break;
		}
	case TMDL_HDMITX_RX_DEVICE_ACTIVE:{
			return "Rx device active";
			break;
		}
	case TMDL_HDMITX_RX_DEVICE_INACTIVE:{
			return "Rx device inactive";
			break;
		}
	case TMDL_HDMITX_EDID_RECEIVED:{
			return "EDID received";
			break;
		}
	case TMDL_HDMITX_VS_RPT_RECEIVED:{
			return "VS interrupt has been received";
			break;
		}
		/*       case TMDL_HDMITX_B_STATUS: {return "TX received BStatus";break;} */
#if defined(TMFL_TDA19989) || defined(TMFL_TDA9984)
	case TMDL_HDMITX_DEBUG_EVENT_1:{
			return "DEBUG_EVENT_1";
			break;
		}
#endif
	default:{
			return "Unkonwn event";
			break;
		}
	}
}


#if defined(TMFL_TDA19989) || defined(TMFL_TDA9984)
static char *tda_spy_hsdc_fail_status(int fail)
{
	switch (fail) {
	case TMDL_HDMITX_HDCP_OK:{
			return "ok";
			break;
		}
	case TMDL_HDMITX_HDCP_BKSV_RCV_FAIL:{
			return "Source does not receive Sink BKsv ";
			break;
		}
	case TMDL_HDMITX_HDCP_BKSV_CHECK_FAIL:{
			return "BKsv does not contain 20 zeros and 20 ones";
			break;
		}
	case TMDL_HDMITX_HDCP_BCAPS_RCV_FAIL:{
			return "Source does not receive Sink Bcaps";
			break;
		}
	case TMDL_HDMITX_HDCP_AKSV_SEND_FAIL:{
			return "Source does not send AKsv";
			break;
		}
	case TMDL_HDMITX_HDCP_R0_RCV_FAIL:{
			return "Source does not receive R'0";
			break;
		}
	case TMDL_HDMITX_HDCP_R0_CHECK_FAIL:{
			return "R0 = R'0 check fail";
			break;
		}
	case TMDL_HDMITX_HDCP_BKSV_NOT_SECURE:{
			return "bksv not secure";
			break;
		}
	case TMDL_HDMITX_HDCP_RI_RCV_FAIL:{
			return "Source does not receive R'i";
			break;
		}
	case TMDL_HDMITX_HDCP_RPT_RI_RCV_FAIL:{
			return "Source does not receive R'i repeater mode";
			break;
		}
	case TMDL_HDMITX_HDCP_RI_CHECK_FAIL:{
			return "RI = R'I check fail";
			break;
		}
	case TMDL_HDMITX_HDCP_RPT_RI_CHECK_FAIL:{
			return "RI = R'I check fail repeater mode";
			break;
		}
	case TMDL_HDMITX_HDCP_RPT_BCAPS_RCV_FAIL:{
			return "Source does not receive Sink Bcaps repeater mode";
			break;
		}
	case TMDL_HDMITX_HDCP_RPT_BCAPS_READY_TIMEOUT:{
			return "bcaps ready timeout";
			break;
		}
	case TMDL_HDMITX_HDCP_RPT_V_RCV_FAIL:{
			return "Source does not receive V";
			break;
		}
	case TMDL_HDMITX_HDCP_RPT_BSTATUS_RCV_FAIL:{
			return "Source does not receive BSTATUS repeater mode";
			break;
		}
	case TMDL_HDMITX_HDCP_RPT_KSVLIST_RCV_FAIL:{
			return "Source does not receive Ksv list in repeater mode";
			break;
		}
	case TMDL_HDMITX_HDCP_RPT_KSVLIST_NOT_SECURE:{
			return "ksvlist not secure";
			break;
		}
	default:{
			return "";
			break;
		}
	}
}

#if 0
static char *tda_spy_hdcp_status(int status)
{
	switch (status) {
	case TMDL_HDMITX_HDCP_CHECK_NOT_STARTED:{
			return "Check not started";
			break;
		}
	case TMDL_HDMITX_HDCP_CHECK_IN_PROGRESS:{
			return "No failures, more to do";
			break;
		}
	case TMDL_HDMITX_HDCP_CHECK_PASS:{
			return "Final check has passed";
			break;
		}
	case TMDL_HDMITX_HDCP_CHECK_FAIL_FIRST:{
			return "First check failure code\nDriver not AUTHENTICATED";
			break;
		}
	case TMDL_HDMITX_HDCP_CHECK_FAIL_DEVICE_T0:{
			return "A T0 interrupt occurred";
			break;
		}
	case TMDL_HDMITX_HDCP_CHECK_FAIL_DEVICE_RI:{
			return "Device RI changed";
			break;
		}
	case TMDL_HDMITX_HDCP_CHECK_FAIL_DEVICE_FSM:{
			return "Device FSM not 10h";
			break;
		}
	default:{
			return "Unknown hdcp status";
			break;
		}
	}

}
#endif

#endif

static char *tda_spy_sink(int sink)
{
	switch (sink) {
	case TMDL_HDMITX_SINK_DVI:{
			return "DVI";
			break;
		}
	case TMDL_HDMITX_SINK_HDMI:{
			return "HDMI";
			break;
		}
	case TMDL_HDMITX_SINK_EDID:{
			return "As currently defined in EDID";
			break;
		}
	default:{
			return "Unkonwn sink";
			break;
		}
	}
}

#if defined(TMFL_TDA19989) || defined(TMFL_TDA9984)
static char *tda_spy_aspect_ratio(int ar)
{
	switch (ar) {
	case TMDL_HDMITX_P_ASPECT_RATIO_UNDEFINED:{
			return "Undefined picture aspect rati";
			break;
		}
	case TMDL_HDMITX_P_ASPECT_RATIO_6_5:{
			return "6:5 picture aspect ratio (PAR";
			break;
		}
	case TMDL_HDMITX_P_ASPECT_RATIO_5_4:{
			return "5:4 PA";
			break;
		}
	case TMDL_HDMITX_P_ASPECT_RATIO_4_3:{
			return "4:3 PA";
			break;
		}
	case TMDL_HDMITX_P_ASPECT_RATIO_16_10:{
			return "16:10 PA";
			break;
		}
	case TMDL_HDMITX_P_ASPECT_RATIO_5_3:{
			return "5:3 PA";
			break;
		}
	case TMDL_HDMITX_P_ASPECT_RATIO_16_9:{
			return "16:9 PA";
			break;
		}
	case TMDL_HDMITX_P_ASPECT_RATIO_9_5:{
			return "9:5 PA";
			break;
		}
	default:{
			return "Unknown aspect ratio";
			break;
		}
	}
}

#if 0				/* no more used */
static char *tda_spy_edid_status(int status)
{
	switch (status) {
	case TMDL_HDMITX_EDID_READ:{
			return "All blocks read";
			break;
		}
	case TMDL_HDMITX_EDID_READ_INCOMPLETE:{
			return "All blocks read OK but buffer too small to return all of the";
			break;
		}
	case TMDL_HDMITX_EDID_ERROR_CHK_BLOCK_0:{
			return "Block 0 checksum erro";
			break;
		}
	case TMDL_HDMITX_EDID_ERROR_CHK:{
			return "Block 0 OK, checksum error in one or more other block";
			break;
		}
	case TMDL_HDMITX_EDID_NOT_READ:{
			return "EDID not read";
			break;
		}
	case TMDL_HDMITX_EDID_STATUS_INVALID:{
			return "Invalid ";
			break;
		}
	default:{
			return "Unknown edid status";
			break;
		}
	}
}
#endif

static char *tda_spy_vfmt(int fmt)
{
	switch (fmt) {
	case TMDL_HDMITX_VFMT_NULL:{
			return "NOT a valid format...";
			break;
		}
	case TMDL_HDMITX_VFMT_01_640x480p_60Hz:{
			return "vic 01: 640x480p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_02_720x480p_60Hz:{
			return "vic 02: 720x480p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_03_720x480p_60Hz:{
			return "vic 03: 720x480p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_04_1280x720p_60Hz:{
			return "vic 04: 1280x720p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_05_1920x1080i_60Hz:{
			return "vic 05: 1920x1080i 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_06_720x480i_60Hz:{
			return "vic 06: 720x480i 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_07_720x480i_60Hz:{
			return "vic 07: 720x480i 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_08_720x240p_60Hz:{
			return "vic 08: 720x240p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_09_720x240p_60Hz:{
			return "vic 09: 720x240p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_10_720x480i_60Hz:{
			return "vic 10: 720x480i 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_11_720x480i_60Hz:{
			return "vic 11: 720x480i 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_12_720x240p_60Hz:{
			return "vic 12: 720x240p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_13_720x240p_60Hz:{
			return "vic 13: 720x240p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_14_1440x480p_60Hz:{
			return "vic 14: 1440x480p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_15_1440x480p_60Hz:{
			return "vic 15: 1440x480p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_16_1920x1080p_60Hz:{
			return "vic 16: 1920x1080p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_17_720x576p_50Hz:{
			return "vic 17: 720x576p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_18_720x576p_50Hz:{
			return "vic 18: 720x576p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_19_1280x720p_50Hz:{
			return "vic 19: 1280x720p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_20_1920x1080i_50Hz:{
			return "vic 20: 1920x1080i 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_21_720x576i_50Hz:{
			return "vic 21: 720x576i 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_22_720x576i_50Hz:{
			return "vic 22: 720x576i 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_23_720x288p_50Hz:{
			return "vic 23: 720x288p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_24_720x288p_50Hz:{
			return "vic 24: 720x288p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_25_720x576i_50Hz:{
			return "vic 25: 720x576i 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_26_720x576i_50Hz:{
			return "vic 26: 720x576i 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_27_720x288p_50Hz:{
			return "vic 27: 720x288p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_28_720x288p_50Hz:{
			return "vic 28: 720x288p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_29_1440x576p_50Hz:{
			return "vic 29: 1440x576p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_30_1440x576p_50Hz:{
			return "vic 30: 1440x576p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_31_1920x1080p_50Hz:{
			return "vic 31: 1920x1080p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_32_1920x1080p_24Hz:{
			return "vic 32: 1920x1080p 24Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_33_1920x1080p_25Hz:{
			return "vic 33: 1920x1080p 25Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_34_1920x1080p_30Hz:{
			return "vic 34: 1920x1080p 30Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_35_2880x480p_60Hz:{
			return "vic 3: 2880x480p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_36_2880x480p_60Hz:{
			return "vic 3: 2880x480p 60Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_37_2880x576p_50Hz:{
			return "vic 3: 2880x576p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_38_2880x576p_50Hz:{
			return "vic 3: 2880x576p 50Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_60_1280x720p_24Hz:{
			return "vic 60: 1280x720p 24Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_61_1280x720p_25Hz:{
			return "vic 61: 1280x720p 25Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_62_1280x720p_30Hz:{
			return "vic 62: 1280x720p 30Hz";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_800x600p_60Hz:{
			return "PC 129";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1152x960p_60Hz:{
			return "PC 130";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1024x768p_60Hz:{
			return "PC 131";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1280x768p_60Hz:{
			return "PC 132";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1280x1024p_60Hz:{
			return "PC 133";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1360x768p_60Hz:{
			return "PC 134";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1400x1050p_60Hz:{
			return "PC 135";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1600x1200p_60Hz:{
			return "PC 136";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1024x768p_70Hz:{
			return "PC 137";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_640x480p_72Hz:{
			return "PC 138";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_800x600p_72Hz:{
			return "PC 139";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_640x480p_75Hz:{
			return "PC 140";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1024x768p_75Hz:{
			return "PC 141";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_800x600p_75Hz:{
			return "PC 142";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1024x864p_75Hz:{
			return "PC 143";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1280x1024p_75Hz:{
			return "PC 144";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_640x350p_85Hz:{
			return "PC 145";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_640x400p_85Hz:{
			return "PC 146";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_720x400p_85Hz:{
			return "PC 147";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_640x480p_85Hz:{
			return "PC 148";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_800x600p_85Hz:{
			return "PC 149";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1024x768p_85Hz:{
			return "PC 150";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1152x864p_85Hz:{
			return "PC 151";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1280x960p_85Hz:{
			return "PC 152";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1280x1024p_85Hz:{
			return "PC 153";
			break;
		}
	case TMDL_HDMITX_VFMT_PC_1024x768i_87Hz:{
			return "PC 154";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_02_720x480p_60Hz:{
			return "vic 02: 720x480p 60Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_17_720x576p_50Hz:{
			return "vic 17: 720x576p 50Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_60_1280x720p_24Hz:{
			return "vic 60: 1280x720p 24Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_61_1280x720p_25Hz:{
			return "vic 61: 1280x720p 25Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_62_1280x720p_30Hz:{
			return "vic 62: 1280x720p 30Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_19_1280x720p_50Hz:{
			return "vic 19: 1280x720p 50Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_04_1280x720p_60Hz:{
			return "vic 04: 1280x720p 60Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_32_1920x1080p_24Hz:{
			return "vic 32: 1920x1080p 24Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_33_1920x1080p_25Hz:{
			return "vic 33: 1920x1080p 25Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_34_1920x1080p_30Hz:{
			return "vic 34: 1920x1080p 30Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_31_1920x1080p_50Hz:{
			return "vic 31: 1920x1080p 50Hz frame packing";
			break;
		}
	case FRAME_PACKING + TMDL_HDMITX_VFMT_16_1920x1080p_60Hz:{
			return "vic 16: 1920x1080p 60Hz frame packing";
			break;
		}
	default:{
			return "unknown video format";
			break;
		}
	}
}
#endif

#if 0
static char *tda_spy_audio_fmt(int fmt)
{
	switch (fmt) {
	case TMDL_HDMITX_AFMT_SPDIF:{
			return "SPDIF";
			break;
		}
	case TMDL_HDMITX_AFMT_I2S:{
			return "I2S";
			break;
		}
	case TMDL_HDMITX_AFMT_OBA:{
			return "OBA";
			break;
		}
	case TMDL_HDMITX_AFMT_DST:{
			return "DST";
			break;
		}
	case TMDL_HDMITX_AFMT_HBR:{
			return "HBR";
			break;
		}
	default:{
			return "Unknown audio format";
			break;
		}
	}
}

static char *tda_spy_audio_freq(int freq)
{
	switch (freq) {
	case TMDL_HDMITX_AFS_32K:{
			return "32k";
			break;
		}
	case TMDL_HDMITX_AFS_44K:{
			return "44k";
			break;
		}
	case TMDL_HDMITX_AFS_48K:{
			return "48k";
			break;
		}
	case TMDL_HDMITX_AFS_88K:{
			return "88k";
			break;
		}
	case TMDL_HDMITX_AFS_96K:{
			return "96k";
			break;
		}
	case TMDL_HDMITX_AFS_176K:{
			return "176k";
			break;
		}
	case TMDL_HDMITX_AFS_192K:{
			return "192k";
			break;
		}
	default:{
			return "Unknown audio freq";
			break;
		}
	}
}

static char *tda_spy_audio_i2c(int bits)
{
	switch (bits) {
	case TMDL_HDMITX_I2SQ_16BITS:{
			return "16 bits";
			break;
		}
	case TMDL_HDMITX_I2SQ_32BITS:{
			return "32 bits";
			break;
		}
	default:{
			return "Unknown audio i2c sampling";
			break;
		}
	}
}

static char *tda_spy_audio_i2c4(int align)
{
	switch (align) {
	case TMDL_HDMITX_I2SFOR_PHILIPS_L:{
			return "Philips Left";
			break;
		}
	case TMDL_HDMITX_I2SFOR_OTH_L:{
			return "other left";
			break;
		}
	case TMDL_HDMITX_I2SFOR_OTH_R:{
			return "other right";
			break;
		}
	default:{
			return "Unknown audio I2C alignement";
			break;
		}
	}
}

static void tda_spy_audio(tmdlHdmiTxAudioInConfig_t *audio)
{
	pr_debug
	    "hdmitx audio input\n format:%d(%s) rate:%d(%s) i2c_format:%d(%s) i2c_qualif:%d(%s) dst_rate:%d channel:%d\n",
	    audio->format, tda_spy_audio_fmt(audio->format), audio->rate,
	    tda_spy_audio_freq(audio->rate), audio->i2sFormat, tda_spy_audio_i2c4(audio->i2sFormat),
	    audio->i2sQualifier, tda_spy_audio_i2c(audio->i2sQualifier), audio->dstRate,
	    audio->channelAllocation);
}
#endif
/*
 *
 */
static int tda_spy(int verbose) {
	tda_instance *this = &our_instance;
	int i, err = 0;

	if (!verbose) {
		return err;
	}

	pr_debug "\n<edid video caps>\n");
	this->tda.edid_video_caps.max = EXAMPLE_MAX_SVD;
	TDA_TRY(tmdlHdmiTxGetEdidVideoCaps(this->tda.instance,
					   this->tda.edid_video_caps.desc,
					   this->tda.edid_video_caps.max,
					   &this->tda.edid_video_caps.written,
					   &this->tda.edid_video_caps.flags));
	pr_debug("written:%d\n", this->tda.edid_video_caps.written);
	pr_debug("flags:0X%x\n", this->tda.edid_video_caps.flags);
	if (this->tda.edid_video_caps.written > this->tda.edid_video_caps.max) {
		pr_err("get %d video caps but was waiting for %d\n",
		       this->tda.edid_video_caps.written, this->tda.edid_video_caps.max);
		this->tda.edid_video_caps.written = this->tda.edid_video_caps.max;
	}
	for (i = 0; i < this->tda.edid_video_caps.written; i++) {
		pr_debug("videoFormat: %s\n",
			 (char *)tda_spy_vfmt((this->tda.edid_video_caps.desc[i].videoFormat)));
		pr_debug("nativeVideoFormat:%s\n",
			 (this->tda.edid_video_caps.desc[i].nativeVideoFormat ? "yes" : "no"));
	}

	pr_debug("\n<edid video timings>\n");
	TDA_TRY(tmdlHdmiTxGetEdidVideoPreferred(this->tda.instance, &this->tda.edid_video_timings));
	pr_debug("Pixel Clock/10 000:%d\n", this->tda.edid_video_timings.pixelClock);
	pr_debug("Horizontal Active Pixels:%d\n", this->tda.edid_video_timings.hActivePixels);
	pr_debug("Horizontal Blanking Pixels:%d\n", this->tda.edid_video_timings.hBlankPixels);
	pr_debug("Vertical Active Lines:%d\n", this->tda.edid_video_timings.vActiveLines);
	pr_debug("Vertical Blanking Lines:%d\n", this->tda.edid_video_timings.vBlankLines);
	pr_debug("Horizontal Sync Offset:%d\n", this->tda.edid_video_timings.hSyncOffset);
	pr_debug("Horiz. Sync Pulse Width:%d\n", this->tda.edid_video_timings.hSyncWidth);
	pr_debug("Vertical Sync Offset:%d\n", this->tda.edid_video_timings.vSyncOffset);
	pr_debug("Vertical Sync Pulse Width:%d\n", this->tda.edid_video_timings.vSyncWidth);
	pr_debug("Horizontal Image Size:%d\n", this->tda.edid_video_timings.hImageSize);
	pr_debug("Vertical Image Size:%d\n", this->tda.edid_video_timings.vImageSize);
	pr_debug("Horizontal Border:%d\n", this->tda.edid_video_timings.hBorderPixels);
	pr_debug("Vertical Border:%d\n", this->tda.edid_video_timings.vBorderPixels);
	pr_debug("Interlace/sync info:%x\n", this->tda.edid_video_timings.flags);

	pr_debug("\n<sink type>\n");
	TDA_TRY(tmdlHdmiTxGetEdidSinkType(this->tda.instance, &this->tda.setio.sink));
	pr_debug("%s\n", tda_spy_sink(this->tda.setio.sink));
	pr_debug("\n<source address>\n");
	TDA_TRY(tmdlHdmiTxGetEdidSourceAddress(this->tda.instance, &this->tda.src_address));
	pr_debug("%x\n", this->tda.src_address);
	pr_debug("\n<detailled timing descriptors>\n");
	this->tda.edid_dtd.max = EXAMPLE_MAX_SVD;
	TDA_TRY(tmdlHdmiTxGetEdidDetailledTimingDescriptors(this->tda.instance,
							    this->tda.edid_dtd.desc,
							    this->tda.edid_dtd.max,
							    &this->tda.edid_dtd.written));
	pr_debug("Interlace/sync info:%x\n", this->tda.edid_dtd.desc[i].flags);
	pr_debug("written:%d\n", this->tda.edid_dtd.written);
	if (this->tda.edid_dtd.written > this->tda.edid_dtd.max) {
		pr_err("get %d video caps but was waiting for %d\n",
		       this->tda.edid_dtd.written, this->tda.edid_dtd.max);
		this->tda.edid_dtd.written = this->tda.edid_dtd.max;
	}
	for (i = 0; i < this->tda.edid_dtd.written; i++) {
		pr_debug("Pixel Clock/10 000:%d\n", this->tda.edid_dtd.desc[i].pixelClock);
		pr_debug("Horizontal Active Pixels:%d\n", this->tda.edid_dtd.desc[i].hActivePixels);
		pr_debug("Horizontal Blanking Pixels:%d\n",
			 this->tda.edid_dtd.desc[i].hBlankPixels);
		pr_debug("Vertical Active Lines:%d\n", this->tda.edid_dtd.desc[i].vActiveLines);
		pr_debug("Vertical Blanking Lines:%d\n", this->tda.edid_dtd.desc[i].vBlankLines);
		pr_debug("Horizontal Sync Offset:%d\n", this->tda.edid_dtd.desc[i].hSyncOffset);
		pr_debug("Horiz. Sync Pulse Width:%d\n", this->tda.edid_dtd.desc[i].hSyncWidth);
		pr_debug("Vertical Sync Offset:%d\n", this->tda.edid_dtd.desc[i].vSyncOffset);
		pr_debug("Vertical Sync Pulse Width:%d\n", this->tda.edid_dtd.desc[i].vSyncWidth);
		pr_debug("Horizontal Image Size:%d\n", this->tda.edid_dtd.desc[i].hImageSize);
		pr_debug("Vertical Image Size:%d\n", this->tda.edid_dtd.desc[i].vImageSize);
		pr_debug("Horizontal Border:%d\n", this->tda.edid_dtd.desc[i].hBorderPixels);
		pr_debug("Vertical Border:%d\n", this->tda.edid_dtd.desc[i].vBorderPixels);
	}

	pr_debug("\n<monitor descriptors>\n");
	this->tda.edid_md.max = EXAMPLE_MAX_SVD;
	TDA_TRY(tmdlHdmiTxGetEdidMonitorDescriptors(this->tda.instance,
						    this->tda.edid_md.desc1,
						    this->tda.edid_md.desc2,
						    this->tda.edid_md.other,
						    this->tda.edid_md.max,
						    &this->tda.edid_md.written));
	pr_debug("written:%d\n", this->tda.edid_md.written);
	if (this->tda.edid_md.written > this->tda.edid_md.max) {
		pr_err("get %d video caps but was waiting for %d\n",
		       this->tda.edid_md.written, this->tda.edid_md.max);
		this->tda.edid_md.written = this->tda.edid_md.max;
	}
	for (i = 0; i < this->tda.edid_md.written; i++) {
		if (this->tda.edid_md.desc1[i].descRecord) {
			this->tda.edid_md.desc1[i].monitorName[EDID_MONITOR_DESCRIPTOR_SIZE - 1] =
			    0;
			pr_debug("Monitor name:%s\n", this->tda.edid_md.desc1[i].monitorName);
		}
		if (this->tda.edid_md.desc1[i].descRecord) {
			pr_debug("Min vertical rate in Hz:%d\n",
				 this->tda.edid_md.desc2[i].minVerticalRate);
			pr_debug("Max vertical rate in Hz:%d\n",
				 this->tda.edid_md.desc2[i].maxVerticalRate);
			pr_debug("Min horizontal rate in Hz:%d\n",
				 this->tda.edid_md.desc2[i].minHorizontalRate);
			pr_debug("Max horizontal rate in Hz:%d\n",
				 this->tda.edid_md.desc2[i].maxHorizontalRate);
			pr_debug("Max supported pixel clock rate in MHz:%d\n",
				 this->tda.edid_md.desc2[i].maxSupportedPixelClk);
		}
	}

	pr_debug("\n<TV picture ratio>\n");
	TDA_TRY(tmdlHdmiTxGetEdidTVPictureRatio(this->tda.instance,
						&this->tda.edid_tv_aspect_ratio));
	pr_debug("%s\n", tda_spy_aspect_ratio(this->tda.edid_tv_aspect_ratio));

	pr_debug("\n<latency info>\n");
	TDA_TRY(tmdlHdmiTxGetEdidLatencyInfo(this->tda.instance, &this->tda.edid_latency));
	if (this->tda.edid_latency.latency_available) {
		pr_debug("Edid video:%d\n", this->tda.edid_latency.Edidvideo_latency);
		pr_debug("Edid audio:%d\n", this->tda.edid_latency.Edidaudio_latency);
	}
	if (this->tda.edid_latency.Ilatency_available) {
		pr_debug("Edid Ivideo:%d\n", this->tda.edid_latency.EdidIvideo_latency);
		pr_debug("Edid Iaudio:%d\n", this->tda.edid_latency.EdidIaudio_latency);
	}
 TRY_DONE:
	return err;
}

#if 0
static char *tda_ioctl(int io) {
	switch (io) {
	case TDA_VERBOSE_ON_CMD:{
			return "TDA_VERBOSE_ON_CMD";
			break;
		}
	case TDA_VERBOSE_OFF_CMD:{
			return "TDA_VERBOSE_OFF_CMD";
			break;
		}
	case TDA_BYEBYE_CMD:{
			return "TDA_BYEBYE_CMD";
			break;
		}
	case TDA_GET_SW_VERSION_CMD:{
			return "TDA_GET_SW_VERSION_CMD";
			break;
		}
	case TDA_SET_POWER_CMD:{
			return "TDA_SET_POWER_CMD";
			break;
		}
	case TDA_GET_POWER_CMD:{
			return "TDA_GET_POWER_CMD";
			break;
		}
	case TDA_SETUP_CMD:{
			return "TDA_SETUP_CMD";
			break;
		}
	case TDA_GET_SETUP_CMD:{
			return "TDA_GET_SETUP_CMD";
			break;
		}
	case TDA_WAIT_EVENT_CMD:{
			return "TDA_WAIT_EVENT_CMD";
			break;
		}
	case TDA_ENABLE_EVENT_CMD:{
			return "TDA_ENABLE_EVENT_CMD";
			break;
		}
	case TDA_DISABLE_EVENT_CMD:{
			return "TDA_DISABLE_EVENT_CMD";
			break;
		}
	case TDA_GET_VIDEO_SPEC_CMD:{
			return "TDA_GET_VIDEO_SPEC_CMD";
			break;
		}
	case TDA_SET_INPUT_OUTPUT_CMD:{
			return "TDA_SET_INPUT_OUTPUT_CMD";
			break;
		}
	case TDA_SET_AUDIO_INPUT_CMD:{
			return "TDA_SET_AUDIO_INPUT_CMD";
			break;
		}
	case TDA_SET_VIDEO_INFOFRAME_CMD:{
			return "TDA_SET_VIDEO_INFOFRAME_CMD";
			break;
		}
	case TDA_SET_AUDIO_INFOFRAME_CMD:{
			return "TDA_SET_AUDIO_INFOFRAME_CMD";
			break;
		}
	case TDA_SET_ACP_CMD:{
			return "TDA_SET_ACP_CMD";
			break;
		}
	case TDA_SET_GCP_CMD:{
			return "TDA_SET_GCP_CMD";
			break;
		}
	case TDA_SET_ISRC1_CMD:{
			return "TDA_SET_ISRC1_CMD";
			break;
		}
	case TDA_SET_ISRC2_CMD:{
			return "TDA_SET_ISRC2_CMD";
			break;
		}
	case TDA_SET_MPS_INFOFRAME_CMD:{
			return "TDA_SET_MPS_INFOFRAME_CMD";
			break;
		}
	case TDA_SET_SPD_INFOFRAME_CMD:{
			return "TDA_SET_SPD_INFOFRAME_CMD";
			break;
		}
	case TDA_SET_VS_INFOFRAME_CMD:{
			return "TDA_SET_VS_INFOFRAME_CMD";
			break;
		}
	case TDA_SET_AUDIO_MUTE_CMD:{
			return "TDA_SET_AUDIO_MUTE_CMD";
			break;
		}
	case TDA_RESET_AUDIO_CTS_CMD:{
			return "TDA_RESET_AUDIO_CTS_CMD";
			break;
		}
	case TDA_GET_EDID_STATUS_CMD:{
			return "TDA_GET_EDID_STATUS_CMD";
			break;
		}
	case TDA_GET_EDID_AUDIO_CAPS_CMD:{
			return "TDA_GET_EDID_AUDIO_CAPS_CMD";
			break;
		}
	case TDA_GET_EDID_VIDEO_CAPS_CMD:{
			return "TDA_GET_EDID_VIDEO_CAPS_CMD";
			break;
		}
	case TDA_GET_EDID_VIDEO_PREF_CMD:{
			return "TDA_GET_EDID_VIDEO_PREF_CMD";
			break;
		}
	case TDA_GET_EDID_SINK_TYPE_CMD:{
			return "TDA_GET_EDID_SINK_TYPE_CMD";
			break;
		}
	case TDA_GET_EDID_SOURCE_ADDRESS_CMD:{
			return "TDA_GET_EDID_SOURCE_ADDRESS_CMD";
			break;
		}
	case TDA_SET_GAMMUT_CMD:{
			return "TDA_SET_GAMMUT_CMD";
			break;
		}
	case TDA_GET_EDID_DTD_CMD:{
			return "TDA_GET_EDID_DTD_CMD";
			break;
		}
	case TDA_GET_EDID_MD_CMD:{
			return "TDA_GET_EDID_MD_CMD";
			break;
		}
	case TDA_GET_EDID_TV_ASPECT_RATIO_CMD:{
			return "TDA_GET_EDID_TV_ASPECT_RATIO_CMD";
			break;
		}
	case TDA_GET_EDID_LATENCY_CMD:{
			return "TDA_GET_EDID_LATENCY_CMD";
			break;
		}
	case TDA_GET_HPD_STATUS_CMD:{
			return "TDA_GET_HPD_STATUS_CMD";
			break;
		}
	default:{
			return "unknown";
			break;
		}
	}


}
#endif
/*
 * On HDCP
 */
void _tda19989_hdcp_on(tda_instance *this) {
	int err = 0;
	HDMI_FUNC();

	if (this->tda.hdcp_status != HDCP_IS_NOT_INSTALLED) {	/* check HDCP is installed ... */
		if (this->tda.hdcp_enable) {	/* ... but requested ! */
			TDA_TRY(tmdlHdmiTxSetHdcp(this->tda.instance, True));	/* switch if on */
#if defined(TMFL_TDA19989) || defined(TMFL_TDA9984)
			/* hide video content until HDCP authentification is finished */
			if (!this->tda.setup.simplayHd) {
				TDA_TRY(tmdlHdmiTxSetBScreen
					(this->tda.instance, TMDL_HDMITX_PATTERN_BLUE));
			}
#endif
		}
	}
 TRY_DONE:
	(void)0;
}

/*
 * Off HDCP
 */
void _tda19989_hdcp_off(tda_instance *this) {
	int err = 0;
	HDMI_FUNC();

	if (this->tda.hdcp_status != HDCP_IS_NOT_INSTALLED) {	/* check HDCP is installed ... */

		if (this->tda.hdcp_enable) {	/* but no more requested */
			TDA_TRY(tmdlHdmiTxSetHdcp(this->tda.instance, False));	/* switch if off */
		}
	}
 TRY_DONE:
	(void)0;
}

/*
 * Run video
 */
void _tda19989_show_video(tda_instance *this) {

	int err = 0;

	HDMI_FUNC();
#if 1
	_tda19989_hdcp_off(this);
	TDA_TRY(tmdlHdmiTxSetInputOutput(this->tda.instance,
					 this->tda.setio.video_in,
					 this->tda.setio.video_out,
					 this->tda.setio.audio_in, this->tda.setio.sink));
	_tda19989_hdcp_on(this);
#else
	HDMI_LOG("%s, this->tda.rx_device_active=%d\n", __func__, this->tda.rx_device_active);
	if (this->tda.rx_device_active) {	/* check RxSens */
		HDMI_LOG("%s, this->tda.hot_plug_detect=%d\n", __func__, this->tda.hot_plug_detect);

		if (this->tda.hot_plug_detect == TMDL_HDMITX_HOTPLUG_ACTIVE) {	/* should be useless, but legacy... */

			HDMI_LOG("%s, this->tda.power=%d\n", __func__, this->tda.power);

			if (this->tda.power == tmPowerOn) {	/* check CEC or DSS didn't switch it off */

				HDMI_LOG("%s, this->tda.src_address=0x%08x\n", __func__,
					 this->tda.src_address);
				if (this->tda.src_address != 0xFFFF) {	/* check EDID has been received */
					hdcp_off(this);
					TDA_TRY(tmdlHdmiTxSetInputOutput(this->tda.instance,
									 this->tda.setio.video_in,
									 this->tda.setio.video_out,
									 this->tda.setio.audio_in,
									 this->tda.setio.sink));
					hdcp_on(this);
					/*
					   Mind that SetInputOutput disable the blue color matrix settings of tmdlHdmiTxSetBScreen ...
					   so put tmdlHdmiTxSetBScreen (or hdcp_on) always after
					 */
				}
			}
		}
	}
#endif
 TRY_DONE:
	(void)0;
}

/*
 *  TDA callback
 */
void _tda19989_eventCallbackTx(tmdlHdmiTxEvent_t event) {
	tda_instance *this = &our_instance;
	/* int err=0; */
	unsigned short new_addr;
#if defined(TMFL_TDA19989) || defined(TMFL_TDA9984)
	tda_hdcp_fail hdcp_fail;
#endif
	HDMI_LOG("%s, event=0x%08x\n", __func__, event);

	this->tda.event = event;
	if (TMDL_HDMITX_HDCP_INACTIVE != event) {
		HDMI_LOG("hdmi %s\n", tda_spy_event(event));
	}

	switch (event) {
	case TMDL_HDMITX_EDID_RECEIVED:
		/* xuecheng */
		/* if EDID failed, still continue */
		if (tmdlHdmiTxGetEdidSourceAddress(this->tda.instance, &new_addr)) {
			HDMI_LOG("tmdlHdmiTxGetEdidSourceAddress() failed\n");
		}
		HDMI_LOG("hdmi,phy.@:%x\n", new_addr);
		/*       if (this->tda.src_address == new_addr) { */
		/*          break; */
		/*       } */
		this->tda.src_address = new_addr;
		/* #if defined (TMFL_TDA19989) || defined (TMFL_TDA9984) */
		tda_spy(this->param.verbose > 1);
		/* #endif */
		/*
		   Customer may add stuff to analyse EDID (see tda_spy())
		   and select automatically some video/audio settings.
		   By default, let go on with next case and activate
		   default video/audio settings with tmdlHdmiTxSetInputOutput()
		 */

		if (tmdlHdmiTxGetEdidSinkType(this->tda.instance, &this->tda.setio.sink)) {
			HDMI_LOG("tmdlHdmiTxGetEdidSinkType failed\n");
		}
		if (TMDL_HDMITX_SINK_HDMI != this->tda.setio.sink) {
			HDMI_LOG("/!\\ CAUTION /!\\ sink is not HDMI but %s\n",
				 tda_spy_sink(this->tda.setio.sink));
		}
		msleep(100);

		/*
		   /!\ WARNING /!                                              \
		   the core driver does not send any HPD nor RXSENS when HDMI was plugged after at boot time
		   and only EDID_RECEIVED is send, so rx_device_active shall be forced now.
		   Do not skip the next case nor add any break here please
		 */
	case TMDL_HDMITX_RX_DEVICE_ACTIVE:	/* TV is ready to receive */

		/* TODO: refine this */
		hdmi_util.state_callback(HDMI_STATE_ACTIVE);
		this->tda.rx_device_active = 1;
		/* _tda19989_show_video(this); */
		break;
	case TMDL_HDMITX_RX_DEVICE_INACTIVE:	/* TV is ignoring the source */

		hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);
		this->tda.rx_device_active = 0;
		break;
	case TMDL_HDMITX_HPD_ACTIVE:	/* HDMI is so funny u can get RxSens without being plugged !!! */

		this->tda.hot_plug_detect = TMDL_HDMITX_HOTPLUG_ACTIVE;
		hdmi_util.state_callback(HDMI_STATE_ACTIVE);
		/* _tda19989_show_video(this); */
		break;
	case TMDL_HDMITX_HPD_INACTIVE:	/* unplug */

		this->tda.hot_plug_detect = TMDL_HDMITX_HOTPLUG_INACTIVE;
		this->tda.src_address = 0xFFFF;
		hdmi_util.state_callback(HDMI_STATE_NO_DEVICE);
		break;
#if defined(TMFL_TDA19989) || defined(TMFL_TDA9984)
	case TMDL_HDMITX_HDCP_INACTIVE:	/* HDCP drops off */

		tmdlHdmiTxGetHdcpFailStatus(this->tda.instance,
					    &hdcp_fail, &this->tda.hdcp_raw_status);
		if (this->tda.hdcp_fail != hdcp_fail) {
			if (this->tda.hdcp_fail) {
				LOG(KERN_INFO, "%s (%d)\n",
				    tda_spy_hsdc_fail_status(this->tda.hdcp_fail),
				    this->tda.hdcp_raw_status);
			}
			this->tda.hdcp_fail = hdcp_fail;
			tmdlHdmiTxSetBScreen(this->tda.instance, TMDL_HDMITX_PATTERN_BLUE);
		}
		break;
	case TMDL_HDMITX_RX_KEYS_RECEIVED:	/* end of HDCP authentification */

		if (!this->tda.setup.simplayHd) {
			tmdlHdmiTxRemoveBScreen(this->tda.instance);
		}
		break;
#endif
	default:

		break;
	}

	this->driver.poll_done = true;
/* TRY_DONE: */
	(void)0;
}

void tda19989_cable_fake_plug_in(void) {
	hdmi_util.state_callback(HDMI_STATE_ACTIVE);
}


static int _tda19989_tx_init(tda_instance *this) {
	int err = 0;
	HDMI_FUNC();
	/*Initialize HDMI Transmiter */
	TDA_TRY(tmdlHdmiTxOpen(&this->tda.instance));
	/* Register the HDMI TX events callbacks */
	TDA_TRY(tmdlHdmiTxRegisterCallbacks
		(this->tda.instance, (ptmdlHdmiTxCallback_t) _tda19989_eventCallbackTx));
	/* EnableEvent, all by default */
	TDA_TRY(tmdlHdmiTxEnableEvent(this->tda.instance, TMDL_HDMITX_HDCP_ACTIVE));
	TDA_TRY(tmdlHdmiTxEnableEvent(this->tda.instance, TMDL_HDMITX_HDCP_INACTIVE));
	TDA_TRY(tmdlHdmiTxEnableEvent(this->tda.instance, TMDL_HDMITX_HPD_ACTIVE));
	TDA_TRY(tmdlHdmiTxEnableEvent(this->tda.instance, TMDL_HDMITX_HPD_INACTIVE));
	TDA_TRY(tmdlHdmiTxEnableEvent(this->tda.instance, TMDL_HDMITX_RX_KEYS_RECEIVED));
	TDA_TRY(tmdlHdmiTxEnableEvent(this->tda.instance, TMDL_HDMITX_RX_DEVICE_ACTIVE));
	TDA_TRY(tmdlHdmiTxEnableEvent(this->tda.instance, TMDL_HDMITX_RX_DEVICE_INACTIVE));
	TDA_TRY(tmdlHdmiTxEnableEvent(this->tda.instance, TMDL_HDMITX_EDID_RECEIVED));

	/* Size of the application EDID buffer */
	this->tda.setup.edidBufferSize = EDID_BLOCK_COUNT * EDID_BLOCK_SIZE;
	/* Buffer to store the application EDID data */
	this->tda.setup.pEdidBuffer = this->tda.raw_edid;
	/* To Enable/disable repeater feature, nor relevant here */
	this->tda.setup.repeaterEnable = false;
	/* To enable/disable simplayHD feature: blue screen when not authenticated */
#ifdef SIMPLAYHD
	this->tda.setup.simplayHd = (this->tda.hdcp_enable ? true : false);
#else
	this->tda.setup.simplayHd = false;
#endif
	/* Provides HDMI TX instance configuration */
	TDA_TRY(tmdlHdmiTxInstanceSetup(this->tda.instance, &this->tda.setup));
	/* Get IC version */
	TDA_TRY(tmdlHdmiTxGetCapabilities(&this->tda.capabilities));




 TRY_DONE:
	return err;
}

static int _tda19989_tx_exit(tda_instance *this) {
	int err = 0;
	HDMI_FUNC();
	/*Initialize HDMI Transmiter */
	TDA_TRY(tmdlHdmiTxClose(this->tda.instance));
 TRY_DONE:
	return err;
}

extern tmErrorCode_t tmdlHdmiTxHandleInterrupt(tmInstance_t instance);

static void hdmi_drv_set_util_funcs(const HDMI_UTIL_FUNCS *util) {
	memcpy(&hdmi_util, util, sizeof(HDMI_UTIL_FUNCS));
}

#define USING_720P
static void hdmi_drv_get_params(HDMI_PARAMS *params) {
	memset(params, 0, sizeof(HDMI_PARAMS));
	HDMI_FUNC();
#if defined(USING_720P)
	HDMI_LOG("720p\n");
	params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
#else
	HDMI_LOG("[hdmi_drv]480p\n");
	params->init_config.vformat = HDMI_VIDEO_720x480p_60Hz;
#endif
	params->init_config.aformat = HDMI_AUDIO_PCM_16bit_44100;

	params->clk_pol = HDMI_POLARITY_FALLING;
	params->de_pol = HDMI_POLARITY_RISING;
	params->vsync_pol = HDMI_POLARITY_FALLING;
	params->hsync_pol = HDMI_POLARITY_FALLING;

#if defined(USING_720P)
	HDMI_LOG("[hdmi_drv]720p\n");
#if 1
	params->hsync_front_porch = 110;
	params->hsync_pulse_width = 40;
	params->hsync_back_porch = 220;

	params->vsync_front_porch = 5;
	params->vsync_pulse_width = 5;
	params->vsync_back_porch = 20;
#else
	params->hsync_front_porch = 110;
	params->hsync_pulse_width = 40;
	params->hsync_back_porch = 220;

	params->vsync_front_porch = 5;
	params->vsync_pulse_width = 5;
	params->vsync_back_porch = 20;


#endif
#else
	HDMI_LOG("[hdmi_drv]480p\n");
	params->hsync_front_porch = 16;
	params->hsync_pulse_width = 62;
	params->hsync_back_porch = 60;

	params->vsync_front_porch = 9;
	params->vsync_pulse_width = 6;
	params->vsync_back_porch = 30;
#endif

	params->rgb_order = HDMI_COLOR_ORDER_RGB;

	params->io_driving_current = IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num = 4;

	params->output_mode = HDMI_OUTPUT_MODE_VIDEO_MODE;
	params->is_force_awake = 0;
	params->is_force_landscape = 0;
	params->scaling_factor = 5;
}

#if 0
static void _tda19989_irq_handler(void) {
	/* int ret = 0; */

	HDMI_LOG("[hdmi_drv]hdmi detected!!!\n");
	atomic_set(&hdmi_event, 1);
	wake_up_interruptible(&hdmi_event_wq);

	mt65xx_eint_unmask(8);


}
#endif

extern tmErrorCode_t suspend_i2c(void);
extern tmErrorCode_t resume_i2c(void);

void hdmi_drv_suspend(void) {
	int err = 0;
	tda_instance *this = g_inst;
	HDMI_FUNC();

	this->tda.power = tmPowerSuspend;
	err = tmdlHdmiTxSetPowerState(this->tda.instance, this->tda.power);

	err &= 0xfff;
	if (err == TM_ERR_NO_RESOURCES) {
		HDMI_LOG("[hdmi]Busy...\n");
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
	}

	tmdlHdmiTxGetHPDStatus(this->tda.instance, &this->tda.hot_plug_detect);


 TRY_DONE:
	return /*err */;
}

void hdmi_drv_resume(void) {
	int err = 0;
	tda_instance *this = g_inst;
	HDMI_FUNC();


	this->tda.power = tmPowerOn;
	err = tmdlHdmiTxSetPowerState(this->tda.instance, this->tda.power);
	HDMI_LOG(", %s, err = %d,0x%08x\n", __func__, err, err);
	err &= 0xfff;
	if (err == TM_ERR_NO_RESOURCES) {
		HDMI_LOG("[hdmi]Busy...\n");
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
	}

	tmdlHdmiTxGetHPDStatus(this->tda.instance, &this->tda.hot_plug_detect);


 TRY_DONE:
	return /*err */;
}

tmdlHdmiTxVinMode_t tda19989_vin_format_convert(HDMI_VIDEO_INPUT_FORMAT vin) {
	switch (vin) {
	case HDMI_VIN_FORMAT_RGB565:
		return TMDL_HDMITX_VINMODE_RGB444;
	case HDMI_VIN_FORMAT_RGB666:
		return TMDL_HDMITX_VINMODE_RGB444;
	case HDMI_VIN_FORMAT_RGB888:
		return TMDL_HDMITX_VINMODE_RGB444;
		/* dafault: return TMDL_HDMITX_VINMODE_INVALID; */
	}
	return TMDL_HDMITX_VINMODE_INVALID;	/* avoid warning */
}

tmdlHdmiTxVoutMode_t tda19989_vout_format_convert(HDMI_VIDEO_OUTPUT_FORMAT vout) {
	switch (vout) {
	case HDMI_VOUT_FORMAT_RGB888:
		return TMDL_HDMITX_VOUTMODE_RGB444;
	case HDMI_VOUT_FORMAT_YUV422:
		return TMDL_HDMITX_VOUTMODE_YUV422;
	case HDMI_VOUT_FORMAT_YUV444:
		return TMDL_HDMITX_VOUTMODE_YUV444;
		/* dafault: return TMDL_HDMITX_VOUTMODE_INVALID; */
	}
	return TMDL_HDMITX_VOUTMODE_INVALID;	/* avoid warning */
}

/* TODO: */
static int hdmi_drv_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
				 HDMI_VIDEO_OUTPUT_FORMAT vout) {
	int err = 0;
	tda_instance *this = g_inst;

	HDMI_FUNC();

	/* Main settings */
	this->tda.setio.video_out.mode = TMDL_HDMITX_VOUTMODE_RGB444;
	this->tda.setio.video_out.colorDepth = TMDL_HDMITX_COLORDEPTH_24;
#ifdef TMFL_TDA19989
	this->tda.setio.video_out.dviVqr = TMDL_HDMITX_VQR_DEFAULT;	/* Use HDMI rules for DVI output */
#endif
	/*    this->tda.setio.video_out.format = TMDL_HDMITX_VFMT_31_1920x1080p_50Hz; */
	/*    this->tda.setio.video_out.format = TMDL_HDMITX_VFMT_PC_640x480p_60Hz; */
	/*    this->tda.setio.video_out.format = TMDL_HDMITX_VFMT_PC_640x480p_72Hz; */
	/* this->tda.setio.video_out.format = TMDL_HDMITX_VFMT_04_1280x720p_60Hz; */
	/* this->tda.setio.video_out.format = TMDL_HDMITX_VFMT_19_1280x720p_50Hz; */
	if (vformat == HDMI_VIDEO_720x480p_60Hz) {
		HDMI_LOG("[hdmi_drv]480p\n");
		this->tda.setio.video_out.format = TMDL_HDMITX_VFMT_02_720x480p_60Hz;
	} else if (vformat == HDMI_VIDEO_1280x720p_60Hz) {
		HDMI_LOG("[hdmi_drv]720p\n");
		this->tda.setio.video_out.format = TMDL_HDMITX_VFMT_04_1280x720p_60Hz;
	} else if (vformat == HDMI_VIDEO_1920x1080p_30Hz) {
		HDMI_LOG("[hdmi_drv]1080p\n");
		this->tda.setio.video_out.format = TMDL_HDMITX_VFMT_34_1920x1080p_30Hz;
	} else {
		HDMI_LOG("%s, video format not support now\n", __func__);
	}

	this->tda.setio.video_in.mode = TMDL_HDMITX_VINMODE_RGB444;
	/*    this->tda.setio.video_in.mode = TMDL_HDMITX_VINMODE_CCIR656; */
	/*    this->tda.setio.video_in.mode = TMDL_HDMITX_VINMODE_YUV422; */
	this->tda.setio.video_in.format = this->tda.setio.video_out.format;
	this->tda.setio.video_in.pixelRate = TMDL_HDMITX_PIXRATE_SINGLE;
	this->tda.setio.video_in.syncSource = TMDL_HDMITX_SYNCSRC_EXT_VS;	/* we use HS,VS as synchronisation source */



	this->tda.setio.sink = TMDL_HDMITX_SINK_HDMI;	/* skip edid reading */


	_tda19989_hdcp_off(this);
	TDA_TRY(tmdlHdmiTxSetInputOutput(this->tda.instance,
					 this->tda.setio.video_in,
					 this->tda.setio.video_out,
					 this->tda.setio.audio_in, this->tda.setio.sink));
	_tda19989_hdcp_on(this);
#if 0
	/*    this->tda.src_address = 0x1000; /\* debug *\/ */
	this->tda.src_address = NO_PHY_ADDR;	/* it's unref */
	if (this->tda.rx_device_active) {	/* check RxSens */
		if (this->tda.hot_plug_detect == TMDL_HDMITX_HOTPLUG_ACTIVE) {	/* should be useless, but legacy... */
			if (this->tda.power == tmPowerOn) {	/* check CEC or DSS didn't switch it off */
				/* if (this->tda.src_address != 0xFFFF) { /* check EDID has been received */ */
				_tda19989_hdcp_off(this);
				TDA_TRY(tmdlHdmiTxSetInputOutput(this->tda.instance,
								 this->tda.setio.video_in,
								 this->tda.setio.video_out,
								 this->tda.setio.audio_in,
								 this->tda.setio.sink));
				_tda19989_hdcp_on(this);
				/*
				   Mind that SetInputOutput disable the blue color matrix settings of tmdlHdmiTxSetBScreen ...
				   so put tmdlHdmiTxSetBScreen (or hdcp_on) always after
				 */
				/* } */
			}
		}
	}
	tmdlHdmiTxSetBScreen(this->tda.instance,
			     TMDL_HDMITX_PATTERN_BLUE /*TMDL_HDMITX_PATTERN_CBAR8 */);
#endif
	/* tda19989_colorbar(true); */
	/* return 0; */

 TRY_DONE :
	return 0;
}

static int hdmi_drv_audio_config(HDMI_AUDIO_FORMAT aformat) {
	/* int err = 0; */
	tda_instance *this = g_inst;

	HDMI_FUNC();

	this->tda.setio.audio_in.format = TMDL_HDMITX_AFMT_I2S;
	if (aformat == HDMI_AUDIO_PCM_16bit_48000) {
		this->tda.setio.audio_in.rate = TMDL_HDMITX_AFS_48K;
	} else if (aformat == HDMI_AUDIO_PCM_16bit_44100) {
		this->tda.setio.audio_in.rate = TMDL_HDMITX_AFS_44K;
	} else if (aformat == HDMI_AUDIO_PCM_16bit_32000) {
		this->tda.setio.audio_in.rate = TMDL_HDMITX_AFS_32K;
	} else {
		HDMI_LOG("aformat is not support\n");
	}

	this->tda.setio.audio_in.i2sFormat = TMDL_HDMITX_I2SFOR_PHILIPS_L;
	this->tda.setio.audio_in.i2sQualifier = TMDL_HDMITX_I2SQ_16BITS;
	this->tda.setio.audio_in.dstRate = TMDL_HDMITX_DSTRATE_SINGLE;	/* not relevant here */
	this->tda.setio.audio_in.channelAllocation = 0;	/* audio channel allocation (Ref to CEA-861D p85) */
	/* audio channel allocation (Ref to CEA-861D p85) */
	this->tda.setio.audio_in.channelStatus.PcmIdentification = TMDL_HDMITX_AUDIO_DATA_PCM;
	this->tda.setio.audio_in.channelStatus.CopyrightInfo = TMDL_HDMITX_CSCOPYRIGHT_UNPROTECTED;
	this->tda.setio.audio_in.channelStatus.FormatInfo = TMDL_HDMITX_CSFI_PCM_2CHAN_NO_PRE;
	this->tda.setio.audio_in.channelStatus.categoryCode = 0;
	this->tda.setio.audio_in.channelStatus.clockAccuracy = TMDL_HDMITX_CSCLK_LEVEL_II;
	this->tda.setio.audio_in.channelStatus.maxWordLength = TMDL_HDMITX_CSMAX_LENGTH_24;
	this->tda.setio.audio_in.channelStatus.wordLength = TMDL_HDMITX_CSWORD_DEFAULT;
	this->tda.setio.audio_in.channelStatus.origSampleFreq = TMDL_HDMITX_CSOFREQ_44_1k;
	/* _tda19989_hdcp_off(this); */
	tmdlHdmiTxSetAudioInput(this->tda.instance, this->tda.setio.audio_in, this->tda.setio.sink);
	/* _tda19989_hdcp_on(this); */

/* TRY_DONE: */
	return 0;
}

static int hdmi_drv_video_enable(bool enable) {
	int err = 0;

	tda_instance *this = g_inst;
	HDMI_FUNC();
	LOG(KERN_INFO, "called\n");


	this->driver.omap_dss_hdmi_panel = true;

	if (enable) {
		this->tda.power = tmPowerOn;
		TDA_TRY(tmdlHdmiTxSetPowerState(this->tda.instance, this->tda.power));
		if (err == TM_ERR_NO_RESOURCES) {
			HDMI_LOG("Busy...\n");
			TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
			TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
			TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		}
		tmdlHdmiTxGetHPDStatus(this->tda.instance, &this->tda.hot_plug_detect);
	} else {
		this->tda.power = tmPowerSuspend;
		TDA_TRY(tmdlHdmiTxSetPowerState(this->tda.instance, this->tda.power));
		if (err == TM_ERR_NO_RESOURCES) {
			HDMI_LOG("Busy...\n");
			TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
			TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
			TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		}
		tmdlHdmiTxGetHPDStatus(this->tda.instance, &this->tda.hot_plug_detect);

	}

 TRY_DONE:
	return 0;
}

static int hdmi_drv_audio_enable(bool enable) {
	int err = 0;

	tda_instance *this = g_inst;
	HDMI_FUNC();
	LOG(KERN_INFO, "called\n");

	TDA_TRY(tmdlHdmiTxSetAudioMute(this->tda.instance, !enable));


 TRY_DONE:
	return 0;
}

static int last_hot_plug_detect_status;

static int hdmi_hpd_detect_kthread(void *data) {
	tda_instance *this = g_inst;
	/* int ret = 0; */
	int hpd_result = 0;
	struct sched_param param = {
	.sched_priority = RTPM_PRIO_SCRN_UPDATE};

	tmdlHdmiTxRxSense_t rx_sense_status = TMDL_HDMITX_RX_SENSE_INVALID;
	HDMI_FUNC();

	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		/* ret =tmdlHdmiTxHandleInterrupt(this->tda.instance); */
		/* HDMI_LOG("%s, return %d\n", __func__, ret); */
		/* HDMI_LOG("%s, mdelay begin\n", __func__); */
		tmdlHdmiTxGetHPDStatus(this->tda.instance, &this->tda.hot_plug_detect);
		tmdlHdmiTxGetRXSenseStatus(this->tda.instance, &rx_sense_status);
		this->tda.rx_device_active =
		    (rx_sense_status == TMDL_HDMITX_RX_SENSE_ACTIVE) ? true : false;
		hpd_result = (this->tda.hot_plug_detect || this->tda.rx_device_active);
		if (hpd_result != last_hot_plug_detect_status) {
			HDMI_LOG("==============hdmi detect============\n");
			HDMI_LOG("this->tda.rx_device_active=%d, this->tda.hot_plug_detect=%d\n",
				 this->tda.rx_device_active, this->tda.hot_plug_detect);
			HDMI_LOG("=====================================\n");
			/* tmdlHdmiTxHandleInterrupt(this->tda.instance); */
		}

		tmdlHdmiTxHandleInterrupt(this->tda.instance);
		last_hot_plug_detect_status = hpd_result;
		msleep(500);

		if (kthread_should_stop()) {
			HDMI_LOG("%s, kthread stop\n", __func__);
			break;
		}
	}

	return 0;
}

static int hdmi_drv_init(void) {
	HDMI_FUNC();
	memset((void *)&our_instance, 0, sizeof(tda_instance));

	g_inst = &our_instance;

	Init_i2c();

	return 0;
}

static int hdmi_drv_enter(void) {
	HDMI_FUNC();

	return 0;
}

static int hdmi_drv_exit(void) {
	HDMI_FUNC();

#if 0
	mt_set_gpio_mode(GPIO60, GPIO_MODE_01);
	mt_set_gpio_dir(GPIO60, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO60, true);
	mt_set_gpio_pull_select(GPIO60, GPIO_PULL_UP);


	mt65xx_eint_registration(8, 0, MT65XX_EINT_POL_NEG, NULL, 0);
	mt65xx_eint_set_sens(8, MT65xx_EDGE_SENSITIVE);
	mt65xx_eint_unmask(8);
#endif
	return 0;
}

int hdmi_drv_power_on(void) {
	int err = 0;
	tda_instance *this = g_inst;
	tmdlHdmiTxRxSense_t rx_sense_status = TMDL_HDMITX_RX_SENSE_INVALID;

	HDMI_FUNC();

	resume_i2c();
#if USE_GPIO_HDMI_POWER_CONTROL

#if defined	GPIO_HDMI_POWER_CONTROL
	mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, 0);
#else
	HDMI_LOG("FATAL ERROR!!!, HDMI GPIO is not defined -- GPIO_HDMI_POWER_CONTROL\n");
#endif

#endif
	TDA_TRY(_tda19989_tx_init(g_inst));
	tmdlHdmiTxGetHPDStatus(this->tda.instance, &this->tda.hot_plug_detect);
	HDMI_LOG("%s, before enter standby, hot_plug_detect=%d\n", __func__,
		 this->tda.hot_plug_detect);

	this->tda.power = tmPowerSuspend;
	TDA_TRY(tmdlHdmiTxSetPowerState(this->tda.instance, this->tda.power));
	if (err == TM_ERR_NO_RESOURCES) {
		HDMI_LOG("Busy...\n");
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
	}
#if USE_GPIO_HDMI_POWER_CONTROL

#if defined	GPIO_HDMI_POWER_CONTROL
	mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, 1);
#else
	HDMI_LOG("FATAL ERROR!!!, HDMI GPIO is not defined -- GPIO_HDMI_POWER_CONTROL\n");
#endif

#endif

	tmdlHdmiTxGetRXSenseStatus(this->tda.instance, &rx_sense_status);
	this->tda.rx_device_active =
	    (rx_sense_status == TMDL_HDMITX_RX_SENSE_ACTIVE) ? true : false;
	tmdlHdmiTxGetHPDStatus(this->tda.instance, &this->tda.hot_plug_detect);
	HDMI_LOG("this->tda.rx_device_active=%d, this->tda.hot_plug_detect=%d\n",
		 this->tda.rx_device_active, this->tda.hot_plug_detect);

#if 0
	/* this is for HPD EINT setting. */
	/* please don't delete this, maybe we will use it one day. */
	mt_set_gpio_mode(GPIO60, GPIO_MODE_01);
	mt_set_gpio_dir(GPIO60, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO60, true);
	mt_set_gpio_pull_select(GPIO60, GPIO_PULL_UP);

	mt65xx_eint_set_sens(8, MT65xx_EDGE_SENSITIVE);
	mt65xx_eint_registration(8, 0, MT65XX_EINT_POL_NEG, &_tda19989_irq_handler, 0);

	mt65xx_eint_unmask(8);
#endif

	hdmi_hpd_detect_task =
	    kthread_create(hdmi_hpd_detect_kthread, NULL, "hdmi_hpd_detect_kthread");
	wake_up_process(hdmi_hpd_detect_task);

 TRY_DONE:
	return err;
}


void hdmi_drv_power_off(void) {
	tda_instance *this = g_inst;

	int err = 0;
	HDMI_FUNC();
	this->tda.power = tmPowerOff;
	TDA_TRY(tmdlHdmiTxSetPowerState(this->tda.instance, this->tda.power));
	if (err == TM_ERR_NO_RESOURCES) {
		LOG(KERN_INFO, "Busy...\n");
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
		TDA_TRY(tmdlHdmiTxHandleInterrupt(this->tda.instance));
	}
	kthread_stop(hdmi_hpd_detect_task);
	_tda19989_tx_exit(g_inst);
#if USE_GPIO_HDMI_POWER_CONTROL

#if defined	GPIO_HDMI_POWER_CONTROL
	mt_set_gpio_mode(GPIO_HDMI_POWER_CONTROL, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_POWER_CONTROL, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_POWER_CONTROL, 0);
#else
	HDMI_LOG("FATAL ERROR!!!, HDMI GPIO is not defined -- GPIO_HDMI_POWER_CONTROL\n");
#endif

#endif
	suspend_i2c();

	last_hot_plug_detect_status = 0;

 TRY_DONE:
	return;
}

HDMI_STATE hdmi_drv_get_state(void) {
	tda_instance *this = g_inst;
	tmdlHdmiTxRxSense_t rx_sense_status = TMDL_HDMITX_RX_SENSE_INVALID;
	HDMI_FUNC();

	tmdlHdmiTxGetHPDStatus(this->tda.instance, &this->tda.hot_plug_detect);
	tmdlHdmiTxGetRXSenseStatus(this->tda.instance, &rx_sense_status);
	this->tda.rx_device_active =
	    (rx_sense_status == TMDL_HDMITX_RX_SENSE_ACTIVE) ? true : false;

	if (this->tda.hot_plug_detect || this->tda.rx_device_active)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;
}

void hdmi_drv_log_enable(bool enable) {
	hdmi_log_on = enable;
}

const HDMI_DRIVER *HDMI_GetDriver(void) {
	static const HDMI_DRIVER HDMI_DRV = {
	.set_util_funcs = hdmi_drv_set_util_funcs, .get_params = hdmi_drv_get_params, .init =
		    hdmi_drv_init, .enter = hdmi_drv_enter, .exit = hdmi_drv_exit, .suspend =
		    hdmi_drv_suspend, .resume = hdmi_drv_resume, .video_config =
		    hdmi_drv_video_config, .audio_config =
		    hdmi_drv_audio_config, .video_enable =
		    hdmi_drv_video_enable, .audio_enable = hdmi_drv_audio_enable, .power_on =
		    hdmi_drv_power_on, .power_off = hdmi_drv_power_off, .get_state =
		    hdmi_drv_get_state, .log_enable = hdmi_drv_log_enable};

	HDMI_FUNC();

	return &HDMI_DRV;
}


#endif

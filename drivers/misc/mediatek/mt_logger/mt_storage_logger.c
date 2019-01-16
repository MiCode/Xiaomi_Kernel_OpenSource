/*
 * Copyright (C) 2011 MediaTek, Inc.
 *
 * Author: Holmes Chiou <holmes.chiou@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <stdarg.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <mach/mt_storage_logger.h>
#include <linux/unistd.h>
#include <linux/file.h>
#include <linux/vmalloc.h>
#include <fs/proc/internal.h>
#include <linux/printk.h>
/*
 *  Constant
 */

#define LCA_MAX_SHOW 2048
#define MAX_FILENAME_TOTAL_LENGTH (50)
#define MAX_SINGLE_LINE_SIZE 128
#define MAX_SINGLE_LINE_SIZE_INT	32
#define MAX_WHILE_LOOP 100
#define DUMP_FILE_PATH "/mnt/sdcard/"
#define DEFAULT_FILE_NAME DUMP_FILE_PATH"storage_logger_dump"
#define ACCESS_PERMISSION 0660	/* 666 means that all users can read/write but not execute */

/*
 *  Macro
 */

#define TEST_LOGGER_STORAGE
#undef TEST_LOGGER_STORAGE

#ifdef TEST_LOGGER_STORAGE
#define STORAGE__LOGGER_PRE_WRITE_DATE_TEST()\
{\
  int i;\
  static logger_tester_start;\
  unsigned int *tmpptr;\
  \
  tmpptr = (unsigned int *)storage_logger_mem_pool;\
  for (i = 0; i < storage_logger_bufsize/sizeof(int); i++)\
  {\
   tmpptr[i] = i+logger_tester_start;\
  } \
  \
  tmpptr[i-1] = logger_tester_start;\
  logger_tester_start++;\
}
#else
#define STORAGE__LOGGER_PRE_WRITE_DATE_TEST()
#endif

enum storage_logger_status_code {
	FAIL_TO_DUMP_FILE_FLAG,
	FAIL_TO_OPEN_FILE
};

#define PERFORMANCE_FILE_SET_STATUS_FLAG(sTATUSFLAG, BIT)  sTATUSFLAG |= ((unsigned long)(1<<BIT))
#define PERFORMANCE_FILE_CLEAR_STATUS_FLAG(sTATUSFLAG, BIT)  sTATUSFLAG &= (~(unsigned long)(1<<BIT))
#define PERFORMANCE_FILE_TEST_STATUS_FLAG(sTATUSFLAG, BIT)  (sTATUSFLAG & ((unsigned long)(1<<BIT)))

/*
 *  Global variable
 */
static unsigned int trigger;

static bool storage_logger_iosched;

static unsigned int storage_logger_bufsize;
static unsigned int statusFlag;
static unsigned int wroteFileSize;
static char dump_filename[MAX_FILENAME_TOTAL_LENGTH + 1];

static char printkbuffer[MAX_SINGLE_LINE_SIZE];

static uint *storage_logger_mem_pool;
/* static char* seq_buf_ptr = NULL; */
static int writeIndex;
static int readIndex;
static int readMax = LCA_MAX_SHOW;
static int lastwriteIndex;
static bool iswrapped;
static atomic_t external_read_counter;


static DEFINE_SPINLOCK(logger_lock);

static bool enable_printk;
static bool enable_inMem;
static bool enable_ProcName = true;
static bool enable_StorageLogger;
static bool enable_mmcqd_dump;
static bool enable_blklayer_dump;
static bool enable_vfs_dump;
static bool enable_msdc_dump;

/*========USB PART========*/
static bool enable_musb_dump;
static bool enable_usb_gadget_dump;
/*======Thermal PART======*/
static bool enable_mthermal_dump;
/*========================*/
/*========Threshold for fs ========*/
static int fs_rectime_threshold = 1000;	/* default is 1s */
/*==========================*/

#if defined(FEATURE_STORAGE_PERF_INDEX)
static int mtk_io_osd_config;
static int mtk_io_osd_latency;
extern unsigned int mmcqd_work_percent[];
extern unsigned int mmcqd_w_throughput[];
extern unsigned int mmcqd_r_throughput[];
extern unsigned int mmcqd_read_clear[];
extern pid_t mmcqd[];
#endif
#ifdef DRV_MOUDLE_READY
static int storage_logger_probe(struct platform_device *pdev);
static void storage_logger_remove(struct platform_device *pdev);
static int storage_logger_suspend(struct platform_device *pdev, pm_message_t mesg);
static int storage_logger_resume(struct platform_device *pdev);

static struct class *pclass_storage_logger;
#endif
/* static spinlock_t storage_logger_lock; */

/*========STORAGE PART========*/
static struct _loggerMsgFormat storageLoggerFmt[] = {
	{STORAGE_LOGGER_MSG_ISSUE_RQ, "%d%d%d", "r/w[%d],sec[%d],sz[%d]"},
	{STORAGE_LOGGER_MSG_ISSUE_RQ_1, "%Lu", "dt[%Lu]-end"},
	{STORAGE_LOGGER_MSG_SUBMITBIO, "%d%Lu%s%d", "rw[%d]cur sec[%Lu],dev name[%s],size[%d]"},
	{STORAGE_LOGGER_MSG_VFS_SDCARD, "%s%u", "SD_name:[%s],sz[%u]"},
	{STORAGE_LOGGER_MSG_VFS_SDCARD_END, "%s%X%Lu", "SD_name:[%s],ret[%X],dt[%Lu]-end"},
	{STORAGE_LOGGER_MSG_VFS_OPEN_SDCARD, "%s", "SD_name:[%s]"},
	{STORAGE_LOGGER_MSG_VFS_OPEN_SDCARD_END, "%s%Lu", "SD_name:[%s],dt[%Lu]-end"},
	{STORAGE_LOGGER_MSG_VFS_INTFS, "%s%u", "iFS_name:[%s],sz[%u]"},
	{STORAGE_LOGGER_MSG_VFS_INTFS_END, "%s%X%Lu", "iFS_name:[%s],ret[%X],dt[%Lu]-end"},
	{STORAGE_LOGGER_MSG_VFS_OPEN_INTFS, "%s", "iFS_name:[%s]"},
	{STORAGE_LOGGER_MSG_VFS_OPEN_INTFS_END, "%s%Lu", "iFS_name:[%s],dt[%Lu]-end"},
	{STORAGE_LOGGER_MSG_IOSCHED1, "%d%Lu%Lu%Lu",
	 "nr_iowait[%d][1]iowait_jf[%Lu]total_jf[%Lu]iowait_sum[%Lu]"},
	{STORAGE_LOGGER_MSG_IOSCHED2, "%d%Lu%Lu%Lu%Lu",
	 "nr_iowait[%d][2]iowait_jf[%Lu]total_jf[%Lu]io_count[%Lu]iowait_sum[%Lu]"},
	{STORAGE_LOGGER_MSG_MSDC_DO, "%s%d", "%s,sz[%d]"},
	{STORAGE_LOGGER_MSG_MSDC_DO_END, "%s", "%s"},
	{STORAGE_LOGGER_MSG_GET_REQUEST, "%d%d", "BLK_RW_SYNC[%d],BLK_RW_ASYNC[%d]"},
	{STORAGE_LOGGER_MSG_GET_REQUEST_END, "%d%d", "in_flight1[%d],in_flight0[%d]-end"},
	{STORAGE_LOGGER_MSG_LAST_ONE, ""}
};

static struct _loggerFuncName storageLoggerfunc[] = {
	{STORAGE_LOG_API___submit_bio__func, "submit_bio"},
	{STORAGE_LOG_API___mmc_blk_issue_rq__func, "mmc_blk_issue_rq"},
	{STORAGE_LOG_API___do_sys_open__func, "do_sys_open"},
	{STORAGE_LOG_API___vfs_read__func, "vfs_read"},
	{STORAGE_LOG_API___vfs_write__func, "vfs_write"},
	{STORAGE_LOG_API___msdc_do_request__func, "msdc_do_request"},
	{STORAGE_LOG_API___msdc_ops_request__func, "msdc_ops_request"},
	{STORAGE_LOG_API___io_schedule__func, "io_schedule"},
	{STORAGE_LOG_API___get_request_wait__func, "get_request_wait"},
	{STORAGE_LOG_API___get_request__func, "get_request"},
	{STORAGE_LOG_API___make_request__func, "make_request"},
	{STORAGE_LOG_API___MAX__func, "Invalid function"}
};

/*========USB PART========*/
#define MSG_ARRAY_INDEX(a, b, c) [USB_LOGGER_MSG_##a - USB_LOGGER_MSG_FIRST_ONE] = \
								{USB_LOGGER_MSG_##a, b, c}

static struct _loggerMsgFormat usb_logger_format[] = {
	MSG_ARRAY_INDEX(MUSB_INTERRUPT, "%s%04x%04x%04x", "%s usb%04x tx%04x rx%04x"),
	MSG_ARRAY_INDEX(MUSB_STAGE0_IRQ, "%x%x%x", "Power=%02x, DevCtl=%02x, int_usb=0x%x"),
	MSG_ARRAY_INDEX(MUSB_G_EP0_IRQ, "%04x%d%d%s", "csr=%04x, count %d, myaddr %d, ep0stage %s"),
	MSG_ARRAY_INDEX(TXSTATE, "%d%d%d%03x", "hw_ep%d, maxpacket %d, fifo count %d, txcsr %03x"),
	MSG_ARRAY_INDEX(TXSTATE_END, "%s%s%d%d%04x%d%d",
			"%s TX/IN %s len %d/%d, txcsr %04x, fifo %d/%d"),
	MSG_ARRAY_INDEX(MUSB_G_TX, "%s%04x", "ep=%s, txcsr=%04x"),
	MSG_ARRAY_INDEX(MUSB_G_RX, "%s%04x%s%p", "ep=%s, rxcsr=%04x-%s,req=%p"),
	MSG_ARRAY_INDEX(MUSB_READ_SETUP, "%02x%02x%04x%04x%d", "SETUP bmRequestType=%02x,"
			"bRequest=%02x, wValue=%04x wIndex=%04x wLength=%d"),

	MSG_ARRAY_INDEX(FORWARD_TO_DRIVER, "%s", "call [%s] setup func"),

	MSG_ARRAY_INDEX(COMPOSITE_SETUP, "%02x%02x%04x%04x%d", "bmRequestType=%02x,"
			"bRequest=%02x, wValue=%04x wIndex=%04x wLength=%d"),

	MSG_ARRAY_INDEX(USB_ADD_FUNCTION, "%s%p%s%p", "adding %s/%p to config %s/%p"),
	MSG_ARRAY_INDEX(SET_CONFIG, "%d%d%s", "%d speed config #%d: %s"),
	MSG_ARRAY_INDEX(DEVICE_DESCRIPTOR, "%02x%02x%02x%02x%02x%02x%02x%02x", "config_descriptor, "
			"bLength=%02x, bDescriptorType=%02x, wTotalLength=%02x, "
			"bNumInterfaces=%02x, bConfigurationValue=%02x, iConfiguration=%02x, "
			"bmAttributes=%02x,	bMaxPower=%02x"),
	MSG_ARRAY_INDEX(INTERFACE_DESCRIPTOR, "%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			"interface_descriptor, "
			"bLength=%02x, bDescriptorType=%02x, bInterfaceNumber=%02x, "
			"bAlternateSetting=%02x, bNumEndpoints=%02x, bInterfaceClass=%02x, "
			"bInterfaceSubClass=%02x, bInterfaceProtocol=%02x, iInterface=%2x"),
	MSG_ARRAY_INDEX(ENDPOINT_DESCRIPTOR, "%02x%02x%02x%02x%04x%02x",
			"endpoint_descriptor, "
			"bLength=%02x, bDescriptorType=%02x, bEndpointAddress=%02x, "
			"bmAttributes=%02x, wMaxPacketSize=%04x, bInterval=%02x,"),
	MSG_ARRAY_INDEX(ANDROID_WORK, "%d", "USBSTATE=%d"),
	MSG_ARRAY_INDEX(GS_RX_PUSH, "%d%d%d", "acm port_num=%d,actual=%d,n_read=%d"),
	MSG_ARRAY_INDEX(GS_START_TX, "%d%d", "acm port_num=%d,len=%d"),
	MSG_ARRAY_INDEX(ACM_SETUP, "%d%02x%02x%04x%04x%d",
			"acm ttyGS%d req%02x.%02x v%04x i%04x len=%d"),
	MSG_ARRAY_INDEX(ACM_SET_ALT, "%d%d%d%p%p",
			"acm intf=%d,ctrl_id=%d,data_id=%d,notify=%p,port.in=%p"),
	MSG_ARRAY_INDEX(ACM_BIND, "%d%s%s%s%s", "acm ttyGS%d: %s speed IN/%s OUT/%s NOTIFY/%s"),
	MSG_ARRAY_INDEX(GS_OPEN, "%d%p%p", "acm ttyGS%d (%p,%p)"),
	MSG_ARRAY_INDEX(GS_CLOSE, "%d%p%p", "acm ttyGS%d (%p,%p)"),
	MSG_ARRAY_INDEX(ACM_CDC_LINE_CODING, "%d%d%d%d", "acm rate=%d,stop=%d,parity=%d,data=%d"),
	MSG_ARRAY_INDEX(STRING, "%s%s", "%s=%s"),
	MSG_ARRAY_INDEX(HEX_NUM, "%s%x", "%s=%x"),
	MSG_ARRAY_INDEX(DEC_NUM, "%s%d", "%s=%d"),
	MSG_ARRAY_INDEX(LAST_ONE, "", "")
};

#define MSG_FUNC_INDEX(a, b) [USB_FUNC_STRING_INDEX_##a] = {USB_FUNC_STRING_INDEX_##a, b}

static struct _loggerFuncName usb_func_name_string[] = {
	MSG_FUNC_INDEX(MUSB_INTERRUPT, "musb_interrupt"),
	MSG_FUNC_INDEX(MUSB_STAGE0_IRQ, "musb_stage0_irq"),
	MSG_FUNC_INDEX(MUSB_G_EP0_IRQ, "musb_g_ep0_irq"),
	MSG_FUNC_INDEX(TXSTATE, "txstate"),
	MSG_FUNC_INDEX(MUSB_G_TX, "musb_g_tx"),
	MSG_FUNC_INDEX(MUSB_G_RX, "musb_g_rx"),
	MSG_FUNC_INDEX(MUSB_READ_SETUP, "musb_read_setup"),
	MSG_FUNC_INDEX(FORWARD_TO_DRIVER, "forward_to_driver"),
	MSG_FUNC_INDEX(COMPOSITE_SETUP, "composite_setup"),
	MSG_FUNC_INDEX(USB_ADD_FUNCTION, "usb_add_function"),
	MSG_FUNC_INDEX(SET_CONFIG, "set_config"),
	MSG_FUNC_INDEX(CONFIG_BUF, "config_buf"),
	MSG_FUNC_INDEX(USB_DESCRIPTOR_FILLBUF, "usb_descriptor_fillbuf"),
	MSG_FUNC_INDEX(ANDROID_WORK, "android_work"),
	MSG_FUNC_INDEX(GS_RX_PUSH, "gs_rx_push"),
	MSG_FUNC_INDEX(GS_START_TX, "gs_start_tx"),
	MSG_FUNC_INDEX(ACM_SETUP, "acm_setup"),
	MSG_FUNC_INDEX(ACM_SET_ALT, "acm_set_alt"),
	MSG_FUNC_INDEX(ACM_BIND, "acm_bind"),
	MSG_FUNC_INDEX(GS_OPEN, "gs_open"),
	MSG_FUNC_INDEX(GS_CLOSE, "gs_close"),
	MSG_FUNC_INDEX(ACM_COMPLETE_SET_LINE, "acm_complete_set_line_coding"),
	MSG_FUNC_INDEX(ADB_READ, "adb_read"),
	MSG_FUNC_INDEX(ADB_WRITE, "adb_write"),
	MSG_FUNC_INDEX(MAX, "Invalid function")
};

/*========Thermal PART========*/
#define MSG_THRML_ARRAY_INDEX(a, b, c) [THRML_LOGGER_MSG_##a - THRML_LOGGER_MSG_FIRST_ONE] = \
								{THRML_LOGGER_MSG_##a, b, c}


static struct _loggerMsgFormat thermalLoggerFmt[] = {
	MSG_THRML_ARRAY_INDEX(STRING, "%s%s", "%s=%s"),
	MSG_THRML_ARRAY_INDEX(HEX_NUM, "%s%x", "%s=%x"),
	MSG_THRML_ARRAY_INDEX(DEC_NUM, "%s%d", "%s=%d"),
	MSG_THRML_ARRAY_INDEX(BIND, "%s%d%s", "[Thermal Zone]:%s [Trip]:%d [Cooler]:%s"),
	MSG_THRML_ARRAY_INDEX(ZONE_TEMP, "%s%d", "[Thermal Zone]:%s [Temp]:%d"),
	MSG_THRML_ARRAY_INDEX(COOL_STAE, "%s%d%s%d",
			      "[Thermal Zone]:%s [Trip]:%d [Cooler]:%s [State]:%d"),
	MSG_THRML_ARRAY_INDEX(TRIP_POINT, "%s%d%d",
			      "[Thermal Zone]:%s [Trip ID]:%d [Trip Point]:%d"),
	MSG_THRML_ARRAY_INDEX(BATTERY_INFO, "%d%d%d",
			      "[fg_batt_crrnt]:%d [batt_vol]:%d [batt_temp]:%d"),
	MSG_THRML_ARRAY_INDEX(CPU_INFO, "%d%d%d%d",
			      "[cpu0_usage]:%d [cpu1_usage]:%d [cpu0_freq]:%d [cpu1_freq]:%d"),
	MSG_THRML_ARRAY_INDEX(CPU_INFO_EX, "%d%d%d%d%d%d%d%d%d%d",
			      "[C0U]:%d [C1U]:%d [C2U]:%d [C3U]:%d [C0F]:%d [C1F]:%d [C2F]:%d [C3F]:%d [GU]:%d [GF]:%d"),
	MSG_THRML_ARRAY_INDEX(MISC_INFO, "%d%d", "[Modem TX Power]:%d [wifi_thro]:%d"),
	MSG_THRML_ARRAY_INDEX(MISC_EX_INFO, "%s%d%s", "[Name]:%s [Val]:%d [Unit]:%s"),
	MSG_THRML_ARRAY_INDEX(LAST_ONE, "", "")

};

#define MSG_THRML_FUNC_INDEX(a, b) [THRML_FID_##a] = {THRML_FID_##a, b}

static struct _loggerFuncName thermalLoggerfunc[] = {
	MSG_THRML_FUNC_INDEX(bind, "mtk_thermal_wrapper_bind"),
	MSG_THRML_FUNC_INDEX(unbind, "mtk_thermal_wrapper_unbind"),
	MSG_THRML_FUNC_INDEX(get_temp, "mtk_thermal_wrapper_get_temp"),
	MSG_THRML_FUNC_INDEX(get_mode, "mtk_thermal_wrapper_get_mode"),
	MSG_THRML_FUNC_INDEX(set_mode, "mtk_thermal_wrapper_set_mode"),
	MSG_THRML_FUNC_INDEX(get_trip_type, "mtk_thermal_wrapper_get_trip_type"),
	MSG_THRML_FUNC_INDEX(get_trip_temp, "mtk_thermal_wrapper_get_trip_temp"),
	MSG_THRML_FUNC_INDEX(get_crit_temp, "mtk_thermal_wrapper_get_crit_temp"),
	MSG_THRML_FUNC_INDEX(get_max_state, "mtk_cooling_wrapper_get_max_state"),
	MSG_THRML_FUNC_INDEX(get_cur_state, "mtk_cooling_wrapper_get_cur_state"),
	MSG_THRML_FUNC_INDEX(set_cur_state, "mtk_cooling_wrapper_set_cur_state"),
	MSG_THRML_FUNC_INDEX(get_battery_info, "mtk_thermal_get_battery_info"),
	MSG_THRML_FUNC_INDEX(get_cpu_info, "mtk_thermal_get_cpu_info"),
	MSG_THRML_FUNC_INDEX(get_cpu_info_ex, "mtk_thermal_get_cpu_ex_info"),
	MSG_THRML_FUNC_INDEX(get_misc_info, "mtk_thermal_get_misc_info"),
	MSG_THRML_FUNC_INDEX(get_misc_ex_info, "mtk_thermal_get_misc_ex_info"),
	MSG_THRML_FUNC_INDEX(get_real_time, "mtk_thermal_get_real_time"),
	MSG_THRML_FUNC_INDEX(MAX, "Invalid function")
};


/*========================*/

struct logger_type_info {
	enum logger_type type;
	enum Storage_Logger_MsgID msg_id_start;
	enum Storage_Logger_MsgID msg_id_end;
	enum Storage_LogAPI func_id_max;
	struct _loggerMsgFormat *logger_format;
	struct _loggerFuncName *logger_func_string;
};

static struct logger_type_info logger_info[] = {
	{LOGGER_TYPE_STORAGE, STORAGE_LOGGER_MSG_FIRST_ONE, STORAGE_LOGGER_MSG_LAST_ONE,
	 STORAGE_LOG_API___MAX__func, &storageLoggerFmt[0], &storageLoggerfunc[0]},

	{LOGGER_TYPE_USB, USB_LOGGER_MSG_FIRST_ONE, USB_LOGGER_MSG_LAST_ONE,
	 USB_FUNC_STRING_INDEX_MAX, &usb_logger_format[0], &usb_func_name_string[0]},

	{LOGGER_TYPE_THRML, THRML_LOGGER_MSG_FIRST_ONE, THRML_LOGGER_MSG_LAST_ONE,
	 THRML_FID_MAX, &thermalLoggerFmt[0], &thermalLoggerfunc[0]}

};

#ifdef DRV_MOUDLE_READY
static struct platform_driver storage_logger_driver = {
	.probe = storage_logger_probe,
	.remove = storage_logger_remove,
	.driver = {
		   .name = "storage_logger_driver",
		   .owner = THIS_MODULE,
		   }
};
#endif
static int storage_logger_dump_write_proc(struct file *file, const char *buffer, size_t count,
					  loff_t *data)
{
	char tmpBuf[4];
	unsigned long u4CopySize = 0;
	struct file *fp;
	unsigned int logSize;
	unsigned int wrote = 0;
	mm_segment_t old_fs;
	ssize_t ret;
	char *tempBufPtr;

	tempBufPtr = (char *)storage_logger_mem_pool;
	logSize = storage_logger_bufsize;
	u4CopySize = (count < (sizeof(tmpBuf) - 1)) ? count : (sizeof(tmpBuf) - 1);
	SLog_MSG("storage_logger_bufsize:%d", storage_logger_bufsize);
	if (copy_from_user(tmpBuf, buffer, u4CopySize)) {
		goto error;
	}
	tmpBuf[u4CopySize] = '\0';

	if (0 == sscanf(tmpBuf, "%d", &trigger)) {
		trigger = 0;
		goto error;
	}

	if (NULL == storage_logger_mem_pool) {
		goto error;
	}
	STORAGE__LOGGER_PRE_WRITE_DATE_TEST();

	if (1 == trigger) {
		fp = filp_open(dump_filename, O_WRONLY | O_CREAT | O_TRUNC, 0);

		if (!IS_ERR(fp)) {
			PERFORMANCE_FILE_CLEAR_STATUS_FLAG(statusFlag, FAIL_TO_OPEN_FILE);
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			while (logSize > 0) {
				SLog_MSG("logSize:%d", logSize);
				ret = do_sync_write(fp, tempBufPtr, logSize, &fp->f_pos);
				if (ret <= 0)
					goto end;

				logSize -= ret;
				tempBufPtr += ret;
				wrote += ret;
			}
		} else {
			PERFORMANCE_FILE_SET_STATUS_FLAG(statusFlag, FAIL_TO_OPEN_FILE);
			goto error;
		}
 end:
		set_fs(old_fs);
		ret = filp_close(fp, NULL);
		wroteFileSize = wrote;
		if (ret >= 0) {
			PERFORMANCE_FILE_CLEAR_STATUS_FLAG(statusFlag, FAIL_TO_DUMP_FILE_FLAG);
		} else if (ret < 0) {
			PERFORMANCE_FILE_SET_STATUS_FLAG(statusFlag, FAIL_TO_DUMP_FILE_FLAG);
		}
	}
	return count;
 error:
	trigger = 0;
	return count;
}

static int storage_logger_bufsize_show_proc(struct seq_file *m, void *data)
{

	seq_printf(m, "%d, %dMB\n", storage_logger_bufsize, storage_logger_bufsize / (1024 * 1024));

	return 0;
}

static int storage_logger_bufsize_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, storage_logger_bufsize_show_proc, NULL);
}

static int storage_logger_bufsize_write_proc(struct file *file, const char *buffer, size_t count,
					     loff_t *data)
{
	char tmpBuf[30];
	unsigned long u4CopySize = 0;
	unsigned int local_logger_bs;

	u4CopySize = (count < (sizeof(tmpBuf) - 1)) ? count : (sizeof(tmpBuf) - 1);

	if (copy_from_user(tmpBuf, buffer, u4CopySize))
		return 0;

	if (0 == sscanf(tmpBuf, "%d", &local_logger_bs))
		return count;

	if (0 != local_logger_bs) {
		if ((local_logger_bs != storage_logger_bufsize)) {
			/* Free first, then vmalloc */
			SLog_MSG("storage_logger_mem_pool_addr:0x%p", storage_logger_mem_pool);

			if (NULL != storage_logger_mem_pool)
				vfree(storage_logger_mem_pool);

			storage_logger_mem_pool = vmalloc(local_logger_bs);
			SLog_MSG("storage_logger_mem_pool_addr:0x%p, local_logger_bs:%d,"
				 "storage_logger_bufsize:%d", storage_logger_mem_pool,
				 local_logger_bs, storage_logger_bufsize);
		}
		storage_logger_bufsize = local_logger_bs;

		if (NULL == storage_logger_mem_pool)
			storage_logger_bufsize = 0;
	}
	return count;
}

static int storage_logger_filename_write_proc(struct file *file, const char *buffer, size_t count,
					      loff_t *data)
{
	char tmpBuf[MAX_FILENAME_TOTAL_LENGTH + 1];
	unsigned long u4CopySize = 0;

	u4CopySize = (count < (sizeof(tmpBuf) - 1)) ? count : (sizeof(tmpBuf) - 1);

	if (copy_from_user(tmpBuf, buffer, u4CopySize)) {
		pr_debug("Fail to copy buffer from user space, %s, %d\n", __func__, __LINE__);
		return 0;
	}

	if (count > MAX_FILENAME_TOTAL_LENGTH) {
		BUG_ON(count > MAX_FILENAME_TOTAL_LENGTH);
		return 0;
	}
	sscanf(tmpBuf, "%s", dump_filename);

	return count;
}

static int storage_logger_filename_show_proc(struct seq_file *m, void *data)
{

	seq_printf(m, "%s\n", dump_filename);

	return 0;
}

static int storage_logger_filename_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, storage_logger_filename_show_proc, NULL);
}

bool ioschedule_dump(void)
{
	return storage_logger_iosched;
}

static int storage_logger_iosched_write_proc(struct file *file, const char *buffer, size_t count,
					     loff_t *data)
{
	char tmpBuf[4];
	int u4CopySize = 0;
	int tmp = 0;

	u4CopySize = (count < (sizeof(tmpBuf) - 1)) ? count : (sizeof(tmpBuf) - 1);

	if (copy_from_user(tmpBuf, buffer, u4CopySize))
		return 0;

	tmpBuf[u4CopySize] = '\0';

	if (0 == sscanf(tmpBuf, "%d", &tmp))
		storage_logger_iosched = false;

	if (tmp > 0)
		storage_logger_iosched = true;
	else
		storage_logger_iosched = false;

	return count;
}

static int storage_logger_iosched_show_proc(struct seq_file *m, void *data)
{

	seq_printf(m, "%d\n", storage_logger_iosched);

	return 0;
}

static int storage_logger_iosched_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, storage_logger_iosched_show_proc, NULL);
}

static int storage_logger_status_show_proc(struct seq_file *m, void *data)
{

	seq_printf(m, "[Storage logger status]\nFile name:%s\n", dump_filename);


	if (NULL == storage_logger_mem_pool)
		seq_puts(m, "Storage memory pool is NULL\n");
	else
		seq_printf(m, "Storage memory pool addr:0x%p, size:%d, %dMB\n",
			   storage_logger_mem_pool, storage_logger_bufsize,
			   storage_logger_bufsize / (1024 * 1024));

	if (PERFORMANCE_FILE_TEST_STATUS_FLAG(statusFlag, FAIL_TO_OPEN_FILE))
		seq_puts(m, "Fail to open storage logger file!\n");
	else if (PERFORMANCE_FILE_TEST_STATUS_FLAG(statusFlag, FAIL_TO_DUMP_FILE_FLAG))
		seq_puts(m, "Fail to dump storage logger!\n");
	else {
		if (trigger)
			seq_puts(m, "Storage logger file dump successfully\n");
		else
			seq_puts(m, "Storage logger file not yet dump\n");
	}

	if (storage_logger_iosched)
		seq_puts(m, "io_schedule logger enable\n");
	else
		seq_puts(m, "io_schedule logger disable\n");

	return 0;
}

static int storage_logger_status_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, storage_logger_status_show_proc, NULL);
}

int dumpFsRecTime(void)
{
	return fs_rectime_threshold;
}

static int storage_logger_fs_rectime_threshold_show_proc(struct seq_file *m, void *data)
{

	seq_printf(m, "%d ms\n", fs_rectime_threshold);


	return 0;
}

static int storage_logger_fs_rectime_threshold_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, storage_logger_fs_rectime_threshold_show_proc, NULL);
}

static int storage_logger_fs_rectime_threshold_write_proc(struct file *file, const char *buffer,
							  size_t count, loff_t *data)
{
	char tmpBuf[30];
	unsigned long u4CopySize = 0;
	unsigned int local_logger_bs;

	u4CopySize = (count < (sizeof(tmpBuf) - 1)) ? count : (sizeof(tmpBuf) - 1);

	if (copy_from_user(tmpBuf, buffer, u4CopySize))
		return 0;

	if (0 == sscanf(tmpBuf, "%d", &local_logger_bs))
		return count;

	if (0 != local_logger_bs) {
		fs_rectime_threshold = local_logger_bs;
	}
	/* printk(KERN_INFO " %s, %d - write file system threshold to fs_rectime with %d ms\n", __FUNCTION__, __LINE__, fs_rectime_threshold); */
	return count;
}



bool dumpMMCqd(void)
{
	return enable_mmcqd_dump;
}

bool dumpBlkLayer(void)
{
	return enable_blklayer_dump;
}

bool dumpVFS(void)
{
	return enable_vfs_dump;
}

bool dumpMSDC(void)
{
	return enable_msdc_dump;
}

/*========USB PART========*/
bool is_dump_musb(void)
{
	return enable_musb_dump;
}

bool is_dump_usb_gadget(void)
{
	return enable_usb_gadget_dump;
}

/*======Thermal PART======*/
bool is_dump_mthermal(void)
{
	return enable_mthermal_dump;
}

/*========================*/

static inline void __add_trace(enum logger_type type, unsigned int msg_id,
			       unsigned int line_cnt, unsigned int func_id, va_list ap)
{
	int len = 0;
	int bin_len = 0;
	int err_count = 0;
	uint total_len = 0;
	const char *fmt = NULL;
	unsigned long flags;
	unsigned long long curtime;
	uint localbuffer[MAX_SINGLE_LINE_SIZE_INT];
	uint payload[MAX_SINGLE_LINE_SIZE_INT];


	struct logger_type_info *info = &logger_info[type];

	/*Disable the Storage Logger. Do nothing and just return */
	if (!enable_StorageLogger || (!storage_logger_mem_pool))
		return;

	if (msg_id < info->msg_id_start && msg_id > info->msg_id_end)
		msg_id = info->msg_id_end;

	fmt = info->logger_format[msg_id - info->msg_id_start].MsgFmt;

	/* Get current system clock */
	curtime = sched_clock();


	/* spin_lock_irq(&storage_logger_lock); */
	localbuffer[len++] = msg_id;

	/* store the current->comm into 2 int */
	if (enable_ProcName) {
		/* Max lenth of process name is 8 chars */
		scnprintf((char *)(localbuffer + len), 8, "%8.8s", current->comm);
		len += 2;	/* Shift 2 integer */
	}

	localbuffer[len++] = task_pid_nr(current);
	func_id = min(func_id, (unsigned int)info->func_id_max);

	localbuffer[len++] = func_id;
	localbuffer[len++] = line_cnt;
	localbuffer[len++] = curtime & 0xFFFFFFFF;
	localbuffer[len++] = (curtime >> 32) & 0xFFFFFFFF;

	/* Enabel in RT printk storage logger */
	if (unlikely(enable_printk)) {
		vsnprintf(printkbuffer, MAX_SINGLE_LINE_SIZE,
			  info->logger_format[msg_id - info->msg_id_start].DispFmt, ap);

		STrace_MSG("MsgID: %d, PID: %d, Func: %s, Line: %d, len:%d - %s",
			   msg_id, task_pid_nr(current), info->logger_func_string[func_id].FuncName,
			   line_cnt, len, printkbuffer);
	}

	/* Enabel in memory storage logger */
	if (likely(enable_inMem)) {
#ifdef CONFIG_BINARY_PRINTF
		bin_len = vbin_printf(payload, MAX_SINGLE_LINE_SIZE_INT, fmt, ap);
#endif
		localbuffer[len++] = bin_len;


		/* Lock section */
		spin_lock_irqsave(&logger_lock, flags);

		/* Buffer is overflow!!!=>Reset writeindex = 0; */
		if ((len + bin_len + writeIndex) >= (storage_logger_bufsize >> 2)) {
			iswrapped = true;
			lastwriteIndex = writeIndex;	/* Keep the last */
			writeIndex = 0;
			readIndex = 0;
		}

		if (iswrapped) {
			err_count = 0;
			while ((len + bin_len) >= (readIndex - writeIndex)) {
				/* Check read index over boundary!!! */
				if (writeIndex >=
				    ((storage_logger_bufsize >> 2) -
				     (storage_logger_bufsize >> 6))) {
					readIndex = 0;
					/* printk(KERN_INFO"Holmes Wrapped Wx: %d\n",writeIndex); */
					break;
				}

				if (unlikely(readIndex >= lastwriteIndex)) {
					readIndex = 0;
					break;
				}

				if ((readIndex == 0) && (writeIndex != 0)) {
					readIndex = 0;
					break;
				}
				if (unlikely(err_count > MAX_WHILE_LOOP)) {
					readIndex = 0;
					break;
				}
				BUG_ON(readIndex > (storage_logger_bufsize >> 2));
				total_len = (storage_logger_mem_pool[readIndex] >> 24) & 0xFF;
				BUG_ON(total_len == 0);
				readIndex += total_len;
				err_count++;
			}
		}

		total_len = (len + bin_len) & 0xFF;
		localbuffer[0] = localbuffer[0] | (total_len << 24);

		memcpy(storage_logger_mem_pool + writeIndex, localbuffer, sizeof(uint) * len);
		writeIndex += len;
		memcpy(storage_logger_mem_pool + writeIndex, payload, sizeof(uint) * bin_len);
		writeIndex += bin_len;
		spin_unlock_irqrestore(&logger_lock, flags);

	}
}

void add_trace(enum logger_type type, unsigned int msg_id,
	       unsigned int line_cnt, unsigned int func_id, ...)
{
	va_list ap;
	va_start(ap, func_id);
	__add_trace(type, msg_id, line_cnt, func_id, ap);
	va_end(ap);
}

#ifdef DRV_MOUDLE_READY
static int storage_logger_probe(struct platform_device *pdev)
{
	int ret = 0;
#ifdef DRV_MOUDLE_READY
	if (pdev == NULL) {
		dev_err(&pdev->dev, "pdev is NULL\n");
		return -ENXIO;
	}

	/* Create class register */
	pclass_storage_logger = class_create(THIS_MODULE, "storage_logger_drv");
	if (IS_ERR(pclass_storage_logger)) {
		ret = PTR_ERR(pclass_storage_logger);
		dev_err(&pdev->dev, "Unable to create class, err = %d\n", ret);
		return ret;
	}
#endif

	return ret;

}

static void storage_logger_remove(struct platform_device *pdev)
{
	SLog_MSG("storage_logger_remove");
}

static int storage_logger_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int storage_logger_resume(struct platform_device *pdev)
{
	return 0;
}
#endif



static int storage_logger_show_proc(struct seq_file *m, void *v)
{

	seq_puts(m, "\r\n[storage_logger debug flag]\r\n");
	seq_puts(m, "=========================================\r\n");
	seq_printf(m, "enable_StorageLogger = %d\r\n", enable_StorageLogger);
	seq_printf(m, "readIndex = %d\r\n", readIndex);
	seq_printf(m, "writeIndex = %d\r\n", writeIndex);

	return 0;
}

static int storage_logger_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, storage_logger_show_proc, NULL);
}

static int storage_logger_write_proc(struct file *file, const char *buffer, size_t count,
				     loff_t *data)
{
	char acBuf[32];
	char cmd[32];
	unsigned long CopySize = 0;
	int tmp;

	CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	if (copy_from_user(acBuf, buffer, CopySize))
		return 0;

	acBuf[CopySize] = '\0';

	if (2 == sscanf(acBuf, "%s %d", cmd, &tmp)) {
		SLog_MSG("Your command: %s data: %d", cmd, tmp);
		if (!strncmp(cmd, "ENABLE", sizeof("ENABLE"))) {
			if (atomic_read(&external_read_counter) > 0) {
				return count;
			}

			enable_StorageLogger = (tmp > 0) ? true : false;

			if (enable_StorageLogger) {
				iswrapped = false;
				writeIndex = 0;
				readIndex = 0;
				lastwriteIndex = 0;
			}
		}
	}
	return count;
}

void storage_logger_switch(bool enabled)
{
	enable_StorageLogger = enabled;
	if (enable_StorageLogger) {
		iswrapped = false;
		writeIndex = 0;
		readIndex = 0;
		lastwriteIndex = 0;
	}
}

#if defined(FEATURE_STORAGE_PERF_INDEX)

static int mtk_io_osd_show_proc(struct seq_file *m, void *data)
{
	seq_printf(m, "%d\n%d\n", mtk_io_osd_config, mtk_io_osd_latency);

	return 0;
}

static int mtk_io_osd_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_io_osd_show_proc, NULL);
}

static int mtk_io_osd_write_proc(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char acBuf[32];
	unsigned long CopySize = 0;
	unsigned int p1, p2;

	CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	if (copy_from_user(acBuf, buffer, CopySize))
		return 0;

	acBuf[CopySize] = '\0';

	if (2 == sscanf(acBuf, "%d %d", &p1, &p2)) {
		mtk_io_osd_config = p1;
		mtk_io_osd_latency = p2;
	}
	return count;
}

static int mtk_io_osd_mmcqd1_show_proc(struct seq_file *m, void *data)
{

	if (mmcqd_read_clear[0] == 2) {
		seq_printf(m, "mmcqd1= %d %% \t", mmcqd_work_percent[0]);
		seq_printf(m, "Write= %d kB/s Read= %d kB/s\n", mmcqd_w_throughput[0],
			   mmcqd_r_throughput[0]);
		mmcqd_read_clear[0] = 1;
	} else if (mmcqd_read_clear[0] == 1) {
		seq_printf(m, "mmcqd1= %d %% \t", mmcqd_work_percent[0]);
		seq_printf(m, "Write= %d kB/s Read= %d kB/s\n", mmcqd_w_throughput[0],
			   mmcqd_r_throughput[0]);
		mmcqd_read_clear[0] = 0;
	} else {
		mmcqd_work_percent[0] = 0;
		seq_printf(m, "mmcqd1= %d %% \t", mmcqd_work_percent[0]);
		seq_printf(m, "Write= %d kB/s Read= %d kB/s\n", 0, 0);
		mmcqd_read_clear[0] = 0;
	}

	return 0;
}

static int mtk_io_osd_mmcqd1_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_io_osd_mmcqd1_show_proc, NULL);
}

static int mtk_io_osd_mmcqd2_show_proc(struct seq_file *m, void *data)
{
	if (0 == mmcqd[3]) {
		seq_puts(m, " ");
	} else {

		if (mmcqd_read_clear[3] == 2) {
			seq_printf(m, "mmcqd2= %d %% \t", mmcqd_work_percent[3]);
			seq_printf(m, "Write= %d kB/s Read= %d kB/s\n", mmcqd_w_throughput[3],
				   mmcqd_r_throughput[3]);
			mmcqd_read_clear[3] = 1;
		} else if (mmcqd_read_clear[3] == 1) {
			seq_printf(m, "mmcqd2= %d %% \t", mmcqd_work_percent[3]);
			seq_printf(m, "Write= %d kB/s Read= %d kB/s\n", mmcqd_w_throughput[3],
				   mmcqd_r_throughput[3]);
			mmcqd_read_clear[3] = 0;
		} else {
			mmcqd_work_percent[3] = 0;
			seq_printf(m, "mmcqd2= %d %% \t", mmcqd_work_percent[3]);
			seq_printf(m, "Write= %d kB/s Read= %d kB/s\n", 0, 0);
			mmcqd_read_clear[3] = 0;
		}
	}


	return 0;
}


static int mtk_io_osd_mmcqd2_open_proc(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_io_osd_mmcqd2_show_proc, NULL);
}

#endif

static int storage_logger_show_flag(struct seq_file *m, void *data)
{

	seq_puts(m, "\r\n[storage_logger debug flag]\r\n");
	seq_puts(m, "=========================================\r\n");
	seq_printf(m, "enable_inMem = %d\r\n", enable_inMem);
	seq_printf(m, "enable_printk = %d\r\n", enable_printk);
	seq_printf(m, "enable_mmcqd_dump = %d\r\n", enable_mmcqd_dump);
	seq_printf(m, "enable_blklayer_dump = %d\r\n", enable_blklayer_dump);
	seq_printf(m, "enable_vfs_dump = %d\r\n", enable_vfs_dump);
	seq_printf(m, "enable_msdc_dump = %d\r\n", enable_msdc_dump);
	seq_printf(m, "enable_ProcName = %d\r\n", enable_ProcName);

	return 0;
}

static int storage_logger_open_flag(struct inode *inode, struct file *file)
{
	return single_open(file, storage_logger_show_flag, NULL);
}

char acBuf[MAX_SINGLE_LINE_SIZE];
char cmd[MAX_SINGLE_LINE_SIZE];

static int storage_logger_write_flag(struct file *file, const char *buffer, size_t count,
				     loff_t *data)
{
	unsigned long CopySize = 0;
	unsigned int tmp1, tmp2, tmp3, tmp4;

	CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	if (copy_from_user(acBuf, buffer, CopySize))
		return 0;
	acBuf[CopySize] = '\0';

	if (4 == sscanf(acBuf, "%d %d %d %d", &tmp1, &tmp2, &tmp3, &tmp4)) {
		enable_mmcqd_dump = (tmp1 > 0) ? true : false;
		enable_blklayer_dump = (tmp2 > 0) ? true : false;
		enable_vfs_dump = (tmp3 > 0) ? true : false;
		enable_msdc_dump = (tmp4 > 0) ? true : false;
	} else if (3 == sscanf(acBuf, "%d %d %d", &tmp1, &tmp2, &tmp3)) {
		enable_inMem = (tmp1 > 0) ? true : false;
		enable_printk = (tmp2 > 0) ? true : false;
		enable_ProcName = (tmp3 > 0) ? true : false;
	} else if (2 == sscanf(acBuf, "%s %d", cmd, &tmp1)) {
		if (!strncmp(cmd, "read", sizeof("read")))
			readMax = tmp1;
	} else if (1 == sscanf(acBuf, "%s", cmd)) {
		if (!strncmp(cmd, "reset", sizeof("reset")))
			readIndex = 0;
	}

	return count;
}

static int usb_logger_show_flag(struct seq_file *m, void *data)
{

	seq_puts(m, "\r\n[usb_logger debug flag]\r\n");
	seq_puts(m, "=========================================\r\n");
	seq_printf(m, "readIndex              = %d\r\n", readIndex);
	seq_printf(m, "writeIndex             = %d\r\n", writeIndex);
	seq_printf(m, "Enable logger          = %d\r\n", enable_StorageLogger);
	seq_printf(m, "Allocated memory       = %d\r\n", !!storage_logger_mem_pool);
	seq_printf(m, "Maximun dumped items   = %d\r\n", readMax);
	seq_printf(m, "Dump IO schedule       = %d\r\n", !!storage_logger_iosched);
	seq_printf(m, "Store in memory  (Bit1)= %d\r\n", enable_inMem);
	seq_printf(m, "Printout         (Bit2)= %d\r\n", enable_printk);
	seq_puts(m, "-----------------------------------------\r\n");
	seq_printf(m, "Dump MUSB        (Bit3)= %d\r\n", enable_musb_dump);
	seq_printf(m, "Dump Gadget      (Bit4)= %d\r\n", enable_usb_gadget_dump);

	return 0;
}


static int usb_logger_open_flag(struct inode *inode, struct file *file)
{
	return single_open(file, usb_logger_show_flag, NULL);
}

static int usb_logger_write_flag(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	char kbuf[MAX_SINGLE_LINE_SIZE];
	unsigned long len = 0;
	unsigned int tmp = 0;

	len = min((unsigned long)count, (unsigned long)(sizeof(kbuf) - 1));

	if (copy_from_user(kbuf, buffer, len))
		return 0;

	kbuf[len] = '\0';

	if (1 == sscanf(kbuf, "%d", &tmp)) {
		enable_inMem = ((tmp >> 0) & 0x1) ? true : false;
		enable_printk = ((tmp >> 1) & 0x1) ? true : false;
		enable_musb_dump = ((tmp >> 2) & 0x1) ? true : false;
		enable_usb_gadget_dump = ((tmp >> 3) & 0x1) ? true : false;
	}
	return count;
}

static int thermal_logger_show_flag(struct seq_file *m, void *data)
{

	seq_puts(m, "\r\n[thermal_logger debug flag]\r\n");
	seq_puts(m, "=========================================\r\n");
	seq_printf(m, "readIndex              = %d\r\n", readIndex);
	seq_printf(m, "writeIndex             = %d\r\n", writeIndex);
	seq_printf(m, "Enable logger          = %d\r\n", enable_StorageLogger);
	seq_printf(m, "Allocated memory       = %d\r\n", !!storage_logger_mem_pool);
	seq_printf(m, "Maximun dumped items   = %d\r\n", readMax);
	seq_printf(m, "Dump IO schedule       = %d\r\n", !!storage_logger_iosched);
	seq_printf(m, "Store in memory  (Bit1)= %d\r\n", enable_inMem);
	seq_printf(m, "Printout         (Bit2)= %d\r\n", enable_printk);
	seq_puts(m, "-----------------------------------------\r\n");
	seq_printf(m, "Dump MTHERMAL        (Bit3)= %d\r\n", enable_mthermal_dump);

	return 0;
}

static int thermal_logger_open_flag(struct inode *inode, struct file *file)
{
	return single_open(file, thermal_logger_show_flag, NULL);
}

static int thermal_logger_write_flag
    (struct file *file, const char *buffer, size_t count, loff_t *data) {
	char kbuf[MAX_SINGLE_LINE_SIZE];
	unsigned long len = 0;
	unsigned int tmp = 0;

	len = min((unsigned long)count, (unsigned long)(sizeof(kbuf) - 1));

	if (copy_from_user(kbuf, buffer, len))
		return 0;

	kbuf[len] = '\0';

	if (1 == sscanf(kbuf, "%d", &tmp)) {
		enable_inMem = ((tmp >> 0) & 0x1) ? true : false;
		enable_printk = ((tmp >> 1) & 0x1) ? true : false;
		enable_mthermal_dump = ((tmp >> 2) & 0x1) ? true : false;
	}
	return count;

}



static inline struct logger_type_info *find_type_info(unsigned int msg_id)
{
	unsigned int tmp = (unsigned int)LOGGER_TYPE_START;
	for (; tmp < LOGGER_TYPE_END; tmp++) {
		if (logger_info[tmp].msg_id_start <= msg_id
		    && logger_info[tmp].msg_id_end >= msg_id)
			return &logger_info[tmp];
	}

	/* Cant find match type for this msg id, give the storage type. */
	return &logger_info[LOGGER_TYPE_START];
}

#if 1
static int storage_logger_proc_show(struct seq_file *m, void *v)
{
	SLog_MSG("storage_logger_proc_show");
	/* seq_buf_ptr = m->buf; */
	m->buf = (char *)storage_logger_mem_pool;
	m->count = writeIndex << 2;
	m->size = storage_logger_bufsize;
	return 0;
}
#endif

#if 0
static int storage_logger_proc_show(struct seq_file *m, void *v)
{
	int binOffset = 0;
	int nrItem = 0;
	char *fmt;
	char procname[16];
	unsigned long long curtime_high, curtime_low;
	unsigned long nanosec_rem;
	uint localbuffer[MAX_SINGLE_LINE_SIZE_INT];

	while (binOffset < writeIndex) {
		unsigned int msg_id, tmp_pid, func_id, line_cnt, payload_len;
		struct logger_type_info *info = NULL;

		/*Parser the data from the log memory */
		msg_id = storage_logger_mem_pool[binOffset++];
		msg_id = min(msg_id, (unsigned int)LOGGER_MSG_ID_MAX);

		if (enable_ProcName) {
			memcpy(procname, (char *)(storage_logger_mem_pool + binOffset), 8);
			procname[8] = '\0';
			binOffset += 2;
		}

		tmp_pid = storage_logger_mem_pool[binOffset++];
		func_id = storage_logger_mem_pool[binOffset++];
		line_cnt = storage_logger_mem_pool[binOffset++];

		curtime_low = storage_logger_mem_pool[binOffset++];
		curtime_high = storage_logger_mem_pool[binOffset++];
		curtime_low += (curtime_high << 32);

		payload_len = storage_logger_mem_pool[binOffset++];

		info = find_type_info(msg_id);
		func_id = min(func_id, info->func_id_max);

		/* Output the relative info into the display buffer */
		if (info->type == LOGGER_TYPE_USB) {
			nanosec_rem = do_div(curtime_low, 1000000000);
			seq_printf(m, "[USB] MsgID:%d,PID:[%s|%d]-%s @%d,[%5lu.%06lu]-",
				   msg_id, procname, tmp_pid,
				   info->logger_func_string[func_id].FuncName, line_cnt,
				   (unsigned long)curtime_low, nanosec_rem / 1000);
		} else {
			seq_printf(m, "MsgID:%d,PID:[%s|%d],%s_%d,[%Lu]-",
				   msg_id, procname, tmp_pid,
				   info->logger_func_string[func_id].FuncName, line_cnt,
				   curtime_low);
		}

		/* Decode the binary buffer */
		fmt = info->logger_format[msg_id - (info->msg_id_start)].DispFmt;

#ifdef CONFIG_BINARY_PRINTF
		bstr_printf((char *)localbuffer, MAX_SINGLE_LINE_SIZE, fmt,
			    storage_logger_mem_pool + binOffset);
#endif
		/* Output the decoded info to the display buffer */
		seq_printf(m, " %s\n", (char *)localbuffer);
		binOffset += payload_len;	/*Payload is decoded!!!! */
		nrItem++;

		if (nrItem > readMax && readMax != 0)
			break;
	}

	seq_printf(m, "Storage logger - nrItem = %d\r\n", nrItem - 1);
	SLog_MSG("Finish nrItem %d, Wx= %d,Rx = %d", nrItem - 1, writeIndex, readIndex);

	return 0;
}
#endif

static int straverse(struct seq_file *m, loff_t offset)
{
	loff_t pos = 0, index;
	int error = 0;
	void *p;

	m->version = 0;
	index = 0;
	m->count = m->from = 0;
	if (!offset) {
		m->index = index;
		return 0;
	}
#if 0				/* No need for storage logger */
	if (!m->buf) {
		m->buf = vmalloc(m->size = PAGE_SIZE);
		if (!m->buf)
			return -ENOMEM;
	}
#endif
	p = m->op->start(m, &index);
	while (p) {
		error = PTR_ERR(p);
		if (IS_ERR(p))
			break;
		error = m->op->show(m, p);
		if (error < 0)
			break;
		if (unlikely(error)) {
			error = 0;
			m->count = 0;
		}
		if (m->count == m->size)
			goto Eoverflow;
		if (pos + m->count > offset) {
			m->from = offset - pos;
			m->count -= m->from;
			m->index = index;
			break;
		}
		pos += m->count;
		m->count = 0;
		if (pos == offset) {
			index++;
			m->index = index;
			break;
		}
		p = m->op->next(m, p, &index);

	}
	m->op->stop(m, p);
	m->index = index;
	return error;

 Eoverflow:
	m->op->stop(m, p);
#if 0				/* No need for storage logger */
	vfree(m->buf);
	m->buf = vmalloc(m->size <<= 1);
#endif
	return !m->buf ? -ENOMEM : -EAGAIN;
}


ssize_t storage_logger_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	size_t copied = 0;
	size_t n;
	int err = 0;

	/* Force disable the storage logger */
	if (enable_StorageLogger) {
		enable_StorageLogger = false;
		/* Boundary value check */
		BUG_ON(readIndex > (storage_logger_bufsize >> 2));
		BUG_ON(writeIndex > (storage_logger_bufsize >> 2));
		BUG_ON(lastwriteIndex > (storage_logger_bufsize >> 2));
		BUG_ON(readIndex > lastwriteIndex);

		/* Take a sleep to wait the storage logger stopping */
		msleep(10);
	}
	/* SLog_MSG("storage_logger_proc_read: %Lu %d  %d %d\n",*ppos,readIndex,writeIndex,lastwriteIndex); */

	mutex_lock(&m->lock);

	if (*ppos == 0) {
		/* seq_buf_ptr = m->buf; */

		if (!iswrapped)
			lastwriteIndex = writeIndex;

		m->buf = (char *)(&storage_logger_mem_pool[readIndex]);
		m->count = (lastwriteIndex - readIndex) << 2;
		m->size = storage_logger_bufsize;
		m->index = 0;
	}


	m->version = file->f_version;
	/* grab buffer if we didn't have one */

	if (m->count) {
		n = min(m->count, size);
		err = copy_to_user(buf, m->buf + m->from, n);
		/* SLog_MSG("copy_to_user: %d\n",n); */

		if (err)
			goto Done;
		m->count -= n;
		m->from += n;
		size -= n;
		buf += n;
		copied += n;
		if (!size)
			goto Done;
		if (!iswrapped)
			goto Done;
	}

	if (m->index)
		goto Done;

	/* we put the bottom record into buffer */
	if (writeIndex < readIndex) {
		m->index++;
		m->buf = (char *)(&storage_logger_mem_pool[0]);
		m->count = writeIndex << 2;
		m->size = storage_logger_bufsize;
		m->from = 0;
	}
	/* SLog_MSG("second part: %Lu  %d\n",m->index,m->count); */

 Done :
	if (!copied)
		copied = err;
	else {
		*ppos += copied;
		m->read_pos += copied;
	}
	file->f_version = m->version;
	mutex_unlock(&m->lock);
	/* SLog_MSG("return: %d  %d\n",copied,m->count); */
	return copied;
}

#if 0
ssize_t storage_logger_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	size_t copied = 0;
	loff_t pos;
	size_t n;
	void *p;
	int err = 0;

	/* Force disable the storage logger */
	if (enable_StorageLogger) {
		enable_StorageLogger = false;
		/* Take a sleep to wait the storage logger stopping */
		msleep(10);
	}
	printk(KERN_INFO "Holmes storage_logger_proc_read: %d %Lu %Lu\n", size, *ppos, m->read_pos);
	mutex_lock(&m->lock);

	/* Don't assume *ppos is where we left it */
	if (unlikely(*ppos != m->read_pos)) {
		m->read_pos = *ppos;
		while ((err = straverse(m, *ppos)) == -EAGAIN);

		if (err) {
			/* With prejudice... */
			m->read_pos = 0;
			m->version = 0;
			m->index = 0;
			m->count = 0;
			goto Done;
		}
	}

	/*
	 * seq_file->op->..m_start/m_stop/m_next may do special actions
	 * or optimisations based on the file->f_version, so we want to
	 * pass the file->f_version to those methods.
	 *
	 * seq_file->version is just copy of f_version, and seq_file
	 * methods can treat it simply as file version.
	 * It is copied in first and copied out after all operations.
	 * It is convenient to have it as  part of structure to avoid the
	 * need of passing another argument to all the seq_file methods.
	 */
	m->version = file->f_version;
	/* grab buffer if we didn't have one */
	if (!m->buf) {
		m->buf = vmalloc(m->size = PAGE_SIZE);
		if (!m->buf)
			goto Enomem;
	}
	/* if not empty - flush it first */
	if (m->count) {
		n = min(m->count, size);
		err = copy_to_user(buf, m->buf + m->from, n);
		printk(KERN_INFO "Holmes copy_to_user: %d\n", n);

		if (err)
			goto Efault;
		m->count -= n;
		m->from += n;
		size -= n;
		buf += n;
		copied += n;
		if (!m->count)
			m->index++;
		if (!size)
			goto Done;
	}
	/* we need at least one record in buffer */
	pos = m->index;
	p = m->op->start(m, &pos);
	printk(KERN_INFO "Holmes start: %Lu     %d\n", m->index, m->count);

	while (1) {
		err = PTR_ERR(p);
		if (!p || IS_ERR(p))
			break;
		err = m->op->show(m, p);

		printk(KERN_INFO "Holmes show: %Lu   %d\n", m->index, m->count);

		if (err < 0)
			break;
		if (unlikely(err))
			m->count = 0;
		if (unlikely(!m->count)) {
			p = m->op->next(m, p, &pos);
			m->index = pos;
			continue;
		}
		if (m->count < m->size)
			goto Fill;
		m->op->stop(m, p);
		vfree(m->buf);
		m->buf = vmalloc(m->size <<= 1);
		if (!m->buf)
			goto Enomem;
		m->count = 0;
		m->version = 0;
		pos = m->index;
		p = m->op->start(m, &pos);
		printk(KERN_INFO "Holmes start2: %Lu   %d\n", m->index, m->count);
	}
	m->op->stop(m, p);
	m->count = 0;
	goto Done;
 Fill:
	/* they want more? let's try to get some more */
	while (m->count < size) {
		size_t offs = m->count;
		loff_t next = pos;
		p = m->op->next(m, p, &next);
		if (!p || IS_ERR(p)) {
			err = PTR_ERR(p);
			break;
		}
		err = m->op->show(m, p);
		if (m->count == m->size || err) {
			m->count = offs;
			if (likely(err <= 0))
				break;
		}
		pos = next;
	}
	m->op->stop(m, p);
	n = min(m->count, size);
	err = copy_to_user(buf, m->buf, n);
	printk(KERN_INFO "Holmes copy_to_user: %d\n", n);

	if (err)
		goto Efault;
	copied += n;
	m->count -= n;
	if (m->count)
		m->from = n;
	else
		pos++;
	m->index = pos;
 Done:
	if (!copied)
		copied = err;
	else {
		*ppos += copied;
		m->read_pos += copied;
	}
	file->f_version = m->version;
	mutex_unlock(&m->lock);
	printk(KERN_INFO "Holmes return: %d\n", copied);

	return copied;
 Enomem:
	err = -ENOMEM;
	goto Done;
 Efault:
	err = -EFAULT;
	goto Done;
}
#endif

int __storage_logger_proc_release(struct inode *inode, struct file *file)
{
	struct seq_file *m = file->private_data;
#if 0				/* No need for storage logger */
	m->buf = seq_buf_ptr;
	vfree(m->buf);
#endif
	kfree(m);
	return 0;
}


int storage_logger_proc_release(struct inode *inode, struct file *file)
{
	unsigned long flags;
	const struct seq_operations *op = ((struct seq_file *)file->private_data)->op;
	int res = __storage_logger_proc_release(inode, file);
	kfree(op);

	spin_lock_irqsave(&logger_lock, flags);
	atomic_dec(&external_read_counter);
	spin_unlock_irqrestore(&logger_lock, flags);

	return res;
}

loff_t storage_logger_proc_lseek(struct file *file, loff_t offset, int origin)
{
	struct seq_file *m = file->private_data;
	loff_t retval = -EINVAL;

	mutex_lock(&m->lock);
	m->version = file->f_version;
	switch (origin) {
	case 1:
		offset += file->f_pos;
	case 0:
		if (offset < 0)
			break;
		retval = offset;
		if (offset != m->read_pos) {
			while ((retval = straverse(m, offset)) == -EAGAIN);
			if (retval) {
				/* with extreme prejudice... */
				file->f_pos = 0;
				m->read_pos = 0;
				m->version = 0;
				m->index = 0;
				m->count = 0;
			} else {
				m->read_pos = offset;
				retval = file->f_pos = offset;
			}
		}
	}
	file->f_version = m->version;
	mutex_unlock(&m->lock);
	return retval;
}

static void *single_start(struct seq_file *p, loff_t *pos)
{
	return NULL + (*pos == 0);
}

static void *single_next(struct seq_file *p, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void single_stop(struct seq_file *p, void *v)
{
}

static int storage_logger_proc_single_open(struct file *file,
					   int (*show) (struct seq_file *, void *), void *data)
{
	struct seq_operations *op = kmalloc(sizeof(*op), GFP_KERNEL);
	int res = -ENOMEM;

	if (op) {
		op->start = single_start;
		op->next = single_next;
		op->stop = single_stop;
		op->show = show;
		res = seq_open(file, op);
		if (!res)
			((struct seq_file *)file->private_data)->private = data;
		else
			kfree(op);
	}
	return res;
}

static int storage_logger_proc_open(struct inode *inode, struct file *file)
{
	unsigned long flags;
	spin_lock_irqsave(&logger_lock, flags);
	atomic_inc(&external_read_counter);
	spin_unlock_irqrestore(&logger_lock, flags);

	return storage_logger_proc_single_open(file, storage_logger_proc_show, NULL);
}


static const struct file_operations storage_logger_proc_fops = {
	.owner = THIS_MODULE,
	.open = storage_logger_proc_open,
	.read = seq_read,
	.llseek = storage_logger_proc_lseek,
	.release = storage_logger_proc_release,
};

static const struct file_operations driver_base_proc_fops = {
	.owner = THIS_MODULE,
	.write = storage_logger_write_proc,
	.read = seq_read,
	.open = storage_logger_open_proc,
};

static const struct file_operations driver_config_proc_fops = {
	.owner = THIS_MODULE,
	.write = storage_logger_write_flag,
	.read = seq_read,
	.open = storage_logger_open_flag,
};

static const struct file_operations driver_usb_config_proc_fops = {
	.owner = THIS_MODULE,
	.write = usb_logger_write_flag,
	.read = seq_read,
	.open = usb_logger_open_flag,
};

static const struct file_operations driver_thermal_config_proc_fops = {
	.owner = THIS_MODULE,
	.write = thermal_logger_write_flag,
	.read = seq_read,
	.open = thermal_logger_open_flag,
};

#if defined(FEATURE_STORAGE_PERF_INDEX)

static const struct file_operations mtk_io_osd_config_proc_fops = {
	.owner = THIS_MODULE,
	.write = mtk_io_osd_write_proc,
	.read = seq_read,
	.open = mtk_io_osd_open_proc,
};

static const struct file_operations mtk_io_osd_mmcqd1_proc_fops = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.open = mtk_io_osd_mmcqd1_open_proc,
};

static const struct file_operations mtk_io_osd_mmcqd2_proc_fops = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.open = mtk_io_osd_mmcqd2_open_proc,
};

#endif

static const struct file_operations driver_dump_proc_fops = {
	.owner = THIS_MODULE,
	.write = storage_logger_dump_write_proc,
};

static const struct file_operations driver_bufsize_proc_fops = {
	.owner = THIS_MODULE,
	.write = storage_logger_bufsize_write_proc,
	.read = seq_read,
	.open = storage_logger_bufsize_open_proc,
};

static const struct file_operations driver_filename_proc_fops = {
	.owner = THIS_MODULE,
	.write = storage_logger_filename_write_proc,
	.read = seq_read,
	.open = storage_logger_filename_open_proc,
};

static const struct file_operations driver_status_proc_fops = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.open = storage_logger_status_open_proc,
};

static const struct file_operations driver_iosched_proc_fops = {
	.owner = THIS_MODULE,
	.write = storage_logger_iosched_write_proc,
	.read = seq_read,
	.open = storage_logger_iosched_open_proc,
};

static const struct file_operations driver_fs_rectime_proc_fops = {
	.owner = THIS_MODULE,
	.write = storage_logger_fs_rectime_threshold_write_proc,
	.read = seq_read,
	.open = storage_logger_fs_rectime_threshold_open_proc,
};

static int __init storage_logger_init(void)
{
	int ret = 0;
	struct proc_dir_entry *procEntry = NULL;

#ifdef DRV_MOUDLE_READY
	ret = platform_driver_register(&storage_logger_driver);
#endif

	CREATE_PROC_ENTRY(procEntry, "driver/storage_logger", ACCESS_PERMISSION, NULL,
			  &driver_base_proc_fops);
	if (procEntry) {
		procEntry->gid = 1000;
	} else
		SLog_MSG("add /proc/driver/storage_logger entry fail");

	CREATE_PROC_ENTRY(procEntry, "driver/storage_logger_config", ACCESS_PERMISSION, NULL,
			  &driver_config_proc_fops);
	if (procEntry) {
		procEntry->gid = 1000;
	} else
		SLog_MSG("add /proc/driver/storage_logger_config entry fail");

	CREATE_PROC_ENTRY(procEntry, "driver/usb_logger_config", ACCESS_PERMISSION, NULL,
			  &driver_usb_config_proc_fops);
	if (!procEntry)
		SLog_MSG("add /proc/driver/usb_logger_config entry fail");

	/* Thermal Log */
	CREATE_PROC_ENTRY(procEntry, "driver/thermal_logger_config", ACCESS_PERMISSION, NULL,
			  &driver_thermal_config_proc_fops);
	if (procEntry) {
		procEntry->gid = 1000;
	} else
		SLog_MSG("add /proc/driver/thermal_logger_config entry fail");

	CREATE_PROC_ENTRY(procEntry, "driver/storage_logger_display", ACCESS_PERMISSION, NULL,
			  &storage_logger_proc_fops);
	if (procEntry) {
		procEntry->gid = 1000;
	} else
		SLog_MSG("add /proc/driver/storage_logger entry fail");

#if defined(FEATURE_STORAGE_PERF_INDEX)

	CREATE_PROC_ENTRY(procEntry, "driver/mtk_io_osd_config", ACCESS_PERMISSION, NULL,
			  &mtk_io_osd_config_proc_fops);
	if (procEntry) {
		procEntry->gid = 1000;
	} else
		SLog_MSG("add /proc/driver/mtk_io_osd entry fail");

	CREATE_PROC_ENTRY(procEntry, "driver/mtk_io_osd_mmcqd1", ACCESS_PERMISSION, NULL,
			  &mtk_io_osd_mmcqd1_proc_fops);
	if (procEntry) {
		procEntry->gid = 1000;
	} else
		SLog_MSG("add /proc/driver/mtk_io_osd entry fail");

	CREATE_PROC_ENTRY(procEntry, "driver/mtk_io_osd_mmcqd2", ACCESS_PERMISSION, NULL,
			  &mtk_io_osd_mmcqd2_proc_fops);
	if (procEntry) {
		procEntry->gid = 1000;
	} else
		SLog_MSG("add /proc/driver/mtk_io_osd entry fail");

#endif
	/* Initialize the variable used in this module */
	writeIndex = 0;
	readIndex = 0;
	lastwriteIndex = 0;
	readMax = LCA_MAX_SHOW;
	iswrapped = false;
	enable_printk = false;
	atomic_set(&external_read_counter, 0);
#if defined(STORAGE_LOGGER_DEFAULT_ON) && defined(CONFIG_BINARY_PRINTF)
	enable_inMem = true;
	enable_mmcqd_dump = false;
	enable_blklayer_dump = false;
	enable_vfs_dump = false;
	enable_msdc_dump = false;
	enable_ProcName = true;
	storage_logger_iosched = false;
	/* Storage logger buffer pool 10MB */
	storage_logger_bufsize = 2 * 1024 * 1024;
	storage_logger_mem_pool = vmalloc(storage_logger_bufsize);

	enable_StorageLogger = true;
#else
	enable_inMem = false;
	enable_mmcqd_dump = false;
	enable_blklayer_dump = false;
	enable_vfs_dump = false;
	enable_msdc_dump = false;
	enable_ProcName = false;
	enable_StorageLogger = false;
	storage_logger_iosched = false;
	storage_logger_mem_pool = NULL;
	storage_logger_bufsize = 0;
#endif
	/*-USB-------------------*/
	enable_musb_dump = false;
	enable_usb_gadget_dump = false;
    /*-THERMAL-------------------*/
	enable_mthermal_dump = false;
	/*-----------------------*/

	/* spin_lock_init(&storage_logger_lock); */

	/* Default file name */
	snprintf(dump_filename, sizeof(dump_filename), DEFAULT_FILE_NAME);

	CREATE_PROC_ENTRY(procEntry, "driver/storage_logger_dump", ACCESS_PERMISSION, NULL,
			  &driver_dump_proc_fops);
	CREATE_PROC_ENTRY(procEntry, "driver/storage_logger_bufsize_malloc", ACCESS_PERMISSION,
			  NULL, &driver_bufsize_proc_fops);
	if (procEntry) {
		procEntry->gid = 1000;
	}

	CREATE_PROC_ENTRY(procEntry, "driver/storage_logger_filename", ACCESS_PERMISSION, NULL,
			  &driver_filename_proc_fops);
	CREATE_PROC_ENTRY(procEntry, "driver/storage_logger_status", ACCESS_PERMISSION, NULL,
			  &driver_status_proc_fops);
	CREATE_PROC_ENTRY(procEntry, "driver/storage_logger_iosched", ACCESS_PERMISSION, NULL,
			  &driver_iosched_proc_fops);

	/* Add for File system directory file check */
	CREATE_PROC_ENTRY(procEntry, "driver/fs_rectime", ACCESS_PERMISSION, NULL,
			  &driver_fs_rectime_proc_fops);
	if (!procEntry)
		SLog_MSG("add /proc/driver/fs_rectime entry fail");

	SLog_MSG("storage_logger_init success\n");
	return ret;
}

static void __exit storage_logger_exit(void)
{
#ifdef DRV_MOUDLE_READY
	platform_driver_unregister(&storage_logger_driver);
#endif
	if (NULL != storage_logger_mem_pool) {
		vfree(storage_logger_mem_pool);
		storage_logger_mem_pool = NULL;
	}
}
module_init(storage_logger_init);
module_exit(storage_logger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Storage Logger driver");

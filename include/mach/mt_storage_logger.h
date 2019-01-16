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

#ifndef __MT_STORAGE_LOGGER_H__
#define __MT_STORAGE_LOGGER_H__


#define SLTAG "[StorageLogger]"
#define SLog_MSG(fmt, args...) \
do {    \
		printk(KERN_INFO SLTAG""fmt" <- %s(): L<%d>  PID<%s><%d>\n", \
		##args , __func__, __LINE__, current->comm, current->pid); \
} while (0);

#define STrace_MSG(fmt, args...) \
do {    \
		printk(KERN_INFO SLTAG""fmt" in PID<%s><%d>\n", \
		##args, current->comm, current->pid); \
} while (0);

struct _loggerFuncName {
	int FuncID;
	char *FuncName;
};

struct _loggerMsgFormat {
	int MsgID;
	char *MsgFmt;
	char *DispFmt;
};

enum Storage_Logger_MsgID {
	STORAGE_LOGGER_MSG_FIRST_ONE = 0,
	STORAGE_LOGGER_MSG_ISSUE_RQ = STORAGE_LOGGER_MSG_FIRST_ONE,
	STORAGE_LOGGER_MSG_ISSUE_RQ_1,
	STORAGE_LOGGER_MSG_SUBMITBIO,
	STORAGE_LOGGER_MSG_VFS_SDCARD,
	STORAGE_LOGGER_MSG_VFS_SDCARD_END,
	STORAGE_LOGGER_MSG_VFS_OPEN_SDCARD,
	STORAGE_LOGGER_MSG_VFS_OPEN_SDCARD_END,
	STORAGE_LOGGER_MSG_VFS_INTFS,
	STORAGE_LOGGER_MSG_VFS_INTFS_END,
	STORAGE_LOGGER_MSG_VFS_OPEN_INTFS,
	STORAGE_LOGGER_MSG_VFS_OPEN_INTFS_END,
	STORAGE_LOGGER_MSG_IOSCHED1,
	STORAGE_LOGGER_MSG_IOSCHED2,
	STORAGE_LOGGER_MSG_MSDC_DO,
	STORAGE_LOGGER_MSG_MSDC_DO_END,
	STORAGE_LOGGER_MSG_GET_REQUEST,
	STORAGE_LOGGER_MSG_GET_REQUEST_END,
	STORAGE_LOGGER_MSG_LAST_ONE,	/* MUST BE THE LAST STORAGE MSG ID */

	USB_LOGGER_MSG_FIRST_ONE = STORAGE_LOGGER_MSG_LAST_ONE + 1,
	USB_LOGGER_MSG_MUSB_INTERRUPT = USB_LOGGER_MSG_FIRST_ONE,
	USB_LOGGER_MSG_MUSB_STAGE0_IRQ,
	USB_LOGGER_MSG_MUSB_G_EP0_IRQ,
	USB_LOGGER_MSG_TXSTATE,
	USB_LOGGER_MSG_TXSTATE_END,
	USB_LOGGER_MSG_MUSB_G_TX,
	USB_LOGGER_MSG_MUSB_G_RX,
	USB_LOGGER_MSG_MUSB_READ_SETUP,
	USB_LOGGER_MSG_FORWARD_TO_DRIVER,
	USB_LOGGER_MSG_COMPOSITE_SETUP,
	USB_LOGGER_MSG_USB_ADD_FUNCTION,
	USB_LOGGER_MSG_SET_CONFIG,
	USB_LOGGER_MSG_DEVICE_DESCRIPTOR,
	USB_LOGGER_MSG_INTERFACE_DESCRIPTOR,
	USB_LOGGER_MSG_ENDPOINT_DESCRIPTOR,
	USB_LOGGER_MSG_ANDROID_WORK,
	USB_LOGGER_MSG_GS_RX_PUSH,
	USB_LOGGER_MSG_GS_START_TX,
	USB_LOGGER_MSG_ACM_SETUP,
	USB_LOGGER_MSG_ACM_SET_ALT,
	USB_LOGGER_MSG_ACM_BIND,
	USB_LOGGER_MSG_GS_OPEN,
	USB_LOGGER_MSG_GS_CLOSE,
	USB_LOGGER_MSG_ACM_CDC_LINE_CODING,
	USB_LOGGER_MSG_STRING,
	USB_LOGGER_MSG_HEX_NUM,
	USB_LOGGER_MSG_DEC_NUM,
	USB_LOGGER_MSG_LAST_ONE,	/* MUST BE THE LAST USB MSG ID */

	THRML_LOGGER_MSG_FIRST_ONE = USB_LOGGER_MSG_LAST_ONE + 1,
	THRML_LOGGER_MSG_STRING = THRML_LOGGER_MSG_FIRST_ONE,
	THRML_LOGGER_MSG_HEX_NUM,
	THRML_LOGGER_MSG_DEC_NUM,
	THRML_LOGGER_MSG_BIND,
	THRML_LOGGER_MSG_ZONE_TEMP,
	THRML_LOGGER_MSG_COOL_STAE,
	THRML_LOGGER_MSG_TRIP_POINT,
	THRML_LOGGER_MSG_BATTERY_INFO,
	THRML_LOGGER_MSG_CPU_INFO,
	THRML_LOGGER_MSG_CPU_INFO_EX,
	THRML_LOGGER_MSG_MISC_INFO,
	THRML_LOGGER_MSG_MISC_EX_INFO,
	THRML_LOGGER_MSG_LAST_ONE,

	LOGGER_MSG_ID_MAX = THRML_LOGGER_MSG_LAST_ONE	/* THE WHOLE ENUM LAST ONE */
};

/*========STORAGE PART========*/
enum Storage_LogAPI {
	STORAGE_LOG_API___submit_bio__func = 0,
	STORAGE_LOG_API___mmc_blk_issue_rq__func,
	STORAGE_LOG_API___do_sys_open__func,
	STORAGE_LOG_API___vfs_read__func,
	STORAGE_LOG_API___vfs_write__func,
	STORAGE_LOG_API___msdc_do_request__func,
	STORAGE_LOG_API___msdc_ops_request__func,
	STORAGE_LOG_API___io_schedule__func,
	STORAGE_LOG_API___get_request_wait__func,
	STORAGE_LOG_API___get_request__func,
	STORAGE_LOG_API___make_request__func,
	STORAGE_LOG_API___MAX__func
};

/*========USB PART========*/
enum usb_func_string_index {
	USB_FUNC_STRING_INDEX_MUSB_INTERRUPT = 0,
	USB_FUNC_STRING_INDEX_MUSB_STAGE0_IRQ,
	USB_FUNC_STRING_INDEX_MUSB_G_EP0_IRQ,
	USB_FUNC_STRING_INDEX_TXSTATE,
	USB_FUNC_STRING_INDEX_MUSB_G_TX,
	USB_FUNC_STRING_INDEX_MUSB_G_RX,
	USB_FUNC_STRING_INDEX_MUSB_READ_SETUP,
	USB_FUNC_STRING_INDEX_FORWARD_TO_DRIVER,
	USB_FUNC_STRING_INDEX_COMPOSITE_SETUP,
	USB_FUNC_STRING_INDEX_USB_ADD_FUNCTION,
	USB_FUNC_STRING_INDEX_SET_CONFIG,
	USB_FUNC_STRING_INDEX_CONFIG_BUF,
	USB_FUNC_STRING_INDEX_USB_DESCRIPTOR_FILLBUF,
	USB_FUNC_STRING_INDEX_ANDROID_WORK,
	USB_FUNC_STRING_INDEX_GS_RX_PUSH,
	USB_FUNC_STRING_INDEX_GS_START_TX,
	USB_FUNC_STRING_INDEX_ACM_SETUP,
	USB_FUNC_STRING_INDEX_ACM_SET_ALT,
	USB_FUNC_STRING_INDEX_ACM_BIND,
	USB_FUNC_STRING_INDEX_GS_OPEN,
	USB_FUNC_STRING_INDEX_GS_CLOSE,
	USB_FUNC_STRING_INDEX_ACM_COMPLETE_SET_LINE,
	USB_FUNC_STRING_INDEX_ADB_READ,
	USB_FUNC_STRING_INDEX_ADB_WRITE,
	USB_FUNC_STRING_INDEX_MAX
};
/*========Thermal PART========*/
enum thermal_func_string_index {
	/* Thermal API INDEX */
	THRML_FID_bind = 0,
	THRML_FID_unbind,
	THRML_FID_get_temp,
	THRML_FID_get_mode,
	THRML_FID_set_mode,
	THRML_FID_get_trip_type,	/* 5 */
	THRML_FID_get_trip_temp,
	THRML_FID_get_crit_temp,
	/* Cooling API INDEX */
	THRML_FID_get_max_state,
	THRML_FID_get_cur_state,
	THRML_FID_set_cur_state,	/* 10 */
	/* Battery API INDEX */
	THRML_FID_get_battery_info,
	THRML_FID_get_cpu_info,
	THRML_FID_get_real_time,
	THRML_FID_get_cpu_info_ex,	/* /< JB only, ICS not support */
	THRML_FID_get_misc_info,	/* /< JB only, ICS not support */
	THRML_FID_get_misc_ex_info,	/* /< JB only, ICS not support */
	THRML_FID_MAX
};
/*========================*/

enum logger_type {
	LOGGER_TYPE_START = 0,
	LOGGER_TYPE_STORAGE = LOGGER_TYPE_START,
	LOGGER_TYPE_USB,
	LOGGER_TYPE_THRML,
	LOGGER_TYPE_END
};

#ifndef USER_BUILD_KERNEL	/* engineering mode */

#define CREATE_PROC_ENTRY(proc, x, y, z, o) proc = proc_create(x, y, z, o)

#else

#define CREATE_PROC_ENTRY(proc, x, y, z, o)

#endif

/*
 * add the api to disable the storage logger
 */
extern void storage_logger_switch(bool enabled);

extern void add_trace(enum logger_type type, unsigned int msg_id,
		      unsigned int line_cnt, unsigned int func_id, ...);

/*========STORAGE PART========*/
extern bool dumpMMCqd(void);
extern bool dumpBlkLayer(void);
extern bool dumpVFS(void);
extern bool dumpMSDC(void);
extern bool ioschedule_dump(void);
/*======= File System PART========*/
extern int dumpFsRecTime(void);

#define AddStorageTrace(msg_id, name, ...) \
	add_trace(LOGGER_TYPE_STORAGE, msg_id, __LINE__, \
		STORAGE_LOG_API___##name##__func, __VA_ARGS__);
/*========USB PART========*/
extern bool is_dump_musb(void);
extern bool is_dump_usb_gadget(void);

#define ADD_USB_TRACE(func_id, name, ...) \
	add_trace(LOGGER_TYPE_USB, USB_LOGGER_MSG_##func_id, __LINE__, \
		USB_FUNC_STRING_INDEX_##name, __VA_ARGS__);

/*======Thermal PART======*/
extern bool is_dump_mthermal(void);

#define AddThrmlTrace(msg_id, name, ...) \
	add_trace(LOGGER_TYPE_THRML, msg_id, __LINE__, \
		THRML_FID_##name, __VA_ARGS__);

/*========================*/
#endif				/* !__MT_STORAGE_LOGGER_H__ */

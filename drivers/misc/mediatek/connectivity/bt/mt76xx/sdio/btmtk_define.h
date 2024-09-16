/*
 *  Copyright (c) 2016 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __BTMTK_DEFINE_H__
#define __BTMTK_DEFINE_H__

#include "btmtk_config.h"

/**
 * Type definition
 */
#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#ifndef UNUSED
	#define UNUSED(x) (void)(x)
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/**
 * Log level definition
 */
#define BTMTK_LOG_LEVEL_ERROR		1
#define BTMTK_LOG_LEVEL_WARNING		2
#define BTMTK_LOG_LEVEL_INFO		3
#define BTMTK_LOG_LEVEL_DEBUG		4
#define BTMTK_LOG_LEVEL_MAX		BTMTK_LOG_LEVEL_DEBUG
#define BTMTK_LOG_LEVEL_DEFAULT		BTMTK_LOG_LEVEL_INFO	/* default setting */
extern u8 btmtk_log_lvl;

#define BTMTK_ERR(fmt, ...)     \
	do {if (btmtk_log_lvl >= BTMTK_LOG_LEVEL_ERROR)		\
		pr_info("[btmtk_err] %s: "fmt"\n", __func__, ##__VA_ARGS__); } while (0)
#define BTMTK_WARN(fmt, ...)    \
	do {if (btmtk_log_lvl >= BTMTK_LOG_LEVEL_WARNING)	\
		pr_info("[btmtk_warn] %s: "fmt"\n", __func__, ##__VA_ARGS__); } while (0)
#define BTMTK_INFO(fmt, ...)    \
	do {if (btmtk_log_lvl >= BTMTK_LOG_LEVEL_INFO)		\
		pr_info("[btmtk_info] %s: "fmt"\n", __func__, ##__VA_ARGS__); } while (0)
#define BTMTK_DBG(fmt, ...)     \
	do {if (btmtk_log_lvl >= BTMTK_LOG_LEVEL_DEBUG)		\
		pr_info("[btmtk_debug] %s: "fmt"\n", __func__, ##__VA_ARGS__); } while (0)

#define BTMTK_MAX_LOG_LEN		64	/* default length setting */

#define BTSDIO_INFO_RAW(p, l, fmt, ...)								\
do {												\
	if (btmtk_log_lvl >= BTMTK_LOG_LEVEL_INFO) {						\
		int raw_count = 0;								\
		char str[BTMTK_MAX_LOG_LEN * 3 + 1];						\
		char *p_str = str;								\
		const unsigned char *ptr = p;							\
		for (raw_count = 0; raw_count < MIN(l, BTMTK_MAX_LOG_LEN); ++raw_count)		\
			p_str += sprintf(p_str, " %02X", ptr[raw_count]);			\
		*p_str = '\0';									\
		pr_info("[btmtk_info]"fmt"\n", ##__VA_ARGS__);					\
		pr_info(" %s:%d - Length(%d): %s\n", __func__, __LINE__, l, str);		\
	}											\
} while (0)

#define BTSDIO_DEBUG_RAW(p, l, fmt, ...)							\
do {												\
	if (btmtk_log_lvl >= BTMTK_LOG_LEVEL_DEBUG) {						\
		int raw_count = 0;								\
		char str[BTMTK_MAX_LOG_LEN * 3 + 1];						\
		char *p_str = str;								\
		const unsigned char *ptr = p;							\
		for (raw_count = 0; raw_count < MIN(l, BTMTK_MAX_LOG_LEN); ++raw_count)		\
			p_str += sprintf(p_str, " %02X", ptr[raw_count]);			\
		*p_str = '\0';									\
		pr_info("[btmtk_debug]"fmt"\n", ##__VA_ARGS__);					\
		pr_info(" %s:%d - Length(%d): %s\n", __func__, __LINE__, l, str);		\
	}											\
} while (0)

#define MTK_HCI_WRITE_CR_PKT		0x07
#define MTK_HCI_READ_CR_PKT		0x08

#define MTK_HCI_READ_CR_PKT_LENGTH	0x05
#define MTK_HCI_WRITE_CR_PKT_LENGTH	0x09

#define MTK_HCI_CMD_HEADER_LEN	(4)
#define MTK_HCI_ACL_HEADER_LEN	(5)
#define MTK_HCI_SCO_HEADER_LEN	(4)

#define PRINT_DUMP_COUNT		20

/**
 * SYS control
 */
#define SYSCTL	0x400000

/**
 * WLAN
 */
#define WLAN	0x410000

/**
 * MCUCTL
 */
#define CLOCK_CTL	0x0708
#define INT_LEVEL	0x0718
#define COM_REG0	0x0730
#define SEMAPHORE_00	0x07B0
#define SEMAPHORE_01	0x07B4
#define SEMAPHORE_02	0x07B8
#define SEMAPHORE_03	0x07BC

/**
 * Timeout setting, mescs
 */
#define USB_CTRL_IO_TIMO	100
#define USB_INTR_MSG_TIMO	2000


/**
 * USB request type definition
 */
#define DEVICE_VENDOR_REQUEST_OUT	0x40
#define DEVICE_VENDOR_REQUEST_IN	0xc0
#define DEVICE_CLASS_REQUEST_OUT	0x20
#define DEVICE_CLASS_REQUEST_IN		0xa0

#define BTUSB_MAX_ISOC_FRAMES	10
#define BTUSB_INTR_RUNNING	0
#define BTUSB_BULK_RUNNING	1
#define BTUSB_ISOC_RUNNING	2
#define BTUSB_SUSPENDING	3
#define BTUSB_DID_ISO_RESUME	4

/**
 * ROM patch related
 */
#define PATCH_HCI_HEADER_SIZE	4
#define PATCH_WMT_HEADER_SIZE	5
#define PATCH_HEADER_SIZE	(PATCH_HCI_HEADER_SIZE + PATCH_WMT_HEADER_SIZE)
#define UPLOAD_PATCH_UNIT	2048
#define PATCH_INFO_SIZE		30
#define PATCH_PHASE1		1
#define PATCH_PHASE2		2
#define PATCH_PHASE3		3
#define PATCH_LEN_ILM		(192 * 1024)


#define USB_IO_BUF_SIZE		(HCI_MAX_EVENT_SIZE > 256 ? HCI_MAX_EVENT_SIZE : 256)
#define HCI_SNOOP_ENTRY_NUM	30
#define HCI_SNOOP_BUF_SIZE	32
#define FW_LOG_PKT		0xFF

/**
 * stpbt device node
 */
#define BUFFER_SIZE	(1024 * 4)	/* Size of RX Queue */

/**
 * fw log queue count
 */
#define FWLOG_QUEUE_COUNT 200
#define FWLOG_ASSERT_QUEUE_COUNT 10000
#define FWLOG_BLUETOOTH_KPI_QUEUE_COUNT 200

/**
 * Maximum rom patch file name length
 */
#define MAX_BIN_FILE_NAME_LEN 32


/**
 *  Firmware version size
 */
#define FW_VERSION_BUF_SIZE 32	/* 14 bytes for firmware version + 1 bytes for '0' */
#define FW_VERSION_SIZE 15	/* 14 bytes for firmware version + 1 bytes for '0' */

#endif /* __BTMTK_DEFINE_H__ */

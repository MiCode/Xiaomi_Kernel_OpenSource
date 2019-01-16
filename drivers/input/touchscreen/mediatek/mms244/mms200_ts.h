/*
 * mms_ts.h - Platform data for Melfas MMS-series touch driver
 *
 * Copyright (C) 2013 Melfas Inc.
 * Author: DVK team <dvk@melfas.com>
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef _LINUX_MMS_TOUCH_H
#define _LINUX_MMS_TOUCH_H
#include <mach/mt_typedefs.h>
struct mms_ts_platform_data {
	int	max_x;
	int	max_y;

	int	gpio_sda;
	int	gpio_scl;
	int	gpio_resetb;
	int	gpio_vdd_en;
};

/* Flag to enable touch key */
#define MMS_HAS_TOUCH_KEY	1

/*
 * ISC_XFER_LEN	- ISC unit transfer length.
 * Give number of 2 power n, where  n is between 2 and 10 
 * i.e. 4, 8, 16 ,,, 1024 
 */
#define ISC_XFER_LEN		128//1024

#define MMS_FLASH_PAGE_SZ	1024
#define ISC_BLOCK_NUM		(MMS_FLASH_PAGE_SZ / ISC_XFER_LEN)

#define FLASH_VERBOSE_DEBUG	1
#define MAX_SECTION_NUM		3

#define MAX_FINGER_NUM		5
#define FINGER_EVENT_SZ		6
#define MAX_WIDTH		30
#define MAX_PRESSURE		255
#define MAX_LOG_LENGTH		128

/* Registers */
#define MMS_MODE_CONTROL	0x01
#define MMS_TX_NUM		0x0B
#define MMS_RX_NUM		0x0C
#define MMS_EVENT_PKT_SZ	0x0F
#define MMS_INPUT_EVENT		0x10
#define MMS_UNIVERSAL_CMD	0xA0
#define MMS_UNIVERSAL_RESULT	0xAF
#define MMS_CMD_ENTER_ISC	0x5F
#define MMS_FW_VERSION		0xE1
#define MMS_CORE_VERSION		0xE2
#define MMS_CONFIG_VERSION		0xE3

#define MMS_FIRM_INFO		0xC3
#define MMS_CHIP_INFO		0xC4
#define MMS_MANUFACTURE_INFO		0xC5

/* Universal commands */
#define MMS_CMD_SET_LOG_MODE	0x20

/* Event types */
#define MMS_LOG_EVENT		0xD
#define MMS_NOTIFY_EVENT	0xE
#define MMS_ERROR_EVENT		0xF
#define MMS_TOUCH_KEY_EVENT	0x02

/* Firmware file name */
#define FW_NAME			"mms_ts.fw"

enum {
	GET_RX_NUM	= 1,
	GET_TX_NUM,
	GET_EVENT_DATA,
};

enum {
	LOG_TYPE_U08	= 2,
	LOG_TYPE_S08,
	LOG_TYPE_U16,
	LOG_TYPE_S16,
	LOG_TYPE_U32	= 8,
	LOG_TYPE_S32,
};

enum {
	ISC_ADDR		= 0xD5,

	ISC_CMD_READ_STATUS	= 0xD9,	
	ISC_CMD_READ		= 0x4000,
	ISC_CMD_EXIT		= 0x8200,
	ISC_CMD_PAGE_ERASE	= 0xC000,
	
	ISC_PAGE_ERASE_DONE	= 0x10000,
	ISC_PAGE_ERASE_ENTER	= 0x20000,
};

enum {
	EXT_INFO_ERASE		= 0x01,
	EXT_INFO_WRITE		= 0x10,
};

enum {
	ISC_DMA_W		= 1,
	ISC_DMA_R		= 2,
};

struct mms_bin_hdr {
	char	tag[8];
	U16	core_version;
	U16	section_num;
	U16	contains_full_binary;
	U16	reserved0;

	U32	binary_offset;
	U32	binary_length;

	U32	extention_offset;	
	U32	reserved1;
	
} __attribute__ ((packed));

struct mms_ext_hdr {
	U32	data_ID;
	U32	offset;
	U32	length;
	U32	next_item;
	U8	data[0];
} __attribute__ ((packed));

struct mms_fw_img {
	U16	type;
	U16	version;

	U16	start_page;
	U16	end_page;

	U32	offset;
	U32	length;

} __attribute__ ((packed));

struct isc_packet {
	U8	cmd;
	U32	addr;
	U8	data[0];
} __attribute__ ((packed));


#endif /* _LINUX_MMS_TOUCH_H */

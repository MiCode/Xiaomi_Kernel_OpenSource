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

#include <linux/cdev.h>


/* Change IIC Slave Address */
#define FW_DL_To_Change_IIC_Address 0//demo address:0x48,some project change for other; anderson

/* Firmware file name */
#define FW_NAME					"mms_ts.fw"
#define FW_CONFIG_NAME			"mms_ts_cfg.fw"

/* mfsp offset */
#define MMS_MFSP_OFFSET			16

/* Runtime config */
#define MMS_RUN_CONF_POINTER	0xA1
#define MMS_GET_RUN_CONF		0xA2
#define MMS_SET_RUN_CONF		0xA3
#define MMS_READ_BYTE			8//the size of one point info,8 or 6,anderson

/* Registers */
#define MMS_MODE_CONTROL		0x01
#define MMS_TX_NUM				0x0B
#define MMS_RX_NUM				0x0C
#define MMS_EVENT_PKT_SZ		0x0F
#define MMS_INPUT_EVENT			0x10
#define MMS_UNIVERSAL_CMD		0xA0
#define MMS_UNIVERSAL_RESULT_LENGTH	0xAE
#define MMS_UNIVERSAL_RESULT	0xAF
#define MMS_UNIV_SET_SPEC		0x30
#define MMS_UNIV_ENTER_TEST		0x40
#define MMS_UNIV_TEST_CM		0x41
#define MMS_UNIV_GET_CM			0x42
#define MMS_UNIV_EXIT_TEST		0x4F
#define MMS_UNIV_INTENSITY		0x70
#define MMS_CMD_ENTER_ISC		0x5F
#define MMS_FW_VERSION			0xE1
#define MMS_ERASE_DEFEND		0xB0

/* Universal commands */
#define MMS_CMD_SET_LOG_MODE	0x20
#define MMS_CMD_CONTROL			0x22
#define MMS_SUBCMD_START		0x80

/* Firmware Start Control */
#define RUN_START				0
#define RUN_STOP				1

#define MAX_SECTION_NUM			3
#define ISC_XFER_LEN			128
#define MMS_FLASH_PAGE_SZ		1024
#define ISC_BLOCK_NUM			(MMS_FLASH_PAGE_SZ / ISC_XFER_LEN)

// runtime config
#define MMS_RUNTIME

enum {
	MMS_RUN_TYPE_SINGLE 	= 1,
	MMS_RUN_TYPE_ARRAY,
	MMS_RUN_TYPE_END,
	MMS_RUN_TYPE_INFO,
	MMS_RUN_TYPE_UNKNOWN,
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

struct mms_ts_platform_data {
	int	max_x;
	int	max_y;

	int	gpio_sda;
	int	gpio_scl;
	int	gpio_resetb;
	int	gpio_vdd_en;
};

struct mms_config_item {
        u16	type;
        u16	category;
        u16	offset;
        u16	datasize;
	u16	data_blocksize;
        u16	reserved;

        u32     value;
} __attribute__ ((packed));

struct mms_config_hdr {
	char	mark[4];

	char	tag[4];
	
	u32	core_version;
	u32	config_version;
	u32	data_offset;
	u32	data_count;

	u32	reserved0;
	u32	info_offset;
	u32	reserved2;
	u32	reserved3;
	u32	reserved4;
	u32	reserved5;

} __attribute__ ((packed));

struct mms_ts_info {
	struct i2c_client 		*client;
	struct input_dev 		*input_dev;
	char 				phys[32];

	u8				tx_num;
	u8				rx_num;
	int				data_cmd;

	
	int 				irq;

	struct mms_ts_platform_data 	*pdata;

	char 				*fw_name;
	struct completion 		init_done;
	struct early_suspend		early_suspend;

	struct mutex 			lock;
	bool				enabled;

	struct cdev			cdev;
	dev_t				mms_dev;
	struct class			*class;

	u8				ver[3];
	int				run_count;
	
	struct mms_log_data {
		u8			*data;
		int			cmd;
	} log;
	
	char				*cm_intensity;
};

struct mms_bin_hdr {
	char	tag[8];
	u16	core_version;
	u16	section_num;
	u16	contains_full_binary;
	u16	reserved0;

	u32	binary_offset;
	u32	binary_length;

	u32	extention_offset;	
	u32	reserved1;
	
} __attribute__ ((packed));

struct mms_fw_img {
	u16	type;
	u16	version;

	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;

} __attribute__ ((packed));

struct isc_packet {
	u8	cmd;
	u32	addr;
	u8	data[0];
} __attribute__ ((packed));

extern int mms_config_start(struct mms_ts_info *info);
void mms_fw_update_controller(const struct firmware *fw, void * context);


#endif /* _LINUX_MMS_TOUCH_H */

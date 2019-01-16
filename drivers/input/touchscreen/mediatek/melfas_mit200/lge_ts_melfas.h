/* lge_touch_melfas.h
 *
 * Copyright (C) 2013 LGE.
 *
 * Author: WX-BSP-TS@lge.com
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

#include <linux/types.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>
//#include <mach/gpiomux.h>
//#include <gpiomux.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/async.h>

#include "lge_ts_core.h"


#ifndef LGE_TS_MELFAS_H
#define LGE_TS_MELFAS_H

#define FW_BLOCK_SIZE		128
#define FW_MAX_SIZE		(64 * 1024)

enum melfas_ic_type {
	MMS100S = 0,
	MMS128S = MMS100S,
	MMS134S = MMS100S,
	MMS100A = 1,
	MMS136 = MMS100A,
	MMS144 = MMS100A,
	MMS152 = MMS100A,
	MIT200 = 2,
};

enum {
	FW_UP_TO_DATE = 0,
	FW_UPDATE_BY_ISC,
	FW_UPDATE_BY_ISP,
};

enum  {
	OTP_NOT_SUPPORTED = 0,
	OTP_NONE,
	OTP_APPLIED,
};

enum  {
	GPIOMODE_ISP_START = 0,
	GPIOMODE_ISP_END,
	GPIOMODE_FX_START,
	GPIOMODE_FX_END,
};

enum {
	GPIO_SDA = 0,
	GPIO_SCL,
	GPIO_INT
};

enum {
	FAIL_MULTI_TOUCH = 1,
	FAIL_TOUCH_SLOP,
	FAIL_TAP_DISTANCE,
	FAIL_TAP_TIME,
	FAIL_TOTAL_COUNT,
	FAIL_DELAY_TIME,
	FAIL_PALM,
	FAIL_ACTIVE_AREA
};

#define FINGER_EVENT_SZ		6
#define MIT_FINGER_EVENT_SZ		8
#define MIT_LPWG_EVENT_SZ		3
#define MAX_WIDTH		30
#define MAX_PRESSURE		255
#define MAX_LOG_LENGTH		128

#define SECTION_NUM		3
#define PAGE_HEADER		3
#define PAGE_DATA		1024
#define PAGE_CRC		2
#define PACKET_SIZE		(PAGE_HEADER + PAGE_DATA + PAGE_CRC)

#define KNOCKON_DELAY   700

#define MAX_COL	15
#define MAX_ROW	25
#define MIT_ROW_NUM             0x0B
#define MIT_COL_NUM             0x0C

#define MIT_EVENT_PKT_SZ		0x0F
#define MIT_INPUT_EVENT			0x10

/* MIT 200 Registers */

#define MIT_REGH_CMD				0x10

#define MIT_REGL_MODE_CONTROL			0x01
#define MIT_REGL_ROW_NUM			0x0B
#define MIT_REGL_COL_NUM			0x0C
#define MIT_REGL_RAW_TRACK			0x0E
#define MIT_REGL_EVENT_PKT_SZ			0x0F
#define MIT_REGL_INPUT_EVENT			0x10
#define MIT_REGL_UCMD				0xA0
#define MIT_REGL_UCMD_RESULT_LENGTH		0xAE
#define MIT_REGL_UCMD_RESULT			0xAF
#define MIT_FW_VERSION				0xC2
#define MIT_FW_PRODUCT		0xE0

/* MIT 200 LPWG Registers */
#define MIT_LPWG_IDLE_REPORTRATE_REG     0x60
#define MIT_LPWG_ACTIVE_REPORTRATE_REG   0x61
#define MIT_LPWG_SENSITIVITY_REG         0x62
#define MIT_LPWG_ACTIVE_AREA_REG         0x63

#define MIT_LPWG_TCI_ENABLE_REG          0x70
#define MIT_LPWG_TOUCH_SLOP_REG          0x71
#define MIT_LPWG_TAP_MIN_DISTANCE_REG    0x72
#define MIT_LPWG_TAP_MAX_DISTANCE_REG    0x73
#define MIT_LPWG_MIN_INTERTAP_REG        0x74
#define MIT_LPWG_MAX_INTERTAP_REG        0x76
#define MIT_LPWG_TAP_COUNT_REG           0x78
#define MIT_LPWG_INTERRUPT_DELAY_REG     0x79

#define MIT_LPWG_TCI_ENABLE_REG2         0x80
#define MIT_LPWG_TOUCH_SLOP_REG2         0x81
#define MIT_LPWG_TAP_MIN_DISTANCE_REG2   0x82
#define MIT_LPWG_TAP_MAX_DISTANCE_REG2   0x83
#define MIT_LPWG_MIN_INTERTAP_REG2       0x84
#define MIT_LPWG_MAX_INTERTAP_REG2       0x86
#define MIT_LPWG_TAP_COUNT_REG2          0x88
#define MIT_LPWG_INTERRUPT_DELAY_REG2    0x89

#define MIT_LPWG_STORE_INFO_REG          0x8F
#define MIT_LPWG_START_REG               0x90
#define MIT_LPWG_PANEL_DEBUG_REG         0x91
#define MIT_LPWG_FAIL_REASON_REG         0x92

/* Universal commands */
#define MIT_UNIV_ENTER_TESTMODE			0x40
#define MIT_UNIV_TESTA_START			0x41
#define MIT_UNIV_GET_RAWDATA			0x44
#define MIT_UNIV_TESTB_START			0x48
#define MIT_UNIV_GET_OPENSHORT_TEST		0x50
#define MIT_UNIV_EXIT_TESTMODE			0x6F
#define MIT_UNIV_GET_READ_OTP_STATUS	0x77
#define MIT_UNIV_SEND_THERMAL_INFO		0x58

#define MMS_CMD_SET_LOG_MODE	0x20

/* Event types */
#define MIT_LOG_EVENT		0xD
#define MIT_LPWG_EVENT	0xE
#define MIT_ERROR_EVENT		0xF
#define MIT_TOUCH_KEY_EVENT	0x40
#define MIT_REQUEST_THERMAL_INFO		0xB
#define MIT_ERRORCODE_FAIL_REASON	0x14

#define CRACK_SPEC	0

enum {
	LOG_TYPE_U08	= 2,
	LOG_TYPE_S08,
	LOG_TYPE_U16,
	LOG_TYPE_S16,
	LOG_TYPE_U32	= 8,
	LOG_TYPE_S32,
};

enum {
	RAW_DATA_SHOW	= 0,
	RAW_DATA_STORE,
	OPENSHORT,
	OPENSHORT_STORE,
	SLOPE,
	CRACK_CHECK,
};

enum {
	SD_RAWDATA = 0,
	SD_OPENSHORT,
	SD_SLOPE,
};

struct mms_dev {
	u16 x_resolution;
	u16 y_resolution;
	u8 contact_on_event_thres;
	u8 moving_event_thres;
	u8 active_report_rate;
	u8 operation_mode;
	u8 tx_ch_num;
	u8 rx_ch_num;
	u8 row_num;
	u8 col_num;
	u8 key_num;
};

struct mms_section {
	u8 version;
	u8 compatible_version;
	u8 start_addr;
	u8 end_addr;
	int offset;
	u32 crc;
};

struct mms_module {
	u8 product_code[16];
	u8 version[2];
	u8 otp;
};

struct mms_log {
	u8 *data;
	int cmd;
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

struct mms_data {
	bool probed;

	struct i2c_client *client;
	struct touch_platform_data *pdata;
	struct regulator *vdd_regulator[TOUCH_PWR_NUM];
	struct lpwg_tci_data *lpwg_data;
	struct mms_dev dev;
	bool need_update[SECTION_NUM];
	struct mms_section ts_section[SECTION_NUM];
	struct mms_bin_hdr *fw_hdr;
	struct mms_fw_img* fw_img[SECTION_NUM];
	struct mms_module module;
	char buf[PACKET_SIZE];
	struct mms_log log;
	uint16_t *mit_data[MAX_ROW];
	s16 *intensity_data[MAX_ROW];
	u8 test_mode;
	int count_short;
	int thermal_info_send_block;
	int r_max;
	int r_min;
	int o_max;
	int o_min;
	int s_max;
	int s_min;
};

struct mms_log_pkt {
	u8	marker;
	u8	log_info;
	u8	code;
	u8	element_sz;
	u8	row_sz;
} __attribute__ ((packed));

#ifdef USE_DMA
#define mms_i2c_write_block(client, buf, len) i2c_dma_write(client, buf, len)
#else
#define mms_i2c_write_block(client, buf, len) i2c_master_send(client, buf, len)
#endif
extern atomic_t dev_state;
#if defined(TOUCH_USE_DSV)
extern void mdss_dsv_ctl(int mdss_dsv_en);
#endif

int mms_i2c_read(struct i2c_client *client, u8 reg, char *buf, int len);
int mit_isc_fwupdate(struct mms_data *ts, struct touch_fw_info *info);
int mms_set_gpio_mode(struct touch_platform_data *pdata, int mode);
int mms_power_ctrl(struct i2c_client* client, int power_ctrl);
int mms_power_reset(struct mms_data *ts);
ssize_t mit_get_test_result(struct i2c_client *client, char *buf, int type);
ssize_t mit_delta_show(struct i2c_client *client, char *buf);
//ssize_t mit_rawdata_show(struct i2c_client *client,char *buf);
//ssize_t mit_openshort_show(struct i2c_client *client, char *buf);
int mit_isc_page_read(struct mms_data *ts, u8 *rdata, int addr);
int mit_isc_exit(struct mms_data *ts);
#endif // LGE_TS_MELFAS_H


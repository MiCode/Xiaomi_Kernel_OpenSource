/*******************************************************************************
 * Copyright (C) 2015 Maxim Integrated Products, Inc., All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 *******************************************************************************
 *
 *  DS28E16.c - DS28E16 device module. Requires low level 1-Wire connection.
 */
#define pr_fmt(fmt)	"[ds28e16] %s: " fmt, __func__

#include <linux/slab.h> /* kfree() */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/string.h>
#include "sha384_software.h"
#include "ds28e16.h"
#include "onewire_gpio.h"

#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/random.h>

#ifdef CONFIG_TARGET_PROJECT_J20C
#define ds_info	pr_err
#define ds_dbg	pr_err
#define ds_err	pr_err
#define ds_log	pr_err
#else
#define ds_info	pr_debug
#define ds_dbg	pr_debug
#define ds_err	pr_err
#define ds_log	pr_debug
#endif


struct ds28e16_data {
	struct platform_device *pdev;
	struct device *dev;

	int version;

	struct power_supply *verify_psy;
	struct power_supply_desc verify_psy_d;

	struct delayed_work	authentic_work;
};
unsigned int attr_trytimes = 1;
unsigned char session_seed[32];
unsigned char S_secret[32];

void get_random_number(void)
{
	/*S-Secret Seeds for Generating S-Secret is as follows:*/
	unsigned char session_seeds[10][32]= {
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x00,0x0F,0x1D,0x17,0x30,0x19,0x22,0x04,
		0x14,0x56,0x00,0x4B,0x5C,0x21,0x36,0x08},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x38,0x45,0x5B,0x53,0x02,0x36,0x5B,0x2B,
		0x43,0x32,0x33,0x2E,0x23,0x28,0x00,0x05},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x57,0x05,0x5E,0x24,0x34,0x4C,0x05,0x3B,
		0x2E,0x1D,0x3E,0x40,0x00,0x1B,0x52,0x52},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x18,0x61,0x06,0x27,0x24,0x30,0x0F,0x2F,
		0x19,0x3E,0x36,0x0F,0x5D,0x41,0x32,0x27},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x00,0x62,0x5B,0x16,0x45,0x62,0x18,0x35,
		0x0A,0x63,0x43,0x01,0x39,0x0A,0x0A,0x4F},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x0A,0x4E,0x2D,0x4B,0x3B,0x53,0x01,0x15,
		0x07,0x0A,0x21,0x0C,0x00,0x35,0x41,0x36},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x1C,0x04,0x1D,0x26,0x1E,0x5E,0x61,0x28,
		0x1B,0x10,0x10,0x40,0x29,0x29,0x47,0x20},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x52,0x08,0x13,0x43,0x2D,0x23,0x0E,0x46,
		0x5C,0x35,0x08,0x4B,0x28,0x2E,0x31,0x14},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x3F,0x14,0x12,0x00,0x08,0x2D,0x00,0x00,
		0x4E,0x25,0x1C,0x5B,0x3F,0x3E,0x00,0x09},
		{
		0x31,0x42,0x4E,0x35,0x39,0x4B,0x37,0x41,
		0x31,0x30,0x3C,0x07,0x2D,0x36,0x28,0x00,
		0x20,0x09,0x00,0x10,0x5C,0x09,0x2C,0x1B,
		0x57,0x4B,0x1B,0x43,0x19,0x08,0x03,0x20}
	};
	/*The responding S-Secret is as follows:*/
	unsigned char S_secrets[10][32] = {
		{
		0xE7,0xC4,0xF6,0x35,0x02,0x69,0xFF,0x47,
		0xD7,0x26,0x9F,0xEE,0x8A,0x44,0xFE,0x43,
		0xB5,0xBF,0x55,0x27,0x1C,0x38,0x85,0xEE,
		0xB9,0xEA,0x86,0x0B,0xB8,0x7D,0x7E,0xB6},
		{
		0x1D,0x51,0x9E,0x41,0xC7,0x13,0x1E,0xC1,
		0x93,0x5A,0xFB,0xAC,0x59,0x31,0xA9,0xC3,
		0xB0,0x4C,0x66,0xEF,0x5A,0xF8,0xE2,0xAE,
		0xAF,0xAF,0x0D,0xD6,0x7C,0xF8,0xE5,0x06},
		{
		0x02,0xD6,0x8C,0x74,0xD6,0xAD,0x15,0xD6,
		0x64,0x41,0xA3,0x3D,0x17,0x5D,0xDB,0xE6,
		0xC9,0x3D,0x4A,0x99,0x27,0x17,0xB6,0xC5,
		0x6E,0x35,0x68,0xA7,0xFD,0xF5,0xBF,0xCE},
		{
		0xED,0x53,0x37,0xE8,0x99,0x4F,0xD7,0x07,
		0x98,0xAE,0x28,0xE6,0xFF,0x54,0x30,0x03,
		0xBD,0x6D,0xB5,0x95,0xB6,0xC0,0x04,0x0C,
		0xDC,0x0C,0x37,0xB3,0xEB,0x2E,0x51,0xC4},
		{
		0x22,0x63,0xFC,0xB7,0x81,0x88,0x2E,0x14,
		0xCB,0x0A,0x86,0xC1,0x3F,0x58,0x80,0x0C,
		0x2A,0xA3,0x5B,0xF6,0x37,0xB1,0xC2,0xAE,
		0xEC,0x01,0x7F,0x81,0x73,0x5B,0xDE,0x66},
		{
		0x23,0xF4,0xE1,0x16,0xDD,0x2C,0xFD,0x78,
		0xB9,0x89,0xF9,0x5B,0x16,0x4E,0xAF,0x26,
		0xEA,0x0C,0xC2,0x18,0x85,0x01,0x9A,0x48,
		0x01,0x70,0x41,0xCE,0xE3,0x62,0x4E,0x94},
		{
		0x43,0x7E,0xE8,0xBA,0x74,0xAB,0xA5,0x87,
		0xC1,0x3C,0x42,0x6D,0x45,0x9F,0xC5,0x41,
		0xA8,0x95,0xD6,0x59,0x0D,0x6D,0x4F,0xED,
		0xC7,0x51,0xCA,0xAF,0x72,0x35,0x7F,0x70},
		{
		0x75,0x24,0x2B,0xDC,0x7D,0xFA,0xD1,0x29,
		0x85,0x66,0x12,0xA8,0xBA,0x00,0x21,0xE1,
		0xCA,0x87,0x73,0x8F,0x30,0xD7,0xF1,0x12,
		0x4C,0xE8,0xAC,0x65,0x1C,0x91,0xB0,0x49},
		{
		0x58,0x97,0x6D,0xBD,0x5E,0x25,0x60,0x90,
		0xA7,0x6E,0xC8,0x19,0xF3,0x14,0xEE,0x56,
		0xBA,0x03,0x07,0x79,0x36,0x1F,0x7D,0xFC,
		0xE9,0x82,0xBE,0x0A,0xE1,0xD7,0x3E,0x6F},
		{
		0x1F,0x7E,0x11,0xAE,0x02,0xBA,0x31,0x27,
		0x31,0xEC,0x2A,0xC6,0xA6,0x8D,0xFD,0x7B,
		0xFB,0xAE,0x84,0x32,0x49,0xC8,0xCD,0x45,
		0x6C,0x1A,0x5F,0x8F,0x58,0x31,0xD1,0x8C}
	};

	int i;
	unsigned int randnum;
	int seed_result;

	get_random_bytes(&randnum, sizeof(randnum));
	seed_result = randnum % 10;
	if(seed_result < 0){
		pr_err("session_seed match error!\n");
		/*We will use session_seeds[0] as default*/
		seed_result = 0;
	}
	pr_err("randnum = %u,seed_result = %d\n",randnum,seed_result);

	for(i = 0; i < 32; i++){
		session_seed[i] = session_seeds[seed_result][i];
		S_secret[i] = S_secrets[seed_result][i];
	}
}

unsigned char challenge[32] = {0x00};
int auth_ANON = 1;
int auth_BDCONST = 1;
int pagenumber = 1;

// maxim define
int tm = 1;
unsigned short CRC16;
const short oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };
unsigned char last_result_byte = RESULT_SUCCESS;

unsigned char MANID[2] = {0x00};

// mi add
unsigned char flag_mi_romid = 0;
unsigned char flag_mi_status = 0;
unsigned char flag_mi_page0_data = 0;
unsigned char flag_mi_page1_data = 0;
unsigned char flag_mi_counter = 0;
unsigned char flag_mi_auth_result = 0;
unsigned char mi_romid[8] = {0x00};
unsigned char mi_status[8] = {0x00};
unsigned char mi_page0_data[16] = {0x00};
unsigned char mi_page1_data[16] = {0x00};
unsigned char mi_counter[16] = {0x00};
int mi_auth_result = 0x00;


unsigned char crc_low_first(unsigned char *ptr, unsigned char len)
{
	unsigned char i;
	unsigned char crc = 0x00;

	while (len--) {
		crc ^= *ptr++;
		for (i = 0; i < 8; ++i) {
			if (crc & 0x01)
				crc = (crc >> 1) ^ 0x8c;
			else
				crc = (crc >> 1);
		}
	}

	return (crc);
}

short Read_RomID(unsigned char *RomID)
{
	unsigned char i;
	unsigned char crc = 0x00;

	/*ds_dbg("getian--RomID = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	RomID[0], RomID[1], RomID[2], RomID[3],
	RomID[4], RomID[5], RomID[6], RomID[7]);
	ds_dbg("getian--mi_romid = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	mi_romid[0], mi_romid[1], mi_romid[2], mi_romid[3],
	mi_romid[4], mi_romid[5], mi_romid[6], mi_romid[7]);
	*/
	if (flag_mi_romid == 2) {
		memcpy(RomID, mi_romid, 8);
		ds_log("getian---Read_RomID00\n");
		return DS_TRUE;
	}

	if ((ow_reset()) != 0) {
		ds_err("Read_RomID: Failed to reset ds28e16!\n");
		ow_reset();
		return ERROR_NO_DEVICE;
	}

	//ds_dbg("Ready to write 0x33 to maxim IC!\n");
	write_byte(CMD_READ_ROM);
	Delay_us(10);
	for (i = 0; i < 8; i++)
		RomID[i] = read_byte();

	ds_dbg("RomID = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	RomID[0], RomID[1], RomID[2], RomID[3],
	RomID[4], RomID[5], RomID[6], RomID[7]);

	crc = crc_low_first(RomID, 7);
	ds_dbg("crc_low_first = %02x\n", crc);

	if (crc == RomID[7]) {
		if (flag_mi_status == 0)
			flag_mi_romid = 1;
		else
			flag_mi_romid = 2;
		memcpy(mi_romid, RomID, 8);

		ds_dbg("getian--mi_romid = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
		mi_romid[0], mi_romid[1], mi_romid[2], mi_romid[3],
		mi_romid[4], mi_romid[5], mi_romid[6], mi_romid[7]);
		ds_log("getian---Read_RomID22\n");
		return DS_TRUE;
	} else {
		ow_reset();
		ds_log("getian---Read_RomID33\n");
		return DS_FALSE;
	}
}

unsigned short docrc16(unsigned short data)
{
	data = (data ^ (CRC16 & 0xff)) & 0xff;
	CRC16 >>= 8;

	if (oddparity[data & 0xf] ^ oddparity[data >> 4])
		CRC16 ^= 0xc001;

	data <<= 6;
	CRC16  ^= data;
	data <<= 1;
	CRC16   ^= data;

	return CRC16;
}

int DS28E16_standard_cmd_flow(unsigned char *write_buf, int delay_ms,
unsigned char *read_buf, int *read_len, int write_len)
{
	unsigned char buf[128];
	int i;
	int buf_len = 0;
	int expected_read_len = *read_len;

	//NEW FLOW
	/*'1 Wire
	'''''''''''''''''''''''
	?<Reset/Presence>
	?<ROM level command Sequence>
	?TX: Start Command
	?TX: Length Byte
	?TX: Memory Command
	?TX: Parameter, TX: Data
	?RX: CRC16
	?TX: Release Byte
	?<Strong pull-up Delay>
	?RX: Dummy Byte
	?RX: Length Byte
	?RX: Result byte
	?RX: CRC16
	?< wait for reset>
	'''''''''''''''''''''''*/
	ds_dbg("DS28E16_standard_cmd_flow start\n");

	if ((ow_reset()) != 0) {
		ds_err("Failed to reset ds28e16!\n");
		ow_reset();
		return ERROR_NO_DEVICE;
	}

	write_byte(CMD_SKIP_ROM);

	buf[buf_len++] = CMD_START;
	memcpy(&buf[buf_len], write_buf, write_len);
	buf_len += write_len;
	for (i = 0; i < buf_len; i++)
		write_byte(buf[i]);

	buf[buf_len++] = read_byte();
	buf[buf_len++] = read_byte();

	CRC16 = 0;
	for (i = 0; i < buf_len; i++)
		docrc16(buf[i]);

	if (CRC16 != 0xB001) {
		ow_reset();
		ds_dbg("DS28E16_standard_cmd_flow: crc error!\n");
		return DS_FALSE;
	}

	// check for strong pull-up
	if (delay_ms > 0) {
		write_byte(CMD_RELEASE_BYTE);
		Delay_us(1000*delay_ms);
	}

	read_byte();
	*read_len = read_byte();

	if (expected_read_len != *read_len) {
		ow_reset();
		ds_dbg("DS28E16_standard_cmd_flow: len error!\n");
		return DS_FALSE;
	}

	buf_len = *read_len + 2;
	for (i = 0; i < buf_len; i++)
		buf[i] = read_byte();

	CRC16 = 0;
	docrc16(*read_len);
	for (i = 0; i < buf_len; i++)
		docrc16(buf[i]);

	if (CRC16 != 0xB001) {
		ow_reset();
		ds_dbg("DS28E16_standard_cmd_flow: crc2 error!\n");
		return DS_FALSE;
	}

	memcpy(read_buf, buf, *read_len);
	ds_dbg("DS28E16_standard_cmd_flow success!\n");
	return DS_TRUE;
}

//--------------------------------------------------------------------------
/// 'Read Status' command
///
/// @param[out] data
/// pointer to unsigned char (buffer of length 1) for page protection data
///
///  @return
///  DS_TRUE - command successful @n
///  DS_FALSE - command failed
///
int DS28E16_cmd_readStatus(unsigned char *data)
{
	unsigned char write_buf[255];
	unsigned char read_buf[255];
	int write_len = 0;
	int len_byte = 1;
	int read_len = 7;

	ds_dbg("getian--data = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	data[0], data[1], data[2], data[3],
	data[4], data[5], data[6], data[7]);
	ds_dbg("getian--mi_status = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	mi_status[0], mi_status[1], mi_status[2], mi_status[3],
	mi_status[4], mi_status[5], mi_status[6], mi_status[7]);
	if (flag_mi_status) {
		memcpy(data, mi_status, 8);
		ds_log("getian---DS28E16_cmd_readStatus00\n");
		return DS_TRUE;
	}

	last_result_byte = RESULT_FAIL_NONE;
	/*
	?<Start, device address write>
	?TX: Read Page Protection command
	?TX: Length (SMBus) [always 1]
	?TX: Parameter
	?<Stop>
	?<Delay>
	?<Start, device address read>
	?RX: Length (SMBus) [always 1]
	?RX: Protection Data
	?<Stop>
	*/

	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_READ_STATUS;

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_READ*tm,
		read_buf, &read_len, write_len)) {
		if (read_buf[0] == RESULT_SUCCESS) {
			last_result_byte = read_buf[0];
			memcpy(data, &read_buf[1], 8);
			flag_mi_status = 1;
			memcpy(mi_status, data, 8);
			MANID[0] = data[4];
			ds_dbg("getian--data = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]);
			ds_dbg("getian--mi_status = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
			mi_status[0], mi_status[1], mi_status[2], mi_status[3],
			mi_status[4], mi_status[5], mi_status[6], mi_status[7]);
			ds_log("getian---DS28E16_cmd_readStatus11\n");
			return DS_TRUE;
		}
	}

	ow_reset();
	ds_log("getian---DS28E16_cmd_readStatus22\n");
	return DS_FALSE;
}

void DS28E16_cmd_romid_pre(void)
{
	unsigned char write_buf[255];
	int write_len = 0;
	int len_byte = 1;
	int i;

	ow_reset();
	write_byte(CMD_SKIP_ROM);

	write_buf[write_len++] = CMD_START;
	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_READ_STATUS;
	for (i = 0; i < write_len; i++)
		write_byte(write_buf[i]);

	for (i = 0; i < 2; i++)
		read_byte();

	write_byte(CMD_RELEASE_BYTE);
		Delay_us(1000*DELAY_DS28E16_EE_READ*tm);

	//discard 11 bytes to get romid
	for (i = 0; i < 11; i++)
		read_byte();
	ow_reset();

	ds_log("DS28E16_cmd_romid_pre done\n");
}


//--------------------------------------------------------------------------
/// 'Read Memory' command
///
/// @param[in] pg
/// page number to read
/// @param[out] data
/// buffer length must be at least 32 bytes to hold memory read
///
///  @return
///  DS_TRUE - command successful @n
///  DS_FALSE - command failed
///
int DS28E16_cmd_readMemory(int pg, unsigned char *data)
{
	unsigned char write_buf[3];
	unsigned char read_buf[255];
	int write_len = 0;
	int read_len = 33;
	int length_byte = 2;
	unsigned char pagenum = (unsigned char)pg & 0x03;

	switch (pagenum) {
	case 0x00:
		if (flag_mi_page0_data) {
			memcpy(data, mi_page0_data, 16);
			return DS_TRUE;
		}
		break;
	case 0x01:
		if (flag_mi_page1_data) {
			memcpy(data, mi_page1_data, 16);
			return DS_TRUE;
		}
		break;
	case 0x02:
		if (flag_mi_counter) {
			memcpy(data, mi_counter, 16);
			return DS_TRUE;
		}
		break;
	default:
		return DS_FALSE;
	}

	last_result_byte = RESULT_FAIL_NONE;

	/*
	?<Start, device address write>
	?TX: Read Memory Command
	?TX: Length (SMBus)
	?TX: Parameter
	?<Stop>
	?<Delay>
	?<Start, device address read>
	?RX: Length (SMBus)
	?RX: Result byte
	?RX: Data
	?<Stop>
	*/

	write_buf[write_len++] = length_byte;
	write_buf[write_len++] = CMD_READ_MEM;
	write_buf[write_len++] = pagenum;

	ds_dbg("DS28E16_cmd_readMemory write_buf:\n");
	ds_dbg("%02x %02x %02x", write_buf[0], write_buf[1], write_buf[2]);

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_READ*tm,
		read_buf, &read_len, write_len)) {
		if (read_len == 33) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS) {
				memcpy(data, &read_buf[1], 16);
				if (pagenum == 0x00) {
					flag_mi_page0_data = 1;
					memcpy(mi_page0_data, data, 16);
				}
				if (pagenum == 0x01) {
					flag_mi_page1_data = 1;
					memcpy(mi_page1_data, data, 16);
				}
				return DS_TRUE;
			} else {
				if (read_buf[0] == 0x55 && pagenum == 2) {
					memcpy(data, &read_buf[1], 16);
					flag_mi_counter = 1;
					memcpy(mi_counter, data, 16);
					return DS_TRUE;
				}
			}
		}
	}
	ow_reset();
	return DS_FALSE;
}

//--------------------------------------------------------------------------
/// 'Write Memory' command
///
/// @param[in] pg
/// page number to write
/// @param[in] data
/// buffer must be at least 32 bytes
///
///  @return
///  DS_TRUE - command successful @n
///  DS_FALSE - command failed
///
int DS28E16_cmd_writeMemory(int pg, unsigned char *data)
{
	unsigned char write_buf[255];
	unsigned char read_buf[255];
	int write_len = 0;
	int read_len = 1;
	int len_byte = 18;
	unsigned char pagenum = (unsigned char)pg & 0x03;

	last_result_byte = RESULT_FAIL_NONE;
	/*
	?<Start, device address write>
	?TX: Write Memory Command
	?TX: Length (SMBus) [always 33]
	?TX: Parameter
	?TX: Data
	?<Stop>
	?<Delay>
	?<Start, device address read>
	?RX: Length (SMBus) [always 1]
	?RX: Result byte
	?<Stop>
	*/

	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_WRITE_MEM;
	write_buf[write_len++] = pagenum;
	memcpy(&write_buf[write_len], data, 16);
	write_len += 16;

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE*tm,
		read_buf, &read_len, write_len)) {
		if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS) {
				if (pagenum == 0x00) {
					flag_mi_page0_data = 0;
					memset(mi_page0_data, 0x00, 16);
				}
				if (pagenum == 0x01) {
					flag_mi_page1_data = 0;
					memset(mi_page1_data, 0x00, 16);
				}
				if (pagenum == 0x02) {
					flag_mi_counter = 0;
					memset(mi_counter, 0x00, 16);
				}
				return DS_TRUE;
			}
		}
	}

	ow_reset();
	return DS_FALSE;
}

//--------------------------------------------------------------------------
/// 'Decrement Counter' command
///
///  @return
///  DS_TRUE - command successful @n
///  DS_FALSE - command failed
///
int DS28E16_cmd_decrementCounter(void)
{
	unsigned char write_buf[255];
	unsigned char read_buf[255];
	int write_len = 0;
	int read_len = 1;

	last_result_byte = RESULT_FAIL_NONE;
	/*
	?<Start, device address write>
	?TX: Decrement Counter Command
	?<Stop>
	?<Delay>
	?<Start, device address read>
	?RX: Length (SMBus) [always 1]
	?RX: Result byte
	?<Stop>
	*/

	write_buf[write_len++] = 1;
	write_buf[write_len++] = CMD_DECREMENT_CNT;

	if (DS28E16_standard_cmd_flow(write_buf, 50, read_buf, &read_len, write_len)) {
		if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS)
				return DS_TRUE;
		}
	}

	ow_reset();
	return DS_FALSE;
}

//--------------------------------------------------------------------------
/// 'Set Page Protection' command
///
/// @param[in] pg
/// block to set protection
/// @param[in] prot
/// protection value to set
///
///  @return
///  DS_TRUE - command successful @n
///  DS_FALSE - command failed
///
int DS28E16_cmd_setPageProtection(int page, unsigned char prot)
{
	unsigned char write_buf[255];
	unsigned char read_buf[255];
	int write_len = 0;
	int read_len = 1;
	int len_byte = 3;

	last_result_byte = RESULT_FAIL_NONE;
	/*
	?<Start, device address write>
	?TX: Set Page Protection Command
	?TX: Length (SMBus) [always 2]
	?TX: Parameter
	?TX: Protection Data
	?<Stop>
	?<Delay>
	?<Start, device address read>
	?RX: Length (SMBus) [always 1]
	?RX: Result byte
	?<Stop>
	*/

	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_SET_PAGE_PROT;
	write_buf[write_len++] = page & 0x03;
	write_buf[write_len++] = prot & 0x03;

	if (DS28E16_standard_cmd_flow(write_buf,
	DELAY_DS28E16_EE_WRITE*tm, read_buf, &read_len, write_len)) {
		if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS)
				return DS_TRUE;
		}
	}

	ow_reset();
	return DS_FALSE;
}

int DS28E16_cmd_device_disable(int op, unsigned char *password)
{
	unsigned char write_buf[64];
	unsigned char read_buf[64];
	int write_len = 0;
	int read_len = 1;
	int length_byte = 10;

	last_result_byte = RESULT_FAIL_NONE;
	/*
	?<Start, device address write>
	?TX: Length
	?TX: Device Disable Command
	?TX: Parameter
	?<Stop>
	?<Delay>
	?<Start, device address read>
	?RX: Length (SMBus)
	?RX: Result byte
	?RX: Data
	?<Stop>
	*/

	write_buf[write_len++] = length_byte;
	write_buf[write_len++] = CMD_DISABLE_DEVICE;
	write_buf[write_len++] = op & 0x0F;
	memcpy(&write_buf[write_len], password, 8);
	write_len += 8;

	if (DS28E16_standard_cmd_flow(write_buf,
	DELAY_DS28E16_EE_WRITE, read_buf, &read_len, write_len)) {
		if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS)
				return DS_TRUE;
		}
	}

	ow_reset();

	return DS_FALSE;
}

// --------------------------------------------------------------------------
/// 'Compute and Read Page Authentication' command
///
/// @param[in] anon - boolean parameter
/// @param[in] pg - Page number   2,counter; 0,page0; 1,page1;
/// @param[in] challenge
/// @param[out] hmac    The calculated result returned is 32 bytes
/// @return
/// DS_TRUE - command successful @n
/// DS_FALSE - command failed
///
int DS28E16_cmd_computeReadPageAuthentication(int anon, int pg,
unsigned char *challenge, unsigned char *hmac)
{
	unsigned char write_buf[255];
	unsigned char read_buf[255];
	int write_len = 0;
	int read_len = 33;
	int len_byte = 35;

	last_result_byte = RESULT_FAIL_NONE;

	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_COMP_READ_AUTH;
	write_buf[write_len] = pg & 0x03;
	if (anon)
		write_buf[write_len] = (write_buf[write_len] | 0xE0);

	write_len++;
	write_buf[write_len++] = 0x02;// Fixed Parameter
	memcpy(&write_buf[write_len], challenge, 32);// Challenge
	write_len += 32;

	ds_dbg("computeReadPageAuthen:\n");
	/*ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			write_buf[0], write_buf[1], write_buf[2], write_buf[3],
			write_buf[4], write_buf[5], write_buf[6], write_buf[7],
			write_buf[8], write_buf[9], write_buf[10], write_buf[11],
			write_buf[12], write_buf[13], write_buf[14], write_buf[15],
			write_buf[16], write_buf[17], write_buf[18], write_buf[19],
			write_buf[20], write_buf[21], write_buf[22], write_buf[23],
			write_buf[24], write_buf[25], write_buf[26], write_buf[27],
			write_buf[28], write_buf[29], write_buf[30], write_buf[31],
			write_buf[32], write_buf[33], write_buf[34]);
	*/

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE, read_buf, &read_len, write_len)) {
		last_result_byte = read_buf[0];
		if (read_buf[0] == RESULT_SUCCESS) {
			/*ds_dbg("hmac read_buf:\n");
			ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			read_buf[0], read_buf[1], read_buf[2], read_buf[3],
			read_buf[4], read_buf[5], read_buf[6], read_buf[7],
			read_buf[8], read_buf[9], read_buf[10], read_buf[11],
			read_buf[12], read_buf[13], read_buf[14], read_buf[15],
			read_buf[16], read_buf[17], read_buf[18], read_buf[19],
			read_buf[20], read_buf[21], read_buf[22], read_buf[23],
			read_buf[24], read_buf[25], read_buf[26], read_buf[27],
			read_buf[28], read_buf[29], read_buf[30], read_buf[31], read_buf[32]);
			ds_dbg("hmac:\n");
			ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			hmac[0], hmac[1], hmac[2], hmac[3],
			hmac[4], hmac[5], hmac[6], hmac[7],
			hmac[8], hmac[9], hmac[10], hmac[11],
			hmac[12], hmac[13], hmac[14], hmac[15],
			hmac[16], hmac[17], hmac[18], hmac[19],
			hmac[20], hmac[21], hmac[22], hmac[23],
			hmac[24], hmac[25], hmac[26], hmac[27],
			hmac[28], hmac[29], hmac[30], hmac[31]);
			*/
			memcpy(hmac, &read_buf[1], 32);

			return DS_TRUE;
		}
	}

	ow_reset();

	return DS_FALSE;
}

// --------------------------------------------------------------------------
/// 'Compute Secret S' command
///
/// @param[in] anon - boolean parameter
/// @param[in] bdconst - boolean parameter
/// @param[in] pg - Page number   页面
/// @param[in] partial secret   32个字节的种子
///
/// @return
/// DS_TRUE - command successful @n
/// DS_FALSE - command failed
///
int DS28E16_cmd_computeS_Secret(int anon, int bdconst,
int pg, unsigned char *partial)
{
	unsigned char write_buf[40];
	unsigned char read_buf[40];
	int write_len = 0;
	int read_len = 1;
	int len_byte = 35;
	int param = pg & 0x03;

	last_result_byte = RESULT_FAIL_NONE;

	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_COMP_S_SECRET;

	if (bdconst)
		param = param | 0x04;
	if (anon)
		param = param | 0xE0;

	write_buf[write_len] = param;
	write_len++;
	write_buf[write_len++] = 0x08;// Fixed Parameter
	memcpy(&write_buf[write_len], partial, 32); // Partial Secret
	write_len += 32;

	/*ds_dbg("computeS_Secret:\n");
	ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			write_buf[0], write_buf[1], write_buf[2], write_buf[3],
			write_buf[4], write_buf[5], write_buf[6], write_buf[7],
			write_buf[8], write_buf[9], write_buf[10], write_buf[11],
			write_buf[12], write_buf[13], write_buf[14], write_buf[15],
			write_buf[16], write_buf[17], write_buf[18], write_buf[19],
			write_buf[20], write_buf[21], write_buf[22], write_buf[23],
			write_buf[24], write_buf[25], write_buf[26], write_buf[27],
			write_buf[28], write_buf[29], write_buf[30], write_buf[31],
			write_buf[32], write_buf[33], write_buf[34]);
	*/

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE,
	read_buf, &read_len, write_len)) {
		last_result_byte = read_buf[0];
		if (read_buf[0] == RESULT_SUCCESS)
			return DS_TRUE;
	}

	ow_reset();

	return DS_FALSE;
}

int AuthenticateDS28E16(int anon, int bdconst, int S_Secret_PageNum, int PageNum,
unsigned char *Challenge, unsigned char *Secret_Seeds, unsigned char *S_Secret)
{
	unsigned char PageData[32], MAC_Read_Value[32], CAL_MAC[32];
	unsigned char status_chip[16];
	unsigned char MAC_Computer_Datainput[128];
	int i = 0;
	int msg_len = 0;
	unsigned char flag = DS_FALSE;

	//if (flag_mi_auth_result)
	if (mi_auth_result == DS_TRUE)
		return mi_auth_result;
	//if (anon != ANONYMOUS) {

		if (ds28el16_Read_RomID_retry(mi_romid) != DS_TRUE) {
			ow_reset();
			return ERROR_R_ROMID;
		}

		if (ds28el16_get_page_status_retry(status_chip) == DS_TRUE) {
			MANID[0] = status_chip[4];
		} else {
			ow_reset();
			return ERROR_R_STATUS;
		}
	//}

	// DS28E16 calculate its session secret
	flag = DS28E16_cmd_computeS_Secret_retry(anon,
	bdconst, S_Secret_PageNum, Secret_Seeds);
	if (flag == DS_FALSE) {
		ds_err("DS28E16_cmd_computeS_Secret error");
		ow_reset();
		return ERROR_S_SECRET;
	}

	// DS28E16 compute its MAC based on above sessio secret
	flag = DS28E16_cmd_computeReadPageAuthentication_retry(anon,
	PageNum, Challenge, MAC_Read_Value);
	if (flag == DS_FALSE) {
		ds_err("DS28E16_cmd_computeReadPageAuthentication error");
		ow_reset();
		return ERROR_COMPUTE_MAC;
	}
	/*ds_dbg("%02x %02x %02x %02x", anon, bdconst, S_Secret_PageNum, PageNum);

	ds_dbg("Seeds:\n");
	ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			Secret_Seeds[0], Secret_Seeds[1], Secret_Seeds[2], Secret_Seeds[3],
			Secret_Seeds[4], Secret_Seeds[5], Secret_Seeds[6], Secret_Seeds[7],
			Secret_Seeds[8], Secret_Seeds[9], Secret_Seeds[10], Secret_Seeds[11],
			Secret_Seeds[12], Secret_Seeds[13], Secret_Seeds[14], Secret_Seeds[15],
			Secret_Seeds[16], Secret_Seeds[17], Secret_Seeds[18], Secret_Seeds[19],
			Secret_Seeds[20], Secret_Seeds[21], Secret_Seeds[22], Secret_Seeds[23],
			Secret_Seeds[24], Secret_Seeds[25], Secret_Seeds[26], Secret_Seeds[27],
			Secret_Seeds[28], Secret_Seeds[29], Secret_Seeds[30], Secret_Seeds[31]);

	ds_dbg("S_Secret:\n");
	ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			S_Secret[0], S_Secret[1], S_Secret[2], S_Secret[3],
			S_Secret[4], S_Secret[5], S_Secret[6], S_Secret[7],
			S_Secret[8], S_Secret[9], S_Secret[10], S_Secret[11],
			S_Secret[12], S_Secret[13], S_Secret[14], S_Secret[15],
			S_Secret[16], S_Secret[17], S_Secret[18], S_Secret[19],
			S_Secret[20], S_Secret[21], S_Secret[22], S_Secret[23],
			S_Secret[24], S_Secret[25], S_Secret[26], S_Secret[27],
			S_Secret[28], S_Secret[29], S_Secret[30], S_Secret[31]);

	ds_dbg("Challenge:\n");
	ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			Challenge[0], Challenge[1], Challenge[2], Challenge[3],
			Challenge[4], Challenge[5], Challenge[6], Challenge[7],
			Challenge[8], Challenge[9], Challenge[10], Challenge[11],
			Challenge[12], Challenge[13], Challenge[14], Challenge[15],
			Challenge[16], Challenge[17], Challenge[18], Challenge[19],
			Challenge[20], Challenge[21], Challenge[22], Challenge[23],
			Challenge[24], Challenge[25], Challenge[26], Challenge[27],
			Challenge[28], Challenge[29], Challenge[30], Challenge[31]);

	ds_dbg("MAC_Read_Value:\n");
	ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			MAC_Read_Value[0], MAC_Read_Value[1], MAC_Read_Value[2], MAC_Read_Value[3],
			MAC_Read_Value[4], MAC_Read_Value[5], MAC_Read_Value[6], MAC_Read_Value[7],
			MAC_Read_Value[8], MAC_Read_Value[9], MAC_Read_Value[10], MAC_Read_Value[11],
			MAC_Read_Value[12], MAC_Read_Value[13], MAC_Read_Value[14], MAC_Read_Value[15],
			MAC_Read_Value[16], MAC_Read_Value[17], MAC_Read_Value[18], MAC_Read_Value[19],
			MAC_Read_Value[20], MAC_Read_Value[21], MAC_Read_Value[22], MAC_Read_Value[23],
			MAC_Read_Value[24], MAC_Read_Value[25], MAC_Read_Value[26], MAC_Read_Value[27],
			MAC_Read_Value[28], MAC_Read_Value[29], MAC_Read_Value[30], MAC_Read_Value[31]);
	*/

	// read the page data
	flag = ds28el16_get_page_data_retry(PageNum, PageData);
	if (flag != DS_TRUE) {
		ds_err("DS28E16_cmd_readMemory error");
		ow_reset();
		return ERROR_R_PAGEDATA;
	}

	// insert mi_romid
	if (anon != ANONYMOUS)
		memcpy(MAC_Computer_Datainput, mi_romid, 8);
	else
		memset(MAC_Computer_Datainput, 0xff, 8);
	msg_len += 8;

	// insert Page Data
	memcpy(&MAC_Computer_Datainput[msg_len], PageData, 16);
	msg_len += 16;
	memset(&MAC_Computer_Datainput[msg_len], 0x00, 16);
	msg_len += 16;

	// insert Challenge
	memcpy(&MAC_Computer_Datainput[msg_len], Challenge, 32);
	msg_len += 32;

	// insert Bind Data Page number
	MAC_Computer_Datainput[msg_len] = PageNum & 0x03;
	MAC_Computer_Datainput[msg_len] = MAC_Computer_Datainput[msg_len] & 0x03;
	msg_len += 1;

	// insert MANID
	memcpy(&MAC_Computer_Datainput[msg_len], MANID, 2);
	msg_len += 2;

	// calculate the MAC
	sha3_256_hmac(S_Secret, 32, MAC_Computer_Datainput, msg_len, CAL_MAC);

	/*ds_dbg("host data:\n");
	ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			MAC_Computer_Datainput[0], MAC_Computer_Datainput[1], MAC_Computer_Datainput[2],
			MAC_Computer_Datainput[3], MAC_Computer_Datainput[4], MAC_Computer_Datainput[5],
			MAC_Computer_Datainput[6], MAC_Computer_Datainput[7], MAC_Computer_Datainput[8],
			MAC_Computer_Datainput[9], MAC_Computer_Datainput[10], MAC_Computer_Datainput[11],
			MAC_Computer_Datainput[12], MAC_Computer_Datainput[13], MAC_Computer_Datainput[14],
			MAC_Computer_Datainput[15],	MAC_Computer_Datainput[16], MAC_Computer_Datainput[17],
			MAC_Computer_Datainput[18], MAC_Computer_Datainput[19], MAC_Computer_Datainput[20],
			MAC_Computer_Datainput[21], MAC_Computer_Datainput[22], MAC_Computer_Datainput[23],
			MAC_Computer_Datainput[24], MAC_Computer_Datainput[25], MAC_Computer_Datainput[26],
			MAC_Computer_Datainput[27], MAC_Computer_Datainput[28], MAC_Computer_Datainput[29],
			MAC_Computer_Datainput[30], MAC_Computer_Datainput[31],	MAC_Computer_Datainput[32],
			MAC_Computer_Datainput[33], MAC_Computer_Datainput[34], MAC_Computer_Datainput[35],
			MAC_Computer_Datainput[36], MAC_Computer_Datainput[37], MAC_Computer_Datainput[38],
			MAC_Computer_Datainput[39],	MAC_Computer_Datainput[40], MAC_Computer_Datainput[41],
			MAC_Computer_Datainput[42], MAC_Computer_Datainput[43], MAC_Computer_Datainput[44],
			MAC_Computer_Datainput[45], MAC_Computer_Datainput[46], MAC_Computer_Datainput[47],
			MAC_Computer_Datainput[48], MAC_Computer_Datainput[49], MAC_Computer_Datainput[50],
			MAC_Computer_Datainput[51], MAC_Computer_Datainput[52], MAC_Computer_Datainput[53],
			MAC_Computer_Datainput[54], MAC_Computer_Datainput[55], MAC_Computer_Datainput[56],
			MAC_Computer_Datainput[57], MAC_Computer_Datainput[58], MAC_Computer_Datainput[59],
			MAC_Computer_Datainput[60], MAC_Computer_Datainput[61], MAC_Computer_Datainput[62],
			MAC_Computer_Datainput[63], MAC_Computer_Datainput[64], MAC_Computer_Datainput[65],
			MAC_Computer_Datainput[66], MAC_Computer_Datainput[67], MAC_Computer_Datainput[68],
			MAC_Computer_Datainput[69], MAC_Computer_Datainput[70], MAC_Computer_Datainput[71],
			MAC_Computer_Datainput[72], MAC_Computer_Datainput[73], MAC_Computer_Datainput[74],
			MAC_Computer_Datainput[75], MAC_Computer_Datainput[76], MAC_Computer_Datainput[77],
			MAC_Computer_Datainput[78], MAC_Computer_Datainput[79]);

	ds_dbg("host mac:\n");
	ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			CAL_MAC[0], CAL_MAC[1], CAL_MAC[2], CAL_MAC[3],
			CAL_MAC[4], CAL_MAC[5], CAL_MAC[6], CAL_MAC[7],
			CAL_MAC[8], CAL_MAC[9], CAL_MAC[10], CAL_MAC[11],
			CAL_MAC[12], CAL_MAC[13], CAL_MAC[14], CAL_MAC[15],
			CAL_MAC[16], CAL_MAC[17], CAL_MAC[18], CAL_MAC[19],
			CAL_MAC[20], CAL_MAC[21], CAL_MAC[22], CAL_MAC[23],
			CAL_MAC[24], CAL_MAC[25], CAL_MAC[26], CAL_MAC[27],
			CAL_MAC[28], CAL_MAC[29], CAL_MAC[30], CAL_MAC[31]);
	*/

	for (i = 0; i < 32; i++) {
		if (CAL_MAC[i] != MAC_Read_Value[i])
			break;
	}

	if (i != 32) {
		flag_mi_auth_result = 1;
		mi_auth_result = ERROR_UNMATCH_MAC;
		// for debug
		ds_dbg("hmac is not match. result=ERROR_UNMATCH_MAC\n");
		flag_mi_page1_data = 0;
		return ERROR_UNMATCH_MAC;
	} else {
		flag_mi_auth_result = 1;
		ds_dbg("hmac is match. result=DS_TRUE\n");
		mi_auth_result = DS_TRUE;
		return DS_TRUE;
	}
}


// retry interface start //
static int ds28el16_Read_RomID_retry(unsigned char *RomID)
{
	int i;
	static bool read_romid_ok = false;

	ds_info("read rom id communication start ...\n");
	if(read_romid_ok){
			ds_log("ds28el16_Read_RomID_retry success ...\n");
			return DS_TRUE;
	}else{
		for (i = 0; i < GET_ROM_ID_RETRY; i++) {
			DS28E16_cmd_romid_pre();
			if (Read_RomID(RomID) == DS_TRUE) {
				ds_log("ds28el16_Read_RomID_retry success %d\n", i);
				read_romid_ok = true;
				return DS_TRUE;
			}
		}
	}
	ds_log("ds28el16_Read_RomID_retry fail\n");

	return DS_FALSE;
}

static int ds28el16_get_page_status_retry(unsigned char *data)
{
	int i;
	for (i = 0; i < GET_BLOCK_STATUS_RETRY; i++) {
		ds_info("read page status communication start... %d\n", i);

		if (DS28E16_cmd_readStatus(data) == DS_TRUE)
			return DS_TRUE;
	}

	return DS_FALSE;
}

/*
static int ds28el16_set_page_status_retry(unsigned char page, unsigned char status)
{
	int i;

	for (i = 0; i < SET_BLOCK_STATUS_RETRY; i++) {
		ds_info("set page status communication start... %d\n", i);

		if (DS28E16_cmd_setPageProtection(page, status) == TRUE)
			return DS_TRUE;
	}

	return DS_FALSE;
}
*/

static int ds28el16_get_page_data_retry(int page, unsigned char *data)
{
	int i;

	if (page >= MAX_PAGENUM)
		return DS_FALSE;

	for (i = 0; i < GET_USER_MEMORY_RETRY; i++) {
		ds_dbg("read page data communication start... %d\n", i);

		if (DS28E16_cmd_readMemory(page, data) == DS_TRUE) {
			ds_dbg("page number is %d\n", page);
			/*ds_dbg("page data:\n");
			ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7],
			data[8], data[9], data[10], data[11],
			data[12], data[13], data[14], data[15]);
			ds_dbg("flag_mi_page0_data is %d\n", flag_mi_page0_data);
			ds_dbg("mi_page0_data data:\n");
			ds_dbg("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			mi_page0_data[0], mi_page0_data[1], mi_page0_data[2], mi_page0_data[3],
			mi_page0_data[4], mi_page0_data[5], mi_page0_data[6], mi_page0_data[7],
			mi_page0_data[8], mi_page0_data[9], mi_page0_data[10], mi_page0_data[11],
			mi_page0_data[12], mi_page0_data[13], mi_page0_data[14], mi_page0_data[15]);
			ds_dbg("flag_mi_page1_data is %d\n", flag_mi_page1_data);
			ds_dbg("mi_page1_data data:\n");
			ds_dbg("%%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			mi_page1_data[0], mi_page1_data[1], mi_page1_data[2], mi_page1_data[3],
			mi_page1_data[4], mi_page1_data[5], mi_page1_data[6], mi_page1_data[7],
			mi_page1_data[8], mi_page1_data[9], mi_page1_data[10], mi_page1_data[11],
			mi_page1_data[12], mi_page1_data[13], mi_page1_data[14], mi_page1_data[15]);*/
			ds_dbg("flag_mi_counter is %d\n", flag_mi_counter);

			return DS_TRUE;
		}
	}

	return DS_FALSE;
}

static int DS28E16_cmd_computeS_Secret_retry(int anon, int bdconst,
				int pg, unsigned char *partial)
{
	int i;

	if (pg >= MAX_PAGENUM)
		return DS_FALSE;

	for (i = 0; i < GET_S_SECRET_RETRY; i++) {
		if (DS28E16_cmd_computeS_Secret(anon, bdconst,
				pg, partial) == DS_TRUE)
			return DS_TRUE;
	}

	return DS_FALSE;
}

static int DS28E16_cmd_computeReadPageAuthentication_retry(int anon, int pg,
				unsigned char *challenge, unsigned char *hmac)
{
	int i;

	if (pg >= MAX_PAGENUM)
		return DS_FALSE;

	for (i = 0; i < GET_MAC_RETRY; i++) {
		if (DS28E16_cmd_computeReadPageAuthentication(anon, pg,
					challenge, hmac) == DS_TRUE)
			return DS_TRUE;
	}

	return DS_FALSE;
}

// retry interface end //

/* All power supply functions here */

static enum power_supply_property verify_props[] = {
	POWER_SUPPLY_PROP_ROMID,
	POWER_SUPPLY_PROP_DS_STATUS,
	POWER_SUPPLY_PROP_PAGENUMBER,
	POWER_SUPPLY_PROP_PAGEDATA,
	POWER_SUPPLY_PROP_AUTHEN_RESULT,
	POWER_SUPPLY_PROP_SESSION_SEED,
	POWER_SUPPLY_PROP_S_SECRET,
	POWER_SUPPLY_PROP_CHALLENGE,
	POWER_SUPPLY_PROP_AUTH_ANON,
	POWER_SUPPLY_PROP_AUTH_BDCONST,
	POWER_SUPPLY_PROP_PAGE0_DATA,
	POWER_SUPPLY_PROP_PAGE1_DATA,
	POWER_SUPPLY_PROP_VERIFY_MODEL_NAME,
	POWER_SUPPLY_PROP_CHIP_OK,
};

static int verify_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret;
	unsigned char buf[50];

	switch (psp) {
	case POWER_SUPPLY_PROP_VERIFY_MODEL_NAME:
		ret = ds28el16_Read_RomID_retry(mi_romid);
		if (ret == DS_TRUE)
			val->strval = "ds28e16";
		else
			val->strval = "unknown";
		break;
	case POWER_SUPPLY_PROP_AUTHEN_RESULT:
		ret = AuthenticateDS28E16(auth_ANON, auth_BDCONST, 0,
			pagenumber, challenge, session_seed, S_secret);
		if (ret == DS_TRUE)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_PAGENUMBER:
		val->intval = pagenumber;
		break;
	case POWER_SUPPLY_PROP_ROMID:
		ret = ds28el16_Read_RomID_retry(mi_romid);
		memcpy(val->arrayval, mi_romid, 8);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		ds_log("getian---POWER_SUPPLY_PROP_CHIP_OK\n");
		ret = ds28el16_Read_RomID_retry(mi_romid);
		if ((mi_romid[0] == FAMILY_CODE) && (mi_romid[6] == CUSTOM_ID_MSB) && ((mi_romid[5] & 0xf0) == CUSTOM_ID_LSB))
			val->intval = true;
		else
			val->intval = false;
		ds_log("getian---flag_mi_romid: %d, flag_mi_status: %d\n", flag_mi_romid, flag_mi_status);
		break;
	case POWER_SUPPLY_PROP_DS_STATUS:
		ret = ds28el16_get_page_status_retry(buf);
		memcpy(val->arrayval, buf, 8);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_PAGEDATA:
		ret = ds28el16_get_page_data_retry(pagenumber, buf);
		memcpy(val->arrayval, buf, 16);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_PAGE0_DATA:
		ret = ds28el16_get_page_data_retry(0, buf);
		memcpy(val->arrayval, buf, 16);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_PAGE1_DATA:
		ret = ds28el16_get_page_data_retry(1, buf);
		memcpy(val->arrayval, buf, 16);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	default:
		ds_err("unsupported property %d\n", psp);
		return -ENODATA;
	}

	return 0;
}

static int verify_set_property(struct power_supply *psy,
			       enum power_supply_property prop,
			       const union power_supply_propval *val)
{
	//int ret;
	//unsigned char buf[50];

	switch (prop) {
	case POWER_SUPPLY_PROP_PAGENUMBER:
		pagenumber = val->intval;
		break;
/*
	case POWER_SUPPLY_PROP_PAGEDATA:
		memcpy(buf, val->arrayval, 16);
		ret = DS28E16_cmd_writeMemory(pagenumber, buf);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_SESSION_SEED:
		memcpy(session_seed, val->arrayval, 32);
		break;
	case POWER_SUPPLY_PROP_S_SECRET:
		memcpy(S_secret, val->arrayval, 32);
		break;
	case POWER_SUPPLY_PROP_CHALLENGE:
		memcpy(challenge, val->arrayval, 32);
		break;
*/
	case POWER_SUPPLY_PROP_AUTH_ANON:
		auth_ANON  = val->intval;
		break;
	case POWER_SUPPLY_PROP_AUTH_BDCONST:
		auth_BDCONST   = val->intval;
		break;
	default:
		ds_err("unsupported property %d\n", prop);
		return -ENODATA;
	}

	return 0;
}

static int verify_prop_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_PAGENUMBER:
	case POWER_SUPPLY_PROP_PAGEDATA:
	case POWER_SUPPLY_PROP_SESSION_SEED:
	case POWER_SUPPLY_PROP_S_SECRET:
	case POWER_SUPPLY_PROP_CHALLENGE:
	case POWER_SUPPLY_PROP_AUTH_ANON:
	case POWER_SUPPLY_PROP_AUTH_BDCONST:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int verify_psy_register(struct ds28e16_data *ds)
{
	struct power_supply_config verify_psy_cfg = {};

	ds->verify_psy_d.name = "batt_verify";
	ds->verify_psy_d.type = POWER_SUPPLY_TYPE_BATT_VERIFY;
	ds->verify_psy_d.properties = verify_props;
	ds->verify_psy_d.num_properties = ARRAY_SIZE(verify_props);
	ds->verify_psy_d.get_property = verify_get_property;
	ds->verify_psy_d.set_property = verify_set_property;
	ds->verify_psy_d.property_is_writeable = verify_prop_is_writeable;

	verify_psy_cfg.drv_data = ds;
	verify_psy_cfg.of_node = ds->dev->of_node;
	verify_psy_cfg.num_supplicants = 0;
	ds->verify_psy = devm_power_supply_register(ds->dev,
						&ds->verify_psy_d,
						&verify_psy_cfg);
	if (IS_ERR(ds->verify_psy)) {
		ds_err("Failed to register verify_psy");
		return PTR_ERR(ds->verify_psy);
	}

	ds_log("%s power supply register successfully\n", ds->verify_psy_d.name);
	return 0;
}


static void verify_psy_unregister(struct ds28e16_data *ds)
{
	power_supply_unregister(ds->verify_psy);
}

// parse dts
static int ds28e16_parse_dt(struct device *dev,
				struct ds28e16_data *pdata)
{
	int error, val;
	struct device_node *np = dev->of_node;

	// parse version
	pdata->version = 0;
	error = of_property_read_u32(np, "maxim,version", &val);
	if (error && (error != -EINVAL))
		ds_err("Unable to read bootloader address\n");
	else if (error != -EINVAL)
		pdata->version = val;

	return 0;
}

// read data from file
static ssize_t ds28e16_ds_Auth_Result_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int result;

	result = AuthenticateDS28E16(auth_ANON, auth_BDCONST, 0,
			pagenumber, challenge, session_seed, S_secret);
	if (result == ERROR_R_STATUS)
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate failed : ERROR_R_STATUS!\n");
	else if (result == ERROR_UNMATCH_MAC)
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate failed : MAC is not match!\n");
	else if (result == ERROR_R_ROMID)
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate failed : ERROR_R_ROMID!\n");
	else if (result == ERROR_COMPUTE_MAC)
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate failed : ERROR_COMPUTE_MAC!\n");
	else if (result == ERROR_S_SECRET)
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate failed : ERROR_S_SECRET!\n");
	else if (result == DS_TRUE)
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate success!!!\n");
	else
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate failed : other reason.\n");
}

static ssize_t ds28e16_ds_romid_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	short status;
	unsigned char RomID[10] = {0x00};
	int i = 0; int count = 0;

	for (i = 0; i < attr_trytimes; i++) {
		status = ds28el16_Read_RomID_retry(RomID);

		if (status == DS_TRUE) {
			count++;
			ds_log("Read_RomID success!\n");
		} else {
			ds_log("Read_RomID fail!\n");
		}
		Delay_us(1000);
	}
#ifdef CONFIG_TARGET_PROJECT_J20C
	/*ds_dbg("RomID = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	RomID[0], RomID[1], RomID[2], RomID[3],
	RomID[4], RomID[5], RomID[6], RomID[7]);
	*/
#endif
	ds_log("test done\nsuccess time : %d\n", count);
	return scnprintf(buf, PAGE_SIZE,
	"Success = %d\nRomID = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	count, RomID[0], RomID[1], RomID[2], RomID[3],
	RomID[4], RomID[5], RomID[6], RomID[7]);
}

static ssize_t ds28e16_ds_pagenumber_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02x\n", pagenumber);
}

static ssize_t ds28e16_ds_pagenumber_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	int buf_int;

	if (sscanf(buf, "%d", &buf_int) != 1)
		return -EINVAL;

	ds_dbg("new pagenumber = %d\n", buf_int);

	if ((buf_int >= 0) && (buf_int <= 3))
		pagenumber = buf_int;

	return count;
}

static ssize_t ds28e16_ds_pagedata_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int result;
	unsigned char pagedata[16] = {0x00};
	int i = 0; int count = 0;

	for (i = 0; i < attr_trytimes; i++) {
		result = ds28el16_get_page_data_retry(pagenumber, pagedata);

		if (result == DS_TRUE) {
			count++;
			ds_log("DS28E16_cmd_readMemory success!\n");
		} else {
			ds_log("DS28E16_cmd_readMemory fail!\n");
		}
		ds_dbg("pagedata = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
		pagedata[0], pagedata[1], pagedata[2], pagedata[3],
		pagedata[4], pagedata[5], pagedata[6], pagedata[7],
		pagedata[8], pagedata[9], pagedata[10], pagedata[11],
		pagedata[12], pagedata[13], pagedata[14], pagedata[15]);
		Delay_us(1000);
	}
	ds_log("test done\nsuccess time : %d\n", count);
	return scnprintf(buf, PAGE_SIZE,
	"Success = %d\npagedata = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	count, pagedata[0], pagedata[1], pagedata[2], pagedata[3],
	pagedata[4], pagedata[5], pagedata[6], pagedata[7],
	pagedata[8], pagedata[9], pagedata[10], pagedata[11],
	pagedata[12], pagedata[13], pagedata[14], pagedata[15]);
}

static ssize_t ds28e16_ds_pagedata_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	int result;
	unsigned char pagedata[16] = {0x00};

	if (sscanf(buf, "%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx",
	&pagedata[0], &pagedata[1], &pagedata[2], &pagedata[3],
	&pagedata[4], &pagedata[5], &pagedata[6], &pagedata[7],
	&pagedata[8], &pagedata[9], &pagedata[10], &pagedata[11],
	&pagedata[12], &pagedata[13], &pagedata[14], &pagedata[15]) != 16)
		return -EINVAL;

	ds_dbg("new data = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	pagedata[0], pagedata[1], pagedata[2], pagedata[3],
	pagedata[4], pagedata[5], pagedata[6], pagedata[7],
	pagedata[8], pagedata[9], pagedata[10], pagedata[11],
	pagedata[12], pagedata[13], pagedata[14], pagedata[15]);

	result = DS28E16_cmd_writeMemory(pagenumber, pagedata);
	if (result == DS_TRUE)
		ds_log("DS28E16_cmd_writeMemory success!\n");
	else
		ds_log("DS28E16_cmd_writeMemory fail!\n");

	return count;
}

static ssize_t ds28e16_ds_time_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", attr_trytimes);
}

static ssize_t ds28e16_ds_time_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	int buf_int;

	if (sscanf(buf, "%d", &buf_int) != 1)
		return -EINVAL;

	ds_log("new trytimes = %d\n", buf_int);

	if (buf_int > 0)
		attr_trytimes = buf_int;

	return count;
}

static ssize_t ds28e16_ds_session_seed_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
	"%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n",
	session_seed[0], session_seed[1], session_seed[2], session_seed[3],
	session_seed[4], session_seed[5], session_seed[6], session_seed[7],
	session_seed[8], session_seed[9], session_seed[10], session_seed[11],
	session_seed[12], session_seed[13], session_seed[14], session_seed[15],
	session_seed[16], session_seed[17], session_seed[18], session_seed[19],
	session_seed[20], session_seed[21], session_seed[22], session_seed[23],
	session_seed[24], session_seed[25], session_seed[26], session_seed[27],
	session_seed[28], session_seed[29], session_seed[30], session_seed[31]);
}

static ssize_t ds28e16_ds_session_seed_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	if (sscanf(buf, "%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx",
		&session_seed[0], &session_seed[1], &session_seed[2],
		&session_seed[3], &session_seed[4], &session_seed[5],
		&session_seed[6], &session_seed[7], &session_seed[8],
		&session_seed[9], &session_seed[10], &session_seed[11],
		&session_seed[12], &session_seed[13], &session_seed[14],
		&session_seed[15], &session_seed[16], &session_seed[17],
		&session_seed[18], &session_seed[19], &session_seed[20],
		&session_seed[21], &session_seed[22], &session_seed[23],
		&session_seed[24], &session_seed[25], &session_seed[26],
		&session_seed[27], &session_seed[28], &session_seed[29],
		&session_seed[30], &session_seed[31]) != 32)
		return -EINVAL;

	return count;
}

static ssize_t ds28e16_ds_challenge_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
	"%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n",
	challenge[0], challenge[1], challenge[2], challenge[3],
	challenge[4], challenge[5], challenge[6], challenge[7],
	challenge[8], challenge[9], challenge[10], challenge[11],
	challenge[12], challenge[13], challenge[14], challenge[15],
	challenge[16], challenge[17], challenge[18], challenge[19],
	challenge[20], challenge[21], challenge[22], challenge[23],
	challenge[24], challenge[25], challenge[26], challenge[27],
	challenge[28], challenge[29], challenge[30], challenge[31]);
}

static ssize_t ds28e16_ds_challenge_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	if (sscanf(buf, "%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx",
		&challenge[0], &challenge[1], &challenge[2], &challenge[3],
		&challenge[4], &challenge[5], &challenge[6], &challenge[7],
		&challenge[8], &challenge[9], &challenge[10], &challenge[11],
		&challenge[12], &challenge[13], &challenge[14], &challenge[15],
		&challenge[16], &challenge[17], &challenge[18], &challenge[19],
		&challenge[20], &challenge[21], &challenge[22], &challenge[23],
		&challenge[24], &challenge[25], &challenge[26], &challenge[27],
		&challenge[28], &challenge[29],
		&challenge[30], &challenge[31]) != 32)
		return -EINVAL;

	return count;
}

static ssize_t ds28e16_ds_S_secret_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
	"%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,\n",
	S_secret[0], S_secret[1], S_secret[2], S_secret[3],
	S_secret[4], S_secret[5], S_secret[6], S_secret[7],
	S_secret[8], S_secret[9], S_secret[10], S_secret[11],
	S_secret[12], S_secret[13], S_secret[14], S_secret[15],
	S_secret[16], S_secret[17], S_secret[18], S_secret[19],
	S_secret[20], S_secret[21], S_secret[22], S_secret[23],
	S_secret[24], S_secret[25], S_secret[26], S_secret[27],
	S_secret[28], S_secret[29], S_secret[30], S_secret[31]);
}

static ssize_t ds28e16_ds_S_secret_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	if (sscanf(buf, "%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx,%2hhx",
		&S_secret[0], &S_secret[1], &S_secret[2], &S_secret[3],
		&S_secret[4], &S_secret[5], &S_secret[6], &S_secret[7],
		&S_secret[8], &S_secret[9], &S_secret[10], &S_secret[11],
		&S_secret[12], &S_secret[13], &S_secret[14], &S_secret[15],
		&S_secret[16], &S_secret[17], &S_secret[18], &S_secret[19],
		&S_secret[20], &S_secret[21], &S_secret[22], &S_secret[23],
		&S_secret[24], &S_secret[25], &S_secret[26], &S_secret[27],
		&S_secret[28], &S_secret[29],
		&S_secret[30], &S_secret[31]) != 32)
		return -EINVAL;

	return count;
}

static ssize_t ds28e16_ds_auth_ANON_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02x\n", auth_ANON);
}

static ssize_t ds28e16_ds_auth_ANON_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	if (sscanf(buf, "%d", &auth_ANON) != 1)
		return -EINVAL;

	return count;
}

static ssize_t ds28e16_ds_auth_BDCONST_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02x\n", auth_BDCONST);
}

static ssize_t ds28e16_ds_auth_BDCONST_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	if (sscanf(buf, "%d", &auth_BDCONST) != 1)
		return -EINVAL;

	return count;
}

static ssize_t ds28e16_ds_readstatus_status_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	int result;
	unsigned char status[16] = {0x00};
	int i = 0; int count = 0;


	for (i = 0; i < attr_trytimes; i++) {
		result = ds28el16_get_page_status_retry(status);

		if (result == DS_TRUE) {
			count++;
			ds_log("DS28E16_cmd_readStatus success!\n");
		} else {
			ds_log("DS28E16_cmd_readStatus fail!\n");
		}
		ds_dbg("Status = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
		status[0], status[1], status[2], status[3],
		status[4], status[5], status[6], status[7],
		status[8], status[9], status[10], status[11],
		status[12], status[13], status[14], status[15]);
		Delay_us(1000);
	}
	ds_log("test done\nsuccess time : %d\n", count);
	return scnprintf(buf, PAGE_SIZE,
	"Success = %d\nStatus = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
	count, status[0], status[1], status[2], status[3],
	status[4], status[5], status[6], status[7],
	status[8], status[9], status[10], status[11],
	status[12], status[13], status[14], status[15]);
}

static DEVICE_ATTR(ds_readstatus, S_IRUGO,
		ds28e16_ds_readstatus_status_read, NULL);
static DEVICE_ATTR(ds_romid, S_IRUGO,
		ds28e16_ds_romid_status_read, NULL);
static DEVICE_ATTR(ds_pagenumber, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_pagenumber_status_read,
		ds28e16_ds_pagenumber_store);
static DEVICE_ATTR(ds_pagedata, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_pagedata_status_read,
		ds28e16_ds_pagedata_store);
static DEVICE_ATTR(ds_time, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_time_status_read,
		ds28e16_ds_time_store);
static DEVICE_ATTR(ds_session_seed, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_session_seed_status_read,
		ds28e16_ds_session_seed_store);
static DEVICE_ATTR(ds_challenge, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_challenge_status_read,
		ds28e16_ds_challenge_store);
static DEVICE_ATTR(ds_S_secret, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_S_secret_status_read,
		ds28e16_ds_S_secret_store);
static DEVICE_ATTR(ds_auth_ANON, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_auth_ANON_status_read,
		ds28e16_ds_auth_ANON_store);
static DEVICE_ATTR(ds_auth_BDCONST, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_auth_BDCONST_status_read,
		ds28e16_ds_auth_BDCONST_store);
static DEVICE_ATTR(ds_Auth_Result, S_IRUGO,
		ds28e16_ds_Auth_Result_status_read, NULL);

static struct attribute *ds_attributes[] = {
	&dev_attr_ds_readstatus.attr,
	&dev_attr_ds_romid.attr,
	&dev_attr_ds_pagenumber.attr,
	&dev_attr_ds_pagedata.attr,
	&dev_attr_ds_time.attr,
	&dev_attr_ds_session_seed.attr,
	&dev_attr_ds_challenge.attr,
	&dev_attr_ds_S_secret.attr,
	&dev_attr_ds_auth_ANON.attr,
	&dev_attr_ds_auth_BDCONST.attr,
	&dev_attr_ds_Auth_Result.attr,
	NULL,
};

static const struct attribute_group ds_attr_group = {
	.attrs = ds_attributes,
};

static int authentic_period_ms = 5000;
#define AUTHENTIC_COUNT_MAX 5
int retry_authentic;
static void authentic_work(struct work_struct *work)
{
	int rc;
	union power_supply_propval pval = {0,};

	struct ds28e16_data *ds28e16_data = container_of(work,
				struct ds28e16_data,
				authentic_work.work);

	rc = power_supply_get_property(ds28e16_data->verify_psy,
					POWER_SUPPLY_PROP_AUTHEN_RESULT, &pval);
	if (pval.intval != true) {
		retry_authentic++;
		if (retry_authentic < AUTHENTIC_COUNT_MAX) {
			ds_log("battery authentic work begin to restart.\n");
			schedule_delayed_work(&ds28e16_data->authentic_work,
				msecs_to_jiffies(authentic_period_ms));
		}

		if (retry_authentic == AUTHENTIC_COUNT_MAX) {
			ds_log("authentic result is %d\n", pval.intval);
		}
	} else {
		ds_log("authentic result is %d\n", pval.intval);
	}
}

static int ds28e16_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct ds28e16_data *ds28e16_data;
	union power_supply_propval b_val = {0,};
	get_random_number();
	ds_log("%s entry.", __func__);
	ds_dbg("platform_device is %s", pdev->name);
	if (strcmp(pdev->name, "soc:maxim_ds28e16") != 0)
		return -ENODEV;

	if (!pdev->dev.of_node || !of_device_is_available(pdev->dev.of_node))
		return -ENODEV;

	if (pdev->dev.of_node) {
		ds28e16_data = devm_kzalloc(&pdev->dev,
			sizeof(struct ds28e16_data),
			GFP_KERNEL);
		if (!ds28e16_data) {
			ds_err("Failed to allocate memory\n");
			return -ENOMEM;
		}

		retval = ds28e16_parse_dt(&pdev->dev, ds28e16_data);
		if (retval) {
			retval = -EINVAL;
			goto ds28e16_parse_dt_err;
		}
	} else {
		ds28e16_data = pdev->dev.platform_data;
	}

	if (!ds28e16_data) {
		ds_err("No platform data found\n");
		return -EINVAL;
	}


	ds28e16_data->dev = &pdev->dev;
	ds28e16_data->pdev = pdev;
	platform_set_drvdata(pdev, ds28e16_data);
	INIT_DELAYED_WORK(&ds28e16_data->authentic_work, authentic_work);

	retval = verify_psy_register(ds28e16_data);
	if (retval) {
		ds_err("Failed to verify_psy_register, err:%d\n", retval);
		goto ds28e16_psy_register_err;
	}

	retval = sysfs_create_group(&ds28e16_data->dev->kobj, &ds_attr_group);
	if (retval) {
		ds_err("Failed to register sysfs, err:%d\n", retval);
		goto ds28e16_create_group_err;
	}

	retval = power_supply_get_property(ds28e16_data->verify_psy,
					POWER_SUPPLY_PROP_AUTHEN_RESULT, &b_val);
	if (b_val.intval != true) {
		schedule_delayed_work(&ds28e16_data->authentic_work,
				msecs_to_jiffies(0));
	}
	return 0;

ds28e16_create_group_err:
//sysfs_remove_groups(&ds28e16_data->dev->kobj, &(&ds_attr_group));
ds28e16_psy_register_err:
dev_set_drvdata(ds28e16_data->dev, NULL);
ds28e16_parse_dt_err:
	kfree(ds28e16_data);
	return retval;
}

static int ds28e16_remove(struct platform_device *pdev)
{
	struct ds28e16_data *ds28e16_data = platform_get_drvdata(pdev);

	verify_psy_unregister(ds28e16_data);
	kfree(ds28e16_data);
	return 0;
}

static long ds28e16_dev_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	ds_log("%d, cmd: 0x%x\n", __LINE__, cmd);
	return 0;
}
static int ds28e16_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ds28e16_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations ds28e16_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= ds28e16_dev_open,
	.unlocked_ioctl = ds28e16_dev_ioctl,
	.release	= ds28e16_dev_release,
};

static const struct of_device_id ds28e16_dt_match[] = {
	{.compatible = "maxim,ds28e16"},
	{},
};

static struct platform_driver ds28e16_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "maxim_ds28e16",
		.of_match_table = ds28e16_dt_match,
	},
	.probe = ds28e16_probe,
	.remove = ds28e16_remove,
};

static int __init ds28e16_init(void)
{
	ds_log("%s entry.", __func__);

	return platform_driver_register(&ds28e16_driver);
}

static void __exit ds28e16_exit(void)
{
	ds_log("%s entry.", __func__);
	platform_driver_unregister(&ds28e16_driver);
}

module_init(ds28e16_init);
module_exit(ds28e16_exit);

MODULE_AUTHOR("xiaomi Inc.");
MODULE_DESCRIPTION("ds28e16 driver");
MODULE_LICENSE("GPL");

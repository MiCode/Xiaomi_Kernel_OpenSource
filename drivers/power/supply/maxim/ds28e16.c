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
#include <linux/sched.h>

#define ds_info	pr_info
#define ds_dbg	pr_debug
#define ds_err	pr_err
#define ds_log	pr_debug

struct ds28e16_data {
	struct platform_device *pdev;
	struct device *dev;

	int version;
	int cycle_count;
	bool batt_verified;
#ifdef	CONFIG_FACTORY_BUILD
	bool factory_enable;
#endif

	struct delayed_work	battery_verify_work;
	struct power_supply *verify_psy;
	struct power_supply_desc verify_psy_d;
};

unsigned int attr_trytimes = 1;

unsigned char session_seed[32] = {
0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
unsigned char S_secret[32] = {
0x0C, 0x99, 0x2B, 0xD3, 0x95, 0xDB, 0xA0, 0xB4,
0xEF, 0x07, 0xB3, 0xD8, 0x75, 0xF3, 0xC7, 0xAE,
0xDA, 0xC4, 0x41, 0x2F, 0x48, 0x93, 0xB5, 0xD9,
0xE1, 0xE5, 0x4B, 0x20, 0x9B, 0xF3, 0x77, 0x39};
unsigned char challenge[32] = {0x00};
int auth_ANON = 1;
int auth_BDCONST = 1;
int pagenumber = 0;

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

static void set_sched_affinity_to_current(void)
{
    long ret;
    int current_cpu;

    preempt_disable();
    current_cpu = smp_processor_id();
    ret = sched_setaffinity(CURRENT_DS28E16_TASK, cpumask_of(current_cpu));
    preempt_enable();
    if(ret) {
        pr_info("Setting cpu affinity to current cpu failed(%ld) in %s.\n", ret, __func__);
    } else {
        pr_info("Setting cpu affinity to current cpu(%d) in %s.\n", current_cpu, __func__);
    }
}

static void set_sched_affinity_to_all(void)
{
    long ret;
    cpumask_t dstp;

    cpumask_setall(&dstp);
    ret = sched_setaffinity(CURRENT_DS28E16_TASK, &dstp);
    if(ret) {
        pr_info("Setting cpu affinity to all valid cpus failed(%ld) in %s.\n", ret, __func__);
    } else {
        pr_info("Setting cpu affinity to all valid cpus in %s.\n", __func__);
    }
}

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

	/*if (flag_mi_romid == 2) {
		memcpy(RomID, mi_romid, 8);
		return DS_TRUE;
	}*/

	if ((ow_reset()) != 0) {
		ds_err("Failed to reset ds28e16!\n");
		return ERROR_NO_DEVICE;
	}

	ds_dbg("Ready to write 0x33 to maxim IC!\n");
	write_byte(CMD_READ_ROM);
	Delay_us(10);
	for (i = 0; i < 8; i++)
		RomID[i] = read_byte();

	ds_info("RomID = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
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
		return DS_TRUE;
	} else {
		return DS_FALSE;
	}
}

static int ds28el16_Read_RomID_retry(unsigned char *RomID)
{
	int i;

	set_sched_affinity_to_current();
	for (i = 0; i < GET_ROM_ID_RETRY; i++) {
		ds_info("read rom id communication start %d...\n", i);

		if (Read_RomID(RomID) == DS_TRUE){
			set_sched_affinity_to_all();
			return DS_TRUE;
		}
	}
	set_sched_affinity_to_all();
	return DS_FALSE;
}

static int ds28el16_get_page_status_retry(unsigned char *data)
{
	int i;

	set_sched_affinity_to_current();
	for (i = 0; i < GET_BLOCK_STATUS_RETRY; i++) {
		ds_info("read page status communication start... %d\n", i);

		if (DS28E16_cmd_readStatus(data) == DS_TRUE) {
			set_sched_affinity_to_all();
			return DS_TRUE;
		}
	}
	set_sched_affinity_to_all();

	return DS_FALSE;
}

static int ds28el16_get_page_data_retry(int page, unsigned char *data)
{
	int i;

	if (page >= MAX_PAGENUM)
		return DS_FALSE;

	set_sched_affinity_to_current();
	for (i = 0; i < GET_USER_MEMORY_RETRY; i++) {
		ds_dbg("read page data communication start... %d\n", i);

		if (DS28E16_cmd_readMemory(page, data) == DS_TRUE) {
			set_sched_affinity_to_all();
			return DS_TRUE;
		}
	}
	set_sched_affinity_to_all();

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

	if ((ow_reset()) != 0) {
		ds_err("Failed to reset ds28e16!\n");
		goto final_reset;
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

	if (CRC16 != 0xB001)
		return DS_FALSE;

	// check for strong pull-up
	if (delay_ms > 0) {
		write_byte(CMD_RELEASE_BYTE);
		Delay_us(1000*delay_ms);
	}

	read_byte();
	*read_len = read_byte();

	if (expected_read_len != *read_len)
		return DS_FALSE;

	buf_len = *read_len + 2;
	for (i = 0; i < buf_len; i++)
		buf[i] = read_byte();

	CRC16 = 0;
	docrc16(*read_len);
	for (i = 0; i < buf_len; i++)
		docrc16(buf[i]);

	if (CRC16 != 0xB001)
		return DS_FALSE;

	memcpy(read_buf, buf, *read_len);
	return DS_TRUE;

final_reset:
	ow_reset();
	return ERROR_NO_DEVICE;
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

	/*if (flag_mi_status) {
		memcpy(data, mi_status, 8);
		return DS_TRUE;
	}*/

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
			return DS_TRUE;
		}
	}

	return DS_FALSE;
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

	switch (pagenum)
	{
		case 0x00:
			if (flag_mi_page0_data) {
				memcpy(data, mi_page0_data, 16);
				return DS_TRUE;
			}
			break;
		/*case 0x01:
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
			break;*/
		default:
			break;
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

	if (pg >= DS28EL16_MAX_PAGE) {
		ds_log("page(%d) data should not be set.\n", pg);
		return DS_FALSE;
	}

	write_buf[write_len++] = length_byte;
	write_buf[write_len++] = CMD_READ_MEM;
	write_buf[write_len++] = pagenum;

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
				/*if (pagenum == 0x01) {
					flag_mi_page1_data = 1;
					memcpy(mi_page1_data, data, 16);
				}*/
				return DS_TRUE;
			} else {
				if (read_buf[0] == 0x55 && pagenum == 2) {
					memcpy(data, &read_buf[1], 16);
					//flag_mi_counter = 1;
					//memcpy(mi_counter, data, 16);
					return DS_TRUE;
				}
			}
		}
		/* check result byte, implements result byte */
		/*if (read_len == 33) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS) {
				memcpy(data, &read_buf[1], 16);
				return DS_TRUE;
			} else {
				if (read_buf[0] == RESULT_FAIL_PROTECTION && pg == 2) {
					memcpy(data, &read_buf[1], 16);
					return DS_TRUE;
				}
			}
		}*/
	}

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

	if (pg > DS28EL16_MAX_USABLE_PAGE) {
		ds_log("page(%d) data should not be set.\n", pg);
		return DS_FALSE;
	}
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
		/*if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS){
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
		}*/
		/* check result byte */
		if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS)
				return DS_TRUE;
		}
	}

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

	return DS_FALSE;
}

// --------------------------------------------------------------------------
/// 'Compute and Read Page Authentication' command
///
/// @param[in] anon - boolean parameter
/// @param[in] pg - Page number   2,计数器; 0,page0; 1,page1;
/// @param[in] challenge
/// @param[out] hmac   返回的计算结果32个字节
///
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
	int i;

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
	for (i = 0; i < 35; i++)
		ds_dbg("write_buf[%d] = %02x ", i, write_buf[i]);

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE, read_buf, &read_len, write_len)) {
		last_result_byte = read_buf[0];
		if (read_buf[0] == RESULT_SUCCESS) {
			memcpy(hmac, &read_buf[1], 32);
			return DS_TRUE;
		}
	}

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
	int i;

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

	ds_dbg("computeS_Secret:\n");
	for (i = 0; i < 35; i++)
		ds_dbg("write_buf[%d] = %02x ", i, write_buf[i]);

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE,
	read_buf, &read_len, write_len)) {
		last_result_byte = read_buf[0];
		if (read_buf[0] == RESULT_SUCCESS)
			return DS_TRUE;
	}

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

	/*if (flag_mi_auth_result)
		return mi_auth_result;*/

	if (ds28el16_get_page_status_retry(status_chip) == DS_TRUE)
		MANID[0] = status_chip[4];
	else
		return ERROR_R_STATUS;

	if (ds28el16_Read_RomID_retry(mi_romid) != DS_TRUE)
		return ERROR_R_ROMID;

	// DS28E16 calculate its session secret
	flag = DS28E16_cmd_computeS_Secret_retry(anon,
	bdconst, S_Secret_PageNum, Secret_Seeds);
	if (flag == DS_FALSE) {
		ds_err("DS28E16_cmd_computeS_Secret error");
		return ERROR_S_SECRET;
	}

	// DS28E16 compute its MAC based on above sessio secret
	flag = DS28E16_cmd_computeReadPageAuthentication_retry(anon,
	PageNum, Challenge, MAC_Read_Value);
	if (flag == DS_FALSE) {
		ds_err("DS28E16_cmd_computeReadPageAuthentication error");
		return ERROR_COMPUTE_MAC;
	}

	ds_dbg("%02x %02x %02x %02x", anon, bdconst, S_Secret_PageNum, PageNum);

	ds_dbg("Seeds:\n");
	for (i = 0; i < 32; i++)
		ds_dbg("Secret_Seeds[%d] = %02x ", i, Secret_Seeds[i]);

	ds_dbg("S_Secret:\n");
	for (i = 0; i < 32; i++)
		ds_dbg("S_Secret[%d] = %02x ", i, S_Secret[i]);

	ds_dbg("Challenge:\n");
	for (i = 0; i < 32; i++)
		ds_dbg("Challenge[%d] = %02x ", i, Challenge[i]);

	ds_dbg("MAC_Read_Value:\n");
	for (i = 0; i < 32; i++)
		ds_dbg("MAC_Read_Value[%d] = %02x ", i, MAC_Read_Value[i]);

	// read the page data
	flag = ds28el16_get_page_data_retry(PageNum, PageData);
	if (flag != DS_TRUE) {
		ds_err("DS28E16_cmd_readMemory error");
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

	ds_dbg("host data:\n");
	for (i = 0; i < 80; i++)
		ds_dbg("MAC_Computer_Datainput[%d] = %02x ", i, MAC_Computer_Datainput[i]);

	ds_dbg("host mac:\n");
	for (i = 0; i < 32; i++)
		ds_dbg("CAL_MAC[%d] = %02x ", i, CAL_MAC[i]);

	for (i = 0; i < 32; i++) {
		if (CAL_MAC[i] != MAC_Read_Value[i])
			break;
	}

	if (i != 32) {
		flag_mi_auth_result = 1;
		mi_auth_result = ERROR_UNMATCH_MAC;
		return ERROR_UNMATCH_MAC;
	} else {
		flag_mi_auth_result = 1;
		mi_auth_result = DS_TRUE;
		return DS_TRUE;
	}
}

static int ds28el16_do_authentication(struct ds28e16_data *data)
{
	int result = 0, i;

	ds_log("%s enter\n", __func__);

	set_sched_affinity_to_current();
	for (i = 0; i < GET_VERIFY_RETRY; i++) {
		result = AuthenticateDS28E16(auth_ANON, auth_BDCONST, 0,
			pagenumber, challenge, session_seed, S_secret);
		if (result == DS_TRUE) {
			data->batt_verified = 1;
			set_sched_affinity_to_all();
			ds_log("%s battery verify ok[%d]", __func__, result);
			return result;
		}
	}
	set_sched_affinity_to_all();

	if (result != DS_TRUE) {
		data->batt_verified = 0;
		ds_log("%s battery verify failed[%d]", __func__, result);
	}
	return result;
}


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
	POWER_SUPPLY_PROP_MAXIM_BATT_CYCLE_COUNT,
	POWER_SUPPLY_PROP_AUTHENTIC,
};

static int verify_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct ds28e16_data *data = power_supply_get_drvdata(psy);
	unsigned char pagedata[16] = {0x00};
	unsigned char buf[50];
	int ret;
#ifdef	CONFIG_FACTORY_BUILD
	static bool chip_ok_flag;
#endif

	switch (psp) {
	case POWER_SUPPLY_PROP_VERIFY_MODEL_NAME:
		ret = Read_RomID(mi_romid);
		if (ret == DS_TRUE)
			val->strval = "ds28e16";
		else
			val->strval = "unknown";
		break;
	case POWER_SUPPLY_PROP_AUTHEN_RESULT:
		if (data->batt_verified == DS_TRUE)
			val->intval = true;
		else
			val->intval = false;
		break;
	case POWER_SUPPLY_PROP_PAGENUMBER:
		val->intval = pagenumber;
		break;
	case POWER_SUPPLY_PROP_ROMID:
		ret = Read_RomID(mi_romid);
		ds_err("get RomID = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				mi_romid[0], mi_romid[1], mi_romid[2], mi_romid[3],
				mi_romid[4], mi_romid[5], mi_romid[6], mi_romid[7]);
		memcpy(val->arrayval, mi_romid, 8);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		ret = Read_RomID(mi_romid);
		ds_err("get chip_ok read RomID = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
				mi_romid[0], mi_romid[1], mi_romid[2], mi_romid[3],
				mi_romid[4], mi_romid[5], mi_romid[6], mi_romid[7]);
#ifdef CONFIG_FACTORY_BUILD
		ds_err("CONFIG_FACTORY_BUILD, chip_ok_flag=%d.\n", chip_ok_flag);
		if ((mi_romid[0] == 0x9f) && (mi_romid[6] == 0x04) && ((mi_romid[5] & 0xf0) == 0xf0)) {
			val->intval = true;
			if (data->factory_enable)
				chip_ok_flag = true;
		} else if (chip_ok_flag) {
			val->intval = true;
		} else {
			val->intval = false;
		}
#else
		if ((mi_romid[0] == 0x9f) && (mi_romid[6] == 0x04) && ((mi_romid[5] & 0xf0) == 0xf0))
			val->intval = true;
		else
			val->intval = false;
#endif
		break;
	case POWER_SUPPLY_PROP_DS_STATUS:
		ret = DS28E16_cmd_readStatus(buf);
		memcpy(val->arrayval, buf, 8);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_PAGEDATA:
		ret = DS28E16_cmd_readMemory(pagenumber, buf);
		memcpy(val->arrayval, buf, 16);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_PAGE0_DATA:
		ret = DS28E16_cmd_readMemory(0, buf);
		memcpy(val->arrayval, buf, 16);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_PAGE1_DATA:
		ret = DS28E16_cmd_readMemory(1, buf);
		memcpy(val->arrayval, buf, 16);
		if (ret != DS_TRUE)
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_MAXIM_BATT_CYCLE_COUNT:
		ret = ds28el16_get_page_data_retry(DC_PAGE, pagedata);
		if (ret == DS_TRUE) {
			data->cycle_count = (pagedata[2] << 16) + (pagedata[1] << 8)
						+ pagedata[0];
			val->intval = DC_INIT_VALUE - data->cycle_count;
		}
		break;
	default:
		ds_dbg("unsupported property %d\n", psp);
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
	struct ds28e16_data *data = power_supply_get_drvdata(psy);
	int authen_result;

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
	case POWER_SUPPLY_PROP_MAXIM_BATT_CYCLE_COUNT:
		DS28E16_cmd_decrementCounter();
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		if (val->intval == 1) {
			authen_result = ds28el16_do_authentication(data);
			pr_err("redo authentic: authen_result: %d\n", authen_result);
		}
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
	case POWER_SUPPLY_PROP_MAXIM_BATT_CYCLE_COUNT:
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

#ifdef	CONFIG_FACTORY_BUILD
	pdata->factory_enable = of_property_read_bool(np,
			"mi,factory-enable");
#endif

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
		status = Read_RomID(RomID);

		if (status == DS_TRUE) {
			count++;
			ds_log("Read_RomID success!\n");
		} else {
			ds_log("Read_RomID fail!\n");
		}
		ds_dbg("RomID = %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
		RomID[0], RomID[1], RomID[2], RomID[3],
		RomID[4], RomID[5], RomID[6], RomID[7]);
		Delay_us(1000);
	}
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
		result = DS28E16_cmd_readMemory(pagenumber, pagedata);

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
		result = DS28E16_cmd_readStatus(status);

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

#define VERIFY_PERIOD_S		(5*1000)
#define VERIFY_MAX_COUNT	5
static void battery_verify(struct work_struct *work)
{
	int result, i;
	static int count;
	struct ds28e16_data *data = container_of(work, struct ds28e16_data,
							battery_verify_work.work);

	ds_log("%s enter\n", __func__);
	for (i = 0; i < GET_VERIFY_RETRY; i++) {
		result = AuthenticateDS28E16(auth_ANON, auth_BDCONST, 0,
			pagenumber, challenge, session_seed, S_secret);
		if (result == DS_TRUE)
			break;
	}

	if (result == DS_TRUE) {
		data->batt_verified = 1;
		ds_info("%s batt_verified = 1 \n", __func__);
	} else {
		data->batt_verified = 0;
		if (count < VERIFY_MAX_COUNT) {
			schedule_delayed_work(&data->battery_verify_work,
						msecs_to_jiffies(VERIFY_PERIOD_S));
			ds_info("%s battery verify failed times[%d]", __func__, count);
			count++;
		} else {
			ds_info("%s battery verify failed[%d]", __func__, result);
		}
	}
}

static int ds28e16_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct ds28e16_data *ds28e16_data;

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
	INIT_DELAYED_WORK(&ds28e16_data->battery_verify_work, battery_verify);
	schedule_delayed_work(&ds28e16_data->battery_verify_work, msecs_to_jiffies(0));
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

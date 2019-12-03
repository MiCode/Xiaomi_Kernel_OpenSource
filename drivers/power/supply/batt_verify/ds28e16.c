/*******************************************************************************
 * Copyright (C) 2015 Maxim Integrated Products, Inc., All Rights Reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include "sha384_software.h"
#include <linux/string.h>
#include <linux/random.h>
#include <linux/power_supply.h>
#include "ds28e16.h"
#include "onewire_gpio.h"
#include <linux/pmic-voter.h>

/*---------------------------------------------------------------------------
  -------- DS28E16 Memory functions
  ---------------------------------------------------------------------------*/


/* --------------------------------------------------------------------------
  'Set Page Protection' command

  @param[in] pg
  block to set protection
  @param[in] prot
  protection value to set

  @return
  TRUE - command successful @n
  FALSE - command failed  */
int DS28E16_cmd_setStatus(int page, unsigned char prot)
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

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE*tm,
					read_buf, &read_len, write_len)) {
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

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE,
					read_buf, &read_len, write_len)) {
		if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS)
				return DS_TRUE;
		}
	}

	return DS_FALSE;
}

/* --------------------------------------------------------------------------
  'Compute and Read Page Authentication' command

  @param[in] anon - boolean parameter
  @param[in] pg - Page number   2计数器，0 page0    1page1
  @param[in] challenge
  @param[out] hmac   返回的计算结果32个字节

  @return
  TRUE - command successful @n
  FALSE - command failed
  ----------------------------------------------------------------------------*/
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

	/* Bits 7:5: Anonymous Indicator (ANON).
	   These bits specify whether the device’s ROM ID is used for
	   the SHA3-256 authentication computation. To use the ROM ID,
	   these bits must be 000b. To make the SHA3-256 computation
	   anonymous by replacing the ROM ID with FFh bytes,
	   these bits must be 111b. All other codes are invalid and, if chosen,
	   ause the parameter byte to be invalid.*/
	if (anon)
		write_buf[write_len] = (write_buf[write_len] | 0xE0);

	write_len++;
	/* Authentication Parameter (Byte 2) Always (02h)*/
	write_buf[write_len++] = 0x02;
	memcpy(&write_buf[write_len], challenge, 32);
	write_len += 32;

	ds_dbg("computeReadPageAuthen:\n");
	for (i = 0; i < 35; i++)
		ds_dbg("write_buf[%d] = %02x ", i, write_buf[i]);

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE,
					read_buf, &read_len, write_len)) {
		last_result_byte = read_buf[0];
		if (read_buf[0] == RESULT_SUCCESS) {
			memcpy(hmac, &read_buf[1], 32);
			return DS_TRUE;
		}
	}

	return DS_FALSE;
}

/* --------------------------------------------------------------------------
  'Compute Secret S' command

  @param[in] anon - boolean parameter
  @param[in] bdconst - boolean parameter
  @param[in] pg - Page number   页面
  @param[in] partial secret   32个字节的种子

  @return
  TRUE - command successful @n
  FALSE - command failed
 ---------------------------------------------------------------------------*/
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

	/* Construct message */
	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_COMP_S_SECRET;

	/* Bits 7:5: Anonymous Indicator (ANON). These bits specify
	  whether the device’s ROM ID is used for the SHA3-256 authentication
	  computation. To use the ROM ID, these bits must be 000b. To make the
	  SHA3-256 computation anonymous by replacing the ROM ID with FFh bytes,
	  these bits must be 111b. All other codes are invalid and, if chosen,
	  cause the parameter byte to be invalid.

	  Bit 2: Binding Data Constant (BDCONST). Binding page data is constant 00h's when 1b.
	  However, a valid PAGE# still needs to be provided and should be used for calculations.
	  If 0b, then use PAGE# to load binding page data.

	  Bits 1:0: Memory Page Number (PAGE#). These bits select the page number
	  to be used for binding data. Acceptable values are User Pages (from Page 0 to Page 1).
	*/
	if (bdconst)
		param = param | 0x04;

	if (anon)
		param = param | 0xE0;

	write_buf[write_len] = param;
	write_len++;
	/* Secret Parameter (Byte 2) always 08h */
	write_buf[write_len++] = 0x08;
	/* Partial Secret parameter */
	memcpy(&write_buf[write_len], partial, 32);
	write_len += 32;

	ds_dbg("computeS_Secret:\n");
	for (i = 0; i < 35; i++)
		ds_dbg("write_buf[%d] = %02x ", i, write_buf[i]);

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE,
					read_buf, &read_len, write_len)) {
		last_result_byte = read_buf[0];
		if (last_result_byte == RESULT_SUCCESS)
			return DS_TRUE;
	}

	return DS_FALSE;
}

/*-------------------------------------------------------
  Host Authenticate DS28E16
  -------------------------------------------------------*/
int AuthenticateDS28E16(int anon, int bdconst, int S_Secret_PageNum, int PageNum,
			unsigned char *Challenge, unsigned char *Secret_Seeds, unsigned char *S_Secret)
{
	unsigned char PageData[32], MAC_Read_Value[32], CAL_MAC[32];
	unsigned char status_chip[16];
	unsigned char MAC_Computer_Datainput[128];
	int i = 0;
	int msg_len = 0;
	unsigned char flag = DS_FALSE;

	if (ds28el16_get_page_status_retry(status_chip) == DS_TRUE)
		MANID[0] = status_chip[4];
	else
		return ERROR_R_STATUS;

	if (ds28el16_Read_RomID_retry(ROM_NO) == DS_FALSE)
		return ERROR_R_ROMID;

	/* DS28E16 calculate its session secret */
	flag = DS28E16_cmd_computeS_Secret_retry(anon, bdconst,
					S_Secret_PageNum, Secret_Seeds);
	if (flag == DS_FALSE) {
		ds_err("DS28E16_cmd_computeS_Secret error");
		return ERROR_S_SECRET;
	}

	/* DS28E16 compute its MAC based on above sesson secret */
	flag = DS28E16_cmd_computeReadPageAuthentication_retry(anon, PageNum,
						Challenge, MAC_Read_Value);
	if (flag == DS_FALSE) {
		ds_err("DS28E16_cmd_computeReadPageAuthentication error");
		return ERROR_COMPUTE_MAC;
	}

	ds_dbg("%02x %02x %02x %02x\n", anon, bdconst, S_Secret_PageNum, PageNum);

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

	flag = ds28el16_get_page_data_retry(PageNum, PageData);
	if (flag != DS_TRUE) {
		ds_err("DS28E16_cmd_readMemory error");
		return ERROR_R_PAGEDATA;
	}

	/* insert ROM_NO */
	msg_len = 0;
	if (anon != ANONYMOUS)
		memcpy(MAC_Computer_Datainput, ROM_NO, 8);
	else
		memset(MAC_Computer_Datainput, 0xff, 8);

	msg_len += 8;

	/* insert page data */
	memcpy(&MAC_Computer_Datainput[msg_len], PageData, 16);
	msg_len += 16;
	memset(&MAC_Computer_Datainput[msg_len], 0x00, 16);
	msg_len += 16;

	/* insert Challenge */
	memcpy(&MAC_Computer_Datainput[msg_len], Challenge, 32);
	msg_len += 32;

	/* insert Bind Data Page number */
	MAC_Computer_Datainput[msg_len] = PageNum & 0x03;
	MAC_Computer_Datainput[msg_len] = MAC_Computer_Datainput[msg_len] & 0x03;
	msg_len += 1;

	/* insert MANID */
	memcpy(&MAC_Computer_Datainput[msg_len], MANID, 2);
	msg_len += 2;

	/* calculate the MAC */
	sha3_256_hmac(S_Secret, 32, MAC_Computer_Datainput, msg_len, CAL_MAC);

	ds_dbg("host data:\n");
	for (i = 0; i < 80; i++)
		ds_dbg("MAC_Computer_Datainput[%d] = %02x ", i, MAC_Computer_Datainput[i]);

	ds_dbg("host mac:\n");
	for (i = 0; i < 32; i++)
		ds_dbg("CAL_MAC[%d] = %02x ", i, CAL_MAC[i]);

	/* display the SHA-1 64-byte input message and its MAC result */
	for (i = 0; i < 32; i++) {
		if (CAL_MAC[i] != MAC_Read_Value[i])
			break;
	}

	if (i != 32)
		return ERROR_UNMATCH_MAC; /* failed to pass the authentication */
	else
		return DS_TRUE; /* pass the authentication successfully */
}

/* --------------------------------------------------------------------------
   Calculate a new CRC16 from the input data shorteger.  Return the current
   CRC16 and also update the global variable CRC16. */
unsigned short docrc16(unsigned short data)
{
	data = (data ^ (CRC16 & 0xff)) & 0xff;
	CRC16 >>= 8;

	if (oddparity[data & 0xf] ^ oddparity[data >> 4])
		CRC16 ^= 0xc001;

	data <<= 6;
	CRC16  ^= data;
	data <<= 1;
	CRC16 ^= data;

	return CRC16;
}

/*---------------------------------------------------------------------------
  Return last result byte. Useful if a function fails.
  @return
  Result byte
  --------------------------------------------------------------------------*/
int DS28E16_standard_cmd_flow(unsigned char *write_buf, int delay_ms,
			unsigned char *read_buf, int *read_len, int write_len)
{
	int i;
	unsigned char buf[128];
	int buf_len = 0;
	int expected_read_len = *read_len;
	int ret = DS_FALSE;

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
		ds_log("No slave responce.\n");
		ret = ERROR_NO_DEVICE;
		goto final_reset;
	}

	/* SKIP ROM指令 */
	write_byte(CMD_SKIP_ROM);

	buf[buf_len++] = CMD_START;
	memcpy(&buf[buf_len], write_buf, write_len);
	buf_len += write_len;

	for (i = 0; i < buf_len; i++) {
		write_byte(buf[i]);
	}

	/* TX Receive CRC */
	buf[buf_len++] = read_byte();
	buf[buf_len++] = read_byte();

	/* check CRC16 */
	CRC16 = 0;
	for (i = 0; i < buf_len; i++)
		docrc16(buf[i]);

	if (CRC16 != MAXIM_CRC16_RESULT) {
		ds_log("TX CRC16 failed for cmd.\n");
		ret = DS_FALSE;
		goto final_reset;
	}

	/* check for strong pull-up */
	if (delay_ms > 0) {
		/* Release cmd,send release and strong pull-up */
		write_byte(CMD_RELEASE_BYTE);
		/* now wait for the MAC computation. */
		Delay_us(1000*delay_ms);
	}

	/* Read Dummy byte */
	read_byte();

	/* Read the length byte */
	*read_len = read_byte();

	/* check for special case read */
	if (expected_read_len != *read_len) {
		ds_log("unexpect rx length(%d while %d expected)\n",
						*read_len, expected_read_len);
		ret = DS_FALSE;
		goto final_reset;
	}

	/* construct the read buffer + 2 CRC */
	buf_len = *read_len + 2;

	/* send packet */
	for (i = 0; i < buf_len; i++) {
		buf[i] = read_byte();
	}

	/* check CRC16 */
	CRC16 = 0;
	docrc16(*read_len);
	for (i = 0; i < buf_len; i++)
		docrc16(buf[i]);

	if (CRC16 != MAXIM_CRC16_RESULT) {
		ret = DS_FALSE;
		goto final_reset;
	}

	/* set the read result */
	memcpy(read_buf, buf, *read_len);
	ret = DS_TRUE;

final_reset:
	ow_reset();
	return ret;
}

/*--------------------------------------------------------------------------
  'Write Memory' command

  @param[in] pg
  page number to write
  @param[in] data
  buffer must be at least 32 bytes

  @return
   TRUE - command successful @n
   FALSE - command failed
  --------------------------------------------------------------------------*/

int DS28E16_cmd_writeMemory(int pg, unsigned char *data)
{
	unsigned char write_buf[255];
	unsigned char read_buf[255];
	int write_len = 0;
	int read_len = 1;
	int len_byte = 18;

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

	/* construct the write buffer */
	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_WRITE_MEM;
	write_buf[write_len++] = pg & 0x03;
	memcpy(&write_buf[write_len], data, 16);
	write_len += 16;

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_WRITE*tm,
						read_buf, &read_len, write_len)) {
		/* check result byte */
		if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS)
				return DS_TRUE;
		}
	}

	/* no payload in read buffer or failed command */
	return DS_FALSE;
}

/* --------------------------------------------------------------------------
   'Read Memory' command

   @param[in] pg
   page number to read
   @param[out] data
   buffer length must be at least 32 bytes to hold memory read

  @return
   TRUE - command successful @n
   FALSE - command failed
  ---------------------------------------------------------------------------*/
int DS28E16_cmd_readMemory(int pg, unsigned char *data)
{
	unsigned char write_buf[3];
	unsigned char read_buf[255];
	int write_len = 0;
	int read_len = 33;
	int length_byte = 2;

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

	/* construct the write buffer */
	write_buf[write_len++] = length_byte;
	write_buf[write_len++] = CMD_READ_MEM;
	write_buf[write_len++] = pg & 0x03;

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_READ*tm,
					read_buf, &read_len, write_len)) {
		/* check result byte, implements result byte */
		if (read_len == 33) {
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
		}
	}

	/* no payload in read buffer or failed command */
	return DS_FALSE;
}

/*--------------------------------------------------------------------------
   'Read Status' command

   @param[in] pg
   @param[out] data
   pointer to unsigned char (buffer of length 1) for page protection data

   @return
     TRUE - command successful @n
     FALSE - command failed
  --------------------------------------------------------------------------*/
int DS28E16_cmd_readStatus(unsigned char *data)
{
	unsigned char write_buf[255];
	unsigned char read_buf[255];
	int write_len = 0;
	int len_byte = 1;
	int read_len = 7;

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

	// construct the write buffer
	write_buf[write_len++] = len_byte;
	write_buf[write_len++] = CMD_READ_STATUS;

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_READ*tm,
					read_buf, &read_len, write_len)) {
		/* should always be 1 byte length for protection data */
		if (read_buf[0] == RESULT_SUCCESS) {
			last_result_byte = read_buf[0];
			memcpy(data, &read_buf[1], 8);
			MANID[0] = data[4];
			return DS_TRUE;
		}
	}

	/* no payload in read buffer or failed command */
	return DS_FALSE;
}

/*--------------------------------------------------------------------------
  'Decrement Counter' command

     @return
     DS_TRUE - command successful @n
     DS_FALSE - command failed
  -----------------------------------------------------------------------*/
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

	if (DS28E16_standard_cmd_flow(write_buf, DELAY_DS28E16_EE_READ,
					read_buf, &read_len, write_len)) {
		if (read_len == 1) {
			last_result_byte = read_buf[0];
			if (read_buf[0] == RESULT_SUCCESS)
				return DS_TRUE;
		}
	}
	return DS_FALSE;
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

	if ((ow_reset()) != 0) {
		ds_err("Failed to reset ds28e16!\n");
		return DS_FALSE;
	}

	write_byte(CMD_READ_ROM);
	Delay_us(10);

	for (i = 0; i < 8; i++) {
		RomID[i] = read_byte();
		ds_dbg("RomID[%d] = %02x ", i, RomID[i]);
	}

	crc = crc_low_first(RomID, 7);

	if (crc == RomID[7]) {
		memcpy(ROM_NO, RomID, 8);
		return DS_TRUE;
	} else
		return DS_FALSE;
}

static int ds28el16_Read_RomID_retry(unsigned char *RomID)
{
	int i;

	for (i = 0; i < GET_ROM_ID_RETRY; i++) {
		ds_info("read rom id communication start %d...\n", i);

		if (Read_RomID(RomID) == DS_TRUE)
			return DS_TRUE;
	}
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

		if (DS28E16_cmd_readMemory(page, data) == DS_TRUE)
			return DS_TRUE;
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

int ds28e16_get_status(void)
{
	return ds28e16_detected;
}
EXPORT_SYMBOL(ds28e16_get_status);

static int ds28e16_parse_dts(struct device *dev,
				struct ds28e16_data *pdata)
{
	int rc, val;
	struct device_node *np = dev->of_node;

	pdata->version = 0;
	rc = of_property_read_u32(np, "maxim,version", &val);
	if (rc && (rc != -EINVAL))
		ds_err("Unable to read bootloader address\n");
	else if (rc != -EINVAL)
		pdata->version = val;

	return 0;
}

/* read data from file */
static ssize_t ds28e16_ds_Auth_Result_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	int result, i;
	unsigned char key;
	unsigned char session_seed[MAXIM_SESSION_SEED_LEN];
	unsigned char challenge[MAXIM_RANDOM_LEN] = {0x00};
	unsigned char s_secret[MAXIM_SSECRET_LEN];

	get_random_bytes(&key, 1);
	key = key % MAXIM_SHA3256_LEN;
	memcpy(session_seed, ds28e16_session_seed + key, MAXIM_SESSION_SEED_LEN);
	memcpy(s_secret, ds28e16_s_secret + key, MAXIM_SSECRET_LEN);
	get_random_bytes(challenge, MAXIM_RANDOM_LEN);

	for (i = 0; i < GET_VERIFY_RETRY; i++) {
		result = AuthenticateDS28E16(auth_ANON, auth_BDCONST, 0,
			pagenumber, challenge, session_seed, s_secret);
		if (result == DS_TRUE)
			break;
	}

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
	else if (result == ERROR_R_PAGEDATA)
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate failed : ERROR_R_PAGEDATA!\n");
	else if (result == DS_TRUE)
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate success!!!\n");
	else
		return scnprintf(buf, PAGE_SIZE,
			"Authenticate failed : other reason.\n");
}

static ssize_t ds28e16_ds_romid_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int status;
	unsigned char RomID[8] = {0x00};
	int i = 0;
	int val = 0;

	status = ds28el16_Read_RomID_retry(RomID);
	if (status == DS_TRUE) {
		for (i = 0; i < 8; i++)
			ds_log("RomID[%d] = %02x ", i, RomID[i]);
	}
	for (i = 0; i < 8; i++)
		val += snprintf(buf+val, PAGE_SIZE-val, "RomID[%d] = %02x ", i, RomID[i]);

	return val;
}

static ssize_t ds28e16_ds_pagenumber_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%02x\n", pagenumber);
}

static ssize_t ds28e16_ds_pagenumber_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int buf_int;

	if (sscanf(buf, "%d", &buf_int) != 1)
		return -EINVAL;

	ds_log("new pagenumber = %d\n", buf_int);

	if ((buf_int >= 0) && (buf_int <= 3))
		pagenumber = buf_int;

	return count;
}

static ssize_t ds28e16_ds_pagedata_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	int result;
	unsigned char pagedata[16] = {0x00};
	int i = 0;
	int val = 0;

	result = ds28el16_get_page_data_retry(pagenumber, pagedata);
	if (result == DS_TRUE) {
		for (i = 0; i < 16; i++)
			val += snprintf(buf+val, PAGE_SIZE-val, "pagedata[%d] = %02x ", i, pagedata[i]);
	}
	return val;
}

static ssize_t ds28e16_ds_pagedata_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int result, i;
	unsigned char pagedata[16] = {0x00};
	int val = 0;

	for (i = 0; i < 16; i++)
		val += sscanf(buf+val, "%02hhx", &pagedata[i]);
	if (val != 16)
		return -EINVAL;
	else {
		ds_log("The written pagedata is:");
		for (i = 0; i < 16; i++)
			ds_log("pagedata[%d] = %02hhx ", i, pagedata[i]);
		ds_log("\n");
	}

	result = DS28E16_cmd_writeMemory(pagenumber, pagedata);
	if (result == TRUE)
		ds_log("DS28E16_cmd_writeMemory success!\n");
	else
		ds_log("DS28E16_cmd_writeMemory fail!\n");

	return count;
}

static ssize_t ds28e16_ds_retrytimes_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", attr_trytimes);
}

static ssize_t ds28e16_ds_retrytimes_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int buf_int;

	if (sscanf(buf, "%d", &buf_int) != 1)
		return -EINVAL;

	ds_log("new retry times = %d\n", buf_int);

	if (buf_int > 0)
		attr_trytimes = buf_int;

	return count;
}

static ssize_t ds28e16_ds_session_seed_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	int i, val = 0;

	val = snprintf(buf, PAGE_SIZE, "session seed is:\n");
	for (i = 0; i < 32; i++) {
		val += snprintf(buf+val, PAGE_SIZE-val,
				"session_seed[%d] = %02x ", i, session_seed[i]);
	}

	return val;
}

static ssize_t ds28e16_ds_session_seed_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int i, val = 0;

	for (i = 0; i < 32; i++)
		val += sscanf(buf+val, "%02hhx", &session_seed[i]);
	if (val != 32)
		return -EINVAL;
	else {
		ds_log("The written session seed is:");
		for (i = 0; i < 32; i++)
			ds_log("session_seed[%d] = %02hhx ", i, session_seed[i]);
	}

	return count;
}

static ssize_t ds28e16_ds_challenge_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	int i, val = 0;

	val = snprintf(buf, PAGE_SIZE, "challenge is:\n");
	for (i = 0; i < 32; i++) {
		val += snprintf(buf+val, PAGE_SIZE-val,
				"challenge[%d] = %02x ", i, challenge[i]);
	}

	return val;
}

static ssize_t ds28e16_ds_challenge_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int i, val = 0;

	for (i = 0; i < 32; i++)
		val += sscanf(buf+val, "%02hhx", &challenge[i]);
	if (val != 32)
		return -EINVAL;
	else {
		ds_log("The challenge is:");
		for (i = 0; i < 32; i++)
			ds_log("challenge[%d] = %02hhx ", i, challenge[i]);
		ds_log("\n");
	}

	return count;
}

static ssize_t ds28e16_ds_S_secret_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	int i, val = 0;

	val = snprintf(buf, PAGE_SIZE, "s_secret is:\n");
	for (i = 0; i < 32; i++) {
		val += snprintf(buf+val, PAGE_SIZE-val, "s_secret[%d] = %02x ", i, S_secret[i]);
	}

	return val;
}

static ssize_t ds28e16_ds_S_secret_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int i, val = 0;

	for (i = 0; i < 32; i++)
		val += sscanf(buf+val, "%02hhx", &S_secret[i]);
	if (val != 32)
		return -EINVAL;
	else {
		ds_log("The S_secret is:");
		for (i = 0; i < 32; i++)
			ds_log("S_secret[%d] = %02hhx ", i, S_secret[i]);
		ds_log("\n");
	}

	return count;
}

static ssize_t ds28e16_ds_auth_ANON_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%02x\n", auth_ANON);
}

static ssize_t ds28e16_ds_auth_ANON_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	if (sscanf(buf, "%d", &auth_ANON) != 1)
		return -EINVAL;

	return count;
}

static ssize_t ds28e16_ds_auth_BDCONST_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%02x\n", auth_BDCONST);
}

static ssize_t ds28e16_ds_auth_BDCONST_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	if (sscanf(buf, "%d", &auth_BDCONST) != 1)
		return -EINVAL;

	return count;
}

static ssize_t ds28e16_ds_read_page_status_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	int result;
	unsigned char status[16] = {0x00};
	int i = 0;
	int val = 0;

	result = ds28el16_get_page_status_retry(status);
	if (result == DS_TRUE) {
		for (i = 0; i < 16; i++)
			val += snprintf(buf+val, PAGE_SIZE-val, "status[%d] = %02x ", i, status[i]);
	}
	return val;
}

static DEVICE_ATTR(ds_readstatus, S_IRUGO,
		ds28e16_ds_read_page_status_show, NULL);
static DEVICE_ATTR(ds_romid, S_IRUGO,
		ds28e16_ds_romid_show, NULL);
static DEVICE_ATTR(ds_pagenumber, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_pagenumber_show,
		ds28e16_ds_pagenumber_store);
static DEVICE_ATTR(ds_pagedata, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_pagedata_show,
		ds28e16_ds_pagedata_store);
static DEVICE_ATTR(ds_time, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_retrytimes_show,
		ds28e16_ds_retrytimes_store);
static DEVICE_ATTR(ds_session_seed, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_session_seed_show,
		ds28e16_ds_session_seed_store);
static DEVICE_ATTR(ds_challenge, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_challenge_show,
		ds28e16_ds_challenge_store);
static DEVICE_ATTR(ds_S_secret, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_S_secret_show,
		ds28e16_ds_S_secret_store);
static DEVICE_ATTR(ds_auth_ANON, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_auth_ANON_show,
		ds28e16_ds_auth_ANON_store);
static DEVICE_ATTR(ds_auth_BDCONST, S_IRUGO | S_IWUSR | S_IWGRP,
		ds28e16_ds_auth_BDCONST_show,
		ds28e16_ds_auth_BDCONST_store);
static DEVICE_ATTR(ds_Auth_Result, S_IRUGO,
		ds28e16_ds_Auth_Result_show, NULL);

static enum power_supply_property batt_verify_props[] = {
	POWER_SUPPLY_PROP_MAXIM_BATT_VERIFY,
	POWER_SUPPLY_PROP_MAXIM_BATT_CYCLE_COUNT,
};

static int maxim_batt_security_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct ds28e16_data *data = power_supply_get_drvdata(psy);
	unsigned char pagedata[16] = {0x00};
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_MAXIM_BATT_VERIFY:
		val->intval = data->batt_verified;
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
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int maxim_batt_security_set_property(struct power_supply *psy,
			enum power_supply_property psp,
			const union power_supply_propval *val)
{
	struct ds28e16_data *data = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_MAXIM_BATT_VERIFY:
		data->batt_verified = !!val->intval;
		break;
	case POWER_SUPPLY_PROP_MAXIM_BATT_CYCLE_COUNT:
		DS28E16_cmd_decrementCounter();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int maxim_batt_security_is_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_MAXIM_BATT_VERIFY:
	case POWER_SUPPLY_PROP_MAXIM_BATT_CYCLE_COUNT:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc batt_security_psy_desc = {
	.name = "maxim-ds28e16",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = batt_verify_props,
	.num_properties = ARRAY_SIZE(batt_verify_props),
	.get_property = maxim_batt_security_get_property,
	.set_property = maxim_batt_security_set_property,
	.property_is_writeable = maxim_batt_security_is_writeable,
};

#define VERIFY_PERIOD_S		(10*1000)
#define VERIFY_MAX_COUNT	5
static void battery_verify(struct work_struct *work)
{
	int result, i;
	int rc = 0;
	unsigned char key = 0;
	int page_num = PAGE1;
	static int count;
	union power_supply_propval prop = {0, };
	unsigned char session_seed[MAXIM_SESSION_SEED_LEN];
	unsigned char challenge[MAXIM_RANDOM_LEN] = {0x00};
	unsigned char s_secret[MAXIM_SSECRET_LEN];
	struct ds28e16_data *data = container_of(work, struct ds28e16_data,
							battery_verify_work.work);

	get_random_bytes(&key, 1);
	key = key % MAXIM_SHA3256_LEN;
	memcpy(session_seed, ds28e16_session_seed + key, MAXIM_SESSION_SEED_LEN);
	memcpy(s_secret, ds28e16_s_secret + key, MAXIM_SSECRET_LEN);
	get_random_bytes(challenge, MAXIM_RANDOM_LEN);

	for (i = 0; i < GET_VERIFY_RETRY; i++) {
		result = AuthenticateDS28E16(auth_ANON, auth_BDCONST, 0,
				page_num, challenge, session_seed, s_secret);
		if (result == DS_TRUE)
			break;
	}

	if (!data->bms_psy)
		data->bms_psy = power_supply_get_by_name("bms");

	if (result == DS_TRUE) {
		data->batt_verified = 1;
		if (data->bms_psy) {
			prop.intval = true;
			power_supply_set_property(data->bms_psy,
					POWER_SUPPLY_PROP_AUTHENTIC, &prop);
			ds_log("%s battery verify success!", __func__);
			if (!data->usb_psy)
				data->usb_psy = power_supply_get_by_name("usb");
			if (data->usb_psy) {
				rc = power_supply_get_property(data->usb_psy,
						POWER_SUPPLY_PROP_PD_AUTHENTICATION, &prop);
				if (rc < 0) {
					pr_err("Get fastcharge mode status failed, rc=%d\n", rc);
				} else if (prop.intval) {
					pr_err("pd_authentication=%d\n", prop.intval);
					prop.intval = true;
					rc = power_supply_set_property(data->usb_psy,
							POWER_SUPPLY_PROP_FASTCHARGE_MODE, &prop);
				}
			}
		}
	} else {
		data->batt_verified = 0;
		if (count < VERIFY_MAX_COUNT) {
			schedule_delayed_work(&data->battery_verify_work,
						msecs_to_jiffies(VERIFY_PERIOD_S));
			ds_log("%s battery verify failed times[%d]", __func__, count);
			count++;
		} else {
			if (data->bms_psy) {
				prop.intval = false;
				power_supply_set_property(data->bms_psy,
						POWER_SUPPLY_PROP_AUTHENTIC, &prop);
			}
			ds_log("%s battery verify failed[%d]", __func__, result);
		}
	}

	if (data->fcc_votable == NULL) {
		data->fcc_votable = find_votable("FCC");
		if (data->fcc_votable == NULL)
			ds_log("Couldn't find FCC votable\n");
	} else if (data->batt_verified) {
		vote(data->fcc_votable, BATT_VERIFY_VOTER, false, 0);
	} else {
		vote(data->fcc_votable, BATT_VERIFY_VOTER, true, BATT_UNVERIFY_CURR);
	}

	rerun_election(data->fcc_votable);
	power_supply_changed(data->batt_verify_psy);
}

static int ds28e16_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct ds28e16_data *ds28e16_data;
	struct kobject *p;
	struct power_supply_config psy_cfg = {};

	ds_log("%s entry.", __func__);

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

		retval = ds28e16_parse_dts(&pdev->dev, ds28e16_data);
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

	g_ds28e16_data = ds28e16_data;
	psy_cfg.drv_data = ds28e16_data;
	ds28e16_data->pdev = pdev;
	platform_set_drvdata(pdev, ds28e16_data);
	INIT_DELAYED_WORK(&ds28e16_data->battery_verify_work, battery_verify);
	schedule_delayed_work(&ds28e16_data->battery_verify_work,
					msecs_to_jiffies(VERIFY_PERIOD_S));

	ds28e16_data->batt_verify_psy = power_supply_register(&pdev->dev,
						&batt_security_psy_desc, &psy_cfg);
	if (IS_ERR(ds28e16_data->batt_verify_psy)) {
		power_supply_unregister(ds28e16_data->batt_verify_psy);
		return PTR_ERR(ds28e16_data->batt_verify_psy);
	}

	// create device node
	ds28e16_data->dev = device_create(ds28e16_class,
				pdev->dev.parent->parent,
				ds28e16_major, ds28e16_data, "ds28e16_ctrl");
	if (IS_ERR(ds28e16_data->dev)) {
		ds_err("Failed to create interface device\n");
		goto ds28e16_interface_dev_create_err;
	}

	p = &ds28e16_data->dev->kobj;

	// create attr file
	retval = sysfs_create_file(p, &dev_attr_ds_Auth_Result.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_Auth_Result_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_session_seed.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_session_seed_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_challenge.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_challenge_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_S_secret.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_S_secret_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_auth_ANON.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_auth_ANON_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_auth_BDCONST.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_auth_BDCONST_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_pagenumber.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_pagenumber_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_pagedata.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_pagedata_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_readstatus.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_readstatus_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_romid.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_romid_err;
	}

	retval = sysfs_create_file(p, &dev_attr_ds_time.attr);
	if (retval < 0) {
		ds_err("Failed to create sysfs attr file\n");
		goto ds28e16_sysfs_ds_time_err;
	}

	retval = sysfs_create_link(&ds28e16_data->dev->kobj, &pdev->dev.kobj,
								"pltdev");
	if (retval) {
		ds_err("Failed to create sysfs link\n");
		goto ds28e16_syfs_create_link_err;
	}

	return 0;
ds28e16_syfs_create_link_err:
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_ds_romid.attr);
ds28e16_sysfs_ds_time_err:
ds28e16_sysfs_ds_romid_err:
ds28e16_sysfs_ds_readstatus_err:
ds28e16_sysfs_ds_pagedata_err:
ds28e16_sysfs_ds_pagenumber_err:
ds28e16_sysfs_ds_auth_BDCONST_err:
ds28e16_sysfs_ds_auth_ANON_err:
ds28e16_sysfs_ds_S_secret_err:
ds28e16_sysfs_ds_challenge_err:
ds28e16_sysfs_ds_session_seed_err:
ds28e16_sysfs_ds_Auth_Result_err:
	device_destroy(ds28e16_class, ds28e16_major);
ds28e16_interface_dev_create_err:
ds28e16_parse_dt_err:
	kfree(ds28e16_data);
	return retval;
}

static int ds28e16_remove(struct platform_device *pdev)
{
	struct ds28e16_data *ds28e16_data = platform_get_drvdata(pdev);

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
	int retval;

	ds_log("%s entry.", __func__);

	ds28e16_detected = false;

	ds28e16_class = class_create(THIS_MODULE, "ds28e16");
	if (IS_ERR(ds28e16_class)) {
		ds_err("coudn't create class");
		return PTR_ERR(ds28e16_class);
	}

	ds28e16_major = register_chrdev(0, "ds28e16_ctrl", &ds28e16_dev_fops);
	if (ds28e16_major < 0) {
		ds_err("failed to allocate char dev\n");
		retval = ds28e16_major;
		goto class_unreg;
	}

	return platform_driver_register(&ds28e16_driver);

class_unreg:
	class_destroy(ds28e16_class);
	return retval;
}

static void __exit ds28e16_exit(void)
{
	ds_log("%s entry.", __func__);
	platform_driver_unregister(&ds28e16_driver);

	unregister_chrdev(ds28e16_major, "ds28e16ctrl");
	class_destroy(ds28e16_class);
}

module_init(ds28e16_init);
module_exit(ds28e16_exit);

MODULE_AUTHOR("xiaomi Inc.");
MODULE_DESCRIPTION("ds28e16 driver");
MODULE_LICENSE("GPL");

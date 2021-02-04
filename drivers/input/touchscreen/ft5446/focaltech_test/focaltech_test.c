/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2018, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

/************************************************************************
*
* File Name: focaltech_test.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-01
*
* Modify:
*
* Abstract: create char device and proc node for  the comm between APK and TP
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_test.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_test_data test_data;

struct test_funcs *test_funcs_list[] = {
	&test_func_ft5x46,
};

struct test_ic_type ic_types[] = {
	{"FT5X36", 6, IC_FT5X36},
	{"FT5X22", 6, IC_FT5X46},
	{"FT5X46", 6, IC_FT5X46},
	{"FT5X46i", 7, IC_FT5X46I},
	{"FT5526", 6, IC_FT5526},
	{"FT3X17", 6, IC_FT3X17},
	{"FT5436", 6, IC_FT5436},
	{"FT3X27", 6, IC_FT3X27},
	{"FT5526i", 7, IC_FT5526I},
	{"FT5416", 6, IC_FT5416},
	{"FT5426", 6, IC_FT5426},
	{"FT5435", 6, IC_FT5435},
	{"FT7681", 6, IC_FT7681},
	{"FT7661", 6, IC_FT7661},
	{"FT7511", 6, IC_FT7511},
	{"FT7421", 6, IC_FT7421},
	{"FT7311", 6, IC_FT7311},
	{"FT3327DQQ-001", 13, IC_FT3327DQQ_001},
	{"FT5446DQS-W01", 13, IC_FT5446DQS_W01},
	{"FT5446DQS-W02", 13, IC_FT5446DQS_W02},
	{"FT5446DQS-002", 13, IC_FT5446DQS_002},
	{"FT5446DQS-Q02", 13, IC_FT5446DQS_Q02},

	{"FT6X36", 6, IC_FT6X36},
	{"FT3X07", 6, IC_FT3X07},
	{"FT6416", 6, IC_FT6416},
	{"FT6336G/U", 9, IC_FT6426},
	{"FT6236U", 7, IC_FT6236U},
	{"FT6436U", 7, IC_FT6436U},
	{"FT3267", 6, IC_FT3267},
	{"FT3367", 6, IC_FT3367},
	{"FT7401", 6, IC_FT7401},
	{"FT3407U", 7, IC_FT3407U},

	{"FT5822", 6, IC_FT5822},
	{"FT5626", 6, IC_FT5626},
	{"FT5726", 6, IC_FT5726},
	{"FT5826B", 7, IC_FT5826B},
	{"FT3617", 6, IC_FT3617},
	{"FT3717", 6, IC_FT3717},
	{"FT7811", 6, IC_FT7811},
	{"FT5826S", 7, IC_FT5826S},
	{"FT3517U", 7, IC_FT3517U},

	{"FT3518", 6, IC_FT3518},
	{"FT3558", 6, IC_FT3558},

	{"FT8606", 6, IC_FT8606},

	{"FT8716U", 7, IC_FT8716U},
	{"FT8716F", 7, IC_FT8716F},
	{"FT8716", 6, IC_FT8716},
	{"FT8613", 6, IC_FT8613},

	{"FT3C47U", 7, IC_FT3C47U},

	{"FT8607U", 7, IC_FT8607U},
	{"FT8607", 6, IC_FT8607},

	{"FT8736", 6, IC_FT8736},

	{"FT3D47", 6, IC_FT3D47},

	{"FTE716", 6, IC_FTE716},

	{"FT8201", 6, IC_FT8201},

	{"FT8006M", 7, IC_FT8006M},
	{"FT7250", 6, IC_FT7250},

	{"FT8006U", 7, IC_FT8006U},

	{"FT8719", 6, IC_FT8719},
	{"FT8615", 6, IC_FT8615},

	{"FT8739", 6, IC_FT8739},
};

/*Add by HQ-102007757 for ITO normalized test*/
#ifdef ITO_TEST_NORMALIZED_NODE
static struct proc_dir_entry *fts_android_touch_proc;
static struct proc_dir_entry *fts_ito_test_proc;
#endif

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
* functions body
*****************************************************************************/
void sys_delay(int ms)
{
	msleep(ms);
}

int focal_abs(int value)
{
	if (value < 0)
		value = 0 - value;

	return value;
}

void *fts_malloc(size_t size)
{
	return vmalloc(size);
}

void fts_free_proc(void *p)
{
	return vfree(p);
}

/********************************************************************
 * test i2c read/write interface
 *******************************************************************/
int fts_test_i2c_read(u8 *writebuf, int writelen, u8 *readbuf, int readlen)
{
	int ret = 0;
#if 1
	if (NULL == fts_data) {
		FTS_TEST_ERROR("fts_data is null, no test");
		return -EINVAL;
	}
	ret = fts_i2c_read(fts_data->client, writebuf, writelen, readbuf, readlen);
#else
	ret = fts_i2c_read(writebuf, writelen, readbuf, readlen);
#endif

	if (ret < 0)
		return ret;
	else
		return 0;
}

int fts_test_i2c_write(u8 *writebuf, int writelen)
{
	int ret = 0;
#if 1
	if (NULL == fts_data) {
		FTS_TEST_ERROR("fts_data is null, no test");
		return -EINVAL;
	}
	ret = fts_i2c_write(fts_data->client, writebuf, writelen);
#else
	ret = fts_i2c_write(writebuf, writelen);
#endif

	if (ret < 0)
		return ret;
	else
		return 0;
}

int read_reg(u8 addr, u8 *val)
{
	return fts_test_i2c_read(&addr, 1, val, 1);
}

int write_reg(u8 addr, u8 val)
{
	int ret;
	u8 cmd[2] = {0};

	cmd[0] = addr;
	cmd[1] = val;
	ret = fts_test_i2c_write(cmd, 2);

	return ret;
}

/********************************************************************
 * test global function enter work/factory mode
 *******************************************************************/
int enter_work_mode(void)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;
	int j = 0;

	FTS_TEST_FUNC_ENTER();

	ret = read_reg(DEVIDE_MODE_ADDR, &mode);
	if ((0 == ret) && (((mode >> 4) & 0x07) == 0x00))
		return 0;

	for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = write_reg(DEVIDE_MODE_ADDR, 0x00);
		if (0 == ret) {
			for (j = 0; j < 20; j++) {
				ret = read_reg(DEVIDE_MODE_ADDR, &mode);
				if ((0 == ret) && (((mode >> 4) & 0x07) == 0x00))
					return 0;
				else
					sys_delay(FACTORY_TEST_DELAY);
			}
		}

		sys_delay(50);
	}

	if (i >= ENTER_WORK_FACTORY_RETRIES) {
		FTS_TEST_ERROR("Enter work mode fail");
		return -EIO;
	}

	FTS_TEST_FUNC_EXIT();
	return 0;
}

int enter_factory_mode(void)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;
	int j = 0;

	ret = read_reg(DEVIDE_MODE_ADDR, &mode);
	if ((0 == ret) && (((mode >> 4) & 0x07) == 0x04))
		return 0;

	for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = write_reg(DEVIDE_MODE_ADDR, 0x40);
		if (0 == ret) {
			for (j = 0; j < 20; j++) {
				ret = read_reg(DEVIDE_MODE_ADDR, &mode);
				if ((0 == ret) && (((mode >> 4) & 0x07) == 0x04)) {
					sys_delay(200);
					return 0;
				} else
					sys_delay(FACTORY_TEST_DELAY);
			}
		}

		sys_delay(50);
	}

	if (i >= ENTER_WORK_FACTORY_RETRIES) {
		FTS_TEST_ERROR("Enter factory mode fail");
		return -EIO;
	}

	return 0;
}

/************************************************************************
* Name: fts_i2c_read_write
* Brief:  Write/Read Data by IIC
* Input: writebuf, writelen, readlen
* Output: readbuf
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
u8 fts_i2c_read_write(unsigned char *writebuf, int  writelen, unsigned char *readbuf, int readlen)
{
	int ret;

	if (readlen > 0) {
		ret = fts_test_i2c_read(writebuf, writelen, readbuf, readlen);
	} else {
		ret = fts_test_i2c_write(writebuf, writelen);
	}

	if (ret >= 0)
		return ERROR_CODE_OK;
	else
		return ERROR_CODE_COMM_ERROR;
}

/*
 * read_mass_data - read rawdata/short test data
 * addr - register addr which read data from
 * byte_num - read data length, unit:byte
 * buf - save data
 *
 * return 0 if read data succuss, otherwise return error code
 */
int read_mass_data(u8 addr, int byte_num, int *buf)
{
	int ret = 0;
	int i = 0;
	u8 reg_addr = 0;
	u8 *data = NULL;
	int read_num = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;

	data = (u8 *)fts_malloc(byte_num * sizeof(u8));
	if (NULL == data) {
		FTS_TEST_SAVE_ERR("rawdata buffer malloc fail\n");
		return -ENOMEM;
	}

	/* read rawdata buffer */
	FTS_TEST_INFO("mass data len:%d", byte_num);
	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;

	if (byte_num < BYTES_PER_TIME) {
		read_num = byte_num;
	} else {
		read_num = BYTES_PER_TIME;
	}
	FTS_TEST_INFO("packet num:%d, remainder:%d", packet_num, packet_remainder);

	reg_addr = addr;
	ret = fts_test_i2c_read(&reg_addr, 1, data, read_num);
	if (ret) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		goto READ_MASSDATA_ERR;
	}

	for (i = 1; i < packet_num; i++) {
		offset = read_num * i;
		if ((i == (packet_num - 1)) && packet_remainder) {
			read_num = packet_remainder;
		}

		ret = fts_test_i2c_read(NULL, 0, data + offset, read_num);
		if (ret) {
			FTS_TEST_SAVE_ERR("read much rawdata fail\n");
			goto READ_MASSDATA_ERR;
		}
	}

	for (i = 0; i < byte_num; i = i + 2) {
		buf[i >> 1] = (int)(((int)(data[i]) << 8) + data[i + 1]);
	}

READ_MASSDATA_ERR:
	if (data) {
		fts_free(data);
	}

	return ret;
}

/*
 * chip_clb_incell - auto clb
 */
int chip_clb_incell(void)
{
	int ret = 0;
	u8 val = 0;
	int times = 0;

	/* start clb */
	ret = write_reg(FACTORY_REG_CLB, 0x04);
	if (ret) {
		FTS_TEST_SAVE_ERR("write start clb fail\n");
		return ret;
	}

	while (times++ < FACTORY_TEST_RETRY) {
		sys_delay(FACTORY_TEST_RETRY_DELAY);
		ret = read_reg(FACTORY_REG_CLB, &val);
		if ((0 == ret) && (0x02 == val)) {
			/* clb ok */
			break;
		} else
			FTS_TEST_DBG("reg%x=%x,retry:%d", FACTORY_REG_CLB, val, times);
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("chip clb timeout\n");
		return -EIO;
	}

	return 0;
}

/*
 * start_scan_incell - start to scan a frame for incell ic
 */
int start_scan_incell(void)
{
	int ret = 0;
	u8 val = 0x00;
	int times = 0;

	ret = read_reg(DEVIDE_MODE_ADDR, &val);
	if (ret) {
		FTS_TEST_SAVE_ERR("read device mode fail\n");
		return ret;
	}

	/* Top bit position 1, start scan */
	val |= 0x80;
	ret = write_reg(DEVIDE_MODE_ADDR, val);
	if (ret) {
		FTS_TEST_SAVE_ERR("write device mode fail\n");
		return ret;
	}

	while (times++ < FACTORY_TEST_RETRY) {
		/* Wait for the scan to complete */
		sys_delay(FACTORY_TEST_DELAY);

		ret = read_reg(DEVIDE_MODE_ADDR, &val);
		if ((0 == ret) && (0 == (val >> 7))) {
			break;
		} else
			FTS_TEST_DBG("reg%x=%x,retry:%d", DEVIDE_MODE_ADDR, val, times);
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("start scan timeout\n");
		return -EIO;
	}

	return 0;
}

/*
 * wait_state_update - wait fw status update
 */
int wait_state_update(void)
{
	int ret = 0;
	int times = 0;
	u8 state = 0xFF;

	while (times++ < FACTORY_TEST_RETRY) {
		sys_delay(FACTORY_TEST_RETRY_DELAY);
		/* Wait register status update */
		state = 0xFF;
		ret = read_reg(FACTORY_REG_PARAM_UPDATE_STATE, &state);
		if ((0 == ret) && (0x00 == state))
			break;
		else
			FTS_TEST_DBG("reg%x=%x,retry:%d", \
						 FACTORY_REG_PARAM_UPDATE_STATE, state, times);
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("Wait State Update fail\n");
		return -EIO;
	}

	return 0;
}

/*
 * get_rawdata_incell - read rawdata/diff for incell ic
 */
int get_rawdata_incell(int *data)
{
	int ret = 0;
	u8 tx_num = 0;
	u8 rx_num = 0;
	u8 key_num = 0;
	int va_num = 0;

	FTS_TEST_FUNC_ENTER();
	/* enter into factory mode */
	ret = enter_factory_mode();
	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Enter Factory Mode\n");
		return ret;
	}

	/* get tx/rx num */
	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	va_num = tx_num * rx_num;
	key_num = test_data.screen_param.key_num_total;

	/* start to scan a frame */
	ret = start_scan_incell();
	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Scan ...\n");
		return ret;
	}

	/* Read RawData for va Area */
	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret) {
		FTS_TEST_SAVE_ERR("wirte AD to reg01 fail\n");
		return ret;
	}
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, va_num * 2, data);
	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Get RawData of channel.\n");
		return ret;
	}

	/* Read RawData for key Area */
	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAE);
	if (ret) {
		FTS_TEST_SAVE_ERR("wirte AE to reg01 fail\n");
		return ret;
	}
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, key_num * 2, data + va_num);
	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Get RawData of keys.\n");
		return ret;
	}

	FTS_TEST_FUNC_EXIT();
	return 0;
}

/*
 * get_cb_incell - get cb data for incell IC
 */
int get_cb_incell(u16 saddr, int byte_num, int *cb_buf)
{
	int ret = 0;
	int i = 0;
	u8 cb_addr = 0;
	u8 addr_h = 0;
	u8 addr_l = 0;
	int read_num = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int addr = 0;
	u8 *data = NULL;

	data = (u8 *)fts_malloc(byte_num * sizeof(u8));
	if (NULL == data) {
		FTS_TEST_SAVE_ERR("cb buffer malloc fail\n");
		return -ENOMEM;
	}

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;
	read_num = BYTES_PER_TIME;

	FTS_TEST_INFO("cb packet:%d,remainder:%d", packet_num, packet_remainder);
	cb_addr = FACTORY_REG_CB_ADDR;
	for (i = 0; i < packet_num; i++) {
		offset = read_num * i;
		addr = saddr + offset;
		addr_h = (addr >> 8) & 0xFF;
		addr_l = addr & 0xFF;
		if ((i == (packet_num - 1)) && packet_remainder) {
			read_num = packet_remainder;
		}

		ret = write_reg(FACTORY_REG_CB_ADDR_H, addr_h);
		if (ret) {
			FTS_TEST_SAVE_ERR("write cb addr high fail\n");
			goto TEST_CB_ERR;
		}
		ret = write_reg(FACTORY_REG_CB_ADDR_L, addr_l);
		if (ret) {
			FTS_TEST_SAVE_ERR("write cb addr low fail\n");
			goto TEST_CB_ERR;
		}

		ret = fts_test_i2c_read(&cb_addr, 1, data + offset, read_num);
		if (ret) {
			FTS_TEST_SAVE_ERR("read cb fail\n");
			goto TEST_CB_ERR;
		}
	}

	for (i = 0; i < byte_num; i++) {
		cb_buf[i] = data[i];
	}

TEST_CB_ERR:
	fts_free(data);
	return ret;
}

/*
 * weakshort_get_adc_data_incell - get short(adc) data for incell IC
 */
int weakshort_get_adc_data_incell(u8 retval, u8 ch_num, int byte_num, int *adc_buf)
{
	int ret = 0;
	int times = 0;
	u8 short_state = 0;

	FTS_TEST_FUNC_ENTER();

	/* Start ADC sample */
	ret = write_reg(FACTORY_REG_SHORT_TEST_EN, 0x01);
	if (ret) {
		FTS_TEST_SAVE_ERR("start short test fail\n");
		goto ADC_ERROR;
	}
	sys_delay(ch_num * FACTORY_TEST_DELAY);

	for (times = 0; times < FACTORY_TEST_RETRY; times++) {
		ret = read_reg(FACTORY_REG_SHORT_TEST_STATE, &short_state);
		if ((0 == ret) && (retval == short_state))
			break;
		else
			FTS_TEST_DBG("reg%x=%x,retry:%d",
						 FACTORY_REG_SHORT_TEST_STATE, short_state, times);

		sys_delay(FACTORY_TEST_RETRY_DELAY);
	}
	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("short test timeout, ADC data not OK\n");
		ret = -EIO;
		goto ADC_ERROR;
	}

	ret = read_mass_data(FACTORY_REG_SHORT_ADDR, byte_num, adc_buf);
	if (ret) {
		FTS_TEST_SAVE_ERR("get short(adc) data fail\n");
	}

ADC_ERROR:
	FTS_TEST_FUNC_EXIT();
	return ret;
}

/*
 * show_data_incell - show and save test data to testresult.txt
 */
void show_data_incell(int *data, bool include_key)
{
	int row = 0;
	int col = 0;
	int tx_num = test_data.screen_param.tx_num;
	int rx_num = test_data.screen_param.rx_num;
	int key_num = test_data.screen_param.key_num_total;

	FTS_TEST_SAVE_INFO("\nVA Channels: ");
	for (row = 0; row < tx_num; row++) {
		FTS_TEST_SAVE_INFO("\nCh_%02d:  ", row + 1);
		for (col = 0; col < rx_num; col++) {
			FTS_TEST_SAVE_INFO("%5d, ", data[row * rx_num + col]);
		}
	}

	if (include_key) {
		FTS_TEST_SAVE_INFO("\nKeys:   ");
		for (col = 0; col < key_num; col++) {
			FTS_TEST_SAVE_INFO("%5d, ",  data[rx_num * tx_num + col]);
		}
	}
	FTS_TEST_SAVE_INFO("\n");
}

/*
 * compare_data_incell - check data in range or not
 *
 * return true if check pass, or return false
 */
bool compare_data_incell(int *data, int min, int max, int vk_min, int vk_max, bool include_key)
{
	int row = 0;
	int col = 0;
	int value = 0;
	bool tmp_result = true;
	int tx_num = test_data.screen_param.tx_num;
	int rx_num = test_data.screen_param.rx_num;
	int key_num = test_data.screen_param.key_num_total;

	/* VA area */
	for (row = 0; row < tx_num; row++) {
		for (col = 0; col < rx_num; col++) {
			if (0 == test_data.incell_detail_thr.invalid_node[row * rx_num + col])
				continue; /* Invalid Node */
			value = data[row * rx_num + col];
			if (value < min || value > max) {
				tmp_result = false;
				FTS_TEST_SAVE_INFO("test failure. Node=(%d, %d), \
					Get_value=%d,  Set_Range=(%d, %d)\n", \
								   row + 1, col + 1, value, min, max);
			}
		}
	}
	/* Key area */
	if (include_key) {
		if (test_data.screen_param.key_flag) {
			key_num = test_data.screen_param.key_num;
		}
		row = test_data.screen_param.tx_num;
		for (col = 0; col < key_num; col++) {
			if (0 == test_data.incell_detail_thr.invalid_node[tx_num * rx_num + col])
				continue; /* Invalid Node */
			value = data[rx_num * tx_num + col];
			if (value < vk_min || value > vk_max) {
				tmp_result = false;
				FTS_TEST_SAVE_INFO("test failure. Node=(%d, %d), \
					Get_value=%d,  Set_Range=(%d, %d)\n", \
								   row + 1, col + 1, value, vk_min, vk_max);
			}
		}
	}

	return tmp_result;
}


/************************************************************************
* Name: compare_detailthreshold_data_incell
* Brief:  compare_detailthreshold_data_incell
* Input: none
* Output: none
* Return: none.
***********************************************************************/
bool compare_detailthreshold_data_incell(int *data, int *data_min, int *data_max, bool include_key)
{
	int row, col;
	int value;
	bool tmp_result = true;
	int tmp_min, tmp_max;
	int rx_num = test_data.screen_param.rx_num;
	int tx_num = test_data.screen_param.tx_num;
	int key_num = test_data.screen_param.key_num_total;
	// VA
	for (row = 0; row < tx_num; row++) {
		for (col = 0; col < rx_num; col++) {
			if (test_data.incell_detail_thr.invalid_node[row * rx_num + col] == 0)
				continue; //Invalid Node
			tmp_min = data_min[row * rx_num + col];
			tmp_max = data_max[row * rx_num + col];
			value = data[row * rx_num + col];
			if (value < tmp_min || value > tmp_max) {
				tmp_result = false;
				FTS_TEST_SAVE_INFO(" \n test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d)", \
								   row + 1, col + 1, value, tmp_min, tmp_max);
			}
		}
	}
	// Key
	if (include_key) {
		if (test_data.screen_param.key_flag) {
			key_num = test_data.screen_param.key_num;
		}
		row = test_data.screen_param.tx_num;
		for (col = 0; col < key_num; col++) {
			if (test_data.incell_detail_thr.invalid_node[rx_num * tx_num + col] == 0)
				continue; //Invalid Node
			tmp_min = data_min[rx_num * tx_num + col];
			tmp_max = data_max[rx_num * tx_num + col];
			value = data[rx_num * tx_num + col];
			if (value < tmp_min || value > tmp_max) {
				tmp_result = false;
				FTS_TEST_SAVE_INFO(" \n test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d)", \
								   row + 1, col + 1, value, tmp_min, tmp_max);
			}
		}
	}

	return tmp_result;
}

/*
 * save_testdata_incell - save data to testdata.csv
 */
void save_testdata_incell(int *data, char *test_num, int index, u8 row, u8 col, u8 item_count)
{
	int len = 0;
	int i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	/* Save Msg */
	len = snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER, \
				   "%s, %d, %d, %d, %d, %d, ", \
				   test_num, test_data.test_item_code, row, col, \
				   test_data.start_line, item_count);

	memcpy(test_data.msg_area_line2 + test_data.len_msg_area_line2, test_data.tmp_buffer, len);
	test_data.len_msg_area_line2 += len;

	test_data.start_line += row;
	test_data.test_data_count++;

	/* Save Data */
	for (i = 0 + index; (i < row + index) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < col) && (j < RX_NUM_MAX); j++) {
			if (j == (col - 1)) {
				/* The Last Data of the row, add "\n" */
				len = snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER, \
							   "%d, \n", data[col * (i + index) + j]);
			} else {
				len = snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER, \
							   "%d, ", data[col * (i + index) + j]);
			}

			memcpy(test_data.store_data_area + test_data.len_store_data_area, test_data.tmp_buffer, len);
			test_data.len_store_data_area += len;
		}
	}

	FTS_TEST_FUNC_EXIT();
}

/*
 * fts_ic_table_get_ic_code_from_ic_name - get ic code from ic name
 */
u32 fts_ic_table_get_ic_code_from_ic_name(char *ic_name)
{
	int i = 0;
	int type_size = 0;

	type_size = sizeof(ic_types) / sizeof(ic_types[0]);
	FTS_TEST_INFO("FTS ic_name = %s, ic_types[0].ic_name = %s\n", ic_name, ic_types[0].ic_name);
	for (i = 0; i < type_size; i++) {
		if (0 == strncmp(ic_name, ic_types[i].ic_name, ic_types[i].len))
			return ic_types[i].ic_type;
	}

	FTS_TEST_ERROR("no IC type match");
	return 0;
}

/*
 * init_test_funcs - get test function based on ic_type
 */
int init_test_funcs(u32 ic_type)
{
	int i = 0;
	struct test_funcs *funcs = test_funcs_list[0];
	int funcs_len = sizeof(test_funcs_list) / sizeof(test_funcs_list[0]);
	u32 ic_series = 0;

	ic_series = TEST_ICSERIES(ic_type);
	FTS_TEST_INFO("ic_type:%x, test functions len:%x", ic_type, funcs_len);
	for (i = 0; i < funcs_len; i++) {
		funcs = test_funcs_list[i];
		if (ic_series == funcs->ic_series) {
			test_data.func = funcs;
			break;
		}
	}

	if (i >= funcs_len) {
		FTS_TEST_ERROR("no ic serial function match");
		return -ENODATA;
	}

	return 0;
}

/*
 * fts_test_save_test_data - Save test data to SD card etc.
 */
static int fts_test_save_test_data(char *file_name, char *data_buf, int len)
{
	struct file *pfile = NULL;
	struct filename *filename;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	FTS_TEST_FUNC_ENTER();
	memset(filepath, 0, sizeof(filepath));
	snprintf(filepath, PAGE_SIZE, "%s%s", FTS_ITO_RESULT_PATH, file_name);
	filename = getname_kernel(filepath);
	if (NULL == pfile) {
		pfile = file_open_name(filename, O_TRUNC | O_CREAT | O_RDWR, 0);
	}
	if (IS_ERR(pfile)) {
		FTS_TEST_ERROR("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(pfile, data_buf, len, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

/************************************************************************
* Name: fts_set_testitem
* Brief:  set test item code and name
* Input: null
* Output: null
* Return:
**********************************************************************/
void fts_set_testitem(u8 itemcode)
{
	test_data.test_item[test_data.test_num].itemcode = itemcode;
	test_data.test_item[test_data.test_num].testnum = test_data.test_num;
	test_data.test_item[test_data.test_num].testresult = RESULT_NULL;
	test_data.test_num++;
}

/************************************************************************
* Name: merge_all_testdata
* Brief:  Merge All Data of test result
* Input: none
* Output: none
* Return: none
***********************************************************************/
void merge_all_testdata(void)
{
	int len = 0;

	//Msg Area, Add Line1
	test_data.len_store_msg_area += snprintf(test_data.store_msg_area, PAGE_SIZE, "ECC, 85, 170, IC Name, %s, IC Code, %x\n",  test_data.ini_ic_name,  test_data.screen_param.selected_ic);

	/* Add the head part of Line2 */
	len = snprintf(test_data.tmp_buffer, PAGE_SIZE, "TestItem Num, %d, ", test_data.test_data_count);
	memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.tmp_buffer, len);
	test_data.len_store_msg_area += len;

	/* Add other part of Line2, except for "\n" */
	memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.msg_area_line2, test_data.len_msg_area_line2);
	test_data.len_store_msg_area += test_data.len_msg_area_line2;

	/* Add Line3 ~ Line10 */
	len = snprintf(test_data.tmp_buffer, PAGE_SIZE, "\n\n\n\n\n\n\n\n\n");
	memcpy(test_data.store_msg_area + test_data.len_store_msg_area, test_data.tmp_buffer, len);
	test_data.len_store_msg_area += len;

	/* 1.Add Msg Area */
	memcpy(test_data.store_all_data, test_data.store_msg_area,  test_data.len_store_msg_area);

	/* 2.Add Data Area */
	if (0 != test_data.len_store_data_area) {
		memcpy(test_data.store_all_data + test_data.len_store_msg_area, test_data.store_data_area, test_data.len_store_data_area);
	}

	FTS_TEST_DBG("lenStoreMsgArea=%d,  lenStoreDataArea = %d",   test_data.len_store_msg_area, test_data.len_store_data_area);
}

void init_storeparam_testdata(void)
{
	test_data.testresult_len = 0;

	test_data.len_store_msg_area = 0;
	test_data.len_msg_area_line2 = 0;
	test_data.len_store_data_area = 0;
	/* The Start Line of Data Area is 11 */
	test_data.start_line = 11;
	test_data.test_data_count = 0;
}

int allocate_init_testdata_memory(void)
{
	test_data.testresult = fts_malloc(BUFF_LEN_TESTRESULT_BUFFER);
	if (NULL == test_data.testresult)
		goto ERR;

	test_data.store_all_data = fts_malloc(FTS_TEST_STORE_DATA_SIZE);
	if (NULL == test_data.store_all_data)
		goto ERR;

	test_data.store_msg_area = fts_malloc(BUFF_LEN_STORE_MSG_AREA);
	if (NULL == test_data.store_msg_area)
		goto ERR;

	test_data.msg_area_line2 = fts_malloc(BUFF_LEN_MSG_AREA_LINE2);
	if (NULL == test_data.msg_area_line2)
		goto ERR;

	test_data.store_data_area = fts_malloc(BUFF_LEN_STORE_DATA_AREA);
	if (NULL == test_data.store_data_area)
		goto ERR;

	test_data.tmp_buffer = fts_malloc(BUFF_LEN_TMP_BUFFER);
	if (NULL == test_data.tmp_buffer)
		goto ERR;

	init_storeparam_testdata();
	return 0;
ERR:
	FTS_TEST_ERROR("fts_malloc memory failed in function.");
	return -ENOMEM;
}

/************************************************************************
* Name: free_testdata_memory
* Brief:  Release pointer memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
void free_testdata_memory(void)
{
	/* free buff */
	if (NULL != test_data.testresult)
		fts_free(test_data.testresult);

	if (NULL !=  test_data.store_all_data)
		fts_free(test_data.store_all_data);

	if (NULL != test_data.store_msg_area)
		fts_free(test_data.store_msg_area);

	if (NULL != test_data.msg_area_line2)
		fts_free(test_data.msg_area_line2);

	if (NULL != test_data.store_data_area)
		fts_free(test_data.store_data_area);

	if (NULL != test_data.tmp_buffer)
		fts_free(test_data.tmp_buffer);

}

int get_tx_rx_num(u8 tx_rx_reg, u8 *ch_num, u8 ch_num_max)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < 3; i++) {
		ret = read_reg(tx_rx_reg, ch_num);
		if ((ret < 0) || (*ch_num > ch_num_max)) {
			sys_delay(50);
		} else
			break;
	}

	if (i >= 3) {
		FTS_TEST_ERROR("get channel num fail");
		return -EIO;
	}

	return 0;
}

int get_channel_num(void)
{
	int ret = 0;
	u8 tx_num = 0;
	u8 rx_num = 0;

	FTS_TEST_FUNC_ENTER();

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_ERROR("enter factory mode fail, can't get tx/rx num");
		return ret;
	}

	test_data.screen_param.used_max_tx_num = TX_NUM_MAX;
	test_data.screen_param.used_max_rx_num = RX_NUM_MAX;
	test_data.screen_param.key_num_total = KEY_NUM_MAX;
	ret = get_tx_rx_num(FACTORY_REG_CHX_NUM, &tx_num, TX_NUM_MAX);
	if (ret < 0) {
		FTS_TEST_ERROR("get tx_num fail");
		return ret;
	}

	ret = get_tx_rx_num(FACTORY_REG_CHY_NUM, &rx_num, RX_NUM_MAX);
	if (ret < 0) {
		FTS_TEST_ERROR("get rx_num fail");
		return ret;
	}

	test_data.screen_param.tx_num = (int)tx_num;
	test_data.screen_param.rx_num = (int)rx_num;

	FTS_TEST_INFO("TxNum=%d, RxNum=%d, MaxTxNum=%d, MaxRxNum=%d",
				  test_data.screen_param.tx_num,
				  test_data.screen_param.rx_num,
				  test_data.screen_param.used_max_tx_num,
				  test_data.screen_param.used_max_rx_num);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

int fts_test_init_basicinfo(void)
{
	int ret = 0;
	u8 val = 0;

	ret = read_reg(REG_FW_VERSION, &val);
	FTS_TEST_SAVE_INFO("FW version:0x%02x\n", val);

	ret = read_reg(REG_VA_TOUCH_THR, &val);
	test_data.va_touch_thr = val;
	ret = read_reg(REG_VKEY_TOUCH_THR, &val);
	test_data.key_touch_thr = val;

	/* enter into factory mode and read tx/rx num */
	ret = get_channel_num();
	FTS_TEST_SAVE_INFO("tx_num:%d, rx_num:%d\n",
					   test_data.screen_param.tx_num,
					   test_data.screen_param.rx_num);

	return ret;
}

static int fts_test_main_init(void)
{
	int ret = 0;
	int len = 0;

	FTS_TEST_FUNC_ENTER();
	/* allocate memory for test data:csv&txt */
	ret = allocate_init_testdata_memory();
	if (ret < 0) {
		FTS_TEST_ERROR("allocate memory for test data fail");
		return ret;
	}

	/* get basic information: tx/rx num */
	ret = fts_test_init_basicinfo();
	if (ret < 0) {
		FTS_TEST_ERROR("test init basicinfo fail");
		return ret;
	}

	/* Allocate memory for detail threshold structure */
	ret = malloc_struct_DetailThreshold();
	if (ret < 0) {
		FTS_TEST_ERROR("Failed to malloc memory for detaithreshold");
		return ret;
	}

	/*Allocate test data buffer*/
	len = (test_data.screen_param.tx_num + 1) * test_data.screen_param.rx_num;
	test_data.buffer = (int *)fts_malloc(len * sizeof(int));
	if (NULL == test_data.buffer) {
		FTS_TEST_ERROR("test_data.buffer malloc fail");
		return -ENOMEM;
	}
	memset(test_data.buffer, 0, len * sizeof(int));

	FTS_TEST_FUNC_EXIT();
	return ret;
}

/*
 * fts_test_get_testparams - get test parameter from ini
 */
static int fts_test_get_testparams(char *config_name)
{
	int ret = 0;

	ret = fts_test_get_testparam_from_ini(config_name);

	return ret;
}

/************************************************************************
* Name: fts_test_start
* Brief:  Test entry. Select test items based on IC series
* Return: Test Result, PASS or FAIL
***********************************************************************/
/*
 * ============================
 * @Author:   102007757
 * @Version:  1.0
 * @DateTime: 2018-12-28
 * @ito_test_result : true----test pass; false----test failed
 * @ito_test_status : 0----start ito test; 1----ito test ready;
 *                    2----ito test finished; 3----ito test results saved;
 *                    4----ito test end
 * ============================
 */
static bool ito_test_result;
static u8 self_test_result = TP_SELFTEST_RESULT_INVALID;
static u8 ito_test_status;

static bool fts_test_start(void)
{
	bool testresult = false;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_DBG("IC_%s Test",  test_data.ini_ic_name);

	if (test_data.func && test_data.func->start_test) {
		testresult = test_data.func->start_test();
	} else {
		FTS_TEST_ERROR("test func/start_test func is null");
		testresult = false;
	}

	enter_work_mode();

	FTS_TEST_FUNC_EXIT();
	return testresult;
}

static int fts_test_main_exit(void)
{
	int data_len = 0;
	FTS_TEST_FUNC_ENTER();

	/* Merge test data */
	merge_all_testdata();

	/*Gets the number of tests in the test library and saves it*/
	data_len = test_data.len_store_msg_area + test_data.len_store_data_area;
	fts_test_save_test_data(FTS_TESTDATA_FILE_NAME, test_data.store_all_data, data_len);
	fts_test_save_test_data(FTS_TESTRESULT_FILE_NAME, test_data.testresult, test_data.testresult_len);

	/* free memory */
	free_struct_DetailThreshold();
	free_testdata_memory();

	/*free test data buffer*/
	if (test_data.buffer)
		fts_free(test_data.buffer);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

/*
 * fts_test_entry - test main entry
 *
 * warning - need disable irq & esdcheck before call this function
 *
 */
static int fts_test_entry(char *ini_file_name)
{
	int ret = 0;
	int data_len = 0;

	FTS_TEST_FUNC_ENTER();

	/*Add by 102007757 [Date: 2018-12-28]*/
	/*start ito test*/
	ito_test_status = 0;

	/*Initialize pointer memory*/
	ret = fts_test_main_init();
	if (ret < 0) {
		FTS_TEST_ERROR("fts_test_main_init() error.");
		goto TEST_ERR;
	}

	ret = fts_test_init_basicinfo();
	if (ret) {
		FTS_TEST_ERROR("test basic information init fail");
		goto TEST_ERR;
	}

	/*Read parse configuration file*/
	FTS_TEST_DBG("ini_file_name = %s", ini_file_name);
	ret = fts_test_get_testparams(ini_file_name);
	if (ret < 0) {
		FTS_TEST_ERROR("get testparam failed");
		goto TEST_ERR;
	}

	/*Select IC must match  IC from INI*/
	if ((test_data.screen_param.selected_ic >> 4  != IC_FT5X46 >> 4)) {
		FTS_TEST_ERROR("Select IC in driver:0x%x, INI:0x%x, no match", test_data.screen_param.selected_ic, IC_FT5X46);
		goto TEST_ERR;
	}

	/*ito test ready*/
	ito_test_status = 1;

	/*Start testing according to the test configuration*/
	if (true == fts_test_start()) {
		FTS_TEST_SAVE_INFO("\n=======Tp test pass. \n\n");
		ito_test_result = true;
	} else {
		FTS_TEST_SAVE_INFO("\n=======Tp test failure. \n\n");
		ito_test_result = false;
	}

	/*ito test finished*/
	ito_test_status = 2;

	/*Gets the number of tests in the test library and saves it*/
	data_len = test_data.len_store_msg_area + test_data.len_store_data_area;
	ret = fts_test_save_test_data("testdata.csv", test_data.store_all_data, data_len);
	ret = fts_test_save_test_data("testresult.txt", test_data.testresult, test_data.testresult_len);

	/*ito test results saved*/
	if (ret == 0)
		ito_test_status = 3;

	/*Release memory */
	fts_test_main_exit();

	/*ito test end*/
	ito_test_status = 4;

	FTS_TEST_FUNC_EXIT();
	return 0;

TEST_ERR:
	enter_work_mode();
	fts_test_main_exit();

	FTS_TEST_FUNC_EXIT();
	return ret;
}

#ifdef ITO_TEST_NORMALIZED_NODE
u8 FTS_IS_TESTING_FLAG;
static ssize_t fts_ito_test_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return -EPERM;
}

/*
 * return value:
 */
static int fts_ito_test_show(struct seq_file *file, void *data)
{
	char fwname[128] = {0};
	char buf[] = FTS_INI_FILE_NAME;
	char test_result_bmp[5];
	int i = 0;

	FTS_TEST_DBG("TESTING_FLAG = :%u.", FTS_IS_TESTING_FLAG);
	if  (FTS_IS_TESTING_FLAG == 1) {
		seq_printf(file, "ITO Testing\n");
		return 0;
	}
	FTS_IS_TESTING_FLAG = 1;

	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, PAGE_SIZE, "%s", buf);
	FTS_TEST_DBG("fwname:%s.", fwname);

	for (i = FT5X46_ENTER_FACTORY_MODE ; i <= FT5X46_PANELDIFFER_UNIFORMITY_TEST;  i++) {
		test_data.test_item[i].testresult = 0 ;
	}

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch (DISABLE);
#endif

	fts_test_entry(fwname);


#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch (ENABLE);
#endif

	switch (ito_test_status) {
	case 0:
		FTS_TEST_INFO("ITOtest status = 0\n");
		break;
	case 1:
		FTS_TEST_INFO("ITOtest status = 1\n");
		break;
	case 2:
		if (ito_test_result == true) {
			FTS_TEST_INFO(
				"ITOtest status = 2\n\
				ITOtest result: Pass\n\
				ITOtest save: Failed\n");
		} else {
			FTS_TEST_INFO(
				"ITOtest status = 2\n\
				ITOtest result: Failed\n\
				ITOtest save: Failed\n");
		}
		break;
	case 3:
		if (ito_test_result == true) {
			FTS_TEST_INFO(
				"ITOtest status = 3\n\
				ITOtest result: Pass\n\
				ITOtest save: Succeeded\n");
		} else {
			FTS_TEST_INFO(
				"ITOtest status = 3\n\
				ITOtest result: Failed\n\
				ITOtest save: Succeeded\n");
		}
		break;
	case 4:
		if (ito_test_result == true) {
			FTS_TEST_INFO(
				"ITOtest status = 4\n\
				ITOtest result: Pass\n\
				ITOtest save: Succeeded\n\
				ITOtet end\n");
		} else {
			FTS_TEST_INFO(
				"ITOtest status = 4\n\
				ITOtest result: Failed\n\
				ITOtest save: Succeeded\n\
				ITOtet end\n");
		}
		break;
	}

	//I2C_Communication result
	if ((test_data.test_item[FT5X46_ENTER_FACTORY_MODE].testresult)) {
		test_result_bmp[I2C_COMMUNICATION] = 'P';
	} else {
		seq_printf(file, "0F-1F-2F-3F-4F\n");
		return 0;
	}

	//Cap Rawdata result
	if ((test_data.test_item[FT5X46_RAWDATA_TEST].testresult)
		 && (test_data.test_item[FT5X46_SCAP_CB_TEST].testresult)
		 && (test_data.test_item[FT5X46_SCAP_RAWDATA_TEST].testresult)) {
		test_result_bmp[CAP_RAWDATA] = 'P';
	} else {
		test_result_bmp[CAP_RAWDATA] = 'F';
	}

	//open test result
	test_data.test_item[FT5X46_PANELDIFFER_UNIFORMITY_TEST].testresult = 1;
	if ((test_data.test_item[FT5X46_PANELDIFFER_TEST].testresult)
		 && (test_data.test_item[FT5X46_PANELDIFFER_UNIFORMITY_TEST].testresult)) {
		test_result_bmp[OPEN_DATA] = 'P';
	} else {
		test_result_bmp[OPEN_DATA] = 'F';
	}

	//short test result
	if (test_data.test_item[FT5X46_WEAK_SHORT_CIRCUIT_TEST].testresult) {
		test_result_bmp[SHORT_DATA] = 'P';
	} else {
		test_result_bmp[SHORT_DATA] = 'F';
	}

	//other test result
	test_result_bmp[OTHER_DATA] = 'P';

	seq_printf(file, "0%c-1%c-2%c-3%c-4%c\n",
		test_result_bmp[I2C_COMMUNICATION],
		test_result_bmp[CAP_RAWDATA],
		test_result_bmp[OPEN_DATA],
		test_result_bmp[SHORT_DATA],
		test_result_bmp[OTHER_DATA]);

	//seq_printf(file, "0P-1P-2P-3P-4P\n");

	FTS_IS_TESTING_FLAG = 0;
	FTS_TEST_FUNC_EXIT();

	return 0;
}

static int fts_ito_test_open (struct inode *inode, struct file *file)
{
	return single_open(file, fts_ito_test_show, NULL);
}

static const struct file_operations ito_test_ops = {
	.owner = THIS_MODULE,
	.open = fts_ito_test_open,
	.read = seq_read,
	.write = fts_ito_test_write,
};
#endif

/************************************************************************
* Name: fts_test_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t num_read_chars = 0;
	switch (ito_test_status) {
	case 0:
		FTS_TEST_INFO("ITOtest status = 0\n");
		break;
	case 1:
		FTS_TEST_INFO("ITOtest status = 1\n");
		break;
	case 2:
		if (ito_test_result == true) {
			FTS_TEST_INFO(
				"ITOtest status = 2\n\
				ITOtest result: Pass\n\
				ITOtest save: Failed\n");
		} else {
			FTS_TEST_INFO(
				"ITOtest status = 2\n\
				ITOtest result: Failed\n\
				ITOtest save: Failed\n");
		}
		break;
	case 3:
		if (ito_test_result == true) {
			FTS_TEST_INFO(
				"ITOtest status = 3\n\
				ITOtest result: Pass\n\
				ITOtest save: Succeeded\n");
		} else {
			FTS_TEST_INFO(
				"ITOtest status = 3\n\
				ITOtest result: Failed\n\
				ITOtest save: Succeeded\n");
		}
		break;
	case 4:
		if (ito_test_result == true) {
			FTS_TEST_INFO(
				"ITOtest status = 4\n\
				ITOtest result: Pass\n\
				ITOtest save: Succeeded\n\
				ITOtet end\n");
		} else {
			FTS_TEST_INFO(
				"ITOtest status = 4\n\
				ITOtest result: Failed\n\
				ITOtest save: Succeeded\n\
				ITOtet end\n");
		}
		break;
	}

	switch (ito_test_result) {
	case true:
		num_read_chars = snprintf(buf, PAGE_SIZE, "Pass\n");
		break;
	case false:
		num_read_chars = snprintf(buf, PAGE_SIZE, "Failed\n");
		break;
	}

	return num_read_chars;
}

/************************************************************************
* Name: fts_test_store
* Brief:  upgrade from app.bin
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_test_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char fwname[128] = {0};
	struct fts_ts_data *ts_data = fts_data;
	struct i2c_client *client;
	struct input_dev *input_dev;

	FTS_TEST_FUNC_ENTER();

	client = ts_data->client;
	input_dev = ts_data->input_dev;
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, PAGE_SIZE, "%s", buf);
	fwname[count - 1] = '\0';
	FTS_TEST_DBG("fwname:%s.", fwname);

	mutex_lock(&input_dev->mutex);
	disable_irq(client->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch (DISABLE);
#endif

	fts_test_entry(fwname);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch (ENABLE);
#endif

	enable_irq(client->irq);
	mutex_unlock(&input_dev->mutex);

	FTS_TEST_FUNC_EXIT();
	return count;
}

/*  test from test.ini
*    example:echo "***.ini" > fts_test
*/
static DEVICE_ATTR(fts_test, S_IRUGO | S_IWUSR, fts_test_show, fts_test_store);

/* add your attr in here*/
static struct attribute *fts_test_attributes[] = {
	&dev_attr_fts_test.attr,
	NULL
};

static struct attribute_group fts_test_attribute_group = {
	.attrs = fts_test_attributes
};

int fts_test_init(struct i2c_client *client)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();

/*Add by HQ-102007757 for ITO normalized test START*/
#ifdef ITO_TEST_NORMALIZED_NODE
	fts_android_touch_proc = proc_mkdir(FTS_ANDROID_TOUCH, NULL);
	fts_ito_test_proc = proc_create(FTS_ITO_TEST, 0664,
						  fts_android_touch_proc, &ito_test_ops);
		if (!fts_ito_test_proc)
			FTS_TEST_ERROR("create proc entry %s failed", FTS_ITO_TEST);
		else
			FTS_TEST_DBG("create proc entry %s success", FTS_ITO_TEST);
#endif
/*Add by HQ-102007757 for ITO normalized test END*/

	ret = sysfs_create_group(&client->dev.kobj, &fts_test_attribute_group);
	if (0 != ret) {
		FTS_TEST_ERROR("[focal] %s() - ERROR: sysfs_create_group() failed.",  __func__);
		sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);
	} else {
		FTS_TEST_DBG("[focal] %s() - sysfs_create_group() succeeded.", __func__);
	}

	FTS_TEST_FUNC_EXIT();

	return ret;
}

int fts_test_exit(struct i2c_client *client)
{
	FTS_TEST_FUNC_ENTER();

	sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

/*Add by HQ-102007757 for focal tp self test proc*/
#if FTS_TP_SELFTEST
bool fts_selftest_start(int temp)
{
	bool testresult = false;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_DBG("IC_%s Test",  test_data.ini_ic_name);

	testresult = start_selftest_ft5x46(temp);

	enter_work_mode();

	FTS_TEST_FUNC_EXIT();

	return testresult;
}

static int fts_selftest_enry(char *ini_file_name, int temp)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();

	/*Initialize pointer memory*/
	ret = fts_test_main_init();
	if (ret < 0) {
		FTS_TEST_ERROR("fts_test_main_init() error.");
		goto TEST_ERR;
	}

	ret = fts_test_init_basicinfo();
	if (ret) {
		FTS_TEST_ERROR("test basic information init fail");
		goto TEST_ERR;
	}

	/*Read parse configuration file*/
	FTS_TEST_DBG("ini_file_name = %s", ini_file_name);
	ret = fts_test_get_testparams(ini_file_name);
	if (ret < 0) {
		FTS_TEST_ERROR("get testparam failed");
		goto TEST_ERR;
	}

	/*Select IC must match  IC from INI*/
	if ((test_data.screen_param.selected_ic >> 4  != IC_FT5X46 >> 4)) {
		FTS_TEST_ERROR("Select IC in driver:0x%x, INI:0x%x, no match", test_data.screen_param.selected_ic, IC_FT5X46);
		goto TEST_ERR;
	}

	/*Start testing according to the test configuration*/
	if (true == fts_selftest_start(temp)) {
		FTS_TEST_INFO("=======Tp test pass.");
		self_test_result = TP_SELFTEST_RESULT_SUCESS;
	} else {
		FTS_TEST_INFO("=======Tp test failure.");
		self_test_result = TP_SELFTEST_RESULT_FAILED;
	}

TEST_ERR:
	enter_work_mode();
	fts_test_main_exit();

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static ssize_t fts_tp_selftest_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	char tmp[5];
	int cnt;

	FTS_TEST_FUNC_ENTER();

	if (*pos != 0)
		return 0;

	cnt = snprintf(tmp, PAGE_SIZE, "%d\n", self_test_result);

	FTS_TEST_INFO("%s: tmp = %s, self_test_result=%d\n", __func__, tmp, self_test_result);
	if (copy_to_user(buf, tmp, strlen(tmp)))
		return -EFAULT;

	*pos += cnt;
	FTS_TEST_FUNC_EXIT();
	return cnt;
}

static ssize_t fts_tp_selftest_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char tmp[6];
	int retval = 0;
	int test_case = 0;
	char fwname1[128] = "Conf_MultipleTest_C3H_ft5446_Ofilm.ini";
	struct fts_ts_data *ts_data = fts_data;
	struct i2c_client *client;
	struct input_dev *input_dev;
	self_test_result = TP_SELFTEST_RESULT_INVALID;

	FTS_TEST_FUNC_ENTER();

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		FTS_TEST_ERROR("copy from user error\n");
		return retval;
	}

	client = ts_data->client;
	input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	disable_irq(client->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch (DISABLE);
#endif

	if (!strncmp(tmp, "short", 5)) {
		test_case = TP_SELFTEST_SHORT;
	} else if (!strncmp(tmp, "open", 4))
		test_case = TP_SELFTEST_OPEN;
	else if (!strncmp(tmp, "i2c", 3))
		test_case = TP_SELFTEST_I2C;
	else {
		FTS_TEST_ERROR("Invalid teset case\n");
		retval = -EFAULT;
		goto ERR;
	}

	/*I2C won't enter factory mode*/
	if (TP_SELFTEST_I2C != test_case) {
		retval = fts_selftest_enry(fwname1, test_case);
		if (retval) {
			FTS_TEST_ERROR("selftest error\n");
			retval = -EFAULT;
		}
	} else {
		/////////////////////////////////////////////////////// I2C_TEST,
		retval = fts_get_ic_information(ts_data);
		if (ERROR_CODE_OK != retval) {
			FTS_TEST_ERROR("I2C test error.\n");
			FTS_TEST_ERROR("selftest error\n");
			FTS_TEST_INFO("=======Tp test fail.");
			self_test_result = TP_SELFTEST_RESULT_FAILED;
			retval = -EFAULT;
		} else {
			FTS_TEST_ERROR("I2C test PASS.\n");
			FTS_TEST_ERROR("selftest PASS\n");
			FTS_TEST_INFO("=======Tp test PASS.");
			self_test_result = TP_SELFTEST_RESULT_SUCESS;
		}
	}


ERR:
#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch (ENABLE);
#endif

	enable_irq(client->irq);
	mutex_unlock(&input_dev->mutex);

	FTS_TEST_FUNC_EXIT();

	return (!retval ? count : -1);
}

static const struct file_operations tp_selftest_ops = {
	.read       = fts_tp_selftest_read,
	.write      = fts_tp_selftest_write,
};


#define FTS_PROC_TP_SELFTEST "tp_selftest"
static struct proc_dir_entry *tp_selftest;

int fts_tp_selftest_proc(void)
{
	int ret = 0;

	FTS_TEST_INFO("%s:ENTER FUNC ---- %d\n", __func__, __LINE__);
	tp_selftest = proc_create(FTS_PROC_TP_SELFTEST, 0444, NULL, &tp_selftest_ops);
	if (tp_selftest == NULL) {
		FTS_TEST_INFO("fts, create_proc_entry tp_selftest failed\n");
		ret = -1;
	}
	return ret;
}
#endif

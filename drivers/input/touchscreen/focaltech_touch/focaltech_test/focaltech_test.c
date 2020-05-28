/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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
* Author:     Software Department, FocalTech
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
	&test_func_ft8719,
};

static int fts_test_result = SELFTEST_INVALID;

struct test_ic_type ic_types[] = {
	{"FT5X22", 6, IC_FT5X46, 0},
	{"FT5X46", 6, IC_FT5X46, 0},
	{"FT5X46i", 7, IC_FT5X46I, 0},
	{"FT5526", 6, IC_FT5526, 0},
	{"FT3X17", 6, IC_FT3X17, 0},
	{"FT5436", 6, IC_FT5436, 0},
	{"FT3X27", 6, IC_FT3X27, 0},
	{"FT5526i", 7, IC_FT5526I, 0},
	{"FT5416", 6, IC_FT5416, 0},
	{"FT5426", 6, IC_FT5426, 0},
	{"FT5435", 6, IC_FT5435, 0},
	{"FT7681", 6, IC_FT7681, 0},
	{"FT7661", 6, IC_FT7661, 0},
	{"FT7511", 6, IC_FT7511, 0},
	{"FT7421", 6, IC_FT7421, 0},
	{"FT7311", 6, IC_FT7311, 0},

	{"FT6X36", 6, IC_FT6X36, 0},
	{"FT3X07", 6, IC_FT3X07, 0},
	{"FT6416", 6, IC_FT6416, 0},
	{"FT6336G/U", 9, IC_FT6426, 0},
	{"FT6236U", 7, IC_FT6236U, 0},
	{"FT6436U", 7, IC_FT6436U, 0},
	{"FT3267", 6, IC_FT3267, 0},
	{"FT3367", 6, IC_FT3367, 0},
	{"FT7401", 6, IC_FT7401, 0},
	{"FT3407U", 7, IC_FT3407U, 0},

	{"FT5822", 6, IC_FT5822, 0},
	{"FT5626", 6, IC_FT5626, 0},
	{"FT5726", 6, IC_FT5726, 0},
	{"FT5826B", 7, IC_FT5826B, 0},
	{"FT3617", 6, IC_FT3617, 0},
	{"FT3717", 6, IC_FT3717, 0},
	{"FT7811", 6, IC_FT7811, 0},
	{"FT5826S", 7, IC_FT5826S, 0},
	{"FT3517U", 7, IC_FT3517U, 0},

	{"FT8606", 6, IC_FT8606, 0},

	{"FT8716U", 7, IC_FT8716U, 0},
	{"FT8716F", 7, IC_FT8716F, 0},
	{"FT8716", 6, IC_FT8716, 0},
	{"FT8613", 6, IC_FT8613, 0},

	{"FT3C47U", 7, IC_FT3C47U, 0},

	{"FT8607U", 7, IC_FT8607U, 0},
	{"FT8607", 6, IC_FT8607, 0},

	{"FT8736", 6, IC_FT8736, 0},

	{"FT3D47", 6, IC_FT3D47, 0},

	{"FTE716", 6, IC_FTE716, 0},

	{"FT8201", 6, IC_FT8201, 0},

	{"FT8006M", 7, IC_FT8006M, 0},

	{"FT8006U", 7, IC_FT8006U, 0},

	{"FT8719", 6, IC_FT8719, 0x8719},

};

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
	return kzalloc(size, GFP_ATOMIC);
}

void fts_free_proc(void *p)
{
	return kfree(p);
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
	u8 cmd[2] = { 0 };

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
unsigned char fts_i2c_read_write(unsigned char *writebuf, int writelen, unsigned char *readbuf, int readlen)
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

	data = (u8 *) fts_malloc(byte_num * sizeof(u8));
	if (NULL == data) {
		FTS_TEST_SAVE_INFO("rawdata buffer malloc fail\n");
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
		FTS_TEST_SAVE_INFO("read rawdata fail\n");
		goto READ_MASSDATA_ERR;
	}

	for (i = 1; i < packet_num; i++) {
		offset = read_num * i;
		if ((i == (packet_num - 1)) && packet_remainder) {
			read_num = packet_remainder;
		}

		ret = fts_test_i2c_read(NULL, 0, data + offset, read_num);
		if (ret) {
			FTS_TEST_SAVE_INFO("read much rawdata fail\n");
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
		FTS_TEST_SAVE_INFO("write start clb fail\n");
		return ret;
	}

	sys_delay(120);
	while (times++ < FACTORY_TEST_RETRY) {
		ret = read_reg(FACTORY_REG_CLB, &val);
		if ((0 == ret) && (0x02 == val)) {
			/* clb ok */
			break;
		} else
			FTS_TEST_DBG("reg%x=%x,retry:%d", FACTORY_REG_CLB, val, times);

		sys_delay(FACTORY_TEST_DELAY);
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_INFO("chip clb timeout\n");
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
		FTS_TEST_SAVE_INFO("read device mode fail\n");
		return ret;
	}

	/* Top bit position 1, start scan */
	val |= 0x80;
	ret = write_reg(DEVIDE_MODE_ADDR, val);
	if (ret) {
		FTS_TEST_SAVE_INFO("write device mode fail\n");
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
		FTS_TEST_SAVE_INFO("start scan timeout\n");
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
		sys_delay(100);
		/* Wait register status update */
		state = 0xFF;
		ret = read_reg(FACTORY_REG_PARAM_UPDATE_STATE, &state);
		if ((0 == ret) && (0x00 == state))
			break;
		else
			FTS_TEST_DBG("reg%x=%x,retry:%d", FACTORY_REG_PARAM_UPDATE_STATE, state, times);
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_INFO("Wait State Update fail\n");
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
		FTS_TEST_SAVE_INFO("Failed to Enter Factory Mode\n");
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
		FTS_TEST_SAVE_INFO("Failed to Scan ...\n");
		return ret;
	}

	/* Read RawData for va Area */
	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret) {
		FTS_TEST_SAVE_INFO("wirte AD to reg01 fail\n");
		return ret;
	}
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, va_num * 2, data);
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Get RawData of channel.\n");
		return ret;
	}

	/* Read RawData for key Area */
	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAE);
	if (ret) {
		FTS_TEST_SAVE_INFO("wirte AE to reg01 fail\n");
		return ret;
	}
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, key_num * 2, data + va_num);
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Get RawData of keys.\n");
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

	data = (u8 *) fts_malloc(byte_num * sizeof(u8));
	if (NULL == data) {
		FTS_TEST_SAVE_INFO("cb buffer malloc fail\n");
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
			FTS_TEST_SAVE_INFO("write cb addr high fail\n");
			goto TEST_CB_ERR;
		}
		ret = write_reg(FACTORY_REG_CB_ADDR_L, addr_l);
		if (ret) {
			FTS_TEST_SAVE_INFO("write cb addr low fail\n");
			goto TEST_CB_ERR;
		}

		ret = fts_test_i2c_read(&cb_addr, 1, data + offset, read_num);
		if (ret) {
			FTS_TEST_SAVE_INFO("read cb fail\n");
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
		FTS_TEST_SAVE_INFO("start short test fail\n");
		goto ADC_ERROR;
	}
	sys_delay(ch_num * FACTORY_TEST_DELAY);

	for (times = 0; times < FACTORY_TEST_RETRY; times++) {
		ret = read_reg(FACTORY_REG_SHORT_TEST_STATE, &short_state);
		if ((0 == ret) && (retval == short_state))
			break;
		else
			FTS_TEST_DBG("reg%x=%x,retry:%d", FACTORY_REG_SHORT_TEST_STATE, short_state, times);

		sys_delay(FACTORY_TEST_RETRY_DELAY);
	}
	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_INFO("short test timeout, ADC data not OK\n");
		ret = -EIO;
		goto ADC_ERROR;
	}

	ret = read_mass_data(FACTORY_REG_SHORT_ADDR, byte_num, adc_buf);
	if (ret) {
		FTS_TEST_SAVE_INFO("get short(adc) data fail\n");
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
			FTS_TEST_SAVE_INFO("%5d, ", data[rx_num * tx_num + col]);
		}
		FTS_TEST_SAVE_INFO("\n");
	}
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
				continue;	/* Invalid Node */
			value = data[row * rx_num + col];
			if (value < min || value > max) {
				tmp_result = false;
				FTS_TEST_SAVE_INFO("test failure.Node=(%d, %d),Get_value=%d,Set_Range=(%d, %d)\n",
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
				continue;	/* Invalid Node */
			value = data[rx_num * tx_num + col];
			if (value < vk_min || value > vk_max) {
				tmp_result = false;
				FTS_TEST_SAVE_INFO("test failure.Node=(%d, %d),Get_value=%d,Set_Range=(%d, %d)\n",
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
	/* VA */
	for (row = 0; row < tx_num; row++) {
		for (col = 0; col < rx_num; col++) {
			if (test_data.incell_detail_thr.invalid_node[row * rx_num + col] == 0)
				continue;	/* Invalid Node */
			tmp_min = data_min[row * rx_num + col];
			tmp_max = data_max[row * rx_num + col];
			value = data[row * rx_num + col];
			if (value < tmp_min || value > tmp_max) {
				tmp_result = false;
				FTS_TEST_SAVE_INFO
				    (" \n test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d)", row + 1,
				     col + 1, value, tmp_min, tmp_max);
			}
		}
	}
	/* Key */
	if (include_key) {
		if (test_data.screen_param.key_flag) {
			key_num = test_data.screen_param.key_num;
		}
		row = test_data.screen_param.tx_num;
		for (col = 0; col < key_num; col++) {
			if (test_data.incell_detail_thr.invalid_node[rx_num * tx_num + col] == 0)
				continue;	/* Invalid Node */
			tmp_min = data_min[rx_num * tx_num + col];
			tmp_max = data_max[rx_num * tx_num + col];
			value = data[rx_num * tx_num + col];
			if (value < tmp_min || value > tmp_max) {
				tmp_result = false;
				FTS_TEST_SAVE_INFO
				    (" \n test failure. Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d)", row + 1,
				     col + 1, value, tmp_min, tmp_max);
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
	len =
	    snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER, "%s, %d, %d, %d, %d, %d, ", test_num,
		     test_data.test_item_code, row, col, test_data.start_line, item_count);

	memcpy(test_data.msg_area_line2 + test_data.len_msg_area_line2, test_data.tmp_buffer, len);
	test_data.len_msg_area_line2 += len;

	test_data.start_line += row;
	test_data.test_data_count++;

	/* Save Data */
	for (i = 0 + index; (i < row + index) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < col) && (j < RX_NUM_MAX); j++) {
			if (j == (col - 1)) {
				/* The Last Data of the row, add "\n" */
				len =
				    snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER, "%d, \n",
					     data[col * (i + index) + j]);
			} else {
				len =
				    snprintf(test_data.tmp_buffer, BUFF_LEN_TMP_BUFFER, "%d, ",
					     data[col * (i + index) + j]);
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

	if (NULL != test_data.store_all_data)
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

static int get_channel_num(void)
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
	test_data.screen_param.used_max_tx_num = tx_num + KEY_NUM_MAX;
	test_data.screen_param.used_max_rx_num = rx_num + KEY_NUM_MAX;

	FTS_TEST_INFO("TxNum=%d, RxNum=%d, MaxTxNum=%d, MaxRxNum=%d",
		      test_data.screen_param.tx_num, test_data.screen_param.rx_num,
		      test_data.screen_param.used_max_tx_num, test_data.screen_param.used_max_rx_num);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

static int fts_test_init_basicinfo(void)
{
	int ret = 0;
	u8 val = 0;

	FTS_TEST_SAVE_INFO("FTS TESTCODE VERSION:%s\n", IC_TEST_VERSION);

	ret = read_reg(REG_FW_VERSION, &val);
	FTS_TEST_SAVE_INFO("FW version:0x%02x\n", val);

	ret = read_reg(REG_VA_TOUCH_THR, &val);
	test_data.va_touch_thr = val;
	ret = read_reg(REG_VKEY_TOUCH_THR, &val);
	test_data.key_touch_thr = val;

	/* enter into factory mode and read tx/rx num */
	ret = get_channel_num();
	FTS_TEST_SAVE_INFO("tx_num:%d, rx_num:%d\n", test_data.screen_param.tx_num, test_data.screen_param.rx_num);

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

	/*Allocate test data buffer */
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

static int fts_test_main_exit(void)
{
	FTS_TEST_FUNC_ENTER();
	/* free memory */
	free_struct_DetailThreshold();
	free_testdata_memory();

	/*free test data buffer */
	if (test_data.buffer)
		fts_free(test_data.buffer);

	FTS_TEST_FUNC_EXIT();
	return 0;
}


/************************************************************************
* Name: fts_test_show
* Brief:  no
* Input: device, device attribute, char buf
* Output: no
* Return: EPERM
***********************************************************************/
static ssize_t fts_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return -EPERM;
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
	char fwname[128] = { 0 };
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev;

	FTS_TEST_FUNC_ENTER();

	input_dev = ts_data->input_dev;
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, 128, "%s", buf);
	fwname[count - 1] = '\0';
	FTS_TEST_DBG("fwname:%s.", fwname);

	mutex_lock(&input_dev->mutex);
	disable_irq(ts_data->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch(DISABLE);
#endif


#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_esdcheck_switch(ENABLE);
#endif

	enable_irq(ts_data->irq);
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

static int fts_i2c_test(void)
{
	int ret = 0;
	u8 chip_id[2] = { 0 };
	int cnt = 0;

	do {
		ret = fts_i2c_read_reg(fts_data->client, FTS_REG_CHIP_ID, &chip_id[0]);
		ret = fts_i2c_read_reg(fts_data->client, FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			cnt++;
			ret = SELFTEST_FAIL;
			msleep(100);
		} else {
			ret = SELFTEST_PASS;
			break;
		}
	} while (cnt < 10);

	return SELFTEST_PASS;
}

static int fts_tp_selftest_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t fts_tp_selftest_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	char tmp[5];
	int cnt;

	if (*pos != 0)
		return 0;

	cnt = snprintf(tmp, sizeof(fts_test_result), "%d\n", fts_test_result);
	if (copy_to_user(buf, tmp, strlen(tmp)))
		return -EFAULT;
	*pos += cnt;
	return cnt;
}

static int get_ic_types_by_chipid(short chipid)
{
	int i = 0;
	int type_size = 0;

	type_size = sizeof(ic_types) / sizeof(ic_types[0]);
	for (i = 0; i < type_size; i++) {
		if (chipid == ic_types[i].chipid)
			return ic_types[i].ic_type;
	}

	return 0;
}

ssize_t fts_tp_selftest_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char tmp[6];
	int retval;
	int id;

	fts_test_result = SELFTEST_INVALID;

	if (!fts_data->client || count > sizeof(tmp)) {
		retval = -SELFTEST_INVALID;
		fts_test_result = SELFTEST_INVALID;
		goto out;
	}

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		fts_test_result = SELFTEST_INVALID;
		goto out;
	}

	/* test initialize */
	retval = fts_test_main_init();
	if (retval < 0) {
		FTS_TEST_ERROR("fts_test_main_init() error.");
		goto test_err;
	}

	id = get_ic_types_by_chipid(fts_data->chipid);
	if (id == 0) {
		retval = -EFAULT;
		fts_test_result = SELFTEST_INVALID;
		goto out;
	}

	retval = init_test_funcs(id);
	if (retval < 0) {
		retval = -EFAULT;
		fts_test_result = SELFTEST_INVALID;
		goto out;
	}

	if (!test_data.func) {
		FTS_TEST_ERROR("[focal] %s - ERROR: can't find test data func", __func__);
		retval = -SELFTEST_INVALID;
		fts_test_result = SELFTEST_INVALID;
		goto out;
	}

	disable_irq(fts_data->irq);
	if (!strncmp(tmp, "short", 5)) {
		if (test_data.func->open_test)
			fts_test_result = test_data.func->short_test();
	} else if (!strncmp(tmp, "open", 4)) {
		if (test_data.func->short_test)
			fts_test_result = test_data.func->open_test();
	} else if (!strncmp(tmp, "i2c", 3))
		fts_test_result = fts_i2c_test();
	retval = fts_test_result;

	enter_work_mode();
	enable_irq(fts_data->irq);
out:
	if (retval >= 0)
		retval = count;
	fts_test_main_exit();
test_err:
	return retval;
}

int fts_tp_selftest_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations tp_selftest_ops = {
	.open = fts_tp_selftest_open,
	.read = fts_tp_selftest_read,
	.write = fts_tp_selftest_write,
	.release = fts_tp_selftest_release,
};

static int32_t c_show(struct seq_file *m, void *v)
{
	int ret = 0;
	int i = 0, j = 0, len = 0;
	int *rawdata = NULL;
	u8 old_mode = 0;
	u8 tx_num = 0;
	u8 rx_num = 0;

	FTS_TEST_FUNC_ENTER();

	rawdata = (int *)vmalloc(PAGE_SIZE * 4);
	if (!rawdata) {
		ret = -EFAULT;
		goto out;
	}

	/* 0xEE = 1, disable cb */
	ret = write_reg(FACTORY_REG_AUTO_CAL_FLAG, 0x01);
	if (ret) {
		FTS_TEST_ERROR("write data auto cal fail");
		ret = -EFAULT;
		goto out;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_ERROR("enter factory mode fail");
		ret = -EFAULT;
		goto out;
	}

	ret = read_reg(FACTORY_REG_CHX_NUM, &tx_num);
	ret = read_reg(FACTORY_REG_CHY_NUM, &rx_num);

	/* switch to differ mode */
	ret = read_reg(FACTORY_REG_DATA_SELECT, &old_mode);

/*********************GET RAWDATA*********************/
	ret = write_reg(FACTORY_REG_DATA_SELECT, 0x00);
	if (ret) {
		FTS_TEST_ERROR("write data select fail");
		ret = -EFAULT;
		goto out;
	}

	/* start to scan a frame */
	ret = start_scan_incell();
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Scan ...\n");
		return ret;
	}

	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret) {
		FTS_TEST_ERROR("write REG LINE fail");
		ret = -EFAULT;
		goto out;
	}

	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, tx_num * rx_num * 2, rawdata);
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Get RawData of channel.\n");
		ret = -EFAULT;
		goto out;
	}

	seq_printf(m, "\nRAW DATA\n");
	len = 0;
	for (i = 0; (i < tx_num) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < rx_num) && (j < RX_NUM_MAX); j++) {
			if (j == (rx_num - 1)) {
				seq_printf(m, "%5d, \n", rawdata[rx_num * i + j]);
			} else {
				seq_printf(m, "%5d, ", rawdata[rx_num * i + j]);
			}
		}
	}
	seq_printf(m, "\nDIFF DATA\n");

/*********************GET DIFFDATA*********************/
	ret = write_reg(FACTORY_REG_DATA_SELECT, 0x01);
	if (ret) {
		FTS_TEST_ERROR("write data select fail");
		ret = -EFAULT;
		goto out;
	}

	/* start to scan a frame */
	ret = start_scan_incell();
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Scan ...\n");
		return ret;
	}

	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret) {
		FTS_TEST_ERROR("write REG LINE fail");
		ret = -EFAULT;
		goto out;
	}

	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, tx_num * rx_num * 2, rawdata);
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Get RawData of channel.\n");
		ret = -EFAULT;
		goto out;
	}

	for (i = 0; (i < tx_num) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < rx_num) && (j < RX_NUM_MAX); j++) {
			if (j == (rx_num - 1)) {
				seq_printf(m, "%5d, \n", (short)rawdata[rx_num * i + j]);
			} else {
				seq_printf(m, "%5d, ", (short)rawdata[rx_num * i + j]);
			}
		}
	}
	seq_printf(m, "\n\n");

out:
	if (old_mode)
		write_reg(FACTORY_REG_DATA_SELECT, old_mode);
	ret = enter_work_mode();
	enable_irq(fts_data->irq);

	if (rawdata) {
		vfree(rawdata);
		rawdata = NULL;
	}

	FTS_TEST_FUNC_EXIT();
	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations fts_datadump_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show
};

static int32_t fts_tp_data_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &fts_datadump_seq_ops);
}

static const struct file_operations fts_datadump_fops = {
	.owner = THIS_MODULE,
	.open = fts_tp_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int fts_test_init(struct i2c_client *client)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();

	ret = sysfs_create_group(&client->dev.kobj, &fts_test_attribute_group);
	if (0 != ret) {
		FTS_TEST_ERROR("[focal] %s() - ERROR: sysfs_create_group() failed.", __func__);
		sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);
	} else {
		FTS_TEST_DBG("[focal] %s() - sysfs_create_group() succeeded.", __func__);
	}
	fts_data->tp_selftest_proc = proc_create("tp_selftest", 0644, NULL, &tp_selftest_ops);
	if (!fts_data->tp_selftest_proc)
		FTS_TEST_ERROR("[focal] %s() - ERROR: tp_selftest proc create failed.", __func__);

	fts_data->tp_data_dump_proc = proc_create("tp_data_dump", 0444, NULL, &fts_datadump_fops);
	FTS_TEST_ERROR("create file node of tp_data_dump");
    if (!fts_data->tp_data_dump_proc)
		FTS_TEST_ERROR("[focal] %s() - ERROR: tp_data_dump proc create failed.", __func__);

	FTS_TEST_FUNC_EXIT();

	return ret;
}

int fts_test_exit(struct i2c_client *client)
{
	FTS_TEST_FUNC_ENTER();

	sysfs_remove_group(&client->dev.kobj, &fts_test_attribute_group);
	if (fts_data->tp_selftest_proc)
		remove_proc_entry("tp_selftest", NULL);
	if (fts_data->tp_data_dump_proc)
		remove_proc_entry("tp_data_dump", NULL);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

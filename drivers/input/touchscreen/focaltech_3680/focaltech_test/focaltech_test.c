// SPDX-License-Identifier: GPL-2.0
/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 *
 * Copyright (C) 2022 XiaoMi, Inc.
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
struct fts_test *fts_ftest;

struct test_funcs *test_func_list[] = {
	&test_func_ft5662,
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

int fts_abs(int value)
{
	if (value < 0)
		value = 0 - value;

	return value;
}

void *fts_malloc(size_t size)
{
	return kzalloc(size, GFP_KERNEL);
}

void fts_free_proc(void *p)
{
	return kfree(p);
}

void print_buffer(int *buffer, int length, int line_num)
{
	int i = 0;
	int j = 0;
	int tmpline = 0;
	char *tmpbuf = NULL;
	int tmplen = 0;
	int cnt = 0;
	struct fts_test *tdata = fts_ftest;

	if (tdata && tdata->ts_data && (tdata->ts_data->log_level < 3))
		return;

	if ((buffer == NULL) || (length <= 0)) {
		FTS_TEST_DBG("buffer/length(%d) fail", length);
		return;
	}

	tmpline = line_num ? line_num : length;
	tmplen = tmpline * 6 + 128;
	tmpbuf = kzalloc(tmplen, GFP_KERNEL);

	for (i = 0; i < length; i = i + tmpline) {
		cnt = 0;
		for (j = 0; j < tmpline; j++) {
			cnt += snprintf(tmpbuf + cnt, tmplen - cnt, "%5d ", buffer[i + j]);
			if ((cnt >= tmplen) || ((i + j + 1) >= length))
				break;
		}
		FTS_TEST_DBG("%s", tmpbuf);
	}

	kfree(tmpbuf);
	tmpbuf = NULL;
}

/********************************************************************
 * test read/write interface
 *******************************************************************/
static int fts_test_bus_read(
	u8 *cmd, int cmdlen, u8 *data, int datalen)
{
	int ret = 0;

	ret = fts_read(cmd, cmdlen, data, datalen);
	if (ret < 0)
		return ret;
	else
		return 0;
}

static int fts_test_bus_write(u8 *writebuf, int writelen)
{
	int ret = 0;

	ret = fts_write(writebuf, writelen);
	if (ret < 0)
		return ret;
	else
		return 0;
}

int fts_test_read_reg(u8 addr, u8 *val)
{
	return fts_test_bus_read(&addr, 1, val, 1);
}

int fts_test_write_reg(u8 addr, u8 val)
{
	int ret;
	u8 cmd[2] = {0};

	cmd[0] = addr;
	cmd[1] = val;
	ret = fts_test_bus_write(cmd, 2);

	return ret;
}

int fts_test_read(u8 addr, u8 *readbuf, int readlen)
{
	int ret = 0;
	int i = 0;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = readlen;

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;

	if (byte_num < BYTES_PER_TIME)
		packet_length = byte_num;
	else
		packet_length = BYTES_PER_TIME;
	/* FTS_TEST_DBG("packet num:%d, remainder:%d", packet_num, packet_remainder); */

	ret = fts_test_bus_read(&addr, 1, &readbuf[offset], packet_length);
	if (ret < 0) {
		FTS_TEST_ERROR("read buffer fail");
		return ret;
	}
	for (i = 1; i < packet_num; i++) {
		offset += packet_length;
		if ((i == (packet_num - 1)) && packet_remainder)
			packet_length = packet_remainder;

		ret = fts_test_bus_read(&addr, 1, &readbuf[offset],
								packet_length);

		if (ret < 0) {
			FTS_TEST_ERROR("read buffer fail");
			return ret;
		}
	}

	return 0;
}

int fts_test_read_one(u8 addr, u8 *readbuf, int readlen)
{
	return fts_test_bus_read(&addr, 1, readbuf, readlen);
}

int fts_test_write(u8 addr, u8 *writebuf, int writelen)
{
	int ret = 0;
	int i = 0;
	u8 *data = NULL;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = writelen;

	data = fts_malloc(BYTES_PER_TIME + 1);
	if (!data) {
		FTS_TEST_ERROR("malloc memory for bus write data fail");
		return -ENOMEM;
	}

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;

	if (byte_num < BYTES_PER_TIME)
		packet_length = byte_num;
	else
		packet_length = BYTES_PER_TIME;
	/* FTS_TEST_DBG("packet num:%d, remainder:%d", packet_num, packet_remainder); */

	data[0] = addr;
	for (i = 0; i < packet_num; i++) {
		if (i != 0)
			data[0] = addr + 1;
		if ((i == (packet_num - 1)) && packet_remainder)
			packet_length = packet_remainder;
		memcpy(&data[1], &writebuf[offset], packet_length);

		ret = fts_test_bus_write(data, packet_length + 1);
		if (ret < 0) {
			FTS_TEST_ERROR("write buffer fail");
			fts_free(data);
			return ret;
		}

		offset += packet_length;
	}

	fts_free(data);
	return 0;
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

	ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);
	if ((ret >= 0) && (mode == 0x00))
		return 0;

	for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = fts_test_write_reg(DEVIDE_MODE_ADDR, 0x00);
		if (ret >= 0) {
			sys_delay(FACTORY_TEST_DELAY);
			for (j = 0; j < 20; j++) {
				ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);
				if ((ret >= 0) && (mode == 0x00)) {
					FTS_TEST_INFO("enter work mode success");
					return 0;
				}
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

	ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);
	if ((ret >= 0) && (mode == 0x40))
		return 0;

	for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = fts_test_write_reg(DEVIDE_MODE_ADDR, 0x40);
		if (ret >= 0) {
			sys_delay(FACTORY_TEST_DELAY);
			for (j = 0; j < 20; j++) {
				ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);
				if ((ret >= 0) && (mode == 0x40)) {
					FTS_TEST_INFO("enter factory mode success");
					sys_delay(200);
					return 0;
				}
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
	u8 *data = NULL;

	data = (u8 *)fts_malloc(byte_num * sizeof(u8));
	if (data == NULL) {
		FTS_TEST_SAVE_ERR("mass data buffer malloc fail\n");
		return -ENOMEM;
	}

	/* read rawdata buffer */
	FTS_TEST_INFO("mass data len:%d", byte_num);
	ret = fts_test_read(addr, data, byte_num);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read mass data fail\n");
		goto read_massdata_err;
	}

	for (i = 0; i < byte_num; i = i + 2)
		buf[i >> 1] = (int)(short)((data[i] << 8) + data[i + 1]);

	ret = 0;
read_massdata_err:
	fts_free(data);
	return ret;
}

int read_mass_data_u16(u8 addr, int byte_num, int *buf)
{
	int ret = 0;
	int i = 0;
	u8 *data = NULL;

	data = (u8 *)fts_malloc(byte_num * sizeof(u8));
	if (data == NULL) {
		FTS_TEST_SAVE_ERR("mass data buffer malloc fail\n");
		return -ENOMEM;
	}

	/* read rawdata buffer */
	FTS_TEST_INFO("mass data len:%d", byte_num);
	ret = fts_test_read(addr, data, byte_num);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read mass data fail\n");
		goto read_massdata_err;
	}

	for (i = 0; i < byte_num; i = i + 2)
		buf[i >> 1] = (int)(u16)((data[i] << 8) + data[i + 1]);

	ret = 0;
read_massdata_err:
	fts_free(data);
	return ret;
}

int short_get_adcdata_incell(u8 retval, u8 ch_num, int byte_num, int *adc_buf)
{
	int ret = 0;
	int times = 0;
	u8 short_state = 0;

	FTS_TEST_FUNC_ENTER();

	/* Start ADC sample */
	ret = fts_test_write_reg(FACTORY_REG_SHORT_TEST_EN, 0x01);
	if (ret) {
		FTS_TEST_SAVE_ERR("start short test fail\n");
		goto adc_err;
	}

	sys_delay(ch_num * FACTORY_TEST_DELAY);
	for (times = 0; times < FACTORY_TEST_RETRY; times++) {
		ret = fts_test_read_reg(FACTORY_REG_SHORT_TEST_STATE, &short_state);
		if ((ret >= 0) && (retval == short_state))
			break;
		FTS_TEST_DBG("reg%x=%x,retry:%d",
					FACTORY_REG_SHORT_TEST_STATE, short_state, times);

		sys_delay(FACTORY_TEST_RETRY_DELAY);
	}
	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("short test timeout, ADC data not OK\n");
		ret = -EIO;
		goto adc_err;
	}

	ret = read_mass_data(FACTORY_REG_SHORT_ADDR, byte_num, adc_buf);
	if (ret)
		FTS_TEST_SAVE_ERR("get short(adc) data fail\n");

adc_err:
	FTS_TEST_FUNC_EXIT();
	return ret;
}

/*
 * wait_state_update - wait fw status update
 */
int wait_state_update(u8 retval)
{
	int ret = 0;
	int times = 0;
	u8 addr = 0;
	u8 state = 0xFF;
	struct fts_test *tdata = fts_ftest;

	if ((tdata == NULL) || (tdata->func == NULL)) {
		FTS_TEST_SAVE_ERR("test/func is null\n");
		return -EINVAL;
	}

	if (tdata->func->hwtype == IC_HW_INCELL)
		addr = FACTORY_REG_PARAM_UPDATE_STATE;
	else
		addr = FACTORY_REG_PARAM_UPDATE_STATE_TOUCH;

	while (times++ < FACTORY_TEST_RETRY) {
		sys_delay(FACTORY_TEST_DELAY);
		/* Wait register status update */
		state = 0xFF;
		ret = fts_test_read_reg(addr, &state);
		if ((ret >= 0) && (retval == state))
			break;
		FTS_TEST_DBG("reg%x=%x,retry:%d", addr, state, times);
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("Wait State Update fail,reg%x=%x\n", addr, state);
		return -EIO;
	}

	return 0;
}

/*
 * start_scan - start to scan a frame
 */
int start_scan(void)
{
	int ret = 0;
	u8 addr = 0;
	u8 val = 0;
	u8 finish_val = 0;
	int times = 0;
	struct fts_test *tdata = fts_ftest;

	if ((tdata == NULL) || (tdata->func == NULL)) {
		FTS_TEST_SAVE_ERR("test/func is null\n");
		return -EINVAL;
	}

	if (tdata->func->startscan_mode == SCAN_SC) {
		/* sc ic */
		addr = FACTORY_REG_SCAN_ADDR2;
		val = 0x01;
		finish_val = 0x00;
	} else {
		addr = DEVIDE_MODE_ADDR;
		val = 0xC0;
		finish_val = 0x40;
	}

	/* write register to start scan */
	ret = fts_test_write_reg(addr, val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write start scan mode fail\n");
		return ret;
	}

	/* Wait for the scan to complete */
	while (times++ < FACTORY_TEST_RETRY) {
		sys_delay(FACTORY_TEST_DELAY);

		ret = fts_test_read_reg(addr, &val);
		if ((ret >= 0) && (val == finish_val))
			break;
		FTS_TEST_DBG("reg%x=%x,retry:%d", addr, val, times);
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("scan timeout\n");
		return -EIO;
	}

	return 0;
}

static int read_rawdata(
	struct fts_test *tdata,
	u8 off_addr,
	u8 off_val,
	u8 rawdata_addr,
	int byte_num,
	int *data)
{
	int ret = 0;

	/* set line addr or rawdata start addr */
	ret = fts_test_write_reg(off_addr, off_val);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wirte line/start addr fail\n");
		return ret;
	}

	if (tdata->func->raw_u16)
		ret = read_mass_data_u16(rawdata_addr, byte_num, data);
	else
		ret = read_mass_data(rawdata_addr, byte_num, data);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		return ret;
	}

	return 0;
}

int get_rawdata(int *data)
{
	int ret = 0;
	u8 val = 0;
	u8 addr = 0;
	u8 rawdata_addr = 0;
	int byte_num = 0;
	struct fts_test *tdata = fts_ftest;

	if ((tdata == NULL) || (tdata->func == NULL)) {
		FTS_TEST_SAVE_ERR("test/func is null\n");
		return -EINVAL;
	}

	/* enter factory mode */
	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		return ret;
	}

	/* start scanning */
	ret = start_scan();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("scan fail\n");
		return ret;
	}

	/* read rawdata */
	if (tdata->func->hwtype == IC_HW_INCELL) {
		val = 0xAD;
		addr = FACTORY_REG_LINE_ADDR;
		rawdata_addr = FACTORY_REG_RAWDATA_ADDR;
	} else if (tdata->func->hwtype == IC_HW_MC_SC) {
		val = 0xAA;
		addr = FACTORY_REG_LINE_ADDR;
		rawdata_addr = FACTORY_REG_RAWDATA_ADDR_MC_SC;
	} else {
		val = 0x0;
		addr = FACTORY_REG_RAWDATA_SADDR_SC;
		rawdata_addr = FACTORY_REG_RAWDATA_ADDR_SC;
	}

	byte_num = tdata->node.node_num * 2;
	ret = read_rawdata(tdata, addr, val, rawdata_addr, byte_num, data);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		return ret;
	}

	return 0;
}

/*
 * chip_clb - auto clb
 */
int chip_clb(void)
{
	int ret = 0;
	u8 val = 0;
	int times = 0;

	/* start clb */
	ret = fts_test_write_reg(FACTORY_REG_CLB, 0x04);
	if (ret) {
		FTS_TEST_SAVE_ERR("write start clb fail\n");
		return ret;
	}

	while (times++ < FACTORY_TEST_RETRY) {
		sys_delay(FACTORY_TEST_RETRY_DELAY);
		ret = fts_test_read_reg(FACTORY_REG_CLB, &val);
		if ((ret == 0) && (val == 0x02)) {
			/* clb ok */
			break;
		}
		FTS_TEST_DBG("reg%x=%x,retry:%d", FACTORY_REG_CLB, val, times);
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("chip clb timeout\n");
		return -EIO;
	}

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
	if (data == NULL) {
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
		if ((i == (packet_num - 1)) && packet_remainder)
			read_num = packet_remainder;

		ret = fts_test_write_reg(FACTORY_REG_CB_ADDR_H, addr_h);
		if (ret) {
			FTS_TEST_SAVE_ERR("write cb addr high fail\n");
			goto TEST_CB_ERR;
		}
		ret = fts_test_write_reg(FACTORY_REG_CB_ADDR_L, addr_l);
		if (ret) {
			FTS_TEST_SAVE_ERR("write cb addr low fail\n");
			goto TEST_CB_ERR;
		}

		ret = fts_test_read(cb_addr, data + offset, read_num);
		if (ret) {
			FTS_TEST_SAVE_ERR("read cb fail\n");
			goto TEST_CB_ERR;
		}
	}

	for (i = 0; i < byte_num; i++)
		cb_buf[i] = data[i];

TEST_CB_ERR:
	fts_free(data);
	return ret;
}

int get_cb_sc(int byte_num, int *cb_buf, enum byte_mode mode)
{
	int ret = 0;
	int i = 0;
	int read_num = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	u8 cb_addr = 0;
	u8 off_addr = 0;
	u8 off_h_addr = 0;
	struct fts_test *tdata = fts_ftest;
	u8 *cb = NULL;

	if ((tdata == NULL) || (tdata->func == NULL)) {
		FTS_TEST_SAVE_ERR("test/func is null\n");
		return -EINVAL;
	}

	cb = (u8 *)fts_malloc(byte_num * sizeof(u8));
	if (cb == NULL) {
		FTS_TEST_SAVE_ERR("malloc memory for cb buffer fail\n");
		return -ENOMEM;
	}

	if (tdata->func->hwtype == IC_HW_MC_SC) {
		cb_addr = FACTORY_REG_MC_SC_CB_ADDR;
		off_addr = FACTORY_REG_MC_SC_CB_ADDR_OFF;
		off_h_addr = FACTORY_REG_MC_SC_CB_H_ADDR_OFF;
	} else if (tdata->func->hwtype == IC_HW_SC) {
		cb_addr = FACTORY_REG_SC_CB_ADDR;
		off_addr = FACTORY_REG_SC_CB_ADDR_OFF;
	}

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;
	read_num = BYTES_PER_TIME;
	offset = 0;

	FTS_TEST_INFO("cb packet:%d,remainder:%d", packet_num, packet_remainder);
	for (i = 0; i < packet_num; i++) {
		if ((i == (packet_num - 1)) && packet_remainder)
			read_num = packet_remainder;

		ret = fts_test_write_reg(off_addr, offset);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write cb addr offset fail\n");
			goto cb_err;
		}

		if (tdata->func->cb_high_support) {
			ret = fts_test_write_reg(off_h_addr, offset >> 8);
			if (ret < 0) {
				FTS_TEST_SAVE_ERR("write cb_h addr offset fail\n");
				goto cb_err;
			}
		}

		ret = fts_test_read(cb_addr, cb + offset, read_num);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read cb fail\n");
			goto cb_err;
		}

		offset += read_num;
	}

	if (mode == DATA_ONE_BYTE) {
		for (i = 0; i < byte_num; i++)
			cb_buf[i] = cb[i];
	} else if (mode == DATA_TWO_BYTE) {
		for (i = 0; i < byte_num; i = i + 2)
			cb_buf[i >> 1] = (int)(((int)(cb[i]) << 8) + cb[i + 1]);
	}

	ret = 0;
cb_err:
	fts_free(cb);
	return ret;
}

bool compare_data(int *data, int min, int max, int min_vk, int max_vk, bool key)
{
	int i = 0;
	bool result = true;
	struct fts_test *tdata = fts_ftest;
	int rx = tdata->node.rx_num;
	int node_va = tdata->node.node_num - tdata->node.key_num;

	if (!data || !tdata->node_valid) {
		FTS_TEST_SAVE_ERR("data/node_valid is null\n");
		return false;
	}

	for (i = 0; i < node_va; i++) {
		if (tdata->node_valid[i] == 0)
			continue;

		if ((data[i] < min) || (data[i] > max)) {
			FTS_TEST_SAVE_ERR("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)\n",
							  i / rx + 1, i % rx + 1, data[i], min, max);
			result = false;
		}
	}

	if (key) {
		for (i = node_va; i < tdata->node.node_num; i++) {
			if (tdata->node_valid[i] == 0)
				continue;

			if ((data[i] < min_vk) || (data[i] > max_vk)) {
				FTS_TEST_SAVE_ERR("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)\n",
								  i / rx + 1, i % rx + 1,
								  data[i], min_vk, max_vk);
				result = false;
			}
		}
	}

	return result;
}

bool compare_array(int *data, int *min, int *max, bool key)
{
	int i = 0;
	bool result = true;
	struct fts_test *tdata = fts_ftest;
	int rx = tdata->node.rx_num;
	int node_num = tdata->node.node_num;

	if (!data || !min || !max || !tdata->node_valid) {
		FTS_TEST_SAVE_ERR("data/min/max/node_valid is null\n");
		return false;
	}

	if (!key)
		node_num -= tdata->node.key_num;
	for (i = 0; i < node_num; i++) {
		if (tdata->node_valid[i] == 0)
			continue;

		if ((data[i] < min[i]) || (data[i] > max[i])) {
			FTS_TEST_SAVE_ERR("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)\n",
							  i / rx + 1, i % rx + 1, data[i], min[i], max[i]);
			result = false;
		}
	}

	return result;
}

/*
 * show_data - show and save test data to testresult.txt
 */
void show_data(int *data, bool key)
{
#if TXT_SUPPORT
	int i = 0;
	int j = 0;
	struct fts_test *tdata = fts_ftest;
	int node_num = tdata->node.node_num;
	int tx_num = tdata->node.tx_num;
	int rx_num = tdata->node.rx_num;

	FTS_TEST_FUNC_ENTER();
	for (i = 0; i < tx_num; i++) {
		FTS_TEST_SAVE_INFO("Ch/Tx_%02d:  ", i + 1);
		for (j = 0; j < rx_num; j++)
			FTS_TEST_SAVE_INFO("%5d, ", data[i * rx_num + j]);
		FTS_TEST_SAVE_INFO("\n");
	}

	if (key) {
		FTS_TEST_SAVE_INFO("Ch/Tx_%02d:  ", tx_num + 1);
		for (i = tx_num * rx_num; i < node_num; i++)
			FTS_TEST_SAVE_INFO("%5d, ",  data[i]);
		FTS_TEST_SAVE_INFO("\n");
	}
	FTS_TEST_FUNC_EXIT();
#endif
}

/* mc_sc only */
/* Only V3 Pattern has mapping & no-mapping */
int mapping_switch(u8 mapping)
{
	int ret = 0;
	u8 val = 0xFF;
	struct fts_test *tdata = fts_ftest;

	if (tdata->v3_pattern) {
		ret = fts_test_read_reg(FACTORY_REG_NOMAPPING, &val);
		if (ret < 0) {
			FTS_TEST_ERROR("read 0x54 register fail");
			return ret;
		}

		if (val != mapping) {
			ret = fts_test_write_reg(FACTORY_REG_NOMAPPING, mapping);
			if (ret < 0) {
				FTS_TEST_ERROR("write 0x54 register fail");
				return ret;
			}
			sys_delay(FACTORY_TEST_DELAY);
		}
	}

	return 0;
}

bool get_fw_wp(u8 wp_ch_sel, enum wp_type water_proof_type)
{
	bool fw_wp_state = false;

	switch (water_proof_type) {
	case WATER_PROOF_ON:
		/* bit5: 0-check in wp on, 1-not check */
		fw_wp_state = !(wp_ch_sel & 0x20);
		break;
	case WATER_PROOF_ON_TX:
		/* Bit6:  0-check Rx+Tx in wp mode  1-check one channel
		 * Bit2:  0-check Tx in wp mode;  1-check Rx in wp mode
		 */
		fw_wp_state = (!(wp_ch_sel & 0x40) || !(wp_ch_sel & 0x04));
		break;
	case WATER_PROOF_ON_RX:
		fw_wp_state = (!(wp_ch_sel & 0x40) || (wp_ch_sel & 0x04));
		break;
	case WATER_PROOF_OFF:
		/* bit7: 0-check in wp off, 1-not check */
		fw_wp_state = !(wp_ch_sel & 0x80);
		break;
	case WATER_PROOF_OFF_TX:
		/* Bit1-0:  00-check Tx in non-wp mode
		 * 01-check Rx in non-wp mode
		 * 10:check Rx+Tx in non-wp mode
		 */
		fw_wp_state = ((0x0 == (wp_ch_sel & 0x03)) || (0x02 == (wp_ch_sel & 0x03)));
		break;
	case WATER_PROOF_OFF_RX:
		fw_wp_state = ((0x01 == (wp_ch_sel & 0x03)) || (0x02 == (wp_ch_sel & 0x03)));
		break;
	default:
		break;
	}

	return fw_wp_state;
}

int get_cb_mc_sc(u8 wp, int byte_num, int *cb_buf, enum byte_mode mode)
{
	int ret = 0;

	/* 1:waterproof 0:non-waterproof */
	ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, wp);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set mc_sc mode fail\n");
		return ret;
	}

	if (fts_ftest->func->param_update_support) {
		ret = wait_state_update(TEST_RETVAL_AA);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("wait state update fail\n");
			return ret;
		}
	}

	/* read cb */
	ret = get_cb_sc(byte_num, cb_buf, mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get sc cb fail\n");
		return ret;
	}

	return 0;
}

int get_rawdata_mc_sc(enum wp_type wp, int *data)
{
	int ret = 0;
	u8 val = 0;
	u8 addr = 0;
	u8 rawdata_addr = 0;
	int byte_num = 0;
	struct fts_test *tdata = fts_ftest;

	if ((tdata == NULL) || (tdata->func == NULL)) {
		FTS_TEST_SAVE_ERR("test/func is null\n");
		return -EINVAL;
	}

	byte_num = tdata->sc_node.node_num * 2;
	addr = FACTORY_REG_LINE_ADDR;
	rawdata_addr = FACTORY_REG_RAWDATA_ADDR_MC_SC;
	if (wp == WATER_PROOF_ON) {
		val = 0xAC;
	} else if (wp == WATER_PROOF_OFF) {
		val = 0xAB;
	} else if (wp == HIGH_SENSITIVITY) {
		val = 0xA0;
	} else if (wp == HOV) {
		val = 0xA1;
		byte_num = 4 * 2;
	}

	ret = read_rawdata(tdata, addr, val, rawdata_addr, byte_num, data);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		return ret;
	}

	return 0;
}

int get_rawdata_mc(u8 fre, u8 fir, int *rawdata)
{
	int ret = 0;
	int i = 0;

	if (rawdata == NULL) {
		FTS_TEST_SAVE_ERR("rawdata buffer is null\n");
		return -EINVAL;
	}

	/* set frequecy high/low */
	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
		return ret;
	}

	/* fir enable/disable */
	ret = fts_test_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set fir fail,ret=%d\n", ret);
		return ret;
	}

	/* get rawdata */
	for (i = 0; i < 3; i++) {
		/* lost 3 frames, in order to obtain stable data */
		ret = get_rawdata(rawdata);
	}
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get rawdata fail,ret=%d\n", ret);
		return ret;
	}

	return 0;
}

int short_get_adc_data_mc(u8 retval, int byte_num, int *adc_buf, u8 mode)
{
	int ret = 0;
	int i = 0;
	u8 short_state = 0;
	u8 short_state_reg = 0;
	u8 short_en_reg = 0;
	u8 short_data_reg = 0;
	struct fts_test *tdata = fts_ftest;

	FTS_TEST_FUNC_ENTER();
	if (tdata->func->mc_sc_short_v2) {
		short_en_reg = FACTROY_REG_SHORT2_TEST_EN;
		short_state_reg = FACTROY_REG_SHORT2_TEST_STATE;
		short_data_reg = FACTORY_REG_SHORT2_ADDR_MC;

		ret = fts_test_write_reg(FACTROY_REG_SHORT2_TEST_STATE, 0x03);
		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write short state fail\n");
			goto test_err;
		}
	} else {
		short_en_reg = FACTROY_REG_SHORT_TEST_EN;
		short_state_reg = FACTROY_REG_SHORT_TEST_EN;
		short_data_reg = FACTORY_REG_SHORT_ADDR_MC;
	}

	/* select short test mode & start test */
	ret = fts_test_write_reg(short_en_reg, mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write short test mode fail\n");
		goto test_err;
	}

	for (i = 0; i < FACTORY_TEST_RETRY; i++) {
		sys_delay(FACTORY_TEST_RETRY_DELAY);

		ret = fts_test_read_reg(short_state_reg, &short_state);
		if ((ret >= 0) && (retval == short_state))
			break;
		FTS_TEST_DBG("reg%x=%x,retry:%d", short_state_reg, short_state, i);
	}
	if (i >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("short test timeout, ADC data not OK\n");
		ret = -EIO;
		goto test_err;
	}

	ret = read_mass_data(short_data_reg, byte_num, adc_buf);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("get short(adc) data fail\n");

	FTS_TEST_DBG("adc data:\n");
	print_buffer(adc_buf, byte_num / 2, 0);
test_err:
	FTS_TEST_FUNC_EXIT();
	return ret;
}

bool compare_mc_sc(bool tx_check, bool rx_check, int *data, int *min, int *max)
{
	int i = 0;
	bool result = true;
	struct fts_test *tdata = fts_ftest;

	if (rx_check) {
		for (i = 0; i < tdata->sc_node.rx_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if ((data[i] < min[i]) || (data[i] > max[i])) {
				FTS_TEST_SAVE_ERR("test fail,rx%d=%5d,range=(%5d,%5d)\n",
								  i + 1, data[i], min[i], max[i]);
				result = false;
			}
		}
	}

	if (tx_check) {
		for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++) {
			if (tdata->node_valid_sc[i] == 0)
				continue;

			if ((data[i] < min[i]) || (data[i] > max[i])) {
				FTS_TEST_SAVE_INFO("test fail,tx%d=%5d,range=(%5d,%5d)\n",
								   i - tdata->sc_node.rx_num + 1,
								   data[i], min[i], max[i]);
				result = false;
			}
		}
	}

	return result;
}

void show_data_mc_sc(int *data)
{
	int i = 0;
	struct fts_test *tdata = fts_ftest;

	FTS_TEST_SAVE_INFO("SCap Rx: ");
	for (i = 0; i < tdata->sc_node.rx_num; i++)
		FTS_TEST_SAVE_INFO("%5d, ", data[i]);
	FTS_TEST_SAVE_INFO("\n");

	FTS_TEST_SAVE_INFO("SCap Tx: ");
	for (i = tdata->sc_node.rx_num; i < tdata->sc_node.node_num; i++)
		FTS_TEST_SAVE_INFO("%5d, ", data[i]);
	FTS_TEST_SAVE_INFO("\n");
}
/* mc_sc end*/

#if CSV_SUPPORT || TXT_SUPPORT
static int fts_test_save_test_data(char *file_name, char *data_buf, int len)
{
	struct file *pfile = NULL;
	char filepath[FILE_NAME_LENGTH] = { 0 };
	loff_t pos;
	mm_segment_t old_fs;

	FTS_TEST_FUNC_ENTER();
	memset(filepath, 0, sizeof(filepath));
	snprintf(filepath, FILE_NAME_LENGTH, "%s%s", FTS_INI_FILE_PATH, file_name);
	FTS_INFO("save test data to %s", filepath);
	if (pfile == NULL)
		pfile = filp_open(filepath, O_TRUNC | O_CREAT | O_RDWR, 0);
	if (IS_ERR(pfile)) {
		FTS_TEST_ERROR("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	kernel_write(pfile, data_buf, len, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

#if defined(TEST_SAVE_FAIL_RESULT) && TEST_SAVE_FAIL_RESULT
void fts_test_save_fail_result(
	struct fts_test *tdata, char *prefix, char *suffix, char *buf, int len)
{
	char file_name[128];

	if (false == tdata->result) {
		snprintf(file_name, 128, "%s_%ld_%ld%s", prefix,
				 (long)tdata->tv.tv_sec, (long)tdata->tv.tv_usec, suffix);
		fts_test_save_test_data(file_name, buf, len);
	}
}
#endif
#endif

static int fts_test_malloc_free_data_txt(struct fts_test *tdata, bool allocate)
{
#if TXT_SUPPORT
	if (true == allocate) {
		tdata->testresult = vmalloc(TXT_BUFFER_LEN);
		if (tdata->testresult == NULL) {
			FTS_TEST_ERROR("tdata->testresult malloc fail\n");
			return -ENOMEM;
		}

		tdata->testresult_len = 0;
		FTS_TEST_SAVE_INFO("FW version:0x%02x\n", tdata->fw_ver);
		FTS_TEST_SAVE_INFO("tx_num:%d, rx_num:%d, key_num:%d\n",
						   tdata->node.tx_num, tdata->node.rx_num,
						   tdata->node.key_num);
	} else {
		if (tdata->testresult) {
			vfree(tdata->testresult);
			tdata->testresult = NULL;
		}
	}
#endif
	return 0;
}

#if CSV_SUPPORT
static int fts_test_get_item_count_scap_csv(int index)
{
	int ret = 0;
	int i = 0;
	int select = 0;
	u8 wc_sel = 0;
	u8 hc_sel = 0;
	u8 scap_select[4] = { 0 };

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		return index;
	}

	ret = fts_test_read_reg(FACTORY_REG_HC_SEL, &hc_sel);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read high_channel_sel fail,ret=%d\n", ret);
		return index;
	}

	scap_select[0] = get_fw_wp(wc_sel, WATER_PROOF_ON);
	scap_select[1] = get_fw_wp(wc_sel, WATER_PROOF_OFF);
	scap_select[2] = (hc_sel & 0x03) ? 1 : 0;
	scap_select[3] = (hc_sel & 0x04) ? 1 : 0;

	for (i = 0; i < 4; i++) {
		if (scap_select[i])
			select++;
		if (select == index)
			break;
	}

	return (i + 1);
}
#endif

static void fts_test_save_data_csv(struct fts_test *tdata)
{
#if CSV_SUPPORT
	int i = 0;
	int j = 0;
	int index = 0;
	int k = 0;
	int tx = 0;
	int rx = 0;
	int node_num = 0;
	int offset = 0;
	int start_line = 11;
	int data_count = 0;
	char *csv_buffer = NULL;
	char *line2_buffer = NULL;
	int csv_length = 0;
	int line2_length = 0;
	int csv_item_count = 0;
	struct fts_test_data *td = &tdata->testdata;
	struct item_info *info = NULL;

	FTS_TEST_INFO("save data in csv format");
	csv_buffer = vmalloc(CSV_BUFFER_LEN);
	if (!csv_buffer) {
		FTS_TEST_ERROR("csv_buffer malloc fail\n");
		return;
	}

	line2_buffer = vmalloc(CSV_LINE2_BUFFER_LEN);
	if (!line2_buffer) {
		FTS_TEST_ERROR("line2_buffer malloc fail\n");
		goto csv_save_err;
	}

	FTS_TEST_INFO("test item count:%d", td->item_count);
	/* line 1 */
	csv_length += snprintf(csv_buffer + csv_length,
			CSV_BUFFER_LEN - csv_length,
			"ECC, 85, 170, IC Name, %s, IC Code, %x\n",
			tdata->ini.ic_name,
			(tdata->ini.ic_code >> IC_CODE_OFFSET));

	/* line 2 */
	for (i = 0; i < td->item_count; i++) {
		info = &td->info[i];
		if (info->mc_sc) {
			node_num = tdata->sc_node.node_num;
			/* set max len of tx/rx to column */
			rx = (tdata->sc_node.tx_num > tdata->sc_node.rx_num)
				 ? tdata->sc_node.tx_num : tdata->sc_node.rx_num;
		} else {
			if (info->key_support && (tdata->node.key_num > 0))
				node_num = (tdata->node.tx_num + 1) * tdata->node.rx_num;
			else
				node_num = tdata->node.tx_num * tdata->node.rx_num;
			rx = tdata->node.rx_num;
		}

		if (info->datalen > node_num) {
			data_count = (info->datalen - 1) / node_num + 1;
			tx = (node_num - 1) / rx + 1;
		} else {
			data_count = 1;
			tx = ((info->datalen - 1) / rx) + 1;
		}

		for (j = 1; j <= data_count; j++) {
			index = j;

			if (tdata->func->hwtype == IC_HW_MC_SC) {
				/*MC_SC, rawdata index will be 2*/
				if ((info->code == CODE_M_RAWDATA_TEST) && (data_count == 1))
					index = 2;

				/*MC_SC, SCAP index will be 1~4*/
				if ((info->code == CODE_M_SCAP_CB_TEST)
					|| (info->code == CODE_M_SCAP_RAWDATA_TEST))
					index = fts_test_get_item_count_scap_csv(j);
			}

			line2_length += snprintf(line2_buffer + line2_length,
					CSV_LINE2_BUFFER_LEN - line2_length,
					"%s, %d, %d, %d, %d, %d, ",
					info->name, info->code, tx, rx,
					start_line, index);
			start_line += tx;
			csv_item_count++;
		}
	}

	csv_length += snprintf(csv_buffer + csv_length,
			CSV_BUFFER_LEN - csv_length,
			"TestItem Num, %d, ",
			csv_item_count);

	if (line2_length > 0) {
		csv_length += snprintf(csv_buffer + csv_length,
				CSV_BUFFER_LEN - csv_length,
				"%s", line2_buffer);
	}

	/* line 3 ~ 10  "\n" */
	csv_length += snprintf(csv_buffer + csv_length,
			CSV_BUFFER_LEN - csv_length,
			"\n\n\n\n\n\n\n\n\n");

	/* line 11 ~ data area */
	for (i = 0; i < td->item_count; i++) {
		info = &td->info[i];
		if (!info->data) {
			FTS_TEST_ERROR("test item data is null");
			goto csv_save_err;
		}

		if (info->mc_sc) {
			offset = 0;
			for (j = 0; j < info->datalen;) {
				for (k = 0; k < tdata->sc_node.node_num; k++) {
					csv_length += snprintf(csv_buffer + csv_length,
							CSV_BUFFER_LEN - csv_length,
							"%d, ", info->data[offset + k]);
					if ((k + 1) == tdata->sc_node.rx_num) {
						csv_length += snprintf(csv_buffer + csv_length,
								CSV_BUFFER_LEN - csv_length,
								"\n");
					}
				}
				csv_length += snprintf(csv_buffer + csv_length,
						CSV_BUFFER_LEN - csv_length,
						"\n");
				offset += k;
				j += k;
			}
		} else {
			for (j = 0; j < info->datalen; j++) {
				csv_length += snprintf(csv_buffer + csv_length,
						CSV_BUFFER_LEN - csv_length,
						"%d, ", info->data[j]);
				if (((j + 1) % tdata->node.rx_num) == 0) {
					csv_length += snprintf(csv_buffer + csv_length,
							CSV_BUFFER_LEN - csv_length,
							"\n");
				}
			}
		}
	}
	FTS_TEST_INFO("csv length:%d", csv_length);
	fts_test_save_test_data(FTS_CSV_FILE_NAME, csv_buffer, csv_length);

#if defined(TEST_SAVE_FAIL_RESULT) && TEST_SAVE_FAIL_RESULT
	fts_test_save_fail_result(tdata, "testdata_fail", ".csv",
							  csv_buffer, csv_length);
#endif


csv_save_err:
	if (line2_buffer) {
		vfree(line2_buffer);
		line2_buffer = NULL;
	}

	if (csv_buffer) {
		vfree(csv_buffer);
		csv_buffer = NULL;
	}
#endif
}

static void fts_test_save_data_csv_private(struct fts_test *tdata)
{
#if CSV_SUPPORT
	char *csv_buffer = NULL;
	int csv_length = 0;

	FTS_TEST_INFO("save data in csv format");
	csv_buffer = vmalloc(CSV_BUFFER_LEN);
	if (!csv_buffer) {
		FTS_TEST_ERROR("csv_buffer malloc fail\n");
		return;
	}

	if (tdata->func && tdata->func->save_data_private)
		tdata->func->save_data_private(csv_buffer, &csv_length);

	FTS_TEST_INFO("csv length:%d", csv_length);
	fts_test_save_test_data(FTS_CSV_FILE_NAME, csv_buffer, csv_length);

#if defined(TEST_SAVE_FAIL_RESULT) && TEST_SAVE_FAIL_RESULT
	fts_test_save_fail_result(tdata, "testdata_fail", ".csv",
							  csv_buffer, csv_length);
#endif

	if (csv_buffer) {
		vfree(csv_buffer);
		csv_buffer = NULL;
	}
#endif
}

static void fts_test_save_result_txt(struct fts_test *tdata)
{
#if TXT_SUPPORT
	if (!tdata || !tdata->testresult) {
		FTS_TEST_ERROR("test result is null");
		return;
	}

	FTS_TEST_INFO("test result length in txt:%d", tdata->testresult_len);
	fts_test_save_test_data(FTS_TXT_FILE_NAME, tdata->testresult,
							tdata->testresult_len);

#if defined(TEST_SAVE_FAIL_RESULT) && TEST_SAVE_FAIL_RESULT
	fts_test_save_fail_result(tdata, "testresult_fail", ".txt",
							  tdata->testresult, tdata->testresult_len);
#endif

#endif
}

/*****************************************************************************
 * Name: fts_test_save_data
 * Brief: Save test data.
 *        If multi-data of MC, length of data package must be tx*rx,(tx+1)*rx
 *        If multi-data of MC-SC, length of data package should be (tx+rx)*2
 *        Need fill 0 when no actual data
 * Input:
 * Output:
 * Return:
 *****************************************************************************/
void fts_test_save_data(char *name, int code, int *data, int datacnt,
						bool mc_sc, bool key, bool result)
{
	int datalen = datacnt;
	struct fts_test *tdata = fts_ftest;
	struct fts_test_data *td = &tdata->testdata;
	struct item_info *info = &td->info[td->item_count];

	if (!name || !data) {
		FTS_TEST_ERROR("name/data is null");
		return;
	}

	strlcpy(info->name, name, TEST_ITEM_NAME_MAX - 1);
	info->code = code;
	info->mc_sc = mc_sc;
	info->key_support = key;
	info->result = result;
	if (datalen <= 0) {
		if (mc_sc)
			datalen = tdata->sc_node.node_num * 2;
		else {
			if (key && (tdata->node.key_num > 0))
				datalen = (tdata->node.tx_num + 1) * tdata->node.rx_num;
			else
				datalen = tdata->node.tx_num * tdata->node.rx_num;

		}
	}

	FTS_TEST_DBG("name:%s,len:%d", name, datalen);
	info->data = fts_malloc(datalen * sizeof(int));
	if (!info->data) {
		FTS_TEST_ERROR("malloc memory for item(%d) data fail", td->item_count);
		info->datalen = 0;
		return;
	}
	memcpy(info->data, data, datalen * sizeof(int));
	info->datalen = datalen;

	td->item_count++;
}

static void fts_test_free_data(struct fts_test *tdata)
{
	int i = 0;
	struct fts_test_data *td = &tdata->testdata;

	for (i = 0; i < td->item_count; i++) {
		if (td->info[i].data)
			fts_free(td->info[i].data);
	}
}

static int fts_test_malloc_free_incell(struct fts_test *tdata, bool allocate)
{
	struct incell_threshold *thr = &tdata->ic.incell.thr;
	int buflen = tdata->node.node_num * sizeof(int);

	if (true == allocate) {
		FTS_TEST_INFO("buflen:%d", buflen);
		fts_malloc_r(thr->rawdata_min, buflen);
		fts_malloc_r(thr->rawdata_max, buflen);
		if (tdata->func->rawdata2_support) {
			fts_malloc_r(thr->rawdata2_min, buflen);
			fts_malloc_r(thr->rawdata2_max, buflen);
		}
		fts_malloc_r(thr->cb_min, buflen);
		fts_malloc_r(thr->cb_max, buflen);
	} else {
		fts_free(thr->rawdata_min);
		fts_free(thr->rawdata_max);
		if (tdata->func->rawdata2_support) {
			fts_free(thr->rawdata2_min);
			fts_free(thr->rawdata2_max);
		}
		fts_free(thr->cb_min);
		fts_free(thr->cb_max);
	}

	return 0;
}

static int fts_test_malloc_free_mc_sc(struct fts_test *tdata, bool allocate)
{
	struct mc_sc_threshold *thr = &tdata->ic.mc_sc.thr;
	int buflen = tdata->node.node_num * sizeof(int);
	int buflen_sc = tdata->sc_node.node_num * sizeof(int);

	if (true == allocate) {
		fts_malloc_r(thr->rawdata_h_min, buflen);
		fts_malloc_r(thr->rawdata_h_max, buflen);
		if (tdata->func->rawdata2_support) {
			fts_malloc_r(thr->rawdata_l_min, buflen);
			fts_malloc_r(thr->rawdata_l_max, buflen);
		}
		fts_malloc_r(thr->tx_linearity_max, buflen);
		fts_malloc_r(thr->tx_linearity_min, buflen);
		fts_malloc_r(thr->rx_linearity_max, buflen);
		fts_malloc_r(thr->rx_linearity_min, buflen);

		fts_malloc_r(thr->mcap_cmb_min, buflen);
		fts_malloc_r(thr->mcap_cmb_max, buflen);

		fts_malloc_r(thr->scap_cb_off_min, buflen_sc);
		fts_malloc_r(thr->scap_cb_off_max, buflen_sc);
		fts_malloc_r(thr->scap_cb_on_min, buflen_sc);
		fts_malloc_r(thr->scap_cb_on_max, buflen_sc);
		fts_malloc_r(thr->scap_cb_hi_min, buflen_sc);
		fts_malloc_r(thr->scap_cb_hi_max, buflen_sc);
		fts_malloc_r(thr->scap_cb_hov_min, buflen_sc);
		fts_malloc_r(thr->scap_cb_hov_max, buflen_sc);
		fts_malloc_r(thr->scap_cb_off_cf_min, buflen_sc);
		fts_malloc_r(thr->scap_cb_off_cf_max, buflen_sc);
		fts_malloc_r(thr->scap_cb_on_cf_min, buflen_sc);
		fts_malloc_r(thr->scap_cb_on_cf_max, buflen_sc);
		fts_malloc_r(thr->scap_cb_hi_cf_min, buflen_sc);
		fts_malloc_r(thr->scap_cb_hi_cf_max, buflen_sc);

		fts_malloc_r(thr->scap_rawdata_off_min, buflen_sc);
		fts_malloc_r(thr->scap_rawdata_off_max, buflen_sc);
		fts_malloc_r(thr->scap_rawdata_on_min, buflen_sc);
		fts_malloc_r(thr->scap_rawdata_on_max, buflen_sc);
		fts_malloc_r(thr->scap_rawdata_hi_min, buflen_sc);
		fts_malloc_r(thr->scap_rawdata_hi_max, buflen_sc);
		fts_malloc_r(thr->scap_rawdata_hov_min, buflen_sc);
		fts_malloc_r(thr->scap_rawdata_hov_max, buflen_sc);

		fts_malloc_r(thr->panel_differ_min, buflen);
		fts_malloc_r(thr->panel_differ_max, buflen);
		fts_malloc_r(thr->noise_min, buflen);
		fts_malloc_r(thr->noise_max, buflen);
	} else {
		fts_free(thr->rawdata_h_min);
		fts_free(thr->rawdata_h_max);
		if (tdata->func->rawdata2_support) {
			fts_free(thr->rawdata_l_min);
			fts_free(thr->rawdata_l_max);
		}
		fts_free(thr->tx_linearity_max);
		fts_free(thr->tx_linearity_min);
		fts_free(thr->rx_linearity_max);
		fts_free(thr->rx_linearity_min);

		fts_free(thr->mcap_cmb_min);
		fts_free(thr->mcap_cmb_max);

		fts_free(thr->scap_cb_off_min);
		fts_free(thr->scap_cb_off_max);
		fts_free(thr->scap_cb_on_min);
		fts_free(thr->scap_cb_on_max);
		fts_free(thr->scap_cb_hi_min);
		fts_free(thr->scap_cb_hi_max);
		fts_free(thr->scap_cb_hov_min);
		fts_free(thr->scap_cb_hov_max);
		fts_free(thr->scap_cb_off_cf_min);
		fts_free(thr->scap_cb_off_cf_max);
		fts_free(thr->scap_cb_on_cf_min);
		fts_free(thr->scap_cb_on_cf_max);
		fts_free(thr->scap_cb_hi_cf_min);
		fts_free(thr->scap_cb_hi_cf_max);

		fts_free(thr->scap_rawdata_off_min);
		fts_free(thr->scap_rawdata_off_max);
		fts_free(thr->scap_rawdata_on_min);
		fts_free(thr->scap_rawdata_on_max);
		fts_free(thr->scap_rawdata_hi_min);
		fts_free(thr->scap_rawdata_hi_max);
		fts_free(thr->scap_rawdata_hov_min);
		fts_free(thr->scap_rawdata_hov_max);

		fts_free(thr->panel_differ_min);
		fts_free(thr->panel_differ_max);
		fts_free(thr->noise_min);
		fts_free(thr->noise_max);
	}

	return 0;
}

static int fts_test_malloc_free_sc(struct fts_test *tdata, bool allocate)
{
	struct sc_threshold *thr = &tdata->ic.sc.thr;
	int buflen = tdata->node.node_num * sizeof(int);

	if (true == allocate) {
		fts_malloc_r(thr->rawdata_min, buflen);
		fts_malloc_r(thr->rawdata_max, buflen);
		fts_malloc_r(thr->cb_min, buflen);
		fts_malloc_r(thr->cb_max, buflen);
		fts_malloc_r(thr->dcb_sort, buflen);
		fts_malloc_r(thr->dcb_base, buflen);
	} else {
		fts_free(thr->rawdata_min);
		fts_free(thr->rawdata_max);
		fts_free(thr->cb_min);
		fts_free(thr->cb_max);
		fts_free(thr->dcb_sort);
		fts_free(thr->dcb_base);
	}

	return 0;
}

static int fts_test_malloc_free_thr(struct fts_test *tdata, bool allocate)
{
	int ret = 0;

	if ((tdata == NULL) || (tdata->func == NULL)) {
		FTS_TEST_SAVE_ERR("tdata/func is NULL\n");
		return -EINVAL;
	}

	if (true == allocate) {
		fts_malloc_r(tdata->node_valid, tdata->node.node_num * sizeof(int));
		fts_malloc_r(tdata->node_valid_sc, tdata->sc_node.node_num * sizeof(int));
	} else {
		fts_free(tdata->node_valid);
		fts_free(tdata->node_valid_sc);
	}

	switch (tdata->func->hwtype) {
	case IC_HW_INCELL:
		ret = fts_test_malloc_free_incell(tdata, allocate);
		break;
	case IC_HW_MC_SC:
		ret = fts_test_malloc_free_mc_sc(tdata, allocate);
		break;
	case IC_HW_SC:
		ret = fts_test_malloc_free_sc(tdata, allocate);
		break;
	default:
		FTS_TEST_SAVE_ERR("test ic type(%d) fail\n", tdata->func->hwtype);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* default enable all test item */
static void fts_test_init_item(struct fts_test *tdata)
{
	switch (tdata->func->hwtype) {
	case IC_HW_INCELL:
		tdata->ic.incell.u.tmp = 0xFFFFFFFF;
		break;
	case IC_HW_MC_SC:
		tdata->ic.mc_sc.u.tmp = 0xFFFFFFFF;
		break;
	case IC_HW_SC:
		tdata->ic.sc.u.tmp = 0xFFFFFFFF;
		break;
	}
}

static int get_tx_rx_num(u8 tx_rx_reg, u8 *ch_num, u8 ch_num_max)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < 3; i++) {
		ret = fts_test_read_reg(tx_rx_reg, ch_num);
		if ((ret < 0) || (*ch_num > ch_num_max))
			sys_delay(50);
		else
			break;
		if (i >= 3) {
			FTS_TEST_ERROR("get channel num fail");
			return -EIO;
		}
	}

	return 0;
}
static int get_key_num(int *key_num_en, int max_key_num)
{
	int ret = 0;
	u8 key_en = 0;

	if (!max_key_num) {
		FTS_TEST_DBG("not support key, don't read key num register");
		return 0;
	}

	ret = fts_test_read_reg(FACTORY_REG_LEFT_KEY, &key_en);
	if (ret >= 0) {
		if (key_en & 0x01)
			(*key_num_en)++;

		if (key_en & 0x02)
			(*key_num_en)++;

		if (key_en & 0x04)
			(*key_num_en)++;
	}

	ret = fts_test_read_reg(FACTORY_REG_RIGHT_KEY, &key_en);
	if (ret >= 0) {
		if (key_en & 0x01)
			(*key_num_en)++;

		if (key_en & 0x02)
			(*key_num_en)++;

		if (key_en & 0x04)
			(*key_num_en)++;
	}

	if (*key_num_en > max_key_num) {
		FTS_TEST_ERROR("get key num, fw:%d > max:%d", *key_num_en, max_key_num);
		return -EIO;
	}

	return ret;
}

static int get_channel_num(struct fts_test *tdata)
{
	int ret = 0;
	u8 tx_num = 0;
	u8 rx_num = 0;
	int key_num = 0;

	/* node structure */
	if (tdata->func->hwtype == IC_HW_SC) {
		ret = get_tx_rx_num(FACTORY_REG_CH_NUM_SC, &tx_num, NUM_MAX_SC);
		if (ret < 0) {
			FTS_TEST_ERROR("get channel number fail");
			return ret;
		}

		ret = get_tx_rx_num(FACTORY_REG_KEY_NUM_SC, &rx_num, KEY_NUM_MAX);
		if (ret < 0) {
			FTS_TEST_ERROR("get key number fail");
			return ret;
		}

		tdata->node.tx_num = 1;
		tdata->node.rx_num = tx_num;
		tdata->node.channel_num = tx_num;
		tdata->node.node_num = tx_num;
		key_num = rx_num;
	} else {
		ret = get_tx_rx_num(FACTORY_REG_CHX_NUM, &tx_num, TX_NUM_MAX);
		FTS_TEST_INFO("get tx_num:  %d", tx_num);
		if (ret < 0) {
			FTS_TEST_ERROR("get tx_num fail");
			return ret;
		}

		ret = get_tx_rx_num(FACTORY_REG_CHY_NUM, &rx_num, RX_NUM_MAX);
		FTS_TEST_INFO("get rx_num:  %d", rx_num);
		if (ret < 0) {
			FTS_TEST_ERROR("get rx_num fail");
			return ret;
		}

		if (tdata->func->hwtype == IC_HW_INCELL) {
			ret = get_key_num(&key_num, tdata->func->key_num_total);
			if (ret < 0) {
				FTS_TEST_ERROR("get key_num fail");
				return ret;
			}
		} else if (tdata->func->hwtype == IC_HW_MC_SC)
			key_num = tdata->func->key_num_total;
		tdata->node.tx_num = tx_num;
		tdata->node.rx_num = rx_num;
		if (tdata->func->hwtype == IC_HW_INCELL)
			tdata->node.channel_num = tx_num * rx_num;
		else if (tdata->func->hwtype == IC_HW_MC_SC)
			tdata->node.channel_num = tx_num + rx_num;
		tdata->node.node_num = tx_num * rx_num;
	}

	/* key */
	tdata->node.key_num = key_num;
	tdata->node.node_num += tdata->node.key_num;

	/* sc node structure */
	tdata->sc_node = tdata->node;
	if (tdata->func->hwtype == IC_HW_MC_SC) {
		if (tdata->v3_pattern) {
			ret = get_tx_rx_num(FACTORY_REG_CHX_NUM_NOMAP, &tx_num, TX_NUM_MAX);
			if (ret < 0) {
				FTS_TEST_ERROR("get no-mappint tx_num fail");
				return ret;
			}

			ret = get_tx_rx_num(FACTORY_REG_CHY_NUM_NOMAP, &rx_num, TX_NUM_MAX);
			if (ret < 0) {
				FTS_TEST_ERROR("get no-mapping rx_num fail");
				return ret;
			}

			tdata->sc_node.tx_num = tx_num;
			tdata->sc_node.rx_num = rx_num;
		}
		tdata->sc_node.channel_num = tx_num + rx_num;
		tdata->sc_node.node_num = tx_num + rx_num;
	}

	if (tdata->node.tx_num > TX_NUM_MAX) {
		FTS_TEST_ERROR("tx num(%d) fail", tdata->node.tx_num);
		return -EIO;
	}

	if (tdata->node.rx_num > RX_NUM_MAX) {
		FTS_TEST_ERROR("rx num(%d) fail", tdata->node.rx_num);
		return -EIO;
	}

	FTS_TEST_INFO("node_num:%d, tx:%d, rx:%d, key:%d",
				  tdata->node.node_num, tdata->node.tx_num,
				  tdata->node.rx_num, tdata->node.key_num);
	return 0;
}

static int fts_test_init_basicinfo(struct fts_test *tdata)
{
	int ret = 0;
	u8 val = 0;

	if ((tdata == NULL) || (tdata->func == NULL)) {
		FTS_TEST_SAVE_ERR("tdata/func is NULL\n");
		return -EINVAL;
	}

	fts_test_read_reg(REG_FW_VERSION, &val);
	tdata->fw_ver = val;

	if (tdata->func->hwtype == IC_HW_INCELL) {
		fts_test_read_reg(REG_VA_TOUCH_THR, &val);
		tdata->va_touch_thr = val;
		fts_test_read_reg(REG_VKEY_TOUCH_THR, &val);
		tdata->vk_touch_thr = val;
	}

	/* enter factory mode */
	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail\n");
		return ret;
	}

	if (tdata->func->hwtype == IC_HW_MC_SC) {
		fts_test_read_reg(FACTORY_REG_PATTERN, &val);
		tdata->v3_pattern = (val == 1) ? true : false;
		fts_test_read_reg(FACTORY_REG_NOMAPPING, &val);
		tdata->mapping = val;
	}

	/* enter into factory mode and read tx/rx num */
	ret = get_channel_num(tdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get channel number fail\n");
		return ret;
	}

	return ret;
}

static int fts_test_main_init(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;

	FTS_TEST_FUNC_ENTER();
	/* Init fts_test_data to 0 before test,  */
	memset(&tdata->testdata, 0, sizeof(struct fts_test_data));

	/* get basic information: tx/rx num ... */
	ret = fts_test_init_basicinfo(tdata);
	if (ret < 0) {
		FTS_TEST_ERROR("test init basicinfo fail");
		return ret;
	}

	/* allocate memory for test threshold */
	ret = fts_test_malloc_free_thr(tdata, true);
	if (ret < 0) {
		FTS_TEST_ERROR("test malloc for threshold fail");
		return ret;
	}

	/* default enable all test item */
	fts_test_init_item(tdata);

	ret = fts_test_malloc_free_data_txt(tdata, true);
	if (ret < 0) {
		FTS_TEST_ERROR("allocate memory for test data(txt) fail");
		return ret;
	}

	/* allocate test data buffer */
	tdata->buffer_length = (tdata->node.tx_num + 1) * tdata->node.rx_num;
	tdata->buffer_length *= sizeof(int) * 2;
	FTS_TEST_INFO("test buffer length:%d", tdata->buffer_length);
	tdata->buffer = (int *)fts_malloc(tdata->buffer_length);
	if (tdata->buffer == NULL) {
		FTS_TEST_ERROR("test buffer(%d) malloc fail", tdata->buffer_length);
		return -ENOMEM;
	}
	memset(tdata->buffer, 0, tdata->buffer_length);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int fts_test_main_exit(void)
{
	struct fts_test *tdata = fts_ftest;

	FTS_TEST_FUNC_ENTER();
	if (tdata->func && tdata->func->save_data_private)
		fts_test_save_data_csv_private(tdata);
	else
		fts_test_save_data_csv(tdata);
	fts_test_save_result_txt(tdata);

	/* free memory */
	fts_test_malloc_free_data_txt(tdata, false);
	fts_test_malloc_free_thr(tdata, false);

	/* free test data */
	fts_test_free_data(tdata);

	/*free test data buffer*/
	fts_free(tdata->buffer);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

static void fts_free_test_memory(void)
{
	struct fts_test *tdata = fts_ftest;

	FTS_TEST_FUNC_ENTER();
	/* free memory */
	fts_test_malloc_free_data_txt(tdata, false);
	fts_test_malloc_free_thr(tdata, false);

	/* free test data */
	fts_test_free_data(tdata);

	/* free test data buffer */
	fts_free(tdata->buffer);

	vfree(tdata->csv_data_buffer);
	tdata->csv_data_buffer = NULL;

	FTS_TEST_FUNC_EXIT();
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

static int fts_test_start(void)
{
	int testresult = 0;
	struct fts_test *tdata = fts_ftest;

	if (tdata && tdata->func && tdata->func->start_test) {
		tdata->testdata.item_count = 0;
		testresult = tdata->func->start_test();
	} else
		FTS_TEST_ERROR("test func/start_test func is null");

	return testresult;
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

	/* test initialize */
	ret = fts_test_main_init();
	if (ret < 0) {
		FTS_TEST_ERROR("fts_test_main_init fail");
		goto test_err;
	}

	/*Read parse configuration file*/
	FTS_TEST_SAVE_INFO("ini_file_name:%s\n", ini_file_name);
	ret = fts_test_get_testparams(ini_file_name);
	if (ret < 0) {
		FTS_TEST_ERROR("get testparam fail");
		goto test_err;
	}

	/* Start testing according to the test configuration */
	if (true == fts_test_start()) {
		FTS_TEST_SAVE_INFO("\n\n=======Tp test pass.\n");
		fts_ftest->result = true;
	} else {
		FTS_TEST_SAVE_INFO("\n\n=======Tp test failure.\n");
		fts_ftest->result = false;
#if defined(TEST_SAVE_FAIL_RESULT) && TEST_SAVE_FAIL_RESULT
		do_gettimeofday(&(fts_ftest->tv));
#endif
	}

	ret = 0;
test_err:
	fts_test_main_exit();
	enter_work_mode();
	return ret;
}

static ssize_t fts_test_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!fts_ftest)
		return -EINVAL;
	if (fts_ftest->result)
		return snprintf(buf, 5, "PASS");
	else
		return snprintf(buf, 5, "FAIL");
	return -EPERM;
}

static ssize_t fts_test_store(
	struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	char fwname[FILE_NAME_LENGTH] = { 0 };
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	if (ts_data->suspended) {
		FTS_INFO("In suspend, no test, return now");
		return -EINVAL;
	}

	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, FILE_NAME_LENGTH, "%s", buf);
	fwname[count - 1] = '\0';
	FTS_TEST_DBG("fwname:%s.", fwname);

	mutex_lock(&input_dev->mutex);
	fts_irq_disable();
	fts_esdcheck_switch(ts_data, DISABLE);

	ret = fts_enter_test_environment(1);
	if (ret < 0)
		FTS_ERROR("enter test environment fail");
	else
		fts_test_entry(fwname);
	ret = fts_enter_test_environment(0);
	if (ret < 0)
		FTS_ERROR("enter normal environment fail");

	fts_esdcheck_switch(ts_data, ENABLE);
	fts_irq_enable();
	mutex_unlock(&input_dev->mutex);

	return count;
}

/* test from test.ini
 * example:echo "***.ini" > fts_test
 */
static DEVICE_ATTR_RW(fts_test);

static struct attribute *fts_test_attributes[] = {
	&dev_attr_fts_test.attr,
	NULL
};

static struct attribute_group fts_test_attribute_group = {
	.attrs = fts_test_attributes
};

int tp_selftest_result;

static int tp_selftest_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t tp_selftest_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	char tmp[5];
	int cnt;

	if (*pos != 0)
		return 0;

	cnt = snprintf(tmp, sizeof(tp_selftest_result), "%d\n", tp_selftest_result);
	if (copy_to_user(buf, tmp, strlen(tmp)))
		return -EFAULT;
	*pos += cnt;
	return cnt;
}

ssize_t tp_selftest_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char tmp[6];
	int ret;
	struct fts_test *tdata = fts_ftest;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev;
	char *ini_file_name = "Conf_MultipleTest.ini";

	tp_selftest_result = SELFTEST_INVALID;
	if (ts_data->suspended) {
		FTS_INFO("In suspend, no test, return now");
		return -EINVAL;
	}

	input_dev = ts_data->input_dev;

	if (!tdata || !tdata->func || count > sizeof(tmp)) {
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(tmp, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	mutex_lock(&input_dev->mutex);
	fts_irq_disable();
	/* test initialize */
	ret = fts_test_main_init();
	if (ret < 0) {
		FTS_TEST_ERROR("fts_test_main_init error.");
		goto test_err;
	}

	ret = fts_test_get_testparams(ini_file_name);

	if (ret < 0) {
		FTS_TEST_ERROR("get testparam fail");
		goto test_err;
	}

	if (!strncmp(tmp, "short", 5) && tdata->func->short_test)
		tp_selftest_result = tdata->func->short_test();
	else if (!strncmp(tmp, "open", 4) && tdata->func->open_test)
		tp_selftest_result = tdata->func->open_test();
	else if (!strncmp(tmp, "i2c", 3))
		tp_selftest_result = tdata->func->spi_test();

	ret = tp_selftest_result;

	fts_test_main_exit();
	fts_free_test_memory();
	enter_work_mode();
test_err:
	fts_irq_enable();
	mutex_unlock(&input_dev->mutex);
out:
	if (ret >= 0)
		ret = count;
	return ret;
}

int tp_selftest_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations tp_selftest_fops = {
	.open = tp_selftest_open,
	.read = tp_selftest_read,
	.write = tp_selftest_write,
	.release = tp_selftest_release,
};

static int32_t datadump_show(struct seq_file *m, void *v)
{
	int ret = 0, i = 0, j = 0;
	int *rawdata = NULL;
	int *differ_data = NULL;
	struct fts_test *tdata = fts_ftest;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev;

	input_dev = ts_data->input_dev;
	FTS_TEST_FUNC_ENTER();
	if (ts_data->suspended) {
		FTS_INFO("In suspend, no test, return now");
		ret = -EINVAL;
		goto out;
	}
	rawdata = vmalloc(PAGE_SIZE * 2);
	if (!rawdata) {
		ret = -ENOMEM;
		goto out;
	}
	memset(rawdata, 0, PAGE_SIZE * 2);
	differ_data = vmalloc(PAGE_SIZE * 2);
	if (!differ_data) {
		ret = -ENOMEM;
		goto out;
	}
	memset(differ_data, 0, PAGE_SIZE * 2);
	mutex_lock(&input_dev->mutex);
	fts_irq_disable();

	/* before enter factory mode, disable auto calibration */
	ret = fts_test_write_reg(0xEE, 0x01);
	if (ret) {
		FTS_TEST_ERROR("write data auto cal fail\n");
		ret = -EFAULT;
		goto out;
	}
	ret = fts_test_init_basicinfo(tdata);
	if (ret < 0) {
		FTS_TEST_ERROR("test init basicinfo fail");
		ret = -EFAULT;
		goto out;
	}

	/*********************GET RAWDATA*********************/
	if (tdata->func->data_dump) {
		ret = tdata->func->data_dump(rawdata, differ_data);
		if (ret) {
			FTS_TEST_ERROR("get rawdata error");
			ret = -EFAULT;
			goto out;
		}
	}
	FTS_TEST_INFO("tx num:%d,rx num:%d\n", tdata->node.tx_num, tdata->node.rx_num);
	seq_puts(m, "\nRAW DATA\n");
	for (i = 0; (i < tdata->node.tx_num) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < tdata->node.rx_num) && (j < RX_NUM_MAX); j++) {
			seq_printf(m, "%6d", rawdata[tdata->node.rx_num * i + j]);
			if (j == (tdata->node.rx_num - 1))
				seq_puts(m, "\n");
		}
	}
	seq_puts(m, "\nDIFF DATA\n");
	for (i = 0; (i < tdata->node.tx_num) && (i < TX_NUM_MAX); i++) {
		for (j = 0; (j < tdata->node.rx_num) && (j < RX_NUM_MAX); j++) {
			seq_printf(m, "%6d", differ_data[tdata->node.rx_num * i + j]);
			if (j == (tdata->node.rx_num - 1))
				seq_puts(m, "\n");
		}
	}
	seq_puts(m, "\n\n");
	ret = 0;
out:
	enter_work_mode();
	fts_irq_enable();
	mutex_unlock(&input_dev->mutex);
	if (rawdata) {
		vfree(rawdata);
		rawdata = NULL;
	}
	if (differ_data) {
		vfree(differ_data);
		differ_data = NULL;
	}
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static void *datadump_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *datadump_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void datadump_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations tp_datadump_seq_ops = {
	.start = datadump_start,
	.next = datadump_next,
	.stop = datadump_stop,
	.show = datadump_show,
};

static int32_t tp_datadump_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &tp_datadump_seq_ops);
}

static const struct file_operations tp_datadump_fops = {
	.owner = THIS_MODULE,
	.open = tp_datadump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int fts_test_func_init(struct fts_ts_data *ts_data)
{
	int i = 0;
	int j = 0;
	u16 ic_stype = ts_data->ic_info.ids.type;
	struct test_funcs *func = test_func_list[0];
	int func_count = ARRAY_SIZE(test_func_list);

	FTS_TEST_INFO("init test function");
	if (func_count == 0) {
		FTS_TEST_SAVE_ERR("test functions list is NULL, fail\n");
		return -ENODATA;
	}

	fts_ftest = kzalloc(sizeof(*fts_ftest), GFP_KERNEL);
	if (fts_ftest == NULL) {
		FTS_TEST_ERROR("malloc memory for test fail");
		return -ENOMEM;
	}

	for (i = 0; i < func_count; i++) {
		func = test_func_list[i];
		for (j = 0; j < FTS_MAX_COMPATIBLE_TYPE; j++) {
			if (func->ctype[j] == 0)
				break;
			else if (func->ctype[j] == ic_stype) {
				FTS_TEST_INFO("match test function,type:%x", (int)func->ctype[j]);
				fts_ftest->func = func;
			}
		}
	}
	if (fts_ftest->func == NULL) {
		FTS_TEST_ERROR("no test function match, can't test");
		return -ENODATA;
	}

	fts_ftest->ts_data = ts_data;
	return 0;
}

int fts_test_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct ftxxxx_proc *proc = &ts_data->proc;

	FTS_TEST_FUNC_ENTER();
	/* get test function, must be the first step */
	ret = fts_test_func_init(ts_data);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("test functions init fail");
		return ret;
	}

	ret = sysfs_create_group(&ts_data->dev->kobj, &fts_test_attribute_group);
	if (ret != 0) {
		FTS_TEST_ERROR("sysfs(test) create fail");
		sysfs_remove_group(&ts_data->dev->kobj, &fts_test_attribute_group);
	} else
		FTS_TEST_DBG("sysfs(test) create successfully");

	proc->tp_selftest_proc = proc_create("tp_selftest", 0644, NULL, &tp_selftest_fops);
	if (proc->tp_selftest_proc == NULL)
		FTS_TEST_ERROR("tp_selftest proc create failed.");

	proc->tp_data_dump_proc = proc_create("tp_data_dump", 0444, NULL, &tp_datadump_fops);
	if (proc->tp_data_dump_proc == NULL)
		FTS_TEST_ERROR("tp_data_dump proc create failed.");
	FTS_TEST_FUNC_EXIT();

	return ret;
}

int fts_test_exit(struct fts_ts_data *ts_data)
{
	struct ftxxxx_proc *proc = &ts_data->proc;

	FTS_TEST_FUNC_ENTER();

	if (proc->tp_selftest_proc)
		proc_remove(proc->tp_selftest_proc);
	if (proc->tp_data_dump_proc)
		proc_remove(proc->tp_data_dump_proc);
	proc->tp_selftest_proc = NULL;
	proc->tp_data_dump_proc = NULL;

	sysfs_remove_group(&ts_data->dev->kobj, &fts_test_attribute_group);
	fts_free(fts_ftest);
	FTS_TEST_FUNC_EXIT();
	return 0;
}

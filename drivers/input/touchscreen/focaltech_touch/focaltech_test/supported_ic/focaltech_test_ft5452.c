/************************************************************************
* Copyright (C) 2012-2018, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2019 XiaoMi, Inc.
*
* File Name: Focaltech_test_ft5452.c
*
* Author: Focaltech Driver Team
*
* Created: 2018-03-08
*
* Abstract:
*
************************************************************************/

/*****************************************************************************
* included header files
*****************************************************************************/
#include "../focaltech_test.h"

/*****************************************************************************
* private constant and macro definitions using #define
*****************************************************************************/
#define DEVIDE_MODE_ADDR			0x00
#define REG_LINE_NUM				0x01
#define REG_TX_NUM				  0x02
#define REG_RX_NUM				  0x03
#define REG_WATER_CHANNEL_SELECT	0x09
#define REG_FRE_LIST				0x0A
#define REG_PATTERN_5452			0x53
#define REG_MAPPING_SWITCH		  0x54
#define REG_TX_NOMAPPING_NUM		0x55
#define REG_RX_NOMAPPING_NUM		0x56
#define REG_NORMALIZE_TYPE		  0x16
#define REG_SC_CB_BUF0			  0x4E
#define REG_SC_WORK_MODE			0x44
#define REG_SC_CB_ADDRR			 0x45
#define REG_RAW_BUF0				0x36
#define REG_FIR_ENABLE			  0xFB
#define MAPPING					 0x00
#define NO_MAPPING				  0x01
#define SHORT_TEST_RES_LEVEL		0x5A
#define SHORT_TEST_ERR_CH0		  0x58
static int ft5452_get_rawdata(struct i2c_client *client, int is_diff, int *rawdata, int len);
extern int fts_test_get_testparams(char *config_name);

/*****************************************************************************
* private enumerations, structures and unions using typedef
*****************************************************************************/
enum water_proof_type {
	wt_need_proof_on,
	wt_need_proof_off,
	wt_need_tx_on,
	wt_need_rx_on,
	wt_need_tx_off,
	wt_need_rx_off,
};

struct ft5452_test_item {
	bool rawdata_test;
	bool scap_cb_test;
	bool scap_rawdata_test;
	bool panel_differ_test;
	bool weak_short_test;
	bool uniformity_test;
};

struct ft5452_basic_threshold {
	int scap_cb_test_off_min;
	int scap_cb_test_off_max;
	int scap_cb_test_on_min;
	int scap_cb_test_on_max;
	bool scap_cb_test_let_tx_disable;
	u8 scap_cb_test_set_waterproof_off;
	u8 scap_cb_test_set_waterproof_on;
	int scap_rawdata_test_off_min;
	int scap_rawdata_test_off_max;
	int scap_rawdata_test_on_min;
	int scap_rawdata_test_on_max;
	bool scap_rawdata_test_let_tx_disable;
	u8 scap_rawdata_test_set_waterproof_off;
	u8 scap_rawdata_test_set_waterproof_on;
	int weak_short_test_cg;
	int weak_short_test_cc;
	bool uniformity_check_tx;
	bool uniformity_check_rx;
	bool uniformity_check_min_max;
	int  uniformity_tx_hole;
	int  uniformity_rx_hole;
	int  uniformity_min_max_hole;
	int panel_differ_test_min;
	int panel_differ_test_max;
};

enum test_item_ft5452 {
	CODE_FT5452_ENTER_FACTORY_MODE = 0, /* all IC are required to test items */
	CODE_FT5452_RAWDATA_TEST = 7,
	CODE_FT5452_SCAP_CB_TEST = 9,
	CODE_FT5452_SCAP_RAWDATA_TEST = 10,
	CODE_FT5452_WEAK_SHORT_CIRCUIT_TEST = 15,
	CODE_FT5452_UNIFORMITY_TEST = 16,
	CODE_FT5452_PANELDIFFER_TEST = 20,
};

/*****************************************************************************
* static variables
*****************************************************************************/

/*****************************************************************************
* global variable or extern global variabls/functions
*****************************************************************************/
struct ft5452_test_item ft5452_item;
struct ft5452_basic_threshold ft5452_basic_thr;

/*****************************************************************************
* static function prototypes
*****************************************************************************/

static int start_scan(void)
{
	int ret = 0;
	int times = 0;
	u8 val = 0;
	const u8 max_times = 250;  /* The longest wait 160ms */
	ret = read_reg(DEVIDE_MODE_ADDR, &val);

	if (ret) {
		FTS_TEST_SAVE_ERR("read device mode fail\n");
		return ret;
	}

	/* top bit position 1, start scan */
	val |= 0x80;
	ret = write_reg(DEVIDE_MODE_ADDR, val);

	if (ret) {
		FTS_TEST_SAVE_ERR("write device mode fail\n");
		return ret;
	}

	while (times++ < max_times) {
		/* wait for the scan to complete */
		sys_delay(16);
		ret = read_reg(DEVIDE_MODE_ADDR, &val);

		if ((0 == ret) && (0 == (val >> 7))) {
			break;
		} else {
			FTS_TEST_DBG("reg%x=%x,retry:%d\n", DEVIDE_MODE_ADDR, val, times);
		}
	}

	if (times >= max_times) {
		FTS_TEST_SAVE_ERR("start scan timeout\n");
		return -EIO;
	}

	return 0;
}

static int read_rawdata(u8 freq, u8 line_num, int byte_num, int *rev_buffer)
{
	int ret = 0;
	int i;
	int read_num;
	u16 bytes_per_time = 0;
	u8 *i2c_w_buffer;
	u8 *read_data = NULL;
	read_num = byte_num / BYTES_PER_TIME;

	if (0 != (byte_num % BYTES_PER_TIME))
		read_num++;

	if (byte_num <= BYTES_PER_TIME) {
		bytes_per_time = byte_num;
	} else {
		bytes_per_time = BYTES_PER_TIME;
	}

	ret = write_reg(REG_LINE_NUM, line_num);/* set row addr */

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to write REG_LINE_NUM!");
	}

	read_data = fts_malloc_dma(byte_num * sizeof(u8));

	if (NULL == read_data) {
		FTS_TEST_SAVE_ERR("read_data buffer malloc fail");
		return -ENOMEM;
	}

	i2c_w_buffer = fts_malloc_dma(3);

	if (!i2c_w_buffer) {
		FTS_TEST_SAVE_ERR("read_data buffer malloc fail");
		return -ENOMEM;
	}

	/* read raw data */
	i2c_w_buffer[0] = REG_RAW_BUF0;   /* set begin address */

	if (!ret) {
		ret = fts_i2c_read_write(i2c_w_buffer, 1, read_data, bytes_per_time);
	}

	for (i = 1; i < read_num; i++) {
		if (ret)
			break;

		if (i == read_num - 1) { /* last packet &*/
			ret = fts_i2c_read_write(NULL, 0, read_data + BYTES_PER_TIME * i, byte_num - BYTES_PER_TIME * i);
		} else {
			ret = fts_i2c_read_write(NULL, 0, read_data + BYTES_PER_TIME * i, BYTES_PER_TIME);
		}
	}

	if (!ret) {
		for (i = 0; i < (byte_num >> 1); i++) {
			rev_buffer[i] = (read_data[i << 1] << 8) + read_data[(i << 1) + 1];
		}
	}

	fts_free(read_data);
	fts_free(i2c_w_buffer);
	return ret;
}

static int get_tx_sc_cb(u8 index, u8 *pcb_value)
{
	int ret = 0;
	/*int i = 0;*/
	u8 *w_buffer;
	w_buffer = fts_malloc_dma(4);

	if (!w_buffer) {
		FTS_TEST_SAVE_ERR("malloc memory error\n");
		return -ENOMEM;
	}

	if (index < 128) { /* single read */
		*pcb_value = 0;
		write_reg(REG_SC_CB_ADDRR, index);
		ret = read_reg(REG_SC_CB_BUF0, pcb_value);
	} else { /* sequential read length index-128 */
		write_reg(REG_SC_CB_ADDRR, 0);
		*w_buffer = REG_SC_CB_BUF0;
		ret = fts_i2c_read_write(w_buffer, 1, pcb_value, index - 128);
		/*
		for (i = 0; i < ((index - 128) >> 1); i++) {
			pcb_value[i] = (pcb_value[i << 1] << 8) + pcb_value[(i << 1) + 1];
		}
		*/
	}

	fts_free(w_buffer);
	return ret;
}

static void save_testdata_mcap(int *data, char *test_num, int row_array_index, int col_array_index, u8 row, u8 col, u8 item_count)
{
	int len = 0;
	int i = 0, j = 0;
	/* save  Msg (itemcode is enough, itemname is not necessary, so set it to "NA".) */
	len = snprintf(test_data.tmp_buffer, PAGE_SIZE, "%s, %d, %d, %d, %d, %d, ", \
			  test_num, test_data.test_item_code, row, col, test_data.start_line, item_count);
	memcpy(test_data.msg_area_line2 + test_data.len_msg_area_line2, test_data.tmp_buffer, len);
	test_data.len_msg_area_line2 += len;
	test_data.start_line += row;
	test_data.test_data_count++;

	/* save data */
	for (i = 0 + row_array_index; (i < row + row_array_index)  && (i < TX_NUM_MAX); i++) {
		for (j = 0 + col_array_index; (j < col + col_array_index) && (j < RX_NUM_MAX); j++) {
			if (j == (col - 1))
				/* the last data of the row, add "\n" */
				len = snprintf(test_data.tmp_buffer, PAGE_SIZE, "%d, \n",  data[i * (col + row_array_index) + j]);
			else
				len = snprintf(test_data.tmp_buffer, PAGE_SIZE, "%d, ", data[i * (col + row_array_index) + j]);

			memcpy(test_data.store_data_area + test_data.len_store_data_area, test_data.tmp_buffer, len);
			test_data.len_store_data_area += len;
		}
	}
}

static int get_rawdata(int *data)
{
	int ret = 0;
	int tx_num = 0;
	int rx_num = 0;
	int va_num = 0;
	/* enter factory mode */
	ret = enter_factory_mode();

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode...");
		return ret;
	}

	/* get tx/rx num */
	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	va_num = tx_num * rx_num;
	/* start scanning */
	ret = start_scan();

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to scan ...");
		return ret;
	}

	/* read rawdata, only MCAP */
	ret = read_rawdata(1, 0xAA, va_num * 2, data);
	return ret;
}

static void show_data_mcap(int *data, bool scap, int offset)
{
	int row = 0;
	int col = 0;
	int tx_num = test_data.screen_param.tx_num;
	int rx_num = test_data.screen_param.rx_num;

	if (!scap) {
		for (row = 0; row < tx_num; row++) {
			FTS_TEST_SAVE_INFO("\nTx%02d:  ", row + 1);

			for (col = 0; col < rx_num; col++) {
				FTS_TEST_SAVE_INFO("%5d, ", data[row * rx_num + col]);
			}
		}
	} else {
		FTS_TEST_SAVE_INFO("\nSCap Rx: ");

		for (col = 0; col < rx_num; col++) {
			FTS_TEST_SAVE_INFO("%5d	", data[(offset * rx_num) + col]);
		}

		FTS_TEST_SAVE_INFO("\nSCap Tx: ");

		for (col = 0; col < tx_num; col++) {
			FTS_TEST_SAVE_INFO("%5d	", data[((offset + 1) * rx_num) + col]);
		}
	}
}

static int get_channel_num_no_mapping(void)
{
	int ret;
	u8 buffer[1]; /* = new u8; */
	FTS_TEST_SAVE_INFO("get tx num...\n");
	ret = read_reg(REG_TX_NOMAPPING_NUM,  buffer);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to get Tx number");
	} else {
		test_data.screen_param.tx_num = buffer[0];
	}

	FTS_TEST_SAVE_INFO("get rx num...\n");
	ret = read_reg(REG_RX_NOMAPPING_NUM,  buffer);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to get rx number\n");
	} else {
		test_data.screen_param.rx_num = buffer[0];
	}

	return ret;
}

static int mapping_switch(u8 str_switch)
{
	int ret = 0;
	u8 ch_pattern = 0xFF;
	u8 reg_data = 0xFF;
	ret = read_reg(REG_PATTERN_5452, &ch_pattern);

	if (ret) {
		FTS_TEST_SAVE_ERR("switch to no_mapping failed!");
		goto READ_ERR;
	}

	if (1 == ch_pattern) { /* 1: V3 Pattern */
		reg_data = 0xFF;
		ret = read_reg(REG_MAPPING_SWITCH, &reg_data);

		if (ret) {
			FTS_TEST_SAVE_ERR("read REG_MAPPING_SWITCH failed!");
			goto READ_ERR;
		}

		if (str_switch != reg_data) {
			ret = write_reg(REG_MAPPING_SWITCH, str_switch);  /* 0-mapping 1-no mampping */

			if (ret) {
				FTS_TEST_SAVE_ERR("write REG_MAPPING_SWITCH failed!");
				goto READ_ERR;
			}

			sys_delay(20);

			if (str_switch) {
				ret = get_channel_num_no_mapping();

				if (ret) {
					FTS_TEST_SAVE_ERR("get_channel_num_no_mapping failed!");
					goto READ_ERR;
				}
			} else {
				/* only self content will be used before the Mapping, so the end of the test items, need to go after Mapping */
				ret = get_channel_num();

				if (ret) {
					FTS_TEST_SAVE_ERR("\n\n get_channel_num error,ret= %d", ret);
					goto READ_ERR;
				}
			}
		}
	}

READ_ERR:
	return ret;
}

static bool get_test_condition(int test_type, u8 channel_value)
{
	bool is_needed = false;

	switch (test_type) {
	case wt_need_proof_on: /* Bit5:  0:test waterproof mode ;  1 not test waterproof mode */
		is_needed = !(channel_value & 0x20);
		break;

	case wt_need_proof_off: /* Bit7: 0: test normal mode  1:not test normal mode */
		is_needed = !(channel_value & 0x80);
		break;

	case wt_need_tx_on:
		/* Bit6:  0 : test waterproof rx+tx  1:test waterproof single channel */
		/* Bit2:  0: test waterproof tx only;  1:  test waterproof rx only */
		is_needed = !(channel_value & 0x40) || !(channel_value & 0x04);
		break;

	case wt_need_rx_on:
		/* Bit6:  0 : test waterproof rx+tx  1 test waterproof single channel */
		/* Bit2:  0: test waterproof tx only;  1:  test waterproof rx only */
		is_needed = !(channel_value & 0x40) || (channel_value & 0x04);
		break;

	case wt_need_tx_off:/* Bit1,Bit0:  00: test normal tx; 10: test normal rx+tx */
		is_needed = (0x00 == (channel_value & 0x03)) || (0x02 == (channel_value & 0x03));
		break;

	case wt_need_rx_off:/* Bit1,Bit0:  01: test normal rx;	10: test normal rx+tx */
		is_needed = (0x01 == (channel_value & 0x03)) || (0x02 == (channel_value & 0x03));
		break;

	default:
		break;
	}

	return is_needed;
}

static bool compare_scap_data(int *scap_data, u8 line_num, int test_type_0, int test_type_1, int (*data_min)[RX_NUM_MAX], int (*data_max)[RX_NUM_MAX])
{
	int i;
	int min;
	int max;
	int value = 0;
	int scap_data_min;
	int scap_data_max;
	int rx_num;
	int tx_num;
	int count = 0;
	u8 wc_value = 0;
	bool flag = false;
	bool tmp_result = true;
	rx_num = test_data.screen_param.rx_num;
	tx_num = test_data.screen_param.tx_num;
	max = -scap_data[rx_num * line_num];
	min = 2 * scap_data[rx_num * line_num];
	flag = get_test_condition(test_type_0, wc_value);

	for (i = 0; flag && i < rx_num; i++) {
		if (test_data.mcap_detail_thr.invalid_node_sc[0][i] == 0)
			continue;

		scap_data_min = data_min[0][i];
		scap_data_max = data_max[0][i];
		value = scap_data[rx_num * line_num + i];

		if (max < value)
			max = value; /* find the Max value */

		if (min > value)
			min = value; /* fine the min value */

		if (value > scap_data_max || value < scap_data_min) {
			tmp_result = false;
			FTS_TEST_SAVE_ERR("Failed. Num = %d, value = %d, range = (%d, %d) \n",  i + 1, value, scap_data_min, scap_data_max);
		}

		count++;
	}

	flag = get_test_condition(test_type_1, wc_value);

	for (i = 0; flag && i < tx_num; i++) {
		if (test_data.mcap_detail_thr.invalid_node_sc[1][i] == 0)
			continue;

		scap_data_min = data_min[1][i];
		scap_data_max = data_max[1][i];
		value = scap_data[rx_num * (line_num + 1) + i];

		if (max < value)
			max = value; /* find the Max value */

		if (min > value)
			min = value;  /* fine the min value */

		if (value > scap_data_max || value < scap_data_min) {
			tmp_result = false;
			FTS_TEST_SAVE_ERR("failed. num = %d, value = %d, range = (%d, %d) \n",  i + 1, value, scap_data_min, scap_data_max);
		}

		count++;
	}

	if (0 == count) {
		max = 0;
		min = 0;
	}

	return tmp_result;
}

static int weak_short_get_adc_data(int all_adc_data_len, int *rev_buffer, u8 cmd)
{
	int ret = 0;
	int i = 0;
	int read_data_len = all_adc_data_len; /* offset*2 + (clb_data + tx_num + rx_num)*2*2 */
	u8 *data_send = NULL;
	u8 data = 0xFF;
	bool adc_ok = false;
	FTS_TEST_FUNC_ENTER();
	data_send = fts_malloc_dma(read_data_len + 1);

	if (NULL == data_send) {
		FTS_TEST_SAVE_ERR("data_send buffer malloc fail");
		return -ENOMEM;
	}

	memset(data_send, 0, read_data_len + 1);
	ret = write_reg(0x07, cmd);/* test weak short once, after host send an enable command */

	if (ret) {
		FTS_TEST_SAVE_ERR("write_reg error. ");
		goto ADC_END;
	}

	sys_delay(100);

	for (i = 0; i < 100 * 5; i++) {
		sys_delay(10);
		ret = read_reg(0x07, &data); /* data ready,and FW set reg 0x07 to 0. */

		if (!ret) {
			if (data == 0) {
				adc_ok = true;
				break;
			}
		}
	}

	if (!adc_ok) {
		FTS_TEST_SAVE_ERR("ADC data NOT ready, error.");
		goto ADC_END;
	}

	sys_delay(300);
	*data_send = 0xF4;
	ret = fts_i2c_read_write(data_send, 1, data_send + 1, read_data_len);

	if (!ret) {
		for (i = 0; i < read_data_len / 2; i++) {
			rev_buffer[i] = (data_send[1 + 2 * i] << 8) + data_send[1 + 2 * i + 1];
		}
	} else {
		FTS_TEST_SAVE_ERR("fts_i2c_read_write error,ret=%d", ret);
	}

ADC_END:
	fts_free(data_send);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

/*  */
static bool short_test_get_channel_num(int *fm_short_resistance, int *adc_data, int offset, bool *is_weak_short_mut)
{
	int ret = 0;
	int i;
	int count = 0;
	int tx_num;
	int rx_num;
	int total_num;
	int max_tx;
	int all_adc_data_num;
	int res_stalls = 111;
	int code_1 = 1437;
	int min_cc = ft5452_basic_thr.weak_short_test_cc;
	bool tmp_result = true;
	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	total_num = tx_num + rx_num;
	max_tx = tx_num;
	all_adc_data_num = 1 + tx_num + rx_num;

	for (i = 0; i < 1; i++) {
		ret = weak_short_get_adc_data(all_adc_data_num * 2, adc_data, 0x01);
		sys_delay(50);

		if (ret) {
			tmp_result = false;
			FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d", ret);
			goto TEST_END;
		}
	}

	/* print all Adc value */
	for (i = 0; i < all_adc_data_num/*channel_num*/; i++) {
		if (i <= (total_num + 1)) {
			if (i == 0)
				FTS_TEST_SAVE_INFO("\n\n\nOffset %02d: %4d,	\n", i, adc_data[i]);
			else if (i == 1) /* if(i <= max_tx) */
				FTS_TEST_SAVE_INFO("Ground %02d: %4d, \n", i, adc_data[i]);
			else if (i <= (max_tx + 1))
				FTS_TEST_SAVE_INFO("Tx%02d: %4d,	", i - 1, adc_data[i]);
			else  if (i <= (total_num + 1))
				FTS_TEST_SAVE_INFO("Rx%02d: %4d,	", i - max_tx - 1, adc_data[i]);

			if (i % 10 == 0)
				FTS_TEST_SAVE_INFO("\n");
		} else {
			if (i == (total_num + 2))
				FTS_TEST_SAVE_INFO("\n\n\nMultual %02d: %4d,	\n", i, adc_data[i]);
			else if (i <= (max_tx) + (total_num + 2))
				FTS_TEST_SAVE_INFO("Tx%02d: %4d,	", i - (total_num + 2), adc_data[i]);
			else  if (i < all_adc_data_num)
				FTS_TEST_SAVE_INFO("Rx%02d: %4d,	", i - max_tx - (total_num + 2), adc_data[i]);

			if (i % 10 == 0)
				FTS_TEST_SAVE_INFO("\n");
		}
	}

	FTS_TEST_SAVE_INFO("\r\n");
	count = 0;

	for (i = 0; i < total_num; i++) {
		if (code_1 - adc_data[i] <= 0) {
			fm_short_resistance[i] = min_cc;
			continue;
		}

		fm_short_resistance[i] = (adc_data[i] - offset + 395) * res_stalls / (code_1 - adc_data[i]) - 3;

		if (fm_short_resistance[i] < 0)
			fm_short_resistance[i] = abs(fm_short_resistance[i]);

		if (min_cc > fm_short_resistance[i]) {
			count++;
		}
	}

	if (count > 0) {
		*is_weak_short_mut = true;
	}

TEST_END:
	return tmp_result;
}

static bool short_test_channel_to_gnd(int *fm_short_resistance, int *adc_data, int offset, bool *is_weak_short_gnd)
{
	int ret = 0;
	int error_num = 0;
	int min_70k_num = 0;
	int total_num;
	int tx_num;
	int min_cg = 0;
	int min_cc = 0;
	int i;
	int num;
	int code_1 = 1437;
	int code_0 = 1437;
	int res_stalls = 111;
	int fvalue = 0;
	int all_adc_data_num = 0;
	int res_stalls0 = 4;
	int *fg_short_resistance = NULL;
	int *tmp_adc_data = NULL;
	u8 *w_buf = NULL;
	u8 *error_ch = NULL;
	u8 *min_70k_ch = NULL ;
	bool is_used = false;
	bool *tmp_result = false;
	tx_num = test_data.screen_param.tx_num;
	total_num = tx_num + test_data.screen_param.rx_num;
	all_adc_data_num = total_num + 1;
	min_cc = ft5452_basic_thr.weak_short_test_cc;
	min_cg = ft5452_basic_thr.weak_short_test_cg;
	error_ch = fts_malloc(total_num * sizeof(u8));

	if (NULL == error_ch) {
		FTS_TEST_SAVE_ERR("error_ch buffer malloc fail");
		return -ENOMEM;
	}

	min_70k_ch = fts_malloc(total_num * sizeof(u8));

	if (NULL == min_70k_ch) {
		FTS_TEST_SAVE_ERR("min_70k_ch buffer malloc fail");
		return -ENOMEM;
	}

	w_buf = fts_malloc_dma((total_num + 3) * sizeof(u8));

	if (NULL == w_buf) {
		FTS_TEST_SAVE_ERR("w_buf buffer malloc fail");
		return -ENOMEM;
	}

	tmp_adc_data = fts_malloc_dma(total_num * total_num * sizeof(int));

	if (NULL == tmp_adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail");
		return -ENOMEM;
	}

	memset(tmp_adc_data, 0, (total_num * total_num));
	fg_short_resistance =  fts_malloc_dma(total_num * sizeof(int));

	if (NULL == fg_short_resistance) {
		FTS_TEST_SAVE_ERR("fg_short_resistance buffer malloc fail");
		return -ENOMEM;
	}

	memset(fg_short_resistance, 0, total_num);
	error_num = 0;
	min_70k_num = 0;

	for (i = 0; i < total_num; i++) {
		if (fm_short_resistance[i] < min_cc) {
			error_ch[error_num] = (u8)(i + 1);
			error_num++;
		}
	}

	if (error_num > 0) {
		*w_buf = SHORT_TEST_ERR_CH0;
		*(w_buf + 1) = (u8)error_num;

		for (i = 0; i < error_num; i++) {
			w_buf[2 + i] = error_ch[i];
		}

		ret = fts_i2c_read_write(w_buf, (u16)(error_num + 2), NULL, 0);

		for (i = 0; i < 1; i++) {
			ret = weak_short_get_adc_data(error_num * 2, tmp_adc_data, 0x03);
			sys_delay(50);

			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		FTS_TEST_SAVE_INFO("\r\n Ground ADCData:\r\n");

		for (i = 0; i < error_num; i++) {
			FTS_TEST_SAVE_INFO(" 0x%x,	", tmp_adc_data[i]);
		}

		for (i = 0; i < error_num; i++) {
			if (code_1 - tmp_adc_data[i] <= 0) {
				fg_short_resistance[i] = min_cg;
				continue;
			}

			fvalue = (tmp_adc_data[i] - offset + 395) * res_stalls / (code_1 - tmp_adc_data[i]) - 3;

			if (fvalue < 0)
				fvalue = abs(fg_short_resistance[i]);

			if (min_cg > fvalue) {
				fg_short_resistance[error_ch[i] - 1] = fvalue;
				adc_data[error_ch[i] - 1] = tmp_adc_data[i];
				*is_weak_short_gnd = true;

				if (fvalue > 70) {
					if (error_ch[i] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d with GND", error_ch[i]);
					} else {
						FTS_TEST_SAVE_INFO("Rx%d with GND", (error_ch[i] - tx_num));
					}

					FTS_TEST_SAVE_INFO(": %.02d, ADC: %d\n", fvalue, tmp_adc_data[i]);
					tmp_result = false;
				}

				if (fvalue < 70) {
					is_used = false;

					for (num = 0; num < min_70k_num; num++) {
						if (error_ch[i] == min_70k_ch[num]) {
							is_used = true;
							break;
						}
					}

					if (!is_used) {
						min_70k_ch[min_70k_num] = error_ch[i];
						min_70k_num++;
					}
				}
			}
		}
	}

	if (min_70k_num > 0) {
		ret = write_reg(SHORT_TEST_RES_LEVEL, 0x00);

		if (ret) {
			goto TEST_END;
		}

		memset(tmp_adc_data, 0, (all_adc_data_num + 1));
		w_buf[0] = SHORT_TEST_ERR_CH0;
		w_buf[1] = (u8)min_70k_num;

		for (i = 0; i < min_70k_num; i++) {
			w_buf[2 + i] = min_70k_ch[i];
		}

		ret = fts_i2c_read_write(w_buf, (u16)(min_70k_num + 2), NULL, 0);

		for (i = 0; i < 1; i++) {
			ret = weak_short_get_adc_data(min_70k_num * 2, tmp_adc_data, 0x03);
			sys_delay(50);

			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		for (i = 0; i < min_70k_num; i++) {
			if (code_0 - tmp_adc_data[i] <= 0) {
				fg_short_resistance[i] = min_cg;
				continue;
			}

			fvalue = (tmp_adc_data[i] - offset + 395) * res_stalls0 / (code_0 - tmp_adc_data[i]) - 3;

			if (fvalue < 0)
				fvalue = abs(fg_short_resistance[i]);

			if (min_cg > fvalue) {
				fg_short_resistance[min_70k_ch[i] - 1] = fvalue;
				adc_data[min_70k_ch[i] - 1] = tmp_adc_data[i];

				if (min_70k_ch[i] <= tx_num) {
					FTS_TEST_SAVE_INFO("Tx%d with GND", min_70k_ch[i]);
				} else {
					FTS_TEST_SAVE_INFO("Rx%d with GND", (min_70k_ch[i] - tx_num));
				}

				FTS_TEST_SAVE_INFO(": %.02d, ADC: %d\n", fvalue, tmp_adc_data[i]);
				tmp_result = false;
			}
		}
	}

TEST_END:
	fts_free(error_ch);
	fts_free(min_70k_ch);
	fts_free(tmp_adc_data);
	fts_free(fg_short_resistance);
	return tmp_result;
}

static bool short_test_channel_to_channel(int *fm_short_resistance, int *adc_data, int offset)
{
	int ret = 0;
	int error_num = 0;
	int min_70k_num = 0;
	int total_num;
	int rx_num;
	int tx_num;
	int min_cc = 0;
	int i;
	int j;
	int num;
	int code_1 = 1437;
	int code_0 = 1437;
	int res_stalls = 111;
	int fvalue = 0;
	int adc_count = 0;
	int all_adc_data_num = 0;
	int res_stalls0 = 4;
	int *f_origin_resistance = NULL;
	int *tmp_adc_data = NULL;
	u8 *w_buf = NULL;
	u8 *error_ch = NULL;
	u8 *min_70k_ch = NULL ;
	bool is_used = false;
	bool *tmp_result = false;
	rx_num = test_data.screen_param.rx_num;
	tx_num = test_data.screen_param.tx_num;
	total_num = rx_num + tx_num;
	all_adc_data_num = total_num + 1;
	min_cc = ft5452_basic_thr.weak_short_test_cc;
	f_origin_resistance =  fts_malloc(total_num * sizeof(int));

	if (NULL == f_origin_resistance) {
		FTS_TEST_SAVE_ERR("f_origin_resistance buffer malloc fail");
		return -ENOMEM;
	}

	memset(f_origin_resistance, 0, total_num);
	tmp_adc_data = fts_malloc_dma(total_num * total_num * sizeof(int));

	if (NULL == tmp_adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail");
		return -ENOMEM;
	}

	memset(tmp_adc_data, 0, (total_num * total_num));

	for (i = 0; i < total_num; i++) {
		f_origin_resistance[i] = fm_short_resistance[i];
	}

	ret = write_reg(SHORT_TEST_RES_LEVEL, 0x01);

	if (ret) {
		goto TEST_END;
	}

	error_ch = fts_malloc(total_num * sizeof(u8));

	if (NULL == error_ch) {
		FTS_TEST_SAVE_ERR("error_ch buffer malloc fail");
		return -ENOMEM;
	}

	min_70k_ch =  fts_malloc(total_num * sizeof(u8));

	if (NULL == min_70k_ch) {
		FTS_TEST_SAVE_ERR("min_70k_ch buffer malloc fail");
		return -ENOMEM;
	}

	error_num = 0;
	min_70k_num = 0;

	for (i = 0; i < total_num; i++) {
		if (f_origin_resistance[i] < min_cc) {
			error_ch[error_num] = (u8)(i + 1);
			error_num++;
		}
	}

	if (error_num > 1) {
		w_buf[0] = SHORT_TEST_ERR_CH0;
		w_buf[1] = (u8)error_num;

		for (i = 0; i < error_num; i++) {
			w_buf[2 + i] = error_ch[i];
		}

		memset(tmp_adc_data, 0, (all_adc_data_num + 1));
		ret = fts_i2c_read_write(w_buf, (u16)(error_num + 2), NULL, 0);

		for (j = 0; j < 1; j++) {
			ret = weak_short_get_adc_data(error_num * (error_num - 1) * 2 / 2, tmp_adc_data, 0x02);
			sys_delay(50);

			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		adc_count = 0;

		for (i = 0; i < error_num; i++) {
			for (j = i + 1; j < error_num; j++) {
				adc_count++;

				if (code_1 - tmp_adc_data[adc_count] <= 0) {
					fvalue = min_cc;
					continue;
				}

				fvalue = (tmp_adc_data[adc_count] - offset + 395) * res_stalls / (code_1 - tmp_adc_data[adc_count]) - 3;

				if (fvalue < 0)
					fvalue = abs(fvalue);

				if (min_cc > fvalue) {
					fm_short_resistance[error_ch[i] - 1] = fvalue;
					fm_short_resistance[error_ch[j] - 1] = fvalue;
					adc_data[error_ch[i] - 1] = tmp_adc_data[adc_count];
					adc_data[error_ch[j] - 1] = tmp_adc_data[adc_count];

					if (fvalue > 70) {
						if (error_ch[i] <= tx_num) {
							FTS_TEST_SAVE_INFO("Tx%d", (error_ch[i]));
						} else {
							FTS_TEST_SAVE_INFO("Rx%d", (error_ch[i] - tx_num));
						}

						if (error_ch[j] <= tx_num) {
							FTS_TEST_SAVE_INFO("Tx%d", (error_ch[j]));
						} else {
							FTS_TEST_SAVE_INFO("Rx%d", (error_ch[j] - tx_num));
						}

						FTS_TEST_SAVE_INFO(": %.02d , ADC: %d\n", fvalue, tmp_adc_data[adc_count]);
						tmp_result = false;
					} else {
						is_used = false;

						for (num = 0; num < min_70k_num; num++) {
							if (error_ch[i] == min_70k_ch[num]) {
								is_used = true;
								break;
							}
						}

						if (!is_used) {
							min_70k_ch[min_70k_num] = error_ch[i];
							min_70k_num++;
						}

						is_used = false;

						for (num = 0; num < min_70k_num; num++) {
							if (error_ch[j] == min_70k_ch[num]) {
								is_used = true;
								break;
							}
						}

						if (!is_used) {
							min_70k_ch[min_70k_num] = error_ch[j];
							min_70k_num++;
						}
					}
				}
			}
		}
	}

	if (min_70k_num > 0) {
		ret = write_reg(SHORT_TEST_RES_LEVEL, 0x00);

		if (ret) {
			goto TEST_END;
		}

		w_buf[0] = SHORT_TEST_ERR_CH0;
		w_buf[1] = (u8)min_70k_num + 1;

		for (i = 0; i < min_70k_num; i++) {
			w_buf[2 + i] = min_70k_ch[i];
		}

		ret = fts_i2c_read_write(w_buf, (u16)(min_70k_num + 2), NULL, 0);
		memset(tmp_adc_data, 0, (all_adc_data_num + 1));

		for (i = 0; i < 1; i++) {
			ret = weak_short_get_adc_data(min_70k_num * (min_70k_num - 1) * 2 / 2, tmp_adc_data, 0x02);
			sys_delay(50);

			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		adc_count = 0;

		for (i = 0; i < min_70k_num; i++) {
			for (j = i + 1; j < min_70k_num; j++) {
				adc_count++;

				if (0 >= code_0 - tmp_adc_data[adc_count]) {
					fvalue = min_cc;
					continue;
				}

				fvalue = (tmp_adc_data[adc_count] - offset + 395) * res_stalls0 / (code_0 - tmp_adc_data[adc_count]) - 3;

				if (fvalue < 0)
					fvalue = abs(fvalue);

				if (min_cc > fvalue) {
					fm_short_resistance[min_70k_ch[i] - 1] = fvalue;
					fm_short_resistance[min_70k_ch[j] - 1] = fvalue;
					adc_data[min_70k_ch[i] - 1] = tmp_adc_data[adc_count];
					adc_data[min_70k_ch[j] - 1] = tmp_adc_data[adc_count];

					if (min_70k_ch[i] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d", (min_70k_ch[i]));
					} else {
						FTS_TEST_SAVE_INFO("Rx%d", (min_70k_ch[i] - tx_num));
					}

					if (min_70k_ch[j] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d", (min_70k_ch[j]));
					} else {
						FTS_TEST_SAVE_INFO("Rx%d", (min_70k_ch[j] - tx_num));
					}

					FTS_TEST_SAVE_INFO(": %.02d, ADC: %d\n", fvalue, tmp_adc_data[adc_count]);
					tmp_result = false;
				}
			}
		}
	}

TEST_END:
	fts_free(error_ch);
	fts_free(min_70k_ch);
	return tmp_result;
}

static int ft5452_enter_factory_mode(void)
{
	int ret = 0;
	int i = 0;
	FTS_TEST_FUNC_ENTER();
	sys_delay(150);

	for (i = 1; i <= ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = enter_factory_mode();

		if (ret) {
			FTS_TEST_DBG("enter factory mode,retry=%d", i);
			sys_delay(50);
			continue;
		} else {
			break;
		}
	}

	sys_delay(300);
	return ret;
}

static int ft5452_rawdata_test(bool *test_result)
{
	int ret = 0;
	int index = 0;
	int row, col;
	int value = 0;
	int tx_num = 0;
	int rx_num = 0;
	int *rawdata = NULL;
	int rawdata_min;
	int rawdata_max;
	u8 fre;
	u8 fir;
	u8 origin_value = 0xFF;
	bool tmp_result = true;
	FTS_TEST_SAVE_INFO("\n\n\n==============================Test Item: -------- Raw Data  Test \n");
	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	rawdata = test_data.buffer;
	ret = enter_factory_mode();

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d", ret);
		goto TEST_ERR;
	}

	/******************************************************************
	 * Determine whether for v3 TP first, and then read the value of the 0x54
	 * and check the mapping type is right or not,if not, write the register
	 * rawdata test mapping before mapping 0x54=1;after mapping 0x54=0;
	 ******************************************************************/
	ret = mapping_switch(MAPPING);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to switch_to_mapping,ret=%d", ret);
		goto TEST_ERR;
	}

	/* line by line one after the rawdata value, the default 0X16=0 */
	ret = read_reg(REG_NORMALIZE_TYPE, &origin_value);

	if (ret) {
		FTS_TEST_SAVE_ERR("read  REG_NORMALIZE_TYPE error,ret=%d", ret);
		goto TEST_ERR;
	}

	ret =  read_reg(REG_FRE_LIST, &fre);

	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A error,ret=%d", ret);
		goto TEST_ERR;
	}

	ret =  read_reg(REG_FIR_ENABLE, &fir);

	if (ret) {
		FTS_TEST_SAVE_ERR("read 0xFB error,ret=%d", ret);
		goto TEST_ERR;
	}

	/* Auto normalize */
	if (origin_value != 0x01) {
		ret = write_reg(REG_NORMALIZE_TYPE, 0x01);

		if (ret) {
			FTS_TEST_SAVE_ERR("write REG_NORMALIZE_TYPE error,ret=%d", ret);
			goto TEST_ERR;
		}
	}

	/* set frequecy high */
	ret = write_reg(REG_FRE_LIST, 0x81);

	if (ret) {
		FTS_TEST_SAVE_ERR("set frequecy high error,ret=%d", ret);
		goto TEST_ERR;
	}

	/* fie off  0:close, 1:open */
	ret = write_reg(REG_FIR_ENABLE, 1);

	if (ret) {
		FTS_TEST_SAVE_ERR("set fir state: ON error,ret=%d", ret);
		goto TEST_ERR;
	}

	/*********************GET RAWDATA*********************/
	for (index = 0; index < 3; ++index) {
		/* lost 3 frames, in order to obtain stable data */
		ret = get_rawdata(rawdata);
	}

	if (ret) {
		FTS_TEST_SAVE_ERR("get rawdata failed, ret=%d", ret);
		goto TEST_ERR;
	}

	show_data_mcap(rawdata, false, 0);

	/* to determine rawData if in range or not */
	for (row = 0; row < tx_num; row++) {
		for (col = 0; col < rx_num; col++) {
			if (test_data.mcap_detail_thr.invalid_node[row][col] == 0)
				continue; /* Invalid Node */

			rawdata_min = test_data.mcap_detail_thr.rawdata_test_high_min[row][col];
			rawdata_max = test_data.mcap_detail_thr.rawdata_test_high_max[row][col];
			value = rawdata[row * rx_num + col];;

			if (value < rawdata_min || value > rawdata_max) {
				tmp_result = false;
				FTS_TEST_SAVE_ERR("\ntest failure. node=(%d,  %d), get_value=%d, set_range=(%d, %d)\n", \
						  row + 1, col + 1, value, rawdata_min, rawdata_max);
			}
		}
	}

	/* save test data to testdata.csv */
	save_testdata_mcap(rawdata, "rawdata test", 0, 0, tx_num, rx_num, 2);
	/* set the origin value */
	ret = write_reg(REG_NORMALIZE_TYPE, origin_value);

	if (ret) {
		FTS_TEST_SAVE_ERR("restore REG_NORMALIZE_TYPE error,ret=%d", ret);
		goto TEST_ERR;
	}

	ret = write_reg(REG_FRE_LIST, fre);

	if (ret) {
		FTS_TEST_SAVE_ERR("restore 0x0A error,ret= %d", ret);
		goto TEST_ERR;
	}

	ret = write_reg(REG_FIR_ENABLE, fir);

	if (ret) {
		FTS_TEST_SAVE_ERR("restore 0xFB error,ret= %d", ret);
		goto TEST_ERR;
	}

TEST_ERR:

	/* result */
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n\n//rawData test is OK!\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n\n//rawData test is NG!\n");
	}

	return ret;
}

static int ft5452_uniformity_test(bool *test_result)
{
	int ret = 0;
	int index = 0;
	int row = 0;
	int col = 1;
	int deviation = 0;
	int max = 0;
	int min = 0;
	int uniform = 0;
	int value = 0;
	int *rawdata = NULL;
	int *rx_linearity = NULL;
	int *tx_linearity = NULL;
	int tx_num = 0;
	int rx_num = 0;
	int buf_len = 0;
	u8 fir_value = 1;
	u8 fre = 1;
	bool tmp_result = true;
	FTS_TEST_SAVE_INFO("\r\n\r\n==============================Test Item: -------- RawData Uniformity Test\r\n");
	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	buf_len = ((tx_num + 1) * rx_num) * sizeof(int);
	memset(test_data.buffer, 0, buf_len);
	rawdata = test_data.buffer;
	rx_linearity = fts_malloc(buf_len);

	if (NULL == rx_linearity) {
		FTS_TEST_SAVE_ERR("rx_linearity buffer malloc fail");
		ret = -ENOMEM;
		goto TEST_ERR;
	}

	memset(rx_linearity, 0, buf_len);
	tx_linearity = fts_malloc(buf_len);

	if (NULL == tx_linearity) {
		FTS_TEST_SAVE_ERR("tx_linearity buffer malloc fail");
		ret = -ENOMEM;
		goto TEST_ERR;
	}

	memset(tx_linearity, 0, buf_len);
	ret = enter_factory_mode();

	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Enter factory Mode,ret=%d", ret);
		tmp_result = false;
		goto TEST_ERR;
	}

	/******************************************************************
	 * Determine whether for v3 TP first, and then read the value of the 0x54
	 * and check the mapping type is right or not,if not, write the register
	 * rawdata test mapping before mapping 0x54=1;after mapping 0x54=0;
	 ******************************************************************/
	ret = mapping_switch(MAPPING);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to switch_to_mapping,ret=%d", ret);
		goto TEST_ERR;
	}

	ret = read_reg(REG_FIR_ENABLE, &fir_value);

	if (ret) {
		FTS_TEST_SAVE_ERR("read fir reg failed. error:%d. ", ret);
		tmp_result = false;
		goto TEST_ERR;
	}

	ret = read_reg(REG_FRE_LIST, &fre);

	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A reg failed. error:%d. ", ret);
		tmp_result = false;
		goto TEST_ERR;
	}

	ret = write_reg(REG_FRE_LIST, 0x81);

	if (ret) {
		FTS_TEST_SAVE_ERR("write 0x0A error,ret= %d", ret);
		goto TEST_ERR;
	}

	sys_delay(10);
	ret = write_reg(REG_FIR_ENABLE, 1);

	if (ret) {
		FTS_TEST_SAVE_ERR("write 0xFB error,ret= %d", ret);
		goto TEST_ERR;
	}

	sys_delay(10);

	/* change register value before,need to lose 3 frame data */
	for (index = 0; index < 3; ++index) {
		ret = get_rawdata(rawdata);
	}

	if (ret) {
		FTS_TEST_SAVE_ERR("get_rawdata error,ret= %d", ret);
		goto TEST_ERR;
	}

	if (ft5452_basic_thr.uniformity_check_tx) {
		FTS_TEST_SAVE_INFO("\r\n=========Check Tx Linearity \r\n");

		for (row = 0; row < tx_num; ++row) {
			for (col = 1; col <  rx_num; ++col) {
				deviation = abs(rawdata[(row * rx_num) + col] - rawdata[(row * rx_num) + (col - 1)]);
				max = max(rawdata[(row * rx_num) + col], rawdata[(row * rx_num) + (col - 1)]);
				max = max ? max : 1;
				tx_linearity[(row * rx_num) + col] = 100 * deviation / max;
			}
		}

		/*show data in result.txt*/
		FTS_TEST_SAVE_INFO(" Tx Linearity:\n");

		for (row = 0; row < tx_num; row++) {
			FTS_TEST_SAVE_INFO("\nTx%2d:	", row + 1);

			for (col = 1; col < rx_num; col++) {
				value = tx_linearity[(row * rx_num) + col];
				FTS_TEST_SAVE_INFO("%4d,  ", value);
			}
		}

		FTS_TEST_SAVE_INFO("\n");

		/* to determine  if in range or not */
		for (row = 0; row < tx_num; row++) {
			for (col = 0; col < rx_num; col++) {
				if (test_data.mcap_detail_thr.invalid_node[row][col] == 0)
					continue; /*Invalid Node */

				min = 0 ; /* minHole[row][col]; */
				max = test_data.mcap_detail_thr.tx_linearity_test_max[row][col];
				value = tx_linearity[(row * rx_num) + col];

				if (value < min || value > max) {
					tmp_result = false;
					FTS_TEST_SAVE_ERR("Tx Linearity Out Of Range.  Node=(%d,  %d), Get_value=%d,  Set_Range=(%d, %d) \n", \
							  row + 1, col + 1, value, min, max);
				}
			}
		}

		save_testdata_mcap(tx_linearity, "RawData Uniformity Test", 0, 1, tx_num, rx_num - 1, 1);
	}

	if (ft5452_basic_thr.uniformity_check_rx) {
		FTS_TEST_SAVE_INFO("\r\n=========Check Rx Linearity \r\n");

		for (row = 1; row < tx_num; ++row) {
			for (col = 0; col < rx_num; ++col) {
				deviation = abs(rawdata[(row * rx_num) + col] - rawdata[((row - 1) * rx_num) + col]);
				max = max(rawdata[(row * rx_num) + col], rawdata[((row - 1) * rx_num) + col]);
				max = max ? max : 1;
				rx_linearity[(row * rx_num) + col] = 100 * deviation / max;
			}
		}

		FTS_TEST_SAVE_INFO("  Rx Linearity:\n");

		for (row = 1; row < tx_num; row++) {
			FTS_TEST_SAVE_INFO("\nTx%2d:	", row + 1);

			for (col = 0; col < rx_num; col++) {
				value = rx_linearity[(row * rx_num) + col];
				FTS_TEST_SAVE_INFO("%4d,  ", value);
			}
		}

		FTS_TEST_SAVE_INFO("\n");

		/* to determine  if in range or not */
		for (row = 0; row < tx_num; row++) { /* row = 1 */
			for (col = 0; col < rx_num; col++) {
				if (test_data.mcap_detail_thr.invalid_node[row][col] == 0)
					continue; /* invalid node */

				min = 0 ;  /* minHole[row][col];*/
				max = test_data.mcap_detail_thr.rx_linearity_test_max[row][col];
				value = rx_linearity[(row * rx_num) + col];

				if (value < min || value > max) {
					tmp_result = false;
					FTS_TEST_SAVE_ERR("rx linearity out of range.  node=(%d,  %d), get_value=%d, set_range=(%d, %d) \n", \
							  row + 1, col + 1, value, min, max);
				}
			}
		}

		save_testdata_mcap(rx_linearity, "rawdata uniformity test", 1, 0, tx_num - 1, rx_num, 2);
	}

	if (ft5452_basic_thr.uniformity_check_min_max) {
		FTS_TEST_SAVE_INFO("\r\n=========Check Min/Max \r\n") ;
		min = 100000;
		max = -100000;

		for (row = 0; row < tx_num; ++row) {
			for (col = 0; col < rx_num; ++col) {
				if (0 == test_data.mcap_detail_thr.invalid_node[row][col]) {
					continue;
				}

				if (2 == test_data.mcap_detail_thr.invalid_node[row][col]) {
					continue;
				}

				min = min(min, rawdata[(row * rx_num) + col]);
				max = max(max, rawdata[(row * rx_num) + col]);
			}
		}

		max = !max ? 1 : max;
		uniform = 100 * abs(min) / abs(max);
		FTS_TEST_SAVE_INFO("\r\n min: %d, max: %d, , get value of min/max: %d\n", min, max, uniform);

		if (uniform < ft5452_basic_thr.uniformity_min_max_hole) {
			tmp_result = false;
			FTS_TEST_SAVE_ERR("min_max out of range, set value: %d\n", ft5452_basic_thr.uniformity_min_max_hole);
		}
	}

	ret = write_reg(REG_FIR_ENABLE, fir_value);

	if (ret) {
		FTS_TEST_SAVE_ERR("write fir Reg 0xFB Failed. error:%d. ", ret);
		goto TEST_ERR;
	}

	ret = write_reg(REG_FRE_LIST, fre);

	if (ret) {
		FTS_TEST_SAVE_ERR("write 0x0A Reg Failed. error:%d. ", ret);
		goto TEST_ERR;
	}

TEST_ERR:
	fts_free(rx_linearity);
	fts_free(tx_linearity);

	if (tmp_result && (!ret)) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n\n//uniformity test is OK.");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("\n\n//uniformity test is NG.");
	}

	return ret;
}

static int ft5452_scap_cb_test(bool *test_result)
{
	int ret = 0;
	int i;
	bool flag = true;
	bool tmp_result = true;
	u8 wc_value = 0;
	u8 sc_wrok_mode = 0;
	int tx_num = 0;
	int rx_num = 0;
	int *scap_cb = NULL;
	u8 *temp_cb = NULL;
	FTS_TEST_SAVE_INFO("\n\n\n==============================Test Item: -----  Scap CB Test \n");
	ret = enter_factory_mode();

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto TEST_ERR;
	}

	/* get waterproof channel setting, to check if Tx/Rx channel need to test */
	ret = read_reg(REG_WATER_CHANNEL_SELECT, &wc_value);

	if (ret) {
		FTS_TEST_SAVE_ERR("read REG_WATER_CHANNEL_SELECT error,ret=%d\n",  ret);
		goto TEST_ERR;
	}

	ret = read_reg(REG_SC_WORK_MODE, &sc_wrok_mode);

	if (ret) {
		FTS_TEST_SAVE_ERR("read REG_SC_WORK_MODE error,ret=%d\n", ret);
		goto TEST_ERR;
	}

	/* if it is V3 pattern, get Tx/Rx num again */
	flag = mapping_switch(NO_MAPPING);

	if (flag) {
		FTS_TEST_SAVE_ERR("failed to switch_to_no_mapping,ret=%d", ret);
		goto TEST_ERR;
	}

	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	scap_cb = test_data.buffer;
	temp_cb = fts_malloc_dma(((tx_num + 1) * rx_num) * sizeof(u8));

	if (NULL == temp_cb) {
		FTS_TEST_SAVE_ERR("temp_cb buffer malloc fail\n");
		ret = -ENOMEM;
		goto TEST_ERR;
	}

	for (i = 0; i < 3; i++) {
		/* waterproof CB */
		ret = write_reg(REG_SC_WORK_MODE, 1);/* ScWorkMode:  1:waterproof 0:Non-waterproof */

		if (ret) {
			FTS_TEST_SAVE_ERR("get REG_SC_WORK_MODE failed!");
			goto TEST_ERR;
		}

		ret = write_reg(REG_SC_CB_ADDRR, 0);

		if (ret) {
			FTS_TEST_SAVE_ERR("write REG_SC_CB_ADDRR failed!");
			goto TEST_ERR;
		}

		ret = get_tx_sc_cb((tx_num + rx_num) * 2 + 128, temp_cb);

		if (ret) {
			FTS_TEST_SAVE_ERR("get_tx_sc_cb failed!");
			goto TEST_ERR;
		}
		/*
		for (i = 0; i < rx_num; i++) {
			scap_cb[i] = temp_cb[i];
		}

		for (i = 0; i < tx_num; i++) {
			scap_cb[rx_num + i] = temp_cb[rx_num + i];
		}
		*/
		for (i = 0; i < rx_num; i++) {
			scap_cb[i] = (int)(u16)((temp_cb[i << 1] << 8) + temp_cb[(i << 1) + 1]);
		}
		for (i = 0; i < tx_num; i++) {
			scap_cb[rx_num + i] = (int)(u16)((temp_cb[(rx_num + i) << 1] << 8) + temp_cb[((rx_num + i) << 1) + 1]);
		}
		/* non-waterproof rawdata */
		ret = write_reg(REG_SC_WORK_MODE, 0);/* ScWorkMode:  1:waterproof 0:Non-waterproof */

		if (ret) {
			FTS_TEST_SAVE_ERR("get REG_SC_WORK_MODE failed!");
			goto TEST_ERR;
		}

		ret = write_reg(REG_SC_CB_ADDRR, 0);

		if (ret) {
			FTS_TEST_SAVE_ERR("write REG_SC_CB_ADDRR failed!");
			goto TEST_ERR;
		}

		ret = get_tx_sc_cb((tx_num + rx_num) * 2 + 128, temp_cb);

		if (ret) {
			FTS_TEST_SAVE_ERR("get_tx_sc_cb failed!");
			goto TEST_ERR;
		}

		for (i = 0; i < rx_num; ++i) {
			scap_cb[(2 * rx_num) + i] = (int)(u16)((temp_cb[i << 1] << 8) + temp_cb[(i << 1) + 1]);
		}

		for (i = 0; i < tx_num; ++i) {
			scap_cb[(3 * rx_num) + i] = (int)(u16)((temp_cb[(rx_num + i) << 1] << 8) + temp_cb[((rx_num + i) << 1) + 1]);
		}

		if (ret) {
			FTS_TEST_SAVE_ERR("failed to get scap cb!");
		}
	}

	if (ret)
		goto TEST_ERR;

	/* judge */
	/* waterproof ON */
	flag = get_test_condition(wt_need_proof_on, wc_value);

	if (ft5452_basic_thr.scap_cb_test_set_waterproof_on && flag) {
		/*  show Scap CB in WaterProof On Mode */
		FTS_TEST_SAVE_INFO("scap_cb_test in waterproof on mode:  \n");
		show_data_mcap(scap_cb, true, 0);
		tmp_result = compare_scap_data(scap_cb, 0, wt_need_rx_on, wt_need_tx_on, \
						   test_data.mcap_detail_thr.scap_cb_test_on_min, \
						   test_data.mcap_detail_thr.scap_cb_test_on_max);
		save_testdata_mcap(scap_cb, "scap cb test", 0, 0, 2, rx_num, 1);
	}

	/* waterproof OFF */
	flag = get_test_condition(wt_need_proof_off, wc_value);

	if (ft5452_basic_thr.scap_cb_test_set_waterproof_off && flag) {
		/* show Scap CB in WaterProof Off Mode */
		FTS_TEST_SAVE_INFO("\n\nSCapCbTest in WaterProof Off Mode:  \n");
		show_data_mcap(scap_cb, true, 2);
		tmp_result = compare_scap_data(scap_cb, 2, wt_need_rx_off, wt_need_tx_off, \
						   test_data.mcap_detail_thr.scap_cb_test_off_min, \
						   test_data.mcap_detail_thr.scap_cb_test_off_max);
		save_testdata_mcap(scap_cb, "scap cb test", 2, 0, 2, rx_num, 2);
	}

	/* post-stage work */
	ret = mapping_switch(MAPPING);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to switch_to_mapping,ret=%d", ret);
		goto TEST_ERR;
	}

	ret = write_reg(REG_SC_WORK_MODE, sc_wrok_mode);/* set the origin value */

	if (ret) {
		FTS_TEST_SAVE_ERR("write REG_SC_WORK_MODE error,ret= %d\n",  ret);
		goto TEST_ERR;
	}

TEST_ERR:
	fts_free(temp_cb);

	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n\n//scap cb test is OK!\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("\n\n//scap cb test is NG!\n");
	}

	return ret;
}

static int ft5452_scap_rawdata_test(bool *test_result)
{
	int ret = 0;
	int i = 0;
	int tx_num = 0;
	int rx_num = 0;
	bool flag = true;
	bool tmp_result = true;
	int byte_num = 0;
	u8 wc_value = 0; /* waterproof channel value */
	int *scap_rawdata = NULL;
	FTS_TEST_SAVE_INFO("\n\n\n==============================Test Item: -------- Scap RawData Test \n");
	/* 1.preparatory work */
	ret = enter_factory_mode();

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto TEST_ERR;
	}

	/* get waterproof channel setting, to check if Tx/Rx channel need to test */
	ret = read_reg(REG_WATER_CHANNEL_SELECT, &wc_value);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to read REG_WATER_CHANNEL_SELECT,ret= %d\n", ret);
		goto TEST_ERR;
	}

	/* If it is V3 pattern, get Tx/Rx num again */
	ret = mapping_switch(NO_MAPPING);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to switch_to_no_mapping,ret= %d\n", ret);
		goto TEST_ERR;
	}

	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	scap_rawdata = test_data.buffer;
	/* 2.get scap raw data, step: (1) start scanning; (2) read raw data */
	ret = start_scan();

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to scan scap rawdata!");
		goto TEST_ERR;
	}

	for (i = 0; i < 3; i++) {
		/* water rawdata */
		byte_num = (tx_num + rx_num) * 2;
		ret = read_rawdata(0, 0xAC, byte_num, scap_rawdata);

		if (ret) {
			FTS_TEST_SAVE_ERR("failed to read_rawdata water!");
			goto TEST_ERR;
		}

		/* No water rawdata */
		byte_num = (tx_num + rx_num) * 2;
		ret = read_rawdata(0, 0xAB, byte_num, scap_rawdata + (2 * rx_num));

		if (ret) {
			FTS_TEST_SAVE_ERR("failed to read_rawdata no water!");
			goto TEST_ERR;
		}
	}

	/* 3. judge */
	/* waterproof ON */
	flag = get_test_condition(wt_need_proof_on, wc_value);

	if (ft5452_basic_thr.scap_rawdata_test_set_waterproof_on && flag) {
		/* show Scap rawdata in WaterProof On Mode */
		FTS_TEST_SAVE_INFO("\nscap_rawdata_test in waterproof on mode:  \n");
		show_data_mcap(scap_rawdata, true, 0);
		tmp_result = compare_scap_data(scap_rawdata, 0, wt_need_rx_on, wt_need_tx_on, \
						   test_data.mcap_detail_thr.scap_rawdata_on_min, \
						   test_data.mcap_detail_thr.scap_rawdata_on_max);
		save_testdata_mcap(scap_rawdata, "scap rawdata test", 0, 0, 2, rx_num, 1);
	}

	/* waterproof OFF */
	flag = get_test_condition(wt_need_proof_off, wc_value);

	if (ft5452_basic_thr.scap_rawdata_test_set_waterproof_off && flag) {
		/* show scap rawdata in waterproof off mode */
		FTS_TEST_SAVE_INFO("\n\nscap_rawdata_test in waterproof off mode:  \n");
		show_data_mcap(scap_rawdata, true, 2);
		tmp_result = compare_scap_data(scap_rawdata, 2, wt_need_rx_off, wt_need_tx_off, \
						   test_data.mcap_detail_thr.scap_rawdata_off_min, \
						   test_data.mcap_detail_thr.scap_rawdata_off_max);
		save_testdata_mcap(scap_rawdata, "scap rawdata test", 2, 0, 2, rx_num, 2);
	}

	/* 4. post-stage work */
	ret = mapping_switch(MAPPING);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to switch_to_mapping, ret=%d", ret);
		goto TEST_ERR;
	}

TEST_ERR:

	/* 5. test result */
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n\n//scap rawdata test is OK!");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("\n\n//scap rawdata test is NG!");
	}

	return ret;
}

static int ft5452_panel_differ_test(bool *test_result)
{
	int ret = 0;
	int index = 0;
	int row = 0, col = 0;
	int value = 0;
	bool tmp_result = true;
	int max;
	int min;
	int max_value = 0;
	int min_value = 0;
	int avg_value = 0;
	int invaid_num = 0;
	int i = 0;
	int j = 0;
	int tx_num = 0;
	int rx_num = 0;
	int *scap_panel_differ = NULL;
	int *scap_rawdata = NULL;
	int *abs_differ_data = NULL;
	u8 origin_rawdata_type = 0xFF;
	u8 origin_frequecy = 0xFF;
	u8 origin_fir_state = 0xFF;
	FTS_TEST_SAVE_INFO("\n\n\n==============================Test Item: -------- Panel Differ Test  \n");
	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	scap_panel_differ = test_data.buffer;
	abs_differ_data = fts_malloc(((tx_num + 1) * rx_num) * sizeof(int));

	if (NULL == abs_differ_data) {
		FTS_TEST_SAVE_ERR("abs_differ_data buffer malloc fail\n");
		return -ENOMEM;
	}

	scap_rawdata = fts_malloc_dma(((tx_num + 1) * rx_num) * sizeof(int));

	if (NULL == scap_rawdata) {
		FTS_TEST_SAVE_ERR("scap_rawdata buffer malloc fail\n");
		return -ENOMEM;
	}

	ret = enter_factory_mode();
	sys_delay(20);

	if (ret) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode, ret= %d\n", ret);
		goto TEST_ERR;
	}

	/******************************************************************
	 * Determine whether for v3 TP first, and then read the value of the 0x54
	 * and check the mapping type is right or not,if not, write the register
	 * rawdata test mapping before mapping 0x54=1;after mapping 0x54=0;
	 ******************************************************************/
	ret = mapping_switch(MAPPING);

	if (ret) {
		FTS_TEST_SAVE_ERR("switch to mapping failed!\n");
		goto TEST_ERR;
	}

	FTS_TEST_SAVE_INFO("\n=========set auto equalization:\r\n");
	ret = read_reg(REG_NORMALIZE_TYPE, &origin_rawdata_type);/* read the original value */

	if (ret) {
		FTS_TEST_SAVE_ERR("\nread  REG_NORMALIZE_TYPE error, ret= %d\n",  ret);
		tmp_result = false;
		goto TEST_ERR;
	}

	if (origin_rawdata_type != 0) {
		ret = write_reg(REG_NORMALIZE_TYPE, 0x00);
		sys_delay(50);

		if (ret) {
			tmp_result = false;
			FTS_TEST_SAVE_ERR("write reg REG_NORMALIZE_TYPE failed.");
			goto TEST_ERR;
		}
	}

	/* set frequecy high */
	FTS_TEST_SAVE_INFO("=========set frequecy high\n");
	ret = read_reg(REG_FRE_LIST, &origin_frequecy); /* read the original value */

	if (ret) {
		FTS_TEST_SAVE_ERR("read reg 0x0A error,ret= %d",  ret);
		tmp_result = false;
		goto TEST_ERR;
	}

	ret = write_reg(REG_FRE_LIST, 0x81);
	sys_delay(10);

	if (ret) {
		tmp_result = false;
		FTS_TEST_SAVE_ERR("write reg 0x0A failed.");
		goto TEST_ERR;
	}

	FTS_TEST_SAVE_INFO("=========fir state: OFF\n");
	ret = read_reg(REG_FIR_ENABLE, &origin_fir_state);/* read the original value */

	if (ret) {
		FTS_TEST_SAVE_ERR("read reg 0xFB error,ret= %d",  ret);
		tmp_result = false;
		goto TEST_ERR;
	}

	ret = write_reg(REG_FIR_ENABLE, 0);
	sys_delay(50);

	if (ret) {
		FTS_TEST_SAVE_ERR("write reg 0xFB failed.");
		tmp_result = false;
		goto TEST_ERR;
	}

	/* change register value before,need to lose 3 frame data,4th frame data is valid */
	for (index = 0; index < 4; ++index) {
		ret = get_rawdata(scap_rawdata);

		if (ret) {
			FTS_TEST_SAVE_ERR("get_rawdata failed.");
			tmp_result = false;
			goto TEST_ERR;
		}
	}

	/* differ = rawData * 1/10 */
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			scap_panel_differ[(i * rx_num) + j] = scap_rawdata[(i * rx_num) + j] / 10;
		}
	}

	/* to show value */
	FTS_TEST_SAVE_INFO("pannel differ :\n");
	show_data_mcap(scap_panel_differ, false, 0);

	/* whether threshold is in range */
	/* to determine  if in range or not */
	for (row = 0; row < tx_num; row++) { /* row = 1 */
		for (col = 0; col < rx_num; col++) {
			if (test_data.mcap_detail_thr.invalid_node[row][col] == 0)
				continue; /* invalid node */

			value = scap_panel_differ[row * rx_num + col];
			min =  test_data.mcap_detail_thr.panel_differ_test_min[row][col];
			max = test_data.mcap_detail_thr.panel_differ_test_max[row][col];

			/* FTS_TEST_SAVE_INFO("\n node=(%d,  %d), get_value=%d,  set_range=(%d, %d) \n", row+1, col+1, value, min, max); */
			if (value < min || value > max) {
				tmp_result = false;
				FTS_TEST_SAVE_ERR("\nout of range.  node=(%d,  %d), get_value=%d,  set_range=(%d, %d) \n", \
						  row + 1, col + 1, value, min, max);
			}
		}
	}

	/* end determine */

	/* get test data ,and save to .csv file */
	for (i = 0; i <  tx_num; i++) {
		for (j = 0; j <  rx_num; j++) {
			abs_differ_data[(i * rx_num) + j] = abs(scap_panel_differ[(i * rx_num) + j]);

			if (NODE_AST_TYPE == test_data.mcap_detail_thr.invalid_node[i][j] || NODE_INVALID_TYPE == test_data.mcap_detail_thr.invalid_node[i][j]) {
				invaid_num++;
				continue;
			}

			max_value = max(max_value, scap_panel_differ[(i * rx_num) + j]);
			min_value = min(min_value, scap_panel_differ[(i * rx_num) + j]);
			avg_value += scap_panel_differ[(i * rx_num) + j];
		}
	}

	save_testdata_mcap(abs_differ_data, "panel differ test", 0, 0, tx_num, rx_num, 1);
	avg_value = avg_value / (tx_num * rx_num - invaid_num);
	FTS_TEST_SAVE_INFO("\npanel differ:max: %d, min: %d, avg: %d ", max_value, min_value, avg_value);
	ret = write_reg(REG_NORMALIZE_TYPE, origin_rawdata_type);/* set to original value */
	ret = write_reg(REG_FRE_LIST, origin_frequecy); /* set to original value */
	ret = write_reg(REG_FIR_ENABLE, origin_fir_state); /* set to original value */
	fts_free(abs_differ_data);
	fts_free(scap_rawdata);
TEST_ERR:

	/* result */
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n\n//panel differ test is OK!");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_ERR("\n\n//panel differ test is NG!");
	}

	return ret;
}

static int ft5452_short_test(bool *test_result)
{
	int ret = 0;
	int offset = 0;
	int total_num;
	int max_tx = 35;
	int all_adc_data_num = 63;
	int *adc_data  = NULL;
	int *fm_short_resistance = NULL;
	int code_1 = 1437;
	int code_0 = 1437;
	int offset_value[4] = {0};
	u8 stall_value = 1;
	bool tmp_result = true;
	bool is_weak_short_gnd = false;
	bool is_weak_short_mut = false;
	total_num = test_data.screen_param.tx_num + test_data.screen_param.rx_num;
	max_tx = test_data.screen_param.tx_num;
	all_adc_data_num = 1 + total_num;
	ret = enter_factory_mode();

	if (ret) {
		FTS_TEST_SAVE_ERR("Failed to Enter factory Mode,ret= %d\n", ret);
		tmp_result = false;
		goto TEST_END;
	}

	sys_delay(200);
	adc_data = fts_malloc_dma((all_adc_data_num + 1) * sizeof(int));

	if (NULL == adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail\n");
		return -ENOMEM;
	}

	memset(adc_data, 0, (all_adc_data_num + 1));
	fm_short_resistance = fts_malloc_dma(total_num * sizeof(int));

	if (NULL == fm_short_resistance) {
		FTS_TEST_SAVE_ERR("fm_short_resistance buffer malloc fail\n");
		return -ENOMEM;
	}

	memset(fm_short_resistance, 0, total_num);
	ret = read_reg(SHORT_TEST_RES_LEVEL, &stall_value);
	ret = write_reg(SHORT_TEST_RES_LEVEL, 0x01);

	if (ret) {
		tmp_result = false;
		goto TEST_END;
	}

	ret = weak_short_get_adc_data(1 * 2, offset_value, 0x04);

	if (ret) {
		tmp_result = false;
		FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d", ret);
		goto TEST_END;
	}

	sys_delay(50);
	offset = offset_value[0] - 1024;
	/* get short resistance and exceptional channel */
	tmp_result = short_test_get_channel_num(fm_short_resistance, adc_data, offset, &is_weak_short_mut);

	/* use the exceptional channel to conduct channel to ground short circuit test. */
	if (is_weak_short_mut) {
		tmp_result = short_test_channel_to_gnd(fm_short_resistance, adc_data, offset, &is_weak_short_gnd);
	}

	/* use the exceptional channel to conduct channel to channel short circuit test. */
	if (is_weak_short_mut) {
		tmp_result = short_test_channel_to_channel(fm_short_resistance, adc_data, offset);
	}

TEST_END:
	fts_free(adc_data);
	fts_free(fm_short_resistance);
	ret = write_reg(SHORT_TEST_RES_LEVEL, stall_value);
	FTS_TEST_SAVE_INFO("code0:%d, code1:%d, offset:%d\n", code_0, code_1, offset);

	if (is_weak_short_gnd && is_weak_short_mut) {
		FTS_TEST_SAVE_INFO("gnd and mutual weak short!");
	} else if (is_weak_short_gnd) {
		FTS_TEST_SAVE_INFO("gnd weak short!");
	} else if (is_weak_short_mut) {
		FTS_TEST_SAVE_INFO("mutual weak short!");
	} else {
		FTS_TEST_SAVE_INFO("no short!");
	}

	if (tmp_result) {
		FTS_TEST_SAVE_INFO("\n\n//weak short test is OK!");
		*test_result = true;
	} else {
		FTS_TEST_SAVE_ERR("\n\n//weak short test is NG!");
		*test_result = false;
	}

	return ret;
}

static bool start_test_ft5452(void)
{
	int ret = 0;
	int item_count = 0;
	u8 item_code = 0;
	bool test_result = true;
	bool temp_result = true;
	FTS_TEST_FUNC_ENTER();

	/* test item */
	if (0 == test_data.test_num) {
		FTS_TEST_SAVE_ERR("test item == 0\n");
		return false;
	}

	for (item_count = 0; item_count < test_data.test_num; item_count++) {
		item_code = test_data.test_item[item_count].itemcode;
		test_data.test_item_code = item_code;

		/* FT5452_ENTER_FACTORY_MODE */
		if (CODE_FT5452_ENTER_FACTORY_MODE == item_code) {
			ret = ft5452_enter_factory_mode();

			if (0 != ret || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
				break; /* if this item FAIL, no longer test. */
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* FT5452_RAWDATA_TEST */
		if (CODE_FT5452_RAWDATA_TEST == item_code) {
			ret = ft5452_rawdata_test(&temp_result);

			if (0 != ret || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* CODE_FT5452_UNIFORMITY_TEST */
		if (CODE_FT5452_UNIFORMITY_TEST == item_code) {
			ret = ft5452_uniformity_test(&temp_result);

			if (0 != ret || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* FT5452_SCAP_CB_TEST */
		if (CODE_FT5452_SCAP_CB_TEST == item_code) {
			ret = ft5452_scap_cb_test(&temp_result);

			if (0 != ret || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* FT5452_SCAP_RAWDATA_TEST */
		if (CODE_FT5452_SCAP_RAWDATA_TEST == item_code) {
			ret = ft5452_scap_rawdata_test(&temp_result);

			if (0 != ret || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* CODE_FT5452_PANELDIFFER_TEST */
		if (CODE_FT5452_PANELDIFFER_TEST == item_code) {
			ret = ft5452_panel_differ_test(&temp_result);

			if (0 != ret || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* CODE_FT5452_WEAK_SHORT_CIRCUIT_TEST */
		if (CODE_FT5452_WEAK_SHORT_CIRCUIT_TEST == item_code) {
			ret = ft5452_short_test(&temp_result);

			if (0 != ret || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}
	}

	return test_result;
}

static void init_testitem_ft5452(char *ini)
{
	char str[MAX_KEYWORD_VALUE_LEN] = { 0 };
	FTS_TEST_FUNC_ENTER();
	/* RawData Test */
	GetPrivateProfileString("TestItem", "RAWDATA_TEST", "1", str, ini);
	ft5452_item.rawdata_test = fts_atoi(str);
	/* uniformity_test */
	GetPrivateProfileString("TestItem", "UNIFORMITY_TEST", "0", str, ini);
	ft5452_item.uniformity_test = fts_atoi(str);
	/* scap_cb_test */
	GetPrivateProfileString("TestItem", "SCAP_CB_TEST", "1", str, ini);
	ft5452_item.scap_cb_test = fts_atoi(str);
	/* scap_rawdata_test */
	GetPrivateProfileString("TestItem", "SCAP_RAWDATA_TEST", "1", str, ini);
	ft5452_item.scap_rawdata_test = fts_atoi(str);
	/* panel differ_TEST */
	GetPrivateProfileString("TestItem", "PANEL_DIFFER_TEST", "1", str, ini);
	ft5452_item.panel_differ_test = fts_atoi(str);
	/* weak_short_test */
	GetPrivateProfileString("TestItem", "WEAK_SHORT_CIRCUIT_TEST", "1", str, ini);
	ft5452_item.weak_short_test = fts_atoi(str);
	FTS_TEST_FUNC_EXIT();
}

static int ft5452_open_selftest(void)
{
	int ret = SELFTEST_INVALID;
	int *rawdata = NULL;
	int i = 0;

	FTS_TEST_FUNC_ENTER();
	rawdata = (int *)vmalloc(PAGE_SIZE);
	if (!rawdata) {
		return SELFTEST_INVALID;
	} else
		memset(rawdata, 0, PAGE_SIZE);

	ret = get_channel_num();
	if (ret) {
		FTS_TEST_ERROR("get channel num error");
		ret = SELFTEST_INVALID;
		goto out;
	}
	if (fts_data) {
		ret = ft5452_get_rawdata(fts_data->client, 0, rawdata, test_data.screen_param.tx_num * test_data.screen_param.rx_num * 2);
		if (ret) {
			FTS_TEST_ERROR("get rawdata error");
			ret = SELFTEST_INVALID;
			goto out;
		}
		for (i = 0; i < (test_data.screen_param.tx_num * test_data.screen_param.rx_num); i++) {
			printk("FTS rawdata:%d\n", rawdata[i]);
			if (rawdata[i] < fts_data->pdata->open_min) {
				ret = SELFTEST_FAIL;
				goto out;
			}
		}
		if (i == test_data.screen_param.tx_num * test_data.screen_param.rx_num)
			ret = SELFTEST_PASS;
	}
out:
	if (rawdata) {
		vfree(rawdata);
		rawdata = NULL;
	}
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft5452_short_selftest(void)
{
	int ret = SELFTEST_INVALID;
	bool temp_result;


	FTS_TEST_FUNC_ENTER();
	/*Read parse configuration file*/
	ret = fts_test_get_testparams("Conf_MultipleTest.ini");
	if (ret < 0) {
		FTS_TEST_ERROR("get testparam failed");
		return SELFTEST_FAIL;
	}
	ret = ft5452_short_test(&temp_result);
	if (0 != ret || (!temp_result))
		ret = SELFTEST_FAIL;
	else
		ret = SELFTEST_PASS;
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static void init_basicthreshold_ft5452(char *ini)
{
	char str[MAX_KEYWORD_VALUE_LEN] = { 0 };
	FTS_TEST_FUNC_ENTER();
	/* Uniformity */
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_Tx", "0", str, ini);
	ft5452_basic_thr.uniformity_check_tx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_Rx", "0", str, ini);
	ft5452_basic_thr.uniformity_check_rx = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Check_MinMax", "0", str, ini);
	ft5452_basic_thr.uniformity_check_min_max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Tx_Hole", "20", str, ini);
	ft5452_basic_thr.uniformity_tx_hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_Rx_Hole", "20", str, ini);
	ft5452_basic_thr.uniformity_rx_hole = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "UniformityTest_MinMax_Hole", "70", str, ini);
	ft5452_basic_thr.uniformity_min_max_hole = fts_atoi(str);
	/* scap cb */
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Min", "0", str, ini);
	ft5452_basic_thr.scap_cb_test_off_min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Max", "240", str, ini);
	ft5452_basic_thr.scap_cb_test_off_max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Min", "0", str, ini);
	ft5452_basic_thr.scap_cb_test_on_min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Max", "240", str, ini);
	ft5452_basic_thr.scap_cb_test_on_max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ScapCBTest_SetWaterproof_OFF", "0", str, ini);
	ft5452_basic_thr.scap_cb_test_set_waterproof_off = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "ScapCBTest_SetWaterproof_ON", "240", str, ini);
	ft5452_basic_thr.scap_cb_test_set_waterproof_on = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCBTest_LetTx_Disable", "0", str, ini);
	ft5452_basic_thr.scap_cb_test_let_tx_disable = fts_atoi(str);
	/* scap rawdata */
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Min", "5000", str, ini);
	ft5452_basic_thr.scap_rawdata_test_off_min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Max", "8500", str, ini);
	ft5452_basic_thr.scap_rawdata_test_off_max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Min", "5000", str, ini);
	ft5452_basic_thr.scap_rawdata_test_on_min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Max", "8500", str, ini);
	ft5452_basic_thr.scap_rawdata_test_on_max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_SetWaterproof_OFF", "1", str, ini);
	ft5452_basic_thr.scap_rawdata_test_set_waterproof_off = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_SetWaterproof_ON", "0", str, ini);
	ft5452_basic_thr.scap_rawdata_test_set_waterproof_on = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_LetTx_Disable", "0", str, ini);
	ft5452_basic_thr.scap_rawdata_test_let_tx_disable = fts_atoi(str);
	/* panel differ */
	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Min", "150", str, ini);
	ft5452_basic_thr.panel_differ_test_min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Max", "1000", str, ini);
	ft5452_basic_thr.panel_differ_test_max = fts_atoi(str);
	/* weak short */
	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CG", "2000", str, ini);
	ft5452_basic_thr.weak_short_test_cg = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "WeakShortTest_CC", "2000", str, ini);
	ft5452_basic_thr.weak_short_test_cc = fts_atoi(str);
	FTS_TEST_FUNC_EXIT();
}

static void init_detailthreshold_ft5452(char *ini)
{
	FTS_TEST_FUNC_ENTER();
	OnInit_InvalidNode(ini);
	OnInit_DThreshold_RawDataTest(ini);
	OnInit_DThreshold_SCapRawDataTest(ini);
	OnInit_DThreshold_SCapCbTest(ini);
	OnInit_DThreshold_RxLinearityTest(ini);
	OnInit_DThreshold_TxLinearityTest(ini);
	OnInit_DThreshold_PanelDifferTest(ini);
	FTS_TEST_FUNC_EXIT();
}

static void set_testitem_sequence_ft5452(void)
{
	test_data.test_num = 0;
	FTS_TEST_FUNC_ENTER();
	/* Enter Factory Mode */
	fts_set_testitem(CODE_FT5452_ENTER_FACTORY_MODE);

	/* RawData Test */
	if (ft5452_item.rawdata_test == 1) {
		fts_set_testitem(CODE_FT5452_RAWDATA_TEST);
	}

	/* Rawdata Uniformity Test */
	if (ft5452_item.uniformity_test == 1) {
		fts_set_testitem(CODE_FT5452_UNIFORMITY_TEST);
	}

	/* scap_cb_test */
	if (ft5452_item.scap_cb_test == 1) {
		fts_set_testitem(CODE_FT5452_SCAP_CB_TEST);
	}

	/* scap_rawdata_test */
	if (ft5452_item.scap_rawdata_test == 1) {
		fts_set_testitem(CODE_FT5452_SCAP_RAWDATA_TEST);
	}

	/* panel differ_TEST */
	if (ft5452_item.panel_differ_test == 1) {
		fts_set_testitem(CODE_FT5452_PANELDIFFER_TEST);
	}

	/* weak_short_test */
	if (ft5452_item.weak_short_test == 1) {
		fts_set_testitem(CODE_FT5452_WEAK_SHORT_CIRCUIT_TEST);
	}

	FTS_TEST_FUNC_EXIT();
}
/*
static int ft5452_startscan(struct i2c_client *client)
{
	u8 RegVal = 0x00;
	u8 times = 0;
	const u8 MaxTimes = 20;
	u8 ReCode;
	u8 OldMode;

	ret = read_reg(DEVIDE_MODE_ADDR, &OldMode);

	if (ret) {
		FTS_TEST_SAVE_ERR("read device mode fail\n");
		return ret;
	} else
		FTS_TEST_INFO("read device mode %x\n", OldMode);
	ReCode = fts_i2c_write_reg(client, DEVIDE_MODE_ADDR, 0xC0);
	if (ReCode >= 0)
	{
		while (times++ < MaxTimes)
		{
			msleep(8);
			ReCode = fts_i2c_read_reg(client, DEVIDE_MODE_ADDR, &RegVal);
			if (RegVal == 0x40)
				break;
		}
		if (times > MaxTimes)
			return -EFAULT;
	}
	return ReCode;
}
*/

static int ft5452_get_rawdata(struct i2c_client *client, int is_diff, int *rawdata, int len)
{
	u8 reg = 0x36;
	u8 regdata[1280] = { 0 };
	int remain_bytes;
	int pos = 0;
	int i = 0;

	FTS_DEBUG("len=%d, is_diff=%d", len, is_diff);
	fts_i2c_write_reg(client, FACTORY_REG_DATA_SELECT, is_diff);
	/*
	if (ft5452_startscan(client) < 0)
		return -EFAULT;
		*/
	start_scan();
	fts_i2c_write_reg(client, FACTORY_REG_LINE_ADDR, 0xAA);
	if (len <= 256)
		fts_i2c_read(client, &reg, 1, regdata, len);
	else {
		fts_i2c_read(client, &reg, 1, regdata, 256);
		remain_bytes = len - 256;
		for (i = 1; remain_bytes > 0; i++) {
			if (remain_bytes > 256)
			  fts_i2c_read(client, &reg, 0, regdata + i * 256, 256);
			else
				fts_i2c_read(client, &reg, 0, regdata + i * 256, remain_bytes);
			remain_bytes -= 256;
		}
	}
	for (i = 0; i < len;) {
		rawdata[pos++] = ((int)(regdata[i]) << 8) + regdata[i + 1];
		i += 2;
	}
	/* restore data select reg*/
	fts_i2c_write_reg(client, FACTORY_REG_DATA_SELECT, 0);
	return 0;
}

static int ft5452_data_dump(int *rawdata, int *differ_data)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();
	ret = get_channel_num();
	if (ret) {
		FTS_TEST_ERROR("get channel num error");
		ret = -EFAULT;
		goto out;
	}
	if (fts_data) {
		ret = ft5452_get_rawdata(fts_data->client, 0, rawdata, test_data.screen_param.tx_num * test_data.screen_param.rx_num * 2);
		if (ret) {
			FTS_TEST_ERROR("get rawdata error");
			goto out;
		}
	}

	if (fts_data) {
		ret = ft5452_get_rawdata(fts_data->client, 1, differ_data, test_data.screen_param.tx_num * test_data.screen_param.rx_num * 2);
		if (ret) {
			FTS_TEST_ERROR("get differ data error");
			goto out;
		}
	}

out:
	enter_work_mode();
	FTS_TEST_FUNC_EXIT();
	return ret;
}


struct test_funcs test_func_ft5452 = {
	.ic_series = TEST_ICSERIES(IC_FT5452),
	.init_testitem = init_testitem_ft5452,
	.init_basicthreshold = init_basicthreshold_ft5452,
	.init_detailthreshold = init_detailthreshold_ft5452,
	.set_testitem_sequence  = set_testitem_sequence_ft5452,
	.start_test = start_test_ft5452,
	.open_test = ft5452_open_selftest,
	.short_test = ft5452_short_selftest,
	.data_dump = ft5452_data_dump,
};

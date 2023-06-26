/************************************************************************
* Copyright (c) 2012-2020, Focaltech Systems (R), All Rights Reserved.
*
* File Name: Focaltech_test_ft8722.c
*
* Author: Focaltech Driver Team
*
* Created: 2019-03-02
*
* Abstract:
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "../focaltech_test.h"
#include <linux/uaccess.h>
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FACTORY_REG_OPEN_START2				 0xC0
#define FACTORY_REG_OPEN_DV					 0xC1
#define FACTORY_REG_OPEN_STATE2				 0xC2
#define FACTORY_REG_OPEN_ADDR2				  0x89
#ifdef FTS_GET_TP_DIFFER
#define ERROR_CODE_OK                           0x00
#define ERROR_CODE_INVALID_COMMAND              0x02
#define ERROR_CODE_INVALID_PARAM                0x03
#define ERROR_CODE_WAIT_RESPONSE_TIMEOUT        0x07
#define ERROR_CODE_COMM_ERROR                   0x0c
#define ERROR_CODE_ALLOCATE_BUFFER_ERROR        0x0d

#define FTS_PROC_TP_DIFFER "fts_tp_differ"
#define FTS_PROC_TP_rawdata "fts_tp_rawdata"

static struct proc_dir_entry *tp_differ_proc;
static struct proc_dir_entry *tp_rawdata_proc;
#endif

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int ft8720_short_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int i = 0;
	bool tmp_result = false;
	int byte_num = 0;
	int ch_num = 0;
	int min = 0;
	int max = 0;
	int tmp_adc = 0;
	int *adcdata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Short Circuit Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	adcdata = tdata->buffer;

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	byte_num = tdata->node.node_num * 2;
	ch_num = tdata->node.rx_num;
	ret = short_get_adcdata_incell(TEST_RETVAL_AA, ch_num, byte_num, adcdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get adc data fail\n");
		goto test_err;
	}

	/* calculate resistor */
	for (i = 0; i < tdata->node.node_num; i++) {
		tmp_adc = adcdata[i];
		if ((tmp_adc >= 4003) && (tmp_adc < 4096)) {
			tmp_adc = 4003;
		}
		adcdata[i] = (67 * tmp_adc + 7735) / (4095 - tmp_adc);
	}

	/* save */
	show_data(adcdata, false);

	/* compare */
	min = thr->basic.short_res_min;
	max = TEST_SHORT_RES_MAX;
	tmp_result = compare_data(adcdata, min, max, min, max, false);

	ret = 0;
test_err:
	ret = fts_test_write_reg(FACTORY_REG_SHORT_TEST_STATE, 0x03);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write idle to short test state fail\n");
	}

	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ Short Circuit Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ Short Circuit Test NG\n");
	}

	/* save test data */
	fts_test_save_data("Short Circuit Test", CODE_SHORT_TEST,
					   adcdata, 0, false, false, *test_result);

	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8720_open_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int i = 0;
	bool tmp_result = false;
	u8 state = 0;
	int byte_num = 0;
	int min = 0;
	int max = 0;
	int *opendata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Open Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	opendata = tdata->buffer;

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	FTS_INFO("open DV=%d, diff_min=%d", thr->basic.open_delta_V, thr->basic.open_diff_min);
	ret = fts_test_write_reg(FACTORY_REG_OPEN_DV, thr->basic.open_delta_V);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write reg open_DV fail\n");
		goto test_err;
	}

	ret = fts_test_write_reg(FACTORY_REG_OPEN_START2, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("start open test fail\n");
		goto test_err;
	}

	/* check test status */
	for (i = 0; i < FACTORY_TEST_RETRY; i++) {
		sys_delay(FACTORY_TEST_RETRY_DELAY);
		ret = fts_test_read_reg(FACTORY_REG_OPEN_STATE2, &state);
		if ((ret >= 0) && (TEST_RETVAL_AA == state)) {
			break;
		} else {
			FTS_TEST_DBG("reg%x=%x,retry:%d\n",
						 FACTORY_REG_OPEN_STATE2, state, i);
		}
	}
	if (i >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("open test timeout\n");
		goto test_err;
	}

	/* get open data */
	byte_num = tdata->node.node_num * 2;
	ret = read_mass_data(FACTORY_REG_OPEN_ADDR2, byte_num, opendata);
	if (ret) {
		FTS_TEST_SAVE_ERR("get open data fail\n");
	}

	/* save */
	show_data(opendata, false);

	/* compare */
	min = thr->basic.open_diff_min;
	max = 1000000;
	tmp_result = compare_data(opendata, min, max, 0, 0, false);

test_err:
	ret = fts_test_write_reg(FACTORY_REG_OPEN_STATE2, 0x03);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write idle to open test state fail\n");
	}

	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ Open Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ Open Test NG\n");
	}

	/*save test data*/
	fts_test_save_data("Open Test", CODE_OPEN_TEST,
					   opendata, 0, false, false, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8720_cb_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int byte_num = 0;
	int *cbdata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: CB Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	cbdata = tdata->buffer;

	if (!thr->cb_min || !thr->cb_max) {
		FTS_TEST_SAVE_ERR("cb_min/max is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* cb test enable */
	ret = fts_test_write_reg(FACTORY_REG_CB_TEST_EN, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("cb test enable fail\n");
		goto test_err;
	}

	/* auto clb */
	ret = chip_clb();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("auto clb fail\n");
		goto test_err;
	}

	byte_num = tdata->node.node_num;
	ret = get_cb_incell(0, byte_num, cbdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get cb fail\n");
		goto test_err;
	}

	/* save */
	show_data(cbdata, false);

	/* compare */
	tmp_result = compare_array(cbdata, thr->cb_min, thr->cb_max, false);

test_err:
	/* cb test disable */
	ret = fts_test_write_reg(FACTORY_REG_CB_TEST_EN, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("cb test disable fail\n");
	}

	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ CB Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ CB Test NG\n");
	}

	/*save test data*/
	fts_test_save_data("CB Test", CODE_CB_TEST,
					   cbdata, 0, false, false, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8720_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int *rawdata = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: RawData Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	rawdata = tdata->buffer;

	if (!thr->rawdata_min || !thr->rawdata_max) {
		FTS_TEST_SAVE_ERR("rawdata_min/max is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* rawdata test enable */
	ret = fts_test_write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("rawdata test enable fail\n");
		goto test_err;
	}

	/* read rawdata */
	ret = get_rawdata(rawdata);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get RawData fail,ret=%d\n", ret);
		goto test_err;
	}

	/* save */
	show_data(rawdata, false);

	/* compare */
	tmp_result = compare_array(rawdata,
							   thr->rawdata_min,
							   thr->rawdata_max,
							   false);
	ret = 0;

test_err:
	/* rawdata test disble */
	ret = fts_test_write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("rawdata test disable fail\n");
	}

	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ RawData Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ RawData Test NG\n");
	}

	/*save test data*/
	fts_test_save_data("RawData Test", CODE_RAWDATA_TEST,
					   rawdata, 0, false, false, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int ft8720_lcdnoise_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int i = 0;
	bool tmp_result = false;
	u8 old_mode = 0;
	u8 status = 0;
	int byte_num = 0;
	int frame_num = 0;
	int max = 0;
	int *lcdnoise = NULL;
	struct incell_threshold *thr = &tdata->ic.incell.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: LCD Noise Test\n");
	memset(tdata->buffer, 0, tdata->buffer_length);
	lcdnoise = tdata->buffer;

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_DATA_SELECT, &old_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read reg06 fail\n");
		goto test_err;
	}

	ret =  fts_test_write_reg(FACTORY_REG_DATA_SELECT, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 1 to reg06 fail\n");
		goto restore_reg;
	}

	ret =  fts_test_write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write reg01 fail\n");
		goto restore_reg;
	}

	frame_num = thr->basic.lcdnoise_frame;
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_FRAME, frame_num / 4);
	if (ret < 0) {
		FTS_TEST_SAVE_INFO("write frame num fail\n");
		goto restore_reg;
	}

	/* start test */
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_START, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_INFO("start lcdnoise test fail\n");
		goto restore_reg;
	}

	/* check test status */
	sys_delay(frame_num * FACTORY_TEST_DELAY / 2);
	for (i = 0; i < FACTORY_TEST_RETRY; i++) {
		status = 0xFF;
		ret = fts_test_read_reg(FACTORY_REG_LCD_NOISE_TEST_STATE, &status);
		if ((ret >= 0) && (TEST_RETVAL_AA == status)) {
			break;
		} else {
			FTS_TEST_DBG("reg%x=%x,retry:%d\n",
						 FACTORY_REG_LCD_NOISE_TEST_STATE, status, i);
		}
		sys_delay(FACTORY_TEST_RETRY_DELAY);
	}
	if (i >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("lcdnoise test timeout\n");
		goto restore_reg;
	}
	/* read lcdnoise */
	byte_num = tdata->node.node_num * 2;
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, byte_num, lcdnoise);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		goto restore_reg;
	}

	/* save */
	show_data(lcdnoise, false);

	/* compare */
	max = thr->basic.lcdnoise_coefficient * tdata->va_touch_thr * 32 / 100;
	FTS_TEST_DBG("touch thr:%d, max:%d", tdata->va_touch_thr, max);
	tmp_result = compare_data(lcdnoise, 0, max, 0, 0, false);

restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, old_mode);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore reg06 fail\n");
	}

test_err:
	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_START, 0x00);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0 to reg11 fail\n");
	}

	ret = fts_test_write_reg(FACTORY_REG_LCD_NOISE_TEST_STATE, 0x03);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write idle to lcdnoise test state fail\n");
	}

	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("------ LCD Noise Test PASS\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("------ LCD Noise Test NG\n");
	}

	/*save test data*/
	fts_test_save_data("LCD Noise Test", CODE_LCD_NOISE_TEST,
					   lcdnoise, 0, false, false, *test_result);
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static int start_test_ft8720(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	struct incell_testitem *test_item = &tdata->ic.incell.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_INFO("test item:0x%x", fts_ftest->ic.incell.u.tmp);

	if (!tdata || !tdata->testresult || !tdata->buffer) {
		FTS_TEST_ERROR("tdata is null");
		return -EINVAL;
	}
	ret = enter_factory_mode();
	if (ret < 0)
		fts_ftest->test_item[FTS_ENTER_FACTORY_MODE].testresult = RESULT_NG ;
	else {
		FTS_TEST_SAVE_ERR("enter factory pass\n");
		fts_ftest->test_item[FTS_ENTER_FACTORY_MODE].testresult = RESULT_PASS;
	}

	/* short test */
	if (true == test_item->short_test) {
		ret = ft8720_short_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_ftest->test_item[FTS_SHORT_CIRCUIT_TEST].testresult = RESULT_NG ;
	    } else
		    fts_ftest->test_item[FTS_SHORT_CIRCUIT_TEST].testresult = RESULT_PASS ;
	}

	/* open test */
	if (true == test_item->open_test) {
		ret = ft8720_open_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_ftest->test_item[FTS_OPEN_TEST].testresult = RESULT_NG ;
		} else
			fts_ftest->test_item[FTS_OPEN_TEST].testresult = RESULT_PASS ;
	}

	/* cb test */
	if (true == test_item->cb_test) {
		ret = ft8720_cb_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_ftest->test_item[FTS_CB_TEST].testresult = RESULT_NG;
		} else
			fts_ftest->test_item[FTS_CB_TEST].testresult = RESULT_PASS;
	}

	/* rawdata test */
	if (true == test_item->rawdata_test) {
		ret = ft8720_rawdata_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_ftest->test_item[FTS_RAWDATA_TEST].testresult = RESULT_NG;
		} else
			fts_ftest->test_item[FTS_RAWDATA_TEST].testresult = RESULT_PASS;
	}

	/* lcd noise test */
	if (true == test_item->lcdnoise_test) {
		ret = ft8720_lcdnoise_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_ftest->test_item[FTS_NOISE_TEST].testresult = RESULT_NG;
		} else
			fts_ftest->test_item[FTS_NOISE_TEST].testresult = RESULT_PASS;
	}

	return test_result;
}

static int param_init_ft8720(void)
{
	struct incell_threshold *thr = &fts_ftest->ic.incell.thr;

	get_value_basic("OpenTest_DeltaVoltage", &thr->basic.open_delta_V);
	get_value_basic("OpenTest_DifferMin", &thr->basic.open_diff_min);

	return 0;
}

#if	FTS_TP_SELFTEST
bool start_selftest(int tmp)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
//	struct incell_testitem *test_item = &tdata->ic.incell.u.item;
	bool temp_result = false;
	bool test_result = true;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_INFO("test item:0x%x", fts_ftest->ic.incell.u.tmp);

	if (!tdata || !tdata->testresult || !tdata->buffer) {
		FTS_TEST_ERROR("tdata is null");
		return -EINVAL;
	}
	/* enter factory */
	ret = enter_factory_mode();
	if (ret < 0)
		fts_ftest->test_item[FTS_ENTER_FACTORY_MODE].testresult = RESULT_NG ;
	else {
		FTS_TEST_SAVE_ERR("enter factory pass\n");
		fts_ftest->test_item[FTS_ENTER_FACTORY_MODE].testresult = RESULT_PASS;
	}

	/* short test */
	if (tmp == 2) {
		ret = ft8720_short_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_ftest->test_item[FTS_SHORT_CIRCUIT_TEST].testresult = RESULT_NG ;
		} else
			fts_ftest->test_item[FTS_SHORT_CIRCUIT_TEST].testresult = RESULT_PASS ;
	}

	/* open test */
	if (tmp == 1) {
		ret = ft8720_open_test(tdata, &temp_result);
		if ((ret < 0) || (false == temp_result)) {
			test_result = false;
			fts_ftest->test_item[FTS_OPEN_TEST].testresult = RESULT_NG ;
		} else
			fts_ftest->test_item[FTS_OPEN_TEST].testresult = RESULT_PASS ;
	}
	return test_result;
}
#endif

struct test_funcs test_func_ft8720 = {
	.ctype = {0x1C},
	.hwtype = IC_HW_INCELL,
	.key_num_total = 0,
	.param_init = param_init_ft8720,
	.start_test = start_test_ft8720,
};

#ifdef FTS_GET_TP_DIFFER
#define ENTER_WORK_FACTORY_RETRIES              5
#define FACTORY_TEST_DELAY                      18
#define FACTORY_TEST_RETRY_DELAY                100

#define FACTORY_REG_CHX_NUM                     0x02
#define FACTORY_REG_CHY_NUM                     0x03
#define DEVICE_MODE_ADDR                        0x00
#define FACTORY_REG_LINE_ADDR                   0x01
#define FACTORY_REG_RAWDATA_ADDR                0x6A
#define FACTORY_REG_DATA_SELECT                 0x06
#define packet                                  128

int fts_raw_enter_factory_mode(void)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;
	int j = 0;
	u8 addr = DEVICE_MODE_ADDR;

	ret = fts_read(&addr, 1, &mode, 1);
	if ((ret >= 0) && (0x40 == mode))
		return 0;

	for (i = 0; i < 5; i++) {
		ret = fts_write_reg(addr, 0x40);
		if (ret >= 0) {
			msleep(FACTORY_TEST_DELAY);
			for (j = 0; j < 20; j++) {
				ret = fts_read(&addr, 1, &mode, 1);
				if ((ret >= 0) && (0x40 == mode)) {
					FTS_INFO("enter factory mode success");
					msleep(200);
					return 0;
				} else
					msleep(FACTORY_TEST_DELAY);
			}
		}
		msleep(50);
	}

	if (i >= 5) {
		FTS_INFO("Enter factory mode fail");
		return -EIO;
	}
	return 0;
}


static int fts_raw_get_channel_num(u8 tx_rx_reg, u8 *ch_num, u8 ch_num_max)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < 3; i++) {
		ret = fts_read(&tx_rx_reg, 1, ch_num, 1);
		if ((ret < 0) || (*ch_num > ch_num_max)) {
			msleep(50);
		} else
			break;
	}

	if (i >= 3) {
		FTS_INFO("get channel num fail");
		return -EIO;
	}

	return 0;
}


int fts_raw_start_scan(void)
{
	int ret = 0;
	u8 addr = 0;
	u8 val = 0;
	u8 finish_val = 0;
	int times = 0;
	addr = DEVICE_MODE_ADDR;
	val = 0xC0;
	finish_val = 0x40;

	fts_raw_enter_factory_mode();
	ret = fts_write_reg(addr, val);
	if (ret < 0) {
		FTS_INFO("write start scan mode fail\n");
		return ret;
	}

	while (times++ < 50) {
		msleep(18);

		ret = fts_read(&addr, 1, &val, 1);
		if ((ret >= 0) && (val == finish_val)) {
			break;
		} else
			FTS_INFO("reg%x=%x,retry:%d", addr, val, times);
	}

	if (times >= 50) {
		FTS_INFO("scan timeout\n");
		return -EIO;
	}
	return 0;
}


int fts_raw_read_rawdata(u8 addr, u8 *readbuf, int readlen)
{
	int ret = 0;
	int i = 0;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = readlen;

	packet_num = byte_num / packet;
	packet_remainder = byte_num % packet;
	if (packet_remainder)
		packet_num++;

	if (byte_num < packet) {
		packet_length = byte_num;
	} else {
		packet_length = packet;
	}
	/* FTS_TEST_DBG("packet num:%d, remainder:%d", packet_num, packet_remainder); */
	ret = fts_read(&addr, 1, &readbuf[offset], packet_length);
	if (ret < 0) {
		FTS_INFO("read buffer fail");
		return ret;
	}
	for (i = 1; i < packet_num; i++) {
		offset += packet_length;
		if ((i == (packet_num - 1)) && packet_remainder) {
			packet_length = packet_remainder;
		}

		ret = fts_read(&addr, 1, &readbuf[offset], packet_length);
		if (ret < 0) {
			FTS_INFO("read buffer fail");
			return ret;
		}
	}

	return 0;
}

static int fts_raw_read(u8 off_addr, u8 off_val, u8 rawdata_addr, int byte_num, int *buf)
{
	int ret = 0;
	int i = 0;
	u8 *data = NULL;

	/* set line addr or rawdata start addr */
	ret = fts_write_reg(off_addr, off_val);
	if (ret < 0) {
		FTS_INFO("wirte line/start addr fail\n");
		return ret;
	}

	data = (u8 *)kzalloc(byte_num * sizeof(u8), GFP_KERNEL);
	if (NULL == data) {
		FTS_INFO("mass data buffer malloc fail\n");
		return -ENOMEM;
	}

	/* read rawdata buffer */
	FTS_INFO("mass data len:%d", byte_num);
	ret = fts_raw_read_rawdata(rawdata_addr, data, byte_num);
	if (ret < 0) {
		FTS_INFO("read mass data fail\n");
		goto read_massdata_err;
	}

	for (i = 0; i < byte_num; i = i + 2) {
		buf[i >> 1] = (int)(short)((data[i] << 8) + data[i + 1]);
	}

	ret = 0;
read_massdata_err:
	kfree(data);
	return ret;
}

int fts_rawdata_differ(u8 raw_diff, char *buf)
{
	u8 val = 0xAD;
	u8 addr = FACTORY_REG_LINE_ADDR;
	u8 rawdata_addr = FACTORY_REG_RAWDATA_ADDR;
	int count = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	int ret = 0;
	u8 cmd[2] = {0};

	u8 tx_num = 0;
	u8 rx_num = 0;
	int byte_num = 0;
	int buffer_length = 0;
	char *tmp_buf = NULL;
	int *buffer = NULL;
	struct input_dev *input_dev = fts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	fts_irq_disable();

	cmd[0] = 0xEE;
	cmd[1] = 1;
	fts_write(cmd, 2);
	fts_raw_enter_factory_mode();
	ret = fts_test_write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0x01);
	if (ret < 0) {
		FTS_TEST_SAVE_ERR("rawdata test enable fail\n");
		goto read_err;
	}

	fts_raw_get_channel_num(FACTORY_REG_CHX_NUM, &tx_num, 60);
	fts_raw_get_channel_num(FACTORY_REG_CHY_NUM, &rx_num, 60);
	FTS_ERROR("tx_num = %d, rx_num = %d", tx_num, rx_num);
	cmd[0] = FACTORY_REG_DATA_SELECT;
	cmd[1] = raw_diff;
	fts_write(cmd, 2);

	fts_raw_start_scan();
	byte_num = tx_num * rx_num * 2;
	buffer_length = (tx_num + 1) * rx_num;
	buffer_length *= sizeof(int) * 2;
	FTS_INFO("test buffer length:%d", buffer_length);
	buffer = (int *)kzalloc(buffer_length, GFP_KERNEL);
	if (NULL == buffer) {
		FTS_INFO("test buffer(%d) malloc fail", buffer_length);
		goto read_err;
	}
	memset(buffer, 0, buffer_length);

	tmp_buf = kzalloc(buffer_length, GFP_KERNEL);
	if (NULL == buffer) {
		FTS_INFO("test buffer(%d) malloc fail", buffer_length);
		goto read_err;
	}
	memset(tmp_buf, 0, buffer_length);
	ret = fts_raw_read(addr, val, rawdata_addr, byte_num, buffer);
	if (ret < 0) {
		FTS_INFO("read rawdata fail\n");
		goto read_err;
	}
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			count += snprintf(tmp_buf + count, PAGE_SIZE, "%4d ", buffer[k++]);
		}
		count += snprintf(tmp_buf + count, PAGE_SIZE, "\n");
	}
	if (copy_to_user(buf, tmp_buf, 4096))
		FTS_INFO("read rawdata fail\n");
read_err:
	ret = fts_test_write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0x00);
	if (ret < 0)
		FTS_TEST_SAVE_ERR("rawdata test disable fail\n");
	fts_irq_enable();
	mutex_unlock(&input_dev->mutex);
	cmd[0] = DEVICE_MODE_ADDR;
	cmd[1] = 0x00;
	ret = fts_write(cmd, 2);
	cmd[0] = 0xEE;
	cmd[1] = 0x00;
	ret = fts_write(cmd, 2);
	kfree(buffer);
	kfree(tmp_buf);
	return count;

}

ssize_t tp_differ_proc_read (struct file *file, char __user *buf, size_t count, loff_t *off)
{
	int cnt = 0;
	FTS_TEST_FUNC_ENTER();
	if (*off != 0)
		return 0;
	cnt = fts_rawdata_differ(1, buf);
	FTS_INFO("cnt = %d", cnt);
	*off += cnt;
	return cnt;

}

ssize_t tp_rawdata_proc_read (struct file *file, char __user *buf, size_t count, loff_t *off)
{
	int cnt = 0;
	FTS_TEST_FUNC_ENTER();
	if (*off != 0)
		return 0;
	cnt = fts_rawdata_differ(0, buf);
	*off += cnt;
	return cnt;

}

static const struct file_operations tp_differ_proc_fops = {
	.owner = THIS_MODULE,
	.read = tp_differ_proc_read,
};

static const struct file_operations tp_rawdata_proc_fops = {
	.owner = THIS_MODULE,
	.read = tp_rawdata_proc_read,
};

int fts_tp_differ_proc(void)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();
	tp_differ_proc = proc_create(FTS_PROC_TP_DIFFER, 0444, NULL, &tp_differ_proc_fops);
	if (tp_differ_proc == NULL) {
		FTS_TEST_ERROR("[focal] %s() - ERROR: create fts_tp_differ proc()  failed.",  __func__);
		ret = -1;
	}
	tp_rawdata_proc = proc_create(FTS_PROC_TP_rawdata, 0444, NULL, &tp_rawdata_proc_fops);
	if (tp_rawdata_proc == NULL) {
		FTS_TEST_ERROR("[focal] %s() - ERROR: create fts_tp_differ proc()  failed.",  __func__);
		ret = -1;
	}
	return ret;
}
#endif

/************************************************************************
* Copyright (C) 2012-2018, Focaltech Systems (R), All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Focaltech_test_ft8719.c
*
* Author: Focaltech Driver Team
*
* Created: 2017-12-01
*
* Abstract: test item for FT8719
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "../focaltech_test.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FACTORY_REG_VGH02        0x20
#define FACTORY_REG_VGL02        0x21

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct ft8719_test_item {
	bool rawdata_test;
	bool cb_test;
	bool short_test;
	bool lcd_noise_test;
	bool open_test;
};

struct ft8719_basic_threshold {
	int rawdata_test_min;
	int rawdata_test_max;
	bool cb_test_va_check;
	int cb_test_min;
	int cb_test_max;
	bool cb_test_vk_check;
	int cb_test_min_vk;
	int cb_test_max_vk;
	int short_res_min;
	int lcd_noise_test_frame;
	int lcd_noise_test_max;
	int lcd_noise_coefficient;
	int lcd_noise_coefficient_key;
	int open_test_cb_min;
	bool open_test_k1_check;
	int open_test_k1;
	bool open_test_k2_check;
	int open_test_k2;
};

enum test_item_ft8719 {
	CODE_FT8719_ENTER_FACTORY_MODE = 0,
	CODE_FT8719_RAWDATA_TEST = 7,
	CODE_FT8719_CB_TEST = 12,
	CODE_FT8719_SHORT_CIRCUIT_TEST = 14,
	CODE_FT8719_LCD_NOISE_TEST = 15,
	CODE_FT8719_OPEN_TEST = 24,
};

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct ft8719_basic_threshold ft8719_basic_thr;
struct ft8719_test_item ft8719_item;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int ft8719_get_key_num(void)
{
	int ret = 0;
	int i = 0;
	u8 keyval = 0;

	test_data.screen_param.key_num = 0;
	for (i = 0; i < 3; i++) {
		ret = read_reg(FACTORY_REG_LEFT_KEY, &keyval);
		if (0 == ret) {
			if (((keyval >> 0) & 0x01)) {
				test_data.screen_param.left_key1 = true;
				++test_data.screen_param.key_num;
			}
			if (((keyval >> 1) & 0x01)) {
				test_data.screen_param.left_key2 = true;
				++test_data.screen_param.key_num;
			}
			if (((keyval >> 2) & 0x01)) {
				test_data.screen_param.left_key3 = true;
				++test_data.screen_param.key_num;
			}
			if (((keyval >> 3) & 0x01)) {
				test_data.screen_param.left_key4 = true;
				++test_data.screen_param.key_num;
			}
			break;
		} else {
			sys_delay(150);
			continue;
		}
	}

	if (i >= 3) {
		FTS_TEST_SAVE_INFO("can't get left key num");
		return ret;
	}
	return 0;
}

/************************************************************************
 * Name: ft8719_enter_factory_mode
 * Brief:  Check whether TP can enter Factory Mode, and do some thing
 * Input: none
 * Output: none
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int ft8719_enter_factory_mode(void)
{
	int ret = 0;

	ret = enter_factory_mode();
	if (ret < 0) {
		FTS_TEST_SAVE_INFO("enter factory mode fail, can't get tx/rx num");
		return ret;
	}
	ret = ft8719_get_key_num();
	if (ret < 0) {
		FTS_TEST_SAVE_INFO("get key num fail");
		return ret;
	}

	return ret;
}

/************************************************************************
 * Name: ft8719_short_test
 * Brief:  Get short circuit test mode data, judge whether
 *         there is a short circuit
 * Input: test_result
 * Output: test_result, PASS or FAIL
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int ft8719_short_test(bool *test_result)
{
	int ret = 0;
	bool temp_result = false;
	int res_min = 0;
	int byte_num = 0;
	int tx_num = 0;
	int rx_num = 0;
	int key_num = 0;
	int ch_num = 0;
	int row = 0;
	int col = 0;
	int tmp_adc = 0;
	int min = 0;
	int max = 0;
	int *adcdata = NULL;

	FTS_TEST_SAVE_INFO("==============================Test Item: -------- Short Circuit Test");

	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	key_num = test_data.screen_param.key_num_total;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	adcdata = test_data.buffer;

	ret = enter_factory_mode();
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Enter factory mode,ret=%d\n", ret);
		goto TEST_END;
	}

	byte_num = (tx_num * rx_num + key_num) * 2;
	ch_num = rx_num;
	ret = weakshort_get_adc_data_incell(TEST_RETVAL_AA, ch_num, byte_num, adcdata);
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to get AdcData,ret=%d\n", ret);
		goto TEST_END;
	}

	/* calculate resistor */
	for (row = 0; row < tx_num + 1; ++row) {
		for (col = 0; col < rx_num; ++col) {
			tmp_adc = adcdata[row * rx_num + col];
			if (tmp_adc < 50) {
				adcdata[row * rx_num + col] = -1;
				continue;
			} else if (tmp_adc < 200) {
				tmp_adc = 200;
			}
			adcdata[row * rx_num + col] = 252000 / (tmp_adc - 120) - 60;
		}
	}

	show_data_incell(adcdata, true);

	/* analyze data */
	res_min = ft8719_basic_thr.short_res_min;
	min = res_min;
	max = 100000000;
	FTS_TEST_SAVE_INFO("Short Circuit test, Set_Range=(%d, %d)\n", min, max);
	temp_result = compare_data_incell(adcdata, min, max, min, max, true);

	save_testdata_incell(adcdata, "Short Circuit Test", 0, tx_num + 1, rx_num, 1);

TEST_END:
	if (temp_result) {
		FTS_TEST_SAVE_INFO("\n==========Short Circuit Test is OK!\r\n");
		*test_result = true;
	} else {
		FTS_TEST_SAVE_INFO("\n==========Short Circuit Test is NG!\r\n");
		*test_result = false;
	}

	return ret;
}

/************************************************************************
 * Name: ft8719_open_test
 * Brief:  Check if channel is open
 * Input: test_result
 * Output: test_result, PASS or FAIL
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int ft8719_open_test(bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int min = 0;
	int max = 0;
	u8 reg_vgh = 0;
	u8 reg_vgl = 0;
	u8 k1 = 0;
	u8 k2 = 0;
	int tx_num = 0;
	int rx_num = 0;
	int cb_byte_num = 0;
	int *opendata = NULL;

	FTS_TEST_SAVE_INFO("\n\n==============================Test Item: --------  Open Test");

	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	opendata = test_data.buffer;

	ret = enter_factory_mode();
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nEnter Factory fail");
		goto TEST_ERR_FACTORY;
	}

	/* open test enable */
	ret = write_reg(FACTORY_REG_OPEN_TEST_EN, 0x01);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nopen test enable fail");
		goto TEST_ERR_FACTORY;
	}

	/* backup defaul value */
	ret = read_reg(FACTORY_REG_VGH02, &reg_vgh);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nread reg 0x20 fail");
		goto TEST_ERR_FACTORY;
	}
	ret = read_reg(FACTORY_REG_VGL02, &reg_vgl);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nread reg 0x21 fail");
		goto TEST_ERR_FACTORY;
	}
	if (ft8719_basic_thr.open_test_k1_check) {
		ret = read_reg(FACTORY_REG_K1, &k1);
		if (ret) {
			FTS_TEST_SAVE_INFO("\r\nread k1 fail");
			goto TEST_ERR_FACTORY;
		}
	}
	if (ft8719_basic_thr.open_test_k2_check) {
		ret = read_reg(FACTORY_REG_K2, &k2);
		if (ret) {
			FTS_TEST_SAVE_INFO("\r\nread k2 fail");
			goto TEST_ERR_FACTORY;
		}
	}

	/* set open test environment */
	ret = write_reg(FACTORY_REG_VGH02, 0x01);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nwrite reg 0x20 fail");
		goto TEST_ERR;
	}

	ret = write_reg(FACTORY_REG_VGL02, 0x01);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nwrite reg 0x21 fail");
		goto TEST_ERR;
	}

	if (ft8719_basic_thr.open_test_k1_check) {
		ret = write_reg(FACTORY_REG_K1, ft8719_basic_thr.open_test_k1);
		if (ret) {
			FTS_TEST_SAVE_INFO("\r\nwrite reg k1 fail");
			goto TEST_ERR;
		}
	}

	if (ft8719_basic_thr.open_test_k2_check) {
		ret = write_reg(FACTORY_REG_K2, ft8719_basic_thr.open_test_k2);
		if (ret) {
			FTS_TEST_SAVE_INFO("\r\nwrite reg k2 fail");
			goto TEST_ERR;
		}
	}

	/* wait fw state update before clb */
	wait_state_update();
	/* auto clb */
	ret = chip_clb_incell();
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nauto clb fail");
		goto TEST_ERR;
	}

	/* get cb data */
	cb_byte_num = tx_num * rx_num;
	ret = get_cb_incell(0, cb_byte_num, opendata);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nget CB fail");
		goto TEST_ERR;
	}

	/* show open data to testresult.txt */
	show_data_incell(opendata, false);

	/* compare data */
	min = ft8719_basic_thr.open_test_cb_min;
	max = TEST_OPEN_MAX_VALUE;
	FTS_TEST_SAVE_INFO("\nOpen test, Set_Range=(%d, %d)\n", min, max);
	tmp_result = compare_data_incell(opendata, min, max, 0, 0, false);

	/* save data to testdata.csv */
	save_testdata_incell(opendata, "Open Test", 0, tx_num, rx_num, 1);

TEST_ERR:
	/* restore reg */
	ret = write_reg(FACTORY_REG_VGH02, reg_vgh);
	if (ret) {
		tmp_result = false;
		FTS_TEST_SAVE_INFO("\r\nrestore 0x20 reg fail");
	}
	ret = write_reg(FACTORY_REG_VGL02, reg_vgl);
	if (ret) {
		tmp_result = false;
		FTS_TEST_SAVE_INFO("\r\nrestore 0x21 reg fail");
	}
	if (ft8719_basic_thr.open_test_k1_check) {
		ret = write_reg(FACTORY_REG_K1, k1);
		if (ret) {
			tmp_result = false;
			FTS_TEST_SAVE_INFO("\r\nrestore reg k1 fail");
		}
	}
	if (ft8719_basic_thr.open_test_k2_check) {
		ret = write_reg(FACTORY_REG_K2, k2);
		if (ret) {
			tmp_result = false;
			FTS_TEST_SAVE_INFO("\r\nrestore reg k2 fail");
		}
	}
	/* wait fw state update before clb */
	wait_state_update();
	/* auto clb */
	ret = chip_clb_incell();
	if (ret) {
		tmp_result = false;
		FTS_TEST_SAVE_INFO("\r\nauto clb fail");
	}

TEST_ERR_FACTORY:
	/* open test disable */
	ret = write_reg(FACTORY_REG_OPEN_TEST_EN, 0x0);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nopen test disable fail");
	}
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n==========Open Test is OK!\r\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n==========Open Test is NG!\r\n");
	}

	return ret;
}

/************************************************************************
 * Name: ft8719_cb_test
 * Brief:  TestItem: Cb Test. Check if Cb is within the range.
 * Input: none
 * Output: test_result, PASS or FAIL
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int ft8719_cb_test(bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int tx_num = 0;
	int rx_num = 0;
	int key_num = 0;
	int cb_byte_num = 0;
	bool include_key = false;
	int *cbdata = NULL;

	FTS_TEST_SAVE_INFO("\n\n==============================Test Item: --------  CB Test");

	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	key_num = test_data.screen_param.key_num_total;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	cbdata = test_data.buffer;

	ret = enter_factory_mode();
	if (ret) {
		FTS_TEST_SAVE_INFO(" Failed to Enter factory mode. Error Code: %d", ret);
		goto TEST_ERR;
	}

	/* cb test enable */
	ret = write_reg(FACTORY_REG_CB_TEST_EN, 0x01);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\ncb test enable fail");
		goto TEST_ERR;
	}

	/* auto clb */
	ret = chip_clb_incell();
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\n//========= auto clb Failed  !");
		goto TEST_ERR;
	}

	cb_byte_num = tx_num * rx_num + key_num;
	ret = get_cb_incell(0, cb_byte_num, cbdata);
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to get CB value...\n");
		goto TEST_ERR;
	}

	/* Show CbData to testresult.txt */
	include_key = ft8719_basic_thr.cb_test_vk_check;
	show_data_incell(cbdata, include_key);

	/* To Determine Cb if in Range or not */
	tmp_result = compare_detailthreshold_data_incell(cbdata,
							 test_data.incell_detail_thr.cb_test_min,
							 test_data.incell_detail_thr.cb_test_max, include_key);

	/* Save Test Data to testdata.csv */
	save_testdata_incell(cbdata, "CB Test", 0, tx_num + 1, rx_num, 1);

TEST_ERR:
	/* cb test disable */
	ret = write_reg(FACTORY_REG_CB_TEST_EN, 0x0);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\ncb test disable fail");
	}
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n==========CB Test is OK!\r\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n==========CB Test is NG!\r\n");
	}

	return ret;
}

/************************************************************************
* Name: ft8719_rawdata_test
* Brief:  TestItem: RawDataTest. Check if MCAP RawData is within the range.
* Input: test_result
* Output: test_result, PASS or FAIL
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int ft8719_rawdata_test(bool *test_result)
{
	int ret = 0;
	bool tmp_result = false;
	int tx_num = 0;
	int rx_num = 0;
	int key_num = 0;
	int i = 0;
	int *rawdata = NULL;

	FTS_TEST_SAVE_INFO("\n\n==============================Test Item: -------- Raw Data Test");
	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	key_num = test_data.screen_param.key_num_total;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	rawdata = test_data.buffer;

	ret = enter_factory_mode();
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Enter factory mode,ret=%d", ret);
		goto TEST_FAC_ERR;
	}

	/* rawdata test enable */
	ret = write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0x01);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nrawdata test enable fail");
		goto TEST_FAC_ERR;
	}

    /*********************GET RAWDATA*********************/
	for (i = 0; i < 3; i++) {
		/* Lost 3 Frames, In order to obtain stable data */
		ret = get_rawdata_incell(rawdata);
	}
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to get Raw Data!! Error Code: %d", ret);
		return ret;
	}
	/* Show RawData to testresult.txt */
	show_data_incell(rawdata, true);

	/* To Determine RawData if in Range or not */
	tmp_result = compare_detailthreshold_data_incell(rawdata,
							 test_data.incell_detail_thr.rawdata_test_min,
							 test_data.incell_detail_thr.rawdata_test_max, true);

	/* Save Test Data to testdata.csv */
	save_testdata_incell(rawdata, "RawData Test", 0, tx_num + 1, rx_num, 1);

TEST_FAC_ERR:
	/* rawdata test enable */
	ret = write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0x0);
	if (ret) {
		FTS_TEST_SAVE_INFO("\r\nrawdata test disable fail");
	}
	if (tmp_result) {
		*test_result = true;
		FTS_TEST_SAVE_INFO("\n==========RawData Test is OK!\n");
	} else {
		*test_result = false;
		FTS_TEST_SAVE_INFO("\n==========RawData Test is NG!\n");
	}
	return ret;
}

/************************************************************************
 * Name: ft8719_lcdnoise_test
 * Brief:  TestItem: LCD NoiseTest. Check if Noise is within the range.
 * Input: test_result
 * Output: test_result, PASS or FAIL
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int ft8719_lcdnoise_test(bool *test_result)
{
	int ret = 0;
	bool result_flag = false;
	u8 old_mode = 0;
	int row = 0;
	int col = 0;
	int value_min = 0;
	int value_max = 0;
	int vk_value_max = 0;
	int retry = 0;
	u8 status = 0xFF;
	u8 noise_value_va = 0xFF;
	u8 noise_value_vk = 0xFF;
	int tx_num = 0;
	int rx_num = 0;
	int key_num = 0;
	int frame_num = 0;
	int lcdnoise_bytenum = 0;
	int *lcdnoise = NULL;

	FTS_TEST_SAVE_INFO("\n\n==============================Test Item: -------- LCD Noise Test");

	tx_num = test_data.screen_param.tx_num;
	rx_num = test_data.screen_param.rx_num;
	key_num = test_data.screen_param.key_num_total;
	memset(test_data.buffer, 0, ((tx_num + 1) * rx_num) * sizeof(int));
	lcdnoise = test_data.buffer;

	ret = enter_factory_mode();
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Enter factory mode, ret:%d", ret);
		goto TEST_END;
	}

	/* switch to differ mode */
	ret = read_reg(FACTORY_REG_DATA_SELECT, &old_mode);
	ret = write_reg(FACTORY_REG_DATA_SELECT, 0x01);
	if (ret) {
		FTS_TEST_SAVE_INFO("write data select fail");
		goto TEST_END;
	}
	sys_delay(FACTORY_TEST_DELAY);

	/* RawData Address Register Offset cleared */
	ret = write_reg(FACTORY_REG_LINE_ADDR, 0xAD);
	if (ret) {
		FTS_TEST_SAVE_INFO("write line addr register fail");
		goto TEST_END;
	}

	/* set scan number */
	frame_num = ft8719_basic_thr.lcd_noise_test_frame;
	FTS_TEST_DBG(" lcd_noise_test_frame:%d\n", frame_num);
	ret = write_reg(FACTORY_REG_LCD_NOISE_FRAME, frame_num / 4);
	if (ret) {
		FTS_TEST_SAVE_INFO("write lcd noise frame fail");
		goto TEST_END;
	}

	/* start test */
	ret = write_reg(FACTORY_REG_LCD_NOISE_START, 0x01);
	if (ret) {
		FTS_TEST_SAVE_INFO("write lcd noise start fail");
		goto TEST_END;
	}

	/* check status */
	sys_delay(frame_num * FACTORY_TEST_DELAY / 2);
	for (retry = 0; retry < FACTORY_TEST_RETRY; retry++) {
		status = 0xFF;
		ret = read_reg(FACTORY_REG_LCD_NOISE_START, &status);
		if ((0 == ret) && (TEST_RETVAL_AA == status)) {
			break;
		} else {
			FTS_TEST_DBG("reg%x=%x,retry:%d\n", FACTORY_REG_LCD_NOISE_START, status, retry);
		}
		sys_delay(FACTORY_TEST_RETRY_DELAY);
	}
	if (retry >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_INFO("Scan Noise Time Out!");
		goto TEST_END;
	}

	lcdnoise_bytenum = tx_num * rx_num * 2 + key_num * 2;
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR, lcdnoise_bytenum, lcdnoise);
	if (ret) {
		FTS_TEST_SAVE_INFO("Failed to Read Data.\n");
		goto TEST_END;
	}

	for (row = 0; row < tx_num + 1; ++row) {
		for (col = 0; col < rx_num; ++col) {
			lcdnoise[row * rx_num + col] = focal_abs(lcdnoise[row * rx_num + col]);
		}
	}

	/* show and save data of lcd_noise to testresult.txt */
	show_data_incell(lcdnoise, true);

	/* analyze lcd noise data */
	noise_value_va = test_data.va_touch_thr;
	noise_value_vk = test_data.key_touch_thr;
	value_min = 0;
	value_max = ft8719_basic_thr.lcd_noise_coefficient * noise_value_va * 32 / 100;
	vk_value_max = ft8719_basic_thr.lcd_noise_coefficient_key * noise_value_vk * 32 / 100;
	FTS_TEST_SAVE_INFO("VA_Set_Range=(%d, %d). VK_Set_Range=(%d, %d)\n",
			   value_min, value_max, value_min, vk_value_max);
	result_flag = compare_data_incell(lcdnoise, value_min, value_max, value_min, vk_value_max, true);

	/* Save Test Data to csv */
	save_testdata_incell(lcdnoise, "LCD Noise Test", 0, tx_num + 1, rx_num, 1);

TEST_END:
	write_reg(FACTORY_REG_DATA_SELECT, old_mode);
	write_reg(FACTORY_REG_LCD_NOISE_START, 0x00);
	sys_delay(FACTORY_TEST_DELAY);
	if (result_flag) {
		FTS_TEST_SAVE_INFO("\n==========LCD Noise Test is OK!\r\n");
		*test_result = true;
	} else {
		FTS_TEST_SAVE_INFO("\n==========LCD Noise Test is NG!\r\n");
		*test_result = false;
	}

	return ret;
}

static bool start_test_ft8719(void)
{
	int ret = 0;
	bool test_result = true;
	bool temp_result = true;
	int item_count = 0;
	u8 item_code = 0;

	FTS_TEST_FUNC_ENTER();

	if (0 == test_data.test_num) {
		FTS_TEST_SAVE_INFO("test item == 0\n");
		return false;
	}

	for (item_count = 0; item_count < test_data.test_num; item_count++) {
		item_code = test_data.test_item[item_count].itemcode;
		test_data.test_item_code = item_code;

		/* FT8719_ENTER_FACTORY_MODE */
		if (CODE_FT8719_ENTER_FACTORY_MODE == item_code) {
			ret = ft8719_enter_factory_mode();
			if ((ret != 0) || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
				break;	/* if this item FAIL, no longer test. */
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* FT8719_SHORT_TEST */
		if (CODE_FT8719_SHORT_CIRCUIT_TEST == item_code) {
			ret = ft8719_short_test(&temp_result);
			if ((ret != 0) || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* FT8719_Open_TEST */
		if (CODE_FT8719_OPEN_TEST == item_code) {
			ret = ft8719_open_test(&temp_result);
			if ((ret != 0) || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* FT8719_CB_TEST */
		if (CODE_FT8719_CB_TEST == item_code) {
			ret = ft8719_cb_test(&temp_result);
			if ((ret != 0) || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* FT8719_RAWDATA_TEST */
		if (CODE_FT8719_RAWDATA_TEST == item_code) {
			ret = ft8719_rawdata_test(&temp_result);
			if ((ret != 0) || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}

		/* FT8719_LCD_NOISE_TEST */
		if (CODE_FT8719_LCD_NOISE_TEST == item_code) {
			ret = ft8719_lcdnoise_test(&temp_result);
			if ((ret != 0) || (!temp_result)) {
				test_result = false;
				test_data.test_item[item_count].testresult = RESULT_NG;
			} else
				test_data.test_item[item_count].testresult = RESULT_PASS;
		}
	}

	return test_result;
}

static int ft8719_open_selftest(void)
{
	int ret = SELFTEST_INVALID;
	bool temp_result = true;

	FTS_TEST_FUNC_ENTER();

	ret = ft8719_enter_factory_mode();
	if (ret != 0) {
		ret = SELFTEST_INVALID;
		goto end;
	}
	ret = ft8719_open_test(&temp_result);
	if ((ret != 0) || (!temp_result)) {
		ret = SELFTEST_FAIL;
	} else
		ret = SELFTEST_PASS;

	FTS_TEST_FUNC_EXIT();
end:
	return ret;
}

static int ft8719_short_selftest(void)
{
	int ret = SELFTEST_INVALID;
	bool temp_result = true;

	FTS_TEST_FUNC_ENTER();

	ret = ft8719_enter_factory_mode();
	if ((ret != 0)) {
		ret = SELFTEST_INVALID;
		goto end;
	}
	ret = ft8719_short_test(&temp_result);
	if ((ret != 0) || (!temp_result)) {
		ret = SELFTEST_FAIL;
	} else
		ret = SELFTEST_PASS;

	FTS_TEST_FUNC_EXIT();

end:
	return ret;
}

static void init_basicthreshold_ft8719(char *ini)
{
	char str[MAX_KEYWORD_VALUE_LEN] = { 0 };

	FTS_TEST_FUNC_ENTER();

	/* RawData Test */
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "5000", str, ini);
	ft8719_basic_thr.rawdata_test_min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "11000", str, ini);
	ft8719_basic_thr.rawdata_test_max = fts_atoi(str);

	/* CB Test */
	GetPrivateProfileString("Basic_Threshold", "CBTest_VA_Check", "1", str, ini);
	ft8719_basic_thr.cb_test_va_check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, ini);
	ft8719_basic_thr.cb_test_min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "60", str, ini);
	ft8719_basic_thr.cb_test_max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_VKey_Check", "1", str, ini);
	ft8719_basic_thr.cb_test_vk_check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Min_Vkey", "3", str, ini);
	ft8719_basic_thr.cb_test_min_vk = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "CBTest_Max_Vkey", "100", str, ini);
	ft8719_basic_thr.cb_test_max_vk = fts_atoi(str);

	/* Short Circuit Test */
	GetPrivateProfileString("Basic_Threshold", "ShortCircuit_ResMin", "200", str, ini);
	ft8719_basic_thr.short_res_min = fts_atoi(str);

	/* LCD Noise Test */
	GetPrivateProfileString("Basic_Threshold", "LCD_NoiseTest_Frame", "50", str, ini);
	ft8719_basic_thr.lcd_noise_test_frame = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "LCD_NoiseTest_Max", "30", str, ini);
	ft8719_basic_thr.lcd_noise_test_max = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "LCD_NoiseTest_Coefficient", "50", str, ini);
	ft8719_basic_thr.lcd_noise_coefficient = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "LCD_NoiseTest_Coefficient_Key", "50", str, ini);
	ft8719_basic_thr.lcd_noise_coefficient_key = fts_atoi(str);

	/* Open Test */
	GetPrivateProfileString("Basic_Threshold", "OpenTest_CBMin", "64", str, ini);
	ft8719_basic_thr.open_test_cb_min = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "OpenTest_Check_K1", "0", str, ini);
	ft8719_basic_thr.open_test_k1_check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "OpenTest_K1Threshold", "0", str, ini);
	ft8719_basic_thr.open_test_k1 = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "OpenTest_Check_K2", "0", str, ini);
	ft8719_basic_thr.open_test_k2_check = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "OpenTest_K2Threshold", "0", str, ini);
	ft8719_basic_thr.open_test_k2 = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();
}

static void init_testitem_ft8719(char *ini)
{
	char str[MAX_KEYWORD_VALUE_LEN] = { 0 };

	FTS_TEST_FUNC_ENTER();

	/* RawData Test */
	GetPrivateProfileString("TestItem", "RAWDATA_TEST", "1", str, ini);
	ft8719_item.rawdata_test = fts_atoi(str);

	/* CB_TEST */
	GetPrivateProfileString("TestItem", "CB_TEST", "1", str, ini);
	ft8719_item.cb_test = fts_atoi(str);

	/* SHORT_CIRCUIT_TEST */
	GetPrivateProfileString("TestItem", "SHORT_CIRCUIT_TEST", "0", str, ini);
	ft8719_item.short_test = fts_atoi(str);

	/* LCD_NOISE_TEST */
	GetPrivateProfileString("TestItem", "LCD_NOISE_TEST", "0", str, ini);
	ft8719_item.lcd_noise_test = fts_atoi(str);

	/* OPEN_TEST */
	GetPrivateProfileString("TestItem", "OPEN_TEST", "0", str, ini);
	ft8719_item.open_test = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();
}

static void init_detailthreshold_ft8719(char *ini)
{
	FTS_TEST_FUNC_ENTER();

	OnInit_InvalidNode(ini);
	OnInit_DThreshold_RawDataTest(ini);
	OnInit_DThreshold_CBTest(ini);

	FTS_TEST_FUNC_EXIT();
}

static void set_testitem_sequence_ft8719(void)
{
	FTS_TEST_FUNC_ENTER();

	test_data.test_num = 0;
	/* Enter Factory Mode */
	fts_set_testitem(CODE_FT8719_ENTER_FACTORY_MODE);

	/* Short Test */
	if (ft8719_item.short_test == 1) {
		fts_set_testitem(CODE_FT8719_SHORT_CIRCUIT_TEST);
	}

	/* Open Test */
	if (ft8719_item.open_test == 1) {
		fts_set_testitem(CODE_FT8719_OPEN_TEST);
	}

	/* CB_TEST */
	if (ft8719_item.cb_test == 1) {
		fts_set_testitem(CODE_FT8719_CB_TEST);
	}

	/* RawData Test */
	if (ft8719_item.rawdata_test == 1) {
		fts_set_testitem(CODE_FT8719_RAWDATA_TEST);
	}

	/* LCD_NOISE_TEST */
	if (ft8719_item.lcd_noise_test == 1) {
		fts_set_testitem(CODE_FT8719_LCD_NOISE_TEST);
	}

	FTS_TEST_FUNC_EXIT();
}

struct test_funcs test_func_ft8719 = {
	.ic_series = TEST_ICSERIES(IC_FT8719),
	.init_testitem = init_testitem_ft8719,
	.init_basicthreshold = init_basicthreshold_ft8719,
	.init_detailthreshold = init_detailthreshold_ft8719,
	.set_testitem_sequence = set_testitem_sequence_ft8719,
	.start_test = start_test_ft8719,
	.open_test = ft8719_open_selftest,
	.short_test = ft8719_short_selftest,
};

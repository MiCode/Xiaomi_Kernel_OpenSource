/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
*
* File Name: focaltech_test_detail_threshold.c
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: Set Detail Threshold for all IC
*
************************************************************************/

#include "focaltech_test.h"

int malloc_struct_DetailThreshold(void)
{
	FTS_TEST_FUNC_ENTER();

	/*malloc mcap detailthreshold */
	test_data.mcap_detail_thr.invalid_node =
	    (unsigned char (*)[RX_NUM_MAX])fts_malloc(NUM_MAX *
						      sizeof(unsigned char));
	if (NULL == test_data.mcap_detail_thr.invalid_node)
		goto ERR;
	test_data.mcap_detail_thr.invalid_node_sc =
	    (unsigned char (*)[RX_NUM_MAX])fts_malloc(NUM_MAX *
						      sizeof(unsigned char));
	if (NULL == test_data.mcap_detail_thr.invalid_node_sc)
		goto ERR;
	test_data.mcap_detail_thr.rawdata_test_min =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.rawdata_test_min)
		goto ERR;
	test_data.mcap_detail_thr.rawdata_test_max =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.rawdata_test_max)
		goto ERR;
	test_data.mcap_detail_thr.rawdata_test_low_min =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.rawdata_test_low_min)
		goto ERR;
	test_data.mcap_detail_thr.rawdata_test_low_max =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.rawdata_test_low_max)
		goto ERR;
	test_data.mcap_detail_thr.rawdata_test_high_min =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.rawdata_test_high_min)
		goto ERR;
	test_data.mcap_detail_thr.rawdata_test_high_max =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.rawdata_test_high_max)
		goto ERR;
	test_data.mcap_detail_thr.rx_linearity_test_max =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.rx_linearity_test_max)
		goto ERR;
	test_data.mcap_detail_thr.tx_linearity_test_max =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.tx_linearity_test_max)
		goto ERR;
	test_data.mcap_detail_thr.scap_rawdata_on_max =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.scap_rawdata_on_max)
		goto ERR;
	test_data.mcap_detail_thr.scap_rawdata_on_min =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.scap_rawdata_on_min)
		goto ERR;
	test_data.mcap_detail_thr.scap_rawdata_off_max =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.scap_rawdata_off_max)
		goto ERR;
	test_data.mcap_detail_thr.scap_rawdata_off_min =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.scap_rawdata_off_min)
		goto ERR;
	test_data.mcap_detail_thr.scap_cb_test_on_max =
	    (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(short));
	if (NULL == test_data.mcap_detail_thr.scap_cb_test_on_max)
		goto ERR;
	test_data.mcap_detail_thr.scap_cb_test_on_min =
	    (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(short));
	if (NULL == test_data.mcap_detail_thr.scap_cb_test_on_min)
		goto ERR;
	test_data.mcap_detail_thr.scap_cb_test_off_max =
	    (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(short));
	if (NULL == test_data.mcap_detail_thr.scap_cb_test_off_max)
		goto ERR;
	test_data.mcap_detail_thr.scap_cb_test_off_min =
	    (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(short));
	if (NULL == test_data.mcap_detail_thr.scap_cb_test_off_min)
		goto ERR;
	test_data.mcap_detail_thr.noise_test_coefficient =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.noise_test_coefficient)
		goto ERR;
	test_data.mcap_detail_thr.panel_differ_test_max =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.panel_differ_test_max)
		goto ERR;
	test_data.mcap_detail_thr.panel_differ_test_min =
	    (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.mcap_detail_thr.panel_differ_test_min)
		goto ERR;

	/*malloc incell detailthreshold */
	test_data.incell_detail_thr.invalid_node =
	    fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.incell_detail_thr.invalid_node)
		goto ERR;
	test_data.incell_detail_thr.rawdata_test_min =
	    fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.incell_detail_thr.rawdata_test_min)
		goto ERR;
	test_data.incell_detail_thr.rawdata_test_max =
	    fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.incell_detail_thr.rawdata_test_max)
		goto ERR;
	test_data.incell_detail_thr.rawdata_test_b_frame_min =
	    fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.incell_detail_thr.rawdata_test_min)
		goto ERR;
	test_data.incell_detail_thr.rawdata_test_b_frame_max =
	    fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.incell_detail_thr.rawdata_test_max)
		goto ERR;
	test_data.incell_detail_thr.cb_test_min =
	    fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.incell_detail_thr.cb_test_min)
		goto ERR;
	test_data.incell_detail_thr.cb_test_max =
	    fts_malloc(NUM_MAX * sizeof(int));
	if (NULL == test_data.incell_detail_thr.cb_test_max)
		goto ERR;

	FTS_TEST_FUNC_EXIT();

	return 0;

ERR:
	FTS_TEST_ERROR("fts_malloc memory failed in function.");
	return -ENOMEM;
}

void free_struct_DetailThreshold(void)
{

	/*free mcap detailthreshold */
	if (NULL != test_data.mcap_detail_thr.invalid_node) {
		fts_free(test_data.mcap_detail_thr.invalid_node);
		test_data.mcap_detail_thr.invalid_node = NULL;
	}
	if (NULL != test_data.mcap_detail_thr.invalid_node_sc) {
		fts_free(test_data.mcap_detail_thr.invalid_node_sc);
		test_data.mcap_detail_thr.invalid_node_sc = NULL;
	}
	if (NULL != test_data.mcap_detail_thr.rawdata_test_min) {
		fts_free(test_data.mcap_detail_thr.rawdata_test_min);
	}
	if (NULL != test_data.mcap_detail_thr.rawdata_test_max) {
		fts_free(test_data.mcap_detail_thr.rawdata_test_max);
	}
	if (NULL != test_data.mcap_detail_thr.rawdata_test_low_min) {
		fts_free(test_data.mcap_detail_thr.rawdata_test_low_min);
	}
	if (NULL != test_data.mcap_detail_thr.rawdata_test_low_max) {
		fts_free(test_data.mcap_detail_thr.rawdata_test_low_max);
	}
	if (NULL != test_data.mcap_detail_thr.rawdata_test_high_min) {
		fts_free(test_data.mcap_detail_thr.rawdata_test_high_min);
	}
	if (NULL != test_data.mcap_detail_thr.rawdata_test_high_max) {
		fts_free(test_data.mcap_detail_thr.rawdata_test_high_max);
	}
	if (NULL != test_data.mcap_detail_thr.rx_linearity_test_max) {
		fts_free(test_data.mcap_detail_thr.rx_linearity_test_max);
	}
	if (NULL != test_data.mcap_detail_thr.tx_linearity_test_max) {
		fts_free(test_data.mcap_detail_thr.tx_linearity_test_max);
	}
	if (NULL != test_data.mcap_detail_thr.scap_rawdata_on_max) {
		fts_free(test_data.mcap_detail_thr.scap_rawdata_on_max);
	}
	if (NULL != test_data.mcap_detail_thr.scap_rawdata_on_min) {
		fts_free(test_data.mcap_detail_thr.scap_rawdata_on_min);
	}
	if (NULL != test_data.mcap_detail_thr.scap_rawdata_off_max) {
		fts_free(test_data.mcap_detail_thr.scap_rawdata_off_max);
	}
	if (NULL != test_data.mcap_detail_thr.scap_rawdata_off_min) {
		fts_free(test_data.mcap_detail_thr.scap_rawdata_off_min);
	}
	if (NULL != test_data.mcap_detail_thr.scap_cb_test_on_max) {
		fts_free(test_data.mcap_detail_thr.scap_cb_test_on_max);
	}
	if (NULL != test_data.mcap_detail_thr.scap_cb_test_on_min) {
		fts_free(test_data.mcap_detail_thr.scap_cb_test_on_min);
	}
	if (NULL != test_data.mcap_detail_thr.scap_cb_test_off_max) {
		fts_free(test_data.mcap_detail_thr.scap_cb_test_off_max);
	}
	if (NULL != test_data.mcap_detail_thr.scap_cb_test_off_min) {
		fts_free(test_data.mcap_detail_thr.scap_cb_test_off_min);
	}
	if (NULL != test_data.mcap_detail_thr.noise_test_coefficient) {
		fts_free(test_data.mcap_detail_thr.noise_test_coefficient);
		test_data.mcap_detail_thr.noise_test_coefficient = NULL;
	}
	if (NULL != test_data.mcap_detail_thr.panel_differ_test_max) {
		fts_free(test_data.mcap_detail_thr.panel_differ_test_max);
		test_data.mcap_detail_thr.panel_differ_test_max = NULL;
	}
	if (NULL != test_data.mcap_detail_thr.panel_differ_test_min) {
		fts_free(test_data.mcap_detail_thr.panel_differ_test_min);
		test_data.mcap_detail_thr.panel_differ_test_min = NULL;
	}

	/*free incell detailthreshold */
	if (NULL != test_data.incell_detail_thr.invalid_node) {
		fts_free(test_data.incell_detail_thr.invalid_node);
		test_data.incell_detail_thr.invalid_node = NULL;
	}
	if (NULL != test_data.incell_detail_thr.rawdata_test_min) {
		fts_free(test_data.incell_detail_thr.rawdata_test_min);
		test_data.incell_detail_thr.rawdata_test_min = NULL;
	}
	if (NULL != test_data.incell_detail_thr.rawdata_test_max) {
		fts_free(test_data.incell_detail_thr.rawdata_test_max);
		test_data.incell_detail_thr.rawdata_test_max = NULL;
	}
	if (NULL != test_data.incell_detail_thr.rawdata_test_b_frame_min) {
		fts_free(test_data.incell_detail_thr.rawdata_test_b_frame_min);
		test_data.incell_detail_thr.rawdata_test_b_frame_min = NULL;
	}
	if (NULL != test_data.incell_detail_thr.rawdata_test_b_frame_max) {
		fts_free(test_data.incell_detail_thr.rawdata_test_b_frame_max);
		test_data.incell_detail_thr.rawdata_test_b_frame_max = NULL;
	}
	if (NULL != test_data.incell_detail_thr.cb_test_min) {
		fts_free(test_data.incell_detail_thr.cb_test_min);
		test_data.incell_detail_thr.cb_test_min = NULL;
	}
	if (NULL != test_data.incell_detail_thr.cb_test_max) {
		fts_free(test_data.incell_detail_thr.cb_test_max);
		test_data.incell_detail_thr.cb_test_max = NULL;
	}
}

void OnInit_InvalidNode(char *strIniFile)
{

	char str[MAX_PATH] = { 0 }, strTemp[MAX_PATH] = {
	0};
	int i = 0, j = 0;
	FTS_TEST_FUNC_ENTER();

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			snprintf(strTemp, MAX_PATH, "InvalidNode[%d][%d]", (i + 1),
				(j + 1));

			GetPrivateProfileString("INVALID_NODE", strTemp, "1",
						str, strIniFile);
			if (fts_atoi(str) == 0) {
				test_data.mcap_detail_thr.invalid_node[i][j] =
				    0;
				test_data.incell_detail_thr.invalid_node[i *
									 test_data.
									 screen_param.
									 rx_num
									 + j] =
				    0;
				FTS_TEST_DBG("node (%d, %d)", (i + 1), (j + 1));

			} else if (fts_atoi(str) == 2) {
				test_data.mcap_detail_thr.invalid_node[i][j] =
				    2;
				test_data.incell_detail_thr.invalid_node[i *
									 test_data.
									 screen_param.
									 rx_num
									 + j] =
				    2;
			} else {
				test_data.mcap_detail_thr.invalid_node[i][j] =
				    1;
				test_data.incell_detail_thr.invalid_node[i *
									 test_data.
									 screen_param.
									 rx_num
									 + j] =
				    1;
			}
		}
	}

	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			snprintf(strTemp, MAX_PATH, "InvalidNodeS[%d][%d]", (i + 1),
				(j + 1));
			GetPrivateProfileString("INVALID_NODES", strTemp, "1",
						str, strIniFile);
			if (fts_atoi(str) == 0) {
				test_data.mcap_detail_thr.
				    invalid_node_sc[i][j] = 0;
			} else if (fts_atoi(str) == 2) {
				test_data.mcap_detail_thr.
				    invalid_node_sc[i][j] = 2;
			} else
				test_data.mcap_detail_thr.
				    invalid_node_sc[i][j] = 1;
		}

	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_RawDataTest(char *strIniFile)
{
	char str[128];
	char strTemp[MAX_PATH];
	char strValue[MAX_PATH];
	int MaxValue, MinValue, B_MaxValue, B_MinValue;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	if ((TEST_ICSERIES(test_data.screen_param.selected_ic) ==
	     TEST_ICSERIES(IC_FT8716))
	    || (TEST_ICSERIES(test_data.screen_param.selected_ic) ==
		TEST_ICSERIES(IC_FT8736))
	    || (TEST_ICSERIES(test_data.screen_param.selected_ic) ==
		TEST_ICSERIES(IC_FTE716))) {
		return;
	}

	/*RawData Test */
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "10000",
				str, strIniFile);
	MaxValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_BFrame_Max",
				"5000", str, strIniFile);
	B_MaxValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.mcap_detail_thr.rawdata_test_max[i][j] =
			    MaxValue;
			test_data.incell_detail_thr.rawdata_test_max[i *
								     test_data.
								     screen_param.
								     rx_num +
								     j] =
			    MaxValue;
			test_data.incell_detail_thr.rawdata_test_b_frame_max[i *
									     test_data.
									     screen_param.
									     rx_num
									     +
									     j]
			    = B_MaxValue;
		}
	}

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "RawData_Max_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "111", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    rawdata_test_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				test_data.incell_detail_thr.rawdata_test_max[i *
									     test_data.
									     screen_param.
									     rx_num
									     +
									     k]
				    = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}

	}

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "7000",
				str, strIniFile);
	MinValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_BFrame_Min",
				"11000", str, strIniFile);
	B_MinValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.mcap_detail_thr.rawdata_test_min[i][j] =
			    MinValue;
			test_data.incell_detail_thr.rawdata_test_min[i *
								     test_data.
								     screen_param.
								     rx_num +
								     j] =
			    MinValue;
			test_data.incell_detail_thr.rawdata_test_b_frame_min[i *
									     test_data.
									     screen_param.
									     rx_num
									     +
									     j]
			    = B_MinValue;
		}
	}
	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "RawData_Min_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    rawdata_test_min[i][k] =
				    (short)(fts_atoi(str_tmp));
				test_data.incell_detail_thr.rawdata_test_min[i *
									     test_data.
									     screen_param.
									     rx_num
									     +
									     k]
				    = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	/*RawData Test Low */
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Max",
				"15000", str, strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.mcap_detail_thr.rawdata_test_low_max[i][j] =
			    MaxValue;
		}
	}
	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "RawData_Max_Low_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    rawdata_test_low_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Min",
				"3000", str, strIniFile);
	MinValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.mcap_detail_thr.rawdata_test_low_min[i][j] =
			    MinValue;
		}
	}
	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "RawData_Min_Low_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    rawdata_test_low_min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	/*RawData Test High */
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Max",
				"15000", str, strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.mcap_detail_thr.rawdata_test_high_max[i][j] =
			    MaxValue;
		}
	}
	GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Min",
				"3000", str, strIniFile);
	MinValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.mcap_detail_thr.rawdata_test_high_min[i][j] =
			    MinValue;
		}
	}
	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "RawData_Max_High_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    rawdata_test_high_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "RawData_Min_High_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    rawdata_test_high_min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_SCapRawDataTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	/*SCapRawDataTest_OFF */
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Min",
				"150", str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Max",
				"1000", str, strIniFile);
	MaxValue = fts_atoi(str);

	/*Max */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.scap_rawdata_off_max[i][j] =
			    MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		snprintf(str, 128, "ScapRawData_OFF_Max_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    scap_rawdata_off_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	/*Min */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.scap_rawdata_off_min[i][j] =
			    MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		snprintf(str, 128, "ScapRawData_OFF_Min_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    scap_rawdata_off_min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	 /*ON*/ GetPrivateProfileString("Basic_Threshold",
					"SCapRawDataTest_ON_Min", "150", str,
					strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Max",
				"1000", str, strIniFile);
	MaxValue = fts_atoi(str);

	/*Max */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.scap_rawdata_on_max[i][j] =
			    MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		snprintf(str, 128, "ScapRawData_ON_Max_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    scap_rawdata_on_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	/*Min */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.scap_rawdata_on_min[i][j] =
			    MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		snprintf(str, 128, "ScapRawData_ON_Min_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    scap_rawdata_on_min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_SCapCbTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	/*SCapCbTest_ON */
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Min", "0",
				str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Max", "240",
				str, strIniFile);
	MaxValue = fts_atoi(str);

	/*Max */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.scap_cb_test_on_max[i][j] =
			    MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		snprintf(str, 128, "ScapCB_ON_Max_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    scap_cb_test_on_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	/*Min */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.scap_cb_test_on_min[i][j] =
			    MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		snprintf(str, 128, "ScapCB_ON_Min_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    scap_cb_test_on_min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	/*SCapCbTest_OFF */
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Min", "0",
				str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Max", "240",
				str, strIniFile);
	MaxValue = fts_atoi(str);
	/*Max */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.scap_cb_test_off_max[i][j] =
			    MaxValue;
		}
	}
	for (i = 0; i < 2; i++) {
		snprintf(str, 128, "ScapCB_OFF_Max_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    scap_cb_test_off_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	/*Min */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.scap_cb_test_off_min[i][j] =
			    MinValue;
		}
	}
	for (i = 0; i < 2; i++) {
		snprintf(str, 128, "ScapCB_OFF_Min_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    scap_cb_test_off_min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_PanelDifferTest(char *strIniFile)
{

	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int dividerPos = 0;
	int index = 0;
	int k = 0, i = 0, j = 0;
	char str_tmp[128];

	FTS_TEST_FUNC_ENTER();
	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Max",
				"1000", str, strIniFile);
	MaxValue = fts_atoi(str);
	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			test_data.mcap_detail_thr.panel_differ_test_max[i][j] =
			    MaxValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		snprintf(str, 128, "Panel_Differ_Max_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    panel_differ_test_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Min", "150",
				str, strIniFile);
	MinValue = fts_atoi(str);
	for (i = 0; i < TX_NUM_MAX; i++) {
		for (j = 0; j < RX_NUM_MAX; j++) {
			test_data.mcap_detail_thr.panel_differ_test_min[i][j] =
			    MinValue;
		}
	}

	for (i = 0; i < TX_NUM_MAX; i++) {
		snprintf(str, 128, "Panel_Differ_Min_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    panel_differ_test_min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();

}

void OnInit_DThreshold_RxLinearityTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	GetPrivateProfileString("Basic_Threshold", "RxLinearityTest_Max", "50",
				str, strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.rx_linearity_test_max[i][j] =
			    MaxValue;
		}
	}

	for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
		snprintf(str, 128, "Rx_Linearity_Max_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "111", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    rx_linearity_test_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}

	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_TxLinearityTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue = 0;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	GetPrivateProfileString("Basic_Threshold", "TxLinearityTest_Max", "50",
				str, strIniFile);
	MaxValue = fts_atoi(str);


	for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.tx_linearity_test_max[i][j] =
			    MaxValue;
		}
	}

	for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
		snprintf(str, 128, "Tx_Linearity_Max_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "111", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    tx_linearity_test_max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}

	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_ForceTouch_SCapRawDataTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	GetPrivateProfileString("Basic_Threshold",
				"ForceTouch_SCapRawDataTest_OFF_Min", "150",
				str, strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold",
				"ForceTouch_SCapRawDataTest_OFF_Max", "1000",
				str, strIniFile);
	MaxValue = fts_atoi(str);
	for (i = 0; i < 1; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.
			    ForceTouch_SCapRawDataTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		snprintf(str, 128, "ForceTouch_ScapRawData_OFF_Max_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    ForceTouch_SCapRawDataTest_OFF_Max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	for (i = 0; i < 1; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.
			    ForceTouch_SCapRawDataTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		snprintf(str, 128, "ForceTouch_ScapRawData_OFF_Min_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    ForceTouch_SCapRawDataTest_OFF_Min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold",
				"ForceTouch_SCapRawDataTest_ON_Min", "150", str,
				strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold",
				"ForceTouch_SCapRawDataTest_ON_Max", "1000",
				str, strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < 1; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.
			    ForceTouch_SCapRawDataTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		snprintf(str, 128, "ForceTouch_ScapRawData_ON_Max_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    ForceTouch_SCapRawDataTest_ON_Max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	for (i = 0; i < 1; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.
			    ForceTouch_SCapRawDataTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		snprintf(str, 128, "ForceTouch_ScapRawData_ON_Min_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    ForceTouch_SCapRawDataTest_ON_Min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_ForceTouch_SCapCbTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	GetPrivateProfileString("Basic_Threshold",
				"ForceTouch_SCapCbTest_ON_Min", "0", str,
				strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold",
				"ForceTouch_SCapCbTest_ON_Max", "240", str,
				strIniFile);
	MaxValue = fts_atoi(str);
	for (i = 0; i < 1; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.
			    ForceTouch_SCapCbTest_ON_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		snprintf(str, 128, "ForceTouch_ScapCB_ON_Max_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    ForceTouch_SCapCbTest_ON_Max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	for (i = 0; i < 1; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.
			    ForceTouch_SCapCbTest_ON_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		snprintf(str, 128, "ForceTouch_ScapCB_ON_Min_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		FTS_TEST_DBG("%s\r", strTemp);
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    ForceTouch_SCapCbTest_ON_Min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold",
				"ForceTouch_SCapCbTest_OFF_Min", "0", str,
				strIniFile);
	MinValue = fts_atoi(str);
	GetPrivateProfileString("Basic_Threshold",
				"ForceTouch_SCapCbTest_OFF_Max", "240", str,
				strIniFile);
	MaxValue = fts_atoi(str);
	for (i = 0; i < 1; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.
			    ForceTouch_SCapCbTest_OFF_Max[i][j] = MaxValue;
		}
	}
	for (i = 0; i < 1; i++) {
		snprintf(str, 128, "ForceTouch_ScapCB_OFF_Max_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    ForceTouch_SCapCbTest_OFF_Max[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	for (i = 0; i < 1; i++) {
		for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
			test_data.mcap_detail_thr.
			    ForceTouch_SCapCbTest_OFF_Min[i][j] = MinValue;
		}
	}
	for (i = 0; i < 1; i++) {
		snprintf(str, 128, "ForceTouch_ScapCB_OFF_Min_%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0x00, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.mcap_detail_thr.
				    ForceTouch_SCapCbTest_OFF_Min[i][k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_CBTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue, MaxValue_Vkey, MinValue_Vkey;
	int ChannelNumTest_ChannelXNum, ChannelNumTest_ChannelYNum;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();
	if ((TEST_ICSERIES(test_data.screen_param.selected_ic) ==
	     TEST_ICSERIES(IC_FT8606))) {
		return;
	}

	GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str,
				strIniFile);
	MaxValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "CBTest_Max_Vkey", "100",
				str, strIniFile);
	MaxValue_Vkey = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX",
				"15", str, strIniFile);
	ChannelNumTest_ChannelXNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY",
				"24", str, strIniFile);
	ChannelNumTest_ChannelYNum = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.incell_detail_thr.cb_test_max[i *
								test_data.
								screen_param.
								rx_num + j] =
			    MaxValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < test_data.screen_param.rx_num; j++) {
				test_data.incell_detail_thr.cb_test_max[i *
									test_data.
									screen_param.
									rx_num +
									j] =
				    MaxValue_Vkey;
			}
		}
	}

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "CB_Max_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.incell_detail_thr.cb_test_max[i *
									test_data.
									screen_param.
									rx_num +
									k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str,
				strIniFile);
	MinValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "CBTest_Min_Vkey", "3", str,
				strIniFile);
	MinValue_Vkey = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.incell_detail_thr.cb_test_min[i *
								test_data.
								screen_param.
								rx_num + j] =
			    MinValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < test_data.screen_param.rx_num; j++) {
				test_data.incell_detail_thr.cb_test_min[i *
									test_data.
									screen_param.
									rx_num +
									j] =
				    MinValue_Vkey;
			}
		}
	}

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "CB_Min_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.incell_detail_thr.cb_test_min[i *
									test_data.
									screen_param.
									rx_num +
									k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_AllButtonCBTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	if ((TEST_ICSERIES(test_data.screen_param.selected_ic) ==
	     TEST_ICSERIES(IC_FT8606))) {
		return;
	}

	GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str,
				strIniFile);
	MaxValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.incell_detail_thr.cb_test_max[i *
								test_data.
								screen_param.
								rx_num + j] =
			    MaxValue;
		}
	}

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "CB_Max_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.incell_detail_thr.cb_test_max[i *
									test_data.
									screen_param.
									rx_num +
									k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str,
				strIniFile);
	MinValue = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.incell_detail_thr.cb_test_min[i *
								test_data.
								screen_param.
								rx_num + j] =
			    MinValue;
		}
	}

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "CB_Min_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.incell_detail_thr.cb_test_min[i *
									test_data.
									screen_param.
									rx_num +
									k] =
				    (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	FTS_TEST_FUNC_EXIT();
}

void OnThreshold_VkAndVaRawDataSeparateTest(char *strIniFile)
{
	char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
	int MaxValue, MinValue, MaxValue_Vkey, MinValue_Vkey;
	int ChannelNumTest_ChannelXNum, ChannelNumTest_ChannelYNum;
	int dividerPos = 0;
	char str_tmp[128];
	int index = 0;
	int k = 0, i = 0, j = 0;

	FTS_TEST_FUNC_ENTER();

	if ((TEST_ICSERIES(test_data.screen_param.selected_ic) !=
	     TEST_ICSERIES(IC_FT8716))
	    && (TEST_ICSERIES(test_data.screen_param.selected_ic) !=
		TEST_ICSERIES(IC_FT8736))
	    && (TEST_ICSERIES(test_data.screen_param.selected_ic) !=
		TEST_ICSERIES(IC_FTE716))) {
		return;
	}

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "11000",
				str, strIniFile);
	MaxValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max_VKey",
				"11000", str, strIniFile);
	MaxValue_Vkey = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX",
				"15", str, strIniFile);
	ChannelNumTest_ChannelXNum = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY",
				"24", str, strIniFile);
	ChannelNumTest_ChannelYNum = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.incell_detail_thr.rawdata_test_max[i *
								     test_data.
								     screen_param.
								     rx_num +
								     j] =
			    MaxValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < test_data.screen_param.rx_num; j++) {
				test_data.incell_detail_thr.rawdata_test_max[i *
									     test_data.
									     screen_param.
									     rx_num
									     +
									     j]
				    = MaxValue_Vkey;
			}
		}
	}

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "RawData_Max_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.incell_detail_thr.rawdata_test_max[i *
									     test_data.
									     screen_param.
									     rx_num
									     +
									     k]
				    = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "5000",
				str, strIniFile);
	MinValue = fts_atoi(str);

	GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min_VKey",
				"5000", str, strIniFile);
	MinValue_Vkey = fts_atoi(str);

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		for (j = 0; j < test_data.screen_param.rx_num; j++) {
			test_data.incell_detail_thr.rawdata_test_min[i *
								     test_data.
								     screen_param.
								     rx_num +
								     j] =
			    MinValue;
		}

		if (i == ChannelNumTest_ChannelXNum) {
			for (j = 0; j < test_data.screen_param.rx_num; j++) {
				test_data.incell_detail_thr.rawdata_test_min[i *
									     test_data.
									     screen_param.
									     rx_num
									     +
									     j]
				    = MinValue_Vkey;
			}
		}
	}

	for (i = 0; i < test_data.screen_param.tx_num + 1; i++) {
		snprintf(str, 128, "RawData_Min_Tx%d", (i + 1));
		dividerPos =
		    GetPrivateProfileString("SpecialSet", str, "NULL", strTemp,
					    strIniFile);
		snprintf(strValue, MAX_PATH, "%s", strTemp);
		if (0 == dividerPos)
			continue;
		index = 0;
		k = 0;
		memset(str_tmp, 0, sizeof(str_tmp));
		for (j = 0; j < dividerPos; j++) {
			if (',' == strValue[j]) {
				test_data.incell_detail_thr.rawdata_test_min[i *
									     test_data.
									     screen_param.
									     rx_num
									     +
									     k]
				    = (short)(fts_atoi(str_tmp));
				index = 0;
				memset(str_tmp, 0x00, sizeof(str_tmp));
				k++;
			} else {
				if (' ' == strValue[j])
					continue;
				str_tmp[index] = strValue[j];
				index++;
			}
		}
	}
	FTS_TEST_FUNC_EXIT();
}

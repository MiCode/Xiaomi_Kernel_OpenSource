/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)，All Rights Reserved.
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

    test_data.mcap_detail_thr.InvalidNode = (unsigned char (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(unsigned char));
    if (NULL == test_data.mcap_detail_thr.InvalidNode) goto ERR;
    test_data.mcap_detail_thr.InvalidNode_SC = (unsigned char (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(unsigned char));
    if (NULL == test_data.mcap_detail_thr.InvalidNode_SC) goto ERR;
    test_data.mcap_detail_thr.RawDataTest_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.RawDataTest_Min) goto ERR;
    test_data.mcap_detail_thr.RawDataTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.RawDataTest_Max) goto ERR;
    test_data.mcap_detail_thr.RawDataTest_Low_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.RawDataTest_Low_Min) goto ERR;
    test_data.mcap_detail_thr.RawDataTest_Low_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.RawDataTest_Low_Max) goto ERR;
    test_data.mcap_detail_thr.RawDataTest_High_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.RawDataTest_High_Min) goto ERR;
    test_data.mcap_detail_thr.RawDataTest_High_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.RawDataTest_High_Max) goto ERR;
    test_data.mcap_detail_thr.RxLinearityTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.RxLinearityTest_Max) goto ERR;
    test_data.mcap_detail_thr.TxLinearityTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.TxLinearityTest_Max) goto ERR;
    test_data.mcap_detail_thr.SCapRawDataTest_ON_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.SCapRawDataTest_ON_Max) goto ERR;
    test_data.mcap_detail_thr.SCapRawDataTest_ON_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.SCapRawDataTest_ON_Min) goto ERR;
    test_data.mcap_detail_thr.SCapRawDataTest_OFF_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.SCapRawDataTest_OFF_Max) goto ERR;
    test_data.mcap_detail_thr.SCapRawDataTest_OFF_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.SCapRawDataTest_OFF_Min) goto ERR;
    test_data.mcap_detail_thr.SCapCbTest_ON_Max = (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(short));
    if (NULL == test_data.mcap_detail_thr.SCapCbTest_ON_Max) goto ERR;
    test_data.mcap_detail_thr.SCapCbTest_ON_Min = (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(short));
    if (NULL == test_data.mcap_detail_thr.SCapCbTest_ON_Min) goto ERR;
    test_data.mcap_detail_thr.SCapCbTest_OFF_Max = (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(short));
    if (NULL == test_data.mcap_detail_thr.SCapCbTest_OFF_Max) goto ERR;
    test_data.mcap_detail_thr.SCapCbTest_OFF_Min = (short (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(short));
    if (NULL == test_data.mcap_detail_thr.SCapCbTest_OFF_Min) goto ERR;
    test_data.mcap_detail_thr.NoistTest_Coefficient = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.NoistTest_Coefficient) goto ERR;
    test_data.mcap_detail_thr.PanelDifferTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.PanelDifferTest_Max) goto ERR;
    test_data.mcap_detail_thr.PanelDifferTest_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL == test_data.mcap_detail_thr.PanelDifferTest_Min) goto ERR;
    test_data.incell_detail_thr.InvalidNode =  (unsigned char (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(unsigned char));
    if (NULL ==  test_data.incell_detail_thr.InvalidNode)  goto ERR;
    test_data.incell_detail_thr.RawDataTest_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL ==  test_data.incell_detail_thr.RawDataTest_Min) goto ERR;
    test_data.incell_detail_thr.RawDataTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL ==  test_data.incell_detail_thr.RawDataTest_Max)goto ERR;
    test_data.incell_detail_thr.CBTest_Min = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL ==  test_data.incell_detail_thr.CBTest_Min)goto ERR;
    test_data.incell_detail_thr.CBTest_Max = (int (*)[RX_NUM_MAX])fts_malloc(NUM_MAX * sizeof(int));
    if (NULL ==  test_data.incell_detail_thr.CBTest_Max)goto ERR;

    FTS_TEST_FUNC_EXIT();

    return 0;

ERR:
    FTS_TEST_ERROR("fts_malloc memory failed in function.");
    return -1;
}

void free_struct_DetailThreshold(void)
{
    if (NULL != test_data.mcap_detail_thr.InvalidNode) {
        fts_free(test_data.mcap_detail_thr.InvalidNode);
        test_data.mcap_detail_thr.InvalidNode = NULL;
    }
    if (NULL != test_data.mcap_detail_thr.InvalidNode_SC) {
        fts_free(test_data.mcap_detail_thr.InvalidNode_SC);
        test_data.mcap_detail_thr.InvalidNode_SC = NULL;
    }
    if (NULL != test_data.mcap_detail_thr.RawDataTest_Min) {
        fts_free(test_data.mcap_detail_thr.RawDataTest_Min);
    }
    if (NULL != test_data.mcap_detail_thr.RawDataTest_Max) {
        fts_free(test_data.mcap_detail_thr.RawDataTest_Max);
    }
    if (NULL != test_data.mcap_detail_thr.RawDataTest_Low_Min) {
        fts_free(test_data.mcap_detail_thr.RawDataTest_Low_Min);
    }
    if (NULL != test_data.mcap_detail_thr.RawDataTest_Low_Max) {
        fts_free(test_data.mcap_detail_thr.RawDataTest_Low_Max);
    }
    if (NULL != test_data.mcap_detail_thr.RawDataTest_High_Min) {
        fts_free(test_data.mcap_detail_thr.RawDataTest_High_Min);
    }
    if (NULL != test_data.mcap_detail_thr.RawDataTest_High_Max) {
        fts_free(test_data.mcap_detail_thr.RawDataTest_High_Max);
    }
    if (NULL != test_data.mcap_detail_thr.RxLinearityTest_Max) {
        fts_free(test_data.mcap_detail_thr.RxLinearityTest_Max);
    }
    if (NULL != test_data.mcap_detail_thr.TxLinearityTest_Max) {
        fts_free(test_data.mcap_detail_thr.TxLinearityTest_Max);
    }
    if (NULL != test_data.mcap_detail_thr.SCapRawDataTest_ON_Max) {
        fts_free(test_data.mcap_detail_thr.SCapRawDataTest_ON_Max);
    }
    if (NULL != test_data.mcap_detail_thr.SCapRawDataTest_ON_Min) {
        fts_free(test_data.mcap_detail_thr.SCapRawDataTest_ON_Min);
    }
    if (NULL != test_data.mcap_detail_thr.SCapRawDataTest_OFF_Max) {
        fts_free(test_data.mcap_detail_thr.SCapRawDataTest_OFF_Max);
    }
    if (NULL != test_data.mcap_detail_thr.SCapRawDataTest_OFF_Min) {
        fts_free(test_data.mcap_detail_thr.SCapRawDataTest_OFF_Min);
    }
    if (NULL != test_data.mcap_detail_thr.SCapCbTest_ON_Max) {
        fts_free(test_data.mcap_detail_thr.SCapCbTest_ON_Max);
    }
    if (NULL != test_data.mcap_detail_thr.SCapCbTest_ON_Min) {
        fts_free(test_data.mcap_detail_thr.SCapCbTest_ON_Min);
    }
    if (NULL != test_data.mcap_detail_thr.SCapCbTest_OFF_Max) {
        fts_free(test_data.mcap_detail_thr.SCapCbTest_OFF_Max);
    }
    if (NULL != test_data.mcap_detail_thr.SCapCbTest_OFF_Min) {
        fts_free(test_data.mcap_detail_thr.SCapCbTest_OFF_Min);
    }
    if (NULL != test_data.mcap_detail_thr.NoistTest_Coefficient) {
        fts_free(test_data.mcap_detail_thr.NoistTest_Coefficient);
        test_data.mcap_detail_thr.NoistTest_Coefficient = NULL;
    }
    if (NULL != test_data.mcap_detail_thr.PanelDifferTest_Max) {
        fts_free(test_data.mcap_detail_thr.PanelDifferTest_Max);
        test_data.mcap_detail_thr.PanelDifferTest_Max = NULL;
    }
    if (NULL != test_data.mcap_detail_thr.PanelDifferTest_Min) {
        fts_free(test_data.mcap_detail_thr.PanelDifferTest_Min);
        test_data.mcap_detail_thr.PanelDifferTest_Min = NULL;
    }
    if (NULL !=  test_data.incell_detail_thr.InvalidNode) {
        fts_free( test_data.incell_detail_thr.InvalidNode);
        test_data.incell_detail_thr.InvalidNode = NULL;
    }
    if (NULL !=   test_data.incell_detail_thr.RawDataTest_Min) {
        fts_free(  test_data.incell_detail_thr.RawDataTest_Min);
        test_data.incell_detail_thr.RawDataTest_Min = NULL;
    }
    if (NULL !=   test_data.incell_detail_thr.RawDataTest_Max) {
        fts_free(  test_data.incell_detail_thr.RawDataTest_Max);
        test_data.incell_detail_thr.RawDataTest_Max = NULL;
    }
    if (NULL !=   test_data.incell_detail_thr.CBTest_Min) {
        fts_free(  test_data.incell_detail_thr.CBTest_Min);
        test_data.incell_detail_thr.CBTest_Min = NULL;
    }
    if (NULL !=   test_data.incell_detail_thr.CBTest_Max) {
        fts_free(  test_data.incell_detail_thr.CBTest_Max);
        test_data.incell_detail_thr.CBTest_Max = NULL;
    }
}

void OnInit_MCap_DetailThreshold(char *strIniFile)
{

    FTS_TEST_FUNC_ENTER();

    OnInit_InvalidNode(strIniFile);
    OnInit_DThreshold_RawDataTest(strIniFile);
    OnInit_DThreshold_SCapRawDataTest(strIniFile);
    OnInit_DThreshold_SCapCbTest(strIniFile);

    OnInit_DThreshold_ForceTouch_SCapRawDataTest(strIniFile);
    OnInit_DThreshold_ForceTouch_SCapCbTest(strIniFile);

    OnInit_DThreshold_RxLinearityTest(strIniFile);
    OnInit_DThreshold_TxLinearityTest(strIniFile);

    OnInit_DThreshold_PanelDifferTest(strIniFile);

    FTS_TEST_FUNC_EXIT();
}
void OnInit_InvalidNode(char *strIniFile)
{

    char str[MAX_PATH] = {0}, strTemp[MAX_PATH] = {0};
    int i = 0, j = 0;
    FTS_TEST_FUNC_ENTER();

    for (i = 0; i < TX_NUM_MAX; i++) {
        for (j = 0; j < RX_NUM_MAX; j++) {
            sprintf(strTemp, "InvalidNode[%d][%d]", (i + 1), (j + 1));

            GetPrivateProfileString("INVALID_NODE", strTemp, "1", str, strIniFile);
            if (fts_atoi(str) == 0) {
                test_data.mcap_detail_thr.InvalidNode[i][j] = 0;
                test_data.incell_detail_thr.InvalidNode[i][j] = 0;
                FTS_TEST_DBG("node (%d, %d) \n", (i + 1),  (j + 1));

            } else if ( fts_atoi( str ) == 2 ) {
                test_data.mcap_detail_thr.InvalidNode[i][j] = 2;
                test_data.incell_detail_thr.InvalidNode[i][j] = 2;
            } else {
                test_data.mcap_detail_thr.InvalidNode[i][j] = 1;
                test_data.incell_detail_thr.InvalidNode[i][j] = 1;
            }
        }
    }

    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            sprintf(strTemp, "InvalidNodeS[%d][%d]", (i + 1), (j + 1));
            GetPrivateProfileString("INVALID_NODES", strTemp, "1", str, strIniFile);
            if (fts_atoi(str) == 0) {
                test_data.mcap_detail_thr.InvalidNode_SC[i][j] = 0;
            } else if ( fts_atoi( str ) == 2 ) {
                test_data.mcap_detail_thr.InvalidNode_SC[i][j] = 2;
            } else
                test_data.mcap_detail_thr.InvalidNode_SC[i][j] = 1;
        }

    }

    FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_RawDataTest(char *strIniFile)
{
    char str[128];
    char strTemp[MAX_PATH];
    char strValue[MAX_PATH];
    int MaxValue, MinValue;
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();

    if ( (test_data.screen_param.selected_ic >> 4 == IC_FT8716 >> 4) || (test_data.screen_param.selected_ic >> 4 == IC_FT8736 >> 4) || (test_data.screen_param.selected_ic >> 4 == IC_FTE716 >> 4)) {
        return;
    }

    ////////////////////////////RawData Test
    GetPrivateProfileString( "Basic_Threshold", "RawDataTest_Max", "10000", str, strIniFile);
    MaxValue = fts_atoi(str);

    //FTS_TEST_DBG("MaxValue = %d  ",  MaxValue);

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.RawDataTest_Max[i][j] = MaxValue;
            test_data.incell_detail_thr.RawDataTest_Max[i][j] = MaxValue;

        }
    }

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        sprintf(str, "RawData_Max_Tx%d", (i + 1));
        //FTS_TEST_DBG("%s ",  str);
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "111", strTemp, strIniFile);
        //FTS_TEST_DBG("GetPrivateProfileString = %d ",  dividerPos);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.RawDataTest_Max[i][k] = (short)(fts_atoi(str_tmp));
                test_data.incell_detail_thr.RawDataTest_Max[i][k] = (short)(fts_atoi(str_tmp));
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

    GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "7000", str, strIniFile);
    MinValue = fts_atoi(str);

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.RawDataTest_Min[i][j] = MinValue;
            test_data.incell_detail_thr.RawDataTest_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        sprintf(str, "RawData_Min_Tx%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.RawDataTest_Min[i][k] = (short)(fts_atoi(str_tmp));
                test_data.incell_detail_thr.RawDataTest_Min[i][k] = (short)(fts_atoi(str_tmp));
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

    //RawData Test Low
    GetPrivateProfileString( "Basic_Threshold", "RawDataTest_Low_Max", "15000", str, strIniFile);
    MaxValue = fts_atoi(str);

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.RawDataTest_Low_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        sprintf(str, "RawData_Max_Low_Tx%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.RawDataTest_Low_Max[i][k] = (short)(fts_atoi(str_tmp));
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

    GetPrivateProfileString("Basic_Threshold", "RawDataTest_Low_Min", "3000", str, strIniFile);
    MinValue = fts_atoi(str);

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.RawDataTest_Low_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        sprintf(str, "RawData_Min_Low_Tx%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.RawDataTest_Low_Min[i][k] = (short)(fts_atoi(str_tmp));
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

    //RawData Test High
    GetPrivateProfileString( "Basic_Threshold", "RawDataTest_High_Max", "15000", str, strIniFile);
    MaxValue = fts_atoi(str);

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.RawDataTest_High_Max[i][j] = MaxValue;
        }
    }
    GetPrivateProfileString("Basic_Threshold", "RawDataTest_High_Min", "3000", str, strIniFile);
    MinValue = fts_atoi(str);

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.RawDataTest_High_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        sprintf(str, "RawData_Max_High_Tx%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.RawDataTest_High_Max[i][k] = (short)(fts_atoi(str_tmp));
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


    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        sprintf(str, "RawData_Min_High_Tx%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.RawDataTest_High_Min[i][k] = (short)(fts_atoi(str_tmp));
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
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();

    //////////////////OFF
    GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Min", "150", str, strIniFile);
    MinValue = fts_atoi(str);
    GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_OFF_Max", "1000", str, strIniFile);
    MaxValue = fts_atoi(str);

    ///Max
    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.SCapRawDataTest_OFF_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < 2; i++) {
        sprintf(str, "ScapRawData_OFF_Max_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.SCapRawDataTest_OFF_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    ////Min
    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.SCapRawDataTest_OFF_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < 2; i++) {
        sprintf(str, "ScapRawData_OFF_Min_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.SCapRawDataTest_OFF_Min[i][k] = (short)(fts_atoi(str_tmp));
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

    //////////////////ON
    GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Min", "150", str, strIniFile);
    MinValue = fts_atoi(str);
    GetPrivateProfileString("Basic_Threshold", "SCapRawDataTest_ON_Max", "1000", str, strIniFile);
    MaxValue = fts_atoi(str);

    ///Max
    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.SCapRawDataTest_ON_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < 2; i++) {
        sprintf(str, "ScapRawData_ON_Max_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.SCapRawDataTest_ON_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    ////Min
    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.SCapRawDataTest_ON_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < 2; i++) {
        sprintf(str, "ScapRawData_ON_Min_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.SCapRawDataTest_ON_Min[i][k] = (short)(fts_atoi(str_tmp));
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
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();

    //////////////////ON
    GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Min", "0", str, strIniFile);
    MinValue = fts_atoi(str);
    GetPrivateProfileString("Basic_Threshold", "SCapCbTest_ON_Max", "240", str, strIniFile);
    MaxValue = fts_atoi(str);
    //////读取阈值，若无特殊设置，则以Basic_Threshold替代

    ///Max
    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.SCapCbTest_ON_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < 2; i++) {
        sprintf(str, "ScapCB_ON_Max_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.SCapCbTest_ON_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    ////Min
    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.SCapCbTest_ON_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < 2; i++) {
        sprintf(str, "ScapCB_ON_Min_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.SCapCbTest_ON_Min[i][k] = (short)(fts_atoi(str_tmp));
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

    //////////////////OFF
    GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Min", "0", str, strIniFile);
    MinValue = fts_atoi(str);
    GetPrivateProfileString("Basic_Threshold", "SCapCbTest_OFF_Max", "240", str, strIniFile);
    MaxValue = fts_atoi(str);
    ///Max
    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.SCapCbTest_OFF_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < 2; i++) {
        sprintf(str, "ScapCB_OFF_Max_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.SCapCbTest_OFF_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    ////Min
    for (i = 0; i < 2; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.SCapCbTest_OFF_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < 2; i++) {
        sprintf(str, "ScapCB_OFF_Min_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.SCapCbTest_OFF_Min[i][k] = (short)(fts_atoi(str_tmp));
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
    int  k = 0, i = 0, j = 0;
    char str_tmp[128];

    FTS_TEST_FUNC_ENTER();
    ////////////////////////////Panel_Differ Test
    GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Max", "1000", str, strIniFile);
    MaxValue = fts_atoi(str);
    for ( i = 0; i < TX_NUM_MAX; i++) {
        for ( j = 0; j < RX_NUM_MAX; j++) {
            test_data.mcap_detail_thr.PanelDifferTest_Max[i][j] = MaxValue;
        }
    }

    for ( i = 0; i < TX_NUM_MAX; i++) {
        sprintf(str, "Panel_Differ_Max_Tx%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.PanelDifferTest_Max[i][k] = (short)(fts_atoi(str_tmp));
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


    GetPrivateProfileString("Basic_Threshold", "PanelDifferTest_Min", "150", str, strIniFile);
    MinValue = fts_atoi(str);
    for ( i = 0; i < TX_NUM_MAX; i++) {
        for ( j = 0; j < RX_NUM_MAX; j++) {
            test_data.mcap_detail_thr.PanelDifferTest_Min[i][j] = MinValue;
        }
    }

    for ( i = 0; i < TX_NUM_MAX; i++) {
        sprintf(str, "Panel_Differ_Min_Tx%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.PanelDifferTest_Min[i][k] = (short)(fts_atoi(str_tmp));
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
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();

    ////////////////////////////Rx_Linearity Test
    GetPrivateProfileString( "Basic_Threshold", "RxLinearityTest_Max", "50", str, strIniFile);
    MaxValue = fts_atoi(str);

    //FTS_TEST_DBG("MaxValue = %d  ",  MaxValue);

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.RxLinearityTest_Max[i][j] = MaxValue;
        }
    }

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        sprintf(str, "Rx_Linearity_Max_Tx%d", (i + 1));
        //FTS_TEST_DBG("%s ",  str);
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "111", strTemp, strIniFile);
        //FTS_TEST_DBG("GetPrivateProfileString = %d ",  dividerPos);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.RxLinearityTest_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();

    ////////////////////////////Tx_Linearity Test
    GetPrivateProfileString( "Basic_Threshold", "TxLinearityTest_Max", "50", str, strIniFile);
    MaxValue = fts_atoi(str);

    //FTS_TEST_DBG("MaxValue = %d  ",  MaxValue);

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.TxLinearityTest_Max[i][j] = MaxValue;
        }
    }

    for (i = 0; i < test_data.screen_param.used_max_tx_num; i++) {
        sprintf(str, "Tx_Linearity_Max_Tx%d", (i + 1));
        //FTS_TEST_DBG("%s ",  str);
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "111", strTemp, strIniFile);
        //FTS_TEST_DBG("GetPrivateProfileString = %d ",  dividerPos);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.TxLinearityTest_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();

    //////////////////OFF
    GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_OFF_Min", "150", str, strIniFile);
    MinValue = fts_atoi(str);
    GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_OFF_Max", "1000", str, strIniFile);
    MaxValue = fts_atoi(str);

    ///Max
    for (i = 0; i < 1; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.ForceTouch_SCapRawDataTest_OFF_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < 1; i++) {
        sprintf(str, "ForceTouch_ScapRawData_OFF_Max_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.ForceTouch_SCapRawDataTest_OFF_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    ////Min
    for (i = 0; i < 1; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.ForceTouch_SCapRawDataTest_OFF_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < 1; i++) {
        sprintf(str, "ForceTouch_ScapRawData_OFF_Min_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.ForceTouch_SCapRawDataTest_OFF_Min[i][k] = (short)(fts_atoi(str_tmp));
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

    //////////////////ON
    GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_ON_Min", "150", str, strIniFile);
    MinValue = fts_atoi(str);
    GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapRawDataTest_ON_Max", "1000", str, strIniFile);
    MaxValue = fts_atoi(str);

    //FTS_TEST_DBG("%d:%d\r", MinValue, MaxValue);
    //////读取阈值，若无特殊设置，则以Basic_Threshold替代

    ///Max
    for (i = 0; i < 1; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.ForceTouch_SCapRawDataTest_ON_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < 1; i++) {
        sprintf(str, "ForceTouch_ScapRawData_ON_Max_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp); //FTS_TEST_DBG("%s:%s\r", str, strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.ForceTouch_SCapRawDataTest_ON_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    ////Min
    for (i = 0; i < 1; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.ForceTouch_SCapRawDataTest_ON_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < 1; i++) {
        sprintf(str, "ForceTouch_ScapRawData_ON_Min_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp); //FTS_TEST_DBG("%s:%s\r", str, strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.ForceTouch_SCapRawDataTest_ON_Min[i][k] = (short)(fts_atoi(str_tmp));
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
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();

    //////////////////ON
    GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_ON_Min", "0", str, strIniFile);
    MinValue = fts_atoi(str);
    GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_ON_Max", "240", str, strIniFile);
    MaxValue = fts_atoi(str);

    //FTS_TEST_DBG("%d:%d\r", MinValue, MaxValue);
    //////读取阈值，若无特殊设置，则以Basic_Threshold替代

    ///Max
    for (i = 0; i < 1; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.ForceTouch_SCapCbTest_ON_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < 1; i++) {
        sprintf(str, "ForceTouch_ScapCB_ON_Max_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp); //FTS_TEST_DBG("%s:%s\r", str, strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.ForceTouch_SCapCbTest_ON_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    ////Min
    for (i = 0; i < 1; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.ForceTouch_SCapCbTest_ON_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < 1; i++) {
        sprintf(str, "ForceTouch_ScapCB_ON_Min_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp); //FTS_TEST_DBG("%s:%s\r", str, strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        FTS_TEST_DBG("%s\r", strTemp);
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.ForceTouch_SCapCbTest_ON_Min[i][k] = (short)(fts_atoi(str_tmp));
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

    //////////////////OFF
    GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_OFF_Min", "0", str, strIniFile);
    MinValue = fts_atoi(str);
    GetPrivateProfileString("Basic_Threshold", "ForceTouch_SCapCbTest_OFF_Max", "240", str, strIniFile);
    MaxValue = fts_atoi(str);
    ///Max
    for (i = 0; i < 1; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.ForceTouch_SCapCbTest_OFF_Max[i][j] = MaxValue;
        }
    }
    for (i = 0; i < 1; i++) {
        sprintf(str, "ForceTouch_ScapCB_OFF_Max_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.ForceTouch_SCapCbTest_OFF_Max[i][k] = (short)(fts_atoi(str_tmp));
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
    ////Min
    for (i = 0; i < 1; i++) {
        for (j = 0; j < test_data.screen_param.used_max_rx_num; j++) {
            test_data.mcap_detail_thr.ForceTouch_SCapCbTest_OFF_Min[i][j] = MinValue;
        }
    }
    for (i = 0; i < 1; i++) {
        sprintf(str, "ForceTouch_ScapCB_OFF_Min_%d", (i + 1));
        dividerPos = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0x00, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.mcap_detail_thr.ForceTouch_SCapCbTest_OFF_Min[i][k] = (short)(fts_atoi(str_tmp));
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

void OnInit_Incell_DetailThreshold(char *strIniFile)
{

    FTS_TEST_FUNC_ENTER();

    OnInit_InvalidNode(strIniFile);
    OnInit_DThreshold_RawDataTest(strIniFile);
    OnInit_DThreshold_CBTest(strIniFile);
    OnInit_DThreshold_AllButtonCBTest(strIniFile);
    OnThreshold_VkAndVaRawDataSeparateTest(strIniFile);

    FTS_TEST_FUNC_EXIT();
}

void OnInit_DThreshold_CBTest(char *strIniFile)
{
    char str[128], strTemp[MAX_PATH], strValue[MAX_PATH];
    int MaxValue, MinValue, MaxValue_Vkey, MinValue_Vkey;
    int ChannelNumTest_ChannelXNum, ChannelNumTest_ChannelYNum;
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();
    if (test_data.screen_param.selected_ic >> 4 == IC_FT8606 >> 4) {
        return;
    }

    GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str, strIniFile);
    MaxValue = fts_atoi(str);

    GetPrivateProfileString("Basic_Threshold", "CBTest_Max_Vkey", "100", str, strIniFile);
    MaxValue_Vkey = fts_atoi(str);

    GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX", "15", str, strIniFile);
    ChannelNumTest_ChannelXNum = fts_atoi(str);

    GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY", "24", str, strIniFile);
    ChannelNumTest_ChannelYNum = fts_atoi(str);

    for ( i = 0; i < TX_NUM_MAX; i++) {
        for ( j = 0; j < RX_NUM_MAX; j++) {
            test_data.incell_detail_thr.CBTest_Max[i][j] = MaxValue;
        }

        if (i == ChannelNumTest_ChannelXNum) {
            for ( j = 0; j < RX_NUM_MAX; j++) {
                test_data.incell_detail_thr.CBTest_Max[i][j] = MaxValue_Vkey;
            }
        }
    }

    for ( i = 0; i < TX_NUM_MAX; i++) {
        sprintf(str, "CB_Max_Tx%d", (i + 1));
        //FTS_TEST_DBG("%s ",  str);
        dividerPos  = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        // FTS_TEST_DBG("i = %d, dividerPos = %d \n", i+1, dividerPos);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.incell_detail_thr.CBTest_Max[i][k] = (short)(fts_atoi(str_tmp));
                //FTS_TEST_DBG("node (%d, %d) CB_Max_Tx%d = %d \n", (i+1), (k+1), (i+1),test_data.incell_detail_thr.CBTest_Max[i][k]);
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



    GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, strIniFile);
    MinValue = fts_atoi(str);

    GetPrivateProfileString("Basic_Threshold", "CBTest_Min_Vkey", "3", str, strIniFile);
    MinValue_Vkey = fts_atoi(str);

    for (i = 0; i < TX_NUM_MAX; i++) {
        for ( j = 0; j < RX_NUM_MAX; j++) {
            test_data.incell_detail_thr.CBTest_Min[i][j] = MinValue;
        }

        if (i == ChannelNumTest_ChannelXNum) {
            for ( j = 0; j < RX_NUM_MAX; j++) {
                test_data.incell_detail_thr.CBTest_Min[i][j] = MinValue_Vkey;
            }
        }
    }

    for ( i = 0; i < TX_NUM_MAX; i++) {
        sprintf(str, "CB_Min_Tx%d", (i + 1));
        dividerPos  =  GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        //FTS_TEST_DBG("i = %d, dividerPos = %d \n", i+1, dividerPos);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.incell_detail_thr.CBTest_Min[i][k] = (short)(fts_atoi(str_tmp));
                // FTS_TEST_DBG("node (%d, %d) CB_Min_Tx%d = %d \n", (i+1), (k+1), (i+1),test_data.incell_detail_thr.CBTest_Min[i][k]);
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
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;

    FTS_TEST_FUNC_ENTER();

    if (test_data.screen_param.selected_ic >> 4 != IC_FT8606 >> 4) {
        return;
    }

    GetPrivateProfileString("Basic_Threshold", "CBTest_Max", "100", str, strIniFile);
    MaxValue = fts_atoi(str);


    for ( i = 0; i < TX_NUM_MAX; i++) {
        for ( j = 0; j < RX_NUM_MAX; j++) {
            test_data.incell_detail_thr.CBTest_Max[i][j] = MaxValue;
        }
    }

    for ( i = 0; i < TX_NUM_MAX; i++) {
        sprintf(str, "CB_Max_Tx%d", (i + 1));
        //FTS_TEST_DBG("%s ",  str);
        dividerPos  = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        // FTS_TEST_DBG("i = %d, dividerPos = %d \n", i+1, dividerPos);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.incell_detail_thr.CBTest_Max[i][k] = (short)(fts_atoi(str_tmp));
                //FTS_TEST_DBG("node (%d, %d) value = %d \n", (i+1), (k+1), test_data.incell_detail_thr.CBTest_Max[i][k]);
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



    GetPrivateProfileString("Basic_Threshold", "CBTest_Min", "3", str, strIniFile);
    MinValue = fts_atoi(str);

    for (i = 0; i < TX_NUM_MAX; i++) {
        for ( j = 0; j < RX_NUM_MAX; j++) {
            test_data.incell_detail_thr.CBTest_Min[i][j] = MinValue;
        }
    }

    for ( i = 0; i < TX_NUM_MAX; i++) {
        sprintf(str, "CB_Min_Tx%d", (i + 1));
        dividerPos  =  GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        //FTS_TEST_DBG("i = %d, dividerPos = %d \n", i+1, dividerPos);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.incell_detail_thr.CBTest_Min[i][k] = (short)(fts_atoi(str_tmp));
                // FTS_TEST_DBG("node (%d, %d) value = %d \n", (i+1), (k+1), test_data.incell_detail_thr.CBTest_Min[i][k]);
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
    int   dividerPos = 0;
    char str_tmp[128];
    int index = 0;
    int  k = 0, i = 0, j = 0;


    FTS_TEST_FUNC_ENTER();

    if ( (test_data.screen_param.selected_ic >> 4 != IC_FT8716 >> 4) && (test_data.screen_param.selected_ic >> 4 != IC_FT8736 >> 4) && (test_data.screen_param.selected_ic >> 4 != IC_FTE716 >> 4)) {
        return;
    }

    GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max", "11000", str, strIniFile);
    MaxValue = fts_atoi(str);

    GetPrivateProfileString("Basic_Threshold", "RawDataTest_Max_VKey", "11000", str, strIniFile);
    MaxValue_Vkey = fts_atoi(str);

    GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelX", "15", str, strIniFile);
    ChannelNumTest_ChannelXNum = fts_atoi(str);

    GetPrivateProfileString("Basic_Threshold", "ChannelNumTest_ChannelY", "24", str, strIniFile);
    ChannelNumTest_ChannelYNum = fts_atoi(str);

    for ( i = 0; i < TX_NUM_MAX; i++) {
        for ( j = 0; j < RX_NUM_MAX; j++) {
            test_data.incell_detail_thr.RawDataTest_Max[i][j] = MaxValue;
        }

        if (i == ChannelNumTest_ChannelXNum) {
            for ( j = 0; j < RX_NUM_MAX; j++) {
                test_data.incell_detail_thr.RawDataTest_Max[i][j] = MaxValue_Vkey;
            }
        }
    }


    for ( i = 0; i < TX_NUM_MAX; i++) {
        sprintf(str, "RawData_Max_Tx%d", (i + 1));
        dividerPos  = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        //FTS_TEST_DBG("i = %d, dividerPos = %d \n", i+1, dividerPos);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.incell_detail_thr.RawDataTest_Max[i][k] = (short)(fts_atoi(str_tmp));
                //  FTS_TEST_DBG("node (%d, %d) value = %d \n", (i+1), (k+1), test_data.incell_detail_thr.RawDataTest_Max[i][k]);
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

    GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min", "5000", str, strIniFile);
    MinValue = fts_atoi(str);

    GetPrivateProfileString("Basic_Threshold", "RawDataTest_Min_VKey", "5000", str, strIniFile);
    MinValue_Vkey = fts_atoi(str);

    for ( i = 0; i < TX_NUM_MAX; i++) {
        for ( j = 0; j < RX_NUM_MAX; j++) {
            test_data.incell_detail_thr.RawDataTest_Min[i][j] = MinValue;
        }

        if (i == ChannelNumTest_ChannelXNum) {
            for ( j = 0; j < RX_NUM_MAX; j++) {
                test_data.incell_detail_thr.RawDataTest_Min[i][j] = MinValue_Vkey;
            }
        }
    }

    for ( i = 0; i < TX_NUM_MAX; i++) {
        sprintf(str, "RawData_Min_Tx%d", (i + 1));
        dividerPos  = GetPrivateProfileString( "SpecialSet", str, "NULL", strTemp, strIniFile);
        sprintf(strValue, "%s", strTemp);
        //FTS_TEST_DBG("i = %d, dividerPos = %d \n", i+1, dividerPos);
        if (0 == dividerPos) continue;
        index = 0;
        k = 0;
        memset(str_tmp, 0, sizeof(str_tmp));
        for (j = 0; j < dividerPos; j++) {
            if (',' == strValue[j]) {
                test_data.incell_detail_thr.RawDataTest_Min[i][k] = (short)(fts_atoi(str_tmp));
                //FTS_TEST_DBG("node (%d, %d) value = %d \n", (i+1), (k+1), test_data.incell_detail_thr.RawDataTest_Min[i][k]);
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


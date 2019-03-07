/******************** (C) COPYRIGHT 2017 Goodix ********************
* File Name          : test_param_init.h
* Author             : Zhitao Yang
* Version            : V1.0.0
* Date               : 12/29/2017
* Description        : initial test data
*******************************************************************************/

#ifndef TEST_PARAM_INIT_H
#define TEST_PARAM_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tp_dev_def.h"
#include "test_item_def.h"
#include "test_order.h"

/*************************************Public methods start********************************************/
/*init test item*/
extern s32 init_test_item(PST_TEST_PROC_DATA p_test_proc_data,
				PST_TEST_PARAM p_test_param);
extern s32 release_test_item(PST_TEST_PROC_DATA p_test_proc_data);

/*register callback function*/
extern s32 register_test_item_finished_func(PST_TEST_PROC_DATA
				p_test_proc_data);

/*init proc data*/
extern s32 init_test_proc_data(PST_TEST_PROC_DATA *pp_test_proc_data,
				PST_TEST_PARAM *pp_test_param,
				void *p_drv_dev);
extern s32 release_test_proc_data(PST_TEST_PROC_DATA *
				  pp_test_proc_data);

/*test process*/
extern s32 test_process(void *p_drv_dev);
extern int get_tp_rawdata(void *p_drv_dev, char *buf, int *buf_size);
extern int get_tp_testcfg(void *p_drv_dev, char *buf, int *buf_size);
/*************************************Public methods end********************************************/
#ifdef __cplusplus
}
#endif
#endif

/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name				: test_item_init.h
* Author				: Zhitao Yang
* Version				: V1.0.0
* Date					: 28/03/2018
* Description			: initial test item
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "test_item_init.h"

#include "tp_dev_control.h"
#include "tp_open_test.h"
#include "tp_short_test.h"
#include "tp_version_test.h"
#include "tp_additional_option.h"
#include "tp_test_item_mem.h"
#include "simple_mem_manager.h"
#include "save_test_data.h"

/*******************************************************************************
* Function Name	: free_item_param
* Description	:
* Input			: PST_TEST_ITEM p_test_item
* Output		: none
* Return		: void
*******************************************************************************/
extern void free_item_param(PST_TEST_ITEM p_test_item)
{
	if (p_test_item->p_sub_id_set != NULL) {
		p_test_item->p_sub_id_set->p_id_set = NULL;
	}
	p_test_item->p_sub_id_set = NULL;
	p_test_item->p_tp_dev = NULL;
	p_test_item->item_addr_start.item_addr_ptr = NULL;
	p_test_item->ptr_general_func = NULL;
	p_test_item->ptr_general_func_param = NULL;
	p_test_item->ptr_test_func = NULL;
	p_test_item->ptr_test_item_before_func = NULL;
	p_test_item->ptr_test_item_before_func_param = NULL;
	p_test_item->ptr_test_item_finished_func = NULL;
	p_test_item->ptr_test_item_finished_func_param = NULL;
	p_test_item->ptr_share_data = NULL;
	p_test_item->ptr_test_proc_data = NULL;
	if (p_test_item->ptr_special_callback_data != NULL) {
		free_mem_in_heap(p_test_item->ptr_special_callback_data);
		p_test_item->ptr_special_callback_data = NULL;
	}
}

/*-------------------------------init open test item---------------------------------------*/


/*******************************************************************************
* Function Name	: rawdata_test_before_func
* Description	: set some hardware and init rawdata test param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: ptr32 p_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 rawdata_test_before_func(ptr32 p_data, ptr32 p_param)
{
	s32 ret = 0;
	PST_TEST_ITEM p_test_item = NULL;
	PST_CUR_RAWDATA p_cur_data = NULL;
	PST_TEST_PARAM p_test_param = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_test_item->test_res.ptr_res = NULL;
	p_test_param = (PST_TEST_PARAM) p_param;
	/*init raw param*/
	p_test_item->param.ptr_param = p_test_param->p_raw_test_param;

	/* get rawdata test result buf*/
	ret = alloc_rawdata_test_item_res(p_test_item);
	if (ret == 0) {
		board_print_error("get raw test result error!\n");
		return 0;
	}
	p_cur_data =
		(PST_CUR_RAWDATA) p_test_item->cur_data.ptr_cur_data;
	board_print_debug("rawdata buf len :%d!\n",
			p_cur_data->data_len);
	return ret;
}

/*******************************************************************************
* Function Name	: release_rawdata_test_item
* Description	: release test param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 release_rawdata_test_item(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;
	p_test_item->param.ptr_param = NULL;
	/*release rawdata test res*/
	ret = free_rawdata_test_item_res(p_test_item);
	free_item_param(p_test_item);
	return ret;
}

/*-------------------------------init short test item---------------------------------------*/

/*******************************************************************************
* Function Name	: get_gt900_short_param
* Description	: gt900 short test res mem malloc
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
				: PST_TEST_PARAM p_test_param
* Output		: none
* Return		: s32(0:have not handled 1:handled)
*******************************************************************************/
static s32 get_gt900_short_param(IN_OUT PST_TEST_ITEM p_test_item,
				PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	p_test_item->param.ptr_param = p_test_param->p_short_param;
	return ret;
}

/*******************************************************************************
* Function Name	: get_phoenix_test_param
* Description	: phoenix short test res mem malloc
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:have not handled 1:handled)
*******************************************************************************/
static s32 get_phoenix_test_param(IN_OUT PST_TEST_ITEM p_test_item,
				  PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	u16 i = 0;
	u16 j = 0;
	u16 chk_sum = 0;
	PST_SHORT_PARAM p_short_param = NULL;
	PST_TP_DEV p_tp_dev = NULL;
	u16 param_addr = 0x8118;
	u16 len = 0x81FE - 0x8118;
	u8 *opt_param = NULL;

	p_short_param = p_test_param->p_short_param;
	p_tp_dev = p_test_item->p_tp_dev;
	p_short_param->short_opt.len = len + 5;
	if (alloc_mem_in_heap
		((void **)&p_short_param->short_opt.opt.opt_ptr,
		p_short_param->short_opt.len) == 0) {
		board_print_error(
			"short testopt param malloc momory error!\n");
		return 0;
	}
	memset(p_short_param->short_opt.opt.opt_ptr, 0, len + 3);
	opt_param = p_short_param->short_opt.opt.opt_ptr;
	/*param len*/
	opt_param[i++] = len + 2;
	/*param addr*/
	opt_param[i++] = (param_addr & 0xff);
	opt_param[i++] = (param_addr >> 8) & 0xff;
	/*sen/drv line*/
	for (j = 0; j < p_tp_dev->max_sen_num_in_die; j++) {
		if (j > p_tp_dev->max_sen_num) {
			opt_param[i++] = 0xFF;
		} else {
			opt_param[i++] = p_tp_dev->sen_map[j];
		}
	}
	for (j = 0; j < p_tp_dev->max_drv_num_in_die; j++) {
		if (j > p_tp_dev->max_drv_num) {
			opt_param[i++] = 0xFF;
		} else {
			opt_param[i++] = p_tp_dev->drv_map[j];
		}
	}
	/*  4 bytes reseverd -> 2 bytes reseved add 2 bytes for driver,so driver max number is 46*/
	opt_param[i++] = 0x00;
	opt_param[i++] = 0x00;

	/*Short Check Max num ≤100*/
	opt_param[i++] = 100;
	/*
	ShortCheck
	Mode Sensor Line
	Rx的线序选择：0~3共4种模式；
	PIN脚号：S0~121；
	0：S0~75；1：S46~121；2：S22~97；
	3：S0~37&S84~121。
	根据产品配置自动获取，并上传到此寄存器。
	*/
	/*p_test_param->mode_sen_line = (p_test_param->test_cfg[0x80AC-p_tp_dev->cfg_start_addr]>>5) & 0x03;*/
	opt_param[i++] = p_test_param->mode_sen_line;

	/*Tx&Tx short resistor threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		drv_drv_resistor_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		drv_drv_resistor_threshold >> 8) & 0xFF);
	/*Tx&Rx short resistor threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		drv_sen_resistor_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		drv_sen_resistor_threshold >> 8) & 0xFF);
	/*Rx&Rx short resistor threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		sen_sen_resistor_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		sen_sen_resistor_threshold >> 8) & 0xFF);
	/*adc read delay*/
	opt_param[i++] =
		((p_short_param->short_threshold.adc_read_delay) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		adc_read_delay >> 8) & 0xFF);
	/*Tx&GND short resistor threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		drv_gnd_vdd_resistor_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		drv_gnd_vdd_resistor_threshold >> 8) & 0xFF);
	/*Tx&VDD short resistor threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		drv_gnd_vdd_resistor_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		drv_gnd_vdd_resistor_threshold >> 8) & 0xFF);
	/*Rx&GND short resistor threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		sen_gnd_vdd_resistor_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		sen_gnd_vdd_resistor_threshold >> 8) & 0xFF);
	/*Rx&VDD short resistor threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		sen_gnd_vdd_resistor_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		sen_gnd_vdd_resistor_threshold >> 8) & 0xFF);
	/*第一段ADC数据区间起始时间，单位：0.5us；建议>=200。(暂未使用)*/
	opt_param[i++] = 0x00;
	opt_param[i++] = 0x00;
	/*第二段ADC数据区间起始时间，单位：0.5us；建议>=1000。(暂未使用)*/
	opt_param[i++] = 0x00;
	opt_param[i++] = 0x00;
	/*ADC Samping Time，单位：Hz，建议：<=1700Hz（采样时间约588us），不能低于第二段ADC区间起始时间。(暂未使用)*/
	opt_param[i++] = 0x00;
	opt_param[i++] = 0x00;
	/*Tx&Tx Rx&Rx Tx&Rx short adc threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		gt_short_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		gt_short_threshold >> 8) & 0xFF);
	/*diffcode threshold*/
	opt_param[i++] =
		((p_short_param->short_threshold.
		diffcode_short_threshold) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.
		diffcode_short_threshold >> 8) & 0xFF);
	/*Tx与Tx短路换算系数，范围40~500*/
	opt_param[i++] =
		((p_short_param->short_threshold.tx_tx_factor) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.tx_tx_factor >> 8) & 0xFF);
	/*Tx与Rx短路换算系数，范围40~500*/
	opt_param[i++] =
		((p_short_param->short_threshold.tx_rx_factor) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.tx_rx_factor >> 8) & 0xFF);
	/*Rx与Rx短路换算系数，范围40~500*/
	opt_param[i++] =
		((p_short_param->short_threshold.rx_rx_factor) & 0xFF);
	opt_param[i++] =
		((p_short_param->short_threshold.rx_rx_factor >> 8) & 0xFF);
	/*reserved*/
	opt_param[i++] = 0x00;
	opt_param[i++] = 0x00;

	for (j = 3; j < len + 3; j += 2) {
		chk_sum +=
			((u16) (opt_param[j + 1] << 8) + opt_param[j]);
	}
	chk_sum = 0 - chk_sum;
	opt_param[len + 3] = ((chk_sum) & 0xFF);
	opt_param[len + 4] = ((chk_sum >> 8) & 0xFF);
	opt_param[0] = len + 2;
	p_short_param->short_opt.len = len + 5;
	board_print_debug("short test opt param len:%d\n", len + 2);

	p_test_item->param.ptr_param = p_short_param;

	return ret;
}

/*******************************************************************************
* Function Name		: get_short_param
* Description		: get short param
* Input				: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output			: none
* Return			: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 get_short_param(IN_OUT PST_TEST_ITEM p_test_item,
				PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	switch (p_test_item->p_tp_dev->chip_type) {
	case TP_NANJING:
	case TP_PHOENIX:
		{
			ret =
				get_phoenix_test_param(p_test_item,
						p_test_param);
		}
		break;
	default:
		{
			ret =
				get_gt900_short_param(p_test_item,
						p_test_param);
		}
		break;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: short_test_before_func
* Description	: set some hardware and init short param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: ptr32 p_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 short_test_before_func(ptr32 p_data, ptr32 p_param)
{
	s32 ret = 0;
	PST_TEST_ITEM p_test_item = NULL;
	PST_TEST_PARAM p_test_param = NULL;
	p_test_item = (PST_TEST_ITEM) p_data;
	p_test_item->test_res.ptr_res = NULL;
	p_test_param = (PST_TEST_PARAM) p_param;

	/*init short param*/
	ret = get_short_param(p_test_item, p_test_param);
	if (ret == 0) {
		board_print_error("get short param error!\n");
		return ret;
	}

	ret = alloc_short_test_item_res(p_test_item);
	if (ret == 0) {
		board_print_error("get short res buf error!\n");
		return ret;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: release_short_test_item
* Description	: release memory function
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:have not handled 1:handled)
*******************************************************************************/
extern s32 release_short_test_item(IN_OUT ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = NULL;
	PST_SHORT_PARAM p_short_param = NULL;
	p_test_item = (PST_TEST_ITEM) p_data;
	p_short_param = (PST_SHORT_PARAM) p_test_item->param.ptr_param;

	if (p_short_param != NULL) {
		p_short_param->short_bin.update_bin_addr.bin_ptr = NULL;
		if (p_short_param->short_opt.opt.opt_ptr != NULL) {
			free_mem_in_heap(p_short_param->short_opt.opt.
					 opt_ptr);
			p_short_param->short_opt.opt.opt_ptr = NULL;
		}
		p_short_param = NULL;
	}
	/*release short test result*/
	ret = free_short_test_item_res(p_test_item);
	free_item_param(p_test_item);
	return ret;
}

/*-------------------------------init version test item---------------------------------------*/

/*******************************************************************************
* Function Name	: version_test_before_func
* Description	: set some hardware and init version test param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
				: ptr32 p_param
* Output		: ptr32 p_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 version_test_before_func(ptr32 p_data, ptr32 p_param)
{
	s32 ret = 0;
	PST_TEST_ITEM p_test_item = NULL;
	PST_TEST_PARAM p_test_param = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_test_param = (PST_TEST_PARAM) p_param;
	p_test_item->test_res.ptr_res = NULL;
	/*init version param*/
	p_test_item->param.ptr_param =
		p_test_param->p_version_test_param;
	/* get version test result buf*/
	ret = alloc_version_test_item_res(p_test_item);
	if (ret == 0) {
		board_print_error("get version test result error!\n");
		return 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: version_test_finished_func
* Description	: release test param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 version_test_finished_func(ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = NULL;
	p_test_item = (PST_TEST_ITEM) p_data;
	p_test_item->param.ptr_param = NULL;
	/*release version test res*/
	ret = free_version_test_item_res(p_test_item);
	free_item_param(p_test_item);
	return ret;
}

/*-------------------------------init flash test item---------------------------------------*/

/*******************************************************************************
* Function Name	: flash_test_before_func
* Description	: set some hardware and init flash param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
				: ptr32 p_param
* Output		: ptr32 p_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 flash_test_before_func(ptr32 p_data, ptr32 p_param)
{
	s32 ret = 0;
	PST_TEST_ITEM p_test_item = NULL;
	PST_TEST_PARAM p_test_param = NULL;
	PST_FLASH_TEST_PARAM p_flash_param = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_test_param = (PST_TEST_PARAM) p_param;
	p_test_item->test_res.ptr_res = NULL;

	/*init falsh param*/
	p_flash_param = p_test_param->p_flash_test_param;
	p_flash_param->config_arr = p_test_param->test_cfg;
	p_flash_param->cfg_len = p_test_param->cfg_len;
	p_test_item->param.ptr_param = p_flash_param;

	/* get flash test result buf*/
	ret = alloc_flash_test_item_res(p_test_item);
	if (ret == 0) {
		board_print_error("get flash test result error!\n");
		return 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: release_flash_test_item
* Description	: release test param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 release_flash_test_item(ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = NULL;
	PST_FLASH_TEST_PARAM p_flash_param = NULL;
	p_test_item = (PST_TEST_ITEM) p_data;
	p_flash_param =
		(PST_FLASH_TEST_PARAM) p_test_item->param.ptr_param;
	if (p_test_item->param.ptr_param != NULL) {
		p_flash_param->config_arr = NULL;
		p_test_item->param.ptr_param = NULL;
	}
	/*release flash test res*/
	ret = free_flash_test_item_res(p_test_item);
	free_item_param(p_test_item);
	return ret;
}

/*-------------------------------init send config test item---------------------------------------*/

/*******************************************************************************
* Function Name	: send_cfg_test_before_func
* Description	: set some hardware and init send config param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
				: ptr32 p_param
* Output		: ptr32 p_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 send_cfg_test_before_func(ptr32 p_data, ptr32 p_param)
{
	s32 ret = 0;
	PST_TEST_ITEM p_test_item = NULL;
	PST_TEST_PARAM p_test_param = NULL;
	p_test_item = (PST_TEST_ITEM) p_data;
	p_test_item->test_res.ptr_res = NULL;

	p_test_param = (PST_TEST_PARAM) p_param;
	p_test_item->test_res.ptr_res = NULL;

	ret = get_send_cfg_param_mess(p_test_item, p_test_param);
	if (ret == 0) {
		board_print_error("get send config param error!\n");
		return 0;
	}

	/*get send config test result buf*/
	ret = alloc_chip_cfg_proc_item_res(p_test_item);
	if (ret == 0) {
		board_print_error
			("get send config test result error!\n");
		return 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_send_cfg_param_mess
* Description	: get send cfg param
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 get_send_cfg_param_mess(PST_TEST_ITEM p_test_item,
					PST_TEST_PARAM p_test_param)
{
	s32 ret = 0;
	PST_CHIP_CONFIG_PROC_PARAM p_send_cfg_param = NULL;

	ret =
		alloc_mem_in_heap((void **)&p_send_cfg_param,
			sizeof(ST_CHIP_CONFIG_PROC_PARAM));
	if (ret == 0) {
		board_print_error(
			"send config param alloc memory error!\n");
		return 0;
	}

	p_send_cfg_param->config = p_test_param->test_cfg;
	p_send_cfg_param->cfg_len = p_test_param->cfg_len;
	p_send_cfg_param->option = p_test_param->send_cfg_option;
	p_test_item->param.ptr_param = p_send_cfg_param;
	return ret;
}

/*******************************************************************************
* Function Name	: release_cfg_test_item
* Description	: release test param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 release_cfg_test_item(ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;

	/*release send config test param*/
	ret = release_send_cfg_param(p_test_item);
	/*release send config test res*/
	ret = free_chip_cfg_proc_item_res(p_test_item);
	free_item_param(p_test_item);
	return ret;
}

/*******************************************************************************
* Function Name	: release_send_cfg_param
* Description	: release send config param
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_send_cfg_param(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_CHIP_CONFIG_PROC_PARAM p_send_cfg_param = NULL;
	p_send_cfg_param =
		(PST_CHIP_CONFIG_PROC_PARAM) p_test_item->param.ptr_param;
	if (p_send_cfg_param != NULL) {
		p_send_cfg_param->config = NULL;
		ret =
			free_mem_in_heap_p((void **)&p_test_item->param.ptr_param);
		p_test_item->param.ptr_param = NULL;
	}
	return ret;
}

/*-------------------------------init config recover test item---------------------------------------*/

/*******************************************************************************
* Function Name	: cfg_recover_test_before_func
* Description	: set some hardware and init recover config param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: ptr32 p_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 cfg_recover_test_before_func(ptr32 p_data)
{
	s32 ret = 0;
	PST_RECOVER_CHIP_CFG_RES p_res = NULL;
	PST_TEST_ITEM p_test_item = NULL;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_test_item->test_res.ptr_res = NULL;

	ret = get_recover_cfg_param_mess(p_test_item);
	if (ret == 0) {
		board_print_error("get recover config param error!\n");
		return 0;
	}
	/* get cfg recover test result buf*/
	ret = alloc_recover_cfg_item_res(p_test_item);
	if (ret == 0) {
		board_print_error
			("get cfg recover test result error!\n");
		return 0;
	}
	p_res =
		(PST_RECOVER_CHIP_CFG_RES) p_test_item->test_res.ptr_res;
	return ret;
}

/*******************************************************************************
* Function Name	: get_recover_cfg_param_mess
* Description	: get recover cfg param
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 get_recover_cfg_param_mess(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_RECOVER_CHIP_CFG_PARAM p_recover_cfg_param = NULL;
	PST_TP_DEV p_tp_dev = p_test_item->p_tp_dev;

	ret =
		alloc_mem_in_heap((void **)&p_recover_cfg_param,
				sizeof(ST_RECOVER_CHIP_CFG_PARAM));
	if (ret == 0) {
		board_print_error
			("recovery config param alloc memory error!\n");
		return 0;
	}

	p_recover_cfg_param->config_arr = p_tp_dev->cfg;
	p_recover_cfg_param->cfg_len = p_tp_dev->cfg_len;
	p_test_item->param.ptr_param = p_recover_cfg_param;
	return ret;
}
/*******************************************************************************
* Function Name	: release_cfg_recover_test_item
* Description	: release test param
* Input			: ptr32 p_data(You must cast it to type PST_TEST_ITEM )
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 release_cfg_recover_test_item(ptr32 p_data)
{
	s32 ret = 1;
	PST_TEST_ITEM p_test_item = (PST_TEST_ITEM) p_data;

	/*release config recover test param*/
	ret = release_cfg_recover_param(p_test_item);
	/*release config recover test res*/
	ret = free_recover_cfg_item_res(p_test_item);
	free_item_param(p_test_item);
	return ret;
}

/*******************************************************************************
* Function Name	: release_cfg_recover_param
* Description	: release config recover param
* Input			: PST_TEST_ITEM p_test_item
* Output		: PST_TEST_ITEM p_test_item
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_cfg_recover_param(PST_TEST_ITEM p_test_item)
{
	s32 ret = 0;
	PST_RECOVER_CHIP_CFG_PARAM p_recover_cfg_param = NULL;
	p_recover_cfg_param =
		(PST_RECOVER_CHIP_CFG_PARAM) p_test_item->param.ptr_param;
	if (p_recover_cfg_param != NULL) {
		p_recover_cfg_param->config_arr = NULL;
		ret = free_mem_in_heap_p((void **)&p_test_item->param.ptr_param);
		p_test_item->param.ptr_param = NULL;
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

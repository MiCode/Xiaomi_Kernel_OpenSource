/******************** (C) COPYRIGHT 2017 Goodix ********************
* File Name          : test_param_init.h
* Author             : Zhitao Yang
* Version            : V1.0.0
* Date               : 12/29/2017
* Description        : initial test param
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "test_param_init.h"
#include "test_proc.h"
#include "simple_mem_manager.h"

#include "tp_dev_control.h"
#include "tp_open_test.h"
#include "tp_short_test.h"
#include "tp_version_test.h"
#include "tp_additional_option.h"

#include "test_item_init.h"
#include "save_test_data.h"
#include "generic_func.h"
#include "extra_tp_control.h"
#include "tp_item_permutation.h"
#include "board_opr_interface.h"
#include <linux/string.h>

#include "tp_normandy_control.h"

/*static s8 s_ic_name[10] = "";*/

/*******************************************************************************
* Function Name	: init_test_item
* Description	: initial test item
* Input			: PST_TEST_PROC_DATA p_test_proc_data
				: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PROC_DATA p_test_proc_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 init_test_item(PST_TEST_PROC_DATA p_test_proc_data,
			PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	u16 i = 0;
	PST_TEST_ITEM *pp_test_item_arr = NULL;
	u16 item_num = 0;

	pp_test_item_arr = p_test_proc_data->pp_test_item_arr;
	item_num = p_test_proc_data->test_item_num;

	for (i = 0; i < item_num; i++) {
		switch (pp_test_item_arr[i]->test_item_id) {
			/*----------------------tp open test-----------------------*/
		case TP_RAWDATA_TEST_ITEMS_SET_ID:
			{
				ret = rawdata_test_before_func((p_test_proc_data->pp_test_item_arr)[i], p_test_param);
			} break;

			/*----------------------hardware test-----------------------*/
		case TP_SHORT_TEST_ITEM_ID:
			{
				ret = short_test_before_func((p_test_proc_data->pp_test_item_arr)[i], p_test_param);
			}
			break;

		case TP_VERSION_TEST_ITEM_ID:
			{
				ret = version_test_before_func((p_test_proc_data->pp_test_item_arr)[i], p_test_param);
			}
			break;

			/*----------------------other test-----------------------*/
		case TP_CHIP_CFG_PROC_ITEM_ID:
			{
				ret = send_cfg_test_before_func((p_test_proc_data->pp_test_item_arr)[i], p_test_param);
			}
			break;

		case TP_RECOVER_CFG_ITEM_ID:
			{
				ret = cfg_recover_test_before_func((p_test_proc_data->pp_test_item_arr)[i]);
			}
			break;

		case TP_FLASH_TEST_ITEM_ID:
			{
				ret = flash_test_before_func((p_test_proc_data->pp_test_item_arr)[i], p_test_param);
			}
			break;

		default:
			{
				;
			}
			break;
		}
	}

	return ret;

}

/*******************************************************************************
* Function Name	: register_test_item_finished_func
* Description	: print test result
* Input			: PST_TEST_PROC_DATA* pp_test_proc_data
* Output		: PST_TEST_PROC_DATA* pp_test_proc_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 register_test_item_finished_func(PST_TEST_PROC_DATA
						p_test_proc_data)
{
	s32 ret = 1;
	u16 i = 0;
	PST_TEST_ITEM *pp_test_item_arr = NULL;
	u16 item_num = 0;
	pp_test_item_arr = p_test_proc_data->pp_test_item_arr;
	item_num = p_test_proc_data->test_item_num;
	if (pp_test_item_arr == NULL) {
		return ret;
	}

	for (i = 0; i < item_num; i++) {
		if ((p_test_proc_data->pp_test_item_arr)[i] != NULL) {
			switch (pp_test_item_arr[i]->test_item_id) {
				/*----------------------tp open test-----------------------*/
			case TP_RAWDATA_TEST_ITEMS_SET_ID:
				{
					pp_test_item_arr[i]->
						ptr_test_item_finished_func
						= print_rawdata_test_res;
					pp_test_item_arr[i]->
						ptr_test_item_finished_func_param
						=
						(p_test_proc_data->
						pp_test_item_arr)[i];
				}
				break;

				/*----------------------hardware test-----------------------*/
			case TP_SHORT_TEST_ITEM_ID:
				{
					pp_test_item_arr[i]->
						ptr_test_item_finished_func
						= print_short_test_res;
					pp_test_item_arr[i]->
						ptr_test_item_finished_func_param
						=
						(p_test_proc_data->
						pp_test_item_arr)[i];
				}
				break;

			case TP_VERSION_TEST_ITEM_ID:
				{
					pp_test_item_arr[i]->
						ptr_test_item_finished_func
						= print_version_test_res;
					pp_test_item_arr[i]->
						ptr_test_item_finished_func_param
						=
						(p_test_proc_data->
						pp_test_item_arr)[i];
				}
				break;

				/*----------------------other test-----------------------*/
			case TP_CHIP_CFG_PROC_ITEM_ID:
				{
					pp_test_item_arr[i]->
						ptr_test_item_finished_func
						= print_send_cfg_test_res;
					pp_test_item_arr[i]->
						ptr_test_item_finished_func_param
						=
						(p_test_proc_data->
						pp_test_item_arr)[i];
				}
				break;

			case TP_RECOVER_CFG_ITEM_ID:
				{
					pp_test_item_arr[i]->
						ptr_test_item_finished_func
						=
						print_cfg_recover_test_res;
					pp_test_item_arr[i]->
						ptr_test_item_finished_func_param
						=
						(p_test_proc_data->
						pp_test_item_arr)[i];
				}
				break;

			case TP_FLASH_TEST_ITEM_ID:
				{
					pp_test_item_arr[i]->
						ptr_test_item_finished_func
						= print_flash_test_res;
					pp_test_item_arr[i]->
						ptr_test_item_finished_func_param
						=
						(p_test_proc_data->
						pp_test_item_arr)[i];
				}
				break;

			default:
				{
					;
				}
				break;
			}
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: release_test_item
* Description	: release test item
* Input			: PST_TEST_PROC_DATA* pp_test_proc_data
* Output		: PST_TEST_PROC_DATA* pp_test_proc_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_test_item(PST_TEST_PROC_DATA p_test_proc_data)
{
	s32 ret = 1;
	u16 i = 0;
	PST_TEST_ITEM *pp_test_item_arr = NULL;
	u16 item_num = 0;
	pp_test_item_arr = p_test_proc_data->pp_test_item_arr;
	item_num = p_test_proc_data->test_item_num;
	if (pp_test_item_arr == NULL) {
		return ret;
	}

	for (i = 0; i < item_num; i++) {
		if ((p_test_proc_data->pp_test_item_arr)[i] != NULL) {
			switch (pp_test_item_arr[i]->test_item_id) {
				/*----------------------tp open test-----------------------*/
			case TP_RAWDATA_TEST_ITEMS_SET_ID:
				{
					ret = release_rawdata_test_item((p_test_proc_data->pp_test_item_arr)[i]);
				}
				break;

				/*----------------------hardware test-----------------------*/
			case TP_SHORT_TEST_ITEM_ID:
				{
					ret = release_short_test_item((p_test_proc_data->pp_test_item_arr)[i]);
				}
				break;

			case TP_VERSION_TEST_ITEM_ID:
				{
					ret = version_test_finished_func((p_test_proc_data->pp_test_item_arr)[i]);
				}
				break;

				/*----------------------other test-----------------------*/
			case TP_CHIP_CFG_PROC_ITEM_ID:
				{
					ret = release_cfg_test_item((p_test_proc_data->pp_test_item_arr)[i]);
				}
				break;

			case TP_RECOVER_CFG_ITEM_ID:
				{
					ret = release_cfg_recover_test_item
						((p_test_proc_data->
						pp_test_item_arr)[i]);
				}
				break;

			case TP_FLASH_TEST_ITEM_ID:
				{
					ret = release_flash_test_item((p_test_proc_data->pp_test_item_arr)[i]);
				}
				break;
			default:
				{
					;
				}
				break;
			}
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: init_test_proc_data
* Description	: initial test proc data
* Input			: PST_TEST_PROC_DATA p_test_proc_data
				: PST_TEST_PARAM* pp_test_param
* Output		: PST_TEST_PROC_DATA p_test_proc_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 init_test_proc_data(PST_TEST_PROC_DATA *pp_test_proc_data,
				PST_TEST_PARAM *pp_test_param,
				void *p_drv_dev)
{
	s32 ret = 1;
	u16 i = 0;
	PST_TEST_PARAM p_test_param = NULL;
	PST_TEST_PROC_DATA p_test_proc_data = NULL;

	/*(1)malloc memory for test proc data*/
	if (alloc_mem_in_heap
		((void **)pp_test_proc_data,
		sizeof(ST_TEST_PROC_DATA)) == 0) {
		board_print_error
			("test proc data malloc memory error!");
		return 0;
	}
	p_test_proc_data = (PST_TEST_PROC_DATA) (*pp_test_proc_data);
	p_test_proc_data->p_dev = NULL;
	p_test_proc_data->test_item_num = 0;
	p_test_proc_data->test_break_flag = 0;
	p_test_proc_data->pp_test_item_arr = NULL;
	p_test_proc_data->p_test_items_before_func_ptr = NULL;
	p_test_proc_data->p_test_items_finished_func_ptr = NULL;

	/*malloc memory for tp device */
	ret = alloc_mem_in_heap((void **)&p_test_proc_data->p_dev, sizeof(ST_TP_DEV));
	if (0 == ret) {
		board_print_error("tp device malloc memory error!");
		return 0;
	}
	p_test_proc_data->p_dev->doze_ref_cnt = 1;
	p_test_proc_data->p_dev->hid_state = 1;
	p_test_proc_data->p_dev->chip_type = TP_NORMANDY;
	p_test_proc_data->p_dev->p_logic_dev = p_drv_dev;

	/*(4)parse test param*/
	ret = parse_test_order(pp_test_param, p_test_proc_data->p_dev);
	if (ret == 0) {
		board_print_error("parse test order error!\n");
		return 0;
	}
	p_test_param = (PST_TEST_PARAM) (*pp_test_param);

	/*(7)init test param*/
	p_test_proc_data->test_item_num = p_test_param->test_item_num;
	ret = alloc_mem_in_heap((void **)
				&(p_test_proc_data->pp_test_item_arr),
				p_test_proc_data->test_item_num *
				sizeof(PST_TEST_ITEM));
	if (0 == ret) {
		board_print_error("pp_test_item_arr malloc error!\n");
		return ret;
	}
	/*alloc memory for each varibale*/
	for (i = 0; i < p_test_proc_data->test_item_num; i++) {
		ret = alloc_mem_in_heap((void
					**)(&(p_test_proc_data->
						pp_test_item_arr[i])),
					sizeof(ST_TEST_ITEM));
		if (0 == ret) {
			return ret;
		}
		/*init pointer in test item struct*/
		(p_test_proc_data->pp_test_item_arr)[i]->test_item_id =
			p_test_param->p_test_item_id_set[i].test_id;
		(p_test_proc_data->pp_test_item_arr)[i]->p_sub_id_set =
			p_test_param->p_test_item_id_set[i].p_sub_id_set;
		(p_test_proc_data->pp_test_item_arr)[i]->p_tp_dev =
			p_test_proc_data->p_dev;
		(p_test_proc_data->pp_test_item_arr)[i]->cur_data.
			ptr_cur_data = NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->param.
			ptr_param = NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->test_res.
			ptr_res = NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->
			ptr_general_func = NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->
			ptr_test_item_before_func = NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->
			ptr_test_item_before_func_param = NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->ptr_test_func =
			NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->
			ptr_test_item_finished_func = NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->
			ptr_test_item_finished_func_param = NULL;
		(p_test_proc_data->pp_test_item_arr)[i]->
			ptr_test_proc_data = p_test_proc_data;
		(p_test_proc_data->pp_test_item_arr)[i]->
			ptr_special_callback_data = NULL;
	}

	/*register test item  function*/
	ret = register_test_item_func(p_test_proc_data);

	/*register test item finish func(print test result)*/
	ret = register_test_item_finished_func(p_test_proc_data);

	/*init test item*/
	ret = init_test_item(p_test_proc_data, p_test_param);
	if (ret == 0) {
		board_print_error("init test item error!\n");
		return ret;
	}
	/*sort test item*/
	ret = sort_item_permutation(p_test_proc_data);
	for (i = 0; i < p_test_proc_data->test_item_num; i++) {
		board_print_debug("test seq:%d\n",
				(p_test_proc_data->
				pp_test_item_arr)[i]->test_item_id);
	}

	return ret;
}

/*******************************************************************************
* Function Name	: release_test_proc_data
* Description	: release test proc data ,this function will be called
				after changing test parameter
* Input			: PST_TEST_PROC_DATA* pp_test_proc_data
* Output		: PST_TEST_PROC_DATA* pp_test_proc_data
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_test_proc_data(PST_TEST_PROC_DATA *
				pp_test_proc_data)
{
	s32 ret = 1;
	u16 i = 0;

	PST_TEST_PROC_DATA p_test_proc_data = NULL;
	if ((pp_test_proc_data == NULL)
		|| ((*pp_test_proc_data) == NULL)) {
		return 1;
	}

	p_test_proc_data = (*pp_test_proc_data);
	ret = release_test_item(p_test_proc_data);
	if (NULL != p_test_proc_data->p_dev) {
		if (p_test_proc_data->p_dev->chip_type != 0xffff) {
			if (p_test_proc_data->p_dev->chip_type ==
				TP_PHOENIX) {
				ret = chip_reg_write(p_test_proc_data->
							p_dev,
							p_test_proc_data->
							p_dev->cmd_set.
							hid_ena_cmd.addr,
							p_test_proc_data->
							p_dev->cmd_set.
							hid_ena_cmd.cmd_buf,
							p_test_proc_data->
							p_dev->cmd_set.
							hid_ena_cmd.cmd_len);
			}

		}
		p_test_proc_data->p_dev->p_logic_dev = NULL;
		ret = free_mem_in_heap(p_test_proc_data->p_dev);
		p_test_proc_data->p_dev = NULL;
	}

	if (NULL != p_test_proc_data->pp_test_item_arr) {
		for (i = 0; i < p_test_proc_data->test_item_num; i++) {
			ret = free_mem_in_heap(p_test_proc_data->
						pp_test_item_arr[i]);
			p_test_proc_data->pp_test_item_arr[i] = NULL;
		}
		ret = free_mem_in_heap(p_test_proc_data->
					pp_test_item_arr);
		p_test_proc_data->pp_test_item_arr = NULL;
	}
	ret = free_mem_in_heap(p_test_proc_data);
	p_test_proc_data = NULL;

	return ret;
}

/* test */
/*******************************************************************************
* Function Name	: test_process
* Description	: test process
* Input			: void* p_drv_dev
* Output		: none
* Return		: s32(-1:fail other:test finish)
*******************************************************************************/
extern s32 test_process(void *p_drv_dev)
{
	s32 ret = 0;
	PST_TEST_PARAM p_test_param = NULL;
	PST_TEST_PROC_DATA p_test_proc_data = NULL;

	init_test_res();	/* init result as 0 before start test*/

	/*(1)parse test order and init test proc data*/
	board_print_debug("init test proc data!\n");
	ret =
		init_test_proc_data(&p_test_proc_data, &p_test_param,
				p_drv_dev);
	if (ret == 0) {
		goto TEST_EXIT;
	}
	/*(2)start test*/
	board_print_debug("start test!\n");
	start_test_interface(p_test_proc_data);
	/*(4)set test status*/
	board_print_debug("release test proc data!\n");
	ret &= stop_test_interface(p_test_proc_data);
	if (get_detail_info_flag() == 0) {
	}

TEST_EXIT:
	/*(5)release test proc data*/
	ret &= release_test_proc_data(&p_test_proc_data);
	/*(6)release test param*/
	ret &= release_test_param(&p_test_param);

	/*(7)modify return value*/
	if (ret == 0) {
		return (s32)(-1);
	}
	ret = get_test_res();
	board_print_debug("test_result is:%d\n", ret);
	if (ret & TEST_ABORT) {
		return (s32)(-1);
	}
	return ret;
}

/* test*/
/*******************************************************************************
* Function Name	: get_tp_rawdata
* Description	: get_tp_rawdata
* Input			: void* p_drv_dev
* Output		: none
* Return		: s32(-1:fail other:test finish)
*******************************************************************************/
extern int get_tp_rawdata(void *p_drv_dev, char *buf, int *buf_size)
{
	s32 ret = 0;
	int offset = 0;
	int r = 0, i = 0;
	s32 data_tmp = 0;
	u16 node_num = 0;
	u8 data_size = 0;
	u8 rawdata_test_id = 0;
	PST_TEST_PARAM p_test_param = NULL;
	PST_TEST_PROC_DATA p_test_proc_data = NULL;
	PST_CUR_RAWDATA p_cur_data = NULL;
	/*(1)parse test order and init test proc data*/
	board_print_debug("init get rawdata proc!\n");
	ret = init_test_proc_data(&p_test_proc_data, &p_test_param,
				p_drv_dev);
	if (ret == 0) {
		goto TEST_EXIT;
	}
	/*find the rawdata_test_item id for get_rawdata */
	for (i = 0; i < p_test_proc_data->test_item_num; i++) {
		if (p_test_proc_data->pp_test_item_arr[i]->test_item_id
			== TP_RAWDATA_TEST_ITEMS_SET_ID) {
			rawdata_test_id = i;
			break;
		}
	}
	if (i == p_test_proc_data->test_item_num) {
		goto TEST_EXIT;
	}
	node_num =
		p_test_proc_data->p_dev->sc_sen_num *
		p_test_proc_data->p_dev->sc_drv_num;
	data_size =
		get_data_size(p_test_proc_data->p_dev->rawdata_option);
	board_print_debug("[%s]node_num = %d,data_size = %d\n",
			__func__, node_num, data_size);

	if (!alloc_mem_block
		((void **)&(p_cur_data), sizeof(p_cur_data))) {
		goto TEST_EXIT;
	}
	if (alloc_mem_block
		((void **)&(p_cur_data->p_data_buf),
		node_num * data_size)) {
		/*init_for_buffer*/
		for (i = 0; i < node_num * data_size; i++) {
			p_cur_data->p_data_buf[i] = 0;
		}
	}
	ret = chip_mode_select((p_test_proc_data->
				pp_test_item_arr)[rawdata_test_id]->
				p_tp_dev, CHIP_RAW_DATA_MODE);
	board_get_rawdata((p_test_proc_data->
			pp_test_item_arr)[rawdata_test_id]->p_tp_dev,
			3000, 0, p_cur_data);

	r = snprintf(&buf[offset], 20, "Tx:%d Rx:%d\n",
			p_test_proc_data->p_dev->sc_drv_num,
			p_test_proc_data->p_dev->sc_sen_num);
	offset += r;
	r = snprintf(&buf[offset], 2, "\n");
	offset += r;
	/*end print test cfg */
	r = snprintf(&buf[offset], 20, "Rawdata:\n");
	offset += r;
	board_print_debug("[%s]r = %d\n", __func__, r);
	for (i = 0; i < node_num; i++) {
		data_tmp =
			(s16) ((p_cur_data->p_data_buf[i * 2]) +
				(p_cur_data->p_data_buf[i * 2 + 1] << 8));
		r = snprintf(&buf[offset], 10, "%4d,", data_tmp);
		offset += r;
		if (((i + 1) % (p_test_proc_data->p_dev->sc_drv_num)) ==
			0) {
			r = snprintf(&buf[offset], 2, "\n");
			offset += r;
		}
	}
	*buf_size = offset;
	ret = chip_mode_select((p_test_proc_data->
				pp_test_item_arr)[rawdata_test_id]->
				p_tp_dev, CHIP_COORD_MODE);
	/*get diff data end*/

	ret &= free_mem_block((void *)p_cur_data->p_data_buf);
	p_cur_data->p_data_buf = NULL;
TEST_EXIT:
	ret &= free_mem_block((void *)p_cur_data);
	p_cur_data = NULL;
	/*(5)release test proc data*/
	ret &= release_test_proc_data(&p_test_proc_data);
	/*(6)release test order*/
	ret &= release_test_param(&p_test_param);
	/*(7)modify return value*/
	return offset;
}

/* test*/
/*******************************************************************************
* Function Name	: get_tp_rawdata
* Description	: get_tp_rawdata
* Input			: void* p_drv_dev
* Output		: none
* Return		: s32(-1:fail other:test finish)
*******************************************************************************/
extern int get_tp_testcfg(void *p_drv_dev, char *buf, int *buf_size)
{
	s32 ret = 0;
	int offset = 0;
	int r = 0, i = 0;
	PST_TEST_PARAM p_test_param = NULL;
	PST_TEST_PROC_DATA p_test_proc_data = NULL;

	/*(1)parse test order and init test proc data*/
	board_print_debug("init get rawdata proc!\n");
	ret =
		init_test_proc_data(&p_test_proc_data, &p_test_param,
				p_drv_dev);
	if (ret == 0) {
		goto TEST_EXIT;
	}

	offset += r;
	/*print test cfg */
	r = snprintf(&buf[offset], 20, "Test-config:\n");
	offset += r;
	for (i = 0; i < p_test_param->cfg_len; i++) {
		r = snprintf(&buf[offset], 10, "0x%2x,",
				p_test_param->test_cfg[i]);
		offset += r;
		if ((i + 1) % 30 == 0) {
			r = snprintf(&buf[offset], 2, "\n");
			offset += r;
		}
	}
	r = snprintf(&buf[offset], 2, "\n");
	offset += r;
	/*end print test cfg */
TEST_EXIT:
	/*(5)release test proc data*/
	ret &= release_test_proc_data(&p_test_proc_data);
	/*(6)release test order*/
	ret &= release_test_param(&p_test_param);
	/*(7)modify return value*/

	return offset;
}

#ifdef __cplusplus
}
#endif

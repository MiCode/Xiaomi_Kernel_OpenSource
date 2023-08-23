/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : normandy_short_test.c
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : normandy short test
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "normandy_short_test.h"
#include "tp_short_test.h"
#include "simple_mem_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*************************************Private methods start********************************************/
static s32 have_short_code_in_fw(PST_TP_DEV p_dev);
static s32 download_short_code(ptr32 p_data);
static s32 run_short_code(ptr32 p_data);
static s32 send_cmd(PST_TP_DEV p_dev, u8 cmd, u8 param);
static s32 wait_status(PST_TP_DEV p_dev, u16 sta_addr, u8 right_sta, u16 time_out);
static s32 send_short_param(ptr32 p_data);
static s32 wait_short_end(PST_TP_DEV p_dev);
static u16 calc_short_gnd_vdd(u16 diff_code, s32 avdd, u8 *p_short_opt);
static u16 calc_short_chn(PST_TP_DEV p_dev, IN u8 chn1, IN u8 chn2, IN u16 self_code, IN u16 short_code);
static s32 analyse_short_res(ptr32 p_data);
static u8 short_id_to_selfdata_id(PST_TP_DEV p_dev, u8 short_id);
/*************************************Private methods end********************************************/
/*******************************************************************************
* Function Name		: normandy_short_test
* Description		: normandy short test
* Input				: ptr32 p_data(You must cast it to type 'PST_TEST_ITEM' )
* Output			: none
* Return			: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 normandy_short_test(IN_OUT ptr32 p_data)
{
	s32 ret = 0;
	PST_SHORT_PARAM p_param = NULL;
	PST_SHORT_TEST_RES p_test_res = NULL;
	PST_TEST_ITEM p_item = NULL;
	PST_TP_DEV p_dev = NULL;
	p_item = (PST_TEST_ITEM) p_data;
	if (NULL == p_item) {
		board_print_error("%s\n",
				"in short test, p_item parameter is NULL");
		goto NORMANDY_SHORT_TEST_END;
	}
	p_dev = p_item->p_tp_dev;
	p_param = (PST_SHORT_PARAM) p_item->param.ptr_param;
	if (NULL == p_param) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = SHORT_TEST_PARAMETER_ERROR;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		p_test_res->item_res_header.test_result = TEST_NG;
		board_print_error("%s\n",
				  "in short test, parameter is NULL");
		goto NORMANDY_SHORT_TEST_END;
	}

	p_test_res = (PST_SHORT_TEST_RES) p_item->test_res.ptr_res;
	/*clear short mess*/
	clear_short_mess_in_res(p_item);
/*********test program*************/
	ret = have_short_code_in_fw(p_dev);
	if (1 != ret) {
		/*download short code in ram,and run this code*/
		ret = download_short_code(p_data);
		if (0 == ret) {
			p_test_res->item_res_header.test_item_err_code.
				ptr_err_code_set[0] =
				SHORT_TEST_DOWNLOAD_FW_FAIL;
			p_test_res->item_res_header.test_status =
				TEST_ABORTED;
			p_test_res->item_res_header.test_result =
				TEST_NG;
			board_print_error("%s\n",
					"in short test, download short fw fail");
			goto NORMANDY_SHORT_TEST_END;
		}
	}
	/* (2)run short code*/
	ret = run_short_code(p_data);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = SHORT_TEST_RUN_FW_FAIL;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		p_test_res->item_res_header.test_result = TEST_NG;
		board_print_error("%s\n",
				"in short test, run short fw error");
		goto NORMANDY_SHORT_TEST_END;
	}
	/*(3)send short param*/
	ret = send_short_param(p_data);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] =
		SHORT_TEST_SEND_PARAMETER_FAIL;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		p_test_res->item_res_header.test_result = TEST_NG;
		board_print_error("%s\n",
				"in short test, send short parameter fail");
		goto NORMANDY_SHORT_TEST_END;
	}
	/* (4)wait test end*/
	ret = wait_short_end(p_dev);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] = SHORT_TEST_END_FAIL;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		p_test_res->item_res_header.test_result = TEST_NG;
		board_print_error("%s\n",
				"in short test, wait test end fail");
		goto NORMANDY_SHORT_TEST_END;
	}
	/* (5)annlyze short result*/
	ret = analyse_short_res(p_data);
	if (0 == ret) {
		p_test_res->item_res_header.test_item_err_code.
			ptr_err_code_set[0] =
			SHORT_TEST_ANALYSIS_RESULT_FAIL;
		p_test_res->item_res_header.test_status = TEST_ABORTED;
		p_test_res->item_res_header.test_result = TEST_NG;
		board_print_error("%s\n",
				"in short test, annlyze short test result fail");
		goto NORMANDY_SHORT_TEST_END;
	}

	short_test_channel_map(p_item, TX_RX_SHARED);
	p_test_res->item_res_header.test_status = TEST_FINISH;
NORMANDY_SHORT_TEST_END:
	return ret;
}

#ifdef __cplusplus
}
#endif

/*******************************************************************************
* Function Name	: have_short_code_in_fw
* Description	: Judge that is there short code in firmware
* Input			: PST_TP_DEV p_dev
* Output		: none
* Return 		: s32(0:not exist short code in fw 1:exist short code in fw)
*******************************************************************************/
static s32 have_short_code_in_fw(IN PST_TP_DEV p_dev)
{
	return 1;
}

/*******************************************************************************
* Function Name	: download_short_code
* Description	: download short code
* Input			: ptr32 p_data(You must cast it to type 'PST_TEST_ITEM' )
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
static s32 download_short_code(IN ptr32 p_data)
{
	return 0;
}

/*******************************************************************************
* Function Name	: run_short_code
* Description	: run short code
* Input			: ptr32 p_data(You must cast it to type 'PST_TEST_ITEM' )
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
static s32 run_short_code(IN ptr32 p_data)
{
	s32 ret = 0;
	u8 i = 0, retry_cnt = 3;
	PST_TEST_ITEM p_item = NULL;
	PST_TP_DEV p_dev = NULL;
	p_item = (PST_TEST_ITEM) p_data;
	p_dev = p_item->p_tp_dev;

	for (i = 0; i < retry_cnt; i++) {
		/*switch to short firmware*/
		ret = send_cmd(p_dev, 0x0B, 0x00);
		if (0 == ret) {
			continue;
		}
		/*delay 130ms -> 50ms*/
		board_delay_ms(50);

		/*wait short ready*/
		ret = wait_status(p_dev, NORM_SHORT_STA_ADDR,
				NORM_SHORT_STA_READY, 200);
		if (1 == ret) {
			break;
		}
	}
	if (0 == ret) {
		board_print_error("%s\n", "switch to short firmware fail");
	}
	return ret;
}

/*******************************************************************************
* Function Name		: send_cmd
* Description		: send_cmd
* Input				: PST_TP_DEV p_dev
* Input				: u8 cmd
* Input 			: u8 param
* Output			: none
* Return			: s32(0:fail 1:ok)
*******************************************************************************/
static s32 send_cmd(IN PST_TP_DEV p_dev, IN u8 cmd, IN u8 param)
{
	s32 ret = 0;
	u8 buf_arr[3];
	buf_arr[0] = cmd;
	buf_arr[1] = param;
	buf_arr[2] = (u8) (0 - (cmd + param));
	ret = chip_reg_write(p_dev, p_dev->real_cmd_addr, buf_arr, 3);
	return ret;
}

/*******************************************************************************
* Function Name		: wait_status
* Description		: wait_status
* Input				: PST_TP_DEV p_dev
* Input				: u16 sta_addr
* Input				: u8 right_sta
* Input				: u16 time_out
* Output			: none
* Return			: s32(0:fail 1:ok)
*******************************************************************************/
static s32 wait_status(IN PST_TP_DEV p_dev, IN u16 sta_addr, IN u8 right_sta,
			IN u16 time_out)
{
	s32 ret = 0;
	u16 i = 0;
	u16 cnt = (time_out + 9) / 10;
	/*make sure unequal for initialization*/
	u8 data_tmp = right_sta - 1;

	for (i = 0; i < cnt; i++) {
		ret = chip_reg_read(p_dev, sta_addr, &data_tmp, 1);
		if ((ret == 1) && (data_tmp == right_sta)) {
			break;
		}
		board_delay_ms(10);
	}
	if (i >= cnt) {
		ret = 0;
		board_print_debug("short test status:0x%02x!\n", data_tmp);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: send_short_param
* Description	: send_short_param
* Input			: ptr32 p_data(You must cast it to type 'PST_TEST_ITEM' )
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
static s32 send_short_param(IN ptr32 p_data)
{
	s32 ret = 0;
	u8 buf_tmp[10];

	PST_TEST_ITEM p_item = NULL;
	PST_TP_DEV p_dev = NULL;
	PST_SHORT_PARAM p_short_param = NULL;
	p_item = (PST_TEST_ITEM) p_data;
	p_dev = p_item->p_tp_dev;
	p_short_param = (PST_SHORT_PARAM) p_item->param.ptr_param;

	/*data store in normandy ram wit big-endian
	//Tx&Tx Rx&Rx Tx&Rx short adc threshold
	*/
	buf_tmp[0] =
		((p_short_param->short_threshold.gt_short_threshold >> 8) & 0xFF);
	buf_tmp[1] =
		((p_short_param->short_threshold.gt_short_threshold) & 0xFF);
	/*diffcode threshold*/
	buf_tmp[2] =
		((p_short_param->short_threshold.
		diffcode_short_threshold >> 8) & 0xFF);
	buf_tmp[3] =
		((p_short_param->short_threshold.diffcode_short_threshold) & 0xFF);
	/*adc read delay*/
	buf_tmp[4] =
		((p_short_param->short_threshold.adc_read_delay >> 8) & 0xFF);
	buf_tmp[5] = ((p_short_param->short_threshold.adc_read_delay) & 0xFF);
	ret = chip_reg_write(p_dev, NORM_SHORT_TXRX_ADC_THD_ADDR, buf_tmp, 6);
	if (0 == ret) {
		board_print_error("%s\n",
				"send tx&rx or gnd&vdd adc threshold or adc delay fail");
		goto SEND_END;
	}
	/*Intialize cfg ,and start test*/
	buf_tmp[0] = 0x01;
	ret = chip_reg_write(p_dev, 0x5095, buf_tmp, 1);
	if (0 == ret) {
		board_print_error("%s\n", "write 0x01 to 0x5095 fail");
		goto SEND_END;
	}
SEND_END:
	return ret;
}

/*******************************************************************************
* Function Name	: wait_short_end
* Description	: wait shott end
* Input			: PST_TP_DEV p_dev
* Output		: none
* Return		: u8(0:fail 1:short test end)
*******************************************************************************/
static s32 wait_short_end(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	u8 tmp_data = 0;

	/*send start short test cmd*/
	tmp_data = 0x01;
	ret = chip_reg_write(p_dev, NORM_SHORT_STA_ADDR, &tmp_data, 1);
	if (0 == ret) {
		board_print_error("%s\n", "send start short test cmd fail!");
		goto WAIT_END;
	}
	/*delay 1800ms*/
	board_delay_ms(1800);
	/*wait test end*/
	ret =
		wait_status(p_dev, NORM_SHORT_TESTEND_ADDR, NORM_SHORT_ALL_TEST_END,
			1200);
	if (0 == ret) {
		board_print_error("%s\n", "wait test end fail!");
		goto WAIT_END;
	}
WAIT_END:
	return ret;
}

/*******************************************************************************
* Function Name	: get_chn_group
* Description	: get the group of channel
* Input			: u8 chn
* Output 		: none
* Return 		: u8(group num the channel in)
*******************************************************************************/
static u8 get_chn_group(IN u8 chn)
{
	u8 group = 0;

	if ((chn >= 0) && (chn < 9)) {	/*pad s0~s8*/
		group = 5;
	} else if ((chn >= 9) && (chn < 14)) {	/*pad s9~s13*/
		group = 4;
	} else if ((chn >= 14) && (chn < 18)) {	/*pad s14~s17*/
		group = 3;
	} else if ((chn >= 18) && (chn < 27)) {	/* pad s18~s26*/
		group = 2;
	} else if ((chn >= 27) && (chn < 32)) {	/* pad s27~s31*/
		group = 1;
	} else if ((chn >= 32) && (chn < 36)) {	/* pad s32~s35*/
		group = 0;
	} else if ((chn >= 36) && (chn < 45)) {	/* pad d0~d8*/
		group = 5;
	} else if ((chn >= 45) && (chn < 54)) {	/* pad d9~d17*/
		group = 2;
	} else if ((chn >= 54) && (chn < 59)) {	/* pad d18~d22*/
		group = 1;
	} else if ((chn >= 59) && (chn < 63)) {	/* pad d23~d26*/
		group = 0;
	} else if ((chn >= 63) && (chn < 67)) {	/* pad d27~d30*/
		group = 3;
	} else if ((chn >= 67) && (chn < 72)) {	/* pad d31~d35*/
		group = 4;
	} else if ((chn >= 72) && (chn < 76)) {	/* pad d36~d39 */
		group = 0;
	}
	return group;
}

/*******************************************************************************
* Function Name	: calc_short_gnd_vdd
* Description	: calculate short between channel with gnd or vdd(similar to gt9p)
* Input			: u16 diff_code
* Input			: s32 avdd
* Output		: u8* p_short_opt(0:gnd short 1:vdd short)
* Return		: u16((resistor(0.1kom)))
*******************************************************************************/
static u16 calc_short_gnd_vdd(IN u16 diff_code, IN s32 avdd,
			OUT u8 *p_short_opt)
{
	s32 r = 0;
	if ((diff_code & 0x7fff) == 0) {
		diff_code += 1;
	}
	/*bit15==1 means vdd,bit15==0 means gnd*/
	if ((diff_code & (0x8000)) == 0) {	/*short to GND*/
		/*52662.85/diff_code-40*/
		r = (526628 / (diff_code & (~0x8000)) - 40 * 10);
		*p_short_opt = 0;
	} else {	/*short to VDD*/
		diff_code &= (~0x8000);
		r = 9 * 1024 * (avdd - 9) * 40 / diff_code / 7 - 40 * 10;
		*p_short_opt = 1;
	}
	/*r *= 2;*/
	if (r < 65530) {
		;
	} else {
		r = 65535;
	}
	board_print_info("GND/VDD r:%d", r);
	return (u16) (r >= 0 ? r : 0);
}

/*******************************************************************************
* Function Name	: calc_short_chn
* Description	: calculate short between chn1 with chn2
* Input			: PST_TP_DEV p_dev
				: u8 chn1
* Input			: u8 chn2
* Input			: u16 self_code
* Input			: u16 short_code
* Output		: none
* Return		: u16(resistor(0.1kom))
*******************************************************************************/
static u16 calc_short_chn(PST_TP_DEV p_dev, IN u8 chn1, IN u8 chn2,
			IN u16 self_code, IN u16 short_code)
{
	s32 r = 0;
	s32 b_in_same_grp = 0;
	u8 group1 = 0;
	u8 group2 = 0;
	u8 die_num1 = 0;
	u8 die_num2 = 0;
	u8 chn_flag = 0;
	if (((chn1 & CHANNEL_TX_FLAG) == CHANNEL_TX_FLAG)
		&& ((chn2 & CHANNEL_TX_FLAG) == CHANNEL_TX_FLAG)) {
		chn1 &= ~CHANNEL_TX_FLAG;
		chn2 &= ~CHANNEL_TX_FLAG;
		group1 = get_chn_group(chn1);
		group2 = get_chn_group(chn2);
		chn_flag = 1;
		if (group1 == group2) {
			b_in_same_grp = 1;
			die_num1 += chn1;
			die_num2 += chn2;
		}
	} else if (((chn1 & CHANNEL_TX_FLAG) == 0)
		&& ((chn2 & CHANNEL_TX_FLAG) == 0)) {
		group1 = get_chn_group(chn1);
		group2 = get_chn_group(chn2);
		chn_flag = 1;
		if (group1 == group2) {
			b_in_same_grp = 1;
		}
	}

	if (!b_in_same_grp) {
		r = (10 * self_code * 81 / short_code - 81*10);
	} else {
		die_num1 =
			(die_num1 >=
			die_num2) ? (die_num1 - die_num2) : (die_num2 - die_num1);
		if ((die_num1 > 3) && (group1 == 0)) {	/*different setting*/
			r = (10 * self_code * 81 / short_code - 81 * 10);
		} else {
			r = (10 * self_code * 64 / short_code - 64 * 10);
		}
	}
	if (r < 65535) {
		;
	} else {
		r = 65535;
	}
	return (u16) (r >= 0 ? r : 0);
}

/*******************************************************************************
* Function Name	: analyse_short_res
* Description	: analyse short result
* Input			: ptr32 p_data(You must cast it to type 'PST_TEST_ITEM' )
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
static s32 analyse_short_res(IN_OUT ptr32 p_data)
{
	s32 ret = 0;
	u8 *p_tmp_data_arr = NULL;
	u16 i = 0, j = 0;
	u16 retry_cnt = 3;
	PST_TEST_ITEM p_test_item = NULL;
	PST_TP_DEV p_dev = NULL;
	PST_SHORT_PARAM p_short_param = NULL;
	PST_SHORT_TEST_RES p_short_res = NULL;

	u16 short_code_tmp = 0;
	u16 data_len_tmp = 0;
	u16 tmp_data16 = 0;
	u8 chn_id_tmp1 = 0;
	u8 chn_id_tmp2 = 0;
	u8 id_tmp = 0;
	u8 short_flag = 0;
	u8 chn_short_cnt[3];
	u16 diff_code_tmp = 0;
	u16 short_r = 0;
	u16 *p_self_code_arr = NULL;
	u16 chn_short_mess_addr = 0;
	u8 max_sen_num_in_die = 0;
	u8 max_drv_num_in_die = 0;

	p_test_item = (PST_TEST_ITEM) p_data;
	p_dev = p_test_item->p_tp_dev;
	p_short_param = (PST_SHORT_PARAM) p_test_item->param.ptr_param;
	p_short_res = (PST_SHORT_TEST_RES) p_test_item->test_res.ptr_res;
	max_sen_num_in_die = p_dev->max_sen_num_in_die;
	max_drv_num_in_die = p_dev->max_drv_num_in_die;

	data_len_tmp = (max_drv_num_in_die + max_sen_num_in_die) * 2;
	ret = alloc_mem_block((void **)&p_tmp_data_arr, data_len_tmp);
	if (0 == ret) {
		board_print_error("%s\n",
				"in analyse short test result, malloc memory for p_tmp_data_arr fail");
		goto ANALYSE_SHORT_RES_END;
	}

	data_len_tmp = (max_drv_num_in_die + max_sen_num_in_die) * 2;
	ret = alloc_mem_block((void **)&p_self_code_arr, data_len_tmp);
	if (0 == ret) {
		board_print_error("%s\n",
				"in analyse short test result, malloc memory for self_code_arr fail");
		goto ANALYSE_SHORT_RES_END;
	}
	/*judge short flag*/
	for (i = 0; i < retry_cnt; i++) {
		ret =
			chip_reg_read(p_dev, NORM_SHORT_TEST_RESULT_ADDR,
				&short_flag, 1);
		if (1 == ret) {
			break;
		}
	}
	if (0 == ret) {
		board_print_error("%s\n",
				"in analyse short test result, read short flag fail");
		goto ANALYSE_SHORT_RES_END;
	}

	/*short_flag:
	bit0: Tx & Tx
	bit1: Rx & Rx
	bit2: Tx & Rx
	bit3: Tx/Rx & GND/VDD
	*/

	if (0 == (short_flag & 0x0F)) {
		/*board_print_error("%s\n", "in analyse short test result, read short flag fail");*/
		goto ANALYSE_SHORT_RES_END;
	}
	/*bit3: Tx/Rx & GND/VDD  */
	data_len_tmp = (max_drv_num_in_die + max_sen_num_in_die) * 2;
	if (0x08 == (short_flag & 0x08)) {
		/*get diffcode*/
		for (i = 0; i < retry_cnt; i++) {
			/*NORM_SHORT_DIFFCODE_ADDR is the start address of diffcode*/
			ret =
				chip_reg_read(p_dev, NORM_SHORT_DIFFCODE_ADDR,
					p_tmp_data_arr, data_len_tmp);
			if (1 == ret) {
				break;
			}
		}
		if (0 == ret) {
			board_print_error("%s\n",
					"in analyse Tx/Rx & GND/VDD, read diffcode fail");
			goto ANALYSE_SHORT_RES_END;
		}
		for (i = 0; i < data_len_tmp; i += 2) {
			diff_code_tmp =
				(p_tmp_data_arr[i] << 8) + p_tmp_data_arr[i + 1];
			/*calculate short_r*/
			short_r = calc_short_gnd_vdd(diff_code_tmp,
						(10 * p_dev->power_volt / 1000),
						&chn_id_tmp2);
			chn_id_tmp1 = i >> 1;

			/*must convert it
			//|max_drv_num_in_die|max_sen_num_in_die|
			*/
			if (chn_id_tmp1 < max_drv_num_in_die) {
				chn_id_tmp1 += max_sen_num_in_die;
				chn_id_tmp1 |= CHANNEL_TX_FLAG;
				tmp_data16 =
					p_short_param->short_threshold.
					drv_gnd_vdd_resistor_threshold * 10;
			} else {
				chn_id_tmp1 -= max_drv_num_in_die;
				tmp_data16 =
					p_short_param->short_threshold.
					sen_gnd_vdd_resistor_threshold * 10;
			}
			if (1 == chn_id_tmp2) {
				chn_id_tmp2 = CHANNEL_VDD;
			} else if (0 == chn_id_tmp2) {
				chn_id_tmp2 = CHANNEL_GND;
			}
			/*store this data in result*/
			if (short_r < tmp_data16) {
				store_short_mess_in_res(chn_id_tmp1,
							chn_id_tmp2, short_r,
							p_short_res);
			}
		}
	}

	/*get self code*/
	data_len_tmp = (max_drv_num_in_die + max_sen_num_in_die) * 2;
	for (i = 0; i < retry_cnt; i++) {
		/*NORM_SHORT_DRVSELFCODE_ADDR is the start address of self code*/
		ret = chip_reg_read(p_dev, NORM_SHORT_DRVSELFCODE_ADDR,
				p_tmp_data_arr, data_len_tmp);
		if (1 == ret) {
			break;
		}
	}
	if (0 == ret) {
		board_print_error("%s\n",
				"in analyse Tx/Rx, read selfcode fail");
		goto ANALYSE_SHORT_RES_END;
	}
	for (i = 0; i < data_len_tmp / 2; i++) {
		p_self_code_arr[i] =
			((p_tmp_data_arr[2 * i] << 8) +
			p_tmp_data_arr[2 * i + 1]) & 0x7FFF;
	}

	/*get channel short cnt*/
	for (i = 0; i < retry_cnt; i++) {
		/*0x8042 Tx&Tx short cnt
		//0x8043 Rx&Rx short cnt
		//0x8044 Tx&Rx short cnt
		*/
		ret = chip_reg_read(p_dev, NORM_SHORT_NUMBER_ADDR, chn_short_cnt,
				3);
		if (1 == ret) {
			break;
		}
	}
	if (0 == ret) {
		board_print_error("%s\n", "in analyse Tx/Rx, read channel short cnt fail");
		goto ANALYSE_SHORT_RES_END;
	}
	/*Tx & Tx*/
	if (0x01 == (short_flag & 0x01)) {
		chn_short_mess_addr = NORM_SHORT_TxTx_PIN_NUM_ADDR;
		/*2 bytes short pin num
		//2 bytes reserved
		//2*max_drv_num_in_die bytes TX buffer
		//2 bytes check sum
		*/
		data_len_tmp = 2 + 2 + 2 * max_drv_num_in_die + 2;
		for (i = 0; i < chn_short_cnt[0]; i++) {
			/*get short code*/
			for (j = 0; j < retry_cnt; j++) {
				ret = chip_reg_read(p_dev, chn_short_mess_addr,
						p_tmp_data_arr, data_len_tmp);
				if (1 == ret) {
					break;
				}
			}
			if (0 == ret) {
				board_print_error("%s\n", "in analyse Tx & Tx, read short code fail");
				goto ANALYSE_SHORT_RES_END;
			}
			/*calc resistor
			Normandy Sens Pad and Drv Pads is Unified Numbering
			0~35 is s0~s35(RX) And 36~76 is d0 ~d40(TX)
			*/
			chn_id_tmp1 = (p_tmp_data_arr[0] << 8) + p_tmp_data_arr[1];
			{
				/*Tx*/
				for (j = 0; j < max_drv_num_in_die; j++) {
					short_code_tmp =
						(p_tmp_data_arr
						[NORM_SHORT_HEAD + j * 2] << 8) +
						p_tmp_data_arr[NORM_SHORT_HEAD +
							j * 2 + 1];
					chn_id_tmp1 |= CHANNEL_TX_FLAG;
					chn_id_tmp2 =
						(j +
						max_sen_num_in_die) |
						CHANNEL_TX_FLAG;
					if (short_code_tmp >=
						p_short_param->short_threshold.
						gt_short_threshold) {
						id_tmp =
							short_id_to_selfdata_id
							(p_dev, chn_id_tmp1);
						short_r =
							calc_short_chn(p_dev,
								chn_id_tmp1,
								chn_id_tmp2,
								p_self_code_arr
								[id_tmp],
								short_code_tmp);
						if (short_r <
							p_short_param->
							short_threshold.
							drv_drv_resistor_threshold *
							10) {
							store_short_mess_in_res
								(chn_id_tmp1,
								chn_id_tmp2,
								short_r,
								p_short_res);
						}
					}
				}
			}
			/*update address*/
			chn_short_mess_addr += data_len_tmp;
		}
	}
	/*Rx & Rx*/
	if (0x02 == (short_flag & 0x02)) {
		chn_short_mess_addr = NORM_SHORT_RxRx_PIN_NUM_ADDR;
		/*2 bytes short pin num
		//2 bytes reserved
		//2*max_sen_num_in_die bytes RX buffer
		//2 bytes check sum
		*/
		data_len_tmp = 2 + 2 + 2 * max_sen_num_in_die + 2;
		for (i = 0; i < chn_short_cnt[1]; i++) {
			/*get short code*/
			for (j = 0; j < retry_cnt; j++) {
				ret = chip_reg_read(p_dev, chn_short_mess_addr,
						p_tmp_data_arr, data_len_tmp);
				if (1 == ret) {
					break;
				}
			}
			if (0 == ret) {
				board_print_error("%s\n", "in analyse Rx & Rx, read short code fail");
				goto ANALYSE_SHORT_RES_END;
			}
			/*calc resistor*/
			chn_id_tmp1 = (p_tmp_data_arr[0] << 8) + p_tmp_data_arr[1];	/*Rx*/
			{
				/*Rx*/
				for (j = 0; j < max_sen_num_in_die; j++) {
					/*filter this case*/
					if (j == chn_id_tmp1
						|| (j < chn_id_tmp1
						&& ((j & 0x01) == 0))) {
						continue;
					}

					short_code_tmp =
						(p_tmp_data_arr
						[NORM_SHORT_HEAD + j * 2] << 8) +
						p_tmp_data_arr[NORM_SHORT_HEAD +
							j * 2 + 1];
					chn_id_tmp2 = (u8) j;
					if (short_code_tmp >=
						p_short_param->short_threshold.
						gt_short_threshold) {
						id_tmp =
							short_id_to_selfdata_id
							(p_dev, chn_id_tmp1);
						short_r =
							calc_short_chn(p_dev,
								chn_id_tmp1,
								chn_id_tmp2,
								p_self_code_arr
								[id_tmp],
								short_code_tmp);
						if (short_r <
							p_short_param->
							short_threshold.
							sen_sen_resistor_threshold *
							10) {
							store_short_mess_in_res
								(chn_id_tmp1,
								chn_id_tmp2,
								short_r,
								p_short_res);
						}
					}
				}
			}
			/*update address*/
			chn_short_mess_addr += data_len_tmp;
		}
	}
	/*Tx & Rx*/
	if (0x04 == (short_flag & 0x04)) {
		chn_short_mess_addr = NORM_SHORT_TxRx_PIN_NUM_ADDR;
		/*2 bytes short pin num
		//2 bytes reserved
		//2*max_drv_num_in_die bytes RX buffer
		//2 bytes check sum
		*/
		data_len_tmp = 2 + 2 + 2 * max_drv_num_in_die + 2;
		for (i = 0; i < chn_short_cnt[2]; i++) {
			/*get short code*/
			for (j = 0; j < retry_cnt; j++) {
				ret = chip_reg_read(p_dev, chn_short_mess_addr,
						p_tmp_data_arr, data_len_tmp);
				if (1 == ret) {
					break;
				}
			}
			if (0 == ret) {
				board_print_error("%s\n", "in analyse Tx & Rx, read short code fail");
				goto ANALYSE_SHORT_RES_END;
			}
			/*calc resistor*/
			chn_id_tmp1 = (p_tmp_data_arr[0] << 8) + p_tmp_data_arr[1];	/*Rx*/
			{
				/*Tx*/
				for (j = 0; j < max_drv_num_in_die; j++) {
					short_code_tmp =
						(p_tmp_data_arr
						[NORM_SHORT_HEAD + j * 2] << 8) +
						p_tmp_data_arr[NORM_SHORT_HEAD +
						j * 2 + 1];
					chn_id_tmp2 =
						(j +
						max_sen_num_in_die) |
						CHANNEL_TX_FLAG;
					if (short_code_tmp >=
						p_short_param->short_threshold.
						gt_short_threshold) {
						id_tmp =
							short_id_to_selfdata_id
							(p_dev, chn_id_tmp1);
						short_r =
							calc_short_chn(p_dev,
								chn_id_tmp1,
								chn_id_tmp2,
								p_self_code_arr
								[id_tmp],
								short_code_tmp);
						if (short_r <
							p_short_param->
							short_threshold.
							drv_sen_resistor_threshold *
							10) {
							store_short_mess_in_res
								(chn_id_tmp1,
								chn_id_tmp2,
								short_r,
								p_short_res);
						}
					}
				}
			}
			/*update address*/
			chn_short_mess_addr += data_len_tmp;
		}
	}
	if (p_short_res->short_num > 0) {
		p_short_res->item_res_header.test_item_err_code.
		ptr_err_code_set[0] = SHORT_TEST_EXIST_SHORT;
		p_short_res->item_res_header.test_status = TEST_ABORTED;
		p_short_res->item_res_header.test_result = TEST_NG;
	}

ANALYSE_SHORT_RES_END:
	free_mem_block(p_tmp_data_arr);
	free_mem_block(p_self_code_arr);
	return ret;
}

/*******************************************************************************
* Function Name	: short_id_to_selfdata_id
* Description	: short_id_to_selfdata_id
* Input			: PST_TP_DEV p_dev
* Input			: u8 short_id
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
static u8 short_id_to_selfdata_id(IN PST_TP_DEV p_dev, IN u8 short_id)
{
	u8 ret = 0;
	/*drv*/
	if ((short_id & CHANNEL_TX_FLAG) == CHANNEL_TX_FLAG) {
		ret = (short_id & ~CHANNEL_TX_FLAG) - p_dev->max_sen_num_in_die;
	} else {
		ret = short_id + p_dev->max_drv_num_in_die;
	}
	return ret;
}

#endif

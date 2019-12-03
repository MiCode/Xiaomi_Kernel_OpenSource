/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_normandy_control.cpp
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : normandy device control interface
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "tp_normandy_control.h"
#include "board_opr_interface.h"
#include "tp_dev_control.h"

#ifdef __cplusplus
extern "C" {
#endif

/*************************************Private methods start********************************************/
static s32 normandy_hold_dsp(PST_TP_DEV p_dev);
/*************************************Private methods End********************************************/

/*******************************************************************************
* Function Name	: normandy_reg_write
* Description	: write chip register
* Input			: PST_TP_DEV p_dev(pointer to a touch panel deice, which should be initialized before)
* Input			: u16 addr(address)
* Input			: u32* p_buf(data buffer)
* Input			: u16 buf_len(buffer length)
* Output		: none
* Return		: u32(1:ok 0:fail)
*******************************************************************************/
extern s32 normandy_reg_write(IN PST_TP_DEV p_dev, IN u16 addr,
				IN u8 *p_buf, IN u16 buf_len)
{
	s32 ret = 0;
	u8 retry_cnt = 3;
	u8 i = 0;
	ret = normandy_doze_mode(p_dev, 0, 0);
	if (0 == ret) {
		board_print_error
			("[normandy_reg_write]normandy_doze_mode(p_dev,0,0) fail!\n");
		return ret;
	}

	for (i = 0; i < retry_cnt; i++) {
		ret = board_write_chip_reg(p_dev, addr, p_buf, buf_len);
		if (1 == ret) {
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: normandy_reg_read
* Description	: read chip register
* Input			: PST_TP_DEV p_dev(pointer to a touch panel deice, which should be inited before)
* Input			: u16 addr(address)
* Input			: u32* p_buf(data buffer)
* Input			: u16 buf_len(buffer length)
* Output		: none
* Return		: u32(1:ok 0:fail)
*******************************************************************************/
extern s32 normandy_reg_read(IN PST_TP_DEV p_dev, IN u16 addr,
				IN_OUT u8 *p_buf, IN u16 buf_len)
{
	s32 ret = 0;
	u8 retry_cnt = 3;
	u8 i = 0;
	ret = normandy_doze_mode(p_dev, 0, 0);
	if (0 == ret) {
		board_print_error
			("[normandy_reg_read]normandy_doze_mode(p_dev,0,0) fail!\n");
		return ret;
	}
	for (i = 0; i < retry_cnt; i++) {
		ret = board_read_chip_reg(p_dev, addr, p_buf, buf_len);
		if (1 == ret) {
			break;
		}
	}
	return ret;
}
/*******************************************************************************
* Function Name	: normandy_wakeup_chip
* Description	: normandy wakeup chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_wakeup_chip(PST_TP_DEV p_dev)
{
	return reset_chip_proc(p_dev, RESET_TO_AP);
}
/*******************************************************************************
* Function Name	: normandy_soft_reset_chip
* Description	: normandy soft reset chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_soft_reset_chip(IN PST_TP_DEV p_dev)
{
	s32 ret = 1;
	u8 i = 0, retry_cnt = 3;
	u8 hold_reg_val = 0x01;
	for (i = 0; i < retry_cnt; i++) {
		ret = chip_reg_write(p_dev, 0x4180, &hold_reg_val, 1);
		if (1 == ret) {
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: normandy_reset_hold_chip
* Description	: normandy reset hold chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_reset_hold_chip(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	/* (3) reset ic*/
	ret = board_hard_reset_chip(p_dev);
	if (1 == ret) {
		ret = normandy_hold_dsp(p_dev);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: normandy_force_update_cfg
* Description	: force update cfg (version)
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: u16 cfg_len
* Output		: OUT u8* config
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_force_update_cfg(IN PST_TP_DEV p_dev,
					IN u8 *config, IN u16 cfg_len)
{
	s32 ret = 0;
	u8 ver_tmp = 0;
	u8 i = 0;
	u8 retry_cnt = 3;

	for (i = 0; i < retry_cnt; i++) {
		ret =
			chip_reg_read(p_dev, p_dev->cfg_start_addr,
				&ver_tmp, 1);
		if (1 == ret) {
			break;
		}
	}
	if (0 == ret) {
		board_print_error("%s\n",
				"[normandy_force_update_cfg],read cfg version fail");
		goto UPDATE_CFG_END;
	}
	if (config[0] < ver_tmp && config[0] != 0) {
		config[0] = 0xFF;
	}

	for (i = 0; i < retry_cnt; i++) {
		normandy_cfg_update_chksum(config, cfg_len);
		ret = normandy_write_cfg_arr(p_dev, config, cfg_len);
		if (1 == ret) {
			break;
		}
	}
	if (0 == ret) {
		board_print_error("%s\n",
				"[normandy_force_update_cfg],write cfg fail");
		goto UPDATE_CFG_END;
	}

UPDATE_CFG_END:
	return ret;
}

/*******************************************************************************
* Function Name	: normandy_write_cfg
* Description	: normandy write config to tp chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_write_cfg(IN_OUT PST_TP_DEV p_dev)
{
	return normandy_write_cfg_arr(p_dev, p_dev->cfg, p_dev->cfg_len);
}

/*******************************************************************************
* Function Name	: normandy_read_cfg
* Description	: read config from tp normandy chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: PST_TP_DEV p_dev(touch panel deice)
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_read_cfg(IN_OUT PST_TP_DEV p_dev)
{
	return normandy_read_cfg_arr(p_dev, p_dev->cfg, p_dev->cfg_len);
}

/*******************************************************************************
* Function Name	: normandy_write_cfg_arr
* Description	: write config from tp normandy chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: u32* config
* Input			: u16 cfg_len
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_write_cfg_arr(IN PST_TP_DEV p_dev, IN u8 *config,
				IN u16 cfg_len)
{
	s32 ret = 1;
	u8 buf_arr[10];
	u8 retry_cnt = 10;
	u8 retry_cnt1 = 10;
	u8 i = 0;
	u8 j = 0;
	u8 tmp_write_cmd = 0;
	u8 write_wait_cmd = 0;

	/*----------------------------------------enter idle ------------------------------------------*/
	buf_arr[0] = 0xFF;
	buf_arr[1] = 0x00;
	buf_arr[2] = 0x100 - buf_arr[0] - buf_arr[1];
	ret = chip_reg_write(p_dev, p_dev->real_cmd_addr, buf_arr, 3);
	if (0 == ret) {
		board_print_error("%s\n",
				"write 0xFF to real_cmd_addr fail!");
		goto READ_END;
	}
	/*----------------------------------------scan idle status------------------------------------------*/
	for (i = 0; i < retry_cnt; i++) {
		buf_arr[0] = 0x00;
		ret = chip_reg_read(p_dev, p_dev->real_cmd_addr, buf_arr, 1);
		if (1 == ret && 0xFF == buf_arr[0]) {
			break;
		} else {
			ret = 0;
		}
		board_delay_ms(10);
	}
	if (0 == ret) {
		board_print_error("%s\n", "fail to wait idle status!");
		goto READ_END;
	}

	/*-------------------------------------send write cfg cmd--------------------------------------*/
	if (cfg_len > 32) {
		tmp_write_cmd = 0x80;
		write_wait_cmd = 0x82;
	} else {
		tmp_write_cmd = 0x81;
		write_wait_cmd = 0x81;
	}
	retry_cnt = 3;
	for (i = 0; i < retry_cnt; i++) {
		/*send write cfg cmd*/
		buf_arr[0] = tmp_write_cmd;
		buf_arr[1] = 0x00;
		buf_arr[2] = 0x100 - buf_arr[0] - buf_arr[1];
		ret = chip_reg_write(p_dev, p_dev->real_cmd_addr, buf_arr, 3);
		if (1 == ret) {
			board_delay_ms(50);
			/*wait ready status*/
			retry_cnt1 = 50;
			for (j = 0; j < retry_cnt1; j++) {
				buf_arr[0] = 0x00;
				ret = chip_reg_read(p_dev, p_dev->real_cmd_addr,
						buf_arr, 1);
				if (1 == ret
					&& write_wait_cmd == buf_arr[0]) {
					break;
				} else {
					ret = 0;
				}
				board_delay_ms(10);
			}
			if (ret == 1) {
				break;
			}
		} else if (i == retry_cnt - 1) {
			board_print_error
				("fail to send write cfg cmd!");
			return 0;
		}
	}
	if ((0 == ret) && (j >= retry_cnt1)) {
		board_print_error("%s\n", "fail to wait ready status!");
		goto READ_END;
	}

	/*-------------------------------------- write cfg ------------------------------------------*/
	retry_cnt = 3;
	for (i = 0; i < retry_cnt; i++) {
		ret = chip_reg_write(p_dev, p_dev->cfg_start_addr, config, cfg_len);
		if (1 == ret) {
			break;
		}
	}
	if (0 == ret) {
		board_print_error("%s\n", "fail to write cfg!");
		goto READ_END;
	}
	/*---------------------------------notify write completion------------------------------------*/
	buf_arr[0] = 0x83;
	buf_arr[1] = 0x00;
	buf_arr[2] = 0x100 - buf_arr[0] - buf_arr[1];
	ret = chip_reg_write(p_dev, p_dev->real_cmd_addr, buf_arr, 3);
	if (0 == ret) {
		board_print_error("%s\n",
				"fail to notify write cfg completion!");
		goto READ_END;
	}
	/*---------------------------------wait idle------------------------------------*/
	board_delay_ms(50);
	retry_cnt = 3;
	for (i = 0; i < retry_cnt; i++) {
		ret = chip_reg_read(p_dev, p_dev->real_cmd_addr, buf_arr, 1);
		if (1 == ret && 0xFF == buf_arr[0]) {
			break;
		} else {
			ret = 0;
		}
		board_delay_ms(1);
	}
	if (0 == ret) {
		board_print_error("%s\n", "fail to wait idle status!");
		goto READ_END;
	}
	p_dev->doze_ref_cnt = 1;
	/*for fw store config to fresh*/
	board_delay_ms(50);
READ_END:
	return ret;
}

/*******************************************************************************
* Function Name	: normandy_read_cfg_arr
* Description	: read config from tp normandy chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: u16 cfg_len
* Output		: u8* config
* Return		: s32(0:fail >0:read cfg len)
*******************************************************************************/
extern s32 normandy_read_cfg_arr(IN PST_TP_DEV p_dev,
				IN_OUT u8 *config, IN u16 cfg_len)
{
	s32 ret = 1;
	u8 buf_arr[10];
	u8 retry_cnt = 10;
	u8 retry_cnt1 = 0;
	u8 i = 0;
	u8 j = 0;
	u8 sub_bags = 0;
	u16 offset = 0;
	/*enter idle */
	buf_arr[0] = 0xFF;
	buf_arr[1] = 0x00;
	buf_arr[2] = 0x100 - buf_arr[0] - buf_arr[1];
	board_print_debug("start read cfg!");
	ret = chip_reg_write(p_dev, p_dev->real_cmd_addr, buf_arr, 3);
	if (0 == ret) {
		board_print_error("%s\n",
				"write 0xFF to real_cmd_addr fail!");
		return 0;
	}
	/*scan idle status*/
	for (i = 0; i < retry_cnt; i++) {
		buf_arr[0] = 0x00;
		ret = chip_reg_read(p_dev, p_dev->real_cmd_addr, buf_arr, 1);
		if (1 == ret && 0xFF == buf_arr[0]) {
			break;
		} else {
			ret = 0;
		}
		board_delay_ms(10);
	}
	if (0 == ret) {
		board_print_error("%s\n", "fail to wait idle status!");
		return 0;
	}

	retry_cnt = 5;
	for (i = 0; i < retry_cnt; i++) {
		/*send read cfg cmd*/
		buf_arr[0] = 0x86;
		buf_arr[1] = 0x00;
		buf_arr[2] = 0x100 - buf_arr[0] - buf_arr[1];
		ret = chip_reg_write(p_dev, p_dev->real_cmd_addr, buf_arr, 3);
		if (ret == 1) {
			ret = 0;
			/*wait for firmware ready for reading cfg*/
			retry_cnt1 = 50;
			for (j = 0; j < retry_cnt1; j++) {
				buf_arr[0] = 0x00;
				ret = chip_reg_read(p_dev, p_dev->real_cmd_addr,
						buf_arr, 1);
				if (1 == ret && 0x85 == buf_arr[0]) {
					break;
				} else {
					ret = 0;
				}
				board_delay_ms(10);
			}
			if (ret == 1) {
				break;
			}
		} else if (i == retry_cnt - 1) {
			board_print_error("fail to send read cfg end!");
			return 0;
		}
	}
	if ((0 == ret) && (j >= retry_cnt1)) {
		board_print_error("%s\n",
				"fail to wait for firmware ready for reading cfg!");
		return 0;
	}

	/*read cfg*/
	retry_cnt = 3;
	if (cfg_len == 0) {	/*config len is not known*/
		/*read config head*/
		for (i = 0; i < retry_cnt; i++) {
			ret = chip_reg_read(p_dev, p_dev->cfg_start_addr, config, 4);
			if (ret == 1) {
				break;
			}
		}
		if (0 == ret) {
			board_print_error("%s\n",
					"fail to read cfg head!");
			return 0;
		}
		/*read sub bugs*/
		sub_bags = config[2];
		offset = 4;
		for (j = 0; j < sub_bags; j++) {
			/*read sub bags head*/
			for (i = 0; i < retry_cnt; i++) {
				ret = chip_reg_read(p_dev,
						p_dev->
						cfg_start_addr +
						offset,
						config + offset, 2);
				if (ret == 1) {
					break;
				}
			}
			if (0 == ret) {
				board_print_error
					("fail to read cfg sub bug %d head!\n",
					j);
				return 0;
			}
			/*read sub bugs*/
			for (i = 0; i < retry_cnt; i++) {
				ret = chip_reg_read(p_dev,
						p_dev->
						cfg_start_addr +
						offset + 2,
						config + offset + 2,
						config[offset + 1] +
						1);
				if (ret == 1) {
					break;
				}
			}
			if (0 == ret) {
				board_print_error
					("fail to read cfg sub bug %d!\n",
					j);
				return 0;
			}
			board_print_debug
				("read sub bug %d offset is %d\n", j,
				offset);
			offset += config[offset + 1] + 3;
		}
		cfg_len = offset;
		board_print_debug("chip cfg len: %d\n", cfg_len);

	} else {
		for (i = 0; i < retry_cnt; i++) {
			ret =
				chip_reg_read(p_dev, p_dev->cfg_start_addr,
					config, cfg_len);
			if (1 == ret) {
				break;
			}
		}
		if (0 == ret) {
			board_print_error("%s\n", "fail to read cfg!");
			return 0;
		}
	}

	/*write idle cmd*/
	buf_arr[0] = 0xFF;
	buf_arr[1] = 0x00;
	buf_arr[2] = 0x100 - buf_arr[0] - buf_arr[1];
	ret = chip_reg_write(p_dev, p_dev->real_cmd_addr, buf_arr, 3);
	if (0 == ret) {
		board_print_error("%s\n", "fail to write idle cmd!");
		return 0;
	}

	return cfg_len;
}

/*******************************************************************************
* Function Name	: normandy_cfg_update_chksum
* Description	: update check sum
* Input			: u32* config
* Input			: u16 cfg_len
* Output		: u8* config
* Return		: none
*******************************************************************************/
extern void normandy_cfg_update_chksum(IN_OUT u8 *config,
					IN u16 cfg_len)
{
	u16 pack_map_len_arr[100];
	u16 packNum = 0;
	u16 pack_len_tmp = 0;
	u16 pack_id_tmp = 0;
	u16 i = 0, j = 0;
	u16 cur_pos = 0;
	u8 check_sum = 0;

	if (cfg_len < 4)
		return;

	pack_map_len_arr[pack_id_tmp] = 4;
	packNum = config[2];
	for (i = 4; i < cfg_len;) {
		pack_id_tmp++;
		pack_len_tmp = config[i + 1] + 3;
		pack_map_len_arr[pack_id_tmp] = pack_len_tmp;
		i += pack_len_tmp;
	}

	cur_pos = 0;
	for (i = 0; i <= pack_id_tmp; i++) {
		check_sum = 0;
		for (j = cur_pos; j < cur_pos + pack_map_len_arr[i] - 1;
			j++) {
			check_sum += config[j];
		}
		config[cur_pos + pack_map_len_arr[i] - 1] =
			(u8) (0 - check_sum);
		cur_pos += pack_map_len_arr[i];
	}
}

/*******************************************************************************
* Function Name	: normandy_cmp_cfg
* Description	: normandy_cmp_cfg
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: u32* cfg1
* Input			: u32* cfg2
* Input			: u16 cfg_len
* Output		: OUT u8* config
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_cmp_cfg(IN PST_TP_DEV p_dev, IN u8 *cfg1,
				IN u8 *cfg2, IN u16 cfg_len)
{
	s32 ret = 1;
	u16 i = 0;
	for (i = 0; i < cfg_len; i++) {
		if (0 == i || 1 == i || 3 == i)	/*||
			//cfg_len - 1 == i)*/
		{
			continue;
		}
		if (cfg1[i] != cfg2[i]) {
			ret = 0;
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: normandy_doze_mode
* Description	: normandy_doze_mode
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: u32 b_ena
* Input			: u32 b_force
* Output		: none
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
extern s32 normandy_doze_mode(IN PST_TP_DEV p_dev, IN u8 b_ena,
				IN u8 b_force)
{
	s32 ret = 1;
	u8 temp_data = 0;
	u8 temp_read = 0;
	u8 i = 0;
	u8 j = 0;
	u8 retry_cnt = 20;
	if (b_ena) {	/*enter doze*/

		if (p_dev->doze_ref_cnt == 0 || b_force) {
			temp_data = 0xCC;
			for (i = 0; i < retry_cnt; i++) {
				ret = board_write_chip_reg(p_dev, 0x30f0, &temp_data, 1);
				if (1 == ret) {
					break;
				}
				board_delay_ms(1);
			}
			if (1 == ret && 0 == p_dev->doze_ref_cnt) {
				p_dev->doze_ref_cnt++;
			}
			if (0 == ret) {
				board_print_error("%s\n",
						"[normandy_doze_mode]board_write_chip_reg 0xCC in 0x30f0 fail!");
			}
		}
	} else {	/*exit doze*/

		if (p_dev->doze_ref_cnt == 1 || b_force) {
			for (i = 0; i < retry_cnt; i++) {
				if (0xAA != temp_read) {	/*only 0x3100 is not 0xAA and need write 0x30f0 0xAA */
					temp_data = 0xAA;
					ret = board_write_chip_reg(p_dev,
								0x30f0,
								&temp_data,
								1);
					if (ret == 0) {
						continue;
					}
					board_delay_ms(1);
				}

				for (j = 0; j < retry_cnt; j++) {
					ret = board_read_chip_reg(p_dev,
								0x3100, &temp_read, 1);
					if ((1 == ret
						&& 0xBB == temp_read)) {
						break;
					}
					if ((1 == ret
						&& 0xAA != temp_read)) {
						ret = 0;
						break;	/*if 0x3100 is not 0xAA or 0xBB,rewrite 0x30F0 0xAA*/
					}
					ret = 0;
					board_delay_ms(5);
				}
				if ((1 == ret && 0xBB == temp_read)) {
					break;
				}
			}
			if (1 == ret && 1 == p_dev->doze_ref_cnt) {
				p_dev->doze_ref_cnt--;
			}
			if (0 == ret) {
				board_print_error("%s\n",
						"[normandy_doze_mode]board_write_chip_reg 0xAA in 0x30f0 fail!");
			}
		}
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

/*******************************************************************************
* Function Name	: normandy_hold_dsp
* Description	: normandy hold dsp
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u32(0:fail 1:ok)
*******************************************************************************/
static s32 normandy_hold_dsp(IN PST_TP_DEV p_dev)
{
	/* if failed,retry, for max times 10*/
	s32 ret = 0;
	u8 index = 0;
	u8 write_value = 0x24;
	u8 buf_tmp[2];
	buf_tmp[0] = write_value;
	for (index = 0; index < 10; index++) {
		ret = chip_reg_write(p_dev, 0x2180, buf_tmp, 1);
		if (1 == ret) {
			ret = chip_reg_read(p_dev, 0x2180, &(write_value), 1);
			if (1 == ret && write_value == buf_tmp[0]) {
				ret = 1;
				break;
			}
		}
	}
	return ret;
}

#endif

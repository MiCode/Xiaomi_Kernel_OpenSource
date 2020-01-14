/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name			: test_order.h
* Author			:
* Version			: V1.0.0
* Date				: 03/05/2018
* Description		: parse XML order
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "chip_ini.h"
#include "test_order.h"
#include "crc_decrypt.h"
#include "simple_mem_manager.h"
#include "tp_dev_control.h"
#include "generic_func.h"

#include "generic_func.h"
#include "extra_tp_control.h"
#include "tp_dev_control.h"

#include "config.h"
#include "mxml.h"

static mxml_node_t *top;

/*******************************************************************************
* Function Name	: get_rawdata_len
* Description	: get rawdata len
* Input			: PST_TP_DEV p_tp_dev
* Output		: PST_TP_DEV p_tp_dev
* Return		: none
*******************************************************************************/
static void get_rawdata_len(PST_TP_DEV p_tp_dev)
{
	p_tp_dev->rawdata_len =
		p_tp_dev->total_drv_num * p_tp_dev->total_sen_num;
	p_tp_dev->key_rawdata_num =
		p_tp_dev->total_drv_num * p_tp_dev->total_sen_num -
		p_tp_dev->sc_sen_num * p_tp_dev->sc_drv_num;
}
/*******************************************************************************
* Function Name	: get_key_pos_map
* Description	: get key position
* Input			: PST_TP_DEV p_tp_dev
				: u8* key_pos
				: u8 key_num_in_port
				: u8 drv_flag
* Output		: PST_TP_DEV p_tp_dev
* Return		: none
*******************************************************************************/
static void get_key_pos_map(PST_TP_DEV p_tp_dev,
					u8 *key_pos,
					u8 key_num_in_port,
					u8 drv_flag) {
	u8 i = 0;
	u8 area_key = 0;
	u8 tmp_pos = 0;
	u8 key_num = 0;
	u8 depend_key_rat = 8;
	if (p_tp_dev->chip_type == TP_ALTO) {
		depend_key_rat = 6;
	}
	for (i = 0; i < strlen((const char *)key_pos); i++) {
		if (key_pos[i] % depend_key_rat != 0) {
			area_key = 1;
			break;
		}
	}
	if (area_key == 1) {
		p_tp_dev->key_num =
			p_tp_dev->total_drv_num * p_tp_dev->total_sen_num -
			p_tp_dev->sc_sen_num * p_tp_dev->sc_drv_num;
		for (i = 0; i < p_tp_dev->key_num; i++) {
			p_tp_dev->key_pos_arr[i] = i;
		}
	} else {
		key_num = 0;
		for (i = 0; i < strlen((const char *)key_pos); i++) {
			if (key_pos[i] != 0) {

				if (drv_flag) {
					tmp_pos =
						key_pos[i] /
						depend_key_rat +
						p_tp_dev->sc_sen_num * (i /
						key_num_in_port)
						- 1;
				} else {
					tmp_pos =
						key_pos[i] /
						depend_key_rat +
						p_tp_dev->sc_drv_num * (i /
							key_num_in_port)
						- 1;
				}
				p_tp_dev->key_pos_arr[key_num] =
					tmp_pos;
				key_num++;
			}
		}
		p_tp_dev->key_num = key_num;
	}
}

/*******************************************************************************
* Function Name	: get_sc_drv_sen_key_num
* Description	: get sc_drv_num sc_sen_num key_num
* Input			: PST_TP_DEV p_tp_dev
* Input			: u8 key_start_addr
				: u8 key_en
				：u8 sen_as_key
				：u8 key_com_port_num
				：u8 max_key_num
* Output		: PST_TP_DEV p_tp_dev
* Return 		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 get_sc_drv_sen_key_num(PST_TP_DEV p_tp_dev,
				u8 key_start_addr, u8 key_en,
				u8 sen_as_key, u8 key_com_port_num,
				u8 max_key_num)
{
	s32 ret = 1;
	u8 key_pos[TP_MAX_KEY_NUM];
	u8 max_key_num_in_com_port = 0;
	u8 drv_flag = 0;

	p_tp_dev->sc_drv_num = p_tp_dev->total_drv_num;
	p_tp_dev->sc_sen_num = p_tp_dev->total_sen_num;
	/* screen sensor and driver num */
	if (sen_as_key == 0) {
		drv_flag = 1;

		if (key_com_port_num == 1) {
			max_key_num_in_com_port = max_key_num / 2;
			p_tp_dev->sc_drv_num =
				p_tp_dev->total_drv_num - key_en * 2;
		} else {
			max_key_num_in_com_port = max_key_num;
			p_tp_dev->sc_drv_num =
				p_tp_dev->total_drv_num - key_en;
		}
	} else {
		if (key_com_port_num == 1) {
			max_key_num_in_com_port = max_key_num / 2;
			p_tp_dev->sc_sen_num =
				p_tp_dev->total_sen_num - key_en * 2;
		} else {
			max_key_num_in_com_port = max_key_num;
			p_tp_dev->sc_sen_num =
				p_tp_dev->total_sen_num - key_en;
		}
	}
	board_print_debug("sc drv num is:%d\n", p_tp_dev->sc_drv_num);
	board_print_debug("sc sen num is:%d\n", p_tp_dev->sc_sen_num);
	get_rawdata_len(p_tp_dev);
	if (p_tp_dev->key_rawdata_num == 0) {
		p_tp_dev->key_num = 0;
	} else {
		/*read key_pos*/
		ret =
			chip_reg_read(p_tp_dev, key_start_addr, key_pos,
				sizeof(key_pos));
		if (ret == 0) {
			board_print_error("read key_pos faild!\n");
			return 0;
		}
		get_key_pos_map(p_tp_dev, key_pos,
				max_key_num_in_com_port, drv_flag);
	}
	board_print_debug("key_num:%d\n", p_tp_dev->key_num);
	return ret;
}

/*******************************************************************************
* Function Name	: get_data_step
* Description	: get data step
* Input			: u8 *data_str
* Output		:
* Return		: s32(data step)
*******************************************************************************/
static s32 get_data_step(cu8 *data_str)
{
	s32 step = 0;
	if (data_str[0] == 'T') {
		step = 4;
	} else {
		step = 3;
	}
	return step;
}

/*******************************************************************************
* Function Name	: get_step4_abs_addr
* Description	: get_step4_abs_addr
* Input			: PST_TP_DEV p_dev
* Output		: PST_ADDR_POS_INFO p_addr_info
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
static s32 get_step4_abs_addr(PST_TP_DEV p_dev,
				PST_ADDR_POS_INFO p_addr_info,
				cu8 *config)
{
	s32 ret = 1;
	cu8 *sub_bag_ptr = NULL;
	u8 i;

	sub_bag_ptr = &config[4];
	for (i = 0; i < config[2]; i++) {
		if (sub_bag_ptr[0] == p_addr_info->package_id)
			break;
		sub_bag_ptr += sub_bag_ptr[1] + 3;
	}

	if (i >= config[2]) {
		board_print_error
			("Cann't find the specifiled bag num %d\n",
			p_addr_info->package_id);
		return 0;
	}

	if (sub_bag_ptr[1] + 3 < p_addr_info->offset + 1) {
		board_print_debug
			("Sub bag len less then you want to read: %d < %d,pacakge id:%d,offset:%d,lsb:%d,hsb:%d\n",
			sub_bag_ptr[1] + 3, p_addr_info->offset + 1,
			p_addr_info->package_id, p_addr_info->offset,
			p_addr_info->lsb, p_addr_info->hsb);
		return 0;
	}

	p_addr_info->addr =
		p_dev->cfg_start_addr + (sub_bag_ptr + p_addr_info->offset -
				config);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_step3_addr_data
* Description	: parse_step3_addr_data
* Input			: cu8* string_info
* Input			: PST_ADDR_POS_INFO p_addr1_info
* Input			: PST_ADDR_POS_INFO p_addr2_info
* Output		: PST_ADDR_POS_INFO p_addr1_info
* Output		: PST_ADDR_POS_INFO p_addr2_info
* Return		: s32(0:fail 1,2:ok,1:p_addr1_info 2:p_addr1_info,p_addr2_info)
*******************************************************************************/
static s32 parse_step3_addr_data(PST_TP_DEV p_dev, cu8 *string_info,
				PST_ADDR_POS_INFO p_addr1_info,
				PST_ADDR_POS_INFO p_addr2_info)
{
	s32 ret = 0;
	u8 str_tmp[128];
	u8 string_info_len = strlen((const s8 *)string_info);
	u8 *field = NULL;
	if (NULL == string_info || string_info_len == 0) {
		board_print_error("parse str is null!\n");
		return ret;
	}
	if (string_info_len > sizeof(str_tmp)) {
		board_print_error("ini addr str is error:%s!\n",
				string_info);
		return ret;
	}
	memcpy(str_tmp, string_info, string_info_len);
	str_tmp[string_info_len] = '0';

	if ((strlen((const char *)str_tmp) == 0)) {
		return ret;
	}

	field = (u8 *) strsep((char **)&str_tmp, ",");
	if (field == NULL) {
		board_print_error("get addr1 null!\n");
		return ret;
	}
	p_addr1_info->addr = atohex(field, strlen((const char *)field));

	field = (u8 *) strsep((char **)&str_tmp, ",");
	if (field == NULL) {
		return ret;
	}
	p_addr1_info->lsb = atoi((char const *)field);

	field = (u8 *) strsep((char **)&str_tmp, ",");
	if (field == NULL) {
		return ret;
	}
	p_addr1_info->hsb = atoi((char const *)field);
	ret++;

	if (p_addr2_info != NULL) {
		field = (u8 *) strsep((char **)&str_tmp, ",");
		if (field == NULL) {
			board_print_debug
				("get addr2 null, parse str:%s!\n",
				string_info);
			return ret;
		}
		p_addr2_info->addr =
			atohex(field, strlen((const char *)field));

		field = (u8 *) strsep((char **)&str_tmp, ",");
		if (field == NULL) {
			return ret;
		}
		p_addr2_info->lsb = atoi((char const *)field);

		field = (u8 *) strsep((char **)&str_tmp, ",");
		if (field == NULL) {
			return ret;
		}
		p_addr2_info->hsb = atoi((char const *)field);
		ret++;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: parse_step4_addr_data
* Description	: get data with package id and offset
* Input			: cu8* string_info
* Output		: PST_ADDR_POS_INFO p_addr1_info
* Output		: PST_ADDR_POS_INFO p_addr2_info
* Return		: s32(0:fail 1,2:ok,1:p_addr1_info 2:p_addr1_info,p_addr2_info)
*******************************************************************************/
static s32 parse_step4_addr_data(PST_TP_DEV p_dev, cu8 *string_info,
				PST_ADDR_POS_INFO p_addr1_info,
				PST_ADDR_POS_INFO p_addr2_info)
{
	s32 ret = 0;
	s32 free_mem = 0;
	u8 *field = NULL;
	u8 *str_tmp = NULL;
	u8 string_info_len = 0;

	string_info_len = strlen((const s8 *)string_info);
	if (string_info_len == 0) {
		board_print_error("string_info len is invalid!\n");
		return ret;
	}
	/*board_print_debug("string_info len is: %d!\n",string_info_len);*/
	ret = alloc_mem_in_heap((void **)&str_tmp, string_info_len + 1);
	if (0 == ret) {
		board_print_error("string_info malloc error!\n");
		free_mem_in_heap((void *)str_tmp);
		return ret;
	}
	memcpy(str_tmp, string_info, string_info_len);
	str_tmp[string_info_len] = '\0';

	/*sub package id*/
	field = (u8 *) strsep((char **)&str_tmp, ",");
	if (field == NULL) {
		board_print_error("get addr1 null!\n");
		goto mallc_exit;
	}
	p_addr1_info->package_id = atoi((const char *)&field[1]);

	/*offset*/
	field = (u8 *) strsep((char **)&str_tmp, ",");
	if (field == NULL) {
		goto mallc_exit;
	}
	p_addr1_info->offset = atoi((char const *)field);

	/*low bit*/
	field = (u8 *) strsep((char **)&str_tmp, ",");
	if (field == NULL) {
		goto mallc_exit;
	}
	p_addr1_info->lsb = atoi((char const *)field);

	/*high bit*/
	field = (u8 *) strsep((char **)&str_tmp, ",");
	if (field == NULL) {
		goto mallc_exit;
	}
	p_addr1_info->hsb = atoi((char const *)field);
	ret++;

	field = (u8 *) strsep((char **)&str_tmp, ",");
	if ((p_addr2_info != NULL) && (field != NULL)) {
		if (field == NULL) {
			board_print_debug
				("get addr2 null!,parse str:%s\n",
				string_info);
			goto mallc_exit;
		}
		p_addr2_info->package_id =
			atoi((const char *)&field[1]);

		field = (u8 *) strsep((char **)&str_tmp, ",");
		if (field == NULL) {
			goto mallc_exit;
		}
		p_addr2_info->offset = atoi((char const *)field);

		field = (u8 *) strsep((char **)&str_tmp, ",");
		if (field == NULL) {
			goto mallc_exit;
		}
		p_addr2_info->lsb = atoi((char const *)field);

		field = (u8 *) strsep((char **)&str_tmp, ",");
		if (field == NULL) {
			goto mallc_exit;
		}
		p_addr2_info->hsb = atoi((char const *)field);
		ret++;
	}

mallc_exit:
	free_mem = free_mem_in_heap((void *)str_tmp);
	board_print_debug("free_mem_in_heap return %d \n", free_mem);
	str_tmp = NULL;
	return ret;
}

/*******************************************************************************
* Function Name	: parse_addr_data
* Description	: parse_addr_data
* Input			: u8* string_info
* Input			: PST_ADDR_POS_INFO p_addr1_info
* Input			: PST_ADDR_POS_INFO p_addr2_info
* Input			: u8 data_step
* Output		: PST_ADDR_POS_INFO p_addr1_info
* Output		: PST_ADDR_POS_INFO p_addr2_info
* Return		: s32(0:fail 1,2:ok,1:p_addr1_info 2:p_addr1_info,p_addr2_info)
*******************************************************************************/
static s32 parse_addr_data(PST_TP_DEV p_dev, u8 *string_info,
				PST_ADDR_POS_INFO p_addr1_info,
				PST_ADDR_POS_INFO p_addr2_info,
				u8 data_step)
{
	getrid_space((s8 *) string_info,
			strlen((const char *)string_info));
	if (data_step == 3) {
		return parse_step3_addr_data(p_dev, string_info,
					p_addr1_info,
					p_addr2_info);
	}
	if (data_step == 4) {
		return parse_step4_addr_data(p_dev, string_info,
					p_addr1_info,
					p_addr2_info);
	}
	return 0;
}

/*******************************************************************************
* Function Name	: get_step3_cfg_data
* Description	: get_step3_cfg_data
* Input			: PST_TP_DEV p_tp_dev
* Input			: PST_ADDR_POS_INFO p_addr_info
* Input			: cu8* config
* Output		: none
* Return 		: u8(data in cfg)
*******************************************************************************/
static u8 get_step3_cfg_data(PST_TP_DEV p_tp_dev,
				PST_ADDR_POS_INFO p_addr_info,
				cu8 *config)
{
	u8 value = 0;
	u8 tmp = 0;
	tmp = (1 << (p_addr_info->hsb - p_addr_info->lsb + 1)) - 1;
	tmp &= 0xff;
	value =
		(config[p_addr_info->addr - p_tp_dev->cfg_start_addr] >>
		p_addr_info->lsb) & tmp;
	return value;
}

/*******************************************************************************
* Function Name	: get_step4_cfg_data
* Description	: get_step4_cfg_data
* Input			: PST_TP_DEV p_tp_dev
* Input			: PST_ADDR_POS_INFO p_addr_info
* Input			: cu8* config
* Output		: none
* Return		: u8(data in cfg)
*******************************************************************************/
static u8 get_step4_cfg_data(PST_TP_DEV p_tp_dev,
				PST_ADDR_POS_INFO p_addr_info,
				cu8 *config)
{
	s32 ret = 0;
	u8 value = 0;
	u32 tmp = 0;

	ret = get_step4_abs_addr(p_tp_dev, p_addr_info, config);
	if (ret == 0) {
		return 0;
	}

	tmp = (1 << (p_addr_info->hsb - p_addr_info->lsb + 1)) - 1;
	tmp &= 0xff;
	value =
		((config[p_addr_info->addr - p_tp_dev->cfg_start_addr] >>
		p_addr_info->lsb) & tmp);
	return value;
}

/*******************************************************************************
* Function Name	: get_reg_data
* Description	: get reg data
* Input			: PST_TP_DEV p_tp_dev
* Input			: PST_ADDR_POS_INFO p_addr_info
* Input			: cu8* config
* Input			: u8 data_step
* Output		: none
* Return		: u8(data in reg)
*******************************************************************************/
static u8 get_cfg_data(PST_TP_DEV p_tp_dev,
			PST_ADDR_POS_INFO p_addr_info, cu8 *config,
			u8 data_step)
{
	if (data_step == 3) {
		return get_step3_cfg_data(p_tp_dev, p_addr_info,
					config);
	}
	if (data_step == 4) {
		return get_step4_cfg_data(p_tp_dev, p_addr_info,
					config);
	}
	return 0;
}

/*******************************************************************************
* Function Name	: parse_cfg_value
* Description	: parse_cfg_value
* Input			: PST_TP_DEV p_dev
* Input			: u8* string_info
* Input			: u8* p_config
* Output		: PST_TP_DEV p_tp_dev
* Return		: u32 (data in cfg parsed)
*******************************************************************************/
extern u32 parse_cfg_value(PST_TP_DEV p_dev, u8 *string_info,
				u8 *p_config)
{
	u32 ret = 0;
	s32 step = 0;
	u8 value1 = 0;
	u8 value2 = 0;
	s32 val_num = 0;
	ST_ADDR_POS_INFO addr_info1, addr_info2;
	step = get_data_step(string_info);
	val_num =
		parse_addr_data(p_dev, string_info, &addr_info1,
				&addr_info2, step);
	if (val_num == 0) {
		return 0;
	}
	value1 = get_cfg_data(p_dev, &addr_info1, p_config, step);
	if (val_num >= 2) {
		value2 =
			get_cfg_data(p_dev, &addr_info2, p_config, step);
	}
	ret = value1 + value2;
	return ret;
}

/*******************************************************************************
* Function Name	: parse_addr_from_str
* Description	: parse_addr_from_str
* Input			: PST_TP_DEV p_dev
* Input			: u8* string_info
* Input			: cu8* config
* Output		: none
* Return		: u32 (address)
*******************************************************************************/
extern u32 parse_addr_from_str(PST_TP_DEV p_dev, u8 *string_info,
				cu8 *config)
{
	s32 ret = 0;
	s32 step = 0;
	s32 val_num = 0;
	ST_ADDR_POS_INFO addr_info1, addr_info2;
	step = get_data_step(string_info);
	val_num =
		parse_addr_data(p_dev, string_info, &addr_info1,
				&addr_info2, step);
	if (val_num == 0) {
		return 0;
	}
	if (step == 4) {
		ret = get_step4_abs_addr(p_dev, &addr_info1, config);
		if (ret == 0) {
			return 0;
		}
	}
	return addr_info1.addr;
}

/*******************************************************************************
* Function Name	: disable_hopping
* Description	: disable hopping
* Input			: PST_TP_DEV p_tp_dev
* Input			: u8*str_info
				: u8*cfg
* Output		: PST_TP_DEV p_tp_dev
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 disable_hopping(PST_TP_DEV p_tp_dev, u8 *str_info, u8 *cfg,
				u16 cfg_len)
{
	s32 ret = 1;
	u8 num = 0;
	s32 step = 0;
	u32 hopping_en = 0;
	u8 tmp = 1;
	ST_ADDR_POS_INFO addr_info1;

	step = get_data_step(str_info);
	num =
		parse_addr_data(p_tp_dev, str_info, &addr_info1, NULL,
				step);
	if (num == 0) {
		return 0;
	}
	hopping_en = get_cfg_data(p_tp_dev, &addr_info1, cfg, step);
	if (hopping_en == 1) {
		tmp = (1 << (addr_info1.hsb - addr_info1.lsb + 1)) - 1;
		tmp = tmp << addr_info1.lsb;
		tmp = (~tmp) & 0xff;
		if (step == 4) {
			get_step4_abs_addr(p_tp_dev, &addr_info1, cfg);
		}

		cfg[addr_info1.addr - p_tp_dev->cfg_start_addr] &= tmp;
	}
	update_cfg_checksum(p_tp_dev, cfg, cfg_len);
	return ret;
}

/*******************************************************************************
* Function Name	: init_dev_const_param
* Description	: init dev const param
* Input			: PST_TP_DEV p_tp_dev
* Output		: PST_TP_DEV p_tp_dev
* Return		: void
*******************************************************************************/
extern void init_dev_const_param(PST_TP_DEV p_tp_dev)
{
	p_tp_dev->chip_type = TP_NORMANDY;	/* chip type*/
	p_tp_dev->chip_sub_type = TP_GT9886;	/* chip sub type*/

	/*voltage setting*/
	p_tp_dev->doze_ref_cnt = 1;	/*1:doze mode  0:exit doze*/

	/*cfg*/
	p_tp_dev->cfg_start_addr = 0x6F78;
	p_tp_dev->ext_cfg_addr = 0;
	p_tp_dev->ext_cfg_len = 0;
	p_tp_dev->cfg_len = 690;	/*if ext_cfg_len != 0 then cfg_len = base_cfg_len + ext_cfg_len*/

	p_tp_dev->use_soft_reset = 0;	/* 1 use soft reset first */
	p_tp_dev->no_rst_pin = 0;	/* 1 no rst pin 0 has rst pin default 0*/

	/* max sensor and driver num for die*/
	p_tp_dev->max_sen_num_in_die = 36;
	p_tp_dev->max_drv_num_in_die = 40;

	/* max sensor and driver num for this chip*/
	p_tp_dev->max_sen_num = 36;
	p_tp_dev->max_drv_num = 40;
	/*p_tp_dev->max_key_num = 4;*/

	/*drv channel map*/
	p_tp_dev->map_disable = 0;
	memcpy(p_tp_dev->drv_map, DRV_MAP, sizeof(DRV_MAP));
	memcpy(p_tp_dev->sen_map, SEN_MAP, sizeof(SEN_MAP));

	/*real_cmd_addr*/
	p_tp_dev->real_cmd_addr = 0x6F68;	/*this field may be same as fields in cmd_set*/

	/*rawdata_cmd*/
	p_tp_dev->cmd_set.coorddata_cmd.addr = 0x6F68;
	p_tp_dev->cmd_set.coorddata_cmd.cmd_buf[0] = 0x00;
	p_tp_dev->cmd_set.coorddata_cmd.cmd_buf[1] = 0x00;
	p_tp_dev->cmd_set.coorddata_cmd.cmd_buf[2] = 0x00;
	p_tp_dev->cmd_set.coorddata_cmd.cmd_len = 3;

	p_tp_dev->cmd_set.rawdata_cmd.addr = 0x6F68;
	p_tp_dev->cmd_set.rawdata_cmd.cmd_buf[0] = 0x01;
	p_tp_dev->cmd_set.rawdata_cmd.cmd_buf[1] = 0x00;
	p_tp_dev->cmd_set.rawdata_cmd.cmd_buf[2] = 0xff;
	p_tp_dev->cmd_set.rawdata_cmd.cmd_len = 3;

	p_tp_dev->rawdata_addr = 0x8FA0;
	p_tp_dev->rawdata_len = 0;
	p_tp_dev->rawdata_option =
		_DATA_BIT_16 | _DATA_LARGE_ENDIAN | _DATA_SIGNED |
		_DATA_DRV_SEN_INVERT;

	p_tp_dev->syncflag_addr = 0x4100;
	p_tp_dev->syncflag_mask = 0x80;
}

/*******************************************************************************
* Function Name	: parse_key_data
* Description	: parse key infor
* Input			: PST_TP_DEV p_tp_dev
* Input			: dictionary* p_dict_ini
* Output		: PST_TP_DEV p_tp_dev
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_key_data(PST_TP_DEV p_tp_dev, u8 *cfg)
{
	s32 ret = 1;
	u16 key_start_addr;
	u8 sen_as_key = 0;	/*0:use sen chn as key, 1: use drv chn as key*/
	u8 key_com_port_num = 0;
	u8 key_en = 0;
	u8 max_key_num = 4;

	key_start_addr = parse_addr_from_str(p_tp_dev, KEY_START_ADDR, cfg);
	sen_as_key = parse_cfg_value(p_tp_dev, SEN_AS_KEY_ADDR, cfg);
	key_en = parse_cfg_value(p_tp_dev, KEY_EN, cfg);

	ret = get_sc_drv_sen_key_num(p_tp_dev, key_start_addr, key_en,
				sen_as_key, key_com_port_num, max_key_num);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_cfg
* Description	: prse config
* Input			: PST_TP_DEV p_tp_dev
				: u8*cfg
				: u16 cfg_len
* Output		: PST_TP_DEV p_tp_dev
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_cfg(PST_TP_DEV p_tp_dev, u8 *cfg)
{
	s32 ret = 0;
	p_tp_dev->total_sen_num =
		parse_cfg_value(p_tp_dev, SEN_NUM_ADDR, cfg);
	p_tp_dev->sc_sen_num = p_tp_dev->total_sen_num;
	board_print_debug("total sen num:%d\n",
			p_tp_dev->total_sen_num);
	p_tp_dev->total_drv_num =
		parse_cfg_value(p_tp_dev, DRV_NUM_ADDR, cfg);
	p_tp_dev->sc_drv_num = p_tp_dev->total_drv_num;
	board_print_debug("total drv num:%d\n",
			p_tp_dev->total_drv_num);

	p_tp_dev->sen_start_addr =
		parse_addr_from_str(p_tp_dev, SEN_START_ADDR, cfg);
	p_tp_dev->drv_start_addr =
		parse_addr_from_str(p_tp_dev, DRV_START_ADDR, cfg);
	ret = parse_key_data(p_tp_dev, cfg);
	return ret;
}

/*******************************************************************************
* Function Name	: init_test_config
* Description	: init test config
* Input			: PST_TP_DEV p_tp_dev
				: u8*cfg
				：u16 p_cfg_len
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
static s32 init_test_config(PST_TP_DEV p_tp_dev, u8 *test_cfg,
				u16 *p_cfg_len)
{
	s32 ret = 1;
	if (*p_cfg_len == 0) {
		BOARD_PRINT("switch to original config!\n");
		memcpy(test_cfg, p_tp_dev->cfg, p_tp_dev->cfg_len);
		*p_cfg_len = p_tp_dev->cfg_len;
	} else {
		BOARD_PRINT("switch to test config in order!\n");
		test_cfg[0] = p_tp_dev->cfg[0];
		if (p_tp_dev->chip_type == TP_NORMANDY) {
			test_cfg[1] |= 0x01;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: header_file_init_dev
* Description	: init dev info with chip_ini.h
* Input			: PST_TP_DEV p_tp_dev
* Output		: PST_TP_DEV p_tp_dev
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 header_file_init_dev(PST_TP_DEV p_tp_dev, u8 *cfg,
				u16 *p_cfg_len)
{
	s32 ret = 1;
	/*init const param*/
	init_dev_const_param(p_tp_dev);

	/*read cfg*/
	ret = read_chip_original_cfg(p_tp_dev);
	if (ret == 0) {
		board_print_error("read chip config faild!\n");
		return 0;
	}
	/*init test config*/
	ret = init_test_config(p_tp_dev, cfg, p_cfg_len);
	if (ret == 0) {
		board_print_error("init test config faild!\n");
		return 0;
	}
	/*parse config*/
	ret = parse_cfg(p_tp_dev, cfg);
	if (ret == 0) {
		board_print_error("parse config faild!\n");
		return 0;
	}
	/*disable hopping*/
	ret = disable_hopping(p_tp_dev, HOPPING_EN, cfg, *p_cfg_len);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_init_dev_info
* Description	: parse device info from order
* Input			: PST_TP_DEV p_tp_dev
				: PST_TEST_PARAM p_test_parm
* Output		: PST_TP_DEV p_tp_dev
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_init_dev_info(PST_TP_DEV p_tp_dev,
				PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	mxml_node_t *obj_set = NULL;
	mxml_node_t *chip_ini = NULL;
	mxml_node_t *parent_xnode = NULL;
	u16 chip_type = 0;
	u16 chip_sub_type = 0;

	/* get chip ini content from xml order*/
	obj_set = mxmlFindElement(top, top, "OBJ_SETTING", NULL, NULL, MXML_DESCEND);

	if ((obj_set == NULL) || (obj_set->child == NULL)) {
		board_print_error("can not find OBJ_SETTING in order!\n");
		return 0;
	}

	parent_xnode =
		mxmlFindElement(obj_set, obj_set, "powerVolt", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		p_tp_dev->power_volt = 2800;
	} else {
		p_tp_dev->power_volt =
			str_to_int(parent_xnode->child->value.opaque);
	}
	board_print_debug("power volt:%d!\n", p_tp_dev->power_volt);
	/*find chip type*/
	parent_xnode =
		mxmlFindElement(obj_set, obj_set, "tpMainType", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error("can not find tpMainType in order!\n");
		return 0;
	}
	chip_type = str_to_int(parent_xnode->child->value.opaque);
	if (chip_type != p_tp_dev->chip_type) {
		board_print_error("chip type set in order is error!\n");
		return 0;
	}

	parent_xnode =
		mxmlFindElement(parent_xnode, obj_set, "tpSubType", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error("can not find tpSubType in order!\n");
		return 0;
	}
	chip_sub_type = str_to_int(parent_xnode->child->value.opaque);
	chip_ini = mxmlFindElement(top, top, "chipIni", NULL, NULL, MXML_DESCEND);
	if ((chip_ini == NULL) || (chip_ini->child == NULL)) {
		board_print_error
			("order error,test station should be set as ANDR-MAC!\n");
		return 0;
	}

	memset(p_tp_dev->drv_map, 255, TP_MAX_CHN_NUM);
	memset(p_tp_dev->sen_map, 255, TP_MAX_CHN_NUM);
	memset(p_tp_dev->pack_drv_2_adc_map, 255, TP_MAX_CHN_NUM);
	memset(p_tp_dev->pack_sen_2_adc_map, 255, TP_MAX_CHN_NUM);

	/*init tp device with header file*/
	ret = header_file_init_dev(p_tp_dev, p_test_param->test_cfg, &p_test_param->cfg_len);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_order_config
* Description	: parse order config
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_order_config(PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;
	p_test_param->cfg_len = 0;

	board_print_debug("parse order config!\n");
	ret = alloc_mem_in_heap((void **)&p_test_param->test_cfg, TP_MAX_CFG_LEN);
	if (ret == 0) {
		board_print_error("test config malloc memory error!\n");
		return 0;
	}
	p_test_param->test_cfg[0] = '\0';

	/*find config from top node*/
	parent_xnode = mxmlFindElement(top, top, "chipConfigProc", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find chipConfigProc param in order!\n");
		return 0;
	}
	/*init test config */
	child_xnode = mxmlFindElement(parent_xnode, parent_xnode, "config", NULL,
				NULL, MXML_DESCEND);
	if ((child_xnode != NULL) && (child_xnode->child != NULL)) {
		ret = hexstr_to_array(p_test_param->test_cfg,
				child_xnode->child->value.opaque, TP_MAX_CFG_LEN);
		if (ret > 0) {
			board_print_debug("test cfg len=%d\n", ret);
			p_test_param->cfg_len = ret;
		}
	}

	child_xnode = mxmlFindElement(parent_xnode, parent_xnode, "option", NULL,
			NULL, MXML_DESCEND);
	if ((child_xnode != NULL) && (child_xnode->child != NULL)) {
		p_test_param->send_cfg_option =
			str_to_int(child_xnode->child->value.opaque);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: is_mach_test_item
* Description	: test item
* Input			: u16 test_id
* Output		:
* Return		: s32(0:no 1:yes)
*******************************************************************************/
static s32 is_mach_test_item(u16 test_id)
{
	if (test_id == TP_RAWDATA_TEST_ITEMS_SET_ID) {
		return 1;
	} else if (test_id == TP_SHORT_TEST_ITEM_ID) {
		return 1;
	} else if (test_id == TP_VERSION_TEST_ITEM_ID) {
		return 1;
	} else if (test_id == TP_FLASH_TEST_ITEM_ID) {
		return 1;
	} else if (test_id == TP_CHIP_CFG_PROC_ITEM_ID) {
		return 1;
	} else if (test_id == TP_RECOVER_CFG_ITEM_ID) {
		return 1;
	} else {
		return 0;
	}
}

/*******************************************************************************
* Function Name	: parse_test_item_id
* Description	: parse test item id
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_test_item_id(PST_TEST_PARAM p_test_param)
{
	s32 ret = 0;
	u16 i = 0;
	u16 itest_id = 0xffff;
	u16 test_num = 0;
	u16 sub_id_num = 0;
	mxml_node_t *test_item = NULL;
	mxml_node_t *test_id = NULL;
	mxml_node_t *is_select = NULL;
	mxml_node_t *sub_item = NULL;
	mxml_node_t *parent_xnode = NULL;
	PST_TEST_ITEM_ID_SET p_test_id_set = NULL;
	p_test_param->test_item_num = 0;
	/*定义空节点*/

	/*find testItemSet from top node */
	parent_xnode = mxmlFindElement(top, top, "testItemSet", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_debug("no testItenSet in order file!\n");
		return 0;
	}
	/*malloc memory for test id*/
	ret = alloc_mem_in_heap((void **)&p_test_param->
				p_test_item_id_set,
				MAX_TEST_NUM *
				sizeof(ST_TEST_ITEM_ID_SET));
	if (ret == 0) {
		board_print_error
			("test item id set malloc memory error!\n");
		return 0;
	}
	p_test_id_set = p_test_param->p_test_item_id_set;
	for (i = 0; i < MAX_TEST_NUM; i++) {
		p_test_id_set[i].p_sub_id_set = NULL;
	}

	/*find test item*/
	test_item = mxmlFindElement(parent_xnode, parent_xnode, "testItem",
			NULL, NULL, MXML_DESCEND);
	while ((test_item != NULL) && (test_item->child != NULL)) {
		/*test item is select or not*/
		is_select = mxmlFindElement(test_item, test_item, "bSelected",
					NULL, NULL, MXML_DESCEND);
		if ((is_select != NULL) && (is_select->child != NULL)
			&& (str_to_int(is_select->child->value.opaque) ==
			1)) {
			/*find test id*/
			test_id = mxmlFindElement(test_item, test_item,
					"itemId", NULL, NULL,
					MXML_DESCEND);
			if ((test_id != NULL)
				&& (test_id->child != NULL)) {
				itest_id = str_to_int(test_id->child->value.opaque);
				if (is_mach_test_item(itest_id) != 1) {
					test_item = mxmlFindElement(test_item,
							parent_xnode,
							"testItem",
							NULL, NULL,
							MXML_NO_DESCEND);
					continue;
				}
				board_print_debug("test id :%d\n",itest_id);
				p_test_id_set[test_num].test_id = str_to_int(test_id->child->value.opaque);
				/*find sub test id*/
				sub_item = mxmlFindElement(test_item,
						test_item,
						"subTestItem", NULL,
						NULL, MXML_DESCEND);
				if ((sub_item != NULL)
					&& (sub_item->child != NULL)) {
					sub_id_num = 0;
					ret = alloc_mem_in_heap((void **)
								&p_test_id_set
								[test_num].
								p_sub_id_set,
								sizeof
								(ST_TEST_ITEM_ID_SET));
					if (ret == 0) {
						board_print_error
							("test item id malloc memory error!i=%d\n",
							i);
						return 0;
					}
					p_test_id_set[test_num].p_sub_id_set->len = 0;
					ret = alloc_mem_in_heap((void **)
								&p_test_id_set
								[test_num].
								p_sub_id_set->
								p_id_set,
								MAX_SUBTEST_NUM);
					if (ret == 0) {
						board_print_error
							("sub id malloc memory error!test_id=%d\n",
							p_test_id_set
							[test_num].
							test_id);
						return 0;
					}
				}
				while ((sub_item != NULL)
					&& (sub_item->child != NULL)) {
					is_select = mxmlFindElement(sub_item,
							sub_item,
							"bSelected",
							NULL, NULL,
							MXML_DESCEND);
					board_print_debug
						("sub id is select:%d\n",
						str_to_int(is_select->
						child->value.
						opaque));
					if ((is_select != NULL)
						&& (is_select->child !=
						NULL)
						&&
						(str_to_int
						(is_select->child->value.
						opaque) == 1)) {
						/*find sub item id */
						test_id =
							mxmlFindElement
							(sub_item, sub_item,
							"itemId", NULL,
							NULL,
							MXML_DESCEND);
						if ((test_id != NULL)
							&& (test_id->
							child !=
							NULL)) {
							p_test_id_set
								[test_num].
								p_sub_id_set->
								p_id_set
								[sub_id_num]
								=
								str_to_int
								(test_id->
								child->
								value.
								opaque);
							sub_id_num++;
						}
					}
					/*find next sub test item*/
					sub_item = mxmlFindElement(sub_item,
							test_item,
							"subTestItem",
							NULL, NULL,
							MXML_DESCEND);
				}
				if (p_test_id_set[test_num].
					p_sub_id_set != NULL) {
					p_test_id_set[test_num].
						p_sub_id_set->len =
						sub_id_num;
					board_print_debug
						("sub test num:%d\n",
						sub_id_num);
				}
				test_num++;
			}
		}
		/*find next test item*/
		test_item = mxmlFindElement(test_item, parent_xnode, "testItem",
					NULL, NULL, MXML_NO_DESCEND);
	}
	p_test_param->test_item_num = test_num;
	board_print_debug("test num:%d\n", test_num);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_need_check_node
* Description 	: parse need check node
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: u16(node num)
*******************************************************************************/
extern u16 parse_need_check_node(PST_RAWDATA_TEST_PARAM
				p_raw_test_param)
{
	s32 ret = 0;
	char *tmp_value = NULL;
	u16 data_len = 0;
	mxml_node_t *parent_xnode = NULL;

	parent_xnode = mxmlFindElement(top, top, "needCheckNodeParam", NULL, NULL,
				MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error("please set need check node!\n");
		return 0;
	}

	if (parent_xnode->child == NULL) {
		board_print_error("no need check node!\n");
		return 0;
	}
	tmp_value = parent_xnode->child->value.opaque;
	data_len = (strlen(tmp_value) + 1) / 5 + 1;

	ret = alloc_mem_in_heap((void **)&p_raw_test_param->
				p_need_check_node,
				sizeof(ST_NEED_CHECK_NODE));
	if (ret == 0) {
		board_print_error
			("need check node malloc memory error!\n");
		return 0;
	}
	ret = alloc_mem_in_heap((void **)&p_raw_test_param->
				p_need_check_node->p_data_buf, data_len);
	if (ret == 0) {
		board_print_error
			("need check node malloc memory error!\n");
		return 0;
	}
	/*init need check node*/
	p_raw_test_param->p_need_check_node->data_len = 0;
	memset(p_raw_test_param->p_need_check_node->p_data_buf, 0,
			data_len);
	p_raw_test_param->p_need_check_node->data_len =
		hexstr_to_array(p_raw_test_param->p_need_check_node->
				p_data_buf, tmp_value, data_len);
	return data_len;
}

/*******************************************************************************
* Function Name	: parse_need_check_key
* Description	: parse need check key
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
				: u16 data_len
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_need_check_key(PST_RAWDATA_TEST_PARAM p_raw_test_param,
				u16 data_len)
{
	s32 ret = 0;
	char *tmp_value = NULL;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;
	PST_NEED_CHECK_KEY p_need_check_key = NULL;
	/*judge if need check key*/
	if (p_raw_test_param->p_raw_key_limit == NULL) {
		return 1;
	}

	ret = alloc_mem_in_heap((void **)&p_raw_test_param->
				p_raw_key_limit->p_need_check_key,
				sizeof(ST_NEED_CHECK_KEY));
	if (ret == 0) {
		board_print_error
			("need check key malloc memory error!\n");
		return 0;
	}
	p_need_check_key =
		p_raw_test_param->p_raw_key_limit->p_need_check_key;
	/*init need check key*/
	ret = alloc_mem_in_heap((void **)&p_need_check_key->p_data_buf,
			data_len);
	if (ret == 0) {
		board_print_error
			("need check key buf malloc memory error!\n");
		return 0;
	}
	p_need_check_key->len = 0;
	memset(p_need_check_key->p_data_buf, 0, data_len);

	child_xnode = mxmlFindElement(parent_xnode, top, "needCheckKey", NULL,
				NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_debug
			("no need check key, please set need check key!\n");
		return 1;
	}
	tmp_value = child_xnode->child->value.opaque;
	p_need_check_key->len =
		hexstr_to_array(p_need_check_key->p_data_buf, tmp_value, data_len);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_special_node
* Description	: parse special node
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
				: u16 data_len
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_special_node(PST_RAWDATA_TEST_PARAM p_raw_test_param,
				u16 data_len)
{
	s32 ret = 0;
	u16 i = 0;

	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;
	mxml_node_t *special_xnode = NULL;

	ret = alloc_mem_in_heap((void **)&p_raw_test_param->
			p_special_node, sizeof(ST_RAWDATA_SPECIAL_NODE_PARAM));
	if (ret == 0) {
		board_print_error
			("need check node malloc memory error!\n");
		return 0;
	}

	ret = alloc_mem_in_heap((void **)&p_raw_test_param->
			p_special_node->p_data_buf, data_len * sizeof(ST_SPECIAL_NODE_DEF));
	if (ret == 0) {
		board_print_error
			("need check node malloc memory error!\n");
		return 0;
	}
	/*init sepcial node*/
	p_raw_test_param->p_special_node->data_len = 0;
	special_xnode =
		mxmlFindElement(top, top, "rawdataSpecialNodeParam", NULL,
				NULL, MXML_DESCEND);
	if ((special_xnode == NULL) || (special_xnode->child == NULL)) {
		return 1;
	}

	i = 0;
	parent_xnode = mxmlFindElement(special_xnode, special_xnode, "node", NULL,
			NULL, MXML_DESCEND);
	while ((special_xnode != NULL)
			&& (special_xnode->child != NULL)) {
		/*node id */
		child_xnode = mxmlFindElement(parent_xnode, parent_xnode,
				"nodeId", NULL, NULL, MXML_DESCEND);
		if ((child_xnode == NULL)
			|| (child_xnode->child == NULL)) {
			break;
		}
		p_raw_test_param->p_special_node->p_data_buf[i].
			node_id = str_to_int(child_xnode->child->value.opaque);

		/*high raw limit*/
		child_xnode =
			mxmlFindElement(parent_xnode, parent_xnode,
					"highRawLimit", NULL, NULL, MXML_DESCEND);
		if ((child_xnode == NULL)
			|| (child_xnode->child == NULL)) {
			break;
		}
		p_raw_test_param->p_special_node->p_data_buf[i].
			high_raw_limit =
			str_to_int(child_xnode->child->value.opaque);

		/*low raw limit*/
		child_xnode = mxmlFindElement(parent_xnode, parent_xnode,
					"lowRawLimit", NULL, NULL, MXML_DESCEND);
		if ((child_xnode == NULL)
			|| (child_xnode->child == NULL)) {
			break;
		}
		p_raw_test_param->p_special_node->p_data_buf[i].
			low_raw_limit = str_to_int(child_xnode->child->value.opaque);

		/*accord limit*/
		child_xnode = mxmlFindElement(parent_xnode, parent_xnode,
					"accordLimit", NULL, NULL, MXML_DESCEND);
		if ((child_xnode == NULL)
			|| (child_xnode->child == NULL)) {
			break;
		}
		p_raw_test_param->p_special_node->p_data_buf[i].
			accord_limit = str_to_int(child_xnode->child->value.opaque);

		/*offset limit*/
		child_xnode = mxmlFindElement(parent_xnode, parent_xnode, "offsetLimit", NULL, NULL, MXML_DESCEND);
		if ((child_xnode == NULL)
			|| (child_xnode->child == NULL)) {
			break;
		}
		p_raw_test_param->p_special_node->p_data_buf[i].
			offset_limit = str_to_int(child_xnode->child->value.opaque);

		/*accord option*/
		child_xnode = mxmlFindElement(parent_xnode, parent_xnode,
					"accordOption", NULL, NULL, MXML_DESCEND);
		if ((child_xnode == NULL)
			|| (child_xnode->child == NULL)) {
			break;
		}
		p_raw_test_param->p_special_node->p_data_buf[i].
			accord_option =
			str_to_int(child_xnode->child->value.opaque);
		i++;
		parent_xnode = mxmlFindElement(parent_xnode, special_xnode, "node",
					NULL, NULL, MXML_DESCEND);
	}
	p_raw_test_param->p_special_node->data_len = i;
	return ret;
}

/*******************************************************************************
* Function Name	: parse_raw_max_limit
* Description	: parse raw upper limit
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return 		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_raw_max_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param)
{
	s32 ret = 0;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;
	board_print_debug("parse raw max limit!\n");
	parent_xnode =
		mxmlFindElement(top, top, "rawdataLimitParam", NULL, NULL,
				MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find rawdataLimitParam,please set in order!\n");
		return 0;
	}

	child_xnode =
		mxmlFindElement(parent_xnode, parent_xnode, "upperLimit",
				NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find raw upperLimit,please set in order!\n");
		return 0;
	}

	if (p_raw_test_param->p_rawdata_limit == NULL) {
		ret = alloc_mem_in_heap((void **)&p_raw_test_param->
				p_rawdata_limit, sizeof(ST_RAWDATA_LIMIT_PARAM));
		if (ret == 0) {
			board_print_error
				("rawdata limit malloc memory error!\n");
			return 0;
		}
	}
	p_raw_test_param->p_rawdata_limit->upper_limit =
		str_to_int(child_xnode->child->value.opaque);
	board_print_debug("rawdata upper limit:%d!\n",
			p_raw_test_param->p_rawdata_limit->
			upper_limit);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_raw_min_limit
* Description	: parse raw lower limit
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_raw_min_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param)
{
	s32 ret = 1;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;
	board_print_debug("parse raw lower limit!\n");
	parent_xnode =
		mxmlFindElement(top, top, "rawdataLimitParam", NULL, NULL,
				MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find rawdataLimitParam in order!\n");
		return 0;
	}

	child_xnode = mxmlFindElement(parent_xnode, parent_xnode, "lowerLimit",
				NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find raw lowerLimit,please set raw low limit!\n");
		return 0;
	}

	if (p_raw_test_param->p_rawdata_limit == NULL) {
		ret = alloc_mem_in_heap((void **)&p_raw_test_param->
				p_rawdata_limit, sizeof(ST_RAWDATA_LIMIT_PARAM));
		if (ret == 0) {
			board_print_error
				("rawdata limit malloc memory error!\n");
			return 0;
		}
	}
	p_raw_test_param->p_rawdata_limit->lower_limit =
		str_to_int(child_xnode->child->value.opaque);
	board_print_debug("rawdata lower limit:%d!\n",
			p_raw_test_param->p_rawdata_limit->
			lower_limit);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_accord_limit
* Description	: parse accord limit
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_accord_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param)
{
	s32 ret = 0;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;

	parent_xnode =
		mxmlFindElement(top, top, "accordLimitParam", NULL, NULL,
				MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find accordLimitParam in order!\n");
		return 0;
	}
	ret = alloc_mem_in_heap((void **)&p_raw_test_param->
				p_raw_accord_limit,
				sizeof(ST_RAW_ACCORD_LIMIT_PARAM));
	if (ret == 0) {
		board_print_error
			("accord limit malloc memory error!\n");
		return 0;
	}
	child_xnode =
		mxmlFindElement(parent_xnode, parent_xnode, "accordLimit",
			NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find accordLimit in order!\n");
		return 0;
	}
	p_raw_test_param->p_raw_accord_limit->accord_limit =
		str_to_int(child_xnode->child->value.opaque);

	child_xnode =
		mxmlFindElement(parent_xnode, parent_xnode, "accordOption",
				NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find accordOption in order!\n");
		return 0;
	}
	p_raw_test_param->p_raw_accord_limit->accord_option =
		str_to_int(child_xnode->child->value.opaque);
	board_print_debug("accord limit:%d!\n",
			p_raw_test_param->p_raw_accord_limit->
			accord_limit);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_offset_limit
* Description	: parse offset limit
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_offset_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param)
{
	s32 ret = 0;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;

	parent_xnode =
		mxmlFindElement(top, top, "offsetLimitParam", NULL, NULL,
				MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find offsetLimitParam in order!\n");
		return 0;
	}
	ret = alloc_mem_in_heap((void **)&p_raw_test_param->
				p_raw_offset_limit,
				sizeof(ST_RAW_OFFSET_LIMIT_PARAM));
	if (ret == 0) {
		board_print_error
			("offset limit malloc memory error!\n");
		return 0;
	}
	child_xnode =
		mxmlFindElement(parent_xnode, parent_xnode, "offsetLimit",
				NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find offsetLimit in order!\n");
		return 0;
	}
	p_raw_test_param->p_raw_offset_limit->offset_limit =
		str_to_int(child_xnode->child->value.opaque);
	board_print_debug("offset limit:%d!\n",
			p_raw_test_param->p_raw_offset_limit->
			offset_limit);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_uniform_limit
* Description 	: parse uniform limit
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_uniform_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param)
{
	s32 ret = 0;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;

	parent_xnode =
		mxmlFindElement(top, top, "uniformLimitParam", NULL, NULL,
			 MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find uniformLimitParam in order!\n");
		return 0;
	}
	ret = alloc_mem_in_heap((void **)&p_raw_test_param->
				p_raw_uniform_limit,
				sizeof(ST_RAW_UNIFORM_LIMIT_PARAM));
	if (ret == 0) {
		board_print_error("uniform limit malloc memory error!\n");
		return 0;
	}
	child_xnode =
		mxmlFindElement(parent_xnode, parent_xnode, "uniformLimit",
				NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find uniformLimit, please set uniform limit!\n");
		return 0;
	}
	p_raw_test_param->p_raw_uniform_limit->uniform_limit =
		str_to_int(child_xnode->child->value.opaque);
	board_print_debug("uniform limit:%d!\n",
			p_raw_test_param->p_raw_uniform_limit->
			uniform_limit);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_jitter_limit
* Description	: parse jitter limit
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_jitter_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param)
{
	s32 ret = 0;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;

	parent_xnode =
		mxmlFindElement(top, top, "jitterLimitParam", NULL, NULL,
				MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find jitterLimitParam in order!\n");
		return 0;
	}
	ret =alloc_mem_in_heap((void **)&p_raw_test_param->
			p_raw_jitter_limit,
			sizeof(ST_RAW_JITTER_LIMIT_PARAM));
	if (ret == 0) {
		board_print_error
			("jitter limit malloc memory error!\n");
		return 0;
	}
	child_xnode =
		mxmlFindElement(parent_xnode, parent_xnode, "jitterLimit",
				NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find jitterLimit, please set jitter limit!\n");
		return 0;
	}
	p_raw_test_param->p_raw_jitter_limit->jitter_limit =
		str_to_int(child_xnode->child->value.opaque);
	board_print_debug("jitter limit:%d!\n",
			p_raw_test_param->p_raw_jitter_limit->
			jitter_limit);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_key_max_limit
* Description	: parse key upper limit
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_key_max_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param)
{
	s32 ret = 0;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;

	parent_xnode =
		mxmlFindElement(top, top, "keydataLimitParam", NULL, NULL,
			MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find keydataLimitParam in order!\n");
		return 0;
	}
	if (p_raw_test_param->p_raw_key_limit == NULL) {
		ret = alloc_mem_in_heap((void **)&p_raw_test_param->
					p_raw_key_limit,
					sizeof(ST_RAW_KEY_LIMIT_PARAM));
		if (ret == 0) {
			board_print_error
				("key data limit malloc memory error!\n");
			return 0;
		}
	}
	child_xnode = mxmlFindElement(parent_xnode, parent_xnode, "upperLimit",
				NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find key upperLimit in order!\n");
		return 0;
	}
	p_raw_test_param->p_raw_key_limit->upper_limit =
		str_to_int(child_xnode->child->value.opaque);
	board_print_debug("key upper limit:%d!\n",
			p_raw_test_param->p_raw_key_limit->
			upper_limit);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_key_min_limit
* Description	: parse key lower limit
* Input			: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Output		: PST_RAWDATA_TEST_PARAM p_raw_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_key_min_limit(PST_RAWDATA_TEST_PARAM p_raw_test_param)
{
	s32 ret = 1;
	mxml_node_t *parent_xnode = NULL;
	mxml_node_t *child_xnode = NULL;

	parent_xnode =
		mxmlFindElement(top, top, "keydataLimitParam", NULL, NULL,
			MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find keydataLimitParam in order!\n");
		return 0;
	}
	if (p_raw_test_param->p_raw_key_limit == NULL) {
		ret = alloc_mem_in_heap((void **)&p_raw_test_param->
					p_raw_key_limit,
					sizeof(ST_RAW_KEY_LIMIT_PARAM));
		if (ret == 0) {
			board_print_error
				("key data limit malloc memory error!\n");
			return 0;
		}
	}
	child_xnode =
		mxmlFindElement(parent_xnode, parent_xnode, "lowerLimit",
			NULL, NULL, MXML_DESCEND);
	if ((child_xnode == NULL) || (child_xnode->child == NULL)) {
		board_print_error
			("can not find key lowerLimit in order!\n");
		return 0;
	}
	p_raw_test_param->p_raw_key_limit->lower_limit =
		str_to_int(child_xnode->child->value.opaque);
	board_print_debug("key lower limit:%d!\n",
			p_raw_test_param->p_raw_key_limit->
			lower_limit);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_raw_test_param
* Description	: parse rawdata test param
* Input			: PST_TEST_PARAM p_test_param
				: PST_SUB_TEST_ITEM_ID_SET p_sub_id_set
* Output		: PST_TEST_PARAM p_test_param
* Return 		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_raw_test_param(PST_TEST_PARAM p_test_param,
				PST_SUB_TEST_ITEM_ID_SET p_sub_id_set)
{
	s32 ret = 0;
	u16 i = 0;
	u16 data_len = 0;
	PST_RAWDATA_TEST_PARAM p_raw_test_param = NULL;
	mxml_node_t *test_item = NULL;

	/*no sub id */
	if (p_sub_id_set == NULL) {
		return 1;
	}
	/*find rawdat test param*/
	test_item = mxmlFindElement(top, top, "rawdataTestParam", NULL, NULL,
				MXML_DESCEND);
	if ((test_item == NULL) || (test_item->child == NULL)) {
		board_print_error
			("can not find rawdataTestParam in order!\n");
		return 0;
	}

	ret = alloc_mem_in_heap((void **)&p_test_param->p_raw_test_param,
				sizeof(ST_RAWDATA_TEST_PARAM));
	if (ret == 0) {
		board_print_error
			("raw test param malloc memory error!\n");
		return ret;
	}
	p_raw_test_param =
		(PST_RAWDATA_TEST_PARAM) p_test_param->p_raw_test_param;

	p_raw_test_param->p_need_check_node = NULL;
	p_raw_test_param->p_special_node = NULL;
	p_raw_test_param->p_rawdata_limit = NULL;
	p_raw_test_param->p_raw_accord_limit = NULL;
	p_raw_test_param->p_raw_offset_limit = NULL;
	p_raw_test_param->p_raw_jitter_limit = NULL;
	p_raw_test_param->p_raw_uniform_limit = NULL;
	p_raw_test_param->p_raw_key_limit = NULL;

	for (i = 0; i < p_sub_id_set->len; i++) {
		switch (p_sub_id_set->p_id_set[i]) {
		case TP_RAWDATA_MAX_TEST_ITEM_ID:
			{
				/*rawdata limit*/
				ret = parse_raw_max_limit
					(p_raw_test_param);
				break;
			}
		case TP_RAWDATA_MIN_TEST_ITEM_ID:
			{
				ret = parse_raw_min_limit
					(p_raw_test_param);
				break;
			}
		case TP_ACCORD_TEST_ITEM_ID:
			{
				/*accord limit*/
				ret = parse_accord_limit
					(p_raw_test_param);
				break;
			}
		case TP_OFFSET_TEST_ITEM_ID:
			{
				/*offset limit*/
				ret = parse_offset_limit
					(p_raw_test_param);
				break;
			}
		case TP_UNIFORMITY_TEST_ITEM_ID:
			{
				/*uniform limit*/
				ret = parse_uniform_limit
					(p_raw_test_param);
				break;
			}
		case TP_RAWDATA_JITTER_TEST_ITEM_ID:
			{
				/*jitter limit*/
				ret = parse_jitter_limit
					(p_raw_test_param);
				break;
			}
		case TP_KEYDATA_MAX_TEST_ITEM_ID:
			{
				/*key data upper limit*/
				ret = parse_key_max_limit
					(p_raw_test_param);
				break;
			}
		case TP_KEYDATA_MIN_TEST_ITEM_ID:
			{
				/*key data min limit*/
				ret = parse_key_min_limit
					(p_raw_test_param);
				break;
			}
		default:
			{
				break;
			}
		}

		if (ret == 0) {
			return ret;
			board_print_error
				("parse rawdata item error!\n");
		}

	}
	/*need check node*/
	data_len = parse_need_check_node(p_raw_test_param);
	if (data_len == 0) {
		board_print_error("parse need check node error!\n");
		return 0;
	}
	/*special test node*/
	ret = parse_special_node(p_raw_test_param,
				data_len + TP_MAX_KEY_NUM);
	if (ret == 0) {
		board_print_error("parse special node error!\n");
		return 0;
	}
	board_print_debug("special node len:%d!\n",
			p_raw_test_param->p_special_node->data_len);

	/*need check key*/
	ret = parse_need_check_key(p_raw_test_param, TP_MAX_KEY_NUM);
	if (ret == 0) {
		board_print_error("parse need check key error!\n");
		return 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: parse_short_test_param
* Description	: parse short test param
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_short_test_param(PST_TEST_PARAM p_test_param)
{
	s32 ret = 0;
	mxml_node_t *test_item = NULL;
	mxml_node_t *parent_xnode = NULL;
	PST_SHORT_PARAM p_short_param = NULL;

	/*find short test param*/
	test_item =
		mxmlFindElement(top, top, "shortThresholdParam", NULL, NULL,
				MXML_DESCEND);
	if ((test_item == NULL) || (test_item->child == NULL)) {
		board_print_error
			("can not find shortThresholdParam,plesse set short param in order!\n");
		return 0;
	}

	ret = alloc_mem_in_heap((void **)&p_test_param->p_short_param,
				sizeof(ST_SHORT_PARAM));
	if (ret == 0) {
		board_print_error
			("short test param malloc memory error!\n");
		return 0;
	}
	p_short_param = (PST_SHORT_PARAM) p_test_param->p_short_param;
	p_short_param->short_bin.update_bin_addr.bin_ptr = NULL;
	p_short_param->short_bin.update_bin_len = 0;
	p_short_param->short_opt.opt.opt_ptr = NULL;

	/*short test thrd node*/
	parent_xnode =
		mxmlFindElement(test_item, test_item,
				"drvGndVddResistorThreshold", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find drvGndVddResistorThreshold,plesse set in order!\n");
		return 0;
	}
	p_short_param->short_threshold.drv_gnd_vdd_resistor_threshold =
		str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("drv_gnd_vdd_resistor_threshold is :%d\n",
			p_short_param->short_threshold.
			drv_gnd_vdd_resistor_threshold);

	parent_xnode =
		mxmlFindElement(test_item, test_item,
				"senGndVddResistorThreshold", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find senGndVddResistorThreshold,plesse set in order!\n");
		return 0;
	}
	p_short_param->short_threshold.sen_gnd_vdd_resistor_threshold =
		str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("sen_gnd_vdd_resistor_threshold is :%d\n",
			p_short_param->short_threshold.
			sen_gnd_vdd_resistor_threshold);

	parent_xnode =
		mxmlFindElement(test_item, test_item,
				"senSenResistorThreshold", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find senSenResistorThreshold,plesse set in order!\n");
		return 0;
	}
	p_short_param->short_threshold.sen_sen_resistor_threshold =
		str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("sen_sen_resistor_threshold is :%d\n",
			p_short_param->short_threshold.
			sen_sen_resistor_threshold);

	parent_xnode =
		mxmlFindElement(test_item, test_item,
				"drvDrvResistorThreshold", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find drvDrvResistorThreshold,plesse set in order!\n");
		return 0;
	}
	p_short_param->short_threshold.drv_drv_resistor_threshold =
		str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("drv_drv_resistor_threshold is :%d\n",
			p_short_param->short_threshold.
			drv_drv_resistor_threshold);

	parent_xnode =
		mxmlFindElement(test_item, test_item,
				"drvSenResistorThreshold", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find drvSenResistorThreshold,plesse set in order!\n");
		return 0;
	}
	p_short_param->short_threshold.drv_sen_resistor_threshold =
		str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("drv_sen_resistor_threshold is :%d\n",
			p_short_param->short_threshold.
			drv_sen_resistor_threshold);

	parent_xnode =
		mxmlFindElement(test_item, test_item, "gtShortThreshold",
				NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find gtShortThreshold,plesse set in order!\n");
		return 0;
	}
	p_short_param->short_threshold.gt_short_threshold =
		str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("gt_short_threshold is :%d\n",
			p_short_param->short_threshold.
			gt_short_threshold);

	parent_xnode =
		mxmlFindElement(test_item, test_item, "adcReadDelay", NULL,
				NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find adcReadDelay,plesse set in order!\n");
		return 0;
	}
	p_short_param->short_threshold.adc_read_delay =
		str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("adc_read_delay is :%d\n",
			p_short_param->short_threshold.adc_read_delay);

	parent_xnode =
		mxmlFindElement(test_item, test_item,
				"diffcodeShortThreshold", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find diffcodeShortThreshold,plesse set in order!\n");
		return 0;
	}
	p_short_param->short_threshold.diffcode_short_threshold =
		str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("diffcode_short_threshold is :%d\n",
			p_short_param->short_threshold.
			diffcode_short_threshold);

	p_short_param->short_threshold.tx_tx_factor = 68;
	parent_xnode =
		mxmlFindElement(test_item, test_item, "txTxFactor", NULL,
				NULL, MXML_DESCEND);
	if ((parent_xnode != NULL) && (parent_xnode->child != NULL)) {
		p_short_param->short_threshold.tx_tx_factor =
			str_to_int(parent_xnode->child->value.opaque);
		board_print_debug("tx_tx_factor is :%d\n",
				p_short_param->short_threshold.tx_tx_factor);
	}

	p_short_param->short_threshold.tx_rx_factor = 89;
	parent_xnode =
		mxmlFindElement(test_item, test_item, "txRxFactor", NULL,
				NULL, MXML_DESCEND);
	if ((parent_xnode != NULL) && (parent_xnode->child != NULL)) {
		p_short_param->short_threshold.tx_rx_factor =
			str_to_int(parent_xnode->child->value.opaque);
		board_print_debug("tx_rx_factor is :%d\n",
				p_short_param->short_threshold.tx_rx_factor);
	}

	p_short_param->short_threshold.rx_rx_factor = 75;
	parent_xnode =
		mxmlFindElement(test_item, test_item, "rxRxFactor", NULL,
				NULL, MXML_DESCEND);
	if ((parent_xnode != NULL) && (parent_xnode->child != NULL)) {
		p_short_param->short_threshold.rx_rx_factor =
			str_to_int(parent_xnode->child->value.opaque);
		board_print_debug("rx_rx_factor is :%d\n",
				p_short_param->short_threshold.rx_rx_factor);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: parse_version_test_param
* Description 	: parse version test param
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_version_test_param(PST_TEST_PARAM p_test_param)
{
	s32 ret = 0;
	mxml_node_t *test_item = NULL;
	mxml_node_t *parent_xnode = NULL;
	PST_VERSION_PARAM p_ver_param = NULL;

	/*malloc memory*/
	if (p_test_param->p_version_test_param == NULL) {
		ret = alloc_mem_in_heap((void **)&p_test_param->
					p_version_test_param,
					sizeof(ST_VERSION_PARAM));
		if (ret == 0) {
			board_print_error
				("version test param malloc memory error!\n");
			return 0;
		}
	} else {
		board_print_error("version test param memory error!\n");
	}
	p_ver_param = (PST_VERSION_PARAM) p_test_param->p_version_test_param;
	p_ver_param->p_update_bin_param = NULL;

	/*find version test param*/
	test_item =
		mxmlFindElement(top, top, "versionTestParam", NULL, NULL,
				MXML_DESCEND);
	if ((test_item == NULL) || (test_item->child == NULL)) {
		board_print_error
			("can not find varsion test param,plesse set version param in order!\n");
		return 0;
	}

	parent_xnode =
		mxmlFindElement(test_item, test_item, "icVerAddr", NULL,
				NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find icVerAddr,plesse set icVerAddr in order!\n");
		return 0;
	}
	p_ver_param->ic_ver_addr =
		atohex((unsigned char *)parent_xnode->child->value.opaque,
			strlen((const char *)parent_xnode->child->value.
			opaque));
	board_print_debug("ic version addr:0x%04x!\n",
			p_ver_param->ic_ver_addr);
	p_ver_param->b_auto_update = 0;

	parent_xnode =
		mxmlFindElement(test_item, test_item, "versionStr", NULL,
				NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error
			("can not find varsion info,plesse set fw version in order!\n");
		return 0;
	}
	p_ver_param->ver_param_len =
		hexstr_to_array(p_ver_param->ver_param_str,
				parent_xnode->child->value.opaque,
				MAX_VERSION_LEN);
	board_print_debug("ver_param_len:%d!\n",
			p_ver_param->ver_param_len);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_flash_test_param
* Description	: parse flash test param
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_flash_test_param(PST_TEST_PARAM p_test_param)
{
	s32 ret = 0;
	mxml_node_t *test_item = NULL;
	mxml_node_t *parent_xnode = NULL;
	PST_FLASH_TEST_PARAM p_flash_param = NULL;

	/*find version test param*/
	ret = alloc_mem_in_heap((void **)&p_test_param->
			p_flash_test_param,
			 sizeof(ST_FLASH_TEST_PARAM));
	if (ret == 0) {
		board_print_error
		("flash test param malloc memory error!\n");
		return 0;
	}
	p_flash_param = (PST_FLASH_TEST_PARAM) p_test_param->p_flash_test_param;
	p_flash_param->config_arr = NULL;
	p_flash_param->delay_time = 0;

	test_item = mxmlFindElement(top, top, "flashTestParam", NULL, NULL, MXML_DESCEND);
	if ((test_item == NULL) || (test_item->child == NULL)) {
		board_print_error
		("can not find flash test param,plesse set flash param in order!\n");
		return 0;
	}

	parent_xnode = mxmlFindElement(test_item, test_item, "option", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error("can not find flash option,order error!\n");
		return 0;
	}
	p_flash_param->option =
			str_to_int(parent_xnode->child->value.opaque);
	board_print_debug("flash test option:0x%04x!\n",
			p_flash_param->option);

	parent_xnode =
		mxmlFindElement(test_item, test_item, "delayTime", NULL,
			NULL, MXML_DESCEND);
	if ((parent_xnode != NULL) && (parent_xnode->child != NULL)) {
		p_flash_param->delay_time =
			str_to_int(parent_xnode->child->value.opaque);
	}
	board_print_debug("delay_time in xml:%d!\n", p_flash_param->delay_time);

	return ret;
}

/*******************************************************************************
* Function Name	: parse_ic_name
* Description	: parse_ic_name from order
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
static s32 parse_ic_name(PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	mxml_node_t *obj_set = NULL;
	mxml_node_t *parent_xnode = NULL;

	obj_set = mxmlFindElement(top, top, "OBJ_SETTING", NULL, NULL, MXML_DESCEND);

	if ((obj_set == NULL) || (obj_set->child == NULL)) {
		board_print_error("can not find OBJ_SETTING in order!\n");
		return 0;
	}

	parent_xnode = mxmlFindElement(obj_set, obj_set, "chipName", NULL, NULL, MXML_DESCEND);
	if ((parent_xnode == NULL) || (parent_xnode->child == NULL)) {
		board_print_error("can not find chipName in order!\n");
		return 0;
	}
	memcpy(p_test_param->ic_name, parent_xnode->child->value.opaque,
			strlen((const char *)parent_xnode->child->value.opaque) + 1);
	board_print_debug("ic name in order :%s\n", p_test_param->ic_name);

	return ret;
}

/*******************************************************************************
* Function Name	: parse_test_param
* Description	: parse test param
* Input 		: PST_TEST_PARAM *pp_test_param
* Output		: PST_TEST_PARAM *pp_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 parse_test_param(PST_TEST_PARAM *pp_test_param)
{
	PST_TEST_PARAM p_test_param;
	u16 i = 0;
	s32 ret = 0;
	u8 need_send_cfg = 0;
	u8 cfg_flag = 0;

	/*(1)malloc memory for test param*/
	if (alloc_mem_in_heap
		((void **)pp_test_param, sizeof(ST_TEST_PARAM)) == 0) {
		board_print_error("test param malloc memory error!");
		return 0;
	}
	p_test_param = (PST_TEST_PARAM) (*pp_test_param);
	p_test_param->test_cfg = NULL;
	p_test_param->cfg_len = 0;
	p_test_param->mode_sen_line = 0;
	p_test_param->p_test_item_id_set = NULL;
	p_test_param->p_raw_test_param = NULL;
	p_test_param->p_diff_test_param = NULL;
	p_test_param->p_short_param = NULL;
	p_test_param->p_version_test_param = NULL;
	p_test_param->p_flash_test_param = NULL;

	ret = parse_ic_name(p_test_param);
	if (ret == 0) {
		return 0;
	}

	ret = parse_test_item_id(p_test_param);
	if (ret == 0) {
		board_print_error("parse test item id error!\n");
		return 0;
	}

	if (p_test_param->test_item_num == 0) {
		board_print_error("no need test item!\n");
		return 0;
	}

	ret = parse_order_config(p_test_param);
	if (ret == 0) {
		board_print_error("parse_order_config error!\n");
		return 0;
	}

	for (i = 0; i < p_test_param->test_item_num; i++) {
		switch (p_test_param->p_test_item_id_set[i].test_id) {
		case TP_RAWDATA_TEST_ITEMS_SET_ID:
			{
				p_test_param->close_hop = 1;
				need_send_cfg++;
				ret =
					parse_raw_test_param(p_test_param,
							p_test_param->
							p_test_item_id_set
							[i].
							p_sub_id_set);
				break;
			}
		case TP_SHORT_TEST_ITEM_ID:
			{
				need_send_cfg++;
				ret =
					parse_short_test_param
					(p_test_param);
				break;
			}
		case TP_VERSION_TEST_ITEM_ID:
			{
				ret =
					parse_version_test_param
					(p_test_param);
				break;
			}
		case TP_FLASH_TEST_ITEM_ID:
			{
				ret =
				    parse_flash_test_param
				    (p_test_param);
				break;
			}
		case TP_CHIP_CFG_PROC_ITEM_ID:
			{
				cfg_flag |= 0x01;
				/*ret = parse_order_config(p_test_param);*/
				break;
			}
		case TP_RECOVER_CFG_ITEM_ID:
			{
				cfg_flag |= 0x02;
				break;
			}
		default:
			{
				break;
			}
		}
		if (ret == 0) {
			board_print_error("parse test param error!\n");
			break;
		}
	}

	if (need_send_cfg > 0) {
		if ((cfg_flag & 0x01) == 0) {
			p_test_param->p_test_item_id_set[p_test_param->
							test_item_num].
				test_id = TP_CHIP_CFG_PROC_ITEM_ID;
			p_test_param->test_item_num++;
		}
		if ((cfg_flag & 0x02) == 0) {
			p_test_param->p_test_item_id_set[p_test_param->
						test_item_num].
				test_id = TP_RECOVER_CFG_ITEM_ID;
			p_test_param->test_item_num++;
		}
	} else {
		if (((cfg_flag & 0x01) == 1)
			&& ((cfg_flag & 0x02) == 0)) {
			p_test_param->p_test_item_id_set[p_test_param->
				test_item_num].
				test_id = TP_RECOVER_CFG_ITEM_ID;
			p_test_param->test_item_num++;
		}
	}
	if ((cfg_flag & 0x01) == 0) {
		p_test_param->test_cfg[0] = '\0';
		p_test_param->cfg_len = 0;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: release_order_config
* Description	: release order config
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_order_config(PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	if (p_test_param->test_cfg != NULL) {
		ret = free_mem_in_heap_p((void **)&p_test_param->
					test_cfg);
	}

	return ret;
}

/*******************************************************************************
* Function Name	: release_test_item_id
* Description	: release test item id
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_test_item_id(PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	u16 i = 0;
	if (p_test_param->p_test_item_id_set != NULL) {
		if (p_test_param->p_test_item_id_set[i].p_sub_id_set !=
			NULL) {
			if (p_test_param->p_test_item_id_set[i].
				p_sub_id_set->p_id_set != NULL) {
				ret = free_mem_in_heap_p((void **)
							&p_test_param->
							p_test_item_id_set
							[i].
							p_sub_id_set->
							p_id_set);
			}
			ret = free_mem_in_heap_p((void **)&p_test_param->
					p_test_item_id_set[i].
					p_sub_id_set);
		}
		ret = free_mem_in_heap_p((void **)&p_test_param->
					p_test_item_id_set);
	}

	return ret;
}

/*******************************************************************************
* Function Name	: release_raw_test_param
* Description	: release rawdata test param
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_raw_test_param(PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	PST_RAWDATA_TEST_PARAM p_raw_test_param =
		p_test_param->p_raw_test_param;
	if (p_raw_test_param != NULL) {
		/*free need check node buffer*/
		if (p_raw_test_param->p_need_check_node != NULL) {
			ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_need_check_node->
							p_data_buf);
			ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_need_check_node);
		}
		/*free special node buffer*/
		if (p_raw_test_param->p_special_node != NULL) {
			ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_special_node->
							p_data_buf);
			ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_special_node);
		}
		/*free raw limit parameter buffer*/
		if (p_raw_test_param->p_rawdata_limit != NULL) {
			ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_rawdata_limit);
		}
		/*free rawdata acoord limit parameter buffe*/
		if (p_raw_test_param->p_raw_accord_limit != NULL) {
			ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_raw_accord_limit);
		}
		/*free raw offset limit parameter buffer*/
		if (p_raw_test_param->p_raw_offset_limit != NULL) {
			ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_raw_offset_limit);
		}
		/*free raw jitter limit parameter buffer*/
		if (p_raw_test_param->p_raw_jitter_limit != NULL) {
			ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_raw_jitter_limit);
		}
		/*free raw uniform limit parameter buffer*/
		if (p_raw_test_param->p_raw_uniform_limit != NULL) {
			ret = free_mem_in_heap_p((void **)
						&p_raw_test_param->
						p_raw_uniform_limit);
		}
		/*free keydata limit parameter buffer*/
		if (p_raw_test_param->p_raw_key_limit != NULL) {
			if (p_raw_test_param->p_raw_key_limit->
				p_need_check_key != NULL) {
				if (p_raw_test_param->p_raw_key_limit->
					p_need_check_key->p_data_buf !=
					NULL) {
					ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_raw_key_limit->
							p_need_check_key->
							p_data_buf);
				}
				ret = free_mem_in_heap_p((void **)
							&p_raw_test_param->
							p_raw_key_limit->
							p_need_check_key);
			}
			ret = free_mem_in_heap_p((void **)
						&p_raw_test_param->
						p_raw_key_limit);
		}
		/*free raw test parameter*/
		ret = free_mem_in_heap_p((void **)&p_raw_test_param);
		p_test_param->p_raw_test_param = NULL;
	}

	return ret;
}

/*******************************************************************************
* Function Name	: release_short_test_param
* Description	: release short test param
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_short_test_param(PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	PST_SHORT_PARAM p_short_param = p_test_param->p_short_param;
	if (p_short_param != NULL) {
		ret = free_mem_in_heap_p((void **)&p_short_param);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: release_version_test_param
* Description	: release version test param
* Input			: PST_TEST_PARAM p_test_param
* Output		: PST_TEST_PARAM p_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_version_test_param(PST_TEST_PARAM p_test_param)
{
	s32 ret = 0;
	PST_VERSION_PARAM p_ver_param = NULL;
	p_ver_param = p_test_param->p_version_test_param;
	if (p_ver_param == NULL) {
		return 1;
	}
	p_ver_param->p_update_bin_param = NULL;
	ret = free_mem_in_heap((void *)p_ver_param);
	p_test_param->p_version_test_param = NULL;
	return ret;
}

/*******************************************************************************
* Function Name		: release_flash_test_param
* Description		: release flash test param
* Input				: PST_TEST_PARAM p_test_param
* Output			: PST_TEST_PARAM p_test_param
* Return			: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_flash_test_param(PST_TEST_PARAM p_test_param)
{
	s32 ret = 1;
	PST_FLASH_TEST_PARAM p_flash_param = NULL;
	p_flash_param = p_test_param->p_flash_test_param;
	if (p_flash_param != NULL) {
		if (p_flash_param->config_arr != NULL) {
			ret = free_mem_in_heap((void *)p_flash_param->config_arr);
		}
		ret = free_mem_in_heap((void *)p_flash_param);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: release_test_param
* Description	: release test param
* Input			: PST_TEST_PARAM* pp_test_param
* Output		: PST_TEST_PARAM* pp_test_param
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 release_test_param(PST_TEST_PARAM *pp_test_param)
{
	s32 ret = 1;
	PST_TEST_PARAM p_test_param = NULL;
	if ((pp_test_param == NULL) || ((*pp_test_param) == NULL)) {
		return 1;
	}
	p_test_param = (*pp_test_param);
	ret = release_order_config(p_test_param);
	ret = release_test_item_id(p_test_param);
	ret = release_raw_test_param(p_test_param);
	ret = release_short_test_param(p_test_param);
	ret = release_version_test_param(p_test_param);
	ret = release_flash_test_param(p_test_param);
	ret = free_mem_in_heap(p_test_param);
	p_test_param = NULL;
	return ret;
}

/*******************************************************************************
* Function Name	: decrypt_tp_order
* Description	: decrypt_tp_order
* Input			: s8** pp_buf(buf content)you need free memory by yourself
* Input			: s2* p_len(len)
* Output		: none
* Return		: s32(0:ok other:error code)
*******************************************************************************/
static s32 decrypt_tp_order(PST_TP_DEV p_tp_dev, s8 **pp_buf,
			u32 *p_len)
{
	s32 ret = 0;
	s8 *ver = NULL;

	/*read decode order*/
	ret = read_file_decoder(p_tp_dev, &ver, pp_buf);
	if (ret == 0) {
		board_print_error("read file decoder error!\n");
		free_mem_in_heap(ver);
		return 0;
	}
	/*get length of xml*/
	*p_len = strlen((cstr) * pp_buf);

	free_mem_in_heap(ver);
	return ret;
}

/*******************************************************************************
* Function Name	: parse_init_param_dev
* Description	: parse_init_param_dev
* Input			: PST_TEST_PROC_DATA p_test_proc_data
* Output		: PST_TEST_PROC_DATA p_test_proc_data
* Return		: s32(0:faild 1:ok)
*******************************************************************************/
extern int parse_test_order(PST_TEST_PARAM *pp_test_param,
				PST_TP_DEV p_tp_dev)
{
	s32 ret = 1;
	s8 *file_content = NULL;
	u32 xml_buf_len = 0;
	PST_TEST_PARAM p_test_param = NULL;

	/*(1)decrypt order*/
	board_print_debug("decypt order!");
	ret = decrypt_tp_order(p_tp_dev, &file_content, &xml_buf_len);
	if (ret == 0) {
		free_mem_in_heap(file_content);
		return 0;
	}
	/*(2)load xml file*/
	board_print_debug("load xml file!");
	top = mxmlLoadString(NULL, file_content, MXML_OPAQUE_CALLBACK);
	if (top == NULL) {
		ret = 0;
		board_print_error("load xml fail!\n");
		free_mem_in_heap(file_content);
		return 0;
	}
	/*(3)init test param struct*/
	board_print_debug("parse test param!\n");
	ret = parse_test_param(pp_test_param);
	p_test_param = (PST_TEST_PARAM) (*pp_test_param);
	if (ret == 0) {
		goto PARSE_END;
	}
	/*(2)init tp ev struct*/
	ret = parse_init_dev_info(p_tp_dev, p_test_param);
	if (ret == 0) {
		goto PARSE_END;
	}
	/*judge need check node*/
	if (p_test_param->p_raw_test_param != NULL
		&& p_test_param->p_raw_test_param->p_need_check_node !=
		NULL) {
		if (p_test_param->p_raw_test_param->p_need_check_node->
			data_len <
			p_tp_dev->sc_drv_num * p_tp_dev->sc_sen_num) {
			board_print_error
				("need check node param error, node len in xml:%d, sc_drv_num:%d, sc_sen_num:%d\n",
				p_test_param->p_raw_test_param->
				p_need_check_node->data_len,
				p_tp_dev->sc_drv_num,
				p_tp_dev->sc_sen_num);
			ret = 0;
			goto PARSE_END;
		}
	}

PARSE_END:
	/*release xml file */
	/*mxmlDelete(top);*/
	free_mem_in_heap(file_content);

	return ret;
}

#ifdef __cplusplus
}
#endif

/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name			: tp_dev_def.h
* Author			: Bob Huang
* Version			: V1.0.0
* Date				: 12/31/2017
* Description		: touch panel device define
*******************************************************************************/
#ifndef TP_DEF_DEF_H
#define TP_DEF_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"
#include "tp_product_id_def.h"
#include "tp_micro_def.h"

/* communication info*/
typedef struct TP_COMM {
	u8 comm_type;	/* the type of communication -default i2c*/
	u8 i2c_address_num;
	u8 i2c_address[TP_MAX_I2C_ADDRESS_NUM];	/*127*/
	u8 dev_address;	/* seek i2c address*/
	u8 dev_fix_address;	/* fixed address*/
} ST_TP_COMM, *PST_TP_COMM;

/*device command define*/
typedef struct TP_DEV_CMD {
	u16 addr;
	u8 cmd_len;
	u8 cmd_buf[TP_DEV_CMD_LEN];
} ST_TP_DEV_CMD, *PST_TP_DEV_CMD;
/*cmd set*/
typedef struct TP_DEV_CMD_SET {
	ST_TP_DEV_CMD rawdata_cmd;
	ST_TP_DEV_CMD diffdata_cmd;
	ST_TP_DEV_CMD coorddata_cmd;
	ST_TP_DEV_CMD sleep_cmd;
	ST_TP_DEV_CMD hid_ena_cmd;
	ST_TP_DEV_CMD hid_dis_cmd;
	ST_TP_DEV_CMD enter_gest_cmd;

} ST_TP_DEV_CMD_SET, *PST_TP_DEV_CMD_SET;

/*data option*/
#define _DATA_BIT_8				0x01
#define _DATA_BIT_16			0x02
#define _DATA_BIT_32			0x04
#define _DATA_BIT_64			0x08
#define _DATA_DRV_SEN_INVERT	0x10
#define _DATA_SIGNED			0x40
#define _DATA_LARGE_ENDIAN		0x80

/*touch panel device define*/
typedef struct TP_DEV {
	/* COMM*/
	PST_TP_COMM p_tp_comm;
	ptr32 p_logic_dev;	/*use in pc code,arm code equeal null*/

	u16 chip_type;	/* chip type*/
	u16 chip_sub_type;	/* chip sub type*/

	/*voltage setting*/
	u16 power_volt;	/* mV*/
	u16 comm_volt;	/* mV */

	/*cfg*/
	u16 cfg_start_addr;
	u16 ext_cfg_addr;
	u16 ext_cfg_len;
	u16 cfg_len;	/*if ext_cfg_len != 0 then cfg_len = base_cfg_len + ext_cfg_len*/
	u8 cfg[TP_MAX_CFG_LEN];

	u8 use_soft_reset;	/* 1 use soft reset first */
	u8 no_rst_pin;	/* 1 no rst pin 0 has rst pin default 0*/

	/* max sensor and driver num for die*/
	u8 max_sen_num_in_die;
	u8 max_drv_num_in_die;

	/* max sensor and driver num for this chip*/
	u8 max_sen_num;
	u8 max_drv_num;

	/* current sensor num and driver(include key channel)*/
	u8 total_sen_num;
	u8 total_drv_num;

	/*screen sensor num and driver(exclude key channel)*/
	u8 sc_sen_num;
	u8 sc_drv_num;
	u16 sen_start_addr;
	u16 drv_start_addr;
	u16 sen1_start_addr;	/*have two G1 use,eg Phoenix*/
	u16 drv1_start_addr;	/*have two G1 use,eg Phoenix*/
	/*channel map*/
	u8 map_disable;
	u8 drv_map[TP_MAX_CHN_NUM];
	u8 sen_map[TP_MAX_CHN_NUM];
	u8 pack_drv_2_adc_map[TP_MAX_CHN_NUM];
	u8 pack_sen_2_adc_map[TP_MAX_CHN_NUM];
	u8 chn_map[TP_MAX_CHN_NUM];

	/* key*/
	u8 key_num;
	u8 key_rawdata_num;
	u8 key_pos_arr[TP_MAX_KEY_NUM];

	/* cmd */
	u16 seek_reg_addr;

	u16 real_cmd_addr;	/*this field may be same as fields in cmd_set*/
	ST_TP_DEV_CMD_SET cmd_set;

	u16 rawdata_addr;
	u16 rawdata_len;
	u8 rawdata_option;

	u16 diffdata_addr;
	u16 diffdata_len;
	u8 diffdata_option;

	u16 basedata_addr;
	u16 basedata_len;
	u8 basedata_option;

	u16 syncflag_addr;
	u16 syncflag_mask;
	u16 coordchk_sum;

	/*in GT900/GT9P/GT9L this address is u16,in Phoenix/Normandy this address is u32*/
	u32 custom_info_addr;
	u8 custom_info_len;

	u32 module_test_res_addr;
	u8 module_test_res_len;

	u32 uid_addr;
	u8 uid_len;

	u32 chip_test_res_addr;
	u16 chip_test_res_len;

	/* general flash protocol*/
	u32 flash_rw_cmd_addr;
	u8 flash_read_cmd_byte;
	u8 flash_write_cmd_byte;
	u8 flash_send_cmd_end_byte;

	u32 flash_rw_sta_addr;
	u8 flash_rw_ok_sta_byte;
	u8 flash_rw_fail_sta_byte;
	u8 flash_enter_rw_sta_byte;
	u8 flash_chk_sum_err_sta_byte;
	u8 flash_erase_fail_sta_byte;
	u8 flash_write_fail_sta_byte;
	u8 flash_read_fail_sta_byte;
	u8 flash_access_fail_sta_byte;
	u8 flash_clr_sta_byte;
	u8 flash_rw_available;
	u8 b_flash_big_endian;

	u32 flash_rw_switch_buf_addr;
	u32 flash_max_data_size;
	u32 flash_page_size;

	u8 doze_ref_cnt;	/*1:doze mode  0:exit doze*/
	u8 hid_state;	/*1:state of enable hid  0:state of disable hid*/

} ST_TP_DEV, *PST_TP_DEV;

#ifdef __cplusplus
}
#endif
#endif

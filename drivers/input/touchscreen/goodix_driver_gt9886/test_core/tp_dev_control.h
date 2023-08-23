/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name				: tp_dev_contol.h
* Author				: Bob Huang
* Version				: V1.0.0
* Date					: 07/26/2017
* Description			: device control interface
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_DEV_CONTROL_H
#define TP_DEV_CONTROL_H

#include "tp_dev_def.h"
#include "board_opr_interface.h"
#include "custom_info_def.h"

#ifdef __cplusplus
extern "C" {
#endif
/*************************************Public methods start********************************************/
extern s32 chip_reg_write(PST_TP_DEV p_dev, u16 addr, u8 *p_buf,
				u16 buf_len);
extern s32 chip_reg_read(PST_TP_DEV p_dev, u16 addr, u8 *p_buf,
				u16 buf_len);
extern s32 chip_reg_write_chk(PST_TP_DEV p_dev, u16 addr, u8 *p_buf,
				u16 buf_len);

extern s32 ctrl_chip_io(PST_TP_DEV p_dev, u16 io_id,
				GIOMode_Def modeType);

/*set tp chip io state*/
extern s32 setbit_chip_io(PST_TP_DEV p_dev, u16 io_id,
				GIOBit_Def bit_state);
/*get tp chip io state*/
extern GIOBit_Def getbit_chip_io(PST_TP_DEV p_dev, u16 io_id);

/*rst pin operation*/
typedef enum {
	RESET_TO_AP = 0x00,
	RESET_TP_DSP = 0x01
} RST_OPTION;
extern s32 reset_chip_proc(PST_TP_DEV p_dev, RST_OPTION rst_opt);
extern s32 reset_chip(PST_TP_DEV p_dev);

extern s32 wakeup_chip(PST_TP_DEV p_dev);

/*int pin operation*/
extern s32 set_chip_int_floating(PST_TP_DEV p_dev);

enum {
	CHIP_RAW_DATA_MODE,
	CHIP_COORD_MODE,	/*coordinate mode  */
	CHIP_SLEEP_MODE,
	CHIP_DIFF_DATA_MODE,
};
extern s32 chip_mode_select(PST_TP_DEV p_dev, u8 work_mode);

extern s32 enter_gest_doze(PST_TP_DEV p_dev);
extern s32 exit_gest_doze(PST_TP_DEV p_dev);

/*data flag indicate there is coord data(rawdata) or not*/
extern s32 get_data_flag(PST_TP_DEV p_dev);
extern s32 clr_data_flag(PST_TP_DEV p_dev);

/*cfg process*/
extern s32 write_cfg_arr(PST_TP_DEV p_dev, u8 *config, u16 cfg_len);
extern s32 read_cfg_arr(PST_TP_DEV p_dev, u8 *config, u16 cfg_len);
extern s32 write_cfg(PST_TP_DEV p_dev);
extern s32 read_cfg(PST_TP_DEV p_dev);
extern s32 force_update_cfg(PST_TP_DEV p_dev, u8 *config, u16 cfg_len);

extern s32 cfg_to_flash_delay(PST_TP_DEV p_dev);

extern s32 cmp_cfg(PST_TP_DEV p_dev, u8 *cfg1, u8 *cfg2, u16 cfg_len);

/*cmd*/
extern s32 write_cmd(PST_TP_DEV p_dev, u16 cmd_addr, u8 cmd_byte);

#define CHECK_CHIP_TEST_RES_OK			0
#define CHECK_CHIP_TEST_COMM_ERR		1
#define CHECK_CHIP_TEST_CHK_SUM_ERR		2
#define CHECK_CHIP_TEST_RES_NG			3
#define CHECK_CHIP_TEST_QUALITY_NG		4
extern s32 check_chip_test_res_info(PST_TP_DEV p_dev,
					u8 *test_res_info, u32 len);
/*************************************Public methods end**********************************************/

#ifdef __cplusplus
}
#endif
/*************************************Private methods start********************************************//*************************************Private methods end********************************************/
#endif
#endif

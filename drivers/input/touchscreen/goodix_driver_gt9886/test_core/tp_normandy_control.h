/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_normandy_control.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : normandy device control interface
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef TP_NORMANDY_CONTROL_H
#define TP_NORMANDY_CONTROL_H

#include "tp_dev_def.h"
#include "board_opr_interface.h"
#include "custom_info_def.h"

#ifdef __cplusplus
extern "C" {
#endif
/*************************************Public methods start********************************************/
extern s32 normandy_reg_write(PST_TP_DEV p_dev, u16 addr, u8 *p_buf, u16 buf_len);
extern s32 normandy_reg_read(PST_TP_DEV p_dev, u16 addr, u8 *p_buf, u16 buf_len);
extern s32 normandy_soft_reset_chip(PST_TP_DEV p_dev);
extern s32 normandy_reset_hold_chip(PST_TP_DEV p_dev);
extern s32 normandy_wakeup_chip(PST_TP_DEV p_dev);
extern s32 normandy_ctrl_chip_io(PST_TP_DEV p_dev, u16 io_id, GIOMode_Def modeType);
extern s32 normandy_setbit_chip_io(PST_TP_DEV p_dev, u16 io_id, GIOBit_Def bit_state);
extern GIOBit_Def normandy_getbit_chip_io(PST_TP_DEV p_dev, u16 io_id);
extern s32 normandy_force_update_cfg(PST_TP_DEV p_dev, u8 *config, u16 cfg_len);
extern s32 normandy_write_cfg(PST_TP_DEV p_dev);
extern s32 normandy_read_cfg(PST_TP_DEV p_dev);
extern s32 normandy_write_cfg_arr(PST_TP_DEV p_dev, u8 *config, u16 cfg_len);
extern s32 normandy_read_cfg_arr(PST_TP_DEV p_dev, u8 *config, u16 cfg_len);
extern void normandy_cfg_update_chksum(u8 *config, u16 cfg_len);
extern s32 normandy_cmp_cfg(PST_TP_DEV p_dev, u8 *cfg1, u8 *cfg2, u16 cfg_len);
extern s32 normandy_write_custom_info(PST_TP_DEV p_dev, PST_CUSTOM_INFO_BYTE custom_info, u8 len);
extern s32 normandy_read_custom_info(PST_TP_DEV p_dev, PST_CUSTOM_INFO_BYTE custom_info, u8 len);
extern s32 normandy_doze_mode(PST_TP_DEV p_dev, u8 b_ena, u8 b_force);
extern s32 normandy_check_chip_test_res_info(IN PST_TP_DEV p_dev, IN u8 *test_res_info, IN u32 len);
/*************************************Public methods End********************************************/
#ifdef __cplusplus
}
#endif
#endif
#endif

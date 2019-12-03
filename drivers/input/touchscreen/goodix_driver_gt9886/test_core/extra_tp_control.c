
/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name          : extra_tp_control.cpp
* Author             : yangzhitao
* Version            : V1.0.0
* Date               : 30/03/2018
* Description        : tp control func
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "extra_tp_control.h"
#include "tp_dev_control.h"
#include "tp_normandy_control.h"
#include "generic_func.h"
#include <linux/string.h>

/*******************************************************************************
* Function Name	: read_sensor_id
* Description	: read sensor id
* Input			: PST_TP_DEV p_tp_dev
				: u8* sensor_id
* Output		: u8* sensor_id
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 read_sensor_id(PST_TP_DEV p_tp_dev, u8 *sensor_id)
{
	s32 ret = 0;
	u8 tmp_id[1];
	u16 sen_id_addr;

	switch (p_tp_dev->chip_type) {
	case TP_NORMANDY:
		{
			sen_id_addr = 0x4541;
			break;
		} default: {
			sen_id_addr = 0x814A;
			break;
		}
	}
	board_print_debug("sensor id addr:0x%04x!\n", sen_id_addr);
	ret = chip_reg_read(p_tp_dev, sen_id_addr, sensor_id, 1);
	ret = chip_reg_read(p_tp_dev, sen_id_addr, tmp_id, 1);
	if (sensor_id[0] != tmp_id[0]) {
		ret = chip_reg_read(p_tp_dev, sen_id_addr, sensor_id, 1);
		ret = chip_reg_read(p_tp_dev, sen_id_addr, tmp_id, 1);
		if (sensor_id[0] != tmp_id[0]) {
			board_print_error("sensor id is not equal!\n");
			return 0;
		}
	}
	sensor_id[0] &= 0x0F;
	return ret;
}

/*******************************************************************************
* Function Name  : read_chip_original_cfg
* Description    : read chip original config
* Input          : PST_TP_DEV p_tp_dev
* Output         : none
* Return         : s32(0:Fail >0:read config len)
*******************************************************************************/
extern s32 read_chip_original_cfg(PST_TP_DEV p_tp_dev)
{
	s32 ret = 0;
	switch (p_tp_dev->chip_type) {
	case TP_NORMANDY:
		{
			ret = read_cfg_arr(p_tp_dev, p_tp_dev->cfg, 0);
			p_tp_dev->cfg_len = ret;
			/*updata status flag*/
			p_tp_dev->cfg[1] |= 0x01;
			break;
		}
	default:
		{
			ret = read_cfg_arr(p_tp_dev, p_tp_dev->cfg, p_tp_dev->cfg_len);
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: update_cfg_checksum
* Description	: update test config checksum
* Input			: PST_TP_DEV p_tp_dev
				: u8* test_cfg
* Output		:
* Return		: void
*******************************************************************************/
extern void update_cfg_checksum(PST_TP_DEV p_tp_dev, u8 *test_cfg,
		u16 len)
{
	switch (p_tp_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			normandy_cfg_update_chksum(test_cfg, len);
			break;
		}
	default:
		{
			break;
		}
	}
}

#ifdef __cplusplus
}
#endif

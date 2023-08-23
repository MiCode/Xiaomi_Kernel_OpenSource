/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_dev_contol.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 07/26/2017
* Description        : device control interface
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#include "simple_mem_manager.h"
#include "tp_dev_control.h"
#include "tp_dev_def.h"
#include "public_fun.h"
#include "tp_normandy_control.h"

#ifdef __cplusplus
extern "C" {
#endif

static s32 soft_reset_chip(PST_TP_DEV p_dev);

static s32 reset_hold_chip(PST_TP_DEV p_dev);

static s32 chip_int_sync(PST_TP_DEV p_dev);

/*******************************************************************************
* Function Name	: chip_reg_write
* Description	: write chip register
* Input			: PST_TP_DEV p_dev(pointer to a touch panel deice, which should be inited before)
* Input			: u16 addr(address)
* Input			: s32* p_buf(data buffer)
* Input			: u16 buf_len(buffer length)
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 chip_reg_write(IN PST_TP_DEV p_dev, IN u16 addr,
			IN u8 *p_buf, IN u16 buf_len)
{
	s32 ret = 0;
	u8 retry_cnt = 3;
	u8 i = 0;

	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret =
				normandy_reg_write(p_dev, addr, p_buf,
						buf_len);
		} break;
	default:
		{
			for (i = 0; i < retry_cnt; i++) {
				ret = board_write_chip_reg(p_dev, addr, p_buf, buf_len);
				if (1 == ret) {
					break;
				}
			}
		}
		break;
	}

	return ret;
}

/*******************************************************************************
* Function Name	: chip_reg_read
* Description	: read chip register
* Input			: PST_TP_DEV p_dev(pointer to a touch panel deice, which should be inited before)
* Input			: u16 addr(address)
* Input			: s32* p_buf(data buffer)
* Input			: u16 buf_len(buffer length)
* Output		: s32* p_buf
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 chip_reg_read(IN PST_TP_DEV p_dev, IN u16 addr,
			OUT u8 *p_buf, IN u16 buf_len)
{
	s32 ret = 0;
	u8 retry_cnt = 3;
	u8 i = 0;

	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_reg_read(p_dev, addr, p_buf, buf_len);
		}
		break;
	default:
		{
			for (i = 0; i < retry_cnt; i++) {
				ret = board_read_chip_reg(p_dev, addr, p_buf, buf_len);
				if (1 == ret) {
					break;
				}
			}
		}
		break;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: chip_reg_write_chk
* Description	: write chip register and check whether writing successfully
* Input			: PST_TP_DEV p_dev(pointer to a touch panel deice, which should be inited before)
* Input			: u16 addr(address)
* Input			: s32* p_buf(data buffer)
* Input			: u16 buf_len(buffer length)
* Output		: s32* p_buf
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 chip_reg_write_chk(IN PST_TP_DEV p_dev, IN u16 addr,
				IN u8 *p_buf, IN u16 buf_len)
{
	s32 ret = 0;
	u16 i = 0;
	u8 *p_buf_read_tmp = NULL;
	ret = chip_reg_write(p_dev, addr, p_buf, buf_len);
	if (1 == ret) {
		ret = alloc_mem_in_heap((void **)&p_buf_read_tmp, buf_len);
		if (0 == ret) {
			return ret;
		}
		ret = chip_reg_read(p_dev, addr, p_buf_read_tmp, buf_len);
		if (1 == ret) {
			for (i = 0; i < buf_len; i++) {
				if (p_buf[i] != p_buf_read_tmp[i]) {
					ret = 0;
					break;
				}
			}
		}
		free_mem_in_heap(p_buf_read_tmp);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: reset_chip
* Description	: reset chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: RST_OPTION rst_opt(reset option,you can refer RST_OPTION)
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 reset_chip_proc(IN PST_TP_DEV p_dev, IN RST_OPTION rst_opt)
{
	s32 ret = 0;
	if (p_dev == NULL) {
		return ret;
	}
	/* reset ap flow
	// 1. reset chip 2.chip init sync 3.seek dev addr
	// (1)reset chip
	*/
	switch (rst_opt) {
	case RESET_TO_AP:
		{
			ret = reset_chip(p_dev);
			/*(2)sync int pin*/
			if (ret) {
				ret = chip_int_sync(p_dev);
			}
		}
		break;
	case RESET_TP_DSP:
		{
			ret = reset_hold_chip(p_dev);
		}
		break;
	default:
		break;
	}

	return ret;
}

/*******************************************************************************
* Function Name	: chip_mode_select
* Description	: set chip to a pointed work mode
* Input			: PST_TP_DEV p_dev(pointer to a touch panel deice, which should be inited before)
* Input			: s32 work_mode
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 chip_mode_select(IN PST_TP_DEV p_dev, IN u8 work_mode)
{
	s32 ret = 0;
	switch (work_mode) {
	case CHIP_RAW_DATA_MODE:
		ret = board_enter_rawdata_mode(p_dev);
		break;
	case CHIP_COORD_MODE:
		ret = board_enter_coord_mode(p_dev);
		break;
	case CHIP_SLEEP_MODE:
		ret = board_enter_sleep_mode(p_dev);
		break;
	case CHIP_DIFF_DATA_MODE:
		ret = board_enter_diffdata_mode(p_dev);
		break;
	default:
		break;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: reset_chip
* Description	: reset chip,hardware reset or softreset
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 reset_chip(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	if (p_dev->no_rst_pin) {
		ret = soft_reset_chip(p_dev);
	} else {
		/*(1)Judge whether use soft reset*/
		if (p_dev->use_soft_reset) {
			ret = soft_reset_chip(p_dev);
		}
		/*(2)If soft reset fail or no use soft reset*/
		if (0 == ret) {
			ret = board_hard_reset_chip(p_dev);
		}
	}
	/*clear doze_ref_cnt,module power on initialized with doze*/
	p_dev->doze_ref_cnt = 1;
	/*p_dev->hid_state = 1;*/

	return ret;
}
/*******************************************************************************
* Function Name	: wakeup_chip
* Description	: wakeup chip,reset or read
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return 		: s32(0:fail 1:ok)
*******************************************************************************/
s32 wakeup_chip(PST_TP_DEV p_dev)
{
	s32 ret = 1;
	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_wakeup_chip(p_dev);
		}
		break;
	default:
		{
			board_print_warning
				("chip type (%d) is not exist, please check it",
				p_dev->chip_type);
			ret = 0;
		}
		break;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: get_data_flag
* Description	: get data flag
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: s32(0:no flag 1:flag)
*******************************************************************************/
extern s32 get_data_flag(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	u16 i = 0, retry_cnt = 15;
	u8 data_temp;
	for (i = 0; i < retry_cnt; i++) {
		ret = chip_reg_read(p_dev, p_dev->syncflag_addr, &data_temp, 1);
		if ((1 == ret)
			&& (0 != (p_dev->syncflag_mask & data_temp))) {
			ret = 1;
			break;
		} else {
			ret = 0;
		}
		board_delay_ms(2);
	}
	return ret;
}

/*******************************************************************************
* Function Name	: clr_data_flag
* Description	: clr data flag
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 clr_data_flag(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	u8 i = 0, retry_cnt = 3;
	u8 buf_tmp = 0x00;
	for (i = 0; i < retry_cnt; i++) {
		ret = chip_reg_write(p_dev, p_dev->syncflag_addr, &buf_tmp, 1);
		if (1 == ret) {
			break;
		}
	}
	if (0 == ret) {
		board_print_warning("clr_data_flag fail");
	}
	return ret;
}

/*******************************************************************************
* Function Name	: write_cfg
* Description	: write config to tp chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: u8* config(the buffer store cfg)
* Input			: u16 cfg_len(cfg len)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 force_update_cfg(IN PST_TP_DEV p_dev, IN u8 *config,
				IN u16 cfg_len)
{
	s32 ret = 0;
	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_force_update_cfg(p_dev, config, cfg_len);
		}
		break;
	default:
		{
			ret = 0;
			board_print_warning
				("chip type (%d) is not exist, please check it",
				p_dev->chip_type);
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: write_cfg
* Description	: write config to tp chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: s32* cfg1
* Input			: s32* cfg2
* Input			: u16 cfg_len
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 cmp_cfg(IN PST_TP_DEV p_dev, IN u8 *cfg1, IN u8 *cfg2,
			IN u16 cfg_len)
{
	s32 ret = 0;
	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_cmp_cfg(p_dev, cfg1, cfg2, cfg_len);
		}
		break;
	default:
		{
			ret = 0;
			board_print_warning
				("chip type (%d) is not exist, please check it",
				p_dev->chip_type);
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: write_cmd
* Description	: write_cmd
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: u16 cmd_addr
* Input			: s32 cmd_byte
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 write_cmd(IN PST_TP_DEV p_dev, IN u16 cmd_addr,
			IN u8 cmd_byte)
{
	s32 ret = 0;
	switch (p_dev->chip_type) {
	case TP_GT900:
	case TP_ALTO:
		{
			ret = chip_reg_write(p_dev, cmd_addr, &cmd_byte, 1);
		}
		break;

	default:
		{
			u8 buf_tmp[3];
			buf_tmp[0] = cmd_byte;
			buf_tmp[1] = 0;
			buf_tmp[2] = 0x100 - buf_tmp[0] - buf_tmp[1];
			ret = chip_reg_write(p_dev, cmd_addr, buf_tmp, 3);
		}
		break;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: write_cfg
* Description	: write config to tp chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 write_cfg(IN PST_TP_DEV p_dev)
{
	return write_cfg_arr(p_dev, p_dev->cfg, p_dev->cfg_len);
}

/*******************************************************************************
* Function Name	: read_cfg
* Description	: read config from tp chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: PST_TP_DEV p_dev(touch panel deice)
* Return		: s32(0:fail >0:read config len)
*******************************************************************************/
extern s32 read_cfg(IN_OUT PST_TP_DEV p_dev)
{
	return read_cfg_arr(p_dev, p_dev->cfg, p_dev->cfg_len);
}

/*******************************************************************************
* Function Name	: write_cfg_arr
* Description	: write config to tp chip
* Input			: PST_TP_DEV p_dev (touch panel deice)
* Input			: s32* config (the buffer store cfg)
* Input			: u16 cfg_len (cfg len)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 write_cfg_arr(IN PST_TP_DEV p_dev, IN u8 *config,
			IN u16 cfg_len)
{
	s32 ret = 0;
	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_write_cfg(p_dev);
		}
		break;
	default:
		{
			board_print_warning
				("chip type (%d) is not exist, please check it",
				p_dev->chip_type);
			ret = 0;
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: read_cfg
* Description	: read config from tp chip
* Input			: PST_TP_DEV p_dev (touch panel deice)
* Input			: s32* config (the buffer store cfg)
* Input			: u16 cfg_len (cfg len)
* Output		: s32* config
* Return		: s32(0:fail >0:read config len)
*******************************************************************************/
extern s32 read_cfg_arr(IN PST_TP_DEV p_dev, IN_OUT u8 *config,
			IN u16 cfg_len)
{
	s32 ret = 0;
	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_read_cfg_arr(p_dev, config, cfg_len);
		}
		break;
	default:
		{
			board_print_warning
				("chip type (%d) is not exist, please check it",
				p_dev->chip_type);
			ret = 0;
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: cfg_to_flash_delay
* Description	: wait cfg save to flash from ram
* Input			: PST_TP_DEV p_dev(pointer to a touch panel deice, which should be inited before)
* Output		: none
* Return		: s32(0:Fail 1:Ok)
*******************************************************************************/
extern s32 cfg_to_flash_delay(IN PST_TP_DEV p_dev)
{
	s32 ret = 1;
	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			board_delay_ms(600);
		}
		break;
	default:
		{
			board_print_warning
				("chip type (%d) is not exist, please check it",
				p_dev->chip_type);
			ret = 0;
		}
		break;
	}
	return ret;
}

#ifdef __cplusplus
}
#endif

/*************************************Private methods start********************************************/

/*******************************************************************************
* Function Name	: soft_reset_chip
* Description	: soft reset chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
s32 soft_reset_chip(IN PST_TP_DEV p_dev)
{
	s32 ret = 1;
	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_soft_reset_chip(p_dev);
		}
		break;
	default:
		{
			board_print_warning
				("chip type (%d) is not exist, please check it",
				p_dev->chip_type);
			ret = 0;
		}
		break;
	}
return ret;
}

/*******************************************************************************
* Function Name	: reset_hold_chip
* Description	: reset hold chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
static s32 reset_hold_chip(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	switch (p_dev->chip_type) {
	case TP_NORMANDY:
	case TP_OSLO:
		{
			ret = normandy_reset_hold_chip(p_dev);
		}
		break;
	default:
		{
			board_print_warning
				("chip type (%d) is not exist, please check it",
				p_dev->chip_type);
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: chip_int_sync
* Description	: chip int pin sync
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
static s32 chip_int_sync(IN PST_TP_DEV p_dev)
{
	s32 ret = 1;
	/*set gpio mode GPIO_Mode_Out_PP( if you wanna input high)*/
	board_gpio_mode_set(p_dev, TPYE_GPIO_B, MODULE_INT, Mode_Out_PP);
	board_gpio_reset_bits(p_dev, TPYE_GPIO_B, MODULE_INT);
	board_delay_ms(50);
	board_gpio_set_bits(p_dev, TPYE_GPIO_B, MODULE_INT);
	board_gpio_mode_set(p_dev, TPYE_GPIO_B, MODULE_INT, Mode_IN_FLOATING);
	return ret;
}

/*************************************Private methods end********************************************/

#endif

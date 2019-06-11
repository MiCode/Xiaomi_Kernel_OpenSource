/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : board_opr_interface.cpp
* Author             :
* Version            : V1.0.0
* Date               : 08/23/2017
* Description        : board operation interface,you may change this file if you
   use a new test platform
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE
#include "board_opr_interface.h"
#include "tp_dev_control.h"
#include "tp_open_test.h"
#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Function Name  : board_delay_ms
* Description    : delay milliseconds
* Input          : u16 ms
* Output         : none
* Return         : void
*******************************************************************************/
extern void board_delay_ms(u16 ms)
{
#if WCE_CODE == 1

#else
	usleep(ms * 1000);
#endif

}
/*******************************************************************************
* Function Name  : board_delay_us
* Description    : delay microseconds
* Input          : u16 us
* Output         : none
* Return         : void
*******************************************************************************/
extern void board_delay_us(u16 us)
{
#if QNX_CODE == 1
	/*usleep(us);*/
#elif ANDROID_CODE == 1

#endif
}

/*******************************************************************************
* Function Name  : board_delay_s
* Description    : delay seconds
* Input          : u16 s
* Output         : none
* Return         : void
*******************************************************************************/
extern void board_delay_s(u16 s)
{

#if ANDROID_CODE == 1

#elif QNX_CODE == 1

#endif
}

/*******************************************************************************
* Function Name	: seg_read_chip_reg
* Description	: read chip reg segmentally
* Input			: PST_TP_DEV p_dev
				: u16 addr(address)
				: u8* p_buf(data buffer)
				: u16 buf_len(buffer length)
* Output		: u8* p_buf
* Return		: u8(0:have not handled 1:handled)
*******************************************************************************/
static s32 seg_read_chip_reg(PST_TP_DEV p_dev, u16 addr, u8 *p_buf, u16 buf_len)
{
	struct device *dev;
	struct goodix_ts_core *core_data;
	struct goodix_ts_device *ts_dev;
	const struct goodix_ts_hw_ops *hw_ops;
	s32 ret = 1;

	u16 i = 0;
	u16 pack_size = 255;
	u16 code_len = buf_len;
	u16 packages;
	u32 send_len;
	u16 start_addr, tmp_addr;
	start_addr = addr;
	tmp_addr = addr;
	send_len = pack_size;
	packages = code_len / pack_size + 1;
	for (i = 0; i < packages; i++) {
		if (send_len > code_len) {
			send_len = code_len;
		}
		/*read code*/
		dev = (struct device *)p_dev->p_logic_dev;
		core_data = dev_get_drvdata(dev);
		ts_dev = core_data->ts_dev;
		hw_ops = ts_dev->hw_ops;
		ret = hw_ops->read_trans(ts_dev, tmp_addr,
					(u8 *) &p_buf[tmp_addr - start_addr], send_len);
		if (ret < 0)
			ret = 0;
		else
			ret = 1;

		if (ret == 0)
			return 0;
		tmp_addr += send_len;
		code_len -= send_len;
		if (code_len <= 0) {
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: seg_write_chip_reg
* Description	: write chip reg segmentally
* Input			: PST_TP_DEV p_dev
				: u16 addr(address)
				: u8* p_buf(data buffer)
				: u16 buf_len(buffer length)
* Output		: u8* p_buf
* Return        : u8(0:have not handled 1:handled)
*******************************************************************************/
static s32 seg_write_chip_reg(PST_TP_DEV p_dev, u16 addr, u8 *p_buf,
			u16 buf_len)
{
	struct device *dev;
	struct goodix_ts_core *core_data;
	struct goodix_ts_device *ts_dev;
	const struct goodix_ts_hw_ops *hw_ops;
	s32 ret = 1;

	u16 i = 0;
	u16 pack_size = 255;
	u16 code_len = buf_len;
	u16 packages;
	u32 write_len;
	u16 start_addr, tmp_addr;
	start_addr = addr;
	tmp_addr = addr;
	write_len = pack_size;
	packages = code_len / pack_size + 1;
	for (i = 0; i < packages; i++) {
		if (write_len > code_len) {
			write_len = code_len;
		}
		/*send code*/
		dev = (struct device *)p_dev->p_logic_dev;
		core_data = dev_get_drvdata(dev);
		ts_dev = core_data->ts_dev;
		hw_ops = ts_dev->hw_ops;
		ret = hw_ops->write_trans(ts_dev, tmp_addr,
				(u8 *) &p_buf[tmp_addr - start_addr], write_len);
		if (ret < 0) {
			ret = 0;
		} else {
			ret = 1;
		}

		if (ret == 0) {
			/*board_print_error("segment write code error!\n");*/
			return 0;
		}

		tmp_addr += write_len;
		code_len -= write_len;
		if (code_len <= 0) {
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: board_read_chip_reg
* Description	: read data from module
* Input			: PST_TP_DEV p_dev
				: u16 addr(address)
				: u8* p_buf(data buffer)
				: u16 buf_len(buffer length)
* Output		: u8* p_buf
* Return		: u8(0:have not handled 1:handled)
*******************************************************************************/
extern s32 board_read_chip_reg(PST_TP_DEV p_dev, u16 addr, u8 *p_buf, u16 buf_len)
{
	return seg_read_chip_reg(p_dev, addr, p_buf, buf_len);
}
/*******************************************************************************
* Function Name	: board_write_chip_reg
* Description	: write data to module
* Input			: PST_TP_DEV p_dev
				: u16 addr(address)
				: u8* p_buf(data buffer)
				: u16 buf_len(buffer length)
* Output		: none
* Return		: u8(0:have not handled 1:handled)
*******************************************************************************/
extern s32 board_write_chip_reg(PST_TP_DEV p_dev, u16 addr, u8 *p_buf, u16 buf_len)
{
	return seg_write_chip_reg(p_dev, addr, p_buf, buf_len);
}

/*******************************************************************************
* Function Name	: board_hard_reset_chip
* Description	: hard reset chip
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_hard_reset_chip(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	struct device *dev;
	struct goodix_ts_core *core_data;
	struct goodix_ts_device *ts_dev;
	const struct goodix_ts_hw_ops *hw_ops;
	dev = (struct device *)p_dev->p_logic_dev;
	core_data = dev_get_drvdata(dev);
	ts_dev = core_data->ts_dev;
	hw_ops = ts_dev->hw_ops;
	ret = hw_ops->reset(ts_dev);
	if (ret < 0) {
		ret = 0;
	} else {
		ret = 1;
	}
	return ret;
}

/*******************************************************************************
* Function Name	: board_disable_dev_status
* Description	: disable esd ird ...
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_disable_dev_status(PST_TP_DEV p_dev)
{
	s32 ret = 1;
	struct device *dev;
	struct goodix_ts_core *core_data;
	dev = (struct device *)p_dev->p_logic_dev;
	core_data = dev_get_drvdata(dev);
	/*disable esd*/
	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	/*disable irq*/
	goodix_ts_irq_enable(core_data, false);
	return ret;
}

/*******************************************************************************
* Function Name	: board_recovery_dev_status
* Description	: recovey dev status(esd,irq)
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_recovery_dev_status(PST_TP_DEV p_dev)
{
	s32 ret = 1;
	struct device *dev;
	struct goodix_ts_core *core_data;
	dev = (struct device *)p_dev->p_logic_dev;
	core_data = dev_get_drvdata(dev);

	/*disable esd*/
	goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	/*disable irq*/
	goodix_ts_irq_enable(core_data, true);
	return ret;
}

/*******************************************************************************
* Function Name	: board_enter_rawdata_mode
* Description	: chip enter rawdata mode
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_enter_rawdata_mode(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	u8 i = 0;
	u8 retry_cnt = 5;
	board_delay_ms(5);

	for (i = 0; i < retry_cnt; i++) {
		ret = chip_reg_write(p_dev,
				p_dev->cmd_set.rawdata_cmd.addr,
				p_dev->cmd_set.rawdata_cmd.cmd_buf,
				p_dev->cmd_set.rawdata_cmd.cmd_len);
		if (1 == ret) {
			break;
		}
	}

	board_delay_ms(18);
	ret = clr_data_flag(p_dev);
	return ret;
}

/*******************************************************************************
* Function Name	: board_enter_coord_mode
* Description	: set chip enter coordinate mode
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_enter_coord_mode(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	u8 i = 0;
	u8 retry_cnt = 5;

	for (i = 0; i < retry_cnt; i++) {
		ret = chip_reg_write(p_dev,
				p_dev->cmd_set.coorddata_cmd.addr,
				p_dev->cmd_set.coorddata_cmd.cmd_buf,
				p_dev->cmd_set.coorddata_cmd.
				cmd_len);
		if (1 == ret) {
			break;
		}
	}

	board_delay_ms(18);
	ret = clr_data_flag(p_dev);
	return ret;
}

/*******************************************************************************
* Function Name	: board_enter_sleep_mode
* Description	: set chip entering sleep mode
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_enter_sleep_mode(IN PST_TP_DEV p_dev)
{
	s32 ret = 0;
	u8 retry_cnt = 3;
	u8 i = 0;
	for (i = 0; i < retry_cnt; i++) {
		ret = chip_reg_write(p_dev, p_dev->cmd_set.sleep_cmd.addr,
				p_dev->cmd_set.sleep_cmd.cmd_buf,
				p_dev->cmd_set.sleep_cmd.cmd_len);
		if (1 == ret) {
			break;
		}
	}
	return ret;
}

/*******************************************************************************
* Function Name	: board_enter_diffdata_mode
* Description	: set chip entering diffdata mode
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Output		: none
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_enter_diffdata_mode(IN PST_TP_DEV p_dev)
{
	return board_enter_rawdata_mode(p_dev);

}

/*******************************************************************************
* Function Name	: board_get_rawdata
* Description	: board_get_rawdata
* Input			: PST_TP_DEV p_dev(touch panel device)
* Input			: u16 time_out_ms
* Output		: ptr32 p_cur_data
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_get_rawdata(PST_TP_DEV p_dev, u16 time_out_ms,
		u8 b_filter_data, ptr32 p_cur_data)
{
	s32 ret = 0;
	u8 i = 0;
	u8 retry_cnt = 3;
	u8 data_size = 1;
	PST_CUR_RAWDATA p_rawdata = (PST_CUR_RAWDATA) p_cur_data;

	if ((p_dev->chip_type == TP_NORMANDY)
		|| (p_dev->chip_type == TP_OSLO)) {
		if (b_filter_data == 1) {
			return 1;
		}

		data_size = get_data_size(p_dev->rawdata_option);
		for (i = 0; i < retry_cnt; i++) {
			ret = sync_read_rawdata(p_dev->rawdata_addr,
						p_rawdata->p_data_buf,
						p_dev->rawdata_len *
						data_size);
			if (ret >= 0) {
				ret = 1;
				break;
			}
		}
		board_print_debug("rawdata buf len :%d!\n", p_rawdata->data_len);
		if (ret == 1) {
			if ((p_dev->rawdata_option & _DATA_LARGE_ENDIAN)
				|| (p_dev->
				rawdata_option & _DATA_DRV_SEN_INVERT)) {
				reshape_data(p_dev->rawdata_option, p_dev, p_rawdata);
			}
		}
		return ret;
	} else {
		return get_rawdata(p_dev, time_out_ms, b_filter_data, p_rawdata);
	}
}

/*******************************************************************************
* Function Name	: board_get_diffdata
* Description	: board_get_diffdata
* Input			: PST_TP_DEV p_dev(touch panel device)
* Input			: u16 time_out_ms
* Output		: ptr32 p_cur_data
* Return		: u8(0:fail 1:ok)
*******************************************************************************/
extern s32 board_get_diffdata(PST_TP_DEV p_dev, u16 time_out_ms,
		u8 b_filter_data, ptr32 p_cur_data)
{
	s32 ret = 0;
	u8 i = 0;
	u8 retry_cnt = 3;
	u8 data_size = 1;
	PST_CUR_DIFFDATA p_diffdata = (PST_CUR_DIFFDATA) p_cur_data;

	if ((p_dev->chip_type == TP_NORMANDY)
		|| (p_dev->chip_type == TP_OSLO)) {
		if (b_filter_data == 1) {
			return 1;
		}

		data_size = get_data_size(p_dev->diffdata_option);
		for (i = 0; i < retry_cnt; i++) {
			ret = sync_read_rawdata(p_dev->diffdata_addr,
						p_diffdata->p_data_buf,
						p_dev->diffdata_len *
						data_size);
			if (ret > 0) {
				ret = 1;
				break;
			}
		}
		if (ret == 1) {
			if ((p_dev->
				diffdata_option & _DATA_LARGE_ENDIAN)
				|| (p_dev->
				diffdata_option & _DATA_DRV_SEN_INVERT)) {
				reshape_data(p_dev->diffdata_option, p_dev, p_diffdata);
			}
		}
		return ret;
	} else {
		return get_diffdata(p_dev, time_out_ms, b_filter_data, p_diffdata);
	}
}

/*******************************************************************************
* Function Name	: board_hard_reset_chip
* Description	: (use for hid protocol)
* Input			: PST_TP_DEV p_dev(touch panel deice)
* Input			: s32 b_ena(1:enable hid 0:disable hid)
* Output		: none
* Return		: s32(0:fail 1:ok)
*******************************************************************************/
extern s32 board_chip_hid_opr(PST_TP_DEV p_dev, s32 b_ena)
{
	s32 ret = 1;
	s32 i = 0;
	s32 retry_cnt = 3;

	if ((1 == b_ena && 0 == p_dev->hid_state)
		|| (0 == b_ena && 1 == p_dev->hid_state)) {
		p_dev->hid_state = (p_dev->hid_state == 0) ? (1) : (0);
	} else {
		return 1;
	}

	if (1 == b_ena) {
		for (i = 0; i < retry_cnt; i++) {
			ret = board_write_chip_reg(p_dev,
						p_dev->cmd_set.
						hid_ena_cmd.addr,
						p_dev->cmd_set.
						hid_ena_cmd.cmd_buf,
						p_dev->cmd_set.
						hid_ena_cmd.cmd_len);
			/*
			cmd_buf[0] = 0xbb;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x45;
			ret = board_write_chip_reg(p_dev, 0x8040,cmd_buf, 3);
			*/
			if (1 == ret) {
				break;
			}
		}
	} else if (0 == b_ena) {
		for (i = 0; i < retry_cnt; i++) {
			ret = board_write_chip_reg(p_dev,
						p_dev->cmd_set.
						hid_dis_cmd.addr,
						p_dev->cmd_set.
						hid_dis_cmd.cmd_buf,
						p_dev->cmd_set.
						hid_dis_cmd.cmd_len);
			/*
			cmd_buf[0] = 0xaa;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x56;
			ret = board_write_chip_reg(p_dev, 0x8040,cmd_buf, 3);
			*/
			if (1 == ret) {
				break;
			}
		}
	}

	if (0 == ret) {
		board_print_error("%s\n", "send hid opr cmd fail!");
	}
	return ret;
}
extern char *strdup(const char *s)
{
	size_t len = strlen(s) + 1;
	void *new = malloc(len);
	if (new == NULL) {
		return NULL;
	}
	return (char *)memcpy(new, s, len);
}

extern int strtol(const char *nptr, char **endptr, int base)
{
	const char *p = nptr;
	int ret;
	int ch;
	int Overflow;
	int sign = 0, flag, LimitRemainder;
	/*delete space*/
	do {
		ch = *p++;
	} while (ch == ' ');

	if (ch == '-') {
		sign = 1;
		ch = *p++;
	}
	else if (ch == '+')
		ch = *p++;
	if ((base == 0 || base == 16) &&
		ch == '0' && (*p == 'x' || *p == 'X')) {
		ch = p[1];
		p += 2;
		base = 16;
	}
	if (base == 0)
		base = ch == '0' ? 8 : 10;
	Overflow = sign ? -(u32) L_MIN : L_MAX;
	LimitRemainder = Overflow % (u32) base;
	Overflow /= (u32) base;
	for (ret = 0, flag = 0;; ch = *p++) {
		/*get target value*/
		if (ch >= '0' && ch <= '9')
			ch -= '0';
		else if (ch >= 'A' && ch <= 'Z')
			ch -= 'A' - 10;
		else if (ch >= 'a' && ch <= 'z')
			ch -= 'a' - 10;
		else
			break;
		if (ch >= base)
			break;
		/*overflow*/
		if (flag < 0 || ret > Overflow
		|| (ret == Overflow && ch > LimitRemainder))
			flag = -1;
		else {
			flag = 1;
			ret *= base;
			ret += ch;
		}
	}
	if (flag < 0)
		ret = sign ? L_MIN : L_MAX;
	else if (sign)
		ret = -ret;
	if (endptr != 0)
		*endptr = (char *)(flag ? (p - 1) : nptr);
	return ret;
}
/*
extern void* realloc(void* ptr, size_t size){
    void  *new = NULL;
    if (ptr == NULL)
		return NULL;
	if (size != 0){
		if (!(new == kmalloc(size, GFP_KERNEL)))
			return NULL;
		memcpy(new,ptr,strlen((const char*)ptr));
		kfree(ptr);
		return new;
	}else
		return NULL;

}*/

extern void *calloc(size_t n, size_t size)
{
	void *buf = NULL;
	buf = malloc(n * size);
	if (buf != NULL)
		memset(buf, 0, n * size);
	return buf;
}
/****************************************arm interface**************************************************/
extern s32 board_get_exti_flag(PST_TP_DEV p_dev, u8 exti_id)
{
	return 1;
}
extern s32 board_clear_exti_flag(PST_TP_DEV p_dev, u8 exti_id)
{
	return 1;
}

extern u16 board_get_volt_value(PST_TP_DEV p_dev, u8 adc_id, u16 aver_len)
{
	return 1;
}

extern s32 board_set_power_volt(PST_TP_DEV p_dev, u16 volt, u16 aver_len, u8 cnt)
{
	return 1;
}
extern s32 board_set_comm_volt(PST_TP_DEV p_dev, u16 volt, u16 aver_len, u8 cnt)
{
	return 1;
}

extern void board_gpio_mode_set(PST_TP_DEV p_dev, u8 gpio_type,
		u16 gpio_pin, u8 gpio_mode)
{
	;
}
extern void board_gpio_reset_bits(PST_TP_DEV p_dev, u8 gpio_type,
		u16 gpio_pin)
{
	;
}
extern void board_gpio_set_bits(PST_TP_DEV p_dev, u8 gpio_type,
		u16 gpio_pin)
{
	;
}
extern s32 board_set_exti_on(PST_TP_DEV p_dev, u8 exti_id, u16 modle)
{
	return 1;
}
extern void board_get_now_time(u8 *p_tim_buf, u8 *p_len)
{
;
}
extern u32 board_get_avg_current(PST_TP_DEV p_dev, u32 cnt)
{
	return 1;
}

extern s32 board_max_trans_len(PST_TP_DEV p_dev)
{
	return 1024;
}

extern s32 board_seek_dev_addr(PST_TP_DEV p_dev)
{
	return 1;
}

extern s32 board_update_chip_fw(PST_TP_DEV p_dev, ptr32 p_cur_data)
{
	return 0;
}
/****************************************arm interface end**************************************************/

#ifdef __cplusplus
}
#endif

#endif

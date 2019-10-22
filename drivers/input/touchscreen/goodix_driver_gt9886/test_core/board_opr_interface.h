/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : board_opr_interface.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 08/23/2017
* Description        : board operation interface
*******************************************************************************/
#ifndef TP_TEST_MODULE_DISABLE

#ifndef BOARD_OPR_INTERFACE_H
#define BOARD_OPR_INTERFACE_H

#include "user_test_type_def.h"
#include "tp_dev_def.h"
#include "res_inc.h"

#include <linux/kernel.h>
#include "../goodix_ts_core.h"
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/rtc.h>
#include <linux/timex.h>
/*#include <linux/printk.h>*/
/*#include <linux/slab.h>*/

#include <linux/fs.h>
#include <asm/uaccess.h>

#ifdef __cplusplus
extern "C" {
#endif

#define			L_MAX				2147483647		/*0x7FFFFFFF */
#define			L_MIN				(-2147483647-1)	/*-0x80000000*/

#define CONFIG_GOODIX_DEBUG
#ifdef CONFIG_GOODIX_DEBUG
#define board_print_debug(fmt, arg...)		pr_info("[GTP-DBG][%s:%d]"fmt"\n", __func__, __LINE__, ##arg)
#else
#define board_print_debug(fmt, arg...)		do {} while (0)
#endif
#define board_print_error(fmt, arg...)		pr_err("[GTP-ERR][%s:%d]"fmt"\n", __func__, __LINE__, ##arg)
#define board_print_warning(fmt, arg...)	pr_info("[GTP-WARN][%s:%d]"fmt"\n", __func__, __LINE__, ##arg)
#define board_print_info(fmt, arg...)		pr_info("[GTP-INF][%s:%d]"fmt"\n", __func__, __LINE__, ##arg)
#define printf(fmt, arg...)					pr_info("[GTP-INF][%s:%d]"fmt"\n", __func__, __LINE__, ##arg)

#define usleep(ms) msleep(ms/1000)
#define EOF -1
/*********************************str opr**********************************/
extern char *strdup(const char *s);
extern int strtol(const char *nptr, char **endptr, int base);

/*********************************mem opr**********************************/
extern void *malloc(size_t size);
extern void free(void *ptr);
extern void *realloc(void *ptr, size_t size);
extern void *calloc(size_t n, size_t size);

/*********************************adc opr**********************************/
/*such 2 functions only used in arm test board*/
extern u16 board_get_volt_value(PST_TP_DEV p_dev, u8 adc_id, u16 aver_len);
extern u32 board_get_avg_current(PST_TP_DEV p_dev, u32 cnt);

/*********************************dac opr**********************************/
/*such 2 functions only used in arm test board*/
extern s32 board_set_power_volt(PST_TP_DEV p_dev, u16 volt, u16 aver_len, u8 cnt);
extern s32 board_set_comm_volt(PST_TP_DEV p_dev, u16 volt, u16 aver_len, u8 cnt);

/*********************************gpio opr**********************************/
/*such 2 functions only used in arm test board*/
extern void board_gpio_mode_set(PST_TP_DEV p_dev, u8 gpio_type, u16 gpio_pin, u8 gpio_mode);
extern void board_gpio_reset_bits(PST_TP_DEV p_dev, u8 gpio_type, u16 gpio_pin);
extern void board_gpio_set_bits(PST_TP_DEV p_dev, u8 gpio_type, u16 gpio_pin);

/*********************************delay opr**********************************/
extern void board_delay_ms(u16 ms);
extern void board_delay_s(u16 s);
extern void board_delay_us(u16 us);
extern void board_get_now_time(u8 *p_tim_buf, u8 *p_len);

/*********************************communicate opr**********************************/
extern s32 board_read_chip_reg(PST_TP_DEV p_dev, u16 addr, u8 *p_buf, u16 buf_len);
extern s32 board_write_chip_reg(PST_TP_DEV p_dev, u16 addr, u8 *p_buf, u16 buf_len);

/*********************************platform debug**********************************/

/*********************************work mode**********************************/
extern s32 board_enter_rawdata_mode(PST_TP_DEV p_dev);
extern s32 board_enter_coord_mode(PST_TP_DEV p_dev);
extern s32 board_enter_sleep_mode(PST_TP_DEV p_dev);
extern s32 board_enter_diffdata_mode(PST_TP_DEV p_dev);

extern s32 board_get_rawdata(PST_TP_DEV p_dev, u16 time_out_ms, u8 b_filter_data, ptr32 p_cur_data);
extern s32 board_get_diffdata(PST_TP_DEV p_dev, u16 time_out_ms, u8 b_filter_data, ptr32 p_cur_data);

/*********************************chip opr**********************************/
extern s32 board_hard_reset_chip(PST_TP_DEV p_dev);
extern s32 board_chip_hid_opr(PST_TP_DEV p_dev, s32 b_ena);	/*use for hid protocol*/
extern s32 board_disable_dev_status(PST_TP_DEV p_dev);
extern s32 board_recovery_dev_status(PST_TP_DEV p_dev);
extern s32 board_seek_dev_addr(PST_TP_DEV p_dev);
extern s32 board_max_trans_len(PST_TP_DEV p_dev);
extern s32 board_update_chip_fw(PST_TP_DEV p_dev, ptr32 p_cur_data);
#ifdef __cplusplus
}
#endif
#endif
#endif

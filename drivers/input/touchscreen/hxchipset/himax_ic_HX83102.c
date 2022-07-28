/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83102 chipset
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_ic_HX83102.h"
#include "himax_modular.h"

static void hx83102_chip_init(void)
{
	private_ts->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, private_ts->chip_cell_type);
	(IC_CHECKSUM) = HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	(FW_VER_MAJ_FLASH_ADDR) = 49157;  /*0x00C005*/
	(FW_VER_MIN_FLASH_ADDR) = 49158;  /*0x00C006*/
	(CFG_VER_MAJ_FLASH_ADDR) = 49408;  /*0x00C100*/
	(CFG_VER_MIN_FLASH_ADDR) = 49409;  /*0x00C101*/
	(CID_VER_MAJ_FLASH_ADDR) = 49154;  /*0x00C002*/
	(CID_VER_MIN_FLASH_ADDR) = 49155;  /*0x00C003*/
	(CFG_TABLE_FLASH_ADDR) = 0x10000;
	/*PANEL_VERSION_ADDR = 49156;*/  /*0x00C004*/
	(CFG_TABLE_FLASH_ADDR_T) = (CFG_TABLE_FLASH_ADDR);
}

static void hx83102e_chip_init(void)
{
	private_ts->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, private_ts->chip_cell_type);
	(IC_CHECKSUM) = HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	(FW_VER_MAJ_FLASH_ADDR) = 59397;  /*0x00E805*/
	(FW_VER_MIN_FLASH_ADDR) = 59398;  /*0x00E806*/
	(CFG_VER_MAJ_FLASH_ADDR) = 59648;  /*0x00E900*/
	(CFG_VER_MIN_FLASH_ADDR) = 59649;  /*0x00E901*/
	(CID_VER_MAJ_FLASH_ADDR) = 59394;  /*0x00E802*/
	(CID_VER_MIN_FLASH_ADDR) = 59395;  /*0x00E803*/
	(CFG_TABLE_FLASH_ADDR) = 0x10000;
	/*PANEL_VERSION_ADDR = 59396;*/  /*0x00E804*/
	(CFG_TABLE_FLASH_ADDR_T) = (CFG_TABLE_FLASH_ADDR);
}

static void hx83102j_chip_init(void)
{
	private_ts->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, private_ts->chip_cell_type);
	(IC_CHECKSUM) = HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	(FW_VER_MAJ_FLASH_ADDR) = 59397;  /*0x00E805*/
	(FW_VER_MIN_FLASH_ADDR) = 59398;  /*0x00E806*/
	(CFG_VER_MAJ_FLASH_ADDR) = 59648;  /*0x00E900*/
	(CFG_VER_MIN_FLASH_ADDR) = 59649;  /*0x00E901*/
	(CID_VER_MAJ_FLASH_ADDR) = 59394;  /*0x00E802*/
	(CID_VER_MIN_FLASH_ADDR) = 59395;  /*0x00E803*/
	(CFG_TABLE_FLASH_ADDR) = 0x10000;
	/*PANEL_VERSION_ADDR = 59396;*/  /*0x00E804*/
	(CFG_TABLE_FLASH_ADDR_T) = (CFG_TABLE_FLASH_ADDR);
}

void hx83102_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[4];
	int ret = 0;

	tmp_data[0] = 0x31;

	ret = himax_bus_write(0x13, NULL, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (0x10 | auto_add_4_byte);

	ret = himax_bus_write(0x0D, NULL, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return;
	}
}

int hx83102_register_write(uint8_t *addr, uint8_t *data)
{
	int ret = 0;

	ret = himax_bus_write(0x00, addr, data, 8);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return BUS_FAIL;
	}

	return 0;
}

static int hx83102_register_read(uint8_t *addr, uint8_t *buf, int len)
{
	uint8_t tmp_data[4];
	int ret = 0;

	if (len > 256) {
		E("%s: read len over 256!\n", __func__);
		return LENGTH_FAIL;
	}

	if (len > 4)
		hx83102_burst_enable(1);
	else
		hx83102_burst_enable(0);

	ret = himax_bus_write(0x00, addr, NULL, 4);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return BUS_FAIL;
	}
	tmp_data[0] = 0x00;

	ret = himax_bus_write(0x0C, NULL, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return BUS_FAIL;
	}

	ret = himax_bus_read(0x08, buf, len);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return BUS_FAIL;
	}

	return 0;
}

#if defined(HX_RST_PIN_FUNC)
static void hx83102_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	usleep_range(RST_LOW_PERIOD_S, RST_LOW_PERIOD_E);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	usleep_range(RST_HIGH_PERIOD_S, RST_HIGH_PERIOD_E);
}
#endif

static bool hx83102_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_writ[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	do {
		if (cnt == 0
		|| (tmp_data[0] != 0xA5
		&& tmp_data[0] != 0x00
		&& tmp_data[0] != 0x87)) {
			tmp_addr[3] = 0x90;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x5C;

			tmp_writ[3] = 0x00;
			tmp_writ[2] = 0x00;
			tmp_writ[1] = 0x00;
			tmp_writ[0] = 0xA5;
			hx83102_register_write(tmp_addr, tmp_writ);
		}
		msleep(20);

		/* check fw status */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83102_register_read(tmp_addr, tmp_data, DATA_LEN_4);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
				__func__, tmp_data[0]);
			break;
		}

		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x5C;
		hx83102_register_read(tmp_addr, tmp_data, DATA_LEN_4);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
			cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 50) && check_en == true);

	cnt = 0;

	do {
		/**
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		ret = himax_bus_write(0x31, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/**
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		ret = himax_bus_write(0x32, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/**
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83102_register_read(tmp_addr, tmp_data, ADDR_LEN_4);
		I("%s: Check enter_save_mode data[0]=%X\n",
			__func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/**
			 *Reset TCON
			 */
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			hx83102_register_write(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			/**
			 *Reset ADC
			 */
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x94;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			hx83102_register_write(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			hx83102_register_write(tmp_addr, tmp_data);

			return true;
		}
		usleep_range(10000, 10001);
#if defined(HX_RST_PIN_FUNC)
		hx83102_pin_reset();
#endif

	} while (cnt++ < 15);

	return false;
}

#if defined(HX_ZERO_FLASH)
static void hx83102j_reload_to_active(void)
{
	uint8_t addr[DATA_LEN_4] = {0};
	uint8_t data[DATA_LEN_4] = {0};
	uint8_t retry_cnt = 0;

	addr[3] = 0x90;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x48;

	do {
		data[3] = 0x00;
		data[2] = 0x00;
		data[1] = 0x00;
		data[0] = 0xEC;
		g_core_fp.fp_register_write(addr, data, DATA_LEN_4);
		usleep_range(1000, 1100);
		g_core_fp.fp_register_read(addr, data, DATA_LEN_4);
		I("%s: data[1]=%d, data[0]=%d, retry_cnt=%d\n", __func__,
				data[1], data[0], retry_cnt);
		retry_cnt++;
	} while ((data[1] != 0x01
		|| data[0] != 0xEC)
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void hx83102j_resume_ic_action(void)
{
#if !defined(HX_RESUME_HW_RESET)
	hx83102j_reload_to_active();
#endif
}
#endif

static void hx83102j_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};
	int ret = 0;

	I("Enter %s\n", __func__);
	private_ts->notouch_frame = private_ts->ic_notouch_frame;
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	/*msleep(20);*/
	usleep_range(10000, 11000);
	if (!FlashMode) {
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_system_reset();
#endif
	} else {

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], NULL, tmp_data,
			1);
		if (ret < 0) {
			E("%s: cmd=%x bus access fail!\n",
			__func__,
			pic_op->adr_i2c_psw_lb[0]);
		}

		ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0], NULL, tmp_data,
			1);
		if (ret < 0) {
			E("%s: cmd=%x bus access fail!\n",
			__func__,
			pic_op->adr_i2c_psw_ub[0]);
		}
	}
#if defined(HX_ZERO_FLASH)
	hx83102j_reload_to_active();
#endif
}

static bool hx83102j_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	do {
		if (cnt == 0
		|| (tmp_data[0] != 0xA5
		&& tmp_data[0] != 0x00
		&& tmp_data[0] != 0x87))
			g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
				pfw_op->data_fw_stop, DATA_LEN_4);
		/*msleep(20);*/
		usleep_range(10000, 10001);

		/* check fw status */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state,
			tmp_data, ADDR_LEN_4);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
					__func__, tmp_data[0]);
			break;
		}

		g_core_fp.fp_register_read(pfw_op->addr_ctrl_fw_isr, tmp_data,
			4);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
				cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 35) && check_en == true);

	cnt = 0;

	do {
		/**
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		ret = himax_bus_write(0x31, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/**
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		ret = himax_bus_write(0x32, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/**
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		g_core_fp.fp_register_read(tmp_addr, tmp_data, ADDR_LEN_4);
		I("%s: Check enter_save_mode data[0]=%X\n",
				__func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/**
			 *Reset TCON
			 */
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			g_core_fp.fp_register_write(tmp_addr, tmp_data, 4);
			usleep_range(1000, 1001);
			return true;
		}
		/*msleep(10);*/
		usleep_range(5000, 5001);
#if defined(HX_RST_PIN_FUNC)
		himax_rst_gpio_set(private_ts->rst_gpio, 0);
		msleep(20);
		himax_rst_gpio_set(private_ts->rst_gpio, 1);
		msleep(50);
#endif

	} while (cnt++ < 5);

	return false;
}

static bool hx83102j_read_event_stack(uint8_t *buf, uint8_t length)
{
	struct timespec64 t_start, t_end, t_delta;
	int len = length;
	int i2c_speed = 0;

	if (private_ts->debug_log_level & BIT(2))
		ktime_get_ts64(&t_start);

	himax_bus_read(pfw_op->addr_event_addr[0], buf, length);

	if (private_ts->debug_log_level & BIT(2)) {
		ktime_get_ts64(&t_end);
		t_delta.tv_nsec = (t_end.tv_sec * 1000000000 + t_end.tv_nsec)
					- (t_start.tv_sec
					* 1000000000
					+ t_start.tv_nsec); /*ns*/

		i2c_speed = (len * 9 * 1000000
			/ (int)t_delta.tv_nsec) * 13 / 10;
		private_ts->bus_speed = (int)i2c_speed;
		}
	return 1;
}

static void himax_hx83102j_reg_re_init(void)
{
	I("%s:Entering!\n", __func__);
	private_ts->ic_notouch_frame = hx83102j_notouch_frame;
	himax_parse_assign_cmd(hx83102j_fw_addr_raw_out_sel,
			pfw_op->addr_raw_out_sel,
			sizeof(pfw_op->addr_raw_out_sel));
	himax_parse_assign_cmd(hx83102j_data_df_rx,
			(pdriver_op)->data_df_rx,
			sizeof((pdriver_op)->data_df_rx));
	himax_parse_assign_cmd(hx83102j_data_df_tx,
			(pdriver_op)->data_df_tx,
			sizeof((pdriver_op)->data_df_tx));
	himax_parse_assign_cmd(hx83102j_ic_adr_tcon_rst,
			pic_op->addr_tcon_on_rst,
			sizeof(pic_op->addr_tcon_on_rst));
}

static void himax_hx83102j_func_re_init(void)
{
	I("%s:Entering!\n", __func__);

	g_core_fp.fp_chip_init = hx83102j_chip_init;
	g_core_fp.fp_sense_on = hx83102j_sense_on;
	g_core_fp.fp_sense_off = hx83102j_sense_off;
	g_core_fp.fp_read_event_stack = hx83102j_read_event_stack;

#if defined(HX_ZERO_FLASH)
	g_core_fp.fp_resume_ic_action = hx83102j_resume_ic_action;
	g_core_fp.fp_0f_reload_to_active = hx83102j_reload_to_active;
#endif
}

#if defined(HX_ZERO_FLASH)
static void hx83102e_reload_to_active(void)
{
	uint8_t addr[DATA_LEN_4] = {0};
	uint8_t data[DATA_LEN_4] = {0};
	uint8_t retry_cnt = 0;

	addr[3] = 0x90;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x48;

	do {
		data[3] = 0x00;
		data[2] = 0x00;
		data[1] = 0x00;
		data[0] = 0xEC;
		g_core_fp.fp_register_write(addr, data, DATA_LEN_4);
		usleep_range(1000, 1100);
		g_core_fp.fp_register_read(addr, data, DATA_LEN_4);
		I("%s: data[1]=%d, data[0]=%d, retry_cnt=%d\n", __func__,
				data[1], data[0], retry_cnt);
		retry_cnt++;
	} while ((data[1] != 0x01
		|| data[0] != 0xEC)
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void hx83102e_resume_ic_action(void)
{
#if !defined(HX_RESUME_HW_RESET)
	hx83102e_reload_to_active();
#endif
}
#endif

static void hx83102e_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};
	int ret = 0;

	I("Enter %s\n", __func__);
	private_ts->notouch_frame = private_ts->ic_notouch_frame;
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	/*msleep(20);*/
	usleep_range(10000, 11000);
	if (!FlashMode) {
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_system_reset();
#endif
	} else {

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], NULL, tmp_data,
			1);
		if (ret < 0) {
			E("%s: cmd=%x bus access fail!\n",
			__func__,
			pic_op->adr_i2c_psw_lb[0]);
		}

		ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0], NULL, tmp_data,
			1);
		if (ret < 0) {
			E("%s: cmd=%x bus access fail!\n",
			__func__,
			pic_op->adr_i2c_psw_ub[0]);
		}
	}
#if defined(HX_ZERO_FLASH)
	hx83102e_reload_to_active();
#endif
}

static bool hx83102e_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	do {
		if (cnt == 0
		|| (tmp_data[0] != 0xA5
		&& tmp_data[0] != 0x00
		&& tmp_data[0] != 0x87))
			g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
				pfw_op->data_fw_stop, DATA_LEN_4);
		/*msleep(20);*/
		usleep_range(10000, 10001);

		/* check fw status */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state,
			tmp_data, ADDR_LEN_4);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
					__func__, tmp_data[0]);
			break;
		}

		g_core_fp.fp_register_read(pfw_op->addr_ctrl_fw_isr, tmp_data,
			4);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
				cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 35) && check_en == true);

	cnt = 0;

	do {
		/**
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		ret = himax_bus_write(0x31, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/**
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		ret = himax_bus_write(0x32, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/**
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		g_core_fp.fp_register_read(tmp_addr, tmp_data, ADDR_LEN_4);
		I("%s: Check enter_save_mode data[0]=%X\n",
				__func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/**
			 *Reset TCON
			 */
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			g_core_fp.fp_register_write(tmp_addr, tmp_data, 4);
			usleep_range(1000, 1001);
			return true;
		}
		/*msleep(10);*/
		usleep_range(5000, 5001);
#if defined(HX_RST_PIN_FUNC)
		himax_rst_gpio_set(private_ts->rst_gpio, 0);
		msleep(20);
		himax_rst_gpio_set(private_ts->rst_gpio, 1);
		msleep(50);
#endif

	} while (cnt++ < 5);

	return false;
}

static bool hx83102e_read_event_stack(uint8_t *buf, uint8_t length)
{
	struct timespec64 t_start, t_end, t_delta;
	int len = length;
	int i2c_speed = 0;

	if (private_ts->debug_log_level & BIT(2))
		ktime_get_ts64(&t_start);

	himax_bus_read(pfw_op->addr_event_addr[0], buf, length);

	if (private_ts->debug_log_level & BIT(2)) {
		ktime_get_ts64(&t_end);
		t_delta.tv_nsec = (t_end.tv_sec * 1000000000 + t_end.tv_nsec)
					- (t_start.tv_sec
					* 1000000000
					+ t_start.tv_nsec); /*ns*/

		i2c_speed = (len * 9 * 1000000
			/ (int)t_delta.tv_nsec) * 13 / 10;
		private_ts->bus_speed = (int)i2c_speed;
		}
	return 1;
}

static void himax_hx83102e_reg_re_init(void)
{
	I("%s:Entering!\n", __func__);
	private_ts->ic_notouch_frame = hx83102e_notouch_frame;
	himax_parse_assign_cmd(hx83102e_fw_addr_raw_out_sel,
			pfw_op->addr_raw_out_sel,
			sizeof(pfw_op->addr_raw_out_sel));
	himax_parse_assign_cmd(hx83102e_data_df_rx,
			(pdriver_op)->data_df_rx,
			sizeof((pdriver_op)->data_df_rx));
	himax_parse_assign_cmd(hx83102e_data_df_tx,
			(pdriver_op)->data_df_tx,
			sizeof((pdriver_op)->data_df_tx));
	himax_parse_assign_cmd(hx83102e_ic_adr_tcon_rst,
			pic_op->addr_tcon_on_rst,
			sizeof(pic_op->addr_tcon_on_rst));
}

static void himax_hx83102e_func_re_init(void)
{
	I("%s:Entering!\n", __func__);

	g_core_fp.fp_chip_init = hx83102e_chip_init;
	g_core_fp.fp_sense_on = hx83102e_sense_on;
	g_core_fp.fp_sense_off = hx83102e_sense_off;
	g_core_fp.fp_read_event_stack = hx83102e_read_event_stack;

#if defined(HX_ZERO_FLASH)
	g_core_fp.fp_resume_ic_action = hx83102e_resume_ic_action;
	g_core_fp.fp_0f_reload_to_active = hx83102e_reload_to_active;
#endif
}

#if defined(HX_EXCP_RECOVERY)
#if defined(HX_ZERO_FLASH)
static int hx83102d_0f_excp_check(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	int ret = NO_ERR;

	I("Enter %s\n", __func__);

	g_core_fp.fp_register_read(pzf_op->addr_sts_chk, tmp_data,
		DATA_LEN_4);

	if (tmp_data[0] != (pzf_op)->data_activ_sts[0]) {
		ret = ERR_STS_WRONG;
		I("%s:status : %8X = %2X\n", __func__,
			zf_addr_sts_chk, tmp_data[0]);
	}

	g_core_fp.fp_register_read(pzf_op->addr_activ_relod, tmp_data,
		DATA_LEN_4);

	if (tmp_data[0] != (pzf_op)->data_activ_in[0]) {
		ret = ERR_STS_WRONG;
		I("%s:status : %8X = %2X\n", __func__,
			zf_addr_activ_relod, tmp_data[0]);
	}

	return ret;
}
#endif
#endif

static bool hx83102d_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	do {
		if (cnt == 0
		|| (tmp_data[0] != 0xA5
		&& tmp_data[0] != 0x00
		&& tmp_data[0] != 0x87))
			g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
				pfw_op->data_fw_stop, DATA_LEN_4);
		/*msleep(20);*/
		usleep_range(10000, 10001);

		/* check fw status */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state,
			tmp_data, DATA_LEN_4);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
				__func__, tmp_data[0]);
			break;
		}

		g_core_fp.fp_register_read(pfw_op->addr_ctrl_fw_isr, tmp_data,
			4);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
			cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 10) && check_en == true);

	cnt = 0;

	do {
		/*
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		ret = himax_bus_write(0x31, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/*
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		ret = himax_bus_write(0x32, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		/*
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		g_core_fp.fp_register_read(tmp_addr, tmp_data, ADDR_LEN_4);
		I("%s: Check enter_save_mode data[0]=%X\n",
			__func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/*
			 *Reset TCON
			 */
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			g_core_fp.fp_register_write(tmp_addr, tmp_data, 4);
			usleep_range(1000, 1001);

			/*
			 *Reset ADC
			 */
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x94;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			g_core_fp.fp_register_write(tmp_addr, tmp_data, 4);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			g_core_fp.fp_register_write(tmp_addr, tmp_data, 4);

			return true;
		}
		/*msleep(10);*/
		usleep_range(5000, 5001);

#if defined(HX_RST_PIN_FUNC)
		himax_rst_gpio_set(private_ts->rst_gpio, 0);
		msleep(20);
		himax_rst_gpio_set(private_ts->rst_gpio, 1);
		msleep(50);
#endif

	} while (cnt++ < 5);

	return false;
}

#if defined(HX_ZERO_FLASH)
static void himax_hx83102d_reload_to_active(void)
{
	uint8_t addr[DATA_LEN_4] = {0};
	uint8_t data[DATA_LEN_4] = {0};
	uint8_t retry_cnt = 0;

	addr[3] = 0x90;
	addr[2] = 0x00;
	addr[1] = 0x00;
	addr[0] = 0x48;

	do {
		data[3] = 0x00;
		data[2] = 0x00;
		data[1] = 0x00;
		data[0] = 0xEC;
		g_core_fp.fp_register_write(addr, data, DATA_LEN_4);
		usleep_range(1000, 1100);
		g_core_fp.fp_register_read(addr, data, DATA_LEN_4);
		I("%s: data[1]=%d, data[0]=%d, retry_cnt=%d\n", __func__,
				data[1], data[0], retry_cnt);
		retry_cnt++;
	} while ((data[1] != 0x01
		|| data[0] != 0xEC)
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_hx83102d_resume_ic_action(void)
{
#if !defined(HX_RESUME_HW_RESET)
	himax_hx83102d_reload_to_active();
#endif
}

static void himax_hx83102d_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4];
	int retry = 0;
	int ret = 0;

	I("Enter %s\n", __func__);
	private_ts->notouch_frame = private_ts->ic_notouch_frame;
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	/*msleep(20);*/
	usleep_range(10000, 10001);
	if (!FlashMode) {
#if defined(HX_RST_PIN_FUNC)
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_system_reset();
#endif
	} else {
		do {
			g_core_fp.fp_register_read(
				pfw_op->addr_flag_reset_event,
				tmp_data, DATA_LEN_4);
			I("%s:Read status from IC = %X,%X\n", __func__,
					tmp_data[0], tmp_data[1]);
		} while ((tmp_data[1] != 0x01
			|| tmp_data[0] != 0x00)
			&& retry++ < 5);

		if (retry >= 5) {
			E("%s: Fail:\n", __func__);
#if defined(HX_RST_PIN_FUNC)
			g_core_fp.fp_ic_reset(false, false);
#else
			g_core_fp.fp_system_reset();
#endif
		} else {
			I("%s:OK and Read status from IC = %X,%X\n", __func__,
				tmp_data[0], tmp_data[1]);
			/* reset code*/
			tmp_data[0] = 0x00;

			ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], NULL,
				tmp_data, 1);
			if (ret < 0) {
				E("%s: cmd=%x bus access fail!\n",
				__func__,
				pic_op->adr_i2c_psw_lb[0]);
			}

			ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0], NULL,
				tmp_data, 1);
			if (ret < 0) {
				E("%s: cmd=%x bus access fail!\n",
				__func__,
				pic_op->adr_i2c_psw_ub[0]);
			}
		}
	}
	himax_hx83102d_reload_to_active();
}

#if defined(HX_0F_DEBUG)
static void hx83102d_firmware_read_0f(const struct firmware *fw_entry,
		int type)
{
	uint8_t tmp_data[4];

	I("%s, Entering\n", __func__);

	switch (type) {
	case 0:
		g_core_fp.fp_read_sram_0f(fw_entry,
			(pzf_op)->data_sram_start_addr, 0, HX_40K_SZ);
		break;
	case 1:
		g_core_fp.fp_read_sram_0f(fw_entry,
				(pzf_op)->data_cfg_info, 0xC000, 132);
		break;
	case 2:
		g_core_fp.fp_read_sram_0f(fw_entry,
				(pzf_op)->data_fw_cfg_1, 0xC0FE, 484);
		break;
	case 3:
		g_core_fp.fp_read_sram_0f(fw_entry,
				(pzf_op)->data_adc_cfg_1, 0xD000, 768);
		break;
	case 4:
		g_core_fp.fp_read_sram_0f(fw_entry,
				(pzf_op)->data_adc_cfg_2, 0xD300, 1536);
		break;
	case 5:
		g_core_fp.fp_read_sram_0f(fw_entry,
				(pzf_op)->data_adc_cfg_3, 0xE000, 1536);
		break;
	case 6:
		himax_parse_assign_cmd(hx83102d_zf_data_bor_prevent_info,
			tmp_data, 4);
		g_core_fp.fp_read_sram_0f(fw_entry, tmp_data, 0xC9E0, 32);
		break;
	case 7:
		himax_parse_assign_cmd(hx83102d_zf_data_notch_info,
			tmp_data, 4);
		g_core_fp.fp_read_sram_0f(fw_entry, tmp_data,
			0xCA00, 128);
		break;
	case 8:
		himax_parse_assign_cmd(hx83102d_zf_func_info_en,
			tmp_data, 4);
		g_core_fp.fp_read_sram_0f(fw_entry, tmp_data, 0xCB00, 12);
		break;
	case 9:
		himax_parse_assign_cmd(hx83102d_zf_po_sub_func,
			tmp_data, 4);
		g_core_fp.fp_read_sram_0f(fw_entry, tmp_data,
			0xA000, HX4K);
		break;
	default:
		break;
	}
	I("%s, END\n", __func__);
}
#endif

#if defined(HX_CODE_OVERLAY)
static int hx83102d_0f_overlay(int ovl_type, int mode)
{
	int ret = 0;
	uint8_t count = 0;
	uint8_t count2 = 0;
	const struct firmware *fwp = NULL;
	uint8_t part_num = 0;
	uint8_t buf[16];
	uint8_t handshaking_addr[4] = {0xFC, 0x7F, 0x00, 0x10};
	/*uint8_t mpap_addr[4] = {0xEC, 0x73, 0x00, 0x10};*/
	uint8_t ovl_idx_t;
	uint8_t request;
	uint8_t reply;
	uint8_t sram_addr[4] = {0};
	uint32_t offset = 0;
	uint32_t size = 0;
	uint8_t send_data[4] = {0};
	uint8_t recv_data[4] = {0};
	char firmware_name[HIMAX_FIRMWARE_LINE] = { 0 };

	snprintf(firmware_name, sizeof(firmware_name), "%s%s.bin",
		 BOOT_UPGRADE_FWNAME, private_ts->panel_name);
	ret = request_firmware(&fwp, firmware_name,
		private_ts->dev);
	if (ret < 0) {
		E("%s: request firmware FAIL!!!\n", __func__);
		return ret;
	}

	/*1. get number of partition*/
	part_num = fwp->data[HX64K + 12];
	if (part_num <= 1) {
		E("%s, size of cfg part failed! part_num = %d\n",
				__func__, part_num);
		return LENGTH_FAIL;
	}

	I("%s: overlay section %d\n", __func__, ovl_type-1);
	if (ovl_type == 2) {
		request = ovl_gesture_request;
		reply = ovl_gesture_reply;
	} else if (ovl_type == 3) {
		request = ovl_border_request;
		reply = ovl_border_reply;
	} else {
		E("%s: error overlay type %d\n", __func__, ovl_type);
		return HX_INIT_FAIL;
	}
	ovl_idx_t = *((ovl_idx) + ovl_type - 1);
	memcpy(buf, &fwp->data[ovl_idx_t * 0x10 + HX64K], 16);
	memcpy(sram_addr, buf, 4);
	offset = buf[11]<<24 | buf[10]<<16 | buf[9]<<8 | buf[8];
	size = buf[7]<<24 | buf[6]<<16 | buf[5]<<8 | buf[4];

/*
 *	if (ovl_type == 1) {
 *		if (mode == 0) {
 *			//border overlay when finish self_test
 *			send_data[3] = 0x00;
 *			send_data[2] = 0x00;
 *			send_data[1] = 0x00;
 *			send_data[0] = 0x00;
 *			offset = zf_info_arr[2].fw_addr;
 *			size = zf_info_arr[2].write_size;
 *			I("%s: self test overlay section 2\n", __func__);
 *		} else if (mode == 1) {
 *			//sorting overlay when call self_test
 *			send_data[3] = 0x00;
 *			send_data[2] = 0x10;
 *			send_data[1] = 0x73;
 *			send_data[0] = 0x80;
 *			offset = zf_info_arr[0].fw_addr;
 *			size = zf_info_arr[0].write_size;
 *			I("%s: self test overlay section 0\n", __func__);
 *		}
 *	}
 */

/*
 *	if (ovl_type == 1) {
 *
 *		g_core_fp.fp_sense_off(false);
 *
 *		//g_core_fp.fp_register_write(mpap_addr, DATA_LEN_4,
			send_data, 0);
 *		count = 0;
 *		do {
 *			g_core_fp.fp_register_write(mpap_addr, DATA_LEN_4,
				send_data, 0);
 *			g_core_fp.fp_register_read(mpap_addr, DATA_LEN_4,
				recv_data, 0);
 *		} while ((recv_data[0] != send_data[0]
 *			 || recv_data[1] != send_data[1]
 *			 || recv_data[2] != send_data[2]
 *			 || recv_data[3] != send_data[3])
 *			 && count++ < 10);
 *
 *		I("%s: set mpap pw %d times\n", __func__, count);
 *
 *		//g_core_fp.fp_write_sram_0f(fwp, sram_addr, offset, size);
 *		if (g_core_fp.fp_write_sram_0f_crc(fwp, sram_addr,
		offset, size) != 0)
 *			E("%s, Overlay HW CRC FAIL\n", __func__);
 *
 *		g_core_fp.fp_reload_disable(0);
 *
 *		g_core_fp.fp_sense_on(0x00);
 *
 *	} else {
 */

	count = 0;
	do {
		g_core_fp.fp_register_read(handshaking_addr, recv_data,
			DATA_LEN_4);
	} while (recv_data[0] != request && count++ < 10);

	if (count < 10) {
		/*g_core_fp.fp_write_sram_0f(fwp, sram_addr, offset, size);*/
		if (g_core_fp.fp_write_sram_0f_crc(sram_addr,
		&fwp->data[offset], size) != 0)
			E("%s, Overlay HW CRC FAIL\n", __func__);

		send_data[3] = 0x00;
		send_data[2] = 0x00;
		send_data[1] = 0x00;
		send_data[0] = reply;

		count2 = 0;
		do {
			g_core_fp.fp_register_write(handshaking_addr,
				send_data, DATA_LEN_4);
			usleep_range(1000, 1100);
			g_core_fp.fp_register_read(handshaking_addr, recv_data,
				DATA_LEN_4);
		} while (recv_data[0] != reply && count2++ < 10);

		if (ovl_type == 3) {
#if defined(HX_RST_PIN_FUNC)
			g_core_fp.fp_ic_reset(false, false);
#else
			g_core_fp.fp_system_reset();
#endif
		}
	}

	I("%s: overlay request %d times; reply %d times\n", __func__,
			count, count2);

	release_firmware(fwp);

	/* rescue mechanism */
	if (count >= 10)
		g_core_fp.fp_0f_op_file_dirly(firmware_name);

	return 0;
}
#endif
#endif

static void himax_hx83102d_reg_re_init(void)
{
	I("%s:Entering!\n", __func__);
	private_ts->ic_notouch_frame = hx83102d_notouch_frame;
	himax_parse_assign_cmd(hx83102d_fw_addr_raw_out_sel,
			pfw_op->addr_raw_out_sel,
			sizeof(pfw_op->addr_raw_out_sel));
#if defined(HX_ZERO_FLASH)
	himax_parse_assign_cmd(hx83102d_zf_data_sram_start_addr,
			(pzf_op)->data_sram_start_addr,
			sizeof((pzf_op)->data_sram_start_addr));
	himax_parse_assign_cmd(hx83102d_zf_data_adc_cfg_1,
			(pzf_op)->data_adc_cfg_1,
			sizeof((pzf_op)->data_adc_cfg_1));
	himax_parse_assign_cmd(hx83102d_zf_data_adc_cfg_2,
			(pzf_op)->data_adc_cfg_2,
			sizeof((pzf_op)->data_adc_cfg_2));
	himax_parse_assign_cmd(hx83102d_zf_data_adc_cfg_3,
			(pzf_op)->data_adc_cfg_3,
			sizeof((pzf_op)->data_adc_cfg_3));
#endif
	himax_parse_assign_cmd(hx83102d_adr_osc_en,
			pic_op->adr_osc_en,
			sizeof(pic_op->adr_osc_en));
	himax_parse_assign_cmd(hx83102d_adr_osc_pw,
			pic_op->adr_osc_pw,
			sizeof(pic_op->adr_osc_pw));
}

static void himax_hx83102d_func_re_init(void)
{
	I("%s:Entering!\n", __func__);
	g_core_fp.fp_chip_init = hx83102_chip_init;
	g_core_fp.fp_sense_off = hx83102d_sense_off;
#if defined(HX_ZERO_FLASH)
	/*g_core_fp.fp_firmware_update_0f = hx83102d_firmware_update_0f;*/
	g_core_fp.fp_resume_ic_action = himax_hx83102d_resume_ic_action;
	g_core_fp.fp_sense_on = himax_hx83102d_sense_on;
	g_core_fp.fp_0f_reload_to_active = himax_hx83102d_reload_to_active;
#if defined(HX_0F_DEBUG)
	g_core_fp.fp_firmware_read_0f = hx83102d_firmware_read_0f;
#endif
#if defined(HX_CODE_OVERLAY)
	g_core_fp.fp_0f_overlay = hx83102d_0f_overlay;
#endif
#if defined(HX_EXCP_RECOVERY)
	g_core_fp.fp_0f_excp_check = hx83102d_0f_excp_check;
#endif
#endif
}

static bool hx83102_chip_detect(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];
	bool ret_data = false;
	int ret = 0;
	int i = 0;


#if defined(HX_RST_PIN_FUNC)
	hx83102_pin_reset();
#endif

	ret = himax_bus_read(0x13, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return false;
	}
#if defined(HX_ZERO_FLASH)
	if (hx83102_sense_off(false) == false) {
#else
	if (hx83102_sense_off(true) == false) {
#endif
		ret_data = false;
		E("%s:hx83102_sense_off Fail:\n", __func__);
		return ret_data;
	}

	for (i = 0; i < 5; i++) {
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xD0;
		ret = hx83102_register_read(tmp_addr, tmp_data, DATA_LEN_4);
		if (ret != 0) {
			ret_data = false;
			E("%s:hx83102_register_read Fail:\n", __func__);
			return ret_data;
		}
		I("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3],
			tmp_data[2], tmp_data[1]); /*83,10,2X*/

		if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x10)
		&& ((tmp_data[1] == 0x2d)
		|| (tmp_data[1] == 0x2e) || (tmp_data[1] == 0x29))) {
			if (tmp_data[1] == 0x2d) {
				strlcpy(private_ts->chip_name,
					HX_83102D_SERIES_PWON, 30);
				(ic_data)->ic_adc_num =
					hx83102d_data_adc_num;
				I("%s:detect IC HX83102D successfully\n",
					__func__);
			} else if (tmp_data[1] == 0x29) {
				strlcpy(private_ts->chip_name,
					HX_83102J_SERIES_PWON, 30);
				(ic_data)->ic_adc_num =
					hx83102j_data_adc_num;
				I("%s:detect IC HX83102J successfully\n",
					__func__);
			} else {/* 0x2e */
				strlcpy(private_ts->chip_name,
					HX_83102E_SERIES_PWON, 30);
				(ic_data)->ic_adc_num =
					hx83102e_data_adc_num;
				I("%s:detect IC HX83102E successfully\n",
					__func__);
			}

			ret = himax_mcu_in_cmd_struct_init();
			if (ret < 0) {
				ret_data = false;
				E("%s:cmd_struct_init Fail:\n", __func__);
				return ret_data;
			}

			himax_mcu_in_cmd_init();
			if (tmp_data[1] == 0x2d) {
				himax_hx83102d_reg_re_init();
				himax_hx83102d_func_re_init();
			} else if (tmp_data[1] == 0x29) {
				himax_hx83102j_reg_re_init();
				himax_hx83102j_func_re_init();
			} else {/* 0x2e */
				himax_hx83102e_reg_re_init();
				himax_hx83102e_func_re_init();
			}
			ret_data = true;
			break;
		}

		ret_data = false;
		E("%s:Read driver ID register Fail:\n", __func__);
		E("Could NOT find Himax Chipset\n");
		E("Please check 1.VCCD,VCCA,VSP,VSN\n");
		E("2. LCM_RST,TP_RST\n");
		E("3. Power On Sequence\n");

	}

	return ret_data;
}

bool _hx83102_init(void)
{
	bool ret = false;

	I("%s\n", __func__);
	ret = hx83102_chip_detect();
	return ret;
}

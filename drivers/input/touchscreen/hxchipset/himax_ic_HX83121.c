/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83121 chipset
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

#include "himax_ic_HX83121.h"
#include "himax_modular.h"

static void hx83121a_chip_init(void)
{
	private_ts->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, private_ts->chip_cell_type);
	(IC_CHECKSUM) = HX_TP_BIN_CHECKSUM_CRC;
	(FW_VER_MAJ_FLASH_ADDR) = 0x21405;
	(FW_VER_MIN_FLASH_ADDR) = 0x21406;
	(CFG_VER_MAJ_FLASH_ADDR) = 0x21500;
	(CFG_VER_MIN_FLASH_ADDR) = 0x21501;
	(CID_VER_MAJ_FLASH_ADDR) = 0x21402;
	(CID_VER_MIN_FLASH_ADDR) = 0x21403;
	(CFG_TABLE_FLASH_ADDR) = 0x20000;
	(CFG_TABLE_FLASH_ADDR_T) = (CFG_TABLE_FLASH_ADDR);
}

static void hx83121_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[4];
	int ret = 0;

	tmp_data[0] = 0x31;

	ret = himax_bus_write(0x13, NULL, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (0x12 | auto_add_4_byte);

	ret = himax_bus_write(0x0D, NULL, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return;
	}
}

static int hx83121_register_write(uint8_t *addr, uint8_t *data)
{
	int ret = -1;

	ret = himax_bus_write(0x00, addr, data, 8);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return BUS_FAIL;
	}

	return 0;
}

static int hx83121_register_read(uint8_t *addr, uint8_t *buf, int len)
{
	uint8_t tmp_data[4];
	int ret = 0;

	if (len > 256) {
		E("%s: read len over 256!\n", __func__);
		return LENGTH_FAIL;
	}

	if (len > 4)
		hx83121_burst_enable(1);
	else
		hx83121_burst_enable(0);

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
static void hx83121_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	usleep_range(RST_LOW_PERIOD_S, RST_LOW_PERIOD_E);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	usleep_range(RST_HIGH_PERIOD_S, RST_HIGH_PERIOD_E);
}
#endif

static bool hx83121_sense_off(bool check_en)
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
			hx83121_register_write(tmp_addr, tmp_writ);
		}
		msleep(20);

		/* check fw status */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83121_register_read(tmp_addr, tmp_data, DATA_LEN_4);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
				__func__, tmp_data[0]);
			break;
		}

		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x5C;
		hx83121_register_read(tmp_addr, tmp_data, DATA_LEN_4);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
			cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 50) && check_en == true);

	cnt = 0;

	do {
		tmp_data[0] = 0x27;
		ret = himax_bus_write(0x31, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}
		tmp_data[0] = 0x95;
		ret = himax_bus_write(0x32, NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83121_register_read(tmp_addr, tmp_data, ADDR_LEN_4);
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
			hx83121_register_write(tmp_addr, tmp_data);
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
			hx83121_register_write(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			hx83121_register_write(tmp_addr, tmp_data);

			return true;
		}
		usleep_range(10000, 10001);
#if defined(HX_RST_PIN_FUNC)
		hx83121_pin_reset();
#endif

	} while (cnt++ < 15);

	return false;
}

static void hx83121a_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};
	int ret = 0;

	I("Enter %s\n", __func__);
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
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
}

static bool hx83121a_sense_off(bool check_en)
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

static bool hx83121a_read_event_stack(uint8_t *buf, uint8_t length)
{
	struct timespec t_start, t_end, t_delta;
	int len = length;
	int i2c_speed = 0;

	if (private_ts->debug_log_level & BIT(2))
		getnstimeofday(&t_start);

	himax_bus_read(pfw_op->addr_event_addr[0], buf, length);

	if (private_ts->debug_log_level & BIT(2)) {
		getnstimeofday(&t_end);
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

static void himax_hx83121a_reg_re_init(void)
{
	I("%s:Entering!\n", __func__);
	private_ts->ic_notouch_frame = hx83121a_notouch_frame;

	himax_parse_assign_cmd(hx83121a_fw_addr_raw_out_sel,
			pfw_op->addr_raw_out_sel,
			sizeof(pfw_op->addr_raw_out_sel));
	himax_parse_assign_cmd(hx83121a_data_df_rx,
			(pdriver_op)->data_df_rx,
			sizeof((pdriver_op)->data_df_rx));
	himax_parse_assign_cmd(hx83121a_data_df_tx,
			(pdriver_op)->data_df_tx,
			sizeof((pdriver_op)->data_df_tx));
	himax_parse_assign_cmd(hx83121a_ic_cmd_incr4,
			pic_op->data_incr4,
			sizeof(pic_op->data_incr4));
}

static void himax_hx83121a_func_re_init(void)
{
	I("%s:Entering!\n", __func__);

	g_core_fp.fp_chip_init = hx83121a_chip_init;
	g_core_fp.fp_sense_on = hx83121a_sense_on;
	g_core_fp.fp_sense_off = hx83121a_sense_off;
	g_core_fp.fp_read_event_stack = hx83121a_read_event_stack;
}

static bool hx83121_chip_detect(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];
	bool ret_data = false;
	int ret = 0;
	int i = 0;

#if defined(HX_RST_PIN_FUNC)
	hx83121_pin_reset();
#endif

	ret = himax_bus_read(0x13, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return false;
	}

#if defined(HX_ZERO_FLASH)
	if (hx83121_sense_off(false) == false) {
#else
	if (hx83121_sense_off(true) == false) {
#endif
		ret_data = false;
		E("%s:hx83121_sense_off Fail:\n", __func__);
		return ret_data;
	}

	for (i = 0; i < 5; i++) {
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xD0;
		ret = hx83121_register_read(tmp_addr, tmp_data, DATA_LEN_4);
		if (ret != 0) {
			ret_data = false;
			E("%s:hx83121_register_read Fail:\n", __func__);
			return ret_data;
		}
		I("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3],
			tmp_data[2], tmp_data[1]); /*83,10,2X*/

		if (tmp_data[3] == 0x83 && tmp_data[2] == 0x12
		&& (tmp_data[1] == 0x1a)) {
			strlcpy(private_ts->chip_name,
				HX_83121A_SERIES_PWON,
				30);

			(ic_data)->ic_adc_num = hx83121a_data_adc_num;

			(ic_data)->flash_size = HX83121A_FLASH_SIZE;

			I("%s:detect IC HX83121A successfully\n", __func__);

			ret = himax_mcu_in_cmd_struct_init();
			if (ret < 0) {
				ret_data = false;
				E("%s:cmd_struct_init Fail:\n", __func__);
				return ret_data;
			}

			himax_mcu_in_cmd_init();

			himax_hx83121a_reg_re_init();
			himax_hx83121a_func_re_init();

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

bool _hx83121_init(void)
{
	bool ret = false;

	I("%s\n", __func__);
	ret = hx83121_chip_detect();
	return ret;
}

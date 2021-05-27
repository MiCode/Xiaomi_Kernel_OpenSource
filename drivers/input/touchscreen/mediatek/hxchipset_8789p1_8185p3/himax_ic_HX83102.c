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
	(*kp_private_ts)->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, (*kp_private_ts)->chip_cell_type);
	(*kp_IC_CHECKSUM) = HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	(*kp_FW_VER_MAJ_FLASH_ADDR) = 49157;  /*0x00C005*/
	(*kp_FW_VER_MIN_FLASH_ADDR) = 49158;  /*0x00C006*/
	(*kp_CFG_VER_MAJ_FLASH_ADDR) = 49408;  /*0x00C100*/
	(*kp_CFG_VER_MIN_FLASH_ADDR) = 49409;  /*0x00C101*/
	(*kp_CID_VER_MAJ_FLASH_ADDR) = 49154;  /*0x00C002*/
	(*kp_CID_VER_MIN_FLASH_ADDR) = 49155;  /*0x00C003*/
	(*kp_CFG_TABLE_FLASH_ADDR) = 0x10000;
	/*PANEL_VERSION_ADDR = 49156;*/  /*0x00C004*/
}

static void hx83102e_chip_init(void)
{
	(*kp_private_ts)->chip_cell_type = CHIP_IS_IN_CELL;
	I("%s:IC cell type = %d\n", __func__, (*kp_private_ts)->chip_cell_type);
	(*kp_IC_CHECKSUM) = HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	(*kp_FW_VER_MAJ_FLASH_ADDR) = 59397;  /*0x00E805*/
	(*kp_FW_VER_MIN_FLASH_ADDR) = 59398;  /*0x00E806*/
	(*kp_CFG_VER_MAJ_FLASH_ADDR) = 59648;  /*0x00E900*/
	(*kp_CFG_VER_MIN_FLASH_ADDR) = 59649;  /*0x00E901*/
	(*kp_CID_VER_MAJ_FLASH_ADDR) = 59394;  /*0x00E802*/
	(*kp_CID_VER_MIN_FLASH_ADDR) = 59395;  /*0x00E803*/
	(*kp_CFG_TABLE_FLASH_ADDR) = 0x10400;
	/*PANEL_VERSION_ADDR = 59396;*/  /*0x00E804*/
}

void hx83102_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[4];
	int ret = 0;

	tmp_data[0] = 0x31;

	ret = kp_himax_bus_write(0x13, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (0x10 | auto_add_4_byte);

	ret = kp_himax_bus_write(0x0D, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

int hx83102_flash_write_burst(uint8_t *reg_byte, uint8_t *write_data)
{
	uint8_t data_byte[8];
	int i = 0, j = 0, ret = 0;

	for (i = 0; i < 4; i++)
		data_byte[i] = reg_byte[i];
	for (j = 4; j < 8; j++)
		data_byte[j] = write_data[j-4];

	ret = kp_himax_bus_write(0x00, data_byte, 8, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	return 0;
}

static int hx83102_register_read(uint8_t *read_addr, int read_length,
		uint8_t *read_data)
{
	uint8_t tmp_data[4];
	int i = 0;
	int address = 0;
	int ret = 0;

	if (read_length > 256) {
		E("%s: read len over 256!\n", __func__);
		return LENGTH_FAIL;
	}
	if (read_length > 4)
		hx83102_burst_enable(1);
	else
		hx83102_burst_enable(0);

	address = (read_addr[3] << 24)
		+ (read_addr[2] << 16)
		+ (read_addr[1] << 8)
		+ read_addr[0];

	i = address;
	tmp_data[0] = (uint8_t)i;
	tmp_data[1] = (uint8_t)(i >> 8);
	tmp_data[2] = (uint8_t)(i >> 16);
	tmp_data[3] = (uint8_t)(i >> 24);

	ret = kp_himax_bus_write(0x00, tmp_data, 4,
		HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	tmp_data[0] = 0x00;

	ret = kp_himax_bus_write(0x0C, tmp_data, 1,
		HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}

	ret = kp_himax_bus_read(0x08, read_data, read_length,
		HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	if (read_length > 4)
		hx83102_burst_enable(0);

	return 0;
}

#if defined(HX_RST_PIN_FUNC)
static void hx83102_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	kp_himax_rst_gpio_set((*kp_private_ts)->rst_gpio, 0);
	usleep_range(RST_LOW_PERIOD_S, RST_LOW_PERIOD_E);
	kp_himax_rst_gpio_set((*kp_private_ts)->rst_gpio, 1);
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
			hx83102_flash_write_burst(tmp_addr, tmp_writ);
		}
		msleep(20);

		/* check fw status */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83102_register_read(tmp_addr, DATA_LEN_4, tmp_data);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
				__func__, tmp_data[0]);
			break;
		}

		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x5C;
		hx83102_register_read(tmp_addr, DATA_LEN_4, tmp_data);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
			cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 50) && check_en == true);

	cnt = 0;

	do {
		/**
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		ret = kp_himax_bus_write(0x31, tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		ret = kp_himax_bus_write(0x32, tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83102_register_read(tmp_addr, ADDR_LEN_4, tmp_data);
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
			hx83102_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			hx83102_flash_write_burst(tmp_addr, tmp_data);
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
			hx83102_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			hx83102_flash_write_burst(tmp_addr, tmp_data);

			return true;
		}
		usleep_range(10000, 10001);
#if defined(HX_RST_PIN_FUNC)
		hx83102_pin_reset();
#endif

	} while (cnt++ < 15);

	return false;
}

static bool hx83102ab_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	do {
		/**
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		ret = kp_himax_bus_write(0x31, tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		ret = kp_himax_bus_write(0x32, tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83102_register_read(tmp_addr, ADDR_LEN_4, tmp_data);
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
			hx83102_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			hx83102_flash_write_burst(tmp_addr, tmp_data);
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
			hx83102_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			hx83102_flash_write_burst(tmp_addr, tmp_data);

			return true;
		}
		/*msleep(10);*/
		usleep_range(5000, 5001);
#if defined(HX_RST_PIN_FUNC)
		kp_himax_rst_gpio_set((*kp_private_ts)->rst_gpio, 0);
		msleep(20);
		kp_himax_rst_gpio_set((*kp_private_ts)->rst_gpio, 1);
		msleep(50);
#endif

	} while (cnt++ < 5);

	return false;
}

static void hx83102ab_set_SMWP_enable(uint8_t SMWP_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	kp_g_core_fp->fp_sense_off(true);

	do {
		if (SMWP_enable) {
			kp_himax_parse_assign_cmd(fw_func_handshaking_pwd,
				tmp_data, 4);
			kp_g_core_fp->fp_register_write(
				(*kp_pfw_op)->addr_smwp_enable,
				DATA_LEN_4,
				tmp_data,
				0);
			kp_himax_parse_assign_cmd(fw_func_handshaking_pwd,
				back_data, 4);
		} else {
			kp_himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				tmp_data,
				4);
			kp_g_core_fp->fp_register_write(
				(*kp_pfw_op)->addr_smwp_enable,
				DATA_LEN_4,
				tmp_data,
				0);
			kp_himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				back_data,
				4);
		}

		kp_g_core_fp->fp_register_read((*kp_pfw_op)->addr_smwp_enable,
				DATA_LEN_4, tmp_data, false);
		/*I("%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0],SMWP_enable,retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3]
		|| tmp_data[2] != back_data[2]
		|| tmp_data[1] != back_data[1]
		|| tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);

	kp_g_core_fp->fp_sense_on(0x00);
}

static void hx83102ab_set_HSEN_enable(uint8_t HSEN_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];

	if (HSEN_enable) {
		kp_himax_parse_assign_cmd(fw_func_handshaking_pwd,
			tmp_data, 4);
		kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_hsen_enable,
			DATA_LEN_4, tmp_data, 0);
	} else {
		kp_himax_parse_assign_cmd(
			fw_data_safe_mode_release_pw_reset,
			tmp_data,
			4);
		kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_hsen_enable,
				DATA_LEN_4, tmp_data, 0);
	}

	/*I("%s: tmp_data[0]=%d, HSEN_enable=%d, retry_cnt=%d\n", __func__,
	 *		tmp_data[0],HSEN_enable,retry_cnt);
	 */
}

static void hx83102ab_usb_detect_set(uint8_t *cable_config)
{
	uint8_t tmp_data[DATA_LEN_4];

	if (cable_config[1] == 0x01) {
		kp_himax_parse_assign_cmd(fw_func_handshaking_pwd,
			tmp_data, 4);
		kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_usb_detect,
			DATA_LEN_4, tmp_data, 0);
		I("%s: USB detect status IN!\n", __func__);
	} else {
		kp_himax_parse_assign_cmd(
			fw_data_safe_mode_release_pw_reset,
			tmp_data,
			4);
		kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_usb_detect,
			DATA_LEN_4, tmp_data, 0);
		I("%s: USB detect status OUT!\n", __func__);
	}
}

static uint8_t hx83102ab_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data)
{
	int cnt = 0;
	uint8_t req_size = cmd_set[0];

	cmd_set[3] = (*kp_pfw_op)->data_dd_request[0];

	kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_dd_handshak_addr,
			DATA_LEN_4, cmd_set, 0);
	I("%s:cmd_set[0]=0x%02X,set[1]=0x%02X,set[2]=0x%02X,set[3]=0x%02X\n",
		__func__, cmd_set[0], cmd_set[1], cmd_set[2], cmd_set[3]);

	/* Doing hand shaking 0xAA -> 0xBB */
	for (cnt = 0; cnt < 100; cnt++) {
		kp_g_core_fp->fp_register_read(
			(*kp_pfw_op)->addr_dd_handshak_addr,
			DATA_LEN_4,
			tmp_data,
			false);
		usleep_range(10000, 10001);

		if (tmp_data[3] == (*kp_pfw_op)->data_dd_ack[0]) {
			I("%s Data ready goto moving data\n", __func__);
			break;
		}
		if (cnt >= 99) {
			I("%s Data not ready in FW\n", __func__);
			return FW_NOT_READY;
		}
	}

	kp_g_core_fp->fp_sense_off(true);
	kp_g_core_fp->fp_register_read((*kp_pfw_op)->addr_dd_data_addr,
			req_size, tmp_data, false);
	kp_g_core_fp->fp_sense_on(0x01);
	return NO_ERR;
}

static void hx83102ab_power_on_init(void)
{
#if 0
	uint8_t tmp_data[DATA_LEN_4];

	I("%s:\n", __func__);
	kp_himax_parse_assign_cmd(fw_data_safe_mode_release_pw_reset,
			tmp_data, 4);
	kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_raw_out_sel,
			DATA_LEN_4, tmp_data, 0);
	kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_sorting_mode_en,
			DATA_LEN_4, tmp_data, 0);
	kp_g_core_fp->fp_touch_information();
	kp_g_core_fp->fp_calc_touch_data_size();
	/*kp_g_core_fp->fp_sense_on(0x00);*/
#endif

	uint8_t data[4] = {0};
	uint8_t retry = 0;

	/*RawOut select initial*/
	kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_raw_out_sel,
		DATA_LEN_4, data, 0);
	/*DSRAM func initial*/
	kp_g_core_fp->fp_assign_sorting_mode(data);
	/*FW reload done initial*/
	kp_g_core_fp->fp_register_write(
		(*kp_pdriver_op)->addr_fw_define_2nd_flash_reload,
		DATA_LEN_4, data, 0);

	kp_g_core_fp->fp_sense_on(0x00);

	I("%s: waiting for FW reload done\n", __func__);

	while (retry++ <= 30) {
		kp_g_core_fp->fp_register_read(
			(*kp_pdriver_op)->addr_fw_define_2nd_flash_reload,
			DATA_LEN_4, data, 0);

		/* use all 4 bytes to compare */
		if ((data[3] == 0x00 && data[2] == 0x00 &&
			data[1] == 0x72 && data[0] == 0xC0)) {
			I("%s: FW finish reload done\n", __func__);
			break;
		}
		I("%s: wait reload done %d times\n", __func__, retry);
		kp_g_core_fp->fp_read_FW_status();
		usleep_range(10000, 11000);
	}
}

static int hx83102ab_fts_ctpm_fw_upgrade_with_sys_fs_64k(unsigned char *fw,
		int len, bool change_iref)
{
	int burnFW_success = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	if (len != FW_SIZE_64k) {
		E("%s: The file size is not 64K bytes\n", __func__);
		return false;
	}

#if defined(HX_RST_PIN_FUNC)
	kp_g_core_fp->fp_ic_reset(false, false);
#else
	kp_g_core_fp->fp_system_reset();
#endif
	kp_g_core_fp->fp_sense_off(true);
	kp_g_core_fp->fp_chip_erase();
	kp_g_core_fp->fp_flash_programming(fw, FW_SIZE_64k);

	ret = kp_g_core_fp->fp_check_CRC(
		(*kp_pfw_op)->addr_program_reload_from,
		FW_SIZE_64k);
	if (ret == 0)
		burnFW_success = 1;

	kp_himax_parse_assign_cmd(fw_data_safe_mode_release_pw_reset,
			tmp_data, 4);
	kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_raw_out_sel,
			DATA_LEN_4, tmp_data, 0);
	kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_sorting_mode_en,
			DATA_LEN_4, tmp_data, 0);
#if defined(HX_RST_PIN_FUNC)
	kp_g_core_fp->fp_ic_reset(false, false);
#else
	/*System reset*/
	kp_g_core_fp->fp_system_reset();
#endif
	return burnFW_success;
}

#if defined(HX_EXCP_RECOVERY)
static void hx83102ab_excp_ic_reset(void)
{
	(*kp_HX_EXCP_RESET_ACTIVATE) = 1;
#if defined(HX_RST_PIN_FUNC)
	kp_g_core_fp->fp_pin_reset();
#else
	kp_g_core_fp->fp_system_reset();
#endif
	I("%s:\n", __func__);
}
#if defined(HX_ZERO_FLASH)
static int hx83102d_0f_excp_check(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	int ret = NO_ERR;

	I("Enter %s\n", __func__);

	kp_g_core_fp->fp_register_read((*kp_pzf_op)->addr_sts_chk,
			DATA_LEN_4, tmp_data, 0);

	if (tmp_data[0] != (*kp_pzf_op)->data_activ_sts[0]) {
		ret = ERR_STS_WRONG;
		I("%s:status : %8X = %2X\n", __func__,
			zf_addr_sts_chk, tmp_data[0]);
	}

	kp_g_core_fp->fp_register_read((*kp_pzf_op)->addr_activ_relod,
			DATA_LEN_4, tmp_data, 0);

	if (tmp_data[0] != (*kp_pzf_op)->data_activ_in[0]) {
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
			kp_g_core_fp->fp_register_write(
				(*kp_pfw_op)->addr_ctrl_fw_isr,
				DATA_LEN_4,
				(*kp_pfw_op)->data_fw_stop,
				0);
		/*msleep(20);*/
		usleep_range(10000, 10001);

		/* check fw status */
		kp_g_core_fp->fp_register_read(
			(*kp_pic_op)->addr_cs_central_state,
			ADDR_LEN_4,
			tmp_data,
			0);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
				__func__, tmp_data[0]);
			break;
		}

		kp_g_core_fp->fp_register_read((*kp_pfw_op)->addr_ctrl_fw_isr,
			4, tmp_data, false);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
			cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 10) && check_en == true);

	cnt = 0;

	do {
		/*
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		ret = kp_himax_bus_write(0x31, tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		ret = kp_himax_bus_write(0x32, tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83102_register_read(tmp_addr, ADDR_LEN_4, tmp_data);
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
			hx83102_flash_write_burst(tmp_addr, tmp_data);
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
			hx83102_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			hx83102_flash_write_burst(tmp_addr, tmp_data);

			return true;
		}
		/*msleep(10);*/
		usleep_range(5000, 5001);

#if defined(HX_RST_PIN_FUNC)
		kp_himax_rst_gpio_set((*kp_private_ts)->rst_gpio, 0);
		msleep(20);
		kp_himax_rst_gpio_set((*kp_private_ts)->rst_gpio, 1);
		msleep(50);
#endif

	} while (cnt++ < 5);

	return false;
}

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
		kp_g_core_fp->fp_register_write(addr, DATA_LEN_4, data, 0);
		usleep_range(1000, 1100);
		kp_g_core_fp->fp_register_read(addr, DATA_LEN_4, data, 0);
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

static void hx83102e_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};
	int ret = 0;

	I("Enter %s\n", __func__);
	(*kp_private_ts)->notouch_frame = (*kp_private_ts)->ic_notouch_frame;
	kp_g_core_fp->fp_interface_on();
	kp_g_core_fp->fp_register_write(
		(*kp_pfw_op)->addr_ctrl_fw_isr,
		sizeof((*kp_pfw_op)->data_clear),
		(*kp_pfw_op)->data_clear,
		0);
	/*msleep(20);*/
	usleep_range(10000, 11000);
	if (!FlashMode) {
#ifdef HX_RST_PIN_FUNC
		kp_g_core_fp->fp_ic_reset(false, false);
#else
		kp_g_core_fp->fp_system_reset();
#endif

		hx83102e_reload_to_active();
	} else {

		ret = kp_himax_bus_write(
				(*kp_pic_op)->adr_i2c_psw_lb[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: i2c access fail!\n", __func__);

		ret = kp_himax_bus_write(
				(*kp_pic_op)->adr_i2c_psw_ub[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: i2c access fail!\n", __func__);

		hx83102e_reload_to_active();
	}
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
			kp_g_core_fp->fp_register_write(
				(*kp_pfw_op)->addr_ctrl_fw_isr,
				DATA_LEN_4,
				(*kp_pfw_op)->data_fw_stop,
				0);
		/*msleep(20);*/
		usleep_range(10000, 10001);

		/* check fw status */
		kp_g_core_fp->fp_register_read(
			(*kp_pic_op)->addr_cs_central_state,
			ADDR_LEN_4,
			tmp_data, 0);

		if (tmp_data[0] != 0x05) {
			I("%s: Do not need wait FW, Status = 0x%02X!\n",
					__func__, tmp_data[0]);
			break;
		}

		kp_g_core_fp->fp_register_read((*kp_pfw_op)->addr_ctrl_fw_isr,
				4, tmp_data, false);
		I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__,
				cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 35) && check_en == true);

	cnt = 0;

	do {
		/**
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = 0x27;
		ret = kp_himax_bus_write(0x31, tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = 0x95;
		ret = kp_himax_bus_write(0x32, tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/**
		 *Check enter_save_mode
		 */
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		hx83102_register_read(tmp_addr, ADDR_LEN_4, tmp_data);
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
			hx83102_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			return true;
		}
		/*msleep(10);*/
		usleep_range(5000, 5001);
#if defined(HX_RST_PIN_FUNC)
		kp_himax_rst_gpio_set((*kp_private_ts)->rst_gpio, 0);
		msleep(20);
		kp_himax_rst_gpio_set((*kp_private_ts)->rst_gpio, 1);
		msleep(50);
#endif

	} while (cnt++ < 5);

	return false;
}

static bool hx83102d_dd_clk_set(bool enable)
{
	uint8_t data[4] = {0};

	data[0] = (enable)?0xDD:0;
	return (kp_g_core_fp->fp_register_write((*kp_pic_op)->adr_osc_en,
		sizeof((*kp_pic_op)->adr_osc_en), data, 0) == NO_ERR);
}

static void hx83102d_dd_reg_en(bool enable)
{
	uint8_t data[4] = {0};

	data[0] = 0xA5;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	kp_g_core_fp->fp_register_write((*kp_pic_op)->adr_osc_pw,
		DATA_LEN_4, data, 0);

	data[0] = 0xA5;
	data[1] = 0x83;
	data[2] = 0x10;
	data[3] = 0x2D;
	kp_g_core_fp->fp_dd_reg_write(0xB9, 0, 4, data, 0);
}

static bool hx83102d_ic_id_read(void)
{
	int i = 0;
	uint8_t data[4] = {0};

	kp_g_core_fp->fp_dd_clk_set(true);
	kp_g_core_fp->fp_dd_reg_en(true);

	for (i = 0; i < 13; i++) {
		data[0] = 0x2A + i;
		kp_g_core_fp->fp_dd_reg_write(0xBB, 3, 1, data, 0);
		data[0] = 0x08;
		kp_g_core_fp->fp_dd_reg_write(0xBB, 1, 1, data, 0);
		kp_g_core_fp->fp_dd_reg_read(0xBB, 6, 1, data, 0);
		(*kp_ic_data)->vendor_ic_id[i] = data[0];
		I("ic_data->vendor_ic_id[%d] = %02X\n", i,
			(*kp_ic_data)->vendor_ic_id[i]);
	}

	kp_g_core_fp->fp_dd_clk_set(false);

	return true;
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
		kp_g_core_fp->fp_register_write(addr, DATA_LEN_4, data, 0);
		usleep_range(1000, 1100);
		kp_g_core_fp->fp_register_read(addr, DATA_LEN_4, data, 0);
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
	(*kp_private_ts)->notouch_frame = (*kp_private_ts)->ic_notouch_frame;
	kp_g_core_fp->fp_interface_on();
	kp_g_core_fp->fp_register_write((*kp_pfw_op)->addr_ctrl_fw_isr,
		sizeof((*kp_pfw_op)->data_clear), (*kp_pfw_op)->data_clear, 0);
	/*msleep(20);*/
	usleep_range(10000, 10001);
	if (!FlashMode) {
#if defined(HX_RST_PIN_FUNC)
		kp_g_core_fp->fp_ic_reset(false, false);
#else
		kp_g_core_fp->fp_system_reset();
#endif
	} else {
		do {
			kp_g_core_fp->fp_register_read(
				(*kp_pfw_op)->addr_flag_reset_event,
				DATA_LEN_4, tmp_data, 0);
			I("%s:Read status from IC = %X,%X\n", __func__,
					tmp_data[0], tmp_data[1]);
		} while ((tmp_data[1] != 0x01
			|| tmp_data[0] != 0x00)
			&& retry++ < 5);

		if (retry >= 5) {
			E("%s: Fail:\n", __func__);
#if defined(HX_RST_PIN_FUNC)
			kp_g_core_fp->fp_ic_reset(false, false);
#else
			kp_g_core_fp->fp_system_reset();
#endif
		} else {
			I("%s:OK and Read status from IC = %X,%X\n", __func__,
				tmp_data[0], tmp_data[1]);
			/* reset code*/
			tmp_data[0] = 0x00;

			ret = kp_himax_bus_write(
				(*kp_pic_op)->adr_i2c_psw_lb[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);

				ret = kp_himax_bus_write(
					(*kp_pic_op)->adr_i2c_psw_ub[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);
		}
	}
	himax_hx83102d_reload_to_active();
}

static void hx83102ab_firmware_update_0f(const struct firmware *fw_entry)
{
	uint8_t tmp_addr[4];

	I("%s, Entering\n", __func__);

	kp_g_core_fp->fp_register_write((*kp_pzf_op)->addr_system_reset, 4,
			(*kp_pzf_op)->data_system_reset, 0);

	kp_g_core_fp->fp_sense_off(false);

	/* first 32K */
	/*bin file start*/
	/*isram*/
	kp_g_core_fp->fp_write_sram_0f(fw_entry,
			(*kp_pzf_op)->data_sram_start_addr, 0, HX_32K_SZ);

	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x73;
	tmp_addr[0] = 0x00;
	kp_g_core_fp->fp_write_sram_0f(fw_entry, tmp_addr, 0x8000, (0x200*4));

	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x68;
	tmp_addr[0] = 0x00;
	kp_g_core_fp->fp_write_sram_0f(fw_entry, tmp_addr, 0x8800, (0x200*4));

	/*last 16k*/
	/*config info*/
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x72;
	tmp_addr[0] = 0x04; /*0x10007206 <= size 60,from 0x0000C180*/
	kp_g_core_fp->fp_write_sram_0f(fw_entry, tmp_addr, 0xC178, (84));

	/*FW config*/
	if ((*kp_G_POWERONOF) == 1)
		kp_g_core_fp->fp_write_sram_0f(fw_entry,
			(*kp_pzf_op)->data_fw_cfg_1, 0xBFFE, 388);
	else
		kp_g_core_fp->fp_clean_sram_0f((*kp_pzf_op)->data_fw_cfg_1,
			388, 1);
	/*ADC config*/
	if ((*kp_G_POWERONOF) == 1)
		kp_g_core_fp->fp_write_sram_0f(fw_entry,
			(*kp_pzf_op)->data_adc_cfg_1, 0xe000, (128 * 4));
	else
		kp_g_core_fp->fp_clean_sram_0f((*kp_pzf_op)->data_adc_cfg_1,
			(128 * 4), 2);

	I("%s, END\n", __func__);
}
#if defined(HX_0F_DEBUG)
static void hx83102ab_firmware_read_0f(const struct firmware *fw_entry,
		int type)
{
	uint8_t tmp_addr[4];

	I("%s, Entering\n", __func__);
	if (type == 0) { /* first 32K */
		kp_g_core_fp->fp_read_sram_0f(fw_entry,
			(*kp_pzf_op)->data_sram_start_addr, 0, HX_32K_SZ);
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x68;
		tmp_addr[0] = 0x00;
		kp_g_core_fp->fp_read_sram_0f(fw_entry, tmp_addr,
			0x8000, (0x200*4));

		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x73;
		tmp_addr[0] = 0x00;
		kp_g_core_fp->fp_read_sram_0f(fw_entry, tmp_addr,
			0x8800, (0x200*4));

	} else { /*last 16k*/
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x72;
		tmp_addr[0] = 0x04; /*0x10007206 <= size 80,from 0x0000C180*/
		kp_g_core_fp->fp_read_sram_0f(fw_entry, tmp_addr,
				0xC178, (84));

		kp_g_core_fp->fp_read_sram_0f(fw_entry,
				(*kp_pzf_op)->data_fw_cfg_1, 0xBFFE, 388);
		kp_g_core_fp->fp_read_sram_0f(fw_entry,
				(*kp_pzf_op)->data_adc_cfg_1, 0xE000, (128*4));

	}
	I("%s, END\n", __func__);
}
static void hx83102d_firmware_read_0f(const struct firmware *fw_entry,
		int type)
{
	uint8_t tmp_data[4];

	I("%s, Entering\n", __func__);

	switch (type) {
	case 0:
		kp_g_core_fp->fp_read_sram_0f(fw_entry,
			(*kp_pzf_op)->data_sram_start_addr, 0, HX_40K_SZ);
		break;
	case 1:
		kp_g_core_fp->fp_read_sram_0f(fw_entry,
				(*kp_pzf_op)->data_cfg_info, 0xC000, 132);
		break;
	case 2:
		kp_g_core_fp->fp_read_sram_0f(fw_entry,
				(*kp_pzf_op)->data_fw_cfg_1, 0xC0FE, 484);
		break;
	case 3:
		kp_g_core_fp->fp_read_sram_0f(fw_entry,
				(*kp_pzf_op)->data_adc_cfg_1, 0xD000, 768);
		break;
	case 4:
		kp_g_core_fp->fp_read_sram_0f(fw_entry,
				(*kp_pzf_op)->data_adc_cfg_2, 0xD300, 1536);
		break;
	case 5:
		kp_g_core_fp->fp_read_sram_0f(fw_entry,
				(*kp_pzf_op)->data_adc_cfg_3, 0xE000, 1536);
		break;
	case 6:
		kp_himax_parse_assign_cmd(hx83102d_zf_data_bor_prevent_info,
			tmp_data, 4);
		kp_g_core_fp->fp_read_sram_0f(fw_entry, tmp_data, 0xC9E0, 32);
		break;
	case 7:
		kp_himax_parse_assign_cmd(hx83102d_zf_data_notch_info,
			tmp_data, 4);
		kp_g_core_fp->fp_read_sram_0f(fw_entry, tmp_data,
			0xCA00, 128);
		break;
	case 8:
		kp_himax_parse_assign_cmd(hx83102d_zf_func_info_en,
			tmp_data, 4);
		kp_g_core_fp->fp_read_sram_0f(fw_entry, tmp_data, 0xCB00, 12);
		break;
	case 9:
		kp_himax_parse_assign_cmd(hx83102d_zf_po_sub_func,
			tmp_data, 4);
		kp_g_core_fp->fp_read_sram_0f(fw_entry, tmp_data,
			0xA000, HX4K);
		break;
	default:
		break;
	}
	I("%s, END\n", __func__);
}
#endif

#if 0//defined(HX_CODE_OVERLAY)
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

	ret = request_firmware(&fwp, BOOT_UPGRADE_FWNAME,
		(*kp_private_ts)->dev);
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
	ovl_idx_t = *((*kp_ovl_idx) + ovl_type - 1);
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
 *		if (kp_g_core_fp->fp_write_sram_0f_crc(fwp, sram_addr,
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
		kp_g_core_fp->fp_register_read(handshaking_addr, DATA_LEN_4,
				recv_data, 0);
	} while (recv_data[0] != request && count++ < 10);

	if (count < 10) {
		/*g_core_fp.fp_write_sram_0f(fwp, sram_addr, offset, size);*/
		if (kp_g_core_fp->fp_write_sram_0f_crc(fwp, sram_addr,
		offset, size) != 0)
			E("%s, Overlay HW CRC FAIL\n", __func__);

		send_data[3] = 0x00;
		send_data[2] = 0x00;
		send_data[1] = 0x00;
		send_data[0] = reply;

		count2 = 0;
		do {
			kp_g_core_fp->fp_register_write(handshaking_addr,
					DATA_LEN_4,
					send_data, 0);
			usleep_range(1000, 1100);
			kp_g_core_fp->fp_register_read(handshaking_addr,
					DATA_LEN_4,
					recv_data, 0);
		} while (recv_data[0] != reply && count2++ < 10);

		if (ovl_type == 3) {
#if defined(HX_RST_PIN_FUNC)
			kp_g_core_fp->fp_ic_reset(false, false);
#else
			kp_g_core_fp->fp_system_reset();
#endif
		}
	}

	I("%s: overlay request %d times; reply %d times\n", __func__,
			count, count2);

	release_firmware(fwp);

	/* rescue mechanism */
	if (count >= 10) {
		kp_g_core_fp->fp_0f_op_file_dirly(BOOT_UPGRADE_FWNAME);
		kp_g_core_fp->fp_reload_disable(0);
		kp_g_core_fp->fp_sense_on(0x00);
		kp_himax_int_enable(1);
	}

	return 0;
}
#endif
#endif

static bool hx83102e_read_event_stack(uint8_t *buf, uint8_t length)
{
	struct timespec t_start, t_end, t_delta;
	int len = length;
	int i2c_speed = 0;

	if ((*kp_private_ts)->debug_log_level & BIT(2))
		getnstimeofday(&t_start);

	kp_himax_bus_read((*kp_pfw_op)->addr_event_addr[0], buf, length,
			HIMAX_I2C_RETRY_TIMES);

	if ((*kp_private_ts)->debug_log_level & BIT(2)) {
		getnstimeofday(&t_end);
		t_delta.tv_nsec = (t_end.tv_sec * 1000000000 + t_end.tv_nsec)
					- (t_start.tv_sec
					* 1000000000
					+ t_start.tv_nsec); /*ns*/

		i2c_speed = (len * 9 * 1000000
			/ (int)t_delta.tv_nsec) * 13 / 10;
		(*kp_private_ts)->bus_speed = (int)i2c_speed;
		}
	return 1;
}

static void himax_hx83102ab_reg_re_init(uint8_t ic_name)
{
	I("%s:Entering!\n", __func__);
	(*kp_private_ts)->ic_notouch_frame = hx83102ab_notouch_frame;
	kp_himax_parse_assign_cmd(hx83102ab_fw_addr_sorting_mode_en,
			(*kp_pfw_op)->addr_sorting_mode_en,
			sizeof((*kp_pfw_op)->addr_sorting_mode_en));
	kp_himax_parse_assign_cmd(hx83102ab_fw_addr_selftest_addr_en,
			(*kp_pfw_op)->addr_selftest_addr_en,
			sizeof((*kp_pfw_op)->addr_selftest_addr_en));
#if defined(HX_ZERO_FLASH)
	kp_himax_parse_assign_cmd(hx83102ab_data_adc_cfg_1,
			(*kp_pzf_op)->data_adc_cfg_1,
			sizeof((*kp_pzf_op)->data_adc_cfg_1));
#endif

	if (ic_name == 0x2a) {
		kp_himax_parse_assign_cmd(hx83102a_data_df_rx,
				(*kp_pdriver_op)->data_df_rx,
				sizeof((*kp_pdriver_op)->data_df_rx));
		kp_himax_parse_assign_cmd(hx83102a_data_df_tx,
				(*kp_pdriver_op)->data_df_tx,
				sizeof((*kp_pdriver_op)->data_df_tx));
	}
}

static void himax_hx83102ab_func_re_init(void)
{
	I("%s:Entering!\n", __func__);
	kp_g_core_fp->fp_chip_init = hx83102_chip_init;
	kp_g_core_fp->fp_sense_off = hx83102ab_sense_off;
	kp_g_core_fp->fp_set_SMWP_enable = hx83102ab_set_SMWP_enable;
	kp_g_core_fp->fp_set_HSEN_enable = hx83102ab_set_HSEN_enable;
	kp_g_core_fp->fp_usb_detect_set = hx83102ab_usb_detect_set;
	kp_g_core_fp->fp_read_DD_status = hx83102ab_read_DD_status;
	kp_g_core_fp->fp_power_on_init = hx83102ab_power_on_init;
	kp_g_core_fp->fp_fts_ctpm_fw_upgrade_with_sys_fs_64k =
			hx83102ab_fts_ctpm_fw_upgrade_with_sys_fs_64k;
#if defined(HX_EXCP_RECOVERY)
	kp_g_core_fp->fp_excp_ic_reset = hx83102ab_excp_ic_reset;
#endif
#if defined(HX_ZERO_FLASH)
	kp_g_core_fp->fp_firmware_update_0f = hx83102ab_firmware_update_0f;
#if defined(HX_0F_DEBUG)
	kp_g_core_fp->fp_firmware_read_0f = hx83102ab_firmware_read_0f;
#endif
#endif

}

static void himax_hx83102d_reg_re_init(void)
{
	I("%s:Entering!\n", __func__);
	(*kp_private_ts)->ic_notouch_frame = hx83102d_notouch_frame;
	kp_himax_parse_assign_cmd(hx83102d_fw_addr_raw_out_sel,
			(*kp_pfw_op)->addr_raw_out_sel,
			sizeof((*kp_pfw_op)->addr_raw_out_sel));
#if defined(HX_ZERO_FLASH)
	kp_himax_parse_assign_cmd(hx83102d_zf_data_sram_start_addr,
			(*kp_pzf_op)->data_sram_start_addr,
			sizeof((*kp_pzf_op)->data_sram_start_addr));
	kp_himax_parse_assign_cmd(hx83102d_zf_data_adc_cfg_1,
			(*kp_pzf_op)->data_adc_cfg_1,
			sizeof((*kp_pzf_op)->data_adc_cfg_1));
	kp_himax_parse_assign_cmd(hx83102d_zf_data_adc_cfg_2,
			(*kp_pzf_op)->data_adc_cfg_2,
			sizeof((*kp_pzf_op)->data_adc_cfg_2));
	kp_himax_parse_assign_cmd(hx83102d_zf_data_adc_cfg_3,
			(*kp_pzf_op)->data_adc_cfg_3,
			sizeof((*kp_pzf_op)->data_adc_cfg_3));
#endif
	kp_himax_parse_assign_cmd(hx83102d_adr_osc_en,
			(*kp_pic_op)->adr_osc_en,
			sizeof((*kp_pic_op)->adr_osc_en));
	kp_himax_parse_assign_cmd(hx83102d_adr_osc_pw,
			(*kp_pic_op)->adr_osc_pw,
			sizeof((*kp_pic_op)->adr_osc_pw));
}

static void himax_hx83102d_func_re_init(void)
{
	I("%s:Entering!\n", __func__);
	kp_g_core_fp->fp_chip_init = hx83102_chip_init;
	kp_g_core_fp->fp_sense_off = hx83102d_sense_off;
	kp_g_core_fp->fp_ic_id_read = hx83102d_ic_id_read;
	kp_g_core_fp->fp_dd_clk_set = hx83102d_dd_clk_set;
	kp_g_core_fp->fp_dd_reg_en = hx83102d_dd_reg_en;
#if defined(HX_ZERO_FLASH)
	/*kp_g_core_fp->fp_firmware_update_0f = hx83102d_firmware_update_0f;*/
	kp_g_core_fp->fp_resume_ic_action = himax_hx83102d_resume_ic_action;
	kp_g_core_fp->fp_sense_on = himax_hx83102d_sense_on;
	kp_g_core_fp->fp_0f_reload_to_active = himax_hx83102d_reload_to_active;
#if defined(HX_0F_DEBUG)
	kp_g_core_fp->fp_firmware_read_0f = hx83102d_firmware_read_0f;
#endif
#if 0//defined(HX_CODE_OVERLAY)
	kp_g_core_fp->fp_0f_overlay = hx83102d_0f_overlay;
#endif
#if defined(HX_EXCP_RECOVERY)
	kp_g_core_fp->fp_0f_excp_check = hx83102d_0f_excp_check;
#endif
#endif
}

static void himax_hx83102e_reg_re_init(void)
{
	I("%s:Entering!\n", __func__);
	(*kp_private_ts)->ic_notouch_frame = hx83102e_notouch_frame;
	kp_himax_parse_assign_cmd(hx83102e_fw_addr_raw_out_sel,
			(*kp_pfw_op)->addr_raw_out_sel,
			sizeof((*kp_pfw_op)->addr_raw_out_sel));
	kp_himax_parse_assign_cmd(hx83102e_data_df_rx,
			(*kp_pdriver_op)->data_df_rx,
			sizeof((*kp_pdriver_op)->data_df_rx));
	kp_himax_parse_assign_cmd(hx83102e_data_df_tx,
			(*kp_pdriver_op)->data_df_tx,
			sizeof((*kp_pdriver_op)->data_df_tx));
	kp_himax_parse_assign_cmd(hx83102e_ic_adr_tcon_rst,
			(*kp_pic_op)->addr_tcon_on_rst,
			sizeof((*kp_pic_op)->addr_tcon_on_rst));
}

static void himax_hx83102e_func_re_init(void)
{
	I("%s:Entering!\n", __func__);

	kp_g_core_fp->fp_chip_init = hx83102e_chip_init;
	kp_g_core_fp->fp_sense_on = hx83102e_sense_on;
	kp_g_core_fp->fp_sense_off = hx83102e_sense_off;
	kp_g_core_fp->fp_read_event_stack = hx83102e_read_event_stack;

	kp_g_core_fp->fp_resume_ic_action = hx83102e_resume_ic_action;
	kp_g_core_fp->fp_0f_reload_to_active = hx83102e_reload_to_active;

#if 0//defined(HX_CODE_OVERLAY)
	kp_g_core_fp->fp_0f_overlay = hx83102e_0f_overlay;
#endif
}


static bool hx83102_chip_detect(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];
	bool ret_data = false;
	int ret = 0;
	int i = 0;

	if (himax_ic_setup_external_symbols())
		return false;

#if defined(HX_RST_PIN_FUNC)
	hx83102_pin_reset();
#endif

	ret = kp_himax_bus_read(0x13, tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
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
		ret = hx83102_register_read(tmp_addr, DATA_LEN_4, tmp_data);
		if (ret != 0) {
			ret_data = false;
			E("%s:hx83102_register_read Fail:\n", __func__);
			return ret_data;
		}
		I("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3],
			tmp_data[2], tmp_data[1]); /*83,10,2X*/

		if ((tmp_data[3] == 0x83)
		&& (tmp_data[2] == 0x10)
		&& ((tmp_data[1] == 0x2a)
		|| (tmp_data[1] == 0x2b)
		|| (tmp_data[1] == 0x2d)
		|| (tmp_data[1] == 0x2e))) {
			if (tmp_data[1] == 0x2a) {
				strlcpy((*kp_private_ts)->chip_name,
					HX_83102A_SERIES_PWON, 30);
				(*kp_ic_data)->ic_adc_num =
					hx83102a_data_adc_num;
				I("%s:detect IC HX83102A successfully\n",
					__func__);
			} else if (tmp_data[1] == 0x2b) {
				strlcpy((*kp_private_ts)->chip_name,
					HX_83102B_SERIES_PWON, 30);
				(*kp_ic_data)->ic_adc_num =
					hx83102b_data_adc_num;
				I("%s:detect IC HX83102B successfully\n",
					__func__);
			} else if (tmp_data[1] == 0x2d) {
				strlcpy((*kp_private_ts)->chip_name,
					HX_83102D_SERIES_PWON, 30);
				(*kp_ic_data)->ic_adc_num =
					hx83102d_data_adc_num;
				I("%s:detect IC HX83102D successfully\n",
					__func__);
			} else {
				strlcpy((*kp_private_ts)->chip_name,
					HX_83102E_SERIES_PWON, 30);
				(*kp_ic_data)->ic_adc_num =
					hx83102e_data_adc_num;
				I("%s:detect IC HX83102E successfully\n",
					__func__);
			}

			ret = kp_himax_mcu_in_cmd_struct_init();
			if (ret < 0) {
				ret_data = false;
				E("%s:cmd_struct_init Fail:\n", __func__);
				return ret_data;
			}

			kp_himax_mcu_in_cmd_init();
			if ((tmp_data[1] == 0x2a) || (tmp_data[1] == 0x2b)) {
				himax_hx83102ab_reg_re_init(tmp_data[1]);
				himax_hx83102ab_func_re_init();
			} else  if (tmp_data[1] == 0x2d) {
				himax_hx83102d_reg_re_init();
				himax_hx83102d_func_re_init();
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

DECLARE(HX_MOD_KSYM_HX83102);

static int himax_hx83102_probe(void)
{
	I("%s:Enter\n", __func__);
	himax_add_chip_dt(hx83102_chip_detect);

	return 0;
}

static int himax_hx83102_remove(void)
{
	free_chip_dt_table();
	return 0;
}

static int __init himax_hx83102_init(void)
{
	int ret = 0;

	I("%s\n", __func__);
	ret = himax_hx83102_probe();
	return 0;
}

static void __exit himax_hx83102_exit(void)
{
	himax_hx83102_remove();
}

module_init(himax_hx83102_init);
module_exit(himax_hx83102_exit);

MODULE_DESCRIPTION("HIMAX HX83102 touch driver");
MODULE_LICENSE("GPL");


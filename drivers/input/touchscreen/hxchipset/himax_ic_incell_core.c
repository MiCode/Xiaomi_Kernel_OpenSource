/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for incell ic core functions
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

#include "himax_ic_core.h"

struct himax_core_command_operation *g_core_cmd_op;
struct ic_operation *pic_op;
EXPORT_SYMBOL(pic_op);

struct fw_operation *pfw_op;
EXPORT_SYMBOL(pfw_op);

struct flash_operation *pflash_op;
EXPORT_SYMBOL(pflash_op);

struct sram_operation *psram_op;
struct driver_operation *pdriver_op;
EXPORT_SYMBOL(pdriver_op);

#if defined(HX_ZERO_FLASH)
struct zf_operation *pzf_op;
EXPORT_SYMBOL(pzf_op);
#if defined(HX_CODE_OVERLAY)
uint8_t *ovl_idx;
EXPORT_SYMBOL(ovl_idx);
#endif
#endif

#define Arr4_to_Arr4(A, B) {\
	A[3] = B[3];\
	A[2] = B[2];\
	A[1] = B[1];\
	A[0] = B[0];\
	}

int HX_TOUCH_INFO_POINT_CNT;

void (*himax_mcu_cmd_struct_free)(void);
static uint8_t *g_internal_buffer;
uint32_t dbg_reg_ary[4] = {fw_addr_fw_dbg_msg_addr, fw_addr_chk_fw_status,
	fw_addr_chk_dd_status, fw_addr_flag_reset_event};

/* CORE_IC */
/* IC side start*/
static void himax_mcu_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[DATA_LEN_4];
	int ret;

	/*I("%s,Entering\n", __func__);*/
	tmp_data[0] = pic_op->data_conti[0];

	ret = himax_bus_write(pic_op->addr_conti[0], NULL, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (pic_op->data_incr4[0] | auto_add_4_byte);

	ret = himax_bus_write(pic_op->addr_incr4[0], NULL, tmp_data, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return;
	}
}

static int himax_mcu_register_read(uint8_t *addr, uint8_t *buf, uint32_t len)
{
	int ret = -1;

	mutex_lock(&private_ts->reg_lock);

	if (addr[0] == pflash_op->addr_spi200_data[0]
	&& addr[1] == pflash_op->addr_spi200_data[1]
	&& addr[2] == pflash_op->addr_spi200_data[2]
	&& addr[3] == pflash_op->addr_spi200_data[3])
		g_core_fp.fp_burst_enable(0);
	else if (len > DATA_LEN_4)
		g_core_fp.fp_burst_enable(1);
	else
		g_core_fp.fp_burst_enable(0);

	ret = himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], addr, NULL, 4);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		mutex_unlock(&private_ts->reg_lock);
		return BUS_FAIL;
	}

	ret = himax_bus_write(pic_op->addr_ahb_access_direction[0], NULL,
		&pic_op->data_ahb_access_direction_read[0], 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		mutex_unlock(&private_ts->reg_lock);
		return BUS_FAIL;
	}

	ret = himax_bus_read(pic_op->addr_ahb_rdata_byte_0[0], buf, len);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		mutex_unlock(&private_ts->reg_lock);
		return BUS_FAIL;
	}

	mutex_unlock(&private_ts->reg_lock);

	return NO_ERR;
}

static int himax_mcu_register_write(uint8_t *addr, uint8_t *val, uint32_t len)
{
	int ret = -1;

	mutex_lock(&private_ts->reg_lock);

	if (addr[0] == pflash_op->addr_spi200_data[0]
	&& addr[1] == pflash_op->addr_spi200_data[1]
	&& addr[2] == pflash_op->addr_spi200_data[2]
	&& addr[3] == pflash_op->addr_spi200_data[3])
		g_core_fp.fp_burst_enable(0);
	else if (len > DATA_LEN_4)
		g_core_fp.fp_burst_enable(1);
	else
		g_core_fp.fp_burst_enable(0);

	ret = himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], addr, val,
		len+ADDR_LEN_4);
	if (ret < 0) {
		E("%s: xfer fail!\n", __func__);
		mutex_unlock(&private_ts->reg_lock);
		return BUS_FAIL;
	}

	mutex_unlock(&private_ts->reg_lock);

	return NO_ERR;
}

static int himax_write_read_reg(uint8_t *tmp_addr, uint8_t *tmp_data,
		uint8_t hb, uint8_t lb)
{
	uint16_t retry = 0;
	uint8_t r_data[ADDR_LEN_4] = {0};

	while (retry++ < 40) { /* ceil[16.6*2] */
		g_core_fp.fp_register_read(tmp_addr, r_data, DATA_LEN_4);
		if (r_data[1] == lb && r_data[0] == hb)
			break;
		else if (r_data[1] == hb && r_data[0] == lb)
			return NO_ERR;

		g_core_fp.fp_register_write(tmp_addr, tmp_data, DATA_LEN_4);
		usleep_range(1000, 1100);
	}

	if (retry >= 40)
		goto FAIL;

	retry = 0;
	while (retry++ < 200) { /* self test item might take long time */
		g_core_fp.fp_register_read(tmp_addr, r_data, DATA_LEN_4);
		if (r_data[1] == hb && r_data[0] == lb)
			return NO_ERR;

		I("%s: wait data ready %d times\n", __func__, retry);
		usleep_range(10000, 10100);
	}

FAIL:
	E("%s: failed to handshaking with DSRAM\n", __func__);
	E("%s: addr = %02X%02X%02X%02X; data = %02X%02X%02X%02X",
		__func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
	E("%s: target = %02X%02X; r_data = %02X%02X\n",
		__func__, hb, lb, r_data[1], r_data[0]);

	return HX_RW_REG_FAIL;

}

static void himax_mcu_interface_on(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_data2[DATA_LEN_4];
	int cnt = 0;
	int ret = 0;

	/* Read a dummy register to wake up BUS.*/
	ret = himax_bus_read(pic_op->addr_ahb_rdata_byte_0[0], tmp_data,
		DATA_LEN_4);
	if (ret < 0) {/* to knock BUS*/
		E("%s: bus access fail!\n", __func__);
		return;
	}

	do {
		tmp_data[0] = pic_op->data_conti[0];

		ret = himax_bus_write(pic_op->addr_conti[0], NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return;
		}

		tmp_data[0] = pic_op->data_incr4[0];

		ret = himax_bus_write(pic_op->addr_incr4[0], NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return;
		}

		/*Check cmd*/
		himax_bus_read(pic_op->addr_conti[0], tmp_data, 1);
		himax_bus_read(pic_op->addr_incr4[0], tmp_data2, 1);

		if (tmp_data[0] == pic_op->data_conti[0]
		&& tmp_data2[0] == pic_op->data_incr4[0])
			break;

		usleep_range(1000, 1100);
	} while (++cnt < 10);

	if (cnt > 0)
		I("%s:Polling burst mode: %d times\n", __func__, cnt);
}

#define WIP_PRT_LOG "%s: retry:%d, bf[0]=%d, bf[1]=%d,bf[2]=%d, bf[3]=%d\n"
static bool himax_mcu_wait_wip(int Timing)
{
	uint8_t tmp_data[DATA_LEN_4];
	int retry_cnt = 0;

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
		pflash_op->data_spi200_trans_fmt, DATA_LEN_4);
	tmp_data[0] = 0x01;

	do {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			pflash_op->data_spi200_trans_ctrl_1, DATA_LEN_4);

		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			pflash_op->data_spi200_cmd_1, DATA_LEN_4);
		tmp_data[0] = tmp_data[1] = tmp_data[2] = tmp_data[3] = 0xFF;
		g_core_fp.fp_register_read(pflash_op->addr_spi200_data,
			tmp_data, 4);

		if ((tmp_data[0] & 0x01) == 0x00)
			return true;

		retry_cnt++;

		if (tmp_data[0] != 0x00
		|| tmp_data[1] != 0x00
		|| tmp_data[2] != 0x00
		|| tmp_data[3] != 0x00)
			I(WIP_PRT_LOG,
			__func__, retry_cnt, tmp_data[0],
			tmp_data[1], tmp_data[2], tmp_data[3]);

		if (retry_cnt > 100) {
			E("%s: Wait wip error!\n", __func__);
			return false;
		}

		msleep(Timing);
	} while ((tmp_data[0] & 0x01) == 0x01);

	return true;
}

static void himax_mcu_sense_on(uint8_t FlashMode)
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
	usleep_range(10000, 11000);
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
			I("%s:OK and Read status from IC = %X,%X\n",
					__func__, tmp_data[0], tmp_data[1]);
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
}

static bool himax_mcu_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	do {
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], NULL, tmp_data,
			1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0], NULL, tmp_data,
			1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		g_core_fp.fp_register_read(pic_op->addr_cs_central_state,
			tmp_data, ADDR_LEN_4);
		I("%s: Check enter_save_mode data[0]=%X\n", __func__,
			tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst,
				pic_op->data_rst, DATA_LEN_4);
			usleep_range(1000, 1100);

			g_core_fp.fp_register_write(pic_op->addr_adc_on_rst,
				pic_op->data_rst, DATA_LEN_4);
			usleep_range(1000, 1100);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_register_write(pic_op->addr_adc_on_rst,
				tmp_data, DATA_LEN_4);
			goto TRUE_END;
		} else {
			/*msleep(10);*/
#if defined(HX_RST_PIN_FUNC)
			g_core_fp.fp_ic_reset(false, false);
#else
			g_core_fp.fp_system_reset();
#endif
		}
	} while (cnt++ < 15);

	return false;
TRUE_END:
	return true;
}

/*power saving level*/
static void himax_mcu_init_psl(void)
{
	g_core_fp.fp_register_write(pic_op->addr_psl, pic_op->data_rst,
		sizeof(pic_op->data_rst));
	I("%s: power saving level reset OK!\n", __func__);
}

static void himax_mcu_resume_ic_action(void)
{
	/* Nothing to do */
}

static void himax_mcu_suspend_ic_action(void)
{
	/* Nothing to do */
}

static void himax_mcu_power_on_init(void)
{
	uint8_t tmp_data[4] = {0x01, 0x00, 0x00, 0x00};
	uint8_t retry = 0;

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);
	/*N frame initial*/
	/* reset N frame back to default value 1 for normal mode */
	g_core_fp.fp_register_write(pfw_op->addr_set_frame_addr, tmp_data, 4);
	/*FW reload done initial*/
	g_core_fp.fp_register_write(pdriver_op->addr_fw_define_2nd_flash_reload,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));

	g_core_fp.fp_sense_on(0x00);

	I("%s: waiting for FW reload data\n", __func__);

	while (retry++ < 60) {
		g_core_fp.fp_register_read(
			pdriver_op->addr_fw_define_2nd_flash_reload, tmp_data,
			DATA_LEN_4);

		/* use all 4 bytes to compare */
		if ((tmp_data[3] == 0x00 && tmp_data[2] == 0x00 &&
			tmp_data[1] == 0x72 && tmp_data[0] == 0xC0)) {
			I("%s: FW reload done\n", __func__);
			break;
		}
		I("%s: wait FW reload %d times\n", __func__, retry);
		g_core_fp.fp_read_FW_status();
		usleep_range(5000, 5100);
	}
}
/* IC side end*/
/* CORE_IC */

/* CORE_FW */
/* FW side start*/
static void diag_mcu_parse_raw_data(struct himax_report_data *hx_touch_data,
		int mul_num, int self_num, uint8_t diag_cmd,
		int32_t *mutual_data, int32_t *self_data)
{
	int RawDataLen_word;
	int index = 0;
	int temp1, temp2, i;

	if (hx_touch_data->hx_rawdata_buf[0]
	== pfw_op->data_rawdata_ready_lb[0]
	&& hx_touch_data->hx_rawdata_buf[1]
	== pfw_op->data_rawdata_ready_hb[0]
	&& hx_touch_data->hx_rawdata_buf[2] > 0
	&& hx_touch_data->hx_rawdata_buf[3] == diag_cmd) {
		RawDataLen_word = hx_touch_data->rawdata_size / 2;
		index = (hx_touch_data->hx_rawdata_buf[2] - 1)
			* RawDataLen_word;

		/* I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n",index,
		 *	buf[56], buf[57], buf[58], buf[59], mul_num, self_num);
		 * I("RawDataLen=%d , RawDataLen_word=%d ,
		 *	hx_touch_info_size=%d\n",
		 *	RawDataLen, RawDataLen_word, hx_touch_info_size);
		 */
		for (i = 0; i < RawDataLen_word; i++) {
			temp1 = index + i;

			if (temp1 < mul_num) { /*mutual*/
				mutual_data[index + i] =
					((int8_t)hx_touch_data->
					hx_rawdata_buf[i * 2 + 4 + 1]) * 256
					+ hx_touch_data->
					hx_rawdata_buf[i * 2 + 4];
			} else { /*self*/
				temp1 = i + index;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2)
					break;

				self_data[i + index - mul_num] =
					(((int8_t)hx_touch_data->
					hx_rawdata_buf[i * 2 + 4 + 1]) << 8)
					+ hx_touch_data->
					hx_rawdata_buf[i * 2 + 4];
			}
		}
	}
}

static void himax_mcu_system_reset(void)
{
#if defined(HX_PON_PIN_SUPPORT)
	g_core_fp.fp_register_write(pfw_op->addr_system_reset,
		pfw_op->data_system_reset, sizeof(pfw_op->data_system_reset));
#else
	int ret = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int retry = 0;

	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	do {
		/* reset code*/
		/**
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], NULL, tmp_data,
			1);
		if (ret < 0)
			E("%s: bus access fail!\n", __func__);

		/**
		 * I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0], NULL, tmp_data,
			1);
		if (ret < 0)
			E("%s: bus access fail!\n", __func__);

		/**
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x00
		 */
		tmp_data[0] = 0x00;

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], NULL, tmp_data,
			1);
		if (ret < 0)
			E("%s: bus access fail!\n", __func__);

		usleep_range(10000, 11000);

		g_core_fp.fp_register_read(pfw_op->addr_flag_reset_event,
			tmp_data, DATA_LEN_4);
		I("%s:Read status from IC = %X,%X\n", __func__,
				tmp_data[0], tmp_data[1]);
	} while ((tmp_data[1] != 0x02 || tmp_data[0] != 0x00) && retry++ < 5);
#endif
}

static int himax_mcu_Calculate_CRC_with_AP(unsigned char *FW_content,
		int CRC_from_FW, int len)
{
	int i, j, length = 0;
	int fw_data;
	int fw_data_2;
	int CRC = 0xFFFFFFFF;
	int PolyNomial = 0x82F63B78;

	length = len / 4;

	for (i = 0; i < length; i++) {
		fw_data = FW_content[i * 4];

		for (j = 1; j < 4; j++) {
			fw_data_2 = FW_content[i * 4 + j];
			fw_data += (fw_data_2) << (8 * j);
		}
		CRC = fw_data ^ CRC;
		for (j = 0; j < 32; j++) {
			if ((CRC % 2) != 0)
				CRC = ((CRC >> 1) & 0x7FFFFFFF) ^ PolyNomial;
			else
				CRC = (((CRC >> 1) & 0x7FFFFFFF));
		}
	}

	return CRC;
}

static uint32_t himax_mcu_check_CRC(uint8_t *start_addr, int reload_length)
{
	uint32_t result = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int cnt = 0, ret = 0;
	int length = reload_length / DATA_LEN_4;

	ret = g_core_fp.fp_register_write(pfw_op->addr_reload_addr_from,
		start_addr, DATA_LEN_4);
	if (ret < NO_ERR) {
		E("%s: bus access fail!\n", __func__);
		return HW_CRC_FAIL;
	}

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x99;
	tmp_data[1] = (length >> 8);
	tmp_data[0] = length;
	ret = g_core_fp.fp_register_write(pfw_op->addr_reload_addr_cmd_beat,
		tmp_data, DATA_LEN_4);
	if (ret < NO_ERR) {
		E("%s: bus access fail!\n", __func__);
		return HW_CRC_FAIL;
	}
	cnt = 0;

	do {
		ret = g_core_fp.fp_register_read(pfw_op->addr_reload_status,
			tmp_data, DATA_LEN_4);
		if (ret < NO_ERR) {
			E("%s: bus access fail!\n", __func__);
			return HW_CRC_FAIL;
		}

		if ((tmp_data[0] & 0x01) != 0x01) {
			ret = g_core_fp.fp_register_read(
				pfw_op->addr_reload_crc32_result, tmp_data,
				DATA_LEN_4);
			if (ret < NO_ERR) {
				E("%s: bus access fail!\n", __func__);
				return HW_CRC_FAIL;
			}
			I("%s:data[3]=%X,data[2]=%X,data[1]=%X,data[0]=%X\n",
				__func__,
				tmp_data[3],
				tmp_data[2],
				tmp_data[1],
				tmp_data[0]);
			result = ((tmp_data[3] << 24)
					+ (tmp_data[2] << 16)
					+ (tmp_data[1] << 8)
					+ tmp_data[0]);
			goto END;
		} else {
			I("Waiting for HW ready!\n");
			usleep_range(1000, 1100);
			if (cnt >= 100)
				g_core_fp.fp_read_FW_status();
		}

	} while (cnt++ < 100);
END:
	return result;
}

static void himax_mcu_set_reload_cmd(uint8_t *write_data, int idx,
		uint32_t cmd_from, uint32_t cmd_to, uint32_t cmd_beat)
{
	int index = idx * 12;
	int i;

	for (i = 3; i >= 0; i--) {
		write_data[index + i] = (cmd_from >> (8 * i));
		write_data[index + 4 + i] = (cmd_to >> (8 * i));
		write_data[index + 8 + i] = (cmd_beat >> (8 * i));
	}
}

static bool himax_mcu_program_reload(void)
{
	return true;
}

#if defined(HX_ULTRA_LOW_POWER)
static int himax_mcu_ulpm_in(void)
{
	uint8_t tmp_data[4];
	int rtimes = 0;
	int ret = 0;

	I("%s:entering\n", __func__);

	/* 34 -> 11 */
	do {
		if (rtimes > 10) {
			I("%s:1/7 retry over 10 times!\n", __func__);
			return false;
		}
		ret = himax_bus_write(pfw_op->addr_ulpm_34[0], NULL,
			pfw_op->data_ulpm_11, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_34[0], tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x34,correct 0x11=current 0x%2.2X\n",
				__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_11[0]);

	rtimes = 0;
	/* 34 -> 11 */
	do {
		if (rtimes > 10) {
			I("%s:2/7 retry over 10 times!\n", __func__);
			return false;
		}
		ret = himax_bus_write(pfw_op->addr_ulpm_34[0], NULL,
			pfw_op->data_ulpm_11, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_34[0], tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x34,correct 0x11=current 0x%2.2X\n",
				__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_11[0]);

	/* 33 -> 33 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:3/7 retry over 10 times!\n", __func__);
			return false;
		}
		ret = himax_bus_write(pfw_op->addr_ulpm_33[0], NULL,
			pfw_op->data_ulpm_33, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x33,correct 0x33=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_33[0]);

	/* 34 -> 22 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:4/7 retry over 10 times!\n", __func__);
			return false;
		}
		ret = himax_bus_write(pfw_op->addr_ulpm_34[0], NULL,
			pfw_op->data_ulpm_22, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_34[0], tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x34,correct 0x22=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_22[0]);

	/* 33 -> AA */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:5/7 retry over 10 times!\n", __func__);
			return false;
		}
		ret = himax_bus_write(pfw_op->addr_ulpm_33[0], NULL,
			pfw_op->data_ulpm_aa, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x33, correct 0xAA=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_aa[0]);

	/* 33 -> 33 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:6/7 retry over 10 times!\n", __func__);
			return false;
		}
		ret = himax_bus_write(pfw_op->addr_ulpm_33[0], NULL,
			pfw_op->data_ulpm_33, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x33,correct 0x33=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_33[0]);

	/* 33 -> AA */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:7/7 retry over 10 times!\n", __func__);
			return false;
		}
		ret = himax_bus_write(pfw_op->addr_ulpm_33[0], NULL,
			pfw_op->data_ulpm_aa, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x33,correct 0xAA=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != pfw_op->data_ulpm_aa[0]);

	I("%s:END\n", __func__);
	return true;
}

static int himax_mcu_black_gest_ctrl(bool enable)
{
	int ret = 0;

	I("%s:enable=%d, ts->is_suspended=%d\n", __func__,
			enable, private_ts->suspended);

	if (private_ts->suspended) {
		if (enable) {
#if defined(HX_RST_PIN_FUNC)
			g_core_fp.fp_ic_reset(false, false);
#else
			I("%s: Please enable TP reset define\n", __func__);
#endif
		} else {
			g_core_fp.fp_ulpm_in();
		}
	} else {
		g_core_fp.fp_sense_on(0);
	}
	return ret;
}
#endif

static void himax_mcu_set_SMWP_enable(uint8_t SMWP_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (SMWP_enable) {
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_smwp_enable,
				tmp_data, DATA_LEN_4);
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				back_data, 4);
		} else {
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				tmp_data,
				4);
			g_core_fp.fp_register_write(pfw_op->addr_smwp_enable,
				tmp_data, DATA_LEN_4);
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				back_data,
				4);
		}

		g_core_fp.fp_register_read(pfw_op->addr_smwp_enable, tmp_data,
			DATA_LEN_4);
		/*I("%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0],SMWP_enable,retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3]
		|| tmp_data[2] != back_data[2]
		|| tmp_data[1] != back_data[1]
		|| tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_set_HSEN_enable(uint8_t HSEN_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (HSEN_enable) {
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_hsen_enable,
				tmp_data, DATA_LEN_4);
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				back_data, 4);
		} else {
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				tmp_data,
				4);

			g_core_fp.fp_register_write(pfw_op->addr_hsen_enable,
				tmp_data, DATA_LEN_4);

			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				back_data,
				4);
		}

		g_core_fp.fp_register_read(pfw_op->addr_hsen_enable, tmp_data,
			DATA_LEN_4);
		/*I("%s: tmp_data[0]=%d, HSEN_enable=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0],HSEN_enable,retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3]
		|| tmp_data[2] != back_data[2]
		|| tmp_data[1] != back_data[1]
		|| tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_usb_detect_set(uint8_t *cable_config)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (cable_config[1] == 0x01) {
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				tmp_data, 4);
			g_core_fp.fp_register_write(pfw_op->addr_usb_detect,
				tmp_data, DATA_LEN_4);
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				back_data, 4);
			I("%s: USB detect status IN!\n", __func__);
		} else {
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				tmp_data,
				4);
			g_core_fp.fp_register_write(pfw_op->addr_usb_detect,
				tmp_data, DATA_LEN_4);
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				back_data,
				4);
			I("%s: USB detect status OUT!\n", __func__);
		}

		g_core_fp.fp_register_read(pfw_op->addr_usb_detect, tmp_data,
			DATA_LEN_4);
		/*I("%s: tmp_data[0]=%d, USB detect=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0],cable_config[1] ,retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3]
		|| tmp_data[2] != back_data[2]
		|| tmp_data[1] != back_data[1]
		|| tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

#define PRT_DATA "%s:[3]=0x%2X, [2]=0x%2X, [1]=0x%2X, [0]=0x%2X\n"
static void himax_mcu_diag_register_set(uint8_t diag_command,
		uint8_t storage_type, bool is_dirly)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t cnt = 50;

	if (diag_command > 0 && storage_type % 8 > 0 && !is_dirly)
		tmp_data[0] = diag_command + 0x08;
	else
		tmp_data[0] = diag_command;
	I("diag_command = %d, tmp_data[0] = %X\n", diag_command, tmp_data[0]);
	g_core_fp.fp_interface_on();
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00;
	do {
		g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
			tmp_data, DATA_LEN_4);
		g_core_fp.fp_register_read(pfw_op->addr_raw_out_sel, back_data,
			DATA_LEN_4);
		I(PRT_DATA,	__func__,
			back_data[3],
			back_data[2],
			back_data[1],
			back_data[0]);
		cnt--;
	} while (tmp_data[0] != back_data[0] && cnt > 0);
}

static int himax_mcu_chip_self_test(struct seq_file *s, void *v)
{
	uint8_t tmp_data[FLASH_WRITE_BURST_SZ];
	uint8_t self_test_info[20];
	int pf_value = 0x00;
	uint8_t test_result_id = 0;
	int i;

	memset(tmp_data, 0x00, sizeof(tmp_data));
	g_core_fp.fp_interface_on();
	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_register_write(pfw_op->addr_selftest_addr_en,
		pfw_op->data_selftest_request, DATA_LEN_4);
	/*Set criteria 0x10007F1C [0,1]=aa/up,down=, [2-3]=key/up,down,
	 *		[4-5]=avg/up,down
	 */
	tmp_data[0] = pfw_op->data_criteria_aa_top[0];
	tmp_data[1] = pfw_op->data_criteria_aa_bot[0];
	tmp_data[2] = pfw_op->data_criteria_key_top[0];
	tmp_data[3] = pfw_op->data_criteria_key_bot[0];
	tmp_data[4] = pfw_op->data_criteria_avg_top[0];
	tmp_data[5] = pfw_op->data_criteria_avg_bot[0];
	tmp_data[6] = 0x00;
	tmp_data[7] = 0x00;
	g_core_fp.fp_register_write(pfw_op->addr_criteria_addr,
		tmp_data, FLASH_WRITE_BURST_SZ);
	g_core_fp.fp_register_write(pfw_op->addr_set_frame_addr,
		pfw_op->data_set_frame, DATA_LEN_4);
	/*Disable IDLE Mode*/
	g_core_fp.fp_idle_mode(1);
	/*Diable Flash Reload*/
	g_core_fp.fp_reload_disable(1);
	/*start selftest // leave safe mode*/
	g_core_fp.fp_sense_on(0x01);

	/*Hand shaking*/
	for (i = 0; i < 1000; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_selftest_addr_en,
			tmp_data, 4);
		I("%s:data0=0x%2X,data1=0x%2X,data2=0x%2X,data3=0x%2X cnt=%d\n",
			__func__,
			tmp_data[0],
			tmp_data[1],
			tmp_data[2],
			tmp_data[3],
			i);
		usleep_range(10000, 11000);

		if (tmp_data[1] == pfw_op->data_selftest_ack_hb[0]
		&& tmp_data[0] == pfw_op->data_selftest_ack_lb[0]) {
			I("%s Data ready goto moving data\n", __func__);
			break;
		}
	}

	g_core_fp.fp_sense_off(true);
	msleep(20);
	/**
	 * Read test result ==> bit[2][1][0] = [key][AA][avg] => 0xF = PASS
	 */
	g_core_fp.fp_register_read(pfw_op->addr_selftest_result_addr,
		self_test_info, 20);
	test_result_id = self_test_info[0];
	I("%s: check test result, test_result_id=%x, test_result=%x\n",
		__func__, test_result_id, self_test_info[0]);
	I("raw top 1 = %d\n", self_test_info[3] * 256 + self_test_info[2]);
	I("raw top 2 = %d\n", self_test_info[5] * 256 + self_test_info[4]);
	I("raw top 3 = %d\n", self_test_info[7] * 256 + self_test_info[6]);
	I("raw last 1 = %d\n", self_test_info[9] * 256 + self_test_info[8]);
	I("raw last 2 = %d\n", self_test_info[11] * 256 + self_test_info[10]);
	I("raw last 3 = %d\n", self_test_info[13] * 256 + self_test_info[12]);
	I("raw key 1 = %d\n", self_test_info[15] * 256 + self_test_info[14]);
	I("raw key 2 = %d\n", self_test_info[17] * 256 + self_test_info[16]);
	I("raw key 3 = %d\n", self_test_info[19] * 256 + self_test_info[18]);

	if (test_result_id == pfw_op->data_selftest_pass[0]) {
		I("[Himax]: self-test pass\n");
		seq_puts(s, "Self_Test Pass:\n");
		pf_value = 0x0;
	} else {
		E("[Himax]: self-test fail\n");
		seq_puts(s, "Self_Test Fail:\n");
		/*  E("[Himax]: bank_avg = %d, bank_max = %d,%d,%d, bank_min =
		 *	%d,%d,%d, key = %d,%d,%d\n",
		 *	tmp_data[1], tmp_data[2],
		 *	tmp_data[3], tmp_data[4],
		 *	tmp_data[5], tmp_data[6],
		 *	tmp_data[7], tmp_data[8],
		 *	tmp_data[9], tmp_data[10]);
		 */
		pf_value = 0x1;
	}

	/*Enable IDLE Mode*/
	g_core_fp.fp_idle_mode(0);
#if !defined(HX_ZERO_FLASH)
	/* Enable Flash Reload //recovery*/
	g_core_fp.fp_reload_disable(0);
#endif
	g_core_fp.fp_sense_on(0x00);
	msleep(120);

	return pf_value;
}

#define PRT_TMP_DATA "%s:[0]=0x%2X,[1]=0x%2X,	[2]=0x%2X,[3]=0x%2X\n"
static void himax_mcu_idle_mode(int disable)
{
	int retry = 20;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t switch_cmd = 0x00;

	I("%s:entering\n", __func__);

	do {
		I("%s,now %d times!\n", __func__, retry);
		g_core_fp.fp_register_read(pfw_op->addr_fw_mode_status,
			tmp_data, DATA_LEN_4);

		if (disable)
			switch_cmd = pfw_op->data_idle_dis_pwd[0];
		else
			switch_cmd = pfw_op->data_idle_en_pwd[0];

		tmp_data[0] = switch_cmd;
		g_core_fp.fp_register_write(pfw_op->addr_fw_mode_status,
			tmp_data, DATA_LEN_4);
		g_core_fp.fp_register_read(pfw_op->addr_fw_mode_status,
			tmp_data, DATA_LEN_4);

		I(PRT_TMP_DATA,
			__func__,
			tmp_data[0],
			tmp_data[1],
			tmp_data[2],
			tmp_data[3]);

		retry--;
		usleep_range(10000, 11000);
	} while ((tmp_data[0] != switch_cmd) && retry > 0);

	I("%s: setting OK!\n", __func__);
}

static void himax_mcu_reload_disable(int disable)
{
	I("%s:entering\n", __func__);

	if (disable) { /*reload disable*/
		g_core_fp.fp_register_write(
			pdriver_op->addr_fw_define_flash_reload,
			pdriver_op->data_fw_define_flash_reload_dis,
			DATA_LEN_4);
	} else { /*reload enable*/
		g_core_fp.fp_register_write(
			pdriver_op->addr_fw_define_flash_reload,
			pdriver_op->data_fw_define_flash_reload_en,
			DATA_LEN_4);
	}

	I("%s: setting OK!\n", __func__);
}

static int himax_mcu_read_ic_trigger_type(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	int trigger_type = false;

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_int_is_edge,
		tmp_data, DATA_LEN_4);

	if ((tmp_data[1] & 0x01) == 1)
		trigger_type = true;

	return trigger_type;
}

static int himax_mcu_read_i2c_status(void)
{
	return i2c_error_count;
}

/* Please call this function after FW finish reload done */
static void himax_mcu_read_FW_ver(void)
{
	uint8_t data[12] = {0};

	g_core_fp.fp_register_read(pfw_op->addr_fw_ver_addr, data, DATA_LEN_4);
	ic_data->vendor_panel_ver =  data[0];
	ic_data->vendor_fw_ver = data[1] << 8 | data[2];
	I("PANEL_VER : %X\n", ic_data->vendor_panel_ver);
	I("FW_VER : %X\n", ic_data->vendor_fw_ver);
	g_core_fp.fp_register_read(pfw_op->addr_fw_cfg_addr, data, DATA_LEN_4);
	ic_data->vendor_config_ver = data[2] << 8 | data[3];
	/*I("CFG_VER : %X\n",ic_data->vendor_config_ver);*/
	ic_data->vendor_touch_cfg_ver = data[2];
	I("TOUCH_VER : %X\n", ic_data->vendor_touch_cfg_ver);
	ic_data->vendor_display_cfg_ver = data[3];
	I("DISPLAY_VER : %X\n", ic_data->vendor_display_cfg_ver);
	g_core_fp.fp_register_read(pfw_op->addr_fw_vendor_addr, data,
		DATA_LEN_4);
	ic_data->vendor_cid_maj_ver = data[2];
	ic_data->vendor_cid_min_ver = data[3];
	I("CID_VER : %X\n", (ic_data->vendor_cid_maj_ver << 8
			| ic_data->vendor_cid_min_ver));
	g_core_fp.fp_register_read(pfw_op->addr_cus_info, data, 12);
	memcpy(ic_data->vendor_cus_info, data, 12);
	I("Cusomer ID = %s\n", ic_data->vendor_cus_info);
	g_core_fp.fp_register_read(pfw_op->addr_proj_info, data, 12);
	memcpy(ic_data->vendor_proj_info, data, 12);
	I("Project ID = %s\n", ic_data->vendor_proj_info);
}

static bool himax_mcu_read_event_stack(uint8_t *buf, uint8_t length)
{
	uint8_t cmd[DATA_LEN_4];
	struct timespec64 t_start, t_end, t_delta;
	int len = length;
	int i2c_speed = 0;
	int ret = 0;

	/*  AHB_I2C Burst Read Off */
	cmd[0] = pfw_op->data_ahb_dis[0];

	ret = himax_bus_write(pfw_op->addr_ahb_addr[0], NULL, cmd, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return 0;
	}
	if (private_ts->debug_log_level & BIT(2))
		ktime_get_ts64(&t_start);

	himax_bus_read(pfw_op->addr_event_addr[0], buf, length);

	if (private_ts->debug_log_level & BIT(2)) {
		ktime_get_ts64(&t_end);
		t_delta.tv_nsec = (t_end.tv_sec * 1000000000 + t_end.tv_nsec)
			- (t_start.tv_sec * 1000000000 + t_start.tv_nsec);

		i2c_speed = (len * 9 * 1000000
			/ (int)t_delta.tv_nsec) * 13 / 10;
		private_ts->bus_speed = (int)i2c_speed;
	}

	/*  AHB_I2C Burst Read On */
	cmd[0] = pfw_op->data_ahb_en[0];

	ret = himax_bus_write(pfw_op->addr_ahb_addr[0], NULL, cmd, 1);
	if (ret < 0) {
		E("%s: bus access fail!\n", __func__);
		return 0;
	}

	return 1;
}

static void himax_mcu_return_event_stack(void)
{
	int retry = 20, i;
	uint8_t tmp_data[DATA_LEN_4];

	I("%s:entering\n", __func__);

	do {
		I("now %d times!\n", retry);

		for (i = 0; i < DATA_LEN_4; i++)
			tmp_data[i] = psram_op->addr_rawdata_end[i];

		g_core_fp.fp_register_write(psram_op->addr_rawdata_addr,
			tmp_data, DATA_LEN_4);
		g_core_fp.fp_register_read(psram_op->addr_rawdata_addr,
			tmp_data, DATA_LEN_4);
		retry--;
		usleep_range(10000, 11000);
	} while ((tmp_data[1] != psram_op->addr_rawdata_end[1]
		&& tmp_data[0] != psram_op->addr_rawdata_end[0])
		&& retry > 0);

	I("%s: End of setting!\n", __func__);
}

static bool himax_mcu_calculateChecksum(bool change_iref, uint32_t size)
{
	uint32_t CRC_result = 0xFFFFFFFF;
	uint8_t i;
	uint8_t tmp_data[DATA_LEN_4];

	I("%s:Now size=%d\n", __func__, size);
	for (i = 0; i < DATA_LEN_4; i++)
		tmp_data[i] = psram_op->addr_rawdata_end[i];

	CRC_result = g_core_fp.fp_check_CRC(tmp_data, size);
	msleep(50);

	if (CRC_result != 0)
		I("%s: CRC Fail=%d\n", __func__, CRC_result);

	return (CRC_result == 0) ? true : false;
}

static void himax_mcu_read_FW_status(void)
{
	uint8_t len = 0;
	uint8_t i = 0;
	uint8_t addr[4] = {0};
	uint8_t data[4] = {0};

	len = (uint8_t)(sizeof(dbg_reg_ary)/sizeof(uint32_t));

	for (i = 0; i < len; i++) {
		himax_parse_assign_cmd(dbg_reg_ary[i], addr, 4);
		g_core_fp.fp_register_read(addr, data, DATA_LEN_4);

		I("reg[0-3] : 0x%08X = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
		dbg_reg_ary[i], data[0], data[1], data[2], data[3]);
	}
}

static void himax_mcu_irq_switch(int switch_on)
{
	if (switch_on) {
		if (private_ts->use_irq)
			himax_int_enable(switch_on);
		else
			hrtimer_start(&private_ts->timer, ktime_set(1, 0),
				HRTIMER_MODE_REL);

	} else {
		if (private_ts->use_irq)
			himax_int_enable(switch_on);
		else {
			hrtimer_cancel(&private_ts->timer);
			cancel_work_sync(&private_ts->work);
		}
	}
}

static int himax_mcu_assign_sorting_mode(uint8_t *tmp_data)
{
	I("%s:addr: 0x%02X%02X%02X%02X, write to:0x%02X%02X%02X%02X\n",
		__func__,
		pfw_op->addr_sorting_mode_en[3],
		pfw_op->addr_sorting_mode_en[2],
		pfw_op->addr_sorting_mode_en[1],
		pfw_op->addr_sorting_mode_en[0],
		tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);

	g_core_fp.fp_register_write(pfw_op->addr_sorting_mode_en,
		tmp_data, DATA_LEN_4);

	return NO_ERR;
}

static int himax_mcu_check_sorting_mode(uint8_t *tmp_data)
{
	int ret = NO_ERR;

	ret = g_core_fp.fp_register_read(pfw_op->addr_sorting_mode_en, tmp_data,
		DATA_LEN_4);
	I("%s:addr: 0x%02X%02X%02X%02X, Now is:0x%02X%02X%02X%02X\n",
		__func__,
		pfw_op->addr_sorting_mode_en[3],
		pfw_op->addr_sorting_mode_en[2],
		pfw_op->addr_sorting_mode_en[1],
		pfw_op->addr_sorting_mode_en[0],
		tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
	if (tmp_data[3] == 0xFF
		&& tmp_data[2] == 0xFF
		&& tmp_data[1] == 0xFF
		&& tmp_data[0] == 0xFF) {
		ret = BUS_FAIL;
		I("%s, All 0xFF, Fail!\n", __func__);
	}

	return ret;
}

static uint8_t himax_mcu_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data)
{
	int cnt = 0;
	uint8_t req_size = cmd_set[0];

	cmd_set[3] = pfw_op->data_dd_request[0];
	g_core_fp.fp_register_write(pfw_op->addr_dd_handshak_addr,
		cmd_set, DATA_LEN_4);
	I("%s:cmd set[0]=0x%2X,set[1]=0x%2X,set[2]=0x%2X,set[3]=0x%2X\n",
		__func__, cmd_set[0], cmd_set[1], cmd_set[2], cmd_set[3]);

	/* Doing hand shaking 0xAA -> 0xBB */
	for (cnt = 0; cnt < 100; cnt++) {
		g_core_fp.fp_register_read(pfw_op->addr_dd_handshak_addr,
			tmp_data, DATA_LEN_4);
		usleep_range(10000, 11000);

		if (tmp_data[3] == pfw_op->data_dd_ack[0]) {
			I("%s Data ready goto moving data\n", __func__);
			goto FINALIZE;
		} else {
			if (cnt >= 99) {
				I("%s Data not ready in FW\n", __func__);
				return FW_NOT_READY;
			}
		}
	}
FINALIZE:
	g_core_fp.fp_register_read(pfw_op->addr_dd_data_addr, tmp_data,
		req_size);
	return NO_ERR;
}
static void hx_clr_fw_reord_dd_sts(void)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	g_core_fp.fp_register_read(pic_op->addr_cs_central_state, tmp_data,
		ADDR_LEN_4);
	I("%s: Check enter_save_mode data[0]=%02X\n", __func__, tmp_data[0]);

	if (tmp_data[0] == 0x0C) {
		I("%s: Enter safe mode, OK!\n", __func__);
	} else {
		E("%s: It doen't enter safe mode, please check it again\n",
			__func__);
		return;
	}
	g_core_fp.fp_register_read(pfw_op->addr_clr_fw_record_dd_sts, tmp_data,
		DATA_LEN_4);
	I("%s,Before Write :Now 10007FCC=0x%02X%02X%02X%02X\n",
		__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
	usleep_range(10000, 10001);

	tmp_data[2] = 0x00;
	tmp_data[3] = 0x00;
	g_core_fp.fp_register_write(pfw_op->addr_clr_fw_record_dd_sts,
		tmp_data, DATA_LEN_4);
	usleep_range(10000, 10001);

	g_core_fp.fp_register_read(pfw_op->addr_clr_fw_record_dd_sts, tmp_data,
		DATA_LEN_4);
	I("%s,After Write :Now 10007FCC=0x%02X%02X%02X%02X\n",
		__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

}

static void hx_ap_notify_fw_sus(int suspend)
{
	int retry = 0;
	int read_sts = 0;
	uint8_t read_tmp[DATA_LEN_4] = {0};
	uint8_t addr_tmp[DATA_LEN_4] = {0};
	uint8_t data_tmp[DATA_LEN_4] = {0};

	Arr4_to_Arr4(addr_tmp, pfw_op->addr_ap_notify_fw_sus);

	if (suspend) {
		I("%s,Suspend mode!\n", __func__);
		Arr4_to_Arr4(data_tmp, pfw_op->data_ap_notify_fw_sus_en);
	} else {
		I("%s,NonSuspend mode!\n", __func__);
		Arr4_to_Arr4(data_tmp, pfw_op->data_ap_notify_fw_sus_dis);
	}

	I("%s: R%02X%02X%02X%02XH<-0x%02X%02X%02X%02X\n",
		__func__,
		addr_tmp[3], addr_tmp[2], addr_tmp[1], addr_tmp[0],
		data_tmp[3], data_tmp[2], data_tmp[1], data_tmp[0]);
	do {
		g_core_fp.fp_register_write(addr_tmp, data_tmp,
			sizeof(data_tmp));
		usleep_range(1000, 1001);
		read_sts = g_core_fp.fp_register_read(addr_tmp, read_tmp,
			sizeof(read_tmp));
		I("%s: read bus status=%d\n", __func__, read_sts);
		I("%s: Now retry=%d, data=0x%02X%02X%02X%02X\n",
			__func__, retry,
			read_tmp[3], read_tmp[2], read_tmp[1], read_tmp[0]);
	} while ((retry++ < 10) && (read_sts != NO_ERR) &&
		(read_tmp[3] != data_tmp[3] && read_tmp[2] != data_tmp[2] &&
		read_tmp[1] != data_tmp[1] && read_tmp[0] != data_tmp[0]));
}
/* FW side end*/
/* CORE_FW */

/* CORE_FLASH */
/* FLASH side start*/
static void himax_mcu_chip_erase(void)
{
	g_core_fp.fp_interface_on();

	/* Reset power saving level */
	if (g_core_fp.fp_init_psl != NULL)
		g_core_fp.fp_init_psl();

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
		pflash_op->data_spi200_trans_fmt, DATA_LEN_4);

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
		pflash_op->data_spi200_trans_ctrl_2, DATA_LEN_4);
	g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
		pflash_op->data_spi200_cmd_2, DATA_LEN_4);

	g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
		pflash_op->data_spi200_cmd_3, DATA_LEN_4);
	msleep(2000);

	if (!g_core_fp.fp_wait_wip(100))
		E("%s: Chip_Erase Fail\n", __func__);

}

static bool himax_mcu_block_erase(int start_addr, int length)
{
	uint32_t page_prog_start = 0;
	uint32_t block_size = 0x10000;

	uint8_t tmp_data[4] = {0};

	g_core_fp.fp_interface_on();

	g_core_fp.fp_init_psl();

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
		pflash_op->data_spi200_trans_fmt, DATA_LEN_4);

	for (page_prog_start = start_addr;
	page_prog_start < start_addr + length;
	page_prog_start = page_prog_start + block_size) {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			pflash_op->data_spi200_trans_ctrl_2, DATA_LEN_4);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			pflash_op->data_spi200_cmd_2, DATA_LEN_4);

		tmp_data[3] = (page_prog_start >> 24)&0xFF;
		tmp_data[2] = (page_prog_start >> 16)&0xFF;
		tmp_data[1] = (page_prog_start >> 8)&0xFF;
		tmp_data[0] = page_prog_start&0xFF;
		g_core_fp.fp_register_write(pflash_op->addr_spi200_addr,
			tmp_data, DATA_LEN_4);

		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			pflash_op->data_spi200_trans_ctrl_3, DATA_LEN_4);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			pflash_op->data_spi200_cmd_4, DATA_LEN_4);
		msleep(1000);

		if (!g_core_fp.fp_wait_wip(100)) {
			E("%s:Erase Fail\n", __func__);
			return false;
		}
	}

	I("%s:END\n", __func__);
	return true;
}

static bool himax_mcu_sector_erase(int start_addr)
{
	return true;
}

static void himax_mcu_flash_programming(uint8_t *FW_content, int FW_Size)
{
	int page_prog_start = 0;
	int i = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;
	/* 4 bytes for padding*/
	g_core_fp.fp_interface_on();

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
		pflash_op->data_spi200_trans_fmt, DATA_LEN_4);

	for (page_prog_start = 0; page_prog_start < FW_Size;
	page_prog_start += FLASH_RW_MAX_LEN) {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			pflash_op->data_spi200_trans_ctrl_2, DATA_LEN_4);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			pflash_op->data_spi200_cmd_2, DATA_LEN_4);

		 /*Programmable size = 1 page = 256 bytes,*/
		 /*word_number = 256 byte / 4 = 64*/
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			pflash_op->data_spi200_trans_ctrl_4, DATA_LEN_4);

		/* Flash start address 1st : 0x0000_0000*/
		if (page_prog_start < 0x100) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = (uint8_t)page_prog_start;
		} else if (page_prog_start >= 0x100
		&& page_prog_start < 0x10000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		} else if (page_prog_start >= 0x10000
		&& page_prog_start < 0x1000000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = (uint8_t)(page_prog_start >> 16);
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		g_core_fp.fp_register_write(pflash_op->addr_spi200_addr,
			tmp_data, DATA_LEN_4);

		ret = g_core_fp.fp_register_write(pflash_op->addr_spi200_data,
			&FW_content[page_prog_start], 16);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return;
		}

		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			pflash_op->data_spi200_cmd_6, DATA_LEN_4);

		for (i = 0; i < 5; i++) {
			ret =
			g_core_fp.fp_register_write(pflash_op->addr_spi200_data,
				&FW_content[page_prog_start+16+(i*PROGRAM_SZ)],
				PROGRAM_SZ);
			if (ret < 0) {
				E("%s: bus access fail!\n", __func__);
				return;
			}
		}

		if (!g_core_fp.fp_wait_wip(1))
			E("%s:Flash_Programming Fail\n", __func__);

	}
}

static void himax_mcu_flash_page_write(uint8_t *write_addr, int length,
		uint8_t *write_data)
{
}

static void himax_flash_speed_set(uint8_t speed)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	himax_parse_assign_cmd(flash_clk_setup_addr, tmp_addr, 4);
	himax_parse_assign_cmd((uint32_t)speed, tmp_data, 4);
	g_core_fp.fp_register_write(tmp_addr, tmp_data, 4);
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_32k(unsigned char *fw,
		int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_60k(unsigned char *fw,
		int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k(unsigned char *fw,
		int len, bool change_iref)
{
	int burnFW_success = 0;

	if (len != FW_SIZE_64k) {
		E("%s: The file size is not 64K bytes\n", __func__);
		return false;
	}

#if defined(HX_RST_PIN_FUNC)
	g_core_fp.fp_ic_reset(false, false);
#else
	g_core_fp.fp_system_reset();
#endif
	g_core_fp.fp_sense_off(true);
	himax_flash_speed_set(HX_FLASH_SPEED_12p5M);
	g_core_fp.fp_block_erase(0x00, FW_SIZE_64k);
	g_core_fp.fp_flash_programming(fw, FW_SIZE_64k);

	if (g_core_fp.fp_check_CRC(pfw_op->addr_program_reload_from,
	FW_SIZE_64k) == 0)
		burnFW_success = 1;

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);

#if defined(HX_RST_PIN_FUNC)
	g_core_fp.fp_ic_reset(false, false);
#else
	/*System reset*/
	g_core_fp.fp_system_reset();
#endif
	return burnFW_success;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_124k(unsigned char *fw,
		int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k(unsigned char *fw,
		int len, bool change_iref)
{
	int burnFW_success = 0;

	if (len != FW_SIZE_128k) {
		E("%s: The file size is not 128K bytes\n", __func__);
		return false;
	}

#if defined(HX_RST_PIN_FUNC)
	g_core_fp.fp_ic_reset(false, false);
#else
	g_core_fp.fp_system_reset();
#endif
	g_core_fp.fp_sense_off(true);
	himax_flash_speed_set(HX_FLASH_SPEED_12p5M);
	g_core_fp.fp_block_erase(0x00, FW_SIZE_128k);
	g_core_fp.fp_flash_programming(fw, FW_SIZE_128k);

	if (g_core_fp.fp_check_CRC(pfw_op->addr_program_reload_from,
	FW_SIZE_128k) == 0)
		burnFW_success = 1;

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);

/*#if defined(HX_RST_PIN_FUNC)
 *	g_core_fp.fp_ic_reset(false, false);
 *#else
 *	//System reset
 *	g_core_fp.fp_system_reset();
 *#endif
 */
	return burnFW_success;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_255k(unsigned char *fw,
		int len, bool change_iref)
{
	int burnFW_success = 0;

	if (len != FW_SIZE_255k) {
		E("%s: The file size is not 255K bytes\n", __func__);
		return false;
	}

#if defined(HX_RST_PIN_FUNC)
	g_core_fp.fp_ic_reset(false, false);
#else
	g_core_fp.fp_system_reset();
#endif
	g_core_fp.fp_sense_off(true);
	himax_flash_speed_set(HX_FLASH_SPEED_12p5M);
	g_core_fp.fp_block_erase(0x00, FW_SIZE_255k);
	g_core_fp.fp_flash_programming(fw, FW_SIZE_255k);

	if (g_core_fp.fp_check_CRC(pfw_op->addr_program_reload_from,
	FW_SIZE_255k) == 0)
		burnFW_success = 1;

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);

/*#if defined(HX_RST_PIN_FUNC)
 *	g_core_fp.fp_ic_reset(false, false);
 *#else
 *	//System reset
 *	g_core_fp.fp_system_reset();
 *#endif
 */
	return burnFW_success;
}

static void himax_mcu_flash_dump_func(uint8_t local_flash_command,
		int Flash_Size, uint8_t *flash_buffer)
{
	uint8_t tmp_addr[DATA_LEN_4];
	int page_prog_start = 0;

	g_core_fp.fp_sense_off(true);

	for (page_prog_start = 0; page_prog_start < Flash_Size;
	page_prog_start += 128) {
		tmp_addr[0] = page_prog_start % 0x100;
		tmp_addr[1] = (page_prog_start >> 8) % 0x100;
		tmp_addr[2] = (page_prog_start >> 16) % 0x100;
		tmp_addr[3] = page_prog_start / 0x1000000;
		g_core_fp.fp_register_read(tmp_addr,
		flash_buffer+page_prog_start, 128);
	}

	g_core_fp.fp_sense_on(0x01);
}

static bool himax_mcu_flash_lastdata_check(uint32_t size)
{
	uint8_t tmp_addr[4];
	/* 64K - 0x80, which is the address of
	 * the last 128bytes in 64K, default value
	 */
	uint32_t start_addr = 0xFFFFFFFF;
	uint32_t temp_addr = 0;
	uint32_t flash_page_len = 0x80;
	uint8_t flash_tmp_buffer[128];

	if (size < flash_page_len) {
		E("%s: flash size is wrong, terminated\n", __func__);
		E("%s: flash size = %08X; flash page len = %08X\n", __func__,
			size, flash_page_len);
		goto FAIL;
	}

	/* In order to match other size of fw */
	start_addr = size - flash_page_len;
	I("%s: Now size is %d, the start_addr is 0x%08X\n",
			__func__, size, start_addr);
	for (temp_addr = start_addr; temp_addr < (start_addr + flash_page_len);
	temp_addr = temp_addr + flash_page_len) {
		/*I("temp_addr=%d,tmp_addr[0]=0x%2X, tmp_addr[1]=0x%2X,
		 *	tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n",
		 *	temp_addr,tmp_addr[0], tmp_addr[1],
		 *	tmp_addr[2],tmp_addr[3]);
		 */
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		g_core_fp.fp_register_read(tmp_addr, &flash_tmp_buffer[0],
			flash_page_len);
	}

	I("FLASH[%08X] ~ FLASH[%08X] = %02X%02X%02X%02X\n", size-4, size-1,
		flash_tmp_buffer[flash_page_len-4],
		flash_tmp_buffer[flash_page_len-3],
		flash_tmp_buffer[flash_page_len-2],
		flash_tmp_buffer[flash_page_len-1]);

	if ((!flash_tmp_buffer[flash_page_len-4])
	&& (!flash_tmp_buffer[flash_page_len-3])
	&& (!flash_tmp_buffer[flash_page_len-2])
	&& (!flash_tmp_buffer[flash_page_len-1])) {
		I("Fail, Last four Bytes are all 0x00:\n");
		goto FAIL;
	} else if ((flash_tmp_buffer[flash_page_len-4] == 0xFF)
	&& (flash_tmp_buffer[flash_page_len-3] == 0xFF)
	&& (flash_tmp_buffer[flash_page_len-2] == 0xFF)
	&& (flash_tmp_buffer[flash_page_len-1] == 0xFF)) {
		I("Fail, Last four Bytes are all 0xFF:\n");
		goto FAIL;
	} else {
		return 0;
	}

FAIL:
	return 1;
}

static bool hx_bin_desc_data_get(uint32_t addr, uint8_t *flash_buf)
{
	uint8_t data_sz = 0x10;
	uint32_t i = 0, j = 0;
	uint16_t chk_end = 0;
	uint16_t chk_sum = 0;
	uint32_t map_code = 0;
	unsigned long flash_addr = 0;

	for (i = 0; i < FW_PAGE_SZ; i = i + data_sz) {
		for (j = i; j < (i + data_sz); j++) {
			chk_end |= flash_buf[j];
			chk_sum += flash_buf[j];
		}
		if (!chk_end) { /*1. Check all zero*/
			I("%s: End in %X\n",	__func__, i + addr);
			return false;
		} else if (chk_sum % 0x100) { /*2. Check sum*/
			I("%s: chk sum failed in %X\n",	__func__, i + addr);
		} else { /*3. get data*/
			map_code = flash_buf[i] + (flash_buf[i + 1] << 8)
			+ (flash_buf[i + 2] << 16) + (flash_buf[i + 3] << 24);
			flash_addr = flash_buf[i + 4] + (flash_buf[i + 5] << 8)
			+ (flash_buf[i + 6] << 16) + (flash_buf[i + 7] << 24);
			switch (map_code) {
			case FW_CID:
				CID_VER_MAJ_FLASH_ADDR = flash_addr;
				CID_VER_MIN_FLASH_ADDR = flash_addr + 1;
				I("%s: CID_VER in %lX\n", __func__,
				CID_VER_MAJ_FLASH_ADDR);
				break;
			case FW_VER:
				FW_VER_MAJ_FLASH_ADDR = flash_addr;
				FW_VER_MIN_FLASH_ADDR = flash_addr + 1;
				I("%s: FW_VER in %lX\n", __func__,
				FW_VER_MAJ_FLASH_ADDR);
				break;
			case CFG_VER:
				CFG_VER_MAJ_FLASH_ADDR = flash_addr;
				CFG_VER_MIN_FLASH_ADDR = flash_addr + 1;
				I("%s: CFG_VER in = %08lX\n", __func__,
				CFG_VER_MAJ_FLASH_ADDR);
				break;
			case TP_CONFIG_TABLE:
				CFG_TABLE_FLASH_ADDR = flash_addr;
				I("%s: CONFIG_TABLE in %X\n",
				__func__, CFG_TABLE_FLASH_ADDR);
				break;
			}
		}
		chk_end = 0;
		chk_sum = 0;
	}

	return true;
}

static bool hx_mcu_bin_desc_get(unsigned char *fw, uint32_t max_sz)
{
	uint32_t addr_t = 0;
	unsigned char *fw_buf = NULL;
	bool keep_on_flag = false;
	bool g_bin_desc_flag = false;

	do {
		fw_buf = &fw[addr_t];

		/*Check bin is with description table or not*/
		if (!g_bin_desc_flag) {
			if (fw_buf[0x00] == 0x00 && fw_buf[0x01] == 0x00
			&& fw_buf[0x02] == 0x00 && fw_buf[0x03] == 0x00
			&& fw_buf[0x04] == 0x00 && fw_buf[0x05] == 0x00
			&& fw_buf[0x06] == 0x00 && fw_buf[0x07] == 0x00
			&& fw_buf[0x0E] == 0x87)
				g_bin_desc_flag = true;
		}
		if (!g_bin_desc_flag) {
			I("%s: fw_buf[0x00] = %2X, fw_buf[0x0E] = %2X\n",
			__func__, fw_buf[0x00], fw_buf[0x0E]);
			I("%s: No description table\n",	__func__);
			break;
		}

		/*Get related data*/
		keep_on_flag = hx_bin_desc_data_get(addr_t, fw_buf);

		addr_t = addr_t + FW_PAGE_SZ;
	} while (max_sz > addr_t && keep_on_flag);

	return g_bin_desc_flag;
}

static int hx_mcu_diff_overlay_flash(void)
{
	int rslt = 0;
	int diff_val = 0;

	diff_val = (ic_data->vendor_fw_ver);
	I("%s:Now fw ID is 0x%04X\n", __func__, diff_val);
	diff_val = (diff_val >> 12);
	I("%s:Now diff value=0x%04X\n", __func__, diff_val);

	if (diff_val == 1)
		I("%s:Now size should be 128K!\n", __func__);
	else
		I("%s:Now size should be 64K!\n", __func__);
	rslt = diff_val;
	return rslt;
}

/* FLASH side end*/
/* CORE_FLASH */

/* CORE_SRAM */
/* SRAM side start*/
static void himax_mcu_sram_write(uint8_t *FW_content)
{
}

static bool himax_mcu_sram_verify(uint8_t *FW_File, int FW_Size)
{
	return true;
}

static bool himax_mcu_get_DSRAM_data(uint8_t *info_data, bool DSRAM_Flag)
{
	unsigned int i = 0;
	unsigned char tmp_addr[ADDR_LEN_4];
	unsigned char tmp_data[DATA_LEN_4];
	unsigned int max_bus_size = MAX_I2C_TRANS_SZ;
	uint8_t x_num = ic_data->HX_RX_NUM;
	uint8_t y_num = ic_data->HX_TX_NUM;
	/*int m_key_num = 0;*/
	unsigned int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
	unsigned int data_size = (x_num * y_num + x_num + y_num) * 2;
	unsigned int remain_size;
	uint8_t retry = 0;
	/*int mutual_data_size = x_num * y_num * 2;*/
	unsigned int addr = 0;
	uint8_t  *temp_info_data = NULL; /*max mkey size = 8*/
	uint32_t checksum = 0;
	int fw_run_flag = -1;

#if defined(BUS_R_DLEN)
	max_bus_size = BUS_R_DLEN;
#endif

	if (strcmp(private_ts->chip_name, HX_83121A_SERIES_PWON) == 0) {
		if (max_bus_size > 4096)
			max_bus_size = 4096;
	}

	temp_info_data = kcalloc((total_size + 8), sizeof(uint8_t), GFP_KERNEL);
	if (temp_info_data == NULL) {
		E("%s, Failed to allocate memory\n", __func__);
		return false;
	}
	/* 1. Read number of MKey R100070E8H to determin data size */
	/* m_key_num = ic_data->HX_BT_NUM; */
	/* I("%s,m_key_num=%d\n",__func__ ,m_key_num); */
	/* total_size += m_key_num * 2; */

	/* 2. Start DSRAM Rawdata and Wait Data Ready */
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = psram_op->passwrd_start[1];
	tmp_data[0] = psram_op->passwrd_start[0];
	fw_run_flag = himax_write_read_reg(psram_op->addr_rawdata_addr,
			tmp_data,
			psram_op->passwrd_end[1],
			psram_op->passwrd_end[0]);

	if (fw_run_flag < 0) {
		E("%s: Data NOT ready => bypass\n", __func__);
		kfree(temp_info_data);
		return false;
	}
	
	if( private_ts->in_self_test ==1) {
		g_core_fp.fp_sense_off(true);
	}

	/* 3. Read RawData */
	while (retry++ < 5) {
		remain_size = total_size;
		while (remain_size > 0) {

			i = total_size - remain_size;
			addr = sram_adr_rawdata_addr + i;

			tmp_addr[3] = (uint8_t)((addr >> 24) & 0x00FF);
			tmp_addr[2] = (uint8_t)((addr >> 16) & 0x00FF);
			tmp_addr[1] = (uint8_t)((addr >> 8) & 0x00FF);
			tmp_addr[0] = (uint8_t)((addr) & 0x00FF);

			if (remain_size >= max_bus_size) {
				g_core_fp.fp_register_read(tmp_addr,
					&temp_info_data[i], max_bus_size);
				remain_size -= max_bus_size;
			} else {
				g_core_fp.fp_register_read(tmp_addr,
					&temp_info_data[i], remain_size);
				remain_size = 0;
			}
		}

		/* 5. Data Checksum Check */
		/* 2 is meaning PASSWORD NOT included */
		checksum = 0;
		for (i = 2; i < total_size; i += 2)
			checksum += temp_info_data[i+1]<<8 | temp_info_data[i];

		if (checksum % 0x10000 != 0) {

			E("%s: check_sum_cal fail=%08X\n", __func__, checksum);

		} else {
			memcpy(info_data, &temp_info_data[4],
				data_size * sizeof(uint8_t));
			break;
		}
	}

	/* 4. FW stop outputing */
	tmp_data[3] = temp_info_data[3];
	tmp_data[2] = temp_info_data[2];
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	g_core_fp.fp_register_write(psram_op->addr_rawdata_addr, tmp_data,
		DATA_LEN_4);

	kfree(temp_info_data);
	if (retry >= 5)
		return false;
	else
		return true;

}

/* SRAM side end*/
/* CORE_SRAM */

/* CORE_DRIVER */


static void himax_mcu_init_ic(void)
{
	I("%s: use default incell init.\n", __func__);
}

static void himax_suspend_proc(bool suspended)
{
	I("%s: himax suspend.\n", __func__);
}

static void himax_resume_proc(bool suspended)
{
#if defined(HX_ZERO_FLASH)
	int result = 0;
#endif

	I("%s: himax resume.\n", __func__);
#if defined(HX_ZERO_FLASH)
	if (g_core_fp.fp_0f_op_file_dirly != NULL) {
		result = g_core_fp.fp_0f_op_file_dirly(g_fw_boot_upgrade_name);
		if (result)
			E("%s: update FW fail, code[%d]!!\n", __func__, result);
	}
#else
	if (g_core_fp.fp_resend_cmd_func != NULL)
		g_core_fp.fp_resend_cmd_func(suspended);
#endif

	if (g_core_fp._ap_notify_fw_sus != NULL)
		g_core_fp._ap_notify_fw_sus(0);

	if (g_core_fp.fp_resume_ic_action != NULL)
		g_core_fp.fp_resume_ic_action();
}


#if defined(HX_RST_PIN_FUNC)
static void himax_mcu_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	usleep_range(RST_LOW_PERIOD_S, RST_LOW_PERIOD_E);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	usleep_range(RST_HIGH_PERIOD_S, RST_HIGH_PERIOD_E);
}

static void himax_mcu_ic_reset(uint8_t loadconfig, uint8_t int_off)
{
	struct himax_ts_data *ts = private_ts;

	HX_HW_RESET_ACTIVATE = 0;
	I("%s,status: loadconfig=%d,int_off=%d\n", __func__,
			loadconfig, int_off);

	if (ts->rst_gpio >= 0) {
		if (int_off)
			g_core_fp.fp_irq_switch(0);

		g_core_fp.fp_pin_reset();

		/* if (loadconfig) */
		/*	g_core_fp.fp_reload_config(); */

		if (int_off)
			g_core_fp.fp_irq_switch(1);

	}
}
#endif

static uint8_t himax_mcu_tp_info_check(void)
{
	char addr[DATA_LEN_4] = {0};
	char data[DATA_LEN_4] = {0};
	uint32_t rx_num;
	uint32_t tx_num;
	uint32_t bt_num;
	uint32_t max_pt;
	uint8_t int_is_edge;
	uint8_t stylus_func;
	uint8_t stylus_id_v2;
	uint8_t stylus_ratio;
	uint8_t err_cnt = 0;

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_rxnum_txnum, data,
		DATA_LEN_4);
	rx_num = data[2];
	tx_num = data[3];

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_maxpt_xyrvs, data,
		DATA_LEN_4);
	max_pt = data[0];

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_int_is_edge, data,
		DATA_LEN_4);
	if ((data[1] & 0x01) == 1)
		int_is_edge = true;
	else
		int_is_edge = false;

	/*1. Read number of MKey R100070E8H to determin data size*/
	g_core_fp.fp_register_read(psram_op->addr_mkey, data, DATA_LEN_4);
	bt_num = data[0] & 0x03;

	addr[3] = 0x10;
	addr[2] = 0x00;
	addr[1] = 0x71;
	addr[0] = 0x9C;
	g_core_fp.fp_register_read(addr, data, DATA_LEN_4);
	stylus_func = data[3];

	if (ic_data->HX_RX_NUM != rx_num) {
		err_cnt++;
		W("%s: RX_NUM, Set = %d ; FW = %d", __func__,
			ic_data->HX_RX_NUM, rx_num);
	}

	if (ic_data->HX_TX_NUM != tx_num) {
		err_cnt++;
		W("%s: TX_NUM, Set = %d ; FW = %d", __func__,
			ic_data->HX_TX_NUM, tx_num);
	}

	if (ic_data->HX_BT_NUM != bt_num) {
		err_cnt++;
		W("%s: BT_NUM, Set = %d ; FW = %d", __func__,
			ic_data->HX_BT_NUM, bt_num);
	}

	if (ic_data->HX_MAX_PT != max_pt) {
		err_cnt++;
		W("%s: MAX_PT, Set = %d ; FW = %d", __func__,
			ic_data->HX_MAX_PT, max_pt);
	}

	if (ic_data->HX_INT_IS_EDGE != int_is_edge) {
		err_cnt++;
		W("%s: INT_IS_EDGE, Set = %d ; FW = %d", __func__,
			ic_data->HX_INT_IS_EDGE, int_is_edge);
	}

	if (ic_data->HX_STYLUS_FUNC != stylus_func) {
		err_cnt++;
		W("%s: STYLUS_FUNC, Set = %d ; FW = %d", __func__,
			ic_data->HX_STYLUS_FUNC, stylus_func);
	}

	if (ic_data->HX_STYLUS_FUNC) {
		addr[3] = 0x10;
		addr[2] = 0x00;
		addr[1] = 0x71;
		addr[0] = 0xFC;
		g_core_fp.fp_register_read(addr, data, DATA_LEN_4);
		stylus_id_v2 = data[2];/*0x100071FE 0=off 1=on*/
		stylus_ratio = data[3];
		/*0x100071FF 0=ratio_1 10=ratio_10*/
		if (ic_data->HX_STYLUS_ID_V2 != stylus_id_v2) {
			err_cnt++;
			W("%s: STYLUS_ID_V2, Set = %d ; FW = %d", __func__,
				ic_data->HX_STYLUS_ID_V2, stylus_id_v2);
		}
		/*
		if (ic_data->HX_STYLUS_RATIO != stylus_ratio) {
			err_cnt++;
			W("%s: STYLUS_RATIO, Set = %d ; FW = %d", __func__,
				ic_data->HX_STYLUS_RATIO, stylus_ratio);
		} */
	}

	if (err_cnt > 0)
		W("FIX_TOUCH_INFO does NOT match to FW information\n");
	else
		I("FIX_TOUCH_INFO is OK\n");

	return err_cnt;
}

static void himax_mcu_touch_information(void)
{
	if (ic_data->HX_RX_NUM == 0xFFFFFFFF)
		ic_data->HX_RX_NUM = FIX_HX_RX_NUM;

	if (ic_data->HX_TX_NUM == 0xFFFFFFFF)
		ic_data->HX_TX_NUM = FIX_HX_TX_NUM;

	if (ic_data->HX_BT_NUM == 0xFFFFFFFF)
		ic_data->HX_BT_NUM = FIX_HX_BT_NUM;

	if (ic_data->HX_MAX_PT == 0xFFFFFFFF)
		ic_data->HX_MAX_PT = FIX_HX_MAX_PT;

	if (ic_data->HX_INT_IS_EDGE == 0xFF)
		ic_data->HX_INT_IS_EDGE = FIX_HX_INT_IS_EDGE;

	if (ic_data->HX_STYLUS_FUNC == 0xFF)
		ic_data->HX_STYLUS_FUNC = FIX_HX_STYLUS_FUNC;

	if (ic_data->HX_STYLUS_ID_V2 == 0xFF)
		ic_data->HX_STYLUS_ID_V2 = FIX_HX_STYLUS_ID_V2;

	if (ic_data->HX_STYLUS_RATIO == 0xFF)
		ic_data->HX_STYLUS_RATIO = FIX_HX_STYLUS_RATIO;

	ic_data->HX_Y_RES = private_ts->pdata->screenHeight;
	ic_data->HX_X_RES = private_ts->pdata->screenWidth;

	I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d\n", __func__,
		ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	I("%s:HX_MAX_PT=%d\n", __func__, ic_data->HX_MAX_PT);
	I("%s:HX_Y_RES=%d,HX_X_RES =%d\n", __func__,
		ic_data->HX_Y_RES, ic_data->HX_X_RES);
	I("%s:HX_INT_IS_EDGE =%d,HX_STYLUS_FUNC = %d\n", __func__,
	ic_data->HX_INT_IS_EDGE, ic_data->HX_STYLUS_FUNC);
		I("%s:HX_STYLUS_ID_V2 =%d,HX_STYLUS_RATIO = %d\n", __func__,
	ic_data->HX_STYLUS_ID_V2, ic_data->HX_STYLUS_RATIO);
}

static void himax_mcu_calcTouchDataSize(void)
{
	struct himax_ts_data *ts_data = private_ts;

	ts_data->x_channel = ic_data->HX_RX_NUM;
	ts_data->y_channel = ic_data->HX_TX_NUM;
	ts_data->nFinger_support = ic_data->HX_MAX_PT;

	HX_TOUCH_INFO_POINT_CNT = ic_data->HX_MAX_PT * 4;
	if ((ic_data->HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT +=
			(ic_data->HX_MAX_PT / 4) * 4;
	else
		HX_TOUCH_INFO_POINT_CNT +=
			((ic_data->HX_MAX_PT / 4) + 1) * 4;

	if (himax_report_data_init())
		E("%s: allocate data fail\n", __func__);
}

static int himax_mcu_get_touch_data_size(void)
{
	return HIMAX_TOUCH_DATA_SIZE;
}

static int himax_mcu_hand_shaking(void)
{
	/* 0:Running, 1:Stop, 2:I2C Fail */
	int result = 0;
	return result;
}

static int himax_mcu_determin_diag_rawdata(int diag_command)
{
	return diag_command % 10;
}

static int himax_mcu_determin_diag_storage(int diag_command)
{
	return diag_command / 10;
}

static int himax_mcu_cal_data_len(int raw_cnt_rmd, int HX_MAX_PT,
		int raw_cnt_max)
{
	int RawDataLen;
	/* rawdata checksum is 2 bytes */
	if (raw_cnt_rmd != 0x00)
		RawDataLen = MAX_I2C_TRANS_SZ
			- ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 2;
	else
		RawDataLen = MAX_I2C_TRANS_SZ
			- ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 2;

	return RawDataLen;
}

static bool himax_mcu_diag_check_sum(struct himax_report_data *hx_touch_data)
{
	uint16_t check_sum_cal = 0;
	int i;

	/* Check 128th byte CRC */
	for (i = 0, check_sum_cal = 0;
	i < (hx_touch_data->touch_all_size
	- hx_touch_data->touch_info_size);
	i += 2) {
		check_sum_cal += (hx_touch_data->hx_rawdata_buf[i + 1]
			* FLASH_RW_MAX_LEN
			+ hx_touch_data->hx_rawdata_buf[i]);
	}

	if (check_sum_cal % HX64K != 0) {
		I("%s fail=%2X\n", __func__, check_sum_cal);
		return 0;
	}

	return 1;
}

static void himax_mcu_diag_parse_raw_data(
		struct himax_report_data *hx_touch_data,
		int mul_num, int self_num, uint8_t diag_cmd,
		int32_t *mutual_data, int32_t *self_data)
{
	diag_mcu_parse_raw_data(hx_touch_data, mul_num, self_num,
			diag_cmd, mutual_data, self_data);
}

#if defined(HX_EXCP_RECOVERY)
static int himax_mcu_ic_excp_recovery(uint32_t hx_excp_event,
		uint32_t hx_zero_event, uint32_t length)
{
	int ret_val = NO_ERR;

	if (hx_excp_event == length) {
		g_zero_event_count = 0;
		ret_val = HX_EXCP_EVENT;
	} else if (hx_zero_event == length) {
		if (g_zero_event_count > 5) {
			g_zero_event_count = 0;
			I("EXCEPTION event checked - ALL Zero.\n");
			ret_val = HX_EXCP_EVENT;
		} else {
			g_zero_event_count++;
			I("ALL Zero event is %d times.\n",
					g_zero_event_count);
			ret_val = HX_ZERO_EVENT_COUNT;
		}
	}

	return ret_val;
}

static void himax_mcu_excp_ic_reset(void)
{
	HX_EXCP_RESET_ACTIVATE = 0;
#if defined(HX_RST_PIN_FUNC)
	himax_mcu_pin_reset();
#else
	himax_mcu_system_reset();
#endif
	I("%s:\n", __func__);
}
#endif
#if defined(HX_TP_PROC_GUEST_INFO)
char *g_checksum_str = "check sum fail";
/* char *g_guest_info_item[] = {
 *	"projectID",
 *	"CGColor",
 *	"BarCode",
 *	"Reserve1",
 *	"Reserve2",
 *	"Reserve3",
 *	"Reserve4",
 *	"Reserve5",
 *	"VCOM",
 *	"Vcom-3Gar",
 *	NULL
 * };
 */

static int himax_guest_info_get_status(void)
{
	return g_guest_info_data->g_guest_info_ongoing;
}
static void himax_guest_info_set_status(int setting)
{
	g_guest_info_data->g_guest_info_ongoing = setting;
}

static int himax_guest_info_read(uint32_t start_addr,
		uint8_t *flash_tmp_buffer)
{
	uint32_t temp_addr = 0;
	uint8_t tmp_addr[4];
	uint32_t flash_page_len = 0x1000;
	/* uint32_t checksum = 0x00; */
	int result = -1;


	I("%s:Reading guest info in start_addr = 0x%08X !\n", __func__,
		start_addr);

	tmp_addr[0] = start_addr % 0x100;
	tmp_addr[1] = (start_addr >> 8) % 0x100;
	tmp_addr[2] = (start_addr >> 16) % 0x100;
	tmp_addr[3] = start_addr / 0x1000000;
	I("addr[0]=0x%2X,addr[1]=0x%2X,addr[2]=0x%2X,addr[3]=0x%2X\n",
		tmp_addr[0], tmp_addr[1],
		tmp_addr[2], tmp_addr[3]);

	result = g_core_fp.fp_check_CRC(tmp_addr, flash_page_len);
	I("Checksum = 0x%8X\n", result);
	if (result != 0)
		goto END_FUNC;

	for (temp_addr = start_addr;
	temp_addr < (start_addr + flash_page_len);
	temp_addr = temp_addr + 128) {

		/* I("temp_addr=%d,tmp_addr[0]=0x%2X,tmp_addr[1]=0x%2X,
		 *	tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n",
		 *	temp_addr,tmp_addr[0],tmp_addr[1],
		 *	tmp_addr[2],tmp_addr[3]);
		 */
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		g_core_fp.fp_register_read(tmp_addr,
			&flash_tmp_buffer[temp_addr - start_addr], 128);
		/* memcpy(&flash_tmp_buffer[temp_addr - start_addr],
		 *	buffer,128);
		 */
	}

END_FUNC:
	return result;
}

static int hx_read_guest_info(void)
{
	/* uint8_t tmp_addr[4]; */
	uint32_t panel_info_addr = HX_GUEST_INFO_FLASH_SADDR;

	uint32_t info_len;
	uint32_t flash_page_len = 0x1000;/*4k*/
	uint8_t *flash_tmp_buffer = NULL;
	/* uint32_t temp_addr = 0; */
	uint8_t temp_str[128];
	int i = 0;
	unsigned int custom_info_temp = 0;
	int checksum = 0;

	himax_guest_info_set_status(1);

	flash_tmp_buffer = kcalloc(HX_GUEST_INFO_SIZE * flash_page_len,
		sizeof(uint8_t), GFP_KERNEL);
	if (flash_tmp_buffer == NULL) {
		I("%s: Memory allocate fail!\n", __func__);
		return MEM_ALLOC_FAIL;
	}

	g_core_fp.fp_sense_off(true);
	/* g_core_fp.fp_burst_enable(1); */

	for (custom_info_temp = 0;
	custom_info_temp < HX_GUEST_INFO_SIZE;
	custom_info_temp++) {
		checksum = himax_guest_info_read(panel_info_addr
			+ custom_info_temp
			* flash_page_len,
			&flash_tmp_buffer[custom_info_temp * flash_page_len]);
		if (checksum != 0) {
			E("%s:Checksum Fail! g_checksum_str len=%d\n", __func__,
				(int)strlen(g_checksum_str));
			memcpy(&g_guest_info_data->
				g_guest_str_in_format[custom_info_temp][0],
				g_checksum_str, (int)strlen(g_checksum_str));
			memcpy(&g_guest_info_data->
				g_guest_str[custom_info_temp][0],
				g_checksum_str, (int)strlen(g_checksum_str));
			continue;
		}

		info_len = flash_tmp_buffer[custom_info_temp * flash_page_len]
			+ (flash_tmp_buffer[custom_info_temp
			* flash_page_len + 1] << 8)
			+ (flash_tmp_buffer[custom_info_temp
			* flash_page_len + 2] << 16)
			+ (flash_tmp_buffer[custom_info_temp
			* flash_page_len + 3] << 24);

		I("Now custom_info_temp = %d\n", custom_info_temp);

		I("Now size_buff[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
			flash_tmp_buffer[custom_info_temp*flash_page_len],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 1],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 2],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 3]);

		I("Now total length=%d\n", info_len);

		g_guest_info_data->g_guest_data_len[custom_info_temp] =
			info_len;

		I("Now custom_info_id [0]=%d,[1]=%d,[2]=%d,[3]=%d\n",
			flash_tmp_buffer[custom_info_temp*flash_page_len + 4],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 5],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 6],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 7]);

		g_guest_info_data->g_guest_data_type[custom_info_temp] =
			flash_tmp_buffer[custom_info_temp * flash_page_len
			+ 7];

		/* if(custom_info_temp < 3) { */
		if (info_len > 128) {
			I("%s: info_len=%d\n", __func__, info_len);
			info_len = 128;
		}
		for (i = 0; i < info_len; i++)
			temp_str[i] = flash_tmp_buffer[custom_info_temp
				* flash_page_len
				+ HX_GUEST_INFO_LEN_SIZE
				+ HX_GUEST_INFO_ID_SIZE
				+ i];

		I("g_guest_info_data->g_guest_str_in_format[%d]size=%d\n",
			custom_info_temp, info_len);
		memcpy(&g_guest_info_data->
			g_guest_str_in_format[custom_info_temp][0],
			temp_str, info_len);
		/*}*/

		for (i = 0; i < 128; i++)
			temp_str[i] = flash_tmp_buffer[custom_info_temp
				* flash_page_len
				+ i];

		I("g_guest_info_data->g_guest_str[%d] size = %d\n",
				custom_info_temp, 128);
		memcpy(&g_guest_info_data->g_guest_str[custom_info_temp][0],
				temp_str, 128);
		/*if(custom_info_temp == 0)
		 *{
		 *	for ( i = 0; i< 256 ; i++) {
		 *		if(i % 16 == 0 && i > 0)
		 *			I("\n");
		 *		I("g_guest_info_data->g_guest_str[%d][%d]
					= 0x%02X", custom_info_temp, i,
					g_guest_info_data->g_guest_str[
					custom_info_temp][i]);
		 *	}
		 *}
		 */
	}
	/* himax_burst_enable(private_ts->client, 0); */
	g_core_fp.fp_sense_on(0x01);

	kfree(flash_tmp_buffer);
	himax_guest_info_set_status(0);
	return NO_ERR;
}
#endif
/* CORE_DRIVER */

static void himax_mcu_resend_cmd_func(bool suspended)
{
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE)
	struct himax_ts_data *ts = private_ts;
#endif
#if defined(HX_SMART_WAKEUP)
	g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable, suspended);
#endif
#if defined(HX_HIGH_SENSE)
	g_core_fp.fp_set_HSEN_enable(ts->HSEN_enable, suspended);
#endif
#if defined(HX_USB_DETECT_GLOBAL)
	himax_cable_detect_func(true);
#endif
}


int hx_turn_on_mp_func(int on)
{
	int rslt = 0;
	int retry = 3;
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	uint8_t tmp_read[4] = {0};
	/* char *tmp_chipname = private_ts->chip_name; */

	if (strcmp(HX_83102D_SERIES_PWON, private_ts->chip_name) == 0) {
		himax_parse_assign_cmd(fw_addr_ctrl_mpap_ovl, tmp_addr,
			sizeof(tmp_addr));
		if (on) {
			I("%s : Turn on MPAP mode!\n", __func__);
			himax_parse_assign_cmd(fw_data_ctrl_mpap_ovl_on,
				tmp_data, sizeof(tmp_data));
			do {
				g_core_fp.fp_register_write(tmp_addr, tmp_data,
					4);
				usleep_range(10000, 10001);
				g_core_fp.fp_register_read(tmp_addr, tmp_read,
					4);

				I("%s:read2=0x%02X,read1=0x%02X,read0=0x%02X\n",
					__func__, tmp_read[2], tmp_read[1],
					tmp_read[0]);

				retry--;
			} while (((retry > 0)
			&& (tmp_read[2] != tmp_data[2]
			&& tmp_read[1] != tmp_data[1]
			&& tmp_read[0] != tmp_data[0])));
		} else {
			I("%s : Turn off MPAP mode!\n", __func__);
			himax_parse_assign_cmd(fw_data_clear, tmp_data,
				sizeof(tmp_data));
			do {
				g_core_fp.fp_register_write(tmp_addr, tmp_data,
					4);
				usleep_range(10000, 10001);
				g_core_fp.fp_register_read(tmp_addr, tmp_read,
					4);

				I("%s:read2=0x%02X,read1=0x%02X,read0=0x%02X\n",
					__func__, tmp_read[2], tmp_read[1],
					tmp_read[0]);

				retry--;
			} while ((retry > 0)
			&& (tmp_read[2] != tmp_data[2]
			&& tmp_read[1] != tmp_data[1]
			&& tmp_read[0] != tmp_data[0]));
		}
	} else {
		I("%s Nothing to be done!\n", __func__);
	}

	return rslt;
}

#if defined(HX_ZERO_FLASH)
int G_POWERONOF = 1;
EXPORT_SYMBOL(G_POWERONOF);
void hx_dis_rload_0f(int disable)
{
	/*Diable Flash Reload*/
	g_core_fp.fp_register_write(pdriver_op->addr_fw_define_flash_reload,
		pzf_op->data_dis_flash_reload, DATA_LEN_4);
}

void himax_mcu_clean_sram_0f(uint8_t *addr, int write_len, int type)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int address = 0;
	int i = 0;

	uint8_t fix_data = 0x00;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[MAX_I2C_TRANS_SZ] = {0};

	I("%s, Entering\n", __func__);

	total_size_temp = write_len;

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s:addr[3]=0x%2X,addr[2]=0x%2X,addr[1]=0x%2X,addr[0]=0x%2X\n",
		__func__,
		tmp_addr[3],
		tmp_addr[2],
		tmp_addr[1],
		tmp_addr[0]);

	switch (type) {
	case 0:
		fix_data = 0x00;
		break;
	case 1:
		fix_data = 0xAA;
		break;
	case 2:
		fix_data = 0xBB;
		break;
	}

	for (i = 0; i < MAX_I2C_TRANS_SZ; i++)
		tmp_data[i] = fix_data;

	I("%s,  total size=%d\n", __func__, total_size_temp);

	if (total_size_temp % max_bus_size == 0)
		total_read_times = total_size_temp / max_bus_size;
	else
		total_read_times = total_size_temp / max_bus_size + 1;

	for (i = 0; i < (total_read_times); i++) {
		I("[log]write %d time start!\n", i);
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_register_write(tmp_addr, tmp_data,
				max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			I("last total_size_temp=%d\n", total_size_temp);
			g_core_fp.fp_register_write(tmp_addr, tmp_data,
				total_size_temp % max_bus_size);
		}
		address = ((i+1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		usleep_range(10000, 11000);
	}

	I("%s, END\n", __func__);
}

void himax_mcu_write_sram_0f(uint8_t *addr, const uint8_t *data, uint32_t len)
{
	int max_bus_size = MAX_I2C_TRANS_SZ;
	uint32_t remain_len = 0;
	uint32_t address = 0;
	uint32_t i;
	uint8_t tmp_addr[4];

	I("%s: Entering - total write size = %d\n", __func__, len);

#if defined(BUS_W_DLEN)
	max_bus_size = BUS_W_DLEN-ADDR_LEN_4;
#endif

	if (strcmp(private_ts->chip_name, HX_83121A_SERIES_PWON) == 0) {
		if (max_bus_size > 4096)
			max_bus_size = 4096;
	}

	address = (addr[3]<<24) + (addr[2]<<16) + (addr[1]<<8) + addr[0];
	remain_len = len;

	while (remain_len > 0) {
		i = len - remain_len;
		address += i;
		tmp_addr[3] = (address>>24) & 0x00FF;
		tmp_addr[2] = (address>>16) & 0x00FF;
		tmp_addr[1] = (address>>8) & 0x00FF;
		tmp_addr[0] = address & 0x00FF;

		if (remain_len > max_bus_size) {
			g_core_fp.fp_register_write(tmp_addr, (uint8_t *)data+i,
				max_bus_size);
			remain_len -= max_bus_size;
		} else {
			g_core_fp.fp_register_write(tmp_addr, (uint8_t *)data+i,
				remain_len);
			remain_len = 0;
		}

	//	udelay(100);
	}

	I("%s, End\n", __func__);
	/* kfree(tmp_data); */
}

int himax_sram_write_crc_check(uint8_t *addr, const uint8_t *data, uint32_t len)
{
	int retry = 0;
	int crc = -1;

	do {
		g_core_fp.fp_write_sram_0f(addr, data, len);
		crc = g_core_fp.fp_check_CRC(addr, len);
		retry++;
		I("%s, HW CRC %s in %d time\n", __func__,
			(crc == 0) ? "OK" : "Fail", retry);
	} while (crc != 0 && retry < 3);

	return crc;
}

int himax_zf_part_info(const struct firmware *fw, int type)
{
	uint32_t table_addr = CFG_TABLE_FLASH_ADDR;
	int pnum = 0;
	int ret = 0;
	uint8_t buf[16];
	struct zf_info *info;
	uint8_t *cfg_buf;
	uint8_t sram_min[4];
	int cfg_sz = 0;
	int cfg_crc_sw = 0;
	int cfg_crc_hw = 0;
	uint8_t i = 0;
	int i_max = 0;
	int i_min = 0;
	uint32_t dsram_base = 0xFFFFFFFF;
	uint32_t dsram_max = 0;
	int retry = 0;

#if defined(HX_ALG_OVERLAY) || defined(HX_CODE_OVERLAY)
	uint8_t tmp_addr[4] = {0xFC, 0x7F, 0x00, 0x10};
	uint8_t rdata[4] = {0};
	int allovlidx = 0;
#endif
#if defined(HX_ALG_OVERLAY)
	uint8_t alg_sdata[4] = {0xA5, 0x5A, 0x5A, 0xA5};
	uint8_t alg_idx_t = 0;
	uint8_t data[4] = {0x01, 0x00, 0x00, 0x00};
#endif
#if defined(HX_CODE_OVERLAY)
	uint8_t code_sdata[4] = {0};
	uint8_t code_idx_t = 0;
	uint8_t j = 0;
	bool has_code_overlay = false;
#endif

	/* 1. initial check */
	pnum = fw->data[table_addr + 12];
	if (pnum < 2) {
		E("%s: partition number is not correct\n", __func__);
		return FW_NOT_READY;
	}

	info = kcalloc(pnum, sizeof(struct zf_info), GFP_KERNEL);
	if (info == NULL) {
		E("%s: memory allocation fail[info]!!\n", __func__);
		return 1;
	}
	memset(info, 0, pnum * sizeof(struct zf_info));

#if defined(HX_CODE_OVERLAY)
	if (ovl_idx == NULL) {
		ovl_idx = kzalloc(ovl_section_num, GFP_KERNEL);
		if (ovl_idx == NULL) {
			E("%s, ovl_idx alloc failed!\n", __func__);
			return 1;
		}
	} else {
		memset(ovl_idx, 0, ovl_section_num);
	}
#endif

	/* 2. record partition information */
	memcpy(buf, &fw->data[table_addr], 16);
	memcpy(info[0].sram_addr, buf, 4);
	info[0].write_size = buf[7] << 24 | buf[6] << 16 | buf[5] << 8 | buf[4];
	info[0].fw_addr = buf[11] << 24 | buf[10] << 16 | buf[9] << 8 | buf[8];

	for (i = 1; i < pnum; i++) {
		memcpy(buf, &fw->data[i*0x10 + table_addr], 16);

		memcpy(info[i].sram_addr, buf, 4);
		info[i].write_size = buf[7] << 24 | buf[6] << 16
				| buf[5] << 8 | buf[4];
		info[i].fw_addr = buf[11] << 24 | buf[10] << 16
				| buf[9] << 8 | buf[8];
		info[i].cfg_addr = info[i].sram_addr[0];
		info[i].cfg_addr += info[i].sram_addr[1] << 8;
		info[i].cfg_addr += info[i].sram_addr[2] << 16;
		info[i].cfg_addr += info[i].sram_addr[3] << 24;

		if (info[i].cfg_addr % 4 != 0)
			info[i].cfg_addr -= (info[i].cfg_addr % 4);

		I("%s,[%d]SRAM addr=%08X, fw_addr=%08X, write_size=%d\n",
			__func__, i, info[i].cfg_addr, info[i].fw_addr,
			info[i].write_size);

#if defined(HX_ALG_OVERLAY)
		/* alg overlay section */
		if ((buf[15] == 0x77 && buf[14] == 0x88)) {
			I("%s: find alg overlay section in index %d\n",
				__func__, i);
			/* record index of alg overlay section */
			allovlidx |= 1<<i;
			alg_idx_t = i;
			continue;
		}
#endif

#if defined(HX_CODE_OVERLAY)
		/* code overlay section */
		if ((buf[15] == 0x55 && buf[14] == 0x66)
		|| (buf[3] == 0x20 && buf[2] == 0x00
		&& buf[1] == 0x8C && buf[0] == 0xE0)) {
			I("%s: find code overlay section in index %d\n",
				__func__, i);
			has_code_overlay = true;
			/* record index of code overlay section */
			allovlidx |= 1<<i;
			if (buf[15] == 0x55 && buf[14] == 0x66) {
				/* current mechanism */
				j = buf[13];
				if (j < ovl_section_num)
					ovl_idx[j] = i;
			} else {
				/* previous mechanism */
				if (j < ovl_section_num)
					ovl_idx[j++] = i;
			}
			continue;
		}
#endif

		if (dsram_base > info[i].cfg_addr) {
			dsram_base = info[i].cfg_addr;
			i_min = i;
		}
		if (dsram_max < info[i].cfg_addr) {
			dsram_max = info[i].cfg_addr;
			i_max = i;
		}
	}

	/* 3. prepare data to update */
#if defined(HX_ALG_OVERLAY)
	if (alg_idx_t == 0 || info[alg_idx_t].write_size == 0) {
		E("%s: wrong alg overlay section[%d, %d]!\n", __func__,
			alg_idx_t, info[alg_idx_t].write_size);
		ret = FW_NOT_READY;
		goto ALOC_CFG_BUF_FAIL;
	}
#endif
#if defined(HX_CODE_OVERLAY)
	/* ovl_idx[0] - sorting */
	/* ovl_idx[1] - gesture */
	/* ovl_idx[2] - border	*/
	if (has_code_overlay) {
		code_idx_t = ovl_idx[0];
		code_sdata[0] = ovl_sorting_reply;

		if (type == 0) {
#if defined(HX_SMART_WAKEUP)
			if (private_ts->suspended && private_ts->SMWP_enable) {
				code_idx_t = ovl_idx[1];
				code_sdata[0] = ovl_gesture_reply;
			} else {
				code_idx_t = ovl_idx[2];
				code_sdata[0] = ovl_border_reply;
			}
#else
			code_idx_t = ovl_idx[2];
			code_sdata[0] = ovl_border_reply;
#endif
		}

		if (code_idx_t == 0 || info[code_idx_t].write_size == 0) {
			E("%s: wrong code overlay section[%d, %d]!\n", __func__,
				code_idx_t, info[code_idx_t].write_size);
			ret = FW_NOT_READY;
			goto ALOC_CFG_BUF_FAIL;
		}
	}
#endif

	for (i = 0; i < ADDR_LEN_4; i++)
		sram_min[i] = (info[i_min].cfg_addr>>(8*i)) & 0xFF;

	cfg_sz = (dsram_max - dsram_base) + info[i_max].write_size;
	if (cfg_sz % 16 != 0)
		cfg_sz = cfg_sz + 16 - (cfg_sz % 16);

	I("%s, cfg_sz = %d!, dsram_base = %X, dsram_max = %X\n", __func__,
			cfg_sz, dsram_base, dsram_max);

	/* config size should be smaller than DSRAM size */
	if (cfg_sz > DSRAM_SIZE) {
		E("%s: config size error[%d, %d]!!\n", __func__,
			cfg_sz, DSRAM_SIZE);
		ret = LENGTH_FAIL;
		goto ALOC_CFG_BUF_FAIL;
	}

	cfg_buf = kcalloc(cfg_sz, sizeof(uint8_t), GFP_KERNEL);
	if (cfg_buf == NULL) {
		E("%s: memory allocation fail[cfg_buf]!!\n", __func__);
		ret = 1;
		goto ALOC_CFG_BUF_FAIL;
	}

	for (i = 1; i < pnum; i++) {

#if defined(HX_ALG_OVERLAY) || defined(HX_CODE_OVERLAY)
		/* overlay section */
		if (allovlidx & (1<<i)) {
			I("%s: skip overlay section %d\n", __func__, i);
			continue;
		}
#endif
		memcpy(&cfg_buf[info[i].cfg_addr - dsram_base],
			&fw->data[info[i].fw_addr], info[i].write_size);
	}

	/* 4. write to sram */
	/* FW entity */
	if (himax_sram_write_crc_check(info[0].sram_addr,
	&fw->data[info[0].fw_addr], info[0].write_size) != 0) {
		E("%s: HW CRC FAIL\n", __func__);
		ret = 2;
		goto BURN_SRAM_FAIL;
	}

	cfg_crc_sw = g_core_fp.fp_Calculate_CRC_with_AP(cfg_buf, 0, cfg_sz);
	do {
		g_core_fp.fp_write_sram_0f(sram_min, cfg_buf, cfg_sz);
		cfg_crc_hw = g_core_fp.fp_check_CRC(sram_min, cfg_sz);
		if (cfg_crc_hw != cfg_crc_sw) {
			E("Cfg CRC FAIL,HWCRC=%X,SWCRC=%X,retry=%d\n",
				cfg_crc_hw, cfg_crc_sw, retry);
		}
	} while (cfg_crc_hw != cfg_crc_sw && retry++ < 3);

	if (retry > 3) {
		ret = 2;
		goto BURN_SRAM_FAIL;
	}

	/*write back system config*/
	if (type == 0) {
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) \
	|| defined(HX_USB_DETECT_GLOBAL)
		g_core_fp.fp_resend_cmd_func(private_ts->suspended);
#endif
	}

#if defined(HX_ALG_OVERLAY)
	// clear handshaking to 0xA55A5AA5
	retry = 0;
	do {
		g_core_fp.fp_register_write(tmp_addr, alg_sdata, DATA_LEN_4);
		usleep_range(1000, 1100);
		g_core_fp.fp_register_read(tmp_addr, rdata, DATA_LEN_4);
	} while ((rdata[0] != alg_sdata[0]
	|| rdata[1] != alg_sdata[1]
	|| rdata[2] != alg_sdata[2]
	|| rdata[3] != alg_sdata[3])
	&& retry++ < HIMAX_REG_RETRY_TIMES);

	if (retry > HIMAX_REG_RETRY_TIMES) {
		E("%s: init handshaking data FAIL[%02X%02X%02X%02X]!!\n",
			__func__, rdata[0], rdata[1], rdata[2], rdata[3]);
	}

	alg_sdata[3] = ovl_alg_reply;
	alg_sdata[2] = ovl_alg_reply;
	alg_sdata[1] = ovl_alg_reply;
	alg_sdata[0] = ovl_alg_reply;

	g_core_fp.fp_reload_disable(0);

	/*g_core_fp.fp_power_on_init();*/
	/*Rawout Sel initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);
	/* reset N frame back to default for normal mode */
	g_core_fp.fp_register_write(pfw_op->addr_set_frame_addr, data, 4);
	/*FW reload done initial*/
	g_core_fp.fp_register_write(pdriver_op->addr_fw_define_2nd_flash_reload,
		pfw_op->data_clear, sizeof(pfw_op->data_clear));

	g_core_fp.fp_sense_on(0x00);

	retry = 0;
	do {
		usleep_range(3000, 3100);
		g_core_fp.fp_register_read(tmp_addr, rdata, DATA_LEN_4);
	} while ((rdata[0] != ovl_alg_request
	|| rdata[1] != ovl_alg_request
	|| rdata[2] != ovl_alg_request
	|| rdata[3] != ovl_alg_request)
	&& retry++ < 30);

	if (retry > 30) {
		E("%s: fail req data = 0x%02X%02X%02X%02X\n", __func__,
			rdata[0], rdata[1], rdata[2], rdata[3]);
		/* monitor FW status for debug */
		for (i = 0; i < 10; i++) {
			usleep_range(10000, 10100);
			g_core_fp.fp_register_read(tmp_addr, rdata, DATA_LEN_4);
			I("%s: req data = 0x%02X%02X%02X%02X\n",
				__func__, rdata[0], rdata[1], rdata[2],
				rdata[3]);
			g_core_fp.fp_read_FW_status();
		}
		ret = 3;
		goto BURN_OVL_FAIL;
	}

	I("%s: upgrade alg overlay section[%d]\n", __func__, alg_idx_t);

	if (himax_sram_write_crc_check(info[alg_idx_t].sram_addr,
	&fw->data[info[alg_idx_t].fw_addr], info[alg_idx_t].write_size) != 0) {
		E("%s: Alg Overlay HW CRC FAIL\n", __func__);
		ret = 2;
	}
#endif

#if defined(HX_CODE_OVERLAY)
if (has_code_overlay) {
	I("%s: upgrade code overlay section[%d]\n", __func__, code_idx_t);

	if (himax_sram_write_crc_check(info[code_idx_t].sram_addr,
	&fw->data[info[code_idx_t].fw_addr],
	info[code_idx_t].write_size) != 0) {
		E("%s: code overlay HW CRC FAIL\n", __func__);
		code_sdata[0] = ovl_fault;
		ret = 2;
	}

	retry = 0;
	do {
		g_core_fp.fp_register_write(tmp_addr, code_sdata, DATA_LEN_4);
		usleep_range(1000, 1100);
		g_core_fp.fp_register_read(tmp_addr, rdata, DATA_LEN_4);
		retry++;
	} while ((code_sdata[3] != rdata[3]
	|| code_sdata[2] != rdata[2]
	|| code_sdata[1] != rdata[1]
	|| code_sdata[0] != rdata[0])
	&& retry < HIMAX_REG_RETRY_TIMES);

	if (retry >= HIMAX_REG_RETRY_TIMES) {
		E("%s: fail code rpl data = 0x%02X%02X%02X%02X\n",
		__func__, rdata[0], rdata[1], rdata[2], rdata[3]);
	}
}
#endif

#if defined(HX_ALG_OVERLAY)
	retry = 0;
	do {
		g_core_fp.fp_register_write(tmp_addr, alg_sdata, DATA_LEN_4);
		usleep_range(1000, 1100);
		g_core_fp.fp_register_read(tmp_addr, rdata, DATA_LEN_4);
	} while ((alg_sdata[3] != rdata[3]
	|| alg_sdata[2] != rdata[2]
	|| alg_sdata[1] != rdata[1]
	|| alg_sdata[0] != rdata[0])
	&& retry++ < HIMAX_REG_RETRY_TIMES);

	if (retry > HIMAX_REG_RETRY_TIMES) {
		E("%s: fail rpl data = 0x%02X%02X%02X%02X\n", __func__,
			rdata[0], rdata[1], rdata[2], rdata[3]);
		// maybe need to reset
	} else {
		I("%s: waiting for FW reload data", __func__);

		retry = 0;
		while (retry++ < 30) {
			g_core_fp.fp_register_read(
				pdriver_op->addr_fw_define_2nd_flash_reload,
				data, DATA_LEN_4);

			/* use all 4 bytes to compare */
			if ((data[3] == 0x00 && data[2] == 0x00 &&
			data[1] == 0x72 && data[0] == 0xC0)) {
				I("%s: FW reload done\n", __func__);
					break;
			}
			I("%s: wait FW reload %d times\n", __func__, retry);
			g_core_fp.fp_read_FW_status();
			usleep_range(10000, 11000);
		}
	}
#endif
#if defined(HX_ALG_OVERLAY)
BURN_OVL_FAIL:
#endif
BURN_SRAM_FAIL:
	kfree(cfg_buf);
ALOC_CFG_BUF_FAIL:
	kfree(info);

	return ret;
/* ret = 1, memory allocation fail
 *     = 2, crc fail
 *     = 3, flow control error
 */
}

int himax_mcu_firmware_update_0f(const struct firmware *fw, int type)
{
	int ret = 0;

	I("%s,Entering - total FW size=%d\n", __func__, (int)fw->size);

	g_core_fp.fp_register_write(pzf_op->addr_system_reset,
		pzf_op->data_system_reset, 4);

	g_core_fp.fp_sense_off(false);

	if ((int)fw->size > HX64K) {
		ret = himax_zf_part_info(fw, type);
	} else {
		/* first 48K */
		ret = himax_sram_write_crc_check(pzf_op->data_sram_start_addr,
			&fw->data[0], HX_48K_SZ);
		if (ret != 0)
			E("%s, HW CRC FAIL - Main SRAM 48K\n", __func__);

		/*config info*/
		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(pzf_op->data_cfg_info,
				&fw->data[0xC000], 128);
			if (ret != 0)
				E("Config info CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_cfg_info,
				128, 2);
		}

		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(pzf_op->data_fw_cfg_1,
				&fw->data[0xC0FE], 528);
			if (ret != 0)
				E("FW config 1 CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_fw_cfg_1,
				528, 1);
		}

		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(pzf_op->data_fw_cfg_3,
				&fw->data[0xCA00], 128);
			if (ret != 0)
				E("FW config 3 CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_fw_cfg_3,
				128, 2);
		}

		/*ADC config*/
		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(pzf_op->data_adc_cfg_1,
				&fw->data[0xD640], 1200);
			if (ret != 0)
				E("ADC config 1 CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_1,
				1200, 2);
		}

		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(pzf_op->data_adc_cfg_2,
				&fw->data[0xD320], 800);
			if (ret != 0)
				E("ADC config 2 CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_2,
				800, 2);
		}

		/*mapping table*/
		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(pzf_op->data_map_table,
				&fw->data[0xE000], 1536);
			if (ret != 0)
				E("Mapping table CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_map_table,
				1536, 2);
		}
	}

	I("%s, End\n", __func__);

	return ret;
}

int hx_0f_op_file_dirly(char *file_name)
{
	const struct firmware *fw = NULL;
	int reqret = -1;
	int ret = -1;
	int type = 0; /* FW type: 0, normal; 1, MPAP */
	char mapa_name[HIMAX_FIRMWARE_LINE] = { 0 };

	if (g_f_0f_updat == 1) {
		W("%s: Other thread is updating now!\n", __func__);
		return ret;
	}
	g_f_0f_updat = 1;
	I("%s: Preparing to update %s!\n", __func__, file_name);

	reqret = request_firmware(&fw, file_name, private_ts->dev);
	if (reqret < 0) {
#if defined(__EMBEDDED_FW__)
		fw = &g_embedded_fw;
		I("%s: Not find FW in userspace, use embedded FW(size:%zu)\n",
			__func__, g_embedded_fw.size);
#else
		ret = reqret;
		E("%s: request firmware fail, code[%d]!!\n", __func__, ret);
		goto END;
#endif
	}

	snprintf(mapa_name, sizeof(mapa_name), "%s%s.bin",
		 MPAP_FWNAME, private_ts->panel_name);
	if (strcmp(file_name, (const char *)mapa_name) == 0)
		type = 1;

	ret = g_core_fp.fp_firmware_update_0f(fw, type);

	if (reqret >= 0)
		release_firmware(fw);

	if (ret < 0)
		goto END;

#if !defined(HX_ALG_OVERLAY)
	if (type == 1)
		g_core_fp.fp_turn_on_mp_func(1);
	else
		g_core_fp.fp_turn_on_mp_func(0);
	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_power_on_init();
#endif

END:
	g_f_0f_updat = 0;

	I("%s: END\n", __func__);
	return ret;
}

static int himax_mcu_0f_excp_check(void)
{
	return NO_ERR;
}

#if defined(HX_0F_DEBUG)
void himax_mcu_read_sram_0f(const struct firmware *fw_entry,
		uint8_t *addr, int start_index, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0, j = 0;
	int not_same = 0;

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data = NULL;
	int *not_same_buff = NULL;

	I("%s, Entering\n", __func__);

	/*g_core_fp.fp_burst_enable(1);*/

	total_size = read_len;

	total_size_temp = read_len;

#if defined(HX_SPI_OPERATION)
	if (read_len > 2048)
		max_bus_size = 2048;
	else
		max_bus_size = read_len;
#else
	if (read_len > 240)
		max_bus_size = 240;
	else
		max_bus_size = read_len;
#endif

	if (total_size % max_bus_size == 0)
		total_read_times = total_size / max_bus_size;
	else
		total_read_times = total_size / max_bus_size + 1;

	I("%s, total size=%d, bus size=%d, read time=%d\n",
		__func__,
		total_size,
		max_bus_size,
		total_read_times);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s,addr[3]=0x%2X,addr[2]=0x%2X,addr[1]=0x%2X,addr[0]=0x%2X\n",
		__func__,
		tmp_addr[3],
		tmp_addr[2],
		tmp_addr[1],
		tmp_addr[0]);

	temp_info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (temp_info_data == NULL) {
		E("%s, Failed to allocate temp_info_data\n", __func__);
		goto err_malloc_temp_info_data;
	}

	not_same_buff = kcalloc(total_size, sizeof(int), GFP_KERNEL);
	if (not_same_buff == NULL) {
		E("%s, Failed to allocate not_same_buff\n", __func__);
		goto err_malloc_not_same_buff;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_register_read(tmp_addr,
				&temp_info_data[i*max_bus_size], max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			g_core_fp.fp_register_read(tmp_addr,
				&temp_info_data[i*max_bus_size],
				total_size_temp % max_bus_size);
		}

		address = ((i+1) * max_bus_size);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);
		if (tmp_addr[0] < addr[0])
			tmp_addr[1] = addr[1]
				+ (uint8_t) ((address>>8) & 0x00FF) + 1;
		else
			tmp_addr[1] = addr[1]
				+ (uint8_t) ((address>>8) & 0x00FF);

		/*msleep (10);*/
	}
	I("%s,READ Start, start_index = %d\n", __func__, start_index);

	j = start_index;
	for (i = 0; i < read_len; i++, j++) {
		if (fw_entry->data[j] != temp_info_data[i]) {
			not_same++;
			not_same_buff[i] = 1;
		}

		I("0x%2.2X, ", temp_info_data[i]);

		if (i > 0 && i%16 == 15)
			pr_info("\n");

	}
	I("%s,READ END,Not Same count=%d\n", __func__, not_same);

	if (not_same != 0) {
		j = start_index;
		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1)
				I("bin=[%d] 0x%2.2X\n", i, fw_entry->data[j]);
		}
		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1)
				I("sram=[%d] 0x%2.2X\n", i, temp_info_data[i]);
		}
	}
	I("%s,READ END, Not Same count=%d\n", __func__, not_same);

	kfree(not_same_buff);
err_malloc_not_same_buff:
	kfree(temp_info_data);
err_malloc_temp_info_data:
	return;
}

void himax_mcu_read_all_sram(uint8_t *addr, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;
	/* struct file *fn; */
	/* struct filename *vts_name;	*/

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data;

	I("%s, Entering\n", __func__);

	/*g_core_fp.fp_burst_enable(1);*/

	total_size = read_len;

	total_size_temp = read_len;

	if (total_size % max_bus_size == 0)
		total_read_times = total_size / max_bus_size;
	else
		total_read_times = total_size / max_bus_size + 1;

	I("%s, total size=%d\n", __func__, total_size);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s:addr[3]=0x%2X,addr[2]=0x%2X,addr[1]=0x%2X,addr[0]=0x%2X\n",
		__func__,
		tmp_addr[3],
		tmp_addr[2],
		tmp_addr[1],
		tmp_addr[0]);

	temp_info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (temp_info_data == NULL) {
		E("%s, Failed to allocate temp_info_data\n", __func__);
		return;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_register_read(tmp_addr,
				&temp_info_data[i*max_bus_size], max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			g_core_fp.fp_register_read(tmp_addr,
				&temp_info_data[i*max_bus_size],
				total_size_temp % max_bus_size);
		}

		address = ((i+1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		/*msleep (10);*/
	}
	I("%s,addr[3]=0x%2X,addr[2]=0x%2X,addr[1]=0x%2X,addr[0]=0x%2X\n",
		__func__,
		tmp_addr[3],
		tmp_addr[2],
		tmp_addr[1],
		tmp_addr[0]);
	/*for(i = 0;i<read_len;i++)
	 *{
	 *	I("0x%2.2X, ", temp_info_data[i]);
	 *
	 *	if (i > 0 && i%16 == 15)
	 *	
	 *}
	 */

	/* need modify
	 * I("Now Write File start!\n");
	 * vts_name = kp_getname_kernel("/sdcard/dump_dsram.txt");
	 * fn = kp_file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
	 * if (!IS_ERR (fn)) {
	 * I("%s create file and ready to write\n", __func__);
	 * fn->f_op->write(fn, temp_info_data, read_len*sizeof (uint8_t),
	 *	&fn->f_pos);
	 * I("Error: filpp_close (fn, NULL);\n");
	 * }
	 * I("Now Write File End!\n");
	 */

	kfree(temp_info_data);

	I("%s, END\n", __func__);

}

void himax_mcu_firmware_read_0f(const struct firmware *fw_entry, int type)
{
	uint8_t tmp_addr[4];

	I("%s, Entering\n", __func__);
	if (type == 0) { /* first 48K */
		g_core_fp.fp_read_sram_0f(fw_entry,
			pzf_op->data_sram_start_addr,
			0,
			HX_48K_SZ);
		g_core_fp.fp_read_all_sram(tmp_addr, 0xC000);
	} else { /*last 16k*/
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_cfg_info,
			0xC000, 132);

		/*FW config*/
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_fw_cfg_1,
			0xC0FE, 484);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_fw_cfg_2,
			0xC9DE, 36);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_fw_cfg_3,
			0xCA00, 72);

		/*ADC config*/

		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_adc_cfg_1,
			0xD630, 1188);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_adc_cfg_2,
			0xD318, 792);


		/*mapping table*/
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_map_table,
			0xE000, 1536);

		/* set n frame=0*/
		g_core_fp.fp_read_sram_0f(fw_entry, pfw_op->addr_set_frame_addr,
			0xC30C, 4);
	}

	I("%s, END\n", __func__);
}
#endif


/*
 *#if defined(HX_CODE_OVERLAY)
 *int himax_mcu_0f_overlay(int ovl_type, int mode)
 *{
 *return NO_ERR;
 *}
 *#endif
 */

#endif

/* CORE_INIT */
/* init start */
static void himax_mcu_fp_init(void)
{
/* CORE_IC */
	g_core_fp.fp_burst_enable = himax_mcu_burst_enable;
	g_core_fp.fp_register_read = himax_mcu_register_read;
	/*
	 * g_core_fp.fp_flash_write_burst = himax_mcu_flash_write_burst;
	 */
	/*
	 * g_core_fp.fp_flash_write_burst_lenth =
	 *	himax_mcu_flash_write_burst_lenth;
	 */
	g_core_fp.fp_register_write = himax_mcu_register_write;
	g_core_fp.fp_interface_on = himax_mcu_interface_on;
	g_core_fp.fp_sense_on = himax_mcu_sense_on;
	g_core_fp.fp_sense_off = himax_mcu_sense_off;
	g_core_fp.fp_wait_wip = himax_mcu_wait_wip;
	g_core_fp.fp_init_psl = himax_mcu_init_psl;
	g_core_fp.fp_resume_ic_action = himax_mcu_resume_ic_action;
	g_core_fp.fp_suspend_ic_action = himax_mcu_suspend_ic_action;
	g_core_fp.fp_power_on_init = himax_mcu_power_on_init;
/* CORE_IC */
/* CORE_FW */
	g_core_fp.fp_system_reset = himax_mcu_system_reset;
	g_core_fp.fp_Calculate_CRC_with_AP = himax_mcu_Calculate_CRC_with_AP;
	g_core_fp.fp_check_CRC = himax_mcu_check_CRC;
	g_core_fp.fp_set_reload_cmd = himax_mcu_set_reload_cmd;
	g_core_fp.fp_program_reload = himax_mcu_program_reload;
#if defined(HX_ULTRA_LOW_POWER)
	g_core_fp.fp_ulpm_in = himax_mcu_ulpm_in;
	g_core_fp.fp_black_gest_ctrl = himax_mcu_black_gest_ctrl;
#endif
	g_core_fp.fp_set_SMWP_enable = himax_mcu_set_SMWP_enable;
	g_core_fp.fp_set_HSEN_enable = himax_mcu_set_HSEN_enable;
	g_core_fp.fp_usb_detect_set = himax_mcu_usb_detect_set;
	g_core_fp.fp_diag_register_set = himax_mcu_diag_register_set;
	g_core_fp.fp_chip_self_test = himax_mcu_chip_self_test;
	g_core_fp.fp_idle_mode = himax_mcu_idle_mode;
	g_core_fp.fp_reload_disable = himax_mcu_reload_disable;
	g_core_fp.fp_read_ic_trigger_type = himax_mcu_read_ic_trigger_type;
	g_core_fp.fp_read_i2c_status = himax_mcu_read_i2c_status;
	g_core_fp.fp_read_FW_ver = himax_mcu_read_FW_ver;
	g_core_fp.fp_read_event_stack = himax_mcu_read_event_stack;
	g_core_fp.fp_return_event_stack = himax_mcu_return_event_stack;
	g_core_fp.fp_calculateChecksum = himax_mcu_calculateChecksum;
	g_core_fp.fp_read_FW_status = himax_mcu_read_FW_status;
	g_core_fp.fp_irq_switch = himax_mcu_irq_switch;
	g_core_fp.fp_assign_sorting_mode = himax_mcu_assign_sorting_mode;
	g_core_fp.fp_check_sorting_mode = himax_mcu_check_sorting_mode;
	g_core_fp.fp_read_DD_status = himax_mcu_read_DD_status;
	g_core_fp._clr_fw_reord_dd_sts = hx_clr_fw_reord_dd_sts;
	g_core_fp._ap_notify_fw_sus = hx_ap_notify_fw_sus;
/* CORE_FW */
/* CORE_FLASH */
	g_core_fp.fp_chip_erase = himax_mcu_chip_erase;
	g_core_fp.fp_block_erase = himax_mcu_block_erase;
	g_core_fp.fp_sector_erase = himax_mcu_sector_erase;
	g_core_fp.fp_flash_programming = himax_mcu_flash_programming;
	g_core_fp.fp_flash_page_write = himax_mcu_flash_page_write;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_32k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_60k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_124k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_255k =
			himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_255k;
	g_core_fp.fp_flash_dump_func = himax_mcu_flash_dump_func;
	g_core_fp.fp_flash_lastdata_check = himax_mcu_flash_lastdata_check;
	g_core_fp.fp_bin_desc_get = hx_mcu_bin_desc_get;
	g_core_fp._diff_overlay_flash = hx_mcu_diff_overlay_flash;
/* CORE_FLASH */
/* CORE_SRAM */
	g_core_fp.fp_sram_write = himax_mcu_sram_write;
	g_core_fp.fp_sram_verify = himax_mcu_sram_verify;
	g_core_fp.fp_get_DSRAM_data = himax_mcu_get_DSRAM_data;
/* CORE_SRAM */
/* CORE_DRIVER */
	g_core_fp.fp_chip_init = himax_mcu_init_ic;
#if defined(HX_RST_PIN_FUNC)
	g_core_fp.fp_pin_reset = himax_mcu_pin_reset;
	g_core_fp.fp_ic_reset = himax_mcu_ic_reset;
#endif
	g_core_fp.fp_tp_info_check = himax_mcu_tp_info_check;
	g_core_fp.fp_touch_information = himax_mcu_touch_information;
	g_core_fp.fp_calc_touch_data_size = himax_mcu_calcTouchDataSize;
	/*g_core_fp.fp_reload_config = himax_mcu_reload_config;*/
	g_core_fp.fp_get_touch_data_size = himax_mcu_get_touch_data_size;
	g_core_fp.fp_hand_shaking = himax_mcu_hand_shaking;
	g_core_fp.fp_determin_diag_rawdata = himax_mcu_determin_diag_rawdata;
	g_core_fp.fp_determin_diag_storage = himax_mcu_determin_diag_storage;
	g_core_fp.fp_cal_data_len = himax_mcu_cal_data_len;
	g_core_fp.fp_diag_check_sum = himax_mcu_diag_check_sum;
	g_core_fp.fp_diag_parse_raw_data = himax_mcu_diag_parse_raw_data;
#if defined(HX_EXCP_RECOVERY)
	g_core_fp.fp_ic_excp_recovery = himax_mcu_ic_excp_recovery;
	g_core_fp.fp_excp_ic_reset = himax_mcu_excp_ic_reset;
#endif

	g_core_fp.fp_resend_cmd_func = himax_mcu_resend_cmd_func;

#if defined(HX_TP_PROC_GUEST_INFO)
	g_core_fp.guest_info_get_status = himax_guest_info_get_status;
	g_core_fp.read_guest_info = hx_read_guest_info;
#endif
/* CORE_DRIVER */
	g_core_fp.fp_turn_on_mp_func = hx_turn_on_mp_func;
#if defined(HX_ZERO_FLASH)
	g_core_fp.fp_reload_disable = hx_dis_rload_0f;
	g_core_fp.fp_clean_sram_0f = himax_mcu_clean_sram_0f;
	g_core_fp.fp_write_sram_0f = himax_mcu_write_sram_0f;
	g_core_fp.fp_write_sram_0f_crc = himax_sram_write_crc_check;
	g_core_fp.fp_firmware_update_0f = himax_mcu_firmware_update_0f;
	g_core_fp.fp_0f_op_file_dirly = hx_0f_op_file_dirly;
	g_core_fp.fp_0f_excp_check = himax_mcu_0f_excp_check;
#if defined(HX_0F_DEBUG)
	g_core_fp.fp_read_sram_0f = himax_mcu_read_sram_0f;
	g_core_fp.fp_read_all_sram = himax_mcu_read_all_sram;
	g_core_fp.fp_firmware_read_0f = himax_mcu_firmware_read_0f;
#endif
/*
 * #if defined(HX_CODE_OVERLAY)
 *	g_core_fp.fp_0f_overlay = himax_mcu_0f_overlay;
 * #endif
 */
#endif
	g_core_fp.fp_suspend_proc = himax_suspend_proc;
	g_core_fp.fp_resume_proc = himax_resume_proc;
}

void himax_mcu_in_cmd_struct_free(void)
{
	pic_op = NULL;
	pfw_op = NULL;
	pflash_op = NULL;
	psram_op = NULL;
	pdriver_op = NULL;
	kfree(g_internal_buffer);
	g_internal_buffer = NULL;
#if defined(HX_ZERO_FLASH)
	kfree(g_core_cmd_op->zf_op);
	g_core_cmd_op->zf_op = NULL;
#endif
	kfree(g_core_cmd_op->driver_op);
	g_core_cmd_op->driver_op = NULL;
	kfree(g_core_cmd_op->sram_op);
	g_core_cmd_op->sram_op = NULL;
	kfree(g_core_cmd_op->flash_op);
	g_core_cmd_op->flash_op = NULL;
	kfree(g_core_cmd_op->fw_op);
	g_core_cmd_op->fw_op = NULL;
	kfree(g_core_cmd_op->ic_op);
	g_core_cmd_op->ic_op = NULL;
	kfree(g_core_cmd_op);
	g_core_cmd_op = NULL;

	I("%s: release completed\n", __func__);
}

int himax_mcu_in_cmd_struct_init(void)
{
	int err = 0;

	I("%s: Entering!\n", __func__);
	himax_mcu_cmd_struct_free = himax_mcu_in_cmd_struct_free;

	g_core_cmd_op = kzalloc(sizeof(struct himax_core_command_operation),
			GFP_KERNEL);
	if (g_core_cmd_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_fail;
	}

	g_core_cmd_op->ic_op = kzalloc(sizeof(struct ic_operation), GFP_KERNEL);
	if (g_core_cmd_op->ic_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_ic_op_fail;
	}

	g_core_cmd_op->fw_op = kzalloc(sizeof(struct fw_operation), GFP_KERNEL);
	if (g_core_cmd_op->fw_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_fw_op_fail;
	}

	g_core_cmd_op->flash_op = kzalloc(sizeof(struct flash_operation),
			GFP_KERNEL);
	if (g_core_cmd_op->flash_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_flash_op_fail;
	}

	g_core_cmd_op->sram_op = kzalloc(sizeof(struct sram_operation),
			GFP_KERNEL);
	if (g_core_cmd_op->sram_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_sram_op_fail;
	}

	g_core_cmd_op->driver_op = kzalloc(sizeof(struct driver_operation),
			GFP_KERNEL);
	if (g_core_cmd_op->driver_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_driver_op_fail;
	}

	pic_op = g_core_cmd_op->ic_op;
	pfw_op = g_core_cmd_op->fw_op;
	pflash_op = g_core_cmd_op->flash_op;
	psram_op = g_core_cmd_op->sram_op;
	pdriver_op = g_core_cmd_op->driver_op;
#if defined(HX_ZERO_FLASH)
	g_core_cmd_op->zf_op = kzalloc(sizeof(struct zf_operation),
		GFP_KERNEL);
	if (g_core_cmd_op->zf_op == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_zf_op_fail;
	}
	pzf_op = g_core_cmd_op->zf_op;

#endif

	himax_mcu_fp_init();

	return NO_ERR;

#if defined(HX_ZERO_FLASH)
err_g_core_cmd_op_zf_op_fail:
#endif
	kfree(g_core_cmd_op->driver_op);
	g_core_cmd_op->driver_op = NULL;
err_g_core_cmd_op_driver_op_fail:
	kfree(g_core_cmd_op->sram_op);
	g_core_cmd_op->sram_op = NULL;
err_g_core_cmd_op_sram_op_fail:
	kfree(g_core_cmd_op->flash_op);
	g_core_cmd_op->flash_op = NULL;
err_g_core_cmd_op_flash_op_fail:
	kfree(g_core_cmd_op->fw_op);
	g_core_cmd_op->fw_op = NULL;
err_g_core_cmd_op_fw_op_fail:
	kfree(g_core_cmd_op->ic_op);
	g_core_cmd_op->ic_op = NULL;
err_g_core_cmd_op_ic_op_fail:
	kfree(g_core_cmd_op);
	g_core_cmd_op = NULL;
err_g_core_cmd_op_fail:

	return err;
}
EXPORT_SYMBOL(himax_mcu_in_cmd_struct_init);

void himax_mcu_in_cmd_init(void)
{
	I("%s: Entering!\n", __func__);
/* CORE_IC */
	himax_parse_assign_cmd(ic_adr_ahb_addr_byte_0,
		pic_op->addr_ahb_addr_byte_0,
		sizeof(pic_op->addr_ahb_addr_byte_0));
	himax_parse_assign_cmd(ic_adr_ahb_rdata_byte_0,
		pic_op->addr_ahb_rdata_byte_0,
		sizeof(pic_op->addr_ahb_rdata_byte_0));
	himax_parse_assign_cmd(ic_adr_ahb_access_direction,
		pic_op->addr_ahb_access_direction,
		sizeof(pic_op->addr_ahb_access_direction));
	himax_parse_assign_cmd(ic_adr_conti, pic_op->addr_conti,
		sizeof(pic_op->addr_conti));
	himax_parse_assign_cmd(ic_adr_incr4, pic_op->addr_incr4,
		sizeof(pic_op->addr_incr4));
	himax_parse_assign_cmd(ic_adr_i2c_psw_lb, pic_op->adr_i2c_psw_lb,
		sizeof(pic_op->adr_i2c_psw_lb));
	himax_parse_assign_cmd(ic_adr_i2c_psw_ub, pic_op->adr_i2c_psw_ub,
		sizeof(pic_op->adr_i2c_psw_ub));
	himax_parse_assign_cmd(ic_cmd_ahb_access_direction_read,
		pic_op->data_ahb_access_direction_read,
		sizeof(pic_op->data_ahb_access_direction_read));
	himax_parse_assign_cmd(ic_cmd_conti, pic_op->data_conti,
		sizeof(pic_op->data_conti));
	himax_parse_assign_cmd(ic_cmd_incr4, pic_op->data_incr4,
		sizeof(pic_op->data_incr4));
	himax_parse_assign_cmd(ic_cmd_i2c_psw_lb, pic_op->data_i2c_psw_lb,
		sizeof(pic_op->data_i2c_psw_lb));
	himax_parse_assign_cmd(ic_cmd_i2c_psw_ub, pic_op->data_i2c_psw_ub,
		sizeof(pic_op->data_i2c_psw_ub));
	himax_parse_assign_cmd(ic_adr_tcon_on_rst, pic_op->addr_tcon_on_rst,
		sizeof(pic_op->addr_tcon_on_rst));
	himax_parse_assign_cmd(ic_addr_adc_on_rst, pic_op->addr_adc_on_rst,
		sizeof(pic_op->addr_adc_on_rst));
	himax_parse_assign_cmd(ic_adr_psl, pic_op->addr_psl,
		sizeof(pic_op->addr_psl));
	himax_parse_assign_cmd(ic_adr_cs_central_state,
		pic_op->addr_cs_central_state,
		sizeof(pic_op->addr_cs_central_state));
	himax_parse_assign_cmd(ic_cmd_rst, pic_op->data_rst,
		sizeof(pic_op->data_rst));
	himax_parse_assign_cmd(ic_adr_osc_en, pic_op->adr_osc_en,
		sizeof(pic_op->adr_osc_en));
	himax_parse_assign_cmd(ic_adr_osc_pw, pic_op->adr_osc_pw,
		sizeof(pic_op->adr_osc_pw));
/* CORE_IC */
/* CORE_FW */
	himax_parse_assign_cmd(fw_addr_system_reset,
		pfw_op->addr_system_reset,
		sizeof(pfw_op->addr_system_reset));
	himax_parse_assign_cmd(fw_addr_ctrl_fw,
		pfw_op->addr_ctrl_fw_isr,
		sizeof(pfw_op->addr_ctrl_fw_isr));
	himax_parse_assign_cmd(fw_addr_flag_reset_event,
		pfw_op->addr_flag_reset_event,
		sizeof(pfw_op->addr_flag_reset_event));
	himax_parse_assign_cmd(fw_addr_hsen_enable,
		pfw_op->addr_hsen_enable,
		sizeof(pfw_op->addr_hsen_enable));
	himax_parse_assign_cmd(fw_addr_smwp_enable,
		pfw_op->addr_smwp_enable,
		sizeof(pfw_op->addr_smwp_enable));
	himax_parse_assign_cmd(fw_addr_program_reload_from,
		pfw_op->addr_program_reload_from,
		sizeof(pfw_op->addr_program_reload_from));
	himax_parse_assign_cmd(fw_addr_program_reload_to,
		pfw_op->addr_program_reload_to,
		sizeof(pfw_op->addr_program_reload_to));
	himax_parse_assign_cmd(fw_addr_program_reload_page_write,
		pfw_op->addr_program_reload_page_write,
		sizeof(pfw_op->addr_program_reload_page_write));
	himax_parse_assign_cmd(fw_addr_raw_out_sel,
		pfw_op->addr_raw_out_sel,
		sizeof(pfw_op->addr_raw_out_sel));
	himax_parse_assign_cmd(fw_addr_reload_status,
		pfw_op->addr_reload_status,
		sizeof(pfw_op->addr_reload_status));
	himax_parse_assign_cmd(fw_addr_reload_crc32_result,
		pfw_op->addr_reload_crc32_result,
		sizeof(pfw_op->addr_reload_crc32_result));
	himax_parse_assign_cmd(fw_addr_reload_addr_from,
		pfw_op->addr_reload_addr_from,
		sizeof(pfw_op->addr_reload_addr_from));
	himax_parse_assign_cmd(fw_addr_reload_addr_cmd_beat,
		pfw_op->addr_reload_addr_cmd_beat,
		sizeof(pfw_op->addr_reload_addr_cmd_beat));
	himax_parse_assign_cmd(fw_addr_selftest_addr_en,
		pfw_op->addr_selftest_addr_en,
		sizeof(pfw_op->addr_selftest_addr_en));
	himax_parse_assign_cmd(fw_addr_criteria_addr,
		pfw_op->addr_criteria_addr,
		sizeof(pfw_op->addr_criteria_addr));
	himax_parse_assign_cmd(fw_addr_set_frame_addr,
		pfw_op->addr_set_frame_addr,
		sizeof(pfw_op->addr_set_frame_addr));
	himax_parse_assign_cmd(fw_addr_selftest_result_addr,
		pfw_op->addr_selftest_result_addr,
		sizeof(pfw_op->addr_selftest_result_addr));
	himax_parse_assign_cmd(fw_addr_sorting_mode_en,
		pfw_op->addr_sorting_mode_en,
		sizeof(pfw_op->addr_sorting_mode_en));
	himax_parse_assign_cmd(fw_addr_fw_mode_status,
		pfw_op->addr_fw_mode_status,
		sizeof(pfw_op->addr_fw_mode_status));
	himax_parse_assign_cmd(fw_addr_icid_addr,
		pfw_op->addr_icid_addr,
		sizeof(pfw_op->addr_icid_addr));
	himax_parse_assign_cmd(fw_addr_fw_ver_addr,
		pfw_op->addr_fw_ver_addr,
		sizeof(pfw_op->addr_fw_ver_addr));
	himax_parse_assign_cmd(fw_addr_fw_cfg_addr,
		pfw_op->addr_fw_cfg_addr,
		sizeof(pfw_op->addr_fw_cfg_addr));
	himax_parse_assign_cmd(fw_addr_fw_vendor_addr,
		pfw_op->addr_fw_vendor_addr,
		sizeof(pfw_op->addr_fw_vendor_addr));
	himax_parse_assign_cmd(fw_addr_cus_info,
		pfw_op->addr_cus_info,
		sizeof(pfw_op->addr_cus_info));
	himax_parse_assign_cmd(fw_addr_proj_info,
		pfw_op->addr_proj_info,
		sizeof(pfw_op->addr_proj_info));
	himax_parse_assign_cmd(fw_addr_fw_state_addr,
		pfw_op->addr_fw_state_addr,
		sizeof(pfw_op->addr_fw_state_addr));
	himax_parse_assign_cmd(fw_addr_fw_dbg_msg_addr,
		pfw_op->addr_fw_dbg_msg_addr,
		sizeof(pfw_op->addr_fw_dbg_msg_addr));
	himax_parse_assign_cmd(fw_addr_chk_fw_status,
		pfw_op->addr_chk_fw_status,
		sizeof(pfw_op->addr_chk_fw_status));
	himax_parse_assign_cmd(fw_addr_dd_handshak_addr,
		pfw_op->addr_dd_handshak_addr,
		sizeof(pfw_op->addr_dd_handshak_addr));
	himax_parse_assign_cmd(fw_addr_dd_data_addr,
		pfw_op->addr_dd_data_addr,
		sizeof(pfw_op->addr_dd_data_addr));
	himax_parse_assign_cmd(fw_addr_clr_fw_record_dd_sts,
		pfw_op->addr_clr_fw_record_dd_sts,
		sizeof(pfw_op->addr_clr_fw_record_dd_sts));
	himax_parse_assign_cmd(fw_addr_ap_notify_fw_sus,
		pfw_op->addr_ap_notify_fw_sus,
		sizeof(pfw_op->addr_ap_notify_fw_sus));
	himax_parse_assign_cmd(fw_data_ap_notify_fw_sus_en,
		pfw_op->data_ap_notify_fw_sus_en,
		sizeof(pfw_op->data_ap_notify_fw_sus_en));
	himax_parse_assign_cmd(fw_data_ap_notify_fw_sus_dis,
		pfw_op->data_ap_notify_fw_sus_dis,
		sizeof(pfw_op->data_ap_notify_fw_sus_dis));
	himax_parse_assign_cmd(fw_data_system_reset,
		pfw_op->data_system_reset,
		sizeof(pfw_op->data_system_reset));
	himax_parse_assign_cmd(fw_data_safe_mode_release_pw_active,
		pfw_op->data_safe_mode_release_pw_active,
		sizeof(pfw_op->data_safe_mode_release_pw_active));
	himax_parse_assign_cmd(fw_data_clear,
		pfw_op->data_clear,
		sizeof(pfw_op->data_clear));
	himax_parse_assign_cmd(fw_data_clear,
		pfw_op->data_clear,
		sizeof(pfw_op->data_clear));
	himax_parse_assign_cmd(fw_data_fw_stop,
		pfw_op->data_fw_stop,
		sizeof(pfw_op->data_fw_stop));
	himax_parse_assign_cmd(fw_data_safe_mode_release_pw_reset,
		pfw_op->data_safe_mode_release_pw_reset,
		sizeof(pfw_op->data_safe_mode_release_pw_reset));
	himax_parse_assign_cmd(fw_data_program_reload_start,
		pfw_op->data_program_reload_start,
		sizeof(pfw_op->data_program_reload_start));
	himax_parse_assign_cmd(fw_data_program_reload_compare,
		pfw_op->data_program_reload_compare,
		sizeof(pfw_op->data_program_reload_compare));
	himax_parse_assign_cmd(fw_data_program_reload_break,
		pfw_op->data_program_reload_break,
		sizeof(pfw_op->data_program_reload_break));
	himax_parse_assign_cmd(fw_data_selftest_request,
		pfw_op->data_selftest_request,
		sizeof(pfw_op->data_selftest_request));
	himax_parse_assign_cmd(fw_data_criteria_aa_top,
		pfw_op->data_criteria_aa_top,
		sizeof(pfw_op->data_criteria_aa_top));
	himax_parse_assign_cmd(fw_data_criteria_aa_bot,
		pfw_op->data_criteria_aa_bot,
		sizeof(pfw_op->data_criteria_aa_bot));
	himax_parse_assign_cmd(fw_data_criteria_key_top,
		pfw_op->data_criteria_key_top,
		sizeof(pfw_op->data_criteria_key_top));
	himax_parse_assign_cmd(fw_data_criteria_key_bot,
		pfw_op->data_criteria_key_bot,
		sizeof(pfw_op->data_criteria_key_bot));
	himax_parse_assign_cmd(fw_data_criteria_avg_top,
		pfw_op->data_criteria_avg_top,
		sizeof(pfw_op->data_criteria_avg_top));
	himax_parse_assign_cmd(fw_data_criteria_avg_bot,
		pfw_op->data_criteria_avg_bot,
		sizeof(pfw_op->data_criteria_avg_bot));
	himax_parse_assign_cmd(fw_data_set_frame,
		pfw_op->data_set_frame,
		sizeof(pfw_op->data_set_frame));
	himax_parse_assign_cmd(fw_data_selftest_ack_hb,
		pfw_op->data_selftest_ack_hb,
		sizeof(pfw_op->data_selftest_ack_hb));
	himax_parse_assign_cmd(fw_data_selftest_ack_lb,
		pfw_op->data_selftest_ack_lb,
		sizeof(pfw_op->data_selftest_ack_lb));
	himax_parse_assign_cmd(fw_data_selftest_pass,
		pfw_op->data_selftest_pass,
		sizeof(pfw_op->data_selftest_pass));
	himax_parse_assign_cmd(fw_data_normal_cmd,
		pfw_op->data_normal_cmd,
		sizeof(pfw_op->data_normal_cmd));
	himax_parse_assign_cmd(fw_data_normal_status,
		pfw_op->data_normal_status,
		sizeof(pfw_op->data_normal_status));
	himax_parse_assign_cmd(fw_data_sorting_cmd,
		pfw_op->data_sorting_cmd,
		sizeof(pfw_op->data_sorting_cmd));
	himax_parse_assign_cmd(fw_data_sorting_status,
		pfw_op->data_sorting_status,
		sizeof(pfw_op->data_sorting_status));
	himax_parse_assign_cmd(fw_data_dd_request,
		pfw_op->data_dd_request,
		sizeof(pfw_op->data_dd_request));
	himax_parse_assign_cmd(fw_data_dd_ack,
		pfw_op->data_dd_ack,
		sizeof(pfw_op->data_dd_ack));
	himax_parse_assign_cmd(fw_data_idle_dis_pwd,
		pfw_op->data_idle_dis_pwd,
		sizeof(pfw_op->data_idle_dis_pwd));
	himax_parse_assign_cmd(fw_data_idle_en_pwd,
		pfw_op->data_idle_en_pwd,
		sizeof(pfw_op->data_idle_en_pwd));
	himax_parse_assign_cmd(fw_data_rawdata_ready_hb,
		pfw_op->data_rawdata_ready_hb,
		sizeof(pfw_op->data_rawdata_ready_hb));
	himax_parse_assign_cmd(fw_data_rawdata_ready_lb,
		pfw_op->data_rawdata_ready_lb,
		sizeof(pfw_op->data_rawdata_ready_lb));
	himax_parse_assign_cmd(fw_addr_ahb_addr,
		pfw_op->addr_ahb_addr,
		sizeof(pfw_op->addr_ahb_addr));
	himax_parse_assign_cmd(fw_data_ahb_dis,
		pfw_op->data_ahb_dis,
		sizeof(pfw_op->data_ahb_dis));
	himax_parse_assign_cmd(fw_data_ahb_en,
		pfw_op->data_ahb_en,
		sizeof(pfw_op->data_ahb_en));
	himax_parse_assign_cmd(fw_addr_event_addr,
		pfw_op->addr_event_addr,
		sizeof(pfw_op->addr_event_addr));
	himax_parse_assign_cmd(fw_usb_detect_addr,
		pfw_op->addr_usb_detect,
		sizeof(pfw_op->addr_usb_detect));
#if defined(HX_ULTRA_LOW_POWER)
	himax_parse_assign_cmd(fw_addr_ulpm_33, pfw_op->addr_ulpm_33,
		sizeof(pfw_op->addr_ulpm_33));
	himax_parse_assign_cmd(fw_addr_ulpm_34, pfw_op->addr_ulpm_34,
		sizeof(pfw_op->addr_ulpm_34));
	himax_parse_assign_cmd(fw_data_ulpm_11, pfw_op->data_ulpm_11,
		sizeof(pfw_op->data_ulpm_11));
	himax_parse_assign_cmd(fw_data_ulpm_22, pfw_op->data_ulpm_22,
		sizeof(pfw_op->data_ulpm_22));
	himax_parse_assign_cmd(fw_data_ulpm_33, pfw_op->data_ulpm_33,
		sizeof(pfw_op->data_ulpm_33));
	himax_parse_assign_cmd(fw_data_ulpm_aa, pfw_op->data_ulpm_aa,
		sizeof(pfw_op->data_ulpm_aa));
#endif
/* CORE_FW */
/* CORE_FLASH */
	himax_parse_assign_cmd(flash_addr_spi200_trans_fmt,
		pflash_op->addr_spi200_trans_fmt,
		sizeof(pflash_op->addr_spi200_trans_fmt));
	himax_parse_assign_cmd(flash_addr_spi200_trans_ctrl,
		pflash_op->addr_spi200_trans_ctrl,
		sizeof(pflash_op->addr_spi200_trans_ctrl));
	himax_parse_assign_cmd(flash_addr_spi200_fifo_rst,
		pflash_op->addr_spi200_fifo_rst,
		sizeof(pflash_op->addr_spi200_fifo_rst));
	himax_parse_assign_cmd(flash_addr_spi200_flash_speed,
		pflash_op->addr_spi200_flash_speed,
		sizeof(pflash_op->addr_spi200_flash_speed));
	himax_parse_assign_cmd(flash_addr_spi200_rst_status,
		pflash_op->addr_spi200_rst_status,
		sizeof(pflash_op->addr_spi200_rst_status));
	himax_parse_assign_cmd(flash_addr_spi200_cmd,
		pflash_op->addr_spi200_cmd,
		sizeof(pflash_op->addr_spi200_cmd));
	himax_parse_assign_cmd(flash_addr_spi200_addr,
		pflash_op->addr_spi200_addr,
		sizeof(pflash_op->addr_spi200_addr));
	himax_parse_assign_cmd(flash_addr_spi200_data,
		pflash_op->addr_spi200_data,
		sizeof(pflash_op->addr_spi200_data));
	himax_parse_assign_cmd(flash_addr_spi200_bt_num,
		pflash_op->addr_spi200_bt_num,
		sizeof(pflash_op->addr_spi200_bt_num));
	himax_parse_assign_cmd(flash_data_spi200_trans_fmt,
		pflash_op->data_spi200_trans_fmt,
		sizeof(pflash_op->data_spi200_trans_fmt));
	himax_parse_assign_cmd(flash_data_spi200_txfifo_rst,
		pflash_op->data_spi200_txfifo_rst,
		sizeof(pflash_op->data_spi200_txfifo_rst));
	himax_parse_assign_cmd(flash_data_spi200_rxfifo_rst,
		pflash_op->data_spi200_rxfifo_rst,
		sizeof(pflash_op->data_spi200_rxfifo_rst));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_1,
		pflash_op->data_spi200_trans_ctrl_1,
		sizeof(pflash_op->data_spi200_trans_ctrl_1));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_2,
		pflash_op->data_spi200_trans_ctrl_2,
		sizeof(pflash_op->data_spi200_trans_ctrl_2));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_3,
		pflash_op->data_spi200_trans_ctrl_3,
		sizeof(pflash_op->data_spi200_trans_ctrl_3));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_4,
		pflash_op->data_spi200_trans_ctrl_4,
		sizeof(pflash_op->data_spi200_trans_ctrl_4));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_5,
		pflash_op->data_spi200_trans_ctrl_5,
		sizeof(pflash_op->data_spi200_trans_ctrl_5));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_6,
		pflash_op->data_spi200_trans_ctrl_6,
		sizeof(pflash_op->data_spi200_trans_ctrl_6));
	himax_parse_assign_cmd(flash_data_spi200_trans_ctrl_7,
		pflash_op->data_spi200_trans_ctrl_7,
		sizeof(pflash_op->data_spi200_trans_ctrl_7));
	himax_parse_assign_cmd(flash_data_spi200_cmd_1,
		pflash_op->data_spi200_cmd_1,
		sizeof(pflash_op->data_spi200_cmd_1));
	himax_parse_assign_cmd(flash_data_spi200_cmd_2,
		pflash_op->data_spi200_cmd_2,
		sizeof(pflash_op->data_spi200_cmd_2));
	himax_parse_assign_cmd(flash_data_spi200_cmd_3,
		pflash_op->data_spi200_cmd_3,
		sizeof(pflash_op->data_spi200_cmd_3));
	himax_parse_assign_cmd(flash_data_spi200_cmd_4,
		pflash_op->data_spi200_cmd_4,
		sizeof(pflash_op->data_spi200_cmd_4));
	himax_parse_assign_cmd(flash_data_spi200_cmd_5,
		pflash_op->data_spi200_cmd_5,
		sizeof(pflash_op->data_spi200_cmd_5));
	himax_parse_assign_cmd(flash_data_spi200_cmd_6,
		pflash_op->data_spi200_cmd_6,
		sizeof(pflash_op->data_spi200_cmd_6));
	himax_parse_assign_cmd(flash_data_spi200_cmd_7,
		pflash_op->data_spi200_cmd_7,
		sizeof(pflash_op->data_spi200_cmd_7));
	himax_parse_assign_cmd(flash_data_spi200_cmd_8,
		pflash_op->data_spi200_cmd_8,
		sizeof(pflash_op->data_spi200_cmd_8));
	himax_parse_assign_cmd(flash_data_spi200_addr,
		pflash_op->data_spi200_addr,
		sizeof(pflash_op->data_spi200_addr));
/* CORE_FLASH */
/* CORE_SRAM */
	/* sram start*/
	himax_parse_assign_cmd(sram_adr_mkey,
		psram_op->addr_mkey,
		sizeof(psram_op->addr_mkey));
	himax_parse_assign_cmd(sram_adr_rawdata_addr,
		psram_op->addr_rawdata_addr,
		sizeof(psram_op->addr_rawdata_addr));
	himax_parse_assign_cmd(sram_adr_rawdata_end,
		psram_op->addr_rawdata_end,
		sizeof(psram_op->addr_rawdata_end));
	himax_parse_assign_cmd(sram_passwrd_start,
		psram_op->passwrd_start,
		sizeof(psram_op->passwrd_start));
	himax_parse_assign_cmd(sram_passwrd_end,
		psram_op->passwrd_end,
		sizeof(psram_op->passwrd_end));
	/* sram end*/
/* CORE_SRAM */
/* CORE_DRIVER */
	himax_parse_assign_cmd(driver_addr_fw_define_flash_reload,
		pdriver_op->addr_fw_define_flash_reload,
		sizeof(pdriver_op->addr_fw_define_flash_reload));
	himax_parse_assign_cmd(driver_addr_fw_define_2nd_flash_reload,
		pdriver_op->addr_fw_define_2nd_flash_reload,
		sizeof(pdriver_op->addr_fw_define_2nd_flash_reload));
	himax_parse_assign_cmd(driver_addr_fw_define_int_is_edge,
		pdriver_op->addr_fw_define_int_is_edge,
		sizeof(pdriver_op->addr_fw_define_int_is_edge));
	himax_parse_assign_cmd(driver_addr_fw_define_rxnum_txnum,
		pdriver_op->addr_fw_define_rxnum_txnum,
		sizeof(pdriver_op->addr_fw_define_rxnum_txnum));
	himax_parse_assign_cmd(driver_addr_fw_define_maxpt_xyrvs,
		pdriver_op->addr_fw_define_maxpt_xyrvs,
		sizeof(pdriver_op->addr_fw_define_maxpt_xyrvs));
	himax_parse_assign_cmd(driver_addr_fw_define_x_y_res,
		pdriver_op->addr_fw_define_x_y_res,
		sizeof(pdriver_op->addr_fw_define_x_y_res));
	himax_parse_assign_cmd(driver_data_df_rx,
		pdriver_op->data_df_rx,
		sizeof(pdriver_op->data_df_rx));
	himax_parse_assign_cmd(driver_data_df_tx,
		pdriver_op->data_df_tx,
		sizeof(pdriver_op->data_df_tx));
	himax_parse_assign_cmd(driver_data_df_pt,
		pdriver_op->data_df_pt,
		sizeof(pdriver_op->data_df_pt));
	himax_parse_assign_cmd(driver_data_fw_define_flash_reload_dis,
		pdriver_op->data_fw_define_flash_reload_dis,
		sizeof(pdriver_op->data_fw_define_flash_reload_dis));
	himax_parse_assign_cmd(driver_data_fw_define_flash_reload_en,
		pdriver_op->data_fw_define_flash_reload_en,
		sizeof(pdriver_op->data_fw_define_flash_reload_en));
	himax_parse_assign_cmd(
		driver_data_fw_define_rxnum_txnum_maxpt_sorting,
		pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting,
		sizeof(pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting));
	himax_parse_assign_cmd(
		driver_data_fw_define_rxnum_txnum_maxpt_normal,
		pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal,
		sizeof(pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal));
/* CORE_DRIVER */
#if defined(HX_ZERO_FLASH)
	himax_parse_assign_cmd(zf_data_dis_flash_reload,
		pzf_op->data_dis_flash_reload,
		sizeof(pzf_op->data_dis_flash_reload));
	himax_parse_assign_cmd(zf_addr_system_reset,
		pzf_op->addr_system_reset,
		sizeof(pzf_op->addr_system_reset));
	himax_parse_assign_cmd(zf_data_system_reset,
		pzf_op->data_system_reset,
		sizeof(pzf_op->data_system_reset));
	himax_parse_assign_cmd(zf_data_sram_start_addr,
		pzf_op->data_sram_start_addr,
		sizeof(pzf_op->data_sram_start_addr));
	himax_parse_assign_cmd(zf_data_cfg_info,
		pzf_op->data_cfg_info,
		sizeof(pzf_op->data_cfg_info));
	himax_parse_assign_cmd(zf_data_fw_cfg_1,
		pzf_op->data_fw_cfg_1,
		sizeof(pzf_op->data_fw_cfg_1));
	himax_parse_assign_cmd(zf_data_fw_cfg_2,
		pzf_op->data_fw_cfg_2,
		sizeof(pzf_op->data_fw_cfg_2));
	himax_parse_assign_cmd(zf_data_fw_cfg_3,
		pzf_op->data_fw_cfg_3,
		sizeof(pzf_op->data_fw_cfg_3));
	himax_parse_assign_cmd(zf_data_adc_cfg_1,
		pzf_op->data_adc_cfg_1,
		sizeof(pzf_op->data_adc_cfg_1));
	himax_parse_assign_cmd(zf_data_adc_cfg_2,
		pzf_op->data_adc_cfg_2,
		sizeof(pzf_op->data_adc_cfg_2));
	himax_parse_assign_cmd(zf_data_adc_cfg_3,
		pzf_op->data_adc_cfg_3,
		sizeof(pzf_op->data_adc_cfg_3));
	himax_parse_assign_cmd(zf_data_map_table,
		pzf_op->data_map_table,
		sizeof(pzf_op->data_map_table));
/*	himax_parse_assign_cmd(zf_data_mode_switch,
 *		 pzf_op->data_mode_switch,
 *		 sizeof(pzf_op->data_mode_switch));
 */
	himax_parse_assign_cmd(zf_addr_sts_chk,
		pzf_op->addr_sts_chk,
		sizeof(pzf_op->addr_sts_chk));
	himax_parse_assign_cmd(zf_data_activ_sts,
		pzf_op->data_activ_sts,
		sizeof(pzf_op->data_activ_sts));
	himax_parse_assign_cmd(zf_addr_activ_relod,
		pzf_op->addr_activ_relod,
		sizeof(pzf_op->addr_activ_relod));
	himax_parse_assign_cmd(zf_data_activ_in,
		pzf_op->data_activ_in,
		sizeof(pzf_op->data_activ_in));
#endif
}
EXPORT_SYMBOL(himax_mcu_in_cmd_init);

/* init end*/
/* CORE_INIT */

/*
 * Himax Android Driver Sample Code for IC Core
 *
 * Copyright (C) 2018 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "himax_ic_core.h"

struct himax_core_command_operation *g_core_cmd_op;
struct ic_operation *pic_op;
struct fw_operation *pfw_op;
struct flash_operation *pflash_op;
struct sram_operation *psram_op;
struct driver_operation *pdriver_op;
#ifdef HX_ZERO_FLASH
struct zf_operation *pzf_op;
#endif

#ifdef CORE_IC
/* IC side start */
static void himax_mcu_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	/* I("%s,Entering\n",__func__); */
	tmp_data[0] = pic_op->data_conti[0];

	if (himax_bus_write(pic_op->addr_conti[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (pic_op->data_incr4[0] | auto_add_4_byte);

	if (himax_bus_write(pic_op->addr_incr4[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

static int himax_mcu_register_read(uint8_t *read_addr, uint32_t read_length, uint8_t *read_data, uint8_t cfg_flag)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	int i = 0;
	int address = 0;

	/* I("%s,Entering\n",__func__); */

	if (cfg_flag == false) {
		if (read_length > FLASH_RW_MAX_LEN) {
			E("%s: read len over %d!\n", __func__, FLASH_RW_MAX_LEN);
			return LENGTH_FAIL;
		}

		if (read_length > FOUR_BYTE_DATA_SZ)
			g_core_fp.fp_burst_enable(1);
		else
			g_core_fp.fp_burst_enable(0);

		address = (read_addr[3] << 24) + (read_addr[2] << 16) + (read_addr[1] << 8) + read_addr[0];
		i = address;
		tmp_data[0] = (uint8_t)i;
		tmp_data[1] = (uint8_t)(i >> 8);
		tmp_data[2] = (uint8_t)(i >> 16);
		tmp_data[3] = (uint8_t)(i >> 24);

		if (himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], tmp_data, FOUR_BYTE_DATA_SZ, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

		tmp_data[0] = pic_op->data_ahb_access_direction_read[0];

		if (himax_bus_write(pic_op->addr_ahb_access_direction[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

		if (himax_bus_read(pic_op->addr_ahb_rdata_byte_0[0], read_data, read_length, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

		if (read_length > FOUR_BYTE_DATA_SZ)
			g_core_fp.fp_burst_enable(0);

	} else {
		if (himax_bus_read(read_addr[0], read_data, read_length, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}
	}
	return NO_ERR;
}

static int himax_mcu_flash_write_burst(uint8_t *reg_byte, uint8_t *write_data)
{
	uint8_t data_byte[FLASH_WRITE_BURST_SZ];
	int i = 0, j = 0;
	int data_byte_sz = sizeof(data_byte);

	for (i = 0; i < FOUR_BYTE_ADDR_SZ; i++)
		data_byte[i] = reg_byte[i];

	for (j = FOUR_BYTE_ADDR_SZ; j < data_byte_sz; j++)
		data_byte[j] = write_data[j - FOUR_BYTE_ADDR_SZ];

	if (himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], data_byte, data_byte_sz, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	return NO_ERR;
}

static void himax_mcu_flash_write_burst_length(uint8_t *reg_byte, uint8_t *write_data, uint32_t length)
{
	uint8_t *data_byte;
	int i = 0, j = 0;

	/*
	 * if (length + FOUR_BYTE_ADDR_SZ > FLASH_RW_MAX_LEN) {
	 *	E("%s: write len over %d!\n", __func__, FLASH_RW_MAX_LEN);
	 *	return;
	 * }
	 */

	data_byte = kcalloc((length + 4), sizeof(uint8_t), GFP_KERNEL);
	if (!data_byte) {
		E("%s: allocate memory failed!\n", __func__);
		return;
	}

	for (i = 0; i < FOUR_BYTE_ADDR_SZ; i++)
		data_byte[i] = reg_byte[i];

	for (j = FOUR_BYTE_ADDR_SZ; j < length + FOUR_BYTE_ADDR_SZ; j++)
		data_byte[j] = write_data[j - FOUR_BYTE_ADDR_SZ];

	if (himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], data_byte, length + FOUR_BYTE_ADDR_SZ, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		kfree(data_byte);
		return;
	}
	kfree(data_byte);
}

static void himax_mcu_register_write(uint8_t *write_addr, uint32_t write_length, uint8_t *write_data, uint8_t cfg_flag)
{
	int i, address;

	/* I("%s,Entering\n", __func__); */
	if (cfg_flag == false) {
		address = (write_addr[3] << 24) + (write_addr[2] << 16) + (write_addr[1] << 8) + write_addr[0];

		for (i = address; i < address + write_length; i++) {
			if (write_length > FOUR_BYTE_DATA_SZ)
				g_core_fp.fp_burst_enable(1);
			else
				g_core_fp.fp_burst_enable(0);

			g_core_fp.fp_flash_write_burst_length(write_addr, write_data, write_length);
		}
	} else if (cfg_flag == true) {
		if (himax_bus_write(write_addr[0], write_data, write_length, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
	} else {
		E("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);
		return;
	}
}

static int himax_write_read_reg(uint8_t *tmp_addr, uint8_t *tmp_data, uint8_t hb, uint8_t lb)
{
	int cnt = 0;

	do {
		g_core_fp.fp_flash_write_burst(tmp_addr, tmp_data);
		msleep(20);
		g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, 0);
		/*
		 * I("%s:Now tmp_data[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
		 *	__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
		 */
	} while ((tmp_data[1] != hb && tmp_data[0] != lb) && cnt++ < 100);

	if (cnt == 99)
		return HX_RW_REG_FAIL;

	I("Now register 0x%08X : high byte=0x%02X,low byte=0x%02X\n", tmp_addr[3], tmp_data[1], tmp_data[0]);
	return NO_ERR;
}

static void himax_mcu_interface_on(void)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t tmp_data2[FOUR_BYTE_DATA_SZ];
	int cnt = 0;

	/* Read a dummy register to wake up I2C.*/
	if (himax_bus_read(pic_op->addr_ahb_rdata_byte_0[0], tmp_data, FOUR_BYTE_DATA_SZ, HIMAX_I2C_RETRY_TIMES) < 0) {/* to knock I2C*/
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	do {
		/*
		 *===========================================
		 * Enable continuous burst mode : 0x13 ==> 0x31
		 *===========================================
		 */
		tmp_data[0] = pic_op->data_conti[0];

		if (himax_bus_write(pic_op->addr_conti[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/*
		 *===========================================
		 * AHB address auto +4		: 0x0D ==> 0x11
		 * Do not AHB address auto +4 : 0x0D ==> 0x10
		 *===========================================
		 */
		tmp_data[0] = pic_op->data_incr4[0];

		if (himax_bus_write(pic_op->addr_incr4[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/* Check cmd */
		if (himax_bus_read(pic_op->addr_conti[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail: addr_conti!\n", __func__);
			return;
		}
		if (himax_bus_read(pic_op->addr_incr4[0], tmp_data2, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail: addr_incr4!\n", __func__);
			return;
		}
		if (tmp_data[0] == pic_op->data_conti[0] && tmp_data2[0] == pic_op->data_incr4[0])
			break;

		msleep(20);
	} while (++cnt < 10);

	if (cnt > 0)
		I("%s:Polling burst mode: %d times", __func__, cnt);

}

static bool himax_mcu_wait_wip(int Timing)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	int retry_cnt = 0;

	/*
	 *=====================================
	 * SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	 *=====================================
	 */
	g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_fmt, pflash_op->data_spi200_trans_fmt);
	tmp_data[0] = 0x01;

	do {
		/*
		 *=====================================
		 * SPI Transfer Control : 0x8000_0020 ==> 0x4200_0003
		 *=====================================
		 */
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_ctrl, pflash_op->data_spi200_trans_ctrl_1);
		/*
		 *=====================================
		 * SPI Command : 0x8000_0024 ==> 0x0000_0005
		 * read 0x8000_002C for 0x01, means wait success
		 *=====================================
		 */
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_cmd, pflash_op->data_spi200_cmd_1);
		tmp_data[0] = tmp_data[1] = tmp_data[2] = tmp_data[3] = 0xFF;
		g_core_fp.fp_register_read(pflash_op->addr_spi200_data, 4, tmp_data, 0);

		if ((tmp_data[0] & 0x01) == 0x00)
			return true;

		retry_cnt++;

		if (tmp_data[0] != 0x00 || tmp_data[1] != 0x00 || tmp_data[2] != 0x00 || tmp_data[3] != 0x00)
			I("%s:Wait wip retry_cnt:%d, buffer[0]=%d, buffer[1]=%d, buffer[2]=%d, buffer[3]=%d\n",
			  __func__, retry_cnt, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

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
	/* uint8_t tmp_addr[FOUR_BYTE_ADDR_SZ]; */
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	int retry = 0;

	D("Enter %s\n", __func__);
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		sizeof(pfw_op->data_clear), pfw_op->data_clear, false);
	msleep(20);

	if (!FlashMode) {
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#else
		/* ===AHBI2C_SystemReset========== */
		g_core_fp.fp_system_reset();
#endif
	} else {
		do {
			/*
			 * tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x98;
			 * tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x53;
			 */
			g_core_fp.fp_register_write(pfw_op->addr_safe_mode_release_pw,
				sizeof(pfw_op->data_safe_mode_release_pw_active), pfw_op->data_safe_mode_release_pw_active, false);
			/* tmp_addr[0] = 0xE4; */
			g_core_fp.fp_register_read(pfw_op->addr_flag_reset_event, FOUR_BYTE_DATA_SZ, tmp_data, 0);
			I("%s:Read status from IC = %X,%X\n", __func__, tmp_data[0], tmp_data[1]);
		} while ((tmp_data[1] != 0x01 || tmp_data[0] != 0x00) && retry++ < 5);

		if (retry >= 5) {
			E("%s: Fail:\n", __func__);
#ifdef HX_RST_PIN_FUNC
			g_core_fp.fp_ic_reset(false, false);
#else
			/* ===AHBI2C_System Reset========== */
			g_core_fp.fp_system_reset();
#endif
		} else {
			I("%s:OK and Read status from IC = %X,%X\n", __func__, tmp_data[0], tmp_data[1]);
			/* reset code */
			tmp_data[0] = 0x00;

			if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
				E("%s: i2c access fail!\n", __func__);

			if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
				E("%s: i2c access fail!\n", __func__);

			/*
			 * tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x98;
			 * tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
			 */
			g_core_fp.fp_register_write(pfw_op->addr_safe_mode_release_pw,
				sizeof(pfw_op->data_safe_mode_release_pw_reset), pfw_op->data_safe_mode_release_pw_reset, false);
		}
	}
}

static bool himax_mcu_sense_off(void)
{
	uint8_t cnt = 0;
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];

	do {
		/*
		 *===========================================
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 *===========================================
		 */
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *===========================================
		 * I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 *===========================================
		 */
		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*
		 *======================
		 * Check enter_save_mode
		 *======================
		 */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state, FOUR_BYTE_ADDR_SZ, tmp_data, 0);
		D("%s: Check enter_save_mode data[0]=%X\n",
			__func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/*
			 *=====================================
			 * Reset TCON
			 *=====================================
			 */
			g_core_fp.fp_flash_write_burst(pic_op->addr_tcon_on_rst, pic_op->data_rst);
			msleep(20);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_flash_write_burst(pic_op->addr_tcon_on_rst, tmp_data);
			/*
			 *=====================================
			 * Reset ADC
			 *=====================================
			 */
			g_core_fp.fp_flash_write_burst(pic_op->addr_adc_on_rst, pic_op->data_rst);
			msleep(20);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_flash_write_burst(pic_op->addr_adc_on_rst, tmp_data);
			return true;
		}

		msleep(20);
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#endif

	} while (cnt++ < 15);

	return false;
}

static void himax_mcu_init_psl(void) /*power saving level*/
{
	/*
	 *==============================================================
	 * SCU_Power_State_PW : 0x9000_00A0 ==> 0x0000_0000 (Reset PSL)
	 *==============================================================
	 */
	g_core_fp.fp_register_write(pic_op->addr_psl, sizeof(pic_op->data_rst), pic_op->data_rst, false);
	I("%s: power saving level reset OK!\n", __func__);
}

static void himax_mcu_resume_ic_action(void)
{
	/* Nothing to do */
}

static void himax_mcu_suspend_ic_action(void)
{
	/* Keep TS_RESET PIN to LOW, to avoid leakage during suspend */
#ifdef HX_RST_PIN_FUNC
	himax_rst_gpio_set(private_ts->rst_gpio, 0);
#endif
}

static void himax_mcu_power_on_init(void)
{
	D("%s:\n", __func__);
	g_core_fp.fp_touch_information();
	/* RawOut select initial */
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel, sizeof(pfw_op->data_clear), pfw_op->data_clear, false);
	/* DSRAM func initial */
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);
	g_core_fp.fp_sense_on(0x00);
}

/* IC side end */
#endif

#ifdef CORE_FW
/* FW side start */
static void diag_mcu_parse_raw_data(struct himax_report_data *hx_touch_data, int mul_num, int self_num, uint8_t diag_cmd, int32_t *mutual_data, int32_t *self_data)
{
	int RawDataLen_word;
	int index = 0;
	int temp1, temp2, i;

	if (hx_touch_data->hx_rawdata_buf[0] == pfw_op->data_rawdata_ready_lb[0]
	    && hx_touch_data->hx_rawdata_buf[1] == pfw_op->data_rawdata_ready_hb[0]
	    && hx_touch_data->hx_rawdata_buf[2] > 0
	    && hx_touch_data->hx_rawdata_buf[3] == diag_cmd) {
		RawDataLen_word = hx_touch_data->rawdata_size / 2;
		index = (hx_touch_data->hx_rawdata_buf[2] - 1) * RawDataLen_word;

		/*
		 * I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", index, buf[56], buf[57], buf[58], buf[59], mul_num, self_num);
		 * I("RawDataLen=%d , RawDataLen_word=%d , hx_touch_info_size=%d\n", RawDataLen, RawDataLen_word, hx_touch_info_size);
		 */
		for (i = 0; i < RawDataLen_word; i++) {
			temp1 = index + i;

			if (temp1 < mul_num) /* mutual */
				mutual_data[index + i] = ((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 + 4 + 1]) * 256 + hx_touch_data->hx_rawdata_buf[i * 2 + 4];

			else { /* self */
				temp1 = i + index;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2)
					break;

				self_data[i + index - mul_num] = (((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 + 4 + 1]) << 8) +
				hx_touch_data->hx_rawdata_buf[i * 2 + 4];
			}
		}
	}
}

static void himax_mcu_system_reset(void)
{
	/*
	 * tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x18;
	 * tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x55;
	 */
	g_core_fp.fp_register_write(pfw_op->addr_system_reset, sizeof(pfw_op->data_system_reset), pfw_op->data_system_reset, false);
}

static bool himax_mcu_Calculate_CRC_with_AP(unsigned char *FW_content, int CRC_from_FW, int mode)
{
	return true;
}

static uint32_t himax_mcu_check_CRC(uint8_t *start_addr, int reload_length)
{
	uint32_t result = 0;
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	int cnt = 0, ret = 0;
	int length = reload_length / FOUR_BYTE_DATA_SZ;
	/*
	 * CRC4 // 0x8005_0020 <= from, 0x8005_0028 <= 0x0099_length
	 * tmp_addr[3] = 0x80; tmp_addr[2] = 0x05; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
	 */
	ret = g_core_fp.fp_flash_write_burst(pfw_op->addr_reload_addr_from, start_addr);
	if (ret < NO_ERR) {
		E("%s: i2c access fail!\n", __func__);
		return HW_CRC_FAIL;
	}
	/* tmp_addr[3] = 0x80; tmp_addr[2] = 0x05; tmp_addr[1] = 0x00; tmp_addr[0] = 0x28; */
	tmp_data[3] = 0x00; tmp_data[2] = 0x99; tmp_data[1] = (length >> 8); tmp_data[0] = length;
	ret = g_core_fp.fp_flash_write_burst(pfw_op->addr_reload_addr_cmd_beat, tmp_data);
	if (ret < NO_ERR) {
		E("%s: i2c access fail!\n", __func__);
		return HW_CRC_FAIL;
	}
	cnt = 0;

	do {
		/* tmp_addr[3] = 0x80; tmp_addr[2] = 0x05; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00; */
		ret = g_core_fp.fp_register_read(pfw_op->addr_reload_status, FOUR_BYTE_DATA_SZ, tmp_data, 0);
		if (ret < NO_ERR) {
			E("%s: i2c access fail!\n", __func__);
			return HW_CRC_FAIL;
		}

		if ((tmp_data[0] & 0x01) != 0x01) {
			/* tmp_addr[3] = 0x80; tmp_addr[2] = 0x05; tmp_addr[1] = 0x00; tmp_addr[0] = 0x18; */
			ret = g_core_fp.fp_register_read(pfw_op->addr_reload_crc32_result, FOUR_BYTE_DATA_SZ, tmp_data, 0);
			if (ret < NO_ERR) {
				E("%s: i2c access fail!\n", __func__);
				return HW_CRC_FAIL;
			}
			I("%s: tmp_data[3]=%X, tmp_data[2]=%X, tmp_data[1]=%X, tmp_data[0]=%X\n", __func__, tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
			result = ((tmp_data[3] << 24) + (tmp_data[2] << 16) + (tmp_data[1] << 8) + tmp_data[0]);
			break;
		}
	} while (cnt++ < 100);

	return result;
}

static void himax_mcu_set_reload_cmd(uint8_t *write_data, int idx, uint32_t cmd_from, uint32_t cmd_to, uint32_t cmd_beat)
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

static void himax_mcu_set_SMWP_enable(uint8_t SMWP_enable, bool suspended)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t back_data[FOUR_BYTE_DATA_SZ];
	uint8_t retry_cnt = 0;

	do {
		if (SMWP_enable) {
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd, tmp_data, 4);
			g_core_fp.fp_flash_write_burst(pfw_op->addr_smwp_enable, tmp_data);
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd, back_data, 4);
		} else {
			himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset, tmp_data, 4);
			g_core_fp.fp_flash_write_burst(pfw_op->addr_smwp_enable, tmp_data);
			himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset, back_data, 4);
		}

		g_core_fp.fp_register_read(pfw_op->addr_smwp_enable, FOUR_BYTE_DATA_SZ, tmp_data, 0);
		/* I("%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d\n", __func__, tmp_data[0],SMWP_enable,retry_cnt); */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2] || tmp_data[1] != back_data[1]  || tmp_data[0] != back_data[0]) && retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_set_HSEN_enable(uint8_t HSEN_enable, bool suspended)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t back_data[FOUR_BYTE_DATA_SZ];
	uint8_t retry_cnt = 0;

	do {
		if (HSEN_enable) {
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd, tmp_data, 4);
			g_core_fp.fp_flash_write_burst(pfw_op->addr_hsen_enable, tmp_data);
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd, back_data, 4);
		} else {
			himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset, tmp_data, 4);
			g_core_fp.fp_flash_write_burst(pfw_op->addr_hsen_enable, tmp_data);
			himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset, back_data, 4);
		}

		g_core_fp.fp_register_read(pfw_op->addr_hsen_enable, FOUR_BYTE_DATA_SZ, tmp_data, 0);
		/* I("%s: tmp_data[0]=%d, HSEN_enable=%d, retry_cnt=%d\n", __func__, tmp_data[0],HSEN_enable,retry_cnt); */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2] || tmp_data[1] != back_data[1]  || tmp_data[0] != back_data[0]) && retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_usb_detect_set(uint8_t *cable_config)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t back_data[FOUR_BYTE_DATA_SZ];
	uint8_t retry_cnt = 0;

	do {
		if (cable_config[1] == 0x01) {
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd, tmp_data, 4);
			g_core_fp.fp_flash_write_burst(pfw_op->addr_usb_detect, tmp_data);
			himax_in_parse_assign_cmd(fw_func_handshaking_pwd, back_data, 4);
			I("%s: USB detect status IN!\n", __func__);
		} else {
			himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset, tmp_data, 4);
			g_core_fp.fp_flash_write_burst(pfw_op->addr_usb_detect, tmp_data);
			himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset, back_data, 4);
			I("%s: USB detect status OUT!\n", __func__);
		}

		g_core_fp.fp_register_read(pfw_op->addr_usb_detect, FOUR_BYTE_DATA_SZ, tmp_data, 0);
		/* I("%s: tmp_data[0]=%d, USB detect=%d, retry_cnt=%d\n", __func__, tmp_data[0],cable_config[1] ,retry_cnt); */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2] || tmp_data[1] != back_data[1]  || tmp_data[0] != back_data[0]) && retry_cnt < HIMAX_REG_RETRY_TIMES);
}

static void himax_mcu_diag_register_set(uint8_t diag_command, uint8_t storage_type)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];

	if (diag_command > 0 && storage_type % 8 > 0)
		tmp_data[0] = diag_command + 0x08;
	else
		tmp_data[0] = diag_command;
	I("diag_command = %d, tmp_data[0] = %X\n", diag_command, tmp_data[0]);
	g_core_fp.fp_interface_on();
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00;
	g_core_fp.fp_flash_write_burst(pfw_op->addr_raw_out_sel, tmp_data);
	g_core_fp.fp_register_read(pfw_op->addr_raw_out_sel, FOUR_BYTE_DATA_SZ, tmp_data, 0);
	I("%s: tmp_data[3]=0x%02X,tmp_data[2]=0x%02X,tmp_data[1]=0x%02X,tmp_data[0]=0x%02X!\n",
	  __func__, tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
}

static int himax_mcu_chip_self_test(void)
{
	uint8_t tmp_data[FLASH_WRITE_BURST_SZ];
	uint8_t self_test_info[20];
	int pf_value = 0x00;
	uint8_t test_result_id = 0;
	int i;

	memset(tmp_data, 0x00, sizeof(tmp_data));
	g_core_fp.fp_interface_on();
	g_core_fp.fp_sense_off();
	g_core_fp.fp_burst_enable(1);
	/* 0x10007f18 -> 0x00006AA6 */
	g_core_fp.fp_flash_write_burst(pfw_op->addr_selftest_addr_en, pfw_op->data_selftest_request);
	/* Set criteria 0x10007F1C [0,1]=aa/up,down=, [2-3]=key/up,down, [4-5]=avg/up,down */
	tmp_data[0] = pfw_op->data_criteria_aa_top[0];
	tmp_data[1] = pfw_op->data_criteria_aa_bot[0];
	tmp_data[2] = pfw_op->data_criteria_key_top[0];
	tmp_data[3] = pfw_op->data_criteria_key_bot[0];
	tmp_data[4] = pfw_op->data_criteria_avg_top[0];
	tmp_data[5] = pfw_op->data_criteria_avg_bot[0];
	tmp_data[6] = 0x00;
	tmp_data[7] = 0x00;
	g_core_fp.fp_flash_write_burst_length(pfw_op->addr_criteria_addr, tmp_data, FLASH_WRITE_BURST_SZ);
	/* 0x10007294 -> 0x0000190  //SET IIR_MAX FRAMES */
	g_core_fp.fp_flash_write_burst(pfw_op->addr_set_frame_addr, pfw_op->data_set_frame);
	/* Disable IDLE Mode */
	g_core_fp.fp_idle_mode(1);
	/* 0x10007f00 -> 0x0000A55A //Disable Flash Reload */
	g_core_fp.fp_reload_disable(1);
	/* start selftest // leave safe mode */
	g_core_fp.fp_sense_on(0x01);

	/* Hand shaking -> 0x10007f18 waiting 0xA66A */
	for (i = 0; i < 1000; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_selftest_addr_en, 4, tmp_data, 0);
		I("%s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X,tmp_data[3] = 0x%02X, cnt=%d\n",
		  __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3], i);
		msleep(20);

		if (tmp_data[1] == pfw_op->data_selftest_ack_hb[0] && tmp_data[0] == pfw_op->data_selftest_ack_lb[0]) {
			I("%s Data ready goto moving data\n", __func__);
			break;
		}
	}

	g_core_fp.fp_sense_off();
	msleep(20);
	/*
	 *=====================================
	 * Read test result ID : 0x10007f24 ==> bit[2][1][0] = [key][AA][avg] => 0xF = PASS
	 *=====================================
	 */
	g_core_fp.fp_register_read(pfw_op->addr_selftest_result_addr, 20, self_test_info, 0);
	test_result_id = self_test_info[0];
	I("%s: check test result, test_result_id=%x, test_result=%x\n", __func__
	  , test_result_id, self_test_info[0]);
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
		pf_value = 0x0;
	} else {
		E("[Himax]: self-test fail\n");
		/*
		 * E("[Himax]: bank_avg = %d, bank_max = %d,%d,%d, bank_min = %d,%d,%d, key = %d,%d,%d\n",
		 *  tmp_data[1],tmp_data[2],tmp_data[3],tmp_data[4],tmp_data[5],tmp_data[6],tmp_data[7],
		 *  tmp_data[8],tmp_data[9],tmp_data[10]);
		 */
		pf_value = 0x1;
	}

	/* Enable IDLE Mode */
	g_core_fp.fp_idle_mode(0);
	/* 0x10007f00 -> 0x00000000 //Enable Flash Reload //recovery */
	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_sense_on(0x00);
	msleep(120);
	return pf_value;
}

static void himax_mcu_idle_mode(int disable)
{
	int retry = 20;
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t switch_cmd = 0x00;

	I("%s:entering\n", __func__);

	do {
		I("%s,now %d times\n!", __func__, retry);
		g_core_fp.fp_register_read(pfw_op->addr_fw_mode_status, FOUR_BYTE_DATA_SZ, tmp_data, 0);

		if (disable)
			switch_cmd = pfw_op->data_idle_dis_pwd[0];
		else
			switch_cmd = pfw_op->data_idle_en_pwd[0];

		tmp_data[0] = switch_cmd;
		g_core_fp.fp_flash_write_burst(pfw_op->addr_fw_mode_status, tmp_data);
		g_core_fp.fp_register_read(pfw_op->addr_fw_mode_status, FOUR_BYTE_DATA_SZ, tmp_data, 0);
		I("%s:After turn ON/OFF IDLE Mode [0] = 0x%02X,[1] = 0x%02X,[2] = 0x%02X,[3] = 0x%02X\n",
		  __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
		retry--;
		msleep(20);
	} while ((tmp_data[0] != switch_cmd) && retry > 0);

	I("%s: setting OK!\n", __func__);
}

static void himax_mcu_reload_disable(int disable)
{
	I("%s:entering\n", __func__);

	if (disable) /* reload disable */
		g_core_fp.fp_flash_write_burst(pdriver_op->addr_fw_define_flash_reload, pdriver_op->data_fw_define_flash_reload_dis);
	else /* reload enable */
		g_core_fp.fp_flash_write_burst(pdriver_op->addr_fw_define_flash_reload, pdriver_op->data_fw_define_flash_reload_en);

	I("%s: setting OK!\n", __func__);
}

static bool himax_mcu_check_chip_version(void)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t ret_data = false;
	int i = 0;

	for (i = 0; i < 5; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_icid_addr, FOUR_BYTE_DATA_SZ, tmp_data, 0);
		I("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3], tmp_data[2], tmp_data[1]);

		if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x10) && (tmp_data[1] == 0x2a)) {
			strlcpy(private_ts->chip_name, HX_83102A_SERIES_PWON, 30);
			ret_data = true;
			break;
		}

		ret_data = false;
		E("%s:Read driver ID register Fail:\n", __func__);
	}

	return ret_data;
}

static int himax_mcu_read_ic_trigger_type(void)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	int trigger_type = false;

	g_core_fp.fp_register_read(pfw_op->addr_trigger_addr, FOUR_BYTE_DATA_SZ, tmp_data, 0);

	if ((tmp_data[0] & 0x01) == 1)
		trigger_type = true;

	return trigger_type;
}

static int himax_mcu_read_i2c_status(void)
{
	return i2c_error_count;
}

static void himax_mcu_read_FW_ver(void)
{
	uint8_t data[FOUR_BYTE_DATA_SZ];
	uint8_t data_2[FOUR_BYTE_DATA_SZ];
	int retry = 200;
	int reload_status = 0;

	g_core_fp.fp_sense_on(0x00);

	while (reload_status == 0) {
		/* cmd[3] = 0x10; cmd[2] = 0x00; cmd[1] = 0x7f; cmd[0] = 0x00; */
		g_core_fp.fp_register_read(pdriver_op->addr_fw_define_flash_reload, FOUR_BYTE_DATA_SZ, data, 0);
		g_core_fp.fp_register_read(pdriver_op->addr_fw_define_2nd_flash_reload, FOUR_BYTE_DATA_SZ, data_2, 0);

		if ((data[1] == 0x3A && data[0] == 0xA3)
			|| (data_2[1] == 0x72 && data_2[0] == 0xC0)) {
			D("reload OK!\n");
			reload_status = 1;
			break;
		} else if (retry == 0) {
			E("reload 20 times! fail\n");
			ic_data->vendor_panel_ver = 0;
			ic_data->vendor_fw_ver = 0;
			ic_data->vendor_config_ver = 0;
			ic_data->vendor_touch_cfg_ver = 0;
			ic_data->vendor_display_cfg_ver = 0;
			ic_data->vendor_cid_maj_ver = 0;
			ic_data->vendor_cid_min_ver = 0;
			return;
		}

		retry--;
		msleep(20);
		if (retry % 10 == 0)
			E("reload fail ,delay 10ms retry=%d\n", retry);

	}

	D("%s:data[]={0x%2.2X, 0x%2.2X}, data_2[]={0x%2.2X, 0x%2.2X}\n",
		__func__, data[0], data[1], data_2[0], data_2[1]);
	D("reload_status=%d\n", reload_status);
	/*
	 *=====================================
	 * Read FW version : 0x1000_7004  but 05,06 are the real addr for FW Version
	 *=====================================
	 */
	g_core_fp.fp_sense_off();
	g_core_fp.fp_register_read(pfw_op->addr_fw_ver_addr, FOUR_BYTE_DATA_SZ, data, 0);
	ic_data->vendor_panel_ver =  data[0];
	ic_data->vendor_fw_ver = data[1] << 8 | data[2];
	D("PANEL_VER : %X\n", ic_data->vendor_panel_ver);
	D("FW_VER : %X\n", ic_data->vendor_fw_ver);
	g_core_fp.fp_register_read(pfw_op->addr_fw_cfg_addr, FOUR_BYTE_DATA_SZ, data, 0);
	ic_data->vendor_config_ver = data[2] << 8 | data[3];
	/* I("CFG_VER : %X\n",ic_data->vendor_config_ver); */
	ic_data->vendor_touch_cfg_ver = data[2];
	D("TOUCH_VER : %X\n", ic_data->vendor_touch_cfg_ver);
	ic_data->vendor_display_cfg_ver = data[3];
	D("DISPLAY_VER : %X\n", ic_data->vendor_display_cfg_ver);
	g_core_fp.fp_register_read(pfw_op->addr_fw_vendor_addr, FOUR_BYTE_DATA_SZ, data, 0);
	ic_data->vendor_cid_maj_ver = data[2];
	ic_data->vendor_cid_min_ver = data[3];
	D("CID_VER : %X\n", (data[2] << 8 | data[3]));
}

static bool himax_mcu_read_event_stack(uint8_t *buf, uint8_t length)
{
	uint8_t cmd[FOUR_BYTE_DATA_SZ];
	/* AHB_I2C Burst Read Off */
	cmd[0] = pfw_op->data_ahb_dis[0];

	if (himax_bus_write(pfw_op->addr_ahb_addr[0], cmd, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	if (himax_bus_read(pfw_op->addr_event_addr[0], buf, length, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	/* AHB_I2C Burst Read On */
	cmd[0] = pfw_op->data_ahb_en[0];

	if (himax_bus_write(pfw_op->addr_ahb_addr[0], cmd, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	return 1;
}

static void himax_mcu_return_event_stack(void)
{
	int retry = 20, i;
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];

	D("%s:entering\n", __func__);

	do {
		D("now %d times\n!", retry);

		for (i = 0; i < FOUR_BYTE_DATA_SZ; i++)
			tmp_data[i] = psram_op->addr_rawdata_end[i];

		g_core_fp.fp_flash_write_burst(psram_op->addr_rawdata_addr, tmp_data);
		g_core_fp.fp_register_read(psram_op->addr_rawdata_addr, FOUR_BYTE_DATA_SZ, tmp_data, 0);
		retry--;
		msleep(20);
	} while ((tmp_data[1] != psram_op->addr_rawdata_end[1] && tmp_data[0] != psram_op->addr_rawdata_end[0]) && retry > 0);

	D("%s: End of setting!\n", __func__);
}

static bool himax_mcu_calculateChecksum(bool change_iref)
{
	uint8_t CRC_result = 0, i;
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];

	for (i = 0; i < FOUR_BYTE_DATA_SZ; i++)
		tmp_data[i] = psram_op->addr_rawdata_end[i];

	CRC_result = g_core_fp.fp_check_CRC(tmp_data, FW_SIZE_64k);
	msleep(50);

	return (CRC_result == 0) ? true : false;
}

static int himax_mcu_read_FW_status(uint8_t *state_addr, uint8_t *tmp_addr)
{
	uint8_t i;
	uint8_t req_size = 0;
	uint8_t status_addr[FOUR_BYTE_DATA_SZ]; /* 0x10007F44 */
	uint8_t cmd_addr[FOUR_BYTE_DATA_SZ]; /* 0x900000F8 */

	if (state_addr[0] == 0x01) {
		state_addr[1] = 0x04;

		for (i = 0; i < FOUR_BYTE_DATA_SZ; i++) {
			/* 0x10007F44 */
			state_addr[i + 2] = pfw_op->addr_fw_dbg_msg_addr[i];
			status_addr[i] = pfw_op->addr_fw_dbg_msg_addr[i];
		}

		req_size = 0x04;
		g_core_fp.fp_register_read(status_addr, req_size, tmp_addr, 0);
	} else if (state_addr[0] == 0x02) {
		state_addr[1] = 0x30;

		for (i = 0; i < FOUR_BYTE_DATA_SZ; i++) {
			/* 0x10007F44 */
			state_addr[i + 2] = pfw_op->addr_fw_dbg_msg_addr[i];
			cmd_addr[i] = pfw_op->addr_fw_dbg_msg_addr[i];
		}

		req_size = 0x30;
		g_core_fp.fp_register_read(cmd_addr, req_size, tmp_addr, 0);
	}

	return NO_ERR;
}

static void himax_mcu_irq_switch(int switch_on)
{
	if (switch_on) {
		if (private_ts->use_irq)
			himax_int_enable(switch_on);
		else
			hrtimer_start(&private_ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
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

	D("%s:Now tmp[3]=0x%02X, tmp[2]=0x%02X, tmp[1]=0x%02X, tmp[0]=0x%02X\n",
		__func__, tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
	g_core_fp.fp_flash_write_burst(pfw_op->addr_sorting_mode_en, tmp_data);

	return NO_ERR;
}

static int himax_mcu_check_sorting_mode(uint8_t *tmp_data)
{
	g_core_fp.fp_register_read(pfw_op->addr_sorting_mode_en, FOUR_BYTE_DATA_SZ, tmp_data, 0);
	D("%s: tmp[0]=%x,tmp[1]=%x\n", __func__, tmp_data[0], tmp_data[1]);

	return NO_ERR;
}

static int himax_mcu_switch_mode(int mode)
{
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t mode_write_cmd;
	uint8_t mode_read_cmd;
	int result = -1;
	int retry = 200;

	D("%s: Entering\n", __func__);

	if (mode == 0) {
		/* normal mode */
		mode_write_cmd = pfw_op->data_normal_cmd[0];
		mode_read_cmd = pfw_op->data_normal_status[0];
	} else {
		/* sorting mode */
		mode_write_cmd = pfw_op->data_sorting_cmd[0];
		mode_read_cmd = pfw_op->data_sorting_status[0];
	}

	g_core_fp.fp_sense_off();
	/* g_core_fp.fp_interface_on();*/
	/* clean up FW status */

	/*
	 * tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00;
	 * tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	 */
	g_core_fp.fp_flash_write_burst(psram_op->addr_rawdata_addr, psram_op->addr_rawdata_end);
	/* tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x7F; tmp_addr[0] = 0x04;*/
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = mode_write_cmd;
	tmp_data[0] = mode_write_cmd;
	g_core_fp.fp_assign_sorting_mode(tmp_data);
	g_core_fp.fp_idle_mode(1);
	g_core_fp.fp_reload_disable(1);

	/* To stable the sorting*/
	if (mode) {
		/*
		 * tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x70; tmp_addr[0] = 0xF4;
		 * tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x08;
		 */
		g_core_fp.fp_flash_write_burst(pdriver_op->addr_fw_define_rxnum_txnum_maxpt, pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting);
	} else {
		g_core_fp.fp_flash_write_burst(pfw_op->addr_set_frame_addr, pfw_op->data_set_frame);
		/*
		 * tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x70; tmp_addr[0] = 0xF4;
		 * tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x14;
		 */
		g_core_fp.fp_flash_write_burst(pdriver_op->addr_fw_define_rxnum_txnum_maxpt, pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal);
	}

	g_core_fp.fp_sense_on(0x01);

	while (retry != 0) {
		D("[%d] %s Read\n", retry, __func__);
		/* tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x7F; tmp_addr[0] = 0x04; */
		g_core_fp.fp_check_sorting_mode(tmp_data);
		msleep(100);
		D("mode_read_cmd(0)=0x%2.2X,mode_read_cmd(1)=0x%2.2X\n",
			tmp_data[0], tmp_data[1]);

		if (tmp_data[0] == mode_read_cmd && tmp_data[1] == mode_read_cmd) {
			D("Read OK!\n");
			result = 0;
			break;
		}

		/* tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0xA8; */
		g_core_fp.fp_register_read(pfw_op->addr_chk_fw_status, FOUR_BYTE_DATA_SZ, tmp_data, 0);

		if (tmp_data[0] == 0x00 && tmp_data[1] == 0x00 && tmp_data[2] == 0x00 && tmp_data[3] == 0x00) {
			E("%s,: FW Stop!\n", __func__);
			break;
		}

		retry--;
	}

	if (result == 0) {
		if (mode == 0)
			return HX_NORMAL_MODE;  /* normal mode */
		else
			return HX_SORTING_MODE; /* sorting mode */
	} else
		return HX_CHANGE_MODE_FAIL; /* change mode fail */

}

static uint8_t himax_mcu_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data)
{
	int cnt = 0;
	uint8_t req_size = cmd_set[0];

	cmd_set[3] = pfw_op->data_dd_request[0];
	g_core_fp.fp_register_write(pfw_op->addr_dd_handshak_addr, FOUR_BYTE_DATA_SZ, cmd_set, 0);
	I("%s: cmd_set[0] = 0x%02X,cmd_set[1] = 0x%02X,cmd_set[2] = 0x%02X,cmd_set[3] = 0x%02X\n",
	  __func__, cmd_set[0], cmd_set[1], cmd_set[2], cmd_set[3]);

	/* Doing hand shaking 0xAA -> 0xBB */
	for (cnt = 0; cnt < 100; cnt++) {
		g_core_fp.fp_register_read(pfw_op->addr_dd_handshak_addr, FOUR_BYTE_DATA_SZ, tmp_data, 0);
		msleep(20);

		if (tmp_data[3] == pfw_op->data_dd_ack[0]) {
			I("%s Data ready goto moving data\n", __func__);
			break;
		} else if (cnt >= 99) {
			I("%s Data not ready in FW\n", __func__);
			return FW_NOT_READY;
		}
	}

	g_core_fp.fp_register_read(pfw_op->addr_dd_data_addr, req_size, tmp_data, 0);
	return NO_ERR;
}
/* FW side end */
#endif

#ifdef CORE_FLASH
/* FLASH side start */
static void himax_mcu_chip_erase(void)
{
	g_core_fp.fp_interface_on();

	/* Reset power saving level */
	if (g_core_fp.fp_init_psl != NULL)
		g_core_fp.fp_init_psl();

	/*
	 *=====================================
	 * SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	 *=====================================
	 */
	g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_fmt, pflash_op->data_spi200_trans_fmt);
	/*
	 *=====================================
	 * Chip Erase
	 * Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
	 *                2. 0x8000_0024 ==> 0x0000_0006
	 *=====================================
	 */
	g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_ctrl, pflash_op->data_spi200_trans_ctrl_2);
	g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_cmd, pflash_op->data_spi200_cmd_2);
	/*
	 *=====================================
	 * Chip Erase
	 * Erase Command : 0x8000_0024 ==> 0x0000_00C7
	 *=====================================
	 */
	g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_cmd, pflash_op->data_spi200_cmd_3);
	msleep(2000);

	if (!g_core_fp.fp_wait_wip(100))
		E("%s: Chip_Erase Fail\n", __func__);

}

/* complete not yet */
static bool himax_mcu_block_erase(int start_addr, int length)
{
	uint32_t page_prog_start = 0;
	uint32_t block_size = 0x10000;

	g_core_fp.fp_interface_on();

	g_core_fp.fp_init_psl();

	/*
	 *=====================================
	 * SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	 *=====================================
	 */
	g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_fmt, pflash_op->data_spi200_trans_fmt);

	for (page_prog_start = start_addr; page_prog_start < start_addr + length; page_prog_start = page_prog_start + block_size) {
		/*
		 *=====================================
		 * Chip Erase
		 * Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
		 *                2. 0x8000_0024 ==> 0x0000_0006
		 *=====================================
		 */
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_ctrl, pflash_op->data_spi200_trans_ctrl_2);
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_cmd, pflash_op->data_spi200_cmd_2);
		/*
		 *=====================================
		 * Block Erase
		 * Erase Command : 0x8000_0028 ==> 0x0000_0000 //SPI addr
		 *   0x8000_0020 ==> 0x6700_0000 //control
		 *   0x8000_0024 ==> 0x0000_0052 //BE
		 *=====================================
		 */
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_addr, pflash_op->data_spi200_addr);
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_ctrl, pflash_op->data_spi200_trans_ctrl_3);
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_cmd, pflash_op->data_spi200_cmd_4);
		msleep(1000);

		if (!g_core_fp.fp_wait_wip(100)) {
			E("%s:Erase Fail\n", __func__);
			return false;
		}
	}

	D("%s:END\n", __func__);
	return true;
}

static bool himax_mcu_sector_erase(int start_addr)
{
	return true;
}

static void himax_mcu_flash_programming(uint8_t *FW_content, int FW_Size)
{
	int page_prog_start = 0, i = 0, j = 0, k = 0;
	int program_length = PROGRAM_SZ;
	uint8_t tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t buring_data[FLASH_RW_MAX_LEN];	/* Read for flash data, 128K */

	/* 4 bytes for 0x80002C padding */
	g_core_fp.fp_interface_on();
	/*
	 *=====================================
	 * SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	 *=====================================
	 */
	g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_fmt, pflash_op->data_spi200_trans_fmt);

	for (page_prog_start = 0; page_prog_start < FW_Size; page_prog_start += FLASH_RW_MAX_LEN) {
		/*
		 *=====================================
		 * Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
		 *                2. 0x8000_0024 ==> 0x0000_0006
		 *=====================================
		 */
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_ctrl, pflash_op->data_spi200_trans_ctrl_2);
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_cmd, pflash_op->data_spi200_cmd_2);
		/*
		 *=================================
		 * SPI Transfer Control
		 * Set 256 bytes page write : 0x8000_0020 ==> 0x610F_F000
		 * Set read start address	: 0x8000_0028 ==> 0x0000_0000
		 *=================================
		 * data bytes should be 0x6100_0000 + ((word_number)*4-1)*4096 = 0x6100_0000 + 0xFF000 = 0x610F_F000
		 * Programmable size = 1 page = 256 bytes, word_number = 256 byte / 4 = 64
		 */
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_trans_ctrl, pflash_op->data_spi200_trans_ctrl_4);

		/* Flash start address 1st : 0x0000_0000 */
		if (page_prog_start < 0x100) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = (uint8_t)page_prog_start;
		} else if (page_prog_start >= 0x100 && page_prog_start < 0x10000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		} else if (page_prog_start >= 0x10000 && page_prog_start < 0x1000000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = (uint8_t)(page_prog_start >> 16);
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}

		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_addr, tmp_data);

		/*
		 *=================================
		 * Send 16 bytes data : 0x8000_002C ==> 16 bytes data
		 *=================================
		 */
		for (i = 0; i < FOUR_BYTE_ADDR_SZ; i++)
			buring_data[i] = pflash_op->addr_spi200_data[i];

		for (i = page_prog_start, j = 0; i < 16 + page_prog_start; i++, j++)/* <------ bin file */
			buring_data[j + FOUR_BYTE_ADDR_SZ] = FW_content[i];

		if (himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], buring_data, FOUR_BYTE_ADDR_SZ + 16, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/*
		 *=================================
		 * Write command : 0x8000_0024 ==> 0x0000_0002
		 *=================================
		 */
		g_core_fp.fp_flash_write_burst(pflash_op->addr_spi200_cmd, pflash_op->data_spi200_cmd_6);

		/*
		 *=================================
		 * Send 240 bytes data : 0x8000_002C ==> 240 bytes data
		 *=================================
		 */
		for (j = 0; j < 5; j++) {
			for (i = (page_prog_start + 16 + (j * 48)), k = 0; i < (page_prog_start + 16 + (j * 48)) + program_length; i++, k++)
				buring_data[k + FOUR_BYTE_ADDR_SZ] = FW_content[i];

			if (himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], buring_data, program_length + FOUR_BYTE_ADDR_SZ, HIMAX_I2C_RETRY_TIMES) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return;
			}
		}

		if (!g_core_fp.fp_wait_wip(1))
			E("%s:Flash_Programming Fail\n", __func__);
	}
}

static void himax_mcu_flash_page_write(uint8_t *write_addr, int length, uint8_t *write_data)
{
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_32k(unsigned char *fw, int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_60k(unsigned char *fw, int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k(unsigned char *fw, int len, bool change_iref)
{
	int burnFW_success = 0;

	if (len != FW_SIZE_64k) {
		E("%s: The file size is not 64K bytes\n", __func__);
		return false;
	}

#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, false);
#else
	/*
	 * tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x18;
	 * tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x55;
	 */
	g_core_fp.fp_system_reset();
#endif
	g_core_fp.fp_sense_off();
	g_core_fp.fp_chip_erase();
	g_core_fp.fp_flash_programming(fw, FW_SIZE_64k);

	if (g_core_fp.fp_check_CRC(pfw_op->addr_program_reload_from, FW_SIZE_64k) == 0)
		burnFW_success = 1;

	/* RawOut select initial */
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel, sizeof(pfw_op->data_clear), pfw_op->data_clear, false);
	/* DSRAM func initial */
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);

#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, false);
#else
	/*
	 * System reset
	 * tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x18;
	 * tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x55;
	 */
	g_core_fp.fp_system_reset();
#endif
	return burnFW_success;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_124k(unsigned char *fw, int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k(unsigned char *fw, int len, bool change_iref)
{
	/* Not use */
	return 0;
}

static void himax_mcu_flash_dump_func(uint8_t local_flash_command, int Flash_Size, uint8_t *flash_buffer)
{
	uint8_t tmp_addr[FOUR_BYTE_DATA_SZ];
	uint8_t buffer[256];
	int page_prog_start = 0;

	g_core_fp.fp_sense_off();
	g_core_fp.fp_burst_enable(1);

	for (page_prog_start = 0; page_prog_start < Flash_Size; page_prog_start += 128) {
		/*
		 *=================================
		 * SPI Transfer Control
		 * Set 256 bytes page read : 0x8000_0020 ==> 0x6940_02FF
		 * Set read start address  : 0x8000_0028 ==> 0x0000_0000
		 * Set command             : 0x8000_0024 ==> 0x0000_003B
		 *=================================
		 */
		tmp_addr[0] = page_prog_start % 0x100;
		tmp_addr[1] = (page_prog_start >> 8) % 0x100;
		tmp_addr[2] = (page_prog_start >> 16) % 0x100;
		tmp_addr[3] = page_prog_start / 0x1000000;
		himax_mcu_register_read(tmp_addr, 128, buffer, 0);
		memcpy(&flash_buffer[page_prog_start], buffer, 128);
	}

	g_core_fp.fp_burst_enable(0);
	g_core_fp.fp_sense_on(0x01);
}

static bool himax_mcu_flash_lastdata_check(void)
{
	uint8_t tmp_addr[4];
	uint32_t start_addr = 0xFF80;
	uint32_t temp_addr = 0;
	uint32_t flash_page_len = 0x80;
	uint8_t flash_tmp_buffer[128];

	for (temp_addr = start_addr; temp_addr < (start_addr + flash_page_len); temp_addr = temp_addr + flash_page_len) {
		/* I("temp_addr=%d,tmp_addr[0]=0x%2X, tmp_addr[1]=0x%2X,tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n", temp_addr,tmp_addr[0], tmp_addr[1], tmp_addr[2],tmp_addr[3]); */
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		g_core_fp.fp_register_read(tmp_addr, flash_page_len, &flash_tmp_buffer[0], 0);
	}

	if ((!flash_tmp_buffer[flash_page_len-4]) && (!flash_tmp_buffer[flash_page_len-3]) && (!flash_tmp_buffer[flash_page_len-2]) && (!flash_tmp_buffer[flash_page_len-1]))
		return 1;/* FAIL */

	I("flash_buffer[FFFC]=0x%2X,flash_buffer[FFFD]=0x%2X,flash_buffer[FFFE]=0x%2X,flash_buffer[FFFF]=0x%2X\n",
	flash_tmp_buffer[flash_page_len-4], flash_tmp_buffer[flash_page_len-3], flash_tmp_buffer[flash_page_len-2], flash_tmp_buffer[flash_page_len-1]);
	return 0;/* PASS */
}
/* FLASH side end */
#endif

#ifdef CORE_SRAM
/* SRAM side start */
static void himax_mcu_sram_write(uint8_t *FW_content)
{
}

static bool himax_mcu_sram_verify(uint8_t *FW_File, int FW_Size)
{
	return true;
}

static void himax_mcu_get_DSRAM_data(uint8_t *info_data, bool DSRAM_Flag)
{
	int i = 0;
	unsigned char tmp_addr[FOUR_BYTE_ADDR_SZ];
	unsigned char tmp_data[FOUR_BYTE_DATA_SZ];
	uint8_t max_i2c_size = MAX_I2C_TRANS_SZ;
	uint8_t x_num = ic_data->HX_RX_NUM;
	uint8_t y_num = ic_data->HX_TX_NUM;
	/* int m_key_num = 0; */
	int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
	int total_size_temp;
	int mutual_data_size = x_num * y_num * 2;
	int total_read_times = 0;
	int address = 0;
	uint8_t *temp_info_data; /* max mkey size = 8 */
	uint16_t check_sum_cal = 0;
	int fw_run_flag = -1;

	temp_info_data = kcalloc((total_size + 8), sizeof(uint8_t), GFP_KERNEL);
	if (!temp_info_data) {
		E("%s: allocate memory failed!\n", __func__);
		return;
	}
	/*
	 *1. Read number of MKey R100070E8H to determin data size
	 * m_key_num = ic_data->HX_BT_NUM;
	 * I("%s,m_key_num=%d\n",__func__ ,m_key_num);
	 * total_size += m_key_num * 2;
	 *2. Start DSRAM Rawdata and Wait Data Ready
	 *=====tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00;
	 */
	tmp_data[3] = 0x00; tmp_data[2] = 0x00;
	tmp_data[1] = psram_op->passwrd_start[1];
	tmp_data[0] = psram_op->passwrd_start[0];
	fw_run_flag = himax_write_read_reg(psram_op->addr_rawdata_addr, tmp_data, psram_op->passwrd_end[1], psram_op->passwrd_end[0]);

	if (fw_run_flag < 0) {
		I("%s Data NOT ready => bypass\n", __func__);
		kfree(temp_info_data);
		return;
	}

	/* 3. Read RawData */
	total_size_temp = total_size;
	/* ====tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00; */
	I("%s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X,tmp_data[3] = 0x%02X\n",
	  __func__, psram_op->addr_rawdata_addr[0], psram_op->addr_rawdata_addr[1], psram_op->addr_rawdata_addr[2], psram_op->addr_rawdata_addr[3]);
	tmp_addr[0] = psram_op->addr_rawdata_addr[0];
	tmp_addr[1] = psram_op->addr_rawdata_addr[1];
	tmp_addr[2] = psram_op->addr_rawdata_addr[2];
	tmp_addr[3] = psram_op->addr_rawdata_addr[3];

	if (total_size % max_i2c_size == 0)
		total_read_times = total_size / max_i2c_size;
	else
		total_read_times = total_size / max_i2c_size + 1;

	for (i = 0; i < total_read_times; i++) {
		address = (psram_op->addr_rawdata_addr[3] << 24) +
		(psram_op->addr_rawdata_addr[2] << 16) +
		(psram_op->addr_rawdata_addr[1] << 8) +
		psram_op->addr_rawdata_addr[0] + i * max_i2c_size;
		I("%s address = %08X\n", __func__, address);

		tmp_addr[3] = (uint8_t)((address >> 24) & 0x00FF);
		tmp_addr[2] = (uint8_t)((address >> 16) & 0x00FF);
		tmp_addr[1] = (uint8_t)((address >> 8) & 0x00FF);
		tmp_addr[0] = (uint8_t)((address) & 0x00FF);

		if (total_size_temp >= max_i2c_size) {
			g_core_fp.fp_register_read(tmp_addr, max_i2c_size, &temp_info_data[i * max_i2c_size], 0);
			total_size_temp = total_size_temp - max_i2c_size;
		} else {
			/* I("last total_size_temp=%d\n",total_size_temp); */
			g_core_fp.fp_register_read(tmp_addr, total_size_temp % max_i2c_size, &temp_info_data[i * max_i2c_size], 0);
		}
	}

	/* 4. FW stop outputing */
	/* I("DSRAM_Flag=%d\n",DSRAM_Flag); */
	if (DSRAM_Flag == false) {
		/*
		 * I("Return to Event Stack!\n");
		 * ====tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00;
		 * ====tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
		 */
		g_core_fp.fp_flash_write_burst(psram_op->addr_rawdata_addr, psram_op->data_fin);
	} else {
		/*
		 * I("Continue to SRAM!\n");
		 * =====tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x00;
		 * =====tmp_data[3] = 0x11; tmp_data[2] = 0x22; tmp_data[1] = 0x33; tmp_data[0] = 0x44;
		 */
		g_core_fp.fp_flash_write_burst(psram_op->addr_rawdata_addr, psram_op->data_conti);
	}

	/* 5. Data Checksum Check */
	for (i = 2; i < total_size; i += 2) /* 2:PASSWORD NOT included */
		check_sum_cal += (temp_info_data[i + 1] * 256 + temp_info_data[i]);

	if (check_sum_cal % 0x10000 != 0) {
		I("%s check_sum_cal fail=%2X\n", __func__, check_sum_cal);
		kfree(temp_info_data);
		return;
	}

	memcpy(info_data, &temp_info_data[4], mutual_data_size * sizeof(uint8_t));
	/* I("%s checksum PASS\n", __func__); */

	kfree(temp_info_data);
}
/* SRAM side end */
#endif

#ifdef CORE_DRIVER
static bool himax_mcu_detect_ic(void)
{
	D("%s: use default incell detect.\n", __func__);

	return 0;
}


static void himax_mcu_init_ic(void)
{
	D("%s: use default incell init.\n", __func__);
}


#ifdef HX_RST_PIN_FUNC
static void himax_mcu_pin_reset(void)
{
	D("%s: Now reset the Touch chip.\n", __func__);
	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	msleep(20);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	msleep(50);
}

static void himax_mcu_ic_reset(uint8_t loadconfig, uint8_t int_off)
{
	struct himax_ts_data *ts = private_ts;

	HX_HW_RESET_ACTIVATE = 1;
	D("%s, status: loadconfig=%d, int_off=%d\n",
		__func__, loadconfig, int_off);

	if (ts->rst_gpio >= 0) {
		if (int_off)
			g_core_fp.fp_irq_switch(0);

		g_core_fp.fp_pin_reset();

		if (loadconfig)
			g_core_fp.fp_reload_config();

		if (int_off)
			g_core_fp.fp_irq_switch(1);
	}
}
#endif

static void himax_mcu_touch_information(void)
{
#ifndef HX_FIX_TOUCH_INFO
	char data[EIGHT_BYTE_DATA_SZ] = {0};

	/* cmd[3] = 0x10; cmd[2] = 0x00; cmd[1] = 0x70; cmd[0] = 0xF4; */
	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_rxnum_txnum_maxpt, EIGHT_BYTE_DATA_SZ, data, 0);
	ic_data->HX_RX_NUM = data[2];
	ic_data->HX_TX_NUM = data[3];
	ic_data->HX_MAX_PT = data[4];
	/*
	 * I("%s : HX_RX_NUM=%d,ic_data->HX_TX_NUM=%d,ic_data->HX_MAX_PT=%d\n",__func__,ic_data->HX_RX_NUM,ic_data->HX_TX_NUM,ic_data->HX_MAX_PT);
	 * cmd[3] = 0x10; cmd[2] = 0x00; cmd[1] = 0x70; cmd[0] = 0xFA;
	 */
	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_xy_res_enable, FOUR_BYTE_DATA_SZ, data, 0);

	/* I("%s : c_data->HX_XY_REVERSE=0x%2.2X\n",__func__,data[1]); */
	if ((data[1] & 0x04) == 0x04)
		ic_data->HX_XY_REVERSE = true;
	else
		ic_data->HX_XY_REVERSE = false;

	/* cmd[3] = 0x10; cmd[2] = 0x00; cmd[1] = 0x70; cmd[0] = 0xFC; */
	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_x_y_res, FOUR_BYTE_DATA_SZ, data, 0);
	ic_data->HX_Y_RES = data[0] * 256 + data[1];
	ic_data->HX_X_RES = data[2] * 256 + data[3];
	/*
	 * I("%s : ic_data->HX_Y_RES=%d,ic_data->HX_X_RES=%d\n",__func__,ic_data->HX_Y_RES,ic_data->HX_X_RES);
	 *	cmd[3] = 0x10; cmd[2] = 0x00; cmd[1] = 0x70; cmd[0] = 0x89;
	 */
	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_int_is_edge, FOUR_BYTE_DATA_SZ, data, 0);

	/*
	 * I("%s : data[0]=0x%2.2X,data[1]=0x%2.2X,data[2]=0x%2.2X,data[3]=0x%2.2X\n",__func__,data[0],data[1],data[2],data[3]);
	 * I("data[0] & 0x01 = %d\n",(data[0] & 0x01));
	 */
	if ((data[0] & 0x01) == 1)
		ic_data->HX_INT_IS_EDGE = true;
	else
		ic_data->HX_INT_IS_EDGE = false;

	if (ic_data->HX_RX_NUM > 40)
		ic_data->HX_RX_NUM = 32;


	if (ic_data->HX_TX_NUM > 20)
		ic_data->HX_TX_NUM = 18;

	if (ic_data->HX_MAX_PT > 10)
		ic_data->HX_MAX_PT = 10;

	if (ic_data->HX_Y_RES > 2000)
		ic_data->HX_Y_RES = 1280;

	if (ic_data->HX_X_RES > 2000)
		ic_data->HX_X_RES = 720;

	/* 1. Read number of MKey R100070E8H to determin data size */
	/* ====tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x70; tmp_addr[0] = 0xE8; */
	g_core_fp.fp_register_read(psram_op->addr_mkey, FOUR_BYTE_DATA_SZ, data, 0);
	/*
	 * I("%s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X,tmp_data[3] = 0x%02X\n",
	 *	__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
	 */
	ic_data->HX_BT_NUM = data[0] & 0x03;
#else
	ic_data->HX_RX_NUM = FIX_HX_RX_NUM;
	ic_data->HX_TX_NUM = FIX_HX_TX_NUM;
	ic_data->HX_BT_NUM = FIX_HX_BT_NUM;
	ic_data->HX_X_RES = FIX_HX_X_RES;
	ic_data->HX_Y_RES = FIX_HX_Y_RES;
	ic_data->HX_MAX_PT = FIX_HX_MAX_PT;
	ic_data->HX_XY_REVERSE = FIX_HX_XY_REVERSE;
	ic_data->HX_INT_IS_EDGE = FIX_HX_INT_IS_EDGE;
#endif
	D("%s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d\n", __func__,
		ic_data->HX_RX_NUM, ic_data->HX_TX_NUM, ic_data->HX_MAX_PT);
	D("%s:HX_XY_REVERSE =%d,HX_Y_RES =%d,HX_X_RES=%d\n", __func__,
		ic_data->HX_XY_REVERSE, ic_data->HX_Y_RES, ic_data->HX_X_RES);
	D("%s:HX_INT_IS_EDGE =%d\n", __func__, ic_data->HX_INT_IS_EDGE);
}

static void himax_mcu_reload_config(void)
{
	if (himax_report_data_init())
		E("%s: allocate data fail\n", __func__);

	g_core_fp.fp_sense_on(0x00);
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

static int himax_mcu_cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max)
{
	int RawDataLen;

	if (raw_cnt_rmd != 0x00)
		RawDataLen = MAX_I2C_TRANS_SZ - ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 1;
	else
		RawDataLen = MAX_I2C_TRANS_SZ - ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 1;

	return RawDataLen;
}

static bool himax_mcu_diag_check_sum(struct himax_report_data *hx_touch_data)
{
	uint16_t check_sum_cal = 0;
	int i;

	/* Check 128th byte CRC */
	for (i = 0, check_sum_cal = 0; i < (hx_touch_data->touch_all_size - hx_touch_data->touch_info_size); i += 2)
		check_sum_cal += (hx_touch_data->hx_rawdata_buf[i + 1] * FLASH_RW_MAX_LEN + hx_touch_data->hx_rawdata_buf[i]);

	if (check_sum_cal % HX64K != 0) {
		I("%s fail=%2X\n", __func__, check_sum_cal);
		return 0;
	}

	return 1;
}

static void himax_mcu_diag_parse_raw_data(struct himax_report_data *hx_touch_data, int mul_num, int self_num, uint8_t diag_cmd, int32_t *mutual_data, int32_t *self_data)
{
	diag_mcu_parse_raw_data(hx_touch_data, mul_num, self_num, diag_cmd, mutual_data, self_data);
}

#ifdef HX_ESD_RECOVERY
static int himax_mcu_ic_esd_recovery(int hx_esd_event, int hx_zero_event, int length)
{
	int ret_val = NO_ERR;

	if (g_zero_event_count > 5) {
		g_zero_event_count = 0;
		I("[HIMAX TP MSG]: ESD event checked - ALL Zero.\n");
		ret_val = HX_ESD_EVENT;
		goto END_FUNCTION;
	}

	if (hx_esd_event == length) {
		g_zero_event_count = 0;
		ret_val = HX_ESD_EVENT;
		goto END_FUNCTION;
	} else if (hx_zero_event == length) {
		g_zero_event_count++;
		I("[HIMAX TP MSG]: ALL Zero event is %d times.\n", g_zero_event_count);
		ret_val = HX_ZERO_EVENT_COUNT;
		goto END_FUNCTION;
	}

END_FUNCTION:
	return ret_val;
}

static void himax_mcu_esd_ic_reset(void)
{
	HX_ESD_RESET_ACTIVATE = 1;
#ifdef HX_RST_PIN_FUNC
	himax_mcu_pin_reset();
#endif
	I("%s:\n", __func__);
}
#endif
#endif

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) || defined(HX_USB_DETECT_GLOBAL)
static void himax_mcu_resend_cmd_func(bool suspended)
{
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE)
	struct himax_ts_data *ts = private_ts;
#endif
#ifdef HX_SMART_WAKEUP
	g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable, suspended);
#endif
#ifdef HX_HIGH_SENSE
	g_core_fp.fp_set_HSEN_enable(ts->HSEN_enable, suspended);
#endif
#ifdef HX_USB_DETECT_GLOBAL
	himax_cable_detect_func(true);
#endif
}
#endif

#ifdef HX_ZERO_FLASH
int G_POWERONOF = 1;
void himax_mcu_sys_reset(void)
{
	/* 0x10007f00 -> 0x00009AA9 //Disable Flash Reload */
	g_core_fp.fp_flash_write_burst(pzf_op->addr_dis_flash_reload,  pzf_op->data_dis_flash_reload);
	msleep(20);
	g_core_fp.fp_register_write(pzf_op->addr_system_reset,  4,  pzf_op->data_system_reset,  false);
}
void himax_mcu_clean_sram_0f(uint8_t *addr, int write_len, int type)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;

	uint8_t fix_data = 0x00;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[MAX_I2C_TRANS_SZ] = {0};

	I("%s, Entering\n",  __func__);

	total_size = write_len;

	if (total_size > 4096)
		max_bus_size = 4096;

	total_size_temp = write_len;

	g_core_fp.fp_burst_enable(1);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s, write addr tmp_addr[3]=0x%2.2X, tmp_addr[2]=0x%2.2X, tmp_addr[1]=0x%2.2X, tmp_addr[0]=0x%2.2X\n",
		__func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

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


	I("%s, total size=%d\n",  __func__, total_size);

	if (total_size_temp % max_bus_size == 0)
		total_read_times = total_size_temp / max_bus_size;
	else
		total_read_times = total_size_temp / max_bus_size + 1;

	for (i = 0; i < (total_read_times); i++) {
		I("[log]write %d time start!\n", i);
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_flash_write_burst_length(tmp_addr, tmp_data,  max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			I("last total_size_temp=%d\n", total_size_temp);
			g_core_fp.fp_flash_write_burst_length(tmp_addr, tmp_data,  total_size_temp % max_bus_size);
		}
		address = ((i+1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		msleep(20);
	}

	I("%s, END\n",  __func__);
}

void himax_mcu_write_sram_0f(const struct firmware *fw_entry, uint8_t *addr, int start_index, uint32_t write_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;

	uint8_t tmp_addr[4];
	uint8_t *tmp_data;
	uint32_t now_addr;

	I("%s, ---Entering\n",  __func__);

	total_size = fw_entry->size;

	total_size_temp = write_len;

	if (write_len > 4096)
		max_bus_size = 4096;
	else
		max_bus_size = write_len;

	g_core_fp.fp_burst_enable(1);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s, write addr tmp_addr[3]=0x%2.2X, tmp_addr[2]=0x%2.2X, tmp_addr[1]=0x%2.2X, tmp_addr[0]=0x%2.2X\n",
		__func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);
	now_addr = (addr[3] << 24) + (addr[2] << 16) + (addr[1] << 8) + addr[0];
	I("now addr= 0x%08X\n", now_addr);

	I("%s, total size=%d\n",  __func__, total_size);


	tmp_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (!tmp_data) {
		E("%s: allocate memory failed!\n", __func__);
		return;
	}
	memcpy(tmp_data, fw_entry->data, total_size);

	/*
	 * for(i = 0;i<10;i++)
	 * {
	 *	I("[%d] 0x%2.2X", i, tmp_data[i]);
	 * }
	 * I("\n");
	 */
	if (total_size_temp % max_bus_size == 0)
		total_read_times = total_size_temp / max_bus_size;
	else
		total_read_times = total_size_temp / max_bus_size + 1;

	for (i = 0; i < (total_read_times); i++) {
		I("[log]write %d time start!\n", i);
		I("[log]addr[3]=0x%02X, addr[2]=0x%02X, addr[1]=0x%02X, addr[0]=0x%02X!\n", tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_flash_write_burst_length(tmp_addr, &(tmp_data[start_index+i * max_bus_size]),  max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			I("last total_size_temp=%d\n", total_size_temp);
			g_core_fp.fp_flash_write_burst_length(tmp_addr, &(tmp_data[start_index+i * max_bus_size]),  total_size_temp % max_bus_size);
		}

		I("[log]write %d time end!\n", i);
		address = ((i+1) * max_bus_size);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		if (tmp_addr[0] <  addr[0])
			tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF) + 1;
		else
			tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF);

		udelay(100);
	}
	I("%s, ----END\n",  __func__);
	kfree(tmp_data);
}

void himax_mcu_firmware_update_0f(const struct firmware *fw_entry)
{
	int retry = 0;
	int crc = -1;

	I("%s, Entering\n",  __func__);

	g_core_fp.fp_register_write(pzf_op->addr_system_reset,  4,  pzf_op->data_system_reset,  false);

	g_core_fp.fp_sense_off();

	/* first 48K */
	do {
		g_core_fp.fp_write_sram_0f(fw_entry, pzf_op->data_sram_start_addr, 0, HX_48K_SZ);
		crc = g_core_fp.fp_check_CRC(pzf_op->data_sram_start_addr,  HX_48K_SZ);
		if (crc == 0) {
			I("%s, HW CRC OK in %d time\n",  __func__, retry);
			break;
		}
		E("%s, HW CRC FAIL in %d time !\n",  __func__, retry);

		retry++;
	} while (crc != 0 && retry < 3);

	if (crc != 0) {
		E("Last time CRC Fail!\n");
		return;
	}

	/* clean */
	if (G_POWERONOF == 1)
		g_core_fp.fp_clean_sram_0f(pzf_op->data_sram_clean, HX_32K_SZ, 0);

	/* last 16k */
	/* config info */
	if (G_POWERONOF == 1)
		g_core_fp.fp_write_sram_0f(fw_entry, pzf_op->data_cfg_info, HX_48K_SZ, 132);
	else
		g_core_fp.fp_clean_sram_0f(pzf_op->data_cfg_info, 132, 2);

	/* FW config */
	if (G_POWERONOF == 1)
		g_core_fp.fp_write_sram_0f(fw_entry, pzf_op->data_fw_cfg, 0xC0FE, 512);
	else
		g_core_fp.fp_clean_sram_0f(pzf_op->data_fw_cfg, 512, 1);

	/* ADC config */
	if (G_POWERONOF == 1)
		g_core_fp.fp_write_sram_0f(fw_entry, pzf_op->data_adc_cfg_1, 0xD000, 376);
	else
		g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_1, 376, 2);

	if (G_POWERONOF == 1)
		g_core_fp.fp_write_sram_0f(fw_entry, pzf_op->data_adc_cfg_2, 0xD178, 376);
	else
		g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_2, 376, 2);

	if (G_POWERONOF == 1)
		g_core_fp.fp_write_sram_0f(fw_entry, pzf_op->data_adc_cfg_3, 0xD000, 376);
	else
		g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_3, 376, 2);

	msleep(20);
	g_core_fp.fp_sys_reset();
	msleep(20);
	I("%s:End\n",  __func__);
	himax_int_enable(1);

	I("%s, END\n",  __func__);
}

void himax_mcu_0f_operation(struct work_struct *work)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;
	char *firmware_name = "himax.bin";

	I("%s, Entering\n",  __func__);
	I("file name = %s\n", firmware_name);
	err = request_firmware(&fw_entry,  firmware_name,  private_ts->dev);
	if (err < 0) {
		E("%s, fail in line%d error code=%d\n",  __func__,  __LINE__,  err);
		return;
	}
	g_core_fp.fp_firmware_update_0f(fw_entry);
	release_firmware(fw_entry);

	I("%s, END\n",  __func__);
}

#ifdef HX_0F_DEBUG
void himax_mcu_read_sram_0f(const struct firmware *fw_entry, uint8_t *addr, int start_index, int read_len)
{
	int total_read_times = 0;
	int max_i2c_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0, j = 0;
	int not_same = 0;

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data;
	int *not_same_buff;

	I("%s, Entering\n",  __func__);

	g_core_fp.fp_burst_enable(1);
	total_size = read_len;
	total_size_temp = read_len;
	temp_info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (!temp_info_data) {
		E("%s: allocate memory failed: temp_info_data!\n", __func__);
		return;
	}
	not_same_buff = kcalloc(total_size, sizeof(int), GFP_KERNEL);
	if (!not_same_buff) {
		kfree(temp_info_data);
		E("%s: allocate memory failed: not_same_buff!\n", __func__);
		return;
	}

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s, read addr tmp_addr[3]=0x%2.2X, tmp_addr[2]=0x%2.2X, tmp_addr[1]=0x%2.2X, tmp_addr[0]=0x%2.2X\n",
		__func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

	I("%s, total size=%d\n",  __func__, total_size);

	g_core_fp.fp_burst_enable(1);

	if (total_size % max_i2c_size == 0)
		total_read_times = total_size / max_i2c_size;
	else
		total_read_times = total_size / max_i2c_size + 1;

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_i2c_size) {
			g_core_fp.fp_register_read(tmp_addr, max_i2c_size, &temp_info_data[i*max_i2c_size], false);
			total_size_temp = total_size_temp - max_i2c_size;
		} else
			g_core_fp.fp_register_read(tmp_addr, total_size_temp % max_i2c_size, &temp_info_data[i*max_i2c_size], false);

		address = ((i+1) * max_i2c_size);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);
		if (tmp_addr[0] <  addr[0])
			tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF) + 1;
		else
			tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF);

		msleep(20);
	}
	I("%s, READ Start\n",  __func__);
	I("%s, start_index = %d\n",  __func__, start_index);
	j = start_index;
	for (i = 0; i < read_len; i++, j++) {
		if (fw_entry->data[j] != temp_info_data[i]) {
			not_same++;
			not_same_buff[i] = 1;
		}

		I("0x%2.2X, ", temp_info_data[i]);

		if (i > 0 && i%16 == 15)
			I("\n");
	}
	I("%s, READ END\n",  __func__);
	I("%s, Not Same count=%d\n",  __func__, not_same);
	if (not_same != 0) {
		j = start_index;
		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1)
				I("bin = [%d] 0x%2.2X\n", i, fw_entry->data[j]);
		}
		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1)
				I("sram = [%d] 0x%2.2X\n", i, temp_info_data[i]);
		}
	}
	I("%s, READ END\n",  __func__);
	I("%s, Not Same count=%d\n",  __func__, not_same);
	I("%s, END\n",  __func__);

	kfree(not_same_buff);
	kfree(temp_info_data);
}

void himax_mcu_read_all_sram(uint8_t *addr, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;
	/*
	 * struct file *fn;
	 * struct filename *vts_name;
	 */

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data;

	I("%s, Entering\n",  __func__);

	g_core_fp.fp_burst_enable(1);
	total_size = read_len;
	total_size_temp = read_len;
	temp_info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (!temp_info_data) {
		E("%s: allocate memory failed!\n", __func__);
		return;
	}

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s, read addr tmp_addr[3]=0x%2.2X, tmp_addr[2]=0x%2.2X, tmp_addr[1]=0x%2.2X, tmp_addr[0]=0x%2.2X\n",
		__func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

	I("%s, total size=%d\n",  __func__, total_size);

	if (total_size % max_bus_size == 0)
		total_read_times = total_size / max_bus_size;
	else
		total_read_times = total_size / max_bus_size + 1;

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size)
			g_core_fp.fp_register_read(tmp_addr,  max_bus_size,  &temp_info_data[i*max_bus_size],  false);
			total_size_temp = total_size_temp - max_bus_size;
		else
			g_core_fp.fp_register_read(tmp_addr,  total_size_temp % max_bus_size,  &temp_info_data[i*max_bus_size],  false);

		address = ((i+1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		msleep(20);
	}
	I("%s, NOW addr tmp_addr[3]=0x%2.2X, tmp_addr[2]=0x%2.2X, tmp_addr[1]=0x%2.2X, tmp_addr[0]=0x%2.2X\n",
		__func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);
	/*
	 * for(i = 0;i<read_len;i++)
	 * {
	 *	I("0x%2.2X, ", temp_info_data[i]);
	 *	if (i > 0 && i%16 == 15)
	 *		printk("\n");
	 * }
	 */

	/* need modify */
	/*
	 * I("Now Write File start!\n");
	 * vts_name = getname_kernel("/sdcard/dump_dsram.txt");
	 * fn = file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
	 * if (!IS_ERR (fn)) {
	 *	I("%s create file and ready to write\n",  __func__);
	 *	fn->f_op->write(fn, temp_info_data, read_len*sizeof(uint8_t), &fn->f_pos);
	 *	filp_close(fn, NULL);
	 * }
	 * I("Now Write File End!\n");
	 */

	I("%s, END\n",  __func__);

	kfree(temp_info_data);
}

void himax_mcu_firmware_read_0f(const struct firmware *fw_entry, int type)
{
	uint8_t tmp_addr[4];

	I("%s, Entering\n",  __func__);
	if (type == 0) {
		/* first 48K */
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_sram_start_addr, 0, HX_48K_SZ);
		g_core_fp.fp_read_all_sram(tmp_addr, 0xC000);
	} else {
		/* last 16k */
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_cfg_info, 0xC000, 132);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_fw_cfg, 0xC0FE, 512);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_adc_cfg_1, 0xD000, 376);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_adc_cfg_2, 0xD178, 376);
		g_core_fp.fp_read_sram_0f(fw_entry, pzf_op->data_adc_cfg_3, 0xD000, 376);
		g_core_fp.fp_read_all_sram(pzf_op->data_sram_clean, HX_32K_SZ);
	}
	I("%s, END\n",  __func__);
}

void himax_mcu_0f_operation_check(int type)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;
	char *firmware_name = "himax.bin";

	I("%s, Entering\n",  __func__);
	I("file name = %s\n", firmware_name);

	err = request_firmware(&fw_entry,  firmware_name,  private_ts->dev);
	if (err < 0) {
		E("%s, fail in line%d error code=%d\n",  __func__,  __LINE__,  err);
		return;
	}

	I("first 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n", fw_entry->data[0], fw_entry->data[1], fw_entry->data[2], fw_entry->data[3]);
	I("next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n", fw_entry->data[4], fw_entry->data[5], fw_entry->data[6], fw_entry->data[7]);
	I("and next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n", fw_entry->data[8], fw_entry->data[9], fw_entry->data[10], fw_entry->data[11]);

	g_core_fp.fp_firmware_read_0f(fw_entry, type);

	release_firmware(fw_entry);
	I("%s, END\n",  __func__);

}
#endif

#endif

#ifdef CORE_INIT
/* init start */
static void himax_mcu_fp_init(void)
{
#ifdef CORE_IC
	g_core_fp.fp_burst_enable = himax_mcu_burst_enable;
	g_core_fp.fp_register_read = himax_mcu_register_read;
	g_core_fp.fp_flash_write_burst = himax_mcu_flash_write_burst;
	g_core_fp.fp_flash_write_burst_length = himax_mcu_flash_write_burst_length;
	g_core_fp.fp_register_write = himax_mcu_register_write;
	g_core_fp.fp_interface_on = himax_mcu_interface_on;
	g_core_fp.fp_sense_on = himax_mcu_sense_on;
	g_core_fp.fp_sense_off = himax_mcu_sense_off;
	g_core_fp.fp_wait_wip = himax_mcu_wait_wip;
	g_core_fp.fp_init_psl = himax_mcu_init_psl;
	g_core_fp.fp_resume_ic_action = himax_mcu_resume_ic_action;
	g_core_fp.fp_suspend_ic_action = himax_mcu_suspend_ic_action;
	g_core_fp.fp_power_on_init = himax_mcu_power_on_init;
#endif
#ifdef CORE_FW
	g_core_fp.fp_system_reset = himax_mcu_system_reset;
	g_core_fp.fp_Calculate_CRC_with_AP = himax_mcu_Calculate_CRC_with_AP;
	g_core_fp.fp_check_CRC = himax_mcu_check_CRC;
	g_core_fp.fp_set_reload_cmd = himax_mcu_set_reload_cmd;
	g_core_fp.fp_program_reload = himax_mcu_program_reload;
	g_core_fp.fp_set_SMWP_enable = himax_mcu_set_SMWP_enable;
	g_core_fp.fp_set_HSEN_enable = himax_mcu_set_HSEN_enable;
	g_core_fp.fp_usb_detect_set = himax_mcu_usb_detect_set;
	g_core_fp.fp_diag_register_set = himax_mcu_diag_register_set;
	g_core_fp.fp_chip_self_test = himax_mcu_chip_self_test;
	g_core_fp.fp_idle_mode = himax_mcu_idle_mode;
	g_core_fp.fp_reload_disable = himax_mcu_reload_disable;
	g_core_fp.fp_check_chip_version = himax_mcu_check_chip_version;
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
	g_core_fp.fp_switch_mode = himax_mcu_switch_mode;
	g_core_fp.fp_read_DD_status = himax_mcu_read_DD_status;
#endif
#ifdef CORE_FLASH
	g_core_fp.fp_chip_erase = himax_mcu_chip_erase;
	g_core_fp.fp_block_erase = himax_mcu_block_erase;
	g_core_fp.fp_sector_erase = himax_mcu_sector_erase;
	g_core_fp.fp_flash_programming = himax_mcu_flash_programming;
	g_core_fp.fp_flash_page_write = himax_mcu_flash_page_write;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k = himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_32k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k = himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_60k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k = himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_64k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k = himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_124k;
	g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k = himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs_128k;
	g_core_fp.fp_flash_dump_func = himax_mcu_flash_dump_func;
	g_core_fp.fp_flash_lastdata_check = himax_mcu_flash_lastdata_check;
#endif
#ifdef CORE_SRAM
	g_core_fp.fp_sram_write = himax_mcu_sram_write;
	g_core_fp.fp_sram_verify = himax_mcu_sram_verify;
	g_core_fp.fp_get_DSRAM_data = himax_mcu_get_DSRAM_data;
#endif
#ifdef CORE_DRIVER
	g_core_fp.fp_chip_detect = himax_mcu_detect_ic;
	g_core_fp.fp_chip_init = himax_mcu_init_ic;
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_pin_reset = himax_mcu_pin_reset;
	g_core_fp.fp_ic_reset = himax_mcu_ic_reset;
#endif
	g_core_fp.fp_touch_information = himax_mcu_touch_information;
	g_core_fp.fp_reload_config = himax_mcu_reload_config;
	g_core_fp.fp_get_touch_data_size = himax_mcu_get_touch_data_size;
	g_core_fp.fp_hand_shaking = himax_mcu_hand_shaking;
	g_core_fp.fp_determin_diag_rawdata = himax_mcu_determin_diag_rawdata;
	g_core_fp.fp_determin_diag_storage = himax_mcu_determin_diag_storage;
	g_core_fp.fp_cal_data_len = himax_mcu_cal_data_len;
	g_core_fp.fp_diag_check_sum = himax_mcu_diag_check_sum;
	g_core_fp.fp_diag_parse_raw_data = himax_mcu_diag_parse_raw_data;
#ifdef HX_ESD_RECOVERY
	g_core_fp.fp_ic_esd_recovery = himax_mcu_ic_esd_recovery;
	g_core_fp.fp_esd_ic_reset = himax_mcu_esd_ic_reset;
#endif
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) || defined(HX_USB_DETECT_GLOBAL)
	g_core_fp.fp_resend_cmd_func = himax_mcu_resend_cmd_func;
#endif
#endif
#ifdef HX_ZERO_FLASH
	g_core_fp.fp_sys_reset = himax_mcu_sys_reset;
	g_core_fp.fp_clean_sram_0f = himax_mcu_clean_sram_0f;
	g_core_fp.fp_write_sram_0f = himax_mcu_write_sram_0f;
	g_core_fp.fp_firmware_update_0f = himax_mcu_firmware_update_0f;
	g_core_fp.fp_0f_operation = himax_mcu_0f_operation;
#ifdef HX_0F_DEBUG
	g_core_fp.fp_read_sram_0f = himax_mcu_read_sram_0f;
	g_core_fp.fp_read_all_sram = himax_mcu_read_all_sram;
	g_core_fp.fp_firmware_read_0f = himax_mcu_firmware_read_0f;
	g_core_fp.fp_0f_operation_check = himax_mcu_0f_operation_check;
#endif
#endif
}

void himax_mcu_in_cmd_struct_init(void)
{
	D("%s: Entering!\n", __func__);
	g_core_cmd_op = kzalloc(sizeof(struct himax_core_command_operation), GFP_KERNEL);
	if (!g_core_cmd_op)
		return;

	g_core_cmd_op->ic_op = kzalloc(sizeof(struct ic_operation), GFP_KERNEL);
	if (!g_core_cmd_op->ic_op) {
		E("%s: allocate memory failed: g_core_cmd_op->ic_op!\n", __func__);
		goto err_alloc_ic_op;
	}

	g_core_cmd_op->fw_op = kzalloc(sizeof(struct fw_operation), GFP_KERNEL);
	if (!g_core_cmd_op->fw_op) {
		E("%s: allocate memory failed: g_core_cmd_op->fw_op!\n", __func__);
		goto err_alloc_fw_op;
	}

	g_core_cmd_op->flash_op = kzalloc(sizeof(struct flash_operation), GFP_KERNEL);
	if (!g_core_cmd_op->flash_op) {
		E("%s: allocate memory failed: g_core_cmd_op->flash_op!\n", __func__);
		goto err_alloc_flash_op;
	}

	g_core_cmd_op->sram_op = kzalloc(sizeof(struct sram_operation), GFP_KERNEL);
	if (!g_core_cmd_op->sram_op) {
		E("%s: allocate memory failed: g_core_cmd_op->sram_op!\n", __func__);
		goto err_alloc_sram_op;
	}

	g_core_cmd_op->driver_op = kzalloc(sizeof(struct driver_operation), GFP_KERNEL);
	if (!g_core_cmd_op->driver_op) {
		E("%s: allocate memory failed: g_core_cmd_op->driver_op!\n", __func__);
		goto err_alloc_driver_op;
	}

	pic_op = g_core_cmd_op->ic_op;
	pfw_op = g_core_cmd_op->fw_op;
	pflash_op = g_core_cmd_op->flash_op;
	psram_op = g_core_cmd_op->sram_op;
	pdriver_op = g_core_cmd_op->driver_op;
#ifdef HX_ZERO_FLASH
	g_core_cmd_op->zf_op = kzalloc(sizeof(struct zf_operation), GFP_KERNEL);
	if (!g_core_cmd_op->zf_op){
		E("%s: allocate memory failed: g_core_cmd_op->zf_op!\n", __func__);
		goto err_alloc_zf_op;
	}

	pzf_op = g_core_cmd_op->zf_op;
#endif
	himax_mcu_fp_init();
	return;

#ifdef HX_ZERO_FLASH
err_alloc_zf_op:
	kfree(g_core_cmd_op->driver_op);
#endif
err_alloc_driver_op:
	kfree(g_core_cmd_op->sram_op);
err_alloc_sram_op:
	kfree(g_core_cmd_op->flash_op);
err_alloc_flash_op:
	kfree(g_core_cmd_op->fw_op);
err_alloc_fw_op:
	kfree(g_core_cmd_op->ic_op);
err_alloc_ic_op:
	kfree(g_core_cmd_op);
}

/*
 *static void himax_mcu_in_cmd_struct_free(void)
 *{
 *	pic_op = NULL;
 *	pfw_op = NULL;
 *	pflash_op = NULL;
 *	psram_op = NULL;
 *	pdriver_op = NULL;
 *	kfree(g_core_cmd_op);
 *	kfree(g_core_cmd_op->ic_op);
 *	kfree(g_core_cmd_op->flash_op);
 *	kfree(g_core_cmd_op->sram_op);
 *	kfree(g_core_cmd_op->driver_op);
 *}
 */

void himax_in_parse_assign_cmd(uint32_t addr, uint8_t *cmd, int len)
{
	/* I("%s: Entering!\n", __func__); */
	switch (len) {
	case 1:
		cmd[0] = addr;
		/* I("%s: cmd[0] = 0x%02X\n", __func__, cmd[0]); */
		break;

	case 2:
		cmd[0] = addr % 0x100;
		cmd[1] = (addr >> 8) % 0x100;
		/* I("%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X\n", __func__, cmd[0], cmd[1]); */
		break;

	case 4:
		cmd[0] = addr % 0x100;
		cmd[1] = (addr >> 8) % 0x100;
		cmd[2] = (addr >> 16) % 0x100;
		cmd[3] = addr / 0x1000000;
		/*
		 * I("%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X,cmd[2] = 0x%02X,cmd[3] = 0x%02X\n",
		 *	__func__, cmd[0], cmd[1], cmd[2], cmd[3]);
		 */
		break;

	default:
		E("%s: input length fault,len = %d!", __func__, len);
	}
}

void himax_mcu_in_cmd_init(void)
{
	D("%s: Entering!\n", __func__);
#ifdef CORE_IC
	himax_in_parse_assign_cmd(ic_adr_ahb_addr_byte_0, pic_op->addr_ahb_addr_byte_0, sizeof(pic_op->addr_ahb_addr_byte_0));
	himax_in_parse_assign_cmd(ic_adr_ahb_rdata_byte_0, pic_op->addr_ahb_rdata_byte_0, sizeof(pic_op->addr_ahb_rdata_byte_0));
	himax_in_parse_assign_cmd(ic_adr_ahb_access_direction, pic_op->addr_ahb_access_direction, sizeof(pic_op->addr_ahb_access_direction));
	himax_in_parse_assign_cmd(ic_adr_conti, pic_op->addr_conti, sizeof(pic_op->addr_conti));
	himax_in_parse_assign_cmd(ic_adr_incr4, pic_op->addr_incr4, sizeof(pic_op->addr_incr4));
	himax_in_parse_assign_cmd(ic_adr_i2c_psw_lb, pic_op->adr_i2c_psw_lb, sizeof(pic_op->adr_i2c_psw_lb));
	himax_in_parse_assign_cmd(ic_adr_i2c_psw_ub, pic_op->adr_i2c_psw_ub, sizeof(pic_op->adr_i2c_psw_ub));
	himax_in_parse_assign_cmd(ic_cmd_ahb_access_direction_read, pic_op->data_ahb_access_direction_read, sizeof(pic_op->data_ahb_access_direction_read));
	himax_in_parse_assign_cmd(ic_cmd_conti, pic_op->data_conti, sizeof(pic_op->data_conti));
	himax_in_parse_assign_cmd(ic_cmd_incr4, pic_op->data_incr4, sizeof(pic_op->data_incr4));
	himax_in_parse_assign_cmd(ic_cmd_i2c_psw_lb, pic_op->data_i2c_psw_lb, sizeof(pic_op->data_i2c_psw_lb));
	himax_in_parse_assign_cmd(ic_cmd_i2c_psw_ub, pic_op->data_i2c_psw_ub, sizeof(pic_op->data_i2c_psw_ub));
	himax_in_parse_assign_cmd(ic_adr_tcon_on_rst, pic_op->addr_tcon_on_rst, sizeof(pic_op->addr_tcon_on_rst));
	himax_in_parse_assign_cmd(ic_addr_adc_on_rst, pic_op->addr_adc_on_rst, sizeof(pic_op->addr_adc_on_rst));
	himax_in_parse_assign_cmd(ic_adr_psl, pic_op->addr_psl, sizeof(pic_op->addr_psl));
	himax_in_parse_assign_cmd(ic_adr_cs_central_state, pic_op->addr_cs_central_state, sizeof(pic_op->addr_cs_central_state));
	himax_in_parse_assign_cmd(ic_cmd_rst, pic_op->data_rst, sizeof(pic_op->data_rst));
#endif
#ifdef CORE_FW
	himax_in_parse_assign_cmd(fw_addr_system_reset, pfw_op->addr_system_reset, sizeof(pfw_op->addr_system_reset));
	himax_in_parse_assign_cmd(fw_addr_safe_mode_release_pw, pfw_op->addr_safe_mode_release_pw, sizeof(pfw_op->addr_safe_mode_release_pw));
	himax_in_parse_assign_cmd(fw_addr_ctrl_fw, pfw_op->addr_ctrl_fw_isr, sizeof(pfw_op->addr_ctrl_fw_isr));
	himax_in_parse_assign_cmd(fw_addr_flag_reset_event, pfw_op->addr_flag_reset_event, sizeof(pfw_op->addr_flag_reset_event));
	himax_in_parse_assign_cmd(fw_addr_hsen_enable, pfw_op->addr_hsen_enable, sizeof(pfw_op->addr_hsen_enable));
	himax_in_parse_assign_cmd(fw_addr_smwp_enable, pfw_op->addr_smwp_enable, sizeof(pfw_op->addr_smwp_enable));
	himax_in_parse_assign_cmd(fw_addr_program_reload_from, pfw_op->addr_program_reload_from, sizeof(pfw_op->addr_program_reload_from));
	himax_in_parse_assign_cmd(fw_addr_program_reload_to, pfw_op->addr_program_reload_to, sizeof(pfw_op->addr_program_reload_to));
	himax_in_parse_assign_cmd(fw_addr_program_reload_page_write, pfw_op->addr_program_reload_page_write, sizeof(pfw_op->addr_program_reload_page_write));
	himax_in_parse_assign_cmd(fw_addr_raw_out_sel, pfw_op->addr_raw_out_sel, sizeof(pfw_op->addr_raw_out_sel));
	himax_in_parse_assign_cmd(fw_addr_reload_status, pfw_op->addr_reload_status, sizeof(pfw_op->addr_reload_status));
	himax_in_parse_assign_cmd(fw_addr_reload_crc32_result, pfw_op->addr_reload_crc32_result, sizeof(pfw_op->addr_reload_crc32_result));
	himax_in_parse_assign_cmd(fw_addr_reload_addr_from, pfw_op->addr_reload_addr_from, sizeof(pfw_op->addr_reload_addr_from));
	himax_in_parse_assign_cmd(fw_addr_reload_addr_cmd_beat, pfw_op->addr_reload_addr_cmd_beat, sizeof(pfw_op->addr_reload_addr_cmd_beat));
	himax_in_parse_assign_cmd(fw_addr_selftest_addr_en, pfw_op->addr_selftest_addr_en, sizeof(pfw_op->addr_selftest_addr_en));
	himax_in_parse_assign_cmd(fw_addr_criteria_addr, pfw_op->addr_criteria_addr, sizeof(pfw_op->addr_criteria_addr));
	himax_in_parse_assign_cmd(fw_addr_set_frame_addr, pfw_op->addr_set_frame_addr, sizeof(pfw_op->addr_set_frame_addr));
	himax_in_parse_assign_cmd(fw_addr_selftest_result_addr, pfw_op->addr_selftest_result_addr, sizeof(pfw_op->addr_selftest_result_addr));
	himax_in_parse_assign_cmd(fw_addr_sorting_mode_en, pfw_op->addr_sorting_mode_en, sizeof(pfw_op->addr_sorting_mode_en));
	himax_in_parse_assign_cmd(fw_addr_fw_mode_status, pfw_op->addr_fw_mode_status, sizeof(pfw_op->addr_fw_mode_status));
	himax_in_parse_assign_cmd(fw_addr_icid_addr, pfw_op->addr_icid_addr, sizeof(pfw_op->addr_icid_addr));
	himax_in_parse_assign_cmd(fw_addr_trigger_addr, pfw_op->addr_trigger_addr, sizeof(pfw_op->addr_trigger_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_ver_addr, pfw_op->addr_fw_ver_addr, sizeof(pfw_op->addr_fw_ver_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_cfg_addr, pfw_op->addr_fw_cfg_addr, sizeof(pfw_op->addr_fw_cfg_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_vendor_addr, pfw_op->addr_fw_vendor_addr, sizeof(pfw_op->addr_fw_vendor_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_state_addr, pfw_op->addr_fw_state_addr, sizeof(pfw_op->addr_fw_state_addr));
	himax_in_parse_assign_cmd(fw_addr_fw_dbg_msg_addr, pfw_op->addr_fw_dbg_msg_addr, sizeof(pfw_op->addr_fw_dbg_msg_addr));
	himax_in_parse_assign_cmd(fw_addr_chk_fw_status, pfw_op->addr_chk_fw_status, sizeof(pfw_op->addr_chk_fw_status));
	himax_in_parse_assign_cmd(fw_addr_dd_handshak_addr, pfw_op->addr_dd_handshak_addr, sizeof(pfw_op->addr_dd_handshak_addr));
	himax_in_parse_assign_cmd(fw_addr_dd_data_addr, pfw_op->addr_dd_data_addr, sizeof(pfw_op->addr_dd_data_addr));
	himax_in_parse_assign_cmd(fw_data_system_reset, pfw_op->data_system_reset, sizeof(pfw_op->data_system_reset));
	himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_active, pfw_op->data_safe_mode_release_pw_active, sizeof(pfw_op->data_safe_mode_release_pw_active));
	himax_in_parse_assign_cmd(fw_data_clear, pfw_op->data_clear, sizeof(pfw_op->data_clear));
	himax_in_parse_assign_cmd(fw_data_safe_mode_release_pw_reset, pfw_op->data_safe_mode_release_pw_reset, sizeof(pfw_op->data_safe_mode_release_pw_reset));
	himax_in_parse_assign_cmd(fw_data_program_reload_start, pfw_op->data_program_reload_start, sizeof(pfw_op->data_program_reload_start));
	himax_in_parse_assign_cmd(fw_data_program_reload_compare, pfw_op->data_program_reload_compare, sizeof(pfw_op->data_program_reload_compare));
	himax_in_parse_assign_cmd(fw_data_program_reload_break, pfw_op->data_program_reload_break, sizeof(pfw_op->data_program_reload_break));
	himax_in_parse_assign_cmd(fw_data_selftest_request, pfw_op->data_selftest_request, sizeof(pfw_op->data_selftest_request));
	himax_in_parse_assign_cmd(fw_data_criteria_aa_top, pfw_op->data_criteria_aa_top, sizeof(pfw_op->data_criteria_aa_top));
	himax_in_parse_assign_cmd(fw_data_criteria_aa_bot, pfw_op->data_criteria_aa_bot, sizeof(pfw_op->data_criteria_aa_bot));
	himax_in_parse_assign_cmd(fw_data_criteria_key_top, pfw_op->data_criteria_key_top, sizeof(pfw_op->data_criteria_key_top));
	himax_in_parse_assign_cmd(fw_data_criteria_key_bot, pfw_op->data_criteria_key_bot, sizeof(pfw_op->data_criteria_key_bot));
	himax_in_parse_assign_cmd(fw_data_criteria_avg_top, pfw_op->data_criteria_avg_top, sizeof(pfw_op->data_criteria_avg_top));
	himax_in_parse_assign_cmd(fw_data_criteria_avg_bot, pfw_op->data_criteria_avg_bot, sizeof(pfw_op->data_criteria_avg_bot));
	himax_in_parse_assign_cmd(fw_data_set_frame, pfw_op->data_set_frame, sizeof(pfw_op->data_set_frame));
	himax_in_parse_assign_cmd(fw_data_selftest_ack_hb, pfw_op->data_selftest_ack_hb, sizeof(pfw_op->data_selftest_ack_hb));
	himax_in_parse_assign_cmd(fw_data_selftest_ack_lb, pfw_op->data_selftest_ack_lb, sizeof(pfw_op->data_selftest_ack_lb));
	himax_in_parse_assign_cmd(fw_data_selftest_pass, pfw_op->data_selftest_pass, sizeof(pfw_op->data_selftest_pass));
	himax_in_parse_assign_cmd(fw_data_normal_cmd, pfw_op->data_normal_cmd, sizeof(pfw_op->data_normal_cmd));
	himax_in_parse_assign_cmd(fw_data_normal_status, pfw_op->data_normal_status, sizeof(pfw_op->data_normal_status));
	himax_in_parse_assign_cmd(fw_data_sorting_cmd, pfw_op->data_sorting_cmd, sizeof(pfw_op->data_sorting_cmd));
	himax_in_parse_assign_cmd(fw_data_sorting_status, pfw_op->data_sorting_status, sizeof(pfw_op->data_sorting_status));
	himax_in_parse_assign_cmd(fw_data_dd_request, pfw_op->data_dd_request, sizeof(pfw_op->data_dd_request));
	himax_in_parse_assign_cmd(fw_data_dd_ack, pfw_op->data_dd_ack, sizeof(pfw_op->data_dd_ack));
	himax_in_parse_assign_cmd(fw_data_idle_dis_pwd, pfw_op->data_idle_dis_pwd, sizeof(pfw_op->data_idle_dis_pwd));
	himax_in_parse_assign_cmd(fw_data_idle_en_pwd, pfw_op->data_idle_en_pwd, sizeof(pfw_op->data_idle_en_pwd));
	himax_in_parse_assign_cmd(fw_data_rawdata_ready_hb, pfw_op->data_rawdata_ready_hb, sizeof(pfw_op->data_rawdata_ready_hb));
	himax_in_parse_assign_cmd(fw_data_rawdata_ready_lb, pfw_op->data_rawdata_ready_lb, sizeof(pfw_op->data_rawdata_ready_lb));
	himax_in_parse_assign_cmd(fw_addr_ahb_addr, pfw_op->addr_ahb_addr, sizeof(pfw_op->addr_ahb_addr));
	himax_in_parse_assign_cmd(fw_data_ahb_dis, pfw_op->data_ahb_dis, sizeof(pfw_op->data_ahb_dis));
	himax_in_parse_assign_cmd(fw_data_ahb_en, pfw_op->data_ahb_en, sizeof(pfw_op->data_ahb_en));
	himax_in_parse_assign_cmd(fw_addr_event_addr, pfw_op->addr_event_addr, sizeof(pfw_op->addr_event_addr));
	himax_in_parse_assign_cmd(fw_usb_detect_addr, pfw_op->addr_usb_detect, sizeof(pfw_op->addr_usb_detect));
#endif
#ifdef CORE_FLASH
	himax_in_parse_assign_cmd(flash_addr_spi200_trans_fmt, pflash_op->addr_spi200_trans_fmt, sizeof(pflash_op->addr_spi200_trans_fmt));
	himax_in_parse_assign_cmd(flash_addr_spi200_trans_ctrl, pflash_op->addr_spi200_trans_ctrl, sizeof(pflash_op->addr_spi200_trans_ctrl));
	himax_in_parse_assign_cmd(flash_addr_spi200_cmd, pflash_op->addr_spi200_cmd, sizeof(pflash_op->addr_spi200_cmd));
	himax_in_parse_assign_cmd(flash_addr_spi200_addr, pflash_op->addr_spi200_addr, sizeof(pflash_op->addr_spi200_addr));
	himax_in_parse_assign_cmd(flash_addr_spi200_data, pflash_op->addr_spi200_data, sizeof(pflash_op->addr_spi200_data));
	himax_in_parse_assign_cmd(flash_addr_spi200_bt_num, pflash_op->addr_spi200_bt_num, sizeof(pflash_op->addr_spi200_bt_num));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_fmt, pflash_op->data_spi200_trans_fmt, sizeof(pflash_op->data_spi200_trans_fmt));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_1, pflash_op->data_spi200_trans_ctrl_1, sizeof(pflash_op->data_spi200_trans_ctrl_1));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_2, pflash_op->data_spi200_trans_ctrl_2, sizeof(pflash_op->data_spi200_trans_ctrl_2));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_3, pflash_op->data_spi200_trans_ctrl_3, sizeof(pflash_op->data_spi200_trans_ctrl_3));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_4, pflash_op->data_spi200_trans_ctrl_4, sizeof(pflash_op->data_spi200_trans_ctrl_4));
	himax_in_parse_assign_cmd(flash_data_spi200_trans_ctrl_5, pflash_op->data_spi200_trans_ctrl_5, sizeof(pflash_op->data_spi200_trans_ctrl_5));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_1, pflash_op->data_spi200_cmd_1, sizeof(pflash_op->data_spi200_cmd_1));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_2, pflash_op->data_spi200_cmd_2, sizeof(pflash_op->data_spi200_cmd_2));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_3, pflash_op->data_spi200_cmd_3, sizeof(pflash_op->data_spi200_cmd_3));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_4, pflash_op->data_spi200_cmd_4, sizeof(pflash_op->data_spi200_cmd_4));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_5, pflash_op->data_spi200_cmd_5, sizeof(pflash_op->data_spi200_cmd_5));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_6, pflash_op->data_spi200_cmd_6, sizeof(pflash_op->data_spi200_cmd_6));
	himax_in_parse_assign_cmd(flash_data_spi200_cmd_7, pflash_op->data_spi200_cmd_7, sizeof(pflash_op->data_spi200_cmd_7));
	himax_in_parse_assign_cmd(flash_data_spi200_addr, pflash_op->data_spi200_addr, sizeof(pflash_op->data_spi200_addr));
#endif
#ifdef CORE_SRAM
	/* sram start */
	himax_in_parse_assign_cmd(sram_adr_mkey, psram_op->addr_mkey, sizeof(psram_op->addr_mkey));
	himax_in_parse_assign_cmd(sram_adr_rawdata_addr, psram_op->addr_rawdata_addr, sizeof(psram_op->addr_rawdata_addr));
	himax_in_parse_assign_cmd(sram_adr_rawdata_end, psram_op->addr_rawdata_end, sizeof(psram_op->addr_rawdata_end));
	himax_in_parse_assign_cmd(sram_cmd_conti, psram_op->data_conti, sizeof(psram_op->data_conti));
	himax_in_parse_assign_cmd(sram_cmd_fin, psram_op->data_fin, sizeof(psram_op->data_fin));
	himax_in_parse_assign_cmd(sram_passwrd_start, psram_op->passwrd_start, sizeof(psram_op->passwrd_start));
	himax_in_parse_assign_cmd(sram_passwrd_end, psram_op->passwrd_end, sizeof(psram_op->passwrd_end));
	/* sram end */
#endif
#ifdef CORE_DRIVER
	himax_in_parse_assign_cmd(driver_addr_fw_define_flash_reload, pdriver_op->addr_fw_define_flash_reload, sizeof(pdriver_op->addr_fw_define_flash_reload));
	himax_in_parse_assign_cmd(driver_addr_fw_define_2nd_flash_reload, pdriver_op->addr_fw_define_2nd_flash_reload, sizeof(pdriver_op->addr_fw_define_2nd_flash_reload));
	himax_in_parse_assign_cmd(driver_addr_fw_define_int_is_edge, pdriver_op->addr_fw_define_int_is_edge, sizeof(pdriver_op->addr_fw_define_int_is_edge));
	himax_in_parse_assign_cmd(driver_addr_fw_define_rxnum_txnum_maxpt, pdriver_op->addr_fw_define_rxnum_txnum_maxpt, sizeof(pdriver_op->addr_fw_define_rxnum_txnum_maxpt));
	himax_in_parse_assign_cmd(driver_addr_fw_define_xy_res_enable, pdriver_op->addr_fw_define_xy_res_enable, sizeof(pdriver_op->addr_fw_define_xy_res_enable));
	himax_in_parse_assign_cmd(driver_addr_fw_define_x_y_res, pdriver_op->addr_fw_define_x_y_res, sizeof(pdriver_op->addr_fw_define_x_y_res));
	himax_in_parse_assign_cmd(driver_data_fw_define_flash_reload_dis, pdriver_op->data_fw_define_flash_reload_dis, sizeof(pdriver_op->data_fw_define_flash_reload_dis));
	himax_in_parse_assign_cmd(driver_data_fw_define_flash_reload_en, pdriver_op->data_fw_define_flash_reload_en, sizeof(pdriver_op->data_fw_define_flash_reload_en));
	himax_in_parse_assign_cmd(driver_data_fw_define_rxnum_txnum_maxpt_sorting, pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting, sizeof(pdriver_op->data_fw_define_rxnum_txnum_maxpt_sorting));
	himax_in_parse_assign_cmd(driver_data_fw_define_rxnum_txnum_maxpt_normal, pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal, sizeof(pdriver_op->data_fw_define_rxnum_txnum_maxpt_normal));
#endif
#ifdef HX_ZERO_FLASH
	himax_in_parse_assign_cmd(zf_addr_dis_flash_reload, pzf_op->addr_dis_flash_reload, sizeof(pzf_op->addr_dis_flash_reload));
	himax_in_parse_assign_cmd(zf_data_dis_flash_reload, pzf_op->data_dis_flash_reload, sizeof(pzf_op->data_dis_flash_reload));
	himax_in_parse_assign_cmd(zf_addr_system_reset, pzf_op->addr_system_reset, sizeof(pzf_op->addr_system_reset));
	himax_in_parse_assign_cmd(zf_data_system_reset, pzf_op->data_system_reset, sizeof(pzf_op->data_system_reset));
	himax_in_parse_assign_cmd(zf_data_sram_start_addr, pzf_op->data_sram_start_addr, sizeof(pzf_op->data_sram_start_addr));
	himax_in_parse_assign_cmd(zf_data_sram_clean, pzf_op->data_sram_clean, sizeof(pzf_op->data_sram_clean));
	himax_in_parse_assign_cmd(zf_data_cfg_info, pzf_op->data_cfg_info, sizeof(pzf_op->data_cfg_info));
	himax_in_parse_assign_cmd(zf_data_fw_cfg, pzf_op->data_fw_cfg, sizeof(pzf_op->data_fw_cfg));
	himax_in_parse_assign_cmd(zf_data_adc_cfg_1, pzf_op->data_adc_cfg_1, sizeof(pzf_op->data_adc_cfg_1));
	himax_in_parse_assign_cmd(zf_data_adc_cfg_2, pzf_op->data_adc_cfg_2, sizeof(pzf_op->data_adc_cfg_2));
	himax_in_parse_assign_cmd(zf_data_adc_cfg_3, pzf_op->data_adc_cfg_3, sizeof(pzf_op->data_adc_cfg_3));
#endif
}

/* init end */
#endif

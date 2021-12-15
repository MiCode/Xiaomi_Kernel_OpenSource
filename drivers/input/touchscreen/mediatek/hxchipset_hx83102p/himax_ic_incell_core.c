/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#if 0//defined(HX_CODE_OVERLAY)
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

	ret = himax_bus_write(pic_op->addr_conti[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (pic_op->data_incr4[0] | auto_add_4_byte);

	ret = himax_bus_write(pic_op->addr_incr4[0], tmp_data, 1,
			HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

static int himax_mcu_register_read(uint8_t *read_addr, uint32_t read_length,
		uint8_t *read_data, uint8_t cfg_flag)
{
	uint8_t tmp_data[DATA_LEN_4];
	int i = 0;
	int address = 0;
	int ret = 0;

	/*I("%s,Entering\n",__func__);*/

	if (cfg_flag == false) {
		if (read_length > FLASH_RW_MAX_LEN) {
			E("%s: read len over %d!\n", __func__,
				FLASH_RW_MAX_LEN);
			return LENGTH_FAIL;
		}

		if (read_length > DATA_LEN_4)
			g_core_fp.fp_burst_enable(1);
		else
			g_core_fp.fp_burst_enable(0);

		address = (read_addr[3] << 24)
				+ (read_addr[2] << 16)
				+ (read_addr[1] << 8)
				+ read_addr[0];
		i = address;
		tmp_data[0] = (uint8_t)i;
		tmp_data[1] = (uint8_t)(i >> 8);
		tmp_data[2] = (uint8_t)(i >> 16);
		tmp_data[3] = (uint8_t)(i >> 24);

		ret = himax_bus_write(pic_op->addr_ahb_addr_byte_0[0],
			tmp_data, DATA_LEN_4,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

		tmp_data[0] = pic_op->data_ahb_access_direction_read[0];

		ret = himax_bus_write(pic_op->addr_ahb_access_direction[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

		ret = himax_bus_read(pic_op->addr_ahb_rdata_byte_0[0],
			read_data,
			read_length,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}

		if (read_length > DATA_LEN_4)
			g_core_fp.fp_burst_enable(0);

	} else {
		ret = himax_bus_read(read_addr[0], read_data, read_length,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}
	}
	return NO_ERR;
}

static int himax_mcu_flash_write_burst_length(uint8_t *reg_byte,
		uint8_t *write_data, uint32_t length)
{
	uint8_t *data_byte;
	int ret = 0;

	if (!g_internal_buffer) {
		E("%s: internal buffer not initialized!\n", __func__);
		return MEM_ALLOC_FAIL;
	}
	data_byte = g_internal_buffer;

	/* assign addr 4bytes */
	memcpy(data_byte, reg_byte, ADDR_LEN_4);
	/* assign data n bytes */
	memcpy(data_byte + ADDR_LEN_4, write_data, length);

	ret = himax_bus_write(pic_op->addr_ahb_addr_byte_0[0], data_byte,
			length + ADDR_LEN_4, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: xfer fail!\n", __func__);
		return I2C_FAIL;
	}

	return NO_ERR;
}

static int himax_mcu_register_write(uint8_t *write_addr, uint32_t write_length,
		uint8_t *write_data, uint8_t cfg_flag)
{
	int address;
	uint8_t tmp_addr[4];
	uint8_t *tmp_data;
	int total_read_times = 0;
	uint32_t max_bus_size = MAX_I2C_TRANS_SZ;
	uint32_t total_size_temp = 0;
	unsigned int i = 0;
	int ret = 0;

	/*I("%s,Entering\n", __func__);*/
	if (cfg_flag == 0) {
		total_size_temp = write_length;
#if defined(HX_ZERO_FLASH)
		max_bus_size = (write_length > HX_MAX_WRITE_SZ - 4)
			? (HX_MAX_WRITE_SZ - 4)
			: write_length;
#endif

		tmp_addr[3] = write_addr[3];
		tmp_addr[2] = write_addr[2];
		tmp_addr[1] = write_addr[1];
		tmp_addr[0] = write_addr[0];

		if (total_size_temp % max_bus_size == 0)
			total_read_times = total_size_temp / max_bus_size;
		else
			total_read_times = total_size_temp / max_bus_size + 1;

		if (write_length > DATA_LEN_4)
			g_core_fp.fp_burst_enable(1);
		else
			g_core_fp.fp_burst_enable(0);

		for (i = 0; i < (total_read_times); i++) {
			/* I("[log]write %d time start!\n", i);
			 * I("[log]addr[3]=0x%02X, addr[2]=0x%02X,
				addr[1]=0x%02X,	addr[0]=0x%02X!\n",
				tmp_addr[3], tmp_addr[2],
				tmp_addr[1], tmp_addr[0]);
			 * I("%s, write addr = 0x%02X%02X%02X%02X\n",
				__func__, tmp_addr[3], tmp_addr[2],
				tmp_addr[1], tmp_addr[0]);
			 */

			if (total_size_temp >= max_bus_size) {
				tmp_data = write_data+(i * max_bus_size);

				ret = himax_mcu_flash_write_burst_length(
					tmp_addr,
					tmp_data,
					max_bus_size);
				if (ret < 0) {
					I("%s: i2c access fail!\n", __func__);
					return I2C_FAIL;
				}
				total_size_temp = total_size_temp
					- max_bus_size;
			} else {
				tmp_data = write_data+(i * max_bus_size);
				/* I("last total_size_temp=%d\n",
				 *	total_size_temp % max_bus_size);
				 */
				ret = himax_mcu_flash_write_burst_length(
					tmp_addr,
					tmp_data,
					total_size_temp);
				if (ret < 0) {
					I("%s: i2c access fail!\n", __func__);
					return I2C_FAIL;
				}
			}

			/*I("[log]write %d time end!\n", i);*/
			address = ((i+1) * max_bus_size);
			tmp_addr[0] = write_addr[0]
				+ (uint8_t) ((address) & 0x00FF);

			if (tmp_addr[0] <  write_addr[0])
				tmp_addr[1] = write_addr[1]
					+ (uint8_t) ((address>>8) & 0x00FF)
					+ 1;
			else
				tmp_addr[1] = write_addr[1]
					+ (uint8_t) ((address>>8) & 0x00FF);

			udelay(100);
		}
	} else if (cfg_flag == 1) {
		ret = himax_bus_write(write_addr[0], write_data, write_length,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return I2C_FAIL;
		}
	} else
		E("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);

	return NO_ERR;
}

static int himax_write_read_reg(uint8_t *tmp_addr, uint8_t *tmp_data,
		uint8_t hb, uint8_t lb)
{
	int cnt = 0;

	I("[Start]addr:0x%02X%02X%02X%02X, write to 0x%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_data[1], tmp_data[0]);
	do {
		g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, 0);
		usleep_range(10000, 11000);
		g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
		/* I("%s:Now tmp_data[0]=0x%02X,[1]=0x%02X,
		 *	[2]=0x%02X,[3]=0x%02X\n",
		 *	__func__, tmp_data[0],
		 *	tmp_data[1], tmp_data[2], tmp_data[3]);
		 */
	} while ((tmp_data[1] != hb && tmp_data[0] != lb) && cnt++ < 100);

	I("[END]addr:0x%02X%02X%02X%02X=0x%02X%02X, expected=0x%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_data[1], tmp_data[0],
		hb, lb);

	if (cnt >= 100)
		return HX_RW_REG_FAIL;
	return NO_ERR;
}

static void himax_mcu_interface_on(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_data2[DATA_LEN_4];
	int cnt = 0;
	int ret = 0;

	/* Read a dummy register to wake up I2C.*/
	ret = himax_bus_read(pic_op->addr_ahb_rdata_byte_0[0], tmp_data,
			DATA_LEN_4, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {/* to knock I2C*/
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	do {
		tmp_data[0] = pic_op->data_conti[0];

		ret = himax_bus_write(pic_op->addr_conti[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		tmp_data[0] = pic_op->data_incr4[0];

		ret = himax_bus_write(pic_op->addr_incr4[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/*Check cmd*/
		himax_bus_read(pic_op->addr_conti[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		himax_bus_read(pic_op->addr_incr4[0], tmp_data2, 1,
				HIMAX_I2C_RETRY_TIMES);

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
			DATA_LEN_4, pflash_op->data_spi200_trans_fmt, 0);
	tmp_data[0] = 0x01;

	do {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			DATA_LEN_4,
			pflash_op->data_spi200_trans_ctrl_1,
			0);

		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
				DATA_LEN_4, pflash_op->data_spi200_cmd_1, 0);
		tmp_data[0] = tmp_data[1] = tmp_data[2] = tmp_data[3] = 0xFF;
		g_core_fp.fp_register_read(pflash_op->addr_spi200_data, 4,
				tmp_data, 0);

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
		sizeof(pfw_op->data_clear), pfw_op->data_clear, 0);
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
				DATA_LEN_4,
				tmp_data,
				0);
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

			ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);

				ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0],
					tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
			if (ret < 0)
				E("%s: i2c access fail!\n", __func__);
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

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		g_core_fp.fp_register_read(pic_op->addr_cs_central_state,
				ADDR_LEN_4, tmp_data, 0);
		I("%s: Check enter_save_mode data[0]=%X\n", __func__,
			tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst,
				DATA_LEN_4, pic_op->data_rst, 0);
			usleep_range(1000, 1100);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst,
				DATA_LEN_4, tmp_data, 0);

			g_core_fp.fp_register_write(pic_op->addr_adc_on_rst,
				DATA_LEN_4, pic_op->data_rst, 0);
			usleep_range(1000, 1100);
			tmp_data[3] = pic_op->data_rst[3];
			tmp_data[2] = pic_op->data_rst[2];
			tmp_data[1] = pic_op->data_rst[1];
			tmp_data[0] = pic_op->data_rst[0] | 0x01;
			g_core_fp.fp_register_write(pic_op->addr_adc_on_rst,
				DATA_LEN_4, tmp_data, 0);
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
	g_core_fp.fp_register_write(pic_op->addr_psl,
			sizeof(pic_op->data_rst),
			pic_op->data_rst, 0);
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
	uint8_t data[4] = {0};
	uint8_t retry = 0;

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
		sizeof(pfw_op->data_clear), pfw_op->data_clear, 0);
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);
	/*FW reload done initial*/
	g_core_fp.fp_register_write(pdriver_op->addr_fw_define_2nd_flash_reload,
		DATA_LEN_4, data, 0);

	g_core_fp.fp_sense_on(0x00);

	I("%s: waiting for FW reload done", __func__);

	while (retry++ < 30) {
		g_core_fp.fp_register_read(
			pdriver_op->addr_fw_define_2nd_flash_reload,
			DATA_LEN_4, data, 0);

		/* use all 4 bytes to compare */
		if ((data[3] == 0x00 && data[2] == 0x00 &&
			data[1] == 0x72 && data[0] == 0xC0)) {
			I("%s: FW finish reload done\n", __func__);
			break;
		}
		I("%s: wait reload done %d times\n", __func__, retry);
		g_core_fp.fp_read_FW_status();
		usleep_range(10000, 11000);
	}

#if 0
	g_core_fp.fp_read_FW_ver();
	g_core_fp.fp_touch_information();
	g_core_fp.fp_calc_touch_data_size();

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
			sizeof(pfw_op->data_clear),
			pfw_op->data_clear, 0);
	/*DSRAM func initial*/
	g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);
	/* g_core_fp.fp_sense_on(0x00); */
#endif
}

static bool himax_mcu_dd_clk_set(bool enable)
{
	uint8_t data[4] = {0};

	data[0] = (enable)?1:0;
	return (g_core_fp.fp_register_write(pic_op->adr_osc_en,
		sizeof(pic_op->adr_osc_en), data, 0) == NO_ERR);
}

static void himax_mcu_dd_reg_en(bool enable)
{
	uint8_t data[4] = {0};

	g_core_fp.fp_dd_reg_read(0xCB, 8, 1, data, 0);

	if (data[0] != 0x44) { /*need DD Touch PW*/
		data[0] = 0xA5; data[1] = 0x00;
		data[2] = 0x00;	data[3] = 0x00;
		g_core_fp.fp_register_write(pic_op->adr_osc_pw,
				DATA_LEN_4, data, 0);
		data[0] = 0x00;	data[1] = 0x55;
		data[2] = 0xAA;	data[3] = 0x00;
		g_core_fp.fp_dd_reg_write(0xEB, 0, 4, data, 0);
	}

	data[0] = 0x00;	data[1] = 0x83;
	data[2] = 0x11;	data[3] = 0x2A;
	g_core_fp.fp_dd_reg_write(0xB9, 0, 4, data, 0);
}

static bool himax_mcu_dd_reg_write(uint8_t addr, uint8_t pa_num,
		int len, uint8_t *data, uint8_t bank)
{
	/*Calculate total write length*/
	uint32_t data_len = (((len + pa_num - 1) / 4 - pa_num / 4) + 1) * 4;
	uint8_t w_data[data_len];
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	bool *chk_data;
	uint32_t chk_idx = 0;
	int i = 0;

	chk_data = kcalloc(data_len, sizeof(bool), GFP_KERNEL);
	if (chk_data == NULL) {
		E("%s Allocate chk buf failed\n", __func__);
		return false;
	}

	memset(w_data, 0, data_len * sizeof(uint8_t));

	/*put input data*/
	chk_idx = pa_num % 4;
	for (i = 0; i < len; i++) {
		w_data[chk_idx] = data[i];
		chk_data[chk_idx++] = true;
	}

	/*get original data*/
	chk_idx = (pa_num / 4) * 4;
	for (i = 0; i < data_len; i++) {
		if (!chk_data[i]) {
			g_core_fp.fp_dd_reg_read(addr,
				(uint8_t)(chk_idx + i),
				1,
				tmp_data,
				bank);

			w_data[i] = tmp_data[0];
			chk_data[i] = true;
		}
		D("%s w_data[%d] = %2X\n", __func__, i, w_data[i]);
	}

	tmp_addr[3] = 0x30;
	tmp_addr[2] = addr >> 4;
	tmp_addr[1] = (addr << 4) | (bank * 4);
	tmp_addr[0] = chk_idx;
	D("%s Addr = %02X%02X%02X%02X.\n", __func__,
			tmp_addr[3], tmp_addr[2],
			tmp_addr[1], tmp_addr[0]);
	kfree(chk_data);

	return (g_core_fp.fp_register_write(tmp_addr, data_len, w_data, 0)
			== NO_ERR);
}

static bool himax_mcu_dd_reg_read(uint8_t addr, uint8_t pa_num, int len,
		uint8_t *data, uint8_t bank)
{
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	int i = 0;

	for (i = 0; i < len; i++) {
		tmp_addr[3] = 0x30;
		tmp_addr[2] = addr >> 4;
		tmp_addr[1] = (addr << 4) | (bank * 4);
		tmp_addr[0] = pa_num + i;

		if (g_core_fp.fp_register_read(tmp_addr,
		DATA_LEN_4, tmp_data, 0))
			goto READ_FAIL;

		data[i] = tmp_data[0];

		D("%s Addr = %02X%02X%02X%02X.data = %2X\n", __func__,
			tmp_addr[3],
			tmp_addr[2],
			tmp_addr[1],
			tmp_addr[0],
			data[i]);
	}
	return true;

READ_FAIL:
	E("%s Read DD reg Failed.\n", __func__);
	return false;
}

static bool himax_mcu_ic_id_read(void)
{
	int i = 0;
	uint8_t data[4] = {0};

	g_core_fp.fp_dd_clk_set(true);
	g_core_fp.fp_dd_reg_en(true);

	for (i = 0; i < 13; i++) {
		data[0] = 0x28 + i;
		g_core_fp.fp_dd_reg_write(0xBB, 2, 1, data, 0);
		data[0] = 0x80;
		g_core_fp.fp_dd_reg_write(0xBB, 4, 1, data, 0);
		data[0] = 0x00;
		g_core_fp.fp_dd_reg_write(0xBB, 4, 1, data, 0);
		g_core_fp.fp_dd_reg_read(0xBB, 5, 1, data, 0);
		ic_data->vendor_ic_id[i] = data[0];
		I("ic_data->vendor_ic_id[%d] = %02X\n", i,
			ic_data->vendor_ic_id[i]);
	}

	g_core_fp.fp_dd_clk_set(false);

	return true;
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
					((int8_t)hx_touch_data->hx_rawdata_buf[i*2+4+1]) * 256
					+ hx_touch_data->hx_rawdata_buf[i*2+4];
			} else { /*self*/
				temp1 = i + index;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2)
					break;

				self_data[i + index - mul_num] =
					(((int8_t)hx_touch_data->hx_rawdata_buf[i*2+4+1]) << 8)
					+ hx_touch_data->hx_rawdata_buf[i*2+4];
			}
		}
	}
}

static void himax_mcu_system_reset(void)
{
#if defined(HX_PON_PIN_SUPPORT)
	g_core_fp.fp_register_write(pfw_op->addr_system_reset,
		sizeof(pfw_op->data_system_reset),
		pfw_op->data_system_reset,
		0);
#else
	int ret = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int retry = 0;

	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		sizeof(pfw_op->data_clear),
		pfw_op->data_clear,
		0);
	do {
		/* reset code*/
		/**
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 */
		tmp_data[0] = pic_op->data_i2c_psw_lb[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: i2c access fail!\n", __func__);

		/**
		 * I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 */
		tmp_data[0] = pic_op->data_i2c_psw_ub[0];

		ret = himax_bus_write(pic_op->adr_i2c_psw_ub[0],
				tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: i2c access fail!\n", __func__);

		/**
		 * I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x00
		 */
		tmp_data[0] = 0x00;

		ret = himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: i2c access fail!\n", __func__);

		usleep_range(10000, 11000);

		g_core_fp.fp_register_read(pfw_op->addr_flag_reset_event,
				DATA_LEN_4, tmp_data, 0);
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
			DATA_LEN_4, start_addr, 0);
	if (ret < NO_ERR) {
		E("%s: i2c access fail!\n", __func__);
		return HW_CRC_FAIL;
	}

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x99;
	tmp_data[1] = (length >> 8);
	tmp_data[0] = length;
	ret = g_core_fp.fp_register_write(pfw_op->addr_reload_addr_cmd_beat,
			DATA_LEN_4, tmp_data, 0);
	if (ret < NO_ERR) {
		E("%s: i2c access fail!\n", __func__);
		return HW_CRC_FAIL;
	}
	cnt = 0;

	do {
		ret = g_core_fp.fp_register_read(pfw_op->addr_reload_status,
				DATA_LEN_4, tmp_data, 0);
		if (ret < NO_ERR) {
			E("%s: i2c access fail!\n", __func__);
			return HW_CRC_FAIL;
		}

		if ((tmp_data[0] & 0x01) != 0x01) {
			ret = g_core_fp.fp_register_read(
				pfw_op->addr_reload_crc32_result,
				DATA_LEN_4,
				tmp_data,
				0);
			if (ret < NO_ERR) {
				E("%s: i2c access fail!\n", __func__);
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
		ret = himax_bus_write(pfw_op->addr_ulpm_34[0],
			pfw_op->data_ulpm_11, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_34[0],
			tmp_data, 1, HIMAX_I2C_RETRY_TIMES);
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
		ret = himax_bus_write(pfw_op->addr_ulpm_34[0],
			pfw_op->data_ulpm_11, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_34[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
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
		ret = himax_bus_write(pfw_op->addr_ulpm_33[0],
			pfw_op->data_ulpm_33, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
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
		ret = himax_bus_write(pfw_op->addr_ulpm_34[0],
			pfw_op->data_ulpm_22, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_34[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
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
		ret = himax_bus_write(pfw_op->addr_ulpm_33[0],
			pfw_op->data_ulpm_aa, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
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
		ret = himax_bus_write(pfw_op->addr_ulpm_33[0],
			pfw_op->data_ulpm_33, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
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
		ret = himax_bus_write(pfw_op->addr_ulpm_33[0],
			pfw_op->data_ulpm_aa, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(pfw_op->addr_ulpm_33[0], tmp_data, 1,
				HIMAX_I2C_RETRY_TIMES);
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
				DATA_LEN_4, tmp_data, 0);
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				back_data, 4);
		} else {
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				tmp_data,
				4);
			g_core_fp.fp_register_write(pfw_op->addr_smwp_enable,
				DATA_LEN_4,
				tmp_data,
				0);
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				back_data,
				4);
		}

		g_core_fp.fp_register_read(pfw_op->addr_smwp_enable, DATA_LEN_4,
				tmp_data, 0);
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
				DATA_LEN_4, tmp_data, 0);
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				back_data, 4);
		} else {
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				tmp_data,
				4);

			g_core_fp.fp_register_write(pfw_op->addr_hsen_enable,
				DATA_LEN_4, tmp_data, 0);

			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				back_data,
				4);
		}

		g_core_fp.fp_register_read(pfw_op->addr_hsen_enable,
			DATA_LEN_4, tmp_data, 0);
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
				DATA_LEN_4, tmp_data, 0);
			himax_parse_assign_cmd(fw_func_handshaking_pwd,
				back_data, 4);
			I("%s: USB detect status IN!\n", __func__);
		} else {
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				tmp_data,
				4);
			g_core_fp.fp_register_write(pfw_op->addr_usb_detect,
				DATA_LEN_4, tmp_data, 0);
			himax_parse_assign_cmd(
				fw_data_safe_mode_release_pw_reset,
				back_data,
				4);
			I("%s: USB detect status OUT!\n", __func__);
		}

		g_core_fp.fp_register_read(pfw_op->addr_usb_detect, DATA_LEN_4,
				tmp_data, 0);
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
			DATA_LEN_4, tmp_data, 0);
		g_core_fp.fp_register_read(pfw_op->addr_raw_out_sel,
			DATA_LEN_4, back_data, 0);
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
	g_core_fp.fp_burst_enable(1);
	g_core_fp.fp_register_write(pfw_op->addr_selftest_addr_en, DATA_LEN_4,
			pfw_op->data_selftest_request, 0);
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
			FLASH_WRITE_BURST_SZ, tmp_data, 0);
	g_core_fp.fp_register_write(pfw_op->addr_set_frame_addr,
			DATA_LEN_4, pfw_op->data_set_frame, 0);
	/*Disable IDLE Mode*/
	g_core_fp.fp_idle_mode(1);
	/*Disable Flash Reload*/
	g_core_fp.fp_reload_disable(1);
	/*start selftest // leave safe mode*/
	g_core_fp.fp_sense_on(0x01);

	/*Hand shaking*/
	for (i = 0; i < 1000; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_selftest_addr_en, 4,
				tmp_data, 0);
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
	g_core_fp.fp_register_read(pfw_op->addr_selftest_result_addr, 20,
			self_test_info, 0);
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
				DATA_LEN_4, tmp_data, 0);

		if (disable)
			switch_cmd = pfw_op->data_idle_dis_pwd[0];
		else
			switch_cmd = pfw_op->data_idle_en_pwd[0];

		tmp_data[0] = switch_cmd;
		g_core_fp.fp_register_write(pfw_op->addr_fw_mode_status,
				DATA_LEN_4, tmp_data, 0);
		g_core_fp.fp_register_read(pfw_op->addr_fw_mode_status,
				DATA_LEN_4, tmp_data, 0);

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
			DATA_LEN_4,
			pdriver_op->data_fw_define_flash_reload_dis,
			0);
	} else { /*reload enable*/
		g_core_fp.fp_register_write(
			pdriver_op->addr_fw_define_flash_reload,
			DATA_LEN_4,
			pdriver_op->data_fw_define_flash_reload_en,
			0);
	}

	I("%s: setting OK!\n", __func__);
}

static bool himax_mcu_check_chip_version(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t ret_data = false;
	int i = 0;

	for (i = 0; i < 5; i++) {
		g_core_fp.fp_register_read(pfw_op->addr_icid_addr, DATA_LEN_4,
				tmp_data, 0);
		I("%s:Read driver IC ID = %X,%X,%X\n", __func__,
				tmp_data[3], tmp_data[2], tmp_data[1]);

		if ((tmp_data[3] == 0x83)
		&& (tmp_data[2] == 0x10)
		&& (tmp_data[1] == 0x2a)) {
			strlcpy(private_ts->chip_name,
				HX_83102A_SERIES_PWON, 30);
			ret_data = true;
			goto END;
		} else {
			ret_data = false;
			E("%s:Read driver ID register Fail:\n", __func__);
		}
	}
END:
	return ret_data;
}

static int himax_mcu_read_ic_trigger_type(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	int trigger_type = false;

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_int_is_edge,
			DATA_LEN_4, tmp_data, 0);

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
#if 0
	uint8_t retry = 0;
	uint8_t reload_status = 0;

	g_core_fp.fp_register_write(
	pdriver_op->addr_fw_define_2nd_flash_reload,
	DATA_LEN_4,
	data,
	0);

	g_core_fp.fp_sense_on(0x00);

	I("%s: waiting for FW reload done\n", __func__);

	while (reload_status == 0) {
		g_core_fp.fp_register_read(
			pdriver_op->addr_fw_define_2nd_flash_reload,
			DATA_LEN_4,
			data,
			0);

		/* use all 4 bytes to compare */
		if ((data[3] == 0x00 && data[2] == 0x00 &&
			data[1] == 0x72 && data[0] == 0xC0)) {
			I("%s: FW finish reload done\n", __func__);
			reload_status = 1;
			break;
		} else if (retry == 200) {
			E("%s: FW fail reload done !!!!!\n", __func__);
			ic_data->vendor_panel_ver = 0;
			ic_data->vendor_fw_ver = 0;
			ic_data->vendor_config_ver = 0;
			ic_data->vendor_touch_cfg_ver = 0;
			ic_data->vendor_display_cfg_ver = 0;
			ic_data->vendor_cid_maj_ver = 0;
			ic_data->vendor_cid_min_ver = 0;
			goto END;
		} else {
			I("%s: wait reload done %d times\n", __func__, retry);
			g_core_fp.fp_read_FW_status();
			retry++;
			usleep_range(10000, 11000);
		}
	}

	/**
	 * Read FW version
	 */
	g_core_fp.fp_sense_off(true);
#endif

	g_core_fp.fp_register_read(pfw_op->addr_fw_ver_addr, DATA_LEN_4,
		data, 0);
	ic_data->vendor_panel_ver =  data[0];
	ic_data->vendor_fw_ver = data[1] << 8 | data[2];
	I("PANEL_VER : %X\n", ic_data->vendor_panel_ver);
	I("FW_VER : %X\n", ic_data->vendor_fw_ver);
	g_core_fp.fp_register_read(pfw_op->addr_fw_cfg_addr, DATA_LEN_4,
		data, 0);
	ic_data->vendor_config_ver = data[2] << 8 | data[3];
	/*I("CFG_VER : %X\n",ic_data->vendor_config_ver);*/
	ic_data->vendor_touch_cfg_ver = data[2];
	I("TOUCH_VER : %X\n", ic_data->vendor_touch_cfg_ver);
	ic_data->vendor_display_cfg_ver = data[3];
	I("DISPLAY_VER : %X\n", ic_data->vendor_display_cfg_ver);
	g_core_fp.fp_register_read(pfw_op->addr_fw_vendor_addr,
			DATA_LEN_4, data, 0);
	ic_data->vendor_cid_maj_ver = data[2];
	ic_data->vendor_cid_min_ver = data[3];
	I("CID_VER : %X\n", (ic_data->vendor_cid_maj_ver << 8
			| ic_data->vendor_cid_min_ver));
	g_core_fp.fp_register_read(pfw_op->addr_cus_info, 12, data, 0);
	memcpy(ic_data->vendor_cus_info, data, 12);
	I("Cusomer ID = %s\n", ic_data->vendor_cus_info);
	g_core_fp.fp_register_read(pfw_op->addr_proj_info, 12, data, 0);
	memcpy(ic_data->vendor_proj_info, data, 12);
	I("Project ID = %s\n", ic_data->vendor_proj_info);
	/*Touch Hardware Info set*/

#if 0
END:
	return;
#endif
}

static bool himax_mcu_read_event_stack(uint8_t *buf, uint8_t length)
{
	uint8_t cmd[DATA_LEN_4];
	struct timespec t_start, t_end, t_delta;
	int len = length;
	int i2c_speed = 0;
	int ret = 0;

	/*  AHB_I2C Burst Read Off */
	cmd[0] = pfw_op->data_ahb_dis[0];

	ret = himax_bus_write(pfw_op->addr_ahb_addr[0], cmd, 1,
			HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	if (private_ts->debug_log_level & BIT(2))
		getnstimeofday(&t_start);

	himax_bus_read(pfw_op->addr_event_addr[0], buf, length,
			HIMAX_I2C_RETRY_TIMES);

	if (private_ts->debug_log_level & BIT(2)) {
		getnstimeofday(&t_end);
		t_delta.tv_nsec = (t_end.tv_sec * 1000000000 + t_end.tv_nsec)
			- (t_start.tv_sec * 1000000000 + t_start.tv_nsec);

		i2c_speed = (len * 9 * 1000000
			/ (int)t_delta.tv_nsec) * 13 / 10;
		private_ts->bus_speed = (int)i2c_speed;
	}

	/*  AHB_I2C Burst Read On */
	cmd[0] = pfw_op->data_ahb_en[0];

	ret = himax_bus_write(pfw_op->addr_ahb_addr[0], cmd, 1,
		HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
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
				DATA_LEN_4, tmp_data, 0);
		g_core_fp.fp_register_read(psram_op->addr_rawdata_addr,
				DATA_LEN_4, tmp_data, 0);
		retry--;
		usleep_range(10000, 11000);
	} while ((tmp_data[1] != psram_op->addr_rawdata_end[1]
		&& tmp_data[0] != psram_op->addr_rawdata_end[0])
		&& retry > 0);

	I("%s: End of setting!\n", __func__);
}

static bool himax_mcu_calculateChecksum(bool change_iref, uint32_t size)
{
	uint8_t CRC_result = 0, i;
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
		g_core_fp.fp_register_read(addr, DATA_LEN_4, data, 0);

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
			DATA_LEN_4, tmp_data, 0);

	return NO_ERR;
}

static int himax_mcu_check_sorting_mode(uint8_t *tmp_data)
{

	g_core_fp.fp_register_read(pfw_op->addr_sorting_mode_en,
			DATA_LEN_4, tmp_data, 0);
	I("%s:addr: 0x%02X%02X%02X%02X, Now is:0x%02X%02X%02X%02X\n",
		__func__,
		pfw_op->addr_sorting_mode_en[3],
		pfw_op->addr_sorting_mode_en[2],
		pfw_op->addr_sorting_mode_en[1],
		pfw_op->addr_sorting_mode_en[0],
		tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);

	return NO_ERR;
}

static uint8_t himax_mcu_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data)
{
	int cnt = 0;
	uint8_t req_size = cmd_set[0];

	cmd_set[3] = pfw_op->data_dd_request[0];
	g_core_fp.fp_register_write(pfw_op->addr_dd_handshak_addr,
			DATA_LEN_4, cmd_set, 0);
	I("%s:cmd set[0]=0x%2X,set[1]=0x%2X,set[2]=0x%2X,set[3]=0x%2X\n",
		__func__, cmd_set[0], cmd_set[1], cmd_set[2], cmd_set[3]);

	/* Doing hand shaking 0xAA -> 0xBB */
	for (cnt = 0; cnt < 100; cnt++) {
		g_core_fp.fp_register_read(pfw_op->addr_dd_handshak_addr,
				DATA_LEN_4, tmp_data, 0);
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
	g_core_fp.fp_register_read(pfw_op->addr_dd_data_addr,
			req_size, tmp_data, 0);
	return NO_ERR;
}
static void hx_clr_fw_reord_dd_sts(void)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	g_core_fp.fp_register_read(pic_op->addr_cs_central_state,
		ADDR_LEN_4, tmp_data, 0);
	I("%s: Check enter_save_mode data[0]=%02X\n", __func__, tmp_data[0]);

	if (tmp_data[0] == 0x0C) {
		I("%s: Enter safe mode, OK!\n", __func__);
	} else {
		E("%s: It doen't enter safe mode, please check it again\n",
			__func__);
		return;
	}
	g_core_fp.fp_register_read(pfw_op->addr_clr_fw_record_dd_sts,
		DATA_LEN_4, tmp_data, 0);
	I("%s,Before Write :Now 10007FCC=0x%02X%02X%02X%02X\n",
		__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
	usleep_range(10000, 10001);

	tmp_data[2] = 0x00;
	tmp_data[3] = 0x00;
	g_core_fp.fp_register_write(pfw_op->addr_clr_fw_record_dd_sts,
		DATA_LEN_4, tmp_data, 0);
	usleep_range(10000, 10001);

	g_core_fp.fp_register_read(pfw_op->addr_clr_fw_record_dd_sts,
		DATA_LEN_4, tmp_data, 0);
	I("%s,After Write :Now 10007FCC=0x%02X%02X%02X%02X\n",
		__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

}

static void hx_ap_notify_fw_sus(int suspend)
{
	int retry = 0;
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
		g_core_fp.fp_register_write(addr_tmp,
			sizeof(data_tmp), data_tmp, 0);
		usleep_range(1000, 1001);
		g_core_fp.fp_register_read(addr_tmp,
			sizeof(data_tmp), read_tmp, 0);
		I("%s: Now retry=%d, data=0x%02X%02X%02X%02X\n",
			__func__, retry,
			read_tmp[3], read_tmp[2], read_tmp[1], read_tmp[0]);
	} while ((retry++ < 10) &&
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
			DATA_LEN_4, pflash_op->data_spi200_trans_fmt, 0);

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			DATA_LEN_4, pflash_op->data_spi200_trans_ctrl_2, 0);
	g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			DATA_LEN_4, pflash_op->data_spi200_cmd_2, 0);

	g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			DATA_LEN_4, pflash_op->data_spi200_cmd_3, 0);
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
			DATA_LEN_4, pflash_op->data_spi200_trans_fmt, 0);

	for (page_prog_start = start_addr;
	page_prog_start < start_addr + length;
	page_prog_start = page_prog_start + block_size) {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			DATA_LEN_4, pflash_op->data_spi200_trans_ctrl_2, 0);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			DATA_LEN_4, pflash_op->data_spi200_cmd_2, 0);

		tmp_data[3] = (page_prog_start >> 24)&0xFF;
		tmp_data[2] = (page_prog_start >> 16)&0xFF;
		tmp_data[1] = (page_prog_start >> 8)&0xFF;
		tmp_data[0] = page_prog_start&0xFF;
		g_core_fp.fp_register_write(pflash_op->addr_spi200_addr,
				DATA_LEN_4, tmp_data, 0);

		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			DATA_LEN_4, pflash_op->data_spi200_trans_ctrl_3, 0);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			DATA_LEN_4, pflash_op->data_spi200_cmd_4, 0);
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
	int page_prog_start = 0, i = 0, j = 0, k = 0;
	int program_length = PROGRAM_SZ;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t buring_data[FLASH_RW_MAX_LEN];
	int ret = 0;
	/* 4 bytes for padding*/
	g_core_fp.fp_interface_on();

	g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_fmt,
			DATA_LEN_4, pflash_op->data_spi200_trans_fmt, 0);

	for (page_prog_start = 0; page_prog_start < FW_Size;
	page_prog_start += FLASH_RW_MAX_LEN) {
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			DATA_LEN_4, pflash_op->data_spi200_trans_ctrl_2, 0);
		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			DATA_LEN_4, pflash_op->data_spi200_cmd_2, 0);

		 /*Programmable size = 1 page = 256 bytes,*/
		 /*word_number = 256 byte / 4 = 64*/
		g_core_fp.fp_register_write(pflash_op->addr_spi200_trans_ctrl,
			DATA_LEN_4, pflash_op->data_spi200_trans_ctrl_4, 0);

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
				DATA_LEN_4, tmp_data, 0);

		for (i = 0; i < ADDR_LEN_4; i++)
			buring_data[i] = pflash_op->addr_spi200_data[i];

		for (i = page_prog_start, j = 0;
		i < 16 + page_prog_start;
		i++, j++) {
			buring_data[j + ADDR_LEN_4] = FW_content[i];
		}

		ret = himax_bus_write(pic_op->addr_ahb_addr_byte_0[0],
				buring_data,
				ADDR_LEN_4 + 16,
				HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		g_core_fp.fp_register_write(pflash_op->addr_spi200_cmd,
			DATA_LEN_4, pflash_op->data_spi200_cmd_6, 0);

		for (j = 0; j < 5; j++) {
			for (i = (page_prog_start + 16 + (j * 48)), k = 0;
			i < (page_prog_start + 16 + (j * 48)) + program_length;
			i++, k++)
				buring_data[k + ADDR_LEN_4] = FW_content[i];

			ret = himax_bus_write(pic_op->addr_ahb_addr_byte_0[0],
				buring_data,
				program_length + ADDR_LEN_4,
				HIMAX_I2C_RETRY_TIMES);
			if (ret < 0) {
				E("%s: i2c access fail!\n", __func__);
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
	g_core_fp.fp_block_erase(0x00, FW_SIZE_64k);
	g_core_fp.fp_flash_programming(fw, FW_SIZE_64k);

	if (g_core_fp.fp_check_CRC(pfw_op->addr_program_reload_from,
	FW_SIZE_64k) == 0)
		burnFW_success = 1;

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
			sizeof(pfw_op->data_clear), pfw_op->data_clear, 0);
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
	g_core_fp.fp_block_erase(0x00, FW_SIZE_128k);
	g_core_fp.fp_flash_programming(fw, FW_SIZE_128k);

	if (g_core_fp.fp_check_CRC(pfw_op->addr_program_reload_from,
	FW_SIZE_128k) == 0)
		burnFW_success = 1;

	/*RawOut select initial*/
	g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
			sizeof(pfw_op->data_clear), pfw_op->data_clear, 0);
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
	uint8_t buffer[256];
	int page_prog_start = 0;

	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_burst_enable(1);

	for (page_prog_start = 0; page_prog_start < Flash_Size;
	page_prog_start += 128) {
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
		g_core_fp.fp_register_read(tmp_addr, flash_page_len,
				&flash_tmp_buffer[0], 0);
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
		} else if (chk_sum % 0x100) /*2. Check sum*/
			I("%s: chk sum failed in %X\n",	__func__, i + addr);
		else { /*3. get data*/
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
				FW_VER_MAJ_FLASH_ADDR);
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
	uint8_t max_i2c_size = MAX_I2C_TRANS_SZ;
	uint8_t x_num = ic_data->HX_RX_NUM;
	uint8_t y_num = ic_data->HX_TX_NUM;
	/*int m_key_num = 0;*/
	int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
	int total_data_size = (x_num * y_num + x_num + y_num) * 2;
	int total_size_temp;
	/*int mutual_data_size = x_num * y_num * 2;*/
	int total_read_times = 0;
	int address = 0;
	uint8_t  *temp_info_data = NULL; /*max mkey size = 8*/
	uint16_t check_sum_cal = 0;
	int fw_run_flag = -1;

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
	tmp_data[3] = 0x00; tmp_data[2] = 0x00;
	tmp_data[1] = psram_op->passwrd_start[1];
	tmp_data[0] = psram_op->passwrd_start[0];
	fw_run_flag = himax_write_read_reg(psram_op->addr_rawdata_addr,
			tmp_data,
			psram_op->passwrd_end[1],
			psram_op->passwrd_end[0]);

	if (fw_run_flag < 0) {
		I("%s Data NOT ready => bypass\n", __func__);
		g_core_fp.fp_read_FW_status();
		goto FAIL;
	}

	/* 3. Read RawData */
	total_size_temp = total_size;
	I("%s:data[0]=0x%2X,data[1]=0x%2X,data[2]=0x%2X,data[3]=0x%2X\n",
		__func__,
		psram_op->addr_rawdata_addr[0],
		psram_op->addr_rawdata_addr[1],
		psram_op->addr_rawdata_addr[2],
		psram_op->addr_rawdata_addr[3]);

	tmp_addr[0] = psram_op->addr_rawdata_addr[0];
	tmp_addr[1] = psram_op->addr_rawdata_addr[1];
	tmp_addr[2] = psram_op->addr_rawdata_addr[2];
	tmp_addr[3] = psram_op->addr_rawdata_addr[3];

	if (total_size % max_i2c_size == 0)
		total_read_times = total_size / max_i2c_size;
	else
		total_read_times = total_size / max_i2c_size + 1;

	for (i = 0; i < total_read_times; i++) {
		address = (psram_op->addr_rawdata_addr[3] << 24)
				+ (psram_op->addr_rawdata_addr[2] << 16)
				+ (psram_op->addr_rawdata_addr[1] << 8)
				+ psram_op->addr_rawdata_addr[0]
				+ i * max_i2c_size;
		/*I("%s address = %08X\n", __func__, address);*/

		tmp_addr[3] = (uint8_t)((address >> 24) & 0x00FF);
		tmp_addr[2] = (uint8_t)((address >> 16) & 0x00FF);
		tmp_addr[1] = (uint8_t)((address >> 8) & 0x00FF);
		tmp_addr[0] = (uint8_t)((address) & 0x00FF);

		if (total_size_temp >= max_i2c_size) {
			g_core_fp.fp_register_read(tmp_addr, max_i2c_size,
					&temp_info_data[i * max_i2c_size], 0);
			total_size_temp = total_size_temp - max_i2c_size;
		} else {
			/*I("last total_size_temp=%d\n",total_size_temp);*/
			g_core_fp.fp_register_read(tmp_addr,
					total_size_temp % max_i2c_size,
					&temp_info_data[i * max_i2c_size], 0);
		}
	}

	/* 4. FW stop outputing */
	tmp_data[3] = temp_info_data[3];
	tmp_data[2] = temp_info_data[2];
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	g_core_fp.fp_register_write(psram_op->addr_rawdata_addr,
			DATA_LEN_4, tmp_data, 0);

	/* 5. Data Checksum Check */
	for (i = 2; i < total_size; i += 2)/* 2:PASSWORD NOT included */
		check_sum_cal += (temp_info_data[i + 1] * 256
			+ temp_info_data[i]);

	if (check_sum_cal % 0x10000 != 0) {
		I("%s check_sum_cal fail=%2X\n", __func__, check_sum_cal);
		goto FAIL;
	} else {
		memcpy(info_data, &temp_info_data[4],
				total_data_size * sizeof(uint8_t));
		/*I("%s checksum PASS\n", __func__);*/
	}
	kfree(temp_info_data);
	return true;
FAIL:
	kfree(temp_info_data);
	return false;
}
/* SRAM side end*/
/* CORE_SRAM */

/* CORE_DRIVER */

static void himax_mcu_init_ic(void)
{
	I("%s: use default incell init.\n", __func__);
}

#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
static int himax_mcu_fw_ver_bin(void)
{
	I("%s: use default incell address.\n", __func__);
	if (hxfw != NULL) {
		I("Catch fw version in bin file!\n");
		g_i_FW_VER = (hxfw->data[FW_VER_MAJ_FLASH_ADDR] << 8)
				| hxfw->data[FW_VER_MIN_FLASH_ADDR];
		g_i_CFG_VER = (hxfw->data[CFG_VER_MAJ_FLASH_ADDR] << 8)
				| hxfw->data[CFG_VER_MIN_FLASH_ADDR];
		g_i_CID_MAJ = hxfw->data[CID_VER_MAJ_FLASH_ADDR];
		g_i_CID_MIN = hxfw->data[CID_VER_MIN_FLASH_ADDR];
	} else {
		I("FW data is null!\n");
		return 1;
	}
	return NO_ERR;
}
#endif


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

#if !defined(HX_FIX_TOUCH_INFO)
static uint8_t himax_mcu_tp_info_check(void)
{
	uint8_t rx = pdriver_op->data_df_rx[0];
	uint8_t tx = pdriver_op->data_df_tx[0];
	uint8_t pt = pdriver_op->data_df_pt[0];
	uint8_t err_cnt = 0;

	if (ic_data->HX_RX_NUM < (rx / 2)
	|| ic_data->HX_RX_NUM > (rx * 3 / 2)) {
		ic_data->HX_RX_NUM = rx;
		err_cnt |= 0x01;
	}
	if (ic_data->HX_TX_NUM < (tx / 2)
	|| ic_data->HX_TX_NUM > (tx * 3 / 2)) {
		ic_data->HX_TX_NUM = tx;
		err_cnt |= 0x02;
	}
	if (ic_data->HX_MAX_PT < (pt / 2)
	|| ic_data->HX_MAX_PT > (pt * 3 / 2)) {
		ic_data->HX_MAX_PT = pt;
		err_cnt |= 0x04;
	}

	return err_cnt;
}
#endif

static void himax_mcu_touch_information(void)
{
#if !defined(HX_FIX_TOUCH_INFO)
	char addr[DATA_LEN_4] = {0};
	char data[DATA_LEN_8] = {0};
	uint8_t err_cnt = 0;

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_rxnum_txnum_maxpt,
			DATA_LEN_8, data, 0);
	ic_data->HX_RX_NUM = data[2];
	ic_data->HX_TX_NUM = data[3];
	ic_data->HX_MAX_PT = data[4];
	/*I("%s : HX_RX_NUM=%d,ic_data->HX_TX_NUM=%d,ic_data->HX_MAX_PT=%d\n",
	 *	__func__,ic_data->HX_RX_NUM,
	 *	ic_data->HX_TX_NUM,ic_data->HX_MAX_PT);
	 */
	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_xy_res_enable,
			DATA_LEN_4, data, 0);

	/*I("%s : c_data->HX_XY_REVERSE=0x%2.2X\n",__func__,data[1]);*/
	if ((data[1] & 0x04) == 0x04)
		ic_data->HX_XY_REVERSE = true;
	else
		ic_data->HX_XY_REVERSE = false;

	g_core_fp.fp_register_read(pdriver_op->addr_fw_define_int_is_edge,
			DATA_LEN_4, data, 0);
	/*I("%s : data[0]=0x%2.2X,data[1]=0x%2.2X,data[2]=0x%2.2X,
	 *	data[3]=0x%2.2X\n",__func__,data[0],data[1],data[2],data[3]);
	 */
	/*I("data[0] & 0x01 = %d\n",(data[0] & 0x01));*/
	if ((data[1] & 0x01) == 1)
		ic_data->HX_INT_IS_EDGE = true;
	else
		ic_data->HX_INT_IS_EDGE = false;

	/*1. Read number of MKey R100070E8H to determin data size*/
	g_core_fp.fp_register_read(psram_op->addr_mkey, DATA_LEN_4, data, 0);
	/* I("%s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2]=0x%02X,
	 *	tmp_data[3] = 0x%02X\n",
	 */
	/*	__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);*/
	ic_data->HX_BT_NUM = data[0] & 0x03;

	addr[3] = 0x10;
	addr[2] = 0x00;
	addr[1] = 0x71;
	addr[0] = 0x9C;
	g_core_fp.fp_register_read(addr, DATA_LEN_4, data, 0);
	ic_data->HX_PEN_FUNC = data[3];

	err_cnt = himax_mcu_tp_info_check();
	if (err_cnt > 0)
		E("TP Info from IC is wrong, err_cnt = 0x%X", err_cnt);
#else
	ic_data->HX_RX_NUM = FIX_HX_RX_NUM;
	ic_data->HX_TX_NUM = FIX_HX_TX_NUM;
	ic_data->HX_BT_NUM = FIX_HX_BT_NUM;
	ic_data->HX_MAX_PT = FIX_HX_MAX_PT;
	ic_data->HX_XY_REVERSE = FIX_HX_XY_REVERSE;
	ic_data->HX_INT_IS_EDGE = FIX_HX_INT_IS_EDGE;
	ic_data->HX_PEN_FUNC = FIX_HX_PEN_FUNC;
#endif
	ic_data->HX_Y_RES = private_ts->pdata->screenHeight;
	ic_data->HX_X_RES = private_ts->pdata->screenWidth;

	I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d\n", __func__,
		ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	I("%s:HX_MAX_PT=%d,HX_XY_REVERSE =%d\n", __func__,
		ic_data->HX_MAX_PT, ic_data->HX_XY_REVERSE);
	I("%s:HX_Y_RES=%d,HX_X_RES =%d\n", __func__,
		ic_data->HX_Y_RES, ic_data->HX_X_RES);
	I("%s:HX_INT_IS_EDGE =%d,HX_PEN_FUNC = %d\n", __func__,
	ic_data->HX_INT_IS_EDGE, ic_data->HX_PEN_FUNC);
}

static void himax_mcu_calcTouchDataSize(void)
{
	struct himax_ts_data *ts_data = private_ts;

	ts_data->x_channel = ic_data->HX_RX_NUM;
	ts_data->y_channel = ic_data->HX_TX_NUM;
	ts_data->nFinger_support = ic_data->HX_MAX_PT;

#if 0
	ts_data->coord_data_size = 4 * ts_data->nFinger_support;
	ts_data->area_data_size = ((ts_data->nFinger_support / 4)
				+ (ts_data->nFinger_support % 4 ? 1 : 0)) * 4;
	ts_data->coordInfoSize = ts_data->coord_data_size
				+ ts_data->area_data_size + 4;
	ts_data->raw_data_frame_size = 128 - ts_data->coord_data_size
				- ts_data->area_data_size - 4 - 4 - 2;

	if (ts_data->raw_data_frame_size == 0) {
		E("%s: could NOT calculate!\n", __func__);
		return;
	}

	ts_data->raw_data_nframes = ((uint32_t)ts_data->x_channel
					* ts_data->y_channel
					+ ts_data->x_channel
					+ ts_data->y_channel)
					/ ts_data->raw_data_frame_size
					+ (((uint32_t)ts_data->x_channel
					* ts_data->y_channel
					+ ts_data->x_channel
					+ ts_data->y_channel)
					% ts_data->raw_data_frame_size) ? 1 : 0;

	I("%s: coord_dsz:%d,area_dsz:%d,raw_data_fsz:%d,raw_data_nframes:%d",
		__func__,
		ts_data->coord_data_size,
		ts_data->area_data_size,
		ts_data->raw_data_frame_size,
		ts_data->raw_data_nframes);
#endif

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

#if 0
static void himax_mcu_reload_config(void)
{
	if (himax_report_data_init())
		E("%s: allocate data fail\n", __func__);
	g_core_fp.fp_sense_on(0x00);
}
#endif

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
	int result = 0;


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
		g_core_fp.fp_register_read(tmp_addr, 128,
			&flash_tmp_buffer[temp_addr - start_addr], false);
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
			memcpy(&g_guest_info_data->g_guest_str_in_format[custom_info_temp][0],
				g_checksum_str, (int)strlen(g_checksum_str));
			memcpy(&g_guest_info_data->g_guest_str[custom_info_temp][0],
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
		memcpy(&g_guest_info_data->g_guest_str_in_format[custom_info_temp][0],
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

#if defined(HX_SMART_WAKEUP)\
	|| defined(HX_HIGH_SENSE)\
	|| defined(HX_USB_DETECT_GLOBAL)
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
#endif

#if defined(HX_ZERO_FLASH)
int G_POWERONOF = 1;
EXPORT_SYMBOL(G_POWERONOF);
void hx_dis_rload_0f(int disable)
{
	/*Disable Flash Reload*/
	g_core_fp.fp_register_write(pdriver_op->addr_fw_define_flash_reload,
		DATA_LEN_4, pzf_op->data_dis_flash_reload, 0);
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

	max_bus_size = (write_len > HX_MAX_WRITE_SZ - 4)
			? (HX_MAX_WRITE_SZ - 4)
			: write_len;

	total_size_temp = write_len;

	g_core_fp.fp_burst_enable(1);

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
			g_core_fp.fp_register_write(tmp_addr,
				max_bus_size, tmp_data, 0);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			I("last total_size_temp=%d\n", total_size_temp);
			g_core_fp.fp_register_write(tmp_addr,
				total_size_temp % max_bus_size,
				tmp_data, 0);
		}
		address = ((i+1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t) ((address>>8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		usleep_range(10000, 11000);
	}

	I("%s, END\n", __func__);
}

void himax_mcu_write_sram_0f(const struct firmware *fw_entry, uint8_t *addr,
		int start_index, uint32_t write_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int address = 0;
	int i = 0;

	uint8_t tmp_addr[4];
	uint8_t *tmp_data;

	total_size_temp = write_len;
	I("%s, Entering - total write size=%d\n", __func__, total_size_temp);

	max_bus_size = (write_len > HX_MAX_WRITE_SZ - 4)
			? (HX_MAX_WRITE_SZ - 4)
			: write_len;

	g_core_fp.fp_burst_enable(1);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s, write addr = 0x%02X%02X%02X%02X\n", __func__,
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

	/* tmp_data = kcalloc(max_bus_size, sizeof(uint8_t), GFP_KERNEL);
	 * if (tmp_data == NULL) {
	 *	I("%s: Can't allocate enough buf\n", __func__);
	 *	return;
	 * }
	 */

	/*for(i = 0;i<10;i++)
	 *{
	 *	I("[%d] 0x%2.2X", i, tmp_data[i]);
	 *}
	 *I("\n");
	 */
	if (total_size_temp % max_bus_size == 0)
		total_read_times = total_size_temp / max_bus_size;
	else
		total_read_times = total_size_temp / max_bus_size + 1;

	for (i = 0; i < (total_read_times); i++) {
		/*I("[log]write %d time start!\n", i);*/
		/*I("[log]addr[3]=0x%02X, addr[2]=0x%02X,
		 *	addr[1]=0x%02X,
		 *	addr[0]=0x%02X!\n", tmp_addr[3],
		 *	tmp_addr[2], tmp_addr[1], tmp_addr[0]);
		 */

		if (total_size_temp >= max_bus_size) {
			/*memcpy(tmp_data, &fw_entry->data[start_index+i
			 *	* max_bus_size], max_bus_size);
			 */
			tmp_data = (uint8_t *)&fw_entry->data[start_index+i
				* max_bus_size];
			g_core_fp.fp_register_write(tmp_addr,
				max_bus_size, tmp_data, 0);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			/*memcpy(tmp_data, &fw_entry->data[start_index+i
			 *	* max_bus_size],
			 *	total_size_temp % max_bus_size);
			 */
			tmp_data = (uint8_t *)&fw_entry->data[start_index+i
				* max_bus_size];
			I("last total_size_temp=%d\n",
				total_size_temp % max_bus_size);
			g_core_fp.fp_register_write(tmp_addr,
					total_size_temp % max_bus_size,
					tmp_data, 0);
		}

		/*I("[log]write %d time end!\n", i);*/
		address = ((i+1) * max_bus_size);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

		if (tmp_addr[0] <  addr[0])
			tmp_addr[1] = addr[1]
				+ (uint8_t) ((address>>8) & 0x00FF) + 1;
		else
			tmp_addr[1] = addr[1]
				+ (uint8_t) ((address>>8) & 0x00FF);

		udelay(100);
	}
	I("%s, End\n", __func__);
	/* kfree(tmp_data); */
}

int himax_sram_write_crc_check(const struct firmware *fw_entry, uint8_t *addr,
		int strt_idx, uint32_t len)
{
	int retry = 0;
	int crc = -1;

	do {
		g_core_fp.fp_write_sram_0f(fw_entry, addr, strt_idx, len);
		crc = g_core_fp.fp_check_CRC(addr,  len);
		retry++;
		I("%s, HW CRC %s in %d time\n", __func__,
			(crc == 0) ? "OK" : "Fail", retry);
	} while (crc != 0 && retry < 3);

	return crc;
}

int himax_zf_part_info(const struct firmware *fw_entry)
{
	uint32_t cfg_table_pos = CFG_TABLE_FLASH_ADDR;
	int part_num = 0;
	int ret = 0;
	uint8_t buf[16];
	struct zf_info *zf_info_arr;
#if 1
	uint8_t *FW_buf;
	uint8_t sram_min[4];
	int cfg_crc_sw = 0;
	int cfg_crc_hw = 0;
	int cfg_sz = 0;
	int retry = 3;
	uint8_t i = 0;
	int i_max = 0;
	int i_min = 0;
	uint32_t dsram_base = 0xFFFFFFFF;
	uint32_t dsram_max = 0;
#endif
#if defined(HX_CODE_OVERLAY)
	uint8_t tmp_addr[4] = {0xFC, 0x7F, 0x00, 0x10};
	uint8_t send_data[4] = {0xA5, 0x5A, 0x5A, 0xA5};
	uint8_t recv_data[4] = {0};
	uint8_t ovl_idx_t = 0;
//	int retry2 = 0;

//	uint8_t j = 0;
	int allovlidx = 0;
	uint8_t data[4] = {0};
#if 0
	if (ovl_idx == NULL) {
		ovl_idx = kzalloc(ovl_section_num, GFP_KERNEL);
		if (ovl_idx == NULL) {
			E("%s, ovl_idx alloc failed!\n",
				__func__);
			return -ENOMEM;
		}
	} else {
		memset(ovl_idx, 0, ovl_section_num);
	}
#endif
#endif

	/*1. get number of partition*/
	part_num = fw_entry->data[cfg_table_pos + 12];

	I("%s, Number of partition is %d\n", __func__, part_num);
	if (part_num < 1) {
		E("%s, size of cfg part failed! part_num = %d\n",
			__func__, part_num);
		return LENGTH_FAIL;
	}

	/*2. initial struct of array*/
	zf_info_arr = kcalloc(part_num, sizeof(struct zf_info), GFP_KERNEL);
	if (zf_info_arr == NULL) {
		E("%s, Allocate ZF info array failed!\n", __func__);
		ret =  MEM_ALLOC_FAIL;
		goto ALOC_ZF_INFO_ARR_FAIL;
	}
	memset(zf_info_arr, 0x0, part_num * sizeof(struct zf_info));

	/*3. Catch table information */
	memcpy(buf, &fw_entry->data[cfg_table_pos], 16);
	memcpy(zf_info_arr[0].sram_addr, buf, 4);
	zf_info_arr[0].write_size = buf[7] << 24
				| buf[6] << 16
				| buf[5] << 8
				| buf[4];
	zf_info_arr[0].fw_addr = buf[11] << 24
				| buf[10] << 16
				| buf[9] << 8
				| buf[8];

#if 1
	for (i = 1; i < part_num; i++) {
		/*3. get all partition*/
		memcpy(buf, &fw_entry->data[i * 0x10 + cfg_table_pos], 16);

		memcpy(zf_info_arr[i].sram_addr, buf, 4);
		zf_info_arr[i].write_size = buf[7] << 24
					| buf[6] << 16
					| buf[5] << 8
					| buf[4];
		zf_info_arr[i].fw_addr = buf[11] << 24
					| buf[10] << 16
					| buf[9] << 8
					| buf[8];
		zf_info_arr[i].cfg_addr = zf_info_arr[i].sram_addr[0];
		zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[1] << 8;
		zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[2] << 16;
		zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[3] << 24;

#if defined(HX_CODE_OVERLAY)
		/*overlay section*/

	//	if ((buf[15] == 0x55 && buf[14] == 0x66)
	//	|| (buf[3] == 0x20 && buf[2] == 0x00
	//	&& buf[1] == 0x8C && buf[0] == 0xE0)) {
		if ((buf[15] == 0x77 && buf[14] == 0x88)) {
			I("%s: catch overlay section in index %d\n",
				__func__, i);

			/* record index of overlay section */
			allovlidx |= 1<<i;

			ovl_idx_t = i;
		//	if (buf[15] == 0x55 && buf[14] == 0x66) {
				/* current mechanism */
		//		j = buf[13];
		//		if (j < ovl_section_num)
		//			ovl_idx[j] = i;
		//	} else {
				/* previous mechanism */
		//		if (j < ovl_section_num)
		//			ovl_idx[j++] = i;
		//	}

			continue;
		}
#endif

		if (dsram_base > zf_info_arr[i].cfg_addr) {
			dsram_base = zf_info_arr[i].cfg_addr;
			i_min = i;
		}
		if (dsram_max < zf_info_arr[i].cfg_addr) {
			dsram_max = zf_info_arr[i].cfg_addr;
			i_max = i;
		}
	}
	for (i = 0; i < ADDR_LEN_4; i++)
		sram_min[i] = zf_info_arr[i_min].sram_addr[i];

	cfg_sz = (dsram_max - dsram_base) + zf_info_arr[i_max].write_size;
	if (cfg_sz % 16 != 0)
		cfg_sz = cfg_sz + 16 - (cfg_sz % 16);

	I("%s, cfg_sz = %d!, dsram_base = %X, dsram_max = %X\n", __func__,
			cfg_sz, dsram_base, dsram_max);

	if (cfg_sz <= DSRAM_SIZE) {
		/* config size should be smaller than DSRAM size */
		FW_buf = kcalloc(cfg_sz, sizeof(uint8_t), GFP_KERNEL);
		if (FW_buf == NULL) {
			E("%s, Allocate FW_buf array failed!\n", __func__);
			ret = MEM_ALLOC_FAIL;
			goto ALOC_FW_BUF_FAIL;
		}
	} else {
		E("%s: config size is abnormal, please check FW\n", __func__);
		ret = LENGTH_FAIL;
		goto ALOC_FW_BUF_FAIL;
	}

	for (i = 1; i < part_num; i++) {
		if (zf_info_arr[i].cfg_addr % 4 != 0)
			zf_info_arr[i].cfg_addr = zf_info_arr[i].cfg_addr
				- (zf_info_arr[i].cfg_addr % 4);

		I("%s,[%d]SRAM addr=%08X, fw_addr=%08X, write_size=%d\n",
			__func__, i,
			zf_info_arr[i].cfg_addr,
			zf_info_arr[i].fw_addr,
			zf_info_arr[i].write_size);

#if defined(HX_CODE_OVERLAY)
		/*overlay section*/
		if (allovlidx & (1<<i)) {
			I("%s: skip overlay section %d\n", __func__, i);
			continue;
		}
#endif

		memcpy(&FW_buf[zf_info_arr[i].cfg_addr - dsram_base],
			&fw_entry->data[zf_info_arr[i].fw_addr],
			zf_info_arr[i].write_size);
	}

	cfg_crc_sw = g_core_fp.fp_Calculate_CRC_with_AP(FW_buf, 0, cfg_sz);
	I("Now cfg_crc_sw=%X\n", cfg_crc_sw);
#endif
	/*4. write to sram*/
	if (G_POWERONOF == 1) {
			/* FW entity */
		if (himax_sram_write_crc_check(fw_entry,
		zf_info_arr[0].sram_addr,
		zf_info_arr[0].fw_addr,
		zf_info_arr[0].write_size) != 0) {
			E("%s, HW CRC FAIL\n", __func__);
		} else {
			I("%s, HW CRC PASS\n", __func__);
		}
#if 1
		I("Now sram_min[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
			sram_min[0], sram_min[1], sram_min[2], sram_min[3]);
		do {
			g_core_fp.fp_register_write(sram_min, cfg_sz,
				FW_buf, 0);
			cfg_crc_hw = g_core_fp.fp_check_CRC(sram_min, cfg_sz);
			if (cfg_crc_hw != cfg_crc_sw) {
				E("Cfg CRC FAIL,HWCRC=%X,SWCRC=%X,retry=%d\n",
					cfg_crc_hw, cfg_crc_sw, retry);
			} else
				I("Config CRC Pass\n");

		} while (cfg_crc_hw != cfg_crc_sw && retry-- > 0);

#if defined(HX_CODE_OVERLAY)
		// clear handshaking to 0xA55A5AA5
		retry = 0;
		do {
			g_core_fp.fp_register_write(tmp_addr,
					DATA_LEN_4,
					send_data, 0);
			usleep_range(1000, 1100);
			g_core_fp.fp_register_read(tmp_addr,
					DATA_LEN_4,
					recv_data, 0);
		} while ((recv_data[0] != send_data[0]
					|| recv_data[1] != send_data[1]
					|| recv_data[2] != send_data[2]
					|| recv_data[3] != send_data[3])
					&& retry++ < HIMAX_REG_RETRY_TIMES);

		I("%s: clear handshaking data = %02X%02X%02X%02X\n",
			__func__, recv_data[0], recv_data[1], recv_data[2], recv_data[3]);

		send_data[0] = ovl_handalg_reply;
		send_data[1] = ovl_handalg_reply;
		send_data[2] = ovl_handalg_reply;
		send_data[3] = ovl_handalg_reply;

		if (zf_info_arr[ovl_idx_t].write_size == 0) {
			send_data[0] = ovl_fault;
			E("%s, WRONG overlay section, plese check FW!\n", __func__);
		} else {

			g_core_fp.fp_reload_disable(0);

			/*g_core_fp.fp_power_on_init();*/
			g_core_fp.fp_register_write(pfw_op->addr_raw_out_sel,
					sizeof(pfw_op->data_clear), pfw_op->data_clear, 0);
			/*DSRAM func initial*/
			g_core_fp.fp_assign_sorting_mode(pfw_op->data_clear);
			/*FW reload done initial*/
			g_core_fp.fp_register_write(pdriver_op->addr_fw_define_2nd_flash_reload,
							DATA_LEN_4, data, 0);

			g_core_fp.fp_sense_on(0x00);

			retry = 0;
			do {
				usleep_range(1000, 1100);
				g_core_fp.fp_register_read(tmp_addr,
						DATA_LEN_4,
						recv_data, 0);
			} while ((recv_data[0] != ovl_handalg_request
						|| recv_data[1] != ovl_handalg_request
						|| recv_data[2] != ovl_handalg_request
						|| recv_data[3] != ovl_handalg_request)
					&& retry++ < 30);

			if (retry <= 30) {
				if (himax_sram_write_crc_check(fw_entry,
				zf_info_arr[ovl_idx_t].sram_addr,
				zf_info_arr[ovl_idx_t].fw_addr,
				zf_info_arr[ovl_idx_t].write_size) != 0) {
					send_data[0] = ovl_fault;
					E("%s, Overlay HW CRC FAIL\n", __func__);
				} else {
					I("%s, Overlay HW CRC PASS\n", __func__);
				}
			} else {
				send_data[0] = ovl_fault;
			}
		}

		retry = 0;
		do {
			g_core_fp.fp_register_write(tmp_addr,
					DATA_LEN_4, send_data, 0);
			g_core_fp.fp_register_read(tmp_addr,
					DATA_LEN_4, recv_data, 0);
		} while ((send_data[3] != recv_data[3]
				|| send_data[2] != recv_data[2]
				|| send_data[1] != recv_data[1]
				|| send_data[0] != recv_data[0])
				&& retry++ < HIMAX_REG_RETRY_TIMES);



		I("%s: waiting for FW reload done", __func__);

		retry = 0;
		while (retry++ < 30) {
			g_core_fp.fp_register_read(
					pdriver_op->addr_fw_define_2nd_flash_reload,
					DATA_LEN_4, data, 0);

			/* use all 4 bytes to compare */
			if ((data[3] == 0x00 && data[2] == 0x00 &&
						data[1] == 0x72 && data[0] == 0xC0)) {
				I("%s: FW finish reload done\n", __func__);
				break;
			}
			I("%s: wait reload done %d times\n", __func__, retry);
			g_core_fp.fp_read_FW_status();
			usleep_range(10000, 11000);
		}
#endif



#if 0//defined(HX_CODE_OVERLAY)
		/* ovl_idx[0] - sorting */
		/* ovl_idx[1] - gesture */
		/* ovl_idx[2] - border  */

		ovl_idx_t = ovl_idx[0];
		send_data[0] = ovl_sorting_reply;

		if (private_ts->in_self_test == 0) {
#if defined(HX_SMART_WAKEUP)
			if (private_ts->suspended && private_ts->SMWP_enable) {
				ovl_idx_t = ovl_idx[1];
				send_data[0] = ovl_gesture_reply;
			} else {
				ovl_idx_t = ovl_idx[2];
				send_data[0] = ovl_border_reply;
			}
#else
			ovl_idx_t = ovl_idx[2];
			send_data[0] = ovl_border_reply;
#endif

#if defined(HX_SMART_WAKEUP) \
	|| defined(HX_HIGH_SENSE) \
	|| defined(HX_USB_DETECT_GLOBAL)
			/*write back system config*/
			g_core_fp.fp_resend_cmd_func(private_ts->suspended);
#endif
		}
		I("%s: prepare upgrade overlay section = %d\n",
				__func__, ovl_idx_t);

		if (zf_info_arr[ovl_idx_t].write_size == 0) {
			send_data[0] = ovl_fault;
			E("%s, WRONG overlay section, plese check FW!\n",
					__func__);
		} else {
			if (himax_sram_write_crc_check(fw_entry,
			zf_info_arr[ovl_idx_t].sram_addr,
			zf_info_arr[ovl_idx_t].fw_addr,
			zf_info_arr[ovl_idx_t].write_size) != 0) {
				send_data[0] = ovl_fault;
				E("%s, Overlay HW CRC FAIL\n", __func__);
			} else {
				I("%s, Overlay HW CRC PASS\n", __func__);
			}
		}

		retry = 0;
		do {
			g_core_fp.fp_register_write(tmp_addr,
					DATA_LEN_4, send_data, 0);
			g_core_fp.fp_register_read(tmp_addr,
					DATA_LEN_4, recv_data, 0);
			retry++;
		} while ((send_data[3] != recv_data[3]
				|| send_data[2] != recv_data[2]
				|| send_data[1] != recv_data[1]
				|| send_data[0] != recv_data[0])
				&& retry < HIMAX_REG_RETRY_TIMES);
#endif
#endif
	} else {
		g_core_fp.fp_clean_sram_0f(zf_info_arr[0].sram_addr,
				zf_info_arr[0].write_size, 2);
	}
#if 1
	kfree(FW_buf);
ALOC_FW_BUF_FAIL:
#endif
	kfree(zf_info_arr);
ALOC_ZF_INFO_ARR_FAIL:
	return ret;
}

void himax_mcu_firmware_update_0f(const struct firmware *fw_entry)
{
	int ret = 0;
	uint8_t tmp_data[DATA_LEN_4] = {0x01, 0x00, 0x00, 0x00};

	I("%s,Entering - total FW size=%d\n", __func__, (int)fw_entry->size);

	g_core_fp.fp_register_write(pzf_op->addr_system_reset, 4,
			pzf_op->data_system_reset, 0);

	g_core_fp.fp_sense_off(false);

	if ((int)fw_entry->size > HX64K) {
		ret = himax_zf_part_info(fw_entry);
	} else {
		/* first 48K */
		ret = himax_sram_write_crc_check(fw_entry,
			pzf_op->data_sram_start_addr, 0, HX_48K_SZ);
		if (ret != 0)
			E("%s, HW CRC FAIL - Main SRAM 48K\n", __func__);

		/*config info*/
		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(fw_entry,
				pzf_op->data_cfg_info, 0xC000, 128);
			if (ret != 0)
				E("Config info CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_cfg_info,
				128, 2);
		}

		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(fw_entry,
					pzf_op->data_fw_cfg_1, 0xC0FE, 528);
			if (ret != 0)
				E("FW config 1 CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_fw_cfg_1,
				528, 1);
		}

		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(fw_entry,
					pzf_op->data_fw_cfg_3, 0xCA00, 128);
			if (ret != 0)
				E("FW config 3 CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_fw_cfg_3,
				128, 2);
		}

		/*ADC config*/
		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(fw_entry,
					pzf_op->data_adc_cfg_1, 0xD640, 1200);
			if (ret != 0)
				E("ADC config 1 CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_1,
				1200, 2);
		}

		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(fw_entry,
					pzf_op->data_adc_cfg_2, 0xD320, 800);
			if (ret != 0)
				E("ADC config 2 CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_adc_cfg_2,
				800, 2);
		}

		/*mapping table*/
		if (G_POWERONOF == 1) {
			ret = himax_sram_write_crc_check(fw_entry,
				pzf_op->data_map_table, 0xE000, 1536);
			if (ret != 0)
				E("Mapping table CRC Fail!\n");
		} else {
			g_core_fp.fp_clean_sram_0f(pzf_op->data_map_table,
				1536, 2);
		}
	}

	/* set n frame=0*/
	/*if (G_POWERONOF == 1)
	 *	g_core_fp.fp_write_sram_0f(fw_entry,
			pfw_op->addr_set_frame_addr,
			0xC30C, 4);
	 *else
	 * g_core_fp.fp_clean_sram_0f(pfw_op->addr_set_frame_addr, 4, 2);
	 */
	/* reset N frame back to default value 1 for normal mode */
	g_core_fp.fp_register_write(pfw_op->addr_set_frame_addr, 4,
		tmp_data, 0);

	I("%s, End\n", __func__);
}

int hx_0f_op_file_dirly(char *file_name)
{
	int err = NO_ERR, ret;
	const struct firmware *fw_entry = NULL;


	I("%s, Entering,file name = %s\n", __func__, file_name);

	ret = request_firmware(&fw_entry, file_name, private_ts->dev);
	if (ret < 0) {
#if defined(__EMBEDDED_FW__)
		fw_entry = &g_embedded_fw;
		I("%s: Not find FW in userspace, use embedded FW(size:%zu)\n",
			__func__, g_embedded_fw.size);
#else
		E("%s,%d: error code = %d, file maybe fail\n",
				__func__, __LINE__, ret);
		return ret;
#endif
	}

	himax_int_enable(0);

	if (g_f_0f_updat == 1) {
		I("%s:[Warning]Other thread is updating now!\n", __func__);
		err = -1;
		goto END;
	} else {
		I("%s:Entering Update Flow!\n", __func__);
		g_f_0f_updat = 1;
	}

	g_core_fp.fp_firmware_update_0f(fw_entry);
	if (ret >= 0)
		release_firmware(fw_entry);

	g_f_0f_updat = 0;
END:
	I("%s, END\n", __func__);
	return err;
}

int himax_mcu_0f_operation_dirly(void)
{
	int err = NO_ERR, ret;
	const struct firmware *fw_entry = NULL;

	I("%s, Entering,file name = %s\n", __func__, BOOT_UPGRADE_FWNAME);

	ret = request_firmware(&fw_entry, BOOT_UPGRADE_FWNAME,
		private_ts->dev);
	if (ret < 0) {
#if defined(__EMBEDDED_FW__)
		fw_entry = &g_embedded_fw;
		I("%s: Not find FW in userspace, use embedded FW(size:%zu)\n",
			__func__, g_embedded_fw.size);
#else
		E("%s,%d: error code = %d, file maybe fail\n",
				__func__, __LINE__, ret);
		return ret;
#endif
	}

	himax_int_enable(0);

	if (g_f_0f_updat == 1) {
		I("%s:[Warning]Other thread is updating now!\n", __func__);
		err = -1;
		goto END;
	} else {
		I("%s:Entering Update Flow!\n", __func__);
		g_f_0f_updat = 1;
	}

	g_core_fp.fp_firmware_update_0f(fw_entry);
	if (ret >= 0)
		release_firmware(fw_entry);

	g_f_0f_updat = 0;
END:
	I("%s, END\n", __func__);
	return err;
}

void himax_mcu_0f_operation(struct work_struct *work)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;

	I("%s, Entering,file name = %s\n", __func__, BOOT_UPGRADE_FWNAME);

	err = request_firmware(&fw_entry, BOOT_UPGRADE_FWNAME,
		private_ts->dev);
	if (err < 0) {
#if defined(__EMBEDDED_FW__)
		fw_entry = &g_embedded_fw;
		I("%s: Not find FW in userspace, use embedded FW(size:%zu)",
			__func__, g_embedded_fw.size);
#else
		E("%s,%d: error code = %d, file maybe fail\n",
				__func__, __LINE__, err);
		goto END;
#endif
	}

	if (g_f_0f_updat == 1) {
		I("%s:[Warning]Other thread is updating now!\n", __func__);
		goto END;
	} else {
		I("%s:Entering Update Flow!\n", __func__);
		g_f_0f_updat = 1;
	}

	himax_int_enable(0);

	g_core_fp.fp_firmware_update_0f(fw_entry);
	if (err >= 0)
		release_firmware(fw_entry);

//	g_core_fp.fp_reload_disable(0);

//	g_core_fp.fp_power_on_init();
	g_core_fp.fp_read_FW_ver();
	/*msleep (10);*/
	g_core_fp.fp_touch_information();
	g_core_fp.fp_calc_touch_data_size();
	I("%s:End\n", __func__);
	himax_int_enable(1);

	g_f_0f_updat = 0;
END:
	I("%s, END\n", __func__);
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

	g_core_fp.fp_burst_enable(1);

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_register_read(tmp_addr, max_bus_size,
				&temp_info_data[i*max_bus_size], false);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			g_core_fp.fp_register_read(tmp_addr,
				total_size_temp % max_bus_size,
				&temp_info_data[i*max_bus_size],
				false);
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

	g_core_fp.fp_burst_enable(1);

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			g_core_fp.fp_register_read(tmp_addr,
					max_bus_size,
					&temp_info_data[i*max_bus_size],
					false);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			g_core_fp.fp_register_read(tmp_addr,
					total_size_temp % max_bus_size,
					&temp_info_data[i*max_bus_size],
					false);
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
			;
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
	 * filp_close (fn, NULL);
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

void himax_mcu_0f_operation_check(int type)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;
	/* char *firmware_name = "himax.bin"; */

	I("%s, Entering,file name = %s\n", __func__, BOOT_UPGRADE_FWNAME);

	err = request_firmware(&fw_entry, BOOT_UPGRADE_FWNAME,
		private_ts->dev);
	if (err < 0) {
#if defined(__EMBEDDED_FW__)
		fw_entry = &g_embedded_fw;
		I("%s: Not find FW in userspace, use embedded FW(size:%zu)\n",
			__func__, g_embedded_fw.size);
#else
		E("%s,%d: error code = %d\n", __func__, __LINE__, err);
		return;
#endif
	}

	I("first 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
			fw_entry->data[0],
			fw_entry->data[1],
			fw_entry->data[2],
			fw_entry->data[3]);
	I("next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
			fw_entry->data[4],
			fw_entry->data[5],
			fw_entry->data[6],
			fw_entry->data[7]);
	I("and next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
			fw_entry->data[8],
			fw_entry->data[9],
			fw_entry->data[10],
			fw_entry->data[11]);

	g_core_fp.fp_firmware_read_0f(fw_entry, type);

	if (err >= 0)
		release_firmware(fw_entry);
	I("%s, END\n", __func__);
}
#endif

#if 0//defined(HX_CODE_OVERLAY)
int himax_mcu_0f_overlay(int ovl_type, int mode)
{
	return NO_ERR;
}
#endif

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
	 * g_core_fp.fp_flash_write_burst_length =
	 *	himax_mcu_flash_write_burst_length;
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
	g_core_fp.fp_dd_clk_set = himax_mcu_dd_clk_set;
	g_core_fp.fp_dd_reg_en = himax_mcu_dd_reg_en;
	g_core_fp.fp_ic_id_read = himax_mcu_ic_id_read;
	g_core_fp.fp_dd_reg_write = himax_mcu_dd_reg_write;
	g_core_fp.fp_dd_reg_read = himax_mcu_dd_reg_read;
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
#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
	g_core_fp.fp_fw_ver_bin = himax_mcu_fw_ver_bin;
#endif
#if defined(HX_RST_PIN_FUNC)
	g_core_fp.fp_pin_reset = himax_mcu_pin_reset;
	g_core_fp.fp_ic_reset = himax_mcu_ic_reset;
#endif
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
#if defined(HX_SMART_WAKEUP)\
	|| defined(HX_HIGH_SENSE)\
	|| defined(HX_USB_DETECT_GLOBAL)
	g_core_fp.fp_resend_cmd_func = himax_mcu_resend_cmd_func;
#endif
#if defined(HX_TP_PROC_GUEST_INFO)
	g_core_fp.guest_info_get_status = himax_guest_info_get_status;
	g_core_fp.read_guest_info = hx_read_guest_info;
#endif
/* CORE_DRIVER */
#if defined(HX_ZERO_FLASH)
	g_core_fp.fp_reload_disable = hx_dis_rload_0f;
	g_core_fp.fp_clean_sram_0f = himax_mcu_clean_sram_0f;
	g_core_fp.fp_write_sram_0f = himax_mcu_write_sram_0f;
	g_core_fp.fp_write_sram_0f_crc = himax_sram_write_crc_check;
	g_core_fp.fp_firmware_update_0f = himax_mcu_firmware_update_0f;
	g_core_fp.fp_0f_operation = himax_mcu_0f_operation;
	g_core_fp.fp_0f_operation_dirly = himax_mcu_0f_operation_dirly;
	g_core_fp.fp_0f_op_file_dirly = hx_0f_op_file_dirly;
	g_core_fp.fp_0f_excp_check = himax_mcu_0f_excp_check;
#if defined(HX_0F_DEBUG)
	g_core_fp.fp_read_sram_0f = himax_mcu_read_sram_0f;
	g_core_fp.fp_read_all_sram = himax_mcu_read_all_sram;
	g_core_fp.fp_firmware_read_0f = himax_mcu_firmware_read_0f;
	g_core_fp.fp_0f_operation_check = himax_mcu_0f_operation_check;
#endif
#if 0//defined(HX_CODE_OVERLAY)
	g_core_fp.fp_0f_overlay = himax_mcu_0f_overlay;
#endif
#endif
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

	g_internal_buffer = kzalloc(sizeof(uint8_t)*HX_MAX_WRITE_SZ,
		GFP_KERNEL);

	if (g_internal_buffer == NULL) {
		err = -ENOMEM;
		goto err_g_core_cmd_op_g_internal_buffer_fail;
	}
	himax_mcu_fp_init();

	return NO_ERR;

err_g_core_cmd_op_g_internal_buffer_fail:
#if defined(HX_ZERO_FLASH)
	kfree(g_core_cmd_op->zf_op);
	g_core_cmd_op->zf_op = NULL;
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
	himax_parse_assign_cmd(driver_addr_fw_define_rxnum_txnum_maxpt,
		pdriver_op->addr_fw_define_rxnum_txnum_maxpt,
		sizeof(pdriver_op->addr_fw_define_rxnum_txnum_maxpt));
	himax_parse_assign_cmd(driver_addr_fw_define_xy_res_enable,
		pdriver_op->addr_fw_define_xy_res_enable,
		sizeof(pdriver_op->addr_fw_define_xy_res_enable));
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

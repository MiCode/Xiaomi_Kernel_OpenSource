/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for debug nodes
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

#include "himax_debug.h"
#include "himax_ic_core.h"

#if defined(HX_TP_PROC_2T2R)
	bool Is_2T2R;
EXPORT_SYMBOL(Is_2T2R);
	int HX_RX_NUM_2;
	int HX_TX_NUM_2;
#endif

uint8_t g_diag_arr_num;

int g_max_mutual;
int g_min_mutual = 0xFFFF;
int g_max_self;
int g_min_self = 0xFFFF;

/* moved from debug.h */

static char g_file_path[256];

uint8_t proc_reg_addr[4];
uint8_t proc_reg_addr_type;
uint8_t *proc_reg_buf;

struct proc_dir_entry *himax_proc_stack_file;
struct proc_dir_entry *himax_proc_delta_file;
struct proc_dir_entry *himax_proc_dc_file;
struct proc_dir_entry *himax_proc_baseline_file;
bool dsram_flag;

#if defined(HX_TP_PROC_2T2R)
uint32_t *diag_mutual_2;
#endif
int32_t *diag_mutual;
int32_t *diag_mutual_new;
int32_t *diag_mutual_old;
uint8_t hx_state_info[2];
uint8_t diag_coor[128];
int32_t *diag_self;
int32_t *diag_self_new;
int32_t *diag_self_old;

struct proc_dir_entry *himax_proc_debug_file;
bool	fw_update_complete;
bool	fw_update_going;
int handshaking_result;
unsigned char debug_level_cmd;
uint8_t cmd_set[8];
uint8_t mutual_set_flag;

struct proc_dir_entry *himax_proc_flash_dump_file;
uint8_t *g_dump_buffer;
uint8_t g_dump_cmd;
uint8_t g_dump_show;
uint32_t g_dump_addr;
uint32_t g_dump_size;
uint8_t g_flash_progress;
bool g_flash_dump_rst; /*Fail = 0, Pass = 1*/

uint32_t **raw_data_array;
uint8_t X_NUM;
uint8_t Y_NUM;
uint8_t sel_type = 0x0D;

/* Moved from debug.h End */
char buf_tmp[BUF_SIZE] = {0};

struct proc_dir_entry *himax_proc_pen_pos_file;

struct timespec64 timeStart, timeEnd, timeDelta;
int g_switch_mode;
/*
 *	Segment : Himax PROC Debug Function
 */

/****** useful functions ******/
int count_char(char *input)
{
	int count = 0;
	int i = 0;

	for (i = 0; input[i] != '\0'; i++)
		count++;
	return count;
}

#define SIOS_DBG_STR "i = %d, j =%d, idx_find_str=%d, sts_find[%d]=%d\n"

static void _str_to_arr_in_char(char **arr, int arr_size, char *str, char c)
{
	int i = 0;
	int _arr_str_idx = 0;
	int _arr_idx = 0;
	int _arr_max_idx = arr_size;

	for (i = 0; str[i] != '\0'; i++) {
		if (_arr_max_idx <= _arr_idx) {
			I("%s: Oversize!\n", __func__);
			goto END;
		}
		if (str[i] == c || str[i] == '\0') {
			I("%s: String parse compelete!\n", __func__);
			_arr_idx++;
			_arr_str_idx = 0;
		} else {
			arr[_arr_idx][_arr_str_idx] = str[i];
			_arr_str_idx++;
			continue;
		}
	}
END:
	return;
}

bool chk_normal_str(char *str)
{
	int i = 0;
	bool result = false;
	int str_len = strlen(str);

	I("%s, str_len=%d\n", __func__, str_len);
	for (i = 0; i < str_len; i++) {
		if (str[i] >= '0' && str[i] <= '9') {
			result = true;
			continue;
		} else if (str[i] >= 'A' && str[i] <= 'Z') {
			result = true;
			continue;
		} else if (str[i] >= 'a' && str[i] <= 'z') {
			result = true;
			continue;
		} else {
			result = false;
			break;
		}
	}
	return result;
}
bool chk_normal_char(char c)
{
	bool result = false;

	if (c >= '0' && c <= '9')
		result = true;
	else if (c >= 'A' && c <= 'Z')
		result = true;
	else if (c >= 'a' && c <= 'z')
		result = true;
	else
		result = false;

	return result;
}
/* claculate 10's power function */
static int himax_power_cal(int pow, int number, int base)
{
	int i = 0;
	int result = 1;

	for (i = 0; i < pow; i++)
		result *= base;
	result = result * number;

	return result;

}

/* String to int */
static int hiamx_parse_str2int(char *str)
{
	int i = 0;
	int temp_cal = 0;
	int result = -948794;
	unsigned int str_len = strlen(str);
	int negtive_flag = 0;

	for (i = 0; i < str_len; i++) {
		if (i == 0)
			result = 0;
		if (str[i] != '-' && str[i] > '9' && str[i] < '0') {
			E("%s: Parsing fail!\n", __func__);
			result = -9487;
			negtive_flag = 0;
			break;
		}
		if (str[i] == '-') {
			negtive_flag = 1;
			continue;
		}
		temp_cal = str[i] - '0';
		result += himax_power_cal(str_len-i-1, temp_cal, 10);
		/* str's the lowest char is the number's the highest number
		 * So we should reverse this number before using the power
		 * function
		 * -1: starting number is from 0 ex:10^0 = 1,10^1=10
		 */
	}

	if (negtive_flag == 1)
		result = 0 - result;

	return result;
}

/* String in Hex to int */
static int hx_parse_hexstr2int(char *str)
{
	int i = 0;
	int temp_cal = 0;
	int result = -948794;
	unsigned int str_len = strlen(str);
	int negtive_flag = 0;

	for (i = 0; i < str_len; i++) {
		if (i == 0)
			result = 0;
		if (str[i] == '-' && i == 0) {
			negtive_flag = 1;
			continue;
		} else if (str[i] <= '9' && str[i] >= '0') {
			temp_cal = str[i] - '0';
			result += himax_power_cal(str_len-i-1, temp_cal, 16);
		} else if (str[i] <= 'f' && str[i] >= 'a') {
			temp_cal = str[i] - 'a' + 10;
			result += himax_power_cal(str_len-i-1, temp_cal, 16);
		} else if (str[i] <= 'F' && str[i] >= 'A') {
			temp_cal = str[i] - 'A' + 10;
			result += himax_power_cal(str_len-i-1, temp_cal, 16);
		} else {
			E("%s: Parsing fail!\n", __func__);
			result = -9487;
			negtive_flag = 0;
			break;
		}
	}

	if (negtive_flag == 1)
		result = 0 - result;

	return result;
}

/****** end ******/
static int himax_crc_test_read(struct seq_file *m)
{
	int ret = 0;
	uint8_t result = 0;
	uint32_t size = 0;

	g_core_fp.fp_sense_off(true);
	msleep(20);
	if (g_core_fp._diff_overlay_flash() == 1)
		size = FW_SIZE_128k;
	else
		size = FW_SIZE_64k;
	result = g_core_fp.fp_calculateChecksum(false, size);
	g_core_fp.fp_sense_on(0x01);

	if (result)
		seq_printf(m,
				"CRC test is Pass!\n");
	else
		seq_printf(m,
				"CRC test is Fail!\n");


	return ret;
}

static int himax_proc_FW_debug_read(struct seq_file *m)
{
	int ret = 0;
	uint8_t i = 0;
	uint8_t addr[4] = {0};
	uint8_t data[4] = {0};
	int len = 0;

	len = (size_t)(sizeof(dbg_reg_ary)/sizeof(uint32_t));

	for (i = 0; i < len; i++) {
		himax_parse_assign_cmd(dbg_reg_ary[i], addr, 4);
		g_core_fp.fp_register_read(addr, data, DATA_LEN_4);

		seq_printf(m,
		"reg[0-3] : 0x%08X = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
		dbg_reg_ary[i], data[0], data[1], data[2], data[3]);
		I("reg[0-3] : 0x%08X = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
		dbg_reg_ary[i], data[0], data[1], data[2], data[3]);
	}

	return ret;
}

static int himax_attn_read(struct seq_file *m)
{
	int ret = 0;
	struct himax_ts_data *ts_data;

	ts_data = private_ts;

	seq_printf(m, "attn = %x\n",
			himax_int_gpio_read(ts_data->pdata->gpio_irq));

	return ret;
}

static int himax_layout_read(struct seq_file *m)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;

	if (debug_data->is_call_help) {
		seq_printf(m, HELP_LAYOUT);
		debug_data->is_call_help = false;
	} else {
		seq_printf(m, "%d ",
				ts->pdata->abs_x_min);
		seq_printf(m, "%d ",
				ts->pdata->abs_x_max);
		seq_printf(m, "%d ",
				ts->pdata->abs_y_min);
		seq_printf(m, "%d ",
				ts->pdata->abs_y_max);
		 seq_puts(m, "\n");
	}

	return ret;
}

static ssize_t himax_layout_write(char *buf, size_t len)
{
	struct himax_ts_data *ts = private_ts;
	char buf_tmp[5] = {0};
	int i = 0, j = 0, k = 0, ret;
	unsigned long value;
	int layout[4] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (strnstr(buf, "help", len) != NULL) {
		debug_data->is_call_help = true;
		goto END;
	} else {

		for (i = 0; i < 20; i++) {
			if (buf[i] == ',' || buf[i] == '\n') {
				memset(buf_tmp, 0x0, sizeof(buf_tmp));

				if (i - j <= 5) {
					memcpy(buf_tmp, buf + j, i - j);
				} else {
					I("buffer size is over 5 char\n");
					return len;
				}

				j = i + 1;

				if (k < 4) {
					ret = kstrtoul(buf_tmp, 10, &value);
					layout[k++] = value;
				}
			}
		}

		if (k == 4) {
			ts->pdata->abs_x_min = layout[0];
			ts->pdata->abs_x_max = (layout[1] - 1);
			ts->pdata->abs_y_min = layout[2];
			ts->pdata->abs_y_max = (layout[3] - 1);
			I("%d, %d, %d, %d\n",
			ts->pdata->abs_x_min, ts->pdata->abs_x_max,
			ts->pdata->abs_y_min, ts->pdata->abs_y_max);
			input_unregister_device(ts->input_dev);
			himax_input_register(ts);
		} else {
			I("ERR@%d, %d, %d, %d\n",
			ts->pdata->abs_x_min, ts->pdata->abs_x_max,
			ts->pdata->abs_y_min, ts->pdata->abs_y_max);
		}
	}
END:
	return len;
}

#if defined(HX_EXCP_RECOVERY)
#if defined(HW_ED_EXCP_EVENT)
static int himax_excp_cnt_read(struct seq_file *m)
{
	int ret = 0;

	I("%s: enter, %d\n", __func__, __LINE__);

	if (debug_data->is_call_help) {
		seq_printf(m, HELP_EXCPT);
		debug_data->is_call_help = false;
	} else {
		seq_printf(m,
			"EB_cnt = %d, EC_cnt = %d, EE_cnt = %d\n",
			hx_EB_event_flag,
			hx_EC_event_flag,
			hx_EE_event_flag);
	}
	return ret;
}

static ssize_t himax_excp_cnt_write(char *buf, size_t len)
{
	int i = 0;

	if (len >= 12) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (strnstr(buf, "help", len) != NULL) {
		debug_data->is_call_help = true;
	} else {
		if (buf[i] == '0') {
			I("Clear EXCEPTION Flag\n");
			hx_EB_event_flag = 0;
			hx_EC_event_flag = 0;
			hx_EE_event_flag = 0;
		}
	}

	return len;
}
#else
static int himax_excp_cnt_read(struct seq_file *m)
{
	int ret = 0;

	I("%s: enter, %d\n", __func__, __LINE__);
	if (debug_data->is_call_help) {
		seq_printf(m, HELP_EXCPT);
		debug_data->is_call_help = false;
	} else {
		seq_printf(m,
			"EB_cnt = %d, EC_cnt = %d, ED_cnt = %d\n",
			hx_EB_event_flag,
			hx_EC_event_flag,
			hx_ED_event_flag);
	}
	return ret;
}

static ssize_t himax_excp_cnt_write(char *buf, size_t len)
{
	int i = 0;

	if (len >= 12) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (strnstr(buf, "help", len) != NULL) {
		debug_data->is_call_help = true;
	} else {
		I("Clear EXCEPTION Flag\n");

		if (buf[i] == '0') {
			hx_EB_event_flag = 0;
			hx_EC_event_flag = 0;
			hx_ED_event_flag = 0;
		}
	}
	return len;
}
#endif
#endif

static ssize_t himax_sense_on_off_write(char *buf, size_t len)
{
	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (buf[0] == '0') {
		g_core_fp.fp_sense_off(true);
		I("Sense off\n");
	} else if (buf[0] == '1') {
		if (buf[1] == 's') {
			g_core_fp.fp_sense_on(0x00);
			I("Sense on re-map on, run sram\n");
		} else {
			g_core_fp.fp_sense_on(0x01);
			I("Sense on re-map off, run flash\n");
		}
	} else {
		I("Do nothing\n");
	}

	return len;
}

static int test_irq_pin(void)
{
	struct himax_ts_data *ts = private_ts;
	int result = NO_ERR;
	int irq_sts = -1;
	uint8_t tmp_addr[DATA_LEN_4] = {0};
	uint8_t tmp_data[DATA_LEN_4] = {0};
	uint8_t tmp_read[DATA_LEN_4] = {0};

	g_core_fp.fp_sense_off(true);

	I("check IRQ LOW\n");
	usleep_range(20000, 20001);
	himax_parse_assign_cmd(0x90028060, tmp_addr, DATA_LEN_4);
	himax_parse_assign_cmd(0x00000002, tmp_data, DATA_LEN_4);
	g_core_fp.fp_register_write(tmp_addr, tmp_data, DATA_LEN_4);
	usleep_range(20000, 20001);
	g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
	I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);

	usleep_range(20000, 20001);
	himax_parse_assign_cmd(0x90028064, tmp_addr, DATA_LEN_4);
	himax_parse_assign_cmd(0x00000001, tmp_data, DATA_LEN_4);
	g_core_fp.fp_register_write(tmp_addr, tmp_data, DATA_LEN_4);
	usleep_range(20000, 20001);
	g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
	I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);

	usleep_range(20000, 20001);
	himax_parse_assign_cmd(0x90028068, tmp_addr, DATA_LEN_4);
	himax_parse_assign_cmd(0x00000000, tmp_data, DATA_LEN_4);
	g_core_fp.fp_register_write(tmp_addr, tmp_data, DATA_LEN_4);
	usleep_range(20000, 20001);
	g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
	I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);

	usleep_range(20000, 20001);
	irq_sts = himax_int_gpio_read(ts->pdata->gpio_irq);
	if (irq_sts == 0) {
		I("[LOW]Now IRQ is LOW!\n");
		result += NO_ERR;
	} else {
		I("[LOW]Now IRQ is High!\n");
		result += 1;
	}

	I("check IRQ High\n");
	usleep_range(20000, 20001);
	himax_parse_assign_cmd(0x90028060, tmp_addr, DATA_LEN_4);
	himax_parse_assign_cmd(0x00000002, tmp_data, DATA_LEN_4);
	g_core_fp.fp_register_write(tmp_addr, tmp_data, DATA_LEN_4);
	usleep_range(20000, 20001);
	g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
	I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);

	usleep_range(20000, 20001);
	himax_parse_assign_cmd(0x90028064, tmp_addr, DATA_LEN_4);
	himax_parse_assign_cmd(0x00000001, tmp_data, DATA_LEN_4);
	g_core_fp.fp_register_write(tmp_addr, tmp_data, DATA_LEN_4);
	usleep_range(20000, 20001);
	g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
	I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);

	usleep_range(20000, 20001);
	himax_parse_assign_cmd(0x90028068, tmp_addr, DATA_LEN_4);
	himax_parse_assign_cmd(0x00000001, tmp_data, DATA_LEN_4);
	g_core_fp.fp_register_write(tmp_addr, tmp_data, DATA_LEN_4);
	usleep_range(20000, 20001);
	g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
	I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);

	usleep_range(20000, 20001);
	irq_sts = himax_int_gpio_read(ts->pdata->gpio_irq);
	if (irq_sts == 0) {
		I("[High]Now IRQ is LOW!\n");
		result += 1;
	} else {
		I("[High]Now IRQ is High!\n");
		result += NO_ERR;
	}
	debug_data->is_checking_irq = false;

	g_core_fp.fp_sense_on(0x00);

	return result;
}
static int himax_int_en_read(struct seq_file *m)
{
	struct himax_ts_data *ts = private_ts;
	int ret = 0;
	int check_rslt = -1;


	if (debug_data->is_checking_irq) {
		check_rslt = test_irq_pin();
		if (check_rslt == NO_ERR) {
			seq_printf(m,
			"IRQ check OK!\n");
		} else {
			seq_printf(m,
			"IRQ check Fail!\n");
		}
	} else {
		seq_printf(m, "irq_status:%d\n",
		ts->irq_enabled);
	}
	return ret;
}

static ssize_t himax_int_en_write(char *buf, size_t len)
{
	struct himax_ts_data *ts = private_ts;
	int ret = 0;

	if (len >= 12) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	if (buf[0] == '0') {
		himax_int_enable(0);
	} else if (buf[0] == '1') {
		himax_int_enable(1);
	} else if (buf[0] == '2') {
		himax_int_enable(0);
		free_irq(ts->hx_irq, ts);
		ts->irq_enabled = 0;
	} else if (buf[0] == '3') {
		ret = himax_int_en_set();

		if (ret == 0) {
			ts->irq_enabled = 1;
			atomic_set(&ts->irq_state, 1);
		}
	} else if (strnstr(buf, "test", len) != NULL) {
		debug_data->is_checking_irq = true;
		I("Checking IRQ start!\n");
	} else
		return -EINVAL;

	return len;
}

static int himax_irq_info_read(struct seq_file *m)
{
	// struct himax_ts_data *ts = private_ts;
	int ret = 0;

	if (g_core_fp.fp_read_ic_trigger_type() == 1)
		seq_printf(m,
			"IC Interrupt type is edge trigger.\n");
	else if (g_core_fp.fp_read_ic_trigger_type() == 0)
		seq_printf(m,
			"IC Interrupt type is level trigger.\n");
	else
		seq_printf(m,
			"Unkown IC trigger type.\n");

	if (ic_data->HX_INT_IS_EDGE)
		seq_printf(m,
			"Driver register Interrupt : EDGE TIRGGER\n");
	else
		seq_printf(m,
			"Driver register Interrupt : LEVEL TRIGGER\n");

	return ret;
}

static ssize_t himax_irq_info_write(char *buf, size_t len)
{
	// struct himax_ts_data *ts = private_ts;
	// int ret = 0;

	if (len >= 12) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}


	return len;
}

static int himax_debug_level_read(struct seq_file *m)
{
	struct himax_ts_data *ts_data;
	int ret = 0;

	ts_data = private_ts;
	seq_printf(m, "tsdbg: %d\n",
			g_ts_dbg);
	seq_printf(m, "level: %X\n",
			ts_data->debug_log_level);


	return ret;
}

static ssize_t himax_debug_level_write(char *buf, size_t len)
{
	struct himax_ts_data *ts;
	int i;

	ts = private_ts;

	if (len >= 12) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	if (strnstr(buf, "tsdbg", len) != NULL) {
		if (buf[5] == '1') {
			I("Open Ts Debug!\n");
			g_ts_dbg = 1;
		} else if (buf[5] == '0') {
			I("Close Ts Debug!\n");
			g_ts_dbg = 0;
		} else {
			E("Parameter fault for ts debug\n");
		}
	} else {

		ts->debug_log_level = 0;

		for (i = 0; i < len; i++) {
			if (buf[i] >= '0' && buf[i] <= '9')
				ts->debug_log_level |= (buf[i] - '0');
			else if (buf[i] >= 'A' && buf[i] <= 'F')
				ts->debug_log_level |= (buf[i] - 'A' + 10);
			else if (buf[i] >= 'a' && buf[i] <= 'f')
				ts->debug_log_level |= (buf[i] - 'a' + 10);

			if (i != len - 1)
				ts->debug_log_level <<= 4;
		}
		I("Now debug level value=%d\n", ts->debug_log_level);

		if (ts->debug_log_level & BIT(4)) {
			I("Turn on/Enable Debug Mode for Inspection!\n");
			goto END_FUNC;
		}

		if (ts->debug_log_level & BIT(3)) {
			if (ts->pdata->screenWidth > 0
			&& ts->pdata->screenHeight > 0
			&& (ts->pdata->abs_x_max - ts->pdata->abs_x_min) > 0
			&& (ts->pdata->abs_y_max - ts->pdata->abs_y_min) > 0) {
				ts->widthFactor =
					(ts->pdata->screenWidth << SHIFTBITS)
					/ (ts->pdata->abs_x_max
					- ts->pdata->abs_x_min);
				ts->heightFactor =
					(ts->pdata->screenHeight << SHIFTBITS)
					/ (ts->pdata->abs_y_max
					- ts->pdata->abs_y_min);

				if (ts->widthFactor > 0 &&
					ts->heightFactor > 0) {
					ts->useScreenRes = 1;
				} else {
					ts->heightFactor = 0;
					ts->widthFactor = 0;
					ts->useScreenRes = 0;
				}
			} else
				I("En-finger-dbg with raw position mode!\n");
		} else {
			ts->useScreenRes = 0;
			ts->widthFactor = 0;
			ts->heightFactor = 0;
		}
	}
END_FUNC:
	return len;
}

static int himax_proc_register_read(struct seq_file *m)
{
	int ret = 0;
	uint16_t i;

	if (debug_data->is_call_help) {
		seq_printf(m, HELP_REGISTER);
		debug_data->is_call_help = false;
	} else {

		memset(proc_reg_buf, 0x00, 128 * sizeof(uint8_t));

		I("himax_register_show: %02X,%02X,%02X,%02X\n",
			proc_reg_addr[3], proc_reg_addr[2], proc_reg_addr[1],
			proc_reg_addr[0]);

		if (proc_reg_addr_type == 1) {
			ret = himax_bus_read(proc_reg_addr[0], proc_reg_buf,
				128);
			if (ret < 0) {
				E("%s: bus access fail!\n", __func__);
				return BUS_FAIL;
			}
		} else {
			g_core_fp.fp_register_read(proc_reg_addr, proc_reg_buf,
				128);
		}

		seq_printf(m, "command:  %02X,%02X,%02X,%02X\n",
			proc_reg_addr[3], proc_reg_addr[2], proc_reg_addr[1],
			proc_reg_addr[0]);

		for (i = 0; i < 128; i++) {
			seq_printf(m, "0x%2.2X ", proc_reg_buf[i]);
			if ((i % 16) == 15)
				seq_puts(m, "\n");

		}

		 seq_puts(m, "\n");
	}

	return ret;
}

static ssize_t himax_proc_register_write(char *buf, size_t len)
{
	char buff_tmp[16] = {0};
	uint8_t length = 0;
	uint8_t byte_length = 0;
	unsigned long result = 0;
	uint8_t i = 0;
	char *data_str = NULL;
	uint8_t w_data[20] = {0};
	uint8_t x_pos[20] = {0};
	uint8_t count = 0;
	int ret = 0;

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	I("himax %s\n", buf);

	if (strnstr(buf, "help", len) != NULL) {
		debug_data->is_call_help = true;
		goto END;
	}

	memset(proc_reg_addr, 0x0, sizeof(proc_reg_addr));

	if ((buf[0] == 'r' || buf[0] == 'w')
	&& buf[1] == ':' && buf[2] == 'x') {
		length = strlen(buf);

		/* I("%s: length = %d.\n", __func__,length); */
		for (i = 0; i < length; i++) {
			/* find postion of 'x' */
			if (buf[i] == 'x') {
				x_pos[count] = i;
				count++;
			}
		}

		data_str = strrchr(buf, 'x');
		I("%s: %s.\n", __func__, data_str);
		length = strlen(data_str + 1);

		switch (buf[0]) {
		case 'r':
		if (buf[3] == 'F' && buf[4] == 'E' && length == 4) {
			length = length - 2;
			proc_reg_addr_type = 1;
			memcpy(buff_tmp, data_str + 3, length);
		} else {
			proc_reg_addr_type = 0;
			memcpy(buff_tmp, data_str + 1, length);
		}
		byte_length = length / 2;
		if (!kstrtoul(buff_tmp, 16, &result)) {
			for (i = 0; i < byte_length; i++)
				proc_reg_addr[i] = (uint8_t)(result >> (i * 8));
		}

		break;

		case 'w':
		if (buf[3] == 'F' && buf[4] == 'E') {
			proc_reg_addr_type = 1;
			memcpy(buff_tmp, buf + 5, length);
		} else {
			proc_reg_addr_type = 0;
			memcpy(buff_tmp, buf + 3, length);
		}

		if (count < 3) {
			byte_length = length / 2;

			if (!kstrtoul(buff_tmp, 16, &result)) {
				/* command */
				for (i = 0; i < byte_length; i++)
					proc_reg_addr[i] =
						(uint8_t)(result >> (i * 8));
			}

			if (!kstrtoul(data_str + 1, 16, &result)) {
				/* data */
				for (i = 0; i < byte_length; i++)
					w_data[i] = (uint8_t)(result >> i * 8);
			}
		} else {
			for (i = 0; i < count; i++) {
				/* parsing addr after 'x' */
				memset(buff_tmp, 0x0, sizeof(buff_tmp));
				if (proc_reg_addr_type != 0 && i != 0)
					byte_length = 2;
				else
					byte_length = x_pos[1] - x_pos[0] - 2;
					/* original */

				memcpy(buff_tmp, buf+x_pos[i]+1, byte_length);

				if (!kstrtoul(buff_tmp, 16, &result)) {
					if (i == 0)
						proc_reg_addr[i] =
							(uint8_t)(result);
					else
						w_data[i - 1] =
							(uint8_t)(result);
				}
			}

			byte_length = count - 1;
		}

		if (proc_reg_addr_type == 1) {
			ret = himax_bus_write(proc_reg_addr[0], NULL, w_data,
				byte_length);
			if (ret < 0) {
				E("%s: bus access fail!\n", __func__);
				return BUS_FAIL;
			}
		} else {
			g_core_fp.fp_register_write(proc_reg_addr, w_data,
				byte_length);
		}

		break;
		};
	}
END:
	return len;
}

int32_t *getMutualBuffer(void)
{
	return diag_mutual;
}
int32_t *getMutualNewBuffer(void)
{
	return diag_mutual_new;
}
int32_t *getMutualOldBuffer(void)
{
	return diag_mutual_old;
}
int32_t *getSelfBuffer(void)
{
	return diag_self;
}
int32_t *getSelfNewBuffer(void)
{
	return diag_self_new;
}
int32_t *getSelfOldBuffer(void)
{
	return diag_self_old;
}
void setMutualBuffer(uint8_t x_num, uint8_t y_num)
{
	diag_mutual = kzalloc(x_num * y_num * sizeof(int32_t), GFP_KERNEL);
}
void setMutualNewBuffer(uint8_t x_num, uint8_t y_num)
{
	diag_mutual_new = kzalloc(x_num * y_num * sizeof(int32_t), GFP_KERNEL);
}
void setMutualOldBuffer(uint8_t x_num, uint8_t y_num)
{
	diag_mutual_old = kzalloc(x_num * y_num * sizeof(int32_t), GFP_KERNEL);
}
void setSelfBuffer(uint8_t x_num, uint8_t y_num)
{
	diag_self = kzalloc((x_num + y_num) * sizeof(int32_t), GFP_KERNEL);
}
void setSelfNewBuffer(uint8_t x_num, uint8_t y_num)
{
	diag_self_new = kzalloc((x_num + y_num) * sizeof(int32_t), GFP_KERNEL);
}
void setSelfOldBuffer(uint8_t x_num, uint8_t y_num)
{
	diag_self_old = kzalloc((x_num + y_num) * sizeof(int32_t), GFP_KERNEL);
}

#if defined(HX_TP_PROC_2T2R)
int32_t *getMutualBuffer_2(void)
{
	return diag_mutual_2;
}
void setMutualBuffer_2(uint8_t x_num_2, uint8_t y_num_2)
{
	diag_mutual_2 =
	  kzalloc(x_num_2 * y_num_2 * sizeof(int32_t), GFP_KERNEL);
}
#endif

int himax_set_diag_cmd(struct himax_ic_data *ic_data,
		struct himax_report_data *hx_touch_data)
{
	struct himax_ts_data *ts = private_ts;
	int32_t *mutual_data;
	int32_t *self_data;
	int mul_num;
	int self_num;
	/* int RawDataLen = 0; */
	hx_touch_data->diag_cmd = ts->diag_cmd;

	if (hx_touch_data->diag_cmd >= 1 && hx_touch_data->diag_cmd <= 7) {
		/* Check event stack CRC */
		if (!g_core_fp.fp_diag_check_sum(hx_touch_data))
			goto bypass_checksum_failed_packet;

#if defined(HX_TP_PROC_2T2R)
		if (Is_2T2R && (hx_touch_data->diag_cmd >= 4 &&
			hx_touch_data->diag_cmd <= 6)) {
			mutual_data = getMutualBuffer_2();
			self_data = getSelfBuffer();
			/* initiallize the block number of mutual and self */
			mul_num = ic_data->HX_RX_NUM_2 * ic_data->HX_TX_NUM_2;
			self_num = ic_data->HX_RX_NUM_2 + ic_data->HX_TX_NUM_2;
		} else
#endif
		{
			mutual_data = getMutualBuffer();
			self_data = getSelfBuffer();
			/* initiallize the block number of mutual and self */
			mul_num = ic_data->HX_RX_NUM * ic_data->HX_TX_NUM;
			self_num = ic_data->HX_RX_NUM + ic_data->HX_TX_NUM;
		}
		g_core_fp.fp_diag_parse_raw_data(hx_touch_data, mul_num,
			self_num, hx_touch_data->diag_cmd, mutual_data,
			self_data);
	} else if (hx_touch_data->diag_cmd == 8) {
		memset(diag_coor, 0x00, sizeof(diag_coor));
		memcpy(&(diag_coor[0]), &hx_touch_data->hx_coord_buf[0],
		  hx_touch_data->touch_info_size);
	}

	/* assign state info data */
	memcpy(&(hx_state_info[0]), &hx_touch_data->hx_state_info[0], 2);
	return NO_ERR;
bypass_checksum_failed_packet:
	return 1;
}

/* #if defined(HX_DEBUG_LEVEL) */
void himax_log_touch_data(int start)
{
	int i = 0;
	int print_size = 0;
	uint8_t *buf = NULL;

	if (start == 1)
		return; /* report data when end of ts_work*/

	if (hx_touch_data->diag_cmd > 0) {
		print_size = hx_touch_data->touch_all_size;
		buf = kcalloc(print_size, sizeof(uint8_t), GFP_KERNEL);
		if (buf == NULL) {
			E("%s, Failed to allocate memory\n", __func__);
			return;
		}

		memcpy(buf, hx_touch_data->hx_coord_buf,
		  hx_touch_data->touch_info_size);
		memcpy(&buf[hx_touch_data->touch_info_size],
		  hx_touch_data->hx_rawdata_buf,
		  print_size - hx_touch_data->touch_info_size);
	}
#if defined(HX_SMART_WAKEUP)
	else if (private_ts->SMWP_enable > 0 && private_ts->suspended) {
		print_size = hx_touch_data->event_size;
		buf = kcalloc(print_size, sizeof(uint8_t), GFP_KERNEL);
		if (buf == NULL) {
			E("%s, Failed to allocate memory\n", __func__);
			return;
		}

		memcpy(buf, hx_touch_data->hx_event_buf, print_size);
	}
#endif
	else if (hx_touch_data->diag_cmd == 0) {
		print_size = hx_touch_data->touch_info_size;
		buf = kcalloc(print_size, sizeof(uint8_t), GFP_KERNEL);
		if (buf == NULL) {
			E("%s, Failed to allocate memory\n", __func__);
			return;
		}

		memcpy(buf, hx_touch_data->hx_coord_buf, print_size);
	} else {
		E("%s:cmd fault\n", __func__);
		return;
	}

	for (i = 0; i < print_size; i += 8) {
		if ((i + 7) >= print_size) {
			I("P %2d = 0x%2.2X P %2d = 0x%2.2X ",
				i,
				buf[i],
				i + 1,
				buf[i + 1]);
			I("P %2d = 0x%2.2X P %2d = 0x%2.2X\n",
				i + 2,
				buf[i + 2],
				i + 3,
				buf[i + 3]);
			break;
		}

		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ",
		  i, buf[i], i + 1, buf[i + 1]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ",
		  i + 2, buf[i + 2], i + 3, buf[i + 3]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ",
		  i + 4, buf[i + 4], i + 5, buf[i + 5]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ",
		  i + 6, buf[i + 6], i + 7, buf[i + 7]);
		I("\n");
	}
	kfree(buf);
}

void himax_log_touch_event(struct himax_ts_data *ts, int start)
{
	int i = 0;

	if (start == 1)
		return;   /*report data when end of ts_work*/

	if (g_target_report_data->finger_on > 0 &&
		g_target_report_data->finger_num > 0) {
		for (i = 0; i < ts->nFinger_support; i++) {
			if (g_target_report_data->p[i].x >= 0
			&& g_target_report_data->p[i].x
			<= ts->pdata->abs_x_max
			&& g_target_report_data->p[i].y >= 0
			&& g_target_report_data->p[i].y
			<= ts->pdata->abs_y_max) {
				I(PRT_LOG,
					i + 1,
					g_target_report_data->p[i].x,
					g_target_report_data->p[i].y,
					g_target_report_data->p[i].w,
					g_target_report_data->p[i].w,
					i + 1,
					g_target_report_data->ig_count);
				}
		}
	} else if (g_target_report_data->finger_on == 0
	&& g_target_report_data->finger_num == 0) {
		I("All Finger leave\n");
	} else {
		I("%s : wrong input!\n", __func__);
	}
}
void himax_log_touch_int_devation(int touched)
{
	if (touched == HX_FINGER_ON) {
		ktime_get_ts64(&timeStart);
		/* I(" Irq start time = %ld.%06ld s\n",
		 * timeStart.tv_sec, timeStart.tv_nsec/1000);
		 */
	} else if (touched == HX_FINGER_LEAVE) {
		ktime_get_ts64(&timeEnd);
		timeDelta.tv_nsec =
		  (timeEnd.tv_sec * 1000000000 + timeEnd.tv_nsec) -
		  (timeStart.tv_sec * 1000000000 + timeStart.tv_nsec);
		/*  I("Irq finish time = %ld.%06ld s\n",
		 *	timeEnd.tv_sec, timeEnd.tv_nsec/1000);
		 */
		I("Touch latency = %ld us\n", timeDelta.tv_nsec / 1000);
		I("bus_speed = %d kHz\n", private_ts->bus_speed);
		if (g_target_report_data->finger_on == 0
		&& g_target_report_data->finger_num == 0)
			I("All Finger leave\n");
	} else {
		I("%s : wrong input!\n", __func__);
	}
}

void himax_log_touch_event_detail(struct himax_ts_data *ts, int start)
{
	int i = 0;

	if (start == HX_FINGER_LEAVE) {
		for (i = 0; i < ts->nFinger_support; i++) {
			if (((ts->old_finger >> i & 1) == 0)
			&& ((ts->pre_finger_mask >> i & 1) == 1)) {
				if (g_target_report_data->p[i].x >= 0
				&& g_target_report_data->p[i].x
				<= ts->pdata->abs_x_max
				&& g_target_report_data->p[i].y >= 0
				&& g_target_report_data->p[i].y
				<= ts->pdata->abs_y_max) {
					I(RAW_DOWN_STATUS, i + 1,
					g_target_report_data->p[i].x,
					g_target_report_data->p[i].y,
					g_target_report_data->p[i].w);
				}
			} else if ((((ts->old_finger >> i & 1) == 1)
			&& ((ts->pre_finger_mask >> i & 1) == 0))) {
				I(RAW_UP_STATUS, i + 1,
					ts->pre_finger_data[i][0],
					ts->pre_finger_data[i][1]);
			} else {
				/* I("dbg hx_point_num=%d, old_finger=0x%02X,"
				 * " pre_finger_mask=0x%02X\n",
				 * ts->hx_point_num, ts->old_finger,
				 * ts->pre_finger_mask);
				 */
			}
		}
	}
}

void himax_ts_dbg_func(struct himax_ts_data *ts, int start)
{
	if (ts->debug_log_level & BIT(0)) {
		/* I("debug level 1\n"); */
		himax_log_touch_data(start);
	}
	if (ts->debug_log_level & BIT(1)) {
		/* I("debug level 2\n"); */
		himax_log_touch_event(ts, start);
	}
	if (ts->debug_log_level & BIT(2)) {
		/* I("debug level 4\n"); */
		himax_log_touch_int_devation(start);
	}
	if (ts->debug_log_level & BIT(3)) {
		/* I("debug level 8\n"); */
		himax_log_touch_event_detail(ts, start);
	}
}

static int himax_change_mode(uint8_t str_pw, uint8_t end_pw)
{
	uint8_t data[4] = {0};
	int count = 0;

	/*sense off*/
	g_core_fp.fp_sense_off(true);
	/*mode change*/
	data[1] = str_pw; data[0] = str_pw;
	if (g_core_fp.fp_assign_sorting_mode != NULL)
		g_core_fp.fp_assign_sorting_mode(data);

	/*sense on*/
	g_core_fp.fp_sense_on(1);
	/*wait mode change*/
	do {
		if (g_core_fp.fp_check_sorting_mode != NULL)
			g_core_fp.fp_check_sorting_mode(data);
		if ((data[0] == end_pw) && (data[1] == end_pw))
			return 0;

		I("Now retry %d times!\n", count);
		count++;
		msleep(50);
	} while (count < 50);

	return ERR_WORK_OUT;
}

static ssize_t himax_irq_dbg_cmd_write(char *buf, size_t len)
{
	// struct himax_ts_data *ts = private_ts;
	int cmd = 0;

	if (!kstrtoint(buf, 16, &cmd)) {
		I("%s, now irq_dbg status=%d!\n", __func__, cmd);
		g_ts_dbg = cmd;
	} else {
		E("%s: command not int!\n", __func__);
	}
	return len;
}

static ssize_t himax_diag_cmd_write(char *buf, size_t len)
{
	struct himax_ts_data *ts = private_ts;
	char *dbg_map_str = "mode:";
	char *str_ptr = NULL;
	int str_len = 0;
	int rst = 0;
	uint8_t str_pw = 0;
	uint8_t end_pw = 0;

	if (strnstr(buf, "help", len) != NULL) {
		debug_data->is_call_help = true;
		goto END;
	}

	switch (len) {
	case 1:/*raw out select - diag,X*/
		if (!kstrtoint(buf, 16, &rst)) {
			ts->diag_cmd = rst;
			I("%s: dsram_flag = %d\n", __func__, dsram_flag);
			if (dsram_flag) {
				/*Cancal work queue and return to stack*/
				process_type = 0;
				while (dsram_flag == true)
					usleep_range(10000, 11000);
				cancel_delayed_work(&ts->himax_diag_delay_work);
				himax_int_enable(1);
				g_core_fp.fp_return_event_stack();
			}
			g_core_fp.fp_diag_register_set(ts->diag_cmd, 0, false);
			I("%s: Set raw out select 0x%X.\n",
					__func__, ts->diag_cmd);
		}
		if (!ts->diag_cmd) {
			if (mode_flag) /*back to normal mode*/
				himax_change_mode(0x00, 0x99);
		}
		break;
	case 2:/*data processing + rawout select - diag,XY*/
		if (!kstrtoint(buf, 16, &rst)) {
			process_type = (rst >> 4) & 0xF;
			ts->diag_cmd = rst & 0xF;
		}
		if (ts->diag_cmd == 0)
			break;
		else if (process_type > 0 && process_type <= 3) {
			if (!dsram_flag) {
				/*Start wrok queue*/
				himax_int_enable(0);
				g_core_fp.fp_diag_register_set(ts->diag_cmd,
					process_type, false);

				queue_delayed_work(ts->himax_diag_wq,
				&ts->himax_diag_delay_work, 2 * HZ / 100);
				dsram_flag = true;

				I("%s: Start get raw data in DSRAM\n",
					__func__);
			} else {
				g_core_fp.fp_diag_register_set(ts->diag_cmd,
					process_type, false);
			}
		}
		break;
	case 4:/*data processing + rawout select - diag,XXYY*/
		/*ex:XXYY=010A=dsram rawdata*/
		I("%s, now case 4\n", __func__);
		if (!kstrtoint(buf, 16, &rst)) {
			process_type = (rst >> 8) & 0xFF;
			ts->diag_cmd = rst & 0xFF;
			I("%s:process_type=0x%02X, diag_cmd=0x%02X\n",
			__func__, process_type, ts->diag_cmd);
		}
		if (process_type <= 0 || ts->diag_cmd <= 0)
			break;
		else if (process_type > 0 && process_type <= 3) {
			if (!dsram_flag) {
				/*Start wrok queue*/
				himax_int_enable(0);
				g_core_fp.fp_diag_register_set(ts->diag_cmd,
					process_type, true);

				queue_delayed_work(ts->himax_diag_wq,
				&ts->himax_diag_delay_work, 2 * HZ / 100);
				dsram_flag = true;

				I("%s: Start get raw data in DSRAM\n",
					__func__);
			} else {
				g_core_fp.fp_diag_register_set(ts->diag_cmd,
					process_type, true);
			}
		}
		break;
	case 9:/*change mode - mode:XXYY(start PW,end PW)*/
		str_ptr = strnstr(buf, dbg_map_str, len);
		if (str_ptr) {
			str_len = strlen(dbg_map_str);
			if (!kstrtoint(buf + str_len, 16, &rst)) {
				str_pw = (rst >> 8) & 0xFF;
				end_pw = rst & 0xFF;
				if (!himax_change_mode(str_pw, end_pw)) {
					mode_flag = 1;
					I(PRT_OK_LOG, __func__,
					rst, str_pw, end_pw);
				} else
					I(PRT_FAIL_LOG,	__func__,
					str_pw, end_pw);
			}
		} else {
			I("%s: Can't find string [%s].\n",
					__func__, dbg_map_str);
		}
		break;
	default:
		I("%s: Length is not correct.\n", __func__);
	}
END:
	return len;
}

static int himax_diag_arrange_read(struct seq_file *m)
{
	int ret = NO_ERR;

	if (debug_data->is_call_help) {
		seq_printf(m, HELP_DIAG_ARR);
		debug_data->is_call_help = false;
	} else {
		seq_printf(m, "diag value=%d\n", g_diag_arr_num);
	}
	return ret;
}

static ssize_t himax_diag_arrange_write(char *buf, size_t len)
{
	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (strnstr(buf, "help", len) != NULL) {
		debug_data->is_call_help = true;
	} else {
		g_diag_arr_num = buf[0] - '0';
		I("%s: g_diag_arr_num = %d\n", __func__, g_diag_arr_num);
	}
	return len;
}

void himax_get_mutual_edge(void)
{
	int i = 0;

	for (i = 0; i < (ic_data->HX_RX_NUM * ic_data->HX_TX_NUM); i++) {
		if (diag_mutual[i] > g_max_mutual)
			g_max_mutual = diag_mutual[i];

		if (diag_mutual[i] < g_min_mutual)
			g_min_mutual = diag_mutual[i];
	}
}

void himax_get_self_edge(void)
{
	int i = 0;

	for (i = 0; i < (ic_data->HX_RX_NUM + ic_data->HX_TX_NUM); i++) {
		if (diag_self[i] > g_max_self)
			g_max_self = diag_self[i];

		if (diag_self[i] < g_min_self)
			g_min_self = diag_self[i];
	}
}

static void print_state_info(struct seq_file *s)
{
	/* seq_printf(s, "State_info_2bytes:%3d, %3d\n",
	 * _state_info[0],hx_state_info[1]);
	 */

#if defined(HX_NEW_EVENT_STACK_FORMAT)
	seq_printf(s, "ReCal = %d\t", hx_state_info[0] & 0x03);
	seq_printf(s, "Base Line = %d\t", hx_state_info[0] >> 2 & 0x01);
	seq_printf(s, "Palm = %d\t", hx_state_info[0] >> 3 & 0x01);
	seq_printf(s, "Idle mode = %d\t", hx_state_info[0] >> 4 & 0x01);
	seq_printf(s, "Water = %d\n", hx_state_info[0] >> 5 & 0x01);
	seq_printf(s, "TX Hop = %d\t", hx_state_info[0] >> 6 & 0x01);
	seq_printf(s, "AC mode = %d\t", hx_state_info[0] >> 7 & 0x01);
	seq_printf(s, "Glove = %d\t", hx_state_info[1] & 0x01);
	seq_printf(s, "Stylus = %d\t", hx_state_info[1] >> 1 & 0x01);
	seq_printf(s, "Hovering = %d\t", hx_state_info[1] >> 2 & 0x01);
	seq_printf(s, "Proximity = %d\t", hx_state_info[1] >> 3 & 0x01);
	seq_printf(s, "KEY = %d\n", hx_state_info[1] >> 4 & 0x0F);
#else
	seq_printf(s, "ReCal = %d\t", hx_state_info[0] & 0x01);
	seq_printf(s, "Palm = %d\t", hx_state_info[0] >> 1 & 0x01);
	seq_printf(s, "AC mode = %d\t", hx_state_info[0] >> 2 & 0x01);
	seq_printf(s, "Water = %d\n", hx_state_info[0] >> 3 & 0x01);
	seq_printf(s, "Glove = %d\t", hx_state_info[0] >> 4 & 0x01);
	seq_printf(s, "TX Hop = %d\t", hx_state_info[0] >> 5 & 0x01);
	seq_printf(s, "Base Line = %d\t", hx_state_info[0] >> 6 & 0x01);
	seq_printf(s, "OSR Hop = %d\t", hx_state_info[1] >> 3 & 0x01);
	seq_printf(s, "KEY = %d\n", hx_state_info[1] >> 4 & 0x0F);
#endif
}

static void himax_diag_arrange_print(struct seq_file *s, int i, int j,
				int transpose)
{
	if (transpose)
		seq_printf(s, "%6d", diag_mutual[j + i * ic_data->HX_RX_NUM]);
	else
		seq_printf(s, "%6d", diag_mutual[i + j * ic_data->HX_RX_NUM]);
}

/* ready to print second step which is column*/
static void himax_diag_arrange_inloop(struct seq_file *s, int in_init,
				int out_init, bool transpose, int j)
{
	int x_channel = ic_data->HX_RX_NUM;
	int y_channel = ic_data->HX_TX_NUM;
	int i;
	int in_max = 0;

	if (transpose)
		in_max = y_channel;
	else
		in_max = x_channel;

	if (in_init > 0) { /* bit0 = 1 */
		for (i = in_init - 1; i >= 0; i--)
			himax_diag_arrange_print(s, i, j, transpose);

		if (transpose) {
			if (out_init > 0)
				seq_printf(s, " %5d\n", diag_self[j]);
			else
				seq_printf(s, " %5d\n",
					diag_self[x_channel - j - 1]);
		}
	} else {	/* bit0 = 0 */
		for (i = 0; i < in_max; i++)
			himax_diag_arrange_print(s, i, j, transpose);

		if (transpose) {
			if (out_init > 0)
				seq_printf(s, " %5d\n",
					diag_self[x_channel - j - 1]);
			else
				seq_printf(s, " %5d\n", diag_self[j]);
		}
	}
}

/* print first step which is row */
static void himax_diag_arrange_outloop(struct seq_file *s, int transpose,
				int out_init, int in_init)
{
	int j;
	int x_channel = ic_data->HX_RX_NUM;
	int y_channel = ic_data->HX_TX_NUM;
	int out_max = 0;
	int self_cnt = 0;

	if (transpose)
		out_max = x_channel;
	else
		out_max = y_channel;

	if (out_init > 0) { /* bit1 = 1 */
		self_cnt = 1;

		for (j = out_init - 1; j >= 0; j--) {
			seq_printf(s, "%3c%02d%c", '[', j + 1, ']');
			himax_diag_arrange_inloop(s, in_init, out_init,
					transpose, j);

			if (!transpose) {
				seq_printf(s, " %5d\n",
				  diag_self[y_channel + x_channel - self_cnt]);
				self_cnt++;
			}
		}
	} else {	/* bit1 = 0 */
		/* self_cnt = x_channel; */
		for (j = 0; j < out_max; j++) {
			seq_printf(s, "%3c%02d%c", '[', j + 1, ']');
			himax_diag_arrange_inloop(s, in_init, out_init,
					transpose, j);

			if (!transpose) {
				seq_printf(s, " %5d\n",
					diag_self[j + x_channel]);
			}
		}
	}
}

/* determin the output format of diag */
static void himax_diag_arrange(struct seq_file *s)
{
	int x_channel = ic_data->HX_RX_NUM;
	int y_channel = ic_data->HX_TX_NUM;
	int bit2, bit1, bit0;
	int i;
	/* rotate bit */
	bit2 = g_diag_arr_num >> 2;
	/* reverse Y */
	bit1 = g_diag_arr_num >> 1 & 0x1;
	/* reverse X */
	bit0 = g_diag_arr_num & 0x1;

	if (g_diag_arr_num < 4) {
		for (i = 0 ; i <= x_channel; i++)
			seq_printf(s, "%3c%02d%c", '[', i, ']');

		seq_puts(s, "\n");
		himax_diag_arrange_outloop(s, bit2, bit1 * y_channel,
					bit0 * x_channel);
		seq_printf(s, "%6c", ' ');

		if (bit0 == 1) {
			for (i = x_channel - 1; i >= 0; i--)
				seq_printf(s, "%6d", diag_self[i]);
		} else {
			for (i = 0; i < x_channel; i++)
				seq_printf(s, "%6d", diag_self[i]);
		}
	} else {
		for (i = 0 ; i <= y_channel; i++)
			seq_printf(s, "%3c%02d%c", '[', i, ']');

		seq_puts(s, "\n");
		himax_diag_arrange_outloop(s, bit2, bit1 * x_channel,
					bit0 * y_channel);
		seq_printf(s, "%6c", ' ');

		if (bit1 == 1) {
			for (i = x_channel + y_channel - 1; i >= x_channel;
			  i--)
				seq_printf(s, "%6d", diag_self[i]);
		} else {
			for (i = x_channel; i < x_channel + y_channel; i++)
				seq_printf(s, "%6d", diag_self[i]);
		}
	}
}

/* DSRAM thread */
bool himax_ts_diag_func(int dsram_type)
{
	int i = 0, j = 0;
	unsigned int index = 0;
	int x_channel = ic_data->HX_RX_NUM;
	int y_channel = ic_data->HX_TX_NUM;
	int total_size = (y_channel * x_channel + y_channel + x_channel) * 2;
	uint8_t *info_data = NULL;
	int32_t *mutual_data = NULL;
	int32_t *mutual_data_new = NULL;
	int32_t *mutual_data_old = NULL;
	int32_t *self_data = NULL;
	int32_t *self_data_new = NULL;
	int32_t *self_data_old = NULL;
	int32_t new_data;
	/* 1:common dsram,2:100 frame Max,3:N-(N-1)frame */
	if (dsram_type < 1 || dsram_type > 3) {
		E("%s: type %d is out of range\n", __func__, dsram_type);
		return false;
	}

	info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (info_data == NULL) {
		E("%s: Failed to allocate memory\n", __func__);
		return false;
	}

	memset(info_data, 0, total_size * sizeof(uint8_t));

	if (dsram_type == 1 || dsram_type == 2) {
		mutual_data = getMutualBuffer();
		self_data = getSelfBuffer();
	} else if (dsram_type == 3) {
		mutual_data = getMutualBuffer();
		mutual_data_new = getMutualNewBuffer();
		mutual_data_old = getMutualOldBuffer();
		self_data = getSelfBuffer();
		self_data_new = getSelfNewBuffer();
		self_data_old = getSelfOldBuffer();
	}

	if (!g_core_fp.fp_get_DSRAM_data(info_data, dsram_flag)) {
		E("%s: Get DSRAM data failed\n", __func__);
		kfree(info_data);
		return false;
	}

	index = 0;

	for (i = 0; i < y_channel; i++) { /*mutual data*/
		for (j = 0; j < x_channel; j++) {
			new_data = (((int8_t)info_data[index + 1] << 8) |
				info_data[index]);

			if (dsram_type <= 1) {
				mutual_data[i * x_channel + j] = new_data;
			} else if (dsram_type == 2) { /* Keep max data */
				if (mutual_data[i * x_channel + j] < new_data)
					mutual_data[i * x_channel + j] =
						new_data;
			} else if (dsram_type == 3) {
				/* Cal data for [N]-[N-1] frame */
				mutual_data_new[i * x_channel + j] = new_data;
				mutual_data[i * x_channel + j] =
					mutual_data_new[i * x_channel + j]
					- mutual_data_old[i * x_channel + j];
			}
			index += 2;
		}
	}

	for (i = 0; i < x_channel + y_channel; i++) { /*self data*/
		new_data = (((int8_t)info_data[index + 1] << 8) |
				info_data[index]);
		if (dsram_type <= 1) {
			self_data[i] = new_data;
		} else if (dsram_type == 2) { /* Keep max data */
			if (self_data[i] < new_data)
				self_data[i] = new_data;
		} else if (dsram_type == 3) { /* Cal data for [N]-[N-1] frame */
			self_data_new[i] = new_data;
			self_data[i] = self_data_new[i] - self_data_old[i];
		}
		index += 2;
	}

	kfree(info_data);

	if (dsram_type == 3) {
		memcpy(mutual_data_old, mutual_data_new,
			x_channel * y_channel * sizeof(int32_t));
			/* copy N data to N-1 array */
		memcpy(self_data_old, self_data_new,
			(x_channel + y_channel) * sizeof(int32_t));
			/* copy N data to N-1 array */
	}

	return true;
}

static int himax_diag_print(struct seq_file *s, void *v)
{
	int x_num = ic_data->HX_RX_NUM;
	int y_num = ic_data->HX_TX_NUM;
	size_t ret = 0;
	uint16_t mutual_num, self_num, width;

	mutual_num	= x_num * y_num;
	self_num	= x_num + y_num;
	/* don't add KEY_COUNT */
	width		= x_num;
	seq_printf(s, "ChannelStart: %4d, %4d\n\n", x_num, y_num);

	/*	start to show out the raw data in adb shell */
	himax_diag_arrange(s);
	seq_puts(s, "\n");
	seq_puts(s, "ChannelEnd");
	seq_puts(s, "\n");

	/* print Mutual/Slef Maximum and Minimum */
	himax_get_mutual_edge();
	himax_get_self_edge();
	seq_printf(s, "Mutual Max:%3d, Min:%3d\n", g_max_mutual,
		g_min_mutual);
	seq_printf(s, "Self Max:%3d, Min:%3d\n", g_max_self,
		g_min_self);
	/* recovery status after print*/
	g_max_mutual = 0;
	g_min_mutual = 0xFFFF;
	g_max_self = 0;
	g_min_self = 0xFFFF;

	/*pring state info*/
	print_state_info(s);

	if (s->count >= s->size)
		overflow++;

	return ret;
}

static int himax_stack_show(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = private_ts;

	if (debug_data->is_call_help) {
		seq_puts(s, HELP_DIAG);
		debug_data->is_call_help = false;
	} else {
		if (ts->diag_cmd)
			himax_diag_print(s, v);
		else
			seq_puts(s, "Please set raw out select 'echo diag,X > debug'\n\n");
	}
	return 0;
}

static int himax_stack_open(struct inode *inode, struct file *file)
{return single_open(file, himax_stack_show, NULL); } 

static const struct proc_ops himax_stack_ops = {
	.proc_open = himax_stack_open,
	.proc_read = seq_read,
};

//__CREATE_OREAD_NODE_HX(stack);

static int himax_sram_read(struct seq_file *s, void *v, uint8_t rs)
{
	struct himax_ts_data *ts = private_ts;
	int d_type = 0;
	int current_size = 
		((ic_data->HX_TX_NUM + 1) * (ic_data->HX_RX_NUM + 1)
		+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) * 10 + 256;
	if (debug_data->is_call_help) {
		seq_puts(s, HELP_DIAG);
		debug_data->is_call_help = false;
	} else {
		d_type = (!ts->diag_cmd)?rs:ts->diag_cmd;

		s->size = current_size;
		I("%s, s->size = %d\n", __func__, (int)s->size);

		s->buf = kcalloc(s->size, sizeof(char), GFP_KERNEL);
		if (s->buf == NULL) {
			E("%s,%d: Memory allocation falied!\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		memset(s->buf, 0, s->size * sizeof(char));

		if (!overflow) {
			if (!process_type) {
				himax_int_enable(0);
				g_core_fp.fp_diag_register_set(d_type,
					0, false);

				/* use process type 1 for default */
				if (!himax_ts_diag_func(1))
					seq_puts(s, "Get sram data failed.");
				else
					himax_diag_print(s, v);

				ts->diag_cmd = 0;
				g_core_fp.fp_diag_register_set(0, 0, false);
				himax_int_enable(1);
			}
		}

		if ((process_type <= 3
		&& ts->diag_cmd
		&& dsram_flag)
		|| overflow) {
			himax_diag_print(s, v);
			overflow = 0;
		}
	}
	return 0;
}

static int himax_delta_show(struct seq_file *s, void *v)
{
	return himax_sram_read(s, v, 0x09);
}

static int himax_delta_open(struct inode *inode, struct file *file)
{return single_open(file, himax_delta_show, NULL); } 
static const struct proc_ops himax_delta_ops = {
	.proc_open = himax_delta_open,
	.proc_read = seq_read,
};

//__CREATE_OREAD_NODE_HX(delta);

static int himax_dc_show(struct seq_file *s, void *v)
{
	return himax_sram_read(s, v, 0x0A);
}
static int himax_dc_open(struct inode *inode, struct file *file)
{return single_open(file, himax_dc_show, NULL); } 
static const struct proc_ops himax_dc_ops = {
	.proc_open = himax_dc_open,
	.proc_read = seq_read,
};
//__CREATE_OREAD_NODE_HX(dc);

static int himax_baseline_show(struct seq_file *s, void *v)
{
	return himax_sram_read(s, v, 0x0B);
}
static int himax_baseline_open(struct inode *inode, struct file *file)
{return single_open(file, himax_baseline_show, NULL); } 
static const struct proc_ops himax_baseline_ops = {
	.proc_open = himax_baseline_open,
	.proc_read = seq_read,
};
//__CREATE_OREAD_NODE_HX(baseline);

#if defined(HX_RST_PIN_FUNC)
static void test_rst_pin(void)
{
	int rst_sts1 = -1;
	int rst_sts2 = -1;
	int cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4] = {0};
	uint8_t tmp_data[DATA_LEN_4] = {0};
	uint8_t tmp_read[DATA_LEN_4] = {0};

	himax_int_enable(0);
	g_core_fp.fp_sense_off(true);


	usleep_range(20000, 20001);
	himax_parse_assign_cmd(0x900000F0, tmp_addr, DATA_LEN_4);
	himax_parse_assign_cmd(0x00000001, tmp_data, DATA_LEN_4);
	g_core_fp.fp_register_write(tmp_addr, tmp_data, DATA_LEN_4);
	usleep_range(20000, 20001);
	g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
	I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);
	I("trigger Reset Pin\n");
	g_core_fp.fp_ic_reset(false, false);

	usleep_range(20000, 20001);
	do {
		himax_parse_assign_cmd(0x900000A8, tmp_addr, DATA_LEN_4);
		g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
		I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
			tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
			tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);
		rst_sts1 = tmp_read[0];
		cnt++;
		if (rst_sts1 == 0x05)
			break;
		if (rst_sts1 == 0x00)
			cnt += 5;
		if (cnt > 20)
			goto END_FUNC;
	} while (rst_sts1 == 0x04);

	himax_parse_assign_cmd(0x900000F0, tmp_addr, DATA_LEN_4);
	g_core_fp.fp_register_read(tmp_addr, tmp_read, DATA_LEN_4);
	I("R%02X%02X%02X%02XH = 0x%02X%02X%02X%02X\n",
		tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0],
		tmp_read[3], tmp_read[2], tmp_read[1], tmp_read[0]);
	rst_sts2 = tmp_read[0];

END_FUNC:
	if (rst_sts1 == 0x05 && rst_sts2 == 0x00)
		I("%s: TP Reset test OK!\n", __func__);
	else if (rst_sts1 == 0xFF || rst_sts2 == 0x01)
		I("%s: TP Reset test Fail!\n", __func__);
	else
		I("%s, Unknown Fail state1=0x%02X, state2=0x%02X!\n",
			__func__, rst_sts1, rst_sts2);

	g_core_fp.fp_sense_on(0x00);
	himax_int_enable(1);
}
#endif

static int himax_reset_read(struct seq_file *m)
{
	int ret = NO_ERR;

	if (debug_data->is_call_help) {
		seq_printf(m, HELP_RST);
		debug_data->is_call_help = false;
	} else {
		seq_printf(m, "diag value=%d\n", g_diag_arr_num);
	}
	return ret;
}
static ssize_t himax_reset_write(char *buf, size_t len)
{
	if (len >= 12) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	if (strnstr(buf, "help", len) != NULL) {
		debug_data->is_call_help = true;
	} else {
#if defined(HX_RST_PIN_FUNC)
		if (buf[0] == '1')
			g_core_fp.fp_ic_reset(false, false);
		else if (buf[0] == '2')
			g_core_fp.fp_ic_reset(false, true);
		/* else if (buf[0] == '5') */
		/*	ESD_HW_REST(); */
		else if (strnstr(buf, "test", len) != NULL)
			test_rst_pin();
#endif
#if defined(HX_ZERO_FLASH)
		if (g_core_fp.fp_0f_reload_to_active)
			g_core_fp.fp_0f_reload_to_active();
#endif
	}
	return len;
}



void hx_dump_prog_set(uint8_t prog)
{
	g_flash_progress = prog;
	if (prog == ONGOING)
		debug_data->flash_dump_going = ONGOING;
	else
		debug_data->flash_dump_going = START;
}

static int himax_flash_dump_show(struct seq_file *s, void *v)
{
	ssize_t ret = 0;
	int i;
	uint8_t flash_progress = g_flash_progress;
	// uint8_t flash_cmd = g_dump_show;
	bool flash_rst = g_flash_dump_rst;
	mm_segment_t fs;
	struct file *fn = NULL;
	//struct filename *vts_name = NULL;
	//loff_t pos = 0;

	I("dump_progress = %d\n", flash_progress);

	if (!flash_rst) {
		seq_puts(s, "DumpStart:Fail\n");
		seq_puts(s, "DumpEnd\n");
		goto END;
	}

	if (flash_progress == START)
		seq_puts(s, "Dump - Start\n");
	else if (flash_progress == ONGOING)
		seq_puts(s, "Dump - On-going\n");
	else if (flash_progress == FINISHED)
		seq_puts(s, "Dump - Finished\n");

	/*print flash dump data*/
	if (g_dump_show == 2 && flash_progress == FINISHED) {
		seq_puts(s, "Start to print dump data\n");
		for (i = 0; i < ic_data->flash_size; i++) {
			seq_printf(s, "0x%02X,", g_dump_buffer[i]);
			if (i % 16 == 15)
				seq_puts(s, "\n");
		}
	} else if (g_dump_show == 1) {
		I("Now Save to file!\n");
#if 0
		if (!kp_getname_kernel) {
			E("kp_getname_kernel is NULL, not open file!\n");
		} else {
			vts_name = kp_getname_kernel(g_file_path);
			I("Now vts_name=%s\n", vts_name->name);
			fn = kp_file_open_name(vts_name,
				O_TRUNC|O_CREAT|O_RDWR, 0660);

			if (IS_ERR(fn)) {
				E("%s Open file failed!\n", __func__);
				g_flash_dump_rst = false;
			} else {
				I("%s create file and ready to write\n",
					__func__);
				fs = get_fs();
				set_fs(KERNEL_DS);
				vfs_write(fn, g_dump_buffer,
				ic_data->flash_size * sizeof(uint8_t), &pos);
				I("Error: filpp_close(fn, NULL);\n");
				set_fs(fs);
			}
		}
#endif
		I("Error: fn = filpp_open(g_file_path,O_TRUNC|O_CREAT|O_RDWR, 0660);\n");
		if (IS_ERR(fn)) {
			E("%s Open file failed!\n", __func__);
			g_flash_dump_rst = false;
		} else {
			I("%s create file and ready to write\n",
				__func__);
			fs = get_fs();
			set_fs(KERNEL_DS);
			I("Error: kernell_write(fn, g_dump_buffer,ic_data->flash_size * sizeof(uint8_t), &pos);  kernell_write(fn, NULL); has been remove\n");
			set_fs(fs);
		}
	}

	seq_puts(s, "DumpEnd\n");
END:
	return ret;
}
#define FDS_DBG1 "%s, input cmd:is_flash=%d, is_file=%d, dump_size=%d, addr=0x%08X\n"
#define FDS_DBG2 "%s, assigned:is_flash=%d, is_file=%d, dump_size=%d, addr=0x%08X\n"
static ssize_t himax_flash_dump_store(struct file *filp,
			const char __user *buff, size_t len, loff_t *data)
{
	char buf[80] = {0};
	char *input_str[4];
	int i = 0;
	int is_flash = -1;
	int is_file = -1;
	int dump_size = -1;
	uint32_t addr = 0x000000;

	g_dump_cmd = 0;
	g_dump_size = 0;
	g_dump_show = 0;
	g_dump_addr = 0;

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	I("%s: buf = %s\n", __func__, buf);

	if (g_flash_progress == ONGOING) {
		E("%s: process is busy , return!\n", __func__);
		return len;
	}

	buf[len - 1] = '\0';

	for (i = 0; i < 4; i++)
		input_str[i] = kzalloc(sizeof(char) * 128, GFP_KERNEL);

	_str_to_arr_in_char(input_str, 4, buf, ',');

	for (i = 0; i < 4; i++) {
		if (input_str[i] == NULL) {
			continue;
		} else {
			I("%d:%s\n", i, input_str[i]);
			switch (i) {
			case 0:
				if (strnstr(input_str[i], "help",
					len) != NULL) {
					is_flash = 0;
					goto CON_WORK;
				} else if (strnstr(input_str[i], "flash",
					len) != NULL) {
					is_flash = 1;
				} else if (strnstr(input_str[i], "ram",
					len) != NULL) {
					is_flash = 2;
				} else {
					is_flash = -1;
					goto CON_WORK;
				}
				break;
			case 1:
				if (strnstr(input_str[i], "print",
					len) != NULL) {
					is_file = 2;
				} else if (strnstr(input_str[i], "file",
					len) != NULL) {
					is_file = 1;
				} else {
					is_file = -1;
					goto CON_WORK;
				}
				break;
			case 2:
				dump_size = hiamx_parse_str2int(
					input_str[i]);
				break;
			case 3:
				if (input_str[i] == NULL)
					addr = 0x00;
				else
					addr = hx_parse_hexstr2int(
						input_str[i]);
				I("addr=%d\n", addr);
				break;
			default:
				break;
			}
		}
	}
CON_WORK:
	I(FDS_DBG1,
		__func__, is_flash, is_file, dump_size, addr);
/*
 *
 * if ((buf[1] == '_') && (buf[2] == '3') && (buf[3] == '2'))
 *	g_dump_size = FW_SIZE_32k;
 * else if ((buf[1] == '_') && (buf[2] == '6')) {
 *	if (buf[3] == '0')
 *		g_dump_size = FW_SIZE_60k;
 *	else if (buf[3] == '4')
 *		g_dump_size = FW_SIZE_64k;

 *} else if ((buf[1] == '_') && (buf[2] == '2')) {
 *	if (buf[3] == '4')
 *		g_dump_size = FW_SIZE_124k;
 *	else if (buf[3] == '8')
 *		g_dump_size = FW_SIZE_128k;
 *}
 *
 * //1 : print flash to window, 2 : dump to sdcard
 *if (buf[0] == '1') {
 *	 //1_32,1_60,1_64,1_24,1_28 for flash size:
 *	 // 32k,60k,64k,124k,128k
 *
 *	g_dump_cmd = 1;
 *	hx_dump_prog_set(START);
 *	g_flash_dump_rst = true;
 *	queue_work(private_ts->dump_wq, &private_ts->dump_work);
 *} else if (buf[0] == '2') {
 *	 // 2_32,2_60,2_64,2_24,2_28 for dump size:
 *	 // 32k,60k,64k,124k,128k
 *
 *	g_dump_cmd = 2;
 *	hx_dump_prog_set(START);
 *	g_flash_dump_rst = true;
 *	queue_work(private_ts->dump_wq, &private_ts->dump_work);
 *}
 **/

	g_dump_cmd = is_flash;
	g_dump_size = dump_size;
	g_dump_show = is_file;
	g_dump_addr = addr;
	I(FDS_DBG2,
		__func__, g_dump_cmd, g_dump_show, g_dump_size, g_dump_addr);
	hx_dump_prog_set(START);
	g_flash_dump_rst = true;
	queue_work(private_ts->dump_wq, &private_ts->dump_work);

	for (i = 0; i < 4; i++)
		kfree(input_str[i]);
	return len;
}

static int himax_flash_dump_open(struct inode *inode, struct file *file)
{return single_open(file, himax_flash_dump_show, NULL); } 
static const struct proc_ops himax_flash_dump_ops = {
	.proc_open = himax_flash_dump_open,
	.proc_write  = himax_flash_dump_store,
	.proc_read  = seq_read,
	.proc_release = single_release,
};
//__CREATE_RW_NODE_HX(flash_dump);

void himax_ts_dump_func(void)
{
	uint8_t tmp_addr[DATA_LEN_4] = {0};
	int ret = NO_ERR;

	hx_dump_prog_set(ONGOING);

	/*msleep(100);*/
	// I("%s: flash_command = %d enter.\n", __func__, flash_command);
	I("%s: entering, is_flash=%d, is_file=%d, size=%d, add=0x%08X\n",
		__func__, g_dump_cmd, g_dump_show, g_dump_size, g_dump_addr);

	if (g_dump_cmd == 1) {
		himax_int_enable(0);
		g_core_fp.fp_flash_dump_func(g_dump_cmd, g_dump_size,
		  g_dump_buffer);
		g_flash_dump_rst = true;
		himax_int_enable(1);
	} else if (g_dump_cmd == 2) {
		I("dump ram start\n");
		if (g_dump_addr != -948794) {
			tmp_addr[0] = g_dump_addr % 0x100;
			tmp_addr[1] = (g_dump_addr >> 8) % 0x100;
			tmp_addr[2] = (g_dump_addr >> 16) % 0x100;
			tmp_addr[3] = g_dump_addr / 0x1000000;
			ret = g_core_fp.fp_register_read(tmp_addr,
				g_dump_buffer, g_dump_size);
			g_flash_dump_rst = true;
		} else {
			E("addr is wrong!\n");
			g_flash_dump_rst = false;
			goto END;
		}
		if (ret) {
			E("read ram fail, please check cmd !\n");
			g_flash_dump_rst = false;
			goto END;
		}
	}

	I("Dump Complete\n");


END:
	hx_dump_prog_set(FINISHED);
}

#if defined(HX_TP_PROC_GUEST_INFO)
static int printMat(struct seq_file *m, int max_size,
		 uint8_t *guest_str, int loc)
{
	int ret = loc;
	int i;

	for (i = 0; i < max_size; i++) {
		if ((i % 16) == 0 && i > 0)
			seq_puts(m, "\n");

		seq_printf(m, "0x%02X\t",
			guest_str[i]);
	}
	return ret;
}

static int printUnit(struct seq_file *m, int max_size,
		char *info_item, uint8_t *guest_str, int loc)
{
	int ret = loc;

	seq_printf(m, "%s:\n", info_item);
	ret = printMat(m, max_size, guest_str, ret);
	seq_puts(m, "\n");
	return ret;
}

static int himax_proc_guest_info_read(struct seq_file *m)
{
	int ret = 0;
	int j = 0;
	int max_size = 128;
	struct hx_guest_info *info = g_guest_info_data;


	if (debug_data->is_call_help) {
		seq_printf(m, HELP_GUEST_INFO);
		debug_data->is_call_help = false;
	} else {
		I("guest info progress\n");

		if (g_core_fp.guest_info_get_status()) {
			seq_printf(m,
				"Not Ready\n");
			goto END_FUNCTION;
		} else {
			if (info->g_guest_info_type == 1) {
				for (j = 0; j < 3; j++) {
					ret = printUnit(m, max_size,
						g_guest_info_item[j],
						info->g_guest_str[j], ret);
					I("str[%d] %s\n", j,
						info->g_guest_str[j]);
				}
				ret = printUnit(m, max_size,
						g_guest_info_item[8],
						info->g_guest_str[8],
						ret);

				I("str[8] %s\n",
				info->g_guest_str[8]);

				ret = printUnit(m, max_size,
						g_guest_info_item[9],
						info->g_guest_str[9],
						ret);

				I("str[9] %s\n", info->g_guest_str[9]);
			} else if (info->g_guest_info_type == 0) {
				for (j = 0; j < 10; j++) {
					if (j == 3)
						j = 8;

					seq_printf(m, "%s:\n",
						g_guest_info_item[j]);

					if (info->g_guest_data_type[j] == 0) {
						seq_printf(m, "%s",
						info->g_guest_str_in_format[j]);
					} else {
						ret = printMat(m,
						info->g_guest_data_len[j],
						info->g_guest_str_in_format[j],
						ret);
					}
					seq_puts(m, "\n");
				}
			}
		}
	}
END_FUNCTION:
	return ret;
}

static ssize_t himax_proc_guest_info_write(char *buf, size_t len)
{
	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	I("%s: buf = %s\n", __func__, buf);

	if (strnstr(buf, "help", len) != NULL) {
		debug_data->is_call_help = true;
	} else {
		if (buf[0] == 'r') {
			I("%s,Test to get", __func__);
			queue_work(private_ts->guest_info_wq,
					&private_ts->guest_info_work);
		}
	}
	return len;
}

#endif

static int himax_test_bus_read(struct seq_file *m)
{
	int ret = NO_ERR;

	if (g_core_fp.fp_read_i2c_status())
		seq_printf(m,
			"Bus communication is bad.\n");
	else
		seq_printf(m,
			"Bus communication is good.\n");
	return ret;
}

static int himax_info_read(struct seq_file *m)
{
	int ret = NO_ERR;

	seq_printf(m,
		"Himax Touch IC Information :\n");
	seq_printf(m,
		"%s\n", private_ts->chip_name);

	switch (IC_CHECKSUM) {
	case HX_TP_BIN_CHECKSUM_SW:
		seq_printf(m,
			"IC Checksum : SW\n");
		break;

	case HX_TP_BIN_CHECKSUM_HW:
		seq_printf(m,
			"IC Checksum : HW\n");
		break;

	case HX_TP_BIN_CHECKSUM_CRC:
		seq_printf(m,
			"IC Checksum : CRC\n");
		break;

	default:
		seq_printf(m,
			"IC Checksum error.\n");
	}

	if (ic_data->HX_INT_IS_EDGE)
		seq_printf(m,
			"Driver register Interrupt : EDGE TIRGGER\n");
	else
		seq_printf(m,
			"Driver register Interrupt : LEVEL TRIGGER\n");

	if (private_ts->protocol_type == PROTOCOL_TYPE_A)
		seq_printf(m,
			"Protocol : TYPE_A\n");
	else
		seq_printf(m,
			"Protocol : TYPE_B\n");

	seq_printf(m,
		"RX Num : %d\n", ic_data->HX_RX_NUM);
	seq_printf(m,
		"TX Num : %d\n", ic_data->HX_TX_NUM);
	seq_printf(m,
		"BT Num : %d\n", ic_data->HX_BT_NUM);
	seq_printf(m,
		"X Resolution : %d\n", ic_data->HX_X_RES);
	seq_printf(m,
		"Y Resolution : %d\n", ic_data->HX_Y_RES);
	seq_printf(m,
		"Max Point : %d\n", ic_data->HX_MAX_PT);
#if defined(HX_TP_PROC_2T2R)
	if (Is_2T2R) {
		seq_printf(m,
			"2T2R panel\n");
		seq_printf(m,
			"RX Num_2 : %d\n", HX_RX_NUM_2);
		seq_printf(m,
			"TX Num_2 : %d\n", HX_TX_NUM_2);
	}
#endif

	return ret;
}

static int himax_pen_info_read(struct seq_file *m)
{
	int ret = NO_ERR;

	seq_printf(m,
		"Pen Battery info:\n");
	seq_printf(m,
		"%d\n", g_target_report_data->s[0].battery_info);

	seq_printf(m,
		"Pen ID:\n");
	seq_printf(m,
		"%llx\n", g_target_report_data->s[0].id);

	return ret;
}

static int hx_node_update(char *buf, size_t is_len)
{
	int result;
	char fileName[128];
	int fw_type = 0;

	const struct firmware *fw = NULL;
	char mapa_name[HIMAX_FIRMWARE_LINE] = { 0 };

	fw_update_complete = false;
	fw_update_going = true;
	memset(fileName, 0, 128);
	/* parse the file name */
	if (!is_len)
		memcpy(fileName, buf, strlen(buf) * sizeof(char));
	else
		snprintf(fileName, is_len - 2, "%s", &buf[2]);

	I("%s: upgrade from file(%s) start!\n", __func__, fileName);
	/* manual upgrade will not use embedded firmware */
	result = request_firmware(&fw, fileName, private_ts->dev);
	if (result < 0) {
		E("request FW %s failed(%d)\n", fileName, result);
		return result;
	}

	I("%s: FW image: %02X, %02X, %02X, %02X\n", __func__,
			fw->data[0], fw->data[1],
			fw->data[2], fw->data[3]);

	himax_int_enable(0);

#if defined(HX_ZERO_FLASH)
	I("NOW Running Zero flash update!\n");

	snprintf(mapa_name, sizeof(mapa_name), "%s%s.bin",
		 MPAP_FWNAME, private_ts->panel_name);

	/* FW type: 0, normal; 1, MPFW */
	if (strcmp(fileName, (const char *)mapa_name) == 0)
		fw_type = 1;

	CFG_TABLE_FLASH_ADDR = CFG_TABLE_FLASH_ADDR_T;
	g_core_fp.fp_bin_desc_get((unsigned char *)fw->data, HX1K);

	result = g_core_fp.fp_firmware_update_0f(fw, fw_type);
	if (result) {
		fw_update_complete = false;
		I("Zero flash update fail!\n");
	} else {
		fw_update_complete = true;
		I("Zero flash update complete!\n");
	}
#else
	I("NOW Running common flow update!\n");

	fw_type = (fw->size) / 1024;
	I("Now FW size is : %dk\n", fw_type);

	switch (fw_type) {
	case 32:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k(
		(unsigned char *)fw->data, fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 60:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k(
		(unsigned char *)fw->data, fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 64:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k(
		(unsigned char *)fw->data, fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 124:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k(
		  (unsigned char *)fw->data, fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 128:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k(
		  (unsigned char *)fw->data, fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 255:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_255k(
		  (unsigned char *)fw->data, fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n",
					__func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	default:
		E("%s: Flash command fail: %d\n", __func__, __LINE__);
		fw_update_complete = false;
		break;
	}
#endif
	release_firmware(fw);
	goto firmware_upgrade_done;
firmware_upgrade_done:
	fw_update_going = false;
	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_power_on_init();
	g_core_fp.fp_read_FW_ver();

	g_core_fp.fp_tp_info_check();

	himax_int_enable(1);
	return result;
}

static int himax_update_read(struct seq_file *m)
{
	int ret = NO_ERR;

	if (debug_data->is_call_help) {
		seq_printf(m, HELP_UPDATE);
		debug_data->is_call_help = false;
	} else {
		if (!fw_update_going) {
			if (fw_update_complete)
				seq_printf(m,
					"FW Update Complete\n");
			else
				seq_printf(m,
					"FW Update Fail\n");
		} else {
			seq_printf(m,
					"FW Update Ongoing\n");
		}
	}

	return ret;
}
static ssize_t himax_update_write(char *buf, size_t len)
{

	I("Now cmd=%s", buf);
	if (strnstr(buf, "help", len) != NULL)
		debug_data->is_call_help = true;
	else
		hx_node_update(buf, 0);
	return len;
}

static int himax_version_read(struct seq_file *m)
{
	int ret = NO_ERR;

	if (debug_data->is_call_help) {
		seq_printf(m, HELP_VER);
		debug_data->is_call_help = false;
	} else {
		seq_printf(m,
					"FW_VER = 0x%2.2X\n",
					ic_data->vendor_fw_ver);
		if (private_ts->chip_cell_type == CHIP_IS_ON_CELL)
			seq_printf(m,
				"CONFIG_VER = 0x%2.2X\n",
				ic_data->vendor_config_ver);
		else {
			seq_printf(m,
				"TOUCH_VER = 0x%2.2X\n",
				ic_data->vendor_touch_cfg_ver);
			seq_printf(m,
				"DISPLAY_VER = 0x%2.2X\n",
				ic_data->vendor_display_cfg_ver);
		}
		if (ic_data->vendor_cid_maj_ver < 0
		&& ic_data->vendor_cid_min_ver < 0)
			seq_printf(m,
				"CID_VER = NULL\n");
		else
			seq_printf(m,
				"CID_VER = 0x%2.2X\n",
				(ic_data->vendor_cid_maj_ver << 8 |
				ic_data->vendor_cid_min_ver));

		if (ic_data->vendor_panel_ver < 0)
			seq_printf(m,
				"PANEL_VER = NULL\n");
		else
			seq_printf(m,
				"PANEL_VER = 0x%2.2X\n",
				ic_data->vendor_panel_ver);
		if (private_ts->chip_cell_type == CHIP_IS_IN_CELL) {
			seq_printf(m,
				"Cusomer = %s\n",
				ic_data->vendor_cus_info);

			seq_printf(m,
				"Project = %s\n",
				ic_data->vendor_proj_info);
		}
		seq_puts(m, "\n");
		seq_printf(m,
			"Himax Touch Driver Version:\n");
		seq_printf(m,
			"%s\n", HIMAX_DRIVER_VER);
	}
	return ret;
}
static ssize_t himax_version_write(char *buf, size_t len)
{
	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	I("Now cmd=%s", buf);
	if (strnstr(buf, "help", len) != NULL)
		debug_data->is_call_help = true;
	else
		g_core_fp.fp_read_FW_ver();

	return len;
}

static int himax_list_cmd_read(struct seq_file *m)
{
	int ret = NO_ERR;
	int i = 0, j = 0;

	for (i = CMD_START_IDX; dbg_cmd_str[i] != NULL; i++) {
		for (j = 0; dbg_cmd_str[i][j] != NULL ; j++) {
			if (j != 0)
				seq_puts(m, ", ");
			seq_printf(m, dbg_cmd_str[i][j]);
		}
		seq_puts(m, "\n");
	}
	return ret;
}

static int himax_help_read(struct seq_file *m)
{
	int ret = NO_ERR;

	seq_printf(m, HELP_ALL_DEBUG);
	return ret;
}
static ssize_t himax_help_write(char *buf, size_t len)
{
	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	I("Now cmd=%s", buf);

	return len;
}

static int himax_debug_show(struct seq_file *m, void *v)
{
	int ret = 0;

	if (dbg_cmd_flag) {
		if (dbg_func_ptr_r[dbg_cmd_flag]) {
			dbg_func_ptr_r[dbg_cmd_flag](m);
			goto END_FUNC_R;
		}
	}

	if (debug_level_cmd == 't') {
		if (!fw_update_going) {
			if (fw_update_complete)
				seq_printf(m,
					"FW Update Complete ");
			else
				seq_printf(m,
					"FW Update Fail ");
		} else {
			seq_printf(m,
					"FW Update Ongoing ");
		}
	} else if (debug_level_cmd == 'h') {
		if (handshaking_result == 0)
			seq_printf(m,
				"Handshaking Result = %d (MCU Running)\n",
				handshaking_result);
		else if (handshaking_result == 1)
			seq_printf(m,
				"Handshaking Result = %d (MCU Stop)\n",
				handshaking_result);
		else if (handshaking_result == 2)
			seq_printf(m,
				"Handshaking Result = %d (I2C Error)\n",
				handshaking_result);
		else
			seq_printf(m,
				"Handshaking Result = error\n");

	} else {
		himax_list_cmd_read(m);
	}

END_FUNC_R:

	return ret;
}

static ssize_t himax_debug_store(struct file *file, const char __user *ubuf,
		size_t len, loff_t *data)
{

	char buf[80] = "\0";
	char *str_ptr = NULL;
	int str_len = 0;
	int input_cmd_len = 0;
	int i = 0, j = 0;

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;

	str_len = len;
	buf[str_len - 1] = '\0';/*remove \n*/
	input_cmd_len = (int)strcspn(buf, ",");
	if (input_cmd_len == -1)
		input_cmd_len = str_len;

	i = CMD_START_IDX; /* start from 1*/
	while (dbg_cmd_str[i] != NULL) {
		while (dbg_cmd_str[i][j] != NULL) {
			str_ptr = strnstr(buf, dbg_cmd_str[i][j], len);
			str_len = strlen(dbg_cmd_str[i][j]);
			if (str_len != input_cmd_len) {
				str_ptr = NULL;
				j++;
				continue;
			}
			if (str_ptr) {
				dbg_cmd_flag = i;
				debug_level_cmd = 0;
				I("Cmd is correct :%s, dbg_cmd = %d\n",
						str_ptr, dbg_cmd_flag);
				goto CHECK;
			}
			j++;
		}
		j = 0;
		i++;
	}
CHECK:
	if (!str_ptr) {
		I("Cmd is in old fromat or incorrect\n");
		dbg_cmd_flag = 0;
		goto CONTI;
	}

	if (buf[str_len] == ',') {
		dbg_cmd_par = buf + str_len + 1;
		if (dbg_func_ptr_w[dbg_cmd_flag])
			/* 2 => '/n' + ','*/
			dbg_func_ptr_w[dbg_cmd_flag](dbg_cmd_par,
					len - str_len - 2);

		I("string of paremeter is %s; dbg_cmd_par = %s, size = %d\n",
				buf + str_len + 1,
				dbg_cmd_par, (int)(len - str_len - 2));
	} else {
		I("Write cmd=%s, without parameter\n",
			dbg_cmd_str[dbg_cmd_flag][0]);
	}
CONTI:
	if (dbg_cmd_flag)
		return len;

	if (buf[0] == 't') {
		debug_level_cmd = buf[0];
		hx_node_update(buf, len);
	}  else if (buf[0] == 'c' && buf[1] == 't' && buf[2] == 'i') {
		/* Compare Touch Information */
		g_core_fp.fp_tp_info_check();
		goto ENDFUCTION;
	} else {
		/* others,do nothing */
		debug_level_cmd = 0;
	}

ENDFUCTION:
	return len;
}

static int himax_debug_open(struct inode *inode, struct file *file)
{return single_open(file, himax_debug_show, NULL); } 
static const struct proc_ops himax_debug_ops = {
	.proc_open = himax_debug_open,
	.proc_write  = himax_debug_store,
	.proc_read  = seq_read,
	.proc_release = single_release,
};
//__CREATE_RW_NODE_HX(debug);

static int himax_vendor_show(struct seq_file *m, void *v)
{
	int ret = 0;

	seq_printf(m,
			"IC = %s\n", private_ts->chip_name);

	seq_printf(m,
			"FW_VER = 0x%2.2X\n", ic_data->vendor_fw_ver);

	if (private_ts->chip_cell_type == CHIP_IS_ON_CELL) {
		seq_printf(m,
				"CONFIG_VER = 0x%2.2X\n",
				ic_data->vendor_config_ver);
	} else {
		seq_printf(m,
				"TOUCH_VER = 0x%2.2X\n",
				ic_data->vendor_touch_cfg_ver);
		seq_printf(m,
				"DISPLAY_VER = 0x%2.2X\n",
				ic_data->vendor_display_cfg_ver);
	}

	if (ic_data->vendor_cid_maj_ver < 0
	&& ic_data->vendor_cid_min_ver < 0) {
		seq_printf(m,
				"CID_VER = NULL\n");
	} else {
		seq_printf(m,
				"CID_VER = 0x%2.2X\n",
				(ic_data->vendor_cid_maj_ver << 8 |
				ic_data->vendor_cid_min_ver));
	}

	if (ic_data->vendor_panel_ver < 0) {
		seq_printf(m,
				"PANEL_VER = NULL\n");
	} else {
		seq_printf(m,
				"PANEL_VER = 0x%2.2X\n",
				ic_data->vendor_panel_ver);
	}
	if (private_ts->chip_cell_type == CHIP_IS_IN_CELL) {
		seq_printf(m,
				"Cusomer = %s\n",
				ic_data->vendor_cus_info);
		seq_printf(m,
				"Project = %s\n",
				ic_data->vendor_proj_info);
	}
	seq_puts(m, "\n");
	seq_printf(m,
			"Himax Touch Driver Version:\n");
	seq_printf(m, "%s\n",
			HIMAX_DRIVER_VER);

	return ret;
}
static int himax_vendor_open(struct inode *inode, struct file *file)
{return single_open(file, himax_vendor_show, NULL); } 
static const struct proc_ops himax_vendor_ops = {
	.proc_open = himax_vendor_open,
	.proc_read = seq_read,
};

//__CREATE_OREAD_NODE_HX(vendor);

static void himax_himax_data_init(void)
{
	debug_data->fp_ts_dbg_func = himax_ts_dbg_func;
	debug_data->fp_set_diag_cmd = himax_set_diag_cmd;
	debug_data->flash_dump_going = false;
	debug_data->is_checking_irq = false;
	debug_data->is_call_help = false;
}

static void himax_ts_dump_work_func(struct work_struct *work)
{
	himax_ts_dump_func();
}
#if defined(HX_TP_PROC_GUEST_INFO)
static void himax_ts_guest_info_work_func(struct work_struct *work)
{
	g_core_fp.read_guest_info();
}
#endif

static void himax_ts_diag_work_func(struct work_struct *work)
{
	himax_ts_diag_func(process_type);

	if (process_type != 0) {
		queue_delayed_work(private_ts->himax_diag_wq,
			&private_ts->himax_diag_delay_work, 1 / 10 * HZ);
	} else {
		dsram_flag = false;
	}
}

void dbg_func_ptr_init(void)
{
	/*debug function ptr init*/
	int idx = 1;

	dbg_func_ptr_r[idx] = himax_crc_test_read;
	idx++;

	dbg_func_ptr_r[idx] = himax_proc_FW_debug_read;
	idx++;

	dbg_func_ptr_r[idx] = himax_attn_read;
	idx++;

	dbg_func_ptr_w[idx] = himax_layout_write;
	dbg_func_ptr_r[idx] = himax_layout_read;
	idx++;

#if defined(HX_EXCP_RECOVERY)
	dbg_func_ptr_w[idx] = himax_excp_cnt_write;
	dbg_func_ptr_r[idx] = himax_excp_cnt_read;
#endif
	idx++;

	dbg_func_ptr_w[idx] = himax_sense_on_off_write;
	idx++;

	dbg_func_ptr_w[idx] = himax_debug_level_write;
	dbg_func_ptr_r[idx] = himax_debug_level_read;
	idx++;

#if defined(HX_TP_PROC_GUEST_INFO)
	dbg_func_ptr_w[idx] = himax_proc_guest_info_write;
	dbg_func_ptr_r[idx] = himax_proc_guest_info_read;
#endif
	idx++;

	dbg_func_ptr_w[idx] = himax_int_en_write;
	dbg_func_ptr_r[idx] = himax_int_en_read;
	idx++;

	dbg_func_ptr_w[idx] = himax_irq_info_write;
	dbg_func_ptr_r[idx] = himax_irq_info_read;
	idx++;

	dbg_func_ptr_r[idx] = himax_proc_register_read;
	dbg_func_ptr_w[idx] = himax_proc_register_write;
	idx++;

	dbg_func_ptr_r[idx] = himax_reset_read;
	dbg_func_ptr_w[idx] = himax_reset_write;
	idx++;

	dbg_func_ptr_r[idx] = himax_diag_arrange_read;
	dbg_func_ptr_w[idx] = himax_diag_arrange_write;
	idx++;

	dbg_func_ptr_w[idx] = himax_diag_cmd_write;
	idx++;

	dbg_func_ptr_w[idx] = himax_irq_dbg_cmd_write;
	idx++;

	dbg_func_ptr_r[idx] = himax_test_bus_read;
	idx++;

	dbg_func_ptr_w[idx] = himax_update_write;
	dbg_func_ptr_r[idx] = himax_update_read;
	idx++;

	dbg_func_ptr_w[idx] = himax_version_write;
	dbg_func_ptr_r[idx] = himax_version_read;
	idx++;

	dbg_func_ptr_r[idx] = himax_info_read;
	idx++;

	dbg_func_ptr_r[idx] = himax_pen_info_read;
	idx++;

	dbg_func_ptr_r[idx] = himax_list_cmd_read;
	idx++;

	dbg_func_ptr_w[idx] = himax_help_write;
	dbg_func_ptr_r[idx] = himax_help_read;
	idx++;

}

int himax_touch_proc_init(void)
{
	himax_proc_diag_dir = proc_mkdir(HIMAX_PROC_DIAG_FOLDER,
			himax_touch_proc_dir);

	if (himax_proc_diag_dir == NULL) {
		E(" %s: himax_proc_diag_dir file create failed!\n", __func__);
		return -ENOMEM;
	}

	himax_proc_stack_file = proc_create(HIMAX_PROC_STACK_FILE, 0444,
	  himax_proc_diag_dir, &himax_stack_ops);
	if (himax_proc_stack_file == NULL) {
		E(" %s: proc stack file create failed!\n", __func__);
		goto fail_2_1;
	}

	himax_proc_delta_file = proc_create(HIMAX_PROC_DELTA_FILE, 0444,
	  himax_proc_diag_dir, &himax_delta_ops);
	if (himax_proc_delta_file == NULL) {
		E(" %s: proc delta file create failed!\n", __func__);
		goto fail_2_2;
	}

	himax_proc_dc_file = proc_create(HIMAX_PROC_DC_FILE, 0444,
	  himax_proc_diag_dir, &himax_dc_ops);
	if (himax_proc_dc_file == NULL) {
		E(" %s: proc dc file create failed!\n", __func__);
		goto fail_2_3;
	}

	himax_proc_baseline_file = proc_create(HIMAX_PROC_BASELINE_FILE, 0444,
	  himax_proc_diag_dir, &himax_baseline_ops);
	if (himax_proc_baseline_file == NULL) {
		E(" %s: proc baseline file create failed!\n", __func__);
		goto fail_2_4;
	}

	himax_proc_debug_file = proc_create(HIMAX_PROC_DEBUG_FILE,
				0644, himax_touch_proc_dir,
				&himax_debug_ops);
	if (himax_proc_debug_file == NULL) {
		E(" %s: proc debug file create failed!\n", __func__);
		goto fail_3;
	}
	dbg_func_ptr_init();

	himax_proc_flash_dump_file = proc_create(HIMAX_PROC_FLASH_DUMP_FILE,
				0644, himax_touch_proc_dir,
				&himax_flash_dump_ops);
	if (himax_proc_flash_dump_file == NULL) {
		E(" %s: proc flash dump file create failed!\n", __func__);
		goto fail_4;
	}

	return 0;

fail_4: remove_proc_entry(HIMAX_PROC_DEBUG_FILE, himax_touch_proc_dir);
fail_3:	remove_proc_entry(HIMAX_PROC_BASELINE_FILE, himax_proc_diag_dir);
fail_2_4: remove_proc_entry(HIMAX_PROC_DC_FILE, himax_proc_diag_dir);
fail_2_3: remove_proc_entry(HIMAX_PROC_DELTA_FILE, himax_proc_diag_dir);
fail_2_2: remove_proc_entry(HIMAX_PROC_STACK_FILE, himax_proc_diag_dir);
fail_2_1:
	return -ENOMEM;
}

void himax_touch_proc_deinit(void)
{

	remove_proc_entry(HIMAX_PROC_FLASH_DUMP_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_DEBUG_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_BASELINE_FILE, himax_proc_diag_dir);
	remove_proc_entry(HIMAX_PROC_DC_FILE, himax_proc_diag_dir);
	remove_proc_entry(HIMAX_PROC_DELTA_FILE, himax_proc_diag_dir);
	remove_proc_entry(HIMAX_PROC_STACK_FILE, himax_proc_diag_dir);
}

int himax_debug_init(void)
{
	struct himax_ts_data *ts = private_ts;

	I("%s:Enter\n", __func__);

	if (ts == NULL) {
		E("%s: ts struct is NULL\n", __func__);
		return -EPROBE_DEFER;
	}

	proc_reg_buf = kzalloc(128 * sizeof(uint8_t), GFP_KERNEL);
	if (proc_reg_buf == NULL) {
		E("%s: reg_read_data allocate failed\n", __func__);
		goto err_alloc_reg_read_data_fail;
	}

	debug_data = kzalloc(sizeof(struct himax_debug), GFP_KERNEL);
	if (debug_data == NULL) { /*Allocate debug data space*/
		E("%s: debug_data allocate failed\n", __func__);
		goto err_alloc_debug_data_fail;
	}

	himax_himax_data_init();

	g_dump_buffer = kcalloc(ic_data->flash_size,
		sizeof(uint8_t),
		GFP_KERNEL);
	if (g_dump_buffer == NULL) {
		E("%s: dump buffer allocate fail failed\n", __func__);
		goto err_dump_buf_alloc_failed;
	}

	ts->dump_wq = create_singlethread_workqueue("himax_dump_wq");
	if (!ts->dump_wq) {
		E("%s: create flash workqueue failed\n", __func__);
		goto err_create_flash_dump_wq_failed;
	}
	INIT_WORK(&ts->dump_work, himax_ts_dump_work_func);
	g_flash_progress = START;

#if defined(HX_TP_PROC_GUEST_INFO)
	if (g_guest_info_data == NULL) {
		g_guest_info_data = kzalloc(sizeof(struct hx_guest_info),
				GFP_KERNEL);
		if (g_guest_info_data == NULL) {
			E("%s: flash buffer allocate fail failed\n", __func__);
			goto err_guest_info_alloc_failed;
		}
		g_guest_info_data->g_guest_info_ongoing = 0;
		g_guest_info_data->g_guest_info_type = 0;
	}

	ts->guest_info_wq =
		create_singlethread_workqueue("himax_guest_info_wq");
	if (!ts->guest_info_wq) {
		E("%s: create guest info workqueue failed\n", __func__);
		goto err_create_guest_info_wq_failed;
	}
	INIT_WORK(&ts->guest_info_work, himax_ts_guest_info_work_func);
#endif

	setSelfBuffer(ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	if (getSelfBuffer() == NULL) {
		E("%s: self buffer allocate failed\n", __func__);
		goto err_self_buf_alloc_failed;
	}

	setSelfNewBuffer(ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	if (getSelfNewBuffer() == NULL) {
		E("%s: self new buffer allocate failed\n", __func__);
		goto err_self_new_alloc_failed;
	}

	setSelfOldBuffer(ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	if (getSelfOldBuffer() == NULL) {
		E("%s: self old buffer allocate failed\n", __func__);
		goto err_self_old_alloc_failed;
	}

	setMutualBuffer(ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	if (getMutualBuffer() == NULL) {
		E("%s: mutual buffer allocate failed\n", __func__);
		goto err_mut_buf_alloc_failed;
	}

	setMutualNewBuffer(ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	if (getMutualNewBuffer() == NULL) {
		E("%s: mutual new buffer allocate failed\n", __func__);
		goto err_mut_new_alloc_failed;
	}

	setMutualOldBuffer(ic_data->HX_RX_NUM, ic_data->HX_TX_NUM);
	if (getMutualOldBuffer() == NULL) {
		E("%s: mutual old buffer allocate failed\n", __func__);
		goto err_mut_old_alloc_failed;
	}

#if defined(HX_TP_PROC_2T2R)
	if (Is_2T2R) {
		setMutualBuffer_2(ic_data->HX_RX_NUM_2, ic_data->HX_TX_NUM_2);
		if (getMutualBuffer_2() == NULL) {
			E("%s: mutual buffer 2 allocate failed\n", __func__);
			goto err_mut_buf2_alloc_failed;
		}
	}
#endif

	ts->himax_diag_wq = create_singlethread_workqueue("himax_diag");
	if (!ts->himax_diag_wq) {
		E("%s: create diag workqueue failed\n", __func__);
		goto err_create_diag_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->himax_diag_delay_work, himax_ts_diag_work_func);

	if (himax_touch_proc_init())
		goto err_proc_init_failed;


	snprintf(g_file_path, (int)(strlen(HX_RSLT_OUT_PATH)
		+ strlen(HX_RSLT_OUT_FILE)+1),
		"%s%s", HX_RSLT_OUT_PATH, HX_RSLT_OUT_FILE);

	return 0;

err_proc_init_failed:
	cancel_delayed_work_sync(&ts->himax_diag_delay_work);
	destroy_workqueue(ts->himax_diag_wq);
err_create_diag_wq_failed:
#if defined(HX_TP_PROC_2T2R)
	kfree(diag_mutual_2);
	diag_mutual_2 = NULL;
err_mut_buf2_alloc_failed:
#endif
	kfree(diag_mutual_old);
	diag_mutual_old = NULL;
err_mut_old_alloc_failed:
	kfree(diag_mutual_new);
	diag_mutual_new = NULL;
err_mut_new_alloc_failed:
	kfree(diag_mutual);
	diag_mutual = NULL;
err_mut_buf_alloc_failed:
	kfree(diag_self_old);
	diag_self_old = NULL;
err_self_old_alloc_failed:
	kfree(diag_self_new);
	diag_self_new = NULL;
err_self_new_alloc_failed:
	kfree(diag_self);
	diag_self = NULL;
err_self_buf_alloc_failed:
#if defined(HX_TP_PROC_GUEST_INFO)
	cancel_work_sync(&ts->guest_info_work);
	destroy_workqueue(ts->guest_info_wq);
err_create_guest_info_wq_failed:
	if (g_guest_info_data != NULL) {
		kfree(g_guest_info_data);
		g_guest_info_data = NULL;
	}
err_guest_info_alloc_failed:
#endif
	cancel_work_sync(&ts->dump_work);
	destroy_workqueue(ts->dump_wq);
err_create_flash_dump_wq_failed:
	kfree(g_dump_buffer);
	g_dump_buffer = NULL;
err_dump_buf_alloc_failed:
	kfree(debug_data);
	debug_data = NULL;
err_alloc_debug_data_fail:
	kfree(proc_reg_buf);
	proc_reg_buf = NULL;
err_alloc_reg_read_data_fail:

	return -ENOMEM;
}
EXPORT_SYMBOL(himax_debug_init);

int himax_debug_remove(void)
{
	struct himax_ts_data *ts = private_ts;

	himax_touch_proc_deinit();

	cancel_delayed_work_sync(&ts->himax_diag_delay_work);
	destroy_workqueue(ts->himax_diag_wq);

#if defined(HX_TP_PROC_2T2R)
	kfree(diag_mutual_2);
	diag_mutual_2 = NULL;
#endif

	kfree(diag_mutual_old);
	diag_mutual_old = NULL;

	kfree(diag_mutual_new);
	diag_mutual_new = NULL;

	kfree(diag_mutual);
	diag_mutual = NULL;

	kfree(diag_self_old);
	diag_self_old = NULL;

	kfree(diag_self_new);
	diag_self_new = NULL;

	kfree(diag_self);
	diag_self = NULL;

#if defined(HX_TP_PROC_GUEST_INFO)
	cancel_work_sync(&ts->guest_info_work);
	destroy_workqueue(ts->guest_info_wq);
	if (g_guest_info_data != NULL) {
		kfree(g_guest_info_data);
		g_guest_info_data = NULL;
	}
#endif

	cancel_work_sync(&ts->dump_work);
	destroy_workqueue(ts->dump_wq);

	kfree(g_dump_buffer);
	g_dump_buffer = NULL;

	kfree(debug_data);
	debug_data = NULL;

	kfree(proc_reg_buf);
	proc_reg_buf = NULL;

	return 0;
}
EXPORT_SYMBOL(himax_debug_remove);


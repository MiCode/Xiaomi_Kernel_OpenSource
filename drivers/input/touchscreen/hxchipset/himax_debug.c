/*
 * Himax Android Driver Sample Code for debug nodes
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

#include "himax_debug.h"
#include "himax_ic_core.h"

static struct proc_dir_entry *himax_proc_debug_level_file;
static struct proc_dir_entry *himax_proc_vendor_file;
static struct proc_dir_entry *himax_proc_attn_file;
static struct proc_dir_entry *himax_proc_int_en_file;
static struct proc_dir_entry *himax_proc_layout_file;
static struct proc_dir_entry *himax_proc_CRC_test_file;

#ifdef HX_RST_PIN_FUNC
	extern void himax_ic_reset(uint8_t loadconfig, uint8_t int_off);
#endif

#ifdef HX_TP_PROC_2T2R
	bool Is_2T2R = false;
	int HX_RX_NUM_2	= 0;
	int HX_TX_NUM_2	= 0;
#endif
uint8_t g_diag_arr_num;

int g_max_mutual;
int g_min_mutual = 0xFFFF;
int g_max_self;
int g_min_self = 0xFFFF;

struct timespec timeStart, timeEnd, timeDelta;
int g_switch_mode;

#ifdef HX_TP_PROC_2T2R
static uint8_t x_channel_2;
static uint8_t y_channel_2;
static uint32_t *diag_mutual_2;

int32_t *getMutualBuffer_2(void);
#endif

#define HIMAX_PROC_REGISTER_FILE "register"
struct proc_dir_entry *himax_proc_register_file;
uint8_t byte_length;
uint8_t register_command[4];
uint8_t cfg_flag;

#define HIMAX_PROC_DIAG_FILE "diag"
struct proc_dir_entry *himax_proc_diag_file;
#define HIMAX_PROC_DIAG_ARR_FILE "diag_arr"
struct proc_dir_entry *himax_proc_diag_arrange_file;
struct file *diag_sram_fn;
uint8_t write_counter;
uint8_t write_max_count = 30;

#define IIR_DUMP_FILE "/sdcard/HX_IIR_Dump.txt"
#define DC_DUMP_FILE "/sdcard/HX_DC_Dump.txt"
#define BANK_DUMP_FILE "/sdcard/HX_BANK_Dump.txt"

uint8_t x_channel;
uint8_t y_channel;
int32_t *diag_mutual;
int32_t *diag_mutual_new;
int32_t *diag_mutual_old;
uint8_t diag_max_cnt;
uint8_t hx_state_info[2] = {0};
uint8_t diag_coor[128];
int32_t diag_self[100] = {0};
int32_t diag_self_new[100] = {0};
int32_t diag_self_old[100] = {0};
int32_t *getMutualBuffer(void);
int32_t *getMutualNewBuffer(void);
int32_t *getMutualOldBuffer(void);
int32_t *getSelfBuffer(void);
int32_t *getSelfNewBuffer(void);
int32_t *getSelfOldBuffer(void);

#define HIMAX_PROC_DEBUG_FILE "debug"
struct proc_dir_entry *himax_proc_debug_file;
#define HIMAX_PROC_FW_DEBUG_FILE "FW_debug"
struct proc_dir_entry *himax_proc_fw_debug_file;
#define HIMAX_PROC_DD_DEBUG_FILE "DD_debug"
struct proc_dir_entry *himax_proc_dd_debug_file;
bool fw_update_complete;
int handshaking_result;
unsigned char debug_level_cmd;
uint8_t cmd_set[8];
uint8_t mutual_set_flag;

#define HIMAX_PROC_FLASH_DUMP_FILE "flash_dump"
struct proc_dir_entry *himax_proc_flash_dump_file;
static int Flash_Size = 131072;
static uint8_t *flash_buffer;
static uint8_t flash_command;
static uint8_t flash_read_step;
static uint8_t flash_progress;
static uint8_t flash_dump_complete;
static uint8_t flash_dump_fail;
static uint8_t sys_operation;
static bool flash_dump_going;
static uint8_t getFlashDumpComplete(void);
static uint8_t getFlashDumpFail(void);
static uint8_t getFlashDumpProgress(void);
static uint8_t getFlashReadStep(void);
static void setFlashCommand(uint8_t command);
static void setFlashReadStep(uint8_t step);

uint32_t **raw_data_array;
uint8_t X_NUM, Y_NUM;
uint8_t sel_type = 0x0D;

#define HIMAX_PROC_RESET_FILE "reset"
struct proc_dir_entry *himax_proc_reset_file;

#define HIMAX_PROC_SENSE_ON_OFF_FILE "SenseOnOff"
struct proc_dir_entry *himax_proc_SENSE_ON_OFF_file;

#ifdef HX_ESD_RECOVERY
	#define HIMAX_PROC_ESD_CNT_FILE "ESD_cnt"
	struct proc_dir_entry *himax_proc_ESD_cnt_file;
#endif

#define COMMON_BUF_SZ 80
#define PROC_DD_BUF_SZ 20
#define DEBUG_BUF_SZ 12

/* raw type */
#define RAW_IIR    1
#define RAW_DC     2
#define RAW_BANK   3
#define RAW_IIR2   4
#define RAW_IIR2_N 5
#define RAW_FIR2   6
#define RAW_BASELINE 7
#define RAW_DUMP_COORD 8

/* status type */
#define START_TEST   0
#define RAW_DATA     1
#define PERCENT_TEST 2
#define DEV_TEST     3
#define NOISE_TEST   4

#define END_TEST     9

/*
 *=========================================================================
 *
 *      Segment : Himax PROC Debug Function
 *
 *=========================================================================
 */
#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)

static int himax_read_i2c_status(void)
{
	return i2c_error_count;
}

static ssize_t himax_ito_test_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	uint8_t result = 0;
	uint8_t status = 0;
	char *temp_buf;

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return ret;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	status = ito_get_step_status();

	switch (status) {
	case START_TEST:
		ret += snprintf(temp_buf + ret, len - ret, "Step : START_TEST\n");
		break;

	case RAW_DATA:
		ret += snprintf(temp_buf + ret, len - ret, "Step : RAW_DATA\n");
		break;

	case PERCENT_TEST:
		ret += snprintf(temp_buf + ret, len - ret, "Step : PERCENT_TEST\n");
		break;

	case DEV_TEST:
		ret += snprintf(temp_buf + ret, len - ret, "Step : DEV_TEST\n");
		break;

	case NOISE_TEST:
		ret += snprintf(temp_buf + ret, len - ret, "Step : NOISE_TEST\n");
		break;

	case END_TEST:
		ret += snprintf(temp_buf + ret, len - ret, "Step : END_TEST\n");
		break;

	default:
		ret += snprintf(temp_buf + ret, len - ret, "Step : Null\n");
	}

	result = ito_get_result_status();

	if (result == 0xF)
		ret += snprintf(temp_buf + ret, len - ret, "ITO test is On-going!\n");
	else if (result == 0)
		ret += snprintf(temp_buf + ret, len - ret, "ITO test is Pass!\n");
	else if (result == 2)
		ret += snprintf(temp_buf + ret, len - ret, "Open config file fail!\n");
	else
		ret += snprintf(temp_buf + ret, len - ret, "ITO test is Fail!\n");

	HX_PROC_SEND_FLAG = 1;

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	return ret;
}

static ssize_t himax_ito_test_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	uint8_t result = 0;
	char buf[COMMON_BUF_SZ] = {0};

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	result = ito_get_result_status();
	I("%s: buf = %s, result = %d.\n", __func__, buf, result);

	if (buf[0] == '1' && result != 0xF) {
		I("%s: buf[0] = %c.\n", __func__, buf[0]);
		ito_set_step_status(0);
		queue_work(ts->ito_test_wq, &ts->ito_test_work);
	}

	return len;
}

static const struct file_operations himax_proc_ito_test_ops = {
	.owner = THIS_MODULE,
	.read = himax_ito_test_read,
	.write = himax_ito_test_write,
};
#endif

static ssize_t himax_CRC_test_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	uint8_t result = 0;
	char *temp_buf;

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	g_core_fp.fp_sense_off();
	msleep(20);
	result = g_core_fp.fp_calculateChecksum(false);
	g_core_fp.fp_sense_on(0x01);

	if (result)
		ret += snprintf(temp_buf + ret, len - ret, "CRC test is Pass!\n");
	else
		ret += snprintf(temp_buf + ret, len - ret, "CRC test is Fail!\n");

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static const struct file_operations himax_proc_CRC_test_ops = {
	.owner = THIS_MODULE,
	.read = himax_CRC_test_read,
};

static ssize_t himax_vendor_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	char *temp_buf;

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	ret += snprintf(temp_buf + ret, len - ret, "FW_VER = 0x%2.2X\n", ic_data->vendor_fw_ver);

	if (private_ts->chip_cell_type == CHIP_IS_ON_CELL)
		ret += snprintf(temp_buf + ret, len - ret, "CONFIG_VER = 0x%2.2X\n", ic_data->vendor_config_ver);
	else {
		ret += snprintf(temp_buf + ret, len - ret, "TOUCH_VER = 0x%2.2X\n", ic_data->vendor_touch_cfg_ver);
		ret += snprintf(temp_buf + ret, len - ret, "DISPLAY_VER = 0x%2.2X\n", ic_data->vendor_display_cfg_ver);
	}

	if (ic_data->vendor_cid_maj_ver < 0 && ic_data->vendor_cid_min_ver < 0)
		ret += snprintf(temp_buf + ret, len - ret, "CID_VER = NULL\n");
	else
		ret += snprintf(temp_buf + ret, len - ret, "CID_VER = 0x%2.2X\n", (ic_data->vendor_cid_maj_ver << 8 | ic_data->vendor_cid_min_ver));

	if (ic_data->vendor_panel_ver < 0)
		ret += snprintf(temp_buf + ret, len - ret, "PANEL_VER = NULL\n");
	else
		ret += snprintf(temp_buf + ret, len - ret, "PANEL_VER = 0x%2.2X\n", ic_data->vendor_panel_ver);

	ret += snprintf(temp_buf + ret, len - ret, "\n");
	ret += snprintf(temp_buf + ret, len - ret, "Himax Touch Driver Version:\n");
	ret += snprintf(temp_buf + ret, len - ret, "%s\n", HIMAX_DRIVER_VER);
	HX_PROC_SEND_FLAG = 1;

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);

	return ret;
}

static const struct file_operations himax_proc_vendor_ops = {
	.owner = THIS_MODULE,
	.read = himax_vendor_read,
};

static ssize_t himax_attn_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	struct himax_ts_data *ts_data;
	char *temp_buf;

	ts_data = private_ts;

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	ret += snprintf(temp_buf + ret, len - ret, "attn = %x\n", himax_int_gpio_read(ts_data->pdata->gpio_irq));

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}


static const struct file_operations himax_proc_attn_ops = {
	.owner = THIS_MODULE,
	.read = himax_attn_read,
};

static ssize_t himax_int_en_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;
	char *temp_buf;

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	ret += snprintf(temp_buf + ret, len - ret, "%d ", ts->irq_enabled);
	ret += snprintf(temp_buf + ret, len - ret, "\n");

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static ssize_t himax_int_en_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf_tmp[DEBUG_BUF_SZ] = {0};
	int value, ret = 0;

	if (len >= DEBUG_BUF_SZ) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf_tmp, buff, len))
		return -EFAULT;

	if (buf_tmp[0] == '0')
		value = false;
	else if (buf_tmp[0] == '1')
		value = true;
	else
		return -EINVAL;

	if (value) {
		ret = himax_int_en_set();

		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
		}
	} else {
		himax_int_enable(0);
		free_irq(ts->client->irq, ts);
		ts->irq_enabled = 0;
	}

	return len;
}

static const struct file_operations himax_proc_int_en_ops = {
	.owner = THIS_MODULE,
	.read = himax_int_en_read,
	.write = himax_int_en_write,
};

static ssize_t himax_layout_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed\n", __func__);
		return 0;
	}
	ret += snprintf(temp_buf + ret, len - ret, "%d ", ts->pdata->abs_x_min);
	ret += snprintf(temp_buf + ret, len - ret, "%d ", ts->pdata->abs_x_max);
	ret += snprintf(temp_buf + ret, len - ret, "%d ", ts->pdata->abs_y_min);
	ret += snprintf(temp_buf + ret, len - ret, "%d ", ts->pdata->abs_y_max);
	ret += snprintf(temp_buf + ret, len - ret, "\n");

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static ssize_t himax_layout_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf_tmp[5];
	int i = 0, j = 0, k = 0, ret;
	unsigned long value;
	int layout[4] = {0};
	char buf[COMMON_BUF_SZ] = {0};

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	for (i = 0; i < 20; i++) {
		if (buf[i] == ',' || buf[i] == '\n') {
			memset(buf_tmp, 0x0, sizeof(buf_tmp));

			if (i - j <= 5)
				memcpy(buf_tmp, buf + j, i - j);
			else {
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
		ts->pdata->abs_x_max = layout[1];
		ts->pdata->abs_y_min = layout[2];
		ts->pdata->abs_y_max = layout[3];
		I("%d, %d, %d, %d\n",
		  ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
		input_unregister_device(ts->input_dev);
		himax_input_register(ts);
	} else
		I("ERR@%d, %d, %d, %d\n",
		  ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);

	return len;
}

static const struct file_operations himax_proc_layout_ops = {
	.owner = THIS_MODULE,
	.read = himax_layout_read,
	.write = himax_layout_write,
};

static ssize_t himax_debug_level_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts_data;
	size_t ret = 0;
	char *temp_buf;

	ts_data = private_ts;

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	ret += snprintf(temp_buf + ret, len - ret, "%d\n", ts_data->debug_log_level);

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static ssize_t himax_debug_level_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	struct himax_ts_data *ts;
	char buf_tmp[DEBUG_BUF_SZ];
	int i;

	ts = private_ts;

	if (len >= DEBUG_BUF_SZ) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf_tmp, buff, len))
		return -EFAULT;

	ts->debug_log_level = 0;

	for (i = 0; i < len - 1; i++) {
		if (buf_tmp[i] >= '0' && buf_tmp[i] <= '9')
			ts->debug_log_level |= (buf_tmp[i] - '0');
		else if (buf_tmp[i] >= 'A' && buf_tmp[i] <= 'F')
			ts->debug_log_level |= (buf_tmp[i] - 'A' + 10);
		else if (buf_tmp[i] >= 'a' && buf_tmp[i] <= 'f')
			ts->debug_log_level |= (buf_tmp[i] - 'a' + 10);

		if (i != len - 2)
			ts->debug_log_level <<= 4;
	}

	if (ts->debug_log_level & BIT(3)) {
		if (ts->pdata->screenWidth > 0 && ts->pdata->screenHeight > 0 &&
			(ts->pdata->abs_x_max - ts->pdata->abs_x_min) > 0 &&
			(ts->pdata->abs_y_max - ts->pdata->abs_y_min) > 0) {
			ts->widthFactor = (ts->pdata->screenWidth << SHIFTBITS) / (ts->pdata->abs_x_max - ts->pdata->abs_x_min);
			ts->heightFactor = (ts->pdata->screenHeight << SHIFTBITS) / (ts->pdata->abs_y_max - ts->pdata->abs_y_min);

			if (ts->widthFactor > 0 && ts->heightFactor > 0)
				ts->useScreenRes = 1;
			else {
				ts->heightFactor = 0;
				ts->widthFactor = 0;
				ts->useScreenRes = 0;
			}
		} else
			I("Enable finger debug with raw position mode!\n");
	} else {
		ts->useScreenRes = 0;
		ts->widthFactor = 0;
		ts->heightFactor = 0;
	}

	return len;
}

static const struct file_operations himax_proc_debug_level_ops = {
	.owner = THIS_MODULE,
	.read = himax_debug_level_read,
	.write = himax_debug_level_write,
};

static ssize_t himax_proc_register_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	int ret = 0;
	uint16_t loop_i;
	uint8_t data[128];
	char *temp_buf;

	memset(data, 0x00, sizeof(data));

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	I("himax_register_show: %02X,%02X,%02X,%02X\n", register_command[3], register_command[2], register_command[1], register_command[0]);
	g_core_fp.fp_register_read(register_command, 128, data, cfg_flag);
	ret += snprintf(temp_buf + ret, len - ret, "command:  %02X,%02X,%02X,%02X\n", register_command[3], register_command[2], register_command[1], register_command[0]);

	for (loop_i = 0; loop_i < 128; loop_i++) {
		ret += snprintf(temp_buf + ret, len - ret, "0x%2.2X ", data[loop_i]);
		if ((loop_i % 16) == 15)
			ret += snprintf(temp_buf + ret, len - ret, "\n");
	}

	ret += snprintf(temp_buf + ret, len - ret, "\n");

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static ssize_t himax_proc_register_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	char buf[COMMON_BUF_SZ] = {0};
	char buf_tmp[16];
	uint8_t length = 0;
	unsigned long result = 0;
	uint8_t loop_i = 0;
	uint16_t base = 2;
	char *data_str = NULL;
	uint8_t w_data[20];
	uint8_t x_pos[20];
	uint8_t count = 0;

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	memset(w_data, 0x0, sizeof(w_data));
	memset(x_pos, 0x0, sizeof(x_pos));
	memset(register_command, 0x0, sizeof(register_command));

	I("himax %s\n", buf);

	if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':' && buf[2] == 'x') {
		length = strlen(buf);

		/* I("%s: length = %d.\n", __func__,length); */
		for (loop_i = 0; loop_i < length; loop_i++) { /* find postion of 'x' */
			if (buf[loop_i] == 'x') {
				x_pos[count] = loop_i;
				count++;
			}
		}

		data_str = strrchr(buf, 'x');
		I("%s: %s.\n", __func__, data_str);
		length = strlen(data_str + 1) - 1;

		if (buf[0] == 'r') {
			if (buf[3] == 'F' && buf[4] == 'E' && length == 4) {
				length = length - base;
				cfg_flag = 1;
				memcpy(buf_tmp, data_str + base + 1, length);
			} else {
				cfg_flag = 0;
				memcpy(buf_tmp, data_str + 1, length);
			}

			byte_length = length / 2;

			if (!kstrtoul(buf_tmp, 16, &result)) {
				for (loop_i = 0 ; loop_i < byte_length ; loop_i++)
					register_command[loop_i] = (uint8_t)(result >> loop_i * 8);
			}

			if (strcmp(HX_85XX_H_SERIES_PWON, private_ts->chip_name) == 0 && cfg_flag == 0)
				cfg_flag = 2;
		} else if (buf[0] == 'w') {
			if (buf[3] == 'F' && buf[4] == 'E') {
				cfg_flag = 1;
				memcpy(buf_tmp, buf + base + 3, length);
			} else {
				cfg_flag = 0;
				memcpy(buf_tmp, buf + 3, length);
			}

			if (count < 3) {
				byte_length = length / 2;

				if (!kstrtoul(buf_tmp, 16, &result)) { /* command */
					for (loop_i = 0 ; loop_i < byte_length ; loop_i++)
						register_command[loop_i] = (uint8_t)(result >> loop_i * 8);
				}

				if (!kstrtoul(data_str + 1, 16, &result)) { /* data */
					for (loop_i = 0 ; loop_i < byte_length ; loop_i++)
						w_data[loop_i] = (uint8_t)(result >> loop_i * 8);
				}

				g_core_fp.fp_register_write(register_command, byte_length, w_data, cfg_flag);
			} else {
				for (loop_i = 0; loop_i < count; loop_i++) { /* parsing addr after 'x' */
					memset(buf_tmp, 0x0, sizeof(buf_tmp));
					if (cfg_flag != 0 && loop_i != 0)
						byte_length = 2;
					else
						byte_length = x_pos[1] - x_pos[0] - 2; /* original */

					memcpy(buf_tmp, buf + x_pos[loop_i] + 1, byte_length);

					/* I("%s: buf_tmp = %s\n", __func__,buf_tmp); */
					if (kstrtoul(buf_tmp, 16, &result) != 0)
						continue;

					if (loop_i == 0)
						register_command[loop_i] = (uint8_t)(result);
					else
						w_data[loop_i - 1] = (uint8_t)(result);
				}

				byte_length = count - 1;
				if (strcmp(HX_85XX_H_SERIES_PWON, private_ts->chip_name) == 0 && cfg_flag == 0)
					cfg_flag = 2;
				g_core_fp.fp_register_write(register_command, byte_length, &w_data[0], cfg_flag);
			}
		} else
			return len;
	}

	return len;
}

static const struct file_operations himax_proc_register_ops = {
	.owner = THIS_MODULE,
	.read = himax_proc_register_read,
	.write = himax_proc_register_write,
};

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
	return &diag_self[0];
}
int32_t *getSelfNewBuffer(void)
{
	return &diag_self_new[0];
}
int32_t *getSelfOldBuffer(void)
{
	return &diag_self_old[0];
}
uint8_t getXChannel(void)
{
	return x_channel;
}
uint8_t getYChannel(void)
{
	return y_channel;
}
void setXChannel(uint8_t x)
{
	x_channel = x;
}
void setYChannel(uint8_t y)
{
	y_channel = y;
}
void setMutualBuffer(void)
{
	diag_mutual = kzalloc(x_channel * y_channel * sizeof(int32_t), GFP_KERNEL);
	if (!diag_mutual)
		E("%s: allocate memory failed!\n", __func__);
}
void setMutualNewBuffer(void)
{
	diag_mutual_new = kzalloc(x_channel * y_channel * sizeof(int32_t), GFP_KERNEL);
	if (!diag_mutual_new)
		E("%s: allocate memory failed!\n", __func__);
}
void setMutualOldBuffer(void)
{
	diag_mutual_old = kzalloc(x_channel * y_channel * sizeof(int32_t), GFP_KERNEL);
	if (!diag_mutual_old)
		E("%s: allocate memory failed!\n", __func__);
}

#ifdef HX_TP_PROC_2T2R
int32_t *getMutualBuffer_2(void)
{
	return diag_mutual_2;
}

uint8_t getXChannel_2(void)
{
	return x_channel_2;
}

uint8_t getYChannel_2(void)
{
	return y_channel_2;
}

void setXChannel_2(uint8_t x)
{
	x_channel_2 = x;
}

void setYChannel_2(uint8_t y)
{
	y_channel_2 = y;
}

void setMutualBuffer_2(void)
{
	diag_mutual_2 = kzalloc(x_channel_2 * y_channel_2 * sizeof(int32_t), GFP_KERNEL);
}
#endif

int himax_set_diag_cmd(struct himax_ic_data *ic_data, struct himax_report_data *hx_touch_data)
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

#ifdef HX_TP_PROC_2T2R

		if (Is_2T2R && (hx_touch_data->diag_cmd >= 4 && hx_touch_data->diag_cmd <= 6)) {
			mutual_data = getMutualBuffer_2();
			self_data	= getSelfBuffer();
			/* initiallize the block number of mutual and self */
			mul_num = getXChannel_2() * getYChannel_2();
#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel_2() + getYChannel_2() + ic_data->HX_BT_NUM;
#else
			self_num = getXChannel_2() + getYChannel_2();
#endif
		} else
#endif
		{
			mutual_data = getMutualBuffer();
			self_data	= getSelfBuffer();
			/* initiallize the block number of mutual and self */
			mul_num = getXChannel() * getYChannel();
#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel() + getYChannel() + ic_data->HX_BT_NUM;
#else
			self_num = getXChannel() + getYChannel();
#endif
		}

		g_core_fp.fp_diag_parse_raw_data(hx_touch_data, mul_num, self_num, hx_touch_data->diag_cmd, mutual_data, self_data);
	}

	else
		if (hx_touch_data->diag_cmd == 8) {
			memset(diag_coor, 0x00, sizeof(diag_coor));
			memcpy(&(diag_coor[0]), &hx_touch_data->hx_coord_buf[0], hx_touch_data->touch_info_size);
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
	int loop_i = 0;
	int print_size = 0;
	uint8_t *buf;

	if (start == 1)
		return;

	if (hx_touch_data->diag_cmd == 0) {
		print_size = hx_touch_data->touch_info_size;
		buf = kcalloc(hx_touch_data->touch_info_size, sizeof(uint8_t), GFP_KERNEL);
		if (!buf) {
			E("%s: allocate memory failed!\n", __func__);
			return;
		}
		memcpy(buf, hx_touch_data->hx_coord_buf, hx_touch_data->touch_info_size);
	} else if (hx_touch_data->diag_cmd > 0) {
		print_size = hx_touch_data->touch_all_size;
		buf = kcalloc(hx_touch_data->touch_info_size, sizeof(uint8_t), GFP_KERNEL);
		if (!buf) {
			E("%s: allocate memory failed!\n", __func__);
			return;
		}
		memcpy(buf, hx_touch_data->hx_coord_buf, hx_touch_data->touch_info_size);
		memcpy(&buf[hx_touch_data->touch_info_size], hx_touch_data->hx_rawdata_buf, hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
	} else
		E("%s:cmd fault\n", __func__);

	for (loop_i = 0; loop_i < print_size; loop_i += 8) {
		if ((loop_i + 7) >= print_size) {
			I("%s: over flow\n", __func__);
			break;
		}

		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i, buf[loop_i], loop_i + 1, buf[loop_i + 1]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 2, buf[loop_i + 2], loop_i + 3, buf[loop_i + 3]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 4, buf[loop_i + 4], loop_i + 5, buf[loop_i + 5]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 6, buf[loop_i + 6], loop_i + 7, buf[loop_i + 7]);
		I("\n");
	}
	kfree(buf);
}

void himax_log_touch_event(struct himax_ts_data *ts, int start)
{
	int loop_i = 0;

	if (g_target_report_data->finger_on > 0 && g_target_report_data->finger_num > 0) {

		for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
			if (g_target_report_data->x[loop_i] >= 0 && g_target_report_data->x[loop_i] <= ts->pdata->abs_x_max && g_target_report_data->y[loop_i] >= 0 && g_target_report_data->y[loop_i] <= ts->pdata->abs_y_max) {
				I("Finger %d=> X:%d, Y:%d W:%d, Z:%d, F:%d\n", loop_i + 1,
				g_target_report_data->x[loop_i],
				g_target_report_data->y[loop_i],
				g_target_report_data->w[loop_i],
				g_target_report_data->w[loop_i],
				loop_i + 1);
			}
		}
	} else if (g_target_report_data->finger_on == 0 && g_target_report_data->finger_num == 0)
		I("All Finger leave\n");
	else
		I("%s : wrong input!\n", __func__);

}

void himax_log_touch_int_devation(int touched)
{
	if (touched == HX_FINGER_ON) {
		getnstimeofday(&timeStart);
		/*
		 * I(" Irq start time = %ld.%06ld s\n",
		 *	timeStart.tv_sec, timeStart.tv_nsec/1000);
		 */
	} else if (touched == HX_FINGER_LEAVE) {
		getnstimeofday(&timeEnd);
		timeDelta.tv_nsec = (timeEnd.tv_sec * 1000000000 + timeEnd.tv_nsec) - (timeStart.tv_sec * 1000000000 + timeStart.tv_nsec);
		/*
		 * I("Irq finish time = %ld.%06ld s\n",
		 *	timeEnd.tv_sec, timeEnd.tv_nsec/1000);
		 */
		I("Touch latency = %ld us\n", timeDelta.tv_nsec / 1000);
	} else
		I("%s : wrong input!\n", __func__);
}

void himax_log_touch_event_detail(struct himax_ts_data *ts, int start)
{
	int loop_i = 0;

	if (start == HX_FINGER_LEAVE) {
		for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
			if (((ts->old_finger >> loop_i & 1) == 0) && ((ts->pre_finger_mask >> loop_i & 1) == 1)) {
				if (g_target_report_data->x[loop_i] >= 0 && g_target_report_data->x[loop_i] <= ts->pdata->abs_x_max && g_target_report_data->y[loop_i] >= 0 && g_target_report_data->y[loop_i] <= ts->pdata->abs_y_max)
					I("status: Raw:F:%02d Down, X:%d, Y:%d, W:%d\n", loop_i + 1, g_target_report_data->x[loop_i], g_target_report_data->y[loop_i], g_target_report_data->w[loop_i]);

			} else if ((((ts->old_finger >> loop_i & 1) == 1) && ((ts->pre_finger_mask >> loop_i & 1) == 0)))
				I("status: Raw:F:%02d Up, X:%d, Y:%d\n", loop_i + 1, ts->pre_finger_data[loop_i][0], ts->pre_finger_data[loop_i][1]);
			/*
			 * else
			 *	I("dbg hx_point_num=%d,old_finger=0x%02X,pre_finger_mask=0x%02X\n",ts->hx_point_num,ts->old_finger,ts->pre_finger_mask);
			 */

		}
	}

}

void himax_ts_dbg_func(struct himax_ts_data *ts, int start)
{
	switch (ts->debug_log_level) {
	case 1:
		himax_log_touch_data(start);
		break;
	case 2:
		himax_log_touch_event(ts, start);
		break;
	case 4:
		himax_log_touch_int_devation(start);
		break;
	case 8:
		himax_log_touch_event_detail(ts, start);
		break;
	}
}

/* #endif */
static ssize_t himax_diag_arrange_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	char buf[COMMON_BUF_SZ] = {0};

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	g_diag_arr_num = buf[0] - '0';
	I("%s: g_diag_arr_num = %d\n", __func__, g_diag_arr_num);
	return len;
}

void himax_get_mutual_edge(void)
{
	int i = 0;

	for (i = 0; i < (x_channel * y_channel); i++) {
		if (diag_mutual[i] > g_max_mutual)
			g_max_mutual = diag_mutual[i];

		if (diag_mutual[i] < g_min_mutual)
			g_min_mutual = diag_mutual[i];
	}
}

void himax_get_self_edge(void)
{
	int i = 0;

	for (i = 0; i < (x_channel + y_channel); i++) {
		if (diag_self[i] > g_max_self)
			g_max_self = diag_self[i];

		if (diag_self[i] < g_min_self)
			g_min_self = diag_self[i];
	}
}

/* print first step which is row */
static const struct file_operations himax_proc_diag_arrange_ops = {
	.owner = THIS_MODULE,
	.write = himax_diag_arrange_write,
};

static void print_state_info(struct seq_file *s)
{
	/* seq_printf(s, "State_info_2bytes:%3d, %3d\n",hx_state_info[0],hx_state_info[1]); */
	seq_printf(s, "ReCal = %d\t", hx_state_info[0] & 0x01);
	seq_printf(s, "Palm = %d\t", hx_state_info[0] >> 1 & 0x01);
	seq_printf(s, "AC mode = %d\t", hx_state_info[0] >> 2 & 0x01);
	seq_printf(s, "Water = %d\n", hx_state_info[0] >> 3 & 0x01);
	seq_printf(s, "Glove = %d\t", hx_state_info[0] >> 4 & 0x01);
	seq_printf(s, "TX Hop = %d\t", hx_state_info[0] >> 5 & 0x01);
	seq_printf(s, "Base Line = %d\t", hx_state_info[0] >> 6 & 0x01);
	seq_printf(s, "OSR Hop = %d\t", hx_state_info[1] >> 3 & 0x01);
	seq_printf(s, "KEY = %d\n", hx_state_info[1] >> 4 & 0x0F);
}

static void himax_diag_arrange_print(struct seq_file *s, int i, int j, int transpose)
{
	if (transpose)
		seq_printf(s, "%6d", diag_mutual[j + i * x_channel]);
	else
		seq_printf(s, "%6d", diag_mutual[i + j * x_channel]);
}

/* ready to print second step which is column*/
static void himax_diag_arrange_inloop(struct seq_file *s, int in_init, int out_init, bool transpose, int j)
{
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
				seq_printf(s, " %5d\n", diag_self[x_channel - j - 1]);
		}
	} else {	/* bit0 = 0 */
		for (i = 0; i < in_max; i++)
			himax_diag_arrange_print(s, i, j, transpose);

		if (transpose) {
			if (out_init > 0)
				seq_printf(s, " %5d\n", diag_self[x_channel - j - 1]);
			else
				seq_printf(s, " %5d\n", diag_self[j]);
		}
	}
}

/* print first step which is row */
static void himax_diag_arrange_outloop(struct seq_file *s, int transpose, int out_init, int in_init)
{
	int j;
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
			himax_diag_arrange_inloop(s, in_init, out_init, transpose, j);

			if (!transpose) {
				seq_printf(s, " %5d\n", diag_self[y_channel + x_channel - self_cnt]);
				self_cnt++;
			}
		}
	} else {	/* bit1 = 0 */
		/* self_cnt = x_channel; */
		for (j = 0; j < out_max; j++) {
			seq_printf(s, "%3c%02d%c", '[', j + 1, ']');
			himax_diag_arrange_inloop(s, in_init, out_init, transpose, j);

			if (!transpose)
				seq_printf(s, " %5d\n", diag_self[j + x_channel]);
		}
	}
}

/* determin the output format of diag */
static void himax_diag_arrange(struct seq_file *s)
{
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
		himax_diag_arrange_outloop(s, bit2, bit1 * y_channel, bit0 * x_channel);
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
		himax_diag_arrange_outloop(s, bit2, bit1 * x_channel, bit0 * y_channel);
		seq_printf(s, "%6c", ' ');

		if (bit1 == 1) {
			for (i = x_channel + y_channel - 1; i >= x_channel; i--)
				seq_printf(s, "%6d", diag_self[i]);

		} else {
			for (i = x_channel; i < x_channel + y_channel; i++)
				seq_printf(s, "%6d", diag_self[i]);

		}
	}
}

static void *himax_diag_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;

	return (void *)((unsigned long) *pos + 1);
}

static void *himax_diag_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_diag_seq_stop(struct seq_file *s, void *v)
{
}

static int himax_diag_seq_read(struct seq_file *s, void *v)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;
	uint32_t loop_i;
	uint16_t mutual_num, self_num, width;
	int dsram_type = 0;

	dsram_type = ts->diag_cmd / 10;

#ifdef HX_TP_PROC_2T2R
	if (Is_2T2R && (ts->diag_cmd >= 4 && ts->diag_cmd <= 6)) {
		mutual_num = x_channel_2 * y_channel_2;
		self_num   = x_channel_2 + y_channel_2; /* don't add KEY_COUNT */
		width      = x_channel_2;
		seq_printf(s, "ChannelStart: %4d, %4d\n\n", x_channel_2, y_channel_2);
	} else
#endif
	{
		mutual_num = x_channel * y_channel;
		self_num   = x_channel + y_channel; /* don't add KEY_COUNT */
		width      = x_channel;
		seq_printf(s, "ChannelStart: %4d, %4d\n\n", x_channel, y_channel);
	}

	/* start to show out the raw data in adb shell */
	if ((ts->diag_cmd >= 1 && ts->diag_cmd <= 3) || (ts->diag_cmd == 7)) {
		himax_diag_arrange(s);
		seq_puts(s, "\n");
#ifdef HX_EN_SEL_BUTTON
		seq_puts(s, "\n");

		for (loop_i = 0; loop_i < ic_data->HX_BT_NUM; loop_i++)
			seq_printf(s, "%6d", diag_self[ic_data->HX_RX_NUM + ic_data->HX_TX_NUM + loop_i]);

#endif
		seq_puts(s, "ChannelEnd");
		seq_puts(s, "\n");
	}

#ifdef HX_TP_PROC_2T2R

	else if (Is_2T2R && ts->diag_cmd >= 4 && ts->diag_cmd <= 6) {
		for (loop_i = 0; loop_i < mutual_num; loop_i++) {
			seq_printf(s, "%4d", diag_mutual_2[loop_i]);

			if ((loop_i % width) == (width - 1))
				seq_printf(s, " %4d\n", diag_self[width + loop_i / width]);
		}

		seq_puts(s, "\n");

		for (loop_i = 0; loop_i < width; loop_i++) {
			seq_printf(s, "%4d", diag_self[loop_i]);

			if (((loop_i) % width) == (width - 1))
				seq_puts(s, "\n");
		}

#ifdef HX_EN_SEL_BUTTON
		seq_puts(s, "\n");

		for (loop_i = 0; loop_i < ic_data->HX_BT_NUM; loop_i++)
			seq_printf(s, "%4d", diag_self[ic_data->HX_RX_NUM_2 + ic_data->HX_TX_NUM_2 + loop_i]);

#endif
		seq_puts(s, "ChannelEnd");
		seq_puts(s, "\n");
	}

#endif

	else if (ts->diag_cmd == 8) {
		for (loop_i = 0; loop_i < 128 ; loop_i++) {
			if ((loop_i % 16) == 0)
				seq_puts(s, "LineStart:");

			seq_printf(s, "%4x", diag_coor[loop_i]);

			if ((loop_i % 16) == 15)
				seq_puts(s, "\n");
		}
	} else if (dsram_type > 0 && dsram_type <= 8) {
		himax_diag_arrange(s);
		seq_puts(s, "\n ChannelEnd");
		seq_puts(s, "\n");
	}

	if ((ts->diag_cmd >= 1 && ts->diag_cmd <= 7) || dsram_type > 0) {
		/* print Mutual/Slef Maximum and Minimum */
		himax_get_mutual_edge();
		himax_get_self_edge();
		seq_printf(s, "Mutual Max:%3d, Min:%3d\n", g_max_mutual, g_min_mutual);
		seq_printf(s, "Self Max:%3d, Min:%3d\n", g_max_self, g_min_self);
		/* recovery status after print */
		g_max_mutual = 0;
		g_min_mutual = 0xFFFF;
		g_max_self = 0;
		g_min_self = 0xFFFF;
	}

	/* pring state info */
	print_state_info(s);
	return ret;
}

static const struct seq_operations himax_diag_seq_ops = {
	.start = himax_diag_seq_start,
	.next = himax_diag_seq_next,
	.stop = himax_diag_seq_stop,
	.show = himax_diag_seq_read,
};

static int himax_diag_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_diag_seq_ops);
};

bool DSRAM_Flag;

/* DSRAM thread */
void himax_ts_diag_func(void)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0, j = 0;
	unsigned int index = 0;
	int total_size = (y_channel * x_channel + y_channel + x_channel) * 2;
	uint8_t *info_data;
	int32_t *mutual_data;
	int32_t *mutual_data_new;
	int32_t *mutual_data_old;
	int32_t *self_data;
	int32_t *self_data_new;
	int32_t *self_data_old;
	int32_t new_data;
	/* 1:common dsram,2:100 frame Max,3:N-(N-1)frame */
	int dsram_type = 0;
	char temp_buf[20];
	char write_buf[total_size * 3];

	mutual_data = NULL;
	mutual_data_new = NULL;
	mutual_data_old = NULL;
	self_data = NULL;
	self_data_new = NULL;
	self_data_old = NULL;

	info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (info_data == NULL) {
		E("%s: allocate memory failed!\n", __func__);
		return;
	}
	memset(write_buf, '\0', sizeof(write_buf));
	memset(info_data, 0, total_size * sizeof(uint8_t));
	dsram_type = ts->diag_cmd / 10;
	I("%s:Entering ts->diag_cmd=%d\n!", __func__, ts->diag_cmd);

	if (dsram_type == 8) {
		dsram_type = 1;
		I("%s Sorting Mode run sram type1 !\n", __func__);
	}

	g_core_fp.fp_burst_enable(1);

	if (dsram_type == 1 || dsram_type == 2 || dsram_type == 4) {
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

	g_core_fp.fp_get_DSRAM_data(info_data, DSRAM_Flag);
	index = 0;

	for (i = 0; i < y_channel; i++) { /* mutual data */
		for (j = 0; j < x_channel; j++) {
			new_data = (((int8_t)info_data[index + 1] << 8) | info_data[index]);

			if (dsram_type == 1 || dsram_type == 4)
				mutual_data[i * x_channel + j] = new_data;
			else if (dsram_type == 2) {
					/* Keep max data */
				if (mutual_data[i * x_channel + j] < new_data)
					mutual_data[i * x_channel + j] = new_data;
			} else if (dsram_type == 3) {
				/* Cal data for [N]-[N-1] frame */
				mutual_data_new[i * x_channel + j] = new_data;
				mutual_data[i * x_channel + j] = mutual_data_new[i * x_channel + j] - mutual_data_old[i * x_channel + j];
			}

			index += 2;
		}
	}

	for (i = 0; i < x_channel + y_channel; i++) { /* self data */
		new_data = (info_data[index + 1] << 8 | info_data[index]);
		if (dsram_type == 1 || dsram_type == 4)
			self_data[i] = new_data;
		else if (dsram_type == 2) {
			/* Keep max data */
			if (self_data[i] < new_data)
				self_data[i] = new_data;
		} else if (dsram_type == 3) {
			/* Cal data for [N]-[N-1] frame */
			self_data_new[i] = new_data;
			self_data[i] = self_data_new[i] - self_data_old[i];
		}
		index += 2;
	}

	kfree(info_data);

	if (dsram_type == 3) {
		memcpy(mutual_data_old, mutual_data_new, x_channel * y_channel * sizeof(int32_t)); /* copy N data to N-1 array */
		memcpy(self_data_old, self_data_new, (x_channel + y_channel) * sizeof(int32_t)); /* copy N data to N-1 array */
	}

	diag_max_cnt++;

	if (dsram_type >= 1 && dsram_type <= 3)
		queue_delayed_work(private_ts->himax_diag_wq, &private_ts->himax_diag_delay_wrok, 1 / 10 * HZ);
	else if (dsram_type == 4) {
		for (i = 0; i < x_channel * y_channel; i++) {
			memset(temp_buf, '\0', sizeof(temp_buf));

			if (i == (x_channel * y_channel - 1)) {
				snprintf(temp_buf, sizeof(temp_buf), "%4d\t", mutual_data[i]);
				snprintf(temp_buf, sizeof(temp_buf), "%4d\n", self_data[x_channel + y_channel - 1]);
				I("%s :i = %d 3\n", __func__, i);
			} else if (i % x_channel == (x_channel - 1)) {
				snprintf(temp_buf, sizeof(temp_buf), "%4d\t", mutual_data[i]);
				snprintf(temp_buf, sizeof(temp_buf), "%4d\n", self_data[x_channel + (i / x_channel) + 1]);
			} else
				snprintf(temp_buf, sizeof(temp_buf), "%4d\t", mutual_data[i]);

			strlcat(&write_buf[i*strlen(temp_buf)], temp_buf, strlen(temp_buf));
		}

		for (i = 0; i < x_channel; i++) {
			memset(temp_buf, '\0', sizeof(temp_buf));
			if (i == x_channel - 1)
				snprintf(temp_buf, sizeof(temp_buf), "%4d\n", self_data[i]);
			else
				snprintf(temp_buf, sizeof(temp_buf), "%4d\t", self_data[i]);
			strlcat(&write_buf[(i+x_channel * y_channel)*strlen(temp_buf)], temp_buf, strlen(temp_buf));
		}

		/* save raw data in file */
		if (!IS_ERR(diag_sram_fn)) {
			I("%s create file and ready to write\n", __func__);
			diag_sram_fn->f_op->write(diag_sram_fn, write_buf, sizeof(write_buf), &diag_sram_fn->f_pos);
			write_counter++;

			if (write_counter < write_max_count)
				queue_delayed_work(private_ts->himax_diag_wq, &private_ts->himax_diag_delay_wrok, 1 / 10 * HZ);
			else {
				filp_close(diag_sram_fn, NULL);
				write_counter = 0;
			}
		}
	}
}

static ssize_t himax_diag_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	struct himax_ts_data *ts = private_ts;
	char messages[COMMON_BUF_SZ] = {0};
	struct filename *vts_name;
	uint8_t command[2] = {0x00, 0x00};
	uint8_t receive[1];
	/* 0: common , other: dsram */
	int storage_type = 0;
	/* 1:IIR,2:DC,3:Bank,4:IIR2,5:IIR2_N,6:FIR2,7:Baseline,8:dump coord */
	int rawdata_type = 0;

	memset(receive, 0x00, sizeof(receive));

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	I("%s:g_switch_mode = %d\n", __func__, g_switch_mode);

	if (messages[1] == 0x0A)
		ts->diag_cmd = messages[0] - '0';
	else
		ts->diag_cmd = (messages[0] - '0') * 10 + (messages[1] - '0');

	storage_type = g_core_fp.fp_determin_diag_storage(ts->diag_cmd);
	rawdata_type = g_core_fp.fp_determin_diag_rawdata(ts->diag_cmd);

	if (ts->diag_cmd > 0 && rawdata_type == 0) {
		I("[Himax]ts->diag_cmd=0x%x ,storage_type=%d, rawdata_type=%d! Maybe no support!\n"
		  , ts->diag_cmd, storage_type, rawdata_type);
		ts->diag_cmd = 0x00;
	} else
		I("[Himax]ts->diag_cmd=0x%x ,storage_type=%d, rawdata_type=%d\n", ts->diag_cmd, storage_type, rawdata_type);

	memset(diag_mutual, 0x00, x_channel * y_channel * sizeof(int32_t));
	memset(diag_self, 0x00, sizeof(diag_self));
	if (storage_type == 0 && rawdata_type >= RAW_IIR && rawdata_type < RAW_DUMP_COORD) {
		I("%s,common\n", __func__);

		if (DSRAM_Flag) {
			/* 1. Clear DSRAM flag */
			DSRAM_Flag = false;
			/* 2. Stop DSRAM thread */
			cancel_delayed_work(&private_ts->himax_diag_delay_wrok);
			/* 3. Enable ISR */
			himax_int_enable(1);
			/* (4) FW leave sram and return to event stack */
			g_core_fp.fp_return_event_stack();
		}

		if (g_switch_mode == 2) {
			g_core_fp.fp_idle_mode(0);
			g_switch_mode = g_core_fp.fp_switch_mode(0);
		}

		if (ts->diag_cmd == 0x04) {
#if defined(HX_TP_PROC_2T2R)
			command[0] = ts->diag_cmd;
#else
			ts->diag_cmd = 0x00;
			command[0] = 0x00;
#endif
		} else
			command[0] = ts->diag_cmd;

		g_core_fp.fp_diag_register_set(command[0], storage_type);
	} else if (storage_type > 0 && storage_type < 8
			&& rawdata_type >= RAW_IIR && rawdata_type < RAW_DUMP_COORD) {
		I("%s,dsram\n", __func__);
		diag_max_cnt = 0;

		/* 0. set diag flag */
		if (DSRAM_Flag) {
			/* (1) Clear DSRAM flag */
			DSRAM_Flag = false;
			/* (2) Stop DSRAM thread */
			cancel_delayed_work(&private_ts->himax_diag_delay_wrok);
			/* (3) Enable ISR */
			himax_int_enable(1);
			/* (4) FW leave sram and return to event stack */
			g_core_fp.fp_return_event_stack();
		}

		/* close sorting if turn on */
		if (g_switch_mode == 2) {
			g_core_fp.fp_idle_mode(0);
			g_switch_mode = g_core_fp.fp_switch_mode(0);
		}

		command[0] = rawdata_type;/* ts->diag_cmd; */
		g_core_fp.fp_diag_register_set(command[0], storage_type);
		/* 1. Disable ISR */
		himax_int_enable(0);

		/* Open file for save raw data log */
		if (storage_type == 4) {
			switch (rawdata_type) {
			case RAW_IIR:
				vts_name = getname_kernel(IIR_DUMP_FILE);
				diag_sram_fn = file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
				break;

			case RAW_DC:
				vts_name = getname_kernel(DC_DUMP_FILE);
				diag_sram_fn = file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
				break;

			case RAW_BANK:
				vts_name = getname_kernel(BANK_DUMP_FILE);
				diag_sram_fn = file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
				break;

			default:
				I("%s raw data type is not true. raw data type is %d\n", __func__, rawdata_type);
			}
		}

		/* 2. Start DSRAM thread */
		queue_delayed_work(private_ts->himax_diag_wq, &private_ts->himax_diag_delay_wrok, 2 * HZ / 100);
		I("%s: Start get raw data in DSRAM\n", __func__);

		if (storage_type == 4)
			msleep(6000);

		/* 3. Set DSRAM flag */
		DSRAM_Flag = true;

	} else if (storage_type == 8) {
		I("Soritng mode!\n");

		if (DSRAM_Flag) {
			/* 1. Clear DSRAM flag */
			DSRAM_Flag = false;
			/* 2. Stop DSRAM thread */
			cancel_delayed_work(&private_ts->himax_diag_delay_wrok);
			/* 3. Enable ISR */
			himax_int_enable(1);
			/* (4) FW leave sram and return to event stack */
			g_core_fp.fp_return_event_stack();
		}

		g_core_fp.fp_idle_mode(1);
		g_switch_mode = g_core_fp.fp_switch_mode(1);

		if (g_switch_mode == 2)
			g_core_fp.fp_diag_register_set(command[0], storage_type);

		queue_delayed_work(private_ts->himax_diag_wq, &private_ts->himax_diag_delay_wrok, 2 * HZ / 100);
		DSRAM_Flag = true;
	} else {
		/* set diag flag */
		if (DSRAM_Flag) {
			I("return and cancel sram thread!\n");
			/* (1) Clear DSRAM flag */
			DSRAM_Flag = false;
			/* (2) Stop DSRAM thread */
			cancel_delayed_work(&private_ts->himax_diag_delay_wrok);
			/* (3) Enable ISR */
			himax_int_enable(1);
			/* (4) FW leave sram and return to event stack */
			g_core_fp.fp_return_event_stack();
		}

		if (g_switch_mode == 2) {
			g_core_fp.fp_idle_mode(0);
			g_switch_mode = g_core_fp.fp_switch_mode(0);
		}

		if (ts->diag_cmd != 0x00) {
			E("[Himax]ts->diag_cmd error!diag_command=0x%x so reset\n", ts->diag_cmd);
			command[0] = 0x00;

			if (ts->diag_cmd != 0x08)
				ts->diag_cmd = 0x00;

			g_core_fp.fp_diag_register_set(command[0], storage_type);
		} else {
			command[0] = 0x00;
			ts->diag_cmd = 0x00;
			g_core_fp.fp_diag_register_set(command[0], storage_type);
			I("return to normal ts->diag_cmd=0x%x\n", ts->diag_cmd);
		}
	}

	return len;
}

static const struct file_operations himax_proc_diag_ops = {
	.owner = THIS_MODULE,
	.open = himax_diag_proc_open,
	.read = seq_read,
	.write = himax_diag_write,
};

static ssize_t himax_reset_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	char buf_tmp[DEBUG_BUF_SZ];

	if (len >= DEBUG_BUF_SZ) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf_tmp, buff, len))
		return -EFAULT;

#ifdef HX_RST_PIN_FUNC

	if (buf_tmp[0] == '1')
		g_core_fp.fp_ic_reset(false, false);
	else if (buf_tmp[0] == '2')
		g_core_fp.fp_ic_reset(false, true);
	else if (buf_tmp[0] == '3')
		g_core_fp.fp_ic_reset(true, false);
	else if (buf_tmp[0] == '4')
		g_core_fp.fp_ic_reset(true, true);

	/* else if (buf_tmp[0] == '5') */
	/*	ESD_HW_REST(); */
#endif
	return len;
}

static const struct file_operations himax_proc_reset_ops = {
	.owner = THIS_MODULE,
	.write = himax_reset_write,
};

static ssize_t himax_debug_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	size_t ret = 0;
	char *temp_buf;

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}

	if (debug_level_cmd == 't') {
		if (fw_update_complete)
			ret += snprintf(temp_buf + ret, len - ret, "FW Update Complete ");
		else
			ret += snprintf(temp_buf + ret, len - ret, "FW Update Fail ");

	} else if (debug_level_cmd == 'h') {
		if (handshaking_result == 0)
			ret += snprintf(temp_buf + ret, len - ret, "Handshaking Result = %d (MCU Running)\n", handshaking_result);
		else if (handshaking_result == 1)
			ret += snprintf(temp_buf + ret, len - ret, "Handshaking Result = %d (MCU Stop)\n", handshaking_result);
		else if (handshaking_result == 2)
			ret += snprintf(temp_buf + ret, len - ret, "Handshaking Result = %d (I2C Error)\n", handshaking_result);
		else
			ret += snprintf(temp_buf + ret, len - ret, "Handshaking Result = error\n");

	} else if (debug_level_cmd == 'v') {
		ret += snprintf(temp_buf + ret, len - ret, "FW_VER = 0x%2.2X\n", ic_data->vendor_fw_ver);

		if (private_ts->chip_cell_type == CHIP_IS_ON_CELL)
			ret += snprintf(temp_buf + ret, len - ret, "CONFIG_VER = 0x%2.2X\n", ic_data->vendor_config_ver);
		else {
			ret += snprintf(temp_buf + ret, len - ret, "TOUCH_VER = 0x%2.2X\n", ic_data->vendor_touch_cfg_ver);
			ret += snprintf(temp_buf + ret, len - ret, "DISPLAY_VER = 0x%2.2X\n", ic_data->vendor_display_cfg_ver);
		}
		if (ic_data->vendor_cid_maj_ver < 0 && ic_data->vendor_cid_min_ver < 0)
			ret += snprintf(temp_buf + ret, len - ret, "CID_VER = NULL\n");
		else
			ret += snprintf(temp_buf + ret, len - ret, "CID_VER = 0x%2.2X\n", (ic_data->vendor_cid_maj_ver << 8 | ic_data->vendor_cid_min_ver));

		if (ic_data->vendor_panel_ver < 0)
			ret += snprintf(temp_buf + ret, len - ret, "PANEL_VER = NULL\n");
		else
			ret += snprintf(temp_buf + ret, len - ret, "PANEL_VER = 0x%2.2X\n", ic_data->vendor_panel_ver);

		ret += snprintf(temp_buf + ret, len - ret, "\n");
		ret += snprintf(temp_buf + ret, len - ret, "Himax Touch Driver Version:\n");
		ret += snprintf(temp_buf + ret, len - ret, "%s\n", HIMAX_DRIVER_VER);

	} else if (debug_level_cmd == 'd') {
		ret += snprintf(temp_buf + ret, len - ret, "Himax Touch IC Information :\n");
		ret += snprintf(temp_buf + ret, len - ret, "%s\n", private_ts->chip_name);

		switch (IC_CHECKSUM) {
		case HX_TP_BIN_CHECKSUM_SW:
			ret += snprintf(temp_buf + ret, len - ret, "IC Checksum : SW\n");
			break;

		case HX_TP_BIN_CHECKSUM_HW:
			ret += snprintf(temp_buf + ret, len - ret, "IC Checksum : HW\n");
			break;

		case HX_TP_BIN_CHECKSUM_CRC:
			ret += snprintf(temp_buf + ret, len - ret, "IC Checksum : CRC\n");
			break;

		default:
			ret += snprintf(temp_buf + ret, len - ret, "IC Checksum error.\n");
		}

		if (ic_data->HX_INT_IS_EDGE)
			ret += snprintf(temp_buf + ret, len - ret, "Driver register Interrupt : EDGE TIRGGER\n");
		else
			ret += snprintf(temp_buf + ret, len - ret, "Driver register Interrupt : LEVEL TRIGGER\n");

		if (private_ts->protocol_type == PROTOCOL_TYPE_A)
			ret += snprintf(temp_buf + ret, len - ret, "Protocol : TYPE_A\n");
		else
			ret += snprintf(temp_buf + ret, len - ret, "Protocol : TYPE_B\n");

		ret += snprintf(temp_buf + ret, len - ret, "RX Num : %d\n", ic_data->HX_RX_NUM);
		ret += snprintf(temp_buf + ret, len - ret, "TX Num : %d\n", ic_data->HX_TX_NUM);
		ret += snprintf(temp_buf + ret, len - ret, "BT Num : %d\n", ic_data->HX_BT_NUM);
		ret += snprintf(temp_buf + ret, len - ret, "X Resolution : %d\n", ic_data->HX_X_RES);
		ret += snprintf(temp_buf + ret, len - ret, "Y Resolution : %d\n", ic_data->HX_Y_RES);
		ret += snprintf(temp_buf + ret, len - ret, "Max Point : %d\n", ic_data->HX_MAX_PT);
		ret += snprintf(temp_buf + ret, len - ret, "XY reverse : %d\n", ic_data->HX_XY_REVERSE);
#ifdef HX_TP_PROC_2T2R

		if (Is_2T2R) {
			ret += snprintf(temp_buf + ret, len - ret, "2T2R panel\n");
			ret += snprintf(temp_buf + ret, len - ret, "RX Num_2 : %d\n", HX_RX_NUM_2);
			ret += snprintf(temp_buf + ret, len - ret, "TX Num_2 : %d\n", HX_TX_NUM_2);
		}

#endif
	} else if (debug_level_cmd == 'i') {

		if (g_core_fp.fp_read_i2c_status())
			ret += snprintf(temp_buf + ret, len - ret, "I2C communication is bad.\n");
		else
			ret += snprintf(temp_buf + ret, len - ret, "I2C communication is good.\n");
	} else if (debug_level_cmd == 'n') {

		if (g_core_fp.fp_read_ic_trigger_type() == 1) /* Edgd = 1, Level = 0 */
			ret += snprintf(temp_buf + ret, len - ret, "IC Interrupt type is edge trigger.\n");
		else if (g_core_fp.fp_read_ic_trigger_type() == 0)
			ret += snprintf(temp_buf + ret, len - ret, "IC Interrupt type is level trigger.\n");
		else
			ret += snprintf(temp_buf + ret, len - ret, "Unknown IC trigger type.\n");

		if (ic_data->HX_INT_IS_EDGE)
			ret += snprintf(temp_buf + ret, len - ret, "Driver register Interrupt : EDGE TIRGGER\n");
		else
			ret += snprintf(temp_buf + ret, len - ret, "Driver register Interrupt : LEVEL TRIGGER\n");
	}

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

extern int g_ts_dbg;
static ssize_t himax_debug_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	int result = 0;
	char fileName[128];
	char buf[COMMON_BUF_SZ] = {0};
	int fw_type = 0;
	const struct firmware *fw = NULL;

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == 'h') { /* handshaking */
		debug_level_cmd = buf[0];
		himax_int_enable(0);
		 /* 0:Running, 1:Stop, 2:I2C Fail */
		handshaking_result = g_core_fp.fp_hand_shaking();
		himax_int_enable(1);
		return len;
	} else if (buf[0] == 'v') { /* firmware version */
		himax_int_enable(0);
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#endif
		debug_level_cmd = buf[0];
		g_core_fp.fp_read_FW_ver();
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(true, false);
#endif
		himax_int_enable(1);
		/* himax_check_chip_version(); */
		return len;
	} else if (buf[0] == 'd') { /* ic information */
		debug_level_cmd = buf[0];
		return len;
	} else if (buf[0] == 't') {
		if (buf[1] == 's' && buf[2] == 'd'
			&& buf[3] == 'b' && buf[4] == 'g') {
			if (buf[5] == '1') {
				I("Open Ts Debug!\n");
				g_ts_dbg = 1;
			} else if (buf[5] == '0') {
				I("Close Ts Debug!\n");
				g_ts_dbg = 0;
			} else
				E("Parameter fault for ts debug\n");
			goto ENDFUCTION;
		}
		himax_int_enable(0);
		debug_level_cmd	= buf[0];
		fw_update_complete = false;
		memset(fileName, 0, 128);
		/* parse the file name */
		snprintf(fileName, len - 2, "%s", &buf[2]);
		I("%s: upgrade from file(%s) start!\n", __func__, fileName);
		result = request_firmware(&fw, fileName, private_ts->dev);

		if (result < 0) {
			I("fail to request_firmware fwpath: %s (ret:%d)\n", fileName, result);
			return result;
		}

		I("%s: FW image: %02X, %02X, %02X, %02X\n", __func__, fw->data[0], fw->data[1], fw->data[2], fw->data[3]);
		fw_type = (fw->size) / 1024;
		/* start to upgrade */
		himax_int_enable(0);
		I("Now FW size is : %dk\n", fw_type);

		switch (fw_type) {
		case 32:
			if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k((unsigned char *)fw->data, fw->size, false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
				fw_update_complete = true;
			}

			break;

		case 60:
			if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k((unsigned char *)fw->data, fw->size, false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
				fw_update_complete = true;
			}

			break;

		case 64:
			if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k((unsigned char *)fw->data, fw->size, false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
				fw_update_complete = true;
			}

			break;

		case 124:
			if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k((unsigned char *)fw->data, fw->size, false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
				fw_update_complete = true;
			}

			break;

		case 128:
			if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k((unsigned char *)fw->data, fw->size, false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
				fw_update_complete = true;
			}

			break;

		default:
			E("%s: Flash command fail: %d\n", __func__, __LINE__);
			fw_update_complete = false;
			break;
		}
		release_firmware(fw);
		goto firmware_upgrade_done;
	} else if (buf[0] == 'i' && buf[1] == '2' && buf[2] == 'c') {
		/* i2c commutation */
		debug_level_cmd = 'i';
		return len;
	} else if (buf[0] == 'i' && buf[1] == 'n' && buf[2] == 't') {
		/* INT trigger */
		debug_level_cmd = 'n';
		return len;
	}

#ifdef HX_ZERO_FLASH

	else if (buf[0] == 'z') {

		if (buf[1] == '0')
			g_core_fp.fp_0f_operation_check(0);
		else
			g_core_fp.fp_0f_operation_check(1);
		return len;

	} else if (buf[0] == 'p') {

		I("NOW debug echo r!\n");
		/* himax_program_sram(); */
		private_ts->himax_0f_update_wq = create_singlethread_workqueue("HMX_update_0f_reqest_write");

		if (!private_ts->himax_0f_update_wq)
			E(" allocate syn_update_wq failed\n");

		INIT_DELAYED_WORK(&private_ts->work_0f_update, g_core_fp.fp_0f_operation);
		queue_delayed_work(private_ts->himax_0f_update_wq, &private_ts->work_0f_update, msecs_to_jiffies(100));
		return len;
	} else if (buf[0] == 'x') {
		g_core_fp.fp_sys_reset();
		return len;
	}
#endif

	else { /* others,do nothing */
		debug_level_cmd = 0;
		return len;
	}

firmware_upgrade_done:
	g_core_fp.fp_read_FW_ver();
	g_core_fp.fp_touch_information();
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(true, false);
#else
	g_core_fp.fp_sense_on(0x00);
#endif
	himax_int_enable(1);
/*	todo himax_chip->tp_firmware_upgrade_proceed = 0; */
/*	todo himax_chip->suspend_state = 0; */
/*	todo enable_irq(himax_chip->irq); */
ENDFUCTION:
	return len;
}

static const struct file_operations himax_proc_debug_ops = {
	.owner = THIS_MODULE,
	.read = himax_debug_read,
	.write = himax_debug_write,
};

static ssize_t himax_proc_FW_debug_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	uint8_t loop_i = 0;
	uint8_t tmp_data[64];
	char *temp_buf;

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	cmd_set[0] = 0x01;

	if (g_core_fp.fp_read_FW_status(cmd_set, tmp_data) == NO_ERR) {
		ret += snprintf(temp_buf + ret, len - ret, "0x%02X%02X%02X%02X :\t", cmd_set[5], cmd_set[4], cmd_set[3], cmd_set[2]);

		for (loop_i = 0; loop_i < cmd_set[1]; loop_i++)
			ret += snprintf(temp_buf + ret, len - ret, "%5d\t", tmp_data[loop_i]);

		ret += snprintf(temp_buf + ret, len - ret, "\n");
	}

	cmd_set[0] = 0x02;

	if (g_core_fp.fp_read_FW_status(cmd_set, tmp_data) == NO_ERR) {
		for (loop_i = 0; loop_i < cmd_set[1]; loop_i = loop_i + 2) {
			if ((loop_i % 16) == 0)
				ret += snprintf(temp_buf + ret, len - ret, "0x%02X%02X%02X%02X :\t",
						cmd_set[5], cmd_set[4], cmd_set[3] + (((cmd_set[2] + loop_i) >> 8) & 0xFF), (cmd_set[2] + loop_i) & 0xFF);

			ret += snprintf(temp_buf + ret, len - ret, "%5d\t", tmp_data[loop_i] + (tmp_data[loop_i + 1] << 8));

			if ((loop_i % 16) == 14)
				ret += snprintf(temp_buf + ret, len - ret, "\n");
		}
	}

	ret += snprintf(temp_buf + ret, len - ret, "\n");

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static const struct file_operations himax_proc_fw_debug_ops = {
	.owner = THIS_MODULE,
	.read = himax_proc_FW_debug_read,
};

static ssize_t himax_proc_DD_debug_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	uint8_t tmp_data[64];
	uint8_t loop_i = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	if (mutual_set_flag == 1) {
		if (g_core_fp.fp_read_DD_status(cmd_set, tmp_data) == NO_ERR) {
			for (loop_i = 0; loop_i < cmd_set[0]; loop_i++) {
				if ((loop_i % 8) == 0)
					ret += snprintf(temp_buf + ret, len - ret, "0x%02X : ", loop_i);

				ret += snprintf(temp_buf + ret, len - ret, "0x%02X ", tmp_data[loop_i]);

				if ((loop_i % 8) == 7)
					ret += snprintf(temp_buf + ret, len - ret, "\n");
			}
		}
	}

	ret += snprintf(temp_buf + ret, len - ret, "\n");

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static ssize_t himax_proc_DD_debug_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	uint8_t i = 0;
	uint8_t cnt = 2;
	unsigned long result = 0;
	char buf_tmp[PROC_DD_BUF_SZ];
	char buf_tmp2[4];

	if (len >= PROC_DD_BUF_SZ) {
		I("%s: no command exceeds 20 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf_tmp, buff, len))
		return -EFAULT;

	memset(buf_tmp2, 0x0, sizeof(buf_tmp2));

	if (buf_tmp[2] == 'x' && buf_tmp[6] == 'x' && buf_tmp[10] == 'x') {
		mutual_set_flag = 1;

		for (i = 3; i < 12; i = i + 4) {
			memcpy(buf_tmp2, buf_tmp + i, 2);

			if (!kstrtoul(buf_tmp2, 16, &result))
				cmd_set[cnt] = (uint8_t)result;
			else
				I("String to oul is fail in cnt = %d, buf_tmp2 = %s", cnt, buf_tmp2);

			cnt--;
		}

		I("cmd_set[2] = %02X, cmd_set[1] = %02X, cmd_set[0] = %02X\n", cmd_set[2], cmd_set[1], cmd_set[0]);
	} else
		mutual_set_flag = 0;

	return len;
}

static const struct file_operations himax_proc_dd_debug_ops = {
	.owner = THIS_MODULE,
	.read = himax_proc_DD_debug_read,
	.write = himax_proc_DD_debug_write,
};

uint8_t getFlashCommand(void)
{
	return flash_command;
}

static uint8_t getFlashDumpProgress(void)
{
	return flash_progress;
}

static uint8_t getFlashDumpComplete(void)
{
	return flash_dump_complete;
}

static uint8_t getFlashDumpFail(void)
{
	return flash_dump_fail;
}

uint8_t getSysOperation(void)
{
	return sys_operation;
}

static uint8_t getFlashReadStep(void)
{
	return flash_read_step;
}

bool getFlashDumpGoing(void)
{
	return flash_dump_going;
}

void setFlashBuffer(void)
{
	flash_buffer = kcalloc(Flash_Size, sizeof(uint8_t), GFP_KERNEL);
	if (!flash_buffer)
		E("%s: allocate memory failed!\n", __func__);
}

void setSysOperation(uint8_t operation)
{
	sys_operation = operation;
}

void setFlashDumpProgress(uint8_t progress)
{
	flash_progress = progress;
	/* I("setFlashDumpProgress : progress = %d ,flash_progress = %d\n",progress,flash_progress); */
}

void setFlashDumpComplete(uint8_t status)
{
	flash_dump_complete = status;
}

void setFlashDumpFail(uint8_t fail)
{
	flash_dump_fail = fail;
}

static void setFlashCommand(uint8_t command)
{
	flash_command = command;
}

static void setFlashReadStep(uint8_t step)
{
	flash_read_step = step;
}

void setFlashDumpGoing(bool going)
{
	flash_dump_going = going;
	debug_data->flash_dump_going = going;
}

static ssize_t himax_proc_flash_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	int loop_i;
	uint8_t local_flash_read_step = 0;
	uint8_t local_flash_complete = 0;
	uint8_t local_flash_progress = 0;
	uint8_t local_flash_command = 0;
	uint8_t local_flash_fail = 0;
	char *temp_buf;

	local_flash_complete = getFlashDumpComplete();
	local_flash_progress = getFlashDumpProgress();
	local_flash_command = getFlashCommand();
	local_flash_fail = getFlashDumpFail();
	I("flash_progress = %d\n", local_flash_progress);

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}

	if (local_flash_fail) {
		ret += snprintf(temp_buf + ret, len - ret, "FlashStart:Fail\n");
		ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
		ret += snprintf(temp_buf + ret, len - ret, "\n");

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
		return ret;
	}

	if (!local_flash_complete) {
		ret += snprintf(temp_buf + ret, len - ret, "FlashStart:Ongoing:0x%2.2x\n", flash_progress);
		ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
		ret += snprintf(temp_buf + ret, len - ret, "\n");

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
		return ret;
	}

	if (local_flash_command == 1 && local_flash_complete) {
		ret += snprintf(temp_buf + ret, len - ret, "FlashStart:Complete\n");
		ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
		ret += snprintf(temp_buf + ret, len - ret, "\n");

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
		return ret;
	}

	if (local_flash_command == 3 && local_flash_complete) {
		ret += snprintf(temp_buf + ret, len - ret, "FlashStart:\n");

		for (loop_i = 0; loop_i < 128; loop_i++) {
			ret += snprintf(temp_buf + ret, len - ret, "x%2.2x", flash_buffer[loop_i]);

			if ((loop_i % 16) == 15)
				ret += snprintf(temp_buf + ret, len - ret, "\n");

		}

		ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
		ret += snprintf(temp_buf + ret, len - ret, "\n");

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
		return ret;
	}

	/* flash command == 0 , report the data */
	local_flash_read_step = getFlashReadStep();
	ret += snprintf(temp_buf + ret, len - ret, "FlashStart:%2.2x\n", local_flash_read_step);

	for (loop_i = 0; loop_i < 1024; loop_i++) {
		ret += snprintf(temp_buf + ret, len - ret, "x%2.2X", flash_buffer[local_flash_read_step * 1024 + loop_i]);

		if ((loop_i % 16) == 15)
			ret += snprintf(temp_buf + ret, len - ret, "\n");
	}

	ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
	ret += snprintf(temp_buf + ret, len - ret, "\n");

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static ssize_t himax_proc_flash_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	char buf_tmp[6];
	unsigned long result = 0;
	char buf[COMMON_BUF_SZ] = {0};

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	I("%s: buf = %s\n", __func__, buf);

	if (getSysOperation() == 1) {
		E("%s: PROC is busy , return!\n", __func__);
		return len;
	}

	if (buf[0] == '0') {
		setFlashCommand(0);

		if (buf[1] == ':' && buf[2] == 'x') {
			memcpy(buf_tmp, buf + 3, 2);
			I("%s: read_Step = %s\n", __func__, buf_tmp);

			if (!kstrtoul(buf_tmp, 16, &result)) {
				I("%s: read_Step = %lu\n", __func__, result);
				setFlashReadStep(result);
			}
		}
	} else if (buf[0] == '1') {
		/*  1_32,1_60,1_64,1_24,1_28 for flash size 32k,60k,64k,124k,128k */
		setSysOperation(1);
		setFlashCommand(1);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);

		if ((buf[1] == '_') && (buf[2] == '3') && (buf[3] == '2')) {
			Flash_Size = FW_SIZE_32k;
		} else if ((buf[1] == '_') && (buf[2] == '6')) {
			if (buf[3] == '0')
				Flash_Size = FW_SIZE_60k;
			else if (buf[3] == '4')
				Flash_Size = FW_SIZE_64k;
		} else if ((buf[1] == '_') && (buf[2] == '2')) {
			if (buf[3] == '4')
				Flash_Size = FW_SIZE_124k;
			else if (buf[3] == '8')
				Flash_Size = FW_SIZE_128k;
		}
		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	} else if (buf[0] == '2') {
		/* 2_32,2_60,2_64,2_24,2_28 for flash size 32k,60k,64k,124k,128k */
		setSysOperation(1);
		setFlashCommand(2);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);

		if ((buf[1] == '_') && (buf[2] == '3') && (buf[3] == '2'))
			Flash_Size = FW_SIZE_32k;
		else if ((buf[1] == '_') && (buf[2] == '6')) {
			if (buf[3] == '0')
				Flash_Size = FW_SIZE_60k;
			else if (buf[3] == '4')
				Flash_Size = FW_SIZE_64k;
		} else if ((buf[1] == '_') && (buf[2] == '2')) {
			if (buf[3] == '4')
				Flash_Size = FW_SIZE_124k;
			else if (buf[3] == '8')
				Flash_Size = FW_SIZE_128k;
		}
		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	}

	return len;
}

static const struct file_operations himax_proc_flash_ops = {
	.owner = THIS_MODULE,
	.read = himax_proc_flash_read,
	.write = himax_proc_flash_write,
};

void himax_ts_flash_func(void)
{
	uint8_t local_flash_command = 0;

	himax_int_enable(0);
	setFlashDumpGoing(true);
	/* sector = getFlashDumpSector(); */
	/* page = getFlashDumpPage(); */
	local_flash_command = getFlashCommand();
	msleep(100);
	I("%s: local_flash_command = %d enter.\n", __func__, local_flash_command);

	if ((local_flash_command == 1 || local_flash_command == 2) || (local_flash_command == 0x0F))
		g_core_fp.fp_flash_dump_func(local_flash_command, Flash_Size, flash_buffer);

	I("Complete~~~~~~~~~~~~~~~~~~~~~~~\n");

	if (local_flash_command == 2) {
		struct file *fn;
		struct filename *vts_name;

		vts_name = getname_kernel(FLASH_DUMP_FILE);
		fn = file_open_name(vts_name, O_CREAT | O_WRONLY, 0);

		if (!IS_ERR(fn)) {
			I("%s create file and ready to write\n", __func__);
			fn->f_op->write(fn, flash_buffer, Flash_Size * sizeof(uint8_t), &fn->f_pos);
			filp_close(fn, NULL);
		}
	}

	himax_int_enable(1);
	setFlashDumpGoing(false);
	setFlashDumpComplete(1);
	setSysOperation(0);
	return;
	/* Flash_Dump_i2c_transfer_error: */
	/* himax_int_enable(1); */
	/* setFlashDumpGoing(false); */
	/* setFlashDumpComplete(0); */
	/* setFlashDumpFail(1); */
	/* setSysOperation(0); */
	/* return; */
}



static ssize_t himax_sense_on_off_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	char buf[COMMON_BUF_SZ] = {0};

	if (len >= COMMON_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0') {
		g_core_fp.fp_sense_off();
		I("Sense off\n");
	} else if (buf[0] == '1') {
		if (buf[1] == 's') {
			g_core_fp.fp_sense_on(0x00);
			I("Sense on re-map on, run sram\n");
		} else {
			g_core_fp.fp_sense_on(0x01);
			I("Sense on re-map off, run flash\n");
		}
	} else
		I("Do nothing\n");

	return len;
}

static const struct file_operations himax_proc_sense_on_off_ops = {
	.owner = THIS_MODULE,
	.write = himax_sense_on_off_write,
};

#ifdef HX_ESD_RECOVERY
static ssize_t himax_esd_cnt_read(struct file *file, char __user *buf, size_t len, loff_t *pos)
{
	size_t ret = 0;
	char *temp_buf;

	I("%s: enter, %d\n", __func__, __LINE__);

	if (HX_PROC_SEND_FLAG) {
		HX_PROC_SEND_FLAG = 0;
		return 0;
	}

	temp_buf = kzalloc(len, GFP_KERNEL);
	if (!temp_buf) {
		E("%s: allocate memory failed!\n", __func__);
		return 0;
	}
	ret += snprintf(temp_buf + ret, len - ret, "EB_cnt = %d, EC_cnt = %d, ED_cnt = %d\n", hx_EB_event_flag, hx_EC_event_flag, hx_ED_event_flag);

	if (copy_to_user(buf, temp_buf, len))
		I("%s,here:%d\n", __func__, __LINE__);

	kfree(temp_buf);
	HX_PROC_SEND_FLAG = 1;

	return ret;
}

static ssize_t himax_esd_cnt_write(struct file *file, const char __user *buff, size_t len, loff_t *pos)
{
	int i = 0;
	char buf[DEBUG_BUF_SZ] = {0};

	if (len >= DEBUG_BUF_SZ) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	I("Clear ESD Flag\n");

	if (buf[i] == '0') {
		hx_EB_event_flag = 0;
		hx_EC_event_flag = 0;
		hx_ED_event_flag = 0;
	}

	return len;
}

static const struct file_operations himax_proc_esd_cnt_ops = {
	.owner = THIS_MODULE,
	.read = himax_esd_cnt_read,
	.write = himax_esd_cnt_write,
};
#endif

static void himax_himax_data_init(void)
{
	debug_data->fp_ts_dbg_func = himax_ts_dbg_func;
	debug_data->fp_set_diag_cmd = himax_set_diag_cmd;
	debug_data->flash_dump_going = false;
}

static void himax_ts_flash_work_func(struct work_struct *work)
{
	himax_ts_flash_func();
}

static void himax_ts_diag_work_func(struct work_struct *work)
{
	himax_ts_diag_func();
}

int himax_touch_proc_init(void)
{
	himax_proc_debug_level_file = proc_create(HIMAX_PROC_DEBUG_LEVEL_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_debug_level_ops);
	if (himax_proc_debug_level_file == NULL) {
		E(" %s: proc debug_level file create failed!\n", __func__);
		goto fail_1;
	}

	himax_proc_vendor_file = proc_create(HIMAX_PROC_VENDOR_FILE, 0444,
					himax_touch_proc_dir, &himax_proc_vendor_ops);
	if (himax_proc_vendor_file == NULL) {
		E(" %s: proc vendor file create failed!\n", __func__);
		goto fail_2;
	}

	himax_proc_attn_file = proc_create(HIMAX_PROC_ATTN_FILE, 0444,
					himax_touch_proc_dir, &himax_proc_attn_ops);
	if (himax_proc_attn_file == NULL) {
		E(" %s: proc attn file create failed!\n", __func__);
		goto fail_3;
	}

	himax_proc_int_en_file = proc_create(HIMAX_PROC_INT_EN_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_int_en_ops);
	if (himax_proc_int_en_file == NULL) {
		E(" %s: proc int en file create failed!\n", __func__);
		goto fail_4;
	}

	himax_proc_layout_file = proc_create(HIMAX_PROC_LAYOUT_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_layout_ops);
	if (himax_proc_layout_file == NULL) {
		E(" %s: proc layout file create failed!\n", __func__);
		goto fail_5;
	}

	himax_proc_reset_file = proc_create(HIMAX_PROC_RESET_FILE, 0200,
					himax_touch_proc_dir, &himax_proc_reset_ops);
	if (himax_proc_reset_file == NULL) {
		E(" %s: proc reset file create failed!\n", __func__);
		goto fail_6;
	}

	himax_proc_diag_file = proc_create(HIMAX_PROC_DIAG_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_diag_ops);
	if (himax_proc_diag_file == NULL) {
		E(" %s: proc diag file create failed!\n", __func__);
		goto fail_7;
	}

	himax_proc_diag_arrange_file = proc_create(HIMAX_PROC_DIAG_ARR_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_diag_arrange_ops);
	if (himax_proc_diag_arrange_file == NULL) {
		E(" %s: proc diag file create failed!\n", __func__);
		goto fail_7_1;
	}

	himax_proc_register_file = proc_create(HIMAX_PROC_REGISTER_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_register_ops);
	if (himax_proc_register_file == NULL) {
		E(" %s: proc register file create failed!\n", __func__);
		goto fail_8;
	}

	himax_proc_debug_file = proc_create(HIMAX_PROC_DEBUG_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_debug_ops);
	if (himax_proc_debug_file == NULL) {
		E(" %s: proc debug file create failed!\n", __func__);
		goto fail_9;
	}

	himax_proc_fw_debug_file = proc_create(HIMAX_PROC_FW_DEBUG_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_fw_debug_ops);
	if (himax_proc_fw_debug_file == NULL) {
		E(" %s: proc fw debug file create failed!\n", __func__);
		goto fail_9_1;
	}

	himax_proc_dd_debug_file = proc_create(HIMAX_PROC_DD_DEBUG_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_dd_debug_ops);
	if (himax_proc_dd_debug_file == NULL) {
		E(" %s: proc DD debug file create failed!\n", __func__);
		goto fail_9_2;
	}

	himax_proc_flash_dump_file = proc_create(HIMAX_PROC_FLASH_DUMP_FILE, 0644,
					himax_touch_proc_dir, &himax_proc_flash_ops);
	if (himax_proc_flash_dump_file == NULL) {
		E(" %s: proc flash dump file create failed!\n", __func__);
		goto fail_10;
	}

	himax_proc_SENSE_ON_OFF_file = proc_create(HIMAX_PROC_SENSE_ON_OFF_FILE, 0666,
					himax_touch_proc_dir, &himax_proc_sense_on_off_ops);
	if (himax_proc_SENSE_ON_OFF_file == NULL) {
		E(" %s: proc SENSE_ON_OFF file create failed!\n", __func__);
		goto fail_16;
	}

#ifdef HX_ESD_RECOVERY
	himax_proc_ESD_cnt_file = proc_create(HIMAX_PROC_ESD_CNT_FILE, 0666,
					himax_touch_proc_dir, &himax_proc_esd_cnt_ops);

	if (himax_proc_ESD_cnt_file == NULL) {
		E(" %s: proc ESD cnt file create failed!\n", __func__);
		goto fail_17;
	}

#endif
	himax_proc_CRC_test_file = proc_create(HIMAX_PROC_CRC_TEST_FILE, 0666,
					himax_touch_proc_dir, &himax_proc_CRC_test_ops);

	if (himax_proc_CRC_test_file == NULL) {
		E(" %s: proc CRC test file create failed!\n", __func__);
		goto fail_18;
	}

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	himax_proc_ito_test_file = proc_create(HIMAX_PROC_ITO_TEST_FILE, 0777,
					himax_touch_proc_dir, &himax_proc_ito_test_ops);

	if (himax_proc_ito_test_file == NULL) {
		E(" %s: proc ITO test file create failed!\n", __func__);
		goto fail_19;
	}

#endif
	return 0;
#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	remove_proc_entry(HIMAX_PROC_ITO_TEST_FILE, himax_touch_proc_dir);
fail_19:
#endif
fail_18:
#ifdef HX_ESD_RECOVERY
	remove_proc_entry(HIMAX_PROC_ESD_CNT_FILE, himax_touch_proc_dir);
fail_17:
#endif
	remove_proc_entry(HIMAX_PROC_SENSE_ON_OFF_FILE, himax_touch_proc_dir);
fail_16:
	remove_proc_entry(HIMAX_PROC_FLASH_DUMP_FILE, himax_touch_proc_dir);
fail_10:
	remove_proc_entry(HIMAX_PROC_DEBUG_FILE, himax_touch_proc_dir);
fail_9:
	remove_proc_entry(HIMAX_PROC_FW_DEBUG_FILE, himax_touch_proc_dir);
fail_9_1:
	remove_proc_entry(HIMAX_PROC_DD_DEBUG_FILE, himax_touch_proc_dir);
fail_9_2:
	remove_proc_entry(HIMAX_PROC_REGISTER_FILE, himax_touch_proc_dir);
fail_8:
	remove_proc_entry(HIMAX_PROC_DIAG_FILE, himax_touch_proc_dir);
fail_7:
	remove_proc_entry(HIMAX_PROC_DIAG_ARR_FILE, himax_touch_proc_dir);
fail_7_1:
	remove_proc_entry(HIMAX_PROC_RESET_FILE, himax_touch_proc_dir);
fail_6:
	remove_proc_entry(HIMAX_PROC_LAYOUT_FILE, himax_touch_proc_dir);
fail_5:
	remove_proc_entry(HIMAX_PROC_INT_EN_FILE, himax_touch_proc_dir);
fail_4:
	remove_proc_entry(HIMAX_PROC_ATTN_FILE, himax_touch_proc_dir);
fail_3:
	remove_proc_entry(HIMAX_PROC_VENDOR_FILE, himax_touch_proc_dir);
fail_2:
	remove_proc_entry(HIMAX_PROC_DEBUG_LEVEL_FILE, himax_touch_proc_dir);
fail_1:
	return -ENOMEM;
}

void himax_touch_proc_deinit(void)
{
#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	remove_proc_entry(HIMAX_PROC_ITO_TEST_FILE, himax_touch_proc_dir);
#endif
	remove_proc_entry(HIMAX_PROC_CRC_TEST_FILE, himax_touch_proc_dir);
#ifdef HX_ESD_RECOVERY
	remove_proc_entry(HIMAX_PROC_ESD_CNT_FILE, himax_touch_proc_dir);
#endif
	remove_proc_entry(HIMAX_PROC_SENSE_ON_OFF_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_FLASH_DUMP_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_DEBUG_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_FW_DEBUG_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_DD_DEBUG_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_REGISTER_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_DIAG_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_RESET_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_LAYOUT_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_INT_EN_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_ATTN_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_VENDOR_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_DEBUG_LEVEL_FILE, himax_touch_proc_dir);
}

int himax_debug_init(void)
{
	struct himax_ts_data *ts = private_ts;
	int err = 0;

	I("%s:Enter\n", __func__);

	if (ts == NULL) {
		E("%s: ts struct is NULL\n", __func__);
		return -EPROBE_DEFER;
	}

	debug_data = kzalloc(sizeof(*debug_data), GFP_KERNEL);
	if (debug_data == NULL) {
		E("%s: allocate memory failed!\n", __func__);
		err = -ENOMEM;
		goto err_alloc_debug_data_fail;
	}

	himax_himax_data_init();

	ts->flash_wq = create_singlethread_workqueue("himax_flash_wq");

	if (!ts->flash_wq) {
		E("%s: create flash workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_flash_dump_wq_failed;
	}

	INIT_WORK(&ts->flash_work, himax_ts_flash_work_func);
	setSysOperation(0);
	setFlashBuffer();

	ts->himax_diag_wq = create_singlethread_workqueue("himax_diag");

	if (!ts->himax_diag_wq) {
		E("%s: create diag workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_diag_wq_failed;
	}

	INIT_DELAYED_WORK(&ts->himax_diag_delay_wrok, himax_ts_diag_work_func);

	setXChannel(ic_data->HX_RX_NUM); /* X channel */
	setYChannel(ic_data->HX_TX_NUM); /* Y channel */
	setMutualBuffer();
	setMutualNewBuffer();
	setMutualOldBuffer();

	if (getMutualBuffer() == NULL) {
		E("%s: mutual buffer allocate fail failed\n", __func__);
		err = MEM_ALLOC_FAIL;
		goto err_get_MutualBuffer_failed;
	}
#ifdef HX_TP_PROC_2T2R

	if (Is_2T2R) {
		setXChannel_2(ic_data->HX_RX_NUM_2); /* X channel */
		setYChannel_2(ic_data->HX_TX_NUM_2); /* Y channel */
		setMutualBuffer_2();

		if (getMutualBuffer_2() == NULL) {
			E("%s: mutual buffer 2 allocate fail failed\n", __func__);
			err = MEM_ALLOC_FAIL;
			goto err_get_MutualBuffer2_failed;
		}
	}
#endif

	himax_touch_proc_init();

	return 0;

err_get_MutualBuffer2_failed:
err_get_MutualBuffer_failed:
	cancel_delayed_work_sync(&ts->himax_diag_delay_wrok);
	destroy_workqueue(ts->himax_diag_wq);

err_create_diag_wq_failed:
	destroy_workqueue(ts->flash_wq);

err_create_flash_dump_wq_failed:
	kfree(debug_data);

err_alloc_debug_data_fail:

	return err;
}

int himax_debug_remove(void)
{
	struct himax_ts_data *ts = private_ts;

	himax_touch_proc_deinit();

	cancel_delayed_work_sync(&ts->himax_diag_delay_wrok);
	destroy_workqueue(ts->himax_diag_wq);
	destroy_workqueue(ts->flash_wq);

	kfree(debug_data);

	return 0;
}


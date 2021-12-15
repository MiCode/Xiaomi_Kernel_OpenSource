/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "himax_debug.h"
#include "himax_ic.h"

/* struct himax_debug_data* debug_data; */

#ifdef HX_TP_PROC_DIAG
#ifdef HX_TP_PROC_2T2R
int HX_RX_NUM_2;
int HX_TX_NUM_2;
#endif
uint8_t g_diag_arr_num;
#endif

#if defined(HX_SMART_WAKEUP) || defined(HX_INSPECT_LPWUG_TEST)
bool FAKE_POWER_KEY_SEND;
#endif

int g_max_mutual;
int g_min_mutual = 255;
int g_max_self;
int g_min_self = 255;

#if defined(HX_TP_PROC_SELF_TEST) || defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
int g_self_test_entered;
#endif

struct timespec timeStart, timeEnd, timeDelta;
int g_switch_mode;

/* ======================================
 */
/*  */
/* Segment : Himax PROC Debug Function */
/*  */
/* ======================================
 */
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)

static ssize_t himax_ito_test_read(struct file *file, char *buf, size_t len,
				   loff_t *pos)
{
	ssize_t ret = 0;
	uint8_t result = 0;
	uint8_t status = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		status = ito_get_step_status();

		switch (status) {
		case 0:
			ret += snprintf(temp_buf + ret, len - ret,
					"Step : START_TEST\n");
			break;
		case 1:
			ret += snprintf(temp_buf + ret, len - ret,
					"Step : RAW_DATA\n");
			break;
		case 2:
			ret += snprintf(temp_buf + ret, len - ret,
					"Step : PERCENT_TEST\n");
			break;
		case 3:
			ret += snprintf(temp_buf + ret, len - ret,
					"Step : DEV_TEST\n");
			break;
		case 4:
			ret += snprintf(temp_buf + ret, len - ret,
					"Step : NOISE_TEST\n");
			break;
		case 9:
			ret += snprintf(temp_buf + ret, len - ret,
					"Step : END_TEST\n");
			break;
		default:
			ret += snprintf(temp_buf + ret, len - ret,
					"Step : Null\n");
		}

		result = ito_get_result_status();
		if (result == 0xF)
			ret += snprintf(temp_buf + ret, len - ret,
					"ITO test is On-going!\n");
		else if (result == 0)
			ret += snprintf(temp_buf + ret, len - ret,
					"ITO test is Pass!\n");
		else if (result == 2)
			ret += snprintf(temp_buf + ret, len - ret,
					"Open config file fail!\n");
		else
			ret += snprintf(temp_buf + ret, len - ret,
					"ITO test is Fail!\n");
		HX_PROC_SEND_FLAG = 1;
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
	} else
		HX_PROC_SEND_FLAG = 0;

	return ret;
}

static ssize_t himax_ito_test_write(struct file *file, const char *buff,
				    size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	uint8_t result = 0;
	char buf[80] = {0};

	if (len >= 80) {
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

static ssize_t himax_CRC_test_read(struct file *file, char *buf, size_t len,
				   loff_t *pos)
{
	ssize_t ret = 0;
	uint8_t result = 0;
	char *temp_buf;

	temp_buf = kzalloc(len, GFP_KERNEL);

	if (!HX_PROC_SEND_FLAG) {
		himax_sense_off(private_ts->client);
		msleep(20);
		result = himax_calculateChecksum(private_ts->client, false);
		himax_sense_on(private_ts->client, 0x01);
		if (result == 0)
			ret += snprintf(temp_buf + ret, len - ret,
					"CRC test is Pass!\n");
		else
			ret += snprintf(temp_buf + ret, len - ret,
					"CRC test is Fail!\n");
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;

	return ret;
}

static const struct file_operations himax_proc_CRC_test_ops = {
	.owner = THIS_MODULE, .read = himax_CRC_test_read,
};

static ssize_t himax_vendor_read(struct file *file, char *buf, size_t len,
				 loff_t *pos)
{
	ssize_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		ret += snprintf(temp_buf + ret, len - ret, "FW_VER = 0x%2.2X\n",
				ic_data->vendor_fw_ver);

		if (IC_TYPE < 8)
			ret += snprintf(temp_buf + ret, len - ret,
					"CONFIG_VER = 0x%2.2X\n",
					ic_data->vendor_config_ver);
		else {
			ret += snprintf(temp_buf + ret, len - ret,
					"TOUCH_VER = 0x%2.2X\n",
					ic_data->vendor_touch_cfg_ver);
			ret += snprintf(temp_buf + ret, len - ret,
					"DISPLAY_VER = 0x%2.2X\n",
					ic_data->vendor_display_cfg_ver);
		}
		if (ic_data->vendor_cid_maj_ver < 0 &&
		    ic_data->vendor_cid_min_ver < 0)
			ret += snprintf(temp_buf + ret, len - ret,
					"CID_VER = NULL\n");
		else
			ret += snprintf(temp_buf + ret, len - ret,
					"CID_VER = 0x%2.2X\n",
					(ic_data->vendor_cid_maj_ver << 8 |
					 ic_data->vendor_cid_min_ver));

		if (ic_data->vendor_panel_ver < 0)
			ret += snprintf(temp_buf + ret, len - ret,
					"PANEL_VER = NULL\n");
		else
			ret += snprintf(temp_buf + ret, len - ret,
					"PANEL_VER = 0x%2.2X\n",
					ic_data->vendor_panel_ver);

		ret += snprintf(temp_buf + ret, len - ret, "\n");

		ret += snprintf(temp_buf + ret, len - ret,
				"Himax Touch Driver Version:\n");
		ret += snprintf(temp_buf + ret, len - ret, "%s\n",
				HIMAX_DRIVER_VER);

		HX_PROC_SEND_FLAG = 1;
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
	} else
		HX_PROC_SEND_FLAG = 0;

	return ret;
}

static const struct file_operations himax_proc_vendor_ops = {
	.owner = THIS_MODULE, .read = himax_vendor_read,
};

static ssize_t himax_attn_read(struct file *file, char *buf, size_t len,
			       loff_t *pos)
{
	ssize_t ret = 0;
	struct himax_ts_data *ts_data;
	char *temp_buf;

	ts_data = private_ts;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		ret += snprintf(temp_buf + ret, len - ret, "attn = %x\n",
				himax_int_gpio_read(ts_data->pdata->gpio_irq));
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;

	return ret;
}

static const struct file_operations himax_proc_attn_ops = {
	.owner = THIS_MODULE, .read = himax_attn_read,
};

static ssize_t himax_int_en_read(struct file *file, char *buf, size_t len,
				 loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		ret += snprintf(temp_buf + ret, len - ret, "%d ",
				ts->irq_enabled);
		ret += snprintf(temp_buf + ret, len - ret, "\n");
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static ssize_t himax_int_en_write(struct file *file, const char *buff,
				  size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf_tmp[12] = {0};
	int value, ret = 0;

	if (len >= 12) {
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
		ret = himax_int_en_set(ts->client);
		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
		}
	} else {
		himax_int_enable(ts->client->irq, 0);
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

static ssize_t himax_layout_read(struct file *file, char *buf, size_t len,
				 loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		ret += snprintf(temp_buf + ret, len - ret, "%d ",
				ts->pdata->abs_x_min);
		ret += snprintf(temp_buf + ret, len - ret, "%d ",
				ts->pdata->abs_x_max);
		ret += snprintf(temp_buf + ret, len - ret, "%d ",
				ts->pdata->abs_y_min);
		ret += snprintf(temp_buf + ret, len - ret, "%d ",
				ts->pdata->abs_y_max);
		ret += snprintf(temp_buf + ret, len - ret, "\n");
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;

	return ret;
}

static ssize_t himax_layout_write(struct file *file, const char *buff,
				  size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf_tmp[5];
	int i = 0, j = 0, k = 0, ret;
	unsigned long value;
	int layout[4] = {0};
	char buf[80] = {0};

	if (len >= 80) {
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
		I("%d, %d, %d, %d\n", ts->pdata->abs_x_min,
		  ts->pdata->abs_x_max, ts->pdata->abs_y_min,
		  ts->pdata->abs_y_max);
		input_unregister_device(ts->input_dev);
		himax_input_register(ts);
	} else
		I("ERR@%d, %d, %d, %d\n", ts->pdata->abs_x_min,
		  ts->pdata->abs_x_max, ts->pdata->abs_y_min,
		  ts->pdata->abs_y_max);
	return len;
}

static const struct file_operations himax_proc_layout_ops = {
	.owner = THIS_MODULE,
	.read = himax_layout_read,
	.write = himax_layout_write,
};

static ssize_t himax_debug_level_read(struct file *file, char *buf, size_t len,
				      loff_t *pos)
{
	struct himax_ts_data *ts_data;
	size_t ret = 0;
	char *temp_buf;

	ts_data = private_ts;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		ret += snprintf(temp_buf + ret, len - ret, "%d\n",
				ts_data->debug_log_level);
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;

	return ret;
}

static ssize_t himax_debug_level_write(struct file *file, const char *buff,
				       size_t len, loff_t *pos)
{
	struct himax_ts_data *ts;
	char buf_tmp[11];
	int i;

	ts = private_ts;

	if (len >= 12) {
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
			ts->widthFactor =
				(ts->pdata->screenWidth << SHIFTBITS) /
				(ts->pdata->abs_x_max - ts->pdata->abs_x_min);
			ts->heightFactor =
				(ts->pdata->screenHeight << SHIFTBITS) /
				(ts->pdata->abs_y_max - ts->pdata->abs_y_min);
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

#ifdef HX_TP_PROC_REGISTER
static ssize_t himax_proc_register_read(struct file *file, char *buf,
					size_t len, loff_t *pos)
{
	int ret = 0;
	uint16_t loop_i;
	uint8_t data[128];
	char *temp_buf;

	memset(data, 0x00, sizeof(data));

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		I("himax_register_show: %02X,%02X,%02X,%02X\n",
		  register_command[3], register_command[2], register_command[1],
		  register_command[0]);
		himax_register_read(private_ts->client, register_command, 128,
				    data, cfg_flag);

		ret += snprintf(temp_buf + ret, len - ret,
				"command:  %02X,%02X,%02X,%02X\n",
				register_command[3], register_command[2],
				register_command[1], register_command[0]);

		for (loop_i = 0; loop_i < 128; loop_i++) {
			ret += snprintf(temp_buf + ret, len - ret, "0x%2.2X ",
					data[loop_i]);
			if ((loop_i % 16) == 15)
				ret += snprintf(temp_buf + ret, len - ret,
						"\n");
		}
		ret += snprintf(temp_buf + ret, len - ret, "\n");
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static ssize_t himax_proc_register_write(struct file *file, const char *buff,
					 size_t len, loff_t *pos)
{
	char buf[80] = {0};
	char buf_tmp[16];
	uint8_t length = 0;
	unsigned long result = 0;
	uint8_t loop_i = 0;
	uint16_t base = 2;
	char *data_str = NULL;
	uint8_t w_data[20];
	uint8_t x_pos[20];
	uint8_t count = 0;

	if (len >= 80) {
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

	if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':' &&
	    buf[2] == 'x') {

		length = strlen(buf);

		/* I("%s: length = %d.\n", __func__,length); */
		for (loop_i = 0; loop_i < length;
		     loop_i++) { /* find postion of 'x' */

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
				cfg_flag = true;
				memcpy(buf_tmp, data_str + base + 1, length);
			} else {
				cfg_flag = false;
				memcpy(buf_tmp, data_str + 1, length);
			}

			byte_length = length / 2;
			if (!kstrtoul(buf_tmp, 16, &result)) {
				for (loop_i = 0; loop_i < byte_length;
				     loop_i++) {
					register_command[loop_i] =
						(uint8_t)(result >> loop_i * 8);
				}
			}
		} else if (buf[0] == 'w') {
			if (buf[3] == 'F' && buf[4] == 'E') {
				cfg_flag = true;
				memcpy(buf_tmp, buf + base + 3, length);
			} else {
				cfg_flag = false;
				memcpy(buf_tmp, buf + 3, length);
			}
			if (count < 3) {
				byte_length = length / 2;
				if (!kstrtoul(buf_tmp, 16,
					      &result)) { /* command */

					for (loop_i = 0; loop_i < byte_length;
					     loop_i++) {
						register_command[loop_i] =
							(uint8_t)(result >>
								  loop_i * 8);
					}
				}
				if (!kstrtoul(data_str + 1, 16,
					      &result)) { /* data */

					for (loop_i = 0; loop_i < byte_length;
					     loop_i++) {
						w_data[loop_i] = (uint8_t)(
							result >> loop_i * 8);
					}
				}
				himax_register_write(
					private_ts->client, register_command,
					byte_length, w_data, cfg_flag);
			} else {
				for (loop_i = 0; loop_i < count;
				     loop_i++) { /* parsing addr after 'x' */

					memset(buf_tmp, 0x0, sizeof(buf_tmp));
					if (cfg_flag != 0 && loop_i != 0)
						byte_length = 2;
					else
						byte_length = x_pos[1] -
							      x_pos[0] -
							      2; /* original */
					memcpy(buf_tmp, buf + x_pos[loop_i] + 1,
					       byte_length);
					/* I("%s: buf_tmp = %s\n", */
					/* __func__,buf_tmp); */
					if (!kstrtoul(buf_tmp, 16, &result)
						&& loop_i == 0) {
						register_command[loop_i] =
								(uint8_t)(
									result);
				/* I("%s: */
				/* register_command = */
				/* %X\n", */
				/* __func__,register_command[0]);*/
					}
					if (!kstrtoul(buf_tmp, 16, &result)
						&& loop_i != 0) {
						w_data[loop_i - 1] =
								(uint8_t)(
									result);
							/* I("%s: w_data[%d] =
							 */
							/* %2X\n", */
							/* __func__,loop_i - */
							/* 1,w_data[loop_i - */
							/* 1]); */
					}
				}

				byte_length = count - 1;
				himax_register_write(
					private_ts->client, register_command,
					byte_length, &w_data[0], cfg_flag);
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
#endif

#ifdef HX_TP_PROC_DIAG
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
uint8_t getXChannel(void)
{
	return x_channel;
}
uint8_t getYChannel(void)
{
	return y_channel;
}
uint8_t getDiagCommand(void)
{
	return g_diag_command;
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
	diag_mutual =
		kzalloc(x_channel * y_channel * sizeof(int32_t), GFP_KERNEL);
}
void setMutualNewBuffer(void)
{
	diag_mutual_new =
		kzalloc(x_channel * y_channel * sizeof(int32_t), GFP_KERNEL);
}
void setMutualOldBuffer(void)
{
	diag_mutual_old =
		kzalloc(x_channel * y_channel * sizeof(int32_t), GFP_KERNEL);
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
	diag_mutual_2 = kzalloc(x_channel_2 * y_channel_2 * sizeof(int32_t),
				GFP_KERNEL);
}
#endif

#ifdef HX_TP_PROC_DIAG
int himax_set_diag_cmd(struct himax_ic_data *ic_data,
		       struct himax_report_data *hx_touch_data)
{
	int32_t *mutual_data;
	int32_t *self_data;
	int mul_num;
	int self_num;
	/* int RawDataLen = 0; */

	hx_touch_data->diag_cmd = getDiagCommand();
	if (hx_touch_data->diag_cmd >= 1 && hx_touch_data->diag_cmd <= 7) {
		/* Check event stack CRC */
		if (!diag_check_sum(hx_touch_data))
			goto bypass_checksum_failed_packet;

#ifdef HX_TP_PROC_2T2R
		if (Is_2T2R && (hx_touch_data->diag_cmd >= 4 &&
				hx_touch_data->diag_cmd <= 6)) {
			mutual_data = getMutualBuffer_2();
			self_data = getSelfBuffer();

			/* initiallize the block number of mutual and self */
			mul_num = getXChannel_2() * getYChannel_2();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel_2() + getYChannel_2() +
				   ic_data->HX_BT_NUM;
#else
			self_num = getXChannel_2() + getYChannel_2();
#endif
		} else
#endif
		{
			mutual_data = getMutualBuffer();
			self_data = getSelfBuffer();

			/* initiallize the block number of mutual and self */
			mul_num = getXChannel() * getYChannel();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel() + getYChannel() +
				   ic_data->HX_BT_NUM;
#else
			self_num = getXChannel() + getYChannel();
#endif
		}

		diag_parse_raw_data(hx_touch_data, mul_num, self_num,
				    hx_touch_data->diag_cmd, mutual_data,
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
#endif
/* #if defined(HX_DEBUG_LEVEL) */
void himax_log_touch_data(uint8_t *buf, struct himax_report_data *hx_touch_data)
{
	int loop_i = 0;
	int print_size = 0;

	if (!hx_touch_data->diag_cmd)
		print_size = hx_touch_data->touch_info_size;
	else
		print_size = hx_touch_data->touch_all_size;

	for (loop_i = 0; loop_i < print_size; loop_i += 8) {
		if ((loop_i + 7) >= print_size) {
			I("%s: over flow\n", __func__);
			break;
		}
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i, buf[loop_i],
		  loop_i + 1, buf[loop_i + 1]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 2,
		  buf[loop_i + 2], loop_i + 3, buf[loop_i + 3]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 4,
		  buf[loop_i + 4], loop_i + 5, buf[loop_i + 5]);
		I("P %2d = 0x%2.2X P %2d = 0x%2.2X ", loop_i + 6,
		  buf[loop_i + 6], loop_i + 7, buf[loop_i + 7]);
		I("\n");
	}
}
void himax_log_touch_event(int x, int y, int w, int loop_i,
			   uint8_t EN_NoiseFilter, int touched)
{
	if (touched == HX_FINGER_ON)
		I("Finger %d=> X:%d, Y:%d W:%d, Z:%d, F:%d, N:%d\n", loop_i + 1,
		  x, y, w, w, loop_i + 1, EN_NoiseFilter);
	else if (touched == HX_FINGER_LEAVE)
		I("All Finger leave\n");
	else
		I("%s : wrong input!\n", __func__);
}
void himax_log_touch_int_devation(int touched)
{

	if (touched == HX_FINGER_ON) {
		getnstimeofday(&timeStart);

	} else if (touched == HX_FINGER_LEAVE) {
		getnstimeofday(&timeEnd);
		timeDelta.tv_nsec =
			(timeEnd.tv_sec * 1000000000 + timeEnd.tv_nsec) -
			(timeStart.tv_sec * 1000000000 + timeStart.tv_nsec);

		I("Touch latency = %ld us\n", timeDelta.tv_nsec / 1000);
	} else
		I("%s : wrong input!\n", __func__);
}
void himax_log_touch_event_detail(struct himax_ts_data *ts, int x, int y, int w,
				  int loop_i, uint8_t EN_NoiseFilter,
				  int touched, uint16_t old_finger)
{

	if (touched == HX_FINGER_ON) {
		if (old_finger >> loop_i == 0) {
			if (ts->useScreenRes) {
				I("F:%02d Down, X:%d, Y:%d, W:%d, N:%d\n",
				  loop_i + 1, x * ts->widthFactor >> SHIFTBITS,
				  y * ts->heightFactor >> SHIFTBITS, w,
				  EN_NoiseFilter);
			} else {
				I("F:%02d Down, X:%d, Y:%d, W:%d, N:%d\n",
				  loop_i + 1, x, y, w, EN_NoiseFilter);
			}
		}
	} else if (touched ==
		   HX_FINGER_LEAVE) { /* if (old_finger >> loop_i == 1) */

		if (old_finger >> loop_i == 1) {
			if (ts->useScreenRes) {
				I("F:%02d Up, X:%d, Y:%d, N:%d\n",
				  loop_i + 1, ts->pre_finger_data[loop_i][0] *
							      ts->widthFactor >>
						      SHIFTBITS,
				  ts->pre_finger_data[loop_i][1] *
						  ts->heightFactor >>
					  SHIFTBITS,
				  EN_NoiseFilter); /* Last_EN_NoiseFilter */
			} else {
				I("F:%02d Up, X:%d, Y:%d, N:%d\n",
				  loop_i + 1, ts->pre_finger_data[loop_i][0],
				  ts->pre_finger_data[loop_i][1],
				  EN_NoiseFilter); /* Last_EN_NoiseFilter */
			}
		}
	} else {
		I("%s : wrong input!\n", __func__);
	}
}
/* #endif */
static ssize_t himax_diag_arrange_write(struct file *file, const char *buff,
					size_t len, loff_t *pos)
{

	char buf[80] = {0};

	if (len >= 80) {
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
	.owner = THIS_MODULE, .write = himax_diag_arrange_write,
};
static void print_state_info(struct seq_file *s)
{
	/* seq_printf(s, "State_info_2bytes:%3d, */
	/* %3d\n",hx_state_info[0],hx_state_info[1]); */
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

static void himax_diag_arrange_print(struct seq_file *s, int i, int j,
				     int transpose)
{

	if (transpose)
		seq_printf(s, "%6d", diag_mutual[j + i * x_channel]);
	else
		seq_printf(s, "%6d", diag_mutual[i + j * x_channel]);
}

/* ready to print second step which is column*/
static void himax_diag_arrange_inloop(struct seq_file *s, int in_init,
				      int out_init, bool transpose, int j)
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
				seq_printf(s, " %5d\n",
					   diag_self[x_channel - j - 1]);
		}
	} else { /* bit0 = 0 */

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
					   diag_self[y_channel + x_channel -
						     self_cnt]);
				self_cnt++;
			}
		}
	} else { /* bit1 = 0 */

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
	int bit2, bit1, bit0;
	int i;

	/* rotate bit */
	bit2 = g_diag_arr_num >> 2;
	/* reverse Y */
	bit1 = g_diag_arr_num >> 1 & 0x1;
	/* reverse X */
	bit0 = g_diag_arr_num & 0x1;

	if (g_diag_arr_num < 4) {
		for (i = 0; i <= x_channel; i++)
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
		for (i = 0; i <= y_channel; i++)
			seq_printf(s, "%3c%02d%c", '[', i, ']');
		seq_puts(s, "\n");
		himax_diag_arrange_outloop(s, bit2, bit1 * x_channel,
					   bit0 * y_channel);
		seq_printf(s, "%6c", ' ');
		if (bit1 == 1) {
			for (i = x_channel + y_channel - 1; i >= x_channel;
			     i--) {
				seq_printf(s, "%6d", diag_self[i]);
			}
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
	return (void *)((unsigned long)*pos + 1);
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
	size_t ret = 0;
	uint32_t loop_i;
	uint16_t mutual_num, self_num, width;
	int dsram_type = 0;

	dsram_type = g_diag_command / 10;

#ifdef HX_TP_PROC_2T2R
	if (Is_2T2R && (g_diag_command >= 4 && g_diag_command <= 6)) {
		mutual_num = x_channel_2 * y_channel_2;
		self_num = x_channel_2 + y_channel_2; /* don't add KEY_COUNT */
		width = x_channel_2;
		seq_printf(s, "ChannelStart: %4d, %4d\n\n", x_channel_2,
			   y_channel_2);
	} else
#endif
	{
		mutual_num = x_channel * y_channel;
		self_num = x_channel + y_channel; /* don't add KEY_COUNT */
		width = x_channel;
		seq_printf(s, "ChannelStart: %4d, %4d\n\n", x_channel,
			   y_channel);
	}

	/* start to show out the raw data in adb shell */
	if ((g_diag_command >= 1 && g_diag_command <= 3) ||
	    (g_diag_command == 7)) {
		himax_diag_arrange(s);
		seq_puts(s, "\n");
#ifdef HX_EN_SEL_BUTTON
		seq_puts(s, "\n");
		for (loop_i = 0; loop_i < ic_data->HX_BT_NUM; loop_i++)
			seq_printf(s, "%6d",
				   diag_self[ic_data->HX_RX_NUM +
					     ic_data->HX_TX_NUM + loop_i]);
#endif
		seq_puts(s, "ChannelEnd");
		seq_puts(s, "\n");
	}
#ifdef HX_TP_PROC_2T2R
	else if (Is_2T2R && g_diag_command >= 4 && g_diag_command <= 6) {
		for (loop_i = 0; loop_i < mutual_num; loop_i++) {
			seq_printf(s, "%4d", diag_mutual_2[loop_i]);
			if ((loop_i % width) == (width - 1))
				seq_printf(s, " %4d\n",
					   diag_self[width + loop_i / width]);
		}
		seq_puts(s, "\n");
		for (loop_i = 0; loop_i < width; loop_i++) {
			seq_printf(s, "%4d", diag_self[loop_i]);
			if (((loop_i) % width) == (width - 1))
				seq_puts(s, "\n");
		}

#ifdef HX_EN_SEL_BUTTON
		seq_puts(s, "\n");
		for (loop_i = 0; loop_i < HX_BT_NUM; loop_i++)
			seq_printf(s, "%4d",
				   diag_self[ic_data->HX_RX_NUM_2 +
					     ic_data->HX_TX_NUM_2 + loop_i]);
#endif
		seq_puts(s, "ChannelEnd");
		seq_puts(s, "\n");
	}
#endif
	else if (g_diag_command == 8) {
		for (loop_i = 0; loop_i < 128; loop_i++) {
			if ((loop_i % 16) == 0)
				seq_puts(s, "LineStart:");
			seq_printf(s, "%4x", diag_coor[loop_i]);
			if ((loop_i % 16) == 15)
				seq_puts(s, "\n");
		}
	} else if (dsram_type > 0 && dsram_type <= 8) {
		himax_diag_arrange(s);
		seq_puts(s, "ChannelEnd");
		seq_puts(s, "\n");
	}
	if ((g_diag_command >= 1 && g_diag_command <= 7) || dsram_type > 0) {
		/* print Mutual/Slef Maximum and Minimum */
		himax_get_mutual_edge();
		himax_get_self_edge();
		seq_printf(s, "Mutual Max:%3d, Min:%3d\n", g_max_mutual,
			   g_min_mutual);
		seq_printf(s, "Self Max:%3d, Min:%3d\n", g_max_self,
			   g_min_self);

		/* recovery status after print*/
		g_max_mutual = 0;
		g_min_mutual = 255;
		g_max_self = 0;
		g_min_self = 255;
	}
	/*pring state info*/
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
	int i = 0, j = 0;
	unsigned int index = 0;
	int total_size = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 2;
	uint8_t info_data[total_size];
	int32_t *mutual_data;
	int32_t *mutual_data_new;
	int32_t *mutual_data_old;
	int32_t new_data;
	/* 1:common dsram,2:100 frame Max,3:N-(N-1)frame */
	int dsram_type = 0;
	char temp_buf[20];
	char write_buf[total_size * 3];

	mutual_data = NULL;
	mutual_data_new = NULL;
	mutual_data_old = NULL;
	memset(write_buf, '\0', sizeof(write_buf));

	dsram_type = g_diag_command / 10;

	I("%s:Entering g_diag_command=%d\n!", __func__, g_diag_command);

	if (dsram_type == 8) {
		dsram_type = 1;
		I("%s Sorting Mode run sram type1 !\n", __func__);
	}

	himax_burst_enable(private_ts->client, 1);
	if (dsram_type == 1 || dsram_type == 2 || dsram_type == 4) {
		mutual_data = getMutualBuffer();
	} else if (dsram_type == 3) {
		mutual_data = getMutualBuffer();
		mutual_data_new = getMutualNewBuffer();
		mutual_data_old = getMutualOldBuffer();
	}
	himax_get_DSRAM_data(private_ts->client, info_data);

	index = 0;
	for (i = 0; i < ic_data->HX_TX_NUM; i++) {
		for (j = 0; j < ic_data->HX_RX_NUM; j++) {
			new_data = (((int8_t)info_data[index + 1]) << 8 |
				    info_data[index]);
			if (dsram_type == 1 || dsram_type == 4) {
				mutual_data[i * ic_data->HX_RX_NUM + j] =
					new_data;
			} else if (dsram_type == 2) {
				/* Keep max data */
				if (mutual_data[i * ic_data->HX_RX_NUM + j] <
				    new_data)
					mutual_data[i * ic_data->HX_RX_NUM +
						    j] = new_data;
			} else if (dsram_type == 3) {
				/* Cal data for [N]-[N-1] frame */
				mutual_data_new[i * ic_data->HX_RX_NUM + j] =
					new_data;
				mutual_data[i * ic_data->HX_RX_NUM + j] =
					mutual_data_new[i * ic_data->HX_RX_NUM +
							j] -
					mutual_data_old[i * ic_data->HX_RX_NUM +
							j];
			}
			index += 2;
		}
	}
	if (dsram_type == 3) {
		memcpy(mutual_data_old, mutual_data_new,
		       x_channel * y_channel *
			       sizeof(int16_t)); /* copy N data to N-1 array */
	}
	diag_max_cnt++;
	if (dsram_type >= 1 && dsram_type <= 3) {
		queue_delayed_work(private_ts->himax_diag_wq,
				   &private_ts->himax_diag_delay_wrok,
				   1 / 10 * HZ);
	} else if (dsram_type == 4) {
		for (i = 0; i < x_channel * y_channel; i++) {
			memset(temp_buf, '\0', sizeof(temp_buf));
			if (i == (x_channel * y_channel - 1))
				snprintf(temp_buf, sizeof(temp_buf), "%4d\n\n",
					 mutual_data[i]);
			/* I("%s :temp_buf = %s\n",__func__,temp_buf); */
			else if (i % x_channel == (x_channel - 1))
				snprintf(temp_buf, sizeof(temp_buf), "%4d\n",
					 mutual_data[i]);
			else
				snprintf(temp_buf, sizeof(temp_buf), "%4d\t",
					 mutual_data[i]);
			/* I("%s :mutual_data[%d] = %d, temp_buf = */
			/* %s\n",__func__, i, mutual_data[i], temp_buf); */
			strcat(write_buf, temp_buf);
		}

		/* save raw data in file */
		if (!IS_ERR(diag_sram_fn)) {
			I("%s create file and ready to write\n", __func__);
			diag_sram_fn->f_op->write(diag_sram_fn, write_buf,
						  sizeof(write_buf),
						  &diag_sram_fn->f_pos);
			write_counter++;
			if (write_counter < write_max_count)
				queue_delayed_work(
					private_ts->himax_diag_wq,
					&private_ts->himax_diag_delay_wrok,
					1 / 10 * HZ);
			else {
				filp_close(diag_sram_fn, NULL);
				write_counter = 0;
			}
		}
	}
}

static ssize_t himax_diag_write(struct file *filp, const char __user *buff,
				size_t len, loff_t *data)
{
	char messages[80] = {0};
	uint8_t command[2] = {0x00, 0x00};
	uint8_t receive[1];

	/* 0: common , other: dsram*/
	int storage_type = 0;
	/* 1:IIR,2:DC,3:Bank,4:IIR2,5:IIR2_N,6:FIR2,7:Baseline,8:dump coord */
	int rawdata_type = 0;

	memset(receive, 0x00, sizeof(receive));

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	I("%s:g_switch_mode = %d\n", __func__, g_switch_mode);

	if (messages[1] == 0x0A)
		g_diag_command = messages[0] - '0';
	else
		g_diag_command = (messages[0] - '0') * 10 + (messages[1] - '0');

	storage_type = himax_determin_diag_storage(g_diag_command);
	rawdata_type = himax_determin_diag_rawdata(g_diag_command);

	if (g_diag_command > 0 && rawdata_type == 0) {
		I("command=0x%x ,storage=%d, rawdata=%d!Maybe no support!\n",
		  g_diag_command, storage_type, rawdata_type);
		g_diag_command = 0x00;
	} else
		I("command=0x%x ,storag=%d, rawdata=%d\n",
		  g_diag_command, storage_type, rawdata_type);

	memset(diag_mutual, 0x00,
	       x_channel * y_channel * sizeof(int32_t)); /* Set data 0 */
	memset(diag_self, 0x00, sizeof(diag_self));

	if (storage_type == 0 && rawdata_type > 0 && rawdata_type < 8) {
		I("%s,common\n", __func__);
		if (DSRAM_Flag) {
			/* 1. Clear DSRAM flag */
			DSRAM_Flag = false;
			/* 2. Stop DSRAM thread */
			cancel_delayed_work(&private_ts->himax_diag_delay_wrok);
			/* 3. Enable ISR */
			himax_int_enable(private_ts->client->irq, 1);

			/*(4) FW leave sram and return to event stack*/
			himax_return_event_stack(private_ts->client);
		}

		if (g_switch_mode == 2) {
			himax_idle_mode(private_ts->client, 0);
			g_switch_mode =
				himax_switch_mode(private_ts->client, 0);
		}

		if (g_diag_command == 0x04) {
#if defined(HX_TP_PROC_2T2R)
			command[0] = g_diag_command;
#else
			g_diag_command = 0x00;
			command[0] = 0x00;
#endif
		} else
			command[0] = g_diag_command;
		himax_diag_register_set(private_ts->client, command[0]);
	} else if (storage_type > 0 && storage_type < 8 && rawdata_type > 0 &&
		   rawdata_type < 8) {
		I("%s,dsram\n", __func__);

		diag_max_cnt = 0;

		/* 0. set diag flag */
		if (DSRAM_Flag) {
			/* (1) Clear DSRAM flag */
			DSRAM_Flag = false;

			/* (2) Stop DSRAM thread */
			cancel_delayed_work(&private_ts->himax_diag_delay_wrok);
			/* (3) Enable ISR */
			himax_int_enable(private_ts->client->irq, 1);

			/*(4) FW leave sram and return to event stack*/
			himax_return_event_stack(private_ts->client);
		}
		/* close sorting if turn on*/
		if (g_switch_mode == 2) {
			himax_idle_mode(private_ts->client, 0);
			g_switch_mode =
				himax_switch_mode(private_ts->client, 0);
		}

		switch (rawdata_type) {
		case 1:
			command[0] = 0x09; /* IIR */
			break;
		case 2:
			command[0] = 0x0A; /* RAWDATA */
			break;
		case 7:
			command[0] = 0x0B; /* DC */
			break;
		default:
			command[0] = 0x00;
			E("%s: Sram no support this type !\n", __func__);
			break;
		}
		himax_diag_register_set(private_ts->client, command[0]);

		/* 1. Disable ISR */
		himax_int_enable(private_ts->client->irq, 0);

		/* Open file for save raw data log */
		if (storage_type == 4) {
			switch (rawdata_type) {
			case 1:
				diag_sram_fn = filp_open(IIR_DUMP_FILE,
							 O_CREAT | O_WRONLY, 0);
				break;
			case 2:
				diag_sram_fn = filp_open(DC_DUMP_FILE,
							 O_CREAT | O_WRONLY, 0);
				break;
			case 3:
				diag_sram_fn = filp_open(BANK_DUMP_FILE,
							 O_CREAT | O_WRONLY, 0);
				break;
			default:
				I("%s is not true. raw data type is %d\n",
				  __func__, rawdata_type);
			}
		}

		/* 2. Start DSRAM thread */
		queue_delayed_work(private_ts->himax_diag_wq,
				   &private_ts->himax_diag_delay_wrok,
				   2 * HZ / 100);

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
			himax_int_enable(private_ts->client->irq, 1);

			/*(4) FW leave sram and return to event stack*/
			himax_return_event_stack(private_ts->client);
		}

		himax_idle_mode(private_ts->client, 1);
		g_switch_mode = himax_switch_mode(private_ts->client, 1);
		if (g_switch_mode == 2) {
			if (rawdata_type == 1)
				command[0] = 0x09; /* IIR */
			else if (rawdata_type == 2)
				command[0] = 0x0A; /* DC */
			else if (rawdata_type == 7)
				command[0] = 0x08; /* BASLINE */
			else {
				command[0] = 0x00;
				E("%s: not support this command=%d\n",
				  __func__, g_diag_command);
			}
			himax_diag_register_set(private_ts->client, command[0]);
		}

		queue_delayed_work(private_ts->himax_diag_wq,
				   &private_ts->himax_diag_delay_wrok,
				   2 * HZ / 100);
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
			himax_int_enable(private_ts->client->irq, 1);

			/*(4) FW leave sram and return to event stack*/
			himax_return_event_stack(private_ts->client);
		}

		if (g_switch_mode == 2) {
			himax_idle_mode(private_ts->client, 0);
			g_switch_mode =
				himax_switch_mode(private_ts->client, 0);
		}

		if (g_diag_command != 0x00) {

			E("[Himax]error!diag_command=0x%x so reset\n",
			  g_diag_command);
			command[0] = 0x00;
			if (g_diag_command != 0x08)
				g_diag_command = 0x00;
			himax_diag_register_set(private_ts->client, command[0]);
		} else {
			command[0] = 0x00;
			g_diag_command = 0x00;
			himax_diag_register_set(private_ts->client, command[0]);
			I("return to normal g_diag_command=0x%x\n",
			  g_diag_command);
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
#endif

#ifdef HX_TP_PROC_RESET
static ssize_t himax_reset_write(struct file *file, const char *buff,
				 size_t len, loff_t *pos)
{
	char buf_tmp[12];

	if (len >= 12) {
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf_tmp, buff, len))
		return -EFAULT;

#ifdef HX_RST_PIN_FUNC
	if (buf_tmp[0] == '1')
		himax_ic_reset(false, false);
	else if (buf_tmp[0] == '2')
		himax_ic_reset(false, true);
	else if (buf_tmp[0] == '3')
		himax_ic_reset(true, false);
	else if (buf_tmp[0] == '4')
		himax_ic_reset(true, true);
/* else if (buf_tmp[0] == '5') */
/* ESD_HW_REST(); */
#endif
	return len;
}

static const struct file_operations himax_proc_reset_ops = {
	.owner = THIS_MODULE, .write = himax_reset_write,
};
#endif

#ifdef HX_TP_PROC_DEBUG
static ssize_t himax_debug_read(struct file *file, char *buf, size_t len,
				loff_t *pos)
{
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		if (debug_level_cmd == 't') {
			if (fw_update_complete) {
				ret += snprintf(temp_buf + ret, len - ret,
						"FW Update Complete ");
			} else {
				ret += snprintf(temp_buf + ret, len - ret,
						"FW Update Fail ");
			}
		} else if (debug_level_cmd == 'h') {
			if (handshaking_result == 0) {
				ret += snprintf(
					temp_buf + ret, len - ret,
					"Handshaking Result = %d (MCU Running)\n",
					handshaking_result);
			} else if (handshaking_result == 1) {
				ret += snprintf(
					temp_buf + ret, len - ret,
					"Handshaking Result = %d (MCU Stop)\n",
					handshaking_result);
			} else if (handshaking_result == 2) {
				ret += snprintf(
					temp_buf + ret, len - ret,
					"Handshaking Result = %d (I2C Error)\n",
					handshaking_result);
			} else {
				ret += snprintf(temp_buf + ret, len - ret,
						"Handshaking Result = error\n");
			}
		} else if (debug_level_cmd == 'v') {
			ret += snprintf(temp_buf + ret, len - ret,
					"FW_VER = 0x%2.2X\n",
					ic_data->vendor_fw_ver);

			if (IC_TYPE < 8)
				ret += snprintf(temp_buf + ret, len - ret,
						"CONFIG_VER = 0x%2.2X\n",
						ic_data->vendor_config_ver);
			else {
				ret += snprintf(temp_buf + ret, len - ret,
						"TOUCH_VER = 0x%2.2X\n",
						ic_data->vendor_touch_cfg_ver);
				ret += snprintf(
					temp_buf + ret, len - ret,
					"DISPLAY_VER = 0x%2.2X\n",
					ic_data->vendor_display_cfg_ver);
			}
			if (ic_data->vendor_cid_maj_ver < 0 &&
			    ic_data->vendor_cid_min_ver < 0)
				ret += snprintf(temp_buf + ret, len - ret,
						"CID_VER = NULL\n");
			else
				ret += snprintf(
					temp_buf + ret, len - ret,
					"CID_VER = 0x%2.2X\n",
					(ic_data->vendor_cid_maj_ver << 8 |
					 ic_data->vendor_cid_min_ver));

			if (ic_data->vendor_panel_ver < 0)
				ret += snprintf(temp_buf + ret, len - ret,
						"PANEL_VER = NULL\n");
			else
				ret += snprintf(temp_buf + ret, len - ret,
						"PANEL_VER = 0x%2.2X\n",
						ic_data->vendor_panel_ver);

			ret += snprintf(temp_buf + ret, len - ret, "\n");

			ret += snprintf(temp_buf + ret, len - ret,
					"Himax Touch Driver Version:\n");
			ret += snprintf(temp_buf + ret, len - ret, "%s\n",
					HIMAX_DRIVER_VER);
		} else if (debug_level_cmd == 'd') {
			ret += snprintf(temp_buf + ret, len - ret,
					"Himax Touch IC Information :\n");
			switch (IC_TYPE) {
			case HX_85XX_D_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX852xD\n");
				break;
			case HX_85XX_E_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX852xE\n");
				break;
			case HX_85XX_ES_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX852xES\n");
				break;
			case HX_85XX_F_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX852xF\n");
				break;
			case HX_83100A_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83100A\n");
				break;
			case HX_83102A_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83102A\n");
				break;
			case HX_83102B_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83102B\n");
				break;
			case HX_83103A_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83103A\n");
				break;
			case HX_83110A_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83110A\n");
				break;
			case HX_83110B_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83110B\n");
				break;
			case HX_83111B_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83111B\n");
				break;
			case HX_83112A_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83112A\n");
				break;
			case HX_83112B_SERIES_PWON:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type : HX83112B\n");
				break;
			default:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Type error.\n");
			}
			switch (IC_CHECKSUM) {
			case HX_TP_BIN_CHECKSUM_SW:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Checksum : SW\n");
				break;
			case HX_TP_BIN_CHECKSUM_HW:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Checksum : HW\n");
				break;
			case HX_TP_BIN_CHECKSUM_CRC:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Checksum : CRC\n");
				break;
			default:
				ret += snprintf(temp_buf + ret, len - ret,
						"IC Checksum error.\n");
			}

			if (ic_data->HX_INT_IS_EDGE) {
				ret += snprintf(
					temp_buf + ret, len - ret,
					"Driver register Interrupt : EDGE TIRGGER\n");
			} else {
				ret += snprintf(
					temp_buf + ret, len - ret,
					"Driver register Interrupt : LEVEL TRIGGER\n");
			}
			if (private_ts->protocol_type == PROTOCOL_TYPE_A) {
				ret += snprintf(temp_buf + ret, len - ret,
						"Protocol : TYPE_A\n");
			} else {
				ret += snprintf(temp_buf + ret, len - ret,
						"Protocol : TYPE_B\n");
			}
			ret += snprintf(temp_buf + ret, len - ret,
					"RX Num : %d\n", ic_data->HX_RX_NUM);
			ret += snprintf(temp_buf + ret, len - ret,
					"TX Num : %d\n", ic_data->HX_TX_NUM);
			ret += snprintf(temp_buf + ret, len - ret,
					"BT Num : %d\n", ic_data->HX_BT_NUM);
			ret += snprintf(temp_buf + ret, len - ret,
					"X Resolution : %d\n",
					ic_data->HX_X_RES);
			ret += snprintf(temp_buf + ret, len - ret,
					"Y Resolution : %d\n",
					ic_data->HX_Y_RES);
			ret += snprintf(temp_buf + ret, len - ret,
					"Max Point : %d\n", ic_data->HX_MAX_PT);
			ret += snprintf(temp_buf + ret, len - ret,
					"XY reverse : %d\n",
					ic_data->HX_XY_REVERSE);
#ifdef HX_TP_PROC_2T2R
			if (Is_2T2R) {
				ret += snprintf(temp_buf + ret, len - ret,
						"2T2R panel\n");
				ret += snprintf(temp_buf + ret, len - ret,
						"RX Num_2 : %d\n", HX_RX_NUM_2);
				ret += snprintf(temp_buf + ret, len - ret,
						"TX Num_2 : %d\n", HX_TX_NUM_2);
			}
#endif
		} else if (debug_level_cmd == 'i') {
			if (himax_read_i2c_status(private_ts->client))
				ret += snprintf(temp_buf + ret, len - ret,
						"I2C communication is bad.\n");
			else
				ret += snprintf(temp_buf + ret, len - ret,
						"I2C communication is good.\n");
		} else if (debug_level_cmd == 'n') {
			if (himax_read_ic_trigger_type(private_ts->client) ==
			    1) /* Edgd = 1, Level = 0 */
				ret += snprintf(
					temp_buf + ret, len - ret,
					"IC Interrupt type is edge trigger.\n");
			else if (himax_read_ic_trigger_type(
					 private_ts->client) == 0)
				ret += snprintf(
					temp_buf + ret, len - ret,
					"IC Interrupt type is level trigger.\n");
			else
				ret += snprintf(temp_buf + ret, len - ret,
						"Unknown IC trigger type.\n");
			if (ic_data->HX_INT_IS_EDGE)
				ret += snprintf(
					temp_buf + ret, len - ret,
					"Driver register Interrupt : EDGE TIRGGER\n");
			else
				ret += snprintf(
					temp_buf + ret, len - ret,
					"Driver register Interrupt : LEVEL TRIGGER\n");
		}
#if defined(HX_CHIP_STATUS_MONITOR)
		else if (debug_level_cmd == 'c') {
			ret += snprintf(
				temp_buf + ret, len - ret, "chip_monitor :%d\n",
				g_chip_monitor_data->HX_CHIP_MONITOR_EN);
		}
#endif
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static ssize_t himax_debug_write(struct file *file, const char *buff,
				 size_t len, loff_t *pos)
{
	int result = 0;
	char fileName[128];
	char buf[80] = {0};
	int fw_type = 0;
	const struct firmware *fw = NULL;

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == 'h') { /* handshaking */

		debug_level_cmd = buf[0];

		himax_int_enable(private_ts->client->irq, 0);

		handshaking_result = himax_hand_shaking(
			private_ts->client); /* 0:Running, 1:Stop, 2:I2C Fail */

		himax_int_enable(private_ts->client->irq, 1);

		return len;
	}

	else if (buf[0] == 'v') { /* firmware version */

		himax_int_enable(private_ts->client->irq, 0);
#ifdef HX_RST_PIN_FUNC
		himax_ic_reset(false, false);
#endif
		debug_level_cmd = buf[0];
		himax_read_FW_ver(private_ts->client);
#ifdef HX_RST_PIN_FUNC
		himax_ic_reset(true, false);
#endif
		himax_int_enable(private_ts->client->irq, 1);
		return len;
	}

	else if (buf[0] == 'd') { /* ic information */

		debug_level_cmd = buf[0];
		return len;
	}

	else if (buf[0] == 't') {

		himax_int_enable(private_ts->client->irq, 0);

#ifdef HX_CHIP_STATUS_MONITOR
		g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
		g_chip_monitor_data->HX_CHIP_MONITOR_EN = 0;
		cancel_delayed_work_sync(&private_ts->himax_chip_monitor);
#endif

		debug_level_cmd = buf[0];
		fw_update_complete = false;

		memset(fileName, 0, 128);
		/* parse the file name */
		snprintf(fileName, len - 2, "%s", &buf[2]);
		I("%s: upgrade from file(%s) start!\n", __func__, fileName);
		result = request_firmware(&fw, fileName, private_ts->dev);
		if (result < 0) {
			I("fail to request_firmware fwpath: %s (ret:%d)\n",
			  fileName, result);
			return result;
		}
		I("%s: FW image: %02X, %02X, %02X, %02X\n", __func__,
		  fw->data[0], fw->data[1], fw->data[2], fw->data[3]);

		fw_type = (fw->size) / 1024;
		/* start to upgrade */
		himax_int_enable(private_ts->client->irq, 0);
		I("Now FW size is : %dk\n", fw_type);
		switch (fw_type) {
		case 32:
			if (fts_ctpm_fw_upgrade_with_sys_fs_32k(
				    private_ts->client,
				    (unsigned char *)fw->data, fw->size,
				    false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = true;
			}
			break;
		case 60:
			if (fts_ctpm_fw_upgrade_with_sys_fs_60k(
				    private_ts->client,
				    (unsigned char *)fw->data, fw->size,
				    false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = true;
			}
			break;
		case 64:
			if (fts_ctpm_fw_upgrade_with_sys_fs_64k(
				    private_ts->client,
				    (unsigned char *)fw->data, fw->size,
				    false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = true;
			}
			break;
		case 124:
			if (fts_ctpm_fw_upgrade_with_sys_fs_124k(
				    private_ts->client,
				    (unsigned char *)fw->data, fw->size,
				    false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = true;
			}
			break;
		case 128:
			if (fts_ctpm_fw_upgrade_with_sys_fs_128k(
				    private_ts->client,
				    (unsigned char *)fw->data, fw->size,
				    false) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__,
				  __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__,
				  __LINE__);
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
		/* return count; */
	} else if (buf[0] == 'i' && buf[1] == '2' &&
		   buf[2] == 'c') { /* i2c commutation */

		debug_level_cmd = 'i';
		return len;
	}

	else if (buf[0] == 'i' && buf[1] == 'n' &&
		 buf[2] == 't') { /* INT trigger */

		debug_level_cmd = 'n';
		return len;
	}
#if defined(HX_CHIP_STATUS_MONITOR)
	else if (buf[0] == 'c') {
		debug_level_cmd = buf[0];
		g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
		g_chip_monitor_data->HX_CHIP_MONITOR_EN = 0;
		cancel_delayed_work_sync(&private_ts->himax_chip_monitor);
		return len;
	}
#endif
#ifdef HX_ZERO_FLASH
	else if (buf[0] == 'z') {
		himax_0f_operation_check();
		return len;
	} else if (buf[0] == 'p') {
		I("NOW debug echo r!\n");
		/* himax_program_sram(); */
		private_ts->himax_0f_update_wq = create_singlethread_workqueue(
			"HMX_update_0f_request_write");
		if (!private_ts->himax_0f_update_wq)
			E(" allocate syn_update_wq failed\n");

		INIT_DELAYED_WORK(&private_ts->work_0f_update,
				  himax_0f_operation);
		queue_delayed_work(private_ts->himax_0f_update_wq,
				   &private_ts->work_0f_update,
				   msecs_to_jiffies(100));
		return len;
	} else if (buf[0] == 'x') {
		himax_sys_reset();
		return len;
	}
#endif
	/* others,do nothing */
	else {
		debug_level_cmd = 0;
		return len;
	}

firmware_upgrade_done:
	himax_read_FW_ver(private_ts->client);
	himax_touch_information(private_ts->client);

#ifdef HX_RST_PIN_FUNC
	himax_ic_reset(true, false);
#else
	himax_sense_on(private_ts->client, 0);
#endif
	himax_int_enable(private_ts->client->irq, 1);

#ifdef HX_CHIP_STATUS_MONITOR
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 1;
	queue_delayed_work(private_ts->himax_chip_monitor_wq,
			   &private_ts->himax_chip_monitor,
			   g_chip_monitor_data->HX_POLLING_TIMES * HZ);
#endif

	/* todo himax_chip->tp_firmware_upgrade_proceed = 0; */
	/* todo himax_chip->suspend_state = 0; */
	/* todo enable_irq(himax_chip->irq); */
	return len;
}

static const struct file_operations himax_proc_debug_ops = {
	.owner = THIS_MODULE,
	.read = himax_debug_read,
	.write = himax_debug_write,
};

static ssize_t himax_proc_FW_debug_read(struct file *file, char *buf,
					size_t len, loff_t *pos)
{
	int ret = 0;
	uint8_t loop_i = 0;
	uint8_t tmp_data[64];
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		cmd_set[0] = 0x01;
		if (himax_read_FW_status(cmd_set, tmp_data) == NO_ERR) {
			ret += snprintf(temp_buf + ret, len - ret,
					"0x%02X%02X%02X%02X :\t", cmd_set[5],
					cmd_set[4], cmd_set[3], cmd_set[2]);
			for (loop_i = 0; loop_i < cmd_set[1]; loop_i++) {
				ret += snprintf(temp_buf + ret, len - ret,
						"%5d\t", tmp_data[loop_i]);
			}
			ret += snprintf(temp_buf + ret, len - ret, "\n");
		}
		cmd_set[0] = 0x02;
		if (himax_read_FW_status(cmd_set, tmp_data) == NO_ERR) {
			for (loop_i = 0; loop_i < cmd_set[1];
			     loop_i = loop_i + 2) {
				if ((loop_i % 16) == 0)
					ret += snprintf(
						temp_buf + ret, len - ret,
						"0x%02X%02X%02X%02X :\t",
						cmd_set[5], cmd_set[4],
						cmd_set[3] + (((cmd_set[2] +
								loop_i) >>
							       8) &
							      0xFF),
						(cmd_set[2] + loop_i) & 0xFF);
				ret += snprintf(
					temp_buf + ret, len - ret, "%5d\t",
					tmp_data[loop_i] +
						(tmp_data[loop_i + 1] << 8));
				if ((loop_i % 16) == 14)
					ret += snprintf(temp_buf + ret,
							len - ret, "\n");
			}
		}
		ret += snprintf(temp_buf + ret, len - ret, "\n");
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static const struct file_operations himax_proc_fw_debug_ops = {
	.owner = THIS_MODULE, .read = himax_proc_FW_debug_read,
};

static ssize_t himax_proc_DD_debug_read(struct file *file, char *buf,
					size_t len, loff_t *pos)
{
	int ret = 0;
	uint8_t tmp_data[64];
	uint8_t loop_i = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		if (mutual_set_flag == 1) {
			if (himax_read_DD_status(cmd_set, tmp_data) == NO_ERR) {
				for (loop_i = 0; loop_i < cmd_set[0];
				     loop_i++) {
					if ((loop_i % 8) == 0)
						ret += snprintf(temp_buf + ret,
								len - ret,
								"0x%02X : ",
								loop_i);
					ret += snprintf(temp_buf + ret,
							len - ret, "0x%02X ",
							tmp_data[loop_i]);
					if ((loop_i % 8) == 7)
						ret += snprintf(temp_buf + ret,
								len - ret,
								"\n");
				}
			}
		}
		/* else */
		ret += snprintf(temp_buf + ret, len - ret, "\n");
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static ssize_t himax_proc_DD_debug_write(struct file *file, const char *buff,
					 size_t len, loff_t *pos)
{
	uint8_t i = 0;
	uint8_t cnt = 2;
	unsigned long result = 0;
	char buf_tmp[20];
	char buf_tmp2[4];

	if (len >= 20) {
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
				I("fail in cnt = %d, buf_tmp2 = %s",
				  cnt, buf_tmp2);
			cnt--;
		}
		I("cmd_set[2] = %02X, cmd_set[1] = %02X, cmd_set[0] = %02X\n",
		  cmd_set[2], cmd_set[1], cmd_set[0]);
	} else
		mutual_set_flag = 0;

	return len;
}

static const struct file_operations himax_proc_dd_debug_ops = {
	.owner = THIS_MODULE,
	.read = himax_proc_DD_debug_read,
	.write = himax_proc_DD_debug_write,
};

#endif

#ifdef HX_TP_PROC_FLASH_DUMP

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
	memset(flash_buffer, 0x00, Flash_Size);
}

void setSysOperation(uint8_t operation)
{
	sys_operation = operation;
}

void setFlashDumpProgress(uint8_t progress)
{
	flash_progress = progress;
	/* I("setFlashDumpProgress : progress = %d ,flash_progress = %d */
	/* \n",progress,flash_progress); */
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
}

static ssize_t himax_proc_flash_read(struct file *file, char *buf, size_t len,
				     loff_t *pos)
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
	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (local_flash_fail) {
			ret += snprintf(temp_buf + ret, len - ret,
					"FlashStart:Fail\n");
			ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
			ret += snprintf(temp_buf + ret, len - ret, "\n");
			if (copy_to_user(buf, temp_buf, len))
				I("%s,here:%d\n", __func__, __LINE__);
			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
			return ret;
		}

		if (!local_flash_complete) {
			ret += snprintf(temp_buf + ret, len - ret,
					"FlashStart:Ongoing:0x%2.2x\n",
					flash_progress);
			ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
			ret += snprintf(temp_buf + ret, len - ret, "\n");
			if (copy_to_user(buf, temp_buf, len))
				I("%s,here:%d\n", __func__, __LINE__);
			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
			return ret;
		}

		if (local_flash_command == 1 && local_flash_complete) {
			ret += snprintf(temp_buf + ret, len - ret,
					"FlashStart:Complete\n");
			ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
			ret += snprintf(temp_buf + ret, len - ret, "\n");
			if (copy_to_user(buf, temp_buf, len))
				I("%s,here:%d\n", __func__, __LINE__);
			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
			return ret;
		}

		if (local_flash_command == 3 && local_flash_complete) {
			ret += snprintf(temp_buf + ret, len - ret,
					"FlashStart:\n");
			for (loop_i = 0; loop_i < 128; loop_i++) {
				ret += snprintf(temp_buf + ret, len - ret,
						"x%2.2x", flash_buffer[loop_i]);
				if ((loop_i % 16) == 15) {
					ret += snprintf(temp_buf + ret,
							len - ret, "\n");
				}
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

		ret += snprintf(temp_buf + ret, len - ret, "FlashStart:%2.2x\n",
				local_flash_read_step);

		for (loop_i = 0; loop_i < 1024; loop_i++) {
			ret += snprintf(
				temp_buf + ret, len - ret, "x%2.2X",
				flash_buffer[local_flash_read_step * 1024 +
					     loop_i]);

			if ((loop_i % 16) == 15) {
				ret += snprintf(temp_buf + ret, len - ret,
						"\n");
			}
		}

		ret += snprintf(temp_buf + ret, len - ret, "FlashEnd");
		ret += snprintf(temp_buf + ret, len - ret, "\n");
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static ssize_t himax_proc_flash_write(struct file *file, const char *buff,
				      size_t len, loff_t *pos)
{
	char buf_tmp[6];
	unsigned long result = 0;
	char buf[80] = {0};

	if (len >= 80) {
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
	} else if (buf[0] == '1') /* 1_32,1_60,1_64,1_24,1_28 for flash size */
				  /* 32k,60k,64k,124k,128k */
	{
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
	} else if (buf[0] == '2') /* 2_32,2_60,2_64,2_24,2_28 for flash size */
				  /* 32k,60k,64k,124k,128k */
	{
		setSysOperation(1);
		setFlashCommand(2);
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

	himax_int_enable(private_ts->client->irq, 0);
	setFlashDumpGoing(true);

	/* sector = getFlashDumpSector(); */
	/* page = getFlashDumpPage(); */

	local_flash_command = getFlashCommand();

	msleep(100);

	I("%s: local_flash_command = %d enter.\n", __func__,
	  local_flash_command);

	if ((local_flash_command == 1 || local_flash_command == 2) ||
	    (local_flash_command == 0x0F)) {
		himax_flash_dump_func(private_ts->client, local_flash_command,
				      Flash_Size, flash_buffer);
	}

	I("Complete~~~~~~~~~~~~~~~~~~~~~~~\n");

	if (local_flash_command == 2) {
		struct file *fn;

		fn = filp_open(FLASH_DUMP_FILE, O_CREAT | O_WRONLY, 0);
		if (!IS_ERR(fn)) {
			I("%s create file and ready to write\n", __func__);
			fn->f_op->write(fn, flash_buffer,
					Flash_Size * sizeof(uint8_t),
					&fn->f_pos);
			filp_close(fn, NULL);
		}
	}

	himax_int_enable(private_ts->client->irq, 1);
	setFlashDumpGoing(false);

	setFlashDumpComplete(1);
	setSysOperation(0);
}

#endif

#ifdef HX_TP_PROC_SELF_TEST
static ssize_t himax_self_test_read(struct file *file, char *buf, size_t len,
				    loff_t *pos)
{
	int val = 0x00;
	ssize_t ret = 0;
	char *temp_buf;

	I("%s: enter, %d\n", __func__, __LINE__);
	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		himax_int_enable(private_ts->client->irq, 0); /* disable irq */
		g_self_test_entered = 1;
		val = himax_chip_self_test(private_ts->client);
#ifdef HX_ESD_RECOVERY
		HX_ESD_RESET_ACTIVATE = 1;
#endif
		himax_int_enable(private_ts->client->irq, 1); /* enable irq */

		if (val == 0x01) {
			ret += snprintf(temp_buf + ret, len - ret,
					"Self_Test Pass\n");
		} else {
			ret += snprintf(temp_buf + ret, len - ret,
					"Self_Test Fail\n");
		}
		g_self_test_entered = 0;
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static const struct file_operations himax_proc_self_test_ops = {
	.owner = THIS_MODULE, .read = himax_self_test_read,
};
#endif

#ifdef HX_TP_PROC_SENSE_ON_OFF
static ssize_t himax_sense_on_off_write(struct file *file, const char *buff,
					size_t len, loff_t *pos)
{
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0') {
		himax_sense_off(private_ts->client);
		I("Sense off\n");
	} else if (buf[0] == '1') {
		if (buf[1] == 's') {
			himax_sense_on(private_ts->client, 0x00);
			I("Sense on re-map on, run sram\n");
		} else {
			himax_sense_on(private_ts->client, 0x01);
			I("Sense on re-map off, run flash\n");
		}
	} else {
		I("Do nothing\n");
	}
	return len;
}

static const struct file_operations himax_proc_sense_on_off_ops = {
	.owner = THIS_MODULE, .write = himax_sense_on_off_write,
};
#endif

#ifdef HX_HIGH_SENSE
static ssize_t himax_HSEN_read(struct file *file, char *buf, size_t len,
			       loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		ret += snprintf(temp_buf + ret, len - ret, "%d\n",
				ts->HSEN_enable);
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static ssize_t himax_HSEN_write(struct file *file, const char *buff, size_t len,
				loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0')
		ts->HSEN_enable = 0;
	else if (buf[0] == '1')
		ts->HSEN_enable = 1;
	else
		return -EINVAL;

	himax_set_HSEN_enable(ts->client, ts->HSEN_enable, ts->suspended);

	I("%s: HSEN_enable = %d.\n", __func__, ts->HSEN_enable);

	return len;
}

static const struct file_operations himax_proc_HSEN_ops = {
	.owner = THIS_MODULE,
	.read = himax_HSEN_read,
	.write = himax_HSEN_write,
};
#endif

#ifdef HX_SMART_WAKEUP
static ssize_t himax_SMWP_read(struct file *file, char *buf, size_t len,
			       loff_t *pos)
{
	size_t ret = 0;
	struct himax_ts_data *ts = private_ts;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		ret += snprintf(temp_buf + ret, len - ret, "%d\n",
				ts->SMWP_enable);
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;

	return ret;
}

static ssize_t himax_SMWP_write(struct file *file, const char *buff, size_t len,
				loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0')
		ts->SMWP_enable = 0;
	else if (buf[0] == '1')
		ts->SMWP_enable = 1;
	else
		return -EINVAL;

	himax_set_SMWP_enable(ts->client, ts->SMWP_enable, ts->suspended);

	HX_SMWP_EN = ts->SMWP_enable;
	I("%s: SMART_WAKEUP_enable = %d.\n", __func__, HX_SMWP_EN);

	return len;
}

static const struct file_operations himax_proc_SMWP_ops = {
	.owner = THIS_MODULE,
	.read = himax_SMWP_read,
	.write = himax_SMWP_write,
};

static ssize_t himax_GESTURE_read(struct file *file, char *buf, size_t len,
				  loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0;
	ssize_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		for (i = 0; i < 16; i++)
			ret += snprintf(temp_buf + ret, len - ret,
					"ges_en[%d]=%d\n", i,
					ts->gesture_cust_en[i]);
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else {
		HX_PROC_SEND_FLAG = 0;
		ret = 0;
	}
	return ret;
}

static ssize_t himax_GESTURE_write(struct file *file, const char *buff,
				   size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	I("himax_GESTURE_store= %s\n", buf);
	for (i = 0; i < 16; i++) {
		if (buf[i] == '0')
			ts->gesture_cust_en[i] = 0;
		else if (buf[i] == '1')
			ts->gesture_cust_en[i] = 1;
		else
			ts->gesture_cust_en[i] = 0;
		I("gesture en[%d]=%d\n", i, ts->gesture_cust_en[i]);
	}
	return len;
}

static const struct file_operations himax_proc_Gesture_ops = {
	.owner = THIS_MODULE,
	.read = himax_GESTURE_read,
	.write = himax_GESTURE_write,
};
#endif

#ifdef HX_ESD_RECOVERY
static ssize_t himax_esd_cnt_read(struct file *file, char *buf, size_t len,
				  loff_t *pos)
{
	ssize_t ret = 0;
	char *temp_buf;

	I("%s: enter, %d\n", __func__, __LINE__);
	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);

		ret += snprintf(temp_buf + ret, len - ret,
				"EB_cnt = %d, EC_cnt = %d, ED_cnt = %d\n",
				hx_EB_event_flag, hx_EC_event_flag,
				hx_ED_event_flag);
		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static ssize_t himax_esd_cnt_write(struct file *file, const char *buff,
				   size_t len, loff_t *pos)
{
	int i = 0;
	char buf[12] = {0};

	if (len >= 12) {
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

#ifdef HX_TP_PROC_GUEST_INFO

static ssize_t himax_proc_guest_info_read(struct file *file, char *buf,
					  size_t len, loff_t *pos)
{
	int ret = 0;
	/* int loop_i; */
	int i = 0;
	int j = 0;
	char *temp_buf;
	/* int panel_tmp_color; */
	temp_buf = kzalloc(len, GFP_KERNEL);

	I("guest info progress\n");
	if (!HX_PROC_SEND_FLAG) {
		if (himax_guest_info_get_status()) {
			ret += sprintf(temp_buf + ret, "Not Ready\n");
			if (copy_to_user(buf, temp_buf, len))
				I("%s,here:%d\n", __func__, __LINE__);
			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
			return ret;
		}
		for (j = 0; j < 4; j++) {
			ret += sprintf(temp_buf + ret, "g_guest_str%d:\n", j);
			for (i = 0; i < 128; i++) {
				if (i % 16 == 0 && i > 0)
					ret += sprintf(temp_buf + ret, "\n");
				ret += sprintf(temp_buf + ret, "0x%02X\t",
					       g_guest_str[j][i]);
			}
			ret += sprintf(temp_buf + ret, "\n");
			I("str[%d] %s\n", j, g_guest_str[j]);
		}
		ret += sprintf(temp_buf + ret, "g_guest_str8:\n");
		for (i = 0; i < 128; i++) {
			if (i % 16 == 0 && i > 0)
				ret += sprintf(temp_buf + ret, "\n");
			ret += sprintf(temp_buf + ret, "0x%02X\t",
				       g_guest_str[8][i]);
		}
		ret += sprintf(temp_buf + ret, "\n");

		I("str[8] %s\n", g_guest_str[8]);

		ret += sprintf(temp_buf + ret, "g_guest_str9:\n");
		for (i = 0; i < 128; i++) {
			if (i % 16 == 0 && i > 0)
				ret += sprintf(temp_buf + ret, "\n");
			ret += sprintf(temp_buf + ret, "0x%02X\t",
				       g_guest_str[9][i]);
		}
		ret += sprintf(temp_buf + ret, "\n");

		I("str[8] %s\n", g_guest_str[9]);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);
		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else
		HX_PROC_SEND_FLAG = 0;
	return ret;
}

static ssize_t himax_proc_guest_info_write(struct file *file, const char *buff,
					   size_t len, loff_t *pos)
{

	/* unsigned long result = 0; */
	char buf[80] = {0};
	/* int shift = 0; */

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	I("%s: buf = %s\n", __func__, buf);

	if (getSysOperation() == 1) {
		E("%s: PROC is busy , return!\n", __func__);
		return len;
	}

	if (buf[0] == 'r') {
		I("%s,Test to get", __func__);
		queue_work(private_ts->guest_info_wq,
			   &private_ts->guest_info_work);
	}

	return len;
}

static const struct file_operations himax_proc_guest_info_ops = {
	.owner = THIS_MODULE,
	.read = himax_proc_guest_info_read,
	.write = himax_proc_guest_info_write,
};
#endif

int himax_touch_proc_init(void)
{
	himax_touch_proc_dir = proc_mkdir(HIMAX_PROC_TOUCH_FOLDER, NULL);
	if (himax_touch_proc_dir == NULL) {
		E(" %s: himax_touch_proc_dir file create failed!\n", __func__);
		return -ENOMEM;
	}

	himax_proc_debug_level_file =
		proc_create(HIMAX_PROC_DEBUG_LEVEL_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_debug_level_ops);
	if (himax_proc_debug_level_file == NULL) {
		E(" %s: proc debug_level file create failed!\n", __func__);
		goto fail_1;
	}

	himax_proc_vendor_file =
		proc_create(HIMAX_PROC_VENDOR_FILE, (0444),
			    himax_touch_proc_dir, &himax_proc_vendor_ops);
	if (himax_proc_vendor_file == NULL) {
		E(" %s: proc vendor file create failed!\n", __func__);
		goto fail_2;
	}

	himax_proc_attn_file =
		proc_create(HIMAX_PROC_ATTN_FILE, (0444),
			    himax_touch_proc_dir, &himax_proc_attn_ops);
	if (himax_proc_attn_file == NULL) {
		E(" %s: proc attn file create failed!\n", __func__);
		goto fail_3;
	}

	himax_proc_int_en_file =
		proc_create(HIMAX_PROC_INT_EN_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_int_en_ops);
	if (himax_proc_int_en_file == NULL) {
		E(" %s: proc int en file create failed!\n", __func__);
		goto fail_4;
	}

	himax_proc_layout_file =
		proc_create(HIMAX_PROC_LAYOUT_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_layout_ops);
	if (himax_proc_layout_file == NULL) {
		E(" %s: proc layout file create failed!\n", __func__);
		goto fail_5;
	}

#ifdef HX_TP_PROC_RESET
	himax_proc_reset_file =
		proc_create(HIMAX_PROC_RESET_FILE, (0200),
			    himax_touch_proc_dir, &himax_proc_reset_ops);
	if (himax_proc_reset_file == NULL) {
		E(" %s: proc reset file create failed!\n", __func__);
		goto fail_6;
	}
#endif

#ifdef HX_TP_PROC_DIAG
	himax_proc_diag_file =
		proc_create(HIMAX_PROC_DIAG_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_diag_ops);
	if (himax_proc_diag_file == NULL) {
		E(" %s: proc diag file create failed!\n", __func__);
		goto fail_7;
	}
	himax_proc_diag_arrange_file =
		proc_create(HIMAX_PROC_DIAG_ARR_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_diag_arrange_ops);
	if (himax_proc_diag_arrange_file == NULL) {
		E(" %s: proc diag file create failed!\n", __func__);
		goto fail_7_1;
	}
#endif

#ifdef HX_TP_PROC_REGISTER
	himax_proc_register_file =
		proc_create(HIMAX_PROC_REGISTER_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_register_ops);
	if (himax_proc_register_file == NULL) {
		E(" %s: proc register file create failed!\n", __func__);
		goto fail_8;
	}
#endif

#ifdef HX_TP_PROC_DEBUG
	himax_proc_debug_file =
		proc_create(HIMAX_PROC_DEBUG_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_debug_ops);
	if (himax_proc_debug_file == NULL) {
		E(" %s: proc debug file create failed!\n", __func__);
		goto fail_9;
	}

	himax_proc_fw_debug_file =
		proc_create(HIMAX_PROC_FW_DEBUG_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_fw_debug_ops);
	if (himax_proc_fw_debug_file == NULL) {
		E(" %s: proc fw debug file create failed!\n", __func__);
		goto fail_9_1;
	}

	himax_proc_dd_debug_file =
		proc_create(HIMAX_PROC_DD_DEBUG_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_dd_debug_ops);
	if (himax_proc_dd_debug_file == NULL) {
		E(" %s: proc DD debug file create failed!\n", __func__);
		goto fail_9_2;
	}
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
	himax_proc_flash_dump_file =
		proc_create(HIMAX_PROC_FLASH_DUMP_FILE, (0644),
			    himax_touch_proc_dir, &himax_proc_flash_ops);
	if (himax_proc_flash_dump_file == NULL) {
		E(" %s: proc flash dump file create failed!\n", __func__);
		goto fail_10;
	}
#endif

#ifdef HX_TP_PROC_SELF_TEST
	himax_proc_self_test_file =
		proc_create(HIMAX_PROC_SELF_TEST_FILE, (0444),
			    himax_touch_proc_dir, &himax_proc_self_test_ops);
	if (himax_proc_self_test_file == NULL) {
		E(" %s: proc self_test file create failed!\n", __func__);
		goto fail_11;
	}
#endif

#ifdef HX_HIGH_SENSE
	himax_proc_HSEN_file =
		proc_create(HIMAX_PROC_HSEN_FILE, (0666),
			    himax_touch_proc_dir, &himax_proc_HSEN_ops);
	if (himax_proc_HSEN_file == NULL) {
		E(" %s: proc HSEN file create failed!\n", __func__);
		goto fail_13;
	}
#endif

#ifdef HX_SMART_WAKEUP
	himax_proc_SMWP_file =
		proc_create(HIMAX_PROC_SMWP_FILE, (0666),
			    himax_touch_proc_dir, &himax_proc_SMWP_ops);
	if (himax_proc_SMWP_file == NULL) {
		E(" %s: proc SMWP file create failed!\n", __func__);
		goto fail_14;
	}
	himax_proc_GESTURE_file = proc_create(
		HIMAX_PROC_GESTURE_FILE, (0666),
		himax_touch_proc_dir, &himax_proc_Gesture_ops);
	if (himax_proc_GESTURE_file == NULL) {
		E(" %s: proc GESTURE file create failed!\n", __func__);
		goto fail_15;
	}
#endif

#ifdef HX_TP_PROC_SENSE_ON_OFF
	himax_proc_SENSE_ON_OFF_file = proc_create(
		HIMAX_PROC_SENSE_ON_OFF_FILE, (0666),
		himax_touch_proc_dir, &himax_proc_sense_on_off_ops);
	if (himax_proc_SENSE_ON_OFF_file == NULL) {
		E(" %s: proc SENSE_ON_OFF file create failed!\n", __func__);
		goto fail_16;
	}
#endif
#ifdef HX_ESD_RECOVERY
	himax_proc_ESD_cnt_file = proc_create(
		HIMAX_PROC_ESD_CNT_FILE, (0666),
		himax_touch_proc_dir, &himax_proc_esd_cnt_ops);
	if (himax_proc_ESD_cnt_file == NULL) {
		E(" %s: proc ESD cnt file create failed!\n", __func__);
		goto fail_17;
	}
#endif
	himax_proc_CRC_test_file = proc_create(
		HIMAX_PROC_CRC_TEST_FILE, (0666),
		himax_touch_proc_dir, &himax_proc_CRC_test_ops);
	if (himax_proc_CRC_test_file == NULL) {
		E(" %s: proc CRC test file create failed!\n", __func__);
		goto fail_18;
	}
#ifdef HX_TP_PROC_GUEST_INFO
	himax_proc_guest_info_file = proc_create(
		HIMAX_PROC_GUEST_INFO_FILE, (0666),
		himax_touch_proc_dir, &himax_proc_guest_info_ops);
	if (himax_proc_guest_info_file == NULL) {
		E(" %s: proc guest information file create failed!\n",
		  __func__);
		goto fail_19;
	}
#endif
	return 0;

#ifdef HX_TP_PROC_GUEST_INFO
fail_19:
	remove_proc_entry(HIMAX_PROC_GUEST_INFO_FILE, himax_touch_proc_dir);
#endif
fail_18:
#ifdef HX_ESD_RECOVERY
	remove_proc_entry(HIMAX_PROC_ESD_CNT_FILE, himax_touch_proc_dir);
fail_17:
#endif
#ifdef HX_TP_PROC_SENSE_ON_OFF
	remove_proc_entry(HIMAX_PROC_SENSE_ON_OFF_FILE, himax_touch_proc_dir);
fail_16:
#endif
#ifdef HX_SMART_WAKEUP
	remove_proc_entry(HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir);
fail_15:
	remove_proc_entry(HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir);
fail_14:
#endif
#ifdef HX_HIGH_SENSE
	remove_proc_entry(HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir);
fail_13:
#endif
#ifdef HX_TP_PROC_SELF_TEST
	remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, himax_touch_proc_dir);
fail_11:
#endif
#ifdef HX_TP_PROC_FLASH_DUMP
	remove_proc_entry(HIMAX_PROC_FLASH_DUMP_FILE, himax_touch_proc_dir);
fail_10:
#endif
#ifdef HX_TP_PROC_DEBUG
	remove_proc_entry(HIMAX_PROC_DEBUG_FILE, himax_touch_proc_dir);
fail_9:
	remove_proc_entry(HIMAX_PROC_FW_DEBUG_FILE, himax_touch_proc_dir);
fail_9_1:
	remove_proc_entry(HIMAX_PROC_DD_DEBUG_FILE, himax_touch_proc_dir);
fail_9_2:
#endif
#ifdef HX_TP_PROC_REGISTER
	remove_proc_entry(HIMAX_PROC_REGISTER_FILE, himax_touch_proc_dir);
fail_8:
#endif
#ifdef HX_TP_PROC_DIAG
	remove_proc_entry(HIMAX_PROC_DIAG_FILE, himax_touch_proc_dir);
fail_7:
	remove_proc_entry(HIMAX_PROC_DIAG_ARR_FILE, himax_touch_proc_dir);
fail_7_1:
#endif
#ifdef HX_TP_PROC_RESET
	remove_proc_entry(HIMAX_PROC_RESET_FILE, himax_touch_proc_dir);
fail_6:
#endif
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
	remove_proc_entry(HIMAX_PROC_TOUCH_FOLDER, NULL);
	return -ENOMEM;
}

void himax_touch_proc_deinit(void)
{
	remove_proc_entry(HIMAX_PROC_CRC_TEST_FILE, himax_touch_proc_dir);
#ifdef HX_ESD_RECOVERY
	remove_proc_entry(HIMAX_PROC_ESD_CNT_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_SENSE_ON_OFF
	remove_proc_entry(HIMAX_PROC_SENSE_ON_OFF_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_SMART_WAKEUP
	remove_proc_entry(HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_DOT_VIEW
	remove_proc_entry(HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_SELF_TEST
	remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_FLASH_DUMP
	remove_proc_entry(HIMAX_PROC_FLASH_DUMP_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_DEBUG
	remove_proc_entry(HIMAX_PROC_DEBUG_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_FW_DEBUG_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_DD_DEBUG_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_REGISTER
	remove_proc_entry(HIMAX_PROC_REGISTER_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_DIAG
	remove_proc_entry(HIMAX_PROC_DIAG_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_RESET
	remove_proc_entry(HIMAX_PROC_RESET_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_GUEST_INFO
	remove_proc_entry(HIMAX_PROC_GUEST_INFO_FILE, himax_touch_proc_dir);
#endif
	remove_proc_entry(HIMAX_PROC_LAYOUT_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_INT_EN_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_ATTN_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_VENDOR_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_DEBUG_LEVEL_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_TOUCH_FOLDER, NULL);
}
#endif

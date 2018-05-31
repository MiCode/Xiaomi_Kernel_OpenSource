/* Himax Android Driver Sample Code for Himax chipset
*
* Copyright (C) 2015 Himax Corporation.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include "himax_debug.h"
#include "himax_ic.h"

//struct himax_debug_data* debug_data;

extern struct himax_ic_data* ic_data;
extern struct himax_ts_data *private_ts;
extern unsigned char	IC_TYPE;
extern unsigned char	IC_CHECKSUM;
extern int himax_input_register(struct himax_ts_data *ts);
#ifdef QCT
extern irqreturn_t himax_ts_thread(int irq, void *ptr);
#endif
#ifdef MTK
#ifdef CONFIG_OF_TOUCH
extern irqreturn_t tpd_eint_interrupt_handler(int irq, void *desc);
#else
extern void tpd_eint_interrupt_handler(void);
#endif
#endif

#ifdef HX_TP_PROC_DIAG
#ifdef HX_TP_PROC_2T2R
int	HX_RX_NUM_2			= 0;
int	HX_TX_NUM_2			= 0;
#endif
int	touch_monitor_stop_flag		= 0;
int	touch_monitor_stop_limit	= 5;
uint8_t	g_diag_arr_num			= 0;
#endif

#ifdef HX_ESD_WORKAROUND
u8 HX_ESD_RESET_ACTIVATE;
#endif

#ifdef HX_SMART_WAKEUP
bool FAKE_POWER_KEY_SEND;
#endif

//=============================================================================================================
//
//	Segment : Himax PROC Debug Function
//
//=============================================================================================================
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)

static ssize_t himax_vendor_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	char *temp_buf;

	if(!HX_PROC_SEND_FLAG)
	{
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}

		ret += snprintf(temp_buf, len, "%s_FW:%#x_CFG:%#x_SensorId:%#x\n", HIMAX_common_NAME,
		ic_data->vendor_fw_ver, ic_data->vendor_config_ver, ic_data->vendor_sensor_id);
		HX_PROC_SEND_FLAG=1;

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
	}
	else
		HX_PROC_SEND_FLAG=0;

	return ret;
}

static const struct file_operations himax_proc_vendor_ops =
{
	.owner = THIS_MODULE,
	.read = himax_vendor_read,
};

static ssize_t himax_attn_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	ssize_t ret = 0;
	struct himax_ts_data *ts_data;
	char *temp_buf;

	ts_data = private_ts;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}
		ret += snprintf(temp_buf, len, "attn = %x\n", himax_int_gpio_read(ts_data->pdata->gpio_irq));

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	}
	else
		HX_PROC_SEND_FLAG=0;

	return ret;
}


static const struct file_operations himax_proc_attn_ops =
{
	.owner = THIS_MODULE,
	.read = himax_attn_read,
};

static ssize_t himax_int_en_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}
		ret += snprintf(temp_buf, len, "%d ", ts->irq_enabled);
		ret += snprintf(temp_buf+ret, len-ret, "\n");

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	}
	else
		HX_PROC_SEND_FLAG=0;
	return ret;
}

static ssize_t himax_int_en_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf_tmp[12]= {0};
	int value, ret=0;

	if (len >= 12)
	{
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf_tmp, buff, len))
	{
		return -EFAULT;
	}

	if (buf_tmp[0] == '0')
		value = false;
	else if (buf_tmp[0] == '1')
		value = true;
	else
		return -EINVAL;

	if (value) {
		if(ic_data->HX_INT_IS_EDGE)
		{
#ifdef MTK
#ifdef CONFIG_OF_TOUCH
			himax_int_enable(ts->client->irq,1);
#else
			//mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE);
			//mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
			mt_eint_registration(ts->client->irq, EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 1);
#endif
#endif
#ifdef QCT
			ret = request_threaded_irq(ts->client->irq, NULL, himax_ts_thread,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ts->client->name, ts);
#endif
		}
		else
		{
#ifdef MTK
#ifdef CONFIG_OF_TOUCH
			himax_int_enable(ts->client->irq,1);
#else
			//mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE);
			//mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
			mt_eint_registration(ts->client->irq, EINTF_TRIGGER_LOW, tpd_eint_interrupt_handler, 1);
#endif
#endif
#ifdef QCT
			ret = request_threaded_irq(ts->client->irq, NULL, himax_ts_thread,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT, ts->client->name, ts);
#endif
		}
		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
		}
	} else {
		himax_int_enable(ts->client->irq,0);
		free_irq(ts->client->irq, ts);
		ts->irq_enabled = 0;
	}

	return len;
}

static const struct file_operations himax_proc_int_en_ops =
{
	.owner = THIS_MODULE,
	.read = himax_int_en_read,
	.write = himax_int_en_write,
};

static ssize_t himax_layout_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}
		ret += snprintf(temp_buf, len,  "%d ", ts->pdata->abs_x_min);
		ret += snprintf(temp_buf+ret, len-ret, "%d ", ts->pdata->abs_x_max);
		ret += snprintf(temp_buf+ret, len-ret, "%d ", ts->pdata->abs_y_min);
		ret += snprintf(temp_buf+ret, len-ret, "%d ", ts->pdata->abs_y_max);
		ret += snprintf(temp_buf+ret, len-ret, "\n");

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	}
	else
		HX_PROC_SEND_FLAG=0;

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

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}

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
		ts->pdata->abs_x_min=layout[0];
		ts->pdata->abs_x_max=layout[1];
		ts->pdata->abs_y_min=layout[2];
		ts->pdata->abs_y_max=layout[3];
		I("%d, %d, %d, %d\n",ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
		input_unregister_device(ts->input_dev);
		himax_input_register(ts);
	} else
		I("ERR@%d, %d, %d, %d\n",ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
	return len;
}

static const struct file_operations himax_proc_layout_ops =
{
	.owner = THIS_MODULE,
	.read = himax_layout_read,
	.write = himax_layout_write,
};

static ssize_t himax_debug_level_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts_data;
	size_t ret = 0;
	char *temp_buf;
	ts_data = private_ts;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}
		ret += snprintf(temp_buf, len, "%d\n", ts_data->debug_log_level);

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	}
	else
		HX_PROC_SEND_FLAG=0;

	return ret;
}

static ssize_t himax_debug_level_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts;
	char buf_tmp[11];
	int i;
	ts = private_ts;

	if (len >= 12)
	{
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf_tmp, buff, len))
	{
		return -EFAULT;
	}

	ts->debug_log_level = 0;
	for(i=0; i<len-1; i++)
	{
		if( buf_tmp[i]>='0' && buf_tmp[i]<='9' )
			ts->debug_log_level |= (buf_tmp[i]-'0');
		else if( buf_tmp[i]>='A' && buf_tmp[i]<='F' )
			ts->debug_log_level |= (buf_tmp[i]-'A'+10);
		else if( buf_tmp[i]>='a' && buf_tmp[i]<='f' )
			ts->debug_log_level |= (buf_tmp[i]-'a'+10);

		if(i!=len-2)
			ts->debug_log_level <<= 4;
	}

	if (ts->debug_log_level & BIT(3)) {
		if (ts->pdata->screenWidth > 0 && ts->pdata->screenHeight > 0 &&
		 (ts->pdata->abs_x_max - ts->pdata->abs_x_min) > 0 &&
		 (ts->pdata->abs_y_max - ts->pdata->abs_y_min) > 0) {
			ts->widthFactor = (ts->pdata->screenWidth << SHIFTBITS)/(ts->pdata->abs_x_max - ts->pdata->abs_x_min);
			ts->heightFactor = (ts->pdata->screenHeight << SHIFTBITS)/(ts->pdata->abs_y_max - ts->pdata->abs_y_min);
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

static const struct file_operations himax_proc_debug_level_ops =
{
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

	I("himax_register_show: %x,%x,%x,%x\n", register_command[0],register_command[1],register_command[2],register_command[3]);
	if(!HX_PROC_SEND_FLAG)
	{
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}
		himax_register_read(private_ts->client, register_command, 1, data);

		ret += snprintf(temp_buf, len, "command:  %x,%x,%x,%x\n", register_command[0],register_command[1],register_command[2],register_command[3]);

		for (loop_i = 0; loop_i < 128; loop_i++) {
			ret += snprintf(temp_buf+ret, len-ret, "0x%2.2X ", data[loop_i]);
			if ((loop_i % 16) == 15)
				ret += snprintf(temp_buf+ret, len-ret, "\n");
		}
		ret += snprintf(temp_buf+ret, len-ret, "\n");
		HX_PROC_SEND_FLAG=1;

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
	}
	else
		HX_PROC_SEND_FLAG=0;
	return ret;
}

static ssize_t himax_proc_register_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	char buf_tmp[16], length = 0;
	unsigned long result    = 0;
	uint8_t loop_i          = 0;
	uint16_t base           = 5;
	uint8_t write_da[128];
	char buf[80] = {0};

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}

	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	memset(write_da, 0x0, sizeof(write_da));

	I("himax %s \n",buf);

	if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':') {

		if (buf[2] == 'x') {
			memcpy(buf_tmp, buf + 3, 8);
			if (!kstrtoul(buf_tmp, 16, &result))
				{
					register_command[0] = (uint8_t)result;
					register_command[1] = (uint8_t)(result >> 8);
					register_command[2] = (uint8_t)(result >> 16);
					register_command[3] = (uint8_t)(result >> 24);
				}
			base = 11;
			I("CMD: %x,%x,%x,%x\n", register_command[0],register_command[1],register_command[2],register_command[3]);

			for (loop_i = 0; loop_i < 128 && (base+10)<80; loop_i++) {
				if (buf[base] == '\n') {
					if (buf[0] == 'w') {
						himax_register_write(private_ts->client, register_command, 1, write_da);
						I("CMD: %x, %x, %x, %x, len=%d\n", write_da[0], write_da[1],write_da[2],write_da[3],length);
					}
					I("\n");
					return len;
				}
				if (buf[base + 1] == 'x') {
					buf_tmp[10] = '\n';
					buf_tmp[11] = '\0';
					memcpy(buf_tmp, buf + base + 2, 8);
					if (!kstrtoul(buf_tmp, 16, &result)) {
						write_da[loop_i] = (uint8_t)result;
						write_da[loop_i+1] = (uint8_t)(result >> 8);
						write_da[loop_i+2] = (uint8_t)(result >> 16);
						write_da[loop_i+3] = (uint8_t)(result >> 24);
					}
					length+=4;
				}
				base += 10;
			}
		}
	}
	return len;
}

static const struct file_operations himax_proc_register_ops =
{
	.owner = THIS_MODULE,
	.read = himax_proc_register_read,
	.write = himax_proc_register_write,
};
#endif

#ifdef HX_TP_PROC_DIAG
int16_t *getMutualBuffer(void)
{
	return diag_mutual;
}
int16_t *getMutualNewBuffer(void)
{
	return diag_mutual_new;
}
int16_t *getMutualOldBuffer(void)
{
	return diag_mutual_old;
}
int16_t *getSelfBuffer(void)
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
	return diag_command;
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
	diag_mutual = kzalloc(x_channel * y_channel * sizeof(int16_t), GFP_KERNEL);
}
void setMutualNewBuffer(void)
{
	diag_mutual_new = kzalloc(x_channel * y_channel * sizeof(int16_t), GFP_KERNEL);
}
void setMutualOldBuffer(void)
{
	diag_mutual_old = kzalloc(x_channel * y_channel * sizeof(int16_t), GFP_KERNEL);
}

#ifdef HX_TP_PROC_2T2R
int16_t *getMutualBuffer_2(void)
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
	diag_mutual_2 = kzalloc(x_channel_2 * y_channel_2 * sizeof(int16_t), GFP_KERNEL);
}
#endif

static ssize_t himax_diag_arrange_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	//struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}

	g_diag_arr_num = buf[0] - '0';
	I("%s: g_diag_arr_num = %d \n", __func__,g_diag_arr_num);

	return len;
}

static const struct file_operations himax_proc_diag_arrange_ops =
{
	.owner = THIS_MODULE,
	.write = himax_diag_arrange_write,
};

static void himax_diag_arrange_print(struct seq_file *s, int i, int j, int transpose)
{
	if(transpose)
		seq_printf(s, "%6d", diag_mutual[ j + i*x_channel]);
	else
		seq_printf(s, "%6d", diag_mutual[ i + j*x_channel]);
}

static void himax_diag_arrange_inloop(struct seq_file *s, int in_init,bool transpose, int j)
{
	int i;
	int in_max = 0;

	if(transpose)
		in_max = y_channel;
	else
		in_max = x_channel;

	if (in_init > 0)
	{
		for(i = in_init-1;i >= 0;i--)
		{
			himax_diag_arrange_print(s, i, j, transpose);
		}
	}
	else
	{
		for (i = 0; i < in_max; i++)
		{
			himax_diag_arrange_print(s, i, j, transpose);
		}
	}
}

static void himax_diag_arrange_outloop(struct seq_file *s, int transpose, int out_init, int in_init)
{
	int j;
	int out_max = 0;

	if(transpose)
		out_max = x_channel;
	else
		out_max = y_channel;

	if(out_init > 0)
	{
		for(j = out_init-1;j >= 0;j--)
		{
			himax_diag_arrange_inloop(s, in_init, transpose, j);
			seq_printf(s, " %5d\n", diag_self[j]);
		}
	}
	else
	{
		for(j = 0;j < out_max;j++)
		{
			himax_diag_arrange_inloop(s, in_init, transpose, j);
			seq_printf(s, " %5d\n", diag_self[j]);
		}
	}
}

static void himax_diag_arrange(struct seq_file *s)
{
	int bit2,bit1,bit0;
	int i;

	bit2 = g_diag_arr_num >> 2;
	bit1 = g_diag_arr_num >> 1 & 0x1;
	bit0 = g_diag_arr_num & 0x1;

	if (g_diag_arr_num < 4)
	{
		himax_diag_arrange_outloop(s, bit2, bit1 * y_channel, bit0 * x_channel);
		for (i = y_channel; i < x_channel + y_channel; i++) {
			seq_printf(s, "%6d", diag_self[i]);
		}
	}
	else
	{
		himax_diag_arrange_outloop(s, bit2, bit1 * x_channel, bit0 * y_channel);
		for (i = x_channel; i < x_channel + y_channel; i++) {
			seq_printf(s, "%6d", diag_self[i]);
		}
	}
}

static void *himax_diag_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos>=1) return NULL;
	return (void *)((unsigned long) *pos+1);
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
	size_t count = 0;
	int32_t loop_i;//,loop_j
	uint16_t mutual_num, self_num, width;

#ifdef HX_TP_PROC_2T2R
	if(Is_2T2R && diag_command == 4)
	{
		mutual_num	= x_channel_2 * y_channel_2;
		self_num	= x_channel_2 + y_channel_2; //don't add KEY_COUNT
		width		= x_channel_2;
		seq_printf(s, "ChannelStart: %4d, %4d\n\n", x_channel_2, y_channel_2);
	}
	else
#endif
	{
		mutual_num	= x_channel * y_channel;
		self_num	= x_channel + y_channel; //don't add KEY_COUNT
		width		= x_channel;
		seq_printf(s, "ChannelStart: %4d, %4d\n\n", x_channel, y_channel);
	}

	// start to show out the raw data in adb shell
	if (diag_command >= 1 && diag_command <= 6) {
		if (diag_command <= 3) {
			himax_diag_arrange(s);
			seq_printf(s, "\n\n");
#ifdef HX_EN_SEL_BUTTON
			seq_printf(s, "\n");
			for (loop_i = 0; loop_i < HX_BT_NUM; loop_i++)
					seq_printf(s, "%6d", diag_self[HX_RX_NUM + HX_TX_NUM + loop_i]);
#endif
#ifdef HX_TP_PROC_2T2R
		}else if(Is_2T2R && diag_command == 4 ) {
			for (loop_i = 0; loop_i < mutual_num; loop_i++) {
				seq_printf(s, "%4d", diag_mutual_2[loop_i]);
				if ((loop_i % width) == (width - 1))
					seq_printf(s, " %6d\n", diag_self[width + loop_i/width]);
			}
			seq_printf(s, "\n");
			for (loop_i = 0; loop_i < width; loop_i++) {
				seq_printf(s, "%6d", diag_self[loop_i]);
				if (((loop_i) % width) == (width - 1))
					seq_printf(s, "\n");
			}
#ifdef HX_EN_SEL_BUTTON
			seq_printf(s, "\n");
			for (loop_i = 0; loop_i < HX_BT_NUM; loop_i++)
				seq_printf(s, "%4d", diag_self[HX_RX_NUM_2 + HX_TX_NUM_2 + loop_i]);
#endif
#endif
		} else if (diag_command > 4) {
			for (loop_i = 0; loop_i < self_num; loop_i++) {
				seq_printf(s, "%4d", diag_self[loop_i]);
				if (((loop_i - mutual_num) % width) == (width - 1))
					seq_printf(s, "\n");
			}
		} else {
			for (loop_i = 0; loop_i < mutual_num; loop_i++) {
				seq_printf(s, "%4d", diag_mutual[loop_i]);
				if ((loop_i % width) == (width - 1))
					seq_printf(s, "\n");
			}
		}
		seq_printf(s, "ChannelEnd");
		seq_printf(s, "\n");
	} else if (diag_command == 7) {
		for (loop_i = 0; loop_i < 128 ;loop_i++) {
			if ((loop_i % 16) == 0)
				seq_printf(s, "LineStart:");
				seq_printf(s, "%4d", diag_coor[loop_i]);
			if ((loop_i % 16) == 15)
				seq_printf(s, "\n");
		}
	} else if (diag_command == 9 || diag_command == 91 || diag_command == 92){
		himax_diag_arrange(s);
		seq_printf(s, "\n");
	}

	return count;
}
static const struct seq_operations himax_diag_seq_ops =
{
	.start	= himax_diag_seq_start,
	.next	= himax_diag_seq_next,
	.stop	= himax_diag_seq_stop,
	.show	= himax_diag_seq_read,
};
static int himax_diag_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_diag_seq_ops);
};
bool DSRAM_Flag;

//DSRAM thread
void himax_ts_diag_func(void)
{
	int i=0, j=0;
	unsigned int index = 0;
	int total_size = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 2;
	uint8_t  info_data[total_size];
	int16_t *mutual_data     = NULL;
	int16_t *mutual_data_new = NULL;
	int16_t *mutual_data_old = NULL;
	int16_t new_data;

	himax_burst_enable(private_ts->client, 1);
	if(diag_command == 9 || diag_command == 91)
	{
		mutual_data = getMutualBuffer();
	}else if(diag_command == 92){
		mutual_data = getMutualBuffer();
		mutual_data_new = getMutualNewBuffer();
		mutual_data_old = getMutualOldBuffer();
	}
	himax_get_DSRAM_data(private_ts->client, info_data);

	index = 0;
	for (i = 0; i < ic_data->HX_TX_NUM; i++)
	{
		for (j = 0; j < ic_data->HX_RX_NUM; j++)
		{
			new_data = (short)(info_data[index + 1] << 8 | info_data[index]);
			if(diag_command == 9){
				mutual_data[i*ic_data->HX_RX_NUM+j] = new_data;
			}else if(diag_command == 91){ //Keep max data for 100 frame
				if(mutual_data[i * ic_data->HX_RX_NUM + j] < new_data)
				mutual_data[i * ic_data->HX_RX_NUM + j] = new_data;
			}else if(diag_command == 92){ //Cal data for [N]-[N-1] frame
				mutual_data_new[i * ic_data->HX_RX_NUM + j] = new_data;
				mutual_data[i * ic_data->HX_RX_NUM + j] = mutual_data_new[i * ic_data->HX_RX_NUM + j] - mutual_data_old[i * ic_data->HX_RX_NUM + j];
			}
			index += 2;
		}
	}
	if(diag_command == 92){
		memcpy(mutual_data_old,mutual_data_new,x_channel * y_channel * sizeof(int16_t)); //copy N data to N-1 array
	}
	diag_max_cnt++;
	if(diag_command == 9 || diag_command == 92){
		queue_delayed_work(private_ts->himax_diag_wq, &private_ts->himax_diag_delay_wrok, 1/10*HZ);
	}else if(diag_command == 91){
		if(diag_max_cnt > 100) //count for 100 frame
		{
			//Clear DSRAM flag
			DSRAM_Flag = false;

			//Enable ISR
			himax_int_enable(private_ts->client->irq,1);

			//=====================================
			// test result command : 0x8002_0324 ==> 0x00
			//=====================================
			himax_diag_register_set(private_ts->client, 0x00);
		}else{
			queue_delayed_work(private_ts->himax_diag_wq, &private_ts->himax_diag_delay_wrok, 1/10*HZ);
		}
	}
}

static ssize_t himax_diag_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	char messages[80] = {0};

	uint8_t command[2] = {0x00, 0x00};
	uint8_t receive[1];

	memset(receive, 0x00, sizeof(receive));

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(messages, buff, len))
	{
		return -EFAULT;
	}
	if (messages[1] == 0x0A){
		diag_command =messages[0] - '0';
	}else{
		diag_command =(messages[0] - '0')*10 + (messages[1] - '0');
	}

	I("[Himax]diag_command=0x%x\n",diag_command);
	if (diag_command < 0x04){
		if(DSRAM_Flag)
		{
			//1. Clear DSRAM flag
			DSRAM_Flag = false;

			//2. Stop DSRAM thread
			cancel_delayed_work_sync(&private_ts->himax_diag_delay_wrok);

			//3. Enable ISR
			himax_int_enable(private_ts->client->irq,1);
		}
		command[0] = diag_command;
		himax_diag_register_set(private_ts->client, command[0]);
	}
	//coordinate dump start
	else if (diag_command == 0x08)	{
		E("%s: coordinate_dump_file_create error\n", __func__);
	}
	else if (diag_command == 0x09 || diag_command == 91 || diag_command == 92){
		diag_max_cnt = 0;
		memset(diag_mutual, 0x00, x_channel * y_channel * sizeof(int16_t)); //Set data 0 everytime

		//1. Disable ISR
		himax_int_enable(private_ts->client->irq,0);

		//2. Start DSRAM thread
		//himax_diag_register_set(private_ts->client, 0x0A);

		queue_delayed_work(private_ts->himax_diag_wq, &private_ts->himax_diag_delay_wrok, 2*HZ/100);

		I("%s: Start get raw data in DSRAM\n", __func__);

		//3. Set DSRAM flag
		DSRAM_Flag = true;
	}else{
		command[0] = 0x00;
		himax_diag_register_set(private_ts->client, command[0]);
		E("[Himax]Diag command error!diag_command=0x%x\n",diag_command);
	}
	return len;
}

static const struct file_operations himax_proc_diag_ops =
{
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

	if (len >= 12)
	{
		I("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf_tmp, buff, len))
	{
		return -EFAULT;
	}
	//if (buf_tmp[0] == '1')
	//	ESD_HW_REST();

	return len;
}

static const struct file_operations himax_proc_reset_ops =
{
	.owner = THIS_MODULE,
	.write = himax_reset_write,
};
#endif

#ifdef HX_TP_PROC_DEBUG
static ssize_t himax_debug_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	size_t count = 0;
	char *temp_buf;

	if(!HX_PROC_SEND_FLAG)
	{
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf){
			HX_PROC_SEND_FLAG=0;
			return count;
		}

		if (debug_level_cmd == 't')
		{
			if (fw_update_complete)
				count += snprintf(temp_buf+count, len-count, "FW Update Complete ");
			else
			{
				count += snprintf(temp_buf+count, len-count, "FW Update Fail ");
			}
		}
		else if (debug_level_cmd == 'h')
		{
			if (handshaking_result == 0)
			{
				count += snprintf(temp_buf+count, len-count, "Handshaking Result = %d (MCU Running)\n", handshaking_result);
			}
			else if (handshaking_result == 1)
			{
				count += snprintf(temp_buf+count, len-count, "Handshaking Result = %d (MCU Stop)\n", handshaking_result);
			}
			else if (handshaking_result == 2)
			{
				count += snprintf(temp_buf+count, len-count, "Handshaking Result = %d (I2C Error)\n", handshaking_result);
			}
			else
			{
				count += snprintf(temp_buf+count, len-count, "Handshaking Result = error\n");
			}
		}
		else if (debug_level_cmd == 'v')
		{
			count += snprintf(temp_buf+count, len-count, "FW_VER = ");
			count += snprintf(temp_buf+count, len-count, "0x%2.2X\n", ic_data->vendor_fw_ver);
			count += snprintf(temp_buf+count, len-count, "CONFIG_VER = ");
			count += snprintf(temp_buf+count, len-count, "0x%2.2X\n", ic_data->vendor_config_ver);
			count += snprintf(temp_buf+count, len-count, "\n");
		}
		else if (debug_level_cmd == 'd')
		{
			count += snprintf(temp_buf+count, len-count, "Himax Touch IC Information :\n");
			if (IC_TYPE == HX_85XX_D_SERIES_PWON)
			{
				count += snprintf(temp_buf+count, len-count, "IC Type : D\n");
			}
			else if (IC_TYPE == HX_85XX_E_SERIES_PWON)
			{
				count += snprintf(temp_buf+count, len-count, "IC Type : E\n");
			}
			else if (IC_TYPE == HX_85XX_ES_SERIES_PWON)
			{
				count += snprintf(temp_buf+count, len-count, "IC Type : ES\n");
			}
			else if (IC_TYPE == HX_85XX_F_SERIES_PWON)
			{
				count += snprintf(temp_buf+count, len-count, "IC Type : F\n");
			}
			else
			{
				count += snprintf(temp_buf+count, len-count, "IC Type error.\n");
			}

			if (IC_CHECKSUM == HX_TP_BIN_CHECKSUM_SW)
			{
				count += snprintf(temp_buf+count, len-count, "IC Checksum : SW\n");
			}
			else if (IC_CHECKSUM == HX_TP_BIN_CHECKSUM_HW)
			{
				count += snprintf(temp_buf+count, len-count, "IC Checksum : HW\n");
			}
			else if (IC_CHECKSUM == HX_TP_BIN_CHECKSUM_CRC)
			{
				count += snprintf(temp_buf+count, len-count, "IC Checksum : CRC\n");
			}
			else
			{
				count += snprintf(temp_buf+count, len-count, "IC Checksum error.\n");
			}

			if (ic_data->HX_INT_IS_EDGE)
			{
				count += snprintf(temp_buf+count, len-count, "Interrupt : EDGE TIRGGER\n");
			}
			else
			{
				count += snprintf(temp_buf+count, len-count, "Interrupt : LEVEL TRIGGER\n");
			}

			count += snprintf(temp_buf+count, len-count, "RX Num : %d\n", ic_data->HX_RX_NUM);
			count += snprintf(temp_buf+count, len-count, "TX Num : %d\n", ic_data->HX_TX_NUM);
			count += snprintf(temp_buf+count, len-count, "BT Num : %d\n", ic_data->HX_BT_NUM);
			count += snprintf(temp_buf+count, len-count, "X Resolution : %d\n", ic_data->HX_X_RES);
			count += snprintf(temp_buf+count, len-count, "Y Resolution : %d\n", ic_data->HX_Y_RES);
			count += snprintf(temp_buf+count, len-count, "Max Point : %d\n", ic_data->HX_MAX_PT);
			count += snprintf(temp_buf+count, len-count, "XY reverse : %d\n", ic_data->HX_XY_REVERSE);
	#ifdef HX_TP_PROC_2T2R
			if(Is_2T2R)
			{
				count += snprintf(temp_buf+count, len-count, "2T2R panel\n");
				count += snprintf(temp_buf+count, len-count, "RX Num_2 : %d\n", HX_RX_NUM_2);
				count += snprintf(temp_buf+count, len-count, "TX Num_2 : %d\n", HX_TX_NUM_2);
			}
	#endif
		}
		else if (debug_level_cmd == 'i')
		{
			count += snprintf(temp_buf+count, len-count, "Himax Touch Driver Version:\n");
			count += snprintf(temp_buf+count, len-count, "%s\n", HIMAX_DRIVER_VER);
		}
		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG=1;
	}
	else
		HX_PROC_SEND_FLAG=0;
	return count;
}

static ssize_t himax_debug_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	const struct firmware *fw = NULL;
	unsigned char *fw_data = NULL;
	char fileName[128];
	char buf[80] = {0};
	int result;

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}

	if ( buf[0] == 'h') //handshaking
	{
		debug_level_cmd = buf[0];

		himax_int_enable(private_ts->client->irq,0);

		handshaking_result = himax_hand_shaking(private_ts->client); //0:Running, 1:Stop, 2:I2C Fail

		himax_int_enable(private_ts->client->irq,1);

		return len;
	}

	else if ( buf[0] == 'v') //firmware version
	{
		debug_level_cmd = buf[0];
		himax_int_enable(private_ts->client->irq,0);
#ifdef HX_RST_PIN_FUNC
		himax_HW_reset(false,false);
#endif
		himax_read_FW_ver(private_ts->client);
		//himax_check_chip_version();
#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true,false);
#endif
	himax_int_enable(private_ts->client->irq,1);
		return len;
	}

	else if ( buf[0] == 'd') //ic information
	{
		debug_level_cmd = buf[0];
		return len;
	}

	else if ( buf[0] == 'i') //driver version
	{
		debug_level_cmd = buf[0];
		return len;
	}

	else if (buf[0] == 't')
	{

		himax_int_enable(private_ts->client->irq,0);

		debug_level_cmd 		= buf[0];
		fw_update_complete		= false;

		memset(fileName, 0, 128);
		// parse the file name
		snprintf(fileName, len-4, "%s", &buf[4]);
		I("%s: upgrade from file(%s) start!\n", __func__, fileName);
		// open file
		result = request_firmware(&fw, fileName, private_ts->dev);
		if (result) {
			E("%s: open firmware file failed\n", __func__);
			goto firmware_upgrade_done;
			//return len;
		}

		I("%s: FW len %d\n", __func__, fw->size);
		fw_data = (unsigned char *)fw->data;

		I("%s: FW image,len %d: %02X, %02X, %02X, %02X\n", __func__, result, upgrade_fw[0], upgrade_fw[1], upgrade_fw[2], upgrade_fw[3]);

		if (fw_data != NULL)
		{
			// start to upgrade
			himax_int_enable(private_ts->client->irq,0);

			if ((buf[1] == '6') && (buf[2] == '0'))
			{
				if (fts_ctpm_fw_upgrade_with_sys_fs_60k(private_ts->client,upgrade_fw, result, false) == 0)
				{
					E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
					fw_update_complete = false;
				}
				else
				{
					I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
					fw_update_complete = true;
				}
			}
			else if ((buf[1] == '6') && (buf[2] == '4'))
			{
				if (fts_ctpm_fw_upgrade_with_sys_fs_64k(private_ts->client,upgrade_fw, result, false) == 0)
				{
					E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
					fw_update_complete = false;
				}
				else
				{
					I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
					fw_update_complete = true;
				}
			}
			else if ((buf[1] == '2') && (buf[2] == '4'))
			{
				if (fts_ctpm_fw_upgrade_with_sys_fs_124k(private_ts->client,upgrade_fw, result, false) == 0)
				{
					E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
					fw_update_complete = false;
				}
				else
				{
					I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
					fw_update_complete = true;
				}
			}
			else if ((buf[1] == '2') && (buf[2] == '8'))
			{
				if (fts_ctpm_fw_upgrade_with_sys_fs_128k(private_ts->client,upgrade_fw, result, false) == 0)
				{
					E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
					fw_update_complete = false;
				}
				else
				{
					I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
					fw_update_complete = true;
				}
			}
			else
			{
				E("%s: Flash command fail: %d\n", __func__, __LINE__);
				fw_update_complete = false;
			}
			release_firmware(fw);
			goto firmware_upgrade_done;
			//return count;
		}
	}

	firmware_upgrade_done:

#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true,false);
#endif

	himax_sense_on(private_ts->client, 0x01);
	msleep(120);
#ifdef HX_ESD_WORKAROUND
	HX_ESD_RESET_ACTIVATE = 1;
#endif
	himax_int_enable(private_ts->client->irq,1);

	//todo himax_chip->tp_firmware_upgrade_proceed = 0;
	//todo himax_chip->suspend_state = 0;
	//todo enable_irq(himax_chip->irq);
	return len;
}

static const struct file_operations himax_proc_debug_ops =
{
	.owner = THIS_MODULE,
	.read = himax_debug_read,
	.write = himax_debug_write,
};

#endif

#ifdef HX_TP_PROC_FLASH_DUMP

static uint8_t getFlashCommand(void)
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
/*
static uint8_t getFlashDumpSector(void)
{
	return flash_dump_sector;
}

static uint8_t getFlashDumpPage(void)
{
	return flash_dump_page;
}
*/
bool getFlashDumpGoing(void)
{
	return flash_dump_going;
}

void setFlashBuffer(void)
{
	flash_buffer = kzalloc(Flash_Size * sizeof(uint8_t), GFP_KERNEL);
	if (flash_buffer)
		memset(flash_buffer,0x00,Flash_Size);
}

void setSysOperation(uint8_t operation)
{
	sys_operation = operation;
}

static void setFlashDumpProgress(uint8_t progress)
{
	flash_progress = progress;
	//I("setFlashDumpProgress : progress = %d ,flash_progress = %d \n",progress,flash_progress);
}

static void setFlashDumpComplete(uint8_t status)
{
	flash_dump_complete = status;
}

static void setFlashDumpFail(uint8_t fail)
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

static void setFlashDumpSector(uint8_t sector)
{
	flash_dump_sector = sector;
}

static void setFlashDumpPage(uint8_t page)
{
	flash_dump_page = page;
}

static void setFlashDumpGoing(bool going)
{
	flash_dump_going = going;
}

static ssize_t himax_proc_flash_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	int ret = 0;
	int loop_i;
	uint8_t local_flash_read_step=0;
	uint8_t local_flash_complete = 0;
	uint8_t local_flash_progress = 0;
	uint8_t local_flash_command = 0;
	uint8_t local_flash_fail = 0;
	char *temp_buf;
	local_flash_complete = getFlashDumpComplete();
	local_flash_progress = getFlashDumpProgress();
	local_flash_command = getFlashCommand();
	local_flash_fail = getFlashDumpFail();

	I("flash_progress = %d \n",local_flash_progress);
	if(!HX_PROC_SEND_FLAG)
	{
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}

		if (local_flash_fail)
		{
			ret += snprintf(temp_buf+ret, len-ret, "FlashStart:Fail \n");
			ret += snprintf(temp_buf+ret, len-ret, "FlashEnd");
			ret += snprintf(temp_buf+ret, len-ret, "\n");

			if (copy_to_user(buf, temp_buf, len))
			{
				I("%s,here:%d\n", __func__, __LINE__);
			}

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
			return ret;
		}

		if (!local_flash_complete)
		{
			ret += snprintf(temp_buf+ret, len-ret, "FlashStart:Ongoing:0x%2.2x \n",flash_progress);
			ret += snprintf(temp_buf+ret, len-ret, "FlashEnd");
			ret += snprintf(temp_buf+ret, len-ret, "\n");

			if (copy_to_user(buf, temp_buf, len))
			{
				I("%s,here:%d\n", __func__, __LINE__);
			}

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
			return ret;
		}

		if (local_flash_command == 1 && local_flash_complete)
		{
			ret += snprintf(temp_buf+ret, len-ret, "FlashStart:Complete \n");
			ret += snprintf(temp_buf+ret, len-ret, "FlashEnd");
			ret += snprintf(temp_buf+ret, len-ret, "\n");

			if (copy_to_user(buf, temp_buf, len))
			{
				I("%s,here:%d\n", __func__, __LINE__);
			}

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
			return ret;
		}

		if (local_flash_command == 3 && local_flash_complete)
		{
			ret += snprintf(temp_buf+ret, len-ret, "FlashStart: \n");
			for(loop_i = 0; loop_i < 128; loop_i++)
			{
				ret += snprintf(temp_buf+ret, len-ret, "x%2.2x", flash_buffer[loop_i]);
				if ((loop_i % 16) == 15)
				{
					ret += snprintf(temp_buf+ret, len-ret, "\n");
				}
			}
			ret += snprintf(temp_buf+ret, len-ret, "FlashEnd");
			ret += snprintf(temp_buf+ret, len-ret, "\n");

			if (copy_to_user(buf, temp_buf, len))
			{
				I("%s,here:%d\n", __func__, __LINE__);
			}

			kfree(temp_buf);
			HX_PROC_SEND_FLAG = 1;
			return ret;
		}

		//flash command == 0 , report the data
		local_flash_read_step = getFlashReadStep();

		ret += snprintf(temp_buf+ret, len-ret, "FlashStart:%2.2x \n",local_flash_read_step);

		for (loop_i = 0; loop_i < 1024; loop_i++)
		{
			ret += snprintf(temp_buf+ret, len-ret, "x%2.2X", flash_buffer[local_flash_read_step*1024 + loop_i]);

			if ((loop_i % 16) == 15)
			{
				ret += snprintf(temp_buf+ret, len-ret, "\n");
			}
		}

		ret += snprintf(temp_buf+ret, len-ret, "FlashEnd");
		ret += snprintf(temp_buf+ret, len-ret, "\n");
		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	}
	else
		HX_PROC_SEND_FLAG=0;
	return ret;
}

static ssize_t himax_proc_flash_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	char buf_tmp[6];
	unsigned long result = 0;
	uint8_t loop_i = 0;
	int base = 0;
	char buf[80] = {0};

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}
	memset(buf_tmp, 0x0, sizeof(buf_tmp));

	I("%s: buf[0] = %s\n", __func__, buf);

	if (getSysOperation() == 1)
	{
		E("%s: PROC is busy , return!\n", __func__);
		return len;
	}

	if (buf[0] == '0')
	{
		setFlashCommand(0);
		if (buf[1] == ':' && buf[2] == 'x')
		{
			memcpy(buf_tmp, buf + 3, 2);
			I("%s: read_Step = %s\n", __func__, buf_tmp);
			if (!kstrtoul(buf_tmp, 16, &result))
			{
				I("%s: read_Step = %lu \n", __func__, result);
				setFlashReadStep(result);
			}
		}
	}
	else if (buf[0] == '1')// 1_60,1_64,1_24,1_28 for flash size 60k,64k,124k,128k
	{
		setSysOperation(1);
		setFlashCommand(1);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);
		if ((buf[1] == '_' ) && (buf[2] == '6' )){
			if (buf[3] == '0'){
				Flash_Size = FW_SIZE_60k;
			}else if (buf[3] == '4'){
				Flash_Size = FW_SIZE_64k;
			}
		}else if ((buf[1] == '_' ) && (buf[2] == '2' )){
			if (buf[3] == '4'){
				Flash_Size = FW_SIZE_124k;
			}else if (buf[3] == '8'){
				Flash_Size = FW_SIZE_128k;
			}
		}
		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	}
	else if (buf[0] == '2') // 2_60,2_64,2_24,2_28 for flash size 60k,64k,124k,128k
	{
		setSysOperation(1);
		setFlashCommand(2);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);
		if ((buf[1] == '_' ) && (buf[2] == '6' )){
			if (buf[3] == '0'){
				Flash_Size = FW_SIZE_60k;
			}else if (buf[3] == '4'){
				Flash_Size = FW_SIZE_64k;
			}
		}else if ((buf[1] == '_' ) && (buf[2] == '2' )){
			if (buf[3] == '4'){
				Flash_Size = FW_SIZE_124k;
			}else if (buf[3] == '8'){
				Flash_Size = FW_SIZE_128k;
			}
		}
		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	}
	else if (buf[0] == '3')
	{
		setSysOperation(1);
		setFlashCommand(3);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);

		memcpy(buf_tmp, buf + 3, 2);
		if (!kstrtoul(buf_tmp, 16, &result))
		{
			setFlashDumpSector(result);
		}

		memcpy(buf_tmp, buf + 7, 2);
		if (!kstrtoul(buf_tmp, 16, &result))
		{
			setFlashDumpPage(result);
		}

		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	}
	else if (buf[0] == '4')
	{
		I("%s: command 4 enter.\n", __func__);
		setSysOperation(1);
		setFlashCommand(4);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);

		memcpy(buf_tmp, buf + 3, 2);
		if (!kstrtoul(buf_tmp, 16, &result))
		{
			setFlashDumpSector(result);
		}
		else
		{
			E("%s: command 4 , sector error.\n", __func__);
			return len;
		}

		memcpy(buf_tmp, buf + 7, 2);
		if (!kstrtoul(buf_tmp, 16, &result))
		{
			setFlashDumpPage(result);
		}
		else
		{
			E("%s: command 4 , page error.\n", __func__);
			return len;
		}

		base = 11;

		I("=========Himax flash page buffer start=========\n");
		for(loop_i=0;loop_i<128 && base<80;loop_i++)
		{
			memcpy(buf_tmp, buf + base, 2);
			if (!kstrtoul(buf_tmp, 16, &result))
			{
				flash_buffer[loop_i] = result;
				I("%d ",flash_buffer[loop_i]);
				if (loop_i % 16 == 15)
				{
					I("\n");
				}
			}
			base += 3;
		}
		I("=========Himax flash page buffer end=========\n");

		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	}
	return len;
}

static const struct file_operations himax_proc_flash_ops =
{
	.owner = THIS_MODULE,
	.read = himax_proc_flash_read,
	.write = himax_proc_flash_write,
};

void himax_ts_flash_func(void)
{
	uint8_t local_flash_command = 0;

	himax_int_enable(private_ts->client->irq,0);
	setFlashDumpGoing(true);

	//sector = getFlashDumpSector();
	//page = getFlashDumpPage();

	local_flash_command = getFlashCommand();

	msleep(100);

	I("%s: local_flash_command = %d enter.\n", __func__,local_flash_command);

	if ((local_flash_command == 1 || local_flash_command == 2)|| (local_flash_command==0x0F))
	{
		himax_flash_dump_func(private_ts->client, local_flash_command,Flash_Size, flash_buffer);
	}

	I("Complete~~~~~~~~~~~~~~~~~~~~~~~\n");

	if (local_flash_command == 2)
	{
		E("Flash dump failed\n");
	}

	himax_int_enable(private_ts->client->irq,1);
	setFlashDumpGoing(false);

	setFlashDumpComplete(1);
	setSysOperation(0);
	return;

/*	Flash_Dump_i2c_transfer_error:

	himax_int_enable(private_ts->client->irq,1);
	setFlashDumpGoing(false);
	setFlashDumpComplete(0);
	setFlashDumpFail(1);
	setSysOperation(0);
	return;
*/
}

#endif

#ifdef HX_TP_PROC_SELF_TEST
static ssize_t himax_self_test_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	int val=0x00;
	int ret = 0;
	char *temp_buf;

	I("%s: enter, %d \n", __func__, __LINE__);
	if(!HX_PROC_SEND_FLAG)
	{
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}
		himax_int_enable(private_ts->client->irq,0);//disable irq
		val = himax_chip_self_test(private_ts->client);
#ifdef HX_ESD_WORKAROUND
		HX_ESD_RESET_ACTIVATE = 1;
#endif
		himax_int_enable(private_ts->client->irq,1);//enable irq

		if (val == 0x01) {
			ret += snprintf(temp_buf+ret, len-ret, "Self_Test Pass\n");
		} else {
			ret += snprintf(temp_buf+ret, len-ret, "Self_Test Fail\n");
		}

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	}
	else
		HX_PROC_SEND_FLAG=0;
	return ret;
}

/*
static ssize_t himax_chip_self_test_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	char buf_tmp[2];
	unsigned long result = 0;

	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	memcpy(buf_tmp, buf, 2);
	if(!kstrtoul(buf_tmp, 16, &result))
		{
			sel_type = (uint8_t)result;
		}
	I("sel_type = %x \r\n", sel_type);
	return count;
}
*/

static const struct file_operations himax_proc_self_test_ops =
{
	.owner = THIS_MODULE,
	.read = himax_self_test_read,
};
#endif

#ifdef HX_TP_PROC_SENSE_ON_OFF
static ssize_t himax_sense_on_off_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	char buf[80] = {0};

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}

	if(buf[0] == '0')
	{
		himax_sense_off(private_ts->client);
		I("Sense off \n");
	}
	else if(buf[0] == '1')
	{
		if(buf[1] == '1'){
			himax_sense_on(private_ts->client, 0x01);
			I("Sense on re-map off, run flash \n");
		}else if(buf[1] == '0'){
			himax_sense_on(private_ts->client, 0x00);
			I("Sense on re-map on, run sram \n");
		}else{
			I("Do nothing \n");
		}
	}
	else
	{
		I("Do nothing \n");
	}
	return len;
}

static const struct file_operations himax_proc_sense_on_off_ops =
{
	.owner = THIS_MODULE,
	.write = himax_sense_on_off_write,
};
#endif

#ifdef HX_HIGH_SENSE
static ssize_t himax_HSEN_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	char *temp_buf;

	if(!HX_PROC_SEND_FLAG)
	{
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return count;
		}
		count = snprintf(temp_buf, len, "%d\n", ts->HSEN_enable);
		HX_PROC_SEND_FLAG=1;

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
	}
	else
		HX_PROC_SEND_FLAG=0;
	return count;
}

static ssize_t himax_HSEN_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};


	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}

	if (buf[0] == '0'){
		ts->HSEN_enable = 0;
	}
	else if (buf[0] == '1'){
		ts->HSEN_enable = 1;
	}
	else
		return -EINVAL;

	himax_set_HSEN_func(ts->client, ts->HSEN_enable);

	I("%s: HSEN_enable = %d.\n", __func__, ts->HSEN_enable);

	return len;
}

static const struct file_operations himax_proc_HSEN_ops =
{
	.owner = THIS_MODULE,
	.read = himax_HSEN_read,
	.write = himax_HSEN_write,
};
#endif

#ifdef HX_SMART_WAKEUP
static ssize_t himax_SMWP_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	size_t count = 0;
	struct himax_ts_data *ts = private_ts;
	char *temp_buf;

	if(!HX_PROC_SEND_FLAG)
	{
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return count;
		}
		count = snprintf(temp_buf, len, "%d\n", ts->SMWP_enable);

		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG=1;
	}
	else
		HX_PROC_SEND_FLAG=0;

	return count;
}

static ssize_t himax_SMWP_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}


	if (buf[0] == '0')
	{
		ts->SMWP_enable = 0;
	}
	else if (buf[0] == '1')
	{
		ts->SMWP_enable = 1;
	}
	else
		return -EINVAL;

	himax_set_SMWP_func(ts->client, ts->SMWP_enable);
	HX_SMWP_EN = ts->SMWP_enable;
	I("%s: SMART_WAKEUP_enable = %d.\n", __func__, HX_SMWP_EN);

	return len;
}

static const struct file_operations himax_proc_SMWP_ops =
{
	.owner = THIS_MODULE,
	.read = himax_SMWP_read,
	.write = himax_SMWP_write,
};

static ssize_t himax_GESTURE_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i =0;
	int ret = 0;
	char *temp_buf;

	if(!HX_PROC_SEND_FLAG)
	{
		temp_buf = kzalloc(len, GFP_KERNEL);
		if (!temp_buf) {
			HX_PROC_SEND_FLAG=0;
			return ret;
		}
		for(i=0;i<16;i++)
			ret += snprintf(temp_buf+ret, len-ret, "ges_en[%d]=%d\n", i, ts->gesture_cust_en[i]);
		HX_PROC_SEND_FLAG = 1;
		if (copy_to_user(buf, temp_buf, len))
		{
			I("%s,here:%d\n", __func__, __LINE__);
		}

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	}
	else
	{
		HX_PROC_SEND_FLAG = 0;
		ret = 0;
	}
	return ret;
}

static ssize_t himax_GESTURE_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i =0;
	char buf[80] = {0};

	if (len >= 80)
	{
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}

	I("himax_GESTURE_store= %s \n",buf);
	for (i=0;i<16;i++)
	{
		if (buf[i] == '0')
			ts->gesture_cust_en[i]= 0;
		else if (buf[i] == '1')
			ts->gesture_cust_en[i]= 1;
		else
			ts->gesture_cust_en[i]= 0;
		I("gesture en[%d]=%d \n", i, ts->gesture_cust_en[i]);
	}
	return len;
}

static const struct file_operations himax_proc_Gesture_ops =
{
	.owner = THIS_MODULE,
	.read = himax_GESTURE_read,
	.write = himax_GESTURE_write,
};
#endif

int himax_touch_proc_init(void)
{
	himax_touch_proc_dir = proc_mkdir( HIMAX_PROC_TOUCH_FOLDER, NULL);
	if (himax_touch_proc_dir == NULL)
	{
		E(" %s: himax_touch_proc_dir file create failed!\n", __func__);
		return -ENOMEM;
	}

	himax_proc_debug_level_file = proc_create(HIMAX_PROC_DEBUG_LEVEL_FILE, (S_IWUSR|S_IRUGO), himax_touch_proc_dir, &himax_proc_debug_level_ops);
	if (himax_proc_debug_level_file == NULL)
	{
		E(" %s: proc debug_level file create failed!\n", __func__);
		goto fail_1;
	}

	himax_proc_vendor_file = proc_create(HIMAX_PROC_VENDOR_FILE, (S_IRUGO),himax_touch_proc_dir, &himax_proc_vendor_ops);
	if(himax_proc_vendor_file == NULL)
	{
		E(" %s: proc vendor file create failed!\n", __func__);
		goto fail_2;
	}

	himax_proc_attn_file = proc_create(HIMAX_PROC_ATTN_FILE, (S_IRUGO),himax_touch_proc_dir, &himax_proc_attn_ops);
	if(himax_proc_attn_file == NULL)
	{
		E(" %s: proc attn file create failed!\n", __func__);
		goto fail_3;
	}

	himax_proc_int_en_file = proc_create(HIMAX_PROC_INT_EN_FILE, (S_IWUSR|S_IRUGO), himax_touch_proc_dir, &himax_proc_int_en_ops);
	if(himax_proc_int_en_file == NULL)
	{
		E(" %s: proc int en file create failed!\n", __func__);
		goto fail_4;
	}

	himax_proc_layout_file = proc_create(HIMAX_PROC_LAYOUT_FILE, (S_IWUSR|S_IRUGO), himax_touch_proc_dir, &himax_proc_layout_ops);
	if(himax_proc_layout_file == NULL)
	{
		E(" %s: proc layout file create failed!\n", __func__);
		goto fail_5;
	}

#ifdef HX_TP_PROC_RESET
	himax_proc_reset_file = proc_create(HIMAX_PROC_RESET_FILE, (S_IWUSR), himax_touch_proc_dir, &himax_proc_reset_ops);
	if(himax_proc_reset_file == NULL)
	{
		E(" %s: proc reset file create failed!\n", __func__);
		goto fail_6;
	}
#endif

#ifdef HX_TP_PROC_DIAG
	himax_proc_diag_file = proc_create(HIMAX_PROC_DIAG_FILE, (S_IWUSR|S_IRUGO), himax_touch_proc_dir, &himax_proc_diag_ops);
	if(himax_proc_diag_file == NULL)
	{
		E(" %s: proc diag file create failed!\n", __func__);
		goto fail_7;
	}
	himax_proc_diag_arrange_file = proc_create(HIMAX_PROC_DIAG_ARR_FILE, (S_IWUSR|S_IRUGO), himax_touch_proc_dir, &himax_proc_diag_arrange_ops);
	if(himax_proc_diag_arrange_file == NULL)
	{
		E(" %s: proc diag file create failed!\n", __func__);
		goto fail_7_1;
	}
#endif

#ifdef HX_TP_PROC_REGISTER
	himax_proc_register_file = proc_create(HIMAX_PROC_REGISTER_FILE, (S_IWUSR|S_IRUGO), himax_touch_proc_dir, &himax_proc_register_ops);
	if(himax_proc_register_file == NULL)
	{
		E(" %s: proc register file create failed!\n", __func__);
		goto fail_8;
	}
#endif

#ifdef HX_TP_PROC_DEBUG
	himax_proc_debug_file = proc_create(HIMAX_PROC_DEBUG_FILE, (S_IWUSR|S_IRUGO), himax_touch_proc_dir, &himax_proc_debug_ops);
	if(himax_proc_debug_file == NULL)
	{
		E(" %s: proc debug file create failed!\n", __func__);
		goto fail_9;
	}
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
	himax_proc_flash_dump_file = proc_create(HIMAX_PROC_FLASH_DUMP_FILE, (S_IWUSR|S_IRUGO), himax_touch_proc_dir, &himax_proc_flash_ops);
	if(himax_proc_flash_dump_file == NULL)
	{
		E(" %s: proc flash dump file create failed!\n", __func__);
		goto fail_10;
	}
#endif

#ifdef HX_TP_PROC_SELF_TEST
	himax_proc_self_test_file = proc_create(HIMAX_PROC_SELF_TEST_FILE, (S_IRUGO), himax_touch_proc_dir, &himax_proc_self_test_ops);
	if(himax_proc_self_test_file == NULL)
	{
		E(" %s: proc self_test file create failed!\n", __func__);
		goto fail_11;
	}
#endif

#ifdef HX_HIGH_SENSE
	himax_proc_HSEN_file = proc_create(HIMAX_PROC_HSEN_FILE, (S_IWUSR|S_IRUGO|S_IWUGO), himax_touch_proc_dir, &himax_proc_HSEN_ops);
	if(himax_proc_HSEN_file == NULL)
	{
		E(" %s: proc HSEN file create failed!\n", __func__);
		goto fail_12;
	}
#endif

#ifdef HX_SMART_WAKEUP
	himax_proc_SMWP_file = proc_create(HIMAX_PROC_SMWP_FILE, (S_IWUSR|S_IRUGO|S_IWUGO), himax_touch_proc_dir, &himax_proc_SMWP_ops);
	if(himax_proc_SMWP_file == NULL)
	{
		E(" %s: proc SMWP file create failed!\n", __func__);
		goto fail_13;
	}
	himax_proc_GESTURE_file = proc_create(HIMAX_PROC_GESTURE_FILE, (S_IWUSR|S_IRUGO|S_IWUGO), himax_touch_proc_dir, &himax_proc_Gesture_ops);
	if(himax_proc_GESTURE_file == NULL)
	{
		E(" %s: proc GESTURE file create failed!\n", __func__);
		goto fail_14;
	}
#endif

#ifdef HX_TP_PROC_SENSE_ON_OFF
	himax_proc_SENSE_ON_OFF_file = proc_create(HIMAX_PROC_SENSE_ON_OFF_FILE, (S_IWUSR|S_IRUGO|S_IWUGO), himax_touch_proc_dir, &himax_proc_sense_on_off_ops);
	if(himax_proc_SENSE_ON_OFF_file == NULL)
	{
		E(" %s: proc SENSE_ON_OFF file create failed!\n", __func__);
		goto fail_15;
	}
#endif

	return 0 ;

#ifdef HX_TP_PROC_SENSE_ON_OFF
	fail_15:
#endif
#ifdef HX_SMART_WAKEUP
	remove_proc_entry( HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir );
	fail_14:
	remove_proc_entry( HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir );
	fail_13:
#endif
#ifdef HX_HIGH_SENSE
	remove_proc_entry( HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir );
	fail_12:
#endif
#ifdef HX_TP_PROC_SELF_TEST
	remove_proc_entry( HIMAX_PROC_SELF_TEST_FILE, himax_touch_proc_dir );
	fail_11:
#endif
#ifdef HX_TP_PROC_FLASH_DUMP
	remove_proc_entry( HIMAX_PROC_FLASH_DUMP_FILE, himax_touch_proc_dir );
	fail_10:
#endif
#ifdef HX_TP_PROC_DEBUG
	remove_proc_entry( HIMAX_PROC_DEBUG_FILE, himax_touch_proc_dir );
	fail_9:
#endif
#ifdef HX_TP_PROC_REGISTER
	remove_proc_entry( HIMAX_PROC_REGISTER_FILE, himax_touch_proc_dir );
	fail_8:
#endif
#ifdef HX_TP_PROC_DIAG
	remove_proc_entry( HIMAX_PROC_DIAG_FILE, himax_touch_proc_dir );
	fail_7:
	remove_proc_entry( HIMAX_PROC_DIAG_ARR_FILE, himax_touch_proc_dir );
	fail_7_1:
#endif
#ifdef HX_TP_PROC_RESET
	remove_proc_entry( HIMAX_PROC_RESET_FILE, himax_touch_proc_dir );
	fail_6:
#endif
	remove_proc_entry( HIMAX_PROC_LAYOUT_FILE, himax_touch_proc_dir );
	fail_5: remove_proc_entry( HIMAX_PROC_INT_EN_FILE, himax_touch_proc_dir );
	fail_4: remove_proc_entry( HIMAX_PROC_ATTN_FILE, himax_touch_proc_dir );
	fail_3: remove_proc_entry( HIMAX_PROC_VENDOR_FILE, himax_touch_proc_dir );
	fail_2: remove_proc_entry( HIMAX_PROC_DEBUG_LEVEL_FILE, himax_touch_proc_dir );
	fail_1: remove_proc_entry( HIMAX_PROC_TOUCH_FOLDER, NULL );
	return -ENOMEM;
}

void himax_touch_proc_deinit(void)
{
#ifdef HX_TP_PROC_SENSE_ON_OFF
	remove_proc_entry( HIMAX_PROC_SENSE_ON_OFF_FILE, himax_touch_proc_dir );
#endif
#ifdef HX_SMART_WAKEUP
	remove_proc_entry( HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir );
	remove_proc_entry( HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir );
#endif
#ifdef HX_DOT_VIEW
	remove_proc_entry( HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir );
#endif
#ifdef HX_TP_PROC_SELF_TEST
	remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_FLASH_DUMP
	remove_proc_entry(HIMAX_PROC_FLASH_DUMP_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_DEBUG
	remove_proc_entry( HIMAX_PROC_DEBUG_FILE, himax_touch_proc_dir );
#endif
#ifdef HX_TP_PROC_REGISTER
	remove_proc_entry(HIMAX_PROC_REGISTER_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_DIAG
	remove_proc_entry(HIMAX_PROC_DIAG_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_TP_PROC_RESET
	remove_proc_entry( HIMAX_PROC_RESET_FILE, himax_touch_proc_dir );
#endif
	remove_proc_entry( HIMAX_PROC_LAYOUT_FILE, himax_touch_proc_dir );
	remove_proc_entry( HIMAX_PROC_INT_EN_FILE, himax_touch_proc_dir );
	remove_proc_entry( HIMAX_PROC_ATTN_FILE, himax_touch_proc_dir );
	remove_proc_entry( HIMAX_PROC_VENDOR_FILE, himax_touch_proc_dir );
	remove_proc_entry( HIMAX_PROC_DEBUG_LEVEL_FILE, himax_touch_proc_dir );
	remove_proc_entry( HIMAX_PROC_TOUCH_FOLDER, NULL );
}
#endif

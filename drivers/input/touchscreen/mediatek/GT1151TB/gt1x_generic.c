/* drivers/input/touchscreen/gt1x_generic.c
 *
 * 2010 - 2014 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 1.0
 * Revision RecordL:
 *      V1.0:  first release. 2014/09/28.
 *
 */

#include <linux/input.h>

#include "include/gt1x_tpd_common.h"
#include "gt1x_config.h"

#ifdef CONFIG_GTP_PROXIMITY
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif

#ifdef CONFIG_GTP_ICS_SLOT_REPORT
#include <linux/input/mt.h>
#endif

/*******************GLOBAL VARIABLE*********************/
struct i2c_client *gt1x_i2c_client = NULL;
static struct workqueue_struct *gt1x_workqueue;

u8 gt1x_config[GTP_CONFIG_MAX_LENGTH] = { 0 };

u32 gt1x_cfg_length = GTP_CONFIG_MAX_LENGTH;
bool check_flag = false;

enum CHIP_TYPE_T gt1x_chip_type = CHIP_TYPE_GT1X;
struct gt1x_version_info gt1x_version = {
	.product_id = {0},
	.patch_id = 0,
	.mask_id = 0,
	.sensor_id = 0,
	.match_opt = 0
};


#if defined(CONFIG_GTP_WITH_STYLUS) && defined(CONFIG_GTP_HAVE_STYLUS_KEY)
const u16 gt1x_stylus_key_array[] = GTP_STYLUS_KEY_TAB;
#endif

u8 gt1x_clk_buf[6];
u8 gt1x_clk_retries = 0;
u8 gt1x_ref_retries = 0;
u8 gt1x_driver_num = 0;
u8 gt1x_sensor_num = 0;

u8 gt1x_int_type = 0;
u8 gt1x_wakeup_level = 0;
u32 gt1x_abs_x_max = 0;
u32 gt1x_abs_y_max = 0;
u8 gt1x_rawdiff_mode = 0;

u8 gt1x_init_failed = 0;

u8 is_resetting = 0;

static ssize_t gt1x_debug_read_proc(struct file *, char __user *, size_t, loff_t *);
static ssize_t gt1x_debug_write_proc(struct file *, const char __user *, size_t, loff_t *);

static struct proc_dir_entry *gt1x_debug_proc_entry;

static const struct file_operations gt1x_debug_fops = {
	.owner = THIS_MODULE,
	.read = gt1x_debug_read_proc,
	.write = gt1x_debug_write_proc,
};

s32 gt1x_init_debug_node(void)
{
	gt1x_debug_proc_entry = proc_create(GT1X_DEBUG_PROC_FILE, 0660, NULL, &gt1x_debug_fops);
	if (gt1x_debug_proc_entry == NULL) {
		GTP_ERROR("create_proc_entry %s FAILED!", GT1X_DEBUG_PROC_FILE);
		return -1;
	}
	GTP_ERROR("create_proc_entry %s SUCCESS.", GT1X_DEBUG_PROC_FILE);
	return 0;
}

void gt1x_deinit_debug_node(void)
{
	if (gt1x_debug_proc_entry != NULL)
		remove_proc_entry(GT1X_DEBUG_PROC_FILE, NULL);
}

static ssize_t gt1x_debug_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	char *ptr = page;
	char temp_data[GTP_CONFIG_MAX_LENGTH] = { 0 };
	int i;
	ssize_t ret;

	if (*ppos)
		return 0;

	ptr += sprintf(ptr, "==== GT1X default config setting in driver====\n");

	for (i = 0; i < GTP_CONFIG_MAX_LENGTH; i++) {
		ptr += sprintf(ptr, "0x%02X,", gt1x_config[i]);
		if (i % 10 == 9)
			ptr += sprintf(ptr, "\n");
	}

	ptr += sprintf(ptr, "\n");

	ptr += sprintf(ptr, "==== GT1X config read from chip====\n");
	i = gt1x_i2c_read(GTP_REG_CONFIG_DATA, temp_data, GTP_CONFIG_MAX_LENGTH);
	GTP_INFO("I2C TRANSFER: %d", i);
	for (i = 0; i < GTP_CONFIG_MAX_LENGTH; i++) {
		ptr += sprintf(ptr, "0x%02X,", temp_data[i]);

		if (i % 10 == 9)
			ptr += sprintf(ptr, "\n");
	}

	/* Touch PID & VID */
	ptr += sprintf(ptr, "\n");
	ptr += sprintf(ptr, "==== GT1X Version Info ====\n");

	gt1x_i2c_read(GTP_REG_VERSION, temp_data, 12);
	ptr += sprintf(ptr, "ProductID: GT%c%c%c%c\n", temp_data[0], temp_data[1], temp_data[2], temp_data[3]);
	ptr += sprintf(ptr, "PatchID: %02X%02X\n", temp_data[4], temp_data[5]);
	ptr += sprintf(ptr, "MaskID: %02X%02X\n", temp_data[7], temp_data[8]);
	ptr += sprintf(ptr, "SensorID: %02X\n", temp_data[10] & 0x0F);
	ptr += sprintf(ptr, "Driver Num: %02d. Sensor Num: %02d\n", gt1x_driver_num, gt1x_sensor_num);

	*ppos += ptr - page;
	ret = ptr - page;
	return ret;
}

static ssize_t gt1x_debug_write_proc(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	s32 ret = 0;
	u8 buf[GTP_CONFIG_MAX_LENGTH] = { 0 };
	char mode_str[50] = { 0 };
	int mode;
	int cfg_len;
	char arg1[50] = { 0 };
	u8 temp_config[GTP_CONFIG_MAX_LENGTH] = { 0 };

	GTP_DEBUG("write count %ld\n", (unsigned long)count);

	if (count > GTP_CONFIG_MAX_LENGTH) {
		GTP_ERROR("Too much data, buffer size: %d, data:%ld", GTP_CONFIG_MAX_LENGTH, (unsigned long)count);
		return -EFAULT;
	}

	if (copy_from_user(buf, buffer, count)) {
		GTP_ERROR("copy from user fail!");
		return -EFAULT;
	}
	/*send config*/
	if (count == gt1x_cfg_length) {
		memcpy(gt1x_config, buf, count);
		ret = gt1x_send_cfg(gt1x_config, gt1x_cfg_length);
		if (ret < 0) {
			GTP_ERROR("send gt1x_config failed.");
			return -EFAULT;
		}
		gt1x_abs_x_max = (gt1x_config[RESOLUTION_LOC + 1] << 8) + gt1x_config[RESOLUTION_LOC];
		gt1x_abs_y_max = (gt1x_config[RESOLUTION_LOC + 3] << 8) + gt1x_config[RESOLUTION_LOC + 2];
		return count;
	}

	ret = sscanf(buf, "%s %d", (char *)&mode_str, &mode);
	if (ret < 0) {
		GTP_ERROR("Sscanf buf ERROR1");
		return ret;
	}
	/*force clear gt1x_config*/
	if (strcmp(mode_str, "clear_config") == 0) {
		GTP_INFO("Force clear gt1x_config");
		gt1x_send_cmd(GTP_CMD_CLEAR_CFG, 0);
		return count;
	}
	if (strcmp(mode_str, "init") == 0) {
		GTP_INFO("Init panel");
		gt1x_init_panel();
		return count;
	}
	if (strcmp(mode_str, "chip") == 0) {
		GTP_INFO("Get chip type:");
		gt1x_get_chip_type();
		return count;
	}
	if (strcmp(mode_str, "int") == 0) {
		if (mode == 0) {
			GTP_INFO("Disable irq.");
			gt1x_irq_disable();
		} else {
			GTP_INFO("Enable irq.");
			gt1x_irq_enable();
		}
		return count;
	}

	if (strcmp(mode_str, "poweron") == 0) {
		gt1x_power_switch(1);
		return count;
	}

	if (strcmp(mode_str, "poweroff") == 0) {
		gt1x_power_switch(0);
		return count;
	}

	if (strcmp(mode_str, "version") == 0) {
		gt1x_read_version(NULL);
		return count;
	}

	if (strcmp(mode_str, "reset") == 0) {
		gt1x_reset_guitar();
		return count;
	}
#ifdef CONFIG_GTP_CHARGER_SWITCH
	if (strcmp(mode_str, "charger") == 0) {
		gt1x_charger_config(mode);
		return count;
	}
#endif
	ret = sscanf(buf, "%s %s", (char *)&mode_str, (char *)&arg1);
	if (ret < 0) {
		GTP_ERROR("Sscanf buf ERROR2");
		return ret;
	}
	if (strcmp(mode_str, "update") == 0) {
		gt1x_update_firmware(arg1);
		return count;
	}

	if (strcmp(mode_str, "sendconfig") == 0) {
		cfg_len = gt1x_parse_config(arg1, temp_config);
		if (cfg_len < 0)
			return -1;
		gt1x_send_cfg(gt1x_config, gt1x_cfg_length);
		return count;
	}

	return gt1x_debug_proc(buf, count);
}

static u8 ascii2hex(u8 a)
{
	s8 value = 0;

	if (a >= '0' && a <= '9')
		value = a - '0';
	else if (a >= 'A' && a <= 'F')
		value = a - 'A' + 0x0A;
	else if (a >= 'a' && a <= 'f')
		value = a - 'a' + 0x0A;
	else
		value = 0xff;
	return value;
}

int gt1x_parse_config(char *filename, u8 *config)
{
	mm_segment_t old_fs;
	struct file *fp = NULL;
	u8 *buf;
	int i;
	int len;
	int cur_len = -1;
	u8 high, low;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		GTP_ERROR("Open config file error!(file: %s)", filename);
		goto parse_cfg_fail1;
	}
	len = fp->f_op->llseek(fp, 0, SEEK_END);
	if (len > GTP_CONFIG_MAX_LENGTH * 6 || len < GTP_CONFIG_MAX_LENGTH) {
		GTP_ERROR("Config is invalid!(length: %d)", len);
		goto parse_cfg_fail2;
	}
	buf = kzalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		GTP_ERROR("Allocate memory failed!(size: %d)", len);
		goto parse_cfg_fail2;
	}
	fp->f_op->llseek(fp, 0, SEEK_SET);
	if (fp->f_op->read(fp, (char *)buf, len, &fp->f_pos) != len)
		GTP_ERROR("Read %d bytes from file failed!", len);

	GTP_INFO("Parse config file: %s (%d bytes)", filename, len);

	for (i = 0, cur_len = 0; i < len && cur_len < GTP_CONFIG_MAX_LENGTH;) {
		if (buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\n' || buf[i] == ',') {
			i++;
			continue;
		}
		if (buf[i] == '0' && (buf[i + 1] == 'x' || buf[i + 1] == 'X')) {

			high = ascii2hex(buf[i + 2]);
			low = ascii2hex(buf[i + 3]);

			if (high != 0xFF && low != 0xFF) {
				config[cur_len++] = (high << 4) + low;
				i += 4;
				continue;
			}
		}
		GTP_ERROR("Illegal config file!");
		cur_len = -1;
		break;
	}

	if (cur_len < GTP_CONFIG_MIN_LENGTH || config[cur_len - 1] != 0x01) {
		cur_len = -1;
	} else {
		for (i = 0; i < cur_len; i++) {
			if (i % 10 == 0)
				GTP_INFO("\n<<GTP-DBG>>:");
			GTP_INFO("0x%02x,", config[i]);
		}
		GTP_INFO("\n");
	}

	kfree(buf);
 parse_cfg_fail2:
	filp_close(fp, NULL);
 parse_cfg_fail1:
	set_fs(old_fs);

	return cur_len;
}

s32 _do_i2c_read(struct i2c_msg *msgs, u16 addr, u8 *buffer, s32 len)
{
	s32 ret = -1;
	s32 pos = 0;
	s32 data_length = len;
	s32 transfer_length = 0;
	u8 *data = NULL;
	u16 address = addr;
	u8 *addr_buf = NULL;

	addr_buf = kmalloc(GTP_ADDR_LENGTH, GFP_KERNEL);
	if (addr_buf == NULL)
		return ERROR_MEM;
	msgs[0].buf = addr_buf;

	data =
	    kmalloc(IIC_MAX_TRANSFER_SIZE <
			   (len + GTP_ADDR_LENGTH) ? IIC_MAX_TRANSFER_SIZE : (len + GTP_ADDR_LENGTH), GFP_KERNEL);
	if (data == NULL)
		return ERROR_MEM;
	msgs[1].buf = data;

	while (pos != data_length) {
		if ((data_length - pos) > IIC_MAX_TRANSFER_SIZE)
			transfer_length = IIC_MAX_TRANSFER_SIZE;
		else
			transfer_length = data_length - pos;
		msgs[0].buf[0] = (address >> 8) & 0xFF;
		msgs[0].buf[1] = address & 0xFF;
		msgs[1].len = transfer_length;

		ret = i2c_transfer(gt1x_i2c_client->adapter, msgs, 2);
		if (ret != 2) {
			GTP_INFO("I2c Transfer error! (%d)", ret);
			kfree(addr_buf);
			kfree(data);
			return ERROR_IIC;
		}
		memcpy(&buffer[pos], msgs[1].buf, transfer_length);
		pos += transfer_length;
		address += transfer_length;
	}

	kfree(addr_buf);
	kfree(data);
	return 0;
}

s32 _do_i2c_write(struct i2c_msg *msg, u16 addr, u8 *buffer, s32 len)
{
	s32 ret = -1;
	s32 pos = 0;
	s32 data_length = len;
	s32 transfer_length = 0;
	u8 *data = NULL;
	u16 address = addr;

	data =
	    kmalloc(IIC_MAX_TRANSFER_SIZE <
			   (len + GTP_ADDR_LENGTH) ? IIC_MAX_TRANSFER_SIZE : (len + GTP_ADDR_LENGTH), GFP_KERNEL);
	if (data == NULL)
		return ERROR_MEM;

	msg->buf = data;

	while (pos != data_length) {
		if ((data_length - pos) > (IIC_MAX_TRANSFER_SIZE - GTP_ADDR_LENGTH))
			transfer_length = IIC_MAX_TRANSFER_SIZE - GTP_ADDR_LENGTH;
		else
			transfer_length = data_length - pos;

		msg->buf[0] = (address >> 8) & 0xFF;
		msg->buf[1] = address & 0xFF;
		msg->len = transfer_length + GTP_ADDR_LENGTH;
		memcpy(&msg->buf[GTP_ADDR_LENGTH], &buffer[pos], transfer_length);

		ret = i2c_transfer(gt1x_i2c_client->adapter, msg, 1);
		if (ret != 1) {
			GTP_INFO("I2c Transfer error! (%d)", ret);
			kfree(data);
			return ERROR_IIC;
		}
		pos += transfer_length;
		address += transfer_length;
	}

	kfree(data);
	return 0;
}

s32 gt1x_i2c_test(void)
{
	u8 retry = 0;
	s32 ret = -1;
	u32 hw_info = 0;

	GTP_DEBUG_FUNC();

	while (retry++ < 5) {
		ret = gt1x_i2c_read(GTP_REG_HW_INFO, (u8 *) &hw_info, sizeof(hw_info));
		if (!ret) {
			GTP_ERROR("GTP_REG_HW_INFO : %08X", hw_info);
			return ret;
		}

		msleep(20);
		GTP_ERROR("GTP_REG_HW_INFO : %08X", hw_info);
		GTP_ERROR("GTP i2c test failed time %d.", retry);
	}

	return ERROR_RETRY;
}

s32 gt1x_i2c_read_dbl_check(u16 addr, u8 *buffer, s32 len)
{
	u8 buf[16] = { 0 };
	u8 confirm_buf[16] = { 0 };

	if (len > 16) {
		GTP_ERROR("i2c_read_dbl_check length %d is too long, do not beyond %d", len, (int)(sizeof(buf)));
		return ERROR;
	}

	memset(buf, 0xAA, 16);
	gt1x_i2c_read(addr, buf, len);

	msleep(20);

	memset(confirm_buf, 0, 16);
	gt1x_i2c_read(addr, confirm_buf, len);

	if (!memcmp(buf, confirm_buf, len)) {
		memcpy(buffer, confirm_buf, len);
		return 0;
	}
	GTP_ERROR("i2c read 0x%04X, %d bytes, double check failed!", addr, len);
	return ERROR;
}

/**
 * gt1x_get_info - Get information from ic, such as resolution and
 * int trigger type
 * Return    <0: i2c failed, 0: i2c ok
 */
s32 gt1x_get_info(void)
{
	u8 opr_buf[4] = { 0 };
	s32 ret = 0;

	ret = gt1x_i2c_read(GTP_REG_CONFIG_DATA + 1, opr_buf, 4);
	if (ret < 0)
		return ret;

	gt1x_abs_x_max = (opr_buf[1] << 8) + opr_buf[0];
	gt1x_abs_y_max = (opr_buf[3] << 8) + opr_buf[2];

	ret = gt1x_i2c_read(GTP_REG_CONFIG_DATA + 6, opr_buf, 1);
	if (ret < 0)
		return ret;
	gt1x_int_type = opr_buf[0] & 0x03;

	GTP_INFO("X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x", gt1x_abs_x_max, gt1x_abs_y_max, gt1x_int_type);

	return 0;
}

/**
 * gt1x_send_cfg - Send gt1x_config Function.
 * @config: pointer of the configuration array.
 * @cfg_len: length of configuration array.
 * Return 0--success,non-0--fail.
 */
s32 gt1x_send_cfg(u8 *config, int cfg_len)
{
#ifdef CONFIG_GTP_DRIVER_SEND_CFG
	int i;
	s32 ret = 0;
	s32 retry = 0;
	u16 checksum = 0;

	GTP_DEBUG("Driver Send Config, length: %d", cfg_len);
	for (i = 0; i < cfg_len - 3; i += 2)
		checksum += (config[i] << 8) + config[i + 1];
	if (!checksum) {
		GTP_ERROR("Invalid config, all of the bytes is zero!");
		return -1;
	}
	checksum = 0 - checksum;
	GTP_DEBUG("Config checksum: 0x%04X", checksum);
	config[cfg_len - 3] = (checksum >> 8) & 0xFF;
	config[cfg_len - 2] = checksum & 0xFF;
	config[cfg_len - 1] = 0x01;

	while (retry++ < 5) {
		ret = gt1x_i2c_write(GTP_REG_CONFIG_DATA, config, cfg_len);
		if (!ret) {
			msleep(200);	/* must 200ms, wait for storing config into flash. */
			GTP_DEBUG("Send config successfully!");
			return 0;
		}
	}
	GTP_ERROR("Send config failed!");
	return ret;
#endif
	return 0;
}

s32 gt1x_init_panel(void)
{
	s32 ret = 0;
	u8 cfg_len = 0;

#ifdef CONFIG_GTP_DRIVER_SEND_CFG
	u8 sensor_id = 0;

	const u8 cfg_grp0[] = GTP_CFG_GROUP0;
	const u8 cfg_grp1[] = GTP_CFG_GROUP1;
	const u8 cfg_grp2[] = GTP_CFG_GROUP2;
	const u8 cfg_grp3[] = GTP_CFG_GROUP3;
	const u8 cfg_grp4[] = GTP_CFG_GROUP4;
	const u8 cfg_grp5[] = GTP_CFG_GROUP5;
	const u8 *cfgs[] = {
		cfg_grp0, cfg_grp1, cfg_grp2,
		cfg_grp3, cfg_grp4, cfg_grp5
	};
	u8 cfg_lens[] = {
		CFG_GROUP_LEN(cfg_grp0),
		CFG_GROUP_LEN(cfg_grp1),
		CFG_GROUP_LEN(cfg_grp2),
		CFG_GROUP_LEN(cfg_grp3),
		CFG_GROUP_LEN(cfg_grp4),
		CFG_GROUP_LEN(cfg_grp5)
	};

#ifdef CONFIG_GTP_CHARGER_SWITCH
	const u8 cfg_grp0_charger[] = GTP_CFG_GROUP0_CHARGER;
	const u8 cfg_grp1_charger[] = GTP_CFG_GROUP1_CHARGER;
	const u8 cfg_grp2_charger[] = GTP_CFG_GROUP2_CHARGER;
	const u8 cfg_grp3_charger[] = GTP_CFG_GROUP3_CHARGER;
	const u8 cfg_grp4_charger[] = GTP_CFG_GROUP4_CHARGER;
	const u8 cfg_grp5_charger[] = GTP_CFG_GROUP5_CHARGER;
	const u8 *cfgs_charger[] = {
		cfg_grp0_charger, cfg_grp1_charger, cfg_grp2_charger,
		cfg_grp3_charger, cfg_grp4_charger, cfg_grp5_charger
	};
	u8 cfg_lens_charger[] = {
		CFG_GROUP_LEN(cfg_grp0_charger),
		CFG_GROUP_LEN(cfg_grp1_charger),
		CFG_GROUP_LEN(cfg_grp2_charger),
		CFG_GROUP_LEN(cfg_grp3_charger),
		CFG_GROUP_LEN(cfg_grp4_charger),
		CFG_GROUP_LEN(cfg_grp5_charger)
	};
#endif				/* end  CONFIG_GTP_CHARGER_SWITCH */

	GTP_DEBUG("Config Groups Length: %d, %d, %d, %d, %d, %d", cfg_lens[0], cfg_lens[1], cfg_lens[2], cfg_lens[3],
		  cfg_lens[4], cfg_lens[5]);

	sensor_id = gt1x_version.sensor_id;
	if (sensor_id >= 6 || cfg_lens[sensor_id] < GTP_CONFIG_MIN_LENGTH
	    || cfg_lens[sensor_id] > GTP_CONFIG_MAX_LENGTH) {
		sensor_id = 0;
	}

	cfg_len = cfg_lens[sensor_id];

	GTP_INFO("CTP_CONFIG_GROUP%d used, gt1x_config length: %d", sensor_id, cfg_len);

	if (cfg_len < GTP_CONFIG_MIN_LENGTH || cfg_len > GTP_CONFIG_MAX_LENGTH) {
		GTP_ERROR
		    ("CTP_CONFIG_GROUP%d is INVALID CONFIG GROUP! NO Config Sent! ;"
			"You need to check you header file CFG_GROUP section!",
		     sensor_id + 1);
		return -1;
	}

	memset(gt1x_config, 0, sizeof(gt1x_config));
	memcpy(gt1x_config, cfgs[sensor_id], cfg_len);

	/* clear the flag, avoid failure when send the_config of driver. */
	gt1x_config[0] &= 0x7F;

#ifdef CONFIG_GTP_CUSTOM_CFG
	gt1x_config[RESOLUTION_LOC] = (u8) tpd_dts_data.tpd_resolution[0];
	gt1x_config[RESOLUTION_LOC + 1] = (u8) (tpd_dts_data.tpd_resolution[0] >> 8);
	gt1x_config[RESOLUTION_LOC + 2] = (u8) tpd_dts_data.tpd_resolution[1];
	gt1x_config[RESOLUTION_LOC + 3] = (u8) (tpd_dts_data.tpd_resolution[1] >> 8);

	GTP_INFO("Res: %d * %d, trigger: %d", tpd_dts_data.tpd_resolution[0],
		tpd_dts_data.tpd_resolution[1], GTP_INT_TRIGGER);

	if (GTP_INT_TRIGGER == 0) {	/* RISING  */
		gt1x_config[TRIGGER_LOC] &= 0xfe;
	} else if (GTP_INT_TRIGGER == 1) {	/* FALLING */
		gt1x_config[TRIGGER_LOC] |= 0x01;
	}
#endif				/* END CONFIG_GTP_CUSTOM_CFG */

#ifdef CONFIG_GTP_CHARGER_SWITCH
	GTP_DEBUG("Charger Config Groups Length: %d, %d, %d, %d, %d, %d", cfg_lens_charger[0],
		  cfg_lens_charger[1], cfg_lens_charger[2], cfg_lens_charger[3], cfg_lens_charger[4],
		  cfg_lens_charger[5]);

	memset(gt1x_config_charger, 0, sizeof(gt1x_config_charger));
	if (cfg_lens_charger[sensor_id] == cfg_len)
		memcpy(gt1x_config_charger, cfgs_charger[sensor_id], cfg_len);

	/* clear the flag, avoid failure when send the config of driver. */
	gt1x_config_charger[0] &= 0x7F;

#ifdef CONFIG_GTP_CUSTOM_CFG
	gt1x_config_charger[RESOLUTION_LOC] = (u8) tpd_dts_data.tpd_resolution[0];
	gt1x_config_charger[RESOLUTION_LOC + 1] = (u8) (tpd_dts_data.tpd_resolution[0] >> 8);
	gt1x_config_charger[RESOLUTION_LOC + 2] = (u8) tpd_dts_data.tpd_resolution[1];
	gt1x_config_charger[RESOLUTION_LOC + 3] = (u8) (tpd_dts_data.tpd_resolution[1] >> 8);

	if (GTP_INT_TRIGGER == 0) {	/* RISING  */
		gt1x_config_charger[TRIGGER_LOC] &= 0xfe;
	} else if (GTP_INT_TRIGGER == 1) {	/* FALLING */
		gt1x_config_charger[TRIGGER_LOC] |= 0x01;
	}
#endif				/* END CONFIG_GTP_CUSTOM_CFG */
	if (cfg_lens_charger[sensor_id] != cfg_len)
		memset(gt1x_config_charger, 0, sizeof(gt1x_config_charger));
#endif				/* END CONFIG_GTP_CHARGER_SWITCH */

#else				/* DRIVER NOT SEND CONFIG */
	cfg_len = GTP_CONFIG_MAX_LENGTH;
	ret = gt1x_i2c_read(GTP_REG_CONFIG_DATA, gt1x_config, cfg_len);
	if (ret < 0)
		return ret;
#endif				/* END CONFIG_GTP_DRIVER_SEND_CFG */

	GTP_DEBUG_FUNC();
	/* match resolution when gt1x_abs_x_max & gt1x_abs_y_max have been set already */
	if ((gt1x_abs_x_max == 0) && (gt1x_abs_y_max == 0)) {
		gt1x_abs_x_max = (gt1x_config[RESOLUTION_LOC + 1] << 8) + gt1x_config[RESOLUTION_LOC];
		gt1x_abs_y_max = (gt1x_config[RESOLUTION_LOC + 3] << 8) + gt1x_config[RESOLUTION_LOC + 2];
		gt1x_int_type = (gt1x_config[TRIGGER_LOC]) & 0x03;
		gt1x_wakeup_level = !(gt1x_config[MODULE_SWITCH3_LOC] & 0x20);
	} else {
		gt1x_config[RESOLUTION_LOC] = (u8) gt1x_abs_x_max;
		gt1x_config[RESOLUTION_LOC + 1] = (u8) (gt1x_abs_x_max >> 8);
		gt1x_config[RESOLUTION_LOC + 2] = (u8) gt1x_abs_y_max;
		gt1x_config[RESOLUTION_LOC + 3] = (u8) (gt1x_abs_y_max >> 8);
		set_reg_bit(gt1x_config[MODULE_SWITCH3_LOC], 5, !gt1x_wakeup_level);
		gt1x_config[TRIGGER_LOC] = (gt1x_config[TRIGGER_LOC] & 0xFC) | gt1x_int_type;
#ifdef CONFIG_GTP_CHARGER_SWITCH
		gt1x_config_charger[RESOLUTION_LOC] = (u8) gt1x_abs_x_max;
		gt1x_config_charger[RESOLUTION_LOC + 1] = (u8) (gt1x_abs_x_max >> 8);
		gt1x_config_charger[RESOLUTION_LOC + 2] = (u8) gt1x_abs_y_max;
		gt1x_config_charger[RESOLUTION_LOC + 3] = (u8) (gt1x_abs_y_max >> 8);
		set_reg_bit(gt1x_config[MODULE_SWITCH3_LOC], 5, !gt1x_wakeup_level);
		gt1x_config[TRIGGER_LOC] = (gt1x_config[TRIGGER_LOC] & 0xFC) | gt1x_int_type;
#endif
	}

	GTP_INFO("X_MAX=%d,Y_MAX=%d,TRIGGER=0x%02x,WAKEUP_LEVEL=%d", gt1x_abs_x_max, gt1x_abs_y_max, gt1x_int_type,
		 gt1x_wakeup_level);

	gt1x_cfg_length = cfg_len;
	ret = gt1x_send_cfg(gt1x_config, gt1x_cfg_length);
	return ret;
}

void gt1x_select_addr(void)
{
	GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);
	msleep(20);
	GTP_GPIO_OUTPUT(GTP_INT_PORT, gt1x_i2c_client->addr == 0x14);
	msleep(20);
	GTP_GPIO_OUTPUT(GTP_RST_PORT, 1);
}

s32 gt1x_reset_guitar(void)
{
	s32 ret = 0;

	GTP_INFO("GTP RESET!\n");

	/* select i2c address */
	gt1x_select_addr();
	msleep(20);		/*must >= 6ms*/

	/* int synchronization */
	if (CHIP_TYPE_GT2X == gt1x_chip_type) {
		/* for GT2X */
	} else {
		GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
		msleep(50);
		GTP_GPIO_AS_INT(GTP_INT_PORT);
	}

#ifdef CONFIG_GTP_ESD_PROTECT
	ret = gt1x_init_ext_watchdog();
#else
	ret = gt1x_i2c_test();
#endif
	return ret;
}

/**
 * gt1x_read_version - Read gt1x version info.
 * @ver_info: address to store version info
 * Return 0-succeed.
 */
s32 gt1x_read_version(struct gt1x_version_info *ver_info)
{
	s32 ret = -1;
	u8 buf[12] = { 0 };
	u32 mask_id = 0;
	u32 patch_id = 0;
	u8 product_id[5] = { 0 };
	u8 sensor_id = 0;
	u8 match_opt = 0;
	int i, retry = 3;
	u8 checksum = 0;

	GTP_DEBUG_FUNC();

	while (retry--) {
		ret = gt1x_i2c_read_dbl_check(GTP_REG_VERSION, buf, sizeof(buf));
		if (!ret) {
			checksum = 0;

			for (i = 0; i < sizeof(buf); i++)
				checksum += buf[i];

			if (checksum == 0 &&	/* first 3 bytes must be number or char */
				/*sensor id == 0xFF, retry */
		IS_NUM_OR_CHAR(buf[0]) && IS_NUM_OR_CHAR(buf[1]) && IS_NUM_OR_CHAR(buf[2]) && buf[10] != 0xFF) {
				break;
			}
			GTP_ERROR("GTP read version failed!(checksum error)");
		} else {
			GTP_ERROR("GTP read version failed!");
		}
		GTP_DEBUG("GTP reread version : %d", retry);
		msleep(100);
	}

	if (retry <= 0)
		return -1;

	mask_id = (u32) ((buf[7] << 16) | (buf[8] << 8) | buf[9]);
	patch_id = (u32) ((buf[4] << 16) | (buf[5] << 8) | buf[6]);
	memcpy(product_id, buf, 4);
	sensor_id = buf[10] & 0x0F;
	match_opt = (buf[10] >> 4) & 0x0F;

	GTP_INFO("IC VERSION: GT%s_%04X(Patch)_%04X(Mask)_%02X(SensorID)", product_id, patch_id >> 8, mask_id >> 8,
		 sensor_id);

	if (ver_info != NULL) {
		ver_info->mask_id = mask_id;
		ver_info->patch_id = patch_id;
		memcpy(ver_info->product_id, product_id, 5);
		ver_info->sensor_id = sensor_id;
		ver_info->match_opt = match_opt;
	}
	return 0;
}

/**
 * gt1x_get_chip_type - get chip type .
 *
 * different chip synchronize in different way,
 */
s32 gt1x_get_chip_type(void)
{
	u8 opr_buf[4] = { 0x00 };
	u8 gt1x_data[] = { 0x02, 0x08, 0x90, 0x00 };
	u8 gt9l_data[] = { 0x01, 0x10, 0x90, 0x00 };
	s32 ret = -1;

	/* chip type already exist */
	if (gt1x_chip_type != CHIP_TYPE_NONE)
		return 0;

	/* read hardware */
	ret = gt1x_i2c_read_dbl_check(GTP_REG_HW_INFO, opr_buf, sizeof(opr_buf));
	if (ret) {
		GTP_ERROR("I2c communication error.");
		return -1;
	}

	/* find chip type */
	if (!memcmp(opr_buf, gt1x_data, sizeof(gt1x_data)))
		gt1x_chip_type = CHIP_TYPE_GT1X;
	else if (!memcmp(opr_buf, gt9l_data, sizeof(gt9l_data)))
		gt1x_chip_type = CHIP_TYPE_GT2X;

	if (gt1x_chip_type != CHIP_TYPE_NONE) {
		GTP_INFO("Chip Type: %s", (gt1x_chip_type == CHIP_TYPE_GT1X) ? "GT1X" : "GT2X");
		return 0;
	} else {
		return -1;
	}
}

/**
 * gt1x_enter_sleep - Eter sleep function.
 *
 * Returns  0--success,non-0--fail.
 */
s32 gt1x_enter_sleep(void)
{
	s32 ret = ERROR;

	if (CHIP_TYPE_GT2X == gt1x_chip_type) {
		/*Store bak ref*/
		/*ret = gt1x_bak_ref_proc(GTP_BAK_REF_STORE);*/
		if (ret)
			GTP_ERROR("[gt1x_enter_sleep]Store bak ref failed.");
	}
#ifdef CONFIG_GTP_POWER_CTRL_SLEEP
	gt1x_power_switch(SWITCH_OFF);
	GTP_INFO("GTP enter sleep by poweroff!");
	return 0;
#else
	{
		s32 retry = 0;

		if (gt1x_wakeup_level == 1) {	/* high level wakeup */
			GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
		}
		msleep(20);

		while (retry++ < 5) {
			if (!gt1x_send_cmd(GTP_CMD_SLEEP, 0)) {
				GTP_INFO("GTP enter sleep!");
				return 0;
			}
			msleep(20);
		}

		GTP_ERROR("GTP send sleep cmd failed.");
		return -1;
	}
#endif
}

/**
 * gt1x_wakeup_sleep - wakeup from sleep mode Function.
 *
 * Return: 0--success,non-0--fail.
 */
s32 gt1x_wakeup_sleep(void)
{
#ifndef CONFIG_GTP_POWER_CTRL_SLEEP
	u8 retry = 0;
	s32 ret = -1;
#endif
	GTP_DEBUG("GTP wakeup begin.");

#ifdef CONFIG_GTP_POWER_CTRL_SLEEP	/* power manager unit control the procedure */
	gt1x_power_reset();
	GTP_INFO("Ic wakeup by poweron");
	return 0;
#else				/* gesture wakeup & int port wakeup */
	while (retry++ < 2) {
#ifdef CONFIG_GTP_GESTURE_WAKEUP
		if (gesture_enabled) {
			if (DOZE_WAKEUP != gesture_doze_status)
				GTP_INFO("Powerkey wakeup.");
			else
				GTP_INFO("Gesture wakeup.");
			gesture_doze_status = DOZE_DISABLED;
			ret = gt1x_reset_guitar();
			if (!ret)
				break;
		} else
#endif
		{
			/* wake up through int port */
			GTP_GPIO_OUTPUT(GTP_INT_PORT, gt1x_wakeup_level);
			msleep(20);

			if (CHIP_TYPE_GT2X == gt1x_chip_type) {
				/* for GT2X */
			} else {
				/* Synchronize int IO */
				GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
				msleep(50);
				GTP_GPIO_AS_INT(GTP_INT_PORT);
			}

			/* test i2c */
			ret = gt1x_i2c_test();
			if (!ret) {

				/* i2c test succeed, init externl watchdog */
#ifdef CONFIG_GTP_ESD_PROTECT
				ret = gt1x_init_ext_watchdog();
				if (!ret)
					break;
#else
				break;
#endif
			}
		}
	}

	if (ret) {		/* wakeup failed , try waking up by resetting */
		while (retry--) {
			ret = gt1x_reset_guitar();
			if (!ret)
				break;
		}
	}

	if (ret) {
		GTP_ERROR("GTP wakeup sleep failed.");
		return -1;
	}
	GTP_INFO("GTP wakeup sleep.");
	return 0;
#endif				/* END CONFIG_GTP_POWER_CTRL_SLEEP */
}

/**
 * gt1x_send_cmd - seng cmd
 * must write data & checksum first
 * byte    content
 * 0       cmd
 * 1       data
 * 2       checksum
 * Returns 0 - succeed,non-0 - failed
 */
s32 gt1x_send_cmd(u8 cmd, u8 data)
{
	s32 ret;
	static DEFINE_MUTEX(cmd_mutex);
	u8 buffer[3] = { cmd, data, 0 };

	mutex_lock(&cmd_mutex);
	buffer[2] = (u8) ((0 - cmd - data) & 0xFF);
	ret = gt1x_i2c_write(GTP_REG_CMD + 1, &buffer[1], 2);
	ret |= gt1x_i2c_write(GTP_REG_CMD, &buffer[0], 1);
	msleep(50);
	mutex_unlock(&cmd_mutex);

	return ret;
}

void gt1x_power_reset(void)
{
	s32 i = 0;
	s32 ret = 0;

	if (is_resetting || update_info.status)
		return;
	GTP_INFO("force_reset_guitar");
	is_resetting = 1;
	gt1x_irq_disable();
	gt1x_power_switch(SWITCH_OFF);
	msleep(30);
	gt1x_power_switch(SWITCH_ON);
	msleep(30);

	for (i = 0; i < 5; i++) {
		ret = gt1x_reset_guitar();
		if (ret < 0)
			continue;
		ret = gt1x_send_cfg(gt1x_config, gt1x_cfg_length);
		if (ret < 0) {
			msleep(500);
			continue;
		}
		break;
	}
	gt1x_irq_enable();
	is_resetting = 0;
}

s32 gt1x_request_event_handler(void)
{
	s32 ret = -1;
	u8 rqst_data = 0;

	ret = gt1x_i2c_read(GTP_REG_RQST, &rqst_data, 1);
	if (ret) {
		GTP_ERROR("I2C transfer error. errno:%d", ret);
		return -1;
	}
	GTP_DEBUG("Request state:0x%02x.", rqst_data);
	switch (rqst_data & 0x0F) {
	case GTP_RQST_CONFIG:
		GTP_INFO("Request Config.");
		ret = gt1x_send_cfg(gt1x_config, gt1x_cfg_length);
		if (ret) {
			GTP_ERROR("Send gt1x_config error.");
		} else {
			GTP_INFO("Send gt1x_config success.");
			rqst_data = GTP_RQST_RESPONDED;
			gt1x_i2c_write(GTP_REG_RQST, &rqst_data, 1);
		}
		break;
	case GTP_RQST_RESET:
		GTP_INFO("Request Reset.");
		gt1x_reset_guitar();
		rqst_data = GTP_RQST_RESPONDED;
		gt1x_i2c_write(GTP_REG_RQST, &rqst_data, 1);
		break;
	case GTP_RQST_BAK_REF:
		GTP_INFO("Request Ref.");
		break;
	case GTP_RQST_MAIN_CLOCK:
		GTP_INFO("Request main clock.");
		break;
#ifdef CONFIG_GTP_HOTKNOT
	case GTP_RQST_HOTKNOT_CODE:
		GTP_INFO("Request HotKnot Code.");
		break;
#endif
	default:
		break;
	}
	return 0;
}

/**
 * gt1x_touch_event_handler - handle touch event
 * (pen event, key event, finger touch envent)
 * @data:
 * Return    <0: failed, 0: succeed
 */
s32 gt1x_touch_event_handler(u8 *data, struct input_dev *dev, struct input_dev *pen_dev)
{
	u8 touch_data[1 + 8 * GTP_MAX_TOUCH + 2] = { 0 };
	u8 touch_num = 0;
	u16 cur_event = 0;
	static u16 pre_event;
	static u16 pre_index;

	u8 key_value = 0;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 id = 0;
	s32 i = 0;
	s32 ret = -1;

	GTP_DEBUG_FUNC();
	touch_num = data[0] & 0x0f;
	if (touch_num > GTP_MAX_TOUCH) {
		GTP_ERROR("Illegal finger number = %d!", touch_num);
		return ERROR_VALUE;
	}

	memcpy(touch_data, data, 11);

	/* read the remaining coor data */
	if (touch_num > 1) {
		ret = gt1x_i2c_read((GTP_READ_COOR_ADDR + 11), &touch_data[11], 1 + 8 * touch_num + 2 - 11);
		if (ret) {
			GTP_ERROR("Read coordinate i2c error.");
			return ret;
		}
	}

	/* checksum */

/*
 * cur_event , pre_event bit defination
 * bit4	bit3		    bit2	 bit1	   bit0
 * hover  stylus_key  stylus  key     touch
 *
 */
	key_value = touch_data[1 + 8 * touch_num];
	/* check current event */
	if ((touch_data[0] & 0x10) && key_value) {
#ifdef CONFIG_GTP_HAVE_STYLUS_KEY
		/* get current key states */
		if (key_value & 0xF0)
			SET_BIT(cur_event, BIT_STYLUS_KEY);
		else if (key_value & 0x0F)
			SET_BIT(cur_event, BIT_TOUCH_KEY);
#endif
		if (tpd_dts_data.use_tpd_button) {
			/* get current key states */
			if (key_value & 0xF0)
				SET_BIT(cur_event, BIT_STYLUS_KEY);
			else if (key_value & 0x0F)
				SET_BIT(cur_event, BIT_TOUCH_KEY);
		}
	}
#ifdef CONFIG_GTP_WITH_STYLUS
	else if (touch_data[1] & 0x80)
		SET_BIT(cur_event, BIT_STYLUS);
#endif
	else if (touch_num)
		SET_BIT(cur_event, BIT_TOUCH);

/* handle current event and pre-event */
#ifdef CONFIG_GTP_HAVE_STYLUS_KEY
	if (CHK_BIT(cur_event, BIT_STYLUS_KEY) || CHK_BIT(pre_event, BIT_STYLUS_KEY)) {
		/*
		 * 0x10 -- stylus key0 down
		 * 0x20 -- stylus key1 down
		 * 0x40 -- stylus key0 & stylus key1 both down
		 */
		u8 temp = (key_value & 0x40) ? 0x30 : key_value;

		for (i = 4; i < 6; i++)
			input_report_key(pen_dev, gt1x_stylus_key_array[i - 4], temp & (0x01 << i));
		GTP_DEBUG("Stulus key event.");
	}
#endif

#ifdef CONFIG_GTP_WITH_STYLUS
	if (CHK_BIT(cur_event, BIT_STYLUS)) {
		coor_data = &touch_data[1];
		id = coor_data[0] & 0x7F;
		input_x = coor_data[1] | (coor_data[2] << 8);
		input_y = coor_data[3] | (coor_data[4] << 8);
		input_w = coor_data[5] | (coor_data[6] << 8);

		input_x = GTP_WARP_X(gt1x_abs_x_max, input_x);
		input_y = GTP_WARP_Y(gt1x_abs_y_max, input_y);

		GTP_DEBUG("Pen touch DOWN.");
		gt1x_pen_down(input_x, input_y, input_w, 0);
	} else if (CHK_BIT(pre_event, BIT_STYLUS)) {
		GTP_DEBUG("Pen touch UP.");
		gt1x_pen_up(0);
	}
#endif
	if (tpd_dts_data.use_tpd_button) {
		if (CHK_BIT(cur_event, BIT_TOUCH_KEY) || CHK_BIT(pre_event, BIT_TOUCH_KEY)) {
			for (i = 0; i < tpd_dts_data.tpd_key_num; i++)
				input_report_key(dev, tpd_dts_data.tpd_key_local[i], key_value & (0x01 << i));
			if (CHK_BIT(cur_event, BIT_TOUCH_KEY))
				GTP_DEBUG("Key Down.");
			else
				GTP_DEBUG("Key Up.");
		}
	}

/* finger touch event*/
	if (CHK_BIT(cur_event, BIT_TOUCH)) {
		u8 report_num = 0;

		coor_data = &touch_data[1];
		id = coor_data[0] & 0x0F;
		for (i = 0; i < GTP_MAX_TOUCH; i++) {
			if (i == id) {
				input_x = coor_data[1] | (coor_data[2] << 8);
				input_y = coor_data[3] | (coor_data[4] << 8);
				input_w = coor_data[5] | (coor_data[6] << 8);

				input_x = GTP_WARP_X(gt1x_abs_x_max, input_x);
				input_y = GTP_WARP_Y(gt1x_abs_y_max, input_y);

				GTP_DEBUG("(%d)(%d, %d)[%d]", id, input_x, input_y, input_w);
				gt1x_touch_down(input_x, input_y, input_w, i);
				if (report_num++ < touch_num) {
					coor_data += 8;
					id = coor_data[0] & 0x0F;
				}
				pre_index |= 0x01 << i;
			} else if (pre_index & (0x01 << i)) {
#ifdef CONFIG_GTP_ICS_SLOT_REPORT
				gt1x_touch_up(i);
#endif
				pre_index &= ~(0x01 << i);
			}
		}
	} else if (CHK_BIT(pre_event, BIT_TOUCH)) {
#ifdef CONFIG_GTP_ICS_SLOT_REPORT
		int cycles = pre_index < 3 ? 3 : GTP_MAX_TOUCH;

		for (i = 0; i < cycles; i++) {
			if (pre_index >> i & 0x01)
				gt1x_touch_up(i);
		}
#else
		gt1x_touch_up(0);
#endif
		GTP_DEBUG("Released Touch.");
		pre_index = 0;
	}

	/* input sync report */
	if (CHK_BIT(cur_event, BIT_STYLUS_KEY | BIT_STYLUS)
	    || CHK_BIT(pre_event, BIT_STYLUS_KEY | BIT_STYLUS)) {
		input_sync(pen_dev);
	}

	if (CHK_BIT(cur_event, BIT_TOUCH_KEY | BIT_TOUCH)
	    || CHK_BIT(pre_event, BIT_TOUCH_KEY | BIT_TOUCH)) {
		input_sync(dev);
	}

	if (!pre_event && !cur_event)
		GTP_DEBUG("Additional Int Pulse.");
	else
		pre_event = cur_event;

	return 0;
}

#ifdef CONFIG_GTP_WITH_STYLUS
struct input_dev *pen_dev;

void gt1x_pen_init(void)
{
	s32 ret = 0;

	pen_dev = input_allocate_device();
	if (pen_dev == NULL) {
		GTP_ERROR("Failed to allocate input device for pen/stylus.");
		return;
	}

	pen_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	pen_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	set_bit(BTN_TOOL_PEN, pen_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);

#ifdef CONFIG_GTP_HAVE_STYLUS_KEY
	input_set_capability(pen_dev, EV_KEY, BTN_STYLUS);
	input_set_capability(pen_dev, EV_KEY, BTN_STYLUS2);
#endif

	input_set_abs_params(pen_dev, ABS_MT_POSITION_X, 0, gt1x_abs_x_max, 0, 0);
	input_set_abs_params(pen_dev, ABS_MT_POSITION_Y, 0, gt1x_abs_y_max, 0, 0);
	input_set_abs_params(pen_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(pen_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(pen_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	pen_dev->name = "goodix-pen";
	pen_dev->phys = "input/ts";
	pen_dev->id.bustype = BUS_I2C;

	ret = input_register_device(pen_dev);
	if (ret) {
		GTP_ERROR("Register %s input device failed", pen_dev->name);
		return;
	}
}

void gt1x_pen_down(s32 x, s32 y, s32 size, s32 id)
{
	input_report_key(pen_dev, BTN_TOOL_PEN, 1);
#ifdef CONFIG_GTP_CHANGE_X2Y
	GTP_SWAP(x, y);
#endif

#ifdef CONFIG_GTP_ICS_SLOT_REPORT
	input_mt_slot(pen_dev, id);
	input_report_abs(pen_dev, ABS_MT_PRESSURE, size);
	input_report_abs(pen_dev, ABS_MT_TOUCH_MAJOR, size);
	input_report_abs(pen_dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(pen_dev, ABS_MT_POSITION_X, x);
	input_report_abs(pen_dev, ABS_MT_POSITION_Y, y);
#else
	input_report_key(pen_dev, BTN_TOUCH, 1);
	if ((!size) && (!id)) {
		/* for virtual button */
		input_report_abs(pen_dev, ABS_MT_PRESSURE, 100);
		input_report_abs(pen_dev, ABS_MT_TOUCH_MAJOR, 100);
	} else {
		input_report_abs(pen_dev, ABS_MT_PRESSURE, size);
		input_report_abs(pen_dev, ABS_MT_TOUCH_MAJOR, size);
		input_report_abs(pen_dev, ABS_MT_TRACKING_ID, id);
	}
	input_report_abs(pen_dev, ABS_MT_POSITION_X, x);
	input_report_abs(pen_dev, ABS_MT_POSITION_Y, y);
	input_mt_sync(pen_dev);
#endif
}

void gt1x_pen_up(s32 id)
{
	input_report_key(pen_dev, BTN_TOOL_PEN, 0);
#ifdef CONFIG_GTP_ICS_SLOT_REPORT
	input_mt_slot(pen_dev, id);
	input_report_abs(pen_dev, ABS_MT_TRACKING_ID, -1);
#else
	input_report_key(pen_dev, BTN_TOUCH, 0);
	input_mt_sync(pen_dev);
#endif
}
#endif

/**
 *		PROXIMITY
 */
#ifdef CONFIG_GTP_PROXIMITY
#define GTP_REG_PROXIMITY_VALID                   0x814E
#define GTP_REG_PROXIMITY_ENABLE                  0x8049
u8 gt1x_proximity_flag = 0;
u8 gt1x_proximity_detect = 1;	/*0-->close ; 1--> far away*/
static struct hwmsen_object obj_ps;


s32 gt1x_ps_operate(void *self, u32 command, void *buff_in, s32 size_in, void *buff_out, s32 size_out, s32 *actualout)
{
	s32 err = 0;
	s32 value;
	hwm_sensor_data *sensor_data;

	GTP_INFO("psensor operator cmd:%d", command);
	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GTP_ERROR("Set delay parameter error!");
			err = -EINVAL;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GTP_ERROR("Enable sensor parameter error!");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			err = gt1x_enable_ps(value);
		}

		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			GTP_ERROR("Get sensor data parameter error!");
			err = -EINVAL;
		} else {
			sensor_data = (hwm_sensor_data *) buff_out;
			sensor_data->values[0] = gt1x_get_ps_value();
			sensor_data->value_divide = 1;
			sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		}

		break;

	default:
		GTP_ERROR("proxmy sensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}

void gt1x_ps_init(void)
{
	s32 err = 0;
	/*obj_ps.self = cm3623_obj;*/
	obj_ps.polling = 0;	/*0--interrupt mode;1--polling mode;*/
	obj_ps.sensor_operate = gt1x_ps_operate;
	err = hwmsen_attach(ID_PROXIMITY, &obj_ps);
	if (err)
		GTP_ERROR("hwmsen attach fail, return:%d.", err);
}

void gt1x_report_ps(u8 state)
{
	s32 ret = -1;
	hwm_sensor_data sensor_data;
	/*get raw data*/
	GTP_DEBUG("P-sensor state:%s", state ? "AWAY" : "NEAR");
	/*map and store data to hwm_sensor_data*/
	sensor_data.values[0] = state;
	sensor_data.value_divide = 1;
	sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
	/*report to the up-layer*/
	ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);

	if (ret)
		GTP_ERROR("Call hwmsen_get_interrupt_data fail = %d\n", ret);
}

static s32 gt1x_get_ps_value(void)
{
	return gt1x_proximity_detect;
}

static s32 gt1x_enable_ps(s32 enable)
{
	u8 state;
	s32 ret = -1;

	GTP_INFO("TPD proximity function to be %s.", enable ? "on" : "off");
	state = enable ? 1 : 0;
	ret = gt1x_i2c_write(GTP_REG_PROXIMITY_ENABLE, &state, 1);
	if (ret)
		GTP_ERROR("TPD %s proximity cmd failed.", state ? "enable" : "disable");

	if (enable) {
		if (!ret) {
			gt1x_proximity_flag = 1;
			gt1x_proximity_detect = 1;
		}
	} else {
		gt1x_proximity_flag = 0;
	}

	GTP_INFO("TPD proximity function %s %s.", state ? "enable" : "disable", ret ? "fail" : "success");
	return ret;
}

int gt1x_prox_event_handler(u8 *data)
{
	u8 proximity_status = 0;

	if (gt1x_proximity_flag) {
		GTP_DEBUG("REG INDEX[0x814E]:0x%02X\n", data[0]);
		proximity_status = (data[0] & 0x60) ? 0 : 1;
		if (proximity_status != gt1x_proximity_detect) {
			gt1x_report_ps(proximity_status);
			gt1x_proximity_detect = proximity_status;
		}
		if (proximity_status == 0)
			return 1;
		else
			return 0;
	}
	return -1;
}

#endif				/*CONFIG_GTP_PROXIMITY */

/**
 *			ESD PROTECT
 */
#ifdef CONFIG_GTP_ESD_PROTECT
static int esd_work_cycle = 200;
static struct delayed_work esd_check_work;
static int esd_running;
struct mutex esd_lock;
static void gt1x_esd_check_func(struct work_struct *);

void gt1x_init_esd_protect(void)
{
	esd_work_cycle = 2 * HZ;	/*HZ: clock ticks in 1 second generated by system*/
	GTP_DEBUG("Clock ticks for an esd cycle: %d", esd_work_cycle);
	INIT_DELAYED_WORK(&esd_check_work, gt1x_esd_check_func);
	mutex_init(&esd_lock);
}

void gt1x_deinit_esd_protect(void)
{
	gt1x_esd_switch(SWITCH_OFF);
}

s32 gt1x_init_ext_watchdog(void)
{
	s32 ret;
	u8 value = 0xAA;

	GTP_DEBUG("Init external watchdog.");
	ret = gt1x_send_cmd(GTP_CMD_ESD, 0);
	ret |= gt1x_i2c_write(GTP_REG_ESD_CHECK, &value, 1);
	return ret;
}

void gt1x_esd_switch(s32 on)
{
	mutex_lock(&esd_lock);
	if (SWITCH_ON == on) {	/* switch on esd check */
		if (!esd_running) {
			esd_running = 1;
			GTP_INFO("Esd protector started!");
			queue_delayed_work(gt1x_workqueue, &esd_check_work, esd_work_cycle);
		}
	} else {		/* switch off esd check */
		if (esd_running) {
			esd_running = 0;
			GTP_INFO("Esd protector stopped!");
			cancel_delayed_work(&esd_check_work);
		}
	}
	mutex_unlock(&esd_lock);
}

static void gt1x_esd_check_func(struct work_struct *work)
{
	s32 i = 0;
	s32 ret = -1;
	u8 esd_buf[4] = { 0 };

	if (!esd_running) {
		GTP_INFO("Esd protector suspended!");
		return;
	}

	for (i = 0; i < 3; i++) {
		ret = gt1x_i2c_read(GTP_REG_CMD, esd_buf, 4);
		GTP_DEBUG("[Esd]0x8040 = 0x%02X, 0x8043 = 0x%02X", esd_buf[0], esd_buf[3]);
		if (!ret && esd_buf[0] != 0xAA && esd_buf[3] == 0xAA)
			break;
		msleep(50);
	}

	if (i < 3) {
		/* IC works normally, Write 0x8040 0xAA, feed the watchdog */
		gt1x_send_cmd(GTP_CMD_ESD, 0);
	} else {
		if (esd_running) {
			GTP_INFO("IC works abnormally! Process reset guitar.");
			memset(esd_buf, 0x01, sizeof(esd_buf));
			gt1x_i2c_write(0x4226, esd_buf, sizeof(esd_buf));
			msleep(50);

			gt1x_power_reset();
		} else {
			GTP_INFO("Esd protector suspended, no need reset!");
		}
	}

	mutex_lock(&esd_lock);
	if (esd_running)
		queue_delayed_work(gt1x_workqueue, &esd_check_work, esd_work_cycle);
	else
		GTP_INFO("Esd protector suspended!");
	mutex_unlock(&esd_lock);
}
#endif

/**
 *         CHARGER SWITCH
 */
#ifdef CONFIG_GTP_CHARGER_SWITCH

u8 gt1x_config_charger[GTP_CONFIG_MAX_LENGTH] = { 0 };

static struct delayed_work charger_switch_work;
static int charger_work_cycle = 200;
static spinlock_t charger_lock;
static int charger_running;
static void gt1x_charger_work_func(struct work_struct *);

void gt1x_init_charger(void)
{
	charger_work_cycle = 2 * HZ;	/*HZ: clock ticks in 1 second generated by system*/
	GTP_DEBUG("Clock ticks for an charger cycle: %d", charger_work_cycle);
	INIT_DELAYED_WORK(&charger_switch_work, gt1x_charger_work_func);
	spin_lock_init(&charger_lock);
}

/**
 * gt1x_charger_switch - switch states of charging work thread
 *
 * @on: SWITCH_ON - start work thread, SWITCH_OFF: stop .
 *
 */
void gt1x_charger_switch(s32 on)
{
	spin_lock(&charger_lock);
	if (SWITCH_ON == on) {
		if (!charger_running) {
			charger_running = 1;
			spin_unlock(&charger_lock);
			GTP_INFO("Charger checker started!");
			queue_delayed_work(gt1x_workqueue, &charger_switch_work, charger_work_cycle);
		} else {
			spin_unlock(&charger_lock);
		}
	} else {
		if (charger_running) {
			charger_running = 0;
			spin_unlock(&charger_lock);
			cancel_delayed_work(&charger_switch_work);
			GTP_INFO("Charger checker stopped!");
		} else {
			spin_unlock(&charger_lock);
		}
	}
}

/**
 * gt1x_charger_config - check and update charging status configuration
 * @dir_update
 * 0: check before send charging status configuration
 * 1: directly send charging status configuration
 *
 */
void gt1x_charger_config(s32 dir_update)
{
	static u8 chr_pluggedin;

	if (gt1x_get_charger_status()) {
		if (!chr_pluggedin || dir_update) {
			GTP_INFO("Charger Plugin.");
			if (gt1x_send_cfg(gt1x_config_charger, gt1x_cfg_length))
				GTP_ERROR("Send config for Charger Plugin failed!");
			if (gt1x_send_cmd(GTP_CMD_CHARGER_ON, 0))
				GTP_ERROR("Update status for Charger Plugin failed!");
			chr_pluggedin = 1;
		}
	} else {
		if (chr_pluggedin || dir_update) {
			GTP_INFO("Charger Plugout.");
			if (gt1x_send_cfg(gt1x_config, gt1x_cfg_length))
				GTP_INFO("Send config for Charger Plugout failed!");
			if (gt1x_send_cmd(GTP_CMD_CHARGER_OFF, 0))
				GTP_ERROR("Update status for Charger Plugout failed!");
			chr_pluggedin = 0;
		}
	}
}

static void gt1x_charger_work_func(struct work_struct *work)
{
	if (!charger_running) {
		GTP_INFO("Charger checker suspended!");
		return;
	}

	gt1x_charger_config(0);

	GTP_DEBUG("Charger check done!");
	if (charger_running)
		queue_delayed_work(gt1x_workqueue, &charger_switch_work, charger_work_cycle);
}
#endif

s32 gt1x_init(void)
{
	s32 ret = -1;
	s32 retry = 0;
	u8 reg_val[1];

	gt1x_power_switch(SWITCH_ON);

	/* select i2c address */
	gt1x_select_addr();
	msleep(20);

	while (retry++ < 5) {
		gt1x_init_failed = 0;
		/* get chip type */
		ret = gt1x_get_chip_type();
		if (ret != 0) {
			GTP_ERROR("GTP get chip type failed!");
			continue;
		}

			GTP_ERROR("GTP reset guitar begin!");
		/* reset ic */
		ret = gt1x_reset_guitar();
		if (ret != 0) {
			GTP_ERROR("GTP reset guitar failed!");
			continue;
		} else {
			tpd_load_status = 1;
			check_flag = true;
			GTP_ERROR("GTP check_flag = true!");
			wake_up_interruptible(&init_waiter);
		}

		ret = gt1x_i2c_read_dbl_check(0x41E4, reg_val, 1);
		if (ret != 0) {
			continue;
		} else if (reg_val[0] != 0xBE) {
			GTP_ERROR("Check 0x41E4 failed.");
			gt1x_init_failed = 1;
			break;
		}

		/* read version information */
		ret = gt1x_read_version(&gt1x_version);
		if (ret != 0) {
			GTP_ERROR("GTP get verision failed!");
			gt1x_init_failed = 1;
			continue;
		}

		/* init and send configs */
		ret = gt1x_init_panel();
		if (ret != 0) {
			GTP_ERROR("GTP init panel failed.");
			continue;
		} else {
			break;
		}
	}

	/* if the initialization fails, set default setting */
	ret |= gt1x_init_failed;
	if (ret) {
		GTP_INFO("Init failed, use default setting");
		gt1x_abs_x_max = tpd_dts_data.tpd_resolution[0];
		gt1x_abs_y_max = tpd_dts_data.tpd_resolution[1];
		gt1x_int_type = GTP_INT_TRIGGER;
		gt1x_wakeup_level = GTP_WAKEUP_LEVEL;
		return ret;
	}

	gt1x_workqueue = create_singlethread_workqueue("gt1x_workthread");
	if (gt1x_workqueue == NULL)
		GTP_ERROR("create workqueue failed!");

/* init auxiliary  node and functions */
	gt1x_init_debug_node();

#ifdef CONFIG_GTP_CREATE_WR_NODE
	gt1x_init_tool_node();
#endif

#if defined(CONFIG_GTP_GESTURE_WAKEUP) || defined(CONFIG_GTP_HOTKNOT)
	gt1x_init_node();
#endif

#ifdef CONFIG_GTP_PROXIMITY
	gt1x_ps_init();
#endif

#ifdef CONFIG_GTP_CHARGER_SWITCH
	gt1x_charger_config(1);
	gt1x_init_charger();
	gt1x_charger_switch(SWITCH_ON);
#endif

#ifdef CONFIG_GTP_WITH_STYLUS
	gt1x_pen_init();
#endif

	return ret;
}

void gt1x_deinit(void)
{
#ifdef CONFIG_GTP_CREATE_WR_NODE
	gt1x_deinit_tool_node();
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
	gt1x_deinit_esd_protect();
#endif

#ifdef CONFIG_GTP_CHARGER_SWITCH
	gt1x_charger_switch(SWITCH_OFF);
#endif

	if (gt1x_workqueue)
		destroy_workqueue(gt1x_workqueue);
}

#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/hrtimer.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include "ndt_core.h"
#ifdef CONFIG_TOUCHSCREEN_ST_CORE
#include "./fts/fts.h"
#endif
#ifdef CONFIG_TOUCHSCREEN_ST_FTS_V521
#include "./fts_521/fts.h"
#endif
#define IIC_EEPROM 0
#define IIC_DEV_ID 0x02
#define IIC_MANU_ID 0x03
#define IIC_MODULE_ID 0x04
#define IIC_FW_VER 0x05
#define IIC_WAKE_UP 0x06
#define IIC_SLEEP 0x07
#define IIC_DEBUG_MODE 0x60
#define IIC_DATA_READY 0x61
#define IIC_DEBUG_DATA1 0x62
#define IIC_DEBUG_DATA2 0x63
#define IIC_DEBUG_DATA3 0x64
#define IIC_DEBUG_DATA4 0x65
#define IIC_KEY_SEN 0xD0
#define CONFIG_INPUT_NDT_FWUPDATE

#ifdef CONFIG_INPUT_NDT_INTERRUPT
#define RELEASE_THRE 70
#endif
static struct i2c_client *g_ndt_client;
struct input_dev *ndt_input_dev;
static int ndt_read_register(unsigned char reg, unsigned char *datbuf, int byteno);
static int ndt_write_register(unsigned char reg, unsigned char *datbuf, int byteno);
#ifdef CONFIG_INPUT_NDT_FWUPDATE
static int ndt_read_eeprom(unsigned short reg, unsigned char *datbuf, int byteno);
static int ndt_write_eeprom(unsigned short reg, unsigned char *datbuf, int byteno);
static void ndt_reset(void);
static int ndt_update_fw(bool force, char *fw_name);
#endif
#ifdef CONFIG_INPUT_NDT_FWUPDATE
static int ndt_burn_fw(unsigned char *buf, unsigned int len);
static int ndt_eeprom_erase(void);
static int ndt_eeprom_skip(void);
static ssize_t pressure_update_fw_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t pressure_update_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t pressure_erase_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t pressure_force_update_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
#endif
static ssize_t pressure_fw_info_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t pressure_pressure_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ndt_threshold_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ndt_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ndt_irq_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ndt_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ndt_reset_and_read_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR(fw_info, 0444, pressure_fw_info_show, NULL);
static DEVICE_ATTR(pressure, 0444, pressure_pressure_show, NULL);
static DEVICE_ATTR(reset, 0200, NULL, ndt_reset_store);
static DEVICE_ATTR(irq_enable, 0200, NULL, ndt_irq_enable_store);
static DEVICE_ATTR(reset_and_read, 0200, NULL, ndt_reset_and_read_store);
static DEVICE_ATTR(threshold, 0644, ndt_threshold_show, ndt_threshold_store);
#ifdef CONFIG_INPUT_NDT_FWUPDATE
static DEVICE_ATTR(fw_update, (S_IRUGO | S_IWUSR | S_IWGRP), pressure_update_fw_show, pressure_update_fw_store);
static DEVICE_ATTR(fw_erase, (S_IRUGO | S_IWUSR | S_IWGRP), pressure_update_fw_show, pressure_erase_fw_store);
static DEVICE_ATTR(fw_update_force, (S_IRUGO | S_IWUSR | S_IWGRP), pressure_update_fw_show, pressure_force_update_fw_store);
#endif

static struct attribute *ndt_attr[] = {
	&dev_attr_fw_info.attr,
	&dev_attr_pressure.attr,
	&dev_attr_reset.attr,
	&dev_attr_irq_enable.attr,
	&dev_attr_reset_and_read.attr,
	&dev_attr_threshold.attr,
#ifdef CONFIG_INPUT_NDT_FWUPDATE
	&dev_attr_fw_update.attr,
	&dev_attr_fw_update_force.attr,
	&dev_attr_fw_erase.attr,
#endif
	NULL
};

static const struct attribute_group ndt_attr_group = {
	.attrs = ndt_attr,
};

struct ndt_force_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct hrtimer release_timer;
	int released;
	struct work_struct       release_work;
	struct workqueue_struct  *event_wq;
	int report_status;
	struct ndt_platform_data *pdata;
	int suspend_flag;
	struct regulator *ndt_vdd;
	struct regulator *ndt_vddio;
	bool fw_updated;
	struct work_struct       fwupdate_work;
};

struct ndt_platform_data {
	int irq_gpio;
};

static int ndt_read_register(unsigned char reg, unsigned char *datbuf, int byteno)
{
	struct i2c_msg msg[2];
	int ret = 0, retry = 0;
	msg[0].addr = g_ndt_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;
	msg[1].addr = g_ndt_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = byteno;
	msg[1].buf = datbuf;

	if (!datbuf)
		return -EINVAL;
	do {
		ret = i2c_transfer(g_ndt_client->adapter, msg, 2);
		if (ret >= 0)
			break;
		if (ret < 0)
			pr_err("ndt:i2c_transfer Error ! err_code:%d, retry:%d\n", ret, retry);
	} while (retry++ < 5);
	return ret;
}

static int ndt_write_register(unsigned char reg, unsigned char *datbuf, int byteno)
{
	unsigned char *buf;
	struct i2c_msg msg;
	int ret = 0, retry = 0;

	if (!datbuf)
		return -EINVAL;

	buf = (unsigned char *)kmalloc(byteno + 1, GFP_KERNEL);

	if (!buf)
		return -EINVAL;

	memset(buf, 0, byteno + 1);
	buf[0] = reg;

	if (byteno > 0) {
		memcpy(buf + 1, datbuf, byteno);
	}
	msg.addr = g_ndt_client->addr;
	msg.flags = 0;
	msg.len = byteno + 1;
	msg.buf = buf;
	do {
		ret = i2c_transfer(g_ndt_client->adapter, &msg, 1);
		if (ret >= 0)
			break;
		if (ret < 0) {
			pr_err("ndt:i2c_transfer Error ! err_code:%d, retry:%d\n", ret, retry);
		}
	} while (retry++ < 5);

	kfree(buf);
	return ret;
}

static unsigned int g_last_x;
static unsigned int g_last_y;
static unsigned char g_touch_flag;

int ndt_get_pressure(int touch_flag, int x, int y)
{
	int pressure;
	unsigned char buf[10];

	if (g_ndt_client == NULL)
		return 1;

	pr_debug("%s,touch_flag:%d,g_touch_flag:%d,x:%d,y:%d\n", __func__, touch_flag, g_touch_flag, x, y);
	if (touch_flag == 0 && g_touch_flag == 0) {
		return 1;
	}

	if (touch_flag == 0 && x == 0 && y == 0) {
		x = g_last_x;
		y = g_last_y;
	}
	buf[0] = x & 0xff;
	buf[1] = x >> 8;
	buf[2] = y & 0xff;
	buf[3] = y >> 8;
	buf[4] = touch_flag;
	if (x != g_last_x || y != g_last_y) {
		ndt_write_register(0x11, buf, 5);
	}
	g_last_x = x;
	g_last_y = y;
	if (touch_flag == 1 && g_touch_flag == 0) {
		buf[0] = 1;
		ndt_write_register(0x1f, buf, 1);
		ndt_write_register(0x10, buf, 1);
		g_touch_flag = 1;
	} else if (touch_flag == 0 && g_touch_flag == 1) {
		buf[0] = 0;
		ndt_write_register(0x10, buf, 1);
		g_touch_flag = 0;
		return 1;
	}
	memset(buf, 0, sizeof(buf));

	ndt_read_register(0x21, buf, 2);

	pressure = buf[1] << 8 | buf[0];
	pr_debug("ndt:pressure:pressure = %d\n", pressure);
	return pressure == 0 ? 1 : pressure;
}

#ifdef CONFIG_INPUT_NDT_FWUPDATE
static void ndt_reset(void)
{
	int error = 0;
	struct ndt_force_data *data = i2c_get_clientdata(g_ndt_client);

	error = regulator_disable(data->ndt_vdd);
	msleep(2000);
	error = regulator_enable(data->ndt_vdd);
	msleep(20);

}

static int ndt_burn_fw(unsigned char *buf, unsigned int len)
{
	unsigned short reg;
	int byteno = 0;
	int pos = 0;
	unsigned char *read_buf;
	int ret = 1;
	int i = 0;
	int number = 0;
	bool i2c_ok_flag = true;

	if (len % 128 != 0) {
		pr_err("ndt:burn len is not 128 *\n");
		return 0;
	}
	/*reset*/
	ndt_reset();
	/*erase flash*/
	read_buf = (unsigned char *)kmalloc(len, GFP_KERNEL);

	do {
		pr_info("ndt:burn eeprom number: %d\n", number + 1);
		ret = 1;
		byteno = 0;

		if (ndt_eeprom_erase() == 0) {
			i2c_ok_flag = false;
		}
	/*write eeprom*/
		pos = 0;
		reg = IIC_EEPROM;
		byteno = 128;
		while (i2c_ok_flag && pos < len) {
			if (ndt_write_eeprom(reg, buf + pos, byteno) > 0) {
				pr_debug("reg=0x%02x,byteno=%d\n", reg, byteno);
			} else {
				pr_info("failed!\n");
				i2c_ok_flag = false;
			}

			pos += byteno;
			reg += byteno;
			msleep(15);
		}

	/*read eeprom and check*/
		pos = 0;
		reg = IIC_EEPROM;
		byteno = 256;

		while (i2c_ok_flag && pos < len) {
			if (ndt_read_eeprom(reg, read_buf + pos, byteno) > 0) {
				pr_debug("ndt:reg=0x%02x,byteno=%d\n", reg, byteno);
			} else {
				pr_info("ndt:failed!\n");
				i2c_ok_flag = false;
			}

			pos += byteno;
			reg += byteno;
			msleep(15);
		}

	/*check */
		for (i = 0; i < len; i++) {
			if (i2c_ok_flag && buf[i] != read_buf[i]) {
				pr_info("ndt:burn check error!%d,%d\n", buf[i], read_buf[i]);
				ret = 0;
				break;
			}
		}

		number++;
	} while (i2c_ok_flag && number < 3 && ret == 0);

	if (i2c_ok_flag == false || ret == 0) {
		pr_err("ndt:burn eeprom fail!\n");
		ret = 0;
	} else {
		pr_info("ndt:burn eeprom succeed!\n");
	}

	/*exit burn */
	ndt_eeprom_skip();
	kfree(read_buf);
	return ret;
}

static ssize_t pressure_update_fw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	int fw_size;
	unsigned char ic_fw_ver[2];
	unsigned char reg;
	int byteno;
	unsigned char file_fw_ver[2];
	const struct firmware *fw_entry = NULL;
	char *data;

	ret = request_firmware(&fw_entry, "ndt_fw.bin", &(g_ndt_client->dev));
	if (ret != 0) {
		pr_err("%s request firmware failed\n", __func__);
		return -EINVAL;
	}

	fw_size = fw_entry->size;
	data = (char *)kzalloc(fw_size * sizeof(char), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s kzalloc memory fail\n", __func__);
		release_firmware(fw_entry);
		return -EINVAL;
	}
	memcpy(data, fw_entry->data, fw_size);

	reg = IIC_FW_VER;
	byteno = 2;
	if (ndt_read_register(reg, ic_fw_ver, byteno) > 0) {
		pr_debug("ndt:reg=0x%02x,byteno=%d\n", reg, byteno);
	} else {
		pr_err("ndt:failed!\n");
	}
	memcpy(file_fw_ver, data + 8, sizeof(file_fw_ver));
	pr_info("ndt:read file_fw_ver: %02x%02x\n", file_fw_ver[0], file_fw_ver[1]);
	ret = snprintf(buf, PAGE_SIZE, "ic_fw_ver:%02x%02x,file_fw_ver:%02x%02x\n", ic_fw_ver[0], ic_fw_ver[1], file_fw_ver[1], file_fw_ver[0]);
	release_firmware(fw_entry);
	kfree(data);
	data = NULL;
	return ret;
}

static ssize_t ndt_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int yes_no;

	yes_no = simple_strtoul(buf, NULL, 10);

	if (yes_no == 1)
		ndt_reset();
	return count;
}

static ssize_t ndt_irq_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int yes_no;

	yes_no = simple_strtoul(buf, NULL, 10);

	if (yes_no == 1)
		enable_irq(g_ndt_client->irq);
	else
		disable_irq(g_ndt_client->irq);
	return count;
}

#define MIN_LIMIT 50
#define MAX_LIMIT 10000
static ssize_t ndt_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned char buffer[10];

	ret = sscanf(buf, "%d", &value);
	if (ret != 1 || value < MIN_LIMIT || value > MAX_LIMIT) {
		pr_err("failed to set trigger value -- invalid argumet\n");
		return -EINVAL;
	}

	buffer[0] = 0x32;
	ndt_write_register(0x80, buffer, 1);
	msleep(20);

	buffer[0] = 0x00;
	buffer[1] = 0x00;
	buffer[2] = value & 0xFF;
	buffer[3] = (value >> 8) & 0xFF;
	buffer[4] = (value >> 16) & 0xFF;
	buffer[5] = (value >> 24) & 0xFF;
	ndt_write_register(0x82, buffer, 6);
	msleep(20);

	buffer[0] = 0x06;
	ndt_write_register(0x81, buffer, 1);
	msleep(100);

	buffer[0] = 0x00;
	ndt_write_register(0x80, buffer, 1);
	pr_info("%s,threshold:%d\n", __func__, value);

	return count;
}

static ssize_t ndt_reset_and_read_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int yes_no;
	int error = 0;
	struct ndt_force_data *data = i2c_get_clientdata(g_ndt_client);
	static unsigned char g_ver[2] = {0};

	yes_no = simple_strtoul(buf, NULL, 10);

	if (yes_no == 1) {
		error = regulator_disable(data->ndt_vdd);
		msleep(20);
		error = regulator_enable(data->ndt_vdd);
		msleep(10);
	}
	/*i2c test */
	error = ndt_read_register(0x05, g_ver, 2);
	if (error < 0) {
		pr_err("ndt:i2c test error\n");
	} else
		pr_info("ndt:i2c test ok,g_ver:0x%x:0x%x\n", g_ver[0], g_ver[1]);
	return count;
}

static ssize_t pressure_erase_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct ndt_force_data *data = i2c_get_clientdata(g_ndt_client);
	int yes_no = simple_strtoul(buf, NULL, 10);

	if (yes_no != 1) {
		pr_info("ndt:no need update fw\n");
		return count;
	}
	disable_irq(data->client->irq);
	ndt_reset();
	ndt_eeprom_erase();
	enable_irq(data->client->irq);
	return count;
}

static ssize_t pressure_update_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int len = 0;
	char *fw_name;

	len = strnlen(buf, count);
	fw_name = kzalloc(len + 1, GFP_KERNEL);
	if (fw_name == NULL)
		return -ENOMEM;
	if (count > 0) {
		strlcpy(fw_name, buf, len);
		if (fw_name[len - 1] == '\n')
			fw_name[len - 1] = 0;
		else
			fw_name[len] = 0;
	}
	pr_info("ndt:fw_name%s\n", fw_name);
	ndt_update_fw(false, fw_name);
	return count;
}

static int ndt_eeprom_erase(void)
{
	unsigned char erase_cmd[10];
	unsigned short reg;
	int byteno = 0;

	if (g_ndt_client == NULL)
		return 0;

	/*erase flash*/
	reg = IIC_EEPROM;
	erase_cmd[byteno++] = 0xaa;
	erase_cmd[byteno++] = 0x55;
	erase_cmd[byteno++] = 0xa5;
	erase_cmd[byteno++] = 0x5a;

	if (ndt_write_eeprom(reg, erase_cmd, byteno) > 0) {
		pr_debug("ndt:reg=0x%02x,byteno=%d\n", reg, byteno);
	} else {
		pr_err("ndt:failed!\n");
		return 0;
	}

	msleep(2000);
	return 1;
}

static int ndt_eeprom_skip(void)
{
	unsigned char erase_cmd[10];
	unsigned short reg;
	int byteno = 0;

	ndt_reset();
	msleep(100);

	if (g_ndt_client == NULL)
		return 0;

	reg = IIC_EEPROM;
	byteno = 0;
	erase_cmd[byteno++] = 0x7e;
	erase_cmd[byteno++] = 0xe7;
	erase_cmd[byteno++] = 0xee;
	erase_cmd[byteno++] = 0x77;

	if (ndt_write_eeprom(reg, erase_cmd, byteno) > 0) {
		pr_debug("ndt:reg=0x%02x,byteno=%d\n", reg, byteno);
	} else {
		pr_err("ndt:failed!\n");
		return 0;
	}

	return 1;
}

static int ndt_update_fw(bool force, char *fw_name)
{
	loff_t pos = 0;
	char fw_data_len[4];
	int len = 0;
	char *fw_data = NULL;
	char *data = NULL;
	char tmpbuf[10];
	unsigned char ic_fw_ver[2];
	unsigned char reg = 0;
	int byteno = 0;
	unsigned char file_fw_ver[2];
	int ret = 1;
	int fw_size = 0;
	const struct firmware *fw_entry = NULL;
	struct ndt_force_data *ndt_data = NULL;i2c_get_clientdata(g_ndt_client);

	if (g_ndt_client == NULL)
		return -EINVAL;
	ndt_data = i2c_get_clientdata(g_ndt_client);
	if (!force && ndt_data->fw_updated) {
		pr_info("ndt fw aleady updated or fw is updating,no need to update fw\n");
		return ret;
	}
	disable_irq(g_ndt_client->irq);
	ret = request_firmware(&fw_entry, fw_name, &(g_ndt_client->dev));
	if (ret != 0) {
		pr_err("%s request firmware failed\n", __func__);
		enable_irq(g_ndt_client->irq);
		ndt_threshold_store(&g_ndt_client->dev, NULL, "70", 2);
		return -EINVAL;
	}

	fw_size = fw_entry->size;
	data = (char *)kzalloc(fw_size * sizeof(char), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s kzalloc memory fail\n", __func__);
		ret = -EINVAL;
		goto FAIL;
	}
	memcpy(data, fw_entry->data, fw_size);
	memcpy(tmpbuf, data, sizeof(tmpbuf));
	pr_debug("ndt:file header: 0x%02x%02x\n", tmpbuf[0], tmpbuf[1]);
	pos = 8;
	data += pos;
	reg = IIC_FW_VER;
	byteno = 2;
	ic_fw_ver[0] = 0;
	ic_fw_ver[1] = 0;
	if (ndt_read_register(reg, ic_fw_ver, byteno) > 0) {
		pr_info("ndt:ic_fw_ver: %02x%02x\n", ic_fw_ver[0], ic_fw_ver[1]);
	} else
		pr_err("ndt:failed!\n");
	memcpy(file_fw_ver, data, sizeof(file_fw_ver));
	pr_info("ndt:read file_fw_ver: %02x%02x\n", file_fw_ver[0], file_fw_ver[1]);
	if ((ic_fw_ver[0] | (ic_fw_ver[1] << 8)) == (file_fw_ver[1] | (file_fw_ver[0] << 8))) {
		if (!force) {
			pr_info("ndt:fw version is equal,no need update!\n");
			ret = -EINVAL;
			goto FAIL;
		}
	}
	pos = 12 - 8;
	data += pos;
	memcpy(fw_data_len, data, sizeof(fw_data_len));
	pr_debug("ndt:read fw_data_len: 0x%02x%02x%02x%02x\n", fw_data_len[0], fw_data_len[1], fw_data_len[2], fw_data_len[3]);
	len = (int)((unsigned int)fw_data_len[3] << 0) | ((unsigned int)fw_data_len[2] << 8) | ((unsigned int)fw_data_len[1] << 16) | (fw_data_len[0] << 24);
	fw_data = (char *)kzalloc(len, GFP_KERNEL);
	if (fw_data == NULL) {
		pr_err("%s malloc fw_data error", __func__);
		ret = -EINVAL;
		goto FAIL;
	}
	pos = 0x100 - 0x0c;
	data += pos;
	memcpy(fw_data, data, len);

	if (ndt_burn_fw(fw_data, len) == 0) {
		pr_err("ndt:Burn FW failed!\n");
		ret = -EINVAL;
	}
FAIL:
	if (data) {
		kfree(data);
		data = NULL;
	}
	if (fw_data) {
		kfree(fw_data);
		fw_data = NULL;
	}
	release_firmware(fw_entry);
	enable_irq(g_ndt_client->irq);
#ifdef CONFIG_INPUT_NDT_INTERRUPT
	ndt_threshold_store(&g_ndt_client->dev, NULL, "70", 2);
#endif
	return ret;
}

static ssize_t pressure_force_update_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int len = 0;
	char *fw_name;

	len = strnlen(buf, count);
	fw_name = kzalloc(len + 1, GFP_KERNEL);
	if (fw_name == NULL)
		return -ENOMEM;
	if (count > 0) {
		strlcpy(fw_name, buf, len);
		if (fw_name[len - 1] == '\n')
			fw_name[len - 1] = 0;
		else
			fw_name[len] = 0;
	}
	pr_info("ndt:fw_name%s\n", fw_name);
	ndt_update_fw(true, fw_name);
	return count;
}

#endif

bool ndt_is_pressed(void)
{
	struct ndt_force_data *data = NULL;

	if (g_ndt_client == NULL)
		return false;
	else {
		data = i2c_get_clientdata(g_ndt_client);
		return data->released;
	}
}

static ssize_t pressure_pressure_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int pressure, count;
	int touch_down, x, y;

	fts_get_pointer(&touch_down, &x, &y);
	pressure = ndt_get_pressure(touch_down, x, y);

	count = snprintf(buf, PAGE_SIZE, "Pressure:%d\n", pressure);
	return count;
}

static ssize_t pressure_fw_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	char s_dev_id[30];
	ssize_t ret;
	unsigned char dev_id[10];
	unsigned char manu_id[2];
	unsigned char module_id[2];
	unsigned char fw_ver[2];
	unsigned char reg;
	int byteno = 0;

	memset(dev_id, 0, sizeof(dev_id));
	memset(manu_id, 0, sizeof(manu_id));
	memset(module_id, 0, sizeof(module_id));
	memset(fw_ver, 0, sizeof(fw_ver));
	reg = IIC_DEV_ID;
	byteno = 10;

	if (ndt_read_register(reg, dev_id, byteno) > 0) {
		pr_debug("ndt:reg=0x%02x,byteno=%d\n", reg, byteno);
	} else {
		pr_err("ndt:failed!\n");
	}

	reg = IIC_MANU_ID;
	byteno = 2;

	if (ndt_read_register(reg, manu_id, byteno) > 0) {
		pr_debug("ndt:reg=0x%02x,byteno=%d\n", reg, byteno);
	} else {
		pr_err("ndt:failed!\n");
	}

	reg = IIC_MODULE_ID;
	byteno = 2;

	if (ndt_read_register(reg, module_id, byteno) > 0) {
		pr_debug("ndt:reg=0x%02x,byteno=%d\n", reg, byteno);
	} else {
		pr_err("ndt:failed!\n");
	}

	reg = IIC_FW_VER;
	byteno = 2;

	if (ndt_read_register(reg, fw_ver, byteno) > 0) {
		pr_debug("ndt:reg=0x%02x,byteno=%d\n", reg, byteno);
	} else {
		pr_err("ndt:failed!\n");
	}

	memset(s_dev_id, 0, 30);

	for (i = 0; i < 10; i++) {
		snprintf(s_dev_id + 2 * i, PAGE_SIZE, "%02x", dev_id[i]);
	}

	ret = snprintf(buf, PAGE_SIZE, "device id:%s,manu id:%02x%02x,module id:%02x%02x,fw version:%02x%02x\n", s_dev_id, manu_id[0], manu_id[1], module_id[0], module_id[1], fw_ver[0], fw_ver[1]);
	return ret + 1;
}

static ssize_t ndt_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	int value;
	int retry = 0;
	unsigned char buffer[10];

	buffer[0] = 0x33;
	ndt_write_register(0x80, buffer, 1);
	msleep(20);
	buffer[0] = 0x00;
	buffer[1] = 0x00;
	ndt_write_register(0x82, buffer, 2);
	msleep(20);
	buffer[0] = 0x02;
	ndt_write_register(0x81, buffer, 1);
	msleep(20);
	/* check if ready*/
	do {
		ndt_read_register(0x81, buffer, 1);
		if (buffer[0] == 1 || buffer[0] == 4)
			break;
		msleep(20);
	} while (retry++ < 5);
	if (buffer[0] != 4) {
		ret = snprintf(buf, PAGE_SIZE, "failed to read back threshold");
		goto exit_error;
	}
	ndt_read_register(0x82, buffer, 4);
	value = buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
	ret = snprintf(buf, PAGE_SIZE, "threshold: %d\n", value);
exit_error:
	buffer[0] = 0x00;
	ndt_write_register(0x80, buffer, 1);
	return ret;
}

#ifdef CONFIG_INPUT_NDT_FWUPDATE
static int ndt_read_eeprom(unsigned short reg, unsigned char *datbuf, int byteno)
{
	struct i2c_msg msg[2];
	int ret;
	unsigned char reg16[2];
	int count = 0;

	if (!datbuf)
		return -EINVAL;

	reg16[0] = (reg >> 8) & 0xff;
	reg16[1] = reg & 0xff;
	msg[0].addr = g_ndt_client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = reg16;
	msg[1].addr = g_ndt_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = byteno;
	msg[1].buf = datbuf;
	do {
		ret = i2c_transfer(g_ndt_client->adapter, msg, 2);
		if (ret >= 0)
			break;
		msleep(1);
	} while (count++ < 5);

	if (ret < 0) {
		pr_err("ndt:i2c_transfer Error ! err_code:%d\n", ret);
	} else {
		pr_debug("ndt:i2c_transfer OK !\n");
	}

	return ret;
}

static int ndt_write_eeprom(unsigned short reg, unsigned char *datbuf, int byteno)
{
	unsigned char *buf;
	struct i2c_msg msg;
	int ret;
	int count = 0;

	if (!datbuf)
		return -EINVAL;

	buf = (unsigned char *)kmalloc(byteno + 2, GFP_KERNEL);

	if (!buf)
		return -EINVAL;

	memset(buf, 0, byteno + 2);
	buf[0] = (reg >> 8) & 0xff;
	buf[1] = reg & 0xff;
	memcpy(buf + 2, datbuf, byteno);
	msg.addr = g_ndt_client->addr;
	msg.flags = 0;
	msg.len = byteno + 2;
	msg.buf = buf;
	do {
		ret = i2c_transfer(g_ndt_client->adapter, &msg, 1);
		if (ret >= 0)
			break;
		msleep(1);
	} while (count++ < 5);

	if (ret < 0) {
		pr_err("ndt:i2c_master_send Error ! err_code:%d\n", ret);
	} else {
		pr_debug("ndt:i2c_master_send OK !\n");
	}

	kfree(buf);
	return ret;
}

static void ndt_fwupdate_work(struct work_struct *work)
{
	ndt_update_fw(false, "ndt_fw.bin");
}
#endif

#ifdef CONFIG_INPUT_NDT_INTERRUPT
static void ndt_abs_input_report(struct ndt_force_data *data, int pressure)
{

		if (fts_is_infod()) {
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, pressure);
			input_sync(data->input_dev);
		}
}

bool ndt_down_flag;

static void ndt_release_work(struct work_struct *work)
{
	struct ndt_force_data *data = container_of(work, struct ndt_force_data, release_work);
	int pressure = 0;
	int touch_down, x, y;

	fts_get_pointer(&touch_down, &x, &y);
	pressure =  ndt_get_pressure(touch_down, x, y);
	pr_info("%s,pressure:%d\n", __func__, pressure);
	if (pressure < RELEASE_THRE) {
		input_report_abs(data->input_dev, ABS_MT_PRESSURE, 0);
		input_sync(data->input_dev);
	}
	ndt_down_flag = false;
	/*
#ifdef CONFIG_TOUCHSCREEN_ST_CORE
	fts_senseoff_without_cal();
#endif
*/
}

static enum hrtimer_restart ndt_timer_func(struct hrtimer *timer)
{
	struct ndt_force_data *data = container_of(timer, struct ndt_force_data, release_timer);

	data->released = 1;
	queue_work(data->event_wq, &data->release_work);
	return HRTIMER_NORESTART;
}

static irqreturn_t ndt_force_interrupt(int irq, void *dev_id)
{
	struct ndt_force_data *data = (struct ndt_force_data *)dev_id;
	int pressure;
	int touch_down, x, y;

	pr_debug("%s\n", __func__);
	fts_get_pointer(&touch_down, &x, &y);
	pressure = ndt_get_pressure(touch_down, x, y);
	if (!ndt_down_flag) {
		pr_info("ndt down in the fod area,pressure:%d\n", pressure);
		ndt_down_flag = true;
	}
	/*
#ifdef CONFIG_TOUCHSCREEN_ST_CORE
	fts_senseon_without_cal();
#endif
*/
	ndt_abs_input_report(data, pressure);
	if (data->released == 1)
		hrtimer_start(&data->release_timer, ktime_set(0, 15 * 1000 * 1000), HRTIMER_MODE_REL);
	if (!data->released) {
		hrtimer_start_range_ns(&data->release_timer, ns_to_ktime(15 * 1000 * 1000), 0, HRTIMER_MODE_REL);
	}
	data->released = 0;
	return IRQ_HANDLED;
}
#endif

static int ndt_open(struct inode *inode, struct file *file)
{

	if (g_ndt_client == NULL) {
		return -EINVAL;
	} else {
		return 0;
	}
}

static int ndt_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t ndt_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	char *kbuf = NULL;
	int err = 0;
	char reg = 0;

	kbuf = kzalloc(count, GFP_KERNEL);
	if (!kbuf) {
		err = -ENOMEM;
		goto exit;
	}
	/*get reg addr buf[0]*/
	if (copy_from_user(&reg, buf, 1)) {
		err = -EFAULT;
		goto exit_kfree;
	}

	ndt_read_register(reg, kbuf, count);

	if (copy_to_user(buf + 1, kbuf, count))
		err = -EFAULT;
exit_kfree:
	kfree(kbuf);

exit:
	return err;

}

static ssize_t ndt_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	char *kbuf = NULL;
	int err = 0;
	char reg = 0;

	kbuf = kzalloc(count, GFP_KERNEL);
	if (!kbuf) {
		err = -ENOMEM;
		goto exit;
	}

	if (copy_from_user(&reg, buf, 1) || copy_from_user(kbuf, buf + 1, count)) {
		err = -EFAULT;
		goto exit_kfree;
	}

	ndt_write_register(reg, kbuf, count);
exit_kfree:
	kfree(kbuf);

exit:
	return err;
}

static const struct file_operations ndt_fops = {
	.owner		= THIS_MODULE,
	.read		= ndt_read,
	.write		= ndt_write,
	.open		= ndt_open,
	.release	= ndt_close,
};

static struct miscdevice ndt_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "ndt_i2c_node",
	.fops  = &ndt_fops,
};

static int ndt_parse_dt(struct device *dev, struct ndt_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;
	/* irq gpio */
	pdata->irq_gpio = of_get_named_gpio(np, "ndt,irq-gpio", 0);
	return 0;
}

#ifdef CONFIG_PM
static int ndt_pm_suspend(struct device *dev)
{
	struct ndt_force_data *data = dev_get_drvdata(dev);
	pr_info("%s enable ndt irq wake\n", __func__);
	enable_irq_wake(data->client->irq);
#ifdef CONFIG_INPUT_NDT_INTERRUPT
	hrtimer_cancel(&data->release_timer);
	cancel_work(&data->release_work);
#endif
	data->suspend_flag = 1;
	return 0;
}

static int ndt_pm_resume(struct device *dev)
{
	struct ndt_force_data *data = dev_get_drvdata(dev);
	pr_info("%s disable ndt irq wake\n", __func__);
	disable_irq_wake(data->client->irq);
	data->suspend_flag = 0;
	return 0;
}

static const struct dev_pm_ops ndt_dev_pm_ops = {
	.suspend = ndt_pm_suspend,
	.resume  = ndt_pm_resume,
};
#endif

static int ndt_force_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct ndt_force_data *data;
	int error = -EINVAL;
	static unsigned char g_ver[2] = {0};

	data = kzalloc(sizeof(struct ndt_force_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory for data!\n");
		return -ENOMEM;
	}

	data->pdata = kzalloc(sizeof(struct ndt_platform_data), GFP_KERNEL);
	if (!data->pdata) {
		dev_err(&client->dev, "Failed to allocate memory for data!\n");
		goto ndt_free_data;
	}

	error = ndt_parse_dt(&client->dev, data->pdata);
	if (error < 0) {
		dev_err(&client->dev, "Failed to get pdata!\n");
		goto ndt_free_pdata;
	}

	error = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!error) {
		dev_err(&client->dev, "I2C check functionality failed\n");
		goto ndt_free_pdata;
	}
	/* enable power */
	data->ndt_vdd = regulator_get(&client->dev, "ndt,vdd");
	if (IS_ERR(data->ndt_vdd)) {
		dev_err(&client->dev, "failed to get ndt vdd");
		goto ndt_free_pdata;
	}
	error = regulator_enable(data->ndt_vdd);
	if (error < 0) {
		dev_err(&client->dev, "Failed to enable power regulator\n");
		goto ndt_free_pdata;
	}

	data->ndt_vddio = regulator_get(&client->dev, "ndt,vddio");
	if (IS_ERR(data->ndt_vddio)) {
		dev_err(&client->dev, "failed to get ndt vdd");
		goto ndt_free_pdata;
	}
	error = regulator_enable(data->ndt_vddio);
	if (error < 0) {
		dev_err(&client->dev, "Failed to enable power regulator\n");
		goto ndt_free_pdata;
	}

	if (data->pdata->irq_gpio) {
		error = gpio_request(data->pdata->irq_gpio, "ndt_force_irq_gpio");
		if (error) {
			dev_err(&client->dev, "Unable to request gpio [%d]\n", data->pdata->irq_gpio);
			goto ndt_vddio_disable;
		}
		error = gpio_direction_input(data->pdata->irq_gpio);
		if (error) {
			dev_err(&client->dev, "unable to set direction for gpio [%d]\n", data->pdata->irq_gpio);
			goto ndt_free_irq_gpio;
		}
		client->irq = gpio_to_irq(data->pdata->irq_gpio);
	}
	data->client = client;
	i2c_set_clientdata(data->client, data);
	g_ndt_client = client;
	data->fw_updated = false;

	/* Initialize input device */
	data->input_dev = input_allocate_device();

	if (!data->input_dev) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		goto ndt_free_irq_gpio;
	}
	data->input_dev->name = "ndt_force";
	data->input_dev->id.bustype = BUS_I2C;
	data->input_dev->dev.parent = &data->client->dev;
	input_set_capability(data->input_dev, EV_KEY, KEY_POWER);
	input_set_abs_params(data->input_dev, ABS_MT_PRESSURE, 0, 2048, 0, 0);
	input_set_drvdata(data->input_dev, data);
	dev_set_drvdata(&data->client->dev, data);
	__set_bit(EV_SYN, data->input_dev->evbit);
	__set_bit(EV_KEY, data->input_dev->evbit);
	error = input_register_device(data->input_dev);

	if (error) {
		dev_err(&client->dev, "Unable to register input device, error: %d\n", error);
		goto ndt_free_irq_gpio;
	}

	msleep(10);
	/*i2c test *, if i2c error ,maybe fw  is wrong ,so goon*/
	error = ndt_read_register(0x05, g_ver, 2);
	if (error < 0) {
		pr_err("ndt:i2c test error, maybe fw fail, force update fw\n");
		error = ndt_update_fw(1, "ndt_fw.bin");
		if (error == -EINVAL) {
			pr_err("ndt:i2c error after fw update, unregister device\n");
			goto ndt_free_input_dev;
		} else {
			pr_err("i2c is ok after fw update\n");
			data->fw_updated = true;
		}
	} else
		pr_info("ndt:i2c test ok,g_ver:0x%x:0x%x\n", g_ver[0], g_ver[1]);
	error = sysfs_create_group(&client->dev.kobj, &ndt_attr_group);

	if (error)
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);

	data->event_wq = create_singlethread_workqueue("ndt-event-queue");
#ifdef CONFIG_INPUT_NDT_FWUPDATE
	INIT_WORK(&data->fwupdate_work, ndt_fwupdate_work);
	schedule_work(&data->fwupdate_work);
#endif
#ifdef CONFIG_INPUT_NDT_INTERRUPT
	INIT_WORK(&data->release_work, ndt_release_work);
	hrtimer_init(&data->release_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->release_timer.function = ndt_timer_func;
	data->released = 1;
	error = request_threaded_irq(client->irq, NULL, ndt_force_interrupt,
				     IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->dev.driver->name, data);

	if (error) {
		dev_err(&client->dev, "Error %d registering irq\n", error);
		goto ndt_free_input_dev;
	}
#endif
	misc_register(&ndt_misc);
	ndt_input_dev = data->input_dev;
	pr_info("%s,probe ok\n", __func__);
	return 0;
ndt_free_input_dev:
	input_unregister_device(data->input_dev);
	g_ndt_client = NULL;
ndt_free_irq_gpio:
	gpio_free(data->pdata->irq_gpio);
ndt_vddio_disable:
	regulator_disable(data->ndt_vddio);
ndt_free_pdata:
	kfree(data->pdata);
	data->pdata = NULL;
ndt_free_data:
	kfree(data);
	data = NULL;
	return error;
}

static const struct i2c_device_id cyt_id[] = {
	{"ndt_press", 0},
	{ },
};

static struct of_device_id ndt_match_table[] = {
	{ .compatible = "ndt,press",},
	{ },
};

static int ndt_force_remove(struct i2c_client *client)
{
	struct ndt_force_data *data = i2c_get_clientdata(client);
	struct ndt_platform_data *pdata = data->pdata;

	misc_deregister(&ndt_misc);
	free_irq(client->irq, data);
	if (data->input_dev != NULL)
		input_unregister_device(data->input_dev);

	gpio_free(pdata->irq_gpio);
	if (pdata != NULL) {
		kfree(pdata);
		pdata = NULL;
	}
	if (data != NULL) {
		kfree(data);
		data = NULL;
	}
	return 0;
}

static struct i2c_driver ndt_force_driver = {
	.driver = {
		.name	= "ndt_press",
		.owner	= THIS_MODULE,
		.of_match_table = ndt_match_table,
#ifdef CONFIG_PM
		.pm = &ndt_dev_pm_ops,
#endif
	},
	.probe		= ndt_force_probe,
	.remove		= ndt_force_remove,
	.id_table	= cyt_id,
};

static int __init ndt_force_init(void)
{
	return i2c_add_driver(&ndt_force_driver);
}

static void __exit ndt_force_exit(void)
{
	i2c_del_driver(&ndt_force_driver);
}

module_init(ndt_force_init);
module_exit(ndt_force_exit);

MODULE_AUTHOR("Liuyinghong <liuyinghong@xiaomi.com>");
MODULE_DESCRIPTION("Ndt Press Driver");
MODULE_LICENSE("GPL");


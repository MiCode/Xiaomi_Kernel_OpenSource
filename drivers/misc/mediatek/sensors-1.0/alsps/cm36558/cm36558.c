/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * Author: yucong xiong <yucong.xion@mediatek.com>
 *
 */

#define pr_fmt(fmt) "<CM36558> " fmt

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "cust_alsps.h"
#include "cm36558.h"
#include "alsps.h"

#define CM36558_DEV_NAME "CM36558"

#define I2C_FLAG_WRITE 0
#define I2C_FLAG_READ 1
/*----------------------------------------------------------------------------*/
static int CM36558_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id);
static int CM36558_i2c_remove(struct i2c_client *client);
static int CM36558_i2c_detect(struct i2c_client *client,
			      struct i2c_board_info *info);
static int CM36558_i2c_suspend(struct device *dev);
static int CM36558_i2c_resume(struct device *dev);

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id CM36558_i2c_id[] = {
			{CM36558_DEV_NAME, 0}, {} };
static unsigned long long int_top_time;
/*----------------------------------------------------------------------------*/
struct CM36558_priv {
	struct alsps_hw hw;
	struct i2c_client *client;
	struct work_struct eint_work;

	/*misc */
	u16 als_modulus;
	atomic_t i2c_retry;
	atomic_t als_suspend;
	atomic_t als_debounce; /*debounce time after enabling als */
	atomic_t als_deb_on;   /*indicates if the debounce is on */
	atomic_t als_deb_end;  /*the jiffies representing the end of debounce */
	atomic_t ps_mask;      /*mask ps: always return far away */
	atomic_t ps_debounce;  /*debounce time after enabling ps */
	atomic_t ps_deb_on;    /*indicates if the debounce is on */
	atomic_t ps_deb_end;   /*the jiffies representing the end of debounce */
	atomic_t ps_suspend;
	atomic_t trace;
	atomic_t init_done;
	struct device_node *irq_node;
	int irq;

	/*data */
	u16 als;
	u16 ps;
	u8 _align;
	u16 als_level_num;
	u16 als_value_num;
	u32 als_level[C_CUST_ALS_LEVEL - 1];
	u32 als_value[C_CUST_ALS_LEVEL];
	int ps_cali;

	atomic_t als_cmd_val; /*the cmd value can't be read, stored in ram */
	atomic_t ps_cmd_val;  /*the cmd value can't be read, stored in ram */

	atomic_t
		ps_thd_val_high; /*the cmd value can't be read, stored in ram */
	atomic_t ps_thd_val_low; /*the cmd value can't be read, stored in ram */

	atomic_t
		als_thd_val_high; /*cmd value can't be read, stored in ram*/
	atomic_t
		als_thd_val_low; /*cmd value can't be read, stored in ram */
	atomic_t ps_thd_val;
	ulong enable;       /*enable mask */
	ulong pending_intr; /*pending interrupt */
};

#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,alsps"}, {},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops CM36558_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(CM36558_i2c_suspend, CM36558_i2c_resume)};
#endif

static struct i2c_driver CM36558_i2c_driver = {
	.probe = CM36558_i2c_probe,
	.remove = CM36558_i2c_remove,
	.detect = CM36558_i2c_detect,
	.id_table = CM36558_i2c_id,
	.driver = {

			.name = CM36558_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
			.pm = &CM36558_pm_ops,
#endif
#ifdef CONFIG_OF
			.of_match_table = alsps_of_match,
#endif
		},
};


struct PS_CALI_DATA_STRUCT {
	int close;
	int far_away;
	int valid;
};

static struct i2c_client *CM36558_i2c_client;
static struct CM36558_priv *CM36558_obj;

static int CM36558_local_init(void);
static int CM36558_remove(void);
static int CM36558_init_flag = -1;
static struct alsps_init_info CM36558_init_info = {
	.name = "CM36558",
	.init = CM36558_local_init,
	.uninit = CM36558_remove,

};

static DEFINE_MUTEX(CM36558_mutex);

enum { CMC_BIT_ALS = 1,
CMC_BIT_PS = 2,
} CMC_BIT;

enum { CMC_TRC_ALS_DATA = 0x0001,
CMC_TRC_PS_DATA = 0x0002,
CMC_TRC_EINT = 0x0004,
CMC_TRC_IOCTL = 0x0008,
CMC_TRC_I2C = 0x0010,
CMC_TRC_CVT_ALS = 0x0020,
CMC_TRC_CVT_PS = 0x0040,
CMC_TRC_CVT_AAL = 0x0080,
CMC_TRC_DEBUG = 0x8000,
} CMC_TRC;

int CM36558_i2c_master_operate(struct i2c_client *client, char *buf, int count,
			       int i2c_flag)
{
	int res = 0;
#ifndef CONFIG_MTK_I2C_EXTENSION
	struct i2c_msg msg[2];
#endif
	mutex_lock(&CM36558_mutex);
	switch (i2c_flag) {
	case I2C_FLAG_WRITE:
#ifdef CONFIG_MTK_I2C_EXTENSION
		client->addr &= I2C_MASK_FLAG;
		res = i2c_master_send(client, buf, count);
		client->addr &= I2C_MASK_FLAG;
#else
		res = i2c_master_send(client, buf, count);
#endif
		break;

	case I2C_FLAG_READ:
#ifdef CONFIG_MTK_I2C_EXTENSION
		client->addr &= I2C_MASK_FLAG;
		client->addr |= I2C_WR_FLAG;
		client->addr |= I2C_RS_FLAG;
		res = i2c_master_send(client, buf, (count << 8) | 1);
		client->addr &= I2C_MASK_FLAG;
#else
		msg[0].addr = client->addr;
		msg[0].flags = 0;
		msg[0].len = 1;
		msg[0].buf = buf;

		msg[1].addr = client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = count;
		msg[1].buf = buf;
		res = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
#endif
		break;
	default:
		pr_debug("CM36558_i2c_master_operate i2c_flag not support!\n");
		break;
	}
	if (res < 0)
		goto EXIT_ERR;
	mutex_unlock(&CM36558_mutex);
	return res;
EXIT_ERR:
	mutex_unlock(&CM36558_mutex);
	pr_err("CM36558_i2c_master_operate fail\n");
	return res;
}

/********************************************************************/
int CM36558_enable_ps(struct i2c_client *client, int enable)
{
	struct CM36558_priv *obj = i2c_get_clientdata(client);
	int res = 0;
	u8 databuf[3];

	if (enable == 1) {

		pr_debug("CM36558_enable_ps enable_ps\n");
		databuf[0] = CM36558_REG_PS_CONF1_2;
		res = CM36558_i2c_master_operate(client, databuf, 2,
						 I2C_FLAG_READ);
		if (res < 0) {
			pr_err("i2c_master_send function err\n");
			goto ENABLE_PS_EXIT_ERR;
		}
		pr_debug("CM36558_REG_PS_CONF1_2 valuelow = %x, high = %x\n",
			databuf[0], databuf[1]);
		databuf[2] = databuf[1];
		databuf[1] = databuf[0] & 0xFE;

		databuf[0] = CM36558_REG_PS_CONF1_2;
		res = CM36558_i2c_master_operate(client, databuf, 0x3,
						 I2C_FLAG_WRITE);
		if (res < 0) {
			pr_err("i2c_master_send function err\n");
			goto ENABLE_PS_EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 1);
		atomic_set(
			&obj->ps_deb_end,
			jiffies + atomic_read(&obj->ps_debounce) / (1000 / HZ));

	} else {
		pr_debug("CM36558_enable_ps disable_ps\n");
		databuf[0] = CM36558_REG_PS_CONF1_2;
		res = CM36558_i2c_master_operate(client, databuf, 2,
						 I2C_FLAG_READ);
		if (res < 0) {
			pr_err("i2c_master_send function err\n");
			goto ENABLE_PS_EXIT_ERR;
		}

		pr_debug("CM36558_REG_PS_CONF1_2 valuelow = %x,high = %x\n",
			databuf[0], databuf[1]);

		databuf[2] = databuf[1];
		databuf[1] = databuf[0] | 0x01;
		databuf[0] = CM36558_REG_PS_CONF1_2;

		res = CM36558_i2c_master_operate(client, databuf, 0x3,
						 I2C_FLAG_WRITE);
		if (res < 0) {
			pr_err("i2c_master_send function err\n");
			goto ENABLE_PS_EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 0);
	}

	return 0;
ENABLE_PS_EXIT_ERR:
	return res;
}

/********************************************************************/
int CM36558_enable_als(struct i2c_client *client, int enable)
{
	struct CM36558_priv *obj = i2c_get_clientdata(client);
	int res = 0;
	u8 databuf[3];

	if (enable == 1) {
		pr_debug("CM36558_enable_als enable_als\n");
		databuf[0] = CM36558_REG_ALS_UV_CONF;
		res = CM36558_i2c_master_operate(client, databuf, 2,
						 I2C_FLAG_READ);
		if (res < 0) {
			pr_err("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}

		pr_debug("CM36558_REG_ALS_UV_CONF low= %x,high = %x\n",
			databuf[0], databuf[1]);

		databuf[2] = databuf[1];
		databuf[1] = databuf[0] & 0xFE;
		databuf[0] = CM36558_REG_ALS_UV_CONF;

		res = CM36558_i2c_master_operate(client, databuf, 0x3,
						 I2C_FLAG_WRITE);
		if (res < 0) {
			pr_err("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}
		atomic_set(&obj->als_deb_on, 1);
		atomic_set(&obj->als_deb_end,
			   jiffies +
				   atomic_read(&obj->als_debounce) /
					   (1000 / HZ));
	} else {
		pr_debug("CM36558_enable_als disable_als\n");
		databuf[0] = CM36558_REG_ALS_UV_CONF;
		res = CM36558_i2c_master_operate(client, databuf, 2,
						 I2C_FLAG_READ);
		if (res < 0) {
			pr_err("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}

		pr_debug("CM36558_REG_ALS_UV_CONF valuelow = %x, high = %x\n",
			databuf[0], databuf[1]);

		databuf[2] = databuf[1];
		databuf[1] = databuf[0] | 0x01;
		databuf[0] = CM36558_REG_ALS_UV_CONF;

		res = CM36558_i2c_master_operate(client, databuf, 0x3,
						 I2C_FLAG_WRITE);
		if (res < 0) {
			pr_err("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}
		atomic_set(&obj->als_deb_on, 0);
	}
	return 0;
ENABLE_ALS_EXIT_ERR:
	return res;
}

/********************************************************************/
long CM36558_read_ps(struct i2c_client *client, u16 *data)
{
	long res = 0;
	u8 databuf[2];
	struct CM36558_priv *obj = i2c_get_clientdata(client);

	databuf[0] = CM36558_REG_PS_DATA;
	res = CM36558_i2c_master_operate(client, databuf, 2, I2C_FLAG_READ);
	if (res < 0) {
		pr_err("i2c_master_send function err\n");
		goto READ_PS_EXIT_ERR;
	}
	if (atomic_read(&obj->trace) & CMC_TRC_DEBUG)
		pr_debug("CM36558_REG_PS_DATA valuelow = %x,high = %x\n",
			databuf[0], databuf[1]);

	*data = ((databuf[1] << 8) | databuf[0]);
	if (*data < obj->ps_cali)
		*data = 0;
	else
		*data = *data - obj->ps_cali;
	return 0;
READ_PS_EXIT_ERR:
	return res;
}


long CM36558_read_als(struct i2c_client *client, u16 *data)
{
	long res = 0;
	u8 databuf[2];
	struct CM36558_priv *obj = i2c_get_clientdata(client);

	databuf[0] = CM36558_REG_ALS_DATA;
	res = CM36558_i2c_master_operate(client, databuf, 2, I2C_FLAG_READ);
	if (res < 0) {
		pr_err("i2c_master_send function err\n");
		goto READ_ALS_EXIT_ERR;
	}

	if (atomic_read(&obj->trace) & CMC_TRC_DEBUG)
		pr_debug("CM36558_REG_ALS_DATA value: %d\n",
			((databuf[1] << 8) | databuf[0]));

	*data = ((databuf[1] << 8) | databuf[0]);

	return 0;
READ_ALS_EXIT_ERR:
	return res;
}

/********************************************************************/
static int CM36558_get_ps_value(struct CM36558_priv *obj, u8 ps)
{
	int val = 0, mask = atomic_read(&obj->ps_mask);
	int invalid = 0;

	val = 0;

	if (ps > atomic_read(&obj->ps_thd_val_high))
		val = 0;
	else if (ps < atomic_read(&obj->ps_thd_val_low))
		val = 1;

	if (atomic_read(&obj->ps_suspend)) {
		invalid = 1;
	} else if (atomic_read(&obj->ps_deb_on) == 1) {
		unsigned long endt = atomic_read(&obj->ps_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->ps_deb_on, 0);

		if (atomic_read(&obj->ps_deb_on) == 1)
			invalid = 1;
	}

	if (!invalid) {
		if (unlikely(atomic_read(&obj->trace) & CMC_TRC_CVT_PS)) {
			if (mask)
				pr_debug("PS:  %05d => %05d [M]\n", ps, val);
			else
				pr_debug("PS:  %05d => %05d\n", ps, val);
		}
		if (test_bit(CMC_BIT_PS, &obj->enable) == 0) {
			pr_debug("PS:not enable, do not report this value\n");
			return -1;
		} else {
			return val;
		}

	} else {
		if (unlikely(atomic_read(&obj->trace) & CMC_TRC_CVT_PS))
			pr_debug("PS:  %05d => %05d (-1)\n", ps, val);
		return -1;
	}
}

static int CM36558_get_als_value(struct CM36558_priv *obj, u16 als)
{
	int idx = 0;
	int invalid = 0;
	int level_high = 0;
	int level_low = 0;
	int level_diff = 0;
	int value_high = 0;
	int value_low = 0;
	int value_diff = 0;
	int value = 0;

	if ((obj->als_level_num == 0) || (obj->als_value_num == 0)) {
		pr_err("invalid als_level_num = %d, als_value_num = %d\n",
			   obj->als_level_num, obj->als_value_num);
		return -1;
	}
	if (atomic_read(&obj->als_deb_on) == 1) {
		unsigned long endt = atomic_read(&obj->als_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->als_deb_on, 0);

		if (atomic_read(&obj->als_deb_on) == 1)
			invalid = 1;
	}
	for (idx = 0; idx < obj->als_level_num; idx++) {
		if (als < obj->hw.als_level[idx])
			break;
	}
	if (idx >= obj->als_level_num || idx >= obj->als_value_num) {
		if (idx < obj->als_value_num)
			value = obj->hw.als_value[idx - 1];
		else
			value = obj->hw.als_value[obj->als_value_num - 1];
	} else {
		level_high = obj->hw.als_level[idx];
		level_low = (idx > 0) ? obj->hw.als_level[idx - 1] : 0;
		level_diff = level_high - level_low;
		value_high = obj->hw.als_value[idx];
		value_low = (idx > 0) ? obj->hw.als_value[idx - 1] : 0;
		value_diff = value_high - value_low;

		if ((level_low >= level_high) || (value_low >= value_high))
			value = value_low;
		else
			value = (level_diff * value_low +
				 (als - level_low) * value_diff +
				 ((level_diff + 1) >> 1)) /
				level_diff;
	}

	if (!invalid) {
		if (atomic_read(&obj->trace) & CMC_TRC_CVT_AAL)
			pr_debug("ALS: %d [%d, %d] => %d [%d, %d]\n", als,
				level_low, level_high, value, value_low,
				value_high);

	} else {
		if (atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)
			pr_debug("ALS: %05d => %05d (-1)\n", als, value);

		return -1;
	}
	return value;
}

static ssize_t CM36558_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}

	res = snprintf(
		buf, PAGE_SIZE,
		"(%d %d %d %d %d\n)threadhold_low=%d threadhold_high=%d\n",
		atomic_read(&CM36558_obj->i2c_retry),
		atomic_read(&CM36558_obj->als_debounce),
		atomic_read(&CM36558_obj->ps_mask),
		atomic_read(&CM36558_obj->ps_thd_val),
		atomic_read(&CM36558_obj->ps_debounce),
		atomic_read(&CM36558_obj->ps_thd_val_low),
		atomic_read(&CM36558_obj->ps_thd_val_high));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_store_config(struct device_driver *ddri, const char *buf,
				    size_t count)
{
	int retry = 0, als_deb = 0, ps_deb = 0, mask = 0, thres = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "%d %d %d %d %d", &retry, &als_deb, &mask, &thres,
		   &ps_deb) == 5) {
		atomic_set(&CM36558_obj->i2c_retry, retry);
		atomic_set(&CM36558_obj->als_debounce, als_deb);
		atomic_set(&CM36558_obj->ps_mask, mask);
		atomic_set(&CM36558_obj->ps_thd_val, thres);
		atomic_set(&CM36558_obj->ps_debounce, ps_deb);
	} else {
		pr_err("invalid content: '%s', length = %d\n", buf,
			   (unsigned int)count);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n",
		       atomic_read(&CM36558_obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_store_trace(struct device_driver *ddri, const char *buf,
				   size_t count)
{
	int trace = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&CM36558_obj->trace, trace);
	else
		pr_err("invalid content: '%s', length = %d\n", buf,
			   (unsigned int)count);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_als(struct device_driver *ddri, char *buf)
{
	int res = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}
	res = CM36558_read_als(CM36558_obj->client, &CM36558_obj->als);
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", CM36558_obj->als);
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_ps(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;

	if (!CM36558_obj) {
		pr_err("cm3623_obj is null!!\n");
		return 0;
	}
	res = CM36558_read_ps(CM36558_obj->client, &CM36558_obj->ps);
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n",
				(unsigned int)res);
	else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", CM36558_obj->ps);
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_reg(struct device_driver *ddri, char *buf)
{
	u8 _bIndex = 0;
	u8 databuf[2] = {0};
	ssize_t _tLength = 0;
	int res = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}

	for (_bIndex = 0; _bIndex < 0x0E; _bIndex++) {
		databuf[0] = _bIndex;
		res = CM36558_i2c_master_operate(CM36558_obj->client, databuf,
						 2, I2C_FLAG_READ);
		if (res < 0)
			pr_err("CM36558_i2c_master_operate err res = %d\n",
				   res);

		_tLength += snprintf((buf + _tLength), (PAGE_SIZE - _tLength),
				     "Reg[0x%02X]: 0x%04X\n", _bIndex,
				     databuf[0] | databuf[1] << 8);
	}

	return _tLength;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_send(struct device_driver *ddri, char *buf)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_store_send(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	int addr = 0, cmd = 0;
	u8 dat = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	} else if (sscanf(buf, "%x %x", &addr, &cmd) != 2) {
		pr_err("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8)cmd;
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_recv(struct device_driver *ddri, char *buf)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_store_recv(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	int addr = 0, err = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}
	err = kstrtoint(buf, 16, &addr);
	if (err != 0) {
		pr_err("invalid format: '%s'\n", buf);
		return 0;
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d, (%d %d)\n",
			CM36558_obj->hw.i2c_num, CM36558_obj->hw.power_id,
			CM36558_obj->hw.power_vol);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"REGS: %02X %02X %02X %02lX %02lX\n",
			atomic_read(&CM36558_obj->als_cmd_val),
			atomic_read(&CM36558_obj->ps_cmd_val),
			atomic_read(&CM36558_obj->ps_thd_val),
			CM36558_obj->enable, CM36558_obj->pending_intr);

	len += snprintf(buf + len, PAGE_SIZE - len, "MISC: %d %d\n",
			atomic_read(&CM36558_obj->als_suspend),
			atomic_read(&CM36558_obj->ps_suspend));

	return len;
}

/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct CM36558_priv *obj, const char *buf,
			     size_t count, u32 data[], int len)
{
	int idx = 0, err = 0;
	char *cur = (char *)buf, *end = (char *)(buf + count);

	while (idx < len) {
		while ((cur < end) && IS_SPACE(*cur))
			cur++;
		err = kstrtoint(cur, 10, &data[idx]);
		if (err != 0)
			break;

		idx++;
		while ((cur < end) && !IS_SPACE(*cur))
			cur++;
	}
	return idx;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < CM36558_obj->als_level_num; idx++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d ",
				CM36558_obj->hw.als_level[idx]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_store_alslv(struct device_driver *ddri, const char *buf,
				   size_t count)
{
	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def")) {
		memcpy(CM36558_obj->als_level, CM36558_obj->hw.als_level,
		       sizeof(CM36558_obj->als_level));
	} else if (CM36558_obj->als_level_num !=
		   read_int_from_buf(CM36558_obj, buf, count,
				     CM36558_obj->hw.als_level,
				     CM36558_obj->als_level_num)) {
		pr_err("invalid format: '%s'\n", buf);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < CM36558_obj->als_value_num; idx++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d ",
				CM36558_obj->hw.als_value[idx]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t CM36558_store_alsval(struct device_driver *ddri, const char *buf,
				    size_t count)
{
	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def")) {
		memcpy(CM36558_obj->als_value, CM36558_obj->hw.als_value,
		       sizeof(CM36558_obj->als_value));
	} else if (CM36558_obj->als_value_num !=
		   read_int_from_buf(CM36558_obj, buf, count,
				     CM36558_obj->hw.als_value,
				     CM36558_obj->als_value_num)) {
		pr_err("invalid format: '%s'\n", buf);
	}
	return count;
}

static DRIVER_ATTR(als, 0644, CM36558_show_als, NULL);
static DRIVER_ATTR(ps, 0644, CM36558_show_ps, NULL);
static DRIVER_ATTR(config, 0644, CM36558_show_config,
		   CM36558_store_config);
static DRIVER_ATTR(alslv, 0644, CM36558_show_alslv,
		   CM36558_store_alslv);
static DRIVER_ATTR(alsval, 0644, CM36558_show_alsval,
		   CM36558_store_alsval);
static DRIVER_ATTR(trace, 0644, CM36558_show_trace,
		   CM36558_store_trace);
static DRIVER_ATTR(status, 0644, CM36558_show_status, NULL);
static DRIVER_ATTR(send, 0644, CM36558_show_send,
		   CM36558_store_send);
static DRIVER_ATTR(recv, 0644, CM36558_show_recv,
		   CM36558_store_recv);
static DRIVER_ATTR(reg, 0644, CM36558_show_reg, NULL);

static struct driver_attribute *CM36558_attr_list[] = {
	&driver_attr_als,
	&driver_attr_ps,
	&driver_attr_trace, /*trace log */
	&driver_attr_config,
	&driver_attr_alslv,
	&driver_attr_alsval,
	&driver_attr_status,
	&driver_attr_send,
	&driver_attr_recv,
	&driver_attr_reg,
};


static int CM36558_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(CM36558_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, CM36558_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				   CM36558_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int CM36558_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(CM36558_attr_list));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, CM36558_attr_list[idx]);

	return err;
}
static int intr_flag;
/*----------------------------------------------------------------------------*/
static int CM36558_check_intr(struct i2c_client *client)
{
	int res = 0;
	u8 databuf[2];

	databuf[0] = CM36558_REG_PS_DATA;
	res = CM36558_i2c_master_operate(client, databuf, 2, I2C_FLAG_READ);
	if (res < 0) {
		pr_err("i2c_master_send function err res = %d\n", res);
		goto EXIT_ERR;
	}

	pr_info(
		"CM36558_REG_PS_DATA value value_low = %x, value_reserve = %x\n",
		databuf[0], databuf[1]);

	databuf[0] = CM36558_REG_INT_FLAG;
	res = CM36558_i2c_master_operate(client, databuf, 2, I2C_FLAG_READ);
	if (res < 0) {
		pr_err("i2c_master_send function err res = %d\n", res);
		goto EXIT_ERR;
	}

	pr_info("CM36558_REG_INT_FLAG value value_low = %x, value_high = %x\n",
		 databuf[0], databuf[1]);

	if (databuf[1] & 0x02) {
		intr_flag = 0;
	} else if (databuf[1] & 0x01) {
		intr_flag = 1;
	} else {
		res = -1;
		pr_err("CM36558_check_intr fail databuf[1]&0x01: %d\n",
			   res);
		goto EXIT_ERR;
	}

	return 0;
EXIT_ERR:
	pr_err("CM36558_check_intr dev: %d\n", res);
	return res;
}


static void CM36558_eint_work(struct work_struct *work)
{
	struct CM36558_priv *obj = (struct CM36558_priv *)container_of(work,
		struct CM36558_priv, eint_work);
	int res = 0;

	pr_info("CM36558 int top half time = %lld\n", int_top_time);

	res = CM36558_check_intr(obj->client);
	if (res != 0) {
		goto EXIT_INTR_ERR;
	} else {
		pr_debug("CM36558 interrupt value = %d\n", intr_flag);
		res = ps_report_interrupt_data(intr_flag);
	}
#if defined(CONFIG_OF)
	enable_irq(obj->irq);
#elif defined(CUST_EINT_ALS_TYPE)
	mt_eint_unmask(CUST_EINT_ALS_NUM);
#else
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
#endif
	return;
EXIT_INTR_ERR:
#if defined(CONFIG_OF)
	enable_irq(obj->irq);
#elif defined(CUST_EINT_ALS_TYPE)
	mt_eint_unmask(CUST_EINT_ALS_NUM);
#else
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
#endif
	pr_err("CM36558_eint_work err: %d\n", res);
}

/*----------------------------------------------------------------------------*/
static void CM36558_eint_func(void)
{
	struct CM36558_priv *obj = CM36558_obj;

	if (!obj)
		return;
	int_top_time = sched_clock();
	schedule_work(&obj->eint_work);
}

#if defined(CONFIG_OF)
static irqreturn_t CM36558_eint_handler(int irq, void *desc)
{
	CM36558_eint_func();
	disable_irq_nosync(CM36558_obj->irq);

	return IRQ_HANDLED;
}
#endif
/*----------------------------------------------------------------------------*/
int CM36558_setup_eint(struct i2c_client *client)
{
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;
	u32 ints[2] = {0, 0};

	/* gpio setting */
	pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		pr_err("Cannot find alsps pinctrl!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		pr_err("Cannot find alsps pinctrl default!\n");
	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		pr_err("Cannot find alsps pinctrl pin_cfg!\n");
		return ret;
	}
	pinctrl_select_state(pinctrl, pins_cfg);
	/* eint request */
	if (CM36558_obj->irq_node) {
#ifndef CONFIG_MTK_EIC
		/*upstream code*/
		ints[0] = of_get_named_gpio(CM36558_obj->irq_node,
				"deb-gpios", 0);
		if (ints[0] < 0) {
			pr_err("debounce gpio not found\n");
		} else{
			ret = of_property_read_u32(CM36558_obj->irq_node,
						"debounce", &ints[1]);
			if (ret < 0)
				pr_err("debounce time not found\n");
			else
				gpio_set_debounce(ints[0], ints[1]);
			pr_debug("in[0]:%d, in[1]:%d!!\n", ints[0], ints[1]);
		}
#else
		ret = of_property_read_u32_array(CM36558_obj->irq_node,
				"debounce", ints, ARRAY_SIZE(ints));
		if (ret) {
			pr_err("of_property_read_u32_array fail: %d\n", ret);
			return ret;
		}
		gpio_set_debounce(ints[0], ints[1]);
		pr_debug("in[0] = %d, in[1] = %d!!\n", ints[0], ints[1]);
#endif

		CM36558_obj->irq =
			irq_of_parse_and_map(CM36558_obj->irq_node, 0);
		pr_debug("CM36558_obj->irq = %d\n", CM36558_obj->irq);
		if (!CM36558_obj->irq) {
			pr_err("irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}

		if (request_irq(CM36558_obj->irq, CM36558_eint_handler,
				IRQF_TRIGGER_NONE, "ALS-eint", NULL)) {
			pr_err("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}

		enable_irq_wake(CM36558_obj->irq);
		enable_irq(CM36558_obj->irq);
	} else {
		pr_err("null irq node!!\n");
		return -EINVAL;
	}

	return 0;
}


static int set_psensor_threshold(struct i2c_client *client)
{
	struct CM36558_priv *obj = i2c_get_clientdata(client);
	int res = 0;
	u8 databuf[3];

	pr_info("set_psensor_threshold function high: 0x%x, low:0x%x\n",
		 atomic_read(&obj->ps_thd_val_high),
		 atomic_read(&obj->ps_thd_val_low));
	databuf[0] = CM36558_REG_PS_THDL;
	databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_low) & 0xFF);
	databuf[2] = (u8)(atomic_read(&obj->ps_thd_val_low) >> 8);
	res = CM36558_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		pr_err("i2c_master_send function err\n");
		return -1;
	}
	databuf[0] = CM36558_REG_PS_THDH;
	databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_high) & 0xFF);
	databuf[2] = (u8)(atomic_read(&obj->ps_thd_val_high) >> 8);
	res = CM36558_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		pr_err("i2c_master_send function err\n");
		return -1;
	}
	return 0;
}

static int CM36558_init_client(struct i2c_client *client)
{
	struct CM36558_priv *obj = i2c_get_clientdata(client);
	u8 databuf[3];
	int res = 0;

	pr_debug("%s\n", __func__);
	databuf[0] = CM36558_REG_ALS_UV_CONF;
	if (obj->hw.polling_mode_als == 1)
		databuf[1] = 0x01;
	else
		databuf[1] = 0x03;
	databuf[2] = 0x01;
	res = CM36558_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		pr_err("i2c_master_send function err\n");
		goto EXIT_ERR;
	}
	pr_debug("CM36558 ps CM36558_REG_ALS_UV_CONF command!\n");

	databuf[0] = CM36558_REG_PS_CONF1_2;
	databuf[1] = 0x01;
	if (obj->hw.polling_mode_ps == 1)
		databuf[2] = 0x00;
	else
		databuf[2] = 0x03;
	res = CM36558_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		pr_err("i2c_master_send function err\n");
		goto EXIT_ERR;
	}
	pr_debug("CM36558 ps CM36558_REG_PS_CONF1_2 command!\n");

	databuf[0] = CM36558_REG_PS_CONF3_MS;
	databuf[1] = 0x30 /*0b00110000*/;
	databuf[2] = 0x02 /*0b00000010*/;
	res = CM36558_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		pr_err("i2c_master_send function err\n");
		goto EXIT_ERR;
	}
	pr_debug("CM36558 ps CM36558_REG_PS_CONF3_MS command!\n");
	databuf[0] = CM36558_REG_PS_CANC;
	databuf[1] = 0x00;
	databuf[2] = 0x00;
	res = CM36558_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		pr_err("i2c_master_send function err\n");
		goto EXIT_ERR;
	}

	pr_debug("CM36558 ps CM36558_REG_PS_CANC command!\n");

	if (obj->hw.polling_mode_als == 0) {
		databuf[0] = CM36558_REG_ALS_THDH;
		databuf[1] = (u8)(atomic_read(&obj->als_thd_val_high) & 0xFF);
		databuf[2] = (u8)(atomic_read(&obj->als_thd_val_high) >> 8);
		res = CM36558_i2c_master_operate(client, databuf, 0x3,
						 I2C_FLAG_WRITE);
		if (res <= 0) {
			pr_err("i2c_master_send function err\n");
			goto EXIT_ERR;
		}
		databuf[0] = CM36558_REG_ALS_THDL;
		databuf[1] = (u8)(atomic_read(&obj->als_thd_val_low) & 0xFF);
		databuf[2] = (u8)(atomic_read(&obj->als_thd_val_low) >> 8);
		res = CM36558_i2c_master_operate(client, databuf, 0x3,
						 I2C_FLAG_WRITE);
		if (res <= 0) {
			pr_err("i2c_master_send function err\n");
			goto EXIT_ERR;
		}
	}
	if (obj->hw.polling_mode_ps == 0) {
		databuf[0] = CM36558_REG_PS_THDL;
		databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_low) & 0xFF);
		databuf[2] = (u8)(atomic_read(&obj->ps_thd_val_low) >> 8);
		res = CM36558_i2c_master_operate(client, databuf, 0x3,
						 I2C_FLAG_WRITE);
		if (res <= 0) {
			pr_err("i2c_master_send function err\n");
			goto EXIT_ERR;
		}
		databuf[0] = CM36558_REG_PS_THDH;
		databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_high) & 0xFF);
		databuf[2] = (u8)(atomic_read(&obj->ps_thd_val_high) >> 8);
		res = CM36558_i2c_master_operate(client, databuf, 0x3,
						 I2C_FLAG_WRITE);
		if (res <= 0) {
			pr_err("i2c_master_send function err\n");
			goto EXIT_ERR;
		}
	}
	res = CM36558_setup_eint(client);
	if (res != 0) {
		pr_err("setup eint: %d\n", res);
		return res;
	}

	return CM36558_SUCCESS;

EXIT_ERR:
	pr_err("init dev: %d\n", res);
	return res;
}

static int als_open_report_data(int open)
{
	return 0;
}

static int als_enable_nodata(int en)
{
	int res = 0;

	pr_info("CM36558_obj als enable value = %d\n", en);

	mutex_lock(&CM36558_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &CM36558_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &CM36558_obj->enable);
	mutex_unlock(&CM36558_mutex);
	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return -1;
	}
	res = CM36558_enable_als(CM36558_obj->client, en);
	if (res) {
		pr_err("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;
}

static int als_set_delay(u64 ns)
{
	return 0;
}

static int als_batch(int flag, int64_t samplingPeriodNs,
		     int64_t maxBatchReportLatencyNs)
{
	return als_set_delay(samplingPeriodNs);
}

static int als_flush(void)
{
	return als_flush_report();
}

static int als_get_data(int *value, int *status)
{
	int err = 0;
	struct CM36558_priv *obj = NULL;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return -1;
	}
	obj = CM36558_obj;
	err = CM36558_read_als(obj->client, &obj->als);
	if (err) {
		err = -1;
	} else {
		*value = CM36558_get_als_value(obj, obj->als);
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return err;
}

static int ps_open_report_data(int open)
{
	return 0;
}

static int ps_enable_nodata(int en)
{
	int res = 0;

	pr_debug("CM36558_obj als enable value = %d\n", en);
	mutex_lock(&CM36558_mutex);
	if (en)
		set_bit(CMC_BIT_PS, &CM36558_obj->enable);
	else
		clear_bit(CMC_BIT_PS, &CM36558_obj->enable);
	mutex_unlock(&CM36558_mutex);
	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return -1;
	}
	res = CM36558_enable_ps(CM36558_obj->client, en);
	if (res) {
		pr_err("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;
}

static int ps_set_delay(u64 ns)
{
	return 0;
}
static int ps_batch(int flag, int64_t samplingPeriodNs,
		    int64_t maxBatchReportLatencyNs)
{
	return 0;
}

static int ps_flush(void)
{
	return ps_flush_report();
}

static int ps_get_data(int *value, int *status)
{
	int err = 0;

	if (!CM36558_obj) {
		pr_err("CM36558_obj is null!!\n");
		return -1;
	}
	err = CM36558_read_ps(CM36558_obj->client, &CM36558_obj->ps);
	if (err) {
		err = -1;
	} else {
		*value = CM36558_get_ps_value(CM36558_obj, CM36558_obj->ps);
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return 0;
}

static int cm36558_als_factory_enable_sensor(bool enable_disable,
					     int64_t sample_periods_ms)
{
	int err = 0;

	err = als_enable_nodata(enable_disable ? 1 : 0);
	if (err) {
		pr_err("%s:%s failed\n", __func__,
			   enable_disable ? "enable" : "disable");
		return -1;
	}
	err = als_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		pr_err("%s set_batch failed\n", __func__);
		return -1;
	}
	return 0;
}
static int cm36558_als_factory_get_data(int32_t *data)
{
	int status;

	return als_get_data(data, &status);
}
static int cm36558_als_factory_get_raw_data(int32_t *data)
{
	int err = 0;
	struct CM36558_priv *obj = CM36558_obj;

	if (!obj) {
		pr_err("obj is null!!\n");
		return -1;
	}

	err = CM36558_read_als(obj->client, &obj->als);
	if (err) {
		pr_err("%s failed\n", __func__);
		return -1;
	}
	*data = CM36558_obj->als;

	return 0;
}
static int cm36558_als_factory_enable_calibration(void)
{
	return 0;
}
static int cm36558_als_factory_clear_cali(void)
{
	return 0;
}
static int cm36558_als_factory_set_cali(int32_t offset)
{
	return 0;
}
static int cm36558_als_factory_get_cali(int32_t *offset)
{
	return 0;
}
static int cm36558_ps_factory_enable_sensor(bool enable_disable,
					    int64_t sample_periods_ms)
{
	int err = 0;

	err = ps_enable_nodata(enable_disable ? 1 : 0);
	if (err) {
		pr_err("%s:%s failed\n", __func__,
			   enable_disable ? "enable" : "disable");
		return -1;
	}
	err = ps_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		pr_err("%s set_batch failed\n", __func__);
		return -1;
	}
	return err;
}
static int cm36558_ps_factory_get_data(int32_t *data)
{
	int err = 0, status = 0;

	err = ps_get_data(data, &status);
	if (err < 0)
		return -1;
	return 0;
}
static int cm36558_ps_factory_get_raw_data(int32_t *data)
{
	int err = 0;
	struct CM36558_priv *obj = CM36558_obj;

	err = CM36558_read_ps(obj->client, &obj->ps);
	if (err) {
		pr_err("%s failed\n", __func__);
		return -1;
	}
	*data = CM36558_obj->ps;
	return 0;
}
static int cm36558_ps_factory_enable_calibration(void)
{
	return 0;
}
static int cm36558_ps_factory_clear_cali(void)
{
	struct CM36558_priv *obj = CM36558_obj;

	obj->ps_cali = 0;
	return 0;
}
static int cm36558_ps_factory_set_cali(int32_t offset)
{
	struct CM36558_priv *obj = CM36558_obj;

	obj->ps_cali = offset;
	return 0;
}
static int cm36558_ps_factory_get_cali(int32_t *offset)
{
	struct CM36558_priv *obj = CM36558_obj;

	*offset = obj->ps_cali;
	return 0;
}
static int cm36558_ps_factory_set_threshold(int32_t threshold[2])
{
	int err = 0;
	struct CM36558_priv *obj = CM36558_obj;

	pr_info("%s set threshold high: 0x%x, low: 0x%x\n", __func__,
		 threshold[0], threshold[1]);
	atomic_set(&obj->ps_thd_val_high, (threshold[0] + obj->ps_cali));
	atomic_set(&obj->ps_thd_val_low, (threshold[1] + obj->ps_cali));
	err = set_psensor_threshold(obj->client);

	if (err < 0) {
		pr_err("set_psensor_threshold fail\n");
		return -1;
	}
	return 0;
}
static int cm36558_ps_factory_get_threshold(int32_t threshold[2])
{
	struct CM36558_priv *obj = CM36558_obj;

	threshold[0] = atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
	threshold[1] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali;
	return 0;
}

static struct alsps_factory_fops cm36558_factory_fops = {
	.als_enable_sensor = cm36558_als_factory_enable_sensor,
	.als_get_data = cm36558_als_factory_get_data,
	.als_get_raw_data = cm36558_als_factory_get_raw_data,
	.als_enable_calibration = cm36558_als_factory_enable_calibration,
	.als_clear_cali = cm36558_als_factory_clear_cali,
	.als_set_cali = cm36558_als_factory_set_cali,
	.als_get_cali = cm36558_als_factory_get_cali,

	.ps_enable_sensor = cm36558_ps_factory_enable_sensor,
	.ps_get_data = cm36558_ps_factory_get_data,
	.ps_get_raw_data = cm36558_ps_factory_get_raw_data,
	.ps_enable_calibration = cm36558_ps_factory_enable_calibration,
	.ps_clear_cali = cm36558_ps_factory_clear_cali,
	.ps_set_cali = cm36558_ps_factory_set_cali,
	.ps_get_cali = cm36558_ps_factory_get_cali,
	.ps_set_threshold = cm36558_ps_factory_set_threshold,
	.ps_get_threshold = cm36558_ps_factory_get_threshold,
};

static struct alsps_factory_public cm36558_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &cm36558_factory_fops,
};

static int CM36558_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct CM36558_priv *obj = NULL;

	int err = 0;
	struct als_control_path als_ctl = {0};
	struct als_data_path als_data = {0};
	struct ps_control_path ps_ctl = {0};
	struct ps_data_path ps_data = {0};

	pr_debug("%s\n", __func__);
	/* get customization and power on */
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	err = get_alsps_dts_func(client->dev.of_node, &obj->hw);
	if (err < 0) {
		pr_err("get customization info from dts failed\n");
		goto exit_init_failed;
	}

	CM36558_obj = obj;

	INIT_WORK(&obj->eint_work, CM36558_eint_work);

	obj->client = client;
	i2c_set_clientdata(client, obj);

	atomic_set(&obj->als_debounce, 200);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 200);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->ps_cmd_val, 0xC1);
	atomic_set(&obj->ps_thd_val_high, obj->hw.ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low, obj->hw.ps_threshold_low);
	atomic_set(&obj->als_thd_val_high, obj->hw.als_threshold_high);
	atomic_set(&obj->als_thd_val_low, obj->hw.als_threshold_low);
	atomic_set(&obj->init_done, 0);
	obj->irq_node = client->dev.of_node;

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->ps_cali = 0;
	obj->als_level_num = ARRAY_SIZE(obj->hw.als_level);
	obj->als_value_num = ARRAY_SIZE(obj->hw.als_value);

	WARN_ON(sizeof(obj->als_level) != sizeof(obj->hw.als_level));
	memcpy(obj->als_level, obj->hw.als_level, sizeof(obj->als_level));
	WARN_ON(sizeof(obj->als_value) != sizeof(obj->hw.als_value));
	memcpy(obj->als_value, obj->hw.als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	clear_bit(CMC_BIT_ALS, &obj->enable);
	clear_bit(CMC_BIT_PS, &obj->enable);

	CM36558_i2c_client = client;
	err = CM36558_init_client(client);
	if (err)
		goto exit_init_failed;
	pr_debug("CM36558_init_client() OK!\n");
	/* err = misc_register(&CM36558_device); */
	err = alsps_factory_device_register(&cm36558_factory_device);
	if (err) {
		pr_err("CM36558_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	als_ctl.is_use_common_factory = false;
	ps_ctl.is_use_common_factory = false;
	pr_debug("CM36558_device misc_register OK!\n");

	err = CM36558_create_attr(
		&(CM36558_init_info.platform_diver_addr->driver));
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay = als_set_delay;
	als_ctl.batch = als_batch;
	als_ctl.flush = als_flush;
	als_ctl.is_report_input_direct = false;

	als_ctl.is_support_batch = false;

	err = als_register_control_path(&als_ctl);
	if (err) {
		pr_err("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if (err) {
		pr_err("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.batch = ps_batch;
	ps_ctl.flush = ps_flush;
	ps_ctl.is_report_input_direct = true;
	ps_ctl.is_support_batch = false;

	err = ps_register_control_path(&ps_ctl);
	if (err) {
		pr_err("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err) {
		pr_err("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	CM36558_init_flag = 0;
	pr_debug("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
exit_sensor_obj_attach_fail:
exit_misc_device_register_failed:
/* misc_deregister(&CM36558_device); */
exit_init_failed:
	kfree(obj);
exit:
	obj = NULL;
	CM36558_obj = NULL;
	CM36558_i2c_client = NULL;
	pr_err("%s: err = %d\n", __func__, err);
	CM36558_init_flag = -1;
	return err;
}

static int CM36558_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = CM36558_delete_attr(
		&(CM36558_init_info.platform_diver_addr->driver));
	if (err)
		pr_err("CM36558_delete_attr fail: %d\n", err);

	alsps_factory_device_deregister(&cm36558_factory_device);

	CM36558_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static int CM36558_i2c_detect(struct i2c_client *client,
			      struct i2c_board_info *info)
{
	strlcpy(info->type, CM36558_DEV_NAME, sizeof(info->type));
	return 0;
}

static int CM36558_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct CM36558_priv *obj = i2c_get_clientdata(client);
	int err = 0;

	pr_debug("%s\n", __func__);
	if (!obj) {
		pr_err("null pointer!!\n");
		return 0;
	}

	atomic_set(&obj->als_suspend, 1);
	err = CM36558_enable_als(obj->client, 0);
	if (err)
		pr_err("disable als fail: %d\n", err);
	return 0;
}

static int CM36558_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct CM36558_priv *obj = i2c_get_clientdata(client);
	int err = 0;

	pr_debug("%s\n", __func__);
	if (!obj) {
		pr_err("null pointer!!\n");
		return 0;
	}

	atomic_set(&obj->als_suspend, 0);
	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = CM36558_enable_als(obj->client, 1);
		if (err)
			pr_err("enable als fail: %d\n", err);
	}
	return 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int CM36558_remove(void)
{
	i2c_del_driver(&CM36558_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/

static int CM36558_local_init(void)
{
	if (i2c_add_driver(&CM36558_i2c_driver)) {
		pr_err("add driver error\n");
		return -1;
	}
	if (-1 == CM36558_init_flag)
		return -1;
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init CM36558_init(void)
{
	alsps_driver_add(&CM36558_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit CM36558_exit(void)
{
	pr_debug("%s\n", __func__);
}

/*----------------------------------------------------------------------------*/
module_init(CM36558_init);
module_exit(CM36558_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("yucong xiong");
MODULE_DESCRIPTION("CM36558 driver");
MODULE_LICENSE("GPL");

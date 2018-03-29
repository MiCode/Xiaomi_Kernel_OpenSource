/*
 * Author: yucong xiong <yucong.xion@mediatek.com>
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
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include "cm3232.h"
#include <linux/sched.h>
#include <alsps.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define CM3232_DEV_NAME     "cm3232"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               pr_info(APS_TAG"%s\n", __func__)
#define APS_ERR(fmt, args...)    pr_err(APS_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    pr_info(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    pr_debug(APS_TAG fmt, ##args)

#define I2C_FLAG_WRITE 0
#define I2C_FLAG_READ 1

#define GLASS_FACTOR 17 /*6% of glass redouce value*/

/******************************************************************************
 * extern functions
*******************************************************************************/
static int cm3232_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id);
static int cm3232_i2c_remove(struct i2c_client *client);
static int cm3232_i2c_detect(struct i2c_client *client,
			     struct i2c_board_info *info);
static int cm3232_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int cm3232_i2c_resume(struct i2c_client *client);

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id cm3232_i2c_id[] = {
	{CM3232_DEV_NAME, 0}, {}
};

/* Maintain alsps cust info here */
struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;
struct platform_device *alspsPltFmDev;

/* For alsp driver get cust info */
struct alsps_hw *get_cust_alsps(void)
{
	return &alsps_cust;
}

/*----------------------------------------------------------------------------*/
struct cm3232_priv {
	struct alsps_hw *hw;
	struct i2c_client *client;
	struct work_struct eint_work;

	/*misc */
	u16 als_modulus;
	atomic_t i2c_retry;
	atomic_t als_suspend;
	atomic_t als_debounce;	/*debounce time after enabling als */
	atomic_t als_deb_on;	/*indicates if the debounce is on */
#ifdef CONFIG_64BIT
	atomic64_t als_deb_end;	/*the jiffies representing the end of debounce */
#else
	atomic_t als_deb_end;	/*the jiffies representing the end of debounce */
#endif
	atomic_t ps_mask;	/*mask ps: always return far away */
	atomic_t ps_debounce;	/*debounce time after enabling ps */
	atomic_t ps_deb_on;	/*indicates if the debounce is on */
#ifdef CONFIG_64BIT
	atomic64_t ps_deb_end;	/*the jiffies representing the end of debounce */
#else
	atomic_t ps_deb_end;	/*the jiffies representing the end of debounce */
#endif
	atomic_t ps_suspend;
	atomic_t trace;
	atomic_t init_done;
	struct device_node *irq_node;
	int irq;

	/*data */
	u16 als;
	u16 ps;
	/* u8  _align; */
	u16 als_level_num;
	u16 als_value_num;
	u32 als_level[C_CUST_ALS_LEVEL - 1];
	u32 als_value[C_CUST_ALS_LEVEL];
	int ps_cali;
	int als_cali;

	atomic_t als_cmd_val;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_cmd_val;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_thd_val_high;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_thd_val_low;	/*the cmd value can't be read, stored in ram */
	atomic_t als_thd_val_high;	/*the cmd value can't be read, stored in ram */
	atomic_t als_thd_val_low;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_thd_val;
	ulong enable;		/*enable mask */
	ulong pending_intr;	/*pending interrupt */
};
/*----------------------------------------------------------------------------*/

#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,als_ps"},
	{},
};
#endif

static struct i2c_driver cm3232_i2c_driver = {
	.probe = cm3232_i2c_probe,
	.remove = cm3232_i2c_remove,
	.detect = cm3232_i2c_detect,
	.suspend = cm3232_i2c_suspend,
	.resume = cm3232_i2c_resume,
	.id_table = cm3232_i2c_id,
	.driver = {
		   .name = CM3232_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = alsps_of_match,
#endif
		   },
};

/*----------------------------------------------------------------------------*/
struct PS_CALI_DATA_STRUCT {
	int close;
	int far_away;
	int valid;
};

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct i2c_client *cm3232_i2c_client;
static struct cm3232_priv *cm3232_obj;

static int cm3232_local_init(void);
static int cm3232_remove(void);
static int cm3232_init_flag = -1;	/* 0<==>OK -1 <==> fail */
static struct alsps_init_info cm3232_init_info = {
	.name = "cm3232",
	.init = cm3232_local_init,
	.uninit = cm3232_remove,
};

/*----------------------------------------------------------------------------*/
static DEFINE_MUTEX(cm3232_mutex);
/*----------------------------------------------------------------------------*/
typedef enum {
	CMC_BIT_ALS = 1,
	CMC_BIT_PS = 2,
} CMC_BIT;
/*-----------------------------CMC for debugging-------------------------------*/
typedef enum {
	CMC_TRC_ALS_DATA = 0x0001,
	CMC_TRC_PS_DATA = 0x0002,
	CMC_TRC_EINT = 0x0004,
	CMC_TRC_IOCTL = 0x0008,
	CMC_TRC_I2C = 0x0010,
	CMC_TRC_CVT_ALS = 0x0020,
	CMC_TRC_CVT_PS = 0x0040,
	CMC_TRC_CVT_AAL = 0x0080,
	CMC_TRC_DEBUG = 0x8000,
} CMC_TRC;
/*-----------------------------------------------------------------------------*/

int CM3232_i2c_master_operate(struct i2c_client *client, char *buf, int count,
			      int i2c_flag)
{
	int res = 0;
	u8 beg;
	struct i2c_msg msgs[2];
	mutex_lock(&cm3232_mutex);
	switch (i2c_flag) {
	case I2C_FLAG_WRITE:
		res = i2c_master_send(client, buf, count);
		break;
	case I2C_FLAG_READ:
		beg = buf[0];
		msgs[0].addr = client->addr;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &beg;
		msgs[1].addr = client->addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = count;
		msgs[1].buf = buf;

		if (!client)
			return -EINVAL;
		res = i2c_transfer(client->adapter, msgs,
				   sizeof(msgs) / sizeof(msgs[0]));
		if (res < 0) {
			APS_LOG("i2c_transfer error: (%d %p %d) %d\n",
				beg, buf, count, res);
			res = -EIO;
		} else {
			res = 0;	/*no error */
		}
		break;
	default:
		APS_LOG
		    ("CM3232_i2c_master_operate i2c_flag command not support!\n");
		break;
	}

	if (res < 0)
		goto EXIT_ERR;

	mutex_unlock(&cm3232_mutex);
	return res;
EXIT_ERR:
	mutex_unlock(&cm3232_mutex);
	APS_ERR("CM3232_i2c_master_operate fail(%d)\n", res);
	return res;
}

/*----------------------------------------------------------------------------*/
static void cm3232_power(struct alsps_hw *hw, unsigned int on)
{
}

/********************************************************************/
int cm3232_enable_als(struct i2c_client *client, int enable)
{
	struct cm3232_priv *obj = i2c_get_clientdata(client);
	int res;
	u8 databuf[3];

	if (enable == 1) {
		APS_LOG("cm3232_enable_als enable_als\n");
		databuf[1] = 0x02; /*0b00000010, ALS_IT:100ms, HS = 1*/
		databuf[0] = CM3232_REG_ALS_CONF;

		res =
		    CM3232_i2c_master_operate(client, databuf, 0x2,
					      I2C_FLAG_WRITE);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}
		atomic_set(&obj->als_deb_on, 1);
#ifdef CONFIG_64BIT
		atomic64_set(&obj->als_deb_end,
			     jiffies +
			     atomic_read(&obj->als_debounce) / (1000 / HZ));
#else
		atomic_set(&obj->als_deb_end,
			   jiffies +
			   atomic_read(&obj->als_debounce) / (1000 / HZ));
#endif

	} else {
		databuf[1] = 0x41;
		databuf[0] = CM3232_REG_ALS_CONF;

		res =
		    CM3232_i2c_master_operate(client, databuf, 0x2,
					      I2C_FLAG_WRITE);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}
		atomic_set(&obj->als_deb_on, 0);
	}
	return 0;
ENABLE_ALS_EXIT_ERR:
	return res;
}

/********************************************************************/
long cm3232_read_als(struct i2c_client *client, u16 *data)
{
	long res;
	u8 databuf[2];
	struct cm3232_priv *obj = i2c_get_clientdata(client);

	databuf[0] = CM3232_REG_ALS_DATA;
	res = CM3232_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_READ);
	if (res < 0) {
		APS_ERR("i2c_master_send function err\n");
		goto READ_ALS_EXIT_ERR;
	}

	if (atomic_read(&obj->trace) & CMC_TRC_DEBUG)
		APS_LOG("CM3232_REG_ALS_DATA value: %d\n",
			((databuf[1] << 8) | databuf[0]));

	*data = ((databuf[1] << 8) | databuf[0]);

	return 0;
READ_ALS_EXIT_ERR:
	return res;
}

/********************************************************************/
static int cm3232_get_als_value(struct cm3232_priv *obj, u16 als)
{
	int value = 0;

	/*when IT = 100ms, HS =2, 0.032Lux/bit is used to calculate Lux*/
	if (atomic_read(&obj->trace) & CMC_TRC_DEBUG)
		APS_LOG("cm3232_get_als_value: obj->als_cali: %d, als: %d\n", obj->als_cali, als);
	value = GLASS_FACTOR * als * 32 * obj->als_cali/1000;
	return value;
}

/*-------------------------------attribute file for debugging----------------------------------*/

/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
static ssize_t cm3232_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "(%d %d %d %d %d)\n",
		       atomic_read(&cm3232_obj->i2c_retry),
		       atomic_read(&cm3232_obj->als_debounce),
		       atomic_read(&cm3232_obj->ps_mask),
		       atomic_read(&cm3232_obj->ps_thd_val),
		       atomic_read(&cm3232_obj->ps_debounce));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_store_config(struct device_driver *ddri, const char *buf,
				   size_t count)
{
	int retry, als_deb, ps_deb, mask, thres;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}

	if (5 ==
	    sscanf(buf, "%d %d %d %d %d", &retry, &als_deb, &mask, &thres,
		   &ps_deb)) {
		atomic_set(&cm3232_obj->i2c_retry, retry);
		atomic_set(&cm3232_obj->als_debounce, als_deb);
		atomic_set(&cm3232_obj->ps_mask, mask);
		atomic_set(&cm3232_obj->ps_thd_val, thres);
		atomic_set(&cm3232_obj->ps_debounce, ps_deb);
	} else {
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}

	res =
	    snprintf(buf, PAGE_SIZE, "0x%04X\n",
		     atomic_read(&cm3232_obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_store_trace(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	int trace;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&cm3232_obj->trace, trace);
	else
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_show_als(struct device_driver *ddri, char *buf)
{
	int res;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}
	res = cm3232_read_als(cm3232_obj->client, &cm3232_obj->als);
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", cm3232_obj->als);
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_show_reg(struct device_driver *ddri, char *buf)
{
	u8 _bIndex = 0;
	u8 databuf[2] = { 0 };
	ssize_t _tLength = 0;

	if (!cm3232_obj) {
		APS_ERR("cm3623_obj is null!!\n");
		return 0;
	}

	for (_bIndex = 0; _bIndex < 0x0D; _bIndex++) {
		databuf[0] = _bIndex;
		CM3232_i2c_master_operate(cm3232_obj->client, databuf, 0x2,
					  I2C_FLAG_READ);
		_tLength +=
		    snprintf((buf + _tLength), (PAGE_SIZE - _tLength),
			     "Reg[0x%02X]: 0x%02X\n", _bIndex, databuf[0]);
	}

	return _tLength;

}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_show_send(struct device_driver *ddri, char *buf)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_store_send(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	int addr, cmd;
	u8 dat;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	} else if (2 != sscanf(buf, "%x %x", &addr, &cmd)) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8) cmd;

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_show_recv(struct device_driver *ddri, char *buf)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_store_recv(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	int addr;
	int ret;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}
	ret = kstrtoint(buf, 16, &addr);
	if (ret < 0) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}

	if (cm3232_obj->hw) {
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "CUST: %d, (%d %d)\n",
			     cm3232_obj->hw->i2c_num, cm3232_obj->hw->power_id,
			     cm3232_obj->hw->power_vol);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}

	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "REGS: %02X %02X %02X %02lX %02lX\n",
		     atomic_read(&cm3232_obj->als_cmd_val),
		     atomic_read(&cm3232_obj->ps_cmd_val),
		     atomic_read(&cm3232_obj->ps_thd_val), cm3232_obj->enable,
		     cm3232_obj->pending_intr);

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "MISC: %d %d\n",
		     atomic_read(&cm3232_obj->als_suspend),
		     atomic_read(&cm3232_obj->ps_suspend));

	return len;
}

/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct cm3232_priv *obj, const char *buf,
			     size_t count, u32 data[], int len)
{
	int idx = 0;
	int ret;
	char *cur = (char *)buf, *end = (char *)(buf + count);

	while (idx < len) {
		while ((cur < end) && IS_SPACE(*cur))
			cur++;

		ret = kstrtoint(cur, 10, &data[idx]);
		if (ret < 0)
			break;

		idx++;
		while ((cur < end) && !IS_SPACE(*cur))
			cur++;
	}
	return idx;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < cm3232_obj->als_level_num; idx++)
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "%d ",
			     cm3232_obj->hw->als_level[idx]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_store_alslv(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def")) {
		memcpy(cm3232_obj->als_level, cm3232_obj->hw->als_level,
		       sizeof(cm3232_obj->als_level));
	} else if (cm3232_obj->als_level_num !=
		   read_int_from_buf(cm3232_obj, buf, count,
				     cm3232_obj->hw->als_level,
				     cm3232_obj->als_level_num)) {
		APS_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < cm3232_obj->als_value_num; idx++)
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "%d ",
			     cm3232_obj->hw->als_value[idx]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t cm3232_store_alsval(struct device_driver *ddri, const char *buf,
				   size_t count)
{
	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def")) {
		memcpy(cm3232_obj->als_value, cm3232_obj->hw->als_value,
		       sizeof(cm3232_obj->als_value));
	} else if (cm3232_obj->als_value_num !=
		   read_int_from_buf(cm3232_obj, buf, count,
				     cm3232_obj->hw->als_value,
				     cm3232_obj->als_value_num)) {
		APS_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}

/*---------------------------------------------------------------------------------------*/
static DRIVER_ATTR(als, S_IWUSR | S_IRUGO, cm3232_show_als, NULL);
static DRIVER_ATTR(config, S_IWUSR | S_IRUGO, cm3232_show_config,
		   cm3232_store_config);
static DRIVER_ATTR(alslv, S_IWUSR | S_IRUGO, cm3232_show_alslv,
		   cm3232_store_alslv);
static DRIVER_ATTR(alsval, S_IWUSR | S_IRUGO, cm3232_show_alsval,
		   cm3232_store_alsval);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, cm3232_show_trace,
		   cm3232_store_trace);
static DRIVER_ATTR(status, S_IWUSR | S_IRUGO, cm3232_show_status, NULL);
static DRIVER_ATTR(send, S_IWUSR | S_IRUGO, cm3232_show_send,
		   cm3232_store_send);
static DRIVER_ATTR(recv, S_IWUSR | S_IRUGO, cm3232_show_recv,
		   cm3232_store_recv);
static DRIVER_ATTR(reg, S_IWUSR | S_IRUGO, cm3232_show_reg, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *cm3232_attr_list[] = {
	&driver_attr_als,
	&driver_attr_trace,	/*trace log */
	&driver_attr_config,
	&driver_attr_alslv,
	&driver_attr_alsval,
	&driver_attr_status,
	&driver_attr_send,
	&driver_attr_recv,
	&driver_attr_reg,
};

/*----------------------------------------------------------------------------*/
static int cm3232_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(cm3232_attr_list) / sizeof(cm3232_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, cm3232_attr_list[idx]);
		if (err) {
			APS_ERR("driver_create_file (%s) = %d\n",
				cm3232_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int cm3232_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(cm3232_attr_list) / sizeof(cm3232_attr_list[0]));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, cm3232_attr_list[idx]);

	return err;
}

/*-------------------------------MISC device related------------------------------------------*/

/************************************************************/
static int cm3232_open(struct inode *inode, struct file *file)
{
	file->private_data = cm3232_i2c_client;

	if (!file->private_data) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/************************************************************/
static int cm3232_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/************************************************************/

static long cm3232_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct cm3232_priv *obj = i2c_get_clientdata(client);
	long err = 0;
	void __user *ptr = (void __user *)arg;
	int dat;
	uint32_t enable;
	int ps_cali, als_cali;
	int threshold[2];

	switch (cmd) {
	case ALSPS_SET_PS_MODE:
	case ALSPS_GET_PS_MODE:
	case ALSPS_GET_PS_DATA:
	case ALSPS_GET_PS_RAW_DATA:
		APS_LOG("ps not supported");
		goto err_out;

	case ALSPS_SET_ALS_MODE:

		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		if (enable) {
			err = cm3232_enable_als(obj->client, 1);
			if (err) {
				APS_ERR("enable als fail: %ld\n", err);
				goto err_out;
			}
			set_bit(CMC_BIT_ALS, &obj->enable);
		} else {
			err = cm3232_enable_als(obj->client, 0);
			if (err) {
				APS_ERR("disable als fail: %ld\n", err);
				goto err_out;
			}
			clear_bit(CMC_BIT_ALS, &obj->enable);
		}
		break;

	case ALSPS_GET_ALS_MODE:
		enable = test_bit(CMC_BIT_ALS, &obj->enable) ? (1) : (0);
		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_ALS_DATA:
		err = cm3232_read_als(obj->client, &obj->als);
		if (err)
			goto err_out;

		dat = cm3232_get_als_value(obj, obj->als);
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_ALS_RAW_DATA:
		err = cm3232_read_als(obj->client, &obj->als);
		if (err)
			goto err_out;

		dat = obj->als;
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

		/*----------------------------------for factory mode test---------------------------------------*/
	case ALSPS_GET_PS_TEST_RESULT:
		APS_LOG("ps not supported");
		goto err_out;

	case ALSPS_IOCTL_CLR_CALI:
		if (copy_from_user(&dat, ptr, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		if (dat == 0)
			obj->ps_cali = 0;

		break;

	case ALSPS_IOCTL_GET_CALI:
		ps_cali = obj->ps_cali;
		if (copy_to_user(ptr, &ps_cali, sizeof(ps_cali))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_IOCTL_SET_CALI:
		if (copy_from_user(&als_cali, ptr, sizeof(als_cali))) {
			err = -EFAULT;
			goto err_out;
		}
		obj->als_cali = als_cali;
		APS_LOG("%s sALSPS_IOCTL_SET_CALI: %d\n", __func__,
			als_cali);
		break;

	case ALSPS_SET_PS_THRESHOLD:
		if (copy_from_user(threshold, ptr, sizeof(threshold))) {
			err = -EFAULT;
			goto err_out;
		}
		APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n", __func__,
			threshold[0], threshold[1]);
		atomic_set(&obj->ps_thd_val_high,
			   (threshold[0] + obj->ps_cali));
		atomic_set(&obj->ps_thd_val_low, (threshold[1] + obj->ps_cali));	/* need to confirm */
		break;

	case ALSPS_GET_PS_THRESHOLD_HIGH:
		threshold[0] =
		    atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
		APS_ERR("%s get threshold high: 0x%x\n", __func__,
			threshold[0]);
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_PS_THRESHOLD_LOW:
		threshold[0] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali;
		APS_ERR("%s get threshold low: 0x%x\n", __func__, threshold[0]);
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
		/*------------------------------------------------------------------------------------------*/

	default:
		APS_ERR("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

err_out:
	return err;
}

#ifdef CONFIG_COMPAT
static long compat_cm3232_unlocked_ioctl(struct file *filp, unsigned int cmd,
					 unsigned long arg)
{
	APS_FUN();

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		APS_ERR
		    ("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_ALSPS_SET_PS_MODE:
	case COMPAT_ALSPS_GET_PS_MODE:
	case COMPAT_ALSPS_GET_PS_DATA:
	case COMPAT_ALSPS_GET_PS_RAW_DATA:
	case COMPAT_ALSPS_SET_ALS_MODE:
	case COMPAT_ALSPS_GET_ALS_MODE:
	case COMPAT_ALSPS_GET_ALS_DATA:
	case COMPAT_ALSPS_GET_ALS_RAW_DATA:
	case COMPAT_ALSPS_GET_PS_TEST_RESULT:
	case COMPAT_ALSPS_GET_ALS_TEST_RESULT:
	case COMPAT_ALSPS_GET_PS_THRESHOLD_HIGH:
	case COMPAT_ALSPS_GET_PS_THRESHOLD_LOW:
	case COMPAT_ALSPS_GET_ALS_THRESHOLD_HIGH:
	case COMPAT_ALSPS_GET_ALS_THRESHOLD_LOW:
	case COMPAT_ALSPS_IOCTL_CLR_CALI:
	case COMPAT_ALSPS_IOCTL_GET_CALI:
	case COMPAT_ALSPS_IOCTL_SET_CALI:
	case COMPAT_ALSPS_SET_PS_THRESHOLD:
	case COMPAT_ALSPS_SET_ALS_THRESHOLD:
	case COMPAT_AAL_SET_ALS_MODE:
	case COMPAT_AAL_GET_ALS_MODE:
	case COMPAT_AAL_GET_ALS_DATA:
		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)
						  compat_ptr(arg));
	default:
		APS_ERR("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
	}
}
#endif
/********************************************************************/
/*------------------------------misc device related operation functions------------------------------------*/
static const struct file_operations cm3232_fops = {
	.owner = THIS_MODULE,
	.open = cm3232_open,
	.release = cm3232_release,
	.unlocked_ioctl = cm3232_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_cm3232_unlocked_ioctl,
#endif
};

static struct miscdevice cm3232_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &cm3232_fops,
};

/*--------------------------------------------------------------------------------*/
static int cm3232_init_client(struct i2c_client *client)
{
	u8 databuf[2];
	int res = 0;

	APS_LOG("cm3232_init_client\n");
	databuf[0] = CM3232_REG_ALS_CONF;
	databuf[1] = 0x01;
	res = CM3232_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res < 0) {
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}

	databuf[0] = CM3232_REG_ALS_CONF;
	databuf[1] = 0x41;
	res = CM3232_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if (res < 0) {
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}

	return CM3232_SUCCESS;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;
}

/*--------------------------------------------------------------------------------*/

/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int als_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */

static int als_enable_nodata(int en)
{
	int res = 0;

	APS_LOG("cm3232_obj als enable value = %d\n", en);

	mutex_lock(&cm3232_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &cm3232_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &cm3232_obj->enable);
	mutex_unlock(&cm3232_mutex);

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return -EINVAL;
	}

	res = cm3232_enable_als(cm3232_obj->client, en);
	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -EINVAL;
	}

	return 0;
}

static int als_set_delay(u64 ns)
{
	return 0;
}

static int als_get_data(int *value, int *status)
{
	int err = 0;
	struct cm3232_priv *obj = NULL;

	if (!cm3232_obj) {
		APS_ERR("cm3232_obj is null!!\n");
		return -EINVAL;
	}
	obj = cm3232_obj;
	err = cm3232_read_als(obj->client, &obj->als);
	if (err) {
		err = -EINVAL;
	} else {
		*value = cm3232_get_als_value(obj, obj->als);
		if (*value < 0)
			err = -EINVAL;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return err;
}

/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int ps_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
static int ps_enable_nodata(int en)
{
	/* no suppport */
	return 0;

}

static int ps_set_delay(u64 ns)
{
	return 0;
}

static int ps_get_data(int *value, int *status)
{
	/* no suppport */
	return 0;
}

/*-----------------------------------i2c operations----------------------------------*/
static int cm3232_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct cm3232_priv *obj;

	int err = 0;
	struct als_control_path als_ctl = { 0 };
	struct als_data_path als_data = { 0 };
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };

	APS_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));
	cm3232_obj = obj;

	obj->hw = hw;
	obj->client = client;
	i2c_set_clientdata(client, obj);

	/*-----------------------------value need to be confirmed-----------------------------------------*/
	atomic_set(&obj->als_debounce, 200);
	atomic_set(&obj->als_deb_on, 0);
#ifdef CONFIG_64BIT
	atomic64_set(&obj->als_deb_end, 0);
#else
	atomic_set(&obj->als_deb_end, 0);
#endif
	atomic_set(&obj->ps_debounce, 200);
	atomic_set(&obj->ps_deb_on, 0);
#ifdef CONFIG_64BIT
	atomic64_set(&obj->ps_deb_end, 0);
#else
	atomic_set(&obj->ps_deb_end, 0);
#endif
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->ps_cmd_val, 0xC1);
	atomic_set(&obj->ps_thd_val_high, obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low, obj->hw->ps_threshold_low);
	atomic_set(&obj->als_thd_val_high, obj->hw->als_threshold_high);
	atomic_set(&obj->als_thd_val_low, obj->hw->als_threshold_low);
	atomic_set(&obj->init_done, 0);
	obj->irq_node =
	    of_find_compatible_node(NULL, NULL, "mediatek, ALS-eint");

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->ps_cali = 100;
	obj->als_cali = 100;
	obj->als_level_num =
	    sizeof(obj->hw->als_level) / sizeof(obj->hw->als_level[0]);
	obj->als_value_num =
	    sizeof(obj->hw->als_value) / sizeof(obj->hw->als_value[0]);
	/*-----------------------------value need to be confirmed-----------------------------------------*/

	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	clear_bit(CMC_BIT_ALS, &obj->enable);
	clear_bit(CMC_BIT_PS, &obj->enable);

	cm3232_i2c_client = client;

	err = cm3232_init_client(client);
	if (err)
		goto exit_init_failed;
	APS_LOG("cm3232_init_client() OK!\n");

	err = misc_register(&cm3232_device);
	if (err) {
		APS_ERR("cm3232_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	als_ctl.is_use_common_factory = false;
	ps_ctl.is_use_common_factory = false;
	APS_LOG("cm3232_device misc_register OK!\n");

	/*------------------------cm3232 attribute file for debug--------------------------------------*/
	err =
	    cm3232_create_attr(&(cm3232_init_info.platform_diver_addr->driver));
	if (err) {
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	/*------------------------cm3232 attribute file for debug--------------------------------------*/
	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay = als_set_delay;
	als_ctl.is_report_input_direct = false;
	als_ctl.is_support_batch = false;

	err = als_register_control_path(&als_ctl);
	if (err) {
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if (err) {
		APS_ERR("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.is_report_input_direct = false;
	ps_ctl.is_support_batch = false;
	ps_ctl.is_polling_mode = obj->hw->polling_mode_ps;

	err = ps_register_control_path(&ps_ctl);
	if (err) {
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err) {
		APS_ERR("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	err =
	    batch_register_support_info(ID_LIGHT, als_ctl.is_support_batch, 1,
					0);
	if (err)
		APS_ERR("register light batch support err = %d\n", err);

	err =
	    batch_register_support_info(ID_PROXIMITY, ps_ctl.is_support_batch,
					1, 0);
	if (err)
		APS_ERR("register proximity batch support err = %d\n", err);

	cm3232_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
exit_sensor_obj_attach_fail:
exit_misc_device_register_failed:
	misc_deregister(&cm3232_device);
exit_init_failed:
	kfree(obj);
exit:
	cm3232_i2c_client = NULL;
	APS_ERR("%s: err = %d\n", __func__, err);
	cm3232_init_flag = -1;
	return err;
}

static int cm3232_i2c_remove(struct i2c_client *client)
{
	int err;
	/*------------------------cm3232 attribute file for debug--------------------------------------*/
	err =
	    cm3232_delete_attr(&(cm3232_init_info.platform_diver_addr->driver));
	if (err)
		APS_ERR("cm3232_delete_attr fail: %d\n", err);
	/*----------------------------------------------------------------------------------------*/

	err = misc_deregister(&cm3232_device);
	if (err)
		APS_ERR("misc_deregister fail: %d\n", err);

	cm3232_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;

}

static int cm3232_i2c_detect(struct i2c_client *client,
			     struct i2c_board_info *info)
{
	strlcpy(info->type, CM3232_DEV_NAME, 6);
	return 0;

}

static int cm3232_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct cm3232_priv *obj = i2c_get_clientdata(client);
	int err;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return 0;
	}

	atomic_set(&obj->als_suspend, 1);
	err = cm3232_enable_als(obj->client, 0);
	if (err)
		APS_ERR("disable als fail: %d\n", err);
	return 0;
}

static int cm3232_i2c_resume(struct i2c_client *client)
{
	struct cm3232_priv *obj = i2c_get_clientdata(client);
	int err;
	struct hwm_sensor_data sensor_data;

	memset(&sensor_data, 0, sizeof(sensor_data));
	APS_FUN();
	if (!obj) {
		APS_ERR("null pointer!!\n");
		return 0;
	}

	atomic_set(&obj->als_suspend, 0);
	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = cm3232_enable_als(obj->client, 1);
		if (err)
			APS_ERR("enable als fail: %d\n", err);
	}
	return 0;
}

/*----------------------------------------------------------------------------*/
static int cm3232_remove(void)
{
	cm3232_power(hw, 0);

	i2c_del_driver(&cm3232_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/

static int cm3232_local_init(void)
{
	cm3232_power(hw, 1);
	if (i2c_add_driver(&cm3232_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -EINVAL;
	}
	if (-1 == cm3232_init_flag)
		return -EINVAL;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init cm3232_init(void)
{
	const char *name = "mediatek,cm3232";

	hw = get_alsps_dts_func(name, hw);
	if (!hw) {
		APS_ERR("get_alsps_dts_func fail\n");
		return 0;
	}
	alsps_driver_add(&cm3232_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit cm3232_exit(void)
{
	APS_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(cm3232_init);
module_exit(cm3232_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("andrew yang");
MODULE_DESCRIPTION("cm3232 driver");
MODULE_LICENSE("GPL");

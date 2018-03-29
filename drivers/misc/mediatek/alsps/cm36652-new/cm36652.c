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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>

#include "cust_alsps.h"
#include "cm36652.h"
#include "alsps.h"
#ifdef CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif


/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define CM36652_DEV_NAME	 "cm36652"
/*----------------------------------------------------------------------------*/
#define APS_TAG				  "[cm36652] "
#define APS_ERR(fmt, args...)	pr_err(APS_TAG fmt, ##args)
#define APS_LOG(fmt, args...)	pr_debug(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)	pr_debug(APS_TAG fmt, ##args)


#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1

/*----------------------------------------------------------------------------*/
static int cm36652_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int cm36652_i2c_remove(struct i2c_client *client);
static int cm36652_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int cm36652_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int cm36652_i2c_resume(struct i2c_client *client);

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id cm36652_i2c_id[] = {{CM36652_DEV_NAME, 0}, {} };
/* static struct i2c_board_info __initdata i2c_cm36652={ I2C_BOARD_INFO(CM36652_DEV_NAME, 0x60)}; */
static unsigned long long int_top_time;
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
struct cm36652_priv {
	struct alsps_hw  *hw;
	struct i2c_client *client;
	struct work_struct	eint_work;
#ifdef CUSTOM_KERNEL_SENSORHUB
	struct work_struct init_done_work;
#endif

	/*misc*/
	u16		als_modulus;
	atomic_t	i2c_retry;
	atomic_t	als_suspend;
	atomic_t	als_debounce;	/*debounce time after enabling als*/
	atomic_t	als_deb_on;	/*indicates if the debounce is on*/
	atomic_t	als_deb_end;	/*the jiffies representing the end of debounce*/
	atomic_t	ps_mask;		/*mask ps: always return far away*/
	atomic_t	ps_debounce;	/*debounce time after enabling ps*/
	atomic_t	ps_deb_on;		/*indicates if the debounce is on*/
	atomic_t	ps_deb_end;	/*the jiffies representing the end of debounce*/
	atomic_t	ps_suspend;
	atomic_t	trace;
	atomic_t  init_done;
	struct device_node *irq_node;
	int		irq;

	/*data*/
	u16			als;
	u8			ps;
	u8			_align;
	u16			als_level_num;
	u16			als_value_num;
	u32			als_level[C_CUST_ALS_LEVEL-1];
	u32			als_value[C_CUST_ALS_LEVEL];
	int			ps_cali;

	atomic_t	als_cmd_val;	/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_cmd_val;	/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val_high;	 /*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val_low;	/*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_high;	 /*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_low;	/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val;
	ulong		enable;		/*enable mask*/
	ulong		pending_intr;	/*pending interrupt*/

};
/*----------------------------------------------------------------------------*/

#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,alsps"},
	{},
};
#endif

static struct i2c_driver cm36652_i2c_driver = {
	.probe	  = cm36652_i2c_probe,
	.remove	 = cm36652_i2c_remove,
	.detect	 = cm36652_i2c_detect,
	.suspend	= cm36652_i2c_suspend,
	.resume	 = cm36652_i2c_resume,
	.id_table   = cm36652_i2c_id,
	.driver = {
		.name = CM36652_DEV_NAME,
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


static struct i2c_client *cm36652_i2c_client;
static struct cm36652_priv *cm36652_obj;
/* static struct PS_CALI_DATA_STRUCT ps_cali={0,0,0}; */
static int intr_flag = 1; /* hw default away after enable. */

static int cm36652_local_init(void);
static int cm36652_remove(void);
static int cm36652_init_flag =  -1;
static struct alsps_init_info cm36652_init_info = {
		.name = "cm36652",
		.init = cm36652_local_init,
		.uninit = cm36652_remove,

};

static DEFINE_MUTEX(cm36652_mutex);

enum {
	CMC_BIT_ALS	= 1,
	CMC_BIT_PS	   = 2,
} CMC_BIT;
/*-----------------------------CMC for debugging-------------------------------*/
enum {
	CMC_TRC_ALS_DATA = 0x0001,
	CMC_TRC_PS_DATA = 0x0002,
	CMC_TRC_EINT	= 0x0004,
	CMC_TRC_IOCTL   = 0x0008,
	CMC_TRC_I2C	 = 0x0010,
	CMC_TRC_CVT_ALS = 0x0020,
	CMC_TRC_CVT_PS  = 0x0040,
	CMC_TRC_CVT_AAL = 0x0080,
	CMC_TRC_DEBUG   = 0x8000,
} CMC_TRC;




int CM36652_i2c_master_operate(struct i2c_client *client, const char *buf, int count, int i2c_flag)
{
	int res = 0;

	mutex_lock(&cm36652_mutex);
	switch (i2c_flag) {
	case I2C_FLAG_WRITE:
	client->addr &= I2C_MASK_FLAG;
	res = i2c_master_send(client, buf, count);
	client->addr &= I2C_MASK_FLAG;
	break;

	case I2C_FLAG_READ:
	client->addr &= I2C_MASK_FLAG;
	client->addr |= I2C_WR_FLAG;
	client->addr |= I2C_RS_FLAG;
	res = i2c_master_send(client, buf, count);
	client->addr &= I2C_MASK_FLAG;
	break;
	default:
	APS_LOG("CM36652_i2c_master_operate i2c_flag command not support!\n");
	break;
	}
	if (res < 0)
		goto EXIT_ERR;
	mutex_unlock(&cm36652_mutex);
	return res;
EXIT_ERR:
	mutex_unlock(&cm36652_mutex);
	APS_ERR("CM36652_i2c_master_operate fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/
static void cm36652_power(struct alsps_hw *hw, unsigned int on)
{
}
/********************************************************************/
int cm36652_enable_ps(struct i2c_client *client, int enable)
{
	struct cm36652_priv *obj = i2c_get_clientdata(client);
	int res;
	u8 databuf[3];

	if (enable == 1) {
		APS_LOG("cm36652_enable_ps enable_ps\n");
		databuf[0] = CM36652_REG_PS_CONF1_2;
		res = CM36652_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_PS_EXIT_ERR;
		}
		/* APS_LOG("CM36652_REG_PS_CONF1_2 value value_low = %x, value_high = %x\n", databuf[0], databuf[1]); */
		databuf[2] = databuf[1];
		databuf[1] = databuf[0]&0xFE;
		databuf[0] = CM36652_REG_PS_CONF1_2;
		res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_PS_EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 1);
		atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
		intr_flag = 1; /* reset hw status to away after enable. */
	} else {
		APS_LOG("cm36652_enable_ps disable_ps\n");
		databuf[0] = CM36652_REG_PS_CONF1_2;
		res = CM36652_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_PS_EXIT_ERR;
		}
		/* APS_LOG("CM36652_REG_PS_CONF1_2 value value_low = %x, value_high = %x\n", databuf[0], databuf[1]); */

		databuf[2] = databuf[1];
		databuf[1] = databuf[0]|0x01;
		databuf[0] = CM36652_REG_PS_CONF1_2;

		res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_PS_EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 0);
	}

	return 0;
ENABLE_PS_EXIT_ERR:
	return res;
}
/********************************************************************/
int cm36652_enable_als(struct i2c_client *client, int enable)
{
	struct cm36652_priv *obj = i2c_get_clientdata(client);
	int res;
	u8 databuf[3];

	if (enable == 1) {
		/* APS_LOG("cm36652_enable_als enable_als\n"); */
		databuf[0] = CM36652_REG_CS_CONF;
		res = CM36652_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}
		/* APS_LOG("CM36652_REG_CS_CONF value value_low = %x, value_high = %x\n", databuf[0], databuf[1]); */

		databuf[2] = databuf[1];
		databuf[1] = databuf[0]&0xFE;
		databuf[0] = CM36652_REG_CS_CONF;
		res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}
		atomic_set(&obj->als_deb_on, 1);
		atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
	} else {
		/* APS_LOG("cm36652_enable_als disable_als\n"); */
		databuf[0] = CM36652_REG_CS_CONF;
		res = CM36652_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
		if (res < 0) {
			APS_ERR("i2c_master_send function err\n");
			goto ENABLE_ALS_EXIT_ERR;
		}
		/* APS_LOG("CM36652_REG_CS_CONF value value_low = %x, value_high = %x\n", databuf[0], databuf[1]); */

		databuf[2] = databuf[1];
		databuf[1] = databuf[0]|0x01;
		databuf[0] = CM36652_REG_CS_CONF;
		res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
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
long cm36652_read_ps(struct i2c_client *client, u8 *data)
{
	long res;
	u8 databuf[2];
	struct cm36652_priv *obj = i2c_get_clientdata(client);

	databuf[0] = CM36652_REG_PS_DATA;
	res = CM36652_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
	if (res < 0) {
		APS_ERR("i2c_master_send function err\n");
		goto READ_PS_EXIT_ERR;
	}
	if (atomic_read(&obj->trace) & CMC_TRC_DEBUG)
		APS_LOG("CM36652_REG_PS_DATA value value_low = %x, value_high = %x\n", databuf[0], databuf[1]);

	if (databuf[0] < obj->ps_cali)
		*data = 0;
	else
		*data = databuf[0] - obj->ps_cali;
	return 0;
READ_PS_EXIT_ERR:
	return res;
}
/********************************************************************/
long cm36652_read_als(struct i2c_client *client, u16 *data)
{
	long res;
	u8 databuf[2];
	struct cm36652_priv *obj = i2c_get_clientdata(client);

	databuf[0] = CM36652_REG_ALS_DATA;
	res = CM36652_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
	if (res < 0) {
		APS_ERR("i2c_master_send function err\n");
		goto READ_ALS_EXIT_ERR;
	}

	if (atomic_read(&obj->trace) & CMC_TRC_DEBUG)
		APS_LOG("CM36652_REG_ALS_DATA value: %d\n", ((databuf[1]<<8)|databuf[0]));

	*data = ((databuf[1]<<8)|databuf[0]);

	return 0;
READ_ALS_EXIT_ERR:
	return res;
}
/********************************************************************/
static int cm36652_get_ps_value(struct cm36652_priv *obj, u8 ps)
{
	int val, mask = atomic_read(&obj->ps_mask);
	int invalid = 0;

	val = intr_flag; /* value between high/low threshold should sync. with hw status. */
	if (ps > atomic_read(&obj->ps_thd_val_high))
		val = 0;  /*close*/
	else if (ps < atomic_read(&obj->ps_thd_val_low))
		val = 1;  /*far away*/

	if (atomic_read(&obj->ps_suspend))
		invalid = 1;
	else if (1 == atomic_read(&obj->ps_deb_on)) {
		unsigned long endt = atomic_read(&obj->ps_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->ps_deb_on, 0);

		if (1 == atomic_read(&obj->ps_deb_on))
			invalid = 1;
	}

	if (!invalid) {
		if (unlikely(atomic_read(&obj->trace) & CMC_TRC_CVT_PS)) {
			if (mask)
				APS_DBG("PS:  %05d => %05d [M]\n", ps, val);
			else
				APS_DBG("PS:  %05d => %05d\n", ps, val);
		}
		if ((0 == test_bit(CMC_BIT_PS, &obj->enable))) {
			APS_DBG("PS: not enable and do not report this value\n");
			return -1;
		} else
			return val;

	} else {
		if (unlikely(atomic_read(&obj->trace) & CMC_TRC_CVT_PS))
			APS_DBG("PS:  %05d => %05d (-1)\n", ps, val);
		return -1;
	}
}
/********************************************************************/
static int cm36652_get_als_value(struct cm36652_priv *obj, u16 als)
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

	if ((0 == obj->als_level_num) || (0 == obj->als_value_num)) {
		APS_ERR("invalid als_level_num = %d, als_value_num = %d\n", obj->als_level_num, obj->als_value_num);
		return -1;
	}

	if (1 == atomic_read(&obj->als_deb_on))	{
		unsigned long endt = atomic_read(&obj->als_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->als_deb_on, 0);

		if (1 == atomic_read(&obj->als_deb_on))
			invalid = 1;
	}

	for (idx = 0; idx < obj->als_level_num; idx++) {
		if (als < obj->hw->als_level[idx])
			break;
	}

	if (idx >= obj->als_level_num || idx >= obj->als_value_num) {
		if (idx < obj->als_value_num)
			value = obj->hw->als_value[idx-1];
		else
			value = obj->hw->als_value[obj->als_value_num-1];
	} else {
		level_high = obj->hw->als_level[idx];
		level_low = (idx > 0) ? obj->hw->als_level[idx-1] : 0;
		level_diff = level_high - level_low;
		value_high = obj->hw->als_value[idx];
		value_low = (idx > 0) ? obj->hw->als_value[idx-1] : 0;
		value_diff = value_high - value_low;

		if ((level_low >= level_high) || (value_low >= value_high))
			value = value_low;
		else
			value = (level_diff * value_low + (als - level_low) * value_diff
				+ ((level_diff + 1) >> 1)) / level_diff;
	}

	if (!invalid) {
		if (atomic_read(&obj->trace) & CMC_TRC_CVT_AAL)
			APS_DBG("ALS: %d [%d, %d] => %d [%d, %d]\n", als, level_low,
				level_high, value, value_low, value_high);

	} else {
		if (atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)
			APS_DBG("ALS: %05d => %05d (-1)\n", als, value);

		return -1;
	}
	return value;
}


/*-------------------------------attribute file for debugging----------------------------------*/

/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
static ssize_t cm36652_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "(%d %d %d %d %d)\n",
	atomic_read(&cm36652_obj->i2c_retry), atomic_read(&cm36652_obj->als_debounce),
	atomic_read(&cm36652_obj->ps_mask), atomic_read(&cm36652_obj->ps_thd_val),
		atomic_read(&cm36652_obj->ps_debounce));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_store_config(struct device_driver *ddri, const char *buf, size_t count)
{
	int retry, als_deb, ps_deb, mask, thres;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}

	if (5 == sscanf(buf, "%d %d %d %d %d", &retry, &als_deb, &mask, &thres, &ps_deb)) {
		atomic_set(&cm36652_obj->i2c_retry, retry);
		atomic_set(&cm36652_obj->als_debounce, als_deb);
		atomic_set(&cm36652_obj->ps_mask, mask);
		atomic_set(&cm36652_obj->ps_thd_val, thres);
		atomic_set(&cm36652_obj->ps_debounce, ps_deb);
	} else
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&cm36652_obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_store_trace(struct device_driver *ddri, const char *buf, size_t count)
{
	int trace;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&cm36652_obj->trace, trace);
	else
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_show_als(struct device_driver *ddri, char *buf)
{
	int res;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}
	res = cm36652_read_als(cm36652_obj->client, &cm36652_obj->als);
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", cm36652_obj->als);
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_show_ps(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!cm36652_obj) {
		APS_ERR("cm3623_obj is null!!\n");
		return 0;
	}

	res = cm36652_read_ps(cm36652_obj->client, &cm36652_obj->ps);
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", (int)res);
	else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", cm36652_obj->ps);
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_show_reg(struct device_driver *ddri, char *buf)
{
	u8  _bIndex = 0;
	u8 databuf[2] = {0};
	ssize_t  _tLength  = 0;
	int res = 0;

	if (!cm36652_obj) {
		APS_ERR("cm3623_obj is null!!\n");
		return 0;
	}

	for (_bIndex = 0; _bIndex < 0x0D; _bIndex++) {
		databuf[0] = _bIndex;
		res = CM36652_i2c_master_operate(cm36652_obj->client, databuf, 0x201, I2C_FLAG_READ);
		if (res < 0) {
			APS_ERR("i2c_master_send function err res = %d\n", res);
		}
		_tLength += snprintf((buf + _tLength), (PAGE_SIZE - _tLength),
			"Reg[0x%02X]: 0x%02X\n", _bIndex, databuf[0]);
	}

	return _tLength;

}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_show_send(struct device_driver *ddri, char *buf)
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_store_send(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr, cmd;
	u8 dat;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	} else if (2 != sscanf(buf, "%x %x", &addr, &cmd)) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8)cmd;
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_show_recv(struct device_driver *ddri, char *buf)
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_store_recv(struct device_driver *ddri, const char *buf, size_t count)
{
	int *addr = NULL;
	int ret;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}
	/*} else if (1 != sscanf(buf, "%x", &addr)) {*/
	ret = kstrtoint(buf, 16, addr);
	if (ret < 0) {
		APS_ERR("invalid format: '%s'\n", buf);
		return ret;
	}

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}

	if (cm36652_obj->hw) {
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d)\n",
		cm36652_obj->hw->i2c_num, cm36652_obj->hw->power_id, cm36652_obj->hw->power_vol);
	} else
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");

	len += snprintf(buf+len, PAGE_SIZE-len, "REGS: %02X %02X %02X %02lX %02lX\n",
	atomic_read(&cm36652_obj->als_cmd_val), atomic_read(&cm36652_obj->ps_cmd_val),
	atomic_read(&cm36652_obj->ps_thd_val), cm36652_obj->enable, cm36652_obj->pending_intr);
	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d %d\n",
		atomic_read(&cm36652_obj->als_suspend), atomic_read(&cm36652_obj->ps_suspend));

	return len;
}
/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct cm36652_priv *obj, const char *buf, size_t count, u32 data[], int len)
{
	int idx = 0;
	int ret;
	char *cur = (char *)buf, *end = (char *)(buf+count);

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
static ssize_t cm36652_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < cm36652_obj->als_level_num; idx++)
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", cm36652_obj->hw->als_level[idx]);

	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_store_alslv(struct device_driver *ddri, const char *buf, size_t count)
{
	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def"))
		memcpy(cm36652_obj->als_level, cm36652_obj->hw->als_level, sizeof(cm36652_obj->als_level));
	else if (cm36652_obj->als_level_num != read_int_from_buf(cm36652_obj, buf, count,
			cm36652_obj->hw->als_level, cm36652_obj->als_level_num))
		APS_ERR("invalid format: '%s'\n", buf);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;

	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < cm36652_obj->als_value_num; idx++)
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", cm36652_obj->hw->als_value[idx]);

	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36652_store_alsval(struct device_driver *ddri, const char *buf, size_t count)
{
	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def"))
		memcpy(cm36652_obj->als_value, cm36652_obj->hw->als_value, sizeof(cm36652_obj->als_value));
	else if (cm36652_obj->als_value_num != read_int_from_buf(cm36652_obj, buf, count,
			cm36652_obj->hw->als_value, cm36652_obj->als_value_num))
		APS_ERR("invalid format: '%s'\n", buf);

	return count;
}
/*---------------------------------------------------------------------------------------*/
static DRIVER_ATTR(als,	 S_IWUSR | S_IRUGO, cm36652_show_als, NULL);
static DRIVER_ATTR(ps,	  S_IWUSR | S_IRUGO, cm36652_show_ps, NULL);
static DRIVER_ATTR(config,  S_IWUSR | S_IRUGO, cm36652_show_config,	cm36652_store_config);
static DRIVER_ATTR(alslv,   S_IWUSR | S_IRUGO, cm36652_show_alslv, cm36652_store_alslv);
static DRIVER_ATTR(alsval,  S_IWUSR | S_IRUGO, cm36652_show_alsval, cm36652_store_alsval);
static DRIVER_ATTR(trace,   S_IWUSR | S_IRUGO, cm36652_show_trace,		cm36652_store_trace);
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, cm36652_show_status, NULL);
static DRIVER_ATTR(send,	S_IWUSR | S_IRUGO, cm36652_show_send, cm36652_store_send);
static DRIVER_ATTR(recv,	S_IWUSR | S_IRUGO, cm36652_show_recv, cm36652_store_recv);
static DRIVER_ATTR(reg,	 S_IWUSR | S_IRUGO, cm36652_show_reg, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *cm36652_attr_list[] = {
	&driver_attr_als,
	&driver_attr_ps,
	&driver_attr_trace,		/*trace log*/
	&driver_attr_config,
	&driver_attr_alslv,
	&driver_attr_alsval,
	&driver_attr_status,
	&driver_attr_send,
	&driver_attr_recv,
	&driver_attr_reg,
};

/*----------------------------------------------------------------------------*/
static int cm36652_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(cm36652_attr_list)/sizeof(cm36652_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)	{
		err = driver_create_file(driver, cm36652_attr_list[idx]);
		if (err) {
			APS_ERR("driver_create_file (%s) = %d\n", cm36652_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int cm36652_delete_attr(struct device_driver *driver)
{
	int idx , err = 0;
	int num = (int)(sizeof(cm36652_attr_list)/sizeof(cm36652_attr_list[0]));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, cm36652_attr_list[idx]);

	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------interrupt functions--------------------------------*/
#ifndef CUSTOM_KERNEL_SENSORHUB
static int cm36652_check_intr(struct i2c_client *client)
{
	int res;
	u8 databuf[2];

	databuf[0] = CM36652_REG_PS_DATA;
	res = CM36652_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
	if (res < 0) {
		APS_ERR("i2c_master_send function err res = %d\n", res);
		goto EXIT_ERR;
	}
	APS_LOG("CM36652_REG_PS_DATA value value_low = %x, value_reserve = %x\n", databuf[0], databuf[1]);

	databuf[0] = CM36652_REG_INT_FLAG;
	res = CM36652_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
	if (res < 0) {
		APS_ERR("i2c_master_send function err res = %d\n", res);
		goto EXIT_ERR;
	}
	APS_LOG("CM36652_REG_INT_FLAG value value_low = %x, value_high = %x\n", databuf[0], databuf[1]);

	if (databuf[1]&0x02)
		intr_flag = 0;/* for close */
	else if (databuf[1]&0x01)
		intr_flag = 1;/* for away */
	else {
		res = -1;
		APS_ERR("cm36652_check_intr fail databuf[1]&0x01: %d\n", res);
		goto EXIT_ERR;
	}

	return 0;
EXIT_ERR:
	APS_ERR("cm36652_check_intr dev: %d\n", res);
	return res;
}
#endif /* #ifndef CUSTOM_KERNEL_SENSORHUB */
/*----------------------------------------------------------------------------*/
static void cm36652_eint_work(struct work_struct *work)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
	int res = 0;

	res = ps_report_interrupt_data(intr_flag);
	if (res != 0)
		APS_ERR("cm36652_eint_work err: %d\n", res);
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	struct cm36652_priv *obj = (struct cm36652_priv *)container_of(work, struct cm36652_priv, eint_work);
	int res = 0;

	APS_LOG("cm36652 int top half time = %lld\n", int_top_time);

	res = cm36652_check_intr(obj->client);
	if (res != 0)
		goto EXIT_INTR_ERR;
	else {
		APS_LOG("cm36652 interrupt value = %d\n", intr_flag);
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
	APS_ERR("cm36652_eint_work err: %d\n", res);
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */
}
/*----------------------------------------------------------------------------*/
#ifdef CUSTOM_KERNEL_SENSORHUB
static void cm36652_init_done_work(struct work_struct *work)
{
	struct cm36652_priv *obj = cm36652_obj;
	struct CM36652_CUST_DATA *p_cust_data;
	SCP_SENSOR_HUB_DATA data;
	int max_cust_data_size_per_packet;
	int i;
	uint sizeOfCustData;
	uint len;
	char *p = (char *)obj->hw;

	APS_FUN();

	p_cust_data = (struct CM36652_CUST_DATA *)data.set_cust_req.custData;
	sizeOfCustData = sizeof(*(obj->hw));
	max_cust_data_size_per_packet = sizeof(data.set_cust_req.custData) - offsetof(struct CM36652_SET_CUST, data);

	for (i = 0; sizeOfCustData > 0; i++) {
		data.set_cust_req.sensorType = ID_PROXIMITY;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		p_cust_data->setCust.action = CM36652_CUST_ACTION_SET_CUST;
		p_cust_data->setCust.part = i;

		if (sizeOfCustData > max_cust_data_size_per_packet)
			len = max_cust_data_size_per_packet;
		else
			len = sizeOfCustData;

		memcpy(p_cust_data->setCust.data, p, len);
		sizeOfCustData -= len;
		p += len;

		len += offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + offsetof(struct CM36652_SET_CUST, data);
		SCP_sensorHub_req_send(&data, &len, 1);
	}

	data.set_cust_req.sensorType = ID_PROXIMITY;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	p_cust_data->setEintInfo.action = CM36652_CUST_ACTION_SET_EINT_INFO;
	p_cust_data->setEintInfo.gpio_mode = GPIO_ALS_EINT_PIN_M_EINT;
	p_cust_data->setEintInfo.gpio_pin = GPIO_ALS_EINT_PIN;
	p_cust_data->setEintInfo.eint_num = CUST_EINT_ALS_NUM;
	p_cust_data->setEintInfo.eint_is_deb_en = CUST_EINT_ALS_DEBOUNCE_EN;
	p_cust_data->setEintInfo.eint_type = CUST_EINT_ALS_TYPE;
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(p_cust_data->setEintInfo);
	SCP_sensorHub_req_send(&data, &len, 1);

	mutex_lock(&cm36652_mutex);
	data.activate_req.sensorType = ID_PROXIMITY;
	data.activate_req.action = SENSOR_HUB_ACTIVATE;
	if (test_bit(CMC_BIT_PS, &obj->enable))
		data.activate_req.enable = 1;
	else
		data.activate_req.enable = 0;

	len = sizeof(data.activate_req);
	SCP_sensorHub_req_send(&data, &len, 1);
	data.activate_req.sensorType = ID_LIGHT;
	if (test_bit(CMC_BIT_ALS, &obj->enable))
		data.activate_req.enable = 1;
	else
		data.activate_req.enable = 0;

	len = sizeof(data.activate_req);
	SCP_sensorHub_req_send(&data, &len, 1);
	mutex_unlock(&cm36652_mutex);

	atomic_set(&obj->init_done,  1);
}
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */
/*----------------------------------------------------------------------------*/
static void cm36652_eint_func(void)
{
	struct cm36652_priv *obj = cm36652_obj;

	if (!obj)
		return;

	int_top_time = sched_clock();
	schedule_work(&obj->eint_work);
}
#if defined(CONFIG_OF)
static irqreturn_t cm36652_eint_handler(int irq, void *desc)
{
	cm36652_eint_func();
	disable_irq_nosync(cm36652_obj->irq);

	return IRQ_HANDLED;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CUSTOM_KERNEL_SENSORHUB
static int cm36652_irq_handler(void *data, uint len)
{
	struct cm36652_priv *obj = cm36652_obj;
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;

	if (!obj)
		return -1;

	APS_ERR("len = %d, type = %d, action = %d, errCode = %d\n",
		len, rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode);

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_INIT_DONE:
			schedule_work(&obj->init_done_work);
			/* schedule_delayed_work(&obj->init_done_work, HZ); */
			break;
		case SCP_NOTIFY:
			if (CM36652_NOTIFY_PROXIMITY_CHANGE == rsp->notify_rsp.data[0]) {
				intr_flag = rsp->notify_rsp.data[1];
				cm36652_eint_func();
			} else
				APS_ERR("Unknown notify");
			break;
		default:
			APS_ERR("Error sensor hub notify");
			break;
		}
		break;
	default:
		APS_ERR("Error sensor hub action");
		break;
	}

	return 0;
}
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */
/*----------------------------------------------------------------------------*/
int cm36652_setup_eint(struct i2c_client *client)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
	int err = 0;

	err = SCP_sensorHub_rsp_registration(ID_PROXIMITY, cm36652_irq_handler);
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;
	u32 ints[2] = {0, 0};

	alspsPltFmDev = get_alsps_platformdev();
/* gpio setting */
	pinctrl = devm_pinctrl_get(&alspsPltFmDev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		APS_ERR("Cannot find alsps pinctrl!\n");
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		APS_ERR("Cannot find alsps pinctrl default!\n");

	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		APS_ERR("Cannot find alsps pinctrl pin_cfg!\n");

	}
/* eint request */
	if (cm36652_obj->irq_node) {
		ret = of_property_read_u32_array(cm36652_obj->irq_node, "debounce", ints, ARRAY_SIZE(ints));
		if (!ret)
			APS_LOG("of_property_read_u32_array fail!!\n");
		gpio_request(ints[0], "p-sensor");
		gpio_set_debounce(ints[0], ints[1]);
		pinctrl_select_state(pinctrl, pins_cfg);
		APS_LOG("ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);

		cm36652_obj->irq = irq_of_parse_and_map(cm36652_obj->irq_node, 0);
		APS_LOG("cm36652_obj->irq = %d\n", cm36652_obj->irq);
		if (!cm36652_obj->irq) {
			APS_ERR("irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}
		if (request_irq(cm36652_obj->irq, cm36652_eint_handler, IRQF_TRIGGER_NONE, "ALS-eint", NULL)) {
			APS_ERR("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}
		enable_irq(cm36652_obj->irq);
		enable_irq_wake(cm36652_obj->irq);
	} else {
		APS_ERR("null irq node!!\n");
		return -EINVAL;
	}

#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	return 0;
}
/*-------------------------------MISC device related------------------------------------------*/



/************************************************************/
static int cm36652_open(struct inode *inode, struct file *file)
{
	file->private_data = cm36652_i2c_client;

	if (!file->private_data) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/************************************************************/
static int cm36652_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/************************************************************/
static int set_psensor_threshold(struct i2c_client *client)
{
	struct cm36652_priv *obj = i2c_get_clientdata(client);
	int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	struct CM36652_CUST_DATA *pCustData;
	int len;
	int32_t ps_thd_val_low, ps_thd_val_high;

	ps_thd_val_low = atomic_read(&obj->ps_thd_val_low);
	ps_thd_val_high = atomic_read(&obj->ps_thd_val_high);

	/* ps_cali would be add back in SCP side. */
	ps_thd_val_low -= obj->ps_cali;
	ps_thd_val_high -= obj->ps_cali;

	data.set_cust_req.sensorType = ID_PROXIMITY;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData = (struct CM36652_CUST_DATA *)(&data.set_cust_req.custData);

	pCustData->setPSThreshold.action = CM36652_CUST_ACTION_SET_PS_THRESHODL;
	pCustData->setPSThreshold.threshold[0] = ps_thd_val_low;
	pCustData->setPSThreshold.threshold[1] = ps_thd_val_high;
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setPSThreshold);

	res = SCP_sensorHub_req_send(&data, &len, 1);
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	u8 databuf[3];

	APS_ERR("set_psensor_threshold function high: 0x%x, low:0x%x\n",
		atomic_read(&obj->ps_thd_val_high), atomic_read(&obj->ps_thd_val_low));
	databuf[0] = CM36652_REG_PS_THD;
	databuf[1] = atomic_read(&obj->ps_thd_val_low);
	databuf[2] = atomic_read(&obj->ps_thd_val_high);/* threshold value need to confirm */
	res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		APS_ERR("i2c_master_send function err\n");
		return -1;
	}
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	return 0;

}
static long cm36652_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct cm36652_priv *obj = i2c_get_clientdata(client);
	long err = 0;
	void __user *ptr = (void __user *) arg;
	int dat;
	uint32_t enable;
	int ps_result;
	int ps_cali;
	int threshold[2];
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	struct CM36652_CUST_DATA *pCustData;
	int len;

	data.set_cust_req.sensorType = ID_PROXIMITY;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData = (struct CM36652_CUST_DATA *)(&data.set_cust_req.custData);
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	switch (cmd) {
	case ALSPS_SET_PS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		if (enable) {
			err = cm36652_enable_ps(obj->client, 1);
			if (err) {
				APS_ERR("enable ps fail: %ld\n", err);
				goto err_out;
			}
			set_bit(CMC_BIT_PS, &obj->enable);
		} else {
			err = cm36652_enable_ps(obj->client, 0);
			if (err) {
				APS_ERR("disable ps fail: %ld\n", err);
				goto err_out;
			}
			clear_bit(CMC_BIT_PS, &obj->enable);
		}
		break;

	case ALSPS_GET_PS_MODE:
		enable = test_bit(CMC_BIT_PS, &obj->enable) ? (1) : (0);
		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_PS_DATA:
		err = cm36652_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;

		dat = cm36652_get_ps_value(obj, obj->ps);
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_PS_RAW_DATA:
		err = cm36652_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;

		dat = obj->ps;
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_SET_ALS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		if (enable) {
			err = cm36652_enable_als(obj->client, 1);
			if (err) {
				APS_ERR("enable als fail: %ld\n", err);
				goto err_out;
			}
			set_bit(CMC_BIT_ALS, &obj->enable);
		} else {
			err = cm36652_enable_als(obj->client, 0);
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
		err = cm36652_read_als(obj->client, &obj->als);
		if (err)
			goto err_out;

		dat = cm36652_get_als_value(obj, obj->als);
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_ALS_RAW_DATA:
		err = cm36652_read_als(obj->client, &obj->als);
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
		err = cm36652_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;

		if (obj->ps > atomic_read(&obj->ps_thd_val_low))
			ps_result = 0;
		else
			ps_result = 1;

		if (copy_to_user(ptr, &ps_result, sizeof(ps_result))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_IOCTL_CLR_CALI:
		if (copy_from_user(&dat, ptr, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		if (dat == 0)
			obj->ps_cali = 0;

#ifdef CUSTOM_KERNEL_SENSORHUB
		pCustData->clearCali.action = CM36652_CUST_ACTION_CLR_CALI;
		len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->clearCali);
		err = SCP_sensorHub_req_send(&data, &len, 1);
#endif
		break;

	case ALSPS_IOCTL_GET_CALI:
		ps_cali = obj->ps_cali;
		if (copy_to_user(ptr, &ps_cali, sizeof(ps_cali))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_IOCTL_SET_CALI:
		if (copy_from_user(&ps_cali, ptr, sizeof(ps_cali))) {
			err = -EFAULT;
			goto err_out;
		}
		obj->ps_cali = ps_cali;

#ifdef CUSTOM_KERNEL_SENSORHUB
		pCustData->setCali.action = CM36652_CUST_ACTION_SET_CALI;
		pCustData->setCali.cali = ps_cali;
		len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setCali);
		err = SCP_sensorHub_req_send(&data, &len, 1);
#endif
		break;

	case ALSPS_SET_PS_THRESHOLD:
		if (copy_from_user(threshold, ptr, sizeof(threshold))) {
			err = -EFAULT;
			goto err_out;
		}
		APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n", __func__, threshold[0], threshold[1]);
		atomic_set(&obj->ps_thd_val_high,  (threshold[0]+obj->ps_cali));
		atomic_set(&obj->ps_thd_val_low,  (threshold[1]+obj->ps_cali));/* need to confirm */
		set_psensor_threshold(obj->client);
		break;

	case ALSPS_GET_PS_THRESHOLD_HIGH:
		threshold[0] = atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
		APS_ERR("%s get threshold high: 0x%x\n", __func__, threshold[0]);
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

	default:
		APS_ERR("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

err_out:
	return err;
}
#ifdef CONFIG_COMPAT
static long cm36652_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_ALSPS_SET_PS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_PS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_RAW_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_RAW_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_SET_ALS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_ALS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_RAW_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_RAW_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_TEST_RESULT:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_TEST_RESULT, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_CLR_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_CLR_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_GET_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_GET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_SET_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_SET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_SET_PS_THRESHOLD:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_PS_THRESHOLD, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_THRESHOLD_HIGH:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_THRESHOLD_HIGH, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_THRESHOLD_LOW:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_THRESHOLD_LOW, (unsigned long)arg32);
		break;
	default:
		APS_ERR("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}
#endif
/*------------------------------misc device related operation functions------------------------------------*/
static const struct file_operations cm36652_fops = {
	.owner = THIS_MODULE,
	.open = cm36652_open,
	.release = cm36652_release,
	.unlocked_ioctl = cm36652_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cm36652_compat_ioctl,
#endif
};

static struct miscdevice cm36652_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &cm36652_fops,
};


/*--------------------------------------------------------------------------------*/
static int cm36652_init_client(struct i2c_client *client)
{
	struct cm36652_priv *obj = i2c_get_clientdata(client);
	u8 databuf[3];
	int res = 0;

	databuf[0] = CM36652_REG_CS_CONF;
	if (1 == obj->hw->polling_mode_als)
		databuf[1] = 0x11;
	else
		databuf[1] = 0x17;
	databuf[2] = 0x0;
	res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}
	/* APS_LOG("cm36652 ps CM36652_REG_CS_CONF command!\n"); */

	databuf[0] = CM36652_REG_PS_CONF1_2;
	databuf[1] = 0x19;
	if (1 == obj->hw->polling_mode_ps)
		databuf[2] = 0x60;
	else
		databuf[2] = 0x62;

	res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}
	/* APS_LOG("cm36652 ps CM36652_REG_PS_CONF1_2 command!\n"); */

	databuf[0] = CM36652_REG_PS_CANC;/* value need to confirm */
	databuf[1] = 0x00;
	databuf[2] = 0x00;
	res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if (res <= 0) {
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}

	/* APS_LOG("cm36652 ps CM36652_REG_PS_CANC command!\n"); */

	if (0 == obj->hw->polling_mode_als) {
			databuf[0] = CM36652_REG_ALS_THDH;
			databuf[1] = 0x00;
			databuf[2] = atomic_read(&obj->als_thd_val_high);
			res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
			if (res <= 0) {
				APS_ERR("i2c_master_send function err\n");
				goto EXIT_ERR;
			}
			databuf[0] = CM36652_REG_ALS_THDL;
			databuf[1] = 0x00;
			databuf[2] = atomic_read(&obj->als_thd_val_low);/* threshold value need to confirm */
			res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
			if (res <= 0) {
				APS_ERR("i2c_master_send function err\n");
				goto EXIT_ERR;
			}
		}
	if (0 == obj->hw->polling_mode_ps) {
			databuf[0] = CM36652_REG_PS_THD;
			databuf[1] = atomic_read(&obj->ps_thd_val_low);
			databuf[2] = atomic_read(&obj->ps_thd_val_high);/* threshold value need to confirm */
			res = CM36652_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
			if (res <= 0) {
				APS_ERR("i2c_master_send function err\n");
				goto EXIT_ERR;
			}
		}
	res = cm36652_setup_eint(client);
	if (res != 0) {
		APS_ERR("setup eint: %d\n", res);
		return res;
	}

	return CM36652_SUCCESS;

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
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	/* APS_LOG("cm36652_obj als enable value = %d\n", en); */

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (atomic_read(&cm36652_obj->init_done)) {
		req.activate_req.sensorType = ID_LIGHT;
		req.activate_req.action = SENSOR_HUB_ACTIVATE;
		req.activate_req.enable = en;
		len = sizeof(req.activate_req);
		res = SCP_sensorHub_req_send(&req, &len, 1);
	} else
		APS_ERR("sensor hub has not been ready!!\n");

	mutex_lock(&cm36652_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &cm36652_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &cm36652_obj->enable);
	mutex_unlock(&cm36652_mutex);
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	mutex_lock(&cm36652_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &cm36652_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &cm36652_obj->enable);
	mutex_unlock(&cm36652_mutex);
	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return -1;
	}
	res = cm36652_enable_als(cm36652_obj->client, en);
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
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
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#else
	struct cm36652_priv *obj = NULL;
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (atomic_read(&cm36652_obj->init_done)) {
		req.get_data_req.sensorType = ID_LIGHT;
		req.get_data_req.action = SENSOR_HUB_GET_DATA;
		len = sizeof(req.get_data_req);
		err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err)
		APS_ERR("SCP_sensorHub_req_send fail!\n");
	else {
		*value = req.get_data_rsp.int16_Data[0];
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&cm36652_obj->trace) & CMC_TRC_PS_DATA)
		APS_LOG("value = %d\n", *value);
	else {
		APS_ERR("sensor hub hat not been ready!!\n");
		err = -1;
	}
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return -1;
	}
	obj = cm36652_obj;
	err = cm36652_read_als(obj->client, &obj->als);
	if (err)
		err = -1;
	else {
		*value = cm36652_get_als_value(obj, obj->als);
		if (*value < 0)
			err = -1;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

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
	int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	/* APS_LOG("cm36652_obj als enable value = %d\n", en); */

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (atomic_read(&cm36652_obj->init_done)) {
		req.activate_req.sensorType = ID_PROXIMITY;
		req.activate_req.action = SENSOR_HUB_ACTIVATE;
		req.activate_req.enable = en;
		len = sizeof(req.activate_req);
		res = SCP_sensorHub_req_send(&req, &len, 1);
	} else
		APS_ERR("sensor hub has not been ready!!\n");

	mutex_lock(&cm36652_mutex);
	if (en)
		set_bit(CMC_BIT_PS, &cm36652_obj->enable);
	else
		clear_bit(CMC_BIT_PS, &cm36652_obj->enable);
	mutex_unlock(&cm36652_mutex);
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	mutex_lock(&cm36652_mutex);
	if (en)
		set_bit(CMC_BIT_PS, &cm36652_obj->enable);

	else
		clear_bit(CMC_BIT_PS, &cm36652_obj->enable);

	mutex_unlock(&cm36652_mutex);
	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return -1;
	}
	res = cm36652_enable_ps(cm36652_obj->client, en);
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;

}

static int ps_set_delay(u64 ns)
{
	return 0;
}

static int ps_get_data(int *value, int *status)
{
	int err = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (atomic_read(&cm36652_obj->init_done)) {
		req.get_data_req.sensorType = ID_PROXIMITY;
		req.get_data_req.action = SENSOR_HUB_GET_DATA;
		len = sizeof(req.get_data_req);
		err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		APS_ERR("SCP_sensorHub_req_send fail!\n");
		*value = -1;
		err = -1;
	} else {
		*value = req.get_data_rsp.int16_Data[0];
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&cm36652_obj->trace) & CMC_TRC_PS_DATA)
		APS_LOG("value = %d\n", *value)
	else {
		APS_ERR("sensor hub has not been ready!!\n");
		err = -1;
	}
#else /* #ifdef CUSTOM_KERNEL_SENSORHUB */
	if (!cm36652_obj) {
		APS_ERR("cm36652_obj is null!!\n");
		return -1;
	}

	err = cm36652_read_ps(cm36652_obj->client, &cm36652_obj->ps);
	if (err)
		err = -1;
	else {
		*value = cm36652_get_ps_value(cm36652_obj, cm36652_obj->ps);
		if (*value < 0)
			err = -1;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	return err;
}


/*-----------------------------------i2c operations----------------------------------*/
static int cm36652_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cm36652_priv *obj;
	struct als_control_path als_ctl = {0};
	struct als_data_path als_data = {0};
	struct ps_control_path ps_ctl = {0};
	struct ps_data_path ps_data = {0};
	int err = 0;

	APS_LOG("cm36652_i2c_probe\n");

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));
	cm36652_obj = obj;
	obj->hw = hw;
	INIT_WORK(&obj->eint_work, cm36652_eint_work);
#ifdef CUSTOM_KERNEL_SENSORHUB
	INIT_WORK(&obj->init_done_work, cm36652_init_done_work);
#endif /* #ifdef CUSTOM_KERNEL_SENSORHUB */

	obj->client = client;
	i2c_set_clientdata(client, obj);

	/*-----------------------------value need to be confirmed-----------------------------------------*/
	atomic_set(&obj->als_debounce, 200);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 200);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->ps_cmd_val,  0xC1);
	atomic_set(&obj->ps_thd_val_high,  obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low,  obj->hw->ps_threshold_low);
	atomic_set(&obj->als_thd_val_high,  obj->hw->als_threshold_high);
	atomic_set(&obj->als_thd_val_low,  obj->hw->als_threshold_low);
	atomic_set(&obj->init_done,  0);
	obj->irq_node = of_find_compatible_node(NULL, NULL, "mediatek, als-eint");

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->ps_cali = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);
	/*-----------------------------value need to be confirmed-----------------------------------------*/

	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	clear_bit(CMC_BIT_ALS, &obj->enable);
	clear_bit(CMC_BIT_PS, &obj->enable);

	cm36652_i2c_client = client;

	err = cm36652_init_client(client);
	if (err)
		goto exit_init_failed;
	APS_LOG("cm36652_init_client() OK!\n");

	err = misc_register(&cm36652_device);
	if (err) {
		APS_ERR("cm36652_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	als_ctl.is_use_common_factory = false;
	ps_ctl.is_use_common_factory = false;
	APS_LOG("cm36652_device misc_register OK!\n");

	/*------------------------cm36652 attribute file for debug--------------------------------------*/
	err = cm36652_create_attr(&(cm36652_init_info.platform_diver_addr->driver));
	if (err) {
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	/*------------------------cm36652 attribute file for debug--------------------------------------*/
	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay  = als_set_delay;
	als_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	als_ctl.is_support_batch = obj->hw->is_batch_supported_als;
#else
	als_ctl.is_support_batch = false;
#endif

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
	ps_ctl.set_delay  = ps_set_delay;
	ps_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	ps_ctl.is_support_batch = obj->hw->is_batch_supported_ps;
#else
	ps_ctl.is_support_batch = false;
#endif
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

	err = batch_register_support_info(ID_LIGHT, als_ctl.is_support_batch, 1, 0);
	if (err)
		APS_ERR("register light batch support err = %d\n", err);

	err = batch_register_support_info(ID_PROXIMITY, ps_ctl.is_support_batch, 1, 0);
	if (err)
		APS_ERR("register proximity batch support err = %d\n", err);

	cm36652_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
exit_sensor_obj_attach_fail:
exit_misc_device_register_failed:
		misc_deregister(&cm36652_device);
exit_init_failed:
		kfree(obj);
exit:
	cm36652_i2c_client = NULL;
	APS_ERR("%s: err = %d\n", __func__, err);
	cm36652_init_flag =  -1;
	return err;
}

static int cm36652_i2c_remove(struct i2c_client *client)
{
	int err;

	err = cm36652_delete_attr(&(cm36652_init_info.platform_diver_addr->driver));
	if (err)
		APS_ERR("cm36652_delete_attr fail: %d\n", err);

	err = misc_deregister(&cm36652_device);
	if (err)
		APS_ERR("misc_deregister fail: %d\n", err);

	cm36652_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;

}

static int cm36652_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strncpy(info->type, CM36652_DEV_NAME, strlen(CM36652_DEV_NAME));
	return 0;

}

static int cm36652_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct cm36652_priv *obj = i2c_get_clientdata(client);
	int err;

	/* APS_LOG("cm36652_i2c_suspend\n"); */

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return 0;
	}

	atomic_set(&obj->als_suspend, 1);
	err = cm36652_enable_als(obj->client, 0);
	if (err)
		APS_ERR("disable als fail: %d\n", err);

	return 0;
}

static int cm36652_i2c_resume(struct i2c_client *client)
{
	struct cm36652_priv *obj = i2c_get_clientdata(client);
	int err;
	struct hwm_sensor_data sensor_data;

	memset(&sensor_data, 0, sizeof(sensor_data));
	/* APS_LOG("cm36652_i2c_resume\n"); */
	if (!obj) {
		APS_ERR("null pointer!!\n");
		return 0;
	}

	atomic_set(&obj->als_suspend, 0);
	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = cm36652_enable_als(obj->client, 1);
		if (err)
			APS_ERR("enable als fail: %d\n", err);
	}
	return 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int cm36652_remove(void)
{
	cm36652_power(hw, 0);/* ***************** */

	i2c_del_driver(&cm36652_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/

static int  cm36652_local_init(void)
{
	/* printk("fwq loccal init+++\n"); */

	cm36652_power(hw, 1);
	if (i2c_add_driver(&cm36652_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -1;
	}
	if (-1 == cm36652_init_flag)
		return -1;

	return 0;
}


/*----------------------------------------------------------------------------*/
static int __init cm36652_init(void)
{
	const char *name = "mediatek,cm36652";

	hw =   get_alsps_dts_func(name, hw);
	if (!hw)
		APS_ERR("get dts info fail\n");
	alsps_driver_add(&cm36652_init_info);
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit cm36652_exit(void)
{
#ifdef CONFIG_CUSTOM_KERNEL_ALSPS_MODULE
	alsps_success_Flag = false;
#endif
}
/*----------------------------------------------------------------------------*/
module_init(cm36652_init);
module_exit(cm36652_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("yucong xiong");
MODULE_DESCRIPTION("cm36652 driver");
MODULE_LICENSE("GPL");


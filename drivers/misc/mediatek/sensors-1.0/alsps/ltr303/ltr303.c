/*
 * Author: tong jun <tongjun@szroco.com>
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
#include <linux/sched/clock.h>
#include "cust_alsps.h"
#include "ltr303.h"
#include "alsps.h"

#define LTR303_DEV_NAME			"ltr303"
#define APS_TAG					"[ALS] "
#define APS_FUN(f)              pr_info(APS_TAG"%s\n", __func__)
#define APS_ERR(fmt, args...)   pr_info(APS_TAG"%s %d : "\
		fmt, __func__, __LINE__, ##args)
#define APS_LOG(fmt, args...)   pr_info(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)   pr_info(APS_TAG fmt, ##args)

static const struct i2c_device_id ltr303_i2c_id[] = {
	{LTR303_DEV_NAME, 0},
	{},
};
static unsigned long long int_top_time;
struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;

static int ltr303_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id);
static int ltr303_i2c_remove(struct i2c_client *client);
static int ltr303_i2c_detect(struct i2c_client *client,
				struct i2c_board_info *info);
static int ltr303_i2c_suspend(struct device *dev);
static int ltr303_i2c_resume(struct device *dev);
static int als_gainrange;
static int final_lux_val;

enum CMC_BIT {
	CMC_BIT_ALS = 1,
};

struct ltr303_priv {
	struct alsps_hw  *hw;
	struct i2c_client *client;
	struct work_struct	eint_work;

	/*misc*/
	u16		als_modulus;
	atomic_t	i2c_retry;
	atomic_t	als_suspend;
	atomic_t	als_debounce;
	atomic_t	als_deb_on;
	atomic_t	als_deb_end;
	atomic_t	trace;

#ifdef CONFIG_OF
	struct device_node *irq_node;
	int		irq;
#endif

	/*data*/
	u16		als;
	u8		_align;
	u16		als_level_num;
	u16		als_value_num;
	u32		als_level[C_CUST_ALS_LEVEL-1];
	u32		als_value[C_CUST_ALS_LEVEL];
	atomic_t	als_cmd_val;
	atomic_t	als_thd_val_high;
	atomic_t	als_thd_val_low;
	ulong		enable;	/*enable mask*/
	ulong		pending_intr;	/*pending interrupt*/
};

static struct ltr303_priv *ltr303_obj;
static struct i2c_client *ltr303_i2c_client;
static DEFINE_MUTEX(ltr303_mutex);
static int ltr303_local_init(void);
static int ltr303_remove(void);
static int ltr303_init_flag =  -1;
static int irq_enabled;
static struct alsps_init_info ltr303_init_info = {
	.name = "ltr303",
	.init = ltr303_local_init,
	.uninit = ltr303_remove,
};

#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,alsps"},
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops ltr303_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ltr303_i2c_suspend, ltr303_i2c_resume)
};
#endif

static struct i2c_driver ltr303_i2c_driver = {
	.probe = ltr303_i2c_probe,
	.remove = ltr303_i2c_remove,
	.detect = ltr303_i2c_detect,
	.id_table = ltr303_i2c_id,
	.driver = {
		.name = LTR303_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = alsps_of_match,
#endif
#ifdef CONFIG_PM_SLEEP
		.pm = &ltr303_pm_ops,
#endif
	},
};

/* I2C Read */
static int ltr303_i2c_read_reg(u8 regnum)
{
	u8 buffer[1], reg_value[1];
	int res = 0;

	mutex_lock(&ltr303_mutex);
	buffer[0] = regnum;
	res = i2c_master_send(ltr303_obj->client, buffer, 0x1);
	if (res <= 0) {
		APS_ERR("read reg send res = %d\n", res);
		mutex_unlock(&ltr303_mutex);
		return res;
	}

	res = i2c_master_recv(ltr303_obj->client, reg_value, 0x1);
	if (res <= 0) {
		APS_ERR("read reg recv res = %d\n", res);
		mutex_unlock(&ltr303_mutex);
		return res;
	}

	mutex_unlock(&ltr303_mutex);
	return reg_value[0];
}

/* I2C Write */
static int ltr303_i2c_write_reg(u8 regnum, u8 value)
{
	u8 databuf[2];
	int res = 0;

	mutex_lock(&ltr303_mutex);
	databuf[0] = regnum;
	databuf[1] = value;
	res = i2c_master_send(ltr303_obj->client, databuf, 0x2);
	mutex_unlock(&ltr303_mutex);
	if (res < 0) {
		APS_ERR("write reg send res = %d\n", res);
		return res;
	} else
		return 0;
}

static void ltr303_power(struct alsps_hw *hw, unsigned int on)
{
#ifdef DEMO_BOARD
	static unsigned int power_on;

	if (hw->power_id != POWER_NONE_MACRO) {
		if (power_on == on) {
			APS_LOG("ignore power control: %d\n", on);
		} else if (on) {
			if (!hwPowerOn(hw->power_id, hw->power_vol, "ltr303"))
				APS_ERR("power on fails!!\n");
		} else {
			if (!hwPowerDown(hw->power_id, "ltr303"))
				APS_ERR("power off fail!!\n");
		}
	}

	power_on = on;
#endif
}

static int ltr303_als_enable(struct i2c_client *client, int enable)
{
	int err = 0;
	u8 regdata = 0;

	regdata = ltr303_i2c_read_reg(LTR303_ALS_CONTR);
	if (enable != 0) {
		APS_LOG("ALS(1): enable als only\n");
		regdata |= 0x01;
	} else {
		APS_LOG("ALS(1): disable als only\n");
		regdata &= 0xfe;
	}

	err = ltr303_i2c_write_reg(LTR303_ALS_CONTR, regdata);
	if (err < 0) {
		APS_ERR("ALS: enable als err: %d en: %d\n", err, enable);
		return err;
	}

	mdelay(WAKEUP_DELAY);
	return 0;
}

static int ltr303_als_read(struct i2c_client *client, u16 *data)
{
	int alsval_ch0_lo, alsval_ch0_hi, alsval_ch0;
	int alsval_ch1_lo, alsval_ch1_hi, alsval_ch1;
	int luxdata_int;
	int ratio;

	alsval_ch1_lo = ltr303_i2c_read_reg(LTR303_ALS_DATA_CH1_0);
	alsval_ch1_hi = ltr303_i2c_read_reg(LTR303_ALS_DATA_CH1_1);
	alsval_ch1 = (alsval_ch1_hi * 256) + alsval_ch1_lo;
	APS_DBG("alsval_ch1_lo = %d, alsval_ch1_hi=%d, alsval_ch1=%d\n",
		alsval_ch1_lo, alsval_ch1_hi, alsval_ch1);
	alsval_ch0_lo = ltr303_i2c_read_reg(LTR303_ALS_DATA_CH0_0);
	alsval_ch0_hi = ltr303_i2c_read_reg(LTR303_ALS_DATA_CH0_1);
	alsval_ch0 = (alsval_ch0_hi * 256) + alsval_ch0_lo;
	APS_DBG("alsval_ch0_lo = %d, alsval_ch0_hi=%d, alsval_ch0=%d\n",
		alsval_ch0_lo, alsval_ch0_hi, alsval_ch0);

	if ((alsval_ch1 == 0) || (alsval_ch0 == 0)) {
		luxdata_int = 0;
		goto out;
	}
	ratio = (alsval_ch1 * 100) / (alsval_ch0 + alsval_ch1);
	APS_DBG("ratio = %d  gainrange = %d\n", ratio, als_gainrange);
	if (ratio < 45)
		luxdata_int = (((17743 * alsval_ch0) + (11059 * alsval_ch1))
			/ als_gainrange) / 1000;
	else if ((ratio < 64) && (ratio >= 45))
		luxdata_int = (((42785 * alsval_ch0) - (19548 * alsval_ch1))
			/ als_gainrange) / 1000;
	else if ((ratio < 85) && (ratio >= 64))
		luxdata_int = (((5926 * alsval_ch0) + (1185 * alsval_ch1))
			/ als_gainrange) / 1000;
	else
		luxdata_int = 0;

	APS_DBG("%s: als_value_lux = %d\n", __func__, luxdata_int);
out:
	*data = luxdata_int;
	final_lux_val = luxdata_int;
	return luxdata_int;
}

static int ltr303_get_als_value(struct ltr303_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;

	APS_DBG("als  = %d\n", als);
	for (idx = 0; idx < obj->als_level_num; idx++) {
		if (als < obj->hw->als_level[idx])
			break;
	}
	if (idx >= obj->als_value_num) {
		APS_ERR("exceed range\n");
		idx = obj->als_value_num - 1;
	}

	if (atomic_read(&obj->als_deb_on) == 1) {
		unsigned long endt = atomic_read(&obj->als_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->als_deb_on, 0);

		if (atomic_read(&obj->als_deb_on) == 1)
			invalid = 1;
	}

	if (!invalid) {
		APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
		return obj->hw->als_value[idx];
	}

	APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);
	return -1;
}

/*---------------------attribute file for debugging------------------------*/
static ssize_t ltr303_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "(%d %d)\n",
		atomic_read(&ltr303_obj->i2c_retry),
		atomic_read(&ltr303_obj->als_debounce));
	return res;
}

static ssize_t ltr303_store_config(struct device_driver *ddri, const char *buf,
	size_t count)
{
	int retry, als_deb;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "%d %d", &retry, &als_deb) == 2) {
		atomic_set(&ltr303_obj->i2c_retry, retry);
		atomic_set(&ltr303_obj->als_debounce, als_deb);
	} else
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}

static ssize_t ltr303_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}
	res = snprintf(buf, PAGE_SIZE, "0x%04x\n",
				atomic_read(&ltr303_obj->trace));
	return res;
}

static ssize_t ltr303_store_trace(struct device_driver *ddri, const char *buf,
	size_t count)
{
	int trace;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&ltr303_obj->trace, trace);
	else
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}

static ssize_t ltr303_show_als(struct device_driver *ddri, char *buf)
{
	int res;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}

	res = ltr303_als_read(ltr303_obj->client, &ltr303_obj->als);
	return snprintf(buf, PAGE_SIZE, "0x%04X(%d)\n", res, res);
}

static ssize_t ltr303_show_reg(struct device_driver *ddri, char *buf)
{
	int i, len = 0;
	int reg[] = {0x80, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8f,
					0x97, 0x98, 0x99, 0x9a, 0x9e};

	for (i = 0; i < 15; i++)
		len += snprintf(buf+len, PAGE_SIZE-len,
			"reg:0x%04x value: 0x%04x\n",
			reg[i], ltr303_i2c_read_reg(reg[i]));

	return len;
}

static ssize_t ltr303_show_send(struct device_driver *ddri, char *buf)
{
	return 0;
}

static ssize_t ltr303_store_send(struct device_driver *ddri, const char *buf,
	size_t count)
{
	int addr, cmd;
	u8 dat;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	} else if (sscanf(buf, "%x %x", &addr, &cmd) != 2) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8)cmd;

	return count;
}

static ssize_t ltr303_show_recv(struct device_driver *ddri, char *buf)
{
	return 0;
}

static ssize_t ltr303_store_recv(struct device_driver *ddri, const char *buf,
	size_t count)
{
	int addr;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	} else if (kstrtouint(buf, 10, &addr) != 1) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	return count;
}

static ssize_t ltr303_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}

	if (ltr303_obj->hw) {
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d)\n",
			ltr303_obj->hw->i2c_num, ltr303_obj->hw->power_id,
			ltr303_obj->hw->power_vol);
	} else {
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d\n",
			atomic_read(&ltr303_obj->als_suspend));
	return len;
}

#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
static int read_int_from_buf(struct ltr303_priv *obj, const char *buf,
	size_t count, u32 data[], int len)
{
	int idx = 0;
	char *cur = (char *) buf, *end = (char *)(buf+count);

	while (idx < len) {
		while ((cur < end) && IS_SPACE(*cur))
			cur++;

		if (kstrtouint(cur, 10, &data[idx]) != 1)
			break;

		idx++;
		while ((cur < end) && !IS_SPACE(*cur))
			cur++;
	}

	return idx;
}

static ssize_t ltr303_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < ltr303_obj->als_level_num; idx++)
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ",
				ltr303_obj->hw->als_level[idx]);

	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}

static ssize_t ltr303_store_alslv(struct device_driver *ddri, const char *buf,
	size_t count)
{
	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def"))
		memcpy(ltr303_obj->als_level, ltr303_obj->hw->als_level,
				sizeof(ltr303_obj->als_level));
	else if (ltr303_obj->als_level_num != read_int_from_buf(ltr303_obj,
		buf, count, ltr303_obj->hw->als_level,
		ltr303_obj->als_level_num))
		APS_ERR("invalid format: '%s'\n", buf);

	return count;
}

static ssize_t ltr303_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < ltr303_obj->als_value_num; idx++)
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ",
			ltr303_obj->hw->als_value[idx]);

	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}

static ssize_t ltr303_store_alsval(struct device_driver *ddri,
	const char *buf, size_t count)
{
	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def"))
		memcpy(ltr303_obj->als_value, ltr303_obj->hw->als_value,
			sizeof(ltr303_obj->als_value));
	else if (ltr303_obj->als_value_num != read_int_from_buf(ltr303_obj,
		buf, count, ltr303_obj->hw->als_value,
		ltr303_obj->als_value_num))
		APS_ERR("invalid format: '%s'\n", buf);

	return count;
}

static DRIVER_ATTR(als,     0444, ltr303_show_als,		NULL);
static DRIVER_ATTR(config,  0644, ltr303_show_config,	ltr303_store_config);
static DRIVER_ATTR(alslv,   0644, ltr303_show_alslv,	ltr303_store_alslv);
static DRIVER_ATTR(alsval,  0644, ltr303_show_alsval,	ltr303_store_alsval);
static DRIVER_ATTR(trace,   0644, ltr303_show_trace,	ltr303_store_trace);
static DRIVER_ATTR(status,  0444, ltr303_show_status,	NULL);
static DRIVER_ATTR(send,    0644, ltr303_show_send,	ltr303_store_send);
static DRIVER_ATTR(recv,    0644, ltr303_show_recv,	ltr303_store_recv);
static DRIVER_ATTR(reg,     0444, ltr303_show_reg,		NULL);

static struct driver_attribute *ltr303_attr_list[] = {
	&driver_attr_als,
	&driver_attr_trace,        /*trace log*/
	&driver_attr_config,
	&driver_attr_alslv,
	&driver_attr_alsval,
	&driver_attr_status,
	&driver_attr_send,
	&driver_attr_recv,
	&driver_attr_reg,
};

static int ltr303_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = ARRAY_SIZE(ltr303_attr_list);

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, ltr303_attr_list[idx]);
		if (err) {
			APS_ERR("driver_create_file (%s) = %d\n",
					ltr303_attr_list[idx]->attr.name, err);
			break;
		}
	}

	return err;
}

static int ltr303_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = ARRAY_SIZE(ltr303_attr_list);

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, ltr303_attr_list[idx]);


	return err;
}

/*-------------------------------interrupt functions------------------------*/
#ifndef CUSTOM_KERNEL_SENSORHUB
static int ltr303_check_intr(struct i2c_client *client)
{
	int res, intl;
	u8 buffer[2];

	APS_FUN();
	buffer[0] = LTR303_ALS_STATUS;
	res = i2c_master_send(client, buffer, 0x1);
	if (res <= 0)
		goto EXIT_ERR;


	res = i2c_master_recv(client, buffer, 0x1);
	if (res <= 0)
		goto EXIT_ERR;


	res = 0;
	intl = 0;
	if ((buffer[0] & 0x08) != 0) {
		res = 2;
		intl = 1;
	}

	return res;

EXIT_ERR:
	APS_ERR("%s fail\n", __func__);
	return 0;
}
#endif

static void ltr303_eint_work(struct work_struct *work)
{
	struct ltr303_priv *obj = (struct ltr303_priv *)container_of(work,
		struct ltr303_priv, eint_work);
	int err;

	err = ltr303_check_intr(obj->client);
	if (err < 0)
		goto EXIT_INTR;

EXIT_INTR:
#ifdef CONFIG_OF
	enable_irq(obj->irq);
#endif
}

static void ltr303_eint_func(void)
{
	struct ltr303_priv *obj = ltr303_obj;

	if (!obj)
		return;

	int_top_time = sched_clock();
	schedule_work(&obj->eint_work);
}

#ifdef CONFIG_OF
static irqreturn_t ltr303_eint_handler(int irq, void *desc)
{
	if (irq_enabled == 2) {
		disable_irq_nosync(ltr303_obj->irq);
		ltr303_eint_func();
	}

	return IRQ_HANDLED;
}
#endif

int ltr303_setup_eint(struct i2c_client *client)
{
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_cfg;
	u32 ints[2] = { 0, 0};

	APS_FUN();
	/* gpio setting */
	pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		APS_ERR("Cannot find alsps pinctrl!\n");
		return ret;
	}
	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		APS_ERR("Cannot find alsps pinctrl pin_cfg!\n");
	}
	/* eint request */
	if (ltr303_obj->irq_node) {
		of_property_read_u32_array(ltr303_obj->irq_node, "debounce",
			ints, ARRAY_SIZE(ints));
		gpio_request(ints[0], "p-sensor");
		gpio_set_debounce(ints[0], ints[1]);
		pinctrl_select_state(pinctrl, pins_cfg);
		APS_LOG("ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);
		ltr303_obj->irq = irq_of_parse_and_map(ltr303_obj->irq_node, 0);
		APS_LOG("ltr303_obj->irq = %d\n", ltr303_obj->irq);
		if (!ltr303_obj->irq) {
			APS_ERR("irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}
		APS_ERR("irq to gpio = %d\n", irq_to_gpio(ltr303_obj->irq));
		if (request_irq(ltr303_obj->irq, ltr303_eint_handler,
				IRQF_TRIGGER_NONE, "ALS-eint", NULL)) {
			APS_ERR("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}
		enable_irq_wake(ltr303_obj->irq);
		irq_enabled = 1;
	} else {
		APS_ERR("null irq node!!\n");
		return -EINVAL;
	}

	return 0;
}

/*-------------------------MISC device related------------------------------*/
static int ltr303_open(struct inode *inode, struct file *file)
{
	file->private_data = ltr303_i2c_client;
	if (!file->private_data) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}
static int ltr303_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long ltr303_unlocked_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct ltr303_priv *obj = i2c_get_clientdata(client);
	int err = 0;
	void __user *ptr = (void __user *) arg;
	int dat;
	uint32_t enable;
	int ps_cali;
	int threshold[2];

	APS_DBG("cmd= %d\n", cmd);
	switch (cmd) {
	case ALSPS_SET_PS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
	case ALSPS_GET_PS_RAW_DATA:
		dat = 0;
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
		err = ltr303_als_enable(obj->client, enable);
		if (err < 0) {
			APS_ERR("enable als fail: %d en: %d\n", err, enable);
			goto err_out;
		}
		if (enable)
			set_bit(CMC_BIT_ALS, &obj->enable);
		else
			clear_bit(CMC_BIT_ALS, &obj->enable);
		break;
	case ALSPS_GET_ALS_RAW_DATA:
		obj->als = ltr303_als_read(obj->client, &obj->als);
		if (obj->als < 0)
			goto err_out;

		dat = obj->als;
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
/*--------------------------for factory mode test---------------------------*/
	case ALSPS_GET_PS_TEST_RESULT:
		dat = 0;
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
	case ALSPS_IOCTL_CLR_CALI:
		if (copy_from_user(&dat, ptr, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
	case ALSPS_IOCTL_GET_CALI:
		ps_cali = 0;
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
		break;
	case ALSPS_SET_PS_THRESHOLD:
		if (copy_from_user(threshold, ptr, sizeof(threshold))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
	case ALSPS_GET_PS_THRESHOLD_HIGH:
		threshold[0] = 300;
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
		break;
	case ALSPS_GET_PS_THRESHOLD_LOW:
		threshold[0] = 100;
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	default:
		err = -ENOIOCTLCMD;
		break;
	}
err_out:
	return err;
}

/*------------------misc device related operation functions-----------------*/
static const struct file_operations ltr303_fops = {
	.owner = THIS_MODULE,
	.open = ltr303_open,
	.release = ltr303_release,
	.unlocked_ioctl = ltr303_unlocked_ioctl,
};

static struct miscdevice ltr303_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &ltr303_fops,
};

static int ltr303_init_client(void)
{
	int res;
	int init_als_gain;
	struct i2c_client *client = ltr303_obj->client;

	mdelay(PON_DELAY);
	als_gainrange = ALS_RANGE_64K;
	init_als_gain = als_gainrange;
	APS_ERR("ALS sensor gainrange %d!\n", init_als_gain);
	switch (init_als_gain) {
	case ALS_RANGE_64K:
		res = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_Range1);
		break;
	case ALS_RANGE_32K:
		res = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_Range2);
		break;
	case ALS_RANGE_16K:
		res = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_Range3);
		break;
	case ALS_RANGE_8K:
		res = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_Range4);
		break;
	case ALS_RANGE_1300:
		res = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_Range5);
		break;
	case ALS_RANGE_600:
		res = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_Range6);
		break;
	default:
		res = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_Range1);
		break;
	}
	res = ltr303_als_enable(client, 1);
	if (res < 0) {
		APS_ERR("enable als fail: %d\n", res);
		goto EXIT_ERR;
	}
	res = ltr303_setup_eint(client);
	if (res != 0) {
		APS_ERR("setup eint: %d\n", res);
		goto EXIT_ERR;
	}
	return 0;
EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return 1;
}

static int als_open_report_data(int open)
{
	/*should queuq work to report event if  is_report_input_direct=true*/
	return 0;
}

/*************************************************************
 * if use  this typ of enable ,
 * Sensor only enabled but not report inputEvent to HAL
 *************************************************************/
static int als_enable_nodata(int en)
{
	int res = 0;

	APS_LOG("ltr303_obj als enable value = %d\n", en);
	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return -1;
	}
	mutex_lock(&ltr303_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &ltr303_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &ltr303_obj->enable);
	mutex_unlock(&ltr303_mutex);
	res = ltr303_als_enable(ltr303_obj->client, en);
	if (res) {
		APS_ERR("%s is failed!!\n", __func__);
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

	if (!ltr303_obj) {
		APS_ERR("ltr303_obj is null!!\n");
		return -1;
	}
	ltr303_obj->als = ltr303_als_read(ltr303_obj->client, &ltr303_obj->als);
	if (ltr303_obj->als < 0)
		err = -1;
	else {
		*value = ltr303_get_als_value(ltr303_obj, ltr303_obj->als);
		if (*value < 0)
			err = -1;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return err;
}

static int ps_open_report_data(int open)
{
	/*should queuq work to report event if  is_report_input_direct=true*/
	return 0;
}

/* if  this typ of enable , sensor only enable but not report Event to HAL*/
static int ps_enable_nodata(int en)
{
	APS_LOG("ltr559_obj ps enable value = %d\n", en);
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

	*value = 0;
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return err;
}

/*--------------------------------i2c operations--------------------------*/
static int ltr303_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ltr303_priv *obj = NULL;
	struct als_control_path als_ctl = {0};
	struct als_data_path als_data = {0};
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };
	int err = 0;

	APS_FUN();
	err = get_alsps_dts_func(client->dev.of_node, hw);
	if (err < 0) {
		APS_ERR("get customization info from dts failed\n");
		return -EFAULT;
	}
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	ltr303_obj = obj;
	hw->i2c_addr[0] = 0x52;
	client->addr = (0x52 >> 1);
	obj->hw = hw;
	INIT_WORK(&obj->eint_work, ltr303_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);
	/*---------------value need to be confirmed--------------------*/
	atomic_set(&obj->als_debounce, 300);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_thd_val_high,  obj->hw->als_threshold_high);
	atomic_set(&obj->als_thd_val_low,  obj->hw->als_threshold_low);
	obj->irq_node = client->dev.of_node;
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = ARRAY_SIZE(obj->hw->als_level);
	obj->als_value_num = ARRAY_SIZE(obj->hw->als_value);
	obj->als_modulus = (400*100)/(16*150);
	/*----------------value need to be confirmed-------------------*/
	WARN_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	WARN_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	APS_LOG("ltr303_init_client() start...!\n");
	ltr303_i2c_client = client;
	err = ltr303_init_client();
	if (err)
		goto exit_init_failed;

	APS_LOG("ltr303_init_client() OK!\n");
	err = misc_register(&ltr303_device);
	if (err) {
		APS_ERR("ltr303_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	als_ctl.is_use_common_factory = false;
	ps_ctl.is_use_common_factory = false;
	err = ltr303_create_attr(&(ltr303_i2c_driver.driver));
	if (err) {
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay  = als_set_delay;
	als_ctl.batch = als_batch;
	als_ctl.flush = als_flush;
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
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}
	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.batch = ps_batch;
	ps_ctl.flush = ps_flush;
	ps_ctl.is_report_input_direct = false;
	ps_ctl.is_support_batch = false;
	ps_ctl.is_polling_mode = hw->polling_mode_ps;
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

	ltr303_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
exit_sensor_obj_attach_fail:
exit_misc_device_register_failed:
	misc_deregister(&ltr303_device);
exit_init_failed:
	kfree(obj);
exit:
	ltr303_i2c_client = NULL;
	APS_ERR("%s: err = %d\n", __func__, err);
	ltr303_init_flag = -1;
	return err;
}

static int ltr303_i2c_remove(struct i2c_client *client)
{
	int err;

	err = ltr303_delete_attr(&(ltr303_i2c_driver.driver));
	if (err)
		APS_ERR("ltr303_delete_attr fail: %d\n", err);

	misc_deregister(&ltr303_device);
	ltr303_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static int ltr303_i2c_detect(struct i2c_client *client,
				struct i2c_board_info *info)
{
	strcpy(info->type, LTR303_DEV_NAME);
	return 0;
}

static int ltr303_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltr303_priv *obj = i2c_get_clientdata(client);
	int err;

	APS_FUN();
	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	atomic_set(&obj->als_suspend, 1);
	err = ltr303_als_enable(obj->client, 0);
	if (err < 0) {
		APS_ERR("disable als: %d\n", err);
		return err;
	}
	ltr303_power(obj->hw, 0);
	return 0;
}

static int ltr303_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltr303_priv *obj = i2c_get_clientdata(client);
	int err;

	APS_FUN();
	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	ltr303_power(obj->hw, 1);
	atomic_set(&obj->als_suspend, 0);

	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = ltr303_als_enable(obj->client, 1);
		if (err < 0)
			APS_ERR("enable als fail: %d\n", err);
	}
	return 0;
}

static int ltr303_remove(void)
{
	APS_FUN();
	ltr303_power(hw, 0);
	i2c_del_driver(&ltr303_i2c_driver);
	return 0;
}

static int  ltr303_local_init(void)
{
	APS_FUN();
	ltr303_power(hw, 1);
	if (i2c_add_driver(&ltr303_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -1;
	}

	if (ltr303_init_flag == -1)
		return -1;

	return 0;
}

static int __init ltr303_init(void)
{
	alsps_driver_add(&ltr303_init_info);
	return 0;
}

static void __exit ltr303_exit(void)
{
	APS_FUN();
}

module_init(ltr303_init);
module_exit(ltr303_exit);

MODULE_AUTHOR("Liteon");
MODULE_DESCRIPTION("LTR-303ALS Driver");
MODULE_LICENSE("GPL");

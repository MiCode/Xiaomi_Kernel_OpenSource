/*
 *
 * Author: MingHsien Hsieh <minghsien.hsieh@mediatek.com>
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
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>

#include "cust_alsps.h"
#include "ltr559.h"
#include "alsps.h"

/******************************************************************************
 * configuration
 ******************************************************************************/
/*----------------------------------------------------------------------------*/

#define LTR559_DEV_NAME   "ltr559"

#define SUPPORT_PSENSOR

#ifdef SUPPORT_PSENSOR
#define GN_MTK_BSP_PS_DYNAMIC_CALI
#endif
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ltr559] "
#define APS_FUN(f)               pr_debug(APS_TAG "%s\n", __func__)

#define APS_ERR(fmt, args...)	pr_info(APS_TAG fmt, ##args)
#if 1
#define APS_LOG(fmt, args...)	pr_debug(APS_TAG fmt, ##args)

#define APS_DBG(fmt, args...)	pr_debug(APS_TAG fmt, ##args)
#else
#define APS_DBG(fmt, args...)
#define APS_LOG(fmt, args...)
#endif

/*----------------------------------------------------------------------------*/

static struct i2c_client *ltr559_i2c_client;

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id ltr559_i2c_id[] = {
	{LTR559_DEV_NAME, 0},
	{}
};
static unsigned long long int_top_time;
struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;
struct platform_device *alspsPltFmDev;

/*----------------------------------------------------------------------------*/
static int ltr559_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id);
static int ltr559_i2c_remove(struct i2c_client *client);
static int ltr559_i2c_detect(struct i2c_client *client,
			     struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_PM_SLEEP
static int ltr559_suspend(struct device *dev);
static int ltr559_resume(struct device *dev);
#endif

//static int ps_gainrange;
static int als_gainrange;

#ifdef SUPPORT_PSENSOR
static int final_prox_val;
#endif
static int final_lux_val;

/*----------------------------------------------------------------------------*/
static int ltr559_als_read(struct i2c_client *client, u16 *data);

/*----------------------------------------------------------------------------*/

enum {
	CMC_BIT_ALS = 1,
	CMC_BIT_PS = 2,
};

/*----------------------------------------------------------------------------*/
struct ltr559_i2c_addr {	/*define a series of i2c slave address */
	u8 write_addr;
	u8 ps_thd;		/*PS INT threshold */
};

/*----------------------------------------------------------------------------*/

struct ltr559_priv {
	struct alsps_hw *hw;
	struct i2c_client *client;
	struct work_struct eint_work;
	struct mutex lock;
	/*i2c address group */
	struct ltr559_i2c_addr addr;

	/*misc */
	u16 als_modulus;
	atomic_t i2c_retry;
	/*debounce time after enabling als */
	atomic_t als_debounce;
	/*indicates if the debounce is on */
	atomic_t als_deb_on;
	/*the jiffies representing the end of debounce */
	atomic_t als_deb_end;
	/*mask ps: always return far away */
	atomic_t ps_mask;
	/*debounce time after enabling ps */
	atomic_t ps_debounce;
	/*indicates if the debounce is on */
	atomic_t ps_deb_on;
	/*the jiffies representing the end of debounce */
	atomic_t ps_deb_end;
	atomic_t ps_suspend;
	atomic_t als_suspend;
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
	u16 ps_cali;

	atomic_t als_cmd_val;
	atomic_t ps_cmd_val;
	atomic_t ps_thd_val;
	atomic_t ps_thd_val_high;
	atomic_t ps_thd_val_low;
	ulong enable;
	ulong pending_intr;

	/*early suspend */
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_drv;
#endif

	/* The ALS calibration threshold for Diag . Default Value 400. */
	u32 lux_threshold;
	u32 transmittance;
};

/*----------------------------------------------------------------------------*/
#define DEF_LUX_THRESHOLD (400)
#define DEF_TRANSMITTANCE (1092)

#define ALS_CAL_FILE   "/data/als_cal_data.bin"
#define LTR_DATA_BUF_NUM 1

unsigned int als_cal;
static int als_enable_nodata(int en);
static int als_get_data(int *value, int *status);
static int ltr559_get_als_value(struct ltr559_priv *obj, u16 als);

/*----------------------------------------------------------------------------*/
#ifdef SUPPORT_PSENSOR
struct PS_CALI_DATA_STRUCT {
	int close;
	int far_away;
	int valid;
};
static struct PS_CALI_DATA_STRUCT ps_cali = { 0, 0, 0 };
static int intr_flag_value;
#endif

static struct ltr559_priv *ltr559_obj;

static struct i2c_client *ltr559_i2c_client;

static DEFINE_MUTEX(ltr559_i2c_mutex);
static DEFINE_MUTEX(ltr559_mutex);
static DEFINE_MUTEX(ltrinterrupt_mutex);

static int ltr559_local_init(void);
static int ltr559_remove(void);
#ifdef GN_MTK_BSP_PS_DYNAMIC_CALI
static int last_min_value = 2047;
static int ltr559_dynamic_calibrate(void);
#endif
static int ltr559_init_flag = -1;
static struct alsps_init_info ltr559_init_info = {
	.name = "ltr559",
	.init = ltr559_local_init,
	.uninit = ltr559_remove,

};

#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,alsps"},
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops ltr559_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ltr559_suspend, ltr559_resume)
};
#endif
/*----------------------------------------------------------------------------*/
static struct i2c_driver ltr559_i2c_driver = {
	.probe = ltr559_i2c_probe,
	.remove = ltr559_i2c_remove,
	.detect = ltr559_i2c_detect,
	.id_table = ltr559_i2c_id,
	.driver = {
		   .name = LTR559_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
	.pm = &ltr559_pm_ops,
#endif
#ifdef CONFIG_OF
	.of_match_table = alsps_of_match,
#endif
	},
};

/*
 * #########
 * ## I2C ##
 * #########
 */

// I2C Read
static int ltr559_i2c_read_reg(u8 regnum)
{
	u8 buffer[1], reg_value[1];
	int res = 0;

	mutex_lock(&ltr559_i2c_mutex);

	buffer[0] = regnum;
	res = i2c_master_send(ltr559_obj->client, buffer, 0x1);
	if (res <= 0) {

		APS_ERR("read reg send res = %d\n", res);
		mutex_unlock(&ltr559_i2c_mutex);
		return res;
	}
	res = i2c_master_recv(ltr559_obj->client, reg_value, 0x1);
	if (res <= 0) {
		APS_ERR("read reg recv res = %d\n", res);
		mutex_unlock(&ltr559_i2c_mutex);
		return res;
	}

	mutex_unlock(&ltr559_i2c_mutex);

	return reg_value[0];
}

// I2C Write
static int ltr559_i2c_write_reg(u8 regnum, u8 value)
{
	u8 databuf[2];
	int res = 0;

	mutex_lock(&ltr559_i2c_mutex);
	databuf[0] = regnum;
	databuf[1] = value;
	res = i2c_master_send(ltr559_obj->client, databuf, 0x2);
	mutex_unlock(&ltr559_i2c_mutex);
	if (res < 0) {
		APS_ERR("write reg send res = %d\n", res);
		return res;
	} else
		return 0;
}

#ifdef SUPPORT_PSENSOR
static int ltr559_ps_set_thres(void)
{
	int res;
	u8 databuf[2];

	struct i2c_client *client = ltr559_obj->client;
	struct ltr559_priv *obj = ltr559_obj;

	APS_FUN();

	//BUILD_BUG_ON_ZERO(0>1);

	APS_DBG("ps_cali.valid: %d\n", ps_cali.valid);
	if (ps_cali.valid == 1) {
		databuf[0] = LTR559_PS_THRES_LOW_0;
		databuf[1] = (u8) (ps_cali.far_away & 0x00FF);
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}
		databuf[0] = LTR559_PS_THRES_LOW_1;
		databuf[1] = (u8) ((ps_cali.far_away & 0xFF00) >> 8);
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}
		databuf[0] = LTR559_PS_THRES_UP_0;
		databuf[1] = (u8) (ps_cali.close & 0x00FF);
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}
		databuf[0] = LTR559_PS_THRES_UP_1;
		databuf[1] = (u8) ((ps_cali.close & 0xFF00) >> 8);
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}
	} else {
		databuf[0] = LTR559_PS_THRES_LOW_0;
		databuf[1] =
		    (u8) ((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}
		databuf[0] = LTR559_PS_THRES_LOW_1;
		databuf[1] =
		    (u8) ((atomic_read(&obj->ps_thd_val_low) >> 8) & 0x00FF);

		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}
		databuf[0] = LTR559_PS_THRES_UP_0;
		databuf[1] =
		    (u8) ((atomic_read(&obj->ps_thd_val_high)) & 0x00FF);
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}
		databuf[0] = LTR559_PS_THRES_UP_1;
		databuf[1] =
		    (u8) ((atomic_read(&obj->ps_thd_val_high) >> 8) & 0x00FF);
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}

	}
	APS_DBG("ps low: %d high: %d\n", atomic_read(&obj->ps_thd_val_low),
		atomic_read(&obj->ps_thd_val_high));

	res = 0;
	return res;

EXIT_ERR:
	APS_ERR("set thres: %d\n", res);
	return res;

}

//static int ltr559_ps_enable(int gainrange)
static int ltr559_ps_enable(struct i2c_client *client, int enable)
{
	//struct ltr559_priv *obj = ltr559_obj;
	u8 regdata;
	int err;

	//int setgain;
	APS_LOG("%s ...start!\n", __func__);

	err = ltr559_i2c_write_reg(LTR559_PS_LED, 0x7F);
	if (err < 0) {
		APS_LOG("ltr559 set ps pulse error\n");
		return err;
	}

	err = ltr559_i2c_write_reg(LTR559_PS_N_PULSES, 0x08);
	if (err < 0) {
		APS_LOG("ltr559 set ps pulse error\n");
		return err;
	}

	err = ltr559_i2c_write_reg(0x84, 0x00);
	if (err < 0) {
		APS_LOG("ltr559 set ps meas error\n");
		return err;
	}

	err = ltr559_i2c_write_reg(LTR559_INTERRUPT, 0x01);
	if (err < 0) {
		APS_LOG("ltr559 set ps pulse error\n");
		return err;
	}

	err = ltr559_i2c_write_reg(LTR559_INTERRUPT_PERSIST, 0x10);
	if (err < 0) {
		APS_LOG("ltr559 set ps pulse error\n");
		return err;
	}

	regdata = ltr559_i2c_read_reg(LTR559_PS_CONTR);

	if (enable == 1) {
		APS_LOG("PS: enable ps only\n");

		regdata |= 0x03;
	} else {
		APS_LOG("PS: disable ps only\n");

		regdata &= 0xfc;
	}

	err = ltr559_i2c_write_reg(LTR559_PS_CONTR, regdata);
	if (err < 0) {
		APS_ERR("PS: enable ps err: %d en: %d\n", err, enable);
		return err;
	}
	msleep(WAKEUP_DELAY);
	regdata = ltr559_i2c_read_reg(LTR559_PS_CONTR);

#ifdef GN_MTK_BSP_PS_DYNAMIC_CALI

	if (regdata & 0x02) {
		if (ltr559_dynamic_calibrate() < 0)
			return -1;
	}

	ltr559_ps_set_thres();

#endif

	return 0;

}

static int ltr559_ps_read(struct i2c_client *client, u16 *data)
{
	int psval_lo, psval_hi, psdata;

	psval_lo = ltr559_i2c_read_reg(LTR559_PS_DATA_0);
	APS_DBG("ps_rawdata_psval_lo = %d\n", psval_lo);
	if (psval_lo < 0) {

		APS_DBG("psval_lo error\n");
		psdata = psval_lo;
		goto out;
	}
	psval_hi = ltr559_i2c_read_reg(LTR559_PS_DATA_1);
	APS_DBG("ps_rawdata_psval_hi = %d\n", psval_hi);

	if (psval_hi < 0) {
		APS_DBG("psval_hi error\n");
		psdata = psval_hi;
		goto out;
	}

	psdata = ((psval_hi & 7) * 256) + psval_lo;
	APS_DBG("ps_rawdata = %d\n", psdata);

	*data = psdata;
	return 0;

out:
	final_prox_val = psdata;

	return psdata;
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t als_show(struct device_driver *ddri, char *buf)
{
	int res;

	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return 0;
	}
	res = ltr559_als_read(ltr559_obj->client, &ltr559_obj->als);
	return snprintf(buf, PAGE_SIZE, "%d\n", ltr559_obj->als);

}
#ifdef SUPPORT_PSENSOR
/*----------------------------------------------------------------------------*/
static ssize_t ps_show(struct device_driver *ddri, char *buf)
{
	int res;

	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return 0;
	}
	res = ltr559_ps_read(ltr559_obj->client, &ltr559_obj->ps);
	return snprintf(buf, PAGE_SIZE, "0x%04X\n", ltr559_obj->ps);
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t status_show(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;

	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return 0;
	}

	if (ltr559_obj->hw) {

		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "CUST: %d, (%d %d)\n",
			     ltr559_obj->hw->i2c_num, ltr559_obj->hw->power_id,
			     ltr559_obj->hw->power_vol);

	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "MISC: %d %d\n",
		     atomic_read(&ltr559_obj->als_suspend),
		     atomic_read(&ltr559_obj->ps_suspend));

	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t reg_show(struct device_driver *ddri, char *buf)
{
	int i, len = 0;
	int reg[] = { 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
		      0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
		      0x92, 0x93, 0x94, 0x95, 0x97, 0x98, 0x99, 0x9a, 0x9e
	};

	for (i = 0; i < 27; i++) {
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "reg:0x%04X value: 0x%04X\n", reg[i],
			     ltr559_i2c_read_reg(reg[i]));

	}
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t reg_store(struct device_driver *ddri, const char *buf,
				size_t count)
{
	int ret, value;
	u8 reg;

	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "%hhx %x ", &reg, &value) == 2) {
		APS_DBG
		    ("before write reg: %x, reg_value = %x  write value=%x\n",
		     reg, ltr559_i2c_read_reg(reg), value);
		ret = ltr559_i2c_write_reg(reg, value);
		APS_DBG("after write reg: %x, reg_value = %x\n", reg,
			ltr559_i2c_read_reg(reg));
	} else {
		APS_DBG("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;
}

/******************************************************************************
 * Sysfs attributes for Diag kaka
 ******************************************************************************/

inline uint32_t ltr_alscode2lux(uint32_t alscode)
{
	alscode += ((alscode << 7) + (alscode << 3) + (alscode >> 1));
	alscode <<= 3;
	if (ltr559_obj->transmittance > 0)
		alscode /= ltr559_obj->transmittance;
	else
		return 0;

	return alscode;
}

static inline int32_t ltr559_als_get_data_avg(int sSampleNo)
{
	int32_t DataCount = 0;
	int32_t sAveAlsData = 0;

	u16 als_reading = 0;
	int result = 0;

	struct ltr559_priv *obj = NULL;

	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return -1;
	}
	obj = ltr559_obj;

	result = ltr559_als_read(obj->client, &als_reading);
	APS_DBG("[%s]: Ignore first als value:%d\n", __func__, als_reading);

	while (DataCount < sSampleNo) {
		msleep(50);
		result = ltr559_als_read(obj->client, &als_reading);
		APS_ERR("%s: [#23][LTR]als code = %d\n", __func__,
		       als_reading);
		sAveAlsData += als_reading;
		DataCount++;
	}
	sAveAlsData /= sSampleNo;
	return sAveAlsData;
}

static bool als_store_cali_transmittance_in_file(const char *filename,
						 unsigned int value)
{
	struct file *cali_file;
	mm_segment_t fs;
	char w_buf[LTR_DATA_BUF_NUM * sizeof(unsigned int) * 2 + 1] = { 0 };
	char r_buf[LTR_DATA_BUF_NUM * sizeof(unsigned int) * 2 + 1] = { 0 };
	int i;
	char *dest = w_buf;

	APS_ERR("%s enter", __func__);
	cali_file = filp_open(filename, O_CREAT | O_RDWR, 0777);

	if (IS_ERR(cali_file)) {
		APS_ERR("%s open error exit!\n", __func__);
		return false;
	}

	fs = get_fs();
	set_fs(get_ds());

	for (i = 0; i < LTR_DATA_BUF_NUM; i++) {
		sprintf(dest, "%02X", value & 0x000000FF);
		dest += 2;
		sprintf(dest, "%02X", (value >> 8) & 0x000000FF);
		dest += 2;
		sprintf(dest, "%02X", (value >> 16) & 0x000000FF);
		dest += 2;
		sprintf(dest, "%02X", (value >> 24) & 0x000000FF);
		dest += 2;
	}

	APS_ERR("w_buf: %s\n", w_buf);
	cali_file->f_op->write(cali_file, (void *)w_buf,
			       LTR_DATA_BUF_NUM * sizeof(unsigned int) *
			       2 + 1, &cali_file->f_pos);
	cali_file->f_pos = 0x00;
	cali_file->f_op->read(cali_file, (void *)r_buf,
			      LTR_DATA_BUF_NUM * sizeof(unsigned int) *
			      2 + 1, &cali_file->f_pos);

	for (i = 0; i < LTR_DATA_BUF_NUM * sizeof(unsigned int) * 2 + 1;
	     i++) {
		if (r_buf[i] != w_buf[i]) {
			filp_close(cali_file, NULL);
			APS_ERR("%s read back error! exit!\n",
			       __func__);
			return false;
		}
	}

	set_fs(fs);

	filp_close(cali_file, NULL);
	APS_ERR("pass\n");
	return true;
}

static ssize_t enable_show(struct device_driver *ddri, char *buf)
{
	int32_t enabled = 0;
	int32_t ret = 0;
	u8 regdata = 0;

	if (test_bit(CMC_BIT_ALS, &ltr559_obj->enable))
		enabled = 1;
	else
		enabled = 0;

	regdata = ltr559_i2c_read_reg(LTR559_ALS_CONTR);

	if (regdata & 0x01) {
		APS_LOG("ALS Enabled\n");
		ret = 1;
	} else {
		APS_LOG("ALS Disabled\n");
		ret = 0;
	}

	if (enabled != ret)
		APS_ERR(
		       "%s: driver and sensor mismatch! driver_enable=0x%x, sensor_enable=%x\n",
		       __func__, enabled, ret);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t enable_store(struct device_driver *ddri,
				       const char *buf, size_t size)
{
	uint8_t en;

	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else {
		APS_ERR("%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	als_enable_nodata(en);

	APS_DBG("%s: Enable ALS : %d\n", __func__, en);

	return size;
}

static ssize_t lux_show(struct device_driver *ddri, char *buf)
{
	int32_t als_reading = 0;

	als_reading = ltr559_als_get_data_avg(5);

	als_reading = ltr_alscode2lux(als_reading);

	return scnprintf(buf, PAGE_SIZE, "%d lux\n", als_reading);

}

static ssize_t lux_threshold_show(struct device_driver *ddri,
					     char *buf)
{
	int32_t lux_threshold;

	lux_threshold = ltr559_obj->lux_threshold;
	return scnprintf(buf, PAGE_SIZE, "%d\n", lux_threshold);
}

static ssize_t lux_threshold_store(struct device_driver *ddri,
					      const char *buf, size_t size)
{
	unsigned long value = 0;
	int ret;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0) {
		APS_ERR("%s:strict_strtoul failed, ret=0x%x\n",
		       __func__, ret);
		return ret;
	}
	ltr559_obj->lux_threshold = value;
	return size;
}

//kaka
static ssize_t transmittance_show(struct device_driver *ddri,
					     char *buf)
{
	int32_t transmittance;

	transmittance = ltr559_obj->transmittance;
	return scnprintf(buf, PAGE_SIZE, "%d\n", transmittance);
}

static ssize_t transmittance_store(struct device_driver *ddri,
					      const char *buf, size_t size)
{
	unsigned long value = 0;
	int ret;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0) {
		APS_ERR("%s:strict_strtoul failed, ret=0x%x\n",
		       __func__, ret);
		return ret;
	}
	ltr559_obj->transmittance = value;
	return size;
}

static ssize_t cali_Light_show(struct device_driver *ddri, char *buf)
{
	int32_t als_reading;
	int32_t als_value_cali_adc;
	int32_t als_value_cali;
	bool result = false;

	APS_ERR("%s:[#23][LTR]Start Cali light...\n", __func__);

	msleep(150);
	als_reading = ltr559_als_get_data_avg(5);

	als_value_cali_adc = als_reading;
	als_value_cali = ltr_alscode2lux(als_reading);

	if (((als_value_cali * ltr559_obj->transmittance) /
	     (ltr559_obj->lux_threshold)) > 0
	    && (als_value_cali_adc <= 65535)) {

		/* transmittance for cali */
		ltr559_obj->transmittance =
			(als_value_cali * ltr559_obj->transmittance) /
			(ltr559_obj->lux_threshold);

		result =
		    als_store_cali_transmittance_in_file(ALS_CAL_FILE,
					ltr559_obj->transmittance);
		APS_ERR("%s: result:=%d\n", __func__, result);
		//calculate lux base on calibrated transmittance
		als_value_cali = ltr_alscode2lux(als_reading);
		APS_ERR(
		       "%s:[#23][LTR]cali light done!!! als_value_cali = %d lux, ltr559_obj->transmittance = %d, als_value_cali_adc = %d code\n",
		       __func__, als_value_cali, ltr559_obj->transmittance,
		       als_value_cali_adc);

	} else {
		APS_ERR(
		       "%s:[#23][LTR]cali light fail!!! cci_als_value_cali = %d lux, ltr559_obj->transmittance = %d, als_value_cali_adc = %d code\n",
		       __func__, als_value_cali, ltr559_obj->transmittance,
		       als_value_cali_adc);
		result = false;
	}

	return scnprintf(buf, PAGE_SIZE,
			 "%s: als_value_cali = %d lux, ltr559_obj->transmittance = %d, als_value_cali_adc = %d code\n",
			 result ? "PASSED" : "FAIL", als_value_cali,
			 ltr559_obj->transmittance, als_value_cali_adc);
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR_RO(als);
#ifdef SUPPORT_PSENSOR
static DRIVER_ATTR_RO(ps);
#endif
static DRIVER_ATTR_RO(status);
static DRIVER_ATTR_RW(reg);
/*----------------------------------------------------------------------------*/
//For Diag to Calibrate the ALS
static DRIVER_ATTR_RW(enable);
static DRIVER_ATTR_RO(lux);
static DRIVER_ATTR_RW(lux_threshold);
static DRIVER_ATTR_RW(transmittance);
static DRIVER_ATTR_RO(cali_Light);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *ltr559_attr_list[] = {
	&driver_attr_als,
#ifdef SUPPORT_PSENSOR
	&driver_attr_ps,
#endif
	&driver_attr_status,
	&driver_attr_reg,
	&driver_attr_enable,
	&driver_attr_lux,
	&driver_attr_lux_threshold,
	&driver_attr_transmittance,
	&driver_attr_cali_Light,
};

/*----------------------------------------------------------------------------*/
static int ltr559_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(ltr559_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, ltr559_attr_list[idx]);
		if (err) {
			APS_ERR("driver_create_file (%s) = %d\n",
				ltr559_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int ltr559_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(ltr559_attr_list);

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, ltr559_attr_list[idx]);

	return err;
}

/************************
 * ALS CONFIG
 ************************/

#ifdef GN_MTK_BSP_PS_DYNAMIC_CALI
static int ltr559_dynamic_calibrate(void)
{
	//int ret = 0;
	int i = 0;
	int j = 0;
	int data = 0;
	int noise = 0;
	//int len = 0;
	//int err = 0;
	int max = 0;
	//int idx_table = 0;
	unsigned long data_total = 0;
	struct ltr559_priv *obj = ltr559_obj;

	APS_FUN(f);
	if (!obj)
		goto err;

	msleep(20);
	for (i = 0; i < 5; i++) {
		if (max++ > 5)
			goto err;

		msleep(20);

		ltr559_ps_read(obj->client, &obj->ps);
		data = obj->ps;

		if (data == 0)
			j++;

		data_total += data;
	}
	noise = data_total / (5 - j);
	//isadjust = 1;
	if ((noise < last_min_value + 100)) {
		last_min_value = noise;
		if (noise < 50) {
			atomic_set(&obj->ps_thd_val_high, noise + 50);	//15
			atomic_set(&obj->ps_thd_val_low, noise + 20);	//14

		} else if (noise < 100) {
			atomic_set(&obj->ps_thd_val_high, noise + 60);	//15
			atomic_set(&obj->ps_thd_val_low, noise + 30);	//14
		} else if (noise < 200) {
			atomic_set(&obj->ps_thd_val_high, noise + 90);	//15
			atomic_set(&obj->ps_thd_val_low, noise + 60);	//14
		} else if (noise < 300) {
			atomic_set(&obj->ps_thd_val_high, noise + 130);	//15
			atomic_set(&obj->ps_thd_val_low, noise + 90);	//14
		} else if (noise < 400) {
			atomic_set(&obj->ps_thd_val_high, noise + 150);	//15
			atomic_set(&obj->ps_thd_val_low, noise + 120);	//14
		} else if (noise < 600) {
			atomic_set(&obj->ps_thd_val_high, noise + 220);	//15
			atomic_set(&obj->ps_thd_val_low, noise + 180);	//14
		} else if (noise < 800) {
			atomic_set(&obj->ps_thd_val_high, noise + 280);	//15
			atomic_set(&obj->ps_thd_val_low, noise + 240);	//14
		} else {
			atomic_set(&obj->ps_thd_val_high, 1000);
			atomic_set(&obj->ps_thd_val_low, 880);
		}

	}
	APS_DBG(" calibrate:noise=%d, thdlow= %d , thdhigh = %d\n", noise,
		atomic_read(&obj->ps_thd_val_low),
		atomic_read(&obj->ps_thd_val_high));

	return 0;
err:
	APS_ERR("%s fail!!!\n", __func__);
	return -1;
}
#endif

static int ltr559_als_enable(struct i2c_client *client, int enable)
{
	//struct ltr559_priv *obj = i2c_get_clientdata(client);
	int err = 0;
	u8 regdata = 0;
	//if (enable == obj->als_enable)
	//      return 0;

	regdata = ltr559_i2c_read_reg(LTR559_ALS_CONTR);

	if (enable == 1) {
		APS_LOG("ALS(1): enable als only\n");
		regdata |= 0x01;
	} else {
		APS_LOG("ALS(1): disable als only\n");
		regdata &= 0xfe;
	}

	err = ltr559_i2c_write_reg(LTR559_ALS_CONTR, regdata);
	if (err < 0) {
		APS_ERR("ALS: enable als err: %d en: %d\n", err, enable);
		return err;
	}
	//obj->als_enable = enable;

	mdelay(WAKEUP_DELAY);

	return 0;

}

static int ltr559_als_read(struct i2c_client *client, u16 *data)
{
	struct ltr559_priv *obj = i2c_get_clientdata(client);
	int alsval_ch0_lo, alsval_ch0_hi, alsval_ch0;
	int alsval_ch1_lo, alsval_ch1_hi, alsval_ch1;
	int luxdata_int;
	int ratio;

	if (atomic_read(&obj->als_suspend)) {
		luxdata_int = 0;
		goto out;
	}

	alsval_ch1_lo = ltr559_i2c_read_reg(LTR559_ALS_DATA_CH1_0);
	alsval_ch1_hi = ltr559_i2c_read_reg(LTR559_ALS_DATA_CH1_1);
	alsval_ch1 = (alsval_ch1_hi * 256) + alsval_ch1_lo;
	APS_DBG("alsval_ch1_lo = %d,alsval_ch1_hi=%d,alsval_ch1=%d\n",
		alsval_ch1_lo, alsval_ch1_hi, alsval_ch1);

	alsval_ch0_lo = ltr559_i2c_read_reg(LTR559_ALS_DATA_CH0_0);
	alsval_ch0_hi = ltr559_i2c_read_reg(LTR559_ALS_DATA_CH0_1);
	alsval_ch0 = (alsval_ch0_hi * 256) + alsval_ch0_lo;
	APS_DBG("alsval_ch0_lo = %d,alsval_ch0_hi=%d,alsval_ch0=%d\n",
		alsval_ch0_lo, alsval_ch0_hi, alsval_ch0);

	if ((alsval_ch1 == 0) || (alsval_ch0 == 0))
		ratio = 0;
	else
		ratio = (alsval_ch1 * 100) / (alsval_ch0 + alsval_ch1);

	APS_DBG("ratio = %d  gainrange = %d\n", ratio, als_gainrange);
	/*CWF light*/
	if (ratio < 50) {
		luxdata_int =
		    (((17743 * alsval_ch0) +
		      (11059 * alsval_ch1)) / als_gainrange) / 1040;
	} /*D65 light*/
	else if ((ratio < 66) && (ratio >= 50)) {
		luxdata_int =
			(((5926 * alsval_ch0) +
			(1185 * alsval_ch1)) / als_gainrange) / 884;
	} /* A light */
	else if ((ratio < 99) && (ratio >= 66)) {
		luxdata_int =
		    (((5926 * alsval_ch0) +
			(1185 * alsval_ch1)) / als_gainrange) / 1030;
	} else {
		luxdata_int = 0;
	}
	APS_DBG("als_ratio = %d\n", ratio);
	APS_DBG("als_org_value_lux = %d\n", luxdata_int);

out:
	*data = luxdata_int;
	final_lux_val = luxdata_int;
	return 0;

}

void ltr559_eint_func(void)
{
	struct ltr559_priv *obj = ltr559_obj;

	APS_FUN();

	if (!obj)
		return;

	int_top_time = sched_clock();
	schedule_work(&obj->eint_work);
	//schedule_delayed_work(&obj->eint_work);
}

#if defined(CONFIG_OF)
static irqreturn_t ltr559_eint_handler(int irq, void *desc)
{
	disable_irq_nosync(ltr559_obj->irq);
	ltr559_eint_func();

	return IRQ_HANDLED;
}
#endif

/*----------------------------------------------------------------------------*/
int ltr559_setup_eint(struct i2c_client *client)
{
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;
	u32 ints[2] = { 0, 0 };

	APS_FUN();

	/* gpio setting */
	pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		APS_ERR("Cannot find alsps pinctrl!\n");
		return ret;
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
		return ret;
	}
	pinctrl_select_state(pinctrl, pins_cfg);

/* eint request */
	if (ltr559_obj->irq_node) {
		of_property_read_u32_array(ltr559_obj->irq_node, "debounce",
					   ints, ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);
		APS_LOG("ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);
		ltr559_obj->irq = irq_of_parse_and_map(ltr559_obj->irq_node, 0);
		APS_LOG("ltr559_obj->irq = %d\n", ltr559_obj->irq);
		if (!ltr559_obj->irq) {
			APS_ERR("irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}
		if (request_irq(ltr559_obj->irq, ltr559_eint_handler,
			IRQF_TRIGGER_FALLING, "PS-eint", NULL)) {
			APS_ERR("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}
	} else {
		APS_ERR("null irq node!!\n");
		return -EINVAL;
	}

	return 0;
}

static int ltr559_check_and_clear_intr(struct i2c_client *client)
{
	int res, intp, intl;
	u8 buffer[2];
	u8 temp;

	APS_FUN();

	buffer[0] = LTR559_ALS_PS_STATUS;
	res = i2c_master_send(client, buffer, 0x1);
	if (res <= 0)
		goto EXIT_ERR;

	res = i2c_master_recv(client, buffer, 0x1);
	if (res <= 0)
		goto EXIT_ERR;

	temp = buffer[0];
	res = 1;
	intp = 0;
	intl = 0;
	if (0 != (buffer[0] & 0x02)) {
		res = 0;
		intp = 1;
	}
	if (0 != (buffer[0] & 0x08)) {
		res = 0;
		intl = 1;
	}

	if (res == 0) {
		if ((intp == 1) && (intl == 0))
			buffer[1] = buffer[0] & 0xfD;
		else if ((intp == 0) && (intl == 1))
			buffer[1] = buffer[0] & 0xf7;
		else
			buffer[1] = buffer[0] & 0xf5;

		buffer[0] = LTR559_ALS_PS_STATUS;
		res = i2c_master_send(client, buffer, 0x2);
		if (res <= 0)
			goto EXIT_ERR;
		else
			res = 0;
	} else
		return 0;

EXIT_ERR:
	APS_ERR("%s fail\n", __func__);
	return 1;
}


/*----------------------------------------------------------------------------*/
#ifdef SUPPORT_PSENSOR
static int ltr559_check_intr(struct i2c_client *client)
{

	int res, intp, intl;
	u8 buffer[2];

	APS_FUN();

	buffer[0] = ltr559_i2c_read_reg(LTR559_ALS_PS_STATUS);

	APS_LOG("status = %x\n", buffer[0]);

	res = 1;
	intp = 0;
	intl = 0;
	if (0 != (buffer[0] & 0x02)) {

		res = 0;
		intp = 1;
	}
	if (0 != (buffer[0] & 0x08)) {
		res = 0;
		intl = 1;
	}

	if (res == 0) {
		if ((intp == 1) && (intl == 0)) {
			APS_LOG("PS interrupt\n");
			buffer[1] = buffer[0] & 0xfD;

		} else if ((intp == 0) && (intl == 1)) {
			APS_LOG("ALS interrupt\n");
			buffer[1] = buffer[0] & 0xf7;
		} else {
			APS_LOG("Check ALS/PS interrupt error\n");
			buffer[1] = buffer[0] & 0xf5;
		}
	}

	return res;
}

static int ltr559_clear_intr(struct i2c_client *client)
{
	u8 buffer[2];

	APS_FUN();

	//APS_DBG("buffer[0] = %d\n",buffer[0]);
	buffer[0] = ltr559_i2c_read_reg(LTR559_ALS_PS_STATUS);
	APS_LOG("status = %x\n", buffer[0]);

	return 0;
}
#endif

static int ltr559_devinit(void)
{
	int res;
	int init_ps_gain;
	int init_als_gain;
	u8 databuf[2];

	struct i2c_client *client = ltr559_obj->client;

	struct ltr559_priv *obj = ltr559_obj;

	mdelay(PON_DELAY);

	init_ps_gain = MODE_PS_Gain16;

	APS_LOG("LTR559_PS setgain = %d!\n", init_ps_gain);
	res = ltr559_i2c_write_reg(LTR559_PS_CONTR, init_ps_gain);
	if (res < 0) {
		APS_LOG("ltr559 set ps gain error\n");
		return res;
	}

	res = ltr559_i2c_write_reg(LTR559_ALS_MEAS_RATE, 0x01);
	if (res < 0) {
		APS_LOG("ltr559 set als meas rate error\n");
		return res;
	}

	mdelay(WAKEUP_DELAY);

	res = ltr559_i2c_write_reg(LTR559_PS_LED, 0x7F);
	if (res < 0) {
		APS_LOG("ltr559 set ps pulse error\n");
		return res;
	}

	res = ltr559_i2c_write_reg(LTR559_PS_N_PULSES, 0x08);
	if (res < 0) {
		APS_LOG("ltr559 set ps pulse error\n");
		return res;
	}
	// Enable ALS to Full Range at startup
	als_gainrange = ALS_RANGE_8K;

	init_als_gain = als_gainrange;

	switch (init_als_gain) {
	case ALS_RANGE_64K:
		res = ltr559_i2c_write_reg(LTR559_ALS_CONTR, MODE_ALS_Range1);
		break;

	case ALS_RANGE_32K:
		res = ltr559_i2c_write_reg(LTR559_ALS_CONTR, MODE_ALS_Range2);
		break;

	case ALS_RANGE_16K:
		res = ltr559_i2c_write_reg(LTR559_ALS_CONTR, MODE_ALS_Range3);
		break;

	case ALS_RANGE_8K:
		res = ltr559_i2c_write_reg(LTR559_ALS_CONTR, MODE_ALS_Range4);
		break;

	case ALS_RANGE_1300:
		res = ltr559_i2c_write_reg(LTR559_ALS_CONTR, MODE_ALS_Range5);
		break;

	case ALS_RANGE_600:
		res = ltr559_i2c_write_reg(LTR559_ALS_CONTR, MODE_ALS_Range6);
		break;

	default:
		res = ltr559_i2c_write_reg(LTR559_ALS_CONTR, MODE_ALS_Range1);
		APS_ERR("proxmy sensor gainrange %d!\n", init_als_gain);
		break;
	}

	/*for interrupt work mode support */
	if (obj->hw->polling_mode_ps == 0) {
		APS_LOG("eint enable");
#ifdef SUPPORT_PSENSOR
		ltr559_ps_set_thres();
#endif
		databuf[0] = LTR559_INTERRUPT;
		databuf[1] = 0x01;
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}

		databuf[0] = LTR559_INTERRUPT_PERSIST;
		databuf[1] = 0x20;
		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {
			goto EXIT_ERR;
			return ltr559_ERR_I2C;
		}

	}

	res = ltr559_check_and_clear_intr(client);
	if (res) {
		APS_ERR("check/clear intr: %d\n", res);
		return res;
	}

	res = 0;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;

}

/*----------------------------------------------------------------------------*/

static int ltr559_get_als_value(struct ltr559_priv *obj, u16 als)
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

	APS_ERR("ALS: %05d => %05d (-1)\n", als,
		obj->hw->als_value[idx]);
	return -1;

}

#ifdef SUPPORT_PSENSOR
/*----------------------------------------------------------------------------*/
static int ltr559_get_ps_value(struct ltr559_priv *obj, u16 ps)
{
	int val, invalid = 0;
	static int val_temp = 1;

	if (ps > atomic_read(&obj->ps_thd_val_high)) {
		val = 0;	/*close */
		val_temp = 0;
		intr_flag_value = 1;
	} else if (ps < atomic_read(&obj->ps_thd_val_low)) {
		val = 1;	/*far away */
		val_temp = 1;
		intr_flag_value = 0;
	} else
		val = val_temp;

	if (atomic_read(&obj->ps_suspend)) {
		invalid = 1;
	} else if (atomic_read(&obj->ps_deb_on) == 1) {
		unsigned long endt = atomic_read(&obj->ps_deb_end);

		if (time_after(jiffies, endt))
			atomic_set(&obj->ps_deb_on, 0);

		if (atomic_read(&obj->ps_deb_on) == 1)
			invalid = 1;
	} else if (obj->als > 50000) {
		//invalid = 1;
		APS_DBG("ligh too high will result to failt proximiy\n");
		return 1;	/*far away */
	}

	if (!invalid) {
		APS_DBG("PS:  %05d => %05d\n", ps, val);
		return val;
	} else {
		return -1;
	}
}
#endif

/*----------------------------------------------------------------------------*/
static int als_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

static int als_enable_nodata(int en)
{
	int res = 0;

	APS_LOG("ltr559_obj als enable value = %d\n", en);

	mutex_lock(&ltr559_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &ltr559_obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &ltr559_obj->enable);
	mutex_unlock(&ltr559_mutex);
	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return -1;
	}
	res = ltr559_als_enable(ltr559_obj->client, en);
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

	struct ltr559_priv *obj = NULL;
	int cali_lux = 0;

	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return -1;
	}
	obj = ltr559_obj;
	err = ltr559_als_read(obj->client, &obj->als);
	if (err)
		err = -1;
	else {

		cali_lux = ltr_alscode2lux(obj->als);
		/* *value = ltr559_get_als_value(obj, cali_lux); */
		/* APS_ERR("als: %d\n", obj->als); */
		/* *value = obj->als; */
		*value = cali_lux;
		APS_DBG("als after cali value: %d\n", cali_lux);
		if (*value < 0)
			err = -1;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return err;
}

#ifdef SUPPORT_PSENSOR
static int ps_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

static int ps_enable_nodata(int en)
{
	int res = 0;

	APS_LOG("ltr559_obj als enable value = %d\n", en);

	mutex_lock(&ltr559_mutex);
	if (en)
		set_bit(CMC_BIT_PS, &ltr559_obj->enable);

	else
		clear_bit(CMC_BIT_PS, &ltr559_obj->enable);

	mutex_unlock(&ltr559_mutex);
	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return -1;
	}

	res = ltr559_ps_enable(ltr559_obj->client, en);
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

	if (!ltr559_obj) {
		APS_ERR("ltr559_obj is null!!\n");
		return -1;
	}

	err = ltr559_ps_read(ltr559_obj->client, &ltr559_obj->ps);
	if (err)
		err = -1;
	else {
		*value = ltr559_get_ps_value(ltr559_obj, ltr559_obj->ps);
		if (*value < 0)
			err = -1;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return err;
}
#endif
/*----------------------------------------------------------------------------*/
/*for interrupt work mode support */
static void ltr559_eint_work(struct work_struct *work)
{
#ifdef SUPPORT_PSENSOR
	struct ltr559_priv *obj =
	    (struct ltr559_priv *)container_of(work, struct ltr559_priv,
					       eint_work);
	int err;
	u8 databuf[2];
	int res = 0;
	int value = 1;
	int i;
	int reg[] = { 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
		      0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
		      0x92, 0x93, 0x94, 0x95, 0x97, 0x98, 0x99, 0x9a, 0x9e
	};

	APS_FUN();

	mutex_lock(&ltrinterrupt_mutex);

	err = ltr559_check_intr(obj->client);
	if (err < 0) {
		APS_ERR("%s check intrs: %d\n", __func__, err);
	} else {
		//get raw data
		ltr559_ps_read(obj->client, &obj->ps);
		if (obj->ps < 0) {
			err = -1;
			goto REPORT;
			return;
		}

		APS_DBG("%s rawdata ps=%d als_ch0=%d!\n", __func__,
			obj->ps, obj->als);
		value = ltr559_get_ps_value(obj, obj->ps);
		APS_DBG("intr_flag_value=%d\n", intr_flag_value);
		if (intr_flag_value) {
			APS_DBG(" interrupt value ps will < ps_threshold_low");

			databuf[0] = LTR559_PS_THRES_LOW_0;
			databuf[1] =
			    (u8) ((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0) {
				goto REPORT;
				return;
			}
			databuf[0] = LTR559_PS_THRES_LOW_1;
			databuf[1] =
			    (u8) (((atomic_read(&obj->ps_thd_val_low)) & 0xFF00)
				  >> 8);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0) {
				goto REPORT;
				return;
			}
			databuf[0] = LTR559_PS_THRES_UP_0;
			databuf[1] = (u8) (0x00FF);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0) {
				goto REPORT;
				return;
			}
			databuf[0] = LTR559_PS_THRES_UP_1;
			databuf[1] = (u8) ((0xFF00) >> 8);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0) {
				goto REPORT;
				return;
			}
		} else {
//ble start

			for (i = 0; i < 27; i++) {
				APS_DBG("reg:0x%04X value: 0x%04X\n", reg[i],
				       ltr559_i2c_read_reg(reg[i]));

			}

//ble end
#ifdef GN_MTK_BSP_PS_DYNAMIC_CALI
			if (obj->ps < (last_min_value - 100)) {
				last_min_value = obj->ps;
				APS_DBG(" last_min_value is %d,noise is %d\n",
					last_min_value, obj->ps);
				if (obj->ps < 50) {
					atomic_set(&obj->ps_thd_val_high,
						   obj->ps + 50);
					atomic_set(&obj->ps_thd_val_low,
						   obj->ps + 20);
				} else if (obj->ps < 100) {
					atomic_set(&obj->ps_thd_val_high,
						   obj->ps + 60);
					atomic_set(&obj->ps_thd_val_low,
						   obj->ps + 30);
				} else if (obj->ps < 200) {
					atomic_set(&obj->ps_thd_val_high,
						   obj->ps + 90);
					atomic_set(&obj->ps_thd_val_low,
						   obj->ps + 60);
				} else if (obj->ps < 300) {
					atomic_set(&obj->ps_thd_val_high,
						   obj->ps + 130);
					atomic_set(&obj->ps_thd_val_low,
						   obj->ps + 90);
				} else if (obj->ps < 400) {
					atomic_set(&obj->ps_thd_val_high,
						   obj->ps + 150);
					atomic_set(&obj->ps_thd_val_low,
						   obj->ps + 120);
				} else if (obj->ps < 600) {
					atomic_set(&obj->ps_thd_val_high,
						   obj->ps + 220);
					atomic_set(&obj->ps_thd_val_low,
						   obj->ps + 180);
				} else if (obj->ps < 800) {
					atomic_set(&obj->ps_thd_val_high,
						   obj->ps + 280);
					atomic_set(&obj->ps_thd_val_low,
						   obj->ps + 240);
				} else {
					atomic_set(&obj->ps_thd_val_high, 1000);
					atomic_set(&obj->ps_thd_val_low, 880);
				}
			}
#endif
			APS_DBG
			    (" interrupt value ps will > ps_threshold_high\n");
			databuf[0] = LTR559_PS_THRES_LOW_0;
			databuf[1] = (u8) (0 & 0x00FF);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0) {
				goto REPORT;
				return;
			}
			databuf[0] = LTR559_PS_THRES_LOW_1;
			databuf[1] = (u8) ((0 & 0xFF00) >> 8);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0) {
				goto REPORT;
				return;
			}
			databuf[0] = LTR559_PS_THRES_UP_0;
			databuf[1] =
			    (u8) ((atomic_read(&obj->ps_thd_val_high)) &
				  0x00FF);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0) {
				goto REPORT;
				return;
			}
			databuf[0] = LTR559_PS_THRES_UP_1;
			databuf[1] =
			    (u8) (((atomic_read(&obj->ps_thd_val_high)) &
				   0xFF00) >> 8);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if (res <= 0) {
				goto REPORT;
				return;
			}
		}
	}
	ltr559_clear_intr(obj->client);

REPORT:
	enable_irq(ltr559_obj->irq);
	mutex_unlock(&ltrinterrupt_mutex);
	ps_report_interrupt_data(value);
#endif
}

/******************************************************************************
 * Function Configuration
 *****************************************************************************/
static int ltr559_open(struct inode *inode, struct file *file)
{
	file->private_data = ltr559_i2c_client;

	if (!file->private_data) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int ltr559_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

/*----------------------------------------------------------------------------*/

static long ltr559_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct ltr559_priv *obj = i2c_get_clientdata(client);
	int err = 0;
	void __user *ptr = (void __user *)arg;
	int dat;
	uint32_t enable;
	int ps_cali;
#ifdef SUPPORT_PSENSOR
	int ps_result;
	int threshold[2];
#endif
	APS_DBG("cmd= %d\n", cmd);
	switch (cmd) {
	case ALSPS_SET_PS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
#ifdef SUPPORT_PSENSOR
		err = ltr559_ps_enable(obj->client, enable);
		if (err < 0) {
			APS_ERR("enable ps fail: %d en: %d\n", err, enable);
			goto err_out;
		}
		set_bit(CMC_BIT_PS, &obj->enable);
#endif
		break;

	case ALSPS_GET_PS_MODE:
#ifdef SUPPORT_PSENSOR
		enable = test_bit(CMC_BIT_PS, &obj->enable) ? (1) : (0);
		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
#endif
		break;

	case ALSPS_GET_PS_DATA:
#ifdef SUPPORT_PSENSOR
		APS_DBG("ALSPS_GET_PS_DATA\n");
		err = ltr559_ps_read(obj->client, &obj->ps);
		if (err < 0)
			goto err_out;

		dat = ltr559_get_ps_value(obj, obj->ps);
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
#endif
		break;

	case ALSPS_GET_PS_RAW_DATA:
#ifdef SUPPORT_PSENSOR
		err = ltr559_ps_read(obj->client, &obj->ps);
		if (err < 0)
			goto err_out;

		dat = obj->ps;
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
#endif
		break;

	case ALSPS_SET_ALS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		//if(enable)
		//{
		err = ltr559_als_enable(obj->client, enable);
		if (err < 0) {
			APS_ERR("enable als fail: %d en: %d\n", err, enable);
			goto err_out;
		}
		set_bit(CMC_BIT_ALS, &obj->enable);
		//}
		//else
		//{
		//    err = ltr559_als_disable();
		//      if(err < 0)
		//      {
		//              APS_ERR("disable als fail: %d\n", err);
		//              goto err_out;
		//      }
		//      clear_bit(CMC_BIT_ALS, &obj->enable);
		//}
		break;

	case ALSPS_GET_ALS_MODE:
		enable = test_bit(CMC_BIT_ALS, &obj->enable) ? (1) : (0);
		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_ALS_DATA:
		err = ltr559_als_read(obj->client, &obj->als);
		if (err < 0)
			goto err_out;

		dat = ltr559_get_als_value(obj, obj->als);
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_ALS_RAW_DATA:
		err = ltr559_als_read(obj->client, &obj->als);
		if (err < 0)
			goto err_out;

		dat = obj->als;
		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

/*---------------------for factory mode test--------------------*/
	case ALSPS_GET_PS_TEST_RESULT:
#ifdef SUPPORT_PSENSOR
		err = ltr559_ps_read(obj->client, &obj->ps);
		if (err)
			goto err_out;

		if (obj->ps > atomic_read(&obj->ps_thd_val_high))
			ps_result = 0;
		else
			ps_result = 1;

		if (copy_to_user(ptr, &ps_result, sizeof(ps_result))) {
			err = -EFAULT;
			goto err_out;
		}
#endif
		break;

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
		if (copy_from_user(&ps_cali, ptr, sizeof(ps_cali))) {
			err = -EFAULT;
			goto err_out;
		}

		obj->ps_cali = ps_cali;
		break;

	case ALSPS_SET_PS_THRESHOLD:
#ifdef SUPPORT_PSENSOR
		if (copy_from_user(threshold, ptr, sizeof(threshold))) {
			err = -EFAULT;
			goto err_out;
		}
		APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n", __func__,
			threshold[0], threshold[1]);
		atomic_set(&obj->ps_thd_val_high,
			   (threshold[0] + obj->ps_cali));
		atomic_set(&obj->ps_thd_val_low, (threshold[1] + obj->ps_cali));

		//set_psensor_threshold(obj->client);
#endif
		break;

	case ALSPS_GET_PS_THRESHOLD_HIGH:
#ifdef SUPPORT_PSENSOR
		threshold[0] =
		    atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
		APS_ERR("%s get threshold high: 0x%x\n", __func__,
			threshold[0]);
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
#endif
		break;

	case ALSPS_GET_PS_THRESHOLD_LOW:
#ifdef SUPPORT_PSENSOR
		threshold[0] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali;
		APS_ERR("%s get threshold low: 0x%x\n", __func__, threshold[0]);
		if (copy_to_user(ptr, &threshold[0], sizeof(threshold[0]))) {
			err = -EFAULT;
			goto err_out;
		}
#endif
		break;

	default:
		APS_ERR("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

err_out:
	return err;
}

/*----------------------------------------------------------------------------*/
static const struct file_operations ltr559_fops = {
	//.owner = THIS_MODULE,
	.open = ltr559_open,
	.release = ltr559_release,
	.unlocked_ioctl = ltr559_unlocked_ioctl,
};

/*----------------------------------------------------------------------------*/
static struct miscdevice ltr559_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &ltr559_fops,
};

#ifdef CONFIG_PM_SLEEP
static int ltr559_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltr559_priv *obj = i2c_get_clientdata(client);
	int err;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	atomic_set(&obj->als_suspend, 1);
	err = ltr559_als_enable(obj->client, 0);
	if (err < 0) {
		APS_ERR("disable als: %d\n", err);
		return err;
	}

#ifdef SUPPORT_PSENSOR
	atomic_set(&obj->ps_suspend, 1);
	err = ltr559_ps_enable(obj->client, 0);
	if (err < 0) {
		APS_ERR("disable ps:  %d\n", err);
		return err;
	}
#endif

	return 0;
}
/*----------------------------------------------------------------------------*/
static int ltr559_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltr559_priv *obj = i2c_get_clientdata(client);
	int err;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	atomic_set(&obj->als_suspend, 0);
	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = ltr559_als_enable(obj->client, 1);
		if (err < 0)
			APS_ERR("enable als fail: %d\n", err);
	}
	atomic_set(&obj->ps_suspend, 0);
	if (test_bit(CMC_BIT_PS, &obj->enable)) {
#ifdef SUPPORT_PSENSOR
		err = ltr559_ps_enable(obj->client, 1);
		if (err < 0)
			APS_ERR("enable ps fail: %d\n", err);
#endif
	}

	return 0;
}
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void ltr559_early_suspend(struct early_suspend *h)
{				/*early_suspend is only applied for ALS */
	struct ltr559_priv *obj =
	    container_of(h, struct ltr559_priv, early_drv);
	int err;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return;
	}

	atomic_set(&obj->als_suspend, 1);
	err = ltr559_als_enable(obj->client, 0);
	if (err < 0)
		APS_ERR("disable als fail: %d\n", err);
}

static void ltr559_late_resume(struct early_suspend *h)
{				/*early_suspend is only applied for ALS */
	struct ltr559_priv *obj =
	    container_of(h, struct ltr559_priv, early_drv);
	int err;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return;
	}

	atomic_set(&obj->als_suspend, 0);
	if (test_bit(CMC_BIT_ALS, &obj->enable)) {
		err = ltr559_als_enable(obj->client, 1);
		if (err < 0)
			APS_ERR("enable als fail: %d\n", err);
	}
}
#endif

/*----------------------------------------------------------------------------*/
static int ltr559_i2c_detect(struct i2c_client *client,
			     struct i2c_board_info *info)
{
	strcpy(info->type, LTR559_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ltr559_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct ltr559_priv *obj;
	struct als_control_path als_ctl = { 0 };
	struct als_data_path als_data = { 0 };
#ifdef SUPPORT_PSENSOR
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };
#endif
	int err = 0;

	APS_LOG("%s\n", __func__);

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
	ltr559_obj = obj;

	obj->hw = hw;

	INIT_WORK(&obj->eint_work, ltr559_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);
	atomic_set(&obj->als_debounce, 300);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 300);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->ps_thd_val_high, obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low, obj->hw->ps_threshold_low);
	atomic_set(&obj->ps_thd_val, obj->hw->ps_threshold);

	ltr559_obj = obj;
	obj->irq_node = client->dev.of_node;

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num =
	    ARRAY_SIZE(obj->hw->als_level);
	obj->als_value_num =
	    ARRAY_SIZE(obj->hw->als_value);
	obj->als_modulus = (400 * 100) / (16 * 150);
	//(400)/16*2.72 here is amplify *100
	WARN_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	WARN_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);

	APS_LOG("ltr559_devinit() start...!\n");
	ltr559_i2c_client = client;
	ltr559_i2c_client->addr = 0x23;
	err = ltr559_devinit();
	if (err)
		goto exit_init_failed;

	APS_LOG("ltr559_devinit() ...OK!\n");

	err = misc_register(&ltr559_device);
	if (err) {
		APS_ERR("ltr559_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	/* Register sysfs attribute */
	err =
	    ltr559_create_attr(&(ltr559_init_info.platform_diver_addr->driver));
	if (err) {
		APS_ERR("create attribute err = %d\n", err);
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
#ifdef SUPPORT_PSENSOR
	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.batch = ps_batch;
	ps_ctl.flush = ps_flush;
	ps_ctl.is_report_input_direct = false;
	ps_ctl.is_support_batch = false;
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

	err = ltr559_setup_eint(client);
	if (err != 0) {
		APS_ERR("setup eint: %d\n", err);
		return err;
	}
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	    obj->early_drv.suspend = ltr559_early_suspend,
	    obj->early_drv.resume = ltr559_late_resume,
	    register_early_suspend(&obj->early_drv);
#endif

	ltr559_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);


	ltr559_obj->lux_threshold = DEF_LUX_THRESHOLD;
	ltr559_obj->transmittance = DEF_TRANSMITTANCE;

	return 0;

exit_create_attr_failed:
exit_sensor_obj_attach_fail:
exit_misc_device_register_failed:
	misc_deregister(&ltr559_device);
exit_init_failed:
	kfree(obj);
exit:
	ltr559_i2c_client = NULL;
	APS_ERR("%s: err = %d\n", __func__, err);
	ltr559_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/

static int ltr559_i2c_remove(struct i2c_client *client)
{
	int err;

	err = ltr559_delete_attr(&ltr559_i2c_driver.driver);
	if (err)
		APS_ERR("ltr559_delete_attr fail: %d\n", err);

	misc_deregister(&ltr559_device);

	ltr559_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ltr559_remove(void)
{
	//struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();

	i2c_del_driver(&ltr559_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ltr559_local_init(void)
{
	if (i2c_add_driver(&ltr559_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -1;
	}
	if (-1 == ltr559_init_flag)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init ltr559_init(void)
{
	APS_FUN();

	alsps_driver_add(&ltr559_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit ltr559_exit(void)
{
	APS_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(ltr559_init);
module_exit(ltr559_exit);
/*----------------------------------------------------------------------------*/

MODULE_AUTHOR("MingHsien Hsieh");
MODULE_DESCRIPTION("LTR-559ALS Driver");
MODULE_LICENSE("GPL v2");

/*
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
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_wakeup.h>
#include <hwmsensor.h>
#include "cust_alsps.h"
#include "alsps.h"
#include "stk3x1x.h"

#define DRIVER_VERSION			"3.9.2 20180403"
#define MTK_AUTO_DETECT_ALSPS
/*
 * configuration
 */

#define stk3x1x_DEV_NAME "stk3x1x"
#define APS_TAG			"[ALS/PS] "
#define APS_FUN(f)		pr_debug(APS_TAG"%s\n", __func__)
#define APS_ERR(fmt, args...)	pr_info(APS_TAG"%s %d : "fmt,\
				__func__, __LINE__, ##args)
#define APS_DBG(fmt, args...)	pr_debug(APS_TAG fmt, ##args)
#define APS_LOG(fmt, args...)	pr_info(APS_TAG fmt, ##args)
/*
 * extern functions
 */
#define STK_H_PS			400
#define STK_H_HT			300
#define STK_H_LT			200

#define STK_IRC_MAX_ALS_CODE		20000
#define STK_IRC_MIN_ALS_CODE		25
#define STK_IRC_MIN_IR_CODE		50
#define STK_IRC_ALS_DENOMI		2
#define STK_IRC_ALS_NUMERA		5
#define STK_IRC_ALS_CORREC		748

#define STK_IRS_IT_REDUCE		2
#define STK_ALS_READ_IRS_IT_REDUCE	5
#define STK_ALS_THRESHOLD		30

#define STK3310SA_PID			0x17
#define STK3311SA_PID			0x1E
#define STK3311WV_PID			0x1D
#define STK3311X_PID			0x12
#define STK33119_PID			0x11

static unsigned int als_value;		/* lux value in test level*/
static unsigned int als_value_cali;	/* lux value for ALS calibration */
static unsigned int transmittance_cali;	/* transmittance for ALS calibration */
static unsigned int als_cal;
/*
 * Since baro sensor driver in AP,
 * it should register information to sensorlist.
 */
static struct sensorInfo_NonHub_t alsps_devinfo;

#define ALS_DEFAULT_TRANSMITTANCE 1553
#define ALS_CALIBRATION_LUX 400

/*----------------------------------------------------------------------------*/
static struct i2c_client *stk3x1x_i2c_client;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id stk3x1x_i2c_id[] = {
	{stk3x1x_DEV_NAME, 0},
	{}
};
/*----------------------------------------------------------------------------*/
static int stk3x1x_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id);
static int stk3x1x_i2c_remove(struct i2c_client *client);

static int stk3x1x_i2c_detect(struct i2c_client *client,
				struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_PM_SLEEP
static int stk3x1x_suspend(struct device *dev);
static int stk3x1x_resume(struct device *dev);
#endif

#ifndef C_I2C_FIFO_SIZE
#define C_I2C_FIFO_SIZE		8
#endif
static DEFINE_MUTEX(STK3X1X_i2c_mutex);
static int stk3x1x_init_flag = -1;	/* 0 <==> OK -1 <==> fail */
static int stk3x1x_local_init(void);
static int stk3x1x_local_uninit(void);
static struct alsps_init_info stk3x1x_init_info = {
	.name = "stk3x1x",
	.init = stk3x1x_local_init,
	.uninit = stk3x1x_local_uninit,
};

struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;
struct platform_device *alspsPltFmDev;
/* For alsp driver get cust info */
struct alsps_hw *get_cust_alsps(void)
{
	return &alsps_cust;
}
/*----------------------------------------------------------------------------*/
enum {
	STK_TRC_ALS_DATA = 0x0001,
	STK_TRC_PS_DATA = 0x0002,
	STK_TRC_EINT = 0x0004,
	STK_TRC_IOCTL = 0x0008,
	STK_TRC_I2C = 0x0010,
	STK_TRC_CVT_ALS = 0x0020,
	STK_TRC_CVT_PS = 0x0040,
	STK_TRC_DEBUG = 0x8000,
} STK_TRC;
/*----------------------------------------------------------------------------*/
enum {
	STK_BIT_ALS = 1,
	STK_BIT_PS = 2,
} STK_BIT;
/*----------------------------------------------------------------------------*/
struct stk3x1x_i2c_addr {
	/*define a series of i2c slave address*/
	u8 state;		/* enable/disable state */
	u8 psctrl;		/* PS control */
	u8 alsctrl;		/* ALS control */
	u8 ledctrl;		/* LED control */
	u8 intmode;		/* INT mode */
	u8 wait;		/* wait time */
	u8 thdh1_ps;		/* PS INT threshold high 1 */
	u8 thdh2_ps;		/* PS INT threshold high 2 */
	u8 thdl1_ps;		/* PS INT threshold low 1 */
	u8 thdl2_ps;		/* PS INT threshold low 2 */
	u8 thdh1_als;		/* ALS INT threshold high 1 */
	u8 thdh2_als;		/* ALS INT threshold high 2 */
	u8 thdl1_als;		/* ALS INT threshold low 1 */
	u8 thdl2_als;		/* ALS INT threshold low 2 */
	u8 flag;		/* int flag */
	u8 data1_ps;		/* ps data1 */
	u8 data2_ps;		/* ps data2 */
	u8 data1_als;		/* als data1 */
	u8 data2_als;		/* als data2 */
	u8 data1_offset;	/* offset data1 */
	u8 data2_offset;	/* offset data2 */
	u8 data1_ir;		/* ir data1 */
	u8 data2_ir;		/* ir data2 */
	u8 soft_reset;		/* software reset */
};
/*----------------------------------------------------------------------------*/
struct stk3x1x_priv {
	struct alsps_hw	 *hw;
	struct i2c_client *client;
	struct delayed_work eint_work;

	/*i2c address group*/
	struct stk3x1x_i2c_addr addr;

	/*misc*/
	atomic_t trace;
	atomic_t i2c_retry;
	atomic_t als_suspend;
	atomic_t als_debounce;	/*debounce time after enabling als*/
	atomic_t als_deb_on;	/*indicates if the debounce is on*/
	atomic_t als_deb_end;	/*the jiffies representing the end of debounce*/
	atomic_t ps_mask;	/*mask ps: always return far away*/
	atomic_t ps_debounce;	/*debounce time after enabling ps*/
	atomic_t ps_deb_on;	/*indicates if the debounce is on*/
	atomic_t ps_deb_end;	/*the jiffies representing the end of debounce*/
	atomic_t ps_suspend;
	atomic_t init_done;
	struct device_node *irq_node;
	int irq;

	/*data*/
	u16 als;
	u16 ps;
	u8 _align;

	atomic_t state_val;
	atomic_t psctrl_val;
	atomic_t alsctrl_val;
	u8 wait_val;
	u8 ledctrl_val;
	u8 int_val;

	atomic_t ps_high_thd_val;
	/*the cmd value can't be read, stored in ram*/
	atomic_t ps_low_thd_val;
	/*the cmd value can't be read, stored in ram*/
	ulong enable;
	/*enable mask*/
	ulong pending_intr;
	/*pending interrupt*/
	atomic_t recv_reg;
	bool first_boot;
	uint16_t ir_code;
	uint16_t als_correct_factor;
	bool als_last;
	bool re_enable_ps;
	bool re_enable_als;
	u16 ps_cali;

	uint8_t pid;
	uint8_t	p_wv_r_bd_with_co;
	uint32_t als_code_last;
	uint16_t als_data_index;
	uint8_t ps_distance_last;
	uint8_t	p_1x_r_bd_with_co;
	uint8_t	p_19_r_bc;

	uint32_t als_transmittance;
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,alsps"},
	{},
};
#endif
#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops stk3x1x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(stk3x1x_suspend, stk3x1x_resume)
};
#endif

static struct i2c_driver stk3x1x_i2c_driver = {
	.probe = stk3x1x_i2c_probe,
	.remove = stk3x1x_i2c_remove,
	.detect = stk3x1x_i2c_detect,
	.id_table = stk3x1x_i2c_id,
	.driver = {
		.name = stk3x1x_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
		.pm = &stk3x1x_pm_ops,
#endif

#ifdef CONFIG_OF
		.of_match_table = alsps_of_match,
#endif
	},
};

static struct stk3x1x_priv *stk3x1x_obj;
static int stk3x1x_get_ps_value(struct stk3x1x_priv *obj, u16 ps);
static int stk3x1x_get_ps_value_only(struct stk3x1x_priv *obj, u16 ps);
static uint32_t stk_alscode2lux(struct stk3x1x_priv *obj, uint32_t alscode);
static int stk3x1x_read_als(struct i2c_client *client, u16 *data);
static int stk3x1x_read_ps(struct i2c_client *client, u16 *data);
static int stk3x1x_set_als_int_thd(struct i2c_client *client, u16 als_data_reg);
static int32_t stk3x1x_get_ir_value(struct stk3x1x_priv *obj,
		int32_t als_it_reduce);
#ifdef STK_CHK_REG
static int stk3x1x_validate_n_handle(struct i2c_client *client);
#endif
static int stk3x1x_init_client(struct i2c_client *client);
static struct wakeup_source mps_lock;

static DEFINE_MUTEX(run_cali_mutex);
/*----------------------------------------------------------------------------*/
int stk3x1x_get_addr(struct alsps_hw *hw, struct stk3x1x_i2c_addr *addr)
{
	if (!hw || !addr)
		return -EFAULT;

	addr->state = STK_STATE_REG;
	addr->psctrl = STK_PSCTRL_REG;
	addr->alsctrl = STK_ALSCTRL_REG;
	addr->ledctrl = STK_LEDCTRL_REG;
	addr->intmode = STK_INT_REG;
	addr->wait = STK_WAIT_REG;
	addr->thdh1_ps = STK_THDH1_PS_REG;
	addr->thdh2_ps = STK_THDH2_PS_REG;
	addr->thdl1_ps = STK_THDL1_PS_REG;
	addr->thdl2_ps = STK_THDL2_PS_REG;
	addr->thdh1_als = STK_THDH1_ALS_REG;
	addr->thdh2_als = STK_THDH2_ALS_REG;
	addr->thdl1_als = STK_THDL1_ALS_REG;
	addr->thdl2_als = STK_THDL2_ALS_REG;
	addr->flag = STK_FLAG_REG;
	addr->data1_ps = STK_DATA1_PS_REG;
	addr->data2_ps = STK_DATA2_PS_REG;
	addr->data1_als = STK_DATA1_ALS_REG;
	addr->data2_als = STK_DATA2_ALS_REG;
	addr->data1_offset = STK_DATA1_OFFSET_REG;
	addr->data2_offset = STK_DATA2_OFFSET_REG;
	addr->data1_ir = STK_DATA1_IR_REG;
	addr->data2_ir = STK_DATA2_IR_REG;
	addr->soft_reset = STK_SW_RESET_REG;
	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_hwmsen_read_block(struct i2c_client *client,
				u8 addr, u8 *data, u8 len)
{
	int err;
	u8 beg = addr;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &beg
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		}
	};

	mutex_lock(&STK3X1X_i2c_mutex);

	if (!client) {
		mutex_unlock(&STK3X1X_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		mutex_unlock(&STK3X1X_i2c_mutex);
		APS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	mutex_unlock(&STK3X1X_i2c_mutex);

	if (err != 2) {
		APS_ERR("i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;/*no error*/
	}

	return err;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_get_timing(void)
{
	return 200;
}

/*----------------------------------------------------------------------------*/
int stk3x1x_master_recv(struct i2c_client *client, u16 addr, u8 *buf, int count)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0, retry = 0;
	int trc = atomic_read(&obj->trace);
	int max_try = atomic_read(&obj->i2c_retry);

	while (retry++ < max_try) {
		ret = stk3x1x_hwmsen_read_block(client, addr, buf, count);

		if (ret == 0)
			break;

		udelay(100);
	}

	if (unlikely(trc)) {
		if ((retry != 1) && (trc & STK_TRC_DEBUG))
			APS_DBG("(recv) %d/%d\n", retry - 1, max_try);
	}

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 0) ? count : ret;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_master_send(struct i2c_client *client, u16 addr, u8 *buf, int count)
{
	int ret = 0, retry = 0;
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int trc = atomic_read(&obj->trace);
	int max_try = atomic_read(&obj->i2c_retry);

	while (retry++ < max_try) {
		ret = hwmsen_write_block(client, addr, buf, count);

		if (ret == 0)
			break;

		udelay(100);
	}

	if (unlikely(trc)) {
		if ((retry != 1) && (trc & STK_TRC_DEBUG))
			APS_DBG("(send) %d/%d\n", retry - 1, max_try);
	}

	/* If everything went ok (i.e. 1 msg transmitted) */
	/* return bytes transmitted, else error code. */
	return (ret == 0) ? count : ret;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_otp_read_byte_data(struct i2c_client *client, u8 command)
{
	int ret;
	int value;
	u8 data;

	data = 0x2;
	ret = stk3x1x_master_send(client, 0x0, &data, 1);
	if (ret < 0) {
		APS_ERR("write 0x0 = %d\n", ret);
		return -EFAULT;
	}

	data = command;
	ret = stk3x1x_master_send(client, 0x90, &data, 1);
	if (ret < 0) {
		APS_ERR("write 0x90 = %d\n", ret);
		return -EFAULT;
	}

	data = 0x82;
	ret = stk3x1x_master_send(client, 0x92, &data, 1);
	if (ret < 0) {
		APS_ERR("write 0x0 = %d\n", ret);
		return -EFAULT;
	}

	usleep_range(2000, 4000);
	ret = stk3x1x_master_recv(client, 0x91, &data, 1);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	value = data;
	APS_DBG("%s: 0x%x=0x%x\n", __func__, command, value);
	data = 0x0;
	ret = stk3x1x_master_send(client, 0x0, &data, 1);
	if (ret < 0) {
		APS_ERR("write 0x0 = %d\n", ret);
		return -EFAULT;
	}

	return value;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_led(struct i2c_client *client, u8 data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	ret = stk3x1x_master_send(client, obj->addr.ledctrl, &data, 1);
	if (ret < 0) {
		APS_ERR("write led = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk_als_ir_get_corr(struct stk3x1x_priv *obj, u32 als_data)
{
	int32_t als_comperator;

	if (obj->ir_code) {
		obj->als_correct_factor = 1000;
		if (als_data < STK_IRC_MAX_ALS_CODE
				&& als_data > STK_IRC_MIN_ALS_CODE &&
		    obj->ir_code > STK_IRC_MIN_IR_CODE) {
			als_comperator = als_data *
			STK_IRC_ALS_NUMERA / STK_IRC_ALS_DENOMI;

			if (obj->ir_code > als_comperator)
				obj->als_correct_factor = STK_IRC_ALS_CORREC;
		}

		APS_DBG("%s: als=%d, ir=%d, als_correct_factor=%d",
				__func__, als_data,
				obj->ir_code, obj->als_correct_factor);
		obj->ir_code = 0;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
#ifdef STK_IRS
int stk_als_ir_skip_als(struct stk3x1x_priv *obj)
{
	int ret;
	u8 buf[2];

	if (obj->als_data_index < 60000)
		obj->als_data_index++;
	else
		obj->als_data_index = 0;

	if (obj->als_data_index % 10 == 1) {
		ret = stk3x1x_master_recv(obj->client,
				obj->addr.data1_als, buf, 0x02);
		if (ret < 0) {
			APS_ERR("error: %d\n", ret);
			return -EFAULT;
		}

		return 1;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static int stk_als_ir_run(struct stk3x1x_priv *obj)
{
	u32 ir_data;

	if (obj->als_data_index % 10 == 0) {
		if (obj->ps_distance_last != 0 && obj->ir_code == 0) {
			ir_data = stk3x1x_get_ir_value(obj, STK_IRS_IT_REDUCE);

			if (ir_data > 0)
				obj->ir_code = ir_data;
		}

		return ir_data;
	}

	return 0;
}
#endif	/* #ifdef STK_IRS */
/*----------------------------------------------------------------------------*/

#define STK_ALS_THRESHOLD	30
int stk3x1x_read_als(struct i2c_client *client, u16 *data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;
	u8 buf[2];
	u32 als_data;
	int32_t ir_data;
#ifdef STK_IRS
	const int ir_enlarge =
		1 << (STK_ALS_READ_IRS_IT_REDUCE - STK_IRS_IT_REDUCE);
#endif
	if (!client)
		return -EINVAL;

#ifdef STK_IRS
	ret = stk_als_ir_skip_als(obj);
	if (ret == 1)
		return 0;

#endif
	if (atomic_read(&obj->als_suspend)) {
		als_data = 0;
		goto out;
	}

	ret = stk3x1x_master_recv(client, obj->addr.data1_als, buf, 0x02);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	als_data = (buf[0] << 8) | (buf[1]);

	if (obj->p_1x_r_bd_with_co == 0x07 || obj->p_19_r_bc == 0x03) {
		als_data = als_data * 16 / 10;

		if (als_data > 65535)
			als_data = 65535;
	}

	if (obj->p_wv_r_bd_with_co & 0x02) {
		if (als_data < STK_ALS_THRESHOLD &&
				obj->als_code_last > 10000) {
			ir_data = stk3x1x_get_ir_value(obj,
					STK_ALS_READ_IRS_IT_REDUCE);
#ifdef STK_IRS

			if (ir_data > 0)
				obj->ir_code = ir_data * ir_enlarge;

#endif
			if (ir_data > (STK_ALS_THRESHOLD * 3))
				als_data = obj->als_code_last;
		}

#ifdef STK_IRS
		else
			obj->ir_code = 0;
#endif
	}

	obj->als_code_last = als_data;
#ifdef STK_IRS
	stk_als_ir_get_corr(obj, als_data);
	als_data = als_data * obj->als_correct_factor / 1000;
#endif
out:
	*data = (u16)als_data;
#ifdef STK_IRS
	stk_als_ir_run(obj);
#endif

	if (atomic_read(&obj->trace) & STK_TRC_ALS_DATA)
		APS_DBG("ALS: 0x%04X\n", (u32)(*data));

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_als(struct i2c_client *client, u8 data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	ret = stk3x1x_master_send(client, obj->addr.alsctrl, &data, 1);
	if (ret < 0) {
		APS_ERR("write als = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_read_state(struct i2c_client *client, u8 *data)
{
	int ret = 0;
	u8 buf;

	if (!client)
		return -EINVAL;

	ret = stk3x1x_master_recv(client, STK_STATE_REG, &buf, 0x01);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	*data = buf;

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_read_flag(struct i2c_client *client, u8 *data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;
	u8 buf;

	if (!client)
		return -EINVAL;

	ret = stk3x1x_master_recv(client, obj->addr.flag, &buf, 0x01);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}
	*data = buf;

	if (atomic_read(&obj->trace) & STK_TRC_ALS_DATA)
		APS_DBG("PS NF flag: 0x%04X\n", (u32)(*data));

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_read_id(struct i2c_client *client)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;
	u8 buf[2];
	u8 pid_msb;
	int otp25 = 0;

	if (!client)
		return -EINVAL;

	obj->p_wv_r_bd_with_co = 0;
	obj->p_1x_r_bd_with_co = 0;
	obj->p_19_r_bc = 0;
	ret = stk3x1x_master_recv(client, STK_PDT_ID_REG, buf, 0x02);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	obj->pid = buf[0];
	APS_DBG("%s: PID=0x%x, VID=0x%x\n", __func__, buf[0], buf[1]);

	if (obj->pid == STK3310SA_PID || obj->pid == STK3311SA_PID)
		obj->ledctrl_val &= 0x3F;

	if (buf[0] == STK3311WV_PID)
		obj->p_wv_r_bd_with_co |= 0x04;
	else if (buf[0] == STK3311X_PID)
		obj->p_1x_r_bd_with_co |= 0x04;
	else if (buf[0] == STK33119_PID)
		obj->p_19_r_bc |= 0x02;

	if (buf[1] == 0xC3) {
		obj->p_wv_r_bd_with_co |= 0x02;
		obj->p_1x_r_bd_with_co |= 0x02;
	} else if (buf[1] == 0xC2) {
		obj->p_19_r_bc |= 0x01;
	}

	ret = stk3x1x_otp_read_byte_data(client, 0x25);

	if (ret < 0)
		return ret;

	otp25 = ret;

	if (otp25 & 0x80)
		obj->p_wv_r_bd_with_co |= 0x01;

	APS_DBG("%s: p_wv_r_bd_with_co = 0x%x\n",
		__func__, obj->p_wv_r_bd_with_co);

	if (otp25 & 0x40)
		obj->p_1x_r_bd_with_co |= 0x01;

	APS_DBG("%s: p_1x_r_bd_with_co = 0x%x\n",
		__func__, obj->p_1x_r_bd_with_co);
	APS_DBG("%s: p_19_r_bc = 0x%x\n",
		__func__, obj->p_19_r_bc);

	if (buf[0] == 0) {
		APS_ERR("PID=0x0, please make sure the chip is stk3x1x!\n");
		return -2;
	}

	pid_msb = buf[0] & 0xF0;

	switch (pid_msb) {
	case 0x10:
	case 0x20:
	case 0x30:
		return 0;

	default:
		APS_ERR("invalid PID(%#x)\n", buf[0]);
		return -1;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_read_ps(struct i2c_client *client, u16 *data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;
	u8 buf[2];

	if (!client) {
		APS_ERR("i2c client is NULL\n");
		return -EINVAL;
	}

	ret = stk3x1x_master_recv(client, obj->addr.data1_ps, buf, 0x02);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}
	*data = (buf[0] << 8) | (buf[1]);

	if (atomic_read(&obj->trace) & STK_TRC_ALS_DATA)
		APS_DBG("PS: 0x%04X\n", (u32)(*data));

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_ps(struct i2c_client *client, u8 data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	ret = stk3x1x_master_send(client, obj->addr.psctrl, &data, 1);
	if (ret < 0) {
		APS_ERR("write ps = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
int stk3x1x_write_wait(struct i2c_client *client, u8 data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	ret = stk3x1x_master_send(client, obj->addr.wait, &data, 1);
	if (ret < 0) {
		APS_ERR("write wait = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
int stk3x1x_write_int(struct i2c_client *client, u8 data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	ret = stk3x1x_master_send(client, obj->addr.intmode, &data, 1);
	if (ret < 0) {
		APS_ERR("write intmode = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_state(struct i2c_client *client, u8 data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	ret = stk3x1x_master_send(client, obj->addr.state, &data, 1);
	if (ret < 0) {
		APS_ERR("write state = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_flag(struct i2c_client *client, u8 data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	ret = stk3x1x_master_send(client, obj->addr.flag, &data, 1);
	if (ret < 0) {
		APS_ERR("write ps = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_sw_reset(struct i2c_client *client)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf = 0, r_buf = 0;
	int ret = 0;

	APS_LOG("%s: In\n", __func__);
	buf = 0x7F;
	ret = stk3x1x_master_send(client, obj->addr.wait,
				(char *)&buf, sizeof(buf));

	if (ret < 0) {
		APS_ERR("i2c write test error = %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(client, obj->addr.wait, &r_buf, 1);

	if (ret < 0) {
		APS_ERR("i2c read test error = %d\n", ret);
		return -EFAULT;
	}

	if (buf != r_buf) {
		APS_ERR("read-back is not the same, write=0x%x, read=0x%x\n",
			buf, r_buf);
		return -EIO;
	}

	buf = 0;
	ret = stk3x1x_master_send(client, obj->addr.soft_reset,
				(char *)&buf, sizeof(buf));
	if (ret < 0) {
		APS_ERR("write software reset error = %d\n", ret);
		return -EFAULT;
	}

	msleep(20);
	return 0;
}

/*----------------------------------------------------------------------------*/
int stk3x1x_write_ps_high_thd(struct i2c_client *client, u16 thd)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8)((0xFF00 & thd) >> 8);
	buf[1] = (u8)(0x00FF & thd);
	ret = stk3x1x_master_send(client, obj->addr.thdh1_ps, &buf[0], 1);
	if (ret < 0) {
		APS_ERR("WARNING: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_send(client, obj->addr.thdh2_ps, &(buf[1]), 1);
	if (ret < 0) {
		APS_ERR("WARNING: %d\n", ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_ps_low_thd(struct i2c_client *client, u16 thd)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8)((0xFF00 & thd) >> 8);
	buf[1] = (u8)(0x00FF & thd);
	ret = stk3x1x_master_send(client, obj->addr.thdl1_ps, &buf[0], 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_send(client, obj->addr.thdl2_ps, &(buf[1]), 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_als_high_thd(struct i2c_client *client, u16 thd)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8)((0xFF00 & thd) >> 8);
	buf[1] = (u8)(0x00FF & thd);
	ret = stk3x1x_master_send(client, obj->addr.thdh1_als, &buf[0], 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_send(client, obj->addr.thdh2_als, &(buf[1]), 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int stk3x1x_write_als_low_thd(struct i2c_client *client, u16 thd)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8)((0xFF00 & thd) >> 8);
	buf[1] = (u8)(0x00FF & thd);
	ret = stk3x1x_master_send(client, obj->addr.thdl1_als, &buf[0], 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_send(client, obj->addr.thdl2_als, &(buf[1]), 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef STK_PS_DEBUG
static int show_allreg(void)
{
	int ret = 0;
	u8 rbuf[0x22];
	int cnt;

	memset(rbuf, 0, sizeof(rbuf));
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 0, &rbuf[0], 7);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 7, &rbuf[7], 7);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 14, &rbuf[14], 7);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 21, &rbuf[21], 7);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 28, &rbuf[28], 4);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client,
				STK_PDT_ID_REG, &rbuf[32], 2);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	for (cnt = 0; cnt < 0x20; cnt++)
		APS_DBG("reg[0x%x]=0x%x\n", cnt, rbuf[cnt]);

	APS_DBG("reg[0x3E]=0x%x\n", rbuf[cnt]);
	APS_DBG("reg[0x3F]=0x%x\n", rbuf[cnt++]);
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static void stk_ps_report(struct stk3x1x_priv *obj, int nf)
{
	if (ps_report_interrupt_data(nf))
		APS_ERR("call ps_report_interrupt_data fail\n");

	APS_DBG("%s:ps raw 0x%x -> value 0x%x\n", __func__, obj->ps, nf);
	obj->ps_distance_last = nf;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_enable_als(struct i2c_client *client, int enable)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int err, cur = 0, old = atomic_read(&obj->state_val);
	int trc = atomic_read(&obj->trace);

	APS_LOG("%s: enable=%d\n", __func__, enable);

	cur = old & (~(STK_STATE_EN_ALS_MASK | STK_STATE_EN_WAIT_MASK));

	if (enable)
		cur |= STK_STATE_EN_ALS_MASK;
	else if (old & STK_STATE_EN_PS_MASK)
		cur |= STK_STATE_EN_WAIT_MASK;

	if (trc & STK_TRC_DEBUG)
		APS_DBG("%s: %08X, %08X, %d\n", __func__, cur, old, enable);

	if (0 == (cur ^ old))
		return 0;

#ifdef STK_IRS

	if (enable && !(old & STK_STATE_EN_PS_MASK)) {
		err = stk3x1x_get_ir_value(obj, STK_IRS_IT_REDUCE);
		if (err > 0)
			obj->ir_code = err;
	}

#endif

	if (enable && obj->hw->polling_mode_als == 0) {
		stk3x1x_write_als_high_thd(client, 0x0);
		stk3x1x_write_als_low_thd(client, 0xFFFF);
	}

	err = stk3x1x_write_state(client, cur);
	if (err < 0)
		return err;

	atomic_set(&obj->state_val, cur);
	if (enable) {
		obj->als_last = 0;

		if (obj->hw->polling_mode_als) {
			atomic_set(&obj->als_deb_on, 1);
			atomic_set(&obj->als_deb_end, jiffies +
			atomic_read(&obj->als_debounce)*HZ / 1000);
		} else {
			schedule_delayed_work(&obj->eint_work, 220 * HZ / 1000);
		}
	}

#ifdef STK_IRS
	obj->als_data_index = 0;
#endif

	if (trc & STK_TRC_DEBUG)
		APS_DBG("enable als (%d)\n", enable);

	return err;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_enable_ps_set_thd(struct stk3x1x_priv *obj)
{
	int err;

	err = stk3x1x_write_ps_high_thd(obj->client,
				atomic_read(&obj->ps_high_thd_val));
	if (err) {
		APS_ERR("write high thd error: %d\n", err);
		return err;
	}

	err = stk3x1x_write_ps_low_thd(obj->client,
				atomic_read(&obj->ps_low_thd_val));
	if (err) {
		APS_ERR("write low thd error: %d\n", err);
		return err;
	}

	APS_DBG("%s:in reg, HT=%d, LT=%d\n", __func__,
			atomic_read(&obj->ps_high_thd_val),
			atomic_read(&obj->ps_low_thd_val));
	APS_DBG("%s:in driver, HT=%d, LT=%d\n", __func__,
			atomic_read(&obj->ps_high_thd_val),
			atomic_read(&obj->ps_low_thd_val));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_enable_ps(struct i2c_client *client,
					int enable, int validate_reg)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int err, cur = 0, old = atomic_read(&obj->state_val);
	int trc = atomic_read(&obj->trace);

#ifdef STK_PS_DEBUG
	if (enable) {
		APS_LOG("%s: before enable ps, HT=%d, LT=%d\n", __func__,
				atomic_read(&obj->ps_high_thd_val),
				atomic_read(&obj->ps_low_thd_val));
		APS_LOG("%s: before enable ps, show all reg\n", __func__);
		show_allreg(); /*for debug*/
	}

#endif
	pr_info("%s: enable=%d\n", __func__, enable);
	cur = old;
	cur &= (~(0x45));

	if (enable) {
		cur |= (STK_STATE_EN_PS_MASK);

		if (!(old & STK_STATE_EN_ALS_MASK))
			cur |= STK_STATE_EN_WAIT_MASK;
	}

	if (0 == (cur ^ old)) {
		pr_info("%s: repeat enable=%d, reg=0x%x\n",
			__func__, enable, cur);
		return 0;
	}

#ifdef STK_CHK_REG

	if (validate_reg) {
		err = stk3x1x_validate_n_handle(obj->client);
		if (err < 0)
			APS_ERR("stk3x1x_validate_n_handle fail: %d\n", err);
	}

#endif
	if (obj->first_boot == true)
		obj->first_boot = false;

	if (enable) {
		stk3x1x_enable_ps_set_thd(obj);

		if (obj->hw->polling_mode_ps == 1)
			__pm_stay_awake(&mps_lock);
	} else {
		if (obj->hw->polling_mode_ps == 1)
			__pm_relax(&mps_lock);
	}

	if (trc & STK_TRC_DEBUG)
		APS_DBG("%s: %08X, %08X, %d\n", __func__, cur, old, enable);

	err = stk3x1x_write_state(client, cur);

	if (err < 0)
		return err;

	atomic_set(&obj->state_val, cur);

	if (enable) {
		if (obj->hw->polling_mode_ps) {
			atomic_set(&obj->ps_deb_on, 1);
			atomic_set(&obj->ps_deb_end,
			jiffies + atomic_read(&obj->ps_debounce)*HZ / 1000);
		} else {
#ifdef STK_CHK_REG

			if (!validate_reg) {
				stk_ps_report(obj, 1);
			} else
#endif
			{
				msleep(20);
				err = stk3x1x_read_ps(obj->client, &obj->ps);
				if (err) {
					APS_ERR("read ps data:%d\n", err);
					return err;
				}

				err = stk3x1x_get_ps_value_only(obj, obj->ps);

				if (err < 0) {
					APS_ERR("get ps value: %d\n", err);
					return err;
				} else if (stk3x1x_obj->hw->polling_mode_ps
						== 0)
					stk_ps_report(obj, err);

#ifdef STK_PS_DEBUG
				APS_LOG("%s: after stk_ps_report, show reg\n",
					__func__);
				show_allreg();
				/*for debug*/
#endif
			}
		}
	}

	if (trc & STK_TRC_DEBUG)
		APS_DBG("enable ps (%d)\n", enable);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_ps_calibration(struct i2c_client *client)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 reg;
	int err = 0;
	int org_state_reg = atomic_read(&obj->state_val);
	bool re_en_ps = false, re_en_als = false;
	int counter = 0;
	uint32_t ps_sum = 0;
	const int PS_CALI_COUNT = 5;

	if (org_state_reg & STK_STATE_EN_PS_MASK) {
		APS_LOG("%s: force disable PS\n", __func__);
		stk3x1x_enable_ps(obj->client, 0, 1);
		re_en_ps = true;
	}

	if (org_state_reg & STK_STATE_EN_ALS_MASK) {
		APS_LOG("%s: force disable ALS\n", __func__);
		stk3x1x_enable_als(obj->client, 0);
		re_en_als = true;
	}

#if defined(CONFIG_OF)
	disable_irq(obj->irq);
#else
	mt_eint_mask(CUST_EINT_ALS_NUM);
#endif
	reg = STK_STATE_EN_WAIT_MASK | STK_STATE_EN_PS_MASK;
	err = stk3x1x_write_state(client, reg);

	if (err < 0)
		goto err_out;

	while (counter < PS_CALI_COUNT) {
		msleep(60);

		err = stk3x1x_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;

		APS_LOG("%s: ps=%d\n", __func__, obj->ps);
		ps_sum += obj->ps;
		counter++;
	}

	obj->ps_cali = (ps_sum / PS_CALI_COUNT);

err_out:
#if defined(CONFIG_OF)
	disable_irq(obj->irq);
#else
	mt_eint_mask(CUST_EINT_ALS_NUM);
#endif

	if (re_en_als) {
		APS_LOG("%s: re-enable ALS\n", __func__);
		stk3x1x_enable_als(obj->client, 1);
	}

	if (re_en_ps) {
		APS_LOG("%s: re-enable PS\n", __func__);
		stk3x1x_enable_ps(obj->client, 1, 1);
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_check_intr(struct i2c_client *client, u8 *status)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int err;

	err = stk3x1x_read_flag(client, status);
	if (err < 0) {
		APS_ERR("WARNING: read flag reg error: %d\n", err);
		return -EFAULT;
	}

	APS_DBG("%s: read status reg: 0x%x\n", __func__, *status);

	if (*status & STK_FLG_ALSINT_MASK)
		set_bit(STK_BIT_ALS, &obj->pending_intr);
	else
		clear_bit(STK_BIT_ALS, &obj->pending_intr);

	if (*status & STK_FLG_PSINT_MASK)
		set_bit(STK_BIT_PS, &obj->pending_intr);
	else
		clear_bit(STK_BIT_PS, &obj->pending_intr);

	if (atomic_read(&obj->trace) & STK_TRC_DEBUG)
		APS_DBG("check intr: 0x%02X => 0x%08lX\n",
			*status, obj->pending_intr);

	return 0;
}


static int stk3x1x_clear_intr(struct i2c_client *client,
				u8 status, u8 disable_flag)
{
	int err = 0;

	status = status | (STK_FLG_ALSINT_MASK | STK_FLG_PSINT_MASK
			| STK_FLG_OUI_MASK | STK_FLG_IR_RDY_MASK);
	status &= (~disable_flag);

	err = stk3x1x_write_flag(client, status);
	if (err)
		APS_ERR("stk3x1x_write_flag failed, err=%d\n", err);

	return err;
}

/*----------------------------------------------------------------------------*/
#ifdef STK_CHK_REG
static int stk3x1x_chk_reg_valid(struct stk3x1x_priv *obj)
{
	int ret = 0;
	u8 buf[9];

	if (!obj)
		return -EINVAL;

	memset(buf, 0, sizeof(buf));
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 1, &buf[0], 7);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 8, &buf[7], 2);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	if (buf[0] != atomic_read(&obj->psctrl_val)) {
		APS_ERR("%s: invalid reg 0x01=0x%2x\n", __func__, buf[0]);
		return 0xFF;
	}

#ifdef STK_IRS

	if (buf[1] != atomic_read(&obj->alsctrl_val)
		&& buf[1] != (atomic_read(&obj->alsctrl_val) -
		STK_IRS_IT_REDUCE)
		&& buf[1] != (atomic_read(&obj->alsctrl_val) -
		STK_ALS_READ_IRS_IT_REDUCE))
#else
	if (buf[1] != atomic_read(&obj->alsctrl_val)
		&& buf[1] != (atomic_read(&obj->alsctrl_val) -
		STK_ALS_READ_IRS_IT_REDUCE))
#endif
	{
		APS_ERR("%s: invalid reg 0x02=0x%2x\n", __func__, buf[1]);
		return 0xFF;
	}

	if (buf[2] != obj->ledctrl_val) {
		APS_ERR("%s: invalid reg 0x03=0x%2x\n", __func__, buf[2]);
		return 0xFF;
	}

	if (buf[3] != obj->int_val) {
		APS_ERR("%s: invalid reg 0x04=0x%2x\n", __func__, buf[3]);
		return 0xFF;
	}

	if (buf[4] != obj->wait_val) {
		APS_ERR("%s: invalid reg 0x05=0x%2x\n", __func__, buf[4]);
		return 0xFF;
	}

	if (buf[5] != (atomic_read(&obj->ps_high_thd_val) & 0xFF00) >> 8) {
		APS_ERR("%s: invalid reg 0x06=0x%2x\n", __func__, buf[5]);
		return 0xFF;
	}

	if (buf[6] != (atomic_read(&obj->ps_high_thd_val) & 0x00FF)) {
		APS_ERR("%s: invalid reg 0x07=0x%2x\n", __func__, buf[6]);
		return 0xFF;
	}

	if (buf[7] != (atomic_read(&obj->ps_low_thd_val) & 0xFF00) >> 8) {
		APS_ERR("%s: invalid reg 0x08=0x%2x\n", __func__, buf[7]);
		return 0xFF;
	}

	if (buf[8] != (atomic_read(&obj->ps_low_thd_val) & 0x00FF)) {
		APS_ERR("%s: invalid reg 0x09=0x%2x\n", __func__, buf[8]);
		return 0xFF;
	}

	return 0;
}

static int stk3x1x_validate_n_handle(struct i2c_client *client)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int err;

	err = stk3x1x_chk_reg_valid(obj);
	if (err < 0) {
		APS_ERR("stk3x1x_chk_reg_valid fail: %d\n", err);
		return err;
	}

	if (err == 0xFF) {
		APS_ERR("%s: Re-init chip\n", __func__);
		stk3x1x_init_client(obj->client);

		err = stk3x1x_write_ps_high_thd(client,
				atomic_read(&obj->ps_high_thd_val);
		if (err) {
			APS_ERR("write high thd error: %d\n", err);
			return err;
		}

		err = stk3x1x_write_ps_low_thd(client,
				atomic_read(&obj->ps_low_thd_val);
		if (err) {
			APS_ERR("write low thd error: %d\n", err);
			return err;
		}

		return 0xFF;
	}

	return 0;
}
#endif /* #ifdef STK_CHK_REG	*/
/*----------------------------------------------------------------------------*/
static int stk3x1x_set_als_int_thd(struct i2c_client *client, u16 als_data_reg)
{
	s32 als_thd_h, als_thd_l;

	als_thd_h = als_data_reg + STK_ALS_CODE_CHANGE_THD;
	als_thd_l = als_data_reg - STK_ALS_CODE_CHANGE_THD;

	if (als_thd_h >= (1 << 16))
		als_thd_h = (1 << 16) - 1;

	if (als_thd_l < 0)
		als_thd_l = 0;

	APS_DBG("%s:als_thd_h:%d,als_thd_l:%d\n",
			__func__, als_thd_h, als_thd_l);
	stk3x1x_write_als_high_thd(client, als_thd_h);
	stk3x1x_write_als_low_thd(client, als_thd_l);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int stk_als_int_handle(struct stk3x1x_priv *obj, u16 als_reading)
{
	struct hwm_sensor_data sensor_data;

	memset(&sensor_data, 0, sizeof(sensor_data));
	stk3x1x_set_als_int_thd(obj->client, obj->als);
	sensor_data.values[0] = stk_alscode2lux(obj, obj->als);
	sensor_data.value_divide = 1;
	sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
	APS_DBG("%s:als raw 0x%x -> value 0x%x\n", __func__,
				obj->als, sensor_data.values[0]);
	/*let up layer to know*/
	obj->als_code_last = als_reading;

	return 0;
}


/*----------------------------------------------------------------------------*/
void stk3x1x_eint_func(void)
{
	struct stk3x1x_priv *obj = stk3x1x_obj;

	APS_DBG(" interrupt fuc\n");
	if (!obj)
		return;

	if (obj->hw->polling_mode_ps == 0 || obj->hw->polling_mode_als == 0)
		schedule_delayed_work(&obj->eint_work, 0);

	if (atomic_read(&obj->trace) & STK_TRC_EINT)
		APS_DBG("eint: als/ps intrs\n");
}
/*----------------------------------------------------------------------------*/
static void stk3x1x_eint_work(struct work_struct *work)
{
	struct stk3x1x_priv *obj = stk3x1x_obj;
	int err;
	u8 flag_reg, disable_flag = 0;

	APS_DBG(" eint work\n");

	err = stk3x1x_check_intr(obj->client, &flag_reg);
	if (err) {
		APS_ERR("stk3x1x_check_intr fail: %d\n", err);
		goto err_i2c_rw;
	}

	APS_DBG(" &obj->pending_intr =%lx\n", obj->pending_intr);

	if (((1 << STK_BIT_ALS) & obj->pending_intr)
				&& (obj->hw->polling_mode_als == 0)) {
		/*get raw data*/
		APS_DBG("stk als change\n");
		disable_flag |= STK_FLG_ALSINT_MASK;

		err = stk3x1x_read_als(obj->client, &obj->als);
		if (err) {
			APS_ERR("stk3x1x_read_als failed %d\n", err);
			goto err_i2c_rw;
		}

		err = stk_als_int_handle(obj, obj->als);

		if (err < 0)
			goto err_i2c_rw;
	}

	if (((1 << STK_BIT_PS) &  obj->pending_intr)
				&& (obj->hw->polling_mode_ps == 0)) {
		APS_DBG("stk ps change\n");
		disable_flag |= STK_FLG_PSINT_MASK;

		err = stk3x1x_read_ps(obj->client, &obj->ps);
		if (err) {
			APS_ERR("stk3x1x read ps data: %d\n", err);
			goto err_i2c_rw;
		}

#ifdef STK_PS_DEBUG
		APS_DBG("%s: ps interrupt, show all reg\n", __func__);
		show_allreg(); /*for debug*/
#endif
	}

	if (disable_flag) {
		err = stk3x1x_clear_intr(obj->client, flag_reg, disable_flag);
		if (err) {
			APS_ERR("fail: %d\n", err);
			goto err_i2c_rw;
		}
	}

	msleep(20);
#if defined(CONFIG_OF)
	enable_irq(obj->irq);
#endif
	return;
err_i2c_rw:
	msleep(30);
#if defined(CONFIG_OF)
	enable_irq(obj->irq);
#endif
}


#if defined(CONFIG_OF)
static irqreturn_t stk3x1x_eint_handler(int irq, void *desc)
{
	pr_info("%s\n", __func__);
	disable_irq_nosync(stk3x1x_obj->irq);
	stk3x1x_eint_func();
	return IRQ_HANDLED;
}
#endif


/*----------------------------------------------------------------------------*/
int stk3x1x_setup_eint(struct i2c_client *client)
{
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;
	u32 ints[2] = { 0, 0 };

	/* gpio setting */
	pinctrl = devm_pinctrl_get(&client->dev);

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
	if (stk3x1x_obj->irq_node) {
		of_property_read_u32_array(stk3x1x_obj->irq_node,
					"debounce", ints, ARRAY_SIZE(ints));
		gpio_request(ints[0], "p-sensor");
		gpio_set_debounce(ints[0], ints[1]);
		pinctrl_select_state(pinctrl, pins_cfg);
		APS_DBG("ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);
		stk3x1x_obj->irq =
			irq_of_parse_and_map(stk3x1x_obj->irq_node, 0);
		APS_DBG("stk3x1x_obj->irq = %d\n", stk3x1x_obj->irq);

		if (!stk3x1x_obj->irq) {
			APS_ERR("irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}

		ret = request_irq(stk3x1x_obj->irq, stk3x1x_eint_handler,
					IRQ_TYPE_LEVEL_LOW, "ALS-eint", NULL);
		if (ret) {
			APS_ERR("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}
	} else {
		APS_ERR("null irq node!!\n");
		return -EINVAL;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_init_client(struct i2c_client *client)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int err;
	int ps_ctrl;

	APS_LOG("%s: In\n", __func__);

	err = stk3x1x_write_sw_reset(client);
	if (err) {
		APS_ERR("software reset error, err=%d", err);
		return err;
	}

	err = stk3x1x_read_id(client);
	if (err) {
		APS_ERR("stk3x1x_read_id error, err=%d", err);
		return err;
	}

	if (obj->first_boot == true) {
		if (obj->hw->polling_mode_ps == 0
			|| obj->hw->polling_mode_als == 0) {
			err = stk3x1x_setup_eint(client);
			if (err) {
				APS_ERR("setup eint error: %d\n", err);
				return err;
			}
		}
	}

	err = stk3x1x_write_state(client, atomic_read(&obj->state_val));
	if (err) {
		APS_ERR("write stete error: %d\n", err);
		return err;
	}
	ps_ctrl = atomic_read(&obj->psctrl_val);

	if (obj->hw->polling_mode_ps == 1)
		ps_ctrl &= 0x3F;

	err = stk3x1x_write_ps(client, ps_ctrl);
	if (err) {
		APS_ERR("write ps error: %d\n", err);
		return err;
	}

	err = stk3x1x_write_als(client, atomic_read(&obj->alsctrl_val));
	if (err) {
		APS_ERR("write als error: %d\n", err);
		return err;
	}

	err = stk3x1x_write_led(client, obj->ledctrl_val);
	if (err) {
		APS_ERR("write led error: %d\n", err);
		return err;
	}

	err = stk3x1x_write_wait(client, obj->wait_val);
	if (err) {
		APS_ERR("write wait error: %d\n", err);
		return err;
	}

	err = stk3x1x_write_int(client, obj->int_val);
	if (err) {
		APS_ERR("write int mode error: %d\n", err);
		return err;
	}

	obj->re_enable_ps = false;
	obj->re_enable_als = false;
	obj->als_code_last = 100;

	obj->ps_distance_last = -1;
	obj->als_code_last = 100;

	return 0;
}

/*----------------------------------------------------------------------------*/
static inline uint32_t stk3x1x_get_als_reading_avg(struct i2c_client *client,
		int sSampleNo)
{
	int res;
	uint16_t ALSData = 0;
	uint16_t DataCount = 0;
	uint32_t sAveAlsData = 0;

	while (DataCount < sSampleNo) {
		msleep(100);

		res = stk3x1x_read_als(stk3x1x_obj->client, &stk3x1x_obj->als);
		if (res)
			APS_ERR("get_als_reading_avg: stk3x1x_read_als!!\n");

		ALSData = stk3x1x_obj->als;
		APS_DBG("%s: [STK]als code = %d\n", __func__, ALSData);
		sAveAlsData += ALSData;
		DataCount++;
	}
	sAveAlsData /= sSampleNo;
	return sAveAlsData;
}
/*
 * Sysfs attributes
 */
static ssize_t config_show(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	res = scnprintf(buf, PAGE_SIZE, "(%d %d %d %d %d %d)\n",
			atomic_read(&stk3x1x_obj->i2c_retry),
			atomic_read(&stk3x1x_obj->als_debounce),
			atomic_read(&stk3x1x_obj->ps_mask),
			atomic_read(&stk3x1x_obj->ps_high_thd_val),
			atomic_read(&stk3x1x_obj->ps_low_thd_val),
			atomic_read(&stk3x1x_obj->ps_debounce));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t config_store(struct device_driver *ddri,
					const char *buf, size_t count)
{
	int retry, als_deb, ps_deb, mask, hthres, lthres, err;
	struct i2c_client *client;

	client = stk3x1x_i2c_client;
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "%d %d %d %d %d %d",
		&retry, &als_deb, &mask, &hthres, &lthres, &ps_deb) == 6) {
		atomic_set(&stk3x1x_obj->i2c_retry, retry);
		atomic_set(&stk3x1x_obj->als_debounce, als_deb);
		atomic_set(&stk3x1x_obj->ps_mask, mask);
		atomic_set(&stk3x1x_obj->ps_high_thd_val, hthres);
		atomic_set(&stk3x1x_obj->ps_low_thd_val, lthres);
		atomic_set(&stk3x1x_obj->ps_debounce, ps_deb);

		err = stk3x1x_write_ps_high_thd(client,
				atomic_read(&stk3x1x_obj->ps_high_thd_val));
		if (err) {
			APS_ERR("write high thd error: %d\n", err);
			return err;
		}

		err = stk3x1x_write_ps_low_thd(client,
				atomic_read(&stk3x1x_obj->ps_low_thd_val));
		if (err) {
			APS_ERR("write low thd error: %d\n", err);
			return err;
		}
	} else {
		APS_ERR("invalid content: '%s'\n", buf);
	}

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t trace_show(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	res = scnprintf(buf, PAGE_SIZE, "0x%04X\n",
				atomic_read(&stk3x1x_obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t trace_store(struct device_driver *ddri,
					const char *buf, size_t count)
{
	int trace;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&stk3x1x_obj->trace, trace);
	else
		APS_ERR("invalid content: '%s', count = %d\n", buf, (int)count);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ir_show(struct device_driver *ddri, char *buf)
{
	int32_t reading;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	reading = stk3x1x_get_ir_value(stk3x1x_obj, STK_IRS_IT_REDUCE);

	if (reading < 0)
		return scnprintf(buf, PAGE_SIZE, "ERROR: %d\n", reading);

	stk3x1x_obj->ir_code = reading;
	return scnprintf(buf, PAGE_SIZE, "0x%04X\n", stk3x1x_obj->ir_code);
}

/*----------------------------------------------------------------------------*/
static ssize_t als_show(struct device_driver *ddri, char *buf)
{
	int res;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	if (stk3x1x_obj->hw->polling_mode_als == 1) {
		res = stk3x1x_read_als(stk3x1x_obj->client, &stk3x1x_obj->als);
		if (res)
			return scnprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
		else
			return scnprintf(buf, PAGE_SIZE,
					"%d\n", stk3x1x_obj->als);
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", stk3x1x_obj->als_code_last);
}

/*----------------------------------------------------------------------------*/
static ssize_t ps_show(struct device_driver *ddri, char *buf)
{
	int res;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	res = stk3x1x_read_ps(stk3x1x_obj->client, &stk3x1x_obj->ps);
	if (res)
		return scnprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	else
		return scnprintf(buf, PAGE_SIZE, "%d\n", stk3x1x_obj->ps);
}
/*----------------------------------------------------------------------------*/
static ssize_t reg_show(struct device_driver *ddri, char *buf)
{
	u8 int_status;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	/*read*/
	stk3x1x_check_intr(stk3x1x_obj->client, &int_status);
	stk3x1x_read_ps(stk3x1x_obj->client, &stk3x1x_obj->ps);
	stk3x1x_read_als(stk3x1x_obj->client, &stk3x1x_obj->als);
	/*write*/
	stk3x1x_write_als(stk3x1x_obj->client,
			atomic_read(&stk3x1x_obj->alsctrl_val));
	stk3x1x_write_ps(stk3x1x_obj->client,
			atomic_read(&stk3x1x_obj->psctrl_val));
	stk3x1x_write_ps_high_thd(stk3x1x_obj->client,
			atomic_read(&stk3x1x_obj->ps_high_thd_val));
	stk3x1x_write_ps_low_thd(stk3x1x_obj->client,
			atomic_read(&stk3x1x_obj->ps_low_thd_val));
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t send_show(struct device_driver *ddri, char *buf)
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t send_store(struct device_driver *ddri,
		const char *buf, size_t count)
{
	int addr, cmd;
	u8 dat;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	} else if (sscanf(buf, "%x %x", &addr, &cmd) != 2) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8)cmd;
	APS_DBG("send(%02X, %02X) = %d\n", addr, cmd,
		stk3x1x_master_send(stk3x1x_obj->client,
				    (u16)addr, &dat, sizeof(dat)));
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t recv_show(struct device_driver *ddri, char *buf)
{
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	return scnprintf(buf, PAGE_SIZE, "0x%04X\n",
			atomic_read(&stk3x1x_obj->recv_reg));
}
/*----------------------------------------------------------------------------*/
static ssize_t recv_store(struct device_driver *ddri,
					const char *buf, size_t count)
{
	int addr;
	u8 dat;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	} else if (kstrtouint(buf, 16, &addr) != 0) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	APS_DBG("recv(%02X) = %d, 0x%02X\n", addr,
			stk3x1x_master_recv(stk3x1x_obj->client,
			(u16)addr, (char *)&dat, sizeof(dat)), dat);
	atomic_set(&stk3x1x_obj->recv_reg, dat);

	return count;
}
/*----------------------------------------------------------------------------*/

static ssize_t allreg_show(struct device_driver *ddri, char *buf)
{
	int ret = 0;
	u8 rbuf[0x22];
	int cnt;
	int len = 0;

	memset(rbuf, 0, sizeof(rbuf));
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 0, &rbuf[0], 7);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 7, &rbuf[7], 7);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 14, &rbuf[14], 7);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 21, &rbuf[21], 7);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 28, &rbuf[28], 4);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client,
					STK_PDT_ID_REG, &rbuf[32], 2);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	for (cnt = 0; cnt < 0x20; cnt++) {
		APS_LOG("reg[0x%x]=0x%x\n", cnt, rbuf[cnt]);
		len += scnprintf(buf + len, PAGE_SIZE - len,
					"[%2X]%2X,", cnt, rbuf[cnt]);
	}

	APS_DBG("reg[0x3E]=0x%x\n", rbuf[cnt]);
	APS_DBG("reg[0x3F]=0x%x\n", rbuf[cnt++]);
	len += scnprintf(buf + len, PAGE_SIZE - len, "[0x3E]%2X,[0x3F]%2X\n",
			 rbuf[cnt - 1], rbuf[cnt]);
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t status_show(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	u8 rbuf[25];
	int ret = 0;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	if (stk3x1x_obj->hw) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
	"CUST: %d, (%d %d) (%02X)(%02X %02X %02X) (%02X %02X %02X %02X)\n",
				stk3x1x_obj->hw->i2c_num,
				stk3x1x_obj->hw->power_id,
				stk3x1x_obj->hw->power_vol,
				stk3x1x_obj->addr.flag,
				stk3x1x_obj->addr.alsctrl,
				stk3x1x_obj->addr.data1_als,
				stk3x1x_obj->addr.data2_als,
				stk3x1x_obj->addr.psctrl,
				stk3x1x_obj->addr.data1_ps,
				stk3x1x_obj->addr.data2_ps,
				stk3x1x_obj->addr.thdh1_ps);
	} else {
		len += scnprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}

	len += scnprintf(buf + len, PAGE_SIZE - len,
		"REGS: %02X %02X %02X %02X %02X %02X %02X %02X %02lX %02lX\n",
			atomic_read(&stk3x1x_obj->state_val),
			atomic_read(&stk3x1x_obj->psctrl_val),
			atomic_read(&stk3x1x_obj->alsctrl_val),
			stk3x1x_obj->ledctrl_val, stk3x1x_obj->int_val,
			stk3x1x_obj->wait_val,
			atomic_read(&stk3x1x_obj->ps_high_thd_val),
			atomic_read(&stk3x1x_obj->ps_low_thd_val),
			stk3x1x_obj->enable,
			stk3x1x_obj->pending_intr);
#ifdef MT6516
	len += scnprintf(buf + len, PAGE_SIZE - len, "EINT: %d (%d %d %d %d)\n",
			 mt_get_gpio_in(GPIO_ALS_EINT_PIN),
			 CUST_EINT_ALS_NUM,
			 CUST_EINT_ALS_POLARITY,
			 CUST_EINT_ALS_DEBOUNCE_EN,
			 CUST_EINT_ALS_DEBOUNCE_CN);
	len += scnprintf(buf + len, PAGE_SIZE - len, "GPIO: %d (%d %d %d %d)\n",
			 GPIO_ALS_EINT_PIN,
			 mt_get_gpio_dir(GPIO_ALS_EINT_PIN),
			 mt_get_gpio_mode(GPIO_ALS_EINT_PIN),
			 mt_get_gpio_pull_enable(GPIO_ALS_EINT_PIN),
			 mt_get_gpio_pull_select(GPIO_ALS_EINT_PIN));
#endif
	len += scnprintf(buf + len, PAGE_SIZE - len, "MISC: %d %d\n",
			 atomic_read(&stk3x1x_obj->als_suspend),
			 atomic_read(&stk3x1x_obj->ps_suspend));
	len += scnprintf(buf + len, PAGE_SIZE - len, "VER.: %s\n",
			 DRIVER_VERSION);
	memset(rbuf, 0, sizeof(rbuf));
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 0, &rbuf[0], 7);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 7, &rbuf[7], 7);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_recv(stk3x1x_obj->client, 14, &rbuf[14], 7);

	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}
	len += scnprintf(buf + len, PAGE_SIZE - len,
			"[PS=%2X] [ALS=%2X] [WAIT=%4Xms]\n",
			rbuf[0] & 0x01, (rbuf[0] & 0x02) >> 1,
			((rbuf[0] & 0x04) >> 2) * rbuf[5] * 6);
	len += scnprintf(buf + len, PAGE_SIZE - len,
			"[EN_ASO=%2X] [EN_AK=%2X] [NEAR/FAR=%2X]\n",
			(rbuf[0] & 0x20) >> 5, (rbuf[0] & 0x40) >> 6,
			rbuf[16] & 0x01);
	len += scnprintf(buf + len, PAGE_SIZE - len,
			"[FLAG_OUI=%2X] [FLAG_PSINT=%2X] [FLAG_ALSINT=%2X]\n",
			(rbuf[16] & 0x04) >> 2,
			(rbuf[16] & 0x10) >> 4, (rbuf[16] & 0x20) >> 5);
	return len;
}
/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static ssize_t pscalibration_show(struct device_driver *ddri, char *buf)
{
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t pscalibration_store(struct device_driver *ddri,
				const char *buf, size_t count)
{
	int ret;

	ret = stk3x1x_ps_calibration(stk3x1x_obj->client);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t pthredcalibration_show(struct device_driver *ddri,
							char *buf)
{
	return 0;
}

static ssize_t ps_offset_store(struct device_driver *ddri,
			const char *buf, size_t count)
{
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t enable_show(struct device_driver *ddri, char *buf)
{
	int32_t enable;
	u8 r_buf;
	int ret;

	ret = stk3x1x_master_recv(stk3x1x_obj->client,
					STK_STATE_REG, &r_buf, 0x01);
	if (ret < 0) {
		APS_ERR("error: %d\n", ret);
		return -EFAULT;
	}

	enable = (r_buf & STK_STATE_EN_ALS_MASK) ? 1 : 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", enable);
}

static ssize_t enable_store(struct device_driver *ddri,
		const char *buf, size_t size)
{
	uint8_t en;

	if (sysfs_streq(buf, "1")) {
		en = 1;
	} else if (sysfs_streq(buf, "0")) {
		en = 0;
	} else {
		APS_ERR("%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
	APS_LOG("%s: Enable ALS : %d\n", __func__, en);

	stk3x1x_enable_als(stk3x1x_obj->client, en);
	return size;
}

/*----------------------------------------------------------------------------*/
static ssize_t alstest_show(struct device_driver *ddri, char *buf)
{
	int32_t als_reading;

	APS_LOG("%s: [STK]Start testing light...\n", __func__);

	msleep(150);
	als_reading = stk3x1x_get_als_reading_avg(stk3x1x_obj->client, 5);
	APS_LOG("%s: [STK]als_reading = %d\n", __func__, als_reading);

	als_value = stk_alscode2lux(stk3x1x_obj, als_reading);

	APS_LOG("%s: [STK]Start testing light done!!! als_value = %d\n",
		__func__, als_value);
	APS_LOG("%s: [STK]Start testing light done!!! als_test_adc = %d\n",
		__func__, als_reading);
	return scnprintf(buf, PAGE_SIZE,
			"als_value = %5d lux, cci_als_test_adc = %5d\n",
			als_value, als_reading);
}

/*----------------------------------------------------------------------------*/
static ssize_t alscali_show(struct device_driver *ddri, char *buf)
{
	int32_t als_reading;
	unsigned int als_adc_cali;
	bool result = true;
	int als_calibration_lux;

	APS_LOG("%s: [STK]Start Cali light...\n", __func__);

	als_calibration_lux = ALS_CALIBRATION_LUX;

	msleep(150);
	als_reading = stk3x1x_get_als_reading_avg(stk3x1x_obj->client, 5);
	als_adc_cali = als_reading;

	als_value_cali = stk_alscode2lux(stk3x1x_obj, als_reading);

	if (((als_value_cali * stk3x1x_obj->als_transmittance)
		/ als_calibration_lux) > 0 && (als_adc_cali <= 65535)) {

		transmittance_cali = (als_value_cali
			* stk3x1x_obj->als_transmittance) / als_calibration_lux;
		/* transmittance for cali */

		stk3x1x_obj->als_transmittance = transmittance_cali;

		/* calculate lux base on calibrated transmittance */
		als_value_cali = stk_alscode2lux(stk3x1x_obj, als_reading);

		APS_LOG("%s: [STK]cali light done!!! als_value_cali = %d lux\n",
			__func__, als_value_cali);
		APS_LOG("%s:[STK]cali light done!!! transmittance_cali = %d\n",
			__func__, transmittance_cali);
		APS_LOG("%s:[STK]cali light done!!! als_adc_cali = %d code\n",
			__func__, als_adc_cali);
	} else {
		APS_LOG("%s: [STK]cali light fail!!! als_value_cali = %d lux\n",
			__func__, als_value_cali);
		APS_LOG("%s:[STK]cali light fail!!! transmittance_cali = %d\n",
			__func__, transmittance_cali);
		APS_LOG("%s:[STK]cali light fail!!! als_adc_cali = %d code\n",
			__func__, als_adc_cali);
		result = false;
	}
	/* compute transmittance for ALS calibration end */

	return scnprintf(buf, PAGE_SIZE,
		"%s: cali =%d lux, transmittance_cali= %d,adc_cali= %d code\n",
		result ? "PASSED" : "FAIL", als_value_cali,
		transmittance_cali, als_adc_cali);
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR_RO(als);
static DRIVER_ATTR_RO(ps);
static DRIVER_ATTR_RO(ir);
static DRIVER_ATTR_RW(config);
static DRIVER_ATTR_RW(trace);
static DRIVER_ATTR_RO(status);
static DRIVER_ATTR_RW(send);
static DRIVER_ATTR_RW(recv);
static DRIVER_ATTR_RO(reg);
static DRIVER_ATTR_RO(allreg);
static DRIVER_ATTR_RW(pscalibration);
static DRIVER_ATTR_RO(pthredcalibration);
static DRIVER_ATTR_WO(ps_offset);
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR_RW(enable);
static DRIVER_ATTR_RO(alstest);
static DRIVER_ATTR_RO(alscali);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *stk3x1x_attr_list[] = {
	&driver_attr_als,
	&driver_attr_ps,
	&driver_attr_ir,
	&driver_attr_trace,		   /*trace log*/
	&driver_attr_config,
	&driver_attr_status,
	&driver_attr_send,
	&driver_attr_recv,
	&driver_attr_allreg,
	&driver_attr_reg,
	&driver_attr_pscalibration,
	&driver_attr_pthredcalibration,
	&driver_attr_ps_offset,

	&driver_attr_enable,
	&driver_attr_alstest,
	&driver_attr_alscali,
};

/*----------------------------------------------------------------------------*/
static int stk3x1x_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(stk3x1x_attr_list) /
			sizeof(stk3x1x_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, stk3x1x_attr_list[idx]);
		if (err) {
			APS_ERR("driver_create_file (%s) = %d\n",
					stk3x1x_attr_list[idx]->attr.name, err);
			break;
		}
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(stk3x1x_attr_list) /
			sizeof(stk3x1x_attr_list[0]));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, stk3x1x_attr_list[idx]);

	return err;
}

/*
 * Function Configuration
 */
static uint32_t stk_alscode2lux(struct stk3x1x_priv *obj, uint32_t alscode)
{
	/*
	 * Lux = ALS ADC data / Display Transmittance
	 * Formula for ADC to LUX
	 */
	alscode += ((alscode<<7)+(alscode<<3)+(alscode>>1));
	alscode <<= 3;
	alscode /= obj->als_transmittance;
	return alscode;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_get_ps_value_only(struct stk3x1x_priv *obj, u16 ps)
{
	int mask = atomic_read(&obj->ps_mask);
	int invalid = 0, val;
	int err;
	u8 flag;

	err = stk3x1x_read_flag(obj->client, &flag);
	if (err)
		return err;

	val = (flag & STK_FLG_NF_MASK) ? 1 : 0;

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
		if (unlikely(atomic_read(&obj->trace) & STK_TRC_CVT_PS)) {
			if (mask)
				APS_DBG("PS: %05d => %05d [M]\n", ps, val);
			else
				APS_DBG("PS: %05d => %05d\n", ps, val);
		}

		return val;
	}
	APS_ERR(" ps value is invalid, PS: %05d => %05d\n", ps, val);

	if (unlikely(atomic_read(&obj->trace) & STK_TRC_CVT_PS))
		APS_ERR("PS:  %05d => %05d (-1)\n", ps, val);

	return -1;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_get_ps_value(struct stk3x1x_priv *obj, u16 ps)
{
	int mask = atomic_read(&obj->ps_mask);
	int invalid = 0, val;
	int err;
	u8 flag;

	err = stk3x1x_read_flag(obj->client, &flag);
	if (err)
		return err;

	val = (flag & STK_FLG_NF_MASK) ? 1 : 0;

	APS_DBG("%s: read_flag = 0x%x, val = %d\n", __func__, flag, val);

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
		if (unlikely(atomic_read(&obj->trace) & STK_TRC_CVT_PS)) {
			if (mask)
				APS_DBG("PS:  %05d => %05d [M]\n", ps, val);
			else
				APS_DBG("PS:  %05d => %05d\n", ps, val);
		}

		return val;
	}
	APS_ERR("ps value is invalid, PS: %05d => %05d\n", ps, val);

	if (unlikely(atomic_read(&obj->trace) & STK_TRC_CVT_PS))
		APS_ERR("PS:  %05d => %05d (-1)\n", ps, val);

	return -1;
}

/*----------------------------------------------------------------------------*/

static int32_t stk3x1x_set_irs_it_slp(struct stk3x1x_priv *obj,
				uint16_t *slp_time, int32_t ials_it_reduce)
{
	uint8_t irs_alsctrl;
	int32_t ret;

	irs_alsctrl = (atomic_read(&obj->alsctrl_val) & 0x0F) - ials_it_reduce;
	switch (irs_alsctrl) {
	case 2:
		*slp_time = 1;
		break;

	case 3:
		*slp_time = 2;
		break;

	case 4:
		*slp_time = 3;
		break;

	case 5:
		*slp_time = 6;
		break;

	case 6:
		*slp_time = 12;
		break;

	case 7:
		*slp_time = 24;
		break;

	case 8:
		*slp_time = 48;
		break;

	case 9:
		*slp_time = 96;
		break;

	case 10:
		*slp_time = 192;
		break;

	default:
		APS_ERR("%s: unknown ALS IT=0x%x\n", __func__, irs_alsctrl);
		ret = -EINVAL;
		return ret;
	}

	irs_alsctrl |= (atomic_read(&obj->alsctrl_val) & 0xF0);
	ret = i2c_smbus_write_byte_data(obj->client,
					STK_ALSCTRL_REG, irs_alsctrl);

	if (ret < 0) {
		APS_ERR("%s: write i2c error\n", __func__);
		return ret;
	}

	return 0;
}

static int32_t stk3x1x_get_ir_value(struct stk3x1x_priv *obj,
		int32_t als_it_reduce)
{
	int32_t word_data, ret;
	uint8_t w_reg, retry = 0;
	uint16_t irs_slp_time = 100;

	u8 flag;
	u8 buf[2];

	ret = stk3x1x_set_irs_it_slp(obj, &irs_slp_time, als_it_reduce);

	if (ret < 0)
		goto irs_err_i2c_rw;

	w_reg = atomic_read(&obj->state_val) | STK_STATE_EN_IRS_MASK;
	ret = i2c_smbus_write_byte_data(obj->client, STK_STATE_REG, w_reg);

	if (ret < 0) {
		APS_ERR("%s: write i2c error\n", __func__);
		goto irs_err_i2c_rw;
	}

	msleep(irs_slp_time);

	do {
		msleep(20);
		ret = stk3x1x_read_flag(obj->client, &flag);

		if (ret < 0) {
			APS_ERR("WARNING: read flag reg error: %d\n", ret);
			goto irs_err_i2c_rw;
		}

		retry++;
	} while (retry < 10 && ((flag & STK_FLG_IR_RDY_MASK) == 0));

	if (retry == 10) {
		APS_ERR("%s: ir data is not ready for a long time\n", __func__);
		ret = -EINVAL;
		goto irs_err_i2c_rw;
	}

	ret = stk3x1x_clear_intr(obj->client, flag, STK_FLG_IR_RDY_MASK);

	if (ret < 0) {
		APS_ERR("%s: write i2c error\n", __func__);
		goto irs_err_i2c_rw;
	}

	ret = stk3x1x_master_recv(obj->client, STK_DATA1_IR_REG, buf, 2);

	if (ret < 0) {
		APS_ERR("%s fail, ret=0x%x", __func__, ret);
		goto irs_err_i2c_rw;
	}

	word_data = (buf[0] << 8) | buf[1];
	ret = i2c_smbus_write_byte_data(obj->client, STK_ALSCTRL_REG,
				atomic_read(&obj->alsctrl_val));

	if (ret < 0) {
		APS_ERR("%s: write i2c error\n", __func__);
		goto irs_err_i2c_rw;
	}
	return word_data;
irs_err_i2c_rw:
	return ret;
}

/*
 * Function Configuration
 */
static int stk3x1x_open(struct inode *inode, struct file *file)
{
	file->private_data = stk3x1x_i2c_client;

	if (!file->private_data) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
static long stk3x1x_unlocked_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	long err = 0;
	void __user *ptr = (void __user *) arg;
	int dat;
	uint32_t enable;
	int ps_result, ps_cali_result;
	int threshold[2];

	switch (cmd) {
	case ALSPS_SET_PS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}

		if (enable) {
			err = stk3x1x_enable_ps(obj->client, 1, 1);
			if (err) {
				APS_ERR("enable ps fail: %ld\n", err);
				goto err_out;
			}

			set_bit(STK_BIT_PS, &obj->enable);
		} else {
			err = stk3x1x_enable_ps(obj->client, 0, 1);
			if (err) {
				APS_ERR("disable ps fail: %ld\n", err);
				goto err_out;
			}

			clear_bit(STK_BIT_PS, &obj->enable);
		}

		break;

	case ALSPS_GET_PS_MODE:
		enable = test_bit(STK_BIT_PS, &obj->enable) ? (1) : (0);

		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}

		break;

	case ALSPS_GET_PS_DATA:
		err = stk3x1x_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;

		dat = stk3x1x_get_ps_value(obj, obj->ps);
		if (dat < 0) {
			err = dat;
			goto err_out;
		}

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}

		break;

	case ALSPS_GET_PS_RAW_DATA:
		err = stk3x1x_read_ps(obj->client, &obj->ps);
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
			err = stk3x1x_enable_als(obj->client, 1);
			if (err) {
				APS_ERR("enable als fail: %ld\n", err);
				goto err_out;
			}

			set_bit(STK_BIT_ALS, &obj->enable);
		} else {
			err = stk3x1x_enable_als(obj->client, 0);
			if (err) {
				APS_ERR("disable als fail: %ld\n", err);
				goto err_out;
			}

			clear_bit(STK_BIT_ALS, &obj->enable);
		}

		break;

	case ALSPS_GET_ALS_MODE:
		enable = test_bit(STK_BIT_ALS, &obj->enable) ? (1) : (0);

		if (copy_to_user(ptr, &enable, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}

		break;

	case ALSPS_GET_ALS_DATA:
		err = stk3x1x_read_als(obj->client, &obj->als);
		if (err)
			goto err_out;

		dat = stk_alscode2lux(obj, obj->als);

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}

		break;

	case ALSPS_GET_ALS_RAW_DATA:
		err = stk3x1x_read_als(obj->client, &obj->als);
		if (err)
			goto err_out;

		dat = obj->als;

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}

		break;

	case ALSPS_GET_PS_THRESHOLD_HIGH:
		dat = atomic_read(&obj->ps_high_thd_val);
		APS_LOG("%s:ps_high_thd_val:%d\n", __func__, dat);

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}

		break;

	case ALSPS_GET_PS_THRESHOLD_LOW:
		dat = atomic_read(&obj->ps_low_thd_val);
		APS_LOG("%s:ps_low_thd_val:%d\n", __func__, dat);

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}

		break;

	case ALSPS_GET_PS_TEST_RESULT:
		err = stk3x1x_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;

		if (obj->ps > atomic_read(&obj->ps_high_thd_val))
			ps_result = 0;
		else
			ps_result = 1;

		APS_LOG("ALSPS_GET_PS_TEST: ps_result = %d\n", ps_result);

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

		obj->ps_cali = 0;
		atomic_set(&obj->ps_high_thd_val, obj->hw->ps_threshold_high);
		atomic_set(&obj->ps_low_thd_val, obj->hw->ps_threshold_low);
		APS_LOG("ALSPS_IOCTL_CLR_CAL : ps_cali:%d high:%d low:%d\n",
			obj->ps_cali,
			atomic_read(&obj->ps_high_thd_val),
			atomic_read(&obj->ps_low_thd_val));
		break;

	case ALSPS_IOCTL_GET_CALI:
		stk3x1x_ps_calibration(obj->client);
		ps_cali_result = obj->ps_cali;
		APS_LOG("ALSPS_IOCTL_GET_CAL : ps_cali = %d\n", obj->ps_cali);

		if (copy_to_user(ptr, &ps_cali_result,
				sizeof(ps_cali_result))) {
			err = -EFAULT;
			goto err_out;
		}

		break;

	case ALSPS_IOCTL_SET_CALI:

		/*1. libhwm.so calc value store in ps_cali;*/
		/*2. nvram_daemon update ps_cali in driver*/
		if (copy_from_user(&ps_cali_result, ptr,
			sizeof(ps_cali_result))) {
			err = -EFAULT;
			goto err_out;
		}

		obj->ps_cali = ps_cali_result;

		atomic_set(&obj->ps_high_thd_val, obj->ps_cali + 200);
		atomic_set(&obj->ps_low_thd_val, obj->ps_cali + 150);

		err = stk3x1x_write_ps_high_thd(obj->client,
				atomic_read(&obj->ps_high_thd_val));
		if (err)
			goto err_out;

		err = stk3x1x_write_ps_low_thd(obj->client,
				atomic_read(&obj->ps_low_thd_val));
		if (err)
			goto err_out;

		APS_LOG("ALSPS_IOCTL_SET_CAL :ps_cali_result = %d\n",
			ps_cali_result);
		APS_LOG("ALSPS_IOCTL_SET_CAL :obj->ps_cali:%d high:%d low:%d\n",
			obj->ps_cali,
			atomic_read(&obj->ps_high_thd_val),
			atomic_read(&obj->ps_low_thd_val));
		break;

	case ALSPS_SET_PS_THRESHOLD:
		if (copy_from_user(threshold, ptr, sizeof(threshold))) {
			err = -EFAULT;
			goto err_out;
		}

		APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n",
					__func__, threshold[0], threshold[1]);
		atomic_set(&obj->ps_high_thd_val,
				(threshold[0] + obj->ps_cali));
		atomic_set(&obj->ps_low_thd_val,
				(threshold[1] + obj->ps_cali));

		err = stk3x1x_write_ps_high_thd(obj->client,
				atomic_read(&obj->ps_high_thd_val));
		if (err) {
			APS_ERR("write high thd error: %ld\n", err);
			goto err_out;
		}

		err = stk3x1x_write_ps_low_thd(obj->client,
				atomic_read(&obj->ps_low_thd_val));
		if (err) {
			APS_ERR("write low thd error: %ld\n", err);
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
/*----------------------------------------------------------------------------*/
static const struct file_operations stk3x1x_fops = {
	.open = stk3x1x_open,
	.release = stk3x1x_release,
	.unlocked_ioctl = stk3x1x_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice stk3x1x_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &stk3x1x_fops,
};
/*----------------------------------------------------------------------------*/

#ifdef CONFIG_PM_SLEEP
static int stk3x1x_suspend(struct device *dev)
{
	struct stk3x1x_priv *obj = dev_get_drvdata(dev);
	int err = 0;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	atomic_set(&obj->als_suspend, 1);
	err = stk3x1x_enable_als(obj->client, 0);
	if (err) {
		APS_ERR("disable als fail: %d\n", err);
		return err;
	}

	atomic_set(&obj->ps_suspend, 1);
	err = stk3x1x_enable_ps(obj->client, 0, 1);
	if (err) {
		APS_ERR("disable ps fail: %d\n", err);
		return err;
	}

	return 0;
}

static int stk3x1x_resume(struct device *dev)
{
	struct stk3x1x_priv *obj = dev_get_drvdata(dev);
	int err = 0;

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	err = stk3x1x_init_client(obj->client);
	if (err) {
		APS_ERR("initialize client fail!!\n");
		return err;
	}

	err = stk3x1x_enable_als(obj->client, 1);
	if (err) {
		APS_ERR("enable als fail: %d\n", err);
		return err;
	}

	atomic_set(&obj->als_suspend, 0);
	err = stk3x1x_enable_ps(obj->client, 1, 1);
	if (err) {
		APS_ERR("enable ps fail: %d\n", err);
		return err;
	}

	atomic_set(&obj->ps_suspend, 0);

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int stk3x1x_i2c_detect(struct i2c_client *client,
		struct i2c_board_info *info)
{
	strcpy(info->type, stk3x1x_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int als_open_report_data(int open)
{
	return 0;
}

static int als_enable_nodata(int en)
{
	int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /*#ifdef CUSTOM_KERNEL_SENSORHUB*/
	APS_LOG("stk3x1x_obj als enable value = %d\n", en);
#ifdef CUSTOM_KERNEL_SENSORHUB
	req.activate_req.sensorType = ID_LIGHT;
	req.activate_req.action = SENSOR_HUB_ACTIVATE;
	req.activate_req.enable = en;
	len = sizeof(req.activate_req);
	res = SCP_sensorHub_req_send(&req, &len, 1);
#else /*#ifdef CUSTOM_KERNEL_SENSORHUB*/

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return -1;
	}

	res = stk3x1x_enable_als(stk3x1x_obj->client, en);
#endif /*#ifdef CUSTOM_KERNEL_SENSORHUB*/

	if (res) {
		APS_ERR("%s is failed!!\n", __func__);
		return -1;
	}

	return 0;
}

static int als_set_delay(u64 ns)
{
	/* TODO */
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

#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#else
	struct stk3x1x_priv *obj = NULL;
#endif /*#ifdef CUSTOM_KERNEL_SENSORHUB*/

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.get_data_req.sensorType = ID_LIGHT;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);

	if (err) {
		APS_ERR("SCP_sensorHub_req_send fail!\n");
	} else {
		*value = req.get_data_rsp.int16_Data[0];
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&stk3x1x_obj->trace) & CMC_TRC_PS_DATA) {
		APS_DBG("value = %d\n", *value);
		/*show data*/
	}

#else /*#ifdef CUSTOM_KERNEL_SENSORHUB*/

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return -1;
	}

	obj = stk3x1x_obj;

	err = stk3x1x_read_als(obj->client, &obj->als);
	if (err) {
		err = -1;
		goto out;
	}

	*value = stk_alscode2lux(obj, obj->als);

	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

#endif /*#ifdef CUSTOM_KERNEL_SENSORHUB*/

out:
	return err;
}

static int ps_open_report_data(int open)
{
	return 0;
}

static int ps_enable_nodata(int en)
{
	int res = 0;

#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /*#ifdef CUSTOM_KERNEL_SENSORHUB*/
	APS_LOG("stk3x1x_obj ps enable value = %d\n", en);
#ifdef CUSTOM_KERNEL_SENSORHUB
	req.activate_req.sensorType = ID_PROXIMITY;
	req.activate_req.action = SENSOR_HUB_ACTIVATE;
	req.activate_req.enable = en;
	len = sizeof(req.activate_req);
	res = SCP_sensorHub_req_send(&req, &len, 1);
#else /*#ifdef CUSTOM_KERNEL_SENSORHUB*/

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return -1;
	}

	res = stk3x1x_enable_ps(stk3x1x_obj->client, en, 1);

	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}

#endif /*#ifdef CUSTOM_KERNEL_SENSORHUB*/
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
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif /*#ifdef CUSTOM_KERNEL_SENSORHUB*/
#ifdef CUSTOM_KERNEL_SENSORHUB
	req.get_data_req.sensorType = ID_PROXIMITY;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);

	if (err) {
		APS_ERR("SCP_sensorHub_req_send fail!\n");
	} else {
		*value = req.get_data_rsp.int16_Data[0];
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&stk3x1x_obj->trace) & CMC_TRC_PS_DATA) {
		APS_DBG("value = %d\n", *value);
		/*show data*/
	}

#else /*#ifdef CUSTOM_KERNEL_SENSORHUB*/

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return -1;
	}

	err = stk3x1x_read_ps(stk3x1x_obj->client, &stk3x1x_obj->ps);
	if (err) {
		err = -1;
	} else {
		*value = stk3x1x_get_ps_value(stk3x1x_obj, stk3x1x_obj->ps);
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

#endif /*#ifdef CUSTOM_KERNEL_SENSORHUB*/
	return 0;
}


/*----------------------------------------------------------------------------*/
static int stk3x1x_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct stk3x1x_priv *obj = NULL;
	int err = 0;
	struct als_control_path als_ctl = { 0 };
	struct als_data_path als_data = { 0 };
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };

	APS_LOG("%s: driver version: %s\n", __func__, DRIVER_VERSION);
	err = get_alsps_dts_func(client->dev.of_node, hw);
	if (err < 0) {
		APS_ERR("get customization info from dts failed\n");
		goto exit;
	}

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));
	stk3x1x_obj = obj;
	obj->hw = hw;
	stk3x1x_get_addr(obj->hw, &obj->addr);
	INIT_DELAYED_WORK(&obj->eint_work, stk3x1x_eint_work);
	client->addr = *hw->i2c_addr;
	obj->client = client;
	i2c_set_clientdata(client, obj);
	atomic_set(&obj->als_debounce, 200);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 10);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->trace, 0x00);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->init_done, 0);
	obj->irq_node = client->dev.of_node;
	atomic_set(&obj->state_val, 0);
	atomic_set(&obj->psctrl_val, 0x30); /*0x31*/
	atomic_set(&obj->alsctrl_val, 0x39);
	obj->ledctrl_val = 0xFF;
	obj->wait_val = 0xF;
	obj->int_val = 0;
	obj->first_boot = true;
	obj->als_correct_factor = 1000;

	atomic_set(&obj->ps_high_thd_val, obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_low_thd_val, obj->hw->ps_threshold_low);
	atomic_set(&obj->recv_reg, 0);

	if (obj->hw->polling_mode_ps == 0)
		APS_LOG("%s: enable PS interrupt\n", __func__);

	obj->int_val |= STK_INT_PS_MODE1;

	if (obj->hw->polling_mode_als == 0) {
		obj->int_val |= STK_INT_ALS;
		APS_LOG("%s: enable ALS interrupt\n", __func__);
	}

	obj->enable = 0;
	obj->pending_intr = 0;
	atomic_set(&obj->i2c_retry, 3);

	if (atomic_read(&obj->state_val) & STK_STATE_EN_ALS_MASK)
		set_bit(STK_BIT_ALS, &obj->enable);

	if (atomic_read(&obj->state_val) & STK_STATE_EN_PS_MASK)
		set_bit(STK_BIT_PS, &obj->enable);

	stk3x1x_i2c_client = client;

	err = stk3x1x_init_client(client);
	if (err) {
		APS_ERR("stk3x1x init client failed\n");
		goto exit_init_failed;
	}

	err = misc_register(&stk3x1x_device);
	if (err) {
		APS_ERR("stk3x1x_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	err =
	stk3x1x_create_attr(&(stk3x1x_init_info.platform_diver_addr->driver));

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
	als_ctl.is_use_common_factory = false;

	if (obj->hw->polling_mode_als == 1)
		als_ctl.is_polling_mode = true;
	else
		als_ctl.is_polling_mode = false;

#ifdef CUSTOM_KERNEL_SENSORHUB
	als_ctl.is_support_batch = obj->hw->is_batch_supported_als;
#else
	als_ctl.is_support_batch = false;
#endif
	err = als_register_control_path(&als_ctl);

	if (err) {
		APS_ERR("als_control register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if (err) {
		APS_ERR("als_data register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.batch = ps_batch;
	ps_ctl.flush = ps_flush;
	ps_ctl.ps_calibration = NULL;
	ps_ctl.ps_calibration = NULL;
	ps_ctl.is_use_common_factory = false;

	if (obj->hw->polling_mode_ps == 1) {
		ps_ctl.is_polling_mode = true;
		ps_ctl.is_report_input_direct = false;
		wakeup_source_init(&mps_lock, "ps wakelock");
	} else {
		ps_ctl.is_polling_mode = false;
		ps_ctl.is_report_input_direct = true;
	}
#ifdef CUSTOM_KERNEL_SENSORHUB
	ps_ctl.is_support_batch = obj->hw->is_batch_supported_ps;
#else
	ps_ctl.is_support_batch = false;
#endif
	err = ps_register_control_path(&ps_ctl);
	if (err) {
		APS_ERR("ps_control register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err) {
		APS_ERR("ps_data register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	stk3x1x_init_flag = 0;
	APS_LOG("%s: state_val=0x%x, psctrl_val=0x%x, alsctrl_val=0x%x\n",
		__func__, atomic_read(&obj->state_val),
		atomic_read(&obj->psctrl_val), atomic_read(&obj->alsctrl_val));
	APS_LOG("ledctrl_val=0x%x, wait_val=0x%x, int_val=0x%x\n",
		obj->ledctrl_val, obj->wait_val, obj->int_val);
	/*
	 * Since alsps sensor driver in AP,
	 * it should register information to sensorlist.
	 */
	strncpy(alsps_devinfo.name, STK3x1x_DEV_NAME, sizeof(STK3x1x_DEV_NAME));
	sensorlist_register_deviceinfo(ID_PRESSURE, &alsps_devinfo);
	APS_LOG("%s: OK\n", __func__);
	return 0;

exit_sensor_obj_attach_fail:
exit_create_attr_failed:
	misc_deregister(&stk3x1x_device);
exit_misc_device_register_failed:
exit_init_failed:
	kfree(obj);
exit:
	stk3x1x_i2c_client = NULL;
#ifdef MT6516
	MT6516_EINTIRQMask(CUST_EINT_ALS_NUM);	/*mask interrupt if fail*/
#endif
	stk3x1x_init_flag = -1;
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int stk3x1x_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err =
	stk3x1x_delete_attr(&(stk3x1x_init_info.platform_diver_addr->driver));

	if (err)
		APS_ERR("stk3x1x_delete_attr fail: %d\n", err);

	misc_deregister(&stk3x1x_device);

	stk3x1x_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static int stk3x1x_local_uninit(void)
{
	APS_FUN();

	i2c_del_driver(&stk3x1x_i2c_driver);
	stk3x1x_i2c_client = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/

static int stk3x1x_local_init(void)
{
	struct stk3x1x_i2c_addr addr;

	APS_FUN();

	stk3x1x_get_addr(hw, &addr);

	if (i2c_add_driver(&stk3x1x_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -1;
	}

	if (-1 == stk3x1x_init_flag) {
		APS_ERR("%s fail with stk3x1x_init_flag=%d\n",
			__func__, stk3x1x_init_flag);
		return -1;
	}

	/* wait for idme for calibration ready open it */
	als_cal = 65536; //idme_get_alscal_value();

	if (als_cal > 0 && als_cal <= 65535)
		stk3x1x_obj->als_transmittance = als_cal;
	else
		stk3x1x_obj->als_transmittance = ALS_DEFAULT_TRANSMITTANCE;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init stk3x1x_init(void)
{
	alsps_driver_add(&stk3x1x_init_info);
	/* hwmsen_alsps_add(&stk3x1x_init_info);*/
	pr_info("%s done\n", __func__);
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit stk3x1x_exit(void)
{
	APS_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(stk3x1x_init);
module_exit(stk3x1x_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("MingHsien Hsieh");
MODULE_DESCRIPTION("SensorTek stk3x1x proximity and light sensor driver");
MODULE_LICENSE("GPL");

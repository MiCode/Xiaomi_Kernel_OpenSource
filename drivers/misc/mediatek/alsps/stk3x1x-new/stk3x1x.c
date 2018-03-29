/*
* Copyright(C)2014 MediaTek Inc.
* Modification based on code covered by the below mentioned copyright
* and/or permission notice(S).
*/

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
#include <linux/fs.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/of_irq.h>

#include <alsps.h>
#include "stk3x1x.h"

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif

/* #define STK_PS_POLLING_LOG */
/* #define STK_FIR */
/* #define STK_IRS */
/* #include <mach/mt_devs.h> */

#define DRIVER_VERSION          "3.1.2.1nk"

/*------------------------- define-------------------------------*/
#define POWER_NONE_MACRO MT65XX_POWER_NONE

/******************************************************************************
 * configuration
*******************************************************************************/
#define PSCTRL_VAL	0x71	/* ps_persistance=4, ps_gain=64X, PS_IT=0.391ms */
#define ALSCTRL_VAL	0x38	/* als_persistance=1, als_gain=64X, ALS_IT=50ms */
#define LEDCTRL_VAL	0xFF	/* 100mA IRDR, 64/64 LED duty */
#define WAIT_VAL		0x7	/* 50 ms */

/*----------------------------------------------------------------------------*/
#define stk3x1x_DEV_NAME     "stk3x1x"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               pr_debug(APS_TAG"%s\n", __func__)
#define APS_ERR(fmt, args...)    pr_err(APS_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    pr_debug(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    pr_debug(fmt, ##args)
/******************************************************************************
 * extern functions
*******************************************************************************/
/*----------------------------------------------------------------------------*/
#define STK2213_PID			0x23
#define STK2213I_PID			0x22
#define STK3010_PID			0x33
#define STK3210_STK3310_PID	0x13
#define STK3211_STK3311_PID	0x12

#define STK_IRC_MAX_ALS_CODE		20000
#define STK_IRC_MIN_ALS_CODE		25
#define STK_IRC_MIN_IR_CODE		50
#define STK_IRC_ALS_DENOMI		2
#define STK_IRC_ALS_NUMERA		5
#define STK_IRC_ALS_CORREC		748

/*----------------------------------------------------------------------------*/
static struct i2c_client *stk3x1x_i2c_client;
struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id stk3x1x_i2c_id[] = { {stk3x1x_DEV_NAME, 0}, {} };

/*----------------------------------------------------------------------------*/
static int stk3x1x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int stk3x1x_i2c_remove(struct i2c_client *client);
/*----------------------------------------------------------------------------*/
static int stk3x1x_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int stk3x1x_i2c_resume(struct i2c_client *client);
static struct stk3x1x_priv *g_stk3x1x_ptr;
static unsigned long long int_top_time;
#define C_I2C_FIFO_SIZE     8

static DEFINE_MUTEX(STK3X1X_i2c_mutex);
static int stk3x1x_init_flag = -1;	/* 0<==>OK -1 <==> fail */
static int stk3x1x_local_init(void);
static int stk3x1x_local_uninit(void);
static struct alsps_init_info stk3x1x_init_info = {
	.name = "stk3x1x",
	.init = stk3x1x_local_init,
	.uninit = stk3x1x_local_uninit,
};


/*----------------------------------------------------------------------------*/
typedef enum {
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
typedef enum {
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
#ifdef STK_FIR
struct data_filter {
	s16 raw[8];
	int sum;
	int num;
	int idx;
};
#endif

struct stk3x1x_priv {
	struct alsps_hw *hw;
	struct i2c_client *client;
	struct delayed_work eint_work;

	/*i2c address group */
	struct stk3x1x_i2c_addr addr;

	struct device_node *irq_node;
	int irq;

	/*misc */
	atomic_t trace;
	atomic_t i2c_retry;
	atomic_t als_suspend;
	atomic_t als_debounce;	/*debounce time after enabling als */
	atomic_t als_deb_on;	/*indicates if the debounce is on */
	atomic_t als_deb_end;	/*the jiffies representing the end of debounce */
	atomic_t ps_mask;	/*mask ps: always return far away */
	atomic_t ps_debounce;	/*debounce time after enabling ps */
	atomic_t ps_deb_on;	/*indicates if the debounce is on */
	atomic_t ps_deb_end;	/*the jiffies representing the end of debounce */
	atomic_t ps_suspend;


	/*data */
	u16 als;
	u16 ps;
	u8 _align;
	u16 als_level_num;
	u16 als_value_num;
	u32 als_level[C_CUST_ALS_LEVEL - 1];
	u32 als_value[C_CUST_ALS_LEVEL];
	int ps_cali;

	atomic_t state_val;
	atomic_t psctrl_val;
	atomic_t alsctrl_val;
	u8 wait_val;
	u8 ledctrl_val;
	u8 int_val;

	atomic_t ps_high_thd_val;	/*the cmd value can't be read, stored in ram */
	atomic_t ps_low_thd_val;	/*the cmd value can't be read, stored in ram */
	ulong enable;		/*enable mask */
	ulong pending_intr;	/*pending interrupt */
	atomic_t recv_reg;
	/*early suspend */
	bool first_boot;
#ifdef STK_FIR
	struct data_filter fir;
#endif
	uint16_t ir_code;
	uint16_t als_correct_factor;
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id alsps_of_match[] = {
	{.compatible = "mediatek,alsps"},
	{},
};
#endif

static struct i2c_driver stk3x1x_i2c_driver = {
	.probe = stk3x1x_i2c_probe,
	.remove = stk3x1x_i2c_remove,
	.suspend = stk3x1x_i2c_suspend,
	.resume = stk3x1x_i2c_resume,
	.id_table = stk3x1x_i2c_id,
	.driver = {
		.name = stk3x1x_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = alsps_of_match,
#endif
	},
};

#if defined(CONFIG_CUSTOM_KERNEL_SENSORHUB)
struct stk_raw_data {
	u8 raw_bytes[PACKAGE_SIZE];
	u16 ps_raw;
	u16 ps_state;
	u16 ps_int_state;
	u16 als_ch0_raw;
	u16 als_ch1_raw;
};
static struct stk_raw_data gRawData;
#endif

static struct stk3x1x_priv *stk3x1x_obj;
static int stk3x1x_get_ps_value(struct stk3x1x_priv *obj, u16 ps);
static int stk3x1x_get_ps_value_only(struct stk3x1x_priv *obj, u16 ps);
static int stk3x1x_get_als_value(struct stk3x1x_priv *obj, u16 als);
static int stk3x1x_read_als(struct i2c_client *client, u16 *data);
static int stk3x1x_read_ps(struct i2c_client *client, u16 *data);
/*static int stk3x1x_set_als_int_thd(struct i2c_client *client, u16 als_data_reg);*/
static int32_t stk3x1x_get_ir_value(struct stk3x1x_priv *obj);
struct wake_lock ps_lock;

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
static int stk3x1x_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	int err;
	u8 beg = addr;
	struct i2c_msg msgs[2] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &beg, },
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data, }
	};

	mutex_lock(&STK3X1X_i2c_mutex);
	if (!client) {
		mutex_unlock(&STK3X1X_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		mutex_unlock(&STK3X1X_i2c_mutex);
		APS_LOG(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	mutex_unlock(&STK3X1X_i2c_mutex);
	if (err != 2) {
		APS_LOG("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;	/*no error */
	}
	return err;
}

static int stk3x1x_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	int err, idx, num;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&STK3X1X_i2c_mutex);
	if (!client) {
		mutex_unlock(&STK3X1X_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		APS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&STK3X1X_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++) {
		buf[num++] = data[idx];
	}

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		APS_ERR("send command error!!\n");
		mutex_unlock(&STK3X1X_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&STK3X1X_i2c_mutex);
	return err;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_get_timing(void)
{
	return 200;
/*
	u32 base = I2C2_BASE;
	return (__raw_readw(mt6516_I2C_HS) << 16) | (__raw_readw(mt6516_I2C_TIMING));
*/
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_master_recv(struct i2c_client *client, u16 addr, u8 *buf, int count)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0, retry = 0;
	int trc = atomic_read(&obj->trace);
	int max_try = atomic_read(&obj->i2c_retry);

	while (retry++ < max_try) {
		ret = stk3x1x_i2c_read_block(client, addr, buf, count);
		if (ret == 0)
			break;
		udelay(100);
	}

	if (unlikely(trc)) {
		if ((retry != 1) && (trc & STK_TRC_DEBUG)) {
			APS_LOG("(recv) %d/%d\n", retry - 1, max_try);

		}
	}

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	return (ret == 0) ? count : ret;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_master_send(struct i2c_client *client, u16 addr, u8 *buf, int count)
{
	int ret = 0, retry = 0;
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int trc = atomic_read(&obj->trace);
	int max_try = atomic_read(&obj->i2c_retry);


	while (retry++ < max_try) {
		ret = stk3x1x_i2c_write_block(client, addr, buf, count);
		if (ret == 0)
			break;
		udelay(100);
	}

	if (unlikely(trc)) {
		if ((retry != 1) && (trc & STK_TRC_DEBUG)) {
			APS_LOG("(send) %d/%d\n", retry - 1, max_try);
		}
	}
	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	   transmitted, else error code. */
	return (ret == 0) ? count : ret;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_write_led(struct i2c_client *client, u8 data)
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
int stk3x1x_read_als(struct i2c_client *client, u16 *data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;
	u8 buf[2];
	int32_t als_comperator;
	u16 als_data;
#ifdef STK_FIR
	int idx;
#endif
	if (NULL == client) {
		return -EINVAL;
	}
	ret = stk3x1x_master_recv(client, obj->addr.data1_als, buf, 0x02);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	} else {
		als_data = (buf[0] << 8) | (buf[1]);
#ifdef STK_FIR
		if (obj->fir.num < 8) {
			obj->fir.raw[obj->fir.num] = als_data;
			obj->fir.sum += als_data;
			obj->fir.num++;
			obj->fir.idx++;
		} else {
			idx = obj->fir.idx % 8;
			obj->fir.sum -= obj->fir.raw[idx];
			obj->fir.raw[idx] = als_data;
			obj->fir.sum += als_data;
			obj->fir.idx++;
			als_data = obj->fir.sum / 8;
		}
#endif
	}

	if (obj->ir_code) {
		obj->als_correct_factor = 1000;
		if (als_data < STK_IRC_MAX_ALS_CODE && als_data > STK_IRC_MIN_ALS_CODE &&
		    obj->ir_code > STK_IRC_MIN_IR_CODE) {
			als_comperator = als_data * STK_IRC_ALS_NUMERA / STK_IRC_ALS_DENOMI;
			if (obj->ir_code > als_comperator)
				obj->als_correct_factor = STK_IRC_ALS_CORREC;
		}
		APS_LOG("%s: als=%d, ir=%d, als_correct_factor=%d", __func__, als_data,
			obj->ir_code, obj->als_correct_factor);
		obj->ir_code = 0;
	}
	*data = als_data * obj->als_correct_factor / 1000;

	if (atomic_read(&obj->trace) & STK_TRC_ALS_DATA) {
		APS_DBG("ALS: 0x%04X\n", (u32) (*data));
	}

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
int stk3x1x_read_flag(struct i2c_client *client, u8 *data)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret = 0;
	u8 buf;

	if (NULL == client) {
		return -EINVAL;
	}
	ret = stk3x1x_master_recv(client, obj->addr.flag, &buf, 0x01);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	} else {
		*data = buf;
	}

	if (atomic_read(&obj->trace) & STK_TRC_ALS_DATA) {
		APS_DBG("PS NF flag: 0x%04X\n", (u32) (*data));
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
int stk3x1x_read_id(struct i2c_client *client)
{
	int ret = 0;
	u8 buf[2];

	if (NULL == client) {
		return -EINVAL;
	}
	ret = stk3x1x_master_recv(client, STK_PDT_ID_REG, buf, 0x02);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}
	APS_LOG("%s: PID=0x%d, VID=0x%x\n", __func__, buf[0], buf[1]);

	if (buf[1] == 0xC0)
		APS_LOG("%s: RID=0xC0!!!!!!!!!!!!!\n", __func__);

	switch (buf[0]) {
	case STK2213_PID:
	case STK2213I_PID:
	case STK3010_PID:
	case STK3210_STK3310_PID:
	case STK3211_STK3311_PID:
		return 0;
	case 0x0:
		APS_ERR("PID=0x0, please make sure the chip is stk3x1x!\n");
		return -2;
	default:
		APS_ERR("%s: invalid PID(%#x)\n", __func__, buf[0]);
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

	if (NULL == client) {
		APS_ERR("i2c client is NULL\n");
		return -EINVAL;
	}
	ret = stk3x1x_master_recv(client, obj->addr.data1_ps, buf, 0x02);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	} else {
		if (((buf[0] << 8) | (buf[1])) < obj->ps_cali)
			*data = 0;
		else
			*data = ((buf[0] << 8) | (buf[1])) - obj->ps_cali;
	}

	if (atomic_read(&obj->trace) & STK_TRC_ALS_DATA) {
		APS_DBG("PS: 0x%04X\n", (u32) (*data));
	}

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
static int stk3x1x_write_sw_reset(struct i2c_client *client)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf = 0, r_buf = 0;
	int ret = 0;

	buf = 0x7F;
	ret = stk3x1x_master_send(client, obj->addr.wait, (char *)&buf, sizeof(buf));
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
		APS_ERR
		    ("i2c r/w test error, read-back value is not the same, write=0x%x, read=0x%x\n",
		     buf, r_buf);
		return -EIO;
	}

	buf = 0;
	ret = stk3x1x_master_send(client, obj->addr.soft_reset, (char *)&buf, sizeof(buf));
	if (ret < 0) {
		APS_ERR("write software reset error = %d\n", ret);
		return -EFAULT;
	}
	mdelay(1);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_write_ps_high_thd(struct i2c_client *client, u16 thd)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8) ((0xFF00 & thd) >> 8);
	buf[1] = (u8) (0x00FF & thd);
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
static int stk3x1x_write_ps_low_thd(struct i2c_client *client, u16 thd)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8) ((0xFF00 & thd) >> 8);
	buf[1] = (u8) (0x00FF & thd);
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
static int stk3x1x_write_als_high_thd(struct i2c_client *client, u16 thd)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8) ((0xFF00 & thd) >> 8);
	buf[1] = (u8) (0x00FF & thd);
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
static int stk3x1x_write_als_low_thd(struct i2c_client *client, u16 thd)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8) ((0xFF00 & thd) >> 8);
	buf[1] = (u8) (0x00FF & thd);
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
#if 0
int stk3x1x_write_foffset(struct i2c_client *client, u16 ofset)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;

	buf[0] = (u8) ((0xFF00 & ofset) >> 8);
	buf[1] = (u8) (0x00FF & ofset);
	ret = stk3x1x_master_send(client, obj->addr.data1_offset, &buf[0], 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_send(client, obj->addr.data2_offset, &(buf[1]), 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/

int stk3x1x_write_aoffset(struct i2c_client *client, u16 ofset)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	u8 buf[2];
	int ret = 0;
	u8 s_buf = 0, re_en;

	ret = stk3x1x_master_recv(client, obj->addr.state, &s_buf, 1);
	if (ret < 0) {
		APS_ERR("i2c read state error = %d\n", ret);
		return -EFAULT;
	}
	re_en = (s_buf & STK_STATE_EN_AK_MASK) ? 1 : 0;
	if (re_en) {
		s_buf &= (~STK_STATE_EN_AK_MASK);
		ret = stk3x1x_master_send(client, obj->addr.state, &s_buf, 1);
		if (ret < 0) {
			APS_ERR("write state = %d\n", ret);
			return -EFAULT;
		}
		msleep(3);
	}

	buf[0] = (u8) ((0xFF00 & ofset) >> 8);
	buf[1] = (u8) (0x00FF & ofset);
	ret = stk3x1x_master_send(client, 0x0E, &buf[0], 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}

	ret = stk3x1x_master_send(client, 0x0F, &(buf[1]), 1);
	if (ret < 0) {
		APS_ERR("WARNING: %s: %d\n", __func__, ret);
		return -EFAULT;
	}
	if (!re_en)
		return 0;
	s_buf |= STK_STATE_EN_AK_MASK;
	ret = stk3x1x_master_send(client, obj->addr.state, &s_buf, 1);
	if (ret < 0) {
		APS_ERR("write state = %d\n", ret);
		return -EFAULT;
	}
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static void stk3x1x_power(struct alsps_hw *hw, unsigned int on)
{
/*
	static unsigned int power_on;

	if (hw->power_id != POWER_NONE_MACRO) {
		if (power_on == on) {
			APS_LOG("ignore power control: %d\n", on);
		} else if (on) {
			if (!hwPowerOn(hw->power_id, hw->power_vol, "stk3x1x")) {
				APS_ERR("power on fails!!\n");
			}
		} else {
			if (!hwPowerDown(hw->power_id, "stk3x1x")) {
				APS_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
*/
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_enable_als(struct i2c_client *client, int enable)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int err, cur = 0;
	int old = atomic_read(&obj->state_val);
	int trc = atomic_read(&obj->trace);

	APS_LOG("%s: enable=%d\n", __func__, enable);
	cur = old & (~(STK_STATE_EN_ALS_MASK | STK_STATE_EN_WAIT_MASK));
	if (enable)
		cur |= STK_STATE_EN_ALS_MASK;
	else if (old & STK_STATE_EN_PS_MASK)
		cur |= STK_STATE_EN_WAIT_MASK;

	if (trc & STK_TRC_DEBUG)
		APS_LOG("%s: %08X, %08X, %d\n", __func__, cur, old, enable);

	if (0 == (cur ^ old))
		return 0;
#ifdef STK_IRS
	if (enable && !(old & STK_STATE_EN_PS_MASK)) {
		err = stk3x1x_get_ir_value(obj);
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
	else
		atomic_set(&obj->state_val, cur);

	if (enable) {
		if (obj->hw->polling_mode_als) {
			atomic_set(&obj->als_deb_on, 1);
			atomic_set(&obj->als_deb_end,
				   jiffies + atomic_read(&obj->als_debounce) * HZ / 1000);
		} else {
			schedule_delayed_work(&obj->eint_work, 220 * HZ / 1000);
		}
	}

	if (trc & STK_TRC_DEBUG)
		APS_LOG("enable als (%d)\n", enable);

	return err;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_enable_ps(struct i2c_client *client, int enable)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int err, cur = 0, old = atomic_read(&obj->state_val);
	int trc = atomic_read(&obj->trace);
	int int_flag = 0;

	cur = old;
	err = stk3x1x_write_ps_high_thd(client, atomic_read(&obj->ps_high_thd_val));
	if (err) {
		APS_ERR("write high thd error: %d\n", err);
		return err;
	}

	err = stk3x1x_write_ps_low_thd(client, atomic_read(&obj->ps_low_thd_val));
	if (err) {
		APS_ERR("write low thd error: %d\n", err);
		return err;
	}

	APS_LOG("%s: enable=%d\n", __func__, enable);
	cur &= (~(0x45));
	if (enable) {
		cur |= (STK_STATE_EN_PS_MASK);
		if (!(old & STK_STATE_EN_ALS_MASK))
			cur |= STK_STATE_EN_WAIT_MASK;
		if (1 == obj->hw->polling_mode_ps)
			wake_lock(&ps_lock);
	} else {
		if (1 == obj->hw->polling_mode_ps)
			wake_unlock(&ps_lock);
	}

	if (trc & STK_TRC_DEBUG) {
		APS_LOG("%s: %08X, %08X, %d\n", __func__, cur, old, enable);
	}

	if (0 == (cur ^ old)) {
		return 0;
	}

	err = stk3x1x_write_state(client, cur);
	if (err < 0)
		return err;
	else
		atomic_set(&obj->state_val, cur);

	if (enable) {
		if (obj->hw->polling_mode_ps) {
			atomic_set(&obj->ps_deb_on, 1);
			atomic_set(&obj->ps_deb_end,
				   jiffies + atomic_read(&obj->ps_debounce) * HZ / 1000);
		} else {
			msleep(4);
			err = stk3x1x_read_ps(obj->client, &obj->ps);
			if (err) {
				APS_ERR("stk3x1x read ps data: %d\n", err);
				return err;
			}

			err = stk3x1x_get_ps_value_only(obj, obj->ps);
			if (err < 0) {
				APS_ERR("stk3x1x get ps value: %d\n", err);
				return err;
			} else if (stk3x1x_obj->hw->polling_mode_ps == 0) {
				APS_LOG("%s:ps raw 0x%x -> value 0x%x\n", __func__, obj->ps,int_flag);
				if (ps_report_interrupt_data(int_flag))
					APS_ERR("call ps_report_interrupt_data fail\n");
			}
		}
	}

	if (trc & STK_TRC_DEBUG)
		APS_LOG("enable ps  (%d)\n", enable);

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
	APS_LOG("%s: read status reg: 0x%x\n", __func__, *status);

	if (*status & STK_FLG_ALSINT_MASK)
		set_bit(STK_BIT_ALS, &obj->pending_intr);
	else
		clear_bit(STK_BIT_ALS, &obj->pending_intr);

	if (*status & STK_FLG_PSINT_MASK)
		set_bit(STK_BIT_PS, &obj->pending_intr);
	else
		clear_bit(STK_BIT_PS, &obj->pending_intr);

	if (atomic_read(&obj->trace) & STK_TRC_DEBUG)
		APS_LOG("check intr: 0x%02X => 0x%08lX\n", *status, obj->pending_intr);

	return 0;
}


static int stk3x1x_clear_intr(struct i2c_client *client, u8 status, u8 disable_flag)
{
	int err = 0;

	status =
	    status | (STK_FLG_ALSINT_MASK | STK_FLG_PSINT_MASK | STK_FLG_OUI_MASK |
		      STK_FLG_IR_RDY_MASK);
	status &= (~disable_flag);
	APS_LOG(" set flag reg: 0x%x\n", status);
	err = stk3x1x_write_flag(client, status);
	if (err)
		APS_ERR("stk3x1x_write_flag failed, err=%d\n", err);
	return err;
}

/*----------------------------------------------------------------------------*/
/*static int stk3x1x_set_als_int_thd(struct i2c_client *client, u16 als_data_reg)
{
	s32 als_thd_h, als_thd_l;

	als_thd_h = als_data_reg + STK_ALS_CODE_CHANGE_THD;
	als_thd_l = als_data_reg - STK_ALS_CODE_CHANGE_THD;
	if (als_thd_h >= (1 << 16))
		als_thd_h = (1 << 16) - 1;
	if (als_thd_l < 0)
		als_thd_l = 0;
	APS_LOG("stk3x1x_set_als_int_thd:als_thd_h:%d,als_thd_l:%d\n", als_thd_h, als_thd_l);

	stk3x1x_write_als_high_thd(client, als_thd_h);
	stk3x1x_write_als_low_thd(client, als_thd_l);

	return 0;
}
*/
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
static int alsps_irq_handler(void *data, uint len)
{
	struct stk3x1x_priv *obj = stk3x1x_obj;
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;

	if (!obj)
		return -1;

	APS_ERR("len = %d, type = %d, sction = %d, event = %d, data = %d\n", len,
		rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode, rsp->notify_rsp.data[1]);

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_INIT_DONE:
			schedule_work(&obj->init_done_work);
			break;
		case SCP_NOTIFY:
			if (STK3X1X_NOTIFY_PROXIMITY_CHANGE == rsp->notify_rsp.data[0]) {
				gRawData.ps_state = rsp->notify_rsp.data[1];
				schedule_work(&obj->eint_work);
			} else if (STK3X1X_NOTIFY_PROXIMITY_NOT_CHANGE == rsp->notify_rsp.data[0]) {
				gRawData.ps_state = rsp->notify_rsp.data[1];
				schedule_work(&obj->data_work);
			} else {
				APS_ERR("Unknown notify\n");
			}
			break;
		default:
			APS_ERR("Error sensor hub notify\n");
			break;
		}
		break;
	default:
		APS_ERR("Error sensor hub action\n");
		break;
	}

	return 0;
}
#else
static irqreturn_t stk3x1x_eint_func(int irq, void *dev_id)
{
	struct stk3x1x_priv *obj = g_stk3x1x_ptr;

	int_top_time = sched_clock();

	if (!obj)
		return IRQ_HANDLED;

	disable_irq_nosync(stk3x1x_obj->irq);
	if (obj->hw->polling_mode_ps == 0 || obj->hw->polling_mode_als == 0)
		schedule_delayed_work(&obj->eint_work, 0);
	if (atomic_read(&obj->trace) & STK_TRC_EINT) {
		APS_LOG("eint: als/ps intrs\n");
	}

	return IRQ_HANDLED;
}
#endif

/*----------------------------------------------------------------------------*/
static void stk3x1x_eint_work(struct work_struct *work)
{
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	int res = 0;

	res = ps_report_interrupt_data(gRawData.ps_state);
	if (res != 0)
		APS_ERR("stk3x1x_eint_work err: %d\n", res);
	return res;
#else
	struct stk3x1x_priv *obj = g_stk3x1x_ptr;
	int err;
	int int_flag = 0;
	u8 flag_reg, disable_flag = 0;

	APS_LOG("stk3x1x int top half time = %lld\n", int_top_time);

	err = stk3x1x_check_intr(obj->client, &flag_reg);
	if (err) {
		APS_ERR("stk3x1x_check_intr fail: %d\n", err);
		goto err_i2c_rw;
	}

	APS_LOG("obj->pending_intr =%lx\n", obj->pending_intr);

	if (((1 << STK_BIT_PS) & obj->pending_intr) && (obj->hw->polling_mode_ps == 0)) {
		APS_LOG("stk ps change\n");
		disable_flag |= STK_FLG_PSINT_MASK;

		err = stk3x1x_read_ps(obj->client, &obj->ps);
		if (err) {
			APS_ERR("stk3x1x read ps data: %d\n", err);
			goto err_i2c_rw;
		}

		int_flag = (flag_reg & STK_FLG_NF_MASK) ? 1 : 0;
		/* let up layer to know */
		if (ps_report_interrupt_data(int_flag)) {
			APS_ERR("call ps_report_interrupt_data fail\n");
		}
	}

	err = stk3x1x_clear_intr(obj->client, flag_reg, disable_flag);
	if (err) {
		APS_ERR("fail: %d\n", err);
		goto err_i2c_rw;
	}

	mdelay(1);
	enable_irq(stk3x1x_obj->irq);
	return;

err_i2c_rw:
	msleep(30);
	enable_irq(stk3x1x_obj->irq);
	return;
#endif

}

/*----------------------------------------------------------------------------*/
static int stk3x1x_setup_eint(struct i2c_client *client)
{
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	int err = 0;

	err = SCP_sensorHub_rsp_registration(ID_PROXIMITY, alsps_irq_handler);
#else
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int ret;
	u32 ints[2] = { 0, 0 };
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;
	struct platform_device *alsps_pdev = get_alsps_platformdev();

	APS_LOG("stk3x1x_setup_eint\n");

	g_stk3x1x_ptr = obj;

	/*configure to GPIO function, external interrupt */
	pinctrl = devm_pinctrl_get(&alsps_pdev->dev);
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
	pinctrl_select_state(pinctrl, pins_cfg);

	if (stk3x1x_obj->irq_node) {
		of_property_read_u32_array(stk3x1x_obj->irq_node, "debounce", ints,
					   ARRAY_SIZE(ints));
		gpio_request(ints[0], "p-sensor");
		gpio_set_debounce(ints[0], ints[1]);
		APS_LOG("ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);

		stk3x1x_obj->irq = irq_of_parse_and_map(stk3x1x_obj->irq_node, 0);
		APS_LOG("stk3x1x_obj->irq = %d\n", stk3x1x_obj->irq);
		if (!stk3x1x_obj->irq) {
			APS_ERR("irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}
		if (request_irq
		    (stk3x1x_obj->irq, stk3x1x_eint_func, IRQF_TRIGGER_NONE, "ALS-eint", NULL)) {
			APS_ERR("IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}
		/*enable_irq(stk3x1x_obj->irq);*/
	} else {
		APS_ERR("null irq node!!\n");
		return -EINVAL;
	}

	/*enable_irq(stk3x1x_obj->irq);*/
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_init_client(struct i2c_client *client)
{
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);
	int err;
	int ps_ctrl;
	/* u8 int_status; */

	err = stk3x1x_write_sw_reset(client);
	if (err) {
		APS_ERR("software reset error, err=%d", err);
		return err;
	}
/*
	if((err = stk3x1x_read_id(client)))
	{
		APS_ERR("stk3x1x_read_id error, err=%d", err);
		return err;
	}
*/
	if (obj->hw->polling_mode_ps == 0 || obj->hw->polling_mode_als == 0) {
		/*disable_irq(stk3x1x_obj->irq);*/
		err = stk3x1x_setup_eint(client);
		if (err) {
			APS_ERR("setup eint error: %d\n", err);
			return err;
		}
	}

	err = stk3x1x_write_state(client, atomic_read(&obj->state_val));
	if (err) {
		APS_ERR("write stete error: %d\n", err);
		return err;
	}

	/*
	   if((err = stk3x1x_check_intr(client, &int_status)))
	   {
	   APS_ERR("check intr error: %d\n", err);
	   //    return err;
	   }

	   if((err = stk3x1x_clear_intr(client, int_status, STK_FLG_PSINT_MASK | STK_FLG_ALSINT_MASK)))
	   {
	   APS_ERR("clear intr error: %d\n", err);
	   return err;
	   }
	 */
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
#ifdef STK_FIR
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif
	return 0;
}

/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
static ssize_t stk3x1x_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	res = scnprintf(buf, PAGE_SIZE, "(%d %d %d %d %d %d)\n",
			atomic_read(&stk3x1x_obj->i2c_retry),
			atomic_read(&stk3x1x_obj->als_debounce), atomic_read(&stk3x1x_obj->ps_mask),
			atomic_read(&stk3x1x_obj->ps_high_thd_val),
			atomic_read(&stk3x1x_obj->ps_low_thd_val),
			atomic_read(&stk3x1x_obj->ps_debounce));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_store_config(struct device_driver *ddri, const char *buf, size_t count)
{
	int retry, als_deb, ps_deb, mask, hthres, lthres, err;
	struct i2c_client *client;

	client = stk3x1x_i2c_client;
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	if (6 ==
	    sscanf(buf, "%d %d %d %d %d %d", &retry, &als_deb, &mask, &hthres, &lthres, &ps_deb)) {
		atomic_set(&stk3x1x_obj->i2c_retry, retry);
		atomic_set(&stk3x1x_obj->als_debounce, als_deb);
		atomic_set(&stk3x1x_obj->ps_mask, mask);
		atomic_set(&stk3x1x_obj->ps_high_thd_val, hthres);
		atomic_set(&stk3x1x_obj->ps_low_thd_val, lthres);
		atomic_set(&stk3x1x_obj->ps_debounce, ps_deb);

		err = stk3x1x_write_ps_high_thd(client,atomic_read(&stk3x1x_obj->ps_high_thd_val));
		if (err) {
			APS_ERR("write high thd error: %d\n", err);
			return err;
		}

		err = stk3x1x_write_ps_low_thd(client, atomic_read(&stk3x1x_obj->ps_low_thd_val));
		if (err) {
			APS_ERR("write low thd error: %d\n", err);
			return err;
		}
	} else {
		APS_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	res = scnprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&stk3x1x_obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_store_trace(struct device_driver *ddri, const char *buf, size_t count)
{
	int trace;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&stk3x1x_obj->trace, trace);
	} else {
		APS_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_ir(struct device_driver *ddri, char *buf)
{
	int32_t reading;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}
	reading = stk3x1x_get_ir_value(stk3x1x_obj);
	if (reading < 0)
		return scnprintf(buf, PAGE_SIZE, "ERROR: %d\n", reading);

	stk3x1x_obj->ir_code = reading;
	return scnprintf(buf, PAGE_SIZE, "0x%04X\n", stk3x1x_obj->ir_code);
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_als(struct device_driver *ddri, char *buf)
{
	int res;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}
	if ((res = stk3x1x_read_als(stk3x1x_obj->client, &stk3x1x_obj->als))) {
		return scnprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	} else {
		return scnprintf(buf, PAGE_SIZE, "0x%04X\n", stk3x1x_obj->als);
	}
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_ps(struct device_driver *ddri, char *buf)
{
	int res;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	if ((res = stk3x1x_read_ps(stk3x1x_obj->client, &stk3x1x_obj->ps))) {
		return scnprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	} else {
		return scnprintf(buf, PAGE_SIZE, "0x%04X\n", stk3x1x_obj->ps);
	}
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_reg(struct device_driver *ddri, char *buf)
{
	u8 int_status;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	/*read */
	stk3x1x_check_intr(stk3x1x_obj->client, &int_status);
	/* stk3x1x_clear_intr(stk3x1x_obj->client, int_status, 0x0); */
	stk3x1x_read_ps(stk3x1x_obj->client, &stk3x1x_obj->ps);
	stk3x1x_read_als(stk3x1x_obj->client, &stk3x1x_obj->als);
	/*write */
	stk3x1x_write_als(stk3x1x_obj->client, atomic_read(&stk3x1x_obj->alsctrl_val));
	stk3x1x_write_ps(stk3x1x_obj->client, atomic_read(&stk3x1x_obj->psctrl_val));
	stk3x1x_write_ps_high_thd(stk3x1x_obj->client, atomic_read(&stk3x1x_obj->ps_high_thd_val));
	stk3x1x_write_ps_low_thd(stk3x1x_obj->client, atomic_read(&stk3x1x_obj->ps_low_thd_val));
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_send(struct device_driver *ddri, char *buf)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_store_send(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr, cmd;
	u8 dat;
	int ret = 0;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	} else if (2 != sscanf(buf, "%x %x", &addr, &cmd)) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8) cmd;
	ret = stk3x1x_master_send(stk3x1x_obj->client, (u16) addr, &dat, sizeof(dat));
	APS_LOG("send(%02X, %02X) = %d\n", addr, cmd, ret);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_recv(struct device_driver *ddri, char *buf)
{
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}
	return scnprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&stk3x1x_obj->recv_reg));
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_store_recv(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr;
	u8 dat;
	int ret = 0;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	} else if (1 != sscanf(buf, "%x", &addr)) {
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}
	ret = stk3x1x_master_recv(stk3x1x_obj->client, (u16) addr, (char *)&dat, sizeof(dat));
	APS_LOG("recv(%02X) = %d, 0x%02X\n", addr, ret, dat);
	atomic_set(&stk3x1x_obj->recv_reg, dat);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_allreg(struct device_driver *ddri, char *buf)
{
	int ret = 0;
	u8 rbuf[27];
	int cnt;

	memset(rbuf, 0, sizeof(rbuf));
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 0, &rbuf[0], 7);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 7, &rbuf[7], 7);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 14, &rbuf[14], 7);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 21, &rbuf[21], 4);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}
	ret = stk3x1x_master_recv(stk3x1x_obj->client, STK_PDT_ID_REG, &rbuf[25], 2);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}

	for (cnt = 0; cnt < 25; cnt++) {
		APS_LOG("reg[0x%x]=0x%x\n", cnt, rbuf[cnt]);
	}
	APS_LOG("reg[0x3E]=0x%x\n", rbuf[cnt]);
	APS_LOG("reg[0x3F]=0x%x\n", rbuf[cnt++]);
	return scnprintf(buf, PAGE_SIZE,
			 "[0]%2X [1]%2X [2]%2X [3]%2X [4]%2X [5]%2X [6/7 HTHD]%2X,%2X [8/9 LTHD]%2X, %2X [A]%2X [B]%2X [C]%2X [D]%2X [E/F Aoff]%2X,%2X,[10]%2X [11/12 PS]%2X,%2X [13]%2X [14]%2X [15/16 Foff]%2X,%2X [17]%2X [18]%2X [3E]%2X [3F]%2X\n",
			 rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7],
			 rbuf[8], rbuf[9], rbuf[10], rbuf[11], rbuf[12], rbuf[13], rbuf[14],
			 rbuf[15], rbuf[16], rbuf[17], rbuf[18], rbuf[19], rbuf[20], rbuf[21],
			 rbuf[22], rbuf[23], rbuf[24], rbuf[25], rbuf[26]);
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	u8 rbuf[25];
	int ret = 0;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	if (stk3x1x_obj->hw) {
		len +=
		    scnprintf(buf + len, PAGE_SIZE - len,
			      "CUST: %d, (%d %d) (%02X) (%02X %02X %02X) (%02X %02X %02X %02X)\n",
			      stk3x1x_obj->hw->i2c_num, stk3x1x_obj->hw->power_id,
			      stk3x1x_obj->hw->power_vol, stk3x1x_obj->addr.flag,
			      stk3x1x_obj->addr.alsctrl, stk3x1x_obj->addr.data1_als,
			      stk3x1x_obj->addr.data2_als, stk3x1x_obj->addr.psctrl,
			      stk3x1x_obj->addr.data1_ps, stk3x1x_obj->addr.data2_ps,
			      stk3x1x_obj->addr.thdh1_ps);
	} else {
		len += scnprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}

	len +=
	    scnprintf(buf + len, PAGE_SIZE - len,
		      "REGS: %02X %02X %02X %02X %02X %02X %02X %02X %02lX %02lX\n",
		      atomic_read(&stk3x1x_obj->state_val), atomic_read(&stk3x1x_obj->psctrl_val),
		      atomic_read(&stk3x1x_obj->alsctrl_val), stk3x1x_obj->ledctrl_val,
		      stk3x1x_obj->int_val, stk3x1x_obj->wait_val,
		      atomic_read(&stk3x1x_obj->ps_high_thd_val),
		      atomic_read(&stk3x1x_obj->ps_low_thd_val), stk3x1x_obj->enable,
		      stk3x1x_obj->pending_intr);

	len +=
	    scnprintf(buf + len, PAGE_SIZE - len, "MISC: %d %d\n",
		      atomic_read(&stk3x1x_obj->als_suspend),
		      atomic_read(&stk3x1x_obj->ps_suspend));
	len += scnprintf(buf + len, PAGE_SIZE - len, "VER.: %s\n", DRIVER_VERSION);

	memset(rbuf, 0, sizeof(rbuf));
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 0, &rbuf[0], 7);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 7, &rbuf[7], 7);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}
	ret = stk3x1x_master_recv(stk3x1x_obj->client, 14, &rbuf[14], 7);
	if (ret < 0) {
		APS_DBG("error: %d\n", ret);
		return -EFAULT;
	}
	/*
	   ret = stk3x1x_master_recv(stk3x1x_obj->client, 21, &rbuf[21], 4);
	   if(ret < 0)
	   {
	   APS_DBG("error: %d\n", ret);
	   return -EFAULT;
	   }
	 */
	len +=
	    scnprintf(buf + len, PAGE_SIZE - len,
		      "[PS=%2X] [ALS=%2X] [WAIT=%4Xms] [EN_ASO=%2X] [EN_AK=%2X] [NEAR/FAR=%2X] [FLAG_OUI=%2X] [FLAG_PSINT=%2X] [FLAG_ALSINT=%2X]\n",
		      rbuf[0] & 0x01, (rbuf[0] & 0x02) >> 1, ((rbuf[0] & 0x04) >> 2) * rbuf[5] * 6,
		      (rbuf[0] & 0x20) >> 5, (rbuf[0] & 0x40) >> 6, rbuf[16] & 0x01,
		      (rbuf[16] & 0x04) >> 2, (rbuf[16] & 0x10) >> 4, (rbuf[16] & 0x20) >> 5);

	return len;
}

/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct stk3x1x_priv *obj, const char *buf, size_t count,
			     u32 data[], int len)
{
	int idx = 0;
	char *cur = (char *)buf, *end = (char *)(buf + count);

	while (idx < len) {
		while ((cur < end) && IS_SPACE(*cur)) {
			cur++;
		}

		if (1 != sscanf(cur, "%d", &data[idx])) {
			break;
		}

		idx++;
		while ((cur < end) && !IS_SPACE(*cur)) {
			cur++;
		}
	}
	return idx;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < stk3x1x_obj->als_level_num; idx++) {
		len +=
		    scnprintf(buf + len, PAGE_SIZE - len, "%d ", stk3x1x_obj->hw->als_level[idx]);
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_store_alslv(struct device_driver *ddri, const char *buf, size_t count)
{
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def")) {
		memcpy(stk3x1x_obj->als_level, stk3x1x_obj->hw->als_level,
		       sizeof(stk3x1x_obj->als_level));
	} else if (stk3x1x_obj->als_level_num !=
		   read_int_from_buf(stk3x1x_obj, buf, count, stk3x1x_obj->hw->als_level,
				     stk3x1x_obj->als_level_num)) {
		APS_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	}

	for (idx = 0; idx < stk3x1x_obj->als_value_num; idx++) {
		len +=
		    scnprintf(buf + len, PAGE_SIZE - len, "%d ", stk3x1x_obj->hw->als_value[idx]);
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t stk3x1x_store_alsval(struct device_driver *ddri, const char *buf, size_t count)
{
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return 0;
	} else if (!strcmp(buf, "def")) {
		memcpy(stk3x1x_obj->als_value, stk3x1x_obj->hw->als_value,
		       sizeof(stk3x1x_obj->als_value));
	} else if (stk3x1x_obj->als_value_num !=
		   read_int_from_buf(stk3x1x_obj, buf, count, stk3x1x_obj->hw->als_value,
				     stk3x1x_obj->als_value_num)) {
		APS_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(als, S_IWUSR | S_IRUGO, stk3x1x_show_als, NULL);
static DRIVER_ATTR(ps, S_IWUSR | S_IRUGO, stk3x1x_show_ps, NULL);
static DRIVER_ATTR(ir, S_IWUSR | S_IRUGO, stk3x1x_show_ir, NULL);
static DRIVER_ATTR(config, S_IWUSR | S_IRUGO, stk3x1x_show_config, stk3x1x_store_config);
static DRIVER_ATTR(alslv, S_IWUSR | S_IRUGO, stk3x1x_show_alslv, stk3x1x_store_alslv);
static DRIVER_ATTR(alsval, S_IWUSR | S_IRUGO, stk3x1x_show_alsval, stk3x1x_store_alsval);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, stk3x1x_show_trace, stk3x1x_store_trace);
static DRIVER_ATTR(status, S_IWUSR | S_IRUGO, stk3x1x_show_status, NULL);
static DRIVER_ATTR(send, S_IWUSR | S_IRUGO, stk3x1x_show_send, stk3x1x_store_send);
static DRIVER_ATTR(recv, S_IWUSR | S_IRUGO, stk3x1x_show_recv, stk3x1x_store_recv);
static DRIVER_ATTR(reg, S_IWUSR | S_IRUGO, stk3x1x_show_reg, NULL);
static DRIVER_ATTR(allreg, S_IWUSR | S_IRUGO, stk3x1x_show_allreg, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *stk3x1x_attr_list[] = {
	&driver_attr_als,
	&driver_attr_ps,
	&driver_attr_ir,
	&driver_attr_trace,	/*trace log */
	&driver_attr_config,
	&driver_attr_alslv,
	&driver_attr_alsval,
	&driver_attr_status,
	&driver_attr_send,
	&driver_attr_recv,
	&driver_attr_allreg,
/* &driver_attr_i2c, */
	&driver_attr_reg,
};

/*----------------------------------------------------------------------------*/
static int stk3x1x_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(stk3x1x_attr_list) / sizeof(stk3x1x_attr_list[0]));

	if (driver == NULL) {
		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, stk3x1x_attr_list[idx]);
		if (err) {
			APS_ERR("create attr fail(%s) = %d\n", stk3x1x_attr_list[idx]->attr.name,err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(stk3x1x_attr_list) / sizeof(stk3x1x_attr_list[0]));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		driver_remove_file(driver, stk3x1x_attr_list[idx]);
	}

	return err;
}

/******************************************************************************
 * Function Configuration
******************************************************************************/
static int stk3x1x_get_als_value(struct stk3x1x_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;

	for (idx = 0; idx < obj->als_level_num; idx++) {
		if (als < obj->hw->als_level[idx]) {
			break;
		}
	}

	if (idx >= obj->als_value_num) {
		APS_ERR("exceed range\n");
		idx = obj->als_value_num - 1;
	}

	if (1 == atomic_read(&obj->als_deb_on)) {
		unsigned long endt = atomic_read(&obj->als_deb_end);

		if (time_after(jiffies, endt)) {
			atomic_set(&obj->als_deb_on, 0);
		}

		if (1 == atomic_read(&obj->als_deb_on)) {
			invalid = 1;
		}
	}

	if (!invalid) {
#if defined(CONFIG_MTK_AAL_SUPPORT)
		int level_high = obj->hw->als_level[idx];
		int level_low = (idx > 0) ? obj->hw->als_level[idx - 1] : 0;
		int level_diff = level_high - level_low;
		int value_high = obj->hw->als_value[idx];
		int value_low = (idx > 0) ? obj->hw->als_value[idx - 1] : 0;
		int value_diff = value_high - value_low;
		int value = 0;

		if ((level_low >= level_high) || (value_low >= value_high))
			value = value_low;
		else
			value =
			    (level_diff * value_low + (als - level_low) * value_diff +
			     ((level_diff + 1) >> 1)) / level_diff;
		APS_DBG("ALS: %d [%d, %d] => %d [%d, %d]\n", als, level_low, level_high, value,
			value_low, value_high);
		return value;
#endif

		if (atomic_read(&obj->trace) & STK_TRC_CVT_ALS) {
			APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
		}

		return obj->hw->als_value[idx];
	} else {
		if (atomic_read(&obj->trace) & STK_TRC_CVT_ALS) {
			APS_DBG("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);
		}
		return -1;
	}
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
	} else if (1 == atomic_read(&obj->ps_deb_on)) {
		unsigned long endt = atomic_read(&obj->ps_deb_end);

		if (time_after(jiffies, endt)) {
			atomic_set(&obj->ps_deb_on, 0);
		}

		if (1 == atomic_read(&obj->ps_deb_on)) {
			invalid = 1;
		}
	}

	if (!invalid) {
		if (unlikely(atomic_read(&obj->trace) & STK_TRC_CVT_PS)) {
			if (mask) {
				APS_DBG("PS:  %05d => %05d [M]\n", ps, val);
			} else {
				APS_DBG("PS:  %05d => %05d\n", ps, val);
			}
		}
		return val;

	} else {
		APS_ERR(" ps value is invalid, PS:  %05d => %05d\n", ps, val);
		if (unlikely(atomic_read(&obj->trace) & STK_TRC_CVT_PS)) {
			APS_DBG("PS:  %05d => %05d (-1)\n", ps, val);
		}
		return -1;
	}
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
	err = stk3x1x_clear_intr(obj->client, flag, STK_FLG_OUI_MASK);
	if (err) {
		APS_ERR("fail: %d\n", err);
		return err;
	}

	if (atomic_read(&obj->ps_suspend)) {
		invalid = 1;
	} else if (1 == atomic_read(&obj->ps_deb_on)) {
		unsigned long endt = atomic_read(&obj->ps_deb_end);

		if (time_after(jiffies, endt)) {
			atomic_set(&obj->ps_deb_on, 0);
		}

		if (1 == atomic_read(&obj->ps_deb_on)) {
			invalid = 1;
		}
	}


	if (!invalid) {
		if (unlikely(atomic_read(&obj->trace) & STK_TRC_CVT_PS)) {
			if (mask) {
				APS_DBG("PS:  %05d => %05d [M]\n", ps, val);
			} else {
				APS_DBG("PS:  %05d => %05d\n", ps, val);
			}
		}
		return val;

	} else {
		APS_ERR(" ps value is invalid, PS:  %05d => %05d\n", ps, val);
		if (unlikely(atomic_read(&obj->trace) & STK_TRC_CVT_PS)) {
			APS_DBG("PS:  %05d => %05d (-1)\n", ps, val);
		}
		return -1;
	}
}

/*----------------------------------------------------------------------------*/

static int32_t stk3x1x_set_irs_it_slp(struct stk3x1x_priv *obj, uint16_t *slp_time)
{
	uint8_t irs_alsctrl;
	int32_t ret;

	irs_alsctrl = (atomic_read(&obj->alsctrl_val) & 0x0F) - 2;
	switch (irs_alsctrl) {
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
	default:
		APS_ERR("%s: unknown ALS IT=0x%x\n", __func__, irs_alsctrl);
		ret = -EINVAL;
		return ret;
	}
	irs_alsctrl |= (atomic_read(&obj->alsctrl_val) & 0xF0);
	ret = i2c_smbus_write_byte_data(obj->client, STK_ALSCTRL_REG, irs_alsctrl);
	if (ret < 0) {
		APS_ERR("%s: write i2c error\n", __func__);
		return ret;
	}
	return 0;
}

static int32_t stk3x1x_get_ir_value(struct stk3x1x_priv *obj)
{
	int32_t word_data, ret;
	uint8_t w_reg, retry = 0;
	uint16_t irs_slp_time = 100;
	bool re_enable_ps = false;
	u8 flag;
	u8 buf[2];

	re_enable_ps = (atomic_read(&obj->state_val) & STK_STATE_EN_PS_MASK) ? true : false;
	if (re_enable_ps) {
		stk3x1x_enable_ps(obj->client, 0);
	}

	ret = stk3x1x_set_irs_it_slp(obj, &irs_slp_time);
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
		msleep(3);
		ret = stk3x1x_read_flag(obj->client, &flag);
		if (ret < 0) {
			APS_ERR("WARNING: read flag reg error: %d\n", ret);
			goto irs_err_i2c_rw;
		}
		retry++;
	} while (retry < 10 && ((flag & STK_FLG_IR_RDY_MASK) == 0));

	if (retry == 10) {
		APS_ERR("%s: ir data is not ready for 300ms\n", __func__);
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

	ret =
	    i2c_smbus_write_byte_data(obj->client, STK_ALSCTRL_REG, atomic_read(&obj->alsctrl_val));
	if (ret < 0) {
		APS_ERR("%s: write i2c error\n", __func__);
		goto irs_err_i2c_rw;
	}
	if (re_enable_ps)
		stk3x1x_enable_ps(obj->client, 1);
	return word_data;

irs_err_i2c_rw:
	if (re_enable_ps)
		stk3x1x_enable_ps(obj->client, 1);
	return ret;
}

/******************************************************************************
 * Function Configuration
******************************************************************************/
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
static long stk3x1x_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	void __user *ptr = (void __user *)arg;
	int dat;
	uint32_t enable;
	int ps_result;
	int ps_cali;
	int threshold[2];
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct stk3x1x_priv *obj = i2c_get_clientdata(client);


	switch (cmd) {
	case ALSPS_SET_PS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}
		if (enable) {
			err = stk3x1x_enable_ps(obj->client, 1);
			if (err) {
				APS_ERR("enable ps fail: %d\n", err);
				goto err_out;
			}

			set_bit(STK_BIT_PS, &obj->enable);
		} else {
			err = stk3x1x_enable_ps(obj->client, 0);
			if (err) {
				APS_ERR("disable ps fail: %d\n", err);
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
#ifdef STK_PS_POLLING_LOG
		APS_LOG("%s:ps raw 0x%x -> value 0x%x\n", __func__, obj->ps, dat);
#endif
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
				APS_ERR("enable als fail: %d\n", err);
				goto err_out;
			}
			set_bit(STK_BIT_ALS, &obj->enable);
		} else {
			err = stk3x1x_enable_als(obj->client, 0);
			if (err) {
				APS_ERR("disable als fail: %d\n", err);
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

		dat = stk3x1x_get_als_value(obj, obj->als);
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
			/*----------------------------------for factory mode test---------------------------------------*/
	case ALSPS_GET_PS_TEST_RESULT:
		err = stk3x1x_read_ps(obj->client, &obj->ps);
		if (err)
			goto err_out;

		if (obj->ps > atomic_read(&obj->ps_high_thd_val))
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
		if (copy_from_user(threshold, ptr, sizeof(threshold))) {
			err = -EFAULT;
			goto err_out;
		}
		APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n", __func__, threshold[0],
			threshold[1]);
		atomic_set(&obj->ps_high_thd_val, (threshold[0] + obj->ps_cali));
		atomic_set(&obj->ps_low_thd_val, (threshold[1] + obj->ps_cali));	/* need to confirm */

		err = stk3x1x_write_ps_high_thd(obj->client, atomic_read(&obj->ps_high_thd_val));
		if (err) {
			APS_ERR("write high thd error: %d\n", err);
			goto err_out;
		}
		err = stk3x1x_write_ps_low_thd(obj->client, atomic_read(&obj->ps_low_thd_val));
		if (err) {
			APS_ERR("write low thd error: %d\n", err);
			goto err_out;
		}

		break;

	case ALSPS_GET_PS_THRESHOLD_HIGH:
		threshold[0] = atomic_read(&obj->ps_high_thd_val) - obj->ps_cali;
		APS_ERR("%s get threshold high: 0x%x\n", __func__, threshold[0]);
		err = copy_to_user(ptr, &threshold[0], sizeof(threshold[0]));
		if (err) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	case ALSPS_GET_PS_THRESHOLD_LOW:
		threshold[0] = atomic_read(&obj->ps_low_thd_val) - obj->ps_cali;
		APS_ERR("%s get threshold low: 0x%x\n", __func__, threshold[0]);
		err = copy_to_user(ptr, &threshold[0], sizeof(threshold[0]));
		if (err) {
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
static int stk3x1x_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	APS_FUN();

	return 0;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_i2c_resume(struct i2c_client *client)
{
	APS_FUN();

	return 0;
}

/*----------------------------------------------------------------------------*/
/*
static void stk3x1x_early_suspend(struct early_suspend *h)
{
	int err;
	struct stk3x1x_priv *obj = container_of(h, struct stk3x1x_priv, early_drv);
	int old = atomic_read(&obj->state_val);

	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return;
	}

	if (old & STK_STATE_EN_ALS_MASK) {
		atomic_set(&obj->als_suspend, 1);
		if ((err = stk3x1x_enable_als(obj->client, 0))) {
			APS_ERR("disable als fail: %d\n", err);
		}
	}
}

static void stk3x1x_late_resume(struct early_suspend *h)
{
	int err;
	hwm_sensor_data sensor_data;
	struct stk3x1x_priv *obj = container_of(h, struct stk3x1x_priv, early_drv);

	memset(&sensor_data, 0, sizeof(sensor_data));
	APS_FUN();

	if (!obj) {
		APS_ERR("null pointer!!\n");
		return;
	}
	if (atomic_read(&obj->als_suspend)) {
		atomic_set(&obj->als_suspend, 0);
		if (test_bit(STK_BIT_ALS, &obj->enable)) {
			if ((err = stk3x1x_enable_als(obj->client, 1))) {
				APS_ERR("enable als fail: %d\n", err);

			}
		}
	}
}
*/

/*
int stk3x1x_ps_operate(void *self, uint32_t command, void *buff_in, int size_in,
		       void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data *sensor_data;
	struct stk3x1x_priv *obj = (struct stk3x1x_priv *)self;

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			APS_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			APS_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value) {
				if ((err = stk3x1x_enable_ps(obj->client, 1))) {
					APS_ERR("enable ps fail: %d\n", err);
					return -1;
				}
				set_bit(STK_BIT_PS, &obj->enable);
			} else {
				if ((err = stk3x1x_enable_ps(obj->client, 0))) {
					APS_ERR("disable ps fail: %d\n", err);
					return -1;
				}
				clear_bit(STK_BIT_PS, &obj->enable);
			}
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			APS_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			sensor_data = (hwm_sensor_data *) buff_out;

			if ((err = stk3x1x_read_ps(obj->client, &obj->ps))) {
				err = -1;
			} else {
				value = stk3x1x_get_ps_value(obj, obj->ps);
				if (value < 0) {
					err = -1;
				} else {
					sensor_data->values[0] = value;
					sensor_data->value_divide = 1;
					sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
#ifdef STK_PS_POLLING_LOG
					APS_LOG("%s:ps raw 0x%x -> value 0x%x\n", __func__,
						obj->ps, sensor_data->values[0]);
#endif
				}
			}
		}
		break;
	default:
		APS_ERR("proximity sensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
*/
/*
int stk3x1x_als_operate(void *self, uint32_t command, void *buff_in, int size_in,
			void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data *sensor_data;
	struct stk3x1x_priv *obj = (struct stk3x1x_priv *)self;
	u8 flag;

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			APS_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			APS_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value) {
				if ((err = stk3x1x_enable_als(obj->client, 1))) {
					APS_ERR("enable als fail: %d\n", err);
					return -1;
				}
				set_bit(STK_BIT_ALS, &obj->enable);
			} else {
				if ((err = stk3x1x_enable_als(obj->client, 0))) {
					APS_ERR("disable als fail: %d\n", err);
					return -1;
				}
				clear_bit(STK_BIT_ALS, &obj->enable);
			}

		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			APS_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			err = stk3x1x_read_flag(obj->client, &flag);
			if (err)
				return err;

			if (!(flag & STK_FLG_ALSDR_MASK))
				return -1;

			sensor_data = (hwm_sensor_data *) buff_out;
			if ((err = stk3x1x_read_als(obj->client, &obj->als))) {
				err = -1;
			} else {
				sensor_data->values[0] = stk3x1x_get_als_value(obj, obj->als);
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
		}
		break;
	default:
		APS_ERR("light sensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
*/
static int als_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
static int als_enable_nodata(int en)
{
	int res = 0;
	SCP_SENSOR_HUB_DATA req;
	int len;

	APS_LOG("stk3x1x_obj als enable value = %d\n", en);

	req.activate_req.sensorType = ID_LIGHT;
	req.activate_req.action = SENSOR_HUB_ACTIVATE;
	req.activate_req.enable = en;
	len = sizeof(req.activate_req);
	res = SCP_sensorHub_req_send(&req, &len, 1);
	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;
}
#else
static int als_enable_nodata(int en)
{
	int res = 0;

	APS_LOG("stk3x1x_obj als enable value = %d\n", en);
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return -1;
	}
	res = stk3x1x_enable_als(stk3x1x_obj->client, en);
	if (res) {
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;
}
#endif
static int als_set_delay(u64 ns)
{
	return 0;
}

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
static int als_get_data(int *value, int *status)
{
	int err = 0;
	SCP_SENSOR_HUB_DATA req;
	int len;

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

	if (atomic_read(&stk3x1x_obj->trace) & CMC_TRC_PS_DATA)
		APS_LOG("value = %d\n", *value);

	return err;
}

#else
static int als_get_data(int *value, int *status)
{
	int err = 0;
	struct stk3x1x_priv *obj = NULL;

	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return -1;
	}
	obj = stk3x1x_obj;
	err = stk3x1x_read_als(obj->client, &obj->als);
	if (err) {
		err = -1;
	} else {
		*value = stk3x1x_get_als_value(obj, obj->als);
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return err;
}
#endif

static int ps_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , sensor only enabled but not report inputEvent to HAL */
static int ps_enable_nodata(int en)
{
	int res = 0;
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif

	APS_LOG("stk3x1x_obj als enable value = %d\n", en);

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	req.activate_req.sensorType = ID_PROXIMITY;
	req.activate_req.action = SENSOR_HUB_ACTIVATE;
	req.activate_req.enable = en;
	len = sizeof(req.activate_req);
	res = SCP_sensorHub_req_send(&req, &len, 1);
#else
	if (!stk3x1x_obj) {
		APS_ERR("stk3x1x_obj is null!!\n");
		return -1;
	}
	res = stk3x1x_enable_ps(stk3x1x_obj->client, en);
#endif

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
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#endif

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
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

	if (atomic_read(&stk3x1x_obj->trace) & CMC_TRC_PS_DATA)
		APS_LOG("value = %d\n", *value);
#else
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
#endif

	return 0;
}


/*----------------------------------------------------------------------------*/
static int stk3x1x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct stk3x1x_priv *obj;
	struct als_control_path als_ctl = { 0 };
	struct als_data_path als_data = { 0 };
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };

	APS_LOG("%s: driver version: %s\n", __func__, DRIVER_VERSION);
	if (!(obj = kzalloc(sizeof(*obj), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}

	stk3x1x_obj = obj;
	obj->hw = hw;
	stk3x1x_get_addr(obj->hw, &obj->addr);

	INIT_DELAYED_WORK(&obj->eint_work, stk3x1x_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);
	atomic_set(&obj->als_debounce, 200);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 100);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->trace, 0x00);
	atomic_set(&obj->als_suspend, 0);

	atomic_set(&obj->state_val, 0x0);
	atomic_set(&obj->psctrl_val, PSCTRL_VAL);
	atomic_set(&obj->alsctrl_val, ALSCTRL_VAL);
	obj->ledctrl_val = LEDCTRL_VAL;
	obj->wait_val = WAIT_VAL;
	obj->int_val = 0;
	obj->first_boot = true;
	obj->als_correct_factor = 1000;
	obj->ps_cali = 0;

	atomic_set(&obj->ps_high_thd_val, obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_low_thd_val, obj->hw->ps_threshold_low);

	atomic_set(&obj->recv_reg, 0);
	obj->irq_node = of_find_compatible_node(NULL, NULL, "mediatek, als-eint");

	if (obj->hw->polling_mode_ps == 0)
		APS_LOG("%s: enable PS interrupt\n", __func__);
	obj->int_val |= STK_INT_PS_MODE1;

	if (obj->hw->polling_mode_als == 0) {
		obj->int_val |= STK_INT_ALS;
		APS_LOG("%s: enable ALS interrupt\n", __func__);
	}

	APS_LOG
	    ("%s: state=0x%x, psctrl=0x%x, alsctrl=0x%x, ledctrl=0x%x, wait=0x%x, int=0x%x\n",
	     __func__, atomic_read(&obj->state_val), atomic_read(&obj->psctrl_val),
	     atomic_read(&obj->alsctrl_val), obj->ledctrl_val, obj->wait_val, obj->int_val);

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level) / sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value) / sizeof(obj->hw->als_value[0]);
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	if (atomic_read(&obj->state_val) & STK_STATE_EN_ALS_MASK) {
		set_bit(STK_BIT_ALS, &obj->enable);
	}

	if (atomic_read(&obj->state_val) & STK_STATE_EN_PS_MASK) {
		set_bit(STK_BIT_PS, &obj->enable);
	}

	stk3x1x_i2c_client = client;

	err = stk3x1x_init_client(client);
	if (err)
		goto exit_init_failed;

	err = misc_register(&stk3x1x_device);
	if (err) {
		APS_ERR("stk3x1x_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	als_ctl.is_use_common_factory = false;
	ps_ctl.is_use_common_factory = false;

	err = stk3x1x_create_attr(&(stk3x1x_init_info.platform_diver_addr->driver));
	if (err) {
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}



	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay = als_set_delay;
	als_ctl.is_report_input_direct = false;
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
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
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.is_report_input_direct = true;
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
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

	err = batch_register_support_info(ID_LIGHT, als_ctl.is_support_batch, 100, 0);
	if (err)
		APS_ERR("register light batch support err = %d\n", err);

	err = batch_register_support_info(ID_PROXIMITY, ps_ctl.is_support_batch, 100, 0);
	if (err)
		APS_ERR("register proximity batch support err = %d\n", err);

	stk3x1x_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);
	return 0;

exit_sensor_obj_attach_fail:
exit_create_attr_failed:
	misc_deregister(&stk3x1x_device);
exit_misc_device_register_failed:
exit_init_failed:
	kfree(obj);
	obj = NULL;
exit:
	stk3x1x_i2c_client = NULL;
	stk3x1x_init_flag = -1;
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}

static int stk3x1x_i2c_remove(struct i2c_client *client)
{
	int err;

	err = stk3x1x_delete_attr(&(stk3x1x_init_info.platform_diver_addr->driver));
	if (err)
		APS_ERR("stk3x1x_delete_attr fail: %d\n", err);

	err = misc_deregister(&stk3x1x_device);
	if (err)
		APS_ERR("misc_deregister fail: %d\n", err);

	stk3x1x_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}

/*----------------------------------------------------------------------------*/
static int stk3x1x_local_init(void)
{
	stk3x1x_power(hw, 1);
	if (i2c_add_driver(&stk3x1x_i2c_driver)) {
		APS_ERR("add driver error\n");
		return -1;
	}
	if (-1 == stk3x1x_init_flag)
		return -1;

	return 0;
}

static int stk3x1x_local_uninit(void)
{
	APS_FUN();
	stk3x1x_power(hw, 0);
	i2c_del_driver(&stk3x1x_i2c_driver);
	return 0;
}


/*----------------------------------------------------------------------------*/
static int __init stk3x1x_init(void)
{
	const char *name = "mediatek,stk3x1x";

	APS_FUN();

	hw =  get_alsps_dts_func(name, hw);
	if (!hw)
		APS_ERR("get dts info fail\n");
	alsps_driver_add(&stk3x1x_init_info);
	return 0;
}

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

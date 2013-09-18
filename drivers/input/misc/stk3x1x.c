/*
 *  stk3x1x.c - Linux kernel modules for sensortek stk301x, stk321x and stk331x
 *  proximity/ambient light sensor
 *
 *  Copyright (c) 2013, The Linux Foundation. All Rights Reserved.
 *  Copyright (C) 2012 Lex Hsieh / sensortek <lex_hsieh@sitronix.com.tw> or
 *   <lex_hsieh@sensortek.com.tw>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Linux Foundation chooses to take subject only to the GPLv2 license
 *  terms, and distributes only under these terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "linux/stk3x1x.h"

#define DRIVER_VERSION  "3.4.4ts"

/* Driver Settings */
#define CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD
#ifdef CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD
#define STK_ALS_CHANGE_THD	20	/* The threshold to trigger ALS interrupt, unit: lux */
#endif	/* #ifdef CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD */
#define STK_INT_PS_MODE			1	/* 1, 2, or 3	*/
#define STK_POLL_PS
#define STK_POLL_ALS		/* ALS interrupt is valid only when STK_PS_INT_MODE = 1	or 4*/

#define STK_DEBUG_PRINTF

/* Define Register Map */
#define STK_STATE_REG 			0x00
#define STK_PSCTRL_REG 			0x01
#define STK_ALSCTRL_REG 		0x02
#define STK_LEDCTRL_REG 		0x03
#define STK_INT_REG 			0x04
#define STK_WAIT_REG 			0x05
#define STK_THDH1_PS_REG 		0x06
#define STK_THDH2_PS_REG 		0x07
#define STK_THDL1_PS_REG 		0x08
#define STK_THDL2_PS_REG 		0x09
#define STK_THDH1_ALS_REG 		0x0A
#define STK_THDH2_ALS_REG 		0x0B
#define STK_THDL1_ALS_REG 		0x0C
#define STK_THDL2_ALS_REG 		0x0D
#define STK_FLAG_REG 			0x10
#define STK_DATA1_PS_REG	 	0x11
#define STK_DATA2_PS_REG 		0x12
#define STK_DATA1_ALS_REG 		0x13
#define STK_DATA2_ALS_REG 		0x14
#define STK_DATA1_OFFSET_REG 	0x15
#define STK_DATA2_OFFSET_REG 	0x16
#define STK_DATA1_IR_REG 		0x17
#define STK_DATA2_IR_REG 		0x18
#define STK_PDT_ID_REG 			0x3E
#define STK_RSRVD_REG 			0x3F
#define STK_SW_RESET_REG		0x80


/* Define state reg */
#define STK_STATE_EN_IRS_SHIFT  	7
#define STK_STATE_EN_AK_SHIFT  		6
#define STK_STATE_EN_ASO_SHIFT  	5
#define STK_STATE_EN_IRO_SHIFT  	4
#define STK_STATE_EN_WAIT_SHIFT  	2
#define STK_STATE_EN_ALS_SHIFT  	1
#define STK_STATE_EN_PS_SHIFT  		0

#define STK_STATE_EN_IRS_MASK	0x80
#define STK_STATE_EN_AK_MASK	0x40
#define STK_STATE_EN_ASO_MASK	0x20
#define STK_STATE_EN_IRO_MASK	0x10
#define STK_STATE_EN_WAIT_MASK	0x04
#define STK_STATE_EN_ALS_MASK	0x02
#define STK_STATE_EN_PS_MASK	0x01

/* Define PS ctrl reg */
#define STK_PS_PRS_SHIFT  		6
#define STK_PS_GAIN_SHIFT  		4
#define STK_PS_IT_SHIFT  		0

#define STK_PS_PRS_MASK			0xC0
#define STK_PS_GAIN_MASK		0x30
#define STK_PS_IT_MASK			0x0F

/* Define ALS ctrl reg */
#define STK_ALS_PRS_SHIFT  		6
#define STK_ALS_GAIN_SHIFT  	4
#define STK_ALS_IT_SHIFT  		0

#define STK_ALS_PRS_MASK		0xC0
#define STK_ALS_GAIN_MASK		0x30
#define STK_ALS_IT_MASK			0x0F

/* Define LED ctrl reg */
#define STK_LED_IRDR_SHIFT  	6
#define STK_LED_DT_SHIFT  		0

#define STK_LED_IRDR_MASK		0xC0
#define STK_LED_DT_MASK			0x3F

/* Define interrupt reg */
#define STK_INT_CTRL_SHIFT  	7
#define STK_INT_OUI_SHIFT  		4
#define STK_INT_ALS_SHIFT  		3
#define STK_INT_PS_SHIFT  		0

#define STK_INT_CTRL_MASK		0x80
#define STK_INT_OUI_MASK		0x10
#define STK_INT_ALS_MASK		0x08
#define STK_INT_PS_MASK			0x07

#define STK_INT_ALS				0x08

/* Define flag reg */
#define STK_FLG_ALSDR_SHIFT  		7
#define STK_FLG_PSDR_SHIFT  		6
#define STK_FLG_ALSINT_SHIFT  		5
#define STK_FLG_PSINT_SHIFT  		4
#define STK_FLG_OUI_SHIFT  			2
#define STK_FLG_IR_RDY_SHIFT  		1
#define STK_FLG_NF_SHIFT  			0

#define STK_FLG_ALSDR_MASK		0x80
#define STK_FLG_PSDR_MASK		0x40
#define STK_FLG_ALSINT_MASK		0x20
#define STK_FLG_PSINT_MASK		0x10
#define STK_FLG_OUI_MASK		0x04
#define STK_FLG_IR_RDY_MASK		0x02
#define STK_FLG_NF_MASK			0x01

/* misc define */
#define MIN_ALS_POLL_DELAY_NS	110000000

#define DEVICE_NAME		"stk_ps"
#define ALS_NAME		"stk3x1x-ls"
#define PS_NAME "proximity"

/* POWER SUPPLY VOLTAGE RANGE */
#define STK3X1X_VDD_MIN_UV	2000000
#define STK3X1X_VDD_MAX_UV	3300000
#define STK3X1X_VIO_MIN_UV	1750000
#define STK3X1X_VIO_MAX_UV	1950000

#define STK_FIR_LEN 16
#define MAX_FIR_LEN 32
struct data_filter {
	u16 raw[MAX_FIR_LEN];
	int sum;
	int number;
	int idx;
};

struct stk3x1x_data {
	struct i2c_client *client;
#if (!defined(STK_POLL_PS) || !defined(STK_POLL_ALS))
    int32_t irq;
    struct work_struct stk_work;
	struct workqueue_struct *stk_wq;
#endif
	int		int_pin;
	uint8_t wait_reg;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend stk_early_suspend;
#endif
	uint16_t ps_thd_h;
	uint16_t ps_thd_l;
	struct mutex io_lock;
	struct input_dev *ps_input_dev;
	int32_t ps_distance_last;
	bool ps_enabled;
	struct wake_lock ps_wakelock;
    struct work_struct stk_ps_work;
	struct workqueue_struct *stk_ps_wq;
#ifdef STK_POLL_PS
	struct wake_lock ps_nosuspend_wl;
#endif
	struct input_dev *als_input_dev;
	int32_t als_lux_last;
	uint32_t als_transmittance;
	bool als_enabled;
	struct hrtimer als_timer;
	struct hrtimer ps_timer;
	ktime_t als_poll_delay;
	ktime_t ps_poll_delay;
#ifdef STK_POLL_ALS
    struct work_struct stk_als_work;
	struct workqueue_struct *stk_als_wq;
#endif
	struct regulator *vdd;
	struct regulator *vio;
	bool power_enabled;
	bool use_fir;
	struct data_filter      fir;
	atomic_t                firlength;
};

#if( !defined(CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD))
static uint32_t lux_threshold_table[] =
{
	3,
	10,
	40,
	65,
	145,
	300,
	550,
	930,
	1250,
	1700,
};

#define LUX_THD_TABLE_SIZE (sizeof(lux_threshold_table)/sizeof(uint32_t)+1)
static uint16_t code_threshold_table[LUX_THD_TABLE_SIZE+1];
#endif

static int32_t stk3x1x_enable_ps(struct stk3x1x_data *ps_data, uint8_t enable);
static int32_t stk3x1x_enable_als(struct stk3x1x_data *ps_data, uint8_t enable);
static int32_t stk3x1x_set_ps_thd_l(struct stk3x1x_data *ps_data, uint16_t thd_l);
static int32_t stk3x1x_set_ps_thd_h(struct stk3x1x_data *ps_data, uint16_t thd_h);
static int32_t stk3x1x_set_als_thd_l(struct stk3x1x_data *ps_data, uint16_t thd_l);
static int32_t stk3x1x_set_als_thd_h(struct stk3x1x_data *ps_data, uint16_t thd_h);
//static int32_t stk3x1x_set_ps_aoffset(struct stk3x1x_data *ps_data, uint16_t offset);

inline uint32_t stk_alscode2lux(struct stk3x1x_data *ps_data, uint32_t alscode)
{
	alscode += ((alscode<<7)+(alscode<<3)+(alscode>>1));
    alscode<<=3;
    alscode/=ps_data->als_transmittance;
	return alscode;
}

inline uint32_t stk_lux2alscode(struct stk3x1x_data *ps_data, uint32_t lux)
{
    lux*=ps_data->als_transmittance;
    lux/=1100;
    if (unlikely(lux>=(1<<16)))
        lux = (1<<16) -1;
    return lux;
}

#ifndef CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD
static void stk_init_code_threshold_table(struct stk3x1x_data *ps_data)
{
    uint32_t i,j;
    uint32_t alscode;

    code_threshold_table[0] = 0;
#ifdef STK_DEBUG_PRINTF
    printk(KERN_INFO "alscode[0]=%d\n",0);
#endif
    for (i=1,j=0;i<LUX_THD_TABLE_SIZE;i++,j++)
    {
        alscode = stk_lux2alscode(ps_data, lux_threshold_table[j]);
        printk(KERN_INFO "alscode[%d]=%d\n",i,alscode);
        code_threshold_table[i] = (uint16_t)(alscode);
    }
    code_threshold_table[i] = 0xffff;
    printk(KERN_INFO "alscode[%d]=%d\n",i,alscode);
}

static uint32_t stk_get_lux_interval_index(uint16_t alscode)
{
    uint32_t i;
    for (i=1;i<=LUX_THD_TABLE_SIZE;i++)
    {
        if ((alscode>=code_threshold_table[i-1])&&(alscode<code_threshold_table[i]))
        {
            return i;
        }
    }
    return LUX_THD_TABLE_SIZE;
}
#else
inline void stk_als_set_new_thd(struct stk3x1x_data *ps_data, uint16_t alscode)
{
    int32_t high_thd,low_thd;
    high_thd = alscode + stk_lux2alscode(ps_data, STK_ALS_CHANGE_THD);
    low_thd = alscode - stk_lux2alscode(ps_data, STK_ALS_CHANGE_THD);
    if (high_thd >= (1<<16))
        high_thd = (1<<16) -1;
    if (low_thd <0)
        low_thd = 0;
    stk3x1x_set_als_thd_h(ps_data, (uint16_t)high_thd);
    stk3x1x_set_als_thd_l(ps_data, (uint16_t)low_thd);
}
#endif // CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD


static int32_t stk3x1x_init_all_reg(struct stk3x1x_data *ps_data, struct stk3x1x_platform_data *plat_data)
{
	int32_t ret;
	uint8_t w_reg;

	w_reg = plat_data->state_reg;
    ret = i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG, w_reg);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
        return ret;
    }

	ps_data->ps_thd_h = plat_data->ps_thd_h;
	ps_data->ps_thd_l = plat_data->ps_thd_l;

	w_reg = plat_data->psctrl_reg;
    ret = i2c_smbus_write_byte_data(ps_data->client, STK_PSCTRL_REG, w_reg);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
        return ret;
    }
	w_reg = plat_data->alsctrl_reg;
    ret = i2c_smbus_write_byte_data(ps_data->client, STK_ALSCTRL_REG, w_reg);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
        return ret;
    }
	w_reg = plat_data->ledctrl_reg;
    ret = i2c_smbus_write_byte_data(ps_data->client, STK_LEDCTRL_REG, w_reg);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
        return ret;
    }
	ps_data->wait_reg = plat_data->wait_reg;

	if(ps_data->wait_reg < 2)
	{
		printk(KERN_WARNING "%s: wait_reg should be larger than 2, force to write 2\n", __func__);
		ps_data->wait_reg = 2;
	}
	else if (ps_data->wait_reg > 0xFF)
	{
		printk(KERN_WARNING "%s: wait_reg should be less than 0xFF, force to write 0xFF\n", __func__);
		ps_data->wait_reg = 0xFF;
	}
	w_reg = plat_data->wait_reg;
    ret = i2c_smbus_write_byte_data(ps_data->client, STK_WAIT_REG, w_reg);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
        return ret;
    }
	stk3x1x_set_ps_thd_h(ps_data, ps_data->ps_thd_h);
	stk3x1x_set_ps_thd_l(ps_data, ps_data->ps_thd_l);

	w_reg = 0;
#ifndef STK_POLL_PS
	w_reg |= STK_INT_PS_MODE;
#else
	w_reg |= 0x01;
#endif

#if (!defined(STK_POLL_ALS) && (STK_INT_PS_MODE != 0x02) && (STK_INT_PS_MODE != 0x03))
	w_reg |= STK_INT_ALS;
#endif
    ret = i2c_smbus_write_byte_data(ps_data->client, STK_INT_REG, w_reg);
    if (ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
	}

	ret = i2c_smbus_write_byte_data(ps_data->client, 0x87, 0x60);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write i2c error\n", __func__);
		return ret;
	}
	return 0;
}

static int32_t stk3x1x_check_pid(struct stk3x1x_data *ps_data)
{
	int32_t err1, err2;

	err1 = i2c_smbus_read_byte_data(ps_data->client,STK_PDT_ID_REG);
	if (err1 < 0)
	{
		printk(KERN_ERR "%s: read i2c error, err=%d\n", __func__, err1);
		return err1;
	}

    err2 = i2c_smbus_read_byte_data(ps_data->client,STK_RSRVD_REG);
    if (err2 < 0)
    {
        printk(KERN_ERR "%s: read i2c error, err=%d\n", __func__, err2);
        return -1;
    }
	printk(KERN_INFO "%s: PID=0x%x, RID=0x%x\n", __func__, err1, err2);
	if(err2 == 0xC0)
		printk(KERN_INFO "%s: RID=0xC0!!!!!!!!!!!!!\n", __func__);

	return 0;
}


static int32_t stk3x1x_software_reset(struct stk3x1x_data *ps_data)
{
    int32_t r;
    uint8_t w_reg;

    w_reg = 0x7F;
    r = i2c_smbus_write_byte_data(ps_data->client,STK_WAIT_REG,w_reg);
    if (r<0)
    {
        printk(KERN_ERR "%s: software reset: write i2c error, ret=%d\n", __func__, r);
        return r;
    }
    r = i2c_smbus_read_byte_data(ps_data->client,STK_WAIT_REG);
    if (w_reg != r)
    {
        printk(KERN_ERR "%s: software reset: read-back value is not the same\n", __func__);
        return -1;
    }

    r = i2c_smbus_write_byte_data(ps_data->client,STK_SW_RESET_REG,0);
    if (r<0)
    {
        printk(KERN_ERR "%s: software reset: read error after reset\n", __func__);
        return r;
    }
    msleep(1);
    return 0;
}


static int32_t stk3x1x_set_als_thd_l(struct stk3x1x_data *ps_data, uint16_t thd_l)
{
    uint8_t temp;
    uint8_t* pSrc = (uint8_t*)&thd_l;
    temp = *pSrc;
    *pSrc = *(pSrc+1);
    *(pSrc+1) = temp;
    return i2c_smbus_write_word_data(ps_data->client,STK_THDL1_ALS_REG,thd_l);
}
static int32_t stk3x1x_set_als_thd_h(struct stk3x1x_data *ps_data, uint16_t thd_h)
{
	uint8_t temp;
    uint8_t* pSrc = (uint8_t*)&thd_h;
    temp = *pSrc;
    *pSrc = *(pSrc+1);
    *(pSrc+1) = temp;
    return i2c_smbus_write_word_data(ps_data->client,STK_THDH1_ALS_REG,thd_h);
}

static int32_t stk3x1x_set_ps_thd_l(struct stk3x1x_data *ps_data, uint16_t thd_l)
{
    uint8_t temp;
    uint8_t* pSrc = (uint8_t*)&thd_l;

    temp = *pSrc;
    *pSrc = *(pSrc+1);
    *(pSrc+1) = temp;
    ps_data->ps_thd_l = thd_l;
	return i2c_smbus_write_word_data(ps_data->client,STK_THDL1_PS_REG,thd_l);
}

static int32_t stk3x1x_set_ps_thd_h(struct stk3x1x_data *ps_data, uint16_t thd_h)
{
    uint8_t temp;
    uint8_t* pSrc = (uint8_t*)&thd_h;

    temp = *pSrc;
    *pSrc = *(pSrc+1);
    *(pSrc+1) = temp;
    ps_data->ps_thd_h = thd_h;
	return i2c_smbus_write_word_data(ps_data->client,STK_THDH1_PS_REG,thd_h);
}

/*
static int32_t stk3x1x_set_ps_foffset(struct stk3x1x_data *ps_data, uint16_t offset)
{
	uint8_t temp;
    uint8_t* pSrc = (uint8_t*)&offset;
    temp = *pSrc;
    *pSrc = *(pSrc+1);
    *(pSrc+1) = temp;
    return i2c_smbus_write_word_data(ps_data->client,STK_DATA1_OFFSET_REG,offset);
}

static int32_t stk3x1x_set_ps_aoffset(struct stk3x1x_data *ps_data, uint16_t offset)
{
	uint8_t temp;
    uint8_t* pSrc = (uint8_t*)&offset;
	int ret;
	uint8_t w_state_reg;
	uint8_t re_en;

	ret = i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
	if (ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
	}
	re_en = (ret & STK_STATE_EN_AK_MASK) ? 1: 0;
	if(re_en)
	{
		w_state_reg = (uint8_t)(ret & (~STK_STATE_EN_AK_MASK));
		ret = i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG, w_state_reg);
		if (ret < 0)
		{
			printk(KERN_ERR "%s: write i2c error\n", __func__);
			return ret;
		}
		msleep(1);
	}
    temp = *pSrc;
    *pSrc = *(pSrc+1);
    *(pSrc+1) = temp;
	ret = i2c_smbus_write_word_data(ps_data->client,0x0E,offset);
	if(!re_en)
		return ret;

	w_state_reg |= STK_STATE_EN_AK_MASK;
	ret = i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG, w_state_reg);
	if (ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
	}

	return 0;
}
*/

static inline uint32_t stk3x1x_get_ps_reading(struct stk3x1x_data *ps_data)
{
	int32_t word_data, tmp_word_data;

	tmp_word_data = i2c_smbus_read_word_data(ps_data->client,STK_DATA1_PS_REG);
	if(tmp_word_data < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, tmp_word_data);
		return tmp_word_data;
	}
	word_data = ((tmp_word_data & 0xFF00) >> 8) | ((tmp_word_data & 0x00FF) << 8) ;
	return word_data;
}

static int32_t stk3x1x_set_flag(struct stk3x1x_data *ps_data, uint8_t org_flag_reg, uint8_t clr)
{
	uint8_t w_flag;
	w_flag = org_flag_reg | (STK_FLG_ALSINT_MASK | STK_FLG_PSINT_MASK | STK_FLG_OUI_MASK | STK_FLG_IR_RDY_MASK);
	w_flag &= (~clr);
	//printk(KERN_INFO "%s: org_flag_reg=0x%x, w_flag = 0x%x\n", __func__, org_flag_reg, w_flag);
    return i2c_smbus_write_byte_data(ps_data->client,STK_FLAG_REG, w_flag);
}

static int32_t stk3x1x_get_flag(struct stk3x1x_data *ps_data)
{
    return i2c_smbus_read_byte_data(ps_data->client,STK_FLAG_REG);
}

static int32_t stk3x1x_enable_ps(struct stk3x1x_data *ps_data, uint8_t enable)
{
    int32_t ret;
	uint8_t w_state_reg;
	uint8_t curr_ps_enable;
	curr_ps_enable = ps_data->ps_enabled?1:0;
	if(curr_ps_enable == enable)
		return 0;

    ret = i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
    if (ret < 0)
    {
			printk(KERN_ERR "%s: write i2c error, ret=%d\n", __func__, ret);
		return ret;
    }
	w_state_reg = ret;
	w_state_reg &= ~(STK_STATE_EN_PS_MASK | STK_STATE_EN_WAIT_MASK | 0x60);
	if(enable)
	{
		w_state_reg |= STK_STATE_EN_PS_MASK;
		if(!(ps_data->als_enabled))
			w_state_reg |= STK_STATE_EN_WAIT_MASK;
	}
    ret = i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG, w_state_reg);
    if (ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error, ret=%d\n", __func__, ret);
		return ret;
	}

    if(enable)
	{
#ifdef STK_POLL_PS
		hrtimer_start(&ps_data->ps_timer, ps_data->ps_poll_delay, HRTIMER_MODE_REL);
		ps_data->ps_distance_last = -1;
#endif
		ps_data->ps_enabled = true;
#ifndef STK_POLL_PS
#ifndef STK_POLL_ALS
		if(!(ps_data->als_enabled))
#endif	/* #ifndef STK_POLL_ALS	*/
			enable_irq(ps_data->irq);
		msleep(1);
		ret = stk3x1x_get_flag(ps_data);
		if (ret < 0)
		{
			printk(KERN_ERR "%s: read i2c error, ret=%d\n", __func__, ret);
			return ret;
		}

		near_far_state = ret & STK_FLG_NF_MASK;
		ps_data->ps_distance_last = near_far_state;
		input_report_abs(ps_data->ps_input_dev, ABS_DISTANCE, near_far_state);
		input_sync(ps_data->ps_input_dev);
		wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);
		reading = stk3x1x_get_ps_reading(ps_data);
		printk(KERN_INFO "%s: ps input event=%d, ps code = %d\n",__func__, near_far_state, reading);
#endif	/* #ifndef STK_POLL_PS */
	}
	else
	{
#ifdef STK_POLL_PS
		hrtimer_cancel(&ps_data->ps_timer);
#else
#ifndef STK_POLL_ALS
		if(!(ps_data->als_enabled))
#endif
			disable_irq(ps_data->irq);
#endif
		ps_data->ps_enabled = false;
	}
	return ret;
}

static int32_t stk3x1x_enable_als(struct stk3x1x_data *ps_data, uint8_t enable)
{
    int32_t ret;
	uint8_t w_state_reg;
	uint8_t curr_als_enable = (ps_data->als_enabled)?1:0;

	if(curr_als_enable == enable)
		return 0;

#ifndef STK_POLL_ALS
    if (enable)
	{
        stk3x1x_set_als_thd_h(ps_data, 0x0000);
        stk3x1x_set_als_thd_l(ps_data, 0xFFFF);
	}
#endif
    ret = i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
    }
	w_state_reg = (uint8_t)(ret & (~(STK_STATE_EN_ALS_MASK | STK_STATE_EN_WAIT_MASK)));
	if(enable)
		w_state_reg |= STK_STATE_EN_ALS_MASK;
	else if (ps_data->ps_enabled)
		w_state_reg |= STK_STATE_EN_WAIT_MASK;

    ret = i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG, w_state_reg);
    if (ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
	}

    if (enable)
    {
		ps_data->als_enabled = true;
#ifdef STK_POLL_ALS
		hrtimer_start(&ps_data->als_timer, ps_data->als_poll_delay, HRTIMER_MODE_REL);
#else
#ifndef STK_POLL_PS
		if(!(ps_data->ps_enabled))
#endif
			enable_irq(ps_data->irq);
#endif
    }
	else
	{
		ps_data->als_enabled = false;
#ifdef STK_POLL_ALS
		hrtimer_cancel(&ps_data->als_timer);
#else
#ifndef STK_POLL_PS
		if(!(ps_data->ps_enabled))
#endif
			disable_irq(ps_data->irq);
#endif
	}
    return ret;
}

static inline int32_t stk3x1x_filter_reading(struct stk3x1x_data *ps_data,
			int32_t word_data)
{
	int index;
	int firlen = atomic_read(&ps_data->firlength);

	if (ps_data->fir.number < firlen) {
		ps_data->fir.raw[ps_data->fir.number] = word_data;
		ps_data->fir.sum += word_data;
		ps_data->fir.number++;
		ps_data->fir.idx++;
	} else {
		index = ps_data->fir.idx % firlen;
		ps_data->fir.sum -= ps_data->fir.raw[index];
		ps_data->fir.raw[index] = word_data;
		ps_data->fir.sum += word_data;
		ps_data->fir.idx++;
		word_data = ps_data->fir.sum/firlen;
	}
	return word_data;
}

static inline int32_t stk3x1x_get_als_reading(struct stk3x1x_data *ps_data)
{
    int32_t word_data, tmp_word_data;

	tmp_word_data = i2c_smbus_read_word_data(ps_data->client, STK_DATA1_ALS_REG);
	if(tmp_word_data < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, tmp_word_data);
		return tmp_word_data;
	}
	word_data = ((tmp_word_data & 0xFF00) >> 8) | ((tmp_word_data & 0x00FF) << 8) ;
	if (ps_data->use_fir)
		word_data = stk3x1x_filter_reading(ps_data, word_data);

	return word_data;
}

static int32_t stk3x1x_get_ir_reading(struct stk3x1x_data *ps_data)
{
    int32_t word_data, tmp_word_data;
	int32_t ret;
	uint8_t w_reg, retry = 0;

	if(ps_data->ps_enabled)
	{
		stk3x1x_enable_ps(ps_data, 0);
		ps_data->ps_enabled = true;
	}
    ret = i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
    }
	w_reg = (uint8_t)(ret & (~STK_STATE_EN_IRS_MASK));
	w_reg |= STK_STATE_EN_IRS_MASK;

    ret = i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG, w_reg);
    if (ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
	}
	msleep(100);

	do
	{
		msleep(50);
		ret = stk3x1x_get_flag(ps_data);
		if (ret < 0)
		{
			printk(KERN_ERR "%s: write i2c error\n", __func__);
			return ret;
		}
		retry++;
	}while(retry < 5 && ((ret&STK_FLG_IR_RDY_MASK) == 0));

	if(retry == 5)
	{
		printk(KERN_ERR "%s: ir data is not ready for 300ms\n", __func__);
		return -EINVAL;
	}

	ret = stk3x1x_get_flag(ps_data);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
    }

	ret = stk3x1x_set_flag(ps_data, ret, STK_FLG_IR_RDY_MASK);
    if (ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
	}

	tmp_word_data = i2c_smbus_read_word_data(ps_data->client, STK_DATA1_IR_REG);
	if(tmp_word_data < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, tmp_word_data);
		return tmp_word_data;
	}
	word_data = ((tmp_word_data & 0xFF00) >> 8) | ((tmp_word_data & 0x00FF) << 8) ;

	if(ps_data->ps_enabled)
		stk3x1x_enable_ps(ps_data, 1);
	return word_data;
}


static ssize_t stk_als_code_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    int32_t reading;

    reading = stk3x1x_get_als_reading(ps_data);
    return scnprintf(buf, PAGE_SIZE, "%d\n", reading);
}


static ssize_t stk_als_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    int32_t enable, ret;

    mutex_lock(&ps_data->io_lock);
	enable = (ps_data->als_enabled)?1:0;
    mutex_unlock(&ps_data->io_lock);
    ret = i2c_smbus_read_byte_data(ps_data->client,STK_STATE_REG);
    ret = (ret & STK_STATE_EN_ALS_MASK)?1:0;

	if(enable != ret)
		printk(KERN_ERR "%s: driver and sensor mismatch! driver_enable=0x%x, sensor_enable=%x\n", __func__, enable, ret);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t stk_als_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data = dev_get_drvdata(dev);
	uint8_t en;
	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else
	{
		printk(KERN_ERR "%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
    printk(KERN_INFO "%s: Enable ALS : %d\n", __func__, en);
    mutex_lock(&ps_data->io_lock);
    stk3x1x_enable_als(ps_data, en);
    mutex_unlock(&ps_data->io_lock);
    return size;
}

static ssize_t stk_als_lux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data = dev_get_drvdata(dev);
    int32_t als_reading;
	uint32_t als_lux;
    als_reading = stk3x1x_get_als_reading(ps_data);
	mutex_lock(&ps_data->io_lock);
	als_lux = stk_alscode2lux(ps_data, als_reading);
	mutex_unlock(&ps_data->io_lock);
    return scnprintf(buf, PAGE_SIZE, "%d lux\n", als_lux);
}

static ssize_t stk_als_lux_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 16, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
    mutex_lock(&ps_data->io_lock);
    ps_data->als_lux_last = value;
	input_report_abs(ps_data->als_input_dev, ABS_MISC, value);
	input_sync(ps_data->als_input_dev);
	mutex_unlock(&ps_data->io_lock);
	printk(KERN_INFO "%s: als input event %ld lux\n",__func__, value);

    return size;
}


static ssize_t stk_als_transmittance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    int32_t transmittance;
    mutex_lock(&ps_data->io_lock);
    transmittance = ps_data->als_transmittance;
    mutex_unlock(&ps_data->io_lock);
    return scnprintf(buf, PAGE_SIZE, "%d\n", transmittance);
}


static ssize_t stk_als_transmittance_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
	mutex_lock(&ps_data->io_lock);
    ps_data->als_transmittance = value;
    mutex_unlock(&ps_data->io_lock);
    return size;
}

static ssize_t stk_als_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%lld\n", ktime_to_ns(ps_data->als_poll_delay));
}

static inline void stk_als_delay_store_fir(struct stk3x1x_data *ps_data)
{
	ps_data->fir.number = 0;
	ps_data->fir.idx = 0;
	ps_data->fir.sum = 0;
}
static ssize_t stk_als_delay_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    uint64_t value = 0;
	int ret;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	ret = kstrtoull(buf, 10, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:kstrtoull failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
#ifdef STK_DEBUG_PRINTF
	printk(KERN_INFO "%s: set als poll delay=%lld\n", __func__, value);
#endif
	if(value < MIN_ALS_POLL_DELAY_NS)
	{
		printk(KERN_ERR "%s: delay is too small\n", __func__);
		value = MIN_ALS_POLL_DELAY_NS;
	}
	mutex_lock(&ps_data->io_lock);
	if(value != ktime_to_ns(ps_data->als_poll_delay))
		ps_data->als_poll_delay = ns_to_ktime(value);

	if (ps_data->use_fir)
		stk_als_delay_store_fir(ps_data);

	mutex_unlock(&ps_data->io_lock);
	return size;
}

static ssize_t stk_als_ir_code_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    int32_t reading;
    reading = stk3x1x_get_ir_reading(ps_data);
    return scnprintf(buf, PAGE_SIZE, "%d\n", reading);
}

static ssize_t stk_als_firlen_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	int len = atomic_read(&ps_data->firlength);

	dev_dbg(dev, "%s: len = %2d, idx = %2d\n",
			__func__, len, ps_data->fir.idx);
	dev_dbg(dev, "%s: sum = %5d, ave = %5d\n",
			__func__, ps_data->fir.sum, ps_data->fir.sum/len);

	return scnprintf(buf, PAGE_SIZE, "%d\n", len);
}

static ssize_t stk_als_firlen_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	uint64_t value = 0;
	int ret;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	ret = kstrtoull(buf, 10, &value);
	if (ret < 0) {
		dev_err(dev, "%s:strict_strtoull failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}

	if (value > MAX_FIR_LEN) {
		dev_err(dev, "%s: firlen exceed maximum filter length\n",
			__func__);
	} else if (value < 1) {
		atomic_set(&ps_data->firlength, 1);
		memset(&ps_data->fir, 0x00, sizeof(ps_data->fir));
	} else {
		atomic_set(&ps_data->firlength, value);
		memset(&ps_data->fir, 0x00, sizeof(ps_data->fir));
	}
	return size;
}

static ssize_t stk_als_fir_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ps_data->use_fir);
}

static ssize_t stk_als_fir_enable_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	uint64_t value = 0;
	int ret;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	ret = kstrtoull(buf, 10, &value);
	if (ret < 0) {
		dev_err(dev, "%s:strict_strtoull failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}

	if (value) {
		ps_data->use_fir = true;
		memset(&ps_data->fir, 0x00, sizeof(ps_data->fir));
	} else {
		ps_data->use_fir = false;
	}
	return size;
}
static ssize_t stk_ps_code_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    uint32_t reading;
    reading = stk3x1x_get_ps_reading(ps_data);
    return scnprintf(buf, PAGE_SIZE, "%d\n", reading);
}

static ssize_t stk_ps_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int32_t enable, ret;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);

    mutex_lock(&ps_data->io_lock);
	enable = (ps_data->ps_enabled)?1:0;
    mutex_unlock(&ps_data->io_lock);
    ret = i2c_smbus_read_byte_data(ps_data->client,STK_STATE_REG);
    ret = (ret & STK_STATE_EN_PS_MASK)?1:0;

	if(enable != ret)
		printk(KERN_ERR "%s: driver and sensor mismatch! driver_enable=0x%x, sensor_enable=%x\n", __func__, enable, ret);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t stk_ps_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	uint8_t en;
	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else
	{
		printk(KERN_ERR "%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
    printk(KERN_INFO "%s: Enable PS : %d\n", __func__, en);
    mutex_lock(&ps_data->io_lock);
    stk3x1x_enable_ps(ps_data, en);
    mutex_unlock(&ps_data->io_lock);
    return size;
}

static ssize_t stk_ps_enable_aso_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int32_t ret;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);

    ret = i2c_smbus_read_byte_data(ps_data->client,STK_STATE_REG);
    ret = (ret & STK_STATE_EN_ASO_MASK)?1:0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t stk_ps_enable_aso_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	uint8_t en;
    int32_t ret;
	uint8_t w_state_reg;

	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else
	{
		printk(KERN_ERR "%s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
    printk(KERN_INFO "%s: Enable PS ASO : %d\n", __func__, en);

    ret = i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
    if (ret < 0)
    {
        printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
    }
	w_state_reg = (uint8_t)(ret & (~STK_STATE_EN_ASO_MASK));
	if(en)
		w_state_reg |= STK_STATE_EN_ASO_MASK;

    ret = i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG, w_state_reg);
    if (ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
	}

	return size;
}


static ssize_t stk_ps_offset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    int32_t word_data, tmp_word_data;

	tmp_word_data = i2c_smbus_read_word_data(ps_data->client, STK_DATA1_OFFSET_REG);
	if(tmp_word_data < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, tmp_word_data);
		return tmp_word_data;
	}
		word_data = ((tmp_word_data & 0xFF00) >> 8) | ((tmp_word_data & 0x00FF) << 8) ;
	return scnprintf(buf, PAGE_SIZE, "%d\n", word_data);
}

static ssize_t stk_ps_offset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	uint16_t offset;

	ret = kstrtoul(buf, 10, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
	if(value > 65535)
	{
		printk(KERN_ERR "%s: invalid value, offset=%ld\n", __func__, value);
		return -EINVAL;
	}

	offset = (uint16_t) ((value&0x00FF) << 8) | ((value&0xFF00) >>8);
	ret = i2c_smbus_write_word_data(ps_data->client,STK_DATA1_OFFSET_REG,offset);
	if(ret < 0)
	{
		printk(KERN_ERR "%s: write i2c error\n", __func__);
		return ret;
	}
	return size;
}


static ssize_t stk_ps_distance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    int32_t dist=1, ret;

    mutex_lock(&ps_data->io_lock);
    ret = stk3x1x_get_flag(ps_data);
	if(ret < 0)
	{
		printk(KERN_ERR "%s: stk3x1x_get_flag failed, ret=0x%x\n", __func__, ret);
		return ret;
	}
    dist = (ret & STK_FLG_NF_MASK)?1:0;

    ps_data->ps_distance_last = dist;
	input_report_abs(ps_data->ps_input_dev, ABS_DISTANCE, dist);
	input_sync(ps_data->ps_input_dev);
    mutex_unlock(&ps_data->io_lock);
	wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);
	printk(KERN_INFO "%s: ps input event %d cm\n",__func__, dist);
    return scnprintf(buf, PAGE_SIZE, "%d\n", dist);
}


static ssize_t stk_ps_distance_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
    mutex_lock(&ps_data->io_lock);
    ps_data->ps_distance_last = value;
	input_report_abs(ps_data->ps_input_dev, ABS_DISTANCE, value);
	input_sync(ps_data->ps_input_dev);
    mutex_unlock(&ps_data->io_lock);
	wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);
	printk(KERN_INFO "%s: ps input event %ld cm\n",__func__, value);
    return size;
}


static ssize_t stk_ps_code_thd_l_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int32_t ps_thd_l1_reg, ps_thd_l2_reg;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    mutex_lock(&ps_data->io_lock);
    ps_thd_l1_reg = i2c_smbus_read_byte_data(ps_data->client,STK_THDL1_PS_REG);
    if(ps_thd_l1_reg < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, ps_thd_l1_reg);
		return -EINVAL;
	}
    ps_thd_l2_reg = i2c_smbus_read_byte_data(ps_data->client,STK_THDL2_PS_REG);
    if(ps_thd_l2_reg < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, ps_thd_l2_reg);
		return -EINVAL;
	}
    mutex_unlock(&ps_data->io_lock);
	ps_thd_l1_reg = ps_thd_l1_reg<<8 | ps_thd_l2_reg;
    return scnprintf(buf, PAGE_SIZE, "%d\n", ps_thd_l1_reg);
}


static ssize_t stk_ps_code_thd_l_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
    mutex_lock(&ps_data->io_lock);
    stk3x1x_set_ps_thd_l(ps_data, value);
    mutex_unlock(&ps_data->io_lock);
    return size;
}

static ssize_t stk_ps_code_thd_h_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int32_t ps_thd_h1_reg, ps_thd_h2_reg;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    mutex_lock(&ps_data->io_lock);
    ps_thd_h1_reg = i2c_smbus_read_byte_data(ps_data->client,STK_THDH1_PS_REG);
    if(ps_thd_h1_reg < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, ps_thd_h1_reg);
		return -EINVAL;
	}
    ps_thd_h2_reg = i2c_smbus_read_byte_data(ps_data->client,STK_THDH2_PS_REG);
    if(ps_thd_h2_reg < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, ps_thd_h2_reg);
		return -EINVAL;
	}
    mutex_unlock(&ps_data->io_lock);
	ps_thd_h1_reg = ps_thd_h1_reg<<8 | ps_thd_h2_reg;
    return scnprintf(buf, PAGE_SIZE, "%d\n", ps_thd_h1_reg);
}


static ssize_t stk_ps_code_thd_h_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
    mutex_lock(&ps_data->io_lock);
    stk3x1x_set_ps_thd_h(ps_data, value);
    mutex_unlock(&ps_data->io_lock);
    return size;
}

#if 0
static ssize_t stk_als_lux_thd_l_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int32_t als_thd_l0_reg,als_thd_l1_reg;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	uint32_t als_lux;

    mutex_lock(&ps_data->io_lock);
    als_thd_l0_reg = i2c_smbus_read_byte_data(ps_data->client,STK_THDL1_ALS_REG);
    als_thd_l1_reg = i2c_smbus_read_byte_data(ps_data->client,STK_THDL2_ALS_REG);
    if(als_thd_l0_reg < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, als_thd_l0_reg);
		return -EINVAL;
	}
	if(als_thd_l1_reg < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, als_thd_l1_reg);
		return -EINVAL;
	}
    als_thd_l0_reg|=(als_thd_l1_reg<<8);
	als_lux = stk_alscode2lux(ps_data, als_thd_l0_reg);
	mutex_unlock(&ps_data->io_lock);
    return scnprintf(buf, PAGE_SIZE, "%d\n", als_lux);
}


static ssize_t stk_als_lux_thd_l_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
    mutex_lock(&ps_data->io_lock);
	value = stk_lux2alscode(ps_data, value);
    stk3x1x_set_als_thd_l(ps_data, value);
    mutex_unlock(&ps_data->io_lock);
    return size;
}

static ssize_t stk_als_lux_thd_h_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int32_t als_thd_h0_reg,als_thd_h1_reg;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	uint32_t als_lux;

    mutex_lock(&ps_data->io_lock);
    als_thd_h0_reg = i2c_smbus_read_byte_data(ps_data->client,STK_THDH1_ALS_REG);
    als_thd_h1_reg = i2c_smbus_read_byte_data(ps_data->client,STK_THDH2_ALS_REG);
    if(als_thd_h0_reg < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, als_thd_h0_reg);
		return -EINVAL;
	}
	if(als_thd_h1_reg < 0)
	{
		printk(KERN_ERR "%s fail, err=0x%x", __func__, als_thd_h1_reg);
		return -EINVAL;
	}
    als_thd_h0_reg|=(als_thd_h1_reg<<8);
	als_lux = stk_alscode2lux(ps_data, als_thd_h0_reg);
	mutex_unlock(&ps_data->io_lock);
    return scnprintf(buf, PAGE_SIZE, "%d\n", als_lux);
}


static ssize_t stk_als_lux_thd_h_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = strict_strtoul(buf, 10, &value);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:strict_strtoul failed, ret=0x%x\n", __func__, ret);
		return ret;
	}
	mutex_lock(&ps_data->io_lock);
    value = stk_lux2alscode(ps_data, value);
    stk3x1x_set_als_thd_h(ps_data, value);
    mutex_unlock(&ps_data->io_lock);
    return size;
}
#endif


static ssize_t stk_all_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int32_t ps_reg[27];
	uint8_t cnt;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
    mutex_lock(&ps_data->io_lock);
	for(cnt=0;cnt<25;cnt++)
	{
		ps_reg[cnt] = i2c_smbus_read_byte_data(ps_data->client, (cnt));
		if(ps_reg[cnt] < 0)
		{
			mutex_unlock(&ps_data->io_lock);
			printk(KERN_ERR "stk_all_reg_show:i2c_smbus_read_byte_data fail, ret=%d", ps_reg[cnt]);
			return -EINVAL;
		}
		else
		{
			printk(KERN_INFO "reg[0x%2X]=0x%2X\n", cnt, ps_reg[cnt]);
		}
	}
	ps_reg[cnt] = i2c_smbus_read_byte_data(ps_data->client, STK_PDT_ID_REG);
	if(ps_reg[cnt] < 0)
	{
		mutex_unlock(&ps_data->io_lock);
		printk( KERN_ERR "all_reg_show:i2c_smbus_read_byte_data fail, ret=%d", ps_reg[cnt]);
		return -EINVAL;
	}
	printk( KERN_INFO "reg[0x%x]=0x%2X\n", STK_PDT_ID_REG, ps_reg[cnt]);
	cnt++;
	ps_reg[cnt] = i2c_smbus_read_byte_data(ps_data->client, STK_RSRVD_REG);
	if(ps_reg[cnt] < 0)
	{
		mutex_unlock(&ps_data->io_lock);
		printk( KERN_ERR "all_reg_show:i2c_smbus_read_byte_data fail, ret=%d", ps_reg[cnt]);
		return -EINVAL;
	}
	printk( KERN_INFO "reg[0x%x]=0x%2X\n", STK_RSRVD_REG, ps_reg[cnt]);
    mutex_unlock(&ps_data->io_lock);

    return scnprintf(buf, PAGE_SIZE, "%2X %2X %2X %2X %2X,%2X %2X %2X %2X %2X,%2X %2X %2X %2X %2X,%2X %2X %2X %2X %2X,%2X %2X %2X %2X %2X,%2X %2X\n",
		ps_reg[0], ps_reg[1], ps_reg[2], ps_reg[3], ps_reg[4], ps_reg[5], ps_reg[6], ps_reg[7], ps_reg[8],
		ps_reg[9], ps_reg[10], ps_reg[11], ps_reg[12], ps_reg[13], ps_reg[14], ps_reg[15], ps_reg[16], ps_reg[17],
		ps_reg[18], ps_reg[19], ps_reg[20], ps_reg[21], ps_reg[22], ps_reg[23], ps_reg[24], ps_reg[25], ps_reg[26]);
}

static ssize_t stk_recv_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}


static ssize_t stk_recv_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long value = 0;
	int ret;
	int32_t recv_data;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);

	ret = kstrtoul(buf, 16, &value);
	if (ret < 0) {
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
	recv_data = i2c_smbus_read_byte_data(ps_data->client,value);
	printk("%s: reg 0x%x=0x%x\n", __func__, (int)value, recv_data);
	return size;
}


static ssize_t stk_send_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}


static ssize_t stk_send_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int addr, cmd;
	u8 addr_u8, cmd_u8;
	int32_t ret, i;
	char *token[10];
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);

	for (i = 0; i < 2; i++)
		token[i] = strsep((char **)&buf, " ");
	ret = kstrtoul(token[0], 16, (unsigned long *)&(addr));
	if (ret < 0) {

		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
	ret = kstrtoul(token[1], 16, (unsigned long *)&(cmd));
	if (ret < 0) {
		printk(KERN_ERR "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
	printk(KERN_INFO "%s: write reg 0x%x=0x%x\n", __func__, addr, cmd);

	addr_u8 = (u8) addr;
	cmd_u8 = (u8) cmd;
	//mutex_lock(&ps_data->io_lock);
	ret = i2c_smbus_write_byte_data(ps_data->client,addr_u8,cmd_u8);
	//mutex_unlock(&ps_data->io_lock);
	if (0 != ret)
	{
		printk(KERN_ERR "%s: i2c_smbus_write_byte_data fail\n", __func__);
		return ret;
	}

	return size;
}

static struct device_attribute als_enable_attribute = __ATTR(enable,0664,stk_als_enable_show,stk_als_enable_store);
static struct device_attribute als_lux_attribute = __ATTR(lux,0664,stk_als_lux_show,stk_als_lux_store);
static struct device_attribute als_code_attribute = __ATTR(code, 0444, stk_als_code_show, NULL);
static struct device_attribute als_transmittance_attribute = __ATTR(transmittance,0664,stk_als_transmittance_show,stk_als_transmittance_store);
static struct device_attribute als_poll_delay_attribute =
	__ATTR(poll_delay, 0664, stk_als_delay_show, stk_als_delay_store);
static struct device_attribute als_ir_code_attribute = __ATTR(ircode,0444,stk_als_ir_code_show,NULL);
static struct device_attribute als_firlen_attribute =
	__ATTR(firlen, 0664, stk_als_firlen_show, stk_als_firlen_store);
static struct device_attribute als_fir_enable_attribute =
	__ATTR(fir_enable, 0664, stk_als_fir_enable_show,
	stk_als_fir_enable_store);

static struct attribute *stk_als_attrs [] =
{
	&als_enable_attribute.attr,
    &als_lux_attribute.attr,
    &als_code_attribute.attr,
    &als_transmittance_attribute.attr,
	&als_poll_delay_attribute.attr,
	&als_ir_code_attribute.attr,
	&als_firlen_attribute.attr,
	&als_fir_enable_attribute.attr,
    NULL
};

static struct attribute_group stk_als_attribute_group = {
	.attrs = stk_als_attrs,
};


static struct device_attribute ps_enable_attribute = __ATTR(enable,0664,stk_ps_enable_show,stk_ps_enable_store);
static struct device_attribute ps_enable_aso_attribute = __ATTR(enableaso,0664,stk_ps_enable_aso_show,stk_ps_enable_aso_store);
static struct device_attribute ps_distance_attribute = __ATTR(distance,0664,stk_ps_distance_show, stk_ps_distance_store);
static struct device_attribute ps_offset_attribute = __ATTR(offset,0664,stk_ps_offset_show, stk_ps_offset_store);
static struct device_attribute ps_code_attribute = __ATTR(code, 0444, stk_ps_code_show, NULL);
static struct device_attribute ps_code_thd_l_attribute = __ATTR(codethdl,0664,stk_ps_code_thd_l_show,stk_ps_code_thd_l_store);
static struct device_attribute ps_code_thd_h_attribute = __ATTR(codethdh,0664,stk_ps_code_thd_h_show,stk_ps_code_thd_h_store);
static struct device_attribute recv_attribute = __ATTR(recv,0664,stk_recv_show,stk_recv_store);
static struct device_attribute send_attribute = __ATTR(send,0664,stk_send_show, stk_send_store);
static struct device_attribute all_reg_attribute = __ATTR(allreg, 0444, stk_all_reg_show, NULL);

static struct attribute *stk_ps_attrs [] =
{
    &ps_enable_attribute.attr,
    &ps_enable_aso_attribute.attr,
    &ps_distance_attribute.attr,
	&ps_offset_attribute.attr,
    &ps_code_attribute.attr,
	&ps_code_thd_l_attribute.attr,
	&ps_code_thd_h_attribute.attr,
	&recv_attribute.attr,
	&send_attribute.attr,
	&all_reg_attribute.attr,
    NULL
};

static struct attribute_group stk_ps_attribute_group = {
	.attrs = stk_ps_attrs,
};

#ifdef STK_POLL_ALS
static enum hrtimer_restart stk_als_timer_func(struct hrtimer *timer)
{
	struct stk3x1x_data *ps_data = container_of(timer, struct stk3x1x_data, als_timer);
	queue_work(ps_data->stk_als_wq, &ps_data->stk_als_work);
	hrtimer_forward_now(&ps_data->als_timer, ps_data->als_poll_delay);
	return HRTIMER_RESTART;
}

static void stk_als_work_func(struct work_struct *work)
{
	struct stk3x1x_data *ps_data = container_of(work, struct stk3x1x_data, stk_als_work);
	int32_t reading;

    mutex_lock(&ps_data->io_lock);
	reading = stk3x1x_get_als_reading(ps_data);
	if(reading < 0)
		return;
	ps_data->als_lux_last = stk_alscode2lux(ps_data, reading);
	input_report_abs(ps_data->als_input_dev, ABS_MISC, ps_data->als_lux_last);
	input_sync(ps_data->als_input_dev);
	mutex_unlock(&ps_data->io_lock);
	//printk(KERN_INFO "%s: als input event %d lux\n",__func__, ps_data->als_lux_last);
}
#endif

static enum hrtimer_restart stk_ps_timer_func(struct hrtimer *timer)
{
	struct stk3x1x_data *ps_data = container_of(timer, struct stk3x1x_data, ps_timer);
	queue_work(ps_data->stk_ps_wq, &ps_data->stk_ps_work);
#ifdef STK_POLL_PS
	hrtimer_forward_now(&ps_data->ps_timer, ps_data->ps_poll_delay);
	return HRTIMER_RESTART;
#else
	hrtimer_cancel(&ps_data->ps_timer);
	return HRTIMER_NORESTART;
#endif
}

static void stk_ps_work_func(struct work_struct *work)
{
	struct stk3x1x_data *ps_data = container_of(work, struct stk3x1x_data, stk_ps_work);
	uint32_t reading;
	int32_t near_far_state;
    uint8_t org_flag_reg;
	int32_t ret;
    uint8_t disable_flag = 0;
    mutex_lock(&ps_data->io_lock);

	org_flag_reg = stk3x1x_get_flag(ps_data);
	if(org_flag_reg < 0)
	{
		printk(KERN_ERR "%s: get_status_reg fail, ret=%d", __func__, org_flag_reg);
		goto err_i2c_rw;
	}
	near_far_state = (org_flag_reg & STK_FLG_NF_MASK)?1:0;
	reading = stk3x1x_get_ps_reading(ps_data);
	if(ps_data->ps_distance_last != near_far_state)
	{
		ps_data->ps_distance_last = near_far_state;
		input_report_abs(ps_data->ps_input_dev, ABS_DISTANCE, near_far_state);
		input_sync(ps_data->ps_input_dev);
		wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);
#ifdef STK_DEBUG_PRINTF
		printk(KERN_INFO "%s: ps input event %d cm, ps code = %d\n",__func__, near_far_state, reading);
#endif
	}
	ret = stk3x1x_set_flag(ps_data, org_flag_reg, disable_flag);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:stk3x1x_set_flag fail, ret=%d\n", __func__, ret);
		goto err_i2c_rw;
	}

	mutex_unlock(&ps_data->io_lock);
	return;

err_i2c_rw:
	mutex_unlock(&ps_data->io_lock);
	msleep(30);
	return;
}


#if (!defined(STK_POLL_PS) || !defined(STK_POLL_ALS))
static void stk_work_func(struct work_struct *work)
{
	uint32_t reading;
#if ((STK_INT_PS_MODE != 0x03) && (STK_INT_PS_MODE != 0x02))
    int32_t ret;
    uint8_t disable_flag = 0;
    uint8_t org_flag_reg;
#endif	/* #if ((STK_INT_PS_MODE != 0x03) && (STK_INT_PS_MODE != 0x02)) */

#ifndef CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD
	uint32_t nLuxIndex;
#endif
	struct stk3x1x_data *ps_data = container_of(work, struct stk3x1x_data, stk_work);
	int32_t near_far_state;

    mutex_lock(&ps_data->io_lock);

#if (STK_INT_PS_MODE	== 0x03)
	near_far_state = gpio_get_value(ps_data->int_pin);
#elif	(STK_INT_PS_MODE	== 0x02)
	near_far_state = !(gpio_get_value(ps_data->int_pin));
#endif

#if ((STK_INT_PS_MODE == 0x03) || (STK_INT_PS_MODE	== 0x02))
	ps_data->ps_distance_last = near_far_state;
	input_report_abs(ps_data->ps_input_dev, ABS_DISTANCE, near_far_state);
	input_sync(ps_data->ps_input_dev);
	wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);
	reading = stk3x1x_get_ps_reading(ps_data);
#ifdef STK_DEBUG_PRINTF
	printk(KERN_INFO "%s: ps input event %d cm, ps code = %d\n",__func__, near_far_state, reading);
#endif
#else
	/* mode 0x01 or 0x04 */
	org_flag_reg = stk3x1x_get_flag(ps_data);
	if(org_flag_reg < 0)
	{
		printk(KERN_ERR "%s: get_status_reg fail, org_flag_reg=%d", __func__, org_flag_reg);
		goto err_i2c_rw;
	}

    if (org_flag_reg & STK_FLG_ALSINT_MASK)
    {
		disable_flag |= STK_FLG_ALSINT_MASK;
        reading = stk3x1x_get_als_reading(ps_data);
		if(reading < 0)
		{
			printk(KERN_ERR "%s: stk3x1x_get_als_reading fail, ret=%d", __func__, reading);
			goto err_i2c_rw;
		}
#ifndef CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD
        nLuxIndex = stk_get_lux_interval_index(reading);
        stk3x1x_set_als_thd_h(ps_data, code_threshold_table[nLuxIndex]);
        stk3x1x_set_als_thd_l(ps_data, code_threshold_table[nLuxIndex-1]);
#else
        stk_als_set_new_thd(ps_data, reading);
#endif //CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD
		ps_data->als_lux_last = stk_alscode2lux(ps_data, reading);
		input_report_abs(ps_data->als_input_dev, ABS_MISC, ps_data->als_lux_last);
		input_sync(ps_data->als_input_dev);
#ifdef STK_DEBUG_PRINTF
		printk(KERN_INFO "%s: als input event %d lux\n",__func__, ps_data->als_lux_last);
#endif
    }
    if (org_flag_reg & STK_FLG_PSINT_MASK)
    {
		disable_flag |= STK_FLG_PSINT_MASK;
		near_far_state = (org_flag_reg & STK_FLG_NF_MASK)?1:0;

		ps_data->ps_distance_last = near_far_state;
		input_report_abs(ps_data->ps_input_dev, ABS_DISTANCE, near_far_state);
		input_sync(ps_data->ps_input_dev);
		wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);
        reading = stk3x1x_get_ps_reading(ps_data);
#ifdef STK_DEBUG_PRINTF
		printk(KERN_INFO "%s: ps input event=%d, ps code = %d\n",__func__, near_far_state, reading);
#endif
    }

    ret = stk3x1x_set_flag(ps_data, org_flag_reg, disable_flag);
	if(ret < 0)
	{
		printk(KERN_ERR "%s:reset_int_flag fail, ret=%d\n", __func__, ret);
		goto err_i2c_rw;
	}
#endif

	msleep(1);
    enable_irq(ps_data->irq);
    mutex_unlock(&ps_data->io_lock);
	return;

err_i2c_rw:
	mutex_unlock(&ps_data->io_lock);
	msleep(30);
	enable_irq(ps_data->irq);
	return;
}
#endif

#if (!defined(STK_POLL_PS) || !defined(STK_POLL_ALS))
static irqreturn_t stk_oss_irq_handler(int irq, void *data)
{
	struct stk3x1x_data *pData = data;
	disable_irq_nosync(irq);
	queue_work(pData->stk_wq,&pData->stk_work);
	return IRQ_HANDLED;
}
#endif	/*	#if (!defined(STK_POLL_PS) || !defined(STK_POLL_ALS))	*/

static inline void stk3x1x_init_fir(struct stk3x1x_data *ps_data)
{
	memset(&ps_data->fir, 0x00, sizeof(ps_data->fir));
	atomic_set(&ps_data->firlength, STK_FIR_LEN);
}

static int32_t stk3x1x_init_all_setting(struct i2c_client *client, struct stk3x1x_platform_data *plat_data)
{
	int32_t ret;
	struct stk3x1x_data *ps_data = i2c_get_clientdata(client);

	mutex_lock(&ps_data->io_lock);
	ps_data->als_enabled = false;
	ps_data->ps_enabled = false;
	mutex_unlock(&ps_data->io_lock);

	ret = stk3x1x_software_reset(ps_data);
	if(ret < 0)
		return ret;

	stk3x1x_check_pid(ps_data);
	if(ret < 0)
		return ret;

	ret = stk3x1x_init_all_reg(ps_data, plat_data);
	if(ret < 0)
		return ret;
#ifndef CONFIG_STK_PS_ALS_USE_CHANGE_THRESHOLD
	stk_init_code_threshold_table(ps_data);
#endif

	if (plat_data->use_fir)
		stk3x1x_init_fir(ps_data);

    return 0;
}

#if (!defined(STK_POLL_PS) || !defined(STK_POLL_ALS))
static int stk3x1x_setup_irq(struct i2c_client *client)
{
	int irq, err = -EIO;
	struct stk3x1x_data *ps_data = i2c_get_clientdata(client);

	irq = gpio_to_irq(ps_data->int_pin);
#ifdef STK_DEBUG_PRINTF
	printk(KERN_INFO "%s: int pin #=%d, irq=%d\n",__func__, ps_data->int_pin, irq);
#endif
	if (irq <= 0)
	{
		printk(KERN_ERR "irq number is not specified, irq # = %d, int pin=%d\n",irq, ps_data->int_pin);
		return irq;
	}
	ps_data->irq = irq;
	err = gpio_request(ps_data->int_pin,"stk-int");
	if(err < 0)
	{
		printk(KERN_ERR "%s: gpio_request, err=%d", __func__, err);
		return err;
	}
	err = gpio_direction_input(ps_data->int_pin);
	if(err < 0)
	{
		printk(KERN_ERR "%s: gpio_direction_input, err=%d", __func__, err);
		return err;
	}
#if ((STK_INT_PS_MODE == 0x03) || (STK_INT_PS_MODE	== 0x02))
	err = request_any_context_irq(irq, stk_oss_irq_handler, IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING, DEVICE_NAME, ps_data);
#else
	err = request_any_context_irq(irq, stk_oss_irq_handler, IRQF_TRIGGER_LOW, DEVICE_NAME, ps_data);
#endif
	if (err < 0)
	{
		printk(KERN_WARNING "%s: request_any_context_irq(%d) failed for (%d)\n", __func__, irq, err);
		goto err_request_any_context_irq;
	}
	disable_irq(irq);

	return 0;
err_request_any_context_irq:
	gpio_free(ps_data->int_pin);
	return err;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void stk3x1x_early_suspend(struct early_suspend *h)
{
	struct stk3x1x_data *ps_data = container_of(h, struct stk3x1x_data, stk_early_suspend);
#ifndef STK_POLL_PS
	int err;
#endif

	printk(KERN_INFO "%s", __func__);
    mutex_lock(&ps_data->io_lock);
	if(ps_data->als_enabled)
	{
		stk3x1x_enable_als(ps_data, 0);
		ps_data->als_enabled = true;
	}
	if(ps_data->ps_enabled)
	{
#ifdef STK_POLL_PS
		wake_lock(&ps_data->ps_nosuspend_wl);
#else
		err = enable_irq_wake(ps_data->irq);
		if (err)
			printk(KERN_WARNING "%s: set_irq_wake(%d) failed, err=(%d)\n", __func__, ps_data->irq, err);
#endif
	}
	mutex_unlock(&ps_data->io_lock);
	return;
}

static void stk3x1x_late_resume(struct early_suspend *h)
{
	struct stk3x1x_data *ps_data = container_of(h, struct stk3x1x_data, stk_early_suspend);
#ifndef STK_POLL_PS
	int err;
#endif

	printk(KERN_INFO "%s", __func__);
    mutex_lock(&ps_data->io_lock);
	if(ps_data->als_enabled)
		stk3x1x_enable_als(ps_data, 1);

	if(ps_data->ps_enabled)
	{
#ifdef STK_POLL_PS
		wake_lock(&ps_data->ps_nosuspend_wl);
#else
		err = disable_irq_wake(ps_data->irq);
		if (err)
			printk(KERN_WARNING "%s: disable_irq_wake(%d) failed, err=(%d)\n", __func__, ps_data->irq, err);
#endif
	}
	mutex_unlock(&ps_data->io_lock);
	return;
}
#endif	//#ifdef CONFIG_HAS_EARLYSUSPEND

static int stk3x1x_power_on(struct stk3x1x_data *data, bool on)
{
	int ret = 0;

	if (!on && data->power_enabled) {
		ret = regulator_disable(data->vdd);
		if (ret) {
			dev_err(&data->client->dev,
				"Regulator vdd disable failed ret=%d\n", ret);
			return ret;
		}

		ret = regulator_disable(data->vio);
		if (ret) {
			dev_err(&data->client->dev,
				"Regulator vio disable failed ret=%d\n", ret);
			ret = regulator_enable(data->vdd);
			if (ret) {
				dev_err(&data->client->dev,
					"Regulator vdd enable failed ret=%d\n",
					ret);
			}
		}
	} else if (on && !data->power_enabled) {

		ret = regulator_enable(data->vdd);
		if (ret) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed ret=%d\n", ret);
			return ret;
		}

		ret = regulator_enable(data->vio);
		if (ret) {
			dev_err(&data->client->dev,
				"Regulator vio enable failed ret=%d\n", ret);
			regulator_disable(data->vdd);
		}
	} else {
		dev_warn(&data->client->dev,
				"Power on=%d. enabled=%d\n",
				on, data->power_enabled);
	}

	return ret;
}

static int stk3x1x_power_init(struct stk3x1x_data *data, bool on)
{
	int ret;

	if (!on) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd,
					0, STK3X1X_VDD_MAX_UV);

		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio,
					0, STK3X1X_VIO_MAX_UV);

		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			ret = PTR_ERR(data->vdd);
			dev_err(&data->client->dev,
				"Regulator get failed vdd ret=%d\n", ret);
			return ret;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			ret = regulator_set_voltage(data->vdd,
					STK3X1X_VDD_MIN_UV,
					STK3X1X_VDD_MAX_UV);
			if (ret) {
				dev_err(&data->client->dev,
					"Regulator set failed vdd ret=%d\n",
					ret);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			ret = PTR_ERR(data->vio);
			dev_err(&data->client->dev,
				"Regulator get failed vio ret=%d\n", ret);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			ret = regulator_set_voltage(data->vio,
					STK3X1X_VIO_MIN_UV,
					STK3X1X_VIO_MAX_UV);
			if (ret) {
				dev_err(&data->client->dev,
				"Regulator set failed vio ret=%d\n", ret);
				goto reg_vio_put;
			}
		}
	}

	return 0;

reg_vio_put:
	regulator_put(data->vio);
reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, STK3X1X_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return ret;
}

#ifdef CONFIG_OF
static int stk3x1x_parse_dt(struct device *dev,
			struct stk3x1x_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	pdata->int_pin = of_get_named_gpio_flags(np, "stk,irq-gpio",
				0, &pdata->int_flags);
	if (pdata->int_pin < 0) {
		dev_err(dev, "Unable to read irq-gpio\n");
		return pdata->int_pin;
	}

	rc = of_property_read_u32(np, "stk,transmittance", &temp_val);
	if (!rc)
		pdata->transmittance = temp_val;
	else {
		dev_err(dev, "Unable to read transmittance\n");
		return rc;
	}

	rc = of_property_read_u32(np, "stk,state-reg", &temp_val);
	if (!rc)
		pdata->state_reg = temp_val;
	else {
		dev_err(dev, "Unable to read state-reg\n");
		return rc;
	}

	rc = of_property_read_u32(np, "stk,psctrl-reg", &temp_val);
	if (!rc)
		pdata->psctrl_reg = (u8)temp_val;
	else {
		dev_err(dev, "Unable to read psctrl-reg\n");
		return rc;
	}

	rc = of_property_read_u32(np, "stk,alsctrl-reg", &temp_val);
	if (!rc)
		pdata->alsctrl_reg = (u8)temp_val;
	else {
		dev_err(dev, "Unable to read alsctrl-reg\n");
		return rc;
	}

	rc = of_property_read_u32(np, "stk,ledctrl-reg", &temp_val);
	if (!rc)
		pdata->ledctrl_reg = (u8)temp_val;
	else {
		dev_err(dev, "Unable to read ledctrl-reg\n");
		return rc;
	}

	rc = of_property_read_u32(np, "stk,wait-reg", &temp_val);
	if (!rc)
		pdata->wait_reg = (u8)temp_val;
	else {
		dev_err(dev, "Unable to read wait-reg\n");
		return rc;
	}

	rc = of_property_read_u32(np, "stk,ps-thdh", &temp_val);
	if (!rc)
		pdata->ps_thd_h = (u16)temp_val;
	else {
		dev_err(dev, "Unable to read ps-thdh\n");
		return rc;
	}

	rc = of_property_read_u32(np, "stk,ps-thdl", &temp_val);
	if (!rc)
		pdata->ps_thd_l = (u16)temp_val;
	else {
		dev_err(dev, "Unable to read ps-thdl\n");
		return rc;
	}

	pdata->use_fir = of_property_read_bool(np, "stk,use-fir");

	return 0;
}
#else
static int stk3x1x_parse_dt(struct device *dev,
			struct stk3x1x_platform_data *pdata)
{
	return -ENODEV;
}
#endif /* !CONFIG_OF */

static int stk3x1x_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
    int err = -ENODEV;
    struct stk3x1x_data *ps_data;
	struct stk3x1x_platform_data *plat_data;
    printk(KERN_INFO "%s: driver version = %s\n", __func__, DRIVER_VERSION);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
    {
        printk(KERN_ERR "%s: No Support for I2C_FUNC_SMBUS_BYTE_DATA\n", __func__);
        return -ENODEV;
    }
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA))
    {
        printk(KERN_ERR "%s: No Support for I2C_FUNC_SMBUS_WORD_DATA\n", __func__);
        return -ENODEV;
    }

	ps_data = kzalloc(sizeof(struct stk3x1x_data),GFP_KERNEL);
	if(!ps_data)
	{
		printk(KERN_ERR "%s: failed to allocate stk3x1x_data\n", __func__);
		return -ENOMEM;
	}
	ps_data->client = client;
	i2c_set_clientdata(client,ps_data);
	mutex_init(&ps_data->io_lock);
	wake_lock_init(&ps_data->ps_wakelock,WAKE_LOCK_SUSPEND, "stk_input_wakelock");

#ifdef STK_POLL_PS
	wake_lock_init(&ps_data->ps_nosuspend_wl,WAKE_LOCK_SUSPEND, "stk_nosuspend_wakelock");
#endif
	if (client->dev.of_node) {
		plat_data = devm_kzalloc(&client->dev,
			sizeof(struct stk3x1x_platform_data), GFP_KERNEL);
		if (!plat_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = stk3x1x_parse_dt(&client->dev, plat_data);
		dev_err(&client->dev,
			"%s: stk3x1x_parse_dt ret=%d\n", __func__, err);
		if (err)
			return err;
	} else
		plat_data = client->dev.platform_data;

	if (!plat_data) {
		dev_err(&client->dev,
			"%s: no stk3x1x platform data!\n", __func__);
		goto err_als_input_allocate;
	}
	ps_data->als_transmittance = plat_data->transmittance;
	ps_data->int_pin = plat_data->int_pin;
	ps_data->use_fir = plat_data->use_fir;

	if (ps_data->als_transmittance == 0) {
		dev_err(&client->dev,
			"%s: Please set als_transmittance\n", __func__);
		goto err_als_input_allocate;
	}

	ps_data->als_input_dev = input_allocate_device();
	if (ps_data->als_input_dev==NULL)
	{
		printk(KERN_ERR "%s: could not allocate als device\n", __func__);
		err = -ENOMEM;
		goto err_als_input_allocate;
	}
	ps_data->ps_input_dev = input_allocate_device();
	if (ps_data->ps_input_dev==NULL)
	{
		printk(KERN_ERR "%s: could not allocate ps device\n", __func__);
		err = -ENOMEM;
		goto err_ps_input_allocate;
	}
	ps_data->als_input_dev->name = ALS_NAME;
	ps_data->ps_input_dev->name = PS_NAME;
	set_bit(EV_ABS, ps_data->als_input_dev->evbit);
	set_bit(EV_ABS, ps_data->ps_input_dev->evbit);
	input_set_abs_params(ps_data->als_input_dev, ABS_MISC, 0, stk_alscode2lux(ps_data, (1<<16)-1), 0, 0);
	input_set_abs_params(ps_data->ps_input_dev, ABS_DISTANCE, 0,1, 0, 0);
	err = input_register_device(ps_data->als_input_dev);
	if (err<0)
	{
		printk(KERN_ERR "%s: can not register als input device\n", __func__);
		goto err_als_input_register;
	}
	err = input_register_device(ps_data->ps_input_dev);
	if (err<0)
	{
		printk(KERN_ERR "%s: can not register ps input device\n", __func__);
		goto err_ps_input_register;
	}

	err = sysfs_create_group(&ps_data->als_input_dev->dev.kobj, &stk_als_attribute_group);
	if (err < 0)
	{
		printk(KERN_ERR "%s:could not create sysfs group for als\n", __func__);
		goto err_als_sysfs_create_group;
	}
	err = sysfs_create_group(&ps_data->ps_input_dev->dev.kobj, &stk_ps_attribute_group);
	if (err < 0)
	{
		printk(KERN_ERR "%s:could not create sysfs group for ps\n", __func__);
		goto err_ps_sysfs_create_group;
	}
	input_set_drvdata(ps_data->als_input_dev, ps_data);
	input_set_drvdata(ps_data->ps_input_dev, ps_data);

#ifdef STK_POLL_ALS
	ps_data->stk_als_wq = create_singlethread_workqueue("stk_als_wq");
	INIT_WORK(&ps_data->stk_als_work, stk_als_work_func);
	hrtimer_init(&ps_data->als_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ps_data->als_poll_delay = ns_to_ktime(110 * NSEC_PER_MSEC);
	ps_data->als_timer.function = stk_als_timer_func;
#endif

	ps_data->stk_ps_wq = create_singlethread_workqueue("stk_ps_wq");
	INIT_WORK(&ps_data->stk_ps_work, stk_ps_work_func);
	hrtimer_init(&ps_data->ps_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ps_data->ps_poll_delay = ns_to_ktime(110 * NSEC_PER_MSEC);
	ps_data->ps_timer.function = stk_ps_timer_func;
#if (!defined(STK_POLL_ALS) || !defined(STK_POLL_PS))
	ps_data->stk_wq = create_singlethread_workqueue("stk_wq");
	INIT_WORK(&ps_data->stk_work, stk_work_func);
	err = stk3x1x_setup_irq(client);
	if(err < 0)
		goto err_stk3x1x_setup_irq;
#endif

	err = stk3x1x_power_init(ps_data, true);
	if (err)
		goto err_power_init;

	err = stk3x1x_power_on(ps_data, true);
	if (err)
		goto err_power_on;

	err = stk3x1x_init_all_setting(client, plat_data);
	if(err < 0)
		goto err_init_all_setting;
#ifdef CONFIG_HAS_EARLYSUSPEND
	ps_data->stk_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ps_data->stk_early_suspend.suspend = stk3x1x_early_suspend;
	ps_data->stk_early_suspend.resume = stk3x1x_late_resume;
	register_early_suspend(&ps_data->stk_early_suspend);
#endif
	printk(KERN_INFO "%s: probe successfully", __func__);
	return 0;

err_init_all_setting:
	stk3x1x_power_on(ps_data, false);
err_power_on:
	stk3x1x_power_init(ps_data, false);
err_power_init:
#ifndef STK_POLL_PS
	free_irq(ps_data->irq, ps_data);
	gpio_free(plat_data->int_pin);
#endif
#if (!defined(STK_POLL_ALS) || !defined(STK_POLL_PS))
err_stk3x1x_setup_irq:
#endif
#ifdef STK_POLL_ALS
	hrtimer_try_to_cancel(&ps_data->als_timer);
	destroy_workqueue(ps_data->stk_als_wq);
#endif
	destroy_workqueue(ps_data->stk_ps_wq);
#if (!defined(STK_POLL_ALS) || !defined(STK_POLL_PS))
	destroy_workqueue(ps_data->stk_wq);
#endif
	sysfs_remove_group(&ps_data->ps_input_dev->dev.kobj, &stk_ps_attribute_group);
err_ps_sysfs_create_group:
	sysfs_remove_group(&ps_data->als_input_dev->dev.kobj, &stk_als_attribute_group);
err_als_sysfs_create_group:
	input_unregister_device(ps_data->ps_input_dev);
err_ps_input_register:
	input_unregister_device(ps_data->als_input_dev);
err_als_input_register:
	input_free_device(ps_data->ps_input_dev);
err_ps_input_allocate:
	input_free_device(ps_data->als_input_dev);
err_als_input_allocate:
#ifdef STK_POLL_PS
    wake_lock_destroy(&ps_data->ps_nosuspend_wl);
#endif
    wake_lock_destroy(&ps_data->ps_wakelock);
    mutex_destroy(&ps_data->io_lock);
	kfree(ps_data);
    return err;
}


static int stk3x1x_remove(struct i2c_client *client)
{
	struct stk3x1x_data *ps_data = i2c_get_clientdata(client);
#ifndef STK_POLL_PS
	free_irq(ps_data->irq, ps_data);
	gpio_free(ps_data->int_pin);
#endif
#ifdef STK_POLL_ALS
	hrtimer_try_to_cancel(&ps_data->als_timer);
	destroy_workqueue(ps_data->stk_als_wq);
#endif
	destroy_workqueue(ps_data->stk_ps_wq);
#if (!defined(STK_POLL_ALS) || !defined(STK_POLL_PS))
	destroy_workqueue(ps_data->stk_wq);
#endif
	sysfs_remove_group(&ps_data->ps_input_dev->dev.kobj, &stk_ps_attribute_group);
	sysfs_remove_group(&ps_data->als_input_dev->dev.kobj, &stk_als_attribute_group);
	input_unregister_device(ps_data->ps_input_dev);
	input_unregister_device(ps_data->als_input_dev);
	input_free_device(ps_data->ps_input_dev);
	input_free_device(ps_data->als_input_dev);
#ifdef STK_POLL_PS
	wake_lock_destroy(&ps_data->ps_nosuspend_wl);
#endif
	wake_lock_destroy(&ps_data->ps_wakelock);
    mutex_destroy(&ps_data->io_lock);
	kfree(ps_data);

    return 0;
}

static const struct i2c_device_id stk_ps_id[] =
{
    { "stk_ps", 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, stk_ps_id);

static struct of_device_id stk_match_table[] = {
	{ .compatible = "stk,stk3x1x", },
	{ },
};

static struct i2c_driver stk_ps_driver =
{
    .driver = {
        .name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = stk_match_table,
    },
    .probe = stk3x1x_probe,
    .remove = stk3x1x_remove,
    .id_table = stk_ps_id,
};


static int __init stk3x1x_init(void)
{
	int ret;
    ret = i2c_add_driver(&stk_ps_driver);
    if (ret)
        return ret;

    return 0;
}

static void __exit stk3x1x_exit(void)
{
    i2c_del_driver(&stk_ps_driver);
}

module_init(stk3x1x_init);
module_exit(stk3x1x_exit);
MODULE_AUTHOR("Lex Hsieh <lex_hsieh@sitronix.com.tw>");
MODULE_DESCRIPTION("Sensortek stk3x1x Proximity Sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

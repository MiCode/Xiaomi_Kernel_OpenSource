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
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include <alsps.h>
#ifdef CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif

#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include "cm36283.h"
#include <linux/sched.h>
/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define CM36283_DEV_NAME     "cm36283"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_ERR APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)    

#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1

/******************************************************************************
 * extern functions
*******************************************************************************/
#ifdef CUST_EINT_ALS_TYPE
extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
extern void mt_eint_print_status(void);
#else
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt65xx_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt65xx_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt65xx_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
#endif
/*----------------------------------------------------------------------------*/
static int cm36283_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int cm36283_i2c_remove(struct i2c_client *client);
static int cm36283_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int cm36283_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int cm36283_i2c_resume(struct i2c_client *client);

static int cm36283_local_init(void);
static int cm36283_remove(void);
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id cm36283_i2c_id[] = {{CM36283_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_cm36283={ I2C_BOARD_INFO(CM36283_DEV_NAME, 0x60)};
static unsigned long long int_top_time = 0;
/*----------------------------------------------------------------------------*/
struct cm36283_priv {
	struct alsps_hw  *hw;
	struct i2c_client *client;
	struct work_struct	eint_work;
#ifdef CUSTOM_KERNEL_SENSORHUB
    struct work_struct init_done_work;
#endif

	/*misc*/
	u16 		als_modulus;
	atomic_t	i2c_retry;
	atomic_t	als_suspend;
	atomic_t	als_debounce;	/*debounce time after enabling als*/
	atomic_t	als_deb_on; 	/*indicates if the debounce is on*/
	atomic_t	als_deb_end;	/*the jiffies representing the end of debounce*/
	atomic_t	ps_mask;		/*mask ps: always return far away*/
	atomic_t	ps_debounce;	/*debounce time after enabling ps*/
	atomic_t	ps_deb_on;		/*indicates if the debounce is on*/
	atomic_t	ps_deb_end; 	/*the jiffies representing the end of debounce*/
	atomic_t	ps_suspend;
	atomic_t 	trace;
	
	
	/*data*/
	u32			als;
	u32 		ps;
	//u8			_align;
	u16			als_level_num;
	u16			als_value_num;
	u32			als_level[C_CUST_ALS_LEVEL-1];
	u32			als_value[C_CUST_ALS_LEVEL];
	int			ps_cali;
	
	atomic_t	als_cmd_val;	/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_cmd_val; 	/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val_high;	 /*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val_low; 	/*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_high;	 /*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_low; 	/*the cmd value can't be read, stored in ram*/
	atomic_t	ps_thd_val;
	ulong		enable; 		/*enable mask*/
	ulong		pending_intr;	/*pending interrupt*/
	
	/*early suspend*/
	#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend	early_drv;
	#endif     
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver cm36283_i2c_driver = {	
	.probe      = cm36283_i2c_probe,
	.remove     = cm36283_i2c_remove,
	//.detect     = cm36283_i2c_detect,
	.suspend    = cm36283_i2c_suspend,
	.resume     = cm36283_i2c_resume,
	.id_table   = cm36283_i2c_id,
	.driver = {
		.name = CM36283_DEV_NAME,
	},
};

/*----------------------------------------------------------------------------*/
struct PS_CALI_DATA_STRUCT
{
	int close;
	int far_away;
	int valid;
};

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct i2c_client *cm36283_i2c_client = NULL;
static struct cm36283_priv *g_cm36283_ptr = NULL;
static struct cm36283_priv *cm36283_obj = NULL;

static int cm36283_init_flag =-1; // 0<==>OK -1 <==> fail

static struct alsps_init_info cm36283_init_info = {
		.name = "cm36283",
		.init = cm36283_local_init,
		.uninit = cm36283_remove,
	
};
/*----------------------------------------------------------------------------*/

static DEFINE_MUTEX(cm36283_mutex);


/*----------------------------------------------------------------------------*/
typedef enum {
	CMC_BIT_ALS    = 1,
	CMC_BIT_PS	   = 2,
}CMC_BIT;
/*-----------------------------CMC for debugging-------------------------------*/
typedef enum {
    CMC_TRC_ALS_DATA= 0x0001,
    CMC_TRC_PS_DATA = 0x0002,
    CMC_TRC_EINT    = 0x0004,
    CMC_TRC_IOCTL   = 0x0008,
    CMC_TRC_I2C     = 0x0010,
    CMC_TRC_CVT_ALS = 0x0020,
    CMC_TRC_CVT_PS  = 0x0040,
    CMC_TRC_DEBUG   = 0x8000,
} CMC_TRC;
/*-----------------------------------------------------------------------------*/
#ifndef CUSTOM_KERNEL_SENSORHUB
static void cm36283_eint_unmask(void)
{
#ifdef CUST_EINT_ALS_NUM
#ifdef CUST_EINT_ALS_TYPE
	mt_eint_unmask(CUST_EINT_ALS_NUM);
#else
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
#endif
#else
    APS_ERR("CUST_EINT_ALS_NUM undefined\n");
#endif
}
#endif
/*-----------------------------------------------------------------------------*/
#ifndef CUSTOM_KERNEL_SENSORHUB
static int cm36283_enable_eint(struct i2c_client *client)
{
	//struct cm36283_priv *obj = i2c_get_clientdata(client);        

	//g_cm36283_ptr = obj;

#ifdef GPIO_ALS_EINT_PIN
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP); 
#else
    APS_ERR("GPIO_ALS_EINT_PIN undefined\n");
#endif
	
    cm36283_eint_unmask();

    return 0;
}
#endif//#ifndef CUSTOM_KERNEL_SENSORHUB
#ifndef CUSTOM_KERNEL_SENSORHUB
static int cm36283_disable_eint(struct i2c_client *client)
{
	//struct cm36283_priv *obj = i2c_get_clientdata(client);        

	//g_cm36283_ptr = obj;

#ifdef GPIO_ALS_EINT_PIN
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, FALSE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_DOWN); 
#else
    APS_ERR("GPIO_ALS_EINT_PIN undefined\n");
#endif

#ifdef CUST_EINT_ALS_NUM
#ifdef CUST_EINT_ALS_TYPE
    mt_eint_mask(CUST_EINT_ALS_NUM);
#else
    mt65xx_eint_mask(CUST_EINT_ALS_NUM);
#endif
#else
    APS_ERR("CUST_EINT_ALS_NUM undefined\n");
#endif

    return 0;
}
#endif//#ifndef CUSTOM_KERNEL_SENSORHUB
int CM36283_i2c_master_operate(struct i2c_client *client, const char *buf, int count, int i2c_flag)
{
	int res = 0;
	mutex_lock(&cm36283_mutex);
	switch(i2c_flag){	
	case I2C_FLAG_WRITE:
	client->addr &=I2C_MASK_FLAG;
	res = i2c_master_send(client, buf, count);
	client->addr &=I2C_MASK_FLAG;
	break;
	
	case I2C_FLAG_READ:
	client->addr &=I2C_MASK_FLAG;
	client->addr |=I2C_WR_FLAG;
	client->addr |=I2C_RS_FLAG;
	res = i2c_master_send(client, buf, count);
	client->addr &=I2C_MASK_FLAG;
	break;
	default:
	APS_LOG("CM36283_i2c_master_operate i2c_flag command not support!\n");
	break;
	}
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	mutex_unlock(&cm36283_mutex);
	return res;
	EXIT_ERR:
	mutex_unlock(&cm36283_mutex);
	APS_ERR("CM36283_i2c_master_operate fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/
static void cm36283_power(struct alsps_hw *hw, unsigned int on) 
{
#ifndef FPGA_EARLY_PORTING
	static unsigned int power_on = 0;

	APS_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APS_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "CM36283")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "CM36283")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
#endif
}
/********************************************************************/
int cm36283_enable_ps(struct i2c_client *client, int enable)
{
    struct cm36283_priv *obj = i2c_get_clientdata(client);
    int res;
#ifdef CUSTOM_KERNEL_SENSORHUB
    SCP_SENSOR_HUB_DATA req;
    int len;
#else
    u8 databuf[3];
#endif

    if(enable == 1)
    {
        APS_LOG("cm36283_enable_ps enable_ps\n");
#ifdef CUSTOM_KERNEL_SENSORHUB
        req.activate_req.sensorType = ID_PROXIMITY;
        req.activate_req.action = SENSOR_HUB_ACTIVATE;
        req.activate_req.enable = enable;
        len = sizeof(req.activate_req);
        res = SCP_sensorHub_req_send(&req, &len, 1);
        if (res)
        {
            APS_ERR("SCP_sensorHub_req_send!\n");
            goto ENABLE_PS_EXIT_ERR;
        }
#else
        res = cm36283_enable_eint(client);
        if(res!=0)
        {
            APS_ERR("disable eint fail: %d\n", res);
            return res;
        }	

        databuf[0]= CM36283_REG_PS_CONF3_MS;
        res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_PS_EXIT_ERR;
        }
        APS_LOG("CM36283_REG_PS_CONF3_MS value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);

        databuf[0]= CM36283_REG_PS_CANC;
        res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_PS_EXIT_ERR;
        }
        APS_LOG("CM36283_REG_PS_CANC value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);

        databuf[0]= CM36283_REG_PS_CONF1_2;
        res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_PS_EXIT_ERR;
        }
        APS_LOG("CM36283_REG_PS_CONF1_2 value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);

        databuf[2] = databuf[1];
        databuf[1] = databuf[0]&0xFE;

        databuf[0]= CM36283_REG_PS_CONF1_2;
        res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_PS_EXIT_ERR;
        }
#endif
        atomic_set(&obj->ps_deb_on, 1);
        atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
    }
    else{
        APS_LOG("cm36283_enable_ps disable_ps\n");
#ifdef CUSTOM_KERNEL_SENSORHUB
        req.activate_req.sensorType = ID_PROXIMITY;
        req.activate_req.action = SENSOR_HUB_ACTIVATE;
        req.activate_req.enable = enable;
        len = sizeof(req.activate_req);
        res = SCP_sensorHub_req_send(&req, &len, 1);
        if (res)
        {
            APS_ERR("SCP_sensorHub_req_send!\n");
            goto ENABLE_PS_EXIT_ERR;
        }
#else
        databuf[0]= CM36283_REG_PS_CONF1_2;
        res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_PS_EXIT_ERR;
        }

        APS_LOG("CM36283_REG_PS_CONF1_2 value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);

        databuf[2] = databuf[1];
        databuf[1] = databuf[0]|0x01;	
        databuf[0]= CM36283_REG_PS_CONF1_2;

        res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_PS_EXIT_ERR;
        }
        
        res = cm36283_disable_eint(client);
        if(res!=0)
        {
            APS_ERR("disable eint fail: %d\n", res);
            return res;
        }
#endif
        atomic_set(&obj->ps_deb_on, 0);
    }

    return 0;
    ENABLE_PS_EXIT_ERR:
    return res;
}
/********************************************************************/
int cm36283_enable_als(struct i2c_client *client, int enable)
{
	struct cm36283_priv *obj = i2c_get_clientdata(client);
	int res;
#ifdef CUSTOM_KERNEL_SENSORHUB
    SCP_SENSOR_HUB_DATA req;
    int len;
#else
	u8 databuf[3];
#endif

    if(enable == 1)
    {
        APS_LOG("cm36283_enable_als enable_als\n");
#ifdef CUSTOM_KERNEL_SENSORHUB
        req.activate_req.sensorType = ID_PROXIMITY;
        req.activate_req.action = SENSOR_HUB_ACTIVATE;
        req.activate_req.enable = enable;
        len = sizeof(req.activate_req);
        res = SCP_sensorHub_req_send(&req, &len, 1);
        if (res)
        {
            APS_ERR("SCP_sensorHub_req_send!\n");
            goto ENABLE_ALS_EXIT_ERR;
        }
#else
        databuf[0] = CM36283_REG_ALS_CONF;
        res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_ALS_EXIT_ERR;
        }

        APS_LOG("CM36283_REG_ALS_CONF value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);

        databuf[2] = databuf[1];
        databuf[1] = databuf[0]&0xFE;		
        databuf[0] = CM36283_REG_ALS_CONF;
        client->addr &=I2C_MASK_FLAG;

        res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_ALS_EXIT_ERR;
        }
#endif
        atomic_set(&obj->als_deb_on, 1);
        atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
    }
    else{
        APS_LOG("cm36283_enable_als disable_als\n");
#ifdef CUSTOM_KERNEL_SENSORHUB
        req.activate_req.sensorType = ID_PROXIMITY;
        req.activate_req.action = SENSOR_HUB_ACTIVATE;
        req.activate_req.enable = enable;
        len = sizeof(req.activate_req);
        res = SCP_sensorHub_req_send(&req, &len, 1);
        if (res)
        {
            APS_ERR("SCP_sensorHub_req_send!\n");
            goto ENABLE_ALS_EXIT_ERR;
        }
#else
        databuf[0] = CM36283_REG_ALS_CONF;
        res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_ALS_EXIT_ERR;
        }

        APS_LOG("CM36283_REG_ALS_CONF value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);

        databuf[2] = databuf[1];
        databuf[1] = databuf[0]|0x01;
        databuf[0] = CM36283_REG_ALS_CONF;
        client->addr &=I2C_MASK_FLAG;

        res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
        if(res < 0)
        {
            APS_ERR("i2c_master_send function err\n");
            goto ENABLE_ALS_EXIT_ERR;
        }
#endif
        atomic_set(&obj->als_deb_on, 0);
    }
	return 0;
	ENABLE_ALS_EXIT_ERR:
	return res;
}
/********************************************************************/
long cm36283_read_ps(struct i2c_client *client, u32 *data)
{
	long res;
    struct cm36283_priv *obj = i2c_get_clientdata(client);
#ifdef CUSTOM_KERNEL_SENSORHUB
    SCP_SENSOR_HUB_DATA req;
    int len;
#else
	u8 databuf[2];
#endif
	//APS_FUN(f);

#ifdef CUSTOM_KERNEL_SENSORHUB
    req.get_data_req.sensorType = ID_PROXIMITY;
    req.get_data_req.action = SENSOR_HUB_GET_DATA;
    len = sizeof(req.get_data_req);
    res = SCP_sensorHub_req_send(&req, &len, 1);
    if (res)
    {
        APS_ERR("SCP_sensorHub_req_send!\n");
        goto READ_PS_EXIT_ERR;
    }

    len -= offsetof(SCP_SENSOR_HUB_GET_DATA_RSP, u8Data);

    if (len >= 0 && len < 4)
    {
        memcpy(data, &(req.get_data_rsp.u8Data), len);
    }
    else
    {
        APS_ERR("data length fail : %d\n", len);
    }

    if(atomic_read(&obj->trace) & CMC_TRC_PS_DATA)
	{
        //show data
	}
#else
	databuf[0] = CM36283_REG_PS_DATA;
	res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		APS_ERR("i2c_master_send function err\n");
		goto READ_PS_EXIT_ERR;
	}
	
	//APS_LOG("CM36283_REG_PS_DATA value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);

	if(databuf[0] < obj->ps_cali)
		*data = 0;
	else
		*data = databuf[0] - obj->ps_cali;
#endif

	return 0;
	READ_PS_EXIT_ERR:
	return res;
}
/********************************************************************/
long cm36283_read_als(struct i2c_client *client, u32 *data)
{
	long res;
#ifdef CUSTOM_KERNEL_SENSORHUB
    struct cm36283_priv *obj = i2c_get_clientdata(client);
    SCP_SENSOR_HUB_DATA req;
    int len;
#else
	u8 databuf[2];
#endif

	//APS_FUN(f);

#ifdef CUSTOM_KERNEL_SENSORHUB
    req.get_data_req.sensorType = ID_LIGHT;
    req.get_data_req.action = SENSOR_HUB_GET_DATA;
    len = sizeof(req.get_data_req);
    res = SCP_sensorHub_req_send(&req, &len, 1);
    if (res)
    {
        APS_ERR("SCP_sensorHub_req_send!\n");
        goto READ_ALS_EXIT_ERR;
    }

    len -= offsetof(SCP_SENSOR_HUB_GET_DATA_RSP, u8Data);

    if (len >= 0 && len < 4)
    {
        memcpy(data, &(req.get_data_rsp.u8Data), len);
    }
    else
    {
        APS_ERR("data length fail : %d\n", len);
    }

    if(atomic_read(&obj->trace) & CMC_TRC_ALS_DATA)
	{
        //show data
	}
#else
	databuf[0] = CM36283_REG_ALS_DATA;
	res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		APS_ERR("i2c_master_send function err\n");
		goto READ_ALS_EXIT_ERR;
	}
	
	//APS_LOG("CM36283_REG_ALS_DATA value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);

	*data = ((databuf[1]<<8)|databuf[0]);
#endif
    
	return 0;
	READ_ALS_EXIT_ERR:
	return res;
}
/********************************************************************/
static int cm36283_get_ps_value(struct cm36283_priv *obj, u32 ps)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
    return ps;
#else
	int val, mask = atomic_read(&obj->ps_mask);
	int invalid = 0;
	val = 0;

	if(ps > atomic_read(&obj->ps_thd_val_high))
	{
		val = 0;  /*close*/
	}
	else if(ps < atomic_read(&obj->ps_thd_val_low))
	{
		val = 1;  /*far away*/
	}
	
	if(atomic_read(&obj->ps_suspend))
	{
		invalid = 1;
	}
	else if(1 == atomic_read(&obj->ps_deb_on))
	{
		unsigned long endt = atomic_read(&obj->ps_deb_end);
		if(time_after(jiffies, endt))
		{
			atomic_set(&obj->ps_deb_on, 0);
		}
		
		if (1 == atomic_read(&obj->ps_deb_on))
		{
			invalid = 1;
		}
	}

	if(!invalid)
	{
		if(unlikely(atomic_read(&obj->trace) & CMC_TRC_CVT_PS))
		{
			if(mask)
			{
				APS_DBG("PS:  %05d => %05d [M] \n", ps, val);
			}
			else
			{
				APS_DBG("PS:  %05d => %05d\n", ps, val);
			}
		}
		if(0 == test_bit(CMC_BIT_PS,  &obj->enable))
		{
		  //if ps is disable do not report value
		  APS_DBG("PS: not enable and do not report this value\n");
		  return -1;
		}
		else
		{
		   return val;
		}
		
	}	
	else
	{
		if(unlikely(atomic_read(&obj->trace) & CMC_TRC_CVT_PS))
		{
			APS_DBG("PS:  %05d => %05d (-1)\n", ps, val);    
		}
		return -1;
	}	
#endif
}
/********************************************************************/
static int cm36283_get_als_value(struct cm36283_priv *obj, u32 als)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
    return als;
#else
		int idx;
		int invalid = 0;
		for(idx = 0; idx < obj->als_level_num; idx++)
		{
			if(als < obj->hw->als_level[idx])
			{
				break;
			}
		}
		if(idx >= obj->als_value_num)
		{
			APS_ERR("exceed range\n"); 
			idx = obj->als_value_num - 1;
		}
		
		if(1 == atomic_read(&obj->als_deb_on))
		{
			unsigned long endt = atomic_read(&obj->als_deb_end);
			if(time_after(jiffies, endt))
			{
				atomic_set(&obj->als_deb_on, 0);
			}
			
			if(1 == atomic_read(&obj->als_deb_on))
			{
				invalid = 1;
			}
		}
	
		if(!invalid)
		{
		#if defined(CONFIG_MTK_AAL_SUPPORT)
      	int level_high = obj->hw->als_level[idx];
    		int level_low = (idx > 0) ? obj->hw->als_level[idx-1] : 0;
        int level_diff = level_high - level_low;
				int value_high = obj->hw->als_value[idx];
        int value_low = (idx > 0) ? obj->hw->als_value[idx-1] : 0;
        int value_diff = value_high - value_low;
        int value = 0;
        
        if ((level_low >= level_high) || (value_low >= value_high))
            value = value_low;
        else
            value = (level_diff * value_low + (als - level_low) * value_diff + ((level_diff + 1) >> 1)) / level_diff;

		APS_DBG("ALS: %d [%d, %d] => %d [%d, %d] \n", als, level_low, level_high, value, value_low, value_high);
		return value;
		#endif
			if (atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)
			{
				APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
			}
			
			return obj->hw->als_value[idx];
		}
		else
		{
			if(atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)
			{
				APS_DBG("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);	  
			}
			return -1;
		}
#endif
}


/*-------------------------------attribute file for debugging----------------------------------*/

/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
static ssize_t cm36283_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "(%d %d %d %d %d)\n", 
		atomic_read(&cm36283_obj->i2c_retry), atomic_read(&cm36283_obj->als_debounce), 
		atomic_read(&cm36283_obj->ps_mask), atomic_read(&cm36283_obj->ps_thd_val), atomic_read(&cm36283_obj->ps_debounce));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_store_config(struct device_driver *ddri, const char *buf, size_t count)
{
	int retry, als_deb, ps_deb, mask, thres;
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	
	if(5 == sscanf(buf, "%d %d %d %d %d", &retry, &als_deb, &mask, &thres, &ps_deb))
	{ 
		atomic_set(&cm36283_obj->i2c_retry, retry);
		atomic_set(&cm36283_obj->als_debounce, als_deb);
		atomic_set(&cm36283_obj->ps_mask, mask);
		atomic_set(&cm36283_obj->ps_thd_val, thres);        
		atomic_set(&cm36283_obj->ps_debounce, ps_deb);
	}
	else
	{
		APS_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&cm36283_obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_store_trace(struct device_driver *ddri, const char *buf, size_t count)
{
    int trace;
    if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&cm36283_obj->trace, trace);
	}
	else 
	{
		APS_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_als(struct device_driver *ddri, char *buf)
{
	int res;
	
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	if((res = cm36283_read_als(cm36283_obj->client, &cm36283_obj->als)))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}
	else
	{
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", cm36283_obj->als);     
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_ps(struct device_driver *ddri, char *buf)
{
	int res;
	if(!cm36283_obj)
	{
		APS_ERR("cm3623_obj is null!!\n");
		return 0;
	}
	
	if((res = cm36283_read_ps(cm36283_obj->client, &cm36283_obj->ps)))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}
	else
	{
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", cm36283_obj->ps);     
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_reg(struct device_driver *ddri, char *buf)
{
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_send(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_store_send(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr, cmd;
	u8 dat;

	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	else if(2 != sscanf(buf, "%x %x", &addr, &cmd))
	{
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8)cmd;
	//****************************
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_recv(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_store_recv(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr;
	//u8 dat;
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	else if(1 != sscanf(buf, "%x", &addr))
	{
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	//****************************
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	
	if(cm36283_obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d)\n", 
			cm36283_obj->hw->i2c_num, cm36283_obj->hw->power_id, cm36283_obj->hw->power_vol);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	
	len += snprintf(buf+len, PAGE_SIZE-len, "REGS: %02X %02X %02X %02lX %02lX\n", 
				atomic_read(&cm36283_obj->als_cmd_val), atomic_read(&cm36283_obj->ps_cmd_val), 
				atomic_read(&cm36283_obj->ps_thd_val),cm36283_obj->enable, cm36283_obj->pending_intr);
	
	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d %d\n", atomic_read(&cm36283_obj->als_suspend), atomic_read(&cm36283_obj->ps_suspend));

	return len;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct cm36283_priv *obj, const char* buf, size_t count, u32 data[], int len)
{
	int idx = 0;
	char *cur = (char*)buf, *end = (char*)(buf+count);

	while(idx < len)
	{
		while((cur < end) && IS_SPACE(*cur))
		{
			cur++;        
		}

		if(1 != sscanf(cur, "%d", &data[idx]))
		{
			break;
		}

		idx++; 
		while((cur < end) && !IS_SPACE(*cur))
		{
			cur++;
		}
	}
	return idx;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	
	for(idx = 0; idx < cm36283_obj->als_level_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", cm36283_obj->hw->als_level[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;    
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_store_alslv(struct device_driver *ddri, const char *buf, size_t count)
{
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(cm36283_obj->als_level, cm36283_obj->hw->als_level, sizeof(cm36283_obj->als_level));
	}
	else if(cm36283_obj->als_level_num != read_int_from_buf(cm36283_obj, buf, count, 
			cm36283_obj->hw->als_level, cm36283_obj->als_level_num))
	{
		APS_ERR("invalid format: '%s'\n", buf);
	}    
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	
	for(idx = 0; idx < cm36283_obj->als_value_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", cm36283_obj->hw->als_value[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;    
}
/*----------------------------------------------------------------------------*/
static ssize_t cm36283_store_alsval(struct device_driver *ddri, const char *buf, size_t count)
{
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(cm36283_obj->als_value, cm36283_obj->hw->als_value, sizeof(cm36283_obj->als_value));
	}
	else if(cm36283_obj->als_value_num != read_int_from_buf(cm36283_obj, buf, count, 
			cm36283_obj->hw->als_value, cm36283_obj->als_value_num))
	{
		APS_ERR("invalid format: '%s'\n", buf);
	}    
	return count;
}
/*---------------------------------------------------------------------------------------*/
static DRIVER_ATTR(als,     S_IWUSR | S_IRUGO, cm36283_show_als, NULL);
static DRIVER_ATTR(ps,      S_IWUSR | S_IRUGO, cm36283_show_ps, NULL);
static DRIVER_ATTR(config,  S_IWUSR | S_IRUGO, cm36283_show_config,	cm36283_store_config);
static DRIVER_ATTR(alslv,   S_IWUSR | S_IRUGO, cm36283_show_alslv, cm36283_store_alslv);
static DRIVER_ATTR(alsval,  S_IWUSR | S_IRUGO, cm36283_show_alsval, cm36283_store_alsval);
static DRIVER_ATTR(trace,   S_IWUSR | S_IRUGO, cm36283_show_trace,		cm36283_store_trace);
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, cm36283_show_status, NULL);
static DRIVER_ATTR(send,    S_IWUSR | S_IRUGO, cm36283_show_send, cm36283_store_send);
static DRIVER_ATTR(recv,    S_IWUSR | S_IRUGO, cm36283_show_recv, cm36283_store_recv);
static DRIVER_ATTR(reg,     S_IWUSR | S_IRUGO, cm36283_show_reg, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *cm36283_attr_list[] = {
    &driver_attr_als,
    &driver_attr_ps,    
    &driver_attr_trace,        /*trace log*/
    &driver_attr_config,
    &driver_attr_alslv,
    &driver_attr_alsval,
    &driver_attr_status,
    &driver_attr_send,
    &driver_attr_recv,
    &driver_attr_reg,
};

/*----------------------------------------------------------------------------*/
static int cm36283_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(cm36283_attr_list)/sizeof(cm36283_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, cm36283_attr_list[idx])))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", cm36283_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
	static int cm36283_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)(sizeof(cm36283_attr_list)/sizeof(cm36283_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, cm36283_attr_list[idx]);
	}
	
	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------interrupt functions--------------------------------*/
static int intr_flag = 0;
/*----------------------------------------------------------------------------*/
#ifndef CUSTOM_KERNEL_SENSORHUB
static int cm36283_check_intr(struct i2c_client *client) 
{
	int res;
	u8 databuf[2];
	//u8 intr;
	
	databuf[0] = CM36283_REG_PS_DATA;
	res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
	if(res<0)
	{
		APS_ERR("i2c_master_send function err res = %d\n",res);
		goto EXIT_ERR;
	}

	APS_LOG("CM36283_REG_PS_DATA value value_low = %x, value_reserve = %x\n",databuf[0],databuf[1]);
	
	databuf[0] = CM36283_REG_INT_FLAG;
	res = CM36283_i2c_master_operate(client, databuf, 0x201, I2C_FLAG_READ);
	if(res<0)
	{
		APS_ERR("i2c_master_send function err res = %d\n",res);
		goto EXIT_ERR;
	}
	
	APS_LOG("CM36283_REG_INT_FLAG value value_low = %x, value_high = %x\n",databuf[0],databuf[1]);
	
	if(databuf[1]&0x02)
	{
		intr_flag = 0;//for close
	}else if(databuf[1]&0x01)
	{
		intr_flag = 1;//for away
	}else{
		res = -1;
		APS_ERR("cm36283_check_intr fail databuf[1]&0x01: %d\n", res);
		goto EXIT_ERR;
	}
	
	return 0;
	EXIT_ERR:
	APS_ERR("cm36283_check_intr dev: %d\n", res);
	return res;
}
#endif//#ifndef CUSTOM_KERNEL_SENSORHUB
/*----------------------------------------------------------------------------*/
static void cm36283_eint_work(struct work_struct *work)
{
    int res = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
#else
	struct cm36283_priv *obj = (struct cm36283_priv *)container_of(work, struct cm36283_priv, eint_work);
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB
	
	APS_LOG("cm36283 int top half time = %lld\n", int_top_time);

#ifdef CUSTOM_KERNEL_SENSORHUB
    res = ps_report_interrupt_data(intr_flag);
    if(res != 0)
    {
        APS_ERR("cm36283_eint_work err: %d\n", res);
    }
#else
	res = cm36283_check_intr(obj->client);
	if(res != 0){
		goto EXIT_INTR_ERR;
	}else{
	    APS_LOG("cm36283 interrupt value = %d\n", intr_flag);
		res = ps_report_interrupt_data(intr_flag);
	}


    cm36283_eint_unmask();
	return;
	EXIT_INTR_ERR:
    cm36283_eint_unmask();
    APS_ERR("cm36283_eint_work err: %d\n", res);
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB
}
/*----------------------------------------------------------------------------*/
static void cm36283_init_done_work(struct work_struct *work)
{
    struct cm36283_priv *obj = cm36283_obj;
    CM36283_CUST_DATA *p_cust_data;
    SCP_SENSOR_HUB_DATA data;
    int max_cust_data_size_per_packet;
    int i;
    uint sizeOfCustData;
    uint len;
    char *p = (char *)obj->hw;

    p_cust_data = (CM36283_CUST_DATA *)data.set_cust_req.custData;

    data.set_cust_req.sensorType = ID_LIGHT;
    data.set_cust_req.action = SENSOR_HUB_SET_CUST;
    sizeOfCustData = sizeof(*(obj->hw));
    p_cust_data->setCust.action = CM36283_CUST_ACTION_SET_CUST;
    max_cust_data_size_per_packet = sizeof(data.set_cust_req.custData) - offsetof(CM36283_SET_CUST, data);
    
    for (i=0;sizeOfCustData>0;i++)
    {
        p_cust_data->setCust.part = i;
        if (sizeOfCustData > max_cust_data_size_per_packet)
        {
            len = max_cust_data_size_per_packet;
        }
        else
        {
            len = sizeOfCustData;
        }

        memcpy(p_cust_data->setCust.data, p, len);
        sizeOfCustData -= len;
        p += len;
        
        len += offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + offsetof(CM36283_SET_CUST, data);
        SCP_sensorHub_req_send(&data, &len, 1);
    }
}
/*----------------------------------------------------------------------------*/
#ifndef CUSTOM_KERNEL_SENSORHUB
static void cm36283_eint_func(void)
{
	struct cm36283_priv *obj = g_cm36283_ptr;
	if(!obj)
	{
		return;
	}	
	int_top_time = sched_clock();
	schedule_work(&obj->eint_work);
}
#else
static int cm36283_irq_handler(void* data, uint len)
{
	struct cm36283_priv *obj = cm36283_obj;
    SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;
    
	if(!obj)
	{
		return -1;
	}

    switch(rsp->rsp.action)
    {
        case SENSOR_HUB_NOTIFY:
            switch(rsp->notify_rsp.event)
            {
                case SCP_INIT_DONE:
                    schedule_work(&obj->init_done_work);
                    break;
                case SCP_NOTIFY:
                    if (CM36283_NOTIFY_PROXIMITY_CHANGE == rsp->notify_rsp.data[0])
                    {
                        intr_flag = rsp->notify_rsp.data[1];
                        schedule_work(&obj->eint_work);
                    }
                    else
                    {
                        APS_ERR("Unknow notify");
                    }
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
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB
/*----------------------------------------------------------------------------*/
int cm36283_setup_eint(struct i2c_client *client)
{
    int err = 0;
	struct cm36283_priv *obj = i2c_get_clientdata(client);        

	g_cm36283_ptr = obj;

#ifdef CUSTOM_KERNEL_SENSORHUB
    err = SCP_sensorHub_rsp_registration(ID_PROXIMITY, cm36283_irq_handler);
#else	
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

#ifdef CUST_EINT_ALS_NUM
#ifdef CUST_EINT_ALS_TYPE
	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, cm36283_eint_func, 0);
#else
	mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_ALS_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_ALS_POLARITY, cm36283_eint_func, 0);
#endif
#else
    APS_ERR("CUST_EINT_ALS_NUM undefined\n");
#endif

    cm36283_eint_unmask();
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB
    return err;
}
/*-------------------------------MISC device related------------------------------------------*/



/************************************************************/
static int cm36283_open(struct inode *inode, struct file *file)
{
	file->private_data = cm36283_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/************************************************************/
static int cm36283_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/************************************************************/
#ifdef CUSTOM_KERNEL_SENSORHUB
static int set_psensor_threshold(struct i2c_client *client)
{
	struct cm36283_priv *obj = i2c_get_clientdata(client);
	u8 databuf[3];    
	int res = 0;
	APS_ERR("set_psensor_threshold function high: 0x%x, low:0x%x\n",atomic_read(&obj->ps_thd_val_high),atomic_read(&obj->ps_thd_val_low));
	databuf[0] = CM36283_REG_PS_THD;
	databuf[1] = atomic_read(&obj->ps_thd_val_low);
	databuf[2] = atomic_read(&obj->ps_thd_val_high);//threshold value need to confirm
	res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err\n");
		return -1;
	}
	return 0;
}
#endif
static long cm36283_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		struct i2c_client *client = (struct i2c_client*)file->private_data;
		struct cm36283_priv *obj = i2c_get_clientdata(client);  
		long err = 0;
		void __user *ptr = (void __user*) arg;
		int dat;
		uint32_t enable;
		int ps_result;
		int ps_cali;
		int threshold[2];
        SCP_SENSOR_HUB_DATA data;
        CM36283_CUST_DATA *pCustData;
        int len;

        data.set_cust_req.sensorType = ID_PROXIMITY;
        data.set_cust_req.action = SENSOR_HUB_SET_CUST;
        pCustData = (CM36283_CUST_DATA *)(&data.set_cust_req.custData);
		
		switch (cmd)
		{
			case ALSPS_SET_PS_MODE:
				if(copy_from_user(&enable, ptr, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				if(enable)
				{
					if((err = cm36283_enable_ps(obj->client, 1)))
					{
						APS_ERR("enable ps fail: %ld\n", err); 
						goto err_out;
					}
					
					set_bit(CMC_BIT_PS, &obj->enable);
				}
				else
				{
					if((err = cm36283_enable_ps(obj->client, 0)))
					{
						APS_ERR("disable ps fail: %ld\n", err); 
						goto err_out;
					}
					clear_bit(CMC_BIT_PS, &obj->enable);
				}
				break;
	
			case ALSPS_GET_PS_MODE:
				enable = test_bit(CMC_BIT_PS, &obj->enable) ? (1) : (0);
				if(copy_to_user(ptr, &enable, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;
	
			case ALSPS_GET_PS_DATA:    
				if((err = cm36283_read_ps(obj->client, &obj->ps)))
				{
					goto err_out;
				}
				
				dat = cm36283_get_ps_value(obj, obj->ps);
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}  
				break;
	
			case ALSPS_GET_PS_RAW_DATA:    
				if((err = cm36283_read_ps(obj->client, &obj->ps)))
				{
					goto err_out;
				}
				
				dat = obj->ps;
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}  
				break;			  
	
			case ALSPS_SET_ALS_MODE:
	
				if(copy_from_user(&enable, ptr, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				if(enable)
				{
					if((err = cm36283_enable_als(obj->client, 1)))
					{
						APS_ERR("enable als fail: %ld\n", err); 
						goto err_out;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					if((err = cm36283_enable_als(obj->client, 0)))
					{
						APS_ERR("disable als fail: %ld\n", err); 
						goto err_out;
					}
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}
				break;
	
			case ALSPS_GET_ALS_MODE:
				enable = test_bit(CMC_BIT_ALS, &obj->enable) ? (1) : (0);
				if(copy_to_user(ptr, &enable, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;
	
			case ALSPS_GET_ALS_DATA: 
				if((err = cm36283_read_als(obj->client, &obj->als)))
				{
					goto err_out;
				}
	
				dat = cm36283_get_als_value(obj, obj->als);
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}			   
				break;
	
			case ALSPS_GET_ALS_RAW_DATA:	
				if((err = cm36283_read_als(obj->client, &obj->als)))
				{
					goto err_out;
				}
	
				dat = obj->als;
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}			   
				break;

			/*----------------------------------for factory mode test---------------------------------------*/
			case ALSPS_GET_PS_TEST_RESULT:
				if((err = cm36283_read_ps(obj->client, &obj->ps)))
				{
					goto err_out;
				}
				if(obj->ps > atomic_read(&obj->ps_thd_val_high))
					{
						ps_result = 0;
					}
				else	ps_result = 1;
				
				if(copy_to_user(ptr, &ps_result, sizeof(ps_result)))
				{
					err = -EFAULT;
					goto err_out;
				}			   
				break;

			case ALSPS_IOCTL_CLR_CALI:
				if(copy_from_user(&dat, ptr, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}
				if(dat == 0)
					obj->ps_cali = 0;

#ifdef CUSTOM_KERNEL_SENSORHUB
                pCustData->clearCali.action = CM36283_CUST_ACTION_CLR_CALI;
                len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->clearCali);
                
                err = SCP_sensorHub_req_send(&data, &len, 1);
#endif
                
				break;

			case ALSPS_IOCTL_GET_CALI:
				ps_cali = obj->ps_cali ;
				if(copy_to_user(ptr, &ps_cali, sizeof(ps_cali)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;

			case ALSPS_IOCTL_SET_CALI:
				if(copy_from_user(&ps_cali, ptr, sizeof(ps_cali)))
				{
					err = -EFAULT;
					goto err_out;
				}

				obj->ps_cali = ps_cali;

#ifdef CUSTOM_KERNEL_SENSORHUB
                pCustData->setCali.action = CM36283_CUST_ACTION_SET_CALI;
                pCustData->setCali.cali = ps_cali;
                len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setCali);
                
                err = SCP_sensorHub_req_send(&data, &len, 1);
#endif

				break;

			case ALSPS_SET_PS_THRESHOLD:
				if(copy_from_user(threshold, ptr, sizeof(threshold)))
				{
					err = -EFAULT;
					goto err_out;
				}
				APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n", __func__, threshold[0],threshold[1]); 
				atomic_set(&obj->ps_thd_val_high,  (threshold[0]+obj->ps_cali));
				atomic_set(&obj->ps_thd_val_low,  (threshold[1]+obj->ps_cali));//need to confirm

#ifdef CUSTOM_KERNEL_SENSORHUB
                pCustData->setPSThreshold.action = CM36283_CUST_ACTION_SET_PS_THRESHODL;
                pCustData->setPSThreshold.threshold[0] = threshold[0];
                pCustData->setPSThreshold.threshold[1] = threshold[1];
                len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setPSThreshold);

                err = SCP_sensorHub_req_send(&data, &len, 1);
#else
				set_psensor_threshold(obj->client);
#endif
				
				break;
				
			case ALSPS_GET_PS_THRESHOLD_HIGH:
				threshold[0] = atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
				APS_ERR("%s get threshold high: 0x%x\n", __func__, threshold[0]); 
				if(copy_to_user(ptr, &threshold[0], sizeof(threshold[0])))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;
				
			case ALSPS_GET_PS_THRESHOLD_LOW:
				threshold[0] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali;
				APS_ERR("%s get threshold low: 0x%x\n", __func__, threshold[0]); 
				if(copy_to_user(ptr, &threshold[0], sizeof(threshold[0])))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;
			/*------------------------------------------------------------------------------------------*/
			
			default:
				APS_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
				err = -ENOIOCTLCMD;
				break;
		}
	
		err_out:
		return err;    
	}
/********************************************************************/
/*------------------------------misc device related operation functions------------------------------------*/
static struct file_operations cm36283_fops = {
	.owner = THIS_MODULE,
	.open = cm36283_open,
	.release = cm36283_release,
	.unlocked_ioctl = cm36283_unlocked_ioctl,
};

static struct miscdevice cm36283_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &cm36283_fops,
};

/*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
static void cm36283_early_suspend(struct early_suspend *h)
{
		struct cm36283_priv *obj = container_of(h, struct cm36283_priv, early_drv);	
		int err;
		APS_FUN();	  
	
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return;
		}
		
		atomic_set(&obj->als_suspend, 1);
		if((err = cm36283_enable_als(obj->client, 0)))
		{
			APS_ERR("disable als fail: %d\n", err); 
		}
}

static void cm36283_late_resume(struct early_suspend *h) 
{
		struct cm36283_priv *obj = container_of(h, struct cm36283_priv, early_drv);		  
		int err;
		hwm_sensor_data sensor_data;
		memset(&sensor_data, 0, sizeof(sensor_data));
		APS_FUN();
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return;
		}
	
		atomic_set(&obj->als_suspend, 0);
		if(test_bit(CMC_BIT_ALS, &obj->enable))
		{
			if((err = cm36283_enable_als(obj->client, 1)))
			{
				APS_ERR("enable als fail: %d\n", err);		  
	
			}
		}
}
#endif
/*--------------------------------------------------------------------------------*/
static int cm36283_init_client(struct i2c_client *client)
{
	struct cm36283_priv *obj = i2c_get_clientdata(client);
	u8 databuf[3];    
	int res = 0;

	databuf[0] = CM36283_REG_ALS_CONF;
	if(1 == obj->hw->polling_mode_als)
	databuf[1] = 0x81;
	else
	databuf[1] = 0x83;	
	databuf[2] = 0x00;
	res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}
	
	databuf[0] = CM36283_REG_PS_CONF1_2;
	databuf[1] = 0x1B;
	if(1 == obj->hw->polling_mode_ps)
	databuf[2] = 0x40;
	else
	databuf[2] = 0x43;
	res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}
	
	databuf[0] = CM36283_REG_PS_CONF3_MS;
	databuf[1] = 0x10;
	databuf[2] = 0x00;//need to confirm interrupt mode PS_MS mode whether to set
	res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}

	databuf[0] = CM36283_REG_PS_CANC;
	databuf[1] = 0x00;
	databuf[2] = 0x00;
	res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}

	if(0 == obj->hw->polling_mode_als){
			databuf[0] = CM36283_REG_ALS_THDH;
			databuf[1] = 0x00;
			databuf[2] = atomic_read(&obj->als_thd_val_high);
			res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
			if(res <= 0)
			{
				APS_ERR("i2c_master_send function err\n");
				goto EXIT_ERR;
			}
			databuf[0] = CM36283_REG_ALS_THDL;
			databuf[1] = 0x00;
			databuf[2] = atomic_read(&obj->als_thd_val_low);//threshold value need to confirm
			res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
			if(res <= 0)
			{
				APS_ERR("i2c_master_send function err\n");
				goto EXIT_ERR;
			}
		}
	if(0 == obj->hw->polling_mode_ps){
			databuf[0] = CM36283_REG_PS_THD;
			databuf[1] = atomic_read(&obj->ps_thd_val_low);
			databuf[2] = atomic_read(&obj->ps_thd_val_high);//threshold value need to confirm
			res = CM36283_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
			if(res <= 0)
			{
				APS_ERR("i2c_master_send function err\n");
				goto EXIT_ERR;
			}
		}
	res = cm36283_setup_eint(client);
	if(res!=0)
	{
		APS_ERR("setup eint: %d\n", res);
		return res;
	}

#ifndef CUSTOM_KERNEL_SENSORHUB
	res = cm36283_disable_eint(client);
	if(res!=0)
	{
		APS_ERR("disable eint fail: %d\n", res);
		return res;
	}
#endif//#ifndef CUSTOM_KERNEL_SENSORHUB
	
	return CM36283_SUCCESS;
	
	EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;
}
/*--------------------------------------------------------------------------------*/
static int als_open_report_data(int open)
{
	//should queuq work to report event if  is_report_input_direct=true
	return 0;
}
/*--------------------------------------------------------------------------------*/
// if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL
static int als_enable_nodata(int en)
{
	int res = 0;
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return -1;
	}
	APS_LOG("cm36283_obj als enable value = %d\n", en);

    if(en)
	{
		if((res = cm36283_enable_als(cm36283_obj->client, 1)))
		{
			APS_ERR("enable als fail: %d\n", res); 
			return -1;
		}
		set_bit(CMC_BIT_ALS, &cm36283_obj->enable);
	}
	else
	{
		if((res = cm36283_enable_als(cm36283_obj->client, 0)))
		{
			APS_ERR("disable als fail: %d\n", res); 
			return -1;
		}
		clear_bit(CMC_BIT_ALS, &cm36283_obj->enable);
	}
    
	if(res){
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}
	return 0;
}
/*--------------------------------------------------------------------------------*/
static int als_set_delay(u64 ns)
{
	return 0;
}
/*--------------------------------------------------------------------------------*/
static int als_get_data(int* value, int* status)
{
	int err = 0;

	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return -1;
	}

	if((err = cm36283_read_als(cm36283_obj->client, &cm36283_obj->als)))
	{
		err = -1;
	}
	else
	{
		*value = cm36283_get_als_value(cm36283_obj, cm36283_obj->als);
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	return err;
}
/*--------------------------------------------------------------------------------*/
// if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL
static int ps_open_report_data(int open)
{
	//should queuq work to report event if  is_report_input_direct=true
	return 0;
}
/*--------------------------------------------------------------------------------*/
// if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL
static int ps_enable_nodata(int en)
{
	int res = 0;
	if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return -1;
	}
	APS_LOG("cm36283_obj als enable value = %d\n", en);

    if(en)
	{
		if((res = cm36283_enable_ps(cm36283_obj->client, 1)))
		{
			APS_ERR("enable ps fail: %d\n", res); 
			return -1;
		}
		set_bit(CMC_BIT_PS, &cm36283_obj->enable);
	}
	else
	{
		if((res = cm36283_enable_ps(cm36283_obj->client, 0)))
		{
			APS_ERR("disable ps fail: %d\n", res); 
			return -1;
		}
		clear_bit(CMC_BIT_PS, &cm36283_obj->enable);
	}
    
	if(res){
		APS_ERR("als_enable_nodata is failed!!\n");
		return -1;
	}
    
	return 0;

}
/*--------------------------------------------------------------------------------*/
static int ps_set_delay(u64 ns)
{
	return 0;
}
/*--------------------------------------------------------------------------------*/
static int ps_get_data(int* value, int* status)
{
    int err = 0;

    if(!cm36283_obj)
	{
		APS_ERR("cm36283_obj is null!!\n");
		return -1;
	}
    
    if((err = cm36283_read_ps(cm36283_obj->client, &cm36283_obj->ps)))
    {
        err = -1;
    }
    else
    {
        *value = cm36283_get_ps_value(cm36283_obj, cm36283_obj->ps);
        *status = SENSOR_STATUS_ACCURACY_MEDIUM;
    }
    
	return err;
}
/*-----------------------------------i2c operations----------------------------------*/
static int cm36283_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cm36283_priv *obj;

    struct als_control_path als_ctl={0};
	struct als_data_path als_data={0};
	struct ps_control_path ps_ctl={0};
	struct ps_data_path ps_data={0};
    
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(*obj));
	cm36283_obj = obj;
	
	obj->hw = get_cust_alsps_hw();//get custom file data struct
	
	INIT_WORK(&obj->eint_work, cm36283_eint_work);
    INIT_WORK(&obj->init_done_work, cm36283_init_done_work);

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
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);

	cm36283_i2c_client = client;

	if((err = cm36283_init_client(client)))
	{
		goto exit_init_failed;
	}
	APS_LOG("cm36283_init_client() OK!\n");

	if((err = misc_register(&cm36283_device)))
	{
		APS_ERR("cm36283_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	APS_LOG("cm36283_device misc_register OK!\n");

	/*------------------------cm36283 attribute file for debug--------------------------------------*/
	if((err = cm36283_create_attr(&cm36283_init_info.platform_diver_addr->driver)))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	/*------------------------cm36283 attribute file for debug--------------------------------------*/

	als_ctl.open_report_data= als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay  = als_set_delay;
	als_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	als_ctl.is_support_batch = true;
#else
    als_ctl.is_support_batch = false;
#endif
	
	err = als_register_control_path(&als_ctl);
	if(err)
	{
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);	
	if(err)
	{
		APS_ERR("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	
	ps_ctl.open_report_data= ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay  = ps_set_delay;
	ps_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	ps_ctl.is_support_batch = true;
#else
    ps_ctl.is_support_batch = false;
#endif
	
	err = ps_register_control_path(&ps_ctl);
	if(err)
	{
		APS_ERR("register fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);	
	if(err)
	{
		APS_ERR("tregister fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}

	#if 0//defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = cm36283_early_suspend,
	obj->early_drv.resume   = cm36283_late_resume,    
	register_early_suspend(&obj->early_drv);
	#endif

    cm36283_init_flag = 0;
	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	exit_sensor_obj_attach_fail:
	exit_misc_device_register_failed:
		misc_deregister(&cm36283_device);
	exit_init_failed:
		kfree(obj);
	exit:
	cm36283_i2c_client = NULL;           
	APS_ERR("%s: err = %d\n", __func__, err);
    cm36283_init_flag = -1;
	return err;
}

static int cm36283_i2c_remove(struct i2c_client *client)
{
	int err;	
	/*------------------------cm36283 attribute file for debug--------------------------------------*/	
	if((err = cm36283_delete_attr(&cm36283_init_info.platform_diver_addr->driver)))
	{
		APS_ERR("cm36283_delete_attr fail: %d\n", err);
	} 
	/*----------------------------------------------------------------------------------------*/
	
	if((err = misc_deregister(&cm36283_device)))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
		
	cm36283_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;

}

static int cm36283_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, CM36283_DEV_NAME);
	return 0;

}

static int cm36283_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	APS_FUN();
	return 0;
}

static int cm36283_i2c_resume(struct i2c_client *client)
{
	APS_FUN();
	return 0;
}

/*----------------------------------------------------------------------------*/
static int  cm36283_local_init(void)
{
    struct alsps_hw *hw = get_cust_alsps_hw();
	//printk("fwq loccal init+++\n");

	cm36283_power(hw, 1);
	if(i2c_add_driver(&cm36283_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	}
	if(-1 == cm36283_init_flag)
	{
	   return -1;
	}
	//printk("fwq loccal init---\n");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int cm36283_remove()
{
	//APS_FUN(); 
	struct alsps_hw *hw = get_cust_alsps_hw();
	
	cm36283_power(hw, 0);//*****************  
	
	i2c_del_driver(&cm36283_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int __init cm36283_init(void)
{
	//APS_FUN();
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_LOG("%s: i2c_number=%d, i2c_addr: 0x%x\n", __func__, hw->i2c_num, hw->i2c_addr[0]);
	i2c_register_board_info(hw->i2c_num, &i2c_cm36283, 1);
	alsps_driver_add(&cm36283_init_info);
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit cm36283_exit(void)
{
	APS_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(cm36283_init);
module_exit(cm36283_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("yucong xiong");
MODULE_DESCRIPTION("cm36283 driver");
MODULE_LICENSE("GPL");


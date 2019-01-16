/* drivers/hwmon/mt6516/amit/rpr410.c - RPR410 ALS/PS driver
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
#include <linux/mutex.h>
//#include <mach/mt_gpio.h>


//#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


#define POWER_NONE_MACRO MT65XX_POWER_NONE
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include "rpr410.h"
#include <linux/sched.h>

/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define RPR410_DEV_NAME     "RPR410"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_ERR APS_TAG fmt, ##args)                 
/******************************************************************************
 * extern functions
*******************************************************************************/
/*for interrup work mode support --add by liaoxl.lenovo 12.08.2011*/
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void mt_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt_eint_set_sens(kal_uint8 eintno, kal_bool sens);

extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
/*----------------------------------------------------------------------------*/
static struct i2c_client *rpr410_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id rpr410_i2c_id[] = {{RPR410_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_RPR410={ I2C_BOARD_INFO("RPR410", (0X72>>1))};
/*the adapter id & i2c address will be available in customization*/
//static unsigned short rpr410_force[] = {0x03, 0X72, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const rpr410_forces[] = { rpr410_force, NULL };
//static struct i2c_client_address_data rpr410_addr_data = { .forces = rpr410_forces,};
/*----------------------------------------------------------------------------*/
static int rpr410_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int rpr410_i2c_remove(struct i2c_client *client);
static int rpr410_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int rpr410_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int rpr410_i2c_resume(struct i2c_client *client);

static struct rpr410_priv *g_rpr410_ptr = NULL;

 struct PS_CALI_DATA_STRUCT
{
    int close;
    int far_away;
    int valid;
} ;

static struct PS_CALI_DATA_STRUCT ps_cali={0,0,0};
static int intr_flag_value = 0;
static unsigned long long int_top_time = 0;

/*----------------------------------------------------------------------------*/
typedef struct {
    unsigned long long data;
    unsigned long long data0;
    unsigned long long data1;
    unsigned char      gain_data0;
    unsigned char      gain_data1;
    unsigned long      dev_unit;
    unsigned char      als_time;
    unsigned short     als_data0;
    unsigned short     als_data1;
} CALC_DATA;

typedef struct {
    unsigned long positive;
    unsigned long decimal;
} CALC_ANS;
typedef enum {
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;
/*----------------------------------------------------------------------------*/
struct rpr410_i2c_addr {    /*define a series of i2c slave address*/
    u8  write_addr;  
    u8  ps_thd;     /*PS INT threshold*/
};
/*----------------------------------------------------------------------------*/
struct rpr410_priv {
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct work_struct  eint_work;

    /*i2c address group*/
    struct rpr410_i2c_addr  addr;
    
    /*misc*/
    u16		    als_modulus;
    atomic_t    i2c_retry;
    atomic_t    als_suspend;
    atomic_t    als_debounce;   /*debounce time after enabling als*/
    atomic_t    als_deb_on;     /*indicates if the debounce is on*/
    atomic_t    als_deb_end;    /*the jiffies representing the end of debounce*/
    atomic_t    ps_mask;        /*mask ps: always return far away*/
    atomic_t    ps_debounce;    /*debounce time after enabling ps*/
    atomic_t    ps_deb_on;      /*indicates if the debounce is on*/
    atomic_t    ps_deb_end;     /*the jiffies representing the end of debounce*/
    atomic_t    ps_suspend;


    /*data*/
    u16         als;
    u16          ps;
    u8          _align;
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];
    int 		ps_cali_val;

    atomic_t    als_cmd_val;    /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_cmd_val;     /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_thd_val_high;     /*the cmd value can't be read, stored in ram*/
	atomic_t    ps_thd_val_low;     /*the cmd value can't be read, stored in ram*/
    ulong       enable;         /*enable mask*/
    ulong       pending_intr;   /*pending interrupt*/

    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver rpr410_i2c_driver = {	
	.probe      = rpr410_i2c_probe,
	.remove     = rpr410_i2c_remove,
	//.detect     = rpr410_i2c_detect,
	.suspend    = rpr410_i2c_suspend,
	.resume     = rpr410_i2c_resume,
	.id_table   = rpr410_i2c_id,
	.driver = {
		.name           = RPR410_DEV_NAME,
	},
};

#define THRES_TOLERANCE		9	//I think this is a proper value. It should not be too big.
#define THRES_DEFAULT_DIFF	5 //15 //35
#define REG_PSTH_MAX	0xFFF
#define REG_PSTL_MAX	0xFFF

static int rpr410_ps_average_val = 0;

static DEFINE_MUTEX(rpr410_mutex);

static struct rpr410_priv *rpr410_obj = NULL;
static struct platform_driver rpr410_alsps_driver;

/*----------------------------------------------------------------------------*/
#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1

int chip_i2c_master_operate(struct i2c_client *client, const char *buf, int count, int i2c_flag)
{
	int res = 0;

	mutex_lock(&rpr410_mutex);
	switch(i2c_flag)
	{	
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
		APS_LOG("chip_i2c_master_operate i2c_flag command not support!\n");
		break;
	}
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	mutex_unlock(&rpr410_mutex);
	return res;
	EXIT_ERR:
	mutex_unlock(&rpr410_mutex);
	APS_ERR("chip_i2c_master_operate fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/

static int rpr410_set_psensor_threshold(struct i2c_client *client)
{
    struct rpr410_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	int res = 0;
	databuf[0] = REG_PSTL_LSB;	
	databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_low) & 0x00FF);
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
		return RPR410_ERR_I2C;
	}
	databuf[0] = REG_PSTL_MBS;	
	databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low) & 0xFF00) >> 8);
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
		return RPR410_ERR_I2C;
	}
	databuf[0] = REG_PSTH_LSB;	
	databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_high) & 0x00FF);
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
		return RPR410_ERR_I2C;
	}
	databuf[0] = REG_PSTH_MBS;	
	databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high) & 0xFF00) >> 8);
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
		return RPR410_ERR_I2C;
	}
	return RPR410_SUCCESS;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return -1;
}


static int rpr410_change_psensor_threshold(struct i2c_client *client, int high, int low)
{
    struct rpr410_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	int res = 0;
	APS_LOG("rpr410_change_psensor_threshold: high:0x%x, low:0x%x\n", high, low);
	databuf[0] = REG_PSTL_LSB;	
	databuf[1] = (u8)(low & 0x00FF);
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
		return RPR410_ERR_I2C;
	}
	databuf[0] = REG_PSTL_MBS;	
	databuf[1] = (u8)((low & 0xFF00) >> 8);
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
		return RPR410_ERR_I2C;
	}
	databuf[0] = REG_PSTH_LSB;	
	databuf[1] = (u8)(high & 0x00FF);
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
		return RPR410_ERR_I2C;
	}
	databuf[0] = REG_PSTH_MBS;	
	databuf[1] = (u8)((high & 0xFF00) >> 8);
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
		return RPR410_ERR_I2C;
	}
	return RPR410_SUCCESS;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return -1;
}


/*----------------------------------------------------------------------------*/

int rpr410_get_addr(struct alsps_hw *hw, struct rpr410_i2c_addr *addr)
{
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->write_addr= hw->i2c_addr[0];
	return 0;
}

static void rpr410_power(struct alsps_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	//APS_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APS_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "RPR410")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "RPR410")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
}
/*----------------------------------------------------------------------------*/
static long rpr410_enable_als(struct i2c_client *client, int enable)
{
	struct rpr410_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];	  
	int res = 0;
	u8 buffer[2];
	u8 power_state, power_set;
	PWR_ST  pwr_st;
		
	
	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
	
	buffer[0] = REG_MODECONTROL;
	res = chip_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}

	power_state = buffer[0] & 0xF;
	if (MCTL_TABLE[power_state].PS == 0) 
	{
            	pwr_st.ps_state = CTL_STANDBY;
    } 
	else 
    {
            	pwr_st.ps_state = CTL_STANDALONE;
    }

	if(enable)
	{
		if (pwr_st.ps_state == CTL_STANDALONE)
		{
			power_set = PWRON_PS_ALS;
			databuf[0] = REG_MODECONTROL;	
			databuf[1] = power_set | (buffer[0] & CLR_LOW4BIT);
			res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}	
		}
		else if (pwr_st.ps_state == CTL_STANDBY)
		{
			power_set = PWRON_ONLY_ALS;
			databuf[0] = REG_MODECONTROL;	
			databuf[1] = power_set | (buffer[0] & CLR_LOW4BIT);
			res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
		}
	}
	else
	{	
		if (pwr_st.ps_state == CTL_STANDALONE)
		{
			power_set = PWRON_ONLY_PS;
			databuf[0] = REG_MODECONTROL;	
			databuf[1] = power_set | (buffer[0] & CLR_LOW4BIT);
			res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
		}
		else if (pwr_st.ps_state == CTL_STANDBY)
		{
			power_set = PWRON_STANDBY;
			databuf[0] = REG_MODECONTROL;	
			databuf[1] = power_set | (buffer[0] & CLR_LOW4BIT);
			res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}			
		}		
	}
		
	return 0;
		
	EXIT_ERR:
		APS_ERR("rpr410_enable_als fail\n");
		return -1; //return correct value when iic error -- by liaoxl.lenovo 3.15.2012
}

/*----------------------------------------------------------------------------*/
static long rpr410_enable_ps(struct i2c_client *client, int enable)
{
	struct rpr410_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	int res = 0;
	u8 buffer[2];
	u8 power_state, power_set;
	PWR_ST  pwr_st;	

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	buffer[0] = REG_MODECONTROL;
	res = chip_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		APS_LOG("rpr410_enable_ps step 1 ret=%d\n", res);
		goto EXIT_ERR;
	}

	power_state = buffer[0] & 0xF;
    if (MCTL_TABLE[power_state].ALS == 0) 
	{
    	pwr_st.als_state = CTL_STANDBY;
    } 
	else 
    {
    	pwr_st.als_state = CTL_STANDALONE;
    }

	if(enable)
	{
		if (pwr_st.als_state == CTL_STANDALONE)
		{
			power_set = PWRON_PS_ALS;
			databuf[0] = REG_MODECONTROL;	
			databuf[1] = power_set | (buffer[0] & CLR_LOW4BIT);
			res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG("rpr410_enable_ps step 2 ret=%d\n", res);
				goto EXIT_ERR;
			}
		}
		else if (pwr_st.als_state == CTL_STANDBY)
		{
			power_set = PWRON_ONLY_PS;
			databuf[0] = REG_MODECONTROL;	
			databuf[1] = power_set | (buffer[0] & CLR_LOW4BIT);
			res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG("rpr410_enable_ps step 3 ret=%d\n", res);
				goto EXIT_ERR;
			}
		}

	}
	else{
		if (pwr_st.als_state == CTL_STANDALONE)
	        {
			power_set = PWRON_ONLY_ALS;
			databuf[0] = REG_MODECONTROL;	
			databuf[1] = power_set | (buffer[0] & CLR_LOW4BIT);
			res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG("rpr410_enable_ps step 13 ret=%d\n", res);
				goto EXIT_ERR;
			}
	        }
	        else if (pwr_st.als_state == CTL_STANDBY)
	        {
			power_set = PWRON_STANDBY;
			databuf[0] = REG_MODECONTROL;	
			databuf[1] = power_set | (buffer[0] & CLR_LOW4BIT);
			res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG("rpr410_enable_ps step 14 ret=%d\n", res);
				goto EXIT_ERR;
			}			
		 }
		rpr410_change_psensor_threshold(client, atomic_read(&obj->ps_thd_val_high), atomic_read(&obj->ps_thd_val_low));

	}
	
	return 0;
	
EXIT_ERR:
	APS_ERR("rpr410_enable_ps fail\n");
	return -1; //return correct value when iic error -- by liaoxl.lenovo 3.15.2012
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
static int rpr410_check_and_clear_intr(struct i2c_client *client) 
{
	u8 buffer[2];
    int res = 0;


	buffer[0] = REG_INTERRUPT;
	res = chip_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	
	return buffer[0];

EXIT_ERR:
	APS_ERR("rpr410_check_and_clear_intr fail\n");
	return -1;
}


/*-----------------------------------------------------------------------------*/
void rpr410_eint_func(void)
{
	struct rpr410_priv *obj = g_rpr410_ptr;
	if(!obj)
	{
		return;
	}
	int_top_time = sched_clock();	
	schedule_work(&obj->eint_work);
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
int rpr410_setup_eint(struct i2c_client *client)
{
	struct rpr410_priv *obj = i2c_get_clientdata(client);        

	g_rpr410_ptr = obj;
	
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, rpr410_eint_func, 0);

	mt_eint_unmask(CUST_EINT_ALS_NUM);  
    return 0;
}

/*----------------------------------------------------------------------------*/

static int rpr410_init_client(struct i2c_client *client)
{
	struct rpr410_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	int res = 0;
    u8 buffer[2];
	u8 ps_id_value[1];
   
	databuf[0] = REG_SYSTEMCONTROL;    
	databuf[1] = REG_SW_RESET | REG_INT_RESET;
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_ERR("rpr410_init_client 1 ret=%d\n", res);
		goto EXIT_ERR;
	}
	
	databuf[0] = REG_MODECONTROL;    
	databuf[1] = PS_ALS_SET_MODE_CONTROL|PWRON_ONLY_ALS;
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_ERR("rpr410_init_client 2 ret=%d\n", res);
		goto EXIT_ERR;
	}

	databuf[0] = REG_ALSPSCONTROL;    
	databuf[1] = PS_ALS_SET_ALSPS_CONTROL;
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_ERR("rpr410_init_client 3 ret=%d\n", res);
		goto EXIT_ERR;
	}

	databuf[0] = REG_PERSISTENCE;    
	databuf[1] = PS_ALS_SET_INTR_PERSIST;
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_ERR("rpr410_init_client 4 ret=%d\n", res);
		goto EXIT_ERR;
	}
	
	if(0 == obj->hw->polling_mode_ps)
	{
		res = rpr410_set_psensor_threshold(client);
		if(res < 0)
			{
			APS_LOG("rpr410_enable_ps rpr410_set_psensor_threshold ret=%d\n", res);
			goto EXIT_ERR;
		}
		
		databuf[0] = REG_INTERRUPT;
		databuf[1] = 0xA1;
		res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if(res < 0)
		{
			APS_LOG("rpr410_enable_ps step 6 ret=%d\n", res);
			goto EXIT_ERR;
		}
	
		mt_eint_unmask(CUST_EINT_ALS_NUM);
	
	}
	else{
		cancel_work_sync(&obj->eint_work);
		mt_eint_mask(CUST_EINT_ALS_NUM);
	}


	databuf[0] = REG_ALSDATA0TH_LSB;    
	databuf[1] = PS_ALS_SET_ALS_TH & 0x00FF ;
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);

	if(res < 0)
	{
		APS_ERR("rpr410_init_client 7 ret=%d\n", res);
		goto EXIT_ERR;
	}

	databuf[0] = REG_ALSDATA0TH_MBS;    
	databuf[1] = (PS_ALS_SET_ALS_TH& 0xFF00) >> 8;
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_ERR("rpr410_init_client 8 ret=%d\n", res);
		goto EXIT_ERR;
	}

	databuf[0] = REG_SYSTEMCONTROL;
	res = chip_i2c_master_operate(client, databuf, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		APS_ERR("rpr410_init_client 9 ret=%d\n", res);
		goto EXIT_ERR;
	}
	APS_ERR("RPR410_CHECKID: 0x%x\n", databuf[0]);
	if(databuf[0]  != 0x09)
	{
		APS_ERR("RPR410_CHECKID_FAIL\n");
		return RPR410_CHECKID_FAIL;
	}
	
	if((res = rpr410_setup_eint(client))!=0)
	{
		APS_ERR("setup eint: %d\n", res);
		return res;
	}
	
	databuf[0] = REG_MODECONTROL;    
	databuf[1] = 0x00;
	res = chip_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_ERR("rpr410_init_client 9 ret=%d\n", res);
		goto EXIT_ERR;
	}

	return RPR410_SUCCESS;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return -1; //return correct value when iic error -- by liaoxl.lenovo 3.15.2012
}
/******************************************************************************
 * NAME       : long_long_divider
 * FUNCTION   : calc divider of unsigned long long int or unsgined long
 * REMARKS    :
 *****************************************************************************/
static int long_long_divider(long long data, unsigned long base_divier, unsigned long *answer, unsigned long long *overplus)
{
    volatile long long divier;
    volatile long      unit_sft;

    if ((data < 0) || (base_divier == 0)) {
        *answer   = 0;
        *overplus = 0;
        return (CALC_ERROR);
    }

    divier = base_divier;
    if (data > MASK_LONG) {
        unit_sft = 0;
        while ((data > divier) && (divier > 0)) {
            unit_sft++;
            divier = divier << 1;
        }
        while ((data > base_divier) && (unit_sft > 0)) {
            if (data > divier) {
                *answer += 1 << unit_sft;
                data    -= divier;
            }
            unit_sft--;
            divier = divier >> 1;
        }
        *overplus = data;
    } else {
        *answer = (unsigned long)(data & MASK_LONG) / base_divier;
        /* calculate over plus and shift 16bit */
        *overplus = (unsigned long long)(data - (*answer * base_divier));
    }

    return (0);
}

/******************************************************************************
 * NAME       : calc_rohm_als_data
 * FUNCTION   : calculate illuminance data for RPR410
 * REMARKS    : final_data is 1000 times, which is defined as CUT_UNIT, of the actual lux value
 *****************************************************************************/
static int calc_rohm_als_data(READ_DATA_BUF data, DEVICE_VAL dev_val)
{
	int DECIMAL_BIT = 15;
	int JUDGE_FIXED_COEF = 100;
	int MAX_OUTRANGE = 11357;
	int MAXRANGE_NMODE = 0xFFFF;
	int MAXSET_CASE = 4;
	int CUT_UNIT = 20;

	int                result, final_data, mid_data;
	CALC_DATA          calc_data;
	CALC_ANS           calc_ans;
	unsigned long      calc_judge;
	unsigned char      set_case;
	unsigned long      div_answer;
	unsigned long long div_overplus;
	unsigned long long overplus;
	unsigned long      max_range;

	/* set the value of measured als data */
	calc_data.als_data0  = data.als_data0;
	calc_data.als_data1  = data.als_data1;
	calc_data.gain_data0 = GAIN_TABLE[dev_val.gain].DATA0;

	/* set max range */
	if (calc_data.gain_data0 == 0) 
	{
		/* issue error value when gain is 0 */
		return (CALC_ERROR);
	}
	else
	{
		max_range = MAX_OUTRANGE / calc_data.gain_data0;
	}
	
	/* calculate data */
	if (calc_data.als_data0 == MAXRANGE_NMODE) 
	{
		calc_ans.positive = max_range;
		calc_ans.decimal  = 0;
	} 
	else 
	{
		/* get the value which is measured from power table */
		calc_data.als_time = MCTL_TABLE[dev_val.time].ALS;
		if (calc_data.als_time == 0) 
		{
			/* issue error value when time is 0 */
			return (CALC_ERROR);
		}

		calc_judge = calc_data.als_data1 * JUDGE_FIXED_COEF;
		if (calc_judge < (calc_data.als_data0 * judge_coefficient[0])) 
		{
			set_case = 0;
		} 
		else if (calc_judge < (data.als_data0 * judge_coefficient[1]))
		{
			set_case = 1;
		} 
		else if (calc_judge < (data.als_data0 * judge_coefficient[2])) 
		{
			set_case = 2;
		}
		else if (calc_judge < (data.als_data0 * judge_coefficient[3])) 
		{
			 set_case = 3;
		} 
		else
		{
			set_case = MAXSET_CASE;
		}
		calc_ans.positive = 0;
		if (set_case >= MAXSET_CASE) 
		{
			calc_ans.decimal = 0;	//which means that lux output is 0
		}
		else
		{
			calc_data.gain_data1 = GAIN_TABLE[dev_val.gain].DATA1;
			if (calc_data.gain_data1 == 0) 
			{
				/* issue error value when gain is 0 */
				return (CALC_ERROR);
			}
                calc_data.data0      = (long long )(data0_coefficient[set_case] * calc_data.als_data0) * calc_data.gain_data1;
                calc_data.data1      = (long long )(data1_coefficient[set_case] * calc_data.als_data1) * calc_data.gain_data0;
			if (calc_data.data0 < calc_data.data1) 
			{
				/* issue error value when data is negtive */
				return (CALC_ERROR);
			}
			calc_data.data       = (calc_data.data0 - calc_data.data1);
			calc_data.dev_unit   = calc_data.gain_data0 * calc_data.gain_data1 * calc_data.als_time * 10;
			if (calc_data.dev_unit == 0) 
			{
				/* issue error value when dev_unit is 0 */
				return (CALC_ERROR);
			}

			/* calculate a positive number */
			div_answer   = 0;
			div_overplus = 0;
                result = long_long_divider(calc_data.data, calc_data.dev_unit, &div_answer, &div_overplus);
                if (result == CALC_ERROR) {
                    return (result);
                }
			calc_ans.positive = div_answer;
			/* calculate a decimal number */
			calc_ans.decimal = 0;
			overplus         = div_overplus;
			if (calc_ans.positive < max_range)
			{
				if (overplus != 0)
				{
					overplus     = overplus << DECIMAL_BIT;
					div_answer   = 0;
					div_overplus = 0;
					long_long_divider(overplus, calc_data.dev_unit, &div_answer, &div_overplus);
					calc_ans.decimal = div_answer;
				}
			}
			else
			{
				calc_ans.positive = max_range;
			}
		}
	}
	
	mid_data = (calc_ans.positive << DECIMAL_BIT) + calc_ans.decimal;
	final_data = calc_ans.positive * CUT_UNIT + ((calc_ans.decimal * CUT_UNIT) >> DECIMAL_BIT);
					
	return (final_data);

}

/************************************************************
 *                      logic function                      *
 ***********************************************************/
/******************************************************************************
 * NAME       : get_from_device
 * FUNCTION   : periodically reads the data from sensor(thread of work)
 * REMARKS    :
 *****************************************************************************/
static int get_from_device(DEVICE_VAL *dev_val, struct i2c_client *client)
{
#define LEDBIT_MASK   (3)
#define GAIN_VAL_MASK (0xF)
#if 0
	struct rpr410_priv *obj = i2c_get_clientdata(client);	 
	u8 buffer[1];
	int res = 0;
    	unsigned char alsps_ctl[1], read_time[1];

   	 /* initalize the returning value */
    	dev_val->time        = 6;
    	dev_val->gain        = (PS_ALS_SET_ALSPS_CONTROL >> 2) & GAIN_VAL_MASK;
    	dev_val->led_current = PS_ALS_SET_ALSPS_CONTROL & LEDBIT_MASK;


	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	mutex_lock(&rpr410_mutex);
	buffer[0]=REG_MODECONTROL;
        client->addr = client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
	res = i2c_master_send(client, buffer, (1<<8) | 1);
        client->addr = client->addr& I2C_MASK_FLAG;
	mutex_unlock(&rpr410_mutex);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	
	dev_val->time = buffer[0] & 0xF;

	mutex_lock(&rpr410_mutex);
	buffer[0]=REG_ALSPSCONTROL;
        client->addr = client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
	res = i2c_master_send(client, buffer, (1<<8) | 1);
        client->addr = client->addr& I2C_MASK_FLAG;
	mutex_unlock(&rpr410_mutex);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

    	dev_val->led_current = buffer[0] & LEDBIT_MASK;
    	dev_val->gain        = (buffer[0] >> 2) & GAIN_VAL_MASK;
#else
    	dev_val->time        = 6;
    	dev_val->gain        = (PS_ALS_SET_ALSPS_CONTROL >> 2) & GAIN_VAL_MASK;
    	dev_val->led_current = PS_ALS_SET_ALSPS_CONTROL & LEDBIT_MASK;
#endif
    return (0);
		
//EXIT_ERR:
//	APS_ERR("rpr410_read_ps fail\n");
//	return res;

#undef LEDBIT_MASK
#undef GAIN_VAL_MASK
}

/****************************************************************************** 
 * Function Configuration
******************************************************************************/
int rpr410_read_als(struct i2c_client *client, u16 *data)
{
	struct rpr410_priv *obj = i2c_get_clientdata(client);	 
	u8 buffer[2];
	u16 prev_als_value = *data;
	int res = 0;
	READ_DATA_BUF   als_data;
	DEVICE_VAL  dev_val;

	
	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	buffer[0] = REG_ALSDATA0_LSB;
	res = chip_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
        als_data.als_data0 = buffer[0] | (buffer[1]<<8);
	
	buffer[0] = REG_ALSDATA1_LSB;
	res = chip_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
        als_data.als_data1 = buffer[0] | (buffer[1]<<8);	
	
    get_from_device(&dev_val, client);

	*data = calc_rohm_als_data(als_data, dev_val);
		if(*data == 0)
		*data ++;
	if(*data == CALC_ERROR)
		*data = prev_als_value;	//Report same value as previous.

	return 0;	 

	
	
EXIT_ERR:
	APS_ERR("rpr410_read_als fail\n");
	return -1; //return correct value when iic error -- by liaoxl.lenovo 3.15.2012
}
/*----------------------------------------------------------------------------*/

static int rpr410_get_als_value(struct rpr410_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;
	for(idx = 0; idx <= obj->als_level_num; idx++)
	{
		if(als <= obj->hw->als_level[idx])
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
		
		//APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);	
		return obj->hw->als_value[idx];
	}
	else
	{
		//APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);    
		return -1;
	}
}
/*----------------------------------------------------------------------------*/
static int chip_check_regs(struct i2c_client *client)
{
	struct APDS9930_priv *obj = i2c_get_clientdata(client);	 
	u8 buffer[2];
	int res = 0;
	int i;

    APS_LOG("chip_check_regs start\n");
	buffer[0] = REG_ALSPSCONTROL;
	res = chip_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		return -EIO;
	}
	if(buffer[0] != PS_ALS_SET_ALSPS_CONTROL)
	{
		return -EINVAL;
	}
    APS_LOG("chip_check_regs start 1\n");
	buffer[0] = REG_PERSISTENCE;
	res = chip_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		return -EIO;
	}
	if(buffer[0] != PS_ALS_SET_INTR_PERSIST)
	{
	   return -EINVAL;
	}
	
    APS_LOG("chip_check_regs start 2\n");
	buffer[0] = REG_INTERRUPT;
	res = chip_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		return -EIO;
	}

	if((buffer[0]&0x3F) != (PS_ALS_SET_INTR | MODE_PROXIMITY))
	{
		return -EINVAL;
	}
    APS_LOG("chip_check_regs end\n");

	return 0;
}


static void chip_esd_check(struct i2c_client *client)
{
	int err;
	struct rpr410_priv *obj = i2c_get_clientdata(client);

	err = chip_check_regs(obj->client);
	APS_LOG("chip_esd_check err =%d\n",err);
	if(err == -EIO)
	{
		APS_ERR("check regs error IIC need reset!!\n");
#if 0
		hwPowerDown(MT65XX_POWER_LDO_VGP, "TMD");
		msleep(20);
		hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_2800, "TMD");
		msleep(100);
#endif
	}
	if(0 != err)
	{
		APS_ERR("check regs error data need init!!\n");
		err = rpr410_init_client(obj->client);
		if(err != 0)
		{
			APS_ERR("initialize client fail!!\n");
		}
		else
		{
			atomic_set(&obj->als_suspend, 0);
			if(test_bit(CMC_BIT_ALS, &obj->enable))
			{
				err = rpr410_enable_als(obj->client, 1);
				if(err != 0)
				{
				APS_ERR("enable als fail: %d\n", err);
				}
			}
			atomic_set(&obj->ps_suspend, 0);
			if(test_bit(CMC_BIT_PS, &obj->enable))
			{
				err = rpr410_enable_ps(obj->client, 1);
				if(err != 0)
				{
				   APS_ERR("enable ps fail: %d\n", err);
				}
			}
		}
	}
}
/*  lenovo esd check -- liaoxl.lenovo 1.13.2013 end */

long rpr410_read_ps(struct i2c_client *client, u16 *data)
{
	struct rpr410_priv *obj = i2c_get_clientdata(client);    
	u16 ps_value;      
	u8 buffer[2];
	int res = 0;

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
	/*  lenovo esd check -- liaoxl.lenovo 1.13.2013 start */
	//chip_esd_check(client);//MTK81126:what does it used for?
	/*  lenovo esd check -- liaoxl.lenovo 1.13.2013 end */

	buffer[0] = REG_PSDATA_LSB;
	res = chip_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	ps_value = buffer[0] | (buffer[1]<<8);
	if(ps_value < obj->ps_cali_val)
	*data = 0;
	else
	*data = ps_value -obj->ps_cali_val;
	return 0;    

EXIT_ERR:
	APS_ERR("rpr410_read_ps fail\n");
	return -1; //return correct value when iic error -- by liaoxl.lenovo 3.15.2012
}
/*----------------------------------------------------------------------------*/
static int rpr410_get_ps_value(struct rpr410_priv *obj, u16 ps)
{
	int val, mask = atomic_read(&obj->ps_mask);
	int invalid = 0;
	static int val_temp = -1;


	if(ps_cali.valid == 1)
	{
		//APS_LOG("rpr410_get_ps_value val_temp  = %d",val_temp);
		if(ps >ps_cali.close)
		{
			val = 0;  /*close*/
			val_temp = 0;
		}
		else if(ps <ps_cali.far_away)
		{
			val = 1;  /*far away*/
			val_temp = 1;
		}
		else
		        val = val_temp;

			//APS_LOG("rpr410_get_ps_value val  = %d",val);
	}
	else
	{
		if(ps > PS_ALS_SET_PS_TH)
		{
			val = 0;  /*close*/
			val_temp = 0;
		}
		else if(ps < PS_ALS_SET_PS_TL)
		{
			val = 1;  /*far away*/
			val_temp = 1;
		}
		else
		       val = val_temp;	
			
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
		APS_DBG("PS:  %05d => %05d\n", ps, val);
		return val;
	}	
	else
	{
		return -1;
	}	
}


/*----------------------------------------------------------------------------*/
static void rpr410_eint_work(struct work_struct *work)
{
	struct rpr410_priv *obj = (struct rpr410_priv *)container_of(work, struct rpr410_priv, eint_work);
	int err=-1;
	int ints,temp;
	hwm_sensor_data sensor_data;

	ints = rpr410_check_and_clear_intr(obj->client);
	if(ints < 0)
	{
		APS_ERR("rpr410_eint_work check intrs: %d\n", ints);
	}
	else
	{
		if(ints & PS_INT_MASK)
		{		
			err = rpr410_read_ps(obj->client, &obj->ps);
		}
		if(ints & ALS_INT_MASK) // 2 kinds of interrupt may occur at same time
		{
				
			err = rpr410_read_ps(obj->client, &obj->ps);
				
		}
		if(!((ints & ALS_INT_MASK) || (ints & PS_INT_MASK)))
		{
			APS_DBG( "Unknown interrupt source.\n");
			return;
		}
		/* proximity */
		if(0 == err) 
		{
			APS_DBG("rpr410_eint_work rawdata ps=%d als_ch0=%d!\n",obj->ps,obj->als);
			
			temp = rpr410_get_ps_value(obj, obj->ps);

			if(temp == 1){
				rpr410_change_psensor_threshold(obj->client, atomic_read(&obj->ps_thd_val_high), 0);
			}else if(temp == 0){
				rpr410_change_psensor_threshold(obj->client, 0xff, atomic_read(&obj->ps_thd_val_low));
			}
			
			sensor_data.values[0] = temp;
			sensor_data.value_divide = 1;
			sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM; 		
					
			//let up layer to know
			if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
			{
			     APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
			}
		}
	}
	
	mt_eint_unmask(CUST_EINT_ALS_NUM);      
}


/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int rpr410_open(struct inode *inode, struct file *file)
{
	file->private_data = rpr410_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int rpr410_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
static long rpr410_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct rpr410_priv *obj = i2c_get_clientdata(client);  
	long err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;
	int ps_cali;
	int threshold[2];
	int ps_result;
	
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
				if(err = rpr410_enable_ps(obj->client, 1))
				{
					APS_ERR("enable ps fail: %ld\n", err); 
					goto err_out;
				}
				
				set_bit(CMC_BIT_PS, &obj->enable);
			}
			else
			{
				if(err = rpr410_enable_ps(obj->client, 0))
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
			if(err = rpr410_read_ps(obj->client, &obj->ps))
			{
				goto err_out;
			}
			
			dat = rpr410_get_ps_value(obj, obj->ps);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}  
			break;

		case ALSPS_GET_PS_RAW_DATA:    
			if(err = rpr410_read_ps(obj->client, &obj->ps))
			{
				goto err_out;
			}
			APS_DBG("IOCTL PS rawdata = %d\n", obj->ps);
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
				if(err = rpr410_enable_als(obj->client, 1))
				{
					APS_ERR("enable als fail: %d\n", err); 
					goto err_out;
				}
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
				if(err = rpr410_enable_als(obj->client, 0))
				{
					APS_ERR("disable als fail: %d\n", err); 
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
			if(err = rpr410_read_als(obj->client, &obj->als))
			{
				goto err_out;
			}

			dat = rpr410_get_als_value(obj, obj->als);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;

		case ALSPS_GET_ALS_RAW_DATA:    
			if(err = rpr410_read_als(obj->client, &obj->als))
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

		case ALSPS_IOCTL_CLR_CALI:
			if(copy_from_user(&dat, ptr, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			if(dat == 0)
				obj->ps_cali_val = 0;
			break;

		case ALSPS_IOCTL_GET_CALI:
			ps_cali = obj->ps_cali_val;
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
			
			obj->ps_cali_val = ps_cali;
			break;

		case ALSPS_SET_PS_THRESHOLD:
			if(copy_from_user(threshold, ptr, sizeof(threshold)))
			{
				err = -EFAULT;
				goto err_out;
			}
			APS_ERR("%s set threshold high: 0x%x, low: 0x%x\n", __func__, threshold[0],threshold[1]); 
			atomic_set(&obj->ps_thd_val_high,  (threshold[0]+obj->ps_cali_val));
			atomic_set(&obj->ps_thd_val_low,  (threshold[1]+obj->ps_cali_val));//need to confirm
			rpr410_set_psensor_threshold(obj->client);

			break;
				
		case ALSPS_GET_PS_THRESHOLD_HIGH:
			threshold[0] = atomic_read(&obj->ps_thd_val_high) - obj->ps_cali_val;
			APS_ERR("%s get threshold high: 0x%x\n", __func__, threshold[0]); 
			if(copy_to_user(ptr, &threshold[0], sizeof(threshold[0])))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;
			
		case ALSPS_GET_PS_THRESHOLD_LOW:
			threshold[0] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali_val;
			APS_ERR("%s get threshold low: 0x%x\n", __func__, threshold[0]); 
			if(copy_to_user(ptr, &threshold[0], sizeof(threshold[0])))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;
		case ALSPS_GET_PS_TEST_RESULT:
			if((err = rpr410_read_ps(obj->client, &obj->ps)))
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

		default:
			APS_ERR("%s not supported = 0x%04x\n", __FUNCTION__, cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	err_out:
	return err;    
}
/*----------------------------------------------------------------------------*/
static struct file_operations rpr410_fops = {
	.owner = THIS_MODULE,
	.open = rpr410_open,
	.release = rpr410_release,
	.unlocked_ioctl = rpr410_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice rpr410_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &rpr410_fops,
};
/*----------------------------------------------------------------------------*/
static int rpr410_i2c_suspend(struct i2c_client *client, pm_message_t msg) 
{
	APS_FUN();    

	return 0;
}
/*----------------------------------------------------------------------------*/
static int rpr410_i2c_resume(struct i2c_client *client)
{
	APS_FUN();

	return 0;
}
/*----------------------------------------------------------------------------*/
static void rpr410_early_suspend(struct early_suspend *h) 
{   /*early_suspend is only applied for ALS*/
	struct rpr410_priv *obj = container_of(h, struct rpr410_priv, early_drv);   
	int err;
	APS_FUN();    

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

	if(test_bit(CMC_BIT_PS, &obj->enable))
	{
	}
	else
	{
		atomic_set(&obj->als_suspend, 1);
		if(test_bit(CMC_BIT_ALS, &obj->enable))
		{
			if(err = rpr410_enable_als(obj->client, 0))
			{
				APS_ERR("disable als fail: %d\n", err); 
			}
		}
	}
}
/*----------------------------------------------------------------------------*/
static void rpr410_late_resume(struct early_suspend *h)
{   /*early_suspend is only applied for ALS*/
	struct rpr410_priv *obj = container_of(h, struct rpr410_priv, early_drv);         
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

    if(0 != atomic_read(&obj->als_suspend))
    {
		atomic_set(&obj->als_suspend, 0);
		if(test_bit(CMC_BIT_ALS, &obj->enable))
		{
			if(err = rpr410_enable_als(obj->client, 1))
			{
				APS_ERR("enable als fail: %d\n", err);        

			}
		}
	}
}

int rpr410_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct rpr410_priv *obj = (struct rpr410_priv *)self;
	
	//APS_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{				
				value = *(int *)buff_in;
				if(value)
				{
					if(err = rpr410_enable_ps(obj->client, 1))
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_PS, &obj->enable);
				}
				else
				{
					if(err = rpr410_enable_ps(obj->client, 0))
					{
						APS_ERR("disable ps fail: %d\nREG_PSTH_MBS", err); 
						return -1;
					}
					clear_bit(CMC_BIT_PS, &obj->enable);
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;	
				rpr410_read_ps(obj->client, &obj->ps);
				
				APS_ERR("rpr410_ps_operate als data=%d!\n",obj->als);
				sensor_data->values[0] = rpr410_get_ps_value(obj, obj->ps);
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;			
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

int rpr410_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct rpr410_priv *obj = (struct rpr410_priv *)self;

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;				
				if(value)
				{
					if(err = rpr410_enable_als(obj->client, 1))
					{
						APS_ERR("enable als fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					if(err = rpr410_enable_als(obj->client, 0))
					{
						APS_ERR("disable als fail: %d\n", err); 
						return -1;
					}
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}
				
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
	
				sensor_data = (hwm_sensor_data *)buff_out;
				rpr410_read_als(obj->client, &obj->als);
				sensor_data->values[0] = rpr410_get_als_value(obj, obj->als);
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;
		default:
			APS_ERR("light sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}


/*----------------------------------------------------------------------------*/
static int rpr410_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	strcpy(info->type, RPR410_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int rpr410_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct rpr410_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	rpr410_obj = obj;

	obj->hw = get_cust_alsps_hw();
	rpr410_get_addr(obj->hw, &obj->addr);

	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	INIT_WORK(&obj->eint_work, rpr410_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);	
	atomic_set(&obj->als_debounce, 50);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 10);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->ps_cmd_val,  0xC1);
	atomic_set(&obj->ps_thd_val_high,  obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low,  obj->hw->ps_threshold_low);
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->ps_cali_val = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);  
	
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);
	rpr410_i2c_client = client;

	if(err = rpr410_init_client(client))
	{
		goto exit_init_failed;
	}
	APS_LOG("rpr410_init_client() OK!\n");

	if(err = misc_register(&rpr410_device))
	{
		APS_ERR("rpr410_device register failed\n");
		goto exit_misc_device_register_failed;
	}
/*
	if(err = rpr410_create_attr(&rpr410_alsps_driver.driver))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
*/
	obj_ps.self = rpr410_obj;
	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	if(1 == obj->hw->polling_mode_ps)
	//if (1)
	{
		obj_ps.polling = 1;
	}
	else
	{
		obj_ps.polling = 0;
	}

	obj_ps.sensor_operate = rpr410_ps_operate;
	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	
	obj_als.self = rpr410_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = rpr410_als_operate;
	if(err = hwmsen_attach(ID_LIGHT, &obj_als))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = rpr410_early_suspend,
	obj->early_drv.resume   = rpr410_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif

	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&rpr410_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
	//exit_kfree:
	kfree(obj);
	exit:
	rpr410_i2c_client = NULL;           
//	MT6516_EINTIRQMask(CUST_EINT_ALS_NUM);  /*mask interrupt if fail*/
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int rpr410_i2c_remove(struct i2c_client *client)
{
	int err;	
/*	
	if(err = rpr410_delete_attr(&rpr410_i2c_driver.driver))
	{
		APS_ERR("rpr410_delete_attr fail: %d\n", err);
	} 
*/
	if(err = misc_deregister(&rpr410_device))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
	
	rpr410_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
static int rpr410_probe(struct platform_device *pdev) 
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	rpr410_power(hw, 1);    
	//rpr410_force[0] = hw->i2c_num;
	//rpr410_force[1] = hw->i2c_addr[0];
	//APS_DBG("I2C = %d, addr =0x%x\n",rpr410_force[0],rpr410_force[1]);
	if(i2c_add_driver(&rpr410_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int rpr410_remove(struct platform_device *pdev)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();    
	rpr410_power(hw, 0);    
	i2c_del_driver(&rpr410_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver rpr410_alsps_driver = {
	.probe      = rpr410_probe,
	.remove     = rpr410_remove,    
	.driver     = {
		.name  = "als_ps",
	}
};
/*----------------------------------------------------------------------------*/
static int __init rpr410_init(void)
{
	APS_FUN();
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_RPR410, 1);
	if(platform_driver_register(&rpr410_alsps_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit rpr410_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&rpr410_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(rpr410_init);
module_exit(rpr410_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Dexiang Liu");
MODULE_DESCRIPTION("rpr410 driver");
MODULE_LICENSE("GPL");


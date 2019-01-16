/* drivers/hwmon/mt6516/amit/TMD2771.c - TMD2771 ALS/PS driver
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
#include "tmd2771.h"
#include <linux/sched.h>
/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define TMD2771_DEV_NAME     "TMD2771"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_ERR APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_ERR APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_ERR APS_TAG fmt, ##args) 

#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1


/******************************************************************************
 * extern functions
*******************************************************************************/
extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
extern void mt_eint_print_status(void);

/*----------------------------------------------------------------------------*/
static struct i2c_client *TMD2771_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id TMD2771_i2c_id[] = {{TMD2771_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_TMD2771={ I2C_BOARD_INFO("TMD2771", 0x39)};
/*----------------------------------------------------------------------------*/
static int TMD2771_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int TMD2771_i2c_remove(struct i2c_client *client);
static int TMD2771_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int TMD2771_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int TMD2771_i2c_resume(struct i2c_client *client);

static unsigned long long int_top_time = 0;

static DEFINE_MUTEX(TMD2771_mutex);

extern struct alsps_hw *TMD2771_get_cust_alsps_hw(void);
	
static int  TMD2771_local_init(void);
static int  TMD2771_remove(void);
static int TMD2771_init_flag =-1; // 0<==>OK -1 <==> fail

static struct sensor_init_info TMD2771_init_info = {
		.name = "TMD2771",
		.init = TMD2771_local_init,
		.uninit = TMD2771_remove,
};



static struct TMD2771_priv *g_TMD2771_ptr = NULL;

 struct PS_CALI_DATA_STRUCT
{
    int close;
    int far_away;
    int valid;
} ;

static struct PS_CALI_DATA_STRUCT ps_cali={0,0,0};
static int intr_flag_value = 0;
/*----------------------------------------------------------------------------*/
typedef enum {
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;
/*----------------------------------------------------------------------------*/
struct TMD2771_i2c_addr {    /*define a series of i2c slave address*/
    u8  write_addr;  
    u8  ps_thd;     /*PS INT threshold*/
};
/*----------------------------------------------------------------------------*/
struct TMD2771_priv {
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct work_struct  eint_work;

    /*i2c address group*/
    struct TMD2771_i2c_addr  addr;
    
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
	int			ps_cali;

    atomic_t    als_cmd_val;    /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_cmd_val;     /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_thd_val_high;     /*the cmd value can't be read, stored in ram*/
	atomic_t    ps_thd_val_low;     /*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_high;	 /*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_low; 	/*the cmd value can't be read, stored in ram*/
    ulong       enable;         /*enable mask*/
    ulong       pending_intr;   /*pending interrupt*/

    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver TMD2771_i2c_driver = {	
	.probe      = TMD2771_i2c_probe,
	.remove     = TMD2771_i2c_remove,
	.detect     = TMD2771_i2c_detect,
	.suspend    = TMD2771_i2c_suspend,
	.resume     = TMD2771_i2c_resume,
	.id_table   = TMD2771_i2c_id,
	.driver = {
		.name           = TMD2771_DEV_NAME,
	},
};

static struct TMD2771_priv *TMD2771_obj = NULL;
//static struct platform_driver TMD2771_alsps_driver;
/*------------------------i2c function for MTK-------------------------------------*/
int TMD2771_i2c_master_operate(struct i2c_client *client, const char *buf, int count, int i2c_flag)
{
	int res = 0;
	mutex_lock(&TMD2771_mutex);
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
	APS_LOG("TMD2771_i2c_master_operate i2c_flag command not support!\n");
	break;
	}
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	mutex_unlock(&TMD2771_mutex);
	return res;
	EXIT_ERR:
	mutex_unlock(&TMD2771_mutex);
	APS_ERR("TMD2771_i2c_transfer fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/
int TMD2771_get_addr(struct alsps_hw *hw, struct TMD2771_i2c_addr *addr)
{
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->write_addr= hw->i2c_addr[0];
	return 0;
}
/*----------------------------------------------------------------------------*/
static void TMD2771_power(struct alsps_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "TMD2771")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "TMD2771")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
}
/*----------------------------------------------------------------------------*/
static long TMD2771_enable_als(struct i2c_client *client, int enable)
{
	struct TMD2771_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];	  
	long res = 0;

	databuf[0]= TMD2771_CMM_ENABLE;
	res = TMD2771_i2c_master_operate(client, databuf, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	APS_LOG("TMD2771_CMM_ENABLE als value = %x\n",databuf[0]);
	
	if(enable)
		{
			databuf[1] = databuf[0]|0x03;
			databuf[0] = TMD2771_CMM_ENABLE;
			APS_LOG("TMD2771_CMM_ENABLE enable als value = %x\n",databuf[1]);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
			atomic_set(&obj->als_deb_on, 1);
			atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
		}
	else {
		if(test_bit(CMC_BIT_PS, &obj->enable))
			databuf[1] = databuf[0]&0xFD;
		else
			databuf[1] = databuf[0]&0xF8;
		
			databuf[0] = TMD2771_CMM_ENABLE;
			APS_LOG("TMD2771_CMM_ENABLE disable als value = %x\n",databuf[1]);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
		}
	return 0;
		
EXIT_ERR:
	APS_ERR("TMD2771_enable_als fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/
static long TMD2771_enable_ps(struct i2c_client *client, int enable)
{
	struct TMD2771_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	long res = 0;
	
	databuf[0]= TMD2771_CMM_ENABLE;
	res = TMD2771_i2c_master_operate(client, databuf, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	
	APS_LOG("TMD2771_CMM_ENABLE ps value = %x\n",databuf[0]);
	
	if(enable)
		{
			databuf[1] = databuf[0]|0x05;
			databuf[0] = TMD2771_CMM_ENABLE;
			APS_LOG("TMD2771_CMM_ENABLE enable ps value = %x\n",databuf[1]);	
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
			atomic_set(&obj->ps_deb_on, 1);
			atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
			mt_eint_unmask(CUST_EINT_ALS_NUM);
		}
	else{
		if(test_bit(CMC_BIT_ALS, &obj->enable))
			databuf[1] = databuf[0]&0xFB;
		else
			databuf[1] = databuf[0]&0xF8;
		
			databuf[0] = TMD2771_CMM_ENABLE;
			APS_LOG("TMD2771_CMM_ENABLE disable ps value = %x\n",databuf[1]);	
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
			mt_eint_mask(CUST_EINT_ALS_NUM);

			databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;	
			databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;	
			databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_low)) & 0xFF00) >> 8);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;	
			databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high)) & 0x00FF);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;	
			databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_high)) & 0xFF00) >> 8);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				goto EXIT_ERR;
			}
		}
	return 0;
	
EXIT_ERR:
	APS_ERR("TMD2771_enable_ps fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
static int TMD2771_check_and_clear_intr(struct i2c_client *client) 
{
	int res,intp,intl;
	u8 buffer[2];

	if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/  
	    return 0;

	buffer[0] = TMD2771_CMM_STATUS;
	res = TMD2771_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	
	res = 0;
	intp = 0;
	intl = 0;
	if(0 != (buffer[0] & 0x20))
	{
		res = 1;
		intp = 1;
	}
	if(0 != (buffer[0] & 0x10))
	{
		res = 1;
		intl = 1;		
	}

	if(1 == res)
	{
		if((1 == intp) && (0 == intl))
		{
			buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x05);
		}
		else if((0 == intp) && (1 == intl))
		{
			buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x06);
		}
		else
		{
			buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x07);
		}

		res = TMD2771_i2c_master_operate(client, buffer, 0x1, I2C_FLAG_WRITE);
		if(res < 0)
		{
			goto EXIT_ERR;
		}
		else
		{
			res = 0;
		}
	}

	return res;

EXIT_ERR:
	APS_ERR("TMD2771_check_and_clear_intr fail\n");
	return 1;
}
/*----------------------------------------------------------------------------*/

/*yucong add for interrupt mode support MTK inc 2012.3.7*/
static int TMD2771_check_intr(struct i2c_client *client) 
{
	int res,intp,intl;
	u8 buffer[2];

	if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/  
	return 0;

	buffer[0] = TMD2771_CMM_STATUS;
	res = TMD2771_i2c_master_operate(client, buffer, 0x101, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	res = 0;
	intp = 0;
	intl = 0;
	if(0 != (buffer[0] & 0x20))
	{
		res = 0;
		intp = 1;
	}
	if(0 != (buffer[0] & 0x10))
	{
		res = 0;
		intl = 1;		
	}

	return res;

EXIT_ERR:
	APS_ERR("TMD2771_check_intr fail\n");
	return 1;
}

static int TMD2771_clear_intr(struct i2c_client *client) 
{
	int res;
	u8 buffer[2];
	
	buffer[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x07);
	res = TMD2771_i2c_master_operate(client, buffer, 0x1, I2C_FLAG_WRITE);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	else
	{
		res = 0;
	}
	return res;

EXIT_ERR:
	APS_ERR("TMD2771_check_and_clear_intr fail\n");
	return 1;
}


/*-----------------------------------------------------------------------------*/
void TMD2771_eint_func(void)
{
	struct TMD2771_priv *obj = g_TMD2771_ptr;
	if(!obj)
	{
		return;
	}
	int_top_time = sched_clock();
	schedule_work(&obj->eint_work);
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
int TMD2771_setup_eint(struct i2c_client *client)
{
	struct TMD2771_priv *obj = i2c_get_clientdata(client);        

	g_TMD2771_ptr = obj;
	
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, TMD2771_eint_func, 0);

	//mt_eint_unmask(CUST_EINT_ALS_NUM);  
	mt_eint_mask(CUST_EINT_ALS_NUM);  
    return 0;
}

/*----------------------------------------------------------------------------*/

static int TMD2771_init_client(struct i2c_client *client)
{
	struct TMD2771_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	int res = 0;
	
	databuf[0] = (TAOS_TRITON_CMD_REG|TAOS_TRITON_CMD_SPL_FN|0x00);
	res = TMD2771_i2c_master_operate(client, databuf, 0x1, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" TMD2771_init_client function 1 err!\n");
		goto EXIT_ERR;
	}

	databuf[0] = TMD2771_CMM_ENABLE;
	if(obj->hw->polling_mode_ps == 1)
	databuf[1] = 0x08;
	if(obj->hw->polling_mode_ps == 0)
	databuf[1] = 0x28;
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" TMD2771_init_client function 2 err!\n");		
		goto EXIT_ERR;
	}
	
	databuf[0] = TMD2771_CMM_ATIME;    
	databuf[1] = 0xF6;
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" TMD2771_init_client function 3 err!\n");
		goto EXIT_ERR;
	}

	databuf[0] = TMD2771_CMM_PTIME;    
	databuf[1] = 0xFF;
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" TMD2771_init_client function 4 err!\n");
		goto EXIT_ERR;
	}

	databuf[0] = TMD2771_CMM_WTIME;    
	databuf[1] = 0xFC;
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" TMD2771_init_client function 5 err!\n");
		goto EXIT_ERR;
	}
	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	if(0 == obj->hw->polling_mode_ps)
	{
		if(1 == ps_cali.valid)
		{
			databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;	
			databuf[1] = (u8)(ps_cali.far_away & 0x00FF);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG(" TMD2771_init_client function 6 err!\n");
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;	
			databuf[1] = (u8)((ps_cali.far_away & 0xFF00) >> 8);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG(" TMD2771_init_client function 7 err!\n");
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;	
			databuf[1] = (u8)(ps_cali.close & 0x00FF);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG(" TMD2771_init_client function 8 err!\n");
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;	
			databuf[1] = (u8)((ps_cali.close & 0xFF00) >> 8);;
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG(" TMD2771_init_client function 9 err!\n");
				goto EXIT_ERR;
			}
		}
		else
		{
			databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;	
			databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG(" TMD2771_init_client function 10 err!\n");
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;	
			databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_low)) & 0xFF00) >> 8);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG(" TMD2771_init_client function 11 err!\n");
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;	
			databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high)) & 0x00FF);
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG(" TMD2771_init_client function 12 err!\n");
				goto EXIT_ERR;
			}
			databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH;	
			databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_high)) & 0xFF00) >> 8);;
			res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_LOG(" TMD2771_init_client function 13 err!\n");
				goto EXIT_ERR;
			}

		}

		databuf[0] = TMD2771_CMM_Persistence;
		databuf[1] = 0x30;
		res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
		if(res < 0)
		{
			APS_LOG(" TMD2771_init_client function 14 err!\n");
			goto EXIT_ERR;
		}

	}

	databuf[0] = TMD2771_CMM_CONFIG;    
	databuf[1] = 0x00;
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" TMD2771_init_client function 15 err!\n");
		goto EXIT_ERR;
	}

       /*Lenovo-sw chenlj2 add 2011-06-03,modified pulse 2  to 4 */
	databuf[0] = TMD2771_CMM_PPCOUNT;    
	databuf[1] = TMD2771_CMM_PPCOUNT_VALUE;
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" TMD2771_init_client function 16 err!\n");
		goto EXIT_ERR;
	}

        /*Lenovo-sw chenlj2 add 2011-06-03,modified gain 16  to 1 */
	databuf[0] = TMD2771_CMM_CONTROL;    
	databuf[1] = TMD2771_CMM_CONTROL_VALUE;
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" TMD2771_init_client function 17 err!\n");
		goto EXIT_ERR;
	}
	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	if((res = TMD2771_setup_eint(client))!=0)
	{
		APS_ERR("setup eint: %d\n", res);
		return res;
	}
	if((res = TMD2771_check_and_clear_intr(client)))
	{
		APS_ERR("check/clear intr: %d\n", res);
	    return res;
	}
	
	return TMD2771_SUCCESS;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;
}

/****************************************************************************** 
 * Function Configuration
******************************************************************************/
int TMD2771_read_als(struct i2c_client *client, u16 *data)
{	 
	struct TMD2771_priv *obj = i2c_get_clientdata(client);
	u16 c0_value, c1_value;	 
	u32 c0_nf, c1_nf;
	u8 buffer[2];
	u16 atio;
	int res = 0;

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
	
	buffer[0]=TMD2771_CMM_C0DATA_L;
	res = TMD2771_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	
	c0_value = buffer[0] | (buffer[1]<<8);
	c0_nf = obj->als_modulus*c0_value;
	//APS_LOG("c0_value=%d, c0_nf=%d, als_modulus=%d\n", c0_value, c0_nf, obj->als_modulus);

	buffer[0]=TMD2771_CMM_C1DATA_L;
	res = TMD2771_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	
	c1_value = buffer[0] | (buffer[1]<<8);
	c1_nf = obj->als_modulus*c1_value;	
	//APS_LOG("c1_value=%d, c1_nf=%d, als_modulus=%d\n", c1_value, c1_nf, obj->als_modulus);
	
	if((c0_value > c1_value) &&(c0_value < 50000))
	{  	/*Lenovo-sw chenlj2 add 2011-06-03,add {*/
		atio = (c1_nf*100)/c0_nf;

	//APS_LOG("atio = %d\n", atio);
	if(atio<30)
	{
		*data = (13*c0_nf - 24*c1_nf)/10000;
	}
	else if(atio>= 30 && atio<38) /*Lenovo-sw chenlj2 add 2011-06-03,modify > to >=*/
	{ 
		*data = (16*c0_nf - 35*c1_nf)/10000;
	}
	else if(atio>= 38 && atio<45)  /*Lenovo-sw chenlj2 add 2011-06-03,modify > to >=*/
	{ 
		*data = (9*c0_nf - 17*c1_nf)/10000;
	}
	else if(atio>= 45 && atio<54) /*Lenovo-sw chenlj2 add 2011-06-03,modify > to >=*/
	{ 
		*data = (6*c0_nf - 10*c1_nf)/10000;
	}
	else
		*data = 0;
	/*Lenovo-sw chenlj2 add 2011-06-03,add }*/
    }
	else if (c0_value > 50000)
	{
		*data = 65535;
	}
	else if(c0_value == 0)
        {
                *data = 0;
        }
        else
	{
		APS_DBG("TMD2771_read_als als_value is invalid!!\n");
		return -1;
	}	

	//APS_LOG("TMD2771_read_als als_value_lux = %d\n", *data);
	return 0;	 

	
	
EXIT_ERR:
	APS_ERR("TMD2771_read_ps fail\n");
	return res;
}
int TMD2771_read_als_ch0(struct i2c_client *client, u16 *data)
{	 
	//struct TMD2771_priv *obj = i2c_get_clientdata(client);
	u16 c0_value;	 
	u8 buffer[2];
	int res = 0;
	
	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

//get adc channel 0 value
	buffer[0]=TMD2771_CMM_C0DATA_L;
	res = TMD2771_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	c0_value = buffer[0] | (buffer[1]<<8);
	*data = c0_value;
	//APS_LOG("c0_value=%d\n", c0_value);
	return 0;	 

	
	
EXIT_ERR:
	APS_ERR("TMD2771_read_ps fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/

static int TMD2771_get_als_value(struct TMD2771_priv *obj, u16 als)
{
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
		APS_ERR("TMD2771_get_als_value exceed range\n"); 
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
		//APS_ERR("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);	
		return obj->hw->als_value[idx];
	}
	else
	{
		//APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);    
		return -1;
	}
}
/*----------------------------------------------------------------------------*/
long TMD2771_read_ps(struct i2c_client *client, u16 *data)
{
	struct TMD2771_priv *obj = i2c_get_clientdata(client);	
	u8 buffer[2];
	long res = 0;

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	buffer[0]=TMD2771_CMM_PDATA_L;
	res = TMD2771_i2c_master_operate(client, buffer, 0x201, I2C_FLAG_READ);
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	
	//APS_LOG("yucong TMD2771_read_ps ps_data=%d, low:%d  high:%d", *data, buffer[0], buffer[1]);

	if((buffer[0] | (buffer[1]<<8)) < obj->ps_cali)
		*data = 0;
	else
		*data = (buffer[0] | (buffer[1]<<8)) - obj->ps_cali;

	return 0;    

EXIT_ERR:
	APS_ERR("TMD2771_read_ps fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/
static int TMD2771_get_ps_value(struct TMD2771_priv *obj, u16 ps)
{
	int val;// mask = atomic_read(&obj->ps_mask);
	int invalid = 0;
	static int val_temp=1;

	if(ps_cali.valid == 1)
		{
			if((ps >ps_cali.close))
			{
				val = 0;  /*close*/
				val_temp = 0;
				intr_flag_value = 1;
			}
			
			else if((ps < ps_cali.far_away))
			{
				val = 1;  /*far away*/
				val_temp = 1;
				intr_flag_value = 0;
			}
			else
				val = val_temp;

			APS_LOG("TMD2771_get_ps_value val  = %d",val);
	}
	else
	{
			if((ps  > atomic_read(&obj->ps_thd_val_high)))
			{
				val = 0;  /*close*/
				val_temp = 0;
				intr_flag_value = 1;
			}
			else if((ps  < atomic_read(&obj->ps_thd_val_low)))
			{
				val = 1;  /*far away*/
				val_temp = 1;
				intr_flag_value = 0;
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
	else if (obj->als > 45000)
	{
		//invalid = 1;
		APS_DBG("ligh too high will result to failt proximiy\n");
		return 1;  /*far away*/
	}

	if(!invalid)
	{
		//APS_DBG("PS:  %05d => %05d\n", ps, val);
		return val;
	}	
	else
	{
		return -1;
	}	
}


/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
#define DEBUG_TMD2771
static void TMD2771_eint_work(struct work_struct *work)
{
	struct TMD2771_priv *obj = (struct TMD2771_priv *)container_of(work, struct TMD2771_priv, eint_work);
	int err;
	hwm_sensor_data sensor_data;
	u8 databuf[3];
	int res = 0;
	APS_LOG("TMD2771_eint_work int top half time = %lld\n", int_top_time);
	if((err = TMD2771_check_intr(obj->client)))
	{
		APS_ERR("TMD2771_eint_work check intrs: %d\n", err);
	}
	else
	{
		//get raw data
		TMD2771_read_ps(obj->client, &obj->ps);
		TMD2771_read_als_ch0(obj->client, &obj->als);
		APS_LOG("TMD2771_eint_work rawdata ps=%d als_ch0=%d!\n",obj->ps,obj->als);
		
		if(obj->als > 40000)
			{
			APS_LOG("TMD2771_eint_work ALS too large may under lighting als_ch0=%d!\n",obj->als);
			return;
			}
		sensor_data.values[0] = TMD2771_get_ps_value(obj, obj->ps);
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;	

#ifdef DEBUG_TMD2771
		databuf[0]= TMD2771_CMM_ENABLE;
		res = TMD2771_i2c_master_operate(obj->client, databuf, 0x101, I2C_FLAG_READ);
		if(res < 0)
		{
			goto EXIT_ERR;
		}
		APS_LOG("TMD2771_eint_work TMD2771_CMM_ENABLE ps value = %x\n",databuf[0]);
		
		databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
		res = TMD2771_i2c_master_operate(obj->client, databuf, 0x201, I2C_FLAG_READ);
		if(res < 0)
		{
			goto EXIT_ERR;
		}
		APS_LOG("TMD2771_eint_work TMD2771_CMM_INT_LOW_THD_LOW before databuf[0]=%d databuf[1]=%d!\n",databuf[0],databuf[1]);

		databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
		res = TMD2771_i2c_master_operate(obj->client, databuf, 0x201, I2C_FLAG_READ);
		if(res < 0)
		{
			goto EXIT_ERR;
		}
		APS_LOG("TMD2771_eint_work TMD2771_CMM_INT_HIGH_THD_LOW before databuf[0]=%d databuf[1]=%d!\n",databuf[0],databuf[1]);
#endif
/*singal interrupt function add*/
		if(intr_flag_value){
						databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;	
						databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
						res = TMD2771_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
						if(res < 0)
						{
							goto EXIT_ERR;
						}
						
						databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;	
						databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_low)) & 0xFF00) >> 8);
						res = TMD2771_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
						if(res < 0)
						{
							goto EXIT_ERR;
						}
						databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;	
						databuf[1] = (u8)(0x00FF);
						res = TMD2771_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
						if(res < 0)
						{
							goto EXIT_ERR;
						}
						
						databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH; 
						databuf[1] = (u8)((0xFF00) >> 8);
						res = TMD2771_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
						if(res < 0)
						{
							goto EXIT_ERR;
						}
						
				}
				else{	
						databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;	
						databuf[1] = (u8)(0 & 0x00FF);
						res = TMD2771_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
						if(res < 0)
						{
							goto EXIT_ERR;
						}
						
						databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;	
						databuf[1] = (u8)((0 & 0xFF00) >> 8);
						res = TMD2771_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
						if(res < 0)
						{
							goto EXIT_ERR;
						}
						
						databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;	
						databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high)) & 0x00FF);
						res = TMD2771_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
						if(res < 0)
						{
							goto EXIT_ERR;
						}
					
						databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH; 
						databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_high)) & 0xFF00) >> 8);
						res = TMD2771_i2c_master_operate(obj->client, databuf, 0x2, I2C_FLAG_WRITE);
						if(res < 0)
						{
							goto EXIT_ERR;
						}
				}
				
		//let up layer to know
		#ifdef DEBUG_TMD2771
		databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;
		res = TMD2771_i2c_master_operate(obj->client, databuf, 0x201, I2C_FLAG_READ);
		if(res < 0)
		{
			goto EXIT_ERR;
		}
		APS_LOG("TMD2771_eint_work TMD2771_CMM_INT_LOW_THD_LOW after databuf[0]=%d databuf[1]=%d!\n",databuf[0],databuf[1]);

		databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;
		res = TMD2771_i2c_master_operate(obj->client, databuf, 0x201, I2C_FLAG_READ);
		if(res < 0)
		{
			goto EXIT_ERR;
		}
		APS_LOG("TMD2771_eint_work TMD2771_CMM_INT_HIGH_THD_LOW after databuf[0]=%d databuf[1]=%d!\n",databuf[0],databuf[1]);
		#endif
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
		  APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}
	
	TMD2771_clear_intr(obj->client);
	mt_eint_unmask(CUST_EINT_ALS_NUM); 
	return;
	EXIT_ERR:
	TMD2771_clear_intr(obj->client);
	mt_eint_unmask(CUST_EINT_ALS_NUM); 
	APS_ERR("i2c_transfer error = %d\n", res);
	return;
}


/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int TMD2771_open(struct inode *inode, struct file *file)
{
	file->private_data = TMD2771_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int TMD2771_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int set_psensor_threshold(struct i2c_client *client)
{
	struct TMD2771_priv *obj = i2c_get_clientdata(client); 
	u8 databuf[3];    
	int res = 0;
	APS_ERR("set_psensor_threshold function high: 0x%x, low:0x%x\n",atomic_read(&obj->ps_thd_val_high),atomic_read(&obj->ps_thd_val_low));
	
	databuf[0] = TMD2771_CMM_INT_LOW_THD_LOW;	
	databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_low) & 0x00FF);
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" set_psensor_threshold 1 err!\n");
		return res;
	}
	databuf[0] = TMD2771_CMM_INT_LOW_THD_HIGH;	
	databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low) & 0xFF00) >> 8);
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" set_psensor_threshold 2 err!\n");
		return res;
	}
	databuf[0] = TMD2771_CMM_INT_HIGH_THD_LOW;	
	databuf[1] = (u8)(atomic_read(&obj->ps_thd_val_high) & 0x00FF);
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" set_psensor_threshold 3 err!\n");
		return res;
	}
	databuf[0] = TMD2771_CMM_INT_HIGH_THD_HIGH; 
	databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high) & 0xFF00) >> 8);;
	res = TMD2771_i2c_master_operate(client, databuf, 0x2, I2C_FLAG_WRITE);
	if(res < 0)
	{
		APS_LOG(" set_psensor_threshold 4 err!\n");
		return res;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static long TMD2771_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct TMD2771_priv *obj = i2c_get_clientdata(client);  
	long err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;
	int ps_result;
		int ps_cali;
		int threshold[2];

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
				if((err = TMD2771_enable_ps(obj->client, 1)))
				{
					APS_ERR("enable ps fail: %ld\n", err); 
					goto err_out;
				}
				
				set_bit(CMC_BIT_PS, &obj->enable);
			}
			else
			{
				if((err = TMD2771_enable_ps(obj->client, 0)))
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
			if((err = TMD2771_read_ps(obj->client, &obj->ps)))
			{
				goto err_out;
			}
			
			dat = TMD2771_get_ps_value(obj, obj->ps);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}  
			break;

		case ALSPS_GET_PS_RAW_DATA:    
			if((err = TMD2771_read_ps(obj->client, &obj->ps)))
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
				if((err = TMD2771_enable_als(obj->client, 1)))
				{
					APS_ERR("enable als fail: %ld\n", err); 
					goto err_out;
				}
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
				if((err = TMD2771_enable_als(obj->client, 0)))
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
			if((err = TMD2771_read_als(obj->client, &obj->als)))
			{
				goto err_out;
			}

			dat = TMD2771_get_als_value(obj, obj->als);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;

		case ALSPS_GET_ALS_RAW_DATA:    
			if((err = TMD2771_read_als(obj->client, &obj->als)))
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
			if((err = TMD2771_read_ps(obj->client, &obj->ps)))
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

				set_psensor_threshold(obj->client);
				
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
/*----------------------------------------------------------------------------*/
static struct file_operations TMD2771_fops = {
	.owner = THIS_MODULE,
	.open = TMD2771_open,
	.release = TMD2771_release,
	.unlocked_ioctl = TMD2771_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice TMD2771_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &TMD2771_fops,
};
/*----------------------------------------------------------------------------*/
static int TMD2771_i2c_suspend(struct i2c_client *client, pm_message_t msg) 
{
//	struct TMD2771_priv *obj = i2c_get_clientdata(client);    
//	int err;
	APS_FUN();    
#if 0
	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return -EINVAL;
		}
		
		atomic_set(&obj->als_suspend, 1);
		if(err = TMD2771_enable_als(client, 0))
		{
			APS_ERR("disable als: %d\n", err);
			return err;
		}

		atomic_set(&obj->ps_suspend, 1);
		if(err = TMD2771_enable_ps(client, 0))
		{
			APS_ERR("disable ps:  %d\n", err);
			return err;
		}
		
		TMD2771_power(obj->hw, 0);
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
static int TMD2771_i2c_resume(struct i2c_client *client)
{
//	struct TMD2771_priv *obj = i2c_get_clientdata(client);        
//	int err;
	APS_FUN();
#if 0
	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	TMD2771_power(obj->hw, 1);
	if(err = TMD2771_init_client(client))
	{
		APS_ERR("initialize client fail!!\n");
		return err;        
	}
	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		if(err = TMD2771_enable_als(client, 1))
		{
			APS_ERR("enable als fail: %d\n", err);        
		}
	}
	atomic_set(&obj->ps_suspend, 0);
	if(test_bit(CMC_BIT_PS,  &obj->enable))
	{
		if(err = TMD2771_enable_ps(client, 1))
		{
			APS_ERR("enable ps fail: %d\n", err);                
		}
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
static void TMD2771_early_suspend(struct early_suspend *h) 
{   /*early_suspend is only applied for ALS*/
	struct TMD2771_priv *obj = container_of(h, struct TMD2771_priv, early_drv);   
	int err;
	APS_FUN();    

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}
	
	#if 1
	atomic_set(&obj->als_suspend, 1);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		if((err = TMD2771_enable_als(obj->client, 0)))
		{
			APS_ERR("disable als fail: %d\n", err); 
		}
	}
	#endif
}
/*----------------------------------------------------------------------------*/
static void TMD2771_late_resume(struct early_suspend *h)
{   /*early_suspend is only applied for ALS*/
	struct TMD2771_priv *obj = container_of(h, struct TMD2771_priv, early_drv);         
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

    #if 1
	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		if((err = TMD2771_enable_als(obj->client, 1)))
		{
			APS_ERR("enable als fail: %d\n", err);        

		}
	}
	#endif
}
/*----------------------------------------------------------------------------*/
static int temp_als = 0;
static int ALS_FLAG = 0;

int TMD2771_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int value;
	int err = 0;
	
	hwm_sensor_data* sensor_data;
	struct TMD2771_priv *obj = (struct TMD2771_priv *)self;
	
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
					if((err = TMD2771_enable_ps(obj->client, 1)))
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_PS, &obj->enable);
					#if 1
					if(!test_bit(CMC_BIT_ALS, &obj->enable))
					{
						ALS_FLAG = 1;
						if((err = TMD2771_enable_als(obj->client, 1)))
						{
							APS_ERR("enable als fail: %d\n", err); 
							return -1;
						}
					}
					#endif
				}
				else
				{
					if((err = TMD2771_enable_ps(obj->client, 0)))
					{
						APS_ERR("disable ps fail: %d\n", err); 
						return -1;
					}
					clear_bit(CMC_BIT_PS, &obj->enable);
					#if 1
					if(ALS_FLAG == 1)
					{
						if((err = TMD2771_enable_als(obj->client, 0)))
						{
							APS_ERR("disable als fail: %d\n", err); 
							return -1;
						}
						ALS_FLAG = 0;
					}
					#endif
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
				TMD2771_read_ps(obj->client, &obj->ps);
				TMD2771_read_als_ch0(obj->client, &obj->als);
				APS_ERR("TMD2771_ps_operate als data=%d!\n",obj->als);
				sensor_data->values[0] = TMD2771_get_ps_value(obj, obj->ps);
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


int TMD2771_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct TMD2771_priv *obj = (struct TMD2771_priv *)self;

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
					if((err = TMD2771_enable_als(obj->client, 1)))
					{
						APS_ERR("enable als fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					if((err = TMD2771_enable_als(obj->client, 0)))
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
				/*yucong MTK add for fixing known issue*/
				TMD2771_read_als(obj->client, &obj->als);
				#if defined(CONFIG_MTK_AAL_SUPPORT)
				sensor_data->values[0] = obj->als;
				#else
				if(obj->als == 0)
				{
					sensor_data->values[0] = temp_als;				
				}else{
					u16 b[2];
					int i;
					for(i = 0;i < 2;i++){
					TMD2771_read_als(obj->client, &obj->als);
					b[i] = obj->als;
					}
					(b[1] > b[0])?(obj->als = b[0]):(obj->als = b[1]);
					sensor_data->values[0] = TMD2771_get_als_value(obj, obj->als);
					temp_als = sensor_data->values[0];
				}
				#endif
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
static int TMD2771_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	strcpy(info->type, TMD2771_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int TMD2771_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct TMD2771_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	TMD2771_obj = obj;
	obj->hw = TMD2771_get_cust_alsps_hw();
	TMD2771_get_addr(obj->hw, &obj->addr);

	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	INIT_WORK(&obj->eint_work, TMD2771_eint_work);
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
	atomic_set(&obj->als_thd_val_high,  obj->hw->als_threshold_high);
	atomic_set(&obj->als_thd_val_low,  obj->hw->als_threshold_low);
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->ps_cali = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);  
	/*Lenovo-sw chenlj2 add 2011-06-03,modified gain 16 to 1/5 accoring to actual thing */
	obj->als_modulus = (400*100*ZOOM_TIME)/(1*150);//(1/Gain)*(400/Tine), this value is fix after init ATIME and CONTROL register value
										//(400)/16*2.72 here is amplify *100 //16
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);

	
	TMD2771_i2c_client = client;
	
	if(1 == obj->hw->polling_mode_ps)
		//if (1)
		{
			obj_ps.polling = 1;
		}
		else
		{
			obj_ps.polling = 0;
		}
	
	if((err = TMD2771_init_client(client)))
	{
		goto exit_init_failed;
	}
	APS_LOG("TMD2771_init_client() OK!\n");

	if((err = misc_register(&TMD2771_device)))
	{
		APS_ERR("TMD2771_device register failed\n");
		goto exit_misc_device_register_failed;
	}
/*
	if(err = TMD2771_create_attr(&TMD2771_alsps_driver.driver))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
*/
	obj_ps.self = TMD2771_obj;
	
	obj_ps.sensor_operate = TMD2771_ps_operate;
	if((err = hwmsen_attach(ID_PROXIMITY, &obj_ps)))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	
	obj_als.self = TMD2771_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = TMD2771_als_operate;
	if((err = hwmsen_attach(ID_LIGHT, &obj_als)))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = TMD2771_early_suspend,
	obj->early_drv.resume   = TMD2771_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif

	APS_LOG("%s: OK\n", __func__);
	TMD2771_init_flag = 0;
	return 0;

	exit_create_attr_failed:
	misc_deregister(&TMD2771_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
	//exit_kfree:
	kfree(obj);
	exit:
	TMD2771_i2c_client = NULL;           
//	MT6516_EINTIRQMask(CUST_EINT_ALS_NUM);  /*mask interrupt if fail*/
	APS_ERR("%s: err = %d\n", __func__, err);
	TMD2771_init_flag = -1;
	return err;
}
/*----------------------------------------------------------------------------*/
static int TMD2771_i2c_remove(struct i2c_client *client)
{
	int err;	
/*	
	if(err = TMD2771_delete_attr(&TMD2771_i2c_driver.driver))
	{
		APS_ERR("TMD2771_delete_attr fail: %d\n", err);
	} 
*/
	if((err = misc_deregister(&TMD2771_device)))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
	
	TMD2771_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
static int TMD2771_remove(void)
{
    struct alsps_hw *hw = TMD2771_get_cust_alsps_hw();

    APS_FUN();    
    TMD2771_power(hw, 0);    
    i2c_del_driver(&TMD2771_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/

static int TMD2771_local_init(void)
{
   struct alsps_hw *hw = TMD2771_get_cust_alsps_hw();
	APS_FUN();

	TMD2771_power(hw, 1);
	if(i2c_add_driver(&TMD2771_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	}
	if(-1 == TMD2771_init_flag)
	{
	   return -1;
	}
	
	return 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int __init TMD2771_init(void)
{
	//APS_FUN();
	struct alsps_hw *hw = TMD2771_get_cust_alsps_hw();
	APS_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_TMD2771, 1);
	hwmsen_alsps_sensor_add(&TMD2771_init_info);
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit TMD2771_exit(void)
{
	APS_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(TMD2771_init);
module_exit(TMD2771_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Dexiang Liu");
MODULE_DESCRIPTION("TMD2771 driver");
MODULE_LICENSE("GPL");


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

#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>

#include "apm_16d.h"


#include <linux/hwmsen_helper.h>


/******************************************************************************
 * configuration
*******************************************************************************/

/*----------------------------------------------------------------------------*/
#define APM_16D_DEV_NAME     "apm_16d"
/*----------------------------------------------------------------------------*/

#define APM_TAG                  "[ALS/PS] "
#define APM_FUN()                printk( APM_TAG"%s\n", __FUNCTION__)
#define APM_ERR(fmt, args...)    printk( APM_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APM_LOG(fmt, args...)    printk( APM_TAG fmt, ##args)
#define APM_DBG(fmt, args...)    printk( fmt, ##args)

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


/*----------------------------------------------------------------------------*/
static struct i2c_client *apm_16d_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id apm_16d_i2c_id[] = {{APM_16D_DEV_NAME,0},{}};
/*the adapter id & i2c address will be available in customization*/
#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
static struct i2c_board_info __initdata i2c_apm_16d={ I2C_BOARD_INFO("apm_16d", (0x90>>1))};
#else
static struct i2c_board_info __initdata i2c_apm_16d={ I2C_BOARD_INFO("apm_16d", (0x90>>1))};
#endif

//static unsigned short apm_16d_force[] = {0x00, 0x00, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const apm_16d_forces[] = { apm_16d_force, NULL };
//static struct i2c_client_address_data apm_16d_addr_data = { .forces = apm_16d_forces,};
/*----------------------------------------------------------------------------*/
static int apm_16d_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int apm_16d_i2c_remove(struct i2c_client *client);
static int apm_16d_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int apm_16d_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int apm_16d_i2c_resume(struct i2c_client *client);

static struct apm_16d_priv *g_apm_16d_ptr = NULL;
static int intr_flag_value = 0;

#ifndef AGOLD_DEFINED_APM_16D_PS_THD_VALUE
#define AGOLD_DEFINED_APM_16D_PS_THD_VALUE	0x046
#endif
static unsigned int Apm_16d_Current_Ps_Thd_Value = AGOLD_DEFINED_APM_16D_PS_THD_VALUE;

//#define AGOLD_PROX_CALI_ENABLE
//static DEFINE_SPINLOCK(ps_cali_lock);
#if defined(AGOLD_PROX_CALI_ENABLE)
#define  PS_CALI_TIMES 25
struct APM_16D_PS_CALI_DATA_STRUCT
{
    int close;
    int far_away;
    int valid;
    int ppcount;
	int no_barrier_flag;
	int deep_cali_flag;
} ;

static struct APM_16D_PS_CALI_DATA_STRUCT ps_cali={0,0,0,0,0,0};

#endif

/*----------------------------------------------------------------------------*/
typedef enum {
    APM_TRC_ALS_DATA= 0x0001,
    APM_TRC_PS_DATA = 0x0002,
    APM_TRC_EINT    = 0x0004,
    APM_TRC_IOCTL   = 0x0008,
    APM_TRC_I2C     = 0x0010,
    APM_TRC_CVT_ALS = 0x0020,
    APM_TRC_CVT_PS  = 0x0040,
    APM_TRC_DEBUG   = 0x8000,
} APM_TRC;
/*----------------------------------------------------------------------------*/
typedef enum {
    APM_BIT_ALS    = 1,
    APM_BIT_PS     = 2,
} APM_BIT;
/*----------------------------------------------------------------------------*/
struct apm_16d_i2c_addr {    /*define a series of i2c slave address*/
    u8  status;     /*Alert Response Address*/
    u8  init;       /*device initialization */
    u8  als_cmd;    /*ALS command*/
    u8  als_dat1;   /*ALS MSB*/
    u8  als_dat0;   /*ALS LSB*/
    u8  ps_cmd;     /*PS command*/
    u8  ps_dat;     /*PS data*/
    u8  ps_thdh;    /*PS INT threshold*/
	u8  ps_thdl;
};
/*----------------------------------------------------------------------------*/
struct apm_16d_priv {
    struct alsps_hw  *hw;
    struct i2c_client *client;
    //struct delayed_work  eint_work;
	struct work_struct  eint_work;
    /*i2c address group*/
    struct apm_16d_i2c_addr  addr;

    /*misc*/
    atomic_t    trace;
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
    int          ps;
    u8          _align;
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

    atomic_t    als_cmd_val;    /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_cmd_val;     /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_thd_val;     /*the cmd value can't be read, stored in ram*/

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
static struct i2c_driver apm_16d_i2c_driver = {
	.probe      = apm_16d_i2c_probe,
	.remove     = apm_16d_i2c_remove,
	.detect     = apm_16d_i2c_detect,
	.suspend    = apm_16d_i2c_suspend,
	.resume     = apm_16d_i2c_resume,
	.id_table   = apm_16d_i2c_id,
	//.address_data = &apm_16d_addr_data,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = APM_16D_DEV_NAME,
	},
};

#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
extern struct alsps_hw *apm_16d_get_cust_alsps_hw(void);

static int apm_16d_probe(void);
static int apm_16d_remove(void);
static int apm_16d_init_flag = 0;
static struct sensor_init_info apm_16d_init_info = {
	.name = "apm_16d",
	.init = apm_16d_probe,
	.uninit = apm_16d_remove,
};
#endif

static struct apm_16d_priv *apm_16d_obj = NULL;
static struct platform_driver apm_16d_alsps_driver;
static int apm_16d_get_ps_value(struct apm_16d_priv *obj, int ps);
static int apm_16d_get_als_value(struct apm_16d_priv *obj, u16 als);
static int apm_16d_read_als(struct i2c_client *client, u16 *data);
static int apm_16d_read_ps(struct i2c_client *client, int *data);
/*----------------------------------------------------------------------------*/
int apm_16d_get_addr(struct alsps_hw *hw, struct apm_16d_i2c_addr *addr)
{
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->status    = ALSPS_STATUS;
	addr->als_cmd   = ALS_CMD;
	addr->als_dat1  = ALS_DT1;
	addr->als_dat0  = ALS_DT2;
	addr->ps_cmd    = PS_CMD;
	addr->ps_thdh    = PS_THDH;
	addr->ps_thdl    = PS_THDL;
	addr->ps_dat    = PS_DT;
	return 0;
}
/*----------------------------------------------------------------------------*/
int apm_16d_get_timing(void)
{
return 200;
}


/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
int apm_16d_master_recv(struct i2c_client *client, u8 addr, u8 *buf ,int count)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
//	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	int ret = 0, retry = 0;
	int trc = atomic_read(&obj->trace);
	int max_try = atomic_read(&obj->i2c_retry);

	while(retry++ < max_try)
	{
		ret = hwmsen_read_block(client, addr, buf, count);
		if(ret == 0)
            break;
		udelay(100);
	}

	if(unlikely(trc))
	{
		if(trc & APM_TRC_I2C)
		{
			APM_LOG("(recv) %x %d %d %p [%02X]\n", msg.addr, msg.flags, msg.len, msg.buf, msg.buf[0]);
		}

		if((retry != 1) && (trc & APM_TRC_DEBUG))
		{
			APM_LOG("(recv) %d/%d\n", retry-1, max_try);

		}
	}

	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	transmitted, else error code. */
	return (ret == 0) ? count : ret;
}
/*----------------------------------------------------------------------------*/
int apm_16d_master_send(struct i2c_client *client, u8 addr, u8 *buf ,int count)
{
	int ret = 0, retry = 0;
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	//struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msg;
	int trc = atomic_read(&obj->trace);
	int max_try = atomic_read(&obj->i2c_retry);


	while(retry++ < max_try)
	{
		ret = hwmsen_write_block(client, addr, buf, count);
		if (ret == 0)
		    break;
		udelay(100);
	}

	if(unlikely(trc))
	{
		if(trc & APM_TRC_I2C)
		{
			APM_LOG("(send) %x %d %d %p [%02X]\n", msg.addr, msg.flags, msg.len, msg.buf, msg.buf[0]);
		}

		if((retry != 1) && (trc & APM_TRC_DEBUG))
		{
			APM_LOG("(send) %d/%d\n", retry-1, max_try);
		}
	}
	/* If everything went ok (i.e. 1 msg transmitted), return #bytes
	transmitted, else error code. */
	return (ret == 0) ? count : ret;
}

/*----------------------------------------------------------------------------*/
int apm_16d_read_als(struct i2c_client *client, u16 *data)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int ret = 0;
	u8 buf[2];

	if(1 != (ret = apm_16d_master_recv(client, obj->addr.als_dat1, (char*)&buf[1], 1)))
	{
		APM_ERR("reads als data1 = %d\n", ret);
		return -EFAULT;
	}
	else if(1 != (ret = apm_16d_master_recv(client, obj->addr.als_dat0, (char*)&buf[0], 1)))
	{
		APM_ERR("reads als data2 = %d\n", ret);
		return -EFAULT;
	}

	*data = (buf[1] << 8) | (buf[0]);
	if(atomic_read(&obj->trace) & APM_TRC_ALS_DATA)
	{
		APM_DBG("ALS: 0x%04X\n", (u32)(*data));
	}
	return 0;
}

int apm_16d_write_als(struct i2c_client *client, u8 data)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

    ret = apm_16d_master_send(client, obj->addr.als_cmd, &data, 1);
	if(ret < 0)
	{
		APM_ERR("write als = %d\n", ret);
		printk("[%s] 123 i2c addr = 0x%x, als_cmd = 0x%x, data = 0x%x\n", __FUNCTION__, client->addr, obj->addr.als_cmd, data);
		return -EFAULT;
	}
        else
       printk("[%s] cc_i2c addr = 0x%x,cc_ als_cmd = 0x%x, cc_data = 0x%x\n", __FUNCTION__, client->addr, obj->addr.als_cmd, data);

	return 0;
}
/*----------------------------------------------------------------------------*/
int apm_16d_read_ps(struct i2c_client *client, int *data)
{
	u8 buf[1];

	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	if(1 != (ret = apm_16d_master_recv(client, obj->addr.ps_dat, (char*)&buf[0], 1)))
	{
		APM_ERR("reads ps data = %d\n", ret);
		return -EFAULT;
	}
	*data= 0xff & buf[0];

	if(atomic_read(&obj->trace) & APM_TRC_PS_DATA)
	{
		APM_DBG("PS:  0x%04X\n", (u32)(*data));
	}


#if 0	
	//buf[0]=obj->addr.ps_cmd;

	apm_16d_master_recv(client, obj->addr.ps_cmd, buf, 1);
	//apm_16d_master_recv(client, client->addr, buf, 1);
	printk("[%s] read ps cmd = 0x%x\n", __FUNCTION__, buf[0]);
	
	//buf[0]=obj->addr.status;
	apm_16d_master_recv(client, obj->addr.status, buf, 1);
	//apm_16d_master_recv(client, client->addr, buf, 1);
	printk("[%s] read apm status = 0x%x\n", __FUNCTION__, buf[0]);
#endif
	
	return 0;

}
/*----------------------------------------------------------------------------*/
int apm_16d_write_ps(struct i2c_client *client, u8 data)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int ret = 0;

	ret = apm_16d_master_send(client, obj->addr.ps_cmd, &data, 1);
	if (ret < 0)
	{
		APM_ERR("write ps = %d\n", ret);
		return -EFAULT;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/


int apm_16d_write_ps_thd(struct i2c_client *client, u8 thd)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);

	int ret = 0;
	#if defined(AGOLD_DEFINED_APM_16D_PS_THD_VALUE)
	ret = apm_16d_master_send(client, obj->addr.ps_thdh, (u8*)&Apm_16d_Current_Ps_Thd_Value, 1);
	if (ret < 0)
	{
		APM_ERR("write thd = %d\n", ret);
		return -EFAULT;
	}

	ret = apm_16d_master_send(client, obj->addr.ps_thdl, (u8*)&Apm_16d_Current_Ps_Thd_Value, 1);
	if (ret < 0)
	{
		APM_ERR("write thd = %d\n", ret);
		return -EFAULT;
	}
	
	#else
	ret = apm_16d_master_send(client, obj->addr.ps_thdh, (char*)&obj->ps_thd_val_high, 1);
	if (ret < 0)
	{
		APM_ERR("write thd = %d\n", ret);
		return -EFAULT;
	}

	ret = apm_16d_master_send(client, obj->addr.ps_thdl, (char*)&obj->ps_thd_val_low, 1);
	if (ret < 0)
	{
		APM_ERR("write thd = %d\n", ret);
		return -EFAULT;
	}
	#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static void apm_16d_power(struct alsps_hw *hw, unsigned int on)
{
	static unsigned int power_on = 0;

#if (defined(DCT_H958))
	printk("[%s] line = %d on = %d\n", __FUNCTION__, __LINE__, on);
        if(power_on == on)
        {
	        APM_LOG("ignore power control: %d\n", on);
        }
        else if(on)
        {
	       // mt_set_gpio_out(GPIO96, GPIO_OUT_ONE);
        }
        else
        {
	       // mt_set_gpio_out(GPIO96, GPIO_OUT_ZERO);
        }
#endif

	//APM_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APM_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "apm_16d"))
			{
				APM_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "apm_16d"))
			{
				APM_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
}
/*----------------------------------------------------------------------------*/
static int apm_16d_enable_als(struct i2c_client *client, int enable)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int err, cur = 0, old = atomic_read(&obj->als_cmd_val);
	int trc = atomic_read(&obj->trace);
	APM_LOG("[Agold spl] %s, enable = %d.\n",__func__,enable);
	if(enable)
	{
		cur = old & (~SD_ALS);
	}
	else
	{
		cur = old | (SD_ALS);
	}

	if(trc & APM_TRC_DEBUG)
	{
		APM_LOG("%s: %08X, %08X, %d\n", __func__, cur, old, enable);
	}

	if(0 == (cur ^ old))
	{
		return 0;
	}

	if(0 == (err = apm_16d_write_als(client, cur)))
	{
		atomic_set(&obj->als_cmd_val, cur);
	}

	if(enable)
	{
		atomic_set(&obj->als_deb_on, 1);
		atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
		//set_bit(APM_BIT_ALS,  &obj->pending_intr);
		//schedule_delayed_work(&obj->eint_work,260); //after enable the value is not accurate
	}

	if(trc & APM_TRC_DEBUG)
	{
		APM_LOG("enable als (%d)\n", enable);
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int apm_16d_enable_ps(struct i2c_client *client, int enable)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int err, cur = 0, old = atomic_read(&obj->ps_cmd_val);
	int trc = atomic_read(&obj->trace);
//	int ps_cmd_buf[1] = {0};
	APM_LOG("[Agold spl] %s, enable = %d.\n",__func__,enable);
	
	if(enable)
	{
		cur = old & (~SD_PS);
	}
	else
	{
		cur = old | (SD_PS);
	}

	if(trc & APM_TRC_DEBUG)
	{
		APM_LOG("%s: %08X, %08X, %d\n", __func__, cur, old, enable);
	}

	if(0 == (cur ^ old))
	{
		return 0;
	}

	if(0 == (err = apm_16d_write_ps(client, cur)))
	{
		atomic_set(&obj->ps_cmd_val, cur);
	}

	if(enable)
	{
		atomic_set(&obj->ps_deb_on, 1);
		atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
		//set_bit(APM_BIT_PS,  &obj->pending_intr);
		//schedule_delayed_work(&obj->eint_work,120);
		schedule_work(&obj->eint_work);
	}

	if(trc & APM_TRC_DEBUG)
	{
		APM_LOG("enable ps  (%d)\n", enable);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int apm_16d_check_and_clear_intr(struct i2c_client *client)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int err;
	u8 status;

	if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/
	{
		printk("debug==========>apm_16d check int no interrupt\n");
	    	//return 0;
	}

    err = apm_16d_master_recv(client, obj->addr.status, &status, 1);
	if (err < 0)
	{
		APM_ERR("WARNING: read status: %d\n", err);
		return 0;
	}
	/*
	if(status & 0x10)
	{
		set_bit(APM_BIT_ALS, &obj->pending_intr);
	}
	else
	{
	   clear_bit(APM_BIT_ALS, &obj->pending_intr);
	}

	if(status & 0x20)
	{
		set_bit(APM_BIT_PS,  &obj->pending_intr);
	}
	else
	{
	    clear_bit(APM_BIT_PS, &obj->pending_intr);
	}

	if(atomic_read(&obj->trace) & APM_TRC_DEBUG)
	{
		APM_LOG("check intr: 0x%02X => 0x%08lX\n", status, obj->pending_intr);
	}
	*/

	if(status & 0x20)
	{
		status = 0x00;
		err = apm_16d_master_send(client, obj->addr.status, &status, 1);
		if (err < 0)
		{
			APM_ERR("WARNING: clear intrrupt: %d\n", err);
			return 0;
		}
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
void apm_16d_eint_func(void)
{
	struct apm_16d_priv *obj = g_apm_16d_ptr;
	printk(" apm_16d_eint_func=========>interrupt fuc\n");
	if(!obj)
	{
		return;
	}
	schedule_work(&obj->eint_work);
	//schedule_delayed_work(&obj->eint_work,0);
	if(atomic_read(&obj->trace) & APM_TRC_EINT)
	{
		APM_LOG("eint: als/ps intrs\n");
	}
}
/*----------------------------------------------------------------------------*/
static void apm_16d_eint_work(struct work_struct *work)
{
	struct apm_16d_priv *obj = g_apm_16d_ptr;
	int err;
	hwm_sensor_data sensor_data;
//	u8 databuf[2];
	//int res = 0;
	memset(&sensor_data, 0, sizeof(sensor_data));
	if((err = apm_16d_check_and_clear_intr(obj->client)))
	{
		APM_ERR("check intrs: %d\n", err);
	}

	//APM_LOG(" apm_16d_eint_work,=====pending_intr =%lx\n",obj->pending_intr);
	/*
	if((1<<APM_BIT_ALS) & obj->pending_intr)
	{
		//get raw data
		APM_LOG(" als change\n");
		if((err = apm_16d_read_als(obj->client, &obj->als)))
		{
			APM_ERR("apm_16d read als data: %d\n", err);
		}
		//map and store data to hwm_sensor_data

		sensor_data.values[0] = apm_16d_get_als_value(obj, obj->als);
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
		APM_LOG("als raw %x -> value %d \n", obj->als,sensor_data.values[0]);
		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_LIGHT, &sensor_data)))
		{
			APM_ERR("111 call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}
	*/
	//if((1<<APM_BIT_PS) &  obj->pending_intr)
	{
		//get raw data
		APM_LOG(" ps change\n");
		if((err = apm_16d_read_ps(obj->client, &obj->ps)))
		{
			APM_ERR("apm_16d read ps data: %d\n", err);
		}
		//map and store data to hwm_sensor_data
		sensor_data.values[0] = apm_16d_get_ps_value(obj, obj->ps);
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
		/*singal interrupt function add*/
		/*
		#if defined(AGOLD_PROX_CALI_ENABLE)
		
		if(intr_flag_value){
			databuf[0] = PS_THDH;
			databuf[1] = 0xFF;
			res = i2c_master_send(obj->client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			
			databuf[0] = PS_THDL;
			databuf[1] = (u8)(Apm_16d_Current_Ps_Thd_Value & 0xFF);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
		}
		else{	
			databuf[0] = PS_THDH;
			databuf[1] = (u8)(Apm_16d_Current_Ps_Thd_Value & 0xFF);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			databuf[0] = PS_THDL;
			databuf[1] = 0 & 0xff;
			res = i2c_master_send(obj->client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
		}
		
		#else
		if(intr_flag_value){
			databuf[0] = PS_THDH;
			databuf[1] = 0xFF;
			res = i2c_master_send(obj->client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			databuf[0] = PS_THDL;
			databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low)) & 0xFF);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
		}
		else{	
			databuf[0] = PS_THDH;
			databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_high))0xFF);
			res = i2c_master_send(obj->client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			databuf[0] = PS_THDL;
			databuf[1] = 0&0xFF;
			res = i2c_master_send(obj->client, databuf, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
		}
		#endif
		*/
		
		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
			APM_ERR("2222 call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}

	apm_16d_check_and_clear_intr(obj->client);
	mt_eint_unmask(CUST_EINT_ALS_NUM); 
	return;	
	
#if 0
EXIT_ERR:
	apm_16d_check_and_clear_intr(obj->client);
	mt_eint_unmask(CUST_EINT_ALS_NUM); 
	APM_ERR("i2c_transfer error = %d\n", res);
	return;	
#endif	

}
/*----------------------------------------------------------------------------*/
int apm_16d_setup_eint(struct i2c_client *client)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);

	g_apm_16d_ptr = obj;
	/*configure to GPIO function, external interrupt*/
	
	//printk ("%s: %d, %d, %d\n", __func__, GPIO_ALS_EINT_PIN, CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);

	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
//	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, apm_16d_eint_func, 0);

	mt_eint_unmask(CUST_EINT_ALS_NUM);  


    return 0;

}
/*----------------------------------------------------------------------------*/
static int apm_16d_init_client(struct i2c_client *client)
{
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int err;
//	u8 databuf[2]; 

	if((err = apm_16d_write_als(client, atomic_read(&obj->als_cmd_val))))
	{
		APM_ERR("write als: %d\n", err);
		return err;
	}

	if((err = apm_16d_write_ps(client, atomic_read(&obj->ps_cmd_val))))
	{
		APM_ERR("write ps: %d\n", err);
		return err;
	}

	if((err = apm_16d_write_ps_thd(client, atomic_read(&obj->ps_thd_val_high))))
	{
		APM_ERR("write thd: %d\n", err);
		return err;
	}

	if((err = apm_16d_setup_eint(client)))
	{
		APM_ERR("setup eint: %d\n", err);
		return err;
	}
	if((err = apm_16d_check_and_clear_intr(client)))
	{
		APM_ERR("check/clear intr: %d\n", err);
		//    return err;
	}
	/*
	if(0 == obj->hw->polling_mode_ps)
	{
		if(1 == ps_cali.valid)
		{
			databuf[0] = PS_THDL;	
			databuf[1] = (u8)(ps_cali.far_away & 0xFF);
			err = i2c_master_send(client, databuf, 0x2);
			if(err <= 0)
			{
				goto EXIT_ERR;
			}

			databuf[0] = PS_THDH;	
			databuf[1] = (u8)(ps_cali.close & 0xFF);
			err = i2c_master_send(client, databuf, 0x2);
			if(err <= 0)
			{
				goto EXIT_ERR;
			}
		}
		else
		{
			databuf[0] = PS_THDL;	
			databuf[1] = (u8)(0x60 & 0xFF);
			err = i2c_master_send(client, databuf, 0x2);
			if(err <= 0)
			{
				goto EXIT_ERR;
			}
			databuf[0] = PS_THDH;	
			databuf[1] = (u8)(0x80 & 0xFF);
			err = i2c_master_send(client, databuf, 0x2);
			if(err <= 0)
			{
				goto EXIT_ERR;
			}
		}
	
		databuf[0] = PS_CMD;
		databuf[1] = 0x01 & 0xFF;
		
		err = i2c_master_send(client, databuf, 0x2);
		if(err <= 0)
		{
			goto EXIT_ERR;
		}

	}
	*/
	return 0;

#if 0
EXIT_ERR:
	APM_ERR("init dev: %d\n", err);
	return err;	
#endif
#if 0
	u8 buf;//debug code
	apm_16d_master_recv(client, obj->addr.ps_thdh,(char*)&buf, sizeof(buf));
	printk("debug========== apm_16d_init_client   ps_thdh=0x%x\n",buf);

	apm_16d_master_recv(client, obj->addr.ps_thdl,(char*)&buf, sizeof(buf));
	printk("debug========== apm_16d_init_client   ps_thdl=0x%x\n",buf);

	apm_16d_master_recv(client, obj->addr.ps_cmd,(char*)&buf, sizeof(buf));
	printk("debug========== apm_16d_init_client   ps_cmd=0x%x\n",buf);
#endif
}//end of apm_16d_init_client()
/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
static ssize_t apm_16d_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "(%d %d %d %d %d %d)\n",
	atomic_read(&apm_16d_obj->i2c_retry), atomic_read(&apm_16d_obj->als_debounce),
	atomic_read(&apm_16d_obj->ps_mask), atomic_read(&apm_16d_obj->ps_thd_val_high),
	atomic_read(&apm_16d_obj->ps_thd_val_low), atomic_read(&apm_16d_obj->ps_debounce));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_store_config(struct device_driver *ddri, const char *buf, size_t count)
{
	int retry, als_deb, ps_deb, mask, thresh, thresl;
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	if(5 == sscanf(buf, "%d %d %d %d %d %d", &retry, &als_deb, &mask, &thresh, &thresl,&ps_deb))
	{
		atomic_set(&apm_16d_obj->i2c_retry, retry);
		atomic_set(&apm_16d_obj->als_debounce, als_deb);
		atomic_set(&apm_16d_obj->ps_mask, mask);
		atomic_set(&apm_16d_obj->ps_thd_val_high, thresh);
		atomic_set(&apm_16d_obj->ps_thd_val_low, thresl);
		atomic_set(&apm_16d_obj->ps_debounce, ps_deb);
	}
	else
	{
		APM_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&apm_16d_obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_store_trace(struct device_driver *ddri, const char *buf, size_t count)
{
    int trace;
    if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&apm_16d_obj->trace, trace);
	}
	else
	{
		APM_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	return count;
}
/*----------------------------------------------------------------------------*/

static int tmp_als = 0;
static ssize_t apm_16d_show_als(struct device_driver *ddri, char *buf)
{
		int res;
#if defined(CONFIG_MTK_AAL_SUPPORT)
		int dat;
#endif
		
		printk("\n[%s]---line = %d----\n",__FUNCTION__, __LINE__);


	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}
	if((res = apm_16d_read_als(apm_16d_obj->client, &apm_16d_obj->als)))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}
	else
	{
		#if defined(CONFIG_MTK_AAL_SUPPORT)
		u16 a[2];
		int i;
		for(i=0;i<2;i++)
		{
			apm_16d_read_als(apm_16d_obj->client, &apm_16d_obj->als);
			a[i]=apm_16d_obj->als;
		}
		(a[1]>a[0])?(apm_16d_obj->als = a[0]):(apm_16d_obj->als = a[1]);

		int idx=0;
		for(idx=0; idx<apm_16d_obj->als_level_num; idx++)
		{
			if(apm_16d_obj->als < apm_16d_obj->hw->als_level[idx])
				break;
		}
		dat = apm_16d_obj->hw->als_value[idx];
		return snprintf(buf, PAGE_SIZE, "%d\n", dat);
		#else
		return snprintf(buf, PAGE_SIZE, "%d\n", tmp_als);
		#endif
	}
}
/*----------------------------------------------------------------------------*/
u8 APM_16D_gCaliState = 0;
static ssize_t apm_16d_show_ps(struct device_driver *ddri, char *buf)
{
	int res;
	int dat=0;
	int raw_ps = 0;
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	if((res = apm_16d_read_ps(apm_16d_obj->client, &raw_ps )))
	{
		printk("apm_16d_show_ps@status:0x%x\n", buf[0]);
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}                                                                                                  
	else
	{
	#if defined(AGOLD_PROX_CALI_ENABLE)
	    if( raw_ps > Apm_16d_Current_Ps_Thd_Value)
	#else
	    if( raw_ps > Apm_16d_Current_Ps_Thd_Value)
	#endif
			dat = 0x80;
        else
            dat = 0x00;
		return snprintf(buf, PAGE_SIZE, "%d\n", dat);
	}
}
#if 0
static ssize_t apm_16d_show_psthd(struct device_driver *ddri, char *buf)
{
	//apm_16d_obj->ps, atomic_read(&apm_16d_obj->ps_thd_val_high), atomic_read(&apm_16d_obj->ps_thd_val_low));

	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", apm_16d_obj->ps);
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_show_reg(struct device_driver *ddri, char *buf)
{
	
    ssize_t len = 0;
  //  int raw_ps = 0;
	
    if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}
  
    /*read*/
	apm_16d_check_and_clear_intr(apm_16d_obj->client);
	apm_16d_read_ps(apm_16d_obj->client, &apm_16d_obj->ps);
	apm_16d_read_als(apm_16d_obj->client, &apm_16d_obj->als);
	/*write*/
	apm_16d_write_als(apm_16d_obj->client, atomic_read(&apm_16d_obj->als_cmd_val));
	apm_16d_write_ps(apm_16d_obj->client, atomic_read(&apm_16d_obj->ps_cmd_val));
	apm_16d_write_ps_thd(apm_16d_obj->client, atomic_read(&apm_16d_obj->ps_thd_val_high));
#if defined(AGOLD_PROX_CALI_ENABLE)	
	len += snprintf(buf+len,PAGE_SIZE-len,"xxd Thd=%04X,state=%d,raw=%04X\n",Apm_16d_Current_Ps_Thd_Value, APM_16D_gCaliState, apm_16d_obj->ps);
#endif
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_show_send(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_store_send(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr, cmd;
	u8 dat;

	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}
	else if(2 != sscanf(buf, "%x %x", &addr, &cmd))
	{
		APM_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8)cmd;
	APM_LOG("send(%02X, %02X) = %d\n", addr, cmd,
	apm_16d_master_send(apm_16d_obj->client, (u16)addr, &dat, sizeof(dat)));

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_show_recv(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_store_recv(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr;
	u8 dat;
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}
	else if(1 != sscanf(buf, "%x", &addr))
	{
		APM_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	APM_LOG("recv(%02X) = %d, 0x%02X\n", addr,apm_16d_master_recv(apm_16d_obj->client, (u16)addr, (char*)&dat, sizeof(dat)), dat);

	//dat=addr;
        //APM_LOG("recv(%02X) = %d, 0x%02X\n", client->addr,apm_16d_master_recv(apm_16d_obj->client, (u16)client->addr, (char*)&dat, sizeof(dat)), dat);
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;

	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	if(apm_16d_obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d) (%d %d) (%d %d %d) (%d %d %d)\n",
			apm_16d_obj->hw->i2c_num, apm_16d_obj->hw->power_id, apm_16d_obj->hw->power_vol, apm_16d_obj->addr.init,
			apm_16d_obj->addr.status,apm_16d_obj->addr.als_cmd, apm_16d_obj->addr.als_dat0, apm_16d_obj->addr.als_dat1,
			apm_16d_obj->addr.ps_cmd, apm_16d_obj->addr.ps_dat, apm_16d_obj->addr.ps_thdl);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "REGS: %d %d %d %d %lu %lu\n",
				atomic_read(&apm_16d_obj->als_cmd_val), atomic_read(&apm_16d_obj->ps_cmd_val), atomic_read(&apm_16d_obj->ps_thd_val_high),
				atomic_read(&apm_16d_obj->ps_thd_val_low), apm_16d_obj->enable, apm_16d_obj->pending_intr);

	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d %d\n", atomic_read(&apm_16d_obj->als_suspend), atomic_read(&apm_16d_obj->ps_suspend));

	return len;
}
/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct apm_16d_priv *obj, const char* buf, size_t count,
                             u32 data[], int len)
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
static ssize_t apm_16d_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	for(idx = 0; idx < apm_16d_obj->als_level_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", apm_16d_obj->hw->als_level[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_store_alslv(struct device_driver *ddri, const char *buf, size_t count)
{
//	struct apm_16d_priv *obj;
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(apm_16d_obj->als_level, apm_16d_obj->hw->als_level, sizeof(apm_16d_obj->als_level));
	}
	else if(apm_16d_obj->als_level_num != read_int_from_buf(apm_16d_obj, buf, count,
			apm_16d_obj->hw->als_level, apm_16d_obj->als_level_num))
	{
		APM_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}

#if 0
static ssize_t apm_16d_store_psthd(struct device_driver *ddri, const char *buf, size_t count)
{
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(apm_16d_obj->als_value, apm_16d_obj->hw->als_value, sizeof(apm_16d_obj->als_value));
	}
	else if(apm_16d_obj->als_value_num != read_int_from_buf(apm_16d_obj, buf, count,
			apm_16d_obj->hw->als_value, apm_16d_obj->als_value_num))
	{
		APM_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}

	for(idx = 0; idx < apm_16d_obj->als_value_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", apm_16d_obj->hw->als_value[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t apm_16d_store_alsval(struct device_driver *ddri, const char *buf, size_t count)
{
	if(!apm_16d_obj)
	{
		APM_ERR("apm_16d_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(apm_16d_obj->als_value, apm_16d_obj->hw->als_value, sizeof(apm_16d_obj->als_value));
	}
	else if(apm_16d_obj->als_value_num != read_int_from_buf(apm_16d_obj, buf, count,
			apm_16d_obj->hw->als_value, apm_16d_obj->als_value_num))
	{
		APM_ERR("invalid format: '%s'\n", buf);
	}
	return count;
}

static ssize_t apm_16d_show_chipinfo(struct device_driver *ddri, char *buf)
{
        ssize_t len = 0;

	len += snprintf(buf+len,PAGE_SIZE-len,"apm_16d\n");

        return len;

}

#if 1
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(als,     S_IWUSR | S_IRUGO, apm_16d_show_als,   NULL);
static DRIVER_ATTR(ps,      S_IWUSR | S_IRUGO, apm_16d_show_ps,    NULL);
//static DRIVER_ATTR(psthd,      S_IWUSR | S_IRUGO, apm_16d_show_psthd,apm_16d_store_psthd);
static DRIVER_ATTR(config,  S_IWUSR | S_IRUGO, apm_16d_show_config,apm_16d_store_config);
static DRIVER_ATTR(alslv,   S_IWUSR | S_IRUGO, apm_16d_show_alslv, apm_16d_store_alslv);
static DRIVER_ATTR(alsval,  S_IWUSR | S_IRUGO, apm_16d_show_alsval,apm_16d_store_alsval);
static DRIVER_ATTR(trace,   S_IWUSR | S_IRUGO, apm_16d_show_trace, apm_16d_store_trace);
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, apm_16d_show_status,  NULL);
static DRIVER_ATTR(send,    S_IWUSR | S_IRUGO, apm_16d_show_send,  apm_16d_store_send);
static DRIVER_ATTR(recv,    S_IWUSR | S_IRUGO, apm_16d_show_recv,  apm_16d_store_recv);
static DRIVER_ATTR(reg,     S_IWUSR | S_IRUGO, apm_16d_show_reg,   NULL);
static DRIVER_ATTR(chipinfo,    S_IWUSR | S_IRUGO, apm_16d_show_chipinfo,  NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *apm_16d_attr_list[] = {
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
    &driver_attr_chipinfo,
};
#endif
/*----------------------------------------------------------------------------*/
static int apm_16d_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(apm_16d_attr_list)/sizeof(apm_16d_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, apm_16d_attr_list[idx])))
		{
			APM_ERR("driver_create_file (%s) = %d\n", apm_16d_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int apm_16d_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(apm_16d_attr_list)/sizeof(apm_16d_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, apm_16d_attr_list[idx]);
	}

return err;
}


#if defined(AGOLD_PROX_CALI_ENABLE)
int apm_16d_set_ps_threshold(struct i2c_client *client,int ps_thd_value)
{
	u8 databuf[2];
	int res = 0;
	databuf[0] = PS_THDH;
	databuf[1] = (u8)(ps_thd_value & 0xFF);
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		return res;
	}
	databuf[0] = PS_THDL;
	databuf[1] = (u8)(ps_thd_value & 0xFF);
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		return res;
	}
	
	return 0;
}
#define APM_16D_READ_TIMES  30

static int apm_16d_get_ps_data_for_cali(struct i2c_client *client)
{
	struct APM_16D_PS_CALI_DATA_STRUCT ps_cali_temp;
	int ps_tmp_dat = 0;
	int max_ps_value = 0;
	u8 i = 0 ;
	int res = 0;
	u8 databuf[2]={0};
	if((res = apm_16d_enable_ps(client, 1)))
	{
		APM_ERR("enable als fail: %ld\n", res); 
		return res;
	}
	for(i=0; i<APM_16D_READ_TIMES; i++){

			if(res = apm_16d_read_ps(client, &ps_tmp_dat))
			{
				APM_LOG("[Agold spl] Read ps data err.\n");
				return res;
			}
			APM_LOG("[Agold spl] ps_tmp_dat = 0x%x.\n",ps_tmp_dat);
			if(max_ps_value < ps_tmp_dat)
			{
				max_ps_value = 0xff&ps_tmp_dat;
			}
			msleep(1);
		}
	
	return max_ps_value;
}

static int pcount_raw_temp[30] = {0};
static int pcount1 = 0;
static int pcount2 = 0;
enum{
    DEEP_CALI_SET_NEAR = 1,
    DEEP_CALI_SET_FAR,
	DEEP_CALI_STRUCTE_NOT_GOOD,
    DEEP_CALI_NEED_GET_PS_RAW_DATA,
	DEEP_CALI_GET_PS_RAW_DATA_ERROR,
};
#endif

/******************************************************************************
 * Function Configuration
******************************************************************************/
static int apm_16d_get_als_value(struct apm_16d_priv *obj, u16 als)
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
		APM_ERR("exceed range\n");
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
		if (atomic_read(&obj->trace) & APM_TRC_CVT_ALS)
		{
			APM_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
		}
		return obj->hw->als_value[idx];
	}
	else
	{
		if(atomic_read(&obj->trace) & APM_TRC_CVT_ALS)
		{
			APM_DBG("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);
		}
		return -1;
	}
}
/*----------------------------------------------------------------------------*/
static int apm_16d_get_ps_value(struct apm_16d_priv *obj, int ps)
{
	int val;//, mask = atomic_read(&obj->ps_mask)
	int invalid = 0;
	int val_temp = 1;
	APM_LOG("[Agold spl][apm_16d] ps_value = %d, Apm_16d_Current_Ps_Thd_Value = %d\n",ps,Apm_16d_Current_Ps_Thd_Value);
	#if defined(AGOLD_PROX_CALI_ENABLE)
	if(ps >= Apm_16d_Current_Ps_Thd_Value)
	#else
	if((ps >= 70))
	#endif
	{
		val = 0;  /*close*/
		val_temp = 0;
		intr_flag_value = 1;
	}
	#if defined(AGOLD_PROX_CALI_ENABLE)
	else if((ps < Apm_16d_Current_Ps_Thd_Value))
	#else
	else if((ps < 70))
	#endif
	{
		val = 1;  /*far away*/
		val_temp = 1;
		intr_flag_value = 0;
	}
	else
		val = val_temp;

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
	else if (obj->als > 10240)
	{
		//invalid = 1;
		APM_DBG("ligh too high will result to failt proximiy\n");
		return 1;  /*far away*/
	}

	if(!invalid)
	{
		//APM_DBG("PS:  %05d => %05d\n", ps, val);
		return val;
	}
	else
	{
		return -1;
	}
}
/******************************************************************************
 * Function Configuration
******************************************************************************/
static int apm_16d_open(struct inode *inode, struct file *file)
{
	file->private_data = apm_16d_i2c_client;

	if (!file->private_data)
	{
		APM_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int apm_16d_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int apm_16d_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
      // unsigned long arg)
static long apm_16d_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	long err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;
	int ps_result = 0;
	#if defined(AGOLD_PROX_CALI_ENABLE)
	void __user *Pcali = NULL;
	struct APM_16D_PS_CALI_DATA_STRUCT ps_cali_temp = {0};
	int ps_tmp_dat = 0;
	int  ps_dat[1] = {0};
	int max_ps_value = 0;
	int i = 0;
	int barrier_flag = 0;
	int cali_mode = 0;
	int no_barrier_flag = 0;
	int max_pcount = 0;
	int near_num = 0;
	int far_num = 0;
	int pcount2_max_num = 0;
	#endif
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
				if((err = apm_16d_enable_ps(obj->client, 1)))
				{
					APM_ERR("enable ps fail: %ld\n", err);
					goto err_out;
				}

				set_bit(APM_BIT_PS, &obj->enable);
			}
			else
			{
				printk("[Agold spl] ioctl disable ps.\n");
				if((err = apm_16d_enable_ps(obj->client, 0)))
				{
					APM_ERR("disable ps fail: %ld\n", err);
					goto err_out;
				}

				clear_bit(APM_BIT_PS, &obj->enable);
			}
			break;

		case ALSPS_GET_PS_MODE:
			enable = test_bit(APM_BIT_PS, &obj->enable) ? (1) : (0);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_PS_DATA:
			if((err = apm_16d_read_ps(obj->client, &obj->ps)))
			{
				goto err_out;
			}

			dat = apm_16d_get_ps_value(obj, obj->ps);
			
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_PS_RAW_DATA:
			#if defined(AGOLD_PROX_CALI_ENABLE)
			if(err = apm_16d_read_ps(obj->client, &dat))
			{
				goto err_out;
			}
			if(dat == 0xff)
			{
				dat = 0x3ff;
			}
			ps_dat[0] = dat;
			printk("[Agold spl] ps_dat[0] = %d .\n",ps_dat[0]);
			if(copy_to_user(ptr, ps_dat, sizeof(ps_dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			#else
			if((err = apm_16d_read_ps(obj->client, &obj->ps)))
			{
				goto err_out;
			}

			dat = obj->ps;
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			#endif
			break;

		case ALSPS_SET_ALS_MODE:
			if(copy_from_user(&enable, ptr, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			if(enable)
			{
				if((err = apm_16d_enable_als(obj->client, 1)))
				{
					APM_ERR("enable als fail: %ld\n", err);
					goto err_out;
				}
				set_bit(APM_BIT_ALS, &obj->enable);
			}
			else
			{
				if((err = apm_16d_enable_als(obj->client, 0)))
				{
					APM_ERR("disable als fail: %ld\n", err);
					goto err_out;
				}
				clear_bit(APM_BIT_ALS, &obj->enable);
			}
			break;

		case ALSPS_GET_ALS_MODE:
			enable = test_bit(APM_BIT_ALS, &obj->enable) ? (1) : (0);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_ALS_DATA:
			if((err = apm_16d_read_als(obj->client, &obj->als)))
			{
				goto err_out;
			}

			dat = apm_16d_get_als_value(obj, obj->als);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_ALS_RAW_DATA:
			if((err = apm_16d_read_als(obj->client, &obj->als)))
			{
				goto err_out;
			}

			dat = apm_16d_get_als_value(obj, obj->als);//obj->als;
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;
/*----------------------------------for factory mode test---------------------------------------*/
		case ALSPS_GET_PS_TEST_RESULT:
			if((err = apm_16d_read_ps(obj->client, &obj->ps)))
			{
				goto err_out;
			}
			if(obj->ps > atomic_read(&obj->ps_thd_val_high))
				{
					ps_result = 1;
				}
			else	ps_result = 0;
				
			if(copy_to_user(ptr, &ps_result, sizeof(ps_result)))
			{
				err = -EFAULT;
				goto err_out;
			}			   
			break;
/*-----------------------------------------------------------------------------------------------*/
		#if defined(AGOLD_PROX_CALI_ENABLE)	
		case ALSPS_SET_PS_CALI:
			Pcali = (void __user*)arg;
			if(Pcali == NULL)
			{
				spin_lock(&ps_cali_lock);
				APM_16D_gCaliState = 1;
				spin_unlock(&ps_cali_lock);
				err = -EINVAL;
				break;	  
			}
			if(copy_from_user(&ps_cali_temp,Pcali, sizeof(ps_cali_temp)))
			{
				spin_lock(&ps_cali_lock);
				APM_16D_gCaliState = 2;
				spin_unlock(&ps_cali_lock);
				err = -EFAULT;
				break;	  
			}
			printk("[Agold spl] %d, ps_cali_temp.close = 0x%x ,ps_cali_temp.ppcount = 0x%x\n ",__LINE__,ps_cali_temp.close,ps_cali_temp.ppcount);

			if(ps_cali_temp.close >0 && ps_cali_temp.close < 0xff)
			{
				spin_lock(&ps_cali_lock);	
				Apm_16d_Current_Ps_Thd_Value = ps_cali_temp.close;
				spin_unlock(&ps_cali_lock);			
			}
			else
			{
				spin_lock(&ps_cali_lock);
				#if defined(AGOLD_DEFINED_APM_16D_PS_THD_VALUE)
				Apm_16d_Current_Ps_Thd_Value = AGOLD_DEFINED_APM_16D_PS_THD_VALUE;
				ps_cali_temp.close = AGOLD_DEFINED_APM_16D_PS_THD_VALUE;
				ps_cali_temp.far_away = AGOLD_DEFINED_APM_16D_PS_THD_VALUE;
				#else
				Apm_16d_Current_Ps_Thd_Value = 0x60;
				ps_cali_temp.close = 0x60;
				ps_cali_temp.far_away = 0x60;
				#endif
				spin_unlock(&ps_cali_lock);	
			}
			printk("[Agold spl] %d, ps_cali_temp.close = 0x%x ,ps_cali_temp.ppcount = 0x%x\n ",__LINE__,ps_cali_temp.close,ps_cali_temp.ppcount);
			printk("[Agold spl] ps_cali_temp.valid = %d,Apm_16d_Current_Ps_Thd_Value = 0x%x \n ",ps_cali_temp.valid, Apm_16d_Current_Ps_Thd_Value);
			
			if(ps_cali_temp.valid)
				apm_16d_set_ps_threshold(obj->client,(unsigned int)(Apm_16d_Current_Ps_Thd_Value));
			else
				apm_16d_set_ps_threshold(obj->client,(unsigned int)(AGOLD_DEFINED_APM_16D_PS_THD_VALUE));
			
			spin_lock(&ps_cali_lock);			
			APM_16D_gCaliState = 3;
			spin_unlock(&ps_cali_lock);
			
			break;
		
		case ALSPS_DEEP_CALI_GET_PS_RAW_DATA:
			ps_cali.no_barrier_flag = 0;
			pcount1 = apm_16d_get_ps_data_for_cali(obj->client);	
			printk("pcount1 = %d,ps_cali.no_barrier_flag = %d\n",pcount1,ps_cali.no_barrier_flag);
			ps_cali.no_barrier_flag = 1;
			if(copy_to_user(ptr, &ps_cali, sizeof(ps_cali)))
			{
				return -EFAULT;
			}  
			return 0;

		case ALSPS_GET_PS_RAW_DATA_FOR_CALI:
			if(1){
				printk("[Agold spl]ps_cali.no_barrier_flag = %d\n",ps_cali.no_barrier_flag);		
				pcount2 = apm_16d_get_ps_data_for_cali(obj->client);

				if(ps_cali.no_barrier_flag)
				{	
					if(pcount1 > 0xF0)//stucture is not good 
					{
						ps_cali_temp.deep_cali_flag = DEEP_CALI_STRUCTE_NOT_GOOD;
						ps_cali_temp.close = 0;
						ps_cali_temp.far_away = 0;
						ps_cali_temp.valid = 0;
						ps_cali_temp.ppcount = 0;
						ps_cali.no_barrier_flag = 0;
						printk("[Agold spl] Warning the structure not good,ps_cali_temp.deep_cali_flag = %d\n",ps_cali_temp.deep_cali_flag);
						if(copy_to_user(ptr, &ps_cali_temp, sizeof(ps_cali_temp)))
						{
							return -EFAULT;
						}  
						return 0;		
					}
					else if((pcount1 - pcount2 >= 0x02)||pcount1 > 0x60) //get ps raw error,maybe you have place obstacle when getting ps raw data
					{
						ps_cali_temp.deep_cali_flag = DEEP_CALI_GET_PS_RAW_DATA_ERROR;
						ps_cali_temp.close = 0;
						ps_cali_temp.far_away = 0;
						ps_cali_temp.valid = 0;
						ps_cali_temp.ppcount = 0;
						ps_cali.no_barrier_flag = 0; 
						printk("[Agold spl] get ps raw error,maybe you have place obstacle when getting ps raw data,ps_cali_temp.deep_cali_flag = %d\n",ps_cali_temp.deep_cali_flag);
						if(copy_to_user(ptr, &ps_cali_temp, sizeof(ps_cali_temp)))
						{
							return -EFAULT;
						}  
						return 0;	
					}
					else if(pcount2 -pcount1 < 0x10)//too far or no barrier,need set near
					{	
						ps_cali_temp.deep_cali_flag = DEEP_CALI_SET_NEAR;
						ps_cali_temp.close = 0;
						ps_cali_temp.far_away = 0;
						ps_cali_temp.valid = 0;
						ps_cali_temp.ppcount = 0;
						ps_cali.no_barrier_flag = 0;
						printk("[Agold spl] too far,need set near,ps_cali_temp.deep_cali_flag = %d\n",ps_cali_temp.deep_cali_flag);
						if(copy_to_user(ptr, &ps_cali_temp, sizeof(ps_cali_temp)))
						{
							return -EFAULT;
						}  
						return 0;	
					}
					else
					{	
						ps_cali_temp.deep_cali_flag = 0;
						max_ps_value = pcount2;
						ps_cali_temp.ppcount = 0;
					}			
				}
				else
				{
					ps_cali_temp.deep_cali_flag = DEEP_CALI_NEED_GET_PS_RAW_DATA;
					ps_cali_temp.close = 0;
					ps_cali_temp.far_away = 0;
					ps_cali_temp.valid = 0;
					ps_cali_temp.ppcount = 0;
					ps_cali.no_barrier_flag = 0;
					printk("[Agold spl] Need Get ps raw data,ps_cali_temp.deep_cali_flag = %d\n",ps_cali_temp.deep_cali_flag);
					if(copy_to_user(ptr, &ps_cali_temp, sizeof(ps_cali_temp)))
					{
						return -EFAULT;
					}  
					return 0;	
				}
				
			}
			else
			{	
				for(i = 0;i < PS_CALI_TIMES; i++){
					if(err = apm_16d_read_ps(obj->client, &ps_tmp_dat))
					{
						spin_lock(&ps_cali_lock);
						APM_16D_gCaliState = 4;
						spin_unlock(&ps_cali_lock);
						err = -EFAULT;
						break;
					}
					APM_LOG("[Agold spl] ps_tmp_dat = 0x%x.\n",ps_tmp_dat);
					if(ps_tmp_dat < 0 || ps_tmp_dat >= 0xff)
					{
						msleep(10);
						ps_tmp_dat = 0 ;
						#if defined(AGOLD_DEFINED_APM_16D_PS_THD_VALUE)
						ps_tmp_dat = AGOLD_DEFINED_APM_16D_PS_THD_VALUE;
						#else
						ps_tmp_dat = 0x60;
						#endif
					}
			
					if(max_ps_value < ps_tmp_dat)
					{
						max_ps_value = 0xff&ps_tmp_dat;
					}
					msleep(1);
				}
			}
			
			spin_lock(&ps_cali_lock);
			if(max_ps_value >= 0xF0)
				max_ps_value = 0xF0;
			ps_cali_temp.close = 0xff & max_ps_value;//ps_tmp_dat;
			ps_cali_temp.far_away = 0xff & max_ps_value;//ps_tmp_dat;
			ps_cali_temp.valid = 1;
			spin_unlock(&ps_cali_lock);
			printk("[spl] ps_cali_temp.close = 0x%x,ps_cali_temp.far_away = 0x%x,ps_cali_temp.valid = 0x%x \n ",ps_cali_temp.close,ps_cali_temp.far_away,ps_cali_temp.valid);
			apm_16d_set_ps_threshold(obj->client,(unsigned int)(ps_cali_temp.close));
	
			spin_lock(&ps_cali_lock);
			Apm_16d_Current_Ps_Thd_Value = ps_cali_temp.close;
			spin_unlock(&ps_cali_lock);			

			if(copy_to_user(ptr, &ps_cali_temp, sizeof(ps_cali_temp)))
			{
				spin_lock(&ps_cali_lock);
				APM_16D_gCaliState = 5;
				spin_unlock(&ps_cali_lock);
				err = -EFAULT;
				goto err_out;
			}  
			spin_lock(&ps_cali_lock);
			APM_16D_gCaliState = 6;            
			spin_unlock(&ps_cali_lock);			
			break;
		#endif

		default:
			APM_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	err_out:
	return err;
}
/*----------------------------------------------------------------------------*/
static struct file_operations apm_16d_fops = {
	//.owner = THIS_MODULE,
	.open = apm_16d_open,
	.release = apm_16d_release,
	.unlocked_ioctl = apm_16d_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice apm_16d_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &apm_16d_fops,
};

static int apm_16d_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	/*
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int err;
	APM_FUN();
	
printk("[%s] line  =%d called \n",__FUNCTION__, __LINE__);
	if(msg.event == PM_EVENT_SUSPEND)
	{
		if(!obj)
		{
			APM_ERR("null pointer!!\n");
			return -EINVAL;
		}

		atomic_set(&obj->als_suspend, 1);
		if((err = apm_16d_enable_als(client, 0)))
		{
			APM_ERR("disable als: %d\n", err);
			return err;
		}

		atomic_set(&obj->ps_suspend, 1);
		if((err = apm_16d_enable_ps(client, 0)))
		{
			APM_ERR("disable ps:  %d\n", err);
			return err;
		}

		apm_16d_power(obj->hw, 0);
	}
	*/
	return 0;
}
/*----------------------------------------------------------------------------*/
static int apm_16d_i2c_resume(struct i2c_client *client)
{
	/*
	struct apm_16d_priv *obj = i2c_get_clientdata(client);
	int err;
	APM_FUN();
	printk("[%s] line  =%d called \n",__FUNCTION__, __LINE__);
	if(!obj)
	{
		APM_ERR("null pointer!!\n");
		return -EINVAL;
	}

	apm_16d_power(obj->hw, 1);
	if((err = apm_16d_init_client(client)))
	{
		APM_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->als_suspend, 0);
	if(test_bit(APM_BIT_ALS, &obj->enable))
	{
		if((err = apm_16d_enable_als(client, 1)))
		{
			APM_ERR("enable als fail: %d\n", err);
		}
	}
	atomic_set(&obj->ps_suspend, 0);
	if(test_bit(APM_BIT_PS,  &obj->enable))
	{
		if((err = apm_16d_enable_ps(client, 1)))
		{
			APM_ERR("enable ps fail: %d\n", err);
		}
	}
	*/
	return 0;
}
/*----------------------------------------------------------------------------*/
static void apm_16d_early_suspend(struct early_suspend *h)
{
/*early_suspend is only applied for ALS*/
#if 1
	struct apm_16d_priv *obj = container_of(h, struct apm_16d_priv, early_drv);
	int err;
	APM_FUN();

	if(!obj)
	{
		APM_ERR("null pointer!!\n");
		return;
	}

	atomic_set(&obj->als_suspend, 1);
	if((err = apm_16d_enable_als(obj->client, 0)))
	{
		APM_ERR("disable als fail: %d\n", err);
	}
#endif
printk("[%s] line  =%d called \n",__FUNCTION__, __LINE__);
}
/*----------------------------------------------------------------------------*/
static void apm_16d_late_resume(struct early_suspend *h)
{
#if 1
/*early_suspend is only applied for ALS*/
	struct apm_16d_priv *obj = container_of(h, struct apm_16d_priv, early_drv);
	int err;
	hwm_sensor_data sensor_data;

	memset(&sensor_data, 0, sizeof(sensor_data));
	APM_FUN();

	if(!obj)
	{
		APM_ERR("null pointer!!\n");
		return;
	}

	atomic_set(&obj->als_suspend, 0);
	if(test_bit(APM_BIT_ALS, &obj->enable))
	{
		if((err = apm_16d_enable_als(obj->client, 1)))
		{
			APM_ERR("enable als fail: %d\n", err);

		}
	}
#endif
printk("[%s] line  =%d called \n",__FUNCTION__, __LINE__);
}

int apm_16d_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct apm_16d_priv *obj = (struct apm_16d_priv *)self;

	//APM_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APM_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APM_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value)
				{
					if((err = apm_16d_enable_ps(obj->client, 1)))
					{
						APM_ERR("enable ps fail: %d\n", err);
						return -1;
					}
					set_bit(APM_BIT_PS, &obj->enable);
				}
				else
				{
					if((err = apm_16d_enable_ps(obj->client, 0)))
					{
						APM_ERR("disable ps fail: %d\n", err);
						return -1;
					}
					clear_bit(APM_BIT_PS, &obj->enable);
				}
			}
			break;

		case SENSOR_GET_DATA:

			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APM_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;

				if((err = apm_16d_read_ps(obj->client, &obj->ps)))
				{
					err = -1;
				}
				else
				{
					sensor_data->values[0] = apm_16d_get_ps_value(obj, obj->ps);
					sensor_data->value_divide = 1;
					sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				}
			}
			break;
		default:
			APM_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}

int apm_16d_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct apm_16d_priv *obj = (struct apm_16d_priv *)self;

	//APM_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APM_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APM_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value)
				{
					if((err = apm_16d_enable_als(obj->client, 1)))
					{
						APM_ERR("enable als fail: %d\n", err);
						return -1;
					}
					set_bit(APM_BIT_ALS, &obj->enable);
				}
				else
				{
					if((err = apm_16d_enable_als(obj->client, 0)))
					{
						APM_ERR("disable als fail: %d\n", err);
						return -1;
					}
					clear_bit(APM_BIT_ALS, &obj->enable);
				}

			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APM_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;

				if((err = apm_16d_read_als(obj->client, &obj->als)))
				{
					err = -1;
				}
				else
				{
					#if defined(CONFIG_MTK_AAL_SUPPORT)
					sensor_data->values[0] = obj->als;
					#else
					if(0==obj->als){
						sensor_data->values[0] = tmp_als;
					}else{
						u16 a[2];
						int i;
						for(i=0;i<2;i++)
						{
							apm_16d_read_als(obj->client, &obj->als);
							a[i]=obj->als;
						}
						(a[1]>a[0])?(obj->als = a[0]):(obj->als = a[1]);
						sensor_data->values[0] = apm_16d_get_als_value(obj, obj->als);
						tmp_als = sensor_data->values[0];						
					}
					#endif
					sensor_data->value_divide = 1;
					sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				}
			}
			break;
		default:
			APM_ERR("light sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}


/*----------------------------------------------------------------------------*/
static int apm_16d_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, APM_16D_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/

static int apm_16d_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct apm_16d_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}

	printk("\n\n[%s] amp_16d chip addr = 0x%x\n\n",__FUNCTION__, client->addr);
	memset(obj, 0, sizeof(*obj));
	apm_16d_obj = obj;
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	obj->hw = apm_16d_get_cust_alsps_hw();	
	#else
	obj->hw = get_cust_alsps_hw();
	#endif
	apm_16d_get_addr(obj->hw, &obj->addr);

	//INIT_DELAYED_WORK(&obj->eint_work, apm_16d_eint_work);
	INIT_WORK(&obj->eint_work, apm_16d_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);
	atomic_set(&obj->als_debounce, 300);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 100);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->trace, 0x00);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0x0D);//range:12780 refresh 800ms //old 0x01
	atomic_set(&obj->ps_cmd_val, 0x01);  //old : 0x65

	atomic_set(&obj->ps_thd_val_high,  obj->hw->ps_threshold_high);
	atomic_set(&obj->ps_thd_val_low,  obj->hw->ps_threshold_low);

    	atomic_set(&obj->ps_thd_val,  obj->hw->ps_threshold);
	if(obj->hw->polling_mode_ps == 0)
	{
		atomic_add(0x02, &obj->ps_cmd_val);//enable ps interrupt
		APM_LOG("enable ps interrupt\n");
	}
	if(obj->hw->polling_mode_als == 0)
	{
		atomic_add(0x02, &obj->als_cmd_val);//enable als interrupt
		APM_LOG("enable als interrupt\n");
	}
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	if(!(atomic_read(&obj->als_cmd_val) & SD_ALS))
	{
		set_bit(APM_BIT_ALS, &obj->enable);
	}
	if(!(atomic_read(&obj->ps_cmd_val) & SD_PS))
	{
		set_bit(APM_BIT_PS, &obj->enable);
	}
	apm_16d_i2c_client = client;

	if((err = apm_16d_init_client(client)))
	{
		goto exit_init_failed;
	}
	if((err = misc_register(&apm_16d_device)))
	{
		APM_ERR("apm_16d_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	if(err = apm_16d_create_attr(&(apm_16d_init_info.platform_diver_addr->driver)))
	#else
	if((err = apm_16d_create_attr(&apm_16d_alsps_driver.driver)))
	#endif
	{
		APM_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	
	obj_ps.self = apm_16d_obj;
	if(1 == obj->hw->polling_mode_ps)
	{
	  obj_ps.polling = 1;
	}
	else
	{
	  obj_ps.polling = 0;//interrupt mode
	}
	obj_ps.sensor_operate = apm_16d_ps_operate;
	if((err = hwmsen_attach(ID_PROXIMITY, &obj_ps)))
	{
		APM_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	obj_als.self = apm_16d_obj;
	if(1 == obj->hw->polling_mode_als)
	{
	  obj_als.polling = 1;
	}
	else
	{
	  obj_als.polling = 0;//interrupt mode
	}
	obj_als.sensor_operate = apm_16d_als_operate;
	if((err = hwmsen_attach(ID_LIGHT, &obj_als)))
	{
		APM_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = apm_16d_early_suspend,
	obj->early_drv.resume   = apm_16d_late_resume,
	register_early_suspend(&obj->early_drv);
#endif
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	apm_16d_init_flag = 0;
	#endif
	APM_LOG("%s: OK\n", __func__);

	return 0;

	exit_create_attr_failed:
	misc_deregister(&apm_16d_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
	//	exit_kfree:
	kfree(obj);
	exit:
	apm_16d_i2c_client = NULL;
	APM_ERR("%s: err = %d\n", __func__, err);
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	apm_16d_init_flag = -1;
	#endif
	return err;
}
/*----------------------------------------------------------------------------*/
static int apm_16d_i2c_remove(struct i2c_client *client)
{
	int err;

	if((err = apm_16d_delete_attr(&apm_16d_i2c_driver.driver)))
	{
		APM_ERR("apm_16d_delete_attr fail: %d\n", err);
	}

	if((err = misc_deregister(&apm_16d_device)))
	{
		APM_ERR("misc_deregister fail: %d\n", err);
	}

	apm_16d_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
static int apm_16d_probe(void)
#else
static int apm_16d_probe(struct platform_device *pdev)
#endif
{
	int test;
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	struct alsps_hw *hw = apm_16d_get_cust_alsps_hw();
	#else	
	struct alsps_hw *hw = get_cust_alsps_hw();
	#endif
	struct apm_16d_i2c_addr addr;

	apm_16d_power(hw, 1);
	apm_16d_get_addr(hw, &addr);
	//apm_16d_force[0] = hw->i2c_num;
	//apm_16d_force[1] = hw->i2c_addr[0];
	if((test = i2c_add_driver(&apm_16d_i2c_driver)))
	{
		APM_ERR("add driver error\n");
		#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
		#else
		return -1;
		#endif
	}
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	if(-1 == apm_16d_init_flag){
		printk("[Agold spl] i2c del driver.\n");
		i2c_del_driver(&apm_16d_i2c_driver);
		return -1;
	}
	#endif

	return 0;
}
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
static int apm_16d_remove(void)
#else
static int apm_16d_remove(struct platform_device *pdev)
#endif
{	
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	struct alsps_hw *hw = apm_16d_get_cust_alsps_hw();
	#else
	struct alsps_hw *hw = get_cust_alsps_hw();
	#endif
	APM_FUN();
	apm_16d_power(hw, 0);
	i2c_del_driver(&apm_16d_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
#else
static struct platform_driver apm_16d_alsps_driver = {
	.probe      = apm_16d_probe,
	.remove     = apm_16d_remove,
	.driver     = {
		.name  = "als_ps",
		.owner = THIS_MODULE,
	}
};
#endif
/*----------------------------------------------------------------------------*/
static int __init apm_16d_init(void)
{
	
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	struct alsps_hw *hw = apm_16d_get_cust_alsps_hw();
	#else
	struct alsps_hw *hw = get_cust_alsps_hw();
	#endif
	APM_FUN();
	APM_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_apm_16d, 1);
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	if(hwmsen_alsps_add(&apm_16d_init_info))
	#else
	if(platform_driver_register(&apm_16d_alsps_driver))
	#endif
	{
		APM_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit apm_16d_exit(void)
{
	APM_FUN();
	#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
	#else
	platform_driver_unregister(&apm_16d_alsps_driver);
	#endif
}
/*----------------------------------------------------------------------------*/
module_init(apm_16d_init);
module_exit(apm_16d_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Everlight");
MODULE_DESCRIPTION("Everlight ALS/PS driver");
MODULE_LICENSE("GPL");

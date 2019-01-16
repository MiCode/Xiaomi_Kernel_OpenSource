/* mmc328x.c - mmc328x compass driver
 * 
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
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/time.h>
#include <linux/hrtimer.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>


#include <cust_mag.h>
#include "mmc328x.h"
#include <linux/hwmsen_helper.h>
/*----------------------------------------------------------------------------*/
#define DEBUG 1
#define MMC328x_DEV_NAME         "mmc328x"
#define DRIVER_VERSION          "1.0.0"
/*----------------------------------------------------------------------------*/
#define MMC328x_DEBUG		1
#define MMC328x_DEBUG_MSG	1
#define MMC328x_DEBUG_FUNC	1
#define MMC328x_DEBUG_DATA	1
#define MAX_FAILURE_COUNT	3
#define MMC328x_RETRY_COUNT	10
#define MMC328x_DEFAULT_DELAY	100
#define	MMC328x_BUFSIZE  0x20
#define MMC328X_DELAY_RM	10	/* ms */


#if MMC328x_DEBUG_MSG
#define MMCDBG(format, ...)	printk(KERN_INFO "mmc328x " format "\n", ## __VA_ARGS__)
#else
#define MMCDBG(format, ...)
#endif

#if MMC328x_DEBUG_FUNC
#define MMCFUNC(func) printk(KERN_INFO "mmc328x " func " is called\n")
#else
#define MMCFUNC(func)
#endif

static struct i2c_client *this_client = NULL;


// calibration msensor and orientation data
static int sensor_data[CALIBRATION_DATA_SIZE];
static struct mutex sensor_data_mutex;
static struct mutex read_i2c_xyz;
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static int mmcd_delay = MMC328x_DEFAULT_DELAY;

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mmc328x_i2c_id[] = {{MMC328x_DEV_NAME,0},{}};
/*the adapter id will be available in customization*/
static unsigned short mmc328x_force[] = {0x00, MMC328x_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
static const unsigned short *const mmc328x_forces[] = { mmc328x_force, NULL };
static struct i2c_client_address_data mmc328x_addr_data = { .forces = mmc328x_forces,};
/*----------------------------------------------------------------------------*/
static int mmc328x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int mmc328x_i2c_remove(struct i2c_client *client);
static int mmc328x_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
//static int mmc_probe(struct platform_device *pdev);
//static int mmc_remove(struct platform_device *pdev);
static int  mmc328x_local_init(void);
static int  mmc328x_remove(void);

static int mmc328x_init_flag =0; // 0<==>OK -1 <==> fail



/*----------------------------------------------------------------------------*/
typedef enum {
    MMC_FUN_DEBUG  = 0x01,
	MMC_DATA_DEBUG = 0X02,
	MMC_HWM_DEBUG  = 0X04,
	MMC_CTR_DEBUG  = 0X08,
	MMC_I2C_DEBUG  = 0x10,
} MMC_TRC;

#define MMC328x_DELAY_TM	10	/* ms */
#define MMC328x_DELAY_SET	10	/* ms */
#define MMC328x_DELAY_RST	10	/* ms */
#define MMC328x_DELAY_STDN	1	/* ms */

#define MMC328x_RESET_INTV	10

static u32 read_idx = 0;


static struct sensor_init_info mmc328x_init_info = {
		.name = "mmc328x",
		.init = mmc328x_local_init,
		.uninit = mmc328x_remove,
	
};


/*----------------------------------------------------------------------------*/
struct mmc328x_i2c_data {
    struct i2c_client *client;
    struct mag_hw *hw; 
    atomic_t layout;   
    atomic_t trace;
	struct hwmsen_convert   cvt;
#if defined(CONFIG_HAS_EARLYSUSPEND)    
    struct early_suspend    early_drv;
#endif 
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver mmc328x_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE, 
        .name  = MMC328x_DEV_NAME,
    },
	.probe      = mmc328x_i2c_probe,
	.remove     = mmc328x_i2c_remove,
	.detect     = mmc328x_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend    = mmc328x_suspend,
	.resume     = mmc328x_resume,
#endif 
	.id_table = mmc328x_i2c_id,
	.address_data = &mmc328x_addr_data,
};

/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;

static int mmc328x_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = MMC328x_REG_CTRL;
	struct mmc328x_i2c_data *obj = i2c_get_clientdata(client);

	if(hwmsen_read_byte(client, addr, databuf))
	{
		printk("mmc328x: read power ctl register err and retry!\n");
		if(hwmsen_read_byte(client, addr, databuf))
	    {
		   printk("mmc328x: read power ctl register retry err!\n");
		   return -1;
	    }
	}

	databuf[0] &= ~MMC328x_CTRL_TM;
	
	if(enable == TRUE)
	{
		databuf[0] |= MMC328x_CTRL_TM;
	}
	else
	{
		// do nothing
	}
	databuf[1] = databuf[0];
	databuf[0] = MMC328x_REG_CTRL;
	

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		printk("mmc328x: set power mode failed!\n");
		return -1;
	}
	else
	{
		printk("mmc328x: set power mode ok %x!\n", databuf[1]);
	}
	
	return 0;    
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static void mmc328x_power(struct mag_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != MT65XX_POWER_NONE)
	{        
		MMCDBG("power %s\n", on ? "on" : "off");
		if(power_on == on)
		{
			MMCDBG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "mmc328x")) 
			{
				printk(KERN_ERR "power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "mmc328x")) 
			{
				printk(KERN_ERR "power off fail!!\n");
			}
		}
	}
	power_on = on;
}
static int I2C_RxData(char *rxData, int length)
{
	uint8_t loop_i;

#if DEBUG
	int i;
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *data = i2c_get_clientdata(client);
	char addr = rxData[0];
#endif


	/* Caller should check parameter validity.*/
	if((rxData == NULL) || (length < 1))
	{
		return -EINVAL;
	}

	for(loop_i = 0; loop_i < MMC328x_RETRY_COUNT; loop_i++)
	{
		this_client->addr = this_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG;
		if(i2c_master_send(this_client, (const char*)rxData, ((length<<0X08) | 0X01)))
		{
			break;
		}
		printk("I2C_RxData delay!\n");
		mdelay(10);
	}
	
	if(loop_i >= MMC328x_RETRY_COUNT)
	{
		printk(KERN_ERR "%s retry over %d\n", __func__, MMC328x_RETRY_COUNT);
		return -EIO;
	}
#if DEBUG
	if(atomic_read(&data->trace) & MMC_I2C_DEBUG)
	{
		printk(KERN_INFO "RxData: len=%02x, addr=%02x\n  data=", length, addr);
		for(i = 0; i < length; i++)
		{
			printk(KERN_INFO " %02x", rxData[i]);
		}
	    printk(KERN_INFO "\n");
	}
#endif
	return 0;
}

static int I2C_TxData(char *txData, int length)
{
	uint8_t loop_i;
	
#if DEBUG
	int i;
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *data = i2c_get_clientdata(client);
#endif

	/* Caller should check parameter validity.*/
	if ((txData == NULL) || (length < 2))
	{
		return -EINVAL;
	}

	this_client->addr = this_client->addr & I2C_MASK_FLAG;
	for(loop_i = 0; loop_i < MMC328x_RETRY_COUNT; loop_i++)
	{
		if(i2c_master_send(this_client, (const char*)txData, length) > 0)
		{
			break;
		}
		printk("I2C_TxData delay!\n");
		mdelay(10);
	}
	
	if(loop_i >= MMC328x_RETRY_COUNT)
	{
		printk(KERN_ERR "%s retry over %d\n", __func__, MMC328x_RETRY_COUNT);
		return -EIO;
	}
#if DEBUG
	if(atomic_read(&data->trace) & MMC_I2C_DEBUG)
	{
		printk(KERN_INFO "TxData: len=%02x, addr=%02x\n  data=", length, txData[0]);
		for(i = 0; i < (length-1); i++)
		{
			printk(KERN_INFO " %02x", txData[i + 1]);
		}
		printk(KERN_INFO "\n");
	}
#endif
	return 0;
}


// Daemon application save the data
static int ECS_SaveData(int buf[12])
{
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *data = i2c_get_clientdata(client);
#endif

	mutex_lock(&sensor_data_mutex);
	memcpy(sensor_data, buf, sizeof(sensor_data));	
	mutex_unlock(&sensor_data_mutex);
	
#if DEBUG
	if(atomic_read(&data->trace) & MMC_HWM_DEBUG)
	{
		MMCDBG("Get daemon data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			sensor_data[0],sensor_data[1],sensor_data[2],sensor_data[3],
			sensor_data[4],sensor_data[5],sensor_data[6],sensor_data[7],
			sensor_data[8],sensor_data[9],sensor_data[10],sensor_data[11]);
	}	
#endif

	return 0;

}
static int ECS_ReadXYZData(int *vec, int size)
{
	unsigned char data[6] = {0,0,0,0,0,0};
	ktime_t expires;
	int wait_n=0;
	int MD_times = 0;
	static int last_data[3];
	struct timespec time1, time2, time3,time4,delay,aa;
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *clientdata = i2c_get_clientdata(client);
#endif
//set_current_state(TASK_INTERRUPTIBLE);
time1 = current_kernel_time();
    
	if(size < 3)
	{
		return -1;
	}
	mutex_lock(&read_i2c_xyz);
#if 0	
	/* do RESET/SET every MMC328x_RESET_INTV times read */
	if (!(read_idx % MMC328x_RESET_INTV))
	{
		/* SET */
		data[0] = MMC328x_REG_CTRL;
		data[1] = MMC328x_CTRL_SET;
		/* not check return value here, assume it always OK */
		I2C_TxData(data, 2);
		//delay.tv_sec =0;
		//delay.tv_nsec = MMC328x_DELAY_STDN *1000;
		//hrtimer_nanosleep(&delay, &aa, HRTIMER_MODE_REL, CLOCK_MONOTONIC);
		msleep(MMC328x_DELAY_STDN);
	}
	
	
	/* wait TM done for coming data read */
//	time2 = current_kernel_time();

	time3 = current_kernel_time();
	/* read xyz raw data */
	read_idx++;
	data[0] = MMC328x_REG_DATA;
	if(I2C_RxData(data, 6) < 0)
	{
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	vec[0] = data[0] << 8 | data[1];
	vec[1] = data[2] << 8 | data[3];
	vec[2] = data[4] << 8 | data[5];
	for(wait_n=0; wait_n<10; wait_n++)
{
	if((vec[0]!=0) && (vec[1]!=0)&&(vec[2]!=0))
		break;
		data[0] = MMC328x_REG_DATA;
	if(I2C_RxData(data, 6) < 0)
	{
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	vec[0] = data[0] << 8 | data[1];
	vec[1] = data[2] << 8 | data[3];
	vec[2] = data[4] << 8 | data[5];
	}	
	time4 = current_kernel_time();
//	printk("start time %d - %d, sleep %d - %d , stop %d !\r\n", time1.tv_sec, time1.tv_nsec, time2.tv_nsec, time3.tv_nsec, time4.tv_nsec);
#if DEBUG
	if(atomic_read(&clientdata->trace) & MMC_DATA_DEBUG)
	{
		printk("[X - %04x] [Y - %04x] [Z - %04x]\r\n", vec[0], vec[1], vec[2]);
		if((vec[0]-last_data[0] > 100) ||(last_data[0]-vec[0] > 100) ||
			(vec[1]-last_data[1] > 100) ||(last_data[1]-vec[1] > 100) ||
			(vec[2]-last_data[2] > 100) ||(last_data[2]-vec[2] > 100))
			{
			msleep(3000);
		printk("data error!\r\n");
	}}	
#endif
	/* send TM cmd before read */
	data[0] = MMC328x_REG_CTRL;
	data[1] = MMC328x_CTRL_TM;
	/* not check return value here, assume it always OK */
	I2C_TxData(data, 2);
#endif
 time2 = current_kernel_time();

if (!(read_idx % MMC328x_RESET_INTV))
	{
		/* RM */
			data[0] = MMC328x_REG_CTRL;
			data[1] = MMC328x_CTRL_RM;
			/* not check return value here, assume it always OK */
			I2C_TxData(data, 2);
			/* wait external capacitor charging done for next RM */
			msleep(MMC328X_DELAY_RM);	  
	}
	time3 = current_kernel_time();
	/* send TM cmd before read */
	data[0] = MMC328x_REG_CTRL;
	data[1] = MMC328x_CTRL_TM;
	/* not check return value here, assume it always OK */
	I2C_TxData(data, 2);	
	msleep(MMC328x_DELAY_TM);
	
	/* Read MD */
		data[0] = MMC328x_REG_DS;
		I2C_RxData(data, 1);
		while (!(data[0] & 0x01)) {
			msleep(1);
			/* Read MD again*/
			data[0] = MMC328x_REG_DS;
			I2C_RxData(data, 1);
			if (data[0] & 0x01) break;
			MD_times++;
			if (MD_times > 3) {	
				printk("TM not work!!");
				mutex_unlock(&read_i2c_xyz);
				return -EFAULT;
			}
		}
	read_idx++;
	data[0] = MMC328x_REG_DATA;
	if(I2C_RxData(data, 6) < 0)
	{
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	vec[0] = data[1] << 8 | data[0];
	vec[1] = data[3] << 8 | data[2];
	vec[2] = data[5] << 8 | data[4];
#if DEBUG
	if(atomic_read(&clientdata->trace) & MMC_DATA_DEBUG)
	{
		printk("[X - %04x] [Y - %04x] [Z - %04x]\n", vec[0], vec[1], vec[2]);
	}	
#endif			
	mutex_unlock(&read_i2c_xyz);
	last_data[0] = vec[0];
	last_data[1] = vec[1];
	last_data[2] = vec[2];
	return 0;
}

static int ECS_GetRawData(int data[3])
{
	int err = 0;
	err = ECS_ReadXYZData(data, 3);
	if(err !=0 )
	{
		printk(KERN_ERR "MMC328x_IOC_TM failed\n");
		return -1;
	}

	// sensitivity 512 count = 1 Guass = 100uT
	data[0] = (data[0] - MMC328X_OFFSET_X) * 100 / MMC328X_SENSITIVITY_X;
	data[1] = (data[1] - MMC328X_OFFSET_X) * 100 / MMC328X_SENSITIVITY_X;
	data[2] = (data[2] - MMC328X_OFFSET_X) * 100 / MMC328X_SENSITIVITY_X;

	return err;
}
static int ECS_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}


/*----------------------------------------------------------------------------*/
static int mmc328x_ReadChipInfo(char *buf, int bufsize)
{
	if((!buf)||(bufsize <= MMC328x_BUFSIZE -1))
	{
		return -1;
	}
	if(!this_client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "mmc328x Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MMC328x_BUFSIZE];
	mmc328x_ReadChipInfo(strbuf, MMC328x_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{

	int sensordata[3];
	char strbuf[MMC328x_BUFSIZE];
	
	ECS_GetRawData(sensordata);
	
	
	sprintf(strbuf, "%d %d %d\n", sensordata[0],sensordata[1],sensordata[2]);

	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	int tmp[3];
	char strbuf[MMC328x_BUFSIZE];
	tmp[0] = sensor_data[0] * CONVERT_O / CONVERT_O_DIV;				
	tmp[1] = sensor_data[1] * CONVERT_O / CONVERT_O_DIV;
	tmp[2] = sensor_data[2] * CONVERT_O / CONVERT_O_DIV;
	sprintf(strbuf, "%d, %d, %d\n", tmp[0],tmp[1], tmp[2]);
		
	return sprintf(buf, "%s\n", strbuf);;           
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;

	if(1 == sscanf(buf, "%d", &layout))
	{
		atomic_set(&data->layout, layout);
		if(!hwmsen_get_convert(layout, &data->cvt))
		{
			printk(KERN_ERR "HWMSEN_GET_CONVERT function error!\r\n");
		}
		else if(!hwmsen_get_convert(data->hw->direction, &data->cvt))
		{
			printk(KERN_ERR "invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		}
		else
		{
			printk(KERN_ERR "invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	}
	else
	{
		printk(KERN_ERR "invalid format = '%s'\n", buf);
	}
	
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *data = i2c_get_clientdata(client);
	ssize_t len = 0;

	if(data->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n", 
			data->hw->i2c_num, data->hw->direction, data->hw->power_id, data->hw->power_vol);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	
	len += snprintf(buf+len, PAGE_SIZE-len, "OPEN: %d\n", atomic_read(&dev_open_count));
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mmc328x_i2c_data *obj = i2c_get_clientdata(this_client);
	if(NULL == obj)
	{
		printk(KERN_ERR "mmc328x_i2c_data is null!!\n");
		return 0;
	}	
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct mmc328x_i2c_data *obj = i2c_get_clientdata(this_client);
	int trace;
	if(NULL == obj)
	{
		printk(KERN_ERR "mmc328x_i2c_data is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}
	else 
	{
		printk(KERN_ERR "invalid content: '%s', length = %d\n", buf, count);
	}
	
	return count;    
}

static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[MMC328x_BUFSIZE];
	sprintf(strbuf, "memsicd");
	return sprintf(buf, "%s", strbuf);		
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(daemon,      S_IRUGO, show_daemon_name, NULL);
static DRIVER_ATTR(chipinfo,    S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata,  S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(posturedata, S_IRUGO, show_posturedata_value, NULL);
static DRIVER_ATTR(layout,      S_IRUGO | S_IWUSR, show_layout_value, store_layout_value);
static DRIVER_ATTR(status,      S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(trace,       S_IRUGO | S_IWUSR, show_trace_value, store_trace_value);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *mmc328x_attr_list[] = {
    &driver_attr_daemon,
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_posturedata,
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_trace,
};
/*----------------------------------------------------------------------------*/
static int mmc328x_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(mmc328x_attr_list)/sizeof(mmc328x_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(err = driver_create_file(driver, mmc328x_attr_list[idx]))
		{            
			printk(KERN_ERR "driver_create_file (%s) = %d\n", mmc328x_attr_list[idx]->attr.name, err);
			break;
		}
	}
	
	return err;
}
/*----------------------------------------------------------------------------*/
static int mmc328x_delete_attr(struct device_driver *driver)
{
	int idx;
	int num = (int)(sizeof(mmc328x_attr_list)/sizeof(mmc328x_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, mmc328x_attr_list[idx]);
	}
	

	return 0;
}


/*----------------------------------------------------------------------------*/
static int mmc328x_open(struct inode *inode, struct file *file)
{    
	struct mmc328x_i2c_data *obj = i2c_get_clientdata(this_client);    
	int ret = -1;	
	
	if(atomic_read(&obj->trace) & MMC_CTR_DEBUG)
	{
		MMCDBG("Open device node:mmc328x\n");
	}
	ret = nonseekable_open(inode, file);
	
	return ret;
}
/*----------------------------------------------------------------------------*/
static int mmc328x_release(struct inode *inode, struct file *file)
{
	struct mmc328x_i2c_data *obj = i2c_get_clientdata(this_client);
	atomic_dec(&dev_open_count);
	if(atomic_read(&obj->trace) & MMC_CTR_DEBUG)
	{
		MMCDBG("Release device node:mmc328x\n");
	}	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mmc328x_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)
{
	void __user *argp = (void __user *)arg;
		
	/* NOTE: In this function the size of "char" should be 1-byte. */
	char buff[MMC328x_BUFSIZE];				/* for chip information */

	int value[12];			/* for SET_YPR */
	int delay;				/* for GET_DELAY */
	int status; 				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
	unsigned char data[16] = {0};
	int vec[3] = {0};	
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *clientdata = i2c_get_clientdata(client);
	hwm_sensor_data* osensor_data;
	uint32_t enable;


	switch (cmd)
	{
		case MMC31XX_IOC_TM:
			data[0] = MMC328x_REG_CTRL;
			data[1] = MMC328x_CTRL_TM;
			if (I2C_TxData(data, 2) < 0)
			{
				printk(KERN_ERR "MMC328x_IOC_TM failed\n");
				return -EFAULT;
			}
			/* wait TM done for coming data read */
			msleep(MMC328x_DELAY_TM);
			break;
			
		case MMC31XX_IOC_SET:
			data[0] = MMC328x_REG_CTRL;
			data[1] = MMC328x_REG_DS;
			if(I2C_TxData(data, 2) < 0)
			{
				printk(KERN_ERR "MMC328x_IOC_SET failed\n");
				return -EFAULT;
			}
			/* wait external capacitor charging done for next SET/RESET */
			msleep(MMC328x_DELAY_SET);
			break;
			
		case MMC31XX_IOC_RESET:
			data[0] = MMC328x_REG_CTRL;
			data[1] = MMC328x_REG_DS;
			if(I2C_TxData(data, 2) < 0)
			{
				printk(KERN_ERR "MMC328x_IOC_RESET failed\n");
				return -EFAULT;
			}
			/* wait external capacitor charging done for next SET/RESET */
			msleep(MMC328x_DELAY_RST);
			break;
			
		case MMC31XX_IOC_READ:
			data[0] = MMC328x_REG_DATA;
			if(I2C_RxData(data, 6) < 0)
			{
				printk(KERN_ERR "MMC328x_IOC_READ failed\n");
				return -EFAULT;
			}
			vec[0] = data[0] << 8 | data[1];
			vec[1] = data[2] << 8 | data[3];
			vec[2] = data[4] << 8 | data[5];
#if DEBUG
			if(atomic_read(&clientdata->trace) & MMC_DATA_DEBUG)
			{
				printk("[X - %04x] [Y - %04x] [Z - %04x]\n", vec[0], vec[1], vec[2]);
			}
#endif
			if(copy_to_user(argp, vec, sizeof(vec)))
			{
				printk(KERN_ERR "MMC328x_IOC_READ: copy to user failed\n");
				return -EFAULT;
			}
			break;
		
		case MMC31XX_IOC_READXYZ:
			ECS_ReadXYZData(vec, 3);
			if(copy_to_user(argp, vec, sizeof(vec)))
			{
				printk(KERN_ERR "MMC328x_IOC_READXYZ: copy to user failed\n");
				return -EFAULT;
			}
			break;			
			
		case ECOMPASS_IOC_GET_DELAY:			
			delay = mmcd_delay;
			if(copy_to_user(argp, &delay, sizeof(delay)))
			{
				printk(KERN_ERR "copy_to_user failed.");
				return -EFAULT;
			}
			break;		
			
		case ECOMPASS_IOC_SET_YPR:			
			if(argp == NULL)
			{
				MMCDBG("invalid argument.");
				return -EINVAL;
			}
			if(copy_from_user(value, argp, sizeof(value)))
			{
				MMCDBG("copy_from_user failed.");
				return -EFAULT;
			}
			ECS_SaveData(value);
			break;

		case ECOMPASS_IOC_GET_OPEN_STATUS:
			status = ECS_GetOpenStatus();			
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MMCDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
			
		case ECOMPASS_IOC_GET_MFLAG:
			sensor_status = atomic_read(&m_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				MMCDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
			
		case ECOMPASS_IOC_GET_OFLAG:
			sensor_status = atomic_read(&o_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				MMCDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;			
		                

		case MSENSOR_IOCTL_READ_CHIPINFO:
			if(argp == NULL)
			{
				printk(KERN_ERR "IO parameter pointer is NULL!\r\n");
				break;
			}
			
			mmc328x_ReadChipInfo(buff, MMC328x_BUFSIZE);
			if(copy_to_user(argp, buff, strlen(buff)+1))
			{
				return -EFAULT;
			}                
			break;

		case MSENSOR_IOCTL_READ_SENSORDATA:	
			if(argp == NULL)
			{
				printk(KERN_ERR "IO parameter pointer is NULL!\r\n");
				break;    
			}
			ECS_GetRawData(vec);			
			sprintf(buff, "%x %x %x", vec[0], vec[1], vec[2]);
			if(copy_to_user(argp, buff, strlen(buff)+1))
			{
				return -EFAULT;
			}                
			break;

		case ECOMPASS_IOC_GET_LAYOUT:
			status = atomic_read(&clientdata->layout);
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MMCDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;

		case MSENSOR_IOCTL_SENSOR_ENABLE:
			
			if(argp == NULL)
			{
				printk(KERN_ERR "IO parameter pointer is NULL!\r\n");
				break;
			}
			if(copy_from_user(&enable, argp, sizeof(enable)))
			{
				MMCDBG("copy_from_user failed.");
				return -EFAULT;
			}
			else
			{
			    printk( "MSENSOR_IOCTL_SENSOR_ENABLE enable=%d!\r\n",enable);
				if(1 == enable)
				{
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}			
				}
				wake_up(&open_wq);
				
			}
			
			break;
			
		case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:			
			if(argp == NULL)
			{
				printk(KERN_ERR "IO parameter pointer is NULL!\r\n");
				break;    
			}
			
			//AKECS_GetRawData(buff, AKM8975_BUFSIZE);
			osensor_data = (hwm_sensor_data *)buff;
		    mutex_lock(&sensor_data_mutex);
				
			osensor_data->values[0] = sensor_data[8] * CONVERT_O;
			osensor_data->values[1] = sensor_data[9] * CONVERT_O;
			osensor_data->values[2] = sensor_data[10] * CONVERT_O;
			osensor_data->status = sensor_data[11];
			osensor_data->value_divide = CONVERT_O_DIV;
					
			mutex_unlock(&sensor_data_mutex);

            sprintf(buff, "%x %x %x %x %x", osensor_data->values[0], osensor_data->values[1],
				osensor_data->values[2],osensor_data->status,osensor_data->value_divide);
			if(copy_to_user(argp, buff, strlen(buff)+1))
			{
				return -EFAULT;
			} 
			
			break;
			
		default:
			printk(KERN_ERR "%s not supported = 0x%04x", __FUNCTION__, cmd);
			return -ENOIOCTLCMD;
			break;		
		}

	return 0;    
}
/*----------------------------------------------------------------------------*/
static struct file_operations mmc328x_fops = {
	.owner = THIS_MODULE,
	.open = mmc328x_open,
	.release = mmc328x_release,
	.ioctl = mmc328x_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mmc328x_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &mmc328x_fops,
};
/*----------------------------------------------------------------------------*/
int mmc328x_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* msensor_data;
	
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *data = i2c_get_clientdata(client);
#endif
	
#if DEBUG
	if(atomic_read(&data->trace) & MMC_FUN_DEBUG)
	{
		MMCFUNC("mmc328x_operate");
	}	
#endif
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk(KERN_ERR "Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 20)
				{
					mmcd_delay = 20;
				}
				mmcd_delay = value;
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk(KERN_ERR "Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;

				if(value == 1)
				{
					atomic_set(&m_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					atomic_set(&m_flag, 0);
					if(atomic_read(&o_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}
				}
				wake_up(&open_wq);
				
				// TODO: turn device into standby or normal mode
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				printk(KERN_ERR "get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				msensor_data = (hwm_sensor_data *)buff_out;
				mutex_lock(&sensor_data_mutex);
				
				msensor_data->values[0] = sensor_data[4] * CONVERT_M;
				msensor_data->values[1] = sensor_data[5] * CONVERT_M;
				msensor_data->values[2] = sensor_data[6] * CONVERT_M;
				msensor_data->status = sensor_data[7];
				msensor_data->value_divide = CONVERT_M_DIV;
					
				mutex_unlock(&sensor_data_mutex);
#if DEBUG
				if(atomic_read(&data->trace) & MMC_HWM_DEBUG)
				{
					MMCDBG("Hwm get m-sensor data: %d, %d, %d. divide %d, status %d!\n",
						msensor_data->values[0],msensor_data->values[1],msensor_data->values[2],
						msensor_data->value_divide,msensor_data->status);
				}	
#endif
			}
			break;
		default:
			printk(KERN_ERR "msensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/
int mmc328x_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* osensor_data;	
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct mmc328x_i2c_data *data = i2c_get_clientdata(client);
#endif
	
#if DEBUG
	if(atomic_read(&data->trace) & MMC_FUN_DEBUG)
	{
		MMCFUNC("mmc328x_orientation_operate");
	}	
#endif

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk(KERN_ERR "Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 20)
				{
					mmcd_delay = 20;
				}
				mmcd_delay = value;
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				printk(KERN_ERR "Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				
				value = *(int *)buff_in;

				if(value == 1)
				{
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}									
				}	
				wake_up(&open_wq);
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				printk(KERN_ERR "get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				osensor_data = (hwm_sensor_data *)buff_out;
				mutex_lock(&sensor_data_mutex);
				
				osensor_data->values[0] = sensor_data[8] * CONVERT_O;
				osensor_data->values[1] = sensor_data[9] * CONVERT_O;
				osensor_data->values[2] = sensor_data[10] * CONVERT_O;
				osensor_data->status = sensor_data[11];
				osensor_data->value_divide = CONVERT_O_DIV;
					
				mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if(atomic_read(&data->trace) & MMC_HWM_DEBUG)
			{
				MMCDBG("Hwm get o-sensor data: %d, %d, %d. divide %d, status %d!\n",
					osensor_data->values[0],osensor_data->values[1],osensor_data->values[2],
					osensor_data->value_divide,osensor_data->status);
			}	
#endif
			}
			break;
		default:
			printk(KERN_ERR "gsensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/
#ifndef	CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int mmc328x_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct mmc328x_i2c_data *obj = i2c_get_clientdata(client)
	    

	if(msg.event == PM_EVENT_SUSPEND)
	{
		mmc328x_power(obj->hw, 0);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mmc328x_resume(struct i2c_client *client)
{
	struct mmc328x_i2c_data *obj = i2c_get_clientdata(client)


	mmc328x_power(obj->hw, 1);
	

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void mmc328x_early_suspend(struct early_suspend *h) 
{
	struct mmc328x_i2c_data *obj = container_of(h, struct mmc328x_i2c_data, early_drv);
    printk("mmc328x early_suspend!!\n");
	//mmc328x_power(obj->hw, 0);
   
	if(NULL == obj)
	{
		printk(KERN_ERR "null pointer!!\n");
		return;
	}
	if(mmc328x_SetPowerMode(obj->client, false))
	{
		printk("mmc328x: write power control fail!!\n");
		return;
	}
	       
}
/*----------------------------------------------------------------------------*/
static void mmc328x_late_resume(struct early_suspend *h)
{
	struct mmc328x_i2c_data *obj = container_of(h, struct mmc328x_i2c_data, early_drv);         

	
	if(NULL == obj)
	{
		printk(KERN_ERR "null pointer!!\n");
		return;
	}
	//mmc328x_power(obj->hw, 1);
	
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
static int mmc328x_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, MMC328x_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int mmc328x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct mmc328x_i2c_data *data;
	char tmp[2];
	int err = 0;
	struct hwmsen_object sobj_m, sobj_o;

    MMCDBG("%s: ++++\n", __func__);
	if(!(data = kmalloc(sizeof(struct mmc328x_i2c_data), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct mmc328x_i2c_data));

	data->hw = mmc328x_get_cust_mag_hw();	
	
	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0);
	

	mutex_init(&sensor_data_mutex);
	mutex_init(&read_i2c_xyz);
	
	init_waitqueue_head(&data_ready_wq);
	init_waitqueue_head(&open_wq);

	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);
	
	this_client = new_client;	
	//this_client->timing=400;


	/* send ST cmd to mag sensor first of all */
	tmp[0] = MMC328x_REG_CTRL;
	tmp[1] = MMC328x_CTRL_RM;
	if(I2C_TxData(tmp, 2) < 0)
	{
		printk(KERN_ERR "mmc328x_device set ST cmd failed\n");
		goto exit_kfree;
	}
	

	/* Register sysfs attribute */
	if(err = mmc328x_create_attr(&(mmc328x_init_info.platform_diver_addr->driver)))
	{
		printk(KERN_ERR "create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}

	
	if(err = misc_register(&mmc328x_device))
	{
		printk(KERN_ERR "mmc328x_device register failed\n");
		goto exit_misc_device_register_failed;	}    

	sobj_m.self = data;
    sobj_m.polling = 1;
    sobj_m.sensor_operate = mmc328x_operate;
	if(err = hwmsen_attach(ID_MAGNETIC, &sobj_m))
	{
		printk(KERN_ERR "attach fail = %d\n", err);
		goto exit_kfree;
	}
	
	sobj_o.self = data;
    sobj_o.polling = 1;
    sobj_o.sensor_operate = mmc328x_orientation_operate;
	if(err = hwmsen_attach(ID_ORIENTATION, &sobj_o))
	{
		printk(KERN_ERR "attach fail = %d\n", err);
		goto exit_kfree;
	}
	
#if CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	data->early_drv.suspend  = mmc328x_early_suspend,
	data->early_drv.resume   = mmc328x_late_resume,    
	register_early_suspend(&data->early_drv);
#endif

	MMCDBG("%s: OK\n", __func__);
	mmc328x_init_flag = 0;

	return 0;

	exit_sysfs_create_group_failed:	
	exit_misc_device_register_failed:
	exit_kfree:
	kfree(data);
	exit:
	printk(KERN_ERR "%s: err = %d\n", __func__, err);
	mmc328x_init_flag = -1;
	return err;
}
/*----------------------------------------------------------------------------*/
static int mmc328x_i2c_remove(struct i2c_client *client)
{
	int err;	
	
	if(err = mmc328x_delete_attr(&(mmc328x_init_info.platform_diver_addr->driver)))
	{
		printk(KERN_ERR "mmc328x_delete_attr fail: %d\n", err);
	}
	
	this_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));	
	misc_deregister(&mmc328x_device);    
	return 0;
}
/*----------------------------------------------------------------------------*/
static int	mmc328x_local_init(void)
{
	struct mag_hw *hw = mmc328x_get_cust_mag_hw();
    MMCFUNC("mmc328x_local_init");
	
	mmc328x_power(hw, 1);
	atomic_set(&dev_open_count, 0);
	mmc328x_force[0] = hw->i2c_num;

	if(i2c_add_driver(&mmc328x_i2c_driver))
	{
		printk(KERN_ERR "add driver error\n");
		return -1;
	} 
	if(-1 == mmc328x_init_flag)
	{
	   return -1;
	}
	return 0;
}

/*----------------------------------------------------------------------------*/
static int mmc328x_remove()
{
	struct mag_hw *hw = mmc328x_get_cust_mag_hw();
	mmc328x_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&mmc328x_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/

static int __init mmc328x_init(void)
{	
	MMCFUNC("mmc328x_init");
	hwmsen_msensor_add(&mmc328x_init_info);
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit mmc328x_exit(void)
{	
   MMCFUNC("mmc328x_exit");
}
/*----------------------------------------------------------------------------*/
module_init(mmc328x_init);
module_exit(mmc328x_exit);

MODULE_AUTHOR("weiqi fu");
MODULE_DESCRIPTION("mmc328x compass driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);


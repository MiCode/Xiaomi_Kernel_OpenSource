/* qmc5983.c - qmc5983 compass driver
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

/*
#ifdef MT6573
#include <mach/mt6573_devs.h>
#include <mach/mt6573_typedefs.h>
#include <mach/mt6573_gpio.h>
#include <mach/mt6573_pll.h>
#endif

#ifdef MT6575
#include <mach/mt6575_devs.h>
#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#endif

#ifdef MT6577
#include <mach/mt6577_devs.h>
#include <mach/mt6577_typedefs.h>
#include <mach/mt6577_gpio.h>
#include <mach/mt6577_pm_ldo.h>
#endif
*/

#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>


#include <cust_mag.h>
#include "qmc5983.h"
#include <linux/hwmsen_helper.h>
/*----------------------------------------------------------------------------*/
#define DEBUG 1
#define QMC5983_DEV_NAME         "qmc5983"
#define DRIVER_VERSION          "1.0.0"
/*----------------------------------------------------------------------------*/
#define QMC5983_DEBUG		1
#define QMC5983_DEBUG_MSG	1
#define QMC5983_DEBUG_FUNC	1
#define QMC5983_DEBUG_DATA	1
#define MAX_FAILURE_COUNT	3
#define QMC5983_RETRY_COUNT	10
#define QMC5983_DEFAULT_DELAY	100
#define	QMC5983_BUFSIZE  0x20


#define QMC5983_AXIS_X            0
#define QMC5983_AXIS_Y            1
#define QMC5983_AXIS_Z            2
#define QMC5983_AXES_NUM          3

#define QMC5983_DEFAULT_DELAY 100
#define CALIBRATION_DATA_SIZE   12


#if QMC5983_DEBUG_MSG
#define QMCDBG(format, ...)	printk(KERN_INFO "qmc5983 " format "\n", ## __VA_ARGS__)
#else
#define QMCDBG(format, ...)
#endif

#if QMC5983_DEBUG_FUNC
#define QMCFUNC(func) printk(KERN_INFO "qmc5983 " func " is called\n")
#else
#define QMCFUNC(func)
#endif

#define MSE_TAG					"[Msensor] "
#define MSE_FUN(f)				printk(MSE_TAG"%s\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)		printk(KERN_ERR MSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)		printk(MSE_TAG fmt, ##args)



static struct i2c_client *this_client = NULL;


static short qmcd_delay = QMC5983_DEFAULT_DELAY;


// calibration msensor and orientation data
static int sensor_data[CALIBRATION_DATA_SIZE];
static struct mutex sensor_data_mutex;
static struct mutex read_i2c_xyz;
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id qmc5983_i2c_id[] = {{QMC5983_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_qmc5983={ I2C_BOARD_INFO("qmc5983", (0X1e))};
/*the adapter id will be available in customization*/
//static unsigned short qmc5983_force[] = {0x00, QMC5983_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const qmc5983_forces[] = { qmc5983_force, NULL };
//static struct i2c_client_address_data qmc5983_addr_data = { .forces = qmc5983_forces,};
/*----------------------------------------------------------------------------*/
static int qmc5983_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int qmc5983_i2c_remove(struct i2c_client *client);
//static int qmc5983_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int qmc_probe(struct platform_device *pdev);
static int qmc_remove(struct platform_device *pdev);

/*----------------------------------------------------------------------------*/
typedef enum {
    QMC_FUN_DEBUG  = 0x01,
	QMC_DATA_DEBUG = 0X02,
	QMC_HWM_DEBUG  = 0X04,
	QMC_CTR_DEBUG  = 0X08,
	QMC_I2C_DEBUG  = 0x10,
} QMC_TRC;


/*----------------------------------------------------------------------------*/
struct qmc5983_i2c_data {
    struct i2c_client *client;
    struct mag_hw *hw; 
    atomic_t layout;   
    atomic_t trace;
	struct hwmsen_convert   cvt;
	//add for qmc5983 start    for layout direction and M sensor sensitivity------------------------
#if 0
	struct QMC5983_platform_data *pdata;
#endif
	short xy_sensitivity;
	short z_sensitivity;
	//add for qmc5983 end-------------------------
#if defined(CONFIG_HAS_EARLYSUSPEND)    
    struct early_suspend    early_drv;
#endif 
};

/*----------------------------------------------------------------------------*/
static struct i2c_driver qmc5983_i2c_driver = {
    .driver = {
//      .owner = THIS_MODULE, 
        .name  = QMC5983_DEV_NAME,
    },
	.probe      = qmc5983_i2c_probe,
	.remove     = qmc5983_i2c_remove,
//	.detect     = qmc5983_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend    = qmc5983_suspend,
	.resume     = qmc5983_resume,
#endif 
	.id_table = qmc5983_i2c_id,
//	.address_data = &qmc5983_addr_data,
};

/*----------------------------------------------------------------------------*/
#if 0
static struct platform_driver qmc_sensor_driver = {
	.probe      = qmc_probe,
	.remove     = qmc_remove,    
	.driver     = {
		.name  = "msensor",
//		.owner = THIS_MODULE,
	}
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id qmc_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver qmc_sensor_driver =
{
	.probe      = qmc_probe,
	.remove     = qmc_remove,    
	.driver     = 
	{
		.name = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = qmc_of_match,
		#endif
	}
};

static int I2C_RxData(char *rxData, int length)
{
	uint8_t loop_i;

#if DEBUG
	int i;
	struct i2c_client *client = this_client;  
	struct qmc5983_i2c_data *data = i2c_get_clientdata(client);
	char addr = rxData[0];
#endif


	/* Caller should check parameter validity.*/
	if((rxData == NULL) || (length < 1))
	{
		return -EINVAL;
	}

	for(loop_i = 0; loop_i < QMC5983_RETRY_COUNT; loop_i++)
	{
		this_client->addr = this_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG;
		if(i2c_master_send(this_client, (const char*)rxData, ((length<<0X08) | 0X01)))
		{
			break;
		}
		printk("I2C_RxData delay!\n");
		mdelay(10);
	}
	
	if(loop_i >= QMC5983_RETRY_COUNT)
	{
		printk(KERN_ERR "%s retry over %d\n", __func__, QMC5983_RETRY_COUNT);
		return -EIO;
	}
#if DEBUG
	if(atomic_read(&data->trace) & QMC_I2C_DEBUG)
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
	struct qmc5983_i2c_data *data = i2c_get_clientdata(client);
#endif

	/* Caller should check parameter validity.*/
	if ((txData == NULL) || (length < 2))
	{
		return -EINVAL;
	}

	this_client->addr = this_client->addr & I2C_MASK_FLAG;
	for(loop_i = 0; loop_i < QMC5983_RETRY_COUNT; loop_i++)
	{
		if(i2c_master_send(this_client, (const char*)txData, length) > 0)
		{
			break;
		}
		printk("I2C_TxData delay!\n");
		mdelay(10);
	}
	
	if(loop_i >= QMC5983_RETRY_COUNT)
	{
		printk(KERN_ERR "%s retry over %d\n", __func__, QMC5983_RETRY_COUNT);
		return -EIO;
	}
#if DEBUG
	if(atomic_read(&data->trace) & QMC_I2C_DEBUG)
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



/* X,Y and Z-axis magnetometer data readout
 * param *mag pointer to \ref QMC5983_t structure for x,y,z data readout
 * note data will be read by multi-byte protocol into a 6 byte structure
 */
static int QMC5983_read_mag_xyz(int *data)
{
	int res;
	unsigned char mag_data[6];
	unsigned char databuf[2];
	int hw_d[3] = { 0 };
	int temp = 0;
	int output[3]={ 0 };
	unsigned char rdy = 0;
	struct i2c_client *client = this_client;  
	struct qmc5983_i2c_data *clientdata = i2c_get_clientdata(client);

	/* Check status register for data availability */
	while(!(rdy & 0x01)){
		msleep(6);
		databuf[0]=SR_REG_M;
		res=I2C_RxData(databuf,1);
		rdy=databuf[0];
		MSE_LOG("QMC5983 Status register is %04X",rdy);
	}
	
	if(rdy & 0x02){
		/*data register is locked,new data can't not place into it,write CRA register to make sure it unlocked*/
		/*write CRA*/
		databuf[0] = CRA_REG_M;
		//databuf[1] = 0x70;/*(8-average, 15 Hz default, normal measurement)*/
		databuf[1] = 0x7c;/*(8-average, 220 Hz default, normal measurement)*/
		if(res=I2C_TxData(databuf,2)<0){
			MSE_ERR("write CRA error!\n");
			return res;
		}
	}
	
	//MSE_LOG("QMC5983 read mag_xyz begin\n");
	
	mutex_lock(&read_i2c_xyz);
	
	databuf[0] = OUT_X_M;
	//res = I2C_RxData(mag_data, 6);/*only can read one by one*/
	if(res = I2C_RxData(databuf, 1)){
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	mag_data[0]=databuf[0];
	
	databuf[0] = OUT_X_L;
	if(res = I2C_RxData(databuf, 1)){
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	mag_data[1]=databuf[0];
	
	databuf[0] = OUT_Z_M;
	if(res = I2C_RxData(databuf, 1)){
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	mag_data[2]=databuf[0];
	
	databuf[0] = OUT_Z_L;
	if(res = I2C_RxData(databuf, 1)){
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	mag_data[3]=databuf[0];

	databuf[0] = OUT_Y_M;
	if(res = I2C_RxData(databuf, 1)){
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	mag_data[4]=databuf[0];
	
	databuf[0] = OUT_Y_L;
	if(res = I2C_RxData(databuf, 1)){
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	mag_data[5]=databuf[0];
	
	mutex_unlock(&read_i2c_xyz);
	/*
	MSE_LOG("mag_data[%d, %d, %d, %d, %d, %d]\n",
mag_data[0], mag_data[1], mag_data[2],
mag_data[3], mag_data[4], mag_data[5]);
	*/
	hw_d[0] = (short) (((mag_data[0]) << 8) | mag_data[1]);
	hw_d[1] = (short) (((mag_data[2]) << 8) | mag_data[3]);
	hw_d[2] = (short) (((mag_data[4]) << 8) | mag_data[5]);

	temp = hw_d[1];		/* swap Y and Z */
	hw_d[1] = hw_d[2];
	hw_d[2] = temp;

	hw_d[0] = hw_d[0] * 1000 / clientdata->xy_sensitivity;
	hw_d[1] = hw_d[1] * 1000 / clientdata->xy_sensitivity;
	hw_d[2] = hw_d[2] * 1000 / clientdata->z_sensitivity;

	//MSE_LOG("Hx=%d, Hy=%d, Hz=%d\n",hw_d[0],hw_d[1],hw_d[2]);

#if 0
	data->x = ((mag->pdata->negate_x) ? (-hw_d[mag->pdata->axis_map_x])
		   : (hw_d[mag->pdata->axis_map_x]));
	data->y = ((mag->pdata->negate_y) ? (-hw_d[mag->pdata->axis_map_y])
		   : (hw_d[mag->pdata->axis_map_y]));
	data->z = ((mag->pdata->negate_z) ? (-hw_d[mag->pdata->axis_map_z])
		   : (hw_d[mag->pdata->axis_map_z]));
#endif
	output[clientdata->cvt.map[QMC5983_AXIS_X]] = clientdata->cvt.sign[QMC5983_AXIS_X]*hw_d[QMC5983_AXIS_X];
	output[clientdata->cvt.map[QMC5983_AXIS_Y]] = clientdata->cvt.sign[QMC5983_AXIS_Y]*hw_d[QMC5983_AXIS_Y];
	output[clientdata->cvt.map[QMC5983_AXIS_Z]] = clientdata->cvt.sign[QMC5983_AXIS_Z]*hw_d[QMC5983_AXIS_Z];

	data[0] =output[QMC5983_AXIS_X];
	data[1] =output[QMC5983_AXIS_Y];
	data[2] =output[QMC5983_AXIS_Z];
	return res;
}


/* 5983 Self Test data collection */
int QMC5983_self_test(char mode, short* buf)
{
	int res = 0;
	unsigned char rdy = 0;
	unsigned char cra = 0;
	unsigned char ctemp = 0;
	unsigned char databuf[6]={0,0,0,0,0,0};
	int data[3];
	
	/* Read register A */
	databuf[0]=CRA_REG_M;
	res=I2C_RxData(databuf,1);
	
	if (res>=0)//read ok
	{
		cra=databuf[0];
		/* Remove old mode and insert new mode */
		ctemp = cra;
		ctemp = ctemp >> 2;
		ctemp = ctemp << 2;
		ctemp += mode;
		cra   = ctemp;

		/* Insert new mode into register A */
		databuf[0]=CRA_REG_M;
		databuf[1]=cra;
		res=I2C_TxData(databuf,1);
		
		if (res>=0)//write ok?
		{
			rdy = 0;
			while (rdy < 1)
			{
				/* Check mode register for data availability */
				databuf[0]=SR_REG_M;
				res=I2C_RxData(databuf,1);
				rdy=databuf[0];
				if ((rdy > 0) && ((rdy & 0x01) == 1))
				{
					
					/* Read magnetic data out of the module */
					res = QMC5983_read_mag_xyz(&data);
					if (res>=0)
					{
						buf[0] = data[0];
						buf[1] = data[1];
						buf[2] = data[2];
						return res;
					}
				}
			}
		}
	}
	return res;
}

/* Set the Gain range */
int QMC5983_set_range(short range)
{
	int err = 0;
	unsigned char data[2];
	struct qmc5983_i2c_data *obj = i2c_get_clientdata(this_client);

	switch (range) {
	case QMC5983_0_88G:
		obj->xy_sensitivity = GAIN_0;
		obj->z_sensitivity = GAIN_0;
		break;
	case QMC5983_1_3G:
		obj->xy_sensitivity = GAIN_1;
		obj->z_sensitivity = GAIN_1;
		break;
	case QMC5983_1_9G:
		obj->xy_sensitivity = GAIN_2;
		obj->z_sensitivity = GAIN_2;
		break;
	case QMC5983_2_5G:
		obj->xy_sensitivity = GAIN_3;
		obj->z_sensitivity = GAIN_3;
		break;
	case QMC5983_4_0G:
		obj->xy_sensitivity = GAIN_4;
		obj->z_sensitivity = GAIN_4;
		break;
	case QMC5983_4_7G:
		obj->xy_sensitivity = GAIN_5;
		obj->z_sensitivity = GAIN_5;
		break;
	case QMC5983_5_6G:
		obj->xy_sensitivity = GAIN_6;
		obj->z_sensitivity = GAIN_6;
		break;
	case QMC5983_8_1G:
		obj->xy_sensitivity = GAIN_7;
		obj->z_sensitivity = GAIN_7;
		break;
	default:
		return -EINVAL;
	}

	data[0] = CRB_REG_M;
	data[1] = range;
	err = I2C_TxData(data, 2);
	return err;

}

/* Get the Gain range */
int QMC5983_get_range(char* range)
{
	int err = 0;
	char buf[1];
	buf[0]=CRB_REG_M;
	err = I2C_RxData(buf,1);
	*range = buf;

	return err;
}


static void qmc5983_put_idle(struct i2c_client *client)
{
printk(KERN_ALERT "qmc5983_put_idle\n");
	i2c_smbus_write_byte_data(client, CRA_REG_M,
			SAMPLE_AVERAGE_8 | OUTPUT_RATE_75 | MEASURE_NORMAL);
	i2c_smbus_write_byte_data(client, CRB_REG_M, GAIN_DEFAULT);
	i2c_smbus_write_byte_data(client, MR_REG_M, QMC5983_IDLE_MODE);
}

static void qmc5983_start_measure(struct i2c_client *client)
{
	i2c_smbus_write_byte_data(client, MR_REG_M, QMC5983_CC_MODE);
}

static void qmc5983_stop_measure(struct i2c_client *client)
{
	i2c_smbus_write_byte_data(client, MR_REG_M, QMC5983_IDLE_MODE);
}

static int qmc5983_enable(struct i2c_client *client)
{
	QMCDBG("start measure!\n");
	qmc5983_start_measure(client);

	QMC5983_set_range(QMC5983_4_7G);
	
	return 0;
}

static int qmc5983_disable(struct i2c_client *client)
{
	QMCDBG("stop measure!\n");
	qmc5983_stop_measure(client);

	return 0;
}



/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;

static int qmc5983_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = MR_REG_M;
	struct qmc5983_i2c_data *obj = i2c_get_clientdata(client);

	
	if(enable == TRUE)
	{
		if(qmc5983_enable(client))
		{
			printk("qmc5983: set power mode failed!\n");
			return -1;
		}
		else
		{
			printk("qmc5983: set power mode enable ok!\n");
		}
	}
	else
	{
		if(qmc5983_disable(client))
		{
			printk("qmc5983: set power mode failed!\n");
			return -1;
		}
		else
		{
			printk("qmc5983: set power mode disable ok!\n");
		}
	}
	
	return 0;    
}

/*----------------------------------------------------------------------------*/
static void qmc5983_power(struct mag_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != MT65XX_POWER_NONE)
	{        
		QMCDBG("power %s\n", on ? "on" : "off");
		if(power_on == on)
		{
			QMCDBG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "qmc5983")) 
			{
				printk(KERN_ERR "power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "qmc5983")) 
			{
				printk(KERN_ERR "power off fail!!\n");
			}
		}
	}
	power_on = on;
}

// Daemon application save the data
static int ECS_SaveData(int buf[12])
{
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct qmc5983_i2c_data *data = i2c_get_clientdata(client);
#endif

	mutex_lock(&sensor_data_mutex);
	memcpy(sensor_data, buf, sizeof(sensor_data));	
	mutex_unlock(&sensor_data_mutex);
	
#if DEBUG
	//if(atomic_read(&data->trace) & QMC_HWM_DEBUG)
	//{QMCDBG
	/*
		MSE_LOG("Get daemon data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			sensor_data[0],sensor_data[1],sensor_data[2],sensor_data[3],
			sensor_data[4],sensor_data[5],sensor_data[6],sensor_data[7],
			sensor_data[8],sensor_data[9],sensor_data[10],sensor_data[11]);
	*/
	//}	
#endif

	return 0;

}

static int ECS_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}


/*----------------------------------------------------------------------------*/
static int qmc5983_ReadChipInfo(char *buf, int bufsize)
{
	if((!buf)||(bufsize <= QMC5983_BUFSIZE -1))
	{
		return -1;
	}
	if(!this_client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "qmc5983 Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[QMC5983_BUFSIZE];
	qmc5983_ReadChipInfo(strbuf, QMC5983_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{

	int sensordata[3];
	char strbuf[QMC5983_BUFSIZE];

	QMC5983_read_mag_xyz(&sensordata);
	
	sprintf(strbuf, "%d %d %d\n", sensordata[0],sensordata[1],sensordata[2]);

	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	int tmp[3];
	char strbuf[QMC5983_BUFSIZE];
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
	struct qmc5983_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct i2c_client *client = this_client;  
	struct qmc5983_i2c_data *data = i2c_get_clientdata(client);
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
	struct qmc5983_i2c_data *data = i2c_get_clientdata(client);
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
	struct qmc5983_i2c_data *obj = i2c_get_clientdata(this_client);
	if(NULL == obj)
	{
		printk(KERN_ERR "qmc5983_i2c_data is null!!\n");
		return 0;
	}	
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct qmc5983_i2c_data *obj = i2c_get_clientdata(this_client);
	int trace;
	if(NULL == obj)
	{
		printk(KERN_ERR "qmc5983_i2c_data is null!!\n");
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
	char strbuf[QMC5983_BUFSIZE];
	sprintf(strbuf, "qmc5983d");
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
static struct driver_attribute *qmc5983_attr_list[] = {
    &driver_attr_daemon,
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_posturedata,
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_trace,
};
/*----------------------------------------------------------------------------*/
static int qmc5983_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(qmc5983_attr_list)/sizeof(qmc5983_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(err = driver_create_file(driver, qmc5983_attr_list[idx]))
		{            
			printk(KERN_ERR "driver_create_file (%s) = %d\n", qmc5983_attr_list[idx]->attr.name, err);
			break;
		}
	}
	
	return err;
}
/*----------------------------------------------------------------------------*/
static int qmc5983_delete_attr(struct device_driver *driver)
{
	int idx;
	int num = (int)(sizeof(qmc5983_attr_list)/sizeof(qmc5983_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, qmc5983_attr_list[idx]);
	}
	

	return 0;
}


/*----------------------------------------------------------------------------*/
static int qmc5983_open(struct inode *inode, struct file *file)
{    
	struct qmc5983_i2c_data *obj = i2c_get_clientdata(this_client);    
	int ret = -1;	
	
	if(atomic_read(&obj->trace) & QMC_CTR_DEBUG)
	{
		QMCDBG("Open device node:qmc5983\n");
	}
	ret = nonseekable_open(inode, file);
	
	return ret;
}
/*----------------------------------------------------------------------------*/
static int qmc5983_release(struct inode *inode, struct file *file)
{
	struct qmc5983_i2c_data *obj = i2c_get_clientdata(this_client);
	atomic_dec(&dev_open_count);
	if(atomic_read(&obj->trace) & QMC_CTR_DEBUG)
	{
		QMCDBG("Release device node:qmc5983\n");
	}	
	return 0;
}

/*----------------------------------------------------------------------------*/
static int qmc5983_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	void __user *argp = (void __user *)arg;
		
	/* NOTE: In this function the size of "char" should be 1-byte. */
	char buff[QMC5983_BUFSIZE];				/* for chip information */

	int value[12];			/* for SET_YPR */
	int delay;				/* for GET_DELAY */
	int status; 				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
	unsigned char data[16] = {0};
	int vec[3] = {0};
	struct i2c_client *client = this_client;  
	struct qmc5983_i2c_data *clientdata = i2c_get_clientdata(client);
	hwm_sensor_data* osensor_data;
	uint32_t enable;
	
	short sbuf[3];
	char cmode = 0;
	int err;

	switch (cmd)
	{
		
	case QMC5983_SET_RANGE:
		if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
		err = QMC5983_set_range(*data);
		return err;

	case QMC5983_SET_BANDWIDTH:
		if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
//		err = QMC5983_set_bandwidth(*data);
		return err;

	case QMC5983_SET_MODE:
		if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
//		err = QMC5983_set_mode(*data);
		return err;

	case QMC5983_READ_MAGN_XYZ:
		if(argp == NULL){
			printk(KERN_ERR "IO parameter pointer is NULL!\r\n");
			break;    
		}
		err = QMC5983_read_mag_xyz(vec);
		printk(KERN_INFO "QMC5983_READ_MAGN_XYZ - Ready to report \n");
		printk(KERN_INFO "mag_data[%d, %d, %d]\n",
				vec[0],vec[1],vec[2]);
			if(copy_to_user(argp, vec, sizeof(vec)))
			{
				return -EFAULT;
			}
			break;

	case QMC5983_SET_REGISTER_A: /* New QMC5983 operation */
		if (copy_from_user(data, (unsigned char *)arg, 3) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
//		err = QMC5983_set_registerA(data[0],data[1],data[2]); /* char avg,char rate,char mode */
		return err;
	case QMC5983_SELF_TEST: /* New QMC5983 operation */
			if (copy_from_user(data, (unsigned char *)arg, 1) != 0) {
			#if DEBUG
			printk(KERN_ERR "copy_from_user error\n");
			#endif
			return -EFAULT;
		}
		cmode = data[0];
		err = QMC5983_self_test(cmode, sbuf);
		if (copy_to_user(argp,sbuf,3) != 0)
		{
			#if DEBUG
			printk(KERN_ERR "copy_to error\n");
			#endif
			return -EFAULT;
		}
		return err;

/*------------------------------for daemon------------------------*/
		case ECS_IOCTL_SET_YPR:
			if(argp == NULL)
			{
				QMCDBG("invalid argument.");
				return -EINVAL;
			}
			if(copy_from_user(value, argp, sizeof(value)))
			{
				QMCDBG("copy_from_user failed.");
				return -EFAULT;
			}
			ECS_SaveData(value);
			break;
			
		case ECS_IOCTL_GET_OPEN_STATUS:
			status = ECS_GetOpenStatus();			
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				QMCDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
			
		case ECOMPASS_IOC_GET_MFLAG:
			sensor_status = atomic_read(&m_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				QMCDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;
			
		case ECOMPASS_IOC_GET_OFLAG:
			sensor_status = atomic_read(&o_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				QMCDBG("copy_to_user failed.");
				return -EFAULT;
			}
			break;	
		case ECS_IOCTL_GET_DELAY:
            delay = qmcd_delay;
            if (copy_to_user(argp, &delay, sizeof(delay))) {
                 QMCDBG("copy_to_user failed.");
                 return -EFAULT;
            }
            break;
/*------------------------------for ftm------------------------*/		                

		case MSENSOR_IOCTL_READ_CHIPINFO:       //reserved?
			if(argp == NULL)
			{
				printk(KERN_ERR "IO parameter pointer is NULL!\r\n");
				break;
			}
			
			qmc5983_ReadChipInfo(buff, QMC5983_BUFSIZE);
			if(copy_to_user(argp, buff, strlen(buff)+1))
			{
				return -EFAULT;
			}                
			break;

		case MSENSOR_IOCTL_READ_SENSORDATA:	//for daemon
			if(argp == NULL)
			{
				MSE_LOG("IO parameter pointer is NULL!\r\n");
				break;    
			}
			//MSE_LOG("QMC5983_READ sensor data begin\n");
			QMC5983_read_mag_xyz(vec);
			//MSE_LOG("mag_data[%d, %d, %d]\n",
			//		vec[0],vec[1],vec[2]);
			sprintf(buff, "%x %x %x", vec[0], vec[1], vec[2]);
			if(copy_to_user(argp, buff, strlen(buff)+1))
			{
				return -EFAULT;
			} 

				break;


		case ECOMPASS_IOC_GET_LAYOUT:   //not use
			status = atomic_read(&clientdata->layout);
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_LOG("copy_to_user failed.");
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
				QMCDBG("copy_from_user failed.");
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
				MSE_ERR( "IO parameter pointer is NULL!\r\n");
				break;    
			}
			
			osensor_data = (hwm_sensor_data *)buff;
			mutex_lock(&sensor_data_mutex);
				
			osensor_data->values[0] = sensor_data[8];
			osensor_data->values[1] = sensor_data[9];
			osensor_data->values[2] = sensor_data[10];
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
static struct file_operations qmc5983_fops = {
//	.owner = THIS_MODULE,
	.open = qmc5983_open,
	.release = qmc5983_release,
	.unlocked_ioctl = qmc5983_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice qmc5983_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &qmc5983_fops,
};
/*----------------------------------------------------------------------------*/
int qmc5983_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* msensor_data;
	
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct qmc5983_i2c_data *data = i2c_get_clientdata(client);
#endif
	
#if DEBUG
	if(atomic_read(&data->trace) & QMC_FUN_DEBUG)
	{
		QMCFUNC("qmc5983_operate");
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
					qmcd_delay = 20;
				}
				qmcd_delay = value;
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
				if(atomic_read(&data->trace) & QMC_HWM_DEBUG)
				{
					QMCDBG("Hwm get m-sensor data: %d, %d, %d. divide %d, status %d!\n",
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
int qmc5983_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* osensor_data;	
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct qmc5983_i2c_data *data = i2c_get_clientdata(client);
#endif
	
#if DEBUG
	if(atomic_read(&data->trace) & QMC_FUN_DEBUG)
	{
		QMCFUNC("qmc5983_orientation_operate");
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
					qmcd_delay = 20;
				}
				qmcd_delay = value;
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
			//if(atomic_read(&data->trace) & QMC_HWM_DEBUG)
			//{	QMCDBG
			/*
				MSE_LOG("Hwm get o-sensor data: %d, %d, %d. divide %d, status %d!\n",
					osensor_data->values[0],osensor_data->values[1],osensor_data->values[2],
					osensor_data->value_divide,osensor_data->status);
			*/
			//}	
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
static int qmc5983_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct qmc5983_i2c_data *obj = i2c_get_clientdata(client)
	    

	if(msg.event == PM_EVENT_SUSPEND)
	{
		qmc5983_power(obj->hw, 0);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int qmc5983_resume(struct i2c_client *client)
{
	struct qmc5983_i2c_data *obj = i2c_get_clientdata(client)


	qmc5983_power(obj->hw, 1);
	

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void qmc5983_early_suspend(struct early_suspend *h) 
{
	struct qmc5983_i2c_data *obj = container_of(h, struct qmc5983_i2c_data, early_drv);   

	if(NULL == obj)
	{
		printk(KERN_ERR "null pointer!!\n");
		return;
	}
	if(qmc5983_SetPowerMode(obj->client, false))
	{
		printk("qmc5983: write power control fail!!\n");
		return;
	}
	       
}
/*----------------------------------------------------------------------------*/
static void qmc5983_late_resume(struct early_suspend *h)
{
	struct qmc5983_i2c_data *obj = container_of(h, struct qmc5983_i2c_data, early_drv);         

	
	if(NULL == obj)
	{
		printk(KERN_ERR "null pointer!!\n");
		return;
	}

	if(qmc5983_SetPowerMode(obj->client, true))
	{
		printk("qmc5983: write power control fail!!\n");
		return;
	}
	
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
#if 0
static int qmc5983_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, QMC5983_DEV_NAME);
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int qmc5983_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	MSE_FUN();
	struct i2c_client *new_client;
	struct qmc5983_i2c_data *data;
	char tmp[2];
	int err = 0;
	unsigned char databuf[6] = {0,0,0,0,0,0};
	struct hwmsen_object sobj_m, sobj_o;

	int vec[3]={0,0,0};

	int i=0;

    MSE_LOG("qmc5983 i2c probe 1\n");
	if(!(data = kmalloc(sizeof(struct qmc5983_i2c_data), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct qmc5983_i2c_data));

	data->hw = get_cust_mag_hw();	

	if (hwmsen_get_convert(data->hw->direction, &data->cvt)) {
        printk(KERN_ERR "QMC5983 invalid direction: %d\n", data->hw->direction);
        goto exit_kfree;
    }
	
	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0);
	
	MSE_LOG("qmc5983 i2c probe 2\n");
	
	mutex_init(&sensor_data_mutex);
	mutex_init(&read_i2c_xyz);
	
	init_waitqueue_head(&data_ready_wq);
	init_waitqueue_head(&open_wq);

	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);
	
	this_client = new_client;	
	//this_client->timing=400;

	MSE_LOG("qmc5983 i2c probe 3\n");
	/* read chip id */
	databuf[0] = IRA_REG_M;
	if(I2C_RxData(databuf, 1)<0){
		MSE_ERR("I2C_RxData error!\n");
		goto exit_i2c_failed;
	}
	if (databuf[0] == 0x48) {
		MSE_LOG("I2C driver registered!\n");
	} else {
		MSE_LOG("qmc5983 check ID faild!\n");
		goto exit_i2c_failed;
	}
	MSE_LOG("qmc5983 i2c probe 4\n");


	/*write CRA*/
	databuf[0] = CRA_REG_M;
	//databuf[1] = 0x70;/*(8-average, 15 Hz default, normal measurement)*/
	databuf[1] = 0x7c;/*(8-average, 220 Hz default, normal measurement)*/
	if(I2C_TxData(databuf,2)<0){
		MSE_ERR("write CRA error!\n");
		goto exit_i2c_failed;
	}
	qmc5983_SetPowerMode(new_client,true);/*CC mode & set range GAIN=5 */

	MSE_LOG("qmc5983 i2c probe 5\n");

/*read for test   modify while ok*/
	for(i=0;i<5;i++){
		QMC5983_read_mag_xyz(vec);
		MSE_LOG("QMC5983:%d     %d      %d\n",vec[0],vec[1],vec[2]);
	}

	/* Register sysfs attribute */
	if(err = qmc5983_create_attr(&qmc_sensor_driver.driver))
	{
		printk(KERN_ERR "create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}

	
	if(err = misc_register(&qmc5983_device))
	{
		printk(KERN_ERR "qmc5983_device register failed\n");
		goto exit_misc_device_register_failed;	}    

	sobj_m.self = data;
    sobj_m.polling = 1;
    sobj_m.sensor_operate = qmc5983_operate;
	if(err = hwmsen_attach(ID_MAGNETIC, &sobj_m))
	{
		printk(KERN_ERR "attach fail = %d\n", err);
		goto exit_kfree;
	}
	
	sobj_o.self = data;
    sobj_o.polling = 1;
    sobj_o.sensor_operate = qmc5983_orientation_operate;
	if(err = hwmsen_attach(ID_ORIENTATION, &sobj_o))
	{
		printk(KERN_ERR "attach fail = %d\n", err);
		goto exit_kfree;
	}
	
#if CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	data->early_drv.suspend  = qmc5983_early_suspend,
	data->early_drv.resume   = qmc5983_late_resume,    
	register_early_suspend(&data->early_drv);
#endif

	QMCDBG("%s: OK\n", __func__);
	return 0;

	exit_i2c_failed:
	exit_sysfs_create_group_failed:	
	exit_misc_device_register_failed:
	exit_kfree:
	kfree(data);
	exit:
	printk(KERN_ERR "%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int qmc5983_i2c_remove(struct i2c_client *client)
{
	int err;	
	
	if(err = qmc5983_delete_attr(&qmc_sensor_driver.driver))
	{
		printk(KERN_ERR "qmc5983_delete_attr fail: %d\n", err);
	}
	
	this_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));	
	misc_deregister(&qmc5983_device);    
	return 0;
}
/*----------------------------------------------------------------------------*/
static int qmc_probe(struct platform_device *pdev) 
{
	struct mag_hw *hw = get_cust_mag_hw();

	qmc5983_power(hw, 1);
	MSE_FUN();
	atomic_set(&dev_open_count, 0);
//	qmc5983_force[0] = hw->i2c_num;

	if(i2c_add_driver(&qmc5983_i2c_driver))
	{
		printk(KERN_ERR "add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int qmc_remove(struct platform_device *pdev)
{
	struct mag_hw *hw = get_cust_mag_hw();
 
	qmc5983_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&qmc5983_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int __init qmc5983_init(void)
{
	struct mag_hw *hw = get_cust_mag_hw();
	printk("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_qmc5983, 1);
	//i2c_register_board_info(0, &i2c_qmc5983, 1);	
	if(platform_driver_register(&qmc_sensor_driver))
	{
		printk(KERN_ERR "failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit qmc5983_exit(void)
{	
	platform_driver_unregister(&qmc_sensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(qmc5983_init);
module_exit(qmc5983_exit);

MODULE_AUTHOR("chunlei wang");
MODULE_DESCRIPTION("qmc5983 compass driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);


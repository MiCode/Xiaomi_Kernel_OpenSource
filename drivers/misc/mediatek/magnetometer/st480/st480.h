/*
 * Definitions for senodia compass chip.
 */
#ifndef MAGNETIC_H
#define MAGNETIC_H

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
#include <linux/ioctl.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <cust_mag.h>
#include <linux/hwmsen_helper.h>

/*
 * Self test
 */
#define SENSOR_AUTO_TEST 0

#if SENSOR_AUTO_TEST
#include <linux/kthread.h>
#endif

/*
 * I2C name
 */
#define ST480_I2C_NAME "st480"

/*
 * register shift
 */
#define ST480_REG_DRR_SHIFT 2
#define ST480_DEVICE_ID 0x7c


/*
 * register
 */
#define SINGLE_MEASUREMENT_MODE_CMD 0x3E
#define READ_MEASUREMENT_CMD 0x4E
#define WRITE_REGISTER_CMD 0x60
#define READ_REGISTER_CMD 0x50
#define EXIT_REGISTER_CMD 0x80
#define MEMORY_RECALL_CMD 0xD0
#define MEMORY_STORE_CMD 0xE0
#define RESET_CMD 0xF0

#define CALIBRATION_REG (0x02 << ST480_REG_DRR_SHIFT)
#define CALIBRATION_DATA_LOW 0x1C
#define CALIBRATION_DATA_HIGH 0x00

#define ONE_INIT_DATA_LOW 0x7C
#define ONE_INIT_DATA_HIGH 0x00
#define ONE_INIT_REG (0x00 << ST480_REG_DRR_SHIFT)

#define TWO_INIT_DATA_LOW 0x00
#define TWO_INIT_DATA_HIGH 0x00
#define TWO_INIT_REG (0x02 << ST480_REG_DRR_SHIFT)



/*
 * Miscellaneous set.
 */
#define SENSOR_DATA_SIZE 6
#define MAX_FAILURE_COUNT 3
#define ST480_DEFAULT_DELAY   27

/* 
 * Debug
 */

/*******************************************************************/
#define SENODIAIO                   0xA1

/* IOCTLs for st480d */
#define MSENSOR_IOCTL_SET_CALIDATA     	  	_IOW(MSENSOR, 0x0a, int)
#define MSENSOR_IOCTL_SET_POSTURE        	_IOW(MSENSOR, 0x09, int)



#define IOCTL_SENSOR_GET_DATA_MAG           _IO(SENODIAIO, 0x01)
#define IOCTL_SENSOR_WRITE_DATA_COMPASS     _IO(SENODIAIO, 0x02)
#define IOCTL_SENSOR_GET_COMPASS_FLAG	    _IO(SENODIAIO, 0x03)
#define IOCTL_SENSOR_GET_COMPASS_DELAY 	    _IO(SENODIAIO, 0x04)

#ifdef CONFIG_COMPAT
#define COMPAT_MSENSOR_IOCTL_SET_CALIDATA     	  	_IOW(MSENSOR, 0x0a, compat_int_t)
#define COMPAT_MSENSOR_IOCTL_SET_POSTURE        	_IOW(MSENSOR, 0x09, compat_int_t)
	
	
	
#define COMPAT_IOCTL_SENSOR_GET_DATA_MAG           _IO(SENODIAIO, 0x01)
#define COMPAT_IOCTL_SENSOR_WRITE_DATA_COMPASS     _IO(SENODIAIO, 0x02)
#define COMPAT_IOCTL_SENSOR_GET_COMPASS_FLAG	    _IO(SENODIAIO, 0x03)
#define COMPAT_IOCTL_SENSOR_GET_COMPASS_DELAY 	    _IO(SENODIAIO, 0x04)
#endif




struct SensorData {
    rwlock_t datalock;
    rwlock_t ctrllock;    
    int controldata[10];
    unsigned int debug;
    int yaw;
    int roll;
    int pitch;
    int nmx;
    int nmy;
    int nmz;
    int mag_status;
};
#endif


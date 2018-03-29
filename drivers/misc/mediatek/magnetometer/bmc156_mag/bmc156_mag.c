/* bmm150.c - bmm150 compass driver
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 *
 * VERSION: V1.3
 * History:	V1.0 --- Driver creation
 *          V1.1 --- Add share I2C address solution
 *          V1.2 --- Fix bug that daemon can't get
 *                   delay command.
 *          V1.3 --- Fix the issue of accuracy loss of rotation vector data
 *                   that leads to safari jitter.
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
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/module.h>

#ifdef CONFIG_OF
#define USE_NEW_SENSOR_ARCH
#include <mag.h>
#endif

#include <cust_mag.h>
#include <hwmsen_helper.h>
#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include "bmc156_mag.h"

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */

/*----------------------------------------------------------------------------*/
/*
* Enable the driver to block e-compass daemon on suspend
*/
#define BMC156_BLOCK_DAEMON_ON_SUSPEND
/* #undef	BMC156_BLOCK_DAEMON_ON_SUSPEND */
/*
* Enable gyroscope feature with BMC156
*/
#define BMC156_M4G
/* #undef BMC156_M4G */
/*
* Enable rotation vecter feature with BMC156
*/
#define BMC156_VRV
/* #undef BMC156_VRV */

/*
* Enable virtual linear accelerometer feature with BMC156
*/
#define BMC156_VLA
/* #undef BMC156_VLA */

/*
* Enable virtual gravity feature with BMC156
*/
#define BMC156_VG
/* #undef BMC156_VG */

#ifdef BMC156_M4G
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define ECOMPASS_IOC_GET_GFLAG			_IOR(MSENSOR, 0x30, short)
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define ECOMPASS_IOC_GET_GDELAY			_IOR(MSENSOR, 0x31, int)
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define ECOMPASS_IOC_GET_VRVFLAG			_IOR(MSENSOR, 0x32, short)
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define ECOMPASS_IOC_GET_VRVDELAY			_IOR(MSENSOR, 0x33, int)
#endif /* BMC156_VRV */
#ifdef BMC156_VLA
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define ECOMPASS_IOC_GET_VLAFLAG			_IOR(MSENSOR, 0x34, short)
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define ECOMPASS_IOC_GET_VLADELAY			_IOR(MSENSOR, 0x35, int)
#endif /* BMC156_VLA */
#ifdef BMC156_VG
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define ECOMPASS_IOC_GET_VGFLAG			_IOR(MSENSOR, 0x36, short)
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define ECOMPASS_IOC_GET_VGDELAY			_IOR(MSENSOR, 0x37, int)
#endif /* BMC156_VG */

/*----------------------------------------------------------------------------*/
/*   BMM150 API Section	*/
/*----------------------------------------------------------------------------*/

#define MSE_TAG					"[Msensor] "
#define MSE_FUN(f)				pr_info(MSE_TAG"%s\n", __func__)
#define MSE_ERR(fmt, args...)		pr_err(MSE_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)		pr_debug(MSE_TAG fmt, ##args)
#define BMM150_U16 unsigned short
#define BMM150_S16 signed short
#define BMM150_S32 signed int


#define BMM150_BUS_WR_RETURN_TYPE char
#define BMM150_BUS_WR_PARAM_TYPES\
	unsigned char, unsigned char, unsigned char *, unsigned char
#define BMM150_BUS_WR_PARAM_ORDER\
	(device_addr, register_addr, register_data, wr_len)
#define BMM150_BUS_WRITE_FUNC(\
		device_addr, register_addr, register_data, wr_len)\
	bus_write(device_addr, register_addr, register_data, wr_len)

#define BMM150_BUS_RD_RETURN_TYPE char

#define BMM150_BUS_RD_PARAM_TYPES\
	unsigned char, unsigned char, unsigned char *, unsigned char

#define BMM150_BUS_RD_PARAM_ORDER (device_addr, register_addr, register_data)

#define BMM150_BUS_READ_FUNC(device_addr, register_addr, register_data, rd_len)\
	bus_read(device_addr, register_addr, register_data, rd_len)


#define BMM150_DELAY_RETURN_TYPE void

#define BMM150_DELAY_PARAM_TYPES unsigned int

#define BMM150_DELAY_FUNC(delay_in_msec)\
	delay_func(delay_in_msec)

#define BMM150_DELAY_POWEROFF_SUSPEND      1
#define BMM150_DELAY_SUSPEND_SLEEP         2
#define BMM150_DELAY_SLEEP_ACTIVE          1
#define BMM150_DELAY_ACTIVE_SLEEP          1
#define BMM150_DELAY_SLEEP_SUSPEND         1
#define BMM150_DELAY_ACTIVE_SUSPEND        1
#define BMM150_DELAY_SLEEP_POWEROFF        1
#define BMM150_DELAY_ACTIVE_POWEROFF       1
#define BMM150_DELAY_SETTLING_TIME         2


#define BMM150_RETURN_FUNCTION_TYPE        char
#define BMM150_I2C_ADDRESS                 0x10

/*General Info datas*/
#define BMM150_SOFT_RESET7_ON              1
#define BMM150_SOFT_RESET1_ON              1
#define BMM150_SOFT_RESET7_OFF             0
#define BMM150_SOFT_RESET1_OFF             0
#define BMM150_DELAY_SOFTRESET             1

/* Fixed Data Registers */
#define BMM150_CHIP_ID                     0x40

/* Data Registers */
#define BMM150_DATAX_LSB                   0x42
#define BMM150_DATAX_MSB                   0x43
#define BMM150_DATAY_LSB                   0x44
#define BMM150_DATAY_MSB                   0x45
#define BMM150_DATAZ_LSB                   0x46
#define BMM150_DATAZ_MSB                   0x47
#define BMM150_R_LSB                       0x48
#define BMM150_R_MSB                       0x49

/* Status Registers */
#define BMM150_INT_STAT                    0x4A

/* Control Registers */
#define BMM150_POWER_CNTL                  0x4B
#define BMM150_CONTROL                     0x4C
#define BMM150_INT_CNTL                    0x4D
#define BMM150_SENS_CNTL                   0x4E
#define BMM150_LOW_THRES                   0x4F
#define BMM150_HIGH_THRES                  0x50
#define BMM150_NO_REPETITIONS_XY           0x51
#define BMM150_NO_REPETITIONS_Z            0x52

/* Trim Extended Registers */
#define BMM150_DIG_X1                      0x5D
#define BMM150_DIG_Y1                      0x5E
#define BMM150_DIG_Z4_LSB                  0x62
#define BMM150_DIG_Z4_MSB                  0x63
#define BMM150_DIG_X2                      0x64
#define BMM150_DIG_Y2                      0x65
#define BMM150_DIG_Z2_LSB                  0x68
#define BMM150_DIG_Z2_MSB                  0x69
#define BMM150_DIG_Z1_LSB                  0x6A
#define BMM150_DIG_Z1_MSB                  0x6B
#define BMM150_DIG_XYZ1_LSB                0x6C
#define BMM150_DIG_XYZ1_MSB                0x6D
#define BMM150_DIG_Z3_LSB                  0x6E
#define BMM150_DIG_Z3_MSB                  0x6F
#define BMM150_DIG_XY2                     0x70
#define BMM150_DIG_XY1                     0x71


/* Data X LSB Regsiter */
#define BMM150_DATAX_LSB_VALUEX__POS        3
#define BMM150_DATAX_LSB_VALUEX__LEN        5
#define BMM150_DATAX_LSB_VALUEX__MSK        0xF8
#define BMM150_DATAX_LSB_VALUEX__REG        BMM150_DATAX_LSB

#define BMM150_DATAX_LSB_TESTX__POS         0
#define BMM150_DATAX_LSB_TESTX__LEN         1
#define BMM150_DATAX_LSB_TESTX__MSK         0x01
#define BMM150_DATAX_LSB_TESTX__REG         BMM150_DATAX_LSB

/* Data Y LSB Regsiter */
#define BMM150_DATAY_LSB_VALUEY__POS        3
#define BMM150_DATAY_LSB_VALUEY__LEN        5
#define BMM150_DATAY_LSB_VALUEY__MSK        0xF8
#define BMM150_DATAY_LSB_VALUEY__REG        BMM150_DATAY_LSB

#define BMM150_DATAY_LSB_TESTY__POS         0
#define BMM150_DATAY_LSB_TESTY__LEN         1
#define BMM150_DATAY_LSB_TESTY__MSK         0x01
#define BMM150_DATAY_LSB_TESTY__REG         BMM150_DATAY_LSB

/* Data Z LSB Regsiter */
#define BMM150_DATAZ_LSB_VALUEZ__POS        1
#define BMM150_DATAZ_LSB_VALUEZ__LEN        7
#define BMM150_DATAZ_LSB_VALUEZ__MSK        0xFE
#define BMM150_DATAZ_LSB_VALUEZ__REG        BMM150_DATAZ_LSB

#define BMM150_DATAZ_LSB_TESTZ__POS         0
#define BMM150_DATAZ_LSB_TESTZ__LEN         1
#define BMM150_DATAZ_LSB_TESTZ__MSK         0x01
#define BMM150_DATAZ_LSB_TESTZ__REG         BMM150_DATAZ_LSB

/* Hall Resistance LSB Regsiter */
#define BMM150_R_LSB_VALUE__POS             2
#define BMM150_R_LSB_VALUE__LEN             6
#define BMM150_R_LSB_VALUE__MSK             0xFC
#define BMM150_R_LSB_VALUE__REG             BMM150_R_LSB

#define BMM150_DATA_RDYSTAT__POS            0
#define BMM150_DATA_RDYSTAT__LEN            1
#define BMM150_DATA_RDYSTAT__MSK            0x01
#define BMM150_DATA_RDYSTAT__REG            BMM150_R_LSB

/* Interupt Status Register */
#define BMM150_INT_STAT_DOR__POS            7
#define BMM150_INT_STAT_DOR__LEN            1
#define BMM150_INT_STAT_DOR__MSK            0x80
#define BMM150_INT_STAT_DOR__REG            BMM150_INT_STAT

#define BMM150_INT_STAT_OVRFLOW__POS        6
#define BMM150_INT_STAT_OVRFLOW__LEN        1
#define BMM150_INT_STAT_OVRFLOW__MSK        0x40
#define BMM150_INT_STAT_OVRFLOW__REG        BMM150_INT_STAT

#define BMM150_INT_STAT_HIGH_THZ__POS       5
#define BMM150_INT_STAT_HIGH_THZ__LEN       1
#define BMM150_INT_STAT_HIGH_THZ__MSK       0x20
#define BMM150_INT_STAT_HIGH_THZ__REG       BMM150_INT_STAT

#define BMM150_INT_STAT_HIGH_THY__POS       4
#define BMM150_INT_STAT_HIGH_THY__LEN       1
#define BMM150_INT_STAT_HIGH_THY__MSK       0x10
#define BMM150_INT_STAT_HIGH_THY__REG       BMM150_INT_STAT

#define BMM150_INT_STAT_HIGH_THX__POS       3
#define BMM150_INT_STAT_HIGH_THX__LEN       1
#define BMM150_INT_STAT_HIGH_THX__MSK       0x08
#define BMM150_INT_STAT_HIGH_THX__REG       BMM150_INT_STAT

#define BMM150_INT_STAT_LOW_THZ__POS        2
#define BMM150_INT_STAT_LOW_THZ__LEN        1
#define BMM150_INT_STAT_LOW_THZ__MSK        0x04
#define BMM150_INT_STAT_LOW_THZ__REG        BMM150_INT_STAT

#define BMM150_INT_STAT_LOW_THY__POS        1
#define BMM150_INT_STAT_LOW_THY__LEN        1
#define BMM150_INT_STAT_LOW_THY__MSK        0x02
#define BMM150_INT_STAT_LOW_THY__REG        BMM150_INT_STAT

#define BMM150_INT_STAT_LOW_THX__POS        0
#define BMM150_INT_STAT_LOW_THX__LEN        1
#define BMM150_INT_STAT_LOW_THX__MSK        0x01
#define BMM150_INT_STAT_LOW_THX__REG        BMM150_INT_STAT

/* Power Control Register */
#define BMM150_POWER_CNTL_SRST7__POS       7
#define BMM150_POWER_CNTL_SRST7__LEN       1
#define BMM150_POWER_CNTL_SRST7__MSK       0x80
#define BMM150_POWER_CNTL_SRST7__REG       BMM150_POWER_CNTL

#define BMM150_POWER_CNTL_SPI3_EN__POS     2
#define BMM150_POWER_CNTL_SPI3_EN__LEN     1
#define BMM150_POWER_CNTL_SPI3_EN__MSK     0x04
#define BMM150_POWER_CNTL_SPI3_EN__REG     BMM150_POWER_CNTL

#define BMM150_POWER_CNTL_SRST1__POS       1
#define BMM150_POWER_CNTL_SRST1__LEN       1
#define BMM150_POWER_CNTL_SRST1__MSK       0x02
#define BMM150_POWER_CNTL_SRST1__REG       BMM150_POWER_CNTL

#define BMM150_POWER_CNTL_PCB__POS         0
#define BMM150_POWER_CNTL_PCB__LEN         1
#define BMM150_POWER_CNTL_PCB__MSK         0x01
#define BMM150_POWER_CNTL_PCB__REG         BMM150_POWER_CNTL

/* Control Register */
#define BMM150_CNTL_ADV_ST__POS            6
#define BMM150_CNTL_ADV_ST__LEN            2
#define BMM150_CNTL_ADV_ST__MSK            0xC0
#define BMM150_CNTL_ADV_ST__REG            BMM150_CONTROL

#define BMM150_CNTL_DR__POS                3
#define BMM150_CNTL_DR__LEN                3
#define BMM150_CNTL_DR__MSK                0x38
#define BMM150_CNTL_DR__REG                BMM150_CONTROL

#define BMM150_CNTL_OPMODE__POS            1
#define BMM150_CNTL_OPMODE__LEN            2
#define BMM150_CNTL_OPMODE__MSK            0x06
#define BMM150_CNTL_OPMODE__REG            BMM150_CONTROL

#define BMM150_CNTL_S_TEST__POS            0
#define BMM150_CNTL_S_TEST__LEN            1
#define BMM150_CNTL_S_TEST__MSK            0x01
#define BMM150_CNTL_S_TEST__REG            BMM150_CONTROL

/* Interupt Control Register */
#define BMM150_INT_CNTL_DOR_EN__POS            7
#define BMM150_INT_CNTL_DOR_EN__LEN            1
#define BMM150_INT_CNTL_DOR_EN__MSK            0x80
#define BMM150_INT_CNTL_DOR_EN__REG            BMM150_INT_CNTL

#define BMM150_INT_CNTL_OVRFLOW_EN__POS        6
#define BMM150_INT_CNTL_OVRFLOW_EN__LEN        1
#define BMM150_INT_CNTL_OVRFLOW_EN__MSK        0x40
#define BMM150_INT_CNTL_OVRFLOW_EN__REG        BMM150_INT_CNTL

#define BMM150_INT_CNTL_HIGH_THZ_EN__POS       5
#define BMM150_INT_CNTL_HIGH_THZ_EN__LEN       1
#define BMM150_INT_CNTL_HIGH_THZ_EN__MSK       0x20
#define BMM150_INT_CNTL_HIGH_THZ_EN__REG       BMM150_INT_CNTL

#define BMM150_INT_CNTL_HIGH_THY_EN__POS       4
#define BMM150_INT_CNTL_HIGH_THY_EN__LEN       1
#define BMM150_INT_CNTL_HIGH_THY_EN__MSK       0x10
#define BMM150_INT_CNTL_HIGH_THY_EN__REG       BMM150_INT_CNTL

#define BMM150_INT_CNTL_HIGH_THX_EN__POS       3
#define BMM150_INT_CNTL_HIGH_THX_EN__LEN       1
#define BMM150_INT_CNTL_HIGH_THX_EN__MSK       0x08
#define BMM150_INT_CNTL_HIGH_THX_EN__REG       BMM150_INT_CNTL

#define BMM150_INT_CNTL_LOW_THZ_EN__POS        2
#define BMM150_INT_CNTL_LOW_THZ_EN__LEN        1
#define BMM150_INT_CNTL_LOW_THZ_EN__MSK        0x04
#define BMM150_INT_CNTL_LOW_THZ_EN__REG        BMM150_INT_CNTL

#define BMM150_INT_CNTL_LOW_THY_EN__POS        1
#define BMM150_INT_CNTL_LOW_THY_EN__LEN        1
#define BMM150_INT_CNTL_LOW_THY_EN__MSK        0x02
#define BMM150_INT_CNTL_LOW_THY_EN__REG        BMM150_INT_CNTL

#define BMM150_INT_CNTL_LOW_THX_EN__POS        0
#define BMM150_INT_CNTL_LOW_THX_EN__LEN        1
#define BMM150_INT_CNTL_LOW_THX_EN__MSK        0x01
#define BMM150_INT_CNTL_LOW_THX_EN__REG        BMM150_INT_CNTL

/* Sensor Control Register */
#define BMM150_SENS_CNTL_DRDY_EN__POS          7
#define BMM150_SENS_CNTL_DRDY_EN__LEN          1
#define BMM150_SENS_CNTL_DRDY_EN__MSK          0x80
#define BMM150_SENS_CNTL_DRDY_EN__REG          BMM150_SENS_CNTL

#define BMM150_SENS_CNTL_IE__POS               6
#define BMM150_SENS_CNTL_IE__LEN               1
#define BMM150_SENS_CNTL_IE__MSK               0x40
#define BMM150_SENS_CNTL_IE__REG               BMM150_SENS_CNTL

#define BMM150_SENS_CNTL_CHANNELZ__POS         5
#define BMM150_SENS_CNTL_CHANNELZ__LEN         1
#define BMM150_SENS_CNTL_CHANNELZ__MSK         0x20
#define BMM150_SENS_CNTL_CHANNELZ__REG         BMM150_SENS_CNTL

#define BMM150_SENS_CNTL_CHANNELY__POS         4
#define BMM150_SENS_CNTL_CHANNELY__LEN         1
#define BMM150_SENS_CNTL_CHANNELY__MSK         0x10
#define BMM150_SENS_CNTL_CHANNELY__REG         BMM150_SENS_CNTL

#define BMM150_SENS_CNTL_CHANNELX__POS         3
#define BMM150_SENS_CNTL_CHANNELX__LEN         1
#define BMM150_SENS_CNTL_CHANNELX__MSK         0x08
#define BMM150_SENS_CNTL_CHANNELX__REG         BMM150_SENS_CNTL

#define BMM150_SENS_CNTL_DR_POLARITY__POS      2
#define BMM150_SENS_CNTL_DR_POLARITY__LEN      1
#define BMM150_SENS_CNTL_DR_POLARITY__MSK      0x04
#define BMM150_SENS_CNTL_DR_POLARITY__REG      BMM150_SENS_CNTL

#define BMM150_SENS_CNTL_INTERRUPT_LATCH__POS            1
#define BMM150_SENS_CNTL_INTERRUPT_LATCH__LEN            1
#define BMM150_SENS_CNTL_INTERRUPT_LATCH__MSK            0x02
#define BMM150_SENS_CNTL_INTERRUPT_LATCH__REG            BMM150_SENS_CNTL

#define BMM150_SENS_CNTL_INTERRUPT_POLARITY__POS         0
#define BMM150_SENS_CNTL_INTERRUPT_POLARITY__LEN         1
#define BMM150_SENS_CNTL_INTERRUPT_POLARITY__MSK         0x01
#define BMM150_SENS_CNTL_INTERRUPT_POLARITY__REG         BMM150_SENS_CNTL

/* Register 6D */
#define BMM150_DIG_XYZ1_MSB__POS         0
#define BMM150_DIG_XYZ1_MSB__LEN         7
#define BMM150_DIG_XYZ1_MSB__MSK         0x7F
#define BMM150_DIG_XYZ1_MSB__REG         BMM150_DIG_XYZ1_MSB


#define BMM150_X_AXIS               0
#define BMM150_Y_AXIS               1
#define BMM150_Z_AXIS               2
#define BMM150_RESISTANCE           3
#define BMM150_X                    1
#define BMM150_Y                    2
#define BMM150_Z                    4
#define BMM150_XYZ                  7

/* Constants */
#define BMM150_NULL                             0
#define BMM150_DISABLE                          0
#define BMM150_ENABLE                           1
#define BMM150_CHANNEL_DISABLE                  1
#define BMM150_CHANNEL_ENABLE                   0
#define BMM150_INTPIN_LATCH_ENABLE              1
#define BMM150_INTPIN_LATCH_DISABLE             0
#define BMM150_OFF                              0
#define BMM150_ON                               1

#define BMM150_NORMAL_MODE                      0x00
#define BMM150_FORCED_MODE                      0x01
#define BMM150_SUSPEND_MODE                     0x02
#define BMM150_SLEEP_MODE                       0x03

#define BMM150_ADVANCED_SELFTEST_OFF            0
#define BMM150_ADVANCED_SELFTEST_NEGATIVE       2
#define BMM150_ADVANCED_SELFTEST_POSITIVE       3

#define BMM150_NEGATIVE_SATURATION_Z            -32767
#define BMM150_POSITIVE_SATURATION_Z            32767

#define BMM150_SPI_RD_MASK                      0x80
#define BMM150_READ_SET                         0x01

#define E_BMM150_NULL_PTR                       ((char)-127)
#define E_BMM150_COMM_RES                       ((char)-1)
#define E_BMM150_OUT_OF_RANGE                   ((char)-2)
#define E_BMM150_UNDEFINED_MODE                 0

#define BMM150_WR_FUNC_PTR\
	char (*bus_write)(unsigned char, unsigned char,\
			unsigned char *, unsigned char)

#define BMM150_RD_FUNC_PTR\
	char (*bus_read)(unsigned char, unsigned char,\
			unsigned char *, unsigned char)
#define BMM150_MDELAY_DATA_TYPE unsigned int

/*Shifting Constants*/
#define SHIFT_RIGHT_1_POSITION                  1
#define SHIFT_RIGHT_2_POSITION                  2
#define SHIFT_RIGHT_3_POSITION                  3
#define SHIFT_RIGHT_4_POSITION                  4
#define SHIFT_RIGHT_5_POSITION                  5
#define SHIFT_RIGHT_6_POSITION                  6
#define SHIFT_RIGHT_7_POSITION                  7
#define SHIFT_RIGHT_8_POSITION                  8

#define SHIFT_LEFT_1_POSITION                   1
#define SHIFT_LEFT_2_POSITION                   2
#define SHIFT_LEFT_3_POSITION                   3
#define SHIFT_LEFT_4_POSITION                   4
#define SHIFT_LEFT_5_POSITION                   5
#define SHIFT_LEFT_6_POSITION                   6
#define SHIFT_LEFT_7_POSITION                   7
#define SHIFT_LEFT_8_POSITION                   8

/* Conversion factors*/
#define BMM150_CONVFACTOR_LSB_UT                6

/* get bit slice  */
#define BMM150_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

/* Set bit slice */
#define BMM150_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/* compensated output value returned if sensor had overflow */
#define BMM150_OVERFLOW_OUTPUT       -32768
#define BMM150_OVERFLOW_OUTPUT_S32   ((BMM150_S32)(-2147483647-1))
#define BMM150_OVERFLOW_OUTPUT_FLOAT 0.0f
#define BMM150_FLIP_OVERFLOW_ADCVAL  -4096
#define BMM150_HALL_OVERFLOW_ADCVAL  -16384


#define BMM150_PRESETMODE_LOWPOWER                  1
#define BMM150_PRESETMODE_REGULAR                   2
#define BMM150_PRESETMODE_HIGHACCURACY              3
#define BMM150_PRESETMODE_ENHANCED                  4

/* PRESET MODES - DATA RATES */
#define BMM150_LOWPOWER_DR                       BMM150_DR_10HZ
#define BMM150_REGULAR_DR                        BMM150_DR_10HZ
#define BMM150_HIGHACCURACY_DR                   BMM150_DR_20HZ
#define BMM150_ENHANCED_DR                       BMM150_DR_10HZ

/* PRESET MODES - REPETITIONS-XY RATES */
#define BMM150_LOWPOWER_REPXY                     1
#define BMM150_REGULAR_REPXY                      4
#define BMM150_HIGHACCURACY_REPXY                23
#define BMM150_ENHANCED_REPXY                     7

/* PRESET MODES - REPETITIONS-Z RATES */
#define BMM150_LOWPOWER_REPZ                      2
#define BMM150_REGULAR_REPZ                      15
#define BMM150_HIGHACCURACY_REPZ                 82
#define BMM150_ENHANCED_REPZ                     26

/* Data Rates */

#define BMM150_DR_10HZ                     0
#define BMM150_DR_02HZ                     1
#define BMM150_DR_06HZ                     2
#define BMM150_DR_08HZ                     3
#define BMM150_DR_15HZ                     4
#define BMM150_DR_20HZ                     5
#define BMM150_DR_25HZ                     6
#define BMM150_DR_30HZ                     7

/*user defined Structures*/
struct bmm150api_mdata {
	BMM150_S16 datax;
	BMM150_S16 datay;
	BMM150_S16 dataz;
	BMM150_U16 resistance;
};
struct bmm150api_mdata_s32 {
	BMM150_S32 datax;
	BMM150_S32 datay;
	BMM150_S32 dataz;
	BMM150_U16 resistance;
};
struct bmm150api_mdata_float {
	float datax;
	float datay;
	float dataz;
	BMM150_U16 resistance;
};

struct bmm150api {
	unsigned char company_id;
	unsigned char dev_addr;

	BMM150_WR_FUNC_PTR;
	BMM150_RD_FUNC_PTR;
	void (*delay_msec)(BMM150_MDELAY_DATA_TYPE);

	signed char dig_x1;
	signed char dig_y1;

	signed char dig_x2;
	signed char dig_y2;

	BMM150_U16 dig_z1;
	BMM150_S16 dig_z2;
	BMM150_S16 dig_z3;
	BMM150_S16 dig_z4;

	unsigned char dig_xy1;
	signed char dig_xy2;

	BMM150_U16 dig_xyz1;
};


static BMM150_RETURN_FUNCTION_TYPE bmm150api_init(struct bmm150api *p_bmm150);
/*static BMM150_RETURN_FUNCTION_TYPE bmm150api_read_mdataXYZ(
		struct bmm150api_mdata *mdata);*/
static BMM150_RETURN_FUNCTION_TYPE bmm150api_read_mdataXYZ_s32(
		struct bmm150api_mdata_s32 *mdata);
#ifdef ENABLE_FLOAT
static BMM150_RETURN_FUNCTION_TYPE bmm150api_read_mdataXYZ_float(
		struct bmm150api_mdata_float *mdata);
#endif
/*static BMM150_RETURN_FUNCTION_TYPE bmm150api_read_register(
		unsigned char addr, unsigned char *data, unsigned char len);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_write_register(
		unsigned char addr, unsigned char *data, unsigned char len);*/
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_self_test_XYZ(
		unsigned char *self_testxyz);
static BMM150_S16 bmm150api_compensate_X(
		BMM150_S16 mdata_x, BMM150_U16 data_R);
static BMM150_S32 bmm150api_compensate_X_s32(
		BMM150_S16 mdata_x, BMM150_U16 data_R);
#ifdef ENABLE_FLOAT
static float bmm150api_compensate_X_float(
		BMM150_S16 mdata_x, BMM150_U16 data_R);
#endif
static BMM150_S16 bmm150api_compensate_Y(
		BMM150_S16 mdata_y, BMM150_U16 data_R);
static BMM150_S32 bmm150api_compensate_Y_s32(
		BMM150_S16 mdata_y, BMM150_U16 data_R);
#ifdef ENABLE_FLOAT
static float bmm150api_compensate_Y_float(
		BMM150_S16 mdata_y, BMM150_U16 data_R);
#endif
/*static BMM150_S16 bmm150api_compensate_Z(
		BMM150_S16 mdata_z, BMM150_U16 data_R);*/
static BMM150_S32 bmm150api_compensate_Z_s32(
		BMM150_S16 mdata_z, BMM150_U16 data_R);
#ifdef ENABLE_FLOAT
static float bmm150api_compensate_Z_float(
		BMM150_S16 mdata_z, BMM150_U16 data_R);
#endif
/*static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_raw_xyz(
		struct bmm150api_mdata *mdata);*/
static BMM150_RETURN_FUNCTION_TYPE bmm150api_init_trim_registers(void);
/*static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_spi3(
		unsigned char value);*/
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_powermode(
		unsigned char *mode);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_powermode(
		unsigned char mode);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_adv_selftest(
		unsigned char adv_selftest);
/*static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_adv_selftest(
		unsigned char *adv_selftest);*/
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_datarate(
		unsigned char data_rate);
/*static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_datarate(
		unsigned char *data_rate);*/
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_functional_state(
		unsigned char functional_state);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_functional_state(
		unsigned char *functional_state);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_selftest(
		unsigned char selftest);
/*static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_selftest(
		unsigned char *selftest);*/
static BMM150_RETURN_FUNCTION_TYPE bmm150api_perform_advanced_selftest(
		BMM150_S16 *diff_z);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_repetitions_XY(
		unsigned char *no_repetitions_xy);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_repetitions_XY(
		unsigned char no_repetitions_xy);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_repetitions_Z(
		unsigned char *no_repetitions_z);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_repetitions_Z(
		unsigned char no_repetitions_z);
/*static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_presetmode(unsigned char *mode);*/
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_presetmode(unsigned char mode);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_control_measurement_x(
		unsigned char enable_disable);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_control_measurement_y(
		unsigned char enable_disable);
static BMM150_RETURN_FUNCTION_TYPE bmm150api_soft_reset(void);

static struct bmm150api *p_bmm150;

static BMM150_RETURN_FUNCTION_TYPE bmm150api_init(struct bmm150api *bmm150)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char a_data_u8r[2];

	p_bmm150 = bmm150;
	p_bmm150->dev_addr = BMM150_I2C_ADDRESS;

	/* set device from suspend into sleep mode */
	bmm150api_set_powermode(BMM150_ON);

	/* wait two millisecond for bmc to settle */
	p_bmm150->delay_msec(BMM150_DELAY_SETTLING_TIME);

	/*Read CHIP_ID and REv. info */
	comres = p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_CHIP_ID, a_data_u8r, 1);
	p_bmm150->company_id = a_data_u8r[0];

	/* Function to initialise trim values */
	bmm150api_init_trim_registers();
	bmm150api_set_presetmode(BMM150_PRESETMODE_REGULAR);
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_presetmode(unsigned char mode)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;

	switch (mode) {
	case BMM150_PRESETMODE_LOWPOWER:
		/* Set the data rate for Low Power mode */
		comres = bmm150api_set_datarate(BMM150_LOWPOWER_DR);
		/* Set the XY-repetitions number for Low Power mode */
		comres |= bmm150api_set_repetitions_XY(BMM150_LOWPOWER_REPXY);
		/* Set the Z-repetitions number  for Low Power mode */
		comres |= bmm150api_set_repetitions_Z(BMM150_LOWPOWER_REPZ);
		break;
	case BMM150_PRESETMODE_REGULAR:
		/* Set the data rate for Regular mode */
		comres = bmm150api_set_datarate(BMM150_REGULAR_DR);
		/* Set the XY-repetitions number for Regular mode */
		comres |= bmm150api_set_repetitions_XY(BMM150_REGULAR_REPXY);
		/* Set the Z-repetitions number  for Regular mode */
		comres |= bmm150api_set_repetitions_Z(BMM150_REGULAR_REPZ);
		break;
	case BMM150_PRESETMODE_HIGHACCURACY:
		/* Set the data rate for High Accuracy mode */
		comres = bmm150api_set_datarate(BMM150_HIGHACCURACY_DR);
		/* Set the XY-repetitions number for High Accuracy mode */
		comres |= bmm150api_set_repetitions_XY(BMM150_HIGHACCURACY_REPXY);
		/* Set the Z-repetitions number  for High Accuracyr mode */
		comres |= bmm150api_set_repetitions_Z(BMM150_HIGHACCURACY_REPZ);
		break;
	case BMM150_PRESETMODE_ENHANCED:
		/* Set the data rate for Enhanced Accuracy mode */
		comres = bmm150api_set_datarate(BMM150_ENHANCED_DR);
		/* Set the XY-repetitions number for High Enhanced mode */
		comres |= bmm150api_set_repetitions_XY(BMM150_ENHANCED_REPXY);
		/* Set the Z-repetitions number  for High Enhanced mode */
		comres |= bmm150api_set_repetitions_Z(BMM150_ENHANCED_REPZ);
		break;
	default:
		comres = E_BMM150_OUT_OF_RANGE;
		break;
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_functional_state(
		unsigned char functional_state)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		switch (functional_state) {
		case BMM150_NORMAL_MODE:
			comres = bmm150api_get_powermode(&v_data1_u8r);
			if (v_data1_u8r == BMM150_OFF) {
				comres |= bmm150api_set_powermode(BMM150_ON);
				p_bmm150->delay_msec(
						BMM150_DELAY_SUSPEND_SLEEP);
			}
			{
				comres |= p_bmm150->BMM150_BUS_READ_FUNC(
						p_bmm150->dev_addr,
						BMM150_CNTL_OPMODE__REG,
						&v_data1_u8r, 1);
				v_data1_u8r = BMM150_SET_BITSLICE(
						v_data1_u8r,
						BMM150_CNTL_OPMODE,
						BMM150_NORMAL_MODE);
				comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
						p_bmm150->dev_addr,
						BMM150_CNTL_OPMODE__REG,
						&v_data1_u8r, 1);
			}
			break;
		case BMM150_SUSPEND_MODE:
			comres = bmm150api_set_powermode(BMM150_OFF);
			break;
		case BMM150_FORCED_MODE:
			comres = bmm150api_get_powermode(&v_data1_u8r);
			if (v_data1_u8r == BMM150_OFF) {
				comres = bmm150api_set_powermode(BMM150_ON);
				p_bmm150->delay_msec(
						BMM150_DELAY_SUSPEND_SLEEP);
			}
			comres |= p_bmm150->BMM150_BUS_READ_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_OPMODE__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMM150_SET_BITSLICE(
					v_data1_u8r,
					BMM150_CNTL_OPMODE, BMM150_ON);
			comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_OPMODE__REG,
					&v_data1_u8r, 1);
			break;
		case BMM150_SLEEP_MODE:
			bmm150api_get_powermode(&v_data1_u8r);
			if (v_data1_u8r == BMM150_OFF) {
				comres = bmm150api_set_powermode(BMM150_ON);
				p_bmm150->delay_msec(
						BMM150_DELAY_SUSPEND_SLEEP);
			}
			comres |= p_bmm150->BMM150_BUS_READ_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_OPMODE__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMM150_SET_BITSLICE(
					v_data1_u8r,
					BMM150_CNTL_OPMODE,
					BMM150_SLEEP_MODE);
			comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_OPMODE__REG,
					&v_data1_u8r, 1);
			break;
		default:
			comres = E_BMM150_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_functional_state(
		unsigned char *functional_state)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_CNTL_OPMODE__REG,
				&v_data_u8r, 1);
		*functional_state = BMM150_GET_BITSLICE(
				v_data_u8r, BMM150_CNTL_OPMODE);
	}
	return comres;
}
#if 0
static BMM150_RETURN_FUNCTION_TYPE bmm150api_read_mdataXYZ(struct bmm150api_mdata *mdata)
{
	BMM150_RETURN_FUNCTION_TYPE comres;

	unsigned char a_data_u8r[8];

	struct {
		BMM150_S16 raw_dataX;
		BMM150_S16 raw_dataY;
		BMM150_S16 raw_dataZ;
		BMM150_U16 raw_dataR;
	} raw_dataXYZ;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
				BMM150_DATAX_LSB, a_data_u8r, 8);

		/* Reading data for X axis */
		a_data_u8r[0] = BMM150_GET_BITSLICE(a_data_u8r[0],
				BMM150_DATAX_LSB_VALUEX);
		raw_dataXYZ.raw_dataX = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[1])) <<
					SHIFT_LEFT_5_POSITION) | a_data_u8r[0]);

		/* Reading data for Y axis */
		a_data_u8r[2] = BMM150_GET_BITSLICE(a_data_u8r[2],
				BMM150_DATAY_LSB_VALUEY);
		raw_dataXYZ.raw_dataY = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[3])) <<
					SHIFT_LEFT_5_POSITION) | a_data_u8r[2]);

		/* Reading data for Z axis */
		a_data_u8r[4] = BMM150_GET_BITSLICE(a_data_u8r[4],
				BMM150_DATAZ_LSB_VALUEZ);
		raw_dataXYZ.raw_dataZ = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[5])) <<
					SHIFT_LEFT_7_POSITION) | a_data_u8r[4]);

		/* Reading data for Resistance*/
		a_data_u8r[6] = BMM150_GET_BITSLICE(a_data_u8r[6],
				BMM150_R_LSB_VALUE);
		raw_dataXYZ.raw_dataR = (BMM150_U16)((((BMM150_U16)
						a_data_u8r[7]) <<
					SHIFT_LEFT_6_POSITION) | a_data_u8r[6]);

		/* Compensation for X axis */
		mdata->datax = bmm150api_compensate_X(raw_dataXYZ.raw_dataX,
				raw_dataXYZ.raw_dataR);

		/* Compensation for Y axis */
		mdata->datay = bmm150api_compensate_Y(raw_dataXYZ.raw_dataY,
				raw_dataXYZ.raw_dataR);

		/* Compensation for Z axis */
		mdata->dataz = bmm150api_compensate_Z(raw_dataXYZ.raw_dataZ,
				raw_dataXYZ.raw_dataR);

		/* Output raw resistance value */
		mdata->resistance = raw_dataXYZ.raw_dataR;
	}
	return comres;
}
#endif
static BMM150_RETURN_FUNCTION_TYPE bmm150api_read_mdataXYZ_s32(
	struct bmm150api_mdata_s32 *mdata)
{
	BMM150_RETURN_FUNCTION_TYPE comres;
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;
#else
	unsigned char a_data_u8r[8];
#endif

	struct {
		BMM150_S16 raw_dataX;
		BMM150_S16 raw_dataY;
		BMM150_S16 raw_dataZ;
		BMM150_U16 raw_dataR;
	} raw_dataXYZ;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
		req.get_data_req.sensorType = ID_MAGNETIC;
		req.get_data_req.action = SENSOR_HUB_GET_DATA;
		len = sizeof(req.get_data_req);
		err = SCP_sensorHub_req_send(&req, &len, 1);
		if (err) {
			MSE_ERR("SCP_sensorHub_req_send failed!\n");
			return E_BMM150_NULL_PTR;
		}

		if (ID_MAGNETIC != req.get_data_rsp.sensorType ||
			SENSOR_HUB_GET_DATA != req.get_data_rsp.action || 0 != req.get_data_rsp.errCode) {
			MSE_ERR("error : %d\n", req.get_data_rsp.errCode);
			return E_BMM150_NULL_PTR;
		}
		comres = req.get_data_rsp.errCode;
		raw_dataXYZ.raw_dataX = req.get_data_rsp.int16_Data[0];
		raw_dataXYZ.raw_dataY = req.get_data_rsp.int16_Data[1];
		raw_dataXYZ.raw_dataZ = req.get_data_rsp.int16_Data[2];
		raw_dataXYZ.raw_dataR = (BMM150_U16)req.get_data_rsp.int16_Data[3];
		/*MSE_ERR("x = %d, y = %d, z = %d, R = %d\n", raw_dataXYZ.raw_dataX,
		raw_dataXYZ.raw_dataY, raw_dataXYZ.raw_dataZ, raw_dataXYZ.raw_dataR);*/

#else

		comres = p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
				BMM150_DATAX_LSB, a_data_u8r, 8);

		/* Reading data for X axis */
		a_data_u8r[0] = BMM150_GET_BITSLICE(a_data_u8r[0],
				BMM150_DATAX_LSB_VALUEX);
		raw_dataXYZ.raw_dataX = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[1])) <<
					SHIFT_LEFT_5_POSITION) | a_data_u8r[0]);

		/* Reading data for Y axis */
		a_data_u8r[2] = BMM150_GET_BITSLICE(a_data_u8r[2],
				BMM150_DATAY_LSB_VALUEY);
		raw_dataXYZ.raw_dataY = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[3])) <<
					SHIFT_LEFT_5_POSITION) | a_data_u8r[2]);

		/* Reading data for Z axis */
		a_data_u8r[4] = BMM150_GET_BITSLICE(a_data_u8r[4],
				BMM150_DATAZ_LSB_VALUEZ);
		raw_dataXYZ.raw_dataZ = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[5])) <<
					SHIFT_LEFT_7_POSITION) | a_data_u8r[4]);

		/* Reading data for Resistance*/
		a_data_u8r[6] = BMM150_GET_BITSLICE(a_data_u8r[6],
				BMM150_R_LSB_VALUE);
		raw_dataXYZ.raw_dataR = (BMM150_U16)((((BMM150_U16)
						a_data_u8r[7]) <<
					SHIFT_LEFT_6_POSITION) | a_data_u8r[6]);
#endif/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */

		/* Compensation for X axis */
		mdata->datax = bmm150api_compensate_X_s32(raw_dataXYZ.raw_dataX,
				raw_dataXYZ.raw_dataR);

		/* Compensation for Y axis */
		mdata->datay = bmm150api_compensate_Y_s32(raw_dataXYZ.raw_dataY,
				raw_dataXYZ.raw_dataR);

		/* Compensation for Z axis */
		mdata->dataz = bmm150api_compensate_Z_s32(raw_dataXYZ.raw_dataZ,
				raw_dataXYZ.raw_dataR);

		/* Output raw resistance value */
		mdata->resistance = raw_dataXYZ.raw_dataR;
	}
	return comres;
}

#ifdef ENABLE_FLOAT
static BMM150_RETURN_FUNCTION_TYPE bmm150api_read_mdataXYZ_float(
	struct bmm150api_mdata_float *mdata)
{
	BMM150_RETURN_FUNCTION_TYPE comres;

	unsigned char a_data_u8r[8];

	struct {
		BMM150_S16 raw_dataX;
		BMM150_S16 raw_dataY;
		BMM150_S16 raw_dataZ;
		BMM150_U16 raw_dataR;
	} raw_dataXYZ;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
				BMM150_DATAX_LSB, a_data_u8r, 8);

		/* Reading data for X axis */
		a_data_u8r[0] = BMM150_GET_BITSLICE(a_data_u8r[0],
				BMM150_DATAX_LSB_VALUEX);
		raw_dataXYZ.raw_dataX = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[1])) <<
					SHIFT_LEFT_5_POSITION) | a_data_u8r[0]);

		/* Reading data for Y axis */
		a_data_u8r[2] = BMM150_GET_BITSLICE(a_data_u8r[2],
				BMM150_DATAY_LSB_VALUEY);
		raw_dataXYZ.raw_dataY = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[3])) <<
					SHIFT_LEFT_5_POSITION) | a_data_u8r[2]);

		/* Reading data for Z axis */
		a_data_u8r[4] = BMM150_GET_BITSLICE(a_data_u8r[4],
				BMM150_DATAZ_LSB_VALUEZ);
		raw_dataXYZ.raw_dataZ = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[5])) <<
					SHIFT_LEFT_7_POSITION) | a_data_u8r[4]);

		/* Reading data for Resistance*/
		a_data_u8r[6] = BMM150_GET_BITSLICE(a_data_u8r[6],
				BMM150_R_LSB_VALUE);
		raw_dataXYZ.raw_dataR = (BMM150_U16)((((BMM150_U16)
						a_data_u8r[7]) <<
					SHIFT_LEFT_6_POSITION) | a_data_u8r[6]);

		/* Compensation for X axis */
		mdata->datax = bmm150api_compensate_X_float(raw_dataXYZ.raw_dataX,
				raw_dataXYZ.raw_dataR);

		/* Compensation for Y axis */
		mdata->datay = bmm150api_compensate_Y_float(raw_dataXYZ.raw_dataY,
				raw_dataXYZ.raw_dataR);

		/* Compensation for Z axis */
		mdata->dataz = bmm150api_compensate_Z_float(raw_dataXYZ.raw_dataZ,
				raw_dataXYZ.raw_dataR);

		/* Output raw resistance value */
		mdata->resistance = raw_dataXYZ.raw_dataR;
	}
	return comres;
}
#endif
#if 0
static BMM150_RETURN_FUNCTION_TYPE bmm150api_read_register(unsigned char addr,
		unsigned char *data, unsigned char len)
{
	BMM150_RETURN_FUNCTION_TYPE comres;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres += p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			addr, data, len);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_write_register(unsigned char addr,
		unsigned char *data, unsigned char len)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_WRITE_FUNC(p_bmm150->dev_addr,
			addr, data, len);
	}
	return comres;
}
#endif
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_selftest(unsigned char selftest)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr, BMM150_CNTL_S_TEST__REG,
				&v_data1_u8r, 1);
		v_data1_u8r = BMM150_SET_BITSLICE(
				v_data1_u8r, BMM150_CNTL_S_TEST, selftest);
		comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
				p_bmm150->dev_addr, BMM150_CNTL_S_TEST__REG,
				&v_data1_u8r, 1);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_self_test_XYZ(
		unsigned char *self_testxyz)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char a_data_u8r[5], v_result_u8r = 0x00;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr, BMM150_DATAX_LSB_TESTX__REG,
				a_data_u8r, 5);

		v_result_u8r = BMM150_GET_BITSLICE(a_data_u8r[4],
				BMM150_DATAZ_LSB_TESTZ);

		v_result_u8r = (v_result_u8r << 1);
		v_result_u8r = (v_result_u8r | BMM150_GET_BITSLICE(
					a_data_u8r[2], BMM150_DATAY_LSB_TESTY));

		v_result_u8r = (v_result_u8r << 1);
		v_result_u8r = (v_result_u8r | BMM150_GET_BITSLICE(
					a_data_u8r[0], BMM150_DATAX_LSB_TESTX));

		*self_testxyz = v_result_u8r;
	}
	return comres;
}
#if 0
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_spi3(unsigned char value)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_POWER_CNTL_SPI3_EN__REG, &v_data1_u8r, 1);
		v_data1_u8r = BMM150_SET_BITSLICE(v_data1_u8r,
			BMM150_POWER_CNTL_SPI3_EN, value);
		comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(p_bmm150->dev_addr,
			BMM150_POWER_CNTL_SPI3_EN__REG, &v_data1_u8r, 1);
	}
	return comres;
}
#endif
static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_datarate(unsigned char data_rate)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_CNTL_DR__REG,
				&v_data1_u8r, 1);
		v_data1_u8r = BMM150_SET_BITSLICE(v_data1_u8r,
				BMM150_CNTL_DR, data_rate);
		comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
				p_bmm150->dev_addr,
				BMM150_CNTL_DR__REG,
				&v_data1_u8r, 1);
	}
	return comres;
}
#if 0
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_datarate(unsigned char *data_rate)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_CNTL_DR__REG,
				&v_data_u8r, 1);
		*data_rate = BMM150_GET_BITSLICE(v_data_u8r,
				BMM150_CNTL_DR);
	}
	return comres;
}
#endif
static BMM150_RETURN_FUNCTION_TYPE bmm150api_perform_advanced_selftest(
		BMM150_S16 *diff_z)
{
	BMM150_RETURN_FUNCTION_TYPE comres;
	BMM150_S16 result_positive, result_negative;
	struct bmm150api_mdata_s32 mdata;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		/* set sleep mode to prepare for forced measurement.
		 * If sensor is off, this will turn it on
		 * and respect needed delays. */
		comres = bmm150api_set_functional_state(BMM150_SLEEP_MODE);

		/* set normal accuracy mode */
		comres |= bmm150api_set_repetitions_Z(BMM150_LOWPOWER_REPZ);
		/* 14 repetitions Z in normal accuracy mode */

		/* disable X, Y channel */
		comres |= bmm150api_set_control_measurement_x(
				BMM150_CHANNEL_DISABLE);
		comres |= bmm150api_set_control_measurement_y(
				BMM150_CHANNEL_DISABLE);

		/* enable positive current and force a
		 * measurement with positive field */
		comres |= bmm150api_set_adv_selftest(
				BMM150_ADVANCED_SELFTEST_POSITIVE);
		comres |= bmm150api_set_functional_state(BMM150_FORCED_MODE);
		/* wait for measurement to complete */
		p_bmm150->delay_msec(4);

		/* read result from positive field measurement */
		comres |= bmm150api_read_mdataXYZ_s32(&mdata);
		result_positive = mdata.dataz;

		/* enable negative current and force a
		 * measurement with negative field */
		comres |= bmm150api_set_adv_selftest(
				BMM150_ADVANCED_SELFTEST_NEGATIVE);
		comres |= bmm150api_set_functional_state(BMM150_FORCED_MODE);
		p_bmm150->delay_msec(4); /* wait for measurement to complete */

		/* read result from negative field measurement */
		comres |= bmm150api_read_mdataXYZ_s32(&mdata);
		result_negative = mdata.dataz;

		/* turn off self test current */
		comres |= bmm150api_set_adv_selftest(
				BMM150_ADVANCED_SELFTEST_OFF);

		/* enable X, Y channel */
		comres |= bmm150api_set_control_measurement_x(
				BMM150_CHANNEL_ENABLE);
		comres |= bmm150api_set_control_measurement_y(
				BMM150_CHANNEL_ENABLE);

		/* write out difference in positive and negative field.
		 * This should be ~ 200 mT = 3200 LSB */
		*diff_z = (result_positive - result_negative);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_init_trim_registers(void)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char a_data_u8r[2];

	comres = p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_X1, (unsigned char *)&p_bmm150->dig_x1, 1);
	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_Y1, (unsigned char *)&p_bmm150->dig_y1, 1);
	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_X2, (unsigned char *)&p_bmm150->dig_x2, 1);
	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_Y2, (unsigned char *)&p_bmm150->dig_y2, 1);
	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_XY1, (unsigned char *)&p_bmm150->dig_xy1, 1);
	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_XY2, (unsigned char *)&p_bmm150->dig_xy2, 1);

	/* shorts can not be recasted into (unsigned char*)
	 * due to possible mixup between trim data
	 * arrangement and memory arrangement */

	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_Z1_LSB, a_data_u8r, 2);
	p_bmm150->dig_z1 = (BMM150_U16)((((BMM150_U16)((unsigned char)
						a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);

	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_Z2_LSB, a_data_u8r, 2);
	p_bmm150->dig_z2 = (BMM150_S16)((((BMM150_S16)(
						(signed char)a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);

	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_Z3_LSB, a_data_u8r, 2);
	p_bmm150->dig_z3 = (BMM150_S16)((((BMM150_S16)(
						(signed char)a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);

	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_Z4_LSB, a_data_u8r, 2);
	p_bmm150->dig_z4 = (BMM150_S16)((((BMM150_S16)(
						(signed char)a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);

	comres |= p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_DIG_XYZ1_LSB, a_data_u8r, 2);
	a_data_u8r[1] = BMM150_GET_BITSLICE(a_data_u8r[1], BMM150_DIG_XYZ1_MSB);
	p_bmm150->dig_xyz1 = (BMM150_U16)((((BMM150_U16)
					((unsigned char)a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_adv_selftest(unsigned char adv_selftest)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		switch (adv_selftest) {
		case BMM150_ADVANCED_SELFTEST_OFF:
			comres = p_bmm150->BMM150_BUS_READ_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMM150_SET_BITSLICE(
					v_data1_u8r,
					BMM150_CNTL_ADV_ST,
					BMM150_ADVANCED_SELFTEST_OFF);
			comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			break;
		case BMM150_ADVANCED_SELFTEST_POSITIVE:
			comres = p_bmm150->BMM150_BUS_READ_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMM150_SET_BITSLICE(
					v_data1_u8r,
					BMM150_CNTL_ADV_ST,
					BMM150_ADVANCED_SELFTEST_POSITIVE);
			comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			break;
		case BMM150_ADVANCED_SELFTEST_NEGATIVE:
			comres = p_bmm150->BMM150_BUS_READ_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMM150_SET_BITSLICE(
					v_data1_u8r,
					BMM150_CNTL_ADV_ST,
					BMM150_ADVANCED_SELFTEST_NEGATIVE);
			comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
					p_bmm150->dev_addr,
					BMM150_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			break;
		default:
			break;
		}
	}
	return comres;
}
#if 0
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_adv_selftest(unsigned char *adv_selftest)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
			BMM150_CNTL_ADV_ST__REG, &v_data_u8r, 1);
		*adv_selftest = BMM150_GET_BITSLICE(v_data_u8r,
			BMM150_CNTL_ADV_ST);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_presetmode(
	unsigned char *mode)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char data_rate = 0;
	unsigned char repetitionsxy = 0;
	unsigned char repetitionsz = 0;

	/* Get the current data rate */
	comres = bmm150api_get_datarate(&data_rate);
	/* Get the preset number of XY Repetitions */
	comres |= bmm150api_get_repetitions_XY(&repetitionsxy);
	/* Get the preset number of Z Repetitions */
	comres |= bmm150api_get_repetitions_Z(&repetitionsz);
	if ((data_rate == BMM150_LOWPOWER_DR) && (
		repetitionsxy == BMM150_LOWPOWER_REPXY) && (
		repetitionsz == BMM150_LOWPOWER_REPZ)) {
		*mode = BMM150_PRESETMODE_LOWPOWER;
	} else {
		if ((data_rate == BMM150_REGULAR_DR) && (
			repetitionsxy == BMM150_REGULAR_REPXY) && (
			repetitionsz == BMM150_REGULAR_REPZ)) {
			*mode = BMM150_PRESETMODE_REGULAR;
		} else {
			if ((data_rate == BMM150_HIGHACCURACY_DR) && (
				repetitionsxy == BMM150_HIGHACCURACY_REPXY) && (
				repetitionsz == BMM150_HIGHACCURACY_REPZ)) {
					*mode = BMM150_PRESETMODE_HIGHACCURACY;
			} else {
				if ((data_rate == BMM150_ENHANCED_DR) && (
				repetitionsxy == BMM150_ENHANCED_REPXY) && (
				repetitionsz == BMM150_ENHANCED_REPZ)) {
					*mode = BMM150_PRESETMODE_ENHANCED;
				} else {
					*mode = E_BMM150_UNDEFINED_MODE;
				}
			}
		}
	}
	return comres;
}
#endif
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_powermode(unsigned char *mode)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_POWER_CNTL_PCB__REG,
				&v_data_u8r, 1);
		*mode = BMM150_GET_BITSLICE(v_data_u8r,
				BMM150_POWER_CNTL_PCB);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_powermode(unsigned char mode)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_POWER_CNTL_PCB__REG,
				&v_data_u8r, 1);
		v_data_u8r = BMM150_SET_BITSLICE(v_data_u8r,
				BMM150_POWER_CNTL_PCB, mode);
		comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
				p_bmm150->dev_addr,
				BMM150_POWER_CNTL_PCB__REG,
				&v_data_u8r, 1);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_repetitions_XY(
		unsigned char *no_repetitions_xy)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_NO_REPETITIONS_XY,
				&v_data_u8r, 1);
		*no_repetitions_xy = v_data_u8r;
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_repetitions_XY(
		unsigned char no_repetitions_xy)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		v_data_u8r = no_repetitions_xy;
		comres = p_bmm150->BMM150_BUS_WRITE_FUNC(
				p_bmm150->dev_addr,
				BMM150_NO_REPETITIONS_XY,
				&v_data_u8r, 1);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_repetitions_Z(
		unsigned char *no_repetitions_z)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_NO_REPETITIONS_Z,
				&v_data_u8r, 1);
		*no_repetitions_z = v_data_u8r;
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_repetitions_Z(
		unsigned char no_repetitions_z)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		v_data_u8r = no_repetitions_z;
		comres = p_bmm150->BMM150_BUS_WRITE_FUNC(p_bmm150->dev_addr,
				BMM150_NO_REPETITIONS_Z, &v_data_u8r, 1);
	}
	return comres;
}

static BMM150_S16 bmm150api_compensate_X(BMM150_S16 mdata_x, BMM150_U16 data_R)
{
	BMM150_S16 inter_retval;

	if (mdata_x != BMM150_FLIP_OVERFLOW_ADCVAL) {   /* no overflow */
		inter_retval = ((BMM150_S16)(((BMM150_U16)
			((((BMM150_S32)p_bmm150->dig_xyz1) << 14) /
			(data_R != 0 ? data_R : p_bmm150->dig_xyz1))) -
			((BMM150_U16)0x4000)));
		inter_retval = ((BMM150_S16)((((BMM150_S32)mdata_x) *
			((((((((BMM150_S32)p_bmm150->dig_xy2) *
			((((BMM150_S32)inter_retval) *
			((BMM150_S32)inter_retval)) >> 7)) +
			(((BMM150_S32)inter_retval) *
			((BMM150_S32)(((BMM150_S16)p_bmm150->dig_xy1)
			<< 7)))) >> 9) +
			((BMM150_S32)0x100000)) *
			((BMM150_S32)(((BMM150_S16)p_bmm150->dig_x2) +
			((BMM150_S16)0xA0)))) >> 12)) >> 13)) +
			(((BMM150_S16)p_bmm150->dig_x1) << 3);
	} else {
		/* overflow */
		inter_retval = BMM150_OVERFLOW_OUTPUT;
	}
	return inter_retval;
}

static BMM150_S32 bmm150api_compensate_X_s32 (BMM150_S16 mdata_x, BMM150_U16 data_R)
{
	BMM150_S32 retval;

	retval = bmm150api_compensate_X(mdata_x, data_R);
	if (retval == (BMM150_S32)BMM150_OVERFLOW_OUTPUT)
		retval = BMM150_OVERFLOW_OUTPUT_S32;
	return retval;
}

#ifdef ENABLE_FLOAT
static float bmm150api_compensate_X_float (BMM150_S16 mdata_x, BMM150_U16 data_R)
{
	float inter_retval;

	if (mdata_x != BMM150_FLIP_OVERFLOW_ADCVAL) {	/* no overflow */
		if (data_R != 0) {
			inter_retval = ((((float)p_bmm150->dig_xyz1)*16384.0f
				/data_R)-16384.0f);
		} else {
			inter_retval = 0;
		}
		inter_retval = (((mdata_x * ((((((float)p_bmm150->dig_xy2) *
			(inter_retval*inter_retval / 268435456.0f) +
			inter_retval*((float)p_bmm150->dig_xy1)/16384.0f))
			+ 256.0f) *	(((float)p_bmm150->dig_x2) + 160.0f)))
			/ 8192.0f) + (((float)p_bmm150->dig_x1) * 8.0f))/16.0f;
	} else {
		inter_retval = BMM150_OVERFLOW_OUTPUT_FLOAT;
	}
	return inter_retval;
}
#endif

static BMM150_S16 bmm150api_compensate_Y(BMM150_S16 mdata_y, BMM150_U16 data_R)
{
	BMM150_S16 inter_retval;

	if (mdata_y != BMM150_FLIP_OVERFLOW_ADCVAL) {  /* no overflow */
		inter_retval = ((BMM150_S16)(((BMM150_U16)(((
			(BMM150_S32)p_bmm150->dig_xyz1) << 14) /
			(data_R != 0 ?
			data_R : p_bmm150->dig_xyz1))) -
			((BMM150_U16)0x4000)));
		inter_retval = ((BMM150_S16)((((BMM150_S32)mdata_y) *
			((((((((BMM150_S32)
			p_bmm150->dig_xy2) *
			((((BMM150_S32) inter_retval) *
			((BMM150_S32)inter_retval)) >> 7)) +
			(((BMM150_S32)inter_retval) *
			((BMM150_S32)(((BMM150_S16)
			p_bmm150->dig_xy1) << 7)))) >> 9) +
			((BMM150_S32)0x100000)) *
			((BMM150_S32)(((BMM150_S16)p_bmm150->dig_y2)
			+ ((BMM150_S16)0xA0))))
			>> 12)) >> 13)) +
			(((BMM150_S16)p_bmm150->dig_y1) << 3);
	} else {
		/* overflow */
			inter_retval = BMM150_OVERFLOW_OUTPUT;
	}
	return inter_retval;
}

static BMM150_S32 bmm150api_compensate_Y_s32 (BMM150_S16 mdata_y, BMM150_U16 data_R)
{
	BMM150_S32 retval;

	retval = bmm150api_compensate_Y(mdata_y, data_R);
	if (retval == BMM150_OVERFLOW_OUTPUT)
		retval = BMM150_OVERFLOW_OUTPUT_S32;
	return retval;
}

#ifdef ENABLE_FLOAT
static float bmm150api_compensate_Y_float(BMM150_S16 mdata_y, BMM150_U16 data_R)
{
	float inter_retval;

	if (mdata_y != BMM150_FLIP_OVERFLOW_ADCVAL) { /* no overflow */
		if (data_R != 0) {
			inter_retval = ((((float)p_bmm150->dig_xyz1)*16384.0f
			/data_R)-16384.0f);
		} else {
			inter_retval = 0;
		}
		inter_retval = (((mdata_y * ((((((float)p_bmm150->dig_xy2) *
			(inter_retval*inter_retval / 268435456.0f) +
			inter_retval * ((float)p_bmm150->dig_xy1)/16384.0f)) +
			256.0f) * (((float)p_bmm150->dig_y2) + 160.0f)))
			/ 8192.0f) + (((float)p_bmm150->dig_y1) * 8.0f))/16.0f;
	} else {
		/* overflow, set output to 0.0f */
		inter_retval = BMM150_OVERFLOW_OUTPUT_FLOAT;
	}
	return inter_retval;
}
#endif
#if 0
static BMM150_S16 bmm150api_compensate_Z(BMM150_S16 mdata_z, BMM150_U16 data_R)
{
	BMM150_S32 retval;

	if (mdata_z != BMM150_HALL_OVERFLOW_ADCVAL) {	/* no overflow */
		retval = (((((BMM150_S32)(mdata_z - p_bmm150->dig_z4)) << 15) -
			((((BMM150_S32)p_bmm150->dig_z3) *
			((BMM150_S32)(((BMM150_S16)data_R) -
			((BMM150_S16)
			p_bmm150->dig_xyz1))))>>2)) /
			(p_bmm150->dig_z2 +
			((BMM150_S16)(((((BMM150_S32)
			p_bmm150->dig_z1) *
			((((BMM150_S16)data_R) << 1)))+
			(1<<15))>>16))));
		/* saturate result to +/- 2 mT */
		if (retval > BMM150_POSITIVE_SATURATION_Z) {
			retval = BMM150_POSITIVE_SATURATION_Z;
		} else {
			if (retval < BMM150_NEGATIVE_SATURATION_Z)
				retval = BMM150_NEGATIVE_SATURATION_Z;
		}
	} else {
		/* overflow */
		retval = BMM150_OVERFLOW_OUTPUT;
	}
	return (BMM150_S16)retval;
}
#endif
static BMM150_S32 bmm150api_compensate_Z_s32(BMM150_S16 mdata_z, BMM150_U16 data_R)
{
	BMM150_S32 retval;

	if (mdata_z != BMM150_HALL_OVERFLOW_ADCVAL) {
		retval = (((((BMM150_S32)(mdata_z - p_bmm150->dig_z4)) << 15) -
			((((BMM150_S32)p_bmm150->dig_z3) *
			((BMM150_S32)(((BMM150_S16)data_R) -
			((BMM150_S16)p_bmm150->dig_xyz1))))>>2)) /
			(p_bmm150->dig_z2 +
			((BMM150_S16)(((((BMM150_S32)p_bmm150->dig_z1) *
			((((BMM150_S16)data_R) << 1)))+(1<<15))>>16))));
	} else {
		retval = BMM150_OVERFLOW_OUTPUT_S32;
	}
	return retval;
}

#ifdef ENABLE_FLOAT
static float bmm150api_compensate_Z_float (BMM150_S16 mdata_z, BMM150_U16 data_R)
{
	float inter_retval;

	if (mdata_z != BMM150_HALL_OVERFLOW_ADCVAL) { /* no overflow */
		inter_retval = ((((((float)mdata_z)-((float)p_bmm150->dig_z4))*
		131072.0f)-(((float)p_bmm150->dig_z3)*(((float)data_R)-
		((float)p_bmm150->dig_xyz1))))/((((float)p_bmm150->dig_z2)+
		((float)p_bmm150->dig_z1)*((float)data_R)/32768.0)*4.0))/16.0;
	} else {
		/* overflow, set output to 0.0f */
		inter_retval = BMM150_OVERFLOW_OUTPUT_FLOAT;
	}
	return inter_retval;
}
#endif

static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_control_measurement_x(
		unsigned char enable_disable)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_SENS_CNTL_CHANNELX__REG,
				&v_data1_u8r, 1);
		v_data1_u8r = BMM150_SET_BITSLICE(v_data1_u8r,
				BMM150_SENS_CNTL_CHANNELX,
				enable_disable);
		comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
				p_bmm150->dev_addr,
				BMM150_SENS_CNTL_CHANNELX__REG,
				&v_data1_u8r, 1);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_set_control_measurement_y(
		unsigned char enable_disable)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_SENS_CNTL_CHANNELY__REG,
				&v_data1_u8r, 1);
		v_data1_u8r = BMM150_SET_BITSLICE(
				v_data1_u8r,
				BMM150_SENS_CNTL_CHANNELY,
				enable_disable);
		comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
				p_bmm150->dev_addr,
				BMM150_SENS_CNTL_CHANNELY__REG,
				&v_data1_u8r, 1);
	}
	return comres;
}

static BMM150_RETURN_FUNCTION_TYPE bmm150api_soft_reset(void)
{
	BMM150_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		v_data_u8r = BMM150_ON;

		comres = p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_POWER_CNTL_SRST7__REG,
				&v_data_u8r, 1);
		v_data_u8r = BMM150_SET_BITSLICE(v_data_u8r,
				BMM150_POWER_CNTL_SRST7,
				BMM150_SOFT_RESET7_ON);
		comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
				p_bmm150->dev_addr,
				BMM150_POWER_CNTL_SRST7__REG, &v_data_u8r, 1);

		comres |= p_bmm150->BMM150_BUS_READ_FUNC(
				p_bmm150->dev_addr,
				BMM150_POWER_CNTL_SRST1__REG,
				&v_data_u8r, 1);
		v_data_u8r = BMM150_SET_BITSLICE(v_data_u8r,
				BMM150_POWER_CNTL_SRST1,
				BMM150_SOFT_RESET1_ON);
		comres |= p_bmm150->BMM150_BUS_WRITE_FUNC(
				p_bmm150->dev_addr,
				BMM150_POWER_CNTL_SRST1__REG,
				&v_data_u8r, 1);

		p_bmm150->delay_msec(BMM150_DELAY_SOFTRESET);
	}
	return comres;
}
#if 0
static BMM150_RETURN_FUNCTION_TYPE bmm150api_get_raw_xyz(struct bmm150api_mdata *mdata)
{
	BMM150_RETURN_FUNCTION_TYPE comres;
	unsigned char a_data_u8r[6];

	if (p_bmm150 == BMM150_NULL) {
		comres = E_BMM150_NULL_PTR;
	} else {
		comres = p_bmm150->BMM150_BUS_READ_FUNC(p_bmm150->dev_addr,
				BMM150_DATAX_LSB, a_data_u8r, 6);

		a_data_u8r[0] = BMM150_GET_BITSLICE(a_data_u8r[0],
				BMM150_DATAX_LSB_VALUEX);
		mdata->datax = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[1]))
					<< SHIFT_LEFT_5_POSITION)
				| a_data_u8r[0]);

		a_data_u8r[2] = BMM150_GET_BITSLICE(a_data_u8r[2],
				BMM150_DATAY_LSB_VALUEY);
		mdata->datay = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[3]))
					<< SHIFT_LEFT_5_POSITION)
				| a_data_u8r[2]);

		a_data_u8r[4] = BMM150_GET_BITSLICE(a_data_u8r[4],
				BMM150_DATAZ_LSB_VALUEZ);
		mdata->dataz = (BMM150_S16)((((BMM150_S16)
						((signed char)a_data_u8r[5]))
					<< SHIFT_LEFT_7_POSITION)
				| a_data_u8r[4]);
	}
	return comres;
}
#endif
/*----------------------------------------------------------------------------*/
/* End of API Section */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#define DEBUG 1
#define BMM150_DEV_NAME         "bmm150"
/*----------------------------------------------------------------------------*/

#define SENSOR_CHIP_ID_BMM	(0x32)

#define BMM150_DEFAULT_DELAY	100
#define BMM150_BUFSIZE  0x20


static struct i2c_client *this_client;

/* calibration msensor and orientation data */
static int sensor_data[CALIBRATION_DATA_SIZE];
#if defined(BMC156_M4G) || defined(BMC156_VRV)
static int m4g_data[CALIBRATION_DATA_SIZE];
#endif /* BMC156_M4G || BMC156_VRV */
#if defined(BMC156_VLA)
static int vla_data[CALIBRATION_DATA_SIZE];
#endif /* BMC156_VLA */

#if defined(BMC156_VG)
static int vg_data[CALIBRATION_DATA_SIZE];
#endif /* BMC156_VG */

static struct mutex sensor_data_mutex;
static DECLARE_WAIT_QUEUE_HEAD(uplink_event_flag_wq);

static int bmm150d_delay = BMM150_DEFAULT_DELAY;
#ifdef BMC156_M4G
static int m4g_delay = BMM150_DEFAULT_DELAY;
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
static int vrv_delay = BMM150_DEFAULT_DELAY;
#endif /* BMC156_VRV */
#ifdef BMC156_VLA
static int vla_delay = BMM150_DEFAULT_DELAY;
#endif /* BMC156_VRV */

#ifdef BMC156_VG
static int vg_delay = BMM150_DEFAULT_DELAY;
#endif /* BMC156_VG */

static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);
#ifdef BMC156_M4G
static atomic_t g_flag = ATOMIC_INIT(0);
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
static atomic_t vrv_flag = ATOMIC_INIT(0);
#endif /* BMC156_VRV */
#ifdef BMC156_VLA
static atomic_t vla_flag = ATOMIC_INIT(0);
#endif /* BMC156_VLA */
#ifdef BMC156_VG
static atomic_t vg_flag = ATOMIC_INIT(0);
#endif /* BMC156_VG */

#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
static atomic_t driver_suspend_flag = ATOMIC_INIT(0);
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */

static struct mutex uplink_event_flag_mutex;
/* uplink event flag */
static u32 uplink_event_flag;
/* uplink event flag bitmap */
enum {
	/* active */
	BMMDRV_ULEVT_FLAG_O_ACTIVE = 0x0001,
	BMMDRV_ULEVT_FLAG_M_ACTIVE = 0x0002,
	BMMDRV_ULEVT_FLAG_G_ACTIVE = 0x0004,
	BMMDRV_ULEVT_FLAG_VRV_ACTIVE = 0x0008,/* Virtual Rotation Vector */
	BMMDRV_ULEVT_FLAG_FLIP_ACTIVE = 0x0010,
	BMMDRV_ULEVT_FLAG_VLA_ACTIVE = 0x0020,/* Virtual Linear Accelerometer */
	BMMDRV_ULEVT_FLAG_VG_ACTIVE = 0x0040,/* Virtual Gravity */

	/* delay */
	BMMDRV_ULEVT_FLAG_O_DELAY = 0x0100,
	BMMDRV_ULEVT_FLAG_M_DELAY = 0x0200,
	BMMDRV_ULEVT_FLAG_G_DELAY = 0x0400,
	BMMDRV_ULEVT_FLAG_VRV_DELAY = 0x0800,
	BMMDRV_ULEVT_FLAG_FLIP_DELAY = 0x1000,
	BMMDRV_ULEVT_FLAG_VLA_DELAY = 0x2000,
	BMMDRV_ULEVT_FLAG_VG_DELAY = 0x4000,

	/* all */
	BMMDRV_ULEVT_FLAG_ALL = 0xffff
};

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id bmm150_i2c_id[] = {{BMM150_DEV_NAME, 0}, {} };
/*static struct i2c_board_info __initdata bmm150_i2c_info = {I2C_BOARD_INFO(BMM150_DEV_NAME, BMM150_I2C_ADDR)};*/

/* Maintain  cust info here */
struct mag_hw mag_cust;
static struct mag_hw *hw = &mag_cust;

/* For  driver get cust info */
struct mag_hw *get_cust_mag_hw(void)
{
	return &mag_cust;
}
/*----------------------------------------------------------------------------*/

enum MMC_TRC {
	MMC_FUN_DEBUG = 0x01,
	MMC_DATA_DEBUG = 0X02,
	MMC_HWM_DEBUG = 0X04,
	MMC_CTR_DEBUG = 0X08,
	MMC_I2C_DEBUG = 0x10,
};

/*----------------------------------------------------------------------------*/
struct bmm150_i2c_data {
	struct i2c_client *client;
	struct mag_hw *hw;
	atomic_t layout;
	atomic_t trace;
	struct hwmsen_convert cvt;
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	struct work_struct irq_work;
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */

	struct bmm150api device;

	u8 op_mode;
	u8 odr;
	u8 rept_xy;
	u8 rept_z;
	s16 result_test;
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	int SCP_init_done;
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */

#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_drv;
#endif
};
/*----------------------------------------------------------------------------*/
static void bmm150_restore_hw_cfg(struct i2c_client *client);
#ifdef USE_NEW_SENSOR_ARCH
static int bmm150_local_init(void);
static int bmm150_remove(void);
static int bmm150_init_flag = -1;
static struct mag_init_info bmm150_init_info = {
		 .name = "bmm150",
		 .init = bmm150_local_init,
		 .uninit = bmm150_remove,
};
#else
static int bmm150_probe(struct platform_device *pdev);
static int bmm150_remove(struct platform_device *pdev);
#endif

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
static int msensor_setup_irq(void);
static DEFINE_MUTEX(msensor_scp_en_mutex);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
int BMM150_SCP_SetPowerMode(bool enable)
{
	static bool msensor_scp_en_status;
	static unsigned int gsensor_scp_en_map;
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	mutex_lock(&msensor_scp_en_mutex);

	if (true == enable)
		gsensor_scp_en_map |= (1 << ID_MAGNETIC);
	else
		gsensor_scp_en_map &= ~(1 << ID_MAGNETIC);

	if (0 == gsensor_scp_en_map)
		enable = false;
	else
		enable = true;

	if (msensor_scp_en_status != enable) {
		msensor_scp_en_status = enable;

		req.activate_req.sensorType = ID_MAGNETIC;
		req.activate_req.action = SENSOR_HUB_ACTIVATE;
		req.activate_req.enable = enable;
		len = sizeof(req.activate_req);
		err = SCP_sensorHub_req_send(&req, &len, 1);
		if (err)
			MSE_ERR("SCP_sensorHub_req_send fail\n");
	}

	mutex_unlock(&msensor_scp_en_mutex);

	return err;
}
#endif/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
/*----------------------------------------------------------------------------*/
static void bmm150_power(struct mag_hw *hw, unsigned int on)
{
	/*static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO) {
		MSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on) {
			MSE_LOG("ignore power control: %d\n", on);
		} else if(on) {
			if(!hwPowerOn(hw->power_id, hw->power_vol, "bmm150")) {
				MSE_ERR( "power on fails!!\n");
			}
		} else {
			if(!hwPowerDown(hw->power_id, "bmm150")) {
				MSE_ERR( "power off fail!!\n");
			}
		}
	}
	power_on = on;*/
}

/* Daemon application save the data */
static int ECS_SaveData(int buf[CALIBRATION_DATA_SIZE])
{
#if DEBUG
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);
#endif

	mutex_lock(&sensor_data_mutex);
	switch (buf[0]) {
	case 2:	/* SENSOR_HANDLE_MAGNETIC_FIELD */
		memcpy(sensor_data+4, buf+1, 4*sizeof(int));
		break;
	case 3:	/* SENSOR_HANDLE_ORIENTATION */
		memcpy(sensor_data+8, buf+1, 4*sizeof(int));
		break;
#ifdef BMC156_M4G
	case 4:	/* SENSOR_HANDLE_GYROSCOPE */
		memcpy(m4g_data, buf+1, 4*sizeof(int));
		break;
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
	case 11:	/* SENSOR_HANDLE_ROTATION_VECTOR */
		memcpy(m4g_data+4, buf+1, 4*sizeof(int));
		break;
#endif /* BMC156_VRV */
#ifdef BMC156_VLA
	case 10: /* SENSOR_HANDLE_LINEAR_ACCELERATION */
		memcpy(vla_data, buf+1, 4*sizeof(int));
		break;
#endif /* BMC156_VLA */
#ifdef BMC156_VG
	case 9: /* SENSOR_HANDLE_GRAVITY */
		memcpy(vg_data, buf+1, 4*sizeof(int));
		break;
#endif /* BMC156_VG */
	default:
		break;
	}
	mutex_unlock(&sensor_data_mutex);

#if DEBUG
	if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
		MSE_LOG("Get daemon data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			sensor_data[0], sensor_data[1], sensor_data[2], sensor_data[3],
			sensor_data[4], sensor_data[5], sensor_data[6], sensor_data[7],
			sensor_data[8], sensor_data[9], sensor_data[10], sensor_data[11]);
#if defined(BMC156_M4G) || defined(BMC156_VRV)
		MSE_LOG("Get m4g data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			m4g_data[0], m4g_data[1], m4g_data[2], m4g_data[3],
			m4g_data[4], m4g_data[5], m4g_data[6], m4g_data[7],
			m4g_data[8], m4g_data[9], m4g_data[10], m4g_data[11]);
#endif /* BMC156_M4G || BMC156_VRV */
#if defined(BMC156_VLA)
		MSE_LOG("Get vla data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			vla_data[0], vla_data[1], vla_data[2], vla_data[3],
			vla_data[4], vla_data[5], vla_data[6], vla_data[7],
			vla_data[8], vla_data[9], vla_data[10], vla_data[11]);
#endif /* BMC156_VLA */

#if defined(BMC156_VG)
		MSE_LOG("Get vg data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			vg_data[0], vg_data[1], vg_data[2], vg_data[3],
			vg_data[4], vg_data[5], vg_data[6], vg_data[7],
			vg_data[8], vg_data[9], vg_data[10], vg_data[11]);
#endif /* BMC156_VG */
	}
#endif

	return 0;
}
/*----------------------------------------------------------------------------*/
static int ECS_GetRawData(int data[3])
{
	struct bmm150api_mdata_s32 mdata;
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);

	u8 databuf[2] = {BMM150_CONTROL, 0x02};

	bmm150api_read_mdataXYZ_s32(&mdata);
	/* data in uT */
	data[0] = mdata.datax/16;
	data[1] = mdata.datay/16;
	data[2] = mdata.dataz/16;

	/* measure magnetic field for next sample */
	if (obj->op_mode == BMM150_SUSPEND_MODE) {
		/* power on firstly */
		bmm150api_set_powermode(BMM150_ON);
	}
	/* special treat of forced mode
	 * for optimization */
	i2c_master_send(this_client, databuf, 2);
	obj->op_mode = BMM150_SLEEP_MODE;

	return 0;
}
/*----------------------------------------------------------------------------*/
static int bmm150_ReadChipInfo(char *buf, int bufsize)
{
	if ((!buf) || (bufsize <= BMM150_BUFSIZE - 1))
		return -1;
	if (!this_client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "BMC156 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static void bmm150_SetPowerMode(struct i2c_client *client, bool enable)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(client);

	/*u8 power_mode;*/

	if (enable == false) {
		if (bmm150api_set_functional_state(BMM150_SUSPEND_MODE) != 0) {
			MSE_ERR("fail to suspend sensor");
			return;
		}
		obj->op_mode = BMM150_SUSPEND_MODE;
	} else {
		if (obj->op_mode == BMM150_SUSPEND_MODE)
			obj->op_mode = BMM150_SLEEP_MODE;
		bmm150_restore_hw_cfg(client);
	}
}

/*----------------------------------------------------------------------------*/
/* Driver Attributes Functions Section */
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[BMM150_BUFSIZE];

	bmm150_ReadChipInfo(strbuf, BMM150_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	int sensordata[3];
	char strbuf[BMM150_BUFSIZE];

	ECS_GetRawData(sensordata);
	sprintf(strbuf, "%d %d %d\n", sensordata[0], sensordata[1], sensordata[2]);
	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	int tmp[3];
	char strbuf[BMM150_BUFSIZE];

	tmp[0] = sensor_data[0] * CONVERT_O / CONVERT_O_DIV;
	tmp[1] = sensor_data[1] * CONVERT_O / CONVERT_O_DIV;
	tmp[2] = sensor_data[2] * CONVERT_O / CONVERT_O_DIV;
	sprintf(strbuf, "%d, %d, %d\n", tmp[0], tmp[1], tmp[2]);

	return sprintf(buf, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction, atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2], data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	/*struct i2c_client *client = this_client;*/
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);
	int layout = 0;
	int err = 0;

	err = kstrtoint(buf, 10, &layout);
	if (err == 0) {
		atomic_set(&data->layout, layout);
		if (!hwmsen_get_convert(layout, &data->cvt)) {
			MSE_ERR("HWMSEN_GET_CONVERT function error!\r\n");
		} else if (!hwmsen_get_convert(data->hw->direction, &data->cvt)) {
			MSE_ERR("invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		} else {
			MSE_ERR("invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	} else {
		MSE_ERR("invalid format = '%s'\n", buf);
	}

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);

	ssize_t len = 0;

	if (data->hw) {
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n",
			data->hw->i2c_num, data->hw->direction, data->hw->power_id, data->hw->power_vol);
	} else {
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}

	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);

	if (NULL == obj) {
		MSE_ERR("bmm150_i2c_data is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);
	int trace;

	if (NULL == obj) {
		MSE_ERR("bmm150_i2c_data is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		MSE_ERR("invalid content: '%s', length = %d\n", buf, (int)count);

	return count;
}
/*----------------------------------------------------------------------------*/
#define BMM150_AXIS_X          0
#define BMM150_AXIS_Y          1
#define BMM150_AXIS_Z          2
static ssize_t show_cpsdata_value(struct device_driver *ddri, char *buf)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);

	struct bmm150api_mdata_s32 mdata;
	s32 mag[3];

	bmm150api_read_mdataXYZ_s32(&mdata);

	/*remap coordinate*/
	mag[obj->cvt.map[BMM150_AXIS_X]] = obj->cvt.sign[BMM150_AXIS_X]*mdata.datax;
	mag[obj->cvt.map[BMM150_AXIS_Y]] = obj->cvt.sign[BMM150_AXIS_Y]*mdata.datay;
	mag[obj->cvt.map[BMM150_AXIS_Z]] = obj->cvt.sign[BMM150_AXIS_Z]*mdata.dataz;

	return sprintf(buf, "%d %d %d\n", mag[BMM150_AXIS_X], mag[BMM150_AXIS_Y], mag[BMM150_AXIS_Z]);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cpsopmode_value(struct device_driver *ddri, char *buf)
{
	/*struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);*/

	u8 op_mode = 0xff;
	u8 power_mode;

	bmm150api_get_powermode(&power_mode);
	if (power_mode)
		bmm150api_get_functional_state(&op_mode);
	else
		op_mode = BMM150_SUSPEND_MODE;

	MSE_LOG("op_mode: %d", op_mode);

	return sprintf(buf, "%d\n", op_mode);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cpsopmode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);
	long op_mode = -1;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;
	if ((unsigned char)op_mode > 3)
		return -EINVAL;

	if (op_mode == obj->op_mode) {
		/* don't return error here */
		return count;
	}

	if (BMM150_FORCED_MODE == op_mode) {
		u8 databuf[2] = {BMM150_CONTROL, 0x02};

		if (obj->op_mode == BMM150_SUSPEND_MODE) {
			/* power on firstly */
			bmm150api_set_powermode(BMM150_ON);
		}
		/* special treat of forced mode
		 * for optimization */
		i2c_master_send(this_client, databuf, 2);
		obj->op_mode = BMM150_SLEEP_MODE;
	} else {
		bmm150api_set_functional_state((unsigned char)op_mode);
		obj->op_mode = op_mode;
	}

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cpsreptxy_value(struct device_driver *ddri, char *buf)
{
	unsigned char data = 0;
	u8 power_mode;
	int err;

	bmm150api_get_powermode(&power_mode);
	if (power_mode)
		err = bmm150api_get_repetitions_XY(&data);
	else
		err = -EIO;

	if (err)
		return err;

	return sprintf(buf, "%d\n", data);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cpsreptxy_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);

	unsigned long tmp = 0;
	int err;
	u8 data;
	u8 power_mode;

	err = kstrtoul(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	bmm150api_get_powermode(&power_mode);
	if (power_mode) {
		err = bmm150api_set_repetitions_XY(data);
		if (!err)
			/* mdelay(BMM_I2C_WRITE_DELAY_TIME); */
			obj->rept_xy = data;
	} else {
		err = -EIO;
	}

	if (err)
		return err;

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cpsreptz_value(struct device_driver *ddri, char *buf)
{
	unsigned char data = 0;
	u8 power_mode;
	int err;

	bmm150api_get_powermode(&power_mode);
	if (power_mode)
		err = bmm150api_get_repetitions_Z(&data);
	else
		err = -EIO;

	if (err)
		return err;

	return sprintf(buf, "%d\n", data);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cpsreptz_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);
	unsigned long tmp = 0;
	int err;
	u8 data;
	u8 power_mode;

	err = kstrtoul(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	bmm150api_get_powermode(&power_mode);
	if (power_mode) {
		err = bmm150api_set_repetitions_Z(data);
		if (!err) {
			/* mdelay(BMM_I2C_WRITE_DELAY_TIME); */
			obj->rept_z = data;
		}
	} else {
		err = -EIO;
	}

	if (err)
		return err;

	return count;
}

static ssize_t show_test_value(struct device_driver *ddri, char *buf)
{
	struct bmm150_i2c_data *client_data = i2c_get_clientdata(this_client);
	int err;

	err = sprintf(buf, "%d\n", client_data->result_test);
	return err;
}

static ssize_t store_test_value(struct device_driver *ddri, const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct bmm150_i2c_data *client_data = i2c_get_clientdata(this_client);
	u8 dummy;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	/* the following code assumes the work thread is not running */
	if (1 == data) {
		/* self test */
		err = bmm150api_set_functional_state(BMM150_SLEEP_MODE);
		mdelay(3);
		err = bmm150api_set_selftest(1);
		mdelay(3);
		dummy = 0;
		err = bmm150api_get_self_test_XYZ(&dummy);
		client_data->result_test = dummy;
	} else if (2 == data) {
		/* advanced self test */
		err = bmm150api_perform_advanced_selftest(&client_data->result_test);
	} else {
		err = -EINVAL;
	}

	if (!err) {
		bmm150api_soft_reset();
		mdelay(1);
		bmm150_restore_hw_cfg(this_client);
	}

	if (err)
		count = -1;

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[256];

	sprintf(strbuf, "bstd");
	return sprintf(buf, "%s", strbuf);
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,    S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata,  S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(posturedata, S_IRUGO, show_posturedata_value, NULL);
static DRIVER_ATTR(layout,      S_IRUGO | S_IWUSR, show_layout_value, store_layout_value);
static DRIVER_ATTR(status,      S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(trace,       S_IRUGO | S_IWUSR, show_trace_value, store_trace_value);
static DRIVER_ATTR(cpsdata, S_IWUSR | S_IRUGO, show_cpsdata_value, NULL);
static DRIVER_ATTR(cpsopmode,      S_IRUGO | S_IWUSR, show_cpsopmode_value, store_cpsopmode_value);
static DRIVER_ATTR(cpsreptxy,      S_IRUGO | S_IWUSR, show_cpsreptxy_value, store_cpsreptxy_value);
static DRIVER_ATTR(cpsreptz,      S_IRUGO | S_IWUSR, show_cpsreptz_value, store_cpsreptz_value);
static DRIVER_ATTR(test,      S_IRUGO | S_IWUSR, show_test_value, store_test_value);
static DRIVER_ATTR(daemon,      S_IRUGO, show_daemon_name, NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *bmm150_attr_list[] = {
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_posturedata,
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_trace,
	&driver_attr_cpsdata,
	&driver_attr_cpsopmode,
	&driver_attr_cpsreptxy,
	&driver_attr_cpsreptz,
	&driver_attr_test,
	&driver_attr_daemon,
};
/*----------------------------------------------------------------------------*/
static int bmm150_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(bmm150_attr_list)/sizeof(bmm150_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, bmm150_attr_list[idx]);
		if (err) {
			MSE_ERR("driver_create_file (%s) = %d\n", bmm150_attr_list[idx]->attr.name, err);
			break;
		}
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int bmm150_delete_attr(struct device_driver *driver)
{
	int idx;
	int num = (int)(sizeof(bmm150_attr_list)/sizeof(bmm150_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, bmm150_attr_list[idx]);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int bmm150_open(struct inode *inode, struct file *file)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);
	int ret = -1;

	if (atomic_read(&obj->trace) & MMC_CTR_DEBUG)
		MSE_LOG("Open device node:bmm150\n");
	ret = nonseekable_open(inode, file);

	return ret;
}
/*----------------------------------------------------------------------------*/
static int bmm150_release(struct inode *inode, struct file *file)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);

	if (atomic_read(&obj->trace) & MMC_CTR_DEBUG)
		MSE_LOG("Release device node:bmm150\n");
	return 0;
}
/*----------------------------------------------------------------------------*/
/* !!! add a new definition in linux/sensors_io.h if possible !!! */
#define BMM_IOC_GET_EVENT_FLAG	ECOMPASS_IOC_GET_OPEN_STATUS
#define BMM_IOC_GET_NONBLOCK_EVENT_FLAG _IOR(MSENSOR, 0x38, int)
static long bmm150_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	/* NOTE: In this function the size of "char" should be 1-byte. */
	char buff[BMM150_BUFSIZE];				/* for chip information */

	int value[CALIBRATION_DATA_SIZE];			/* for SET_YPR */
	int status;				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
	int vec[3] = {0};
	struct bmm150_i2c_data *clientdata = i2c_get_clientdata(this_client);
	struct hwm_sensor_data *osensor_data;
	uint32_t enable;

	switch (cmd) {
	case BMM_IOC_GET_EVENT_FLAG:	/* used by daemon only */
		/* block if no event updated */
		wait_event_interruptible(uplink_event_flag_wq, (uplink_event_flag != 0));
		mutex_lock(&uplink_event_flag_mutex);
		status = uplink_event_flag;
		mutex_unlock(&uplink_event_flag_mutex);
		if (copy_to_user(argp, &status, sizeof(status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case BMM_IOC_GET_NONBLOCK_EVENT_FLAG:	/* used by daemon only */
		/* nonblock daemon process */
		/* wait_event_interruptible(uplink_event_flag_wq, (uplink_event_flag != 0)); */
		mutex_lock(&uplink_event_flag_mutex);
		status = uplink_event_flag;
		mutex_unlock(&uplink_event_flag_mutex);
		if (copy_to_user(argp, &status, sizeof(status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECOMPASS_IOC_GET_DELAY:			/* used by daemon */
		if (copy_to_user(argp, &bmm150d_delay, sizeof(bmm150d_delay))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_M_DELAY) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_M_DELAY;
		else if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_O_DELAY) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_O_DELAY;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;

	case ECOMPASS_IOC_SET_YPR:				/* used by daemon */
		if (argp == NULL) {
			MSE_ERR("invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(value, argp, sizeof(value))) {
			MSE_ERR("copy_from_user failed.");
			return -EFAULT;
		}
		ECS_SaveData(value);
		break;

	case ECOMPASS_IOC_GET_MFLAG:		/* used by daemon */
		sensor_status = atomic_read(&m_flag);
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
		if ((sensor_status == 1) && (atomic_read(&driver_suspend_flag) == 1))
			/* de-active m-channel when driver suspend regardless of m_flag*/
			sensor_status = 0;

#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */
		if (copy_to_user(argp, &sensor_status, sizeof(sensor_status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_M_ACTIVE) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_M_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;

	case ECOMPASS_IOC_GET_OFLAG:		/* used by daemon */
		sensor_status = atomic_read(&o_flag);
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
		if ((sensor_status == 1) && (atomic_read(&driver_suspend_flag) == 1))
			/* de-active m-channel when driver suspend regardless of m_flag*/
			sensor_status = 0;
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */
		if (copy_to_user(argp, &sensor_status, sizeof(sensor_status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_O_ACTIVE) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_O_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;

#ifdef BMC156_M4G
	case ECOMPASS_IOC_GET_GDELAY:			/* used by daemon */
		if (copy_to_user(argp, &m4g_delay, sizeof(m4g_delay))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_G_DELAY) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_G_DELAY;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;

	case ECOMPASS_IOC_GET_GFLAG:		/* used by daemon */
		sensor_status = atomic_read(&g_flag);
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
		if ((sensor_status == 1) && (atomic_read(&driver_suspend_flag) == 1))
			/* de-active g-channel when driver suspend regardless of g_flag*/
			sensor_status = 0;
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */
		if (copy_to_user(argp, &sensor_status, sizeof(sensor_status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_G_ACTIVE) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_G_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;
#endif /* BMC156_M4G */

#ifdef BMC156_VRV
	case ECOMPASS_IOC_GET_VRVDELAY:			/* used by daemon */
		if (copy_to_user(argp, &vrv_delay, sizeof(vrv_delay))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_VRV_DELAY) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_VRV_DELAY;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;

	case ECOMPASS_IOC_GET_VRVFLAG:		/* used by daemon */
		sensor_status = atomic_read(&vrv_flag);
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
		if ((sensor_status == 1) && (atomic_read(&driver_suspend_flag) == 1)) {
			/* de-active vrv-channel when driver suspend regardless of vrv_flag*/
			sensor_status = 0;
		}
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */
		if (copy_to_user(argp, &sensor_status, sizeof(sensor_status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_VRV_ACTIVE) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_VRV_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;
#endif /* BMC156_VRV */

#ifdef BMC156_VLA
	case ECOMPASS_IOC_GET_VLADELAY:		/* used by daemon */
		if (copy_to_user(argp, &vla_delay, sizeof(vla_delay))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_VLA_DELAY) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_VLA_DELAY;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;

	case ECOMPASS_IOC_GET_VLAFLAG:		/* used by daemon */
		sensor_status = atomic_read(&vla_flag);
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
		if ((sensor_status == 1) && (atomic_read(&driver_suspend_flag) == 1))
			/* de-active vla-channel when driver suspend regardless of vla_flag*/
			sensor_status = 0;
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */
		if (copy_to_user(argp, &sensor_status, sizeof(sensor_status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_VLA_ACTIVE) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_VLA_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;
#endif /* BMC156_VLA */

#ifdef BMC156_VG
	case ECOMPASS_IOC_GET_VGDELAY:		/* used by daemon */
		if (copy_to_user(argp, &vg_delay, sizeof(vg_delay))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_VG_DELAY) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_VG_DELAY;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;

	case ECOMPASS_IOC_GET_VGFLAG:		/* used by daemon */
		sensor_status = atomic_read(&vg_flag);
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
		if ((sensor_status == 1) && (atomic_read(&driver_suspend_flag) == 1))
			/* de-active vla-channel when driver suspend regardless of vla_flag*/
			sensor_status = 0;
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */
		if (copy_to_user(argp, &sensor_status, sizeof(sensor_status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		/* clear the flag */
		mutex_lock(&uplink_event_flag_mutex);
		if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_VG_ACTIVE) != 0)
			uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_VG_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;
#endif /* BMC156_VG */
	case MSENSOR_IOCTL_READ_CHIPINFO:		/* reserved */
		if (argp == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}

		bmm150_ReadChipInfo(buff, BMM150_BUFSIZE);
		if (copy_to_user(argp, buff, strlen(buff)+1))
			return -EFAULT;
		break;

	case MSENSOR_IOCTL_READ_SENSORDATA:		/* used by MTK ftm or engineering mode */
		if (argp == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}
		ECS_GetRawData(vec);
		sprintf(buff, "%x %x %x", vec[0], vec[1], vec[2]);
		if (copy_to_user(argp, buff, strlen(buff)+1))
			return -EFAULT;
		break;

	case ECOMPASS_IOC_GET_LAYOUT:		/* used by daemon */
		status = atomic_read(&clientdata->layout);
		if (copy_to_user(argp, &status, sizeof(status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case MSENSOR_IOCTL_SENSOR_ENABLE:		/* used by MTK ftm */
		if (argp == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}
		if (copy_from_user(&enable, argp, sizeof(enable))) {
			MSE_ERR("copy_from_user failed.");
			return -EFAULT;
		}
		MSE_LOG("MSENSOR_IOCTL_SENSOR_ENABLE enable=%d!\r\n", enable);
		if (1 == enable)
			atomic_set(&o_flag, 1);
		else
			atomic_set(&o_flag, 0);

			/* set the flag */
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
		break;

	case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:		/* used by MTK ftm */
		if (argp == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}

		osensor_data = (struct hwm_sensor_data *)buff;
		mutex_lock(&sensor_data_mutex);

		osensor_data->values[0] = sensor_data[8];
		osensor_data->values[1] = sensor_data[9];
		osensor_data->values[2] = sensor_data[10];
		osensor_data->status = sensor_data[11];
		osensor_data->value_divide = CONVERT_O_DIV;

		mutex_unlock(&sensor_data_mutex);

		sprintf(buff, "%x %x %x %x %x", osensor_data->values[0], osensor_data->values[1],
			osensor_data->values[2], osensor_data->status, osensor_data->value_divide);
		if (copy_to_user(argp, buff, strlen(buff)+1))
			return -EFAULT;
		break;

	default:
		MSE_ERR("%s not supported = 0x%04x", __func__, cmd);
		return -ENOIOCTLCMD;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static const struct file_operations bmm150_fops = {
	.owner = THIS_MODULE,
	.open = bmm150_open,
	.release = bmm150_release,
	.unlocked_ioctl = bmm150_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice bmm150_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msensor",
	.fops = &bmm150_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef USE_NEW_SENSOR_ARCH
static int bmm150_operate(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *msensor_data;

#if DEBUG
	/*struct i2c_client *client = this_client;*/
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & MMC_FUN_DEBUG)
		MSE_FUN();
#endif
	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;

			bmm150d_delay = value;
			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_DELAY;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {

			value = *(int *)buff_in;

			if (value == 1)
				atomic_set(&m_flag, 1);
			else
				atomic_set(&m_flag, 0);

			bmm150_SetPowerMode(this_client, (value == 1));

			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			MSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			msensor_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			msensor_data->values[0] = sensor_data[4];
			msensor_data->values[1] = sensor_data[5];
			msensor_data->values[2] = sensor_data[6];
			msensor_data->status = sensor_data[7];
			msensor_data->value_divide = CONVERT_M_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
		if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
			MSE_LOG("Hwm get m-sensor data: %d, %d, %d. divide %d, status %d!\n",
				msensor_data->values[0], msensor_data->values[1], msensor_data->values[2],
				msensor_data->value_divide, msensor_data->status);
		}
#endif
		}
		break;
	default:
		MSE_ERR("msensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
int bmm150_orientation_operate(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *osensor_data;
#if DEBUG
	/*struct i2c_client *client = this_client; */
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & MMC_FUN_DEBUG)
		MSE_FUN();
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			bmm150d_delay = value;
			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_DELAY;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {

			value = *(int *)buff_in;

			if (value == 1)
				atomic_set(&o_flag, 1);
			else
				atomic_set(&o_flag, 0);

			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			MSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			osensor_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			osensor_data->values[0] = sensor_data[8];
			osensor_data->values[1] = sensor_data[9];
			osensor_data->values[2] = sensor_data[10];
			osensor_data->status = sensor_data[11];
			osensor_data->value_divide = CONVERT_O_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
				MSE_LOG("Hwm get o-sensor data: %d, %d, %d. divide %d, status %d!\n",
				osensor_data->values[0], osensor_data->values[1], osensor_data->values[2],
				osensor_data->value_divide, osensor_data->status);
			}
#endif
		}
		break;
	default:
		MSE_ERR("osensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
/*----------------------------------------------------------------------------*/
#ifdef BMC156_M4G
int bmm150_m4g_operate(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *g_data;
#if DEBUG
	/*struct i2c_client *client = this_client;  */
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & MMC_FUN_DEBUG)
		MSE_FUN();
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			m4g_delay = value;
			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_G_DELAY;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {

			value = *(int *)buff_in;

			if (value == 1)
				atomic_set(&g_flag, 1);
			else
				atomic_set(&g_flag, 0);

			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_G_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			MSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			g_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			g_data->values[0] = m4g_data[0];
			g_data->values[1] = m4g_data[1];
			g_data->values[2] = m4g_data[2];
			g_data->status = m4g_data[3];
			g_data->value_divide = CONVERT_G_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
				MSE_LOG("Hwm get m4g data: %d, %d, %d. divide %d, status %d!\n",
					g_data->values[0], g_data->values[1], g_data->values[2],
					g_data->value_divide, g_data->status);
			}
#endif
			}
		break;
	default:
		MSE_ERR("m4g operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
#endif /* BMC156_M4G */
/*----------------------------------------------------------------------------*/
#ifdef BMC156_VRV
int bmm150_vrv_operate(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *vrv_data;
#if DEBUG
	/*struct i2c_client *client = this_client;  */
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & MMC_FUN_DEBUG)
		MSE_FUN();
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			vrv_delay = value;
			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VRV_DELAY;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {

			value = *(int *)buff_in;

			if (value == 1)
				atomic_set(&vrv_flag, 1);
			else
				atomic_set(&vrv_flag, 0);

			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VRV_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			MSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			vrv_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			vrv_data->values[0] = m4g_data[4];
			vrv_data->values[1] = m4g_data[5];
			vrv_data->values[2] = m4g_data[6];
			vrv_data->status = m4g_data[7];
			vrv_data->value_divide = CONVERT_VRV_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
				MSE_LOG("Hwm get rotation vector data: %d, %d, %d. divide %d, status %d!\n",
					vrv_data->values[0], vrv_data->values[1], vrv_data->values[2],
					vrv_data->value_divide, vrv_data->status);
			}
#endif
			}
		break;
	default:
		MSE_ERR("rotation vector operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
#endif /* BMC156_VRV */
/*----------------------------------------------------------------------------*/
#ifdef BMC156_VLA
int bmm150_vla_operate(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *vla_value;
#if DEBUG
	/*struct i2c_client *client = this_client;  */
#ifdef BMC156_SHARE_I2C_ADDRESS
	struct bmm150_i2c_data *data = g_bmm_obj;
#else
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);
#endif
#endif

#if DEBUG
	if (atomic_read(&data->trace) & MMC_FUN_DEBUG)
		MSE_FUN();
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			vla_delay = value;
			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VLA_DELAY;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {

			value = *(int *)buff_in;

			if (value == 1)
				atomic_set(&vla_flag, 1);
			else
				atomic_set(&vla_flag, 0);

			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VLA_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			MSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			vla_value = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			vla_value->values[0] = vla_data[0];
			vla_value->values[1] = vla_data[1];
			vla_value->values[2] = vla_data[2];
			vla_value->status = vla_data[3];
			vla_value->value_divide = CONVERT_VLA_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
				MSE_LOG("Hwm get virtual linear accelerometer data: ");
				MSE_LOG(" %d, %d, %d. divide %d, status %d!\n",
					vla_value->values[0], vla_value->values[1], vla_value->values[2],
					vla_value->value_divide, vla_value->status);
			}
#endif
		}
		break;
	default:
		MSE_ERR("virtual linear accelerometer operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
#endif /* BMC156_VLA */
/*----------------------------------------------------------------------------*/
#ifdef BMC156_VG
int bmm150_vg_operate(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *vg_value;
#if DEBUG
	/*struct i2c_client *client = this_client;  */
#ifdef BMC156_SHARE_I2C_ADDRESS
	struct bmm150_i2c_data *data = g_bmm_obj;
#else
	struct bmm150_i2c_data *data = i2c_get_clientdata(this_client);
#endif
#endif

#if DEBUG
	if (atomic_read(&data->trace) & MMC_FUN_DEBUG)
		MSE_FUN();
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			vg_delay = value;
			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VG_DELAY;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			MSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {

			value = *(int *)buff_in;

			if (value == 1)
				atomic_set(&vg_flag, 1);
			else
				atomic_set(&vg_flag, 0);

			/* set the flag */
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VG_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			MSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			vg_value = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			vg_value->values[0] = vg_data[0];
			vg_value->values[1] = vg_data[1];
			vg_value->values[2] = vg_data[2];
			vg_value->status = vg_data[3];
			vg_value->value_divide = CONVERT_VG_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
				MSE_LOG("Hwm get virtual gravity data: %d, %d, %d. divide %d, status %d!\n",
					vg_value->values[0], vg_value->values[1], vg_value->values[2],
					vg_value->value_divide, vg_value->status);
			}
#endif
		}
		break;
	default:
		MSE_ERR("virtual gravity operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
#endif /* BMC156_VG */
#endif
/*----------------------------------------------------------------------------*/
static void bmm150_restore_hw_cfg(struct i2c_client *client)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(client);

	if (obj->op_mode > 3)
		obj->op_mode = BMM150_SLEEP_MODE;

	bmm150api_set_functional_state(obj->op_mode);

	bmm150api_set_datarate(obj->odr);
	/* mdelay(BMM_I2C_WRITE_DELAY_TIME); */

	bmm150api_set_repetitions_XY(obj->rept_xy);
	/* mdelay(BMM_I2C_WRITE_DELAY_TIME); */

	bmm150api_set_repetitions_Z(obj->rept_z);
	/* mdelay(BMM_I2C_WRITE_DELAY_TIME); */
}

/*----------------------------------------------------------------------------*/
#ifndef	CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int bmm150_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(client);

	if (msg.event == PM_EVENT_SUSPEND) {
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
		/* set driver suspend flag */
		atomic_set(&driver_suspend_flag, 1);
		if (atomic_read(&m_flag) == 1) {
			/* set the flag to block e-compass daemon*/
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
		}
		if (atomic_read(&o_flag) == 1) {
			/* set the flag to block e-compass daemon*/
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
		}
#ifdef BMC156_M4G
		if (atomic_read(&g_flag) == 1) {
			/* set the flag to block e-compass daemon*/
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_G_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
		}
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
		if (atomic_read(&vrv_flag) == 1) {
			/* set the flag to block e-compass daemon*/
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VRV_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
		}
#endif /* BMC156_VRV */
#ifdef BMC156_VLA
		if (atomic_read(&vla_flag) == 1) {
			/* set the flag to block e-compass daemon*/
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VLA_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
		}
#endif /* BMC156_VLA */
#ifdef BMC156_VG
		if (atomic_read(&vg_flag) == 1) {
			/* set the flag to block e-compass daemon*/
			mutex_lock(&uplink_event_flag_mutex);
			uplink_event_flag |= BMMDRV_ULEVT_FLAG_VG_ACTIVE;
			mutex_unlock(&uplink_event_flag_mutex);
		}
#endif /* BMC156_VG */

		/* wake up the wait queue */
		wake_up(&uplink_event_flag_wq);
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
		BMM150_SCP_SetPowerMode(false);
#endif/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
		bmm150_SetPowerMode(obj->client, false);
		bmm150_power(obj->hw, 0);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int bmm150_resume(struct i2c_client *client)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(client);

	bmm150_power(obj->hw, 1);
	bmm150_SetPowerMode(obj->client, true);
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	BMM150_SCP_SetPowerMode(true);
#endif/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
	/* clear driver suspend flag */
	atomic_set(&driver_suspend_flag, 0);
	if (atomic_read(&m_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
	if (atomic_read(&o_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#ifdef BMC156_M4G
	if (atomic_read(&g_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_G_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
	if (atomic_read(&vrv_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_VRV_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_VRV */
#ifdef BMC156_VG
	if (atomic_read(&vg_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_VG_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_VG */

	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */
	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void bmm150_early_suspend(struct early_suspend *h)
{
	struct bmm150_i2c_data *obj = container_of(h, struct bmm150_i2c_data, early_drv);
	u8 power_mode;
	int err = 0;

	if (NULL == obj) {
		MSE_ERR("null pointer!!\n");
		return;
	}

#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
	/* set driver suspend flag */
	atomic_set(&driver_suspend_flag, 1);
	if (atomic_read(&m_flag) == 1) {
		/* set the flag to block e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
	if (atomic_read(&o_flag) == 1) {
		/* set the flag to block e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#ifdef BMC156_M4G
	if (atomic_read(&g_flag) == 1) {
		/* set the flag to block e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_G_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
	if (atomic_read(&vrv_flag) == 1) {
		/* set the flag to block e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_VRV_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_VRV */
#ifdef BMC156_VLA
	if (atomic_read(&vla_flag) == 1) {
		/* set the flag to block e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_VLA_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_VLA */
#ifdef BMC156_VRV
	if (atomic_read(&vg_flag) == 1) {
		/* set the flag to block e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_VG_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_VG */

	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */

	bmm150_SetPowerMode(obj->client, false);
	bmm150_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void bmm150_late_resume(struct early_suspend *h)
{
	struct bmm150_i2c_data *obj = container_of(h, struct bmm150_i2c_data, early_drv);

	if (NULL == obj) {
		MSE_ERR("null pointer!!\n");
		return;
	}

	bmm150_power(obj->hw, 1);
	bmm150_SetPowerMode(obj->client, true);
#ifdef BMC156_BLOCK_DAEMON_ON_SUSPEND
	/* clear driver suspend flag */
	atomic_set(&driver_suspend_flag, 0);
	if (atomic_read(&m_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
	if (atomic_read(&o_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#ifdef BMC156_M4G
	if (atomic_read(&g_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_G_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
	if (atomic_read(&vrv_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_VRV_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_VRV */
#ifdef BMC156_VLA
	if (atomic_read(&vla_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_VLA_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_VLA */
#ifdef BMC156_VG
	if (atomic_read(&vg_flag) == 1) {
		/* set the flag to unblock e-compass daemon*/
		mutex_lock(&uplink_event_flag_mutex);
		uplink_event_flag |= BMMDRV_ULEVT_FLAG_VG_ACTIVE;
		mutex_unlock(&uplink_event_flag_mutex);
	}
#endif /* BMC156_VG */

	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);
#endif /* BMC156_BLOCK_DAEMON_ON_SUSPEND */
}
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
#define BMM_MAX_RETRY_WAKEUP (5)
#define BMM_I2C_WRITE_DELAY_TIME (1)
static int bmm150_wakeup(struct i2c_client *client)
{
	int err = 0;
	int try_times = BMM_MAX_RETRY_WAKEUP;
	u8 data[2] = {BMM150_POWER_CNTL, 0x01};
	u8 dummy;

	MSE_LOG("waking up the chip...");

	while (try_times) {
		err = i2c_master_send(client, data, 2);
		mdelay(BMM_I2C_WRITE_DELAY_TIME);
		dummy = 0;
		err = hwmsen_read_block(client, BMM150_POWER_CNTL, &dummy, 1);
		if (data[1] == dummy)
			break;
		try_times--;
	}

	MSE_LOG("wake up result: %s, tried times: %d",
			(try_times > 0) ? "succeed" : "fail",
			BMM_MAX_RETRY_WAKEUP - try_times + 1);

	err = (try_times > 0) ? 0 : -1;

	return err;
}
/*----------------------------------------------------------------------------*/
static int bmm150_checkchipid(struct i2c_client *client)
{
	/*int err = 0;*/
	u8 chip_id = 0;

	hwmsen_read_block(client, BMM150_CHIP_ID, &chip_id, 1);
	MSE_LOG("read chip id result: %#x", chip_id);

	if ((chip_id & 0xff) != SENSOR_CHIP_ID_BMM)
		return -1;
	else
		return 0;
}
/*----------------------------------------------------------------------------*/
static char bmm150_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	return hwmsen_read_block(this_client, reg_addr, data, len);
}
/*----------------------------------------------------------------------------*/
static char bmm150_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	u8 buff[BMM150_BUFSIZE + 1];

	if (len > BMM150_BUFSIZE)
		return -1;

	buff[0] = reg_addr;
	memcpy(buff+1, data, len);
	if (i2c_master_send(this_client, buff, len+1) != (len+1)) /* I2C transfer error */
		return -EIO;
	return 0;
}
/*----------------------------------------------------------------------------*/
static void bmm150_delay(u32 msec)
{
	mdelay(msec);
}
/*----------------------------------------------------------------------------*/
static int bmm150_init_client(struct i2c_client *client)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	res = bmm150_wakeup(client);
	if (res < 0)
		return res;
	res = bmm150_checkchipid(client);
	if (res < 0)
		return res;
	MSE_LOG("check chip ID ok");

	/* bmm150 api init */
	obj->device.bus_read = bmm150_i2c_read_wrapper;
	obj->device.bus_write = bmm150_i2c_write_wrapper;
	obj->device.delay_msec = bmm150_delay;
	bmm150api_init(&obj->device);

	/* now it's power on which is considered as resuming from suspend */
	obj->op_mode = BMM150_SUSPEND_MODE;
	obj->odr = BMM150_REGULAR_DR;
	obj->rept_xy = BMM150_REGULAR_REPXY;
	obj->rept_z = BMM150_REGULAR_REPZ;

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	res = msensor_setup_irq();
	if (res != 0)
		return res;
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
	res = bmm150api_set_functional_state(BMM150_SUSPEND_MODE);
	if (res)
		return -EIO;

	return 0;
}
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
static void msensor_irq_work(struct work_struct *work)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);
	struct scp_mag_hw scp_hw;
	union BMM150_CUST_DATA *p_cust_data;
	SCP_SENSOR_HUB_DATA data;
	int max_cust_data_size_per_packet;
	int i;
	uint sizeOfCustData;
	uint len;
	char *p = (char *)&scp_hw;

	MSE_FUN();

	scp_hw.i2c_num = obj->hw->i2c_num;
	scp_hw.direction = obj->hw->direction;
	scp_hw.power_id = obj->hw->power_id;
	scp_hw.power_vol = obj->hw->power_vol;
	memcpy(scp_hw.i2c_addr, obj->hw->i2c_addr, sizeof(obj->hw->i2c_addr));
	scp_hw.power_vio_id = obj->hw->power_vio_id;
	scp_hw.power_vio_vol = obj->hw->power_vio_vol;
	scp_hw.is_batch_supported = obj->hw->is_batch_supported;

	p_cust_data = (union BMM150_CUST_DATA *) data.set_cust_req.custData;
	sizeOfCustData = sizeof(scp_hw);
	max_cust_data_size_per_packet =
		sizeof(data.set_cust_req.custData) - offsetof(struct BMM150_SET_CUST, data);

	/*MSE_ERR("sizeOfCustData = %d, max_cust_data_size_per_packet = %d\n", sizeOfCustData,
		max_cust_data_size_per_packet);
	MSE_ERR("offset %lu\n", offsetof(struct BMA255_SET_CUST, data));*/

	for (i = 0; sizeOfCustData > 0; i++) {
		data.set_cust_req.sensorType = ID_MAGNETIC;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		p_cust_data->setCust.action = BMM150_CUST_ACTION_SET_CUST;
		p_cust_data->setCust.part = i;
		if (sizeOfCustData > max_cust_data_size_per_packet)
			len = max_cust_data_size_per_packet;
		else
			len = sizeOfCustData;

		memcpy(p_cust_data->setCust.data, p, len);
		sizeOfCustData -= len;
		p += len;

		/*MSE_ERR("i= %d, sizeOfCustData = %d, len = %d\n", i, sizeOfCustData, len);*/
		len +=
			offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + offsetof(struct BMM150_SET_CUST,
			data);
		/*MSE_ERR("data.set_cust_req.sensorType= %d\n", data.set_cust_req.sensorType);*/
		SCP_sensorHub_req_send(&data, &len, 1);

	}
	obj->SCP_init_done = 1;
}

/*----------------------------------------------------------------------------*/
static int msensor_irq_handler(void *data, uint len)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(this_client);
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;

	/*MSE_ERR("gsensor_irq_handler len = %d, type = %d, action = %d, errCode = %d\n", len,
		rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode);*/
	if (!obj)
		return -1;

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_INIT_DONE:
			schedule_work(&obj->irq_work);
			/*MSE_ERR("OK sensor hub notify\n");*/
			break;
		default:
			MSE_ERR("Error sensor hub notify\n");
			break;
		}
		break;
	default:
		MSE_ERR("Error sensor hub action\n");
		break;
	}

	return 0;
}

static int msensor_setup_irq(void)
{
	int err = 0;

	err = SCP_sensorHub_rsp_registration(ID_MAGNETIC, msensor_irq_handler);

	return err;
}
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
/*----------------------------------------------------------------------------*/
#ifndef USE_NEW_SENSOR_ARCH
static struct platform_driver bmm150_sensor_driver = {
	.probe = bmm150_probe,
	.remove = bmm150_remove,
	.driver = {
		.name = "msensor",
	/* .owner = THIS_MODULE, */
	}
};
#endif

#ifdef USE_NEW_SENSOR_ARCH
/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
int bmc156_gyro_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */

int bmc156_gyro_enable_nodata(int en)
{

	if (en == 1)
		atomic_set(&g_flag, 1);
	else
		atomic_set(&g_flag, 0);

	/* set the flag */
	mutex_lock(&uplink_event_flag_mutex);
	uplink_event_flag |= BMMDRV_ULEVT_FLAG_G_ACTIVE;
	mutex_unlock(&uplink_event_flag_mutex);
	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);

	return 0;
}

int bmc156_gyro_set_delay(u64 ns)
{
	int value = (int)ns/1000/1000;

	m4g_delay = value;

	/* set the flag */
	mutex_lock(&uplink_event_flag_mutex);
	uplink_event_flag |= BMMDRV_ULEVT_FLAG_G_DELAY;
	mutex_unlock(&uplink_event_flag_mutex);

	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);

	return 0;
}

int bmc156_gyro_get_data(int *x , int *y, int *z, int *status)
{

	mutex_lock(&sensor_data_mutex);

	*x = m4g_data[0];
	*y = m4g_data[1];
	*z = m4g_data[2];
	*status = m4g_data[3];

	mutex_unlock(&sensor_data_mutex);

	return 0;
}

int bmc156_m_enable(int en)
{
	if (en == 1)
		atomic_set(&m_flag, 1);
	else
		atomic_set(&m_flag, 0);

	/* set the flag */
	mutex_lock(&uplink_event_flag_mutex);
	uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_ACTIVE;
	mutex_unlock(&uplink_event_flag_mutex);
	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	BMM150_SCP_SetPowerMode(en);
#endif/*CONFIG_CUSTOM_KERNEL_SENSORHUB*/
	return 0;
}

int bmc156_m_set_delay(u64 ns)
{
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err;
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
	int value = (int)ns/1000/1000;

	bmm150d_delay = value;
	/* set the flag */
	mutex_lock(&uplink_event_flag_mutex);
	uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_DELAY;
	mutex_unlock(&uplink_event_flag_mutex);
	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	req.set_delay_req.sensorType = ID_MAGNETIC;
	req.set_delay_req.action = SENSOR_HUB_SET_DELAY;
	req.set_delay_req.delay = value;
	len = sizeof(req.activate_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		MSE_ERR("SCP_sensorHub_req_send!\n");
		return err;
	}
#endif			/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
	return 0;
}

int bmc156_m_open_report_data(int open)
{
	return 0;
}

int bmc156_m_get_data(int *x , int *y, int *z, int *status)
{
	mutex_lock(&sensor_data_mutex);

	*x = sensor_data[4];
	*y = sensor_data[5];
	*z = sensor_data[6];
	*status = sensor_data[7];

	mutex_unlock(&sensor_data_mutex);

	return 0;
}

int bmc156_o_enable(int en)
{
	if (en == 1)
		atomic_set(&o_flag, 1);
	else
		atomic_set(&o_flag, 0);

	/* set the flag */
	mutex_lock(&uplink_event_flag_mutex);
	uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
	mutex_unlock(&uplink_event_flag_mutex);
	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	BMM150_SCP_SetPowerMode(en);
#endif/*CONFIG_CUSTOM_KERNEL_SENSORHUB*/

	return 0;
}

int bmc156_o_set_delay(u64 ns)
{
#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err;
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
	int value = (int)ns/1000/1000;

	bmm150d_delay = value;
	/* set the flag */
	mutex_lock(&uplink_event_flag_mutex);
	uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_DELAY;
	mutex_unlock(&uplink_event_flag_mutex);
	/* wake up the wait queue */
	wake_up(&uplink_event_flag_wq);

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	req.set_delay_req.sensorType = ID_MAGNETIC;
	req.set_delay_req.action = SENSOR_HUB_SET_DELAY;
	req.set_delay_req.delay = value;
	len = sizeof(req.activate_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		MSE_ERR("SCP_sensorHub_req_send!\n");
		return err;
	}
#endif			/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */

	return 0;
}

int bmc156_o_open_report_data(int open)
{
	return 0;
}

int bmc156_o_get_data(int *x , int *y, int *z, int *status)
{
	mutex_lock(&sensor_data_mutex);

	*x = sensor_data[8];
	*y = sensor_data[9];
	*z = sensor_data[10];
	*status = sensor_data[11];

	mutex_unlock(&sensor_data_mutex);

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
static int bmm150_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct bmm150_i2c_data *data;
	int err = 0;
#ifdef USE_NEW_SENSOR_ARCH
	struct mag_control_path ctl = {0};
	struct mag_data_path mag_data = {0};
#else
	struct hwmsen_object sobj_m, sobj_o;
#ifdef BMC156_M4G
	struct hwmsen_object sobj_g;
#endif /* BMC156_M4G */
#ifdef BMC156_VRV
	struct hwmsen_object sobj_vrv;
#endif /* BMC156_VRV */
#ifdef BMC156_VLA
	struct hwmsen_object sobj_vla;
#endif /* BMC156_VLA */
#ifdef BMC156_VG
	struct hwmsen_object sobj_vg;
#endif /* BMC156_VG */
#endif

#ifdef BMC156_SHARE_I2C_ADDRESS
	if (client == NULL)
		return -1;
#endif
	data = kmalloc(sizeof(struct bmm150_i2c_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct bmm150_i2c_data));

	data->hw = get_cust_mag_hw();
	err = hwmsen_get_convert(data->hw->direction, &data->cvt);
	if (err) {
		MSE_ERR("invalid direction: %d\n", data->hw->direction);
		goto exit;
	}
	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0);

	mutex_init(&sensor_data_mutex);
	mutex_init(&uplink_event_flag_mutex);

	init_waitqueue_head(&uplink_event_flag_wq);

	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);

	this_client = new_client;

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
	INIT_WORK(&data->irq_work, msensor_irq_work);
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
	/* initial client */
	err = bmm150_init_client(this_client);
	if (err) {
		MSE_ERR("fail to initialize client");
		goto exit_client_failed;
	}

	/* Register sysfs attribute */
#ifdef USE_NEW_SENSOR_ARCH
	err = bmm150_create_attr(&(bmm150_init_info.platform_diver_addr->driver));
#else
	err = bmm150_create_attr(&bmm150_sensor_driver.driver);
#endif
	if (err) {
		MSE_ERR("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}
	err = misc_register(&bmm150_device);
	if (err) {
		MSE_ERR("bmm150_device register failed\n");
		goto exit_misc_device_register_failed;
	}

#ifdef USE_NEW_SENSOR_ARCH
	ctl.m_enable = bmc156_m_enable;
	ctl.m_set_delay = bmc156_m_set_delay;
	ctl.m_open_report_data = bmc156_m_open_report_data;
	ctl.o_enable = bmc156_o_enable;
	ctl.o_set_delay = bmc156_o_set_delay;
	ctl.o_open_report_data = bmc156_o_open_report_data;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = data->hw->is_batch_supported;

	err = mag_register_control_path(&ctl);
	if (err) {
		MAG_ERR("register mag control path err\n");
		goto exit_kfree;
	}

	mag_data.div_m = 4;
	mag_data.div_o = 71;
	mag_data.get_data_o = bmc156_o_get_data;
	mag_data.get_data_m = bmc156_m_get_data;

	err = mag_register_data_path(&mag_data);
	if (err) {
		MAG_ERR("register data control path err\n");
		goto exit_kfree;
	}
#else
	sobj_m.self = data;
	sobj_m.polling = 1;
	sobj_m.sensor_operate = bmm150_operate;
	err = hwmsen_attach(ID_MAGNETIC, &sobj_m);
	if (err) {
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}

	sobj_o.self = data;
	sobj_o.polling = 1;
	sobj_o.sensor_operate = bmm150_orientation_operate;
	err = hwmsen_attach(ID_ORIENTATION, &sobj_o);
	if (err) {
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}

#ifdef BMC156_M4G
	sobj_g.self = data;
	sobj_g.polling = 1;
	sobj_g.sensor_operate = bmm150_m4g_operate;
	err = hwmsen_attach(ID_GYROSCOPE, &sobj_g);
	if (err) {
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
#endif /* BMC156_M4G */

#ifdef BMC156_VRV
	sobj_vrv.self = data;
	sobj_vrv.polling = 1;
	sobj_vrv.sensor_operate = bmm150_vrv_operate;
	err = hwmsen_attach(ID_ROTATION_VECTOR, &sobj_vrv);
	if (err) {
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
#endif /* BMC156_VRV */

#ifdef BMC156_VLA
	sobj_vla.self = data;
	sobj_vla.polling = 1;
	sobj_vla.sensor_operate = bmm150_vla_operate;
	err = hwmsen_attach(ID_LINEAR_ACCELERATION, &sobj_vla);
	if (err) {
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
#endif /* BMC156_VLA */

#ifdef BMC156_VG
	sobj_vg.self = data;
	sobj_vg.polling = 1;
	sobj_vg.sensor_operate = bmm150_vg_operate;
	err = hwmsen_attach(ID_GRAVITY, &sobj_vg);
	if (err) {
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
#endif /* BMC156_VG */
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	data->early_drv.suspend = bmm150_early_suspend,
	data->early_drv.resume = bmm150_late_resume,
	register_early_suspend(&data->early_drv);
#endif
#ifdef USE_NEW_SENSOR_ARCH
	bmm150_init_flag = 1;
#endif
	MSE_LOG("%s: OK\n", __func__);
	return 0;

exit_client_failed:
exit_sysfs_create_group_failed:
exit_misc_device_register_failed:
exit_kfree:
	kfree(data);
exit:
	MSE_ERR("%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int bmm150_i2c_remove(struct i2c_client *client)
{
	struct bmm150_i2c_data *obj = i2c_get_clientdata(client);

#ifdef USE_NEW_SENSOR_ARCH
	if (bmm150_delete_attr(&(bmm150_init_info.platform_diver_addr->driver)))
#else
	if (bmm150_delete_attr(&bmm150_sensor_driver.driver))
#endif
		MSE_ERR("bmm150_delete_attr fail");

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&obj->early_drv);
#endif
	bmm150api_set_functional_state(BMM150_SUSPEND_MODE);
	this_client = NULL;
	i2c_unregister_device(client);

	kfree(obj);
	misc_deregister(&bmm150_device);
	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id mag_of_match[] = {
	{.compatible = "mediatek,msensor"},
	{},
};
#endif
/*----------------------------------------------------------------------------*/

static struct i2c_driver bmm150_i2c_driver = {
	.driver = {
		.name = BMM150_DEV_NAME,
#ifdef CONFIG_OF
	.of_match_table = mag_of_match,
#endif
	},
	.probe = bmm150_i2c_probe,
	.remove = bmm150_i2c_remove,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = bmm150_suspend,
	.resume = bmm150_resume,
#endif
	.id_table = bmm150_i2c_id,
};
#ifdef USE_NEW_SENSOR_ARCH
/*----------------------------------------------------------------------------*/
static int bmm150_remove(void)
{
	MSE_FUN();
	bmm150_power(hw, 0);
	i2c_del_driver(&bmm150_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int	bmm150_local_init(void)
{

	bmm150_power(hw, 1);
	if (i2c_add_driver(&bmm150_i2c_driver)) {
		MSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == bmm150_init_flag)
		return -1;
	return 0;
}
#else
/*----------------------------------------------------------------------------*/
static int bmm150_probe(struct platform_device *pdev)
{
	bmm150_power(hw, 1);
	if (i2c_add_driver(&bmm150_i2c_driver)) {
		MSE_ERR("add driver error\n");
		return -1;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static int bmm150_remove(struct platform_device *pdev)
{

	i2c_del_driver(&bmm150_i2c_driver);
	bmm150_power(hw, 0);
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int __init bmm150_init(void)
{
	const char *name = "mediatek,bmc156_mag";

	hw = get_mag_dts_func(name, hw);
	if (!hw)
		MAG_ERR("get dts info fail\n");


	/*i2c_register_board_info(hw->i2c_num, &bmm150_i2c_info, 1);*/
#ifdef USE_NEW_SENSOR_ARCH
	mag_driver_add(&bmm150_init_info);
#else
	if (platform_driver_register(&bmm150_sensor_driver)) {
		MSE_ERR("failed to register driver");
		return -ENODEV;
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit bmm150_exit(void)
{
#ifndef USE_NEW_SENSOR_ARCH
	platform_driver_unregister(&bmm150_sensor_driver);
#endif
}
/*----------------------------------------------------------------------------*/
module_init(bmm150_init);
module_exit(bmm150_exit);

MODULE_AUTHOR("hongji.zhou@bosch-sensortec.com");
MODULE_DESCRIPTION("bmm150 compass driver");
MODULE_LICENSE("GPLv2");


/* bmm050.c - bmm050 compass driver
 * 
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
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


#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <cust_mag.h>
#include <linux/hwmsen_helper.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "bmm050.h"

/*----------------------------------------------------------------------------*/
/*   BMM050 API Section	*/
/*----------------------------------------------------------------------------*/
#define BMC050_U16 unsigned short
#define BMC050_S16 signed short
#define BMC050_S32 signed int

#define BMC050_BUS_WR_RETURN_TYPE char

/*
//#define BMC050_BUS_WR_PARAM_TYPES\
//	unsigned char, unsigned char, unsigned char *, unsigned char
//#define BMC050_BUS_WR_PARAM_ORDER\
//	(device_addr, register_addr, register_data, wr_len)
*/

#define BMC050_BUS_WRITE_FUNC(device_addr, register_addr, register_data, wr_len) bus_write(device_addr, register_addr, register_data, wr_len)

#define BMC050_BUS_RD_RETURN_TYPE char

/*
//#define BMC050_BUS_RD_PARAM_TYPES\
//	unsigned char, unsigned char, unsigned char *, unsigned char

//#define BMC050_BUS_RD_PARAM_ORDER (device_addr, register_addr, register_data)
*/

#define BMC050_BUS_READ_FUNC(device_addr, register_addr, register_data, rd_len) bus_read(device_addr, register_addr, register_data, rd_len)

#define BMC050_DELAY_RETURN_TYPE void

#define BMC050_DELAY_PARAM_TYPES unsigned int

/*
//#define BMC050_DELAY_FUNC(delay_in_msec)
	//delay_func(delay_in_msec)
*/

#define BMC050_DELAY_POWEROFF_SUSPEND      1
#define BMC050_DELAY_SUSPEND_SLEEP         2
#define BMC050_DELAY_SLEEP_ACTIVE          1
#define BMC050_DELAY_ACTIVE_SLEEP          1
#define BMC050_DELAY_SLEEP_SUSPEND         1
#define BMC050_DELAY_ACTIVE_SUSPEND        1
#define BMC050_DELAY_SLEEP_POWEROFF        1
#define BMC050_DELAY_ACTIVE_POWEROFF       1
#define BMC050_DELAY_SETTLING_TIME         2


#define BMC050_RETURN_FUNCTION_TYPE        char
#define BMC050_I2C_ADDRESS                 0x10

/*General Info datas*/
#define BMC050_SOFT_RESET7_ON              1
#define BMC050_SOFT_RESET1_ON              1
#define BMC050_SOFT_RESET7_OFF             0
#define BMC050_SOFT_RESET1_OFF             0
#define BMC050_DELAY_SOFTRESET             1

/* Fixed Data Registers */
#define BMC050_CHIP_ID                     0x40
#define BMC050_REVISION_ID                 0x41

/* Data Registers */
#define BMC050_DATAX_LSB                   0x42
#define BMC050_DATAX_MSB                   0x43
#define BMC050_DATAY_LSB                   0x44
#define BMC050_DATAY_MSB                   0x45
#define BMC050_DATAZ_LSB                   0x46
#define BMC050_DATAZ_MSB                   0x47
#define BMC050_R_LSB                       0x48
#define BMC050_R_MSB                       0x49

/* Status Registers */
#define BMC050_INT_STAT                    0x4A

/* Control Registers */
#define BMC050_POWER_CNTL                  0x4B
#define BMC050_CONTROL                     0x4C
#define BMC050_INT_CNTL                    0x4D
#define BMC050_SENS_CNTL                   0x4E
#define BMC050_LOW_THRES                   0x4F
#define BMC050_HIGH_THRES                  0x50
#define BMC050_NO_REPETITIONS_XY           0x51
#define BMC050_NO_REPETITIONS_Z            0x52

/* Trim Extended Registers */
#define BMC050_DIG_X1                      0x5D
#define BMC050_DIG_Y1                      0x5E
#define BMC050_DIG_Z4_LSB                  0x62
#define BMC050_DIG_Z4_MSB                  0x63
#define BMC050_DIG_X2                      0x64
#define BMC050_DIG_Y2                      0x65
#define BMC050_DIG_Z2_LSB                  0x68
#define BMC050_DIG_Z2_MSB                  0x69
#define BMC050_DIG_Z1_LSB                  0x6A
#define BMC050_DIG_Z1_MSB                  0x6B
#define BMC050_DIG_XYZ1_LSB                0x6C
#define BMC050_DIG_XYZ1_MSB                0x6D
#define BMC050_DIG_Z3_LSB                  0x6E
#define BMC050_DIG_Z3_MSB                  0x6F
#define BMC050_DIG_XY2                     0x70
#define BMC050_DIG_XY1                     0x71


/* Data X LSB Regsiter */
#define BMC050_DATAX_LSB_VALUEX__POS        3
#define BMC050_DATAX_LSB_VALUEX__LEN        5
#define BMC050_DATAX_LSB_VALUEX__MSK        0xF8
#define BMC050_DATAX_LSB_VALUEX__REG        BMC050_DATAX_LSB

#define BMC050_DATAX_LSB_TESTX__POS         0
#define BMC050_DATAX_LSB_TESTX__LEN         1
#define BMC050_DATAX_LSB_TESTX__MSK         0x01
#define BMC050_DATAX_LSB_TESTX__REG         BMC050_DATAX_LSB

/* Data Y LSB Regsiter */
#define BMC050_DATAY_LSB_VALUEY__POS        3
#define BMC050_DATAY_LSB_VALUEY__LEN        5
#define BMC050_DATAY_LSB_VALUEY__MSK        0xF8
#define BMC050_DATAY_LSB_VALUEY__REG        BMC050_DATAY_LSB

#define BMC050_DATAY_LSB_TESTY__POS         0
#define BMC050_DATAY_LSB_TESTY__LEN         1
#define BMC050_DATAY_LSB_TESTY__MSK         0x01
#define BMC050_DATAY_LSB_TESTY__REG         BMC050_DATAY_LSB

/* Data Z LSB Regsiter */
#define BMC050_DATAZ_LSB_VALUEZ__POS        1
#define BMC050_DATAZ_LSB_VALUEZ__LEN        7
#define BMC050_DATAZ_LSB_VALUEZ__MSK        0xFE
#define BMC050_DATAZ_LSB_VALUEZ__REG        BMC050_DATAZ_LSB

#define BMC050_DATAZ_LSB_TESTZ__POS         0
#define BMC050_DATAZ_LSB_TESTZ__LEN         1
#define BMC050_DATAZ_LSB_TESTZ__MSK         0x01
#define BMC050_DATAZ_LSB_TESTZ__REG         BMC050_DATAZ_LSB

/* Hall Resistance LSB Regsiter */
#define BMC050_R_LSB_VALUE__POS             2
#define BMC050_R_LSB_VALUE__LEN             6
#define BMC050_R_LSB_VALUE__MSK             0xFC
#define BMC050_R_LSB_VALUE__REG             BMC050_R_LSB

#define BMC050_DATA_RDYSTAT__POS            0
#define BMC050_DATA_RDYSTAT__LEN            1
#define BMC050_DATA_RDYSTAT__MSK            0x01
#define BMC050_DATA_RDYSTAT__REG            BMC050_R_LSB

/* Interupt Status Register */
#define BMC050_INT_STAT_DOR__POS            7
#define BMC050_INT_STAT_DOR__LEN            1
#define BMC050_INT_STAT_DOR__MSK            0x80
#define BMC050_INT_STAT_DOR__REG            BMC050_INT_STAT

#define BMC050_INT_STAT_OVRFLOW__POS        6
#define BMC050_INT_STAT_OVRFLOW__LEN        1
#define BMC050_INT_STAT_OVRFLOW__MSK        0x40
#define BMC050_INT_STAT_OVRFLOW__REG        BMC050_INT_STAT

#define BMC050_INT_STAT_HIGH_THZ__POS       5
#define BMC050_INT_STAT_HIGH_THZ__LEN       1
#define BMC050_INT_STAT_HIGH_THZ__MSK       0x20
#define BMC050_INT_STAT_HIGH_THZ__REG       BMC050_INT_STAT

#define BMC050_INT_STAT_HIGH_THY__POS       4
#define BMC050_INT_STAT_HIGH_THY__LEN       1
#define BMC050_INT_STAT_HIGH_THY__MSK       0x10
#define BMC050_INT_STAT_HIGH_THY__REG       BMC050_INT_STAT

#define BMC050_INT_STAT_HIGH_THX__POS       3
#define BMC050_INT_STAT_HIGH_THX__LEN       1
#define BMC050_INT_STAT_HIGH_THX__MSK       0x08
#define BMC050_INT_STAT_HIGH_THX__REG       BMC050_INT_STAT

#define BMC050_INT_STAT_LOW_THZ__POS        2
#define BMC050_INT_STAT_LOW_THZ__LEN        1
#define BMC050_INT_STAT_LOW_THZ__MSK        0x04
#define BMC050_INT_STAT_LOW_THZ__REG        BMC050_INT_STAT

#define BMC050_INT_STAT_LOW_THY__POS        1
#define BMC050_INT_STAT_LOW_THY__LEN        1
#define BMC050_INT_STAT_LOW_THY__MSK        0x02
#define BMC050_INT_STAT_LOW_THY__REG        BMC050_INT_STAT

#define BMC050_INT_STAT_LOW_THX__POS        0
#define BMC050_INT_STAT_LOW_THX__LEN        1
#define BMC050_INT_STAT_LOW_THX__MSK        0x01
#define BMC050_INT_STAT_LOW_THX__REG        BMC050_INT_STAT

/* Power Control Register */
#define BMC050_POWER_CNTL_SRST7__POS       7
#define BMC050_POWER_CNTL_SRST7__LEN       1
#define BMC050_POWER_CNTL_SRST7__MSK       0x80
#define BMC050_POWER_CNTL_SRST7__REG       BMC050_POWER_CNTL

#define BMC050_POWER_CNTL_SPI3_EN__POS     2
#define BMC050_POWER_CNTL_SPI3_EN__LEN     1
#define BMC050_POWER_CNTL_SPI3_EN__MSK     0x04
#define BMC050_POWER_CNTL_SPI3_EN__REG     BMC050_POWER_CNTL

#define BMC050_POWER_CNTL_SRST1__POS       1
#define BMC050_POWER_CNTL_SRST1__LEN       1
#define BMC050_POWER_CNTL_SRST1__MSK       0x02
#define BMC050_POWER_CNTL_SRST1__REG       BMC050_POWER_CNTL

#define BMC050_POWER_CNTL_PCB__POS         0
#define BMC050_POWER_CNTL_PCB__LEN         1
#define BMC050_POWER_CNTL_PCB__MSK         0x01
#define BMC050_POWER_CNTL_PCB__REG         BMC050_POWER_CNTL

/* Control Register */
#define BMC050_CNTL_ADV_ST__POS            6
#define BMC050_CNTL_ADV_ST__LEN            2
#define BMC050_CNTL_ADV_ST__MSK            0xC0
#define BMC050_CNTL_ADV_ST__REG            BMC050_CONTROL

#define BMC050_CNTL_DR__POS                3
#define BMC050_CNTL_DR__LEN                3
#define BMC050_CNTL_DR__MSK                0x38
#define BMC050_CNTL_DR__REG                BMC050_CONTROL

#define BMC050_CNTL_OPMODE__POS            1
#define BMC050_CNTL_OPMODE__LEN            2
#define BMC050_CNTL_OPMODE__MSK            0x06
#define BMC050_CNTL_OPMODE__REG            BMC050_CONTROL

#define BMC050_CNTL_S_TEST__POS            0
#define BMC050_CNTL_S_TEST__LEN            1
#define BMC050_CNTL_S_TEST__MSK            0x01
#define BMC050_CNTL_S_TEST__REG            BMC050_CONTROL

/* Interupt Control Register */
#define BMC050_INT_CNTL_DOR_EN__POS            7
#define BMC050_INT_CNTL_DOR_EN__LEN            1
#define BMC050_INT_CNTL_DOR_EN__MSK            0x80
#define BMC050_INT_CNTL_DOR_EN__REG            BMC050_INT_CNTL

#define BMC050_INT_CNTL_OVRFLOW_EN__POS        6
#define BMC050_INT_CNTL_OVRFLOW_EN__LEN        1
#define BMC050_INT_CNTL_OVRFLOW_EN__MSK        0x40
#define BMC050_INT_CNTL_OVRFLOW_EN__REG        BMC050_INT_CNTL

#define BMC050_INT_CNTL_HIGH_THZ_EN__POS       5
#define BMC050_INT_CNTL_HIGH_THZ_EN__LEN       1
#define BMC050_INT_CNTL_HIGH_THZ_EN__MSK       0x20
#define BMC050_INT_CNTL_HIGH_THZ_EN__REG       BMC050_INT_CNTL

#define BMC050_INT_CNTL_HIGH_THY_EN__POS       4
#define BMC050_INT_CNTL_HIGH_THY_EN__LEN       1
#define BMC050_INT_CNTL_HIGH_THY_EN__MSK       0x10
#define BMC050_INT_CNTL_HIGH_THY_EN__REG       BMC050_INT_CNTL

#define BMC050_INT_CNTL_HIGH_THX_EN__POS       3
#define BMC050_INT_CNTL_HIGH_THX_EN__LEN       1
#define BMC050_INT_CNTL_HIGH_THX_EN__MSK       0x08
#define BMC050_INT_CNTL_HIGH_THX_EN__REG       BMC050_INT_CNTL

#define BMC050_INT_CNTL_LOW_THZ_EN__POS        2
#define BMC050_INT_CNTL_LOW_THZ_EN__LEN        1
#define BMC050_INT_CNTL_LOW_THZ_EN__MSK        0x04
#define BMC050_INT_CNTL_LOW_THZ_EN__REG        BMC050_INT_CNTL

#define BMC050_INT_CNTL_LOW_THY_EN__POS        1
#define BMC050_INT_CNTL_LOW_THY_EN__LEN        1
#define BMC050_INT_CNTL_LOW_THY_EN__MSK        0x02
#define BMC050_INT_CNTL_LOW_THY_EN__REG        BMC050_INT_CNTL

#define BMC050_INT_CNTL_LOW_THX_EN__POS        0
#define BMC050_INT_CNTL_LOW_THX_EN__LEN        1
#define BMC050_INT_CNTL_LOW_THX_EN__MSK        0x01
#define BMC050_INT_CNTL_LOW_THX_EN__REG        BMC050_INT_CNTL

/* Sensor Control Register */
#define BMC050_SENS_CNTL_DRDY_EN__POS          7
#define BMC050_SENS_CNTL_DRDY_EN__LEN          1
#define BMC050_SENS_CNTL_DRDY_EN__MSK          0x80
#define BMC050_SENS_CNTL_DRDY_EN__REG          BMC050_SENS_CNTL

#define BMC050_SENS_CNTL_IE__POS               6
#define BMC050_SENS_CNTL_IE__LEN               1
#define BMC050_SENS_CNTL_IE__MSK               0x40
#define BMC050_SENS_CNTL_IE__REG               BMC050_SENS_CNTL

#define BMC050_SENS_CNTL_CHANNELZ__POS         5
#define BMC050_SENS_CNTL_CHANNELZ__LEN         1
#define BMC050_SENS_CNTL_CHANNELZ__MSK         0x20
#define BMC050_SENS_CNTL_CHANNELZ__REG         BMC050_SENS_CNTL

#define BMC050_SENS_CNTL_CHANNELY__POS         4
#define BMC050_SENS_CNTL_CHANNELY__LEN         1
#define BMC050_SENS_CNTL_CHANNELY__MSK         0x10
#define BMC050_SENS_CNTL_CHANNELY__REG         BMC050_SENS_CNTL

#define BMC050_SENS_CNTL_CHANNELX__POS         3
#define BMC050_SENS_CNTL_CHANNELX__LEN         1
#define BMC050_SENS_CNTL_CHANNELX__MSK         0x08
#define BMC050_SENS_CNTL_CHANNELX__REG         BMC050_SENS_CNTL

#define BMC050_SENS_CNTL_DR_POLARITY__POS      2
#define BMC050_SENS_CNTL_DR_POLARITY__LEN      1
#define BMC050_SENS_CNTL_DR_POLARITY__MSK      0x04
#define BMC050_SENS_CNTL_DR_POLARITY__REG      BMC050_SENS_CNTL

#define BMC050_SENS_CNTL_INTERRUPT_LATCH__POS            1
#define BMC050_SENS_CNTL_INTERRUPT_LATCH__LEN            1
#define BMC050_SENS_CNTL_INTERRUPT_LATCH__MSK            0x02
#define BMC050_SENS_CNTL_INTERRUPT_LATCH__REG            BMC050_SENS_CNTL

#define BMC050_SENS_CNTL_INTERRUPT_POLARITY__POS         0
#define BMC050_SENS_CNTL_INTERRUPT_POLARITY__LEN         1
#define BMC050_SENS_CNTL_INTERRUPT_POLARITY__MSK         0x01
#define BMC050_SENS_CNTL_INTERRUPT_POLARITY__REG         BMC050_SENS_CNTL

/* Register 6D */
#define BMC050_DIG_XYZ1_MSB__POS         0
#define BMC050_DIG_XYZ1_MSB__LEN         7
#define BMC050_DIG_XYZ1_MSB__MSK         0x7F
#define BMC050_DIG_XYZ1_MSB__REG         BMC050_DIG_XYZ1_MSB


#define BMC050_X_AXIS               0
#define BMC050_Y_AXIS               1
#define BMC050_Z_AXIS               2
#define BMC050_RESISTANCE           3
#define BMC050_X                    1
#define BMC050_Y                    2
#define BMC050_Z                    4
#define BMC050_XYZ                  7

/* Constants */
#define BMC050_NULL                             0
#define BMC050_INTPIN_DISABLE                   1
#define BMC050_INTPIN_ENABLE                    0
#define BMC050_DISABLE                          0
#define BMC050_ENABLE                           1
#define BMC050_CHANNEL_DISABLE                  1
#define BMC050_CHANNEL_ENABLE                   0
#define BMC050_INTPIN_LATCH_ENABLE              1
#define BMC050_INTPIN_LATCH_DISABLE             0
#define BMC050_OFF                              0
#define BMC050_ON                               1

#define BMC050_NORMAL_MODE                      0x00
#define BMC050_FORCED_MODE                      0x01
#define BMC050_SUSPEND_MODE                     0x02
#define BMC050_SLEEP_MODE                       0x03

#define BMC050_ADVANCED_SELFTEST_OFF            0
#define BMC050_ADVANCED_SELFTEST_NEGATIVE       2
#define BMC050_ADVANCED_SELFTEST_POSITIVE       3

#define BMC050_NEGATIVE_SATURATION_Z            -32767
#define BMC050_POSITIVE_SATURATION_Z            32767

#define BMC050_SPI_RD_MASK                      0x80
#define BMC050_READ_SET                         0x01

#define E_BMC050_NULL_PTR                       ((char)-127)
#define E_BMC050_COMM_RES                       ((char)-1)
#define E_BMC050_OUT_OF_RANGE                   ((char)-2)
#define E_BMC050_UNDEFINED_MODE                 0

#define BMC050_WR_FUNC_PTR\
	char (*bus_write)(unsigned char, unsigned char,\
			unsigned char *, unsigned char)

#define BMC050_RD_FUNC_PTR\
	char (*bus_read)(unsigned char, unsigned char,\
			unsigned char *, unsigned char)
#define BMC050_MDELAY_DATA_TYPE unsigned int

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
#define BMC050_CONVFACTOR_LSB_UT                6

/* get bit slice  */
#define BMC050_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

/* Set bit slice */
#define BMC050_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/* compensated output value returned if sensor had overflow */
#define BMC050_OVERFLOW_OUTPUT       -32768
/* Flipcore overflow ADC value */
#define BMC050_FLIP_OVERFLOW_ADCVAL  -4096
/* Hall overflow 1 ADC value */
#define BMC050_HALL_OVERFLOW_ADCVAL  -16384


#define BMC050_PRESETMODE_LOWPOWER                  1
#define BMC050_PRESETMODE_REGULAR                   2
#define BMC050_PRESETMODE_HIGHACCURACY              3

/* PRESET MODES - DATA RATES */
#define BMC050_LOWPOWER_DR                       BMC050_DR_10HZ
#define BMC050_REGULAR_DR                        BMC050_DR_10HZ
#define BMC050_HIGHACCURACY_DR                   BMC050_DR_20HZ

/* PRESET MODES - REPETITIONS-XY RATES */
#define BMC050_LOWPOWER_REPXY                     2
#define BMC050_REGULAR_REPXY                      5
#define BMC050_HIGHACCURACY_REPXY                40

/* PRESET MODES - REPETITIONS-Z RATES */
#define BMC050_LOWPOWER_REPZ                      4
#define BMC050_REGULAR_REPZ                      13
#define BMC050_HIGHACCURACY_REPZ                 89

/* Data Rates */

#define BMC050_DR_10HZ                     0
#define BMC050_DR_02HZ                     1
#define BMC050_DR_06HZ                     2
#define BMC050_DR_08HZ                     3
#define BMC050_DR_15HZ                     4
#define BMC050_DR_20HZ                     5
#define BMC050_DR_25HZ                     6
#define BMC050_DR_30HZ                     7

/*user defined Structures*/
struct bmm050api_mdata {
	BMC050_S16 datax;
	BMC050_S16 datay;
	BMC050_S16 dataz;
	BMC050_U16 resistance;
};

struct bmm050api_offset {
	BMC050_S16 datax;
	BMC050_S16 datay;
	BMC050_S16 dataz;
};

struct bmc050 {
	unsigned char company_id;
	unsigned char revision_info;
	unsigned char dev_addr;

	BMC050_WR_FUNC_PTR;
	BMC050_RD_FUNC_PTR;
	void(*delay_msec)(BMC050_MDELAY_DATA_TYPE);

	signed char dig_x1;
	signed char dig_y1;

	signed char dig_x2;
	signed char dig_y2;

	BMC050_U16 dig_z1;
	BMC050_S16 dig_z2;
	BMC050_S16 dig_z3;
	BMC050_S16 dig_z4;

	unsigned char dig_xy1;
	signed char dig_xy2;

	BMC050_U16 dig_xyz1;
};

 BMC050_RETURN_FUNCTION_TYPE bmm050api_init(struct bmc050 *p_bmc050);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_flipdataX(
		BMC050_S16 *mdata_x);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_flipdataY(
		BMC050_S16 *mdata_y);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_halldataZ(
		BMC050_S16 *mdata_z);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_raw_xyz(
		struct bmm050api_mdata *mdata);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_raw_xyzr(
		struct bmm050api_mdata *mdata);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_read_mdataXYZ(
		struct bmm050api_mdata *mdata);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_read_mdataX(
		BMC050_S16 *mdata_x);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_read_mdataY(
		BMC050_S16 *mdata_y);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_read_mdataZ(
		BMC050_S16 *mdata_z);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_mdataResistance(
		BMC050_U16  *mdata_resistance);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_read_register(
		unsigned char addr, unsigned char *data, unsigned char len);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_write_register(
		unsigned char addr, unsigned char *data, unsigned char len);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_self_test_X(
		unsigned char *self_testx);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_self_test_Y(
		unsigned char *self_testy);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_self_test_Z(
		unsigned char *self_testz);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_self_test_XYZ(
		unsigned char *self_testxyz);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_rdy_status(
		unsigned char *rdy_status);
BMC050_S16 bmm050api_compensate_X(
		BMC050_S16 mdata_x, BMC050_U16 data_R);
BMC050_S16 bmm050api_compensate_Y(
		BMC050_S16 mdata_y, BMC050_U16 data_R);
BMC050_S16 bmm050api_compensate_Z(
		BMC050_S16 mdata_z,  BMC050_U16 data_R);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_init_trim_registers(void);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_status_reg(
		unsigned char *status_data);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_power_control_reg(
		unsigned char *pwr_cntl_data);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_soft_reset(void);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_spi3(
		unsigned char value);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_powermode(
		unsigned char *mode);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_powermode(
		unsigned char mode);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_adv_selftest(
		unsigned char adv_selftest);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_adv_selftest(
		unsigned char *adv_selftest);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_datarate(
		unsigned char data_rate);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_datarate(
		unsigned char *data_rate);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_functional_state(
		unsigned char functional_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_functional_state(
		unsigned char *functional_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_selftest(
		unsigned char selftest);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_selftest(
		unsigned char *selftest);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_perform_advanced_selftest(
		BMC050_S16 *diff_z);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_overrun_function(
		unsigned char data_overrun_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_overrun_function(
		unsigned char *data_overrun_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_overflow_function(
		unsigned char data_overflow);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_overflow_function(
		unsigned char *data_overflow);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_highthreshold_Z_function(
		unsigned char data_highthreshold_z_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_highthreshold_Z_function(
		unsigned char *data_highthreshold_z_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_highthreshold_Y_function(
		unsigned char data_highthreshold_y_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_highthreshold_Y_function(
		unsigned char *data_highthreshold_y_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_highthreshold_X_function(
		unsigned char data_highthreshold_x_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_highthreshold_X_function(
		unsigned char *data_highthreshold_x_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_lowthreshold_Z_function(
		unsigned char data_lowthreshold_z_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_lowthreshold_Z_function(
		unsigned char *data_lowthreshold_z_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_lowthreshold_Y_function(
		unsigned char data_lowthreshold_y_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_lowthreshold_Y_function(
		unsigned char *data_lowthreshold_y_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_lowthreshold_X_function(
		unsigned char data_lowthreshold_x_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_lowthreshold_X_function(
		unsigned char *data_lowthreshold_x_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_data_ready_function(
		unsigned char *data_ready_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_data_ready_function(
		unsigned char data_ready_function_state);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_interrupt_func(
		unsigned char *int_func);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_interrupt_func(
		unsigned char int_func);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_control_measurement_z(
		unsigned char *enable_disable);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_control_measurement_z(
		unsigned char enable_disable);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_control_measurement_y(
		unsigned char enable_disable);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_control_measurement_y(
		unsigned char *enable_disable);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_control_measurement_x(
		unsigned char enable_disable);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_control_measurement_x(
		unsigned char *enable_disable);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_DR_polarity(
		unsigned char dr_polarity_select);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_DR_polarity(
		unsigned char *dr_polarity_select);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_interrupt_latch(
		unsigned char interrupt_latch_select);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_interrupt_latch(
		unsigned char *interrupt_latch_select);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_intpin_polarity(
		unsigned char int_polarity_select);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_intpin_polarity(
		unsigned char *int_polarity_select);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_low_threshold(
		BMC050_S16 *low_threshold);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_low_threshold(
		BMC050_S16 low_threshold);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_high_threshold(
		BMC050_S16 *high_threshold);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_high_threshold(
		BMC050_S16 high_threshold);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_repetitions_XY(
		unsigned char *no_repetitions_xy);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_repetitions_XY(
		unsigned char no_repetitions_xy);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_repetitions_Z(
		unsigned char *no_repetitions_z);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_repetitions_Z(
		unsigned char no_repetitions_z);
 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_presetmode(unsigned char *mode);

 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_presetmode(unsigned char mode);

static void bmm050_restore_hw_cfg(struct i2c_client *client);
static int bmm050_probe(struct platform_device *pdev); 
static int bmm050_remove(struct platform_device *pdev);
static struct bmc050 *p_bmc050;

 BMC050_RETURN_FUNCTION_TYPE bmm050api_init(struct bmc050 *bmc050)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char a_data_u8r[2];
	p_bmc050 = bmc050;

	p_bmc050->dev_addr = BMC050_I2C_ADDRESS;

	/* set device from suspend into sleep mode */
	bmm050api_set_powermode(BMC050_ON);

	/* wait two millisecond for bmc to settle */
	p_bmc050->delay_msec(BMC050_DELAY_SETTLING_TIME);

	/*Read CHIP_ID and REv. info */
	comres = p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_CHIP_ID, a_data_u8r, 2);
	p_bmc050->company_id = a_data_u8r[0];
	p_bmc050->revision_info = a_data_u8r[1];

	/* Function to initialise trim values */
	bmm050api_init_trim_registers();
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_init_trim_registers(void)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char a_data_u8r[2];
	comres = p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_X1, (unsigned char *)&p_bmc050->dig_x1, 1);
	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_Y1, (unsigned char *)&p_bmc050->dig_y1, 1);
	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_X2, (unsigned char *)&p_bmc050->dig_x2, 1);
	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_Y2, (unsigned char *)&p_bmc050->dig_y2, 1);
	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_XY1, (unsigned char *)&p_bmc050->dig_xy1, 1);
	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_XY2, (unsigned char *)&p_bmc050->dig_xy2, 1);

	/* shorts can not be recasted into (unsigned char*)
	 * due to possible mixup between trim data
	 * arrangement and memory arrangement */

	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_Z1_LSB, a_data_u8r, 2);
	p_bmc050->dig_z1 = (BMC050_U16)((((BMC050_U16)((unsigned char)
						a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);

	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_Z2_LSB, a_data_u8r, 2);
	p_bmc050->dig_z2 = (BMC050_S16)((((BMC050_S16)(
						(signed char)a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);

	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_Z3_LSB, a_data_u8r, 2);
	p_bmc050->dig_z3 = (BMC050_S16)((((BMC050_S16)(
						(signed char)a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);

	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_Z4_LSB, a_data_u8r, 2);
	p_bmc050->dig_z4 = (BMC050_S16)((((BMC050_S16)(
						(signed char)a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);

	comres |= p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
			BMC050_DIG_XYZ1_LSB, a_data_u8r, 2);
	a_data_u8r[1] = BMC050_GET_BITSLICE(a_data_u8r[1], BMC050_DIG_XYZ1_MSB);
	p_bmc050->dig_xyz1 = (BMC050_U16)((((BMC050_U16)
					((unsigned char)a_data_u8r[1])) <<
				SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);
	return comres;
}


 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_datarate(unsigned char *data_rate)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_CNTL_DR__REG,
				&v_data_u8r, 1);
		*data_rate = BMC050_GET_BITSLICE(v_data_u8r,
				BMC050_CNTL_DR);
	}
	return comres;
}


 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_datarate(unsigned char data_rate)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_CNTL_DR__REG,
				&v_data1_u8r, 1);
		v_data1_u8r = BMC050_SET_BITSLICE(v_data1_u8r,
				BMC050_CNTL_DR, data_rate);
		comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
				p_bmc050->dev_addr,
				BMC050_CNTL_DR__REG,
				&v_data1_u8r, 1);
	}
	return comres;
}


 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_powermode(unsigned char *mode)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_POWER_CNTL_PCB__REG,
				&v_data_u8r, 1);
		*mode = BMC050_GET_BITSLICE(v_data_u8r,
				BMC050_POWER_CNTL_PCB);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_powermode(unsigned char mode)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_POWER_CNTL_PCB__REG,
				&v_data_u8r, 1);
		v_data_u8r = BMC050_SET_BITSLICE(v_data_u8r,
				BMC050_POWER_CNTL_PCB, mode);
		comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
				p_bmc050->dev_addr,
				BMC050_POWER_CNTL_PCB__REG,
				&v_data_u8r, 1);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_functional_state(
		unsigned char *functional_state)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_CNTL_OPMODE__REG,
				&v_data_u8r, 1);
		*functional_state = BMC050_GET_BITSLICE(
				v_data_u8r, BMC050_CNTL_OPMODE);
	}
	return comres;
}


 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_functional_state(
		unsigned char functional_state)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		switch (functional_state) {
		case BMC050_NORMAL_MODE:
			comres = bmm050api_get_powermode(&v_data1_u8r);
			if (v_data1_u8r == BMC050_OFF) {
				comres = bmm050api_set_powermode(BMC050_ON);
				p_bmc050->delay_msec(
						BMC050_DELAY_SUSPEND_SLEEP);
			}
			{
				comres |= p_bmc050->BMC050_BUS_READ_FUNC(
						p_bmc050->dev_addr,
						BMC050_CNTL_OPMODE__REG,
						&v_data1_u8r, 1);
				v_data1_u8r = BMC050_SET_BITSLICE(
						v_data1_u8r,
						BMC050_CNTL_OPMODE,
						BMC050_NORMAL_MODE);
				comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
						p_bmc050->dev_addr,
						BMC050_CNTL_OPMODE__REG,
						&v_data1_u8r, 1);
			}
			break;
		case BMC050_SUSPEND_MODE:
			comres = bmm050api_set_powermode(BMC050_OFF);
			break;
		case BMC050_FORCED_MODE:
			comres = bmm050api_get_powermode(&v_data1_u8r);
			if (v_data1_u8r == BMC050_OFF) {
				comres = bmm050api_set_powermode(BMC050_ON);
				p_bmc050->delay_msec(
						BMC050_DELAY_SUSPEND_SLEEP);
			}
			comres |= p_bmc050->BMC050_BUS_READ_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_OPMODE__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMC050_SET_BITSLICE(
					v_data1_u8r,
					BMC050_CNTL_OPMODE, BMC050_ON);
			comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_OPMODE__REG,
					&v_data1_u8r, 1);
			break;
		case BMC050_SLEEP_MODE:
			bmm050api_get_powermode(&v_data1_u8r);
			if (v_data1_u8r == BMC050_OFF) {
				comres = bmm050api_set_powermode(BMC050_ON);
				p_bmc050->delay_msec(
						BMC050_DELAY_SUSPEND_SLEEP);
			}
			comres |= p_bmc050->BMC050_BUS_READ_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_OPMODE__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMC050_SET_BITSLICE(
					v_data1_u8r,
					BMC050_CNTL_OPMODE,
					BMC050_SLEEP_MODE);
			comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_OPMODE__REG,
					&v_data1_u8r, 1);
			break;
		default:
			comres = E_BMC050_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_repetitions_XY(
		unsigned char *no_repetitions_xy)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_NO_REPETITIONS_XY,
				&v_data_u8r, 1);
		*no_repetitions_xy = v_data_u8r;
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_repetitions_XY(
		unsigned char no_repetitions_xy)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		v_data_u8r = no_repetitions_xy;
		comres = p_bmc050->BMC050_BUS_WRITE_FUNC(
				p_bmc050->dev_addr,
				BMC050_NO_REPETITIONS_XY,
				&v_data_u8r, 1);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_repetitions_Z(
		unsigned char *no_repetitions_z)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_NO_REPETITIONS_Z,
				&v_data_u8r, 1);
		*no_repetitions_z = v_data_u8r;
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_repetitions_Z(
		unsigned char no_repetitions_z)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		v_data_u8r = no_repetitions_z;
		comres = p_bmc050->BMC050_BUS_WRITE_FUNC(p_bmc050->dev_addr,
				BMC050_NO_REPETITIONS_Z, &v_data_u8r, 1);
	}
	return comres;
}


 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_raw_xyz(struct bmm050api_mdata *mdata)
{
	BMC050_RETURN_FUNCTION_TYPE comres;
	unsigned char a_data_u8r[6];
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
				BMC050_DATAX_LSB, a_data_u8r, 6);

		a_data_u8r[0] = BMC050_GET_BITSLICE(a_data_u8r[0],
				BMC050_DATAX_LSB_VALUEX);
		mdata->datax = (BMC050_S16)((((BMC050_S16)
						((signed char)a_data_u8r[1]))
					<< SHIFT_LEFT_5_POSITION)
				| a_data_u8r[0]);

		a_data_u8r[2] = BMC050_GET_BITSLICE(a_data_u8r[2],
				BMC050_DATAY_LSB_VALUEY);
		mdata->datay = (BMC050_S16)((((BMC050_S16)
						((signed char)a_data_u8r[3]))
					<< SHIFT_LEFT_5_POSITION)
				| a_data_u8r[2]);

		a_data_u8r[4] = BMC050_GET_BITSLICE(a_data_u8r[4],
				BMC050_DATAZ_LSB_VALUEZ);
		mdata->dataz = (BMC050_S16)((((BMC050_S16)
						((signed char)a_data_u8r[5]))
					<< SHIFT_LEFT_7_POSITION)
				| a_data_u8r[4]);
	}
	return comres;
}


 BMC050_RETURN_FUNCTION_TYPE bmm050api_read_mdataXYZ(struct bmm050api_mdata *mdata)
{
	BMC050_RETURN_FUNCTION_TYPE comres;

	unsigned char a_data_u8r[8];

	struct {
		BMC050_S16 raw_dataX;
		BMC050_S16 raw_dataY;
		BMC050_S16 raw_dataZ;
		BMC050_U16 raw_dataR;
	} raw_dataXYZ;

	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
				BMC050_DATAX_LSB, a_data_u8r, 8);

		/* Reading data for X axis */
		a_data_u8r[0] = BMC050_GET_BITSLICE(a_data_u8r[0],
				BMC050_DATAX_LSB_VALUEX);
		raw_dataXYZ.raw_dataX = (BMC050_S16)((((BMC050_S16)
						((signed char)a_data_u8r[1])) <<
					SHIFT_LEFT_5_POSITION) | a_data_u8r[0]);

		/* Reading data for Y axis */
		a_data_u8r[2] = BMC050_GET_BITSLICE(a_data_u8r[2],
				BMC050_DATAY_LSB_VALUEY);
		raw_dataXYZ.raw_dataY = (BMC050_S16)((((BMC050_S16)
						((signed char)a_data_u8r[3])) <<
					SHIFT_LEFT_5_POSITION) | a_data_u8r[2]);

		/* Reading data for Z axis */
		a_data_u8r[4] = BMC050_GET_BITSLICE(a_data_u8r[4],
				BMC050_DATAZ_LSB_VALUEZ);
		raw_dataXYZ.raw_dataZ = (BMC050_S16)((((BMC050_S16)
						((signed char)a_data_u8r[5])) <<
					SHIFT_LEFT_7_POSITION) | a_data_u8r[4]);

		/* Reading data for Resistance*/
		a_data_u8r[6] = BMC050_GET_BITSLICE(a_data_u8r[6],
				BMC050_R_LSB_VALUE);
		raw_dataXYZ.raw_dataR = (BMC050_U16)((((BMC050_U16)
						a_data_u8r[7]) <<
					SHIFT_LEFT_6_POSITION) | a_data_u8r[6]);

		/* Compensation for X axis */
		mdata->datax = bmm050api_compensate_X(raw_dataXYZ.raw_dataX,
				raw_dataXYZ.raw_dataR);

		/* Compensation for Y axis */
		mdata->datay = bmm050api_compensate_Y(raw_dataXYZ.raw_dataY,
				raw_dataXYZ.raw_dataR);

		/* Compensation for Z axis */
		mdata->dataz = bmm050api_compensate_Z(raw_dataXYZ.raw_dataZ,
				raw_dataXYZ.raw_dataR);
	}
	return comres;
}

BMC050_S16 bmm050api_compensate_X(BMC050_S16 mdata_x, BMC050_U16 data_R)
{
	BMC050_S16 inter_retval;
	if (mdata_x != BMC050_FLIP_OVERFLOW_ADCVAL  /* no overflow */
	   ) {
		inter_retval = ((BMC050_S16)(((BMC050_U16)
				((((BMC050_S32)p_bmc050->dig_xyz1) << 14) /
				 (data_R != 0 ? data_R : p_bmc050->dig_xyz1))) -
				((BMC050_U16)0x4000)));
		inter_retval = ((BMC050_S16)((((BMC050_S32)mdata_x) *
				((((((((BMC050_S32)p_bmc050->dig_xy2) *
			      ((((BMC050_S32)inter_retval) *
				((BMC050_S32)inter_retval)) >> 7)) +
			     (((BMC050_S32)inter_retval) *
			      ((BMC050_S32)(((BMC050_S16)p_bmc050->dig_xy1)
			      << 7)))) >> 9) +
			   ((BMC050_S32)0x100000)) *
			  ((BMC050_S32)(((BMC050_S16)p_bmc050->dig_x2) +
			  ((BMC050_S16)0xA0)))) >> 12)) >> 13)) +
			(((BMC050_S16)p_bmc050->dig_x1) << 3);
	} else {
		/* overflow */
		inter_retval = BMC050_OVERFLOW_OUTPUT;
	}
	return inter_retval;
}

BMC050_S16 bmm050api_compensate_Y(BMC050_S16 mdata_y, BMC050_U16 data_R)
{
	BMC050_S16 inter_retval;
	if (mdata_y != BMC050_FLIP_OVERFLOW_ADCVAL  /* no overflow */
	   ) {
		inter_retval = ((BMC050_S16)(((BMC050_U16)(((
			(BMC050_S32)p_bmc050->dig_xyz1) << 14) /
			(data_R != 0 ?
			 data_R : p_bmc050->dig_xyz1))) -
			((BMC050_U16)0x4000)));
		inter_retval = ((BMC050_S16)((((BMC050_S32)mdata_y) *
				((((((((BMC050_S32)
				       p_bmc050->dig_xy2) *
				      ((((BMC050_S32) inter_retval) *
					((BMC050_S32)inter_retval)) >> 7)) +
				     (((BMC050_S32)inter_retval) *
				      ((BMC050_S32)(((BMC050_S16)
				      p_bmc050->dig_xy1) << 7)))) >> 9) +
				   ((BMC050_S32)0x100000)) *
				  ((BMC050_S32)(((BMC050_S16)p_bmc050->dig_y2)
					  + ((BMC050_S16)0xA0))))
				 >> 12)) >> 13)) +
			(((BMC050_S16)p_bmc050->dig_y1) << 3);
	} else {
		/* overflow */
		inter_retval = BMC050_OVERFLOW_OUTPUT;
	}
	return inter_retval;
}

BMC050_S16 bmm050api_compensate_Z(BMC050_S16 mdata_z, BMC050_U16 data_R)
{
	BMC050_S32 retval;
	if ((mdata_z != BMC050_HALL_OVERFLOW_ADCVAL)	/* no overflow */
	   ) {
		retval = (((((BMC050_S32)(mdata_z - p_bmc050->dig_z4)) << 15) -
					((((BMC050_S32)p_bmc050->dig_z3) *
					  ((BMC050_S32)(((BMC050_S16)data_R) -
						  ((BMC050_S16)
						   p_bmc050->dig_xyz1))))>>2)) /
				(p_bmc050->dig_z2 +
				 ((BMC050_S16)(((((BMC050_S32)
					 p_bmc050->dig_z1) *
					 ((((BMC050_S16)data_R) << 1)))+
						 (1<<15))>>16))));
		/* saturate result to +/- 2 mT */
		if (retval > BMC050_POSITIVE_SATURATION_Z) {
			retval =  BMC050_POSITIVE_SATURATION_Z;
		} else {
			if (retval < BMC050_NEGATIVE_SATURATION_Z)
				retval = BMC050_NEGATIVE_SATURATION_Z;
		}
	} else {
		/* overflow */
		retval = BMC050_OVERFLOW_OUTPUT;
	}
	return (BMC050_S16)retval;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_soft_reset(void)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		v_data_u8r = BMC050_ON;

		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_POWER_CNTL_SRST7__REG,
				&v_data_u8r, 1);
		v_data_u8r = BMC050_SET_BITSLICE(v_data_u8r,
				BMC050_POWER_CNTL_SRST7,
				BMC050_SOFT_RESET7_ON);
		comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
				p_bmc050->dev_addr,
				BMC050_POWER_CNTL_SRST7__REG, &v_data_u8r, 1);

		comres |= p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_POWER_CNTL_SRST1__REG,
				&v_data_u8r, 1);
		v_data_u8r = BMC050_SET_BITSLICE(v_data_u8r,
				BMC050_POWER_CNTL_SRST1,
				BMC050_SOFT_RESET1_ON);
		comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
				p_bmc050->dev_addr,
				BMC050_POWER_CNTL_SRST1__REG,
				&v_data_u8r, 1);

		p_bmc050->delay_msec(BMC050_DELAY_SOFTRESET);
	}
	return comres;
}


 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_self_test_XYZ(
		unsigned char *self_testxyz)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char a_data_u8r[5], v_result_u8r = 0x00;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr, BMC050_DATAX_LSB_TESTX__REG,
				a_data_u8r, 5);

		v_result_u8r = BMC050_GET_BITSLICE(a_data_u8r[4],
				BMC050_DATAZ_LSB_TESTZ);

		v_result_u8r = (v_result_u8r << 1);
		v_result_u8r = (v_result_u8r | BMC050_GET_BITSLICE(
					a_data_u8r[2], BMC050_DATAY_LSB_TESTY));

		v_result_u8r = (v_result_u8r << 1);
		v_result_u8r = (v_result_u8r | BMC050_GET_BITSLICE(
					a_data_u8r[0], BMC050_DATAX_LSB_TESTX));

		*self_testxyz = v_result_u8r;
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_selftest(unsigned char selftest)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr, BMC050_CNTL_S_TEST__REG,
				&v_data1_u8r, 1);
		v_data1_u8r = BMC050_SET_BITSLICE(
				v_data1_u8r, BMC050_CNTL_S_TEST, selftest);
		comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
				p_bmc050->dev_addr, BMC050_CNTL_S_TEST__REG,
				&v_data1_u8r, 1);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_perform_advanced_selftest(
		BMC050_S16 *diff_z)
{
	BMC050_RETURN_FUNCTION_TYPE comres;
	BMC050_S16 result_positive, result_negative;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		/* set sleep mode to prepare for forced measurement.
		 * If sensor is off, this will turn it on
		 * and respect needed delays. */
		comres = bmm050api_set_functional_state(BMC050_SLEEP_MODE);

		/* set normal accuracy mode */
		comres |= bmm050api_set_repetitions_Z(BMC050_LOWPOWER_REPZ);
		/* 14 repetitions Z in normal accuracy mode */

		/* disable X, Y channel */
		comres |= bmm050api_set_control_measurement_x(
				BMC050_CHANNEL_DISABLE);
		comres |= bmm050api_set_control_measurement_y(
				BMC050_CHANNEL_DISABLE);

		/* enable positive current and force a
		 * measurement with positive field */
		comres |= bmm050api_set_adv_selftest(
				BMC050_ADVANCED_SELFTEST_POSITIVE);
		comres |= bmm050api_set_functional_state(BMC050_FORCED_MODE);
		/* wait for measurement to complete */
		p_bmc050->delay_msec(4);

		/* read result from positive field measurement */
		comres |= bmm050api_read_mdataZ(&result_positive);

		/* enable negative current and force a
		 * measurement with negative field */
		comres |= bmm050api_set_adv_selftest(
				BMC050_ADVANCED_SELFTEST_NEGATIVE);
		comres |= bmm050api_set_functional_state(BMC050_FORCED_MODE);
		p_bmc050->delay_msec(4); /* wait for measurement to complete */

		/* read result from negative field measurement */
		comres |= bmm050api_read_mdataZ(&result_negative);

		/* turn off self test current */
		comres |= bmm050api_set_adv_selftest(
				BMC050_ADVANCED_SELFTEST_OFF);

		/* enable X, Y channel */
		comres |= bmm050api_set_control_measurement_x(
				BMC050_CHANNEL_ENABLE);
		comres |= bmm050api_set_control_measurement_y(
				BMC050_CHANNEL_ENABLE);

		/* write out difference in positive and negative field.
		 * This should be ~ 200 mT = 3200 LSB */
		*diff_z = (result_positive - result_negative);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_control_measurement_x(
		unsigned char enable_disable)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_SENS_CNTL_CHANNELX__REG,
				&v_data1_u8r, 1);
		v_data1_u8r = BMC050_SET_BITSLICE(v_data1_u8r,
				BMC050_SENS_CNTL_CHANNELX,
				enable_disable);
		comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
				p_bmc050->dev_addr,
				BMC050_SENS_CNTL_CHANNELX__REG,
				&v_data1_u8r, 1);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_control_measurement_y(
		unsigned char enable_disable)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_SENS_CNTL_CHANNELY__REG,
				&v_data1_u8r, 1);
		v_data1_u8r = BMC050_SET_BITSLICE(
				v_data1_u8r,
				BMC050_SENS_CNTL_CHANNELY,
				enable_disable);
		comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
				p_bmc050->dev_addr,
				BMC050_SENS_CNTL_CHANNELY__REG,
				&v_data1_u8r, 1);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_set_adv_selftest(unsigned char adv_selftest)
{
	BMC050_RETURN_FUNCTION_TYPE comres = 0;
	unsigned char v_data1_u8r;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		switch (adv_selftest) {
		case BMC050_ADVANCED_SELFTEST_OFF:
			comres = p_bmc050->BMC050_BUS_READ_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMC050_SET_BITSLICE(
					v_data1_u8r,
					BMC050_CNTL_ADV_ST,
					BMC050_ADVANCED_SELFTEST_OFF);
			comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			break;
		case BMC050_ADVANCED_SELFTEST_POSITIVE:
			comres = p_bmc050->BMC050_BUS_READ_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMC050_SET_BITSLICE(
					v_data1_u8r,
					BMC050_CNTL_ADV_ST,
					BMC050_ADVANCED_SELFTEST_POSITIVE);
			comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			break;
		case BMC050_ADVANCED_SELFTEST_NEGATIVE:
			comres = p_bmc050->BMC050_BUS_READ_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			v_data1_u8r = BMC050_SET_BITSLICE(
					v_data1_u8r,
					BMC050_CNTL_ADV_ST,
					BMC050_ADVANCED_SELFTEST_NEGATIVE);
			comres |= p_bmc050->BMC050_BUS_WRITE_FUNC(
					p_bmc050->dev_addr,
					BMC050_CNTL_ADV_ST__REG,
					&v_data1_u8r, 1);
			break;
		default:
			break;
		}
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_read_mdataZ(BMC050_S16 *mdata_z)
{
	BMC050_RETURN_FUNCTION_TYPE comres;
	BMC050_S16 raw_dataZ;
	BMC050_U16 raw_dataR;
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = bmm050api_get_halldataZ(&raw_dataZ);
		comres |= bmm050api_get_mdataResistance(&raw_dataR);

		/* Compensation for Z axis */
		*mdata_z = bmm050api_compensate_Z(raw_dataZ, raw_dataR);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_halldataZ(BMC050_S16 *mdata_z)
{
	BMC050_RETURN_FUNCTION_TYPE comres;
	unsigned char a_data_u8r[2];
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(p_bmc050->dev_addr,
				BMC050_DATAZ_LSB, a_data_u8r, 2);
		a_data_u8r[0] = BMC050_GET_BITSLICE(a_data_u8r[0],
				BMC050_DATAZ_LSB_VALUEZ);
		*mdata_z = (BMC050_S16)((((BMC050_S16)
						((signed char)a_data_u8r[1]))
					<< SHIFT_LEFT_7_POSITION)
				| a_data_u8r[0]);
	}
	return comres;
}

 BMC050_RETURN_FUNCTION_TYPE bmm050api_get_mdataResistance(
		BMC050_U16 *mdata_resistance)
{
	BMC050_RETURN_FUNCTION_TYPE comres;
	unsigned char a_data_u8r[2];
	if (p_bmc050 == BMC050_NULL) {
		comres = E_BMC050_NULL_PTR;
	} else {
		comres = p_bmc050->BMC050_BUS_READ_FUNC(
				p_bmc050->dev_addr,
				BMC050_R_LSB_VALUE__REG,
				a_data_u8r, 2);
		a_data_u8r[0] = BMC050_GET_BITSLICE(a_data_u8r[0],
				BMC050_R_LSB_VALUE);
		*mdata_resistance = (BMC050_U16)
			((((BMC050_U16)a_data_u8r[1])
			  << SHIFT_LEFT_6_POSITION)
			 | a_data_u8r[0]);
	}
	return comres;
}

/*----------------------------------------------------------------------------*/
/* End of API Section */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#define DEBUG 1
#define BMM050_DEV_NAME         "bmm050"
/*----------------------------------------------------------------------------*/

#define SENSOR_CHIP_ID_BMM 	(0x32)

#define BMM050_DEFAULT_DELAY	100
#define BMM050_BUFSIZE  0x20

#define MSE_TAG					"[Msensor] "
#define MSE_FUN(f)				printk(KERN_ERR MSE_TAG"%s\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)		printk(KERN_ERR MSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)		printk(KERN_INFO MSE_TAG fmt, ##args)

static struct i2c_client *this_client = NULL;

// calibration msensor and orientation data
static int sensor_data[CALIBRATION_DATA_SIZE];
static struct mutex sensor_data_mutex;
static DECLARE_WAIT_QUEUE_HEAD(open_wq);
static DECLARE_WAIT_QUEUE_HEAD(uplink_event_flag_wq);

static int bmm050d_delay = BMM050_DEFAULT_DELAY;

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);

static struct mutex uplink_event_flag_mutex;
/* uplink event flag */
static volatile u8 uplink_event_flag = 0;
/* uplink event flag bitmap */
enum {
	/* active */
	BMMDRV_ULEVT_FLAG_O_ACTIVE = 0x01,
	BMMDRV_ULEVT_FLAG_M_ACTIVE = 0x02,
	/* delay */
	BMMDRV_ULEVT_FLAG_O_DELAY = 0x04,
	BMMDRV_ULEVT_FLAG_M_DELAY = 0x08,

	/* all */
	BMMDRV_ULEVT_FLAG_ALL = 0xff
};

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id bmm050_i2c_id[] = {{BMM050_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_bmm050={ I2C_BOARD_INFO("bmm050", (0x10))};

/*the adapter id will be available in customization*/
//static unsigned short bmm050_force[] = {0x00, BMM050_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const bmm050_forces[] = { bmm050_force, NULL };
//static struct i2c_client_address_data bmm050_addr_data = { .forces = bmm050_forces,};
/*----------------------------------------------------------------------------*/

typedef enum {
    	MMC_FUN_DEBUG  = 0x01,
	MMC_DATA_DEBUG = 0X02,
	MMC_HWM_DEBUG  = 0X04,
	MMC_CTR_DEBUG  = 0X08,
	MMC_I2C_DEBUG  = 0x10,
} MMC_TRC;

/*----------------------------------------------------------------------------*/
struct bmm050_i2c_data {
	struct i2c_client *client;
	struct mag_hw *hw; 
	atomic_t layout;   
	atomic_t trace;
	struct hwmsen_convert   cvt;

	struct bmc050 device;

	u8 op_mode;
	u8 odr;
	u8 rept_xy;
	u8 rept_z;
	
#if defined(CONFIG_HAS_EARLYSUSPEND)    
	struct early_suspend    early_drv;
#endif 
};

/*----------------------------------------------------------------------------*/
static void bmm050_power(struct mag_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)
	{        
		MSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)
		{
			MSE_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "bmm050")) 
			{
				MSE_ERR( "power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "bmm050")) 
			{
				MSE_ERR( "power off fail!!\n");
			}
		}
	}
	power_on = on;
}

// Daemon application save the data
static int ECS_SaveData(int buf[CALIBRATION_DATA_SIZE])
{
#if DEBUG	
	struct bmm050_i2c_data *data = i2c_get_clientdata(this_client);
#endif

	mutex_lock(&sensor_data_mutex);
	switch (buf[0])
	{
	case 2:	/* SENSOR_HANDLE_MAGNETIC_FIELD */
		memcpy(sensor_data+4, buf+1, 4*sizeof(int));	
		break;
	case 3:	/* SENSOR_HANDLE_ORIENTATION */
		memcpy(sensor_data+8, buf+1, 4*sizeof(int));	
		break;
	default:
		break;
	}
	mutex_unlock(&sensor_data_mutex);
	
#if DEBUG
	if(atomic_read(&data->trace) & MMC_HWM_DEBUG)
	{
		MSE_LOG("Get daemon data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			sensor_data[0],sensor_data[1],sensor_data[2],sensor_data[3],
			sensor_data[4],sensor_data[5],sensor_data[6],sensor_data[7],
			sensor_data[8],sensor_data[9],sensor_data[10],sensor_data[11]);
	}	
#endif

	return 0;
}
/*----------------------------------------------------------------------------*/
static int ECS_GetRawData(int data[3])
{
	struct bmm050api_mdata mdata;
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);
	u8 databuf[2] = {BMC050_CONTROL, 0x02};

	bmm050api_read_mdataXYZ(&mdata);
	//data in uT
	data[0] = mdata.datax/16;
	data[1] = mdata.datay/16;
	data[2] = mdata.dataz/16;

	/* measure magnetic field for next sample */
	if (obj->op_mode == BMC050_SUSPEND_MODE)
	{
		/* power on firstly */
		bmm050api_set_powermode(BMC050_ON);
	}
	/* special treat of forced mode
	 * for optimization */
	i2c_master_send(this_client, databuf, 2);
	obj->op_mode = BMC050_SLEEP_MODE;
	
	return 0;
}
/*----------------------------------------------------------------------------*/
/*
static int ECS_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}
*/
/*----------------------------------------------------------------------------*/
static int bmm050_ReadChipInfo(char *buf, int bufsize)
{
	if((!buf)||(bufsize <= BMM050_BUFSIZE -1))
	{
		return -1;
	}
	if(!this_client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "bmm050 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static void bmm050_SetPowerMode(struct i2c_client *client, bool enable)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(client);
	//u8 power_mode;

	if (enable == FALSE)
	{
		if (bmm050api_set_functional_state(BMC050_SUSPEND_MODE) != 0)
		{
			MSE_ERR("fail to suspend sensor");
			return;
		}
	}
	else
	{
		if (obj->op_mode == BMC050_SUSPEND_MODE)
		{
			obj->op_mode = BMC050_SLEEP_MODE;
		}
		bmm050_restore_hw_cfg(client);
	}
}

/*----------------------------------------------------------------------------*/
/* Driver Attributes Functions Section */
static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[20];
	sprintf(strbuf, "bmm050d");
	return sprintf(buf, "%s", strbuf);		
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[BMM050_BUFSIZE];
	bmm050_ReadChipInfo(strbuf, BMM050_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	int sensordata[3];
	char strbuf[BMM050_BUFSIZE];
	
	ECS_GetRawData(sensordata);
	sprintf(strbuf, "%d %d %d\n", sensordata[0],sensordata[1],sensordata[2]);
	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	int tmp[3];
	char strbuf[BMM050_BUFSIZE];
	tmp[0] = sensor_data[0] * CONVERT_O / CONVERT_O_DIV;				
	tmp[1] = sensor_data[1] * CONVERT_O / CONVERT_O_DIV;
	tmp[2] = sensor_data[2] * CONVERT_O / CONVERT_O_DIV;
	sprintf(strbuf, "%d, %d, %d\n", tmp[0],tmp[1], tmp[2]);
		
	return sprintf(buf, "%s\n", strbuf);;           
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct bmm050_i2c_data *data = i2c_get_clientdata(this_client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = this_client;  
	struct bmm050_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;

	if(1 == sscanf(buf, "%d", &layout))
	{
		atomic_set(&data->layout, layout);
		if(!hwmsen_get_convert(layout, &data->cvt))
		{
			MSE_ERR( "HWMSEN_GET_CONVERT function error!\r\n");
		}
		else if(!hwmsen_get_convert(data->hw->direction, &data->cvt))
		{
			MSE_ERR( "invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		}
		else
		{
			MSE_ERR( "invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	}
	else
	{
		MSE_ERR( "invalid format = '%s'\n", buf);
	}
	
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	struct bmm050_i2c_data *data = i2c_get_clientdata(this_client);
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
	
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);
	if(NULL == obj)
	{
		MSE_ERR( "bmm050_i2c_data is null!!\n");
		return 0;
	}	
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);
	int trace;
	if(NULL == obj)
	{
		MSE_ERR( "bmm050_i2c_data is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}
	else 
	{
		MSE_ERR( "invalid content: '%s', length = %d\n", buf, count);
	}
	
	return count;    
}
/*----------------------------------------------------------------------------*/
#define BMM050_AXIS_X          0
#define BMM050_AXIS_Y          1
#define BMM050_AXIS_Z          2
static ssize_t show_cpsdata_value(struct device_driver *ddri, char *buf)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);
	struct bmm050api_mdata mdata;
	s16 mag[3];

	bmm050api_read_mdataXYZ(&mdata);
	
	/*remap coordinate*/
	mag[obj->cvt.map[BMM050_AXIS_X]] = obj->cvt.sign[BMM050_AXIS_X]*mdata.datax;
	mag[obj->cvt.map[BMM050_AXIS_Y]] = obj->cvt.sign[BMM050_AXIS_Y]*mdata.datay;
	mag[obj->cvt.map[BMM050_AXIS_Z]] = obj->cvt.sign[BMM050_AXIS_Z]*mdata.dataz;

	return sprintf(buf, "%hd %hd %hd\n", mag[BMM050_AXIS_X], mag[BMM050_AXIS_Y], mag[BMM050_AXIS_Z]);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cpsopmode_value(struct device_driver *ddri, char *buf)
{
	//struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);
	u8 op_mode = 0xff;
	u8 power_mode;

	bmm050api_get_powermode(&power_mode);
	if (power_mode) 
	{
		bmm050api_get_functional_state(&op_mode);
	} 
	else 
	{
		op_mode = BMC050_SUSPEND_MODE;
	}

	MSE_LOG("op_mode: %d", op_mode);

	return sprintf(buf, "%d\n", op_mode);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cpsopmode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);
	int err;	
	long op_mode = 0;
	
	err = strict_strtoul(buf, 10, &op_mode);
	/*
	if (((unsigned char)op_mode > 3) || (op_mode == obj->op_mode))
	{
		return -EINVAL;
	}*/
	if((unsigned char)op_mode > 3)
	{
		return -EINVAL;
	}
	if(op_mode == obj->op_mode)
	{
		return count;
	}


	if (BMC050_FORCED_MODE == op_mode) 
	{
		u8 databuf[2] = {BMC050_CONTROL, 0x02};

		if (obj->op_mode == BMC050_SUSPEND_MODE)
		{
			/* power on firstly */
			bmm050api_set_powermode(BMC050_ON);
		}
		/* special treat of forced mode
		 * for optimization */
		i2c_master_send(this_client, databuf, 2);
		obj->op_mode = BMC050_SLEEP_MODE;
	} 
	else 
	{
		bmm050api_set_functional_state((unsigned char)op_mode);
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

	bmm050api_get_powermode(&power_mode);
	if (power_mode) 
	{
		err = bmm050api_get_repetitions_XY(&data);
	} 
	else 
	{
		err = -EIO;
	}

	if (err)
		return err;

	return sprintf(buf, "%d\n", data);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cpsreptxy_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);
	unsigned long tmp = 0;
	int err;
	u8 data;
	u8 power_mode;

	err = strict_strtoul(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	bmm050api_get_powermode(&power_mode);
	if (power_mode) 
	{
		err = bmm050api_set_repetitions_XY(data);
		if (!err) 
		{
			//mdelay(BMM_I2C_WRITE_DELAY_TIME);
			obj->rept_xy = data;
		}
	} 
	else 
	{
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

	bmm050api_get_powermode(&power_mode);
	if (power_mode) 
	{
		err = bmm050api_get_repetitions_Z(&data);
	} 
	else 
	{
		err = -EIO;
	}

	if (err)
		return err;

	return sprintf(buf, "%d\n", data);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cpsreptz_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);
	unsigned long tmp = 0;
	int err;
	u8 data;
	u8 power_mode;

	err = strict_strtoul(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	bmm050api_get_powermode(&power_mode);
	if (power_mode) 
	{
		err = bmm050api_set_repetitions_Z(data);
		if (!err) 
		{
			//mdelay(BMM_I2C_WRITE_DELAY_TIME);
			obj->rept_z = data;
		}
	} 
	else 
	{
		err = -EIO;
	}

	if (err)
		return err;

	return count;
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(daemon,      S_IRUGO, show_daemon_name, NULL);
static DRIVER_ATTR(chipinfo,    S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata,  S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(posturedata, S_IRUGO, show_posturedata_value, NULL);
static DRIVER_ATTR(layout,      S_IRUGO | S_IWUSR, show_layout_value, store_layout_value);
static DRIVER_ATTR(status,      S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(trace,       S_IRUGO | S_IWUSR, show_trace_value, store_trace_value);
static DRIVER_ATTR(cpsdata, S_IWUSR | S_IRUGO, show_cpsdata_value,    NULL);
static DRIVER_ATTR(cpsopmode,      S_IRUGO | S_IWUSR, show_cpsopmode_value, store_cpsopmode_value);
static DRIVER_ATTR(cpsreptxy,      S_IRUGO | S_IWUSR, show_cpsreptxy_value, store_cpsreptxy_value);
static DRIVER_ATTR(cpsreptz,      S_IRUGO | S_IWUSR, show_cpsreptz_value, store_cpsreptz_value);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *bmm050_attr_list[] = {
	&driver_attr_daemon,
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
};
/*----------------------------------------------------------------------------*/
static int bmm050_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(bmm050_attr_list)/sizeof(bmm050_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, bmm050_attr_list[idx])))
		{            
			MSE_ERR( "driver_create_file (%s) = %d\n", bmm050_attr_list[idx]->attr.name, err);
			break;
		}
	}
	
	return err;
}
/*----------------------------------------------------------------------------*/
static int bmm050_delete_attr(struct device_driver *driver)
{
	int idx;
	int num = (int)(sizeof(bmm050_attr_list)/sizeof(bmm050_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, bmm050_attr_list[idx]);
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int bmm050_open(struct inode *inode, struct file *file)
{    
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);    
	int ret = -1;	
	
	if(atomic_read(&obj->trace) & MMC_CTR_DEBUG)
	{
		MSE_LOG("Open device node:bmm050\n");
	}
	ret = nonseekable_open(inode, file);
	
	return ret;
}
/*----------------------------------------------------------------------------*/
static int bmm050_release(struct inode *inode, struct file *file)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(this_client);

	if(atomic_read(&obj->trace) & MMC_CTR_DEBUG)
	{
		MSE_LOG("Release device node:bmm050\n");
	}	
	return 0;
}
/*----------------------------------------------------------------------------*/
#define BMM_IOC_GET_EVENT_FLAG	ECOMPASS_IOC_GET_OPEN_STATUS
//static int bmm050_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)
static long bmm050_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
		
	/* NOTE: In this function the size of "char" should be 1-byte. */
	char buff[BMM050_BUFSIZE];				/* for chip information */

	int value[CALIBRATION_DATA_SIZE];			/* for SET_YPR */
	int status; 				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
	int vec[3] = {0};	
	struct bmm050_i2c_data *clientdata = i2c_get_clientdata(this_client);
	hwm_sensor_data osensor_data;
	uint32_t enable;

	switch (cmd)
	{
		case BMM_IOC_GET_EVENT_FLAG:	// used by daemon only
			/* block if no event updated */
			wait_event_interruptible(uplink_event_flag_wq, (uplink_event_flag != 0));
			mutex_lock(&uplink_event_flag_mutex);
			status = uplink_event_flag;
			mutex_unlock(&uplink_event_flag_mutex);
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			break;
			
		case ECOMPASS_IOC_GET_DELAY:			//used by daemon
			if(copy_to_user(argp, &bmm050d_delay, sizeof(bmm050d_delay)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			/* clear the flag */
			mutex_lock(&uplink_event_flag_mutex);
			if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_M_DELAY) != 0)
			{
				uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_M_DELAY;
			}
			else if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_O_DELAY) != 0)
			{
				uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_O_DELAY;
			}
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
			break;		
			
		case ECOMPASS_IOC_SET_YPR:				//used by daemon
			if(argp == NULL)
			{
				MSE_ERR("invalid argument.");
				return -EINVAL;
			}
			if(copy_from_user(value, argp, sizeof(value)))
			{
				MSE_ERR("copy_from_user failed.");
				return -EFAULT;
			}
			ECS_SaveData(value);
			break;

		case ECOMPASS_IOC_GET_MFLAG:		//used by daemon
			sensor_status = atomic_read(&m_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			/* clear the flag */
			mutex_lock(&uplink_event_flag_mutex);
			if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_M_ACTIVE) != 0)
			{
				uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_M_ACTIVE;
			}
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
			break;
			
		case ECOMPASS_IOC_GET_OFLAG:		//used by daemon
			sensor_status = atomic_read(&o_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			/* clear the flag */
			mutex_lock(&uplink_event_flag_mutex);
			if ((uplink_event_flag & BMMDRV_ULEVT_FLAG_O_ACTIVE) != 0)
			{
				uplink_event_flag &= ~BMMDRV_ULEVT_FLAG_O_ACTIVE;
			}
			mutex_unlock(&uplink_event_flag_mutex);
			/* wake up the wait queue */
			wake_up(&uplink_event_flag_wq);
			break;			
		                

		case MSENSOR_IOCTL_READ_CHIPINFO:		//reserved
			if(argp == NULL)
			{
				MSE_ERR( "IO parameter pointer is NULL!\r\n");
				break;
			}
			
			bmm050_ReadChipInfo(buff, BMM050_BUFSIZE);
			if(copy_to_user(argp, buff, strlen(buff)+1))
			{
				return -EFAULT;
			}                
			break;

		case MSENSOR_IOCTL_READ_SENSORDATA:		//used by MTK ftm or engineering mode
			if(argp == NULL)
			{
				MSE_ERR( "IO parameter pointer is NULL!\r\n");
				break;    
			}
			ECS_GetRawData(vec);			
			sprintf(buff, "%x %x %x", vec[0], vec[1], vec[2]);
			if(copy_to_user(argp, buff, strlen(buff)+1))
			{
				return -EFAULT;
			}                
			break;

		case ECOMPASS_IOC_GET_LAYOUT:		//used by daemon ?
			status = atomic_read(&clientdata->layout);
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			break;

		case MSENSOR_IOCTL_SENSOR_ENABLE:		//used by MTK ftm
			if(argp == NULL)
			{
				MSE_ERR( "IO parameter pointer is NULL!\r\n");
				break;
			}
			if(copy_from_user(&enable, argp, sizeof(enable)))
			{
				MSE_ERR("copy_from_user failed.");
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
				
				/* set the flag */
				mutex_lock(&uplink_event_flag_mutex);
				uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
				mutex_unlock(&uplink_event_flag_mutex);
				/* wake up the wait queue */
				wake_up(&uplink_event_flag_wq);
			}
			break;
			
		case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:		//used by MTK ftm	
			if(argp == NULL)
			{
				MSE_ERR( "IO parameter pointer is NULL!\r\n");
				break;    
			}
			
			mutex_lock(&sensor_data_mutex);
				
			osensor_data.values[0] = sensor_data[8];
			osensor_data.values[1] = sensor_data[9];
			osensor_data.values[2] = sensor_data[10];
			osensor_data.status = sensor_data[11];
			osensor_data.value_divide = CONVERT_O_DIV;
					
			mutex_unlock(&sensor_data_mutex);
			if(copy_to_user(argp, &osensor_data, sizeof(hwm_sensor_data)))
			{
				return -EFAULT;
			} 
			break;
			
		default:
			MSE_ERR( "%s not supported = 0x%04x", __FUNCTION__, cmd);
			return -ENOIOCTLCMD;
			break;		
		}

	return 0;    
}
/*----------------------------------------------------------------------------*/
static struct file_operations bmm050_fops = {
	//.owner = THIS_MODULE,
	.open = bmm050_open,
	.release = bmm050_release,
	.unlocked_ioctl = bmm050_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice bmm050_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &bmm050_fops,
};
/*----------------------------------------------------------------------------*/
static int bmm050_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* msensor_data;
	
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct bmm050_i2c_data *data = i2c_get_clientdata(client);
#endif
	
#if DEBUG
	if(atomic_read(&data->trace) & MMC_FUN_DEBUG)
	{
		MSE_FUN();
	}	
#endif
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR( "Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 20)
				{
					bmm050d_delay = 20;
				}
				else
				{
					bmm050d_delay = value;
				}
				/* set the flag */
				mutex_lock(&uplink_event_flag_mutex);
				uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_DELAY;
				mutex_unlock(&uplink_event_flag_mutex);
				/* wake up the wait queue */
				wake_up(&uplink_event_flag_wq);
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR( "Enable sensor parameter error!\n");
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

				bmm050_SetPowerMode(this_client, (value == 1));

				/* set the flag */
				mutex_lock(&uplink_event_flag_mutex);
				uplink_event_flag |= BMMDRV_ULEVT_FLAG_M_ACTIVE;
				mutex_unlock(&uplink_event_flag_mutex);
				/* wake up the wait queue */
				wake_up(&uplink_event_flag_wq);
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR( "get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				msensor_data = (hwm_sensor_data *)buff_out;
				mutex_lock(&sensor_data_mutex);
				
				msensor_data->values[0] = sensor_data[4];
				msensor_data->values[1] = sensor_data[5];
				msensor_data->values[2] = sensor_data[6];
				msensor_data->status = sensor_data[7];
				msensor_data->value_divide = CONVERT_M_DIV;
					
				mutex_unlock(&sensor_data_mutex);
#if DEBUG
				if(atomic_read(&data->trace) & MMC_HWM_DEBUG)
				{
					MSE_LOG("Hwm get m-sensor data: %d, %d, %d. divide %d, status %d!\n",
						msensor_data->values[0],msensor_data->values[1],msensor_data->values[2],
						msensor_data->value_divide,msensor_data->status);
				}	
#endif
			}
			break;
		default:
			MSE_ERR( "msensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/
int bmm050_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* osensor_data;	
#if DEBUG	
	struct i2c_client *client = this_client;  
	struct bmm050_i2c_data *data = i2c_get_clientdata(client);
#endif
	
#if DEBUG
	if(atomic_read(&data->trace) & MMC_FUN_DEBUG)
	{
		MSE_FUN();
	}	
#endif

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR( "Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 20)
				{
					bmm050d_delay = 20;
				}
				else
				{
					bmm050d_delay = value;
				}
				/* set the flag */
				mutex_lock(&uplink_event_flag_mutex);
				uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_DELAY;
				mutex_unlock(&uplink_event_flag_mutex);
				/* wake up the wait queue */
				wake_up(&uplink_event_flag_wq);
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR( "Enable sensor parameter error!\n");
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
				
				/* set the flag */
				mutex_lock(&uplink_event_flag_mutex);
				uplink_event_flag |= BMMDRV_ULEVT_FLAG_O_ACTIVE;
				mutex_unlock(&uplink_event_flag_mutex);
				/* wake up the wait queue */
				wake_up(&uplink_event_flag_wq);
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR( "get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				osensor_data = (hwm_sensor_data *)buff_out;
				mutex_lock(&sensor_data_mutex);
				
				osensor_data->values[0] = sensor_data[8];
				osensor_data->values[1] = sensor_data[9];
				osensor_data->values[2] = sensor_data[10];
				osensor_data->status = sensor_data[11];
				osensor_data->value_divide = CONVERT_O_DIV;
					
				mutex_unlock(&sensor_data_mutex);
#if DEBUG
				if(atomic_read(&data->trace) & MMC_HWM_DEBUG)
				{
					MSE_LOG("Hwm get o-sensor data: %d, %d, %d. divide %d, status %d!\n",
						osensor_data->values[0],osensor_data->values[1],osensor_data->values[2],
						osensor_data->value_divide,osensor_data->status);
				}	
#endif
			}
			break;
		default:
			MSE_ERR( "osensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/
static void bmm050_restore_hw_cfg(struct i2c_client *client)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(client);

	if (obj->op_mode > 3)
	{
		obj->op_mode = BMC050_SLEEP_MODE;
	}
	bmm050api_set_functional_state(obj->op_mode);

	bmm050api_set_datarate(obj->odr);
	//mdelay(BMM_I2C_WRITE_DELAY_TIME);

	bmm050api_set_repetitions_XY(obj->rept_xy);
	//mdelay(BMM_I2C_WRITE_DELAY_TIME);

	bmm050api_set_repetitions_Z(obj->rept_z);
	//mdelay(BMM_I2C_WRITE_DELAY_TIME);
}

/*----------------------------------------------------------------------------*/
#ifndef	CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int bmm050_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(client)
	    

	if(msg.event == PM_EVENT_SUSPEND)
	{
		bmm050_SetPowerMode(obj->client, FALSE);
		bmm050_power(obj->hw, 0);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int bmm050_resume(struct i2c_client *client)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(client)

	bmm050_power(obj->hw, 1);
	bmm050_SetPowerMode(obj->client, TRUE);
	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void bmm050_early_suspend(struct early_suspend *h) 
{
	struct bmm050_i2c_data *obj = container_of(h, struct bmm050_i2c_data, early_drv);   
	//u8 power_mode;
	//int err = 0;

	if(NULL == obj)
	{
		MSE_ERR( "null pointer!!\n");
		return;
	}

	bmm050_SetPowerMode(obj->client, FALSE);
	bmm050_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void bmm050_late_resume(struct early_suspend *h)
{
	struct bmm050_i2c_data *obj = container_of(h, struct bmm050_i2c_data, early_drv);         

	
	if(NULL == obj)
	{
		MSE_ERR( "null pointer!!\n");
		return;
	}

	bmm050_power(obj->hw, 1);
	bmm050_SetPowerMode(obj->client, TRUE);
}
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
#define BMM_MAX_RETRY_WAKEUP (5)
#define BMM_I2C_WRITE_DELAY_TIME (1)
static int bmm050_wakeup(struct i2c_client *client)
{
	int err = 0;
	int try_times = BMM_MAX_RETRY_WAKEUP;
	u8 data[2] = {BMC050_POWER_CNTL, 0x01};
	u8 dummy;

	MSE_LOG("waking up the chip...");

	while (try_times) {
		err = i2c_master_send(client, data, 2);
		mdelay(BMM_I2C_WRITE_DELAY_TIME);
		dummy = 0;
		err = hwmsen_read_block(client, BMC050_POWER_CNTL, &dummy, 1);
		if (data[1] == dummy)
		{
			break;
		}
		try_times--;
	}

	MSE_LOG("wake up result: %s, tried times: %d",
			(try_times > 0) ? "succeed" : "fail",
			BMM_MAX_RETRY_WAKEUP - try_times + 1);

	err = (try_times > 0) ? 0 : -1;

	return err;
}
/*----------------------------------------------------------------------------*/
static int bmm050_checkchipid(struct i2c_client *client)
{
	//int err = 0;
	u8 chip_id = 0;

   MSE_ERR( "ivy bmm050_checkchipid22");
	hwmsen_read_block(client, BMC050_CHIP_ID, &chip_id, 1);
	MSE_LOG("read chip id result: %#x", chip_id);

	if ((chip_id & 0xff) != SENSOR_CHIP_ID_BMM)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}
/*----------------------------------------------------------------------------*/
static char bmm050_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	return hwmsen_read_block(this_client, reg_addr, data, len);
}
/*----------------------------------------------------------------------------*/
static char bmm050_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	u8 buff[BMM050_BUFSIZE + 1];

	if (len > BMM050_BUFSIZE)
	{
		return -1;
	}

	buff[0] = reg_addr;
	memcpy(buff+1, data, len);
	if (i2c_master_send(this_client, buff, len+1) != (len+1))
	{
		/* I2C transfer error */
		return -EIO;
	}
	else
	{
		return 0;
	}
}
/*----------------------------------------------------------------------------*/
static void bmm050_delay(u32 msec)
{
	mdelay(msec);
}
/*----------------------------------------------------------------------------*/
static int bmm050_init_client(struct i2c_client *client)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	MSE_ERR( "ivy bmm050_init_client");

	res = bmm050_wakeup(client);
	if (res < 0)
	{
		return res;
	}
	MSE_ERR( "ivy bmm050_checkchipid");
	res = bmm050_checkchipid(client); 
	if(res < 0)
	{
		return res;
	}	
	MSE_LOG("check chip ID ok");

	//bmm050 api init
	obj->device.bus_read = bmm050_i2c_read_wrapper;
	obj->device.bus_write = bmm050_i2c_write_wrapper;
	obj->device.delay_msec = bmm050_delay;
	bmm050api_init(&obj->device);

	/* now it's power on which is considered as resuming from suspend */
	obj->op_mode = BMC050_SUSPEND_MODE;
	obj->odr = BMC050_REGULAR_DR;
	obj->rept_xy = BMC050_REGULAR_REPXY;
	obj->rept_z = BMC050_REGULAR_REPZ;

	res = bmm050api_set_functional_state(BMC050_SUSPEND_MODE);
	if (res) 
	{
		return -EIO;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
//static int bmm050_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
//{    
//	strcpy(info->type, BMM050_DEV_NAME);
//	return 0;
//}

/*----------------------------------------------------------------------------*/
#if 0
static struct platform_driver bmm050_sensor_driver = {
	.probe      = bmm050_probe,
	.remove     = bmm050_remove,    
	.driver     = {
		.name  = "msensor",
		//.owner = THIS_MODULE,
	}
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id bmm050_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver bmm050_sensor_driver =
{
	.probe      = bmm050_probe,
	.remove     = bmm050_remove,    
	.driver     = 
	{
		.name = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = bmm050_of_match,
		#endif
	}
};

/*----------------------------------------------------------------------------*/
static int bmm050_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct bmm050_i2c_data *data;
	int err = 0;
	struct hwmsen_object sobj_m, sobj_o;

	if(!(data = kmalloc(sizeof(struct bmm050_i2c_data), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct bmm050_i2c_data));

	data->hw = get_cust_mag_hw();	
	if((err = hwmsen_get_convert(data->hw->direction, &data->cvt)))
	{
		MSE_ERR("invalid direction: %d\n", data->hw->direction);
		goto exit;
	}
	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0);

	mutex_init(&sensor_data_mutex);
	mutex_init(&uplink_event_flag_mutex);
	
	init_waitqueue_head(&open_wq);
	init_waitqueue_head(&uplink_event_flag_wq);

	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);
	
	this_client = new_client;	

	//initial client
	if ((err = bmm050_init_client(this_client)))
	{
		MSE_ERR("fail to initialize client");
		goto exit_client_failed;
	}

	/* Register sysfs attribute */
	if((err = bmm050_create_attr(&bmm050_sensor_driver.driver)))
	{
		MSE_ERR("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}
	
	if((err = misc_register(&bmm050_device)))
	{
		MSE_ERR("bmm050_device register failed\n");
		goto exit_misc_device_register_failed;	
	}    

	sobj_m.self = data;
	sobj_m.polling = 1;
	sobj_m.sensor_operate = bmm050_operate;
	if((err = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
	{
		MSE_ERR( "attach fail = %d\n", err);
		goto exit_kfree;
	}
	
	sobj_o.self = data;
	sobj_o.polling = 1;
	sobj_o.sensor_operate = bmm050_orientation_operate;
	if((err = hwmsen_attach(ID_ORIENTATION, &sobj_o)))
	{
		MSE_ERR( "attach fail = %d\n", err);
		goto exit_kfree;
	}
	
#if CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	data->early_drv.suspend  = bmm050_early_suspend,
	data->early_drv.resume   = bmm050_late_resume,    
	register_early_suspend(&data->early_drv);
#endif

	MSE_LOG("%s: OK\n", __func__);
	return 0;

	exit_client_failed:
	exit_sysfs_create_group_failed:	
	exit_misc_device_register_failed:
	exit_kfree:
	kfree(data);
	exit:
	MSE_ERR( "%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int bmm050_i2c_remove(struct i2c_client *client)
{
	struct bmm050_i2c_data *obj = i2c_get_clientdata(client);
	
	if(bmm050_delete_attr(&bmm050_sensor_driver.driver))
	{
		MSE_ERR( "bmm050_delete_attr fail");
	}
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&obj->early_drv);
#endif
	bmm050api_set_functional_state(BMC050_SUSPEND_MODE);
	this_client = NULL;
	i2c_unregister_device(client);
	kfree(obj);	
	misc_deregister(&bmm050_device);    
	return 0;
}
/*----------------------------------------------------------------------------*/
static struct i2c_driver bmm050_i2c_driver = {
	.driver = {
	//	.owner = THIS_MODULE, 
		.name  = BMM050_DEV_NAME,
	},
	.probe      = bmm050_i2c_probe,
	.remove     = bmm050_i2c_remove,
//	.detect     = bmm050_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend    = bmm050_suspend,
	.resume     = bmm050_resume,
#endif 
	.id_table = bmm050_i2c_id,
//	.address_data = &bmm050_addr_data,
};
/*----------------------------------------------------------------------------*/
static int bmm050_probe(struct platform_device *pdev) 
{
	struct mag_hw *hw = get_cust_mag_hw();
    MSE_ERR( "bmm050_probe");
	bmm050_power(hw, 1);
	//bmm050_force[0] = hw->i2c_num;
	if(i2c_add_driver(&bmm050_i2c_driver))
	{
		MSE_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int bmm050_remove(struct platform_device *pdev)
{
	struct mag_hw *hw = get_cust_mag_hw();
 
	i2c_del_driver(&bmm050_i2c_driver);
	bmm050_power(hw, 0);    
	return 0;
}
/*----------------------------------------------------------------------------*/
static int __init bmm050_init(void)
{
    	struct mag_hw *hw = get_cust_mag_hw();
	MSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
    	i2c_register_board_info(hw->i2c_num, &i2c_bmm050, 1);
	if(platform_driver_register(&bmm050_sensor_driver))
	{
		MSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit bmm050_exit(void)
{	
	platform_driver_unregister(&bmm050_sensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(bmm050_init);
module_exit(bmm050_exit);

MODULE_AUTHOR("hongji.zhou@bosch-sensortec.com");
MODULE_DESCRIPTION("bmm050 compass driver");
MODULE_LICENSE("GPL");


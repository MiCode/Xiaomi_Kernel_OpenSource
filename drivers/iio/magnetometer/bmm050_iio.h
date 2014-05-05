/*!
 * @section LICENSE
 * (C) Copyright 2011~2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename    bmm050_iio.h
 * @date        "Thu Apr 24 10:40:36 2014 +0800"
 * @id          "6d0d027"
 * @version     v1.0
 * @brief	BMM050 Linux IIO Driver Head File
 */
#ifndef __BMM050_IIO_H__
#define __BMM050_IIO_H__

#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>


#define BMM050_BUS_WRITE_FUNC(\
		device_addr, register_addr, register_data, wr_len)\
	bus_write(device_addr, register_addr, register_data, wr_len)

#define BMM050_BUS_READ_FUNC(device_addr, register_addr, register_data, rd_len)\
	bus_read(device_addr, register_addr, register_data, rd_len)

#define BMM050_DELAY_POWEROFF_SUSPEND      1
#define BMM050_DELAY_SUSPEND_SLEEP         2
#define BMM050_DELAY_SLEEP_ACTIVE          1
#define BMM050_DELAY_ACTIVE_SLEEP          1
#define BMM050_DELAY_SLEEP_SUSPEND         1
#define BMM050_DELAY_ACTIVE_SUSPEND        1
#define BMM050_DELAY_SLEEP_POWEROFF        1
#define BMM050_DELAY_ACTIVE_POWEROFF       1
#define BMM050_DELAY_SETTLING_TIME         2

#define BMM050_I2C_ADDRESS                 0x10

/*General Info datas*/
#define BMM050_SOFT_RESET7_ON              1
#define BMM050_SOFT_RESET1_ON              1
#define BMM050_SOFT_RESET7_OFF             0
#define BMM050_SOFT_RESET1_OFF             0
#define BMM050_DELAY_SOFTRESET             1

/* Fixed Data Registers */
#define BMM050_CHIP_ID                     0x40

/* Data Registers */
#define BMM050_DATAX_LSB                   0x42
#define BMM050_DATAX_MSB                   0x43
#define BMM050_DATAY_LSB                   0x44
#define BMM050_DATAY_MSB                   0x45
#define BMM050_DATAZ_LSB                   0x46
#define BMM050_DATAZ_MSB                   0x47
#define BMM050_R_LSB                       0x48
#define BMM050_R_MSB                       0x49

/* Status Registers */
#define BMM050_INT_STAT                    0x4A

/* Control Registers */
#define BMM050_POWER_CNTL                  0x4B
#define BMM050_CONTROL                     0x4C
#define BMM050_INT_CNTL                    0x4D
#define BMM050_SENS_CNTL                   0x4E
#define BMM050_LOW_THRES                   0x4F
#define BMM050_HIGH_THRES                  0x50
#define BMM050_NO_REPETITIONS_XY           0x51
#define BMM050_NO_REPETITIONS_Z            0x52

/* Trim Extended Registers */
#define BMM050_DIG_X1                      0x5D
#define BMM050_DIG_Y1                      0x5E
#define BMM050_DIG_Z4_LSB                  0x62
#define BMM050_DIG_Z4_MSB                  0x63
#define BMM050_DIG_X2                      0x64
#define BMM050_DIG_Y2                      0x65
#define BMM050_DIG_Z2_LSB                  0x68
#define BMM050_DIG_Z2_MSB                  0x69
#define BMM050_DIG_Z1_LSB                  0x6A
#define BMM050_DIG_Z1_MSB                  0x6B
#define BMM050_DIG_XYZ1_LSB                0x6C
#define BMM050_DIG_XYZ1_MSB                0x6D
#define BMM050_DIG_Z3_LSB                  0x6E
#define BMM050_DIG_Z3_MSB                  0x6F
#define BMM050_DIG_XY2                     0x70
#define BMM050_DIG_XY1                     0x71


/* Data X LSB Regsiter */
#define BMM050_DATAX_LSB_VALUEX__POS        3
#define BMM050_DATAX_LSB_VALUEX__LEN        5
#define BMM050_DATAX_LSB_VALUEX__MSK        0xF8
#define BMM050_DATAX_LSB_VALUEX__REG        BMM050_DATAX_LSB

#define BMM050_DATAX_LSB_TESTX__POS         0
#define BMM050_DATAX_LSB_TESTX__LEN         1
#define BMM050_DATAX_LSB_TESTX__MSK         0x01
#define BMM050_DATAX_LSB_TESTX__REG         BMM050_DATAX_LSB

/* Data Y LSB Regsiter */
#define BMM050_DATAY_LSB_VALUEY__POS        3
#define BMM050_DATAY_LSB_VALUEY__LEN        5
#define BMM050_DATAY_LSB_VALUEY__MSK        0xF8
#define BMM050_DATAY_LSB_VALUEY__REG        BMM050_DATAY_LSB

#define BMM050_DATAY_LSB_TESTY__POS         0
#define BMM050_DATAY_LSB_TESTY__LEN         1
#define BMM050_DATAY_LSB_TESTY__MSK         0x01
#define BMM050_DATAY_LSB_TESTY__REG         BMM050_DATAY_LSB

/* Data Z LSB Regsiter */
#define BMM050_DATAZ_LSB_VALUEZ__POS        1
#define BMM050_DATAZ_LSB_VALUEZ__LEN        7
#define BMM050_DATAZ_LSB_VALUEZ__MSK        0xFE
#define BMM050_DATAZ_LSB_VALUEZ__REG        BMM050_DATAZ_LSB

#define BMM050_DATAZ_LSB_TESTZ__POS         0
#define BMM050_DATAZ_LSB_TESTZ__LEN         1
#define BMM050_DATAZ_LSB_TESTZ__MSK         0x01
#define BMM050_DATAZ_LSB_TESTZ__REG         BMM050_DATAZ_LSB

/* Hall Resistance LSB Regsiter */
#define BMM050_R_LSB_VALUE__POS             2
#define BMM050_R_LSB_VALUE__LEN             6
#define BMM050_R_LSB_VALUE__MSK             0xFC
#define BMM050_R_LSB_VALUE__REG             BMM050_R_LSB

#define BMM050_DATA_RDYSTAT__POS            0
#define BMM050_DATA_RDYSTAT__LEN            1
#define BMM050_DATA_RDYSTAT__MSK            0x01
#define BMM050_DATA_RDYSTAT__REG            BMM050_R_LSB

/* Interupt Status Register */
#define BMM050_INT_STAT_DOR__POS            7
#define BMM050_INT_STAT_DOR__LEN            1
#define BMM050_INT_STAT_DOR__MSK            0x80
#define BMM050_INT_STAT_DOR__REG            BMM050_INT_STAT

#define BMM050_INT_STAT_OVRFLOW__POS        6
#define BMM050_INT_STAT_OVRFLOW__LEN        1
#define BMM050_INT_STAT_OVRFLOW__MSK        0x40
#define BMM050_INT_STAT_OVRFLOW__REG        BMM050_INT_STAT

#define BMM050_INT_STAT_HIGH_THZ__POS       5
#define BMM050_INT_STAT_HIGH_THZ__LEN       1
#define BMM050_INT_STAT_HIGH_THZ__MSK       0x20
#define BMM050_INT_STAT_HIGH_THZ__REG       BMM050_INT_STAT

#define BMM050_INT_STAT_HIGH_THY__POS       4
#define BMM050_INT_STAT_HIGH_THY__LEN       1
#define BMM050_INT_STAT_HIGH_THY__MSK       0x10
#define BMM050_INT_STAT_HIGH_THY__REG       BMM050_INT_STAT

#define BMM050_INT_STAT_HIGH_THX__POS       3
#define BMM050_INT_STAT_HIGH_THX__LEN       1
#define BMM050_INT_STAT_HIGH_THX__MSK       0x08
#define BMM050_INT_STAT_HIGH_THX__REG       BMM050_INT_STAT

#define BMM050_INT_STAT_LOW_THZ__POS        2
#define BMM050_INT_STAT_LOW_THZ__LEN        1
#define BMM050_INT_STAT_LOW_THZ__MSK        0x04
#define BMM050_INT_STAT_LOW_THZ__REG        BMM050_INT_STAT

#define BMM050_INT_STAT_LOW_THY__POS        1
#define BMM050_INT_STAT_LOW_THY__LEN        1
#define BMM050_INT_STAT_LOW_THY__MSK        0x02
#define BMM050_INT_STAT_LOW_THY__REG        BMM050_INT_STAT

#define BMM050_INT_STAT_LOW_THX__POS        0
#define BMM050_INT_STAT_LOW_THX__LEN        1
#define BMM050_INT_STAT_LOW_THX__MSK        0x01
#define BMM050_INT_STAT_LOW_THX__REG        BMM050_INT_STAT

/* Power Control Register */
#define BMM050_POWER_CNTL_SRST7__POS       7
#define BMM050_POWER_CNTL_SRST7__LEN       1
#define BMM050_POWER_CNTL_SRST7__MSK       0x80
#define BMM050_POWER_CNTL_SRST7__REG       BMM050_POWER_CNTL

#define BMM050_POWER_CNTL_SPI3_EN__POS     2
#define BMM050_POWER_CNTL_SPI3_EN__LEN     1
#define BMM050_POWER_CNTL_SPI3_EN__MSK     0x04
#define BMM050_POWER_CNTL_SPI3_EN__REG     BMM050_POWER_CNTL

#define BMM050_POWER_CNTL_SRST1__POS       1
#define BMM050_POWER_CNTL_SRST1__LEN       1
#define BMM050_POWER_CNTL_SRST1__MSK       0x02
#define BMM050_POWER_CNTL_SRST1__REG       BMM050_POWER_CNTL

#define BMM050_POWER_CNTL_PCB__POS         0
#define BMM050_POWER_CNTL_PCB__LEN         1
#define BMM050_POWER_CNTL_PCB__MSK         0x01
#define BMM050_POWER_CNTL_PCB__REG         BMM050_POWER_CNTL

/* Control Register */
#define BMM050_CNTL_ADV_ST__POS            6
#define BMM050_CNTL_ADV_ST__LEN            2
#define BMM050_CNTL_ADV_ST__MSK            0xC0
#define BMM050_CNTL_ADV_ST__REG            BMM050_CONTROL

#define BMM050_CNTL_DR__POS                3
#define BMM050_CNTL_DR__LEN                3
#define BMM050_CNTL_DR__MSK                0x38
#define BMM050_CNTL_DR__REG                BMM050_CONTROL

#define BMM050_CNTL_OPMODE__POS            1
#define BMM050_CNTL_OPMODE__LEN            2
#define BMM050_CNTL_OPMODE__MSK            0x06
#define BMM050_CNTL_OPMODE__REG            BMM050_CONTROL

#define BMM050_CNTL_S_TEST__POS            0
#define BMM050_CNTL_S_TEST__LEN            1
#define BMM050_CNTL_S_TEST__MSK            0x01
#define BMM050_CNTL_S_TEST__REG            BMM050_CONTROL

/* Interupt Control Register */
#define BMM050_INT_CNTL_DOR_EN__POS            7
#define BMM050_INT_CNTL_DOR_EN__LEN            1
#define BMM050_INT_CNTL_DOR_EN__MSK            0x80
#define BMM050_INT_CNTL_DOR_EN__REG            BMM050_INT_CNTL

#define BMM050_INT_CNTL_OVRFLOW_EN__POS        6
#define BMM050_INT_CNTL_OVRFLOW_EN__LEN        1
#define BMM050_INT_CNTL_OVRFLOW_EN__MSK        0x40
#define BMM050_INT_CNTL_OVRFLOW_EN__REG        BMM050_INT_CNTL

#define BMM050_INT_CNTL_HIGH_THZ_EN__POS       5
#define BMM050_INT_CNTL_HIGH_THZ_EN__LEN       1
#define BMM050_INT_CNTL_HIGH_THZ_EN__MSK       0x20
#define BMM050_INT_CNTL_HIGH_THZ_EN__REG       BMM050_INT_CNTL

#define BMM050_INT_CNTL_HIGH_THY_EN__POS       4
#define BMM050_INT_CNTL_HIGH_THY_EN__LEN       1
#define BMM050_INT_CNTL_HIGH_THY_EN__MSK       0x10
#define BMM050_INT_CNTL_HIGH_THY_EN__REG       BMM050_INT_CNTL

#define BMM050_INT_CNTL_HIGH_THX_EN__POS       3
#define BMM050_INT_CNTL_HIGH_THX_EN__LEN       1
#define BMM050_INT_CNTL_HIGH_THX_EN__MSK       0x08
#define BMM050_INT_CNTL_HIGH_THX_EN__REG       BMM050_INT_CNTL

#define BMM050_INT_CNTL_LOW_THZ_EN__POS        2
#define BMM050_INT_CNTL_LOW_THZ_EN__LEN        1
#define BMM050_INT_CNTL_LOW_THZ_EN__MSK        0x04
#define BMM050_INT_CNTL_LOW_THZ_EN__REG        BMM050_INT_CNTL

#define BMM050_INT_CNTL_LOW_THY_EN__POS        1
#define BMM050_INT_CNTL_LOW_THY_EN__LEN        1
#define BMM050_INT_CNTL_LOW_THY_EN__MSK        0x02
#define BMM050_INT_CNTL_LOW_THY_EN__REG        BMM050_INT_CNTL

#define BMM050_INT_CNTL_LOW_THX_EN__POS        0
#define BMM050_INT_CNTL_LOW_THX_EN__LEN        1
#define BMM050_INT_CNTL_LOW_THX_EN__MSK        0x01
#define BMM050_INT_CNTL_LOW_THX_EN__REG        BMM050_INT_CNTL

/* Sensor Control Register */
#define BMM050_SENS_CNTL_DRDY_EN__POS          7
#define BMM050_SENS_CNTL_DRDY_EN__LEN          1
#define BMM050_SENS_CNTL_DRDY_EN__MSK          0x80
#define BMM050_SENS_CNTL_DRDY_EN__REG          BMM050_SENS_CNTL

#define BMM050_SENS_CNTL_IE__POS               6
#define BMM050_SENS_CNTL_IE__LEN               1
#define BMM050_SENS_CNTL_IE__MSK               0x40
#define BMM050_SENS_CNTL_IE__REG               BMM050_SENS_CNTL

#define BMM050_SENS_CNTL_CHANNELZ__POS         5
#define BMM050_SENS_CNTL_CHANNELZ__LEN         1
#define BMM050_SENS_CNTL_CHANNELZ__MSK         0x20
#define BMM050_SENS_CNTL_CHANNELZ__REG         BMM050_SENS_CNTL

#define BMM050_SENS_CNTL_CHANNELY__POS         4
#define BMM050_SENS_CNTL_CHANNELY__LEN         1
#define BMM050_SENS_CNTL_CHANNELY__MSK         0x10
#define BMM050_SENS_CNTL_CHANNELY__REG         BMM050_SENS_CNTL

#define BMM050_SENS_CNTL_CHANNELX__POS         3
#define BMM050_SENS_CNTL_CHANNELX__LEN         1
#define BMM050_SENS_CNTL_CHANNELX__MSK         0x08
#define BMM050_SENS_CNTL_CHANNELX__REG         BMM050_SENS_CNTL

#define BMM050_SENS_CNTL_DR_POLARITY__POS      2
#define BMM050_SENS_CNTL_DR_POLARITY__LEN      1
#define BMM050_SENS_CNTL_DR_POLARITY__MSK      0x04
#define BMM050_SENS_CNTL_DR_POLARITY__REG      BMM050_SENS_CNTL

#define BMM050_SENS_CNTL_INTERRUPT_LATCH__POS            1
#define BMM050_SENS_CNTL_INTERRUPT_LATCH__LEN            1
#define BMM050_SENS_CNTL_INTERRUPT_LATCH__MSK            0x02
#define BMM050_SENS_CNTL_INTERRUPT_LATCH__REG            BMM050_SENS_CNTL

#define BMM050_SENS_CNTL_INTERRUPT_POLARITY__POS         0
#define BMM050_SENS_CNTL_INTERRUPT_POLARITY__LEN         1
#define BMM050_SENS_CNTL_INTERRUPT_POLARITY__MSK         0x01
#define BMM050_SENS_CNTL_INTERRUPT_POLARITY__REG         BMM050_SENS_CNTL

/* Register 6D */
#define BMM050_DIG_XYZ1_MSB__POS         0
#define BMM050_DIG_XYZ1_MSB__LEN         7
#define BMM050_DIG_XYZ1_MSB__MSK         0x7F
#define BMM050_DIG_XYZ1_MSB__REG         BMM050_DIG_XYZ1_MSB


#define BMM050_X_AXIS               0
#define BMM050_Y_AXIS               1
#define BMM050_Z_AXIS               2
#define BMM050_RESISTANCE           3
#define BMM050_X                    1
#define BMM050_Y                    2
#define BMM050_Z                    4
#define BMM050_XYZ                  7

/* Constants */
#define BMM050_NULL                             0
#define BMM050_DISABLE                          0
#define BMM050_ENABLE                           1
#define BMM050_CHANNEL_DISABLE                  1
#define BMM050_CHANNEL_ENABLE                   0
#define BMM050_INTPIN_LATCH_ENABLE              1
#define BMM050_INTPIN_LATCH_DISABLE             0
#define BMM050_OFF                              0
#define BMM050_ON                               1

#define BMM050_NORMAL_MODE                      0x00
#define BMM050_FORCED_MODE                      0x01
#define BMM050_SUSPEND_MODE                     0x02
#define BMM050_SLEEP_MODE                       0x03

#define BMM050_ADVANCED_SELFTEST_OFF            0
#define BMM050_ADVANCED_SELFTEST_NEGATIVE       2
#define BMM050_ADVANCED_SELFTEST_POSITIVE       3

#define BMM050_NEGATIVE_SATURATION_Z            -32767
#define BMM050_POSITIVE_SATURATION_Z            32767

#define BMM050_SPI_RD_MASK                      0x80
#define BMM050_READ_SET                         0x01

#define E_BMM050_NULL_PTR                       ((char)-127)
#define E_BMM050_COMM_RES                       ((char)-1)
#define E_BMM050_OUT_OF_RANGE                   ((char)-2)
#define E_BMM050_UNDEFINED_MODE                 0

#define BMM050_WR_FUNC_PTR\
	char (*bus_write)(unsigned char, unsigned char,\
			unsigned char *, unsigned char)

#define BMM050_RD_FUNC_PTR\
	char (*bus_read)(unsigned char, unsigned char,\
			unsigned char *, unsigned char)

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
#define BMM050_CONVFACTOR_LSB_UT                6

/* get bit slice  */
#define BMM050_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

/* Set bit slice */
#define BMM050_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/* compensated output value returned if sensor had overflow */
#define BMM050_OVERFLOW_OUTPUT       -32768
#define BMM050_OVERFLOW_OUTPUT_S32   ((int)(-2147483647-1))
#define BMM050_OVERFLOW_OUTPUT_FLOAT 0.0f
#define BMM050_FLIP_OVERFLOW_ADCVAL  -4096
#define BMM050_HALL_OVERFLOW_ADCVAL  -16384


#define BMM050_PRESETMODE_LOWPOWER                  1
#define BMM050_PRESETMODE_REGULAR                   2
#define BMM050_PRESETMODE_HIGHACCURACY              3
#define BMM050_PRESETMODE_ENHANCED                  4

/* PRESET MODES - DATA RATES */
#define BMM050_LOWPOWER_DR                       BMM050_DR_10HZ
#define BMM050_REGULAR_DR                        BMM050_DR_10HZ
#define BMM050_HIGHACCURACY_DR                   BMM050_DR_20HZ
#define BMM050_ENHANCED_DR                       BMM050_DR_10HZ

/* PRESET MODES - REPETITIONS-XY RATES */
#define BMM050_LOWPOWER_REPXY                     1
#define BMM050_REGULAR_REPXY                      4
#define BMM050_HIGHACCURACY_REPXY                23
#define BMM050_ENHANCED_REPXY                     7

/* PRESET MODES - REPETITIONS-Z RATES */
#define BMM050_LOWPOWER_REPZ                      2
#define BMM050_REGULAR_REPZ                      15
#define BMM050_HIGHACCURACY_REPZ                 82
#define BMM050_ENHANCED_REPZ                     26

/* Data Rates */

#define BMM050_DR_10HZ                     0
#define BMM050_DR_02HZ                     1
#define BMM050_DR_06HZ                     2
#define BMM050_DR_08HZ                     3
#define BMM050_DR_15HZ                     4
#define BMM050_DR_20HZ                     5
#define BMM050_DR_25HZ                     6
#define BMM050_DR_30HZ                     7

/*user defined Structures*/
struct bmm050_mdata {
	short datax;
	short datay;
	short dataz;
	unsigned short resistance;
};
struct bmm050_mdata_s32 {
	int datax;
	int datay;
	int dataz;
	unsigned short resistance;
	unsigned short drdy;
};
struct bmm050_iio_mdata_s32 {
		int data;
		unsigned short drdy;
		unsigned short value_x_valid;
		unsigned short value_y_valid;
		unsigned short value_z_valid;
};

struct bmm050_mdata_float {
	float datax;
	float datay;
	float  dataz;
	unsigned short resistance;
};

struct bmm050 {
	unsigned char company_id;
	unsigned char dev_addr;

	BMM050_WR_FUNC_PTR;
	BMM050_RD_FUNC_PTR;
	void (*delay_msec)(unsigned int);

	signed char dig_x1;
	signed char dig_y1;

	signed char dig_x2;
	signed char dig_y2;

	unsigned short dig_z1;
	short dig_z2;
	short dig_z3;
	short dig_z4;

	unsigned char dig_xy1;
	signed char dig_xy2;

	unsigned short dig_xyz1;
};

/************************Start**********************************************/
struct bmm_client_data {
	struct bmm050 device;
	struct i2c_client *client;
	struct input_dev *input;
	struct iio_trigger	*trig;
	struct delayed_work work;
	unsigned char *buffer_data;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_handler;
#endif

	atomic_t delay;
	/* whether the system in suspend state */
	atomic_t in_suspend;

	struct bmm050_mdata_s32 value;
	struct bmm050_iio_mdata_s32 iio_mdata;
	u8 enable:1;
	s8 op_mode:4;
	u8 odr;
	u8 rept_xy;
	u8 rept_z;

	s16 result_test;

	struct mutex mutex_power_mode;

	/* controls not only reg, but also workqueue */
	struct mutex mutex_op_mode;
	struct mutex mutex_enable;
	struct mutex mutex_odr;
	struct mutex mutex_rept_xy;
	struct mutex mutex_rept_z;

	struct mutex mutex_value;
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	struct bosch_sensor_specific *bst_pd;
#endif
};

/*Related with output data type*/
enum BMM_SCAN_INDEX {
		BMM_SCAN_MAG_X,
		BMM_SCAN_MAG_Y,
		BMM_SCAN_MAG_Z,
		BMM_SCAN_TIMESTAMP,
};


#define BMM_NUMBER_DATA_CHANNELS           3
#define BMM_BYTE_PRE_CHANNEL                       2

irqreturn_t bmm_buffer_handler(int irq, void *p);
int bmm_probe_trigger(struct iio_dev *indio_dev);
void bmm_deallocate_ring(struct iio_dev *indio_dev);





/************************End**********************************************/
char bmm050_init(struct bmm050 *p_bmm050);
char bmm050_read_mdataXYZ(struct bmm050_mdata *mdata);
char bmm050_read_mdataXYZ_s32(struct bmm050_mdata_s32 *mdata);
#ifdef ENABLE_FLOAT
char bmm050_read_mdataXYZ_float(struct bmm050_mdata_float *mdata);
#endif
char bmm050_read_register(
		unsigned char addr, unsigned char *data, unsigned char len);
char bmm050_write_register(
		unsigned char addr, unsigned char *data, unsigned char len);
char bmm050_get_self_test_XYZ(unsigned char *self_testxyz);
short bmm050_compensate_X(short mdata_x, unsigned short data_R);
int bmm050_compensate_X_s32(short mdata_x,  unsigned short data_R);
#ifdef ENABLE_FLOAT
float bmm050_compensate_X_float(short mdata_x,  unsigned short data_R);
#endif
short bmm050_compensate_Y(short mdata_y, unsigned short data_R);
int bmm050_compensate_Y_s32(short mdata_y,  unsigned short data_R);
#ifdef ENABLE_FLOAT
float bmm050_compensate_Y_float(short mdata_y,  unsigned short data_R);
#endif
short bmm050_compensate_Z(short mdata_z,  unsigned short data_R);
int bmm050_compensate_Z_s32(short mdata_z,  unsigned short data_R);
#ifdef ENABLE_FLOAT
float bmm050_compensate_Z_float(short mdata_z,  unsigned short data_R);
#endif
char bmm050_get_raw_xyz(struct bmm050_mdata *mdata);
char bmm050_init_trim_registers(void);
char bmm050_set_spi3(unsigned char value);
char bmm050_get_powermode(unsigned char *mode);
char bmm050_set_powermode(unsigned char mode);
char bmm050_set_adv_selftest(unsigned char adv_selftest);
char bmm050_get_adv_selftest(unsigned char *adv_selftest);
char bmm050_set_datarate(unsigned char data_rate);
char bmm050_get_datarate(unsigned char *data_rate);
char bmm050_set_functional_state(unsigned char functional_state);
char bmm050_get_functional_state(unsigned char *functional_state);
char bmm050_set_selftest(unsigned char selftest);
char bmm050_get_selftest(unsigned char *selftest);
char bmm050_perform_advanced_selftest(short *diff_z);
char bmm050_get_repetitions_XY(unsigned char *no_repetitions_xy);
char bmm050_set_repetitions_XY(unsigned char no_repetitions_xy);
char bmm050_get_repetitions_Z(unsigned char *no_repetitions_z);
char bmm050_set_repetitions_Z(unsigned char no_repetitions_z);
char bmm050_get_presetmode(unsigned char *mode);
char bmm050_set_presetmode(unsigned char mode);
char bmm050_set_control_measurement_x(unsigned char enable_disable);
char bmm050_set_control_measurement_y(unsigned char enable_disable);
char bmm050_soft_reset(void);

#endif

/*!
 * @section LICENSE
 * (C) Copyright 2011~2015 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_core.h
 * @date     2014/11/25 14:40
 * @id       "128af5d"
 * @version  1.2
 *
 * @brief
 * The head file of BMI160 device driver core code
*/

#ifndef __BMI160_CORE_H__
#define __BMI160_CORE_H__

#include <linux/string.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include "bmi160.h"
/* sensor specific */
#define SENSOR_NAME "bmi160"


#define BMI160_ENABLE_INT1 1
#define BMI160_DEBUG 1
#define SENSOR_CHIP_ID_BMI (0xD0)
#define SENSOR_CHIP_ID_BMI_C2 (0xD1)
#define SENSOR_CHIP_ID_BMI_C3 (0xD3)

#define SENSOR_CHIP_REV_ID_BMI (0x00)

#define CHECK_CHIP_ID_TIME_MAX  5

#define BMI_REG_NAME(name) BMI160_##name##__REG
#define BMI_VAL_NAME(name) BMI160_##name
#define BMI_CALL_API(name) bmi160_##name

#define BMI_I2C_WRITE_DELAY_TIME (1)

/* generic */
#define BMI_MAX_RETRY_I2C_XFER (10)
#define BMI_MAX_RETRY_WAKEUP (5)
#define BMI_MAX_RETRY_WAIT_DRDY (100)

#define BMI_DELAY_MIN (1)
#define BMI_DELAY_DEFAULT (200)

#define BMI_VALUE_MAX (32767)
#define BMI_VALUE_MIN (-32768)

#define BYTES_PER_LINE (16)

#define BUF_SIZE_PRINT (16)

/*! FIFO 1024 byte, max fifo frame count not over 150 */
#define FIFO_FRAME_CNT 170
#define IIO_AORGBUFFER 16
#define FIFO_DATA_BUFSIZE    1024


#define FRAME_LEN_ACC    6
#define FRAME_LEN_GYRO    6
#define FRAME_LEN_MAG    8

/*! BMI Self test */
#define BMI_SELFTEST_AMP_HIGH       1

/* CMD  */
#define CMD_FOC_START                 0x03
#define CMD_PMU_ACC_SUSPEND           0x10
#define CMD_PMU_ACC_NORMAL            0x11
#define CMD_PMU_ACC_LP1               0x12
#define CMD_PMU_ACC_LP2               0x13
#define CMD_PMU_GYRO_SUSPEND          0x14
#define CMD_PMU_GYRO_NORMAL           0x15
#define CMD_PMU_GYRO_FASTSTART        0x17
#define CMD_PMU_MAG_SUSPEND           0x18
#define CMD_PMU_MAG_NORMAL            0x19
#define CMD_PMU_MAG_LP1               0x1A
#define CMD_PMU_MAG_LP2               0x1B
#define CMD_CLR_FIFO_DATA             0xB0
#define CMD_RESET_INT_ENGINE          0xB1
#define CMD_RESET_USER_REG            0xB6

#define USER_DAT_CFG_PAGE              0x00

/*! FIFO Head definition*/
#define FIFO_HEAD_A        0x84
#define FIFO_HEAD_G        0x88
#define FIFO_HEAD_M        0x90

#define FIFO_HEAD_G_A        (FIFO_HEAD_G | FIFO_HEAD_A)
#define FIFO_HEAD_M_A        (FIFO_HEAD_M | FIFO_HEAD_A)
#define FIFO_HEAD_M_G        (FIFO_HEAD_M | FIFO_HEAD_G)

#define FIFO_HEAD_M_G_A         (FIFO_HEAD_M | FIFO_HEAD_G | FIFO_HEAD_A)

#define FIFO_HEAD_SENSOR_TIME        0x44
#define FIFO_HEAD_SKIP_FRAME        0x40
#define FIFO_HEAD_OVER_READ_LSB       0x80
#define FIFO_HEAD_OVER_READ_MSB       0x00

/*! FIFO head mode Frame bytes number definition */
#define A_BYTES_FRM      6
#define G_BYTES_FRM      6
#define M_BYTES_FRM      8
#define GA_BYTES_FRM     12
#define MG_BYTES_FRM     14
#define MA_BYTES_FRM     14
#define MGA_BYTES_FRM    20

#define ACC_FIFO_HEAD       "acc"
#define GYRO_FIFO_HEAD     "gyro"
#define MAG_FIFO_HEAD         "mag"

/*! Bosch sensor unknown place*/
#define BOSCH_SENSOR_PLACE_UNKNOWN (-1)
/*! Bosch sensor remapping table size P0~P7*/
#define MAX_AXIS_REMAP_TAB_SZ 8

#define ENABLE     1
#define DISABLE    0

/* bmi sensor HW interrupt pin number */
#define BMI_INT0      0
#define BMI_INT1       1

#define BMI_INT_LEVEL      0
#define BMI_INT_EDGE        1

/*! BMI mag interface */


/* compensated output value returned if sensor had overflow */
#define BMM050_OVERFLOW_OUTPUT       -32768
#define BMM050_OVERFLOW_OUTPUT_S32   ((s32)(-2147483647-1))

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

struct bmi160mag_compensate_t {
	signed char dig_x1;
	signed char dig_y1;

	signed char dig_x2;
	signed char dig_y2;

	u16 dig_z1;
	s16 dig_z2;
	s16 dig_z3;
	s16 dig_z4;

	unsigned char dig_xy1;
	signed char dig_xy2;

	u16 dig_xyz1;
};

/*bmi fifo sensor type combination*/
enum BMI_FIFO_DATA_SELECT_T {
	BMI_FIFO_A_SEL = 1,
	BMI_FIFO_G_SEL,
	BMI_FIFO_G_A_SEL,
	BMI_FIFO_M_SEL,
	BMI_FIFO_M_A_SEL,
	BMI_FIFO_M_G_SEL,
	BMI_FIFO_M_G_A_SEL,
	BMI_FIFO_DATA_SEL_MAX
};

/* scan element definition */
enum BMI_AXIS_SCAN {
	BMI_SCAN_GYRO_X,
	BMI_SCAN_GYRO_Y,
	BMI_SCAN_GYRO_Z,
	BMI_SCAN_ACCL_X,
	BMI_SCAN_ACCL_Y,
	BMI_SCAN_ACCL_Z,
	BMI_SCAN_MAGN_X,
	BMI_SCAN_MAGN_Y,
	BMI_SCAN_MAGN_Z,
	BMI_SCAN_SENSORTM,
	BMI_SCAN_TIMESTAMP,
};


/*extern the iio_dev of three devices*/
extern struct iio_dev *accl_iio_private;
extern struct iio_dev *gyro_iio_private;
extern struct iio_dev *magn_iio_private;


/*! bmi160 sensor spec of power mode */
struct pw_mode {
	u8 acc_pm;
	u8 gyro_pm;
	u8 mag_pm;
};

/*! bmi160 sensor spec of odr */
struct odr_t {
	u8 acc_odr;
	u8 gyro_odr;
	u8 mag_odr;
};

/*! bmi160 sensor spec of range */
struct range_t {
	u8 acc_range;
	u8 gyro_range;
};

/*! bmi160 sensor error status */
struct err_status {
	u8 fatal_err;
	u8 err_code;
	u8 i2c_fail;
	u8 drop_cmd;
	u8 mag_drdy_err;
	u8 err_st_all;
};

/*!
 * we use a typedef to hide the detail,
 * because this type might be changed
 */
struct bosch_sensor_axis_remap {
	/* src means which source will be mapped to target x, y, z axis */
	/* if an target OS axis is remapped from (-)x,
	 * src is 0, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)y,
	 * src is 1, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)z,
	 * src is 2, sign_* is (-)1 */
	int src_x:3;
	int src_y:3;
	int src_z:3;

	int sign_x:2;
	int sign_y:2;
	int sign_z:2;
};


struct bosch_sensor_data {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

struct pedometer_data_t {
	/*! Fix step detector misinformation for the first time*/
	u8 wkar_step_detector_status;
	u_int32_t last_step_counter_value;
};

struct bmi160_t *bmi160_get_ptr(void);
struct bmi_client_data {
	struct bmi160_t device;
	struct device *dev;
	u8 chip_id;
	struct pw_mode pw;
	struct odr_t odr;
	struct range_t range; /*TO DO*/
	struct err_status err_st;
	struct pedometer_data_t pedo_data;
	s8 place;
	struct iio_trigger *acctrig;
	struct iio_trigger *gyrotrig;
	struct iio_trigger *magtrig;
	u8 selftest;
	u32 sensor_time;
	atomic_t delay;
	atomic_t selftest_result;
	u8  fifo_data_sel;
	u16 fifo_bytecount;
	u8 fifo_head_en;
	unsigned char fifo_int_tag_en;
	spinlock_t time_stamp_lock;
	unsigned char *fifo_data;
	u64 fifo_time;
	u64 fifo_time2;
	u64 alarm_time;
	u64 del_time;
	u8 stc_enable;
	uint16_t gpio_pin;
	u8 std;
	u8 sig_flag;
	u8 sig_value;
	struct mutex mutex_op_mode;
	struct mutex mutex_enable;
	int IRQ;
	int reg_sel;
	int reg_len;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_handler;
#endif
};

s8 bmi_i2c_read(u8 dev_addr, u8 reg_addr, u8 *data, u16 len);
s8 bmi_i2c_write(u8 dev_addr, u8 reg_addr, u8 *data, u8 len);
int bmi_probe(struct iio_dev *accl_iio_private,
				struct iio_dev *gyro_iio_private,
				struct iio_dev *magn_iio_private);
int bmi_remove(struct device *dev);
int bmi_suspend(struct device *dev);
int bmi_resume(struct device *dev);
int bmi_allocate_ring(struct iio_dev *indio_dev);
void bmi_deallocate_ring(struct iio_dev *indio_dev);
int bmi_probe_acctrigger(struct iio_dev *indio_dev);
int bmi_probe_gyrotrigger(struct iio_dev *indio_dev);
int bmi_probe_magtrigger(struct iio_dev *indio_dev);

#endif

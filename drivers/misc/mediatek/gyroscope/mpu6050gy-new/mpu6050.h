#ifndef MPU6050_H
#define MPU6050_H

#include <linux/ioctl.h>

#define MPU6050_ACCESS_BY_GSE_I2C

#ifdef MPU6050_ACCESS_BY_GSE_I2C
#define MPU6050_I2C_SLAVE_ADDR		(0xD2)	/* mtk i2c not allow to probe two same address */
#else
#define MPU6050_I2C_SLAVE_ADDR		0xD0
#endif


/* MPU6050 Register Map  (Please refer to MPU6050 Specifications) */

#define MPU6050_REG_DEVID           0x75
#define MPU6050_REG_FIFO_EN         0x23
#define MPU6050_REG_AUX_VDD         0x01

#define MPU6050_REG_SAMRT_DIV       0x19
/* set external sync, full-scale range and sample rate, low pass filter bandwidth */
#define MPU6050_REG_CFG             0x1A
#define MPU6050_REG_GYRO_CFG        0x1B	/* full-scale range and sample rate, */


#define MPU6050_REG_GYRO_XH         0x43

#define MPU6050_REG_TEMPH           0x41


#define MPU6050_REG_FIFO_CNTH       0x72
#define MPU6050_REG_FIFO_CNTL       0x73
#define MPU6050_REG_FIFO_DATA       0x74
#define MPU6050_REG_FIFO_CTL        0x6A
#define MPU6050_REG_PWR_CTL	        0x6B
#define MPU6050_REG_PWR_CTL2        0x6C


/*MPU6050 Register Bit definitions*/

#define MPU6050_FIFO_GYROX_EN       0x40	/* insert the X Gyro data into FIFO */
#define MPU6050_FIFO_GYROY_EN       0x20	/* insert the Y Gyro data into FIFO */
#define MPU6050_FIFO_GYROZ_EN       0x10	/* insert the Z Gyro data into FIFO */

#define MPU6050_AUX_VDDIO_DIS       0x00	/* disable VDD level for the secondary I2C bus clock and data lines */

/* for MPU6050_REG_CFG */
/* 0x05  //captue the state of external frame sync input pin to insert into LSB of registers */
#define MPU6050_EXT_SYNC			0x03
#define MPU6050_SYNC_GYROX			0x02

/* for MPU6050_REG_GYRO_CFG */
#define MPU6050_FS_RANGE			0x03	/* set the full-scale range of the gyro sensors */
#define MPU6050_FS_1000				0x02


#define MPU6050_FS_1000_LSB			33
#define MPU6050_FS_MAX_LSB			131

#define MPU6050_RATE_1K_LPFB_188HZ	0x01
#define MPU6050_RATE_1K_LPFB_256HZ	0x00

#define MPU6050_FIFO_EN				0x40	/* enable FIFO operation for sensor data */

#define MPU6050_FIFO_RST			0x40	/* reset FIFO function */

#define MPU6050_SLEEP               0x40	/* enable low power sleep mode */


#define MPU6050_SUCCESS             0
#define MPU6050_ERR_I2C             -1
#define MPU6050_ERR_STATUS          -3
#define MPU6050_ERR_SETUP_FAILURE   -4
#define MPU6050_ERR_GETGSENSORDATA  -5
#define MPU6050_ERR_IDENTIFICATION  -6


#define MPU6050_BUFSIZE 60

/* 1 rad = 180/PI degree, MAX_LSB = 131, */
/* 180*131/PI = 7506 */
#define DEGREE_TO_RAD	7506

extern int MPU6050_gse_power(void);
extern int MPU6050_gse_mode(void);
#ifdef MPU6050_ACCESS_BY_GSE_I2C
extern int MPU6050_hwmsen_read_block(u8 addr, u8 *buf, u8 len);
extern int MPU6050_hwmsen_write_block(u8 addr, u8 *buf, u8 len);
#endif
#endif				/* MPU6050_H */


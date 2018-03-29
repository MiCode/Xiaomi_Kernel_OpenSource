#ifndef MPU6515_H
#define MPU6515_H

#include <linux/ioctl.h>

#define MPU6515_ACCESS_BY_GSE_I2C

#ifdef MPU6515_ACCESS_BY_GSE_I2C
    #define MPU6515_I2C_SLAVE_ADDR		(0xD2)   /* mtk i2c not allow to probe two same address */
#else
    #define MPU6515_I2C_SLAVE_ADDR		0xD0
#endif


/* MPU6515 Register Map  (Please refer to MPU6515 Specifications) */

#define MPU6515_REG_DEVID           0x75
#define MPU6515_REG_FIFO_EN         0x23
#define MPU6515_REG_AUX_VDD         0x01

#define MPU6515_REG_SAMRT_DIV       0x19
#define MPU6515_REG_CFG             0x1A	  /* set external sync, full-scale range and sample rate,
 low pass filter bandwidth */
#define MPU6515_REG_GYRO_CFG        0x1B	  /* full-scale range and sample rate, */


#define MPU6515_REG_GYRO_XH         0x43

#define MPU6515_REG_TEMPH           0x41


#define MPU6515_REG_FIFO_CNTH       0x72
#define MPU6515_REG_FIFO_CNTL       0x73
#define MPU6515_REG_FIFO_DATA       0x74
#define MPU6515_REG_FIFO_CTL        0x6A
#define MPU6515_REG_PWR_CTL	        0x6B
#define MPU6515_REG_PWR_CTL2        0x6C


/*MPU6515 Register Bit definitions*/

#define MPU6515_FIFO_GYROX_EN       0x40	/* insert the X Gyro data into FIFO */
#define MPU6515_FIFO_GYROY_EN       0x20	/* insert the Y Gyro data into FIFO */
#define MPU6515_FIFO_GYROZ_EN       0x10	/* insert the Z Gyro data into FIFO */

#define MPU6515_AUX_VDDIO_DIS       0x00	/* disable VDD level for the secondary I2C bus clock and data lines */

/* for MPU6515_REG_CFG */
#define MPU6515_EXT_SYNC			0x03	/* 0x05	//captue the state of external frame sync input
 pin to insert into LSB of registers */
#define MPU6515_SYNC_GYROX			0x02

/* for MPU6515_REG_GYRO_CFG */
#define MPU6515_FS_RANGE			0x03	/* set the full-scale range of the gyro sensors */
#define MPU6515_FS_1000				0x02


#define MPU6515_FS_1000_LSB			33
#define MPU6515_FS_MAX_LSB			131

#define MPU6515_RATE_1K_LPFB_188HZ	0x01
#define MPU6515_RATE_1K_LPFB_256HZ	0x00

#define MPU6515_FIFO_EN				0x40	/* enable FIFO operation for sensor data */

#define MPU6515_FIFO_RST			0x40    /* reset FIFO function */

#define MPU6515_SLEEP               0x40	/* enable low power sleep mode */


#define MPU6515_SUCCESS             0
#define MPU6515_ERR_I2C             -1
#define MPU6515_ERR_STATUS          -3
#define MPU6515_ERR_SETUP_FAILURE   -4
#define MPU6515_ERR_GETGSENSORDATA  -5
#define MPU6515_ERR_IDENTIFICATION  -6


#define MPU6515_BUFSIZE 60

/* 1 rad = 180/PI degree, MAX_LSB = 131, */
/* 180*131/PI = 7506 */
#define DEGREE_TO_RAD	7506

#endif /* MPU6515_H */

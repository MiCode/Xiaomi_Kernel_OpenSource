/*
 * Definitions for mmc3524x magnetic sensor chip.
 */
#ifndef __MMC3524x_H__
#define __MMC3524x_H__

#include <linux/ioctl.h>

#define MMC3524x_I2C_NAME		"mmc3524x"
#define CALIBRATION_DATA_SIZE	12


#define MMC3524x_I2C_ADDR		0x30	/* 7-bit */

/* MMC3524x register address */
#define MMC3524X_REG_CTRL		0x07
#define MMC3524X_REG_BITS		0x08
#define MMC3524X_REG_DATA		0x00
#define MMC3524X_REG_DS			0x06
#define MMC3524X_REG_PRODUCTID_0		0x10
#define MMC3524X_REG_PRODUCTID_1		0x20

/* MMC3524x control bit */
#define MMC3524X_CTRL_TM			0x01
#define MMC3524X_CTRL_CM			0x02
#define MMC3524X_CTRL_50HZ		0x00
#define MMC3524X_CTRL_25HZ		0x04
#define MMC3524X_CTRL_12HZ		0x08
#define MMC3524X_CTRL_NOBOOST            0x10
#define MMC3524X_CTRL_SET	        0x20
#define MMC3524X_CTRL_RESET              0x40
#define MMC3524X_CTRL_REFILL             0x80

#define MMC3524X_BITS_SLOW_16            0x00
#define MMC3524X_BITS_FAST_16            0x01
#define MMC3524X_BITS_14                 0x02

/* conversion of magnetic data (for mmc3524x) to uT units */
/* conversion of magnetic data to uT units */
/* 32768 = 1Guass = 100 uT */
/* 100 / 32768 = 25 / 8192 */
/* 65536 = 360Degree */
/* 360 / 65536 = 45 / 8192 */


#define CONVERT_M			25
#define CONVERT_M_DIV		8192
#define CONVERT_O			45
#define CONVERT_O_DIV		8192

/* sensitivity 512 count = 1 Guass = 100uT */

#define MMC3524X_OFFSET_X		32768
#define MMC3524X_OFFSET_Y		32768
#define MMC3524X_OFFSET_Z		32768
#define MMC3524X_SENSITIVITY_X		1024
#define MMC3524X_SENSITIVITY_Y		1024
#define MMC3524X_SENSITIVITY_Z		1024



#endif /* __MMC3524x_H__ */

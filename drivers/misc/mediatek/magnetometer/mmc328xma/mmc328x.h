/*
 * Definitions for mmc328x magnetic sensor chip.
 */
#ifndef __MMC328x_H__
#define __MMC328x_H__

#include <linux/ioctl.h>

#define MMC328x_I2C_NAME		"mmc328x"
#define CALIBRATION_DATA_SIZE	12


#define MMC328x_I2C_ADDR		0x60

/* MMC328x register address */
#define MMC328x_REG_CTRL		0x07
#define MMC328x_REG_DATA		0x00
#define MMC328x_REG_DS			0x06

/* MMC328x control bit */
#define MMC328x_CTRL_TM			0x01
#define MMC328x_CTRL_RM			0x20
#define MMC328x_CTRL_RRM			0x40



// conversion of magnetic data (for mmc328x) to uT units
// conversion of magnetic data to uT units
// 32768 = 1Guass = 100 uT
// 100 / 32768 = 25 / 8096
// 65536 = 360Degree
// 360 / 65536 = 45 / 8192


#define CONVERT_M			25
#define CONVERT_M_DIV		8096			
#define CONVERT_O			45
#define CONVERT_O_DIV		8192

// sensitivity 512 count = 1 Guass = 100uT

#define MMC328X_OFFSET_X		4096
#define MMC328X_OFFSET_Y		4096
#define MMC328X_OFFSET_Z		4096
#define MMC328X_SENSITIVITY_X		512
#define MMC328X_SENSITIVITY_Y		512
#define MMC328X_SENSITIVITY_Z		512



#endif /* __MMC328x_H__ */


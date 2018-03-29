/* bmm150.c - bmm150 compass driver
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */

/*
 * Definitions for bmm150 magnetic sensor chip.
 */
#ifndef __BMM150_H__
#define __BMM150_H__

#include <linux/ioctl.h>

#define CALIBRATION_DATA_SIZE	12

/* 7-bit addr:
*	0x12 (SDO connected to GND, CSB2 connected to VDDIO)
*	0x13 (SDO connected to VDDIO, CSB2 connected to VDDIO)
*/
#define BMM150_I2C_ADDR		0x12

/* conversion of magnetic data (for bmm150) to uT units
	conversion of magnetic data to uT units
	32768 = 1Guass = 100 uT
	100 / 32768 = 25 / 8096
	65536 = 360Degree
	360 / 65536 = 45 / 8192
*/

#define CONVERT_M			1
#define CONVERT_M_DIV		4
#define CONVERT_O			1
#define CONVERT_O_DIV		71		/*(C_PI_F32X * AXIS_RESOLUTION_FACTOR / 180)*/
#define CONVERT_G			1
#define CONVERT_G_DIV		938
#define CONVERT_VRV			1
#define CONVERT_VRV_DIV	(0x40000000)
#define CONVERT_VLA_DIV	16384
#define CONVERT_VG_DIV 16384

#ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB
/*----------------------------------------------------------------------------*/
enum CUST_ACTION {
	BMM150_CUST_ACTION_SET_CUST = 1,
	BMM150_CUST_ACTION_SET_CALI,
	BMM150_CUST_ACTION_RESET_CALI
};
/*----------------------------------------------------------------------------*/
struct BMM150_CUST {
	uint16_t action;
};
/*----------------------------------------------------------------------------*/
struct BMM150_SET_CUST {
	uint16_t action;
	uint16_t part;
	int32_t data[0];
};
/*----------------------------------------------------------------------------*/
struct BMM150_SET_CALI {
	uint16_t action;
	int32_t data[3];
};
/*----------------------------------------------------------------------------*/
union BMM150_CUST_DATA {
	uint32_t data[10];
	struct BMM150_CUST cust;
	struct BMM150_SET_CUST setCust;
	struct BMM150_SET_CALI setCali;
	struct BMM150_CUST resetCali;
};
/*----------------------------------------------------------------------------*/
#endif				/* #ifdef CONFIG_CUSTOM_KERNEL_SENSORHUB */
#endif /* __BMM150_H__ */


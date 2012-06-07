/* Header file for Quanta I2C Battery Driver
 *
 * Copyright (C) 2009 Quanta Computer Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

 /*
 *
 *  The Driver with I/O communications via the I2C Interface for ON2 of AP BU.
 *  And it is only working on the nuvoTon WPCE775x Embedded Controller.
 *
 */

#ifndef __QCI_BATTERY_H__
#define __QCI_BATTERY_H__

#define BAT_I2C_ADDRESS 0x1A
#define BATTERY_ID_NAME          "qci-i2cbat"
#define EC_FLAG_ADAPTER_IN		0x01
#define EC_FLAG_POWER_ON		0x02
#define EC_FLAG_ENTER_S3		0x04
#define EC_FLAG_ENTER_S4		0x08
#define EC_FLAG_IN_STANDBY		0x10
#define EC_FLAG_SYSTEM_ON		0x20
#define EC_FLAG_WAIT_HWPG		0x40
#define EC_FLAG_S5_POWER_ON	0x80

#define MAIN_BATTERY_STATUS_BAT_DISCHRG		0x01
#define MAIN_BATTERY_STATUS_BAT_CHARGING	0x02
#define MAIN_BATTERY_STATUS_BAT_ABNORMAL	0x04
#define MAIN_BATTERY_STATUS_BAT_IN		0x08
#define MAIN_BATTERY_STATUS_BAT_FULL		0x10
#define MAIN_BATTERY_STATUS_BAT_LOW		0x20
#define MAIN_BATTERY_STATUS_BAT_SMB_VALID	0x80

#define CHG_STATUS_BAT_CHARGE			0x01
#define CHG_STATUS_BAT_PRECHG			0x02
#define CHG_STATUS_BAT_OVERTEMP			0x04
#define CHG_STATUS_BAT_TYPE			0x08
#define CHG_STATUS_BAT_GWROK			0x10
#define CHG_STATUS_BAT_INCHARGE			0x20
#define CHG_STATUS_BAT_WAKECHRG			0x40
#define CHG_STATUS_BAT_CHGTIMEOUT		0x80

#define EC_ADAPTER_PRESENT		0x1
#define EC_BAT_PRESENT		        0x1
#define EC_ADAPTER_NOT_PRESENT		0x0
#define EC_BAT_NOT_PRESENT		0x0

#define ECRAM_POWER_SOURCE              0x40
#define ECRAM_CHARGER_ALARM		0x42
#define ECRAM_BATTERY_STATUS            0x82
#define ECRAM_BATTERY_CURRENT_LSB       0x83
#define ECRAM_BATTERY_CURRENT_MSB       0x84
#define ECRAM_BATTERY_VOLTAGE_LSB       0x87
#define ECRAM_BATTERY_VOLTAGE_MSB       0x88
#define ECRAM_BATTERY_CAPACITY          0x89
#define ECRAM_BATTERY_TEMP_LSB          0x8C
#define ECRAM_BATTERY_TEMP_MSB          0x8D
#define ECRAM_BATTERY_EVENTS            0x99

#define EC_EVENT_BATTERY                0x01
#define EC_EVENT_CHARGER                0x02
#define EC_EVENT_AC                     0x10
#define EC_EVENT_TIMER                  0x40

/* smbus access */
#define SMBUS_READ_BYTE_PRTCL		0x07
#define SMBUS_READ_WORD_PRTCL		0x09
#define SMBUS_READ_BLOCK_PRTCL		0x0B

/* smbus status code */
#define SMBUS_OK			0x00
#define SMBUS_DONE			0x80
#define SMBUS_ALARM			0x40
#define SMBUS_UNKNOW_FAILURE		0x07
#define SMBUS_DEVICE_NOACK		0x10
#define SMBUS_DEVICE_ERROR		0x11
#define SMBUS_UNKNOW_ERROR		0x13
#define SMBUS_TIME_OUT			0x18
#define SMBUS_BUSY			0x1A

/* ec ram mapping */
#define ECRAM_SMB_PRTCL			0
#define ECRAM_SMB_STS			1
#define ECRAM_SMB_ADDR			2
#define ECRAM_SMB_CMD			3
#define ECRAM_SMB_DATA_START		4
#define ECRAM_SMB_DATA0			4
#define ECRAM_SMB_DATA1			5
#define ECRAM_SMB_BCNT			36
#define ECRAM_SMB_ALARM_ADDR		37
#define ECRAM_SMB_ALARM_DATA0		38
#define ECRAM_SMB_ALARM_DATA1		39

/* smart battery commands */
#define BATTERY_SLAVE_ADDRESS		0x16
#define BATTERY_FULL_CAPACITY		0x10
#define BATTERY_AVERAGE_TIME_TO_EMPTY	0x12
#define BATTERY_AVERAGE_TIME_TO_FULL	0x13
#define BATTERY_CYCLE_COUNT		0x17
#define BATTERY_DESIGN_CAPACITY		0x18
#define BATTERY_DESIGN_VOLTAGE		0x19
#define BATTERY_SERIAL_NUMBER		0x1C
#define BATTERY_MANUFACTURE_NAME        0x20
#define BATTERY_DEVICE_NAME		0x21

/* alarm bit */
#define ALARM_REMAIN_CAPACITY           0x02
#define ALARM_OVER_TEMP                 0x10
#define ALARM_OVER_CHARGE               0x80
#endif

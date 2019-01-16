/* linux/drivers/hwmon/lis33de.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * MS5607 driver for MT6516
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef MS5607_H
#define MS5607_H 
	 
#include <linux/ioctl.h>

#define MS5607_I2C_SLAVE_ADDR		0xEE

	 
#define TRUE 1
#define FALSE 0
//#define F_CPU 4000000UL // 4 MHz external XTAL
//#define SCL_CLOCK 100000L // I2C clock in Hz
//#define ADDR_W 0xEF // Module address write mode
//#define ADDR_R 0xEF // Module address read mode
#define CMD_RESET 0x1E // ADC reset command
#define CMD_ADC_READ 0x00 // ADC read command
#define CMD_ADC_CONV 0x40 // ADC conversion command
#define CMD_ADC_D1 0x00 // ADC D1 conversion
#define CMD_ADC_D2 0x10 // ADC D2 conversion
#define CMD_ADC_256 0x00 // ADC OSR=256
#define CMD_ADC_512 0x02 // ADC OSR=512
#define CMD_ADC_1024 0x04 // ADC OSR=1024
#define CMD_ADC_2048 0x06 // ADC OSR=2048
#define CMD_ADC_4096 0x08 // ADC OSR=4096
#define CMD_PROM_RD 0xA0 // Prom read command

	 
	 
#define MS5607_SUCCESS						0
#define MS5607_ERR_I2C						-1
#define MS5607_ERR_STATUS					-3
#define MS5607_ERR_SETUP_FAILURE			-4
#define MS5607_ERR_GETGSENSORDATA			-5
#define MS5607_ERR_IDENTIFICATION			-6
	 
	 
	 
#define MS5607_BUFSIZE				256
	 
#endif


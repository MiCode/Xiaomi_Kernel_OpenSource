/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_DEF_H__
#define __ADAPTOR_DEF_H__

#define MODE_MAXCNT 16
#define OF_SENSOR_NAMES_MAXCNT 16
#define POWERON_ONCE_OPENED
#define IMGSENSOR_DEBUG
#define OF_SENSOR_NAME_PREFIX "sensor"

enum {
	CLK_6M = 0,
	CLK_12M,
	CLK_13M,
	CLK_24M,
	CLK_26M,
	CLK_48M,
	CLK_52M,
	CLK_MCLK,
	CLK_MAXCNT,
};

#define ADAPTOR_CLK_NAMES \
	"6", \
	"12", \
	"13", \
	"24", \
	"26", \
	"48", \
	"52", \
	"mclk", \

enum {
	STATE_MCLK_OFF = 0,
	STATE_MCLK_2MA,
	STATE_MCLK_4MA,
	STATE_MCLK_6MA,
	STATE_MCLK_8MA,
	STATE_RST_LOW,
	STATE_RST_HIGH,
	STATE_PDN_LOW,
	STATE_PDN_HIGH,
	STATE_MIPI_SWITCH_OFF,
	STATE_MIPI_SWITCH_ON,
	STATE_AVDD_OFF,
	STATE_AVDD_ON,
	STATE_DVDD_OFF,
	STATE_DVDD_ON,
	STATE_DOVDD_OFF,
	STATE_DOVDD_ON,
	STATE_AFVDD_OFF,
	STATE_AFVDD_ON,
	STATE_MAXCNT,
};

#define ADAPTOR_STATE_NAMES \
	"mclk_off", \
	"mclk_2mA", \
	"mclk_4mA", \
	"mclk_6mA", \
	"mclk_8mA", \
	"rst_low", \
	"rst_high", \
	"pdn_low", \
	"pdn_high", \
	"mipi_switch_off", \
	"mipi_switch_on", \
	"avdd_off", \
	"avdd_on", \
	"dvdd_off", \
	"dvdd_on", \
	"dovdd_off", \
	"dovdd_on", \
	"afvdd_off", \
	"afvdd_on", \

enum {
	REGULATOR_AVDD = 0,
	REGULATOR_DVDD,
	REGULATOR_DOVDD,
	REGULATOR_AFVDD,
	REGULATOR_MAXCNT,
};

#define ADAPTOR_REGULATOR_NAMES \
	"avdd", \
	"dvdd", \
	"dovdd", \
	"afvdd", \

#endif

/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_DEF_H__
#define __ADAPTOR_DEF_H__

#define MODE_MAXCNT 25
#define OF_SENSOR_NAMES_MAXCNT 20
//#define POWERON_ONCE_OPENED
#define IMGSENSOR_DEBUG
#define OF_SENSOR_NAME_PREFIX "sensor"

#define IMGSENSOR_LOG_MORE 0

enum {
	CLK_6M = 0,
	CLK_12M,
	CLK_13M,
	CLK_19_2M,
	CLK_24M,
	CLK_26M,
	CLK_52M,
	CLK_MCLK,
	CLK1_6M,
	CLK1_12M,
	CLK1_13M,
	CLK1_19_2M,
	CLK1_24M,
	CLK1_26M,
	CLK1_26M_ULPOSC,
	CLK1_52M,
	CLK1_MCLK1,
	CLK_MAXCNT,
};

#define ADAPTOR_CLK_NAMES \
	"6", \
	"12", \
	"13", \
	"19.2", \
	"24", \
	"26", \
	"52", \
	"mclk", \
	"clk1_6", \
	"clk1_12", \
	"clk1_13", \
	"clk1_19.2", \
	"clk1_24", \
	"clk1_26", \
	"clk1_26_ulposc", \
	"clk1_52", \
	"clk1_mclk1", \

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
	STATE_AFVDD1_OFF,
	STATE_AFVDD1_ON,
	STATE_AVDD1_OFF,
	STATE_AVDD1_ON,
	STATE_AVDD2_OFF,
	STATE_AVDD2_ON,
	STATE_MCLK1_OFF,
	STATE_MCLK1_2MA,
	STATE_MCLK1_4MA,
	STATE_MCLK1_6MA,
	STATE_MCLK1_8MA,
	STATE_DVDD1_OFF,
	STATE_DVDD1_ON,
	STATE_RST1_LOW,
	STATE_RST1_HIGH,
	STATE_PONV_LOW,
	STATE_PONV_HIGH,
	STATE_SCL_AP,
	STATE_SCL_SCP,
	STATE_SDA_AP,
	STATE_SDA_SCP,
	STATE_EINT,
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
	"afvdd1_off", \
	"afvdd1_on", \
	"avdd1_off", \
	"avdd1_on", \
	"avdd2_off", \
	"avdd2_on", \
	"mclk1_off", \
	"mclk1_2mA", \
	"mclk1_4mA", \
	"mclk1_6mA", \
	"mclk1_8mA", \
	"dvdd1_off", \
	"dvdd1_on", \
	"rst1_low", \
	"rst1_high", \
	"ponv_low", \
	"ponv_high", \
	"scl_ap", \
	"scl_scp", \
	"sda_ap", \
	"sda_scp", \
	"eint", \

enum {
	REGULATOR_AVDD = 0,
	REGULATOR_DVDD,
	REGULATOR_DOVDD,
	REGULATOR_AFVDD,
	REGULATOR_AFVDD1,
	REGULATOR_AVDD1,
	REGULATOR_AVDD2,
	REGULATOR_DVDD1,
	REGULATOR_MAXCNT,
};

#define ADAPTOR_REGULATOR_NAMES \
	"avdd", \
	"dvdd", \
	"dovdd", \
	"afvdd", \
	"afvdd1", \
	"avdd1", \
	"avdd2", \
	"dvdd1", \

/* Format code util */

#define to_std_fmt_code(code) \
	((code) & 0xFFFF)

#define to_mtk_ext_fmt_code(stdcode, mode) \
	(0x10000000 | (((mode) & 0xFF) << 16) | to_std_fmt_code(stdcode))

#define set_std_parts_fmt_code(code, stdcode) \
{ \
	code = (((code) & 0xFFFF0000) | to_std_fmt_code(stdcode)); \
}

#define is_mtk_ext_fmt_code(code) \
	(((code) >> 28) == 0x1)

#define get_sensor_mode_from_fmt_code(code) \
({ \
	int __val = 0; \
	__val = ((code) >> 16) & 0xFF; \
	__val; \
})

#endif

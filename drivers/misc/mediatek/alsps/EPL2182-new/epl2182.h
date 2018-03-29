/* drivers/hwmon/mt6516/amit/epl2182.c - EPL2182 ALS/PS driver
 *
 * Author: MingHsien Hsieh <minghsien.hsieh@mediatek.com>
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

#ifndef __EPL2182_H__
#define __EPL2182_H__

#define REG_0			0X00
#define REG_1			0X01
#define REG_2			0X02
#define REG_3			0X03
#define REG_4			0X04
#define REG_5			0X05
#define REG_6			0X06
#define REG_7			0X07
#define REG_8			0X08
#define REG_9			0X09
#define REG_10			0X0A
#define REG_11			0X0B
#define REG_12			0X0C
#define REG_13			0X0D
#define REG_14			0X0E
#define REG_15			0X0F
#define REG_16			0X10
#define REG_17			0X11
#define REG_18			0X12
#define REG_19			0X13
#define REG_20			0X14
#define REG_21			0X15

#define W_SINGLE_BYTE	0X00
#define W_TWO_BYTE		0X01
#define W_THREE_BYTE	0X02
#define W_FOUR_BYTE		0X03
#define W_FIVE_BYTE		0X04
#define W_SIX_BYTE		0X05
#define W_SEVEN_BYTE	0X06
#define W_EIGHT_BYTE	0X07

#define R_SINGLE_BYTE	0X00
#define R_TWO_BYTE		0X01
#define R_THREE_BYTE	0X02
#define R_FOUR_BYTE		0X03
#define R_FIVE_BYTE		0X04
#define R_SIX_BYTE		0X05
#define R_SEVEN_BYTE	0X06
#define R_EIGHT_BYTE	0X07

#define EPL_SENSING_1_TIME	(0 << 5)
#define EPL_SENSING_2_TIME	(1 << 5)
#define EPL_SENSING_4_TIME	(2 << 5)
#define EPL_SENSING_8_TIME	(3 << 5)
#define EPL_SENSING_16_TIME	(4 << 5)
#define EPL_SENSING_32_TIME	(5 << 5)
#define EPL_SENSING_64_TIME	(6 << 5)
#define EPL_SENSING_128_TIME (7 << 5)
#define EPL_C_SENSING_MODE	(0 << 4)
#define EPL_S_SENSING_MODE	(1 << 4)
#define EPL_ALS_MODE		(0 << 2)
#define EPL_PS_MODE			(1 << 2)
#define EPL_H_GAIN			(0)
#define EPL_M_GAIN			(1)
#define EPL_L_GAIN			(3)
#define EPL_AUTO_GAIN		(2)


#define EPL_8BIT_ADC		0
#define EPL_10BIT_ADC		1
#define EPL_12BIT_ADC		2
#define EPL_14BIT_ADC		3


#define EPL_C_RESET			0x00
#define EPL_C_START_RUN		0x04
#define EPL_C_P_UP			0x04
#define EPL_C_P_DOWN		0x06
#define EPL_DATA_LOCK_ONLY	0x01
#define EPL_DATA_LOCK		0x05
#define EPL_DATA_UNLOCK		0x04

#define EPL_GO_MID			0x1E
#define EPL_GO_LOW			0x1E

#define EPL_DRIVE_60MA		(0<<4)
#define EPL_DRIVE_120MA		(1<<4)

#define EPL_INT_BINARY			0
#define EPL_INT_DISABLE			2
#define EPL_INT_ACTIVE_LOW		3
#define EPL_INT_FRAME_ENABLE	4

#define EPL_PST_1_TIME		(0 << 2)
#define EPL_PST_4_TIME		(1 << 2)
#define EPL_PST_8_TIME		(2 << 2)
#define EPL_PST_16_TIME		(3 << 2)

/*----------------------------------------------------------------------------*/
enum EPL2182_NOTIFY_TYPE {
	EPL2182_NOTIFY_PROXIMITY_CHANGE = 1,
	EPL2182_NOTIFY_ALS_RAW_DATA,
	EPL2182_NOTIFY_PS_RAW_DATA,
	EPL2182_NOTIFY_PROXIMITY_NOT_CHANGE
};
/*----------------------------------------------------------------------------*/
enum EPL2182_CUST_ACTION {
	EPL2182_CUST_ACTION_SET_CUST = 1,
	EPL2182_CUST_ACTION_CLR_CALI,
	EPL2182_CUST_ACTION_SET_CALI,
	EPL2182_CUST_ACTION_SET_PS_THRESHODL,
	EPL2182_CUST_ACTION_SET_EINT_INFO,
	EPL2182_CUST_ACTION_GET_ALS_RAW_DATA,
	EPL2182_CUST_ACTION_GET_PS_RAW_DATA,
};
/*----------------------------------------------------------------------------*/
struct EPL2182_CUST {
	uint16_t action;
};
/*----------------------------------------------------------------------------*/
struct EPL2182_SET_CUST {
	uint16_t action;
	uint16_t part;
	int32_t data[0];
};
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
struct EPL2182_SET_CALI {
	uint16_t action;
	int32_t cali;
};
/*----------------------------------------------------------------------------*/
struct EPL2182_SET_PS_THRESHOLD {
	uint16_t action;
	int32_t threshold[2];
};
/*----------------------------------------------------------------------------*/
struct EPL2182_SET_EINT_INFO {
	uint16_t action;
	uint32_t gpio_pin;
	uint32_t gpio_mode;
	uint32_t eint_num;
	uint32_t eint_is_deb_en;
	uint32_t eint_type;
};
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
union EPL2182_CUST_DATA {
	uint32_t data[10];
	struct EPL2182_CUST cust;
	struct EPL2182_SET_CUST setCust;
	struct EPL2182_CUST clearCali;
	struct EPL2182_SET_CALI setCali;
	struct EPL2182_SET_PS_THRESHOLD setPSThreshold;
	struct EPL2182_SET_EINT_INFO setEintInfo;
	struct EPL2182_CUST getALSRawData;
	struct EPL2182_CUST getPSRawData;
};
/*----------------------------------------------------------------------------*/
extern struct platform_device *get_alsps_platformdev(void);

#endif

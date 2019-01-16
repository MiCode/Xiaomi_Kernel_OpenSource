/* include/linux/input/max1187x.h
 *
 * Copyright (c)2012 Maxim Integrated Products, Inc.
 *
 * Driver Version: 3.1.4
 * Release Date: Mar 27, 2013
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

#ifndef __MAX1187X_H
#define __MAX1187X_H

#define MAX1187X_NAME   "max1187x"
#define MAX1187X_TOUCH  MAX1187X_NAME "_touchscreen_0"
#define MAX1187X_KEY    MAX1187X_NAME "_key_0"

#define CMD_LEN_PACKET_MAX  9  /* command address space 0x00-0x09 minus header
				=> 9 command words maximum */
#define CMD_LEN_MAX  (15 * CMD_LEN_PACKET_MAX)  /* maximum 15 packets
				9 payload words each */
#define RPT_LEN_PACKET_MAX   245  /* address space 0x00-0xFF minus 0x00-0x09 for
				commands minus header, maximum 1 report packet*/
#define RPT_LEN_MAX   1000  /* Maximum report size */

#define MAX1187X_NUM_FW_MAPPINGS_MAX    5
#define MAX1187X_TOUCH_COUNT_MAX        10
#define MAX1187X_REPORT_TOUCH_RAW       0x0800
#define MAX1187X_REPORT_TOUCH_BASIC     0x0801
#define MAX1187X_REPORT_TOUCH_EXTENDED  0x0802
#define MAX1187X_REPORT_POWER_MODE	0x0121
/* #define MAX1187X_PROTOCOL_A */
#define MAX1187X_TOUCH_REPORT_MODE 1 /* 1=basic, 2=extended */
#define MAX1187X_REPORT_FAST_CALCULATION
#define MAX_REPORT_READERS	5
#define FW_DOWNLOAD_FEATURE
/* #define TOUCH_WAKEUP_FEATURE */
#define DEBUG_STRING_LEN_MAX 60
#define MAX_FW_RETRIES 5

/***************************++MTK add++************************************/
#pragma pack(1)
typedef struct
{
    u16 pid;                 //product id   //
    u16 vid;                 //version id   //
} st_tpd_info;
#pragma pack()

#define TPD_KEY_COUNT   4
#define key_1           60,850              //auto define  
#define key_2           180,850
#define key_3           300,850
#define key_4           420,850

#define TPD_KEYS        {KEY_BACK, KEY_HOME, KEY_MENU, KEY_SEARCH}
#define TPD_KEYS_DIM    {{key_1,50,30},{key_2,50,30},{key_3,50,30},{key_4,50,30}}

#define MAX1187X_LOCAL_PDATA
#define MAX1187X_PROTOCOL_A

#ifdef CONFIG_OF
#define CONFIG_OF_TOUCH
#endif
/***************************--MTK add--************************************/

#define STATUS_ADDR_H 0x00
#define STATUS_ADDR_L 0xFF
#define DATA_ADDR_H   0x00
#define DATA_ADDR_L   0xFE
#define STATUS_READY_H 0xAB
#define STATUS_READY_L 0xCC
#define RXTX_COMPLETE_H 0x54
#define RXTX_COMPLETE_L 0x32


#ifndef MAX1187X_REPORT_FAST_CALCULATION
#define MAX1187X_PI 205887 /* pi multiplied by 2^16 */
/* tanlist - array containing tan(i)*(2^16-1) for i=[0,45], i in degrees */
u16 tanlist[] = {0, 1144, 2289, 3435, 4583, 5734,
			6888, 8047, 9210, 10380, 11556, 12739,
			13930, 15130, 16340, 17560, 18792, 20036,
			21294, 22566, 23853, 25157, 26478, 27818,
			29178, 30559, 31964, 33392, 34846, 36327,
			37837, 39377, 40951, 42559, 44204, 45888,
			47614, 49384, 51202, 53069, 54990, 56969,
			59008, 61112, 63286, 65535};
#endif

struct max1187x_touch_report_header {
	u16 header;
	u16 report_id;
	u16 report_size;
	u16 touch_count:4;
	u16 reserved0:12;
	u16 button0:1;
	u16 button1:1;
	u16 button2:1;
	u16 button3:1;
	u16 reserved1:12;
	u16 framecounter;
};

struct max1187x_touch_report_basic {
	u16 finger_id:4;
	u16 reserved0:4;
	u16 tool_type:4;
	u16 reserved1:4;
	u16 x:12;
	u16 reserved2:4;
	u16 y:12;
	u16 reserved3:4;
	u16 reserved4:8;
	u16 z:8;
};

struct max1187x_touch_report_extended {
	u16 finger_id:4;
	u16 reserved0:4;
	u16 tool_type:4;
	u16 reserved1:4;
	u16 x:12;
	u16 reserved2:4;
	u16 y:12;
	u16 reserved3:4;
	u16 reserved4:8;
	u16 z:8;
	s16 xspeed;
	s16 yspeed;
	s8 xpixel;
	s8 ypixel;
	u16 area;
	u16 xmin;
	u16 xmax;
	u16 ymin;
	u16 ymax;
};

struct max1187x_fw_mapping {
	u32				config_id;
	u32				chip_id;
	char			*filename;
	u32				filesize;
	u32				filecrc16;
	u32				file_codesize;
};

struct max1187x_pdata {
	u32			gpio_tirq;
	u32			num_fw_mappings;
	struct max1187x_fw_mapping  fw_mapping[MAX1187X_NUM_FW_MAPPINGS_MAX];
	u32			defaults_allow;
	u32			default_config_id;
	u32			default_chip_id;
	u32			i2c_words;
	#define MAX1187X_REVERSE_X  0x0001
	#define MAX1187X_REVERSE_Y  0x0002
	#define MAX1187X_SWAP_XY    0x0004
	u32			coordinate_settings;
	u32			panel_margin_xl;
	u32			lcd_x;
	u32			panel_margin_xh;
	u32			panel_margin_yl;
	u32			lcd_y;
	u32			panel_margin_yh;
	u32			num_sensor_x;
	u32			num_sensor_y;
	u32			button_code0;
	u32			button_code1;
	u32			button_code2;
	u32			button_code3;
};

#endif /* __MAX1187X_H */


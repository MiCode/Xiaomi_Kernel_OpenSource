/* drivers/input/touchscreen/gt1x_generic.h
 *
 * 2010 - 2014 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 1.4
 * Release Date:  2015/07/10
 */

#ifndef _GT1X_GENERIC_H_
#define _GT1X_GENERIC_H_

#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/********* Device Tree Support *********/
#ifdef CONFIG_OF
#define GTP_CONFIG_OF
#endif
/***************************PART1:ON/OFF define*******************************/
#define GTP_INCELL_PANEL      0
#define GTP_DRIVER_SEND_CFG   1
#define GTP_CUSTOM_CFG        1

#define GTP_CHANGE_X2Y        0
#define GTP_WARP_X_ON         0
#define GTP_WARP_Y_ON         0

#define GTP_GESTURE_WAKEUP    1
/* buffer used to store ges track points coor. */

#define GES_BUFFER_ADDR       0xBE0C



#define EGDE_INHIBITION_ADDR 0x43

#ifndef GES_BUFFER_ADDR
#warning  [GOODIX] need define GES_BUFFER_ADDR .
#endif
#define KEY_GES_REGULAR       KEY_F2
#define KEY_GES_CUSTOM        KEY_F3

#define GTP_HOTKNOT           0
#define HOTKNOT_TYPE          0
#define HOTKNOT_BLOCK_RW      0

#define GTP_AUTO_UPDATE       1
#define GTP_HEADER_FW_UPDATE  1
#define GTP_FW_UPDATE_VERIFY  1

#define GTP_REQUEST_FW_UPDATE 0

#define GTP_HAVE_TOUCH_KEY    0
#define GTP_WITH_STYLUS       0
#define GTP_HAVE_STYLUS_KEY   0

#define GTP_POWER_CTRL_SLEEP  0
#define GTP_ICS_SLOT_REPORT   1
#define GTP_CREATE_WR_NODE    1

#define GTP_PROXIMITY         0
#define GTP_SMART_COVER       0

#define GTP_ESD_PROTECT       1
#define GTP_CHARGER_SWITCH    0

#define GTP_DEBUG_ON          0
#define GTP_DEBUG_ARRAY_ON    0
#define GTP_DEBUG_FUNC_ON     0
#define SUPPORT_READ_TP_VERSION
#define EDGE_INHIBITION
#define EDGE_INHIBITION_PROC  "edge_control"

/***************************PART2:TODO define**********************************/
/* Normal Configs
 *  TODO: puts the config info corresponded to your TP here, the following is just
 *         a sample config, send this config should cause the chip cannot work normally
 */
#define CFG_GROUP_LEN(p_cfg_grp)  (sizeof(p_cfg_grp) / sizeof(p_cfg_grp[0]))
/* TODO define your config for Sensor_ID == 0 here, if needed */
#define GTP_CFG_GROUP0 {\
	0x53, 0xD0, 0x02, 0xF0, 0x05, 0x0A, 0x3C, 0x14, 0xD2, 0x88, 0xCE, 0x0F, 0x5A, 0x3C, 0x3E, 0x11, 0x00, 0x10, 0x0A, 0x1E,\
	0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x43, 0x00, 0xA0, 0x0A, 0x0C, 0x5A, 0x2D, 0x64, 0x11, 0x10, 0x88, 0x94,\
	0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC7, 0xE1, 0x15, 0x34, 0x50, 0xBE, 0x00, 0x00, 0x8E, 0x20, 0x1D, 0x2D, 0x2F, 0xB8,\
	0x08, 0x00, 0x2E, 0x13, 0x24, 0x7B, 0x00, 0x00, 0x55, 0xDC, 0x05, 0x41, 0x43, 0x02, 0x6E, 0x0F, 0x14, 0x00, 0x00, 0x00,\
	0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x20, 0x48, 0xC0, 0x8A, 0xB4, 0x28, 0x14, 0x04, 0xAA, 0x26, 0x93, 0x2D, 0x85, 0x34, 0x7A, 0x3A,\
	0x73, 0x41, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1D, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x10, 0x0F,\
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0xFF, 0xFF, 0xFF, 0x06, 0x08, 0x0C,\
	0x12, 0x13, 0x14, 0x15, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,\
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xF8, 0x0A, 0x24, 0x23, 0x2A, 0x7E, 0x05, 0x50, 0x32, 0x05,\
	0x42, 0x00, 0x00, 0xA4, 0x61, 0x00, 0x6A, 0xF4, 0x40, 0x00, 0xAA, 0x00, 0x33, 0x33, 0x00, 0x40, 0x56, 0x60, 0x01, 0xD3,\
	0x32, 0x78, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0xA5, 0x00, 0x00, 0x00, 0x00, 0x78, 0x04, 0x65, 0x00, 0x00,\
	0x02, 0x12, 0x00, 0x16, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x55, 0x77, 0x02, 0x0F, 0x32, 0x78, 0xAA,\
	0xA6, 0x04, 0x00, 0x75, 0x23, 0x0F, 0x01, 0xDC, 0x00, 0x00, 0x00, 0x14, 0x28, 0x46, 0x11, 0x20, 0x34, 0xEF, 0x86, 0xC4,\
	0x09, 0x0C, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F,\
	0x00, 0x00, 0x62, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x60, 0x00, 0x00, 0x14, 0x00,\
	0x0A, 0x03, 0x06, 0x2D, 0x33, 0x8A, 0x55, 0x00, 0x64, 0x28, 0x22, 0x5A, 0x1E, 0x19, 0x00, 0x0A, 0x1E, 0x25, 0xA2, 0x22,\
	0xB8, 0x94, 0x26, 0x42, 0x90, 0xA8, 0xB0\
}
/* TODO define your config for Sensor_ID == 1 here, if needed */
#define GTP_CFG_GROUP1 {\
    }
/* TODO define your config for Sensor_ID == 2 here, if needed */
#define GTP_CFG_GROUP2 {\
    }
/* TODO define your config for Sensor_ID == 3 here, if needed */
#define GTP_CFG_GROUP3 {\
    }
/* TODO define your config for Sensor_ID == 4 here, if needed */
#define GTP_CFG_GROUP4 {\
    }
/* TODO define your config for Sensor_ID == 5 here, if needed */
#define GTP_CFG_GROUP5 {\
    }

/*
  *         Charger Configs
*/
/* TODO define your config for Sensor_ID == 0 here, if needed */
#define GTP_CHARGER_CFG_GROUP0 {\
}
/* TODO define your config for Sensor_ID == 1 here, if needed */
#define GTP_CHARGER_CFG_GROUP1 {\
}
/* TODO define your config for Sensor_ID == 2 here, if needed */
#define GTP_CHARGER_CFG_GROUP2 {\
}
/* TODO define your config for Sensor_ID == 3 here, if needed */
#define GTP_CHARGER_CFG_GROUP3 {\
}
/* TODO define your config for Sensor_ID == 4 here, if needed */
#define GTP_CHARGER_CFG_GROUP4 {\
}
/* TODO define your config for Sensor_ID == 5 here, if needed */
#define GTP_CHARGER_CFG_GROUP5 {\
}

/*
  *         Smart Cover Configs
*/
/* TODO define your config for Sendor_ID == 0 here, if needed */
#define GTP_SMART_COVER_CFG_GROUP0 {\
	0x46, 0xD0, 0x02, 0x64, 0x02, 0x05, 0xBD, 0x10, 0x00, 0x08, 0x00, 0x0F, 0x50, 0x28, 0x5E,\
	0x02, 0x11, 0x00, 0x00, 0x00, 0x28, 0x82, 0x96, 0xFC, 0xC8, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x87, 0x26, 0x0B, 0x64, 0x66, 0xDF, 0x07, 0x91, 0x31, 0x18, 0x0E, 0x43, 0x24, 0x00,\
	0x04, 0x28, 0x6E, 0x80, 0x94, 0x02, 0x05, 0x08, 0x04, 0xDA, 0x33, 0xAF, 0x3F, 0x92, 0x4A,\
	0x7F, 0x56, 0x71, 0x62, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x0F, 0x19, 0x04, 0x0F, 0x10, 0x42, 0xD8, 0x0F,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x1E,\
	0x00, 0x70, 0x17, 0x50, 0x1E, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0F, 0x0E, 0x10, 0x0D, 0x12,\
	0x13, 0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0xFF, 0xFF,\
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,\
	0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x00, 0x00, 0x00, 0x00, 0x24, 0x1E, 0x6D,\
	0x00, 0x14, 0x28, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9E, 0x29, 0x01\
}
/* TODO define your config for Sendor_ID == 0 here, if needed */
#define GTP_SMART_COVER_CFG_GROUP1 {\
}
/* TODO define your config for Sendor_ID == 0 here, if needed */
#define GTP_SMART_COVER_CFG_GROUP2 {\
}
/* TODO define your config for Sendor_ID == 0 here, if needed */
#define GTP_SMART_COVER_CFG_GROUP3 {\
}
/* TODO define your config for Sendor_ID == 0 here, if needed */
#define GTP_SMART_COVER_CFG_GROUP4 {\
}
/* TODO define your config for Sendor_ID == 0 here, if needed */
#define GTP_SMART_COVER_CFG_GROUP5 {\
}

#if GTP_CUSTOM_CFG
#define GTP_MAX_HEIGHT   1520
#define GTP_MAX_WIDTH    720
#define GTP_INT_TRIGGER  0
#define GTP_WAKEUP_LEVEL 1
#else
#define GTP_MAX_HEIGHT   2160
#define GTP_MAX_WIDTH    1080
#define GTP_INT_TRIGGER  1
#define GTP_WAKEUP_LEVEL 0
#endif

#define GTP_MAX_TOUCH    10

#if GTP_WITH_STYLUS
#define GTP_STYLUS_KEY_TAB {BTN_STYLUS, BTN_STYLUS2}
#endif

#if GTP_HAVE_TOUCH_KEY
#define GTP_KEY_TAB	 {KEY_BACK, KEY_HOMEPAGE, KEY_MENU, KEY_SEARCH}
#define GTP_MAX_KEY_NUM  4
#endif

/****************************PART3:OTHER define*********************************/
#define GTP_DRIVER_VERSION          "V1.4<2015/07/10>"
#define GTP_I2C_NAME                "Goodix-TS"
#define GT1X_DEBUG_PROC_FILE        "gt1x_debug"
#define GTP_POLL_TIME               10
#define GTP_ADDR_LENGTH             2
#define GTP_CONFIG_MIN_LENGTH       186
#define GTP_CONFIG_MAX_LENGTH       368
#define GTP_CONFIG_ORG_LENGTH       239
#define GTP_CONFIG_EXT_LENGTH       128
#define GTP_MAX_I2C_XFER_LEN        250
#define SWITCH_OFF                  0
#define SWITCH_ON                   1

#define GTP_REG_MATRIX_DRVNUM           0x8069
#define GTP_REG_MATRIX_SENNUM           0x806A
#define GTP_REG_RQST                    0x8044
#define GTP_REG_BAK_REF                 0x90EC
#define GTP_REG_MAIN_CLK                0x8020
#define GTP_REG_HAVE_KEY                0x8057
#define GTP_REG_HN_STATE                0x8800
#define GTP_REG_COLOR_GT1151Q			0x99A4

#define GTP_REG_WAKEUP_GESTURE         0x814C
#define GTP_REG_WAKEUP_GESTURE_DETAIL  0xBE0C
#define GTP_REG_LOCKDOWN               0x99A4

#define GTP_BAK_REF_PATH                "/data/gt1x_ref.bin"
#define GTP_MAIN_CLK_PATH               "/data/gt1x_clk.bin"

/* request type */
#define GTP_RQST_CONFIG                 0x01
#define GTP_RQST_BAK_REF                0x02
#define GTP_RQST_RESET                  0x03
#define GTP_RQST_MAIN_CLOCK             0x04
#define GTP_RQST_HOTKNOT_CODE           0x20
#define GTP_RQST_RESPONDED              0x00
#define GTP_RQST_IDLE                   0xFF

#define HN_DEVICE_PAIRED                0x80
#define HN_MASTER_DEPARTED              0x40
#define HN_SLAVE_DEPARTED               0x20
#define HN_MASTER_SEND                  0x10
#define HN_SLAVE_RECEIVED               0x08

/*Register define */
#define GTP_READ_COOR_ADDR          0x814E
#define GTP_REG_CMD                 0x8040
#define GTP_REG_SENSOR_ID           0x814A
#define GTP_REG_CONFIG_DATA         0x8050
#define GTP_REG_CONFIG_RESOLUTION   0x8051
#define GTP_REG_CONFIG_TRIGGER      0x8056
#define GTP_REG_CONFIG_CHECKSUM     0x813C
#define GTP_REG_CONFIG_UPDATE       0x813E
#define GTP_REG_EXT_CONFIG          0xBF7B
#define GTP_REG_VERSION             0x8140
#define GTP_REG_HW_INFO             0x4220
#define GTP_REG_REFRESH_RATE	    0x8056
#define GTP_REG_ESD_CHECK           0x8043
#define GTP_REG_FLASH_PASSBY        0x8006
#define GTP_REG_HN_PAIRED           0x81AA
#define GTP_REG_HN_MODE             0x81A8
#define GTP_REG_MODULE_SWITCH3      0x8058
#define GTP_REG_FW_CHK_MAINSYS      0x41E4
#define GTP_REG_FW_CHK_SUBSYS       0x5095

#define set_reg_bit(reg,pos,val)   ((reg)=((reg) & (~(1<<(pos))))|(!!(val)<<(pos)))

/* cmd define */
#define GTP_CMD_SLEEP               0x05
#define GTP_CMD_CHARGER_ON          0x06
#define GTP_CMD_CHARGER_OFF         0x07
#define GTP_CMD_GESTURE_WAKEUP      0x08
#define GTP_CMD_CLEAR_CFG           0x10
#define GTP_CMD_ESD                 0xAA
#define GTP_CMD_HN_TRANSFER         0x22
#define GTP_CMD_HN_EXIT_SLAVE       0x28

/* define offset in the config*/
#define RESOLUTION_LOC              (GTP_REG_CONFIG_RESOLUTION - GTP_REG_CONFIG_DATA)
#define TRIGGER_LOC                 (GTP_REG_CONFIG_TRIGGER - GTP_REG_CONFIG_DATA)
#define MODULE_SWITCH3_LOC	     	(GTP_REG_MODULE_SWITCH3 - GTP_REG_CONFIG_DATA)

#define GTP_I2C_ADDRESS       		0x28

#if GTP_WARP_X_ON
#define GTP_WARP_X(x_max, x) ( x_max - 1 - x)
#else
#define GTP_WARP_X(x_max, x) x
#endif

#if GTP_WARP_Y_ON
#define GTP_WARP_Y(y_max, y) ( y_max - 1 - y)
#else
#define GTP_WARP_Y(y_max, y) y
#endif

#define IS_NUM_OR_CHAR(x)    (((x) >= 'A' && (x) <= 'Z') || ((x) >= '0' && (x) <= '9'))


#define GTP_INFO(fmt,arg...)           printk("<<GTP-INF>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define GTP_ERROR(fmt,arg...)          printk("<<GTP-ERR>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define GTP_DEBUG(fmt,arg...)          do{\
                                         if (GTP_DEBUG_ON)\
                                         printk("<<GTP-DBG>>[%s:%d]"fmt"\n",__func__, __LINE__, ##arg);\
                                       }while(0)
#define GTP_DEBUG_ARRAY(array, num)    do{\
                                         s32 i;\
                                         u8 *a = array;\
                                         if (GTP_DEBUG_ARRAY_ON)\
                                         {\
                                            printk("<<GTP-DBG>>");\
                                            for (i = 0; i < (num); i++)\
                                            {\
                                                printk("%02x ", (a)[i]);\
                                                if ((i + 1) %10 == 0)\
                                                {\
                                                    printk("\n<<GTP-DBG>>");\
                                                }\
                                            }\
                                            printk("\n");\
                                        }\
                                       }while(0)
#define GTP_DEBUG_FUNC()               do{\
                                         if (GTP_DEBUG_FUNC_ON)\
                                         printk("<<GTP-FUNC>> Func:%s@Line:%d\n",__func__,__LINE__);\
                                       }while(0)

#define GTP_SWAP(x, y)                 do{\
                                         typeof(x) z = x;\
                                         x = y;\
                                         y = z;\
                                       }while (0)

#pragma pack(1)
struct gt1x_version_info {
	u8 product_id[5];
	u32 patch_id;
	u32 mask_id;
	u8 sensor_id;
	u8 match_opt;
};
#pragma pack()

typedef enum {
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
} DOZE_T;

typedef enum {
	CHIP_TYPE_GT1X = 0,
	CHIP_TYPE_GT2X = 1,
	CHIP_TYPE_NONE = 0xFF
} CHIP_TYPE_T;

#define _ERROR(e)      ((0x01 << e) | (0x01 << (sizeof(s32) * 8 - 1)))
#define ERROR          _ERROR(1)

#define ERROR_IIC      _ERROR(2)
#define ERROR_MEM      _ERROR(3)


#define ERROR_HN_VER   _ERROR(10)
#define ERROR_CHECK    _ERROR(11)
#define ERROR_RETRY    _ERROR(12)
#define ERROR_PATH     _ERROR(13)
#define ERROR_FW       _ERROR(14)
#define ERROR_FILE     _ERROR(15)
#define ERROR_VALUE    _ERROR(16)

/* bit operation */
#define SET_BIT(data, flag)	((data) |= (flag))
#define CLR_BIT(data, flag)	((data) &= ~(flag))
#define CHK_BIT(data, flag)	((data) & (flag))

/* touch states */
#define BIT_TOUCH			0x01
#define BIT_TOUCH_KEY		0x02
#define BIT_STYLUS			0x04
#define BIT_STYLUS_KEY		0x08
#define BIT_HOVER			0x10

#include <linux/input.h>
struct i2c_msg;

/*          Export global variables and functions          */

/* Export from gt1x_extents.c and gt1x_firmware.h */
#if GTP_HOTKNOT
extern u8 hotknot_enabled;
extern u8 hotknot_transfer_mode;
extern u8 gt1x_patch_jump_fw[];
extern u8 hotknot_auth_fw[];
extern u8 hotknot_transfer_fw[];
#if HOTKNOT_BLOCK_RW
extern s32 hotknot_paired_flag;
extern s32 hotknot_event_handler(u8 *data);
#endif
#endif

extern s32 gt1x_init_node(void);
extern void gt1x_deinit_node(void);

#if GTP_GESTURE_WAKEUP
extern DOZE_T gesture_doze_status;
extern int gesture_enabled;
extern void gt1x_gesture_debug(int on) ;
extern s32 gesture_event_handler(struct input_dev *dev);
extern s32 gesture_enter_doze(void);
extern void gesture_clear_wakeup_data(void);
extern int gt1x_select_gesture_mode(struct input_dev *dev,unsigned int type,unsigned int code,int value);
#endif

struct goodix_pinctrl {
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *int_default;
	struct pinctrl_state *int_out_high;
	struct pinctrl_state *int_out_low;
	struct pinctrl_state *int_input;
	struct pinctrl_state *erst_as_default;
        struct pinctrl_state *erst_output_low;
        struct pinctrl_state *erst_output_high;

};


/* Export from gt1x_tpd.c */
extern struct goodix_pinctrl gt_pinctrl;
extern void gt1x_touch_down(s32 x, s32 y, s32 size, s32 id);
extern void gt1x_touch_up(s32 id);
extern int gt1x_power_switch(s32 state);
extern void gt1x_irq_enable(void);
extern void gt1x_irq_disable(void);
extern int gt1x_debug_proc(u8 *buf, int count);

struct fw_update_info {
	int update_type;
	int status;
	int progress;
	int max_progress;
    int force_update;
	struct fw_info *firmware;
	u32 fw_length;
    const struct firmware *fw;

	char *fw_name;
	u8 *buffer;
	mm_segment_t old_fs;
	struct file *fw_file;


	u8 *fw_data;
};

/* Export form gt1x_update.c */
extern struct fw_update_info update_info;

extern u8 gt1x_default_FW[];
extern int gt1x_hold_ss51_dsp(void);
extern int gt1x_auto_update_proc(void *data);
extern int gt1x_update_firmware(void *filename);
extern void gt1x_enter_update_mode(void);
extern void gt1x_leave_update_mode(void);
extern int gt1x_hold_ss51_dsp_no_reset(void);
extern int gt1x_load_patch(u8 *patch, u32 patch_size, int offset, int bank_size);
extern int gt1x_startup_patch(void);

/* Export from gt1x_tool.c */
#if GTP_CREATE_WR_NODE
extern int gt1x_init_tool_node(void);
extern void gt1x_deinit_tool_node(void);
#endif

/* Export from gt1x_generic.c */
extern struct i2c_client *gt1x_i2c_client;

extern CHIP_TYPE_T gt1x_chip_type;
extern struct gt1x_version_info gt1x_version;

extern s32 _do_i2c_read(struct i2c_msg *msgs, u16 addr, u8 *buffer, s32 len);
extern s32 _do_i2c_write(struct i2c_msg *msg, u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_write(u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_read(u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_read_dbl_check(u16 addr, u8 *buffer, s32 len);

extern u8 gt1x_config[];
extern u32 gt1x_cfg_length;
extern u8 gt1x_int_type;
extern u8 gt1x_wakeup_level;
extern u32 gt1x_abs_x_max;
extern u32 gt1x_abs_y_max;
extern u8 gt1x_init_failed;
extern int gt1x_halt;
extern volatile int gt1x_rawdiff_mode;

extern s32 gt1x_init(void);
extern void gt1x_deinit(void);
extern s32 gt1x_read_version(struct gt1x_version_info *ver_info);
extern s32 gt1x_init_panel(void);
extern s32 gt1x_get_chip_type(void);
extern s32 gt1x_request_event_handler(void);
extern int gt1x_send_cmd(u8 cmd, u8 data);
extern s32 gt1x_send_cfg(u8 *config, int cfg_len);
extern void gt1x_select_addr(void);
extern s32 gt1x_reset_guitar(void);
extern void gt1x_power_reset(void);
extern int gt1x_parse_config(char *filename, u8 *gt1x_config);
extern s32 gt1x_touch_event_handler(u8 *data, struct input_dev *dev, struct input_dev *pen_dev);
extern void gt1x_suspend(struct work_struct *work);
extern void gt1x_resume(struct work_struct *work);

#if GTP_HAVE_TOUCH_KEY
extern const u16 gt1x_touch_key_array[];
#endif

#if GTP_WITH_STYLUS
extern struct input_dev *pen_dev;
extern void gt1x_pen_up(s32 id);
extern void gt1x_pen_down(s32 x, s32 y, s32 size, s32 id);
#endif

#if GTP_PROXIMITY
extern u8 gt1x_proximity_flag;
extern int gt1x_prox_event_handler(u8 *data);
#endif

#if GTP_SMART_COVER
extern int gt1x_parse_sc_cfg(int sensor_id);
#endif

#if GTP_ESD_PROTECT
extern void gt1x_init_esd_protect(void);
extern void gt1x_esd_switch(s32 on);
#endif

#if GTP_CHARGER_SWITCH
extern u32 gt1x_get_charger_status(void);
extern void gt1x_charger_switch(s32 on);
extern void gt1x_charger_config(s32 dir_update);
extern int gt1x_parse_chr_cfg(int sensor_id);
#endif

#ifdef EDGE_INHIBITION
extern s32 edge_inhibition_init(void);
#endif


#endif


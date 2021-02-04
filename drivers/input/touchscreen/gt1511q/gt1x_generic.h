//* drivers/input/touchscreen/gt1x_generic.h
//
// 2010 - 2016 Goodix Technology.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be a reference
// to you, when you are integrating the GOODiX's CTP IC into your system,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// Version: 1.6


#ifndef _GT1X_GENERIC_H_
#define _GT1X_GENERIC_H_

#include <linux/string.h>
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
#include <linux/irq.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/hqsysfs.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#endif
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define GTP_DRIVER_VERSION			"V1.6<2016/11/02>"
#define GTP_I2C_NAME				"Goodix-TS"
#define GT1X_DEBUG_PROC_FILE		"gt1x_debug"
#define GTP_POLL_TIME				10
#define GTP_ADDR_LENGTH				2
#define GTP_CONFIG_MIN_LENGTH		186
#define GTP_CONFIG_MAX_LENGTH		240
#define GTP_CONFIG_ORG_LENGTH		239
#define GTP_CONFIG_EXT_LENGTH		128
#define GTP_MAX_I2C_XFER_LEN		250
#define SWITCH_OFF					0
#define SWITCH_ON					1

#define GTP_VENDOR_INFO                             "[Vendor]holitech(TP) + holitech(LCD), [TP-IC]GT1151Q, [FW]Ver"

// buffer used to store ges track points coor. */
//#define GES_BUFFER_ADDR		0xA2A0  // GT1151
#define GES_BUFFER_ADDR			0xBE0C	//GT1151Q
//#define GES_BUFFER_ADDR		0x9734  // GT1152
//#define GES_BUFFER_ADDR		0xBDA8  // GT9286
//#define GES_BUFFER_ADDR		0xBC74  // GT6286
#ifndef GES_BUFFER_ADDR
#warning  [GOODIX] need define GES_BUFFER_ADDR .
#endif

#define KEY_GES_REGULAR			KEY_F2	// regular gesture-key
#define KEY_GES_CUSTOM			KEY_F3	//customize gesture-key

#ifdef CONFIG_GTP_DEBUG_ON
#define GTP_DEBUG_ON	1
#else
#define GTP_DEBUG_ON	0
#endif

#ifdef CONFIG_GTP_DEBUG_ARRAY_ON
#define GTP_DEBUG_ARRAY_ON	1
#else
#define GTP_DEBUG_ARRAY_ON	0
#endif

#ifdef CONFIG_GTP_DEBUG_FUNC_ON
#define GTP_DEBUG_FUNC_ON	1
#else
#define GTP_DEBUG_FUNC_ON	0
#endif

#ifdef CONFIG_GTP_CUSTOM_CFG
#define GTP_MAX_HEIGHT		1440
#define GTP_MAX_WIDTH		720
#define GTP_INT_TRIGGER		1	//0:Rising 1:Falling
#define GTP_WAKEUP_LEVEL	1
#else
#define GTP_MAX_HEIGHT		4096
#define GTP_MAX_WIDTH		4096
#define GTP_INT_TRIGGER		1
#define GTP_WAKEUP_LEVEL	1
#endif

#define GTP_MAX_TOUCH	10
#define	INVALID			-1
#define	VALID			1

#define FW_NAME_MAX_LEN	80

#ifdef CONFIG_GTP_WITH_STYLUS
#define GTP_STYLUS_KEY_TAB {BTN_STYLUS, BTN_STYLUS2}
#endif

#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
#define GTP_KEY_TAB	 {KEY_BACK, KEY_HOMEPAGE, KEY_MENU, KEY_SEARCH}
#define GTP_MAX_KEY_NUM  4
#endif

#define GTP_REG_MATRIX_DRVNUM			0x8069
#define GTP_REG_MATRIX_SENNUM			0x806A
#define GTP_REG_RQST					0x8044
#define GTP_REG_BAK_REF					0x90EC
#define GTP_REG_MAIN_CLK				0x8020
#define GTP_REG_HAVE_KEY				0x8057
#define GTP_REG_HN_STATE				0x8800
#define GTP_REG_COLOR_GT1151Q			0x99A4

#define GTP_REG_WAKEUP_GESTURE			0x814C
#define GTP_REG_WAKEUP_GESTURE_DETAIL	0xA2A0	// need change
#define GTP_BAK_REF_PATH				"/data/gt1x_ref.bin"
#define GTP_MAIN_CLK_PATH				"/data/gt1x_clk.bin"

#define GT1X_LOCKDOWN_PROC_FILE			"tp_lockdown_info"

#define GT1X_CONFIG_VERSION_PROC_FILE		"goodix_cfg_ver"

#define EDGE_INHIBITION_PROC			"edge_control"

// request type */
#define GTP_RQST_CONFIG					0x01
#define GTP_RQST_BAK_REF				0x02
#define GTP_RQST_RESET					0x03
#define GTP_RQST_MAIN_CLOCK				0x04
#define GTP_RQST_HOTKNOT_CODE			0x20
#define GTP_RQST_RESPONDED				0x00
#define GTP_RQST_IDLE					0xFF

#define HN_DEVICE_PAIRED				0x80
#define HN_MASTER_DEPARTED				0x40
#define HN_SLAVE_DEPARTED				0x20
#define HN_MASTER_SEND					0x10
#define HN_SLAVE_RECEIVED				0x08
#define EGDE_INHIBITION_ADDR			0x43

//Register define */
#define GTP_READ_COOR_ADDR			0x814E
#define GTP_REG_CMD					0x8040
#define GTP_REG_SENSOR_ID			0x814A
#define GTP_REG_CONFIG_DATA			0x8050
#define GTP_REG_CONFIG_RESOLUTION	0x8051
#define GTP_REG_CONFIG_TRIGGER		0x8056
#define GTP_REG_CONFIG_CHECKSUM		0x813C
#define GTP_REG_CONFIG_UPDATE		0x813E
#define GTP_REG_EXT_CFG_FLAG		0x805A
#define GTP_REG_EXT_CONFIG			0xBF7B
#define GTP_REG_VERSION				0x8140
#define GTP_REG_HW_INFO				0x4220
#define GTP_REG_REFRESH_RATE		0x8056
#define GTP_REG_ESD_CHECK			0x8043
#define GTP_REG_FLASH_PASSBY		0x8006
#define GTP_REG_HN_PAIRED			0x81AA
#define GTP_REG_HN_MODE				0x81A8
#define GTP_REG_MODULE_SWITCH3		0x8058
#define GTP_REG_FW_CHK_MAINSYS		0x41E4
#define GTP_REG_FW_CHK_SUBSYS		0x5095

#define set_reg_bit(reg, pos, val)	((reg) = ((reg) & (~(1<<(pos))))|(!!(val)<<(pos)))

// cmd define */
#define GTP_CMD_SLEEP				0x05
#define GTP_CMD_CHARGER_ON			0x06
#define GTP_CMD_CHARGER_OFF			0x07
#define GTP_CMD_GESTURE_WAKEUP		0x08
#define GTP_CMD_CLEAR_CFG			0x10
#define GTP_CMD_ESD					0xAA
#define GTP_CMD_HN_TRANSFER			0x22
#define GTP_CMD_HN_EXIT_SLAVE		0x28

// define offset in the config*/
#define RESOLUTION_LOC				(GTP_REG_CONFIG_RESOLUTION - GTP_REG_CONFIG_DATA)
#define TRIGGER_LOC					(GTP_REG_CONFIG_TRIGGER - GTP_REG_CONFIG_DATA)
#define MODULE_SWITCH3_LOC			(GTP_REG_MODULE_SWITCH3 - GTP_REG_CONFIG_DATA)

#ifdef CONFIG_GTP_WARP_X_ON
#define GTP_WARP_X(x_max, x) (x_max - 1 - x)
#else
#define GTP_WARP_X(x_max, x) x
#endif

#ifdef CONFIG_GTP_WARP_Y_ON
#define GTP_WARP_Y(y_max, y) (y_max - 1 - y)
#else
#define GTP_WARP_Y(y_max, y) y
#endif

#define IS_NUM_OR_CHAR(x)    (((x) >= 'A' && (x) <= 'Z') || ((x) >= '0' && (x) <= '9'))

//Log define
#define GTP_INFO(fmt, arg...)			printk("<<GTP-INF>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define GTP_ERROR(fmt, arg...)			printk("<<GTP-ERR>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define GTP_DEBUG(fmt, arg...)			do {\
										if (GTP_DEBUG_ON)\
										printk("<<GTP-DBG>>[%s:%d]"fmt"\n", __func__, __LINE__, ##arg);\
										} while (0)
#define GTP_DEBUG_ARRAY(array, num)	do {\
										s32 i;\
										u8 *a = array;\
										if (GTP_DEBUG_ARRAY_ON) {\
											printk("<<GTP-DBG>>");\
											for (i = 0; i < (num); i++) {\
												printk("%02x ", (a)[i]);\
												if ((i + 1) % 10 == 0) {\
													printk("\n<<GTP-DBG>>");\
												} \
											} \
											printk("\n");\
										} \
										} while (0)
#define GTP_DEBUG_FUNC()				do {\
										if (GTP_DEBUG_FUNC_ON)\
										printk("<<GTP-FUNC>> Func:%s@Line:%d\n", __func__, __LINE__);\
										} while (0)

#define GTP_SWAP(x, y)					do {\
										typeof(x) z = x;\
										x = y;\
										y = z;\
										} while (0)

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
} gt1x_chip_type_t;

#define _ERROR(e)		((0x01 << e) | (0x01 << (sizeof(s32) * 8 - 1)))
#define ERROR			_ERROR(1)	//for common use
//system relevant
#define ERROR_IIC		_ERROR(2)	//IIC communication error.
#define ERROR_MEM		_ERROR(3)	//memory error.

//system irrelevant
#define ERROR_HN_VER	_ERROR(10)	//HotKnot version error.
#define ERROR_CHECK		_ERROR(11)	//Compare src and dst error.
#define ERROR_RETRY		_ERROR(12)	//Too many retries.
#define ERROR_PATH		_ERROR(13)	//Mount path error
#define ERROR_FW		_ERROR(14)
#define ERROR_FILE		_ERROR(15)
#define ERROR_VALUE		_ERROR(16)	//Illegal value of variables

#define GTP_RETRY_3		3
#define GTP_RETRY_5		5

// bit operation */
#define SET_BIT(data, flag)	((data) |= (flag))
#define CLR_BIT(data, flag)	((data) &= ~(flag))
#define CHK_BIT(data, flag)	((data) & (flag))

// touch states */
#define BIT_TOUCH			0x01
#define BIT_TOUCH_KEY		0x02
#define BIT_STYLUS			0x04
#define BIT_STYLUS_KEY		0x08
#define BIT_HOVER			0x10

#include <linux/input.h>
struct i2c_msg;

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


//			Export global variables and functions		*/

// Export from gt1x_extents.c and gt1x_firmware.h */
#ifdef CONFIG_GTP_HOTKNOT
extern u8 hotknot_enabled;
extern u8 hotknot_transfer_mode;
extern u8 gt1x_patch_jump_fw[];
extern u8 hotknot_auth_fw[];
extern u8 hotknot_transfer_fw[];
extern void hotknot_wakeup_block(void);
#ifdef CONFIG_HOTKNOT_BLOCK_RW
extern s32 hotknot_paired_flag;
extern s32 hotknot_event_handler(u8 *data);
#endif
#endif //GTP_HOTKNOT
#define CONFIG_GTP_ESD_PROTECT 1

extern s32 gt1x_init_node(void);
extern void gt1x_deinit_node(void);

#ifdef CONFIG_GTP_GESTURE_WAKEUP
extern DOZE_T gesture_doze_status;
extern int gesture_enabled;
extern void gt1x_gesture_debug(int on) ;
extern s32 gesture_event_handler(struct input_dev *dev);
extern s32 gesture_enter_doze(void);
extern void gesture_clear_wakeup_data(void);
#endif

// Export from gt1x_tpd.c */
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
	struct fw_info *firmware_info;
	u32 fw_length;
	const struct firmware *fw;

	// file update
	char *fw_name;
	u8 *buffer;
	mm_segment_t old_fs;
	struct file *fw_file;

	// header update
	u8 *fw_data;
};

// Export form gt1x_update.c */
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

// Export from gt1x_tool.c */
#ifdef CONFIG_GTP_CREATE_WR_NODE
extern int gt1x_init_tool_node(void);
extern void gt1x_deinit_tool_node(void);
#endif

// Export from gt1x_generic.c */
extern struct i2c_client *gt1x_i2c_client;

extern gt1x_chip_type_t gt1x_chip_type;
extern struct gt1x_version_info gt1x_version;

extern s32 _do_i2c_read(struct i2c_msg *msgs, u16 addr, u8 *buffer, s32 len);
extern s32 _do_i2c_write(struct i2c_msg *msg, u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_write(u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_read(u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_read_dbl_check(u16 addr, u8 *buffer, s32 len);

extern u8 gt1x_config[];
extern u32 gt1x_cfg_length;
extern u8 gt1x_int_type;
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
extern int gt1x_suspend(void);
extern int gt1x_resume(void);
extern s32 gtp_test_sysfs_init(void);

#ifdef CONFIG_GTP_HAVE_TOUCH_KEY
extern const u16 gt1x_touch_key_array[];
#endif

#ifdef CONFIG_GTP_WITH_STYLUS
extern struct input_dev *pen_dev;
extern void gt1x_pen_up(s32 id);
extern void gt1x_pen_down(s32 x, s32 y, s32 size, s32 id);
#endif

#ifdef CONFIG_GTP_PROXIMITY
extern u8 gt1x_proximity_flag;
extern int gt1x_prox_event_handler(u8 *data);
#endif

#ifdef CONFIG_GTP_SMART_COVER
extern int gt1x_parse_sc_cfg(int sensor_id);
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
extern void gt1x_init_esd_protect(void);
extern void gt1x_esd_switch(s32 on);
#endif

#ifdef CONFIG_GTP_CHARGER_SWITCH
extern u32 gt1x_get_charger_status(void);
extern void gt1x_charger_switch(s32 on);
extern void gt1x_charger_config(s32 dir_update);
extern int gt1x_parse_chr_cfg(int sensor_id);
#endif

#define IIC_MAX_TRANSFER_SIZE       250

#ifdef CONFIG_MTK_PLATFORM
// MTK platform */
#include <asm/uaccess.h>
#ifdef CONFIG_MTK_BOOT
#include "mt_boot_common.h"
#endif
#include <linux/rtpm_prio.h>
#include <mtk_thermal_typedefs.h>
#ifndef MT6589
#include <mt_gpio.h>
#endif
#include "tpd.h"
#include "upmu_common.h"

#define GTP_GPIO_AS_INT(pin) tpd_gpio_as_int(pin)
#define GTP_GPIO_OUTPUT(pin, level) tpd_gpio_output(pin, level)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
#define GTP_MTK_LEGACY
#endif

#define PLATFORM_MTK
#define GTP_I2C_ADDRESS	0x5D
#define TPD_I2C_NUMBER				1

#ifdef CONFIG_MTK_I2C_EXTENSION
#define TPD_SUPPORT_I2C_DMA			1
#else
#define TPD_SUPPORT_I2C_DMA			0
#endif

#if defined(CONFIG_MTK_LEGACY)
#define TPD_POWER_SOURCE_CUSTOM	MT6328_POWER_LDO_VGP1
#endif

#ifdef MT6589
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
#define mt_eint_mask mt65xx_eint_mask
#define mt_eint_unmask mt65xx_eint_unmask
#endif

#define IIC_DMA_MAX_TRANSFER_SIZE		250
#define I2C_MASTER_CLOCK				300
#define TPD_HAVE_CALIBRATION
#define TPD_CALIBRATION_MATRIX			{962, 0, 0, 0, 1600, 0, 0, 0};

extern void tpd_on(void);
extern void tpd_off(void);

#else
// Generic Platform(Qcom or othter) */
#ifdef CONFIG_OF
extern int gt1x_rst_gpio;
extern int gt1x_int_gpio;
#define GTP_RST_PORT gt1x_rst_gpio
#define GTP_INT_PORT gt1x_int_gpio
#else
#define GTP_RST_PORT	102
#define GTP_INT_PORT	52
#endif

#define GTP_GPIO_AS_INPUT(pin)		gpio_direction_input(pin)
#define GTP_GPIO_AS_INT(pin)		GTP_GPIO_AS_INPUT(pin)
#define GTP_GPIO_OUTPUT(pin, level)	gpio_direction_output(pin, level)
#define GTP_IRQ_TAB					{IRQ_TYPE_EDGE_RISING, IRQ_TYPE_EDGE_FALLING,\
		IRQ_TYPE_LEVEL_LOW, IRQ_TYPE_LEVEL_HIGH}

#endif // CONFIG_MTK_PLATFORM */

int gt1x_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value);


#endif // _GT1X_GENERIC_H_


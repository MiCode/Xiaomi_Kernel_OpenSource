/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef GT1X_TPD_COMMON_H__
#define GT1X_TPD_COMMON_H__

#include <linux/uaccess.h>
#ifdef CONFIG_MTK_BOOT
#include "mtk_boot_common.h"
#endif
#include "tpd.h"
#include "upmu_common.h"
#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <uapi/linux/sched/types.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#ifdef CONFIG_MTK_I2C_EXTENSION
/* if gt9l, better enable it if hardware platform supported*/
#define TPD_SUPPORT_I2C_DMA         1
#else
#define TPD_SUPPORT_I2C_DMA         0
#endif

#if defined(CONFIG_MTK_LEGACY)
#define TPD_POWER_SOURCE_CUSTOM	MT6328_POWER_LDO_VGP1
#endif

#define GTP_GPIO_AS_INT(pin) tpd_gpio_as_int(pin)
#define GTP_GPIO_OUTPUT(pin, level) tpd_gpio_output(pin, level)

#define IIC_MAX_TRANSFER_SIZE         8
#define IIC_DMA_MAX_TRANSFER_SIZE     250
#define I2C_MASTER_CLOCK              300
#define TPD_MAX_RESET_COUNT           3
#define TPD_HAVE_CALIBRATION
#define TPD_CALIBRATION_MATRIX        {962, 0, 0, 0, 1600, 0, 0, 0}
#define KEY_GESTURE           KEY_F24	/* customize gesture-key */
#define DEFAULT_MAX_TOUCH_NUM         10

extern int tpd_em_log;

#define CFG_GROUP_LEN(p_cfg_grp)  (ARRAY_SIZE(p_cfg_grp) / sizeof(p_cfg_grp[0]))

#ifdef CONFIG_GTP_CUSTOM_CFG
#define GTP_INT_TRIGGER  1	/*0:Rising 1:Falling*/
#define GTP_WAKEUP_LEVEL 1
#endif

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
#ifdef CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM
#define GTP_WARP_X_ON         1
#define GTP_WARP_Y_ON         1
#else   /* CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM */
#define GTP_WARP_X_ON         0
#define GTP_WARP_Y_ON         0
#endif  /* CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM */
#else   /* CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW */
#ifdef CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM
#define GTP_WARP_X_ON         0
#define GTP_WARP_Y_ON         0
#else   /* CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM */
#define GTP_WARP_X_ON         1
#define GTP_WARP_Y_ON         1
#endif  /* CONFIG_TOUCHSCREEN_PHYSICAL_ROTATION_WITH_LCM */
#endif  /* CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW */

#ifdef CONFIG_GTP_WITH_STYLUS
#define GTP_STYLUS_KEY_TAB {BTN_STYLUS, BTN_STYLUS2}
#endif


/****************************PART3:OTHER define*******************************/
#define GTP_DRIVER_VERSION          "V1.0<2014/09/28>"
#define GTP_I2C_NAME                "Goodix-TS"
#define GT1X_DEBUG_PROC_FILE        "gt1x_debug"
#define GTP_POLL_TIME               10
#define GTP_ADDR_LENGTH             2
#define GTP_CONFIG_MIN_LENGTH       186
#define GTP_CONFIG_MAX_LENGTH       240
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

#define GTP_REG_WAKEUP_GESTURE         0x814C
#define GTP_REG_WAKEUP_GESTURE_DETAIL  0xA2A0	/*need change */

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
#define GTP_REG_VERSION             0x8140
#define GTP_REG_HW_INFO             0x4220
#define GTP_REG_REFRESH_RATE	    0x8056
#define GTP_REG_ESD_CHECK           0x8043
#define GTP_REG_FLASH_PASSBY        0x8006
#define GTP_REG_HN_PAIRED           0x81AA
#define GTP_REG_HN_MODE             0x81A8
#define GTP_REG_MODULE_SWITCH3      0x8058

#define set_reg_bit(reg, index, val)	((reg) ^= (!(val) << (index)))

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
#define RESOLUTION_LOC              \
	(GTP_REG_CONFIG_RESOLUTION - GTP_REG_CONFIG_DATA)
#define TRIGGER_LOC                 \
	(GTP_REG_CONFIG_TRIGGER - GTP_REG_CONFIG_DATA)
#define MODULE_SWITCH3_LOC	    \
	(GTP_REG_MODULE_SWITCH3 - GTP_REG_CONFIG_DATA)

#define GTP_I2C_ADDRESS				0xBA

#if GTP_WARP_X_ON
#define GTP_WARP_X(x_max, x) (x_max - 1 - x)
#else
#define GTP_WARP_X(x_max, x) x
#endif

#if GTP_WARP_Y_ON
#define GTP_WARP_Y(y_max, y) (y_max - 1 - y)
#else
#define GTP_WARP_Y(y_max, y) y
#endif

#define IS_NUM_OR_CHAR(x)    \
	(((x) > 'A' && (x) < 'Z') || ((x) > '0' && (x) < '9'))

/*Log define*/
#define GTP_INFO(fmt, arg...)           \
	pr_info("<<GTP-INF>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define GTP_ERROR(fmt, arg...)          \
	pr_info("<<GTP-ERR>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define GTP_DEBUG(fmt, arg...)				\
	do {								\
		if (tpd_em_log)						\
			pr_debug("<<GTP-DBG>>[%s:%d]"fmt"\n", \
			__func__, __LINE__, ##arg);\
	} while (0)
#ifdef CONFIG_GTP_DEBUG_ARRAY_ON
#define GTP_DEBUG_ARRAY(array, num)			\
	do {								\
		s32 i;							\
		u8 *a = array;						\
		pr_debug("<<GTP-DBG>>");		\
		for (i = 0; i < (num); i++) {	\
			pr_debug("%02x ", (a)[i]);	\
			if ((i + 1) % 10 == 0) {	\
				pr_debug("\n<<GTP-DBG>>");\
			}						\
		}							\
		pr_debug("\n");						\
	} while (0)
#else
#define GTP_DEBUG_ARRAY(array, num)	do {} while (0)
#endif
#ifdef CONFIG_GTP_DEBUG_FUNC_ON
#define GTP_DEBUG_FUNC()	\
	pr_debug("<<GTP-FUNC>> Func:%s@Line:%d\n", __func__, __LINE__)
#else
#define GTP_DEBUG_FUNC()	do {} while (0)
#endif
#define GTP_SWAP(x, y)		\
	do {					\
		typeof(x) z = x;	\
		x = y;				\
		y = z;				\
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

enum DOZE_T {
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
};

enum CHIP_TYPE_T {
	CHIP_TYPE_GT1X = 0,
	CHIP_TYPE_GT2X = 1,
	CHIP_TYPE_NONE = 0xFF
};

#define _ERROR(e)      ((0x01 << e) | (0x01 << (sizeof(s32) * 8 - 1)))
#define ERROR          _ERROR(1)	/*for common use */
/*system relevant*/
#define ERROR_IIC      _ERROR(2)	/*IIC communication error. */
#define ERROR_MEM      _ERROR(3)	/*memory error. */

/*system irrelevant*/
#define ERROR_HN_VER   _ERROR(10)	/*HotKnot version error. */
#define ERROR_CHECK    _ERROR(11)	/*Compare src and dst error. */
#define ERROR_RETRY    _ERROR(12)	/*Too many retries. */
#define ERROR_PATH     _ERROR(13)	/*Mount path error */
#define ERROR_FW       _ERROR(14)
#define ERROR_FILE     _ERROR(15)
#define ERROR_VALUE    _ERROR(16)	/*Illegal value of variables */

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

struct i2c_msg;
extern void tpd_on(void);
extern void tpd_off(void);
/*          Export global variables and functions          */

/* Export from gt1x_extents.c and gt1x_firmware.h */

#ifdef CONFIG_GTP_HOTKNOT
extern u8 hotknot_enabled;
extern u8 hotknot_transfer_mode;
extern u8 gt1x_patch_jump_fw[];
extern u8 hotknot_auth_fw[];
extern u8 hotknot_transfer_fw[];
#ifdef CONFIG_HOTKNOT_BLOCK_RW
extern s32 hotknot_paired_flag;
extern s32 hotknot_event_handler(u8 *data);
#endif
#endif				/*CONFIG_GTP_HOTKNOT */
extern s32 gt1x_init_node(void);
extern bool check_flag;
#ifdef CONFIG_GTP_GESTURE_WAKEUP
extern enum DOZE_T gesture_doze_status;
extern int gesture_enabled;
extern s32 gesture_event_handler(struct input_dev *dev);
extern s32 gesture_enter_doze(void);
extern void gesture_clear_wakeup_data(void);
#endif

/* Export from gt1x_tpd.c */
extern void gt1x_touch_down(s32 x, s32 y, s32 size, s32 id);
extern void gt1x_touch_up(s32 id);
extern void gt1x_power_switch(s32 state);
extern void gt1x_irq_enable(void);
extern void gt1x_irq_disable(void);
extern int gt1x_debug_proc(u8 *buf, int count);

struct fw_update_info {
	int update_type;
	int status;
	int progress;
	int max_progress;
	struct fw_info *firmware;
	u32 fw_length;

	/* file update */
	char *fw_name;
	u8 *buffer;
	mm_segment_t old_fs;
	struct file *fw_file;

	/* header update */
	u8 *fw_data;
};

/* Export form gt1x_update.c */
extern struct fw_update_info update_info;

extern u8 gt1x_default_FW[];
extern int gt1x_hold_ss51_dsp(void);
extern int gt1x_auto_update_proc(void *data);
extern int gt1x_update_firmware(char *filename);
extern void gt1x_enter_update_mode(void);
extern void gt1x_leave_update_mode(void);
extern int gt1x_hold_ss51_dsp_no_reset(void);
extern int gt1x_load_patch(
	u8 *patch, u32 patch_size, int offset, int bank_size);
extern int gt1x_startup_patch(void);
extern void gt1x_auto_update_done(void);
extern int gt1x_is_tpd_halt(void);

/* Export from gt1x_tool.c */
#ifdef CONFIG_GTP_CREATE_WR_NODE
extern int gt1x_init_tool_node(void);
extern void gt1x_deinit_tool_node(void);
#endif

/* Export from gt1x_generic.c */
extern struct i2c_client *gt1x_i2c_client;

extern enum CHIP_TYPE_T gt1x_chip_type;
extern struct gt1x_version_info gt1x_version;

extern s32 gt1x_init_debug_node(void);
extern void gt1x_deinit_debug_node(void);

extern s32 _do_i2c_read(struct i2c_msg *msgs, u16 addr, u8 *buffer, s32 len);
extern s32 _do_i2c_write(struct i2c_msg *msg, u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_write(u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_read(u16 addr, u8 *buffer, s32 len);
extern s32 gt1x_i2c_test(void);
extern s32 gt1x_i2c_read_dbl_check(u16 addr, u8 *buffer, s32 len);

extern u8 gt1x_config[];
extern u32 gt1x_cfg_length;
extern u8 gt1x_int_type;
extern u8 gt1x_wakeup_level;
extern u32 gt1x_abs_x_max;
extern u32 gt1x_abs_y_max;
extern u8 gt1x_rawdiff_mode;
extern u8 gt1x_driver_num;
extern u8 gt1x_sensor_num;
extern u8 gt1x_init_failed;

extern s32 gt1x_init(void);
extern void gt1x_deinit(void);
extern s32 gt1x_read_version(struct gt1x_version_info *ver_info);
extern s32 gt1x_enter_sleep(void);
extern s32 gt1x_wakeup_sleep(void);
extern s32 gt1x_init_panel(void);
extern s32 gt1x_get_chip_type(void);
extern s32 gt1x_request_event_handler(void);
extern int gt1x_send_cmd(u8 cmd, u8 data);
extern s32 gt1x_send_cfg(u8 *config, int cfg_len);
extern void gt1x_select_addr(void);
extern s32 gt1x_reset_guitar(void);
extern void gt1x_power_reset(void);
extern void gt1x_power_reset2(void);
extern int gt1x_parse_config(char *filename, u8 *gt1x_config);
extern s32 gt1x_touch_event_handler(
	u8 *data, struct input_dev *dev, struct input_dev *pen_dev);


#ifdef CONFIG_GTP_WITH_STYLUS
extern struct input_dev *pen_dev;
extern void gt1x_pen_down(s32 x, s32 y, s32 size, s32 id);
extern void gt1x_pen_up(s32 id);
#endif

#ifdef CONFIG_GTP_PROXIMITY
extern u8 gt1x_proximity_flag;
extern u8 gt1x_proximity_detect;
extern void gt1x_report_ps(u8 state);
extern void gt1x_ps_init(void);
extern int gt1x_prox_event_handler(u8 *data);
#endif

#ifdef CONFIG_GTP_ESD_PROTECT
extern void gt1x_init_esd_protect(void);
extern void gt1x_deinit_esd_protect(void);
extern s32 gt1x_init_ext_watchdog(void);
extern void gt1x_esd_switch(s32 on);
#endif

#ifdef CONFIG_GTP_CHARGER_SWITCH
extern u8 gt1x_config_charger[GTP_CONFIG_MAX_LENGTH];
extern u32 gt1x_get_charger_status(void);
extern void gt1x_charger_switch(s32 on);
extern void gt1x_charger_config(s32 dir_update);
#ifdef MT6573
#define CHR_CON0      (0xF7000000+0x2FA00)
#else
extern bool upmu_is_chr_det(void);
#endif
#endif
extern struct tpd_filter_t tpd_filter;
extern wait_queue_head_t init_waiter;
extern u8 is_resetting;

/* AF power is connected to Touch power */
#ifdef CONFIG_MTK_LENS
extern void AF_PowerDown(void);
#endif

#endif /* GT1X_TPD_COMMON_H__ */

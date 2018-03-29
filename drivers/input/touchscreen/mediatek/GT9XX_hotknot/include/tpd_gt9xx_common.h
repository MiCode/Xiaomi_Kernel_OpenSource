/* drivers/input/touchscreen/tpd_gt9xx_common.h
 *
 * 2010 - 2012 Goodix Technology.
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
 */

#ifndef TPD_CUSTOM_GT9XX_H__
#define TPD_CUSTOM_GT9XX_H__

#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
/* #include <linux/io.h> */

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
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/interrupt.h>
#include <linux/time.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>

/* Pre-defined definition */

#define TPD_KEY_COUNT   4

#define TPD_KEYS        {KEY_BACK, KEY_HOME, KEY_MENU, KEY_SEARCH}
#define TPD_KEYS_DIM    {{60, 850, 50, 30}, {180, 850, 50, 30}, {300, 850, 50, 30}, {420, 850, 50, 30} }
#define GTP_KEY_MAP_ARRAY         {{60, 850}, {180, 850}, {300, 850},}

#define TOUCH_FILTER 1
#if TOUCH_FILTER
#define TPD_FILTER_PARA {1, 195} /* {enable, pixel density} */
#endif
/*
struct goodix_ts_data
{
    spinlock_t irq_lock;
    struct i2c_client *client;
    struct input_dev  *input_dev;
    struct hrtimer timer;
    struct work_struct  work;
    struct early_suspend early_suspend;
    s32 irq_is_disable;
    s32 use_irq;
    u16 abs_x_max;
    u16 abs_y_max;
    u8  max_touch_num;
    u8  int_trigger_type;
    u8  green_wake_mode;
    u8  chip_type;
    u8  enter_update;
    u8  gtp_is_suspend;
    u8  gtp_rawdiff_mode;
};
*/

extern u16 show_len;
extern u16 total_len;
extern u8 gtp_rawdiff_mode;
extern u8 load_fw_process;

extern int tpd_halt;
extern s32 gtp_send_cfg(struct i2c_client *client);
extern void gtp_reset_guitar(struct i2c_client *client, s32 ms);
extern void gtp_int_sync(s32 ms);
extern u8 gup_init_update_proc(struct i2c_client *client);
extern u8 gup_init_fw_proc(struct i2c_client *client);
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len);
extern s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len);
extern int i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len);
extern int i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len);
extern s32 gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr, u8 *rxbuf, int len);
extern u8 wait_hotknot_state;
extern u8 got_hotknot_state;
extern u8 got_hotknot_extra_state;
extern u8 hotknot_paired_flag;
extern wait_queue_head_t bp_waiter;
extern s32 gup_load_hotknot_system(void);

extern unsigned char gtp_default_FW[];
extern unsigned char gtp_default_FW_fl[];
/* ***************************PART1:ON/OFF define******************************* */
/*
#define GTP_CUSTOM_CFG        1
#define GTP_DRIVER_SEND_CFG   1       //driver send config to TP on intilization (for no config built in TP flash)
#define GTP_HAVE_TOUCH_KEY    0
#define GTP_POWER_CTRL_SLEEP  1       //turn off power on suspend
#define GTP_AUTO_UPDATE       1      //update FW to TP FLASH
#define GTP_CHANGE_X2Y        0				//set for se1
// auto updated fw by gtp_default_FW in gt9xx_firmware.h, function together with GTP_AUTO_UPDATE
#define GTP_HEADER_FW_UPDATE  1
#define GTP_AUTO_UPDATE_CFG   1       // auto update config by .cfg file, function together with GTP_AUTO_UPDATE

#define GTP_SUPPORT_I2C_DMA   1       // if gt9xxf, better enable it if hardware platform supported
#define GTP_COMPATIBLE_MODE   1       // compatible with GT9XXF
#define GTP_CREATE_WR_NODE    1
#define GTP_ESD_PROTECT       0       // esd protection with a cycle of 2 seconds
#define GUP_USE_HEADER_FILE   0
#define GTP_FW_DOWNLOAD       0       //update FW to TP SRAM
//#define GTP_CHARGER_DETECT

#define GTP_CONFIG_MIN_LENGTH       186
#define GTP_CONFIG_MAX_LENGTH       240
#define GTP_CHARGER_SWITCH    1       // charger plugin & plugout detect
#define GTP_WITH_PEN          0
#define GTP_SLIDE_WAKEUP      0
#define GTP_DBL_CLK_WAKEUP    0       // double-click wakup, function together with GTP_SLIDE_WAKEUP
#define HOTKNOT_BLOCK_RW      0
//#define TPD_PROXIMITY
//#define TPD_HAVE_BUTTON               //report key as coordinate,Vibration feedback
//#define TPD_WARP_X
//#define TPD_WARP_Y

#define GTP_DEBUG_ON          0
#define GTP_DEBUG_ARRAY_ON    0
#define GTP_DEBUG_FUNC_ON     0
*/
#define CFG_GROUP_LEN(p_cfg_grp)  (sizeof(p_cfg_grp) / sizeof(p_cfg_grp[0]))
#define FLASHLESS_FLASH_WORKROUND  0

/* STEP_2(REQUIRED):Change I/O define & I/O operation mode. */
/*
#define GTP_RST_PORT    GPIO_CTP_RST_PIN
#define GTP_INT_PORT    GPIO_CTP_EINT_PIN
*/
/*
#define GTP_GPIO_AS_INPUT(pin)          do{\
					    if(pin == GPIO_CTP_EINT_PIN)\
						mt_set_gpio_mode(pin, GPIO_CTP_EINT_PIN_M_GPIO);\
					    else\
						mt_set_gpio_mode(pin, GPIO_CTP_RST_PIN_M_GPIO);\
					    mt_set_gpio_dir(pin, GPIO_DIR_IN);\
					    mt_set_gpio_pull_enable(pin, GPIO_PULL_DISABLE);\
					}while(0)
#define GTP_GPIO_AS_INT(pin)            do{\
					    mt_set_gpio_mode(pin, GPIO_CTP_EINT_PIN_M_EINT);\
					    mt_set_gpio_dir(pin, GPIO_DIR_IN);\
					    mt_set_gpio_pull_enable(pin, GPIO_PULL_DISABLE);\
					}while(0)
#define GTP_GPIO_GET_VALUE(pin)         mt_get_gpio_in(pin)
#define GTP_GPIO_OUTPUT(pin,level)      do{\
					    if(pin == GPIO_CTP_EINT_PIN)\
						mt_set_gpio_mode(pin, GPIO_CTP_EINT_PIN_M_GPIO);\
					    else\
						mt_set_gpio_mode(pin, GPIO_CTP_RST_PIN_M_GPIO);\
					    mt_set_gpio_dir(pin, GPIO_DIR_OUT);\
					    mt_set_gpio_out(pin, level);\
					}while(0)
#define GTP_GPIO_REQUEST(pin, label)    gpio_request(pin, label)
#define GTP_GPIO_FREE(pin)              gpio_free(pin)
#define GTP_IRQ_TAB	{IRQ_TYPE_EDGE_RISING, \
					IRQ_TYPE_EDGE_FALLING, \
					IRQ_TYPE_LEVEL_LOW, \
					IRQ_TYPE_LEVEL_HIGH}
*/
/* STEP_3(optional):Custom set some config by themself,if need. */
#if defined(CONFIG_GTP_CUSTOM_CFG)
#define GTP_INT_TRIGGER  0	/* 0:Rising 1:Falling */
#else
#define GTP_INT_TRIGGER  1
#endif
#define GTP_MAX_TOUCH      5

#define TPD_POWER_SOURCE_CUSTOM	MT6323_POWER_LDO_VGP1	/* MT6323_POWER_LDO_VGP1 */
#define VELOCITY_CUSTOM
#define TPD_VELOCITY_CUSTOM_X 15
#define TPD_VELOCITY_CUSTOM_Y 15

/* STEP_4(optional):If this project have touch key,Set touch key config. */
#define GTP_KEY_TAB	 {KEY_MENU, KEY_HOME, KEY_BACK, KEY_SEND}

/* ***************************PART3:OTHER define********************************* */
#define GTP_DRIVER_VERSION          "V2.1<2014/01/10>"
#define GTP_I2C_NAME                "Goodix-TS"
#define GT91XX_CONFIG_PROC_FILE     "gt9xx_config"
#define GTP_POLL_TIME               10
#define GTP_ADDR_LENGTH             2
#define GTP_CONFIG_MIN_LENGTH       186
#define GTP_CONFIG_MAX_LENGTH       240
#define FAIL                        0
#define SUCCESS                     1
#define SWITCH_OFF                  0
#define SWITCH_ON                   1

/* ******************** For GT9XXF Start **********************/
#if defined(CONFIG_GTP_COMPATIBLE_MODE)
enum CHIP_TYPE_T {
	CHIP_TYPE_GT9 = 0,
	CHIP_TYPE_GT9F = 1,
};
#endif

#define GTP_REG_MATRIX_DRVNUM           0x8069
#define GTP_REG_MATRIX_SENNUM           0x806A
#define GTP_REG_RQST                    0x8043
#define GTP_REG_BAK_REF                 0x99D0
#define GTP_REG_MAIN_CLK                0x8020
#define GTP_REG_CHIP_TYPE               0x8000
#define GTP_REG_HAVE_KEY                0x804E
#define GTP_REG_HN_STATE                0xAB10

#define GTP_FL_FW_BURN                  0x00
#define GTP_FL_ESD_RECOVERY             0x01
#define GTP_FL_READ_REPAIR              0x02

#define GTP_BAK_REF_SEND                0
#define GTP_BAK_REF_STORE               1
#define CFG_LOC_DRVA_NUM                29
#define CFG_LOC_DRVB_NUM                30
#define CFG_LOC_SENS_NUM                31

#define GTP_CHK_FW_MAX                  1000
#define GTP_CHK_FS_MNT_MAX              300
#define GTP_BAK_REF_PATH                "/data/gtp_ref.bin"
#define GTP_MAIN_CLK_PATH               "/data/gtp_clk.bin"
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


/* ******************** For GT9XXF End **********************/

/* Register define */
#define GTP_READ_COOR_ADDR          0x814E
#define GTP_REG_SLEEP               0x8040
#define GTP_REG_SENSOR_ID           0x814A
#define GTP_REG_CONFIG_DATA         0x8047
#define GTP_REG_VERSION             0x8140
#define GTP_REG_HW_INFO             0x4220
#define GTP_REG_REFRESH_RATE		0x8056

#define RESOLUTION_LOC              3
#define TRIGGER_LOC                 8


#define GTP_DMA_MAX_TRANSACTION_LENGTH  31	/* for DMA mode */
#define GTP_DMA_MAX_I2C_TRANSFER_SIZE   (GTP_DMA_MAX_TRANSACTION_LENGTH - GTP_ADDR_LENGTH)
#define MAX_TRANSACTION_LENGTH        8
#define TPD_I2C_NUMBER				  1
#define I2C_MASTER_CLOCK              300
#define MAX_I2C_TRANSFER_SIZE         (MAX_TRANSACTION_LENGTH - GTP_ADDR_LENGTH)
#define TPD_MAX_RESET_COUNT           3
#define TPD_CALIBRATION_MATRIX        {962, 0, 0, 0, 1600, 0, 0, 0}
#define TPD_CALIBRATION_MATRIX_ROTATION_NORMAL {-4096, 0, 3276800, 0, -4096, 5242880, 0, 0}
#define TPD_CALIBRATION_MATRIX_ROTATION_FACTORY {-4096, 0, 3276800, 0, -4096, 5242880, 0, 0}


#define TPD_RESET_ISSUE_WORKAROUND
#define TPD_HAVE_CALIBRATION
#define TPD_NO_GPIO
#define TPD_RESET_ISSUE_WORKAROUND

#ifdef TPD_WARP_X
#undef TPD_WARP_X
#define TPD_WARP_X(x_max, x) (x_max - 1 - x)
#else
#define TPD_WARP_X(x_max, x) x
#endif

#ifdef TPD_WARP_Y
#undef TPD_WARP_Y
#define TPD_WARP_Y(y_max, y) (y_max - 1 - y)
#else
#define TPD_WARP_Y(y_max, y) y
#endif

/* Log define */
#if 0
#define GTP_INFO(fmt, arg...)           pr_err("<<-GTP-INFO->> "fmt"\n", ##arg)
#else
#define GTP_INFO(fmt, arg...)
#endif

#define GTP_ERROR(fmt, arg...)          pr_err("<<-GTP-ERROR->> "fmt"\n", ##arg)
#if 0
#define GTP_DEBUG(fmt, arg...) \
	pr_err("<<-GTP-DEBUG->> [%d]"fmt"\n", __LINE__, ##arg)
#else
#define GTP_DEBUG(fmt, arg...)					pr_debug("<<-GTP-DEBUG->> [%d]"fmt"\n", __LINE__, ##arg)
#endif

#if defined(CONFIG_GTP_DEBUG_ARRAY_ON)
#define GTP_DEBUG_ARRAY(array, num) \
	do {\
		s32 i;\
		u8 *a = array;\
		GTP_DEBUG("<<-GTP-DEBUG-ARRAY->>\n");\
		for (i = 0; i < (num); i++) {\
			GTP_DEBUG("%02x   ", (a)[i]);\
			if ((i + 1) % 10 == 0) \
				GTP_DEBUG("\n");\
		} \
		GTP_DEBUG("\n");\
	} while (0)
#else
#define GTP_DEBUG_ARRAY(array, num)
#endif

#if defined(CONFIG_GTP_DEBUG_FUNC_ON)
#define GTP_DEBUG_FUNC() GTP_DEBUG("<<-GTP-FUNC->> Func:%s@Line:%d\n", __func__, __LINE__)
#else
#define GTP_DEBUG_FUNC()
#endif
#define GTP_SWAP(x, y)                 do {\
					 typeof(x) z = x;\
					 x = y;\
					 y = z;\
				       } while (0)

/* ****************************PART4:UPDATE define******************************* */
/* Error no */
#define ERROR_NO_FILE           2	/* ENOENT */
#define ERROR_FILE_READ         23	/* ENFILE */
#define ERROR_FILE_TYPE         21	/* EISDIR */
#define ERROR_GPIO_REQUEST      4	/* EINTR */
#define ERROR_I2C_TRANSFER      5	/* EIO */
#define ERROR_NO_RESPONSE       16	/* EBUSY */
#define ERROR_TIMEOUT           110	/* ETIMEDOUT */

/* *****************************End of Part III******************************** */

extern struct tpd_device *tpd;
#ifdef VELOCITY_CUSTOM
extern int tpd_v_magnify_x;
extern int tpd_v_magnify_y;
#endif
#if TOUCH_FILTER
extern struct tpd_filter_t tpd_filter;
#endif
#if defined(CONFIG_GTP_CHARGER_SWITCH)
extern int g_bat_init_flag;
extern bool upmu_is_chr_det(void);
#endif

#if defined(CONFIG_GTP_CHARGER_DETECT)
extern bool upmu_get_pchr_chrdet(void);
#endif

#if defined(CONFIG_GTP_CREATE_WR_NODE)
extern s32 init_wr_node(struct i2c_client *);
extern void uninit_wr_node(void);
#endif

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
extern u8 gup_check_fs_mounted(char *path_name);
extern u8 gup_clk_calibration(void);
extern s32 gup_load_main_system(char *filepath);
extern s32 gup_fw_download_proc(void *dir, u8 dwn_mode);
#endif

extern s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len);
extern s32 i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len);
extern s32 gup_enter_update_mode(struct i2c_client *client);
extern void gup_leave_update_mode(void);
extern s32 gup_update_proc(void *dir);
extern s32 gup_load_hotknot_system(void);
extern s32 gup_load_fx_system(void);
extern s32 gup_recovery_main_system(void);
extern s32 gup_load_main_system(char *filepath);
void gtp_esd_switch(struct i2c_client *client, s32 on);
#if (defined(CONFIG_GTP_ESD_PROTECT) || defined(CONFIG_GTP_COMPATIBLE_MODE))
void force_reset_guitar(void);
#endif


#if (defined(CONFIG_GTP_ESD_PROTECT) || defined(CONFIG_GTP_COMPATIBLE_MODE))
extern u8 is_resetting;
#endif

extern unsigned int touch_irq;
extern unsigned int tpd_rst_gpio_number;
extern unsigned int tpd_int_gpio_number;
extern struct i2c_client *i2c_client_tp_point;
#if defined(CONFIG_GTP_ESD_PROTECT)
extern void gtp_esd_switch(struct i2c_client *client, s32 on);
#endif

#if defined(CONFIG_GTP_COMPATIBLE_MODE)
extern enum CHIP_TYPE_T gtp_chip_type;
extern u8 rqst_processing;
extern u8 gtp_fw_startup(struct i2c_client *client);
s32 gup_recovery_main_system(void);
s32 gup_load_main_system(char *filepath);
s32 gup_fw_download_proc(void *dir, u8 dwn_mode);
char *gup_load_fw_from_file(char *filepath);
s32 gup_load_system(char *firmware, s32 length, u8 need_check);
#endif


#if defined(CONFIG_GTP_ESD_PROTECT)
extern void gtp_esd_switch(struct i2c_client *client, s32 on);
#endif

extern u8 fw_updating;
extern u8 cfg_len;
extern struct mutex i2c_access;

#endif				/* TPD_CUSTOM_GT9XX_H__ */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __TPD_H
#define __TPD_H
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <generated/autoconf.h>
#include <linux/kobject.h>
#include <linux/regulator/consumer.h>

/*debug macros */
#define TPD_DEBUG
#define TPD_DEBUG_CODE
/* #define TPD_DEBUG_TRACK */
#define TPD_DMESG(a, arg...) \
	pr_info(TPD_DEVICE ":[%s:%d] " a, __func__, __LINE__, ##arg)
#if defined(TPD_DEBUG)
#undef TPD_DEBUG
#define TPD_DEBUG(a, arg...) \
	pr_info(TPD_DEVICE ":[%s:%d] " a, __func__, __LINE__, ##arg)
#else
#define TPD_DEBUG(arg...)
#endif
#define SPLIT ", "

/* register, address, configurations */
#define TPD_DEVICE            "mtk-tpd"
#define TPD_X                  0
#define TPD_Y                  1
#define TPD_Z1                 2
#define TPD_Z2                 3
#define TP_DELAY              (2*HZ/100)
#define TP_DRV_MAX_COUNT          (20)
#define TPD_WARP_CNT          (4)
#define TPD_VIRTUAL_KEY_MAX   (10)

/* various mode */
#define TPD_MODE_NORMAL        0
#define TPD_MODE_KEYPAD        1
#define TPD_MODE_SW 2
#define TPD_MODE_FAV_SW 3
#define TPD_MODE_FAV_HW 4
#define TPD_MODE_RAW_DATA 5
#undef TPD_RES_X
#undef TPD_RES_Y
extern unsigned long TPD_RES_X;
extern unsigned long TPD_RES_Y;
extern int tpd_load_status;	/* 0: failed, 1: success */
extern int tpd_mode;
extern int tpd_mode_axis;
extern int tpd_mode_min;
extern int tpd_mode_max;
extern int tpd_mode_keypad_tolerance;
extern int tpd_em_debounce_time;
extern int tpd_em_debounce_time0;
extern int tpd_em_debounce_time1;
extern int tpd_em_asamp;
extern int tpd_em_auto_time_interval;
extern int tpd_em_sample_cnt;
extern int tpd_calmat[];
extern int tpd_def_calmat[];
extern int tpd_calmat[];
extern int tpd_def_calmat[];
extern int TPD_DO_WARP;
extern int tpd_wb_start[];
extern int tpd_wb_end[];
extern int tpd_v_magnify_x;
extern int tpd_v_magnify_y;
extern unsigned int DISP_GetScreenHeight(void);
extern unsigned int DISP_GetScreenWidth(void);
#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_47) || \
	defined(CONFIG_MTK_S3320_50)
extern void synaptics_init_sysfs(void);
#endif /* CONFIG_MTK_S3320 */
extern void tpd_button_init(void);
struct tpd_device {
	struct device *tpd_dev;
	struct regulator *reg;
	struct regulator *io_reg;
	struct input_dev *dev;
	struct input_dev *kpd;
	struct timer_list timer;
	struct tasklet_struct tasklet;
	int btn_state;
};
struct tpd_key_dim_local {
	int key_x;
	int key_y;
	int key_width;
	int key_height;
};

struct tpd_filter_t {
	int enable; /*0: disable, 1: enable*/
	int pixel_density; /*XXX pixel/cm*/
	int W_W[3][4];/*filter custom setting prameters*/
	unsigned int VECLOCITY_THRESHOLD[3];/*filter speed custom settings*/
};

struct tpd_dts_info {
	int tpd_resolution[2];
	int touch_max_num;
	int use_tpd_button;
	int tpd_key_num;
	int tpd_key_local[4];
	bool tpd_use_ext_gpio;
	int rst_ext_gpio_num;
	int rst_gpio_num;
	int eint_gpio_num;
	struct tpd_key_dim_local tpd_key_dim_local[4];
	struct tpd_filter_t touch_filter;
};
extern struct tpd_dts_info tpd_dts_data;
struct tpd_attrs {
	struct device_attribute **attr;
	int num;
};
struct tpd_driver_t {
	char *tpd_device_name;
	int (*tpd_local_init)(void);
	void (*suspend)(struct device *h);
	void (*resume)(struct device *h);
	int tpd_have_button;
	struct tpd_attrs attrs;
};

void tpd_button(unsigned int x, unsigned int y, unsigned int down);
void tpd_button_init(void);
ssize_t tpd_virtual_key(char *buf);
/* #ifndef TPD_BUTTON_HEIGHT */
/* #define TPD_BUTTON_HEIGHT TPD_RES_Y */
/* #endif */

extern int tpd_driver_add(struct tpd_driver_t *tpd_drv);
extern int tpd_driver_remove(struct tpd_driver_t *tpd_drv);
void tpd_button_setting(int keycnt, void *keys, void *keys_dim);
extern int tpd_em_spl_num;
extern int tpd_em_pressure_threshold;
extern struct tpd_device *tpd;
extern struct tpd_dts_info tpd_dts_data;

extern void tpd_get_dts_info(void);
#define GTP_RST_PORT    0
#define GTP_INT_PORT    1
extern void tpd_gpio_as_int(int pin);
extern void tpd_gpio_output(int pin, int level);
extern const struct of_device_id touch_of_match[];
#ifdef TPD_DEBUG_CODE
#include "tpd_debug.h"
#endif
#ifdef TPD_DEBUG_TRACK
int DAL_Clean(void);
int DAL_Printf(const char *fmt, ...);
int LCD_LayerEnable(int id, BOOL enable);
#endif

#ifdef TPD_HAVE_CALIBRATION
#include "tpd_calibrate.h"
#endif

#include "tpd_default.h"

/* switch touch panel into different mode */
void _tpd_switch_single_mode(void);
void _tpd_switch_multiple_mode(void);
void _tpd_switch_sleep_mode(void);
void _tpd_switch_normal_mode(void);
#endif

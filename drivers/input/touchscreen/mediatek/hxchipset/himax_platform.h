/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _HIMAX_PLATFORM_H_
#define _HIMAX_PLATFORM_H_

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/types.h>

#include <linux/dma-mapping.h>
#include <linux/kthread.h>
/* #include <linux/rtpm_prio.h> */
#include "tpd.h"

#define CONFIG_OF_TOUCH
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>

#define MTK_KERNEL_44
/* #define MTK_I2C_DMA */
/* #define MTK_INT_NOT_WORK_WORKAROUND */

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
#define D(fmt, arg...) pr_debug("<<-GTP-ERROR->> " fmt "\n", ##arg)
#define I(fmt, arg...) pr_info("<<-GTP-INFO->> " fmt "\n", ##arg)
#define W(fmt, arg...) pr_debug("<<-GTP-ERROR->> " fmt "\n", ##arg)
#define E(fmt, arg...) pr_debug("<<-GTP-ERROR->> " fmt "\n", ##arg)
#define DIF(fmt, arg...)                                                       \
	do {                                                                   \
		if (debug_flag)                                                \
			pr_debug("[HXTP][DEBUG] " x "\n")                      \
	} while (0)

#else
#define D(fmt, arg...)
#define I(fmt, arg...)
#define W(fmt, arg...)
#define E(fmt, arg...)
#define DIF(fmt, arg...)
#endif

#define HIMAX_common_NAME "generic" /* "himax_tp" */

extern struct tpd_device *tpd;

static unsigned short force[] = {0, 0x90, I2C_CLIENT_END, I2C_CLIENT_END};
static const unsigned short *const forces[] = {force, NULL};

static DECLARE_WAIT_QUEUE_HEAD(waiter);

struct himax_i2c_platform_data {
	int abs_x_min;
	int abs_x_max;
	int abs_x_fuzz;
	int abs_y_min;
	int abs_y_max;
	int abs_y_fuzz;
	int abs_pressure_min;
	int abs_pressure_max;
	int abs_pressure_fuzz;
	int abs_width_min;
	int abs_width_max;
	int screenWidth;
	int screenHeight;
	uint8_t fw_version;
	uint8_t tw_id;
	uint8_t powerOff3V3;
	uint8_t cable_config[2];
	uint8_t protocol_type;
	int gpio_irq;
	int gpio_reset;
	int gpio_3v3_en;
	int (*power)(int on);
	void (*reset)(void);
	struct himax_virtual_key *virtual_key;
	struct kobject *vk_obj;
	struct kobj_attribute *vk2Use;

	int hx_config_size;
};

extern int irq_enable_count;
extern int i2c_himax_read(struct i2c_client *client, uint8_t command,
			  uint8_t *data, uint8_t length, uint8_t toRetry);
extern int i2c_himax_write(struct i2c_client *client, uint8_t command,
			   uint8_t *data, uint8_t length, uint8_t toRetry);
extern int i2c_himax_write_command(struct i2c_client *client, uint8_t command,
				   uint8_t toRetry);
extern int i2c_himax_master_write(struct i2c_client *client, uint8_t *data,
				  uint8_t length, uint8_t toRetry);
extern void himax_int_enable(int irqnum, int enable);
extern int himax_ts_register_interrupt(struct i2c_client *client);
extern uint8_t himax_int_gpio_read(int pinnum);

extern int himax_gpio_power_config(struct i2c_client *client,
				   struct himax_i2c_platform_data *pdata);
extern int of_get_himax85xx_platform_data(struct device *dev);

#if defined(CONFIG_FB)
extern int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data);
#endif

extern struct i2c_client *i2c_client_point;

extern struct himax_ic_data *ic_data;
extern struct himax_ts_data *private_ts;
extern void himax_ts_work(struct himax_ts_data *ts);
extern enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer);

#ifdef HX_TP_PROC_DIAG
extern uint8_t getDiagCommand(void);
#endif

extern void himax_log_touch_int_devation(int touched);

#endif

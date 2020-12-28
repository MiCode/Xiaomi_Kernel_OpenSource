/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */

#ifndef __AL6021_H__
#define __AL6021_H__

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <uapi/linux/ispv2_ioparam.h>


#define TIME_DETECTION 1

#define AL6021_SPI_SPEED_HZ	(1 * 1000 * 1000)
#define ISPV2_CLK_NUM		(1)
#define NO_SET_RATE			(-1)

#define ISPV2_PINCTRL_STATE_SLEEP "ispv2_suspend"
#define ISPV2_PINCTRL_STATE_DEFAULT "ispv2_default"


enum al6021_rgltr_type {
	AL6021_RGLTR_VCC,			//1p8
	AL6021_RGLTR_VDD,			//0p9
	AL6021_RGLTR_VCLK,
	AL6021_RGLTR_MAX,
};

enum al6021_power_state_type {
	AL6021_POWER_ON,
	AL6021_POWER_OFF,
	AL6021_POWER_MAX,
};

struct al6021_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	uint8_t pinctrl_status;
};

struct al6021_data_info {
	struct class *driver_class;
	struct spi_device *spi;
	struct device *dev;
	struct cdev cdev;
	struct device *devfile;
	struct mutex mutex_devfile;
	struct completion completion_irq_cause;
	dev_t devnum;

	enum al6021_power_state_type power_state;
	uint8_t read_only;
	int reset_gpio;
	int irq_gpio;
	int irq_num;

	uint32_t				num_rgltr;
	struct regulator		*rgltr[AL6021_RGLTR_MAX];
	const char			*rgltr_name[AL6021_RGLTR_MAX];
	uint32_t				rgltr_min_volt[AL6021_RGLTR_MAX];
	uint32_t				rgltr_max_volt[AL6021_RGLTR_MAX];
	uint32_t				rgltr_op_mode[AL6021_RGLTR_MAX];

	struct al6021_pinctrl_info pinctrl_info;
	const char	*clk_name;
	struct clk	*clk;
	int32_t		clk_rate;

	struct al6021_ioparam_s *iop_full;
	struct {
		u32 irq;
		u32 out_bytes;
		u32 in_bytes;
	} stats;
};

#define ALTEK_SIZEOF_STATS	(sizeof(((struct al6021_data_info *)0)->stats))

#define __FILENAME__ \
	( \
		__builtin_strrchr(__FILE__, '/') ? \
			__builtin_strrchr(__FILE__, '/') + 1 \
		: \
			__FILE__ \
	)

#define pr_e(fmt, ...) \
	pr_err("%s:%u:%s(): " fmt "\n", __FILENAME__, __LINE__, \
		__func__, ##__VA_ARGS__)

#define pr_w(fmt, ...) \
	pr_warn("%s:%u:%s(): " fmt "\n", __FILENAME__, __LINE__, \
		__func__, ##__VA_ARGS__)

#define pr_i(fmt, ...) \
	pr_info("%s:%u:%s(): " fmt "\n", __FILENAME__, __LINE__, \
		__func__, ##__VA_ARGS__)

#define pr_d(fmt, ...) \
	pr_debug("%s:%u:%s(): " fmt "\n", __FILENAME__, __LINE__, \
		__func__, ##__VA_ARGS__)

//========================================================
static int al6021_power_up(struct al6021_data_info *data);
static int al6021_power_down(struct al6021_data_info *data);

#endif // __AL6021_H__

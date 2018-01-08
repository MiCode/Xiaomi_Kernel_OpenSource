/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __QCOM_TSENS_H__
#define __QCOM_TSENS_H__

#include <linux/kernel.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DEBUG_SIZE				10
#define TSENS_MAX_SENSORS			16
#define TSENS_CONTROLLER_ID(n)			(n)
#define TSENS_CTRL_ADDR(n)			(n)
#define TSENS_TM_SN_STATUS(n)			((n) + 0xa0)

enum tsens_dbg_type {
	TSENS_DBG_POLL,
	TSENS_DBG_LOG_TEMP_READS,
	TSENS_DBG_LOG_INTERRUPT_TIMESTAMP,
	TSENS_DBG_LOG_BUS_ID_DATA,
	TSENS_DBG_MTC_DATA,
	TSENS_DBG_LOG_MAX
};

#define tsens_sec_to_msec_value		1000

struct tsens_device;

#if defined(CONFIG_THERMAL_TSENS)
int tsens2xxx_dbg(struct tsens_device *data, u32 id, u32 dbg_type, int *temp);
#else
static inline int tsens2xxx_dbg(struct tsens_device *data, u32 id,
						u32 dbg_type, int *temp)
{ return -ENXIO; }
#endif

struct tsens_dbg {
	u32				idx;
	unsigned long long		time_stmp[DEBUG_SIZE];
	unsigned long			temp[DEBUG_SIZE];
};

struct tsens_dbg_context {
	struct tsens_device		*tmdev;
	struct tsens_dbg		sensor_dbg_info[TSENS_MAX_SENSORS];
	int				tsens_critical_wd_cnt;
	u32				irq_idx;
	unsigned long long		irq_time_stmp[DEBUG_SIZE];
	struct delayed_work		tsens_critical_poll_test;
};

struct tsens_context {
	enum thermal_device_mode	high_th_state;
	enum thermal_device_mode	low_th_state;
	enum thermal_device_mode	crit_th_state;
	int				high_temp;
	int				low_temp;
	int				crit_temp;
};

struct tsens_sensor {
	struct tsens_device		*tmdev;
	struct thermal_zone_device	*tzd;
	u32				hw_id;
	u32				id;
	const char			*sensor_name;
	struct tsens_context		thr_state;
};

/**
 * struct tsens_ops - operations as supported by the tsens device
 * @init: Function to initialize the tsens device
 * @get_temp: Function which returns the temp in millidegC
 */
struct tsens_ops {
	int (*hw_init)(struct tsens_device *);
	int (*get_temp)(struct tsens_sensor *, int *);
	int (*set_trips)(struct tsens_sensor *, int, int);
	int (*interrupts_reg)(struct tsens_device *);
	int (*dbg)(struct tsens_device *, u32, u32, int *);
	int (*sensor_en)(struct tsens_device *, u32);
};

struct tsens_irqs {
	const char			*name;
	irqreturn_t (*handler)(int, void *);
};

/**
 * struct tsens_data - tsens instance specific data
 * @num_sensors: Max number of sensors supported by platform
 * @ops: operations the tsens instance supports
 * @hw_ids: Subset of sensors ids supported by platform, if not the first n
 */
struct tsens_data {
	const u32			num_sensors;
	const struct tsens_ops		*ops;
	unsigned int			*hw_ids;
	u32				temp_factor;
	bool				cycle_monitor;
	u32				cycle_compltn_monitor_mask;
	bool				wd_bark;
	u32				wd_bark_mask;
	bool				mtc;
};

struct tsens_mtc_sysfs {
	uint32_t	zone_log;
	int			zone_mtc;
	int			th1;
	int			th2;
	uint32_t	zone_hist;
};

struct tsens_device {
	struct device			*dev;
	struct platform_device		*pdev;
	struct list_head		list;
	struct regmap			*map;
	struct regmap_field		*status_field;
	void __iomem			*tsens_srot_addr;
	void __iomem			*tsens_tm_addr;
	const struct tsens_ops		*ops;
	struct tsens_dbg_context	tsens_dbg;
	spinlock_t			tsens_crit_lock;
	spinlock_t			tsens_upp_low_lock;
	const struct tsens_data		*ctrl_data;
	struct tsens_sensor		sensor[0];
	struct tsens_mtc_sysfs	mtcsys;
};

extern const struct tsens_data data_tsens2xxx, data_tsens23xx, data_tsens24xx;
extern struct list_head tsens_device_list;

#endif /* __QCOM_TSENS_H__ */

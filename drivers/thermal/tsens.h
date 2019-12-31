/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
#include <linux/ipc_logging.h>

#define DEBUG_SIZE					10
#define TSENS_MAX_SENSORS			16
#define TSENS_NUM_SENSORS_8937		11
#define TSENS_NUM_SENSORS_405		10
#define TSENS_NUM_SENSORS_9607		5
#define TSENS_SROT_OFFSET_8937		0x4
#define TSENS_SROT_OFFSET_405		0x4
#define TSENS_SROT_OFFSET_9607		0x0
#define TSENS_SN_STATUS_ADDR_8937	0x44
#define TSENS_TRDY_ADDR_8937		0x84
#define TSENS_SN_STATUS_ADDR_405	0x44
#define TSENS_TRDY_ADDR_405		0x84
#define TSENS_SN_STATUS_ADDR_9607	0x30
#define TSENS_TRDY_ADDR_9607		0x5c

#define TSENS_CONTROLLER_ID(n)			(n)
#define TSENS_CTRL_ADDR(n)			(n)
#define TSENS_TM_SN_STATUS(n)			((n) + 0xa0)

#define ONE_PT_CALIB		0x1
#define ONE_PT_CALIB2		0x2
#define TWO_PT_CALIB		0x3

#define SLOPE_FACTOR		1000
#define SLOPE_DEFAULT		3200

#define IPC_LOGPAGES 10

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

#ifdef CONFIG_DEBUG_FS
#define TSENS_IPC(idx, dev, msg, args...) do { \
		if (dev) { \
			if ((idx == 0) && (dev)->ipc_log0) \
				ipc_log_string((dev)->ipc_log0, \
					"%s: " msg, __func__, args); \
			else if ((idx == 1) && (dev)->ipc_log1) \
				ipc_log_string((dev)->ipc_log1, \
					"%s: " msg, __func__, args); \
			else if ((idx == 2) && (dev)->ipc_log2) \
				ipc_log_string((dev)->ipc_log2, \
					"%s: " msg, __func__, args); \
			else \
				pr_debug("tsens: invalid logging index\n"); \
		} \
	} while (0)
#define TSENS_DUMP(dev, msg, args...) do {				\
		TSENS_IPC(2, dev, msg, args); \
		pr_info(msg, ##args);	\
	} while (0)
#define TSENS_ERR(dev, msg, args...) do {				\
		pr_err(msg, ##args);	\
		TSENS_IPC(1, dev, msg, args); \
	} while (0)
#define TSENS_INFO(dev, msg, args...) do {				\
		pr_info(msg, ##args);	\
		TSENS_IPC(1, dev, msg, args); \
	} while (0)
#define TSENS_DBG(dev, msg, args...) do {				\
		pr_debug(msg, ##args);	\
		if (dev) { \
			TSENS_IPC(0, dev, msg, args); \
		}	\
	} while (0)
#define TSENS_DBG1(dev, msg, args...) do {				\
		pr_debug(msg, ##args);	\
		if (dev) { \
			TSENS_IPC(1, dev, msg, args); \
		}	\
	} while (0)
#else
#define	TSENS_DBG1(x...)		pr_debug(x)
#define	TSENS_DBG(x...)		pr_debug(x)
#define	TSENS_INFO(x...)		pr_info(x)
#define	TSENS_ERR(x...)		pr_err(x)
#define	TSENS_DUMP(x...)		pr_info(x)
#endif

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
	int				high_adc_code;
	int				low_adc_code;
};

struct tsens_sensor {
	struct tsens_device		*tmdev;
	struct thermal_zone_device	*tzd;
	u32				hw_id;
	u32				id;
	const char			*sensor_name;
	struct tsens_context		thr_state;
	int				offset;
	int				slope;
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
	int (*calibrate)(struct tsens_device *);
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
	bool				valid_status_check;
	u32				ver_major;
	u32				ver_minor;
	const u32			tsens_srot_offset;
	const u32			tsens_sn_offset;
	const u32			tsens_trdy_offset;
};

struct tsens_mtc_sysfs {
	u32			zone_log;
	int			zone_mtc;
	int			th1;
	int			th2;
	u32			zone_hist;
};

struct tsens_device {
	struct device			*dev;
	struct platform_device		*pdev;
	struct list_head		list;
	struct regmap			*map;
	struct regmap_field		*status_field;
	void __iomem			*tsens_srot_addr;
	void __iomem			*tsens_tm_addr;
	void __iomem			*tsens_calib_addr;
	const struct tsens_ops		*ops;
	void					*ipc_log0;
	void					*ipc_log1;
	void					*ipc_log2;
	phys_addr_t				phys_addr_tm;
	struct tsens_dbg_context	tsens_dbg;
	spinlock_t			tsens_crit_lock;
	spinlock_t			tsens_upp_low_lock;
	const struct tsens_data		*ctrl_data;
	struct tsens_mtc_sysfs  mtcsys;
	int				trdy_fail_ctr;
	struct workqueue_struct		*tsens_reinit_work;
	struct work_struct		therm_fwk_notify;
	bool				tsens_reinit_wa;
	struct tsens_sensor		sensor[0];
};

extern const struct tsens_data data_tsens2xxx, data_tsens23xx, data_tsens24xx;
extern const struct tsens_data data_tsens14xx, data_tsens14xx_405,
						data_tsens14xx_9607;
extern struct list_head tsens_device_list;

extern int calibrate_8937(struct tsens_device *tmdev);
extern int calibrate_405(struct tsens_device *tmdev);
extern int calibrate_9607(struct tsens_device *tmdev);

#endif /* __QCOM_TSENS_H__ */

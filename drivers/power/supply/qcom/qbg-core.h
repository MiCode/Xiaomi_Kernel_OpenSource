/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef __QBG_CORE_H__
#define __QBG_CORE_H__

#define qbg_dbg(chip, reason, fmt, ...)			\
	do {							\
		if (*chip->debug_mask & (reason))		\
			pr_err(fmt, ##__VA_ARGS__);	\
		else						\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

enum debug_mask {
	QBG_DEBUG_BUS_READ	= BIT(0),
	QBG_DEBUG_BUS_WRITE	= BIT(1),
	QBG_DEBUG_SDAM		= BIT(2),
	QBG_DEBUG_IRQ		= BIT(3),
	QBG_DEBUG_DEVICE	= BIT(4),
	QBG_DEBUG_PROFILE	= BIT(5),
	QBG_DEBUG_SOC		= BIT(6),
	QBG_DEBUG_STATUS	= BIT(7),
	QBG_DEBUG_PON		= BIT(8),
};

enum qbg_sdam {
	SDAM_CTRL0 = 0,
	SDAM_CTRL1,
	SDAM_DATA0,
	SDAM_DATA1,
	SDAM_DATA2,
	SDAM_DATA3,
	SDAM_DATA4,
};

enum qbg_data_tag {
	QBG_DATA_TAG_FAST_CHAR,
};

enum QBG_SAMPLE_NUM_TYPE {
	SAMPLE_NUM_1,
	SAMPLE_NUM_2,
	SAMPLE_NUM_4,
	SAMPLE_NUM_8,
	SAMPLE_NUM_16,
	SAMPLE_NUM_32,
	QBG_SAMPLE_NUM_INVALID,
};

enum QBG_ACCUM_INTERVAL_TYPE {
	ACCUM_INTERVAL_100MS,
	ACCUM_INTERVAL_200MS,
	ACCUM_INTERVAL_500MS,
	ACCUM_INTERVAL_1000MS,
	ACCUM_INTERVAL_2000MS,
	ACCUM_INTERVAL_5000MS,
	ACCUM_INTERVAL_10000MS,
	ACCUM_INTERVAL_100000MS,
	ACCUM_INTERVAL_INVALID,
};

/**
 * struct qti_qbg - Structure for QTI QBG device
 * @dev:		Pointer to QBG device structure
 * @regmap:		Pointer to regmap structure
 * @qbg_psy:		Pointer to QBG power supply
 * @batt_psy:		Pointer to Battery power supply
 * @qbg_class:		Pointer to QBG class
 * @qbg_device:		Pointer to QBG device
 * @qbg_cdev:		Member for QBG char device
 * @dev_no:		Device number for QBG char device
 * @batt_node:		Pointer to battery device node
 * @indio_dev:		Pointer to QBG IIO device
 * @iio_chan:		Pointer to QBG IIO channels
 * @sdam:		Pointer to multiple QBG SDAMs
 * @fifo:		QBG FIFO data
 * @essential_params:	QBG essential params
 * @status_change_work:	Power supply status change work
 * @udata_work:		User space data change work
 * @nb:			Power supply notifier block
 * @kdata:		QBG Kernel space data structure
 * @udata:		QBG user space data structure
 * @battery:		Pointer to QBG battery data structure
 * @step_chg_jeita_params:	Jeita step charge parameters structure
 * @fifo_lock:		Lock for reading FIFO data
 * @data_lock:		Lock for reading kdata from QBG char device
 * @batt_id_chan:	IIO channel to read battery ID
 * @batt_temp_chan:	IIO channel to read battery temperature
 * @rtc:		RTC device to read real time
 * @last_fast_char_time: Timestamp of last time QBG in fast char mode
 * @qbg_wait_q:		Wait queue for reads to QBG char device
 * @irq_name:		QBG interrupt name
 * @batt_type_str:	String array denoting battery type
 * @irq:		QBG irq number
 * @base:		Base address of QBG HW
 * @num_data_sdams:	Number of data sdams used for QBG
 * @batt_id_ohm:	Battery resistance in ohms
 * @sdam_batt_id:	Battery ID stored and retrieved from SDAM
 * @essential_param_revid:	QBG essential parameters revision ID
 * @sample_time_us:	Array of accumulator sample time in each QBG HW state
 * @debug_mask:		Debug mask to enable/disable debug prints
 * @pon_ocv:		Power-on OCV of QBG device
 * @pon_ibat:		Power-on current of QBG device
 * @pon_soc:		Power-on SOC of QBG device
 * @soc:		Monotonic SOC of QBG device
 * @batt_soc:		Battery SOC
 * @sys_soc:		Battery system SOC
 * @esr:		Battery equivalent series resistance
 * @ocv_uv:		Battery open circuit voltage
 * @voltage_now:	Battery voltage
 * @current_now:	Battery current
 * @tbat:		Battery temperature
 * @charge_cycle_count:	Battery charge cycle count
 * @nominal_capacity:	Battery nominal capacity
 * @learned_capacity:	Battery learned capacity
 * @ttf:		Time to full
 * @tte:		Time to empty
 * @soh:		Battery state of health
 * @charge_type:	Charging type
 * @float_volt_uv:	Battery maximum voltage
 * @fastchg_curr_ma:	Battery fast charge current
 * @vbat_cutoff_mv:	Battery cutoff voltage
 * @ibat_cutoff_ma:	Battery cutoff current
 * @vph_min_mv:	Battery minimum power
 * @iterm_ma:	Charge Termination current
 * @rconn_mohm:	Battery connector resistance
 * @previous_ep_time:	Previous timestamp when essential params stored
 * @current_time:	Current time stamp
 * @profile_loaded:	Flag to indicated battery profile is loaded
 * @battery_missing:	Flag to indicate battery is missing
 * @data_ready:		Flag to indicate QBG data is ready
 * @in_fast_char:	Flag to indicate QBG is in fast char mode
 */
struct qti_qbg {
	struct device		*dev;
	struct regmap		*regmap;
	struct power_supply	*qbg_psy;
	struct power_supply	*batt_psy;
	struct class		*qbg_class;
	struct device		*qbg_device;
	struct cdev		qbg_cdev;
	dev_t			dev_no;
	struct device_node      *batt_node;
	struct iio_dev		*indio_dev;
	struct iio_chan_spec	*iio_chan;
	struct nvmem_device	**sdam;
	struct fifo_data	fifo[MAX_FIFO_COUNT];
	struct qbg_essential_params	essential_params;
	struct work_struct	status_change_work;
	struct work_struct	udata_work;
	struct notifier_block	nb;
	struct qbg_kernel_data	kdata;
	struct qbg_user_data	udata;
	struct qbg_battery_data	*battery;
	struct qbg_step_chg_jeita_params	*step_chg_jeita_params;
	struct mutex		fifo_lock;
	struct mutex		data_lock;
	struct iio_channel	*batt_id_chan;
	struct iio_channel	*batt_temp_chan;
	struct rtc_device	*rtc;
	ktime_t			last_fast_char_time;
	wait_queue_head_t	qbg_wait_q;
	const char		*irq_name;
	const char		*batt_type_str;
	int			irq;
	u32			base;
	u32			sdam_base;
	u32			num_data_sdams;
	u32			batt_id_ohm;
	u32			sdam_batt_id;
	u32			essential_param_revid;
	u32			sample_time_us[QBG_STATE_MAX];
	u32			*debug_mask;
	int			pon_ocv;
	int			pon_ibat;
	int			pon_tbat;
	int			pon_soc;
	int			soc;
	int			batt_soc;
	int			sys_soc;
	int			esr;
	int			ocv_uv;
	int			voltage_now;
	int			current_now;
	int			tbat;
	int			charge_cycle_count;
	int			nominal_capacity;
	int			learned_capacity;
	int			ttf;
	int			tte;
	int			soh;
	int			charge_type;
	int			float_volt_uv;
	int			fastchg_curr_ma;
	int			vbat_cutoff_mv;
	int			ibat_cutoff_ma;
	int			vph_min_mv;
	int			iterm_ma;
	int			rconn_mohm;
	unsigned long		previous_ep_time;
	unsigned long		current_time;
	bool			profile_loaded;
	bool			battery_missing;
	bool			data_ready;
	bool			in_fast_char;
};
#endif /* __QBG_CORE_H__ */

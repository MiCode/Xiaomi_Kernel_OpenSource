/*
 * Copyright (c) 2011-2012 Synaptics Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/rmi.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/wakelock.h>
#include <asm-generic/uaccess.h>

#define FUNCTION_DATA rmi_fn_54_data
#define FNUM 54

#include "rmi_driver.h"

/* Set this to 1 for raw hex dump of returned data. */
#define RAW_HEX 0
/* Set this to 1 for human readable dump of returned data. */
#define HUMAN_READABLE 0
/* The watchdog timer can be useful when debugging certain firmware related
 * issues.
 */
#define F54_WATCHDOG 1

/* define fn $54 commands */
#define GET_REPORT                1
#define FORCE_CAL                 2

#define NO_AUTO_CAL_MASK 1
/* status */
#define BUSY 1
#define IDLE 0

/* Offsets for data */
#define RMI_F54_REPORT_DATA_OFFSET	3
#define RMI_F54_FIFO_OFFSET		1
#define RMI_F54_NUM_TX_OFFSET		1
#define RMI_F54_NUM_RX_OFFSET		0

/* Fixed sizes of reports */
#define RMI_54_FULL_RAW_CAP_MIN_MAX_SIZE 4
#define RMI_54_HIGH_RESISTANCE_SIZE 6

#define FUNCTION_NUMBER 0x54

/* data already read (STALE or FRESH)? */
#define F54_REPORT_FRESH     1
#define F54_REPORT_STALE     0
#define RMI_F54_WATCHDOG_TIMEOUT_MSEC 100
#define RMI_F54_STEP_MSEC 5
#define MAX_USER_BUFFER_SIZE 16

#define FN_54_DT_CONF_BUFFER_SIZE 2048

/* character device name for fast image transfer (if enabled) */
/* the device_register will add another '0' to this, making it "rawsensor00" */
#define RAW_IMAGE_CHAR_DEVICE_NAME "rawsensor0"

struct raw_data_char_dev {
	/* mutex for file operation*/
	struct mutex mutex_file_op;
	/* main char dev structure */
	struct cdev raw_data_dev;
	struct class *raw_data_device_class;
	struct rmi_fn_54_data *my_parents_instance_data;
};

/* definitions for F54 Query Registers */
union f54_ad_query {
	struct {
	/* query 0 */
		u8 num_of_rx_electrodes;

	/* query 1 */
		u8 num_of_tx_electrodes;

			/* query2 */
			u8 f54_ad_query2_b0__1:2;
			u8 has_baseline:1;
			u8 has_image8:1;
			u8 f54_ad_query2_b4__5:2;
			u8 has_image16:1;
			u8 f54_ad_query2_b7:1;

	/* query 3.0 and 3.1 */
	u16 clock_rate;

	/* query 4 */
	u8 touch_controller_family;

	/* query 5 */
			u8 has_pixel_touch_threshold_adjustment:1;
			u8 f54_ad_query5_b1__7:7;

	/* query 6 */
		u8 has_sensor_assignment:1;
		u8 has_interference_metric:1;
		u8 has_sense_frequency_control:1;
		u8 has_firmware_noise_mitigation:1;
		u8 f54_ad_query6_b4:1;
		u8 has_two_byte_report_rate:1;
		u8 has_one_byte_report_rate:1;
		u8 has_relaxation_control:1;

	/* query 7 */
			u8 curve_compensation_mode:2;
			u8 f54_ad_query7_b2__7:6;

	/* query 8 */
		u8 f54_ad_query2_b0:1;
		u8 has_iir_filter:1;
		u8 has_cmn_removal:1;
		u8 has_cmn_maximum:1;
		u8 has_touch_hysteresis:1;
		u8 has_edge_compensation:1;
		u8 has_per_frequency_noise_control:1;
		u8 f54_ad_query8_b7:1;

	u8 f54_ad_query9;
	u8 f54_ad_query10;

		/* query 11 */
		u8 f54_ad_query11_b0__6:7;
		u8 has_query_15:1;

	/* query 12 */
			u8 number_of_sensing_frequencies:4;
			u8 f54_ad_query12_b4__7:4;
	} __attribute__((__packed__));
	struct {
		u8 regs[14];
		u16 address;
	} __attribute__((__packed__));
};

/* And now for the very large amount of control registers */

/* Ctrl registers */

union f54_ad_control_0 {
	/* control 0 */
	struct {
		u8 no_relax:1;
		u8 no_scan:1;
		u8 f54_ad_ctrl0_b2__7:6;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_1 {
	/* control 1 */
	struct {
		/* control 1 */
		u8 bursts_per_cluster:4;
		u8 f54_ad_ctrl1_b4__7:4;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_2 {
	/* control 2 */
	struct {
		u16 saturation_cap;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_3 {
	/* control 3 */
	struct {
		u16 pixel_touch_threshold;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_4__6 {
	struct {
		/* control 4 */
		u8 rx_feedback_cap:2;
		u8 f54_ad_ctrl4_b2__7:6;

		/* control 5 */
		u8 low_ref_cap:2;
		u8 low_ref_feedback_cap:2;
		u8 low_ref_polarity:1;
		u8 f54_ad_ctrl5_b5__7:3;

		/* control 6 */
		u8 high_ref_cap:2;
		u8 high_ref_feedback_cap:2;
		u8 high_ref_polarity:1;
		u8 f54_ad_ctrl6_b5__7:3;
	} __attribute__((__packed__));
	struct {
		u8 regs[3];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_7 {
	struct {
		/* control 7 */
		u8 cbc_cap:2;
		u8 cbc_polarity:2;
		u8 cbc_tx_carrier_selection:1;
		u8 f54_ad_ctrl6_b5__7:3;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_8__9 {
	struct {
		/* control 8 */
		u16 integration_duration:10;
		u16 f54_ad_ctrl8_b10__15:6;
		/* control 9 */
		u8 reset_duration;
	} __attribute__((__packed__));
	struct {
		u8 regs[3];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_10 {
	struct {
		/* control 10 */
		u8 noise_sensing_bursts_per_image:4;
		u8 f54_ad_ctrl10_b4__7:4;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_11 {
	struct {
		/* control 11 */
		u8 reserved;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_12__13 {
	struct {
		/* control 12 */
		u8 slow_relaxation_rate;

		/* control 13 */
		u8 fast_relaxation_rate;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_14 {
	struct {
		/* control 14 */
			u8 rxs_on_xaxis:1;
			u8 curve_comp_on_txs:1;
			u8 f54_ad_ctrl14b2__7:6;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

struct f54_ad_control_15n {
	/*Control 15.* */
	u8 sensor_rx_assignment;
};

struct f54_ad_control_15 {
		struct f54_ad_control_15n *regs;
		u16 address;
		u8 length;
};

struct f54_ad_control_16n {
	/*Control 16.* */
	u8 sensor_tx_assignment;
};

struct f54_ad_control_16 {
		struct f54_ad_control_16n *regs;
		u16 address;
		u8 length;
};


/* control 17 */
struct f54_ad_control_17n {
	u8 burst_countb10__8:3;
	u8 disable:1;
	u8 f54_ad_ctrlb4:1;
	u8 filter_bandwidth:3;
} __attribute__((__packed__));

struct f54_ad_control_17 {
		struct f54_ad_control_17n *regs;
		u16 address;
		u8 length;
};

struct f54_ad_control_18n {
	/*Control 18.* */
	u8 burst_countb7__0n;
} __attribute__((__packed__));

struct f54_ad_control_18 {
		struct f54_ad_control_18n *regs;
		u16 address;
		u8 length;
};

struct f54_ad_control_19n {
	/*Control 19.* */
	u8 stretch_duration;
} __attribute__((__packed__));

struct f54_ad_control_19 {
		struct f54_ad_control_19n *regs;
		u16 address;
		u8 length;
};

union f54_ad_control_20 {
	struct {
		/* control 20 */
		u8 disable_noise_mitigation:1;
		u8 f54_ad_ctrl20b2__7:7;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_21 {
	struct {
		/* control 21 */
		u16 freq_shift_noise_threshold;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_22__26 {
	struct {
		/* control 22 */
		/* u8 noise_density_threshold; */
		u8 f54_ad_ctrl22;

		/* control 23 */
		u16 medium_noise_threshold;

		/* control 24 */
		u16 high_noise_threshold;

		/* control 25 */
		u8 noise_density;

		/* control 26 */
		u8 frame_count;
	} __attribute__((__packed__));
	struct {
		u8 regs[7];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_27 {
	struct {
		/* control 27 */
		u8 iir_filter_coef;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_28 {
	struct {
		/* control 28 */
		u16 quiet_threshold;
	} __attribute__((__packed__));
	struct {
		u8 regs[2];
		u16 address;
	} __attribute__((__packed__));
};


union f54_ad_control_29 {
	struct {
		/* control 29 */
		u8 f54_ad_ctrl20b0__6:7;
		u8 cmn_filter_disable:1;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_30 {
	struct {
		/* control 30 */
		u8 cmn_filter_max;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_31 {
	struct {
		/* control 31 */
		u8 touch_hysteresis;
	} __attribute__((__packed__));
	struct {
		u8 regs[1];
		u16 address;
	} __attribute__((__packed__));
};

union f54_ad_control_32__35 {
	struct {
		/* control 32 */
		u16 rx_low_edge_comp;

		/* control 33 */
		u16 rx_high_edge_comp;

		/* control 34 */
		u16 tx_low_edge_comp;

		/* control 35 */
		u16 tx_high_edge_comp;
	} __attribute__((__packed__));
	struct {
		u8 regs[8];
		u16 address;
	} __attribute__((__packed__));
};

struct f54_ad_control_36n {
	/*Control 36.* */
	u8 axis1_comp;
} __attribute__((__packed__));

struct f54_ad_control_36 {
		struct f54_ad_control_36n *regs;
		u16 address;
		u8 length;
};

struct f54_ad_control_37n {
	/*Control 37.* */
	u8 axis2_comp;
} __attribute__((__packed__));

struct f54_ad_control_37 {
		struct f54_ad_control_37n *regs;
		u16 address;
		u8 length;
};

struct f54_ad_control_38n {
	/*Control 38.* */
	u8 noise_control_1;
} __attribute__((__packed__));

struct f54_ad_control_38 {
		struct f54_ad_control_38n *regs;
		u16 address;
		u8 length;
};

struct f54_ad_control_39n {
	/*Control 39.* */
	u8 noise_control_2;
} __attribute__((__packed__));

struct f54_ad_control_39 {
		struct f54_ad_control_39n *regs;
		u16 address;
		u8 length;
};

struct f54_ad_control_40n {
	/*Control 40.* */
	u8 noise_control_3;
} __attribute__((__packed__));

struct f54_ad_control_40 {
		struct f54_ad_control_40n *regs;
		u16 address;
		u8 length;
};

struct f54_ad_control {
	union f54_ad_control_0 *reg_0;
	union f54_ad_control_1 *reg_1;
	union f54_ad_control_2 *reg_2;
	union f54_ad_control_3 *reg_3;
	union f54_ad_control_4__6 *reg_4__6;
	union f54_ad_control_7 *reg_7;
	union f54_ad_control_8__9 *reg_8__9;
	union f54_ad_control_10 *reg_10;
	union f54_ad_control_11 *reg_11;
	union f54_ad_control_12__13 *reg_12__13;
	union f54_ad_control_14 *reg_14;
	/* control 15 */
	struct f54_ad_control_15 *reg_15;
	/* control 16 */
	struct f54_ad_control_16 *reg_16;

	/* This register is n repetitions of f54_ad_control_17 */
	struct f54_ad_control_17 *reg_17;

	/* control 18 */
	struct f54_ad_control_18 *reg_18;

	/* control 19 */
	struct f54_ad_control_19 *reg_19;

	union f54_ad_control_20 *reg_20;
	union f54_ad_control_21 *reg_21;
	union f54_ad_control_22__26 *reg_22__26;
	union f54_ad_control_27 *reg_27;
	union f54_ad_control_28 *reg_28;
	union f54_ad_control_29 *reg_29;
	union f54_ad_control_30 *reg_30;
	union f54_ad_control_31 *reg_31;
	union f54_ad_control_32__35 *reg_32__35;
	/* control 36 */
	struct f54_ad_control_36 *reg_36;

	/* control 37 */
	struct f54_ad_control_37 *reg_37;

	/* control 38 */
	struct f54_ad_control_38 *reg_38;

	/* control 39 */
	struct f54_ad_control_39 *reg_39;

	/* control 40 */
	struct f54_ad_control_40 *reg_40;
};

/* define report types */
enum f54_report_types {
	F54_8BIT_IMAGE = 1,
	F54_16BIT_IMAGE = 2,
	F54_RAW_16BIT_IMAGE = 3,
	F54_HIGH_RESISTANCE = 4,
	F54_TX_TO_TX_SHORT = 5,
	F54_RX_TO_RX1 = 7,
	F54_TRUE_BASELINE = 9,
	F54_FULL_RAW_CAP_MIN_MAX = 13,
	F54_RX_OPENS1 = 14,
	F54_TX_OPEN = 15,
	F54_TX_TO_GROUND = 16,
	F54_RX_TO_RX2 = 17,
	F54_RX_OPENS2 = 18,
	F54_FULL_RAW_CAP = 19,
	F54_FULL_RAW_CAP_RX_COUPLING_COMP = 20,
	F54_16BIT_UNSIGNED_RAW_IMAGE  =    24,
};


/* sysfs functions */
show_store_union_struct_prototype(report_type)

store_union_struct_prototype(get_report)

store_union_struct_prototype(force_cal)

show_union_struct_prototype(status)

static ssize_t rmi_fn_54_data_read(struct file *data_file, struct kobject *kobj,
					struct bin_attribute *attributes,
					char *buf, loff_t pos, size_t count);

show_union_struct_prototype(num_of_rx_electrodes)
show_union_struct_prototype(num_of_tx_electrodes)
show_union_struct_prototype(has_image16)
show_union_struct_prototype(has_image8)
show_union_struct_prototype(has_baseline)
show_union_struct_prototype(clock_rate)
show_union_struct_prototype(touch_controller_family)
show_union_struct_prototype(has_pixel_touch_threshold_adjustment)
show_union_struct_prototype(has_sensor_assignment)
show_union_struct_prototype(has_interference_metric)
show_union_struct_prototype(has_sense_frequency_control)
show_union_struct_prototype(has_firmware_noise_mitigation)
show_union_struct_prototype(has_two_byte_report_rate)
show_union_struct_prototype(has_one_byte_report_rate)
show_union_struct_prototype(has_relaxation_control)
show_union_struct_prototype(curve_compensation_mode)
show_union_struct_prototype(has_iir_filter)
show_union_struct_prototype(has_cmn_removal)
show_union_struct_prototype(has_cmn_maximum)
show_union_struct_prototype(has_touch_hysteresis)
show_union_struct_prototype(has_edge_compensation)
show_union_struct_prototype(has_per_frequency_noise_control)
show_union_struct_prototype(number_of_sensing_frequencies)
show_store_union_struct_prototype(no_auto_cal)
show_store_union_struct_prototype(fifoindex)

/* Repeated Control Registers */
show_union_struct_prototype(sensor_rx_assignment)
show_union_struct_prototype(sensor_tx_assignment)
show_union_struct_prototype(filter_bandwidth)
show_union_struct_prototype(disable)
show_union_struct_prototype(burst_count)
show_union_struct_prototype(stretch_duration)
show_store_union_struct_prototype(axis1_comp)
show_store_union_struct_prototype(axis2_comp)
show_union_struct_prototype(noise_control_1)
show_union_struct_prototype(noise_control_2)
show_union_struct_prototype(noise_control_3)

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
show_store_union_struct_prototype(no_relax)
show_store_union_struct_prototype(no_scan)
show_store_union_struct_prototype(bursts_per_cluster)
show_store_union_struct_prototype(saturation_cap)
show_store_union_struct_prototype(pixel_touch_threshold)
show_store_union_struct_prototype(rx_feedback_cap)
show_store_union_struct_prototype(low_ref_cap)
show_store_union_struct_prototype(low_ref_feedback_cap)
show_store_union_struct_prototype(low_ref_polarity)
show_store_union_struct_prototype(high_ref_cap)
show_store_union_struct_prototype(high_ref_feedback_cap)
show_store_union_struct_prototype(high_ref_polarity)
show_store_union_struct_prototype(cbc_cap)
show_store_union_struct_prototype(cbc_polarity)
show_store_union_struct_prototype(cbc_tx_carrier_selection)
show_store_union_struct_prototype(integration_duration)
show_store_union_struct_prototype(reset_duration)
show_store_union_struct_prototype(noise_sensing_bursts_per_image)
show_store_union_struct_prototype(slow_relaxation_rate)
show_store_union_struct_prototype(fast_relaxation_rate)
show_store_union_struct_prototype(rxs_on_xaxis)
show_store_union_struct_prototype(curve_comp_on_txs)
show_store_union_struct_prototype(disable_noise_mitigation)
show_store_union_struct_prototype(freq_shift_noise_threshold)
/*show_store_union_struct_prototype(noise_density_threshold)*/
show_store_union_struct_prototype(medium_noise_threshold)
show_store_union_struct_prototype(high_noise_threshold)
show_store_union_struct_prototype(noise_density)
show_store_union_struct_prototype(frame_count)
show_store_union_struct_prototype(iir_filter_coef)
show_store_union_struct_prototype(quiet_threshold)
show_store_union_struct_prototype(cmn_filter_disable)
show_store_union_struct_prototype(cmn_filter_max)
show_store_union_struct_prototype(touch_hysteresis)
show_store_union_struct_prototype(rx_low_edge_comp)
show_store_union_struct_prototype(rx_high_edge_comp)
show_store_union_struct_prototype(tx_low_edge_comp)
show_store_union_struct_prototype(tx_high_edge_comp)

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

static struct attribute *attrs[] = {
	attrify(report_type),
	attrify(get_report),
	attrify(force_cal),
	attrify(status),
	attrify(num_of_rx_electrodes),
	attrify(num_of_tx_electrodes),
	attrify(has_image16),
	attrify(has_image8),
	attrify(has_baseline),
	attrify(clock_rate),
	attrify(touch_controller_family),
	attrify(has_pixel_touch_threshold_adjustment),
	attrify(has_sensor_assignment),
	attrify(has_interference_metric),
	attrify(has_sense_frequency_control),
	attrify(has_firmware_noise_mitigation),
	attrify(has_two_byte_report_rate),
	attrify(has_one_byte_report_rate),
	attrify(has_relaxation_control),
	attrify(curve_compensation_mode),
	attrify(has_iir_filter),
	attrify(has_cmn_removal),
	attrify(has_cmn_maximum),
	attrify(has_touch_hysteresis),
	attrify(has_edge_compensation),
	attrify(has_per_frequency_noise_control),
	attrify(number_of_sensing_frequencies),
	attrify(no_auto_cal),
	attrify(fifoindex),
	NULL
};

static struct attribute_group attrs_query = GROUP(attrs);

struct bin_attribute dev_rep_data = {
	.attr = {
		 .name = "rep_data",
		 .mode = RMI_RO_ATTR},
	.size = 0,
	.read = rmi_fn_54_data_read,
};

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

static struct attribute *attrs_reg_0[] = {
	attrify(no_relax),
	attrify(no_scan),
	NULL
};

static struct attribute *attrs_reg_1[] = {
	attrify(bursts_per_cluster),
	NULL
};

static struct attribute *attrs_reg_2[] = {
	attrify(saturation_cap),
	NULL
};

static struct attribute *attrs_reg_3[] = {
	attrify(pixel_touch_threshold),
	NULL
};

static struct attribute *attrs_reg_4__6[] = {
	attrify(rx_feedback_cap),
	attrify(low_ref_cap),
	attrify(low_ref_feedback_cap),
	attrify(low_ref_polarity),
	attrify(high_ref_cap),
	attrify(high_ref_feedback_cap),
	attrify(high_ref_polarity),
	NULL
};

static struct attribute *attrs_reg_7[] = {
	attrify(cbc_cap),
	attrify(cbc_polarity),
	attrify(cbc_tx_carrier_selection),
	NULL
};

static struct attribute *attrs_reg_8__9[] = {
	attrify(integration_duration),
	attrify(reset_duration),
	NULL
};

static struct attribute *attrs_reg_10[] = {
	attrify(noise_sensing_bursts_per_image),
	NULL
};

static struct attribute *attrs_reg_12__13[] = {
	attrify(slow_relaxation_rate),
	attrify(fast_relaxation_rate),
	NULL
};

static struct attribute *attrs_reg_14__16[] = {
	attrify(rxs_on_xaxis),
	attrify(curve_comp_on_txs),
	attrify(sensor_rx_assignment),
	attrify(sensor_tx_assignment),
	NULL
};

static struct attribute *attrs_reg_17__19[] = {
	attrify(filter_bandwidth),
	attrify(disable),
	attrify(burst_count),
	attrify(stretch_duration),
	NULL
};

static struct attribute *attrs_reg_20[] = {
	attrify(disable_noise_mitigation),
	NULL
};

static struct attribute *attrs_reg_21[] = {
	attrify(freq_shift_noise_threshold),
	NULL
};

static struct attribute *attrs_reg_22__26[] = {
	/*attrify(noise_density_threshold),*/
	attrify(medium_noise_threshold),
	attrify(high_noise_threshold),
	attrify(noise_density),
	attrify(frame_count),
	NULL
};

static struct attribute *attrs_reg_27[] = {
	attrify(iir_filter_coef),
	NULL
};

static struct attribute *attrs_reg_28[] = {
	attrify(quiet_threshold),
	NULL
};

static struct attribute *attrs_reg_29[] = {
	attrify(cmn_filter_disable),
	NULL
};

static struct attribute *attrs_reg_30[] = {
	attrify(cmn_filter_max),
	NULL
};

static struct attribute *attrs_reg_31[] = {
	attrify(touch_hysteresis),
	NULL
};

static struct attribute *attrs_reg_32__35[] = {
	attrify(rx_low_edge_comp),
	attrify(rx_high_edge_comp),
	attrify(tx_low_edge_comp),
	attrify(tx_high_edge_comp),
	NULL
};

static struct attribute *attrs_reg_36[] = {
	attrify(axis1_comp),
	NULL
};

static struct attribute *attrs_reg_37[] = {
	attrify(axis2_comp),
	NULL
};

static struct attribute *attrs_reg_38__40[] = {
	attrify(noise_control_1),
	attrify(noise_control_2),
	attrify(noise_control_3),
	NULL
};

static struct attribute_group attrs_ctrl_regs[] = {
	GROUP(attrs_reg_0),
	GROUP(attrs_reg_1),
	GROUP(attrs_reg_2),
	GROUP(attrs_reg_3),
	GROUP(attrs_reg_4__6),
	GROUP(attrs_reg_7),
	GROUP(attrs_reg_8__9),
	GROUP(attrs_reg_10),
	GROUP(attrs_reg_12__13),
	GROUP(attrs_reg_14__16),
	GROUP(attrs_reg_17__19),
	GROUP(attrs_reg_20),
	GROUP(attrs_reg_21),
	GROUP(attrs_reg_22__26),
	GROUP(attrs_reg_27),
	GROUP(attrs_reg_28),
	GROUP(attrs_reg_29),
	GROUP(attrs_reg_30),
	GROUP(attrs_reg_31),
	GROUP(attrs_reg_32__35),
	GROUP(attrs_reg_36),
	GROUP(attrs_reg_37),
	GROUP(attrs_reg_38__40)
};

/* data specific to fn $54 that needs to be kept around */
struct rmi_fn_54_data {
	union f54_ad_query query;
	struct f54_ad_control control;
	bool attrs_ctrl_regs_exist[ARRAY_SIZE(attrs_ctrl_regs)];
	enum f54_report_types report_type;
	u16 fifoindex;
	signed char status;
	bool no_auto_cal;
	unsigned int report_size;
	u8 *report_data;
	unsigned int bufsize;
	struct mutex data_mutex;
	struct mutex status_mutex;
	struct mutex control_mutex;
#if F54_WATCHDOG
	struct hrtimer watchdog;
#endif
	struct rmi_function_dev *fn_dev;
	struct work_struct work;

	signed char fresh_or_stale;
	struct raw_data_char_dev *raw_data_feed;
};

static int raw_data_char_dev_register(struct rmi_fn_54_data *);

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
#if F54_WATCHDOG
static enum hrtimer_restart clear_status(struct hrtimer *timer);

static void clear_status_worker(struct work_struct *work);
#endif

static int rmi_f54_alloc_memory(struct rmi_function_dev *fn_dev);

static void rmi_f54_free_memory(struct rmi_function_dev *fn_dev);

static int rmi_f54_initialize(struct rmi_function_dev *fn_dev);

static int rmi_f54_reset(struct rmi_function_dev *fn_dev);

static int rmi_f54_create_sysfs(struct rmi_function_dev *fn_dev);

static int rmi_f54_probe(struct rmi_function_dev *fn_dev)
{
	int retval = 0;
	struct rmi_fn_54_data *f54;

	retval = rmi_f54_alloc_memory(fn_dev);
	if (retval < 0)
		goto error_exit;

	retval = rmi_f54_initialize(fn_dev);
	if (retval < 0)
			goto error_exit;

	retval = rmi_f54_create_sysfs(fn_dev);
	if (retval < 0)
		goto error_exit;
	f54 = fn_dev->data;
	f54->status = IDLE;
	return retval;

error_exit:
	rmi_f54_free_memory(fn_dev);

	return retval;
}

static int rmi_f54_alloc_memory(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_54_data *f54;

	f54 = devm_kzalloc(&fn_dev->dev,
			   sizeof(struct rmi_fn_54_data), GFP_KERNEL);
	if (!f54) {
		dev_err(&fn_dev->dev, "Failed to allocate rmi_fn_54_data.\n");
		return -ENOMEM;
	}
	fn_dev->data = f54;
	f54->fn_dev = fn_dev;


	return 0;
}

static void rmi_f54_free_memory(struct rmi_function_dev *fn_dev)
{
	int reg_num;
	struct rmi_fn_54_data *f54 = fn_dev->data;
	sysfs_remove_group(&fn_dev->dev.kobj, &attrs_query);
	for (reg_num = 0; reg_num < ARRAY_SIZE(attrs_ctrl_regs); reg_num++)
		sysfs_remove_group(&fn_dev->dev.kobj,
				   &attrs_ctrl_regs[reg_num]);
	sysfs_remove_bin_file(&fn_dev->dev.kobj, &dev_rep_data);
	if (f54)
		kfree(f54->report_data);
}

static int rmi_f54_reset(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_54_data *data = fn_dev->data;
	struct rmi_driver *driver = fn_dev->rmi_dev->driver;

#if F54_WATCHDOG
	hrtimer_cancel(&data->watchdog);
#endif

	mutex_lock(&data->status_mutex);
	if (driver->restore_irq_mask) {
		dev_dbg(&fn_dev->dev, "%s: Restoring interupts!\n", __func__);
		driver->restore_irq_mask(fn_dev->rmi_dev);
	} else {
		dev_err(&fn_dev->dev, "No way to restore interrupts!\n");
	}
	data->status = -ECONNRESET;
	mutex_unlock(&data->status_mutex);
	/*
	* for direct touch mode, we need to let the 
	* user space process know that a reset
	* has occurred
	*/
	dev_dbg(&fn_dev->dev, "%s: checking report_type ( = %d ) \n", __func__, data->report_type);

	if (data->report_type == F54_16BIT_UNSIGNED_RAW_IMAGE
	    &&
	    data->report_data
	    &&
	    data->report_size > 80) {

	  strcpy(&data->report_data[data->report_size-80],
		 "resetresetresetreset");
	  data->fresh_or_stale = F54_REPORT_FRESH;
	  data->status = IDLE;
	}


	return 0;
}

static int rmi_f54_remove(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_54_data *data = fn_dev->data;

#if F54_WATCHDOG
	/* Stop timer */
	hrtimer_cancel(&data->watchdog);
#endif

	rmi_f54_free_memory(fn_dev);

	return 0;
}

static int rmi_f54_create_sysfs(struct rmi_function_dev *fn_dev)
{
	int reg_num;
	int retval;
	struct rmi_fn_54_data *f54 = fn_dev->data;
	dev_dbg(&fn_dev->dev, "Creating sysfs files.");
	/* Set up sysfs device attributes. */

	if (sysfs_create_group(&fn_dev->dev.kobj, &attrs_query) < 0) {
		dev_err(&fn_dev->dev, "Failed to create query sysfs files.");
		return -ENODEV;
	}
	for (reg_num = 0; reg_num < ARRAY_SIZE(attrs_ctrl_regs); reg_num++) {
		if (f54->attrs_ctrl_regs_exist[reg_num]) {
			retval = sysfs_create_group(&fn_dev->dev.kobj,
					&attrs_ctrl_regs[reg_num]);
			if (retval < 0) {
				dev_err(&fn_dev->dev, "Failed to create sysfs file group for reg group %d, error = %d.",
							reg_num, retval);
				return -ENODEV;
			}
		}
	}

	/* Binary sysfs file to report the data back */
	retval = sysfs_create_bin_file(&fn_dev->dev.kobj, &dev_rep_data);
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Failed to create sysfs file for F54 data (error = %d).\n",
				retval);
		return -ENODEV;
	}
	return 0;
}



static int rmi_f54_initialize(struct rmi_function_dev *fn_dev)
{
	struct rmi_fn_54_data *f54 = fn_dev->data;
	struct f54_ad_control *control;
	int retval = 0;
	u8 size = 0;
	u16 next_loc;
	u8 reg_num;

#if F54_WATCHDOG
	/* Set up watchdog timer to catch unanswered get_report commands */
	hrtimer_init(&f54->watchdog, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	f54->watchdog.function = clear_status;

	/* work function to do unlocking */
	INIT_WORK(&f54->work, clear_status_worker);
#endif

	/* Read F54 Query Data */
	retval = rmi_read_block(fn_dev->rmi_dev, fn_dev->fd.query_base_addr,
		(u8 *)&f54->query, sizeof(f54->query));
	if (retval < 0) {
		dev_err(&fn_dev->dev, "Could not read query registers from 0x%04x\n",
				fn_dev->fd.query_base_addr);
		return retval;
	}

	/* Initialize the control registers */
	next_loc = fn_dev->fd.control_base_addr;
	reg_num = 0;
	control = &f54->control;

	f54->attrs_ctrl_regs_exist[reg_num] = true;
	reg_num++;
	control->reg_0 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f54_ad_control_0), GFP_KERNEL);
	if (!control->reg_0) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_0->address = next_loc;
	next_loc += sizeof(control->reg_0->regs);

	if (f54->query.touch_controller_family == 0
			|| f54->query.touch_controller_family == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_1 = devm_kzalloc(&fn_dev->dev,
						sizeof(union f54_ad_control_1),
						GFP_KERNEL);
		if (!control->reg_1) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_1->address = next_loc;
		next_loc += sizeof(control->reg_1->regs);
	}
	reg_num++;

	f54->attrs_ctrl_regs_exist[reg_num] = true;
	reg_num++;
	control->reg_2 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f54_ad_control_2), GFP_KERNEL);
	if (!control->reg_2) {
		dev_err(&fn_dev->dev, "Failed to allocate control registers.");
		return -ENOMEM;
	}
	control->reg_2->address = next_loc;
	next_loc += sizeof(control->reg_2->regs);

	if (f54->query.has_pixel_touch_threshold_adjustment == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;

		control->reg_3 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_3),
					GFP_KERNEL);
		if (!control->reg_3) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_3->address = next_loc;
		next_loc += sizeof(control->reg_3->regs);
	}
	reg_num++;

	if (f54->query.touch_controller_family == 0
		|| f54->query.touch_controller_family == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_4__6 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_4__6),
					GFP_KERNEL);
		if (!control->reg_4__6) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_4__6->address = next_loc;
		next_loc += sizeof(control->reg_4__6->regs);
	}
	reg_num++;

	if (f54->query.touch_controller_family == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_7 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_7),
					GFP_KERNEL);
		if (!control->reg_7) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_7->address = next_loc;
		next_loc += sizeof(control->reg_7->regs);
	}
	reg_num++;

	if (f54->query.touch_controller_family == 0
		|| f54->query.touch_controller_family == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_8__9 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f54_ad_control_8__9),
				GFP_KERNEL);
		if (!control->reg_8__9) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_8__9->address = next_loc;
		next_loc += sizeof(control->reg_8__9->regs);
	}
	reg_num++;

	if (f54->query.has_interference_metric == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_10 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_10),
					GFP_KERNEL);
		if (!control->reg_10) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_10->address = next_loc;
		next_loc += sizeof(control->reg_10->regs);
	}
	reg_num++;

	/* F54 Control Register 11 is reserved */
	next_loc++;

	if (f54->query.has_relaxation_control == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_12__13 = devm_kzalloc(&fn_dev->dev,
			sizeof(union f54_ad_control_12__13), GFP_KERNEL);
		if (!control->reg_12__13) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_12__13->address = next_loc;
		next_loc += sizeof(control->reg_12__13->regs);
	}
	reg_num++;

	if (f54->query.has_sensor_assignment == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_14 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_14),
					GFP_KERNEL);
		if (!control->reg_14) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_15 = devm_kzalloc(&fn_dev->dev,
					sizeof(struct f54_ad_control_15),
					GFP_KERNEL);
		if (!control->reg_15) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_15->length = f54->query.num_of_rx_electrodes;
		control->reg_15->regs = devm_kzalloc(&fn_dev->dev,
					control->reg_15->length *
					sizeof(struct f54_ad_control_15n),
					GFP_KERNEL);
		if (!control->reg_15->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_16 = devm_kzalloc(&fn_dev->dev,
					sizeof(struct f54_ad_control_16),
					GFP_KERNEL);
		if (!control->reg_16) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_16->length = f54->query.num_of_tx_electrodes;
		control->reg_16->regs = devm_kzalloc(&fn_dev->dev,
					control->reg_16->length *
					sizeof(struct f54_ad_control_16n),
					GFP_KERNEL);
		if (!control->reg_16->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_14->address = next_loc;
		next_loc += sizeof(control->reg_14->regs);
		control->reg_15->address = next_loc;
		next_loc += control->reg_15->length;
		control->reg_16->address = next_loc;
		next_loc += control->reg_16->length;
	}
	reg_num++;

	if (f54->query.has_sense_frequency_control == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		size = f54->query.number_of_sensing_frequencies;

		control->reg_17 = devm_kzalloc(&fn_dev->dev,
					sizeof(struct f54_ad_control_17),
					GFP_KERNEL);
		if (!control->reg_17) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_17->length = size;
		control->reg_17->regs = devm_kzalloc(&fn_dev->dev,
				size * sizeof(struct f54_ad_control_17n),
				GFP_KERNEL);
		if (!control->reg_17->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_18 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f54_ad_control_18),
				GFP_KERNEL);
		if (!control->reg_18) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_18->length = size;
		control->reg_18->regs = devm_kzalloc(&fn_dev->dev,
				size * sizeof(struct f54_ad_control_18n),
				GFP_KERNEL);
		if (!control->reg_18->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_19 = devm_kzalloc(&fn_dev->dev,
					sizeof(struct f54_ad_control_19),
					GFP_KERNEL);
		if (!control->reg_19) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_19->length = size;
		control->reg_19->regs = devm_kzalloc(&fn_dev->dev,
				size * sizeof(struct f54_ad_control_19n),
				GFP_KERNEL);
		if (!control->reg_19->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_17->address = next_loc;
		next_loc += size;
		control->reg_18->address = next_loc;
		next_loc += size;
		control->reg_19->address = next_loc;
		next_loc += size;
	}
	reg_num++;

	f54->attrs_ctrl_regs_exist[reg_num] = true;
	control->reg_20 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f54_ad_control_20), GFP_KERNEL);
	control->reg_20->address = next_loc;
	next_loc += sizeof(control->reg_20->regs);
	reg_num++;

	if (f54->query.has_sense_frequency_control == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_21 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_21),
					GFP_KERNEL);
		control->reg_21->address = next_loc;
		next_loc += sizeof(control->reg_21->regs);
	}
	reg_num++;

	if (f54->query.has_sense_frequency_control == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_22__26 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f54_ad_control_22__26),
				GFP_KERNEL);
		control->reg_22__26->address = next_loc;
		next_loc += sizeof(control->reg_22__26->regs);
	}
	reg_num++;

	if (f54->query.has_iir_filter == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_27 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_27),
					GFP_KERNEL);
		control->reg_27->address = next_loc;
		next_loc += sizeof(control->reg_27->regs);
	}
	reg_num++;

	if (f54->query.has_firmware_noise_mitigation == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_28 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_28),
					GFP_KERNEL);
		control->reg_28->address = next_loc;
		next_loc += sizeof(control->reg_28->regs);
	}
	reg_num++;

	if (f54->query.has_cmn_removal == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_29 = devm_kzalloc(&fn_dev->dev,
					sizeof(union f54_ad_control_29),
					GFP_KERNEL);
		control->reg_29->address = next_loc;
		next_loc += sizeof(control->reg_29->regs);
	}
	reg_num++;

	if (f54->query.has_cmn_maximum == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_30 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f54_ad_control_30),
				GFP_KERNEL);
		control->reg_30->address = next_loc;
		next_loc += sizeof(control->reg_30->regs);
	}
	reg_num++;

	if (f54->query.has_touch_hysteresis == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_31 = devm_kzalloc(&fn_dev->dev,
				sizeof(union f54_ad_control_31),
				GFP_KERNEL);
		control->reg_31->address = next_loc;
		next_loc += sizeof(control->reg_31->regs);
	}
	reg_num++;

	if (f54->query.has_interference_metric == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_32__35 = devm_kzalloc(&fn_dev->dev,
			sizeof(union f54_ad_control_32__35), GFP_KERNEL);
		control->reg_32__35->address = next_loc;
		next_loc += sizeof(control->reg_32__35->regs);
	}
	reg_num++;

	if (f54->query.curve_compensation_mode == 1) {
		size = max(f54->query.num_of_rx_electrodes,
				f54->query.num_of_tx_electrodes);
	}
	if (f54->query.curve_compensation_mode == 2)
		size = f54->query.num_of_rx_electrodes;
	if (f54->query.curve_compensation_mode == 1
			|| f54->query.curve_compensation_mode == 2) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;

		control->reg_36 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f54_ad_control_36), GFP_KERNEL);
		if (!control->reg_36) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_36->length = size;
		control->reg_36->regs = devm_kzalloc(&fn_dev->dev,
				size * sizeof(struct f54_ad_control_36n),
				GFP_KERNEL);
		if (!control->reg_36->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_36->address = next_loc;
		next_loc += size;
	}
	reg_num++;

	if (f54->query.curve_compensation_mode == 2) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;

		control->reg_37 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f54_ad_control_37), GFP_KERNEL);
		if (!control->reg_37) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_37->length = f54->query.num_of_tx_electrodes;
		control->reg_37->regs = devm_kzalloc(&fn_dev->dev,
				control->reg_37->length*
				sizeof(struct f54_ad_control_37n),
				GFP_KERNEL);
		if (!control->reg_37->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_37->address = next_loc;
		next_loc += control->reg_37->length;
	}
	reg_num++;

	if (f54->query.has_per_frequency_noise_control == 1) {
		f54->attrs_ctrl_regs_exist[reg_num] = true;

		control->reg_38 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f54_ad_control_38), GFP_KERNEL);
		if (!control->reg_38) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_38->length =
				f54->query.number_of_sensing_frequencies;
		control->reg_38->regs = devm_kzalloc(&fn_dev->dev,
			control->reg_38->length*
			sizeof(struct f54_ad_control_38n),
			GFP_KERNEL);
		if (!control->reg_38->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_39 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f54_ad_control_39), GFP_KERNEL);
		if (!control->reg_39) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_39->length =
				f54->query.number_of_sensing_frequencies;
		control->reg_39->regs = devm_kzalloc(&fn_dev->dev,
				control->reg_39->length*
				sizeof(struct f54_ad_control_39n),
				GFP_KERNEL);
		if (!control->reg_39->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_40 = devm_kzalloc(&fn_dev->dev,
				sizeof(struct f54_ad_control_40), GFP_KERNEL);
		if (!control->reg_40) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}
		control->reg_40->length =
				f54->query.number_of_sensing_frequencies;
		control->reg_40->regs = devm_kzalloc(&fn_dev->dev,
			control->reg_40->length*
			sizeof(struct f54_ad_control_40n),
			GFP_KERNEL);
		if (!control->reg_40->regs) {
			dev_err(&fn_dev->dev, "Failed to allocate control registers.");
			return -ENOMEM;
		}

		control->reg_38->address = next_loc;
		next_loc += control->reg_38->length;
		control->reg_39->address = next_loc;
		next_loc += control->reg_39->length;
		control->reg_40->address = next_loc;
		next_loc += control->reg_40->length;
	}
	reg_num++;

	mutex_init(&f54->data_mutex);

	mutex_init(&f54->status_mutex);

	mutex_init(&f54->control_mutex);

	f54->fresh_or_stale = F54_REPORT_STALE;
	
	/* character devices for fast reports */
	raw_data_char_dev_register(f54);
	return retval;
}

static void set_report_size(struct rmi_function_dev *fn_dev)
{
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_device_platform_data *pdata;
	struct rmi_fn_54_data *data = fn_dev->data;
	u8 rx = data->query.num_of_rx_electrodes;
	u8 tx = data->query.num_of_tx_electrodes;
	pdata = to_rmi_platform_data(rmi_dev);
	switch (data->report_type) {
	case F54_8BIT_IMAGE:
		data->report_size = rx * tx;
		break;
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
		data->report_size = 2 * rx * tx;
		break;
	case F54_16BIT_UNSIGNED_RAW_IMAGE:
		/* We assign the report type sized base off of platform data because
		 * it is difficult to know what the size will be, as it can change with
		 * whether or not CDM4 is active or not. */

		data->report_size = pdata->f54_direct_touch_report_size;
		break;
	case F54_HIGH_RESISTANCE:
		data->report_size = RMI_54_HIGH_RESISTANCE_SIZE;
		break;
	case F54_FULL_RAW_CAP_MIN_MAX:
		data->report_size = RMI_54_FULL_RAW_CAP_MIN_MAX_SIZE;
		break;
	case F54_TX_TO_TX_SHORT:
	case F54_TX_OPEN:
	case F54_TX_TO_GROUND:
		data->report_size =  (tx + 7) / 8;
		break;
	case F54_RX_TO_RX1:
	case F54_RX_OPENS1:
		if (rx < tx)
			data->report_size = 2 * rx * rx;
		else
			data->report_size = 2 * rx * tx;
		break;
	case F54_RX_TO_RX2:
	case F54_RX_OPENS2:
		if (rx <= tx)
			data->report_size = 0;
		else
			data->report_size = 2 * rx * (rx - tx);
		break;
	default:
		data->report_size = 0;
	}
}

int rmi_f54_attention(struct rmi_function_dev *fn_dev,
						unsigned long *irq_bits)
{
	struct rmi_driver *driver = fn_dev->rmi_dev->driver;
	u8 fifo[2];
	struct rmi_fn_54_data *data = fn_dev->data;
	int error = 0;
	int retval;
	struct rmi_device *rmi_dev = fn_dev->rmi_dev;
	struct rmi_driver_data *driver_data = dev_get_drvdata(&rmi_dev->dev);
	struct rmi_device_platform_data *pdata;
	int current_block_delay_us;
	int current_read_delay_us;

	set_report_size(fn_dev);
	if (data->report_size == 0) {
		dev_err(&fn_dev->dev, "Invalid report type set in %s. This should never happen.\n",
			__func__);
		error = -EINVAL;
		goto error_exit;
	}
	/*
	 * We need to ensure the buffer is big enough. A Buffer size of 0 means
	 * that the buffer has not been allocated.
	 */
	if (data->bufsize < data->report_size) {
		mutex_lock(&data->data_mutex);
		if (data->bufsize > 0)
			kfree(data->report_data);
		data->report_data = kzalloc(data->report_size, GFP_KERNEL);
		if (!data->report_data) {
			dev_err(&fn_dev->dev, "Failed to allocate report_data.\n");
			error = -ENOMEM;
			data->bufsize = 0;
			mutex_unlock(&data->data_mutex);
			goto error_exit;
		}
		data->bufsize = data->report_size;
		mutex_unlock(&data->data_mutex);
	}
	dev_vdbg(&fn_dev->dev, "F54 Interrupt handler is running.\nSize: %d\n",
		 data->report_size);

	/*
	** store current SPI delays
	 */
	pdata = to_rmi_platform_data(rmi_dev);
	current_block_delay_us = pdata->spi_data.block_delay_us;
	current_read_delay_us  = pdata->spi_data.read_delay_us;

	pdata->spi_data.block_delay_us = 0;
	pdata->spi_data.read_delay_us  = 0;
	rmi_dev->interrupt_restore_block_flag = 1;

	/* Write 0 to fifohi and fifolo. */
	fifo[0] = 0;
	fifo[1] = 0;
	error = rmi_write_block(fn_dev->rmi_dev, fn_dev->fd.data_base_addr
				+ RMI_F54_FIFO_OFFSET, fifo,	sizeof(fifo));
	if (error < 0)
		dev_err(&fn_dev->dev, "Failed to write fifo to zero!\n");
	else
		retval = rmi_read_block(fn_dev->rmi_dev,
			fn_dev->fd.data_base_addr + RMI_F54_REPORT_DATA_OFFSET,
			data->report_data, data->report_size);
	//dev_info(&fn_dev->dev, "%s: read data retval = 0x%x\n", __func__, retval);
	
	/*
	** restore current SPI delays
	*/
	pdata->spi_data.block_delay_us = current_block_delay_us;
	pdata->spi_data.read_delay_us  = current_read_delay_us;

	data->fresh_or_stale = F54_REPORT_FRESH;
	error = IDLE;

	if (retval < 0) {
		dev_err(&fn_dev->dev, "F54 data read failed. Code: %d.\n", error);

		data->fresh_or_stale = F54_REPORT_STALE;
		error = IDLE;
		rmi_dev->interrupt_restore_block_flag = 0;
		goto error_exit;
	}
	// dev_dbg(&fn_dev->dev, "%s: The Report Size is %d",__func__,data->report_size);
	if (retval != data->report_size) {
		dev_dbg(&fn_dev->dev, "report size not correct!! %d != %d",retval, data->report_size);
		error = -EINVAL;
	        data->fresh_or_stale = F54_REPORT_STALE;
		error = IDLE;
		rmi_dev->interrupt_restore_block_flag = 0;

		goto error_exit;
	}
#if RAW_HEX
	int l;
	/* Debugging: Print out the file in hex. */
	pr_info("Report data (raw hex), size: %5d:\n", retval);
	for (l = 0; l <= data->report_size; l += 54) {
		pr_info("%3d - %3d: 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x 0x%02x%02x\n",
			l, l+15,
			data->report_data[l+ 1], data->report_data[l+ 0],
			data->report_data[l+ 3], data->report_data[l+ 2],
			data->report_data[l+ 5], data->report_data[l+ 4],
			data->report_data[l+ 7], data->report_data[l+ 6],
			data->report_data[l+ 9], data->report_data[l+ 8],
			data->report_data[l+11], data->report_data[l+10],
			data->report_data[l+13], data->report_data[l+12],
			data->report_data[l+15], data->report_data[l+14],
			data->report_data[l+17], data->report_data[l+16],
			data->report_data[l+19], data->report_data[l+18],
			data->report_data[l+21], data->report_data[l+20],
			data->report_data[l+23], data->report_data[l+22],
			data->report_data[l+25], data->report_data[l+24],
			data->report_data[l+27], data->report_data[l+26],
			data->report_data[l+29], data->report_data[l+28],
			data->report_data[l+31], data->report_data[l+30],
			data->report_data[l+33], data->report_data[l+32],
			data->report_data[l+35], data->report_data[l+34],
			data->report_data[l+37], data->report_data[l+36],
			data->report_data[l+39], data->report_data[l+38],
			data->report_data[l+41], data->report_data[l+40],
			data->report_data[l+43], data->report_data[l+42],
			data->report_data[l+45], data->report_data[l+44],
			data->report_data[l+47], data->report_data[l+46],
			data->report_data[l+49], data->report_data[l+48],
			data->report_data[l+51], data->report_data[l+50],
			data->report_data[l+53], data->report_data[l+52]
			);
	}


	pr_info("Report data (raw Dec), size: %5d:\n", retval);
	for (l = 0; l <= data->report_size; l += 54) {
		pr_info("%3d - %3d: %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d %6d\n",
			l, l+15,
			(data->report_data[l+ 1]*256)+ data->report_data[l+ 0],
			(data->report_data[l+ 3]*256)+ data->report_data[l+ 2],
			(data->report_data[l+ 5]*256)+ data->report_data[l+ 4],
			(data->report_data[l+ 7]*256)+ data->report_data[l+ 6],
			(data->report_data[l+ 9]*256)+ data->report_data[l+ 8],
			(data->report_data[l+11]*256)+ data->report_data[l+10],
			(data->report_data[l+13]*256)+ data->report_data[l+12],
			(data->report_data[l+15]*256)+ data->report_data[l+14],
			(data->report_data[l+17]*256)+ data->report_data[l+16],
			(data->report_data[l+19]*256)+ data->report_data[l+18],
			(data->report_data[l+21]*256)+ data->report_data[l+20],
			(data->report_data[l+23]*256)+ data->report_data[l+22],
			(data->report_data[l+25]*256)+ data->report_data[l+24],
			(data->report_data[l+27]*256)+ data->report_data[l+26],
			(data->report_data[l+29]*256)+ data->report_data[l+28],
			(data->report_data[l+31]*256)+ data->report_data[l+30],
			(data->report_data[l+33]*256)+ data->report_data[l+32],
			(data->report_data[l+35]*256)+ data->report_data[l+34],
			(data->report_data[l+37]*256)+ data->report_data[l+36],
			(data->report_data[l+39]*256)+ data->report_data[l+38],
			(data->report_data[l+41]*256)+ data->report_data[l+40],
			(data->report_data[l+43]*256)+ data->report_data[l+42],
			(data->report_data[l+45]*256)+ data->report_data[l+44],
			(data->report_data[l+47]*256)+ data->report_data[l+46],
			(data->report_data[l+49]*256)+ data->report_data[l+48],
			(data->report_data[l+51]*256)+ data->report_data[l+50],
			(data->report_data[l+53]*256)+ data->report_data[l+52]
			);
	}


#endif
#if HUMAN_READABLE
	/* Debugging: Print out file in human understandable image */
	switch (data->report_type) {
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
		pr_info("Report data (Image):\n");
		int i, j, k;
		k = 0;
		for (i = 0; i < data->query.num_of_tx_electrodes; i++) {
			for (j = 0; j < data->query.num_of_rx_electrodes; j++) {
				s16 s = (s16) batohs(data->report_data+k);
				if (s < -64)
					printk(".");
				else if (s < 0)
					printk("-");
				else if (s > 64)
					printk("*");
				else if (s > 0)
					printk("+");
				else
					printk("0");
				k += 2;
			}
			pr_info("\n");
		}
		pr_info("EOF\n");
		break;
	default:
		pr_info("Report type %d debug image not supported",
							data->report_type);
	}
#endif
	error = IDLE;
error_exit:
	mutex_lock(&data->status_mutex);
	/* Turn back on other interupts, if it
	 * appears that we turned them off. */
	if (driver->restore_irq_mask) {
		dev_dbg(&fn_dev->dev, "Restoring interupts!\n");
		driver->restore_irq_mask(fn_dev->rmi_dev);
	} else {
		dev_err(&fn_dev->dev, "No way to restore interrupts!\n");
	}
	data->status = error;
	mutex_unlock(&data->status_mutex);
	return data->status;
}


#if F54_WATCHDOG
static void clear_status_worker(struct work_struct *work)
{
	struct rmi_fn_54_data *data = container_of(work,
					struct rmi_fn_54_data, work);
	struct rmi_function_dev *fn_dev = data->fn_dev;
	struct rmi_driver *driver = fn_dev->rmi_dev->driver;
	u8 command;
	int result;

	mutex_lock(&data->status_mutex);
	if (data->status == BUSY) {
		pr_err("F54 Timeout Occured: Determining status.\n");
		result = rmi_read_block(fn_dev->rmi_dev,
					fn_dev->fd.command_base_addr,
								&command, 1);
		if (result < 0) {
			dev_err(&fn_dev->dev, "Could not read get_report register from %#06x.\n",
						fn_dev->fd.command_base_addr);
			data->status = -ETIMEDOUT;
		} else {
			if (command & GET_REPORT) {
				dev_warn(&fn_dev->dev, "Report type unsupported!");
				data->status = -EINVAL;
			} else {
				data->status = -ETIMEDOUT;
			}
		}
		if (driver->restore_irq_mask) {
			dev_dbg(&fn_dev->dev, "%s: Restoring interupts!\n", __func__);
			driver->restore_irq_mask(fn_dev->rmi_dev);
		} else {
			dev_err(&fn_dev->dev, "No way to restore interrupts!\n");
		}
	}
	mutex_unlock(&data->status_mutex);
}

static enum hrtimer_restart clear_status(struct hrtimer *timer)
{
	struct rmi_fn_54_data *data = container_of(timer,
					struct rmi_fn_54_data, watchdog);
	schedule_work(&(data->work));
	return HRTIMER_NORESTART;
}
#endif

/* Check if report_type is valid */
static bool is_report_type_valid(enum f54_report_types reptype)
{
	/* Basic checks on report_type to ensure we write a valid type
	 * to the sensor.
	 * TODO: Check Query3 to see if some specific reports are
	 * available. This is currently listed as a reserved register.
	 */
	switch (reptype) {
	case F54_8BIT_IMAGE:
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_HIGH_RESISTANCE:
	case F54_TX_TO_TX_SHORT:
	case F54_RX_TO_RX1:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP_MIN_MAX:
	case F54_RX_OPENS1:
	case F54_TX_OPEN:
	case F54_TX_TO_GROUND:
	case F54_RX_TO_RX2:
	case F54_RX_OPENS2:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
	case F54_16BIT_UNSIGNED_RAW_IMAGE:
		return true;
		break;
	default:
		return false;
	}
}

/* SYSFS file show/store functions */
static ssize_t rmi_fn_54_report_type_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *f54;

	fn_dev = to_rmi_function_dev(dev);
	f54 = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%u\n", f54->report_type);
}

static ssize_t rmi_fn_54_report_type_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count) {
	int result;
	unsigned long val;
	u8 data;
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *instance_data;
	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;

	/* need to convert the string data to an actual value */
	result = strict_strtoul(buf, 10, &val);
	if (result)
		return result;
	if (!is_report_type_valid(val)) {
		dev_err(dev, "%s : Report type %d is invalid.\n",
					__func__, (u8) val);
		return -EINVAL;
	}
	mutex_lock(&instance_data->status_mutex);
	if (instance_data->status != BUSY) {
		instance_data->report_type = (enum f54_report_types)val;
		data = (u8)val;
		/* Write the Report Type back to the first Block
		 * Data registers (F54_AD_Data0). */
		result = rmi_write_block(fn_dev->rmi_dev,
				fn_dev->fd.data_base_addr, &data, 1);
		mutex_unlock(&instance_data->status_mutex);
		if (result < 0) {
			dev_err(dev, "%s : Could not write report type to 0x%x\n",
				__func__, fn_dev->fd.data_base_addr);
			return result;
		}
		return count;
	} else {
		dev_err(dev, "%s : Report type cannot be changed in the middle of command.\n",
			__func__);
		mutex_unlock(&instance_data->status_mutex);
		return -EINVAL;
	}
}

static ssize_t rmi_fn_54_get_report_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count) {
	unsigned long val;
	int error, result;
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *instance_data;
	struct rmi_driver *driver;
	u8 command;
	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;
	driver = fn_dev->rmi_dev->driver;

	/* need to convert the string data to an actual value */
	error = strict_strtoul(buf, 10, &val);
	if (error)
		return error;
	/* Do nothing if not set to 1. This prevents accidental commands. */
	if (val != 1)
		return count;
	command = GET_REPORT;
	/* Basic checks on report_type to ensure we write a valid type
	 * to the sensor.
	 * TODO: Check Query3 to see if some specific reports are
	 * available. This is currently listed as a reserved register.
	 */
	if (!is_report_type_valid(instance_data->report_type)) {
		dev_err(dev, "%s : Report type %d is invalid.\n",
				__func__, instance_data->report_type);
		return -EINVAL;
	}
	mutex_lock(&instance_data->status_mutex);
	if (instance_data->status != IDLE) {
		if (instance_data->status != BUSY) {
			dev_dbg(dev, "F54 status is in an abnormal state: 0x%x",
							instance_data->status);
		} else {
			dev_info(dev, "F54 status is currently busy: Ignoring request");
		}
		mutex_unlock(&instance_data->status_mutex);
		return count;
	}
	/* Store interrupts */
	/* Do not exit if we fail to turn off interupts. We are likely
	 * to still get useful data. The report data can, however, be
	 * corrupted, and there may be unexpected behavior.
	 */
	dev_dbg(dev, "Storing and overriding interupts\n");
	if (driver->store_irq_mask)
		driver->store_irq_mask(fn_dev->rmi_dev,
					fn_dev->irq_mask);
	else
		dev_err(dev, "No way to store interupts!\n");
	instance_data->status = BUSY;

	/* small delay to avoid race condition in firmare. This value is a bit
	 * higher than absolutely necessary. Should be removed once issue is
	 * resolved in firmware. */

	mdelay(1);

	dev_dbg(dev, "%s: actually writing to RMI\n", __func__);


	/* Write the command to the command register */
	result = rmi_write_block(fn_dev->rmi_dev, fn_dev->fd.command_base_addr,
						&command, 1);

	/* Mark the current data buffer stale -- we requested a new one */
	instance_data->fresh_or_stale = F54_REPORT_STALE;


	mutex_unlock(&instance_data->status_mutex);
	if (result < 0) {
		dev_err(dev, "%s : Could not write command to 0x%x\n",
				__func__, fn_dev->fd.command_base_addr);
		return result;
	}
#if F54_WATCHDOG
	/* start watchdog timer */
	hrtimer_start(&instance_data->watchdog, ktime_set(4, 0),
							HRTIMER_MODE_REL);
#endif
	return count;
}

static ssize_t rmi_fn_54_force_cal_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count) {
	unsigned long val;
	int error, result;
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *instance_data;
	struct rmi_driver *driver;
	u8 command;

	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;
	driver = fn_dev->rmi_dev->driver;

	/* need to convert the string data to an actual value */
	error = strict_strtoul(buf, 10, &val);
	if (error)
		return error;
	/* Do nothing if not set to 1. This prevents accidental commands. */
	if (val != 1)
		return count;

	command = FORCE_CAL;

	if (instance_data->status == BUSY)
		return -EBUSY;
	/* Write the command to the command register */
	result = rmi_write_block(fn_dev->rmi_dev, fn_dev->fd.command_base_addr,
						&command, 1);
	if (result < 0) {
		dev_err(dev, "%s : Could not write command to 0x%x\n",
				__func__, fn_dev->fd.command_base_addr);
		return result;
	}
	return count;
}

static ssize_t rmi_fn_54_status_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *instance_data;

	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%d\n", instance_data->status);
}

simple_show_union_struct_unsigned(query, num_of_rx_electrodes)
simple_show_union_struct_unsigned(query, num_of_tx_electrodes)
simple_show_union_struct_unsigned(query, has_image16)
simple_show_union_struct_unsigned(query, has_image8)
simple_show_union_struct_unsigned(query, has_baseline)
simple_show_union_struct_unsigned(query, clock_rate)
simple_show_union_struct_unsigned(query, touch_controller_family)
simple_show_union_struct_unsigned(query, has_pixel_touch_threshold_adjustment)
simple_show_union_struct_unsigned(query, has_sensor_assignment)
simple_show_union_struct_unsigned(query, has_interference_metric)
simple_show_union_struct_unsigned(query, has_sense_frequency_control)
simple_show_union_struct_unsigned(query, has_firmware_noise_mitigation)
simple_show_union_struct_unsigned(query, has_two_byte_report_rate)
simple_show_union_struct_unsigned(query, has_one_byte_report_rate)
simple_show_union_struct_unsigned(query, has_relaxation_control)
simple_show_union_struct_unsigned(query, curve_compensation_mode)
simple_show_union_struct_unsigned(query, has_iir_filter)
simple_show_union_struct_unsigned(query, has_cmn_removal)
simple_show_union_struct_unsigned(query, has_cmn_maximum)
simple_show_union_struct_unsigned(query, has_touch_hysteresis)
simple_show_union_struct_unsigned(query, has_edge_compensation)
simple_show_union_struct_unsigned(query, has_per_frequency_noise_control)
simple_show_union_struct_unsigned(query, number_of_sensing_frequencies)

static ssize_t rmi_fn_54_no_auto_cal_show(struct device *dev,
				struct device_attribute *attr, char *buf) {
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *data;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;

	return snprintf(buf, PAGE_SIZE, "%u\n",
				data->no_auto_cal ? 1 : 0);
}

static ssize_t rmi_fn_54_no_auto_cal_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count) {
	int result;
	unsigned long val;
	u8 data;
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *instance_data;

	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;

	/* need to convert the string data to an actual value */
	result = strict_strtoul(buf, 10, &val);

	/* if an error occured, return it */
	if (result)
		return result;
	/* Do nothing if not 0 or 1. This prevents accidental commands. */
	if (val > 1)
		return -EINVAL;
	/* Read current control values */
	result = rmi_read_block(fn_dev->rmi_dev, fn_dev->fd.control_base_addr,
			   &data, 1);
	if (result < 0) {
		dev_err(dev, "%s : Could not read control base address to 0x%x\n",
		       __func__, fn_dev->fd.control_base_addr);
		return result;
	}

	/* if the current control registers are already set as we want them, do
	 * nothing to them */
	if ((data & NO_AUTO_CAL_MASK) == val)
		return count;
	/* Write the control back to the control register (F54_AD_Ctrl0)
	 * Ignores everything but bit 0 */
	data = (data & ~NO_AUTO_CAL_MASK) | (val & NO_AUTO_CAL_MASK);
	result = rmi_write_block(fn_dev->rmi_dev, fn_dev->fd.control_base_addr,
				 &data, 1);
	if (result < 0) {
		dev_err(dev, "%s : Could not write control to 0x%x\n",
		       __func__, fn_dev->fd.control_base_addr);
		return result;
	}
	/* update our internal representation iff the write succeeds */
	instance_data->no_auto_cal = (val == 1);
	return count;
}

static ssize_t rmi_fn_54_fifoindex_show(struct device *dev,
				  struct device_attribute *attr, char *buf) {
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *instance_data;
	struct rmi_driver *driver;
	u8 temp_buf[2];
	int retval;

	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;
	driver = fn_dev->rmi_dev->driver;

	/* Read fifoindex from device */
	retval = rmi_read_block(fn_dev->rmi_dev,
				fn_dev->fd.data_base_addr + RMI_F54_FIFO_OFFSET,
				temp_buf, ARRAY_SIZE(temp_buf));

	if (retval < 0) {
		dev_err(dev, "Could not read fifoindex from 0x%04x\n",
		       fn_dev->fd.data_base_addr + RMI_F54_FIFO_OFFSET);
		return retval;
	}
	instance_data->fifoindex = batohs(temp_buf);
	return snprintf(buf, PAGE_SIZE, "%u\n", instance_data->fifoindex);
}
static ssize_t rmi_fn_54_fifoindex_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	int error;
	unsigned long val;
	u8 data[2];
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *instance_data;

	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;

	/* need to convert the string data to an actual value */
	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	instance_data->fifoindex = val;

	/* Write the FifoIndex back to the first data registers. */
	hstoba(data, (u16)val);

	error = rmi_write_block(fn_dev->rmi_dev,
				fn_dev->fd.data_base_addr + RMI_F54_FIFO_OFFSET,
				data, ARRAY_SIZE(data));

	if (error < 0) {
		dev_err(dev, "Could not write fifoindex to 0x%x\n",
		       fn_dev->fd.data_base_addr + RMI_F54_FIFO_OFFSET);
		return error;
	}
	return count;
}

/* Provide access to last report */
static ssize_t rmi_fn_54_data_read(struct file *data_file, struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count)
{
	struct device *dev;
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *instance_data;

	struct device *parent_dev;
	struct rmi_device *rmi_dev;
	struct rmi_device *parent_rmi_dev;
	struct rmi_driver *parent_rmi_drvr;
	struct rmi_driver_data *data;

	int i;
	// int error;

	dev = container_of(kobj, struct device, kobj);
	fn_dev = to_rmi_function_dev(dev);
	instance_data = fn_dev->data;
	mutex_lock(&instance_data->data_mutex);
	if (count < instance_data->report_size) {
		dev_err(dev,  "F54 report size too large for buffer: %d. Need at least: %d for Report type: %d.\n",
				count, instance_data->report_size,
			instance_data->report_type);
		mutex_unlock(&instance_data->data_mutex);
		return -EINVAL;
	}

	for (i=0; i< RMI_F54_WATCHDOG_TIMEOUT_MSEC; i += RMI_F54_STEP_MSEC) {

	  if (instance_data->report_data
	      &&
	      (instance_data->fresh_or_stale == F54_REPORT_FRESH)
	      ) {
	    /* Copy data from instance_data to buffer */

	    memcpy(buf, instance_data->report_data,
	  	   instance_data->report_size);

	    instance_data->fresh_or_stale = F54_REPORT_STALE;

	    mutex_unlock(&instance_data->data_mutex);

	    dev_dbg(dev, "%s: Presumably successful.", __func__);

	    return instance_data->report_size;
	  }

	  mutex_unlock(&instance_data->data_mutex);
	  msleep(RMI_F54_STEP_MSEC);

	} /* bottom of the step-wait-step loop */
	
	if (instance_data->report_data) {
		/* Copy data from instance_data to buffer */
		memcpy(buf, instance_data->report_data,
					instance_data->report_size);
		mutex_unlock(&instance_data->data_mutex);
		dev_dbg(dev, "%s: Presumably successful.", __func__);
		return instance_data->report_size;
	} else {
		dev_err(dev, "%s: F54 report_data does not exist!\n", __func__);
		mutex_unlock(&instance_data->data_mutex);
		return -EINVAL;
	}
}

/* Repeated Register sysfs functions */
show_repeated_union_struct_unsigned(control, reg_15, sensor_rx_assignment)
show_repeated_union_struct_unsigned(control, reg_16, sensor_tx_assignment)

show_repeated_union_struct_unsigned(control, reg_17, filter_bandwidth)
show_repeated_union_struct_unsigned(control, reg_17, disable)


static ssize_t rmi_fn_54_burst_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf) {
	struct rmi_function_dev *fn_dev;
	struct rmi_fn_54_data *data;
	int result, size = 0;
	char *temp;
	int i;

	fn_dev = to_rmi_function_dev(dev);
	data = fn_dev->data;
	mutex_lock(&data->control_mutex);
	/* Read current control values */
	result = rmi_read_block(fn_dev->rmi_dev, data->control.reg_17->address,
			(u8 *) data->control.reg_17->regs,
			data->control.reg_17->length * sizeof(u8));
	if (result < 0) {
		dev_err(dev, "Could not read control at %#06x Data may be outdated.",
					data->control.reg_17->address);
	}

	result = rmi_read_block(fn_dev->rmi_dev, data->control.reg_18->address,
			(u8 *)data->control.reg_18->regs,
			data->control.reg_18->length * sizeof(u8));
	if (result < 0) {
		dev_err(dev, "Could not read control at %#06x. Data may be outdated.",
					data->control.reg_18->address);
	}
	mutex_unlock(&data->control_mutex);
	temp = buf;
	for (i = 0; i < data->control.reg_17->length; i++) {
		result = snprintf(temp, PAGE_SIZE - size, "%u ", (1<<8) *
			data->control.reg_17->regs[i].burst_countb10__8 +
			data->control.reg_18->regs[i].burst_countb7__0n);
		size += result;
		temp += result;
	}
	return size + snprintf(temp, PAGE_SIZE - size, "\n");
}

show_repeated_union_struct_unsigned(control, reg_19, stretch_duration)
show_store_repeated_union_struct_unsigned(control, reg_36, axis1_comp)
show_store_repeated_union_struct_unsigned(control, reg_37, axis2_comp)

show_repeated_union_struct_unsigned(control, reg_38, noise_control_1)
show_repeated_union_struct_unsigned(control, reg_39, noise_control_2)
show_repeated_union_struct_unsigned(control, reg_40, noise_control_3)

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

show_store_union_struct_unsigned(control, reg_0, no_relax)
show_store_union_struct_unsigned(control, reg_0, no_scan)
show_store_union_struct_unsigned(control, reg_1, bursts_per_cluster)
show_store_union_struct_unsigned(control, reg_2, saturation_cap)
show_store_union_struct_unsigned(control, reg_3, pixel_touch_threshold)
show_store_union_struct_unsigned(control, reg_4__6, rx_feedback_cap)
show_store_union_struct_unsigned(control, reg_4__6, low_ref_cap)
show_store_union_struct_unsigned(control, reg_4__6, low_ref_feedback_cap)
show_store_union_struct_unsigned(control, reg_4__6, low_ref_polarity)
show_store_union_struct_unsigned(control, reg_4__6, high_ref_cap)
show_store_union_struct_unsigned(control, reg_4__6, high_ref_feedback_cap)
show_store_union_struct_unsigned(control, reg_4__6, high_ref_polarity)
show_store_union_struct_unsigned(control, reg_7, cbc_cap)
show_store_union_struct_unsigned(control, reg_7, cbc_polarity)
show_store_union_struct_unsigned(control, reg_7, cbc_tx_carrier_selection)
show_store_union_struct_unsigned(control, reg_8__9, integration_duration)
show_store_union_struct_unsigned(control, reg_8__9, reset_duration)
show_store_union_struct_unsigned(control, reg_10,
						noise_sensing_bursts_per_image)
show_store_union_struct_unsigned(control, reg_12__13, slow_relaxation_rate)
show_store_union_struct_unsigned(control, reg_12__13, fast_relaxation_rate)
show_store_union_struct_unsigned(control, reg_14, rxs_on_xaxis)
show_store_union_struct_unsigned(control, reg_14, curve_comp_on_txs)
show_store_union_struct_unsigned(control, reg_20, disable_noise_mitigation)
show_store_union_struct_unsigned(control, reg_21, freq_shift_noise_threshold)
show_store_union_struct_unsigned(control, reg_22__26, medium_noise_threshold)
show_store_union_struct_unsigned(control, reg_22__26, high_noise_threshold)
show_store_union_struct_unsigned(control, reg_22__26, noise_density)
show_store_union_struct_unsigned(control, reg_22__26, frame_count)
show_store_union_struct_unsigned(control, reg_27, iir_filter_coef)
show_store_union_struct_unsigned(control, reg_28, quiet_threshold)
show_store_union_struct_unsigned(control, reg_29, cmn_filter_disable)
show_store_union_struct_unsigned(control, reg_30, cmn_filter_max)
show_store_union_struct_unsigned(control, reg_31, touch_hysteresis)
show_store_union_struct_unsigned(control, reg_32__35, rx_low_edge_comp)
show_store_union_struct_unsigned(control, reg_32__35, rx_high_edge_comp)
show_store_union_struct_unsigned(control, reg_32__35, tx_low_edge_comp)
show_store_union_struct_unsigned(control, reg_32__35, tx_high_edge_comp)

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

static struct rmi_function_driver function_driver = {
	.driver = {
		.name = "rmi_f54",
	},
	.func = FUNCTION_NUMBER,
	.probe = rmi_f54_probe,
	.remove = rmi_f54_remove,
	.reset = rmi_f54_reset,
	.attention = rmi_f54_attention,
};

module_rmi_function_driver(function_driver);

MODULE_AUTHOR("Daniel Rosenberg <daniel.rosenberg@synaptics.com>");
MODULE_DESCRIPTION("RMI F54 module");
MODULE_LICENSE("GPL");
MODULE_VERSION(RMI_DRIVER_VERSION);

/*
** Direct-Touch control files block diagram
**
**
**
**             -------------------------------
**             |                             |
**             |                             |
**             |                             |
**             |       Touch Sensor          |
**             |          Plate              |
**             |                             |
**             |                             |
**             |                             |
**             ---------------||--------------
**                            ||
**                            ||
**                            ||
**             ---------------||--------------
**             |                             |
**             |                             |
**             |                             |
**             |       Synaptics 1321        |
**             |         or 1322             |
**             |                             |
**             |                             |
**             |                             |
**             ---------------||--------------
**                            ||
**                            ||
**                            ||
**   -------------------------||--------------------------
**   |           fn11/mode   / \\  fn54/get_report == 0  |
**   |            == 0      /   \\ Do Nothing            |
**   |           then       |   ||                       |
**   |           read       |   || fn54/get_report == 1  |
**   |           interrupts |   || Get Image             |
**   |                      |   || (resets to 0          |
**   |            == 1      |   ||  after 1 image)       |
**   |           then       |   ||                       |
**   |           read       |   ||                       |
**   |           from       |   ||                       |
**   |           /dev/      |   ||                       |
**   |           rawtouch00 |   ||                       |
**   |           (for       |   ||                       |
**   |            finger   /    |\                       |
**   |            data)   /      \\                      |
**   |                   |        \\                     |
**   |  fn11/mode == 1   |        ||                     |
**   |  accept finger    |        ||                     |
**   |  events From      |        ||                     |
**   |  Direct Touch     |        ||                     |
**   |  Daemon           |        ||                     |
**   |  (*)              |        ||                     |
**   |               fn11/mode    ||                     |
**   |                  =?= 0     ||                     |
**   |                   |        ||                     |
**   |                   |        ||                     |
**   |                  \ /      \  /                    |
**   |             ------|--     -\/------               |
**   |             | FN11  |     | FN54  |               |
**   |             |  \ /  |     |       |               |
**   |        /=====   X   |     |       |               |
**   |       /|    |  / \  |     |       |               |
**   |       EE    |       |     |       |               |
**   |       EE    ----/\---     --||-----               |
**   |       EE       /  \         ||                    |
**   |       EE        ||          ||                    |
**   |       EE     fn11/mode      ||                    |
**   |       EE       =?= 1        ||                    |
**   |       EE        ||          ||                    |
**   |       EE        ||          ||                    |
**   |       EE        ||          ||                    |
**   |       EE        ||          ||                    |
**   |       EE        ||          ||                    |
**   |       EE        ||          || make image         |
**   |       EE        ||          || available in       |
**   |       EE     read from      || /dev/rawsensor00   |
**   |       EE     /dev/rawtouch00|| (fast)             |
**   |       EE        ||          ||                    |
**   |       EE        ||          ||  (**)              |
**   |       EE        ||          ||                    |
**   |       EE        ||         \  /                   |
**   |       EE    ____||__________\/____                |
**   |       EE    |                     |               |
**   |       EE    |                     |               |
**   |       EE    |                     |               |
**   |       EE    |                     |               |
**   |       EE    |     SYNAPTICS       |               |
**   |       EE    |      DIRECT         |               |
**   |       EE    |       TOUCH         |               |
**   |       EE    |     Executable      |               |
**   |       EE    |                     |               |
**   |       EE    |                     |               |
**   |       EE    ______________________                |
**   |       EE                                          |
**   |      \  /                                         |
**   |    ---\/----------------------------------        |
**   |    |         ANDROID INPUT STREAM        |        |
**   |    ---------------------------------------        |
**   |                                                   |
**   |                                                   |
**   |                 ANDROID HOST                      |
**   |                                                   |
**   |                                                   |
**   |                                                   |
**   |                                                   |
**   -----------------------------------------------------
**
**   (*) fn11/mode file:controls whether or not fn11 finger
**       reports come from the
**       sensor chip (==0)
**       or, Direct_touch executable (==1)
**       use
**       cat /sys/devices/sensor00/fn11/mode
**       to see current value
**
**       use
**       echo 0 > /sys/devices/sensor00/fn11/mode
**       to set finger report source to the sensor chip
**
**       echo 1 > /sys/devices/sensor00/fn11/mode
**       to set finger report source to direct touch daemon
**
**
**
**
**    (**) fn54/user_data file: controls whether images are transferred from
**         the driver to the Direct touch executable through
**         /sys/devices/sensor00/fn54/rep_data or
**         /dev/rawsensor00
**         files.
**         /dev/rawsensor00 is faster, but uses more platform-specific stuff
**
**         cat /sys/devices/sensor00/fn54/user_data shows current value
**
**         echo 0x0 > /sys/devices/sensor00/fn54/user_data
**         sets transfer source file to /sys/devices/sensor00/fn54/rep_data
**
**         echo 0x1 > /sys/devices/sensor00/fn54/user_data
**         sets transfer source file to /dev/rawsensor00
*/


static ssize_t SynSens_char_dev_read(struct file *, const char __user *,
				     size_t, loff_t *);
static ssize_t SynSens_char_dev_write(struct file *, const char __user *,
				      size_t, loff_t *);
static int     SynSens_char_dev_open(struct inode *, struct file *);
static int     SynSens_char_dev_release(struct inode *, struct file *);


static const struct file_operations SynSens_char_dev_fops = {
	.owner =    THIS_MODULE,
	.read =     SynSens_char_dev_read,
	.write =    SynSens_char_dev_write,
	.open =     SynSens_char_dev_open,
	.release =  SynSens_char_dev_release,
};

/*
 * SynSens_char_devnode - return device permission
 *
 * @dev: char device structure
 * @mode: file permission
 *
 */
static char *SynSens_char_devnode(struct device *dev, mode_t *mode)
{
	if (!mode)
		return NULL;
	/* rmi** */
	/**mode = 0666*/
	*mode = (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	dev_dbg(dev, "%s: setting mode of %s to 0x%08x\n", __func__,
		RAW_IMAGE_CHAR_DEVICE_NAME, *mode);
	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}



/*store dynamically allocated major number of char device*/
static int rmi_char_dev_major_num;


/*
 * raw_data_char_dev_register - register char device
 * called from init
 *
 * @phy: a pointer to an rmi_phys_devices structure
 *
 * @return: zero if suceeds
 */
static int
raw_data_char_dev_register(struct rmi_fn_54_data *rmi_f54_instance_data)
{
	dev_t dev_no;
	int err;
	int result;
	struct device *device_ptr;
	struct raw_data_char_dev *char_dev;

	if (!rmi_f54_instance_data) {
		dev_err(&rmi_f54_instance_data->fn_dev->dev,"%s: No RMI F54 data structure instance\n",
			__func__);
	}

	if (rmi_char_dev_major_num) {
		dev_no = MKDEV(rmi_char_dev_major_num, 0);
		result = register_chrdev_region(dev_no, 1,
						RAW_IMAGE_CHAR_DEVICE_NAME);
	} else {
		result = alloc_chrdev_region(&dev_no, 0, 1,
					     RAW_IMAGE_CHAR_DEVICE_NAME);
		/* let kernel allocate a major for us */
		rmi_char_dev_major_num = MAJOR(dev_no);
	}
	pr_info("%s: Major number of rmi_char_dev: %d\n",
		__func__,  rmi_char_dev_major_num);

	if (result < 0)
		return result;

	/* allocate device space */
	char_dev = kzalloc(sizeof(struct raw_data_char_dev), GFP_KERNEL);
	if (!char_dev) {
		dev_err(&rmi_f54_instance_data->fn_dev->dev,
				"%s: Failed to allocate rmi_char_dev.\n", __func__);
		/* unregister the char device region */
		__unregister_chrdev(rmi_char_dev_major_num, MINOR(dev_no), 1,
				    RAW_IMAGE_CHAR_DEVICE_NAME);
		return -ENOMEM;
	}

	mutex_init(&char_dev->mutex_file_op);

	rmi_f54_instance_data->raw_data_feed = char_dev;


	/* initialize the device */
	cdev_init(&char_dev->raw_data_dev, &SynSens_char_dev_fops);



	char_dev->raw_data_dev.owner = THIS_MODULE;


	/* tell the linux kernel to add the device */
	err = cdev_add(&char_dev->raw_data_dev, dev_no, 1);

	dev_dbg(&rmi_f54_instance_data->fn_dev->dev, "%s: cdev_add returned with code= %d\n", __func__, err);

	if (err) {
		dev_err(&rmi_f54_instance_data->fn_dev->dev,"%s: Error %d adding raw_data_char_dev.\n",
			__func__, err);
		return err;
	}

	/* create device node */
	rmi_f54_instance_data->raw_data_feed->raw_data_device_class =
		class_create(THIS_MODULE, RAW_IMAGE_CHAR_DEVICE_NAME);

	if (IS_ERR(rmi_f54_instance_data->raw_data_feed->
		       raw_data_device_class)) {

		dev_err(&rmi_f54_instance_data->fn_dev->dev, "%s: Failed to create /dev/%s.\n",
				__func__, RAW_IMAGE_CHAR_DEVICE_NAME);

		return -ENODEV;
	}

	/* setup permission */
	rmi_f54_instance_data->raw_data_feed->raw_data_device_class->devnode =
		SynSens_char_devnode;

	/* class creation */
	device_ptr = device_create(
		rmi_f54_instance_data->raw_data_feed->
		raw_data_device_class,
		NULL, dev_no, NULL,
		RAW_IMAGE_CHAR_DEVICE_NAME"%d",
		MINOR(dev_no));

	if (IS_ERR(device_ptr)) {
	  dev_err((const struct device *)&rmi_f54_instance_data->raw_data_feed->raw_data_dev.dev,
			"Failed to create raw_data_read device.\n");

		return -ENODEV;
	}

	rmi_f54_instance_data->raw_data_feed->my_parents_instance_data =
		rmi_f54_instance_data;

	return 0;
}

/* file operations for SynSens char device */

/* unsigned char junk_char_buf[2048]; */

static ssize_t SynSens_char_dev_read(struct file *filp, const char __user *buf,
				     size_t count, loff_t *f_pos)
{
	struct rmi_fn_54_data *my_instance_data = NULL;
	struct raw_data_char_dev *char_dev_container = NULL;
	ssize_t ret_value  = 0;

	int min_usecs = RMI_F54_STEP_MSEC * 1000 / 4;
	int max_usecs = RMI_F54_STEP_MSEC * 1000 / 2;
	unsigned char *buffer_to_copy_to_output;
	long int curr_sleep = 0;
	u8 command = 1;

	if (!filp) {
		pr_info("%s: called with NULL file pointer\n", __func__);
		return -EINVAL;
	}
	char_dev_container = filp->private_data;

	if (!char_dev_container) {
		pr_info("%s: called with NULL private_data\n", __func__);
		return -EINVAL;
	}

	my_instance_data = char_dev_container->my_parents_instance_data;

	if (count == 0) {
		pr_info("%s: count = %d -- no space to copy output to!!!\n",
			__func__, count);
		return -ENOMEM;
	}

	if (count < my_instance_data->report_size) {
		pr_info("%s: count = %d but need %d bytes -- not enough space\n",
			__func__, count, my_instance_data->report_size);
		return -ENOMEM;
	}
	
	/* get the next report, unless special report type */
	if (my_instance_data->report_type != F54_16BIT_UNSIGNED_RAW_IMAGE) {
		/* Write the command to the command register */
		pr_info("%s: Get data report\n", __func__);
		ret_value = rmi_write_block(my_instance_data->fn_dev->rmi_dev, 
					my_instance_data->fn_dev->fd.command_base_addr,
					&command, 1);
		if (ret_value < 0) {
			pr_info("%s : Could not write command to 0x%x\n",
					__func__, my_instance_data->fn_dev->fd.command_base_addr);
			return ret_value;
		}
		/* Mark the current data buffer stale -- we requested a new one */
		my_instance_data->fresh_or_stale = F54_REPORT_STALE;
	}

	/*
	** if the data is not ready, wait until it is...
	*/

	while (((!my_instance_data->report_data)
		||
		(my_instance_data->fresh_or_stale != F54_REPORT_FRESH)
		) 
	       && 
	       (curr_sleep < 3000000)) { /* 3 seconds is below the threshold of kernel watchdog */
	        /*
		** We stay in this loop as long as:
		** there is no fresh report   (the current one is already read)
		** or, there is no report at all
		** but, we leave the loop if the
		** total wait time is 3 seconds
		** so that we do not trigger a watchdog timeout
		** in this case, we send the stale frame over
		** again, no real harm done.
		*/

		usleep_range(min_usecs, max_usecs);

		curr_sleep += (max_usecs + min_usecs)/2;

		if (curr_sleep > RMI_F54_WATCHDOG_TIMEOUT_MSEC*1000) {

			/*
			** no -EFAULT here,
			** just 0 stating that 0 bytes were copied
			** not even return anymore -- low_power mode requires we
			** wait until the chip wakes up
			*/
			/* return 0; */
		        /*
			**  if we are here, it means that the chip
			** went into doze mode. Just increase the sleep 
			** duration so that we do not wake up the
			** reading process (direct touch daemon)
			** too often -- that would cost cpu cycles
			** and battery power
			*/
		        min_usecs = 30000;
		        max_usecs = 40000;
		}
	}
	/*
	** pr_info("   wait over\n");
	*/
	/*
	** decide whether we need to send image buffer or config buffer
	*/
	buffer_to_copy_to_output = my_instance_data->report_data;

	/*
	** mutex_lock(&(my_instance_data->raw_data_feed->mutex_file_op));
	*/
	if (curr_sleep >= 3000000) {
		pr_debug("#");
	}

	ret_value = copy_to_user((void __user *)buf,
				 (const void *)buffer_to_copy_to_output,
				 my_instance_data->report_size);

	*f_pos += my_instance_data->report_size;

	my_instance_data->fresh_or_stale = F54_REPORT_STALE;
	memset(my_instance_data->report_data,
	       '\0',
	       my_instance_data->report_size);
	/*
	** mutex_unlock(&(my_instance_data->raw_data_feed->mutex_file_op));
	*/

	/*
	 * release the wake_lock.  the DT daemon will know what to do.
	 */
	return my_instance_data->report_size - ret_value;
}

#define F54_REPORT_STOP		0
#define F54_REPORT_START	1
#define F54_REPORT_SET_TYPE	2

/*
 * SynSens_char_dev_write: - use to write data into RMI stream
 * First byte is indication of parameter to change
 *
 * @filep : file structure for write
 * @buf: user-level buffer pointer contains data to be written
 * @count: number of byte be be written
 * @f_pos: offset (starting register address)
 *
 * @return number of bytes written from user buffer (buf) if succeeds
 *         negative number if error occurs.
 */
static ssize_t SynSens_char_dev_write(struct file *filp, const char __user *buf,
				      size_t count, loff_t *f_pos)
{
	struct rmi_fn_54_data *f54 = NULL;
	struct raw_data_char_dev *char_dev_container = NULL;
	struct rmi_function_dev *fn_dev;
	struct rmi_driver *driver;
	struct rmi_driver_data *driver_data;
	unsigned char tmpbuf[128];
	char command;
	u8 val;
	int retval = 0;
	pr_info("%s: Write called.\n", __func__);
	if (!filp) {
		dev_err(&fn_dev->dev, "%s: called with NULL file pointer\n", __func__);
		return -EINVAL;
	}
	char_dev_container = filp->private_data;

	if (!char_dev_container) {
		dev_err(&fn_dev->dev, "%s: called with NULL private_data\n", __func__);
		return -EINVAL;
	}
	f54 = char_dev_container->my_parents_instance_data;
	fn_dev = f54->fn_dev;
	driver = fn_dev->rmi_dev->driver;
	driver_data = dev_get_drvdata(&fn_dev->rmi_dev->dev);
	memset(tmpbuf, '\0', 128);
	pr_info("%s: count = %d tmpbuf size = %d\n", __func__, count, 128);
	retval = copy_from_user(tmpbuf, buf, count > 128 ? 128 : count);
	command = tmpbuf[0];
	switch(command) {
	case F54_REPORT_SET_TYPE:
		val = (u8)tmpbuf[1];
		if (!is_report_type_valid(val)) {
			dev_err(&fn_dev->dev, "%s : Report type %d is invalid.\n",
						__func__, val);
			return -EINVAL;
		}
		mutex_lock(&f54->status_mutex);
		if (f54->status != BUSY) {
			pr_info("%s:report type: %d\n", __func__,val);
			f54->report_type = (enum f54_report_types)val;
			/* Write the Report Type back to the first Block
			 * Data registers (F54_AD_Data0). */
			 
			retval = rmi_write_block(fn_dev->rmi_dev,
					fn_dev->fd.data_base_addr, &val, 1);
			mutex_unlock(&f54->status_mutex);
			if (retval < 0) {
				dev_err(&fn_dev->dev, "%s : Could not write report type to 0x%x\n",
					__func__, fn_dev->fd.data_base_addr);
				return retval;
			}
			return count;
		} else {
			dev_err(&fn_dev->dev, "%s : Report type cannot be changed in the middle of command.\n",
				__func__);
			mutex_unlock(&f54->status_mutex);
			return -EINVAL;
		}
	case F54_REPORT_START:
		/* Overwrite and store interrupts */
		if (driver->store_irq_mask)
			driver->store_irq_mask(fn_dev->rmi_dev, fn_dev->irq_mask);
	case F54_REPORT_STOP:
		/* Turn back on other interupts, if it
		 * appears that we turned them off. */
		if (driver_data->irq_stored && driver->restore_irq_mask) {
			dev_dbg(&fn_dev->dev, "%s: Restoring interupts!\n", __func__);
			driver->restore_irq_mask(fn_dev->rmi_dev);
		}
	}
	return count;
}


/*
 * SynSens_char_dev_open: - get a new handle for reading raw Touch Sensor images
 * @inp : inode struture
 * @filp: file structure for read/write
 *
 * @return 0 if succeeds
 */
static int SynSens_char_dev_open(struct inode *inp, struct file *filp)
{
	int ret_value = 0;
	/* store the device pointer to file structure */

	struct raw_data_char_dev *my_dev ;

	pr_info("%s: rmi: synaptics: user space app opened the character device\n", __func__);

	my_dev = container_of(inp->i_cdev,
			      struct raw_data_char_dev,
			      raw_data_dev);


	filp->private_data = my_dev;

	return ret_value;
}

/*
 *  SynSens_char_dev_release: - release an existing handle
 *  @inp: inode structure
 *  @filp: file structure for read/write
 *
 *  @return 0 if succeeds
 */
static int SynSens_char_dev_release(struct inode *inp, struct file *filp)
{
	return 0;
}


/*
 * SynSens_char_dev_clean_up - release memory or unregister driver
 * @SynSens_char_dev: SynSens_char_dev structure
 *
 */
static void SynSens_char_dev_clean_up(struct raw_data_char_dev *char_dev,
				      struct class *char_device_class)
{
	int junk; /*
		  ** placeholder for later
		  */
	junk = 0;
	return;
}



/* SynSens_char_dev_unregister - unregister char device (called from up-level)
 *
 * @phys: pointer to an rmi_phys_device structure
 */

void SynSens_char_dev_unregister(struct raw_data_char_dev *raw_char_dev)
{
	/* clean up */
	if (raw_char_dev)
		SynSens_char_dev_clean_up(raw_char_dev,
					  raw_char_dev->raw_data_device_class);
}
EXPORT_SYMBOL(SynSens_char_dev_unregister);



MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 Char Device");
MODULE_LICENSE("GPL");

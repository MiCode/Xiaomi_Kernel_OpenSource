/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_SOC_TEMP_LVTS_H__
#define __MTK_SOC_TEMP_LVTS_H__
/*==================================================
 * Definition or macro function
 *==================================================
 */
#define LK_LVTS_MAGIC (0x0000555)

#define DISABLE_THERMAL_HW_REBOOT (-274000)

#define CLOCK_26MHZ_CYCLE_NS	(38)
#define BUS_ACCESS_US		(2)

#define FEATURE_DEVICE_AUTO_RCK	(BIT(0))
#define FEATURE_CK26M_ACTIVE	(BIT(1))
#define ENABLE_FEATURE(feature)		(lvts_data->feature_bitmap |= feature)
#define DISABLE_FEATURE(feature)	(lvts_data->feature_bitmap &= (~feature))
#define IS_ENABLE(feature)		(lvts_data->feature_bitmap & feature)

#define IS_EMPTY_STR(str)	(str[0] == '\0')

#define GET_BASE_ADDR(tc_id)	\
	(lvts_data->domain[lvts_data->tc[tc_id].domain_index].base	\
	+ lvts_data->tc[tc_id].addr_offset)

#define SET_TC_SPEED_IN_US(pu, gd, fd, sd) \
	{	\
		.period_unit = ((pu * 1000) / (256 * CLOCK_26MHZ_CYCLE_NS)),	\
		.group_interval_delay = (gd / pu),	\
		.filter_interval_delay = (fd / pu),	\
		.sensor_interval_delay = (sd / pu),	\
	}

#define GET_CAL_DATA_BITMASK(index, h, l)	\
	((index < lvts_data->num_efuse_addr)	\
	? ((lvts_data->efuse[index] & GENMASK(h, l)) >> l)	\
	: 0)
#define GET_CAL_DATA_BIT(index, bit)	\
	((index < lvts_data->num_efuse_addr)	\
	? ((lvts_data->efuse[index] & BIT(bit)) >> bit)	\
	: 0)

#define GET_TC_SENSOR_NUM(tc_id)	\
	(lvts_data->tc[tc_id].num_sensor)

#define ONE_SAMPLE (lvts_data->counting_window_us + 2 * BUS_ACCESS_US)

#define NUM_OF_SAMPLE(tc_id)	\
	((lvts_data->tc[tc_id].hw_filter < LVTS_FILTER_2) ? 1 :	\
	((lvts_data->tc[tc_id].hw_filter > LVTS_FILTER_16_OF_18) ? 1 :	\
	((lvts_data->tc[tc_id].hw_filter == LVTS_FILTER_16_OF_18) ? 18 :\
	((lvts_data->tc[tc_id].hw_filter == LVTS_FILTER_8_OF_10) ? 10 :	\
	(lvts_data->tc[tc_id].hw_filter * 2)))))

#define PERIOD_UNIT_US(tc_id)	\
	((lvts_data->tc[tc_id].tc_speed.period_unit * 256 *	\
	CLOCK_26MHZ_CYCLE_NS) / 1000)
#define FILTER_INT_US(tc_id)	\
	(lvts_data->tc[tc_id].tc_speed.filter_interval_delay	\
	* PERIOD_UNIT_US(tc_id))
#define SENSOR_INT_US(tc_id)	\
	(lvts_data->tc[tc_id].tc_speed.sensor_interval_delay	\
	* PERIOD_UNIT_US(tc_id))
#define GROUP_INT_US(tc_id)	\
	(lvts_data->tc[tc_id].tc_speed.group_interval_delay	\
	* PERIOD_UNIT_US(tc_id))

#define SENSOR_LATENCY_US(tc_id) \
	((NUM_OF_SAMPLE(tc_id) - 1) * FILTER_INT_US(tc_id)	\
	+ NUM_OF_SAMPLE(tc_id) * ONE_SAMPLE)

#define GROUP_LATENCY_US(tc_id)	\
	(GET_TC_SENSOR_NUM(tc_id) * SENSOR_LATENCY_US(tc_id)	\
	+ (GET_TC_SENSOR_NUM(tc_id) - 1) * SENSOR_INT_US(tc_id)	\
	+ GROUP_INT_US(tc_id))
/* LVTS HW filter settings
 * 000: Get one sample
 * 001: Get 2 samples and average them
 * 010: Get 4 samples, drop max and min, then average the rest of 2 samples
 * 011: Get 6 samples, drop max and min, then average the rest of 4 samples
 * 100: Get 10 samples, drop max and min, then average the rest of 8 samples
 * 101: Get 18 samples, drop max and min, then average the rest of 16 samples
 */
enum lvts_hw_filter {
	LVTS_FILTER_1,
	LVTS_FILTER_2,
	LVTS_FILTER_2_OF_4,
	LVTS_FILTER_4_OF_6,
	LVTS_FILTER_8_OF_10,
	LVTS_FILTER_16_OF_18
};
enum lvts_sensing_point {
	SENSING_POINT0,
	SENSING_POINT1,
	SENSING_POINT2,
	SENSING_POINT3,
	ALL_SENSING_POINTS
};

enum calibration_mode {
	CALI_NT,
	CALI_HT,
	ALL_CALI_MODES
};

/*==================================================
 * Data structure
 *==================================================
 */
struct lvts_data;

struct speed_settings {
	unsigned int period_unit;
	unsigned int group_interval_delay;
	unsigned int filter_interval_delay;
	unsigned int sensor_interval_delay;
};

struct formula_coeff {
	int a[ALL_SENSING_POINTS];
	unsigned int golden_temp;
	enum calibration_mode cali_mode;
};

struct tc_settings {
	unsigned int domain_index;
	unsigned int addr_offset;
	unsigned int num_sensor;
	unsigned int sensor_map[ALL_SENSING_POINTS]; /* In sensor ID */
	unsigned int device_id[ALL_SENSING_POINTS]; /* In LVTS Device ID */
	struct speed_settings tc_speed;
	/* HW filter setting
	 * 000: Get one sample
	 * 001: Get 2 samples and average them
	 * 010: Get 4 samples, drop max and min, then average the rest of 2 samples
	 * 011: Get 6 samples, drop max and min, then average the rest of 4 samples
	 * 100: Get 10 samples, drop max and min, then average the rest of 8 samples
	 * 101: Get 18 samples, drop max and min, then average the rest of 16 samples
	 */
	unsigned int hw_filter;
	/* Dominator_sensing point is used to select a sensing point
	 * and reference its temperature to trigger Thermal HW Reboot
	 * When it is ALL_SENSING_POINTS, it will select all sensing points
	 */
	int dominator_sensing_point;
	int hw_reboot_trip_point; /* -274000: Disable HW reboot */
	unsigned int irq_bit;
	struct formula_coeff coeff;
};

struct sensor_cal_data {
	int use_fake_efuse;	/* 1: Use fake efuse, 0: Use real efuse */
	unsigned int golden_temp;
	unsigned int golden_temp_ht;
	unsigned int cali_mode;
	unsigned int *count_r;
	unsigned int *count_rc;
	unsigned int *count_rc_now;
	unsigned int *efuse_data;

	unsigned int default_golden_temp;
	unsigned int default_golden_temp_ht;
	unsigned int default_count_r;
	unsigned int default_count_rc;
};

struct platform_ops {
	void (*device_identification)(struct lvts_data *lvts_data);
	void (*efuse_to_cal_data)(struct lvts_data *lvts_data);
	void (*device_enable_and_init)(struct lvts_data *lvts_data);
	void (*device_enable_auto_rck)(struct lvts_data *lvts_data);
	int (*device_read_count_rc_n)(struct lvts_data *lvts_data);
	void (*set_cal_data)(struct lvts_data *lvts_data);
	void (*init_controller)(struct lvts_data *lvts_data);
	int (*lvts_raw_to_temp)(struct formula_coeff *co, unsigned int id, unsigned int msr_raw);
	unsigned int (*lvts_temp_to_raw)(struct formula_coeff *co, unsigned int id, int temp);
	void (*check_cal_data)(struct lvts_data *lvts_data);
	void (*update_coef_data)(struct lvts_data *lvts_data);
};

struct power_domain {
	void __iomem *base;	/* LVTS base addresses */
	unsigned int irq_num;	/* LVTS interrupt numbers */
	struct reset_control *reset;
};

struct sensor_data {
	int temp;		/* Current temperature */
	unsigned int msr_raw;	/* MSR raw data from LVTS */
};

struct lvts_data {
	struct device *dev;
	struct clk *clk;
	unsigned int num_domain;
	struct power_domain *domain;

	int num_tc;			/* Number of LVTS thermal controllers */
	struct tc_settings *tc;
	int counting_window_us;		/* LVTS device counting window */

	int num_sensor;			/* Number of sensors in this platform */
	struct sensor_data *sen_data;
	struct mutex sen_data_lock;	/* protect sen_data */
	struct thermal_zone_device *tz_dev; /* tz_dev of id 0 for HW reboot trip point update */

	struct platform_ops ops;
	int feature_bitmap;		/* Show what features are enabled */

	unsigned int num_efuse_addr;
	unsigned int *efuse;
	unsigned int num_efuse_block;	/* Number of contiguous efuse indexes */
	struct sensor_cal_data cal_data;
	bool init_done; /*lvts driver init finish*/
	unsigned int *irq_bitmap;
	int enable_dump_log;
};

struct soc_temp_tz {
	unsigned int id; /* if id is 0, get max temperature of all sensors */
	struct lvts_data *lvts_data;
};

struct match_entry {
	char	chip[32];
	struct lvts_data *lvts_data;
};

struct lvts_match_data {
	unsigned int hw_version;
	struct match_entry *table;
	void (*set_up_common_callbacks)(struct lvts_data *lvts_data);
	struct list_head node;
};

struct lvts_id {
	unsigned int hw_version;
	char	chip[32];
};

/*==================================================
 * LVTS device register
 *==================================================
 */
#define RG_TSFM_DATA_0	0x00
#define RG_TSFM_DATA_1	0x01
#define RG_TSFM_DATA_2	0x02
#define RG_TSFM_CTRL_0	0x03
#define RG_TSFM_CTRL_1	0x04
#define RG_TSFM_CTRL_2	0x05
#define RG_TSFM_CTRL_3	0x06
#define RG_TSFM_CTRL_4	0x07
#define RG_TSV2F_CTRL_0	0x08
#define RG_TSV2F_CTRL_1	0x09
#define RG_TSV2F_CTRL_2	0x0A
#define RG_TSV2F_CTRL_3	0x0B
#define RG_TSV2F_CTRL_4	0x0C
#define RG_TSV2F_CTRL_5	0x0D
#define RG_TSV2F_CTRL_6	0x0E
#define RG_TEMP_DATA_0	0x10
#define RG_TEMP_DATA_1	0x11
#define RG_TEMP_DATA_2	0x12
#define RG_TEMP_DATA_3	0x13
#define RG_RC_DATA_0	0x14
#define RG_RC_DATA_1	0x15
#define RG_RC_DATA_2	0x16
#define RG_RC_DATA_3	0x17
#define RG_DIV_DATA_0	0x18
#define RG_DIV_DATA_1	0x19
#define RG_DIV_DATA_2	0x1A
#define RG_DIV_DATA_3	0x1B
#define RG_TST_DATA_0	0x70
#define RG_TST_DATA_1	0x71
#define RG_TST_DATA_2	0x72
#define RG_TST_CTRL	0x73
#define RG_DBG_FQMTR	0xF0
#define RG_DBG_LPSEQ	0xF1
#define RG_DBG_STATE	0xF2
#define RG_DBG_CHKSUM	0xF3
#define RG_DID_LVTS	0xFC
#define RG_DID_REV	0xFD
#define RG_TSFM_RST	0xFF
/*==================================================
 * LVTS controller register
 *==================================================
 */
#define LVTSMONCTL0_0	0x000
#define LVTS_SINGLE_SENSE	(1 << 9)
#define ENABLE_SENSING_POINT(num)	(LVTS_SINGLE_SENSE | GENMASK((num - 1), 0))
#define DISABLE_SENSING_POINT	(LVTS_SINGLE_SENSE | 0x0)
#define LVTSMONCTL1_0	0x004
#define LVTSMONCTL2_0	0x008
#define LVTSMONINT_0	0x00C
#define STAGE3_INT_EN	(1 << 31)

#define HIGH_OFFSET3_INT_EN	(1 << 25)
#define HIGH_OFFSET2_INT_EN	(1 << 13)
#define HIGH_OFFSET1_INT_EN	(1 << 8)
#define HIGH_OFFSET0_INT_EN	(1 << 3)

#define LOW_OFFSET3_INT_EN	(1 << 24)
#define LOW_OFFSET2_INT_EN	(1 << 12)
#define LOW_OFFSET1_INT_EN	(1 << 7)
#define LOW_OFFSET0_INT_EN	(1 << 2)

#define HOT_INT3_EN		(1 << 23)
#define HOT_INT2_EN		(1 << 11)
#define HOT_INT1_EN		(1 << 6)
#define HOT_INT0_EN		(1 << 1)

#define LVTSMONINTSTS_0	0x010
#define LVTSMONIDET0_0	0x014
#define LVTSMONIDET1_0	0x018
#define LVTSMONIDET2_0	0x01C
#define LVTSMONIDET3_0	0x020
#define LVTSH2NTHRE_0	0x024
#define LVTSHTHRE_0	0x028
#define LVTSCTHRE_0	0x02C
#define LVTSOFFSETH_0	0x030
#define LVTSOFFSETL_0	0x034
#define LVTSMSRCTL0_0	0x038
#define LVTSMSRCTL1_0	0x03C
#define LVTSTSSEL_0	0x040
#define SET_SENSOR_INDEX	0x13121110
#define LVTSDEVICETO_0	0x044
#define LVTSCALSCALE_0	0x048
#define SET_CALC_SCALE_RULES	0x00000300
#define LVTS_ID_0	0x04C
#define LVTS_CONFIG_0	0x050
#define CK26M_ACTIVE	(((lvts_data->feature_bitmap & FEATURE_CK26M_ACTIVE)	\
			? 1 : 0) << 30)
#define BROADCAST_ID_UPDATE	(1 << 26)
#define DEVICE_SENSING_STATUS	(1 << 25)
#define DEVICE_ACCESS_STARTUS	(1 << 24)
#define WRITE_ACCESS		(1 << 16)

#define DEVICE_WRITE		(1 << 31 | 1 << 30 | DEVICE_ACCESS_STARTUS \
				| 1 << 17 | WRITE_ACCESS)


#define DEVICE_READ		(1 << 31 | 1 << 30 | DEVICE_ACCESS_STARTUS \
				| 1 << 17)
#define RESET_ALL_DEVICES	(DEVICE_WRITE | RG_TSFM_RST << 8 | 0xFF)
#define READ_BACK_DEVICE_ID	(1 << 31 | 1 << 30 | BROADCAST_ID_UPDATE	\
				| DEVICE_ACCESS_STARTUS | 1 << 17	\
				| RG_DID_LVTS << 8)
#define READ_DEVICE_REG(reg_idx)	(DEVICE_READ | reg_idx << 8 | 0x00)
#define LVTSEDATA00_0	0x054
#define LVTSEDATA01_0	0x058
#define LVTSEDATA02_0	0x05C
#define LVTSEDATA03_0	0x060
#define LVTSMSR0_0	0x090
#define MRS_RAW_MASK		GENMASK(15, 0)
#define MRS_RAW_VALID_BIT	BIT(16)
#define LVTSMSR1_0	0x094
#define LVTSMSR2_0	0x098
#define LVTSMSR3_0	0x09C
#define LVTSIMMD0_0	0x0A0
#define LVTSIMMD1_0	0x0A4
#define LVTSIMMD2_0	0x0A8
#define LVTSIMMD3_0	0x0AC
#define LVTSRDATA0_0	0x0B0
#define LVTSRDATA1_0	0x0B4
#define LVTSRDATA2_0	0x0B8
#define LVTSRDATA3_0	0x0BC
#define LVTSPROTCTL_0	0x0C0
#define PROTOFFSET	GENMASK(15, 0)
#define LVTSPROTTA_0	0x0C4
#define LVTSPROTTB_0	0x0C8
#define LVTSPROTTC_0	0x0CC
#define LVTSCLKEN_0	0x0E4
#define ENABLE_LVTS_CTRL_CLK	(1)
#define DISABLE_LVTS_CTRL_CLK	(0)
#define LVTSDBGSEL_0	0x0E8
#define LVTSDBGSIG_0	0x0EC
#define LVTSSPARE0_0	0x0F0
#define LVTSSPARE1_0	0x0F4
#define LVTSSPARE2_0	0x0F8
#define LVTSSPARE3_0	0x0FC

#define THERMINTST	0xF04
/*==================================================
 * LVTS register mask
 *==================================================
 */
#define THERMAL_COLD_INTERRUPT_0		0x00000001
#define THERMAL_HOT_INTERRUPT_0			0x00000002
#define THERMAL_LOW_OFFSET_INTERRUPT_0		0x00000004
#define THERMAL_HIGH_OFFSET_INTERRUPT_0		0x00000008
#define THERMAL_HOT2NORMAL_INTERRUPT_0		0x00000010
#define THERMAL_COLD_INTERRUPT_1		0x00000020
#define THERMAL_HOT_INTERRUPT_1			0x00000040
#define THERMAL_LOW_OFFSET_INTERRUPT_1		0x00000080
#define THERMAL_HIGH_OFFSET_INTERRUPT_1		0x00000100
#define THERMAL_HOT2NORMAL_INTERRUPT_1		0x00000200
#define THERMAL_COLD_INTERRUPT_2		0x00000400
#define THERMAL_HOT_INTERRUPT_2			0x00000800
#define THERMAL_LOW_OFFSET_INTERRUPT_2		0x00001000
#define THERMAL_HIGH_OFFSET_INTERRUPT_2		0x00002000
#define THERMAL_HOT2NORMAL_INTERRUPT_2		0x00004000
#define THERMAL_AHB_TIMEOUT_INTERRUPT		0x00008000
#define THERMAL_DEVICE_TIMEOUT_INTERRUPT	0x00008000
#define THERMAL_IMMEDIATE_INTERRUPT_0		0x00010000
#define THERMAL_IMMEDIATE_INTERRUPT_1		0x00020000
#define THERMAL_IMMEDIATE_INTERRUPT_2		0x00040000
#define THERMAL_FILTER_INTERRUPT_0		0x00080000
#define THERMAL_FILTER_INTERRUPT_1		0x00100000
#define THERMAL_FILTER_INTERRUPT_2		0x00200000
#define THERMAL_COLD_INTERRUPT_3		0x00400000
#define THERMAL_HOT_INTERRUPT_3			0x00800000
#define THERMAL_LOW_OFFSET_INTERRUPT_3		0x01000000
#define THERMAL_HIGH_OFFSET_INTERRUPT_3		0x02000000
#define THERMAL_HOT2NORMAL_INTERRUPT_3		0x04000000
#define THERMAL_IMMEDIATE_INTERRUPT_3		0x08000000
#define THERMAL_FILTER_INTERRUPT_3		0x10000000
#define THERMAL_PROTECTION_STAGE_1		0x20000000
#define THERMAL_PROTECTION_STAGE_2		0x40000000
#define THERMAL_PROTECTION_STAGE_3		0x80000000
#endif /* __MTK_SOC_TEMP_LVTS_H__ */

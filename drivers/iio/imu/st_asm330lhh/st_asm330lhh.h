/*
 * STMicroelectronics st_asm330lhh sensor driver
 *
 * Copyright 2020 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_ASM330LHH_H
#define ST_ASM330LHH_H

#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/ktime.h>
#include <linux/slab.h>

#define ST_ASM330LHH_REVISION			"2.0.1"
#define ST_ASM330LHH_PATCH			"4"

#define ST_ASM330LHH_VERSION			"v"	\
	ST_ASM330LHH_REVISION				\
	"-"						\
	ST_ASM330LHH_PATCH

#define ST_ASM330LHH_MAX_ODR			833
#define ST_ASM330LHH_ODR_LIST_SIZE		8
#define ST_ASM330LHH_ODR_EXPAND(odr, uodr)	((odr * 1000000) + uodr)

#define ST_ASM330LHH_DEV_NAME			"asm330lhh"

#define ST_ASM330LHH_DEFAULT_XL_FS_INDEX	2
#define ST_ASM330LHH_DEFAULT_XL_ODR_INDEX	1
#define ST_ASM330LHH_DEFAULT_G_FS_INDEX		3
#define ST_ASM330LHH_DEFAULT_G_ODR_INDEX	1
#define ST_ASM330LHH_DEFAULT_T_FS_INDEX		0
#define ST_ASM330LHH_DEFAULT_T_ODR_INDEX	1

#define ST_ASM330LHH_REG_FIFO_CTRL1_ADDR	0x07
#define ST_ASM330LHH_REG_FIFO_CTRL2_ADDR	0x08
#define ST_ASM330LHH_REG_FIFO_WTM_MASK		GENMASK(8, 0)
#define ST_ASM330LHH_REG_FIFO_WTM8_MASK		BIT(0)
#define ST_ASM330LHH_REG_FIFO_STATUS_DIFF	GENMASK(9, 0)

#define ST_ASM330LHH_REG_FIFO_CTRL3_ADDR	0x09
#define ST_ASM330LHH_REG_BDR_XL_MASK		GENMASK(3, 0)
#define ST_ASM330LHH_REG_BDR_GY_MASK		GENMASK(7, 4)

#define ST_ASM330LHH_REG_FIFO_CTRL4_ADDR	0x0a
#define ST_ASM330LHH_REG_FIFO_MODE_MASK		GENMASK(2, 0)
#define ST_ASM330LHH_REG_DEC_TS_MASK		GENMASK(7, 6)
#define ST_ASM330LHH_REG_ODR_T_BATCH_MASK	GENMASK(5, 4)

#define ST_ASM330LHH_REG_INT1_CTRL_ADDR		0x0d
#define ST_ASM330LHH_REG_INT2_CTRL_ADDR		0x0e
#define ST_ASM330LHH_REG_INT_FIFO_TH_MASK	BIT(3)

#define ST_ASM330LHH_REG_WHOAMI_ADDR		0x0f
#define ST_ASM330LHH_WHOAMI_VAL			0x6b

#define ST_ASM330LHH_CTRL1_XL_ADDR		0x10
#define ST_ASM330LHH_CTRL2_G_ADDR		0x11

#define ST_ASM330LHH_REG_CTRL3_C_ADDR		0x12
#define ST_ASM330LHH_REG_SW_RESET_MASK		BIT(0)
#define ST_ASM330LHH_REG_PP_OD_MASK		BIT(4)
#define ST_ASM330LHH_REG_H_LACTIVE_MASK		BIT(5)
#define ST_ASM330LHH_REG_BDU_MASK		BIT(6)
#define ST_ASM330LHH_REG_BOOT_MASK		BIT(7)

#define ST_ASM330LHH_REG_CTRL4_C_ADDR		0x13
#define ST_ASM330LHH_REG_DRDY_MASK		BIT(3)

#define ST_ASM330LHH_REG_CTRL5_C_ADDR		0x14
#define ST_ASM330LHH_REG_ROUNDING_MASK		GENMASK(6, 5)

#define ST_ASM330LHH_REG_CTRL9_XL_ADDR		0x18
#define ST_ASM330LHH_REG_DEVICE_CONF_MASK	BIT(1)

#define ST_ASM330LHH_REG_CTRL10_C_ADDR		0x19
#define ST_ASM330LHH_REG_TIMESTAMP_EN_MASK	BIT(5)

#define ST_ASM330LHH_REG_STATUS_ADDR		0x1e
#define ST_ASM330LHH_REG_STATUS_TDA		BIT(2)

#define ST_ASM330LHH_REG_OUT_TEMP_L_ADDR	0x20

#define ST_ASM330LHH_REG_OUTX_L_A_ADDR		0x28
#define ST_ASM330LHH_REG_OUTY_L_A_ADDR		0x2a
#define ST_ASM330LHH_REG_OUTZ_L_A_ADDR		0x2c

#define ST_ASM330LHH_REG_OUTX_L_G_ADDR		0x22
#define ST_ASM330LHH_REG_OUTY_L_G_ADDR		0x24
#define ST_ASM330LHH_REG_OUTZ_L_G_ADDR		0x26

#define ST_ASM330LHH_REG_TAP_CFG0_ADDR		0x56
#define ST_ASM330LHH_REG_LIR_MASK		BIT(0)

#define ST_ASM330LHH_REG_THS_6D_ADDR		0x59
#define ST_ASM330LHH_SIXD_THS_MASK		GENMASK(6, 5)

#define ST_ASM330LHH_REG_WAKE_UP_THS_ADDR	0x5b
#define ST_ASM330LHH_WAKE_UP_THS_MASK		GENMASK(5, 0)

#define ST_ASM330LHH_REG_WAKE_UP_DUR_ADDR	0x5c
#define ST_ASM330LHH_WAKE_UP_DUR_MASK		GENMASK(6, 5)

#define ST_ASM330LHH_REG_FREE_FALL_ADDR		0x5d
#define ST_ASM330LHH_FF_THS_MASK		GENMASK(2, 0)

#define ST_ASM330LHH_INTERNAL_FREQ_FINE		0x63

/* Timestamp Tick 25us/LSB */
#define ST_ASM330LHH_TS_DELTA_NS		25000ULL

/* Define Custom events for FIFO flush */
#define CUSTOM_IIO_EV_DIR_FIFO_EMPTY (IIO_EV_DIR_NONE + 1)
#define CUSTOM_IIO_EV_DIR_FIFO_DATA (IIO_EV_DIR_NONE + 2)
#define CUSTOM_IIO_EV_TYPE_FIFO_FLUSH (IIO_EV_TYPE_CHANGE + 1)

/* Temperature in uC */
#define ST_ASM330LHH_TEMP_GAIN			256
#define ST_ASM330LHH_TEMP_FS_GAIN		(1000000 / \
						ST_ASM330LHH_TEMP_GAIN)
#define ST_ASM330LHH_TEMP_OFFSET		6400

/* FIFO simple size and depth */
#define ST_ASM330LHH_SAMPLE_SIZE		6
#define ST_ASM330LHH_TS_SAMPLE_SIZE		4
#define ST_ASM330LHH_TAG_SIZE			1
#define ST_ASM330LHH_FIFO_SAMPLE_SIZE		(ST_ASM330LHH_SAMPLE_SIZE + \
						 ST_ASM330LHH_TAG_SIZE)
#define ST_ASM330LHH_MAX_FIFO_DEPTH		416

#define ST_ASM330LHH_DATA_CHANNEL(chan_type, addr, mod, ch2, scan_idx,	\
				rb, sb, sg)				\
{									\
	.type = chan_type,						\
	.address = addr,						\
	.modified = mod,						\
	.channel2 = ch2,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
			      BIT(IIO_CHAN_INFO_SCALE),			\
	.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = scan_idx,						\
	.scan_type = {							\
		.sign = sg,						\
		.realbits = rb,						\
		.storagebits = sb,					\
		.endianness = IIO_LE,					\
	},								\
}

static const struct iio_event_spec st_asm330lhh_flush_event = {
	.type = CUSTOM_IIO_EV_TYPE_FIFO_FLUSH,
	.dir = IIO_EV_DIR_EITHER,
};

#define ST_ASM330LHH_EVENT_CHANNEL(ctype, etype)	\
{							\
	.type = ctype,					\
	.modified = 0,					\
	.scan_index = -1,				\
	.indexed = -1,					\
	.event_spec = &st_asm330lhh_flush_event,	\
	.num_event_specs = 1,				\
}

#define ST_ASM330LHH_RX_MAX_LENGTH	64
#define ST_ASM330LHH_TX_MAX_LENGTH	16

#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
#define ASM_MAXSAMPLE        4000
#define G_MAX                    23920640
struct asm_sample {
	int xyz[3];
	unsigned int tsec;
	unsigned long long tnsec;
};
#endif

struct st_asm330lhh_transfer_buffer {
	u8 rx_buf[ST_ASM330LHH_RX_MAX_LENGTH];
	u8 tx_buf[ST_ASM330LHH_TX_MAX_LENGTH] ____cacheline_aligned;
};

struct st_asm330lhh_transfer_function {
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
	int (*write)(struct device *dev, u8 addr, int len, const u8 *data);
};

/**
 * struct st_asm330lhh_reg - Generic sensor register description (addr + mask)
 * @addr: Address of register.
 * @mask: Bitmask register for proper usage.
 */
struct st_asm330lhh_reg {
	u8 addr;
	u8 mask;
};

/**
 * struct st_asm330lhh_odr - Single ODR entry
 * @hz: Most significant part of the sensor ODR (Hz).
 * @uhz: Less significant part of the sensor ODR (micro Hz).
 * @val: ODR register value.
 * @batch_val: Batching ODR register value.
 */
struct st_asm330lhh_odr {
	u16 hz;
	u32 uhz;
	u8 val;
	u8 batch_val;
};

/**
 * struct st_asm330lhh_odr_table_entry - Sensor ODR table
 * @size: Size of ODR table.
 * @reg: ODR register.
 * @batching_reg: ODR register for batching on fifo.
 * @odr_avl: Array of supported ODR value.
 */
struct st_asm330lhh_odr_table_entry {
	u8 size;
	struct st_asm330lhh_reg reg;
	struct st_asm330lhh_reg batching_reg;
	struct st_asm330lhh_odr odr_avl[ST_ASM330LHH_ODR_LIST_SIZE];
};

/**
 * struct st_asm330lhh_fs - Full Scale sensor table entry
 * @reg: Register description for FS settings.
 * @gain: Sensor sensitivity (mdps/LSB, mg/LSB and uC/LSB).
 * @val: FS register value.
 */
struct st_asm330lhh_fs {
	struct st_asm330lhh_reg reg;
	u32 gain;
	u8 val;
};

#define ST_ASM330LHH_FS_LIST_SIZE		6
#define ST_ASM330LHH_FS_ACC_LIST_SIZE		4
#define ST_ASM330LHH_FS_GYRO_LIST_SIZE		6
#define ST_ASM330LHH_FS_TEMP_LIST_SIZE		1

/**
 * struct st_asm330lhh_fs_table_entry - Full Scale sensor table
 * @size: Full Scale sensor table size.
 * @fs_avl: Full Scale list entries.
 */
struct st_asm330lhh_fs_table_entry {
	u8 size;
	struct st_asm330lhh_fs fs_avl[ST_ASM330LHH_FS_LIST_SIZE];
};

#define ST_ASM330LHH_ACC_FS_2G_GAIN	IIO_G_TO_M_S_2(61)
#define ST_ASM330LHH_ACC_FS_4G_GAIN	IIO_G_TO_M_S_2(122)
#define ST_ASM330LHH_ACC_FS_8G_GAIN	IIO_G_TO_M_S_2(244)
#define ST_ASM330LHH_ACC_FS_16G_GAIN	IIO_G_TO_M_S_2(488)

#define ST_ASM330LHH_GYRO_FS_125_GAIN	IIO_DEGREE_TO_RAD(4375)
#define ST_ASM330LHH_GYRO_FS_250_GAIN	IIO_DEGREE_TO_RAD(8750)
#define ST_ASM330LHH_GYRO_FS_500_GAIN	IIO_DEGREE_TO_RAD(17500)
#define ST_ASM330LHH_GYRO_FS_1000_GAIN	IIO_DEGREE_TO_RAD(35000)
#define ST_ASM330LHH_GYRO_FS_2000_GAIN	IIO_DEGREE_TO_RAD(70000)
#define ST_ASM330LHH_GYRO_FS_4000_GAIN	IIO_DEGREE_TO_RAD(140000)

enum st_asm330lhh_sensor_id {
	ST_ASM330LHH_ID_GYRO = 0,
	ST_ASM330LHH_ID_ACC,
	ST_ASM330LHH_ID_TEMP,
	ST_ASM330LHH_ID_MAX,
};

enum st_asm330lhh_fifo_mode {
	ST_ASM330LHH_FIFO_BYPASS = 0x0,
	ST_ASM330LHH_FIFO_CONT = 0x6,
};

enum {
	ST_ASM330LHH_HW_FLUSH,
	ST_ASM330LHH_HW_OPERATIONAL,
};

/**
 * struct st_asm330lhh_sensor - ST IMU sensor instance
 * @id: Sensor identifier.
 * @hw: Pointer to instance of struct st_asm330lhh_hw.
 * @gain: Configured sensor sensitivity.
 * @odr: Output data rate of the sensor [Hz].
 * @watermark: Sensor watermark level.
 * @last_fifo_timestamp: Store last sample timestamp in FIFO, used by flush
 */
struct st_asm330lhh_sensor {
	enum st_asm330lhh_sensor_id id;
	struct st_asm330lhh_hw *hw;
	struct iio_trigger *trig;

	u32 gain;
	u32 offset;

	__le16 old_data;

	int odr;
	int uodr;

	u16 max_watermark;
	u16 watermark;
	s64 last_fifo_timestamp;
#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
	bool read_boot_sample;
	int bufsample_cnt;
	bool buffer_asm_samples;
	struct kmem_cache *asm_cachepool;
	struct asm_sample *asm_samplist[ASM_MAXSAMPLE];
	ktime_t timestamp;
	int max_buffer_time;
	struct input_dev *buf_dev;
	int report_evt_cnt;
	struct mutex sensor_buff;
#endif
};

/**
 * struct st_asm330lhh_hw - ST IMU MEMS hw instance
 * @dev: Pointer to instance of struct device (I2C or SPI).
 * @irq: Device interrupt line (I2C or SPI).
 * @lock: Mutex to protect read and write operations.
 * @fifo_lock: Mutex to prevent concurrent access to the hw FIFO.
 * @page_lock: Mutex to prevent concurrent memory page configuration.
 * @fifo_mode: FIFO operating mode supported by the device.
 * @state: hw operational state.
 * @enable_mask: Enabled sensor bitmask.
 * @ts_offset: Hw timestamp offset.
 * @hw_ts: Latest hw timestamp from the sensor.
 * @ts: Latest timestamp from irq handler.
 * @delta_ts: Delta time between two consecutive interrupts.
 * @ts_delta_ns: Calibrate delta time tick.
 * @hw_ts: Latest hw timestamp from the sensor.
 * @val_ts_old: Hold hw timestamp for timer rollover.
 * @hw_ts_high: Save MSB hw timestamp.
 * @tsample: Timestamp for each sensor sample.
 * @delta_ts: Delta time between two consecutive interrupts.
 * @ts: Latest timestamp from irq handler.
 * @odr_table_entry: Sensors ODR table.
 * @iio_devs: Pointers to acc/gyro iio_dev instances.
 * @tf: Transfer function structure used by I/O operations.
 * @tb: Transfer buffers used by SPI I/O operations.
 */
struct st_asm330lhh_hw {
	struct device *dev;
	int irq;
	int int_pin;

	struct mutex lock;
	struct mutex fifo_lock;
	struct mutex page_lock;

	enum st_asm330lhh_fifo_mode fifo_mode;
	unsigned long state;
	u32 enable_mask;

	s64 ts_offset;
	u64 ts_delta_ns;
	s64 hw_ts;
	u32 val_ts_old;
	u32 hw_ts_high;
	s64 tsample;
	s64 delta_ts;
	s64 ts;
	const struct st_asm330lhh_odr_table_entry *odr_table_entry;
	struct iio_dev *iio_devs[ST_ASM330LHH_ID_MAX];

	const struct st_asm330lhh_transfer_function *tf;
	struct st_asm330lhh_transfer_buffer tb;
	struct regulator *vdd;
	struct regulator *vio;
	struct iio_mount_matrix orientation;
	int enable_gpio;
	bool asm330_hrtimer;
	struct hrtimer st_asm330lhh_hrtimer;
};

extern const struct dev_pm_ops st_asm330lhh_pm_ops;

static inline int st_asm330lhh_read_atomic(struct st_asm330lhh_hw *hw, u8 addr,
					 int len, u8 *data)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = hw->tf->read(hw->dev, addr, len, data);
	mutex_unlock(&hw->page_lock);

	return err;
}

static inline int st_asm330lhh_write_atomic(struct st_asm330lhh_hw *hw, u8 addr,
					  int len, u8 *data)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = hw->tf->write(hw->dev, addr, len, data);
	mutex_unlock(&hw->page_lock);

	return err;
}

int __st_asm330lhh_write_with_mask(struct st_asm330lhh_hw *hw, u8 addr, u8 mask,
				 u8 val);
static inline int st_asm330lhh_write_with_mask(struct st_asm330lhh_hw *hw,
		u8 addr, u8 mask, u8 val)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = __st_asm330lhh_write_with_mask(hw, addr, mask, val);
	mutex_unlock(&hw->page_lock);

	return err;
}

static inline bool st_asm330lhh_is_fifo_enabled(struct st_asm330lhh_hw *hw)
{
	return hw->enable_mask & (BIT(ST_ASM330LHH_ID_GYRO) |
				  BIT(ST_ASM330LHH_ID_ACC));
}

int st_asm330lhh_probe(struct device *dev, int irq,
		       const struct st_asm330lhh_transfer_function *tf_ops);
int st_asm330lhh_sensor_set_enable(struct st_asm330lhh_sensor *sensor,
				   bool enable);
int st_asm330lhh_buffers_setup(struct st_asm330lhh_hw *hw);
int st_asm330lhh_get_batch_val(struct st_asm330lhh_sensor *sensor, int odr,
			       int uodr, u8 *val);
int st_asm330lhh_update_watermark(struct st_asm330lhh_sensor *sensor,
				  u16 watermark);
ssize_t st_asm330lhh_flush_fifo(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size);
ssize_t st_asm330lhh_get_max_watermark(struct device *dev,
				       struct device_attribute *attr, char *buf);
ssize_t st_asm330lhh_get_watermark(struct device *dev,
				   struct device_attribute *attr, char *buf);
ssize_t st_asm330lhh_set_watermark(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size);
int st_asm330lhh_suspend_fifo(struct st_asm330lhh_hw *hw);
int st_asm330lhh_set_fifo_mode(struct st_asm330lhh_hw *hw,
			       enum st_asm330lhh_fifo_mode fifo_mode);
int __st_asm330lhh_set_sensor_batching_odr(struct st_asm330lhh_sensor *sensor,
					   bool enable);
int st_asm330lhh_update_batching(struct iio_dev *iio_dev, bool enable);
int st_asm330lhh_reset_hwts(struct st_asm330lhh_hw *hw);
int st_asm330lhh_update_fifo(struct iio_dev *iio_dev, bool enable);
int asm330_check_acc_gyro_early_buff_enable_flag(
		struct st_asm330lhh_sensor *sensor);
void st_asm330lhh_set_cpu_idle_state(bool value);
void st_asm330lhh_hrtimer_reset(struct st_asm330lhh_hw *hw, s64 irq_delta_ts);
#endif /* ST_ASM330LHH_H */

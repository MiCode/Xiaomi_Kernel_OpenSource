/*
 * STMicroelectronics st_asm330lhh sensor driver
 *
 * Copyright 2018 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_ASM330LHH_H
#define ST_ASM330LHH_H

#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/input.h>
#include <linux/ktime.h>
#include <linux/slab.h>

#define ST_ASM330LHH_REVISION		"2.0.1"
#define ST_ASM330LHH_PATCH		"2"

#define ST_ASM330LHH_VERSION		"v"	\
	ST_ASM330LHH_REVISION			\
	"-"					\
	ST_ASM330LHH_PATCH

#define ST_ASM330LHH_DEV_NAME		"asm330lhh"

#define ST_ASM330LHH_SAMPLE_SIZE	6
#define ST_ASM330LHH_TS_SAMPLE_SIZE	4
#define ST_ASM330LHH_TAG_SIZE		1
#define ST_ASM330LHH_FIFO_SAMPLE_SIZE	(ST_ASM330LHH_SAMPLE_SIZE + \
					 ST_ASM330LHH_TAG_SIZE)
#define ST_ASM330LHH_MAX_FIFO_DEPTH	416

#define ST_ASM330LHH_REG_FIFO_BATCH_ADDR	0x09
#define ST_ASM330LHH_REG_FIFO_CTRL4_ADDR	0x0a
#define ST_ASM330LHH_REG_STATUS_ADDR		0x1e
#define ST_ASM330LHH_REG_STATUS_TDA		BIT(2)
#define ST_ASM330LHH_REG_OUT_TEMP_L_ADDR	0x20
#define ST_ASM330LHH_REG_OUT_TEMP_H_ADDR	0x21

#define ST_ASM330LHH_MAX_ODR			416

/* Define Custom events for FIFO flush */
#define CUSTOM_IIO_EV_DIR_FIFO_EMPTY (IIO_EV_DIR_NONE + 1)
#define CUSTOM_IIO_EV_DIR_FIFO_DATA (IIO_EV_DIR_NONE + 2)
#define CUSTOM_IIO_EV_TYPE_FIFO_FLUSH (IIO_EV_TYPE_CHANGE + 1)

#define ST_ASM330LHH_CHANNEL(chan_type, addr, mod, ch2, scan_idx,	\
			   rb, sb, sg)					\
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

#define ST_ASM330LHH_FLUSH_CHANNEL(dtype)		\
{							\
	.type = dtype,					\
	.modified = 0,					\
	.scan_index = -1,				\
	.indexed = -1,					\
	.event_spec = &st_asm330lhh_flush_event,	\
	.num_event_specs = 1,				\
}

#define ST_ASM330LHH_RX_MAX_LENGTH	8
#define ST_ASM330LHH_TX_MAX_LENGTH	8

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
	int (*write)(struct device *dev, u8 addr, int len, u8 *data);
};

struct st_asm330lhh_reg {
	u8 addr;
	u8 mask;
};

struct st_asm330lhh_odr {
	u16 hz;
	u8 val;
};

#define ST_ASM330LHH_ODR_LIST_SIZE	7
struct st_asm330lhh_odr_table_entry {
	struct st_asm330lhh_reg reg;
	struct st_asm330lhh_odr odr_avl[ST_ASM330LHH_ODR_LIST_SIZE];
};

struct st_asm330lhh_fs {
	u32 gain;
	u8 val;
};

#define ST_ASM330LHH_FS_ACC_LIST_SIZE		4
#define ST_ASM330LHH_FS_GYRO_LIST_SIZE		6
#define ST_ASM330LHH_FS_TEMP_LIST_SIZE		1
#define ST_ASM330LHH_FS_LIST_SIZE		6
struct st_asm330lhh_fs_table_entry {
	u32 size;
	struct st_asm330lhh_reg reg;
	struct st_asm330lhh_fs fs_avl[ST_ASM330LHH_FS_LIST_SIZE];
};

enum st_asm330lhh_sensor_id {
	ST_ASM330LHH_ID_ACC,
	ST_ASM330LHH_ID_GYRO,
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
 * @batch_mask: Sensor mask for FIFO batching register
 */
struct st_asm330lhh_sensor {
	enum st_asm330lhh_sensor_id id;
	struct st_asm330lhh_hw *hw;

	u32 gain;
	u16 odr;
	u32 offset;

	__le16 old_data;

	u8 std_samples;
	u8 std_level;

	u16 watermark;
	u8 batch_mask;
	u8 batch_addr;
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
#endif
};

/**
 * struct st_asm330lhh_hw - ST IMU MEMS hw instance
 * @dev: Pointer to instance of struct device (I2C or SPI).
 * @irq: Device interrupt line (I2C or SPI).
 * @lock: Mutex to protect read and write operations.
 * @fifo_lock: Mutex to prevent concurrent access to the hw FIFO.
 * @fifo_mode: FIFO operating mode supported by the device.
 * @state: hw operational state.
 * @enable_mask: Enabled sensor bitmask.
 * @ts_offset: Hw timestamp offset.
 * @hw_ts: Latest hw timestamp from the sensor.
 * @ts: Latest timestamp from irq handler.
 * @delta_ts: Delta time between two consecutive interrupts.
 * @iio_devs: Pointers to acc/gyro iio_dev instances.
 * @tf: Transfer function structure used by I/O operations.
 * @tb: Transfer buffers used by SPI I/O operations.
 */
struct st_asm330lhh_hw {
	struct device *dev;
	int irq;

	struct mutex lock;
	struct mutex fifo_lock;

	enum st_asm330lhh_fifo_mode fifo_mode;
	unsigned long state;
	u8 enable_mask;

	s64 ts_offset;
	u32 hw_val;
	u32 hw_val_old;
	s64 hw_ts;
	s64 hw_ts_high;
	s64 delta_ts;
	s64 ts;
	s64 tsample;
	s64 hw_ts_old;
	s64 delta_hw_ts;

	/* Timestamp sample ODR */
	u16 odr;

	struct iio_dev *iio_devs[ST_ASM330LHH_ID_MAX];

	const struct st_asm330lhh_transfer_function *tf;
	struct st_asm330lhh_transfer_buffer tb;
	struct regulator *vdd;
	struct regulator *vio;
};

extern const struct dev_pm_ops st_asm330lhh_pm_ops;

int st_asm330lhh_probe(struct device *dev, int irq,
		       const struct st_asm330lhh_transfer_function *tf_ops);
int st_asm330lhh_sensor_set_enable(struct st_asm330lhh_sensor *sensor,
				   bool enable);
int st_asm330lhh_fifo_setup(struct st_asm330lhh_hw *hw);
int st_asm330lhh_write_with_mask(struct st_asm330lhh_hw *hw, u8 addr, u8 mask,
				 u8 val);
int st_asm330lhh_get_odr_val(enum st_asm330lhh_sensor_id id, u16 odr, u8 *val);
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
int st_asm330lhh_set_fifo_mode(struct st_asm330lhh_hw *hw,
			       enum st_asm330lhh_fifo_mode fifo_mode);
int st_asm330lhh_suspend_fifo(struct st_asm330lhh_hw *hw);
int st_asm330lhh_update_watermark(struct st_asm330lhh_sensor *sensor,
					u16 watermark);
int st_asm330lhh_update_fifo(struct iio_dev *iio_dev, bool enable);
int asm330_check_acc_gyro_early_buff_enable_flag(
		struct st_asm330lhh_sensor *sensor);
#endif /* ST_ASM330LHH_H */

/*
 * STMicroelectronics st_asm330lhhx sensor driver
 *
 * Copyright 2019 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 * Tesi Mario <mario.tesi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_ASM330LHHX_H
#define ST_ASM330LHHX_H

#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/bitfield.h>

#define ST_ASM330LHHX_ODR_EXPAND(odr, uodr)	((odr * 1000000) + uodr)

#define ST_ASM330LHHX_DEV_NAME		"asm330lhhx"
#define ST_ASM330LHHX_DRV_VERSION	"1.1"

/* FIFO simple size and depth */
#define ST_ASM330LHHX_SAMPLE_SIZE	6
#define ST_ASM330LHHX_TS_SAMPLE_SIZE	4
#define ST_ASM330LHHX_TAG_SIZE		1
#define ST_ASM330LHHX_FIFO_SAMPLE_SIZE	(ST_ASM330LHHX_SAMPLE_SIZE + \
					 ST_ASM330LHHX_TAG_SIZE)
#define ST_ASM330LHHX_MAX_FIFO_DEPTH	416

#define ST_ASM330LHHX_DATA_CHANNEL(chan_type, addr, mod, ch2, scan_idx,	\
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

static const struct iio_event_spec st_asm330lhhx_flush_event = {
	.type = IIO_EV_TYPE_FIFO_FLUSH,
	.dir = IIO_EV_DIR_EITHER,
};

static const struct iio_event_spec st_asm330lhhx_thr_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
	.mask_separate = BIT(IIO_EV_INFO_ENABLE),
};

#define ST_ASM330LHHX_EVENT_CHANNEL(ctype, etype)	\
{							\
	.type = ctype,					\
	.modified = 0,					\
	.scan_index = -1,				\
	.indexed = -1,					\
	.event_spec = &st_asm330lhhx_##etype##_event,	\
	.num_event_specs = 1,				\
}

#define ST_ASM330LHHX_SHIFT_VAL(val, mask)	(((val) << __ffs(mask)) & (mask))

enum st_asm330lhhx_pm_t {
	ST_ASM330LHHX_HP_MODE = 0,
	ST_ASM330LHHX_LP_MODE,
	ST_ASM330LHHX_NO_MODE,
};

#ifdef CONFIG_IIO_ST_ASM330LHHX_MLC
enum st_asm330lhhx_fsm_mlc_enable_id {
	ST_ASM330LHHX_MLC_FSM_DISABLED = 0,
	ST_ASM330LHHX_MLC_ENABLED = BIT(0),
	ST_ASM330LHHX_FSM_ENABLED = BIT(1),
};

/**
 * struct mlc_config_t -
 * @mlc_int_addr: interrupt register address
 * @mlc_int_mask: interrupt register mask
 * @fsm_int_addr: interrupt register address
 * @fsm_int_mask: interrupt register mask
 * @mlc_configured: number of mlc configured
 * @fsm_configured: number of fsm configured
 * @bin_len: fw binary size
 */
struct st_asm330lhhx_mlc_config_t {
	uint8_t mlc_int_addr;
	uint8_t mlc_int_mask;
	uint8_t fsm_int_addr[2];
	uint8_t fsm_int_mask[2];
	uint8_t mlc_configured;
	uint8_t fsm_configured;
	uint16_t bin_len;
	enum st_asm330lhhx_fsm_mlc_enable_id status;
};
#endif /* CONFIG_IIO_ST_ASM330LHHX_MLC */

/**
 * struct st_asm330lhhx_reg - Generic sensor register description (addr + mask)
 * @addr: Address of register.
 * @mask: Bitmask register for proper usage.
 */
struct st_asm330lhhx_reg {
	u8 addr;
	u8 mask;
};

/**
 * struct st_asm330lhhx_odr - Single ODR entry
 * @hz: Most significant part of the sensor ODR (Hz).
 * @uhz: Less significant part of the sensor ODR (micro Hz).
 * @val: ODR register value.
 * @batch_val: Batching ODR register value.
 */
struct st_asm330lhhx_odr {
	u16 hz;
	u32 uhz;
	u8 val;
	u8 batch_val;
};

/**
 * struct st_asm330lhhx_odr_table_entry - Sensor ODR table
 * @size: Size of ODR table.
 * @reg: ODR register.
 * @pm: Power mode register.
 * @batching_reg: ODR register for batching on fifo.
 * @odr_avl: Array of supported ODR value.
 */
struct st_asm330lhhx_odr_table_entry {
	u8 size;
	struct st_asm330lhhx_reg reg;
	struct st_asm330lhhx_reg pm;
	struct st_asm330lhhx_reg batching_reg;
	struct st_asm330lhhx_odr odr_avl[8];
};

/**
 * struct st_asm330lhhx_fs - Full Scale sensor table entry
 * @reg: Register description for FS settings.
 * @gain: Sensor sensitivity (mdps/LSB, mg/LSB and uC/LSB).
 * @val: FS register value.
 */
struct st_asm330lhhx_fs {
	struct st_asm330lhhx_reg reg;
	u32 gain;
	u8 val;
};

/**
 * struct st_asm330lhhx_fs_table_entry - Full Scale sensor table
 * @size: Full Scale sensor table size.
 * @fs_avl: Full Scale list entries.
 */
struct st_asm330lhhx_fs_table_entry {
	u8 size;
	struct st_asm330lhhx_fs fs_avl[5];
};

enum st_asm330lhhx_sensor_id {
	ST_ASM330LHHX_ID_GYRO = 0,
	ST_ASM330LHHX_ID_ACC,
	ST_ASM330LHHX_ID_TEMP,
#ifdef CONFIG_IIO_ST_ASM330LHHX_MLC
	ST_ASM330LHHX_ID_MLC,
	ST_ASM330LHHX_ID_MLC_0,
	ST_ASM330LHHX_ID_MLC_1,
	ST_ASM330LHHX_ID_MLC_2,
	ST_ASM330LHHX_ID_MLC_3,
	ST_ASM330LHHX_ID_MLC_4,
	ST_ASM330LHHX_ID_MLC_5,
	ST_ASM330LHHX_ID_MLC_6,
	ST_ASM330LHHX_ID_MLC_7,
	ST_ASM330LHHX_ID_FSM_0,
	ST_ASM330LHHX_ID_FSM_1,
	ST_ASM330LHHX_ID_FSM_2,
	ST_ASM330LHHX_ID_FSM_3,
	ST_ASM330LHHX_ID_FSM_4,
	ST_ASM330LHHX_ID_FSM_5,
	ST_ASM330LHHX_ID_FSM_6,
	ST_ASM330LHHX_ID_FSM_7,
	ST_ASM330LHHX_ID_FSM_8,
	ST_ASM330LHHX_ID_FSM_9,
	ST_ASM330LHHX_ID_FSM_10,
	ST_ASM330LHHX_ID_FSM_11,
	ST_ASM330LHHX_ID_FSM_12,
	ST_ASM330LHHX_ID_FSM_13,
	ST_ASM330LHHX_ID_FSM_14,
	ST_ASM330LHHX_ID_FSM_15,
#endif /* CONFIG_IIO_ST_ASM330LHHX_MLC */
	ST_ASM330LHHX_ID_MAX,
};

static const enum st_asm330lhhx_sensor_id st_asm330lhhx_mlc_sensor_list[] = {
	 [0] = ST_ASM330LHHX_ID_MLC_0,
	 [1] = ST_ASM330LHHX_ID_MLC_1,
	 [2] = ST_ASM330LHHX_ID_MLC_2,
	 [3] = ST_ASM330LHHX_ID_MLC_3,
	 [4] = ST_ASM330LHHX_ID_MLC_4,
	 [5] = ST_ASM330LHHX_ID_MLC_5,
	 [6] = ST_ASM330LHHX_ID_MLC_6,
	 [7] = ST_ASM330LHHX_ID_MLC_7,
};

static const enum st_asm330lhhx_sensor_id st_asm330lhhx_fsm_sensor_list[] = {
	 [0] = ST_ASM330LHHX_ID_FSM_0,
	 [1] = ST_ASM330LHHX_ID_FSM_1,
	 [2] = ST_ASM330LHHX_ID_FSM_2,
	 [3] = ST_ASM330LHHX_ID_FSM_3,
	 [4] = ST_ASM330LHHX_ID_FSM_4,
	 [5] = ST_ASM330LHHX_ID_FSM_5,
	 [6] = ST_ASM330LHHX_ID_FSM_6,
	 [7] = ST_ASM330LHHX_ID_FSM_7,
	 [8] = ST_ASM330LHHX_ID_FSM_8,
	 [9] = ST_ASM330LHHX_ID_FSM_9,
	 [10] = ST_ASM330LHHX_ID_FSM_10,
	 [11] = ST_ASM330LHHX_ID_FSM_11,
	 [12] = ST_ASM330LHHX_ID_FSM_12,
	 [13] = ST_ASM330LHHX_ID_FSM_13,
	 [14] = ST_ASM330LHHX_ID_FSM_14,
	 [15] = ST_ASM330LHHX_ID_FSM_15,
};

enum st_asm330lhhx_fifo_mode {
	ST_ASM330LHHX_FIFO_BYPASS = 0x0,
	ST_ASM330LHHX_FIFO_CONT = 0x6,
};

enum {
	ST_ASM330LHHX_HW_FLUSH,
	ST_ASM330LHHX_HW_OPERATIONAL,
};

/**
 * struct st_asm330lhhx_sensor - ST IMU sensor instance
 * @name: Sensor name.
 * @id: Sensor identifier.
 * @hw: Pointer to instance of struct st_asm330lhhx_hw.
 * @gain: Configured sensor sensitivity.
 * @offset: Sensor data offset.
 * @odr: Output data rate of the sensor [Hz].
 * @uodr: Output data rate of the sensor [uHz].
 * @old_data: Used by Temperature sensor for data comtinuity.
 * @max_watermark: Max supported watermark level.
 * @watermark: Sensor watermark level.
 * @pm: sensor power mode (HP, LP).
 */
struct st_asm330lhhx_sensor {
	char name[32];
	enum st_asm330lhhx_sensor_id id;
	struct st_asm330lhhx_hw *hw;
	union {
		struct {
			u32 gain;
			u32 offset;
			int odr;
			int uodr;
			__le16 old_data;
			u16 max_watermark;
			u16 watermark;
			u8 pm;
			s64 last_fifo_timestamp;
		};
		struct {
			uint8_t status_reg;
			uint8_t outreg_addr;
			enum st_asm330lhhx_fsm_mlc_enable_id status;
		};
	};
};

/**
 * struct st_asm330lhhx_hw - ST IMU MEMS hw instance
 * @dev: Pointer to instance of struct device (I2C or SPI).
 * @irq: Device interrupt line (I2C or SPI).
 * @regmap: Register map of the device.
 * @fifo_lock: Mutex to prevent concurrent access to the hw FIFO.
 * @fifo_mode: FIFO operating mode supported by the device.
 * @state: hw operational state.
 * @enable_mask: Enabled sensor bitmask.
 * @requested_mask: Sensor requesting bitmask.
 * @ts_delta_ns: Calibrated delta timestamp.
 * @ts_offset: Hw timestamp offset.
 * @hw_ts: Latest hw timestamp from the sensor.
 * @tsample: Sample timestamp.
 * @delta_ts: Delta time between two consecutive interrupts.
 * @ts: Latest timestamp from irq handler.
 * @mlc_config:
 * @odr_table_entry: Sensors ODR table.
 * @iio_devs: Pointers to acc/gyro iio_dev instances.
 */
struct st_asm330lhhx_hw {
	struct device *dev;
	int irq;

	struct regmap *regmap;
	struct mutex fifo_lock;
	struct mutex page_lock;

	enum st_asm330lhhx_fifo_mode fifo_mode;
	unsigned long state;
	u32 enable_mask;
	u32 requested_mask;

	s64 ts_offset;
	u64 ts_delta_ns;
	s64 hw_ts;
	u32 val_ts_old;
	u32 hw_ts_high;
	s64 tsample;
	s64 delta_ts;
	s64 ts;
	struct st_asm330lhhx_mlc_config_t *mlc_config;
	const struct st_asm330lhhx_odr_table_entry *odr_table_entry;
	struct iio_dev *iio_devs[ST_ASM330LHHX_ID_MAX];
};

extern const struct dev_pm_ops st_asm330lhhx_pm_ops;

static inline bool st_asm330lhhx_is_fifo_enabled(struct st_asm330lhhx_hw *hw)
{
	return hw->enable_mask & (BIT(ST_ASM330LHHX_ID_GYRO) |
				  BIT(ST_ASM330LHHX_ID_ACC));
}

static inline int
st_asm330lhhx_update_bits_locked(struct st_asm330lhhx_hw *hw, unsigned int addr,
			      unsigned int mask, unsigned int val)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = regmap_update_bits(hw->regmap, addr, mask, val);
	mutex_unlock(&hw->page_lock);

	return err;
}

static inline int
st_asm330lhhx_read_locked(struct st_asm330lhhx_hw *hw, unsigned int addr,
		       void *val, unsigned int len)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = regmap_bulk_read(hw->regmap, addr, val, len);
	mutex_unlock(&hw->page_lock);

	return err;
}

static inline int
st_asm330lhhx_write_locked(struct st_asm330lhhx_hw *hw, unsigned int addr,
			unsigned int val)
{
	int err;

	mutex_lock(&hw->page_lock);
	err = regmap_write(hw->regmap, addr, val);
	mutex_unlock(&hw->page_lock);

	return err;
}

int st_asm330lhhx_probe(struct device *dev, int irq,
		       struct regmap *regmap);
int st_asm330lhhx_sensor_set_enable(struct st_asm330lhhx_sensor *sensor,
				   bool enable);
int st_asm330lhhx_buffers_setup(struct st_asm330lhhx_hw *hw);
int st_asm330lhhx_get_odr_val(struct st_asm330lhhx_sensor *sensor, int odr,
			     int uodr, int *podr, int *puodr, u8 *val);
int st_asm330lhhx_get_batch_val(struct st_asm330lhhx_sensor *sensor, int odr,
			       int uodr, u8 *val);
int st_asm330lhhx_update_watermark(struct st_asm330lhhx_sensor *sensor,
				  u16 watermark);
ssize_t st_asm330lhhx_flush_fifo(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size);
ssize_t st_asm330lhhx_get_max_watermark(struct device *dev,
				       struct device_attribute *attr, char *buf);
ssize_t st_asm330lhhx_get_watermark(struct device *dev,
				   struct device_attribute *attr, char *buf);
ssize_t st_asm330lhhx_set_watermark(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size);
int st_asm330lhhx_suspend_fifo(struct st_asm330lhhx_hw *hw);
int st_asm330lhhx_set_fifo_mode(struct st_asm330lhhx_hw *hw,
			       enum st_asm330lhhx_fifo_mode fifo_mode);
int __st_asm330lhhx_set_sensor_batching_odr(struct st_asm330lhhx_sensor *sensor,
					   bool enable);
int st_asm330lhhx_update_batching(struct iio_dev *iio_dev, bool enable);
int st_asm330lhhx_of_get_pin(struct st_asm330lhhx_hw *hw, int *pin);
#ifdef CONFIG_IIO_ST_ASM330LHHX_MLC
int st_asm330lhhx_mlc_probe(struct st_asm330lhhx_hw *hw);
int st_asm330lhhx_mlc_remove(struct device *dev);
int st_asm330lhhx_mlc_check_status(struct st_asm330lhhx_hw *hw);
#endif /* CONFIG_IIO_ST_ASM330LHHX_MLC */
#endif /* ST_ASM330LHHX_H */

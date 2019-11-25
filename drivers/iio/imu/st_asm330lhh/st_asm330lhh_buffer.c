/*
 * STMicroelectronics st_asm330lhh FIFO buffer library driver
 *
 * Copyright 2018 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <asm/unaligned.h>
#include <linux/of.h>
#include <asm/arch_timer.h>

#include "st_asm330lhh.h"

#define ST_ASM330LHH_REG_FIFO_THL_ADDR		0x07
#define ST_ASM330LHH_REG_FIFO_LEN_MASK		GENMASK(8, 0)
#define ST_ASM330LHH_REG_FIFO_MODE_MASK		GENMASK(2, 0)
#define ST_ASM330LHH_REG_DEC_TS_MASK		GENMASK(7, 6)
#define ST_ASM330LHH_REG_HLACTIVE_ADDR		0x12
#define ST_ASM330LHH_REG_HLACTIVE_MASK		BIT(5)
#define ST_ASM330LHH_REG_PP_OD_ADDR		0x12
#define ST_ASM330LHH_REG_PP_OD_MASK		BIT(4)
#define ST_ASM330LHH_REG_FIFO_DIFFL_ADDR	0x3a
#define ST_ASM330LHH_REG_TS0_ADDR		0x40
#define ST_ASM330LHH_REG_TS2_ADDR		0x42
#define ST_ASM330LHH_REG_FIFO_OUT_TAG_ADDR	0x78
#define ST_ASM330LHH_GYRO_TAG			0x01
#define ST_ASM330LHH_ACC_TAG			0x02
#define ST_ASM330LHH_TS_TAG			0x04

#define ST_ASM330LHH_TS_DELTA_NS		25000ULL /* 25us/LSB */
#define QTIMER_DIV				192
#define QTIMER_MUL				10000

static int asm330_use_qtimer;

static inline u64 qTimerTime(void)
{
	u64 qTCount = 0;

	qTCount = arch_counter_get_cntvct();

	return mul_u64_u32_div(qTCount, QTIMER_MUL, QTIMER_DIV);
}

static inline s64 st_asm330lhh_get_time_ns(void)
{
	struct timespec ts;

	/* if enabled, use qtimer instead of monotonic timestamp */
	if (asm330_use_qtimer)
		return (s64)qTimerTime();

	get_monotonic_boottime(&ts);
	return timespec_to_ns(&ts);
}

#define ST_ASM330LHH_EWMA_LEVEL			120
#define ST_ASM330LHH_EWMA_DIV			128
static inline s64 st_asm330lhh_ewma(s64 old, s64 new, int weight)
{
	s64 diff, incr;

	diff = new - old;
	incr = div_s64((ST_ASM330LHH_EWMA_DIV - weight) * diff,
		       ST_ASM330LHH_EWMA_DIV);

	return old + incr;
}

static inline int st_asm330lhh_reset_hwts(struct st_asm330lhh_hw *hw)
{
	u8 data = 0xaa;

	hw->ts = st_asm330lhh_get_time_ns();
	hw->ts_offset = hw->ts;
	hw->hw_ts_old = 0ull;
	hw->tsample = 0ull;
	hw->hw_ts_high = 0ull;
	hw->hw_val_old = 0ull;

	return hw->tf->write(hw->dev, ST_ASM330LHH_REG_TS2_ADDR, sizeof(data),
			     &data);
}

int st_asm330lhh_set_fifo_mode(struct st_asm330lhh_hw *hw,
			       enum st_asm330lhh_fifo_mode fifo_mode)
{
	int err;

	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_FIFO_CTRL4_ADDR,
					   ST_ASM330LHH_REG_FIFO_MODE_MASK,
					   fifo_mode);
	if (err < 0)
		return err;

	hw->fifo_mode = fifo_mode;

	return 0;
}

static int st_asm330lhh_set_sensor_batching_odr(struct st_asm330lhh_sensor *sensor,
						bool enable)
{
	struct st_asm330lhh_hw *hw = sensor->hw;
	u8 data = 0;
	int err;

	if (enable) {
		err = st_asm330lhh_get_odr_val(sensor->id, sensor->odr, &data);
		if (err < 0)
			return err;
	}

	return st_asm330lhh_write_with_mask(hw,
					    sensor->batch_addr,
					    sensor->batch_mask, data);
}

static u16 st_asm330lhh_ts_odr(struct st_asm330lhh_hw *hw)
{
	struct st_asm330lhh_sensor *sensor;
	u16 odr = 0;
	u8 i;

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		sensor = iio_priv(hw->iio_devs[i]);
		if (hw->enable_mask & BIT(sensor->id))
			odr = max_t(u16, odr, sensor->odr);
	}

	return odr;
}

int st_asm330lhh_update_watermark(struct st_asm330lhh_sensor *sensor,
					 u16 watermark)
{
	u16 fifo_watermark = ST_ASM330LHH_MAX_FIFO_DEPTH, cur_watermark = 0;
	struct st_asm330lhh_hw *hw = sensor->hw;
	struct st_asm330lhh_sensor *cur_sensor;
	__le16 wdata;
	int i, err;
	u8 data;

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		cur_sensor = iio_priv(hw->iio_devs[i]);

		if (!(hw->enable_mask & BIT(cur_sensor->id)))
			continue;

		cur_watermark = (cur_sensor == sensor) ? watermark
						       : cur_sensor->watermark;

		fifo_watermark = min_t(u16, fifo_watermark, cur_watermark);
	}

	fifo_watermark = max_t(u16, fifo_watermark, 2);
	mutex_lock(&hw->lock);

	err = hw->tf->read(hw->dev, ST_ASM330LHH_REG_FIFO_THL_ADDR + 1,
			   sizeof(data), &data);
	if (err < 0)
		goto out;

	fifo_watermark = ((data << 8) & ~ST_ASM330LHH_REG_FIFO_LEN_MASK) |
			 (fifo_watermark & ST_ASM330LHH_REG_FIFO_LEN_MASK);
	wdata = cpu_to_le16(fifo_watermark);
	err = hw->tf->write(hw->dev, ST_ASM330LHH_REG_FIFO_THL_ADDR,
			    sizeof(wdata), (u8 *)&wdata);

out:
	mutex_unlock(&hw->lock);

	return err < 0 ? err : 0;
}

static inline void st_asm330lhh_sync_hw_ts(struct st_asm330lhh_hw *hw, s64 ts)
{
	s64 delta = ts - hw->hw_ts;

	hw->ts_offset = st_asm330lhh_ewma(hw->ts_offset, delta,
					  ST_ASM330LHH_EWMA_LEVEL);
}

static struct iio_dev *st_asm330lhh_get_iiodev_from_tag(struct st_asm330lhh_hw *hw,
							u8 tag)
{
	struct iio_dev *iio_dev;

	switch (tag) {
	case ST_ASM330LHH_GYRO_TAG:
		iio_dev = hw->iio_devs[ST_ASM330LHH_ID_GYRO];
		break;
	case ST_ASM330LHH_ACC_TAG:
		iio_dev = hw->iio_devs[ST_ASM330LHH_ID_ACC];
		break;
	default:
		iio_dev = NULL;
		break;
	}

	return iio_dev;
}
#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
int asm330_check_acc_gyro_early_buff_enable_flag(
		struct st_asm330lhh_sensor *sensor)
{
	if (sensor->buffer_asm_samples == true)
		return 1;
	else
		return 0;
}
#else
int asm330_check_acc_gyro_early_buff_enable_flag(
		struct st_asm330lhh_sensor *sensor)
{
	return 0;
}
#endif

#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
static void store_acc_gyro_boot_sample(struct st_asm330lhh_sensor *sensor,
					u8 *iio_buf, s64 tsample)
{
	int x, y, z;

	if (false == sensor->buffer_asm_samples)
		return;

	sensor->timestamp = (ktime_t)tsample;
	x = iio_buf[1]<<8|iio_buf[0];
	y = iio_buf[3]<<8|iio_buf[2];
	z = iio_buf[5]<<8|iio_buf[4];

	if (ktime_to_timespec(sensor->timestamp).tv_sec
			<  sensor->max_buffer_time) {
		if (sensor->bufsample_cnt < ASM_MAXSAMPLE) {
			sensor->asm_samplist[sensor->bufsample_cnt]->xyz[0] = x;
			sensor->asm_samplist[sensor->bufsample_cnt]->xyz[1] = y;
			sensor->asm_samplist[sensor->bufsample_cnt]->xyz[2] = z;
			sensor->asm_samplist[sensor->bufsample_cnt]->tsec =
				ktime_to_timespec(sensor->timestamp).tv_sec;
			sensor->asm_samplist[sensor->bufsample_cnt]->tnsec =
				ktime_to_timespec(sensor->timestamp).tv_nsec;
			sensor->bufsample_cnt++;
		}
	} else {
		dev_info(sensor->hw->dev, "End of sensor %d buffering %d\n",
				sensor->id, sensor->bufsample_cnt);
		sensor->buffer_asm_samples = false;
	}
}
#else
static void store_acc_gyro_boot_sample(struct st_asm330lhh_sensor *sensor,
					u8 *iio_buf, s64 tsample)
{
}
#endif

static int st_asm330lhh_read_fifo(struct st_asm330lhh_hw *hw)
{
	u8 iio_buf[ALIGN(ST_ASM330LHH_SAMPLE_SIZE, sizeof(s64)) + sizeof(s64)];
	u8 buf[30 * ST_ASM330LHH_FIFO_SAMPLE_SIZE], tag, *ptr;
	s64 ts_delta_hw_ts = 0, ts_irq;
	s64 ts_delta_offs;
	int i, err, read_len, word_len, fifo_len;
	struct st_asm330lhh_sensor *sensor;
	struct iio_dev *iio_dev;
	__le16 fifo_status;
	u16 fifo_depth;
	int ts_processed = 0;
	s64 hw_ts = 0ull, delta_hw_ts, cpu_timestamp;

	ts_irq = hw->ts - hw->delta_ts;

	do {
		err = hw->tf->read(hw->dev, ST_ASM330LHH_REG_FIFO_DIFFL_ADDR,
				   sizeof(fifo_status), (u8 *)&fifo_status);
		if (err < 0)
			return err;

		fifo_depth = le16_to_cpu(fifo_status) & ST_ASM330LHH_REG_FIFO_LEN_MASK;
		if (!fifo_depth)
			return 0;

		read_len = 0;
		fifo_len = fifo_depth * ST_ASM330LHH_FIFO_SAMPLE_SIZE;
		while (read_len < fifo_len) {
			word_len = min_t(int, fifo_len - read_len, sizeof(buf));
			err = hw->tf->read(hw->dev,
					   ST_ASM330LHH_REG_FIFO_OUT_TAG_ADDR,
					   word_len, buf);
			if (err < 0)
				return err;

			for (i = 0; i < word_len; i += ST_ASM330LHH_FIFO_SAMPLE_SIZE) {
				ptr = &buf[i + ST_ASM330LHH_TAG_SIZE];
				tag = buf[i] >> 3;

				if (tag == ST_ASM330LHH_TS_TAG) {
					hw->hw_val = get_unaligned_le32(ptr);

					/* check for timer rollover */
					if (hw->hw_val < hw->hw_val_old)
						hw->hw_ts_high++;
					hw->hw_ts =
					(hw->hw_val + (hw->hw_ts_high << 32))
						* ST_ASM330LHH_TS_DELTA_NS;
					ts_delta_hw_ts = hw->hw_ts - hw->hw_ts_old;
					hw_ts += ts_delta_hw_ts;
					ts_delta_offs =
						div_s64(hw->delta_hw_ts * ST_ASM330LHH_MAX_ODR, hw->odr);

					hw->ts_offset = st_asm330lhh_ewma(hw->ts_offset, ts_irq -
						hw->hw_ts + ts_delta_offs, ST_ASM330LHH_EWMA_LEVEL);

					ts_irq += (hw->hw_ts + ts_delta_offs);
					hw->hw_ts_old = hw->hw_ts;
					hw->hw_val_old = hw->hw_val;
					ts_processed++;

					if (!hw->tsample)
						hw->tsample =
							hw->ts_offset + (hw->hw_ts + ts_delta_offs);
					else
						hw->tsample =
							hw->tsample + (ts_delta_hw_ts + ts_delta_offs);
				} else {
					iio_dev = st_asm330lhh_get_iiodev_from_tag(hw, tag);
					if (!iio_dev)
						continue;

					sensor = iio_priv(iio_dev);
					if (sensor->std_samples < sensor->std_level) {
						sensor->std_samples++;
						continue;
					}

					sensor = iio_priv(iio_dev);

					/* Check if timestamp is in the future. */
					cpu_timestamp = st_asm330lhh_get_time_ns();

					/* Avoid samples in the future. */
					if (hw->tsample > cpu_timestamp)
						hw->tsample = cpu_timestamp;

					memcpy(iio_buf, ptr, ST_ASM330LHH_SAMPLE_SIZE);
					iio_push_to_buffers_with_timestamp(iio_dev,
									   iio_buf,
									   hw->tsample);
					store_acc_gyro_boot_sample(sensor,
							iio_buf, hw->tsample);
				}
			}
			read_len += word_len;
		}

		delta_hw_ts = div_s64(hw->delta_ts - hw_ts, ts_processed);
		delta_hw_ts = div_s64(delta_hw_ts * hw->odr, ST_ASM330LHH_MAX_ODR);
		hw->delta_hw_ts = st_asm330lhh_ewma(hw->delta_hw_ts,
							delta_hw_ts,
							ST_ASM330LHH_EWMA_LEVEL);
	} while(read_len);

	return read_len;
}

ssize_t st_asm330lhh_get_max_watermark(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ST_ASM330LHH_MAX_FIFO_DEPTH);
}

ssize_t st_asm330lhh_get_watermark(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);

	return sprintf(buf, "%d\n", sensor->watermark);
}

ssize_t st_asm330lhh_set_watermark(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	int err, val;

	if (asm330_check_acc_gyro_early_buff_enable_flag(sensor))
		return -EBUSY;

	mutex_lock(&iio_dev->mlock);
	if (iio_buffer_enabled(iio_dev)) {
		err = -EBUSY;
		goto out;
	}

	err = kstrtoint(buf, 10, &val);
	if (err < 0)
		goto out;

	err = st_asm330lhh_update_watermark(sensor, val);
	if (err < 0)
		goto out;

	sensor->watermark = val;

out:
	mutex_unlock(&iio_dev->mlock);

	return err < 0 ? err : size;
}

ssize_t st_asm330lhh_flush_fifo(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	struct st_asm330lhh_hw *hw = sensor->hw;
	s64 type, event;
	int count;
	s64 ts;

	mutex_lock(&hw->fifo_lock);
	ts = st_asm330lhh_get_time_ns();
	hw->delta_ts = ts - hw->ts;
	hw->ts = ts;
	set_bit(ST_ASM330LHH_HW_FLUSH, &hw->state);

	count = st_asm330lhh_read_fifo(hw);

	mutex_unlock(&hw->fifo_lock);

	type = count > 0 ? CUSTOM_IIO_EV_DIR_FIFO_DATA : CUSTOM_IIO_EV_DIR_FIFO_EMPTY;
	event = IIO_UNMOD_EVENT_CODE(iio_dev->channels[0].type, -1,
				     CUSTOM_IIO_EV_TYPE_FIFO_FLUSH, type);
	iio_push_event(iio_dev, event, st_asm330lhh_get_time_ns());

	return size;
}

int st_asm330lhh_suspend_fifo(struct st_asm330lhh_hw *hw)
{
	int err;

	mutex_lock(&hw->fifo_lock);

	st_asm330lhh_read_fifo(hw);
	err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_BYPASS);

	mutex_unlock(&hw->fifo_lock);

	return err;
}

int st_asm330lhh_update_fifo(struct iio_dev *iio_dev, bool enable)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	struct st_asm330lhh_hw *hw = sensor->hw;
	int err;

	mutex_lock(&hw->fifo_lock);

	err = st_asm330lhh_sensor_set_enable(sensor, enable);
	if (err < 0)
		goto out;

	err = st_asm330lhh_set_sensor_batching_odr(sensor, enable);
	if (err < 0)
		goto out;

	err = st_asm330lhh_update_watermark(sensor, sensor->watermark);
	if (err < 0)
		goto out;

	hw->odr = st_asm330lhh_ts_odr(hw);

	if (enable && hw->fifo_mode == ST_ASM330LHH_FIFO_BYPASS) {
		st_asm330lhh_reset_hwts(hw);
		err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_CONT);
	} else if (!hw->enable_mask) {
		err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_BYPASS);
	}

out:
	mutex_unlock(&hw->fifo_lock);

	return err;
}

static irqreturn_t st_asm330lhh_handler_irq(int irq, void *private)
{
	struct st_asm330lhh_hw *hw = (struct st_asm330lhh_hw *)private;
	s64 ts = st_asm330lhh_get_time_ns();

	hw->delta_ts = ts - hw->ts;
	hw->ts = ts;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t st_asm330lhh_handler_thread(int irq, void *private)
{
	struct st_asm330lhh_hw *hw = (struct st_asm330lhh_hw *)private;

	mutex_lock(&hw->fifo_lock);

	st_asm330lhh_read_fifo(hw);
	clear_bit(ST_ASM330LHH_HW_FLUSH, &hw->state);

	mutex_unlock(&hw->fifo_lock);

	return IRQ_HANDLED;
}

static int st_asm330lhh_buffer_preenable(struct iio_dev *iio_dev)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);

	if (asm330_check_acc_gyro_early_buff_enable_flag(sensor))
		return 0;
	else
		return st_asm330lhh_update_fifo(iio_dev, true);
}

static int st_asm330lhh_buffer_postdisable(struct iio_dev *iio_dev)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);

	if (asm330_check_acc_gyro_early_buff_enable_flag(sensor))
		return 0;
	else
		return st_asm330lhh_update_fifo(iio_dev, false);
}

static const struct iio_buffer_setup_ops st_asm330lhh_buffer_ops = {
	.preenable = st_asm330lhh_buffer_preenable,
	.postdisable = st_asm330lhh_buffer_postdisable,
};

static int st_asm330lhh_fifo_init(struct st_asm330lhh_hw *hw)
{
	return st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_FIFO_CTRL4_ADDR,
					    ST_ASM330LHH_REG_DEC_TS_MASK, 1);
}

int st_asm330lhh_fifo_setup(struct st_asm330lhh_hw *hw)
{
	struct device_node *np = hw->dev->of_node;
	struct iio_buffer *buffer;
	unsigned long irq_type;
	bool irq_active_low;
	int i, err;

	irq_type = irqd_get_trigger_type(irq_get_irq_data(hw->irq));

	switch (irq_type) {
	case IRQF_TRIGGER_HIGH:
	case IRQF_TRIGGER_RISING:
		irq_active_low = false;
		break;
	case IRQF_TRIGGER_LOW:
	case IRQF_TRIGGER_FALLING:
		irq_active_low = true;
		break;
	default:
		dev_info(hw->dev, "mode %lx unsupported\n", irq_type);
		return -EINVAL;
	}

	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_HLACTIVE_ADDR,
					   ST_ASM330LHH_REG_HLACTIVE_MASK,
					   irq_active_low);
	if (err < 0)
		return err;

	if (np && of_property_read_bool(np, "drive-open-drain")) {
		err = st_asm330lhh_write_with_mask(hw,
					ST_ASM330LHH_REG_PP_OD_ADDR,
					ST_ASM330LHH_REG_PP_OD_MASK, 1);
		if (err < 0)
			return err;

		irq_type |= IRQF_SHARED;
	}

	/* use qtimer if property is enabled */
	if (of_property_read_u32(np, "qcom,use_qtimer", &asm330_use_qtimer))
		asm330_use_qtimer = 0; //force to 0 if not in dt

	err = devm_request_threaded_irq(hw->dev, hw->irq,
					st_asm330lhh_handler_irq,
					st_asm330lhh_handler_thread,
					irq_type | IRQF_ONESHOT,
					"asm330lhh", hw);
	if (err) {
		dev_err(hw->dev, "failed to request trigger irq %d\n",
			hw->irq);
		return err;
	}

	for (i = ST_ASM330LHH_ID_ACC; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		buffer = devm_iio_kfifo_allocate(hw->dev);
		if (!buffer)
			return -ENOMEM;

		iio_device_attach_buffer(hw->iio_devs[i], buffer);
		hw->iio_devs[i]->modes |= INDIO_BUFFER_SOFTWARE;
		hw->iio_devs[i]->setup_ops = &st_asm330lhh_buffer_ops;
	}

	return st_asm330lhh_fifo_init(hw);
}


/*
 * STMicroelectronics st_asm330lhh sensor driver
 *
 * Copyright 2020 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pm.h>

#include <linux/platform_data/st_sensors_pdata.h>

#include "st_asm330lhh.h"

static const struct st_asm330lhh_odr_table_entry st_asm330lhh_odr_table[] = {
	[ST_ASM330LHH_ID_ACC] = {
		.size = 7,
		.reg = {
			.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
			.mask = GENMASK(7, 4),
		},
		.batching_reg = {
			.addr = ST_ASM330LHH_REG_FIFO_CTRL3_ADDR,
			.mask = GENMASK(3, 0),
		},
		.odr_avl[0] = {  12, 500000,  0x01,  0x01 },
		.odr_avl[1] = {  26,      0,  0x02,  0x02 },
		.odr_avl[2] = {  52,      0,  0x03,  0x03 },
		.odr_avl[3] = { 104,      0,  0x04,  0x04 },
		.odr_avl[4] = { 208,      0,  0x05,  0x05 },
		.odr_avl[5] = { 416,      0,  0x06,  0x06 },
		.odr_avl[6] = { 833,      0,  0x07,  0x07 },
	},
	[ST_ASM330LHH_ID_GYRO] = {
		.size = 7,
		.reg = {
			.addr = ST_ASM330LHH_CTRL2_G_ADDR,
			.mask = GENMASK(7, 4),
		},
		.batching_reg = {
			.addr = ST_ASM330LHH_REG_FIFO_CTRL3_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {  12, 500000,  0x01,  0x01 },
		.odr_avl[1] = {  26,      0,  0x02,  0x02 },
		.odr_avl[2] = {  52,      0,  0x03,  0x03 },
		.odr_avl[3] = { 104,      0,  0x04,  0x04 },
		.odr_avl[4] = { 208,      0,  0x05,  0x05 },
		.odr_avl[5] = { 416,      0,  0x06,  0x06 },
		.odr_avl[6] = { 833,      0,  0x07,  0x07 },
	},
	[ST_ASM330LHH_ID_TEMP] = {
		.size = 2,
		.batching_reg = {
			.addr = ST_ASM330LHH_REG_FIFO_CTRL4_ADDR,
			.mask = GENMASK(5, 4),
		},
		.odr_avl[0] = { 12, 500000,   0x02,  0x02 },
		.odr_avl[1] = { 52,      0,   0x03,  0x03 },
	},
};

static const struct st_asm330lhh_fs_table_entry st_asm330lhh_fs_table[] = {
	[ST_ASM330LHH_ID_ACC] = {
		.size = ST_ASM330LHH_FS_ACC_LIST_SIZE,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_ASM330LHH_ACC_FS_2G_GAIN,
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_ASM330LHH_ACC_FS_4G_GAIN,
			.val = 0x2,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_ASM330LHH_ACC_FS_8G_GAIN,
			.val = 0x3,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = ST_ASM330LHH_ACC_FS_16G_GAIN,
			.val = 0x1,
		},
	},
	[ST_ASM330LHH_ID_GYRO] = {
		.size = ST_ASM330LHH_FS_GYRO_LIST_SIZE,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_250_GAIN,
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_500_GAIN,
			.val = 0x4,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_1000_GAIN,
			.val = 0x8,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_2000_GAIN,
			.val = 0x0C,
		},
		.fs_avl[4] = {
			.reg = {
				.addr = ST_ASM330LHH_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = ST_ASM330LHH_GYRO_FS_4000_GAIN,
			.val = 0x1,
		},
	},
	[ST_ASM330LHH_ID_TEMP] = {
		.size = ST_ASM330LHH_FS_TEMP_LIST_SIZE,
		.fs_avl[0] = {
			.gain = ST_ASM330LHH_TEMP_FS_GAIN,
			.val = 0x0
		},
	},
};

static const struct iio_chan_spec st_asm330lhh_acc_channels[] = {
	ST_ASM330LHH_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_OUTX_L_A_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHH_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_OUTY_L_A_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHH_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_OUTZ_L_A_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_ASM330LHH_EVENT_CHANNEL(IIO_ACCEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_asm330lhh_gyro_channels[] = {
	ST_ASM330LHH_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_OUTX_L_G_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHH_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_OUTY_L_G_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHH_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_OUTZ_L_G_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_ASM330LHH_EVENT_CHANNEL(IIO_ANGL_VEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_asm330lhh_temp_channels[] = {
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO
	{
		.type = IIO_TEMP,
		.address = ST_ASM330LHH_REG_OUT_TEMP_L_ADDR,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW)
				| BIT(IIO_CHAN_INFO_OFFSET)
				| BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		}
	},
	ST_ASM330LHH_EVENT_CHANNEL(IIO_TEMP, flush),
	IIO_CHAN_SOFT_TIMESTAMP(1),
#else /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO */
	{
		.type = IIO_TEMP,
		.address = ST_ASM330LHH_REG_OUT_TEMP_L_ADDR,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW)
				| BIT(IIO_CHAN_INFO_OFFSET)
				| BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = -1,
	},
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO */
};

int __st_asm330lhh_write_with_mask(struct st_asm330lhh_hw *hw, u8 addr, u8 mask,
				 u8 val)
{
	u8 data, old_data;
	int err;

	mutex_lock(&hw->lock);

	err = hw->tf->read(hw->dev, addr, sizeof(old_data), &old_data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %02x register\n", addr);
		goto out;
	}

	data = (old_data & ~mask) | ((val << __ffs(mask)) & mask);

	/* avoid to write same value */
	if (old_data == data)
		goto out;

	err = hw->tf->write(hw->dev, addr, sizeof(data), &data);
	if (err < 0)
		dev_err(hw->dev, "failed to write %02x register\n", addr);

out:
	mutex_unlock(&hw->lock);

	return (err < 0) ? err : 0;
}

int __maybe_unused st_asm330lhh_read_with_mask(struct st_asm330lhh_hw *hw, u8 addr, u8 mask,
				u8 *val)
{
	u8 data;
	int err;

	err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %02x register\n", addr);
		goto out;
	}

	*val = (data & mask) >> __ffs(mask);

out:
	return (err < 0) ? err : 0;
}

static int st_asm330lhh_check_whoami(struct st_asm330lhh_hw *hw)
{
	u8 data;
	int err;

	err = hw->tf->read(hw->dev, ST_ASM330LHH_REG_WHOAMI_ADDR, sizeof(data),
			   &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read whoami register\n");
		return err;
	}

	if (data != ST_ASM330LHH_WHOAMI_VAL) {
		dev_err(hw->dev, "unsupported whoami [%02x]\n", data);
		return -ENODEV;
	}

	return err;
}

static int st_asm330lhh_get_odr_calibration(struct st_asm330lhh_hw *hw)
{
	s64 odr_calib;
	int err;
	s8 data;

	err = hw->tf->read(hw->dev,
			   ST_ASM330LHH_INTERNAL_FREQ_FINE,
			   sizeof(data), (u8 *)&data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %d register\n",
				ST_ASM330LHH_INTERNAL_FREQ_FINE);
		return err;
	}

	odr_calib = (data * 37500) / 1000;
	hw->ts_delta_ns = ST_ASM330LHH_TS_DELTA_NS - odr_calib;

	dev_info(hw->dev, "Freq Fine %lld (ts %lld)\n", odr_calib, hw->ts_delta_ns);

	return 0;
}

static int st_asm330lhh_set_full_scale(struct st_asm330lhh_sensor *sensor,
				       u32 gain)
{
	enum st_asm330lhh_sensor_id id = sensor->id;
	int i, err;
	u8 val;

	/* for other sensors gain is fixed */
	if (id > ST_ASM330LHH_ID_ACC)
		return 0;

	for (i = 0; i < st_asm330lhh_fs_table[id].size; i++)
		if (st_asm330lhh_fs_table[id].fs_avl[i].gain == gain)
			break;

	if (i == st_asm330lhh_fs_table[id].size)
		return -EINVAL;

	val = st_asm330lhh_fs_table[id].fs_avl[i].val;
	err = st_asm330lhh_write_with_mask(sensor->hw,
				st_asm330lhh_fs_table[id].fs_avl[i].reg.addr,
				st_asm330lhh_fs_table[id].fs_avl[i].reg.mask,
				val);
	if (err < 0)
		return err;

	sensor->gain = gain;

	return 0;
}

static int st_asm330lhh_get_odr_val(struct st_asm330lhh_sensor *sensor, int odr,
			     int uodr, int *podr, int *puodr, u8 *val)
{
	int required_odr = ST_ASM330LHH_ODR_EXPAND(odr, uodr);
	enum st_asm330lhh_sensor_id id = sensor->id;
	int sensor_odr;
	int i;

	for (i = 0; i < st_asm330lhh_odr_table[id].size; i++) {
		sensor_odr = ST_ASM330LHH_ODR_EXPAND(
				st_asm330lhh_odr_table[id].odr_avl[i].hz,
				st_asm330lhh_odr_table[id].odr_avl[i].uhz);
		if (sensor_odr >= required_odr)
			break;
	}

	if (i == st_asm330lhh_odr_table[id].size)
		return -EINVAL;

	*val = st_asm330lhh_odr_table[id].odr_avl[i].val;

	if (podr && puodr) {
		*podr = st_asm330lhh_odr_table[id].odr_avl[i].hz;
		*puodr = st_asm330lhh_odr_table[id].odr_avl[i].uhz;
    }

	return 0;
}

int st_asm330lhh_get_batch_val(struct st_asm330lhh_sensor *sensor,
			       int odr, int uodr, u8 *val)
{
	int required_odr = ST_ASM330LHH_ODR_EXPAND(odr, uodr);
	enum st_asm330lhh_sensor_id id = sensor->id;
	int sensor_odr;
	int i;

	for (i = 0; i < st_asm330lhh_odr_table[id].size; i++) {
		sensor_odr = ST_ASM330LHH_ODR_EXPAND(
				st_asm330lhh_odr_table[id].odr_avl[i].hz,
				st_asm330lhh_odr_table[id].odr_avl[i].uhz);
		if (sensor_odr >= required_odr)
			break;
	}

	if (i == st_asm330lhh_odr_table[id].size)
		return -EINVAL;

	*val = st_asm330lhh_odr_table[id].odr_avl[i].batch_val;

	return 0;
}

static u16 st_asm330lhh_check_odr_dependency(struct st_asm330lhh_hw *hw,
					   int odr, int uodr,
					   enum st_asm330lhh_sensor_id ref_id)
{
	struct st_asm330lhh_sensor *ref = iio_priv(hw->iio_devs[ref_id]);
	bool enable = ST_ASM330LHH_ODR_EXPAND(odr, uodr) > 0;
	u16 ret;

	if (enable) {
		/* uodr not used */
		if (hw->enable_mask & BIT(ref_id))
			ret = max_t(u16, ref->odr, odr);
		else
			ret = odr;
	} else {
		ret = (hw->enable_mask & BIT(ref_id)) ? ref->odr : 0;
	}

	return ret;
}

static int st_asm330lhh_set_odr(struct st_asm330lhh_sensor *sensor, int req_odr,
				int req_uodr)
{
	enum st_asm330lhh_sensor_id id = sensor->id;
	struct st_asm330lhh_hw *hw = sensor->hw;
	u8 val = 0;
	int err;

	switch (id) {
#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO
	case ST_ASM330LHH_ID_TEMP:
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO */
	case ST_ASM330LHH_ID_ACC: {
		int odr;
		int i;

		id = ST_ASM330LHH_ID_ACC;
		for (i = ST_ASM330LHH_ID_ACC; i < ST_ASM330LHH_ID_MAX; i++) {
			if (!hw->iio_devs[i])
				continue;

			if (i == sensor->id)
				continue;

			odr = st_asm330lhh_check_odr_dependency(hw, req_odr,
								req_uodr, i);
			if (odr != req_odr) {
				/* device already configured */
				return 0;
			}
		}
		break;
	}
	default:
		break;
	}

	err = st_asm330lhh_get_odr_val(sensor, req_odr, req_uodr, NULL,
				       NULL, &val);
	if (err < 0)
		return err;

	err = st_asm330lhh_write_with_mask(hw,
					   st_asm330lhh_odr_table[sensor->id].reg.addr,
					   st_asm330lhh_odr_table[sensor->id].reg.mask,
					   val);

	return err < 0 ? err : 0;
}

int st_asm330lhh_sensor_set_enable(struct st_asm330lhh_sensor *sensor,
				 bool enable)
{
	int uodr = enable ? sensor->uodr : 0;
	int odr = enable ? sensor->odr : 0;
	int err;

#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO
	err = st_asm330lhh_set_odr(sensor, odr, uodr);
	if (err < 0)
		return err;
#else /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO */
	if (sensor->id != ST_ASM330LHH_ID_TEMP) {
		err = st_asm330lhh_set_odr(sensor, odr, uodr);
		if (err < 0)
			return err;
	}
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO */

	if (enable)
		sensor->hw->enable_mask |= BIT(sensor->id);
	else
		sensor->hw->enable_mask &= ~BIT(sensor->id);

	return 0;
}

static int st_asm330lhh_read_oneshot(struct st_asm330lhh_sensor *sensor,
				   u8 addr, int *val)
{
	int err, delay;
	__le16 data;

#ifdef CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO
	if (sensor->id == ST_ASM330LHH_ID_TEMP) {
		u8 status;

		mutex_lock(&sensor->hw->fifo_lock);
		err = sensor->hw->tf->read(sensor->hw->dev,
					   ST_ASM330LHH_REG_STATUS_ADDR,
					   sizeof(status), &status);
		if (err < 0)
			goto unlock;

		if (status & ST_ASM330LHH_REG_STATUS_TDA) {
			err = sensor->hw->tf->read(sensor->hw->dev,
						   addr, sizeof(data),
					   (u8 *)&data);
			if (err < 0)
				goto unlock;

			sensor->old_data = data;
		} else
			data = sensor->old_data;
unlock:
		mutex_unlock(&sensor->hw->fifo_lock);

	} else {
		err = st_asm330lhh_sensor_set_enable(sensor, true);
		if (err < 0)
			return err;

		/* Use big delay for data valid because of drdy mask enabled */
		delay = 10000000 / sensor->odr;
		usleep_range(delay, 2 * delay);

		err = st_asm330lhh_read_atomic(sensor->hw, addr, sizeof(data),
					       (u8 *)&data);
		if (err < 0)
			return err;

		err = st_asm330lhh_sensor_set_enable(sensor, false);
	}
#else /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO */
	err = st_asm330lhh_sensor_set_enable(sensor, true);
	if (err < 0)
		return err;

	/* Use big delay for data valid because of drdy mask enabled */
	delay = 10000000 / sensor->odr;
	usleep_range(delay, 2 * delay);

	err = st_asm330lhh_read_atomic(sensor->hw, addr, sizeof(data),
				       (u8 *)&data);
	if (err < 0)
		return err;

	err = st_asm330lhh_sensor_set_enable(sensor, false);
#endif /* CONFIG_IIO_ST_ASM330LHH_EN_TEMPERATURE_FIFO */

	*val = (s16)le16_to_cpu(data);

	return IIO_VAL_INT;
}

static int st_asm330lhh_read_raw(struct iio_dev *iio_dev,
				 struct iio_chan_spec const *ch,
				 int *val, int *val2, long mask)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(iio_dev);
		if (ret)
			break;

		ret = st_asm330lhh_read_oneshot(sensor, ch->address, val);
		iio_device_release_direct_mode(iio_dev);
		break;
	case IIO_CHAN_INFO_OFFSET:
		switch (ch->type) {
		case IIO_TEMP:
			*val = sensor->offset;
			ret = IIO_VAL_INT;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = (int)sensor->odr;
		*val2 = (int)sensor->uodr;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (ch->type) {
		case IIO_TEMP:
			*val = 1;
			*val2 = ST_ASM330LHH_TEMP_GAIN;
			ret = IIO_VAL_FRACTIONAL;
			break;
		case IIO_ACCEL:
		case IIO_ANGL_VEL:
			*val = 0;
			*val2 = sensor->gain;
			ret = IIO_VAL_INT_PLUS_MICRO;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int st_asm330lhh_write_raw(struct iio_dev *iio_dev,
				  struct iio_chan_spec const *chan,
				  int val, int val2, long mask)
{
	struct st_asm330lhh_sensor *s = iio_priv(iio_dev);
	int err;

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		err = iio_device_claim_direct_mode(iio_dev);
		if (err)
			return err;

		err = st_asm330lhh_set_full_scale(s, val2);
		iio_device_release_direct_mode(iio_dev);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ: {
		int todr, tuodr;
		u8 data;

		err = st_asm330lhh_get_odr_val(s, val, val2, &todr, &tuodr, &data);
		if (!err) {
			s->odr = val;
			s->uodr = tuodr;

			/*
			 * VTS test testSamplingRateHotSwitchOperation not
			 * toggle the enable status of sensor after changing
			 * the ODR -> force it
			 */
			if (s->hw->enable_mask & BIT(s->id)) {
				switch (s->id) {
				case ST_ASM330LHH_ID_GYRO:
				case ST_ASM330LHH_ID_ACC:
					err = st_asm330lhh_set_odr(s, s->odr, s->uodr);
					if (err < 0)
						break;

					st_asm330lhh_update_batching(iio_dev, 1);
					break;
				default:
					break;
				}
			}
		}
		break;
	}
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static ssize_t
st_asm330lhh_sysfs_sampling_freq_avail(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_asm330lhh_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < st_asm330lhh_odr_table[id].size; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d.%06d ",
				 st_asm330lhh_odr_table[id].odr_avl[i].hz,
				 st_asm330lhh_odr_table[id].odr_avl[i].uhz);
	}

	buf[len - 1] = '\n';

	return len;
}

static ssize_t st_asm330lhh_sysfs_scale_avail(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_asm330lhh_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < st_asm330lhh_fs_table[id].size; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0.%06u ",
				 st_asm330lhh_fs_table[id].fs_avl[i].gain);
	buf[len - 1] = '\n';

	return len;
}

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_asm330lhh_sysfs_sampling_freq_avail);
static IIO_DEVICE_ATTR(in_accel_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(in_anglvel_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_max, 0444,
		       st_asm330lhh_get_max_watermark, NULL, 0);
static IIO_DEVICE_ATTR(in_temp_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_flush, 0200, NULL, st_asm330lhh_flush_fifo, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark, 0644, st_asm330lhh_get_watermark,
		       st_asm330lhh_set_watermark, 0);

static struct attribute *st_asm330lhh_acc_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_accel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhh_acc_attribute_group = {
	.attrs = st_asm330lhh_acc_attributes,
};

static const struct iio_info st_asm330lhh_acc_info = {
	.attrs = &st_asm330lhh_acc_attribute_group,
	.read_raw = st_asm330lhh_read_raw,
	.write_raw = st_asm330lhh_write_raw,
};

static struct attribute *st_asm330lhh_gyro_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhh_gyro_attribute_group = {
	.attrs = st_asm330lhh_gyro_attributes,
};

static const struct iio_info st_asm330lhh_gyro_info = {
	.attrs = &st_asm330lhh_gyro_attribute_group,
	.read_raw = st_asm330lhh_read_raw,
	.write_raw = st_asm330lhh_write_raw,
};

static struct attribute *st_asm330lhh_temp_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_temp_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhh_temp_attribute_group = {
	.attrs = st_asm330lhh_temp_attributes,
};

static const struct iio_info st_asm330lhh_temp_info = {
	.attrs = &st_asm330lhh_temp_attribute_group,
	.read_raw = st_asm330lhh_read_raw,
	.write_raw = st_asm330lhh_write_raw,
};

static const unsigned long st_asm330lhh_available_scan_masks[] = { 0x7, 0x0 };

static int st_asm330lhh_of_get_pin(struct st_asm330lhh_hw *hw, int *pin)
{
	if (!dev_fwnode(hw->dev))
		return -EINVAL;

	return device_property_read_u32(hw->dev, "st,int-pin", pin);
}

static int st_asm330lhh_get_int_reg(struct st_asm330lhh_hw *hw, u8 *drdy_reg)
{
	int err = 0, int_pin;

	if (st_asm330lhh_of_get_pin(hw, &int_pin) < 0) {
		struct st_sensors_platform_data *pdata;
		struct device *dev = hw->dev;

		pdata = (struct st_sensors_platform_data *)dev->platform_data;
		int_pin = pdata ? pdata->drdy_int_pin : 1;
	}

	switch (int_pin) {
	case 1:
		*drdy_reg = ST_ASM330LHH_REG_INT1_CTRL_ADDR;
		break;
	case 2:
		*drdy_reg = ST_ASM330LHH_REG_INT2_CTRL_ADDR;
		break;
	default:
		dev_err(hw->dev, "unsupported interrupt pin\n");
		err = -EINVAL;
		break;
	}

	hw->int_pin = int_pin;

	return err;
}

static int st_asm330lhh_reset_device(struct st_asm330lhh_hw *hw)
{
	int err;

	/* set configuration bit */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL9_XL_ADDR,
				           ST_ASM330LHH_REG_DEVICE_CONF_MASK, 1);
	if (err < 0)
		return err;

	/* sw reset */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL3_C_ADDR,
					 ST_ASM330LHH_REG_SW_RESET_MASK, 1);
	if (err < 0)
		return err;

	msleep(50);

	/* boot */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL3_C_ADDR,
					 ST_ASM330LHH_REG_BOOT_MASK, 1);

	msleep(50);

	return err;
}

static int st_asm330lhh_init_device(struct st_asm330lhh_hw *hw)
{
	u8 drdy_reg;
	int err;

	/* latch interrupts */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_TAP_CFG0_ADDR,
					 ST_ASM330LHH_REG_LIR_MASK, 1);
	if (err < 0)
		return err;

	/* enable Block Data Update */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL3_C_ADDR,
					 ST_ASM330LHH_REG_BDU_MASK, 1);
	if (err < 0)
		return err;

	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL5_C_ADDR,
					 ST_ASM330LHH_REG_ROUNDING_MASK, 3);
	if (err < 0)
		return err;

	/* init timestamp engine */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL10_C_ADDR,
					ST_ASM330LHH_REG_TIMESTAMP_EN_MASK, 1);
	if (err < 0)
		return err;

	err = st_asm330lhh_get_int_reg(hw, &drdy_reg);
	if (err < 0)
		return err;

	/* enable DRDY MASK for filters settling time */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_CTRL4_C_ADDR,
					 ST_ASM330LHH_REG_DRDY_MASK, 1);
	if (err < 0)
		return err;

	/* enable FIFO watermak interrupt */
	return st_asm330lhh_write_with_mask(hw, drdy_reg,
					 ST_ASM330LHH_REG_INT_FIFO_TH_MASK, 1);
}

static struct iio_dev *st_asm330lhh_alloc_iiodev(struct st_asm330lhh_hw *hw,
						 enum st_asm330lhh_sensor_id id)
{
	struct st_asm330lhh_sensor *sensor;
	struct iio_dev *iio_dev;

	iio_dev = devm_iio_device_alloc(hw->dev, sizeof(*sensor));
	if (!iio_dev)
		return NULL;

	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->dev.parent = hw->dev;

	sensor = iio_priv(iio_dev);
	sensor->id = id;
	sensor->hw = hw;
	sensor->watermark = 1;

	/*
	 * for acc/gyro the default Android full scale settings are:
	 * Acc FS 8g (78.40 m/s^2)
	 * Gyro FS 1000dps (16.45 radians/sec)
	 */
	switch (id) {
	case ST_ASM330LHH_ID_ACC:
		iio_dev->channels = st_asm330lhh_acc_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_acc_channels);
		iio_dev->name = "asm330lhh_accel";
		iio_dev->info = &st_asm330lhh_acc_info;
		iio_dev->available_scan_masks =
					st_asm330lhh_available_scan_masks;
		sensor->max_watermark = ST_ASM330LHH_MAX_FIFO_DEPTH;
		sensor->gain = st_asm330lhh_fs_table[id].fs_avl[ST_ASM330LHH_DEFAULT_XL_FS_INDEX].gain;
		sensor->odr = st_asm330lhh_odr_table[id].odr_avl[ST_ASM330LHH_DEFAULT_XL_ODR_INDEX].hz;
		sensor->uodr = st_asm330lhh_odr_table[id].odr_avl[ST_ASM330LHH_DEFAULT_XL_ODR_INDEX].uhz;
		sensor->offset = 0;
		break;
	case ST_ASM330LHH_ID_GYRO:
		iio_dev->channels = st_asm330lhh_gyro_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_gyro_channels);
		iio_dev->name = "asm330lhh_gyro";
		iio_dev->info = &st_asm330lhh_gyro_info;
		iio_dev->available_scan_masks =
					st_asm330lhh_available_scan_masks;
		sensor->max_watermark = ST_ASM330LHH_MAX_FIFO_DEPTH;
		sensor->gain = st_asm330lhh_fs_table[id].fs_avl[ST_ASM330LHH_DEFAULT_G_FS_INDEX].gain;
		sensor->odr = st_asm330lhh_odr_table[id].odr_avl[ST_ASM330LHH_DEFAULT_G_ODR_INDEX].hz;
		sensor->uodr = st_asm330lhh_odr_table[id].odr_avl[ST_ASM330LHH_DEFAULT_G_ODR_INDEX].uhz;
		sensor->offset = 0;
		break;
	case ST_ASM330LHH_ID_TEMP:
		iio_dev->channels = st_asm330lhh_temp_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_temp_channels);
		iio_dev->name = "asm330lhh_temp";
		iio_dev->info = &st_asm330lhh_temp_info;
		sensor->max_watermark = ST_ASM330LHH_MAX_FIFO_DEPTH;
		sensor->gain = st_asm330lhh_fs_table[id].fs_avl[ST_ASM330LHH_DEFAULT_T_FS_INDEX].gain;
		sensor->odr = st_asm330lhh_odr_table[id].odr_avl[ST_ASM330LHH_DEFAULT_T_ODR_INDEX].hz;
		sensor->uodr = st_asm330lhh_odr_table[id].odr_avl[ST_ASM330LHH_DEFAULT_T_ODR_INDEX].uhz;
		sensor->offset = ST_ASM330LHH_TEMP_OFFSET;
		break;
	default:
		devm_iio_device_free(hw->dev, iio_dev);

		return NULL;
	}

	st_asm330lhh_set_full_scale(sensor, sensor->gain);

	return iio_dev;
}

int st_asm330lhh_probe(struct device *dev, int irq,
		     const struct st_asm330lhh_transfer_function *tf_ops)
{
	struct st_asm330lhh_hw *hw;
	int i, err;

	hw = devm_kzalloc(dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	dev_set_drvdata(dev, (void *)hw);

	mutex_init(&hw->lock);
	mutex_init(&hw->fifo_lock);
	mutex_init(&hw->page_lock);

	hw->dev = dev;
	hw->irq = irq;
	hw->tf = tf_ops;
	hw->odr_table_entry = st_asm330lhh_odr_table;

	dev_info(hw->dev, "Ver: %s\n", ST_ASM330LHH_VERSION);
	err = st_asm330lhh_check_whoami(hw);
	if (err < 0)
		return err;

	err = st_asm330lhh_get_odr_calibration(hw);
	if (err < 0)
		return err;

	err = st_asm330lhh_reset_device(hw);
	if (err < 0)
		return err;

	err = st_asm330lhh_init_device(hw);
	if (err < 0)
		return err;

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		hw->iio_devs[i] = st_asm330lhh_alloc_iiodev(hw, i);
		if (!hw->iio_devs[i])
			return -ENOMEM;
	}

	if (hw->irq > 0) {
		err = st_asm330lhh_buffers_setup(hw);
		if (err < 0)
			return err;
	}

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		err = devm_iio_device_register(hw->dev, hw->iio_devs[i]);
		if (err)
			return err;
	}

#if defined(CONFIG_PM) && defined(CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP)
	err = device_init_wakeup(dev, 1);
	if (err)
		return err;
#endif /* CONFIG_PM && CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP */

	dev_info(hw->dev, "probe ok\n");

	return 0;
}
EXPORT_SYMBOL(st_asm330lhh_probe);

static int __maybe_unused st_asm330lhh_suspend(struct device *dev)
{
	struct st_asm330lhh_hw *hw = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor;
	int i, err = 0;

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		sensor = iio_priv(hw->iio_devs[i]);
		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		/* power off enabled sensors */
		err = st_asm330lhh_set_odr(sensor, 0, 0);
		if (err < 0)
			return err;
	}

	if (st_asm330lhh_is_fifo_enabled(hw)) {
		err = st_asm330lhh_suspend_fifo(hw);
		if (err < 0)
			return err;
	}

#ifdef CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP
	if (device_may_wakeup(dev))
		enable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP */

	return err < 0 ? err : 0;
}

static int __maybe_unused st_asm330lhh_resume(struct device *dev)
{
	struct st_asm330lhh_hw *hw = dev_get_drvdata(dev);
	struct st_asm330lhh_sensor *sensor;
	int i, err = 0;

#ifdef CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP
	if (device_may_wakeup(dev))
		disable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_ASM330LHH_MAY_WAKEUP */

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		sensor = iio_priv(hw->iio_devs[i]);
		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_asm330lhh_set_odr(sensor, sensor->odr, sensor->uodr);
		if (err < 0)
			return err;
	}

	err = st_asm330lhh_reset_hwts(hw);
	if (err < 0)
		return err;

	if (st_asm330lhh_is_fifo_enabled(hw))
		err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_CONT);

	return err < 0 ? err : 0;
}

const struct dev_pm_ops st_asm330lhh_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_asm330lhh_suspend, st_asm330lhh_resume)
};
EXPORT_SYMBOL(st_asm330lhh_pm_ops);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_asm330lhh driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(ST_ASM330LHH_VERSION);

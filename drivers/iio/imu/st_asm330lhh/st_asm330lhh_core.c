/*
 * STMicroelectronics st_asm330lhh sensor driver
 *
 * Copyright 2018 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/pm.h>
#include <linux/version.h>
#include <linux/of.h>

#include <linux/platform_data/st_sensors_pdata.h>

#include "st_asm330lhh.h"

#define ST_ASM330LHH_REG_INT1_ADDR		0x0d
#define ST_ASM330LHH_REG_INT2_ADDR		0x0e
#define ST_ASM330LHH_REG_FIFO_CTRL4_ADDR	0x0a
#define ST_ASM330LHH_REG_FIFO_FTH_IRQ_MASK	BIT(3)
#define ST_ASM330LHH_REG_WHOAMI_ADDR		0x0f
#define ST_ASM330LHH_WHOAMI_VAL			0x6b
#define ST_ASM330LHH_REG_CTRL1_XL_ADDR		0x10
#define ST_ASM330LHH_REG_CTRL2_G_ADDR		0x11
#define ST_ASM330LHH_REG_RESET_ADDR		0x12
#define ST_ASM330LHH_REG_RESET_MASK		BIT(0)
#define ST_ASM330LHH_REG_BDU_ADDR		0x12
#define ST_ASM330LHH_REG_BDU_MASK		BIT(6)
#define ST_ASM330LHH_REG_INT2_ON_INT1_ADDR	0x13
#define ST_ASM330LHH_REG_INT2_ON_INT1_MASK	BIT(5)
#define ST_ASM330LHH_REG_ROUNDING_ADDR		0x14
#define ST_ASM330LHH_REG_ROUNDING_MASK		GENMASK(6, 5)
#define ST_ASM330LHH_REG_TIMESTAMP_EN_ADDR	0x19
#define ST_ASM330LHH_REG_TIMESTAMP_EN_MASK	BIT(5)

#define ST_ASM330LHH_REG_GYRO_OUT_X_L_ADDR	0x22
#define ST_ASM330LHH_REG_GYRO_OUT_Y_L_ADDR	0x24
#define ST_ASM330LHH_REG_GYRO_OUT_Z_L_ADDR	0x26

#define ST_ASM330LHH_REG_ACC_OUT_X_L_ADDR	0x28
#define ST_ASM330LHH_REG_ACC_OUT_Y_L_ADDR	0x2a
#define ST_ASM330LHH_REG_ACC_OUT_Z_L_ADDR	0x2c

#define ST_ASM330LHH_REG_LIR_ADDR		0x56
#define ST_ASM330LHH_REG_LIR_MASK		BIT(0)

#define ST_ASM330LHH_ACC_FS_2G_GAIN		IIO_G_TO_M_S_2(61)
#define ST_ASM330LHH_ACC_FS_4G_GAIN		IIO_G_TO_M_S_2(122)
#define ST_ASM330LHH_ACC_FS_8G_GAIN		IIO_G_TO_M_S_2(244)
#define ST_ASM330LHH_ACC_FS_16G_GAIN		IIO_G_TO_M_S_2(488)

#define ST_ASM330LHH_GYRO_FS_125_GAIN		IIO_DEGREE_TO_RAD(4375)
#define ST_ASM330LHH_GYRO_FS_250_GAIN		IIO_DEGREE_TO_RAD(8750)
#define ST_ASM330LHH_GYRO_FS_500_GAIN		IIO_DEGREE_TO_RAD(17500)
#define ST_ASM330LHH_GYRO_FS_1000_GAIN		IIO_DEGREE_TO_RAD(35000)
#define ST_ASM330LHH_GYRO_FS_2000_GAIN		IIO_DEGREE_TO_RAD(70000)
#define ST_ASM330LHH_GYRO_FS_4000_GAIN		IIO_DEGREE_TO_RAD(140000)

/* Temperature in uC */
#define ST_ASM330LHH_TEMP_GAIN			256
#define ST_ASM330LHH_TEMP_FS_GAIN		(1000000 / ST_ASM330LHH_TEMP_GAIN)
#define ST_ASM330LHH_OFFSET			(6400)

struct st_asm330lhh_std_entry {
	u16 odr;
	u8 val;
};

/* Minimal number of sample to be discarded */
struct st_asm330lhh_std_entry st_asm330lhh_std_table[] = {
	{  13,  2 },
	{  26,  3 },
	{  52,  4 },
	{ 104,  6 },
	{ 208,  8 },
	{ 416, 18 },
};

static const struct st_asm330lhh_odr_table_entry st_asm330lhh_odr_table[] = {
	[ST_ASM330LHH_ID_ACC] = {
		.reg = {
			.addr = ST_ASM330LHH_REG_CTRL1_XL_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   0, 0x00 },
		.odr_avl[1] = {  13, 0x01 },
		.odr_avl[2] = {  26, 0x02 },
		.odr_avl[3] = {  52, 0x03 },
		.odr_avl[4] = { 104, 0x04 },
		.odr_avl[5] = { 208, 0x05 },
		.odr_avl[6] = { 416, 0x06 },
	},
	[ST_ASM330LHH_ID_GYRO] = {
		.reg = {
			.addr = ST_ASM330LHH_REG_CTRL2_G_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   0, 0x00 },
		.odr_avl[1] = {  13, 0x01 },
		.odr_avl[2] = {  26, 0x02 },
		.odr_avl[3] = {  52, 0x03 },
		.odr_avl[4] = { 104, 0x04 },
		.odr_avl[5] = { 208, 0x05 },
		.odr_avl[6] = { 416, 0x06 },
	},
	[ST_ASM330LHH_ID_TEMP] = {
		.odr_avl[0] = {   0, 0x00 },
		.odr_avl[1] = {  52, 0x01 },
	}
};

static const struct st_asm330lhh_fs_table_entry st_asm330lhh_fs_table[] = {
	[ST_ASM330LHH_ID_ACC] = {
		.reg = {
			.addr = ST_ASM330LHH_REG_CTRL1_XL_ADDR,
			.mask = GENMASK(3, 2),
		},
		.size = ST_ASM330LHH_FS_ACC_LIST_SIZE,
		.fs_avl[0] = {  ST_ASM330LHH_ACC_FS_2G_GAIN, 0x0 },
		.fs_avl[1] = {  ST_ASM330LHH_ACC_FS_4G_GAIN, 0x2 },
		.fs_avl[2] = {  ST_ASM330LHH_ACC_FS_8G_GAIN, 0x3 },
		.fs_avl[3] = { ST_ASM330LHH_ACC_FS_16G_GAIN, 0x1 },
	},
	[ST_ASM330LHH_ID_GYRO] = {
		.reg = {
			.addr = ST_ASM330LHH_REG_CTRL2_G_ADDR,
			.mask = GENMASK(3, 0),
		},
		.size = ST_ASM330LHH_FS_GYRO_LIST_SIZE,
		.fs_avl[0] = {  ST_ASM330LHH_GYRO_FS_125_GAIN, 0x2 },
		.fs_avl[1] = {  ST_ASM330LHH_GYRO_FS_250_GAIN, 0x0 },
		.fs_avl[2] = {  ST_ASM330LHH_GYRO_FS_500_GAIN, 0x4 },
		.fs_avl[3] = { ST_ASM330LHH_GYRO_FS_1000_GAIN, 0x8 },
		.fs_avl[4] = { ST_ASM330LHH_GYRO_FS_2000_GAIN, 0xC },
		.fs_avl[5] = { ST_ASM330LHH_GYRO_FS_4000_GAIN, 0x1 },
	},
	[ST_ASM330LHH_ID_TEMP] = {
		.size = ST_ASM330LHH_FS_TEMP_LIST_SIZE,
		.fs_avl[0] = {  ST_ASM330LHH_TEMP_FS_GAIN, 0x0 },
	}
};

static const struct iio_chan_spec st_asm330lhh_acc_channels[] = {
	ST_ASM330LHH_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_ACC_OUT_X_L_ADDR,
			   1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHH_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_ACC_OUT_Y_L_ADDR,
			   1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHH_CHANNEL(IIO_ACCEL, ST_ASM330LHH_REG_ACC_OUT_Z_L_ADDR,
			   1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_ASM330LHH_FLUSH_CHANNEL(IIO_ACCEL),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_asm330lhh_gyro_channels[] = {
	ST_ASM330LHH_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_GYRO_OUT_X_L_ADDR,
			   1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHH_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_GYRO_OUT_Y_L_ADDR,
			   1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHH_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHH_REG_GYRO_OUT_Z_L_ADDR,
			   1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_ASM330LHH_FLUSH_CHANNEL(IIO_ANGL_VEL),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_asm330lhh_temp_channels[] = {
	{
		.type = IIO_TEMP,
		.address = ST_ASM330LHH_REG_OUT_TEMP_L_ADDR,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW)
				| BIT(IIO_CHAN_INFO_OFFSET)
				| BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = -1,
	},
};

int st_asm330lhh_write_with_mask(struct st_asm330lhh_hw *hw, u8 addr, u8 mask,
				 u8 val)
{
	u8 data;
	int err;

	mutex_lock(&hw->lock);

	err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %02x register\n", addr);
		goto out;
	}

	data = (data & ~mask) | ((val << __ffs(mask)) & mask);

	err = hw->tf->write(hw->dev, addr, sizeof(data), &data);
	if (err < 0)
		dev_err(hw->dev, "failed to write %02x register\n", addr);

out:
	mutex_unlock(&hw->lock);

	return err;
}

static int st_asm330lhh_check_whoami(struct st_asm330lhh_hw *hw)
{
	int err;
	u8 data;

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

	return 0;
}

static int st_asm330lhh_set_full_scale(struct st_asm330lhh_sensor *sensor,
				       u32 gain)
{
	enum st_asm330lhh_sensor_id id = sensor->id;
	int i, err;
	u8 val;

	for (i = 0; i < st_asm330lhh_fs_table[id].size; i++)
		if (st_asm330lhh_fs_table[id].fs_avl[i].gain == gain)
			break;

	if (i == st_asm330lhh_fs_table[id].size)
		return -EINVAL;

	val = st_asm330lhh_fs_table[id].fs_avl[i].val;
	err = st_asm330lhh_write_with_mask(sensor->hw,
					st_asm330lhh_fs_table[id].reg.addr,
					st_asm330lhh_fs_table[id].reg.mask,
					val);
	if (err < 0)
		return err;

	sensor->gain = gain;

	return 0;
}

int st_asm330lhh_get_odr_val(enum st_asm330lhh_sensor_id id, u16 odr, u8 *val)
{
	int i;

	for (i = 0; i < ST_ASM330LHH_ODR_LIST_SIZE; i++)
		if (st_asm330lhh_odr_table[id].odr_avl[i].hz >= odr)
			break;

	if (i == ST_ASM330LHH_ODR_LIST_SIZE)
		return -EINVAL;

	*val = st_asm330lhh_odr_table[id].odr_avl[i].val;

	return 0;
}

static int st_asm330lhh_set_std_level(struct st_asm330lhh_sensor *sensor,
			u16 odr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(st_asm330lhh_std_table); i++)
		if (st_asm330lhh_std_table[i].odr == odr)
			break;

	if (i == ARRAY_SIZE(st_asm330lhh_std_table))
		return -EINVAL;

	sensor->std_level = st_asm330lhh_std_table[i].val;
	sensor->std_samples = 0;

	return 0;
}

static int st_asm330lhh_set_odr(struct st_asm330lhh_sensor *sensor, u16 odr)
{
	struct st_asm330lhh_hw *hw = sensor->hw;
	u8 val;

	if (st_asm330lhh_get_odr_val(sensor->id, odr, &val) < 0)
		return -EINVAL;

	return st_asm330lhh_write_with_mask(hw,
				st_asm330lhh_odr_table[sensor->id].reg.addr,
				st_asm330lhh_odr_table[sensor->id].reg.mask, val);
}

int st_asm330lhh_sensor_set_enable(struct st_asm330lhh_sensor *sensor,
				   bool enable)
{
	u16 odr = enable ? sensor->odr : 0;
	int err;

	if (sensor->id != ST_ASM330LHH_ID_TEMP) {
		err = st_asm330lhh_set_odr(sensor, odr);
		if (err < 0)
			return err;
	}

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

	if (sensor->id == ST_ASM330LHH_ID_TEMP) {
		u8 status;

		mutex_lock(&sensor->hw->fifo_lock);
		err = sensor->hw->tf->read(sensor->hw->dev,
					   ST_ASM330LHH_REG_STATUS_ADDR, sizeof(status), &status);
		if (err < 0)
			goto unlock;

		if (status & ST_ASM330LHH_REG_STATUS_TDA) {
			err = sensor->hw->tf->read(sensor->hw->dev, addr, sizeof(data),
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

		delay = 1000000 / sensor->odr;
		usleep_range(delay, 2 * delay);

		err = sensor->hw->tf->read(sensor->hw->dev, addr, sizeof(data),
					   (u8 *)&data);
		if (err < 0)
			return err;

		st_asm330lhh_sensor_set_enable(sensor, false);
	}

	*val = (s16)data;

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
		mutex_lock(&iio_dev->mlock);
		if (iio_buffer_enabled(iio_dev)) {
			ret = -EBUSY;
			mutex_unlock(&iio_dev->mlock);
			break;
		}
		ret = st_asm330lhh_read_oneshot(sensor, ch->address, val);
		mutex_unlock(&iio_dev->mlock);
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
		*val = sensor->odr;
		ret = IIO_VAL_INT;
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
	struct st_asm330lhh_sensor *sensor = iio_priv(iio_dev);
	int err;

	if (asm330_check_acc_gyro_early_buff_enable_flag(sensor))
		return 0;

	mutex_lock(&iio_dev->mlock);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		err = st_asm330lhh_set_full_scale(sensor, val2);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ: {
		u8 data;

		err = st_asm330lhh_set_std_level(sensor, val);
		if (err < 0)
			break;

		err = st_asm330lhh_get_odr_val(sensor->id, val, &data);
		if (!err)
			sensor->odr = val;

		err = st_asm330lhh_set_odr(sensor, sensor->odr);
		break;
	}
	default:
		err = -EINVAL;
		break;
	}

	mutex_unlock(&iio_dev->mlock);

	return err;
}

static ssize_t
st_asm330lhh_sysfs_sampling_frequency_avail(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_asm330lhh_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 1; i < ST_ASM330LHH_ODR_LIST_SIZE; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				 st_asm330lhh_odr_table[id].odr_avl[i].hz);
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
#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
static int asm_read_bootsampl(struct st_asm330lhh_sensor  *sensor,
		unsigned long enable_read)
{
	int i = 0;

	if (enable_read) {
		sensor->buffer_asm_samples = false;
		for (i = 0; i < sensor->bufsample_cnt; i++) {
			dev_dbg(sensor->hw->dev,
				"sensor:%d count:%d x=%d,y=%d,z=%d,tsec=%d,nsec=%lld\n",
				sensor->id, i, sensor->asm_samplist[i]->xyz[0],
				sensor->asm_samplist[i]->xyz[1],
				sensor->asm_samplist[i]->xyz[2],
				sensor->asm_samplist[i]->tsec,
				sensor->asm_samplist[i]->tnsec);
			input_report_abs(sensor->buf_dev, ABS_X,
					sensor->asm_samplist[i]->xyz[0]);
			input_report_abs(sensor->buf_dev, ABS_Y,
					sensor->asm_samplist[i]->xyz[1]);
			input_report_abs(sensor->buf_dev, ABS_Z,
					sensor->asm_samplist[i]->xyz[2]);
			input_report_abs(sensor->buf_dev, ABS_RX,
					sensor->asm_samplist[i]->tsec);
			input_report_abs(sensor->buf_dev, ABS_RY,
					sensor->asm_samplist[i]->tnsec);
			input_sync(sensor->buf_dev);
		}
	} else {
		/* clean up */
		if (sensor->bufsample_cnt != 0) {
			for (i = 0; i < ASM_MAXSAMPLE; i++)
				kmem_cache_free(sensor->asm_cachepool,
					sensor->asm_samplist[i]);
			kmem_cache_destroy(sensor->asm_cachepool);
			sensor->bufsample_cnt = 0;
		}

	}
	/*SYN_CONFIG indicates end of data*/
	input_event(sensor->buf_dev, EV_SYN, SYN_CONFIG, 0xFFFFFFFF);
	input_sync(sensor->buf_dev);
	dev_dbg(sensor->hw->dev, "End of gyro samples bufsample_cnt=%d\n",
			sensor->bufsample_cnt);
	return 0;
}
static ssize_t read_gyro_boot_sample_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));

	return snprintf(buf, PAGE_SIZE, "%d\n",
				sensor->read_boot_sample);
}
static ssize_t read_gyro_boot_sample_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	unsigned long enable = 0;

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable > 1) {
		err = dev_err(sensor->hw->dev,
				"Invalid value of input, input=%ld\n", enable);
		return -EINVAL;
	}
	err = asm_read_bootsampl(sensor, enable);
	if (err)
		return err;
	sensor->read_boot_sample = enable;
	return count;

}

static ssize_t read_acc_boot_sample_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));

	return snprintf(buf, PAGE_SIZE, "%d\n",
				sensor->read_boot_sample);
}

static ssize_t read_acc_boot_sample_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	struct st_asm330lhh_sensor *sensor = iio_priv(dev_get_drvdata(dev));

	unsigned long enable = 0;

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable > 1) {
		err = dev_err(sensor->hw->dev,
				"Invalid value of input, input=%ld\n", enable);
		return -EINVAL;
	}
	err = asm_read_bootsampl(sensor, enable);
	if (err)
		return err;
	sensor->read_boot_sample = enable;
	return count;
}
#endif

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_asm330lhh_sysfs_sampling_frequency_avail);
static IIO_DEVICE_ATTR(in_accel_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
static IIO_DEVICE_ATTR(read_acc_boot_sample, 0444,
		read_acc_boot_sample_show, read_acc_boot_sample_store, 0);
static IIO_DEVICE_ATTR(read_gyro_boot_sample, 0444,
		read_gyro_boot_sample_show, read_gyro_boot_sample_store, 0);
#endif
static IIO_DEVICE_ATTR(in_anglvel_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(in_temp_scale_available, 0444,
		       st_asm330lhh_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_max, 0444,
		       st_asm330lhh_get_max_watermark, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_flush, 0200, NULL, st_asm330lhh_flush_fifo, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark, 0644, st_asm330lhh_get_watermark,
		       st_asm330lhh_set_watermark, 0);

static struct attribute *st_asm330lhh_acc_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
	&iio_dev_attr_read_acc_boot_sample.dev_attr.attr,
#endif
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
	.driver_module = THIS_MODULE,
	.attrs = &st_asm330lhh_acc_attribute_group,
	.read_raw = st_asm330lhh_read_raw,
	.write_raw = st_asm330lhh_write_raw,
};

static struct attribute *st_asm330lhh_gyro_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
	&iio_dev_attr_read_gyro_boot_sample.dev_attr.attr,
#endif
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
	.driver_module = THIS_MODULE,
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
	.driver_module = THIS_MODULE,
	.attrs = &st_asm330lhh_temp_attribute_group,
	.read_raw = st_asm330lhh_read_raw,
	.write_raw = st_asm330lhh_write_raw,
};

static const unsigned long st_asm330lhh_available_scan_masks[] = { 0x7, 0x0 };

static int st_asm330lhh_of_get_drdy_pin(struct st_asm330lhh_hw *hw, int *drdy_pin)
{
	struct device_node *np = hw->dev->of_node;

	if (!np)
		return -EINVAL;

	return of_property_read_u32_array(np, "st,drdy-int-pin", drdy_pin, 1);
}

static int st_asm330lhh_get_drdy_reg(struct st_asm330lhh_hw *hw, u8 *drdy_reg)
{
	int err = 0, drdy_pin;

	if (st_asm330lhh_of_get_drdy_pin(hw, &drdy_pin) < 0) {
		struct st_sensors_platform_data *pdata;
		struct device *dev = hw->dev;

		pdata = (struct st_sensors_platform_data *)dev->platform_data;
		drdy_pin = pdata ? pdata->drdy_int_pin : 1;
	}

	switch (drdy_pin) {
	case 1:
		*drdy_reg = ST_ASM330LHH_REG_INT1_ADDR;
		break;
	case 2:
		*drdy_reg = ST_ASM330LHH_REG_INT2_ADDR;
		break;
	default:
		dev_err(hw->dev, "unsupported data ready pin\n");
		err = -EINVAL;
		break;
	}

	return err;
}

static int st_asm330lhh_init_device(struct st_asm330lhh_hw *hw)
{
	u8 drdy_int_reg;
	int err;

	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_RESET_ADDR,
					   ST_ASM330LHH_REG_RESET_MASK, 1);
	if (err < 0)
		return err;

	msleep(200);

	/* latch interrupts */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_LIR_ADDR,
					   ST_ASM330LHH_REG_LIR_MASK, 1);
	if (err < 0)
		return err;

	/* enable Block Data Update */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_BDU_ADDR,
					   ST_ASM330LHH_REG_BDU_MASK, 1);
	if (err < 0)
		return err;

	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_ROUNDING_ADDR,
					   ST_ASM330LHH_REG_ROUNDING_MASK, 3);
	if (err < 0)
		return err;

	/* init timestamp engine */
	err = st_asm330lhh_write_with_mask(hw, ST_ASM330LHH_REG_TIMESTAMP_EN_ADDR,
					   ST_ASM330LHH_REG_TIMESTAMP_EN_MASK, 1);
	if (err < 0)
		return err;

	/* enable FIFO watermak interrupt */
	err = st_asm330lhh_get_drdy_reg(hw, &drdy_int_reg);
	if (err < 0)
		return err;

	return st_asm330lhh_write_with_mask(hw, drdy_int_reg,
					    ST_ASM330LHH_REG_FIFO_FTH_IRQ_MASK, 1);
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
	iio_dev->available_scan_masks = st_asm330lhh_available_scan_masks;

	sensor = iio_priv(iio_dev);
	sensor->id = id;
	sensor->hw = hw;
	sensor->odr = st_asm330lhh_odr_table[id].odr_avl[1].hz;
	sensor->gain = st_asm330lhh_fs_table[id].fs_avl[0].gain;
	sensor->watermark = 1;
	sensor->old_data = 0;

	switch (id) {
	case ST_ASM330LHH_ID_ACC:
		iio_dev->channels = st_asm330lhh_acc_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_acc_channels);
		iio_dev->name = "asm330lhh_accel";
		iio_dev->info = &st_asm330lhh_acc_info;
		sensor->batch_addr = ST_ASM330LHH_REG_FIFO_BATCH_ADDR;
		sensor->batch_mask = GENMASK(3, 0);
		sensor->offset = 0;
		break;
	case ST_ASM330LHH_ID_GYRO:
		iio_dev->channels = st_asm330lhh_gyro_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_gyro_channels);
		iio_dev->name = "asm330lhh_gyro";
		iio_dev->info = &st_asm330lhh_gyro_info;
		sensor->batch_addr = ST_ASM330LHH_REG_FIFO_BATCH_ADDR;
		sensor->batch_mask = GENMASK(7, 4);
		sensor->offset = 0;
		break;
	case ST_ASM330LHH_ID_TEMP:
		iio_dev->channels = st_asm330lhh_temp_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhh_temp_channels);
		iio_dev->name = "asm330lhh_temp";
		iio_dev->info = &st_asm330lhh_temp_info;
		sensor->offset = ST_ASM330LHH_OFFSET;
		break;
	default:
		return NULL;
	}

	return iio_dev;
}
#ifdef CONFIG_ENABLE_ASM_ACC_GYRO_BUFFERING
static void st_asm330lhh_enable_acc_gyro(struct st_asm330lhh_hw *hw)
{
	int i = 0;
	struct st_asm330lhh_sensor *sensor;
	int  acc_gain = ST_ASM330LHH_ACC_FS_2G_GAIN;
	int  gyro_gain = ST_ASM330LHH_GYRO_FS_125_GAIN;

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;
		sensor = iio_priv(hw->iio_devs[i]);
		sensor->odr = 104;
		sensor->watermark = 30;
		st_asm330lhh_update_fifo(hw->iio_devs[i], false);

		if (sensor->id == ST_ASM330LHH_ID_ACC)
			st_asm330lhh_set_full_scale(sensor, acc_gain);
		else if (sensor->id == ST_ASM330LHH_ID_GYRO)
			st_asm330lhh_set_full_scale(sensor, gyro_gain);

		st_asm330lhh_update_watermark(sensor, sensor->watermark);

		st_asm330lhh_update_fifo(hw->iio_devs[i], true);
	}
}

static int asm330_acc_gyro_early_buff_init(struct st_asm330lhh_hw *hw)
{
	int i = 0, err = 0;
	struct st_asm330lhh_sensor *acc;
	struct st_asm330lhh_sensor *gyro;

	acc = iio_priv(hw->iio_devs[ST_ASM330LHH_ID_ACC]);
	gyro = iio_priv(hw->iio_devs[ST_ASM330LHH_ID_GYRO]);

	acc->bufsample_cnt = 0;
	gyro->bufsample_cnt = 0;
	acc->report_evt_cnt = 5;
	gyro->report_evt_cnt = 5;
	acc->max_buffer_time = 40;
	gyro->max_buffer_time = 40;

	acc->asm_cachepool = kmem_cache_create("acc_sensor_sample",
			sizeof(struct asm_sample),
			0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!acc->asm_cachepool) {
		dev_err(hw->dev,
				"asm_acc_cachepool cache create failed\n");
		err = -ENOMEM;
		goto clean_exit1;
	}

	for (i = 0; i < ASM_MAXSAMPLE; i++) {
		acc->asm_samplist[i] =
			kmem_cache_alloc(acc->asm_cachepool,
					GFP_KERNEL);
		if (!acc->asm_samplist[i]) {
			err = -ENOMEM;
			goto clean_exit2;
		}
	}

	gyro->asm_cachepool = kmem_cache_create("gyro_sensor_sample"
			, sizeof(struct asm_sample), 0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!gyro->asm_cachepool) {
		dev_err(hw->dev,
				"asm_gyro_cachepool cache create failed\n");
		err = -ENOMEM;
		goto clean_exit3;
	}

	for (i = 0; i < ASM_MAXSAMPLE; i++) {
		gyro->asm_samplist[i] =
			kmem_cache_alloc(gyro->asm_cachepool,
					GFP_KERNEL);
		if (!gyro->asm_samplist[i]) {
			err = -ENOMEM;
			goto clean_exit4;
		}
	}

	acc->buf_dev = input_allocate_device();
	if (!acc->buf_dev) {
		err = -ENOMEM;
		dev_err(hw->dev, "input device allocation failed\n");
		goto clean_exit5;
	}
	acc->buf_dev->name = "asm_accbuf";
	acc->buf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(acc->buf_dev,
			acc->report_evt_cnt * ASM_MAXSAMPLE);
	set_bit(EV_ABS, acc->buf_dev->evbit);
	input_set_abs_params(acc->buf_dev, ABS_X,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->buf_dev, ABS_Y,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->buf_dev, ABS_Z,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->buf_dev, ABS_RX,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->buf_dev, ABS_RY,
			-G_MAX, G_MAX, 0, 0);
	err = input_register_device(acc->buf_dev);
	if (err) {
		dev_err(hw->dev,
				"unable to register input device %s\n",
				acc->buf_dev->name);
		goto clean_exit5;
	}

	gyro->buf_dev = input_allocate_device();
	if (!gyro->buf_dev) {
		err = -ENOMEM;
		dev_err(hw->dev, "input device allocation failed\n");
		goto clean_exit6;
	}
	gyro->buf_dev->name = "asm_gyrobuf";
	gyro->buf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(gyro->buf_dev,
			gyro->report_evt_cnt * ASM_MAXSAMPLE);
	set_bit(EV_ABS, gyro->buf_dev->evbit);
	input_set_abs_params(gyro->buf_dev, ABS_X,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(gyro->buf_dev, ABS_Y,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(gyro->buf_dev, ABS_Z,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(gyro->buf_dev, ABS_RX,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(gyro->buf_dev, ABS_RY,
			-G_MAX, G_MAX, 0, 0);
	err = input_register_device(gyro->buf_dev);
	if (err) {
		dev_err(hw->dev,
				"unable to register input device %s\n",
				gyro->buf_dev->name);
		goto clean_exit6;
	}

	acc->buffer_asm_samples = true;
	gyro->buffer_asm_samples = true;

	return 1;
clean_exit6:
	input_free_device(gyro->buf_dev);
	input_unregister_device(acc->buf_dev);
clean_exit5:
	input_free_device(acc->buf_dev);
clean_exit4:
	for (i = 0; i < ASM_MAXSAMPLE; i++)
		kmem_cache_free(gyro->asm_cachepool,
				gyro->asm_samplist[i]);
clean_exit3:
	kmem_cache_destroy(gyro->asm_cachepool);
clean_exit2:
	for (i = 0; i < ASM_MAXSAMPLE; i++)
		kmem_cache_free(acc->asm_cachepool,
				acc->asm_samplist[i]);
clean_exit1:
	kmem_cache_destroy(acc->asm_cachepool);
	return 0;
}
static void asm330_acc_gyro_input_cleanup(
		struct st_asm330lhh_hw *hw)
{
	int i = 0;
	struct st_asm330lhh_sensor *acc;
	struct st_asm330lhh_sensor *gyro;

	acc = iio_priv(hw->iio_devs[ST_ASM330LHH_ID_ACC]);
	gyro = iio_priv(hw->iio_devs[ST_ASM330LHH_ID_GYRO]);

	input_free_device(acc->buf_dev);
	input_unregister_device(gyro->buf_dev);
	input_free_device(gyro->buf_dev);
	for (i = 0; i < ASM_MAXSAMPLE; i++)
		kmem_cache_free(gyro->asm_cachepool,
				gyro->asm_samplist[i]);
	kmem_cache_destroy(gyro->asm_cachepool);
	for (i = 0; i < ASM_MAXSAMPLE; i++)
		kmem_cache_free(acc->asm_cachepool,
				acc->asm_samplist[i]);
	kmem_cache_destroy(acc->asm_cachepool);
}
#else
static void st_asm330lhh_enable_acc_gyro(struct st_asm330lhh_hw *hw)
{
}
static int asm330_acc_gyro_early_buff_init(struct st_asm330lhh_hw *hw)
{
	return 1;
}
static void asm330_acc_gyro_input_cleanup(struct st_asm330lhh_hw *hw)
{
}
#endif


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

	hw->dev = dev;
	hw->irq = irq;
	hw->tf = tf_ops;

	dev_info(hw->dev, "Ver: %s\n", ST_ASM330LHH_VERSION);
	err = st_asm330lhh_check_whoami(hw);
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
		err = st_asm330lhh_fifo_setup(hw);
		if (err < 0)
			return err;
	}

	for (i = 0; i < ST_ASM330LHH_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		err = devm_iio_device_register(hw->dev, hw->iio_devs[i]);
		if (err) {
			if (hw->irq > 0)
				st_asm330lhh_deallocate_fifo(hw);
			return err;
		}
	}

	err = asm330_acc_gyro_early_buff_init(hw);
	if (!err)
		return err;

	st_asm330lhh_enable_acc_gyro(hw);

	dev_info(hw->dev, "probe ok\n");

	return 0;
}

EXPORT_SYMBOL(st_asm330lhh_probe);

int st_asm330lhh_remove(struct device *dev)
{
	struct st_asm330lhh_hw *hw = dev_get_drvdata(dev);

	asm330_acc_gyro_input_cleanup(hw);

	return st_asm330lhh_deallocate_fifo(hw);
}
EXPORT_SYMBOL(st_asm330lhh_remove);

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

		err = st_asm330lhh_set_odr(sensor, 0);
		if (err < 0)
			return err;
	}

	if (hw->enable_mask)
		err = st_asm330lhh_suspend_fifo(hw);

	return err;
}

static int __maybe_unused st_asm330lhh_resume(struct device *dev)
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

		err = st_asm330lhh_set_odr(sensor, sensor->odr);
		if (err < 0)
			return err;
	}

	if (hw->enable_mask)
		err = st_asm330lhh_set_fifo_mode(hw, ST_ASM330LHH_FIFO_CONT);

	return err;
}

const struct dev_pm_ops st_asm330lhh_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_asm330lhh_suspend, st_asm330lhh_resume)
};
EXPORT_SYMBOL(st_asm330lhh_pm_ops);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_asm330lhh driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(ST_ASM330LHH_VERSION);

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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/of.h>

#include <linux/platform_data/st_sensors_pdata.h>

#include "st_asm330lhhx.h"

#define ST_ASM330LHHX_REG_FIFO_CTRL3_ADDR		0x09
#define ST_ASM330LHHX_REG_FIFO_CTRL4_ADDR		0x0a
#define ST_ASM330LHHX_REG_INT1_CTRL_ADDR		0x0d
#define ST_ASM330LHHX_REG_INT2_CTRL_ADDR		0x0e
#define ST_ASM330LHHX_REG_FIFO_TH_MASK			BIT(3)

#define ST_ASM330LHHX_REG_WHOAMI_ADDR			0x0f
#define ST_ASM330LHHX_WHOAMI_VAL			0x6b

#define ST_ASM330LHHX_CTRL1_XL_ADDR			0x10
#define ST_ASM330LHHX_CTRL2_G_ADDR			0x11

#define ST_ASM330LHHX_REG_CTRL3_C_ADDR			0x12
#define ST_ASM330LHHX_REG_SW_RESET_MASK			BIT(0)
#define ST_ASM330LHHX_REG_BOOT_MASK			BIT(7)
#define ST_ASM330LHHX_REG_BDU_MASK			BIT(6)

#define ST_ASM330LHHX_REG_CTRL4_C_ADDR			0x13
#define ST_ASM330LHHX_REG_DRDY_MASK			BIT(3)

#define ST_ASM330LHHX_REG_CTRL5_C_ADDR			0x14
#define ST_ASM330LHHX_REG_ROUNDING_MASK			GENMASK(6, 5)

#define ST_ASM330LHHX_REG_CTRL6_C_ADDR			0x15
#define ST_ASM330LHHX_REG_XL_HM_MODE_MASK		BIT(4)

#define ST_ASM330LHHX_REG_CTRL7_G_ADDR			0x16
#define ST_ASM330LHHX_REG_G_HM_MODE_MASK		BIT(7)

#define ST_ASM330LHHX_REG_CTRL10_C_ADDR			0x19
#define ST_ASM330LHHX_REG_TIMESTAMP_EN_MASK		BIT(5)

#define ST_ASM330LHHX_REG_STATUS_ADDR			0x1e
#define ST_ASM330LHHX_REG_STATUS_TDA			BIT(2)

#define ST_ASM330LHHX_REG_OUT_TEMP_L_ADDR		0x20
#define ST_ASM330LHHX_REG_OUT_TEMP_H_ADDR		0x21

#define ST_ASM330LHHX_REG_OUTX_L_A_ADDR			0x28
#define ST_ASM330LHHX_REG_OUTY_L_A_ADDR			0x2a
#define ST_ASM330LHHX_REG_OUTZ_L_A_ADDR			0x2c

#define ST_ASM330LHHX_REG_OUTX_L_G_ADDR			0x22
#define ST_ASM330LHHX_REG_OUTY_L_G_ADDR			0x24
#define ST_ASM330LHHX_REG_OUTZ_L_G_ADDR			0x26

#define ST_ASM330LHHX_REG_TAP_CFG0_ADDR			0x56
#define ST_ASM330LHHX_REG_LIR_MASK			BIT(0)

#define ST_ASM330LHHX_REG_MD1_CFG_ADDR			0x5e
#define ST_ASM330LHHX_REG_MD2_CFG_ADDR			0x5f
#define ST_ASM330LHHX_REG_INT2_TIMESTAMP_MASK		BIT(0)
#define ST_ASM330LHHX_REG_INT_EMB_FUNC_MASK		BIT(1)

#define ST_ASM330LHHX_INTERNAL_FREQ_FINE		0x63

/* Timestamp Tick 25us/LSB */
#define ST_ASM330LHHX_TS_DELTA_NS			25000ULL

/* Temperature in uC */
#define ST_ASM330LHHX_TEMP_GAIN				256
#define ST_ASM330LHHX_TEMP_OFFSET			6400

static const struct st_asm330lhhx_odr_table_entry st_asm330lhhx_odr_table[] = {
	[ST_ASM330LHHX_ID_ACC] = {
		.size = 8,
		.reg = {
			.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
			.mask = GENMASK(7, 4),
		},
		.pm = {
			.addr = ST_ASM330LHHX_REG_CTRL6_C_ADDR,
			.mask = ST_ASM330LHHX_REG_XL_HM_MODE_MASK,
		},
		.batching_reg = {
			.addr = ST_ASM330LHHX_REG_FIFO_CTRL3_ADDR,
			.mask = GENMASK(3, 0),
		},
		.odr_avl[0] = {   1, 600000,  0x01,  0x0b },
		.odr_avl[1] = {  12, 500000,  0x01,  0x01 },
		.odr_avl[2] = {  26,      0,  0x02,  0x02 },
		.odr_avl[3] = {  52,      0,  0x03,  0x03 },
		.odr_avl[4] = { 104,      0,  0x04,  0x04 },
		.odr_avl[5] = { 208,      0,  0x05,  0x05 },
		.odr_avl[6] = { 416,      0,  0x06,  0x06 },
		.odr_avl[7] = { 833,      0,  0x07,  0x07 },
	},
	[ST_ASM330LHHX_ID_GYRO] = {
		.size = 8,
		.reg = {
			.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
			.mask = GENMASK(7, 4),
		},
		.pm = {
			.addr = ST_ASM330LHHX_REG_CTRL7_G_ADDR,
			.mask = ST_ASM330LHHX_REG_G_HM_MODE_MASK,
		},
		.batching_reg = {
			.addr = ST_ASM330LHHX_REG_FIFO_CTRL3_ADDR,
			.mask = GENMASK(7, 4),
		},
		.odr_avl[0] = {   6, 500000,  0x01,  0x0b },
		.odr_avl[1] = {  12, 500000,  0x01,  0x01 },
		.odr_avl[2] = {  26,      0,  0x02,  0x02 },
		.odr_avl[3] = {  52,      0,  0x03,  0x03 },
		.odr_avl[4] = { 104,      0,  0x04,  0x04 },
		.odr_avl[5] = { 208,      0,  0x05,  0x05 },
		.odr_avl[6] = { 416,      0,  0x06,  0x06 },
		.odr_avl[7] = { 833,      0,  0x07,  0x07 },
	},
	[ST_ASM330LHHX_ID_TEMP] = {
		.size = 3,
		.batching_reg = {
			.addr = ST_ASM330LHHX_REG_FIFO_CTRL4_ADDR,
			.mask = GENMASK(5, 4),
		},
		.odr_avl[0] = {  1, 600000,   0x01,  0x01 },
		.odr_avl[1] = { 12, 500000,   0x02,  0x02 },
		.odr_avl[2] = { 52,      0,   0x03,  0x03 },
	},
};

static const struct st_asm330lhhx_fs_table_entry st_asm330lhhx_fs_table[] = {
	[ST_ASM330LHHX_ID_ACC] = {
		.size = 4,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = IIO_G_TO_M_S_2(61),
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = IIO_G_TO_M_S_2(122),
			.val = 0x2,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = IIO_G_TO_M_S_2(244),
			.val = 0x3,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
				.mask = GENMASK(3, 2),
			},
			.gain = IIO_G_TO_M_S_2(488),
			.val = 0x1,
		},
	},
	[ST_ASM330LHHX_ID_GYRO] = {
		.size = 5,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = IIO_DEGREE_TO_RAD(8750),
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = IIO_DEGREE_TO_RAD(17500),
			.val = 0x4,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = IIO_DEGREE_TO_RAD(35000),
			.val = 0x8,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = IIO_DEGREE_TO_RAD(70000),
			.val = 0x0C,
		},
		.fs_avl[4] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = GENMASK(3, 0),
			},
			.gain = IIO_DEGREE_TO_RAD(140000),
			.val = 0x1,
		},
	},
	[ST_ASM330LHHX_ID_TEMP] = {
		.size = 1,
		.fs_avl[0] = {
			.reg = { 0 },
			.gain = (1000000 / ST_ASM330LHHX_TEMP_GAIN),
			.val = 0x0
		},
	},
};

static const struct iio_chan_spec st_asm330lhhx_acc_channels[] = {
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHHX_REG_OUTX_L_A_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHHX_REG_OUTY_L_A_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ACCEL, ST_ASM330LHHX_REG_OUTZ_L_A_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_ASM330LHHX_EVENT_CHANNEL(IIO_ACCEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_asm330lhhx_gyro_channels[] = {
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHHX_REG_OUTX_L_G_ADDR,
				1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHHX_REG_OUTY_L_G_ADDR,
				1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ANGL_VEL, ST_ASM330LHHX_REG_OUTZ_L_G_ADDR,
				1, IIO_MOD_Z, 2, 16, 16, 's'),
	ST_ASM330LHHX_EVENT_CHANNEL(IIO_ANGL_VEL, flush),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_asm330lhhx_temp_channels[] = {
	{
		.type = IIO_TEMP,
		.address = ST_ASM330LHHX_REG_OUT_TEMP_L_ADDR,
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
	ST_ASM330LHHX_EVENT_CHANNEL(IIO_TEMP, flush),
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

static int st_asm330lhhx_check_whoami(struct st_asm330lhhx_hw *hw)
{
	int err;
	int data;

	err = regmap_read(hw->regmap, ST_ASM330LHHX_REG_WHOAMI_ADDR, &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read whoami register\n");
		return err;
	}

	if (data != ST_ASM330LHHX_WHOAMI_VAL) {
		dev_err(hw->dev, "unsupported whoami [%02x]\n", data);
		return -ENODEV;
	}

	return 0;
}

static int st_asm330lhhx_get_odr_calibration(struct st_asm330lhhx_hw *hw)
{
	int err;
	int data;
	s64 odr_calib;

	err = regmap_read(hw->regmap, ST_ASM330LHHX_INTERNAL_FREQ_FINE, &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %d register\n",
				ST_ASM330LHHX_INTERNAL_FREQ_FINE);
		return err;
	}

	odr_calib = ((s8)data * 37500) / 1000;
	hw->ts_delta_ns = ST_ASM330LHHX_TS_DELTA_NS - odr_calib;

	dev_info(hw->dev, "Freq Fine %lld (ts %lld)\n",
		 odr_calib, hw->ts_delta_ns);

	return 0;
}

static int st_asm330lhhx_set_full_scale(struct st_asm330lhhx_sensor *sensor,
				     u32 gain)
{
	enum st_asm330lhhx_sensor_id id = sensor->id;
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int i, err;
	u8 val;

	for (i = 0; i < st_asm330lhhx_fs_table[id].size; i++)
		if (st_asm330lhhx_fs_table[id].fs_avl[i].gain == gain)
			break;

	if (i == st_asm330lhhx_fs_table[id].size)
		return -EINVAL;

	val = st_asm330lhhx_fs_table[id].fs_avl[i].val;
	err = regmap_update_bits(hw->regmap,
				 st_asm330lhhx_fs_table[id].fs_avl[i].reg.addr,
				 st_asm330lhhx_fs_table[id].fs_avl[i].reg.mask,
				 ST_ASM330LHHX_SHIFT_VAL(val,
				st_asm330lhhx_fs_table[id].fs_avl[i].reg.mask));
	if (err < 0)
		return err;

	sensor->gain = gain;

	return 0;
}

int st_asm330lhhx_get_odr_val(struct st_asm330lhhx_sensor *sensor, int odr,
			     int uodr, int *podr, int *puodr, u8 *val)
{
	enum st_asm330lhhx_sensor_id id = sensor->id;
	int all_odr = ST_ASM330LHHX_ODR_EXPAND(odr, uodr);
	int i;
	int sensor_odr;

	for (i = 0; i < st_asm330lhhx_odr_table[id].size; i++) {
		sensor_odr = ST_ASM330LHHX_ODR_EXPAND(
				st_asm330lhhx_odr_table[id].odr_avl[i].hz,
				st_asm330lhhx_odr_table[id].odr_avl[i].uhz);
		if (sensor_odr >= all_odr)
			break;
	}

	if (i == st_asm330lhhx_odr_table[id].size)
		return -EINVAL;

	*val = st_asm330lhhx_odr_table[id].odr_avl[i].val;
	*podr = st_asm330lhhx_odr_table[id].odr_avl[i].hz;
	*puodr = st_asm330lhhx_odr_table[id].odr_avl[i].uhz;

	return 0;
}

int st_asm330lhhx_get_batch_val(struct st_asm330lhhx_sensor *sensor, int odr,
			       int uodr, u8 *val)
{
	enum st_asm330lhhx_sensor_id id = sensor->id;
	int all_odr = ST_ASM330LHHX_ODR_EXPAND(odr, uodr);
	int i;
	int sensor_odr;

	for (i = 0; i < st_asm330lhhx_odr_table[id].size; i++) {
		sensor_odr = ST_ASM330LHHX_ODR_EXPAND(
				st_asm330lhhx_odr_table[id].odr_avl[i].hz,
				st_asm330lhhx_odr_table[id].odr_avl[i].uhz);
		if (sensor_odr >= all_odr)
			break;
	}

	if (i == st_asm330lhhx_odr_table[id].size)
		return -EINVAL;

	*val = st_asm330lhhx_odr_table[id].odr_avl[i].batch_val;

	return 0;
}


static u16 st_asm330lhhx_check_odr_dependency(struct st_asm330lhhx_hw *hw,
					   int odr, int uodr,
					   enum st_asm330lhhx_sensor_id ref_id)
{
	struct st_asm330lhhx_sensor *ref = iio_priv(hw->iio_devs[ref_id]);
	bool enable = odr > 0;
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

static int st_asm330lhhx_set_odr(struct st_asm330lhhx_sensor *sensor, int req_odr,
				int req_uodr)
{
	struct st_asm330lhhx_hw *hw = sensor->hw;
	enum st_asm330lhhx_sensor_id id = sensor->id;
	int err;
	u8 val = 0;

	switch (id) {
	case ST_ASM330LHHX_ID_TEMP:
	case ST_ASM330LHHX_ID_ACC: {
		int odr;
		int i;

		id = ST_ASM330LHHX_ID_ACC;
		for (i = ST_ASM330LHHX_ID_ACC; i < ST_ASM330LHHX_ID_MAX; i++) {
			if (!hw->iio_devs[i])
				continue;

			if (i == sensor->id)
				continue;

			odr = st_asm330lhhx_check_odr_dependency(hw, req_odr,
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

	err = st_asm330lhhx_get_odr_val(sensor, req_odr, req_uodr, &req_odr,
				       &req_uodr, &val);
	if (err < 0)
		return err;

	/* check if sensor supports power mode setting */
	if (sensor->pm != ST_ASM330LHHX_NO_MODE) {
		err = regmap_update_bits(hw->regmap,
					 st_asm330lhhx_odr_table[sensor->id].pm.addr,
					 st_asm330lhhx_odr_table[sensor->id].pm.mask,
					 ST_ASM330LHHX_SHIFT_VAL(sensor->pm,
						st_asm330lhhx_odr_table[sensor->id].pm.mask));
		if (err < 0)
			return err;
	}

	return regmap_update_bits(hw->regmap,
				  st_asm330lhhx_odr_table[sensor->id].reg.addr,
				  st_asm330lhhx_odr_table[sensor->id].reg.mask,
				  ST_ASM330LHHX_SHIFT_VAL(val,
					st_asm330lhhx_odr_table[sensor->id].reg.mask));
}

int st_asm330lhhx_sensor_set_enable(struct st_asm330lhhx_sensor *sensor,
				   bool enable)
{
	int uodr = enable ? sensor->uodr : 0;
	int odr = enable ? sensor->odr : 0;
	int err;

	err = st_asm330lhhx_set_odr(sensor, odr, uodr);
	if (err < 0)
		return err;

	if (enable)
		sensor->hw->enable_mask |= BIT(sensor->id);
	else
		sensor->hw->enable_mask &= ~BIT(sensor->id);

	return 0;
}

static int st_asm330lhhx_read_oneshot(struct st_asm330lhhx_sensor *sensor,
				   u8 addr, int *val)
{
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int err, delay;
	__le16 data;

	if (sensor->id == ST_ASM330LHHX_ID_TEMP) {
		u8 status;

		err = st_asm330lhhx_read_locked(hw,
					       ST_ASM330LHHX_REG_STATUS_ADDR,
					       &status, sizeof(status));
		if (err < 0)
			return err;

		if (status & ST_ASM330LHHX_REG_STATUS_TDA) {
			err = st_asm330lhhx_read_locked(hw, addr,
						       &data, sizeof(data));
			if (err < 0)
				return err;

			sensor->old_data = data;
		} else {
			data = sensor->old_data;
		}
	} else {
		err = st_asm330lhhx_sensor_set_enable(sensor, true);
		if (err < 0)
			return err;

		/* Use big delay for data valid because of drdy mask enabled */
		delay = 10000000 / (sensor->odr + sensor->uodr);
		usleep_range(delay, 2 * delay);

		err = st_asm330lhhx_read_locked(hw, addr,
				       &data, sizeof(data));

		st_asm330lhhx_sensor_set_enable(sensor, false);
		if (err < 0)
			return err;
	}

	*val = (s16)le16_to_cpu(data);

	return IIO_VAL_INT;
}

static int st_asm330lhhx_read_raw(struct iio_dev *iio_dev,
				 struct iio_chan_spec const *ch,
				 int *val, int *val2, long mask)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(iio_dev);
		if (ret)
			return ret;

		ret = st_asm330lhhx_read_oneshot(sensor, ch->address, val);
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
			*val2 = ST_ASM330LHHX_TEMP_GAIN;
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

static int st_asm330lhhx_write_raw(struct iio_dev *iio_dev,
				  struct iio_chan_spec const *chan,
				  int val, int val2, long mask)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);
	int err;

	mutex_lock(&iio_dev->mlock);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		err = st_asm330lhhx_set_full_scale(sensor, val2);
		break;
	case IIO_CHAN_INFO_SAMP_FREQ: {
		int todr, tuodr;
		u8 data;

		err = st_asm330lhhx_get_odr_val(sensor, val, val2, &todr,
					       &tuodr, &data);
		if (!err) {
			sensor->odr = val;
			sensor->uodr = tuodr;

			/*
			 * VTS test testSamplingRateHotSwitchOperation not
			 * toggle the enable status of sensor after changing
			 * the ODR -> force it
			 */
			if (sensor->hw->enable_mask & BIT(sensor->id)) {
				switch (sensor->id) {
				case ST_ASM330LHHX_ID_GYRO:
				case ST_ASM330LHHX_ID_ACC: {
					err = st_asm330lhhx_set_odr(sensor,
								   sensor->odr,
								   sensor->uodr);
					if (err < 0)
						break;

					st_asm330lhhx_update_batching(iio_dev, 1);
					}
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

	mutex_unlock(&iio_dev->mlock);

	return err;
}

static ssize_t
st_asm330lhhx_sysfs_sampling_frequency_avail(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_asm330lhhx_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < st_asm330lhhx_odr_table[id].size; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d.%06d ",
				 st_asm330lhhx_odr_table[id].odr_avl[i].hz,
				 st_asm330lhhx_odr_table[id].odr_avl[i].uhz);
	}

	buf[len - 1] = '\n';

	return len;
}

static ssize_t st_asm330lhhx_sysfs_scale_avail(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_asm330lhhx_sensor_id id = sensor->id;
	int i, len = 0;

	for (i = 0; i < st_asm330lhhx_fs_table[id].size; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0.%06u ",
				 st_asm330lhhx_fs_table[id].fs_avl[i].gain);
	buf[len - 1] = '\n';

	return len;
}

ssize_t st_asm330lhhx_get_power_mode(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);

	return sprintf(buf, "%d\n", sensor->pm);
}

ssize_t st_asm330lhhx_set_power_mode(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);
	int err, val;

	err = iio_device_claim_direct_mode(iio_dev);
	if (err)
		return err;

	err = kstrtoint(buf, 10, &val);
	if (err < 0)
		goto out;

	if (val >= ST_ASM330LHHX_HP_MODE && val < ST_ASM330LHHX_NO_MODE)
		sensor->pm = val;
	else
		err = -EINVAL;

out:
	iio_device_release_direct_mode(iio_dev);

	return err < 0 ? err : size;
}

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_asm330lhhx_sysfs_sampling_frequency_avail);
static IIO_DEVICE_ATTR(in_accel_scale_available, 0444,
		       st_asm330lhhx_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(in_anglvel_scale_available, 0444,
		       st_asm330lhhx_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(in_temp_scale_available, 0444,
		       st_asm330lhhx_sysfs_scale_avail, NULL, 0);

static IIO_DEVICE_ATTR(hwfifo_watermark_max, 0444,
		       st_asm330lhhx_get_max_watermark, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_flush, 0200, NULL, st_asm330lhhx_flush_fifo, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark, 0644, st_asm330lhhx_get_watermark,
		       st_asm330lhhx_set_watermark, 0);

static IIO_DEVICE_ATTR(power_mode, 0644,
		       st_asm330lhhx_get_power_mode,
		       st_asm330lhhx_set_power_mode, 0);

static struct attribute *st_asm330lhhx_acc_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_accel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	&iio_dev_attr_power_mode.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhhx_acc_attribute_group = {
	.attrs = st_asm330lhhx_acc_attributes,
};

static const struct iio_info st_asm330lhhx_acc_info = {
	.attrs = &st_asm330lhhx_acc_attribute_group,
	.read_raw = st_asm330lhhx_read_raw,
	.write_raw = st_asm330lhhx_write_raw,
};

static struct attribute *st_asm330lhhx_gyro_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	&iio_dev_attr_power_mode.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhhx_gyro_attribute_group = {
	.attrs = st_asm330lhhx_gyro_attributes,
};

static const struct iio_info st_asm330lhhx_gyro_info = {
	.attrs = &st_asm330lhhx_gyro_attribute_group,
	.read_raw = st_asm330lhhx_read_raw,
	.write_raw = st_asm330lhhx_write_raw,
};

static struct attribute *st_asm330lhhx_temp_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_temp_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhhx_temp_attribute_group = {
	.attrs = st_asm330lhhx_temp_attributes,
};

static const struct iio_info st_asm330lhhx_temp_info = {
	.attrs = &st_asm330lhhx_temp_attribute_group,
	.read_raw = st_asm330lhhx_read_raw,
	.write_raw = st_asm330lhhx_write_raw,
};

static const unsigned long st_asm330lhhx_available_scan_masks[] = { 0x7, 0x0 };
static const unsigned long st_asm330lhhx_temp_available_scan_masks[] = { 0x1, 0x0 };

int st_asm330lhhx_of_get_pin(struct st_asm330lhhx_hw *hw, int *pin)
{
	struct device_node *np = hw->dev->of_node;

	if (!np)
		return -EINVAL;

	return of_property_read_u32(np, "st,int-pin", pin);
}

static int st_asm330lhhx_get_int_reg(struct st_asm330lhhx_hw *hw, u8 *drdy_reg)
{
	int err = 0, int_pin;

	if (st_asm330lhhx_of_get_pin(hw, &int_pin) < 0) {
		struct st_sensors_platform_data *pdata;
		struct device *dev = hw->dev;

		pdata = (struct st_sensors_platform_data *)dev->platform_data;
		int_pin = pdata ? pdata->drdy_int_pin : 1;
	}

	switch (int_pin) {
	case 1:
		*drdy_reg = ST_ASM330LHHX_REG_INT1_CTRL_ADDR;
		break;
	case 2:
		*drdy_reg = ST_ASM330LHHX_REG_INT2_CTRL_ADDR;
		break;
	default:
		dev_err(hw->dev, "unsupported interrupt pin\n");
		err = -EINVAL;
		break;
	}

	return err;
}

static int st_asm330lhhx_reset_device(struct st_asm330lhhx_hw *hw)
{
	int err;

	/* sw reset */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL3_C_ADDR,
				 ST_ASM330LHHX_REG_SW_RESET_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_SW_RESET_MASK, 1));
	if (err < 0)
		return err;

	msleep(50);

	/* boot */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL3_C_ADDR,
				 ST_ASM330LHHX_REG_BOOT_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_BOOT_MASK, 1));

	msleep(50);

	return err;
}

static int st_asm330lhhx_init_timestamp_engine(struct st_asm330lhhx_hw *hw,
					    bool enable)
{
	int err;

	/* Init timestamp engine. */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL10_C_ADDR,
				 ST_ASM330LHHX_REG_TIMESTAMP_EN_MASK,
				 ST_ASM330LHHX_SHIFT_VAL(true,
				 			ST_ASM330LHHX_REG_TIMESTAMP_EN_MASK));
	if (err < 0)
		return err;

	/* Enable timestamp rollover interrupt on INT2. */
	return regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_MD2_CFG_ADDR,
				 ST_ASM330LHHX_REG_INT2_TIMESTAMP_MASK,
				 ST_ASM330LHHX_SHIFT_VAL(enable,
				 			ST_ASM330LHHX_REG_INT2_TIMESTAMP_MASK));
}

static int st_asm330lhhx_init_device(struct st_asm330lhhx_hw *hw)
{
	u8 drdy_reg;
	int err;

	/* latch interrupts */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_TAP_CFG0_ADDR,
				 ST_ASM330LHHX_REG_LIR_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_LIR_MASK, 1));
	if (err < 0)
		return err;

	/* enable Block Data Update */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL3_C_ADDR,
				 ST_ASM330LHHX_REG_BDU_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_BDU_MASK, 1));
	if (err < 0)
		return err;

	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL5_C_ADDR,
				 ST_ASM330LHHX_REG_ROUNDING_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_ROUNDING_MASK, 3));
	if (err < 0)
		return err;

	err = st_asm330lhhx_init_timestamp_engine(hw, true);
	if (err < 0)
		return err;

	err = st_asm330lhhx_get_int_reg(hw, &drdy_reg);
	if (err < 0)
		return err;

	/* Enable DRDY MASK for filters settling time */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL4_C_ADDR,
				 ST_ASM330LHHX_REG_DRDY_MASK,
				 ST_ASM330LHHX_SHIFT_VAL(1,
				 			ST_ASM330LHHX_REG_DRDY_MASK));

	if (err < 0)
		return err;

	/* enable FIFO watermak interrupt */
	return regmap_update_bits(hw->regmap, drdy_reg,
				 ST_ASM330LHHX_REG_FIFO_TH_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_FIFO_TH_MASK, 1));
}

static struct iio_dev *st_asm330lhhx_alloc_iiodev(struct st_asm330lhhx_hw *hw,
					       enum st_asm330lhhx_sensor_id id)
{
	struct st_asm330lhhx_sensor *sensor;
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

	/* Set default ODR/FS to each sensor. */
	sensor->odr = st_asm330lhhx_odr_table[id].odr_avl[0].hz;
	sensor->uodr = st_asm330lhhx_odr_table[id].odr_avl[0].uhz;
	sensor->gain = st_asm330lhhx_fs_table[id].fs_avl[0].gain;

	switch (id) {
	case ST_ASM330LHHX_ID_ACC:
		iio_dev->channels = st_asm330lhhx_acc_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhhx_acc_channels);
		iio_dev->name = "asm330lhhx_accel";
		iio_dev->info = &st_asm330lhhx_acc_info;
		iio_dev->available_scan_masks =
					st_asm330lhhx_available_scan_masks;
		sensor->max_watermark = ST_ASM330LHHX_MAX_FIFO_DEPTH;
		sensor->offset = 0;
		sensor->pm = ST_ASM330LHHX_HP_MODE;
		break;
	case ST_ASM330LHHX_ID_GYRO:
		iio_dev->channels = st_asm330lhhx_gyro_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhhx_gyro_channels);
		iio_dev->name = "asm330lhhx_gyro";
		iio_dev->info = &st_asm330lhhx_gyro_info;
		iio_dev->available_scan_masks =
					st_asm330lhhx_available_scan_masks;
		sensor->max_watermark = ST_ASM330LHHX_MAX_FIFO_DEPTH;
		sensor->offset = 0;
		sensor->pm = ST_ASM330LHHX_HP_MODE;
		break;
	case ST_ASM330LHHX_ID_TEMP:
		iio_dev->channels = st_asm330lhhx_temp_channels;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhhx_temp_channels);
		iio_dev->name = "asm330lhhx_temp";
		iio_dev->info = &st_asm330lhhx_temp_info;
		iio_dev->available_scan_masks =
					st_asm330lhhx_temp_available_scan_masks;
		sensor->max_watermark = ST_ASM330LHHX_MAX_FIFO_DEPTH;
		sensor->offset = ST_ASM330LHHX_TEMP_OFFSET;
		sensor->pm = ST_ASM330LHHX_NO_MODE;
		break;
	default:
		return NULL;
	}

	return iio_dev;
}

int st_asm330lhhx_probe(struct device *dev, int irq,
		       struct regmap *regmap)
{
	struct st_asm330lhhx_hw *hw;
	int i, err;

	hw = devm_kzalloc(dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	dev_set_drvdata(dev, (void *)hw);

	mutex_init(&hw->fifo_lock);
	mutex_init(&hw->page_lock);

	hw->regmap = regmap;
	hw->dev = dev;
	hw->irq = irq;
	hw->odr_table_entry = st_asm330lhhx_odr_table;

	err = st_asm330lhhx_check_whoami(hw);
	if (err < 0)
		return err;

	err = st_asm330lhhx_get_odr_calibration(hw);
	if (err < 0)
		return err;

	err = st_asm330lhhx_reset_device(hw);
	if (err < 0)
		return err;

	err = st_asm330lhhx_init_device(hw);
	if (err < 0)
		return err;

	/* register only data sensors */
	for (i = 0; i <= ST_ASM330LHHX_ID_TEMP; i++) {
		hw->iio_devs[i] = st_asm330lhhx_alloc_iiodev(hw, i);
		if (!hw->iio_devs[i])
			return -ENOMEM;
	}

	if (hw->irq > 0) {
		err = st_asm330lhhx_buffers_setup(hw);
		if (err < 0)
			return err;
	}

#ifdef CONFIG_IIO_ST_ASM330LHHX_MLC
	err = st_asm330lhhx_mlc_probe(hw);
	if (err < 0)
		return err;
#endif /* CONFIG_IIO_ST_ASM330LHHX_MLC */

#ifdef CONFIG_IIO_ST_ASM330LHHX_FSM
	err = st_asm330lhhx_fsm_probe(hw);
	if (err < 0)
		return err;
#endif /* CONFIG_IIO_ST_ASM330LHHX_FSM */

	for (i = 0; i < ST_ASM330LHHX_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		err = devm_iio_device_register(hw->dev, hw->iio_devs[i]);
		if (err)
			return err;
	}

#if defined(CONFIG_PM) && defined(CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP)
	err = device_init_wakeup(dev, 1);
	if (err)
		return err;
#endif /* CONFIG_PM && CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP */

	dev_info(dev, "Device probed v%s\n", ST_ASM330LHHX_DRV_VERSION);

	return 0;
}
EXPORT_SYMBOL(st_asm330lhhx_probe);

static int __maybe_unused st_asm330lhhx_suspend(struct device *dev)
{
	struct st_asm330lhhx_hw *hw = dev_get_drvdata(dev);
	struct st_asm330lhhx_sensor *sensor;
	int i, err = 0;

	for (i = 0; i < ST_ASM330LHHX_ID_MAX; i++) {
		sensor = iio_priv(hw->iio_devs[i]);
		if (!hw->iio_devs[i])
			continue;

		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_asm330lhhx_set_odr(sensor, 0, 0);
		if (err < 0)
			return err;
	}

	if (st_asm330lhhx_is_fifo_enabled(hw))
		err = st_asm330lhhx_suspend_fifo(hw);
#ifdef CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP
	if (device_may_wakeup(dev))
		enable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP */
	dev_info(dev, "Suspending device\n");

	return err;
}

static int __maybe_unused st_asm330lhhx_resume(struct device *dev)
{
	struct st_asm330lhhx_hw *hw = dev_get_drvdata(dev);
	struct st_asm330lhhx_sensor *sensor;
	int i, err = 0;

	dev_info(dev, "Resuming device\n");
#ifdef CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP
	if (device_may_wakeup(dev))
		disable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP */

	for (i = 0; i < ST_ASM330LHHX_ID_MAX; i++) {
		sensor = iio_priv(hw->iio_devs[i]);
		if (!hw->iio_devs[i])
			continue;

		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_asm330lhhx_set_odr(sensor, sensor->odr, sensor->uodr);
		if (err < 0)
			return err;
	}

	if (st_asm330lhhx_is_fifo_enabled(hw))
		err = st_asm330lhhx_set_fifo_mode(hw, ST_ASM330LHHX_FIFO_CONT);

	return err;
}

const struct dev_pm_ops st_asm330lhhx_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_asm330lhhx_suspend, st_asm330lhhx_resume)
};
EXPORT_SYMBOL(st_asm330lhhx_pm_ops);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_AUTHOR("Mario Tesi <mario.tesi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_asm330lhhx driver");
MODULE_LICENSE("GPL v2");

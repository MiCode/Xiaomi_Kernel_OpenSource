// SPDX-License-Identifier: GPL-2.0-only
/*
 * STMicroelectronics st_asm330lhhx sensor driver
 *
 * Copyright 2021 STMicroelectronics Inc.
 *
 * Tesi Mario <mario.tesi@st.com>
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

/* use for debug suspend resume */
static int __maybe_unused _st_asm330lhhx_resume(struct st_asm330lhhx_hw *hw);
static int __maybe_unused _st_asm330lhhx_suspend(struct st_asm330lhhx_hw *hw);

static struct st_asm330lhhx_selftest_table {
	char *string_mode;
	u8 accel_value;
	u8 gyro_value;
	u8 gyro_mask;
} st_asm330lhhx_selftest_table[] = {
	[0] = {
		.string_mode = "disabled",
		.accel_value = ST_ASM330LHHX_SELF_TEST_DISABLED_VAL,
		.gyro_value = ST_ASM330LHHX_SELF_TEST_DISABLED_VAL,
	},
	[1] = {
		.string_mode = "positive-sign",
		.accel_value = ST_ASM330LHHX_SELF_TEST_POS_SIGN_VAL,
		.gyro_value = ST_ASM330LHHX_SELF_TEST_POS_SIGN_VAL
	},
	[2] = {
		.string_mode = "negative-sign",
		.accel_value = ST_ASM330LHHX_SELF_TEST_NEG_ACCEL_SIGN_VAL,
		.gyro_value = ST_ASM330LHHX_SELF_TEST_NEG_GYRO_SIGN_VAL
	},
};

static struct st_asm330lhhx_suspend_resume_entry
	st_asm330lhhx_suspend_resume[ST_ASM330LHHX_SUSPEND_RESUME_REGS] =
{
	//[ST_ASM330LHHX_CTRL1_XL_REG] = {
		//.page = FUNC_CFG_ACCESS_0,
		//.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
		//.mask = GENMASK(3, 2),
	//},
	[ST_ASM330LHHX_CTRL2_G_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
		.mask = GENMASK(3, 2),
	},
	[ST_ASM330LHHX_REG_CTRL3_C_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_CTRL3_C_ADDR,
		.mask = ST_ASM330LHHX_REG_BDU_MASK	|
			ST_ASM330LHHX_REG_PP_OD_MASK	|
			ST_ASM330LHHX_REG_H_LACTIVE_MASK,
	},
	[ST_ASM330LHHX_REG_CTRL4_C_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_CTRL4_C_ADDR,
		.mask = ST_ASM330LHHX_REG_DRDY_MASK,
	},
	[ST_ASM330LHHX_REG_CTRL5_C_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_CTRL5_C_ADDR,
		.mask = ST_ASM330LHHX_REG_ROUNDING_MASK,
	},
	//[ST_ASM330LHHX_REG_CTRL6_C_REG] = {
		//.page = FUNC_CFG_ACCESS_0,
		//.addr = ST_ASM330LHHX_REG_CTRL6_C_ADDR,
		//.mask = ST_ASM330LHHX_REG_XL_HM_MODE_MASK,
	//},
	[ST_ASM330LHHX_REG_CTRL10_C_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_CTRL10_C_ADDR,
		.mask = ST_ASM330LHHX_REG_TIMESTAMP_EN_MASK,
	},
	[ST_ASM330LHHX_REG_TAP_CFG0_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_TAP_CFG0_ADDR,
		.mask = ST_ASM330LHHX_REG_LIR_MASK,
	},
	[ST_ASM330LHHX_REG_INT1_CTRL_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_INT1_CTRL_ADDR,
		.mask = ST_ASM330LHHX_REG_FIFO_TH_MASK,
	},
	[ST_ASM330LHHX_REG_INT2_CTRL_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_INT2_CTRL_ADDR,
		.mask = ST_ASM330LHHX_REG_FIFO_TH_MASK,
	},
	[ST_ASM330LHHX_REG_FIFO_CTRL1_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_FIFO_CTRL1_ADDR,
		.mask = GENMASK(7, 0),
	},
	[ST_ASM330LHHX_REG_FIFO_CTRL2_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_FIFO_CTRL2_ADDR,
		.mask = ST_ASM330LHHX_REG_FIFO_WTM8_MASK,
	},
	[ST_ASM330LHHX_REG_FIFO_CTRL3_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_FIFO_CTRL3_ADDR,
		.mask = ST_ASM330LHHX_REG_BDR_XL_MASK |
			ST_ASM330LHHX_REG_BDR_GY_MASK,
	},
	[ST_ASM330LHHX_REG_FIFO_CTRL4_REG] = {
		.page = FUNC_CFG_ACCESS_0,
		.addr = ST_ASM330LHHX_REG_FIFO_CTRL4_ADDR,
		.mask = ST_ASM330LHHX_REG_DEC_TS_MASK |
			ST_ASM330LHHX_REG_ODR_T_BATCH_MASK,
	},
#ifdef ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS
	[ST_ASM330LHHX_REG_EMB_FUNC_EN_B_REG] = {
		.page = FUNC_CFG_ACCESS_FUNC_CFG,
		.addr = ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
		.mask = ST_ASM330LHHX_FSM_EN_MASK |
			ST_ASM330LHHX_MLC_EN_MASK,
	},
	[ST_ASM330LHHX_REG_FSM_INT1_A_REG] = {
		.page = FUNC_CFG_ACCESS_FUNC_CFG,
		.addr = ST_ASM330LHHX_FSM_INT1_A_ADDR,
		.mask = GENMASK(7, 0),
	},
	[ST_ASM330LHHX_REG_FSM_INT1_B_REG] = {
		.page = FUNC_CFG_ACCESS_FUNC_CFG,
		.addr = ST_ASM330LHHX_FSM_INT1_B_ADDR,
		.mask = GENMASK(7, 0),
	},
	[ST_ASM330LHHX_REG_MLC_INT1_REG] = {
		.page = FUNC_CFG_ACCESS_FUNC_CFG,
		.addr = ST_ASM330LHHX_MLC_INT1_ADDR,
		.mask = GENMASK(7, 0),
	},
	[ST_ASM330LHHX_REG_FSM_INT2_A_REG] = {
		.page = FUNC_CFG_ACCESS_FUNC_CFG,
		.addr = ST_ASM330LHHX_FSM_INT2_A_ADDR,
		.mask = GENMASK(7, 0),
	},
	[ST_ASM330LHHX_REG_FSM_INT2_B_REG] = {
		.page = FUNC_CFG_ACCESS_FUNC_CFG,
		.addr = ST_ASM330LHHX_FSM_INT2_B_ADDR,
		.mask = GENMASK(7, 0),
	},
	[ST_ASM330LHHX_REG_MLC_INT2_REG] = {
		.page = FUNC_CFG_ACCESS_FUNC_CFG,
		.addr = ST_ASM330LHHX_MLC_INT2_ADDR,
		.mask = GENMASK(7, 0),
	},
#endif /* ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS */
};

static const struct
st_asm330lhhx_odr_table_entry st_asm330lhhx_odr_table[] = {
	[ST_ASM330LHHX_ID_ACC] = {
		.size = 7,
		.reg = {
			.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
			.mask = ST_ASM330LHHX_CTRL1_XL_ODR_XL_MASK,
		},
		.pm = {
			.addr = ST_ASM330LHHX_REG_CTRL6_C_ADDR,
			.mask = ST_ASM330LHHX_REG_XL_HM_MODE_MASK,
		},
		.batching_reg = {
			.addr = ST_ASM330LHHX_REG_FIFO_CTRL3_ADDR,
			.mask = ST_ASM330LHHX_REG_BDR_XL_MASK,
		},
		//.odr_avl[0] = {   1, 600000,  0x01,  0x0b },
		.odr_avl[0] = {  12, 500000,  0x01,  0x01 },
		.odr_avl[1] = {  26,      0,  0x02,  0x02 },
		.odr_avl[2] = {  52,      0,  0x03,  0x03 },
		.odr_avl[3] = { 104,      0,  0x04,  0x04 },
		.odr_avl[4] = { 208,      0,  0x05,  0x05 },
		.odr_avl[5] = { 416,      0,  0x06,  0x06 },
		.odr_avl[6] = { 833,      0,  0x07,  0x07 },
	},
	[ST_ASM330LHHX_ID_GYRO] = {
		.size = 7,
		.reg = {
			.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
			.mask = ST_ASM330LHHX_CTRL2_G_ODR_G_MASK,
		},
		.pm = {
			.addr = ST_ASM330LHHX_REG_CTRL7_G_ADDR,
			.mask = ST_ASM330LHHX_REG_G_HM_MODE_MASK,
		},
		.batching_reg = {
			.addr = ST_ASM330LHHX_REG_FIFO_CTRL3_ADDR,
			.mask = ST_ASM330LHHX_REG_BDR_GY_MASK,
		},
		//.odr_avl[0] = {   6, 500000,  0x01,  0x0b },
		.odr_avl[0] = {  12, 500000,  0x01,  0x01 },
		.odr_avl[1] = {  26,      0,  0x02,  0x02 },
		.odr_avl[2] = {  52,      0,  0x03,  0x03 },
		.odr_avl[3] = { 104,      0,  0x04,  0x04 },
		.odr_avl[4] = { 208,      0,  0x05,  0x05 },
		.odr_avl[5] = { 416,      0,  0x06,  0x06 },
		.odr_avl[6] = { 833,      0,  0x07,  0x07 },
	},
	[ST_ASM330LHHX_ID_TEMP] = {
		.size = 2,
		.batching_reg = {
			.addr = ST_ASM330LHHX_REG_FIFO_CTRL4_ADDR,
			.mask = ST_ASM330LHHX_REG_ODR_T_BATCH_MASK,
		},
		//.odr_avl[0] = {  1, 600000,   0x01,  0x01 },
		.odr_avl[0] = { 12, 500000,   0x02,  0x02 },
		.odr_avl[1] = { 52,      0,   0x03,  0x03 },
	},
};

static const struct
st_asm330lhhx_fs_table_entry st_asm330lhhx_fs_table[] = {
	[ST_ASM330LHHX_ID_ACC] = {
		.size = 4,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
				.mask = ST_ASM330LHHX_CTRL1_XL_FS_XL_MASK,
			},
			.gain = IIO_G_TO_M_S_2(61),
			.val = 0x0,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
				.mask = ST_ASM330LHHX_CTRL1_XL_FS_XL_MASK,
			},
			.gain = IIO_G_TO_M_S_2(122),
			.val = 0x2,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
				.mask = ST_ASM330LHHX_CTRL1_XL_FS_XL_MASK,
			},
			.gain = IIO_G_TO_M_S_2(244),
			.val = 0x3,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL1_XL_ADDR,
				.mask = ST_ASM330LHHX_CTRL1_XL_FS_XL_MASK,
			},
			.gain = IIO_G_TO_M_S_2(488),
			.val = 0x1,
		},
	},
	[ST_ASM330LHHX_ID_GYRO] = {
		.size = 6,
		.fs_avl[0] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = ST_ASM330LHHX_CTRL2_G_FS_G_MASK,
			},
			.gain = IIO_DEGREE_TO_RAD(4370),
			.val = 0x2,
		},
		.fs_avl[1] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = ST_ASM330LHHX_CTRL2_G_FS_G_MASK,
			},
			.gain = IIO_DEGREE_TO_RAD(8750),
			.val = 0x0,
		},
		.fs_avl[2] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = ST_ASM330LHHX_CTRL2_G_FS_G_MASK,
			},
			.gain = IIO_DEGREE_TO_RAD(17500),
			.val = 0x4,
		},
		.fs_avl[3] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = ST_ASM330LHHX_CTRL2_G_FS_G_MASK,
			},
			.gain = IIO_DEGREE_TO_RAD(35000),
			.val = 0x8,
		},
		.fs_avl[4] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = ST_ASM330LHHX_CTRL2_G_FS_G_MASK,
			},
			.gain = IIO_DEGREE_TO_RAD(70000),
			.val = 0x0C,
		},
		.fs_avl[5] = {
			.reg = {
				.addr = ST_ASM330LHHX_CTRL2_G_ADDR,
				.mask = ST_ASM330LHHX_CTRL2_G_FS_G_MASK,
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

static int __maybe_unused dump_registers(const char *info,
					 struct st_asm330lhhx_hw *hw)
{
	unsigned int data;
	int i, err = 0;

	mutex_lock(&hw->page_lock);

	for (i = ST_ASM330LHHX_REG_FUNC_CFG_ACCESS_ADDR;
	     i <= ST_ASM330LHHX_REG_FIFO_STATUS2_ADDR; i++) {
		switch (i) {
		case ST_ASM330LHHX_REG_FUNC_CFG_ACCESS_ADDR:
		case ST_ASM330LHHX_REG_FIFO_CTRL1_ADDR:
		case ST_ASM330LHHX_REG_FIFO_CTRL2_ADDR:
		case ST_ASM330LHHX_REG_FIFO_CTRL3_ADDR:
		case ST_ASM330LHHX_REG_FIFO_CTRL4_ADDR:
		case ST_ASM330LHHX_REG_INT1_CTRL_ADDR:
		case ST_ASM330LHHX_REG_INT2_CTRL_ADDR:
		case ST_ASM330LHHX_CTRL1_XL_ADDR:
		case ST_ASM330LHHX_CTRL2_G_ADDR:
		case ST_ASM330LHHX_REG_CTRL3_C_ADDR:
		case ST_ASM330LHHX_REG_CTRL4_C_ADDR:
		case ST_ASM330LHHX_REG_CTRL5_C_ADDR:
		case ST_ASM330LHHX_REG_CTRL6_C_ADDR:
		case ST_ASM330LHHX_REG_CTRL7_G_ADDR:
		case ST_ASM330LHHX_REG_CTRL10_C_ADDR:
		case ST_ASM330LHHX_REG_STATUS_ADDR:
		case ST_ASM330LHHX_FSM_STATUS_A_MAINPAGE:
		case ST_ASM330LHHX_FSM_STATUS_B_MAINPAGE:
		case ST_ASM330LHHX_MLC_STATUS_MAINPAGE:
		case ST_ASM330LHHX_REG_FIFO_STATUS1_ADDR:
		case ST_ASM330LHHX_REG_FIFO_STATUS2_ADDR:
			err = regmap_read(hw->regmap, i, &data);
			if (err < 0) {
				dev_err(hw->dev,
					"failed to read register %02x\n", i);
				goto out_lock;
			}

			dev_dbg(hw->dev, "%s: %02x: %02x\n",
				info, i, data);
			break;
		default:
			break;
		}
	}

	err = st_asm330lhhx_set_page_access(hw, true,
					    ST_ASM330LHHX_REG_FUNC_CFG_MASK);
	if (err < 0)
		goto out_lock;

	for (i = ST_ASM330LHHX_PAGE_SEL_ADDR;
	     i <= ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR; i++) {
		switch (i) {
		case ST_ASM330LHHX_PAGE_SEL_ADDR:
		case ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR:
		case ST_ASM330LHHX_FSM_INT1_A_ADDR:
		case ST_ASM330LHHX_FSM_INT1_B_ADDR:
		case ST_ASM330LHHX_MLC_INT1_ADDR:
		case ST_ASM330LHHX_FSM_INT2_A_ADDR:
		case ST_ASM330LHHX_FSM_INT2_B_ADDR:
		case ST_ASM330LHHX_MLC_INT2_ADDR:
		case ST_ASM330LHHX_REG_MLC_STATUS_ADDR:
		case ST_ASM330LHHX_REG_PAGE_RW:
		case ST_ASM330LHHX_FSM_ENABLE_A_ADDR:
		case ST_ASM330LHHX_FSM_ENABLE_B_ADDR:
		case ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR:
			err = regmap_read(hw->regmap, i, &data);
			if (err < 0) {
				dev_err(hw->dev,
					"failed to read register %02x\n", i);
				goto out_lock;
			}

			dev_dbg(hw->dev, "%s: %02x: %02x\n",
				info, i, data);
			break;
		default:
			break;
		}
	}

out_lock:
	st_asm330lhhx_set_page_access(hw, false,
				      ST_ASM330LHHX_REG_FUNC_CFG_MASK);

	mutex_unlock(&hw->page_lock);

	return err;
}

static __maybe_unused int
st_asm330lhhx_reg_access(struct iio_dev *iio_dev,
			 unsigned int reg, unsigned int writeval,
			 unsigned int *readval)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);
	int ret;

	ret = iio_device_claim_direct_mode(iio_dev);
	if (ret)
		return ret;

	if (readval == NULL)
		ret = regmap_write(sensor->hw->regmap, reg, writeval);
	else
		ret = regmap_read(sensor->hw->regmap, reg, readval);

	iio_device_release_direct_mode(iio_dev);

	return (ret < 0) ? ret : 0;
}

static int st_asm330lhhx_set_page_0(struct st_asm330lhhx_hw *hw)
{
	return regmap_write(hw->regmap,
			    ST_ASM330LHHX_REG_FUNC_CFG_ACCESS_ADDR,
			    0);
}

static int st_asm330lhhx_check_whoami(struct st_asm330lhhx_hw *hw)
{
	int err;
	int data;

	err = regmap_read(hw->regmap, ST_ASM330LHHX_REG_WHOAMI_ADDR,
			  &data);
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

static int
st_asm330lhhx_get_odr_calibration(struct st_asm330lhhx_hw *hw)
{
	int err;
	int data;
	s64 odr_calib;

	err = regmap_read(hw->regmap,
			  ST_ASM330LHHX_INTERNAL_FREQ_FINE, &data);
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

static int
st_asm330lhhx_set_full_scale(struct st_asm330lhhx_sensor *sensor,
			     u32 gain)
{
	enum st_asm330lhhx_sensor_id id = sensor->id;
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int i, err;

	for (i = 0; i < st_asm330lhhx_fs_table[id].size; i++)
		if (st_asm330lhhx_fs_table[id].fs_avl[i].gain == gain)
			break;

	if (i == st_asm330lhhx_fs_table[id].size)
		return -EINVAL;

	err = st_asm330lhhx_update_bits_locked(hw,
			st_asm330lhhx_fs_table[id].fs_avl[i].reg.addr,
			st_asm330lhhx_fs_table[id].fs_avl[i].reg.mask,
			st_asm330lhhx_fs_table[id].fs_avl[i].val);
	if (err < 0)
		return err;

	sensor->gain = gain;

	return 0;
}

int st_asm330lhhx_get_odr_val(enum st_asm330lhhx_sensor_id id, int odr,
			      int uodr, int *podr, int *puodr, u8 *val)
{
	int all_odr = ST_ASM330LHHX_ODR_EXPAND(odr, uodr);
	int sensor_odr;
	int i;

	if (all_odr == 0) {
		*val = 0;
		*podr = 0;
		*puodr = 0;

		return 0;
	}

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

int __maybe_unused
st_asm330lhhx_get_odr_from_reg(enum st_asm330lhhx_sensor_id id,
			       u8 reg_val, u16 *podr, u32 *puodr)
{
	int i;

	for (i = 0; i < st_asm330lhhx_odr_table[id].size; i++) {
		if (reg_val == st_asm330lhhx_odr_table[id].odr_avl[i].val)
			break;
	}

	if (i == st_asm330lhhx_odr_table[id].size)
		return -EINVAL;

	*podr = st_asm330lhhx_odr_table[id].odr_avl[i].hz;
	*puodr = st_asm330lhhx_odr_table[id].odr_avl[i].uhz;

	return 0;
}

int st_asm330lhhx_get_batch_val(struct st_asm330lhhx_sensor *sensor,
			        int odr, int uodr, u8 *val)
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

static u16
st_asm330lhhx_check_odr_dependency(struct st_asm330lhhx_hw *hw,
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

static int st_asm330lhhx_update_odr_fsm(struct st_asm330lhhx_hw *hw,
					enum st_asm330lhhx_sensor_id id,
					enum st_asm330lhhx_sensor_id id_req,
					int val, int delay)
{
	int ret = 0;
	int fsm_running = st_asm330lhhx_fsm_running(hw);
	int mlc_running = st_asm330lhhx_mlc_running(hw);
	int status;

	if (fsm_running || mlc_running ||
	    (id_req > ST_ASM330LHHX_ID_FIFO_MLC)) {
		/*
		 * In STMC_PAGE:
		 * Addr 0x02 bit 1 set to 1 -- CLK Disable
		 * Addr 0x05 bit 0 set to 0 -- FSM_EN=0
		 * Addr 0x05 bit 4 set to 0 -- MLC_EN=0
		 * Addr 0x67 bit 0 set to 0 -- FSM_INIT=0
		 * Addr 0x67 bit 4 set to 0 -- MLC_INIT=0
		 * Addr 0x02 bit 1 set to 0 -- CLK Disable
		 * - ODR change
		 * - Wait (~3 ODRs)
		 * In STMC_PAGE:
		 * Addr 0x05 bit 0 set to 1 -- FSM_EN = 1
		 * Addr 0x05 bit 4 set to 1 -- MLC_EN = 1
		 */
		mutex_lock(&hw->page_lock);
		ret = st_asm330lhhx_set_page_access(hw, true,
				       ST_ASM330LHHX_REG_FUNC_CFG_MASK);
		if (ret < 0)
			goto unlock_page;

		ret = regmap_read(hw->regmap,
				  ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
				  &status);
		if (ret < 0)
			goto unlock_page;

		ret = regmap_update_bits(hw->regmap,
					 ST_ASM330LHHX_PAGE_SEL_ADDR,
					 BIT(1), FIELD_PREP(BIT(1), 1));
		if (ret < 0)
			goto unlock_page;

		ret = regmap_update_bits(hw->regmap,
			ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
			ST_ASM330LHHX_FSM_EN_MASK,
			FIELD_PREP(ST_ASM330LHHX_FSM_EN_MASK, 0));
		if (ret < 0)
			goto unlock_page;

		if (st_asm330lhhx_mlc_running(hw)) {
			ret = regmap_update_bits(hw->regmap,
				ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
				ST_ASM330LHHX_MLC_EN_MASK,
				FIELD_PREP(ST_ASM330LHHX_MLC_EN_MASK, 0));
			if (ret < 0)
				goto unlock_page;
		}

		ret = regmap_update_bits(hw->regmap,
			ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR,
			ST_ASM330LHHX_MLC_INIT_MASK,
			FIELD_PREP(ST_ASM330LHHX_MLC_INIT_MASK, 0));
		if (ret < 0)
			goto unlock_page;

		ret = regmap_update_bits(hw->regmap,
			ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR,
			ST_ASM330LHHX_FSM_INIT_MASK,
			FIELD_PREP(ST_ASM330LHHX_FSM_INIT_MASK, 0));
		if (ret < 0)
			goto unlock_page;

		ret = regmap_update_bits(hw->regmap,
					 ST_ASM330LHHX_PAGE_SEL_ADDR,
					 BIT(1), FIELD_PREP(BIT(1), 0));
		if (ret < 0)
			goto unlock_page;

		ret = st_asm330lhhx_set_page_access(hw, false,
				      ST_ASM330LHHX_REG_FUNC_CFG_MASK);
		if (ret < 0)
			goto unlock_page;

		ret = regmap_update_bits(hw->regmap,
			st_asm330lhhx_odr_table[id].reg.addr,
			st_asm330lhhx_odr_table[id].reg.mask,
			ST_ASM330LHHX_SHIFT_VAL(val,
				st_asm330lhhx_odr_table[id].reg.mask));
		if (ret < 0)
			goto unlock_page;

		usleep_range(delay, delay + (delay / 10));

		st_asm330lhhx_set_page_access(hw, true,
				       ST_ASM330LHHX_REG_FUNC_CFG_MASK);

		ret = regmap_write(hw->regmap,
				   ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
				   status);
unlock_page:
		st_asm330lhhx_set_page_access(hw, false,
				       ST_ASM330LHHX_REG_FUNC_CFG_MASK);
		mutex_unlock(&hw->page_lock);
	} else {
		ret = st_asm330lhhx_update_bits_locked(hw,
				st_asm330lhhx_odr_table[id].reg.addr,
				st_asm330lhhx_odr_table[id].reg.mask,
				val);
	}

	return ret;
}

static int st_asm330lhhx_set_odr(struct st_asm330lhhx_sensor *sensor,
				 int req_odr, int req_uodr)
{
	enum st_asm330lhhx_sensor_id id_req = sensor->id;
	enum st_asm330lhhx_sensor_id id = sensor->id;
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int err, delay;
	u8 val = 0;

	switch (id) {
#ifdef CONFIG_IIO_ST_ASM330LHHX_MLC
	case ST_ASM330LHHX_ID_MLC_0:
	case ST_ASM330LHHX_ID_MLC_1:
	case ST_ASM330LHHX_ID_MLC_2:
	case ST_ASM330LHHX_ID_MLC_3:
	case ST_ASM330LHHX_ID_MLC_4:
	case ST_ASM330LHHX_ID_MLC_5:
	case ST_ASM330LHHX_ID_MLC_6:
	case ST_ASM330LHHX_ID_MLC_7:
	case ST_ASM330LHHX_ID_FSM_0:
	case ST_ASM330LHHX_ID_FSM_1:
	case ST_ASM330LHHX_ID_FSM_2:
	case ST_ASM330LHHX_ID_FSM_3:
	case ST_ASM330LHHX_ID_FSM_4:
	case ST_ASM330LHHX_ID_FSM_5:
	case ST_ASM330LHHX_ID_FSM_6:
	case ST_ASM330LHHX_ID_FSM_7:
	case ST_ASM330LHHX_ID_FSM_8:
	case ST_ASM330LHHX_ID_FSM_9:
	case ST_ASM330LHHX_ID_FSM_10:
	case ST_ASM330LHHX_ID_FSM_11:
	case ST_ASM330LHHX_ID_FSM_12:
	case ST_ASM330LHHX_ID_FSM_13:
	case ST_ASM330LHHX_ID_FSM_14:
	case ST_ASM330LHHX_ID_FSM_15:
#endif /* CONFIG_IIO_ST_ASM330LHHX_MLC */
	case ST_ASM330LHHX_ID_EXT0:
	case ST_ASM330LHHX_ID_EXT1:
	case ST_ASM330LHHX_ID_TEMP:
	case ST_ASM330LHHX_ID_ACC: {
		int odr;
		int i;

		id = ST_ASM330LHHX_ID_ACC;
		for (i = ST_ASM330LHHX_ID_ACC;
		     i <= ST_ASM330LHHX_ID_MAX; i++) {
			if (!hw->iio_devs[i] || i == sensor->id)
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
	case ST_ASM330LHHX_ID_GYRO:
		break;
	default:
		return 0;
	}

	err = st_asm330lhhx_get_odr_val(id, req_odr, req_uodr, &req_odr,
				        &req_uodr, &val);
	if (err < 0)
		return err;

	/* check if sensor supports power mode setting */
	if (sensor->pm != ST_ASM330LHHX_NO_MODE) {
		err = st_asm330lhhx_update_bits_locked(hw,
				st_asm330lhhx_odr_table[id].pm.addr,
				st_asm330lhhx_odr_table[id].pm.mask,
				sensor->pm);
		if (err < 0)
			return err;
	}

	delay = 4000000 / req_odr;

	return st_asm330lhhx_update_odr_fsm(hw, id, id_req, val, delay);
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

static int
st_asm330lhhx_read_oneshot(struct st_asm330lhhx_sensor *sensor,
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
						        &data,
							sizeof(data));
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

		/*
		 * use at least three time delay for data valid because
		 * when sensor enabled need 3 samples to be stable
		 */
		delay = 3000000 / sensor->odr;
		usleep_range(delay, delay + (delay / 10));

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

		ret = st_asm330lhhx_read_oneshot(sensor, ch->address,
						 val);
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

		err = st_asm330lhhx_get_odr_val(sensor->id, val, val2,
					       &todr, &tuodr, &data);
		if (!err) {
			sensor->odr = val;
			sensor->uodr = tuodr;

			/*
			 * VTS test testSamplingRateHotSwitchOperation
			 * not toggle the enable status of sensor after
			 * changing the ODR -> force it
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

static ssize_t
st_asm330lhhx_sysfs_scale_avail(struct device *dev,
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

static ssize_t
st_asm330lhhx_get_power_mode(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);

	return sprintf(buf, "%d\n", sensor->pm);
}

static ssize_t
st_asm330lhhx_set_power_mode(struct device *dev,
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

	if ((val >= ST_ASM330LHHX_HP_MODE) &&
	    (val < ST_ASM330LHHX_NO_MODE))
		sensor->pm = val;
	else
		err = -EINVAL;

out:
	iio_device_release_direct_mode(iio_dev);

	return err < 0 ? err : size;
}

int st_asm330lhhx_of_get_pin(struct st_asm330lhhx_hw *hw, int *pin)
{
	struct device_node *np = hw->dev->of_node;

	if (!np)
		return -EINVAL;

	return of_property_read_u32(np, "st,int-pin", pin);
}

static int _st_asm330lhhx_get_int_reg(struct st_asm330lhhx_hw *hw,
				      int int_pin, u8 *drdy_reg)
{
	int err = 0;

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

static int st_asm330lhhx_get_int_reg(struct st_asm330lhhx_hw *hw,
				     u8 *drdy_reg)
{
	int err = 0, int_pin;

	if (st_asm330lhhx_of_get_pin(hw, &int_pin) < 0) {
		struct st_sensors_platform_data *pdata;
		struct device *dev = hw->dev;

		pdata = (struct st_sensors_platform_data *)dev->platform_data;
		int_pin = pdata ? pdata->drdy_int_pin : 1;
	}

	err = _st_asm330lhhx_get_int_reg(hw, int_pin, drdy_reg);
	if (err)
		return err;

	hw->int_pin = int_pin;

	return err;
}

static int
__maybe_unused st_asm330lhhx_bk_regs(struct st_asm330lhhx_hw *hw)
{
	unsigned int data;
#ifdef ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS
	bool restore = 0;
#endif /* ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS */
	int i, err = 0;

	mutex_lock(&hw->page_lock);

	for (i = 0; i < ST_ASM330LHHX_SUSPEND_RESUME_REGS; i++) {
#ifdef ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS
		if (st_asm330lhhx_suspend_resume[i].page != FUNC_CFG_ACCESS_0) {
			err = regmap_update_bits(hw->regmap,
				ST_ASM330LHHX_REG_FUNC_CFG_ACCESS_ADDR,
				ST_ASM330LHHX_REG_ACCESS_MASK,
				FIELD_PREP(ST_ASM330LHHX_REG_ACCESS_MASK,
				 st_asm330lhhx_suspend_resume[i].page));
			if (err < 0) {
				dev_err(hw->dev,
					"failed to update %02x reg\n",
					st_asm330lhhx_suspend_resume[i].addr);
				break;
			}

			restore = 1;
		}
#endif /* ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS */

		err = regmap_read(hw->regmap,
				  st_asm330lhhx_suspend_resume[i].addr,
				  &data);
		if (err < 0) {
			dev_err(hw->dev,
				"failed to read register %02x\n",
				st_asm330lhhx_suspend_resume[i].addr);
			goto out_lock;
		}

#ifdef ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS
		if (restore) {
			err = regmap_update_bits(hw->regmap,
				ST_ASM330LHHX_REG_FUNC_CFG_ACCESS_ADDR,
				ST_ASM330LHHX_REG_ACCESS_MASK,
				FUNC_CFG_ACCESS_0);
			if (err < 0) {
				dev_err(hw->dev,
					"failed to update %02x reg\n",
					st_asm330lhhx_suspend_resume[i].addr);
				break;
			}

			restore = 0;
		}
#endif /* ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS */

		st_asm330lhhx_suspend_resume[i].val = data;

#ifdef ST_ASM330LHHX_ENABLE_DEBUG
		dev_info(hw->dev,
			 "%s %d: %x -> %x\n",
			 __FUNCTION__, __LINE__,
			 st_asm330lhhx_suspend_resume[i].addr,
			 st_asm330lhhx_suspend_resume[i].val);
#endif /* ST_ASM330LHHX_ENABLE_DEBUG */
	}

out_lock:
	mutex_unlock(&hw->page_lock);

	return err;
}

static int
__maybe_unused st_asm330lhhx_restore_regs(struct st_asm330lhhx_hw *hw)
{
#ifdef ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS
	bool restore = 0;
#endif /* ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS */
	int i, err = 0;

	mutex_lock(&hw->page_lock);

	for (i = 0; i < ST_ASM330LHHX_SUSPEND_RESUME_REGS; i++) {
#ifdef ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS
		if (st_asm330lhhx_suspend_resume[i].page != FUNC_CFG_ACCESS_0) {
			err = regmap_update_bits(hw->regmap,
				ST_ASM330LHHX_REG_FUNC_CFG_ACCESS_ADDR,
				ST_ASM330LHHX_REG_ACCESS_MASK,
				FIELD_PREP(ST_ASM330LHHX_REG_ACCESS_MASK,
				 st_asm330lhhx_suspend_resume[i].page));
			if (err < 0) {
				dev_err(hw->dev,
					"failed to backup %02x reg\n",
					st_asm330lhhx_suspend_resume[i].addr);
				break;
			}

			restore = 1;
		}
#endif /* ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS */
		err = regmap_update_bits(hw->regmap,
				st_asm330lhhx_suspend_resume[i].addr,
				st_asm330lhhx_suspend_resume[i].mask,
				st_asm330lhhx_suspend_resume[i].val);
		if (err < 0) {
			dev_err(hw->dev,
				"failed to update %02x reg\n",
				st_asm330lhhx_suspend_resume[i].addr);
			break;
		}

#ifdef ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS
		if (restore) {
			err = regmap_update_bits(hw->regmap,
				ST_ASM330LHHX_REG_FUNC_CFG_ACCESS_ADDR,
				ST_ASM330LHHX_REG_ACCESS_MASK,
				FUNC_CFG_ACCESS_0);
			if (err < 0) {
				dev_err(hw->dev,
					"failed to update %02x reg\n",
					st_asm330lhhx_suspend_resume[i].addr);
				break;
			}

			restore = 0;
		}
#endif /* ST_ASM330LHHX_BACKUP_FUNC_CFG_REGS */

#ifdef ST_ASM330LHHX_ENABLE_DEBUG
		dev_info(hw->dev,
			 "%s %d: %x <- %x\n",
			 __FUNCTION__, __LINE__,
			 st_asm330lhhx_suspend_resume[i].addr,
			 st_asm330lhhx_suspend_resume[i].val);
#endif /* ST_ASM330LHHX_ENABLE_DEBUG */

	}

	mutex_unlock(&hw->page_lock);

	return err;
}

static int
st_asm330lhhx_set_selftest(struct st_asm330lhhx_sensor *sensor,
			   int index)
{
	u8 mode, mask;

	switch (sensor->id) {
	case ST_ASM330LHHX_ID_ACC:
		mask = ST_ASM330LHHX_REG_ST_XL_MASK;
		mode = st_asm330lhhx_selftest_table[index].accel_value;
		break;
	case ST_ASM330LHHX_ID_GYRO:
		mask = ST_ASM330LHHX_REG_ST_G_MASK;
		mode = st_asm330lhhx_selftest_table[index].gyro_value;
		break;
	default:
		return -EINVAL;
	}

	return st_asm330lhhx_update_bits_locked(sensor->hw,
					ST_ASM330LHHX_REG_CTRL5_C_ADDR,
					mask, mode);
}

static ssize_t
st_asm330lhhx_sysfs_get_selftest_available(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s, %s\n",
		       st_asm330lhhx_selftest_table[1].string_mode,
		       st_asm330lhhx_selftest_table[2].string_mode);
}

static ssize_t
st_asm330lhhx_sysfs_get_selftest_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	enum st_asm330lhhx_sensor_id id = sensor->id;
	int8_t result;
	char *message;

	if (id != ST_ASM330LHHX_ID_ACC &&
	    id != ST_ASM330LHHX_ID_GYRO)
		return -EINVAL;

	result = sensor->selftest_status;
	if (result == 0)
		message = "na";
	else if (result < 0)
		message = "fail";
	else if (result > 0)
		message = "pass";

	return sprintf(buf, "%s\n", message);
}

static int
st_asm330lhhx_selftest_sensor(struct st_asm330lhhx_sensor *sensor,
			      int test)
{
	int x_selftest = 0, y_selftest = 0, z_selftest = 0;
	int x = 0, y = 0, z = 0, try_count = 0;
	u8 i, status, n = 0;
	u8 reg, bitmask;
	int ret, delay;
	u8 raw_data[6];

	switch (sensor->id) {
	case ST_ASM330LHHX_ID_ACC:
		reg = ST_ASM330LHHX_REG_OUTX_L_A_ADDR;
		bitmask = ST_ASM330LHHX_REG_STATUS_XLDA;
		break;
	case ST_ASM330LHHX_ID_GYRO:
		reg = ST_ASM330LHHX_REG_OUTX_L_G_ADDR;
		bitmask = ST_ASM330LHHX_REG_STATUS_GDA;
		break;
	default:
		return -EINVAL;
	}

	/* set selftest normal mode */
	ret = st_asm330lhhx_set_selftest(sensor, 0);
	if (ret < 0)
		return ret;

	ret = st_asm330lhhx_sensor_set_enable(sensor, true);
	if (ret < 0)
		return ret;

	/*
	 * wait at least onr ODRs plus 10 % to be sure to fetch new
	 * sample data
	 */
	delay = 1000000 / sensor->odr;

	/* power up, wait 100 ms for stable output */
	msleep(100);

	/*
	 * for 5 times, after checking status bit, read the output
	 * registers
	 */
	for (i = 0; i < 5; i++) {
		try_count = 0;
		while (try_count < 3) {
			usleep_range(delay, delay + delay/10);
			ret = st_asm330lhhx_read_locked(sensor->hw,
					ST_ASM330LHHX_REG_STATUS_ADDR,
					&status, sizeof(status));
			if (ret < 0)
				goto selftest_failure;

			if (status & bitmask) {
				ret = st_asm330lhhx_read_locked(sensor->hw, reg,
						raw_data, sizeof(raw_data));
				if (ret < 0)
					goto selftest_failure;

				/*
				 * for 5 times, after checking status
				 * bit, read the output registers
				 */
				x += ((s16)*(u16 *)&raw_data[0]) / 5;
				y += ((s16)*(u16 *)&raw_data[2]) / 5;
				z += ((s16)*(u16 *)&raw_data[4]) / 5;
				n++;

				break;
			} else {
				try_count++;
			}
		}
	}

	if (i != n) {
		dev_err(sensor->hw->dev,
			"some acc samples missing (expected %d, read %d)\n",
			i, n);
		ret = -1;

		goto selftest_failure;
	}

	n = 0;

	/* set selftest mode */
	st_asm330lhhx_set_selftest(sensor, test);

	/* wait 100 ms for stable output */
	msleep(100);

	/*
	 * for 5 times, after checking status bit,
	 * read the output registers
	 */
	for (i = 0; i < 5; i++) {
		try_count = 0;
		while (try_count < 3) {
			usleep_range(delay, delay + delay/10);
			ret = st_asm330lhhx_read_locked(sensor->hw,
					ST_ASM330LHHX_REG_STATUS_ADDR,
					&status, sizeof(status));
			if (ret < 0)
				goto selftest_failure;

			if (status & bitmask) {
				ret = st_asm330lhhx_read_locked(sensor->hw, reg,
						raw_data, sizeof(raw_data));
				if (ret < 0)
					goto selftest_failure;

				x_selftest += ((s16)*(u16 *)&raw_data[0]) / 5;
				y_selftest += ((s16)*(u16 *)&raw_data[2]) / 5;
				z_selftest += ((s16)*(u16 *)&raw_data[4]) / 5;
				n++;

				break;
			} else {
				try_count++;
			}
		}
	}

	if (i != n) {
		dev_err(sensor->hw->dev,
			"some samples missing (expected %d, read %d)\n",
			i, n);
		ret = -1;

		goto selftest_failure;
	}

	if ((abs(x_selftest - x) < sensor->min_st) ||
	    (abs(x_selftest - x) > sensor->max_st)) {
		sensor->selftest_status = -1;
		goto selftest_failure;
	}

	if ((abs(y_selftest - y) < sensor->min_st) ||
	    (abs(y_selftest - y) > sensor->max_st)) {
		sensor->selftest_status = -1;
		goto selftest_failure;
	}

	if ((abs(z_selftest - z) < sensor->min_st) ||
	    (abs(z_selftest - z) > sensor->max_st)) {
		sensor->selftest_status = -1;
		goto selftest_failure;
	}

	sensor->selftest_status = 1;

selftest_failure:
	/* restore selftest to normal mode */
	st_asm330lhhx_set_selftest(sensor, 0);

	return st_asm330lhhx_sensor_set_enable(sensor, false);
}

static ssize_t st_asm330lhhx_sysfs_start_selftest(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);
	enum st_asm330lhhx_sensor_id id = sensor->id;
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret, test, odr, uodr;
	u8 drdy_reg;
	u32 gain;

	if (id != ST_ASM330LHHX_ID_ACC &&
	    id != ST_ASM330LHHX_ID_GYRO)
		return -EINVAL;

	for (test = 0; test < ARRAY_SIZE(st_asm330lhhx_selftest_table); test++) {
		if (strncmp(buf, st_asm330lhhx_selftest_table[test].string_mode,
			    strlen(st_asm330lhhx_selftest_table[test].string_mode)) == 0)
			break;
	}

	if (test == ARRAY_SIZE(st_asm330lhhx_selftest_table))
		return -EINVAL;

	ret = iio_device_claim_direct_mode(hw->iio_devs[ST_ASM330LHHX_ID_ACC]);
	if (ret)
		return ret;

	ret = iio_device_claim_direct_mode(hw->iio_devs[ST_ASM330LHHX_ID_GYRO]);
	if (ret)
		goto release_acc;

	/* self test mode unavailable if sensor enabled */
	if (hw->enable_mask & BIT(id)) {
		ret = -EBUSY;

		goto out_claim;
	}

	st_asm330lhhx_bk_regs(hw);

	/* disable FIFO watermak interrupt */
	ret = st_asm330lhhx_get_int_reg(hw, &drdy_reg);
	if (ret < 0)
		goto restore_regs;

	ret = st_asm330lhhx_write_with_mask_locked(hw, drdy_reg,
				ST_ASM330LHHX_REG_FIFO_TH_MASK, 0);
	if (ret < 0)
		goto restore_regs;

	gain = sensor->gain;
	odr = sensor->odr;
	uodr = sensor->uodr;

	if (id == ST_ASM330LHHX_ID_ACC) {
		/* set BDU = 1, FS = 4 g, ODR = 52 Hz */
		st_asm330lhhx_set_full_scale(sensor, IIO_G_TO_M_S_2(61));
		st_asm330lhhx_set_odr(sensor, 52, 0);
		sensor->odr = 52;
		sensor->uodr = 0;
		st_asm330lhhx_selftest_sensor(sensor, test);

		/* restore full scale after test */
		st_asm330lhhx_set_full_scale(sensor, gain);
	} else {
		/* set BDU = 1, ODR = 208 Hz, FS = 2000 dps */
		st_asm330lhhx_set_full_scale(sensor,
					     IIO_DEGREE_TO_RAD(70000));
		st_asm330lhhx_set_odr(sensor, 208, 0);
		sensor->odr = 208;
		sensor->uodr = 0;
		st_asm330lhhx_selftest_sensor(sensor, test);

		/* restore full scale after test */
		st_asm330lhhx_set_full_scale(sensor, gain);
	}

	sensor->odr = odr;
	sensor->uodr = uodr;

restore_regs:
	st_asm330lhhx_restore_regs(hw);

out_claim:
	iio_device_release_direct_mode(hw->iio_devs[ST_ASM330LHHX_ID_GYRO]);

release_acc:
	iio_device_release_direct_mode(hw->iio_devs[ST_ASM330LHHX_ID_ACC]);

	return size;
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
static IIO_DEVICE_ATTR(selftest_available, S_IRUGO,
		       st_asm330lhhx_sysfs_get_selftest_available,
		       NULL, 0);
static IIO_DEVICE_ATTR(selftest, S_IWUSR | S_IRUGO,
		       st_asm330lhhx_sysfs_get_selftest_status,
		       st_asm330lhhx_sysfs_start_selftest, 0);

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
	&iio_dev_attr_selftest_available.dev_attr.attr,
	&iio_dev_attr_selftest.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhhx_acc_attribute_group = {
	.attrs = st_asm330lhhx_acc_attributes,
};

static const struct iio_info st_asm330lhhx_acc_info = {
	.attrs = &st_asm330lhhx_acc_attribute_group,
	.read_raw = st_asm330lhhx_read_raw,
	.write_raw = st_asm330lhhx_write_raw,
#ifdef CONFIG_DEBUG_FS
	.debugfs_reg_access = &st_asm330lhhx_reg_access,
#endif /* CONFIG_DEBUG_FS */
};

static struct attribute *st_asm330lhhx_gyro_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	&iio_dev_attr_power_mode.dev_attr.attr,
	&iio_dev_attr_selftest_available.dev_attr.attr,
	&iio_dev_attr_selftest.dev_attr.attr,
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

static const unsigned long st_asm330lhhx_available_scan_masks[] = {
	0x7, 0x0
};
static const unsigned long st_asm330lhhx_temp_available_scan_masks[] = {
	0x1, 0x0
};

static int st_asm330lhhx_reset_device(struct st_asm330lhhx_hw *hw)
{
	int err;

	/* sw reset */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL3_C_ADDR,
				 ST_ASM330LHHX_REG_SW_RESET_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_SW_RESET_MASK, 1));
	if (err < 0)
		return err;

	usleep_range(10500, 11000);

	/* boot */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL3_C_ADDR,
				 ST_ASM330LHHX_REG_BOOT_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_BOOT_MASK, 1));

	usleep_range(20, 30);

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

	/* enable timestamp rollover interrupt on INT2 */
	err = regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_MD2_CFG_ADDR,
				 ST_ASM330LHHX_REG_INT2_TIMESTAMP_MASK,
				 ST_ASM330LHHX_SHIFT_VAL(enable,
					ST_ASM330LHHX_REG_INT2_TIMESTAMP_MASK));
	if (err < 0)
		return err;

	hw->hw_timestamp_enabled = enable;

	return 0;
}

static int st_asm330lhhx_init_device(struct st_asm330lhhx_hw *hw)
{
	u8 drdy_reg;
	int err;

	/* latch interrupts */
	err = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_REG_TAP_CFG0_ADDR,
				 ST_ASM330LHHX_REG_LIR_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_LIR_MASK, 1));
	if (err < 0)
		return err;

	/* enable Block Data Update */
	err = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_REG_CTRL3_C_ADDR,
				 ST_ASM330LHHX_REG_BDU_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_BDU_MASK, 1));
	if (err < 0)
		return err;

	err = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_REG_CTRL5_C_ADDR,
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
	err = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_REG_CTRL4_C_ADDR,
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

static struct
iio_dev *st_asm330lhhx_alloc_iiodev(struct st_asm330lhhx_hw *hw,
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
	sensor->decimator = 0;
	sensor->dec_counter = 0;

	/* Set default FS to each sensor */
	sensor->gain = st_asm330lhhx_fs_table[id].fs_avl[0].gain;

	switch (id) {
	case ST_ASM330LHHX_ID_ACC:
		iio_dev->channels = st_asm330lhhx_acc_channels;
		iio_dev->num_channels =
				ARRAY_SIZE(st_asm330lhhx_acc_channels);
		iio_dev->name = "asm330lhhx_accel";
		iio_dev->info = &st_asm330lhhx_acc_info;
		iio_dev->available_scan_masks =
				st_asm330lhhx_available_scan_masks;
		sensor->max_watermark = ST_ASM330LHHX_MAX_FIFO_DEPTH;
		sensor->offset = 0;
		sensor->pm = ST_ASM330LHHX_HP_MODE;
		sensor->odr = st_asm330lhhx_odr_table[id].odr_avl[0].hz;
		sensor->uodr =
			     st_asm330lhhx_odr_table[id].odr_avl[0].uhz;
		sensor->min_st = ST_ASM330LHHX_SELFTEST_ACCEL_MIN;
		sensor->max_st = ST_ASM330LHHX_SELFTEST_ACCEL_MAX;
		break;
	case ST_ASM330LHHX_ID_GYRO:
		iio_dev->channels = st_asm330lhhx_gyro_channels;
		iio_dev->num_channels =
				ARRAY_SIZE(st_asm330lhhx_gyro_channels);
		iio_dev->name = "asm330lhhx_gyro";
		iio_dev->info = &st_asm330lhhx_gyro_info;
		iio_dev->available_scan_masks =
				st_asm330lhhx_available_scan_masks;
		sensor->max_watermark = ST_ASM330LHHX_MAX_FIFO_DEPTH;
		sensor->offset = 0;
		sensor->pm = ST_ASM330LHHX_HP_MODE;
		sensor->odr = st_asm330lhhx_odr_table[id].odr_avl[0].hz;
		sensor->uodr =
			     st_asm330lhhx_odr_table[id].odr_avl[0].uhz;
		sensor->min_st = ST_ASM330LHHX_SELFTEST_GYRO_MIN;
		sensor->max_st = ST_ASM330LHHX_SELFTEST_GYRO_MAX;
		break;
	case ST_ASM330LHHX_ID_TEMP:
		iio_dev->channels = st_asm330lhhx_temp_channels;
		iio_dev->num_channels =
				ARRAY_SIZE(st_asm330lhhx_temp_channels);
		iio_dev->name = "asm330lhhx_temp";
		iio_dev->info = &st_asm330lhhx_temp_info;
		iio_dev->available_scan_masks =
				st_asm330lhhx_temp_available_scan_masks;
		sensor->max_watermark = ST_ASM330LHHX_MAX_FIFO_DEPTH;
		sensor->offset = ST_ASM330LHHX_TEMP_OFFSET;
		sensor->pm = ST_ASM330LHHX_NO_MODE;
		sensor->odr = st_asm330lhhx_odr_table[id].odr_avl[0].hz;
		sensor->uodr =
			     st_asm330lhhx_odr_table[id].odr_avl[0].uhz;
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
	mutex_init(&hw->handler_lock);

	hw->regmap = regmap;
	hw->dev = dev;
	hw->irq = irq;
	hw->resuming = false;
	hw->odr_table_entry = st_asm330lhhx_odr_table;

	err = st_asm330lhhx_set_page_0(hw);
	if (err < 0)
		return err;

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

	if ((!dev_fwnode(dev) ||
	     !device_property_read_bool(dev, "st,disable-sensor-hub"))) {
		err = st_asm330lhhx_shub_probe(hw);
		if (err < 0)
			return err;
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

	for (i = 0; i < ST_ASM330LHHX_ID_MAX; i++) {
		if (!hw->iio_devs[i])
			continue;

		err = devm_iio_device_register(hw->dev, hw->iio_devs[i]);
		if (err)
			return err;
	}

#ifdef CONFIG_IIO_ST_ASM330LHHX_MLC
	err = st_asm330lhhx_mlc_init_preload(hw);
	if (err)
		return err;
#endif /* CONFIG_IIO_ST_ASM330LHHX_MLC */

#if defined(CONFIG_PM) && defined(CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP)
	device_init_wakeup(dev, 1);
#endif /* CONFIG_PM && CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP */

	dev_info(dev, "Device probed v%s\n", ST_ASM330LHHX_DRV_VERSION);

	return 0;
}
EXPORT_SYMBOL(st_asm330lhhx_probe);

static int
__maybe_unused _st_asm330lhhx_suspend(struct st_asm330lhhx_hw *hw)
{
	struct st_asm330lhhx_sensor *sensor;
	int i, err = 0;

	err = st_asm330lhhx_bk_regs(hw);
	if (err < 0)
		return err;

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

	if (st_asm330lhhx_fsm_running(hw) ||
	    st_asm330lhhx_mlc_running(hw)) {
		struct st_asm330lhhx_sensor *sensor_acc;
		u8 drdy_reg;
		int id_acc;

		/* set low power mode to acc at 12.5 Hz in FIFO */
		id_acc = ST_ASM330LHHX_ID_ACC;
		err = st_asm330lhhx_update_bits_locked(hw,
				st_asm330lhhx_odr_table[id_acc].pm.addr,
				st_asm330lhhx_odr_table[id_acc].pm.mask,
				ST_ASM330LHHX_LP_MODE);
		if (err < 0)
			return err;

		/* reset and flush FIFO data */
		err = st_asm330lhhx_set_fifo_mode(hw, ST_ASM330LHHX_FIFO_BYPASS);
		if (err < 0)
			return err;

		/* get int pin configuration */
		err = _st_asm330lhhx_get_int_reg(hw, hw->int_pin,
						 &drdy_reg);
		if (err < 0)
			return err;

		/* disable FIFO watermark interrupt */
		err = st_asm330lhhx_write_with_mask_locked(hw,
			   drdy_reg, ST_ASM330LHHX_REG_FIFO_TH_MASK, 0);
		if (err < 0)
			return err;

		hw->resume_sample_tick_ns = 80000000ull;
		hw->resume_sample_in_packet = 1;

		/* wait 4 ODRs for internal filter settling time */
		err = st_asm330lhhx_update_odr_fsm(hw, id_acc, id_acc,
			st_asm330lhhx_odr_table[id_acc].odr_avl[0].val,
			4 * (1000000 / 12.5));
		if (err < 0)
			return err;

		/* enable FIFO batch for acc only */
		err = st_asm330lhhx_update_bits_locked(hw,
			hw->odr_table_entry[id_acc].batching_reg.addr,
			hw->odr_table_entry[id_acc].batching_reg.mask,
			st_asm330lhhx_odr_table[id_acc].odr_avl[0].batch_val);
		if (err < 0)
			return err;

		/* disable FIFO batch for gyro */
		err = st_asm330lhhx_update_bits_locked(hw,
			hw->odr_table_entry[ST_ASM330LHHX_ID_GYRO].batching_reg.addr,
			hw->odr_table_entry[ST_ASM330LHHX_ID_GYRO].batching_reg.mask,
			0);
		if (err < 0)
			return err;

		/* setting state to resuming */
		hw->resuming = true;

		/* set FIFO watermark to max level */
		sensor_acc = iio_priv(hw->iio_devs[id_acc]);
		hw->suspend_fifo_watermark = hw->fifo_watermark;
		err = st_asm330lhhx_update_watermark(sensor_acc,
					ST_ASM330LHHX_MAX_FIFO_DEPTH);
		if (err < 0)
			return err;

		/* start acquiring data in FIFO cont. mode */
		err = st_asm330lhhx_set_fifo_mode(hw, ST_ASM330LHHX_FIFO_CONT);
		if (err < 0)
			return err;

		dump_registers("suspend", hw);

	} else {
		if (st_asm330lhhx_is_fifo_enabled(hw)) {
			err = st_asm330lhhx_suspend_fifo(hw);
			if (err < 0)
			      return err;
		}
	}

	return err < 0 ? err : 0;
}

static int __maybe_unused st_asm330lhhx_suspend(struct device *dev)
{
	struct st_asm330lhhx_hw *hw = dev_get_drvdata(dev);
	int err = 0;

	mutex_lock(&hw->fifo_lock);
	err = _st_asm330lhhx_suspend(hw);
	mutex_unlock(&hw->fifo_lock);

	if (err < 0)
		return -EAGAIN;

#ifdef CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP
	if (device_may_wakeup(dev))
		enable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP */

	dev_info(dev, "Suspending device\n");

	mutex_lock(&hw->handler_lock);

	return err < 0 ? err : 0;
}

static int
__maybe_unused _st_asm330lhhx_resume(struct st_asm330lhhx_hw *hw)
{
	struct st_asm330lhhx_sensor *sensor, *sensor_acc;
	int i, err, notify;

	if (hw->resuming) {
		notify = st_asm330lhhx_mlc_check_status(hw);
		if (notify) {
			st_asm330lhhx_read_fifo(hw, notify);
		}

		hw->resuming = false;

		/* stop acc */
		sensor_acc = iio_priv(hw->iio_devs[ST_ASM330LHHX_ID_ACC]);
		err = st_asm330lhhx_set_odr(sensor_acc, 0, 0);
		if (err < 0)
			return err;

		hw->hw_timestamp_enabled = true;
		hw->resume_sample_in_packet = 1;

		/* restore FIFO watermark */
		err = st_asm330lhhx_update_watermark(sensor_acc,
					hw->suspend_fifo_watermark);
		if (err < 0)
			return err;
	}

	mutex_unlock(&hw->handler_lock);

	for (i = 0; i <= ST_ASM330LHHX_ID_EXT1; i++) {
		sensor = iio_priv(hw->iio_devs[i]);
		if (!hw->iio_devs[i])
			continue;

		if (!(hw->enable_mask & BIT(sensor->id)))
			continue;

		err = st_asm330lhhx_set_odr(sensor, sensor->odr,
					    sensor->uodr);
		if (err < 0)
			return err;
	}

	err = st_asm330lhhx_restore_regs(hw);
	if (err < 0)
		return err;

	/* FIFO still configured */
	if (st_asm330lhhx_is_fifo_enabled(hw)) {
		err = st_asm330lhhx_set_fifo_mode(hw, ST_ASM330LHHX_FIFO_CONT);
		if (err < 0)
			return err;
	}

	return err < 0 ? err : 0;
}

static int __maybe_unused st_asm330lhhx_resume(struct device *dev)
{
	struct st_asm330lhhx_hw *hw = dev_get_drvdata(dev);

	dev_info(dev, "Resuming device\n");

#ifdef CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP
	if (device_may_wakeup(dev))
		disable_irq_wake(hw->irq);
#endif /* CONFIG_IIO_ST_ASM330LHHX_MAY_WAKEUP */

	return _st_asm330lhhx_resume(hw);
}

void st_asm330lhhx_shutdown(struct device *dev)
{
	struct st_asm330lhhx_hw *hw = dev_get_drvdata(dev);

	mutex_lock(&hw->fifo_lock);
	/*
	 * after reset the irq line can be pulled up by
	 * hardware, disable it
	 */
	disable_irq(hw->irq);
	mutex_unlock(&hw->fifo_lock);

	/* disable all algos for power consumption reduction */
	st_asm330lhhx_set_page_access(hw, true,
				      ST_ASM330LHHX_REG_FUNC_CFG_MASK);
	regmap_write(hw->regmap, ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR, 0);
	regmap_write(hw->regmap, ST_ASM330LHHX_FSM_ENABLE_A_ADDR, 0);
	regmap_write(hw->regmap, ST_ASM330LHHX_FSM_ENABLE_B_ADDR, 0);
	st_asm330lhhx_set_page_access(hw, false,
				      ST_ASM330LHHX_REG_FUNC_CFG_MASK);

	/* reset device */
	regmap_update_bits(hw->regmap, ST_ASM330LHHX_REG_CTRL3_C_ADDR,
			   ST_ASM330LHHX_REG_SW_RESET_MASK,
			   FIELD_PREP(ST_ASM330LHHX_REG_SW_RESET_MASK,
				      1));

	dev_info(dev, "Set all devices in power down\n");
}
EXPORT_SYMBOL(st_asm330lhhx_shutdown);

const struct dev_pm_ops st_asm330lhhx_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_asm330lhhx_suspend,
				st_asm330lhhx_resume)
};
EXPORT_SYMBOL(st_asm330lhhx_pm_ops);

MODULE_AUTHOR("Mario Tesi <mario.tesi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_asm330lhhx driver");
MODULE_LICENSE("GPL v2");

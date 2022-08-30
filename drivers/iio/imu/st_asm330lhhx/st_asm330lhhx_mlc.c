// SPDX-License-Identifier: GPL-2.0-only
/*
 * STMicroelectronics st_asm330lhhx machine learning core driver
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
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <linux/firmware.h>

#include "st_asm330lhhx.h"
/*
 * Change History
 * 0.1:  First version with static preloaded mlc / fsm
 * 0.2:  Added support to mlc / fsm dynamic load (ucf file)
 * 0.3:  Support static and dynamic fsm / mlc load
 * 0.4:  Added IIO channel for retrieve accelerometer data from fifo after
 *       MLC event detection
 *       Fix fsm/mlc count using bitmask instead of write on INT<X>_ADDR
 *       occurences
 *       Added preload mlc configuration for towing_impact v0.52
 * 0.5:  Add fifo and page lock during mlc updates
 * 0.6:  Move mlc preload configuration data into a separate include file
 * 0.7:  Added st_asm330lhhx_mlc_purge_config method for MLC/FSM configuration
 * 0.8:  Fixed issue when fsm interrupt is enabled but do not generate
 * 	 interrupts
 */
#define ST_ASM330LHHX_MLC_LOADER_VERSION	"0.8"

/* number of machine learning core available on device hardware */
#define ST_ASM330LHHX_MLC_NUMBER		8
#define ST_ASM330LHHX_FSM_NUMBER		16
#define ST_ASM330LHHX_MLC_FIRMWARE_NAME		"st_asm330lhhx_mlc.bin"

#ifdef CONFIG_IIO_ST_ASM330LHHX_MLC_PRELOAD
#include "st_asm330lhhx_preload_mlc.h"
#endif /* CONFIG_IIO_ST_ASM330LHHX_MLC_PRELOAD */

#define FSM_PAGE(__addr) 			((u8)(((__addr >> 8) << 4) | \
						       0x01))
#define FSM_PAGE_MASK(__addr) 			((u8)(__addr >> 8))
#define FSM_OFFSET(__addr) 			((u8)(__addr & 0x00FF))


struct threshold_t {
	u8 th1h;
	u8 th1l;
	u8 th2h;
	u8 th2l;
	u16 addr;
};

struct fsm_duration_t {
	u8 thh;
	u8 thl;
	u16 addr;
};

/* threshold FSM configuration */
static struct threshold_t thresholds[] = {
	{ .th1h = 0x00, .th1l = 0x00, .th2h = 0x00, .th2l = 0x00, .addr = 0x0406 },
	{ .th1h = 0x00, .th1l = 0x00, .th2h = 0x00, .th2l = 0x00, .addr = 0x0448 },
	{ .th1h = 0x00, .th1l = 0x00, .th2h = 0x00, .th2l = 0x00, .addr = 0x048a },
};

/* algo_towing_jack_min_duration */
static struct fsm_duration_t towing_jack_min_duration[] = {
	{ .thh = 0x00, .thl = 0x00, .addr = 0x0410 },
	{ .thh = 0x00, .thl = 0x00, .addr = 0x0452 },
	{ .thh = 0x00, .thl = 0x00, .addr = 0x0494 },
};

/* algo_crash_impact_th */
static struct threshold_t crash_impact_th[] = {
	{ .th1h = 0x00, .th1l = 0x00, .th2h = 0x00, .th2l = 0x00, .addr = 0x04cc },
};

/* algo_crash_min_duration */
static struct fsm_duration_t crash_min_duration[] = {
	{ .thh = 0x00, .thl = 0x00, .addr = 0x04d6 },
};

static struct
iio_dev *st_asm330lhhx_mlc_alloc_iio_dev(struct st_asm330lhhx_hw *hw,
					 enum st_asm330lhhx_sensor_id id);

static const struct
iio_chan_spec st_asm330lhhx_mlc_fifo_acc_channels[] = {
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ACCEL, 0, 1, IIO_MOD_X, 0, 16, 16, 's'),
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ACCEL, 0, 1, IIO_MOD_Y, 1, 16, 16, 's'),
	ST_ASM330LHHX_DATA_CHANNEL(IIO_ACCEL, 0, 1, IIO_MOD_Z, 2, 16, 16, 's'),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const struct iio_chan_spec st_asm330lhhx_mlc_fsm_x_ch[] = {
	ST_ASM330LHHX_EVENT_CHANNEL(IIO_ACTIVITY, thr),
};

static const unsigned long
st_asm330lhhx_fsm_mlc_available_scan_masks[] = {
	0x1, 0x0
};

static const unsigned long st_asm330lhhx_fifo_mlc_scan_masks[] = {
	0x7, 0x0
};

/* remove old mlc/fsm configuration */
static int st_asm330lhhx_mlc_purge_config(struct st_asm330lhhx_hw *hw)
{
	int err;

	err = st_asm330lhhx_update_page_bits_locked(hw,
		 ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
		 ST_ASM330LHHX_MLC_EN_MASK,
		 ST_ASM330LHHX_SHIFT_VAL(0, ST_ASM330LHHX_MLC_EN_MASK));
	if (err < 0)
		return err;

	err = st_asm330lhhx_update_page_bits_locked(hw,
		 ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
		 ST_ASM330LHHX_FSM_EN_MASK,
		 ST_ASM330LHHX_SHIFT_VAL(0, ST_ASM330LHHX_FSM_EN_MASK));
	if (err < 0)
		return err;

	/* wait ~10 ms */
	usleep_range(10000, 10100);

	return 0;
}

static int
st_asm330lhhx_mlc_enable_sensor(struct st_asm330lhhx_sensor *sensor,
				bool enable)
{
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int err = -ENODEV;

	if (sensor->id >= ST_ASM330LHHX_ID_MLC_0 &&
	    sensor->id <= ST_ASM330LHHX_ID_MLC_7) {
		int int_mlc_value;
		int mlc_running;

		mlc_running = st_asm330lhhx_mlc_running(hw);

		int_mlc_value = enable ? hw->mlc_config->mlc_int_mask : 0;
		if (hw->mlc_config->mlc_int_pin  & BIT(0)) {
			err = st_asm330lhhx_write_page_locked(hw,
					ST_ASM330LHHX_MLC_INT1_ADDR,
					&int_mlc_value, 1);
			if (err < 0)
				return err;
		}

		if (hw->mlc_config->mlc_int_pin & BIT(1)) {
			err = st_asm330lhhx_write_page_locked(hw,
					ST_ASM330LHHX_MLC_INT2_ADDR,
					&int_mlc_value, 1);
			if (err < 0)
				return err;
		}

		err = st_asm330lhhx_sensor_set_enable(sensor, enable);
		if (err < 0)
			return err;

		/* check for any other mlc already enabled */
		if ((!mlc_running && st_asm330lhhx_mlc_running(hw)) ||
		    (mlc_running && !st_asm330lhhx_mlc_running(hw))) {
			dev_info(sensor->hw->dev, "Reset MLC Algos\n");
			err = st_asm330lhhx_update_page_bits_locked(hw,
				   ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR,
				   ST_ASM330LHHX_MLC_INIT_MASK,
				   ST_ASM330LHHX_SHIFT_VAL(1,
						ST_ASM330LHHX_MLC_INIT_MASK));
			if (err < 0)
				return err;

#ifdef CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED
			err = st_asm330lhhx_update_page_bits_locked(hw,
					ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
					ST_ASM330LHHX_MLC_EN_MASK,
					ST_ASM330LHHX_SHIFT_VAL(enable,
						ST_ASM330LHHX_MLC_EN_MASK));
			if (err < 0)
				return err;
#endif /* CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED */
		}

		dev_info(sensor->hw->dev,
			"%s MLC sensor %d (INT %x)\n",
			enable ? "Enabling" : "Disabling",
			sensor->id, int_mlc_value);
	}

	return err < 0 ? err : 0;
}

#ifdef CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED
static int
st_asm330lhhx_fsm_check_require_mlc(struct st_asm330lhhx_sensor *sensor,
				    bool enable)
{
	int err = 0;

	if (sensor->id >= ST_ASM330LHHX_ID_FSM_0 &&
	    sensor->id <= ST_ASM330LHHX_ID_FSM_15) {
		if (!st_asm330lhhx_mlc_running(sensor->hw)) {
			err = st_asm330lhhx_update_page_bits_locked(sensor->hw,
					ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
					ST_ASM330LHHX_MLC_EN_MASK,
					ST_ASM330LHHX_SHIFT_VAL(enable,
						ST_ASM330LHHX_MLC_EN_MASK));
			if (err < 0)
				return err;
		}
	}

	return err < 0 ? err : 0;
}
#endif /* CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED */

static int
st_asm330lhhx_fsm_enable_sensor(struct st_asm330lhhx_sensor *sensor,
				bool enable)
{
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int fsm_running = st_asm330lhhx_fsm_running(hw);
	int id = sensor->id;
	u8 mask, bitmask;
	int err = 0;

	if (id >= ST_ASM330LHHX_ID_FSM_0 &&
	    id < ST_ASM330LHHX_ID_FSM_8) {
		mask = BIT(id - ST_ASM330LHHX_ID_FSM_0);
		err = st_asm330lhhx_update_page_bits_locked(hw,
				ST_ASM330LHHX_FSM_ENABLE_A_ADDR,
				mask,
				ST_ASM330LHHX_SHIFT_VAL(enable ? 1 : 0,
						mask));
		if (err < 0)
			return err;

		/* enable interrupts only if requested by ucf */
		bitmask = mask & hw->mlc_config->fsm_int_mask[0];
		if (bitmask) {
			if (hw->mlc_config->mlc_int_pin  & BIT(0)) {
				err = st_asm330lhhx_update_page_bits_locked(hw,
						ST_ASM330LHHX_FSM_INT1_A_ADDR,
						bitmask,
						ST_ASM330LHHX_SHIFT_VAL(enable ? 1 : 0, bitmask));
				if (err < 0)
					return err;
			}

			if (hw->mlc_config->mlc_int_pin  & BIT(1)) {
				err = st_asm330lhhx_update_page_bits_locked(hw,
						ST_ASM330LHHX_FSM_INT2_A_ADDR,
						bitmask,
						ST_ASM330LHHX_SHIFT_VAL(enable ? 1 : 0, bitmask));
				if (err < 0)
					return err;
			}
		}

		dev_info(sensor->hw->dev,
			"%s FSM A sensor %d (INT %x)\n",
			enable ? "Enabling" : "Disabling", id,
			bitmask);
	} else if (id >= ST_ASM330LHHX_ID_FSM_8 &&
		   id < ST_ASM330LHHX_ID_FSM_15) {
		mask = BIT(id - ST_ASM330LHHX_ID_FSM_8);
		err = st_asm330lhhx_update_page_bits_locked(hw,
				ST_ASM330LHHX_FSM_ENABLE_B_ADDR,
				mask,
				ST_ASM330LHHX_SHIFT_VAL(enable ? 1 : 0,
						mask));
		if (err < 0)
			return err;

		bitmask = mask & hw->mlc_config->fsm_int_mask[1];
		if (bitmask) {
			if (hw->mlc_config->mlc_int_pin  & BIT(0)) {
				err = st_asm330lhhx_update_page_bits_locked(hw,
						ST_ASM330LHHX_FSM_INT1_B_ADDR,
						bitmask,
						ST_ASM330LHHX_SHIFT_VAL(enable ? 1 : 0, bitmask));
				if (err < 0)
					return err;
			}

			if (hw->mlc_config->mlc_int_pin  & BIT(1)) {
				err = st_asm330lhhx_update_page_bits_locked(hw,
						ST_ASM330LHHX_FSM_INT2_B_ADDR,
						bitmask,
						ST_ASM330LHHX_SHIFT_VAL(enable ? 1 : 0, bitmask));
				if (err < 0)
					return err;
			}
		}

		dev_info(sensor->hw->dev,
			"%s FSM B sensor %d (INT %x)\n",
			enable ? "Enabling" : "Disabling", id, bitmask);
	} else {
		dev_info(sensor->hw->dev, "Invalid fsm id %d\n", id);

		return -ENODEV;
	}

#ifdef CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED
	/* check if fsm requires mlc enabled */
	err = st_asm330lhhx_fsm_check_require_mlc(sensor, enable);
	if (err < 0)
		return err;
#endif /* CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED */

	err = st_asm330lhhx_sensor_set_enable(sensor, enable);
	if (err < 0)
		return err;

	/* check for any other fsm already enabled */
	if ((!fsm_running && st_asm330lhhx_fsm_running(hw)) ||
	    (fsm_running && !st_asm330lhhx_fsm_running(hw))) {
		dev_info(sensor->hw->dev, "Reset FSM Algos\n");
		err = st_asm330lhhx_update_page_bits_locked(hw,
			   ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR,
			   ST_ASM330LHHX_FSM_INIT_MASK,
			   ST_ASM330LHHX_SHIFT_VAL(1,
					ST_ASM330LHHX_FSM_INIT_MASK));
		if (err < 0)
			return err;

		err = st_asm330lhhx_update_page_bits_locked(hw,
				ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
				ST_ASM330LHHX_FSM_EN_MASK,
				ST_ASM330LHHX_SHIFT_VAL(enable,
					ST_ASM330LHHX_FSM_EN_MASK));
		if (err < 0)
			return err;
	}

	return err < 0 ? err : 0;
}

static int
st_asm330lhhx_mlc_fsm_enable_sensor(struct st_asm330lhhx_sensor *sensor,
				    bool enable)
{
	int err = 0;

	if (sensor->status == ST_ASM330LHHX_MLC_ENABLED) {
		err = st_asm330lhhx_mlc_enable_sensor(sensor, enable);
	} else if (sensor->status == ST_ASM330LHHX_FSM_ENABLED) {
		err = st_asm330lhhx_fsm_enable_sensor(sensor, enable);
	} else {
		return -ENODEV;
	}

	return err < 0 ? err : 0;
}

static int st_asm330lhhx_mlc_write_event_config(struct iio_dev *iio_dev,
				const struct iio_chan_spec *chan,
				enum iio_event_type type,
				enum iio_event_direction dir,
				int state)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);

	return st_asm330lhhx_mlc_fsm_enable_sensor(sensor, state);
}

static int st_asm330lhhx_mlc_read_event_config(struct iio_dev *iio_dev,
				const struct iio_chan_spec *chan,
				enum iio_event_type type,
				enum iio_event_direction dir)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);
	struct st_asm330lhhx_hw *hw = sensor->hw;

	return !!(hw->enable_mask & BIT(sensor->id));
}

/* parse and program mlc / fsm fragments */
static int st_asm330lhhx_program_mlc(const struct firmware *fw,
				     struct st_asm330lhhx_hw *hw)
{
	u8 fsm_int[2] = { 0, 0 }, fsm_enable[2] = { 0, 0 };
	u8 mlc_fsm_en = 0, fsm_mlc_requested_odr = 0;
	u8 mlc_int = 0, mlc_num = 0, fsm_num = 0;
	bool stmc_page = false, skip = false;
	int reg, val, ret, i = 0;
	u32 uodr = 0;
	u16 odr = 0;

	if (!hw->preload_mlc) {
		ret = st_asm330lhhx_mlc_purge_config(hw);
		if (ret < 0)
			return ret;
	}

	mutex_lock(&hw->page_lock);

	while (i < fw->size) {
		reg = fw->data[i++];
		val = fw->data[i++];

		if (reg == 0x01 && val == 0x80) {
			stmc_page = true;
		} else if (reg == 0x01 && val == 0x00) {
			stmc_page = false;
		} else if (stmc_page) {
			/* catch configuration in stmc page */
			switch (reg) {
			case ST_ASM330LHHX_MLC_INT1_ADDR:
			case ST_ASM330LHHX_MLC_INT2_ADDR:
				mlc_int |= val;
				mlc_num = hweight8(mlc_int);
				skip = true;
				break;
			case ST_ASM330LHHX_FSM_INT1_A_ADDR:
			case ST_ASM330LHHX_FSM_INT2_A_ADDR:
				fsm_int[0] |= val;
				fsm_num = hweight16(*(u16 *)fsm_int);
				skip = true;
				break;
			case ST_ASM330LHHX_FSM_INT1_B_ADDR:
			case ST_ASM330LHHX_FSM_INT2_B_ADDR:
				fsm_int[1] |= val;
				fsm_num = hweight16(*(u16 *)fsm_int);
				skip = true;
				break;
			case ST_ASM330LHHX_FSM_ENABLE_A_ADDR:
				fsm_enable[0] |= val;
				skip = true;
				break;
			case ST_ASM330LHHX_FSM_ENABLE_B_ADDR:
				fsm_enable[1] |= val;
				skip = true;
				break;
			case ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR:
				/*
				 * Check if mlc or fsm need to be enabled even
				 * if the interrupts are not used
				 */
				mlc_fsm_en |= val;
				skip = true;
				break;
			default:
				break;
			}
		} else if (!stmc_page) {
			/* catch configuration in page 0 */
			switch (reg) {
			/* skip FIFO and interrupt registers */
			case ST_ASM330LHHX_REG_FIFO_CTRL1_ADDR:
			case ST_ASM330LHHX_REG_FIFO_CTRL2_ADDR:
			case ST_ASM330LHHX_REG_FIFO_CTRL3_ADDR:
			case ST_ASM330LHHX_REG_FIFO_CTRL4_ADDR:
			case ST_ASM330LHHX_REG_INT1_CTRL_ADDR:
			case ST_ASM330LHHX_REG_INT2_CTRL_ADDR:
			case ST_ASM330LHHX_REG_CTRL3_C_ADDR:
			case ST_ASM330LHHX_REG_CTRL4_C_ADDR:
			case ST_ASM330LHHX_REG_CTRL5_C_ADDR:
			case ST_ASM330LHHX_REG_CTRL6_C_ADDR:
			case ST_ASM330LHHX_REG_CTRL7_G_ADDR:
			case ST_ASM330LHHX_REG_CTRL10_C_ADDR:
				skip = true;
				break;
			/* save requested odr for later */
			case ST_ASM330LHHX_CTRL1_XL_ADDR:
				fsm_mlc_requested_odr = val >> 4;
				skip = true;
				break;
			default:
				break;
			}
		}

		if (!skip) {
			ret = regmap_write(hw->regmap, reg, val);
			if (ret) {
				dev_err(hw->dev, "regmap_write fails\n");

				goto unlock_page;
			}
		}

		skip = false;

		if (mlc_num >= ST_ASM330LHHX_MLC_NUMBER ||
		    fsm_num >= ST_ASM330LHHX_FSM_NUMBER)
			break;
	}

	/* if MLC/FSM ODR is not configured uses first available */
	if (!fsm_mlc_requested_odr)
		fsm_mlc_requested_odr = 0x01;

	ret = st_asm330lhhx_get_odr_from_reg(ST_ASM330LHHX_ID_ACC,
					     fsm_mlc_requested_odr,
					     &odr, &uodr);
	if (ret < 0) {
		fsm_num = 0;
		mlc_num = 0;

		dev_err(hw->dev,
			"unsupported ODR %d for MLC/FSM\n",
			fsm_mlc_requested_odr);

		goto unlock_page;
	}

	if (mlc_num) {
		hw->mlc_config->mlc_int_mask = mlc_int;
		hw->mlc_config->status |= ST_ASM330LHHX_MLC_ENABLED;
		hw->mlc_config->mlc_configured = mlc_num;
	}

	if (fsm_num) {
		hw->mlc_config->fsm_int_mask[0] = fsm_int[0];
		hw->mlc_config->fsm_int_mask[1] = fsm_int[1];

		hw->mlc_config->status |= ST_ASM330LHHX_FSM_ENABLED;
		hw->mlc_config->fsm_configured = fsm_num;

		hw->mlc_config->fsm_enabled_mask[0] = fsm_enable[0];
		hw->mlc_config->fsm_enabled_mask[1] = fsm_enable[1];
	}

	hw->mlc_config->mlc_fsm_en = mlc_fsm_en;
	hw->mlc_config->bin_len = fw->size;
	hw->mlc_config->fsm_mlc_requested_odr = odr;
	hw->mlc_config->fsm_mlc_requested_uodr = uodr;

unlock_page:
	mutex_unlock(&hw->page_lock);

	return (fsm_num + mlc_num) > 0 ? fsm_num + mlc_num : 0;
}

static int st_asm330lhhx_read_fsm_data(struct st_asm330lhhx_hw *hw,
				       u16 addr, unsigned int *data)
{
	int init, status, ret = 0;

	mutex_lock(&hw->page_lock);

	ret = st_asm330lhhx_set_page_access(hw, true,
				       ST_ASM330LHHX_REG_FUNC_CFG_MASK);
	if (ret < 0)
		goto unlock_page;

	ret = regmap_read(hw->regmap, ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
                      &status);
	if (ret < 0)
		goto restore_page;

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_PAGE_SEL_ADDR, BIT(1),
				 FIELD_PREP(BIT(1), 1));
	if (ret < 0)
		goto restore_status;

	ret = regmap_write(hw->regmap, ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
			   (status & ~(ST_ASM330LHHX_FSM_EN_MASK |
				       ST_ASM330LHHX_MLC_EN_MASK)));
	if (ret < 0)
		goto restore_status;

	ret = regmap_read(hw->regmap,
			  ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR,
			  &init);
	if (ret < 0)
		goto restore_page;

	ret = regmap_write(hw->regmap,
			   ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR,
			   init & ~(ST_ASM330LHHX_FSM_INIT_MASK |
				     ST_ASM330LHHX_MLC_INIT_MASK));
	if (ret < 0)
		goto restore_status;

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_PAGE_SEL_ADDR, BIT(1),
				 FIELD_PREP(BIT(1), 0));
	if (ret < 0)
		goto restore_status;

	usleep_range(10000, 10100);

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_REG_PAGE_RW,
				 ST_ASM330LHHX_REG_PAGE_READ_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_PAGE_READ_MASK, 1));
	if (ret < 0)
		goto restore_status;

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_PAGE_SEL_ADDR,
				 ST_ASM330LHHX_PAGE_SEL_MASK,
				 FIELD_PREP(ST_ASM330LHHX_PAGE_SEL_MASK,
					    FSM_PAGE_MASK(addr)));
	if (ret < 0)
		goto restore_status;

	ret = regmap_write(hw->regmap, ST_ASM330LHHX_PAGE_ADDRESS_ADDR,
			   FSM_OFFSET(addr));
	if (ret < 0)
		goto restore_status;

	ret = regmap_read(hw->regmap, ST_ASM330LHHX_PAGE_VALUE_ADDR,
			  data);
	if (ret)
		dev_err(hw->dev, "regmap_read fails\n");

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_PAGE_SEL_ADDR,
				 ST_ASM330LHHX_PAGE_SEL_MASK,
				 FIELD_PREP(ST_ASM330LHHX_PAGE_SEL_MASK, 0));
	if (ret < 0)
		goto restore_status;

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_REG_PAGE_RW,
				 ST_ASM330LHHX_REG_PAGE_READ_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_PAGE_WRITE_MASK, 0));

restore_status:
	ret = regmap_write(hw->regmap, ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
			   status);
restore_page:
	st_asm330lhhx_set_page_access(hw, false,
				      ST_ASM330LHHX_REG_FUNC_CFG_MASK);
unlock_page:
	mutex_unlock(&hw->page_lock);

	return ret;
}

/* update fsm thresholds */
static int st_asm330lhhx_write_fsm_data(struct st_asm330lhhx_hw *hw,
					u8 *fsm_update_code,
					int len)
{
	int reg, val;
	int i, ret = 0;
	int status, init;

	mutex_lock(&hw->page_lock);
	ret = st_asm330lhhx_set_page_access(hw, true,
				       ST_ASM330LHHX_REG_FUNC_CFG_MASK);
	if (ret < 0)
		goto unlock_page;

	ret = regmap_read(hw->regmap, ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
			  &status);
	if (ret < 0)
		goto restore_page;

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_PAGE_SEL_ADDR, BIT(1),
				 FIELD_PREP(BIT(1), 1));
	if (ret < 0)
		goto restore_status;

	ret = regmap_write(hw->regmap, ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
			   (status & ~(ST_ASM330LHHX_FSM_EN_MASK |
				       ST_ASM330LHHX_MLC_EN_MASK)));
	if (ret < 0)
		goto restore_status;

	ret = regmap_read(hw->regmap,
			  ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR,
			  &init);
	if (ret < 0)
		goto restore_page;

	ret = regmap_write(hw->regmap,
			   ST_ASM330LHHX_REG_EMB_FUNC_INIT_B_ADDR,
			   (init & ~(ST_ASM330LHHX_FSM_INIT_MASK |
				     ST_ASM330LHHX_MLC_INIT_MASK)));
	if (ret < 0)
		goto restore_status;

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_PAGE_SEL_ADDR, BIT(1),
				 FIELD_PREP(BIT(1), 0));
	if (ret < 0)
		goto restore_status;

	usleep_range(10000, 10100);

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_REG_PAGE_RW,
				 ST_ASM330LHHX_REG_PAGE_WRITE_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_PAGE_WRITE_MASK, 1));
	if (ret < 0)
		goto restore_status;

	for (i = 0; i < len; i += 2) {
		reg = fsm_update_code[i];
		val = fsm_update_code[i + 1];
		ret = regmap_write(hw->regmap, reg, val);
		if (ret) {
			dev_err(hw->dev, "regmap_write fails\n");

			break;
		}
	}

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_PAGE_SEL_ADDR,
				 ST_ASM330LHHX_PAGE_SEL_MASK,
				 FIELD_PREP(ST_ASM330LHHX_PAGE_SEL_MASK, 0));
	if (ret < 0)
		goto restore_status;

	ret = regmap_update_bits(hw->regmap,
				 ST_ASM330LHHX_REG_PAGE_RW,
				 ST_ASM330LHHX_REG_PAGE_WRITE_MASK,
				 FIELD_PREP(ST_ASM330LHHX_REG_PAGE_WRITE_MASK, 0));

restore_status:
	ret = regmap_write(hw->regmap, ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
			   status);
restore_page:
	st_asm330lhhx_set_page_access(hw, false,
				      ST_ASM330LHHX_REG_FUNC_CFG_MASK);
unlock_page:
	mutex_unlock(&hw->page_lock);

	return ret;
}

/* update fsm thresholds */
static int st_asm330lhhx_update_thresholds(struct st_asm330lhhx_hw *hw)
{
	u8 fsm_update_code[] = {
		ST_ASM330LHHX_PAGE_SEL_ADDR, FSM_PAGE(thresholds[0].addr),
		ST_ASM330LHHX_PAGE_ADDRESS_ADDR, FSM_OFFSET(thresholds[0].addr),
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[0].th1l,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[0].th1h,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[0].th2l,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[0].th2h,
		//ST_ASM330LHHX_REG_PAGE_RW, ST_ASM330LHHX_REG_PAGE_WRITE_MASK,
		ST_ASM330LHHX_PAGE_SEL_ADDR, FSM_PAGE(thresholds[1].addr),
		ST_ASM330LHHX_PAGE_ADDRESS_ADDR, FSM_OFFSET(thresholds[1].addr),
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[1].th1l,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[1].th1h,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[1].th2l,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[1].th2h,
		//ST_ASM330LHHX_REG_PAGE_RW, ST_ASM330LHHX_REG_PAGE_WRITE_MASK,
		ST_ASM330LHHX_PAGE_SEL_ADDR, FSM_PAGE(thresholds[2].addr),
		ST_ASM330LHHX_PAGE_ADDRESS_ADDR, FSM_OFFSET(thresholds[2].addr),
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[2].th1l,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[2].th1h,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[2].th2l,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, thresholds[2].th2h,
	};

	return st_asm330lhhx_write_fsm_data(hw, fsm_update_code,
					    ARRAY_SIZE(fsm_update_code));
}

static int st_asm330lhhx_read_thresholds(struct st_asm330lhhx_hw *hw)
{
	unsigned int val;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(thresholds); i++) {
		ret = st_asm330lhhx_read_fsm_data(hw,
						  thresholds[i].addr,
						  &val);
		if (!ret)
			thresholds[i].th1l = val;

		ret = st_asm330lhhx_read_fsm_data(hw,
						  thresholds[i].addr + 1,
						  &val);
		if (!ret)
			thresholds[i].th1h = val;

		ret = st_asm330lhhx_read_fsm_data(hw,
						  thresholds[i].addr + 2,
						  &val);
		if (!ret)
			thresholds[i].th2l = val;

		ret = st_asm330lhhx_read_fsm_data(hw,
						  thresholds[i].addr + 3,
						  &val);
		if (!ret)
			thresholds[i].th2h = val;
	}

	return ret;
}

/* update jack_min_duration */
static int st_asm330lhhx_update_towing_jack_min_duration(struct st_asm330lhhx_hw *hw)
{
	u8 fsm_update_code[] = {
		ST_ASM330LHHX_PAGE_SEL_ADDR, FSM_PAGE(towing_jack_min_duration[0].addr),
		ST_ASM330LHHX_PAGE_ADDRESS_ADDR, FSM_OFFSET(towing_jack_min_duration[0].addr),
		ST_ASM330LHHX_PAGE_VALUE_ADDR, towing_jack_min_duration[0].thl,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, towing_jack_min_duration[0].thh,
		ST_ASM330LHHX_PAGE_SEL_ADDR, FSM_PAGE(towing_jack_min_duration[1].addr),
		ST_ASM330LHHX_PAGE_ADDRESS_ADDR, FSM_OFFSET(towing_jack_min_duration[1].addr),
		ST_ASM330LHHX_PAGE_VALUE_ADDR, towing_jack_min_duration[1].thl,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, towing_jack_min_duration[1].thh,
		ST_ASM330LHHX_PAGE_SEL_ADDR, FSM_PAGE(towing_jack_min_duration[2].addr),
		ST_ASM330LHHX_PAGE_ADDRESS_ADDR, FSM_OFFSET(towing_jack_min_duration[2].addr),
		ST_ASM330LHHX_PAGE_VALUE_ADDR, towing_jack_min_duration[2].thl,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, towing_jack_min_duration[2].thh,
	};

	return st_asm330lhhx_write_fsm_data(hw, fsm_update_code,
					    ARRAY_SIZE(fsm_update_code));
}

static
int st_asm330lhhx_read_towing_jack_min_duration(struct st_asm330lhhx_hw *hw)
{
	unsigned int val;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(towing_jack_min_duration); i++) {
		ret = st_asm330lhhx_read_fsm_data(hw,
						  towing_jack_min_duration[i].addr,
						  &val);
		if (!ret)
			towing_jack_min_duration[i].thl = val;

		ret = st_asm330lhhx_read_fsm_data(hw,
						  towing_jack_min_duration[i].addr + 1,
						  &val);
		if (!ret)
			towing_jack_min_duration[i].thh = val;
	}

	return ret;
}

/* update crash_impact_th */
static
int st_asm330lhhx_update_crash_impact_th(struct st_asm330lhhx_hw *hw)
{
	u8 fsm_update_code[] = {
		ST_ASM330LHHX_PAGE_SEL_ADDR, FSM_PAGE(crash_impact_th[0].addr),
		ST_ASM330LHHX_PAGE_ADDRESS_ADDR, FSM_OFFSET(crash_impact_th[0].addr),
		ST_ASM330LHHX_PAGE_VALUE_ADDR, crash_impact_th[0].th1l,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, crash_impact_th[0].th1h,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, crash_impact_th[0].th2l,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, crash_impact_th[0].th2h,
	};

	return st_asm330lhhx_write_fsm_data(hw, fsm_update_code,
					    ARRAY_SIZE(fsm_update_code));
}

static
int st_asm330lhhx_read_crash_impact_th(struct st_asm330lhhx_hw *hw)
{
	unsigned int val;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(crash_impact_th); i++) {
		ret = st_asm330lhhx_read_fsm_data(hw,
						  crash_impact_th[i].addr,
						  &val);
		if (!ret)
			crash_impact_th[i].th1l = val;

		ret = st_asm330lhhx_read_fsm_data(hw,
						  crash_impact_th[i].addr + 1,
						  &val);
		if (!ret)
			crash_impact_th[i].th1h = val;

		ret = st_asm330lhhx_read_fsm_data(hw,
						  crash_impact_th[i].addr + 2,
						  &val);
		if (!ret)
			crash_impact_th[i].th2l = val;

		ret = st_asm330lhhx_read_fsm_data(hw,
						  crash_impact_th[i].addr + 3,
						  &val);
		if (!ret)
			crash_impact_th[i].th2h = val;
	}

	return ret;
}

/* update algo_crash_min_duration */
static
int st_asm330lhhx_update_crash_min_duration(struct st_asm330lhhx_hw *hw)
{
	u8 fsm_update_code[] = {
		ST_ASM330LHHX_PAGE_SEL_ADDR, FSM_PAGE(crash_min_duration[0].addr),
		ST_ASM330LHHX_PAGE_ADDRESS_ADDR, FSM_OFFSET(crash_min_duration[0].addr),
		ST_ASM330LHHX_PAGE_VALUE_ADDR, crash_min_duration[0].thl,
		ST_ASM330LHHX_PAGE_VALUE_ADDR, crash_min_duration[0].thh,
	};

	return st_asm330lhhx_write_fsm_data(hw, fsm_update_code,
					    ARRAY_SIZE(fsm_update_code));
}

static
int st_asm330lhhx_read_crash_min_duration(struct st_asm330lhhx_hw *hw)
{
	unsigned int val;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(crash_min_duration); i++) {
		ret = st_asm330lhhx_read_fsm_data(hw,
						  crash_min_duration[i].addr,
						  &val);
		if (!ret)
			crash_min_duration[i].thl = val;

		ret = st_asm330lhhx_read_fsm_data(hw,
						  crash_min_duration[i].addr + 1,
						  &val);
		if (!ret)
			crash_min_duration[i].thh = val;
	}

	return ret;
}

static void st_asm330lhhx_mlc_update(const struct firmware *fw,
				     void *context)
{
	bool force_mlc_enabled, force_fsm_enabled = false;
	struct st_asm330lhhx_hw *hw = context;
	enum st_asm330lhhx_sensor_id id;
	u16 fsm_mask = 0;
	u8 mlc_mask = 0;
	int ret, i;

#ifdef CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED
	force_mlc_enabled = false;
#else /* CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED */
	force_mlc_enabled = true;
#endif /* CONFIG_IIO_ST_ASM330LHHX_CHECK_MLC_REQUIRED */

	if (!fw) {
		dev_err(hw->dev, "could not get binary firmware\n");
		return;
	}

	mutex_lock(&hw->fifo_lock);

	ret = st_asm330lhhx_program_mlc(fw, hw);
	if (!ret)
		goto release;

	mlc_mask = hw->mlc_config->mlc_int_mask;
	for (i = 0; i < ST_ASM330LHHX_MLC_NUMBER; i++) {
		if (mlc_mask & BIT(i)) {
			id = st_asm330lhhx_mlc_sensor_list[i];
			hw->iio_devs[id] =
				st_asm330lhhx_mlc_alloc_iio_dev(hw, id);
			if (!hw->iio_devs[id])
				goto release;

			ret = iio_device_register(hw->iio_devs[id]);
			if (ret)
				goto release;
		}
	}

	fsm_mask = (u16)(((u16)hw->mlc_config->fsm_enabled_mask[1] << 8) |
			  hw->mlc_config->fsm_enabled_mask[0]);
	for (i = 0; i < ST_ASM330LHHX_FSM_NUMBER; i++) {
		if (fsm_mask & BIT(i)) {
			id = st_asm330lhhx_fsm_sensor_list[i];
			hw->iio_devs[id] =
				st_asm330lhhx_mlc_alloc_iio_dev(hw, id);
			if (!hw->iio_devs[id])
				goto release;

			ret = iio_device_register(hw->iio_devs[id]);
			if (ret)
				goto release;
		}
	}

	/*
	 * check if int are not configured but mlc/fsm need to
	 * be enabled
	 */
	if ((hw->mlc_config->mlc_fsm_en & ST_ASM330LHHX_MLC_EN_MASK) &&
	    (!hw->mlc_config->mlc_int_mask))
		force_mlc_enabled = true;

	if ((hw->mlc_config->mlc_fsm_en & ST_ASM330LHHX_FSM_EN_MASK) &&
	    (!hw->mlc_config->fsm_int_mask[0]) &&
	    (!hw->mlc_config->fsm_int_mask[1]))
		force_fsm_enabled = true;

	if (force_mlc_enabled) {
		ret = st_asm330lhhx_update_page_bits_locked(hw,
				ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
				ST_ASM330LHHX_MLC_EN_MASK,
				ST_ASM330LHHX_SHIFT_VAL(true,
					    ST_ASM330LHHX_MLC_EN_MASK));
		if (ret < 0)
			goto release;
	}

	if (force_fsm_enabled) {
		ret = st_asm330lhhx_update_page_bits_locked(hw,
				ST_ASM330LHHX_EMB_FUNC_EN_B_ADDR,
				ST_ASM330LHHX_FSM_EN_MASK,
				ST_ASM330LHHX_SHIFT_VAL(true,
					ST_ASM330LHHX_FSM_EN_MASK));
		if (ret < 0)
			goto release;
	}

	dev_info(hw->dev, "MLC loaded (%d) MLC %x FSM %x-%x (MLC %s FSM %s)\n",
		 ret, mlc_mask,
		 (fsm_mask >> 8) & 0xFF, fsm_mask & 0xFF,
		 force_mlc_enabled ? "Forced" : "On req",
		 force_fsm_enabled ? "Forced" : "On req");

release:
	mutex_unlock(&hw->fifo_lock);

	if (hw->preload_mlc) {
		hw->preload_mlc = 0;

		return;
	}

	release_firmware(fw);
}

static int st_asm330lhhx_mlc_flush_all(struct st_asm330lhhx_hw *hw)
{
	struct st_asm330lhhx_sensor *sensor_mlc;
	struct iio_dev *iio_dev;
	int ret = 0, id, i;

	for (i = 0; i < sizeof(st_asm330lhhx_mlc_sensor_list); i++) {
		id = st_asm330lhhx_mlc_sensor_list[i];
		iio_dev = hw->iio_devs[id];
		if (!iio_dev)
			continue;

		sensor_mlc = iio_priv(iio_dev);
		ret = st_asm330lhhx_mlc_fsm_enable_sensor(sensor_mlc,
							  false);
		if (ret < 0)
			break;

		iio_device_unregister(iio_dev);
		iio_device_free(iio_dev);
		hw->iio_devs[id] = NULL;
	}

	for (i = 0; i < sizeof(st_asm330lhhx_fsm_sensor_list); i++) {
		id = st_asm330lhhx_fsm_sensor_list[i];
		iio_dev = hw->iio_devs[id];
		if (!iio_dev)
			continue;

		sensor_mlc = iio_priv(iio_dev);
		ret = st_asm330lhhx_mlc_fsm_enable_sensor(sensor_mlc,
							  false);
		if (ret < 0)
			break;

		iio_device_unregister(iio_dev);
		iio_device_free(iio_dev);
		hw->iio_devs[id] = NULL;
	}

	return st_asm330lhhx_mlc_purge_config(hw);
}

static ssize_t st_asm330lhhx_mlc_info(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;

	return scnprintf(buf, PAGE_SIZE, "mlc %02x fsm %02x\n",
			 hw->mlc_config->mlc_configured,
			 hw->mlc_config->fsm_configured);
}

static ssize_t st_asm330lhhx_mlc_get_version(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "mlc loader Version %s\n",
			 ST_ASM330LHHX_MLC_LOADER_VERSION);
}

static ssize_t st_asm330lhhx_mlc_upload_firmware(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	int err;

	err = request_firmware_nowait(THIS_MODULE, true,
				      ST_ASM330LHHX_MLC_FIRMWARE_NAME,
				      dev, GFP_KERNEL,
				      sensor->hw,
				      st_asm330lhhx_mlc_update);

	return err < 0 ? err : size;
}

static ssize_t st_asm330lhhx_mlc_flush(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = st_asm330lhhx_mlc_flush_all(hw);

	hw->mlc_config->status = 0;
	hw->mlc_config->fsm_configured = 0;
	hw->mlc_config->mlc_configured = 0;
	hw->mlc_config->mlc_fsm_en = 0;
	hw->mlc_config->bin_len = 0;
	hw->mlc_config->fsm_mlc_requested_odr = 0;
	hw->mlc_config->fsm_mlc_requested_uodr = 0;

	return ret < 0 ? ret : size;
}

static int st_asm330lhhx_read_mlc_fifo_raw(struct iio_dev *iio_dev,
					   struct iio_chan_spec const *ch,
					   int *val, int *val2, long mask)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(iio_dev);
	struct st_asm330lhhx_hw *hw = sensor->hw;
	struct st_asm330lhhx_sensor *sensor_acc;
	int ret;

	sensor_acc = iio_priv(hw->iio_devs[ST_ASM330LHHX_ID_ACC]);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = (int)sensor_acc->odr;
		*val2 = (int)sensor_acc->uodr;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (ch->type) {
		case IIO_ACCEL:
			*val = 0;
			*val2 = sensor_acc->gain;
			ret = IIO_VAL_INT_PLUS_MICRO;
			break;
		default:
			return -EINVAL;
		}
		break;
	case IIO_CHAN_INFO_OFFSET:
		return -EINVAL;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int st_asm330lhhx_write_mlc_fifo_raw(struct iio_dev *iio_dev,
					    struct iio_chan_spec const *chan,
					    int val, int val2, long mask)
{
	return 0;
}

static
ssize_t st_asm330lhhx_set_fsm_threshold(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = sscanf(buf, "%hhx,%hhx,%hhx,%hhx,%hhx,%hhx,%hhx,%hhx,%hhx,%hhx,%hhx,%hhx",
		     &thresholds[0].th1h, &thresholds[0].th1l,
		     &thresholds[0].th2h, &thresholds[0].th2l,
		     &thresholds[1].th1h, &thresholds[1].th1l,
		     &thresholds[1].th2h, &thresholds[1].th2l,
		     &thresholds[2].th1h, &thresholds[2].th1l,
		     &thresholds[2].th2h, &thresholds[2].th2l);
	if (ret != 12)
		ret = -EINVAL;

	st_asm330lhhx_update_thresholds(hw);
	
	return size;
}

static ssize_t st_asm330lhhx_get_fsm_threshold(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = st_asm330lhhx_read_thresholds(hw);

	return ret ? ret : scnprintf(buf, PAGE_SIZE,
                                "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                                thresholds[0].th1h, thresholds[0].th1l,
                                thresholds[0].th2h, thresholds[0].th2l,
                                thresholds[1].th1h, thresholds[1].th1l,
                                thresholds[1].th2h, thresholds[1].th2l,
                                thresholds[2].th1h, thresholds[2].th1l,
                                thresholds[2].th2h, thresholds[2].th2l);
}

static
ssize_t st_asm330lhhx_set_towing_jack_min_duration(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = sscanf(buf, "%hhx,%hhx,%hhx,%hhx,%hhx,%hhx",
		     &towing_jack_min_duration[0].thh,
		     &towing_jack_min_duration[0].thl,
		     &towing_jack_min_duration[1].thh,
		     &towing_jack_min_duration[1].thl,
		     &towing_jack_min_duration[2].thh,
		     &towing_jack_min_duration[2].thl);
	if (ret != 6)
		ret = -EINVAL;

	st_asm330lhhx_update_towing_jack_min_duration(hw);
	
	return size;
}

static
ssize_t st_asm330lhhx_get_towing_jack_min_duration(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = st_asm330lhhx_read_towing_jack_min_duration(hw);

	return ret ? ret : scnprintf(buf, PAGE_SIZE,
                                "%x,%x,%x,%x,%x,%x\n",
                                towing_jack_min_duration[0].thh,
                                towing_jack_min_duration[0].thl,
                                towing_jack_min_duration[1].thh,
                                towing_jack_min_duration[1].thl,
                                towing_jack_min_duration[2].thh,
                                towing_jack_min_duration[2].thl);
}

static
ssize_t st_asm330lhhx_set_crash_impact_th(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = sscanf(buf, "%hhx,%hhx,%hhx,%hhx",
		     &crash_impact_th[0].th1h,
		     &crash_impact_th[0].th1l,
		     &crash_impact_th[0].th2h,
		     &crash_impact_th[0].th2l);
	if (ret != 4)
		ret = -EINVAL;

	st_asm330lhhx_update_crash_impact_th(hw);
	
	return size;
}

static
ssize_t st_asm330lhhx_get_crash_impact_th(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = st_asm330lhhx_read_crash_impact_th(hw);

	return ret ? ret : scnprintf(buf, PAGE_SIZE,
                                "%x,%x,%x,%x\n",
                                crash_impact_th[0].th1h,
                                crash_impact_th[0].th1l,
                                crash_impact_th[0].th2h,
                                crash_impact_th[0].th2l);
}

static
ssize_t st_asm330lhhx_set_crash_min_duration(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = sscanf(buf, "%hhx,%hhx",
		     &crash_min_duration[0].thh,
		     &crash_min_duration[0].thl);
	if (ret != 2)
		ret = -EINVAL;

	st_asm330lhhx_update_crash_min_duration(hw);
	
	return size;
}

static
ssize_t st_asm330lhhx_get_crash_min_duration(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct st_asm330lhhx_sensor *sensor = iio_priv(dev_get_drvdata(dev));
	struct st_asm330lhhx_hw *hw = sensor->hw;
	int ret;

	ret = st_asm330lhhx_read_crash_min_duration(hw);

	return ret ? ret : scnprintf(buf, PAGE_SIZE, "%x,%x\n",
                                crash_min_duration[0].thh,
                                crash_min_duration[0].thl);
}

static IIO_DEVICE_ATTR(mlc_info, S_IRUGO,
		       st_asm330lhhx_mlc_info, NULL, 0);
static IIO_DEVICE_ATTR(mlc_flush, S_IWUSR,
		       NULL, st_asm330lhhx_mlc_flush, 0);
static IIO_DEVICE_ATTR(mlc_version, S_IRUGO,
		       st_asm330lhhx_mlc_get_version, NULL, 0);
static IIO_DEVICE_ATTR(load_mlc, S_IWUSR,
		       NULL, st_asm330lhhx_mlc_upload_firmware, 0);
static IIO_DEVICE_ATTR(fsm_threshold, S_IWUSR | S_IRUGO,
		       st_asm330lhhx_get_fsm_threshold,
		       st_asm330lhhx_set_fsm_threshold, 0);
static IIO_DEVICE_ATTR(towing_jack_min_duration, S_IWUSR | S_IRUGO,
		       st_asm330lhhx_get_towing_jack_min_duration,
		       st_asm330lhhx_set_towing_jack_min_duration, 0);
static IIO_DEVICE_ATTR(crash_impact_th, S_IWUSR | S_IRUGO,
		       st_asm330lhhx_get_crash_impact_th,
		       st_asm330lhhx_set_crash_impact_th, 0);
static IIO_DEVICE_ATTR(crash_min_duration, S_IWUSR | S_IRUGO,
		       st_asm330lhhx_get_crash_min_duration,
		       st_asm330lhhx_set_crash_min_duration, 0);

static struct attribute *st_asm330lhhx_mlc_event_attributes[] = {
	&iio_dev_attr_mlc_info.dev_attr.attr,
	&iio_dev_attr_mlc_version.dev_attr.attr,
	&iio_dev_attr_load_mlc.dev_attr.attr,
	&iio_dev_attr_mlc_flush.dev_attr.attr,
	&iio_dev_attr_fsm_threshold.dev_attr.attr,
	&iio_dev_attr_towing_jack_min_duration.dev_attr.attr,
	&iio_dev_attr_crash_impact_th.dev_attr.attr,
	&iio_dev_attr_crash_min_duration.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_asm330lhhx_mlc_event_attribute_group = {
	.attrs = st_asm330lhhx_mlc_event_attributes,
};

static const struct iio_info st_asm330lhhx_mlc_event_info = {
	.attrs = &st_asm330lhhx_mlc_event_attribute_group,
	.read_event_config = st_asm330lhhx_mlc_read_event_config,
	.write_event_config = st_asm330lhhx_mlc_write_event_config,
};

static const struct iio_info st_asm330lhhx_mlc_x_event_info = {
	.read_event_config = st_asm330lhhx_mlc_read_event_config,
	.write_event_config = st_asm330lhhx_mlc_write_event_config,
};

static struct attribute *st_asm330lhhx_mlc_fifo_acc_attributes[] = {
	NULL,
};

static const struct attribute_group st_asm330lhhx_mlc_fifo_acc_attribute_group = {
	.attrs = st_asm330lhhx_mlc_fifo_acc_attributes,
};

static const struct iio_info st_asm330lhhx_mlc_fifo_acc_info = {
	.read_raw = st_asm330lhhx_read_mlc_fifo_raw,
	.write_raw = st_asm330lhhx_write_mlc_fifo_raw,
};

static
struct iio_dev *st_asm330lhhx_mlc_alloc_iio_dev(struct st_asm330lhhx_hw *hw,
						enum st_asm330lhhx_sensor_id id)
{
	struct st_asm330lhhx_sensor *sensor;
	struct iio_dev *iio_dev = NULL;

	/* devm management only for ST_ASM330LHHX_ID_MLC */
	if (id == ST_ASM330LHHX_ID_MLC)
		iio_dev = devm_iio_device_alloc(hw->dev, sizeof(*sensor));
	else
		iio_dev = iio_device_alloc(sizeof(*sensor));

	if (!iio_dev)
		return NULL;

	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->dev.parent = hw->dev;

	sensor = iio_priv(iio_dev);
	sensor->id = id;
	sensor->hw = hw;

	switch (id) {
	case ST_ASM330LHHX_ID_MLC:
		iio_dev->available_scan_masks =
			st_asm330lhhx_fsm_mlc_available_scan_masks;
		iio_dev->channels = st_asm330lhhx_mlc_fsm_x_ch;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhhx_mlc_fsm_x_ch);
		iio_dev->info = &st_asm330lhhx_mlc_event_info;
		scnprintf(sensor->name, sizeof(sensor->name), "asm330lhhx_mlc");
		break;
	case ST_ASM330LHHX_ID_FIFO_MLC:
		iio_dev->channels = st_asm330lhhx_mlc_fifo_acc_channels;
		iio_dev->num_channels =
				ARRAY_SIZE(st_asm330lhhx_mlc_fifo_acc_channels);
		iio_dev->available_scan_masks =
			st_asm330lhhx_fifo_mlc_scan_masks;
		scnprintf(sensor->name, sizeof(sensor->name),
			  "asm330lhhx_mfifo");
		iio_dev->info = &st_asm330lhhx_mlc_fifo_acc_info;
		break;
	case ST_ASM330LHHX_ID_MLC_0:
	case ST_ASM330LHHX_ID_MLC_1:
	case ST_ASM330LHHX_ID_MLC_2:
	case ST_ASM330LHHX_ID_MLC_3:
	case ST_ASM330LHHX_ID_MLC_4:
	case ST_ASM330LHHX_ID_MLC_5:
	case ST_ASM330LHHX_ID_MLC_6:
	case ST_ASM330LHHX_ID_MLC_7:
		iio_dev->available_scan_masks =
			st_asm330lhhx_fsm_mlc_available_scan_masks;
		iio_dev->channels = st_asm330lhhx_mlc_fsm_x_ch;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhhx_mlc_fsm_x_ch);
		iio_dev->info = &st_asm330lhhx_mlc_x_event_info;
		scnprintf(sensor->name, sizeof(sensor->name),
			  "asm330lhhx_mlc_%d", id - ST_ASM330LHHX_ID_MLC_0);
		sensor->outreg_addr = ST_ASM330LHHX_REG_MLC0_SRC_ADDR +
				id - ST_ASM330LHHX_ID_MLC_0;
		sensor->status = ST_ASM330LHHX_MLC_ENABLED;
		sensor->pm = ST_ASM330LHHX_NO_MODE;
		sensor->odr = hw->mlc_config->fsm_mlc_requested_odr;
		sensor->uodr = hw->mlc_config->fsm_mlc_requested_uodr;
		break;
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
		iio_dev->available_scan_masks =
			st_asm330lhhx_fsm_mlc_available_scan_masks;
		iio_dev->channels = st_asm330lhhx_mlc_fsm_x_ch;
		iio_dev->num_channels = ARRAY_SIZE(st_asm330lhhx_mlc_fsm_x_ch);
		iio_dev->info = &st_asm330lhhx_mlc_x_event_info;
		scnprintf(sensor->name, sizeof(sensor->name),
			  "asm330lhhx_fsm_%d", id - ST_ASM330LHHX_ID_FSM_0);
		sensor->outreg_addr = ST_ASM330LHHX_FSM_OUTS1_ADDR +
				id - ST_ASM330LHHX_ID_FSM_0;
		sensor->status = ST_ASM330LHHX_FSM_ENABLED;
		sensor->pm = ST_ASM330LHHX_NO_MODE;
		sensor->odr = hw->mlc_config->fsm_mlc_requested_odr;
		sensor->uodr = hw->mlc_config->fsm_mlc_requested_uodr;
		break;
	default:
		dev_err(hw->dev, "invalid sensor id %d\n", id);
		iio_device_free(iio_dev);

		return NULL;
	}

	iio_dev->name = sensor->name;

	return iio_dev;
}

/*
 * st_asm330lhhx_mlc_check_status - check for mlc/fsm events
 *
 * return: MLC/FSM index, < 0 for error
 */
int st_asm330lhhx_mlc_check_status(struct st_asm330lhhx_hw *hw)
{
	struct st_asm330lhhx_sensor *sensor;
	struct iio_dev *iio_dev;
	__le16 __fsm_status = 0;
	int ret = 0, notify = 0;
	u8 i, mlc_status, id;
	u16 fsm_status;

	if (hw->mlc_config->status & ST_ASM330LHHX_MLC_ENABLED) {
		ret = st_asm330lhhx_read_locked(hw,
					ST_ASM330LHHX_MLC_STATUS_MAINPAGE,
					(void *)&mlc_status, 1);
		if (ret)
			return ret;

		if (mlc_status) {
			u8 mlc_event[ST_ASM330LHHX_MLC_NUMBER];

			for (i = 0; i < ST_ASM330LHHX_MLC_NUMBER; i++) {
				id = st_asm330lhhx_mlc_sensor_list[i];
				if (!(hw->enable_mask & BIT(id)))
					continue;

				if (mlc_status & BIT(i)) {
					iio_dev = hw->iio_devs[id];
					if (!iio_dev) {
						ret = -ENOENT;

						return ret;
					}

					sensor = iio_priv(iio_dev);
					ret = st_asm330lhhx_read_page_locked(hw,
						sensor->outreg_addr,
						(void *)&mlc_event[i], 1);
					if (ret)
						return ret;

					iio_push_event(iio_dev, (u64)mlc_event[i],
						       iio_get_time_ns(iio_dev));

					dev_info(hw->dev,
						 "MLC %d Status %x MLC EVENT %llx\n",
						 id, mlc_status,
						 (u64)mlc_event[i]);

					notify |= BIT(i);
				}
			}
		}
	}

	if (hw->mlc_config->status & ST_ASM330LHHX_FSM_ENABLED) {
		ret = st_asm330lhhx_read_locked(hw,
					ST_ASM330LHHX_FSM_STATUS_A_MAINPAGE,
					(void *)&__fsm_status, 2);
		if (ret)
			return ret;

		fsm_status = le16_to_cpu(__fsm_status);
		if (fsm_status) {
			u8 fsm_event[ST_ASM330LHHX_FSM_NUMBER];

			for (i = 0; i < ST_ASM330LHHX_FSM_NUMBER; i++) {
				id = st_asm330lhhx_fsm_sensor_list[i];
				if (!(hw->enable_mask & BIT(id)))
					continue;

				if (fsm_status & BIT(i)) {
					iio_dev = hw->iio_devs[id];
					if (!iio_dev) {
						ret = -ENOENT;

						return ret;
					}

					sensor = iio_priv(iio_dev);
					ret = st_asm330lhhx_read_page_locked(hw,
						sensor->outreg_addr,
						(void *)&fsm_event[i], 1);
					if (ret)
						return ret;

					iio_push_event(iio_dev, (u64)fsm_event[i],
						       iio_get_time_ns(iio_dev));

					dev_info(hw->dev,
						 "FSM %d Status %x FSM EVENT %llx\n",
						 id, mlc_status,
						 (u64)fsm_event[i]);

					notify |= BIT(i + ST_ASM330LHHX_MLC_NUMBER);
				}
			}
		}
	}

	return ret < 0 ? ret : notify;
}

/**
 * st_asm330lhhx_of_get_mlc_int_pin - Read of configuration of mlc int
 *
 * @hw: Sensor hw structure.
 * @pin: Interrupt pin used by MLC.
 *
 * Possible configurations are:
 * st,mlc-int-pin = <1>; int1 pin will be used by MLC
 * st,mlc-int-pin = <2>; int2 pin will be used by MLC
 * st,mlc-int-pin = <3>; both interrupt pins will be used by MLC
 */
static int st_asm330lhhx_of_get_mlc_int_pin(struct st_asm330lhhx_hw *hw,
					    int *pin)
{
	struct device_node *np = hw->dev->of_node;
	int int_pin;
	int ret;

	if (!np)
		return -EINVAL;

	ret = of_property_read_u32(np, "st,mlc-int-pin", &int_pin);
	if (ret < 0) {
		dev_info(hw->dev,
			 "missing mlc-int-pin, using default (%d)\n",
			 hw->int_pin);
		int_pin = hw->int_pin;
	}

	if (!(int_pin & 0x03)) {
		dev_err(hw->dev,
			"invalid mlc interrupt configuration (%d)\n",
			int_pin);

		return -EINVAL;
	}

	*pin = int_pin;

	return 0;
}

static const struct iio_buffer_setup_ops st_asm330lhhx_mlc_fifo_ops = {
};

int st_asm330lhhx_mlc_probe(struct st_asm330lhhx_hw *hw)
{
	struct iio_buffer *buffer;
	int int_pin;
	int ret;

	ret = st_asm330lhhx_of_get_mlc_int_pin(hw, &int_pin);
	if (ret)
		return -EINVAL;

	/* prepare FIFO_MLC IIO device with buffer */
	hw->iio_devs[ST_ASM330LHHX_ID_FIFO_MLC] =
		st_asm330lhhx_mlc_alloc_iio_dev(hw, ST_ASM330LHHX_ID_FIFO_MLC);
	if (!hw->iio_devs[ST_ASM330LHHX_ID_FIFO_MLC])
		return -ENOMEM;

	buffer = devm_iio_kfifo_allocate(hw->dev);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(hw->iio_devs[ST_ASM330LHHX_ID_FIFO_MLC],
				 buffer);
	hw->iio_devs[ST_ASM330LHHX_ID_FIFO_MLC]->modes |=
						  INDIO_BUFFER_SOFTWARE;
	hw->iio_devs[ST_ASM330LHHX_ID_FIFO_MLC]->setup_ops =
					    &st_asm330lhhx_mlc_fifo_ops;

	/* prepare MLC IIO device */
	hw->iio_devs[ST_ASM330LHHX_ID_MLC] =
		st_asm330lhhx_mlc_alloc_iio_dev(hw, ST_ASM330LHHX_ID_MLC);
	if (!hw->iio_devs[ST_ASM330LHHX_ID_MLC])
		return -ENOMEM;

	hw->mlc_config = devm_kzalloc(hw->dev,
				      sizeof(struct st_asm330lhhx_mlc_config_t),
				      GFP_KERNEL);
	if (!hw->mlc_config)
		return -ENOMEM;

	hw->mlc_config->mlc_int_pin = int_pin;

	return 0;
}

int st_asm330lhhx_mlc_remove(struct device *dev)
{
	struct st_asm330lhhx_hw *hw = dev_get_drvdata(dev);

	return st_asm330lhhx_mlc_flush_all(hw);
}
EXPORT_SYMBOL(st_asm330lhhx_mlc_remove);

int st_asm330lhhx_mlc_init_preload(struct st_asm330lhhx_hw *hw)
{

#ifdef CONFIG_IIO_ST_ASM330LHHX_MLC_PRELOAD
	hw->preload_mlc = 1;
	st_asm330lhhx_mlc_update(&st_asm330lhhx_mlc_preload, hw);
#endif /* CONFIG_IIO_ST_ASM330LHHX_MLC_PRELOAD */

	return 0;
}
			

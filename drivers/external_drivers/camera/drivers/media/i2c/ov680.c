/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/atomisp.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include "ov680.h"

#define to_ov680_device(sub_dev) container_of(sub_dev, struct ov680_device, sd)

static int ov680_i2c_read_reg(struct v4l2_subdev *sd,
			      u16 reg, u8 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	struct i2c_msg msg[2];
	u16 data[2];

	if (!client->adapter) {
		dev_err(&client->dev, "error, no client->adapter\n");
		return -ENODEV;
	}

	memset(msg, 0, sizeof(msg));
	memset(data, 0, sizeof(data));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = I2C_MSG_LENGTH;
	msg[0].buf = (u8 *)data;
	/* high byte goes first */
	data[0] = cpu_to_be16(reg);

	msg[1].addr = client->addr;
	msg[1].len = OV680_16BIT;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = (u8 *)data;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2) {
		if (ret >= 0)
			ret = -EIO;
		goto error;
	}

	*val = (u8)data[0];

	return 0;

error:
	dev_err(&client->dev, "read offset 0x%x error %d\n", reg, ret);
	return ret;
}

static int ov680_i2c_write(struct i2c_client *client, u16 len, u8 *data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);

	return ret == num_msg ? 0 : -EIO;
}

static int ov680_i2c_write_reg(struct v4l2_subdev *sd,
			       u16 reg, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg;
	const u16 len = 1 + sizeof(u16); /* 16-bit address + data */

	if (!client->adapter) {
		dev_err(&client->dev, "error, no client->adapter\n");
		return -ENODEV;
	}

	/* high byte goes out first */
	wreg = (u16 *)data;
	*wreg = cpu_to_be16(reg);

	data[2] = val;

	ret = ov680_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

/*
 * ov680_write_reg_array - Initializes a list of registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __ov680_flush_reg_array(), __ov680_buf_reg_array() and
 * __ov680_write_reg_is_consecutive() are internal functions to
 * ov680_write_reg_array() and should be not used anywhere else.
 */
#ifdef OV680_FAST_I2C_REG_ARRAY
static int __ov680_flush_reg_array(struct i2c_client *client,
				   struct ov680_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return ov680_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __ov680_buf_reg_array(struct i2c_client *client,
				   struct ov680_write_ctrl *ctrl,
				   const struct ov680_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case OV680_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case OV680_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->reg;

	ctrl->index += size;

	/*
	 * Buffer cannot guarantee free space for u32? Better flush it to avoid
	 * possible lack of memory for next item.
	 */
	if (ctrl->index + sizeof(u16) >= OV680_MAX_WRITE_BUF_SIZE)
		__ov680_flush_reg_array(client, ctrl);

	return 0;
}

static int
__ov680_write_reg_is_consecutive(struct i2c_client *client,
				 struct ov680_write_ctrl *ctrl,
				 const struct ov680_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

static int ov680_write_reg_array(struct v4l2_subdev *sd,
				 const struct ov680_reg *reglist)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct ov680_reg *next = reglist;
	struct ov680_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != OV680_TOK_TERM; next++) {
		switch (next->type & OV680_TOK_MASK) {
		case OV680_TOK_DELAY:
			err = __ov680_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__ov680_write_reg_is_consecutive(client, &ctrl,
							      next)) {
				err = __ov680_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __ov680_buf_reg_array(client, &ctrl, next);
			if (err) {
				dev_err(&client->dev, "write error, aborted\n");
				return err;
			}
			break;
		}
	}

	return __ov680_flush_reg_array(client, &ctrl);
}
#else
static int ov680_write_reg_array(struct v4l2_subdev *sd,
				 const struct ov680_reg *reglist)
{
	const struct ov680_reg *next = reglist;
	int err = 0;
	for (; next->type != OV680_TOK_TERM; next++) {
		switch (next->type & OV680_TOK_MASK) {
		case OV680_TOK_DELAY:
			msleep(next->val);
			break;
		default:
			err = ov680_i2c_write_reg(sd, next->reg, next->val);
			if (err)
				return err;
			break;
		}
	}
	return err;
}
#endif

static int ov680_read_sensor(struct v4l2_subdev *sd, int sid,
			     u16 reg, u8 *data) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = ov680_i2c_write_reg(sd, OV680_CMD_OP_REG, OV680_CMD_READ_SENSOR);
	if (ret)
		dev_dbg(&client->dev, "1%s - reg = %x failed\n", __func__, reg);
	ret = ov680_i2c_write_reg(sd, OV680_CMD_SUB_OP_REG, sid);
	if (ret)
		dev_dbg(&client->dev, "2%s - reg = %x failed\n", __func__, reg);
	/* high address */
	ret = ov680_i2c_write_reg(sd, OV680_CMD_PARAMETER_1, reg >> 8);
	if (ret)
		dev_dbg(&client->dev, "3%s - reg = %x failed\n", __func__, reg);
	/* low address */
	ret = ov680_i2c_write_reg(sd, OV680_CMD_PARAMETER_2, reg & 0xff);
	if (ret)
		dev_dbg(&client->dev, "4%s - reg = %x failed\n", __func__, reg);
	ret = ov680_i2c_write_reg(sd, OV680_CMD_CIR_REG,
				  OV680_CMD_CIR_SENSOR_ACCESS_STATE);
	if (ret)
		dev_dbg(&client->dev, "5%s - reg = %x failed\n", __func__, reg);
	usleep_range(8000, 10000);
	ret = ov680_i2c_read_reg(sd, OV680_CMD_PARAMETER_4, data);
	if (ret)
		dev_dbg(&client->dev, "6%s - reg = %x failed\n", __func__, reg);
	dev_dbg(&client->dev, "%s - sid = %x, reg = %x, data= %x successful\n",
		__func__, sid, reg, *data);
	return ret;
}

static int ov680_check_sensor_avail(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 data[2];
	int ret, i;
	bool sensor_fail = false;
	int sid[OV680_MAX_INPUT_SENSOR_NUM] = {
		OV680_SENSOR_0_ID, OV680_SENSOR_1_ID
	};

	for (i = 0; i < OV680_MAX_INPUT_SENSOR_NUM; i++) {
		ret = ov680_read_sensor(sd, sid[i], 0x0000, &data[0]);
		ret = ov680_read_sensor(sd, sid[i], 0x0001, &data[1]);
		if (ret || data[0] != OV680_SENSOR_REG0_VAL ||
		    data[1] != OV680_SENSOR_REG1_VAL) {
			dev_err(&client->dev, "Subdev OV680 sensor %d with"\
				" id:0x%x detection failure.\n", i, sid[i]);
			sensor_fail = true;
		} else {
			dev_info(&client->dev,
				 "Subdev OV680 sensor %d with id:0x%x"\
				 " detection Successful.\n", i, sid[i]);
		}
	}

	return sensor_fail ? -1 : 0;
}

#ifdef ov680_DUMP_DEBUG
static int ov680_dump_snr_regs(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 data;

	int sid = OV680_SENSOR_0_ID;
	ret = ov680_read_sensor(sd, sid, 0x0000, &data); /* 0x97 */
	ret = ov680_read_sensor(sd, sid, 0x0001, &data); /* 0x28 */
	ret = ov680_read_sensor(sd, sid, 0x0100, &data);

	sid = OV680_SENSOR_1_ID;
	ret = ov680_read_sensor(sd, sid, 0x0000, &data); /* 0x97 */
	ret = ov680_read_sensor(sd, sid, 0x0001, &data); /* 0x28 */

	ret = ov680_read_sensor(sd, sid, 0x0100, &data);
	dev_dbg(&client->dev, "OV680 dump sns reg\n");
	return 0;
}

static int ov680_dump_res_regs(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 reg_val;
	u16 reg;

	for (reg = REG_SC_90; reg <= REG_SC_93; reg++) {
		ov680_i2c_read_reg(sd, reg, &reg_val);
		dev_dbg(&client->dev, "%s: reg 0x%4x - with value 0x%2x\n",
			__func__, reg, reg_val);
	}

	return 0;
}

static int ov680_dump_rx_regs(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 reg_val;
	u16 reg;

	for (reg = REG_YUV_CROP1_08; reg <= REG_YUV_CROP1_0B; reg++) {
		ov680_i2c_read_reg(sd, reg, &reg_val);
		dev_dbg(&client->dev, "%s: reg %x - with value %x\n",
			__func__, reg, reg_val);
	}

	for (reg = REG_YUV_CROP1_08; reg <= REG_YUV_CROP1_0B; reg++) {
		ov680_i2c_read_reg(sd, reg, &reg_val);
		dev_dbg(&client->dev, "%s: reg %x - with value %x\n",
			__func__, reg, reg_val);
	}
	return 0;
}
#endif

static int ov680_write_firmware(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov680_device *dev = to_ov680_device(sd);
	int count, ret;
	u16 len;
	const struct ov680_firmware *ov680_fw_header =
		(const struct ov680_firmware *)dev->fw->data;

	count = ov680_fw_header->cmd_count;
	len = count + sizeof(u16); /* 16-bit address + data */

	ret = ov680_i2c_write(client, len, (u8 *)dev->ov680_fw);
	if (ret)
		dev_err(&client->dev, "write failure\n");

	return ret;
}

static int ov680_load_firmware(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov680_device *dev = to_ov680_device(sd);
	int ret;
	u8 read_value;
	unsigned int read_timeout = 500;

	dev_info(&client->dev, "Start to load firmware.\n");

	/* Init clock PLL */
	ret = ov680_write_reg_array(sd, ov680_init_clock_pll);
	if (ret) {
		dev_err(&client->dev, "%s - clock init failed\n", __func__);
		return ret;
	}

	/* Change clock for FW loading */
	ret = ov680_write_reg_array(sd, ov680_dw_fw_change_pll);
	if (ret) {
		dev_err(&client->dev, "%s - clock set failed\n", __func__);
		return ret;
	}

	/* Load FW */
	ret = ov680_write_firmware(sd);
	if (ret) {
		dev_err(&client->dev, "%s - FW load failed\n", __func__);
		return ret;
	}

	/* Restore clock for FW loading */
	ret = ov680_write_reg_array(sd, ov680_dw_fw_change_back_pll);
	if (ret) {
		dev_err(&client->dev, "%s - clk restore failed\n", __func__);
		return ret;
	}

	/* Check for readiness */
	while (read_timeout) {
		ret = ov680_i2c_read_reg(sd, REG_SC_66, &read_value);
		if (ret) {
			dev_err(&client->dev,
				"%s - status check failed\n", __func__);
			return ret;
		} else if (REG_SC_66_GLOBAL_READY == read_value) {
			break;
		} else {
			usleep_range(1000, 2000);
			dev_dbg(&client->dev,
				"%s - status check val: %x\n", __func__,
				read_value);
			--read_timeout;
		}
	}

	if (0 == read_timeout) {
		dev_err(&client->dev,
			"%s - status check timed out\n", __func__);
		return -EBUSY;
	}

	if (dev->probed) {
		/* turn embedded line on */
		ret = ov680_write_reg_array(sd, ov680_720p_2s_embedded_line);
		if (ret) {
			dev_err(&client->dev, "%s - turn embedded on failed\n",
					__func__);
			return ret;
		}
	}
	dev_info(&client->dev, "firmware load successfully.\n");
	return ret;
}

static int __ov680_s_power(struct v4l2_subdev *sd, int on, int load_fw)
{
	struct ov680_device *dev = to_ov680_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	dev_info(&client->dev, "%s - on-%d.\n", __func__, on);

	/* clock control */
	/*
	 * WA: If the app did not disable the clock before exit,
	 * driver has to disable it firstly, or the clock cannot
	 * be enabled any more after device enter sleep.
	 */
	if (dev->power_on && on)
		dev->platform_data->flisclk_ctrl(sd, 0);

	ret = dev->platform_data->flisclk_ctrl(sd, on);
	if (ret) {
		dev_err(&client->dev,
			"%s - set clock error.\n", __func__);
		return ret;
	}

	ret = dev->platform_data->power_ctrl(sd, on);
	if (ret) {
		dev_err(&client->dev,
			"ov680_s_power error. on=%d ret=%d\n", on, ret);
		return ret;
	}

	ret = dev->platform_data->gpio_ctrl(sd, on);
	if (ret) {
		dev_err(&client->dev,
			"%s - gpio control failed\n", __func__);
		return ret;
	}

	dev->power_on = on;
	if (on) {
		/* Load firmware after power on. */
		ret = ov680_load_firmware(sd);
		if (ret)
			dev_err(&client->dev,
				"ov680_load_firmware failed. ret=%d\n", ret);
#ifdef OV680_DUMP_DEBUG
		ov680_dump_rx_regs(sd);
		ov680_dump_res_regs(sd);
		ov680_dump_snr_regs(sd);
#endif
	}

	return ret;
}

static int ov680_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov680_device *dev = to_ov680_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __ov680_s_power(sd, on, 0);
	mutex_unlock(&dev->input_lock);

	dev_dbg(&client->dev, "%s -flag =%d,  ret = %d\n", __func__, on, ret);

	return ret;
}

static int ov680_s_config(struct v4l2_subdev *sd, void *pdata)
{
	struct ov680_device *dev = to_ov680_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *mipi_info;
	u8 reg_val = 0;
	int ret;

	dev_info(&client->dev, "ov680_s_config is called.\n");
	if (pdata == NULL)
		return -ENODEV;

	dev->platform_data = (struct camera_sensor_platform_data *)pdata;

	mutex_lock(&dev->input_lock);

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret)
			goto fail_power;
	}

	ret = __ov680_s_power(sd, 1, 0);
	if (ret)
		goto fail_power;

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_config;

	/* Detect for OV680 */
	ret = ov680_i2c_read_reg(sd, REG_SC_00, &reg_val);
	if (ret) {
		dev_err(&client->dev, "ov680_i2c_read_reg fails: %d\n", ret);
		goto fail_config;
	}

	if (reg_val != 0x1E) { /* default value of REG_SC_00*/
		ret = -EINVAL;
		dev_err(&client->dev,
			"register value doesn't match: 0x1E != 0x%02x\n",
			reg_val);
		goto fail_config;
	}

	/* reg access test purpose */
	ret = ov680_i2c_write_reg(sd, REG_SC_00, 0x03);
	if (ret) {
		dev_err(&client->dev, "ov680 write reg failed\n");
		goto fail_config;
	}

	ret = ov680_i2c_read_reg(sd, REG_SC_00, &reg_val);

	if (ret) {
		dev_err(&client->dev, "ov680_i2c_read_reg fails: %d\n", ret);
		goto fail_config;
	}

	if (reg_val != 0x03) {
		ret = -EINVAL;
		dev_err(&client->dev,
			"register value doesn't match: 0x03 != 0x%02x\n",
			reg_val);
		goto fail_config;
	}

	ov680_i2c_write_reg(sd, REG_SC_00, 0x1E); /* write back value */
	dev_info(&client->dev, "Subdev OV680 Chip detect with reg access ok\n");

	/* detect the input sensor */
	ret = ov680_check_sensor_avail(sd);
	if (ret) {
		dev_err(&client->dev, "detect sensors failed. ret=%d\n", ret);
		goto fail_config;
	}

	mipi_info = v4l2_get_subdev_hostdata(sd);
	if (!mipi_info) {
		dev_err(&client->dev, "ov680_s_config get mipi info failed\n");
		goto fail_config;
	}
	dev->num_lanes = mipi_info->num_lanes;
	/* bayer output or soc output */
	if (mipi_info->input_format == ATOMISP_INPUT_FORMAT_YUV422_8) {
		dev_dbg(&client->dev, "ov680_s_config - yuv output\n");
		dev->bayer_fmt = 0;
		dev->mbus_pixelcode = V4L2_MBUS_FMT_UYVY8_1X16;
	} else {
		dev_dbg(&client->dev, "ov680_s_config - bayer output\n");
		dev->bayer_fmt = 1;
		dev->mbus_pixelcode = V4L2_MBUS_FMT_SBGGR10_1X10;
	}

	ret = __ov680_s_power(sd, 0, 0);
	if (ret)
		dev_err(&client->dev, "ov680 power-down err.\n");

	mutex_unlock(&dev->input_lock);
	return ret;

fail_config:
	dev->platform_data->csi_cfg(sd, 0);
	__ov680_s_power(sd, 0, 0);
fail_power:
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "ov680_s_config failed\n");
	return ret;
}

static int ov680_enum_mbus_fmt(struct v4l2_subdev *sd,
			       unsigned int index,
			       enum v4l2_mbus_pixelcode *code)
{
	struct ov680_device *dev = to_ov680_device(sd);
	*code = dev->mbus_pixelcode;
	return 0;
}

static int ov680_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	struct ov680_device *dev = to_ov680_device(sd);

	if (code->index >= N_FW)
		return -EINVAL;

	code->code = dev->mbus_pixelcode;
	return 0;
}

static int ov680_match_resolution(struct v4l2_mbus_framefmt *fmt)
{
	s32 w0, h0, mismatch, distance;
	s32 w1 = fmt->width;
	s32 h1 = fmt->height;
	s32 min_distance = INT_MAX;
	s32 i, idx = -1;

	if (w1 == 0 || h1 == 0)
		return -1;

	for (i = 0; i < N_FW; i++) {
		w0 = ov680_res_list[i].width;
		h0 = ov680_res_list[i].height;
		if (w0 < w1 || h0 < h1)
			continue;
		mismatch = abs(w0 * h1 - w1 * h0) * 8192 / w1 / h0;
		if (mismatch > 8192 * OV680_MAX_RATIO_MISMATCH / 100)
			continue;
		distance = (w0 * h1 + w1 * h0) * 8192 / w1 / h1;
		if (distance < min_distance) {
			min_distance = distance;
			idx = i;
		}
	}

	return idx;
}

static s32 ov680_try_mbus_fmt_locked(struct v4l2_subdev *sd,
				     struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov680_device *dev = to_ov680_device(sd);
	s32 res_num, idx = -1;

	res_num = N_FW;

	if ((fmt->width <= ov680_res_list[res_num - 1].width) &&
	    (fmt->height <= ov680_res_list[res_num - 1].height))
		idx = ov680_match_resolution(fmt);
	if (idx == -1)
		idx = res_num - 1;

	fmt->width = ov680_res_list[idx].width;
	fmt->height = ov680_res_list[idx].height;
	fmt->code = dev->mbus_pixelcode;
	dev_dbg(&client->dev, "%s - w = %d, h = %d, idx = %d\n",
		__func__, fmt->width, fmt->height, idx);
	return idx;
}

static int ov680_try_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov680_device *dev = to_ov680_device(sd);

	dev_dbg(&client->dev, "%s- mbus format\n", __func__);
	mutex_lock(&dev->input_lock);
	dev->fw_index = ov680_try_mbus_fmt_locked(sd, fmt);
	dev_dbg(&client->dev, "%s - w = %d, h = %d, code=%x\n",
		__func__, fmt->width, fmt->height, fmt->code);
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov680_get_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct ov680_device *dev = to_ov680_device(sd);

	mutex_lock(&dev->input_lock);
	fmt->code = dev->mbus_pixelcode;
	fmt->width = ov680_res_list[dev->fw_index].width;
	fmt->height = ov680_res_list[dev->fw_index].height;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov680_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct ov680_device *dev = to_ov680_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *ov680_info = NULL;
	int ret;

	dev_dbg(&client->dev, "%s - mbusf\n", __func__);

	ov680_info = v4l2_get_subdev_hostdata(sd);
	if (ov680_info == NULL) {
		dev_dbg(&client->dev, "%s - mipi failed\n", __func__);
		return -EINVAL;
	}

	ret = ov680_try_mbus_fmt(sd, fmt);
	if (ret)
		goto out;

	/* Sanity check */
	if (unlikely(dev->fw_index == -1)) {
		dev_dbg(&client->dev, "%s - fw_index failed\n", __func__);
		ret = -EINVAL;
	}

	switch (ov680_info->input_format) {
	case ATOMISP_INPUT_FORMAT_YUV422_8:
		ov680_info->metadata_width = fmt->width * 2;
		break;
	case ATOMISP_INPUT_FORMAT_RAW_10:
		ov680_info->metadata_width = fmt->width * 10 / 8;
		break;
	case ATOMISP_INPUT_FORMAT_RAW_8:
		ov680_info->metadata_width = fmt->width;
		break;
	default:
		ov680_info->metadata_width = 0;
		dev_err(&client->dev, "%s unsupported format for embedded data.\n",
			__func__);
	}

	ov680_info->metadata_height = 2;
	ov680_info->metadata_format = ATOMISP_INPUT_FORMAT_EMBEDDED;

out:
	dev_dbg(&client->dev, "%s - mbusf done ret %d\n", __func__, ret);
	return ret;
}

static int ov680_enum_framesizes(struct v4l2_subdev *sd,
				 struct v4l2_frmsizeenum *fsize)
{
	struct ov680_device *dev = to_ov680_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	s32 index = fsize->index;

	dev_dbg(&client->dev, "%s\n", __func__);

	if (index >= N_FW)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = ov680_res_list[index].width;
	fsize->discrete.height = ov680_res_list[index].height;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov680_enum_frameintervals(struct v4l2_subdev *sd,
				     struct v4l2_frmivalenum *fival)
{
	struct ov680_device *dev = to_ov680_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned int i = fival->index;

	dev_dbg(&client->dev, "%s - i = %d\n", __func__, i);

	if (i >= N_FW)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = ov680_res_list[i].width;
	fival->height = ov680_res_list[i].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = ov680_res_list[i].fps;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov680_g_frame_interval(struct v4l2_subdev *sd,
				  struct v4l2_subdev_frame_interval *interval)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov680_device *dev = to_ov680_device(sd);
	dev_dbg(&client->dev, "%s\n", __func__);

	mutex_lock(&dev->input_lock);
	interval->interval.denominator = ov680_res_list[dev->fw_index].fps;
	interval->interval.numerator = 1;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov680_enum_frame_size(struct v4l2_subdev *sd,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_frame_size_enum *fse)
{
	struct ov680_device *dev = to_ov680_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int index = fse->index;

	dev_dbg(&client->dev, "%s - index = %d\n", __func__, index);

	if (index >= N_FW)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fse->min_width = ov680_res_list[index].width;
	fse->min_height = ov680_res_list[index].height;
	fse->max_width = ov680_res_list[index].width;
	fse->max_height = ov680_res_list[index].height;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static struct
v4l2_mbus_framefmt *__ov680_get_pad_format(struct ov680_device *dev,
					   struct v4l2_subdev_fh *fh,
					   unsigned int pad,
					   enum v4l2_subdev_format_whence which)
{
	dev_dbg((struct device *)dev, "%s\n", __func__);

	if (pad != 0)
		dev_err((struct device *)dev,
			"%s err, pad %x\n", __func__, pad);

	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);

	return &dev->format;
}

static int ov680_get_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov680_device *dev = to_ov680_device(sd);
	struct v4l2_mbus_framefmt *format =
			__ov680_get_pad_format(dev, fh, fmt->pad, fmt->which);

	dev_dbg(&client->dev, "%s\n", __func__);

	if (!format) {
		dev_dbg(&client->dev, "%s - failed\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&dev->input_lock);
	fmt->format = *format;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov680_set_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov680_device *dev = to_ov680_device(sd);

	dev_dbg(&client->dev, "%s\n", __func__);

	mutex_lock(&dev->input_lock);
	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov680_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s\n", __func__);

	*frames = 0;
	return 0;
}

static int ov680_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov680_device *dev = to_ov680_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	dev_dbg(&client->dev, "ov680_s_stream. enable=%d\n", enable);

	mutex_lock(&dev->input_lock);
	if (dev->power_on && enable) {
		/* start streaming */
		ret = ov680_write_reg_array(sd, ov680_720p_2s_embedded_stream_on);
		if (ret) {
			dev_err(&client->dev,
				"%s - stream on failed\n", __func__);
			dev->sys_activated = 0;
		} else {
			dev->sys_activated = 1;
		}
	} else { /* stream off */

		ret = ov680_i2c_write_reg(sd, REG_SC_03,
					  REG_SC_03_GLOBAL_DISABLED);
		if (ret)
			dev_err(&client->dev,
				"%s - stream off failed\n", __func__);
		dev->sys_activated = 0;
	}

	mutex_unlock(&dev->input_lock);
	return ret;
}

static int ov680_set_exposure(struct v4l2_subdev *sd, s32 val)
{
	/* to do: to control ov9729 exposure */
	return 0;
}

static int ov680_set_wb_mode(struct v4l2_subdev *sd, s32 val)
{
	/* to do: to set wb mode */
	return 0;
}

static int ov680_set_special_effect(struct v4l2_subdev *sd, s32 val)
{
	/* to do: to enable digital effects SDE */
	return 0;
}

static int ov680_g_ctrl(struct v4l2_ctrl *ctrl)
{
	if (!ctrl)
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_FOCUS_STATUS:
		ctrl->val = 0;
		break;
	case V4L2_CID_BIN_FACTOR_HORZ:
	case V4L2_CID_BIN_FACTOR_VERT:
		ctrl->val = 0;
		break;
	case V4L2_CID_LINK_FREQ:
		ctrl->val = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ov680_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov680_device *dev = container_of(
		ctrl->handler, struct ov680_device, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_RUN_MODE:
		dev->fw_index = 0;
		break;
	case V4L2_CID_EXPOSURE:
		ov680_set_exposure(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
		ov680_set_wb_mode(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_COLORFX:
		ov680_set_special_effect(&dev->sd, ctrl->val);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}



static int ov680_g_register(struct v4l2_subdev *sd,
			    struct v4l2_dbg_register *reg)
{
	struct ov680_device *dev = to_ov680_device(sd);
	int ret;
	u8 reg_val;

	if (reg->size != 2)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	if (dev->power_on)
		ret = ov680_i2c_read_reg(sd, reg->reg, &reg_val);
	else
		ret = -EIO;
	mutex_unlock(&dev->input_lock);
	if (ret)
		return ret;

	reg->val = reg_val;

	return 0;
}

static int ov680_s_register(struct v4l2_subdev *sd,
			    const struct v4l2_dbg_register *reg)
{
	struct ov680_device *dev = to_ov680_device(sd);
	int ret;

	if (reg->size != 2)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	if (dev->power_on)
		ret = ov680_i2c_write_reg(sd, reg->reg, reg->val);
	else
		ret = -EIO;
	mutex_unlock(&dev->input_lock);
	return ret;
}

static long ov680_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	switch (cmd) {
	case VIDIOC_DBG_G_REGISTER:
		ret = ov680_g_register(sd, arg);
		break;
	case VIDIOC_DBG_S_REGISTER:
		ret = ov680_s_register(sd, arg);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = ov680_s_ctrl,
	.g_volatile_ctrl = ov680_g_ctrl,
};

static const char * const ctrl_run_mode_menu[] = {
	NULL,
	"Video",
	"Still capture",
	"Continuous capture",
	"Preview",
};

static const struct v4l2_ctrl_config ctrls[] = {
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_RUN_MODE,
		.name = "Run Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.min = 1,
		.def = 4,
		.max = 4,
		.qmenu = ctrl_run_mode_menu,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_EXPOSURE,
		.name = "Exposure",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 5,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE,
		.name = "White Balance",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 9,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_COLORFX,
		.name = "Color Special Effect",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.def = 0,
		.max = 15,
		.step = 1,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_BIN_FACTOR_HORZ,
		.name = "horizontal binning factor",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_BIN_FACTOR_VERT,
		.name = "vertical binning factor",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_LINK_FREQ,
		.name = "Link Frequency",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 1,
		.max = 1,
		.step = 1,
		.def = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
	},
};

static struct v4l2_subdev_sensor_ops ov680_sensor_ops = {
	.g_skip_frames	= ov680_g_skip_frames,
};

static const struct v4l2_subdev_video_ops ov680_video_ops = {
	.try_mbus_fmt = ov680_try_mbus_fmt,
	.s_mbus_fmt = ov680_set_mbus_fmt,
	.g_mbus_fmt = ov680_get_mbus_fmt,
	.s_stream = ov680_s_stream,
	.enum_framesizes = ov680_enum_framesizes,
	.enum_frameintervals = ov680_enum_frameintervals,
	.enum_mbus_fmt = ov680_enum_mbus_fmt,
	.g_frame_interval = ov680_g_frame_interval,
};

static const struct v4l2_subdev_core_ops ov680_core_ops = {
	.s_power	= ov680_s_power,
	.queryctrl	= v4l2_subdev_queryctrl,
	.g_ctrl		= v4l2_subdev_g_ctrl,
	.s_ctrl		= v4l2_subdev_s_ctrl,
	.ioctl		= ov680_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= ov680_g_register,
	.s_register	= ov680_s_register,
#endif
};

static const struct v4l2_subdev_pad_ops ov680_pad_ops = {
	.enum_mbus_code	 = ov680_enum_mbus_code,
	.enum_frame_size = ov680_enum_frame_size,
	.get_fmt	 = ov680_get_pad_format,
	.set_fmt	 = ov680_set_pad_format,
};

static const struct v4l2_subdev_ops ov680_ops = {
	.core		= &ov680_core_ops,
	.pad		= &ov680_pad_ops,
	.video		= &ov680_video_ops,
	.sensor		= &ov680_sensor_ops
};

static int ov680_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov680_device *dev = to_ov680_device(sd);

	release_firmware(dev->fw);

	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	media_entity_cleanup(&dev->sd.entity);
	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	mutex_destroy(&dev->input_lock);

	return 0;
}

static int ov680_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct ov680_device *dev;
	int ret;
	unsigned int i;

	const struct ov680_firmware *ov680_fw_header;
	unsigned int ov680_fw_data_size;

	dev_info(&client->dev, "ov680 probe called.\n");

	/* allocate device & init sub device */
	dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	/* Request firmware */
	ret = request_firmware(&dev->fw, "ov680_fw.bin", &client->dev);
	if (ret) {
		dev_err(&client->dev,
			"Requesting ov680_fw.bin failed, ret=%d.\n", ret);
		goto out_free_dev;
	}

	if (!dev->fw) {
		ret = -EINVAL;
		dev_err(&client->dev,
			"No firmware, ret=%d.\n", ret);
		goto out_free_dev;
	}

	ov680_fw_header = (const struct ov680_firmware *)dev->fw->data;
	ov680_fw_data_size = ov680_fw_header->cmd_count *
				ov680_fw_header->cmd_size +
				sizeof(u16);

	/* Check firmware size: FW header size + FW data size */
	if (dev->fw->size != (sizeof(*ov680_fw_header)+ov680_fw_data_size)) {
		dev_err(&client->dev,
			"Firmware size does not match: %lu<->%lu.\n",
			dev->fw->size,
			sizeof(*ov680_fw_header)+ov680_fw_data_size);
		ret = -EINVAL;
		goto out_free_dev;
	}

	/* Save firmware address */
	dev->fw_index = 0;
	dev->ov680_fw = (const struct ov680_reg *)&(ov680_fw_header[1]);

	mutex_init(&dev->input_lock);

	v4l2_i2c_subdev_init(&(dev->sd), client, &ov680_ops);

	if (client->dev.platform_data) {
		ret = ov680_s_config(&dev->sd, client->dev.platform_data);
		if (ret) {
			dev_dbg(&client->dev, "s_config failed\n");
			goto out_free;
		}
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = v4l2_ctrl_handler_init(&dev->ctrl_handler, ARRAY_SIZE(ctrls));
	if (ret) {
		dev_dbg(&client->dev, "v4l2_ctrl_handler_init failed\n");
		ov680_remove(client);
		goto out_free;
	}

	for (i = 0; i < ARRAY_SIZE(ctrls); i++)
		v4l2_ctrl_new_custom(&dev->ctrl_handler, &ctrls[i], NULL);

	if (dev->ctrl_handler.error) {
		dev_dbg(&client->dev, "%s: ctrl_handler error\n", __func__);
		ov680_remove(client);
		ret = dev->ctrl_handler.error;
		goto out_free;
	}

	/* Use same lock for controls as for everything else. */
	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = &dev->ctrl_handler;
	v4l2_ctrl_handler_setup(&dev->ctrl_handler);

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		dev_dbg(&client->dev, "media_entity_init failed\n");
		ov680_remove(client);
	}
	dev_dbg(&client->dev, "%s - driver load done\n", __func__);

	dev->probed = true;
	return ret;

out_free:
	release_firmware(dev->fw);
	v4l2_device_unregister_subdev(&dev->sd);
	mutex_destroy(&dev->input_lock);
out_free_dev:
	return ret;
}

static const struct i2c_device_id ov680_id[] = {
	{OV680_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ov680_id);

static struct i2c_driver ov680_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = OV680_NAME,
	},
	.probe = ov680_probe,
	.remove = ov680_remove,
	.id_table = ov680_id,
};

module_i2c_driver(ov680_driver);

MODULE_DESCRIPTION("OV680 4 Channel MIPI Bridge Controller Driver");
MODULE_AUTHOR("Xiaolin Zhang<xiaolin.zhang@intel.com>");
MODULE_LICENSE("GPL");

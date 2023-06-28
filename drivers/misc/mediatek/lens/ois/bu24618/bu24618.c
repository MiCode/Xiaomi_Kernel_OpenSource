// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "../xiaomi_common/ois_core.h"
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#define DRIVER_NAME "bu24618"

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define BU24618_NAME				"bu24618"

#define BU24618_CTRL_DELAY_US			5000

/* bu24618 device structure */
struct bu24618_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
	struct v4l2_ctrl *focus;
	struct regulator *vin;
	struct regulator *vdd;
	struct pinctrl *vcamaf_pinctrl;
	struct pinctrl_state *vcamaf_on;
	struct pinctrl_state *vcamaf_off;
};

struct mtk_ois_pos_info {
	struct OisInfo *p_ois_info;
};

/* Control commnad */
#define VIDIOC_MTK_S_OIS_MODE _IOW('V', BASE_VIDIOC_PRIVATE + 2, int32_t)

#define VIDIOC_MTK_G_OIS_POS_INFO _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct mtk_ois_pos_info)

#define VIDIOC_MTK_S_GYRO_OFFSET _IOW('V', BASE_VIDIOC_PRIVATE + 4, struct ois_gyro_offset)

#define VIDIOC_MTK_S_OIS_DRIFT _IOW('V', BASE_VIDIOC_PRIVATE + 5, struct ois_drift)

static inline struct bu24618_device *to_bu24618_ois(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct bu24618_device, ctrls);
}

static inline struct bu24618_device *sd_to_bu24618_ois(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct bu24618_device, sd);
}

static struct ois_driver_info g_bu24618_driver_info = {
	.ois_name    = "daumier_bu24618",
	.prog_addr   = 0x0000,
	.coeff_addr  = 0x1B40,
	.mem_addr    = 0x4000,
	.cali_addr   = 0x1DC0,
	.cali_offset = 0x20F2,
	.cali_size   = 38,
	.eeprom_addr = 0xA0,
	.addr_type   = OIS_I2C_TYPE_WORD,
	.data_type   = OIS_I2C_TYPE_BYTE,
	.client      = NULL,
	.task_thread = NULL,
};

static bool ois_streamon = false;

#ifdef CONFIG_FACTORY_BUILD
/* factory setting */
struct ois_setting bu24618_init_setting[] = {
	{0x614F, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6020, 0x01, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x617D, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6023, 0x02, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x602C, 0x70, 0, OP_WRITE},
	{0x602D, 0x21, 0, OP_WRITE},
	{0x602C, 0x71, 0, OP_WRITE},
	{0x602D, 0x00, 0, OP_WRITE},
	{0x602C, 0x72, 0, OP_WRITE},
	{0x602D, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x602A, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6023, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
};

struct ois_setting bu24618_enable_setting[] = {
	{0x6021, 0x03, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6025, 0xD0, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6020, 0x02, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
};
#else
/* miui setting */
struct ois_setting bu24618_init_setting[] = {
	{0x614F, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6020, 0x01, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x617C, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x617D, 0x0C, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x617E, 0x00, 0, OP_WRITE},
	{0x617F, 0x48, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6023, 0x02, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x602C, 0x70, 0, OP_WRITE},
	{0x602D, 0x21, 0, OP_WRITE},
	{0x602C, 0x71, 0, OP_WRITE},
	{0x602D, 0x00, 0, OP_WRITE},
	{0x602C, 0x72, 0, OP_WRITE},
	{0x602D, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x602A, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6023, 0x00, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
};

struct ois_setting bu24618_enable_setting[] = {
	{0x6021, 0x61, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x618E, 0x38, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6025, 0xD0, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6020, 0x02, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
	{0x6157, 0x09, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
};
#endif


struct ois_setting bu24618_disable_setting[] = {
	{0x6020, 0x01, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
};

struct ois_setting bu24618_centeron_setting[] = {
	{0x6020, 0x01, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
};

struct ois_setting bu24618_centeroff_setting[] = {
	{0x6020, 0x02, 0, OP_WRITE},
	{0x6024, 0x01, 0, OP_POLL },
};

static int bu24618_release(struct bu24618_device *bu24618)
{
	ois_streamon = false;
	return 0;
}

static int do_task(void *data)
{
	struct ois_driver_info *info = (struct ois_driver_info *)data;
	u8 buf[52];
	int i = 0;
	int j = 0;
	u64 pre_timestamps  = 0;
	u64 last_timestamps = 0;
	u64 delay_time      = 0;
	int delay_count     = 0;
	int32_t samples;
	int32_t x_shifts[16];
	int32_t y_shifts[16];
	int64_t timestamps[16];
	short x = 1, y = 1;
	u8    m = 0, l = 0;
	const u64 nanoSecondsPerSecond = 1000000000;
	const u32 oisClockRate         = 56600;   //< OIS output one hall position 56.25KHz
	const u32 oisHallStoreRate     = 1961200; //< based on your ois store sample data rate 1.9612ms(regitervalue = OisClockRate *  HallStoreRate)
	u64 tickTimestep               = nanoSecondsPerSecond / oisClockRate;
	u64 sampletimeDiff             = 0;
	int expectDataCnt      = 0;
	int unexpectDiffCnt    = 0;
	int preUnexpectDiffCnt = 0;
	u64 avgTime            = 0;

	while(1) {
		wait_event_interruptible(info->wq, ois_streamon);
		usleep_range(6000, 6010);

		ois_i2c_rd_p8(info->client, info->client->addr, 0x6200, buf, sizeof(buf));
		last_timestamps = ktime_get_boottime_ns();

		/* format ois data */
		i = 0;
		samples = buf[i] & 0xF;
		if (samples > 12) {
			LOG_INF("samples is an invalid value\n");
			continue;
		}
		i++;
		for (j = 0; j < samples; j++) {

            m = buf[i];
            l = buf[i + 1];
            x = ((u16)m << 8) + l;
            if (1 == (m >> 7)) {
                x -= 1;
                x = ~x;
                x *= -1;
            }

            m = buf[i + 2];
            l = buf[i + 3];
            y = ((u16)m << 8) + l;
            if (1 == (m >> 7)) {
                y -= 1;
                y = ~y;
                y *= -1;
            }

			x_shifts[j] = x;
			y_shifts[j] = y;

			i += 4;
		}

		delay_count = ((u16)buf[i] << 8) | buf[i+1];
		if (delay_count > 112) {
			delay_count = 0;
		}
		delay_time = tickTimestep * delay_count;
		i += 2;
		last_timestamps = last_timestamps - delay_time;

		for (j = 0; j < samples; j++) {
			sampletimeDiff = (samples - j - 1) * oisHallStoreRate;
			timestamps[j] = last_timestamps - sampletimeDiff;
		}

		expectDataCnt = 0;
		if (last_timestamps > pre_timestamps) {
			if (((last_timestamps - pre_timestamps) % oisHallStoreRate) > (oisHallStoreRate / 2)) {
				expectDataCnt = ((last_timestamps - pre_timestamps) / oisHallStoreRate) + 1;
			} else {
				expectDataCnt = ((last_timestamps - pre_timestamps) / oisHallStoreRate);
			}
		}
		unexpectDiffCnt = expectDataCnt - samples;
		if ((samples > 0) && (pre_timestamps != 0)) {
			if((preUnexpectDiffCnt != unexpectDiffCnt) && (expectDataCnt != samples)) {
				last_timestamps = pre_timestamps + samples * oisHallStoreRate;
				avgTime = (last_timestamps - pre_timestamps) / samples;
				for(j = 0; j < samples; j++) {
					timestamps[j] = last_timestamps - ((samples - j - 1) * avgTime);
				}
			}
			if (timestamps[0] <= (pre_timestamps + oisHallStoreRate / 8)) {
				if (last_timestamps < pre_timestamps) {
					last_timestamps = pre_timestamps;
				}
				avgTime = (last_timestamps - pre_timestamps) / samples;
				for(j = 0; j < samples; j++) {
					timestamps[j] = last_timestamps - ((samples - j - 1) * avgTime);
				}
			}
		}

		pre_timestamps	   = last_timestamps;
		preUnexpectDiffCnt = unexpectDiffCnt;

		for(j = 0; j < samples; j++) {
			set_ois_data(info, timestamps[j], x_shifts[j], y_shifts[j]);
		}

	}

	LOG_INF("exit task-queue thread!\n");
	return 0;
}

static int bu24618_init(struct bu24618_device *bu24618)
{
	struct i2c_client *client = v4l2_get_subdevdata(&bu24618->sd);
	u32 data_32 = 0;
	u32 sum     = 0;
	u32 i       = 0;

	reset_ois_data(&g_bu24618_driver_info);

	for (i = 0; i < 3; i++) {
		/* start DL */
		ois_i2c_wr_u8(client, client->addr, 0xF010, 00);
		usleep_range(200, 210);

		/* DL prog coeff mem */
		g_bu24618_driver_info.client = client;
		sum = default_ois_fw_download(&g_bu24618_driver_info);

		/* check sum */
		ois_i2c_rd_u32(client, client->addr, 0xF008, &data_32);
		LOG_INF("check sum = 0x%x\n", data_32);
		LOG_INF("fw sum = 0x%x\n", sum);
		if (data_32 == sum) {
			break;
		}
		usleep_range(1000, 1010);
	}

#ifdef CONFIG_FACTORY_BUILD
	LOG_INF("is factory\n");
#endif
	return 0;
}

/* Power handling */
static int bu24618_power_off(struct bu24618_device *bu24618)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = bu24618_release(bu24618);
	if (ret)
		LOG_INF("bu24618 release failed!\n");

	ret = regulator_disable(bu24618->vin);
	if (ret)
		return ret;

	ret = regulator_disable(bu24618->vdd);
	if (ret)
		return ret;

	if (bu24618->vcamaf_pinctrl && bu24618->vcamaf_off)
		ret = pinctrl_select_state(bu24618->vcamaf_pinctrl,
					bu24618->vcamaf_off);

	return ret;
}

static int bu24618_power_on(struct bu24618_device *bu24618)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = regulator_enable(bu24618->vin);
	if (ret < 0)
		return ret;

	ret = regulator_enable(bu24618->vdd);
	if (ret < 0)
		return ret;

	if (bu24618->vcamaf_pinctrl && bu24618->vcamaf_on)
		ret = pinctrl_select_state(bu24618->vcamaf_pinctrl,
					bu24618->vcamaf_on);

	if (ret < 0)
		return ret;

	/*
	 * TODO(b/139784289): Confirm hardware requirements and adjust/remove
	 * the delay.
	 */
	usleep_range(BU24618_CTRL_DELAY_US, BU24618_CTRL_DELAY_US + 100);

	ret = bu24618_init(bu24618);
	if (ret < 0)
		goto fail;

	return 0;

fail:
	regulator_disable(bu24618->vin);
	regulator_disable(bu24618->vdd);
	if (bu24618->vcamaf_pinctrl && bu24618->vcamaf_off) {
		pinctrl_select_state(bu24618->vcamaf_pinctrl,
				bu24618->vcamaf_off);
	}

	return ret;
}

static int bu24618_set_ctrl(struct v4l2_ctrl *ctrl)
{
	/* struct bu24618_device *bu24618 = to_bu24618_ois(ctrl); */

	return 0;
}

static const struct v4l2_ctrl_ops bu24618_ois_ctrl_ops = {
	.s_ctrl = bu24618_set_ctrl,
};

static int bu24618_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	LOG_INF("%s\n", __func__);

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	return 0;
}

static int bu24618_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	LOG_INF("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static long bu24618_ops_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;

	switch (cmd) {

	case VIDIOC_MTK_S_OIS_MODE:
	{
		int *ois_mode = arg;

		switch(*ois_mode) {
		case EnableOIS:
		case Still:
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Enable +\n");
			ois_write_setting(&g_bu24618_driver_info, bu24618_enable_setting,
				sizeof(bu24618_enable_setting) / sizeof(struct ois_setting));
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Enable -\n");
			break;
		case DisenableOIS:
		case CenteringOn:
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Disable +\n");
			ois_write_setting(&g_bu24618_driver_info, bu24618_enable_setting,
				sizeof(bu24618_enable_setting) / sizeof(struct ois_setting));

			ois_write_setting(&g_bu24618_driver_info, bu24618_disable_setting,
				sizeof(bu24618_disable_setting) / sizeof(struct ois_setting));
			LOG_INF("VIDIOC_MTK_S_OIS_MODE Disable -\n");
			break;
		default:
			break;
		}
		ois_streamon = true;
		wake_up_interruptible(&g_bu24618_driver_info.wq);
	}
	break;

	case VIDIOC_MTK_G_OIS_POS_INFO:
	{
		struct mtk_ois_pos_info *info = arg;
		struct OisInfo pos_info;

		pos_info.samples = get_ois_data(&g_bu24618_driver_info, pos_info.timestamps,
										pos_info.x_shifts, pos_info.y_shifts);

		if (copy_to_user((void *)info->p_ois_info, &pos_info, sizeof(pos_info)))
			ret = -EFAULT;
	}
	break;

	case VIDIOC_MTK_S_GYRO_OFFSET:
	{
		struct ois_gyro_offset *gyro_offset = arg;

		LOG_INF("OISGyroOffsetX =0x%04x\n", gyro_offset->OISGyroOffsetX);
		LOG_INF("OISGyroOffsetY =0x%04x\n", gyro_offset->OISGyroOffsetY);
		LOG_INF("OISGyroOffsetZ =0x%04x\n", gyro_offset->OISGyroOffsetZ);

		/* DL ois cali data */
		update_ois_cali(&g_bu24618_driver_info, gyro_offset);

		/* finish DL */
		ois_i2c_wr_u8(g_bu24618_driver_info.client, g_bu24618_driver_info.client->addr, 0xF006, 00);
		usleep_range(1000, 1010);

		/* check status */
		ret = ois_i2c_poll_u8(g_bu24618_driver_info.client, g_bu24618_driver_info.client->addr, 0x6024, 1);
		LOG_INF("check status : %d\n", ret);

		/* write init setting */
		ois_write_setting(&g_bu24618_driver_info, bu24618_init_setting,
			sizeof(bu24618_init_setting) / sizeof(struct ois_setting));
	}
	break;

	case VIDIOC_MTK_S_OIS_DRIFT:
	{
		struct ois_drift *drift = arg;

		// OIS shift X
		ois_i2c_wr_u16(g_bu24618_driver_info.client, g_bu24618_driver_info.client->addr, 0x6170, drift->OISDriftX);

		// OIS shift Y
		ois_i2c_wr_u16(g_bu24618_driver_info.client, g_bu24618_driver_info.client->addr, 0x6172, drift->OISDriftY);
		LOG_INF("OISDrift = %d:%d\n", drift->OISDriftX, drift->OISDriftY);
	}
	break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_internal_ops bu24618_int_ops = {
	.open = bu24618_open,
	.close = bu24618_close,
};

static struct v4l2_subdev_core_ops bu24618_ops_core = {
	.ioctl = bu24618_ops_core_ioctl,
};

static const struct v4l2_subdev_ops bu24618_ops = {
	.core = &bu24618_ops_core,
};

static void bu24618_subdev_cleanup(struct bu24618_device *bu24618)
{
	v4l2_async_unregister_subdev(&bu24618->sd);
	v4l2_ctrl_handler_free(&bu24618->ctrls);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&bu24618->sd.entity);
#endif
}

static int bu24618_init_controls(struct bu24618_device *bu24618)
{
	struct v4l2_ctrl_handler *hdl = &bu24618->ctrls;
	/* const struct v4l2_ctrl_ops *ops = &bu24618_ois_ctrl_ops; */

	v4l2_ctrl_handler_init(hdl, 1);

	if (hdl->error)
		return hdl->error;

	bu24618->sd.ctrl_handler = hdl;

	mutex_init(&g_bu24618_driver_info.mutex);
	init_waitqueue_head(&g_bu24618_driver_info.wq);

	if (!g_bu24618_driver_info.task_thread) {
		g_bu24618_driver_info.task_thread = kthread_run(do_task, &g_bu24618_driver_info, "bu24618-ois");
		if (!g_bu24618_driver_info.task_thread) {
			pr_err("Failed to create kthread : bu24618-ois\n");
		}
	}

	return 0;
}

static int bu24618_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct bu24618_device *bu24618;
	int ret;

	LOG_INF("%s\n", __func__);

	bu24618 = devm_kzalloc(dev, sizeof(*bu24618), GFP_KERNEL);
	if (!bu24618)
		return -ENOMEM;

	bu24618->vin = devm_regulator_get(dev, "vin");
	if (IS_ERR(bu24618->vin)) {
		ret = PTR_ERR(bu24618->vin);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vin regulator\n");
		return ret;
	}

	bu24618->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(bu24618->vdd)) {
		ret = PTR_ERR(bu24618->vdd);
		if (ret != -EPROBE_DEFER)
			LOG_INF("cannot get vdd regulator\n");
		return ret;
	}

	bu24618->vcamaf_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(bu24618->vcamaf_pinctrl)) {
		ret = PTR_ERR(bu24618->vcamaf_pinctrl);
		bu24618->vcamaf_pinctrl = NULL;
		LOG_INF("cannot get pinctrl\n");
	} else {
		bu24618->vcamaf_on = pinctrl_lookup_state(
			bu24618->vcamaf_pinctrl, "vcamaf_on");

		if (IS_ERR(bu24618->vcamaf_on)) {
			ret = PTR_ERR(bu24618->vcamaf_on);
			bu24618->vcamaf_on = NULL;
			LOG_INF("cannot get vcamaf_on pinctrl\n");
		}

		bu24618->vcamaf_off = pinctrl_lookup_state(
			bu24618->vcamaf_pinctrl, "vcamaf_off");

		if (IS_ERR(bu24618->vcamaf_off)) {
			ret = PTR_ERR(bu24618->vcamaf_off);
			bu24618->vcamaf_off = NULL;
			LOG_INF("cannot get vcamaf_off pinctrl\n");
		}
	}

	v4l2_i2c_subdev_init(&bu24618->sd, client, &bu24618_ops);
	bu24618->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	bu24618->sd.internal_ops = &bu24618_int_ops;

	ret = bu24618_init_controls(bu24618);
	if (ret)
		goto err_cleanup;

#if defined(CONFIG_MEDIA_CONTROLLER)
	ret = media_entity_pads_init(&bu24618->sd.entity, 0, NULL);
	if (ret < 0)
		goto err_cleanup;

	bu24618->sd.entity.function = MEDIA_ENT_F_LENS;
#endif

	ret = v4l2_async_register_subdev(&bu24618->sd);
	if (ret < 0)
		goto err_cleanup;

	pm_runtime_enable(dev);

	return 0;

err_cleanup:
	bu24618_subdev_cleanup(bu24618);
	return ret;
}

static int bu24618_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bu24618_device *bu24618 = sd_to_bu24618_ois(sd);

	LOG_INF("%s\n", __func__);

	bu24618_subdev_cleanup(bu24618);
	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		bu24618_power_off(bu24618);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

static int __maybe_unused bu24618_ois_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bu24618_device *bu24618 = sd_to_bu24618_ois(sd);

	return bu24618_power_off(bu24618);
}

static int __maybe_unused bu24618_ois_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bu24618_device *bu24618 = sd_to_bu24618_ois(sd);

	return bu24618_power_on(bu24618);
}

static const struct i2c_device_id bu24618_id_table[] = {
	{ BU24618_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bu24618_id_table);

static const struct of_device_id bu24618_of_table[] = {
	{ .compatible = "mediatek,bu24618" },
	{ },
};
MODULE_DEVICE_TABLE(of, bu24618_of_table);

static const struct dev_pm_ops bu24618_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(bu24618_ois_suspend, bu24618_ois_resume, NULL)
};

static struct i2c_driver bu24618_i2c_driver = {
	.driver = {
		.name = BU24618_NAME,
		.pm = &bu24618_pm_ops,
		.of_match_table = bu24618_of_table,
	},
	.probe_new  = bu24618_probe,
	.remove = bu24618_remove,
	.id_table = bu24618_id_table,
};

module_i2c_driver(bu24618_i2c_driver);

MODULE_AUTHOR("Sam Hung <Sam.Hung@mediatek.com>");
MODULE_DESCRIPTION("BU24618 VCM driver");
MODULE_LICENSE("GPL v2");

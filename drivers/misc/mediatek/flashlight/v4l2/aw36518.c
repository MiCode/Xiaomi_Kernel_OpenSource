// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/videodev2.h>
#include <linux/pinctrl/consumer.h>
#include <media/v4l2-subdev.h>
// #include <aw36518.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <linux/pm_runtime.h>
#include <linux/thermal.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>


#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"

#include <linux/power_supply.h>
#endif


#define AW36518_NAME	"aw36518"
#define AW36518_I2C_ADDR	(0x63)


/* registers definitions */
#define REG_CHIPID      0x00
#define REG_ENABLE		0x01
#define REG_LED0_FLASH_BR	0x03
#define REG_LED1_FLASH_BR	0x03
#define REG_LED0_TORCH_BR	0x05
#define REG_LED1_TORCH_BR	0x05
#define REG_FLASH_TOUT		0x08
#define REG_FLAG1		0x0A
#define REG_FLAG2		0x0B

/* fault mask */
#define FAULT_TIMEOUT	(1<<0)
#define FAULT_THERMAL_SHUTDOWN	(1<<2)
#define FAULT_LED0_SHORT_CIRCUIT	(1<<4)
#define FAULT_LED1_SHORT_CIRCUIT	(1<<4)

/*  FLASH Brightness
 *	min 2940uA, step 5870uA, max 1499790uA
 */
#define AW36518_FLASH_BRT_MIN 2940
#define AW36518_FLASH_BRT_STEP 5880
#define AW36518_FLASH_BRT_MAX 1472940
#define AW36518_FLASH_BRT_uA_TO_REG(a)	\
	((a) < AW36518_FLASH_BRT_MIN ? 0 :	\
	 (((a) - AW36518_FLASH_BRT_MIN) / AW36518_FLASH_BRT_STEP))
#define AW36518_FLASH_BRT_REG_TO_uA(a)		\
	((a) * AW36518_FLASH_BRT_STEP + AW36518_FLASH_BRT_MIN)

/*  FLASH TIMEOUT DURATION
 *	min 40ms, step 40ms, max 1600ms
 */
#define AW36518_FLASH_TOUT_MIN 40
#define AW36518_FLASH_TOUT_STEP 40
#define AW36518_FLASH_TOUT_MAX 1600

/*  TORCH BRT
 *	min 750uA, step 1510uA, max 386000uA
 */
#define AW36518_TORCH_BRT_MIN 750
#define AW36518_TORCH_BRT_STEP 1510
#define AW36518_TORCH_BRT_MAX 386000
#define AW36518_TORCH_BRT_uA_TO_REG(a)	\
	((a) < AW36518_TORCH_BRT_MIN ? 0 :	\
	 (((a) - AW36518_TORCH_BRT_MIN) / AW36518_TORCH_BRT_STEP))
#define AW36518_TORCH_BRT_REG_TO_uA(a)		\
	((a) * AW36518_TORCH_BRT_STEP + AW36518_TORCH_BRT_MIN)

#define AW36518_COOLER_MAX_STATE 5
static const int flash_state_to_current_limit[AW36518_COOLER_MAX_STATE] = {
	200000, 150000, 100000, 50000, 25000
};

enum aw36518_led_id {
	AW36518_LED0 = 0,
	AW36518_LED1,
	AW36518_LED_MAX
};

/* struct aw36518_platform_data
 *
 * @max_flash_timeout: flash timeout
 * @max_flash_brt: flash mode led brightness
 * @max_torch_brt: torch mode led brightness
 */
struct aw36518_platform_data {
	u32 max_flash_timeout;
	u32 max_flash_brt[AW36518_LED_MAX];
	u32 max_torch_brt[AW36518_LED_MAX];
};


enum led_enable {
	MODE_SHDN = 0x00,//flash by ext gpio
	MODE_TORCH = 0x08,
	MODE_FLASH = 0x0C,//torch by ext gpio
};

/**
 * struct aw36518_flash
 *
 * @dev: pointer to &struct device
 * @pdata: platform data
 * @regmap: reg. map for i2c
 * @lock: muxtex for serial access.
 * @led_mode: V4L2 LED mode
 * @ctrls_led: V4L2 controls
 * @subdev_led: V4L2 subdev
 */
struct aw36518_flash {
	struct device *dev;
	struct aw36518_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;

	enum v4l2_flash_led_mode led_mode;
	struct v4l2_ctrl_handler ctrls_led[AW36518_LED_MAX];
	struct v4l2_subdev subdev_led[AW36518_LED_MAX];
	struct device_node *dnode[AW36518_LED_MAX];
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id flash_dev_id[AW36518_LED_MAX];
#endif
	struct thermal_cooling_device *cdev;
	int need_cooler;
	unsigned long max_state;
	unsigned long target_state;
	unsigned long target_current;
	unsigned long ori_current;
	int flash_gpio;
};

/* define usage count */
static int use_count;

static struct aw36518_flash *aw36518_flash_data;

#define to_aw36518_flash(_ctrl, _no)	\
	container_of(_ctrl->handler, struct aw36518_flash, ctrls_led[_no])

/* define pinctrl */
#define AW36518_PINCTRL_PIN_HWEN 0
#define AW36518_PINCTRL_PINSTATE_LOW 0
#define AW36518_PINCTRL_PINSTATE_HIGH 1
#define AW36518_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define AW36518_PINCTRL_STATE_HWEN_LOW  "hwen_low"


/* enable mode control */
static int aw36518_mode_ctrl(struct aw36518_flash *flash)
{
	int rval = -EINVAL;

	pr_info("%s mode:%d", __func__, flash->led_mode);
	switch (flash->led_mode) {
	case V4L2_FLASH_LED_MODE_NONE:
		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x0C, MODE_SHDN);
		break;
	case V4L2_FLASH_LED_MODE_TORCH:
		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x0C, MODE_TORCH);
		break;
	case V4L2_FLASH_LED_MODE_FLASH:
		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x0C, MODE_FLASH);
		break;
	}
	return rval;
}

/* led1/2 enable/disable */
static int aw36518_enable_ctrl(struct aw36518_flash *flash,
			      enum aw36518_led_id led_no, bool on)
{
	int rval;

	pr_info("%s led:%d enable:%d", __func__, led_no, on);

	flashlight_kicker_pbm(on);
	if (flashlight_pt_is_low()) {
		pr_info("pt is low\n");
		return 0;
	}

	if (led_no == AW36518_LED0) {
		if (on)
			rval = regmap_update_bits(flash->regmap,
						  REG_ENABLE, 0x01, 0x01);
		else
			rval = regmap_update_bits(flash->regmap,
						  REG_ENABLE, 0x01, 0x00);
	} else {
		if (on)
			rval = regmap_update_bits(flash->regmap,
						  REG_ENABLE, 0x02, 0x02);
		else
			rval = regmap_update_bits(flash->regmap,
						  REG_ENABLE, 0x02, 0x00);
	}
	return rval;
}

/* torch1/2 brightness control */
static int aw36518_torch_brt_ctrl(struct aw36518_flash *flash,
				 enum aw36518_led_id led_no, unsigned int brt)
{
	int rval;
	u8 br_bits;

	pr_info("%s %d brt:%u\n", __func__, led_no, brt);
	if (brt < AW36518_TORCH_BRT_MIN)
		return aw36518_enable_ctrl(flash, led_no, false);

	if (flash->need_cooler == 0) {
		flash->ori_current = brt;
	} else {
		if (brt > flash->target_current) {
			brt = flash->target_current;
			pr_info("thermal limit current:%d\n", brt);
		}
	}

	br_bits = AW36518_TORCH_BRT_uA_TO_REG(brt);
	if (led_no == AW36518_LED0)
		rval = regmap_update_bits(flash->regmap,
					  REG_LED0_TORCH_BR, 0xFF, br_bits);
	else
		rval = regmap_update_bits(flash->regmap,
					  REG_LED1_TORCH_BR, 0xFF, br_bits);

	return rval;
}

/* flash1/2 brightness control */
static int aw36518_flash_brt_ctrl(struct aw36518_flash *flash,
				 enum aw36518_led_id led_no, unsigned int brt)
{
	int rval;
	u8 br_bits;

	pr_info("%s %d brt:%u", __func__, led_no, brt);
	if (brt < AW36518_FLASH_BRT_MIN)
		return aw36518_enable_ctrl(flash, led_no, false);

	if (flash->need_cooler == 1 && brt > flash->target_current) {
		brt = flash->target_current;
		pr_info("thermal limit current:%d\n", brt);
	}

	br_bits = AW36518_FLASH_BRT_uA_TO_REG(brt);
	if (led_no == AW36518_LED0)
		rval = regmap_update_bits(flash->regmap,
					  REG_LED0_FLASH_BR, 0xFF, br_bits);
	else
		rval = regmap_update_bits(flash->regmap,
					  REG_LED1_FLASH_BR, 0xFF, br_bits);

	return rval;
}

/* flash1/2 timeout control */
static int aw36518_flash_tout_ctrl(struct aw36518_flash *flash,
				unsigned int tout)
{
	int rval;
	u8 tout_bits;

	pr_info("%s tout:%u", __func__, tout);

	tout_bits = (tout / AW36518_FLASH_TOUT_STEP);

	rval = regmap_update_bits(flash->regmap,
				  REG_FLASH_TOUT, 0x0F, tout_bits);

	return rval;
}

/* v4l2 controls  */
static int aw36518_get_ctrl(struct v4l2_ctrl *ctrl, enum aw36518_led_id led_no)
{
	struct aw36518_flash *flash = to_aw36518_flash(ctrl, led_no);
	int rval = -EINVAL;

	mutex_lock(&flash->lock);

	if (ctrl->id == V4L2_CID_FLASH_FAULT) {
		s32 fault = 0;
		unsigned int reg_val = 0;

		rval = regmap_read(flash->regmap, REG_FLAG1, &reg_val);
		if (rval < 0)
			goto out;
		if (reg_val & FAULT_LED0_SHORT_CIRCUIT)
			fault |= V4L2_FLASH_FAULT_SHORT_CIRCUIT;
		if (reg_val & FAULT_LED1_SHORT_CIRCUIT)
			fault |= V4L2_FLASH_FAULT_SHORT_CIRCUIT;
		if (reg_val & FAULT_THERMAL_SHUTDOWN)
			fault |= V4L2_FLASH_FAULT_OVER_TEMPERATURE;
		if (reg_val & FAULT_TIMEOUT)
			fault |= V4L2_FLASH_FAULT_TIMEOUT;
		ctrl->cur.val = fault;
	}

out:
	mutex_unlock(&flash->lock);
	return rval;
}

static int aw36518_set_ctrl(struct v4l2_ctrl *ctrl, enum aw36518_led_id led_no)
{
	struct aw36518_flash *flash = to_aw36518_flash(ctrl, led_no);
	int rval = -EINVAL;

	pr_info("%s led:%d ID:%d", __func__, led_no, ctrl->id);
	mutex_lock(&flash->lock);

	switch (ctrl->id) {
	case V4L2_CID_FLASH_LED_MODE:
		pr_info("%s enter V4L2_CID_FLASH_LED_MODE");
		flash->led_mode = ctrl->val;
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			rval = aw36518_mode_ctrl(flash);
		else
			rval = 0;
		if (flash->led_mode == V4L2_FLASH_LED_MODE_NONE)
			aw36518_enable_ctrl(flash, led_no, false);
		else if (flash->led_mode == V4L2_FLASH_LED_MODE_TORCH)
			rval = aw36518_enable_ctrl(flash, led_no, true);
		break;

	case V4L2_CID_FLASH_STROBE_SOURCE:
		if (ctrl->val == V4L2_FLASH_STROBE_SOURCE_SOFTWARE) {
			pr_info("sw ctrl\n");
			rval = regmap_update_bits(flash->regmap,
					REG_ENABLE, 0x2C, 0x0C);
		} else if (ctrl->val == V4L2_FLASH_STROBE_SOURCE_EXTERNAL) {
			pr_info("hw trigger\n");
			rval = regmap_update_bits(flash->regmap,
					REG_ENABLE, 0x2C, 0x20);
			rval = aw36518_enable_ctrl(flash, led_no, true);
		}
		if (rval < 0)
			goto err_out;
		break;

	case V4L2_CID_FLASH_STROBE:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_FLASH;
		rval = aw36518_mode_ctrl(flash);
		rval = aw36518_enable_ctrl(flash, led_no, true);
		break;

	case V4L2_CID_FLASH_STROBE_STOP:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		aw36518_enable_ctrl(flash, led_no, false);
		flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
		rval = aw36518_mode_ctrl(flash);
		break;

	case V4L2_CID_FLASH_TIMEOUT:
		rval = aw36518_flash_tout_ctrl(flash, ctrl->val);
		break;

	case V4L2_CID_FLASH_INTENSITY:
		rval = aw36518_flash_brt_ctrl(flash, led_no, ctrl->val);
		break;

	case V4L2_CID_FLASH_TORCH_INTENSITY:
		rval = aw36518_torch_brt_ctrl(flash, led_no, ctrl->val);
		break;
	}

err_out:
	mutex_unlock(&flash->lock);
	return rval;
}

static int aw36518_led1_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return aw36518_get_ctrl(ctrl, AW36518_LED1);
}

static int aw36518_led1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return aw36518_set_ctrl(ctrl, AW36518_LED1);
}

static int aw36518_led0_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return aw36518_get_ctrl(ctrl, AW36518_LED0);
}

static int aw36518_led0_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return aw36518_set_ctrl(ctrl, AW36518_LED0);
}

static const struct v4l2_ctrl_ops aw36518_led_ctrl_ops[AW36518_LED_MAX] = {
	[AW36518_LED0] = {
			.g_volatile_ctrl = aw36518_led0_get_ctrl,
			.s_ctrl = aw36518_led0_set_ctrl,
			},
	[AW36518_LED1] = {
			.g_volatile_ctrl = aw36518_led1_get_ctrl,
			.s_ctrl = aw36518_led1_set_ctrl,
			}
};

static int aw36518_init_controls(struct aw36518_flash *flash,
				enum aw36518_led_id led_no)
{
	struct v4l2_ctrl *fault;
	u32 max_flash_brt = flash->pdata->max_flash_brt[led_no];
	u32 max_torch_brt = flash->pdata->max_torch_brt[led_no];
	struct v4l2_ctrl_handler *hdl = &flash->ctrls_led[led_no];
	const struct v4l2_ctrl_ops *ops = &aw36518_led_ctrl_ops[led_no];


	v4l2_ctrl_handler_init(hdl, 8);

	/* flash mode */
	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_FLASH_LED_MODE,
			       V4L2_FLASH_LED_MODE_TORCH, ~0x7,
			       V4L2_FLASH_LED_MODE_NONE);
	flash->led_mode = V4L2_FLASH_LED_MODE_NONE;

	/* flash source */
	v4l2_ctrl_new_std_menu(hdl, ops, V4L2_CID_FLASH_STROBE_SOURCE,
			       0x1, ~0x3, V4L2_FLASH_STROBE_SOURCE_SOFTWARE);

	/* flash strobe */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE, 0, 0, 0, 0);

	/* flash strobe stop */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_STROBE_STOP, 0, 0, 0, 0);

	/* flash strobe timeout */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TIMEOUT,
			  AW36518_FLASH_TOUT_MIN,
			  flash->pdata->max_flash_timeout,
			  AW36518_FLASH_TOUT_STEP,
			  flash->pdata->max_flash_timeout);

	/* flash brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_INTENSITY,
			  AW36518_FLASH_BRT_MIN, max_flash_brt,
			  AW36518_FLASH_BRT_STEP, max_flash_brt);

	/* torch brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TORCH_INTENSITY,
			  AW36518_TORCH_BRT_MIN, max_torch_brt,
			  AW36518_TORCH_BRT_STEP, max_torch_brt);

	/* fault */
	fault = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_FAULT, 0,
				  V4L2_FLASH_FAULT_OVER_VOLTAGE
				  | V4L2_FLASH_FAULT_OVER_TEMPERATURE
				  | V4L2_FLASH_FAULT_SHORT_CIRCUIT
				  | V4L2_FLASH_FAULT_TIMEOUT, 0, 0);
	if (fault != NULL)
		fault->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (hdl->error)
		return hdl->error;

	if (led_no < 0 || led_no >= AW36518_LED_MAX) {
		pr_info("led_no error\n");
		return -1;
	}

	flash->subdev_led[led_no].ctrl_handler = hdl;
	return 0;
}

/* initialize device */
static const struct v4l2_subdev_ops aw36518_ops = {
	.core = NULL,
};

static const struct regmap_config aw36518_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

static void aw36518_v4l2_i2c_subdev_init(struct v4l2_subdev *sd,
		struct i2c_client *client,
		const struct v4l2_subdev_ops *ops)
{
	int ret = 0;

	v4l2_subdev_init(sd, ops);
	sd->flags |= V4L2_SUBDEV_FL_IS_I2C;
	/* the owner is the same as the i2c_client's driver owner */
	sd->owner = client->dev.driver->owner;
	sd->dev = &client->dev;
	/* i2c_client and v4l2_subdev point to one another */
	v4l2_set_subdevdata(sd, client);
	i2c_set_clientdata(client, sd);
	/* initialize name */
	ret = snprintf(sd->name, sizeof(sd->name), "%s %d-%04x",
		client->dev.driver->name, i2c_adapter_id(client->adapter),
		client->addr);
	if (ret < 0)
		pr_info("snprintf failed\n");
}

static int aw36518_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = pm_runtime_get_sync(sd->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(sd->dev);
		return ret;
	}

	return 0;
}

static int aw36518_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops aw36518_int_ops = {
	.open = aw36518_open,
	.close = aw36518_close,
};

static int aw36518_subdev_init(struct aw36518_flash *flash,
			      enum aw36518_led_id led_no, char *led_name)
{
	struct i2c_client *client = to_i2c_client(flash->dev);
	struct device_node *np = flash->dev->of_node, *child;
	const char *fled_name = "flash";
	int rval;

	if (led_no < 0 || led_no >= AW36518_LED_MAX) {
		pr_info("led_no error\n");
		return -1;
	}

	pr_info("%s %d", __func__, led_no);

	aw36518_v4l2_i2c_subdev_init(&flash->subdev_led[led_no],
				client, &aw36518_ops);
	flash->subdev_led[led_no].flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	flash->subdev_led[led_no].internal_ops = &aw36518_int_ops;
	strscpy(flash->subdev_led[led_no].name, led_name,
		sizeof(flash->subdev_led[led_no].name));

	for (child = of_get_child_by_name(np, fled_name); child;
			child = of_find_node_by_name(child, fled_name)) {
		int rv;
		u32 reg = 0;

		rv = of_property_read_u32(child, "reg", &reg);
		if (rv)
			continue;

		if (reg == led_no) {
			flash->dnode[led_no] = child;
			flash->subdev_led[led_no].fwnode =
				of_fwnode_handle(flash->dnode[led_no]);
		}
	}

	rval = aw36518_init_controls(flash, led_no);
	if (rval)
		goto err_out;
	rval = media_entity_pads_init(&flash->subdev_led[led_no].entity, 0, NULL);
	if (rval < 0)
		goto err_out;
	flash->subdev_led[led_no].entity.function = MEDIA_ENT_F_FLASH;

	rval = v4l2_async_register_subdev(&flash->subdev_led[led_no]);
	if (rval < 0)
		goto err_out;

	return rval;

err_out:
	v4l2_ctrl_handler_free(&flash->ctrls_led[led_no]);
	return rval;
}

/* flashlight init */
static int aw36518_init(struct aw36518_flash *flash)
{
	int rval = 0;
	unsigned int reg_val;

	pr_info("%s", __func__);
	/* set timeout */
	rval = aw36518_flash_tout_ctrl(flash, 400);
	if (rval < 0)
		return rval;
	/* output disable */
	flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
	rval = aw36518_mode_ctrl(flash);
	if (rval < 0)
		return rval;

	rval = regmap_update_bits(flash->regmap,
				  REG_LED0_TORCH_BR, 0x80, 0x00);
	if (rval < 0)
		return rval;
	rval = regmap_update_bits(flash->regmap,
				  REG_LED0_FLASH_BR, 0x80, 0x00);
	if (rval < 0)
		return rval;
	/* reset faults */
	rval = regmap_read(flash->regmap, REG_FLAG1, &reg_val);
	return rval;
}

/* flashlight uninit */
static int aw36518_uninit(struct aw36518_flash *flash)
{
	return 0;
}

static int aw36518_flash_open(void)
{
	return 0;
}

static int aw36518_flash_release(void)
{
	return 0;
}

static int aw36518_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if ((int)fl_arg->arg) {
			aw36518_torch_brt_ctrl(aw36518_flash_data, channel, 25000);
			aw36518_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
			aw36518_mode_ctrl(aw36518_flash_data);
			aw36518_enable_ctrl(aw36518_flash_data, channel, true);
		} else {
			if (aw36518_flash_data->led_mode != V4L2_FLASH_LED_MODE_NONE) {
				aw36518_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
				aw36518_mode_ctrl(aw36518_flash_data);
				aw36518_enable_ctrl(aw36518_flash_data, channel, false);
			}
		}
		break;
	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int aw36518_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	//mutex_lock(&aw36518_mutex);
	if (set) {
		if (!use_count)
			ret = aw36518_init(aw36518_flash_data);
		use_count++;
		pr_info("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = aw36518_uninit(aw36518_flash_data);
		if (use_count < 0)
			use_count = 0;
		pr_info("Unset driver: %d\n", use_count);
	}
	//mutex_unlock(&aw36518_mutex);

	return 0;
}

static ssize_t aw36518_strobe_store(struct flashlight_arg arg)
{
	pr_info("%s %d", __func__);
	aw36518_set_driver(1);
	//aw36518_set_level(arg.channel, arg.level);
	//aw36518_timeout_ms[arg.channel] = 0;
	//aw36518_enable(arg.channel);
	aw36518_torch_brt_ctrl(aw36518_flash_data, arg.channel,
				arg.level * 25000);
	aw36518_enable_ctrl(aw36518_flash_data, arg.channel, true);
	aw36518_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
	aw36518_mode_ctrl(aw36518_flash_data);
	msleep(arg.dur);
	//aw36518_disable(arg.channel);
	aw36518_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
	aw36518_mode_ctrl(aw36518_flash_data);
	aw36518_enable_ctrl(aw36518_flash_data, arg.channel, false);
	aw36518_set_driver(0);
	return 0;
}

static int aw36518_cooling_get_max_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct aw36518_flash *flash = cdev->devdata;

	*state = flash->max_state;

	return 0;
}

static int aw36518_cooling_get_cur_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct aw36518_flash *flash = cdev->devdata;

	*state = flash->target_state;

	return 0;
}

static int aw36518_cooling_set_cur_state(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	struct aw36518_flash *flash = cdev->devdata;
	int ret = 0;

	/* Request state should be less than max_state */
	if (state > flash->max_state)
		state = flash->max_state;

	if (flash->target_state == state)
		return 0;

	flash->target_state = state;
	pr_info("set thermal current:%d\n", flash->target_state);

	if (flash->target_state == 0) {
		flash->need_cooler = 0;
		flash->target_current = AW36518_FLASH_BRT_MAX;
		ret = aw36518_torch_brt_ctrl(flash, AW36518_LED0,
					AW36518_TORCH_BRT_MAX);
		ret = aw36518_torch_brt_ctrl(flash, AW36518_LED1,
					AW36518_TORCH_BRT_MAX);
	} else {
		flash->need_cooler = 1;
		flash->target_current =
			flash_state_to_current_limit[flash->target_state - 1];
		ret = aw36518_torch_brt_ctrl(flash, AW36518_LED0,
						flash->target_current);
		ret = aw36518_torch_brt_ctrl(flash, AW36518_LED1,
						flash->target_current);
	}

	return ret;
}

static struct thermal_cooling_device_ops aw36518_cooling_ops = {
	.get_max_state		= aw36518_cooling_get_max_state,
	.get_cur_state		= aw36518_cooling_get_cur_state,
	.set_cur_state		= aw36518_cooling_set_cur_state,
};

static struct flashlight_operations aw36518_flash_ops = {
	aw36518_flash_open,
	aw36518_flash_release,
	aw36518_ioctl,
	aw36518_strobe_store,
	aw36518_set_driver
};

static int aw36518_parse_dt(struct aw36518_flash *flash)
{
	struct device_node *np, *cnp;
	struct device *dev = flash->dev;
	u32 decouple = 0;
	int i = 0;

	if (!dev || !dev->of_node)
		return -ENODEV;

	np = dev->of_node;
	for_each_child_of_node(np, cnp) {
		if (of_property_read_u32(cnp, "type",
					&flash->flash_dev_id[i].type))
			goto err_node_put;
		if (of_property_read_u32(cnp,
					"ct", &flash->flash_dev_id[i].ct))
			goto err_node_put;
		if (of_property_read_u32(cnp,
					"part", &flash->flash_dev_id[i].part))
			goto err_node_put;
		snprintf(flash->flash_dev_id[i].name, FLASHLIGHT_NAME_SIZE,
				flash->subdev_led[i].name);
		flash->flash_dev_id[i].channel = i;
		flash->flash_dev_id[i].decouple = decouple;

		pr_info("Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				flash->flash_dev_id[i].type,
				flash->flash_dev_id[i].ct,
				flash->flash_dev_id[i].part,
				flash->flash_dev_id[i].name,
				flash->flash_dev_id[i].channel,
				flash->flash_dev_id[i].decouple);
		if (flashlight_dev_register_by_device_id(&flash->flash_dev_id[i],
			&aw36518_flash_ops))
			return -EFAULT;
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int aw36518_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct aw36518_flash *flash = NULL;
	struct aw36518_platform_data *pdata = dev_get_platdata(&client->dev);
	int rval = 0;
	unsigned int reg_val = 0;

	pr_info("%s:%d", __func__, __LINE__);

	flash = devm_kzalloc(&client->dev, sizeof(*flash), GFP_KERNEL);
	if (flash == NULL)
		return -ENOMEM;

	flash->regmap = devm_regmap_init_i2c(client, &aw36518_regmap);
	if (IS_ERR(flash->regmap)) {
		rval = PTR_ERR(flash->regmap);
		return rval;
	}

	/* if there is no platform data, use chip default value */
	if (pdata == NULL) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL)
			return -ENODEV;
		pdata->max_flash_timeout = AW36518_FLASH_TOUT_MAX;
		/* led 1 */
		pdata->max_flash_brt[AW36518_LED0] = AW36518_FLASH_BRT_MAX;
		pdata->max_torch_brt[AW36518_LED0] = AW36518_TORCH_BRT_MAX;
		/* led 2 */
		pdata->max_flash_brt[AW36518_LED1] = AW36518_FLASH_BRT_MAX;
		pdata->max_torch_brt[AW36518_LED1] = AW36518_TORCH_BRT_MAX;
	}
	flash->pdata = pdata;
	flash->dev = &client->dev;
	mutex_init(&flash->lock);
	aw36518_flash_data = flash;

	rval = aw36518_subdev_init(flash, AW36518_LED0, "aw36518-led0");
	if (rval < 0)
		return rval;

	rval = aw36518_subdev_init(flash, AW36518_LED1, "aw36518-led1");
	if (rval < 0)
		return rval;

	pm_runtime_enable(flash->dev);

	rval = aw36518_parse_dt(flash);

	i2c_set_clientdata(client, flash);

	flash->max_state = AW36518_COOLER_MAX_STATE;
	flash->target_state = 0;
	flash->need_cooler = 0;
	flash->target_current = AW36518_FLASH_BRT_MAX;
	flash->ori_current = AW36518_TORCH_BRT_MAX;
	flash->cdev = thermal_of_cooling_device_register(client->dev.of_node,
			"flashlight_cooler", flash, &aw36518_cooling_ops);
	if (IS_ERR(flash->cdev))
		pr_info("register thermal failed\n");
	rval = regmap_read(flash->regmap, REG_CHIPID, &reg_val);
	pr_info("read chip id:%d\n",reg_val);
	pr_info("%s:%d", __func__, __LINE__);
	return 0;
}

static int aw36518_remove(struct i2c_client *client)
{
	struct aw36518_flash *flash = i2c_get_clientdata(client);
	unsigned int i;

	thermal_cooling_device_unregister(flash->cdev);
	for (i = AW36518_LED0; i < AW36518_LED_MAX; i++) {
		v4l2_device_unregister_subdev(&flash->subdev_led[i]);
		v4l2_ctrl_handler_free(&flash->ctrls_led[i]);
		media_entity_cleanup(&flash->subdev_led[i].entity);
	}

	pm_runtime_disable(&client->dev);

	pm_runtime_set_suspended(&client->dev);
	return 0;
}

static int __maybe_unused aw36518_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw36518_flash *flash = i2c_get_clientdata(client);

	pr_info("%s %d", __func__, __LINE__);

	return aw36518_uninit(flash);
}

static int __maybe_unused aw36518_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw36518_flash *flash = i2c_get_clientdata(client);

	pr_info("%s %d", __func__, __LINE__);

	return aw36518_init(flash);
}

static const struct i2c_device_id aw36518_id_table[] = {
	{AW36518_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, aw36518_id_table);

static const struct of_device_id aw36518_of_table[] = {
	{ .compatible = "mediatek,aw36518" },
	{ },
};
MODULE_DEVICE_TABLE(of, aw36518_of_table);

static const struct dev_pm_ops aw36518_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(aw36518_suspend, aw36518_resume, NULL)
};

static struct i2c_driver aw36518_i2c_driver = {
	.driver = {
		   .name = AW36518_NAME,
		   .pm = &aw36518_pm_ops,
		   .of_match_table = aw36518_of_table,
		   },
	.probe = aw36518_probe,
	.remove = aw36518_remove,
	.id_table = aw36518_id_table,
};

module_i2c_driver(aw36518_i2c_driver);

MODULE_AUTHOR("ot_xiaofeiw Wang <ot_xiaofeiw.wang@mediatek.com>");
MODULE_DESCRIPTION("Texas Instruments AW36518 LED flash driver");
MODULE_LICENSE("GPL");

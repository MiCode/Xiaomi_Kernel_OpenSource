// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.
// Copyright (C) 2022 XiaoMi, Inc.
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/videodev2.h>
#include <linux/pinctrl/consumer.h>
#include <media/v4l2-subdev.h>
// #include <ktd2687.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <linux/pm_runtime.h>

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"

#include <linux/power_supply.h>
#endif

#define KTD2687_NAME	"ktd2687"
#define KTD2687_I2C_ADDR	(0x63)

/* registers definitions */
#define REG_ENABLE		0x01
#define REG_LED0_FLASH_BR	0x03
#define REG_LED1_FLASH_BR	0x04
#define REG_LED0_TORCH_BR	0x05
#define REG_LED1_TORCH_BR	0x06
#define REG_FLASH_TOUT		0x08
#define REG_FLAG1		0x0A
#define REG_FLAG2		0x0B

/* fault mask */
#define FAULT_TIMEOUT	(1<<0)
#define FAULT_THERMAL_SHUTDOWN	(1<<2)
#define FAULT_LED0_SHORT_CIRCUIT	(1<<5)
#define FAULT_LED1_SHORT_CIRCUIT	(1<<4)

/*  FLASH Brightness
 *	min 11720uA, step 11718.75uA, max 1500000uA
 */
#define KTD2687_FLASH_BRT_MIN 11720
#define KTD2687_FLASH_BRT_STEP 11719
#define KTD2687_FLASH_BRT_MAX 1499975
#define KTD2687_FLASH_BRT_uA_TO_REG(a)	\
	((a) < KTD2687_FLASH_BRT_MIN ? 0 :	\
	 (((a) / KTD2687_FLASH_BRT_STEP) - 1))
#define KTD2687_FLASH_BRT_REG_TO_uA(a)		\
	(((a) + 1) * KTD2687_FLASH_BRT_STEP)

/*  FLASH TIMEOUT DURATION
 *	min 100ms, step 50ms, max 400ms
 */
#define KTD2687_FLASH_TOUT_MIN 100
#define KTD2687_FLASH_TOUT_STEP_1 10
#define KTD2687_FLASH_TOUT_STEP_2 50
#define KTD2687_FLASH_TOUT_STEP 50
#define KTD2687_FLASH_TOUT_MAX 400

/*  TORCH BRT
 *	min 1465uA, step 1464.84375uA, max 187500uA
 */
#define KTD2687_TORCH_BRT_MIN 1465
#define KTD2687_TORCH_BRT_STEP 1465
#define KTD2687_TORCH_BRT_MAX 187500
#define KTD2687_TORCH_BRT_uA_TO_REG(a)	\
	((a) < KTD2687_TORCH_BRT_MIN ? 0 :	\
	 (((a) / KTD2687_TORCH_BRT_STEP) - 1))
#define KTD2687_TORCH_BRT_REG_TO_uA(a)		\
	(((a) * 1) * KTD2687_TORCH_BRT_STEP)

enum ktd2687_led_id {
	KTD2687_LED0 = 0,
	KTD2687_LED1,
	KTD2687_LED_MAX
};

/* struct ktd2687_platform_data
 *
 * @max_flash_timeout: flash timeout
 * @max_flash_brt: flash mode led brightness
 * @max_torch_brt: torch mode led brightness
 */
struct ktd2687_platform_data {
	u32 max_flash_timeout;
	u32 max_flash_brt[KTD2687_LED_MAX];
	u32 max_torch_brt[KTD2687_LED_MAX];
};


enum led_enable {
	MODE_SHDN = 0x0,
	MODE_TORCH = 0x08,
	MODE_FLASH = 0x0C,
};

/**
 * struct ktd2687_flash
 *
 * @dev: pointer to &struct device
 * @pdata: platform data
 * @regmap: reg. map for i2c
 * @lock: muxtex for serial access.
 * @led_mode: V4L2 LED mode
 * @ctrls_led: V4L2 controls
 * @subdev_led: V4L2 subdev
 */
struct ktd2687_flash {
	struct device *dev;
	struct ktd2687_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;

	enum v4l2_flash_led_mode led_mode;
	struct v4l2_ctrl_handler ctrls_led[KTD2687_LED_MAX];
	struct v4l2_subdev subdev_led[KTD2687_LED_MAX];
	struct device_node *dnode[KTD2687_LED_MAX];
	struct pinctrl *ktd2687_hwen_pinctrl;
	struct pinctrl_state *ktd2687_hwen_high;
	struct pinctrl_state *ktd2687_hwen_low;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id flash_dev_id[KTD2687_LED_MAX];
#endif
};

/* define usage count */
static int use_count;

static struct ktd2687_flash *ktd2687_flash_data;

#define to_ktd2687_flash(_ctrl, _no)	\
	container_of(_ctrl->handler, struct ktd2687_flash, ctrls_led[_no])

/* define pinctrl */
#define KTD2687_PINCTRL_PIN_HWEN 0
#define KTD2687_PINCTRL_PINSTATE_LOW 0
#define KTD2687_PINCTRL_PINSTATE_HIGH 1
#define KTD2687_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define KTD2687_PINCTRL_STATE_HWEN_LOW  "hwen_low"
/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int ktd2687_pinctrl_init(struct ktd2687_flash *flash)
{
	int ret = 0;

	/* get pinctrl */
	flash->ktd2687_hwen_pinctrl = devm_pinctrl_get(flash->dev);
	if (IS_ERR(flash->ktd2687_hwen_pinctrl)) {
		pr_info("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(flash->ktd2687_hwen_pinctrl);
		return ret;
	}

	/* Flashlight HWEN pin initialization */
	flash->ktd2687_hwen_high = pinctrl_lookup_state(
			flash->ktd2687_hwen_pinctrl,
			KTD2687_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(flash->ktd2687_hwen_high)) {
		pr_info("Failed to init (%s)\n",
			KTD2687_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(flash->ktd2687_hwen_high);
	}
	flash->ktd2687_hwen_low = pinctrl_lookup_state(
			flash->ktd2687_hwen_pinctrl,
			KTD2687_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(flash->ktd2687_hwen_low)) {
		pr_info("Failed to init (%s)\n", KTD2687_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(flash->ktd2687_hwen_low);
	}

	return ret;
}

static int ktd2687_pinctrl_set(struct ktd2687_flash *flash, int pin, int state)
{
	int ret = 0;

	if (IS_ERR(flash->ktd2687_hwen_pinctrl)) {
		pr_info("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case KTD2687_PINCTRL_PIN_HWEN:
		if (state == KTD2687_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(flash->ktd2687_hwen_low))
			pinctrl_select_state(flash->ktd2687_hwen_pinctrl,
					flash->ktd2687_hwen_low);
		else if (state == KTD2687_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(flash->ktd2687_hwen_high))
			pinctrl_select_state(flash->ktd2687_hwen_pinctrl,
					flash->ktd2687_hwen_high);
		else
			pr_info("set err, pin(%d) state(%d)\n", pin, state);
		break;
	default:
		pr_info("set err, pin(%d) state(%d)\n", pin, state);
		break;
	}
	pr_info("pin(%d) state(%d)\n", pin, state);

	return ret;
}

/* enable mode control */
static int ktd2687_mode_ctrl(struct ktd2687_flash *flash)
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
static int ktd2687_enable_ctrl(struct ktd2687_flash *flash,
			      enum ktd2687_led_id led_no, bool on)
{
	int rval;

	pr_info("%s %d enable:%d", __func__, led_no, on);

	flashlight_kicker_pbm(on);
	if (flashlight_pt_is_low()) {
		pr_info("pt is low\n");
		return 0;
	}

	if (on)
		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x03, 0x03);
	else
		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x03, 0x00);
	return rval;
}

/* torch1/2 brightness control */
static int ktd2687_torch_brt_ctrl(struct ktd2687_flash *flash,
				 enum ktd2687_led_id led_no, unsigned int brt)
{
	int rval;
	u8 br_bits;
	int torch_cur_avg = 0;

	pr_info("%s %d brt:%u", __func__, led_no, brt);
	torch_cur_avg = brt / 2;

	br_bits = KTD2687_TORCH_BRT_uA_TO_REG(torch_cur_avg);

	pr_info("%s avg_brt:%u brt_bit :%x", __func__, torch_cur_avg, br_bits);

	rval = regmap_update_bits(flash->regmap,
				  REG_LED0_TORCH_BR, 0x7f, br_bits);
	rval = regmap_update_bits(flash->regmap,
				  REG_LED1_TORCH_BR, 0x7f, br_bits);

	return rval;
}

/* flash1/2 brightness control */
static int ktd2687_flash_brt_ctrl(struct ktd2687_flash *flash,
				 enum ktd2687_led_id led_no, unsigned int brt)
{
	int rval;
	u8 br_bits;
	int flash_cur_avg = 0;

	pr_info("%s %d brt:%u", __func__, led_no, brt);
	flash_cur_avg = brt / 2;

	br_bits = KTD2687_FLASH_BRT_uA_TO_REG(flash_cur_avg);

	pr_info("%s avg_brt:%u brt_bit :%x", __func__, flash_cur_avg, br_bits);

	rval = regmap_update_bits(flash->regmap,
				  REG_LED0_FLASH_BR, 0x7f, br_bits);
	rval = regmap_update_bits(flash->regmap,
				  REG_LED1_FLASH_BR, 0x7f, br_bits);

	return rval;
}

/* flash1/2 timeout control */
static int ktd2687_flash_tout_ctrl(struct ktd2687_flash *flash,
				unsigned int tout)
{
	int rval;
	u8 tout_bits;

	pr_info("%s flash tout:%d", __func__, tout);

	if (tout < 10 || tout > 400) {
			pr_info("Error arguments tout(%d)\n", tout);
			return -1;
	}

	if (tout <= 100)
		tout_bits = 0x00 + (tout / KTD2687_FLASH_TOUT_STEP_1 - 1);
	else
		tout_bits = 0x09 + (tout / KTD2687_FLASH_TOUT_STEP_2 - 2);

	rval = regmap_update_bits(flash->regmap,
			  REG_FLASH_TOUT, 0x1f, tout_bits);

	return rval;
}

/* v4l2 controls  */
static int ktd2687_get_ctrl(struct v4l2_ctrl *ctrl, enum ktd2687_led_id led_no)
{
	struct ktd2687_flash *flash = to_ktd2687_flash(ctrl, led_no);
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

static int ktd2687_set_ctrl(struct v4l2_ctrl *ctrl, enum ktd2687_led_id led_no)
{
	struct ktd2687_flash *flash = to_ktd2687_flash(ctrl, led_no);
	int rval = -EINVAL;

	pr_info("%s led:%d", __func__, led_no);
	mutex_lock(&flash->lock);

	switch (ctrl->id) {
	case V4L2_CID_FLASH_LED_MODE:
		flash->led_mode = ctrl->val;
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			rval = ktd2687_mode_ctrl(flash);
		else
			rval = 0;
		if (flash->led_mode == V4L2_FLASH_LED_MODE_NONE)
			ktd2687_enable_ctrl(flash, led_no, false);
		else if (flash->led_mode == V4L2_FLASH_LED_MODE_TORCH)
			rval = ktd2687_enable_ctrl(flash, led_no, true);
		break;

	case V4L2_CID_FLASH_STROBE_SOURCE:
		rval = regmap_update_bits(flash->regmap,
					  REG_ENABLE, 0x20, (ctrl->val) << 5);
		if (rval < 0)
			goto err_out;
		break;

	case V4L2_CID_FLASH_STROBE:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_FLASH;
		rval = ktd2687_mode_ctrl(flash);
		rval = ktd2687_enable_ctrl(flash, led_no, true);
		break;

	case V4L2_CID_FLASH_STROBE_STOP:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		ktd2687_enable_ctrl(flash, led_no, false);
		flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
		rval = ktd2687_mode_ctrl(flash);
		break;

	case V4L2_CID_FLASH_TIMEOUT:
		rval = ktd2687_flash_tout_ctrl(flash, ctrl->val);
		break;

	case V4L2_CID_FLASH_INTENSITY:
		rval = ktd2687_flash_brt_ctrl(flash, led_no, ctrl->val);
		break;

	case V4L2_CID_FLASH_TORCH_INTENSITY:
		rval = ktd2687_torch_brt_ctrl(flash, led_no, ctrl->val);
		break;
	}

err_out:
	mutex_unlock(&flash->lock);
	return rval;
}

static int ktd2687_led1_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return ktd2687_get_ctrl(ctrl, KTD2687_LED1);
}

static int ktd2687_led1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return ktd2687_set_ctrl(ctrl, KTD2687_LED1);
}

static int ktd2687_led0_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return ktd2687_get_ctrl(ctrl, KTD2687_LED0);
}

static int ktd2687_led0_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return ktd2687_set_ctrl(ctrl, KTD2687_LED0);
}

static const struct v4l2_ctrl_ops ktd2687_led_ctrl_ops[KTD2687_LED_MAX] = {
	[KTD2687_LED0] = {
			.g_volatile_ctrl = ktd2687_led0_get_ctrl,
			.s_ctrl = ktd2687_led0_set_ctrl,
			},
	[KTD2687_LED1] = {
			.g_volatile_ctrl = ktd2687_led1_get_ctrl,
			.s_ctrl = ktd2687_led1_set_ctrl,
			}
};

static int ktd2687_init_controls(struct ktd2687_flash *flash,
				enum ktd2687_led_id led_no)
{
	struct v4l2_ctrl *fault;
	u32 max_flash_brt = flash->pdata->max_flash_brt[led_no];
	u32 max_torch_brt = flash->pdata->max_torch_brt[led_no];
	struct v4l2_ctrl_handler *hdl = &flash->ctrls_led[led_no];
	const struct v4l2_ctrl_ops *ops = &ktd2687_led_ctrl_ops[led_no];

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
			  KTD2687_FLASH_TOUT_MIN,
			  flash->pdata->max_flash_timeout,
			  KTD2687_FLASH_TOUT_STEP,
			  flash->pdata->max_flash_timeout);

	/* flash brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_INTENSITY,
			  KTD2687_FLASH_BRT_MIN, max_flash_brt,
			  KTD2687_FLASH_BRT_STEP, max_flash_brt);

	/* torch brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TORCH_INTENSITY,
			  KTD2687_TORCH_BRT_MIN, max_torch_brt,
			  KTD2687_TORCH_BRT_STEP, max_torch_brt);

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

	flash->subdev_led[led_no].ctrl_handler = hdl;
	return 0;
}

/* initialize device */
static const struct v4l2_subdev_ops ktd2687_ops = {
	.core = NULL,
};

static const struct regmap_config ktd2687_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

static void ktd2687_v4l2_i2c_subdev_init(struct v4l2_subdev *sd,
		struct i2c_client *client,
		const struct v4l2_subdev_ops *ops)
{
	v4l2_subdev_init(sd, ops);
	sd->flags |= V4L2_SUBDEV_FL_IS_I2C;
	/* the owner is the same as the i2c_client's driver owner */
	sd->owner = client->dev.driver->owner;
	sd->dev = &client->dev;
	/* i2c_client and v4l2_subdev point to one another */
	v4l2_set_subdevdata(sd, client);
	i2c_set_clientdata(client, sd);
	/* initialize name */
	snprintf(sd->name, sizeof(sd->name), "%s %d-%04x",
		client->dev.driver->name, i2c_adapter_id(client->adapter),
		client->addr);
}

static int ktd2687_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
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

static int ktd2687_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops ktd2687_int_ops = {
	.open = ktd2687_open,
	.close = ktd2687_close,
};

static int ktd2687_subdev_init(struct ktd2687_flash *flash,
			      enum ktd2687_led_id led_no, char *led_name)
{
	struct i2c_client *client = to_i2c_client(flash->dev);
	struct device_node *np = flash->dev->of_node, *child;
	const char *fled_name = "flash";
	int rval;

	// pr_info("%s %d", __func__, led_no);

	ktd2687_v4l2_i2c_subdev_init(&flash->subdev_led[led_no],
				client, &ktd2687_ops);
	flash->subdev_led[led_no].flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	flash->subdev_led[led_no].internal_ops = &ktd2687_int_ops;
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

	rval = ktd2687_init_controls(flash, led_no);
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
static int ktd2687_init(struct ktd2687_flash *flash)
{
	int rval = 0;
	unsigned int reg_val;

	ktd2687_pinctrl_set(flash, KTD2687_PINCTRL_PIN_HWEN, KTD2687_PINCTRL_PINSTATE_HIGH);

	/* set timeout */
	rval = ktd2687_flash_tout_ctrl(flash, 400);
	if (rval < 0)
		return rval;
	/* output disable */
	flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
	rval = ktd2687_mode_ctrl(flash);
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

#ifdef XAGA_CAM
	use_count = 1;
#endif

	/* reset faults */
	rval = regmap_read(flash->regmap, REG_FLAG1, &reg_val);
	return rval;
}

/* flashlight uninit */
static int ktd2687_uninit(struct ktd2687_flash *flash)
{
	ktd2687_pinctrl_set(flash,
			KTD2687_PINCTRL_PIN_HWEN, KTD2687_PINCTRL_PINSTATE_LOW);

#ifdef XAGA_CAM
	use_count = 0;
#endif

	return 0;
}

static int ktd2687_flash_open(void)
{
	return 0;
}

static int ktd2687_flash_release(void)
{
	return 0;
}

static int ktd2687_ioctl(unsigned int cmd, unsigned long arg)
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
			ktd2687_torch_brt_ctrl(ktd2687_flash_data, channel, 25000);
			ktd2687_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
			ktd2687_mode_ctrl(ktd2687_flash_data);
			ktd2687_enable_ctrl(ktd2687_flash_data, channel, true);
		} else {
			ktd2687_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
			ktd2687_mode_ctrl(ktd2687_flash_data);
			ktd2687_enable_ctrl(ktd2687_flash_data, channel, false);
		}
		break;
	case XIAOMI_IOC_SET_ONOFF:
		pr_info("XIAOMI_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		if ((int)fl_arg->arg) {
			ktd2687_enable_ctrl(ktd2687_flash_data, channel, true);
		} else {
			ktd2687_enable_ctrl(ktd2687_flash_data, channel, false);
		}
		break;
	case XIAOMI_IOC_SET_FLASH_CUR:
		pr_info("XIAOMI_IOC_SET_FLASH_CUR(%d): %d\n",
				channel, (int)fl_arg->arg);
		ktd2687_flash_brt_ctrl(ktd2687_flash_data, channel, fl_arg->arg);
		break;
	case XIAOMI_IOC_SET_TORCH_CUR:
		pr_info("XIAOMI_IOC_SET_TORCH_CUR(%d): %d\n",
				channel, (int)fl_arg->arg);
		ktd2687_torch_brt_ctrl(ktd2687_flash_data, channel, fl_arg->arg);
		break;
	case XIAOMI_IOC_SET_MODE:
		pr_info("XIAOMI_IOC_SET_MODE(%d): %d\n",
				channel, (int)fl_arg->arg);
		ktd2687_flash_data->led_mode = fl_arg->arg;
		ktd2687_mode_ctrl(ktd2687_flash_data);
		break;
	case XIAOMI_IOC_SET_HW_TIMEOUT:
		pr_info("XIAOMI_IOC_SET_HW_TIMEOUT(%d): %d\n",
				channel, (int)fl_arg->arg);
		ktd2687_flash_tout_ctrl(ktd2687_flash_data, fl_arg->arg);
		break;
	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int ktd2687_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	//mutex_lock(&ktd2687_mutex);
	if (set) {
#ifdef XAGA_CAM
		if (!use_count) {
			ret = ktd2687_init(ktd2687_flash_data);
			pr_debug("Set driver: %d\n", use_count);
		}
#else
		if (!use_count)
			ret = ktd2687_init(ktd2687_flash_data);
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
#endif
	} else {
		use_count--;
		if (!use_count)
			ret = ktd2687_uninit(ktd2687_flash_data);
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	//mutex_unlock(&ktd2687_mutex);

	return 0;
}

static ssize_t ktd2687_strobe_store(struct flashlight_arg arg)
{
	ktd2687_set_driver(1);
	//ktd2687_set_level(arg.channel, arg.level);
	//ktd2687_timeout_ms[arg.channel] = 0;
	//ktd2687_enable(arg.channel);
	ktd2687_torch_brt_ctrl(ktd2687_flash_data, arg.channel,
				arg.level * 25000);
	ktd2687_enable_ctrl(ktd2687_flash_data, arg.channel, true);
	ktd2687_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
	ktd2687_mode_ctrl(ktd2687_flash_data);
	msleep(arg.dur);
	//ktd2687_disable(arg.channel);
	ktd2687_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
	ktd2687_mode_ctrl(ktd2687_flash_data);
	ktd2687_enable_ctrl(ktd2687_flash_data, arg.channel, false);
	ktd2687_set_driver(0);
	return 0;
}

static struct flashlight_operations ktd2687_flash_ops = {
	ktd2687_flash_open,
	ktd2687_flash_release,
	ktd2687_ioctl,
	ktd2687_strobe_store,
	ktd2687_set_driver
};

static int ktd2687_parse_dt(struct ktd2687_flash *flash)
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
			&ktd2687_flash_ops))
			return -EFAULT;
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int ktd2687_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct ktd2687_flash *flash;
	struct ktd2687_platform_data *pdata = dev_get_platdata(&client->dev);
	int rval;

	pr_info("%s:%d", __func__, __LINE__);

	flash = devm_kzalloc(&client->dev, sizeof(*flash), GFP_KERNEL);
	if (flash == NULL)
		return -ENOMEM;

	flash->regmap = devm_regmap_init_i2c(client, &ktd2687_regmap);
	if (IS_ERR(flash->regmap)) {
		rval = PTR_ERR(flash->regmap);
		return rval;
	}

	/* if there is no platform data, use chip default value */
	if (pdata == NULL) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL)
			return -ENODEV;
		pdata->max_flash_timeout = KTD2687_FLASH_TOUT_MAX;
		/* led 1 */
		pdata->max_flash_brt[KTD2687_LED0] = KTD2687_FLASH_BRT_MAX;
		pdata->max_torch_brt[KTD2687_LED0] = KTD2687_TORCH_BRT_MAX;
		/* led 2 */
		pdata->max_flash_brt[KTD2687_LED1] = KTD2687_FLASH_BRT_MAX;
		pdata->max_torch_brt[KTD2687_LED1] = KTD2687_TORCH_BRT_MAX;
	}
	flash->pdata = pdata;
	flash->dev = &client->dev;
	mutex_init(&flash->lock);
	ktd2687_flash_data = flash;

	rval = ktd2687_pinctrl_init(flash);
	if (rval < 0)
		return rval;

	rval = ktd2687_subdev_init(flash, KTD2687_LED0, "ktd2687-led0");
	if (rval < 0)
		return rval;

	rval = ktd2687_subdev_init(flash, KTD2687_LED1, "ktd2687-led1");
	if (rval < 0)
		return rval;

	pm_runtime_enable(flash->dev);

	rval = ktd2687_parse_dt(flash);

	i2c_set_clientdata(client, flash);

	pr_info("%s:%d", __func__, __LINE__);
	return 0;
}

static int ktd2687_remove(struct i2c_client *client)
{
	struct ktd2687_flash *flash = i2c_get_clientdata(client);
	unsigned int i;

	for (i = KTD2687_LED0; i < KTD2687_LED_MAX; i++) {
		v4l2_device_unregister_subdev(&flash->subdev_led[i]);
		v4l2_ctrl_handler_free(&flash->ctrls_led[i]);
		media_entity_cleanup(&flash->subdev_led[i].entity);
	}

	pm_runtime_disable(&client->dev);

	pm_runtime_set_suspended(&client->dev);
	return 0;
}

static int __maybe_unused ktd2687_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ktd2687_flash *flash = i2c_get_clientdata(client);

	pr_info("%s %d", __func__, __LINE__);

	return ktd2687_uninit(flash);
}

static int __maybe_unused ktd2687_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ktd2687_flash *flash = i2c_get_clientdata(client);

	pr_info("%s %d", __func__, __LINE__);

	return ktd2687_init(flash);
}

static const struct i2c_device_id ktd2687_id_table[] = {
	{KTD2687_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ktd2687_id_table);

static const struct of_device_id ktd2687_of_table[] = {
	{ .compatible = "mediatek,ktd2687" },
	{ },
};
MODULE_DEVICE_TABLE(of, ktd2687_of_table);

static const struct dev_pm_ops ktd2687_pm_ops = {
	SET_RUNTIME_PM_OPS(ktd2687_suspend, ktd2687_resume, NULL)
};

static struct i2c_driver ktd2687_i2c_driver = {
	.driver = {
		   .name = KTD2687_NAME,
		   .pm = &ktd2687_pm_ops,
		   .of_match_table = ktd2687_of_table,
		   },
	.probe = ktd2687_probe,
	.remove = ktd2687_remove,
	.id_table = ktd2687_id_table,
};

module_i2c_driver(ktd2687_i2c_driver);

MODULE_AUTHOR("Roger-HY Wang <roger-hy.wang@mediatek.com>");
MODULE_DESCRIPTION("Texas Instruments KTD2687 LED flash driver");
MODULE_LICENSE("GPL");

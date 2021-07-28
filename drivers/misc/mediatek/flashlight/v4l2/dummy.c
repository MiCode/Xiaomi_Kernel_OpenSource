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
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <linux/pm_runtime.h>

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"

#include <linux/power_supply.h>
#endif

#define DUMMY_NAME	"dummy"

/* registers definitions */
/* TODO: define register */
#define DUMMY_FLASH_BRT_MIN 10900
#define DUMMY_FLASH_BRT_STEP 11725
#define DUMMY_FLASH_BRT_MAX 1499975

#define DUMMY_FLASH_TOUT_MIN 200
#define DUMMY_FLASH_TOUT_STEP 200
#define DUMMY_FLASH_TOUT_MAX 1600

#define DUMMY_TORCH_BRT_MIN 1964
#define DUMMY_TORCH_BRT_STEP 2800
#define DUMMY_TORCH_BRT_MAX 357554

enum dummy_led_id {
	DUMMY_LED0 = 0,
	DUMMY_LED1,
	DUMMY_LED_MAX
};

/* struct dummy_platform_data
 *
 * @max_flash_timeout: flash timeout
 * @max_flash_brt: flash mode led brightness
 * @max_torch_brt: torch mode led brightness
 */
struct dummy_platform_data {
	u32 max_flash_timeout;
	u32 max_flash_brt[DUMMY_LED_MAX];
	u32 max_torch_brt[DUMMY_LED_MAX];
};

/**
 * struct dummy_flash
 *
 * @dev: pointer to &struct device
 * @pdata: platform data
 * @regmap: reg. map for i2c
 * @lock: muxtex for serial access.
 * @led_mode: V4L2 LED mode
 * @ctrls_led: V4L2 controls
 * @subdev_led: V4L2 subdev
 */
struct dummy_flash {
	struct device *dev;
	struct dummy_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;

	enum v4l2_flash_led_mode led_mode;
	struct v4l2_ctrl_handler ctrls_led[DUMMY_LED_MAX];
	struct v4l2_subdev subdev_led[DUMMY_LED_MAX];
	struct pinctrl *dummy_hwen_pinctrl;
	struct pinctrl_state *dummy_hwen_high;
	struct pinctrl_state *dummy_hwen_low;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id flash_dev_id[DUMMY_LED_MAX];
#endif
};

/* define usage count */
static int use_count;

static struct dummy_flash *dummy_flash_data;

#define to_dummy_flash(_ctrl, _no)	\
	container_of(_ctrl->handler, struct dummy_flash, ctrls_led[_no])

/* define pinctrl */
/* TODO: define pinctrl */
#define DUMMY_PINCTRL_PIN_HWEN 0
#define DUMMY_PINCTRL_PINSTATE_LOW 0
#define DUMMY_PINCTRL_PINSTATE_HIGH 1
#define DUMMY_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define DUMMY_PINCTRL_STATE_HWEN_LOW  "hwen_low"
/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int dummy_pinctrl_init(struct dummy_flash *flash)
{
	int ret = 0;

	/* get pinctrl */
	flash->dummy_hwen_pinctrl = devm_pinctrl_get(flash->dev);
	if (IS_ERR(flash->dummy_hwen_pinctrl)) {
		pr_info("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(flash->dummy_hwen_pinctrl);
		return ret;
	}

	/* Flashlight HWEN pin initialization */
	flash->dummy_hwen_high = pinctrl_lookup_state(
			flash->dummy_hwen_pinctrl,
			DUMMY_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(flash->dummy_hwen_high)) {
		pr_info("Failed to init (%s)\n",
			DUMMY_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(flash->dummy_hwen_high);
	}
	flash->dummy_hwen_low = pinctrl_lookup_state(
			flash->dummy_hwen_pinctrl,
			DUMMY_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(flash->dummy_hwen_low)) {
		pr_info("Failed to init (%s)\n", DUMMY_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(flash->dummy_hwen_low);
	}

	return ret;
}

static int dummy_pinctrl_set(struct dummy_flash *flash, int pin, int state)
{
	int ret = 0;

	if (IS_ERR(flash->dummy_hwen_pinctrl)) {
		pr_info("pinctrl is not available\n");
		return -1;
	}

	switch (pin) {
	case DUMMY_PINCTRL_PIN_HWEN:
		if (state == DUMMY_PINCTRL_PINSTATE_LOW &&
				!IS_ERR(flash->dummy_hwen_low))
			pinctrl_select_state(flash->dummy_hwen_pinctrl,
					flash->dummy_hwen_low);
		else if (state == DUMMY_PINCTRL_PINSTATE_HIGH &&
				!IS_ERR(flash->dummy_hwen_high))
			pinctrl_select_state(flash->dummy_hwen_pinctrl,
					flash->dummy_hwen_high);
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
static int dummy_mode_ctrl(struct dummy_flash *flash)
{
	int rval = -EINVAL;

	/* TODO: wrap mode ctrl function */
	switch (flash->led_mode) {
	case V4L2_FLASH_LED_MODE_NONE:
		/* turn off */
		break;
	case V4L2_FLASH_LED_MODE_TORCH:
		/* torch mode */
		break;
	case V4L2_FLASH_LED_MODE_FLASH:
		/* flash mode */
		break;
	}
	return rval;
}

/* led1/2 enable/disable */
static int dummy_enable_ctrl(struct dummy_flash *flash,
			      enum dummy_led_id led_no, bool on)
{
	int rval = 0;

	/* TODO: wrap enable function */
	if (led_no == DUMMY_LED0) {
		if (on) {
			/* enable led 0*/
			;
		} else {
			/* disable led 0*/
			;
		}
	} else {
		if (on) {
			/* enable led 1*/
			;
		} else {
			/* disable led 1*/
			;
		}
	}
	return rval;
}

/* torch1/2 brightness control */
static int dummy_torch_brt_ctrl(struct dummy_flash *flash,
				 enum dummy_led_id led_no, unsigned int brt)
{
	int rval = 0;

	/* TODO: wrap set torch brightness function */
	return rval;
}

/* flash1/2 brightness control */
static int dummy_flash_brt_ctrl(struct dummy_flash *flash,
				 enum dummy_led_id led_no, unsigned int brt)
{
	int rval = 0;

	/* TODO: wrap set flash brightness function */
	return rval;
}

/* flash1/2 timeout control */
static int dummy_flash_tout_ctrl(struct dummy_flash *flash,
				unsigned int tout)
{
	int rval = 0;

	/* TODO: wrap set flash timeout function */
	return rval;
}

/* v4l2 controls  */
static int dummy_get_ctrl(struct v4l2_ctrl *ctrl, enum dummy_led_id led_no)
{
	struct dummy_flash *flash = to_dummy_flash(ctrl, led_no);
	int rval = -EINVAL;

	mutex_lock(&flash->lock);

	/* TODO: wrap get hw fault function */
	mutex_unlock(&flash->lock);
	return rval;
}

static int dummy_set_ctrl(struct v4l2_ctrl *ctrl, enum dummy_led_id led_no)
{
	struct dummy_flash *flash = to_dummy_flash(ctrl, led_no);
	int rval = -EINVAL;

	mutex_lock(&flash->lock);

	switch (ctrl->id) {
	case V4L2_CID_FLASH_LED_MODE:
		flash->led_mode = ctrl->val;
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			rval = dummy_mode_ctrl(flash);
		else
			rval = 0;
		if (flash->led_mode == V4L2_FLASH_LED_MODE_NONE)
			dummy_enable_ctrl(flash, led_no, false);
		break;

	case V4L2_CID_FLASH_STROBE_SOURCE:
		break;

	case V4L2_CID_FLASH_STROBE:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_FLASH;
		rval = dummy_mode_ctrl(flash);
		break;

	case V4L2_CID_FLASH_STROBE_STOP:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
		rval = dummy_mode_ctrl(flash);
		dummy_enable_ctrl(flash, led_no, false);
		break;

	case V4L2_CID_FLASH_TIMEOUT:
		rval = dummy_flash_tout_ctrl(flash, ctrl->val);
		break;

	case V4L2_CID_FLASH_INTENSITY:
		rval = dummy_flash_brt_ctrl(flash, led_no, ctrl->val);
		break;

	case V4L2_CID_FLASH_TORCH_INTENSITY:
		rval = dummy_torch_brt_ctrl(flash, led_no, ctrl->val);
		break;
	}

err_out:
	mutex_unlock(&flash->lock);
	return rval;
}

static int dummy_led1_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return dummy_get_ctrl(ctrl, DUMMY_LED1);
}

static int dummy_led1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return dummy_set_ctrl(ctrl, DUMMY_LED1);
}

static int dummy_led0_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return dummy_get_ctrl(ctrl, DUMMY_LED0);
}

static int dummy_led0_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return dummy_set_ctrl(ctrl, DUMMY_LED0);
}

static const struct v4l2_ctrl_ops dummy_led_ctrl_ops[DUMMY_LED_MAX] = {
	[DUMMY_LED0] = {
			.g_volatile_ctrl = dummy_led0_get_ctrl,
			.s_ctrl = dummy_led0_set_ctrl,
			},
	[DUMMY_LED1] = {
			.g_volatile_ctrl = dummy_led1_get_ctrl,
			.s_ctrl = dummy_led1_set_ctrl,
			}
};

static int dummy_init_controls(struct dummy_flash *flash,
				enum dummy_led_id led_no)
{
	struct v4l2_ctrl *fault;
	u32 max_flash_brt = flash->pdata->max_flash_brt[led_no];
	u32 max_torch_brt = flash->pdata->max_torch_brt[led_no];
	struct v4l2_ctrl_handler *hdl = &flash->ctrls_led[led_no];
	const struct v4l2_ctrl_ops *ops = &dummy_led_ctrl_ops[led_no];

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
			  DUMMY_FLASH_TOUT_MIN,
			  flash->pdata->max_flash_timeout,
			  DUMMY_FLASH_TOUT_STEP,
			  flash->pdata->max_flash_timeout);

	/* flash brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_INTENSITY,
			  DUMMY_FLASH_BRT_MIN, max_flash_brt,
			  DUMMY_FLASH_BRT_STEP, max_flash_brt);

	/* torch brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TORCH_INTENSITY,
			  DUMMY_TORCH_BRT_MIN, max_torch_brt,
			  DUMMY_TORCH_BRT_STEP, max_torch_brt);

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
static const struct v4l2_subdev_ops dummy_ops = {
	.core = NULL,
};

static const struct regmap_config dummy_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

static void dummy_v4l2_i2c_subdev_init(struct v4l2_subdev *sd,
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

static int dummy_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
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

static int dummy_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);

	pm_runtime_put(sd->dev);

	return 0;
}

static const struct v4l2_subdev_internal_ops dummy_int_ops = {
	.open = dummy_open,
	.close = dummy_close,
};

static int dummy_subdev_init(struct dummy_flash *flash,
			      enum dummy_led_id led_no, char *led_name)
{
	struct i2c_client *client = to_i2c_client(flash->dev);
	struct device_node *np = flash->dev->of_node, *child;
	const char *fled_name = "flash";
	int rval;

	// pr_info("%s %d", __func__, led_no);

	dummy_v4l2_i2c_subdev_init(&flash->subdev_led[led_no],
				client, &dummy_ops);
	flash->subdev_led[led_no].flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	flash->subdev_led[led_no].internal_ops = &dummy_int_ops;
	strscpy(flash->subdev_led[led_no].name, led_name,
		sizeof(flash->subdev_led[led_no].name));

	for (child = of_get_child_by_name(np, fled_name); child;
			child = of_find_node_by_name(child, fled_name)) {
		int rv;
		u32 reg = 0;

		rv = of_property_read_u32(child, "reg", &reg);
		if (rv)
			continue;

		if (reg == led_no)
			flash->subdev_led[led_no].fwnode = of_fwnode_handle(child);
	}

	rval = dummy_init_controls(flash, led_no);
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
static int dummy_init(struct dummy_flash *flash)
{
	int rval = 0;

	/* TODO: wrap init function */
	dummy_pinctrl_set(flash, DUMMY_PINCTRL_PIN_HWEN, DUMMY_PINCTRL_PINSTATE_HIGH);

	return rval;
}

/* flashlight uninit */
static int dummy_uninit(struct dummy_flash *flash)
{
	dummy_pinctrl_set(flash,
			DUMMY_PINCTRL_PIN_HWEN, DUMMY_PINCTRL_PINSTATE_LOW);

	return 0;
}

static int dummy_flash_open(void)
{
	return 0;
}

static int dummy_flash_release(void)
{
	return 0;
}

static int dummy_ioctl(unsigned int cmd, unsigned long arg)
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
			dummy_torch_brt_ctrl(dummy_flash_data, channel, 25000);
			dummy_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
			dummy_mode_ctrl(dummy_flash_data);
		} else {
			dummy_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
			dummy_mode_ctrl(dummy_flash_data);
			dummy_enable_ctrl(dummy_flash_data, channel, false);
		}
		break;
	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static int dummy_set_driver(int set)
{
	int ret = 0;

	/* set chip and usage count */
	//mutex_lock(&dummy_mutex);
	if (set) {
		if (!use_count)
			ret = dummy_init(dummy_flash_data);
		use_count++;
		pr_debug("Set driver: %d\n", use_count);
	} else {
		use_count--;
		if (!use_count)
			ret = dummy_uninit(dummy_flash_data);
		if (use_count < 0)
			use_count = 0;
		pr_debug("Unset driver: %d\n", use_count);
	}
	//mutex_unlock(&dummy_mutex);

	return 0;
}

static ssize_t dummy_strobe_store(struct flashlight_arg arg)
{
	dummy_set_driver(1);
	//dummy_set_level(arg.channel, arg.level);
	//dummy_timeout_ms[arg.channel] = 0;
	//dummy_enable(arg.channel);
	dummy_torch_brt_ctrl(dummy_flash_data, arg.channel,
				arg.level * 25000);
	dummy_flash_data->led_mode = V4L2_FLASH_LED_MODE_TORCH;
	dummy_mode_ctrl(dummy_flash_data);
	msleep(arg.dur);
	//dummy_disable(arg.channel);
	dummy_flash_data->led_mode = V4L2_FLASH_LED_MODE_NONE;
	dummy_mode_ctrl(dummy_flash_data);
	dummy_enable_ctrl(dummy_flash_data, arg.channel, false);
	dummy_set_driver(0);
	return 0;
}

static struct flashlight_operations dummy_flash_ops = {
	dummy_flash_open,
	dummy_flash_release,
	dummy_ioctl,
	dummy_strobe_store,
	dummy_set_driver
};

static int dummy_parse_dt(struct dummy_flash *flash)
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
			&dummy_flash_ops))
			return -EFAULT;
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}

static int dummy_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct dummy_flash *flash;
	struct dummy_platform_data *pdata = dev_get_platdata(&client->dev);
	int rval;

	pr_info("%s:%d", __func__, __LINE__);

	flash = devm_kzalloc(&client->dev, sizeof(*flash), GFP_KERNEL);
	if (flash == NULL)
		return -ENOMEM;

	flash->regmap = devm_regmap_init_i2c(client, &dummy_regmap);
	if (IS_ERR(flash->regmap)) {
		rval = PTR_ERR(flash->regmap);
		return rval;
	}

	/* if there is no platform data, use chip default value */
	if (pdata == NULL) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL)
			return -ENODEV;
		pdata->max_flash_timeout = DUMMY_FLASH_TOUT_MAX;
		/* led 1 */
		pdata->max_flash_brt[DUMMY_LED0] = DUMMY_FLASH_BRT_MAX;
		pdata->max_torch_brt[DUMMY_LED0] = DUMMY_TORCH_BRT_MAX;
		/* led 2 */
		pdata->max_flash_brt[DUMMY_LED1] = DUMMY_FLASH_BRT_MAX;
		pdata->max_torch_brt[DUMMY_LED1] = DUMMY_TORCH_BRT_MAX;
	}
	flash->pdata = pdata;
	flash->dev = &client->dev;
	mutex_init(&flash->lock);
	dummy_flash_data = flash;

	rval = dummy_pinctrl_init(flash);
	if (rval < 0)
		return rval;

	rval = dummy_subdev_init(flash, DUMMY_LED0, "dummy-led0");
	if (rval < 0)
		return rval;

	rval = dummy_subdev_init(flash, DUMMY_LED1, "dummy-led1");
	if (rval < 0)
		return rval;

	pm_runtime_enable(flash->dev);

	rval = dummy_parse_dt(flash);

	i2c_set_clientdata(client, flash);

	pr_info("%s:%d", __func__, __LINE__);
	return 0;
}

static int dummy_remove(struct i2c_client *client)
{
	struct dummy_flash *flash = i2c_get_clientdata(client);
	unsigned int i;

	for (i = DUMMY_LED0; i < DUMMY_LED_MAX; i++) {
		v4l2_device_unregister_subdev(&flash->subdev_led[i]);
		v4l2_ctrl_handler_free(&flash->ctrls_led[i]);
		media_entity_cleanup(&flash->subdev_led[i].entity);
	}

	pm_runtime_disable(&client->dev);

	pm_runtime_set_suspended(&client->dev);
	return 0;
}

static int __maybe_unused dummy_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dummy_flash *flash = i2c_get_clientdata(client);

	pr_info("%s %d", __func__, __LINE__);

	return dummy_uninit(flash);
}

static int __maybe_unused dummy_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dummy_flash *flash = i2c_get_clientdata(client);

	pr_info("%s %d", __func__, __LINE__);

	return dummy_init(flash);
}

static const struct i2c_device_id dummy_id_table[] = {
	{DUMMY_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, dummy_id_table);

static const struct of_device_id dummy_of_table[] = {
	{ .compatible = "mediatek,dummy" },
	{ },
};
MODULE_DEVICE_TABLE(of, dummy_of_table);

static const struct dev_pm_ops dummy_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(dummy_suspend, dummy_resume, NULL)
};

static struct i2c_driver dummy_i2c_driver = {
	.driver = {
		   .name = DUMMY_NAME,
		   .pm = &dummy_pm_ops,
		   .of_match_table = dummy_of_table,
		   },
	.probe = dummy_probe,
	.remove = dummy_remove,
	.id_table = dummy_id_table,
};

module_i2c_driver(dummy_i2c_driver);

MODULE_AUTHOR("Roger-HY Wang <roger-hy.wang@mediatek.com>");
MODULE_DESCRIPTION("DUMMY LED flash driver");
MODULE_LICENSE("GPL");

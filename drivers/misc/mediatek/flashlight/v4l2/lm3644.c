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
// #include <lm3644.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"

#include <linux/power_supply.h>
#endif

#define LM3644_NAME	"lm3644"
#define LM3644_I2C_ADDR	(0x63)

/* registers definitions */
#define REG_ENABLE		0x01
#define REG_LED0_FLASH_BR	0x03
#define REG_LED1_FLASH_BR	0x04
#define REG_LED0_TORCH_BR	0x05
#define REG_LED1_TORCH_BR	0x06
#define REG_FLASH_TOUT		0x08
#define REG_FLAG1		0x0A
#define REG_FLAG2		0x0B
// #define REG_CONFIG1		0xe0

/* fault mask */
#define FAULT_TIMEOUT	(1<<0)
#define FAULT_THERMAL_SHUTDOWN	(1<<2)
#define FAULT_LED0_SHORT_CIRCUIT	(1<<5)
#define FAULT_LED1_SHORT_CIRCUIT	(1<<4)

/*  FLASH Brightness
 *	min 10900uA, step 11725uA, max 1500000uA
 */
#define LM3644_FLASH_BRT_MIN 10900
#define LM3644_FLASH_BRT_STEP 11725
#define LM3644_FLASH_BRT_MAX 1499975
#define LM3644_FLASH_BRT_uA_TO_REG(a)	\
	((a) < LM3644_FLASH_BRT_MIN ? 0 :	\
	 (((a) - LM3644_FLASH_BRT_MIN) / LM3644_FLASH_BRT_STEP))
#define LM3644_FLASH_BRT_REG_TO_uA(a)		\
	((a) * LM3644_FLASH_BRT_STEP + LM3644_FLASH_BRT_MIN)

/*  FLASH TIMEOUT DURATION
 *	min 32ms, step 32ms, max 1024ms
 */
#define LM3644_FLASH_TOUT_MIN 200
#define LM3644_FLASH_TOUT_STEP 200
#define LM3644_FLASH_TOUT_MAX 1600

/*  TORCH BRT
 *	min 1954uA, step 2800uA, max 357554uA
 */
#define LM3644_TORCH_BRT_MIN 1964
#define LM3644_TORCH_BRT_STEP 2800
#define LM3644_TORCH_BRT_MAX 357554
#define LM3644_TORCH_BRT_uA_TO_REG(a)	\
	((a) < LM3644_TORCH_BRT_MIN ? 0 :	\
	 (((a) - LM3644_TORCH_BRT_MIN) / LM3644_TORCH_BRT_STEP))
#define LM3644_TORCH_BRT_REG_TO_uA(a)		\
	((a) * LM3644_TORCH_BRT_STEP + LM3644_TORCH_BRT_MIN)

enum lm3644_led_id {
	LM3644_LED0 = 0,
	LM3644_LED1,
	LM3644_LED_MAX
};

/* struct lm3644_platform_data
 *
 * @max_flash_timeout: flash timeout
 * @max_flash_brt: flash mode led brightness
 * @max_torch_brt: torch mode led brightness
 */
struct lm3644_platform_data {
	u32 max_flash_timeout;
	u32 max_flash_brt[LM3644_LED_MAX];
	u32 max_torch_brt[LM3644_LED_MAX];
};


enum led_enable {
	MODE_SHDN = 0x0,
	MODE_TORCH = 0x08,
	MODE_FLASH = 0x0C,
};

/**
 * struct lm3644_flash
 *
 * @dev: pointer to &struct device
 * @pdata: platform data
 * @regmap: reg. map for i2c
 * @lock: muxtex for serial access.
 * @led_mode: V4L2 LED mode
 * @ctrls_led: V4L2 controls
 * @subdev_led: V4L2 subdev
 */
struct lm3644_flash {
	struct device *dev;
	struct lm3644_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;

	enum v4l2_flash_led_mode led_mode;
	struct v4l2_ctrl_handler ctrls_led[LM3644_LED_MAX];
	struct v4l2_subdev subdev_led[LM3644_LED_MAX];
	struct pinctrl *lm3644_hwen_pinctrl;
	struct pinctrl_state *lm3644_hwen_high;
	struct pinctrl_state *lm3644_hwen_low;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id flash_dev_id[LM3644_LED_MAX];
#endif
};

#define to_lm3644_flash(_ctrl, _no)	\
	container_of(_ctrl->handler, struct lm3644_flash, ctrls_led[_no])

/* define pinctrl */
#define LM3644_PINCTRL_STATE_HWEN_HIGH "hwen_high"
#define LM3644_PINCTRL_STATE_HWEN_LOW  "hwen_low"
/******************************************************************************
 * Pinctrl configuration
 *****************************************************************************/
static int lm3644_pinctrl_init(struct lm3644_flash *flash)
{
	int ret = 0;

	/* get pinctrl */
	flash->lm3644_hwen_pinctrl = devm_pinctrl_get(flash->dev);
	if (IS_ERR(flash->lm3644_hwen_pinctrl)) {
		pr_info("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(flash->lm3644_hwen_pinctrl);
		return ret;
	}

	/* Flashlight HWEN pin initialization */
	flash->lm3644_hwen_high = pinctrl_lookup_state(
			flash->lm3644_hwen_pinctrl,
			LM3644_PINCTRL_STATE_HWEN_HIGH);
	if (IS_ERR(flash->lm3644_hwen_high)) {
		pr_info("Failed to init (%s)\n",
			LM3644_PINCTRL_STATE_HWEN_HIGH);
		ret = PTR_ERR(flash->lm3644_hwen_high);
	}
	flash->lm3644_hwen_low = pinctrl_lookup_state(
			flash->lm3644_hwen_pinctrl,
			LM3644_PINCTRL_STATE_HWEN_LOW);
	if (IS_ERR(flash->lm3644_hwen_low)) {
		pr_info("Failed to init (%s)\n", LM3644_PINCTRL_STATE_HWEN_LOW);
		ret = PTR_ERR(flash->lm3644_hwen_low);
	}

	return ret;
}

/* enable mode control */
static int lm3644_mode_ctrl(struct lm3644_flash *flash)
{
	int rval = -EINVAL;

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
static int lm3644_enable_ctrl(struct lm3644_flash *flash,
			      enum lm3644_led_id led_no, bool on)
{
	int rval;

	if (led_no == LM3644_LED0) {
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
static int lm3644_torch_brt_ctrl(struct lm3644_flash *flash,
				 enum lm3644_led_id led_no, unsigned int brt)
{
	int rval;
	u8 br_bits;

	if (brt < LM3644_TORCH_BRT_MIN)
		return lm3644_enable_ctrl(flash, led_no, false);

	rval = lm3644_enable_ctrl(flash, led_no, true);

	br_bits = LM3644_TORCH_BRT_uA_TO_REG(brt);
	if (led_no == LM3644_LED0)
		rval = regmap_update_bits(flash->regmap,
					  REG_LED0_TORCH_BR, 0x7f, br_bits);
	else
		rval = regmap_update_bits(flash->regmap,
					  REG_LED1_TORCH_BR, 0x7f, br_bits);

	return rval;
}

/* flash1/2 brightness control */
static int lm3644_flash_brt_ctrl(struct lm3644_flash *flash,
				 enum lm3644_led_id led_no, unsigned int brt)
{
	int rval;
	u8 br_bits;

	if (brt < LM3644_FLASH_BRT_MIN)
		return lm3644_enable_ctrl(flash, led_no, false);

	rval = lm3644_enable_ctrl(flash, led_no, true);

	br_bits = LM3644_FLASH_BRT_uA_TO_REG(brt);
	if (led_no == LM3644_LED0)
		rval = regmap_update_bits(flash->regmap,
					  REG_LED0_FLASH_BR, 0x7f, br_bits);
	else
		rval = regmap_update_bits(flash->regmap,
					  REG_LED1_FLASH_BR, 0x7f, br_bits);

	return rval;
}

/* flash1/2 timeout control */
static int lm3644_flash_tout_ctrl(struct lm3644_flash *flash,
				unsigned int tout)
{
	int rval;
	u8 tout_bits;

	if (tout == 200)
		tout_bits = 0x04;
	else
		tout_bits = 0x07 + (tout / LM3644_FLASH_TOUT_STEP);

	rval = regmap_update_bits(flash->regmap,
				  REG_FLASH_TOUT, 0x1f, tout_bits);

	return rval;
}

/* v4l2 controls  */
static int lm3644_get_ctrl(struct v4l2_ctrl *ctrl, enum lm3644_led_id led_no)
{
	struct lm3644_flash *flash = to_lm3644_flash(ctrl, led_no);
	int rval = -EINVAL;

	mutex_lock(&flash->lock);

	if (ctrl->id == V4L2_CID_FLASH_FAULT) {
		s32 fault = 0;
		unsigned int reg_val;

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

static int lm3644_set_ctrl(struct v4l2_ctrl *ctrl, enum lm3644_led_id led_no)
{
	struct lm3644_flash *flash = to_lm3644_flash(ctrl, led_no);
	int rval = -EINVAL;

	mutex_lock(&flash->lock);

	switch (ctrl->id) {
	case V4L2_CID_FLASH_LED_MODE:
		flash->led_mode = ctrl->val;
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			rval = lm3644_mode_ctrl(flash);
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
		rval = lm3644_mode_ctrl(flash);
		break;

	case V4L2_CID_FLASH_STROBE_STOP:
		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH) {
			rval = -EBUSY;
			goto err_out;
		}
		flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
		rval = lm3644_mode_ctrl(flash);
		break;

	case V4L2_CID_FLASH_TIMEOUT:
		// tout_bits = LM3644_FLASH_TOUT_ms_TO_REG(ctrl->val);
		// rval = regmap_update_bits(flash->regmap,
		//			  REG_FLASH_TOUT, 0x1f, tout_bits);
		rval = lm3644_flash_tout_ctrl(flash, ctrl->val);
		break;

	case V4L2_CID_FLASH_INTENSITY:
		rval = lm3644_flash_brt_ctrl(flash, led_no, ctrl->val);
		break;

	case V4L2_CID_FLASH_TORCH_INTENSITY:
		rval = lm3644_torch_brt_ctrl(flash, led_no, ctrl->val);
		break;
	}

err_out:
	mutex_unlock(&flash->lock);
	return rval;
}

static int lm3644_led1_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return lm3644_get_ctrl(ctrl, LM3644_LED1);
}

static int lm3644_led1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return lm3644_set_ctrl(ctrl, LM3644_LED1);
}

static int lm3644_led0_get_ctrl(struct v4l2_ctrl *ctrl)
{
	return lm3644_get_ctrl(ctrl, LM3644_LED0);
}

static int lm3644_led0_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return lm3644_set_ctrl(ctrl, LM3644_LED0);
}

static const struct v4l2_ctrl_ops lm3644_led_ctrl_ops[LM3644_LED_MAX] = {
	[LM3644_LED0] = {
			.g_volatile_ctrl = lm3644_led0_get_ctrl,
			.s_ctrl = lm3644_led0_set_ctrl,
			},
	[LM3644_LED1] = {
			.g_volatile_ctrl = lm3644_led1_get_ctrl,
			.s_ctrl = lm3644_led1_set_ctrl,
			}
};

static int lm3644_init_controls(struct lm3644_flash *flash,
				enum lm3644_led_id led_no)
{
	struct v4l2_ctrl *fault;
	u32 max_flash_brt = flash->pdata->max_flash_brt[led_no];
	u32 max_torch_brt = flash->pdata->max_torch_brt[led_no];
	struct v4l2_ctrl_handler *hdl = &flash->ctrls_led[led_no];
	const struct v4l2_ctrl_ops *ops = &lm3644_led_ctrl_ops[led_no];

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
			  LM3644_FLASH_TOUT_MIN,
			  flash->pdata->max_flash_timeout,
			  LM3644_FLASH_TOUT_STEP,
			  flash->pdata->max_flash_timeout);

	/* flash brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_INTENSITY,
			  LM3644_FLASH_BRT_MIN, max_flash_brt,
			  LM3644_FLASH_BRT_STEP, max_flash_brt);

	/* torch brt */
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FLASH_TORCH_INTENSITY,
			  LM3644_TORCH_BRT_MIN, max_torch_brt,
			  LM3644_TORCH_BRT_STEP, max_torch_brt);

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
static const struct v4l2_subdev_ops lm3644_ops = {
	.core = NULL,
};

static const struct regmap_config lm3644_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

static int lm3644_subdev_init(struct lm3644_flash *flash,
			      enum lm3644_led_id led_no, char *led_name)
{
	struct i2c_client *client = to_i2c_client(flash->dev);
	struct device_node *np = flash->dev->of_node, *child;
	const char *fled_name = "flash";
	int rval;

	// pr_info("%s %d", __func__, led_no);

	v4l2_i2c_subdev_init(&flash->subdev_led[led_no], client, &lm3644_ops);
	flash->subdev_led[led_no].flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
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
			// pr_info("Roger child name:%s\n", child->name);
			flash->subdev_led[led_no].fwnode = of_fwnode_handle(child);
		}
	}

	rval = lm3644_init_controls(flash, led_no);
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

static int lm3644_open(void)
{
	return 0;
}

static int lm3644_release(void)
{
	return 0;
}

static int lm3644_ioctl(unsigned int cmd, unsigned long arg)
{
	return 0;
}

static ssize_t lm3644_strobe_store(struct flashlight_arg arg)
{
	return 0;
}

static int lm3644_set_driver(int set)
{
	return 0;
}

static struct flashlight_operations lm3644_flash_ops = {
	lm3644_open,
	lm3644_release,
	lm3644_ioctl,
	lm3644_strobe_store,
	lm3644_set_driver
};

static int lm3644_parse_dt(struct lm3644_flash *flash)
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
			&lm3644_flash_ops))
			return -EFAULT;
		i++;
	}

	return 0;

err_node_put:
	of_node_put(cnp);
	return -EINVAL;
}


static int lm3644_init_device(struct lm3644_flash *flash)
{
	int rval = 0;
	unsigned int reg_val;
	/* set timeout */
	rval = lm3644_flash_tout_ctrl(flash, 400);
	if (rval < 0)
		return rval;
	/* output disable */
	flash->led_mode = V4L2_FLASH_LED_MODE_NONE;
	rval = lm3644_mode_ctrl(flash);
	if (rval < 0)
		return rval;
	/* reset faults */
	rval = regmap_read(flash->regmap, REG_FLAG1, &reg_val);
	return rval;
}

static int lm3644_probe(struct i2c_client *client,
			const struct i2c_device_id *devid)
{
	struct lm3644_flash *flash;
	struct lm3644_platform_data *pdata = dev_get_platdata(&client->dev);
	int rval;

	pr_info("%s:%d", __func__, __LINE__);

	flash = devm_kzalloc(&client->dev, sizeof(*flash), GFP_KERNEL);
	if (flash == NULL)
		return -ENOMEM;

	flash->regmap = devm_regmap_init_i2c(client, &lm3644_regmap);
	if (IS_ERR(flash->regmap)) {
		rval = PTR_ERR(flash->regmap);
		return rval;
	}

	/* if there is no platform data, use chip default value */
	if (pdata == NULL) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL)
			return -ENODEV;
		// pdata->peak = LM3644_PEAK_3600mA;
		pdata->max_flash_timeout = LM3644_FLASH_TOUT_MAX;
		/* led 1 */
		pdata->max_flash_brt[LM3644_LED0] = LM3644_FLASH_BRT_MAX;
		pdata->max_torch_brt[LM3644_LED0] = LM3644_TORCH_BRT_MAX;
		/* led 2 */
		pdata->max_flash_brt[LM3644_LED1] = LM3644_FLASH_BRT_MAX;
		pdata->max_torch_brt[LM3644_LED1] = LM3644_TORCH_BRT_MAX;
	}
	flash->pdata = pdata;
	flash->dev = &client->dev;
	mutex_init(&flash->lock);

	rval = lm3644_pinctrl_init(flash);
	if (rval < 0)
		return rval;

	rval = lm3644_subdev_init(flash, LM3644_LED0, "lm3644-led0");
	if (rval < 0)
		return rval;

	rval = lm3644_subdev_init(flash, LM3644_LED1, "lm3644-led1");
	if (rval < 0)
		return rval;

	pr_info("%s:%d", __func__, __LINE__);
	rval = lm3644_parse_dt(flash);
	pr_info("%s:ret=%d", __func__, rval);

	rval = lm3644_init_device(flash);
	if (rval < 0)
		return rval;

	i2c_set_clientdata(client, flash);

	return 0;
}

static int lm3644_remove(struct i2c_client *client)
{
	struct lm3644_flash *flash = i2c_get_clientdata(client);
	unsigned int i;

	for (i = LM3644_LED0; i < LM3644_LED_MAX; i++) {
		v4l2_device_unregister_subdev(&flash->subdev_led[i]);
		v4l2_ctrl_handler_free(&flash->ctrls_led[i]);
		media_entity_cleanup(&flash->subdev_led[i].entity);
	}

	return 0;
}

static const struct i2c_device_id lm3644_id_table[] = {
	{LM3644_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3644_id_table);

static const struct of_device_id lm3644_of_table[] = {
	{ .compatible = "mediatek,lm3644" },
	{ },
};
MODULE_DEVICE_TABLE(of, lm3644_of_table);

static struct i2c_driver lm3644_i2c_driver = {
	.driver = {
		   .name = LM3644_NAME,
		   .pm = NULL,
		   .of_match_table = lm3644_of_table,
		   },
	.probe = lm3644_probe,
	.remove = lm3644_remove,
	.id_table = lm3644_id_table,
};

module_i2c_driver(lm3644_i2c_driver);

MODULE_AUTHOR("Roger-HY Wang <roger-hy.wang@mediatek.com>");
MODULE_DESCRIPTION("Texas Instruments LM3644 LED flash driver");
MODULE_LICENSE("GPL");

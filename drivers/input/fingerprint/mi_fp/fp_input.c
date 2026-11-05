#include "fp_driver.h"
#include <linux/input.h>


#define	SET_BIT(x, bit)		(x |= (1 << bit))
#define	CLEAR_BIT(x, bit)	(x &= ~(1 << bit))

#ifdef FP_ULTRA_QCOM
static struct fp_device *fp_dev;

#ifndef FP_ULTRA_QCOM_P11
static void fp_intr2_work_func(struct work_struct *work)
{
	/* fp_local_time_printk("intr2 operation:SLOT_MAP: %d, GPIO:%d,", fp_dev->position.SLOT_MAP, gpio_get_value(fp_dev->intr2_gpio));*/
	if (fp_dev->position.SLOT_MAP == 0 && gpio_get_value(fp_dev->intr2_gpio) == 1) {
		fp_local_time_printk("input_event: intr2 output low");
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
	}
	if (fp_dev->position.SLOT_MAP > 0 && gpio_get_value(fp_dev->intr2_gpio) == 0) {
		fp_local_time_printk("input_event: intr2 output high");
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_high);
	}
}

static void xiaomifp_intr2_operation(int SLOT_CUR, int code)
{
	int Touch_X = fp_dev->position.touch_x[SLOT_CUR];
	int Touch_Y = fp_dev->position.touch_y[SLOT_CUR];
	int TRACKING_ID = fp_dev->position.TRACKING_ID[SLOT_CUR];

	/* fp_local_time_printk("intr2 operation:SLOT_CUR: %d, touch_x:%d, touch_y:%d, TRACKING_ID:%d, code:%d", SLOT_CUR, Touch_X, Touch_Y, TRACKING_ID, code);*/
	if (code == ABS_MT_TRACKING_ID) {
		if (TRACKING_ID == -1) {
			CLEAR_BIT(fp_dev->position.SLOT_MAP, SLOT_CUR);
		}
	}

	if (code == SYN_REPORT) {
		if (TRACKING_ID > 0) {
			if (Touch_X > fp_dev->position.x1 &&  Touch_X < fp_dev->position.x2
				&& Touch_Y > fp_dev->position.y1 && Touch_Y < fp_dev->position.y2) {
				SET_BIT(fp_dev->position.SLOT_MAP, SLOT_CUR);
			} else {
				CLEAR_BIT(fp_dev->position.SLOT_MAP, SLOT_CUR);
			}
		}
		schedule_work(&fp_dev->intr2_work);
	}
}
#endif

static void xiaomifp_input_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	if (type == EV_ABS) {
		if (code == ABS_MT_SLOT) {
			fp_dev->position.SLOT_CUR = value;
		} else {
			if (code == ABS_MT_POSITION_X) {
				fp_dev->position.touch_x[fp_dev->position.SLOT_CUR] = (value/100);
			}
			if (code == ABS_MT_POSITION_Y) {
				fp_dev->position.touch_y[fp_dev->position.SLOT_CUR] = (value/100);
			}
			if (code == ABS_MT_TRACKING_ID) {
				fp_dev->position.TRACKING_ID[fp_dev->position.SLOT_CUR] = value;
			}
		}
	}
#ifndef FP_ULTRA_QCOM_P11
	if (fp_dev->position.enable && ((code == ABS_MT_TRACKING_ID) || (code == SYN_REPORT))) {
		xiaomifp_intr2_operation(fp_dev->position.SLOT_CUR, code);
	}
#endif
}

static int xiaomifp_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	pr_debug("enter");

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "xiaomifp";

	error = input_register_handle(handle);
	if (error) {
		pr_debug("input_register_handle fail");
		goto err2;
	}

	error = input_open_device(handle);
	if (error) {
		pr_debug("input_open_device fail");
		goto err1;
	}

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	handle = NULL;
	return error;
}

static void xiaomifp_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
	handle = NULL;
}

static const struct input_device_id xiaomifp_ids[] = {
	/* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y)
		},
	},
	{ },
};

static struct input_handler xiaomifp_input_handler = {
	.event		= xiaomifp_input_event,
	.connect	= xiaomifp_input_connect,
	.disconnect	= xiaomifp_input_disconnect,
	.name		= "xiaomi-fp",
	.id_table	= xiaomifp_ids,
};

int xiaomifp_evdev_init(struct fp_device *_fp_dev)
{
	int ret;
	fp_dev = _fp_dev;
	ret = input_register_handler(&xiaomifp_input_handler);
	if (ret == 0) {
		pr_debug("input_register_handler success!");
	}
	fp_dev->position.enable = false;
	fp_dev->position.x1 = fp_dev->position.location[0];
	fp_dev->position.y1 = fp_dev->position.location[1];
	fp_dev->position.x2 = fp_dev->position.location[2];
	fp_dev->position.y2 = fp_dev->position.location[3];
	fp_dev->position.SLOT_MAP = 0;
	fp_dev->position.SLOT_CUR = 0;
	memset(fp_dev->position.TRACKING_ID, 0, sizeof(int) * 32);
	memset(fp_dev->position.touch_x, 0, sizeof(int) * 32);
	memset(fp_dev->position.touch_y, 0, sizeof(int) * 32);
#ifndef FP_ULTRA_QCOM_P11
	INIT_WORK(&fp_dev->intr2_work, fp_intr2_work_func);
#endif
	pr_debug("sensorLocation:%d,%d,%d,%d\n",fp_dev->position.x1, fp_dev->position.y1, fp_dev->position.x2, fp_dev->position.y2);
	return ret;
}

void xiaomifp_evdev_remove(void)
{
	input_unregister_handler(&xiaomifp_input_handler);
	gpio_free(fp_dev->intr2_gpio);
	fp_dev->position.reqIntr2 = 0;
}
#endif

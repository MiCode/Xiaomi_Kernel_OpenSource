#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>

#define TRIGGER_PANIC_TIME_OUT  2000000000
static int is_key_pressed;
static bool enabled = 1;
static bool trigger_panic_handler_registered;
static ktime_t key_down_time;
static inline void trigger_panic_unregister_handler(void);
static inline void trigger_panic_register_handler(void);
static int trigger_panic_toggle_support(const char *val, const struct kernel_param *kp);
module_param_call(enabled, trigger_panic_toggle_support, param_get_int, &enabled, 0644);

static int trigger_panic_toggle_support(const char *val, const struct kernel_param *kp)
{
	int ret;

	if (strlen(val) == 2) {
		if (*val == '0' && *(val+1) == 0x0a) {
			enabled = 0;
			trigger_panic_unregister_handler();
		} else if (*val == '1' && *(val+1) == 0x0a)  {
			enabled = 1;
			trigger_panic_register_handler();
		} else {
			return 0;
		}
	} else {
		return 0;
	}

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	return 0;
}

static bool trigger_panic_filter(struct input_handle *handle,
			 unsigned int type, unsigned int code, int value)
{

	ktime_t now;


	if (type == EV_KEY) {

		switch (code) {
		case KEY_POWER:
			if (is_key_pressed == 2 && value == 1) {
				now = ktime_get();
				if (ktime_sub(now, key_down_time) > TRIGGER_PANIC_TIME_OUT) {
					panic("press key trigger panic");
				}
			} else {
				is_key_pressed = 0;
			}
			break;
		case KEY_VOLUMEDOWN:
			if (value == 1 && is_key_pressed == 0) {
				is_key_pressed++;
			} else {
				is_key_pressed = 0;
			}
			break;
		case KEY_VOLUMEUP:
			if (is_key_pressed == 1 && value == 1) {
				is_key_pressed++;
				key_down_time = ktime_get();
			} else {
				is_key_pressed = 0;
			}
			break;
		}

	}

	return false;
}

static int trigger_panic_connect(struct input_handler *handler,
			 struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;


	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "trigger_panic";

	error = input_register_handle(handle);
	if (error) {
		pr_err("Failed to register input sysrq handler, error %d\n",
			error);
		goto err_free;
	}

	error = input_open_device(handle);
	if (error) {
		pr_err("Failed to open input device, error %d\n", error);
		goto err_unregister;
	}
	return 0;

err_unregister:
	input_unregister_handle(handle);
err_free:
	kfree(handle);
	return error;
}

static void trigger_panic_disconnect(struct input_handle *handle)
{



	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id trigger_panic_ids[] =
	{
		{ .driver_info = 1 },	/* Matches all devices */
		{ },			/* Terminating zero entry */
	};
/*{
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { [BIT_WORD(EV_KEY)] = BIT_MASK(EV_KEY) },
		.keybit = {
					[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER),
					[BIT_WORD(KEY_VOLUMEUP)] = BIT_MASK(KEY_VOLUMEUP),
					[BIT_WORD(KEY_VOLUMEDOWN)] = BIT_MASK(KEY_VOLUMEDOWN),
				  },
	},
	{ },
};*/

static struct input_handler trigger_panic_handler = {
	.filter		= trigger_panic_filter,
	.connect	= trigger_panic_connect,
	.disconnect	= trigger_panic_disconnect,
	.name		= "trigger_panic",
	.id_table	= trigger_panic_ids,
};

static inline void trigger_panic_register_handler(void)
{
	int error;


	error = input_register_handler(&trigger_panic_handler);
	if (error) {
		pr_err("Failed to register input handler, error %d", error);
	} else {
		trigger_panic_handler_registered = true;
	}
}

static inline void trigger_panic_unregister_handler(void)
{
	if (trigger_panic_handler_registered) {
		input_unregister_handler(&trigger_panic_handler);
		trigger_panic_handler_registered = false;
	}
}

static int __init trigger_panic_init(void)
{

	trigger_panic_register_handler();
	return 0;
}
device_initcall(trigger_panic_init);


